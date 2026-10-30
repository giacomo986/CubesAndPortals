#!/usr/bin/env python3
"""The hidden way out of a level opening, for mods/keys, synthesised.

  rumble  1.6 s: stone grinding aside, low filtered noise swelling and dying,
          a few knocks of falling grit over it, and a soft chime as the lamp
          over the niche lights.

Pure Python to a .wav, then ffmpeg to a mono .ogg at 44.1 kHz, as
tools/make_weather_sounds.py. Seeded.

    tools/make_keys_sounds.py   # mods/keys/rumble.ogg
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402


def rumble(rng, length):
    n = int(length * RATE)
    out = [0.0] * n
    lp1 = lp2 = 0.0
    for i in range(n):
        t = i / RATE
        lp1 += (rng.uniform(-1.0, 1.0) - lp1) * 0.02
        lp2 += (lp1 - lp2) * 0.05
        swell = math.sin(math.pi * min(1.0, t / 1.2)) ** 0.7 if t < 1.2 else 0.0
        out[i] = lp2 * 6.0 * swell
    for _ in range(14):
        start = int(rng.uniform(0.1, 1.1) * RATE)
        f = rng.uniform(90, 260)
        for j in range(int(0.05 * RATE)):
            if start + j < n:
                s = j / RATE
                out[start + j] += math.sin(2 * math.pi * f * s) * math.exp(-s * 60) * 0.5
    start = int(1.15 * RATE)
    for j in range(n - start):
        s = j / RATE
        out[start + j] += (math.sin(2 * math.pi * 660 * s) + 0.5 * math.sin(2 * math.pi * 990 * s)) \
            * math.exp(-s * 5) * 0.25
    fade = int(0.04 * RATE)
    for i in range(fade):
        out[i] *= i / fade
        out[n - 1 - i] *= i / fade
    return out


if __name__ == "__main__":
    write(rumble(random.Random(11), 1.6), os.path.join(ROOT, "mods", "keys", "rumble.ogg"))
