#!/usr/bin/env python3
"""A sale at a market stall (mods/trade), synthesised: three coins dropped
into a till, each a short bright ring of two inharmonic partials with a tick.
Pure Python to a .wav, then ffmpeg to a mono .ogg, as
tools/make_weather_sounds.py. Seeded.

    tools/make_trade_sounds.py   # mods/trade/coins.ogg
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402


def coins(rng, length=0.7):
    hits = [(0.0, 3100), (0.11, 3550), (0.19, 2900)]
    out = []
    for i in range(int(length * RATE)):
        t = i / RATE
        v = 0.0
        for at, f in hits:
            u = t - at
            if u < 0:
                continue
            env = math.exp(-u * 14)
            v += env * (math.sin(2 * math.pi * f * u) + 0.6 * math.sin(2 * math.pi * f * 2.76 * u))
            if u < 0.003:
                v += rng.uniform(-1, 1) * 0.8
        out.append(v * 0.25)
    return out


if __name__ == "__main__":
    write(coins(random.Random(3)), os.path.join(ROOT, "mods", "trade", "coins.ogg"))
