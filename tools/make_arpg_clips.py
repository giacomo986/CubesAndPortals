# The robot's clips for games/arpg: a bow drawn and loosed, a staff raised and
# thrust, a nova, a war cry, a dagger's stab and a spinning slash. The swords',
# axes' and maces' blows are the robot's own sword_slash and sword_overhead.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_arpg_clips.py"
#   for m in robot robot_human robot_mole robot_frog; do
#       python3 tools/gltf_add_clips.py mods/robot/$m.gltf ~/kart_assets/arpg_clips_$m.gltf \
#           bow_shoot staff_cast cast_nova war_cry dagger_stab spin_slash; done
#
# Made once for each of the four bodies, from its own file: Blender writes a
# species' arms with another turn at rest, so a key is only good for the file
# it was made from. A clip keeps only the turns of the bones it moves (the
# rest keep walking under it).
# Each pose is turns about world axes, bone by bone from the rest pose, parents
# first, so a forearm turns with its upper arm. Only rotations are keyed: the
# species' bones are where theirs are, and a clip of turns fits them all. The
# robot faces +Y, its right arm at +x; an arm turned about +X by a positive
# angle swings forward, about Y by a negative one the right arm goes out.
import math
import os

import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.expanduser("~/kart_assets/arpg_clips_{}.gltf")
BODIES = ("robot", "robot_human", "robot_mole", "robot_frog")
FPS = 24
X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)
ORDER = ["hips", "spine", "chest", "neck", "head", "upper_arm_R", "forearm_R", "hand_R",
         "upper_arm_L", "forearm_L", "hand_L"]


def d(a):
    return math.radians(a)


# Poses: bone -> [(axis, degrees), ...]. Keys: (frame, pose).
READY_BOW = {"upper_arm_L": [(X, 85), (Z, -10)], "upper_arm_R": [(X, 80), (Z, 20)], "forearm_R": [(X, 20)],
             "chest": [(Z, 20)], "head": [(Z, -15)]}
DRAWN_BOW = {"upper_arm_L": [(X, 88), (Z, -12)], "upper_arm_R": [(X, 100), (Z, 35)], "forearm_R": [(X, -120)],
             "chest": [(Z, 25)], "head": [(Z, -20)]}
LOOSED_BOW = {"upper_arm_L": [(X, 88), (Z, -12)], "upper_arm_R": [(X, 75), (Z, 55)], "forearm_R": [(X, -40)],
              "chest": [(Z, 25)], "head": [(Z, -20)]}
CLIPS = {
    "bow_shoot": [(0, READY_BOW), (6, DRAWN_BOW), (11, DRAWN_BOW), (12, LOOSED_BOW), (16, READY_BOW)],
    "staff_cast": [
        (0, {"upper_arm_R": [(X, 20)]}),
        (4, {"upper_arm_R": [(X, 150)], "chest": [(X, -8)], "upper_arm_L": [(X, 20)]}),
        (8, {"upper_arm_R": [(X, 75)], "forearm_R": [(X, 10)], "chest": [(X, 10)], "upper_arm_L": [(X, 35)]}),
        (12, {"upper_arm_R": [(X, 20)]}),
    ],
    "cast_nova": [
        (0, {}),
        (5, {"upper_arm_R": [(Y, -160)], "upper_arm_L": [(Y, 160)], "chest": [(X, -10)], "head": [(X, -15)]}),
        (9, {"upper_arm_R": [(Y, -55)], "upper_arm_L": [(Y, 55)], "chest": [(X, 15)], "head": [(X, 10)],
             "forearm_R": [(Y, -20)], "forearm_L": [(Y, 20)]}),
        (15, {}),
    ],
    "war_cry": [
        (0, {}),
        (5, {"upper_arm_R": [(Y, -130), (X, -20)], "upper_arm_L": [(Y, 130), (X, -20)],
             "forearm_R": [(Y, -30)], "forearm_L": [(Y, 30)], "chest": [(X, -12)], "head": [(X, -25)]}),
        (14, {"upper_arm_R": [(Y, -135), (X, -25)], "upper_arm_L": [(Y, 135), (X, -25)],
              "forearm_R": [(Y, -35)], "forearm_L": [(Y, 35)], "chest": [(X, -15)], "head": [(X, -30)]}),
        (19, {}),
    ],
    "dagger_stab": [
        (0, {"upper_arm_R": [(X, 15)]}),
        (3, {"upper_arm_R": [(X, 35), (Z, 10)], "forearm_R": [(X, -70)], "chest": [(Z, 12)]}),
        (6, {"upper_arm_R": [(X, 85)], "forearm_R": [(X, 0)], "chest": [(Z, -12)]}),
        (9, {"upper_arm_R": [(X, 15)]}),
    ],
    "spin_slash": [
        (0, {"upper_arm_R": [(Y, -85)], "upper_arm_L": [(Y, 40)]}),
        (4, {"hips": [(Z, -120)], "upper_arm_R": [(Y, -90)], "upper_arm_L": [(Y, 40)]}),
        (8, {"hips": [(Z, -240)], "upper_arm_R": [(Y, -90)], "upper_arm_L": [(Y, 40)]}),
        (12, {"hips": [(Z, -360)], "upper_arm_R": [(Y, -85)], "upper_arm_L": [(Y, 40)]}),
    ],
}


