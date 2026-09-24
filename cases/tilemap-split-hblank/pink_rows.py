#!/usr/bin/env python3
"""List the framebuffer rows that contain pink in jnext screenshots.

    python3 cases/tilemap-split-hblank/pink_rows.py build/frames/*.png

The test program's screen must be solid black, so every row printed is a
defect. Screenshots are 640x512, i.e. the 320x256 framebuffer at 2x.
Needs Pillow.
"""
import re
import sys

from PIL import Image


def frame_number(path):
    m = re.search(r"(\d+)\.png$", path)
    return int(m.group(1)) if m else 0


bad = 0
paths = sorted(sys.argv[1:], key=frame_number)
for path in paths:
    im = Image.open(path).convert("RGB")
    px = im.load()
    rows = sorted({y // 2 for y in range(im.height) for x in range(0, im.width, 8)
                   if px[x, y][0] > 150 and px[x, y][1] < 120})
    print(f"{path}: {rows if rows else 'clean'}")
    bad += bool(rows)
print(f"{bad} of {len(paths)} frames have pink rows")
sys.exit(1 if bad else 0)
