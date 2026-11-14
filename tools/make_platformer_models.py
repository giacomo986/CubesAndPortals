# The platform game's pipes, built in Blender with the helpers of
# tools/make_fps_models.py and exported into games/platformer/mods/
# platformer_core as rigid models: blocks drawn as a model (core.node_model).
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_platformer_models.py"
#
# Starts from an empty file and leaves the work in assets/blender/platformer.blend.
#
# A pipe is two cells wide and round, so each piece is the model of ONE cell
# and reaches half a cell past it: it is built about x = 1, the cell's +x
# wall, and the cell beside it holds a block nobody sees. A block's model is
# drawn in its cell's own unit cube (world_render.cpp): the origin is the
# cell's corner, x 0..1, height 0..1 up Blender's z, and the game's z is
# Blender's -y, so the cell's middle across the way is y = -0.5.
#
#   pipe_body   one cell of the shaft
#   pipe_top    the shaft's last cell: half shaft, half lip, the hole dark
#   pipe_hang   that turned over, for a pipe that comes down out of a ceiling
#   pipe_side   a mouth lying along x, two cells tall (about z = 1), its hole
#               facing +x: the way out of the bonus room
#   mushroom    the power-up that makes a small body big: a red cap with
#               white spots on a pale stem, two eyes, its face to the camera
#   flower      the one that gives fireballs: a stem, two leaves, a ring of
#               orange petals round a yellow face
#   fireball    a ball of flame, glowing
#
# (The level's creatures are Meshy's: tools/meshy_generate.py and
# tools/meshy_prop.py, their sources in assets/meshy/platformer.)
import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_fps_models as d  # noqa: E402

ROOT = d.ROOT
OUT = os.path.join(ROOT, "games", "platformer", "mods", "platformer_core")
BLEND = os.path.join(ROOT, "assets", "blender", "platformer.blend")

R, LIP, HOLE = 0.92, 1.0, 0.78
CX, CY = 1.0, -0.5   # the pipe's axis: the cell's +x wall, the middle across the way
SEG = 24


def shine(p, r, z, h):
    """A lighter strip down the shaft, a third of the way round on both faces."""
    for side in (1, -1):
        a = math.radians(118) * side
        p.box(None, "pipe_light", (CX + r * math.cos(a), CY + r * math.sin(a), z), (0.2, 0.04, h),
              (0, 0, a + math.pi / 2))


def body(p):
    p.cyl(None, "pipe_green", (CX, CY, 0.5), R, 1.0, "Z", seg=SEG)
    shine(p, R, 0.5, 1.0)


def top(p):
    p.cyl(None, "pipe_green", (CX, CY, 0.25), R, 0.5, "Z", seg=SEG)
    p.cyl(None, "pipe_green", (CX, CY, 0.75), LIP, 0.5, "Z", seg=SEG)
    p.cyl(None, "pipe_rim", (CX, CY, 0.995), LIP, 0.02, "Z", seg=SEG)
    p.cyl(None, "pipe_dark", (CX, CY, 1.0), HOLE, 0.02, "Z", seg=SEG)
    shine(p, R, 0.25, 0.5)
    shine(p, LIP, 0.75, 0.5)


def hang(p):
    p.cyl(None, "pipe_green", (CX, CY, 0.75), R, 0.5, "Z", seg=SEG)
    p.cyl(None, "pipe_green", (CX, CY, 0.25), LIP, 0.5, "Z", seg=SEG)
    p.cyl(None, "pipe_rim", (CX, CY, 0.005), LIP, 0.02, "Z", seg=SEG)
    p.cyl(None, "pipe_dark", (CX, CY, 0.0), HOLE, 0.02, "Z", seg=SEG)
    shine(p, R, 0.75, 0.5)
    shine(p, LIP, 0.25, 0.5)


