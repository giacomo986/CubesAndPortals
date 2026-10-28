#!/usr/bin/env python3
"""The Hollows' creatures (mods/deep), made from the zombie (mods/mobs): the
same rig and clips -- its death among them -- in their own size and colours.

  guardian  half again as big, in slate and crystal: grey-violet skin, glowing
            violet eyes and bones of crystal.
  lurker    a little smaller, pale as a thing that never saw the sun, with
            glowing teal eyes like the glowcaps it lives among.

    tools/make_guardian_model.py   # mods/deep/guardian.gltf, lurker.gltf + .bin
"""
import json
import os
import shutil

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "mods", "mobs")
OUT = os.path.join(ROOT, "mods", "deep")

KINDS = {
    "guardian": (1.5, {
        "z_skin": (0.30, 0.29, 0.36), "z_skin_dark": (0.20, 0.19, 0.25), "z_dark": (0.05, 0.04, 0.07),
        "z_eye": (0.85, 0.40, 1.0), "z_tooth": (0.75, 0.60, 0.95), "z_hair": (0.12, 0.10, 0.16),
        "z_shirt": (0.36, 0.22, 0.52), "z_shirt_dark": (0.24, 0.14, 0.36), "z_bone": (0.78, 0.55, 1.0),
        "z_belt": (0.18, 0.16, 0.22), "z_pants": (0.22, 0.21, 0.27), "z_pants_dark": (0.15, 0.14, 0.19),
    }),
    "lurker": (0.85, {
        "z_skin": (0.78, 0.80, 0.74), "z_skin_dark": (0.58, 0.62, 0.56), "z_dark": (0.06, 0.08, 0.07),
        "z_eye": (0.30, 0.95, 0.90), "z_tooth": (0.85, 0.90, 0.80), "z_hair": (0.70, 0.72, 0.66),
        "z_shirt": (0.24, 0.30, 0.28), "z_shirt_dark": (0.16, 0.20, 0.19), "z_bone": (0.60, 0.95, 0.85),
        "z_belt": (0.14, 0.16, 0.15), "z_pants": (0.20, 0.24, 0.22), "z_pants_dark": (0.13, 0.16, 0.15),
    }),
}

for kind, (scale, colours) in KINDS.items():
    g = json.load(open(os.path.join(SRC, "zombie.gltf")))
    for m in g["materials"]:
        if m["name"] in colours:
            m["pbrMetallicRoughness"]["baseColorFactor"] = list(colours[m["name"]]) + [1]
        if m["name"] in ("z_eye", "z_bone", "z_tooth"):
            m["emissiveFactor"] = list(colours[m["name"]])
        m["name"] = m["name"].replace("z_", kind[0] + "_")
    for n in g["nodes"]:
        if n.get("name") == "Zombie_Rig":
            n["scale"] = [s * scale for s in n["scale"]]
            n["name"] = kind.capitalize() + "_Rig"
    g["buffers"][0]["uri"] = kind + ".bin"
    json.dump(g, open(os.path.join(OUT, kind + ".gltf"), "w"), indent=1)
    shutil.copy(os.path.join(SRC, "zombie.bin"), os.path.join(OUT, kind + ".bin"))
    print(f"wrote mods/deep/{kind}.gltf")
