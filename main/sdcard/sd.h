#ifndef SD_CARD_H
#define SD_CARD_H

#include "esp_err.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief SD card mount configuration structure
   */
  typedef struct
  {
    const char *mount_point;
    bool format_if_mount_failed;
    int max_files;
    size_t allocation_unit_size;
    uint8_t bus_width;    // 1 or 4
    bool high_speed_mode; // true for 40MHz, false for 20MHz
  } sd_card_config_t;

  /**
   * @brief SD card handle structure (internal use only)
   */
  typedef struct
  {
    sdmmc_card_t *card;
    const char *mount_point;
    bool is_mounted;
    void *pwr_ctrl_handle; // Power control handle for LDO
  } sd_card_handle_t;

  /**
   * @brief Initialize and mount SD card
   *
   * @param config SD card configuration
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_init(const sd_card_config_t *config);

  /**
   * @brief Unmount and deinitialize SD card
   *
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_deinit(void);

  /**
   * @brief Write data to file on SD card
   *
   * @param path File path relative to mount point (e.g., "/hello.txt")
   * @param data Data to write
   * @param len Length of data (0 for null-terminated string)
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_write_file(const char *path, const char *data, size_t len);

  /**
   * @brief Append data to file on SD card
   *
   * @param path File path relative to mount point
   * @param data Data to append
   * @param len Length of data (0 for null-terminated string)
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_append_file(const char *path, const char *data, size_t len);

  /**
   * @brief Read data from file on SD card
   *
   * @param path File path relative to mount point
   * @param buffer Buffer to store read data
   * @param buffer_size Size of buffer
   * @param bytes_read Pointer to store number of bytes read (can be NULL)
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_read_file(const char *path, char *buffer, size_t buffer_size, size_t *bytes_read);

  /**
   * @brief Delete file from SD card
   *
   * @param path File path relative to mount point
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_delete_file(const char *path);

  /**
   * @brief Rename file on SD card
   *
   * @param old_path Old file path
   * @param new_path New file path
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_rename_file(const char *old_path, const char *new_path);

  /**
   * @brief Check if file exists on SD card
   *
   * @param path File path relative to mount point
   * @param exists Pointer to store result
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_file_exists(const char *path, bool *exists);

  /**
   * @brief Get file size
   *
   * @param path File path relative to mount point
   * @param size Pointer to store file size
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_get_file_size(const char *path, size_t *size);

  /**
   * @brief Print SD card information
   *
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_print_info(void);

  /**
   * @brief Format SD card
   *
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_format(void);

  /**
   * @brief Get default SD card configuration
   *
   * @return sd_card_config_t Default configuration
   */
  sd_card_config_t sd_card_get_default_config(void);

  /**
   * @brief Test SD card pin connections and diagnose hardware issues
   *
   * This function performs diagnostic tests on the SD card pins to check:
   * - Pin recovery time (indicates pull-up strength)
   * - Pin voltage levels
   * - Cross-talk between pins
   *
   * Run this if you're experiencing initialization timeouts.
   *
   * @return esp_err_t ESP_OK on success
   */
  esp_err_t sd_card_test_pins(void);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_H