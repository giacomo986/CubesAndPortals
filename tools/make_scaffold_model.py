# The scaffold of mods/building (building:scaffold), built in Blender out of
# poles and planks.
#
#   flatpak run org.blender.Blender --background --python tools/make_scaffold_model.py
#
# Saves the work as assets/blender/scaffold.blend and exports
# mods/building/scaffold.gltf, the same scaffold held in the hand
# (scaffold_held.gltf, a third of the size about the fist, as
# tools/make_ladder_model.py does it) and its picture in the bag
# (assets/textures/building_scaffold.png, 32 px, outlined). With
# PREVIEW=<file.png> it also renders three stacked, from a corner.
#
# The model is one cell, x y z 0..1 (y up, as the glTF is): four square poles
# at the corners the whole height, so scaffolds stacked are one tower; a cross
# of braces on each side, lashed at the middle; a deck of planks across the
# top with a gap between them, and a rope lashing at each pole's top.
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "building", "scaffold.gltf")
HELD = os.path.join(ROOT, "mods", "building", "scaffold_held.gltf")
ICON = os.path.join(ROOT, "assets", "textures", "building_scaffold.png")
BLEND = os.path.join(ROOT, "assets", "blender", "scaffold.blend")

bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name, rgb, rough=0.85):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1.0)
    return m


POLE = material("pole", (0.45, 0.30, 0.16))
PLANK = material("plank", (0.70, 0.52, 0.30))
ROPE = material("rope", (0.78, 0.70, 0.50))
MATS = [POLE, PLANK, ROPE]

bm = bmesh.new()


def g2b(p):
    """glTF (y up) to Blender (z up): the exporter turns (x, y, z) into (x, z, -y)."""
    return Vector((p[0], -p[2], p[1]))


FACES = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]


def box(x0, y0, z0, x1, y1, z1, mat):
    vs = [bm.verts.new(g2b((x, y, z))) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]
    for f in FACES:
        bm.faces.new([vs[i] for i in f]).material_index = MATS.index(mat)


def beam(a, b, t, mat):
    """A square beam t thick from a to b (glTF coordinates)."""
    a, b = Vector(a), Vector(b)
    d = (b - a).normalized()
    up = Vector((0, 1, 0)) if abs(d.y) < 0.9 else Vector((1, 0, 0))
    u = d.cross(up).normalized() * (t / 2)
    v = d.cross(u).normalized() * (t / 2)
    vs = []
    for e in (a, b):
        for su in (-1, 1):
            for sv in (-1, 1):
                vs.append(bm.verts.new(g2b(e + u * su + v * sv)))
    # vs index: end*4 + su*2 + sv, the same order as box's x*4 + y*2 + z
    for f in FACES:
        bm.faces.new([vs[i] for i in f]).material_index = MATS.index(mat)


P = 0.09                      # a pole's side
LO, HI = 0.02, 0.98 - P       # the poles' near corners
for x in (LO, HI):
    for z in (LO, HI):
        box(x, 0.0, z, x + P, 1.0, z + P, POLE)
        box(x - 0.01, 0.90, z - 0.01, x + P + 0.01, 0.96, z + P + 0.01, ROPE)   # the lashing
# A cross of braces on each side, just outside the poles' faces.
c0, c1 = LO + P / 2, HI + P / 2
for side in range(4):
    for (ya, yb) in ((0.08, 0.86), (0.86, 0.08)):
        if side == 0:
            a, b = (c0, ya, LO - 0.02), (c1, yb, LO - 0.02)
        elif side == 1:
            a, b = (c0, ya, HI + P + 0.02), (c1, yb, HI + P + 0.02)
        elif side == 2:
            a, b = (LO - 0.02, ya, c0), (LO - 0.02, yb, c1)
        else:
            a, b = (HI + P + 0.02, ya, c0), (HI + P + 0.02, yb, c1)
        beam(a, b, 0.045, POLE)
# The deck: five planks across x with gaps, on two bearers along x.
for z in (LO, HI):
    box(0.0, 0.86, z, 1.0, 0.90, z + P, POLE)
for i in range(5):
    z0 = 0.01 + i * 0.198
    box(0.0, 0.90, z0, 1.0, 0.95, z0 + 0.18, PLANK)

bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
mesh = bpy.data.meshes.new("Scaffold")
bm.to_mesh(mesh)
bm.free()
for m in MATS:
    mesh.materials.append(m)
scaffold = bpy.data.objects.new("Scaffold", mesh)
bpy.context.scene.collection.objects.link(scaffold)

os.makedirs(os.path.dirname(BLEND), exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=BLEND)


def export(obj, path):
    for o in bpy.context.view_layer.objects:
        o.select_set(o == obj)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_yup=True, export_materials="EXPORT",
                              export_image_format="NONE")
    print("scaffold: wrote", path)


export(scaffold, OUT)

# In the hand: the cell's middle on the fist, a third of the size.
held = bpy.data.objects.new("building_scaffold_held", mesh.copy())
held.data.transform(Matrix.Scale(0.3, 4) @ Matrix.Translation((-0.5, 0.5, -0.5)))
bpy.context.scene.collection.objects.link(held)
bpy.context.view_layer.update()
export(held, HELD)

# The bag's picture: from a corner a little above, flat-lit, 32 pixels, outlined.
scene = bpy.context.scene
held.hide_render = True
try:
    scene.render.engine = "BLENDER_WORKBENCH"
except TypeError as e:
    print("scaffold: no workbench:", e)
scene.display.shading.light = "FLAT"
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = "BOTH"
scene.display.shading.color_type = "MATERIAL"
scene.render.film_transparent = True
scene.render.resolution_x = scene.render.resolution_y = 256
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
try:
    scene.view_settings.view_transform = "Raw"
except TypeError as e:
    print("scaffold: view transform:", e)
cam = bpy.data.objects.new("IconCam", bpy.data.cameras.new("IconCam"))
scene.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = 1.75
target = Vector((0.5, -0.5, 0.5))
cam.location = target + Vector((2.0, -2.0, 1.6))
cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
scene.camera = cam
scene.render.filepath = ICON
bpy.ops.render.render(write_still=True)
img = bpy.data.images.load(ICON)
img.scale(32, 32)
px = np.array(img.pixels[:]).reshape(32, 32, 4)
solid = px[:, :, 3] > 0.45
px[:, :, 3] = solid.astype(float)
edge = np.zeros_like(solid)
for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
    edge |= np.roll(solid, (dy, dx), axis=(0, 1))
edge &= ~solid
px[edge] = (0.10, 0.07, 0.05, 1.0)
img.pixels[:] = px.ravel()
img.filepath_raw = ICON
img.file_format = "PNG"
img.save()
bpy.data.images.remove(img)
print("scaffold: wrote", ICON)

preview = os.environ.get("PREVIEW")
if preview:
    scene.render.film_transparent = False
    for k in (1, 2):
        o = bpy.data.objects.new(f"Scaffold_{k}", mesh)
        o.location = (0.0, 0.0, float(k))
        scene.collection.objects.link(o)
    pc = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(pc)
    aim = Vector((0.5, -0.5, 1.5))
    pc.location = aim + Vector((3.2, -3.6, 1.2))
    pc.rotation_euler = (aim - pc.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = pc
    scene.render.resolution_x = scene.render.resolution_y = 480
    scene.render.filepath = preview
    bpy.ops.render.render(write_still=True)
    print("scaffold: preview", preview)
