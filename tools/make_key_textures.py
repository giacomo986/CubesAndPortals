#!/usr/bin/env python3
# Pictures for the seven keys and the laboratory (the game's ending, TODO.md
# "The game as a whole"): an icon per key (assets/textures/keys_<world>.png),
# the plaza's pedestal and dark frame, and the laboratory's blocks
# (games/lab/textures/lab_*.png), the Inferno heart's braziers and gate, and
# the backrooms' exit sign and stand.
# 32x32, fixed seeds.
#
#   python3 tools/make_key_textures.py
import os
import random

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
ASSETS = os.path.join(ROOT, 'assets', 'textures')
LAB = os.path.join(ROOT, 'games', 'lab', 'textures')
os.makedirs(LAB, exist_ok=True)

# The key of each world, its colour and the colour of its bit.
KEYS = {
    'pista': ((214, 40, 40), (245, 245, 245)),
    'inferno': ((230, 90, 20), (90, 20, 20)),
    'arena': ((120, 130, 140), (200, 40, 30)),
    'arpg': ((150, 120, 60), (60, 160, 220)),
    'chambers': ((235, 235, 235), (40, 130, 255)),
    'backrooms': ((215, 195, 90), (90, 80, 40)),
    'parkour': ((245, 140, 20), (30, 30, 30)),
}


