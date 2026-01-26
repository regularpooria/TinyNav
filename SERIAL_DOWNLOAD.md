# Serial Log Downloader

This feature allows you to browse and download log files from the ESP32 SD card via serial connection using an interactive TUI (Text User Interface).

## Python Script: `download_log.py`

### Usage

```bash
python download_log.py
```

The script will launch an interactive file browser where you can:
- Navigate with arrow keys (↑/↓)
- Press ENTER to download a file
- Press 'r' to refresh the file list
- Press 'q' to quit

### Configuration

Edit the following variables at the top of `download_log.py`:

- `PORT`: Serial port (default: `/dev/ttyACM0`)
- `BAUD`: Baud rate (default: `115200`)
- `DOWNLOAD_DIR`: Where to save downloaded logs (default: `./logs`)

### Features

- **Interactive TUI**: Browse files with arrow keys
- **File information**: Shows file sizes in human-readable format
- **Live progress**: Real-time download progress indicator
- **Auto-timestamping**: Downloaded files include timestamp to prevent overwrites
- **Error handling**: Clear error messages for troubleshooting

### What It Does

1. Connects to ESP32 via serial
2. Sends `LIST_FILES` command to get all files on SD card
3. Displays an interactive menu with file names and sizes
4. On selection, sends `DOWNLOAD_FILE:<filename>` to download
5. Saves file locally with timestamp: `logs/<filename>_YYYYMMDD_HHMMSS.csv`

## ESP32 Firmware Changes

### New Files

- `main/serial_commands.h` - Command handler header
- `main/serial_commands.cc` - Command handler implementation

### Modified Files

- `main/main_functions.cc` - Added serial command processing to main loop
- `main/CMakeLists.txt` - Added serial_commands.cc to build
- `main/drive_system/depth_sensor.h` - Exposed g_depth_log_filename
- `main/drive_system/depth_sensor.cc` - Made g_depth_log_filename non-static

### Supported Commands

Commands are sent as ASCII text over serial (newline-terminated):

1. **GET_LOG_FILENAME**
   - Response: `LOG_FILENAME:<filename>`
   - Example: `LOG_FILENAME:/revised_log_0024.csv`
   - Gets the current active log file

2. **LIST_FILES**
   - Responses:
     - `FILE_LIST_START` - Beginning of file list
     - `FILE:<name>:<size>` - Regular file with size in bytes
     - `DIR:<name>` - Directory entry
     - `FILE_LIST_END` - End of file list
     - `FILE_LIST_ERROR:<message>` - Error message
   - Lists all files and directories in /sdcard

3. **DOWNLOAD_FILE:<path>**
   - Responses:
     - `FILE_SIZE:<bytes>` - File size in bytes
     - `FILE_START` - Beginning of file data
     - `<file contents>` - Raw file data
     - `FILE_END` - End of file data
     - `FILE_ERROR:<message>` - Error message
   - Downloads a specific file from SD card

## Testing

1. Build and flash the firmware:
   ```bash
   idf.py build flash
   ```

2. Run the download script:
   ```bash
   python download_log.py
   ```

3. Navigate the file browser with arrow keys
4. Press ENTER on a file to download it
5. Check the `logs/` directory for downloaded files

## Troubleshooting

- **No response**: Check that the serial port is correct and ESP32 is powered
- **Empty file list**: Verify SD card is inserted and has files
- **Timeout**: Increase `TIMEOUT` value in `download_log.py`
- **Baud rate mismatch**: Ensure Python script and ESP32 both use 115200
- **TUI display issues**: Make sure your terminal supports curses (most Linux/Mac terminals do)

## Requirements

```bash
pip install pyserial
```

No additional libraries needed - uses Python's built-in `curses` module for the TUI.

## Screenshots

```
================================================================================
           ESP32 SD Card File Browser
================================================================================
↑/↓: Navigate | ENTER: Download | q: Quit | r: Refresh
--------------------------------------------------------------------------------
  [FILE] counter.txt                                      4 B
> [FILE] revised_log_0023.csv                          45.2 KB
  [FILE] revised_log_0024.csv                          67.8 KB
  [DIR]  logs
--------------------------------------------------------------------------------
Found 4 items
```
