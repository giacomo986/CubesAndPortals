#!/usr/bin/env python3
"""mods/farm's pictures and its wheat, drawn by rule.

Into assets/textures: tilled soil (its top in furrows, its sides dirt), and the
hoe, the wheat and the bread as 16x16 masks doubled to 32 px. Into mods/farm:
wheat in four stages, each a model of four upright quads crossed in a '#' --
The classic block game's crop shape -- with its picture beside it, cut out by its alpha.
The pictures of the stages are 16 px blown up to 64 with no smoothing, because
a model's texture is filtered linearly and 16 px of stalk would blur to a smear.

From a fixed seed, so running it again makes the same files.

    python3 tools/make_farm_assets.py
"""
import base64
import json
import os
import random
import struct

from PIL import Image

ROOT = os.path.join(os.path.dirname(__file__), "..")
TEX = os.path.join(ROOT, "assets", "textures")
MOD = os.path.join(ROOT, "mods", "farm")


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c)


def soil(rng):
    side = Image.new("RGB", (16, 16))
    top = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            side.putpixel((x, y), shade((134, 96, 67), rng.uniform(0.8, 1.1)))
            # Furrows across x: a dark trough every fourth row, wet soil between.
            k = 0.6 if y % 4 == 0 else (1.1 if y % 4 == 1 else 1.0)
            top.putpixel((x, y), shade((92, 62, 40), k * rng.uniform(0.85, 1.1)))
    side.resize((32, 32), Image.NEAREST).save(os.path.join(TEX, "farm_soil.png"))
    top.resize((32, 32), Image.NEAREST).save(os.path.join(TEX, "farm_soil_top.png"))


MASKS = {
    "hoe": [
        "................",
        "....#######.....",
        "...########.....",
        "...##...##......",
        ".......##.......",
        "......##........",
        ".....##.........",
        "....##..........",
        "...##...........",
        "..##............",
        ".##.............",
    ],
    "wheat": [
        ".....#...#......",
        "....###.###..#..",
        "....###.###.###.",
        ".#..###.###.###.",
        "###..#...#..###.",
        "###..#...#...#..",
        "###..#...#...#..",
        ".#...#...#...#..",
        ".#...#...#..#...",
        "..#..#..#..#....",
        "...#.#..#.#.....",
        "....###.##......",
        ".....######.....",
        ".....#.##.#.....",
        "....#..##..#....",
        "................",
    ],
    "bread": [
        "................",
        "................",
        "................",
        "................",
        ".....######.....",
        "...##########...",
        "..############..",
        ".##############.",
        ".##############.",
        ".##############.",
        "..############..",
        "................",
    ],
}


def item(name, mask, colour, flecks=None, rng=None, handle=None):
    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    rows = mask + ["." * 16] * (16 - len(mask))
    on = lambda x, y: 0 <= y < 16 and 0 <= x < 16 and rows[y][x] == "#"
    for y in range(16):
        for x in range(16):
            if not on(x, y):
                continue
            edge = not (on(x - 1, y) and on(x + 1, y) and on(x, y - 1) and on(x, y + 1))
            light = not on(x, y - 1) or not on(x - 1, y)
            k = 0.6 if edge and not light else (1.15 if edge else 1.0)
            c = handle if handle and y >= 4 else colour
            if flecks and not edge and rng.random() < flecks[1]:
                c = flecks[0]
            img.putpixel((x, y), shade(c, k) + (255,))
    img.resize((32, 32), Image.NEAREST).save(os.path.join(TEX, name + ".png"))


