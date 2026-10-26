# Builds what a character of games/arpg wears and holds -- every base but the
# ring, one model each -- on the robot's own rig, and exports each one.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_arpg_gear.py"
#
# The same way as tools/make_robot_extras.py: imports mods/robot/robot.gltf so
# the bones are the ones the game poses, builds each piece around the robot at
# rest, every part bound whole to one bone, and exports
# games/arpg/mods/arpg_core/gear_<base>.gltf. arpg_game dresses a player in the
# models of what they have equipped (core.player_wear); the client draws them
# on their skeleton as worn pieces (draw_worn). The trim of every piece is the
# material tint_1, its own colour unless the item is unique or of a set, when
# the mod tints it gold or green.
#
# Shapes are in world space at rest, metres, Z up, the robot facing +Y. A
# weapon is held in the right fist at GRIP and points along +Y, as the robot's
# own sword does (its clips swing it so); a bow, an orb in the left fist; a
# shield on the left forearm. Leaves the work in assets/blender/arpg_gear.blend.
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "games", "arpg", "mods", "arpg_core")
GRIP_R = Vector((0.265, 0.0, 0.604))
GRIP_L = Vector((-0.265, 0.0, 0.604))

# name: (colour, metallic, roughness)
MATS = {
    "steel": ((0.72, 0.74, 0.78), 1.0, 0.32),
    "dark_steel": ((0.36, 0.38, 0.42), 1.0, 0.40),
    "iron": ((0.52, 0.52, 0.54), 0.9, 0.55),
    "bronze": ((0.70, 0.46, 0.22), 1.0, 0.40),
    "wood": ((0.45, 0.29, 0.15), 0.0, 0.80),
    "dark_wood": ((0.27, 0.17, 0.09), 0.0, 0.80),
    "leather": ((0.40, 0.24, 0.13), 0.0, 0.70),
    "dark_leather": ((0.22, 0.14, 0.09), 0.0, 0.75),
    "cloth": ((0.62, 0.56, 0.44), 0.0, 0.95),
    "red_cloth": ((0.48, 0.12, 0.12), 0.0, 0.95),
    "blue_cloth": ((0.16, 0.22, 0.42), 0.0, 0.95),
    "green_cloth": ((0.20, 0.30, 0.16), 0.0, 0.95),
    "chain": ((0.58, 0.60, 0.62), 0.9, 0.65),
    "bone": ((0.86, 0.82, 0.70), 0.0, 0.60),
    "string": ((0.85, 0.82, 0.74), 0.0, 0.90),
    "glow_blue": ((0.35, 0.65, 1.00), 0.0, 0.20),
    "glow_red": ((1.00, 0.30, 0.20), 0.0, 0.20),
    "glow_violet": ((0.70, 0.35, 1.00), 0.0, 0.20),
}
GLOW = {"glow_blue", "glow_red", "glow_violet"}


