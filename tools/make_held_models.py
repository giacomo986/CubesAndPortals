# Builds the model of every item that had none, and the icon of every item
# that had none, and exports each next to the mod that registers the item.
#
#   blender --background --python tools/make_held_models.py
#
# An item is held as the model "<item>_held" (mods register it beside their
# items): drawn in the right hand's palm, rigid, scaled by the forearm like a
# held block (draw_held_block in src/client/main.cpp). The four tools of
# mods/tools are bones of the robot and the armour has its worn model, so
# neither is here.
#
# Each model is made about the grip: the origin is the middle of the fist, +Y is
# the way a tool points out of the hand (forward on the robot at rest, up the
# screen in first person) and +Z is the side a player sees in first person: a
# flat thing lies in XY, facing the eye, like a picture of itself.
# Sizes are metres of the 1.357 robot the tools were made for -- a held block is
# 0.16 across -- and the client scales them with the robot's forearm.
#
# The items with no picture get one: rendered here from their model, flat-lit,
# shrunk to 32x32 and outlined, like the rest of assets/textures. Leaves the
# work in ~/kart_assets/held_items.blend.
import math
import os
import random
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Euler, Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
BLEND = os.path.expanduser("~/kart_assets/held_items.blend")
TEXTURES = os.path.join(ROOT, "assets", "textures")
INFERNO_TEXTURES = os.path.join(ROOT, "games", "inferno", "textures")
LAB_TEXTURES = os.path.join(ROOT, "games", "lab", "textures")
BACKROOMS_TEXTURES = os.path.join(ROOT, "games", "backrooms", "textures")


def mod_dir(mod):
    if mod == "lab":
        return os.path.join(ROOT, "games", "lab", "mods", "lab_core")
    if mod == "inferno":
        return os.path.join(ROOT, "games", "inferno", "mods", "inferno")
    if mod == "backrooms":
        return os.path.join(ROOT, "games", "backrooms", "mods", "backrooms")
    return os.path.join(ROOT, "mods", mod)


# ---------------------------------------------------------------- materials

def material(name, rgb, metal=0.0, rough=0.8, glow=False, two_sided=False):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    if glow:
        bsdf.inputs["Emission Color"].default_value = (*rgb, 1.0)
        bsdf.inputs["Emission Strength"].default_value = 1.0
    m.use_backface_culling = not two_sided
    m.diffuse_color = (*rgb, 1.0)
    return m


# ---------------------------------------------------------------- parts
# Every part is a bmesh built about its own origin, then placed by `at` and
# `rot` (degrees). A part is (bmesh, material name).

def placed(bm, at=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    m = Matrix.Translation(at) @ Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()
    m = m @ Matrix.Diagonal((*scale, 1.0))
    bmesh.ops.transform(bm, matrix=m, verts=bm.verts)
    return bm


def box(size, bevel=0.0, **kw):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=size, verts=bm.verts)
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=bm.edges[:] + bm.verts[:], offset=bevel, segments=2,
                        affect="EDGES", profile=0.5)
    return placed(bm, **kw)


def taper_box(bottom, top, height, **kw):
    """A box whose top face is `top` (x, y) and bottom `bottom`: an ingot."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        w = top if v.co.z > 0 else bottom
        v.co.x *= w[0]
        v.co.y *= w[1]
        v.co.z *= height
    return placed(bm, **kw)


def cyl(r, length, segs=8, r2=None, cap=True, **kw):
    """Along +Y, from y = 0 to `length`; r2 the far end's radius (a cone)."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=cap, segments=segs, radius1=r,
                          radius2=r if r2 is None else r2, depth=length)
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(math.radians(-90), 3, "X"))
    bmesh.ops.translate(bm, verts=bm.verts, vec=(0, length / 2, 0))
    return placed(bm, **kw)


def ball(r, segs=12, rings=8, **kw):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=r)
    return placed(bm, **kw)


def lump(r, seed, rough=0.28, subdiv=1, **kw):
    """A rock: an icosphere every corner of which is pushed in or out."""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=r)
    rnd = random.Random(seed)
    for v in bm.verts:
        v.co *= 1.0 + rnd.uniform(-rough, rough)
    return placed(bm, **kw)


def ring(r, minor, start=0.0, sweep=360.0, segs=16, minor_segs=6, **kw):
    """A torus in the XZ plane about the origin, or an arc of one."""
    bm = bmesh.new()
    closed = sweep >= 360.0
    n = segs if closed else segs + 1
    rows = []
    for i in range(n):
        a = math.radians(start + sweep * i / segs)
        c = Vector((math.cos(a) * r, 0, math.sin(a) * r))
        out = c.normalized()
        rows.append([bm.verts.new(c + out * math.cos(b) * minor + Vector((0, 1, 0)) * math.sin(b) * minor)
                     for b in (2 * math.pi * k / minor_segs for k in range(minor_segs))])
    for i in range(n if closed else n - 1):
        a, b = rows[i], rows[(i + 1) % n]
        for k in range(minor_segs):
            bm.faces.new((a[k], a[(k + 1) % minor_segs], b[(k + 1) % minor_segs], b[k]))
    return placed(bm, **kw)


def slab(points, thick, **kw):
    """A flat shape: `points` (x, y) in the XY plane, `thick` along Z."""
    bm = bmesh.new()
    lo = [bm.verts.new((x, y, -thick / 2)) for x, y in points]
    hi = [bm.verts.new((x, y, thick / 2)) for x, y in points]
    bm.faces.new(list(reversed(lo)))
    bm.faces.new(hi)
    n = len(points)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return placed(bm, **kw)


def ellipse(rx, ry, n=12):
    return [(rx * math.cos(2 * math.pi * i / n), ry * math.sin(2 * math.pi * i / n)) for i in range(n)]


def tear(r, **kw):
    """A drop: a sphere drawn up into a point."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=10, radius=r)
    for v in bm.verts:
        if v.co.z > 0:
            t = v.co.z / r
            v.co.x *= 1.0 - t * 0.85
            v.co.y *= 1.0 - t * 0.85
            v.co.z *= 1.0 + t * 0.9
    return placed(bm, **kw)


def rod(p0, p1, r, segs=6):
    """A cylinder from p0 to p1."""
    a, b = Vector(p0), Vector(p1)
    d = b - a
    bm = cyl(r, d.length, segs=segs)
    q = d.normalized().to_track_quat("Y", "Z")
    return placed(bm, at=a, rot=[math.degrees(x) for x in q.to_euler()])


def bow(stage):
    """The bow in the YZ plane about its grip, the string toward the eye (+Z),
    turned so first person sees it at a slant as the classic block game's is held. `stage`
    0 at rest, 1..3 drawn further each time, with the arrow on the string."""
    pull = (0.0, 0.07, 0.12, 0.17)[stage]
    reach, bend = 0.3 - 0.02 * stage, 0.09 + 0.025 * stage
    arc = [(0.0, reach * t, bend * t * t) for t in (i / 5.0 - 1.0 for i in range(11))]
    parts = [(rod(arc[i], arc[i + 1], 0.012 - 0.004 * abs(i - 4.5) / 4.5), "dark_wood") for i in range(10)]
    parts.append((cyl(0.016, 0.09, segs=8, at=(0, -0.045, 0)), "leather"))
    nock = (0.0, 0.0, bend + pull)
    for tip in (arc[0], arc[-1]):
        parts.append((rod(tip, nock if pull > 0 else (0.0, 0.0, bend), 0.007, segs=4), "line"))
    if pull > 0:
        tail, head = Vector(nock), Vector(nock) + Vector((0, 0, -0.52))
        parts += arrow_parts(tail, head)
    return [(placed(placed(bm, rot=(0, 0, 18)), rot=(0, 30, 0)), m) for bm, m in parts]


def arrow_parts(tail, head):
    """Shaft, flint head and two crossed fins, from the fins' end to the point."""
    tail, head = Vector(tail), Vector(head)
    d = (head - tail).normalized()
    along = [math.degrees(x) for x in d.to_track_quat("Y", "Z").to_euler()]
    parts = [(rod(tail, head - d * 0.045, 0.006), "wood"),
             (placed(cyl(0.018, 0.05, segs=4, r2=0.0), at=head - d * 0.05, rot=along), "flint")]
    for a in (0, 90):
        fin = placed(slab([(0, 0), (0.025, 0.02), (0.025, 0.09), (0, 0.1), (-0.025, 0.09), (-0.025, 0.02)],
                          0.003), rot=(0, a, 0))
        parts.append((placed(fin, at=tail + d * 0.01, rot=along), "white"))
    return parts


