#!/usr/bin/env python3
"""The Inferno's block pictures (games/inferno/textures), drawn by rule.

Drawn 16x16 and doubled to the 32 px every pack here uses (a tile of another
size is skipped by the client), from a fixed seed, so running it again makes
the same files. Named the way the server looks them up: inferno:scoria is
inferno_scoria.png.

    python3 tools/make_inferno_textures.py
"""
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "..", "games", "inferno", "textures")
N = 16


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c)


def speckle(base, spread, rng, bright=None):
    """Every pixel the base colour, a little lighter or darker; now and then a
    brighter fleck."""
    img = Image.new("RGB", (N, N))
    for y in range(N):
        for x in range(N):
            c = shade(base, 1.0 + rng.uniform(-spread, spread))
            if bright and rng.random() < bright[1]:
                c = shade(bright[0], 1.0 + rng.uniform(-0.1, 0.1))
            img.putpixel((x, y), c)
    return img


def blobs(img, colour, count, size, rng):
    """Round patches of a colour, wrapping at the edges so the tile repeats."""
    for _ in range(count):
        cx, cy, r = rng.randrange(N), rng.randrange(N), rng.uniform(1.0, size)
        for y in range(N):
            for x in range(N):
                dx = min(abs(x - cx), N - abs(x - cx))
                dy = min(abs(y - cy), N - abs(y - cy))
                if dx * dx + dy * dy <= r * r:
                    img.putpixel((x, y), shade(colour, 1.0 + rng.uniform(-0.12, 0.12)))
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    rng = random.Random(1709)
    tiles = {}

    tiles["scoria"] = blobs(speckle((110, 36, 34), 0.18, rng), (80, 22, 24), 7, 2.2, rng)

    sand = speckle((84, 64, 52), 0.15, rng)
    for fx, fy in ((4, 5), (11, 10), (5, 12)):   # the faces in it
        for dx, dy in ((0, 0), (2, 0), (1, 2)):
            sand.putpixel(((fx + dx) % N, (fy + dy) % N), (46, 32, 26))
    tiles["ash_sand"] = sand

    tiles["glowrock"] = blobs(speckle((200, 150, 80), 0.12, rng), (255, 230, 150), 9, 2.0, rng)

    quartz = tiles["scoria"].copy()
    for _ in range(5):
        x, y = rng.randrange(N), rng.randrange(N)
        for i in range(3):
            quartz.putpixel(((x + i) % N, (y + i // 2) % N), (236, 226, 214))
    tiles["quartz_ore"] = quartz

    bricks = Image.new("RGB", (N, N))
    for y in range(N):
        for x in range(N):
            row = y // 4
            mortar = y % 4 == 3 or (x + (4 if row % 2 else 0)) % 8 == 7
            base = (30, 12, 16) if mortar else (58, 26, 32)
            bricks.putpixel((x, y), shade(base, 1.0 + rng.uniform(-0.1, 0.1)))
    tiles["bricks"] = bricks

    tiles["obsidian"] = blobs(speckle((22, 16, 34), 0.25, rng, ((60, 40, 90), 0.06)),
                              (34, 22, 52), 4, 2.5, rng)

    tiles["lava"] = blobs(speckle((230, 100, 20), 0.1, rng), (255, 180, 40), 8, 2.4, rng)

    for name, img in tiles.items():
        img.resize((2 * N, 2 * N), Image.NEAREST).convert("RGBA").save(
            os.path.join(OUT, f"inferno_{name}.png"))
    print(f"{len(tiles)} textures in {os.path.normpath(OUT)}")


if __name__ == "__main__":
    main()
