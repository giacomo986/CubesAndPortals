#!/usr/bin/env python3
"""Thunder for mods/weather, synthesised.

A strike is a crack -- white noise, bright and short -- over a rumble: brown
noise (integrated white) through a low-pass, swelling in a few bursts as the
sound comes back off the hills, and fading over four seconds. Pure Python to a
.wav, then ffmpeg to a mono .ogg at 44.1 kHz like the others. Seeded.

    tools/make_weather_sounds.py   # mods/weather/thunder.ogg
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


def thunder(rng, length):
    out = []
    brown = lp1 = lp2 = 0.0
    # Bursts of the rumble: when, how loud, how quick to fade.
    bursts = [(0.0, 1.0, 2.5)] + [(rng.uniform(0.3, 2.2), rng.uniform(0.4, 0.8), rng.uniform(2.0, 4.0))
                                  for _ in range(4)]
    for i in range(int(length * RATE)):
        t = i / RATE
        white = rng.uniform(-1.0, 1.0)
        brown = (brown + white * 0.02) * 0.999
        lp1 += (brown - lp1) * 0.05
        lp2 += (lp1 - lp2) * 0.05
        swell = sum(a * min(1.0, (t - s) / 0.08) * math.exp(-(t - s) * d) for s, a, d in bursts if t >= s)
        crack = white * math.exp(-t * 22.0) * 0.5
        tail = min(1.0, (length - t) / 0.5)
        out.append((crack + lp2 * 9.0 * swell) * tail)
    return out


def write(samples, path):
    peak = max(1e-6, max(abs(s) for s in samples))
    k = 0.85 / peak
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


if __name__ == "__main__":
    write(thunder(random.Random(7), 4.5), os.path.join(ROOT, "mods", "weather", "thunder.ogg"))
