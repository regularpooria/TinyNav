import serial
import numpy as np
import matplotlib.pyplot as plt
import argparse
import sys

# --- Parse command-line arguments ---
parser = argparse.ArgumentParser(
    description='Real-time visualization of ESP32 depth data via serial',
    formatter_class=argparse.ArgumentDefaultsHelpFormatter
)
parser.add_argument('-p', '--port', default='/dev/ttyACM0',
                    help='Serial port (e.g., /dev/ttyACM0, COM3)')
parser.add_argument('-b', '--baud', type=int, default=921600,
                    help='Baud rate')
parser.add_argument('-r', '--rows', type=int, default=25,
                    help='Number of rows in depth grid')
parser.add_argument('-c', '--cols', type=int, default=25,
                    help='Number of columns in depth grid')
parser.add_argument('--cmap', default='inferno',
                    help='Matplotlib colormap name')
parser.add_argument('--timeout', type=float, default=1.0,
                    help='Serial read timeout in seconds')

args = parser.parse_args()

PORT = args.port
BAUD = args.baud
ROWS = args.rows
COLS = args.cols

DEPTH_CHARS = " .:-=+*#%@"
CHAR_TO_VALUE = {c: i for i, c in enumerate(DEPTH_CHARS)}

try:
    ser = serial.Serial(PORT, BAUD, timeout=args.timeout)
    print(f"Connected to {PORT} at {BAUD} baud")
except serial.SerialException as e:
    print(f"ERROR: Could not open serial port {PORT}: {e}")
    sys.exit(1)

def read_frame():
    rows = []

    while True:
        line = ser.readline().decode("ascii", errors="ignore").rstrip("\r\n")

        if not line:
            continue

        if line == "FRAME_END":
            if len(rows) == ROWS:
                frame = np.zeros((ROWS, COLS), dtype=np.float32)
                for r in range(ROWS):
                    for c, ch in enumerate(rows[r]):
                        frame[r, c] = CHAR_TO_VALUE.get(ch, 0)
                return frame
            else:
                rows.clear()
                continue

        if len(line) == COLS:
            rows.append(line)
            if len(rows) > ROWS:
                rows.pop(0)


def main():
    plt.ion()
    frame = np.zeros((ROWS, COLS))

    img = plt.imshow(frame, cmap=args.cmap, vmin=0, vmax=len(DEPTH_CHARS) - 1)
    plt.colorbar(label="Relative depth")
    plt.title(f"Depth Visualization - {PORT} @ {BAUD} baud")
    plt.axis("off")

    print("Receiving data... (Press Ctrl+C to stop)")
    try:
        while True:
            frame = read_frame()
            img.set_data(frame)
            plt.pause(0.001)
    except KeyboardInterrupt:
        print("\nStopped by user")
        ser.close()
        sys.exit(0)


if __name__ == "__main__":
    main()
