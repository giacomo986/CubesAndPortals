#!/usr/bin/env python3
"""The tower defense's turrets that turn, cut in two (games/defense/mods/defense_game):
a base that stands still and the part over it that turns to its target.

    tools/make_turret_parts.py

    turret_arrow   -> turret_arrow_base (the stone and timber box and its turntable,
                      under 0.79) and turret_arrow_top (the post and the crossbow)
    turret_portal  -> turret_portal_base (the plinth, under 0.29) and
                      turret_portal_top (the neck and the ring)

Each triangle goes to the part its middle is in; the vertices, the normals, the
uv and the picture are the whole model's, so the two meet where they did. Pure
Python on the glTF that tools/meshy_prop.py wrote. The brazier, the frost and the
lightning turret strike all round them and are drawn whole, still.
"""
import json
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
DIR = os.path.join(HERE, "..", "games", "defense", "mods", "defense_game")
CUTS = {"arrow": 0.79, "portal": 0.29}


def split(name, cut):
    g = json.load(open(os.path.join(DIR, f"turret_{name}.gltf")))
    data = open(os.path.join(DIR, g["buffers"][0]["uri"]), "rb").read()
    prim = g["meshes"][0]["primitives"][0]

    def view(acc_i):
        acc = g["accessors"][acc_i]
        bv = g["bufferViews"][acc["bufferView"]]
        return acc, bv.get("byteOffset", 0) + acc.get("byteOffset", 0)

    pacc, poff = view(prim["attributes"]["POSITION"])
    ys = [struct.unpack_from("<f", data, poff + i * 12 + 4)[0] for i in range(pacc["count"])]
    iacc, ioff = view(prim["indices"])
    fmt = {5121: "B", 5123: "H", 5125: "I"}[iacc["componentType"]]
    size = struct.calcsize(fmt)
    idx = [struct.unpack_from("<" + fmt, data, ioff + i * size)[0] for i in range(iacc["count"])]
    parts = {"base": [], "top": []}
    for t in range(0, len(idx), 3):
        tri = idx[t:t + 3]
        mid = sum(ys[i] for i in tri) / 3
        parts["base" if mid < cut else "top"].extend(tri)

    for part, tris in parts.items():
        out = json.loads(json.dumps(g))
        stem = f"turret_{name}_{part}"
        blob = bytearray(data)
        while len(blob) % 4:
            blob.append(0)
        start = len(blob)
        for i in tris:
            blob += struct.pack("<I", i)
        out["bufferViews"].append({"buffer": 0, "byteOffset": start, "byteLength": len(tris) * 4, "target": 34963})
        out["accessors"].append({"bufferView": len(out["bufferViews"]) - 1, "componentType": 5125,
                                 "count": len(tris), "type": "SCALAR"})
        out["meshes"][0]["primitives"][0]["indices"] = len(out["accessors"]) - 1
        out["buffers"][0] = {"uri": f"{stem}.bin", "byteLength": len(blob)}
        open(os.path.join(DIR, f"{stem}.bin"), "wb").write(blob)
        json.dump(out, open(os.path.join(DIR, f"{stem}.gltf"), "w"))
        print(f"turret part: {stem}, {len(tris) // 3} triangles")


for n, c in CUTS.items():
    split(n, c)
