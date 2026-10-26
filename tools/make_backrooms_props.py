# The block models of games/backrooms, made in Blender:
#
#   slide.gltf   one cell of a poolrooms slide: a blue plastic chute, its bed
#                falling from 1.25 over the cell's floor at its back (+z) to
#                0.25 at its front, where the next cell's begins -- the line
#                the belt carries a body along (core.node_conveyor 5, 1.25,
#                -1) -- with raised sides and a rolled lip, and the sides
#                carried down under the bed so a run of them reads as one
#                chute in the air.
#   slide_turn_a.gltf, slide_turn_b.gltf
#                a quarter turn of the chute: in through the cell's +x side
#                (a) or its -x side (b), out through its front (-z), the bed
#                falling 1.25 to 0.25 along the curve.
#   ladder.gltf  one cell of a steel ladder against the +z side of its cell:
#                two round rails and two rungs, bright steel. Up a slide's tower and
#                out of every pool.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_backrooms_props.py"
#
# Or in the open Blender, through the MCP: bpy.ops.wm.read_homefile(use_empty=
# True) in one call (in the same call the export finds no context), then run
# this file with __name__ set to anything but "__main__" and call make().
#
# A block's model is authored in its cell, 0..1 each way with its feet at 0
# (glTF's x, y up, z); Blender is Z up: glTF (x, y, z) is Blender (x, -z, y),
# so the cell is x 0..1, y -1..0 here, and glTF's +z is Blender's -y.
# Leaves the work in assets/blender/backrooms_props.blend.
import math
import os

import bmesh
import bpy

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "games", "backrooms", "mods", "backrooms")
BLEND = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "backrooms_props.blend")


def material(name, rgb, metal=0.0, rough=0.5):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1.0)
    return m


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.materials):
        for d in list(coll):
            coll.remove(d)


def mesh_object(name, verts, faces, mats, face_mats):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    for m in mats:
        me.materials.append(m)
    for poly, mi in zip(me.polygons, face_mats):
        poly.material_index = mi
    me.validate()
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def bed(t):
    """The bed's height over the cell's floor at t, 0 at the back (y -1, glTF
    z 1) to 1 at the front (y 0)."""
    return 1.25 - t


# The chute's cross-section, across its width (0..1) then height over the bed;
# the inside first (the ride), then round the outside back to where it began.
# Symmetric about the middle.
SECTION = [(0.06, 0.34), (0.1, 0.36), (0.14, 0.3), (0.14, 0.06), (0.22, 0.0), (0.78, 0.0),
           (0.86, 0.06), (0.86, 0.3), (0.9, 0.36), (0.94, 0.34),
           (0.97, 0.26), (0.97, -0.42), (0.03, -0.42), (0.03, 0.26)]


def sweep(name, path, steps):
    """A chute section: SECTION swept along path(t), t 0 at the cell's way in
    and 1 at its way out, falling with the bed. path(t) gives the middle of the
    ride and the direction across it, in glTF's x and z; written in Blender's
    axes. Ends left open: the next cell's section meets this one face to face."""
    blue = material("slide_blue", (0.08, 0.42, 0.85), rough=0.25)
    inner = material("slide_inner", (0.35, 0.7, 0.95), rough=0.15)
    n = len(SECTION)
    verts, faces, fm = [], [], []
    for s in range(steps + 1):
        t = s / steps
        (cx, cz), (lx, lz) = path(t)
        for x, dz in SECTION:
            gx, gz = cx + (x - 0.5) * lx, cz + (x - 0.5) * lz
            verts.append((gx, -gz, bed(t) + dz))
    for s in range(steps):
        for i in range(n):
            a, b = s * n + i, s * n + (i + 1) % n
            c, d = b + n, a + n
            faces.append((a, b, c, d))
            fm.append(1 if 2 <= i <= 6 else 0)   # the ride is the lighter blue
    obj = mesh_object(name, verts, faces, [blue, inner], fm)
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


