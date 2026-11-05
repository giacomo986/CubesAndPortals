#!/usr/bin/env python3
"""Cinnabar dust in every shape it is drawn in, as glTF models in mods/cinnabar.

Dust is drawn joined the ways it carries power (mods/cinnabar/init.luau): a
shape is four digits, one for each side in the order +X, -X, +Z, -Z -- 0
nothing that way, 1 a line along the floor to the edge, 2 that line and on up
the face of the block beside it, to dust a step higher. Dust joined nowhere is
a cross (the cross is dust.gltf, from tools/make_block_textures.py); otherwise
it has an arm for each way it is joined, one to four.

Every piece is cut out of the cross picture tools/make_block_textures.py drew
(dust.png, dust_on.png): the centre square, an arm, and the arm stood up
against a wall all map the picture where they lie, so a shape needs no picture
of its own.

    python3 tools/make_dust_models.py
"""
import base64
import itertools
import json
import os
import struct

MOD = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "mods", "cinnabar")

H = 1.0 / 64            # over the floor, and off the wall
W0, W1 = 7 / 16, 9 / 16  # the line's two edges: pixels 7 and 8 of the picture

# +X, -X, +Z, -Z, as the mod counts them.
SIDES = ((1, 0), (-1, 0), (0, 1), (0, -1))


def shape_of(sides):
    """The shape dust joined these ways is drawn in (the mod's dust_shape)."""
    n = sum(1 for s in sides if s > 0)
    if n == 0:
        return (1, 1, 1, 1)
    return tuple(sides)


def shapes():
    return sorted({shape_of(s) for s in itertools.product((0, 1, 2), repeat=4)})


def floor_quad(x0, z0, x1, z1):
    """A rectangle on the floor, the picture mapped where it lies (u = x, v = z)."""
    return [((x0, H, z0), (0, 1, 0), (x0, z0)), ((x0, H, z1), (0, 1, 0), (x0, z1)),
            ((x1, H, z1), (0, 1, 0), (x1, z1)), ((x1, H, z0), (0, 1, 0), (x1, z0))]


def wall_quad(side):
    """The line stood up against the block on that side, floor to top: the
    picture's upright arm, across the line and up it."""
    dx, dz = side
    q = []
    for a, y in ((W0, 0.0), (W0, 1.0), (W1, 1.0), (W1, 0.0)):
        if dx:
            x = 1 - H if dx > 0 else H
            q.append(((x, y, a), (-dx, 0, 0), (a, y)))
        else:
            z = 1 - H if dz > 0 else H
            q.append(((a, y, z), (0, 0, -dz), (a, y)))
    return q


def geometry(shape):
    quads = [floor_quad(W0, W0, W1, W1)]
    arms = {(1, 0): (W1, W0, 1, W1), (-1, 0): (0, W0, W0, W1),
            (0, 1): (W0, W1, W1, 1), (0, -1): (W0, 0, W1, W0)}
    for side, s in zip(SIDES, shape):
        if s >= 1:
            quads.append(floor_quad(*arms[side]))
        if s == 2:
            quads.append(wall_quad(side))
    return quads


def write(name, picture, shape):
    quads = geometry(shape)
    verts, idx = [], []
    for q in quads:
        b = len(verts)
        verts += q
        idx += [b, b + 1, b + 2, b, b + 2, b + 3]
    vbytes = b"".join(struct.pack("<3f3f2f", *p, *n, *uv) for p, n, uv in verts)
    ibytes = b"".join(struct.pack("<H", i) for i in idx)
    buf = vbytes + ibytes + b"\x00" * ((4 - len(ibytes) % 4) % 4)
    lo = [min(v[0][i] for v in verts) for i in range(3)]
    hi = [max(v[0][i] for v in verts) for i in range(3)]
    gltf = {
        "asset": {"version": "2.0", "generator": "cubes-and-portals tools/make_dust_models.py"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"name": name, "primitives": [
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3,
             "material": 0}]}],
        "materials": [{"name": picture, "doubleSided": True, "alphaMode": "MASK",
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                "metallicFactor": 0, "roughnessFactor": 1}}],
        "textures": [{"source": 0}],
        "images": [{"uri": picture + ".png"}],
        "buffers": [{"byteLength": len(buf), "uri": "data:application/octet-stream;base64,"
                     + base64.b64encode(buf).decode()}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(vbytes), "byteStride": 32,
             "target": 34962},
            {"buffer": 0, "byteOffset": len(vbytes), "byteLength": len(ibytes), "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": len(verts),
             "type": "VEC3", "min": lo, "max": hi},
            {"bufferView": 0, "byteOffset": 12, "componentType": 5126, "count": len(verts),
             "type": "VEC3"},
            {"bufferView": 0, "byteOffset": 24, "componentType": 5126, "count": len(verts),
             "type": "VEC2"},
            {"bufferView": 1, "componentType": 5123, "count": len(idx), "type": "SCALAR"},
        ],
    }
    with open(os.path.join(MOD, name + ".gltf"), "w") as f:
        f.write(json.dumps(gltf, indent=1) + "\n")


def main():
    n = 0
    for shape in shapes():
        if shape == (1, 1, 1, 1):
            continue   # the cross: dust.gltf and dust_on.gltf, a single quad
        code = "".join(str(s) for s in shape)
        for picture in ("dust", "dust_on"):
            write(f"{picture}_{code}", picture, shape)
            n += 1
    print(f"{n} models in {os.path.normpath(MOD)}")


if __name__ == "__main__":
    main()
