#include "depth_sensor.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../sdcard/sd.h"
#include <WS2812FX.h>
#include "../led_manager.h"
#include "../main_functions.h"

// -------------------- Constants --------------------
#define FRAME_START_BYTE_1 0x00
#define FRAME_START_BYTE_2 0xFF
#define FRAME_END_BYTE 0xDD
#define SD_CARD_COOLDOWN 10 // Ticks

// -------------------- Globals --------------------
int imageRows = 25;
int imageCols = 25;

static int tempBuffer = 0;
static int packetState = 0;
static int bufferIndex = 0;

float depthMap[MAX_IMAGE_SIZE][MAX_IMAGE_SIZE];
uint8_t rxBuffer[BUFFER_SIZE];

char g_depth_log_filename[64];
int g_frame_counter = 0;
FILE *g_depth_log_file = NULL;
short write_to_sd = 0;

int sd_card_cooldown = 0;
static int dropped_frames = 0;

// SD async writer queue
static QueueHandle_t g_sd_queue = NULL;
static StaticQueue_t g_sd_queue_static;
static uint8_t g_sd_queue_storage[SD_QUEUE_DEPTH * sizeof(sd_frame_t)];

// -------------------- Initialization --------------------
void depth_sensor_init()
{
  uart_config_t uart_config = {
      .baud_rate = 115200, // Start at 115200; switched to UART_BAUD_RATE via AT+BAUD below
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,
      .source_clk = UART_SCLK_DEFAULT,
      .flags = {
          .allow_pd = 0,
          .backup_before_sleep = 0}};

  uart_param_config(UART_PORT_NUM, &uart_config);
  uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(UART_PORT_NUM, BUFFER_SIZE * 2, 0, 0, NULL, 0);

  printf("Depth sensor UART initialized\n");

  // Wait before sending commands (mimics Arduino delay)
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Switch sensor baud rate to 230400, then re-init UART to match.
  // Send at current rate (115200), sensor switches immediately after ACK.
  printf("Switching baud rate to 230400\n");
  const char *baud_cmd = "AT+BAUD=3\r";
  uart_write_bytes(UART_PORT_NUM, baud_cmd, strlen(baud_cmd));
  vTaskDelay(pdMS_TO_TICKS(200)); // Wait for sensor to apply change

  // Re-configure ESP32 UART to 230400 to match sensor
  uart_set_baudrate(UART_PORT_NUM, UART_BAUD_RATE);
  uart_flush(UART_PORT_NUM); // Discard any garbage from the baud transition
  vTaskDelay(pdMS_TO_TICKS(200));
  printf("UART now at %d baud\n", UART_BAUD_RATE);

  // Send sensor commands
#ifdef USE_NONLINEAR
  const char *unit_cmd = "AT+UNIT=0\r";
  uart_write_bytes(UART_PORT_NUM, unit_cmd, strlen(unit_cmd));
  vTaskDelay(pdMS_TO_TICKS(2000));
#elif defined(USE_LINEAR)
  char unit_cmd[20];
  snprintf(unit_cmd, sizeof(unit_cmd), "AT+UNIT=%d\r", UNIT_VALUE);
  uart_write_bytes(UART_PORT_NUM, unit_cmd, strlen(unit_cmd));
  vTaskDelay(pdMS_TO_TICKS(2000));
#endif

  printf("UART display turning on\n");
  const char *disp_cmd = "AT+DISP=7\r";
  uart_write_bytes(UART_PORT_NUM, disp_cmd, strlen(disp_cmd));
  vTaskDelay(pdMS_TO_TICKS(5000));

  printf("Setting FPS\n");
  const char *fps_cmd = "AT+FPS=19\r";
  uart_write_bytes(UART_PORT_NUM, fps_cmd, strlen(fps_cmd));
  vTaskDelay(pdMS_TO_TICKS(5000));

  printf("Pixel compression\n");
  char binning_cmd[20];
  snprintf(binning_cmd, sizeof(binning_cmd), "AT+BINN=%d\r", BINNING_FACTOR);
  uart_write_bytes(UART_PORT_NUM, binning_cmd, strlen(binning_cmd));
  vTaskDelay(pdMS_TO_TICKS(5000));

  printf("SENSOR READY\n");

  g_sd_queue = xQueueCreateStatic(SD_QUEUE_DEPTH, sizeof(sd_frame_t),
                                   g_sd_queue_storage, &g_sd_queue_static);

  char buffer[256];
  size_t bytes_read = 0;
  bool counter_exists = false;
  int counter = 0;

  esp_err_t counter_err = sd_card_file_exists("/counter.txt", &counter_exists);

  if (counter_err == ESP_OK && counter_exists)
  {

    esp_err_t errr = sd_card_read_file(
        "/counter.txt",
        buffer,
        sizeof(buffer) - 1,
        &bytes_read);

    if (errr == ESP_OK)
    {
      buffer[bytes_read] = '\0';

      // Convert string to int
      counter = atoi(buffer);
      counter++; // increment counter
    }
    else
    {
      printf("Read failed: %d\n", errr);
      write_to_sd = -1;
    }
  }
  else
  {
    // File does not exist → start counter at 1
    counter = 1;
  }

  // Write updated counter back to file
  FILE *g_counter_file = sd_card_fopen("/counter.txt", "w");
  if (g_counter_file)
  {
    fprintf(g_counter_file, "%d", counter);
    fclose(g_counter_file);
  }
  else
  {
    printf("Failed to open counter file for writing\n");
  }

  // Build filename from counter
  snprintf(
      g_depth_log_filename,
      sizeof(g_depth_log_filename),
      "/revised_log_%04d.csv",
      counter);
  if (write_to_sd != -1)
  {
    printf("Logging to: %s\n", g_depth_log_filename);

    // Create and open file
    g_depth_log_file = sd_card_fopen(g_depth_log_filename, "w");
    if (g_depth_log_file == NULL)
    {
      printf("Failed to open log file: %s\n", g_depth_log_filename);
      write_to_sd = -1;
      printf("Initial mode: Error - SD card not available (toggle with CH3)\n");
      return;
    }

    // Each frame is ~3KB of CSV. 32KB buffers ~10 frames before a physical SD write,
    // reducing write stalls from every 2-3 frames down to every ~10 frames.
    static char file_buffer[32768];
    setvbuf(g_depth_log_file, file_buffer, _IOFBF, sizeof(file_buffer));

    // Write header
    fprintf(g_depth_log_file, "# Depth Sensor Log\n");
    fprintf(g_depth_log_file, "# Binning Factor: %d\n", BINNING_FACTOR);
    fprintf(g_depth_log_file, "# Frame,Steering(millis),Throttle(millis),Width,Height,Data...\n");

    fflush(g_depth_log_file);
    printf("Initial mode: Off (cycle modes with CH3: Off -> Serial -> SD -> Inference -> Off)\n");
  }
  else
  {
    printf("Initial mode: Error - SD card not available (toggle with CH3 for serial print)\n");
  }
}

