# The yafps arena's things, built in Blender, rigged, animated and exported
# into games/yafps/mods/yafps_game.
#
#   flatpak run org.blender.Blender --background --python tools/make_fps_models.py
#   (or blender --background --python ..., or inside Blender)
#
# Starts from an empty file and leaves the work in ~/kart_assets/fps.blend.
#
#   robot_arms   (into yafps_core, beside the robot) the arms seen in first person, holding the four guns: a bone
#                tool_<gun> each, shown only while that gun is in the hand
#                (tools_shown in src/client/main.cpp). Authored in the eye's
#                space, the eye at the origin looking down +Y (-Z once Y is up).
#                Clips: idle, raise, fire_pistol, fire_shotgun, fire_chaingun,
#                fire_plasma, played by the world's code with core.arms_clip.
#   imp, pinky, gazer, ghoul
#                the demons: front +Y, feet on z = 0 like the creatures of
#                mods/mobs, every bone pointing up with no roll. Clips idle,
#                walk, run and attack (melee) or shoot (archer).
#   fireball, caco_ball, bullet, plasma_ball
#                what flies: glowing, drawn at full light.
#   ammo_bullets, ammo_shells, ammo_cells, medkit, health_orb
#                what lies on the floor to be picked up.
#   pistol_held, shotgun_held, chaingun_held, plasma_held
#                each gun on its own, about the grip, barrel +Y: in the robot's
#                hand seen by others, and lying on the floor as a pickup.
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "games", "yafps", "mods", "yafps_game")
ARMS_OUT = os.path.join(ROOT, "games", "yafps", "mods", "yafps_core")   # beside the robot they belong to
BLEND = os.path.expanduser("~/kart_assets/fps.blend")
FPS = 30   # the chaingun's shot, a twelfth of a second, is three frames; and a model is at most 1 MB


# ---------------------------------------------------------------- materials

def material(name, rgb, glow=False, metal=0.0, rough=0.7, alpha=1.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    if alpha < 1.0:   # see-through: the glTF's baseColorFactor alpha, drawn blended
        bsdf.inputs["Alpha"].default_value = alpha
        m.surface_render_method = "BLENDED"
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    if glow:
        bsdf.inputs["Emission Color"].default_value = (*rgb, 1.0)
        bsdf.inputs["Emission Strength"].default_value = 1.0
    m.diffuse_color = (*rgb, 1.0)
    return m


M = {}


def mats():
    for name, rgb, kw in [
        ("gunmetal", (0.16, 0.17, 0.19), {"metal": 0.6}),
        ("steel", (0.42, 0.44, 0.47), {"metal": 0.8}),
        ("polymer", (0.08, 0.08, 0.09), {}),
        ("olive", (0.26, 0.32, 0.18), {}),
        ("armor", (0.20, 0.36, 0.16), {}),
        ("glove", (0.12, 0.11, 0.10), {}),
        ("wood", (0.38, 0.22, 0.10), {}),
        ("cyan", (0.30, 0.90, 1.00), {"glow": True}),
        ("orange", (1.00, 0.55, 0.10), {"glow": True}),
        ("flash", (1.00, 0.85, 0.35), {"glow": True}),
        ("green_glow", (0.35, 1.00, 0.30), {"glow": True}),
        ("green_core", (0.75, 1.00, 0.65), {"glow": True}),
        ("imp_skin", (0.42, 0.26, 0.16), {}),
        ("imp_dark", (0.22, 0.13, 0.08), {}),
        ("bone", (0.85, 0.80, 0.66), {}),
        ("eye_orange", (1.00, 0.50, 0.05), {"glow": True}),
        ("pinky_skin", (0.80, 0.42, 0.45), {}),
        ("pinky_dark", (0.52, 0.22, 0.25), {}),
        ("mouth", (0.30, 0.03, 0.04), {}),
        ("eye_yellow", (1.00, 0.90, 0.20), {"glow": True}),
        ("caco_skin", (0.75, 0.10, 0.08), {}),
        ("caco_dark", (0.45, 0.05, 0.05), {}),
        ("eye_green", (0.40, 1.00, 0.40), {"glow": True}),
        ("eye_white", (0.95, 0.95, 0.90), {}),
        ("zombie_skin", (0.55, 0.52, 0.45), {}),
        ("uniform", (0.30, 0.32, 0.26), {}),
        ("eye_red", (1.00, 0.15, 0.10), {"glow": True}),
        ("fire_core", (1.00, 0.85, 0.30), {"glow": True}),
        ("fire_outer", (1.00, 0.35, 0.05), {"glow": True}),
        ("caco_fire", (1.00, 0.25, 0.60), {"glow": True}),
        ("caco_fire_core", (1.00, 0.70, 0.90), {"glow": True}),
        ("bullet", (1.00, 0.90, 0.40), {"glow": True}),
        ("brass", (0.70, 0.55, 0.20), {"metal": 0.8}),
        ("shell_red", (0.75, 0.10, 0.08), {}),
        ("ammo_box", (0.35, 0.33, 0.20), {}),
        ("white", (0.92, 0.92, 0.90), {}),
        ("red", (0.85, 0.08, 0.06), {}),
        ("blue_glow", (0.30, 0.55, 1.00), {"glow": True}),
    ]:
        M[name] = material(name, rgb, **kw)


# ---------------------------------------------------------------- geometry

class Parts:
    """Primitives gathered into one mesh, each weighted whole to one bone
    (or to none, for a rigid model), each with its material. `pre` places
    everything: the guns in the robot's hand are built in their own frame and
    moved, turned and scaled into its fist by it."""

    def __init__(self, pre=None):
        self.pre = pre if pre is not None else Matrix.Identity(4)
        self.bm = bmesh.new()
        self.dl = self.bm.verts.layers.deform.verify()
        self.groups = []
        self.mats = []

    def _add(self, made, bone, mat, smooth):
        verts = made["verts"]
        vs = set(verts)
        if bone is not None:
            if bone not in self.groups:
                self.groups.append(bone)
            gi = self.groups.index(bone)
            for v in verts:
                v[self.dl][gi] = 1.0
        if mat not in self.mats:
            self.mats.append(mat)
        mi = self.mats.index(mat)
        faces = {f for v in verts for f in v.link_faces if all(x in vs for x in f.verts)}
        for f in faces:
            f.material_index = mi
            f.smooth = smooth

    @staticmethod
    def _m(at, size=(1, 1, 1), rot=(0, 0, 0)):
        return (Matrix.Translation(Vector(at)) @ Matrix.Rotation(rot[2], 4, "Z") @
                Matrix.Rotation(rot[1], 4, "Y") @ Matrix.Rotation(rot[0], 4, "X") @
                Matrix.Diagonal((*size, 1)))

    def box(self, bone, mat, at, size, rot=(0, 0, 0)):
        made = bmesh.ops.create_cube(self.bm, size=1.0, matrix=self.pre @ self._m(at, size, rot))
        self._add(made, bone, mat, False)

    def cyl(self, bone, mat, at, r, depth, axis="Z", r2=None, seg=12, rot=None):
        """Along an axis ("X", "Y" or "Z"), or turned by `rot`. r2: a cone."""
        turn = {"X": (0, math.pi / 2, 0), "Y": (-math.pi / 2, 0, 0), "Z": (0, 0, 0)}[axis]
        made = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=seg,
                                     radius1=r, radius2=r if r2 is None else r2, depth=depth,
                                     matrix=self.pre @ self._m(at, (1, 1, 1), rot or turn))
        self._add(made, bone, mat, True)

    def ball(self, bone, mat, at, r, squash=(1, 1, 1), sub=2):
        made = bmesh.ops.create_icosphere(self.bm, subdivisions=sub, radius=r,
                                          matrix=self.pre @ self._m(at, squash))
        self._add(made, bone, mat, True)

    def limb(self, bone, mat, a, b, w, h=None):
        """A box from a to b, w wide and h deep."""
        a, b = Vector(a), Vector(b)
        d = b - a
        q = d.to_track_quat("Y", "Z")
        m = (Matrix.Translation((a + b) / 2) @ q.to_matrix().to_4x4() @
             Matrix.Diagonal((w, d.length, h or w, 1)))
        made = bmesh.ops.create_cube(self.bm, size=1.0, matrix=self.pre @ m)
        self._add(made, bone, mat, False)

    def spike(self, bone, mat, base, tip, r):
        base, tip = Vector(base), Vector(tip)
        d = tip - base
        q = d.to_track_quat("Z", "Y")
        m = Matrix.Translation((base + tip) / 2) @ q.to_matrix().to_4x4()
        made = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=6,
                                     radius1=r, radius2=0.0, depth=d.length, matrix=self.pre @ m)
        self._add(made, bone, mat, True)

    def build(self, name):
        me = bpy.data.meshes.new(name)
        self.bm.to_mesh(me)
        self.bm.free()
        for m in self.mats:
            me.materials.append(M[m])
        obj = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(obj)
        for g in self.groups:
            obj.vertex_groups.new(name=g)
        return obj


