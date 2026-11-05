# A bee for mods/bees: boxes in Blender on a three-bone rig, its wings beating,
# exported as mods/bees/bee.gltf.
#
#   blender --background --python tools/make_bee_model.py
#
# A quarter of a metre long, bigger than a bee, or nobody would see one: a
# yellow body with two black bands and a black sting, a black head with two
# feelers, and two pale wings on its back. Front +Y, its feet on z = 0, like
# the creatures of mods/mobs (tools/make_pet_models.py's conventions: every
# bone points up, so a wing beats about the body's long axis, Y).
#
# Clips idle, walk and run, the names the engine plays a moving entity by
# (docs/MODDING.md, core.ecs.visible): all the same beat, a tenth of a second.
# Saves assets/blender/bee.blend.
import math
import os

import bpy

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "bees", "bee.gltf")
BLEND = os.path.join(ROOT, "assets", "blender", "bee.blend")
FPS = 24


def material(name, rgb):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.8
    m.diffuse_color = (*rgb, 1.0)
    return m


def boxes_mesh(name, boxes, mats):
    """boxes: [(bone, material, (x0, y0, z0), (x1, y1, z1))], one mesh, each
    box weighted whole to its bone."""
    verts, faces, face_mat, groups = [], [], [], {}
    order = list(mats)
    for bone, mname, lo, hi in boxes:
        (x0, y0, z0), (x1, y1, z1) = lo, hi
        b = len(verts)
        verts += [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
                  (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        for f in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
            faces.append(tuple(b + i for i in f))
            face_mat.append(order.index(mname))
        groups.setdefault(bone, []).extend(range(b, b + 8))
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    for m in order:
        me.materials.append(mats[m])
    for poly, mi in zip(me.polygons, face_mat):
        poly.material_index = mi
        poly.use_smooth = False
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    for bone, idx in groups.items():
        obj.vertex_groups.new(name=bone).add(idx, 1.0, "REPLACE")
    return obj


def rig(name, bones):
    arm = bpy.data.armatures.new(name)
    obj = bpy.data.objects.new(name, arm)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    for bname, parent, (x, y, z) in bones:
        eb = arm.edit_bones.new(bname)
        eb.head = (x, y, z)
        eb.tail = (x, y, z + 0.05)
        eb.roll = 0.0
        if parent:
            eb.parent = arm.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in obj.pose.bones:
        pb.rotation_mode = "XYZ"
    return obj


def beat(arm, name, seconds):
    """The wings up and down about the body's long axis, looped."""
    act = bpy.data.actions.new(f"bee_{name}")
    arm.animation_data_create()
    arm.animation_data.action = act
    frames = max(2, round(seconds * FPS))
    for t, a in ((0.0, 35), (0.5, -25), (1.0, 35)):
        f = 1 + t * frames
        for bone, side in (("wing_L", -1), ("wing_R", 1)):
            pb = arm.pose.bones[bone]
            pb.rotation_euler = (0, math.radians(a * side), 0)
            pb.keyframe_insert("rotation_euler", frame=f)
        pb = arm.pose.bones["body"]
        pb.rotation_euler = (0, 0, 0)
        pb.keyframe_insert("rotation_euler", frame=f)
    arm.animation_data.action = None
    track = arm.animation_data.nla_tracks.new()
    track.name = name
    track.strips.new(name, 1, act)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mats = {"yellow": material("Bee_yellow", (0.95, 0.72, 0.10)),
            "black": material("Bee_black", (0.08, 0.06, 0.05)),
            "wing": material("Bee_wing", (0.86, 0.92, 0.96))}
    z0 = 0.04   # it hangs in the air off its feet's point
    boxes = [
        # The body, back (-Y) to front, banded.
        ("body", "black", (-0.006, -0.135, z0 + 0.035), (0.006, -0.11, z0 + 0.045)),    # the sting
        ("body", "yellow", (-0.045, -0.11, z0), (0.045, -0.07, z0 + 0.08)),
        ("body", "black", (-0.046, -0.07, z0), (0.046, -0.045, z0 + 0.081)),
        ("body", "yellow", (-0.045, -0.045, z0), (0.045, -0.005, z0 + 0.08)),
        ("body", "black", (-0.046, -0.005, z0), (0.046, 0.02, z0 + 0.081)),
        ("body", "yellow", (-0.045, 0.02, z0), (0.045, 0.06, z0 + 0.08)),
        ("body", "black", (-0.035, 0.06, z0 + 0.008), (0.035, 0.11, z0 + 0.072)),       # the head
        ("body", "black", (-0.025, 0.105, z0 + 0.07), (-0.018, 0.115, z0 + 0.1)),       # feelers
        ("body", "black", (0.018, 0.105, z0 + 0.07), (0.025, 0.115, z0 + 0.1)),
        # The wings, flat on the back, out to either side from its middle.
        ("wing_L", "wing", (-0.12, -0.03, z0 + 0.082), (-0.008, 0.045, z0 + 0.088)),
        ("wing_R", "wing", (0.008, -0.03, z0 + 0.082), (0.12, 0.045, z0 + 0.088)),
    ]
    mesh = boxes_mesh("bee", boxes, mats)
    arm = rig("bee_rig", [("body", None, (0, 0, z0 + 0.04)),
                          ("wing_L", "body", (-0.008, 0.0, z0 + 0.085)),
                          ("wing_R", "body", (0.008, 0.0, z0 + 0.085))])
    mesh.parent = arm
    mesh.modifiers.new("Armature", "ARMATURE").object = arm
    for name in ("idle", "walk", "run"):
        beat(arm, name, 0.125)
    for o in bpy.context.view_layer.objects:
        o.select_set(o in (arm, mesh))
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS",
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_yup=True, export_apply=False, export_materials="EXPORT",
                              export_image_format="NONE", export_skins=True, export_def_bones=False)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("wrote", OUT)


main()