def key_icon(body, bit):
    im = Image.new('RGBA', (32, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    dark = tuple(int(c * 0.45) for c in body)
    # The bow: a ring top left.
    d.ellipse((3, 3, 15, 15), fill=dark)
    d.ellipse((5, 5, 13, 13), fill=body)
    d.ellipse((7, 7, 11, 11), fill=(0, 0, 0, 0))
    # The shank, diagonal to the bottom right, and its bit.
    for i in range(14):
        x, y = 12 + i, 12 + i
        d.rectangle((x - 1, y - 1, x + 1, y + 1), fill=body)
    d.line((12, 13, 26, 27), fill=dark)
    d.polygon([(22, 23), (25, 20), (28, 23), (25, 26)], fill=bit)
    d.polygon([(18, 19), (21, 16), (23, 18), (20, 21)], fill=bit)
    # The gem of the world in the bow's middle.
    d.ellipse((8, 8, 10, 10), fill=bit)
    return im


def noise_tile(base, spread, seed, size=32):
    rnd = random.Random(seed)
    im = Image.new('RGB', (size, size))
    px = im.load()
    for y in range(size):
        for x in range(size):
            k = rnd.randint(-spread, spread)
            px[x, y] = tuple(max(0, min(255, c + k)) for c in base)
    return im


def panel(base, seed, lines=True):
    im = noise_tile(base, 6, seed)
    d = ImageDraw.Draw(im)
    edge = tuple(max(0, c - 30) for c in base)
    d.rectangle((0, 0, 31, 31), outline=edge)
    if lines:
        d.line((0, 15, 31, 15), fill=edge)
        for x, y in ((3, 3), (28, 3), (3, 28), (28, 28), (3, 12), (28, 12), (3, 19), (28, 19)):
            d.point((x, y), fill=tuple(min(255, c + 40) for c in base))
    return im


for world, (body, bit) in KEYS.items():
    key_icon(body, bit).save(os.path.join(ASSETS, f'keys_{world}.png'))

# The pedestal: dark stone with a keyhole; the dead frame of the eighth portal.
ped = panel((70, 72, 78), 11, lines=False)
d = ImageDraw.Draw(ped)
d.ellipse((12, 8, 19, 15), fill=(15, 15, 18))
d.polygon([(14, 14), (17, 14), (19, 24), (12, 24)], fill=(15, 15, 18))
ped.save(os.path.join(ASSETS, 'keys_pedestal.png'))
panel((38, 40, 46), 12).save(os.path.join(ASSETS, 'keys_frame.png'))

# The laboratory.
panel((150, 155, 160), 21).save(os.path.join(LAB, 'lab_wall.png'))
panel((60, 64, 70), 22).save(os.path.join(LAB, 'lab_floor.png'))
panel((32, 34, 38), 23).save(os.path.join(LAB, 'lab_steel.png'))
lamp = noise_tile((235, 240, 255), 4, 24)
ImageDraw.Draw(lamp).rectangle((0, 0, 31, 31), outline=(160, 170, 190))
lamp.save(os.path.join(LAB, 'lab_lamp.png'))
breach = noise_tile((40, 10, 60), 30, 25)
dd = ImageDraw.Draw(breach)
rnd = random.Random(26)
for _ in range(10):
    x, y = rnd.randint(0, 31), rnd.randint(0, 31)
    dd.line((x, y, x + rnd.randint(-8, 8), y + rnd.randint(-8, 8)), fill=(200, 80, 255))
breach.save(os.path.join(LAB, 'lab_breach.png'))
hz = Image.new('RGB', (32, 32))
hd = ImageDraw.Draw(hz)
for i in range(-32, 64, 8):
    hd.polygon([(i, 0), (i + 4, 0), (i + 4 + 32, 32), (i + 32, 32)], fill=(220, 180, 20))
hz.save(os.path.join(LAB, 'lab_hazard.png'))
# The heart of the fortress (games/inferno/mods/inferno_heart): the braziers,
# dark and lit, and the gate.
INFERNO = os.path.join(ROOT, 'games', 'inferno', 'textures')
for name, fire in (('brazier', None), ('brazier_lit', (255, 150, 40))):
    im = panel((52, 36, 34), 31, lines=False)
    dd = ImageDraw.Draw(im)
    dd.rectangle((4, 4, 27, 10), fill=(30, 20, 20))
    if fire:
        rnd = random.Random(32)
        for _ in range(40):
            x, y = rnd.randint(5, 26), rnd.randint(4, 10)
            dd.point((x, y), fill=fire if rnd.random() < 0.6 else (255, 230, 120))
    for x in range(2, 30, 6):
        dd.line((x, 12, x, 29), fill=(30, 20, 20))
    im.save(os.path.join(INFERNO, f'inferno_heart_{name}.png'))
gate = panel((26, 18, 36), 33)
gd = ImageDraw.Draw(gate)
for x in range(3, 32, 7):
    gd.line((x, 0, x, 31), fill=(120, 40, 160))
gate.save(os.path.join(INFERNO, 'inferno_heart_gate.png'))

# The way out of the backrooms (games/backrooms/mods/backrooms_exit): the
# green sign and the stand the key lies on.
BACK = os.path.join(ROOT, 'games', 'backrooms', 'textures')
sign = Image.new('RGB', (32, 32), (30, 150, 60))
sd = ImageDraw.Draw(sign)
sd.rectangle((0, 0, 31, 31), outline=(220, 255, 220))
sd.polygon([(6, 16), (14, 9), (14, 13), (25, 13), (25, 19), (14, 19), (14, 23)], fill=(235, 255, 235))
sign.save(os.path.join(BACK, 'backrooms_exit_sign.png'))
panel((70, 70, 74), 41, lines=False).save(os.path.join(BACK, 'backrooms_exit_stand.png'))

# The laboratory's last fight (games/lab/mods/lab): white panels a portal
# takes, glass, the shield's generators, the valves open and shut, the seal
# of the way out, the lava, a dead lamp.
panel((226, 228, 230), 51).save(os.path.join(LAB, 'lab_panel.png'))
glass = Image.new('RGBA', (32, 32), (190, 220, 235, 70))
ImageDraw.Draw(glass).rectangle((0, 0, 31, 31), outline=(220, 240, 250, 200))
glass.save(os.path.join(LAB, 'lab_glass.png'))
gen = noise_tile((40, 60, 80), 8, 52)
gdr = ImageDraw.Draw(gen)
gdr.ellipse((6, 6, 25, 25), fill=(120, 210, 255))
gdr.ellipse((11, 11, 20, 20), fill=(230, 250, 255))
gen.save(os.path.join(LAB, 'lab_generator.png'))
for name, col in (('valve_open', (230, 120, 40)), ('valve_shut', (70, 90, 70))):
    v = panel((60, 62, 66), 53, lines=False)
    vd = ImageDraw.Draw(v)
    vd.ellipse((5, 5, 26, 26), outline=col, width=3)
    vd.line((16, 6, 16, 26), fill=col, width=3)
    vd.line((6, 16, 26, 16), fill=col, width=3)
    v.save(os.path.join(LAB, f'lab_{name}.png'))
seal = noise_tile((50, 30, 70), 20, 54)
sd2 = ImageDraw.Draw(seal)
for i in range(0, 32, 6):
    sd2.line((i, 0, 31 - i, 31), fill=(150, 80, 220))
seal.save(os.path.join(LAB, 'lab_seal.png'))
lava = noise_tile((230, 100, 20), 30, 55)
ImageDraw.Draw(lava).line((0, 12, 31, 18), fill=(255, 200, 60), width=2)
lava.save(os.path.join(LAB, 'lab_lava.png'))
off = noise_tile((40, 42, 46), 4, 56)
ImageDraw.Draw(off).rectangle((0, 0, 31, 31), outline=(70, 72, 78))
off.save(os.path.join(LAB, 'lab_lamp_off.png'))

print('keys and lab textures written')
