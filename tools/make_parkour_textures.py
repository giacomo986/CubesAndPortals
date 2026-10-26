#!/usr/bin/env python3
# The textures of games/parkour, written rather than drawn: grey concrete for
# the floors, red brick for the walls that are run along and kicked off, a
# steel beam for the rope to hold, yellow and black hazard edging, an orange
# pad where a checkpoint is, a lamp, and the doorway's frame.
#
# usage: tools/make_parkour_textures.py
import os, random

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TEX = os.path.join(ROOT, "games", "parkour", "textures")
os.makedirs(TEX, exist_ok=True)

N = 32
rng = random.Random(7)


def noisy(base, spread):
    img = Image.new("RGB", (N, N))
    px = img.load()
    for y in range(N):
        for x in range(N):
            d = rng.randint(-spread, spread)
            px[x, y] = tuple(max(0, min(255, c + d)) for c in base)
    return img


def save(img, name):
    img.save(os.path.join(TEX, "parkour_" + name + ".png"))


# Concrete: grey, speckled, a form joint across the middle.
img = noisy((150, 152, 150), 9)
d = ImageDraw.Draw(img)
d.line([(0, 15), (N - 1, 15)], fill=(124, 126, 124))
for _ in range(10):
    x, y = rng.randrange(N), rng.randrange(N)
    d.point((x, y), fill=(110, 112, 110))
save(img, "concrete")

# Brick: red courses, offset every other row, pale mortar.
img = noisy((214, 206, 196), 4)
d = ImageDraw.Draw(img)
for row in range(4):
    y0 = row * 8
    off = 0 if row % 2 == 0 else 8
    for col in range(-1, 3):
        x0 = col * 16 + off
        r = rng.randint(-14, 14)
        d.rectangle([x0 + 1, y0 + 1, x0 + 14, y0 + 6], fill=(156 + r, 66 + r // 2, 52 + r // 3))
save(img, "brick")

# Steel: dark blue-grey plate, a flange top and bottom, rivets.
img = noisy((70, 78, 88), 4)
d = ImageDraw.Draw(img)
d.rectangle([0, 0, N - 1, 4], fill=(96, 104, 114))
d.rectangle([0, N - 5, N - 1, N - 1], fill=(96, 104, 114))
for x in range(4, N, 8):
    d.ellipse([x - 1, 1, x + 1, 3], fill=(140, 146, 154))
    d.ellipse([x - 1, N - 4, x + 1, N - 2], fill=(140, 146, 154))
save(img, "steel")

# Hazard: yellow and black diagonal stripes.
img = Image.new("RGB", (N, N))
px = img.load()
for y in range(N):
    for x in range(N):
        px[x, y] = (236, 196, 30) if ((x + y) // 6) % 2 == 0 else (30, 30, 32)
save(img, "hazard")

# Pad: a bright orange checkpoint, a white ring on it.
img = noisy((240, 120, 24), 6)
d = ImageDraw.Draw(img)
d.ellipse([6, 6, N - 7, N - 7], outline=(255, 236, 210), width=2)
d.ellipse([13, 13, N - 14, N - 14], fill=(255, 236, 210))
save(img, "pad")

# Lamp: a warm panel in a grey rim.
img = noisy((255, 240, 200), 6)
d = ImageDraw.Draw(img)
d.rectangle([0, 0, N - 1, N - 1], outline=(120, 120, 120), width=3)
save(img, "lamp")

# Frame: black with an orange stripe, for the doorways.
img = noisy((34, 34, 36), 4)
d = ImageDraw.Draw(img)
d.rectangle([0, 12, N - 1, 19], fill=(240, 118, 20))
save(img, "frame")

print("parkour textures written to", TEX)
