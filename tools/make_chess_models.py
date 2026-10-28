# The models of mods/chess, made in Blender:
#
#   board.gltf       a chess table one cell across: four legs, a walnut top and
#                    on it the 64 squares, a tenth of a node each, a1 dark at
#                    the front-left (the front is +Z, the side white sits).
#                    The squares' top is at TOP.
#   {w,b}_{p,n,b,r,q,k}.gltf
#                    the pieces, white in ivory and black in ebony, Staunton
#                    shapes turned on a lathe (the knight's head cut from a
#                    side profile), feet at 0 about their middle, the knight
#                    looking down -Z. Sized to the squares: the king is 0.155.
#   {w,b}_{m,d,o}.gltf
#                    the other games of the table: draughts' man (a disc with a
#                    groove round it and rings on its face) and king (two of
#                    them stacked), and the mill's stone (rounded, lower)
#   blast.gltf, smoke.gltf
#                    atomic chess's explosion: a ball of fire in lumps, white
#                    hot inside to red outside, and a puff of grey smoke, a
#                    unit across about their middle; the client grows and
#                    shrinks them (core.render.model's scale).
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_chess_models.py"
#
# Also renders a preview of all of them to ~/kart_assets/chess.png, and leaves
# the work in assets/blender/chess.blend. Blender is Z up: glTF (x, y, z) is
# Blender (x, -z, y).
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
MOD = os.path.join(ROOT, "mods", "chess")
BLEND = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "chess.blend")
PREVIEW = os.path.expanduser("~/kart_assets/chess.png")

SQ = 0.1      # a square, in nodes
TOP = 0.775   # the squares' top, above the cell's floor


def material(name, rgb, metal=0.0, rough=0.5):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1.0)
    return m


MATS = {}


def mats():
    if not MATS:
        MATS.update({
            "ivory": material("CH Ivory", (0.93, 0.87, 0.74), rough=0.35),
            "ebony": material("CH Ebony", (0.11, 0.09, 0.08), rough=0.3),
            "light": material("CH Light Square", (0.86, 0.73, 0.53), rough=0.45),
            "dark": material("CH Dark Square", (0.44, 0.26, 0.13), rough=0.45),
            "walnut": material("CH Walnut", (0.26, 0.15, 0.08), rough=0.5),
            "felt": material("CH Felt", (0.10, 0.28, 0.14), rough=0.9),
            "core": material("CH Fire Core", (1.0, 0.93, 0.55), rough=1.0),
            "fire": material("CH Fire", (1.0, 0.40, 0.02), rough=1.0),
            "ember": material("CH Ember", (0.70, 0.10, 0.02), rough=1.0),
            "smoke": material("CH Smoke", (0.33, 0.32, 0.31), rough=1.0),
            "soot": material("CH Soot", (0.18, 0.17, 0.17), rough=1.0),
        })
    return MATS


