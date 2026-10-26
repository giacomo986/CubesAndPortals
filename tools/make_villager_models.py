# The sandbox's inhabitants and the little robots that help, built in Blender
# on the robot's own skeleton and exported for mods/villagers and mods/helpers.
#
#   flatpak run org.blender.Blender --background ~/kart_assets/robot18.blend \
#       --python tools/make_villager_models.py
#
# Opens the robot's file read-only and saves the work as
# ~/kart_assets/villagers.blend. Each creature is a copy of Robot_Rig -- the
# same bones, the same 1.8 (the helper at 0.7 of it) -- with a mesh of its own
# made of boxes, each weighted whole to one bone, so every clip the robot has
# plays on it: idle, walk, run, jump_loop, swim, swim_idle, climb, climb_idle,
# chop, dig, mine, sleep. And the clips of the work it does, made here:
#
#   fish    casts a rod over its head and holds it out, now and then a tug
#   gather  down on its knees picking from the ground, a hand to the chest
#   cook    stirs a pot held in the other arm
#   eat     a hand to the mouth and back, chewing
#   lie     on its back in bed, breathing
#   death   over onto its side (tip_over, tools/make_pet_models.py)
#
#   human  mods/villagers   a farmer in a straw hat, the plains' people
#   toad   mods/villagers   squat and wide, a fisher in a waistcoat
#   mole   mods/villagers   hunched, a miner's lamp and digging claws
#   helper mods/helpers     a small boxy robot with a screen for a face
#   guard     mods/villagers   iron helmet and mail, a red tabard; "attack"
#                              swings the robot's sword overhead and down
#   archer    mods/villagers   a green hood and leather, a quiver; "shoot"
#                              draws the bow held out in the right hand
#   inspector mods/villagers   a long dark coat, a tall hat and a ledger
#   mayor     mods/villagers   a blue coat, a red sash, a medal and a round hat
#   builder   mods/villagers   an apron with a nail pouch and a yellow cap
#   woodcutter mods/villagers  a checked red shirt, a beard and a knitted cap
#   miner     mods/villagers   dungarees and a yellow helmet with a lamp
#   digger    mods/villagers   an orange vest, mud to the knees, a wide hat
#   merchant  mods/villagers   a purple robe with gold trim, a purse, a turban
#   crafter   mods/villagers   a leather smock, goggles up on a bald head, a hammer
import importlib.util
import math
import os
import sys

import bpy
from mathutils import Euler, Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
BLEND = os.path.expanduser("~/kart_assets/villagers.blend")
FPS = 30

_spec = importlib.util.spec_from_file_location(
    "make_pet_models", os.path.join(os.path.dirname(os.path.abspath(__file__)), "make_pet_models.py"))
pets = importlib.util.module_from_spec(_spec)
sys.modules["make_pet_models"] = pets
_spec.loader.exec_module(pets)

# The robot's clips every one of them plays as it is.
ROBOT_CLIPS = ["idle", "walk", "run", "jump_loop", "swim", "swim_idle", "climb", "climb_idle",
               "chop", "dig", "mine", "sleep"]


def material(name, rgb, glow=False):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.85
    if glow:
        bsdf.inputs["Emission Color"].default_value = (*rgb, 1.0)
        bsdf.inputs["Emission Strength"].default_value = 1.0
    m.diffuse_color = (*rgb, 1.0)
    return m


class Parts:
    """Boxes in the rig's own space (the robot's: front +Y, up +Z, 1.357
    tall), each weighted whole to one bone."""

    def __init__(self, mats):
        self.mats = mats
        self.verts, self.faces, self.fmat, self.groups = [], [], [], {}

    def _add(self, bone, mat, corners):
        b = len(self.verts)
        self.verts += corners
        for f in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
            self.faces.append(tuple(b + i for i in f))
            self.fmat.append(list(self.mats).index(mat))
        self.groups.setdefault(bone, []).extend(range(b, b + 8))

    def box(self, bone, mat, c, s, rot=(0, 0, 0)):
        """Centre c, size s, turned by rot (XYZ, radians) about its centre."""
        R = Euler(rot, "XYZ").to_matrix()
        hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
        corners = []
        for dx, dy, dz in ((-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
                           (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)):
            corners.append(tuple(Vector(c) + R @ Vector((dx * hx, dy * hy, dz * hz))))
        self._add(bone, mat, corners)

    def limb(self, bone, mat, a, b, w, d=None):
        """A box from a to b, w wide and d deep across it."""
        a, b = Vector(a), Vector(b)
        axis = (b - a).normalized()
        side = Vector((1, 0, 0)) if abs(axis.x) < 0.9 else Vector((0, 1, 0))
        u = axis.cross(side).normalized()
        v = axis.cross(u).normalized()
        hw, hd = w / 2, (d or w) / 2
        corners = []
        for end, sx, sy in ((a, -1, -1), (a, 1, -1), (a, 1, 1), (a, -1, 1),
                            (b, -1, -1), (b, 1, -1), (b, 1, 1), (b, -1, 1)):
            corners.append(tuple(end + v * (sx * hw) + u * (sy * hd)))
        self._add(bone, mat, corners)

    def build(self, name, rig):
        me = bpy.data.meshes.new(name)
        me.from_pydata(self.verts, [], self.faces)
        for m in self.mats.values():
            me.materials.append(m)
        for poly, mi in zip(me.polygons, self.fmat):
            poly.material_index = mi
            poly.use_smooth = False
        me.validate()
        obj = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(obj)
        for bone, idx in self.groups.items():
            obj.vertex_groups.new(name=bone).add(idx, 1.0, "REPLACE")
        obj.parent = rig
        obj.modifiers.new("Armature", "ARMATURE").object = rig
        return obj


def new_rig(name, scale):
    src = bpy.data.objects["Robot_Rig"]
    rig = src.copy()
    rig.data = src.data.copy()
    rig.name = name
    rig.scale = (scale, scale, scale)
    rig.animation_data_clear()
    bpy.context.scene.collection.objects.link(rig)
    ad = rig.animation_data_create()
    for c in ROBOT_CLIPS:
        tr = ad.nla_tracks.new()
        tr.name = c
        tr.strips.new(c, 0, bpy.data.actions[c])
    return rig


def pose(rig, keys):
    """keys: {bone: {"r": (x, y, z) radians about the RIG's axes, "l": (x, y,
    z) in the rig's space}}, each relative to the bone's parent as it stands
    at rest. Front is +Y: an arm (pointing down) turned +X goes forward, a
    spine (pointing up) turned -X leans forward."""
    for pb in rig.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    for bone, k in keys.items():
        pb = rig.pose.bones[bone]
        R = pb.bone.matrix_local
        turn = Euler(k.get("r", (0, 0, 0)), "XYZ").to_matrix().to_4x4()
        move = Matrix.Translation(k.get("l", (0, 0, 0)))
        head = Matrix.Translation(pb.bone.head_local)
        pb.matrix_basis = R.inverted() @ move @ head @ turn @ head.inverted() @ R


def clip(rig, name, seconds, frames, loop=True):
    """frames: [(t in 0..1, keys as pose() takes them)]; loop: t = 1 is t = 0."""
    ad = rig.animation_data
    act = bpy.data.actions.new(f"{rig.name}_{name}")
    ad.action = act
    n = max(1, round(seconds * FPS))
    if loop:
        frames = [f for f in frames if f[0] < 1.0] + [(1.0, frames[0][1])]
    for t, keys in frames:
        pose(rig, keys)
        for pb in rig.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=t * n)
            pb.keyframe_insert("location", frame=t * n)
    ad.action = None
    for pb in rig.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    tr = ad.nla_tracks.new()
    tr.name = name
    tr.strips.new(name, 0, act)


