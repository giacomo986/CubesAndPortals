# The front's guns as the robot holds them seen from outside: clips of the
# arms alone (upper_arm, forearm, hand, left and right), which the client
# lays over whatever the legs are doing (arms_only_clip in src/client).
#
#   flatpak run org.blender.Blender --background --python tools/make_front_poses.py
#
# For each gun: hold_<gun> (at the ready, nose down), aim_<gun> (stock to the
# shoulder, down the sights), fire_<gun> (aimed, its kick; played round while
# the trigger is held), reload_<gun>; and fire_grenade, the throw. The names
# are the first-person arms' too (tools/make_front_models.py), so one
# core.player_clip moves both.
#
# The gun is drawn in hand_R as the hand is turned from its rest
# (draw_held_block): held level, the hand keeps its rest turn, and a gun's
# pitch is the hand's. Each frame the wrists are put where the gun wants
# them by IK, the right hand turned as the gun is, the left reaching for the
# gun's fore end; the arm bones' poses are baked into keys and nothing else
# is keyed. Written into the robot and its species (mods/robot) by
# tools/make_front_poses.sh with tools/gltf_add_clips.py, which keeps their
# meshes as they are.
import json
import math
import os

import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ROBOT = os.path.join(ROOT, "mods", "robot")
TARGETS = ["robot.gltf", "robot_human.gltf", "robot_mole.gltf", "robot_frog.gltf"]
OUT = os.path.join(os.path.expanduser("~/kart_assets"), "front_poses")
FPS = 30
ARM_BONES = ["upper_arm_L", "forearm_L", "hand_L", "upper_arm_R", "forearm_R", "hand_R"]

# Where the right wrist goes (Blender's frame: x right, y front, z up, feet at
# the origin) and how the gun pitches (nose up +), and where the left wrist
# goes along the gun from the right one (its own frame: x right, y along the
# barrel, z up). The robot's arms are short (0.57 from shoulder to wrist): the
# left hand is on the gun's middle, not out at its muzzle.
POSES = {
    "long": {
        "hold": ((0.15, 0.22, 0.95), -0.45, (-0.12, 0.25, 0.0)),
        "aim": ((0.1, 0.2, 1.22), 0.0, (-0.1, 0.24, -0.04)),
    },
    "pistol": {
        "hold": ((0.2, 0.22, 0.98), -0.55, None),
        "aim": ((0.04, 0.38, 1.3), 0.0, (-0.06, -0.02, -0.04)),
    },
    "launcher": {
        "hold": ((0.16, 0.06, 1.12), -0.25, (-0.06, 0.3, -0.06)),
        "aim": ((0.15, 0.04, 1.2), 0.0, (-0.06, 0.3, -0.08)),
    },
}
KIND = {"rifle": "long", "lmg": "long", "sniper": "long", "pistol": "pistol", "launcher": "launcher"}
FIRE = {"pistol": (0.28, 0.06), "rifle": (0.1, 0.025), "lmg": (0.1, 0.03), "sniper": (1.2, 0.07),
        "launcher": (0.6, 0.08)}
RELOAD = {"pistol": 1.2, "rifle": 2.0, "lmg": 4.0, "sniper": 3.0, "launcher": 2.5}
# Where the left hand goes to reload, along the gun from the right wrist.
MAGAZINE = {"long": (0.0, 0.1, -0.16), "pistol": (0.0, -0.02, -0.14), "launcher": (0.0, -0.45, 0.1)}
POUCH = (-0.18, 0.12, 0.92)   # on the belt, left of the buckle


def lerp(a, b, u):
    return tuple(x + (y - x) * u for x, y in zip(a, b))


def curve(t, keys):
    if t <= keys[0][0]:
        return keys[0][1]
    for (t0, a), (t1, b) in zip(keys, keys[1:]):
        if t <= t1:
            u = (t - t0) / (t1 - t0) if t1 > t0 else 1.0
            u = u * u * (3 - 2 * u)   # eased
            return lerp(a, b, u) if isinstance(a, tuple) else a + (b - a) * u
    return keys[-1][1]


