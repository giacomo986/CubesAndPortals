# The track's power-ups as they lie on the road (games/track/mods/track_game):
# the item box a kart drives through, the banana dropped behind one and the two
# shells thrown at others. Rigid, feet on z = 0, built with the helpers of
# tools/make_fps_models.py and exported into games/track/mods/track_core.
#
#   flatpak run org.blender.Blender --background --python tools/make_track_items.py
#
#   item_box       a cube of six coloured faces with a question mark on four,
#                  floating at a kart's height
#   banana         a peel lying on its side
#   shell_green    flies straight and bounces off the walls
#   shell_red      steers for the nearest kart ahead
import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_fps_models as d  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "games", "track", "mods", "track_core")

# A question mark, five by seven, top row first.
QUESTION = [
    ".###.",
    "#...#",
    "....#",
    "...#.",
    "..#..",
    ".....",
    "..#..",
]


def mats():
    for name, rgb, kw in [
        ("box_core", (0.10, 0.18, 0.55), {"glow": True}),
        ("box_red", (1.00, 0.25, 0.20), {"glow": True}),
        ("box_yellow", (1.00, 0.85, 0.15), {"glow": True}),
        ("box_green", (0.30, 1.00, 0.35), {"glow": True}),
        ("box_cyan", (0.25, 0.85, 1.00), {"glow": True}),
        ("box_blue", (0.35, 0.40, 1.00), {"glow": True}),
        ("box_magenta", (1.00, 0.35, 0.95), {"glow": True}),
        ("box_edge", (0.95, 0.95, 1.00), {"glow": True}),
        ("peel", (0.98, 0.84, 0.18), {}),
        ("peel_tip", (0.30, 0.20, 0.08), {}),
        ("shell_green", (0.10, 0.70, 0.18), {}),
        ("shell_red", (0.85, 0.08, 0.06), {}),
        ("shell_rim", (0.96, 0.94, 0.86), {}),
    ]:
        d.M[name] = d.material(name, rgb, **kw)


def item_box(p):
    c, h = 0.9, 0.4   # the middle's height, half a side
    p.box(None, "box_core", (0, 0, c), (2 * h, 2 * h, 2 * h))
    # A colour to each face, a little proud of the core.
    faces = [((h + 0.01, 0, 0), (0.02, 0.7, 0.7), "box_red"),
             ((-h - 0.01, 0, 0), (0.02, 0.7, 0.7), "box_green"),
             ((0, h + 0.01, 0), (0.7, 0.02, 0.7), "box_yellow"),
             ((0, -h - 0.01, 0), (0.7, 0.02, 0.7), "box_cyan"),
             ((0, 0, h + 0.01), (0.7, 0.7, 0.02), "box_blue"),
             ((0, 0, -h - 0.01), (0.7, 0.7, 0.02), "box_magenta")]
    for (x, y, z), size, mat in faces:
        p.box(None, mat, (x, y, c + z), size)
    # The twelve edges.
    for a in (-h, h):
        for b in (-h, h):
            p.box(None, "box_edge", (0, a, c + b), (2 * h + 0.06, 0.06, 0.06))
            p.box(None, "box_edge", (a, 0, c + b), (0.06, 2 * h + 0.06, 0.06))
            p.box(None, "box_edge", (a, b, c), (0.06, 0.06, 2 * h + 0.06))
    # The question mark on the four sides, in pixels of 0.08.
    px = 0.08
    for turn in range(4):
        ang = turn * math.pi / 2
        for row, line in enumerate(QUESTION):
            for col, ch in enumerate(line):
                if ch != "#":
                    continue
                u = (col - 2) * px            # across the face
                v = (3 - row) * px            # up it
                n = h + 0.03                  # out of it
                x = math.cos(ang) * n - math.sin(ang) * u
                y = math.sin(ang) * n + math.cos(ang) * u
                size = (0.02, px, px) if turn % 2 == 0 else (px, 0.02, px)
                p.box(None, "box_edge", (x, y, c + v), size)


def banana(p):
    # Along an arc lying on its side, the tips up.
    pts = []
    for i in range(7):
        t = i / 6 * 2 - 1
        pts.append((t * 0.34, 0.0, 0.09 + 0.16 * t * t))
    for a, b in zip(pts, pts[1:]):
        p.limb(None, "peel", a, b, 0.13, 0.11)
        p.ball(None, "peel", b, 0.065, sub=1)
    p.ball(None, "peel", pts[0], 0.065, sub=1)
    p.spike(None, "peel_tip", pts[0], (pts[0][0] - 0.06, 0, pts[0][2] + 0.06), 0.03)
    p.spike(None, "peel_tip", pts[-1], (pts[-1][0] + 0.06, 0, pts[-1][2] + 0.06), 0.03)
    # Three flaps of the peel lying open about the middle.
    for k in range(3):
        a = k * 2 * math.pi / 3 + 0.4
        p.limb(None, "peel", (0, 0, 0.06), (math.cos(a) * 0.28, math.sin(a) * 0.28, 0.02), 0.1, 0.03)


def shell(colour):
    def make(p):
        p.cyl(None, "shell_rim", (0, 0, 0.1), 0.3, 0.12, "Z", seg=16)
        p.ball(None, colour, (0, 0, 0.16), 0.29, squash=(1, 1, 0.75), sub=2)
        # Six plates round a seventh on top.
        p.ball(None, "shell_rim", (0, 0, 0.375), 0.07, squash=(1, 1, 0.35), sub=1)
        for k in range(6):
            a = k * math.pi / 3
            p.ball(None, "shell_rim", (math.cos(a) * 0.17, math.sin(a) * 0.17, 0.33), 0.055,
                   squash=(1, 1, 0.4), sub=1)
    return make


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    d.OUT = OUT
    d.mats()
    mats()
    for i, (name, make) in enumerate([("item_box", item_box), ("banana", banana),
                                       ("shell_green", shell("shell_green")),
                                       ("shell_red", shell("shell_red"))]):
        d.rigid(name, make, 2.0 * i)
    print("track items written to", os.path.normpath(OUT))
