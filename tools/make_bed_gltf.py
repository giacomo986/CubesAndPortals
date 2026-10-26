#!/usr/bin/env python3
# The bed's models, written rather than modelled: boxes of flat colour.
#
# bed.gltf is the whole bed, worn by its foot cell, two cells long: the foot in
# its own cell and the head in the next along -Z, which is the cell's back at
# turn 0 -- the way mods/bed puts the head, away from whoever placed it. Four
# legs, a wooden frame, a red blanket over the foot and most of the head, a
# white sheet and a pillow at the head. bed_half.gltf is what the head cell
# wears: one triangle too small to see, as doors/door_half.gltf, because the
# foot's model already draws the whole bed.
#
#     python3 tools/make_bed_gltf.py
import base64
import json
import os
import struct

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "mods", "bed")

WOOD = (0.62, 0.40, 0.20)
RED = (0.66, 0.13, 0.13)
WHITE = (0.92, 0.92, 0.90)

S = 1.0 / 16.0
BOXES = [
    # legs, 3/16 square, under the four corners
    (WOOD, (0, 0, -16 * S), (3 * S, 3 * S, -13 * S)),
    (WOOD, (13 * S, 0, -16 * S), (1, 3 * S, -13 * S)),
    (WOOD, (0, 0, 13 * S), (3 * S, 3 * S, 1)),
    (WOOD, (13 * S, 0, 13 * S), (1, 3 * S, 1)),
    # the frame, the whole length
    (WOOD, (0, 3 * S, -1), (1, 6 * S, 1)),
    # the blanket, from the foot to past the head cell's middle, a little proud
    # of the sheet so it reads as laid over it
    (RED, (0, 6 * S, -9 * S), (1, 9.2 * S, 1)),
    (WHITE, (0, 6 * S, -1), (1, 9 * S, -9 * S)),
    # the pillow
    (WHITE, (2 * S, 9 * S, -15 * S), (14 * S, 11 * S, -10 * S)),
]


def box(lo, hi):
    """Twelve triangles, wound outwards, with flat normals."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    faces = [
        ((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
        ((-1, 0, 0), [(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)]),
        ((0, 1, 0), [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)]),
        ((0, -1, 0), [(x0, y0, z1), (x0, y0, z0), (x1, y0, z0), (x1, y0, z1)]),
        ((0, 0, 1), [(x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (x0, y0, z1)]),
        ((0, 0, -1), [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)]),
    ]
    verts = []
    for n, (a, b, c, d) in faces:
        for v in (a, b, c, a, c, d):
            verts.append((v, n))
    return verts


def write(path, parts):
    """parts: [(colour, [(position, normal)])], one primitive and material each."""
    buf = b""
    views, accessors, prims, materials = [], [], [], []
    for i, (colour, verts) in enumerate(parts):
        data = b"".join(struct.pack("<3f3f", *p, *n) for p, n in verts)
        views.append({"buffer": 0, "byteOffset": len(buf), "byteLength": len(data), "byteStride": 24,
                      "target": 34962})
        buf += data
        ps = [p for p, _ in verts]
        accessors.append({"bufferView": i, "byteOffset": 0, "componentType": 5126, "count": len(verts),
                          "type": "VEC3", "min": [min(p[k] for p in ps) for k in range(3)],
                          "max": [max(p[k] for p in ps) for k in range(3)]})
        accessors.append({"bufferView": i, "byteOffset": 12, "componentType": 5126, "count": len(verts),
                          "type": "VEC3"})
        prims.append({"attributes": {"POSITION": 2 * i, "NORMAL": 2 * i + 1}, "material": i})
        materials.append({"name": f"m{i}", "doubleSided": True,
                          "pbrMetallicRoughness": {"baseColorFactor": [*colour, 1.0], "metallicFactor": 0.0,
                                                   "roughnessFactor": 0.9}})
    gltf = {
        "asset": {"version": "2.0", "generator": "cubes-and-portals tools/make_bed_gltf.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "bed", "mesh": 0}],
        "meshes": [{"name": "bed", "primitives": prims}],
        "materials": materials,
        "buffers": [{"byteLength": len(buf),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(buf).decode()}],
        "bufferViews": views,
        "accessors": accessors,
    }
    with open(path, "w") as f:
        json.dump(gltf, f, separators=(",", ":"))
        f.write("\n")


# One primitive a colour, so a bed is three draws and not eight.
by_colour = {}
for colour, lo, hi in BOXES:
    by_colour.setdefault(colour, []).extend(box(lo, hi))
write(os.path.join(OUT, "bed.gltf"), list(by_colour.items()))

tiny = [((0.5, 0.3, 0.5), (0, 1, 0)), ((0.501, 0.3, 0.5), (0, 1, 0)), ((0.5, 0.301, 0.5), (0, 1, 0))]
write(os.path.join(OUT, "bed_half.gltf"), [(WOOD, tiny)])
print("bed.gltf:", sum(len(v) for v in by_colour.values()) // 3, "triangles")
