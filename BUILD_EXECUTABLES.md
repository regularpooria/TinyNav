# Building Executables

This document explains how to build standalone executables for the Python scripts in this project.

## Quick Start

### Linux
```bash
python3 build_executables.py
```

### Windows
```cmd
python build_executables.py
```

## What Gets Built

The script builds three executables with command-line argument support:

1. **visualize** - Creates GIF animations from CSV log files
2. **download_log** - TUI for downloading files from ESP32 via serial
3. **read_serial** - Real-time visualization of serial data from ESP32

## Output

Executables are created in the `dist/` directory:
- Linux: `visualize`, `download_log`, `read_serial`
- Windows: `visualize.exe`, `download_log.exe`, `read_serial.exe`

## Command-Line Usage

All executables support `--help` to see available options.

### visualize
```bash
./dist/visualize [CSV_FILE] [OPTIONS]

# Examples:
./dist/visualize my_log.csv -o output.gif -f 30
./dist/visualize my_log.csv --cmap plasma --vmin 50 --vmax 3000
./dist/visualize --help
```

Options:
- `CSV_FILE` - Input CSV file (default: revised_log_0053.csv)
- `-o, --output` - Output GIF file path (default: depth_animation.gif)
- `-f, --fps` - Frames per second (default: 15)
- `--dpi` - Output DPI (default: 80)
- `--vmin` - Minimum depth value (default: 100)
- `--vmax` - Maximum depth value (default: 2000)
- `--cmap` - Colormap (default: viridis)

Note: Outputs GIF format (Pillow limitation). For MP4, use ffmpeg with Python script.

### download_log
```bash
./dist/download_log [OPTIONS]

# Examples:
./dist/download_log -p /dev/ttyUSB0 -b 115200
./dist/download_log -d ./my_downloads
./dist/download_log -p COM3 -b 921600
```

Options:
- `-p, --port` - Serial port (default: /dev/ttyACM0)
- `-b, --baud` - Baud rate (default: 921600)
- `-d, --dir` - Download directory (default: ./logs)
- `-t, --timeout` - Timeout in seconds (default: 5.0)

### read_serial
```bash
./dist/read_serial [OPTIONS]

# Examples:
./dist/read_serial -p /dev/ttyUSB0 -b 115200
./dist/read_serial --cmap plasma -r 30 -c 30
./dist/read_serial -p COM4
```

Options:
- `-p, --port` - Serial port (default: /dev/ttyACM0)
- `-b, --baud` - Baud rate (default: 921600)
- `-r, --rows` - Grid rows (default: 25)
- `-c, --cols` - Grid columns (default: 25)
- `--cmap` - Colormap (default: inferno)
- `--timeout` - Timeout in seconds (default: 1.0)

## Requirements

- Python 3.8+
- All packages from requirements.txt
- PyInstaller (automatically installed)
- **Windows only**: windows-curses (for download_log.exe)

Note: The `visualize` executable uses Pillow writer and does NOT require ffmpeg.

## Manual Build (Advanced)

If you want to customize the build, you can use PyInstaller directly:

```bash
# Example: Build visualize
pyinstaller --onefile --console \
  --hidden-import numpy \
  --hidden-import pandas \
  --hidden-import matplotlib \
  --name visualize \
  visualize.py

# Example: Build download_log  
pyinstaller --onefile --console \
  --hidden-import serial \
  --hidden-import curses \
  --name download_log \
  download_log.py

# Example: Build read_serial
pyinstaller --onefile --console \
  --hidden-import serial \
  --hidden-import numpy \
  --hidden-import matplotlib \
  --name read_serial \
  read_serial.py
```

## Cross-Compilation Notes

PyInstaller creates executables for the platform it runs on:
- Building on Linux → Linux executables
- Building on Windows → Windows executables

**You cannot cross-compile.** To get both Linux and Windows executables:
1. Build on Linux machine (or WSL) for Linux executables
2. Build on Windows machine for Windows executables

## File Sizes

Executables are larger than the source scripts because they bundle:
- Python interpreter
- All dependencies (numpy, matplotlib, pandas, etc.)
- Required libraries

Typical sizes:
- visualize: ~50 MB
- download_log: ~7-8 MB  
- read_serial: ~41 MB

## Troubleshooting

### ImportError: No module named 'X'

Add the missing module to hidden_imports in build_executables.py:
```python
hidden_imports=['module_name']
```

### Windows: "curses" not found

Install windows-curses:
```cmd
pip install windows-curses
```

### visualize: "ffmpeg is not installed"

The visualize executable requires ffmpeg to be installed separately:
```bash
# Linux
sudo apt install ffmpeg

# macOS
brew install ffmpeg

# Windows
# Download from ffmpeg.org and add to PATH
```

### Executable won't run

1. Check file permissions (Linux):
   ```bash
   chmod +x dist/visualize
   ```

2. Run from terminal to see error messages:
   ```bash
   ./dist/visualize --help
   ```

3. Try rebuilding with `--clean` flag

### Large file size

This is normal for PyInstaller. To reduce size:
- Use `--exclude-module` for unused packages
- Use UPX compression (advanced)
- Consider using conda or virtual environments with minimal packages

## Distribution

The executables in `dist/` are standalone and can be:
- Copied to other machines (same OS)
- Distributed to users
- Run without Python installed

**Important**: The executable must match the target OS and architecture.
