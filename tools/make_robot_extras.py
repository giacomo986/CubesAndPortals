# Builds the robot's small extras -- three for the head, three for the neck --
# on the robot's own rig, and exports each one.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_robot_extras.py"
#
# The same way as tools/make_armor_models.py: imports mods/robot/robot.gltf so
# the bones are the ones the game poses, builds each extra around the robot at
# rest, each part bound whole to one bone, and exports
# mods/robot/robot_<extra>.gltf. The client draws the one a player picked as
# the model "<player model>_<extra>" on their skeleton, like a worn piece
# (draw_worn), in the player's colours: the materials are tint_1..3, the
# primary, secondary and accent the robot's own are.
#
# Bones the robot has not got are added here: one named sway_* hangs from its
# parent and the client swings it about where it is bound (the scarf's ends,
# the medal), so an extra moves a little of its own. Leaves the work in
# assets/blender/robot_extras.blend. Shapes are in world space at rest, metres,
# Z up and the robot facing +Y.
import math
import os

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "robot")

# The robot's own defaults, so an extra on a robot nobody dyed matches it.
MATS = {
    "tint_1": ((0.82, 0.84, 0.86), 0.0, 0.5),
    "tint_2": ((0.10, 0.12, 0.15), 0.0, 0.5),
    "tint_3": ((0.85, 0.18, 0.03), 0.0, 0.45),
    "Extra_Gold": ((1.00, 0.76, 0.20), 1.0, 0.3),
}

# The bones added to the rig: name, parent, head, tail (world, at rest).
BONES = [
    ("sway_scarf_a", "chest", (0.055, 0.195, 1.30), (0.055, 0.195, 1.05)),
    ("sway_scarf_b", "chest", (0.10, 0.215, 1.29), (0.10, 0.215, 1.10)),
    ("sway_medal", "chest", (-0.12, 0.19, 1.235), (-0.12, 0.19, 1.04)),
]

# A part: its shape (box, cyl, cone, dome, ball, torus, star), the bone it is
# bound to, its material, and where: c the centre, s a scale on the shape's own
# axes, r a turn (degrees, applied z first, then y, then x).
def P(k, bone, m, c, s=(1, 1, 1), r=(0, 0, 0), **kw):
    return dict(k=k, bone=bone, m=m, c=c, s=s, r=r, **kw)


