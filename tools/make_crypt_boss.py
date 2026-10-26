# The Custode delle tombe, the crypt's last creature under the waste
# (games/arpg/mods/arpg_crypt, the trial for the Chiave della Cripta,
# mods/keys): a tall robed figure with a skull for a face, a crown of iron,
# blue fire in its eyes and a staff. Built with the helpers of
# tools/make_fps_models.py.
#
#   flatpak run org.blender.Blender --background --python tools/make_crypt_boss.py
#
#   custode   front +Y, feet on z = 0; clips idle, walk, run, shoot, attack, death
import os
import sys

import bpy
from mathutils import Matrix

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_fps_models as d  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "games", "arpg", "mods", "arpg_crypt")
K = 1.35


def custode():
    def at(v):
        return (v[0] * K, v[1] * K, v[2] * K)

    bones = [(n, p, at(h)) for n, p, h in d.biped("custode", 0.8, 1.1, 1.42, 1.34, 0.26)]
    rg = d.rig("custode_rig", bones)
    p = d.Parts(pre=Matrix.Scale(K, 4))
    # a long robe from the chest to the ground, split at the legs
    for sx, b in ((-1, "leg_L"), (1, "leg_R")):
        x = sx * 0.12
        p.limb(b, "robe", (x, 0, 0.84), (x, 0.0, 0.04), 0.24, 0.28)
    p.box("hips", "robe", (0, 0, 0.86), (0.48, 0.32, 0.22))
    p.box("chest", "robe", (0, 0.0, 1.12), (0.5, 0.32, 0.46))
    p.box("chest", "robe_dark", (0, 0.165, 1.1), (0.1, 0.01, 0.4))
    p.box("chest", "iron", (0, 0.0, 0.95), (0.52, 0.34, 0.05))   # a belt
    for sx in (-1, 1):
        p.box("chest", "iron", (sx * 0.27, 0.0, 1.33), (0.18, 0.3, 0.1))   # pauldrons
    # the head: a skull, the hood round it, an iron crown, blue eyes
    p.box("head", "robe_dark", (0, 0.0, 1.55), (0.3, 0.3, 0.32))
    p.box("head", "bone", (0, 0.1, 1.52), (0.2, 0.14, 0.22))
    p.box("head", "blue_glow", (-0.05, 0.175, 1.56), (0.05, 0.02, 0.04))
    p.box("head", "blue_glow", (0.05, 0.175, 1.56), (0.05, 0.02, 0.04))
    p.box("head", "polymer", (0, 0.175, 1.46), (0.1, 0.02, 0.03))
    for k in range(5):
        x = -0.12 + k * 0.06
        p.spike("head", "iron", (x, 0.08, 1.7), (x, 0.08, 1.82), 0.025)
    p.box("head", "iron", (0, 0.08, 1.7), (0.3, 0.2, 0.04))
    # arms in long sleeves, bony hands; the staff in the right, a blue stone on it
    for sx, b in ((-1, "arm_L"), (1, "arm_R")):
        x = sx * 0.26
        p.limb(b, "robe", (x, 0, 1.36), (x + sx * 0.04, 0.06, 0.92), 0.14, 0.14)
        p.box(b, "bone", (x + sx * 0.04, 0.08, 0.86), (0.08, 0.08, 0.1))
    p.limb("arm_R", "wood", (0.3, 0.1, 0.2), (0.3, 0.1, 1.7), 0.05, 0.05)
    p.ball("arm_R", "blue_glow", (0.3, 0.1, 1.75), 0.08, sub=1)
    mesh = p.build("custode_mesh")
    d.skin(mesh, rg)

    d.clip(rg, "idle", 2.8, {
        "chest": [(0.0, {}), (0.5, {"r": (0.05, 0, 0.03)})],
        "head": [(0.0, {}), (0.5, {"r": (-0.06, 0, -0.1)})],
    }, loop=True)
    d.clip(rg, "walk", 1.3, d.gait(0.3, 0.2, 0.02, 0.05), loop=True)
    d.clip(rg, "run", 0.9, d.gait(0.55, 0.4, 0.04, 0.15), loop=True)
    d.clip(rg, "shoot", 1.2, {
        "arm_R": [(0.0, {}), (0.4, {"r": (-1.2, 0, 0.2)}), (0.6, {"r": (-0.4, 0, 0)}), (1.0, {})],
        "arm_L": [(0.0, {}), (0.4, {"r": (-1.6, 0, -0.3)}), (1.0, {})],
        "head": [(0.0, {}), (0.4, {"r": (0.2, 0, 0)}), (1.0, {})],
    })
    d.clip(rg, "attack", 0.9, {
        "arm_R": [(0.0, {}), (0.35, {"r": (-2.4, 0, 0)}), (0.6, {"r": (0.6, 0, 0)}), (1.0, {})],
        "chest": [(0.0, {}), (0.6, {"r": (0.3, 0, 0)}), (1.0, {})],
    })
    # Its head falls, it sinks to its knees and topples forward.
    d.clip(rg, "death", 1.5, {
        "root": [(0.0, {}), (0.4, {"l": (0, -0.5, 0)}), (0.85, {"r": (-1.45, 0, 0), "l": (0, -0.6, 0)}),
                 (0.95, {"r": (-1.4, 0, 0), "l": (0, -0.62, 0)})],
        "leg_L": [(0.0, {}), (0.4, {"r": (-1.4, 0, 0)})],
        "leg_R": [(0.0, {}), (0.4, {"r": (-1.3, 0, 0)})],
        "head": [(0.0, {}), (0.3, {"r": (0.5, 0, 0)}), (0.8, {"r": (0.6, 0, 0.3)})],
        "arm_L": [(0.0, {}), (0.8, {"r": (-2.6, 0, -0.3)})],
        "arm_R": [(0.0, {}), (0.8, {"r": (-2.6, 0, 0.3)})],
    })
    d.export([rg, mesh], os.path.join(OUT, "custode.gltf"), True)


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = d.FPS
    bpy.context.preferences.edit.keyframe_new_interpolation_type = "LINEAR"
    os.makedirs(OUT, exist_ok=True)
    d.mats()
    d.M["robe"] = d.material("robe", (0.14, 0.12, 0.16))
    d.M["robe_dark"] = d.material("robe_dark", (0.06, 0.05, 0.07))
    d.M["iron"] = d.material("iron", (0.30, 0.30, 0.32), metal=0.7)
    custode()
    print("the custode written to", os.path.normpath(OUT))
