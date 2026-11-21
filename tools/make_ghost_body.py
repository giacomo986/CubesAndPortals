#!/usr/bin/env python3
# The body the engine gives mods/ghost's cursed ghost: nothing to see, only its
# size. The ghost itself is drawn by the mod (core.render.model, seen through
# by how far it is), so the creature the engine moves wears this: one bone, and
# one triangle whose three corners lie on a line -- it covers no pixel, but its
# ends span the box a blow aims at (0.8 wide, 1.8 tall). An "idle" and a
# "death" clip, as every creature needs (mods/mobs, docs/MODDING "Creature");
# the death is the mod's dissolve, drawn where the ghost was.
#
# usage: make_ghost_body.py [out.gltf]   (default mods/ghost/ghost_body.gltf)
import base64
import json
import os
import struct
import sys

HALF, TALL = 0.4, 1.8
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "mods", "ghost", "ghost_body.gltf")

pos = [(-HALF, 0.0, -HALF), (HALF, TALL, HALF), (0.0, TALL / 2, 0.0)]   # on one line
normals = [(0.0, 1.0, 0.0)] * 3
uvs = [(0.0, 0.0)] * 3
joints = [(0, 0, 0, 0)] * 3
weights = [(1.0, 0.0, 0.0, 0.0)] * 3
ibm = (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
times = (0.0, 1.0)
still = ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0))

blob = bytearray()
views, accessors = [], []


def add(data: bytes, count: int, comp: int, kind: str, lo=None, hi=None):
    while len(blob) % 4:
        blob.append(0)
    views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)})
    blob.extend(data)
    acc = {"bufferView": len(views) - 1, "componentType": comp, "count": count, "type": kind}
    if lo is not None:
        acc["min"], acc["max"] = lo, hi
    accessors.append(acc)
    return len(accessors) - 1


F, UB = 5126, 5121
a_pos = add(b"".join(struct.pack("<3f", *p) for p in pos), 3, F, "VEC3",
            [-HALF, 0.0, -HALF], [HALF, TALL, HALF])
a_nrm = add(b"".join(struct.pack("<3f", *n) for n in normals), 3, F, "VEC3")
a_uv = add(b"".join(struct.pack("<2f", *u) for u in uvs), 3, F, "VEC2")
a_jnt = add(b"".join(struct.pack("<4B", *j) for j in joints), 3, UB, "VEC4")
a_wgt = add(b"".join(struct.pack("<4f", *w) for w in weights), 3, F, "VEC4")
a_ibm = add(struct.pack("<16f", *ibm), 1, F, "MAT4")
a_t = add(struct.pack("<2f", *times), 2, F, "SCALAR", [0.0], [1.0])
a_v = add(b"".join(struct.pack("<3f", *v) for v in still), 2, F, "VEC3")

gltf = {
    "asset": {"version": "2.0", "generator": "tools/make_ghost_body.py"},
    "scene": 0,
    "scenes": [{"nodes": [0, 1]}],
    "nodes": [{"name": "root"}, {"name": "body", "mesh": 0, "skin": 0}],
    "skins": [{"joints": [0], "inverseBindMatrices": a_ibm}],
    "meshes": [{"primitives": [{"attributes": {"POSITION": a_pos, "NORMAL": a_nrm, "TEXCOORD_0": a_uv,
                                               "JOINTS_0": a_jnt, "WEIGHTS_0": a_wgt}}]}],
    "animations": [
        {"name": name, "samplers": [{"input": a_t, "output": a_v, "interpolation": "LINEAR"}],
         "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]}
        for name in ("idle", "death")
    ],
    "buffers": [{"byteLength": len(blob),
                 "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode()}],
    "bufferViews": views,
    "accessors": accessors,
}
os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
with open(out, "w") as f:
    json.dump(gltf, f, indent=1)
print(f"{out}: {os.path.getsize(out)} bytes")
