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
#include <esp_heap_caps.h>

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
  TfLiteTensor *output_steering = nullptr;
  TfLiteTensor *output_throttle = nullptr;
  int inference_count = 0;

  constexpr int kTensorArenaSize = 96 * 1024; // 96 KB for larger model
  // Allocate tensor arena in PSRAM for ESP32-S3
  uint8_t *tensor_arena = nullptr;

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

  // Allocate tensor arena in PSRAM (External SPIRAM)
  tensor_arena = (uint8_t *)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);
  if (tensor_arena == nullptr)
  {
    MicroPrintf("Failed to allocate tensor arena in PSRAM");
    return;
  }
  MicroPrintf("Tensor arena allocated in PSRAM: %d bytes", kTensorArenaSize);

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
  // Model uses: Conv2D, DepthwiseConv2D, ReLU6, GlobalAveragePooling2D (Mean), FullyConnected
  static tflite::MicroMutableOpResolver<5> resolver;
  resolver.AddConv2D();
  resolver.AddDepthwiseConv2D();
  resolver.AddMean(); // GlobalAveragePooling2D becomes Mean
  resolver.AddFullyConnected();
  resolver.AddRelu6();

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
  output_steering = interpreter->output(0); // First output: steering
  output_throttle = interpreter->output(1); // Second output: throttle

  // Log input/output tensor shapes for debugging
  MicroPrintf("Input shape: [%d, %d, %d, %d]",
              input->dims->data[0], input->dims->data[1],
              input->dims->data[2], input->dims->data[3]);
  MicroPrintf("Output steering shape: [%d, %d]",
              output_steering->dims->data[0], output_steering->dims->data[1]);
  MicroPrintf("Output throttle shape: [%d, %d]",
              output_throttle->dims->data[0], output_throttle->dims->data[1]);

  // Keep track of how many inferences we have performed.
  inference_count = 0;
}
void run_inference()
{
  // Get depth sensor data (25x25) and crop/resize to 24x24
  // depthMap is defined in depth_sensor.h as float[25][25]

  // Center crop: skip first/last row and column to get 24x24 from 25x25
  for (int row = 0; row < 24; row++)
  {
    for (int col = 0; col < 24; col++)
    {
      float depth_value = depthMap[row][col];

      // Normalize depth to [0, 1] range (depth sensor range: 0-2550mm)
      float normalized = depth_value / 2550.0f;
      normalized = normalized > 1.0f ? 1.0f : normalized; // Clamp to [0, 1]

      // Quantize the input from floating-point to int8
      int input_index = row * 24 + col; // Single channel, row-major order
      int8_t quantized = normalized / input->params.scale + input->params.zero_point;
      input->data.int8[input_index] = quantized;
    }
  }

  // Run inference
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk)
  {
    MicroPrintf("Invoke failed\n");
    return;
  }

  // Dequantize outputs
  int8_t steering_quantized = output_steering->data.int8[0];
  int8_t throttle_quantized = output_throttle->data.int8[0];

  float steering = (steering_quantized - output_steering->params.zero_point) *
                   output_steering->params.scale;
  float throttle = (throttle_quantized - output_throttle->params.zero_point) *
                   output_throttle->params.scale;

  // Clamp outputs to expected ranges
  // Steering: [-1, 1] (tanh activation)
  // Throttle: can vary (linear activation)
  steering = steering < -1.0f ? -1.0f : (steering > 1.0f ? 1.0f : steering);
  throttle = throttle < 0.0f ? 0.0f : throttle; // Assuming non-negative throttle

  // TODO: Apply steering and throttle to drive system
  MicroPrintf("Inference: steering=%.3f, throttle=%.3f",
              static_cast<double>(steering), static_cast<double>(throttle));

  inference_count += 1;
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
