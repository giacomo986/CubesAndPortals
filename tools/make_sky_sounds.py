#!/usr/bin/env python3
"""The sky islands' own sounds (games/skies/mods/skies). Pure Python to a .wav,
then ffmpeg to a mono .ogg at 22050 Hz, as tools/make_imp_sounds.py does.

    tools/make_sky_sounds.py

    gust          the Manta's breath: a rush of filtered noise rising and dying, 1.4 s
    manta_say     a low hollow call, two falling tones under wind, 1.6 s
    manta_hurt    a short bark of the same, 0.5 s
    manta_death   a long falling moan that fades into wind, 2.5 s
    squall_say    a little crackle and hiss, 0.6 s
    squall_hurt   a pop of static, 0.3 s
    squall_death  a crackle that fizzles out, 0.8 s

The thunder is mods/weather's, linked beside them as thunder.ogg.
"""
import math
import os
import random
import struct
import subprocess
import tempfile
import wave

RATE = 22050
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "games", "skies", "mods", "skies")
rng = random.Random(7)


def noise(n, smooth):
    """White noise through a one-pole low pass: smooth near 1 is darker."""
    out, y = [], 0.0
    for _ in range(n):
        y = smooth * y + (1 - smooth) * rng.uniform(-1, 1)
        out.append(y)
    peak = max(abs(v) for v in out) or 1.0
    return [v / peak for v in out]


def env(i, n, attack, release):
    t = i / RATE
    length = n / RATE
    a = min(1.0, t / attack) if attack > 0 else 1.0
    r = min(1.0, (length - t) / release) if release > 0 else 1.0
    return max(0.0, min(a, r))


def write(name, samples):
    peak = max(abs(v) for v in samples) or 1.0
    with tempfile.TemporaryDirectory() as tmp:
        wav = os.path.join(tmp, name + ".wav")
        with wave.open(wav, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(b"".join(struct.pack("<h", int(v / peak * 0.85 * 32767)) for v in samples))
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-ac", "1", "-ar", str(RATE),
                        "-c:a", "libvorbis", "-q:a", "3", os.path.join(OUT, name + ".ogg")], check=True)
    print("sky sound:", name)


def gust():
    n = int(1.4 * RATE)
    a, b = noise(n, 0.97), noise(n, 0.85)
    return [(a[i] * 0.7 + b[i] * 0.3 * math.sin(math.pi * i / n)) * env(i, n, 0.5, 0.7) for i in range(n)]


def call(seconds, f0, f1, wind, release):
    n = int(seconds * RATE)
    w = noise(n, 0.96)
    out, ph = [], 0.0
    for i in range(n):
        f = f0 + (f1 - f0) * i / n
        ph += 2 * math.pi * f / RATE
        tone = math.sin(ph) + 0.4 * math.sin(2 * ph) + 0.2 * math.sin(3.01 * ph)
        out.append((tone * 0.6 + w[i] * wind) * env(i, n, 0.08, release))
    return out


def crackle(seconds, density, release):
    n = int(seconds * RATE)
    h = noise(n, 0.3)
    out = []
    for i in range(n):
        spark = rng.uniform(-1, 1) if rng.random() < density else 0.0
        out.append((h[i] * 0.25 + spark) * env(i, n, 0.01, release))
    return out


def main():
    os.makedirs(OUT, exist_ok=True)
    write("gust", gust())
    write("manta_say", call(1.6, 140, 90, 0.35, 0.6))
    write("manta_hurt", call(0.5, 180, 120, 0.2, 0.2))
    write("manta_death", call(2.5, 160, 50, 0.5, 1.5))
    write("squall_say", crackle(0.6, 0.02, 0.3))
    write("squall_hurt", crackle(0.3, 0.08, 0.15))
    write("squall_death", crackle(0.8, 0.05, 0.6))
    link = os.path.join(OUT, "thunder.ogg")
    if os.path.lexists(link):
        os.remove(link)
    os.symlink("../../../../mods/weather/thunder.ogg", link)
    print("sky sound: thunder (a link to mods/weather's)")


if __name__ == "__main__":
    main()
