#!/usr/bin/env python3
"""
Download log files from ESP32 SD card via serial protocol.

This script provides a TUI (Text User Interface) to browse and download
files from the ESP32's SD card.
"""

import serial
import sys
import os
import time
import curses
from datetime import datetime

# Configuration
PORT = "/dev/ttyACM0"
BAUD = 921600
TIMEOUT = 5
DOWNLOAD_DIR = "./logs"

class SerialFileDownloader:
    def __init__(self, port, baudrate, timeout=5):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(0.5)  # Wait for connection to stabilize
        # Flush any existing data
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
    
    def send_command(self, cmd):
        """Send a command to ESP32"""
        self.ser.write((cmd + "\n").encode('ascii'))
        time.sleep(0.1)
    
    def read_until_prompt(self, timeout=2):
        """Read serial output until a prompt or timeout"""
        start = time.time()
        lines = []
        while time.time() - start < timeout:
            if self.ser.in_waiting > 0:
                line = self.ser.readline().decode('ascii', errors='ignore').rstrip()
                if line:
                    lines.append(line)
        return lines
    
    def get_current_log_filename(self):
        """Query ESP32 for current log filename"""
        self.send_command("GET_LOG_FILENAME")
        
        # Wait for response
        start = time.time()
        while time.time() - start < TIMEOUT:
            if self.ser.in_waiting > 0:
                line = self.ser.readline().decode('ascii', errors='ignore').rstrip()
                if line.startswith("LOG_FILENAME:"):
                    filename = line.split(":", 1)[1].strip()
                    return filename
        
        return None
    
    def list_files(self):
        """List all files on SD card"""
        self.send_command("LIST_FILES")
        
        files = []
        dirs = []
        receiving = False
        
        start = time.time()
        while time.time() - start < TIMEOUT:
            if self.ser.in_waiting > 0:
                line = self.ser.readline().decode('ascii', errors='ignore').rstrip()
                
                if line == "FILE_LIST_START":
                    receiving = True
                    start = time.time()
                elif line == "FILE_LIST_END":
                    break
                elif line.startswith("FILE_LIST_ERROR:"):
                    error_msg = line.split(":", 1)[1] if ":" in line else "Unknown error"
                    sys.stderr.write(f"ERROR: {error_msg}\n")
                    return None
                elif receiving:
                    if line.startswith("FILE:"):
                        parts = line.split(":", 2)
                        if len(parts) == 3:
                            filename = parts[1]
                            size = int(parts[2])
                            files.append({"name": filename, "size": size, "type": "file"})
                    elif line.startswith("DIR:"):
                        dirname = line.split(":", 1)[1]
                        dirs.append({"name": dirname, "size": 0, "type": "dir"})
        
        # Sort: directories first, then files by name
        result = sorted(dirs, key=lambda x: x["name"]) + sorted(files, key=lambda x: x["name"])
        return result
    
    def download_file(self, filename, progress_callback=None):
        """Download file from SD card"""
        self.send_command(f"DOWNLOAD_FILE:{filename}")
        
        # Wait for file transfer to begin
        file_data = []
        receiving = False
        file_size = 0
        bytes_received = 0
        
        start = time.time()
        while time.time() - start < TIMEOUT * 10:  # Extended timeout for large files
            if self.ser.in_waiting > 0:
                line = self.ser.readline().decode('ascii', errors='ignore').rstrip()
                
                if line.startswith("FILE_SIZE:"):
                    file_size = int(line.split(":", 1)[1])
                    receiving = True
                    start = time.time()  # Reset timeout
                
                elif line == "FILE_START":
                    receiving = True
                    start = time.time()
                
                elif line == "FILE_END":
                    receiving = False
                    break
                
                elif line.startswith("FILE_ERROR:"):
                    error = line.split(":", 1)[1]
                    return None, error
                
                elif receiving and line:
                    file_data.append(line)
                    bytes_received += len(line) + 1  # +1 for newline
                    
                    # Progress callback
                    if progress_callback and file_size > 0:
                        progress = (bytes_received / file_size) * 100
                        progress_callback(progress, bytes_received, file_size)
        
        if not file_data:
            return None, "No data received"
        
        return "\n".join(file_data), None
    
    def close(self):
        """Close serial connection"""
        self.ser.close()


def format_size(size):
    """Format file size in human-readable format"""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size < 1024.0:
            return f"{size:.1f} {unit}"
        size /= 1024.0
    return f"{size:.1f} TB"


