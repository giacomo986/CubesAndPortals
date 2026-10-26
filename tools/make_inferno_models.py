#!/usr/bin/env python3
"""The Inferno's fireball and brute as glTF (games/inferno/mods/inferno).

The fireball: a mesh of boxes, painted by an emissive material, which is drawn
unshaded and so glows. The brute is the zombie of mods/mobs with its colours
changed, on the same skeleton and the same clips (its "death" too) and the same
.bin: run this again after tools/export_mob_models.py. The wraith and the cinder,
which move, are made in Blender: tools/make_inferno_creatures.py.

Facing -z and standing on the origin, 1.8 tall at most: the box a creature is
hit in, whatever it looks like.

    python3 tools/make_inferno_models.py
"""
import json
import os
import struct

ROOT = os.path.join(os.path.dirname(__file__), "..")
OUT = os.path.join(ROOT, "games", "inferno", "mods", "inferno")


def box(lo, hi):
    """Positions, normals and indices of an axis-aligned box, 24 vertices."""
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    faces = [
        ((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
        ((-1, 0, 0), [(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)]),
        ((0, 1, 0), [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)]),
        ((0, -1, 0), [(x0, y0, z1), (x0, y0, z0), (x1, y0, z0), (x1, y0, z1)]),
        ((0, 0, 1), [(x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (x0, y0, z1)]),
        ((0, 0, -1), [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)]),
    ]
    pos, nor, idx = [], [], []
    for n, quad in faces:
        base = len(pos)
        pos += quad
        nor += [n] * 4
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    return pos, nor, idx


def write(name, parts, materials):
    """parts: {material name: [(lo, hi), ...]}; materials: {name: (rgb, emissive)}."""
    blob = bytearray()
    views, accessors, prims, mats = [], [], [], []

    def view(data, target):
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(data),
                      "target": target})
        blob.extend(data)
        return len(views) - 1

    for mname, boxes in parts.items():
        pos, nor, idx = [], [], []
        for lo, hi in boxes:
            p, n, i = box(lo, hi)
            idx += [len(pos) + k for k in i]
            pos += p
            nor += n
        pv = view(struct.pack(f"<{len(pos) * 3}f", *[c for v in pos for c in v]), 34962)
        nv = view(struct.pack(f"<{len(nor) * 3}f", *[c for v in nor for c in v]), 34962)
        iv = view(struct.pack(f"<{len(idx)}H", *idx), 34963)
        accessors.append({"bufferView": pv, "componentType": 5126, "count": len(pos),
                          "type": "VEC3",
                          "min": [min(v[a] for v in pos) for a in range(3)],
                          "max": [max(v[a] for v in pos) for a in range(3)]})
        accessors.append({"bufferView": nv, "componentType": 5126, "count": len(nor),
                          "type": "VEC3"})
        accessors.append({"bufferView": iv, "componentType": 5123, "count": len(idx),
                          "type": "SCALAR"})
        rgb, glow = materials[mname]
        mat = {"name": mname, "pbrMetallicRoughness": {
            "baseColorFactor": [*rgb, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.9}}
        if glow:
            mat["emissiveFactor"] = list(rgb)
        mats.append(mat)
        a = len(accessors)
        prims.append({"attributes": {"POSITION": a - 3, "NORMAL": a - 2}, "indices": a - 1,
                      "material": len(mats) - 1})

    gltf = {
        "asset": {"generator": "tools/make_inferno_models.py", "version": "2.0"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": name}],
        "meshes": [{"name": name, "primitives": prims}],
        "materials": mats, "accessors": accessors, "bufferViews": views,
        "buffers": [{"byteLength": len(blob), "uri": f"{name}.bin"}],
    }
    with open(os.path.join(OUT, f"{name}.bin"), "wb") as f:
        f.write(blob)
    with open(os.path.join(OUT, f"{name}.gltf"), "w") as f:
        json.dump(gltf, f, indent=1)


def fireball():
    write("fireball", {"f_outer": [((-0.2, -0.2, -0.2), (0.2, 0.2, 0.2))],
                       "f_inner": [((-0.26, -0.12, -0.12), (0.26, 0.12, 0.12)),
                                   ((-0.12, -0.26, -0.12), (0.12, 0.26, 0.12))]},
          {"f_outer": ((1.0, 0.45, 0.08), True), "f_inner": ((1.0, 0.85, 0.3), True)})


def brute():
    # The zombie, coloured as a brute: pink skin, a gold-brown loincloth, a
    # dark leather belt. Same skeleton, clips and .bin (a link to the zombie's).
    src = os.path.join(ROOT, "mods", "mobs", "zombie.gltf")
    with open(src) as f:
        g = json.load(f)
    colours = {
        "z_skin": (0.93, 0.6, 0.58), "z_skin_dark": (0.78, 0.45, 0.45),
        "z_hair": (0.35, 0.2, 0.12), "z_shirt": (0.85, 0.66, 0.2),
        "z_shirt_dark": (0.62, 0.46, 0.12), "z_pants": (0.42, 0.26, 0.14),
        "z_pants_dark": (0.3, 0.18, 0.1), "z_eye": (0.95, 0.85, 0.2),
    }
    for m in g["materials"]:
        if m.get("name") in colours:
            m["pbrMetallicRoughness"]["baseColorFactor"] = [*colours[m["name"]], 1.0]
    g["buffers"][0]["uri"] = "brute.bin"
    with open(os.path.join(OUT, "brute.gltf"), "w") as f:
        json.dump(g, f)
    link = os.path.join(OUT, "brute.bin")
    if not os.path.lexists(link):
        os.symlink("../../../../mods/mobs/zombie.bin", link)


if __name__ == "__main__":
    fireball()
    brute()
    print(f"fireball and brute in {os.path.normpath(OUT)}")
