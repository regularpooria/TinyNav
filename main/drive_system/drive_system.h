#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WS2812FX.h>

void drive_system_setup();
void drive_system_loop(WS2812FX *fx);
void drive_system_motors_setup();
void set_motor_a_speed(int speed);
void set_motor_b_speed(int speed);
void setup_voltage_sensor();
float read_voltage_mv();