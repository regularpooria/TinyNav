#!/usr/bin/env python3
"""
Test script to debug serial communication with ESP32
"""

import serial
import sys
import time

PORT = "/dev/ttyACM0"
BAUD = 921600

def main():
    print(f"Connecting to {PORT} at {BAUD} baud...")
    
    try:
        ser = serial.Serial(PORT, BAUD, timeout=2)
        time.sleep(0.5)
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        print("Connected! Sending LIST_FILES command...")
        ser.write(b"LIST_FILES\n")
        time.sleep(0.1)
        
        print("\nWaiting for response (10 seconds)...\n")
        start = time.time()
        while time.time() - start < 10:
            if ser.in_waiting > 0:
                line = ser.readline().decode('ascii', errors='ignore').rstrip()
                if line:
                    print(f"RX: {repr(line)}")
        
        print("\n\nSending GET_LOG_FILENAME command...")
        ser.write(b"GET_LOG_FILENAME\n")
        time.sleep(0.1)
        
        print("\nWaiting for response (5 seconds)...\n")
        start = time.time()
        while time.time() - start < 5:
            if ser.in_waiting > 0:
                line = ser.readline().decode('ascii', errors='ignore').rstrip()
                if line:
                    print(f"RX: {repr(line)}")
        
        ser.close()
        print("\n\nDone!")
        
    except serial.SerialException as e:
        print(f"Serial error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
