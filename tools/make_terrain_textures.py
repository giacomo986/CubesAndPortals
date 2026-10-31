#!/usr/bin/env python3
"""Pictures for the blocks the first tests left plain -- the engine's own
(stone, grass, dirt, sand, snow, bedrock, the frame, glass), the oak, water,
and the blocks of mods that never had one -- drawn by rule on the 16 grid and
doubled to 32 px, as tools/make_block_textures.py draws the rest: a colour
per pixel shaken a little, clumps and grains of a second and third, lit on
their top-left pixel, wrapping at the edges so a wall of them tiles.

The pictures they replace are kept in assets/archive/textures-2026-10-30/.
From a fixed seed, so running it again makes the same files.

    python3 tools/make_terrain_textures.py
"""
import colorsys
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "textures")


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c[:3])


def save(img, name):
    img.resize((32, 32), Image.NEAREST).save(os.path.join(OUT, name + ".png"))


def ground(base, rng, lo=0.88, hi=1.08):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            img.putpixel((x, y), shade(base, rng.uniform(lo, hi)))
    return img


def clumps(img, colours, n, rng, sizes=((1, 1), (2, 1), (1, 2), (2, 2))):
    for _ in range(n):
        c = rng.choice(colours)
        x0, y0 = rng.randrange(16), rng.randrange(16)
        w, h = rng.choice(sizes)
        for dy in range(h):
            for dx in range(w):
                k = 1.15 if (dx, dy) == (0, 0) and w * h > 1 else rng.uniform(0.92, 1.05)
                img.putpixel(((x0 + dx) % 16, (y0 + dy) % 16), shade(c, k))
    return img


def cracks(img, colour, n, rng):
    for _ in range(n):
        x, y = rng.randrange(16), rng.randrange(16)
        for _ in range(rng.randint(2, 5)):
            img.putpixel((x % 16, y % 16), shade(colour, rng.uniform(0.9, 1.05)))
            x += rng.choice((1, 1, 0, -1))
            y += rng.choice((1, 0, 0, 1))
    return img


def stone(rng):
    img = ground((128, 128, 128), rng)
    clumps(img, [(110, 110, 112), (146, 146, 146), (118, 116, 114)], 40, rng)
    return cracks(img, (92, 92, 94), 7, rng)


def dirt(rng):
    img = ground((126, 90, 62), rng, 0.86, 1.1)
    return clumps(img, [(100, 70, 48), (150, 110, 78), (92, 84, 80), (140, 100, 66)], 46, rng)


def sand(rng):
    img = ground((216, 200, 146), rng, 0.93, 1.05)
    return clumps(img, [(196, 180, 128), (232, 220, 170), (186, 170, 124)], 50, rng, ((1, 1),))


def snow(rng):
    img = ground((238, 244, 250), rng, 0.95, 1.03)
    return clumps(img, [(222, 232, 244), (250, 252, 255), (210, 222, 238)], 30, rng)


def grass_top(rng):
    img = ground((92, 150, 62), rng, 0.85, 1.1)
    return clumps(img, [(74, 128, 50), (112, 172, 74), (84, 140, 56)], 60, rng, ((1, 1), (1, 2)))


def grass_side(rng):
    img = dirt(rng)
    top = grass_top(rng)
    for x in range(16):
        reach = rng.choice((2, 3, 3, 4, 5))
        for y in range(reach):
            img.putpixel((x, y), top.getpixel((x, y)))
    return img


def bedrock(rng):
    img = ground((58, 58, 62), rng, 0.8, 1.15)
    clumps(img, [(30, 30, 34), (90, 90, 96), (44, 44, 48)], 44, rng, ((2, 2), (3, 2), (2, 3), (1, 1)))
    return img


