#!/usr/bin/env python3
"""Assets for mods/pipes: the glowing stacks that travel in a pipe, and the
slot icons of the pipes and the chips.

A stack on its way through a pipe is drawn as a small cube in the colour of
its aura (mods/pipes/init.luau says which): blue for a responder, purple for
a terminator, orange for an overflow, white for what was asked for, green for
a stack that has lost its way. Plain colours (baseColorFactor), no picture.

    python3 tools/make_pipes_assets.py
"""
import json
import os
import struct

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
MOD = os.path.join(ROOT, "mods", "pipes")
ICONS = os.path.join(ROOT, "assets", "textures")

AURAS = {
    "blue": (0.25, 0.55, 1.0), "purple": (0.7, 0.3, 1.0), "orange": (1.0, 0.55, 0.1),
    "white": (0.95, 0.95, 0.95), "green": (0.3, 0.95, 0.35),
}
S = 0.34  # the cube's side, in nodes, more than the tube's 0.25; its feet at y = 0


def cube_gltf(name, rgb):
    h = S / 2
    faces = (((1, 0, 0), [(h, 0, -h), (h, S, -h), (h, S, h), (h, 0, h)]),
             ((-1, 0, 0), [(-h, 0, h), (-h, S, h), (-h, S, -h), (-h, 0, -h)]),
             ((0, 1, 0), [(-h, S, -h), (-h, S, h), (h, S, h), (h, S, -h)]),
             ((0, -1, 0), [(-h, 0, h), (-h, 0, -h), (h, 0, -h), (h, 0, h)]),
             ((0, 0, 1), [(h, 0, h), (h, S, h), (-h, S, h), (-h, 0, h)]),
             ((0, 0, -1), [(-h, 0, -h), (-h, S, -h), (h, S, -h), (h, 0, -h)]))
    verts, idx = [], []
    for n, quad in faces:
        b = len(verts)
        verts += [(p, n, uv) for p, uv in zip(quad, ((0, 0), (0, 1), (1, 1), (1, 0)))]
        idx += [b, b + 1, b + 2, b, b + 2, b + 3]
    # Laid out as Blender's exporter does, which is what the client draws a
    # thing with: one view per attribute, and the bytes in a .bin beside it.
    pos = b"".join(struct.pack("<3f", *p) for p, _, _ in verts)
    nor = b"".join(struct.pack("<3f", *n) for _, n, _ in verts)
    ind = b"".join(struct.pack("<H", i) for i in idx)
    buf = pos + nor + ind
    with open(os.path.join(MOD, name + ".bin"), "wb") as f:
        f.write(buf)
    gltf = {
        "asset": {"version": "2.0", "generator": "cubes-and-portals tools/make_pipes_assets.py"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"name": name, "primitives": [
            {"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 0}]}],
        "materials": [{"name": name, "doubleSided": True, "emissiveFactor": list(rgb), "pbrMetallicRoughness": {
            "baseColorFactor": [*rgb, 1], "metallicFactor": 0, "roughnessFactor": 1}}],
        "buffers": [{"byteLength": len(buf), "uri": name + ".bin"}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(pos), "target": 34962},
            {"buffer": 0, "byteOffset": len(pos), "byteLength": len(nor), "target": 34962},
            {"buffer": 0, "byteOffset": len(pos) + len(nor), "byteLength": len(ind), "target": 34963}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(verts), "type": "VEC3",
             "min": [-h, 0, -h], "max": [h, S, h]},
            {"bufferView": 1, "componentType": 5126, "count": len(verts), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "count": len(idx), "type": "SCALAR"}],
    }
    with open(os.path.join(MOD, name + ".gltf"), "w") as f:
        f.write(json.dumps(gltf, indent=1) + "\n")


# Pipes: a length of tube across the icon, in the block's colour (the mod's
# PIPES table), with a band in the middle for the routed ones.
PIPES = {
    "item_pipe": ((170, 190, 200), None), "junction_pipe": ((70, 75, 80), (90, 220, 110)),
    "interface_pipe": ((70, 75, 80), (230, 190, 60)), "request_pipe": ((70, 75, 80), (80, 140, 240)),
    "firewall_pipe": ((70, 75, 80), (220, 70, 50)),
}
# Chips: a little board with a stripe in its colour (the wiki's illumar).
CHIPS = {
    "blank_chip": (150, 150, 150), "responder_chip": (240, 140, 40),
    "dynamic_responder_chip": (40, 190, 200), "overflow_chip": (90, 200, 70),
    "terminator_chip": (160, 80, 210), "extractor_chip": (40, 170, 190),
    "broadcaster_chip": (220, 80, 200), "stock_keeper_chip": (60, 90, 220),
}
OUTLINE = (30, 24, 20, 255)


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c) + (255,)


def pipe_icon(name, body, band):
    im = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rectangle((2, 10, 29, 21), fill=shade(body, 1.0), outline=OUTLINE)
    d.rectangle((3, 11, 28, 13), fill=shade(body, 1.3))
    d.rectangle((3, 19, 28, 20), fill=shade(body, 0.7))
    if band:
        d.rectangle((12, 8, 19, 23), fill=shade(band, 1.0), outline=OUTLINE)
    im.save(os.path.join(ICONS, f"pipes_{name}.png"))


def chip_icon(name, colour):
    im = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for x in range(8, 25, 4):   # legs
        d.rectangle((x, 4, x + 1, 27), fill=(190, 190, 180, 255))
    d.rectangle((5, 7, 26, 24), fill=(40, 70, 45, 255), outline=OUTLINE)
    d.rectangle((8, 12, 23, 19), fill=shade(colour, 1.0))
    d.rectangle((8, 12, 23, 13), fill=shade(colour, 1.3))
    im.save(os.path.join(ICONS, f"pipes_{name}.png"))


def main():
    os.makedirs(MOD, exist_ok=True)
    for name, rgb in AURAS.items():
        cube_gltf(f"aura_{name}", rgb)
    for name, (body, band) in PIPES.items():
        pipe_icon(name, body, band)
    for name, colour in CHIPS.items():
        chip_icon(name, colour)
    print(f"{len(AURAS)} auras in mods/pipes, {len(PIPES) + len(CHIPS)} icons in assets/textures")


if __name__ == "__main__":
    main()
