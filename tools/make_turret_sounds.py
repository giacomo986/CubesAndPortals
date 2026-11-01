#!/usr/bin/env python3
"""The sounds of games/chambers' turrets and dissolving fields, synthesised.

  turret_shot    a short dry pop: noise through a low resonance, quick decay
  turret_spot    two rising beeps, the turret having seen somebody
  turret_tip     a falling chirp, the turret knocked over and switched off
  field_fizzle   a sizzle: high noise swelling and dying, a portal dissolved

Pure Python to a .wav, then ffmpeg to a mono .ogg at 44.1 kHz. Seeded, so a
rerun writes the same files.

    tools/make_turret_sounds.py   # games/chambers/mods/chambers_game/*.ogg
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
OUT = os.path.join(ROOT, "games", "chambers", "mods", "chambers_game")


def resonate(source, f, decay):
    w = 2.0 * math.pi * f / RATE
    a1, a2 = 2.0 * decay * math.cos(w), -decay * decay
    y1 = y2 = 0.0
    out = []
    for x in source:
        y = x + a1 * y1 + a2 * y2
        y2, y1 = y1, y
        out.append(y)
    return out


def tone(f0, f1, length, loud=0.4):
    out, ph = [], 0.0
    n = int(length * RATE)
    for i in range(n):
        t = i / n
        f = f0 + (f1 - f0) * t
        ph += 2 * math.pi * f / RATE
        env = min(1.0, i / 200) * min(1.0, (n - i) / 400)
        # a square-ish beep: the robot's voice
        out.append(loud * env * math.tanh(3 * math.sin(ph)))
    return out


def write(name, samples):
    peak = max(1e-6, max(abs(s) for s in samples))
    k = 0.9 / peak if peak > 0.9 else 1.0
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


rng = random.Random(12)

noise = [rng.uniform(-1, 1) * math.exp(-i / RATE * 60) for i in range(int(0.12 * RATE))]
shot = [a * 0.03 + b * 0.5 for a, b in zip(resonate(noise, 180, 0.995), noise)]
write("turret_shot", shot)

write("turret_spot", tone(880, 880, 0.08) + [0.0] * 1500 + tone(1320, 1320, 0.12))

write("turret_tip", tone(1200, 300, 0.45, 0.35))

n = int(0.6 * RATE)
fizz = []
prev = 0.0
for i in range(n):
    x = rng.uniform(-1, 1)
    hp = x - prev   # bright: only the hiss
    prev = x
    t = i / n
    fizz.append(0.5 * hp * math.sin(math.pi * t) ** 0.7 * (1 + 0.5 * math.sin(i / RATE * 2 * math.pi * 31)))
write("field_fizzle", fizz)
