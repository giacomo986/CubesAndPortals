# The base's last demon, the Guardian of the reactor (games/yafps, the trial
# for the Chiave dell'Assedio, mods/keys): a horned, goat-legged demon half as
# tall again as an imp, green fire in both hands, and its green fireball.
# Built with the helpers of tools/make_fps_models.py, into the same mod.
#
#   flatpak run org.blender.Blender --background --python tools/make_fps_boss.py
#
#   guardian       front +Y, feet on z = 0; clips idle, walk, run, shoot,
#                  attack, death
#   guardian_ball  what it throws: glowing, drawn at full light
import os
import sys

import bpy
from mathutils import Matrix

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_fps_models as d  # noqa: E402

K = 1.5   # how much bigger than the imp it is built from


def guardian():
    S = Matrix.Scale(K, 4)

    def at(v):
        return (v[0] * K, v[1] * K, v[2] * K)

    bones = [(n, p, at(h)) for n, p, h in d.biped("guardian", 0.8, 1.1, 1.42, 1.34, 0.3)]
    rg = d.rig("guardian_rig", bones)
    p = d.Parts(pre=S)
    # legs: a goat's, furred, bent back, ending in hooves
    for sx, b in ((-1, "leg_L"), (1, "leg_R")):
        x = sx * 0.14
        p.limb(b, "guardian_fur", (x, 0, 0.82), (x, 0.08, 0.44), 0.18, 0.2)
        p.limb(b, "guardian_fur", (x, 0.08, 0.46), (x, -0.06, 0.1), 0.12, 0.13)
        p.box(b, "polymer", (x, 0.0, 0.05), (0.14, 0.18, 0.1))
    # body: a broad chest, the fur up to the waist
    p.box("hips", "guardian_fur", (0, 0, 0.86), (0.44, 0.28, 0.2))
    p.box("chest", "guardian_skin", (0, 0.02, 1.13), (0.52, 0.3, 0.44), (0.08, 0, 0))
    p.box("chest", "guardian_dark", (0, 0.15, 1.1), (0.3, 0.05, 0.3), (0.08, 0, 0))
    for sx in (-1, 1):
        p.box("chest", "guardian_skin", (sx * 0.26, 0.0, 1.32), (0.16, 0.24, 0.14))
    # head: long, with two horns curving back and up, green eyes
    p.box("head", "guardian_skin", (0, 0.06, 1.52), (0.22, 0.26, 0.24))
    p.box("head", "guardian_dark", (0, 0.15, 1.6), (0.24, 0.1, 0.05))
    p.box("head", "eye_green", (-0.06, 0.19, 1.55), (0.05, 0.02, 0.03))
    p.box("head", "eye_green", (0.06, 0.19, 1.55), (0.05, 0.02, 0.03))
    p.box("head", "mouth", (0, 0.19, 1.45), (0.14, 0.03, 0.04))
    for sx in (-1, 1):
        p.spike("head", "bone", (sx * 0.1, 0.02, 1.64), (sx * 0.24, -0.12, 1.76), 0.045)
        p.spike("head", "bone", (sx * 0.24, -0.12, 1.76), (sx * 0.28, -0.04, 1.92), 0.03)
    # arms: heavy, clawed, green fire in each palm
    for sx, b in ((-1, "arm_L"), (1, "arm_R")):
        x = sx * 0.3
        p.limb(b, "guardian_skin", (x, 0, 1.36), (x + sx * 0.04, 0.04, 1.0), 0.14, 0.14)
        p.limb(b, "guardian_dark", (x + sx * 0.04, 0.04, 1.02), (x + sx * 0.05, 0.1, 0.7), 0.12, 0.12)
        for dx in (-0.035, 0.0, 0.035):
            p.spike(b, "bone", (x + sx * 0.05 + dx, 0.1, 0.7), (x + sx * 0.05 + dx, 0.16, 0.6), 0.016)
        p.ball(b, "green_glow", (x + sx * 0.05, 0.14, 0.72), 0.07, sub=1)
    mesh = p.build("guardian_mesh")
    d.skin(mesh, rg)

    d.clip(rg, "idle", 2.4, {
        "chest": [(0.0, {"r": (0.06, 0, 0)}), (0.5, {"r": (0.12, 0, 0.03)})],
        "head": [(0.0, {}), (0.5, {"r": (-0.08, 0, -0.12)})],
        "arm_L": [(0.0, {"r": (0.1, 0, -0.1)})],
        "arm_R": [(0.0, {"r": (0.1, 0, 0.1)})],
    }, loop=True)
    d.clip(rg, "walk", 1.2, d.gait(0.45, 0.35, 0.04, 0.1), loop=True)
    d.clip(rg, "run", 0.75, d.gait(0.8, 0.7, 0.08, 0.3), loop=True)
    d.clip(rg, "shoot", 1.2, {
        "chest": [(0.0, {}), (0.35, {"r": (-0.1, 0, 0.3)}), (0.55, {"r": (0.35, 0, -0.25)}), (1.0, {})],
        "arm_R": [(0.0, {}), (0.35, {"r": (-2.2, 0, 0.2)}), (0.55, {"r": (1.3, 0, 0)}), (1.0, {})],
        "arm_L": [(0.0, {}), (0.35, {"r": (-0.6, 0, -0.3)}), (1.0, {})],
    })
    d.clip(rg, "attack", 0.9, {
        "chest": [(0.0, {}), (0.4, {"r": (-0.2, 0, 0)}), (0.6, {"r": (0.4, 0, 0)}), (1.0, {})],
        "arm_L": [(0.0, {}), (0.4, {"r": (-2.4, 0, 0)}), (0.6, {"r": (0.8, 0, 0)}), (1.0, {})],
        "arm_R": [(0.0, {}), (0.4, {"r": (-2.4, 0, 0)}), (0.6, {"r": (0.8, 0, 0)}), (1.0, {})],
    })
    # To its knees, then forward onto its face, the fire in its hands out.
    d.clip(rg, "death", 1.4, {
        "root": [(0.0, {}), (0.4, {"l": (0, -0.5, 0)}), (0.8, {"r": (-1.45, 0, 0), "l": (0, -0.6, 0)}),
                 (0.9, {"r": (-1.4, 0, 0), "l": (0, -0.62, 0)})],
        "leg_L": [(0.0, {}), (0.4, {"r": (-1.4, 0, 0)})],
        "leg_R": [(0.0, {}), (0.4, {"r": (-1.3, 0, 0)})],
        "head": [(0.0, {}), (0.3, {"r": (-0.5, 0, 0)}), (0.8, {"r": (0.3, 0, 0.2)})],
        "arm_L": [(0.0, {}), (0.4, {"r": (-1.2, 0, -0.4)}), (0.8, {"r": (-2.8, 0, -0.3)})],
        "arm_R": [(0.0, {}), (0.4, {"r": (-1.0, 0, 0.4)}), (0.8, {"r": (-2.8, 0, 0.3)})],
    })
    d.export([rg, mesh], os.path.join(d.OUT, "guardian.gltf"), True)
    d.put_aside([rg], 6.0)


def guardian_ball(p):
    p.ball(None, "green_glow", (0, 0, 0.22), 0.24, sub=2)
    p.ball(None, "green_core", (0, 0.08, 0.22), 0.14, sub=1)


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = d.FPS
    bpy.context.preferences.edit.keyframe_new_interpolation_type = "LINEAR"
    d.mats()
    d.M["guardian_skin"] = d.material("guardian_skin", (0.62, 0.38, 0.30))
    d.M["guardian_dark"] = d.material("guardian_dark", (0.36, 0.18, 0.14))
    d.M["guardian_fur"] = d.material("guardian_fur", (0.24, 0.16, 0.10))
    guardian()
    d.rigid("guardian_ball", guardian_ball, 12.0)
    print("the guardian written to", os.path.normpath(d.OUT))
