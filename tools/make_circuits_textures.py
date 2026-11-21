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


# The blocks that came with walls, buses and chips: a top that says what it is,
# flat sides, and the top as the icon (none of them is a cube worth drawing).
def tile(stem, board, edge, label, ink, arrow=None, sub=None):
    img = Image.new("RGB", (32, 32), board)
    d = ImageDraw.Draw(img)
    d.rectangle((0, 0, 31, 31), outline=edge)
    if arrow:
        d.polygon([(16, 2), (11, 8), (21, 8)], fill=arrow)
    if sub:
        w = d.textlength(sub, font=SMALL)
        d.text(((32 - w) / 2, 21), sub, fill=ink, font=SMALL)
    w = d.textlength(label, font=FONT)
    d.text(((32 - w) / 2, 11 if sub else 13), label, fill=ink, font=FONT)
    img.save(os.path.join(OUT, f"circuits_{stem}_top.png"))
    flat = Image.new("RGB", (32, 32), edge)
    for face in ("bottom", "side"):
        flat.save(os.path.join(OUT, f"circuits_{stem}_{face}.png"))
    return img


SMALL = ImageFont.load_default(size=8)
# A bus gate: blue where a gate is green, the arrow to its output, "BUS" under the name.
for name in ("and", "or", "xor", "not"):
    for on in (False, True):
        img = tile(f"bus_gate_{'on_' if on else ''}{name}", (52, 72, 104) if not on else (80, 110, 150), (32, 44, 70),
                   name.upper(), (225, 230, 245) if not on else (255, 240, 120),
                   (120, 30, 24) if not on else (255, 70, 40), "BUS")
        if not on:
            img.save(os.path.join(OUT, f"circuits_bus_gate_{name}.png"))
for stem, board, label in (("pin_in", (60, 140, 70), "IN"), ("pin_in_on", (120, 230, 120), "IN"),
                           ("pin_out", (140, 70, 50), "OUT"), ("pin_out_on", (250, 140, 90), "OUT")):
    img = tile(stem, board, tuple(c // 2 for c in board), label, (250, 250, 240))
    if not stem.endswith("_on"):
        img.save(os.path.join(OUT, f"circuits_{stem}.png"))
for stem, board in (("chip_block", (40, 44, 50)), ("chip_block_on", (60, 66, 80))):
    img = tile(stem, board, (24, 26, 30), "IC", (230, 230, 230) if stem == "chip_block" else (255, 240, 120))
    d = ImageDraw.Draw(img)
    for i in range(4, 29, 6):    # legs all round, as a chip has
        for x, y in ((i, 0), (i, 30), (0, i), (30, i)):
            d.rectangle((x, y, x + 1, y + 1), fill=(200, 200, 205))
    img.save(os.path.join(OUT, f"circuits_{stem}_top.png"))
    if stem == "chip_block":
        img.save(os.path.join(OUT, "circuits_chip.png"))
        blank = img.copy()
        ImageDraw.Draw(blank).rectangle((8, 10, 23, 21), fill=(40, 44, 50))
        blank.save(os.path.join(OUT, "circuits_blank_chip.png"))
bench = tile("chip_bench", (120, 100, 80), (80, 62, 46), "IC", (40, 30, 20), sub="BENCH")
bench.save(os.path.join(OUT, "circuits_chip_bench.png"))
