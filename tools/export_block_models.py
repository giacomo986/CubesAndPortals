# Exports the chest, furnace and crafting table block models from their Blender file.
#
#   blender assets/blender/chest_furnace.blend --background --python tools/export_block_models.py
#
# One collection per model in the .blend: Chest, ChestDouble, Hidden, Furnace,
# CraftTable.
# Blender names every exported clip "Animation"; a block model's clips are
# found by name ("open" held at how far the cell is open, "loop" played on the
# clock), so the name is written into the file after the export.
import json
import os

import bpy

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
CHEST = os.path.join(ROOT, "mods", "chest")
FURNACE = os.path.join(ROOT, "mods", "furnace")
DOORS = os.path.join(ROOT, "mods", "doors")
CRAFT = os.path.join(ROOT, "mods", "craft")


def export(coll, path, rig_x=0.0, hide=(), clip=None):
    roots = [o for o in coll.objects if o.parent is None]
    saved = {o: o.location.x for o in roots}
    for o in roots:
        o.location.x = rig_x   # each model is laid out beside the others; exported at its cell
    for o in coll.objects:
        o.hide_set(o.name in hide)
    bpy.context.view_layer.update()
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    for o in coll.objects:
        if o.name not in hide:
            o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=clip is not None,
                              export_animation_mode="ACTIVE_ACTIONS", export_force_sampling=True,
                              export_optimize_animation_size=False, export_yup=True,
                              export_apply=False, export_materials="EXPORT",
                              export_image_format="NONE", export_skins=True)
    for o, x in saved.items():
        o.location.x = x
    for o in coll.objects:
        o.hide_set(False)

    if clip is not None:
        with open(path) as f:
            doc = json.load(f)
        doc["animations"][0]["name"] = clip
        with open(path, "w") as f:
            json.dump(doc, f, separators=(",", ":"))


C = bpy.data.collections
export(C["Chest"], os.path.join(CHEST, "chest.gltf"), clip="open")
export(C["ChestDouble"], os.path.join(CHEST, "chest_double_r.gltf"), 0.0, clip="open")   # x 0..2
export(C["ChestDouble"], os.path.join(CHEST, "chest_double_l.gltf"), -1.0, clip="open")  # x -1..1
export(C["Hidden"], os.path.join(CHEST, "chest_half.gltf"))
fire = [o.name for o in C["Furnace"].objects
        if o.name.startswith("Furnace_flame") or o.name == "Furnace_embers"]
export(C["Furnace"], os.path.join(FURNACE, "furnace.gltf"), hide=fire)
export(C["Furnace"], os.path.join(FURNACE, "furnace_lit.gltf"), clip="loop")


# The door: one mesh two cells tall, shut against the back of its cell and
# open along its left side (the hinge). No bones; each is exported at the
# origin, which is the corner of the cell it stands in.
def export_mesh(name, path):
    ob = bpy.data.objects[name]
    kept = ob.location.copy()
    ob.location = (0.0, 0.0, 0.0)
    bpy.context.view_layer.update()
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_yup=True, export_materials="EXPORT",
                              export_image_format="NONE")
    ob.location = kept


os.makedirs(DOORS, exist_ok=True)
export_mesh("Door_Mesh", os.path.join(DOORS, "door.gltf"))
export_mesh("DoorOpen_Mesh", os.path.join(DOORS, "door_open.gltf"))
export(C["Hidden"], os.path.join(DOORS, "door_half.gltf"))
export(C["CraftTable"], os.path.join(CRAFT, "table.gltf"))
