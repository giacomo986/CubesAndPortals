#!/usr/bin/env python3
"""mods/album's frame, on the 16 grid doubled to 32 px into assets/textures --
a wooden frame round a pale mount, which the photograph is drawn over
(core.render.screen) -- and the camera's shutter, synthesised into
mods/album/shutter.ogg: a quick click and the spring's whirr. The camera's
icon is tools/make_item_textures.py's. Seeded.

    python3 tools/make_album_assets.py
"""
import math
import os
import random
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402

OUT = os.path.join(ROOT, "assets", "textures")


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c)


def frame(rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            edge = min(x, y, 15 - x, 15 - y)
            c = (130, 85, 45) if edge < 1 else (160, 110, 60) if edge < 2 else (225, 218, 200)
            img.putpixel((x, y), shade(c, rng.uniform(0.92, 1.06)))
    return img


def shutter(rng, length):
    out = []
    for i in range(int(length * RATE)):
        t = i / RATE
        click = rng.uniform(-1, 1) * math.exp(-t * 300) + rng.uniform(-1, 1) * math.exp(-max(0, t - 0.06) * 260) * (t > 0.06)
        whirr = math.sin(2 * math.pi * 900 * t) * 0.15 * math.exp(-t * 18) * (t > 0.02)
        out.append(click + whirr)
    return out


def main():
    rng = random.Random(1888)
    img = frame(rng)
    img.resize((32, 32), Image.NEAREST).save(os.path.join(OUT, "album_frame.png"))
    write(shutter(random.Random(3), 0.25), os.path.join(ROOT, "mods", "album", "shutter.ogg"))


if __name__ == "__main__":
    main()
