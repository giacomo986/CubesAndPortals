# Builds the cart on the robot's own rig and exports it.
#
#   blender --background --python tools/make_cart_model.py
#
# As tools/make_boat_model.py: every box bound whole to "root" -- the feet,
# which a body in a cart keeps on the rail's top. Front is +Y.
# Shapes are in the rig's world space, metres.
import os

import bmesh
import bpy
from mathutils import Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "carts")

# (bone, low corner, high corner, 0 planks / 1 trim)
HULL = [
    ("root", (-0.42, -0.55, 0.06), (0.42, 0.55, 0.14), 0),     # floor
    ("root", (0.36, -0.55, 0.06), (0.44, 0.55, 0.62), 0),      # sides
    ("root", (-0.44, -0.55, 0.06), (-0.36, 0.55, 0.62), 0),
    ("root", (-0.36, 0.47, 0.06), (0.36, 0.55, 0.62), 0),      # front and back
    ("root", (-0.36, -0.55, 0.06), (0.36, -0.47, 0.62), 0),
    ("root", (-0.46, -0.57, 0.58), (0.46, 0.57, 0.64), 1),     # the rim
    ("root", (0.40, 0.22, -0.04), (0.48, 0.42, 0.16), 1),      # wheels
    ("root", (0.40, -0.42, -0.04), (0.48, -0.22, 0.16), 1),
    ("root", (-0.48, 0.22, -0.04), (-0.40, 0.42, 0.16), 1),
    ("root", (-0.48, -0.42, -0.04), (-0.40, -0.22, 0.16), 1),
]


def material(name, rgb, metal, rough):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Metallic"].default_value = metal
    p.inputs["Roughness"].default_value = rough
    return m


def build(rig):
    name = "cart"
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    groups = {g.name: g.index for g in ob.vertex_groups}

    parts = []
    for part in HULL:
        parts.append(part)

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

    me.materials.append(bpy.data.materials["Boat"])
    me.materials.append(bpy.data.materials["Boat_trim"])
    bev = ob.modifiers.new("bevel", "BEVEL")
    bev.width, bev.segments, bev.limit_method = 0.012, 2, "NONE"
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
    material("Boat", (0.55, 0.56, 0.60), 0.8, 0.45)
    material("Boat_trim", (0.22, 0.22, 0.25), 0.8, 0.6)
    ob = build(rig)
    for x in bpy.data.objects:
        x.select_set(False)
    ob.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = ob
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=os.path.join(OUT, "cart.gltf"), export_format="GLTF_SEPARATE",
        use_selection=True, export_animations=False, export_skins=True,
        export_apply=True, export_yup=True)


main()
