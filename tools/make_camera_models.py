# The security camera of mods/cameras, in pieces, exported beside the mod.
#
#   flatpak run org.blender.Blender --background --python tools/make_camera_models.py
#
#   camera_stand.gltf    the block's own model (core.node_model): a plate, an
#                        arm and a joint, out of the face the camera is put
#                        against (wallmounted: the floor, a wall, a ceiling).
#                        It does not turn; the head does.
#   camera_head.gltf     the head, drawn by each player's machine where the
#   camera_head_on.gltf  camera is turned (core.render.model): a grey box
#                        with a hood, the lens out of its front and a lamp on
#                        top -- dark, or lit red while the camera sees somebody.
#                        Its origin is the pivot, in the fork; the lens looks
#                        along +Z of the glTF (Blender -Y), as a block's front.
#   camera_full.gltf     stand and head together, as placed: what the hand
#                        holds and the bag shows (tools/make_held_models.py).
#
# Blender world, metres = nodes, +Z up; exported Y up.
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "cameras")
PIVOT = Vector((0.5, 0.5, 0.55))  # the head's pivot over the floor (Blender: x, y, up)
CELL = Vector((0.0, -1.0, 0.0))   # from y 0..1, as the parts are drawn, to the glTF's z 0..1


def material(name, rgb, rough=0.6, metal=0.0, glow=False):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metal
    if glow:
        p.inputs["Emission Color"].default_value = (*rgb, 1)
        p.inputs["Emission Strength"].default_value = 1.0
    m.diffuse_color = (*rgb, 1)
    return m


def box(bm, lo, hi, mat):
    ret = bmesh.ops.create_cube(bm, size=1.0)
    for v in ret["verts"]:
        v.co = Vector(((lo[0] + hi[0]) / 2 + v.co.x * (hi[0] - lo[0]),
                       (lo[1] + hi[1]) / 2 + v.co.y * (hi[1] - lo[1]),
                       (lo[2] + hi[2]) / 2 + v.co.z * (hi[2] - lo[2])))
    for f in {f for v in ret["verts"] for f in v.link_faces}:
        f.material_index = mat


def cyl_y(bm, centre, r, y0, y1, mat, segs=16):
    """A cylinder along +Y from y0 to y1 about (centre.x, centre.z)."""
    ret = bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r, radius2=r, depth=y1 - y0)
    bmesh.ops.rotate(bm, verts=ret["verts"], cent=(0, 0, 0), matrix=Matrix.Rotation(math.radians(90), 3, "X"))
    bmesh.ops.translate(bm, verts=ret["verts"], vec=(centre[0], (y0 + y1) / 2, centre[1]))
    for f in {f for v in ret["verts"] for f in v.link_faces}:
        f.material_index = mat


def obj(name, mats, build, at=(0, 0, 0)):
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for m in mats:
        me.materials.append(bpy.data.materials[m])
    bm = bmesh.new()
    build(bm)
    bmesh.ops.translate(bm, verts=bm.verts, vec=at)
    bm.to_mesh(me)
    bm.free()
    return ob


STAND_MATS = ["Cam_dark", "Cam_metal"]


def stand(bm):
    """Upright, feet on the floor: a plate, an arm up and the joint the head
    turns on, its top at 0.44 and the head's pivot at 0.55."""
    box(bm, (0.28, 0.28, 0.0), (0.72, 0.72, 0.06), 0)          # the plate
    box(bm, (0.45, 0.45, 0.06), (0.55, 0.55, 0.38), 1)         # the arm
    box(bm, (0.42, 0.42, 0.36), (0.58, 0.58, 0.45), 1)         # the joint


# The block is wallmounted (mods/cameras): its front, the glTF's +Z, points out
# of the face it was put against. So the stand's up has to be that front: its
# plate on the back face, z 0, and the arm out along +Z -- in Blender, whose -Y
# the export makes +Z, the plate at y 0 and the arm towards -y. Up (Blender z)
# x stays.
TO_BLOCK = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))   # (x, y, z) -> (x, -z, y)


HEAD_MATS = ["Cam_case", "Cam_dark", "Cam_lens", "Cam_metal", "Cam_lamp"]


def head(lamp_on):
    def build(bm):
        # About the pivot: the box, its hood over the front, the lens.
        box(bm, (-0.125, -0.2, -0.1), (0.125, 0.24, 0.11), 0)
        box(bm, (-0.14, -0.3, 0.11), (0.14, -0.12, 0.135), 1)  # the hood
        cyl_y(bm, (0.0, 0.0), 0.08, -0.27, -0.2, 3)              # the lens' ring
        cyl_y(bm, (0.0, 0.0), 0.06, -0.285, -0.27, 2)            # its glass
        box(bm, (-0.04, 0.1, 0.11), (0.04, 0.18, 0.15), 4)       # the lamp
    return build


def export(objs, path):
    for o in bpy.data.objects:
        o.select_set(o in objs)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_yup=True, export_apply=True, export_animations=False)
    print("wrote", path)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    material("Cam_dark", (0.12, 0.12, 0.14))
    material("Cam_metal", (0.55, 0.57, 0.60), rough=0.35, metal=0.8)
    material("Cam_case", (0.80, 0.81, 0.80), rough=0.5)
    material("Cam_lens", (0.10, 0.18, 0.32), rough=0.05)
    material("Cam_lamp_off", (0.25, 0.05, 0.05))
    material("Cam_lamp_lit", (1.0, 0.12, 0.08), glow=True)

    st = obj("camera_stand", STAND_MATS, stand)
    st.data.transform(TO_BLOCK)
    export([st], os.path.join(OUT, "camera_stand.gltf"))
    for lit in (False, True):
        mats = HEAD_MATS[:4] + ["Cam_lamp_lit" if lit else "Cam_lamp_off"]
        hd = obj("camera_head_on" if lit else "camera_head", mats, head(lit))
        export([hd], os.path.join(OUT, "camera_head_on.gltf" if lit else "camera_head.gltf"))
        if not lit:
            # The whole, as placed: the head in its fork, looking ahead (-Y is
            # the block's front, +Z of the glTF).
            # The whole, standing on a floor, the lens to the front (+Z of the
            # glTF): the bag's picture and the hand's (make_held_models.py).
            full_head = obj("camera_full_head", mats, head(False), at=PIVOT + CELL)
            full_stand = obj("camera_full", STAND_MATS, stand, at=CELL)
            bpy.ops.object.select_all(action="DESELECT")
            full_head.select_set(True)
            full_stand.select_set(True)
            bpy.context.view_layer.objects.active = full_stand
            bpy.ops.object.join()
            export([full_stand], os.path.join(OUT, "camera_full.gltf"))


main()
