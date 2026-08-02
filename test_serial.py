#!/usr/bin/env python3
"""Quick serial reader for ESP32 test verification."""
import serial
import time

PORT = "/dev/cu.usbmodem3101"
BAUD = 115200

def main():
    # First, try connecting without reset - ESP32 might already be running
    ser = serial.Serial(PORT, BAUD, timeout=0.5)
    ser.reset_input_buffer()
    
    print(f"Connected to {PORT} at {BAUD} baud")
    print("Reading any pending output first...\n")
    
    # Read any pending output for a few seconds
    start = time.time()
    try:
        while time.time() - start < 5:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                print(data.decode('utf-8', errors='replace'), end='', flush=True)
            time.sleep(0.05)
        
        # Now toggle DTR to reset
        print("\n--- Resetting ESP32 via DTR ---\n")
        ser.dtr = False
        time.sleep(0.2)
        ser.dtr = True
        time.sleep(2.0)
        
        # Re-create serial connection (USB may have re-enumerated)
        ser.close()
        time.sleep(1.0)
        ser = serial.Serial(PORT, BAUD, timeout=0.5)
        ser.reset_input_buffer()
        
        start = time.time()
        while time.time() - start < 15:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                print(data.decode('utf-8', errors='replace'), end='', flush=True)
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        print("\n\nDone.")

if __name__ == "__main__":
    main()
