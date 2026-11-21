#!/usr/bin/env python3
"""The cursed ghost's sounds (mods/ghost), synthesised.

  cursed_say     a low wavering moan, now and then while it is about
  cursed_hurt    a short shriek, a blow landed on it
  cursed_death   a wail rising and thinning away, as it comes apart
  hold           a cold whoosh and a chuckle: it has taken hold of somebody
  throw          a little airy toss: a thing thrown out of the bag

Pure Python to a .wav, then ffmpeg to a mono .ogg at 44.1 kHz. Seeded, so a
rerun writes the same files.

    tools/make_ghost_sounds.py   # mods/ghost/*.ogg
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
OUT = os.path.join(ROOT, "mods", "ghost")
rng = random.Random(7)


def write(name, samples):
    peak = max(1e-6, max(abs(s) for s in samples))
    k = 0.85 / peak
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
        tmp = f.name
    with wave.open(tmp, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(s * k * 32767)) for s in samples))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", tmp, "-ac", "1", "-ar", str(RATE),
                    os.path.join(OUT, name + ".ogg")], check=True)
    os.unlink(tmp)
    print("wrote", name)


def voice(length, pitch, vibrato=5.0, depth=0.04, breath=0.25, env=None):
    """A breathy vowel: a few harmonics on a wavering pitch, with air in it."""
    n = int(length * RATE)
    out, ph, lp = [], 0.0, 0.0
    for i in range(n):
        t = i / n
        f = pitch(t) * (1 + depth * math.sin(2 * math.pi * vibrato * i / RATE))
        ph += 2 * math.pi * f / RATE
        tone = math.sin(ph) + 0.5 * math.sin(2 * ph) + 0.25 * math.sin(3 * ph)
        lp += 0.08 * (rng.uniform(-1, 1) - lp)   # soft hiss
        e = env(t) if env else math.sin(math.pi * t) ** 0.8
        out.append(e * (0.6 * tone + breath * 6 * lp))
    return out


def echo(samples, delay=0.11, gain=0.35, times=3):
    d = int(delay * RATE)
    out = samples + [0.0] * d * times
    for k in range(1, times + 1):
        g = gain ** k
        for i, s in enumerate(samples):
            out[i + d * k] += g * s
    return out


write("cursed_say", echo(voice(1.6, lambda t: 150 + 40 * math.sin(math.pi * t) - 30 * t)))
write("cursed_hurt", voice(0.35, lambda t: 700 + 500 * math.sin(math.pi * t), vibrato=11, depth=0.08,
                           breath=0.4, env=lambda t: min(1, t * 20) * (1 - t) ** 1.5))
write("cursed_death", echo(voice(1.4, lambda t: 260 + 620 * t, vibrato=7, depth=0.06, breath=0.5,
                                 env=lambda t: min(1, t * 8) * (1 - t) ** 2), delay=0.14, gain=0.45))

# A whoosh (noise swelling through a sweeping band) and a chuckle on top.
n = int(0.9 * RATE)
whoosh, lp, bp = [], 0.0, 0.0
for i in range(n):
    t = i / n
    a = 0.02 + 0.2 * t
    lp += a * (rng.uniform(-1, 1) - lp)
    bp = lp - bp * 0.3
    whoosh.append(0.8 * bp * math.sin(math.pi * t))
chuckle = []
for k in range(4):
    chuckle += voice(0.09, lambda t, k=k: 330 - 25 * k, vibrato=0, depth=0, breath=0.3,
                     env=lambda t: math.sin(math.pi * t)) + [0.0] * int(0.05 * RATE)
hold = whoosh[:]
start = int(0.35 * RATE)
for i, s in enumerate(chuckle):
    if start + i < len(hold):
        hold[start + i] += 0.6 * s
write("hold", hold)

n = int(0.25 * RATE)
toss, lp = [], 0.0
for i in range(n):
    t = i / n
    lp += 0.3 * (rng.uniform(-1, 1) - lp)
    toss.append(lp * math.sin(math.pi * t) * (1 - t))
write("throw", toss)
