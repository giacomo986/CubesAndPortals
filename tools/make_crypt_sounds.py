#!/usr/bin/env python3
"""The crypt's sounds for games/arpg/mods/arpg_crypt, synthesised.

  custode_say    a low hollow moan through a skull, 1.2 s
  custode_hurt   a short dry hiss and a bone knock
  custode_death  the moan falling away into a rattle, 2 s
  custode_shoot  a whoosh of fire leaving the staff
  wake           a floor waking: a deep rumble under a tolled bell, 2.5 s
  seal           a seal breaking: a stone crack and a glassy chime fading
  drip           a floor's air, looped: water dripping in a hollow room, 6 s

Pure Python, written as the other tools/make_*_sounds.py are. Seeded, so a
rerun writes the same files.

    python3 tools/make_crypt_sounds.py
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402

OUT = os.path.join(ROOT, "games", "arpg", "mods", "arpg_crypt")


def n(length):
    return int(length * RATE)


def env(t, length, attack, release):
    return max(0.0, min(1.0, t / attack if attack > 0 else 1.0, (length - t) / release if release > 0 else 1.0))


def moan(rng, length, f0, f1):
    out, ph, lp = [], 0.0, 0.0
    for i in range(n(length)):
        t = i / RATE
        u = t / length
        f = f0 + (f1 - f0) * u + 3 * math.sin(2 * math.pi * 5 * t)
        ph += f / RATE
        # A few odd harmonics, breathy: hollow, as through a skull.
        v = sum(math.sin(2 * math.pi * ph * h) / h for h in (1, 3, 5))
        lp += (rng.uniform(-1, 1) - lp) * 0.1
        out.append((v * 0.7 + lp * 0.6) * env(t, length, 0.15, 0.4))
    return out


def knock(out, at, freq, decay, amp):
    s = n(at)
    for i in range(s, min(len(out), s + n(5.0 / decay))):
        t = (i - s) / RATE
        out[i] += amp * math.sin(2 * math.pi * freq * t) * math.exp(-t * decay)


def noise_burst(rng, length, cut, decay):
    out, lp = [], 0.0
    for i in range(n(length)):
        t = i / RATE
        lp += (rng.uniform(-1, 1) - lp) * cut
        out.append(lp * math.exp(-t * decay))
    return out


def custode_hurt(rng):
    out = noise_burst(rng, 0.45, 0.5, 9.0)
    knock(out, 0.05, 420, 40, 0.8)
    return out


def custode_death(rng):
    out = moan(rng, 2.0, 140, 60)
    t = 1.2
    while t < 1.95:
        knock(out, t, rng.uniform(500, 900), 60, 0.5)
        t += rng.uniform(0.04, 0.1)
    return out


def whoosh(rng, length):
    out, lp = [], 0.0
    for i in range(n(length)):
        t = i / RATE
        cut = 0.02 + 0.25 * math.sin(math.pi * t / length)
        lp += (rng.uniform(-1, 1) - lp) * cut
        out.append(lp * env(t, length, 0.05, 0.3))
    return out


def wake(rng):
    length = 2.5
    out, brown = [], 0.0
    for i in range(n(length)):
        t = i / RATE
        brown = (brown + rng.uniform(-1, 1) * 0.02) * 0.998
        bell = sum(math.sin(2 * math.pi * f * t) * a for f, a in ((110, 1.0), (276, 0.5), (402, 0.3))) * math.exp(-t * 1.4)
        out.append(brown * 6 * env(t, length, 0.3, 0.8) + bell * 0.6)
    return out


def seal(rng):
    out = noise_burst(rng, 1.6, 0.7, 14.0)
    for f, a in ((1320, 0.5), (1980, 0.35), (2640, 0.2)):
        for i in range(n(0.08), len(out)):
            t = i / RATE - 0.08
            out[i] += a * math.sin(2 * math.pi * f * t) * math.exp(-t * 2.5)
    return out


def drip(rng):
    length = 6.0
    out = [0.0] * n(length)
    # A faint hollow air under the drops, the same at both ends so it loops.
    lp = 0.0
    for i in range(len(out)):
        lp += (rng.uniform(-1, 1) - lp) * 0.01
        out[i] = lp * 0.4
    for at in (0.4, 1.7, 2.3, 3.9, 5.1):
        f = rng.uniform(900, 1500)
        s = n(at)
        for i in range(s, min(len(out), s + n(0.25))):
            t = (i - s) / RATE
            out[i] += math.sin(2 * math.pi * (f + 900 * t) * t) * math.exp(-t * 22) * 0.9
            # and its echo off the walls
            if i + n(0.18) < len(out):
                out[i + n(0.18)] += math.sin(2 * math.pi * f * t) * math.exp(-t * 22) * 0.3
    return out


if __name__ == "__main__":
    sounds = {
        "custode_say": moan(random.Random(1), 1.2, 95, 80),
        "custode_hurt": custode_hurt(random.Random(2)),
        "custode_death": custode_death(random.Random(3)),
        "custode_shoot": whoosh(random.Random(4), 0.6),
        "wake": wake(random.Random(5)),
        "seal": seal(random.Random(6)),
        "drip": drip(random.Random(7)),
    }
    for name, samples in sounds.items():
        write(samples, os.path.join(OUT, name + ".ogg"))
