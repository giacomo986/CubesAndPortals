#!/usr/bin/env python3
# The application icon (tools/dist/icon.png, 256x256), for the AppImage and
# its .desktop entry: a grey voxel cube with a blue portal on one side and an
# orange one on the other.
#
#   python3 tools/dist/make_icon.py
import math
import os

from PIL import Image, ImageDraw

S = 256
im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(im)
cx, cy, a, h = 128, 30, 96, 96

top = [(cx, cy), (cx + a, cy + a // 2), (cx, cy + a), (cx - a, cy + a // 2)]
left = [(cx - a, cy + a // 2), (cx, cy + a), (cx, cy + a + h), (cx - a, cy + a // 2 + h)]
right = [(cx, cy + a), (cx + a, cy + a // 2), (cx + a, cy + a // 2 + h), (cx, cy + a + h)]
for poly, col in ((top, (205, 205, 200)), (left, (150, 150, 146)), (right, (110, 110, 108))):
    d.polygon(poly, fill=col)
    d.line(poly + [poly[0]], fill=(40, 40, 40), width=4)


def portal(px, py, rx, ry, skew, col):
    # An ellipse sheared onto a side face of the cube.
    pts = [(px + rx * math.cos(t), py + ry * math.sin(t) + skew * rx * math.cos(t))
           for t in (i * 2 * math.pi / 48 for i in range(48))]
    d.polygon(pts, fill=(20, 20, 30))
    d.line(pts + [pts[0]], fill=col, width=9)


mid = cy + a // 2 + h // 2 + a // 4
portal(cx - a // 2, mid, 26, 40, 0.5, (60, 140, 255))
portal(cx + a // 2, mid, 26, 40, -0.5, (255, 140, 30))
im.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'icon.png'))
