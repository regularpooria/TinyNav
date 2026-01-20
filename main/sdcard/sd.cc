#include "sd.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"

#if SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

static const char *TAG = "sd_card";

#define MAX_PATH_LEN 256

// Global SD card handle
static sd_card_handle_t *g_sd_handle = NULL;

/**
 * @brief Build full path from mount point and relative path
 */
static esp_err_t build_full_path(const char *path, char *full_path, size_t max_len)
{
  if (!path || !full_path)
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (!g_sd_handle || !g_sd_handle->is_mounted)
  {
    ESP_LOGE(TAG, "SD card not mounted");
    return ESP_ERR_INVALID_STATE;
  }

  // Remove leading slash if present
  const char *rel_path = path;
  if (path[0] == '/')
  {
    rel_path = path + 1;
  }

  int written = snprintf(full_path, max_len, "%s/%s", g_sd_handle->mount_point, rel_path);
  if (written < 0 || written >= (int)max_len)
  {
    ESP_LOGE(TAG, "Path too long");
    return ESP_ERR_INVALID_SIZE;
  }

  return ESP_OK;
}

sd_card_config_t sd_card_get_default_config(void)
{
  sd_card_config_t config = {
      .mount_point = "/sdcard",
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024,
      .bus_width = 4};
  return config;
}

esp_err_t sd_card_init(const sd_card_config_t *config)
{
  if (!config)
  {
    return ESP_ERR_INVALID_ARG;
  }

  // Check if already initialized
  if (g_sd_handle != NULL)
  {
    ESP_LOGW(TAG, "SD card already initialized");
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t ret;

  // Allocate handle
  g_sd_handle = (sd_card_handle_t *)malloc(sizeof(sd_card_handle_t));
  if (!g_sd_handle)
  {
    ESP_LOGE(TAG, "Failed to allocate handle");
    return ESP_ERR_NO_MEM;
  }

  g_sd_handle->mount_point = config->mount_point;
  g_sd_handle->is_mounted = false;
  g_sd_handle->card = NULL;

  // Configure mount options
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = config->format_if_mount_failed,
      .max_files = config->max_files,
      .allocation_unit_size = config->allocation_unit_size,
      .disk_status_check_enable = false,
      .use_one_fat = false};

  ESP_LOGI(TAG, "Initializing SD card");
  ESP_LOGI(TAG, "Using SDMMC peripheral");

  // Initialize SDMMC host
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();

#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
  sd_pwr_ctrl_ldo_config_t ldo_config = {
      .ldo_chan_id = CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_IO_ID,
  };
  sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

  ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to create LDO power control driver");
    free(g_sd_handle);
    g_sd_handle = NULL;
    return ret;
  }
  host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

  // Configure slot
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = config->bus_width;

#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
#ifdef CONFIG_EXAMPLE_PIN_CLK
  slot_config.clk = CONFIG_EXAMPLE_PIN_CLK;
#endif
#ifdef CONFIG_EXAMPLE_PIN_CMD
  slot_config.cmd = CONFIG_EXAMPLE_PIN_CMD;
#endif
#ifdef CONFIG_EXAMPLE_PIN_D0
  slot_config.d0 = CONFIG_EXAMPLE_PIN_D0;
#endif
#if defined(CONFIG_EXAMPLE_PIN_D1) && CONFIG_BUS_WIDTH == 4
  slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;
  slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;
  slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;
#endif
#endif

  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  ESP_LOGI(TAG, "Mounting filesystem");
  ret = esp_vfs_fat_sdmmc_mount(config->mount_point, &host, &slot_config,
                                &mount_config, &g_sd_handle->card);

  if (ret != ESP_OK)
  {
    if (ret == ESP_FAIL)
    {
      ESP_LOGE(TAG, "Failed to mount filesystem");
    }
    else
    {
      ESP_LOGE(TAG, "Failed to initialize card (%s)", esp_err_to_name(ret));
    }
    free(g_sd_handle);
    g_sd_handle = NULL;
    return ret;
  }

  g_sd_handle->is_mounted = true;
  ESP_LOGI(TAG, "Filesystem mounted");

  // Print card info
  sdmmc_card_print_info(stdout, g_sd_handle->card);

  return ESP_OK;
}

esp_err_t sd_card_deinit(void)
{
  if (!g_sd_handle)
  {
    return ESP_ERR_INVALID_STATE;
  }

  if (g_sd_handle->is_mounted)
  {
    esp_vfs_fat_sdcard_unmount(g_sd_handle->mount_point, g_sd_handle->card);
    ESP_LOGI(TAG, "Card unmounted");
    g_sd_handle->is_mounted = false;
  }

  free(g_sd_handle);
  g_sd_handle = NULL;
  return ESP_OK;
}

esp_err_t sd_card_write_file(const char *path, const char *data, size_t len)
{
  if (!path || !data)
  {
    return ESP_ERR_INVALID_ARG;
  }

  char full_path[MAX_PATH_LEN];
  esp_err_t ret = build_full_path(path, full_path, sizeof(full_path));
  if (ret != ESP_OK)
  {
    return ret;
  }

  ESP_LOGI(TAG, "Writing file %s", full_path);
  FILE *f = fopen(full_path, "w");
  if (!f)
  {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return ESP_FAIL;
  }

  if (len == 0)
  {
    fprintf(f, "%s", data);
  }
  else
  {
    fwrite(data, 1, len, f);
  }

  fclose(f);
  ESP_LOGI(TAG, "File written");

  return ESP_OK;
}

