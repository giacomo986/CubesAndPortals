#!/usr/bin/env python3
"""The front's sounds for games/front, synthesised: the guns, the blasts, the
machines and the soldiers' voices.

A shot is a crack of noise and a thump under it; a blast a rumble that rolls
off; an engine a few low harmonics shaken by its firing; a rotor the blades'
chop. Pure Python to a .wav, then ffmpeg to a mono .ogg at 44.1 kHz, as
tools/make_imp_sounds.py. Seeded, so a rerun writes the same files.

    tools/make_front_sounds.py          # writes games/front/mods/front_game/*.ogg
"""
import math
import os
import random
import shutil
import struct
import subprocess
import tempfile
import wave

RATE = 44100
OUT = os.path.join(os.path.dirname(__file__), "..", "games", "front", "mods", "front_game")


def lowpass(samples, k):
    """One pole: k near 0 dark, near 1 bright."""
    out, y = [], 0.0
    for s in samples:
        y += k * (s - y)
        out.append(y)
    return out


def noise(length, seed):
    rnd = random.Random(seed)
    return [rnd.uniform(-1.0, 1.0) for _ in range(int(length * RATE))]


def decay(samples, attack, tau):
    out = []
    for i, s in enumerate(samples):
        t = i / RATE
        env = min(1.0, t / attack) if attack > 0 else 1.0
        out.append(s * env * math.exp(-t / tau))
    return out


def tone(length, freq, tau, seed=0):
    """A sine that falls in pitch as it dies: the thump under a shot."""
    out, phase = [], 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        phase += freq(t) / RATE
        out.append(math.sin(2 * math.pi * phase) * math.exp(-t / tau))
    return out


def mix(*parts):
    n = max(len(p) for p, _ in parts)
    out = [0.0] * n
    for p, gain in parts:
        for i, s in enumerate(p):
            out[i] += s * gain
    return out


def write(name, samples, peak=0.9):
    top = max(1e-6, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s / top * peak)) * 32767)) for s in samples)
    os.makedirs(OUT, exist_ok=True)
    with tempfile.TemporaryDirectory() as d:
        tmp = os.path.join(d, name + ".wav")
        with wave.open(tmp, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(data)
        target = os.path.join(OUT, name + ".ogg")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", tmp, "-c:a", "libvorbis", "-q:a", "4", target],
                       check=True)
    print("wrote", name)


def shot(name, length, crack, body, thump_hz, seed):
    """A gun: a bright crack, a darker body of noise, a falling thump."""
    n = noise(length, seed)
    write(name, mix((decay(lowpass(n, 0.9), 0.0005, crack), 1.0),
                    (decay(lowpass(n, 0.15), 0.002, body), 1.4),
                    (tone(length, lambda t: thump_hz * (1 - 0.6 * min(1, t / 0.1)), body * 0.8), 0.8)))


def blast(name, length, tau, k, seed):
    """An explosion: a rumble of dark noise rolling off, a deep thump."""
    n = noise(length, seed)
    rumble = decay(lowpass(lowpass(n, k), k), 0.004, tau)
    write(name, mix((rumble, 1.0), (decay(lowpass(n, 0.6), 0.001, 0.05), 0.5),
                    (tone(length, lambda t: 55 * (1 - 0.5 * min(1, t / 0.5)), tau * 0.6), 0.9)))


def engine(name, length, base, firing, rough, seed):
    """A motor: low harmonics of `base`, shaken at `firing` Hz, a little noise."""
    n = lowpass(noise(length, seed), 0.08)
    out = []
    for i in range(int(length * RATE)):
        t = i / RATE
        shake = 0.6 + 0.4 * math.sin(2 * math.pi * firing * t)
        s = sum(math.sin(2 * math.pi * base * h * t) / h for h in (1, 2, 3, 5))
        edge = min(1.0, t / 0.05, (length - t) / 0.05)
        out.append((s * shake + n[i] * rough) * edge)
    write(name, out, 0.7)


def rotor(name, length, chop, seed):
    """Blades: a thud of dark noise every 1/chop s."""
    n = lowpass(noise(length, seed), 0.12)
    out = []
    for i in range(int(length * RATE)):
        t = i / RATE
        ph = (t * chop) % 1.0
        out.append(n[i] * math.exp(-ph / 0.12) + 0.3 * math.sin(2 * math.pi * 38 * t) * math.exp(-ph / 0.2))
    write(name, out, 0.8)


def whoosh(name, length, seed):
    n = noise(length, seed)
    out = []
    y = 0.0
    for i, s in enumerate(n):
        t = i / RATE
        k = 0.05 + 0.5 * min(1.0, t / length)
        y += k * (s - y)
        out.append(y * min(1.0, t / 0.03) * math.exp(-t / (length * 0.5)))
    write(name, mix((out, 1.0), (decay(lowpass(noise(length, seed + 1), 0.4), 0.001, 0.06), 0.8)))


def grunt(name, length, f0, f1, rasp, seed):
    """A soldier's voice: a buzz that slides from f0 to f1, rough at `rasp`."""
    rnd = random.Random(seed)
    out, phase = [], 0.0
    for i in range(int(length * RATE)):
        t = i / RATE
        f = f0 + (f1 - f0) * (t / length)
        phase += f / RATE
        saw = 2.0 * (phase % 1.0) - 1.0
        trem = 0.7 + 0.3 * math.sin(2 * math.pi * rasp * t)
        env = min(1.0, t / 0.03) * min(1.0, (length - t) / 0.12)
        out.append((saw * trem + rnd.uniform(-0.3, 0.3)) * env)
    write(name, lowpass(lowpass(out, 0.25), 0.35), 0.7)


if __name__ == "__main__":
    shot("pistol", 0.35, 0.02, 0.06, 180, 1)
    shot("rifle", 0.4, 0.025, 0.08, 140, 2)
    shot("lmg", 0.45, 0.03, 0.1, 110, 3)
    shot("mg", 0.45, 0.03, 0.11, 100, 4)
    shot("cannon", 1.6, 0.05, 0.45, 60, 5)
    blast("boom", 2.4, 0.6, 0.08, 6)
    blast("boom_small", 1.4, 0.3, 0.12, 7)
    whoosh("rocket", 0.9, 8)
    shot("mortar", 1.0, 0.03, 0.25, 70, 9)
    engine("engine_jeep", 0.6, 48, 22, 0.4, 10)
    engine("engine_tank", 0.6, 32, 14, 0.8, 11)
    engine("engine_plane", 0.4, 90, 45, 0.3, 12)
    rotor("rotor", 0.6, 10, 13)
    grunt("soldier_red_say", 0.35, 140, 120, 30, 14)
    grunt("soldier_red_hurt", 0.3, 220, 160, 45, 15)
    grunt("soldier_red_death", 0.8, 200, 80, 25, 16)
    # Blue's voices are red's: the engine looks each kind's up by its name.
    for what in ("say", "hurt", "death"):
        shutil.copyfile(os.path.join(OUT, f"soldier_red_{what}.ogg"), os.path.join(OUT, f"soldier_blue_{what}.ogg"))
    print("front sounds written to", os.path.normpath(OUT))