// -------------------- Header Parsing --------------------
bool readHeader()
{
  FrameHeader header;
  memcpy(&header, rxBuffer, sizeof(FrameHeader));

  imageRows = header.resolution_rows;
  imageCols = header.resolution_cols;

  // Validate resolution
  if (imageRows > MAX_IMAGE_SIZE || imageCols > MAX_IMAGE_SIZE ||
      imageRows <= 0 || imageCols <= 0)
  {
    // printf("Warning: Invalid resolution %dx%d, dropping frame\n", imageRows, imageCols);
    return false; // Invalid frame, drop it
  }
  return true; // Valid frame
}

// -------------------- Depth Processing --------------------
void processDepth()
{
  int pixelIndex = 0;

  // Read pixel data (starts at headerSize, ends 2 bytes before end marker)
  for (int i = HEADER_SIZE; i < tempBuffer - 2; i++)
  {
    int row = pixelIndex / imageCols;
    int col = pixelIndex % imageCols;

    // Safety check
    if (row < MAX_IMAGE_SIZE && col < MAX_IMAGE_SIZE)
    {
      depthMap[row][col] = toMillimeters(rxBuffer[i]);
    }
    pixelIndex++;
  }
}

float toMillimeters(uint8_t pixelValue)
{
#ifdef USE_NONLINEAR
  // Sensor uses formula 5.1*sqrt(x)
  // Reverse: divide by 5.1 then square
  float normalized = pixelValue / 5.1f;
  return normalized * normalized;
#elif defined(USE_LINEAR)
  return pixelValue * UNIT_VALUE;
#else
  // Default: return raw pixel value if no mode is defined
  return (float)pixelValue;
#endif
}

