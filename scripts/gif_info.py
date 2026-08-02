#!/usr/bin/env python3
"""
Quick GIF preview — resize and show stats without generating header.
Usage: python3 scripts/gif_info.py cat.gif
"""
import sys, struct, os

def info(path):
    with open(path, 'rb') as f:
        d = f.read()
    w = struct.unpack('<H', d[6:8])[0]
    h = struct.unpack('<H', d[8:10])[0]
    pos = 13
    if d[10] & 0x80: pos += 3 * (2 << (d[10] & 7))
    frames = 0
    while pos < len(d):
        if d[pos] == 0x2C: frames += 1
        pos += 1
        if pos > 0 and d[pos-1] == 0x3B: break
    kb = len(d) / 1024
    header_kb = len(d) * 5.3 / 1024
    print(f'{os.path.basename(path)}: {w}x{h}, {frames}f, {kb:.0f}KB → header {header_kb:.0f}KB')
    if header_kb > 3500:
        print('  ⚠️  Too big! Reduce fps/colors/size.')
    else:
        print('  ✅ Will fit in flash.')

if __name__ == '__main__':
    for f in sys.argv[1:]:
        info(f)