def box(lo, hi):
    """Blender coordinates, corner to corner."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=[h - l for l, h in zip(lo, hi)], verts=bm.verts)
    bmesh.ops.translate(bm, vec=[(l + h) / 2 for l, h in zip(lo, hi)], verts=bm.verts)
    return bm


def lathe(profile, segs=32):
    """A solid of revolution about Z from (radius, height) pairs, bottom to
    top; a radius of 0 closes that end."""
    bm = bmesh.new()
    rings = []
    for r, z in profile:
        if r <= 1e-6:
            rings.append([bm.verts.new((0.0, 0.0, z))])
        else:
            rings.append([bm.verts.new((math.cos(a) * r, math.sin(a) * r, z))
                          for a in (2 * math.pi * k / segs for k in range(segs))])
    for lo, hi in zip(rings, rings[1:]):
        for k in range(segs):
            n = (k + 1) % segs
            if len(lo) == 1 and len(hi) == 1:
                continue
            if len(lo) == 1:
                bm.faces.new((lo[0], hi[k], hi[n]))
            elif len(hi) == 1:
                bm.faces.new((lo[k], lo[n], hi[0]))
            else:
                bm.faces.new((lo[k], lo[n], hi[n], hi[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return bm


def arc(cx, cz, r, a0, a1, steps=10):
    """Points of a circle in the (radius, height) plane, a0 to a1 degrees."""
    return [(max(0.0, cx + r * math.cos(math.radians(a))), cz + r * math.sin(math.radians(a)))
            for a in (a0 + (a1 - a0) * k / steps for k in range(steps + 1))]


def sphere(r, at, segs=16):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=segs // 2, radius=r)
    bmesh.ops.translate(bm, vec=at, verts=bm.verts)
    return bm


def prism(points, thick, x0=0.0):
    """A side profile in Blender's (y, z) extruded `thick` along X, centred on x0."""
    bm = bmesh.new()
    a = [bm.verts.new((x0 - thick / 2, y, z)) for y, z in points]
    b = [bm.verts.new((x0 + thick / 2, y, z)) for y, z in points]
    bm.faces.new(a)
    bm.faces.new(b[::-1])
    for k in range(len(points)):
        n = (k + 1) % len(points)
        bm.faces.new((a[k], a[n], b[n], b[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return bm


def mesh_object(name, parts, smooth=True):
    """Each part a bmesh and its material; joined into one object."""
    m = mats()
    objs = []
    for bm, mat in parts:
        me = bpy.data.meshes.new(name + "_part")
        bm.to_mesh(me)
        bm.free()
        me.materials.append(m[mat])
        o = bpy.data.objects.new(name + "_part", me)
        bpy.context.scene.collection.objects.link(o)
        objs.append(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    if smooth:
        bpy.ops.object.shade_smooth()
    return obj


def cut(obj, cutter_bm):
    """Takes a box (or anything) out of obj, for the rook's battlements and
    the bishop's slit."""
    me = bpy.data.meshes.new("cutter")
    cutter_bm.to_mesh(me)
    cutter_bm.free()
    c = bpy.data.objects.new("cutter", me)
    bpy.context.scene.collection.objects.link(c)
    mod = obj.modifiers.new("cut", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.object = c
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(c)


def export(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_image_format="NONE",
                              export_skins=False)


# ------------------------------------------------------------------ the board

def label(text, x, y, turn):
    """A letter or a figure of Blender's own font, a thin slab centred on
    (x, y) on the frame's top, turned about Z by `turn`."""
    cu = bpy.data.curves.new("label", "FONT")
    cu.body = text
    cu.size = 0.032
    cu.extrude = 0.0006
    cu.resolution_u = 3   # curves enough at this size, and a tenth of the triangles
    cu.align_x, cu.align_y = "CENTER", "CENTER"
    o = bpy.data.objects.new("label", cu)
    bpy.context.scene.collection.objects.link(o)
    bm = bmesh.new()
    bm.from_mesh(o.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh())
    bpy.data.objects.remove(o)
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(turn, 3, "Z"))
    bmesh.ops.translate(bm, vec=(x, y, 0.768 + 0.0006), verts=bm.verts)
    return bm


def board():
    parts = []
    # Four legs and an apron under the top.
    for x in (0.08, 0.86):
        for y in (-0.08, -0.86):
            parts.append((box((x, y - 0.06, 0.0), (x + 0.06, y, 0.70)), "walnut"))
    parts.append((box((0.10, -0.90, 0.62), (0.90, -0.10, 0.70)), "walnut"))
    # The top, its frame round the squares, and felt under it.
    parts.append((box((0.03, -0.97, 0.70), (0.97, -0.03, 0.76)), "walnut"))
    parts.append((box((0.05, -0.95, 0.76), (0.95, -0.05, 0.768)), "walnut"))
    for f in range(8):
        for r in range(8):
            x = 0.5 + (f - 3.5) * SQ
            y = -(0.5 + (3.5 - r) * SQ)   # rank 1 at glTF +Z, Blender -Y
            dark = (f + r) % 2 == 0
            parts.append((box((x - SQ / 2, y - SQ / 2, 0.76), (x + SQ / 2, y + SQ / 2, TOP)),
                          "dark" if dark else "light"))
    # The coordinates on the frame, small, in ivory: the files along white's
    # edge and black's, the ranks down the a-file's side and the h-file's,
    # each read from the side whose left it is on.
    for f in range(8):
        x = 0.5 + (f - 3.5) * SQ
        parts.append((label("abcdefgh"[f], x, -0.925, 0.0), "ivory"))
        parts.append((label("abcdefgh"[f], x, -0.075, math.pi), "ivory"))
    for r in range(8):
        y = -(0.5 + (3.5 - r) * SQ)
        parts.append((label(str(r + 1), 0.075, y, 0.0), "ivory"))
        parts.append((label(str(r + 1), 0.925, y, math.pi), "ivory"))
    obj = mesh_object("board", parts, smooth=False)
    export(obj, os.path.join(MOD, "board.gltf"))
    return obj


# ----------------------------------------------------------------- the pieces
# Profiles in squares (radius, height), scaled by SQ.

def base(r):
    """The foot every piece stands on: a plinth, a bead and a cove."""
    return [(0, 0), (r, 0), (r, 0.05), (r * 0.94, 0.08)] + \
        arc(r * 0.84, 0.11, r * 0.1, -90, 90, 4) + \
        [(r * 0.72, 0.16), (r * 0.62, 0.22)]


def pawn():
    p = base(0.34) + [(0.17, 0.30), (0.12, 0.48), (0.21, 0.52), (0.21, 0.55), (0.11, 0.58)]
    p += arc(0, 0.72, 0.16, -60, 90, 12)
    return [(lathe([(a * SQ, b * SQ) for a, b in p]), "M")]


def rook():
    p = base(0.37) + [(0.24, 0.30), (0.21, 0.70), (0.29, 0.76), (0.29, 0.80), (0.25, 0.83),
                      (0.27, 0.86), (0.27, 1.05), (0.19, 1.05), (0.19, 0.97), (0, 0.97)]
    return [(lathe([(a * SQ, b * SQ) for a, b in p]), "M")]


def rook_cut(obj):
    # Four gaps in the battlements.
    for k in range(2):
        bm = box((-0.04 * SQ, -0.4 * SQ, 0.93 * SQ), (0.04 * SQ, 0.4 * SQ, 1.2 * SQ))
        bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(math.pi / 4 + k * math.pi / 2, 3, "Z"))
        cut(obj, bm)


def knight():
    p = base(0.37) + [(0.26, 0.30), (0.26, 0.36), (0.30, 0.38), (0.30, 0.42), (0, 0.42)]
    parts = [(lathe([(a * SQ, b * SQ) for a, b in p]), "M")]
    # The head, looking down +Y (glTF -Z): neck, mane, ears, nose.
    head = [(-0.26, 0.40), (-0.30, 0.62), (-0.28, 0.86), (-0.18, 1.04), (-0.06, 1.14),
            (0.00, 1.24), (0.06, 1.12), (0.16, 1.04), (0.30, 0.90), (0.40, 0.78),
            (0.40, 0.68), (0.32, 0.64), (0.16, 0.70), (0.08, 0.62), (0.16, 0.50),
            (0.22, 0.40)]
    parts.append((prism([(y * SQ, z * SQ) for y, z in head], 0.26 * SQ), "M"))
    # The mane down the back of the neck, a little wider.
    mane = [(-0.30, 0.50), (-0.34, 0.70), (-0.30, 0.92), (-0.18, 1.08), (-0.14, 1.00),
            (-0.22, 0.86), (-0.24, 0.64)]
    parts.append((prism([(y * SQ, z * SQ) for y, z in mane], 0.18 * SQ), "M"))
    # Eyes as two beads either side.
    for x in (-0.13, 0.13):
        parts.append((sphere(0.035 * SQ, (x * SQ, 0.12 * SQ, 0.96 * SQ), 8), "M"))
    return parts


def bishop():
    p = base(0.36) + [(0.20, 0.30), (0.13, 0.72), (0.23, 0.76), (0.23, 0.80), (0.14, 0.83),
                      (0.12, 0.86)]
    p += [(0.16, 0.90), (0.19, 0.98), (0.19, 1.06), (0.16, 1.14), (0.10, 1.21), (0.05, 1.24)]
    p += arc(0, 1.29, 0.06, -60, 90, 6)
    return [(lathe([(a * SQ, b * SQ) for a, b in p]), "M")]


def bishop_cut(obj):
    bm = box((-0.3 * SQ, -0.025 * SQ, 1.02 * SQ), (0.3 * SQ, 0.025 * SQ, 1.4 * SQ))
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 1.06 * SQ), matrix=Matrix.Rotation(math.radians(35), 3, "X"))
    bmesh.ops.translate(bm, vec=(0, 0.04 * SQ, 0), verts=bm.verts)
    cut(obj, bm)


def queen():
    p = base(0.38) + [(0.22, 0.30), (0.14, 0.86), (0.24, 0.90), (0.24, 0.94), (0.15, 0.98),
                      (0.14, 1.02), (0.27, 1.24), (0.24, 1.26), (0.14, 1.24)]
    p += arc(0, 1.24, 0.14, 0, 90, 6)
    p += arc(0, 1.43, 0.06, -60, 90, 6)
    parts = [(lathe([(a * SQ, b * SQ) for a, b in p]), "M")]
    for k in range(8):
        a = 2 * math.pi * k / 8
        parts.append((sphere(0.045 * SQ, (math.cos(a) * 0.255 * SQ, math.sin(a) * 0.255 * SQ, 1.27 * SQ), 8), "M"))
    return parts


def king():
    p = base(0.39) + [(0.23, 0.30), (0.15, 0.92), (0.25, 0.96), (0.25, 1.00), (0.16, 1.04),
                      (0.15, 1.08), (0.24, 1.26), (0.24, 1.30), (0.10, 1.34), (0.06, 1.36), (0, 1.36)]
    parts = [(lathe([(a * SQ, b * SQ) for a, b in p]), "M")]
    w = 0.045
    parts.append((box((-w * SQ, -w * SQ, 1.34 * SQ), (w * SQ, w * SQ, 1.62 * SQ)), "M"))
    parts.append((box((-0.13 * SQ, -w * SQ, 1.44 * SQ), (0.13 * SQ, w * SQ, 1.53 * SQ)), "M"))
    return parts


def man():
    p = [(0, 0), (0.38, 0), (0.40, 0.03), (0.40, 0.06), (0.37, 0.075), (0.40, 0.09), (0.40, 0.12),
         (0.37, 0.145), (0.30, 0.145), (0.28, 0.13), (0.22, 0.13), (0.20, 0.145), (0.10, 0.145),
         (0.08, 0.13), (0, 0.13)]
    return [(lathe([(a * SQ, b * SQ) for a, b in p]), "M")]


def king_man():
    p = [(0, 0), (0.38, 0), (0.40, 0.03), (0.40, 0.11), (0.37, 0.135), (0.40, 0.16), (0.40, 0.25),
         (0.37, 0.28), (0.30, 0.28), (0.28, 0.265), (0.22, 0.265), (0.20, 0.28), (0.12, 0.28),
         (0.12, 0.33), (0.07, 0.33), (0.07, 0.28), (0, 0.28)]
    return [(lathe([(a * SQ, b * SQ) for a, b in p]), "M")]


def stone():
    p = [(0, 0), (0.30, 0), (0.36, 0.04), (0.37, 0.09), (0.33, 0.15), (0.20, 0.185), (0, 0.19)]
    return [(lathe([(a * SQ, b * SQ) for a, b in p]), "M")]


PIECES = {"m": (man, None), "d": (king_man, None), "o": (stone, None), "p": (pawn, None), "r": (rook, rook_cut), "n": (knight, None), "b": (bishop, bishop_cut),
          "q": (queen, None), "k": (king, None)}


def piece(kind, colour):
    make, finish = PIECES[kind]
    mat = "ivory" if colour == "w" else "ebony"
    obj = mesh_object(f"{colour}_{kind}", [(bm, mat) for bm, _ in make()])
    if finish:
        finish(obj)
    export(obj, os.path.join(MOD, f"{colour}_{kind}.gltf"))
    return obj


# ------------------------------------------------------------- the explosion

def lumps(seed, count, spread, radius, mats_by_depth):
    """A cloud of rough spheres round the origin, its middle 0.5 at most:
    the nearer the middle a lump, the earlier its material in the list."""
    import random
    rnd = random.Random(seed)
    parts = []
    for k in range(count):
        d = spread * (k / max(count - 1, 1)) ** 0.7
        a, e = rnd.uniform(0, 2 * math.pi), rnd.uniform(-0.9, 1.2)
        at = (d * math.cos(a) * math.cos(e), d * math.sin(a) * math.cos(e), d * math.sin(e))
        r = radius * rnd.uniform(0.75, 1.15) * (1 - 0.35 * d / max(spread, 1e-6))
        bm = sphere(r, at, segs=12)
        for v in bm.verts:   # knobbly, not balls
            v.co += (v.co - Vector(at)).normalized() * r * rnd.uniform(-0.12, 0.12)
        mat = mats_by_depth[min(len(mats_by_depth) - 1, int(len(mats_by_depth) * d / (spread + 1e-6)))]
        parts.append((bm, mat))
    return parts


def blast():
    obj = mesh_object("blast", lumps(7, 14, 0.30, 0.20, ["core", "fire", "fire", "fire", "ember", "ember"]))
    export(obj, os.path.join(MOD, "blast.gltf"))
    return obj


def smoke():
    obj = mesh_object("smoke", lumps(11, 10, 0.28, 0.21, ["smoke", "smoke", "soot"]))
    export(obj, os.path.join(MOD, "smoke.gltf"))
    return obj


# ---------------------------------------------------------------- the preview

def preview(objs):
    scene = bpy.context.scene
    for i, o in enumerate(objs[1:]):
        o.location = (0.15 + (i % 6) * 0.13, -0.25 - (i // 6) * 0.5, TOP)
        if o.name.endswith("_n"):
            o.rotation_euler.z = math.radians(-90)   # its profile, for the picture
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    cam.location = (0.5, -1.55, 1.45)
    cam.rotation_euler = (math.radians(58), 0, 0)
    scene.camera = cam
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.rotation_euler = (math.radians(40), math.radians(20), math.radians(30))
    scene.collection.objects.link(sun)
    try:
        scene.render.engine = "BLENDER_WORKBENCH"
    except TypeError:
        pass
    scene.display.shading.color_type = "MATERIAL"
    scene.render.resolution_x, scene.render.resolution_y = 960, 720
    scene.render.filepath = PREVIEW
    bpy.ops.render.render(write_still=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
os.makedirs(MOD, exist_ok=True)
made = [board()] + [piece(k, c) for c in "wb" for k in "prnbqkmdo"]
for o in (blast(), smoke()):
    o.location = (-0.6 if o.name == "blast" else -1.4, 0.2, 1.4)   # beside the board in the picture
os.makedirs(os.path.dirname(BLEND), exist_ok=True)
preview(made)
bpy.ops.wm.save_as_mainfile(filepath=BLEND)
print("chess models:", ", ".join(o.name for o in made))
