# The ladder of mods/timber (timber:rungs), built in Blender out of boxes.
#
#   flatpak run org.blender.Blender --background --python tools/make_ladder_model.py
#
# Saves the work as assets/blender/ladder.blend and exports mods/timber/rungs.gltf,
# the same ladder held in the hand (rungs_held.gltf: tools/make_held_models.py's
# way round, flat in XY facing +Z, 0.3 tall about the fist) and its picture in
# the bag (assets/textures/timber_rungs.png, rendered and outlined as that
# script does). With PREVIEW=<file.png> it also renders a look at the block.
#
# The model is one cell: x and y 0..1 (y up, as the glTF is), and everything
# within z 0..0.125, the node box the ladder has against its wall -- the wall
# is -z, the front +z, and the cell's turn (wallmounted) takes both round.
# Two rails the whole height, so ladders stacked are one ladder, and four rungs
# a quarter apart, so the rungs of stacked ones are evenly spaced too, nailed
# to the rails at the front.
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Matrix

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "timber", "rungs.gltf")
HELD = os.path.join(ROOT, "mods", "timber", "rungs_held.gltf")
ICON = os.path.join(ROOT, "assets", "textures", "timber_rungs.png")
BLEND = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "ladder.blend")

bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name, rgb, metal=0.0, rough=0.8):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1.0)
    return m


WOOD = material("wood", (0.62, 0.40, 0.20))
WOOD_DARK = material("wood_dark", (0.42, 0.26, 0.12))
IRON = material("iron", (0.30, 0.30, 0.33), 0.6, 0.5)

bm = bmesh.new()
MATS = [WOOD_DARK, WOOD, IRON]


def box(x0, y0, z0, x1, y1, z1, mat):
    """A box in the cell's own (glTF, y up) coordinates; Blender is z up, and
    the exporter turns its (x, y, z) into the glTF's (x, z, -y)."""
    vs = [bm.verts.new((x, -z, y)) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]
    # vs index: x*4 + y*2 + z
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    for f in faces:
        face = bm.faces.new([vs[i] for i in f])
        face.material_index = MATS.index(mat)


# The rails: two posts the whole height, off the wall by nothing.
for x in (0.0625, 0.8125):
    box(x, 0.0, 0.0, x + 0.125, 1.0, 0.125, WOOD_DARK)
# The rungs, between the rails and set back a little from their front.
for i in range(4):
    y = 0.125 + 0.25 * i
    box(0.1875, y - 0.03125, 0.03125, 0.8125, y + 0.03125, 0.1, WOOD)
    # A nail through each rail into the rung's end.
    for x in (0.109375, 0.859375):
        box(x, y - 0.015625, 0.125, x + 0.03125, y + 0.015625, 0.1328125, IRON)

bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
mesh = bpy.data.meshes.new("Ladder")
bm.to_mesh(mesh)
bm.free()
for m in MATS:
    mesh.materials.append(m)
ladder = bpy.data.objects.new("Ladder", mesh)
bpy.context.scene.collection.objects.link(ladder)

os.makedirs(os.path.dirname(BLEND), exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=BLEND)

for o in bpy.context.view_layer.objects:
    o.select_set(o == ladder)
bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLTF_SEPARATE", use_selection=True,
                          export_animations=False, export_yup=True, export_materials="EXPORT",
                          export_image_format="NONE")
print("ladder: wrote", OUT)

# In the hand: the cell's middle on the fist, its up along +Y and its front
# towards the eye (+Z), a third of the size.
held = bpy.data.objects.new("timber_rungs_held", mesh.copy())
held.data.transform(Matrix.Scale(0.3, 4) @ Matrix.Rotation(math.radians(-90), 4, "X") @
                    Matrix.Translation((-0.5, 0.0625, -0.5)))
bpy.context.scene.collection.objects.link(held)
bpy.context.view_layer.update()
for o in bpy.context.view_layer.objects:
    o.select_set(False)
held.select_set(True)
bpy.ops.export_scene.gltf(filepath=HELD, export_format="GLTF_SEPARATE", use_selection=True,
                          export_animations=False, export_yup=True, export_materials="EXPORT",
                          export_image_format="NONE")
print("ladder: wrote", HELD)

# The bag's picture: the front, flat-lit, 32 pixels, outlined.
scene = bpy.context.scene
held.hide_render = True
try:
    scene.render.engine = "BLENDER_WORKBENCH"
except TypeError as e:
    print("ladder: no workbench:", e)
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
    print("ladder: view transform:", e)
icon_cam = bpy.data.objects.new("IconCam", bpy.data.cameras.new("IconCam"))
scene.collection.objects.link(icon_cam)
icon_cam.data.type = "ORTHO"
icon_cam.data.ortho_scale = 1.05
icon_cam.location = (0.5, -2.0, 0.5)
icon_cam.rotation_euler = (math.radians(90), 0.0, 0.0)
scene.camera = icon_cam
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
px[edge] = (0.10, 0.07, 0.07, 1.0)
img.pixels[:] = px.ravel()
img.filepath_raw = ICON
img.file_format = "PNG"
img.save()
bpy.data.images.remove(img)
print("ladder: wrote", ICON)

preview = os.environ.get("PREVIEW")
if preview:
    scene.render.film_transparent = False
    # Two ladders stacked against a wall, seen from the front and a side.
    top = bpy.data.objects.new("Ladder_top", mesh)
    top.location = (0.0, 0.0, 1.0)
    scene.collection.objects.link(top)
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0.5, 0.5, 1.0))
    wall = bpy.context.active_object
    wall.scale = (3.0, 1.0, 2.0)
    wall.location = (0.5, 0.5, 1.0)
    stone = material("stone", (0.5, 0.5, 0.5))
    wall.data.materials.append(stone)
    cam_data = bpy.data.cameras.new("cam")
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    cam.location = (2.6, -2.8, 1.6)
    d = (0.5 - cam.location[0], 0.0 - cam.location[1], 1.0 - cam.location[2])
    cam.rotation_euler = (math.atan2(math.hypot(d[0], d[1]), -d[2]), 0.0,
                          math.atan2(d[1], d[0]) - math.pi / 2)
    scene.camera = cam
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.rotation_euler = (0.8, 0.2, 0.6)
    scene.collection.objects.link(sun)
    try:
        scene.render.engine = "BLENDER_WORKBENCH"
    except TypeError as e:
        print("ladder: no workbench:", e)
    scene.display.shading.color_type = "MATERIAL"
    scene.render.resolution_x = 480
    scene.render.resolution_y = 480
    scene.render.filepath = preview
    bpy.ops.render.render(write_still=True)
    print("ladder: preview", preview)
