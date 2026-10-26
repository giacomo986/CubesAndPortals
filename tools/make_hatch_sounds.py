#!/usr/bin/env python3
"""Wooden sounds for a door and a trapdoor opening and shutting, synthesised.

A shut is a knock: a short burst of noise through a resonant low body, wood
on wood. An open is a creak first -- a rough tone sliding up, the hinge --
and a softer knock at the end of the swing. The trapdoor's are a little
higher and shorter than the door's. Pure Python to a .wav, then ffmpeg to a
mono .ogg at 44.1 kHz like the others. Seeded, so a rerun writes the same
files.

    tools/make_hatch_sounds.py   # mods/doors/{open,shut}.ogg, mods/building/trapdoor_{open,shut}.ogg
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


def knock(rng, pitch, length, loud):
    """Noise struck into two decaying resonances: the board and the frame."""
    out = []
    y1 = y2 = z1 = z2 = 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        hit = rng.uniform(-1.0, 1.0) * math.exp(-t * 90.0)
        # Two-pole resonators, at the board's note and a fifth above it.
        for f, decay, st in ((pitch, 0.9975, 0), (pitch * 1.5, 0.996, 1)):
            w = 2.0 * math.pi * f / RATE
            a1, a2 = 2.0 * decay * math.cos(w), -decay * decay
            if st == 0:
                y = hit + a1 * y1 + a2 * y2
                y2, y1 = y1, y
            else:
                z = hit + a1 * z1 + a2 * z2
                z2, z1 = z1, z
        out.append((y1 * 0.06 + z1 * 0.03) * loud * math.exp(-t * 18.0))
    return out


def creak(rng, base, length):
    """A hinge: a buzzy tone sliding up, stuttering as wood catches."""
    out = []
    phase = 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        f = base * (1.0 + 0.6 * t / length) * (1.0 + 0.04 * math.sin(t * 37.0))
        phase += f / RATE
        saw = 2.0 * (phase % 1.0) - 1.0
        stutter = 0.5 + 0.5 * math.sin(t * 2.0 * math.pi * (18.0 + 6.0 * rng.random()))
        env = min(1.0, t / 0.03) * min(1.0, (length - t) / 0.08)
        out.append(saw * stutter * env * 0.18 + rng.uniform(-0.02, 0.02) * env)
    return out


def write(samples, path):
    peak = max(1e-6, max(abs(s) for s in samples))
    k = 0.8 / peak
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
        name = tmp.name
    with wave.open(name, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * k)) * 32767)) for s in samples))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", name, "-ac", "1", "-ar", str(RATE),
                    "-c:a", "libvorbis", "-q:a", "4", path], check=True)
    os.unlink(name)
    print("wrote", os.path.relpath(path, ROOT))


def sounds(pitch, creak_base, creak_len, path_open, path_shut, seed):
    rng = random.Random(seed)
    write(knock(rng, pitch, 0.35, 1.0) + [0.0] * int(0.05 * RATE), path_shut)
    swing = creak(rng, creak_base, creak_len)
    end = knock(rng, pitch * 1.1, 0.25, 0.45)
    write(swing + end, path_open)


sounds(140.0, 210.0, 0.45, os.path.join(ROOT, "mods", "doors", "open.ogg"),
       os.path.join(ROOT, "mods", "doors", "shut.ogg"), 7)
sounds(190.0, 260.0, 0.3, os.path.join(ROOT, "mods", "building", "trapdoor_open.ogg"),
       os.path.join(ROOT, "mods", "building", "trapdoor_shut.ogg"), 11)
