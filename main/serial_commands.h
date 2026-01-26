#ifndef SERIAL_COMMANDS_H
#define SERIAL_COMMANDS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize serial command handler
 */
void serial_commands_init();

/**
 * @brief Process incoming serial commands (call from main loop)
 */
void serial_commands_process();

#ifdef __cplusplus
}
#endif

#endif // SERIAL_COMMANDS_H