def setup():
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = FPS
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROBOT, "robot.gltf"))
    rg = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    for o in list(bpy.data.objects):
        if o is not rg and o.parent is not rg:
            bpy.data.objects.remove(o)
    ad = rg.animation_data
    if ad:
        for t in list(ad.nla_tracks):
            ad.nla_tracks.remove(t)
        ad.action = None
    # Each arm one chain for the IK: the robot's bones stop short of the next
    # one's head, on the same line, and are drawn out to meet it (as
    # make_fps_models.fp_arms does) -- the same turn, so the keys still fit.
    bpy.context.view_layer.objects.active = rg
    bpy.ops.object.mode_set(mode="EDIT")
    eb = rg.data.edit_bones
    for up, fore, hand in (("upper_arm_R", "forearm_R", "hand_R"), ("upper_arm_L", "forearm_L", "hand_L")):
        eb[up].tail = eb[fore].head
        eb[fore].tail = eb[hand].head
    bpy.ops.object.mode_set(mode="OBJECT")
    targets = {}
    for side, fore, hand, pole in (("R", "forearm_R", "hand_R", (1.2, -0.6, 0.6)),
                                   ("L", "forearm_L", "hand_L", (-1.2, -0.6, 0.6))):
        t = bpy.data.objects.new(f"target_{side}", None)
        bpy.context.scene.collection.objects.link(t)
        t.rotation_mode = "QUATERNION"
        pl = bpy.data.objects.new(f"pole_{side}", None)
        bpy.context.scene.collection.objects.link(pl)
        pl.location = pole
        ik = rg.pose.bones[fore].constraints.new("IK")
        ik.target, ik.pole_target, ik.chain_count = t, pl, 2
        ik.pole_angle = -math.pi / 2
        cr = rg.pose.bones[hand].constraints.new("COPY_ROTATION")
        cr.target = t
        targets[side] = t
    for pb in rg.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return rg, targets


def capture(rg, targets, seconds, state):
    """state(t) -> (right wrist, gun pitch, gun roll, left wrist or None)."""
    W = rg.matrix_world
    rest = {b.name: W @ b.matrix_local for b in rg.data.bones}
    frames = []
    n = max(1, round(seconds * FPS))
    for f in range(n + 1):
        right, pitch, roll, left = state(f / FPS)
        turn = Matrix.Rotation(pitch, 4, "X") @ Matrix.Rotation(roll, 4, "Y")
        for side, at, base in (("R", right, rest["hand_R"]), ("L", left, rest["hand_L"])):
            t = targets[side]
            if at is None:   # the left hand let hang: its rest, where it is
                at = base.to_translation()
            t.location = Vector(at)
            spin = turn if side == "R" else Matrix.Rotation(-1.2, 4, "X")
            t.rotation_quaternion = (spin.to_3x3() @ base.to_3x3().normalized()).to_quaternion()
        for pb in rg.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        # A bend to start from: the arms are straight at rest.
        for fore in ("forearm_R", "forearm_L"):
            rg.pose.bones[fore].rotation_quaternion = Matrix.Rotation(0.9, 3, "X").to_quaternion()
        bpy.context.view_layer.update()
        frames.append({b: rg.pose.bones[b].matrix.copy() for b in ARM_BONES})
        got = (W @ rg.pose.bones["hand_R"].head - Vector(right)).length
        if f == n and got > 0.02:
            print(f"front pose: right wrist {got:.3f} short")
    return frames


def bake(rg, name, frames):
    """The captured world poses as keys on the arm bones alone."""
    rest = {b.name: b.matrix_local.copy() for b in rg.data.bones}
    parent = {b.name: (b.parent.name if b.parent else None) for b in rg.data.bones}
    act = bpy.data.actions.new(name)
    rg.animation_data_create()
    rg.animation_data.action = act
    prev = {}
    for f, pose in enumerate(frames):
        for b in ARM_BONES:
            pb = rg.pose.bones[b]
            R, P = rest[b], pose[b]
            p = parent[b]
            if p in pose:
                Pp = pose[p]
            else:
                Pp = rest[p]   # the chest: not moved
            basis = (rest[p].inverted() @ R).inverted() @ Pp.inverted() @ P
            loc, rot, sc = basis.decompose()
            if b in prev and prev[b].dot(rot) < 0:
                rot.negate()
            prev[b] = rot
            pb.location, pb.rotation_quaternion, pb.scale = loc, rot, sc
            for path in ("location", "rotation_quaternion", "scale"):
                pb.keyframe_insert(path, frame=1 + f)
    rg.animation_data.action = None
    tr = rg.animation_data.nla_tracks.new()
    tr.name = name
    tr.strips.new(name, 1, act)


