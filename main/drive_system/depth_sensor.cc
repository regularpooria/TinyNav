#include "depth_sensor.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

// -------------------- Constants --------------------
#define FRAME_START_BYTE_1 0x00
#define FRAME_START_BYTE_2 0xFF
#define FRAME_END_BYTE 0xDD

// -------------------- Globals --------------------
int imageRows = 25;
int imageCols = 25;

static int tempBuffer = 0;
static int packetState = 0;
static int bufferIndex = 0;

float depthMap[MAX_IMAGE_SIZE][MAX_IMAGE_SIZE];
uint8_t rxBuffer[BUFFER_SIZE];

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
  vTaskDelay(pdMS_TO_TICKS(2000));

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
  vTaskDelay(pdMS_TO_TICKS(6000));

  printf("Setting FPS\n");
  const char *fps_cmd = "AT+FPS=7\r";
  uart_write_bytes(UART_PORT_NUM, fps_cmd, strlen(fps_cmd));
  vTaskDelay(pdMS_TO_TICKS(6000));

  printf("Pixel compression\n");
  char binning_cmd[20];
  snprintf(binning_cmd, sizeof(binning_cmd), "AT+BINN=%d\r", BINNING_FACTOR);
  uart_write_bytes(UART_PORT_NUM, binning_cmd, strlen(binning_cmd));
  vTaskDelay(pdMS_TO_TICKS(6000));

  printf("SENSOR READY\n");
}

// -------------------- Header Parsing --------------------
void readHeader()
{
  FrameHeader header;
  memcpy(&header, rxBuffer, sizeof(FrameHeader));

  imageRows = header.resolution_rows;
  imageCols = header.resolution_cols;

  // Validate resolution
  if (imageRows > MAX_IMAGE_SIZE || imageCols > MAX_IMAGE_SIZE ||
      imageRows <= 0 || imageCols <= 0)
  {
    printf("Warning: Invalid resolution %dx%d, using defaults\n", imageRows, imageCols);
    imageRows = 25;
    imageCols = 25;
  }
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
  if (getPacket())
  {
    readHeader();   // Create the header structure, sync rows and columns
    processDepth(); // Convert raw bytes into mm and create 2D array depthMap
    printDepth();
  }
}

// -------------------- Main Task --------------------
void depth_sensor_task()
{
  while (1)
  {
    fullPrint();
    vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay
  }
}