def build(name, parts, mats):
    """One object of all the parts, each face wearing its part's material."""
    whole = bmesh.new()
    order = []
    for bm, mname in parts:
        if mname not in order:
            order.append(mname)
        mi = order.index(mname)
        me = bpy.data.meshes.new("tmp")
        bm.to_mesh(me)
        bm.free()
        for p in me.polygons:
            p.material_index = mi
            p.use_smooth = False
        whole.from_mesh(me)
        bpy.data.meshes.remove(me)
    me = bpy.data.meshes.new(name)
    whole.to_mesh(me)
    whole.free()
    for mname in order:
        me.materials.append(mats[mname])
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


# ---------------------------------------------------------------- the items

bpy.ops.wm.read_homefile(use_empty=True)

M = {
    "wood": material("Wood", (0.72, 0.55, 0.35)),
    "dark_wood": material("DarkWood", (0.45, 0.30, 0.18)),
    "iron": material("Iron", (0.82, 0.83, 0.86), 1.0, 0.35),
    "dark_iron": material("DarkIron", (0.24, 0.25, 0.28), 1.0, 0.45),
    "stone": material("Stone", (0.60, 0.61, 0.64), 0.0, 0.9),
    "gold": material("Gold", (0.98, 0.80, 0.25), 1.0, 0.3),
    "dark_gold": material("DarkGold", (0.85, 0.58, 0.12), 1.0, 0.35),
    "copper": material("Copper", (0.92, 0.52, 0.32), 1.0, 0.35),
    "raw_meat": material("RawMeat", (0.72, 0.16, 0.16)),
    "fat": material("Fat", (0.93, 0.86, 0.86)),
    "cooked_meat": material("CookedMeat", (0.45, 0.26, 0.16)),
    "cooked_fat": material("CookedFat", (0.78, 0.58, 0.42)),
    "pork": material("Pork", (0.93, 0.60, 0.62)),
    "cooked_pork": material("CookedPork", (0.72, 0.47, 0.33)),
    "flesh": material("Flesh", (0.70, 0.33, 0.30)),
    "shell": material("Shell", (0.93, 0.89, 0.76)),
    "white": material("White", (0.95, 0.95, 0.93)),
    "yolk": material("Yolk", (0.98, 0.75, 0.15)),
    "leather": material("Leather", (0.55, 0.30, 0.16)),
    "seed": material("Seed", (0.36, 0.46, 0.18)),
    "grip": material("Grip", (0.20, 0.22, 0.28)),
    "wool": material("Wool", (0.92, 0.92, 0.90)),
    "cover": material("Cover", (0.50, 0.30, 0.15)),
    "paper": material("Paper", (0.94, 0.93, 0.88)),
    "crust": material("Crust", (0.82, 0.56, 0.26)),
    "dark_crust": material("DarkCrust", (0.60, 0.38, 0.15)),
    "straw": material("Straw", (0.86, 0.72, 0.35)),
    "grain": material("Grain", (0.93, 0.78, 0.30)),
    "bone": material("Bone", (0.92, 0.90, 0.80)),
    "bucket": material("Bucket", (0.80, 0.82, 0.86), 1.0, 0.35, two_sided=True),
    "water": material("Water", (0.25, 0.45, 0.85), 0.0, 0.1),
    "coal": material("Coal", (0.22, 0.22, 0.24), 0.0, 0.6),
    "charcoal": material("Charcoal", (0.26, 0.19, 0.14), 0.0, 0.8),
    "raw_copper": material("RawCopper", (0.88, 0.50, 0.32)),
    "verdigris": material("Verdigris", (0.35, 0.70, 0.60)),
    "raw_iron": material("RawIron", (0.82, 0.70, 0.62)),
    "raw_iron_dark": material("RawIronDark", (0.60, 0.48, 0.40)),
    "raw_gold": material("RawGold", (0.98, 0.82, 0.20)),
    "lapis": material("Lapis", (0.15, 0.30, 0.80), 0.0, 0.4),
    "fleck": material("Fleck", (0.95, 0.80, 0.30)),
    "cinnabar": material("Cinnabar", (0.85, 0.08, 0.06), glow=True),
    "diamond": material("Diamond", (0.40, 0.90, 0.90), 0.0, 0.05),
    "emerald": material("Emerald", (0.20, 0.80, 0.40), 0.0, 0.05),
    "red": material("Red", (0.75, 0.15, 0.15)),
    "green": material("Green", (0.20, 0.65, 0.25)),
    "blue": material("Blue", (0.20, 0.35, 0.85)),
    "line": material("Line", (0.70, 0.70, 0.72)),
    "fish": material("Fish", (0.45, 0.60, 0.80)),
    "fin": material("Fin", (0.30, 0.45, 0.65)),
    "eye": material("Eye", (0.08, 0.08, 0.10)),
    "cinder": material("Cinder", (1.00, 0.78, 0.22), glow=True),
    "cinder_band": material("CinderBand", (0.95, 0.48, 0.10), glow=True),
    "torch_lens": material("TorchLens", (1.00, 0.95, 0.70), glow=True),
    "frost": material("Frost", (0.65, 0.88, 1.00), glow=True),
    "tear": material("Tear", (0.70, 0.86, 0.93), 0.0, 0.1),
    "flint": material("Flint", (0.30, 0.30, 0.33), 0.0, 0.5),
    "pack": material("Pack", (0.20, 0.21, 0.24), 0.3, 0.5),
    "pack_accent": material("PackAccent", (0.92, 0.42, 0.12)),
    "nozzle": material("Nozzle", (0.10, 0.10, 0.12), 0.6, 0.4),
    "dark_leather": material("DarkLeather", (0.36, 0.19, 0.10)),
    "hopper": material("Hopper", (0.30, 0.30, 0.33), 0.6, 0.45),
    "hopper_rim": material("HopperRim", (0.45, 0.45, 0.48), 0.6, 0.45),
    "hopper_in": material("HopperIn", (0.10, 0.10, 0.11), 0.3, 0.7),
    "belt_frame": material("BeltFrame", (0.32, 0.33, 0.36), 0.7, 0.45),
    "belt": material("Belt", (0.09, 0.09, 0.10), 0.0, 0.9),
    "belt_rib": material("BeltRib", (0.17, 0.17, 0.19), 0.0, 0.9),
    "belt_paint": material("BeltPaint", (0.95, 0.72, 0.12), 0.0, 0.6),
    # A thing's own colours (core.recipe_dye): the client draws a stack's dyes
    # on materials of these names, and these where it has none.
    "tint_1": material("tint_1", (0.55, 0.30, 0.16)),
    "tint_2": material("tint_2", (0.36, 0.19, 0.10)),
    "tint_3": material("tint_3", (0.24, 0.13, 0.06)),
    # The arm pack's, in its own colours: Blender's copies of a tint are
    # still the tint to the client ("tint_1.arm" is tint_1).
    "arm_1": material("tint_1.arm", (0.55, 0.58, 0.62), 0.6, 0.35),
    "arm_2": material("tint_2.arm", (0.12, 0.13, 0.16), 0.2, 0.5),
    "arm_3": material("tint_3.arm", (1.00, 0.55, 0.05), 0.0, 0.4),
}