# ---------------------------------------------------------------- rigs and clips

def rig(name, bones):
    """bones: [(name, parent, head)], each pointing up 0.1 with no roll."""
    arm = bpy.data.armatures.new(name)
    obj = bpy.data.objects.new(name, arm)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    for bname, parent, (x, y, z) in bones:
        eb = arm.edit_bones.new(bname)
        eb.head = (x, y, z)
        eb.tail = (x, y, z + 0.1)
        eb.roll = 0.0
        if parent:
            eb.parent = arm.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in obj.pose.bones:
        pb.rotation_mode = "XYZ"
    return obj


def skin(mesh, arm):
    mesh.parent = arm
    mod = mesh.modifiers.new("Armature", "ARMATURE")
    mod.object = arm


# A bone's own axes, every bone pointing up with no roll: X is the world's X,
# Y the world's up (Z) and Z the world's back (-Y). So a rotation about X
# lifts what points forward, a location (0, d, 0) raises by d and (0, 0, d)
# moves d towards the back -- towards the eye, for the arms.

def clip(arm, name, seconds, keys, rest=None, loop=False):
    """keys: {bone: [(t in 0..1, {"r": (rx, ry, rz), "l": (x, y, z), "s": k})]}.
    A channel a key leaves out is the bone's rest there (`rest`, else none).
    Every bone is keyed at both ends; loop sets t = 1 to what t = 0 is."""
    rest = rest or {}
    act = bpy.data.actions.new(f"{arm.name}_{name}")
    arm.animation_data_create()
    arm.animation_data.action = act
    frames = max(1, round(seconds * FPS))
    for pb in arm.pose.bones:
        base = rest.get(pb.name, {})
        ks = list(keys.get(pb.name, []))
        if not ks or ks[0][0] > 0.0:
            ks.insert(0, (0.0, {}))
        if loop:
            ks = [k for k in ks if k[0] < 1.0] + [(1.0, ks[0][1])]
        elif ks[-1][0] < 1.0:
            ks.append((1.0, ks[-1][1]))
        for t, k in ks:
            pb.rotation_euler = k.get("r", base.get("r", (0, 0, 0)))
            pb.location = k.get("l", base.get("l", (0, 0, 0)))
            s = k.get("s", base.get("s", 1.0))
            pb.scale = (s, s, s)
            f = 1 + t * frames
            for path in ("rotation_euler", "location", "scale"):
                pb.keyframe_insert(path, frame=f)
    arm.animation_data.action = None
    track = arm.animation_data.nla_tracks.new()
    track.name = name
    track.strips.new(name, 1, act)


def export(objs, path, animated):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    kw = dict(filepath=path, export_format="GLTF_SEPARATE", use_selection=True, export_yup=True,
              export_materials="EXPORT", export_image_format="NONE", export_apply=False)
    if animated:
        kw.update(export_animations=True, export_animation_mode="NLA_TRACKS",
                  export_force_sampling=True, export_optimize_animation_size=False,
                  export_skins=True, export_def_bones=False)
    else:
        kw.update(export_animations=False, export_skins=False)
    bpy.ops.export_scene.gltf(**kw)


def put_aside(objs, x):
    """Out of the next one's way in the saved file."""
    for o in objs:
        if o.parent is None:
            o.location.x += x


# ---------------------------------------------------------------- the guns

# Each gun is drawn in its own frame: origin at the top of the grip, +Y along
# the barrel, +Z up. `gun(p, bone, ...)` puts one in any frame the caller's
# bone names give, so the same shapes make the arms' guns and the held ones.

def pistol(p, b, flash=None):
    p.box(b, "gunmetal", (0, 0.07, 0.035), (0.042, 0.19, 0.045))       # slide
    p.box(b, "polymer", (0, 0.05, 0.0), (0.038, 0.15, 0.03))           # frame
    p.box(b, "polymer", (0, -0.005, -0.055), (0.034, 0.045, 0.1), (-0.25, 0, 0))   # grip
    p.box(b, "cyan", (0.022, 0.075, 0.035), (0.004, 0.13, 0.012))      # its light
    p.box(b, "cyan", (-0.022, 0.075, 0.035), (0.004, 0.13, 0.012))
    p.cyl(b, "steel", (0, 0.175, 0.03), 0.013, 0.03, "Y")
    p.box(b, "steel", (0, 0.03, 0.062), (0.01, 0.02, 0.012))           # sight
    if flash:
        p.ball(flash, "flash", (0, 0.21, 0.03), 0.04, (1, 1.6, 1), 1)


def shotgun(p, b, pump, flash=None):
    p.box(b, "gunmetal", (0, 0.06, 0.02), (0.058, 0.2, 0.075))          # receiver
    p.cyl(b, "steel", (0, 0.36, 0.045), 0.018, 0.42, "Y")                # barrel
    p.cyl(b, "gunmetal", (0, 0.33, 0.005), 0.016, 0.36, "Y")             # tube
    p.box(b, "polymer", (0, -0.14, 0.0), (0.045, 0.2, 0.065), (0.12, 0, 0))   # stock
    p.box(b, "polymer", (0, -0.02, -0.055), (0.036, 0.045, 0.095), (-0.3, 0, 0))
    p.box(b, "orange", (0.03, 0.06, 0.035), (0.004, 0.12, 0.012))
    p.box(b, "steel", (0, 0.55, 0.068), (0.008, 0.015, 0.014))           # bead
    p.box(pump, "wood", (0, 0.3, 0.005), (0.052, 0.13, 0.05))
    for dy in (-0.04, 0.0, 0.04):
        p.box(pump, "polymer", (0, 0.3 + dy, -0.021), (0.054, 0.012, 0.012))
    if flash:
        p.ball(flash, "flash", (0, 0.6, 0.045), 0.06, (1.2, 1.8, 1.2), 1)


def chaingun(p, b, spin, flash=None):
    p.box(b, "gunmetal", (0, 0.02, 0.01), (0.1, 0.24, 0.1))              # body
    p.box(b, "olive", (-0.075, 0.0, 0.0), (0.05, 0.14, 0.09))           # ammo drum
    p.box(b, "polymer", (0, -0.02, -0.06), (0.036, 0.045, 0.09), (-0.25, 0, 0))
    p.box(b, "steel", (0, 0.03, 0.075), (0.03, 0.12, 0.05))              # carry handle
    p.box(b, "orange", (0.052, 0.02, 0.02), (0.004, 0.16, 0.02))
    p.cyl(spin, "gunmetal", (0, 0.36, 0.02), 0.05, 0.03, "Y", seg=16)
    p.cyl(spin, "gunmetal", (0, 0.22, 0.02), 0.05, 0.03, "Y", seg=16)
    for i in range(4):
        a = i * math.pi / 2 + math.pi / 4
        p.cyl(spin, "steel", (0.03 * math.cos(a), 0.33, 0.02 + 0.03 * math.sin(a)), 0.011, 0.3, "Y", seg=8)
    p.cyl(spin, "steel", (0, 0.33, 0.02), 0.012, 0.3, "Y", seg=8)
    if flash:
        p.ball(flash, "flash", (0, 0.52, 0.02), 0.07, (1.4, 1.6, 1.4), 1)


def plasma(p, b, core, flash=None):
    p.box(b, "gunmetal", (0, 0.1, 0.0), (0.2, 0.36, 0.15))               # body
    p.box(b, "olive", (0, 0.1, 0.085), (0.16, 0.28, 0.03))               # top plate
    p.box(b, "gunmetal", (0, 0.31, 0.0), (0.17, 0.08, 0.13))
    p.cyl(b, "polymer", (0, 0.36, 0.0), 0.065, 0.04, "Y", seg=16)       # mouth
    p.cyl(b, "green_glow", (0, 0.382, 0.0), 0.045, 0.006, "Y", seg=16)
    for sx in (-1, 1):
        p.box(b, "green_glow", (sx * 0.102, 0.05, 0.0), (0.006, 0.18, 0.04))   # cells
        p.box(b, "steel", (sx * 0.105, 0.2, -0.04), (0.01, 0.06, 0.03))
    p.box(b, "polymer", (0, -0.03, -0.1), (0.04, 0.05, 0.1), (-0.25, 0, 0))
    p.box(b, "polymer", (0, 0.22, -0.1), (0.035, 0.04, 0.08))            # front grip
    p.box(b, "polymer", (0, -0.09, 0.0), (0.12, 0.04, 0.09))             # back plate
    p.box(b, "green_glow", (0, -0.112, 0.02), (0.06, 0.004, 0.02))       # its charge
    p.ball(core, "green_core", (0, 0.1, 0.09), 0.035, (1.2, 1.8, 1.0), 2)  # the window
    if flash:
        p.ball(flash, "green_glow", (0, 0.42, 0.0), 0.12, (1, 0.8, 1), 2)