class Piece:
    """The parts of one model: each a mesh made in world space, bound to a bone."""

    def __init__(self, name, trim):
        self.name, self.trim, self.parts = name, trim, []

    def add(self, bone, bm_fn, mat):
        self.parts.append((bone, bm_fn, mat))

    # Shapes ---------------------------------------------------------------
    def box(self, bone, lo, hi, mat, mirror=False):
        self._box(bone, lo, hi, mat)
        if mirror:
            self._box(bone[:-2] + "_L", (-hi[0], lo[1], lo[2]), (-lo[0], hi[1], hi[2]), mat)

    def _box(self, bone, lo, hi, mat):
        def make(bm):
            ret = bmesh.ops.create_cube(bm, size=1.0)
            c = [(lo[i] + hi[i]) / 2 for i in range(3)]
            s = [hi[i] - lo[i] for i in range(3)]
            for v in ret["verts"]:
                v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
            return ret["verts"]
        self.add(bone, make, mat)

    def cyl(self, bone, a, b, r1, mat, r2=None, segs=10):
        """A cylinder (a cone when r2 differs) from point a to point b."""
        a, b = Vector(a), Vector(b)
        r2 = r1 if r2 is None else r2

        def make(bm):
            d = b - a
            ret = bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r1, radius2=r2, depth=d.length)
            rot = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_matrix().to_4x4()
            m = Matrix.Translation((a + b) / 2) @ rot
            for v in ret["verts"]:
                v.co = m @ v.co
            return ret["verts"]
        self.add(bone, make, mat)

    def ball(self, bone, c, r, mat, squash=(1, 1, 1)):
        c = Vector(c)

        def make(bm):
            ret = bmesh.ops.create_icosphere(bm, subdivisions=2, radius=r)
            for v in ret["verts"]:
                v.co = c + Vector((v.co.x * squash[0], v.co.y * squash[1], v.co.z * squash[2]))
            return ret["verts"]
        self.add(bone, make, mat)

    def blade(self, bone, y0, y1, width, thick, mat, z=0.0, x=GRIP_R.x, tip=0.12, base=GRIP_R):
        """A flat blade along +Y from y0 to y1 past the grip, wide in Z, its last `tip` a point."""
        def make(bm):
            ox, oy, oz = x, base.y, base.z + z
            w, t = width / 2, thick / 2
            body = y1 - tip * (y1 - y0)
            ring = []
            for yy, ww in ((y0, w), (body, w)):
                ring.append([bm.verts.new((ox + dx, oy + yy, oz + dz)) for dx, dz in
                             ((-t, 0), (0, ww), (t, 0), (0, -ww))])
            point = bm.verts.new((ox, oy + y1, oz))
            a, b = ring
            bm.faces.new(list(reversed(a)))
            for i in range(4):
                j = (i + 1) % 4
                bm.faces.new((a[i], a[j], b[j], b[i]))
                bm.faces.new((b[i], b[j], point))
            return a + b + [point]
        self.add(bone, make, mat)


# The weapons, about the right fist; y along the blade, z up across it.
def at(dy=0.0, dz=0.0, dx=0.0, g=GRIP_R):
    return (g.x + dx, g.y + dy, g.z + dz)


def sword(p, length, width, guard, pommel_mat="steel", blade_mat="steel", grip_mat="leather"):
    p.box("hand_R", at(-0.17, -0.025, -0.025), at(-0.13, 0.025, 0.025), pommel_mat)
    p.cyl("hand_R", at(-0.13), at(0.04), 0.018, grip_mat)
    p.box("hand_R", at(0.04, -guard, -0.022), at(0.075, guard, 0.022), "tint")
    p.blade("hand_R", 0.075, 0.075 + length, width, 0.016, blade_mat)
    p.box("hand_R", at(0.09, -0.006, -0.010), at(0.075 + length * 0.75, 0.006, 0.010), "dark_steel")


def axe(p, haft, head_h, double, haft_mat="wood", head_mat="steel"):
    p.cyl("hand_R", at(-0.15), at(haft), 0.02, haft_mat)
    y = haft - 0.08
    p.box("hand_R", at(y - 0.05, -0.03, -0.025), at(y + 0.06, 0.03, 0.025), "tint")
    for side in ((1,) if not double else (1, -1)):
        lo, hi = (0.03, 0.03 + head_h) if side > 0 else (-0.03 - head_h, -0.03)
        p.box("hand_R", at(y - 0.04, lo, -0.008), at(y + 0.05, hi, 0.008), head_mat)
        edge = hi if side > 0 else lo
        p.box("hand_R", at(y - 0.08, edge - 0.025 * side - 0.0125, -0.006),
              at(y + 0.09, edge - 0.025 * side + 0.0125, 0.006), head_mat)


