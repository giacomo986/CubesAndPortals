# Builds the mine cart on the robot's own rig and exports it (mods/carts).
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_cart_model.py"
#
# Worn on the feet (core.player_wear), so every part is bound whole to "root":
# the feet, which a body in a cart keeps on the rail's top. Front is +Y.
# Shapes are in the rig's world space, metres:
#
#   wheels   four, flanged, on two axles, their treads on the rails 0.33 either
#            side of the middle (mods/building's rails: 0.17 and 0.83 of a cell)
#   chassis  two beams along under the bucket, and the axle boxes
#   bucket   wider at the top than the floor, of iron plate; two bands round
#            it, a strap down each corner, a rolled rim; its floor 0.19 over
#            the rail, its rim 0.68
#   bench    a plank across the back, 0.42 high: what the rider sits on
#            (tools/make_cart_clips.py)
#   couplers a hook front and back
import os

import bmesh
import bpy
from mathutils import Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "carts")

FLOOR, RIM = 0.16, 0.68                 # the bucket's bottom and top
BOTTOM, TOP = (0.31, 0.44), (0.42, 0.55)  # half sizes x, y at the bottom and the top
WHEEL_R, GAUGE = 0.085, 0.33


def material(name, rgb, metal, rough):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Metallic"].default_value = metal
    p.inputs["Roughness"].default_value = rough
    return m


MATS = ["Cart_plate", "Cart_band", "Cart_wood", "Cart_wheel"]