def ssg(p, b, barrels, flash=None):
    """The super shotgun: two sawn-off barrels side by side that break open
    about a hinge in front of the receiver."""
    p.box(b, "gunmetal", (0, 0.03, 0.02), (0.064, 0.12, 0.065))          # receiver
    p.box(b, "wood", (0, -0.1, 0.0), (0.05, 0.16, 0.06), (0.2, 0, 0))     # sawn-off stock
    p.box(b, "wood", (0, -0.015, -0.055), (0.036, 0.045, 0.09), (-0.3, 0, 0))
    p.box(b, "steel", (0, 0.07, 0.058), (0.012, 0.03, 0.012))            # top lever
    p.box(b, "orange", (0.033, 0.03, 0.02), (0.004, 0.08, 0.012))
    for sx in (-1, 1):
        p.cyl(barrels, "steel", (sx * 0.021, 0.235, 0.03), 0.02, 0.29, "Y", seg=12)
        p.cyl(barrels, "polymer", (sx * 0.021, 0.382, 0.03), 0.013, 0.006, "Y", seg=10)
    p.box(barrels, "gunmetal", (0, 0.235, 0.052), (0.012, 0.29, 0.008))  # rib
    p.box(barrels, "wood", (0, 0.2, -0.002), (0.058, 0.15, 0.03))       # forend
    if flash:
        for sx in (-1, 1):
            p.ball(flash, "flash", (sx * 0.021, 0.42, 0.03), 0.05, (1.1, 1.6, 1.1), 1)


# ---------------------------------------------------------------- first person

# The arms seen in first person are the robot's own (mods/robot/robot_arms.gltf,
# the player's model here), holding the guns: each gun is a bone under the
# right hand, shown only while it is in the hand (tool_<gun>), and the hands are
# put on the grips by IK -- the right hand on the pistol grip, the left on the
# pump, the barrels or the front grip -- so a kick, a pump or a reload moves the
# arms with the gun. Every frame of every clip is solved here and baked into
# plain bone keys: the file has no constraints.
#
# The robot's hand_R is renamed wrist_R: the client draws "<item>_held" in a
# hand_R, which would put a second gun in the fist.

ROBOT_ARMS = os.path.join(ROOT, "mods", "robot", "robot_arms.gltf")
K = 1.26                         # the guns' size in the hand, of their size on the floor
GRIP = Vector((0, -0.01, -0.055))   # the middle of a gun's grip, in its own frame
FIST_R = Vector((0.265, -0.159, -1.03))   # the robot's right fist at rest (world)
FIST_L = Vector((-0.265, -0.159, -1.03))
# At rest the robot's hands point down and hold a tool along +Y; a pistol grip is
# held with the muzzle half-way between: the gun's rest turn in the fist.
TILT = Matrix.Rotation(-math.pi / 4, 4, "X")
SHOULDER_R = Vector((0.48, 0.3, -0.6))   # moved forward, so the hands reach
SHOULDER_L = Vector((-0.1, 0.62, -0.62))
AIM = (0.07, 0.2, -0.05)         # pitch, yaw towards the crosshair, roll

GUNS = {
    #            where the grip top is      the left hand: on what, where (gun frame)
    "pistol":   {"at": (0.27, 0.6, -0.225), "left": None},
    "shotgun":  {"at": (0.25, 0.54, -0.225), "left": ("pump", (0, 0.3, -0.02))},
    "ssg":      {"at": (0.24, 0.54, -0.225), "left": ("ssg_barrels", (0, 0.2, -0.03))},
    "chaingun": {"at": (0.24, 0.52, -0.235), "left": (None, (-0.02, 0.14, -0.07))},
    "plasma":      {"at": (0.18, 0.5, -0.27), "left": (None, (0, 0.22, -0.14))},
}
# Where each moving part turns or slides from, in the gun's own frame.
PARTS = {
    "pump": ("tool_shotgun", (0, 0.3, 0.0)),
    "ssg_barrels": ("tool_ssg", (0, 0.09, 0.0)),
    "barrels": ("tool_chaingun", (0, 0.3, 0.02)),
    "plasma_core": ("tool_plasma", (0, 0.1, 0.09)),
    "flash_pistol": ("tool_pistol", (0, 0.21, 0.03)),
    "flash_shotgun": ("tool_shotgun", (0, 0.6, 0.045)),
    "flash_ssg": ("ssg_barrels", (0, 0.42, 0.03)),
    "flash_chaingun": ("tool_chaingun", (0, 0.52, 0.02)),
    "flash_plasma": ("tool_plasma", (0, 0.42, 0.0)),
}
HIDDEN = 0.001   # a flash's scale while it is out: 0 would lose its turn in a matrix


def rest_frame():
    """The gun frame at rest, in the world: its grip in the right fist."""
    return Matrix.Translation(FIST_R) @ TILT @ Matrix.Translation(-K * GRIP)


def aimed(at, rot):
    """A gun frame in the eye's space: grip top at `at`, turned by
    (pitch, yaw, roll) from pointing straight ahead."""
    return (Matrix.Translation(Vector(at)) @ Matrix.Rotation(rot[1], 4, "Z") @
            Matrix.Rotation(rot[0], 4, "X") @ Matrix.Rotation(rot[2], 4, "Y"))


def curve(t, keys):
    """Piecewise linear through [(t, value)], value a number or a tuple."""
    if t <= keys[0][0]:
        return keys[0][1]
    for (t0, a), (t1, b) in zip(keys, keys[1:]):
        if t <= t1:
            u = (t - t0) / (t1 - t0) if t1 > t0 else 1.0
            if isinstance(a, tuple):
                return tuple(x + (y - x) * u for x, y in zip(a, b))
            return a + (b - a) * u
    return keys[-1][1]


def blend(ma, mb, u):
    la, ra, _ = ma.decompose()
    lb, rb, _ = mb.decompose()
    return Matrix.Translation(la.lerp(lb, u)) @ ra.slerp(rb, u).to_matrix().to_4x4()


def yafps_guns(p):
    pistol(p, "tool_pistol", "flash_pistol")
    shotgun(p, "tool_shotgun", "pump", "flash_shotgun")
    ssg(p, "tool_ssg", "ssg_barrels", "flash_ssg")
    chaingun(p, "tool_chaingun", "barrels", "flash_chaingun")
    plasma(p, "tool_plasma", "plasma_core", "flash_plasma")


