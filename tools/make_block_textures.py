#!/usr/bin/env python3
"""Pictures for mods/bed, mods/compact and mods/cinnabar, drawn by rule into
assets/textures on the 16 grid and doubled to 32 px.

A storage block is its colour inside a bevelled frame, lit top-left, the way
The classic block game draws its blocks of iron and gold; the bed is a red blanket with a
white pillow over planks; dust is a red cross on nothing, dark when off and
bright when on; the lamp a glass grid over a warm glow; a cinnabar torch a
stick with a red head, dark when off; the repeater and the button stone.

From a fixed seed, so running it again makes the same files.

    python3 tools/make_block_textures.py
"""
import base64
import json
import os
import random
import struct

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "textures")
MOD = os.path.join(os.path.dirname(__file__), "..", "mods", "cinnabar")


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c)


def save(img, name):
    img.resize((32, 32), Image.NEAREST).save(os.path.join(OUT, name + ".png"))


def compact(name, colour, rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            k = rng.uniform(0.92, 1.05)
            if x == 0 or y == 0:
                k = 1.3
            elif x == 15 or y == 15:
                k = 0.6
            elif x in (1, 14) or y in (1, 14):
                k *= 0.85
            img.putpixel((x, y), shade(colour, k))
    # Coal's block is lumps of coal painted in Blockbench, not a frame; the
    # noise is still drawn so the seed gives the other pictures what it did.
    if name != "coal":
        save(img, f"compact_{name}")


def bed(rng):
    wood, red, white = (150, 108, 62), (170, 36, 36), (236, 236, 236)
    top = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            c = white if y < 5 and 1 <= x <= 14 else red
            k = 0.7 if x in (0, 15) else rng.uniform(0.9, 1.05)
            top.putpixel((x, y), shade(c, k))
    save(top, "bed_bed_top")
    side = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            # A box shows the picture's first rows at its BOTTOM, as many as it
            # is sixteenths tall: nine here, a frame under a blanket.
            c = wood if y < 5 else (red if y < 9 else wood)
            side.putpixel((x, y), shade(c, 0.75 if y == 5 else rng.uniform(0.85, 1.05)))
    save(side, "bed_bed_side")
    bottom = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            bottom.putpixel((x, y), shade(wood, 0.8 if y % 4 == 0 else rng.uniform(0.9, 1.05)))
    save(bottom, "bed_bed_bottom")


# Dust is a model and not a box: a box is drawn from the block atlas, where a
# see-through pixel comes out black. One flat quad a sixty-fourth over the
# floor, its picture beside it, cut out by its alpha -- the wheat's way.
def dust(name, colour, rng):
    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for i in range(16):
        for w in (7, 8):
            for x, y in ((i, w), (w, i)):
                img.putpixel((x, y), shade(colour, rng.uniform(0.8, 1.15)) + (255,))
    img.resize((64, 64), Image.NEAREST).save(os.path.join(MOD, name + ".png"))
    save(img, "cinnabar_" + name)   # what the slot shows, if it ever does

    h = 1.0 / 64
    verts = [((0, h, 0), (0, 0)), ((0, h, 1), (0, 1)), ((1, h, 1), (1, 1)), ((1, h, 0), (1, 0))]
    idx = [0, 1, 2, 0, 2, 3]
    vbytes = b"".join(struct.pack("<3f3f2f", *p, 0, 1, 0, *uv) for p, uv in verts)
    ibytes = b"".join(struct.pack("<H", i) for i in idx)
    buf = vbytes + ibytes + b"\x00" * ((4 - len(ibytes) % 4) % 4)
    gltf = {
        "asset": {"version": "2.0", "generator": "cubes-and-portals tools/make_block_textures.py"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"name": name, "primitives": [
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3,
             "material": 0}]}],
        "materials": [{"name": name, "doubleSided": True, "alphaMode": "MASK",
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                "metallicFactor": 0, "roughnessFactor": 1}}],
        "textures": [{"source": 0}],
        "images": [{"uri": name + ".png"}],
        "buffers": [{"byteLength": len(buf), "uri": "data:application/octet-stream;base64,"
                     + base64.b64encode(buf).decode()}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(vbytes), "byteStride": 32,
             "target": 34962},
            {"buffer": 0, "byteOffset": len(vbytes), "byteLength": len(ibytes), "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 4,
             "type": "VEC3", "min": [0, h, 0], "max": [1, h, 1]},
            {"bufferView": 0, "byteOffset": 12, "componentType": 5126, "count": 4, "type": "VEC3"},
            {"bufferView": 0, "byteOffset": 24, "componentType": 5126, "count": 4, "type": "VEC2"},
            {"bufferView": 1, "componentType": 5123, "count": 6, "type": "SCALAR"},
        ],
    }
    with open(os.path.join(MOD, name + ".gltf"), "w") as f:
        f.write(json.dumps(gltf, indent=1) + "\n")


