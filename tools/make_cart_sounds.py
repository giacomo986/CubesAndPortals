#!/usr/bin/env python3
"""The clack of a cart's wheels over a rail joint, for mods/carts, synthesised.

Two knocks, front wheels then back -- a short damped ring at a metallic pitch
over a click of noise -- on a low rumble of the tub. Pure Python to a .wav, then
ffmpeg to a mono .ogg at 44.1 kHz, as tools/make_weather_sounds.py. Seeded.

    tools/make_cart_sounds.py   # mods/carts/clack.ogg
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402


def clack(rng, length):
    out = []
    lo = 0.0
    knocks = [(0.0, 1.0), (0.16, 0.8)]
    for i in range(int(length * RATE)):
        t = i / RATE
        white = rng.uniform(-1.0, 1.0)
        lo += (white - lo) * 0.02
        s = lo * 2.0 * math.exp(-t * 4.0)
        for at, a in knocks:
            if t >= at:
                u = t - at
                ring = math.sin(2 * math.pi * 820 * u) * 0.6 + math.sin(2 * math.pi * 1370 * u) * 0.4
                s += a * (ring * math.exp(-u * 45.0) + white * math.exp(-u * 300.0) * 0.8)
        out.append(s)
    return out


if __name__ == "__main__":
    write(clack(random.Random(5), 0.4), os.path.join(ROOT, "mods", "carts", "clack.ogg"))
