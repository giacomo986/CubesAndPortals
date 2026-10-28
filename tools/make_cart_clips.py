# The robot in a mine cart, for mods/carts: two clips for the body everybody
# sees (mods/robot/robot.gltf).
#
#   cart_enter  a second and a fifth: from beside the cart (its left), a
#               hand on the rim, the right leg over, then the left, and
#               down onto the bench
#   cart_ride   two seconds round: sat on the bench at the back of the cart,
#               knees up, feet on its floor, a hand on each side's rim,
#               swaying a little with the track
#   sit         three seconds round: sat on a chair (mods/building), the body's
#               feet -- its origin -- on the seat 0.40 over the floor, the
#               legs down to the floor, hands on the knees, breathing
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_cart_clips.py"
#   python3 tools/gltf_add_clips.py mods/robot/robot.gltf ~/kart_assets/cart_body.gltf cart_enter cart_ride sit
#
# Arms and legs by IK, solved here frame by frame and baked into plain keys,
# as tools/make_card_hold.py does. The body's feet (its origin) are on the
# rail's top; the cart's floor is 0.19 over them and its bench 0.42
# (tools/make_cart_model.py). Blender world, metres, the robot facing +Y.
import math
import os

import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ROBOT = os.path.join(ROOT, "mods", "robot", "robot.gltf")
OUT = os.path.expanduser("~/kart_assets")
FPS = 24
ANKLE = 0.09          # the foot bone's head over the sole
FLOOR = 0.19          # the cart's floor over the rail top
RIM = 0.70            # its rim

# A pose: hips (world), the body's lean forward and roll (radians), each wrist
# and each ankle (world).
SIT = {"hips": (0.0, -0.30, 0.50), "lean": 0.05, "roll": 0.0,
       "wrist_R": (0.36, 0.06, RIM + 0.02), "wrist_L": (-0.36, 0.06, RIM + 0.02),
       "ankle_R": (0.11, 0.20, FLOOR + ANKLE), "ankle_L": (-0.11, 0.20, FLOOR + ANKLE)}
# Stepping in from the left (-X), the ground 0 there.
ENTER = [
    (0.00, {"hips": (-0.78, 0.0, 0.73), "lean": 0.0, "roll": 0.0,
            "wrist_R": (-0.47, 0.10, RIM + 0.03), "wrist_L": (-0.95, 0.02, 0.62),
            "ankle_R": (-0.69, 0.0, ANKLE), "ankle_L": (-0.87, 0.0, ANKLE)}),
    (0.25, {"hips": (-0.70, 0.0, 0.76), "lean": 0.15, "roll": 0.18,
            "wrist_R": (-0.46, 0.12, RIM + 0.03), "wrist_L": (-0.46, -0.20, RIM + 0.03),
            "ankle_R": (-0.42, 0.05, RIM + 0.14), "ankle_L": (-0.85, 0.0, ANKLE)}),
    (0.50, {"hips": (-0.32, 0.0, 0.90), "lean": 0.12, "roll": 0.10,
            "wrist_R": (-0.44, 0.15, RIM + 0.03), "wrist_L": (-0.46, -0.18, RIM + 0.03),
            "ankle_R": (0.05, 0.05, FLOOR + ANKLE), "ankle_L": (-0.62, 0.0, 0.35)}),
    (0.72, {"hips": (-0.08, 0.0, 0.94), "lean": 0.05, "roll": -0.05,
            "wrist_R": (0.40, 0.10, RIM + 0.03), "wrist_L": (-0.42, -0.05, RIM + 0.03),
            "ankle_R": (0.10, 0.05, FLOOR + ANKLE), "ankle_L": (-0.38, 0.0, RIM + 0.10)}),
    (0.92, {"hips": (0.0, -0.05, 0.86), "lean": 0.08, "roll": 0.0,
            "wrist_R": (0.38, 0.06, RIM + 0.02), "wrist_L": (-0.38, 0.06, RIM + 0.02),
            "ankle_R": (0.11, 0.10, FLOOR + ANKLE), "ankle_L": (-0.11, 0.10, FLOOR + ANKLE)}),
    (1.20, SIT),
]


