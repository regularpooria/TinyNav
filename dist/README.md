# TinyNav Executables - Distribution Package

Standalone executables for TinyNav Python utilities. No Python installation required!

## Contents

- **visualize** - Creates GIF animations from CSV depth log files (~51 MB)
- **download_log** - Interactive TUI for downloading files from ESP32 via serial (~8 MB)
- **read_serial** - Real-time visualization of depth data from ESP32 serial (~42 MB)

## System Requirements

### All Executables
- Linux x86_64 or Windows x86_64 (depending on build)
- No Python installation needed (bundled in executable)
- No external dependencies (ffmpeg not needed)

### download_log and read_serial
- USB serial port access
- ESP32 device connected
- Linux: User must be in `dialout` group

See **REQUIREMENTS.txt** for complete installation instructions.

## Quick Start

All executables support `--help` for usage information:

```bash
# Linux
./visualize --help
./download_log --help
./read_serial --help

# Windows
visualize.exe --help
download_log.exe --help
read_serial.exe --help
```

## Usage Examples

### Create Animation (GIF format)
```bash
./visualize my_log.csv -o output.gif -f 30 --cmap plasma
```

**Note:** Outputs GIF format (Pillow limitation). For MP4 output, use the Python script with ffmpeg installed.

### Download Files from ESP32
```bash
# Linux
./download_log -p /dev/ttyACM0 -b 921600

# Windows
download_log.exe -p COM3 -b 921600
```

### Real-time Visualization
```bash
# Linux
./read_serial -p /dev/ttyACM0 --cmap inferno

# Windows
read_serial.exe -p COM4 --cmap inferno
```

## Documentation

- **USAGE.md** - Complete command-line reference and examples
- **REQUIREMENTS.txt** - System requirements and setup instructions

## Troubleshooting

### "Permission denied" (Linux)
```bash
chmod +x visualize download_log read_serial
```

### "Cannot open /dev/ttyACM0" (Linux)
```bash
# Add user to dialout group (requires logout)
sudo usermod -a -G dialout $USER
```

### Serial port not found
- Check device is connected: `ls /dev/tty*` (Linux) or Device Manager (Windows)
- Use `-p` flag to specify different port

### Want MP4 instead of GIF?
The executable uses Pillow which only supports GIF. For MP4:
- Use the Python script directly: `python3 visualize.py input.csv`
- Requires ffmpeg installed on your system

## Notes

- Executables are standalone and portable
- First run may be slower (unpacking)
- GIF files can be large for long animations
- No external dependencies required
- For MP4 output, use Python script with ffmpeg

## Build Information

- Platform: Linux x86_64
- Python: 3.12.3
- PyInstaller: 6.18.0
- Video Writer: Pillow (GIF format)

For Windows executables, rebuild on Windows using the source scripts.
