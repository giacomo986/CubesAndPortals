#!/usr/bin/env python3
"""A goal reached for mods/goals, synthesised: three bell tones rising (a
major triad), each a sine with two quieter overtones fading out. Pure Python
to a .wav, then ffmpeg to a mono .ogg, as tools/make_weather_sounds.py.

    tools/make_goal_sounds.py   # mods/goals/reached.ogg
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402


def chime(length=1.4):
    notes = [(0.0, 523.25), (0.12, 659.25), (0.24, 783.99)]
    out = []
    for i in range(int(length * RATE)):
        t = i / RATE
        v = 0.0
        for start, f in notes:
            u = t - start
            if u < 0:
                continue
            env = min(1.0, u / 0.004) * math.exp(-u * 3.2)
            v += env * (math.sin(2 * math.pi * f * u) + 0.3 * math.sin(4 * math.pi * f * u)
                        + 0.12 * math.sin(6 * math.pi * f * u))
        out.append(v * 0.28)
    return out


if __name__ == "__main__":
    write(chime(), os.path.join(ROOT, "mods", "goals", "reached.ogg"))
