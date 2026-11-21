#!/usr/bin/env python3
"""The loops the world keeps going round (core.sound_loop), synthesised.

Lava bubbling, a kart's engine, the balloon's burner and a short tune for each
kind of land. All of them go round, so every one is made to have no seam:

- What is a sum of tones and strikes is written into a RING -- a sample past
  the end lands at the start -- and each tone has a whole number of cycles in
  the loop, so the end runs into the beginning as if it went on.
- What is noise (lava's rumble, the burner's roar) is made a second too long
  and that second faded into the start, equal power, as the rain is in
  tools/make_weather_sounds.py.

Pure Python to a .wav, then ffmpeg to a mono .ogg at 44.1 kHz like the others.
Seeded, so running it again writes the same files.

    tools/make_loop_sounds.py   # mods/rocks/lava.ogg, track_core/engine.ogg,
                                # mods/balloon/burner.ogg, mods/music/*.ogg

The engine is one sound in three mods: games/track's track_core, Inferno's
inferno_track and mods/rewards each get their own copy, as a sound a mod
offers is a file in it (a link out of the mod is refused, only .luau may be).
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
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TAU = 2.0 * math.pi


def whole(freq, length):
    """The nearest frequency with a whole number of cycles in `length` s."""
    return max(1, round(freq * length)) / length


def ring_add(ring, start, samples):
    n = len(ring)
    for i, v in enumerate(samples):
        ring[(start + i) % n] += v


def crossfaded(make, length, fade=1.0):
    """`make(count)` samples of something with no cycle, made to go round:
    the second past the end faded into the start."""
    n, f = int(length * RATE), int(fade * RATE)
    body = make(n + f)
    out = body[:n]
    for i in range(f):
        a = i / f
        out[i] = body[i] * math.sin(a * math.pi / 2) + body[n + i] * math.cos(a * math.pi / 2)
    return out


# ------------------------------------------------------------------ lava

def lava(rng, length=6.0):
    """A low rumble that moves, under bubbles: each a blub that rises in pitch
    as it bursts, a few a second, some big and slow."""
    def rumble(count):
        out = []
        brown = lp = lp2 = 0.0
        for i in range(count):
            white = rng.uniform(-1.0, 1.0)
            brown = (brown + white * 0.02) * 0.998
            lp += (brown - lp) * 0.02
            lp2 += (lp - lp2) * 0.02
            # a slow swell, a few seconds round, so it heaves rather than hums
            out.append(lp2 * 14.0 * (0.75 + 0.25 * math.sin(TAU * i / RATE * 0.37)))
        return out
    ring = crossfaded(rumble, length)
    t = 0.0
    while t < length:
        big = rng.random() < 0.25
        dur = rng.uniform(0.12, 0.22) if big else rng.uniform(0.05, 0.1)
        f0 = rng.uniform(60, 90) if big else rng.uniform(110, 200)
        amp = rng.uniform(0.5, 0.8) if big else rng.uniform(0.2, 0.45)
        blub, phase = [], 0.0
        for i in range(int(dur * RATE)):
            u = i / (dur * RATE)
            phase += TAU * f0 * (1.0 + 1.6 * u * u) / RATE
            env = math.sin(math.pi * min(1.0, u * 4.0)) if u < 0.25 else (1.0 - u) ** 2
            blub.append(math.sin(phase) * env * amp)
        # and the pop at the end, a click of noise
        blub += [rng.uniform(-1, 1) * amp * 0.5 * math.exp(-i / 90.0) for i in range(400)]
        ring_add(ring, int(t * RATE), blub)
        t += rng.uniform(0.08, 0.45)
    return ring


# ---------------------------------------------------------------- engine

def engine(rng, length=2.0):
    """A small petrol engine ticking over briskly: a firing every 1/30 s, each
    a thump at the exhaust's note and a breath of noise, over the hum of its
    harmonics. Everything whole in the loop, so it has no seam."""
    n = int(length * RATE)
    ring = [0.0] * n
    f0 = whole(60.0, length)
    for k, a in ((1, 0.45), (2, 0.3), (3, 0.18), (4, 0.1), (6, 0.05)):
        f = f0 * k
        ph = rng.uniform(0, TAU)
        for i in range(n):
            ring[i] += a * math.sin(TAU * f * i / RATE + ph)
    fire = whole(30.0, length)
    count = round(fire * length)
    for j in range(count):
        start = int(j * n / count) + rng.randint(-20, 20)
        amp = rng.uniform(0.7, 1.0)
        thump = []
        for i in range(int(0.03 * RATE)):
            e = math.exp(-i / (0.006 * RATE))
            thump.append((math.sin(TAU * 95.0 * i / RATE) * 0.8 + rng.uniform(-1, 1) * 0.35) * e * amp)
        ring_add(ring, start, thump)
    # the top taken off, round the ring twice so its start is settled too
    lp = 0.0
    for _ in range(2):
        for i in range(n):
            lp += (ring[i] - lp) * 0.25
            ring[i] = lp
    return ring


# ---------------------------------------------------------------- burner

def burner(rng, length=4.0):
    """A propane burner's roar: noise with its top and its bottom taken off,
    a flutter in it, and a low rumble of the flame under."""
    def roar(count):
        out = []
        lp = hp = hp_in = brown = blp = 0.0
        for i in range(count):
            white = rng.uniform(-1.0, 1.0)
            lp += (white - lp) * 0.22
            hp = 0.985 * (hp + lp - hp_in)
            hp_in = lp
            brown = (brown + white * 0.03) * 0.997
            blp += (brown - blp) * 0.03
            t = i / RATE
            flutter = 0.85 + 0.1 * math.sin(TAU * 7.3 * t) + 0.05 * math.sin(TAU * 13.1 * t)
            out.append((hp * 0.9 + blp * 6.0) * flutter)
        return out
    return crossfaded(roar, length)


# ----------------------------------------------------------------- music

NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8,
        "A": 9, "A#": 10, "B": 11}


def hz(name, octave):
    return 440.0 * 2.0 ** ((NOTE[name] + 12 * (octave - 4) - 9) / 12.0)


def tone(freq, dur, partials, attack, release, length):
    """One note, `partials` (multiple, amplitude) over a whole-cycle base."""
    f = whole(freq, length)
    out = []
    count = int(dur * RATE)
    for i in range(count):
        t = i / RATE
        env = min(1.0, t / attack) if attack > 0 else 1.0
        if release > 0 and dur - t < release:
            env *= max(0.0, (dur - t) / release)
        out.append(env * sum(a * math.sin(TAU * f * m * t) for m, a in partials))
    return out


def pluck(freq, dur, bright, length):
    f = whole(freq, length)
    out = []
    for i in range(int(dur * RATE)):
        t = i / RATE
        env = min(1.0, t / 0.03) * math.exp(-t * 3.0)   # a soft finger: a hard one is a click
        out.append(env * (math.sin(TAU * f * t) + bright * math.exp(-t * 6.0) * math.sin(TAU * 2 * f * t)))
    return out


def bell(freq, dur, length):
    # A bell's partials are not whole multiples, which is what makes it a bell.
    f = whole(freq, length)
    out = []
    parts = ((1.0, 1.0, 1.2), (2.76, 0.3, 2.0), (5.4, 0.1, 3.5))
    for i in range(int(dur * RATE)):
        t = i / RATE
        att = min(1.0, t / 0.02)
        out.append(att * sum(a * math.exp(-t * d) * math.sin(TAU * whole(f * m, length) * t) for m, a, d in parts))
    return out


def echo(ring, delay, feedback, passes=4):
    """A delay round the ring: what echoes past the end comes back at the
    start, so the echo of the last note is heard over the first."""
    n, d = len(ring), int(delay * RATE)
    dry = ring[:]
    wet = [0.0] * n
    for _ in range(passes):
        src = [dry[i] + wet[i] for i in range(n)]
        wet = [src[(i - d) % n] * feedback for i in range(n)]
    return [dry[i] + wet[i] for i in range(n)]


def tune(rng, length, chords, scale, octave, voice, pad_level, notes_per_chord, sea=False):
    n = int(length * RATE)
    ring = [0.0] * n
    bar = length / len(chords)
    pad_partials = ((1, 1.0), (2, 0.35), (3, 0.12))
    for c, chord in enumerate(chords):
        start = c * bar - 0.6
        for name, octv in chord:
            note = tone(hz(name, octv), bar + 1.2, pad_partials, 0.9, 1.1, length)
            ring_add(ring, int(start * RATE) % n, [v * pad_level for v in note])
        # the tune over it: a few notes of the scale, nearer the chord's
        for k in range(notes_per_chord):
            if rng.random() < 0.2:
                continue
            at = c * bar + k * bar / notes_per_chord + rng.uniform(-0.03, 0.03)
            name = rng.choice(scale)
            octv = octave
            if voice == "bell":
                note = bell(hz(name, octv), 2.5, length)
            else:
                note = pluck(hz(name, octv), 1.6, 0.6 if voice == "pluck" else 0.25, length)
            ring_add(ring, int(at * RATE) % n, [v * 0.32 for v in note])
    ring = echo(ring, bar / notes_per_chord * 1.5, 0.3)
    if sea:
        # And waves under it: noise swelling and falling in a whole number of
        # breaths round the loop.
        breaths = 4

        def waves(count):
            out, lp, lp2 = [], 0.0, 0.0
            for i in range(count):
                white = rng.uniform(-1, 1)
                lp += (white - lp) * 0.08
                lp2 += (lp - lp2) * 0.08
                swell = 0.5 - 0.5 * math.cos(TAU * breaths * i / n)
                out.append(lp2 * 1.6 * swell ** 2)
            return out
        w = crossfaded(waves, length)
        ring = [a + b for a, b in zip(ring, w)]
    return ring


def music(rng):
    out = {}
    # Plains: major, warm, a harp.
    out["plains"] = tune(rng, 16.0,
                         [[("C", 3), ("E", 3), ("G", 3)], [("A", 2), ("C", 3), ("E", 3)],
                          [("F", 2), ("A", 2), ("C", 3)], [("G", 2), ("B", 2), ("D", 3)]],
                         ["C", "D", "E", "G", "A"], 4, "pluck", 0.16, 4)
    # Desert: a drone under a scale with a step and a half in it, slow.
    out["desert"] = tune(rng, 16.0,
                         [[("E", 2), ("B", 2), ("E", 3)], [("E", 2), ("B", 2), ("F", 3)],
                          [("E", 2), ("B", 2), ("E", 3)], [("D", 2), ("A", 2), ("D", 3)]],
                         ["E", "F", "G#", "A", "B", "C"], 4, "soft", 0.17, 3)
    # The cold (tundra, peaks): minor, sparse, bells high up.
    out["cold"] = tune(rng, 16.0,
                       [[("A", 2), ("E", 3), ("C", 4)], [("F", 2), ("C", 3), ("A", 3)],
                        [("D", 2), ("A", 2), ("F", 3)], [("E", 2), ("B", 2), ("G#", 3)]],
                       ["A", "C", "D", "E", "G"], 4, "bell", 0.12, 2)
    # By the water: open fifths, the waves under them.
    out["sea"] = tune(rng, 16.0,
                      [[("D", 2), ("A", 2), ("F#", 3)], [("G", 2), ("D", 3), ("B", 3)],
                       [("D", 2), ("A", 2), ("E", 3)], [("A", 2), ("E", 3), ("C#", 4)]],
                      ["D", "E", "F#", "A", "B"], 4, "soft", 0.14, 2, sea=True)
    return out


def write(samples, path, level=0.85, quality="4"):
    peak = max(1e-6, max(abs(s) for s in samples))
    k = level / peak
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
        name = tmp.name
    with wave.open(name, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * k)) * 32767)) for s in samples))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", name, "-ac", "1", "-ar", str(RATE),
                    "-c:a", "libvorbis", "-q:a", quality, path], check=True)
    os.unlink(name)
    print("wrote", os.path.relpath(path, ROOT))


if __name__ == "__main__":
    write(lava(random.Random(23)), os.path.join(ROOT, "mods", "rocks", "lava.ogg"))
    first = os.path.join(ROOT, "games", "track", "mods", "track_core", "engine.ogg")
    write(engine(random.Random(31)), first)
    for other in (("games", "inferno", "mods", "inferno_track"), ("mods", "rewards")):
        copy = os.path.join(ROOT, *other, "engine.ogg")
        if os.path.lexists(copy):
            os.unlink(copy)
        shutil.copyfile(first, copy)
        print("wrote", os.path.relpath(copy, ROOT))
    write(burner(random.Random(41)), os.path.join(ROOT, "mods", "balloon", "burner.ogg"))
    for biome, samples in music(random.Random(53)).items():
        write(samples, os.path.join(ROOT, "mods", "music", biome + ".ogg"), level=0.7, quality="3")