def bow(p, length, bend, mat, recurve=False):
    """Limbs along Y about the left fist, bulging to -Z, the string along +Z (towards the archer when drawn)."""
    g = GRIP_L
    p.box("hand_L", at(-0.06, -0.025, -0.022, g), at(0.06, 0.025, 0.022, g), "leather")
    half = length / 2
    steps = 5
    pts = []
    for i in range(-steps, steps + 1):
        t = i / steps
        pts.append((t * half, -bend * (1 - t * t)))
    for (y0, z0), (y1, z1) in zip(pts, pts[1:]):
        th = 0.018 + 0.012 * (1 - abs((y0 + y1) / 2) / half)
        p.cyl("hand_L", at(y0, z0, 0, g), at(y1, z1, 0, g), th, mat, segs=6)
    tips = []
    for s in (1, -1):
        ty, tz = s * half, 0.0
        if recurve:
            p.cyl("hand_L", at(ty, tz, 0, g), at(ty + s * 0.05, tz - 0.05, 0, g), 0.012, mat, segs=6)
        p.box("hand_L", at(ty - 0.02, tz - 0.015, -0.012, g), at(ty + 0.02, tz + 0.015, 0.012, g), "tint")
        tips.append(at(ty, tz + 0.01, 0, g))
    p.cyl("hand_L", tips[0], tips[1], 0.003, "string", segs=4)


def staff(p, top_mat, top_r, battle):
    p.cyl("hand_R", at(-0.55), at(0.80), 0.024, "dark_wood" if battle else "wood")
    p.cyl("hand_R", at(0.70), at(0.76), 0.032, "tint")
    p.ball("hand_R", at(0.80 + top_r * 0.8), top_r, top_mat)
    if battle:
        for k in range(3):
            a = k * 2 * math.pi / 3
            p.cyl("hand_R", at(0.74, 0.0, 0.0), at(0.80 + top_r * 1.6, math.sin(a) * top_r * 1.2, math.cos(a) * top_r * 1.2),
                  0.008, "dark_steel", segs=5)
    p.cyl("hand_R", at(-0.58), at(-0.55), 0.03, "tint")


def wand(p, mat, gem, bone_wand):
    p.cyl("hand_R", at(-0.08), at(0.30), 0.014, mat, r2=0.009)
    p.cyl("hand_R", at(-0.02), at(0.02), 0.02, "tint")
    if bone_wand:
        p.box("hand_R", at(0.30, -0.04, -0.035), at(0.36, 0.04, 0.035), "bone")
        p.box("hand_R", at(0.31, -0.02, -0.04), at(0.34, 0.02, -0.03), gem)
    else:
        p.ball("hand_R", at(0.33), 0.03, gem)


def shield(p, kind):
    # On the outside of the left forearm, its face to -X.
    x0, x1 = -0.375, -0.350
    cz, cy = 0.84, 0.0
    if kind == "buckler":
        p.cyl("forearm_L", (x1, cy, cz), (x0, cy, cz), 0.17, "wood", segs=14)
        p.cyl("forearm_L", (x1 + 0.002, cy, cz), (x0 - 0.002, cy, cz), 0.175, "iron", r2=0.175, segs=14)
        p.ball("forearm_L", (x0 - 0.01, cy, cz), 0.045, "tint", squash=(0.6, 1, 1))
        # the rim as a ring of short bars
    elif kind == "kite":
        p.box("forearm_L", (x0, -0.16, 0.66), (x1, 0.16, 1.12), "steel")
        p.box("forearm_L", (x0 - 0.005, -0.13, 0.55), (x1, 0.13, 0.66), "steel")
        p.box("forearm_L", (x0 - 0.005, -0.06, 0.48), (x1, 0.06, 0.55), "steel")
        p.box("forearm_L", (x0 - 0.01, -0.02, 0.50), (x0, 0.02, 1.10), "tint")
        p.box("forearm_L", (x0 - 0.01, -0.14, 0.92), (x0, 0.14, 0.96), "tint")
    else:   # tower
        p.box("forearm_L", (x0 - 0.01, -0.22, 0.40), (x1, 0.22, 1.30), "dark_steel")
        p.box("forearm_L", (x0 - 0.02, -0.23, 1.26), (x0 - 0.01, 0.23, 1.31), "tint")
        p.box("forearm_L", (x0 - 0.02, -0.23, 0.39), (x0 - 0.01, 0.23, 0.44), "tint")
        p.box("forearm_L", (x0 - 0.02, -0.23, 0.44), (x0 - 0.01, -0.19, 1.26), "tint")
        p.box("forearm_L", (x0 - 0.02, 0.19, 0.44), (x0 - 0.01, 0.23, 1.26), "tint")
        p.ball("forearm_L", (x0 - 0.03, 0.0, 0.85), 0.05, "steel", squash=(0.6, 1, 1))
    # the straps the forearm goes through
    p.box("forearm_L", (x1, -0.09, 0.78), (-0.33, 0.09, 0.80), "leather")
    p.box("forearm_L", (x1, -0.09, 0.90), (-0.33, 0.09, 0.92), "leather")


