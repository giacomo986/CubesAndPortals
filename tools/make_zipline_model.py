# The zipline of the assault (assault_core:zipline), built in Blender out of boxes.
#
#   flatpak run org.blender.Blender --background --python tools/make_zipline_model.py
#
# Exports games/assault/mods/assault_core/zipline.gltf. One cell of a slide
# (core.node_conveyor 7, 1.25, -1): at facing 0 the body comes in high at +z
# and goes out low at -z, sat on the line from y 1.25 down to 0.25. The cable
# runs over that line at the height of the hands, so a body riding it hangs
# from it; cells one after another, a cell down each, are one cable. A clamp
# where each cell's cable meets the next.
import math
import os

import bmesh
import bpy

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "games", "assault", "mods", "assault_core", "zipline.gltf")

HANDS = 2.15   # over the bed's line
R = 0.05       # the cable's half thickness

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


# Bright, to be seen down a dark pit: a rope in the hazard's yellow, steel clamps.
STEEL = material("zip_rope", (0.95, 0.78, 0.18), 0.0, 0.6)
CLAMP = material("zip_clamp", (0.75, 0.76, 0.8), 0.8, 0.35)
MATS = [STEEL, CLAMP]
bm = bmesh.new()


def quad_box(corners, mat):
    """Eight corners in glTF (y up) coordinates, index x*4 + y*2 + z as a box's;
    Blender is z up and the exporter turns its (x, y, z) into (x, z, -y)."""
    vs = [bm.verts.new((x, -z, y)) for x, y, z in corners]
    for f in [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]:
        face = bm.faces.new([vs[i] for i in f])
        face.material_index = MATS.index(mat)


def box(x0, y0, z0, x1, y1, z1, mat):
    quad_box([(x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)], mat)


# The cable: a thin bar along the slope, from (z 1, y 1.25) to (z 0, y 0.25),
# square to it, a little past each end so cells meet without a gap.
over = 0.02
corners = []
for x in (0.5 - R, 0.5 + R):
    for dy in (-R, R):
        for z in (-over, 1.0 + over):
            # Down one along y for every one along z; the bar's thickness square to the slope.
            y = 0.25 + HANDS + z + dy * math.sqrt(2)
            corners.append((x, y, z))
quad_box(corners, STEEL)
# A clamp round it at the cell's high end.
box(0.5 - 2 * R, 1.25 + HANDS - 2.5 * R, 0.94, 0.5 + 2 * R, 1.25 + HANDS + 2.5 * R, 1.0, CLAMP)

bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
mesh = bpy.data.meshes.new("Zipline")
bm.to_mesh(mesh)
bm.free()
for m in MATS:
    mesh.materials.append(m)
zip_obj = bpy.data.objects.new("Zipline", mesh)
bpy.context.scene.collection.objects.link(zip_obj)
bpy.context.view_layer.update()
zip_obj.select_set(True)
bpy.context.view_layer.objects.active = zip_obj
bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLTF_SEPARATE", use_selection=True,
                          export_animations=False, export_yup=True, export_materials="EXPORT",
                          export_image_format="NONE")
print("zipline: wrote", OUT)
