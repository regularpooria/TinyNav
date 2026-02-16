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

  // Inference results - persist between frames
  float inference_steering = 0.0f;
  float inference_throttle = 0.0f;

  // Inference task handle and synchronization for async inference on core 1
  TaskHandle_t inference_task_handle = nullptr;
  SemaphoreHandle_t depth_mutex = nullptr;
  volatile bool inference_requested = false;

  // Separate depth buffer for inference to avoid blocking sensor
  float *inference_depthMap = nullptr;

} // namespace

// Inference task that runs on core 1
void inference_task(void *pvParameters)
{
  while (true)
  {
    // Check if inference is requested
    if (inference_requested)
    {
      run_inference();
      inference_requested = false;
    }
    else
    {
      // Wait briefly before checking again
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

// Non-blocking function to request inference with frame copy
void request_inference()
{
  if (!inference_requested && inference_depthMap != nullptr && depth_mutex != nullptr)
  {
    // Use mutex to safely copy depth data from core 0 to core 1
    if (xSemaphoreTake(depth_mutex, pdMS_TO_TICKS(5)) == pdTRUE)
    {
      // Copy 25x25 depth frame
      memcpy(inference_depthMap, depthMap, 25 * 25 * sizeof(float));
      xSemaphoreGive(depth_mutex);

      inference_requested = true;
    }
  }
}

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

  // Allocate inference depth buffer for core 1
  inference_depthMap = (float *)malloc(25 * 25 * sizeof(float));
  if (inference_depthMap == nullptr)
  {
    MicroPrintf("Failed to allocate inference depth buffer");
    return;
  }
  MicroPrintf("Inference depth buffer allocated: %d bytes", 25 * 25 * sizeof(float));

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
  // Model uses: Conv2D, DepthwiseConv2D, ReLU6, GlobalAveragePooling2D (Mean), FullyConnected, Tanh
  static tflite::MicroMutableOpResolver<7> resolver;
  resolver.AddConv2D();
  resolver.AddDepthwiseConv2D();
  resolver.AddMean(); // GlobalAveragePooling2D becomes Mean
  resolver.AddFullyConnected();
  resolver.AddRelu6();
  resolver.AddTanh();
  resolver.AddLogistic();

  // static tflite::MicroMutableOpResolver<8> resolver;
  // resolver.AddConv2D();
  // resolver.AddDepthwiseConv2D();
  // resolver.AddMean();    // For GlobalAveragePooling2D
  // resolver.AddReshape(); // <--- Often needed between Pooling and Dense
  // resolver.AddFullyConnected();
  // resolver.AddHardSwish();
  // resolver.AddTanh();
  // resolver.AddLogistic();

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
  // IMPORTANT: Output 0 is THROTTLE, Output 1 is STEERING (based on training code)
  output_throttle = interpreter->output(0); // First output: throttle
  output_steering = interpreter->output(1); // Second output: steering

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

  // Create mutex for thread-safe depth data access
  depth_mutex = xSemaphoreCreateMutex();
  if (depth_mutex == nullptr)
  {
    MicroPrintf("Failed to create depth mutex");
    return;
  }

  // Create inference task on core 1
  BaseType_t result = xTaskCreatePinnedToCore(
      inference_task,         // Task function
      "inference_task",       // Task name
      8192,                   // Stack size (8KB)
      nullptr,                // Parameters
      5,                      // Priority (same as main loop)
      &inference_task_handle, // Task handle
      1                       // Pin to core 1
  );

  if (result != pdPASS)
  {
    MicroPrintf("Failed to create inference task on core 1");
    return;
  }

  MicroPrintf("Inference task created on core 1");
}
void run_inference()
{
  // Check if interpreter is initialized
  if (interpreter == nullptr || input == nullptr || output_steering == nullptr || output_throttle == nullptr)
  {
    MicroPrintf("Inference skipped: model not initialized");
    return;
  }

  // Get depth sensor data (25x25), rotate 90° clockwise, then crop to 24x24
  // Use inference_depthMap copy to avoid blocking sensor
  // Python: np.rot90(data, k=-1, axes=(1, 2)) rotates clockwise
  // For 90° clockwise rotation: rotated[i][j] = original[24-j][i]
  for (int row = 0; row < 24; row++)
  {
    for (int col = 0; col < 24; col++)
    {
      // Apply 90° clockwise rotation, then crop center 24x24
      // Map output (row, col) back to input coordinates
      int src_row = 24 - col; // For clockwise: new_row comes from flipped col
      int src_col = row;      // For clockwise: new_col comes from row

      // Access from copied buffer
      float depth_value = inference_depthMap[src_row * 25 + src_col];

      // Normalize depth to [0, 1] range (depth sensor range: 0-2550mm)
      float normalized = depth_value / 2550.0f;
      normalized = normalized > 1.0f ? 1.0f : normalized; // Clamp to [0, 1]

      // Quantize the input: quantized = (value / scale + zero_point) truncated to int8
      // Matches Python: (test_input / input_scale + input_zero).astype(np.int8)
      int input_index = row * 24 + col; // Single channel, row-major order
      float quantized_float = normalized / input->params.scale + input->params.zero_point;
      int32_t quantized = static_cast<int32_t>(quantized_float);
      // Clamp to int8 range
      if (quantized < -128)
        quantized = -128;
      if (quantized > 127)
        quantized = 127;
      input->data.int8[input_index] = static_cast<int8_t>(quantized);
    }
  }

  // Run inference
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk)
  {
    MicroPrintf("Invoke failed\n");
    return;
  }

  // Dequantize outputs: dequantized = (quantized - zero_point) * scale
  int8_t steering_quantized = output_steering->data.int8[0];
  int8_t throttle_quantized = output_throttle->data.int8[0];

  float steering = (float)(steering_quantized - output_steering->params.zero_point) *
                   output_steering->params.scale;
  float throttle = (float)(throttle_quantized - output_throttle->params.zero_point) *
                   output_throttle->params.scale;

  // Clamp outputs to expected ranges
  // Steering: [-1, 1] (tanh activation)
  // Throttle: [-1, 1] (linear activation, allows reverse)
  steering = steering < -1.0f ? -1.0f : (steering > 1.0f ? 1.0f : steering);
  throttle = throttle < -1.0f ? -1.0f : (throttle > 1.0f ? 1.0f : throttle);

  // Save inference results to persist between frames
  inference_steering = steering;
  inference_throttle = throttle;

  // TODO: Apply steering and throttle to drive system
  MicroPrintf("Inference: steering=%.3f, throttle=%.3f",
              static_cast<double>(steering), static_cast<double>(throttle));

  inference_count += 1;
}

// Getter functions for inference results
float get_inference_steering()
{
  return inference_steering;
}

float get_inference_throttle()
{
  return inference_throttle;
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
