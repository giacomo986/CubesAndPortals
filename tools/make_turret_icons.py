# The tower defense's five turrets as things in a bag: an icon each (they had
# none -- the block is invisible, the model is drawn by the client) and a
# small copy to hold in the hand, both from the turret's own whole model.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_turret_icons.py"
#
# Icons: games/defense/textures/defense_game_turret_<name>.png, seen from the
# front and a little above, as tools/make_held_models.py renders its icons.
# Held: games/defense/mods/defense_game/turret_<name>_held.gltf, HELD_SIZE
# tall, its foot in the fist (the grip's origin, +Y up as held).
import ast
import math
import os

import bpy
import numpy as np
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
MOD = os.path.join(ROOT, "games", "defense", "mods", "defense_game")
TEXTURES = os.path.join(ROOT, "games", "defense", "textures")
TURRETS = ("arrow", "brazier", "frost", "lightning", "portal")
HELD_SIZE = 0.32   # metres of the 1.357 robot: a miniature, not a 1 m turret in the palm

# render_icon of tools/make_held_models.py, coloured by the texture: the
# turrets are painted by their PNGs, and by material they came out white.
src = open(os.path.join(HERE, "make_held_models.py")).read()
fn = next(n for n in ast.parse(src).body if isinstance(n, ast.FunctionDef) and n.name == "render_icon")
code = ast.get_source_segment(src, fn).replace('color_type = "MATERIAL"', 'color_type = "TEXTURE"')
scope = {"bpy": bpy, "math": math, "np": np, "Vector": Vector}
exec(code, scope)
render_icon = scope["render_icon"]

# render_icon looks down -Z with +Y up; the glTF comes in Z-up. Turned 30
# degrees about its own up first (rotation mode ZXY: Z, then X), then stood
# up facing the camera and tipped 20 towards it: three quarters from above.
TURN = (-70.0, 0.0, 30.0)

for name in TURRETS:
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(MOD, f"turret_{name}.gltf"))
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    for o in meshes:
        for m in o.data.materials:
            if m is None or m.node_tree is None:
                continue
            p = next((n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
            if p is not None:
                m.diffuse_color = (*p.inputs["Base Color"].default_value[:3], 1.0)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    # Without its armature, if it had one: a rigid thing to hold.
    for mod in list(obj.modifiers):
        obj.modifiers.remove(mod)
    obj.parent = None
    for o in list(bpy.context.scene.objects):
        if o is not obj:
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    obj.rotation_mode = "XYZ"

    # The held copy: HELD_SIZE tall, the middle of its foot at the origin.
    pts = [Vector(c) for c in obj.bound_box]
    tall = max(p.z for p in pts) - min(p.z for p in pts)
    k = HELD_SIZE / max(tall, 1e-6)
    cx = (max(p.x for p in pts) + min(p.x for p in pts)) / 2
    cy = (max(p.y for p in pts) + min(p.y for p in pts)) / 2
    z0 = min(p.z for p in pts)
    obj.data.transform(__import__("mathutils").Matrix.Translation((-cx, -cy, -z0)))
    obj.data.transform(__import__("mathutils").Matrix.Scale(k, 4))
    obj.rotation_euler = (0, 0, 0)
    bpy.context.view_layer.update()
    bpy.ops.export_scene.gltf(filepath=os.path.join(MOD, f"turret_{name}_held.gltf"),
                              export_format="GLTF_SEPARATE", use_selection=True, export_yup=True,
                              export_apply=True, export_animations=False, export_skins=False)

    obj.rotation_mode = "ZXY"
    render_icon(obj, os.path.abspath(os.path.join(TEXTURES, f"defense_game_turret_{name}.png")), TURN)
print("turret icons and held models written")
