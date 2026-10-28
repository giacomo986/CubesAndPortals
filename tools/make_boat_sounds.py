#!/usr/bin/env python3
"""A paddle stroke for mods/boats, synthesised.

The blade going in is a short bright splash -- white noise through a
band-pass, quick to fade -- and the pull after it a low gurgle: noise through
a low-pass, swelling and falling over half a second, with a slow wobble for the
water churning. Pure Python to a .wav, then ffmpeg to a mono .ogg at 44.1 kHz,
as tools/make_weather_sounds.py. Seeded.

    tools/make_boat_sounds.py   # mods/boats/paddle.ogg
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402


def paddle(rng, length):
    out = []
    lo = band_lo = band_hi = 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        white = rng.uniform(-1.0, 1.0)
        band_hi += (white - band_hi) * 0.45
        band_lo += (band_hi - band_lo) * 0.08
        splash = (band_hi - band_lo) * math.exp(-t * 18.0) * min(1.0, t / 0.005)
        lo += (white - lo) * 0.03
        swell = math.sin(math.pi * min(1.0, max(0.0, (t - 0.04) / 0.5))) ** 2
        wobble = 0.6 + 0.4 * math.sin(t * 2 * math.pi * 7.0)
        out.append(splash * 1.4 + lo * 3.0 * swell * wobble)
    return out


if __name__ == "__main__":
    write(paddle(random.Random(11), 0.6), os.path.join(ROOT, "mods", "boats", "paddle.ogg"))
