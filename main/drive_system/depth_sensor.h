#pragma once

#include <stdint.h>
#include "driver/uart.h"

// -------------------- Configuration --------------------
#define UART_PORT_NUM UART_NUM_2
#define UART_BAUD_RATE 115200
#define UART_RX_PIN 21
#define UART_TX_PIN 20

#define BUFFER_SIZE 12000
#define HEADER_SIZE 20
#define MAX_IMAGE_SIZE 100

#define BINNING_FACTOR 4

// #define USE_NONLINEAR
#define USE_LINEAR
#define UNIT_VALUE 10

// -------------------- Data Structures --------------------
typedef struct __attribute__((packed))
{
  uint16_t frame_begin_flag;
  uint16_t frame_data_len;
  uint8_t reserved1;
  uint8_t output_mode;
  uint8_t sensor_temp;
  uint8_t driver_temp;
  uint8_t exposure_time[4];
  uint8_t error_code;
  uint8_t reserved2;
  uint8_t resolution_rows;
  uint8_t resolution_cols;
  uint16_t frame_id;
  uint8_t isp_version;
  uint8_t reserved3;
} FrameHeader;

// -------------------- Globals --------------------
extern int imageRows;
extern int imageCols;

extern float depthMap[MAX_IMAGE_SIZE][MAX_IMAGE_SIZE];
extern uint8_t rxBuffer[BUFFER_SIZE];

// -------------------- API --------------------
// void depth_sensor_task(void *pvParameters);
void depth_sensor_task();
void depth_sensor_init();

bool getPacket();
void readHeader();
void processDepth();
void printDepth();
float toMillimeters(uint8_t pixelValue);
void fullPrint();