def fp_arms(build_guns=None, clips=None):
    """build_guns(p) puts the guns in the fist, clips(capture, moved, flash,
    sub_matrix, free_left, Q_R) captures their fire clips: yafps's when not
    given. GUNS, PARTS and ARMS_OUT are read from this module, so a world with
    guns of its own sets them first (tools/make_front_models.py)."""
    bpy.ops.import_scene.gltf(filepath=ROBOT_ARMS)
    rg = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
    body = next(o for o in bpy.context.scene.objects if o.type == "MESH" and o.parent == rg)
    for o in list(bpy.context.scene.objects):
        if o not in (rg, body):
            bpy.data.objects.remove(o)
    rg.name, body.name = "robot_arms", "robot_arms_body"
    W = rg.matrix_world.copy()        # the robot's arms are scaled 1.326
    Wi = W.inverted()

    # The robot's own tools out: their vertices, their bones, and the swing that
    # would play while the trigger is held.
    bm = bmesh.new()
    bm.from_mesh(body.data)
    dl = bm.verts.layers.deform.verify()
    tool_groups = {g.index for g in body.vertex_groups if g.name.startswith("tool_")}
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if any(v[dl].get(i, 0) > 0 for i in tool_groups)],
                     context="VERTS")
    bm.to_mesh(body.data)
    bm.free()
    ad = rg.animation_data
    for tr in list(ad.nla_tracks):
        if tr.name == "swing":
            ad.nla_tracks.remove(tr)
        else:
            tr.mute = True
    ad.action = None
    rg.data.bones["hand_R"].name = "wrist_R"

    A0 = rest_frame()
    Y = (A0.to_3x3() @ Vector((0, 1, 0))).normalized()
    Z = (A0.to_3x3() @ Vector((0, 0, 1))).normalized()

    def at_rest(bone_or_gun, local):
        return A0 @ (K * Vector(local))

    bpy.context.view_layer.objects.active = rg
    bpy.ops.object.mode_set(mode="EDIT")
    eb = rg.data.edit_bones
    for b in [b for b in eb if b.name.startswith("tool_")]:
        eb.remove(b)
    # Each arm one chain, for the IK: the robot's bones stop short of the next
    # one's head, on the same line, and are drawn out to meet it.
    for up, fore, hand in (("upper_arm_R", "forearm_R", "wrist_R"), ("upper_arm_L", "forearm_L", "hand_L")):
        eb[up].tail = eb[fore].head
        eb[fore].tail = eb[hand].head
        eb[fore].use_connect = True

    def bone(name, parent, head_world):
        e = eb.new(name)
        e.head = Wi @ head_world
        e.tail = Wi @ (head_world + Y * 0.05)
        e.align_roll(Wi.to_3x3() @ Z)
        e.parent = eb[parent]
        e.use_deform = True

    bone("gun", "wrist_R", at_rest(None, (0, 0, 0)))
    for g in GUNS:
        bone(f"tool_{g}", "gun", at_rest(None, (0, 0, 0)))
    for name, (parent, local) in PARTS.items():
        bone(name, parent, at_rest(None, local))
    sh = eb.new("shells")
    sh.head = Wi @ FIST_L
    sh.tail = Wi @ (FIST_L + Vector((0, 0.05, 0)))
    sh.parent = eb["hand_L"]
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in rg.pose.bones:
        pb.rotation_mode = "QUATERNION"

    # The guns, built in the fist.
    p = Parts(A0 @ Matrix.Diagonal((K, K, K, 1)))
    (build_guns or yafps_guns)(p)
    guns = p.build("robot_arms_guns")
    # Two shells in the left fist, shown while it reloads the super shotgun.
    q = Parts()
    for dx in (-0.018, 0.018):
        q.cyl("shells", "shell_red", FIST_L + Vector((dx, 0.0, 0.0)), 0.016, 0.07, "Y", seg=8)
        q.cyl("shells", "brass", FIST_L + Vector((dx, -0.035, 0.0)), 0.018, 0.012, "Y", seg=8)
    shells = q.build("robot_arms_shells")
    for m in (guns, shells):
        m.parent = rg
        m.matrix_parent_inverse = Wi
        mod = m.modifiers.new("Armature", "ARMATURE")
        mod.object = rg

    # IK: each forearm reaches for its wrist's target, each hand turns as it
    # does; the elbows out and down.
    targets = {}
    for side, fore, hand, pole in (("R", "forearm_R", "wrist_R", (0.9, 0.3, -0.9)),
                                   ("L", "forearm_L", "hand_L", (-0.9, 0.5, -0.9))):
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

    rest = {b.name: b.matrix_local.copy() for b in rg.data.bones}
    H0_R = W @ rest["wrist_R"]
    H0_L = W @ rest["hand_L"]
    Q_R = A0.inverted() @ H0_R       # the right hand in the gun's frame

    def sub_matrix(name, s):
        """A part's own move, in the gun's frame: slid by l, turned by r about
        where it hangs."""
        if name is None or name not in s:
            return Matrix.Identity(4)
        loc, rot = s[name].get("l", (0, 0, 0)), s[name].get("r", (0, 0, 0))
        h = K * Vector(PARTS[name][1])
        e = Matrix.Rotation(rot[2], 4, "Z") @ Matrix.Rotation(rot[1], 4, "Y") @ Matrix.Rotation(rot[0], 4, "X")
        return Matrix.Translation(h + K * Vector(loc)) @ e @ Matrix.Translation(-h)

    def set_target(t, m):
        loc, rot, _ = m.decompose()
        t.location = loc
        t.rotation_quaternion = rot

    def free_left(pos, rot=(0, 0, 0)):
        return (Matrix.Translation(Vector(pos)) @ aimed((0, 0, 0), rot) @
                Matrix.Translation(-H0_L.to_translation()) @ H0_L)

    captured = {}

    def capture(name, seconds, state):
        """state(t) -> {"at", "rot", "parts": {bone: {"l", "r", "s"}},
        "left": matrix or None (the gun's own grip), "shells": bool}"""
        frames = max(1, round(seconds * FPS))
        out = []
        for f in range(frames + 1):
            st = state(f / FPS)
            A = aimed(st["at"], st["rot"])
            set_target(targets["R"], A @ Q_R)
            gun = st["gun"]
            left = st.get("left")
            if left is None:
                side = GUNS[gun]["left"]
                if side is None:
                    left = free_left((-0.35, 0.55, -1.0), (0.4, 0, 0))
                else:
                    part, grip = side
                    left = (A @ sub_matrix(part, st["parts"]) @
                            Matrix.Translation(K * (Vector(grip) - GRIP)) @ Q_R)
            elif callable(left):
                left = left(A)
            set_target(targets["L"], left)
            for pb in rg.pose.bones:
                pb.matrix_basis = Matrix.Identity(4)
            for s_name, (r_rest, want) in (("upper_arm_R", (rest["upper_arm_R"], SHOULDER_R)),
                                          ("upper_arm_L", (rest["upper_arm_L"], SHOULDER_L))):
                off = Wi @ want - r_rest.to_translation()
                rg.pose.bones[s_name].matrix_basis = (r_rest.inverted() @ Matrix.Translation(off) @ r_rest)
            # A bend to start from: the robot's arms are straight at rest, and
            # the solver cannot fold a chain in a straight line.
            for fore in ("forearm_R", "forearm_L"):
                rg.pose.bones[fore].rotation_quaternion = Matrix.Rotation(0.9, 3, "X").to_quaternion()
            for pname, ch in st["parts"].items():
                pb = rg.pose.bones[pname]
                loc, rot = ch.get("l", (0, 0, 0)), ch.get("r", (0, 0, 0))
                s = ch.get("s", 1.0)
                pb.location = Vector(loc) * K / W.to_scale()[0]
                pb.rotation_quaternion = (Matrix.Rotation(rot[1], 3, "Y") @ Matrix.Rotation(rot[0], 3, "X") @
                                          Matrix.Rotation(rot[2], 3, "Z")).to_quaternion()
                pb.scale = (s, s, s)
            rg.pose.bones["shells"].scale = (1, 1, 1) if st.get("shells") else (HIDDEN,) * 3
            for fl in [n for n in PARTS if n.startswith("flash_")]:
                if fl not in st["parts"]:
                    rg.pose.bones[fl].scale = (HIDDEN,) * 3
            bpy.context.view_layer.update()
            out.append({pb.name: pb.matrix.copy() for pb in rg.pose.bones})
        # How far the hands fell short of their targets: a clip whose gun is
        # out of the arms' reach says so.
        worst = max((W @ fr["wrist_R"].to_translation() - targets["R"].location).length for fr in out[-1:])
        captured[name] = out
        if worst > 0.01:
            print(f"robot_arms {name}: right hand {worst:.3f} short")
        wl = (W @ out[-1]["hand_L"].to_translation() - targets["L"].location).length
        if wl > 0.01:
            print(f"robot_arms {name}: left hand {wl:.3f} short")

    def still(gun, extra=None):
        base = {"gun": gun, "at": GUNS[gun]["at"], "rot": AIM, "parts": {}}
        if gun == "plasma":
            base["parts"]["plasma_core"] = {"s": 1.0}
        if extra:
            base.update(extra)
        return base

    def moved(gun, t, dat, drot, parts=None, **kw):
        at = tuple(a + b for a, b in zip(GUNS[gun]["at"], dat))
        rot = tuple(a + b for a, b in zip(AIM, drot))
        st = still(gun, {"at": at, "rot": rot})
        st["parts"].update(parts or {})
        st.update(kw)
        return st

    Z3 = (0, 0, 0)
    for g in GUNS:
        # A breath: the gun rises and falls a little, and drifts.
        capture(f"idle_{g}", 3.0, lambda t, g=g: moved(
            g, t, (0, 0, 0.004 * math.sin(t / 3 * 2 * math.pi)),
            (0.012 * math.sin(t / 3 * 2 * math.pi), 0.008 * math.sin(t / 3 * 4 * math.pi), 0)))
        # Up from under the screen.
        capture(f"raise_{g}", 0.35, lambda t, g=g: moved(
            g, t, curve(t, [(0, (0.04, -0.09, -0.45)), (0.35, Z3)]),
            curve(t, [(0, (-0.9, 0.0, 0.3)), (0.35, Z3)])))

    def flash(name, t, on, off):
        return {name: {"s": 1.0}} if on <= t < off else {}

    if clips is not None:
        clips(capture, moved, flash, sub_matrix, free_left, Q_R)
    else:
        capture("fire_pistol", 0.28, lambda t: moved(
            "pistol", t, curve(t, [(0, Z3), (0.05, (0, -0.03, 0.01)), (0.28, Z3)]),
            curve(t, [(0, Z3), (0.05, (0.22, 0, 0.03)), (0.28, Z3)]),
            flash("flash_pistol", t, 0, 0.05)))

        capture("fire_shotgun", 0.85, lambda t: moved(
            "shotgun", t, curve(t, [(0, Z3), (0.06, (0, -0.06, 0.012)), (0.3, Z3), (0.85, Z3)]),
            curve(t, [(0, Z3), (0.06, (0.36, 0, 0.04)), (0.3, (0.04, 0, 0)), (0.62, (0.04, 0, -0.12)),
                      (0.85, Z3)]),
            {"pump": {"l": curve(t, [(0, Z3), (0.35, Z3), (0.48, (0, -0.1, 0)), (0.62, Z3)])},
             **flash("flash_shotgun", t, 0, 0.06)}))

        # The super shotgun: both barrels, a heavy kick, and then the reload the
        # modern one is known for -- broken open, two shells out of the left hand's
        # pocket and in, snapped shut.
        def ssg_state(t):
            opened = curve(t, [(0, 0.0), (0.3, 0.0), (0.42, -0.75), (1.02, -0.75), (1.1, 0.0)])
            st = moved(
                "ssg", t,
                curve(t, [(0, Z3), (0.05, (0, -0.08, 0.02)), (0.25, Z3), (0.4, (0.01, 0, 0.02)),
                          (1.0, (0.01, 0, 0.02)), (1.12, (0, 0.01, 0)), (1.4, Z3)]),
                curve(t, [(0, Z3), (0.05, (0.5, 0, 0.06)), (0.25, (0.02, 0, 0)), (0.4, (0.2, 0, -0.15)),
                          (1.0, (0.2, 0, -0.15)), (1.1, (0.05, 0, 0.02)), (1.4, Z3)]),
                {"ssg_barrels": {"r": (opened, 0, 0)}, **flash("flash_ssg", t, 0, 0.06)},
                shells=0.55 <= t < 0.9)

            def left(A):
                on_barrels = (A @ sub_matrix("ssg_barrels", st["parts"]) @
                              Matrix.Translation(K * (Vector(GUNS["ssg"]["left"][1]) - GRIP)) @ Q_R)
                breech = A @ Matrix.Translation(K * (Vector((0, 0.08, 0.07)) - GRIP)) @ Q_R
                pocket = free_left((-0.4, 0.5, -0.75), (0.2, 0, 0.3))
                if t < 0.42 or t >= 1.02:
                    return on_barrels
                if t < 0.6:
                    return blend(on_barrels, pocket, (t - 0.42) / 0.18)
                if t < 0.8:
                    return blend(pocket, breech, (t - 0.6) / 0.2)
                if t < 0.9:
                    return breech
                return blend(breech, on_barrels, (t - 0.9) / 0.12)
            st["left"] = left
            return st
        capture("fire_ssg", 1.4, ssg_state)

        capture("fire_chaingun", 1.0 / 12.0, lambda t: moved(
            "chaingun", t, curve(t, [(0, Z3), (0.03, (0, -0.012, 0.004)), (1 / 12, Z3)]),
            curve(t, [(0, Z3), (0.03, (0.035, 0, 0.01)), (1 / 12, Z3)]),
            {"barrels": {"r": (0, curve(t, [(0, 0.0), (1 / 12, math.pi / 2)]), 0)},
             **flash("flash_chaingun", t, 0, 0.045)}))

        capture("fire_plasma", 1.1, lambda t: moved(
            "plasma", t, curve(t, [(0, Z3), (0.55, (0, 0.01, -0.01)), (0.62, (0, -0.1, 0.03)), (1.1, Z3)]),
            curve(t, [(0, Z3), (0.55, (-0.03, 0, 0)), (0.62, (0.5, 0, 0.05)), (1.1, Z3)]),
            {"plasma_core": {"s": curve(t, [(0, 1.0), (0.55, 1.9), (0.62, 0.6), (1.1, 1.0)])},
             **flash("flash_plasma", t, 0.6, 0.76)}))

    # Baked: the constraints go, and each frame's pose becomes keys.
    for pb in rg.pose.bones:
        for c in list(pb.constraints):
            pb.constraints.remove(c)
    for o in list(targets.values()) + [bpy.data.objects["pole_R"], bpy.data.objects["pole_L"]]:
        bpy.data.objects.remove(o)
    parent = {b.name: (b.parent.name if b.parent else None) for b in rg.data.bones}
    for name, frames in captured.items():
        act = bpy.data.actions.new(f"robot_arms_{name}")
        ad.action = act
        prev = {}   # each bone's last turn: a quaternion and its negative are the same
        # turn, and keys that flip between them would spin the bone in between
        for f, pose in enumerate(frames):
            for pb in rg.pose.bones:
                R = rest[pb.name]
                P = pose[pb.name]
                if parent[pb.name]:
                    Rp, Pp = rest[parent[pb.name]], pose[parent[pb.name]]
                    basis = (Rp.inverted() @ R).inverted() @ Pp.inverted() @ P
                else:
                    basis = R.inverted() @ P
                loc, rot, sc = basis.decompose()
                if pb.name in prev and prev[pb.name].dot(rot) < 0:
                    rot.negate()
                prev[pb.name] = rot
                pb.location, pb.rotation_quaternion, pb.scale = loc, rot, sc
                for path in ("location", "rotation_quaternion", "scale"):
                    pb.keyframe_insert(path, frame=1 + f)
        ad.action = None
        tr = ad.nla_tracks.new()
        tr.name = name
        tr.strips.new(name, 1, act)
    for tr in ad.nla_tracks:
        tr.mute = False
    for pb in rg.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)

    export([rg, body, guns, shells], os.path.join(ARMS_OUT, "robot_arms.gltf"), True)
    put_aside([rg], -2.0)