def slide():
    """Straight: in at the back (glTF z 1), out at the front (z 0), heading -z
    as the block does at facing 0."""
    return sweep("slide", lambda t: ((0.5, 1.0 - t), (1.0, 0.0)), 6)


def slide_turn(name, mirror):
    """A quarter turn on the way down: in through the +x side heading -x (or,
    mirrored, through -x heading +x), out through the front heading -z -- a
    quarter circle of radius a half about the corner the two sides share.
    The direction across turns from the way in's to the way out's (+x for a
    body heading -z, as the straight's), so the faces wind as the straight's."""
    def path(t):
        if not mirror:
            a = math.radians(90 + 90 * t)
            c, r = (1.0 + 0.5 * math.cos(a), 0.5 * math.sin(a)), (math.cos(a), math.sin(a))
            return c, (-r[0], -r[1])
        a = math.radians(90 - 90 * t)
        return (0.5 * math.cos(a), 0.5 * math.sin(a)), (math.cos(a), math.sin(a))
    return sweep(name, path, 12)


def cylinder(bm, a, b, r, seg=10):
    """A round bar from a to b (points), into bm."""
    ax = [b[i] - a[i] for i in range(3)]
    length = math.sqrt(sum(c * c for c in ax))
    k = [c / length for c in ax]
    # Two directions across the bar.
    up = (0, 0, 1) if abs(k[2]) < 0.9 else (1, 0, 0)
    u = (k[1] * up[2] - k[2] * up[1], k[2] * up[0] - k[0] * up[2], k[0] * up[1] - k[1] * up[0])
    lu = math.sqrt(sum(c * c for c in u))
    u = [c / lu for c in u]
    v = (k[1] * u[2] - k[2] * u[1], k[2] * u[0] - k[0] * u[2], k[0] * u[1] - k[1] * u[0])
    rings = []
    for end in (a, b):
        ring = []
        for i in range(seg):
            ang = 2 * math.pi * i / seg
            ring.append(bm.verts.new([end[j] + r * (math.cos(ang) * u[j] + math.sin(ang) * v[j]) for j in range(3)]))
        rings.append(ring)
    for i in range(seg):
        bm.faces.new((rings[0][i], rings[0][(i + 1) % seg], rings[1][(i + 1) % seg], rings[1][i]))
    bm.faces.new(rings[0][::-1])
    bm.faces.new(rings[1])


def ladder():
    # Steel, but not metallic: the engine has nothing for a metal to reflect,
    # and a metallic one drew black.
    chrome = material("ladder_chrome", (0.78, 0.8, 0.84), metal=0.0, rough=0.3)
    bm = bmesh.new()
    y = -0.88   # against glTF +z: Blender's back of the cell
    for x in (0.18, 0.82):
        cylinder(bm, (x, y, 0.0), (x, y, 1.0), 0.045)
    for z in (0.25, 0.75):
        cylinder(bm, (0.18, y, z), (0.82, y, z), 0.035)
    me = bpy.data.meshes.new("ladder")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(chrome)
    obj = bpy.data.objects.new("ladder", me)
    bpy.context.scene.collection.objects.link(obj)
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


def export(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_yup=True, export_apply=False,
                              export_materials="EXPORT", export_image_format="NONE")


def make():
    clear()
    os.makedirs(OUT, exist_ok=True)
    s = slide()
    export(s, os.path.join(OUT, "slide.gltf"))
    for k, (name, mirror) in enumerate((("slide_turn_a", False), ("slide_turn_b", True))):
        o = slide_turn(name, mirror)
        export(o, os.path.join(OUT, f"{name}.gltf"))
        o.location.x = -1.5 * (k + 1)
    lad = ladder()
    export(lad, os.path.join(OUT, "ladder.gltf"))
    lad.location.x = 1.5
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("slide and ladder in", os.path.normpath(OUT))


if __name__ == "__main__":
    make()
