#!/usr/bin/env python3
# The textures of games/backrooms, written rather than drawn: Level 0's
# yellow wallpaper, damp carpet, ceiling tiles and tube lights; the poolrooms'
# white and blue tiles, water, slide and ladder; Level 1's concrete and lamps;
# the stairs of each level; dead tubes and lamps; the coping round a pool;
# the doorway a portal stands in.
#
# usage: tools/make_backrooms_textures.py
import os, random

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TEX = os.path.join(ROOT, "games", "backrooms", "textures")
os.makedirs(TEX, exist_ok=True)

N = 32
rng = random.Random(11)


def noisy(base, spread):
    img = Image.new("RGB", (N, N))
    px = img.load()
    for y in range(N):
        for x in range(N):
            d = rng.randint(-spread, spread)
            px[x, y] = tuple(max(0, min(255, c + d)) for c in base)
    return img


def save(img, name):
    img.save(os.path.join(TEX, "backrooms_" + name + ".png"))


def tiles(base, grout, step):
    img = noisy(base, 3)
    d = ImageDraw.Draw(img)
    for k in range(0, N, step):
        d.line([(k, 0), (k, N - 1)], fill=grout)
        d.line([(0, k), (N - 1, k)], fill=grout)
    return img


# Level 0: mono-yellow wallpaper, a faint stripe and a chevron of darker yellow.
img = noisy((196, 178, 96), 4)
d = ImageDraw.Draw(img)
for x in range(0, N, 8):
    d.line([(x, 0), (x, N - 1)], fill=(182, 164, 86))
    for y in range(2, N, 8):
        d.line([(x + 2, y), (x + 4, y + 2), (x + 6, y)], fill=(176, 158, 82))
save(img, "wallpaper")

# Damp carpet: brown-yellow, blotched.
img = noisy((150, 128, 70), 9)
d = ImageDraw.Draw(img)
for _ in range(4):
    x, y, r = rng.randint(0, N), rng.randint(0, N), rng.randint(3, 7)
    d.ellipse([x - r, y - r, x + r, y + r], fill=(132, 112, 60))
save(img, "carpet")

save(tiles((206, 198, 160), (170, 162, 128), 16), "ceiling")

# A tube light: a pale panel, two bright tubes.
img = noisy((236, 234, 210), 2)
d = ImageDraw.Draw(img)
d.rectangle([0, 0, N - 1, N - 1], outline=(170, 168, 150))
for y in (9, 22):
    d.rectangle([3, y - 1, N - 4, y + 1], fill=(255, 255, 246))
save(img, "tube")

# The poolrooms.
save(tiles((232, 236, 238), (196, 204, 208), 8), "tile")
save(tiles((150, 206, 222), (112, 170, 190), 8), "pool_tile")
img = tiles((236, 246, 255), (200, 220, 236), 16)
ImageDraw.Draw(img).ellipse([8, 8, 23, 23], fill=(255, 255, 255))
save(img, "pool_lamp")
img = noisy((90, 170, 210), 6)
d = ImageDraw.Draw(img)
for y in range(3, N, 7):
    d.line([(0, y), (8, y + 1), (16, y), (24, y + 1), (31, y)], fill=(150, 210, 236))
save(img, "water")
img = noisy((70, 160, 220), 4)
d = ImageDraw.Draw(img)
for x in (0, N - 1):
    d.line([(x, 0), (x, N - 1)], fill=(230, 240, 248))
save(img, "slide")
img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
for x in (4, 27):
    d.rectangle([x, 0, x + 1, N - 1], fill=(190, 196, 200, 255))
for y in range(3, N, 8):
    d.rectangle([4, y, 28, y + 1], fill=(200, 206, 210, 255))
save(img, "ladder")

# Level 1: concrete, a darker floor slab, a warm lamp, a puddle.
img = noisy((128, 128, 124), 8)
d = ImageDraw.Draw(img)
d.line([(0, 15), (N - 1, 15)], fill=(110, 110, 106))
save(img, "concrete")
img = noisy((96, 96, 94), 7)
for _ in range(3):
    x, y = rng.randint(0, N - 8), rng.randint(0, N - 8)
    ImageDraw.Draw(img).line([(x, y), (x + rng.randint(3, 8), y + rng.randint(1, 6))], fill=(76, 76, 74))
save(img, "slab")
img = noisy((255, 214, 150), 3)
ImageDraw.Draw(img).rectangle([0, 0, N - 1, N - 1], outline=(150, 120, 80), width=3)
save(img, "lamp")
save(noisy((70, 80, 84), 5), "puddle")

# The doorway: dark painted wood; its lintel a lit EXIT-white strip.
img = noisy((44, 40, 36), 4)
d = ImageDraw.Draw(img)
for x in range(0, N, 8):
    d.line([(x, 0), (x, N - 1)], fill=(36, 32, 30))
save(img, "frame")
img = noisy((255, 250, 230), 2)
ImageDraw.Draw(img).rectangle([0, 0, N - 1, N - 1], outline=(200, 190, 160), width=2)
save(img, "glow")

# The stairs, one for each level, dressed as its floor: carpet over the
# treads with a brass nosing on Level 0, tile with a blue edge in the
# poolrooms, bare concrete with a worn yellow line on Level 1. The nosing runs
# along the middle and the top, where the two halves of a step meet.
def stair(base, spread, nosing, name):
    img = noisy(base, spread)
    d = ImageDraw.Draw(img)
    for y in (0, 1, 16, 17):
        d.line([(0, y), (N - 1, y)], fill=nosing)
    save(img, name)


stair((150, 128, 70), 9, (176, 140, 60), "stair_carpet")
img = tiles((232, 236, 238), (196, 204, 208), 8)
d = ImageDraw.Draw(img)
for y in (0, 1, 16, 17):
    d.line([(0, y), (N - 1, y)], fill=(70, 150, 200))
save(img, "stair_tile")
stair((120, 120, 116), 8, (200, 170, 60), "stair_concrete")

# A dead tube and a dead lamp: the lit ones, grey and dull.
img = noisy((150, 150, 140), 3)
d = ImageDraw.Draw(img)
d.rectangle([0, 0, N - 1, N - 1], outline=(110, 110, 100))
for y in (9, 22):
    d.rectangle([3, y - 1, N - 4, y + 1], fill=(176, 176, 166))
save(img, "tube_off")
img = noisy((110, 100, 86), 3)
ImageDraw.Draw(img).rectangle([0, 0, N - 1, N - 1], outline=(70, 64, 56), width=3)
save(img, "lamp_off")

# The coping round a pool: wide white tiles, a blue line on the water's side.
img = tiles((214, 228, 234), (176, 194, 202), 16)
ImageDraw.Draw(img).line([(0, 30), (N - 1, 30)], fill=(70, 150, 200), width=2)
save(img, "pool_edge")
