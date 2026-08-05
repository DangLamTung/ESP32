import serial, time, sys
port = sys.argv[1] if len(sys.argv) > 1 else 'COM9'
dur = float(sys.argv[2]) if len(sys.argv) > 2 else 45
try:
    s = serial.Serial(port, 115200, timeout=1)
except Exception as e:
    print("open failed:", e); sys.exit(1)
end = time.time() + dur
while time.time() < end:
    n = s.in_waiting
    if n:
        try:
            sys.stdout.write(s.read(n).decode('utf-8', errors='replace'))
            sys.stdout.flush()
        except Exception:
            pass
    else:
        time.sleep(0.05)
s.close()
