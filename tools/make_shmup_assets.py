#!/usr/bin/env python3
"""The pictures and the sounds of games/shmup, written rather than drawn or
recorded. (Its planes are tools/make_shmup_models.py's.)

Pictures (32 px, games/shmup/textures/shmup_core_<block>.png): the sea, the
foam round an island, its sand and its green, the carrier's deck, the line
down it and its hull, the doorway's frame and lamp. Seeded.

Sounds (games/shmup/mods/shmup/<name>.ogg), as an arcade board made them, pure
Python to a .wav and ffmpeg to a mono .ogg as tools/make_weather_sounds.py:
shot (the fighter's gun), foe_shot, boom (a plane down), big_boom (the
bomber), hit (a life lost), start, wave and over.

    tools/make_shmup_assets.py
"""
import math
import os
import random
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402
from make_platformer_assets import tone, mix  # noqa: E402

TEX = os.path.join(ROOT, "games", "shmup", "textures")
SND = os.path.join(ROOT, "games", "shmup", "mods", "shmup")
N = 32
rng = random.Random(1942)


def noisy(base, spread):
    img = Image.new("RGB", (N, N))
    px = img.load()
    for y in range(N):
        for x in range(N):
            d = rng.randint(-spread, spread)
            px[x, y] = tuple(max(0, min(255, c + d)) for c in base)
    return img


def save(img, name):
    img.save(os.path.join(TEX, "shmup_core_" + name + ".png"))


def textures():
    os.makedirs(TEX, exist_ok=True)

    # The sea: no grid from overhead -- a few short crests, and the picture
    # tiles on every side.
    sea = noisy((30, 96, 190), 4)
    d = ImageDraw.Draw(sea)
    for _ in range(5):
        x, y, w = rng.randint(0, N - 1), rng.randint(0, N - 1), rng.randint(3, 7)
        for k in range(w):
            sea.putpixel(((x + k) % N, (y + (1 if k in (0, w - 1) else 0)) % N), (70, 140, 220))
    save(sea, "sea")
    foam = noisy((150, 200, 235), 8)
    d = ImageDraw.Draw(foam)
    for _ in range(10):
        x, y = rng.randint(0, N - 3), rng.randint(0, N - 2)
        d.rectangle([x, y, x + 2, y + 1], fill=(232, 244, 252))
    save(foam, "foam")
    save(noisy((222, 200, 130), 7), "sand")
    green = noisy((60, 150, 70), 8)
    d = ImageDraw.Draw(green)
    for _ in range(8):
        x, y = rng.randint(1, N - 5), rng.randint(1, N - 5)
        d.ellipse([x, y, x + 3, y + 3], fill=(40, 118, 52))
    save(green, "green")

    deck = noisy((96, 100, 108), 4)
    d = ImageDraw.Draw(deck)
    d.line([(0, 0), (N - 1, 0)], fill=(74, 78, 86))
    d.line([(0, 0), (0, N - 1)], fill=(74, 78, 86))
    save(deck, "deck")
    line = deck.copy()
    d = ImageDraw.Draw(line)
    d.rectangle([12, 0, 19, N - 1], fill=(240, 220, 60))
    save(line, "line")
    hull = noisy((60, 64, 72), 4)
    d = ImageDraw.Draw(hull)
    for y in range(7, N, 8):
        d.line([(0, y), (N - 1, y)], fill=(44, 48, 56))
        for x in range(3, N, 8):
            hull.putpixel((x, y - 3), (90, 94, 102))
    save(hull, "hull")
    save(noisy((34, 34, 36), 3), "frame")
    lamp = noisy((255, 240, 200), 3)
    ImageDraw.Draw(lamp).rectangle([0, 0, N - 1, N - 1], outline=(200, 180, 130))
    save(lamp, "lamp")


def burst(seconds, vol, decay, low, rumble=0.0, seed=3):
    """Noise through a low-pass that closes as it fades, over a falling thump."""
    r = random.Random(seed)
    out, lp, phase = [], 0.0, 0.0
    for i in range(int(seconds * RATE)):
        u = i / RATE
        k = low * math.exp(-u * 3.0) + 0.02
        lp += (r.uniform(-1, 1) - lp) * k
        phase += (90.0 * math.exp(-u * 6.0) + 35.0) / RATE
        thump = math.sin(2 * math.pi * phase) * rumble
        out.append((lp * 1.6 + thump) * math.exp(-u * decay) * vol * min(1.0, (seconds - u) / 0.02))
    return out


def sounds():
    os.makedirs(SND, exist_ok=True)
    out = lambda samples, name: write(samples, os.path.join(SND, name + ".ogg"))  # noqa: E731
    out(tone([(0.09, (1500, 520))], vol=0.10, duty=0.25, decay=14.0), "shot")
    out(tone([(0.16, (420, 240))], vol=0.10, duty=0.5, decay=8.0), "foe_shot")
    out(burst(0.5, 0.5, 6.0, 0.5, 0.5), "boom")
    out(burst(1.3, 0.6, 2.6, 0.35, 0.8, seed=5), "big_boom")
    out(mix(tone([(0.35, (700, 110))], vol=0.2, duty=0.5, decay=4.0), burst(0.4, 0.4, 7.0, 0.6, 0.3, seed=9)), "hit")
    out(tone([(0.09, 392), (0.09, 523), (0.09, 659), (0.09, 784), (0.4, 1047)], vol=0.15, decay=3.5), "start")
    out(tone([(0.08, 784), (0.08, 988), (0.3, 1175)], vol=0.13, decay=5.0), "wave")
    out(tone([(0.3, 330), (0.3, 277), (0.3, 233), (0.9, 196)], vol=0.17, duty=0.35, decay=1.8), "over")


if __name__ == "__main__":
    textures()
    sounds()
    print("shmup: pictures in", os.path.normpath(TEX), "and sounds in", os.path.normpath(SND))