EXTRAS = {
    # A baseball cap: a dome over the head, its peak forward and a little down.
    "cap": [
        P("dome", "head", "tint_1", (0, 0.005, 1.598), (0.232, 0.178, 0.13)),
        P("cyl", "head", "tint_3", (0, 0.005, 1.606), (0.234, 0.18, 0.018)),
        P("cyl", "head", "tint_3", (0, 0.17, 1.603), (0.175, 0.16, 0.012), (-7, 0, 0)),
        P("ball", "head", "tint_3", (0, 0.005, 1.727), (0.024, 0.024, 0.016)),
    ],
    # A top hat: a crown flaring a little to the top, a band and a curled brim.
    "tophat": [
        P("cyl", "head", "tint_2", (-0.02, 0.005, 1.642), (0.205, 0.19, 0.016)),
        P("torus", "head", "tint_2", (-0.02, 0.005, 1.648), (0.205, 0.19, 0.205), R=1.0, t=0.06),
        P("cone", "head", "tint_2", (-0.02, 0.005, 1.785), (1, 1, 1), r1=0.122, r2=0.142, d=0.27),
        P("cyl", "head", "tint_3", (-0.02, 0.005, 1.685), (0.128, 0.128, 0.05)),
        P("star", "head", "tint_1", (-0.02, 0.135, 1.685), (1, 1, 1), (-90, 0, 0), ro=0.022, ri=0.01, d=0.008),
    ],
    # Headphones: an arched band over the top, padded cups and a microphone.
    "headphones": [
        P("torus", "head", "tint_2", (0, 0.015, 1.50), (0.285, 0.15, 0.285), (90, 0, 0), R=1.0, t=0.07,
          arc=(0, 180)),
        P("cyl", "head", "tint_3", (0.29, 0.015, 1.50), (0.088, 0.088, 0.065), (0, 90, 0)),
        P("cyl", "head", "tint_3", (-0.29, 0.015, 1.50), (0.088, 0.088, 0.065), (0, 90, 0)),
        P("torus", "head", "tint_2", (0.255, 0.015, 1.50), (0.07, 0.07, 0.07), (0, 90, 0), R=1.0, t=0.3),
        P("torus", "head", "tint_2", (-0.255, 0.015, 1.50), (0.07, 0.07, 0.07), (0, 90, 0), R=1.0, t=0.3),
        P("dome", "head", "tint_1", (0.322, 0.015, 1.50), (0.055, 0.055, 0.02), (0, 90, 0)),
        P("dome", "head", "tint_1", (-0.322, 0.015, 1.50), (0.055, 0.055, 0.02), (0, -90, 0)),
        P("cyl", "head", "tint_2", (-0.30, 0.10, 1.465), (0.008, 0.008, 0.18), (-90, 0, 12)),
        P("ball", "head", "tint_3", (-0.282, 0.19, 1.465), (0.022, 0.022, 0.022)),
    ],
    # A knitted scarf: a thick loop round the neck, a knot in front and two
    # striped ends with a fringe, hanging free.
    "scarf": [
        P("torus", "chest", "tint_3", (0, 0.015, 1.335), (0.125, 0.115, 0.125), R=1.0, t=0.36),
        P("ball", "chest", "tint_3", (0.06, 0.15, 1.31), (0.05, 0.04, 0.045)),
        P("box", "sway_scarf_a", "tint_3", (0.055, 0.197, 1.175), (0.085, 0.026, 0.25)),
        P("box", "sway_scarf_a", "tint_2", (0.055, 0.197, 1.11), (0.087, 0.028, 0.018)),
        P("box", "sway_scarf_a", "tint_2", (0.055, 0.197, 1.145), (0.087, 0.028, 0.018)),
        P("box", "sway_scarf_b", "tint_3", (0.10, 0.217, 1.195), (0.075, 0.022, 0.19), (0, -8, 0)),
        P("box", "sway_scarf_b", "tint_2", (0.107, 0.217, 1.14), (0.077, 0.024, 0.018), (0, -8, 0)),
    ] + [P("box", "sway_scarf_a", "tint_2", (0.025 + 0.02 * i, 0.197, 1.035), (0.008, 0.012, 0.03))
         for i in range(4)],
    # A bow tie under the chin: two lobes and a knot.
    "bowtie": [
        P("cone", "chest", "tint_3", (0.062, 0.205, 1.29), (1, 1, 1), (0, -90, 45), r1=0.06, r2=0.016, d=0.085,
          seg=4),
        P("cone", "chest", "tint_3", (-0.062, 0.205, 1.29), (1, 1, 1), (0, 90, 45), r1=0.06, r2=0.016, d=0.085,
          seg=4),
        P("ball", "chest", "tint_2", (0, 0.212, 1.29), (0.03, 0.024, 0.028)),
    ],
    # A medal on a ribbon, on the left of the chest, swinging from its pin.
    "medal": [
        P("box", "chest", "Extra_Gold", (-0.12, 0.192, 1.24), (0.07, 0.012, 0.016)),
        P("box", "sway_medal", "tint_3", (-0.12, 0.19, 1.175), (0.064, 0.012, 0.115)),
        P("box", "sway_medal", "tint_1", (-0.12, 0.191, 1.175), (0.018, 0.013, 0.116)),
        P("cyl", "sway_medal", "Extra_Gold", (-0.12, 0.198, 1.075), (0.05, 0.05, 0.014), (-90, 0, 0)),
        P("torus", "sway_medal", "Extra_Gold", (-0.12, 0.198, 1.075), (0.05, 0.05, 0.05), (-90, 0, 0), R=1.0,
          t=0.16),
        P("star", "sway_medal", "tint_3", (-0.12, 0.206, 1.075), (1, 1, 1), (-90, 0, 0), ro=0.032, ri=0.014,
          d=0.008),
    ],
}


def material(name, rgb, metal, rough):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Metallic"].default_value = metal
    p.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1)   # what a solid viewport shows
    return m


def torus(bm, R, t, arc=(0, 360), nu=28, nv=10):
    """A ring of radius R about Z whose tube is t*R thick, over arc degrees."""
    whole = arc[1] - arc[0] >= 360
    us = nu if whole else nu + 1
    rows = []
    for i in range(us):
        a = math.radians(arc[0] + (arc[1] - arc[0]) * i / nu)
        row = []
        for j in range(nv):
            b = 2 * math.pi * j / nv
            rr = R + t * R * math.cos(b)
            row.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), t * R * math.sin(b))))
        rows.append(row)
    for i in range(nu):
        a, b = rows[i], rows[(i + 1) % us]
        for j in range(nv):
            bm.faces.new((a[j], b[j], b[(j + 1) % nv], a[(j + 1) % nv]))
    if not whole:   # the ends closed
        bm.faces.new(rows[0][::-1])
        bm.faces.new(rows[-1])
    return [v for row in rows for v in row]


