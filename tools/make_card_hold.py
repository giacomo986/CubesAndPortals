# The robot holding a hand of cards, for mods/cards: a clip "cards" for the
# body everybody else sees (mods/robot/robot.gltf) and one of the same name for
# the arms seen in first person (mods/robot/robot_arms.gltf).
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_card_hold.py"
#   python3 tools/gltf_add_clips.py mods/robot/robot.gltf ~/kart_assets/cards_body.gltf cards
#   python3 tools/gltf_add_clips.py mods/robot/robot_arms.gltf ~/kart_assets/cards_arms.gltf cards
#
# Both hands meet in front, low, where the fan's grip is: the mod draws the
# cards there (GRIP_BODY and GRIP_EYE in mods/cards/init.luau, in glTF axes:
# x right, y up, -z ahead). The arms reach by IK, solved here frame by frame and
# baked into plain keys; the robot breathes a little, two seconds a loop.
# Blender is Z up and the robot faces +Y: glTF (x, y, z) is Blender (x, -z, y).
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ROBOT = os.path.join(ROOT, "mods", "robot")
OUT = os.path.expanduser("~/kart_assets")
FPS, SECONDS = 24, 2.0

# Blender world coordinates. The body: feet at the origin. The arms: the eye
# at the origin.
BODY = {"file": "robot.gltf", "out": "cards_body.gltf",
        "wrist_R": (0.075, 0.30, 1.02), "wrist_L": (-0.075, 0.30, 1.02),
        "pole_R": (0.7, -0.2, 0.6), "pole_L": (-0.7, -0.2, 0.6), "head": -0.35}
ARMS = {"file": "robot_arms.gltf", "out": "cards_arms.gltf",
        "wrist_R": (0.075, 0.30, -0.29), "wrist_L": (-0.075, 0.30, -0.29),
        "pole_R": (0.8, -0.1, -0.9), "pole_L": (-0.8, -0.1, -0.9), "head": None}


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)


def make(spec):
    clear()
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROBOT, spec["file"]))
    rg = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    if rg.animation_data:
        for tr in list(rg.animation_data.nla_tracks):
            rg.animation_data.nla_tracks.remove(tr)
    W = rg.matrix_world.copy()

    # Each arm one chain for the IK: the bones stop short of the next one's
    # head, on the same line, and are drawn out to meet it (their turn at rest
    # is the same, so the keys are the model's own).
    bpy.context.view_layer.objects.active = rg
    bpy.ops.object.mode_set(mode="EDIT")
    eb = rg.data.edit_bones
    for up, fore, hand in (("upper_arm_R", "forearm_R", "hand_R"), ("upper_arm_L", "forearm_L", "hand_L")):
        eb[up].tail = eb[fore].head
        eb[fore].tail = eb[hand].head
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in rg.pose.bones:
        pb.rotation_mode = "QUATERNION"

    targets = {}
    for side in ("R", "L"):
        t = bpy.data.objects.new(f"target_{side}", None)
        bpy.context.scene.collection.objects.link(t)
        pl = bpy.data.objects.new(f"pole_{side}", None)
        bpy.context.scene.collection.objects.link(pl)
        pl.location = spec[f"pole_{side}"]
        ik = rg.pose.bones[f"forearm_{side}"].constraints.new("IK")
        ik.target, ik.pole_target, ik.chain_count = t, pl, 2
        ik.pole_angle = -math.pi / 2
        targets[side] = t

    arm_bones = [b for b in ("upper_arm_R", "forearm_R", "hand_R", "upper_arm_L", "forearm_L", "hand_L")]
    keyed = arm_bones + (["head"] if spec["head"] is not None else [])
    frames = round(FPS * SECONDS)
    baked = []
    for f in range(frames + 1):
        breath = 0.006 * math.sin(2 * math.pi * f / frames)
        for side in ("R", "L"):
            targets[side].location = Vector(spec[f"wrist_{side}"]) + Vector((0, 0, breath))
        for pb in rg.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        # a bend to start from: the arms are straight at rest
        for side in ("R", "L"):
            rg.pose.bones[f"forearm_{side}"].rotation_quaternion = Matrix.Rotation(0.9, 3, "X").to_quaternion()
        if spec["head"] is not None:
            rg.pose.bones["head"].rotation_quaternion = Matrix.Rotation(spec["head"], 3, "X").to_quaternion()
        bpy.context.view_layer.update()
        baked.append({n: rg.pose.bones[n].matrix.copy() for n in keyed})

    for side in ("R", "L"):
        got = W @ baked[0][f"hand_{side}"].to_translation()
        short = (got - Vector(spec[f"wrist_{side}"])).length
        tip = W @ (baked[0][f"hand_{side}"] @ Vector((0, rg.data.bones[f"hand_{side}"].length, 0)))
        print(f"{spec['file']}: wrist_{side} at {tuple(round(v, 3) for v in got)} ({short:.3f} short), "
              f"fist end {tuple(round(v, 3) for v in tip)}")

    for side in ("R", "L"):
        for c in list(rg.pose.bones[f"forearm_{side}"].constraints):
            rg.pose.bones[f"forearm_{side}"].constraints.remove(c)
    act = bpy.data.actions.new("cards")
    rg.animation_data_create()
    rg.animation_data.action = act
    for f, pose in enumerate(baked):
        for pb in rg.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        for n in keyed:   # parents first: the list is in chain order
            rg.pose.bones[n].matrix = pose[n]
            bpy.context.view_layer.update()
        for n in keyed:
            pb = rg.pose.bones[n]
            pb.keyframe_insert("rotation_quaternion", frame=f + 1)
            pb.keyframe_insert("location", frame=f + 1)
    bpy.context.scene.frame_start, bpy.context.scene.frame_end = 1, frames + 1
    bpy.context.scene.render.fps = FPS

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, spec["out"]), export_format="GLTF_SEPARATE",
                              use_selection=True, export_animations=True, export_animation_mode="ACTIONS",
                              export_yup=True, export_skins=True, export_materials="NONE")
    bpy.context.scene.frame_set(1)
    return rg


if __name__ == "__main__":
    make(BODY)
    make(ARMS)
