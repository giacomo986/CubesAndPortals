#!/usr/bin/env python3
"""mods/circuits: the tops of the gates (a board, the name, an arrow to the output) and
the icons of the wires. A wire's plain file is its icon, so its block has flat faces of its own.

    python3 tools/make_circuits_textures.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

from make_item_textures import draw

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "textures")
LABELS = {
    "buffer": "BUF", "not": "NOT", "or": "OR", "nor": "NOR", "and": "AND", "nand": "NAND", "xor": "XOR",
    "xnor": "XNOR", "rs_latch": "RS", "toggle": "T", "d_latch": "D", "pulse": "PLS", "timer": "TIM",
    "counter": "CNT", "random": "RND", "light": "LUX",
}
FONT = ImageFont.load_default(size=11)

for name, label in LABELS.items():
    for on in (False, True):
        board = (62, 92, 72) if not on else (70, 120, 80)
        ink = (225, 235, 225) if not on else (255, 240, 120)
        arrow = (120, 30, 24) if not on else (255, 70, 40)
        img = Image.new("RGB", (32, 32), board)
        d = ImageDraw.Draw(img)
        d.rectangle((0, 0, 31, 31), outline=(40, 62, 50))
        # The output is the top edge of the picture.
        d.polygon([(16, 2), (11, 8), (21, 8)], fill=arrow)
        w = d.textlength(label, font=FONT)
        d.text(((32 - w) / 2, 13), label, fill=ink, font=FONT)
        img.save(os.path.join(OUT, f"circuits_gate_{'on_' if on else ''}{name}_top.png"))

WIRE = [
    "................",
    "..........ccc.tt",
    ".........chhcctt",
    "........ch.cc...",
    ".......cc.......",
    ".......ch.......",
    "........cc......",
    ".........ch.....",
    "..........cc....",
    "..........hc....",
    ".........cc.....",
    "........cc......",
    "...cc.hc........",
    "tt.cchc.........",
    "tt..cc..........",
    "................",
]
RISER = [
    "......hcc.......",
    "......hcc.......",
    "......hcc.......",
    ".....ccccc......",
    "......hcc.......",
    "......hcc.......",
    ".....ccccc......",
    "......hcc.......",
    "......hcc.......",
    ".....ccccc......",
    "......hcc.......",
    "......hcc.......",
    "...ttttttttt....",
    "..tttttttttt....",
    "................",
    "................",
]
CABLE = [
    "................",
    "...........tttt.",
    ".........rrrr.t.",
    ".......ggggyy.t.",
    "......bbbyyy....",
    ".....rrrggg.....",
    "....rrgggbb.....",
    "....ggbbbyy.....",
    "....bbyyyrr.....",
    "....yyrrrgg.....",
    ".....rrggg......",
    "...t.ggbb.......",
    "...t.bbb........",
    ".tttt...........",
    "................",
    "................",
]
COPPER = (235, 150, 70)
COLOURS = {
    "white": (234, 236, 236), "orange": (240, 118, 20), "magenta": (190, 68, 180),
    "light_blue": (58, 176, 218), "yellow": (248, 198, 40), "lime": (112, 186, 26), "pink": (238, 142, 172),
    "gray": (62, 68, 72), "light_gray": (142, 142, 136), "cyan": (22, 138, 146), "purple": (122, 42, 172),
    "blue": (52, 56, 158), "brown": (114, 72, 40), "green": (84, 110, 28), "red": (160, 38, 34),
    "black": (20, 20, 24),
}


def lift(c):
    return tuple(min(255, int(v + (255 - v) * 0.5)) for v in c)


def wire(name, rows, colour, colours=None):
    draw(rows, colours or {"c": colour, "h": lift(colour), "t": COPPER}).save(os.path.join(OUT, f"circuits_{name}.png"))
    flat = Image.new("RGB", (32, 32), colour)
    for face in ("top", "bottom", "side"):
        flat.save(os.path.join(OUT, f"circuits_{name}_{face}.png"))


ALLOY = (130, 40, 30)
wire("alloy", WIRE, ALLOY, {"c": (205, 95, 60), "h": (255, 195, 150), "t": COPPER})
wire("alloy_riser", RISER, ALLOY)
wire("cable", CABLE, (70, 70, 80), {"r": (200, 50, 40), "g": (90, 170, 60), "b": (60, 90, 200), "y": (240, 200, 50),
                                     "t": COPPER})
for colour_name, rgb in COLOURS.items():
    wire(f"wire_{colour_name}", WIRE, rgb)

INGOT = [
    "................",
    "................",
    "................",
    "................",
    ".....hhhhhhh....",
    "....hHHHHHHcc...",
    "...hHHHHHHcccc..",
    "..hccccccccccd..",
    "..cccccccccccd..",
    "..cccccccccccd..",
    "..dddddddddddd..",
    "................",
    "................",
    "................",
    "................",
    "................",
]
BOARD = [
    "................",
    "................",
    "..t..t..t..t....",
    ".bbbbbbbbbbbbbb.",
    ".bggggggggggggb.",
    ".bgggggcccgggcb.",
    ".bgggggccgggggb.",
    ".bggcggggggcggb.",
    ".bggggggcggggcb.",
    ".bgggccggggggcb.",
    ".bggggggggggggb.",
    ".bbbbbbbbbbbbbb.",
    "..t..t..t..t....",
    "................",
    "................",
    "................",
]
draw(INGOT, {"h": (250, 150, 110), "H": (225, 110, 80), "c": (170, 60, 44), "d": (110, 32, 26)}).save(
    os.path.join(OUT, "circuits_alloy_ingot.png"))
draw(BOARD, {"b": (40, 70, 52), "g": (62, 104, 74), "c": (220, 190, 90), "t": COPPER}).save(
    os.path.join(OUT, "circuits_plate.png"))
