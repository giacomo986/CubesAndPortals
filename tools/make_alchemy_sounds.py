#!/usr/bin/env python3
"""The still bubbling and a potion drunk, for mods/alchemy, synthesised.

  bubble  a pot on the boil, 2 s: bubbles at random, each a short sine whose
          pitch rises as it grows and bursts, over a low simmer of noise.
  drink   three gulps: a falling low tone each, a little apart.

Pure Python to a .wav, then ffmpeg to a mono .ogg at 44.1 kHz, as
tools/make_weather_sounds.py. Seeded.

    tools/make_alchemy_sounds.py   # mods/alchemy/bubble.ogg, drink.ogg
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402


def bubble(rng, length):
    n = int(length * RATE)
    out = [0.0] * n
    lp = 0.0
    for i in range(n):
        lp += (rng.uniform(-1.0, 1.0) - lp) * 0.05
        out[i] = lp * 0.25
    t0 = 0.0
    while t0 < length - 0.1:
        f, d = rng.uniform(300, 700), rng.uniform(0.03, 0.07)
        start = int(t0 * RATE)
        phase = 0.0
        for j in range(int(d * RATE)):
            if start + j >= n:
                break
            t = j / RATE
            phase += 2 * math.pi * f * (1 + 2.5 * t / d) / RATE
            out[start + j] += math.sin(phase) * math.sin(math.pi * t / d) * 0.5
        t0 += rng.uniform(0.04, 0.2)
    fade = int(0.05 * RATE)
    for i in range(fade):
        out[i] *= i / fade
        out[n - 1 - i] *= i / fade
    return out


def drink(rng, length):
    out = []
    for i in range(int(length * RATE)):
        t = i / RATE
        v = 0.0
        for k in range(3):
            s = t - 0.08 - k * 0.22
            if 0 <= s < 0.14:
                f = 220 - 500 * s + rng.uniform(-3, 3)
                v += math.sin(2 * math.pi * f * s) * math.sin(math.pi * s / 0.14) * 0.7
        out.append(v)
    return out


if __name__ == "__main__":
    write(bubble(random.Random(4), 2.0), os.path.join(ROOT, "mods", "alchemy", "bubble.ogg"))
    write(drink(random.Random(5), 0.8), os.path.join(ROOT, "mods", "alchemy", "drink.ogg"))