# Down on the knees: thighs forward, shins back under them, the hips lower.
KNEEL = {
    "root": {"l": (0, 0, -0.16)},
    "thigh_L": {"r": (1.1, 0, 0)}, "thigh_R": {"r": (1.1, 0, 0)},
    "shin_L": {"r": (-1.7, 0, 0)}, "shin_R": {"r": (-1.7, 0, 0)},
    "foot_L": {"r": (0.6, 0, 0)}, "foot_R": {"r": (0.6, 0, 0)},
}


def work_clips(rig, robot=False):
    def with_(base, **more):
        out = {k: dict(v) for k, v in base.items()}
        out.update(more)
        return out

    clip(rig, "gather", 2.0, [
        (0.0, with_(KNEEL, spine={"r": (-0.5, 0, 0)}, upper_arm_L={"r": (0.9, 0, 0)},
                    upper_arm_R={"r": (1.0, 0, 0)}, head={"r": (-0.3, 0, 0)})),
        (0.3, with_(KNEEL, spine={"r": (-0.6, 0, 0)}, upper_arm_L={"r": (0.8, 0, 0.1)},
                    upper_arm_R={"r": (1.2, 0, -0.1)}, head={"r": (-0.35, 0, 0)})),
        (0.55, with_(KNEEL, spine={"r": (-0.3, 0, 0)}, upper_arm_L={"r": (0.9, 0, 0)},
                     upper_arm_R={"r": (0.5, 0, 0)}, forearm_R={"r": (1.4, 0, 0)},
                     head={"r": (-0.2, 0, 0)})),
        (0.8, with_(KNEEL, spine={"r": (-0.5, 0, 0)}, upper_arm_L={"r": (1.0, 0, 0)},
                    upper_arm_R={"r": (0.9, 0, 0)}, head={"r": (-0.3, 0, 0.1)})),
    ])
    if robot:
        return
    # On its back in its bed, the head the way it faces (the pillow), slowly
    # breathing: the robot's "sleep" is drawn lying by the engine, a worker's
    # lies by its own clip.
    flat = {"root": {"r": (-math.pi / 2, 0, 0), "l": (0, 0, 0.11)},
            "upper_arm_L": {"r": (0, 0.15, 0)}, "upper_arm_R": {"r": (0, -0.15, 0)},
            "head": {"r": (0.15, 0, 0.1)}}
    clip(rig, "lie", 4.0, [
        (0.0, flat),
        (0.5, with_(flat, chest={"r": (-0.05, 0, 0)}, head={"r": (0.2, 0, 0.1)})),
    ])
    # The rod is in the right hand: up over the head behind, whipped out, then
    # held over the water with now and then a tug.
    hold = {"upper_arm_R": {"r": (1.05, 0, 0)}, "forearm_R": {"r": (0.25, 0, 0)},
            "upper_arm_L": {"r": (0.3, 0, 0)}, "spine": {"r": (0.05, 0, 0)}}
    clip(rig, "fish", 4.0, [
        (0.0, hold),
        (0.1, {"upper_arm_R": {"r": (2.7, 0, 0)}, "forearm_R": {"r": (0.3, 0, 0)},
               "spine": {"r": (0.2, 0, 0)}, "upper_arm_L": {"r": (0.2, 0, 0)}}),
        (0.2, {"upper_arm_R": {"r": (0.8, 0, 0)}, "forearm_R": {"r": (0.1, 0, 0)},
               "spine": {"r": (-0.15, 0, 0)}, "upper_arm_L": {"r": (0.4, 0, 0)}}),
        (0.3, hold),
        (0.6, with_(hold, head={"r": (0.1, 0, 0.15)})),
        (0.75, with_(hold, upper_arm_R={"r": (1.35, 0, 0)}, spine={"r": (0.12, 0, 0)})),
        (0.85, hold),
    ])
    # Stirring: the left arm holds the pot before the chest, the right goes
    # round over it.
    pot = {"upper_arm_L": {"r": (0.7, 0, 0.25)}, "forearm_L": {"r": (0.9, 0, 0)},
           "spine": {"r": (-0.1, 0, 0)}, "head": {"r": (-0.25, 0, 0)}}
    clip(rig, "cook", 1.6, [
        (0.0, with_(pot, upper_arm_R={"r": (0.8, 0, -0.25)}, forearm_R={"r": (0.7, 0, 0)})),
        (0.25, with_(pot, upper_arm_R={"r": (1.0, 0, 0.0)}, forearm_R={"r": (0.5, 0, 0)})),
        (0.5, with_(pot, upper_arm_R={"r": (0.8, 0, 0.25)}, forearm_R={"r": (0.7, 0, 0)})),
        (0.75, with_(pot, upper_arm_R={"r": (0.6, 0, 0.0)}, forearm_R={"r": (0.9, 0, 0)})),
    ])
    # The food in the right hand up to the mouth, a bite, chewing, and down.
    clip(rig, "eat", 2.4, [
        (0.0, {"upper_arm_R": {"r": (0.3, 0, 0)}, "forearm_R": {"r": (0.6, 0, 0)}}),
        (0.25, {"upper_arm_R": {"r": (0.5, 0, -0.35)}, "forearm_R": {"r": (2.3, 0, 0)},
                "head": {"r": (-0.1, 0, 0)}}),
        (0.35, {"upper_arm_R": {"r": (0.5, 0, -0.35)}, "forearm_R": {"r": (2.3, 0, 0)},
                "head": {"r": (0.12, 0, 0)}}),
        (0.5, {"upper_arm_R": {"r": (0.35, 0, -0.1)}, "forearm_R": {"r": (1.3, 0, 0)},
               "head": {"r": (-0.05, 0, 0)}}),
        (0.6, {"upper_arm_R": {"r": (0.35, 0, -0.1)}, "forearm_R": {"r": (1.3, 0, 0)},
               "head": {"r": (0.05, 0, 0)}}),
        (0.7, {"upper_arm_R": {"r": (0.35, 0, -0.1)}, "forearm_R": {"r": (1.3, 0, 0)},
               "head": {"r": (-0.05, 0, 0)}}),
    ])