def jetpack():
    """The flight pack as the robot wears it, flames out: a box, its panel, two nozzles."""
    return [(box((0.15, 0.18, 0.07), bevel=0.012, at=(0, 0.1, 0)), "pack"),
            (box((0.1, 0.022, 0.006), at=(0, 0.15, 0.036)), "pack_accent")] + \
           [(cyl(0.028, 0.045, segs=10, r2=0.018, at=(x, -0.035, 0)), "nozzle") for x in (-0.042, 0.042)]


def armpack():
    """The arm pack folded small: the metal body with its vents and lamp, the
    lid, and the two arms up from the lid's corners to a claw each.
    Body, arms and lights are its three colours (tint_1..3)."""
    parts = [(box((0.15, 0.16, 0.08), bevel=0.01, at=(0, 0.085, 0)), "arm_1"),
             (box((0.14, 0.014, 0.07), at=(0, 0.172, 0)), "arm_2"),
             (ball(0.008, segs=8, rings=6, at=(0.05, 0.14, 0.04)), "arm_3")] + \
            [(box((0.09, 0.008, 0.006), at=(0, 0.04 + 0.022 * i, 0.041)), "arm_2") for i in range(3)]
    for s in (-1, 1):
        base, elbow, wrist = (s * 0.05, 0.18, 0), (s * 0.075, 0.26, -0.01), (s * 0.11, 0.30, 0.01)
        parts += [(cyl(0.018, 0.012, segs=10, at=base), "arm_2"),
                  (rod(base, elbow, 0.009), "arm_1"),
                  (ball(0.013, segs=8, rings=6, at=elbow), "dark_iron"),
                  (rod(elbow, wrist, 0.008), "arm_1"),
                  (box((0.022, 0.02, 0.022), at=wrist), "arm_2")]
        for z in (-1, 1):
            parts.append((box((0.012, 0.03, 0.007), at=(wrist[0] + s * 0.008, wrist[1] + 0.022, wrist[2] + z * 0.008),
                              rot=(z * 15, 0, 0)), "arm_3"))
    return parts


def backpack():
    """A leather pack: its body, a darker flap with two buckles, a front pocket, a top loop.
    Body, flap and loop are its three colours (tint_1..3)."""
    return [(box((0.15, 0.17, 0.08), bevel=0.02, at=(0, 0.09, 0)), "tint_1"),
            (box((0.156, 0.05, 0.088), bevel=0.012, at=(0, 0.16, 0)), "tint_2"),
            (box((0.13, 0.06, 0.006), at=(0, 0.125, 0.043)), "tint_2"),
            (box((0.09, 0.06, 0.02), bevel=0.008, at=(0, 0.04, 0.045)), "tint_1")] + \
           [(box((0.016, 0.014, 0.006), at=(x, 0.1, 0.047)), "iron") for x in (-0.035, 0.035)] + \
           [(ring(0.022, 0.006, start=0, sweep=180, segs=10, at=(0, 0.178, 0), rot=(-90, 0, 0)), "tint_3")]


def meat(body, fat, stripe=True, seed=0):
    parts = [(box((0.15, 0.12, 0.045), bevel=0.02, at=(0, 0.04, 0), rot=(0, 0, 15)), body)]
    if stripe:
        parts.append((box((0.11, 0.018, 0.008), at=(0, 0.04, 0.024), rot=(0, 0, 40)), fat))
    else:
        parts.append((box((0.03, 0.03, 0.008), at=(0.035, 0.03, 0.024)), fat))
    return parts


def ingot(mat):
    return [(taper_box((0.16, 0.08), (0.11, 0.045), 0.045, at=(0, 0.04, 0), rot=(0, 0, 90)), mat)]


def raw(body, bits, seed):
    parts = [(lump(0.055, seed, at=(0, 0.04, 0), scale=(1, 1, 0.8)), body)]
    rnd = random.Random(seed + 100)
    for i in range(4):
        a = rnd.uniform(0, 2 * math.pi)
        parts.append((lump(0.016, seed + i, subdiv=1,
                           at=(math.cos(a) * 0.045, 0.04 + math.sin(a) * 0.035, rnd.uniform(-0.01, 0.03))), bits))
    return parts


def bucket(water):
    """Open end up the screen, held round its bottom."""
    parts = [
        (cyl(0.055, 0.12, segs=12, r2=0.07, cap=False, at=(0, -0.02, 0)), "bucket"),
        (cyl(0.055, 0.004, segs=12, at=(0, -0.02, 0)), "bucket"),
        (ring(0.07, 0.006, segs=16, at=(0, 0.1, 0)), "dark_iron"),
        (ring(0.07, 0.004, start=0, sweep=180, segs=10, at=(0, 0.1, 0), rot=(90, 0, 0)), "dark_iron"),
    ]
    if water:
        parts.append((cyl(0.066, 0.004, segs=12, at=(0, 0.085, 0)), "water"))
    return parts


def gem(r, top, bottom, segs, mat):
    """The table up the screen, the point down."""
    return [
        (cyl(r, top, segs=segs, r2=r * 0.65, at=(0, 0.05, 0)), mat),
        (cyl(r, bottom, segs=segs, r2=0.0, at=(0, 0.05, 0), rot=(0, 0, 180)), mat),
    ]


def hopper():
    """The placed hopper (mods/cinnabar/hopper.gltf) at a held block's 0.16: bowl
    up the screen, spout down, the same 16ths as ~/kart_assets/hopper.blend."""
    s = 0.16 / 16

    def part(lo, hi, mat):   # (x, depth, up) in 16ths of the cell
        size = tuple((h - l) * s for l, h in zip(lo, hi))
        mid = [((l + h) / 2 - 8) * s for l, h in zip(lo, hi)]
        return (box((size[0], size[2], size[1]), at=(mid[0], mid[2] + 0.04, mid[1])), mat)

    return [
        part((0, 0, 10), (16, 2, 16), "hopper_rim"), part((0, 14, 10), (16, 16, 16), "hopper_rim"),
        part((0, 2, 10), (2, 14, 16), "hopper_rim"), part((14, 2, 10), (16, 14, 16), "hopper_rim"),
        part((2, 2, 10), (14, 14, 11), "hopper_in"),
        *[part((i - 0.5, 2, 11), (i + 0.5, 14, 11.6), "hopper") for i in (5, 8, 11)],
        part((1, 1, 9), (15, 15, 10), "hopper"),
        part((3, 3, 6.5), (13, 13, 9), "hopper"),
        part((4, 4, 4), (12, 12, 6.5), "hopper"),
        part((6, 6, 0), (10, 10, 4), "hopper"),
    ]


