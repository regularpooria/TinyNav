#include "serial_commands.h"
#include "sdcard/sd.h"
#include "drive_system/depth_sensor.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#define COMMAND_BUFFER_SIZE 256
#define FILE_CHUNK_SIZE 512

static char command_buffer[COMMAND_BUFFER_SIZE];
static int buffer_pos = 0;

extern char g_depth_log_filename[64];

void serial_commands_init()
{
    buffer_pos = 0;
    memset(command_buffer, 0, sizeof(command_buffer));
    
    // Make stdin non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

static void handle_get_log_filename()
{
  printf("LOG_FILENAME:%s\n", g_depth_log_filename);
  fflush(stdout);
}

static void handle_list_files()
{
  printf("FILE_LIST_START\n");
  fflush(stdout);

  const char *mount_point = "/sdcard";
  DIR *dir = opendir(mount_point);

  if (!dir)
  {
    printf("FILE_LIST_ERROR:Cannot open directory %s (errno=%d)\n", mount_point, errno);
    fflush(stdout);
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL)
  {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    // Build full path
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", mount_point, entry->d_name);

    // Get file stats
    struct stat st;
    if (stat(full_path, &st) == 0)
    {
      if (S_ISREG(st.st_mode))
      {
        // Regular file
        printf("FILE:%s:%ld\n", entry->d_name, (long)st.st_size);
      }
      else if (S_ISDIR(st.st_mode))
      {
        // Directory
        printf("DIR:%s\n", entry->d_name);
      }
    }
    fflush(stdout);
  }

  closedir(dir);
  printf("FILE_LIST_END\n");
  fflush(stdout);
}

static void handle_download_file(const char *filename)
{
  bool exists = false;
  esp_err_t err = sd_card_file_exists(filename, &exists);

  if (err != ESP_OK || !exists)
  {
    printf("FILE_ERROR:File not found\n");
    fflush(stdout);
    return;
  }

  // Get file size
  size_t file_size = 0;
  err = sd_card_get_file_size(filename, &file_size);

  if (err != ESP_OK)
  {
    printf("FILE_ERROR:Cannot get file size\n");
    fflush(stdout);
    return;
  }

  printf("FILE_SIZE:%zu\n", file_size);
  printf("FILE_START\n");
  fflush(stdout);

  // Open file for reading
  FILE *fp = sd_card_fopen(filename, "r");
  if (!fp)
  {
    printf("FILE_ERROR:Cannot open file\n");
    fflush(stdout);
    return;
  }

  // Read and send file in chunks
  char chunk[FILE_CHUNK_SIZE];
  size_t bytes_read;

  while ((bytes_read = fread(chunk, 1, sizeof(chunk), fp)) > 0)
  {
    fwrite(chunk, 1, bytes_read, stdout);
    fflush(stdout);
  }

  fclose(fp);

  printf("\nFILE_END\n");
  fflush(stdout);
}

static void process_command(const char *cmd)
{
  if (strncmp(cmd, "GET_LOG_FILENAME", 16) == 0)
  {
    handle_get_log_filename();
  }
  else if (strncmp(cmd, "LIST_FILES", 10) == 0)
  {
    handle_list_files();
  }
  else if (strncmp(cmd, "DOWNLOAD_FILE:", 14) == 0)
  {
    const char *filename = cmd + 14;
    handle_download_file(filename);
  }
}

void serial_commands_process()
{
    char c;
    ssize_t len = read(STDIN_FILENO, &c, 1);
    
    if (len > 0)
    {
        if (c == '\n' || c == '\r')
        {
            if (buffer_pos > 0)
            {
                command_buffer[buffer_pos] = '\0';
                process_command(command_buffer);
                buffer_pos = 0;
            }
        }
        else if (c >= 32 && c <= 126)  // Printable characters only
        {
            if (buffer_pos < COMMAND_BUFFER_SIZE - 1)
            {
                command_buffer[buffer_pos++] = c;
            }
        }
    }
}