# ---------------------------------------------------------------- the bodies

def legs(p, trousers, shoes, w=0.11, foot=(0.12, 0.2, 0.08)):
    for sx, s in ((-1, "L"), (1, "R")):
        x = sx * 0.09
        p.limb(f"thigh_{s}", trousers, (x, 0, 0.58), (x, 0, 0.29), w)
        p.limb(f"shin_{s}", trousers, (x, 0, 0.31), (x, 0, 0.07), w * 0.9)
        p.box(f"foot_{s}", shoes, (x, 0.04, 0.04), foot)


def arms(p, sleeve, hand, w=0.09, hand_s=(0.08, 0.08, 0.09)):
    for sx, s in ((-1, "L"), (1, "R")):
        x = sx * 0.2
        p.limb(f"upper_arm_{s}", sleeve, (x, 0, 0.95), (x, 0, 0.7), w)
        p.limb(f"forearm_{s}", sleeve, (x, 0, 0.72), (x, 0, 0.5), w * 0.9)
        p.box(f"hand_{s}", hand, (x, 0, 0.455), hand_s)


def human():
    rig = new_rig("Human_Rig", 1.3264554738998413)
    mats = {k: material(f"human_{k}", rgb) for k, rgb in {
        "skin": (0.86, 0.66, 0.5), "shirt": (0.36, 0.55, 0.3), "shirt_dark": (0.27, 0.42, 0.22),
        "trousers": (0.45, 0.33, 0.22), "shoes": (0.25, 0.17, 0.1), "belt": (0.2, 0.13, 0.08),
        "hair": (0.38, 0.24, 0.12), "straw": (0.88, 0.76, 0.42), "band": (0.6, 0.18, 0.15),
        "eye": (0.1, 0.1, 0.12), "white": (0.95, 0.95, 0.92)}.items()}
    p = Parts(mats)
    legs(p, "trousers", "shoes")
    p.box("hips", "trousers", (0, 0, 0.6), (0.3, 0.17, 0.12))
    p.box("hips", "belt", (0, 0, 0.665), (0.31, 0.18, 0.035))
    p.box("spine", "shirt", (0, 0, 0.75), (0.3, 0.17, 0.15))
    p.box("chest", "shirt", (0, 0, 0.89), (0.33, 0.18, 0.17))
    p.box("chest", "shirt_dark", (0, 0.09, 0.86), (0.05, 0.01, 0.2))     # buttons' strip
    arms(p, "shirt", "skin")
    p.box("neck", "skin", (0, 0, 1.0), (0.08, 0.08, 0.05))
    p.box("head", "skin", (0, 0.01, 1.12), (0.2, 0.2, 0.2))
    p.box("head", "hair", (0, -0.03, 1.16), (0.21, 0.16, 0.15))
    p.box("head", "white", (-0.05, 0.111, 1.13), (0.045, 0.004, 0.03))
    p.box("head", "white", (0.05, 0.111, 1.13), (0.045, 0.004, 0.03))
    p.box("head", "eye", (-0.05, 0.114, 1.13), (0.022, 0.004, 0.025))
    p.box("head", "eye", (0.05, 0.114, 1.13), (0.022, 0.004, 0.025))
    p.box("head", "skin", (0, 0.12, 1.1), (0.04, 0.03, 0.05))            # nose
    p.box("head", "straw", (0, 0.01, 1.23), (0.4, 0.4, 0.02))            # the hat's brim
    p.box("head", "straw", (0, 0.01, 1.27), (0.22, 0.22, 0.08))
    p.box("head", "band", (0, 0.01, 1.245), (0.225, 0.225, 0.025))
    return rig, p.build("Human_Mesh", rig)


