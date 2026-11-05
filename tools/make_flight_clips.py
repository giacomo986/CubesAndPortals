# The robot flying, for mods/glider and mods/balloon: three clips for the
# body everybody sees (mods/robot/robot.gltf and the species' bodies).
#
#   glide         two seconds round: hung prone under the hang glider's keel
#                 (tools/make_flight_models.py), the body near flat, both
#                 hands on the control bar in front of the head, the legs
#                 trailing behind, swaying a little and the feet loose
#   balloon_idle  three seconds round: stood in the basket, both hands on its
#                 front rim, breathing, the weight from foot to foot
#   balloon_burn  0.8 s round: the left hand on the rim, the right up on the
#                 burner's valve handle, pulling it -- what the body does
#                 while the balloon rises (mods/balloon)
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_flight_clips.py"
#   python3 tools/gltf_add_clips.py mods/robot/robot.gltf ~/kart_assets/flight_body.gltf glide balloon_idle balloon_burn
#   (and robot_frog, robot_human, robot_mole)
#
# The IK and the baking are tools/make_cart_clips.py's. Blender world, metres,
# the robot facing +Y, its feet (the origin) where the body's box stands.
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
BAR = 1.15      # the control bar over the feet, 0.45 ahead (make_flight_models.py)
RIM = 1.0       # the basket's rim
HANDLE = (0.17, 0.12, 1.72)   # the burner's valve handle, hung under it

# Prone under the wing: the hips back and up, the body tipped 75 degrees
# forward about them, the hands out on the bar, the legs straight behind.
PRONE = {"hips": (0.0, -0.32, 1.12), "lean": 1.30, "roll": 0.0,
         "wrist_R": (0.36, 0.45, BAR + 0.03), "wrist_L": (-0.36, 0.45, BAR + 0.03),
         "ankle_R": (0.13, -1.02, 1.02), "ankle_L": (-0.13, -1.02, 1.02)}

STAND = {"hips": (0.0, -0.05, 0.73), "lean": 0.04, "roll": 0.0,
         "wrist_R": (0.24, 0.42, RIM + 0.02), "wrist_L": (-0.24, 0.42, RIM + 0.02),
         "ankle_R": (0.12, 0.0, ANKLE), "ankle_L": (-0.12, 0.0, ANKLE)}


def glide(s):
    w = 2 * math.pi * s / 2.0
    p = dict(PRONE)
    p["roll"] = 0.06 * math.sin(w)
    x, y, z = PRONE["hips"]
    p["hips"] = (x + 0.02 * math.sin(w), y, z + 0.01 * math.sin(2 * w))
    # The feet loose behind, out of step with each other.
    for side, ph in (("ankle_R", 0.0), ("ankle_L", 1.7)):
        x, y, z = PRONE[side]
        p[side] = (x, y + 0.03 * math.sin(w + ph), z + 0.05 * math.sin(w + ph))
    return p


def balloon_idle(s):
    w = 2 * math.pi * s / 3.0
    p = dict(STAND)
    x, y, z = STAND["hips"]
    p["hips"] = (x + 0.025 * math.sin(w), y, z - 0.008 * (1 + math.cos(2 * w)) / 2)
    p["roll"] = -0.03 * math.sin(w)
    p["lean"] = STAND["lean"] + 0.01 * math.sin(2 * w)
    return p


def balloon_burn(s):
    w = 2 * math.pi * s / 0.8
    p = dict(STAND)
    p["lean"] = -0.06   # looking up at the flame
    pull = 0.05 * (1 + math.sin(w)) / 2
    p["wrist_R"] = (HANDLE[0] + 0.03, HANDLE[1], HANDLE[2] + 0.02 - pull)
    p["roll"] = -0.03
    return p


def main():
    rg, targets = cc.rig_up()
    cc.bake(rg, targets, "glide", glide, 2.0)
    cc.bake(rg, targets, "balloon_idle", balloon_idle, 3.0)
    cc.bake(rg, targets, "balloon_burn", balloon_burn, 0.8)
    for chain in targets:
        for c in list(rg.pose.bones[chain].constraints):
            rg.pose.bones[chain].constraints.remove(c)
    bpy.context.scene.render.fps = cc.FPS
    os.makedirs(cc.OUT, exist_ok=True)
    for o in bpy.context.scene.objects:
        o.select_set(o.type in ("ARMATURE", "MESH"))
    bpy.ops.export_scene.gltf(filepath=os.path.join(cc.OUT, "flight_body.gltf"), export_format="GLTF_SEPARATE",
                              use_selection=True, export_animations=True, export_animation_mode="NLA_TRACKS",
                              export_yup=True, export_skins=True, export_materials="NONE")


main()
