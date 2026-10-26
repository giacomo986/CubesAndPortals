# Builds the sixteen armour pieces on the robot's own rig and exports each one.
#
#   blender --background --python tools/make_armor_models.py
#
# Imports mods/robot/robot.gltf, so the bones are the ones the game poses, and
# makes each piece of bevelled boxes around the robot at rest, every box bound
# whole to one bone. The client draws a worn item as the model of the same name
# with the wearer's pose, matched bone by bone by name (draw_worn). Leaves the
# work in ~/kart_assets/armor.blend. Shapes are in the rig's world space, metres.
import os

import bmesh
import bpy
from mathutils import Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "armor")

# Main colour, trim colour, metallic, roughness.
MATS = {
    "wood": ((0.46, 0.30, 0.16), (0.30, 0.19, 0.10), 0.0, 0.75),
    "gold": ((1.00, 0.76, 0.20), (0.80, 0.52, 0.10), 1.0, 0.28),
    "iron": ((0.78, 0.79, 0.82), (0.48, 0.50, 0.54), 1.0, 0.35),
    "diamond": ((0.36, 0.92, 0.90), (0.16, 0.62, 0.66), 0.3, 0.12),
}

# (bone, low corner, high corner, 0 main / 1 trim). A part on a "_R" bone is
# made again mirrored on the "_L" one.
SHAPES = {
    "helmet": [
        ("head", (-0.245, -0.185, 1.642), (0.245, 0.195, 1.69), 0),
        ("head", (-0.245, -0.185, 1.40), (0.245, -0.152, 1.69), 0),
        ("head", (0.214, -0.185, 1.40), (0.245, 0.195, 1.69), 0),
        ("head", (-0.245, -0.185, 1.40), (-0.214, 0.195, 1.69), 0),
        ("head", (-0.245, 0.162, 1.585), (0.245, 0.198, 1.69), 1),
        ("head", (0.138, 0.162, 1.40), (0.245, 0.198, 1.585), 0),
        ("head", (-0.245, 0.162, 1.40), (-0.138, 0.198, 1.585), 0),
        ("head", (-0.03, -0.2, 1.69), (0.03, 0.2, 1.735), 1),
    ],
    "chestplate": [
        ("chest", (-0.248, 0.186, 0.93), (0.248, 0.222, 1.30), 0),
        ("chest", (-0.035, 0.222, 0.96), (0.035, 0.238, 1.27), 1),
        ("chest", (0.226, -0.15, 0.93), (0.25, 0.222, 1.30), 0),
        ("chest", (-0.25, -0.15, 0.93), (-0.226, 0.222, 1.30), 0),
        ("chest", (-0.17, 0.07, 1.30), (0.17, 0.20, 1.345), 1),
        ("chest", (-0.17, -0.16, 1.30), (0.17, -0.07, 1.345), 1),
        ("chest", (0.07, -0.16, 1.30), (0.17, 0.20, 1.345), 1),
        ("chest", (-0.17, -0.16, 1.30), (-0.07, 0.20, 1.345), 1),
        ("upper_arm_R", (0.17, -0.115, 1.225), (0.375, 0.115, 1.355), 0),
        ("upper_arm_R", (0.355, -0.115, 1.12), (0.378, 0.115, 1.355), 1),
    ],
    "leggings": [
        ("hips", (-0.2, -0.146, 0.70), (0.2, 0.146, 0.835), 0),
        ("hips", (-0.045, 0.146, 0.735), (0.045, 0.162, 0.805), 1),
        ("thigh_R", (0.034, -0.092, 0.44), (0.206, 0.092, 0.70), 0),
        ("shin_R", (0.038, -0.09, 0.33), (0.202, 0.106, 0.45), 0),
        ("shin_R", (0.06, 0.106, 0.35), (0.18, 0.122, 0.44), 1),
    ],
    "boots": [
        ("foot_R", (0.03, -0.112, 0.0), (0.21, 0.215, 0.16), 0),
        ("foot_R", (0.034, 0.2, 0.012), (0.206, 0.228, 0.12), 1),
        ("shin_R", (0.038, -0.092, 0.12), (0.202, 0.108, 0.27), 0),
        ("shin_R", (0.034, -0.098, 0.26), (0.206, 0.114, 0.295), 1),
    ],
}


def material(name, rgb, metal, rough):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Metallic"].default_value = metal
    p.inputs["Roughness"].default_value = rough
    return m


def build(rig, piece, mat):
    name = f"armor_{mat}_{piece}"
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    groups = {g.name: g.index for g in ob.vertex_groups}

    parts = []
    for bone, lo, hi, slot in SHAPES[piece]:
        parts.append((bone, lo, hi, slot))
        if bone.endswith("_R"):
            parts.append((bone[:-2] + "_L", (-hi[0], lo[1], lo[2]), (-lo[0], hi[1], hi[2]), slot))

    bm = bmesh.new()
    dl = bm.verts.layers.deform.verify()
    for bone, lo, hi, slot in parts:
        ret = bmesh.ops.create_cube(bm, size=1.0)
        c = [(lo[i] + hi[i]) / 2 for i in range(3)]
        s = [hi[i] - lo[i] for i in range(3)]
        for v in ret["verts"]:
            v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
            v[dl][groups[bone]] = 1.0
        for f in {f for v in ret["verts"] for f in v.link_faces}:
            f.material_index = slot
    bm.to_mesh(me)
    bm.free()

    me.materials.append(bpy.data.materials[f"Armor_{mat}"])
    me.materials.append(bpy.data.materials[f"Armor_{mat}_trim"])
    bev = ob.modifiers.new("bevel", "BEVEL")
    bev.width, bev.segments, bev.limit_method = 0.007, 2, "NONE"
    # Shapes are in world space; the rig is scaled (1.8 tall), so undo it.
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob


def main():
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, "mods", "robot", "robot.gltf"))
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    rig.data.pose_position = "REST"
    for k, (a, b, met, ro) in MATS.items():
        material(f"Armor_{k}", a, met, ro)
        material(f"Armor_{k}_trim", b, met, min(1.0, ro + 0.15))

    for mat in MATS:
        for piece in SHAPES:
            ob = build(rig, piece, mat)
            for x in bpy.data.objects:
                x.select_set(False)
            ob.select_set(True)
            rig.select_set(True)
            bpy.context.view_layer.objects.active = ob
            bpy.ops.export_scene.gltf(
                filepath=os.path.join(OUT, f"{mat}_{piece}.gltf"), export_format="GLTF_SEPARATE",
                use_selection=True, export_animations=False, export_skins=True,
                export_apply=True, export_yup=True)

    bpy.ops.wm.save_as_mainfile(filepath=os.path.expanduser("~/kart_assets/armor.blend"))


main()