def side(p):
    # Lying across its own cell (x 0..1), its mouth on the +x face.
    p.cyl(None, "pipe_green", (0.5, CY, 1.0), LIP, 1.0, "X", seg=SEG)
    p.cyl(None, "pipe_rim", (0.995, CY, 1.0), LIP, 0.02, "X", seg=SEG)
    p.cyl(None, "pipe_dark", (1.0, CY, 1.0), HOLE, 0.02, "X", seg=SEG)
    for s in (1, -1):
        p.box(None, "pipe_light", (0.5, CY + s * LIP * math.sin(math.radians(62)), 1.0 + LIP * math.cos(math.radians(62))),
              (1.0, 0.04, 0.2), (s * -math.radians(62), 0, 0))


def mushroom(p):
    p.cyl(None, "stem", (0, 0, 0.2), 0.2, 0.4, "Z", seg=16)
    p.ball(None, "cap_red", (0, 0, 0.42), 0.38, (1.0, 1.0, 0.72), 2)
    for x, z, y, r in ((0.0, 0.62, -0.18, 0.09), (-0.22, 0.5, -0.24, 0.07), (0.22, 0.5, -0.24, 0.07),
                       (0.0, 0.66, 0.15, 0.08), (-0.3, 0.44, 0.08, 0.06), (0.3, 0.44, 0.08, 0.06)):
        p.ball(None, "cap_white", (x, y, z), r, (1, 1, 1), 1)
    for sx in (-1, 1):
        p.box(None, "eye", (sx * 0.07, -0.19, 0.24), (0.04, 0.02, 0.1))


def flower(p):
    p.cyl(None, "leaf", (0, 0, 0.22), 0.04, 0.44, "Z", seg=8)
    for sx in (-1, 1):
        p.ball(None, "leaf", (sx * 0.14, 0, 0.16), 0.13, (1.2, 0.35, 0.5), 1)
    for k in range(8):
        a = k * math.pi / 4
        p.ball(None, "petal", (math.cos(a) * 0.18, 0, 0.6 + math.sin(a) * 0.18), 0.11, (1, 0.5, 1), 1)
    p.cyl(None, "face", (0, -0.03, 0.6), 0.14, 0.1, "Y", seg=16)
    for sx in (-1, 1):
        p.box(None, "eye", (sx * 0.05, -0.09, 0.62), (0.03, 0.02, 0.07))


def fireball(p):
    p.ball(None, "fire_out", (0, 0, 0.18), 0.18, (1, 1, 1), 2)
    p.ball(None, "fire_in", (0, -0.06, 0.2), 0.11, (1, 1, 1), 2)


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    os.makedirs(OUT, exist_ok=True)
    d.mats()
    for mname, rgb in [("pipe_green", (0.10, 0.55, 0.18)), ("pipe_light", (0.42, 0.86, 0.48)),
                       ("pipe_rim", (0.06, 0.40, 0.12)), ("pipe_dark", (0.01, 0.05, 0.02)),
                       ("stem", (0.92, 0.86, 0.70)), ("cap_red", (0.85, 0.10, 0.06)),
                       ("cap_white", (0.97, 0.97, 0.95)), ("eye", (0.04, 0.04, 0.04)),
                       ("leaf", (0.12, 0.62, 0.16)), ("petal", (0.98, 0.42, 0.06)),
                       ("face", (1.0, 0.90, 0.30))]:
        d.M[mname] = d.material(mname, rgb)
    d.M["fire_out"] = d.material("fire_out", (1.0, 0.38, 0.05), glow=True)
    d.M["fire_in"] = d.material("fire_in", (1.0, 0.92, 0.45), glow=True)
    for x, (name, make) in enumerate([("pipe_body", body), ("pipe_top", top), ("pipe_hang", hang),
                                      ("pipe_side", side), ("mushroom", mushroom), ("flower", flower),
                                      ("fireball", fireball)]):
        p = d.Parts()
        make(p)
        obj = p.build(name)
        d.export([obj], os.path.join(OUT, f"{name}.gltf"), False)
        d.put_aside([obj], 4.0 * x)
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("platformer models written to", os.path.normpath(OUT))