esp_err_t sd_card_append_file(const char *path, const char *data, size_t len)
{
  if (!path || !data)
  {
    return ESP_ERR_INVALID_ARG;
  }

  char full_path[MAX_PATH_LEN];
  esp_err_t ret = build_full_path(path, full_path, sizeof(full_path));
  if (ret != ESP_OK)
  {
    return ret;
  }

  ESP_LOGI(TAG, "Appending to file %s", full_path);
  FILE *f = fopen(full_path, "a");
  if (!f)
  {
    ESP_LOGE(TAG, "Failed to open file for appending");
    return ESP_FAIL;
  }

  if (len == 0)
  {
    fprintf(f, "%s", data);
  }
  else
  {
    fwrite(data, 1, len, f);
  }

  fclose(f);
  ESP_LOGI(TAG, "Data appended");

  return ESP_OK;
}

esp_err_t sd_card_read_file(const char *path, char *buffer, size_t buffer_size, size_t *bytes_read)
{
  if (!path || !buffer)
  {
    return ESP_ERR_INVALID_ARG;
  }

  char full_path[MAX_PATH_LEN];
  esp_err_t ret = build_full_path(path, full_path, sizeof(full_path));
  if (ret != ESP_OK)
  {
    return ret;
  }

  ESP_LOGI(TAG, "Reading file %s", full_path);
  FILE *f = fopen(full_path, "r");
  if (!f)
  {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return ESP_FAIL;
  }

  size_t read = fread(buffer, 1, buffer_size - 1, f);
  buffer[read] = '\0'; // Null terminate

  if (bytes_read)
  {
    *bytes_read = read;
  }

  fclose(f);
  ESP_LOGI(TAG, "Read %zu bytes from file", read);

  return ESP_OK;
}

esp_err_t sd_card_delete_file(const char *path)
{
  if (!path)
  {
    return ESP_ERR_INVALID_ARG;
  }

  char full_path[MAX_PATH_LEN];
  esp_err_t ret = build_full_path(path, full_path, sizeof(full_path));
  if (ret != ESP_OK)
  {
    return ret;
  }

  ESP_LOGI(TAG, "Deleting file %s", full_path);
  if (unlink(full_path) != 0)
  {
    ESP_LOGE(TAG, "Failed to delete file");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "File deleted");
  return ESP_OK;
}

esp_err_t sd_card_rename_file(const char *old_path, const char *new_path)
{
  if (!old_path || !new_path)
  {
    return ESP_ERR_INVALID_ARG;
  }

  char old_full[MAX_PATH_LEN];
  char new_full[MAX_PATH_LEN];

  esp_err_t ret = build_full_path(old_path, old_full, sizeof(old_full));
  if (ret != ESP_OK)
  {
    return ret;
  }

  ret = build_full_path(new_path, new_full, sizeof(new_full));
  if (ret != ESP_OK)
  {
    return ret;
  }

  ESP_LOGI(TAG, "Renaming %s to %s", old_full, new_full);
  if (rename(old_full, new_full) != 0)
  {
    ESP_LOGE(TAG, "Failed to rename file");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "File renamed");
  return ESP_OK;
}

esp_err_t sd_card_file_exists(const char *path, bool *exists)
{
  if (!path || !exists)
  {
    return ESP_ERR_INVALID_ARG;
  }

  char full_path[MAX_PATH_LEN];
  esp_err_t ret = build_full_path(path, full_path, sizeof(full_path));
  if (ret != ESP_OK)
  {
    return ret;
  }

  struct stat st;
  *exists = (stat(full_path, &st) == 0);

  return ESP_OK;
}

esp_err_t sd_card_get_file_size(const char *path, size_t *size)
{
  if (!path || !size)
  {
    return ESP_ERR_INVALID_ARG;
  }

  char full_path[MAX_PATH_LEN];
  esp_err_t ret = build_full_path(path, full_path, sizeof(full_path));
  if (ret != ESP_OK)
  {
    return ret;
  }

  struct stat st;
  if (stat(full_path, &st) != 0)
  {
    ESP_LOGE(TAG, "Failed to get file stats");
    return ESP_FAIL;
  }

  *size = st.st_size;
  return ESP_OK;
}

esp_err_t sd_card_print_info(void)
{
  if (!g_sd_handle || !g_sd_handle->is_mounted)
  {
    return ESP_ERR_INVALID_STATE;
  }

  sdmmc_card_print_info(stdout, g_sd_handle->card);
  return ESP_OK;
}

esp_err_t sd_card_format(void)
{
  if (!g_sd_handle || !g_sd_handle->is_mounted)
  {
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Formatting SD card");
  esp_err_t ret = esp_vfs_fat_sdcard_format(g_sd_handle->mount_point, g_sd_handle->card);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to format SD card (%s)", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "SD card formatted");
  return ESP_OK;
}