def conveyor():
    """A straight belt with a drum at each end, as mods/conveyor/belt.gltf
    (tools/make_conveyor.py) at a held block's 0.16."""
    s = 0.16 / 16

    def part(lo, hi, mat):   # (x, depth, up) in 16ths of the cell
        size = tuple((h - l) * s for l, h in zip(lo, hi))
        mid = [((l + h) / 2 - 8) * s for l, h in zip(lo, hi)]
        return (box((size[0], size[2], size[1]), at=(mid[0], mid[2] + 0.04, mid[1])), mat)

    def drum(depth):
        return (cyl(2.5 * s, 13 * s, segs=12, at=(-6.5 * s, -5.5 * s + 0.04, (depth - 8) * s),
                    rot=(0, 0, -90)), "belt")

    return [
        part((0, 0, 0), (1.5, 16, 6), "belt_frame"), part((14.5, 0, 0), (16, 16, 6), "belt_frame"),
        part((1.5, 2.5, 1), (14.5, 13.5, 5), "belt"), drum(2.5), drum(13.5),
        *[part((1.5, z - 0.4, 5), (14.5, z + 0.4, 5.25), "belt_rib") for z in (5, 11)],
        part((6.5, 7, 5), (9.5, 9, 5.12), "belt_paint"),
    ]


def held_part(lo, hi, mat):
    """A box given in 16ths of a cell (x, depth, up), at a held block's 0.16."""
    s = 0.16 / 16
    size = tuple((h - l) * s for l, h in zip(lo, hi))
    mid = [((l + h) / 2 - 8) * s for l, h in zip(lo, hi)]
    return (box((size[0], size[2], size[1]), at=(mid[0], mid[2] + 0.04, mid[1])), mat)


def sorter():
    """mods/conveyor/sorter.gltf (tools/make_conveyor.py): a low block, four
    posts, a turntable and the three ways out painted on."""
    s = 0.16 / 16
    return [
        held_part((0, 0, 0), (16, 16, 5), "belt_frame"),
        held_part((1, 1, 5), (15, 15, 5.1), "belt"),
        *[held_part((x, z, 5), (x + 2, z + 2, 8), "belt_frame") for x in (0, 14) for z in (0, 14)],
        (cyl(3.2 * s, 0.4 * s, segs=16, at=(0, (5.1 - 8) * s + 0.04, 0)), "iron"),
        held_part((1.5, 7.3, 5.1), (5, 8.7, 5.25), "belt_paint"),
        held_part((11, 7.3, 5.1), (14.5, 8.7, 5.25), "belt_paint"),
        held_part((7.3, 1.5, 5.1), (8.7, 5, 5.25), "belt_paint"),
    ]


def extractor():
    """mods/conveyor/extractor.gltf: a belt coming out of a hood."""
    return [
        held_part((0, 0, 0), (1.5, 16, 6), "belt_frame"), held_part((14.5, 0, 0), (16, 16, 6), "belt_frame"),
        held_part((1.5, 0, 1), (14.5, 9, 5), "belt"),
        held_part((0, 9, 0), (16, 16, 12), "belt_frame"),
        held_part((0, 9, 12), (16, 16, 12.6), "belt_paint"),
        held_part((3, 8.9, 5), (13, 9, 10), "belt"),
    ]


def from_block(path):
    """A block's own model (a glTF in the unit cube, feet at 0, front +Z) as a
    held one: its meshes in their rest pose, joined, at a held block's 0.16,
    about the middle, its front towards the eye. What a block drawn as a model
    looks like in a hand, on the ground and in the bag, the same as placed."""
    def build_it(name):
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, path))
        new = [o for o in bpy.data.objects if o not in before]
        meshes = [o for o in new if o.type == "MESH"]
        for o in meshes:
            for mod in list(o.modifiers):
                o.modifiers.remove(mod)
            mw = o.matrix_world.copy()
            o.parent = None
            o.matrix_world = mw
        for o in new:
            if o.type != "MESH":
                bpy.data.objects.remove(o)
        bpy.ops.object.select_all(action="DESELECT")
        for o in meshes:
            o.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        if len(meshes) > 1:
            bpy.ops.object.join()
        obj = bpy.context.view_layer.objects.active
        obj.name = name
        obj.rotation_mode = "XYZ"   # an imported glTF turns by quaternion, and the icon turns by Euler
        obj.vertex_groups.clear()
        # Blender's x, -depth, up to the grip's x, up, depth, the middle of the
        # cell on the fist, at 0.16 a cell.
        m = (Matrix.Translation((0, 0.04, 0)) @ Matrix.Scale(0.16, 4) @
             Matrix.Rotation(math.radians(-90), 4, "X") @ Matrix.Translation((-0.5, 0.5, -0.5)))
        obj.data.transform(obj.matrix_world)
        obj.matrix_world = Matrix.Identity(4)
        obj.data.transform(m)
        # The workbench draws a material's viewport colour: the glTF's base colour.
        for mat in obj.data.materials:
            bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None) if mat.node_tree else None
            if bsdf is not None:
                mat.diffuse_color = tuple(bsdf.inputs["Base Color"].default_value)
        return obj
    return build_it