def posed(rig, pose):
    """Sets the rig to `pose` and returns each bone's basis rotation."""
    W3 = rig.matrix_world.to_3x3().normalized()
    for pb in rig.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    for name in ORDER:
        if name not in pose:
            continue
        pb = rig.pose.bones[name]
        for axis, deg in pose[name]:
            a = (W3.inverted() @ Vector(axis)).normalized()
            m = pb.matrix.copy()
            h = m.translation.copy()
            pb.matrix = Matrix.Translation(h) @ Matrix.Rotation(d(deg), 4, a) @ Matrix.Translation(-h) @ m
            bpy.context.view_layer.update()
    return {pb.name: pb.matrix_basis.to_quaternion() for pb in rig.pose.bones}


def main(body):
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, "mods", "robot", body + ".gltf"))
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    rig.animation_data_create()
    for tr in list(rig.animation_data.nla_tracks):
        rig.animation_data.nla_tracks.remove(tr)
    for pb in rig.pose.bones:
        pb.rotation_mode = "QUATERNION"
    W = rig.matrix_world

    # Every pose first, with no action or track on the rig: one would be played over them.
    all_poses = {}
    for clip, keys in CLIPS.items():
        all_poses[clip] = []
        for frame, pose in keys:
            q = posed(rig, pose)
            hand = W @ rig.pose.bones["hand_R"].head
            print(f"CLIP {clip} f{frame}: hand_R at {tuple(round(v, 2) for v in hand)}")
            all_poses[clip].append((frame, q))
    for clip, poses in all_poses.items():
        act = bpy.data.actions.new(clip)
        rig.animation_data.action = act
        prev = {}
        for frame, q in poses:
            for name in ORDER:
                pb = rig.pose.bones[name]
                r = q[name]
                if name in prev and prev[name].dot(r) < 0:
                    r = -r   # the short way round between keys
                prev[name] = r
                pb.rotation_quaternion = r
                pb.keyframe_insert("rotation_quaternion", frame=frame + 1)
        track = rig.animation_data.nla_tracks.new()
        track.name = clip
        track.strips.new(clip, 1, act)
        rig.animation_data.action = None

    bpy.context.scene.render.fps = FPS
    for o in bpy.data.objects:
        o.select_set(o.type == "ARMATURE")
    out = OUT.format(body)
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS",
                              export_yup=True, export_skins=True, export_materials="NONE")
    only_turns(out)
    if body == "robot":
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "arpg_clips.blend"))


def only_turns(path):
    """Drops every channel but the rotations of bones a clip moves from their rest."""
    import json
    import struct
    with open(path) as f:
        g = json.load(f)
    with open(os.path.join(os.path.dirname(path), g["buffers"][0]["uri"]), "rb") as f:
        data = f.read()

    def floats(i):
        a = g["accessors"][i]
        v = g["bufferViews"][a["bufferView"]]
        n = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}[a["type"]] * a["count"]
        return struct.unpack_from("<%df" % n, data, v.get("byteOffset", 0) + a.get("byteOffset", 0))

    for an in g["animations"]:
        keep = []
        for c in an["channels"]:
            if c["target"]["path"] != "rotation":
                continue
            rest = g["nodes"][c["target"]["node"]].get("rotation", [0, 0, 0, 1])
            q = floats(an["samplers"][c["sampler"]]["output"])
            if any(min(sum(abs(q[i * 4 + k] - rest[k]) for k in range(4)),
                       sum(abs(q[i * 4 + k] + rest[k]) for k in range(4))) > 1e-3 for i in range(len(q) // 4)):
                keep.append(c)
        an["channels"] = keep
    with open(path, "w") as f:
        json.dump(g, f)


for b in BODIES:
    main(b)
