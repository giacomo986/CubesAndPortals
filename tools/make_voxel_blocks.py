#!/usr/bin/env python3
"""Pictures and plant models for the classic block-game blocks of mods/rocks, mods/plants,
mods/woods, mods/colors, mods/building and mods/dungeons, drawn by rule.

Into assets/textures: every cube face on the 16 grid doubled to 32 px, the
icons of the items, and pack.txt, which points the blocks that share a picture
(a fence and the planks it is made of) at one file instead of copying it. Into
each mod: its plants, flowers and other flat things as glTF quads -- crossed in
an X like the classic block game's, lying flat, or against a wall -- each with its picture
beside it blown up to 64 px with no smoothing and cut out by its alpha.

From fixed seeds, so running it again makes the same files.

    python3 tools/make_voxel_blocks.py
"""
import base64
import json
import os
import random
import struct

from PIL import Image

ROOT = os.path.join(os.path.dirname(__file__), "..")
TEX = os.path.join(ROOT, "assets", "textures")
PACK = []   # pack.txt lines


def mod(name):
    d = os.path.join(ROOT, "mods", name)
    os.makedirs(d, exist_ok=True)
    return d


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c[:3])


def save(img, name):
    img.resize((32, 32), Image.NEAREST).save(os.path.join(TEX, name + ".png"))


def alias(node, file, face=None):
    PACK.append(f"{node}{':' + face if face else ''} = {file}.png")


# ---------------------------------------------------------------- cube faces

def noise(base, rng, lo=0.85, hi=1.1, edge=None):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            k = rng.uniform(lo, hi)
            if edge and (x in (0, 15) or y in (0, 15)):
                k *= edge
            img.putpixel((x, y), shade(base, k))
    return img


def speckle(base, flecks, rng, n=40):
    img = noise(base, rng, 0.9, 1.08)
    for _ in range(n):
        c = rng.choice(flecks)
        x, y = rng.randrange(16), rng.randrange(16)
        img.putpixel((x, y), shade(c, rng.uniform(0.9, 1.1)))
        if rng.random() < 0.4:
            img.putpixel(((x + 1) % 16, y), shade(c, rng.uniform(0.85, 1.0)))
    return img


def cobble(base, rng, moss=None):
    """Irregular stones in dark mortar: each pixel belongs to its nearest of a
    dozen points, wrapping so the block tiles, and the seams are where two
    stones are nearly as near."""
    pts = [(rng.uniform(0, 16), rng.uniform(0, 16), rng.uniform(0.82, 1.15)) for _ in range(11)]
    mossy = [moss is not None and rng.random() < moss[1] for _ in pts]
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            ds = []
            for i, (px, py, _) in enumerate(pts):
                dx = min(abs(x + 0.5 - px), 16 - abs(x + 0.5 - px))
                dy = min(abs(y + 0.5 - py), 16 - abs(y + 0.5 - py))
                ds.append((dx * dx + dy * dy, i))
            ds.sort()
            i = ds[0][1]
            seam = ds[1][0] ** 0.5 - ds[0][0] ** 0.5 < 0.9
            c = moss[0] if mossy[i] else base
            k = 0.5 if seam else pts[i][2] * rng.uniform(0.92, 1.06)
            img.putpixel((x, y), shade(c, k))
    return img


