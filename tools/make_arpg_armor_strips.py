#!/usr/bin/env python3
# The worn pieces' sheets for Meshy's image-to-3D (tools/make_arpg_armor_meshy.py):
# each piece cut out of the inventory's painted sheets (assets/meshy/arpg_icons,
# through tools/make_arpg_icons.py's cutting) and set in a row on white, far
# apart and all of a height, five rows of them:
#
#   python3 tools/make_arpg_armor_strips.py
#
# Writes assets/meshy/arpg/strips/<row>.png and spans.json: where each piece
# is across its row, in pixels, for cutting the model Meshy makes of it.
import json
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_arpg_icons as ic  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "assets", "meshy", "arpg", "strips")
ROWS = {
    "helms": ["cap", "helm", "great_helm", "hood"],
    "torsos": ["quilted_armor", "leather_armor", "chain_mail", "plate_mail"],
    "gloves": ["leather_gloves", "chain_gloves", "gauntlets"],
    "boots": ["boots", "chain_boots", "greaves"],
    "waist": ["sash", "belt", "heavy_belt", "amulet"],
}
W, H, TALL = 1536, 768, 420


def main():
    cut = {}
    for sheet, names in ic.SHEETS.items():
        img, fg, boxes = ic.things(sheet)
        for name, box in zip(names, boxes):
            if name:
                t = img.crop(box).convert("RGBA")
                t.putalpha(fg.crop(box))
                cut[name] = t
    os.makedirs(OUT, exist_ok=True)
    spans = {}
    for row, names in ROWS.items():
        pieces = []
        for n in names:
            t = cut[n]
            k = TALL / t.height
            if t.width * k > W / len(names) * 0.7:
                k = W / len(names) * 0.7 / t.width
            pieces.append(t.resize((max(1, int(t.width * k)), max(1, int(t.height * k))), Image.LANCZOS))
        gap = (W - sum(p.width for p in pieces)) / (len(pieces) + 1)
        canvas = Image.new("RGB", (W, H), (255, 255, 255))
        x = gap
        spans[row] = []
        for p in pieces:
            canvas.paste(p, (int(x), (H - p.height) // 2), p)
            spans[row].append([int(x), int(x) + p.width])
            x += p.width + gap
        canvas.save(os.path.join(OUT, f"{row}.png"))
    with open(os.path.join(OUT, "spans.json"), "w") as f:
        json.dump({"rows": ROWS, "spans": spans}, f, indent=1)
    print("strips written to", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