// -------------------- Print --------------------
const char depthChars[] = " .:-=+*#%@";
const int NUM_CHARS = sizeof(depthChars) - 1;

float minDepth = 50.0f;   // mm
float maxDepth = 1000.0f; // mm
void printDepthAscii()
{
  printf("FRAME_START\n");
  fflush(stdout);

  for (int i = 0; i < imageRows; i++)
  {
    for (int j = 0; j < imageCols; j++)
    {
      float d = depthMap[i][j];

      if (d < minDepth)
        d = minDepth;
      if (d > maxDepth)
        d = maxDepth;

      float norm = (d - minDepth) / (maxDepth - minDepth);
      int idx = (int)(norm * (NUM_CHARS - 1));

      if (idx < 0)
        idx = 0;
      if (idx >= NUM_CHARS)
        idx = NUM_CHARS - 1;

      putchar(depthChars[idx]);
    }
    putchar('\n');
  }

  printf("FRAME_END\n");
  fflush(stdout);
}

// -------------------- Packet Reception --------------------
bool getPacket()
{
  uint8_t byte;
  int len = uart_read_bytes(UART_PORT_NUM, &byte, 1, 0); // non-blocking

  if (len <= 0)
    return false;

  // ---------------------------------------STATE 0------------------------------
  if (packetState == 0)
  {
    // STATE 0, waiting for start (0x00)
    if (byte == FRAME_START_BYTE_1)
    {
      // Found start byte (0x00), reset buffer and save the byte
      bufferIndex = 0;
      rxBuffer[bufferIndex++] = byte;
      // Transitioning to state 1 now
      packetState = 1;
    }
  }
  // ---------------------------------------STATE 1-----------------------------------
  else if (packetState == 1)
  {
    // STATE 1, waiting for second start byte (0xFF)
    if (byte == FRAME_START_BYTE_2)
    {
      // Save new byte to the buffer
      rxBuffer[bufferIndex++] = byte;
      // Transitioning to state 2 now
      packetState = 2;
    }
    else
    {
      // ERROR, follow up byte after 0x00 was not 0xFF, return to packet state 0
      packetState = 0;
    }
  }
  // -------------------------------------STATE 2---------------------------------------
  else if (packetState == 2)
  {
    // STATE 2, data collection
    // This loop continues here to collect bytes until the final byte is sent (0xDD)
    rxBuffer[bufferIndex++] = byte;

    if (byte == FRAME_END_BYTE)
    {
      // This is the end marker, now all of the data is in the rxBuffer array
      // Reset to state 0 and wait for next frame
      packetState = 0;
      tempBuffer = bufferIndex;
      bufferIndex = 0;
      return true;
    }
    else if (bufferIndex >= BUFFER_SIZE)
    {
      // Safety check so that if we miss 0xDD the buffer does not overflow
      printf("\n----- BUFFER OVERFLOW (Dropping packet) -----\n");
      packetState = 0;
      bufferIndex = 0;
    }
  }

  return false;
}

// -------------------- Full Processing --------------------
void fullPrint()
{
}

