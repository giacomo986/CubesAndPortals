#!/usr/bin/env python3
"""The ores' pictures (mods/ore) and the copper ingot (mods/furnace), drawn by
rule into assets/textures.

An ore block is core_stone.png itself with clusters of its mineral pressed
into it: each fleck a 2 px cell (the 16 px grid every item here is drawn on),
lit on its top-left, dark on its bottom-right, and ringed by a darker rim of
the stone, so a seam reads as ore at the end of a tunnel and never as a
differently tinted stone. Items are 16x16 masks doubled to 32 px and shaded the
way tools/make_armor_textures.py shades the ingots.

Named the way the client looks them up: ore:coal_ore is ore_coal_ore.png.
From a fixed seed, so running it again makes the same files.

    python3 tools/make_ore_textures.py
"""
import os
import random

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "textures")

# Where the flecks sit, on the 16 grid, and the same layout for every ore so
# the ores are told apart by colour and shape of the cluster, as in the classic block game.
# Two layouts: a few big clumps (coal, copper, iron, gold) and many small
# specks (cinnabar, lapis, diamond, emerald).
CLUMPS = [
    [(3, 2), (4, 2), (3, 3), (4, 3), (5, 3)],
    [(10, 4), (11, 4), (11, 5), (12, 5)],
    [(2, 9), (3, 9), (3, 10), (4, 10)],
    [(8, 9), (9, 9), (8, 10), (9, 10), (10, 11)],
    [(12, 12), (13, 12), (12, 13)],
    [(6, 13), (7, 13)],
]
SPECKS = [
    [(3, 3), (4, 3), (3, 4)],
    [(10, 2), (11, 3)],
    [(7, 6), (8, 6), (8, 7)],
    [(2, 10), (3, 11)],
    [(12, 8), (12, 9), (13, 9)],
    [(6, 12), (7, 12), (7, 13)],
    [(11, 13), (12, 13)],
]

# Base, highlight and shadow of each mineral.
ORES = {
    "coal_ore":     (CLUMPS, (46, 44, 46), (82, 80, 84), (22, 21, 23)),
    "iron":         (CLUMPS, (216, 176, 146), (240, 212, 186), (160, 118, 90)),
    "copper_ore":   (CLUMPS, (224, 128, 76), (120, 196, 160), (150, 76, 44)),
    "gold":         (CLUMPS, (250, 214, 60), (255, 246, 160), (190, 140, 20)),
    "cinnabar_ore": (SPECKS, (220, 20, 20), (255, 110, 100), (130, 8, 8)),
    "lapis_ore":    (SPECKS, (32, 72, 200), (96, 140, 255), (16, 36, 120)),
    "diamond_ore":  (SPECKS, (98, 224, 222), (210, 255, 252), (40, 140, 140)),
    "emerald_ore":  (SPECKS, (40, 200, 90), (150, 255, 170), (10, 110, 44)),
}


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c[:3])


def ore_block(name, layout, base, light, dark, rng):
    img = Image.open(os.path.join(OUT, "core_stone.png")).convert("RGBA")
    cells = {p for cluster in layout for p in cluster}

    def cell(x, y, colour):
        for dy in range(2):
            for dx in range(2):
                img.putpixel((x * 2 + dx, y * 2 + dy), colour + (255,))

    # The rim first: a darker stone round every fleck, which is what makes a
    # cluster stand out of the grey instead of floating on it.
    for x, y in cells:
        for nx, ny in ((x + 1, y), (x, y + 1), (x + 1, y + 1), (x - 1, y), (x, y - 1)):
            if (nx, ny) not in cells and 0 < nx < 15 and 0 < ny < 15:
                cell(nx, ny, shade(img.getpixel((nx * 2, ny * 2)), 0.62))

    for x, y in cells:
        if (x - 1, y) not in cells and (x, y - 1) not in cells:
            c = light
        elif (x + 1, y) not in cells and (x, y + 1) not in cells:
            c = dark
        else:
            c = shade(base, 1.0 + rng.uniform(-0.08, 0.08))
        cell(x, y, c)
    img.save(os.path.join(OUT, f"ore_{name}.png"))


MASKS = {
    "lump": [
        "................",
        "................",
        "................",
        "......####......",
        "....#######.....",
        "...#########....",
        "..###########...",
        "..############..",
        "..############..",
        "...###########..",
        "...##########...",
        "....#######.....",
        "......###.......",
    ],
    "raw": [
        "................",
        "................",
        "................",
        ".......###......",
        ".....######.....",
        "....#########...",
        "...##########...",
        "..###########...",
        "..############..",
        "...###########..",
        "....#########...",
        ".....##..###....",
    ],
    "dust": [
        "................",
        "................",
        "................",
        "................",
        "......#..#......",
        "...#..........#.",
        "........##......",
        "..#...#####..#..",
        "....#########...",
        "...###########..",
        "..#############.",
        "................",
    ],
    "lapis": [
        "................",
        "................",
        "......####......",
        "....#######.....",
        "...#########....",
        "...##########...",
        "..###########...",
        "..###########...",
        "...##########...",
        "....########....",
        ".....#####......",
        "................",
    ],
    "emerald": [
        "................",
        ".......##.......",
        "......####......",
        ".....######.....",
        ".....######.....",
        "....########....",
        "....########....",
        "....########....",
        ".....######.....",
        ".....######.....",
        "......####......",
        ".......##.......",
    ],
    "ingot": [
        "................",
        "................",
        "................",
        "................",
        "................",
        ".....#######....",
        "....#########...",
        "...###########..",
        "..###########...",
        ".###########....",
        "..#########.....",
        "................",
    ],
}


def item(mask, colour, path, flecks=None, rng=None):
    """The ingots' shading, and optionally a second colour scattered through
    the inside: the rust on raw iron, the glint on coal."""
    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    rows = mask + ["." * 16] * (16 - len(mask))
    on = lambda x, y: 0 <= y < 16 and 0 <= x < 16 and rows[y][x] == "#"
    for y in range(16):
        for x in range(16):
            if not on(x, y):
                continue
            edge = not (on(x - 1, y) and on(x + 1, y) and on(x, y - 1) and on(x, y + 1))
            light = on(x, y - 1) is False or on(x - 1, y) is False
            k = 0.6 if edge and not light else (1.15 if edge else 1.0)
            c = colour
            if flecks and not edge and rng.random() < flecks[1]:
                c = flecks[0]
            img.putpixel((x, y), shade(c, k) + (255,))
    img.resize((32, 32), Image.NEAREST).save(path)


def main():
    rng = random.Random(4417)
    for name, (layout, base, light, dark) in ORES.items():
        ore_block(name, layout, base, light, dark, rng)

    items = {
        "ore_coal": ("lump", (74, 72, 78), ((150, 150, 160), 0.25)),
        "ore_raw_iron": ("raw", (200, 164, 136), ((150, 110, 84), 0.3)),
        "ore_raw_gold": ("raw", (240, 196, 50), ((255, 240, 150), 0.25)),
        "ore_raw_copper": ("raw", (214, 118, 70), ((110, 180, 150), 0.2)),
        "ore_cinnabar": ("dust", (210, 24, 24), None),
        "ore_lapis": ("lapis", (40, 80, 210), ((220, 200, 90), 0.08)),
        "ore_emerald": ("emerald", (44, 206, 96), ((170, 255, 190), 0.12)),
        "furnace_copper_ingot": ("ingot", (224, 128, 76), None),
    }
    for name, (mask, colour, flecks) in items.items():
        item(MASKS[mask], colour, os.path.join(OUT, name + ".png"), flecks, rng)


main()