def orb(p, mat, eye):
    c = at(0.06, -0.06, 0.0, GRIP_L)
    p.ball("hand_L", c, 0.075, mat)
    p.cyl("hand_L", at(0.0, -0.02, 0, GRIP_L), at(0.03, -0.02, 0, GRIP_L), 0.05, "tint")
    if eye:
        p.ball("hand_L", (c[0], c[1] + 0.06, c[2]), 0.035, "glow_red", squash=(1, 0.5, 1))
        p.cyl("hand_L", (c[0], c[1] + 0.035, c[2]), (c[0], c[1] + 0.05, c[2]), 0.05, "tint", segs=12)


# The head is x -0.212..0.212, y -0.146..0.160 (the face at +y), z 1.36..1.638.
def helm(p, kind):
    e = 0.025
    X, Y0, Y1, Z0, Z1 = 0.212 + e, -0.146 - e, 0.160 + e, 1.36, 1.638 + e
    if kind == "cap":
        p.box("head", (-X, Y0, Z1 - 0.04), (X, Y1, Z1), "leather")
        p.box("head", (-X, Y0, Z1 - 0.10), (X, Y0 + 0.02, Z1), "leather")
        p.box("head", (-X, Y0, Z1 - 0.10), (-X + 0.02, Y1, Z1), "leather")
        p.box("head", (X - 0.02, Y0, Z1 - 0.10), (X, Y1, Z1), "leather")
        p.box("head", (-X, Y1 - 0.02, Z1 - 0.07), (X, Y1, Z1), "tint")
    elif kind == "helm":
        p.box("head", (-X, Y0, Z1 - 0.05), (X, Y1, Z1 + 0.02), "iron")
        p.box("head", (-X, Y0, Z0 + 0.06), (X, Y0 + 0.02, Z1), "iron")
        p.box("head", (-X, Y0, Z0 + 0.06), (-X + 0.02, Y1, Z1), "iron")
        p.box("head", (X - 0.02, Y0, Z0 + 0.06), (X, Y1, Z1), "iron")
        p.box("head", (-0.02, Y1 - 0.01, Z0 + 0.12), (0.02, Y1 + 0.01, Z1), "tint")   # nose guard
        p.box("head", (-0.02, Y0, Z1 + 0.02), (0.02, Y1, Z1 + 0.06), "tint")          # ridge
    elif kind == "great":
        p.box("head", (-X, Y0, Z1 - 0.03), (X, Y1, Z1 + 0.03), "steel")
        p.box("head", (-X, Y0, Z0 - 0.04), (X, Y0 + 0.02, Z1), "steel")
        p.box("head", (-X, Y0, Z0 - 0.04), (-X + 0.02, Y1, Z1), "steel")
        p.box("head", (X - 0.02, Y0, Z0 - 0.04), (X, Y1, Z1), "steel")
        p.box("head", (-X, Y1 - 0.02, Z0 - 0.04), (X, Y1, 1.50), "steel")          # below the slit
        p.box("head", (-X, Y1 - 0.02, 1.535), (X, Y1, Z1), "steel")                # above it
        p.box("head", (-X - 0.005, Y1 - 0.025, 1.535), (X + 0.005, Y1 + 0.005, 1.56), "tint")
        p.box("head", (-0.015, Y0, Z1 + 0.03), (0.015, Y1, Z1 + 0.09), "tint")      # crest
        for k in range(4):
            yy = Y1 + 0.001
            p.box("head", (-0.12 + k * 0.08, yy - 0.01, 1.40), (-0.10 + k * 0.08, yy, 1.47), "dark_steel")
    else:   # hood
        p.box("head", (-X, Y0, Z1 - 0.04), (X, Y1 - 0.02, Z1), "green_cloth")
        p.box("head", (-X, Y0, Z0 - 0.08), (X, Y0 + 0.025, Z1), "green_cloth")
        p.box("head", (-X, Y0, Z0 - 0.05), (-X + 0.025, Y1, Z1), "green_cloth")
        p.box("head", (X - 0.025, Y0, Z0 - 0.05), (X, Y1, Z1), "green_cloth")
        p.cyl("head", (0, Y0 + 0.05, Z1 - 0.02), (0, Y0 - 0.10, Z1 - 0.20), 0.07, "green_cloth", r2=0.01, segs=6)
        p.box("head", (-X - 0.005, Y1 - 0.03, Z1 - 0.03), (X + 0.005, Y1, Z1 + 0.005), "tint")
        p.box("chest", (-0.20, -0.17, 1.24), (0.20, 0.20, 1.30), "green_cloth")   # the mantle


