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
    # The emblem's own, whatever the player's colours (tools/dist/icon.png).
    "Emblem_Cube": ((0.80, 0.80, 0.78), 0.0, 0.5),
    "Emblem_Dark": ((0.08, 0.08, 0.12), 0.0, 0.4),
    "Emblem_Blue": ((0.24, 0.55, 1.00), 0.0, 0.4),
    "Emblem_Orange": ((1.00, 0.55, 0.12), 0.0, 0.4),
}

# A part: its shape (box, cyl, cone, dome, ball, torus), the bone it is
# bound to, its material, and where: c the centre, s a scale on the shape's own
# axes, r a turn (degrees, applied z first, then y, then x).
def P(k, bone, m, c, s=(1, 1, 1), r=(0, 0, 0), **kw):
    return dict(k=k, bone=bone, m=m, c=c, s=s, r=r, **kw)


# Where a body wears its extras, world at rest: what differs from one body to
# another (tools/make_species.py has one of these for each species).
#   cap     c the centre (x, y) and z the height of its rim, rx ry its half
#           widths there, h how high it domes; back: worn peak backwards
#   hat     the top hat: c, z its brim, r the crown's radius at the brim, h
#           the crown's height
#   ears    the headphones: x y z where a cup meets the head (the left one at
#           -x), top the band's highest point, cup its size against the
#           robot's, mic how far forward the microphone reaches
#   neck    the scarf's loop: y z its centre, rx ry its radii, tube its
#           thickness; lean the degrees its ends hang forward, over a belly
#   bow     the bow tie's knot: y z, and s its size against the robot's
#   medal   its pin: x y z, and lean as the scarf's
ROBOT = dict(
    cap=dict(c=(0.0, 0.005), z=1.598, rx=0.232, ry=0.178, h=0.13),
    hat=dict(c=(-0.02, 0.005), z=1.642, r=0.122, h=0.27),
    ears=dict(x=0.255, y=0.015, z=1.50, top=1.65, cup=1.0, mic=0.18),
    neck=dict(y=0.015, z=1.335, rx=0.125, ry=0.115, tube=0.045, lean=0),
    bow=dict(y=0.205, z=1.29, s=1.0),
    medal=dict(x=-0.12, y=0.192, z=1.24, lean=0),
)


def emblem(bone, c, size, tilt=0):
    """The game's emblem, a cube with a blue portal on one side and an orange
    one on the other (tools/dist/icon.png), an edge forward and half sunk in
    what it is on at `c`; tipped back `tilt` degrees, on a cap's dome."""
    back = Matrix.Rotation(math.radians(tilt), 3, "X")
    out = [P("box", bone, "Emblem_Cube", c, (size, size, size), (tilt, 0, 45))]
    for side, colour in ((1, "Emblem_Blue"), (-1, "Emblem_Orange")):   # blue on the left, seen from in front
        face = Vector(c) + back @ Vector((side * 0.3536 * size, 0.3536 * size, 0))   # the face's middle
        turn, oval = (tilt - 90, side * 45, 0), (0.25 * size, 0.36 * size)
        out += [P("cyl", bone, "Emblem_Dark", tuple(face), (*oval, 0.02 * size + 0.002), turn),
                P("torus", bone, colour, tuple(face), (*oval, oval[0]), turn, R=1.0, t=0.22)]
    return out


def leaned(parts, pivot, lean):
    """The parts hung `lean` degrees forward about the X axis through `pivot`."""
    turn = Matrix.Rotation(math.radians(lean), 3, "X")
    return [dict(p, c=tuple(Vector(pivot) + turn @ (Vector(p["c"]) - Vector(pivot))),
                 r=(p["r"][0] + lean, p["r"][1], p["r"][2])) for p in parts]


