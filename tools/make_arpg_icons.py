# The arpg's inventory pictures (games/arpg): every item base, the belt's
# potions and scroll, gold, the crafting materials and the quest marks, cut
# out of painted sheets made by Meshy's text-to-image (assets/meshy/arpg_icons/
# *.png, one row or two of things on white), and each base composed once per
# rarity, the picture better the rarer the item:
#
#   normal  a dark slot, the thing on it
#   magic   a blue glow round the thing's outline, blue corners
#   rare    a yellow glow, gold corners, a spark
#   unique  a deep orange-gold glow, a gold frame with gems at its corners
#   set     a green glow, a green-gold frame with leaves at its corners
#
#   python3 tools/make_arpg_icons.py [--blobs]
#
# Writes games/arpg/mods/arpg_icons/icon_<name>[_<rarity>].png (96 px), and
# icons.gltf beside them: a model of one triangle that names every picture, so
# the server serves them as that model's files ("model.arpg_icons_all.icon_...
# .png", which arpg_game draws with core.hud.image). Not items: items share the
# blocks' atlas, whose tile size is voted by its pictures, and two hundred of
# these at 96 px outvoted the blocks' 32 and broke it. --blobs prints what was
# found on each sheet (to fix the order below when a sheet is made again).
#
# Pure PIL: the background is what a flood from the sheet's edge reaches of
# near-white, the things are what is left, joined when closer than a few
# pixels (a pair of gloves is one thing), shadows under them dropped (flat,
# grey), left to right and top to bottom.
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "assets", "meshy", "arpg_icons")
OUT = os.path.join(ROOT, "games", "arpg", "mods", "arpg_icons")
S = 96          # the picture
PAD = 10        # room round the thing for its glow and frame
# How near (quarter pixels) two parts are to be one thing: a pair of gloves.
JOIN = {"hands": 2, "misc": 0}

# Each sheet's things in the order found (rows top to bottom, each left to
# right); None skips one.
SHEETS = {
    "blades": ["short_sword", "long_sword", "broad_sword", "war_sword", "dagger", "kris"],
    "axes": ["hand_axe", "battle_axe", "great_axe", "club", "war_hammer", "cleaver"],
    "bows": ["short_bow", "long_bow", "war_bow", "hunters_bow", "staff", "battle_staff", "wand", "bone_wand"],
    "off": ["buckler", "kite_shield", "tower_shield", "orb", "eye_orb"],
    "armor": ["cap", "helm", "great_helm", "hood", None, "quilted_armor", "leather_armor", "chain_mail",
              "plate_mail"],
    "hands": ["leather_gloves", "chain_gloves", "gauntlets", "boots", "chain_boots", "greaves", "sash", "belt"],
    "misc": ["heavy_belt", "ring", "amulet", "lpot", "mpot", "tp", "gold", "pouch"],
    "mats": ["iron_ore", "leather_hide", "bone", "arcane_dust", "ruby", "soul_shard", "ember_essence",
             "drake_scale"],
}
# What is composed once per rarity: every item base (arpg_game's D.BASES).
BASES = ["short_sword", "long_sword", "broad_sword", "war_sword", "hand_axe", "battle_axe", "great_axe", "club",
         "war_hammer", "dagger", "kris", "short_bow", "long_bow", "war_bow", "hunters_bow", "staff", "battle_staff",
         "wand", "bone_wand", "buckler", "kite_shield", "tower_shield", "orb", "eye_orb", "cap", "helm",
         "great_helm", "hood", "quilted_armor", "leather_armor", "chain_mail", "plate_mail", "leather_gloves",
         "chain_gloves", "gauntlets", "boots", "chain_boots", "greaves", "sash", "belt", "heavy_belt", "ring",
         "amulet"]
# rarity: glow colour, frame colour, frame kind
RARITY = [
    (None, (92, 84, 72), "plain"),
    ((90, 130, 255), (110, 140, 255), "corners"),
    ((255, 220, 70), (240, 200, 80), "corners"),
    ((255, 140, 30), (230, 170, 70), "ornate"),
    ((70, 230, 80), (150, 210, 90), "ornate"),
]
GEM = [None, None, None, (255, 60, 50), (60, 230, 90)]