def toad():
    rig = new_rig("Toad_Rig", 1.3264554738998413)
    mats = {k: material(f"toad_{k}", rgb) for k, rgb in {
        "skin": (0.42, 0.6, 0.25), "dark": (0.3, 0.44, 0.18), "belly": (0.86, 0.82, 0.55),
        "spot": (0.28, 0.38, 0.16), "eye": (0.95, 0.8, 0.2), "pupil": (0.05, 0.05, 0.05),
        "vest": (0.25, 0.32, 0.55), "mouth": (0.35, 0.15, 0.15)}.items()}
    p = Parts(mats)
    # Short thick legs, wide flat feet.
    for sx, s in ((-1, "L"), (1, "R")):
        x = sx * 0.11
        p.limb(f"thigh_{s}", "skin", (x, 0, 0.56), (x, 0.02, 0.29), 0.16)
        p.limb(f"shin_{s}", "dark", (x, 0.02, 0.31), (x, 0, 0.07), 0.12)
        p.box(f"foot_{s}", "dark", (x * 1.15, 0.07, 0.03), (0.17, 0.26, 0.06))
    # A round belly from the hips to the shoulders, the waistcoat over it.
    p.box("hips", "skin", (0, 0, 0.62), (0.44, 0.34, 0.16))
    p.box("spine", "skin", (0, 0, 0.77), (0.48, 0.38, 0.16))
    p.box("chest", "skin", (0, 0, 0.9), (0.42, 0.32, 0.12))
    p.box("spine", "belly", (0, 0.17, 0.72), (0.32, 0.06, 0.26))
    p.box("chest", "vest", (-0.16, 0.0, 0.85), (0.12, 0.36, 0.22))
    p.box("chest", "vest", (0.16, 0.0, 0.85), (0.12, 0.36, 0.22))
    p.box("chest", "vest", (0, -0.17, 0.85), (0.44, 0.06, 0.22))
    for x, y, z in ((-0.12, -0.19, 0.7), (0.1, -0.19, 0.76), (0.0, -0.19, 0.64)):
        p.box("spine", "spot", (x, y, z), (0.06, 0.01, 0.05))
    arms(p, "skin", "dark", w=0.1, hand_s=(0.1, 0.1, 0.08))
    # The head sits low and wide on the shoulders, the eyes on top of it.
    p.box("head", "skin", (0, 0.03, 1.04), (0.4, 0.32, 0.15))
    p.box("head", "belly", (0, 0.06, 0.98), (0.34, 0.26, 0.04))
    p.box("head", "mouth", (0, 0.19, 1.0), (0.3, 0.012, 0.015))
    for sx in (-1, 1):
        p.box("head", "skin", (sx * 0.12, 0.08, 1.13), (0.1, 0.1, 0.07))
        p.box("head", "eye", (sx * 0.12, 0.12, 1.14), (0.08, 0.04, 0.06))
        p.box("head", "pupil", (sx * 0.12, 0.142, 1.14), (0.06, 0.004, 0.02))
    return rig, p.build("Toad_Mesh", rig)


def mole():
    rig = new_rig("Mole_Rig", 1.3264554738998413)
    mats = {k: material(f"mole_{k}", rgb) for k, rgb in {
        "fur": (0.27, 0.24, 0.26), "fur_light": (0.38, 0.34, 0.35), "pink": (0.92, 0.6, 0.62),
        "claw": (0.92, 0.88, 0.78), "eye": (0.05, 0.05, 0.05), "overall": (0.55, 0.38, 0.2),
        "helmet": (0.85, 0.68, 0.15), "lamp": (1.0, 0.95, 0.6), "strap": (0.3, 0.2, 0.1)}.items()}
    mats["lamp"] = material("mole_lamp", (1.0, 0.95, 0.6), glow=True)
    p = Parts(mats)
    legs(p, "fur", "pink", w=0.12, foot=(0.12, 0.18, 0.07))
    p.box("hips", "overall", (0, 0, 0.61), (0.36, 0.26, 0.14))
    p.box("spine", "overall", (0, 0, 0.75), (0.38, 0.28, 0.16))
    p.box("chest", "fur", (0, -0.01, 0.89), (0.36, 0.27, 0.16))
    p.box("chest", "fur_light", (0, 0.12, 0.88), (0.22, 0.04, 0.14))
    p.box("spine", "strap", (-0.11, 0.145, 0.8), (0.04, 0.01, 0.2))
    p.box("spine", "strap", (0.11, 0.145, 0.8), (0.04, 0.01, 0.2))
    # Arms short and strong, with big pink hands and white claws to dig.
    for sx, s in ((-1, "L"), (1, "R")):
        x = sx * 0.21
        p.limb(f"upper_arm_{s}", "fur", (x, 0, 0.94), (x, 0, 0.7), 0.11)
        p.limb(f"forearm_{s}", "fur", (x, 0, 0.72), (x, 0, 0.52), 0.1)
        p.box(f"hand_{s}", "pink", (x, 0.01, 0.46), (0.12, 0.1, 0.09))
        for dx in (-0.035, 0.0, 0.035):
            p.box(f"hand_{s}", "claw", (x + dx, 0.03, 0.4), (0.02, 0.03, 0.05))
    # A head pushed forward, a long pink snout, tiny eyes, a miner's helmet.
    p.box("head", "fur", (0, 0.04, 1.1), (0.24, 0.24, 0.2))
    p.box("head", "fur_light", (0, 0.17, 1.07), (0.12, 0.1, 0.1))
    p.box("head", "pink", (0, 0.25, 1.06), (0.07, 0.08, 0.06))
    p.box("head", "pink", (0, 0.292, 1.06), (0.05, 0.01, 0.045))
    p.box("head", "eye", (-0.06, 0.161, 1.14), (0.025, 0.006, 0.02))
    p.box("head", "eye", (0.06, 0.161, 1.14), (0.025, 0.006, 0.02))
    p.box("head", "helmet", (0, 0.03, 1.22), (0.27, 0.27, 0.06))
    p.box("head", "helmet", (0, 0.03, 1.26), (0.2, 0.2, 0.05))
    p.box("head", "lamp", (0, 0.17, 1.24), (0.07, 0.03, 0.06))
    return rig, p.build("Mole_Mesh", rig)


def body_base(p, skin="skin", eye="eye", white="white"):
    """The human's head and neck, its face looking out; what goes on top is
    each one's own."""
    p.box("neck", skin, (0, 0, 1.0), (0.08, 0.08, 0.05))
    p.box("head", skin, (0, 0.01, 1.12), (0.2, 0.2, 0.2))
    p.box("head", white, (-0.05, 0.111, 1.13), (0.045, 0.004, 0.03))
    p.box("head", white, (0.05, 0.111, 1.13), (0.045, 0.004, 0.03))
    p.box("head", eye, (-0.05, 0.114, 1.13), (0.022, 0.004, 0.025))
    p.box("head", eye, (0.05, 0.114, 1.13), (0.022, 0.004, 0.025))
    p.box("head", skin, (0, 0.12, 1.1), (0.04, 0.03, 0.05))            # nose