# The chest is x -0.225..0.225, y -0.146..0.186, z 0.982..1.326; the hips below.
def armor(p, kind):
    e = 0.02
    X, Y0, Y1 = 0.225 + e, -0.146 - e, 0.186 + e
    mat = {"quilted": "cloth", "leather": "leather", "chain": "chain", "plate": "steel"}[kind]
    p.box("chest", (-X, Y0, 0.98), (X, Y1, 1.30), mat)
    p.box("spine", (-X + 0.02, Y0 + 0.02, 0.86), (X - 0.02, Y1 - 0.02, 0.99), mat)
    if kind == "quilted":
        for z in (1.04, 1.12, 1.20):
            p.box("chest", (-X - 0.003, Y1 - 0.005, z), (X + 0.003, Y1 + 0.003, z + 0.012), "tint")
    elif kind == "leather":
        p.box("upper_arm_R", (0.17, -0.10, 1.20), (0.36, 0.10, 1.34), "dark_leather", mirror=True)
        p.box("chest", (-0.03, Y1 - 0.005, 0.98), (0.03, Y1 + 0.005, 1.30), "tint")
        p.box("chest", (-X - 0.003, Y0 - 0.003, 1.22), (X + 0.003, Y1 + 0.003, 1.25), "tint")
    elif kind == "chain":
        p.box("upper_arm_R", (0.17, -0.10, 1.10), (0.36, 0.10, 1.33), "chain", mirror=True)
        p.box("hips", (-0.20, -0.15, 0.70), (0.20, 0.15, 0.86), "chain")
        p.box("chest", (-X - 0.003, Y1 - 0.003, 1.27), (X + 0.003, Y1 + 0.005, 1.30), "tint")
    else:
        p.box("chest", (-X + 0.04, Y1, 1.02), (X - 0.04, Y1 + 0.025, 1.26), "steel")
        p.box("upper_arm_R", (0.16, -0.12, 1.22), (0.39, 0.12, 1.37), "steel", mirror=True)
        p.box("upper_arm_R", (0.18, -0.11, 1.14), (0.38, 0.11, 1.22), "steel", mirror=True)
        p.box("hips", (-0.21, -0.16, 0.70), (0.21, 0.16, 0.86), "dark_steel")
        p.box("chest", (-0.02, Y1 + 0.02, 1.02), (0.02, Y1 + 0.03, 1.26), "tint")
        p.box("upper_arm_R", (0.16, -0.125, 1.36), (0.39, 0.125, 1.38), "tint", mirror=True)


