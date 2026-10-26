#!/usr/bin/env python3
"""The armour's pictures (mods/armor) and the gold ingot and diamond it is made
of (mods/furnace), drawn by rule into assets/textures.

Each piece is a 16x16 mask doubled to 32 px, filled with its material's colour
and shaded darker at the edges. Named the way the client looks them up:
armor:iron_helmet is armor_iron_helmet.png.

    python3 tools/make_armor_textures.py
"""
import os

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "textures")

MASKS = {
    "helmet": [
        "................",
        "................",
        "................",
        "....########....",
        "...##########...",
        "..############..",
        "..############..",
        "..####....####..",
        "..###......###..",
        "..###......###..",
        "..##........##..",
        "................",
    ],
    "chestplate": [
        "................",
        "..####....####..",
        ".######..######.",
        ".##############.",
        ".##############.",
        ".##.########.##.",
        ".##.########.##.",
        "....########....",
        "....########....",
        "....########....",
        "....########....",
        "....########....",
        "................",
    ],
    "leggings": [
        "................",
        "...##########...",
        "...##########...",
        "...##########...",
        "...####..####...",
        "...####..####...",
        "...###....###...",
        "...###....###...",
        "...###....###...",
        "...###....###...",
        "...###....###...",
        "...###....###...",
        "................",
    ],
    "boots": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "...###....###...",
        "...###....###...",
        "...###....###...",
        "..####....####..",
        ".#####....#####.",
        ".#####....#####.",
        "................",
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
    "gem": [
        "................",
        "................",
        ".....######.....",
        "....########....",
        "...##########...",
        "..############..",
        "...##########...",
        "....########....",
        ".....######.....",
        "......####......",
        ".......##.......",
        "................",
    ],
}

COLOURS = {
    "wood": (164, 118, 66),
    "gold": (240, 200, 60),
    "iron": (206, 206, 212),
    "diamond": (98, 224, 222),
}


def draw(mask, colour, path):
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
            img.putpixel((x, y), tuple(min(255, int(c * k)) for c in colour) + (255,))
    img.resize((32, 32), Image.NEAREST).save(path)


for mat, colour in COLOURS.items():
    for piece in ("helmet", "chestplate", "leggings", "boots"):
        draw(MASKS[piece], colour, os.path.join(OUT, f"armor_{mat}_{piece}.png"))
draw(MASKS["ingot"], COLOURS["gold"], os.path.join(OUT, "furnace_gold_ingot.png"))
draw(MASKS["ingot"], COLOURS["iron"], os.path.join(OUT, "furnace_iron_ingot.png"))
draw(MASKS["gem"], COLOURS["diamond"], os.path.join(OUT, "ore_diamond.png"))
