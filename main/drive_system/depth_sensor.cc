#include "depth_sensor.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../sdcard/sd.h"
#include <WS2812FX.h>
#include "../led_manager.h"

// -------------------- Constants --------------------
#define FRAME_START_BYTE_1 0x00
#define FRAME_START_BYTE_2 0xFF
#define FRAME_END_BYTE 0xDD
#define SD_CARD_COOLDOWN 50 // Ticks

// -------------------- Globals --------------------
int imageRows = 25;
int imageCols = 25;

static int tempBuffer = 0;
static int packetState = 0;
static int bufferIndex = 0;

float depthMap[MAX_IMAGE_SIZE][MAX_IMAGE_SIZE];
uint8_t rxBuffer[BUFFER_SIZE];

static char g_depth_log_filename[64];
int g_frame_counter = 0;
FILE *g_depth_log_file = NULL;
short write_to_sd = 0;

int sd_card_cooldown = 0;

// -------------------- Initialization --------------------
void depth_sensor_init()
{
  uart_config_t uart_config = {
      .baud_rate = UART_BAUD_RATE,
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
  const char *fps_cmd = "AT+FPS=7\r";
  uart_write_bytes(UART_PORT_NUM, fps_cmd, strlen(fps_cmd));
  vTaskDelay(pdMS_TO_TICKS(5000));

  printf("Pixel compression\n");
  char binning_cmd[20];
  snprintf(binning_cmd, sizeof(binning_cmd), "AT+BINN=%d\r", BINNING_FACTOR);
  uart_write_bytes(UART_PORT_NUM, binning_cmd, strlen(binning_cmd));
  vTaskDelay(pdMS_TO_TICKS(5000));

  printf("SENSOR READY\n");

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
  if (write_to_sd == 1)
  {
    printf("Logging to: %s\n", g_depth_log_filename);

    // Create and open file
    g_depth_log_file = sd_card_fopen(g_depth_log_filename, "w");
    if (g_depth_log_file == NULL)
    {
      printf("Failed to open log file: %s\n", g_depth_log_filename);
      return;
    }

    // Write header
    fprintf(g_depth_log_file, "# Depth Sensor Log\n");
    fprintf(g_depth_log_file, "# Binning Factor: %d\n", BINNING_FACTOR);
    fprintf(g_depth_log_file, "# Frame,Width,Height,Data...\n");

    fflush(g_depth_log_file);
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
    printf("Warning: Invalid resolution %dx%d, dropping frame\n", imageRows, imageCols);
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
void printDepth()
{
  printf("----- DEPTH DATA (mm) -----\n");

  for (int i = 0; i < imageRows; i++)
  {
    for (int j = 0; j < imageCols; j++)
    {
      printf("%.0f\t", depthMap[i][j]);
    }
    printf("\n");
  }
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
  static DepthFrame frame; // Static to avoid stack overflow

  while (!getPacket()) // Keep reading until full packet received
  {
    vTaskDelay(pdMS_TO_TICKS(1)); // Small delay to avoid hogging CPU
  }

  if (!readHeader()) // Create the header structure, sync rows and columns
  {
    return; // Drop frame if invalid resolution
  }
  processDepth(); // Convert raw bytes into mm and create 2D array depthMap
  // printDepth();

  // Log to SD card

  if (write_to_sd != -1 && *ch3_ptr >= 0.5 && sd_card_cooldown == 0)
  {
    if (write_to_sd == 1)
    {

      write_to_sd = 0;
      sd_card_cooldown = SD_CARD_COOLDOWN;
      printf("Stopped writing to SD");
      led_manager_set(LED_PRIORITY_HIGH, FX_MODE_STATIC, GREEN, 0, 2000); // 2 second flash
    }
    else
    {
      write_to_sd = 1;
      sd_card_cooldown = SD_CARD_COOLDOWN;
      printf("Started writing to SD");
      led_manager_set(LED_PRIORITY_HIGH, FX_MODE_BLINK, PURPLE, 500, 0); // Blink while recording
    }
  }
  else if (write_to_sd == -1 && *ch3_ptr >= 0.5)
  {
    printf("The SD card is not initialized to turn on logging");
    led_manager_set(LED_PRIORITY_CRITICAL, FX_MODE_BLINK, RED, 200, 3000); // Fast red blink for error
  }

  if (write_to_sd == 1)
  {
    getDepthFrame(&frame);
    appendDepthFrame(&frame);
  }
  if (sd_card_cooldown > 0)
  {
    sd_card_cooldown = sd_card_cooldown - 1;
  }
}

// -------------------- Frame Export --------------------
void getDepthFrame(DepthFrame *frame)
{
  if (frame == NULL)
    return;

  frame->width = imageCols;
  frame->height = imageRows;
  frame->binning_factor = BINNING_FACTOR;

  // Copy depth data
  for (int i = 0; i < imageRows; i++)
  {
    for (int j = 0; j < imageCols; j++)
    {
      frame->data[i][j] = depthMap[i][j];
    }
  }
}

bool appendDepthFrame(const DepthFrame *frame)
{
  if (frame == NULL || g_depth_log_file == NULL)
    return false;

  // Write frame header directly
  fprintf(g_depth_log_file, "%d,%d,%d", g_frame_counter++, frame->width, frame->height);

  // Write depth data row by row to avoid huge buffer
  for (int i = 0; i < frame->height; i++)
  {
    for (int j = 0; j < frame->width; j++)
    {
      fprintf(g_depth_log_file, ",%.1f", frame->data[i][j]);
    }
  }
  fprintf(g_depth_log_file, "\n");

  // Sync to disk every 500 frames for power loss protection
  if (g_frame_counter % 20 == 0)
  {
    fflush(g_depth_log_file);
    fsync(fileno(g_depth_log_file));
  }

  return true;
}