#!/usr/bin/env python3
"""The voices of mods/garden's insects, synthesised into mods/garden.

The engine plays "<creature>_say" now and then, "_hurt" when struck and
"_death" when killed (garden:ant_say is mods/garden/ant_say.ogg).

  ant        say: a dry chitter, a few quick high clicks; hurt: one sharp
             tick and a squeak; death: a small crunch.
  ladybird   say: a short whirr of wings opening; hurt: a click of the
             shell; death: a crunch, lower than the ant's.
  butterfly  say: soft wing beats, breaths of low noise; hurt: one rustle;
             death: the beats slowing and fading.

Seeded, so running it again makes the same files.

    python3 tools/make_garden_sounds.py
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402

MOD = os.path.join(ROOT, "mods", "garden")


def silence(length):
    return [0.0] * int(length * RATE)


def click(out, at, freq, decay, amp=1.0):
    """A ringing tick at `at` seconds: a sine that dies in 1/decay s."""
    start = int(at * RATE)
    for i in range(start, min(len(out), start + int(6.0 / decay * RATE))):
        t = (i - start) / RATE
        out[i] += amp * math.sin(2 * math.pi * freq * t) * math.exp(-t * decay)


def crunch(rng, length, cutoff):
    """Noise in a fast burst of grains, low-passed to `cutoff` (0..1)."""
    out = silence(length)
    lp = 0.0
    for i in range(len(out)):
        t = i / RATE
        grain = 1.0 if rng.random() < 0.02 else 0.15
        lp += (rng.uniform(-1, 1) * grain - lp) * cutoff
        out[i] = lp * math.exp(-t * 9.0)
    return out


def ant_say(rng):
    out = silence(0.35)
    t = 0.01
    while t < 0.3:
        click(out, t, rng.uniform(3800, 5200), 400, rng.uniform(0.6, 1.0))
        t += rng.uniform(0.025, 0.06)
    return out


def ant_hurt(rng):
    out = silence(0.25)
    click(out, 0.0, 4500, 300)
    for i in range(len(out)):
        t = i / RATE
        if 0.03 < t < 0.2:
            f = 2600 + 1800 * (t - 0.03) / 0.17
            out[i] += 0.5 * math.sin(2 * math.pi * f * t) * math.sin(math.pi * (t - 0.03) / 0.17)
    return out


def ladybird_say(rng):
    n = int(0.45 * RATE)
    out, ph = [], 0.0
    for i in range(n):
        t = i / RATE
        ph = (ph + 140 * (1 + 0.05 * math.sin(2 * math.pi * 7 * t)) / RATE) % 1.0
        env = min(1.0, t / 0.05, (0.45 - t) / 0.15)
        out.append((2 * ph - 1) * env * (0.6 + 0.4 * rng.random()))
    return out


def ladybird_hurt(rng):
    out = silence(0.15)
    click(out, 0.0, 2200, 250)
    click(out, 0.02, 3100, 350, 0.6)
    return out


def flutter(rng, length, beats):
    """Soft wing beats: low-passed noise swelling at each beat time."""
    out = silence(length)
    lp1 = lp2 = 0.0
    for i in range(len(out)):
        t = i / RATE
        lp1 += (rng.uniform(-1, 1) - lp1) * 0.08
        lp2 += (lp1 - lp2) * 0.08
        env = sum(math.exp(-((t - b) / 0.025) ** 2) * a for b, a in beats)
        out[i] = lp2 * env
    return out


def butterfly_say(rng):
    return flutter(rng, 0.6, [(0.05 + k * 0.09, 1.0) for k in range(6)])


def butterfly_hurt(rng):
    return flutter(rng, 0.3, [(0.05, 1.0), (0.12, 0.5)])


def butterfly_death(rng):
    beats, t, gap = [], 0.05, 0.09
    for k in range(6):
        beats.append((t, 1.0 - k * 0.15))
        t += gap
        gap *= 1.35
    return flutter(rng, t + 0.1, beats)


if __name__ == "__main__":
    sounds = {
        "ant_say": ant_say(random.Random(1)),
        "ant_hurt": ant_hurt(random.Random(2)),
        "ant_death": crunch(random.Random(3), 0.3, 0.6),
        "ladybird_say": ladybird_say(random.Random(4)),
        "ladybird_hurt": ladybird_hurt(random.Random(5)),
        "ladybird_death": crunch(random.Random(6), 0.35, 0.3),
        "butterfly_say": butterfly_say(random.Random(7)),
        "butterfly_hurt": butterfly_hurt(random.Random(8)),
        "butterfly_death": butterfly_death(random.Random(9)),
    }
    for name, samples in sounds.items():
        write(samples, os.path.join(MOD, name + ".ogg"))
