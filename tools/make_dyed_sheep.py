#!/usr/bin/env python3
"""Dyed sheep (mods/animals): sheep.gltf and sheep_baby.gltf with their wool
in each of the colours of mods/colors but white, read from its COLOURS table
so the two stay in step. The same buffers, rig and clips -- its death among
them.

    tools/make_dyed_sheep.py   # mods/animals/sheep_<colour>.gltf, sheep_<colour>_baby.gltf
"""
import json
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ANIMALS = os.path.join(ROOT, "mods", "animals")
src = open(os.path.join(ROOT, "mods", "colors", "init.luau")).read()
COLOURS = re.findall(r'\{ "([a-z_]+)", \{ (\d+), (\d+), (\d+) \} \}', src)

for name, r, g, b in COLOURS:
    if name == "white":
        continue
    rgb = [int(r) / 255, int(g) / 255, int(b) / 255]
    for model, out in (("sheep", f"sheep_{name}"), ("sheep_baby", f"sheep_{name}_baby")):
        gl = json.load(open(os.path.join(ANIMALS, model + ".gltf")))
        for m in gl["materials"]:
            if m["name"].endswith("_wool"):
                m["pbrMetallicRoughness"]["baseColorFactor"] = rgb + [1]
            elif m["name"].endswith("_wool_dark"):
                m["pbrMetallicRoughness"]["baseColorFactor"] = [c * 0.88 for c in rgb] + [1]
        gl["buffers"][0]["uri"] = model + ".bin"
        json.dump(gl, open(os.path.join(ANIMALS, out + ".gltf"), "w"), indent=1)
print(f"wrote {2 * (len(COLOURS) - 1)} models")