ITEMS = {
    "furnace:furnace": from_block("mods/furnace/furnace.gltf"),
    "chest:box": from_block("mods/chest/chest.gltf"),
    "craft:table": from_block("mods/craft/table.gltf"),
    "cinnabar:hopper": hopper(),
    "conveyor:belt": conveyor(),
    "conveyor:sorter": sorter(),
    "conveyor:extractor": extractor(),
    "jetpack:jetpack": jetpack(),
    "backpack:backpack": backpack(),
    "armpack:armpack": armpack(),
    "animals:beef": meat("raw_meat", "fat"),
    "animals:steak": meat("cooked_meat", "cooked_fat"),
    "animals:pork": meat("pork", "fat", stripe=False),
    "animals:cooked_pork": meat("cooked_pork", "cooked_fat", stripe=False),
    "mobs:flesh": [(lump(0.06, 7, rough=0.2, at=(0, 0.04, 0), scale=(1.2, 1, 0.6)), "flesh"),
                   (box((0.025, 0.025, 0.01), at=(0.03, 0.03, 0.03)), "fat"),
                   (box((0.02, 0.02, 0.01), at=(-0.03, 0.06, 0.028)), "fat")],
    "animals:egg": [(ball(0.045, at=(0, 0.05, 0), scale=(1, 1.3, 1)), "shell")],
    "animals:fried_egg": [(slab(ellipse(0.075, 0.065, 14), 0.01, at=(0, 0.04, 0)), "white"),
                          (ball(0.025, at=(0.01, 0.04, 0.005), scale=(1, 1, 0.5)), "yolk")],
    "animals:feather": [(cyl(0.004, 0.26, segs=5, at=(0, -0.04, 0)), "white"),
                        (slab([(0.0, 0.02), (0.04, 0.08), (0.05, 0.16), (0.03, 0.22), (0.0, 0.24),
                               (-0.028, 0.2), (-0.04, 0.12), (-0.022, 0.05)], 0.004,
                              at=(0, 0, 0)), "white")],
    "animals:leather": [(slab([(-0.07, -0.02), (-0.03, -0.035), (0.04, -0.03), (0.075, -0.01), (0.07, 0.09),
                               (0.035, 0.11), (-0.04, 0.105), (-0.075, 0.08)], 0.012, at=(0, 0.02, 0)), "leather")],
    "animals:seeds": [(ball(0.012, segs=6, rings=4, at=(x, 0.04 + y, z), scale=(1, 1.5, 0.8)), "seed")
                      for x, y, z in ((0, 0, 0), (0.025, 0.02, 0.005), (-0.02, 0.025, -0.004),
                                      (0.012, -0.022, 0.006), (-0.025, -0.012, 0.002))],
    "animals:shears": [
        (slab([(0.0, 0.0), (0.012, 0.0), (0.004, 0.16), (-0.004, 0.14)], 0.006, at=(0, 0, 0.004),
              rot=(0, 0, 12)), "iron"),
        (slab([(0.0, 0.0), (-0.012, 0.0), (-0.004, 0.16), (0.004, 0.14)], 0.006, at=(0, 0, -0.004),
              rot=(0, 0, -12)), "iron"),
        (ring(0.022, 0.006, at=(0.024, -0.03, 0), rot=(90, 0, 0), minor_segs=5), "grip"),
        (ring(0.022, 0.006, at=(-0.024, -0.03, 0), rot=(90, 0, 0), minor_segs=5), "grip"),
    ],
    "animals:wool": [(ball(r, at=(x, 0.04 + y, z)), "wool")
                     for r, x, y, z in ((0.045, 0, 0, 0), (0.035, 0.04, 0.01, 0.01), (0.035, -0.04, -0.01, 0.005),
                                        (0.032, 0.0, 0.04, 0.02), (0.032, 0.0, -0.035, 0.015),
                                        (0.03, 0.02, 0.0, 0.04))],
    "cane:book": [(box((0.13, 0.17, 0.008), at=(0, 0.04, 0.02)), "cover"),
                  (box((0.13, 0.17, 0.008), at=(0, 0.04, -0.02)), "cover"),
                  (box((0.012, 0.17, 0.048), at=(-0.065, 0.04, 0)), "cover"),
                  (box((0.12, 0.16, 0.034), at=(0.002, 0.04, 0)), "paper"),
                  (box((0.03, 0.03, 0.004), at=(0, 0.04, 0.025)), "gold")],
    "cane:paper": [(box((0.14, 0.17, 0.003), at=(0, 0.05, 0), rot=(10, 0, 0)), "paper")],
    "farm:bread": [(ball(0.05, at=(0, 0.04, 0), scale=(1, 2.0, 0.75)), "crust")] +
                  [(box((0.07, 0.01, 0.01), at=(0, 0.04 + d, 0.036), rot=(0, 0, 20)), "dark_crust")
                   for d in (-0.05, 0.0, 0.05)],
    "farm:wheat": [(cyl(0.004, 0.26, segs=4, at=(0, -0.06, 0), rot=(0, 0, a)), "straw") for a in (-12, -6, 0, 6, 12)] +
                  [(ball(0.012, segs=6, rings=4,
                         at=(-math.sin(math.radians(a)) * 0.23, -0.06 + math.cos(math.radians(a)) * 0.23, 0),
                         rot=(0, 0, a), scale=(1, 3, 1)), "grain") for a in (-12, -6, 0, 6, 12)],
    "furnace:copper_ingot": ingot("copper"),
    "furnace:gold_ingot": ingot("gold"),
    "furnace:iron_ingot": ingot("iron"),
    "mobs:bone": [(cyl(0.016, 0.2, segs=6, at=(0, -0.06, 0)), "bone")] +
                 [(ball(0.024, segs=8, rings=6, at=(x, y, 0)), "bone")
                  for y in (-0.065, 0.145) for x in (-0.016, 0.016)],
    "lakes:bucket": bucket(False),
    "lakes:bucket_water": bucket(True),
    "ore:coal": [(lump(0.055, 3, rough=0.3, at=(0, 0.04, 0), scale=(1.1, 1, 0.8)), "coal")],
    "furnace:charcoal": [(lump(0.05, 7, rough=0.35, at=(0, 0.04, 0), scale=(1.3, 0.8, 0.7)), "charcoal")],
    "ore:raw_copper": raw("raw_copper", "verdigris", 11),
    "ore:raw_iron": raw("raw_iron", "raw_iron_dark", 21),
    "ore:raw_gold": raw("raw_gold", "dark_gold", 31),
    "ore:lapis": raw("lapis", "fleck", 41),
    "ore:cinnabar": [(cyl(0.07, 0.03, segs=10, r2=0.015, at=(0, 0.04, -0.015), rot=(90, 0, 0)), "cinnabar")] +
                    [(box((0.012, 0.012, 0.012), at=(x, 0.04 + y, 0.0)), "cinnabar")
                     for x, y in ((0.06, 0.04), (-0.06, 0.03), (0.02, -0.06), (-0.04, -0.05))],
    "ore:diamond": gem(0.05, 0.02, 0.05, 8, "diamond"),
    "ore:emerald": gem(0.035, 0.05, 0.05, 6, "emerald"),
    "palette:brush": [(cyl(0.012, 0.3, segs=6, at=(0, -0.08, 0)), "wood"),
                      (cyl(0.014, 0.04, segs=8, at=(0, 0.22, 0)), "iron"),
                      (cyl(0.014, 0.05, segs=8, r2=0.004, at=(0, 0.26, 0)), "red"),
                      (box((0.01, 0.02, 0.01), at=(0.01, 0.275, 0.006)), "green"),
                      (box((0.01, 0.02, 0.01), at=(-0.01, 0.275, -0.004)), "blue")],
    # The line hangs from the tip back along the rod, down the screen beside it.
    "pets:rod": [(cyl(0.01, 0.66, segs=6, r2=0.005, at=(0, -0.06, 0)), "dark_wood"),
                 (cyl(0.022, 0.02, segs=10, at=(0.018, 0.02, 0), rot=(0, 0, 90)), "dark_iron"),
                 (cyl(0.002, 0.3, segs=4, at=(0.012, 0.6, 0), rot=(0, 0, 180)), "line"),
                 (ring(0.012, 0.002, start=180, sweep=180, segs=6, minor_segs=4, at=(0.024, 0.3, 0),
                       rot=(90, 0, 0)), "line")],
    "pets:fish": [(ball(0.04, at=(0, 0.04, 0), scale=(1.1, 2.2, 0.7)), "fish"),
                  (slab([(0, 0), (0.04, -0.05), (-0.04, -0.05)], 0.006, at=(0, -0.04, 0)), "fin"),
                  (slab([(0, 0), (0.03, 0.025), (0.03, -0.025)], 0.004, at=(0.04, 0.05, 0)), "fin"),
                  (ball(0.008, segs=6, rings=4, at=(0.012, 0.11, 0.024)), "eye")],
    "screwdriver:screwdriver": [(cyl(0.018, 0.18, segs=6, at=(0, -0.06, 0)), "red"),
                                (cyl(0.006, 0.16, segs=6, at=(0, 0.12, 0)), "iron"),
                                (box((0.014, 0.02, 0.003), at=(0, 0.285, 0)), "iron")],
    "timber:stick": [(cyl(0.014, 0.38, segs=6, at=(0, -0.08, 0)), "wood")],
    "archery:bow": bow(0),
    "archery:bow_pulling_0": bow(1),
    "archery:bow_pulling_1": bow(2),
    "archery:bow_pulling_2": bow(3),
    "archery:arrow": arrow_parts((0, -0.2, 0), (0, 0.32, 0)),
    "archery:string": [(ring(0.04, 0.0035, segs=18, minor_segs=4, at=(x, 0.04 + y, 0), rot=(90, 0, a),
                             scale=(1, 1, 0.55)), "white")
                        for x, y, a in ((-0.02, 0.01, -25), (0.0, -0.005, 5), (0.02, 0.012, 35))] +
                       [(rod((0.05, 0.02, 0.0), (0.075, -0.03, 0.004), 0.0035, segs=4), "white"),
                        (rod((0.075, -0.03, 0.004), (0.06, -0.07, 0.0), 0.0035, segs=4), "white")],
    "gravel:flint": [(lump(0.05, 13, rough=0.22, at=(0, 0.04, 0), scale=(1, 1.25, 0.55)), "flint")],
    "inferno:cinder_rod": [(cyl(0.016, 0.38, segs=6, at=(0, -0.08, 0)), "cinder")] +
                        [(cyl(0.019, 0.02, segs=6, at=(0, y, 0)), "cinder_band") for y in (0.0, 0.1, 0.2)],
    "inferno:wraith_tear": [(tear(0.035, at=(0, 0.04, 0), rot=(-90, 0, 0)), "tear")],
    "inferno:gold_nugget": [(lump(r, s, at=(x, 0.04 + y, z), scale=(1, 1, 0.8)), "gold")
                           for r, s, x, y, z in ((0.02, 1, 0, 0, 0), (0.016, 2, 0.022, 0.012, -0.004),
                                                 (0.014, 3, -0.016, 0.02, 0.0))],
    "infernoportal:lighter": [(ring(0.035, 0.008, start=-120, sweep=280, segs=12, at=(0, 0.04, 0.0),
                                   rot=(90, 0, 0)), "iron"),
                             (lump(0.025, 5, rough=0.25, at=(0.05, 0.02, -0.01)), "flint")],
}