def extras(a):
    """The six extras on a body whose places are `a`, and the bones they add to
    the rig: name, parent, head, tail."""
    out = {}

    # A baseball cap: a dome, a band round its rim, a peak forward and a
    # little down, a button on top and the emblem on the front.
    x, y = a["cap"]["c"]
    z, rx, ry, h = (a["cap"][k] for k in ("z", "rx", "ry", "h"))
    cap = [
        P("dome", "head", "tint_1", (x, y, z), (rx, ry, h)),
        P("cyl", "head", "tint_3", (x, y, z + 0.008), (rx + 0.002, ry + 0.002, 0.018)),
        P("cyl", "head", "tint_3", (x, y + 0.93 * ry, z + 0.005), (0.75 * rx, 0.9 * ry, 0.012), (-7, 0, 0)),
        P("ball", "head", "tint_3", (x, y, z + 0.99 * h), (0.024, 0.024, 0.016)),
    ] + emblem("head", (x, y + 0.80 * ry, z + 0.62 * h), 0.3 * rx, 38)
    if a["cap"].get("back"):   # half a turn about the upright through its centre
        cap = [dict(p, c=(2 * x - p["c"][0], 2 * y - p["c"][1], p["c"][2]),
                    r=(-p["r"][0], -p["r"][1], p["r"][2] + 180)) for p in cap]
    out["cap"] = cap

    # A top hat: a brim with a curled edge, a crown flaring a little to the
    # top, a band and the emblem on it.
    x, y = a["hat"]["c"]
    z, r, h = (a["hat"][k] for k in ("z", "r", "h"))
    out["tophat"] = [
        P("cyl", "head", "tint_2", (x, y, z), (1.68 * r, 1.56 * r, 0.016)),
        P("torus", "head", "tint_2", (x, y, z + 0.006), (1.68 * r, 1.56 * r, 1.68 * r), R=1.0, t=0.06),
        P("cone", "head", "tint_2", (x, y, z + 0.008 + h / 2), (1, 1, 1), r1=r, r2=1.16 * r, d=h),
        P("cyl", "head", "tint_3", (x, y, z + 0.043), (1.05 * r, 1.05 * r, 0.05)),
    ] + emblem("head", (x, y + 1.05 * r, z + 0.043), 0.042)

    # Headphones: a band arched over the top, a padded cup on each side and a
    # microphone on a boom from the left one.
    x, y, z, top, k, mic = (a["ears"][k] for k in ("x", "y", "z", "top", "cup", "mic"))
    phones = [P("torus", "head", "tint_2", (0, y, z), (x + 0.03, top - z, 0.285), (90, 0, 0), R=1.0, t=0.07,
                arc=(0, 180))]
    for side in (1, -1):
        phones += [
            P("cyl", "head", "tint_3", (side * (x + 0.035 * k), y, z), (0.088 * k, 0.088 * k, 0.065 * k), (0, 90, 0)),
            P("torus", "head", "tint_2", (side * x, y, z), (0.07 * k, 0.07 * k, 0.07 * k), (0, 90, 0), R=1.0, t=0.3),
            P("dome", "head", "tint_1", (side * (x + 0.067 * k), y, z), (0.055 * k, 0.055 * k, 0.02 * k),
              (0, side * 90, 0)),
        ]
    out["headphones"] = phones + [
        P("cyl", "head", "tint_2", (-(x + 0.045 * k), y - 0.005 + mic / 2, z - 0.035 * k),
          (0.008, 0.008, mic), (-90, 0, 12)),
        P("ball", "head", "tint_3", (-(x + 0.045 * k) + 0.1 * mic, y + mic - 0.005, z - 0.035 * k),
          (0.022, 0.022, 0.022)),
    ]

    # A knitted scarf: wound twice round the neck, a knot in front and two
    # striped ends with a fringe, hanging free.
    y, z, rx, ry, tube, lean = (a["neck"][k] for k in ("y", "z", "rx", "ry", "tube", "lean"))
    kx, ey = 0.48 * rx, y + ry + tube + 0.022
    ends = [
        P("box", "sway_scarf_a", "tint_3", (kx - 0.005, ey, z - 0.16), (0.085, 0.026, 0.25)),
        P("box", "sway_scarf_a", "tint_2", (kx - 0.005, ey, z - 0.225), (0.087, 0.028, 0.018)),
        P("box", "sway_scarf_a", "tint_2", (kx - 0.005, ey, z - 0.19), (0.087, 0.028, 0.018)),
        P("box", "sway_scarf_b", "tint_3", (kx + 0.04, ey + 0.02, z - 0.14), (0.075, 0.022, 0.19), (0, -8, 0)),
        P("box", "sway_scarf_b", "tint_2", (kx + 0.047, ey + 0.02, z - 0.195), (0.077, 0.024, 0.018), (0, -8, 0)),
    ] + [P("box", "sway_scarf_a", "tint_2", (kx - 0.035 + 0.02 * i, ey, z - 0.30), (0.008, 0.012, 0.03))
         for i in range(4)]
    hang = (0, ey, z - 0.035)
    out["scarf"] = [
        P("torus", "chest", "tint_3", (0, y, z), (rx, ry, rx), R=1.0, t=tube / rx),
        P("torus", "chest", "tint_3", (0, y - 0.004, z + 0.75 * tube), (0.93 * rx, 0.93 * ry, 0.93 * rx), (7, 0, 20),
          R=1.0, t=0.8 * tube / rx),
        P("ball", "chest", "tint_3", (kx, y + ry + 0.78 * tube, z - 0.025), (0.05, 0.04, 0.045)),
    ] + leaned(ends, hang, lean)

    # A bow tie under the chin: two lobes and a knot.
    y, z, k = (a["bow"][k] for k in ("y", "z", "s"))
    out["bowtie"] = [
        P("cone", "chest", "tint_3", (0.062 * k, y, z), (1, 1, 1), (0, -90, 45), r1=0.06 * k, r2=0.016 * k,
          d=0.085 * k, seg=4),
        P("cone", "chest", "tint_3", (-0.062 * k, y, z), (1, 1, 1), (0, 90, 45), r1=0.06 * k, r2=0.016 * k,
          d=0.085 * k, seg=4),
        P("ball", "chest", "tint_2", (0, y + 0.007, z), (0.03 * k, 0.024 * k, 0.028 * k)),
    ]

    # A medal on a ribbon, on the left of the chest, swinging from its pin.
    x, y, z, mlean = (a["medal"][k] for k in ("x", "y", "z", "lean"))
    out["medal"] = [P("box", "chest", "Extra_Gold", (x, y, z), (0.07, 0.012, 0.016))] + leaned([
        P("box", "sway_medal", "tint_3", (x, y - 0.002, z - 0.065), (0.064, 0.012, 0.115)),
        P("box", "sway_medal", "tint_1", (x, y - 0.001, z - 0.065), (0.018, 0.013, 0.116)),
        P("cyl", "sway_medal", "Extra_Gold", (x, y + 0.006, z - 0.165), (0.05, 0.05, 0.014), (-90, 0, 0)),
        P("torus", "sway_medal", "Extra_Gold", (x, y + 0.006, z - 0.165), (0.05, 0.05, 0.05), (-90, 0, 0), R=1.0,
          t=0.16),
    ] + emblem("sway_medal", (x, y + 0.013, z - 0.165), 0.046), (x, y, z), mlean)

    def bone(name, head, tail, pivot, by):
        turn = Matrix.Rotation(math.radians(by), 3, "X")
        return (name, "chest", *(tuple(Vector(pivot) + turn @ (Vector(q) - Vector(pivot))) for q in (head, tail)))
    nz = a["neck"]["z"]
    bones = [
        bone("sway_scarf_a", (kx - 0.005, ey - 0.002, nz - 0.035), (kx - 0.005, ey - 0.002, nz - 0.285), hang, lean),
        bone("sway_scarf_b", (kx + 0.04, ey + 0.018, nz - 0.045), (kx + 0.04, ey + 0.018, nz - 0.235), hang, lean),
        bone("sway_medal", (x, y - 0.002, z - 0.005), (x, y - 0.002, z - 0.2), (x, y, z), mlean),
    ]
    return out, bones


EXTRAS, BONES = extras(ROBOT)


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
