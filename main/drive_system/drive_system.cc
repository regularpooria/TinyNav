#include "drive_system.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/mcpwm_prelude.h"
#include <stdio.h>
#include <stdlib.h>
#include <WS2812FX.h>
#include "../led_manager.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

#define RECEIVER_CH1 GPIO_NUM_32
#define RECEIVER_CH2 GPIO_NUM_33
#define RECEIVER_CH3 GPIO_NUM_27
#define CHANNEL_COUNT 3

#define ADC_GPIO GPIO_NUM_51
#define ADC_UNIT ADC_UNIT_2
#define ADC_CHANNEL ADC_CHANNEL_2
#define ADC_ATTEN ADC_ATTEN_DB_12
adc_oneshot_unit_handle_t adc_handle;
adc_cali_handle_t cali_handle = NULL;

// Voltage divider resistor values (in ohms)
#define R1 30000.0f
#define R2 7500.0f
#define REF_VOLTAGE 3.3f

#define IN1 GPIO_NUM_25
#define IN2 GPIO_NUM_24
#define IN3 GPIO_NUM_2
#define IN4 GPIO_NUM_3
#define ENA GPIO_NUM_28
#define ENB GPIO_NUM_29

// PWM Configuration - adjust this to fix beeping
#define PWM_FREQUENCY_HZ 5000      // Try: 20000, 25000, or 30000 Hz
#define PWM_RESOLUTION_HZ 10000000 // 10 MHz base clock

// Calculate period ticks based on desired frequency
#define PWM_PERIOD_TICKS (PWM_RESOLUTION_HZ / PWM_FREQUENCY_HZ)

mcpwm_timer_handle_t timer = NULL;
mcpwm_oper_handle_t operator_a = NULL;
mcpwm_oper_handle_t operator_b = NULL;
mcpwm_cmpr_handle_t comparator_a = NULL;
mcpwm_cmpr_handle_t comparator_b = NULL;
mcpwm_gen_handle_t generator_a = NULL;
mcpwm_gen_handle_t generator_b = NULL;

// Configurable pulse min/max
static int64_t min_pulse = 985;
static int64_t max_pulse = 1980;

// Pulse measurement
static volatile int64_t last_rise_us[CHANNEL_COUNT] = {0, 0, 0};
static volatile int64_t last_edge_us[CHANNEL_COUNT] = {0, 0, 0};
static volatile int64_t pulse_width_us[CHANNEL_COUNT] = {0, 0, 0};
static volatile int64_t period_us[CHANNEL_COUNT] = {0, 0, 0};

float steering_scaled = 0.0;
float throttle_scaled = 0.0;
float ch3_scaled = 0.0;

static void IRAM_ATTR signal_isr(void *arg)
{
  int ch = (int)arg;
  int level = gpio_get_level(ch == 0 ? RECEIVER_CH1 : (ch == 1 ? RECEIVER_CH2 : RECEIVER_CH3));
  int64_t now = esp_timer_get_time();

  if (level)
  {
    // rising edge
    period_us[ch] = now - last_edge_us[ch];
    last_edge_us[ch] = now;
    last_rise_us[ch] = now;
  }
  else
  {
    // falling edge
    pulse_width_us[ch] = now - last_rise_us[ch];
  }
}

float normalize_pulse(int64_t pulse)
{
  if (pulse < min_pulse)
    pulse = min_pulse;
  if (pulse > max_pulse)
    pulse = max_pulse;
  return (float)(pulse - min_pulse) / (max_pulse - min_pulse);
}

void setup_voltage_sensor()
{
  // Configure GPIO51 as analog input
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << ADC_GPIO),
      .mode = GPIO_MODE_DISABLE,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
      .hys_ctrl_mode = GPIO_HYS_SOFT_DISABLE,
  };
  gpio_config(&io_conf);

  // Initialize ADC2 unit
  adc_oneshot_unit_init_cfg_t init_config = {
      .unit_id = ADC_UNIT,
      .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

  // Configure ADC2 Channel 2
  adc_oneshot_chan_cfg_t config = {
      .atten = ADC_ATTEN,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));

  // Initialize calibration (ESP32-P4 uses curve fitting)
  adc_cali_curve_fitting_config_t cali_config = {
      .unit_id = ADC_UNIT,
      .chan = ADC_CHANNEL,
      .atten = ADC_ATTEN,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };

  esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
  if (ret == ESP_OK)
  {
    printf("ADC calibration initialized on GPIO51");
  }
  else
  {
    printf("ADC calibration not available");
    cali_handle = NULL;
  }
}

