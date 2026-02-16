#include "led_manager.h"
#include <stdio.h>

static WS2812FX *g_fx = nullptr;
static QueueHandle_t g_led_queue = nullptr;
static led_command_t g_current_cmd = {LED_PRIORITY_LOW, 0, 0, 0, 0};
static int64_t g_current_cmd_start_time = 0;

#define LED_QUEUE_SIZE 10

void led_manager_init(WS2812FX *fx_ptr)
{
  g_fx = fx_ptr;
  g_led_queue = xQueueCreate(LED_QUEUE_SIZE, sizeof(led_command_t));
  
  if (g_led_queue == nullptr)
  {
    printf("ERROR: Failed to create LED queue\n");
  }
  else
  {
    printf("LED Manager initialized with queue size %d\n", LED_QUEUE_SIZE);
  }
  
  // Initialize with idle state
  g_current_cmd.priority = LED_PRIORITY_LOW;
  g_current_cmd_start_time = 0;
}

bool led_manager_set(led_priority_t priority, uint8_t mode, uint32_t color, uint16_t speed, uint32_t duration_ms)
{
  if (g_led_queue == nullptr)
  {
    printf("ERROR: LED queue not initialized\n");
    return false;
  }
  
  // Don't queue commands with lower priority than current active command
  if (priority < g_current_cmd.priority)
  {
    return false;
  }
  
  led_command_t cmd;
  cmd.priority = priority;
  cmd.mode = mode;
  cmd.color = color;
  cmd.speed = speed;
  cmd.duration_ms = duration_ms;
  
  // Try to add to queue (non-blocking)
  if (xQueueSend(g_led_queue, &cmd, 0) != pdTRUE)
  {
    // Queue full, try to replace if higher priority
    led_command_t peek_cmd;
    if (xQueuePeek(g_led_queue, &peek_cmd, 0) == pdTRUE)
    {
      if (cmd.priority > peek_cmd.priority)
      {
        // Remove lowest priority item and add new one
        xQueueReceive(g_led_queue, &peek_cmd, 0);
        xQueueSend(g_led_queue, &cmd, 0);
        return true;
      }
    }
    return false; // Queue full and no room for this priority
  }
  
  return true;
}

void led_manager_update()
{
  if (g_fx == nullptr || g_led_queue == nullptr)
  {
    return;
  }
  
  int64_t now = esp_timer_get_time() / 1000; // Convert to ms
  
  // Check if current command has expired
  if (g_current_cmd.duration_ms > 0)
  {
    int64_t elapsed = now - g_current_cmd_start_time;
    if (elapsed >= g_current_cmd.duration_ms)
    {
      // Current command expired, clear it
      g_current_cmd.priority = LED_PRIORITY_LOW;
      g_current_cmd.duration_ms = 0;
    }
  }
  
  // Check if there's a higher priority command in queue
  led_command_t next_cmd;
  led_command_t highest_priority_cmd;
  bool found_higher = false;
  int queue_count = uxQueueMessagesWaiting(g_led_queue);
  
  // Scan queue for highest priority command
  for (int i = 0; i < queue_count; i++)
  {
    if (xQueueReceive(g_led_queue, &next_cmd, 0) == pdTRUE)
    {
      // Check if this is higher priority than current
      if (next_cmd.priority > g_current_cmd.priority)
      {
        if (!found_higher || next_cmd.priority > highest_priority_cmd.priority)
        {
          if (found_higher)
          {
            // Put previous highest back in queue
            xQueueSend(g_led_queue, &highest_priority_cmd, 0);
          }
          highest_priority_cmd = next_cmd;
          found_higher = true;
        }
        else
        {
          // Put it back
          xQueueSend(g_led_queue, &next_cmd, 0);
        }
      }
      else if (next_cmd.priority == g_current_cmd.priority)
      {
        // Same priority, replace current
        if (!found_higher)
        {
          highest_priority_cmd = next_cmd;
          found_higher = true;
        }
        else
        {
          xQueueSend(g_led_queue, &next_cmd, 0);
        }
      }
      else
      {
        // Lower priority, put back
        xQueueSend(g_led_queue, &next_cmd, 0);
      }
    }
  }
  
  // Apply highest priority command if found
  if (found_higher)
  {
    g_current_cmd = highest_priority_cmd;
    g_current_cmd_start_time = now;
    
    // Apply to LED strip
    g_fx->setMode(g_current_cmd.mode);
    g_fx->setColor(g_current_cmd.color);
    if (g_current_cmd.speed > 0)
    {
      g_fx->setSpeed(g_current_cmd.speed);
    }
  }
}

void led_manager_clear()
{
  if (g_led_queue == nullptr)
  {
    return;
  }
  
  // Empty the queue
  led_command_t dummy;
  while (xQueueReceive(g_led_queue, &dummy, 0) == pdTRUE)
  {
    // Just drain the queue
  }
  
  // Reset current command to idle
  g_current_cmd.priority = LED_PRIORITY_LOW;
  g_current_cmd.duration_ms = 0;
  
  if (g_fx != nullptr)
  {
    g_fx->setMode(FX_MODE_STATIC);
    g_fx->setColor(YELLOW);
  }
}
