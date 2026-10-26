#!/usr/bin/env python3
"""The imp's voice for games/arpg: say, hurt, death and shoot, synthesised.

A small fiery demon: a rasping growl (a sawtooth through tremolo and noise),
a screech for a wound and a falling one for its death, and a whoosh for the
fireball it throws. Pure Python to a .wav, then ffmpeg to a mono .ogg at
44.1 kHz like mods/mobs. Seeded, so a rerun writes the same files.

    tools/make_imp_sounds.py            # writes games/arpg/mods/arpg_core/imp_*.ogg
"""
import math
import os
import random
import struct
import subprocess
import sys
import tempfile
import wave

RATE = 44100
OUT = os.path.join(os.path.dirname(__file__), "..", "games", "arpg", "mods", "arpg_core")


def envelope(t, length, attack, release):
    if t < attack:
        return t / attack
    if t > length - release:
        return max(0.0, (length - t) / release)
    return 1.0


def voice(length, pitch, rasp, noise, attack=0.02, release=0.15, seed=1):
    """A growl or a screech: a sawtooth whose pitch follows pitch(t), shaken
    by a tremolo of `rasp` Hz, with `noise` of hiss under it."""
    rnd = random.Random(seed)
    out, phase, low = [], 0.0, 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        phase += pitch(t) / RATE
        saw = 2.0 * (phase % 1.0) - 1.0
        trem = 0.6 + 0.4 * math.sin(2 * math.pi * rasp * t)
        hiss = rnd.uniform(-1, 1)
        low += 0.25 * (hiss - low)           # a softer noise, under the voice
        s = (saw * trem * (1 - noise) + low * noise) * envelope(t, length, attack, release)
        out.append(s)
    return out


def whoosh(length, seed=2):
    """Noise through a low-pass that opens and closes: fire going past."""
    rnd = random.Random(seed)
    out, y = [], 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        k = 0.02 + 0.25 * math.sin(math.pi * t / length) ** 2
        y += k * (rnd.uniform(-1, 1) - y)
        out.append(y * 3.0 * envelope(t, length, 0.05, 0.25))
    return out


def write(name, samples):
    peak = max(1e-6, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", int(32767 * 0.8 * s / peak)) for s in samples)
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
        tmp = f.name
    with wave.open(tmp, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)
    target = os.path.join(OUT, f"imp_{name}.ogg")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", tmp, "-c:a", "libvorbis", "-q:a", "4", target],
                   check=True)
    os.unlink(tmp)
    print(f"{target}: {len(samples) / RATE:.2f} s")


def main():
    # A growl that rises and falls, twice.
    write("say", voice(0.9, lambda t: 140 + 50 * math.sin(2 * math.pi * 2.2 * t), 23, 0.35, seed=3))
    # A screech, up and quickly gone.
    write("hurt", voice(0.35, lambda t: 420 + 500 * t, 31, 0.3, attack=0.005, release=0.12, seed=4))
    # The same falling away into a rattle.
    write("death", voice(1.1, lambda t: max(70.0, 520 - 420 * t), 17, 0.45, release=0.4, seed=5))
    # The fireball leaving the hand.
    write("shoot", whoosh(0.6))
    return 0


if __name__ == "__main__":
    sys.exit(main())
