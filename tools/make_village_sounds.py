#!/usr/bin/env python3
"""The village events' sounds, synthesised in pure Python to a .wav, then
ffmpeg to a mono .ogg at 44.1 kHz, as tools/make_weather_sounds.py. Seeded.

    tools/make_village_sounds.py
    # mods/villagers/bell.ogg    the hall's bell: three strokes, a bright bronze ring
    # mods/villagers/music.ogg   the feast's tune: a fiddle-like reed over a drone
    #                            and a drum, 12 seconds, played again and again
    # mods/fireworks/whistle.ogg a rocket going up
    # mods/fireworks/bang.ogg    its burst, with a crackle after
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402


def bell(rng, strokes=3, gap=1.1, length=5.0):
    base = 392.0
    partials = [(0.5, 0.5, 0.9), (1.0, 1.0, 1.4), (1.19, 0.6, 1.9), (1.5, 0.45, 2.4), (2.0, 0.4, 3.0),
                (2.51, 0.25, 4.0), (3.0, 0.15, 5.0)]
    out = [0.0] * int(length * RATE)
    for s in range(strokes):
        start = int(s * gap * RATE)
        phases = [rng.uniform(0, 6.28) for _ in partials]
        for i in range(start, len(out)):
            t = (i - start) / RATE
            v = 0.0
            for (ratio, amp, decay), ph in zip(partials, phases):
                v += amp * math.exp(-t * decay) * math.sin(2 * math.pi * base * ratio * t + ph)
            v *= min(1.0, t / 0.002)
            out[i] += v * 0.28
    return out


def music(rng, length=12.0):
    # A tune in D, eighth notes at 132 a minute, a bar of four beats each line.
    beat = 60 / 132
    D = 293.66
    steps = [0, 2, 4, 5, 7, 9, 11, 12, 14]
    tune = [0, 2, 4, 2, 0, 4, 7, 4, 5, 4, 2, 4, 0, -1, 0, -1,
            7, 5, 4, 5, 7, 8, 7, 5, 4, 2, 4, 2, 0, -1, 0, -1,
            0, 4, 7, 8, 7, 4, 5, 2, 4, 2, 0, 2, 4, -1, 0, -1]
    out = [0.0] * int(length * RATE)
    eighth = beat / 2
    for k, n in enumerate(tune):
        if n < 0:
            continue
        f = D * 2 ** (steps[n % 9] / 12) if n < 9 else D * 2
        start, dur = int(k * eighth * RATE), eighth * 1.9
        for i in range(start, min(len(out), start + int(dur * RATE))):
            t = (i - start) / RATE
            vib = 1 + 0.004 * math.sin(2 * math.pi * 5.5 * t)
            ph = 2 * math.pi * f * vib * t
            v = math.sin(ph) + 0.5 * math.sin(2 * ph) + 0.3 * math.sin(3 * ph) + 0.15 * math.sin(4 * ph)
            env = min(1.0, t / 0.03) * math.exp(-t * 2.2)
            out[i] += v * env * 0.16
    for i in range(len(out)):
        t = i / RATE
        # A drone on D and A, and a drum on every beat, louder on the first.
        out[i] += 0.05 * (math.sin(2 * math.pi * D / 2 * t) + 0.7 * math.sin(2 * math.pi * D * 0.75 * t))
        b = t % beat
        first = int(t / beat) % 4 == 0
        out[i] += math.sin(2 * math.pi * (70 + 60 * math.exp(-b * 30)) * b) * math.exp(-b * 14) * (0.4 if first else 0.25)
    fade = int(0.05 * RATE)
    for i in range(fade):
        out[i] *= i / fade
        out[-1 - i] *= i / fade
    return out


def whistle(rng, length=1.3):
    out = []
    ph = 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        f = 900 + 1500 * t / length
        ph += 2 * math.pi * f / RATE
        hiss = rng.uniform(-1, 1) * 0.3
        env = min(1.0, t / 0.1) * (1 - (t / length) ** 3)
        out.append((math.sin(ph) * 0.35 + hiss * 0.5) * env * 0.5)
    return out


def bang(rng, length=2.2):
    out = []
    low = 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        n = rng.uniform(-1, 1)
        low += (n - low) * 0.08
        v = low * 3.0 * math.exp(-t * 7) + math.sin(2 * math.pi * 50 * t) * math.exp(-t * 12) * 0.7
        if t > 0.25 and rng.random() < 0.0012 * math.exp(-(t - 0.25) * 2):
            v += rng.uniform(0.5, 1.0)     # a crackle
        out.append(max(-1, min(1, v * 0.6)))
    # Every crackle a short click, not one sample.
    for i in range(len(out) - 40, 0, -1):
        if abs(out[i]) > 0.45 and i / RATE > 0.25:
            for k in range(1, 40):
                out[i + k] += out[i] * math.exp(-k / 8) * (1 if k % 2 else -1)
    return out


if __name__ == "__main__":
    write(bell(random.Random(31)), os.path.join(ROOT, "mods", "villagers", "bell.ogg"))
    write(music(random.Random(32)), os.path.join(ROOT, "mods", "villagers", "music.ogg"))
    write(whistle(random.Random(33)), os.path.join(ROOT, "mods", "fireworks", "whistle.ogg"))
    write(bang(random.Random(34)), os.path.join(ROOT, "mods", "fireworks", "bang.ogg"))
