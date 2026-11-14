#!/usr/bin/env python3
"""The pictures and the sounds of games/platformer, written rather than drawn
or recorded.

Pictures (32 px, games/platformer/textures/platformer_<block>.png): grass over
earth, bricks (and 1-2's blue ones, and the dark wall behind them), the prize block and what is left of it once bumped, the pipe,
stone, the room's wall, floor and lamp, the pole and its two flags, a cloud, a
hill, the coin. Seeded.

Sounds (games/platformer/mods/platformer/<name>.ogg), square waves as the
eighties' consoles made them, pure Python to a .wav and ffmpeg to a mono .ogg
as tools/make_weather_sounds.py: jump, coin, bump (a prize block), stomp (a
creature landed on), hurt (a life lost), flag (half way), pipe (through one), power_up (one coming out of its block), grow,
shrink, fire (a fireball thrown), win and over.

    tools/make_platformer_assets.py
"""
import math
import os
import random
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_weather_sounds import ROOT, RATE, write  # noqa: E402

TEX = os.path.join(ROOT, "games", "platformer", "textures")
SND = os.path.join(ROOT, "games", "platformer", "mods", "platformer")
N = 32
rng = random.Random(1985)


def noisy(base, spread):
    img = Image.new("RGB", (N, N))
    px = img.load()
    for y in range(N):
        for x in range(N):
            d = rng.randint(-spread, spread)
            px[x, y] = tuple(max(0, min(255, c + d)) for c in base)
    return img


def save(img, name):
    img.save(os.path.join(TEX, "platformer_" + name + ".png"))