def held_guns():
    for name, make in [("pistol", lambda p: pistol(p, None)),
                       ("shotgun", lambda p: shotgun(p, None, None)),
                       ("ssg", lambda p: ssg(p, None, None)),
                       ("chaingun", lambda p: chaingun(p, None, None)),
                       ("plasma", lambda p: plasma(p, None, None))]:
        p = Parts()
        make(p)
        obj = p.build(f"{name}_held")
        export([obj], os.path.join(OUT, f"{name}_held.gltf"), False)
        put_aside([obj], -4.0 - len(name) * 0.1)


# ---------------------------------------------------------------- the demons

def biped(prefix, hip, chest, head, shoulder, arm_x):
    return [
        ("root", None, (0, 0, 0)),
        ("hips", "root", (0, 0, hip)),
        ("chest", "hips", (0, 0, chest)),
        ("head", "chest", (0, 0.02, head)),
        ("arm_L", "chest", (-arm_x, 0, shoulder)),
        ("arm_R", "chest", (arm_x, 0, shoulder)),
        ("leg_L", "hips", (-0.13, 0, hip)),
        ("leg_R", "hips", (0.13, 0, hip)),
    ]


def gait(swing, arm_swing, bob, lean=0.0):
    return {
        "leg_L": [(0.0, {"r": (swing, 0, 0)}), (0.5, {"r": (-swing, 0, 0)})],
        "leg_R": [(0.0, {"r": (-swing, 0, 0)}), (0.5, {"r": (swing, 0, 0)})],
        "arm_L": [(0.0, {"r": (-arm_swing, 0, 0)}), (0.5, {"r": (arm_swing, 0, 0)})],
        "arm_R": [(0.0, {"r": (arm_swing, 0, 0)}), (0.5, {"r": (-arm_swing, 0, 0)})],
        "hips": [(0.0, {"l": (0, 0, 0)}), (0.25, {"l": (0, bob, 0)}), (0.5, {"l": (0, 0, 0)}),
                 (0.75, {"l": (0, bob, 0)})],
        "chest": [(0.0, {"r": (lean, 0, 0)})],
    }