# A hand is x 0.231..0.298 (right), y -0.050..0.082, z 0.521..0.667.
def gloves(p, kind):
    mat = {"leather": "leather", "chain": "chain", "gauntlets": "steel"}[kind]
    p.box("hand_R", (0.222, -0.06, 0.53), (0.307, 0.092, 0.676), mat, mirror=True)
    if kind != "leather":
        p.box("forearm_R", (0.175, -0.09, 0.67), (0.355, 0.09, 0.80 if kind == "chain" else 0.86), mat, mirror=True)
    p.box("forearm_R", (0.172, -0.093, 0.665), (0.358, 0.093, 0.69), "tint", mirror=True)
    if kind == "gauntlets":
        p.box("hand_R", (0.30, -0.04, 0.55), (0.315, 0.07, 0.66), "dark_steel", mirror=True)


# A foot is x 0.044..0.195 (right), y -0.093..0.192, z 0..0.139; the shin above.
def boots(p, kind):
    mat = {"boots": "leather", "chain": "chain", "greaves": "steel"}[kind]
    top = {"boots": 0.30, "chain": 0.38, "greaves": 0.46}[kind]
    p.box("foot_R", (0.034, -0.103, -0.005), (0.205, 0.205, 0.15), "dark_leather" if kind != "greaves" else "steel",
          mirror=True)
    p.box("shin_R", (0.042, -0.083, 0.14), (0.197, 0.10, top), mat, mirror=True)
    p.box("shin_R", (0.038, -0.088, top - 0.03), (0.201, 0.105, top), "tint", mirror=True)
    if kind == "greaves":
        p.box("shin_R", (0.09, 0.10, top - 0.10), (0.15, 0.12, top + 0.02), "tint", mirror=True)


# The hips are x -0.179..0.179, y -0.126..0.126, z 0.663..0.822.
def belt(p, kind):
    X, Y = 0.19, 0.137
    if kind == "sash":
        p.box("hips", (-X, -Y, 0.76), (X, Y, 0.81), "red_cloth")
        p.box("hips", (0.10, Y - 0.01, 0.62), (0.15, Y + 0.01, 0.78), "red_cloth")
        p.box("hips", (0.09, Y, 0.77), (0.16, Y + 0.02, 0.80), "tint")
    elif kind == "belt":
        p.box("hips", (-X, -Y, 0.77), (X, Y, 0.81), "leather")
        p.box("hips", (-0.035, Y, 0.765), (0.035, Y + 0.015, 0.815), "tint")
        p.box("hips", (0.14, -0.02, 0.70), (0.20, 0.06, 0.78), "dark_leather")   # a pouch
    else:
        p.box("hips", (-X, -Y, 0.75), (X, Y, 0.82), "dark_leather")
        p.box("hips", (-0.05, Y, 0.745), (0.05, Y + 0.02, 0.825), "tint")
        for x in (-0.15, 0.15):
            p.box("hips", (x - 0.03, -Y - 0.005, 0.755), (x + 0.03, Y + 0.005, 0.815), "iron")


def amulet(p):
    pts = [(-0.10, 0.17, 1.30), (-0.08, 0.20, 1.22), (0.0, 0.215, 1.16), (0.08, 0.20, 1.22), (0.10, 0.17, 1.30)]
    for a, b in zip(pts, pts[1:]):
        p.cyl("chest", a, b, 0.006, "bronze", segs=5)
    p.box("chest", (-0.035, 0.205, 1.08), (0.035, 0.23, 1.16), "tint")
    p.ball("chest", (0.0, 0.235, 1.12), 0.02, "glow_blue")