def star(bm, ro, ri, d):
    """A five-pointed star in the XY plane, d thick along Z."""
    pts = []
    for i in range(10):
        a = math.pi / 2 + math.pi * i / 5
        r = ro if i % 2 == 0 else ri
        pts.append((r * math.cos(a), r * math.sin(a)))
    low = [bm.verts.new((x, y, -d / 2)) for x, y in pts]
    high = [bm.verts.new((x, y, d / 2)) for x, y in pts]
    bm.faces.new(low[::-1])
    bm.faces.new(high)
    for i in range(10):
        j = (i + 1) % 10
        bm.faces.new((low[i], low[j], high[j], high[i]))
    return low + high


def shape(bm, part):
    k = part["k"]
    if k == "box":
        verts = bmesh.ops.create_cube(bm, size=1.0)["verts"]
    elif k == "cyl":
        verts = bmesh.ops.create_cone(bm, cap_ends=True, segments=24, radius1=1, radius2=1, depth=1)["verts"]
    elif k == "cone":
        verts = bmesh.ops.create_cone(bm, cap_ends=True, segments=part.get("seg", 24), radius1=part["r1"],
                                      radius2=part["r2"], depth=part["d"])["verts"]
    elif k in ("dome", "ball"):
        verts = bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=1)["verts"]
        if k == "dome":   # the top half, its rim closed
            low = [v for v in verts if v.co.z < -1e-4]
            bmesh.ops.delete(bm, geom=low, context="VERTS")
            verts = [v for v in verts if v.is_valid]
            rim = [e for e in bm.edges if e.is_boundary and all(v in verts for v in e.verts)]
            bmesh.ops.contextual_create(bm, geom=rim)
    elif k == "torus":
        verts = torus(bm, part["R"], part["t"], part.get("arc", (0, 360)))
    elif k == "star":
        verts = star(bm, part["ro"], part["ri"], part["d"])
    turn = Euler([math.radians(a) for a in part["r"]], "ZYX").to_matrix().to_4x4()
    m = Matrix.Translation(Vector(part["c"])) @ turn @ Matrix.Diagonal((*part["s"], 1))
    for v in verts:
        v.co = m @ v.co
    return verts


def build(rig, extra):
    name = f"robot_{extra}"
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    groups = {g.name: g.index for g in ob.vertex_groups}

    used = []   # the materials this one uses, in slot order
    bm = bmesh.new()
    dl = bm.verts.layers.deform.verify()
    for part in EXTRAS[extra]:
        verts = shape(bm, part)
        if part["m"] not in used:
            used.append(part["m"])
        for v in verts:
            v[dl][groups[part["bone"]]] = 1.0
        for f in {f for v in verts for f in v.link_faces}:
            f.material_index = used.index(part["m"])
            f.smooth = part["k"] in ("dome", "ball", "torus", "cyl", "cone") and part.get("seg", 24) > 4
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()

    for m in used:
        me.materials.append(bpy.data.materials[m])
    bev = ob.modifiers.new("bevel", "BEVEL")
    bev.width, bev.segments, bev.limit_method = 0.004, 2, "ANGLE"
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob


def add_bones(rig):
    to_rig = rig.matrix_world.inverted()
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    for name, parent, head, tail in BONES:
        b = rig.data.edit_bones.new(name)
        b.head = to_rig @ Vector(head)
        b.tail = to_rig @ Vector(tail)
        b.parent = rig.data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")


def main():
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, "mods", "robot", "robot.gltf"))
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    rig.data.pose_position = "REST"
    add_bones(rig)
    for k, (rgb, metal, rough) in MATS.items():
        material(k, rgb, metal, rough)

    for extra in EXTRAS:
        ob = build(rig, extra)
        for x in bpy.data.objects:
            x.select_set(False)
        ob.select_set(True)
        rig.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.export_scene.gltf(
            filepath=os.path.join(OUT, f"robot_{extra}.gltf"), export_format="GLTF_SEPARATE",
            use_selection=True, export_animations=False, export_skins=True,
            export_apply=True, export_yup=True)

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "robot_extras.blend"))


if __name__ == "__main__":   # tools/make_species.py borrows the shapes
    main()
