#!/usr/bin/env python3
# The textures of games/chambers, written rather than drawn: the test
# chambers' white panels a portal takes and the dark metal it does not, their
# floors and lights, the floor button, the door, the vent, the aerial faith
# plate, the clock and its button, the pedestal. The models are Blender's:
# tools/make_chambers_models.py and tools/make_portal_gun.py.
#
# usage: tools/make_chambers_assets.py
import os, random

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TEX = os.path.join(ROOT, "games", "chambers", "textures")
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
    img.save(os.path.join(TEX, name + ".png"))


# A portal takes these: white panels, four to a block, seams between.
img = noisy((226, 228, 230), 4)
d = ImageDraw.Draw(img)
for k in (0, 15, 16, 31):
    d.line([(k, 0), (k, 31)], fill=(178, 182, 186))
    d.line([(0, k), (31, k)], fill=(178, 182, 186))
save(img, "chambers_core_panel")
for name in ("shell_low", "shell_high", "shell_one"):   # a portal's panels, hollowed
    save(img, "chambers_core_" + name)

# And not these: dark metal plates, riveted.
img = noisy((112, 118, 126), 5)
d = ImageDraw.Draw(img)
d.rectangle([0, 0, 31, 31], outline=(78, 82, 88))
d.line([(0, 16), (31, 16)], fill=(84, 88, 94))
for x, y in ((3, 3), (28, 3), (3, 28), (28, 28), (3, 19), (28, 19)):
    d.point((x, y), fill=(110, 114, 120))
save(img, "chambers_core_metal")

# The floor: dark tiles.
img = noisy((132, 134, 138), 4)
d = ImageDraw.Draw(img)
for k in (0, 31):
    d.line([(k, 0), (k, 31)], fill=(98, 100, 104))
    d.line([(0, k), (31, k)], fill=(98, 100, 104))
save(img, "chambers_core_floor")

# Glass: a pale frame round nothing much.
img = Image.new("RGBA", (N, N), (190, 220, 235, 50))
d = ImageDraw.Draw(img)
d.rectangle([0, 0, 31, 31], outline=(210, 230, 240, 200))
d.line([(5, 26), (12, 19)], fill=(255, 255, 255, 120))
save(img, "chambers_core_glass")

# A light strip.
img = noisy((236, 244, 250), 2)
d = ImageDraw.Draw(img)
d.rectangle([0, 0, 31, 31], outline=(170, 176, 182))
save(img, "chambers_core_light")

# The floor button: a red disc on a grey ring; pressed, darker and lower.
for name, top in (("chambers_core_button", (210, 40, 30)), ("chambers_core_button_down", (120, 30, 24))):
    img = noisy((120, 124, 128), 3)
    d = ImageDraw.Draw(img)
    d.ellipse([3, 3, 28, 28], fill=(70, 72, 76))
    d.ellipse([6, 6, 25, 25], fill=top)
    save(img, name)

# The door: two grey leaves, a stripe down the middle, an orange light.
img = noisy((150, 154, 160), 4)
d = ImageDraw.Draw(img)
d.line([(15, 0), (15, 31)], fill=(60, 62, 66))
d.line([(16, 0), (16, 31)], fill=(60, 62, 66))
d.rectangle([4, 4, 11, 27], outline=(120, 124, 130))
d.rectangle([20, 4, 27, 27], outline=(120, 124, 130))
save(img, "chambers_core_door")

# The vent a cube drops out of: a dark round mouth.
img = noisy((200, 204, 208), 3)
d = ImageDraw.Draw(img)
d.ellipse([3, 3, 28, 28], fill=(30, 32, 36))
for k in range(8, 24, 4):
    d.line([(6, k), (25, k)], fill=(54, 56, 60))
save(img, "chambers_core_vent")

# The launch plate: yellow and black, an arrow on it.
img = noisy((214, 180, 40), 6)
d = ImageDraw.Draw(img)
for k in range(-32, 32, 8):
    d.line([(k, 31), (k + 31, 0)], fill=(40, 40, 40), width=2)
d.rectangle([5, 5, 26, 26], fill=(90, 94, 100))
d.polygon([(16, 7), (24, 17), (19, 17), (19, 24), (13, 24), (13, 17), (8, 17)], fill=(240, 240, 240))
save(img, "chambers_core_plate")

# The clock: a black face, red segments.
img = Image.new("RGB", (N, N), (20, 20, 22))
d = ImageDraw.Draw(img)
d.rectangle([0, 0, 31, 31], outline=(90, 94, 100))
for x0 in (5, 18):
    d.rectangle([x0, 9, x0 + 8, 10], fill=(220, 30, 20))
    d.rectangle([x0, 21, x0 + 8, 22], fill=(220, 30, 20))
    d.rectangle([x0, 9, x0 + 1, 22], fill=(220, 30, 20))
    d.rectangle([x0 + 7, 9, x0 + 8, 22], fill=(220, 30, 20))
save(img, "chambers_core_clock")

# The push button on its stalk: red on white.
img = noisy((224, 226, 228), 3)
d = ImageDraw.Draw(img)
d.ellipse([9, 9, 22, 22], fill=(220, 30, 20))
save(img, "chambers_core_pushbutton")

# The pedestal a gun waits on.
img = noisy((200, 204, 208), 3)
d = ImageDraw.Draw(img)
d.rectangle([2, 2, 29, 29], outline=(120, 124, 130))
d.ellipse([10, 10, 21, 21], fill=(80, 170, 255))
save(img, "chambers_core_pedestal")

# The way out: green, an arrow.
img = noisy((40, 160, 70), 5)
d = ImageDraw.Draw(img)
d.polygon([(6, 16), (16, 6), (16, 12), (26, 12), (26, 20), (16, 20), (16, 26)], fill=(230, 250, 230))
save(img, "chambers_core_exit")

# The fixed portal frames' rims, blue and orange.
for name, rgb in (("chambers_core_rim_blue", (40, 130, 255)), ("chambers_core_rim_orange", (255, 140, 20))):
    img = noisy(rgb, 8)
    save(img, name)

print("chambers: textures in", TEX)