# Every base of arpg_game's D.BASES but the ring, by its file name: what it is
# made of and the colour of its trim.
GEAR = {
    "short_sword": (lambda p: sword(p, 0.42, 0.055, 0.07), (0.55, 0.36, 0.18)),
    "long_sword": (lambda p: sword(p, 0.62, 0.06, 0.09), (0.70, 0.55, 0.30)),
    "broad_sword": (lambda p: sword(p, 0.68, 0.085, 0.11), (0.62, 0.62, 0.66)),
    "war_sword": (lambda p: sword(p, 0.86, 0.075, 0.14, blade_mat="steel", pommel_mat="dark_steel"), (0.85, 0.65, 0.20)),
    "hand_axe": (lambda p: axe(p, 0.38, 0.10, False), (0.40, 0.40, 0.42)),
    "battle_axe": (lambda p: axe(p, 0.58, 0.13, True), (0.55, 0.36, 0.18)),
    "great_axe": (lambda p: axe(p, 0.85, 0.17, True, haft_mat="dark_wood", head_mat="dark_steel"), (0.75, 0.20, 0.15)),
    "club": (lambda p: (p.cyl("hand_R", at(-0.13), at(0.42), 0.022, "wood", r2=0.05),
                        p.cyl("hand_R", at(-0.13), at(-0.08), 0.026, "tint")), (0.40, 0.24, 0.13)),
    "war_hammer": (lambda p: (p.cyl("hand_R", at(-0.15), at(0.55), 0.02, "dark_wood"),
                              p.box("hand_R", at(0.48, -0.10, -0.045), at(0.60, 0.10, 0.045), "dark_steel"),
                              p.box("hand_R", at(0.47, -0.105, -0.05), at(0.49, 0.105, 0.05), "tint"),
                              p.box("hand_R", at(0.59, -0.105, -0.05), at(0.61, 0.105, 0.05), "tint")), (0.80, 0.62, 0.22)),
    "dagger": (lambda p: (p.cyl("hand_R", at(-0.09), at(0.02), 0.016, "leather"),
                          p.box("hand_R", at(0.02, -0.04, -0.015), at(0.04, 0.04, 0.015), "tint"),
                          p.blade("hand_R", 0.04, 0.30, 0.04, 0.012, "steel")), (0.60, 0.60, 0.62)),
    "kris": (lambda p: (p.cyl("hand_R", at(-0.09), at(0.02), 0.016, "dark_wood"),
                        p.box("hand_R", at(0.02, -0.05, -0.015), at(0.045, 0.05, 0.015), "tint"),
                        p.blade("hand_R", 0.045, 0.14, 0.05, 0.012, "steel", z=0.012, tip=0.0),
                        p.blade("hand_R", 0.13, 0.24, 0.05, 0.012, "steel", z=-0.012, tip=0.0),
                        p.blade("hand_R", 0.23, 0.38, 0.05, 0.012, "steel", z=0.010)), (0.80, 0.62, 0.22)),
    "short_bow": (lambda p: bow(p, 0.95, 0.10, "wood"), (0.40, 0.24, 0.13)),
    "long_bow": (lambda p: bow(p, 1.35, 0.14, "wood"), (0.30, 0.45, 0.25)),
    "war_bow": (lambda p: bow(p, 1.45, 0.12, "dark_wood"), (0.62, 0.62, 0.66)),
    "hunters_bow": (lambda p: bow(p, 1.15, 0.10, "dark_wood", recurve=True), (0.30, 0.45, 0.25)),
    "staff": (lambda p: staff(p, "glow_blue", 0.045, False), (0.70, 0.55, 0.30)),
    "battle_staff": (lambda p: staff(p, "glow_violet", 0.06, True), (0.62, 0.62, 0.66)),
    "wand": (lambda p: wand(p, "wood", "glow_blue", False), (0.70, 0.55, 0.30)),
    "bone_wand": (lambda p: wand(p, "bone", "glow_red", True), (0.30, 0.30, 0.32)),
    "buckler": (lambda p: shield(p, "buckler"), (0.62, 0.62, 0.66)),
    "kite_shield": (lambda p: shield(p, "kite"), (0.55, 0.15, 0.15)),
    "tower_shield": (lambda p: shield(p, "tower"), (0.80, 0.62, 0.22)),
    "orb": (lambda p: orb(p, "glow_blue", False), (0.70, 0.55, 0.30)),
    "eye_orb": (lambda p: orb(p, "glow_violet", True), (0.30, 0.30, 0.32)),
    "cap": (lambda p: helm(p, "cap"), (0.55, 0.36, 0.18)),
    "helm": (lambda p: helm(p, "helm"), (0.62, 0.62, 0.66)),
    "great_helm": (lambda p: helm(p, "great"), (0.80, 0.62, 0.22)),
    "hood": (lambda p: helm(p, "hood"), (0.40, 0.24, 0.13)),
    "quilted_armor": (lambda p: armor(p, "quilted"), (0.45, 0.38, 0.28)),
    "leather_armor": (lambda p: armor(p, "leather"), (0.55, 0.45, 0.25)),
    "chain_mail": (lambda p: armor(p, "chain"), (0.55, 0.15, 0.15)),
    "plate_mail": (lambda p: armor(p, "plate"), (0.80, 0.62, 0.22)),
    "leather_gloves": (lambda p: gloves(p, "leather"), (0.22, 0.14, 0.09)),
    "chain_gloves": (lambda p: gloves(p, "chain"), (0.40, 0.24, 0.13)),
    "gauntlets": (lambda p: gloves(p, "gauntlets"), (0.80, 0.62, 0.22)),
    "boots": (lambda p: boots(p, "boots"), (0.55, 0.36, 0.18)),
    "chain_boots": (lambda p: boots(p, "chain"), (0.40, 0.24, 0.13)),
    "greaves": (lambda p: boots(p, "greaves"), (0.80, 0.62, 0.22)),
    "sash": (lambda p: belt(p, "sash"), (0.85, 0.65, 0.20)),
    "belt": (lambda p: belt(p, "belt"), (0.70, 0.55, 0.30)),
    "heavy_belt": (lambda p: belt(p, "heavy"), (0.62, 0.62, 0.66)),
    "amulet": (lambda p: amulet(p), (0.85, 0.65, 0.20)),
}


