#!/usr/bin/env python3
"""The gong of mods/arena, synthesised: a low strike whose inharmonic
partials ring on and die slowly, the higher ones first, with a soft thud at
the start. Pure Python to a .wav, then ffmpeg to a mono .ogg at 44.1 kHz, as
tools/make_weather_sounds.py. Seeded.

    tools/make_arena_sounds.py   # mods/arena/gong.ogg
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402


def gong(rng, length):
    base = 110.0
    partials = [(1.0, 1.0, 1.2), (1.48, 0.7, 1.6), (2.03, 0.5, 2.2), (2.74, 0.35, 3.0), (3.39, 0.25, 4.0),
                (4.22, 0.15, 5.5)]
    phases = [rng.uniform(0, 6.28) for _ in partials]
    out = []
    for i in range(int(length * RATE)):
        t = i / RATE
        v = 0.0
        for (ratio, amp, decay), ph in zip(partials, phases):
            wobble = 1 + 0.002 * math.sin(2 * math.pi * 3.1 * t)
            v += amp * math.exp(-t * decay / 2.2) * math.sin(2 * math.pi * base * ratio * wobble * t + ph)
        v *= min(1.0, t / 0.004)
        v += math.sin(2 * math.pi * 55 * t) * math.exp(-t * 25) * 0.6
        out.append(v * 0.45)
    return out


if __name__ == "__main__":
    write(gong(random.Random(16), 3.5), os.path.join(ROOT, "mods", "arena", "gong.ogg"))
