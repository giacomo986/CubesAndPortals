#!/usr/bin/env python3
"""The Inferno's sounds (games/inferno/mods/inferno), synthesised: what the
caverns sound like round a player now and then -- a low rumble, lava bubbling
below, a moan far off -- and a fireball's fire catching. Pure Python to a
.wav, then ffmpeg to a mono .ogg at 44.1 kHz, as tools/make_weather_sounds.py.
Seeded.

    tools/make_inferno_sounds.py   # rumble, bubble, moan, ignite .ogg
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402

OUT = os.path.join(ROOT, "games", "inferno", "mods", "inferno")


def envelope(t, length, rise, fall):
    return min(1.0, t / rise) * min(1.0, (length - t) / fall)


def rumble(rng, length):
    """Brown noise low-passed twice, swelling and fading: the rock groaning."""
    out, a, b = [], 0.0, 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        a += (rng.uniform(-1, 1) - a) * 0.02
        b += (a - b) * 0.02
        swell = 0.6 + 0.4 * math.sin(2 * math.pi * 0.35 * t)
        out.append(b * 14 * swell * envelope(t, length, 0.8, 1.2))
    return out


def bubble(rng, length):
    """A few thick bubbles, each a falling tone that pops."""
    out = [0.0] * int(length * RATE)
    at = 0.05
    while at < length - 0.3:
        f0, dur = rng.uniform(90, 160), rng.uniform(0.12, 0.22)
        for i in range(int(dur * RATE)):
            t = i / RATE
            f = f0 * (1 + 1.6 * t / dur)
            v = math.sin(2 * math.pi * f * t) * math.exp(-t * 14) * 0.7
            j = int(at * RATE) + i
            if j < len(out):
                out[j] += v
        at += rng.uniform(0.15, 0.45)
    return out


def moan(rng, length):
    """A low voice far away: two detuned tones sliding down, breathy."""
    out, n = [], 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        f = 150 - 40 * t / length + 6 * math.sin(2 * math.pi * 4.5 * t)
        n += (rng.uniform(-1, 1) - n) * 0.1
        v = 0.5 * math.sin(2 * math.pi * f * t) + 0.35 * math.sin(2 * math.pi * f * 1.012 * t) + 0.3 * n
        out.append(v * 0.35 * envelope(t, length, 0.9, 1.4))
    return out


def ignite(rng, length):
    """A whoosh of noise rising and the crackle of it after."""
    out, lp = [], 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        k = min(1.0, t / 0.25)
        lp += (rng.uniform(-1, 1) - lp) * (0.05 + 0.4 * k)
        v = lp * 1.2 * math.exp(-max(0.0, t - 0.25) * 3)
        if rng.random() < 0.004:
            v += rng.uniform(-0.8, 0.8)
        out.append(v * 0.6)
    return out


if __name__ == "__main__":
    write(rumble(random.Random(71), 4.0), os.path.join(OUT, "rumble.ogg"))
    write(bubble(random.Random(72), 1.6), os.path.join(OUT, "bubble.ogg"))
    write(moan(random.Random(73), 3.2), os.path.join(OUT, "moan.ogg"))
    write(ignite(random.Random(74), 1.0), os.path.join(OUT, "ignite.ogg"))