float read_voltage_mv(void)
{
  int raw_value;
  int adc_voltage_mv;
  float adc_voltage;
  float input_voltage;

  // Read raw ADC value
  ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_value));

  // Get calibrated voltage at ADC pin (in millivolts)
  if (cali_handle != NULL)
  {
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw_value, &adc_voltage_mv));
    adc_voltage = adc_voltage_mv / 1000.0f; // Convert to volts
  }
  else
  {
    // Fallback if no calibration (less accurate)
    adc_voltage = (raw_value / 4095.0f) * REF_VOLTAGE;
  }

  // Calculate actual input voltage using voltage divider formula
  // Vin = Vadc * (R1 + R2) / R2
  input_voltage = adc_voltage * (R1 + R2) / R2;

  return input_voltage;
}

void drive_system_setup()
{
  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = (1ULL << RECEIVER_CH1) | (1ULL << RECEIVER_CH2) | (1ULL << RECEIVER_CH3);
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.intr_type = GPIO_INTR_ANYEDGE;
  gpio_config(&io_conf);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(RECEIVER_CH1, signal_isr, (void *)0);
  gpio_isr_handler_add(RECEIVER_CH2, signal_isr, (void *)1);
  gpio_isr_handler_add(RECEIVER_CH3, signal_isr, (void *)2);

  printf("drive_system initialized on GPIO %d (CH1), GPIO %d (CH2), and GPIO %d (CH3)\n", RECEIVER_CH1, RECEIVER_CH2, RECEIVER_CH3);

  drive_system_motors_setup();
  setup_voltage_sensor();
}
void drive_system_loop()
{
  // Get normalized values (0.0 to 1.0)
  float throttle = normalize_pulse(pulse_width_us[1]); // CH1: 0=reverse, 1=forward
  float steering = normalize_pulse(pulse_width_us[0]); // CH2: 0=right, 1=left
  float ch3 = normalize_pulse(pulse_width_us[2]);      // CH3: 0.0 to 1.0

  // Convert to -1.0 to +1.0 range
  throttle_scaled = (throttle * 2.0f) - 1.0f; // -1.0 (reverse) to +1.0 (forward)
  steering_scaled = (steering * 2.0f) - 1.0f; // -1.0 (right) to +1.0 (left)
  ch3_scaled = ch3;                           // Store for other systems

  // Tank drive mixing
  float left_motor = throttle_scaled + steering_scaled;
  float right_motor = throttle_scaled - steering_scaled;

  // Clamp to -1.0 to +1.0 range
  if (left_motor > 1.0f)
    left_motor = 1.0f;
  if (left_motor < -1.0f)
    left_motor = -1.0f;
  if (right_motor > 1.0f)
    right_motor = 1.0f;
  if (right_motor < -1.0f)
    right_motor = -1.0f;

  // Convert to -100 to +100 for motor functions
  int left_speed = (int)(-left_motor * 100.0f);
  int right_speed = (int)(right_motor * 100.0f);

  // Set motor speeds
  set_motor_a_speed(left_speed);  // Left motor
  set_motor_b_speed(right_speed); // Right motor

  float input_voltage = read_voltage_mv();

  // Debug output
  printf("VOLTAGE: %.2f | Throttle: %.2f | Steering: %.2f | CH3: %.2f | Left: %d%% | Right: %d%%\n",
         input_voltage, throttle_scaled, steering_scaled, ch3_scaled, left_speed, right_speed);

  // Use LED manager with normal priority for drive status
  if (throttle_scaled > 0.1f)
  {
    // Moving forward
    led_manager_set(LED_PRIORITY_NORMAL, FX_MODE_STATIC, BLUE, 0, 0);
  }
  else if (throttle_scaled < -0.1f)
  {
    // Moving backward
    led_manager_set(LED_PRIORITY_NORMAL, FX_MODE_STATIC, RED, 0, 0);
  }
  else
  {
    led_manager_set(LED_PRIORITY_NORMAL, FX_MODE_STATIC, YELLOW, 0, 0);
  }
}

