#!/usr/bin/env python3
"""The faces of the village museum's blocks (mods/villagers, museum.luau), on
the 16 grid doubled to 32 px into assets/textures: the curio that marks the
house -- a little glass case on a wooden stand -- and the plinth of pale
stone a piece of the collection turns on. Seeded.

    python3 tools/make_museum_assets.py
"""
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "textures")


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c)


def save(img, name):
    img.resize((32, 32), Image.NEAREST).save(os.path.join(OUT, name + ".png"))


def main():
    rng = random.Random(1851)
    curio = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            if y >= 10:
                c = (120, 80, 45) if y != 10 else (90, 60, 35)                  # the stand
            else:
                c = (190, 215, 225) if x not in (0, 15) and y != 0 else (150, 120, 60)   # glass, brass frame
            img_c = shade(c, rng.uniform(0.92, 1.06))
            curio.putpixel((x, y), img_c)
    for x, y in ((7, 4), (8, 4), (7, 5), (8, 5), (6, 6), (9, 6), (7, 7), (8, 7)):
        curio.putpixel((x, y), (215, 170, 60))                                   # something golden inside
    save(curio, "villagers_curio")
    plinth = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            band = y in (0, 1, 14, 15)
            plinth.putpixel((x, y), shade((225, 220, 210), (0.85 if band else 1.0) * rng.uniform(0.94, 1.05)))
    save(plinth, "villagers_plinth")
    top = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            edge = x in (0, 15) or y in (0, 15)
            top.putpixel((x, y), shade((235, 230, 220), (0.85 if edge else 1.0) * rng.uniform(0.95, 1.04)))
    save(top, "villagers_plinth_top")


if __name__ == "__main__":
    main()
