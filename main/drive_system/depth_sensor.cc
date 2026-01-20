#include "depth_sensor.h"
#include "driver/gpio.h"
#include "driver/uart.h"
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
      .source_clk = UART_SCLK_XTAL,
      .flags = {
          .allow_pd = 0,
          .backup_before_sleep = 0}};

  uart_param_config(UART_PORT_NUM, &uart_config);
  uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(UART_PORT_NUM, BUFFER_SIZE * 2, 0, 0, NULL, 0);

  printf("Depth sensor UART initialized\n");

  // Send sensor commands
#ifdef USE_NONLINEAR
  const char *unit_cmd = "AT+UNIT=0\r";
  uart_write_bytes(UART_PORT_NUM, unit_cmd, strlen(unit_cmd));
#elif defined(USE_LINEAR)
  char unit_cmd[20];
  snprintf(unit_cmd, sizeof(unit_cmd), "AT+UNIT=%d\r", UNIT_VALUE);
  uart_write_bytes(UART_PORT_NUM, unit_cmd, strlen(unit_cmd));
#endif

  char disp_cmd[] = "AT+DISP=7\r";
  uart_write_bytes(UART_PORT_NUM, disp_cmd, strlen(disp_cmd));

  char fps_cmd[] = "AT+FPS=7\r";
  uart_write_bytes(UART_PORT_NUM, fps_cmd, strlen(fps_cmd));

  char binning_cmd[20];
  snprintf(binning_cmd, sizeof(binning_cmd), "AT+BINN=%d\r", BINNING_FACTOR);
  uart_write_bytes(UART_PORT_NUM, binning_cmd, strlen(binning_cmd));

  printf("Sensor initialized\n");
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
  int maxIndex = tempBuffer - 2;

  // Validate tempBuffer size
  if (tempBuffer < HEADER_SIZE + 2 || tempBuffer > BUFFER_SIZE)
  {
    printf("Warning: Invalid buffer size %d\n", tempBuffer);
    return;
  }

  for (int i = HEADER_SIZE; i < maxIndex; i++)
  {
    int row = pixelIndex / imageCols;
    int col = pixelIndex % imageCols;

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
      printf("%.2f\t", depthMap[i][j]);
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

  switch (packetState)
  {
  case 0: // waiting for 0x00
    if (byte == FRAME_START_BYTE_1)
    {
      bufferIndex = 0;
      rxBuffer[bufferIndex++] = byte;
      packetState = 1;
    }
    break;

  case 1: // waiting for 0xFF
    if (byte == FRAME_START_BYTE_2)
    {
      rxBuffer[bufferIndex++] = byte;
      packetState = 2;
    }
    else
    {
      packetState = 0;
    }
    break;

  case 2: // collecting data
    rxBuffer[bufferIndex++] = byte;

    if (byte == FRAME_END_BYTE)
    {
      packetState = 0;
      tempBuffer = bufferIndex;
      bufferIndex = 0;
      return true;
    }
    else if (bufferIndex >= BUFFER_SIZE)
    {
      printf("----- BUFFER OVERFLOW, dropping packet -----\n");
      packetState = 0;
      bufferIndex = 0;
    }
    break;
  }

  return false;
}

// -------------------- Full Processing --------------------
void fullPrint()
{
  if (getPacket())
  {
    readHeader();
    processDepth();
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