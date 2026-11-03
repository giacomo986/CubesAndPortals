# The robot on a pony, for mods/animals: two clips for the body everybody sees
# (mods/robot/robot.gltf and the species' bodies).
#
#   ride_idle  three seconds round: astride the saddle (0.91 over the feet,
#              tools/make_pony_ridden.py), feet in the stirrups out at the
#              pony's sides, both hands forward on the reins, breathing
#   ride       0.6 s round: the same under way, the hips rising and falling
#              with the stride and the body leaning into it
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_ride_clips.py"
#   python3 tools/gltf_add_clips.py mods/robot/robot.gltf ~/kart_assets/ride_body.gltf ride_idle ride
#   (and robot_frog, robot_human, robot_mole)
#
# The IK and the baking are tools/make_cart_clips.py's. Blender world, nodes,
# the robot facing +Y, its feet (the origin) on the ground the pony stands on.
import importlib.util
import math
import os
import sys

import bpy

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("make_cart_clips", os.path.join(HERE, "make_cart_clips.py"))
cc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cc)

ANKLE = cc.ANKLE
STIRRUP = 0.47    # the stirrup's tread over the ground
SADDLE = {"hips": (0.0, -0.02, 0.98), "lean": 0.12, "roll": 0.0,
          "wrist_R": (0.10, 0.34, 1.04), "wrist_L": (-0.10, 0.34, 1.04),
          "ankle_R": (0.36, 0.04, STIRRUP + ANKLE), "ankle_L": (-0.36, 0.04, STIRRUP + ANKLE)}


def ride_idle(s):
    w = 2 * math.pi * s / 3.0
    p = dict(SADDLE)
    p["hips"] = (SADDLE["hips"][0], SADDLE["hips"][1], SADDLE["hips"][2] + 0.004 * math.sin(w))
    p["lean"] = SADDLE["lean"] + 0.015 * math.sin(w)
    return p


def ride(s):
    w = 2 * math.pi * s / 0.6
    p = dict(SADDLE)
    up = 0.03 * (1 + math.sin(w)) / 2
    p["hips"] = (SADDLE["hips"][0], SADDLE["hips"][1] + 0.01 * math.sin(w), SADDLE["hips"][2] + up)
    p["lean"] = SADDLE["lean"] + 0.06 + 0.04 * math.sin(w + 0.8)
    for side in ("wrist_R", "wrist_L"):
        x, y, z = SADDLE[side]
        p[side] = (x, y + 0.02 * math.sin(w), z + up * 0.6)
    return p


def main():
    rg, targets = cc.rig_up()
    cc.bake(rg, targets, "ride_idle", ride_idle, 3.0)
    cc.bake(rg, targets, "ride", ride, 0.6)
    for chain in targets:
        for c in list(rg.pose.bones[chain].constraints):
            rg.pose.bones[chain].constraints.remove(c)
    bpy.context.scene.render.fps = cc.FPS
    os.makedirs(cc.OUT, exist_ok=True)
    for o in bpy.context.scene.objects:
        o.select_set(o.type in ("ARMATURE", "MESH"))
    bpy.ops.export_scene.gltf(filepath=os.path.join(cc.OUT, "ride_body.gltf"), export_format="GLTF_SEPARATE",
                              use_selection=True, export_animations=True, export_animation_mode="NLA_TRACKS",
                              export_yup=True, export_skins=True, export_materials="NONE")


main()
