#!/usr/bin/env python3
"""Stone grinding on stone, for mods/relics (a glyph turned, a seal giving way),
synthesised.

Brown noise through a low-pass, roughened by a fast irregular tremolo -- the
grit catching -- swelling and fading over 0.7 s, with a low thud at the start.
Pure Python to a .wav, then ffmpeg to a mono .ogg at 44.1 kHz, as
tools/make_weather_sounds.py. Seeded.

    tools/make_relic_sounds.py   # mods/relics/grind.ogg
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402


def grind(rng, length):
    out = []
    brown = lp = 0.0
    grit = 1.0
    for i in range(int(length * RATE)):
        t = i / RATE
        white = rng.uniform(-1.0, 1.0)
        brown = (brown + white * 0.05) * 0.995
        lp += (brown - lp) * 0.12
        if i % 300 == 0:
            grit = rng.uniform(0.3, 1.0)
        swell = math.sin(math.pi * min(1.0, t / length)) ** 0.7
        thud = math.sin(2 * math.pi * 70 * t) * math.exp(-t * 18.0) * 0.6
        out.append(lp * 4.0 * grit * swell + thud)
    return out


if __name__ == "__main__":
    write(grind(random.Random(9), 0.7), os.path.join(ROOT, "mods", "relics", "grind.ogg"))