def guard():
    rig = new_rig("Guard_Rig", 1.3264554738998413)
    mats = {k: material(f"guard_{k}", rgb) for k, rgb in {
        "skin": (0.8, 0.6, 0.46), "iron": (0.62, 0.64, 0.68), "iron_dark": (0.4, 0.42, 0.46),
        "mail": (0.5, 0.52, 0.55), "tabard": (0.62, 0.12, 0.1), "gold": (0.85, 0.68, 0.2),
        "boots": (0.22, 0.15, 0.1), "belt": (0.2, 0.13, 0.08), "eye": (0.1, 0.1, 0.12),
        "white": (0.95, 0.95, 0.92)}.items()}
    p = Parts(mats)
    legs(p, "mail", "boots")
    for sx in (-1, 1):                                                   # greaves
        p.box(f"shin_{'L' if sx < 0 else 'R'}", "iron", (sx * 0.09, 0.02, 0.2), (0.12, 0.1, 0.2))
    p.box("hips", "mail", (0, 0, 0.6), (0.31, 0.18, 0.12))
    p.box("hips", "belt", (0, 0, 0.665), (0.32, 0.19, 0.035))
    p.box("spine", "mail", (0, 0, 0.75), (0.31, 0.18, 0.15))
    p.box("chest", "iron", (0, 0, 0.89), (0.35, 0.2, 0.18))               # the breastplate
    p.box("spine", "tabard", (0, 0.095, 0.72), (0.2, 0.01, 0.3))          # tabard, front
    p.box("hips", "tabard", (0, 0.1, 0.52), (0.18, 0.01, 0.16))
    p.box("chest", "gold", (0, 0.105, 0.86), (0.06, 0.004, 0.08))         # the village's badge
    for sx, s in ((-1, "L"), (1, "R")):                                  # pauldrons
        p.box(f"upper_arm_{s}", "iron", (sx * 0.21, 0, 0.94), (0.14, 0.15, 0.08))
    arms(p, "mail", "iron_dark")
    body_base(p)
    p.box("head", "iron", (0, 0.0, 1.19), (0.23, 0.23, 0.12))            # the helmet
    p.box("head", "iron", (0, -0.02, 1.12), (0.23, 0.19, 0.12))
    p.box("head", "iron_dark", (0, 0.0, 1.135), (0.235, 0.235, 0.02))    # its rim
    p.box("head", "iron", (0, 0.122, 1.12), (0.025, 0.012, 0.09))        # nose guard
    p.box("head", "tabard", (0, -0.02, 1.27), (0.03, 0.18, 0.05))        # crest
    return rig, p.build("Guard_Mesh", rig)


def archer():
    rig = new_rig("Archer_Rig", 1.3264554738998413)
    mats = {k: material(f"archer_{k}", rgb) for k, rgb in {
        "skin": (0.84, 0.64, 0.48), "hood": (0.22, 0.4, 0.2), "hood_dark": (0.16, 0.3, 0.15),
        "leather": (0.5, 0.33, 0.18), "trousers": (0.35, 0.3, 0.22), "boots": (0.25, 0.17, 0.1),
        "quiver": (0.42, 0.25, 0.12), "fletch": (0.9, 0.9, 0.85), "eye": (0.1, 0.1, 0.12),
        "white": (0.95, 0.95, 0.92)}.items()}
    p = Parts(mats)
    legs(p, "trousers", "boots")
    p.box("hips", "leather", (0, 0, 0.6), (0.3, 0.17, 0.12))
    p.box("spine", "leather", (0, 0, 0.75), (0.3, 0.17, 0.15))
    p.box("chest", "leather", (0, 0, 0.89), (0.33, 0.18, 0.17))
    p.box("chest", "hood_dark", (0, 0, 0.97), (0.36, 0.21, 0.06))        # the hood's cape
    p.box("chest", "quiver", (0.07, -0.12, 0.88), (0.08, 0.06, 0.3), rot=(0, 0.35, 0))
    for dx in (-0.015, 0.015):
        p.box("chest", "fletch", (0.13 + dx, -0.12, 1.05), (0.012, 0.03, 0.05), rot=(0, 0.35, 0))
    p.box("chest", "quiver", (0, 0.095, 0.88), (0.04, 0.01, 0.24), rot=(0, -0.7, 0))   # its strap
    arms(p, "leather", "skin")
    body_base(p)
    p.box("head", "hood", (0, -0.025, 1.15), (0.24, 0.2, 0.24))          # the hood, the face open
    p.box("head", "hood", (0, 0.07, 1.245), (0.24, 0.06, 0.05))
    p.box("head", "hood_dark", (0, -0.12, 1.2), (0.08, 0.08, 0.12), rot=(-0.4, 0, 0))   # its point
    return rig, p.build("Archer_Mesh", rig)


def inspector():
    rig = new_rig("Inspector_Rig", 1.3264554738998413)
    mats = {k: material(f"inspector_{k}", rgb) for k, rgb in {
        "skin": (0.88, 0.7, 0.56), "coat": (0.16, 0.16, 0.24), "coat_dark": (0.1, 0.1, 0.16),
        "trousers": (0.2, 0.2, 0.22), "shoes": (0.08, 0.08, 0.08), "brass": (0.85, 0.68, 0.25),
        "hat": (0.08, 0.08, 0.1), "book": (0.45, 0.12, 0.1), "page": (0.92, 0.88, 0.75),
        "beard": (0.55, 0.55, 0.55), "eye": (0.1, 0.1, 0.12), "white": (0.95, 0.95, 0.92)}.items()}
    p = Parts(mats)
    legs(p, "trousers", "shoes")
    p.box("hips", "coat", (0, 0, 0.58), (0.32, 0.19, 0.16))
    p.box("thigh_L", "coat", (-0.08, 0, 0.45), (0.15, 0.19, 0.22))       # the coat's tails
    p.box("thigh_R", "coat", (0.08, 0, 0.45), (0.15, 0.19, 0.22))
    p.box("spine", "coat", (0, 0, 0.75), (0.31, 0.18, 0.15))
    p.box("chest", "coat", (0, 0, 0.89), (0.34, 0.19, 0.17))
    p.box("chest", "coat_dark", (0, 0.096, 0.92), (0.14, 0.01, 0.12))    # lapels
    for z in (0.7, 0.78, 0.86):
        p.box("spine" if z < 0.8 else "chest", "brass", (0, 0.097, z), (0.025, 0.006, 0.025))
    arms(p, "coat", "skin")
    p.box("hand_L", "book", (-0.2, 0.05, 0.47), (0.04, 0.14, 0.18))      # the ledger
    p.box("hand_L", "page", (-0.18, 0.05, 0.47), (0.006, 0.13, 0.17))
    body_base(p)
    p.box("head", "beard", (0, 0.105, 1.06), (0.16, 0.03, 0.07))
    p.box("head", "hat", (0, 0.01, 1.225), (0.32, 0.32, 0.02))           # brim
    p.box("head", "hat", (0, 0.01, 1.32), (0.2, 0.2, 0.18))
    p.box("head", "brass", (0, 0.01, 1.25), (0.205, 0.205, 0.025))
    return rig, p.build("Inspector_Mesh", rig)