// -------------------- Main Task --------------------
void depth_sensor_task(float *steering_ptr, float *throttle_ptr, float *ch3_ptr)
{
  static float prev_ch3 = 0.0;
  static int fps_frame_count = 0;
  static int64_t fps_last_time = 0;
  static int64_t t_getpacket_us = 0, t_processdepth_us = 0, t_append_us = 0;

  int64_t t0;

  t0 = esp_timer_get_time();
  while (!getPacket())
  {
    vTaskDelay(pdMS_TO_TICKS(1)); // Small delay to avoid hogging CPU
  }
  t_getpacket_us += esp_timer_get_time() - t0;

  // Discard any frames that queued up while we were processing the previous one.
  // This ensures we always work with the freshest data, not stale buffered frames.
  uart_flush_input(UART_PORT_NUM);
  packetState = 0;
  bufferIndex = 0;

  if (!readHeader()) // Create the header structure, sync rows and columns
  {
    return; // Drop frame if invalid resolution
  }

  t0 = esp_timer_get_time();
  processDepth(); // Convert raw bytes into mm and create 2D array depthMap
  t_processdepth_us += esp_timer_get_time() - t0;

  // FPS tracking: print every second
  fps_frame_count++;
  int64_t now = esp_timer_get_time(); // microseconds
  if (fps_last_time == 0)
  {
    fps_last_time = now;
  }
  else if (now - fps_last_time >= 1000000)
  {
    if (write_to_sd == 2)
    {
      float elapsed_s = (now - fps_last_time) / 1000000.0f;
      float fps = fps_frame_count / elapsed_s;
      printf("FPS: %.1f | getPacket: %ldus  processDepth: %ldus  appendFrame: %ldus  dropped: %d\n",
             fps,
             (long)(t_getpacket_us / fps_frame_count),
             (long)(t_processdepth_us / fps_frame_count),
             (long)(t_append_us / fps_frame_count),
             dropped_frames);
    }
    fps_frame_count = 0;
    fps_last_time = now;
    t_getpacket_us = 0;
    t_processdepth_us = 0;
    t_append_us = 0;
    dropped_frames = 0;
  }

  // Mode cycling with CH3 button (edge detection: rising edge when ch3 goes from <0.5 to >=0.5)
  if (*ch3_ptr >= 0.5 && prev_ch3 < 0.5 && sd_card_cooldown == 0)
  {
    if (write_to_sd == -1)
    {
      // SD card error: toggle between -1 (error/off) and 1 (serial print)
      write_to_sd = 1;
      sd_card_cooldown = SD_CARD_COOLDOWN;
      printf("Mode: Serial print (SD card not available)\n");
      led_manager_set(LED_PRIORITY_HIGH, FX_MODE_STATIC, YELLOW, 0, 2000);
    }
    else if (write_to_sd == 1)
    {
      // If in serial mode and from error state, go back to error state
      // Otherwise try to go to SD mode
      if (g_depth_log_file == NULL)
      {
        write_to_sd = -1;
        sd_card_cooldown = SD_CARD_COOLDOWN;
        printf("Mode: Off (SD card not available)\n");
        led_manager_set(LED_PRIORITY_HIGH, FX_MODE_STATIC, RED, 0, 2000);
      }
      else
      {
        write_to_sd = 2;
        sd_card_cooldown = SD_CARD_COOLDOWN;
        printf("Mode: Writing to SD card\n");
        led_manager_set(LED_PRIORITY_HIGH, FX_MODE_BLINK, PURPLE, 500, 0); // Blink while recording
      }
    }
    else
    {
      // From mode 0, 2, or 3, cycle to next
      int prev_mode = write_to_sd;
      write_to_sd = (write_to_sd + 1) % 4;
      sd_card_cooldown = SD_CARD_COOLDOWN;

      // Clear LED queue when switching modes to prevent stale commands
      led_manager_clear();

      if (write_to_sd == 0)
      {
        // Enqueue a sentinel so the writer task flushes cleanly before we stop sending
        if (prev_mode == 2 && g_sd_queue != NULL)
        {
          sd_frame_t sentinel = {};
          sentinel.len = 0;
          xQueueSend(g_sd_queue, &sentinel, portMAX_DELAY);
        }
        printf("Mode: Off (no output)\n");
        led_manager_set(LED_PRIORITY_HIGH, FX_MODE_STATIC, GREEN, 0, 2000);
      }
      else if (write_to_sd == 1)
      {
        // Enqueue a sentinel so the writer task flushes cleanly before we stop sending
        if (prev_mode == 2 && g_sd_queue != NULL)
        {
          sd_frame_t sentinel = {};
          sentinel.len = 0;
          xQueueSend(g_sd_queue, &sentinel, portMAX_DELAY);
        }
        printf("Mode: Serial print\n");
        led_manager_set(LED_PRIORITY_HIGH, FX_MODE_STATIC, BLUE, 0, 2000);
      }
      else if (write_to_sd == 2)
      {
        printf("Mode: Writing to SD card\n");
        led_manager_set(LED_PRIORITY_HIGH, FX_MODE_BLINK, PURPLE, 500, 0);
      }
      else if (write_to_sd == 3)
      {
        // Enqueue a sentinel so the writer task flushes cleanly before we stop sending
        if (prev_mode == 2 && g_sd_queue != NULL)
        {
          sd_frame_t sentinel = {};
          sentinel.len = 0;
          xQueueSend(g_sd_queue, &sentinel, portMAX_DELAY);
        }
        // Clear stale frames so inference waits for 10 fresh ones
        reset_frame_buffer();
        printf("Mode: Inference\n");
        led_manager_set(LED_PRIORITY_HIGH, FX_MODE_BLINK, CYAN, 500, 0); // Blink while running AI
      }
    }
  }

  prev_ch3 = *ch3_ptr; // Update previous value

  // Execute based on current mode
  if (write_to_sd == 1)
  {
    printDepthAscii(); // Print to serial
  }
  else if (write_to_sd == 2)
  {
    t0 = esp_timer_get_time();
    appendDepthFrame(*steering_ptr, *throttle_ptr); // Write to SD card with control values
    t_append_us += esp_timer_get_time() - t0;
  }
  else if (write_to_sd == 3)
  {
    // if (frame_skip_counter % 2 == 0)
    // {
    add_frame_to_buffer();
    request_inference();
    // }
    // frame_skip_counter++;
    // if (frame_skip_counter >= 10000)
    // {
    //   frame_skip_counter = 0;
    // }
    // Add current frame to buffer (rotated and cropped to 24x24)
  }

  // Decrement cooldown
  if (sd_card_cooldown > 0)
  {
    sd_card_cooldown = sd_card_cooldown - 1;
  }
}

