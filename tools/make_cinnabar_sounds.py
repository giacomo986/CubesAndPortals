#!/usr/bin/env python3
"""Sounds for mods/cinnabar, synthesised: a pressure plate's click (a short
knock of noise and a low thump) and spikes shooting up (a metallic ring over a
quick scrape). Pure Python to a .wav, then ffmpeg to a mono .ogg, as
tools/make_weather_sounds.py. Seeded.

    tools/make_cinnabar_sounds.py   # mods/cinnabar/click.ogg, spikes.ogg
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402


def click(rng, length=0.12):
    out = []
    hi = 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        w = rng.uniform(-1, 1)
        hi += (w - hi) * 0.5
        out.append(hi * math.exp(-t * 90) * 0.8 + math.sin(2 * math.pi * 140 * t) * math.exp(-t * 40) * 0.5)
    return out


def spikes(rng, length=0.5):
    out = []
    lo = 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        w = rng.uniform(-1, 1)
        lo += (w - lo) * 0.3
        scrape = lo * math.exp(-t * 30) * 0.7
        ring = sum(a * math.sin(2 * math.pi * f * t) for f, a in ((1870, 0.3), (2930, 0.2), (4410, 0.12)))
        out.append(scrape + ring * math.exp(-t * 9) * min(1.0, t / 0.01) * 0.6)
    return out


if __name__ == "__main__":
    rng = random.Random(5)
    write(click(rng), os.path.join(ROOT, "mods", "cinnabar", "click.ogg"))
    write(spikes(rng), os.path.join(ROOT, "mods", "cinnabar", "spikes.ogg"))
