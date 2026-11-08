# Bag icons of the guns that only had a name in the slot: the front's and the
# assault's, rendered from their own <gun>_held models, in profile, muzzle to
# the right, the way tools/make_held_models.py renders its icons.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_gun_icons.py"
#
# Written to assets/textures, which those two rooms read.
import ast
import math
import os

import bpy
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
GUNS = [("front_game", ("pistol", "rifle", "lmg", "sniper", "launcher", "grenade"), "front"),
        ("assault_game", ("pistol", "rifle", "lmg", "sniper", "launcher", "grenade", "wrench"), "assault")]

# Only render_icon of tools/make_held_models.py: importing it builds every model.
tree = ast.parse(open(os.path.join(HERE, "make_held_models.py")).read())
fn = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "render_icon")
scope = {"bpy": bpy, "math": math, "np": np, "Vector": Vector}
exec(compile(ast.Module([fn], []), "render_icon", "exec"), scope)
render_icon = scope["render_icon"]

# The gun's +Y (the muzzle) to the picture's right, its +Z (the sights) up.
PROFILE = [math.degrees(a) for a in Matrix(((0, 1, 0), (0, 0, 1), (1, 0, 0))).to_euler("XYZ")]

for mod, guns, game in GUNS:
    for gun in guns:
        bpy.ops.wm.read_homefile(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, "games", game, "mods", mod, gun + "_held.gltf"))
        meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        for o in meshes:
            for m in o.data.materials:
                p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
                m.diffuse_color = (*p.inputs["Base Color"].default_value[:3], 1.0)
        bpy.ops.object.select_all(action="DESELECT")
        for o in meshes:
            o.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        if len(meshes) > 1:
            bpy.ops.object.join()
        obj = bpy.context.view_layer.objects.active
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        obj.rotation_mode = "XYZ"   # the importer leaves quaternions, which render_icon's turn would not move
        render_icon(obj, os.path.abspath(os.path.join(ROOT, "assets", "textures", f"{mod}_{gun}.png")), PROFILE)
print("gun icons written")
