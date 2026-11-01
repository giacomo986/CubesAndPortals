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

    # The countries (games/inferno: basalt fields, fungus forests).
    basalt = Image.new("RGB", (N, N))
    for x in range(N):
        col = shade((56, 54, 62), 1.0 + rng.uniform(-0.12, 0.12))
        for y in range(N):
            seam = x % 4 == 0
            basalt.putpixel((x, y), shade((30, 28, 34) if seam else col, 1.0 + rng.uniform(-0.05, 0.05)))
    tiles["basalt"] = basalt

    stem = Image.new("RGB", (N, N))
    for x in range(N):
        for y in range(N):
            ridge = (x + (y // 5) % 2) % 3 == 0
            stem.putpixel((x, y), shade((96, 48, 80) if ridge else (124, 66, 100), 1.0 + rng.uniform(-0.08, 0.08)))
    tiles["fungus_stem"] = stem

    tiles["fungus_cap"] = blobs(speckle((40, 140, 124), 0.12, rng), (150, 255, 220), 6, 1.6, rng)

    fplanks = Image.new("RGB", (N, N))
    for y in range(N):
        for x in range(N):
            gap = y % 4 == 3 or (x + (8 if (y // 4) % 2 else 0)) % 16 == 15
            fplanks.putpixel((x, y), shade((70, 36, 58) if gap else (150, 86, 120), 1.0 + rng.uniform(-0.07, 0.07)))
    tiles["fungus_planks"] = fplanks

    qblock = speckle((228, 220, 210), 0.04, rng)
    for i in range(N):
        for e in (0, N - 1):
            qblock.putpixel((i, e), (196, 186, 174))
            qblock.putpixel((e, i), (196, 186, 174))
    tiles["quartz_block"] = qblock

    lantern = blobs(speckle((255, 170, 70), 0.08, rng), (255, 230, 150), 5, 2.0, rng)
    for i in range(N):
        for e in (0, 1, N - 2, N - 1):
            lantern.putpixel((i, e), (200, 150, 40))
            lantern.putpixel((e, i), (200, 150, 40))
    tiles["ember_lantern"] = lantern

    for name, img in tiles.items():
        img.resize((2 * N, 2 * N), Image.NEAREST).convert("RGBA").save(
            os.path.join(OUT, f"inferno_{name}.png"))
    # A fireball's fire: tongues of flame up from the bottom, clear between them.
    fire = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    for x in range(N):
        top = int(N * (0.35 + 0.5 * rng.random()))
        for y in range(N - top, N):
            k = (y - (N - top)) / max(1, top)
            c = (255, int(220 - 150 * k), int(80 - 70 * k))
            fire.putpixel((x, y), (*c, 230))
    fire.resize((2 * N, 2 * N), Image.NEAREST).save(os.path.join(OUT, "inferno_fire_side.png"))
    # Its top and bottom clear: a fire is its four sides of flame.
    for face in ("top", "bottom"):
        Image.new("RGBA", (2 * N, 2 * N), (0, 0, 0, 0)).save(os.path.join(OUT, f"inferno_fire_{face}.png"))
    # A level's way back (inferno_heart:return): obsidian with a ring of fire
    # in it, lit once the guardian is dead.
    ret = tiles["obsidian"].copy()
    for y in range(N):
        for x in range(N):
            r = ((x - 7.5) ** 2 + (y - 7.5) ** 2) ** 0.5
            if 3.5 <= r <= 5.5:
                ret.putpixel((x, y), shade((255, 140, 50), 1.0 + rng.uniform(-0.15, 0.1)))
    ret.resize((2 * N, 2 * N), Image.NEAREST).convert("RGBA").save(os.path.join(OUT, "inferno_heart_return.png"))
    print(f"{len(tiles) + 4} textures in {os.path.normpath(OUT)}")


if __name__ == "__main__":
    main()