def stone_wood(name, rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            c = (140, 102, 60) if 5 <= x <= 10 else (128, 128, 128)
            img.putpixel((x, y), shade(c, rng.uniform(0.85, 1.1)))
    save(img, name)


def lamp(name, glow, rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            frame = x in (0, 15) or y in (0, 15) or x == 7 or y == 7
            c = (70, 44, 30) if frame else glow
            img.putpixel((x, y), shade(c, rng.uniform(0.85, 1.1)))
    save(img, name)


def torch(name, head, rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            # Ten sixteenths tall, first row at the bottom: the head is rows 6-9.
            c = head if 6 <= y <= 9 else (120, 86, 50)
            img.putpixel((x, y), shade(c, rng.uniform(0.85, 1.1)))
    save(img, name)


def stone(name, tint, rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            k = 0.7 if x in (0, 15) or y in (0, 15) else rng.uniform(0.88, 1.08)
            img.putpixel((x, y), shade(tint, k))
    save(img, name)


def cane(rng):
    """Sugar cane: three stalks with joints, cut out by alpha, on the wheat's
    crossed quads a whole cell tall; and the same picture for its slot."""
    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for x in (3, 8, 12):
        for y in range(16):
            c = (150, 210, 110) if y % 5 else (110, 160, 80)
            for dx in (0, 1):
                img.putpixel((x + dx, y), shade(c, rng.uniform(0.85, 1.1)) + (255,))
        img.putpixel((x + 2, 4), (90, 170, 70, 255))
        img.putpixel((x - 1, 10), (90, 170, 70, 255))
    save(img, "cane_sugar_cane")
    out = os.path.join(os.path.dirname(__file__), "..", "mods", "cane")
    img.resize((64, 64), Image.NEAREST).save(os.path.join(out, "sugar_cane.png"))
    quads = []
    for a in (0.25, 0.75):
        quads.append(((0, 0, a), (1, 0, a), (1, 1, a), (0, 1, a), (0, 0, 1)))
        quads.append(((a, 0, 1), (a, 0, 0), (a, 1, 0), (a, 1, 1), (1, 0, 0)))
    verts, idx = [], []
    for p0, p1, p2, p3, nrm in quads:
        b = len(verts)
        for p, uv in ((p0, (0, 1)), (p1, (1, 1)), (p2, (1, 0)), (p3, (0, 0))):
            verts.append((p, nrm, uv))
        idx += [b, b + 1, b + 2, b, b + 2, b + 3]
    vb = b"".join(struct.pack("<3f3f2f", *p, *n, *uv) for p, n, uv in verts)
    ib = b"".join(struct.pack("<H", i) for i in idx)
    buf = vb + ib + b"\x00" * ((4 - len(ib) % 4) % 4)
    gltf = {
        "asset": {"version": "2.0", "generator": "cubes-and-portals tools/make_block_textures.py"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "sugar_cane", "mesh": 0}],
        "meshes": [{"name": "sugar_cane", "primitives": [
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3,
             "material": 0}]}],
        "materials": [{"name": "cane", "doubleSided": True, "alphaMode": "MASK",
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                "metallicFactor": 0, "roughnessFactor": 1}}],
        "textures": [{"source": 0}],
        "images": [{"uri": "sugar_cane.png"}],
        "buffers": [{"byteLength": len(buf), "uri": "data:application/octet-stream;base64,"
                     + base64.b64encode(buf).decode()}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(vb), "byteStride": 32, "target": 34962},
            {"buffer": 0, "byteOffset": len(vb), "byteLength": len(ib), "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": len(verts),
             "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 1]},
            {"bufferView": 0, "byteOffset": 12, "componentType": 5126, "count": len(verts), "type": "VEC3"},
            {"bufferView": 0, "byteOffset": 24, "componentType": 5126, "count": len(verts), "type": "VEC2"},
            {"bufferView": 1, "componentType": 5123, "count": len(idx), "type": "SCALAR"},
        ],
    }
    with open(os.path.join(out, "sugar_cane.gltf"), "w") as f:
        f.write(json.dumps(gltf, indent=1) + "\n")


def flat_item(name, rows, palette, rng):
    """A 16x16 icon from rows of letters, each a colour of `palette`."""
    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in palette:
                img.putpixel((x, y), shade(palette[ch], rng.uniform(0.9, 1.08)) + (255,))
    save(img, name)


def gravel(rng):
    """The classic block game's gravel: pebbles of four greys and a brown packed on dark
    grit, each lit on its top-left pixel, wrapping at the edges so the block
    tiles."""
    img = Image.new("RGB", (16, 16), (86, 80, 78))
    for y in range(16):
        for x in range(16):
            img.putpixel((x, y), shade((86, 80, 78), rng.uniform(0.85, 1.1)))
    stones = [(150, 146, 144), (124, 120, 120), (168, 160, 156), (108, 104, 104), (136, 120, 106)]
    for _ in range(34):
        c = rng.choice(stones)
        x0, y0 = rng.randrange(16), rng.randrange(16)
        w, h = rng.choice(((2, 2), (3, 2), (2, 3), (3, 3), (2, 1)))
        for dy in range(h):
            for dx in range(w):
                k = 1.18 if (dx, dy) == (0, 0) else 0.8 if (dx, dy) == (w - 1, h - 1) else rng.uniform(0.95, 1.05)
                img.putpixel(((x0 + dx) % 16, (y0 + dy) % 16), shade(c, k))
    save(img, "gravel_gravel")


def main():
    rng = random.Random(7331)
    for name, colour in (("coal", (34, 32, 36)), ("copper", (196, 110, 72)),
                         ("iron", (216, 216, 216)), ("gold", (246, 208, 60)),
                         ("lapis", (34, 66, 170)), ("cinnabar", (170, 20, 16)),
                         ("diamond", (100, 220, 214)), ("emerald", (40, 190, 90))):
        compact(name, colour, rng)
    bed(rng)
    dust("dust", (110, 16, 12), rng)
    dust("dust_on", (250, 40, 20), rng)
    stone_wood("cinnabar_lever", rng)
    stone_wood("cinnabar_lever_on", rng)
    lamp("cinnabar_lamp", (176, 112, 60), rng)
    lamp("cinnabar_lamp_on", (255, 220, 150), rng)
    torch("cinnabar_torch", (255, 60, 30), rng)
    torch("cinnabar_torch_off", (90, 25, 20), rng)
    stone("cinnabar_repeater", (150, 150, 150), rng)
    stone("cinnabar_repeater_on", (170, 140, 140), rng)
    stone("cinnabar_button", (128, 128, 128), rng)
    stone("cinnabar_button_on", (110, 110, 110), rng)
    torch("torches_torch", (255, 210, 90), rng)
    cane(rng)
    flat_item("cane_paper", [
        "................", "...##########...", "...#########o...", "...##########...",
        "...##########...", "...#########o...", "...##########...", "...##########...",
        "...#########o...", "...##########...", "...##########...", "...##########...",
    ], {"#": (236, 236, 226), "o": (200, 200, 190)}, rng)
    flat_item("cane_book", [
        "................", "..bbbbbbbbbb....", "..bppppppppbb...", "..bbbbbbbbbbb...",
        "..bbbbbbbbbbb...", "..bbbbgggbbbb...", "..bbbbgggbbbb...", "..bbbbbbbbbbb...",
        "..bbbbbbbbbbb...", "..bbbbbbbbbbb...", "..bppppppppbb...", "..bbbbbbbbbb....",
    ], {"b": (120, 70, 40), "p": (236, 236, 226), "g": (220, 180, 60)}, rng)
    flat_item("animals_leather", [
        "................", "...##......##...", "..############..", "..############..",
        "...##########...", "...##########...", "...##########...", "...##########...",
        "..############..", "..############..", "...##......##...",
    ], {"#": (150, 90, 50)}, rng)
    stone("enchant_table", (40, 24, 44), rng)
    table_top = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            c = (170, 30, 40) if 2 <= x <= 13 and 2 <= y <= 13 else (40, 24, 44)
            if (x, y) in ((1, 1), (14, 1), (1, 14), (14, 14)):
                c = (100, 220, 214)
            table_top.putpixel((x, y), shade(c, rng.uniform(0.9, 1.08)))
    save(table_top, "enchant_table_top")

    stone("cinnabar_comparator", (140, 140, 150), rng)
    stone("cinnabar_comparator_on", (170, 140, 150), rng)
    for name, tint in (("piston", (176, 136, 84)), ("sticky_piston", (120, 170, 90))):
        for suffix in ("", "_on", "_head"):
            face = Image.new("RGB", (16, 16))
            for y in range(16):
                for x in range(16):
                    wood = y < 4                     # the face it pushes with
                    c = tint if wood else (120, 120, 120)
                    k = 0.7 if x in (0, 15) or y in (0, 15) else rng.uniform(0.88, 1.08)
                    face.putpixel((x, y), shade(c, k))
            save(face, f"cinnabar_{name}{suffix}")
    for name, tint in (("observer", (100, 100, 110)), ("observer_on", (150, 90, 100)),
                       ("hopper_locked", (70, 60, 60))):   # cinnabar_hopper.png is the icon (make_held_models.py)
        stone("cinnabar_" + name, tint, rng)
    gravel(random.Random(4242))

main()