def background(img):
    """Of a sheet, a mask of what its edge reaches over near-white (255) and the rest (0)."""
    w, h = img.size
    px = img.load()
    light = Image.new("L", (w, h), 0)
    lp = light.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y][:3]
            if min(r, g, b) > 196 and max(r, g, b) - min(r, g, b) < 26:
                lp[x, y] = 200
    for x in range(0, w, 16):
        for y in (0, h - 1):
            if lp[x, y] == 200:
                ImageDraw.floodfill(light, (x, y), 255)
    for y in range(0, h, 16):
        for x in (0, w - 1):
            if lp[x, y] == 200:
                ImageDraw.floodfill(light, (x, y), 255)
    # And what the edge cannot reach but is the sheet's white all the same: the
    # inside of a bow's string or an amulet's chain -- a wide patch of the very
    # lightest, not a highlight on steel (small, or tinted).
    for y in range(0, h, 3):
        for x in range(0, w, 3):
            if lp[x, y] == 200 and min(px[x, y][:3]) > 228:
                before = light.histogram()[100]
                ImageDraw.floodfill(light, (x, y), 100)
                if light.histogram()[100] - before < 400:
                    ImageDraw.floodfill(light, (x, y), 150)   # small: kept, and not tried again
    return light.point(lambda v: 255 if v in (255, 100) else 0)


