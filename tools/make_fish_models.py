#!/usr/bin/env python3
"""The kinds of fish (mods/pets), made from the one there was: fish_held.gltf
and pets_fish.png recoloured -- body and fins -- one per kind.

    tools/make_fish_models.py   # mods/pets/<kind>_held.gltf, assets/textures/pets_<kind>.png
"""
import colorsys
import json
import os

from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PETS = os.path.join(ROOT, "mods", "pets")
TEX = os.path.join(ROOT, "assets", "textures")

KINDS = {   # body, fins (0..1)
    "carp": ((0.62, 0.55, 0.30), (0.48, 0.38, 0.20)),
    "trout": ((0.55, 0.58, 0.45), (0.80, 0.45, 0.45)),
    "cod": ((0.62, 0.58, 0.50), (0.45, 0.42, 0.36)),
    "eel": ((0.25, 0.30, 0.22), (0.18, 0.22, 0.16)),
    "tuna": ((0.22, 0.30, 0.50), (0.85, 0.80, 0.35)),
    "goldfish": ((0.98, 0.62, 0.12), (1.0, 0.82, 0.30)),
}

src = json.load(open(os.path.join(PETS, "fish_held.gltf")))
icon = Image.open(os.path.join(TEX, "pets_fish.png")).convert("RGBA")
base_h = colorsys.rgb_to_hsv(0.45, 0.60, 0.80)

for kind, (body, fin) in KINDS.items():
    g = json.loads(json.dumps(src))
    for m in g["materials"]:
        if m["name"] == "Fish":
            m["pbrMetallicRoughness"]["baseColorFactor"] = list(body) + [1]
        elif m["name"] == "Fin":
            m["pbrMetallicRoughness"]["baseColorFactor"] = list(fin) + [1]
    g["buffers"][0]["uri"] = "fish_held.bin"
    json.dump(g, open(os.path.join(PETS, kind + "_held.gltf"), "w"), indent=1)

    # The icon: every bluish pixel takes the body's hue and saturation at its
    # own brightness; the outline and the eye stay.
    bh, bs, bv = colorsys.rgb_to_hsv(*body)
    img = icon.copy()
    for y in range(img.height):
        for x in range(img.width):
            r, gg, b, a = img.getpixel((x, y))
            h, s, v = colorsys.rgb_to_hsv(r / 255, gg / 255, b / 255)
            if a and s > 0.2 and abs(h - base_h[0]) < 0.12:
                nr, ng, nb = colorsys.hsv_to_rgb(bh, bs, min(1.0, v * bv / base_h[2]))
                img.putpixel((x, y), (int(nr * 255), int(ng * 255), int(nb * 255), a))
    img.save(os.path.join(TEX, f"pets_{kind}.png"))
    print("wrote", kind)
