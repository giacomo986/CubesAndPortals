# The arpg's chests as models of their blocks (games/arpg/mods/arpg_core):
# the zones' coffer shut and open, and the town's stash, out of one painted
# sheet of three (assets/meshy/arpg/chests.png, Meshy's text-to-image) made one
# model by image-to-3D in smart topology (assets/meshy/arpg/chests.glb):
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_arpg_chests.py"
#
# Cut apart by where each stands across the sheet (SPANS, pixels), each turned
# to face the camera's side (Blender +Y, the world's -Z: arpg_game's camera
# looks from -x -z), sized to its cell (the open one at the shut one's size, its
# lid standing up out of the cell), feet on 0 in the unit cube, decimated, its
# colour baked (tools/meshy_prop.py). Writes prop_coffer, prop_coffer_open and
# prop_stash .gltf + .bin + .png; arpg_core puts them on the blocks
# (core.node_model). A look: ~/.cache/cnp_look/chests.png.
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from meshy_prop import bake, decimate, texture  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "assets", "meshy", "arpg", "chests.glb")
OUT = os.path.join(ROOT, "games", "arpg", "mods", "arpg_core")
LOOK = os.path.expanduser("~/.cache/cnp_look")
TRIS, TEX, COLOURS = 2500, 256, 48
# name, pixels across the sheet, width in the cell (the open one: the shut one's scale)
SPANS = [("prop_coffer", (45, 445), 1.0), ("prop_coffer_open", (478, 880), None), ("prop_stash", (912, 1355), 1.15)]


def take(ob, x0, x1, name):
    me = ob.data.copy()
    cp = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(cp)
    bm = bmesh.new()
    bm.from_mesh(me)
    gone = [f for f in bm.faces if not (x0 <= f.calc_center_median().x < x1)]
    bmesh.ops.delete(bm, geom=gone, context="FACES")
    bm.to_mesh(me)
    bm.free()
    return cp


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=SRC)
    src = next(o for o in bpy.context.scene.objects if o.type == "MESH")
    src.parent = None
    for x in bpy.data.objects:
        x.select_set(x is src)
    bpy.context.view_layer.objects.active = src
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    xs = np.array([v.co.x for v in src.data.vertices])
    lo, hi = xs.min(), xs.max()
    p0, p1 = SPANS[0][1][0], SPANS[-1][1][1]
    at = lambda px: lo + (px - p0) / (p1 - p0) * (hi - lo)
    cuts = [lo - 1] + [at((a[1][1] + b[1][0]) / 2) for a, b in zip(SPANS, SPANS[1:])] + [hi + 1]
    scale = None
    made = []
    for (name, _, width), x0, x1 in zip(SPANS, cuts, cuts[1:]):
        high = take(src, x0, x1, name + "_high")
        vs = np.array([v.co[:] for v in high.data.vertices])
        blo, bhi = vs.min(0), vs.max(0)
        s = width / max(bhi[0] - blo[0], bhi[1] - blo[1]) if width else scale
        scale = scale or s
        c = (blo + bhi) / 2
        # half round (front from -Y to +Y), sized, its feet on 0, in the middle of the
        # cell: Blender (0.5, -0.5) is the glTF cell's (0.5, 0.5), its y being -z
        m = (Matrix.Translation((0.5, -0.5, 0)) @ Matrix.Rotation(math.pi, 4, "Z") @ Matrix.Scale(s, 4) @
             Matrix.Translation((-c[0], -c[1], -blo[2])))
        high.data.transform(m)
        low = bpy.data.objects.new(name, high.data.copy())
        bpy.context.scene.collection.objects.link(low)
        decimate(low, TRIS)
        img = bake(high, low, name, TEX)
        texture(img, COLOURS, os.path.join(OUT, name + ".png"))
        for x in bpy.data.objects:
            x.select_set(x is low)
        bpy.context.view_layer.objects.active = low
        bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, name + ".gltf"), export_format="GLTF_SEPARATE",
                                  use_selection=True, export_image_format="AUTO", export_apply=True,
                                  export_animations=False, export_yup=True)
        for ext in (".gltf", ".bin", ".png"):
            assert os.path.getsize(os.path.join(OUT, name + ext)) < 1 << 20, f"{name}{ext} is over 1 MB"
        vs = np.array([v.co[:] for v in low.data.vertices])
        print(f"CHEST {name}: {len(low.data.polygons)} triangles, box {vs.min(0).round(2)} {vs.max(0).round(2)}")
        made.append(low)
    bpy.data.objects.remove(src)
    look(made)


def look(made):
    scene = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = next((e for e in engines if "EEVEE" in e), "CYCLES")
    world = bpy.data.worlds.new("w")
    world.color = (0.6, 0.6, 0.62)
    scene.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 4
    sun.rotation_euler = (0.8, 0.2, 2.6)
    scene.collection.objects.link(sun)
    for i, ob in enumerate(made):
        ob.location.x += i * 1.4
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    # as arpg_game's camera: from -x -z of the world, which is Blender's -x +y, high up
    target = Vector((1.9, -0.5, 0.4))
    cam.location = target + Vector((-3.0, 3.0, 4.2))
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.resolution_x, scene.render.resolution_y = 900, 500
    os.makedirs(LOOK, exist_ok=True)
    scene.render.filepath = os.path.join(LOOK, "chests.png")
    bpy.ops.render.render(write_still=True)
    print("LOOK", scene.render.filepath)


main()