def smooth(t):
    return t * t * (3 - 2 * t)


def blend(a, b, t):
    out = {}
    for k in a:
        if isinstance(a[k], tuple):
            out[k] = tuple(a[k][i] + (b[k][i] - a[k][i]) * t for i in range(3))
        else:
            out[k] = a[k] + (b[k] - a[k]) * t
    return out


def at(keys, s):
    for (t0, a), (t1, b) in zip(keys, keys[1:]):
        if s <= t1:
            return blend(a, b, smooth((s - t0) / (t1 - t0)))
    return keys[-1][1]


SEAT = 0.40   # a chair's seat over the floor
CHAIR = {"hips": (0.0, -0.08, 0.07), "lean": -0.04, "roll": 0.0,
         "wrist_R": (0.16, 0.20, 0.10), "wrist_L": (-0.16, 0.20, 0.10),
         "ankle_R": (0.12, 0.26, ANKLE - SEAT), "ankle_L": (-0.12, 0.26, ANKLE - SEAT)}


def sit(s):
    w = 2 * math.pi * s / 3.0
    p = dict(CHAIR)
    p["hips"] = (CHAIR["hips"][0], CHAIR["hips"][1], CHAIR["hips"][2] + 0.004 * math.sin(w))
    p["lean"] = CHAIR["lean"] + 0.015 * math.sin(w)
    return p


def ride(s):
    # A sway side to side and a little bob, two seconds round.
    w = 2 * math.pi * s / 2.0
    p = dict(SIT)
    p["roll"] = 0.035 * math.sin(w)
    p["hips"] = (SIT["hips"][0] + 0.01 * math.sin(w), SIT["hips"][1], SIT["hips"][2] + 0.006 * math.sin(2 * w))
    return p


def rig_up():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    bpy.ops.import_scene.gltf(filepath=ROBOT)
    rg = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    if rg.animation_data:
        rg.animation_data.action = None
        for tr in list(rg.animation_data.nla_tracks):
            rg.animation_data.nla_tracks.remove(tr)
    # Each limb one chain for the IK, the bones drawn out to meet the next.
    bpy.context.view_layer.objects.active = rg
    bpy.ops.object.mode_set(mode="EDIT")
    eb = rg.data.edit_bones
    for up, fore, end in (("upper_arm_R", "forearm_R", "hand_R"), ("upper_arm_L", "forearm_L", "hand_L"),
                          ("thigh_R", "shin_R", "foot_R"), ("thigh_L", "shin_L", "foot_L")):
        eb[up].tail = eb[fore].head
        eb[fore].tail = eb[end].head
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in rg.pose.bones:
        pb.rotation_mode = "QUATERNION"
    targets = {}
    for chain, pole in (("forearm_R", (0.9, -0.6, 0.7)), ("forearm_L", (-0.9, -0.6, 0.7)),
                        ("shin_R", (0.15, 1.5, 0.6)), ("shin_L", (-0.15, 1.5, 0.6))):
        t = bpy.data.objects.new(f"target_{chain}", None)
        bpy.context.scene.collection.objects.link(t)
        pl = bpy.data.objects.new(f"pole_{chain}", None)
        bpy.context.scene.collection.objects.link(pl)
        pl.location = pole
        ik = rg.pose.bones[chain].constraints.new("IK")
        ik.target, ik.pole_target, ik.chain_count = t, pl, 2
        ik.pole_angle = -math.pi / 2 if chain.startswith("forearm") else math.pi / 2
        targets[chain] = (t, pl)
    return rg, targets


KEYED = ["hips", "spine", "chest", "neck", "head", "upper_arm_R", "forearm_R", "hand_R",
         "upper_arm_L", "forearm_L", "hand_L", "thigh_R", "shin_R", "foot_R", "thigh_L", "shin_L", "foot_L"]


