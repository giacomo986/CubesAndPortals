#!/usr/bin/env python3
"""The track's power-up sounds, synthesised into games/track/mods/track_core.

  box      an item box taken: a quick rising arpeggio, bright
  boost    a whoosh rising in pitch, 0.8 s
  star     a sparkling run of high bells, 1.2 s
  throw    a shell or a banana let go: a short swish
  spin     a kart hit and spun: a falling wobble and a skid
  bolt     lightning: a crack and a deep falling zap, 1 s
  ink      a wet splat
  rattle   the roulette of a box turning, 1 s (track_game plays it with box)

The backrooms torch's beam too, into games/backrooms/mods/backrooms_hand:

  beam     a humming switch-on, 0.6 s

Pure Python, written as the other tools/make_*_sounds.py are. Seeded.

    python3 tools/make_powerup_sounds.py
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402

TRACK = os.path.join(ROOT, "games", "track", "mods", "track_core")
BACKROOMS = os.path.join(ROOT, "games", "backrooms", "mods", "backrooms_hand")


def n(length):
    return int(length * RATE)


def env(t, length, attack, release):
    return max(0.0, min(1.0, t / attack if attack > 0 else 1.0, (length - t) / release if release > 0 else 1.0))


def tone(out, at, f, length, amp, decay=6.0, square=False):
    s = n(at)
    for i in range(s, min(len(out), s + n(length))):
        t = (i - s) / RATE
        v = math.sin(2 * math.pi * f * t)
        if square:
            v = 1.0 if v >= 0 else -1.0
        out[i] += amp * v * math.exp(-t * decay) * min(1.0, t / 0.005)


def box(rng):
    out = [0.0] * n(0.5)
    for k, f in enumerate((523, 659, 784, 1047)):
        tone(out, k * 0.06, f, 0.3, 0.6, 9.0, square=True)
    return out


def rattle(rng):
    out = [0.0] * n(1.0)
    t = 0.0
    gap = 0.04
    while t < 0.95:
        tone(out, t, rng.choice((880, 988, 1175)), 0.05, 0.4, 40.0, square=True)
        t += gap
        gap *= 1.06
    return out


def whoosh(rng, length, f0, f1):
    out, lp, ph = [], 0.0, 0.0
    for i in range(n(length)):
        t = i / RATE
        u = t / length
        lp += (rng.uniform(-1, 1) - lp) * (0.05 + 0.3 * u)
        ph += (f0 + (f1 - f0) * u) / RATE
        out.append((lp * 0.8 + 0.3 * math.sin(2 * math.pi * ph)) * env(t, length, 0.05, 0.25))
    return out


def star(rng):
    out = [0.0] * n(1.2)
    for k in range(14):
        tone(out, k * 0.07, rng.choice((1568, 1760, 2093, 2349, 2637)), 0.4, 0.4, 7.0)
    return out


def spin(rng):
    out, ph = [], 0.0
    length = 0.9
    for i in range(n(length)):
        t = i / RATE
        f = 600 - 450 * t / length + 60 * math.sin(2 * math.pi * 9 * t)
        ph += f / RATE
        skid = rng.uniform(-1, 1) * 0.3 * math.exp(-t * 2)
        out.append((0.6 * math.sin(2 * math.pi * ph) + skid) * env(t, length, 0.01, 0.3))
    return out


def bolt(rng):
    out, ph, lp = [], 0.0, 0.0
    length = 1.0
    for i in range(n(length)):
        t = i / RATE
        crack = rng.uniform(-1, 1) * math.exp(-t * 25)
        ph += (900 * math.exp(-t * 4) + 60) / RATE
        saw = 2 * (ph % 1.0) - 1
        lp += (saw - lp) * 0.2
        out.append(crack + lp * 0.7 * env(t, length, 0.01, 0.4))
    return out


def ink(rng):
    out, lp = [], 0.0
    length = 0.45
    for i in range(n(length)):
        t = i / RATE
        lp += (rng.uniform(-1, 1) - lp) * 0.15
        gurgle = math.sin(2 * math.pi * (180 + 120 * math.sin(2 * math.pi * 14 * t)) * t) * 0.4
        out.append((lp + gurgle * math.exp(-t * 6)) * math.exp(-t * 7))
    return out


def beam(rng):
    out, ph = [], 0.0
    length = 0.6
    for i in range(n(length)):
        t = i / RATE
        f = 120 + 60 * min(1.0, t / 0.15)
        ph += f / RATE
        hum = sum(math.sin(2 * math.pi * ph * h) / h for h in (1, 2, 3))
        click = rng.uniform(-1, 1) * math.exp(-t * 60)
        out.append((hum * 0.5 + click) * env(t, length, 0.02, 0.25))
    return out


if __name__ == "__main__":
    for name, samples in {
        "box": box(random.Random(1)),
        "rattle": rattle(random.Random(2)),
        "boost": whoosh(random.Random(3), 0.8, 200, 900),
        "star": star(random.Random(4)),
        "throw": whoosh(random.Random(5), 0.25, 500, 300),
        "spin": spin(random.Random(6)),
        "bolt": bolt(random.Random(7)),
        "ink": ink(random.Random(8)),
    }.items():
        write(samples, os.path.join(TRACK, name + ".ogg"))
    write(beam(random.Random(9)), os.path.join(BACKROOMS, "beam.ogg"))