// -------------------- Frame Export --------------------
// Formats the frame into a CSV line and enqueues it for the writer task.
// Returns immediately - does NOT block on SD card I/O.
// Steering/throttle are stored as integer millis (e.g. 1.234 -> 1234) to avoid
// float formatting, which uses the deep-stack Ryu algorithm and causes stack overflows.
bool appendDepthFrame(float steering, float throttle)
{
  if (g_sd_queue == NULL)
    return false;

  static sd_frame_t item;
  int steering_i = (int)(steering * 1000);
  int throttle_i = (int)(throttle * 1000);
  int pos = snprintf(item.data, sizeof(item.data), "%d,%d,%d,%d,%d",
                     g_frame_counter++, steering_i, throttle_i, imageCols, imageRows);

  for (int i = 0; i < imageRows; i++)
    for (int j = 0; j < imageCols; j++)
      pos += snprintf(item.data + pos, sizeof(item.data) - pos, ",%d", (int)depthMap[i][j]);

  item.data[pos++] = '\n';
  item.len = pos;

  // Non-blocking: drop frame if the writer task has fallen behind
  if (xQueueSend(g_sd_queue, &item, 0) != pdTRUE)
  {
    dropped_frames++;
    return false;
  }
  return true;
}

// -------------------- SD Writer Task --------------------
// Runs on its own FreeRTOS task. Drains the queue and writes to the SD card.
// SD write stalls (erase/wear-levelling) no longer affect the sensor loop.
// A sentinel item (len == 0) triggers a final fflush + fsync on the file.
void sd_writer_task(void *pvParameters)
{
  static sd_frame_t item;
  int flush_counter = 0;

  while (true)
  {
    if (xQueueReceive(g_sd_queue, &item, portMAX_DELAY) == pdTRUE)
    {
      if (g_depth_log_file == NULL)
        continue;

      if (item.len == 0)
      {
        // Sentinel: drain is done, safe to flush and sync
        fflush(g_depth_log_file);
        fsync(fileno(g_depth_log_file));
        flush_counter = 0;
      }
      else
      {
        fwrite(item.data, 1, item.len, g_depth_log_file);
        if (++flush_counter >= 20)
        {
          fflush(g_depth_log_file);
          flush_counter = 0;
        }
      }
    }
  }
}