# The tools of every material: a shape of its own for each, not one shape in
# five colours. Wood is whittled -- thick, square, blunt; stone (the hoe only:
# the other four are the robot's tool_* bones) knapped, a rough lump lashed
# on; iron forged -- the plain working shape, a dark collar where head meets
# shaft; gold made for show -- slender, banded handle, a gold knob at the end;
# diamond cut -- crystal heads to sharp points on a wrapped grip, a gem in the
# collar. Heads stand out along Z, as the iron pick's has always done; a
# shovel's blade and a sword's lie in XY, facing the eye.
M["lashing"] = material("Lashing", (0.62, 0.50, 0.30))


def handle(head, length=0.52, top=0.44):
    """The shaft from y = top - length up to the head, and what each material
    does to it."""
    y0 = top - length
    if head == "wood":
        return [(box((0.036, length, 0.036), at=(0, y0 + length / 2, 0)), "dark_wood")]
    if head == "stone":
        return [(cyl(0.017, length, segs=6, at=(0, y0, 0)), "wood"),
                (cyl(0.022, 0.06, segs=6, at=(0, top - 0.09, 0)), "lashing")]
    if head == "iron":
        return [(cyl(0.016, length, segs=6, at=(0, y0, 0)), "wood"),
                (cyl(0.022, 0.05, segs=8, at=(0, top - 0.07, 0)), "dark_iron")]
    if head == "gold":
        return ([(cyl(0.014, length, segs=8, at=(0, y0, 0)), "dark_wood"),
                 (ball(0.024, segs=10, rings=6, at=(0, y0, 0)), "gold"),
                 (cyl(0.02, 0.04, segs=10, at=(0, top - 0.06, 0)), "dark_gold")] +
                [(cyl(0.018, 0.012, segs=8, at=(0, y0 + 0.08 + 0.09 * i, 0)), "gold") for i in range(3)])
    # diamond
    return [(cyl(0.016, length, segs=8, at=(0, y0, 0)), "dark_wood"),
            (cyl(0.019, 0.2, segs=8, at=(0, y0 + 0.02, 0)), "grip"),
            (cyl(0.024, 0.05, segs=8, at=(0, top - 0.07, 0)), "dark_iron"),
            (ball(0.012, segs=6, rings=4, at=(0, top - 0.045, 0.022), scale=(1, 1, 0.6)), "diamond")]


def side(points, thick, y, head, flip=False):
    """A head drawn as `points` in (out along Z, up), set at height y."""
    pts = [(-x, h) for x, h in points] if flip else points
    return (slab(pts, thick, at=(0, y, 0), rot=(0, 90, 0)), head)


def pick(head):
    if head == "wood":
        return handle(head) + [(box((0.05, 0.055, 0.26), at=(0, 0.44, 0)), "wood"),
                               (box((0.04, 0.03, 0.04), at=(0, 0.48, 0)), "dark_wood")]
    if head == "iron":
        return handle(head) + [side([(-0.16, 0.0), (-0.06, 0.02), (0.06, 0.02), (0.16, 0.0), (0.06, -0.025),
                                     (-0.06, -0.025)], 0.03, 0.425, head)]
    if head == "gold":   # long and thin, its points swept down
        return handle(head) + [side([(-0.2, -0.04), (-0.1, 0.012), (0.0, 0.02), (0.1, 0.012), (0.2, -0.04),
                                     (0.1, -0.005), (0.0, -0.012), (-0.1, -0.005)], 0.018, 0.43, head)]
    # diamond: a crystal bar, a long point each way and a ridge on top
    return handle(head) + [side([(-0.21, 0.0), (-0.07, 0.035), (0.0, 0.05), (0.07, 0.035), (0.21, 0.0),
                                 (0.07, -0.03), (0.0, -0.035), (-0.07, -0.03)], 0.034, 0.43, head),
                           (cyl(0.02, 0.05, segs=4, r2=0.0, at=(0, 0.47, 0)), head)]


def axe(head):
    if head == "wood":
        return handle(head) + [(box((0.04, 0.11, 0.1), at=(0, 0.4, 0.06)), "wood"),
                               (box((0.03, 0.05, 0.04), at=(0, 0.4, -0.03)), "wood")]
    if head == "iron":
        return handle(head) + [side([(0.0, -0.04), (0.06, -0.05), (0.12, -0.08), (0.13, 0.06), (0.06, 0.04),
                                     (0.0, 0.04)], 0.025, 0.39, head)]
    if head == "gold":   # two bits, thin, a curve each way
        bit = [(0.0, -0.03), (0.07, -0.04), (0.12, -0.08), (0.135, 0.0), (0.12, 0.08), (0.07, 0.04), (0.0, 0.03)]
        return handle(head) + [side(bit, 0.016, 0.4, head), side(bit, 0.016, 0.4, head, flip=True)]
    # diamond: a bearded crescent one way, a spike the other
    return handle(head) + [side([(0.0, -0.035), (0.05, -0.06), (0.1, -0.13), (0.14, -0.06), (0.15, 0.03),
                                 (0.12, 0.08), (0.05, 0.04), (0.0, 0.035)], 0.026, 0.39, head),
                           side([(0.0, -0.02), (0.09, 0.0), (0.0, 0.02)], 0.022, 0.4, head, flip=True)]


def shovel(head):
    if head == "wood":   # a paddle
        return handle(head, top=0.4) + [(box((0.1, 0.13, 0.025), at=(0, 0.465, 0)), "wood")]
    if head == "iron":
        return handle(head, top=0.42) + [(slab([(-0.045, 0.0), (0.045, 0.0), (0.045, 0.1), (0.0, 0.135),
                                                (-0.045, 0.1)], 0.012, at=(0, 0.41, 0)), head)]
    if head == "gold":   # a round spade
        pts = [(0.05 * math.cos(math.radians(a)), 0.065 + 0.07 * math.sin(math.radians(a)))
               for a in range(-90, 270, 30)]
        return handle(head, top=0.42) + [(slab(pts, 0.01, at=(0, 0.41, 0)), head)]
    # diamond: a pointed blade with a raised crystal spine
    return handle(head, top=0.42) + [(slab([(-0.05, 0.0), (0.05, 0.0), (0.055, 0.08), (0.0, 0.17),
                                            (-0.055, 0.08)], 0.012, at=(0, 0.41, 0)), head),
                                     (box((0.012, 0.13, 0.022), at=(0, 0.47, 0)), head)]