def bake(rg, targets, name, pose_at, seconds):
    W = rg.matrix_world
    Wi = W.inverted()
    frames = round(FPS * seconds)
    baked = []
    # Only this clip's keys: the tracks already made would pose the rig too.
    rg.animation_data_create()
    rg.animation_data.action = None
    for tr in rg.animation_data.nla_tracks:
        tr.mute = True
    rest_hips = rg.data.bones["hips"].matrix_local.copy()
    for f in range(frames + 1):
        p = pose_at(f / FPS)
        for pb in rg.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        # The hips where the pose has them, leant and rolled.
        turn = Matrix.Rotation(-p["roll"], 4, "Y") @ Matrix.Rotation(-p["lean"], 4, "X")
        m = turn @ rest_hips
        m.translation = Wi @ Vector(p["hips"])
        rg.pose.bones["hips"].matrix = m
        bpy.context.view_layer.update()
        for chain, key in (("forearm_R", "wrist_R"), ("forearm_L", "wrist_L"),
                           ("shin_R", "ankle_R"), ("shin_L", "ankle_L")):
            targets[chain][0].location = p[key]
        # A bend to start from: the limbs are straight at rest.
        for side in ("R", "L"):
            rg.pose.bones[f"forearm_{side}"].rotation_quaternion = Matrix.Rotation(0.9, 3, "X").to_quaternion()
            rg.pose.bones[f"shin_{side}"].rotation_quaternion = Matrix.Rotation(-0.9, 3, "X").to_quaternion()
        bpy.context.view_layer.update()
        baked.append({n: rg.pose.bones[n].matrix.copy() for n in KEYED})
    for chain in targets:
        for c in list(rg.pose.bones[chain].constraints):
            c.mute = True
    act = bpy.data.actions.new(name)
    rg.animation_data_create()
    rg.animation_data.action = act
    for f, pose in enumerate(baked):
        for pb in rg.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        for n in KEYED:   # parents first: the list is in chain order
            rg.pose.bones[n].matrix = pose[n]
            bpy.context.view_layer.update()
        # Feet flat: whatever the shin did, the foot keeps its rest turn in the world.
        for side in ("R", "L"):
            fb = rg.pose.bones[f"foot_{side}"]
            m = rg.data.bones[f"foot_{side}"].matrix_local.copy()
            m.translation = fb.matrix.translation
            fb.matrix = m
        bpy.context.view_layer.update()
        for n in KEYED:
            pb = rg.pose.bones[n]
            pb.keyframe_insert("rotation_quaternion", frame=f + 1)
            pb.keyframe_insert("location", frame=f + 1)
    for chain in targets:
        for c in list(rg.pose.bones[chain].constraints):
            c.mute = False
    track = rg.animation_data.nla_tracks.new()
    track.name = name
    track.strips.new(name, 1, act)
    rg.animation_data.action = None
    return act


def main(export=True):
    rg, targets = rig_up()
    bake(rg, targets, "cart_enter", lambda s: at(ENTER, s), ENTER[-1][0])
    bake(rg, targets, "cart_ride", ride, 2.0)
    bake(rg, targets, "sit", sit, 3.0)
    for chain in targets:
        for c in list(rg.pose.bones[chain].constraints):
            rg.pose.bones[chain].constraints.remove(c)
    bpy.context.scene.render.fps = FPS
    if export:
        os.makedirs(OUT, exist_ok=True)
        for o in bpy.context.scene.objects:
            o.select_set(o.type in ("ARMATURE", "MESH"))
        bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, "cart_body.gltf"), export_format="GLTF_SEPARATE",
                                  use_selection=True, export_animations=True, export_animation_mode="NLA_TRACKS",
                                  export_yup=True, export_skins=True, export_materials="NONE")
    return rg


if __name__ == "__main__":
    main()
