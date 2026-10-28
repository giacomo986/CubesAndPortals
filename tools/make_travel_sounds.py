#!/usr/bin/env python3
"""The whoosh of a trip by waystone, for mods/travel, synthesised.

Noise through a band-pass whose centre sweeps up and back down, swelling and
fading over a second, with a soft bell under it. Pure Python to a .wav, then
ffmpeg to a mono .ogg at 44.1 kHz, as tools/make_weather_sounds.py. Seeded.

    tools/make_travel_sounds.py   # mods/travel/whoosh.ogg
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402


def whoosh(rng, length):
    out = []
    lo = hi = 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        u = t / length
        white = rng.uniform(-1.0, 1.0)
        k = 0.02 + 0.25 * math.sin(math.pi * u) ** 2   # the band's centre, swept
        hi += (white - hi) * k
        lo += (hi - lo) * k * 0.3
        swell = math.sin(math.pi * u) ** 1.5
        bell = math.sin(2 * math.pi * 660 * t) * 0.15 * math.exp(-t * 3.0) + \
            math.sin(2 * math.pi * 990 * t) * 0.08 * math.exp(-t * 4.0)
        out.append((hi - lo) * 2.5 * swell + bell)
    return out


if __name__ == "__main__":
    write(whoosh(random.Random(3), 1.1), os.path.join(ROOT, "mods", "travel", "whoosh.ogg"))
