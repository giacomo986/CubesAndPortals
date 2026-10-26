#!/usr/bin/env python3
"""The pictures of what a pet is tamed with, and caught with: mods/pets's rod
and fish, mods/mobs's bone and flesh, which had none, and mods/animals's drops. Drawn by rule into
assets/textures, the way tools/make_armor_textures.py draws the armour: a
16x16 mask, one letter a colour, doubled to 32 px, darker at the lower right
edges. Named the way the client looks them up: pets:fish is pets_fish.png.

    python3 tools/make_pet_textures.py
"""
import os

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "textures")

ICONS = {
    "pets_fish": ({"b": (96, 140, 190), "l": (170, 200, 225), "e": (20, 20, 30), "t": (70, 110, 160)}, [
        "................",
        "................",
        "................",
        "................",
        "......bbbb......",
        "....bbllllbb..tt",
        "...bebllllllbttt",
        "..bbbllllllllbt.",
        "..bbbbbbbbbbbbt.",
        "...bbbbbbbbbbttt",
        "....bbbbbbbb..tt",
        "......bbbb......",
    ]),
    "pets_rod": ({"w": (140, 96, 52), "s": (225, 225, 225), "h": (120, 120, 130)}, [
        "............ww..",
        "...........ww.s.",
        "..........ww..s.",
        ".........ww...s.",
        "........ww....s.",
        ".......ww.....s.",
        "......ww......s.",
        ".....ww.......s.",
        "....ww........s.",
        "...ww.........s.",
        "..ww.........hs.",
        ".ww..........h..",
        "ww..............",
    ]),
    "mobs_bone": ({"b": (236, 232, 214)}, [
        "................",
        "..bb........bb..",
        ".bbbb......bbbb.",
        ".bbbbb....bbbbb.",
        "..bbbbbbbbbbbb..",
        "..bbbbbbbbbbbb..",
        ".bbbbb....bbbbb.",
        ".bbbb......bbbb.",
        "..bb........bb..",
    ]),
    "mobs_flesh": ({"r": (170, 60, 50), "p": (215, 110, 100), "f": (240, 220, 200)}, [
        "................",
        "................",
        ".....rrrrr......",
        "...rrppppprr....",
        "..rppppppppprr..",
        "..rppffppppppr..",
        ".rpppffpppppppr.",
        ".rppppppppffppr.",
        "..rpppppppffpr..",
        "..rrpppppppprr..",
        "....rrrrrrrr....",
    ]),
    # mods/animals's drops.
    "animals_pork": ({"p": (240, 150, 150), "f": (250, 225, 215), "d": (200, 100, 105)}, [
        "................",
        "................",
        "....pppppp......",
        "...ppffpppppp...",
        "..ppffppppppdp..",
        "..pppppppppdddp.",
        "..pppppppppdffd.",
        "...ppppppppdffd.",
        "....ppppppdddd..",
        "......dddddd....",
    ]),
    "animals_beef": ({"r": (160, 40, 40), "m": (200, 70, 60), "f": (245, 235, 225)}, [
        "................",
        "................",
        "...rrrrrrr......",
        "..rmmmmmmmrr....",
        ".rmmffmmmmmmr...",
        ".rmmmffmmmmmmr..",
        ".rmmmmmffmmmmr..",
        "..rmmmmmmffmmr..",
        "..rrmmmmmmmmr...",
        "....rrrrrrrr....",
    ]),
    "animals_wool": ({"w": (238, 236, 230), "s": (205, 202, 195)}, [
        "................",
        "................",
        "....wwwwwww.....",
        "...wwwswwwwww...",
        "..wwwwwwwswwww..",
        "..wswwwwwwwwsw..",
        ".wwwwwswwwwwwww.",
        ".wwwwwwwwwswwww.",
        "..wwswwwwwwwww..",
        "..wwwwwwswwwsw..",
        "...wwwwwwwwww...",
        ".....wwwwww.....",
    ]),
    "animals_feather": ({"w": (245, 245, 240), "s": (190, 190, 185), "q": (150, 150, 145)}, [
        "............ww..",
        "..........wwww..",
        ".........wwwsw..",
        "........wwwsww..",
        ".......wwwsww...",
        "......wwwsww....",
        ".....wwwsww.....",
        "....wwwsww......",
        "....wwsww.......",
        "...wwsw.........",
        "...qs...........",
        "..q.............",
        ".q..............",
    ]),
    "animals_egg": ({"e": (240, 228, 200), "s": (205, 185, 150)}, [
        "................",
        "................",
        "......eeee......",
        ".....eeeeee.....",
        "....eeeeeeee....",
        "....eeeseeee....",
        "...eeeeeeeeee...",
        "...eeeeeeeeee...",
        "...eeeeeeeees...",
        "...eeeeeeeees...",
        "....eeeeeeee....",
        ".....eeeeee.....",
        "......ssss......",
    ]),
    "animals_shears": ({"m": (205, 205, 210), "d": (140, 140, 150), "h": (60, 60, 70)}, [
        "................",
        "..........mm....",
        ".........mmd....",
        "........mmd.....",
        "...mm..mmd......",
        "....mmmmd.......",
        ".....mmd........",
        "....hhdmm.......",
        "...hh..hmm......",
        "..hh....hh......",
        "..h.....hh......",
        "..hh...hh.......",
        "...hhhhh........",
    ]),
    "animals_seeds": ({"s": (120, 160, 60), "d": (80, 110, 40)}, [
        "................",
        "................",
        "................",
        ".....s......s...",
        "....sd...s..d...",
        "........sd......",
        "..s.............",
        "..d....s....s...",
        "......sd....d...",
        "...s............",
        "...d.....s......",
        ".........d......",
    ]),
    "animals_cooked_pork": ({"p": (190, 120, 80), "f": (235, 205, 170), "d": (140, 80, 50)}, [
        "................",
        "................",
        "....pppppp......",
        "...ppffpppppp...",
        "..ppffppppppdp..",
        "..pppppppppdddp.",
        "..pppppppppdffd.",
        "...ppppppppdffd.",
        "....ppppppdddd..",
        "......dddddd....",
    ]),
    "animals_steak": ({"r": (90, 50, 30), "m": (140, 85, 50), "f": (220, 200, 170)}, [
        "................",
        "................",
        "...rrrrrrr......",
        "..rmmmmmmmrr....",
        ".rmmffmmmmmmr...",
        ".rmmmffmmmmmmr..",
        ".rmmmmmffmmmmr..",
        "..rmmmmmmffmmr..",
        "..rrmmmmmmmmr...",
        "....rrrrrrrr....",
    ]),
    "animals_fried_egg": ({"w": (245, 242, 235), "y": (245, 190, 40), "o": (220, 150, 30)}, [
        "................",
        "................",
        "....wwwwww......",
        "..wwwwwwwwww....",
        ".wwwwwwwwwwwww..",
        ".wwwwyyyywwwww..",
        "wwwwyyyyyowwwww.",
        "wwwwyyyyyowwwww.",
        ".wwwwyoooowwww..",
        ".wwwwwwwwwwwww..",
        "..wwwwwwwwwww...",
        "....wwwwwww.....",
    ]),
}


def draw(palette, mask, path):
    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    rows = mask + ["." * 16] * (16 - len(mask))
    on = lambda x, y: 0 <= y < 16 and 0 <= x < 16 and rows[y][x] != "."
    for y in range(16):
        for x in range(16):
            if not on(x, y):
                continue
            k = 0.7 if not (on(x + 1, y) and on(x, y + 1)) else 1.0
            img.putpixel((x, y), tuple(int(c * k) for c in palette[rows[y][x]]) + (255,))
    img.resize((32, 32), Image.NEAREST).save(path)


for name, (palette, mask) in ICONS.items():
    draw(palette, [r.ljust(16, ".") for r in mask], os.path.join(OUT, f"{name}.png"))
print("fish, rod, bone, flesh, pork, beef, wool, feather, egg, shears, seeds and cooked food in", os.path.normpath(OUT))