def imp():
    rg = rig("imp_rig", biped("imp", 0.8, 1.1, 1.42, 1.34, 0.27))
    p = Parts()
    # legs: thigh and a backward shin, clawed feet
    for sx, b in ((-1, "leg_L"), (1, "leg_R")):
        x = sx * 0.13
        p.limb(b, "imp_skin", (x, 0, 0.82), (x, 0.06, 0.42), 0.14, 0.15)
        p.limb(b, "imp_dark", (x, 0.06, 0.44), (x, -0.04, 0.06), 0.1, 0.11)
        p.box(b, "imp_dark", (x, 0.04, 0.03), (0.13, 0.22, 0.06))
        for dx in (-0.04, 0.04):
            p.spike(b, "bone", (x + dx, 0.13, 0.03), (x + dx, 0.2, 0.01), 0.02)
    # body: hunched, a lighter belly, spikes down the back and on the shoulders
    p.box("hips", "imp_dark", (0, 0, 0.86), (0.36, 0.22, 0.16))
    p.box("chest", "imp_skin", (0, 0.02, 1.12), (0.42, 0.28, 0.46), (0.15, 0, 0))
    p.box("chest", "imp_dark", (0, 0.13, 1.08), (0.26, 0.06, 0.3), (0.15, 0, 0))
    for i, z in enumerate((1.0, 1.14, 1.28)):
        p.spike("chest", "bone", (0, -0.12, z), (0, -0.24 - 0.03 * i, z + 0.08), 0.035)
    for sx in (-1, 1):
        p.spike("chest", "bone", (sx * 0.2, -0.02, 1.36), (sx * 0.3, -0.06, 1.5), 0.04)
        p.spike("chest", "bone", (sx * 0.14, -0.05, 1.38), (sx * 0.17, -0.1, 1.52), 0.03)
    # head: low brow, glowing eyes, a mouth of teeth
    p.box("head", "imp_skin", (0, 0.06, 1.5), (0.22, 0.24, 0.2))
    p.box("head", "imp_dark", (0, 0.13, 1.56), (0.24, 0.1, 0.05))
    p.box("head", "eye_orange", (-0.06, 0.18, 1.52), (0.05, 0.02, 0.025))
    p.box("head", "eye_orange", (0.06, 0.18, 1.52), (0.05, 0.02, 0.025))
    p.box("head", "mouth", (0, 0.17, 1.44), (0.14, 0.03, 0.04))
    for dx in (-0.045, 0.0, 0.045):
        p.spike("head", "bone", (dx, 0.185, 1.46), (dx, 0.185, 1.425), 0.012)
    for sx in (-1, 1):
        p.spike("head", "bone", (sx * 0.08, 0.02, 1.6), (sx * 0.14, -0.08, 1.7), 0.03)
    # arms: long, with claws; the right one throws, with the fire in its palm
    for sx, b in ((-1, "arm_L"), (1, "arm_R")):
        x = sx * 0.27
        p.limb(b, "imp_skin", (x, 0, 1.36), (x + sx * 0.03, 0.04, 1.0), 0.11, 0.11)
        p.limb(b, "imp_dark", (x + sx * 0.03, 0.04, 1.02), (x + sx * 0.04, 0.1, 0.72), 0.09, 0.09)
        for dx in (-0.03, 0.0, 0.03):
            p.spike(b, "bone", (x + sx * 0.04 + dx, 0.1, 0.72), (x + sx * 0.04 + dx, 0.15, 0.64), 0.013)
    p.ball("fire_R", "fire_outer", (0.31, 0.12, 0.74), 0.09, sub=1)
    mesh = p.build("imp_mesh")
    add_bone(rg, "fire_R", "arm_R", (0.31, 0.12, 0.74))
    skin(mesh, rg)

    hidden = {"fire_R": {"s": 0.0}}
    clip(rg, "idle", 2.0, {
        "chest": [(0.0, {"r": (0.12, 0, 0)}), (0.5, {"r": (0.18, 0, 0.04)})],
        "head": [(0.0, {}), (0.5, {"r": (-0.1, 0, -0.15)})],
        "arm_L": [(0.0, {"r": (0.1, 0, -0.1)})],
        "arm_R": [(0.0, {"r": (0.1, 0, 0.1)})],
    }, hidden, loop=True)
    clip(rg, "walk", 0.9, gait(0.5, 0.4, 0.03, 0.15), hidden, loop=True)
    clip(rg, "run", 0.55, gait(0.85, 0.8, 0.06, 0.35), hidden, loop=True)
    clip(rg, "shoot", 1.0, {
        "chest": [(0.0, {"r": (0.12, 0, 0)}), (0.35, {"r": (0.0, 0, 0.3)}), (0.55, {"r": (0.35, 0, -0.25)}),
                  (1.0, {"r": (0.12, 0, 0)})],
        "arm_R": [(0.0, {}), (0.35, {"r": (-1.9, 0, 0.2)}), (0.55, {"r": (1.5, 0, 0)}), (1.0, {})],
        "fire_R": [(0.0, {"s": 0.0}), (0.15, {"s": 0.6}), (0.45, {"s": 1.4}), (0.52, {"s": 0.0})],
    }, hidden)
    clip(rg, "attack", 0.8, {
        "arm_R": [(0.0, {}), (0.4, {"r": (-1.8, 0, 0)}), (0.6, {"r": (1.2, 0, 0)}), (1.0, {})],
    }, hidden)
    # Hit, a step back, arms thrown up, and down on its back. The client plays
    # it once while the body fades out (fade_the_gone in src/client/main.cpp).
    clip(rg, "death", 1.0, {
        "root": [(0.0, {}), (0.25, {"r": (0.2, 0, 0.1)}), (0.7, {"r": (1.5, 0, 0.2), "l": (0, 0.1, 0)}),
                 (0.85, {"r": (1.42, 0, 0.2), "l": (0, 0.14, 0)})],
        "chest": [(0.0, {}), (0.25, {"r": (-0.4, 0, 0)})],
        "head": [(0.0, {}), (0.3, {"r": (-0.5, 0, 0.3)})],
        "arm_L": [(0.0, {}), (0.3, {"r": (-2.2, 0, -0.6)}), (0.8, {"r": (-2.6, 0, -0.9)})],
        "arm_R": [(0.0, {}), (0.3, {"r": (-2.0, 0, 0.6)}), (0.8, {"r": (-2.7, 0, 1.0)})],
        "leg_L": [(0.0, {}), (0.4, {"r": (0.6, 0, 0)}), (0.8, {"r": (0.2, 0, -0.2)})],
        "leg_R": [(0.0, {}), (0.4, {"r": (-0.3, 0, 0)}), (0.8, {"r": (0.4, 0, 0.2)})],
    }, hidden)
    export([rg, mesh], os.path.join(OUT, "imp.gltf"), True)
    put_aside([rg], 2.0)


def add_bone(rg, name, parent, head):
    bpy.context.view_layer.objects.active = rg
    bpy.ops.object.mode_set(mode="EDIT")
    eb = rg.data.edit_bones.new(name)
    eb.head = head
    eb.tail = (head[0], head[1], head[2] + 0.1)
    eb.roll = 0.0
    eb.parent = rg.data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    rg.pose.bones[name].rotation_mode = "XYZ"


