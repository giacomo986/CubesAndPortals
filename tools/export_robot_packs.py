# Exports the two things worn on the robot's back from the robot's Blender file.
#
#   blender ~/kart_assets/robot18.blend --background --python tools/export_robot_packs.py
#
# Collections Jetpack (the flight pack, which used to be part of Robot_Mesh)
# and Backpack, each one mesh on Robot_Rig: the jetpack bound to "chest" with
# its flames on "jet_flame", the backpack all on "chest". Exported with the rig
# and no clips; the client poses a worn model with its wearer's bones, matched
# by name (draw_worn), so the robot's clips are what move them.
import os

import bpy

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

if bpy.context.window:   # in the background the file opens on the scene it was saved on
    bpy.context.window.scene = bpy.data.scenes["Scene"]
rig = bpy.data.objects["Robot_Rig"]
rig.data.pose_position = "REST"


def export(mesh, path):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob = bpy.data.objects[mesh]
    ob.hide_set(False)
    ob.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_skins=True, export_apply=False,
                              export_yup=True, export_materials="EXPORT",
                              export_image_format="NONE")


export("Jetpack_Mesh", os.path.join(ROOT, "mods", "jetpack", "jetpack.gltf"))
export("Backpack_Mesh", os.path.join(ROOT, "mods", "backpack", "backpack.gltf"))
