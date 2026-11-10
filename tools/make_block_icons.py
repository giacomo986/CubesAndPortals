#!/usr/bin/env python3
"""Slot icons for the blocks that are not cubes.

A slot shows an item's own file (mod_name.png) and otherwise the block's
texture, so a fence was a square of planks and a pressure plate a square of
stone. This draws each one from its boxes, seen from above and in front.

The file a block's faces read is the same plain file, so what the placed block
wore moves to name_top/_bottom/_side first (or to the same keys in pack.txt).

tools/block_icon_boxes.json is each block's boxes and colour, dumped from a run
of the server. Run this after tools/make_voxel_blocks.py, which writes pack.txt.

    python3 tools/make_block_icons.py
"""
import json
import os
import re

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "textures")
PACK = os.path.join(OUT, "pack.txt")
OUTLINE = (30, 24, 20, 255)
FACES = ("top", "bottom", "side")


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c) + (255,)


def mean(path):
    img = Image.open(path).convert("RGBA").resize((1, 1), Image.BOX).getpixel((0, 0))
    return img[:3]


def project(p):
    x, y, z = p
    return ((x - z) * 0.866, (x + z) * 0.5 - y)


def corners(b):
    x0, y0, z0, x1, y1, z1 = b
    return {
        "top": [(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)],
        "x": [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)],
        "z": [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)],
    }


def icon(boxes, colours):
    pts = [project(p) for b in boxes for f in corners(b).values() for p in f]
    u0, u1 = min(p[0] for p in pts), max(p[0] for p in pts)
    v0, v1 = min(p[1] for p in pts), max(p[1] for p in pts)
    scale = min(26 / max(u1 - u0, 0.01), 26 / max(v1 - v0, 0.01))
    cu, cv = (u0 + u1) / 2, (v0 + v1) / 2

    def at(p):
        u, v = project(p)
        return (15.5 + (u - cu) * scale, 15.5 + (v - cv) * scale)

    img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # ponytail: boxes sorted by their middle; two long boxes that cross can draw in the wrong order
    for b in sorted(boxes, key=lambda b: b[0] + b[3] + b[2] + b[5] + b[1] + b[4]):
        faces = corners(b)
        d.polygon([at(p) for p in faces["z"]], fill=shade(colours["side"], 0.62))
        d.polygon([at(p) for p in faces["x"]], fill=shade(colours["side"], 0.8))
        d.polygon([at(p) for p in faces["top"]], fill=shade(colours["top"], 1.08))
    px = img.load()
    filled = [(x, y) for x in range(32) for y in range(32) if px[x, y][3]]
    for x, y in filled:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < 32 and 0 <= ny < 32 and not px[nx, ny][3]:
                px[nx, ny] = OUTLINE
    return img


# What a block's base shape does not show: the post of a fence is only a post.
FENCE = [[0.1, 0, 0.4, 0.3, 1, 0.6], [0.7, 0, 0.4, 0.9, 1, 0.6],
         [0, 0.65, 0.45, 1, 0.8, 0.55], [0, 0.3, 0.45, 1, 0.45, 0.55]]
PANE = [[0, 0, 0.45, 1, 1, 0.55]]
BARS = [[0.08 + 0.26 * i, 0, 0.45, 0.2 + 0.26 * i, 1, 0.55] for i in range(4)] + \
       [[0, 0.8, 0.45, 1, 0.92, 0.55], [0, 0.1, 0.45, 1, 0.22, 0.55]]


def shown(name, boxes):
    if name.endswith("_fence"):
        return FENCE
    return {"building:glass_pane": PANE, "building:iron_bars": BARS}.get(name, boxes)


def main():
    blocks = json.load(open(os.path.join(HERE, "block_icon_boxes.json")))
    pack = open(PACK).read().split("\n")
    # The sheet's plain keys: the faces of those blocks move to face keys.
    plain = {m[1]: m[2] for m in (re.match(r"^([^#\s]+)\s*=\s*(\S+)$", l) for l in pack) if m}

    def file_of(name, face):
        stem = name.replace(":", "_")
        for key in (f"{name}:{face}", name):
            if key in plain:
                return os.path.join(OUT, plain[key])
        for suffix in (f"_{face}", ""):
            path = os.path.join(OUT, stem + suffix + ".png")
            if os.path.exists(path):
                return path
        return None

    for name, block in sorted(blocks.items()):
        boxes, rgb = block["boxes"], tuple(int(v) for v in block["rgb"])
        stem = name.replace(":", "_")
        target = os.path.join(OUT, stem + ".png")
        # A whole cube already says what it is.
        if any(b[3] - b[0] >= 1 and b[4] - b[1] >= 1 and b[5] - b[2] >= 1 for b in boxes):
            continue
        colours = {}
        for face in ("top", "side"):
            path = file_of(name, face)
            colours[face] = mean(path) if path else rgb
        was_plain = os.path.exists(target)
        old = Image.open(target).convert("RGBA").copy() if was_plain else None
        in_sheet = plain.get(name)
        flat = Image.new("RGB", (32, 32), rgb)
        for face in FACES:
            own = os.path.join(OUT, f"{stem}_{face}.png")
            if os.path.exists(own) or f"{name}:{face}" in plain:
                continue
            if in_sheet:
                pack.append(f"{name}:{face} = {in_sheet}")
            else:
                (old or flat).save(own)
        if in_sheet:
            pack = [l for l in pack if not re.match(rf"^{re.escape(name)}\s*=", l)]
        if was_plain:
            # Other blocks wearing this file keep its old picture.
            pack = [l.replace(f"= {stem}.png", f"= {stem}_side.png") for l in pack]
        top = os.path.join(OUT, f"{stem}_top.png")
        if name.startswith("circuits:gate_") and os.path.exists(top):
            Image.open(top).save(target)    # a gate's top, name and arrow, is its icon
        else:
            icon(shown(name, boxes), colours).save(target)
    open(PACK, "w").write("\n".join(pack))


main()