void drive_system_motors_setup()
{
  // Setup direction pins (IN1-IN4) as regular GPIO
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << IN1) | (1ULL << IN2) |
                      (1ULL << IN3) | (1ULL << IN4),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
      .hys_ctrl_mode = GPIO_HYS_SOFT_DISABLE, // Changed this
  };
  ESP_ERROR_CHECK(gpio_config(&io_conf));

  // Setup MCPWM for ENA and ENB
  mcpwm_timer_config_t timer_config = {
      .group_id = 0,
      .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
      .resolution_hz = PWM_RESOLUTION_HZ,
      .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
      .period_ticks = PWM_PERIOD_TICKS,
      .intr_priority = 0,
      .flags = {},
  };
  ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

  // Motor A operator
  mcpwm_operator_config_t operator_config = {
      .group_id = 0,
      .intr_priority = 0,
      .flags = {},
  };
  ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &operator_a));
  ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operator_a, timer));

  // Motor B operator
  ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &operator_b));
  ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operator_b, timer));

  // Comparator for Motor A (ENA)
  mcpwm_comparator_config_t comparator_config = {};
  comparator_config.flags.update_cmp_on_tez = true;
  ESP_ERROR_CHECK(mcpwm_new_comparator(operator_a, &comparator_config, &comparator_a));
  ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_a, 0));

  // Comparator for Motor B (ENB)
  ESP_ERROR_CHECK(mcpwm_new_comparator(operator_b, &comparator_config, &comparator_b));
  ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_b, 0));

  // Generator for Motor A (ENA - GPIO28)
  mcpwm_generator_config_t generator_config = {
      .gen_gpio_num = ENA,
      .flags = {},
  };
  ESP_ERROR_CHECK(mcpwm_new_generator(operator_a, &generator_config, &generator_a));

  // Generator for Motor B (ENB - GPIO29)
  generator_config.gen_gpio_num = ENB;
  ESP_ERROR_CHECK(mcpwm_new_generator(operator_b, &generator_config, &generator_b));

  // Set generator actions for Motor A
  ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator_a,
                                                            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
  ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator_a,
                                                              MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_a, MCPWM_GEN_ACTION_LOW)));

  // Set generator actions for Motor B
  ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator_b,
                                                            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
  ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator_b,
                                                              MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_b, MCPWM_GEN_ACTION_LOW)));

  // Enable and start timer
  ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
  ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

  printf("Drive motors setup complete\n");
}

// Helper functions - automatically scale based on PWM_PERIOD_TICKS
void set_motor_a_speed(int speed) // -100 to +100
{
  if (speed > 0)
  {
    // Forward
    gpio_set_level(IN1, 1);
    gpio_set_level(IN2, 0);
    uint32_t compare_val = (speed * PWM_PERIOD_TICKS) / 100;
    mcpwm_comparator_set_compare_value(comparator_a, compare_val);
  }
  else if (speed < 0)
  {
    // Reverse
    gpio_set_level(IN1, 0);
    gpio_set_level(IN2, 1);
    uint32_t compare_val = (-speed * PWM_PERIOD_TICKS) / 100;
    mcpwm_comparator_set_compare_value(comparator_a, compare_val);
  }
  else
  {
    // Brake
    gpio_set_level(IN1, 0);
    gpio_set_level(IN2, 0);
    mcpwm_comparator_set_compare_value(comparator_a, 0);
  }
}

void set_motor_b_speed(int speed) // -100 to +100
{
  if (speed > 0)
  {
    gpio_set_level(IN3, 1);
    gpio_set_level(IN4, 0);
    uint32_t compare_val = (speed * PWM_PERIOD_TICKS) / 100;
    mcpwm_comparator_set_compare_value(comparator_b, compare_val);
  }
  else if (speed < 0)
  {
    gpio_set_level(IN3, 0);
    gpio_set_level(IN4, 1);
    uint32_t compare_val = (-speed * PWM_PERIOD_TICKS) / 100;
    mcpwm_comparator_set_compare_value(comparator_b, compare_val);
  }
  else
  {
    gpio_set_level(IN3, 0);
    gpio_set_level(IN4, 0);
    mcpwm_comparator_set_compare_value(comparator_b, 0);
  }
}