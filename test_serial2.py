#!/usr/bin/env python3
"""Serial reader for ESP32-C3 that handles USB re-enumeration after reset."""
import serial
import serial.tools.list_ports
import time
import sys

PORT = "/dev/cu.usbmodem3101"
BAUD = 115200

def wait_for_port(port, timeout=5.0):
    """Wait for a serial port to appear."""
    start = time.time()
    while time.time() - start < timeout:
        for p in serial.tools.list_ports.comports():
            if p.device == port:
                return True
        time.sleep(0.2)
    return False

def main():
    # Connect to ESP32
    ser = serial.Serial(PORT, BAUD, timeout=0.5)
    ser.reset_input_buffer()
    
    print(f"Connected to {PORT} at {BAUD} baud")
    print("Resetting ESP32 via DTR...")
    
    # Toggle DTR to reset
    ser.dtr = False
    time.sleep(0.3)
    ser.dtr = True
    ser.close()
    
    # Wait for USB to re-enumerate
    print("Waiting for port to reappear after reset...")
    time.sleep(0.5)
    if wait_for_port(PORT, timeout=5.0):
        print("Port reappeared!")
    else:
        print("WARNING: Port did not reappear!")
    
    # Reconnect
    time.sleep(1.0)
    ser = serial.Serial(PORT, BAUD, timeout=0.5)
    ser.reset_input_buffer()
    
    print("\n--- ESP32 Output ---\n")
    
    # Read all output for 15 seconds
    start = time.time()
    try:
        while time.time() - start < 15:
            n = ser.in_waiting
            if n > 0:
                data = ser.read(n)
                text = data.decode('utf-8', errors='replace')
                print(text, end='', flush=True)
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        print("\n\nDone.")

if __name__ == "__main__":
    main()