def sword(head):
    if head == "wood":   # a training sword: wide, blunt, square
        return [(box((0.032, 0.13, 0.032), at=(0, 0.005, 0)), "dark_wood"),
                (box((0.13, 0.03, 0.04), at=(0, 0.085, 0)), "dark_wood"),
                (box((0.065, 0.36, 0.028), at=(0, 0.28, 0)), "wood")]
    if head == "iron":
        return [(cyl(0.016, 0.13, segs=6, at=(0, -0.06, 0)), "wood"),
                (box((0.11, 0.02, 0.03), at=(0, 0.075, 0)), "dark_iron"),
                (slab([(-0.025, 0.085), (0.025, 0.085), (0.025, 0.46), (0.0, 0.51), (-0.025, 0.46)], 0.012), head)]
    if head == "gold":   # slender, a swept guard, a knob
        return [(cyl(0.013, 0.13, segs=8, at=(0, -0.06, 0)), "dark_wood"),
                (ball(0.022, segs=10, rings=6, at=(0, -0.065, 0)), "gold"),
                (ring(0.07, 0.01, start=200, sweep=140, segs=10, minor_segs=5, at=(0, 0.14, 0),
                      rot=(90, 0, 0)), "dark_gold"),
                (slab([(-0.017, 0.08), (0.017, 0.08), (0.017, 0.5), (0.0, 0.55), (-0.017, 0.5)], 0.009), head)]
    # diamond: broad, a dark fuller up the middle, a gem in the guard
    return [(cyl(0.016, 0.13, segs=8, at=(0, -0.06, 0)), "grip"),
            (ball(0.02, segs=8, rings=6, at=(0, -0.065, 0)), "dark_iron"),
            (slab([(-0.08, 0.06), (0.08, 0.06), (0.06, 0.09), (-0.06, 0.09)], 0.03), "dark_iron"),
            (ball(0.016, segs=6, rings=4, at=(0, 0.075, 0.016), scale=(1, 1, 0.6)), head),
            (slab([(-0.036, 0.09), (0.036, 0.09), (0.03, 0.44), (0.0, 0.52), (-0.03, 0.44)], 0.014), head),
            (box((0.01, 0.3, 0.017), at=(0, 0.25, 0)), "dark_iron")]


def hoe(head):
    if head == "wood":
        return handle(head) + [(box((0.045, 0.05, 0.05), at=(0, 0.43, 0)), "wood"),
                               (box((0.04, 0.02, 0.1), at=(0, 0.415, 0.065)), "wood")]
    if head == "stone":   # a knapped blade lashed on
        return handle(head) + [(lump(0.045, 7, rough=0.2, at=(0, 0.42, 0.05), scale=(0.5, 0.6, 1.6)), "stone")]
    thick = {"iron": 0.02, "gold": 0.014, "diamond": 0.022}[head]
    blade = {"iron": [(-0.02, -0.02), (0.11, -0.03), (0.12, 0.0), (0.0, 0.025), (-0.02, 0.02)],
             "gold": [(-0.02, -0.015), (0.08, -0.02), (0.13, -0.06), (0.12, 0.0), (0.0, 0.02), (-0.02, 0.015)],
             "diamond": [(-0.03, -0.02), (0.06, -0.025), (0.14, -0.05), (0.13, 0.01), (0.0, 0.03),
                         (-0.03, 0.02)]}[head]
    return handle(head) + [side(blade, thick, 0.43, head)]


TOOL_SHAPES = {"pick": pick, "axe": axe, "shovel": shovel, "sword": sword, "hoe": hoe}
for _kind, _shape in TOOL_SHAPES.items():
    for _head in ("wood", "stone", "iron", "gold", "diamond"):
        if _head == "stone" and _kind != "hoe":
            continue   # the robot's tool_* bones
        name = {"wood": "farm:hoe" if _kind == "hoe" else f"tools:wood_{_kind}",
                "stone": "tools:stone_hoe"}.get(_head, f"furnace:{_head}_{_kind}")
        ITEMS[name] = _shape(_head)

# The laboratory's torch (the engineer's, found at the backrooms' exit) and
# its frost (the crypt's): what the last fight's dark and last form ask for.
ITEMS["lab:torch"] = [(cyl(0.02, 0.13, segs=10, at=(0, -0.02, 0)), "dark_iron"),
                      (cyl(0.021, 0.04, segs=10, r2=0.032, at=(0, 0.11, 0)), "dark_iron"),
                      (cyl(0.029, 0.006, segs=12, at=(0, 0.15, 0)), "torch_lens"),
                      (box((0.012, 0.03, 0.012), at=(0, 0.03, 0.02)), "iron")]
ITEMS["lab:frost"] = gem(0.04, 0.05, 0.09, 6, "frost") + [
    (cyl(0.012, 0.06, segs=5, r2=0.0, at=(0.035, 0.06, 0), rot=(0, 0, -30)), "frost"),
    (cyl(0.01, 0.05, segs=5, r2=0.0, at=(-0.03, 0.07, 0), rot=(0, 0, 35)), "frost")]

# The backrooms' two compasses: a brass case facing the eye, a needle red at
# its tip; the face yellow as Level 0's wallpaper for the way in, green as the
# exit's signs for the way out.
for which, face in (("in", (0.85, 0.76, 0.38)), ("out", (0.24, 0.86, 0.35))):
    M[f"compass_{which}"] = material(f"Compass_{which}", face)
    ITEMS[f"backrooms:compass_{which}"] = [
        (cyl(0.045, 0.014, segs=20, rot=(90, 0, 0), at=(0, 0.04, -0.007)), "gold"),
        (ring(0.042, 0.005, segs=20, rot=(90, 0, 0), at=(0, 0.04, 0.007)), "dark_gold"),
        (cyl(0.038, 0.002, segs=20, rot=(90, 0, 0), at=(0, 0.04, 0.007)), f"compass_{which}"),
        (box((0.009, 0.032, 0.003), at=(0, 0.056, 0.01)), "red"),
        (box((0.009, 0.032, 0.003), at=(0, 0.024, 0.01)), "white"),
        (ball(0.005, at=(0, 0.04, 0.012)), "dark_gold")]

# What the backrooms' creatures leave: a Bacteria's black filament, wound like
# a hair, and a tooth of a Smiler's grin, glowing as the grin does. Two of each
# are the compass to the way out.
M["filament"] = material("Filament", (0.06, 0.06, 0.07), 0.0, 0.3)
M["smiler_tooth"] = material("SmilerTooth", (0.97, 0.97, 0.92), glow=True)
_wave = [(0.025 * math.sin(i * 1.1), -0.06 + 0.026 * i, 0.004 * math.cos(i * 1.7)) for i in range(10)]
ITEMS["backrooms:filament"] = [(rod(a, b, 0.006, segs=5), "filament") for a, b in zip(_wave, _wave[1:])]
ITEMS["backrooms:smiler_tooth"] = [(cyl(0.02, 0.07, segs=8, r2=0.002, at=(0, 0.03, 0)), "smiler_tooth"),
                                  (ball(0.021, segs=8, rings=6, at=(0, 0.03, 0), scale=(1, 0.6, 1)), "smiler_tooth")]

# The backrooms' torch, found in their chests: the laboratory's.
ITEMS["backrooms:torch"] = [(bm.copy(), m) for bm, m in ITEMS["lab:torch"]]   # build() frees what it uses

# The seven keys of the ending (mods/keys): a bow in the fist, the shank up
# the screen, two bits at its end; each in its world's colours, as its icon
# (tools/make_key_textures.py).
KEY_COLOURS = {
    "pista": ((0.84, 0.16, 0.16), (0.96, 0.96, 0.96)), "inferno": ((0.90, 0.35, 0.08), (0.35, 0.08, 0.08)),
    "arena": ((0.47, 0.51, 0.55), (0.78, 0.16, 0.12)), "arpg": ((0.59, 0.47, 0.24), (0.24, 0.63, 0.86)),
    "chambers": ((0.92, 0.92, 0.92), (0.16, 0.51, 1.00)), "backrooms": ((0.84, 0.76, 0.35), (0.35, 0.31, 0.16)),
    "parkour": ((0.96, 0.55, 0.08), (0.12, 0.12, 0.12)),
}
for w, (body, bit) in KEY_COLOURS.items():
    M[f"key_{w}"] = material(f"Key_{w}", body, 0.8, 0.35)
    M[f"key_{w}_bit"] = material(f"KeyBit_{w}", bit, 0.6, 0.4)
    ITEMS[f"keys:{w}"] = [(ring(0.028, 0.009, segs=14, at=(0, 0.0, 0), rot=(90, 0, 0)), f"key_{w}"),
                         (box((0.014, 0.13, 0.014), at=(0, 0.09, 0)), f"key_{w}"),
                         (box((0.03, 0.014, 0.012), at=(0.02, 0.14, 0)), f"key_{w}_bit"),
                         (box((0.024, 0.012, 0.012), at=(0.017, 0.115, 0)), f"key_{w}_bit"),
                         (ball(0.009, at=(0, 0.0, 0.006)), f"key_{w}_bit")]

