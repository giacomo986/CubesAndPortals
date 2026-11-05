#!/usr/bin/env python3
"""The letterbox of mods/passport, on the 16 grid doubled to 32 px into
assets/textures: a red box, its front with the slot and a gold plate under it.
The passport's icon is tools/make_item_textures.py's. Seeded.

    python3 tools/make_passport_assets.py
"""
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "textures")


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c)


def face(rng, slot):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            edge = x in (0, 15) or y in (0, 15)
            img.putpixel((x, y), shade((180, 35, 35), (0.75 if edge else 1.0) * rng.uniform(0.92, 1.06)))
    if slot:
        for x in range(4, 12):
            img.putpixel((x, 5), (30, 20, 20))
            img.putpixel((x, 6), (50, 30, 30))
        for x in range(5, 11):
            img.putpixel((x, 10), (230, 200, 90))
    return img


def main():
    rng = random.Random(77)
    face(rng, False).resize((32, 32), Image.NEAREST).save(os.path.join(OUT, "passport_letterbox.png"))
    face(rng, True).resize((32, 32), Image.NEAREST).save(os.path.join(OUT, "passport_letterbox_front.png"))


if __name__ == "__main__":
    main()
