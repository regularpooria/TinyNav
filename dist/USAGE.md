# TinyNav Executables - Usage Guide

Standalone executables for TinyNav Python utilities with full command-line support.

## Quick Reference

### visualize - Create animations from CSV logs
```bash
# Basic usage
./visualize my_log.csv

# Custom output and settings
./visualize my_log.csv -o animation.gif -f 30 --cmap plasma

# High quality output (slower)
./visualize my_log.csv --dpi 150 --preset slow
```

### download_log - Download files from ESP32
```bash
# Default (Linux)
./download_log

# Custom serial port (Linux)
./download_log -p /dev/ttyUSB0 -b 115200

# Windows
download_log.exe -p COM3 -b 921600

# Custom download directory
./download_log -d ./my_logs
```

### read_serial - Real-time depth visualization
```bash
# Default
./read_serial

# Custom port and colormap
./read_serial -p /dev/ttyUSB0 --cmap plasma

# Windows
read_serial.exe -p COM4

# Different grid size
./read_serial -r 30 -c 30
```

## Full Command-Line Options

### visualize
```
positional arguments:
  csv_file              Input CSV file path (default: revised_log_0053.csv)

options:
  -h, --help            Show help message and exit
  -o OUTPUT, --output OUTPUT
                        Output GIF file path (default: depth_animation.gif)
  -f FPS, --fps FPS     Frames per second (default: 15)
  --dpi DPI             Output DPI (higher = better quality but slower) (default: 80)
  --vmin VMIN           Minimum depth value for colormap (default: 100)
  --vmax VMAX           Maximum depth value for colormap (default: 2000)
  --cmap CMAP           Matplotlib colormap name (default: viridis)
```

**Note:** Uses Pillow writer (no ffmpeg required). Encoding may be slower than ffmpeg but works out-of-the-box.

**Colormap examples:** viridis, plasma, inferno, magma, jet, hot, cool, gray

### download_log
```
options:
  -h, --help            Show help message and exit
  -p PORT, --port PORT  Serial port (e.g., /dev/ttyACM0, COM3) (default: /dev/ttyACM0)
  -b BAUD, --baud BAUD  Baud rate (default: 921600)
  -d DIR, --dir DIR     Download directory (default: ./logs)
  -t TIMEOUT, --timeout TIMEOUT
                        Serial communication timeout in seconds (default: 5.0)
```

**Interactive controls:**
- ↑/↓ - Navigate files
- ENTER - Download selected file
- r - Refresh file list
- q - Quit

### read_serial
```
options:
  -h, --help            Show help message and exit
  -p PORT, --port PORT  Serial port (e.g., /dev/ttyACM0, COM3) (default: /dev/ttyACM0)
  -b BAUD, --baud BAUD  Baud rate (default: 921600)
  -r ROWS, --rows ROWS  Number of rows in depth grid (default: 25)
  -c COLS, --cols COLS  Number of columns in depth grid (default: 25)
  --cmap CMAP           Matplotlib colormap name (default: inferno)
  --timeout TIMEOUT     Serial read timeout in seconds (default: 1.0)
```

**Controls:**
- Ctrl+C - Stop visualization and exit

## Common Serial Ports

### Linux
- `/dev/ttyACM0` - Most USB serial devices
- `/dev/ttyUSB0` - USB-to-serial adapters
- `/dev/ttyACM1`, `/dev/ttyUSB1` - Additional devices

**Find your port:**
```bash
ls /dev/tty* | grep -E "(ACM|USB)"
```

### Windows
- `COM3`, `COM4`, `COM5` - Common ports

**Find your port:**
- Device Manager → Ports (COM & LPT)
- Or use: `mode` command

## Examples

### Create high-quality animation
```bash
./visualize depth_log.csv \
  -o high_quality.gif \
  -f 60 \
  --dpi 150 \
  --cmap plasma \
  --vmin 0 --vmax 3000
```

Note: Encoding with Pillow may be slower. Output files are typically larger than ffmpeg-encoded files.

### Download logs with custom settings
```bash
# Linux
./download_log -p /dev/ttyUSB0 -b 115200 -d ~/esp32_logs

# Windows
download_log.exe -p COM4 -b 115200 -d C:\logs
```

### Monitor different grid sizes
```bash
./read_serial -p /dev/ttyACM0 -r 32 -c 32 --cmap hot
```

## Troubleshooting

### Permission Denied (Linux)
```bash
# Add user to dialout group (logout required)
sudo usermod -a -G dialout $USER

# Or use sudo (temporary)
sudo ./download_log
```

### Port Not Found
```bash
# List available ports (Linux)
ls /dev/tty*

# Check if device is connected
dmesg | tail
```

### Executable Won't Run
```bash
# Make executable (Linux)
chmod +x visualize download_log read_serial

# Run with full path
/path/to/dist/visualize --help
```

### CSV File Not Found (visualize)
```bash
# Specify full path
./visualize /full/path/to/file.csv

# Or change to directory first
cd /path/to/logs
/path/to/visualize my_log.csv
```

## Build Information

- Platform: Linux x86_64
- Python: 3.12.3
- PyInstaller: 6.18.0
- Sizes: visualize (51 MB), download_log (7.6 MB), read_serial (41 MB)

## Notes

- All executables are standalone - no Python installation required
- For Windows executables, build on Windows using `build_executables.py`
- Executables include all dependencies (numpy, matplotlib, pandas, etc.)
- First run may be slower as the executable unpacks