def worker(name, colours, extra):
    """A village worker on the human's frame: shirt, trousers, boots and a
    face, and what `extra` puts on it -- its trade's clothes."""
    rig = new_rig(f"{name}_Rig", 1.3264554738998413)
    base = {"skin": (0.84, 0.64, 0.48), "eye": (0.1, 0.1, 0.12), "white": (0.95, 0.95, 0.92),
            "belt": (0.2, 0.13, 0.08)}
    base.update(colours)
    mats = {k: material(f"{name.lower()}_{k}", rgb) for k, rgb in base.items()}
    p = Parts(mats)
    legs(p, "trousers", "boots")
    p.box("hips", "trousers", (0, 0, 0.6), (0.3, 0.17, 0.12))
    p.box("hips", "belt", (0, 0, 0.665), (0.31, 0.18, 0.035))
    p.box("spine", "shirt", (0, 0, 0.75), (0.3, 0.17, 0.15))
    p.box("chest", "shirt", (0, 0, 0.89), (0.33, 0.18, 0.17))
    arms(p, "shirt", "skin")
    body_base(p)
    extra(p)
    return rig, p.build(f"{name}_Mesh", rig)


def mayor():
    def extra(p):
        p.box("chest", "sash", (0, 0.095, 0.84), (0.06, 0.01, 0.3), rot=(0, 0.6, 0))   # the sash
        p.box("spine", "coat", (0, 0.0, 0.66), (0.33, 0.19, 0.12))                     # the coat's skirt
        p.box("chest", "gold", (0.08, 0.1, 0.92), (0.05, 0.005, 0.05))                # a medal
        p.box("head", "beard", (0, 0.105, 1.06), (0.16, 0.03, 0.07))
        p.box("head", "hat", (0, 0.01, 1.225), (0.3, 0.3, 0.02))                       # a round hat
        p.box("head", "hat", (0, 0.01, 1.29), (0.21, 0.21, 0.12))
        p.box("head", "sash", (0, 0.01, 1.245), (0.215, 0.215, 0.025))
    return worker("Mayor", {"shirt": (0.18, 0.22, 0.42), "coat": (0.14, 0.17, 0.33),
                            "trousers": (0.2, 0.2, 0.24), "boots": (0.08, 0.08, 0.08),
                            "sash": (0.75, 0.15, 0.15), "gold": (0.9, 0.72, 0.2),
                            "beard": (0.75, 0.75, 0.75), "hat": (0.1, 0.1, 0.12)}, extra)


def builder():
    def extra(p):
        p.box("spine", "apron", (0, 0.095, 0.7), (0.26, 0.01, 0.3))                    # the apron
        p.box("hips", "apron", (0, 0.1, 0.52), (0.24, 0.01, 0.16))
        p.box("hips", "pouch", (0.12, 0.1, 0.63), (0.08, 0.04, 0.07))                 # its nail pouch
        p.box("head", "hair", (0, -0.03, 1.16), (0.21, 0.16, 0.15))
        p.box("head", "cap", (0, 0.0, 1.235), (0.23, 0.23, 0.06))                      # a cap, its peak
        p.box("head", "cap", (0, 0.15, 1.215), (0.2, 0.09, 0.02))
    return worker("Builder", {"shirt": (0.75, 0.55, 0.3), "trousers": (0.3, 0.32, 0.45),
                              "boots": (0.3, 0.2, 0.12), "apron": (0.55, 0.4, 0.25),
                              "pouch": (0.35, 0.22, 0.12), "hair": (0.2, 0.14, 0.1),
                              "cap": (0.85, 0.7, 0.2)}, extra)


def woodcutter():
    def extra(p):
        for z in (0.72, 0.8, 0.88, 0.96):                                             # a checked shirt
            bone = "spine" if z < 0.82 else "chest"
            p.box(bone, "check", (0, 0.0, z), (0.335, 0.185, 0.03))
        p.box("head", "beard", (0, 0.1, 1.05), (0.2, 0.04, 0.1))
        p.box("head", "hat", (0, -0.01, 1.22), (0.22, 0.22, 0.08))                    # a knitted cap
        p.box("head", "hat", (0, -0.01, 1.28), (0.08, 0.08, 0.05))
    return worker("Woodcutter", {"shirt": (0.65, 0.12, 0.1), "check": (0.15, 0.08, 0.06),
                                 "trousers": (0.25, 0.3, 0.45), "boots": (0.3, 0.2, 0.12),
                                 "beard": (0.45, 0.25, 0.12), "hat": (0.2, 0.35, 0.25)}, extra)


def miner():
    def extra(p):
        p.box("chest", "strap", (-0.09, 0.095, 0.86), (0.04, 0.01, 0.22))             # dungarees' straps
        p.box("chest", "strap", (0.09, 0.095, 0.86), (0.04, 0.01, 0.22))
        p.box("head", "helmet", (0, 0.01, 1.2), (0.24, 0.24, 0.1))                    # a helmet, a lamp
        p.box("head", "helmet", (0, 0.01, 1.25), (0.2, 0.2, 0.05))
        p.box("head", "lamp", (0, 0.13, 1.21), (0.07, 0.03, 0.06))
        p.box("head", "soot", (0, 0.112, 1.07), (0.12, 0.004, 0.03))
    return worker("Miner", {"shirt": (0.4, 0.4, 0.38), "trousers": (0.22, 0.28, 0.4),
                            "boots": (0.12, 0.1, 0.08), "strap": (0.22, 0.28, 0.4),
                            "helmet": (0.85, 0.72, 0.15), "lamp": (1.0, 0.95, 0.6),
                            "soot": (0.3, 0.25, 0.22)}, extra)


