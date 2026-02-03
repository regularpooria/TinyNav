# TinyNav Executables

This project includes scripts to build standalone executables for the TinyNav Python utilities. The executables support full command-line arguments for customization.

## Quick Start

### Build Executables

**Linux:**
```bash
python3 build_executables.py
```

**Windows:**
```cmd
python build_executables.py
```

Or use the batch file:
```cmd
build_windows.bat
```

### Using the Executables

All executables support `--help` flag. See `dist/USAGE.md` for full documentation.

```bash
# Visualize CSV logs
./dist/visualize my_log.csv -o animation.mp4 -f 30

# Download files from ESP32
./dist/download_log -p /dev/ttyUSB0 -b 115200

# Real-time serial visualization  
./dist/read_serial -p /dev/ttyACM0 --cmap plasma
```

## What's Included

### Scripts
- **build_executables.py** - Main build script for all platforms
- **build_windows.bat** - Windows batch script wrapper
- **visualize.py** - Source: CSV to GIF animation converter
- **download_log.py** - Source: ESP32 file download TUI
- **read_serial.py** - Source: Real-time depth visualization

### Documentation
- **BUILD_EXECUTABLES.md** - Complete build instructions and troubleshooting
- **dist/USAGE.md** - Full command-line reference for executables
- **dist/README.md** - Distribution package documentation

### Output (after build)
- **dist/visualize** (or .exe) - ~51 MB
- **dist/download_log** (or .exe) - ~7.6 MB
- **dist/read_serial** (or .exe) - ~41 MB

## Features

### Command-Line Arguments

All executables now support customization via command-line:

**visualize:**
- Input CSV file path
- Output GIF file path
- FPS, DPI, colormap
- Depth range (vmin/vmax)

**download_log:**
- Serial port and baud rate
- Download directory
- Timeout settings

**read_serial:**
- Serial port and baud rate
- Grid dimensions (rows/cols)
- Colormap selection
- Timeout settings

### Cross-Platform

The scripts work on both Linux and Windows. Build on each platform to get native executables:

- Linux: `./visualize`, `./download_log`, `./read_serial`
- Windows: `visualize.exe`, `download_log.exe`, `read_serial.exe`

## Requirements

- Python 3.8+
- Dependencies from requirements.txt
- PyInstaller (auto-installed by build script)
- **Windows only:** windows-curses (for download_log)

## Examples

```bash
# Create 60 FPS animation with plasma colormap
./dist/visualize depth.csv -o output.gif -f 60 --cmap plasma

# Download from custom serial port
./dist/download_log -p /dev/ttyUSB0 -b 115200 -d ./logs

# Monitor with different grid size
./dist/read_serial -r 30 -c 30 -p /dev/ttyACM0
```

## Documentation

For complete documentation:
- Build process: `BUILD_EXECUTABLES.md`
- Usage guide: `dist/USAGE.md`
- Distribution: `dist/README.md`

## Notes

- All executables are standalone - **no Python installation required**
- **No ffmpeg required** - visualize uses Pillow writer (bundled)
- Cannot cross-compile (must build on target platform)
- First run may be slower as executable unpacks
- File sizes are large due to bundled Python + dependencies

## Support

For issues with:
- Building: See troubleshooting in BUILD_EXECUTABLES.md
- Running: See troubleshooting in dist/USAGE.md
- Serial ports: Check device permissions and port names
