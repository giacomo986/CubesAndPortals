#!/usr/bin/env python3
"""The last items that were drawn as their name in the slot: the stick
(mods/timber), the two buckets (mods/lakes), the palette brush (mods/palette),
the iron pick (mods/furnace) and the screwdriver (mods/screwdriver); the arm
pack (mods/armpack); the boat (mods/boats), the cart (mods/carts), and the
compass and the map (mods/maps).

Drawn the way tools_pick.png is: a 16x16 picture doubled to 32 px, flat
colours lit on the top-left, and a dark outline round every shape. The iron
pick is tools_pick.png with its head recoloured, so the two picks are the same
tool in two metals.

    python3 tools/make_item_textures.py
"""
import os

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "textures")
OUTLINE = (30, 24, 20)
WOOD = (205, 163, 121)

SHAPES = {
    # The arm pack: the metal pack, its lid and vents, and its two arms up from
    # the lid's corners to a claw each.
    "armpack_armpack": (
        [
            "................",
            ".oo..........oo.",
            ".o.o........o.o.",
            "..oj........jo..",
            "...a........a...",
            "...a........a...",
            "....a......a....",
            ".....j....j.....",
            "....kkkkkkkk....",
            "....GGGGGGGg....",
            "....GggggggW....",
            "....gddddddW....",
            "....ggggggrW....",
            "....gddddddW....",
            "....WWWWWWWW....",
            "................",
        ],
        {"o": (255, 140, 20), "j": (92, 94, 100), "a": (176, 182, 192), "k": (48, 52, 60),
         "G": (196, 202, 210), "g": (150, 156, 166), "W": (112, 118, 128), "d": (70, 74, 82),
         "r": (255, 120, 20)},
    ),
    # The boat from the side: a plank hull, its gunwale lighter, a seat inside.
    "boats_boat": (
        [
            "................",
            "................",
            "................",
            "................",
            "................",
            ".r............r.",
            ".rr....ss....rr.",
            ".Lrrrrrrrrrrrrl.",
            ".LPPPPPPPPPPPPl.",
            "..PppppppppppP..",
            "..LPPPPPPPPPPl..",
            "...ppppppppppp..",
            "....PPPPPPPP....",
            "................",
            "................",
            "................",
        ],
        {"r": (226, 180, 120), "s": (150, 104, 60), "L": (196, 140, 84), "l": (140, 92, 50),
         "P": (176, 120, 66), "p": (140, 92, 50)},
    ),
    # The cart from the side: an iron tub with a rim, on two wheels.
    "carts_cart": (
        [
            "................",
            "................",
            "................",
            "..RRRRRRRRRRRR..",
            "..GggggggggggW..",
            "..GgddgddgddgW..",
            "..GggggggggggW..",
            "..GgddgddgddgW..",
            "..GggggggggggW..",
            "..WWWWWWWWWWWW..",
            "...kk......kk...",
            "..kaak....kaak..",
            "..kaak....kaak..",
            "...kk......kk...",
            "................",
            "................",
        ],
        {"R": (220, 224, 232), "G": (196, 202, 210), "g": (150, 156, 166), "W": (112, 118, 128),
         "d": (96, 100, 110), "k": (48, 52, 60), "a": (130, 134, 142)},
    ),
    # The compass: an iron case, its face, a needle red to the north.
    "maps_compass": (
        [
            "................",
            "................",
            ".....GGGGGG.....",
            "....GWWWWWWg....",
            "...GWwwwwwwWg...",
            "...GWwwwrwwWg...",
            "...GWwwrrrwWg...",
            "...GWwwwkwwWg...",
            "...GWwwwswwWg...",
            "...GWwwsssWWg...",
            "...GWwwwwwwWg...",
            "....gWWWWWWg....",
            ".....gggggg.....",
            "................",
            "................",
            "................",
        ],
        {"G": (196, 202, 210), "g": (112, 118, 128), "W": (150, 156, 166), "w": (236, 232, 214),
         "r": (220, 40, 30), "k": (40, 40, 44), "s": (170, 170, 176)},
    ),
    # The map: a sheet of parchment, a coast, a river and a red cross.
    "maps_map": (
        [
            "................",
            "..pppppppppppp..",
            "..pPPPPPPPPPPp..",
            "..pPgggPPPbbPp..",
            "..pPggggPbbbPp..",
            "..pPgggbbbbPPp..",
            "..pPPgbbbPPPPp..",
            "..pPPbbPPrPrPp..",
            "..pPbbPPPPrPPp..",
            "..pPbPPPPrPrPp..",
            "..pPPPPPPPPPPp..",
            "..pPPggPPPPPPp..",
            "..pPgggPPPPPPp..",
            "..pPPPPPPPPPPp..",
            "..pppppppppppp..",
            "................",
        ],
        {"p": (180, 150, 100), "P": (232, 216, 170), "g": (110, 160, 80), "b": (80, 130, 200),
         "r": (200, 40, 30)},
    ),
    "timber_stick": (
        [
            "................",
            "................",
            "............ww..",
            "...........wW...",
            "..........wW....",
            ".........wW.....",
            "........wW......",
            ".......wW.......",
            "......wW........",
            ".....wW.........",
            "....wW..........",
            "...wW...........",
            "..wW............",
            "................",
        ],
        {"w": (226, 186, 140), "W": (170, 128, 86)},
    ),
    "lakes_bucket": (
        [
            "................",
            "................",
            "...hhhhhhhhhh...",
            "..h..........h..",
            "..h..........h..",
            "..mmmmmmmmmmmm..",
            "..mlllllllllsm..",
            "...mllllllllm...",
            "...mllllllssm...",
            "...mlllllllsm...",
            "....mlllllsm....",
            "....mllllssm....",
            "....mmmmmmmm....",
        ],
        {"h": (120, 124, 132), "m": (150, 156, 166), "l": (205, 210, 218), "s": (168, 174, 184)},
    ),
    "lakes_bucket_water": (
        [
            "................",
            "................",
            "...hhhhhhhhhh...",
            "..h..........h..",
            "..h..........h..",
            "..mmmmmmmmmmmm..",
            "..mbbbbbbbbbbm..",
            "...mllllllllm...",
            "...mllllllssm...",
            "...mlllllllsm...",
            "....mlllllsm....",
            "....mllllssm....",
            "....mmmmmmmm....",
        ],
        {"h": (120, 124, 132), "m": (150, 156, 166), "l": (205, 210, 218), "s": (168, 174, 184),
         "b": (64, 120, 200)},
    ),
    "screwdriver_screwdriver": (
        [
            "................",
            "............ss..",
            "...........sS...",
            "..........sS....",
            ".........sS.....",
            "........rR......",
            ".......rrR......",
            "......rrR.......",
            ".....rrR........",
            "....rrR.........",
            "...rR...........",
            "................",
        ],
        {"s": (214, 218, 226), "S": (150, 156, 166), "r": (200, 56, 48), "R": (140, 36, 32)},
    ),
    "palette_brush": (
        [
            "................",
            "............rr..",
            "...........rgr..",
            "..........rgbr..",
            "..........mbb...",
            ".........mm.....",
            "........wW......",
            ".......wW.......",
            "......wW........",
            ".....wW.........",
            "....wW..........",
            "...wW...........",
            "..wW............",
            "................",
        ],
        {"r": (214, 60, 52), "g": (80, 180, 70), "b": (70, 110, 220), "m": (180, 184, 192),
         "w": (226, 186, 140), "W": (170, 128, 86)},
    ),
}


