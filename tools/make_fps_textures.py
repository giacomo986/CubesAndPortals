#!/usr/bin/env python3
"""The yafps arena's block pictures (games/yafps/textures), drawn by rule: a
UAC base breached by Hell -- steel floor plates, tech wall panels, light strips,
hazard trim, and hellrock with fire in its cracks.

Drawn 16x16 and doubled to the 32 px every pack here uses, from a fixed seed,
so running it again makes the same files. Named the way the server looks them
up: yafps_core:floor is yafps_core_floor.png.

    python3 tools/make_fps_textures.py
"""
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "..", "games", "yafps", "textures")
N = 16


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c)


def noisy(base, spread, rng):
    img = Image.new("RGB", (N, N))
    for y in range(N):
        for x in range(N):
            img.putpixel((x, y), shade(base, 1.0 + rng.uniform(-spread, spread)))
    return img


def floor(rng):
    """Four steel plates, bevelled, a rivet in each corner."""
    img = noisy((78, 80, 86), 0.06, rng)
    for y in range(N):
        for x in range(N):
            if x % 8 == 0 or y % 8 == 0:
                img.putpixel((x, y), (40, 41, 46))
            elif x % 8 == 1 or y % 8 == 1:
                img.putpixel((x, y), (104, 106, 112))
    for cx in (2, 6, 10, 14):
        for cy in (2, 6, 10, 14):
            if (cx % 8 in (2, 6)) and (cy % 8 in (2, 6)):
                img.putpixel((cx, cy), (130, 132, 138))
    return img


def wall(rng):
    """A tech panel: dark grooves, a lighter frame, a vent."""
    img = noisy((92, 96, 104), 0.05, rng)
    for y in range(N):
        for x in range(N):
            if x in (0, 15) or y in (0, 15):
                img.putpixel((x, y), (52, 55, 62))
            elif x in (1, 14) or y in (1, 14):
                img.putpixel((x, y), (122, 126, 134))
    for y in (5, 7, 9, 11):
        for x in range(4, 12):
            img.putpixel((x, y), (40, 42, 48))
    return img


def base(rng):
    return noisy((46, 40, 40), 0.12, rng)


def light(rng):
    """A light panel in its frame."""
    img = Image.new("RGB", (N, N), (60, 64, 72))
    for y in range(2, 14):
        for x in range(2, 14):
            img.putpixel((x, y), shade((215, 235, 255), 1.0 + rng.uniform(-0.04, 0.04)))
    return img


def trim(rng):
    """Hazard stripes."""
    img = Image.new("RGB", (N, N))
    for y in range(N):
        for x in range(N):
            c = (220, 170, 30) if ((x + y) // 4) % 2 == 0 else (30, 30, 32)
            img.putpixel((x, y), shade(c, 1.0 + rng.uniform(-0.08, 0.08)))
    return img


def hellrock(rng):
    """Black-red rock with fire in its cracks: a wandering crack or three."""
    img = noisy((70, 22, 18), 0.18, rng)
    for _ in range(3):
        x, y = rng.randrange(N), rng.randrange(N)
        for _ in range(14):
            img.putpixel((x % N, y % N), shade((255, 120, 30), rng.uniform(0.85, 1.1)))
            x += rng.choice((-1, 0, 1))
            y += rng.choice((0, 1))
    return img


def gate(rng):
    """Where the demons come through: a ring and a star in red fire."""
    img = noisy((40, 12, 12), 0.1, rng)
    import math
    for i in range(64):
        a = i / 64 * 2 * math.pi
        img.putpixel((int(7.5 + 6.5 * math.cos(a)), int(7.5 + 6.5 * math.sin(a))), (255, 60, 20))
    pts = [(7.5 + 6 * math.cos(-math.pi / 2 + k * 4 * math.pi / 5),
            7.5 + 6 * math.sin(-math.pi / 2 + k * 4 * math.pi / 5)) for k in range(6)]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        for s in range(24):
            t = s / 23
            img.putpixel((int(round(x0 + (x1 - x0) * t)), int(round(y0 + (y1 - y0) * t))), (255, 90, 30))
    return img


def pillar(rng):
    """Steel with an orange light down the middle."""
    img = noisy((70, 72, 78), 0.05, rng)
    for y in range(N):
        img.putpixel((0, y), (40, 42, 48))
        img.putpixel((15, y), (40, 42, 48))
        for x in (7, 8):
            img.putpixel((x, y), (255, 140, 40) if y % 4 else (160, 80, 30))
    return img


if __name__ == "__main__":
    rng = random.Random(9000)
    os.makedirs(OUT, exist_ok=True)
    for name, make in [("floor", floor), ("wall", wall), ("base", base), ("light", light),
                       ("trim", trim), ("hellrock", hellrock), ("gate", gate), ("pillar", pillar)]:
        make(rng).resize((32, 32), Image.NEAREST).save(os.path.join(OUT, f"yafps_core_{name}.png"))
    print("textures in", os.path.normpath(OUT))