def bricks(base, mortar, rows=4):
    img = noisy(base, 5)
    d = ImageDraw.Draw(img)
    h = N // rows
    for r in range(rows):
        d.line([(0, r * h), (N - 1, r * h)], fill=mortar)
        for x in range((N // 4 if r % 2 else 0), N, N // 2):
            d.line([(x, r * h), (x, r * h + h - 1)], fill=mortar)
        d.line([(0, r * h + 1), (N - 1, r * h + 1)], fill=tuple(min(255, c + 22) for c in base))
    return img


def bevel(img, light, dark):
    d = ImageDraw.Draw(img)
    d.line([(0, 0), (N - 1, 0)], fill=light)
    d.line([(0, 0), (0, N - 1)], fill=light)
    d.line([(0, N - 1), (N - 1, N - 1)], fill=dark)
    d.line([(N - 1, 0), (N - 1, N - 1)], fill=dark)
    return img


def textures():
    os.makedirs(TEX, exist_ok=True)

    # Earth, and grass: a top of its own and a fringe hanging over the side.
    dirt = noisy((150, 92, 50), 8)
    d = ImageDraw.Draw(dirt)
    for _ in range(14):
        x, y = rng.randint(0, N - 2), rng.randint(0, N - 2)
        d.rectangle([x, y, x + 1, y + 1], fill=(122, 72, 38))
    save(dirt, "dirt")
    save(dirt, "grass_bottom")
    save(noisy((96, 176, 64), 10), "grass_top")
    side = dirt.copy()
    d = ImageDraw.Draw(side)
    for x in range(N):
        depth = 5 + (x * 7 % 5) + rng.randint(0, 2)
        for y in range(depth):
            g = rng.randint(-10, 10)
            side.putpixel((x, y), (96 + g, 176 + g, 64 + g))
    save(side, "grass_side")

    save(bricks((190, 92, 44), (92, 40, 20)), "brick")
    save(bricks((52, 96, 170), (18, 36, 80)), "blue_brick")
    save(bricks((40, 54, 104), (24, 32, 66), rows=2), "cave")

    # The prize block: gold, a rivet in each corner, a question mark.
    prize = bevel(noisy((244, 196, 40), 4), (255, 236, 150), (150, 100, 10))
    d = ImageDraw.Draw(prize)
    for x, y in ((3, 3), (N - 5, 3), (3, N - 5), (N - 5, N - 5)):
        d.rectangle([x, y, x + 1, y + 1], fill=(120, 80, 10))
    q = (255, 255, 255)
    d.rectangle([11, 7, 20, 9], fill=q)
    d.rectangle([19, 9, 21, 14], fill=q)
    d.rectangle([14, 14, 20, 16], fill=q)
    d.rectangle([14, 16, 16, 19], fill=q)
    d.rectangle([14, 22, 16, 24], fill=q)
    save(prize, "prize")
    spent = bevel(noisy((150, 104, 60), 4), (190, 146, 100), (86, 54, 26))
    d = ImageDraw.Draw(spent)
    for x, y in ((3, 3), (N - 5, 3), (3, N - 5), (N - 5, N - 5)):
        d.rectangle([x, y, x + 1, y + 1], fill=(70, 44, 20))
    save(spent, "spent")

    # The pipe: lit down one side, dark down the other.
    pipe = Image.new("RGB", (N, N))
    for x in range(N):
        k = 1.25 - abs(x - 9) / 22.0
        col = tuple(max(0, min(255, int(c * k))) for c in (40, 168, 60))
        for y in range(N):
            pipe.putpixel((x, y), col)
    save(pipe, "pipe")
    save(noisy((12, 40, 18), 3), "pipe_dark")

    stone = bevel(noisy((150, 150, 156), 6), (196, 196, 202), (96, 96, 104))
    save(stone, "stone")
    wall = noisy((150, 150, 156), 5)
    d = ImageDraw.Draw(wall)
    d.rectangle([0, 0, N - 1, N - 1], outline=(110, 110, 118))
    save(wall, "wall")
    floor = noisy((112, 96, 84), 6)
    d = ImageDraw.Draw(floor)
    for y in range(0, N, 8):
        d.line([(0, y), (N - 1, y)], fill=(84, 70, 60))
    save(floor, "floor")
    save(noisy((34, 34, 36), 3), "frame")
    lamp = noisy((255, 240, 200), 3)
    ImageDraw.Draw(lamp).rectangle([0, 0, N - 1, N - 1], outline=(200, 180, 130))
    save(lamp, "lamp")

    save(noisy((230, 230, 230), 3), "pole")
    for name, rgb in (("flag", (220, 40, 40)), ("half_flag", (60, 120, 230))):
        f = noisy(rgb, 4)
        d = ImageDraw.Draw(f)
        d.ellipse([10, 10, 21, 21], fill=(255, 255, 255))
        save(f, name)

    cloud = noisy((245, 248, 252), 2)
    d = ImageDraw.Draw(cloud)
    d.line([(0, N - 1), (N - 1, N - 1)], fill=(200, 214, 236))
    d.line([(0, N - 2), (N - 1, N - 2)], fill=(224, 232, 246))
    save(cloud, "cloud")
    hill = noisy((70, 150, 70), 5)
    d = ImageDraw.Draw(hill)
    for _ in range(6):
        x, y = rng.randint(2, N - 6), rng.randint(2, N - 6)
        d.rectangle([x, y, x + 2, y + 1], fill=(48, 118, 52))
    save(hill, "hill")
    save(noisy((200, 230, 240), 2), "glass")

    coin = noisy((255, 214, 40), 3)
    d = ImageDraw.Draw(coin)
    d.rectangle([0, 0, N - 1, N - 1], outline=(190, 140, 10))
    d.rectangle([13, 8, 18, 23], fill=(190, 140, 10))
    save(coin, "coin")


# ----------------------------------------------------------------- sounds

def square(f, t, duty=0.5):
    return 1.0 if (f * t) % 1.0 < duty else -1.0


def tone(notes, vol=0.22, duty=0.5, decay=6.0, gap=0.0):
    """notes: (seconds, hertz or (from, to)); a square wave, each note fading."""
    out = []
    for seconds, f in notes:
        n = int(seconds * RATE)
        phase = 0.0
        for i in range(n):
            u = i / RATE
            hz = f if not isinstance(f, tuple) else f[0] + (f[1] - f[0]) * (i / n)
            phase += hz / RATE
            env = min(1.0, u / 0.003) * math.exp(-u * decay) * min(1.0, (seconds - u) / 0.005)
            out.append((1.0 if phase % 1.0 < duty else -1.0) * env * vol)
        out.extend([0.0] * int(gap * RATE))
    return out


def noise(seconds, vol=0.3, decay=18.0, low=0.4):
    out, lp = [], 0.0
    r = random.Random(7)
    for i in range(int(seconds * RATE)):
        u = i / RATE
        lp += (r.uniform(-1, 1) - lp) * low
        out.append(lp * math.exp(-u * decay) * vol)
    return out


def mix(a, b):
    n = max(len(a), len(b))
    return [(a[i] if i < len(a) else 0.0) + (b[i] if i < len(b) else 0.0) for i in range(n)]


def sounds():
    os.makedirs(SND, exist_ok=True)
    out = lambda samples, name: write(samples, os.path.join(SND, name + ".ogg"))  # noqa: E731
    out(tone([(0.22, (330, 760))], vol=0.16, duty=0.25, decay=5.0), "jump")
    out(tone([(0.07, 988), (0.42, 1319)], vol=0.16, decay=5.0), "coin")
    out(mix(tone([(0.09, (220, 140))], vol=0.2, decay=10.0), noise(0.08, 0.2)), "bump")
    out(mix(tone([(0.12, (520, 180))], vol=0.2, duty=0.3, decay=9.0), noise(0.12, 0.25, 22.0)), "stomp")
    out(tone([(0.1, 392), (0.1, 311), (0.1, 262), (0.3, 196)], vol=0.18, duty=0.35, decay=4.0), "hurt")
    out(tone([(0.09, 523), (0.09, 659), (0.09, 784), (0.32, 1047)], vol=0.16, decay=4.0), "flag")
    out(tone([(0.07, f) for f in (784, 587, 392, 784, 587, 392, 784, 587, 392)], vol=0.15, duty=0.3, decay=3.0), "pipe")
    out(tone([(0.06, f) for f in (392, 440, 494, 523, 587, 659, 698, 784)], vol=0.14, duty=0.25, decay=2.0), "power_up")
    out(tone([(0.05, f) for f in (262, 392, 330, 523, 392, 659, 523, 784, 659, 1047)], vol=0.15, decay=1.5), "grow")
    out(tone([(0.05, f) for f in (784, 523, 659, 392, 523, 330, 392, 262)], vol=0.15, duty=0.3, decay=1.5), "shrink")
    out(tone([(0.06, (900, 300))], vol=0.14, duty=0.25, decay=12.0), "fire")
    out(tone([(0.12, 523), (0.12, 659), (0.12, 784), (0.12, 1047), (0.12, 784), (0.12, 1047), (0.6, 1319)],
             vol=0.16, decay=2.5), "win")
    out(tone([(0.25, 392), (0.25, 330), (0.25, 262), (0.25, 247), (0.7, 196)], vol=0.18, duty=0.35, decay=2.0),
        "over")


if __name__ == "__main__":
    textures()
    sounds()
    print("platformer: pictures in", os.path.normpath(TEX), "and sounds in", os.path.normpath(SND))
