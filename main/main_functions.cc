/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "main_functions.h"
#include "model.h"
#include "constants.h"
#include "output_handler.h"

#include "drive_system/drive_system.h"
#include "drive_system/depth_sensor.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "sdcard/sd.h"

// LED‌ stuff
#include <WS2812FX.h>
#include "led_manager.h"

// Serial commands
#include "serial_commands.h"

// Globals, used for compatibility with Arduino-style sketches.
namespace
{
  const tflite::Model *model = nullptr;
  tflite::MicroInterpreter *interpreter = nullptr;
  TfLiteTensor *input = nullptr;
  TfLiteTensor *output = nullptr;
  int inference_count = 0;

  constexpr int kTensorArenaSize = 2000;
  uint8_t tensor_arena[kTensorArenaSize];

  const int NUM_LEDS = 8;
  const int LED_GPIO = 50;
  led_strip_handle_t led_strip = nullptr;
  WS2812FX *fx = nullptr;

} // namespace

// The name of this function is important for Arduino compatibility.
void setup()
{
  vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay before init
  // xTaskCreate(depth_sensor_task, "depth_sensor_task", 8192, NULL, 5, NULL);
  sd_card_config_t config = sd_card_get_default_config();
  esp_err_t ret = sd_card_init(&config);
  if (ret == ESP_OK)
  {
    ESP_LOGI("MAIN", "SD card initialized successfully!");
    sd_card_print_info();
  }
  else
  {
    ESP_LOGE("MAIN", "SD card init failed: %s", esp_err_to_name(ret));
  }

  depth_sensor_init();
  drive_system_setup();
  setup_leds();
  serial_commands_init();
  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model = tflite::GetModel(g_model);
  if (model->version() != TFLITE_SCHEMA_VERSION)
  {
    MicroPrintf("Model provided is schema version %d not equal to supported "
                "version %d.",
                model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  // Pull in only the operation implementations we need.
  static tflite::MicroMutableOpResolver<1> resolver;
  if (resolver.AddFullyConnected() != kTfLiteOk)
  {
    return;
  }

  // Build an interpreter to run the model with.
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk)
  {
    MicroPrintf("AllocateTensors() failed");
    return;
  }

  // Obtain pointers to the model's input and output tensors.
  input = interpreter->input(0);
  output = interpreter->output(0);

  // Keep track of how many inferences we have performed.
  inference_count = 0;
}
void run_inference()
{
  // Calculate an x value to feed into the model. We compare the current
  // inference_count to the number of inferences per cycle to determine
  // our position within the range of possible x values the model was
  // trained on, and use this to calculate a value.
  float position = static_cast<float>(inference_count) /
                   static_cast<float>(kInferencesPerCycle);
  float x = position * kXrange;

  // Quantize the input from floating-point to integer
  int8_t x_quantized = x / input->params.scale + input->params.zero_point;
  // Place the quantized input in the model's input tensor
  input->data.int8[0] = x_quantized;

  // Run inference, and report any error
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk)
  {
    MicroPrintf("Invoke failed on x: %f\n",
                static_cast<double>(x));
    return;
  }

  // Obtain the quantized output from model's output tensor
  int8_t y_quantized = output->data.int8[0];
  // Dequantize the output from integer to floating-point
  float y = (y_quantized - output->params.zero_point) * output->params.scale;

  // Output the results. A custom HandleOutput function can be implemented
  // for each supported hardware target.
  HandleOutput(x, y);

  // Increment the inference_counter, and reset it if we have reached
  // the total number per cycle
  inference_count += 1;
  if (inference_count >= kInferencesPerCycle)
    inference_count = 0;
}

void setup_leds()
{
  led_strip_config_t strip_config = {
      .strip_gpio_num = LED_GPIO,
      .max_leds = NUM_LEDS,
      .led_model = LED_MODEL_WS2812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
      .flags = {
          .invert_out = false,
      }};

  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10 * 1000 * 1000,
      .mem_block_symbols = 0, // Let the driver choose
      .flags = {
          .with_dma = false,
      }};

  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

  fx = new WS2812FX(NUM_LEDS, led_strip);

  // Have a colour show up as a sign that LEDs are alive
  fx->init();
  fx->setBrightness(100);
  fx->setSpeed(1000);
  fx->setMode(FX_MODE_BREATH);
  fx->setColor(YELLOW);
  fx->start();

  // Initialize LED manager
  led_manager_init(fx);
}
// The name of this function is important for Arduino compatibility.
void loop()
{
  // run_inference();

  // Run drive system and LED updates
  drive_system_loop();

  // Run depth sensor with shared channel values
  depth_sensor_task(&steering_scaled, &throttle_scaled, &ch3_scaled);

  // LED stuff
  led_manager_update();
  fx->service();
  
  // Process serial commands
  serial_commands_process();
}
