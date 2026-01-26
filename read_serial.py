import serial
import numpy as np
import matplotlib.pyplot as plt

PORT = "/dev/ttyACM0"
BAUD = 921600
ROWS = 25
COLS = 25

DEPTH_CHARS = " .:-=+*#%@"
CHAR_TO_VALUE = {c: i for i, c in enumerate(DEPTH_CHARS)}

ser = serial.Serial(PORT, BAUD, timeout=1)
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

    img = plt.imshow(frame, cmap="inferno", vmin=0, vmax=len(DEPTH_CHARS) - 1)
    plt.colorbar(label="Relative depth")
    plt.axis("off")

    while True:
        frame = read_frame()
        img.set_data(frame)
        plt.pause(0.001)


if __name__ == "__main__":
    main()