# A stage's picture: stalks up from the bottom to `h` of 16, the colour going
# from green to gold, and ears on top once it is grown.
def stage(n, rng):
    h = (5, 8, 11, 15)[n]
    stalk = [(80, 170, 60), (110, 170, 50), (160, 160, 50), (190, 160, 60)][n]
    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    for x in range(1, 16, 3):
        top = 16 - h + rng.randint(0, 2)
        for y in range(top, 16):
            img.putpixel((x, y), shade(stalk, rng.uniform(0.75, 1.1)) + (255,))
        if n == 3:
            # An ear: the top of the stalk gold, a grain either side every
            # other pixel down it.
            for y in range(top, top + 5):
                img.putpixel((x, y), shade((220, 190, 90), rng.uniform(0.8, 1.1)) + (255,))
                dx = -1 if y % 2 else 1
                if 0 <= x + dx < 16 and y > top:
                    img.putpixel((x + dx, y), shade((200, 170, 80), rng.uniform(0.8, 1.1)) + (255,))
    img = img.resize((64, 64), Image.NEAREST)
    img.save(os.path.join(MOD, f"wheat_{n}.png"))
    return h / 16.0


def crop_gltf(n, height):
    # Four quads, two along x at z 0.25 and 0.75 and two along z, the whole
    # cell wide: the '#' a crop is drawn as. One material, both sides drawn.
    quads = []
    for a in (0.25, 0.75):
        quads.append(((0, 0, a), (1, 0, a), (1, height, a), (0, height, a), (0, 0, 1)))
        quads.append(((a, 0, 1), (a, 0, 0), (a, height, 0), (a, height, 1), (1, 0, 0)))
    verts, idx = [], []
    for p0, p1, p2, p3, nrm in quads:
        base = len(verts)
        for p, uv in ((p0, (0, 1)), (p1, (1, 1)), (p2, (1, 1 - height)), (p3, (0, 1 - height))):
            verts.append((p, nrm, uv))
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    vbytes = b"".join(struct.pack("<3f3f2f", *p, *nrm, *uv) for p, nrm, uv in verts)
    ibytes = b"".join(struct.pack("<H", i) for i in idx)
    buf = vbytes + ibytes + b"\x00" * ((4 - len(ibytes) % 4) % 4)
    gltf = {
        "asset": {"version": "2.0", "generator": "cubes-and-portals tools/make_farm_assets.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": f"wheat_{n}", "mesh": 0}],
        "meshes": [{"name": f"wheat_{n}", "primitives": [
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3,
             "material": 0}]}],
        "materials": [{"name": "wheat", "doubleSided": True, "alphaMode": "MASK",
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                "metallicFactor": 0, "roughnessFactor": 1}}],
        "textures": [{"source": 0}],
        "images": [{"uri": f"wheat_{n}.png"}],
        "buffers": [{"byteLength": len(buf),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(buf).decode()}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(vbytes), "byteStride": 32,
             "target": 34962},
            {"buffer": 0, "byteOffset": len(vbytes), "byteLength": len(ibytes), "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": len(verts),
             "type": "VEC3", "min": [0, 0, 0], "max": [1, height, 1]},
            {"bufferView": 0, "byteOffset": 12, "componentType": 5126, "count": len(verts),
             "type": "VEC3"},
            {"bufferView": 0, "byteOffset": 24, "componentType": 5126, "count": len(verts),
             "type": "VEC2"},
            {"bufferView": 1, "componentType": 5123, "count": len(idx), "type": "SCALAR"},
        ],
    }
    with open(os.path.join(MOD, f"wheat_{n}.gltf"), "w") as f:
        f.write(json.dumps(gltf, indent=1) + "\n")


def main():
    rng = random.Random(5150)
    os.makedirs(MOD, exist_ok=True)
    soil(rng)
    item("farm_hoe", MASKS["hoe"], (150, 150, 150), handle=(140, 100, 60))
    item("farm_wheat", MASKS["wheat"], (214, 180, 80), ((240, 214, 120), 0.2), rng)
    item("farm_bread", MASKS["bread"], (196, 132, 60), ((230, 180, 100), 0.2), rng)
    for n in range(4):
        crop_gltf(n, stage(n, rng))


main()