def digger():
    def extra(p):
        p.box("spine", "vest", (0, 0.0, 0.76), (0.32, 0.19, 0.16))                    # a vest
        p.box("chest", "vest", (0, 0.0, 0.9), (0.345, 0.19, 0.15))
        p.box("chest", "stripe", (0, 0.0, 0.88), (0.35, 0.195, 0.03))
        for sx, s in ((-1, "L"), (1, "R")):                                           # mud to the knee
            p.box(f"shin_{s}", "mud", (sx * 0.09, 0.0, 0.14), (0.11, 0.11, 0.14))
        p.box("head", "hat", (0, 0.01, 1.22), (0.36, 0.36, 0.02))                      # a wide brim
        p.box("head", "hat", (0, 0.01, 1.26), (0.21, 0.21, 0.07))
    return worker("Digger", {"shirt": (0.6, 0.55, 0.45), "vest": (0.85, 0.45, 0.12),
                             "stripe": (0.9, 0.9, 0.6), "trousers": (0.4, 0.33, 0.25),
                             "boots": (0.2, 0.14, 0.1), "mud": (0.35, 0.25, 0.15),
                             "hat": (0.5, 0.4, 0.25)}, extra)


def merchant():
    def extra(p):
        p.box("spine", "robe", (0, 0, 0.7), (0.34, 0.2, 0.22))                         # a long robe
        p.box("hips", "robe", (0, 0, 0.5), (0.36, 0.22, 0.24))
        p.box("hips", "trim", (0, 0.0, 0.38), (0.37, 0.23, 0.03))
        p.box("chest", "trim", (0, 0.097, 0.88), (0.04, 0.006, 0.2))
        p.box("hips", "purse", (-0.15, 0.1, 0.6), (0.08, 0.05, 0.09))                # a purse at the belt
        p.box("head", "beard", (0, 0.105, 1.05), (0.12, 0.03, 0.08))
        p.box("head", "turban", (0, 0.0, 1.22), (0.24, 0.24, 0.1))                     # a turban, a jewel
        p.box("head", "turban", (0, 0.0, 1.28), (0.19, 0.19, 0.06))
        p.box("head", "jewel", (0, 0.122, 1.22), (0.035, 0.008, 0.035))
    return worker("Merchant", {"shirt": (0.45, 0.18, 0.45), "robe": (0.38, 0.14, 0.4),
                               "trim": (0.9, 0.72, 0.2), "trousers": (0.3, 0.12, 0.3),
                               "boots": (0.25, 0.15, 0.1), "purse": (0.5, 0.33, 0.15),
                               "beard": (0.2, 0.15, 0.12), "turban": (0.92, 0.88, 0.78),
                               "jewel": (0.2, 0.75, 0.4)}, extra)


def crafter():
    def extra(p):
        p.box("spine", "smock", (0, 0.0, 0.72), (0.32, 0.19, 0.2))                    # a leather smock
        p.box("chest", "smock", (0, 0.0, 0.88), (0.335, 0.19, 0.12))
        p.box("hips", "smock", (0, 0.1, 0.52), (0.26, 0.01, 0.16))
        p.box("hips", "hammer", (0.16, 0.1, 0.6), (0.03, 0.03, 0.14))                # a hammer at the belt
        p.box("hips", "iron", (0.16, 0.1, 0.68), (0.08, 0.04, 0.035))
        p.box("head", "beard", (0, 0.1, 1.05), (0.18, 0.04, 0.09))
        p.box("head", "goggles", (0, 0.0, 1.2), (0.215, 0.215, 0.035))               # goggles pushed up
        p.box("head", "lens", (-0.05, 0.11, 1.2), (0.05, 0.01, 0.045))
        p.box("head", "lens", (0.05, 0.11, 1.2), (0.05, 0.01, 0.045))
    return worker("Crafter", {"shirt": (0.55, 0.5, 0.42), "smock": (0.42, 0.26, 0.14),
                              "trousers": (0.28, 0.24, 0.2), "boots": (0.15, 0.1, 0.07),
                              "hammer": (0.4, 0.28, 0.15), "iron": (0.45, 0.45, 0.48),
                              "beard": (0.55, 0.3, 0.15), "goggles": (0.3, 0.2, 0.1),
                              "lens": (0.55, 0.75, 0.8)}, extra)


def fight_clips(rig, kind):
    """A guard's blow and an archer's draw: what the client plays while a
    creature is swinging ("attack") or drawing ("shoot")."""
    if kind == "guard":
        up = {"upper_arm_R": {"r": (2.6, 0, -0.2)}, "forearm_R": {"r": (0.5, 0, 0)},
              "spine": {"r": (0.15, 0, -0.1)}, "upper_arm_L": {"r": (0.5, 0, 0.2)}}
        clip(rig, "attack", 0.8, [
            (0.0, up),
            (0.35, {"upper_arm_R": {"r": (0.9, 0, 0.15)}, "forearm_R": {"r": (0.2, 0, 0)},
                    "spine": {"r": (-0.25, 0, 0.15)}, "upper_arm_L": {"r": (0.3, 0, 0.2)}}),
            (0.6, {"upper_arm_R": {"r": (0.7, 0, 0.1)}, "forearm_R": {"r": (0.3, 0, 0)},
                   "spine": {"r": (-0.15, 0, 0.1)}}),
        ])
    elif kind == "archer":
        # The bow out in the right hand, the left pulling the string to the chin.
        held = {"upper_arm_R": {"r": (1.55, 0, 0.1)}, "forearm_R": {"r": (0.0, 0, 0)},
                "spine": {"r": (0, 0, 0.25)}, "head": {"r": (0, 0, -0.2)}}
        drawn = dict(held, upper_arm_L={"r": (1.45, 0, -0.6)}, forearm_L={"r": (1.9, 0, 0)})
        clip(rig, "shoot", 1.0, [
            (0.0, dict(held, upper_arm_L={"r": (1.3, 0, -0.1)}, forearm_L={"r": (0.4, 0, 0)})),
            (0.55, drawn),
            (0.8, drawn),
            (0.85, dict(held, upper_arm_L={"r": (1.2, 0, -0.9)}, forearm_L={"r": (0.6, 0, 0)})),
        ])