def material(name, rgb, metal, rough, glow=False):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Metallic"].default_value = metal
    p.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1)   # the viewport's, for a look in Blender
    if glow:
        p.inputs["Emission Color"].default_value = (*rgb, 1)
        p.inputs["Emission Strength"].default_value = 2.0
    return m


def build(rig, key, maker, trim):
    piece = Piece(key, trim)
    maker(piece)
    name = f"gear_{key}"
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    groups = {g.name: g.index for g in ob.vertex_groups}

    used = []
    bm = bmesh.new()
    dl = bm.verts.layers.deform.verify()
    for bone, make, mat in piece.parts:
        if mat not in used:
            used.append(mat)
        verts = make(bm)
        bm.verts.ensure_lookup_table()
        for v in verts:
            v[dl][groups[bone]] = 1.0
        for f in {f for v in verts for f in v.link_faces}:
            f.material_index = used.index(mat)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    for mat in used:
        if mat == "tint":
            me.materials.append(material("tint_1", trim, 0.6, 0.45))
        else:
            rgb, metal, rough = MATS[mat]
            me.materials.append(material(mat, rgb, metal, rough, mat in GLOW))
    bev = ob.modifiers.new("bevel", "BEVEL")
    bev.width, bev.segments, bev.limit_method = 0.004, 1, "ANGLE"
    # Shapes are in world space; the rig is scaled, so undo it.
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob


def main():
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, "mods", "robot", "robot.gltf"))
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    rig.data.pose_position = "REST"
    for o in list(bpy.data.objects):
        if o.type == "MESH":
            o.hide_set(True)

    for key, (maker, trim) in GEAR.items():
        ob = build(rig, key, maker, trim)
        for x in bpy.data.objects:
            x.select_set(False)
        ob.select_set(True)
        rig.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.export_scene.gltf(
            filepath=os.path.join(OUT, f"gear_{key}.gltf"), export_format="GLTF_SEPARATE",
            use_selection=True, export_animations=False, export_skins=True,
            export_apply=True, export_yup=True)
        ob.hide_set(True)
        print(f"GEAR {key}: {len(ob.data.polygons)} faces")

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "arpg_gear.blend"))


main()
