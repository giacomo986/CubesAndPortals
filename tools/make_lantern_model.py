# The lantern of mods/building as models: standing on a block and hung under one.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_lantern_model.py"
#
# Saves assets/blender/lantern.blend and exports mods/building/lantern.gltf and
# lantern_hung.gltf. In the unit cell, feet at 0, as tools/make_rail_models.py:
# Blender x 0..1, y -1..0, z up (glTF z is Blender -y). An iron frame -- a
# base, four corner posts, a pyramid roof with a knob and a ring -- round four
# panes of glowing glass and a flame; hung, the same higher up on a chain of
# links from the top of the cell.
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "building")
BLEND = os.path.join(ROOT, "assets", "blender", "lantern.blend")
C = Vector((0.5, -0.5, 0.0))   # the cell's middle on its floor


def material(name, rgb, metal, rough, glow=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Metallic"].default_value = metal
    p.inputs["Roughness"].default_value = rough
    if glow > 0:
        p.inputs["Emission Color"].default_value = (*rgb, 1)
        p.inputs["Emission Strength"].default_value = glow
    return m


MATS = [("Lantern_iron", (0.13, 0.13, 0.14), 0.85, 0.5, 0.0),
        ("Lantern_glass", (1.0, 0.72, 0.38), 0.0, 0.3, 2.5),
        ("Lantern_flame", (1.0, 0.85, 0.5), 0.0, 0.5, 6.0)]


def box(bm, lo, hi, slot):
    r = bmesh.ops.create_cube(bm, size=1.0)
    for v in r["verts"]:
        v.co = Vector(((lo[0] + hi[0]) / 2 + v.co.x * (hi[0] - lo[0]),
                       (lo[1] + hi[1]) / 2 + v.co.y * (hi[1] - lo[1]),
                       (lo[2] + hi[2]) / 2 + v.co.z * (hi[2] - lo[2])))
    for f in {f for v in r["verts"] for f in v.link_faces}:
        f.material_index = slot


def centred(bm, half, z0, z1, slot):
    box(bm, (C.x - half[0], C.y - half[1], z0), (C.x + half[0], C.y + half[1], z1), slot)


def ring(bm, centre, radius, thick, axis_x, slot, n=10):
    # A torus of n boxes, in the plane holding Z and X (axis_x) or Z and Y.
    for i in range(n):
        a = 2 * math.pi * i / n
        d = Vector((math.cos(a), 0, math.sin(a))) if axis_x else Vector((0, math.cos(a), math.sin(a)))
        r = bmesh.ops.create_cube(bm, size=1.0)
        rot = Matrix.Rotation(a, 3, "Y" if axis_x else "X")
        for v in r["verts"]:
            local = Vector((v.co.x * thick, v.co.y * thick, v.co.z * radius * 2 * math.pi / n))
            if not axis_x:
                local = Vector((v.co.x * thick, v.co.y * thick, v.co.z * radius * 2 * math.pi / n))
            v.co = centre + d * radius + (rot @ Matrix.Rotation(math.pi / 2, 3, "Y" if axis_x else "X") @ local)
        for f in {f for v in r["verts"] for f in v.link_faces}:
            f.material_index = slot


def lantern(bm, z):
    # Base, posts, panes, flame, roof, knob and ring, from z up.
    centred(bm, (0.17, 0.17), z, z + 0.04, 0)
    centred(bm, (0.15, 0.15), z + 0.04, z + 0.06, 0)
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = C.x + sx * 0.135, C.y + sy * 0.135
            box(bm, (x - 0.02, y - 0.02, z + 0.06), (x + 0.02, y + 0.02, z + 0.34), 0)
    centred(bm, (0.12, 0.12), z + 0.06, z + 0.34, 1)       # the glass, glowing
    centred(bm, (0.03, 0.03), z + 0.08, z + 0.20, 2)       # the flame on its wick
    centred(bm, (0.16, 0.16), z + 0.34, z + 0.37, 0)
    for k, half in enumerate((0.13, 0.10, 0.07, 0.045)):    # the roof, stepped
        centred(bm, (half, half), z + 0.37 + k * 0.025, z + 0.395 + k * 0.025, 0)
    centred(bm, (0.025, 0.025), z + 0.47, z + 0.50, 0)
    ring(bm, Vector((C.x, C.y, z + 0.545)), 0.04, 0.012, True, 0)


def build(name, hung):
    bm = bmesh.new()
    if hung:
        lantern(bm, 0.32)
        # The chain: links turned a quarter about each other, to the cell's top.
        z = 0.88
        k = 0
        while z < 1.0:
            ring(bm, Vector((C.x, C.y, z + 0.02)), 0.022, 0.008, k % 2 == 0, 0, 8)
            z += 0.035
            k += 1
    else:
        lantern(bm, 0.0)
    me = bpy.data.meshes.new(name)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    for m in MATS:
        me.materials.append(material(*m))
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def main(export=True):
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    obs = {"lantern": build("lantern", False), "lantern_hung": build("lantern_hung", True)}
    obs["lantern_hung"].location = (1.3, 0, 0)
    if not export:
        return obs
    for name, ob in obs.items():
        at = ob.location.copy()
        ob.location = (0, 0, 0)
        for o in bpy.data.objects:
            o.select_set(o is ob)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, name + ".gltf"), export_format="GLTF_SEPARATE",
                                  use_selection=True, export_yup=True, export_apply=True)
        ob.location = at
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    return obs


if __name__ == "__main__":
    main()
