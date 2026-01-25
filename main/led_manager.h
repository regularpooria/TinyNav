#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <WS2812FX.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Priority levels (higher number = higher priority)
typedef enum {
  LED_PRIORITY_LOW = 0,      // Idle animations
  LED_PRIORITY_NORMAL = 1,   // Drive system status
  LED_PRIORITY_HIGH = 2,     // Depth sensor events
  LED_PRIORITY_CRITICAL = 3  // Errors/warnings
} led_priority_t;

// LED command structure
typedef struct {
  led_priority_t priority;
  uint8_t mode;        // FX_MODE_*
  uint32_t color;      // RGB color
  uint16_t speed;      // Animation speed (optional)
  uint32_t duration_ms; // How long to display (0 = until overridden)
} led_command_t;

// Initialize the LED manager
void led_manager_init(WS2812FX *fx_ptr);

// Submit a command to the queue
bool led_manager_set(led_priority_t priority, uint8_t mode, uint32_t color, uint16_t speed, uint32_t duration_ms);

// Process queued commands (call this periodically, e.g., in main loop)
void led_manager_update();

// Clear all pending commands and reset to idle
void led_manager_clear();

#endif // LED_MANAGER_H
