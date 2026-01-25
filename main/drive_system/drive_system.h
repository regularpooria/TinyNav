#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WS2812FX.h>

void drive_system_setup();
void drive_system_loop();
void drive_system_motors_setup();
void set_motor_a_speed(int speed);
void set_motor_b_speed(int speed);
void setup_voltage_sensor();
float read_voltage_mv();

// Shared channel values for other systems
extern float steering_scaled;
extern float throttle_scaled;
extern float ch3_scaled;