# The ones with no icon, and how each is turned for its picture, which is seen
# the way first person sees it: long things lie corner to corner like the
# tools' icons.
ICONS = {
    # Three quarters from above, as a block: turned 45 about the screen's up,
    # then its top tipped 30 towards the eye.
    "cinnabar:hopper": tuple(math.degrees(a) for a in (
        Matrix.Rotation(math.radians(30), 3, "X") @ Matrix.Rotation(math.radians(45), 3, "Y")).to_euler("XYZ")),
    "furnace:furnace": tuple(math.degrees(a) for a in (
        Matrix.Rotation(math.radians(30), 3, "X") @ Matrix.Rotation(math.radians(35), 3, "Y")).to_euler("XYZ")),
    "chest:box": tuple(math.degrees(a) for a in (
        Matrix.Rotation(math.radians(30), 3, "X") @ Matrix.Rotation(math.radians(35), 3, "Y")).to_euler("XYZ")),
    "craft:table": tuple(math.degrees(a) for a in (
        Matrix.Rotation(math.radians(30), 3, "X") @ Matrix.Rotation(math.radians(35), 3, "Y")).to_euler("XYZ")),
    "conveyor:sorter": tuple(math.degrees(a) for a in (
        Matrix.Rotation(math.radians(30), 3, "X") @ Matrix.Rotation(math.radians(45), 3, "Y")).to_euler("XYZ")),
    "conveyor:extractor": tuple(math.degrees(a) for a in (
        Matrix.Rotation(math.radians(30), 3, "X") @ Matrix.Rotation(math.radians(45), 3, "Y")).to_euler("XYZ")),
    "conveyor:belt": tuple(math.degrees(a) for a in (
        Matrix.Rotation(math.radians(30), 3, "X") @ Matrix.Rotation(math.radians(45), 3, "Y")).to_euler("XYZ")),
    "furnace:charcoal": (-20, 0, 15),
    "jetpack:jetpack": (0, 0, 0),
    "backpack:backpack": (0, 0, 0),
    "archery:bow": (147.0, -55.5, -106.8),   # its plane to the camera, string to the lower left
    "archery:arrow": (0, 0, -45),
    "archery:string": (0, 0, 0),
    "gravel:flint": (0, 0, 0),
    "inferno:cinder_rod": (0, 0, -45),
    "inferno:wraith_tear": (0, 0, 0),
    "inferno:gold_nugget": (-30, 0, 15),
    "infernoportal:lighter": (0, 0, 0),
    "lab:torch": (0, 0, -45),
    "lab:frost": (0, 0, 0),
    "backrooms:compass_in": (0, 0, 0),
    "backrooms:compass_out": (0, 0, 0),
    "backrooms:filament": (0, 0, -45),
    "backrooms:smiler_tooth": (0, 0, -30),
    "backrooms:torch": (0, 0, -45),
}


def export(obj, item):
    mod, name = item.split(":")
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=os.path.join(mod_dir(mod), f"{name}_held.gltf"),
                              export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_yup=True, export_materials="EXPORT",
                              export_image_format="NONE", export_skins=False)


def render_icon(obj, out, turn, size=32):
    """Seen from +Z with +Y up, orthographic, flat light; 256 pixels down to `size`."""
    scene = bpy.context.scene
    try:
        scene.render.engine = "BLENDER_WORKBENCH"
    except TypeError as e:
        print("workbench:", e)
    scene.display.shading.light = "FLAT"      # the colours as given; the edges from the cavity
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = "BOTH"
    scene.display.shading.color_type = "MATERIAL"
    scene.render.film_transparent = True
    scene.render.resolution_x = scene.render.resolution_y = 256
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    try:   # the colours as given, without a film curve
        scene.view_settings.view_transform = "Raw"
    except TypeError as e:
        print("view transform:", e)

    for o in scene.objects:
        o.hide_render = o is not obj
    obj.rotation_euler = [math.radians(a) for a in turn]
    bpy.context.view_layer.update()
    pts = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    centre = sum(pts, Vector()) / 8
    extent = max(max(p.x for p in pts) - min(p.x for p in pts), max(p.y for p in pts) - min(p.y for p in pts))

    cam = bpy.data.objects.get("IconCam")
    if cam is None:
        cam = bpy.data.objects.new("IconCam", bpy.data.cameras.new("IconCam"))
        scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = extent * 1.2
    cam.location = centre + Vector((0, 0, 1.0))
    cam.rotation_euler = (0, 0, 0)
    scene.camera = cam

    scene.render.filepath = out
    bpy.ops.render.render(write_still=True)

    # 32 pixels, hard alpha, and a dark line round the outside like the rest.
    img = bpy.data.images.load(out)
    img.scale(size, size)
    px = np.array(img.pixels[:]).reshape(size, size, 4)
    solid = px[:, :, 3] > 0.45
    px[:, :, 3] = solid.astype(float)
    edge = np.zeros_like(solid)
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        edge |= np.roll(solid, (dy, dx), axis=(0, 1))
    edge &= ~solid
    px[edge] = (0.10, 0.07, 0.07, 1.0)
    img.pixels[:] = px.ravel()
    img.filepath_raw = out
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)
    obj.rotation_euler = (0, 0, 0)


# `-- DIR` after the script: a picture of every model in DIR as well, to look
# them over; the icons of assets/textures are only made for ICONS.
# ONLY="cinnabar:hopper,..." in the environment: just those, so one new item
# does not rewrite every other model and icon.
args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ONLY = [s for s in os.environ.get("ONLY", "").split(",") if s]
for i, (item, parts) in enumerate(ITEMS.items()):
    if ONLY and item not in ONLY:
        continue
    name_held = item.replace(":", "_") + "_held"
    obj = parts(name_held) if callable(parts) else build(name_held, parts, M)
    export(obj, item)
    mod, name = item.split(":")
    if item in ICONS:
        render_icon(obj, os.path.join(INFERNO_TEXTURES if mod == "inferno" else LAB_TEXTURES if mod == "lab"
                                      else BACKROOMS_TEXTURES if mod == "backrooms" else TEXTURES,
                                      f"{mod}_{name}.png"),
                    ICONS[item])
    if args:
        # PREVIEW_TURN="x,y,z": every picture from that turn instead, to see
        # what lies along Z.
        turn = os.environ.get("PREVIEW_TURN")
        render_icon(obj, os.path.join(args[0], f"{mod}_{name}.png"),
                    tuple(float(v) for v in turn.split(",")) if turn else ICONS.get(item, (0, 0, 0)),
                    128 if turn else 32)
    obj.location.x = (i % 8) * 0.5   # laid out in the .blend; exported at the origin
    obj.location.y = (i // 8) * 0.8
for o in bpy.context.scene.objects:
    o.hide_render = False
if not ONLY:
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
print(f"{len(ITEMS)} models, {len(ICONS)} icons")