def blobs(fg, join):
    """Boxes of the joined parts of a mask (255 = thing), worked at a quarter size."""
    q = 4
    small = fg.resize((fg.width // q, fg.height // q), Image.BOX).point(lambda v: 255 if v > 40 else 0)
    grown = small.filter(ImageFilter.MaxFilter(join * 2 + 1)) if join > 0 else small
    w, h = grown.size
    p = grown.load()
    seen = set()
    out = []
    for y in range(h):
        for x in range(w):
            if p[x, y] and (x, y) not in seen:
                stack, box = [(x, y)], [x, y, x, y]
                seen.add((x, y))
                while stack:
                    cx, cy = stack.pop()
                    box = [min(box[0], cx), min(box[1], cy), max(box[2], cx), max(box[3], cy)]
                    for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                        if 0 <= nx < w and 0 <= ny < h and p[nx, ny] and (nx, ny) not in seen:
                            seen.add((nx, ny))
                            stack.append((nx, ny))
                out.append([(box[0] + join) * q, (box[1] + join) * q, (box[2] - join + 1) * q, (box[3] - join + 1) * q])
    return out


def things(name):
    img = Image.open(os.path.join(SRC, f"{name}.png")).convert("RGB")
    fg = ImageChops.invert(background(img))
    found = []
    for b in blobs(fg, JOIN.get(name, 1)):
        x0, y0, x1, y1 = max(0, b[0]), max(0, b[1]), min(img.width, b[2]), min(img.height, b[3])
        if x1 - x0 < 30 or y1 - y0 < 30:
            continue
        crop = img.crop((x0, y0, x1, y1))
        # a shadow: flat, low, grey
        if (y1 - y0) < (x1 - x0) * 0.25 and max(ImageChops.difference(crop.convert("L").convert("RGB"),
                                                                        crop).getextrema()[k][1]
                                                for k in range(3)) < 30:
            continue
        found.append((x0, y0, x1, y1))
    # rows: a box is in the row of the first box it overlaps vertically by half
    found.sort(key=lambda b: b[1])
    rows = []
    for b in found:
        for row in rows:
            r = row[0]
            if min(b[3], r[3]) - max(b[1], r[1]) > 0.3 * min(b[3] - b[1], r[3] - r[1]):
                row.append(b)
                break
        else:
            rows.append([b])
    rows.sort(key=lambda row: min(b[1] for b in row))
    ordered = [b for row in rows for b in sorted(row, key=lambda b: b[0])]
    return img, fg, ordered


def cut(img, fg, box):
    """The thing in box on a clear square, fitted into S - 2 PAD."""
    x0, y0, x1, y1 = box
    t = img.crop(box).convert("RGBA")
    t.putalpha(fg.crop(box).filter(ImageFilter.GaussianBlur(0.6)))
    # A long thing (a sword, a bow, a staff) lies across the square corner to
    # corner, as such things do in a bag, and fills it instead of a strip of it.
    if (y1 - y0) > 2.2 * (x1 - x0):
        t = t.rotate(-45, resample=Image.BICUBIC, expand=True)
        t = t.crop(t.getbbox())
    tw, th = t.size
    side = max(tw, th)
    sq = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    sq.paste(t, ((side - tw) // 2, (side - th) // 2), t)
    inner = S - 2 * PAD
    return sq.resize((inner, inner), Image.LANCZOS)


def glow(thing, colour, strength):
    a = thing.getchannel("A")
    halo = Image.new("L", (S, S), 0)
    halo.paste(a, (PAD, PAD))
    halo = halo.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(4))
    halo = halo.point(lambda v: min(255, int(v * strength)))
    layer = Image.new("RGBA", (S, S), colour + (0,))
    layer.putalpha(halo)
    return layer


def slot(r):
    """The square behind: dark, a little lighter in its middle, the rarer the warmer."""
    base = Image.new("RGBA", (S, S), (24, 20, 17, 255))
    d = ImageDraw.Draw(base)
    tint = RARITY[r][0]
    for k in range(18, 0, -1):
        f = k / 18
        c = (34, 29, 24) if tint is None else tuple(int(20 + v * 0.16 * (1 - f)) for v in tint)
        m = int(S / 2 * f)
        d.ellipse((S / 2 - m * 1.3, S / 2 - m * 1.3, S / 2 + m * 1.3, S / 2 + m * 1.3), fill=c + (255,))
    return base


def frame(img, r):
    d = ImageDraw.Draw(img)
    _, col, kind = RARITY[r]
    dark = tuple(v // 3 for v in col)
    d.rectangle((0, 0, S - 1, S - 1), outline=dark + (255,), width=3)
    d.rectangle((2, 2, S - 3, S - 3), outline=tuple(v // 2 for v in col) + (255,), width=1)
    if kind == "plain":
        return
    L = 22 if kind == "corners" else 30
    for sx in (0, 1):
        for sy in (0, 1):
            x = S - 1 if sx else 0
            y = S - 1 if sy else 0
            dx = -1 if sx else 1
            dy = -1 if sy else 1
            d.line((x, y + dy * 1, x + dx * L, y + dy * 1), fill=col + (255,), width=3)
            d.line((x + dx * 1, y, x + dx * 1, y + dy * L), fill=col + (255,), width=3)
            if kind == "ornate":
                cx, cy = x + dx * 7, y + dy * 7
                d.polygon([(cx, cy - 6), (cx + 6, cy), (cx, cy + 6), (cx - 6, cy)], fill=col + (255,),
                          outline=dark + (255,))
                gem = GEM[r]
                d.polygon([(cx, cy - 3), (cx + 3, cy), (cx, cy + 3), (cx - 3, cy)], fill=gem + (255,))
    if kind == "ornate":
        for k in range(3):   # a studded band along the top and bottom
            for y in (1, S - 2):
                x = S // 2 + (k - 1) * 12
                d.ellipse((x - 2, y - 2, x + 2, y + 2), fill=col + (255,))


def spark(img, colour):
    d = ImageDraw.Draw(img)
    x, y = S - 20, 18
    for (a, b) in (((x - 7, y), (x + 7, y)), ((x, y - 7), (x, y + 7))):
        d.line((a, b), fill=(255, 255, 230, 230), width=1)
    d.ellipse((x - 2, y - 2, x + 2, y + 2), fill=colour + (255,))


def compose(thing, r):
    img = slot(r)
    tint = RARITY[r][0]
    if tint is not None:
        img.alpha_composite(glow(thing, tint, 1.2 + 0.35 * r))
    img.alpha_composite(thing, (PAD, PAD))
    if r == 2:
        spark(img, (255, 240, 150))
    frame(img, r)
    return img


def plain(thing):
    """A picture with no rarity: the thing alone on clear, for the belt, the ground and the pouch."""
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    img.alpha_composite(thing, (PAD, PAD))
    return img


def holder(files):
    """icons.gltf: one triangle, a buffer of it inline, and every picture named."""
    import base64
    import json
    import struct
    data = struct.pack("<9f", 0, 0, 0, 0.01, 0, 0, 0, 0.01, 0)
    gltf = {
        "asset": {"version": "2.0", "generator": "tools/make_arpg_icons.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": "icons"}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
                       "min": [0, 0, 0], "max": [0.01, 0.01, 0]}],
        "bufferViews": [{"buffer": 0, "byteLength": len(data)}],
        "buffers": [{"byteLength": len(data),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(data).decode()}],
        "images": [{"uri": f} for f in sorted(files)],
    }
    with open(os.path.join(OUT, "icons.gltf"), "w") as f:
        json.dump(gltf, f, indent=1)


def main():
    show = "--blobs" in sys.argv
    got = {}
    for sheet, names in SHEETS.items():
        img, fg, boxes = things(sheet)
        if show or len(boxes) != len(names):
            print(f"{sheet}: {len(boxes)} found for {len(names)} names")
            for b in boxes:
                print("   ", b)
        for name, box in zip(names, boxes):
            if name:
                got[name] = cut(img, fg, box)
    os.makedirs(OUT, exist_ok=True)
    files = []
    for name, thing in got.items():
        if name in BASES:
            for r in range(5):
                compose(thing, r).save(os.path.join(OUT, f"icon_{name}_{r}.png"), optimize=True)
                files.append(f"icon_{name}_{r}.png")
        else:
            plain(thing).save(os.path.join(OUT, f"icon_{name}.png"), optimize=True)
            files.append(f"icon_{name}.png")
    n = len(files)
    holder(files)
    missing = [b for b in BASES if b not in got]
    print(f"{n} pictures written to {os.path.normpath(OUT)}" + (f"; no picture for {missing}" if missing else ""))


if __name__ == "__main__":
    main()
