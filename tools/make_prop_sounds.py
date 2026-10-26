#!/usr/bin/env python3
"""The sounds of games/arpg's breakable things, synthesised.

A blow on wood is a knock (as tools/make_hatch_sounds.py's); a barrel or a
crate breaking is a heavy knock and then cracks -- short bursts of noise each
rung through a board's resonance, the first loud, the rest scattered and
fading -- with the pieces clattering after. A blow on the urn is a tap that
rings high; it breaking is a crack and a shower of tinkles, small bright
tones that die at once. Pure Python to a .wav, then ffmpeg to a mono .ogg at
44.1 kHz. Seeded, so a rerun writes the same files.

    tools/make_prop_sounds.py   # games/arpg/mods/arpg_core/{barrel,crate,urn}_{hurt,death}.ogg
"""
import math
import os
import random
import struct
import subprocess
import tempfile
import wave

RATE = 44100
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "games", "arpg", "mods", "arpg_core")


def resonate(source, pitches, decay):
    """Runs a signal through two-pole resonators, one a pitch, and sums them."""
    out = [0.0] * len(source)
    for f, gain in pitches:
        w = 2.0 * math.pi * f / RATE
        a1, a2 = 2.0 * decay * math.cos(w), -decay * decay
        y1 = y2 = 0.0
        for i, x in enumerate(source):
            y = x + a1 * y1 + a2 * y2
            y2, y1 = y1, y
            out[i] += y * gain
    return out


def burst(rng, length, sharp):
    """Noise struck once and dying: what drives a resonance."""
    return [rng.uniform(-1.0, 1.0) * math.exp(-i / RATE * sharp) for i in range(int(length * RATE))]


def mix(into, sound, at, loud=1.0):
    start = int(at * RATE)
    if len(into) < start + len(sound):
        into += [0.0] * (start + len(sound) - len(into))
    for i, s in enumerate(sound):
        into[start + i] += s * loud
    return into


def knock(rng, pitch, loud=1.0):
    return [s * loud * 0.05 for s in resonate(burst(rng, 0.3, 90.0), ((pitch, 1.0), (pitch * 1.5, 0.5)), 0.9975)]


def crack(rng, pitch):
    """A board giving way: sharp noise through a high, quick resonance, and the raw noise over it."""
    b = burst(rng, 0.12, 60.0)
    rung = resonate(b, ((pitch, 1.0), (pitch * 2.3, 0.6), (pitch * 3.7, 0.3)), 0.993)
    return [r * 0.03 + n * 0.35 * math.exp(-i / RATE * 45.0) for i, (r, n) in enumerate(zip(rung, b))]


def wood_break(rng, low, length):
    out = mix([], knock(rng, low, 1.4), 0.0)
    t = 0.0
    for k in range(9):
        t += rng.uniform(0.015, 0.06) * (1 + k * 0.25)
        mix(out, crack(rng, rng.uniform(600, 1500)), t, math.exp(-k * 0.28))
    for k in range(7):   # the pieces landing
        mix(out, knock(rng, rng.uniform(250, 520), 0.35), 0.25 + rng.uniform(0.0, length - 0.5),
            rng.uniform(0.3, 0.7))
    return out


def tink(rng, pitch, length):
    """A shard: a bright tone, a little out of tune with itself, dying fast."""
    out = []
    p1, p2 = 0.0, 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        p1 += pitch / RATE
        p2 += pitch * 2.76 / RATE
        env = math.exp(-t * rng.uniform(28.0, 40.0))
        out.append((math.sin(2 * math.pi * p1) + 0.4 * math.sin(2 * math.pi * p2)) * env * 0.4)
    return out


def shatter(rng):
    out = mix([], crack(rng, 2600), 0.0, 1.0)
    mix(out, crack(rng, 1900), 0.012, 0.8)
    for k in range(26):
        mix(out, tink(rng, rng.uniform(2200, 6500), 0.18), rng.uniform(0.02, 0.75) ** 1.4,
            rng.uniform(0.25, 0.8) * math.exp(-k * 0.05))
    return out


def tap(rng):
    out = mix([], knock(rng, 420, 0.7), 0.0)
    return mix(out, tink(rng, 1850, 0.35), 0.0, 0.9)


def write(samples, name):
    samples += [0.0] * int(0.05 * RATE)
    fade = int(0.02 * RATE)
    for i in range(fade):
        samples[-1 - i] *= i / fade
    peak = max(1e-6, max(abs(s) for s in samples))
    k = 0.8 / peak
    path = os.path.join(OUT, name + ".ogg")
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
        wav = tmp.name
    with wave.open(wav, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * k)) * 32767)) for s in samples))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-ac", "1", "-ar", str(RATE),
                    "-c:a", "libvorbis", "-q:a", "4", path], check=True)
    os.unlink(wav)
    print("wrote", os.path.relpath(path, ROOT))


rng = random.Random(17)
write(knock(rng, 150, 1.0), "barrel_hurt")
write(wood_break(rng, 120, 0.9), "barrel_death")
write(knock(rng, 190, 1.0), "crate_hurt")
write(wood_break(rng, 170, 0.8), "crate_death")
write(tap(rng), "urn_hurt")
write(shatter(rng), "urn_death")