def pinky():
    rg = rig("pinky_rig", [
        ("root", None, (0, 0, 0)),
        ("body", "root", (0, -0.2, 0.62)),
        ("head", "body", (0, 0.42, 0.78)),
        ("jaw", "head", (0, 0.5, 0.66)),
        ("leg_FL", "body", (-0.3, 0.28, 0.6)),
        ("leg_FR", "body", (0.3, 0.28, 0.6)),
        ("leg_BL", "body", (-0.24, -0.42, 0.55)),
        ("leg_BR", "body", (0.24, -0.42, 0.55)),
    ])
    p = Parts()
    # a bull of a thing: a hump of muscle before, a smaller rump behind
    p.box("body", "pinky_skin", (0, 0.1, 0.8), (0.78, 0.6, 0.62))
    p.box("body", "pinky_dark", (0, 0.05, 1.12), (0.5, 0.36, 0.1))
    p.box("body", "pinky_skin", (0, -0.4, 0.7), (0.56, 0.44, 0.44))
    for i in range(4):
        p.box("body", "pinky_dark", (0, 0.3 - i * 0.16, 1.06 - i * 0.05), (0.62, 0.05, 0.1))
    p.spike("body", "pinky_dark", (0, -0.62, 0.72), (0, -0.82, 0.62), 0.05)   # tail
    for sx in (-1, 1):
        p.box("body", "steel", (sx * 0.4, -0.35, 0.72), (0.03, 0.2, 0.08))   # the bolts in its hide
    # legs: thick in front, bent behind, hooves
    for b, x, y, w in (("leg_FL", -0.3, 0.28, 0.2), ("leg_FR", 0.3, 0.28, 0.2),
                       ("leg_BL", -0.24, -0.42, 0.17), ("leg_BR", 0.24, -0.42, 0.17)):
        p.limb(b, "pinky_skin", (x, y, 0.64), (x, y + 0.02, 0.1), w, w)
        p.box(b, "imp_dark", (x, y + 0.03, 0.05), (w + 0.03, w + 0.06, 0.1))
    # head: small eyes, a jaw full of teeth, horns
    p.box("head", "pinky_skin", (0, 0.6, 0.86), (0.46, 0.38, 0.3))
    p.box("head", "pinky_dark", (0, 0.62, 1.03), (0.4, 0.3, 0.06))
    p.box("head", "eye_yellow", (-0.14, 0.8, 0.94), (0.06, 0.02, 0.035))
    p.box("head", "eye_yellow", (0.14, 0.8, 0.94), (0.06, 0.02, 0.035))
    p.box("head", "mouth", (0, 0.72, 0.72), (0.38, 0.2, 0.05))
    for sx in (-1, 1):
        p.spike("head", "bone", (sx * 0.2, 0.6, 1.0), (sx * 0.34, 0.72, 1.14), 0.05)
    for i in range(6):
        x = -0.15 + i * 0.06
        p.spike("head", "bone", (x, 0.79, 0.76), (x, 0.8, 0.68), 0.018)
        p.spike("jaw", "bone", (x, 0.77, 0.63), (x, 0.78, 0.71), 0.018)
    p.box("jaw", "pinky_skin", (0, 0.66, 0.6), (0.4, 0.26, 0.1))
    mesh = p.build("pinky_mesh")
    skin(mesh, rg)

    def run(swing, bob, s):
        return {
            "leg_FL": [(0.0, {"r": (swing, 0, 0)}), (0.5, {"r": (-swing, 0, 0)})],
            "leg_BR": [(0.0, {"r": (swing, 0, 0)}), (0.5, {"r": (-swing, 0, 0)})],
            "leg_FR": [(0.0, {"r": (-swing, 0, 0)}), (0.5, {"r": (swing, 0, 0)})],
            "leg_BL": [(0.0, {"r": (-swing, 0, 0)}), (0.5, {"r": (swing, 0, 0)})],
            "body": [(0.0, {"l": (0, 0, 0)}), (0.25, {"l": (0, bob, 0), "r": (s, 0, 0)}),
                     (0.5, {"l": (0, 0, 0)}), (0.75, {"l": (0, bob, 0), "r": (-s, 0, 0)})],
            "head": [(0.0, {"r": (0.1, 0, 0)}), (0.5, {"r": (-0.05, 0, 0)})],
            "jaw": [(0.0, {"r": (0.15, 0, 0)})],
        }
    clip(rg, "idle", 1.6, {
        "body": [(0.0, {}), (0.5, {"l": (0, -0.02, 0)})],
        "jaw": [(0.0, {"r": (0.05, 0, 0)}), (0.5, {"r": (0.2, 0, 0)})],
        "head": [(0.0, {}), (0.5, {"r": (0, 0, 0.12)})],
    }, loop=True)
    clip(rg, "walk", 0.7, run(0.45, 0.03, 0.03), loop=True)
    clip(rg, "run", 0.42, run(0.8, 0.07, 0.08), loop=True)
    clip(rg, "attack", 0.6, {
        "body": [(0.0, {}), (0.3, {"l": (0, 0, -0.35), "r": (0.2, 0, 0)}), (1.0, {})],
        "head": [(0.0, {}), (0.2, {"r": (-0.3, 0, 0)}), (0.35, {"r": (0.35, 0, 0)}), (1.0, {})],
        "jaw": [(0.0, {}), (0.2, {"r": (0.7, 0, 0)}), (0.35, {"r": (0.0, 0, 0)}), (1.0, {})],
    })
    # Skids, and rolls over onto its side with its legs out and its jaw open.
    clip(rg, "death", 1.0, {
        "root": [(0.0, {}), (0.2, {"l": (0, 0, -0.2)}), (0.7, {"r": (0, 0, 1.45), "l": (0, 0.1, -0.3)}),
                 (0.85, {"r": (0, 0, 1.4), "l": (0, 0.12, -0.3)})],
        "head": [(0.0, {}), (0.3, {"r": (-0.5, 0, 0)}), (0.8, {"r": (0.3, 0, 0.3)})],
        "jaw": [(0.0, {}), (0.3, {"r": (0.8, 0, 0)}), (0.8, {"r": (0.6, 0, 0)})],
        "leg_FL": [(0.0, {}), (0.6, {"r": (-0.9, 0, -0.4)})],
        "leg_FR": [(0.0, {}), (0.6, {"r": (-0.7, 0, 0.5)})],
        "leg_BL": [(0.0, {}), (0.6, {"r": (0.9, 0, -0.4)})],
        "leg_BR": [(0.0, {}), (0.6, {"r": (0.8, 0, 0.5)})],
    })
    export([rg, mesh], os.path.join(OUT, "pinky.gltf"), True)
    put_aside([rg], 4.0)


def gazer():
    c = 0.65   # the middle of the ball, over the feet
    rg = rig("caco_rig", [
        ("root", None, (0, 0, 0)),
        ("body", "root", (0, 0, c)),
        ("jaw", "body", (0, 0.2, c - 0.1)),
        ("eye", "body", (0, 0.5, c + 0.12)),
    ])
    p = Parts()
    p.ball("body", "caco_skin", (0, 0, c), 0.6, (1, 1, 0.95), 3)
    p.ball("body", "caco_dark", (0, -0.05, c + 0.05), 0.585, (1.02, 1.02, 0.97), 1)   # the blotches
    p.ball("eye", "eye_white", (0, 0.5, c + 0.14), 0.18, (1, 0.5, 1), 2)
    p.ball("eye", "eye_green", (0, 0.58, c + 0.14), 0.09, (1, 0.5, 1), 2)
    p.ball("eye", "polymer", (0, 0.61, c + 0.14), 0.035, (1, 0.4, 1.4), 1)
    p.box("body", "caco_dark", (0, 0.5, c + 0.3), (0.34, 0.1, 0.06), (0.3, 0, 0))    # brow
    for i, (x, z) in enumerate(((0.0, 0.6), (-0.28, 0.5), (0.28, 0.5), (-0.46, 0.3), (0.46, 0.3))):
        p.spike("body", "bone" if i == 0 else "caco_dark", (x, -0.05, c + z), (x * 1.3, -0.1, c + z + 0.3), 0.07)
    # the mouth: a dark gape under the eye, teeth above and on the jaw
    p.ball("body", "mouth", (0, 0.42, c - 0.2), 0.3, (1.2, 0.6, 0.5), 2)
    p.ball("jaw", "caco_skin", (0, 0.3, c - 0.33), 0.36, (1.15, 0.9, 0.45), 2)
    for i in range(7):
        a = -0.9 + i * 0.3
        x, y = math.sin(a) * 0.3, 0.42 + math.cos(a) * 0.14
        p.spike("body", "bone", (x, y, c - 0.1), (x, y + 0.02, c - 0.2), 0.025)
        p.spike("jaw", "bone", (x, y - 0.02, c - 0.36), (x, y, c - 0.26), 0.022)
    mesh = p.build("caco_mesh")
    skin(mesh, rg)
    float_ = {
        "body": [(0.0, {"l": (0, 0, 0)}), (0.5, {"l": (0, 0.06, 0), "r": (0, 0.05, 0)})],
        "jaw": [(0.0, {"r": (0.05, 0, 0)}), (0.5, {"r": (0.12, 0, 0)})],
        "eye": [(0.0, {}), (0.25, {"r": (0, 0, 0.1)}), (0.75, {"r": (0, 0, -0.1)})],
    }
    clip(rg, "idle", 2.4, float_, loop=True)
    clip(rg, "walk", 1.6, float_, loop=True)
    clip(rg, "run", 1.0, float_, loop=True)
    clip(rg, "shoot", 1.0, {
        "body": [(0.0, {}), (0.3, {"r": (-0.25, 0, 0)}), (0.5, {"r": (0.2, 0, 0)}), (1.0, {})],
        "jaw": [(0.0, {}), (0.3, {"r": (0.8, 0, 0)}), (0.6, {"r": (0.8, 0, 0)}), (1.0, {})],
    })
    # The float goes out of it: it drops out of the air, tipping, mouth wide,
    # the eye rolling up.
    clip(rg, "death", 1.0, {
        "body": [(0.0, {}), (0.15, {"l": (0, 0.2, 0), "r": (-0.3, 0, 0)}),
                 (0.75, {"l": (0, -2.8, 0), "r": (0.7, 0.4, 0.3)}), (0.85, {"l": (0, -2.7, 0), "r": (0.65, 0.4, 0.3)})],
        "jaw": [(0.0, {}), (0.2, {"r": (1.0, 0, 0)})],
        "eye": [(0.0, {}), (0.3, {"r": (-0.5, 0, 0)})],
    })
    export([rg, mesh], os.path.join(OUT, "gazer.gltf"), True)
    put_aside([rg], 6.0)