def planks_frame(base, rng):
    """A frame of planks: boards across, darker joints, a lit rim."""
    img = ground(base, rng, 0.9, 1.06)
    for y in range(16):
        for x in range(16):
            if y % 4 == 3:
                img.putpixel((x, y), shade(base, 0.62))
            elif (x + (y // 4) * 7) % 16 == 0:
                img.putpixel((x, y), shade(base, 0.7))
    for i in range(16):
        img.putpixel((i, 0), shade(base, 1.25))
        img.putpixel((0, i), shade(base, 1.25))
        img.putpixel((i, 15), shade(base, 0.55))
        img.putpixel((15, i), shade(base, 0.55))
    return img


def glass(rng):
    img = Image.new("RGBA", (16, 16), (210, 236, 246, 40))
    for i in range(16):
        for p in ((i, 0), (0, i), (i, 15), (15, i)):
            img.putpixel(p, (236, 248, 252, 220))
    for x, y in ((3, 4), (4, 3), (5, 2), (10, 9), (11, 8), (12, 7), (9, 10)):
        img.putpixel((x, y), (250, 254, 255, 170))
    return img


def bark(base, rng):
    img = ground(base, rng, 0.85, 1.08)
    for x in range(16):
        if x % 4 in (0, 3) or rng.random() < 0.15:
            for y in range(16):
                if rng.random() < 0.85:
                    img.putpixel((x, y), shade(base, rng.uniform(0.6, 0.75)))
    return img


def rings(base, wood, rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            d = max(abs(x - 7.5), abs(y - 7.5))
            c = base if d > 6.5 else wood
            k = 0.8 if int(d) % 2 == 0 and d <= 6.5 else rng.uniform(0.92, 1.06)
            img.putpixel((x, y), shade(c, k))
    return img


def leaves(base, rng):
    img = ground(base, rng, 0.7, 1.1)
    clumps(img, [shade(base, 0.55), shade(base, 1.25), shade(base, 0.8)], 70, rng)
    return img


def water(rng):
    """See-through, as glass is: a light blue a little shaken, at about a
    third, and a few ripples of brighter, less clear blue."""
    img = Image.new("RGBA", (16, 16))
    for y in range(16):
        for x in range(16):
            img.putpixel((x, y), shade((70, 130, 200), rng.uniform(0.94, 1.05)) + (int(rng.uniform(84, 100)),))
    for _ in range(8):
        x, y = rng.randrange(16), rng.randrange(16)
        for dx in range(rng.randint(2, 4)):
            img.putpixel(((x + dx) % 16, y), (170, 210, 240, 150))
    return img


def bricks(base, mortar, moss, rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            row = y // 4
            joint = y % 4 == 3 or (x + (4 if row % 2 else 0)) % 8 == 7
            c = mortar if joint else base
            img.putpixel((x, y), shade(c, rng.uniform(0.86, 1.08)))
    if moss:
        clumps(img, [moss, shade(moss, 1.2), shade(moss, 0.8)], 26, rng, ((1, 1), (2, 1), (2, 2)))
    return img


def lamp(glow, frame, rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            edge = x in (0, 15) or y in (0, 15) or x == 7 or y == 7
            c = frame if edge else glow
            k = rng.uniform(0.9, 1.06) if edge else (1.0 - 0.012 * (abs(x - 7.5) + abs(y - 7.5)))
            img.putpixel((x, y), shade(c, k))
    return img


def plate(base, rng, rivets=True):
    """Metal: a sheet shaken a little, a bevel, a rivet in each corner."""
    img = ground(base, rng, 0.94, 1.05)
    for i in range(16):
        img.putpixel((i, 0), shade(base, 1.3))
        img.putpixel((0, i), shade(base, 1.3))
        img.putpixel((i, 15), shade(base, 0.6))
        img.putpixel((15, i), shade(base, 0.6))
    if rivets:
        for x, y in ((2, 2), (13, 2), (2, 13), (13, 13)):
            img.putpixel((x, y), shade(base, 1.5))
            img.putpixel((x + 1, y + 1), shade(base, 0.55))
    return img


def crystal(base, rng):
    img = ground(base, rng, 0.8, 1.1)
    for _ in range(6):
        x, y = rng.randrange(16), rng.randrange(16)
        for k in range(rng.randint(3, 6)):
            img.putpixel(((x + k) % 16, (y - k) % 16), shade(base, 1.4))
    return img


def carved(base, rng):
    """A block of cut stone with a sunk panel: an altar, a grave."""
    img = ground(base, rng, 0.9, 1.06)
    for y in range(3, 13):
        for x in range(3, 13):
            if x in (3, 12) or y in (3, 12):
                img.putpixel((x, y), shade(base, 0.7))
    for i in range(16):
        img.putpixel((i, 0), shade(base, 1.2))
        img.putpixel((15, i), shade(base, 0.7))
    return img


def hue(i, rng):
    """mods/palette's twenty hues, as dyed clay rather than flat paint."""
    r, g, b = colorsys.hsv_to_rgb(i / 20.0, 0.75, 0.9)
    base = (int(r * 255), int(g * 255), int(b * 255))
    img = ground(base, rng, 0.92, 1.06)
    return clumps(img, [shade(base, 0.86), shade(base, 1.1)], 24, rng)


def main():
    rng = random.Random(30102026)
    save(stone(rng), "core_stone")
    save(grass_top(rng), "core_grass_top")
    save(grass_side(rng), "core_grass")
    save(dirt(rng), "core_grass_bottom")
    save(dirt(rng), "core_dirt")
    save(sand(rng), "core_sand")
    save(snow(rng), "core_snow")
    save(snow(rng), "weather_snow")
    save(bedrock(rng), "core_bedrock")
    save(planks_frame((170, 140, 100), rng), "core_frame")
    save(glass(rng), "core_glass")
    save(bark((104, 78, 50), rng), "trees_trunk")
    save(rings((104, 78, 50), (170, 136, 90), rng), "trees_trunk_top")
    save(leaves((58, 122, 46), rng), "trees_leaves")
    save(water(rng), "lakes_water")
    save(bricks((104, 112, 74), (70, 72, 64), (60, 120, 50), rng), "ruins_mossy_brick")
    save(lamp((250, 214, 130), (90, 70, 50), rng), "ruins_lamp")
    save(carved((120, 80, 160), rng), "deep_altar")
    save(crystal((190, 120, 250), rng), "deep_crystal")
    save(leaves((80, 210, 230), rng), "deep_glowcap")
    save(leaves((60, 170, 120), rng), "deep_glowmoss")
    save(clumps(ground((52, 50, 60), rng), [(40, 38, 48), (70, 68, 80)], 40, rng), "deep_slate")
    save(plate((200, 204, 212), rng), "keys_exit_frame")
    save(lamp((120, 230, 140), (60, 66, 70), rng), "keys_exit_lamp")
    save(plate((38, 40, 46), rng), "keys_frame")
    save(lamp((70, 74, 66), (54, 56, 52), rng), "keys_hint")
    save(lamp((120, 230, 140), (54, 56, 52), rng), "keys_hint_lit")
    save(carved((120, 118, 112), rng), "grave_stone")
    save(plate((70, 120, 230), rng, False), "portal_frame")
    save(plate((90, 190, 110), rng), "creative_box")
    save(lamp((255, 240, 180), (120, 110, 90), rng), "beacons_beacon")
    for i in range(20):
        save(hue(i, rng), f"palette_hue{i}")


if __name__ == "__main__":
    main()
