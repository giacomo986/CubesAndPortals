# The chambers' sentry turret (games/chambers, chambers 12 and 13), built
# with the helpers of tools/make_fps_models.py:
#
#   turret       standing: a white egg on three dark legs, one red eye in
#                front, a gun panel on each side. Front +Y, feet on z = 0.
#                Clips idle (a slow sway), shoot (the panels out, shaking),
#                death (shrinks to nothing: it fizzles).
#   turret_down  the same lying on its side, knocked over: clips idle, death.
#
#   flatpak run org.blender.Blender --background --python tools/make_chambers_turret.py
#
# Leaves the work in assets/blender/chambers_turret.blend.
import math
import os
import sys

import bpy
from mathutils import Matrix

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_fps_models as d  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "games", "chambers", "mods", "chambers_game")
BLEND = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "chambers_turret.blend")

BODY_Z = 0.75


def parts(p, root, body, gl, gr):
    # Three legs from under the body to the floor, two in front, one behind.
    for a in (math.radians(40), math.radians(140), math.radians(270)):
        foot = (math.cos(a) * 0.3, math.sin(a) * 0.3, 0.02)
        p.limb(root, "gunmetal", (math.cos(a) * 0.06, math.sin(a) * 0.06, 0.45), foot, 0.035)
        p.ball(root, "polymer", foot, 0.04)
    p.cyl(root, "gunmetal", (0, 0, 0.42), 0.07, 0.08)
    # The egg, a dark seam round it, the eye.
    p.ball(body, "white", (0, 0, BODY_Z), 0.24, (0.85, 0.8, 1.55), 3)
    p.cyl(body, "polymer", (0, 0, BODY_Z), 0.205, 0.03, seg=24)
    p.cyl(body, "polymer", (0, 0.17, BODY_Z + 0.1), 0.06, 0.04, "Y")
    p.ball(body, "eye_red", (0, 0.19, BODY_Z + 0.1), 0.04)
    # The gun panels, each with two barrels looking ahead.
    for b, sx in ((gl, -1), (gr, 1)):
        p.box(b, "steel", (sx * 0.2, 0.0, BODY_Z), (0.05, 0.2, 0.42))
        for dz in (-0.08, 0.08):
            p.cyl(b, "polymer", (sx * 0.2, 0.12, BODY_Z + dz), 0.018, 0.08, "Y")


def standing():
    rg = d.rig("turret_rig", [("root", None, (0, 0, 0)), ("body", "root", (0, 0, 0.45)),
                              ("gun_L", "body", (-0.2, 0, BODY_Z)), ("gun_R", "body", (0.2, 0, BODY_Z))])
    p = d.Parts()
    parts(p, "root", "body", "gun_L", "gun_R")
    obj = p.build("turret")
    d.skin(obj, rg)
    # Bone axes (fps_models): rotation about Y turns about the world's up,
    # location x is the world's x.
    d.clip(rg, "idle", 3.0, {"body": [(0.25, {"r": (0, 0.08, 0)}), (0.75, {"r": (0, -0.08, 0)})]}, loop=True)
    d.clip(rg, "shoot", 0.2, {
        "gun_L": [(0.0, {"l": (-0.07, 0, 0)}), (0.5, {"l": (-0.07, 0, 0.03)}), (1.0, {"l": (-0.07, 0, 0)})],
        "gun_R": [(0.0, {"l": (0.07, 0, 0)}), (0.5, {"l": (0.07, 0, 0.03)}), (1.0, {"l": (0.07, 0, 0)})],
        "body": [(0.5, {"r": (0.03, 0, 0)})],
    }, loop=True)
    d.clip(rg, "death", 0.5, {"root": [(0.0, {"s": 1.0}), (1.0, {"s": 0.01})]})
    d.export([rg, obj], os.path.join(OUT, "turret.gltf"), True)
    return [rg, obj]


def lying():
    # On its right side, the egg's middle 0.24 over the floor, the eye ahead.
    pre = Matrix.Translation((0, 0, 0.25)) @ Matrix.Rotation(math.pi / 2, 4, "Y") @ Matrix.Translation((0, 0, -BODY_Z))
    rg = d.rig("turret_down_rig", [("root", None, (0, 0, 0))])
    p = d.Parts(pre=pre)
    parts(p, "root", "root", "root", "root")
    obj = p.build("turret_down")
    d.skin(obj, rg)
    d.clip(rg, "idle", 1.0, {}, loop=True)
    d.clip(rg, "death", 0.5, {"root": [(0.0, {"s": 1.0}), (1.0, {"s": 0.01})]})
    d.export([rg, obj], os.path.join(OUT, "turret_down.gltf"), True)
    return [rg, obj]


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = d.FPS
    bpy.context.preferences.edit.keyframe_new_interpolation_type = "LINEAR"
    d.mats()
    a = standing()
    b = lying()
    d.put_aside(b, 2.0)
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("turret models written to", os.path.normpath(OUT))