def draw_menu(stdscr, files, current_row, status_msg=""):
    """Draw the file selection menu"""
    stdscr.clear()
    h, w = stdscr.getmaxyx()
    
    # Title
    title = "ESP32 SD Card File Browser"
    stdscr.addstr(0, (w - len(title)) // 2, title, curses.A_BOLD)
    stdscr.addstr(1, 0, "=" * w)
    
    # Instructions
    instructions = "↑/↓: Navigate | ENTER: Download | q: Quit | r: Refresh"
    stdscr.addstr(2, 0, instructions)
    stdscr.addstr(3, 0, "-" * w)
    
    # File list
    start_row = 4
    visible_rows = h - start_row - 3  # Leave space for status
    
    # Calculate scroll offset
    scroll_offset = max(0, current_row - visible_rows + 1)
    
    for idx in range(scroll_offset, min(len(files), scroll_offset + visible_rows)):
        item = files[idx]
        y = start_row + (idx - scroll_offset)
        
        if idx == current_row:
            stdscr.attron(curses.A_REVERSE)
        
        if item["type"] == "dir":
            line = f"[DIR]  {item['name']}"
        else:
            line = f"[FILE] {item['name']:<40} {format_size(item['size']):>10}"
        
        # Truncate if too long
        if len(line) > w - 2:
            line = line[:w-5] + "..."
        
        try:
            stdscr.addstr(y, 1, line)
        except curses.error:
            pass
        
        if idx == current_row:
            stdscr.attroff(curses.A_REVERSE)
    
    # Status bar
    status_y = h - 2
    stdscr.addstr(status_y, 0, "-" * w)
    if status_msg:
        # Truncate status if too long
        if len(status_msg) > w - 2:
            status_msg = status_msg[:w-5] + "..."
        try:
            stdscr.addstr(status_y + 1, 1, status_msg)
        except curses.error:
            pass
    
    stdscr.refresh()


def tui_main(stdscr, downloader):
    """Main TUI loop"""
    curses.curs_set(0)  # Hide cursor
    stdscr.keypad(True)
    
    current_row = 0
    status_msg = "Loading files..."
    
    # Initial file list load
    files = downloader.list_files()
    
    if files is None:
        return "ERROR: Could not list files from ESP32"
    
    if not files:
        return "No files found on SD card"
    
    status_msg = f"Found {len(files)} items"
    
    while True:
        draw_menu(stdscr, files, current_row, status_msg)
        
        key = stdscr.getch()
        
        if key == curses.KEY_UP and current_row > 0:
            current_row -= 1
            status_msg = ""
        elif key == curses.KEY_DOWN and current_row < len(files) - 1:
            current_row += 1
            status_msg = ""
        elif key == ord('q') or key == ord('Q'):
            return None
        elif key == ord('r') or key == ord('R'):
            status_msg = "Refreshing..."
            draw_menu(stdscr, files, current_row, status_msg)
            files = downloader.list_files()
            if files is None:
                status_msg = "ERROR: Could not refresh file list"
            elif not files:
                status_msg = "No files found"
            else:
                status_msg = f"Refreshed: {len(files)} items found"
            current_row = min(current_row, len(files) - 1) if files else 0
        elif key == ord('\n') or key == curses.KEY_ENTER or key == 10 or key == 13:
            selected = files[current_row]
            
            if selected["type"] == "dir":
                status_msg = "Cannot download directories"
            else:
                filename = "/" + selected["name"]
                status_msg = f"Downloading {selected['name']}..."
                draw_menu(stdscr, files, current_row, status_msg)
                
                # Download progress callback
                def progress_cb(percent, current, total):
                    msg = f"Downloading: {percent:.1f}% ({format_size(current)} / {format_size(total)})"
                    draw_menu(stdscr, files, current_row, msg)
                
                file_content, error = downloader.download_file(filename, progress_cb)
                
                if error:
                    status_msg = f"ERROR: {error}"
                elif file_content:
                    # Save file
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    local_filename = f"{DOWNLOAD_DIR}/{selected['name'].replace('.csv', '')}_{timestamp}.csv"
                    
                    try:
                        with open(local_filename, 'w') as f:
                            f.write(file_content)
                        status_msg = f"✓ Saved to {local_filename} ({format_size(len(file_content))})"
                    except IOError as e:
                        status_msg = f"ERROR: Could not save file: {e}"
                else:
                    status_msg = "ERROR: Download failed"
    
    return None


def main():
    # Create download directory if it doesn't exist
    os.makedirs(DOWNLOAD_DIR, exist_ok=True)
    
    try:
        print(f"Connecting to {PORT} at {BAUD} baud...")
        downloader = SerialFileDownloader(PORT, BAUD, TIMEOUT)
        print("Connected! Starting file browser...")
        time.sleep(0.5)
        
        # Launch TUI
        result = curses.wrapper(tui_main, downloader)
        
        downloader.close()
        
        if result:
            print(result)
        else:
            print("Exited normally")
        
    except serial.SerialException as e:
        print(f"Serial error: {e}")
        print(f"Make sure {PORT} is the correct port and the device is connected")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(0)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