def helper():
    rig = new_rig("Helper_Rig", 1.3264554738998413 * 0.62)
    mats = {k: material(f"helper_{k}", rgb) for k, rgb in {
        "body": (0.95, 0.55, 0.12), "dark": (0.22, 0.22, 0.25), "metal": (0.65, 0.66, 0.7),
        "screen": (0.08, 0.1, 0.12)}.items()}
    mats["glow"] = material("helper_glow", (0.3, 0.95, 0.85), glow=True)
    mats["light"] = material("helper_light", (1.0, 0.3, 0.2), glow=True)
    p = Parts(mats)
    for sx, s in ((-1, "L"), (1, "R")):
        x = sx * 0.1
        p.limb(f"thigh_{s}", "metal", (x, 0, 0.57), (x, 0, 0.29), 0.08)
        p.limb(f"shin_{s}", "dark", (x, 0, 0.31), (x, 0, 0.08), 0.11)
        p.box(f"foot_{s}", "dark", (x, 0.03, 0.04), (0.14, 0.22, 0.08))
    # A boxy body with a hatch and a dial, on a short neck a head that is a screen.
    p.box("hips", "dark", (0, 0, 0.6), (0.3, 0.22, 0.1))
    p.box("spine", "body", (0, 0, 0.78), (0.42, 0.32, 0.3))
    p.box("chest", "body", (0, 0, 0.92), (0.44, 0.33, 0.1))
    p.box("spine", "dark", (0, 0.165, 0.76), (0.24, 0.01, 0.16))
    p.box("spine", "glow", (-0.06, 0.172, 0.79), (0.05, 0.006, 0.03))
    p.box("spine", "light", (0.06, 0.172, 0.79), (0.05, 0.006, 0.03))
    p.box("spine", "metal", (0, 0.172, 0.72), (0.16, 0.006, 0.02))
    for sx, s in ((-1, "L"), (1, "R")):
        x = sx * 0.25
        p.box(f"upper_arm_{s}", "dark", (x, 0, 0.93), (0.1, 0.1, 0.1))
        p.limb(f"upper_arm_{s}", "metal", (x, 0, 0.92), (x, 0, 0.71), 0.06)
        p.limb(f"forearm_{s}", "metal", (x, 0, 0.72), (x, 0, 0.5), 0.06)
        p.box(f"hand_{s}", "dark", (x, 0, 0.46), (0.09, 0.07, 0.07))
    p.box("neck", "metal", (0, 0, 1.0), (0.08, 0.08, 0.06))
    p.box("head", "body", (0, 0, 1.12), (0.34, 0.24, 0.2))
    p.box("head", "screen", (0, 0.121, 1.12), (0.28, 0.004, 0.15))
    p.box("head", "glow", (-0.07, 0.125, 1.14), (0.05, 0.004, 0.05))
    p.box("head", "glow", (0.07, 0.125, 1.14), (0.05, 0.004, 0.05))
    p.box("head", "glow", (0, 0.125, 1.08), (0.1, 0.004, 0.015))
    p.limb("antenna", "metal", (0.07, 0, 1.22), (0.07, 0, 1.33), 0.02)
    p.box("antenna", "light", (0.07, 0, 1.35), (0.04, 0.04, 0.04))
    return rig, p.build("Helper_Mesh", rig)


def with_tools(rig, mesh, robot_mesh):
    """The robot's axe, pick, shovel and sword -- the parts of its mesh on the
    tool_* bones, shown only while that tool is in the hand (tools_shown in
    src/client/main.cpp) -- joined into this mesh."""
    tools = robot_mesh.copy()
    tools.data = robot_mesh.data.copy()
    bpy.context.scene.collection.objects.link(tools)
    groups = {g.index for g in tools.vertex_groups if g.name.startswith("tool_")}
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(tools.data)
    deform = bm.verts.layers.deform.active
    gone = [v for v in bm.verts if not any(i in groups and w > 0.5 for i, w in v[deform].items())]
    bmesh.ops.delete(bm, geom=gone, context="VERTS")
    bm.to_mesh(tools.data)
    bm.free()
    tools.parent = rig
    tools.matrix_parent_inverse = Matrix.Identity(4)
    tools.location = (0, 0, 0)
    tools.modifiers.clear()
    bpy.ops.object.select_all(action="DESELECT")
    tools.select_set(True)
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = mesh
    bpy.ops.object.join()
    return mesh


def export(rig, mesh, path):
    ad = rig.animation_data
    ad.action = None
    bpy.ops.object.select_all(action="DESELECT")
    saved = rig.location.copy()
    rig.location = (0, 0, 0)
    bpy.context.view_layer.update()
    rig.select_set(True)
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS",
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_yup=True, export_apply=False, export_materials="EXPORT",
                              export_image_format="NONE", export_skins=True,
                              export_def_bones=False)
    rig.location = saved


if __name__ == "__main__":
    bpy.context.scene.render.fps = FPS
    robot_mesh = bpy.data.objects["Robot_Mesh"]
    robot_mesh.parent = None
    robot_mesh.matrix_world = Matrix.Identity(4)
    for o in list(bpy.data.objects):
        if o.name not in ("Robot_Rig", "Robot_Mesh"):
            bpy.data.objects.remove(o)
    villagers = os.path.join(ROOT, "mods", "villagers")
    helpers = os.path.join(ROOT, "mods", "helpers")
    os.makedirs(villagers, exist_ok=True)
    os.makedirs(helpers, exist_ok=True)
    for i, (make, out, name, robot) in enumerate([(human, villagers, "human", False),
                                                  (toad, villagers, "toad", False),
                                                  (mole, villagers, "mole", False),
                                                  (guard, villagers, "guard", False),
                                                  (archer, villagers, "archer", False),
                                                  (inspector, villagers, "inspector", False),
                                                  (mayor, villagers, "mayor", False),
                                                  (builder, villagers, "builder", False),
                                                  (woodcutter, villagers, "woodcutter", False),
                                                  (miner, villagers, "miner", False),
                                                  (digger, villagers, "digger", False),
                                                  (merchant, villagers, "merchant", False),
                                                  (crafter, villagers, "crafter", False),
                                                  (helper, helpers, "helper", True)]):
        rig, mesh = make()
        mesh = with_tools(rig, mesh, robot_mesh)
        work_clips(rig, robot)
        fight_clips(rig, name)
        pets.tip_over(rig, [mesh])
        export(rig, mesh, os.path.join(out, f"{name}.gltf"))
        rig.location.x = 1.0 + i
    bpy.data.objects.remove(bpy.data.objects["Robot_Rig"])
    bpy.data.objects.remove(robot_mesh)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("villagers written to", os.path.normpath(villagers), "and", os.path.normpath(helpers))
