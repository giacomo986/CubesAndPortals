# Exports the creatures from their Blender file.
#
#   run inside assets/blender/mobs.blend (Text editor, or the MCP bridge)
#
# One armature and one mesh per creature: Zombie_Rig/Zombie_Mesh and
# Skeleton_Rig/Skeleton_Mesh, on the robot's skeleton (same bones, same 1.8
# height, front +Y) so every clip the robot has works on them. Their clips are
# NLA tracks named as the engine asks for them ("idle", "walk", "attack",
# "aim", "shoot"...), each playing a zombie_*/skeleton_* action: a clip name is
# per model and an action name is per file.
#
# Each gets its "death" clip here, before it is exported (tip_over in
# tools/make_pet_models.py, over onto its side), and the file is saved with it.
#
#   blender assets/blender/mobs.blend --background --python tools/export_mob_models.py
import importlib.util
import os
import sys

import bpy

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "mobs")


def export(rig_name, mesh_name, path):
    rig = bpy.data.objects[rig_name]
    ad = rig.animation_data
    ad.action = None                      # the tracks, and nothing laid over them
    for t in ad.nla_tracks:
        t.mute = False
    saved = rig.location.copy()
    rig.location = (0.0, 0.0, 0.0)
    bpy.context.view_layer.update()
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    for n in (rig_name, mesh_name):
        bpy.data.objects[n].hide_set(False)
        bpy.data.objects[n].select_set(True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS",
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_yup=True, export_apply=False, export_materials="EXPORT",
                              export_image_format="NONE", export_skins=True,
                              export_def_bones=False)
    rig.location = saved


_spec = importlib.util.spec_from_file_location(
    "make_pet_models", os.path.join(os.path.dirname(os.path.abspath(__file__)), "make_pet_models.py"))
pets = importlib.util.module_from_spec(_spec)
sys.modules["make_pet_models"] = pets
_spec.loader.exec_module(pets)
for rig, mesh in (("Zombie_Rig", "Zombie_Mesh"), ("Skeleton_Rig", "Skeleton_Mesh")):
    pets.tip_over(bpy.data.objects[rig], [bpy.data.objects[mesh]])

os.makedirs(OUT, exist_ok=True)
export("Zombie_Rig", "Zombie_Mesh", os.path.join(OUT, "zombie.gltf"))
export("Skeleton_Rig", "Skeleton_Mesh", os.path.join(OUT, "skeleton.gltf"))

# The arrow a skeleton shoots: no bones, nose along +Y (the engine's forward),
# its middle at the origin -- the arrow's body is placed by that point.
arrow = bpy.data.objects["Arrow_Mesh"]
kept = arrow.location.copy()
arrow.location = (0.0, 0.0, 0.0)
for o in bpy.context.view_layer.objects:
    o.select_set(False)
arrow.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, "arrow.gltf"), export_format="GLTF_SEPARATE",
                          use_selection=True, export_animations=False, export_yup=True,
                          export_materials="EXPORT", export_image_format="NONE")
arrow.location = kept
bpy.ops.wm.save_mainfile()
