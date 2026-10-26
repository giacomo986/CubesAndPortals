#!/usr/bin/env python3
# The radio in the backrooms' Level 1 (games/backrooms/mods/backrooms_exit):
# 1.2 s of static with two short beeps in it, synthesised, written as mono
# .ogg with ffmpeg. Fixed seed. And l'Ospite's breath for the laboratory.
#
#   python3 tools/make_radio_sound.py
import math
import os
import random
import struct
import subprocess
import tempfile
import wave

RATE = 22050
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "games", "backrooms", "mods",
                   "backrooms_exit", "radio.ogg")

rnd = random.Random(7)
n = int(RATE * 1.2)
samples = []
low = 0.0
for i in range(n):
    t = i / RATE
    # Static: filtered noise, swelling and fading.
    low += 0.35 * (rnd.uniform(-1, 1) - low)
    env = math.sin(math.pi * t / 1.2) ** 0.5
    s = 0.25 * low * env
    # Two beeps, 1 kHz, a little wavering.
    for start in (0.3, 0.55):
        if start <= t < start + 0.12:
            k = (t - start) / 0.12
            s += 0.35 * math.sin(2 * math.pi * (1000 + 30 * math.sin(40 * t)) * t) * math.sin(math.pi * k)
    samples.append(max(-1.0, min(1.0, s)))

with tempfile.TemporaryDirectory() as tmp:
    wav = os.path.join(tmp, "radio.wav")
    with wave.open(wav, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(s * 32767)) for s in samples))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-c:a", "libvorbis", "-q:a", "3", OUT],
                   check=True)
print("radio written to", os.path.normpath(OUT))

# And l'Ospite's breath in the laboratory's dark (games/lab, mods/lab_core):
# 1.6 s, a low rumble rising and falling, a rattle in it.
BREATH = os.path.join(os.path.dirname(OUT), "..", "..", "..", "lab", "mods", "lab_core", "ospite_breath.ogg")
n = int(RATE * 1.6)
samples = []
low = 0.0
for i in range(n):
    t = i / RATE
    low += 0.05 * (rnd.uniform(-1, 1) - low)
    env = math.sin(math.pi * t / 1.6) ** 1.5
    s = (0.5 * math.sin(2 * math.pi * 55 * t) + 0.3 * math.sin(2 * math.pi * 82 * t + math.sin(9 * t))) * env
    s += 2.5 * low * env * (0.5 + 0.5 * math.sin(2 * math.pi * 17 * t))
    samples.append(max(-1.0, min(1.0, s * 0.8)))
with tempfile.TemporaryDirectory() as tmp:
    wav = os.path.join(tmp, "breath.wav")
    with wave.open(wav, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(s * 32767)) for s in samples))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-c:a", "libvorbis", "-q:a", "3", BREATH],
                   check=True)
print("breath written to", os.path.normpath(BREATH))
