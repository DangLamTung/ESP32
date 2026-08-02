#!/usr/bin/env python3
"""Interactive serial test for ESP32-C3 - sends commands and reads responses."""
import serial
import time

PORT = "/dev/cu.usbmodem3101"
BAUD = 115200

def main():
    ser = serial.Serial(PORT, BAUD, timeout=0.5)
    ser.reset_input_buffer()
    
    # Read any pending output
    print("Reading initial output...\n")
    time.sleep(0.5)
    if ser.in_waiting > 0:
        print(ser.read(ser.in_waiting).decode('utf-8', errors='replace'))
    
    # Send 'i' command for system info
    print("\n--- Sending 'i' (info) command ---")
    ser.write(b'i\r\n')
    time.sleep(1.0)
    if ser.in_waiting > 0:
        print(ser.read(ser.in_waiting).decode('utf-8', errors='replace'))
    
    # Wait for next blink message
    print("\n--- Waiting for next periodic message ---")
    start = time.time()
    try:
        while time.time() - start < 12:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                print(data.decode('utf-8', errors='replace'), end='', flush=True)
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        print("\nDone.")

if __name__ == "__main__":
    main()
