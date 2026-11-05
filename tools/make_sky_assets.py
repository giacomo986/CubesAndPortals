#!/usr/bin/env python3
"""The meteorite of mods/sky, drawn by rule on the 16 grid and doubled to
32 px into assets/textures: a dark scorched stone, pitted, with flecks of
metal and a few still glowing orange. Seeded.

    python3 tools/make_sky_assets.py
"""
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "textures")


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c)


def main():
    rng = random.Random(9177)
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            img.putpixel((x, y), shade((62, 54, 58), rng.uniform(0.75, 1.15)))
    for _ in range(9):                          # pits
        x, y = rng.randrange(16), rng.randrange(16)
        img.putpixel((x, y), (28, 24, 26))
        img.putpixel(((x + 1) % 16, y), (40, 35, 38))
    for _ in range(10):                         # metal
        x, y = rng.randrange(16), rng.randrange(16)
        img.putpixel((x, y), shade((170, 172, 180), rng.uniform(0.9, 1.1)))
    for _ in range(5):                          # still hot
        x, y = rng.randrange(16), rng.randrange(16)
        img.putpixel((x, y), (255, 140, 40))
        img.putpixel((x, (y + 1) % 16), (200, 80, 20))
    img.resize((32, 32), Image.NEAREST).save(os.path.join(OUT, "sky_meteorite.png"))


if __name__ == "__main__":
    main()