def draw(rows, colours):
    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    filled = set()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in colours:
                img.putpixel((x, y), colours[ch] + (255,))
                filled.add((x, y))
    # The outline: every empty cell beside a filled one, as the other items have.
    for x, y in list(filled):
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if 0 <= n[0] < 16 and 0 <= n[1] < 16 and n not in filled:
                img.putpixel(n, OUTLINE + (255,))
    return img.resize((32, 32), Image.NEAREST)


# A grey head's brightness k (0..1) to the material's colour, the highlights
# kept as highlights. Iron's is the one the iron pick has always had.
HEADS = {
    "iron": lambda k: (120 * k + 20, 132 * k + 18, 150 * k + 22),
    "gold": lambda k: (235 * k + 20, 190 * k + 14, 50 * k + 10),
    "diamond": lambda k: (80 * k + 16, 215 * k + 22, 210 * k + 24),
    "wood": lambda k: (150 * k + 10, 100 * k + 8, 52 * k + 6),
}


def recolour(kind, head):
    """tools_<kind>.png with its grey head in another material: the stone
    tools' pictures are every other material's."""
    # The hoe's stone picture is the one farm_hoe.png was before the wooden
    # hoe took that name.
    src = "tools_stone_hoe.png" if kind == "hoe" else f"tools_{kind}.png"
    img = Image.open(os.path.join(OUT, src)).convert("RGBA")
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = img.getpixel((x, y))
            grey = a > 0 and abs(r - g) < 12 and abs(g - b) < 14 and r > 60
            if grey:
                img.putpixel((x, y), tuple(int(c) for c in HEADS[head](r / 255.0)) + (a,))
    return img


def main():
    for name, (rows, colours) in SHAPES.items():
        draw(rows, colours).save(os.path.join(OUT, name + ".png"))
    for kind in ("pick", "axe", "shovel", "sword", "hoe"):
        for head in HEADS:
            name = ("farm_hoe" if kind == "hoe" else f"tools_wood_{kind}") if head == "wood" \
                else f"furnace_{head}_{kind}"
            recolour(kind, head).save(os.path.join(OUT, name + ".png"))


if __name__ == "__main__":
    main()