def ghoul():
    rg = rig("ghoul_rig", biped("ghoul", 0.86, 1.15, 1.5, 1.4, 0.22))
    add_bone(rg, "flash", "arm_R", (0.02, 0.7, 1.12))
    p = Parts()
    for sx, b in ((-1, "leg_L"), (1, "leg_R")):
        x = sx * 0.11
        p.limb(b, "uniform", (x, 0, 0.88), (x, 0, 0.1), 0.15, 0.16)
        p.box(b, "polymer", (x, 0.03, 0.05), (0.15, 0.24, 0.1))
    p.box("hips", "uniform", (0, 0, 0.9), (0.36, 0.2, 0.14))
    p.box("chest", "uniform", (0, 0, 1.17), (0.42, 0.24, 0.46))
    p.box("chest", "olive", (0, 0.1, 1.2), (0.36, 0.08, 0.36))     # vest
    p.box("chest", "eye_red", (0.1, 0.145, 1.3), (0.04, 0.01, 0.02))
    p.box("head", "zombie_skin", (0, 0.02, 1.55), (0.2, 0.2, 0.22))
    p.box("head", "olive", (0, 0.0, 1.66), (0.23, 0.23, 0.08))     # helmet
    p.box("head", "eye_red", (-0.05, 0.12, 1.57), (0.04, 0.01, 0.02))
    p.box("head", "eye_red", (0.05, 0.12, 1.57), (0.04, 0.01, 0.02))
    p.box("head", "mouth", (0, 0.12, 1.49), (0.08, 0.01, 0.02))
    # arms held forward on a rifle, the right one's hand at the grip
    p.limb("arm_R", "uniform", (0.22, 0, 1.4), (0.2, 0.28, 1.14), 0.11)
    p.limb("arm_L", "uniform", (-0.22, 0, 1.4), (-0.08, 0.36, 1.12), 0.11)
    p.box("arm_R", "zombie_skin", (0.12, 0.28, 1.12), (0.07, 0.08, 0.07))
    p.box("arm_L", "zombie_skin", (-0.04, 0.42, 1.12), (0.07, 0.08, 0.07))
    p.box("arm_R", "gunmetal", (0.03, 0.3, 1.16), (0.06, 0.5, 0.08))
    p.cyl("arm_R", "steel", (0.03, 0.62, 1.17), 0.014, 0.2, "Y")
    p.box("arm_R", "wood", (0.03, 0.0, 1.14), (0.05, 0.2, 0.08))
    p.box("arm_R", "polymer", (0.03, 0.34, 1.08), (0.04, 0.05, 0.1))   # magazine
    p.ball("flash", "flash", (0.03, 0.76, 1.17), 0.07, (1, 1.6, 1), 1)
    mesh = p.build("ghoul_mesh")
    skin(mesh, rg)
    hidden = {"flash": {"s": 0.0}}
    held = {"arm_L": [(0.0, {})], "arm_R": [(0.0, {})]}
    clip(rg, "idle", 2.4, {
        "chest": [(0.0, {}), (0.5, {"r": (0.05, 0, 0.05)})],
        "head": [(0.0, {}), (0.5, {"r": (0.1, 0, -0.2)})],
    }, hidden, loop=True)
    g = gait(0.45, 0.0, 0.025)
    g.update(held)
    clip(rg, "walk", 1.0, g, hidden, loop=True)
    g = gait(0.8, 0.0, 0.05, 0.2)
    g.update(held)
    clip(rg, "run", 0.6, g, hidden, loop=True)
    clip(rg, "shoot", 1.2, {
        "chest": [(0.0, {}), (0.3, {"r": (0.05, 0, -0.05)}), (0.6, {})],
        "arm_R": [(0.0, {}), (0.4, {}), (0.45, {"r": (0.18, 0, 0)}), (0.6, {})],
        "arm_L": [(0.0, {}), (0.4, {}), (0.45, {"r": (0.18, 0, 0)}), (0.6, {})],
        "flash": [(0.0, {"s": 0.0}), (0.42, {"s": 0.0}), (0.44, {"s": 1.0}), (0.5, {"s": 1.0}),
                  (0.52, {"s": 0.0})],
    }, hidden)
    # Knees first, then face down, the rifle falling out of its arms' aim.
    clip(rg, "death", 1.0, {
        "hips": [(0.0, {}), (0.35, {"l": (0, -0.35, 0)})],
        "leg_L": [(0.0, {}), (0.35, {"r": (1.4, 0, 0)})],
        "leg_R": [(0.0, {}), (0.35, {"r": (1.3, 0, 0.1)})],
        "root": [(0.0, {}), (0.35, {}), (0.8, {"r": (-1.3, 0, 0), "l": (0, -0.1, 0)})],
        "chest": [(0.0, {}), (0.3, {"r": (0.4, 0, 0)})],
        "head": [(0.0, {}), (0.4, {"r": (0.4, 0, 0.3)})],
        "arm_L": [(0.0, {}), (0.5, {"r": (-0.9, 0, -0.3)})],
        "arm_R": [(0.0, {}), (0.5, {"r": (-1.1, 0, 0.3)})],
    }, hidden)
    export([rg, mesh], os.path.join(OUT, "ghoul.gltf"), True)
    put_aside([rg], 8.0)


# ---------------------------------------------------------------- shots and pickups

def rigid(name, make, x):
    p = Parts()
    make(p)
    obj = p.build(name)
    export([obj], os.path.join(OUT, f"{name}.gltf"), False)
    put_aside([obj], x)


def shots_and_pickups():
    def fireball(p):
        p.ball(None, "fire_outer", (0, 0, 0.15), 0.16, sub=2)
        p.ball(None, "fire_core", (0, 0.06, 0.15), 0.1, sub=1)

    def caco_ball(p):
        p.ball(None, "caco_fire", (0, 0, 0.2), 0.2, sub=2)
        p.ball(None, "caco_fire_core", (0, 0.08, 0.2), 0.12, sub=1)

    def bullet(p):
        p.ball(None, "bullet", (0, 0, 0.05), 0.04, (0.8, 3.0, 0.8), 1)

    def plasma_ball(p):
        p.ball(None, "green_glow", (0, 0, 0.4), 0.4, sub=3)
        p.ball(None, "green_core", (0, 0, 0.4), 0.26, sub=2)
        for i in range(8):
            a = i * math.pi / 4
            p.spike(None, "green_glow", (0, 0, 0.4),
                    (math.cos(a) * 0.7, math.sin(a) * 0.7, 0.4 + 0.3 * math.sin(2 * a)), 0.06)

    def bullets(p):
        p.box(None, "ammo_box", (0, 0, 0.1), (0.36, 0.22, 0.2))
        p.box(None, "brass", (0, 0, 0.21), (0.3, 0.16, 0.02))
        p.box(None, "polymer", (0, 0.111, 0.12), (0.22, 0.004, 0.06))

    def shells(p):
        p.box(None, "shell_red", (0, 0, 0.09), (0.34, 0.24, 0.18))
        for i in range(5):
            p.cyl(None, "shell_red", (-0.12 + i * 0.06, 0, 0.23), 0.022, 0.1, "Z", seg=8)
            p.cyl(None, "brass", (-0.12 + i * 0.06, 0, 0.18), 0.024, 0.02, "Z", seg=8)

    def cells(p):
        p.box(None, "gunmetal", (0, 0, 0.14), (0.3, 0.2, 0.28))
        p.box(None, "green_glow", (0, 0.101, 0.14), (0.2, 0.004, 0.18))
        p.box(None, "steel", (0, 0, 0.3), (0.12, 0.08, 0.04))

    def medkit(p):
        p.box(None, "white", (0, 0, 0.12), (0.42, 0.3, 0.24))
        p.box(None, "red", (0, 0.151, 0.13), (0.24, 0.004, 0.07))
        p.box(None, "red", (0, 0.151, 0.13), (0.07, 0.004, 0.18))
        p.box(None, "red", (0, 0, 0.241), (0.07, 0.2, 0.004))
        p.box(None, "red", (0, 0, 0.241), (0.2, 0.07, 0.004))

    def orb(p):
        p.ball(None, "blue_glow", (0, 0, 0.35), 0.13, sub=2)
        p.ball(None, "white", (0, 0, 0.35), 0.07, sub=1)

    for i, (name, make) in enumerate([
            ("fireball", fireball), ("caco_ball", caco_ball), ("bullet", bullet),
            ("plasma_ball", plasma_ball), ("ammo_bullets", bullets), ("ammo_shells", shells),
            ("ammo_cells", cells), ("medkit", medkit), ("health_orb", orb)]):
        rigid(name, make, 10.0 + i)


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = FPS
    bpy.context.preferences.edit.keyframe_new_interpolation_type = "LINEAR"
    os.makedirs(OUT, exist_ok=True)
    mats()
    fp_arms()
    held_guns()
    imp()
    pinky()
    gazer()
    ghoul()
    shots_and_pickups()
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("fps models written to", os.path.normpath(OUT))
