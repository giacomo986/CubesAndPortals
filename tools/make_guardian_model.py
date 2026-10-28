#!/usr/bin/env python3
"""The Hollow's guardian (mods/deep), made from the zombie (mods/mobs): the
same rig and clips -- its death among them -- half again as big, in slate and
crystal: grey-violet skin, glowing violet eyes and bones of crystal.

    tools/make_guardian_model.py   # mods/deep/guardian.gltf + .bin
"""
import json
import os
import shutil

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "mods", "mobs")
OUT = os.path.join(ROOT, "mods", "deep")

COLOURS = {
    "z_skin": (0.30, 0.29, 0.36), "z_skin_dark": (0.20, 0.19, 0.25), "z_dark": (0.05, 0.04, 0.07),
    "z_eye": (0.85, 0.40, 1.0), "z_tooth": (0.75, 0.60, 0.95), "z_hair": (0.12, 0.10, 0.16),
    "z_shirt": (0.36, 0.22, 0.52), "z_shirt_dark": (0.24, 0.14, 0.36), "z_bone": (0.78, 0.55, 1.0),
    "z_belt": (0.18, 0.16, 0.22), "z_pants": (0.22, 0.21, 0.27), "z_pants_dark": (0.15, 0.14, 0.19),
}

g = json.load(open(os.path.join(SRC, "zombie.gltf")))
for m in g["materials"]:
    if m["name"] in COLOURS:
        m["pbrMetallicRoughness"]["baseColorFactor"] = list(COLOURS[m["name"]]) + [1]
    if m["name"] in ("z_eye", "z_bone", "z_tooth"):
        m["emissiveFactor"] = list(COLOURS[m["name"]])
    m["name"] = m["name"].replace("z_", "g_")
for n in g["nodes"]:
    if n.get("name") == "Zombie_Rig":
        n["scale"] = [s * 1.5 for s in n["scale"]]
        n["name"] = "Guardian_Rig"
g["buffers"][0]["uri"] = "guardian.bin"
json.dump(g, open(os.path.join(OUT, "guardian.gltf"), "w"), indent=1)
shutil.copy(os.path.join(SRC, "zombie.bin"), os.path.join(OUT, "guardian.bin"))
print("wrote mods/deep/guardian.gltf")