def gun_states(gun):
    kind = KIND[gun]
    P = POSES[kind]
    hold_at, hold_pitch, hold_left = P["hold"]
    aim_at, _, aim_left = P["aim"]

    def left_of(at, pitch, local):
        if local is None:
            return None
        return tuple(Vector(at) + Matrix.Rotation(pitch, 3, "X") @ Vector(local))

    def still(at, pitch, left):
        return lambda t: (at, pitch, 0.0, left_of(at, pitch, left))

    states = {"hold": (2.0, still(hold_at, hold_pitch, hold_left)),
              "aim": (2.0, still(aim_at, 0.0, aim_left))}
    every, kick = FIRE[gun]

    def fire(t):
        k = curve(t, [(0, 0.0), (min(0.03, every / 3), 1.0), (every, 0.0)])
        at = (aim_at[0], aim_at[1] - kick * k, aim_at[2] + kick * 0.3 * k)
        pitch = 0.12 * k * (3 if gun == "sniper" else 1)
        return at, pitch, 0.0, left_of(at, pitch, aim_left)
    states["fire"] = (every, fire)

    total = RELOAD[gun]
    mag = MAGAZINE[kind]

    def reload(t):
        u = t / total
        # Tipped and raised to look at, the left hand to the magazine, down
        # to the pouch, back with a fresh one, home.
        tilt = curve(u, [(0, 0.0), (0.12, 1.0), (0.85, 1.0), (1.0, 0.0)])
        at = lerp(hold_at, (hold_at[0] - 0.05, hold_at[1] + 0.02, hold_at[2] + 0.12), tilt)
        pitch = hold_pitch + 0.3 * tilt
        roll = -0.5 * tilt
        on_mag = left_of(at, pitch, mag)
        home = left_of(at, pitch, hold_left) if hold_left else None
        if u < 0.25:
            left = lerp(home or POUCH, on_mag, curve(u, [(0, 0.0), (0.25, 1.0)]))
        elif u < 0.45:
            left = lerp(on_mag, POUCH, curve(u, [(0.25, 0.0), (0.45, 1.0)]))
        elif u < 0.65:
            left = lerp(POUCH, on_mag, curve(u, [(0.45, 0.0), (0.65, 1.0)]))
        elif u < 0.85:
            left = on_mag
        else:
            left = lerp(on_mag, home or POUCH, curve(u, [(0.85, 0.0), (1.0, 1.0)]))
        return at, pitch, roll, left
    states["reload"] = (total, reload)
    return states


def throw_state(t):
    # Back over the shoulder and over: the grenade leaves at 0.42 s.
    at = curve(t, [(0, (0.27, 0.05, 0.75)), (0.25, (0.3, -0.25, 1.45)), (0.42, (0.25, 0.35, 1.35)),
                   (0.6, (0.22, 0.4, 0.95)), (0.9, (0.27, 0.05, 0.75))])
    pitch = curve(t, [(0, 0.0), (0.25, 1.2), (0.42, -0.4), (0.6, -1.0), (0.9, 0.0)])
    return at, pitch, 0.0, None


def export(rg, path):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    rg.select_set(True)
    for o in rg.children:
        o.select_set(True)
    bpy.context.view_layer.objects.active = rg
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_yup=True, export_animations=True, export_animation_mode="NLA_TRACKS",
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_skins=True, export_def_bones=False, export_materials="NONE")


def arms_only(path):
    """Drop every channel but the arm bones' from the exported clips: a clip
    that keys the legs is played instead of the walk, not over it."""
    with open(path) as f:
        doc = json.load(f)
    names = [n.get("name") for n in doc["nodes"]]
    for a in doc.get("animations", []):
        a["channels"] = [c for c in a["channels"] if names[c["target"]["node"]] in ARM_BONES]
    with open(path, "w") as f:
        json.dump(doc, f)


if __name__ == "__main__":
    rg, targets = setup()
    clips = []
    for gun in KIND:
        for state, (seconds, fn) in gun_states(gun).items():
            name = f"{state}_{gun}"
            bake(rg, name, capture(rg, targets, seconds, fn))
            clips.append(name)
    bake(rg, "fire_grenade", capture(rg, targets, 0.9, throw_state))
    clips.append("fire_grenade")
    for pb in rg.pose.bones:
        for c in list(pb.constraints):
            pb.constraints.remove(c)
        pb.matrix_basis = Matrix.Identity(4)
    os.makedirs(OUT, exist_ok=True)
    src = os.path.join(OUT, "front_poses.gltf")
    export(rg, src)
    arms_only(src)
    # Merged into the robot and its species outside Blender (its own Python
    # may not be the system's): tools/make_front_poses.sh.
    with open(os.path.join(OUT, "clips.txt"), "w") as f:
        f.write(" ".join(clips))
    print("front poses:", len(clips), "clips in", src)