class Parts:
    def __init__(self):
        self.bm = bmesh.new()

    def _tag(self, faces, slot):
        for f in faces:
            f.material_index = slot

    def box(self, lo, hi, slot):
        ret = bmesh.ops.create_cube(self.bm, size=1.0)
        c = [(lo[i] + hi[i]) / 2 for i in range(3)]
        s = [hi[i] - lo[i] for i in range(3)]
        for v in ret["verts"]:
            v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
        self._tag({f for v in ret["verts"] for f in v.link_faces}, slot)

    def wheel(self, x, y, r, width, slot, segments=20):
        # A disc along X: its axis across the track.
        ret = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=segments,
                                    radius1=r, radius2=r, depth=width)
        for v in ret["verts"]:
            v.co = Vector((x + v.co.z, y + v.co.x, WHEEL_R + v.co.y))
        self._tag({f for v in ret["verts"] for f in v.link_faces}, slot)

    def shell(self, z0, z1, b0, b1, t, slot):
        # Four walls of a box wider at z1 than at z0, `t` thick, no floor or lid.
        def ring(z, b, k):
            return [Vector((sx * (b[0] - k), sy * (b[1] - k), z)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        outer = [ring(z0, b0, 0), ring(z1, b1, 0)]
        inner = [ring(z0, b0, t), ring(z1, b1, t)]
        vs = {}
        for name, rings in (("o", outer), ("i", inner)):
            vs[name] = [[self.bm.verts.new(p) for p in r] for r in rings]
        faces = []
        for i in range(4):
            j = (i + 1) % 4
            faces.append(self.bm.faces.new((vs["o"][0][i], vs["o"][0][j], vs["o"][1][j], vs["o"][1][i])))
            faces.append(self.bm.faces.new((vs["i"][0][j], vs["i"][0][i], vs["i"][1][i], vs["i"][1][j])))
            faces.append(self.bm.faces.new((vs["o"][1][i], vs["o"][1][j], vs["i"][1][j], vs["i"][1][i])))
            faces.append(self.bm.faces.new((vs["o"][0][j], vs["o"][0][i], vs["i"][0][i], vs["i"][0][j])))
        self._tag(faces, slot)


def half_at(z):
    k = (z - FLOOR) / (RIM - FLOOR)
    return (BOTTOM[0] + (TOP[0] - BOTTOM[0]) * k, BOTTOM[1] + (TOP[1] - BOTTOM[1]) * k)


def build(rig):
    p = Parts()
    # Wheels, flanges inside the rails, and the axles.
    for y in (-0.28, 0.28):
        for sx in (-1, 1):
            p.wheel(sx * GAUGE, y, WHEEL_R, 0.05, 3)
            p.wheel(sx * (GAUGE - 0.035), y, WHEEL_R + 0.018, 0.016, 3)
            p.wheel(sx * (GAUGE + 0.03), y, 0.03, 0.03, 1, 10)   # the hub
        p.box((-GAUGE, y - 0.018, WHEEL_R - 0.018), (GAUGE, y + 0.018, WHEEL_R + 0.018), 1)
        p.box((-0.3, y - 0.05, WHEEL_R - 0.02), (-0.2, y + 0.05, FLOOR), 1)   # axle boxes
        p.box((0.2, y - 0.05, WHEEL_R - 0.02), (0.3, y + 0.05, FLOOR), 1)
    # Chassis beams.
    for sx in (-1, 1):
        p.box((sx * 0.25 - 0.04, -0.5, FLOOR - 0.05), (sx * 0.25 + 0.04, 0.5, FLOOR), 2)
    # The bucket: its floor, its walls, two bands, corner straps, the rim.
    p.box((-BOTTOM[0], -BOTTOM[1], FLOOR), (BOTTOM[0], BOTTOM[1], FLOOR + 0.03), 0)
    p.shell(FLOOR, RIM, BOTTOM, TOP, 0.03, 0)
    for z in (0.30, 0.52):
        b = half_at(z)
        p.shell(z - 0.025, z + 0.025, (b[0] + 0.012, b[1] + 0.012), (b[0] + 0.015, b[1] + 0.015), 0.02, 1)
    for sx in (-1, 1):
        for sy in (-1, 1):
            lo, hi = half_at(FLOOR), half_at(RIM)
            for k in range(4):
                z0 = FLOOR + (RIM - FLOOR) * k / 4
                z1 = FLOOR + (RIM - FLOOR) * (k + 1) / 4
                a, c = half_at(z0), half_at(z1)
                x, y = sx * (a[0] + c[0]) / 2, sy * (a[1] + c[1]) / 2
                p.box((x - 0.03, y - 0.03, z0), (x + 0.03, y + 0.03, z1), 1)
    p.shell(RIM - 0.02, RIM + 0.03, (TOP[0] + 0.02, TOP[1] + 0.02), (TOP[0] + 0.025, TOP[1] + 0.025), 0.06, 1)
    # The bench across the back, on two blocks.
    p.box((-0.36, -0.46, 0.38), (0.36, -0.22, 0.42), 2)
    for sx in (-1, 1):
        p.box((sx * 0.3 - 0.03, -0.42, FLOOR + 0.03), (sx * 0.3 + 0.03, -0.26, 0.38), 2)
    # Couplers.
    for sy in (-1, 1):
        y0 = sy * (BOTTOM[1] + 0.02)
        p.box((-0.04, min(y0, y0 + sy * 0.14), 0.12), (0.04, max(y0, y0 + sy * 0.14), 0.17), 1)
        y1 = y0 + sy * 0.14
        p.box((-0.05, min(y1, y1 + sy * 0.04), 0.1), (0.05, max(y1, y1 + sy * 0.04), 0.2), 1)

    name = "cart"
    me = bpy.data.meshes.new(name)
    p.bm.normal_update()
    p.bm.to_mesh(me)
    p.bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for m in MATS:
        me.materials.append(bpy.data.materials[m])
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    ob.vertex_groups["root"].add(list(range(len(me.vertices))), 1.0, "REPLACE")
    bev = ob.modifiers.new("bevel", "BEVEL")
    bev.width, bev.segments, bev.limit_method = 0.006, 1, "ANGLE"
    for poly in me.polygons:
        poly.use_smooth = False
    # Shapes are in world space; the rig is scaled (1.8 tall), so undo it.
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob


def main(export=True):
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, "mods", "robot", "robot.gltf"))
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    for o in list(bpy.data.objects):
        if o.type == "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    rig.data.pose_position = "REST"
    material("Cart_plate", (0.30, 0.31, 0.33), 0.75, 0.55)
    material("Cart_band", (0.16, 0.15, 0.15), 0.85, 0.5)
    material("Cart_wood", (0.45, 0.30, 0.17), 0.0, 0.85)
    material("Cart_wheel", (0.12, 0.12, 0.13), 0.9, 0.45)
    ob = build(rig)
    if export:
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
    return ob


if __name__ == "__main__":
    main()