def bricks(base, mortar, rng, h=4, w=8):
    img = Image.new("RGB", (16, 16), mortar)
    for row in range(16 // h):
        off = (w // 2) * (row % 2)
        for x in range(16):
            for y in range(row * h, row * h + h - 1):
                if (x + off) % w == w - 1:
                    continue
                k = rng.uniform(0.88, 1.08) * (1.1 if y == row * h else 1.0)
                img.putpixel((x, y), shade(base, k))
    return img


def stripes(base, rng, horizontal=True, dark=0.8):
    img = noise(base, rng, 0.9, 1.06)
    for i in range(0, 16, rng.choice((3, 4))):
        for j in range(16):
            if rng.random() < 0.75:
                xy = (j, i) if horizontal else (i, j)
                img.putpixel(xy, shade(base, dark * rng.uniform(0.9, 1.05)))
    return img


def log_side(bark, rng, streak=0.72, marks=None):
    img = stripes(bark, rng, horizontal=False, dark=streak)
    if marks:
        for _ in range(10):
            x, y = rng.randrange(16), rng.randrange(16)
            for dx in range(rng.choice((2, 3))):
                img.putpixel(((x + dx) % 16, y), shade(marks, rng.uniform(0.9, 1.05)))
    return img


def log_top(bark, wood, rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            r = max(abs(x - 7.5), abs(y - 7.5))
            if r > 6.5:
                c = shade(bark, rng.uniform(0.85, 1.05))
            else:
                c = shade(wood, (0.82 if int(r) % 2 == 0 else 1.0) * rng.uniform(0.95, 1.05))
            img.putpixel((x, y), c)
    return img


def planks(wood, rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            k = rng.uniform(0.9, 1.06)
            if y % 4 == 3:
                k = 0.62
            elif (x + (y // 4) * 5) % 16 == 0:
                k = 0.7
            img.putpixel((x, y), shade(wood, k))
    return img


def leaves(green, rng):
    img = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            k = rng.uniform(0.7, 1.15) if rng.random() > 0.18 else 0.42
            img.putpixel((x, y), shade(green, k))
    return img


def framed(base, rng, inner=None):
    img = noise(base, rng, 0.92, 1.05)
    for i in range(16):
        for xy in ((i, 0), (i, 15), (0, i), (15, i)):
            img.putpixel(xy, shade(base, 0.7))
    if inner:
        for i in range(3, 13):
            for xy in ((i, 3), (i, 12), (3, i), (12, i)):
                img.putpixel(xy, shade(inner, 1.0))
    return img


# ---------------------------------------------------------------- cut-out pictures

def blank():
    return Image.new("RGBA", (16, 16), (0, 0, 0, 0))


def put(img, x, y, c, rng=None, k=1.0):
    if 0 <= x < 16 and 0 <= y < 16:
        kk = k * (rng.uniform(0.9, 1.1) if rng else 1.0)
        img.putpixel((x, y), shade(c, kk) + (255,))


def grass_tuft(green, rng, tall=12, blades=7):
    img = blank()
    for _ in range(blades):
        x = rng.randrange(1, 15)
        h = rng.randint(tall // 2, tall)
        lean = rng.choice((-1, 0, 1))
        for i in range(h):
            put(img, x + (lean * i) // 5, 15 - i, green, rng, 0.8 + 0.3 * i / h)
    return img


def fern(green, rng):
    img = blank()
    for stem, lean in ((7, 0), (4, -1), (11, 1)):
        for i in range(12):
            x = stem + (lean * i) // 4
            put(img, x, 15 - i, green, rng, 0.8)
            if i % 2 and i > 2:
                put(img, x - 1, 15 - i, green, rng)
                put(img, x + 1, 15 - i, green, rng)
    return img


def flower(petal, rng, centre=None, shape="round", stem=(60, 130, 50), height=10):
    img = blank()
    for i in range(height):
        put(img, 7, 15 - i, stem, rng)
    put(img, 6, 12, stem, rng)
    put(img, 8, 10, stem, rng)
    top = 15 - height
    if shape == "round":
        for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1), (-1, -1), (1, -1), (-1, 1), (1, 1)):
            put(img, 7 + dx, top + dy, petal, rng)
        put(img, 7, top, centre or petal, rng)
    elif shape == "tulip":
        for dx in (-1, 0, 1):
            for dy in (-2, -1, 0):
                put(img, 7 + dx, top + dy, petal, rng, 0.9 if dx else 1.05)
        put(img, 6, top - 3, petal, rng)
        put(img, 8, top - 3, petal, rng)
    elif shape == "ball":
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                if dx * dx + dy * dy <= 5:
                    put(img, 7 + dx, top + dy, petal, rng, 1.1 if (dx + dy) % 2 else 0.85)
    elif shape == "bells":
        for x, y in ((5, top), (9, top + 1), (6, top + 3), (9, top + 4)):
            put(img, x, y, petal, rng)
            put(img, x, y + 1, petal, rng, 0.85)
    elif shape == "orchid":
        for dx, dy in ((-2, 0), (2, 0), (-1, -1), (1, -1), (0, 1), (-1, 1), (1, 1), (0, -2)):
            put(img, 7 + dx, top + dy, petal, rng)
        put(img, 7, top, centre or petal, rng)
    elif shape == "cluster":
        for dx, dy in ((-2, 0), (2, 0), (0, -2), (0, 0), (-1, 2), (1, 2), (-2, -2), (2, -2)):
            put(img, 7 + dx, top + dy, petal, rng)
            put(img, 7 + dx, top + dy + 1, centre or petal, rng, 0.9)
    return img


def dead_bush(rng):
    img = blank()
    brown = (140, 100, 50)
    for x0, lean in ((7, 0), (5, -1), (9, 1), (6, -2), (8, 2)):
        for i in range(rng.randint(6, 11)):
            put(img, x0 + (lean * i) // 3, 15 - i, brown, rng)
    return img


def mushroom(cap, rng, spots=None):
    img = blank()
    for y in range(10, 16):
        put(img, 7, y, (220, 210, 190), rng)
        put(img, 8, y, (220, 210, 190), rng, 0.9)
    for y in range(6, 10):
        for x in range(4 + (9 - y) // 2 - 1, 12 - (9 - y) // 2 + 1):
            put(img, x, y, cap, rng)
    if spots:
        for x, y in ((6, 7), (9, 8), (8, 6)):
            put(img, x, y, spots, rng)
    return img


def cobweb(rng):
    img = blank()
    w = (235, 235, 240)
    for i in range(16):
        put(img, i, i, w)
        put(img, 15 - i, i, w)
        if i % 5 == 2:
            for j in range(16):
                if abs(j - 7.5) <= abs(i - 7.5) + 1 and rng.random() < 0.7:
                    put(img, j, i, w, None, 0.9)
                    put(img, i, j, w, None, 0.9)
    for i in range(16):
        put(img, 7, i, w)
        put(img, i, 7, w)
    return img


def cluster(rng):
    img = blank()
    for x0, h in ((7, 13), (4, 8), (11, 9), (6, 6), (9, 7)):
        for i in range(h):
            w = max(0, 1 - i // 5)
            for dx in range(-w, w + 1):
                put(img, x0 + dx, 15 - i, (190, 120, 230), rng, 1.2 if dx == 0 else 0.9)
    return img


def dripstone_spike(rng):
    img = blank()
    for y in range(16):
        w = y // 5
        for dx in range(-w, w + 1):
            put(img, 7 + dx, y, (150, 120, 100), rng)
    return img


def vine(rng):
    img = blank()
    for x0 in range(1, 16, 3):
        y = 0
        x = x0
        while y < 16:
            put(img, x, y, (60, 120, 40), rng, 0.8)
            if rng.random() < 0.5:
                put(img, x + 1, y, (70, 140, 50), rng)
            x += rng.choice((-1, 0, 1))
            y += 1
    return img


def lily_pad(rng):
    img = blank()
    for y in range(16):
        for x in range(16):
            dx, dy = x - 7.5, y - 7.5
            if dx * dx + dy * dy < 50 and not (dx > 0 and abs(dy) < 1.2):
                put(img, x, y, (50, 120, 40), rng, 0.8 if (x + y) % 5 == 0 else 1.0)
    return img


def rail(rng):
    img = blank()
    for y in range(1, 16, 3):
        for x in range(2, 14):
            put(img, x, y, (110, 80, 50), rng)
    for y in range(16):
        for x in (3, 12):
            put(img, x, y, (170, 170, 175), rng)
    return img


def icon(mask, colours, rng):
    """A small item picture from rows of characters."""
    img = blank()
    for y, row in enumerate(mask):
        for x, ch in enumerate(row):
            if ch in colours:
                put(img, x, y, colours[ch], rng)
    return img


def save_rgba(img, name):
    img.resize((32, 32), Image.NEAREST).save(os.path.join(TEX, name + ".png"))


# ---------------------------------------------------------------- plant models

def quads_gltf(path, png, quads, name):
    verts, idx = [], []
    for p0, p1, p2, p3, nrm in quads:
        base = len(verts)
        for p, uv in ((p0, (0, 1)), (p1, (1, 1)), (p2, (1, 0)), (p3, (0, 0))):
            verts.append((p, nrm, uv))
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    vbytes = b"".join(struct.pack("<3f3f2f", *p, *n, *uv) for p, n, uv in verts)
    ibytes = b"".join(struct.pack("<H", i) for i in idx)
    buf = vbytes + ibytes + b"\x00" * ((4 - len(ibytes) % 4) % 4)
    lo = [min(v[0][a] for v in verts) for a in range(3)]
    hi = [max(v[0][a] for v in verts) for a in range(3)]
    gltf = {
        "asset": {"version": "2.0", "generator": "cubes-and-portals tools/make_voxel_blocks.py"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"name": name, "primitives": [
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0}]}],
        "materials": [{"name": name, "doubleSided": True, "alphaMode": "MASK",
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                "metallicFactor": 0, "roughnessFactor": 1}}],
        "textures": [{"source": 0}],
        "images": [{"uri": png}],
        "buffers": [{"byteLength": len(buf),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(buf).decode()}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(vbytes), "byteStride": 32, "target": 34962},
            {"buffer": 0, "byteOffset": len(vbytes), "byteLength": len(ibytes), "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": len(verts),
             "type": "VEC3", "min": lo, "max": hi},
            {"bufferView": 0, "byteOffset": 12, "componentType": 5126, "count": len(verts), "type": "VEC3"},
            {"bufferView": 0, "byteOffset": 24, "componentType": 5126, "count": len(verts), "type": "VEC2"},
            {"bufferView": 1, "componentType": 5123, "count": len(idx), "type": "SCALAR"},
        ],
    }
    with open(path, "w") as f:
        f.write(json.dumps(gltf, indent=1) + "\n")


def plant_model(modname, name, img, shape="cross", flip=False):
    """`name`.gltf and .png in the mod; the icon in assets/textures."""
    d = mod(modname)
    pic = img.transpose(Image.FLIP_TOP_BOTTOM) if flip else img
    pic.resize((64, 64), Image.NEAREST).save(os.path.join(d, f"{name}.png"))
    save_rgba(img, f"{modname}_{name}")
    if shape == "cross":
        a, b = 0.15, 0.85
        quads = [((a, 0, a), (b, 0, b), (b, 1, b), (a, 1, a), (0.7, 0, -0.7)),
                 ((b, 0, a), (a, 0, b), (a, 1, b), (b, 1, a), (0.7, 0, 0.7))]
    elif shape == "flat":
        y = 0.02
        quads = [((0, y, 1), (1, y, 1), (1, y, 0), (0, y, 0), (0, 1, 0))]
    else:   # "wall": against the -Z face, as a wallmounted block is authored
        z = 0.04
        quads = [((1, 0, z), (0, 0, z), (0, 1, z), (1, 1, z), (0, 0, 1))]
    quads_gltf(os.path.join(d, f"{name}.gltf"), f"{name}.png", quads, name)


# ---------------------------------------------------------------- the content

WOODS = {
    #          bark              wood             leaves
    "birch":    ((216, 215, 210), (196, 176, 118), (104, 140, 64)),
    "spruce":   ((74, 54, 34),    (114, 84, 50),   (52, 88, 58)),
    "jungle":   ((88, 68, 30),    (160, 116, 80),  (48, 150, 32)),
    "acacia":   ((104, 98, 88),   (170, 92, 50),   (104, 128, 40)),
    "dark_oak": ((60, 46, 26),    (66, 43, 20),    (46, 96, 30)),
}
OAK = (160, 130, 80)   # timber:planks

COLOURS = {
    "white": (234, 236, 236), "orange": (240, 118, 20), "magenta": (190, 68, 180),
    "light_blue": (58, 176, 218), "yellow": (248, 198, 40), "lime": (112, 186, 26),
    "pink": (238, 142, 172), "gray": (62, 68, 72), "light_gray": (142, 142, 136),
    "cyan": (22, 138, 146), "purple": (122, 42, 172), "blue": (52, 56, 158),
    "brown": (114, 72, 40), "green": (84, 110, 28), "red": (160, 38, 34), "black": (20, 20, 24),
}

FLOWERS = {
    # name: (petal, centre, shape)
    "dandelion": ((250, 220, 40), None, "round"),
    "poppy": ((220, 30, 30), (40, 30, 20), "round"),
    "blue_orchid": ((40, 170, 230), None, "orchid"),
    "allium": ((190, 110, 230), None, "ball"),
    "azure_bluet": ((235, 235, 240), (240, 220, 80), "cluster"),
    "red_tulip": ((220, 50, 40), None, "tulip"),
    "orange_tulip": ((240, 130, 30), None, "tulip"),
    "white_tulip": ((240, 240, 235), None, "tulip"),
    "pink_tulip": ((240, 170, 200), None, "tulip"),
    "oxeye_daisy": ((240, 240, 235), (240, 200, 40), "orchid"),
    "cornflower": ((70, 110, 230), None, "orchid"),
    "lily_of_the_valley": ((245, 245, 245), None, "bells"),
}


def rocks(rng):
    stone = (128, 128, 132)
    save(cobble(stone, rng), "rocks_cobblestone")
    save(cobble(stone, rng, moss=((80, 120, 50), 0.35)), "rocks_mossy_cobblestone")
    save(speckle((134, 134, 136), [(110, 110, 112), (160, 160, 160)], rng, 60), "rocks_andesite")
    save(speckle((200, 200, 200), [(120, 120, 122), (240, 240, 240)], rng, 50), "rocks_diorite")
    save(speckle((150, 104, 86), [(186, 130, 110), (110, 70, 60), (200, 170, 160)], rng, 60), "rocks_granite")
    save(stripes((74, 74, 80), rng, True, 0.78), "rocks_deepslate")
    save(noise((74, 74, 80), rng, 0.85, 1.1), "rocks_deepslate_top")
    save(speckle((108, 110, 102), [(90, 92, 86), (130, 132, 120)], rng, 70), "rocks_tuff")
    save(speckle((222, 224, 220), [(200, 200, 200), (240, 240, 238)], rng, 40), "rocks_calcite")
    save(speckle((136, 90, 190), [(180, 130, 230), (100, 60, 150), (210, 170, 250)], rng, 70), "rocks_amethyst_block")
    save(stripes((142, 110, 92), rng, True, 0.82), "rocks_dripstone_block")
    sculk = speckle((14, 36, 42), [(10, 60, 70), (40, 200, 210)], rng, 30)
    save(sculk, "rocks_sculk")
    save(noise((160, 166, 178), rng, 0.93, 1.05), "rocks_clay")
    save(stripes((218, 204, 150), rng, True, 0.88), "rocks_sandstone")
    save(noise((222, 208, 156), rng, 0.93, 1.05), "rocks_sandstone_top")
    save(noise((210, 196, 140), rng, 0.93, 1.05), "rocks_sandstone_bottom")
    save(speckle((190, 102, 36), [(170, 86, 30), (210, 120, 50)], rng, 50), "rocks_red_sand")
    save(speckle((120, 86, 58), [(90, 64, 44), (150, 116, 86), (100, 100, 100)], rng, 60), "rocks_coarse_dirt")
    save(speckle((100, 70, 30), [(70, 50, 20), (130, 90, 40)], rng, 60), "rocks_podzol_top")
    pod = noise((134, 96, 67), rng)
    for x in range(16):
        for y in range(4):
            pod.putpixel((x, y), shade((100, 70, 30), rng.uniform(0.85, 1.1)))
    save(pod, "rocks_podzol_side")
    save(speckle((120, 100, 120), [(150, 120, 150), (100, 86, 100)], rng, 70), "rocks_mycelium_top")
    my = noise((134, 96, 67), rng)
    for x in range(16):
        for y in range(3):
            my.putpixel((x, y), shade((120, 100, 120), rng.uniform(0.85, 1.1)))
    save(my, "rocks_mycelium_side")
    ice = noise((150, 190, 250), rng, 0.93, 1.07)
    for i in range(3, 13):
        ice.putpixel((i, i - 2), (220, 235, 255))
    save(ice, "rocks_ice")
    save(bricks((122, 122, 124), (80, 80, 82), rng), "rocks_stone_bricks")
    sm = noise((160, 160, 162), rng, 0.96, 1.03)
    for i in range(16):
        sm.putpixel((i, 0), (120, 120, 122))
        sm.putpixel((i, 15), (120, 120, 122))
    save(sm, "rocks_smooth_stone")
    lava = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            k = rng.uniform(0.75, 1.15)
            lava.putpixel((x, y), (min(255, int(230 * k)), int(110 * k * rng.uniform(0.6, 1.2)), 10))
    save(lava, "rocks_lava")
    plant_model("rocks", "amethyst_cluster", cluster(rng))
    plant_model("rocks", "pointed_dripstone", dripstone_spike(rng), flip=True)   # hanging: wide at the top
    plant_model("rocks", "dripstone_up", dripstone_spike(rng))
    save_rgba(icon(["", "", "", "", "      aa", "     aAa", "    aAAa", "   aAAa", "   aAa", "   aa"],
                   {"a": (150, 90, 200), "A": (210, 170, 250)}, rng), "rocks_amethyst_shard")
    save_rgba(icon(["", "", "", "", "", "     cccc", "    ccCCcc", "    cCCccc", "    cccccc", "     cccc"],
                   {"c": (150, 158, 172), "C": (190, 196, 210)}, rng), "rocks_clay_ball")
    save_rgba(icon(["", "", "", "", "", "   bbbbbbbbbb", "  bBBBBBBBBbd", "  bBbbbbbbbbd", "  bbbbbbbbbbd",
                    "   dddddddddd"],
                   {"b": (170, 80, 60), "B": (205, 110, 85), "d": (120, 55, 40)}, rng), "rocks_brick")


def plants(rng):
    plant_model("plants", "tall_grass", grass_tuft((90, 150, 60), rng))
    plant_model("plants", "fern", fern((70, 130, 50), rng))
    for name, (petal, centre, shape) in FLOWERS.items():
        plant_model("plants", name, flower(petal, rng, centre, shape))
    plant_model("plants", "dead_bush", dead_bush(rng))
    plant_model("plants", "brown_mushroom", mushroom((150, 110, 80), rng))
    plant_model("plants", "red_mushroom", mushroom((200, 30, 30), rng, (245, 245, 245)))
    plant_model("plants", "cobweb", cobweb(rng))
    plant_model("plants", "vine", vine(rng), shape="wall")
    plant_model("plants", "lily_pad", lily_pad(rng), shape="flat")
    # A cactus: ribs down its sides, a flower-less top, and its spines.
    side = stripes((70, 130, 50), rng, False, 0.75)
    for _ in range(14):
        side.putpixel((rng.randrange(16), rng.randrange(16)), (230, 230, 200))
    save(side, "plants_cactus_side")
    top = framed((80, 140, 60), rng, (60, 110, 40))
    save(top, "plants_cactus_top")
    save(noise((70, 120, 50), rng), "plants_cactus_bottom")
    pumpkin = stripes((226, 130, 30), rng, False, 0.8)
    save(pumpkin, "plants_pumpkin_side")
    pt = noise((210, 120, 30), rng)
    for y in range(6, 10):
        for x in range(7, 9):
            pt.putpixel((x, y), shade((100, 80, 30), 1.0))
    save(pt, "plants_pumpkin_top")
    save(noise((210, 120, 30), rng), "plants_pumpkin_bottom")
    melon = stripes((120, 170, 40), rng, False, 0.7)
    save(melon, "plants_melon_side")
    save(framed((110, 160, 40), rng), "plants_melon_top")
    save(framed((110, 160, 40), rng), "plants_melon_bottom")
    save_rgba(icon(["", "", "", "", "    ggg", "   grrrg", "  grrbrrg", "  grrrrrg", "   grbrg", "    ggg"],
                   {"g": (80, 160, 40), "r": (220, 60, 50), "b": (30, 20, 20)}, rng), "plants_melon_slice")


def woods(rng):
    d = "woods"
    save(planks(OAK, rng), "timber_planks")   # the oak's, which had only its colour
    for name, (bark, wood, green) in WOODS.items():
        marks = (40, 40, 40) if name == "birch" else None
        save(log_side(bark, rng, 0.8 if name == "birch" else 0.7, marks), f"{d}_{name}_log")
        save(log_top(bark, wood, rng), f"{d}_{name}_log_top")
        save(planks(wood, rng), f"{d}_{name}_planks")
        save(leaves(green, rng), f"{d}_{name}_leaves")


def colors(rng):
    for c, rgb in COLOURS.items():
        save(noise(rgb, rng, 0.9, 1.06), f"colors_{c}_wool")
        tc = shade(rgb, 0.62)
        tc = tuple(int(v * 0.7 + t * 0.3) for v, t in zip(tc, (152, 94, 67)))
        save(noise(tc, rng, 0.95, 1.04), f"colors_{c}_terracotta")
        save(noise(rgb, rng, 0.98, 1.02), f"colors_{c}_concrete")
        save(speckle(rgb, [shade(rgb, 0.8), shade(rgb, 1.2)], rng, 90), f"colors_{c}_concrete_powder")
        save_rgba(icon(["", "", "", "     ww", "     ww", "    dddd", "   dddddd", "   dDdddd",
                        "   dDdddd", "   dddddd", "    dddd"],
                       {"w": (230, 230, 230), "d": rgb, "D": shade(rgb, 1.3)}, rng), f"colors_{c}_dye")
    save(noise((152, 94, 67), rng, 0.95, 1.04), "colors_terracotta")


def building(rng):
    d = "building"
    shelf = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            shelf.putpixel((x, y), shade(OAK, rng.uniform(0.85, 1.05)))
    for row in (1, 9):
        x = 1
        while x < 15:
            w = rng.choice((1, 2))
            c = rng.choice([(150, 40, 40), (40, 80, 150), (60, 120, 50), (160, 130, 60), (90, 60, 120)])
            for dx in range(w):
                for y in range(row, row + 6):
                    shelf.putpixel((x + dx, y), shade(c, rng.uniform(0.85, 1.1)))
            x += w + (1 if rng.random() < 0.3 else 0)
    save(shelf, f"{d}_bookshelf")
    alias("building:bookshelf", "timber_planks", "top")
    alias("building:bookshelf", "timber_planks", "bottom")
    tnt = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            c = (200, 50, 40) if y < 4 or y > 11 else (230, 230, 225)
            tnt.putpixel((x, y), shade(c, rng.uniform(0.88, 1.05)))
    for x, y in ((3, 7), (4, 7), (4, 8), (6, 7), (7, 7), (7, 8), (9, 7), (10, 7), (10, 8), (12, 7)):
        tnt.putpixel((x, y), (30, 30, 30))
    save(tnt, f"{d}_tnt_side")
    save(framed((200, 50, 40), rng, (230, 230, 225)), f"{d}_tnt_top")
    save(noise((200, 50, 40), rng), f"{d}_tnt_bottom")
    save(noise((250, 250, 240), rng, 0.95, 1.0), f"{d}_tnt_lit")
    bars = Image.new("RGB", (16, 16), (70, 70, 74))
    for x in range(16):
        for y in range(16):
            if x % 4 == 1 or y in (1, 14):
                bars.putpixel((x, y), shade((170, 170, 176), rng.uniform(0.9, 1.1)))
    save(bars, f"{d}_iron_bars")
    iron = noise((68, 68, 72), rng, 0.9, 1.08)
    save(iron, f"{d}_anvil")
    save(framed((60, 60, 64), rng), f"{d}_cauldron")
    for level in (1, 2, 3):
        top = framed((60, 60, 64), rng)
        for y in range(2, 14):
            for x in range(2, 14):
                top.putpixel((x, y), shade((52, 104, 178), rng.uniform(0.9, 1.1)))
        save(top, f"{d}_cauldron_{level}_top")
        alias(f"building:cauldron_{level}", f"{d}_cauldron", "side")
        alias(f"building:cauldron_{level}", f"{d}_cauldron", "bottom")
    hay = stripes((200, 170, 50), rng, False, 0.8)
    for y in (4, 11):
        for x in range(16):
            hay.putpixel((x, y), (120, 40, 30))
    save(hay, f"{d}_hay_bale")
    save(framed((190, 160, 50), rng, (170, 140, 40)), f"{d}_hay_bale_top")
    alias("building:hay_bale", f"{d}_hay_bale_top", "bottom")
    rail(rng)   # its draw kept so the seeds after it stay; the model is tools/make_rail_models.py's now
    for wood in ["oak"] + list(WOODS):
        planks_file = "timber_planks" if wood == "oak" else f"woods_{wood}_planks"
        base = OAK if wood == "oak" else WOODS[wood][1]
        for i in range(16):
            alias(f"building:{wood}_fence" + (f"_{i}" if i else ""), planks_file)
        for s in ("", "_open"):
            alias(f"building:{wood}_fence_gate{s}", planks_file)
        trap = planks(base, rng)
        for y in (4, 5, 10, 11):
            for x in (4, 5, 10, 11):
                trap.putpixel((x, y), shade(base, 0.35))
        save(trap, f"{d}_{wood}_trapdoor")
        for s in ("_open", "_rung"):
            alias(f"building:{wood}_trapdoor{s}", f"{d}_{wood}_trapdoor")
        alias(f"building:{wood}_sign", planks_file)
        alias(f"building:{wood}_standing_sign", planks_file)
    for i in range(16):
        tail = f"_{i}" if i else ""
        alias(f"building:glass_pane{tail}", "core_glass")
        if i:
            alias(f"building:iron_bars{tail}", f"{d}_iron_bars")
    save_rgba(icon(["", "", "", "", "", "      gg", "    gggGg", "   gGgggGg", "  gggGgggg", "  gggggggg"],
                   {"g": (70, 70, 70), "G": (120, 120, 120)}, rng), f"{d}_gunpowder")


def dungeons(rng):
    cage = Image.new("RGB", (16, 16), (20, 20, 24))
    for x in range(16):
        for y in range(16):
            if x % 4 == 0 or y % 4 == 0:
                cage.putpixel((x, y), shade((70, 74, 84), rng.uniform(0.9, 1.1)))
    save(cage, "dungeons_spawner")


def faces(node, file):
    """Every face of `node` wearing `file`, its own name left free for its
    bag picture (tools/make_held_models.py's block icons)."""
    for face in ("side", "top", "bottom"):
        alias(node, file, face)


def decor(rng):
    """mods/building's decoration: stained glass, tiles, the lantern; walls,
    panes, carpets and furniture wear the blocks they are made of."""
    d = "building"
    for c, rgb in COLOURS.items():
        g = Image.new("RGBA", (16, 16))
        for y in range(16):
            for x in range(16):
                edge = x in (0, 15) or y in (0, 15)
                streak = (x + y) % 7 == 0 and 3 < x < 13
                k = 1.25 if edge else 1.12 if streak else rng.uniform(0.95, 1.03)
                g.putpixel((x, y), shade(rgb, k) + (215 if edge else 150,))
        save_rgba(g, f"{d}_{c}_stained_glass")
        for i in range(16):
            (faces if i == 0 else alias)(f"{d}:{c}_stained_glass_pane" + (f"_{i}" if i else ""), f"{d}_{c}_stained_glass")
        # Glazed tiles: four to a face, a glossy corner, grout between.
        t = Image.new("RGB", (16, 16))
        grout = shade(rgb, 0.55)
        for y in range(16):
            for x in range(16):
                if x in (0, 7, 8, 15) or y in (0, 7, 8, 15):
                    t.putpixel((x, y), shade(grout, rng.uniform(0.95, 1.05)))
                else:
                    lx, ly = x % 8, y % 8
                    k = 1.22 if lx + ly <= 3 else rng.uniform(0.94, 1.04)
                    t.putpixel((x, y), shade(rgb, k))
        save(t, f"{d}_{c}_tiles")
        faces(f"{d}:{c}_carpet", f"colors_{c}_wool")
    stone = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            line = x in (0, 15) or y in (0, 15) or (y == 7 and x < 8) or (x == 7 and y > 7)
            stone.putpixel((x, y), shade((150, 150, 152), 0.75 if line else rng.uniform(0.95, 1.05)))
    save(stone, f"{d}_stone_tiles")
    chq = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            white = ((x // 4) + (y // 4)) % 2 == 0
            chq.putpixel((x, y), shade((232, 230, 224) if white else (28, 28, 32), rng.uniform(0.95, 1.04)))
    save(chq, f"{d}_chequer_tiles")
    lantern = Image.new("RGB", (16, 16))
    for y in range(16):
        for x in range(16):
            frame = x in (0, 1, 14, 15) or y in (0, 1, 14, 15)
            lantern.putpixel((x, y), shade((60, 60, 64), rng.uniform(0.9, 1.1)) if frame
                             else shade((255, 200, 110), rng.uniform(0.92, 1.05)))
    save(lantern, f"{d}_lantern_box")
    faces(f"{d}:lantern", f"{d}_lantern_box")
    alias(f"{d}:lantern_hung", f"{d}_lantern_box")
    for i in range(16):
        tail = f"_{i}" if i else ""
        put = faces if i == 0 else alias
        put(f"{d}:cobblestone_wall{tail}", "rocks_cobblestone")
        put(f"{d}:mossy_cobblestone_wall{tail}", "rocks_mossy_cobblestone")
        put(f"{d}:stone_brick_wall{tail}", "rocks_stone_bricks")
    # The sandbox's other blocks of boxes with a bag picture of their own:
    # their faces somewhere else (an icon is not a face).
    faces("animals:trough", "timber_planks")
    faces("animals:trough_full", "timber_planks")
    faces("villagers:board", "timber_planks")
    save(speckle((184, 115, 75), [(150, 90, 60), (210, 150, 100)], rng, 50), "alchemy_still_face")
    faces("alchemy:still", "alchemy_still_face")
    faces("alchemy:still_on", "alchemy_still_face")
    save(speckle((90, 200, 190), [(60, 160, 150), (150, 235, 225)], rng, 50), "travel_waystone_face")
    faces("travel:waystone", "travel_waystone_face")
    save(speckle((200, 160, 60), [(170, 130, 40), (235, 200, 110)], rng, 40), "arena_gong_face")
    faces("arena:gong", "arena_gong_face")
    for wood in ["oak"] + list(WOODS):
        planks_file = "timber_planks" if wood == "oak" else f"woods_{wood}_planks"
        faces(f"{d}:{wood}_chair", planks_file)
        faces(f"{d}:{wood}_table", planks_file)


def main():
    os.makedirs(TEX, exist_ok=True)
    rocks(random.Random(9001))
    plants(random.Random(9002))
    woods(random.Random(9003))
    colors(random.Random(9004))
    building(random.Random(9005))
    dungeons(random.Random(9006))
    decor(random.Random(9007))
    # mods/steps' stairs of wood and sandstone wear their block's picture.
    for kind, file in (("oak", "timber_planks"), ("spruce", "woods_spruce_planks"),
                       ("dark_oak", "woods_dark_oak_planks"), ("sandstone", "rocks_sandstone")):
        alias(f"steps:{kind}_stair", file)
    with open(os.path.join(TEX, "pack.txt"), "w") as f:
        f.write("# Blocks that wear another block's picture, written by tools/make_voxel_blocks.py.\n")
        f.write("\n".join(PACK) + "\n")


main()
