#!/usr/bin/env python3
"""The pictures of what the inhabitants and the helpers brought: the helper
robot packed up (mods/helpers), a sapling (mods/trees, its icon and the
picture on its crossed planes) and a cooked fish (mods/pets).

Drawn the way tools/make_item_textures.py draws: 16x16 doubled to 32 px, flat
colours and a dark outline round every shape.

    python3 tools/make_villager_textures.py
"""
import importlib.util
import os
import shutil

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
_spec = importlib.util.spec_from_file_location("make_item_textures", os.path.join(HERE, "make_item_textures.py"))
items = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(items)
OUT = items.OUT

SHAPES = {
    "helpers_robot": (
        [
            "................",
            ".........r......",
            ".........m......",
            ".....oooooooo...",
            ".....osssssso...",
            ".....osgsgsso...",
            ".....osssssso...",
            ".....oooooooo...",
            ".......mmmm.....",
            "....oOOOOOOOOo..",
            "...moOddddddOom.",
            "...moOdgdrdddOm.",
            "...m.OOOOOOOO.m.",
            ".......d..d.....",
            "......dd..dd....",
            "................",
        ],
        {"o": (242, 150, 40), "O": (214, 120, 26), "s": (26, 30, 34), "g": (80, 240, 210),
         "r": (255, 80, 60), "m": (170, 172, 180), "d": (60, 60, 66)},
    ),
    "trees_sapling": (
        [
            "................",
            "......ll........",
            ".....lLll.l.....",
            "....lLLl.lLl....",
            "......lLlLl.....",
            "...ll..lLl..ll..",
            "..lLLl..t..lLl..",
            "....lLl.t.lLl...",
            "......l.t.l.....",
            "........t.......",
            "........t.......",
            ".......tT.......",
            "........t.......",
            ".......ddd......",
            "......dddDd.....",
            "................",
        ],
        {"l": (96, 170, 60), "L": (60, 120, 40), "t": (120, 86, 52), "T": (90, 62, 36),
         "d": (110, 80, 50), "D": (80, 58, 36)},
    ),
    "pets_cooked_fish": (
        [
            "................",
            "................",
            "................",
            "..........b.....",
            "......bbbbbb..b.",
            "....bbBBBBBbbbb.",
            "...bkBBccBBBbb..",
            "..bBBBcBBcBBbb..",
            "..bBBBBBBBBBbbb.",
            "...bbBBBBBbb..b.",
            ".....bbbbbb.....",
            "................",
            "................",
            "................",
            "................",
            "................",
        ],
        {"b": (150, 86, 40), "B": (196, 128, 64), "c": (120, 66, 30), "k": (30, 24, 20)},
    ),
}


def main():
    for name, (rows, colours) in SHAPES.items():
        items.draw(rows, colours).save(os.path.join(OUT, name + ".png"))
    # The sapling's planes: the same picture, bigger, as the plants' are.
    big = Image.open(os.path.join(OUT, "trees_sapling.png")).resize((64, 64), Image.NEAREST)
    big.save(os.path.join(ROOT, "mods", "trees", "sapling.png"))


if __name__ == "__main__":
    main()
