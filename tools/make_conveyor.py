# Builds the conveyor belt's block models (mods/conveyor) and leaves the work in
# assets/blender/conveyor.blend.
#
#   flatpak run org.blender.Blender --background --python tools/make_conveyor.py [-- PREVIEW_DIR]
#
# Eight models, one per shape a cell of belt takes from its neighbours (the mod
# swaps the block): a straight run with a drum at each open end -- none, the
# out end, the in end, both -- a quarter turn fed from the left or the right,
# and a ramp a block up or down. A drum is where the belt ends; two cells that
# meet have none there, so a line of them reads as one belt.
#
# The belt moves: its ribs and arrows are bones of a clip called "loop", which
# the client plays on the clock, so every cell of a line is in step and a rib
# that leaves one cell is the rib coming into the next. Half a second a loop,
# half a cell of travel: 1 m/s, the mod's SPEED. A rib is hidden (scaled to
# nothing) while its middle is off the flat of the belt -- over a drum, or in
# the next cell, which draws it itself.
#
# Authored in the unit cube, feet at 0, in 16ths: x across, y up, z the depth of
# the cell. The belt runs towards z = 0 (glTF -Z): the front of an upright
# block (+Z) faces whoever put it down, and the belt carries away from them.
# A curve is fed through its side (x = 0 on the left one) and leaves through
# z = 0 like the rest; a ramp comes in at the belt's height and leaves a block
# higher or lower, its top standing out of its cell.
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "conveyor")
BLEND = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "conveyor.blend")

BELT_TOP = 5.0   # the stacks ride here; core.node_box and core.node_conveyor in the mod say the same
LOOP = 0.5       # seconds a loop
TRAVEL = 8.0     # 16ths a loop: 1 m/s
FPS = 100
EPS = 1.0 / FPS  # a rib appears or goes within one sample


def material(name, rgb, metal=0.0, rough=0.8):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1.0)
    return m


# ---------------------------------------------------------------- parts, in 16ths

def prism(outline, y0, y1):
    """A convex outline in xz (in order round it), stood from y0 to y1."""
    bm = bmesh.new()
    lo = [bm.verts.new((x, y0, z)) for x, z in outline]
    hi = [bm.verts.new((x, y1, z)) for x, z in outline]
    n = len(outline)
    bm.faces.new(lo)
    bm.faces.new(hi)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    return bm


def box(lo, hi):
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    return prism([(x0, z0), (x1, z0), (x1, z1), (x0, z1)], y0, y1)


def strip(p0, p1, w, y0, y1):
    """A bar w wide from p0 to p1 in xz."""
    dx, dz = p1[0] - p0[0], p1[1] - p0[1]
    ln = math.hypot(dx, dz)
    nx, nz = -dz / ln * w / 2, dx / ln * w / 2
    return prism([(p0[0] + nx, p0[1] + nz), (p1[0] + nx, p1[1] + nz),
                  (p1[0] - nx, p1[1] - nz), (p0[0] - nx, p0[1] - nz)], y0, y1)


def sector(r0, r1, y0, y1, segs=12):
    """A quarter ring about the corner x = z = 0, from +x round to +z."""
    bm = bmesh.new()
    ring = []
    for i in range(segs + 1):
        a = math.pi / 2 * i / segs
        ring.append([bm.verts.new((r * math.cos(a), y, r * math.sin(a)))
                     for r, y in ((r0, y0), (r1, y0), (r1, y1), (r0, y1))])
    bm.faces.new(ring[0][::-1])
    bm.faces.new(ring[-1])
    for i in range(segs):
        a, b = ring[i], ring[i + 1]
        for k in range(4):
            bm.faces.new((a[k], a[(k + 1) % 4], b[(k + 1) % 4], b[k]))
    return bm


def cyl_x(r, y, z, x0, x1, segs=12):
    """A roller across the belt."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r, radius2=r, depth=x1 - x0)
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(math.pi / 2, 3, "Y"))
    bmesh.ops.translate(bm, verts=bm.verts, vec=((x0 + x1) / 2, y, z))
    return bm


def chevron(cx, cz, vx, vz, size=1.0):
    """An arrow painted on the belt, its point the way the belt runs: one bmesh."""
    px, pz = -vz, vx
    tip = (cx + vx * 1.5 * size, cz + vz * 1.5 * size)
    bm = bmesh.new()
    for s in (1, -1):
        back = (tip[0] - (vx * 3 - px * 3 * s) * size, tip[1] - (vz * 3 - pz * 3 * s) * size)
        arm = strip(tip, back, 1.0 * size, BELT_TOP, BELT_TOP + 0.12)
        me = bpy.data.meshes.new("tmp")
        arm.to_mesh(me)
        arm.free()
        bm.from_mesh(me)
        bpy.data.meshes.remove(me)
    return bm


def rib(z):
    return box((1.5, BELT_TOP, z - 0.4), (14.5, BELT_TOP + 0.25, z + 0.4))


def sheared(bm, lift):
    """Everything above the floor lifted by lift(z): a ramp from a flat part."""
    for v in bm.verts:
        if v.co.y > 0.5:
            v.co.y += lift(v.co.z)
    return bm


# ---------------------------------------------------------------- the shapes
#
# A shape is its still parts, (bmesh, material), and its moving ones: (bmesh,
# material, pivot, move, shown) -- move(t) the cell-space matrix at t seconds,
# shown(point) whether a rib whose middle is there is drawn.

def straight(drum_out, drum_in):
    z0 = 2.5 if drum_out else 0.0
    z1 = 13.5 if drum_in else 16.0
    still = [
        (box((0, 0, 0), (1.5, 6, 16)), "frame"),
        (box((14.5, 0, 0), (16, 6, 16)), "frame"),
        (box((1.5, 1, z0), (14.5, BELT_TOP, z1)), "belt"),
    ]
    for on, z in ((drum_out, 2.5), (drum_in, 13.5)):
        if on:
            still.append((cyl_x(2.5, 2.5, z, 1.5, 14.5), "belt"))
            still.append((cyl_x(0.9, 2.5, z, -0.3, 16.3, segs=8), "axle"))

    def move(t):
        return Matrix.Translation((0, 0, -TRAVEL * t / LOOP))

    def shown(p):
        return z0 <= p.z <= z1

    moving = []
    for z in range(2, 23, 4):
        if z - TRAVEL <= z1 and z >= z0:
            moving.append((rib(z), "rib", Vector((8, BELT_TOP, z)), move, shown))
    for z in (4, 12, 20):
        moving.append((chevron(8, z, 0, -1), "paint", Vector((8, BELT_TOP, z)), move, shown))
    return still, moving


def ramp(up):
    """In at the belt's height at z = 16, out a block higher (or lower) at z = 0."""
    def lift(z):
        return 16 - z if up else z

    still = [
        (sheared(box((0, 0, 0), (1.5, 6, 16)), lift), "frame"),
        (sheared(box((14.5, 0, 0), (16, 6, 16)), lift), "frame"),
        (sheared(box((1.5, 0, 0), (14.5, BELT_TOP, 16)), lift), "belt"),
    ]
    rise = TRAVEL if up else -TRAVEL

    def move(t):
        return Matrix.Translation((0, rise * t / LOOP, -TRAVEL * t / LOOP))

    def shown(p):
        return 0 <= p.z <= 16

    moving = []
    for z in range(2, 23, 4):
        moving.append((sheared(rib(z), lift), "rib", Vector((8, BELT_TOP + lift(z), z)), move, shown))
    for z in (4, 12, 20):
        moving.append((sheared(chevron(8, z, 0, -1), lift), "paint", Vector((8, BELT_TOP + lift(z), z)),
                       move, shown))
    return still, moving


def curve():
    """Fed from the left (x = 0), out through z = 0: a quarter about that corner."""
    still = [
        (sector(14.5, 16, 0, 6), "frame"),
        (sector(0.01, 1.5, 0, 6, segs=4), "frame"),
        (sector(1.5, 14.5, 1, BELT_TOP), "belt"),
    ]
    corner = Vector((0, BELT_TOP, 0))

    def move(t):   # round the corner, from +z towards +x
        return Matrix.Rotation(math.radians(45) * t / LOOP, 4, "Y")

    def shown(p):
        return 0 <= math.degrees(math.atan2(p.z, p.x)) <= 90

    moving = []
    for k in range(6):
        a = math.radians(11.25 + 22.5 * k)
        c, s = math.cos(a), math.sin(a)
        moving.append((strip((1.5 * c, 1.5 * s), (14.5 * c, 14.5 * s), 0.8, BELT_TOP, BELT_TOP + 0.25),
                       "rib", corner, move, shown))
    for deg in (22.5, 67.5, 112.5):
        a = math.radians(deg)
        moving.append((chevron(8 * math.cos(a), 8 * math.sin(a), math.sin(a), -math.cos(a), 0.8),
                       "paint", corner, move, shown))
    return still, moving


def disc(r, y0, y1, segs=16):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r, radius2=r, depth=y1 - y0)
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(math.pi / 2, 3, "X"))
    bmesh.ops.translate(bm, verts=bm.verts, vec=(8, (y0 + y1) / 2, 8))
    return bm


def sorter():
    """One way in (z = 16), three out: left (x = 0), ahead (z = 0), right. A
    turntable in the middle, turning a quarter a loop; an arrow painted to each
    way out, and the way in a dark mouth between two posts."""
    still = [
        (box((0, 0, 0), (16, BELT_TOP, 16)), "frame"),
        (box((1, BELT_TOP, 1), (15, BELT_TOP + 0.1, 15)), "belt"),
        (box((4, BELT_TOP + 0.1, 15), (12, BELT_TOP + 0.2, 16)), "paint"),   # the way in
    ]
    for x0, z0 in ((0, 0), (14, 0), (0, 14), (14, 14)):
        still.append((box((x0, BELT_TOP, z0), (x0 + 2, 8, z0 + 2)), "frame"))
    for (cx, cz), (vx, vz) in (((3.5, 8), (-1, 0)), ((8, 3.5), (0, -1)), ((12.5, 8), (1, 0))):
        still.append((chevron(cx, cz, vx, vz, 0.8), "paint"))
    top = BELT_TOP + 0.1
    table = disc(3.2, top, top + 0.4)
    for a in (0, 90):   # a cross on it, so a turn shows
        bar = strip((8 - 3 * math.cos(math.radians(a)), 8 - 3 * math.sin(math.radians(a))),
                    (8 + 3 * math.cos(math.radians(a)), 8 + 3 * math.sin(math.radians(a))), 0.7,
                    top + 0.4, top + 0.55)
        me = bpy.data.meshes.new("tmp")
        bar.to_mesh(me)
        bar.free()
        table.from_mesh(me)
        bpy.data.meshes.remove(me)
    center = Vector((8, top, 8))

    def turn(t):
        return (Matrix.Translation(center) @ Matrix.Rotation(math.radians(90) * t / LOOP, 4, "Y")
                @ Matrix.Translation(-center))

    return still, [(table, "axle", center, turn, lambda p: True)]


def extractor():
    """A belt's first cell under a hood: things come out of the hood's mouth at
    the middle and ride on out at z = 0. The back of the hood is against the
    chest it empties."""
    still, moving = straight(False, True)
    still += [
        (box((0, 0, 9), (16, 12, 16)), "hood"),
        (box((3, BELT_TOP, 8.9), (13, 10, 9)), "belt"),            # the mouth
        (box((0, 12, 9), (16, 12.6, 16)), "paint"),                # a band on top
        (box((2, 2, 15.95), (14, 10, 16.05)), "belt"),             # the grille at the back
    ]
    for x in (4, 7, 10, 13):
        still.append((box((x - 0.4, 2, 16), (x + 0.4, 10, 16.2)), "hood"))
    return still, moving


def middle(bm):
    return sum((v.co for v in bm.verts), Vector()) / len(bm.verts)


SHAPES = {
    "belt": (straight(True, True), False),
    "belt_in": (straight(True, False), False),    # fed from behind: a drum only where it ends
    "belt_out": (straight(False, True), False),   # feeding on: a drum only where it starts
    "belt_mid": (straight(False, False), False),
    "belt_left": (curve(), False),
    "belt_right": (curve(), True),                # a left one in a mirror
    "belt_up": (ramp(True), False),
    "belt_down": (ramp(False), False),
    "sorter": (sorter(), False),
    "extractor": (extractor(), False),
}


def build(name, still, moving, mats, mirror):
    """One mesh, skinned: the still parts on a bone that never moves, each
    moving part on its own, keyed round a loop."""
    # 16ths of the cell to metres, and x, up, depth to Blender's x, -y, z (the
    # export turns that back into glTF's x, y, z).
    sx = -1 if mirror else 1
    C = Matrix(((sx / 16, 0, 0, 1 if mirror else 0), (0, 0, -1 / 16, 0), (0, 1 / 16, 0, 0), (0, 0, 0, 1)))

    arm = bpy.data.objects.new(name + "_rig", bpy.data.armatures.new(name + "_rig"))
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    b = arm.data.edit_bones.new("base")
    b.head, b.tail = (0, 0, 0), (0, 0, 0.1)
    bones = []
    for i, (bm, _, pivot, _, _) in enumerate(moving):
        b = arm.data.edit_bones.new(f"part{i}")
        b.head = C @ pivot
        b.tail = b.head + Vector((0, 0, 0.05))
        bones.append(b.name)
    bpy.ops.object.mode_set(mode="OBJECT")

    whole = bmesh.new()
    order, groups = [], []
    parts = [(bm, m, "base") for bm, m in still] + [(p[0], p[1], bones[i]) for i, p in enumerate(moving)]
    centres = [None] * len(still) + [middle(p[0]) for p in moving]
    for bm, mname, group in parts:
        if mname not in order:
            order.append(mname)
        me = bpy.data.meshes.new("tmp")
        bm.to_mesh(me)
        bm.free()
        for p in me.polygons:
            p.material_index = order.index(mname)
        start = len(whole.verts)
        whole.from_mesh(me)
        bpy.data.meshes.remove(me)
        groups.append((group, start, len(whole.verts)))
    bmesh.ops.transform(whole, matrix=C, verts=whole.verts)
    bmesh.ops.recalc_face_normals(whole, faces=whole.faces)
    me = bpy.data.meshes.new(name)
    whole.to_mesh(me)
    whole.free()
    for mname in order:
        me.materials.append(mats[mname])
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    for group, a, z in groups:
        vg = obj.vertex_groups.get(group) or obj.vertex_groups.new(name=group)
        vg.add(list(range(a, z)), 1.0, "REPLACE")
    obj.parent = arm
    obj.modifiers.new("rig", "ARMATURE").object = arm

    # The loop: each part keyed at both ends of it, and either side of every
    # moment it comes on to the flat of the belt or leaves it.
    arm.animation_data_create()
    arm.animation_data.action = bpy.data.actions.new(name + "_loop")
    Ci = C.inverted()
    for (_, _, _, move, shown), bname, centre in zip(moving, bones, centres[len(still):]):
        pb = arm.pose.bones[bname]
        pb.rotation_mode = "QUATERNION"
        rest = arm.data.bones[bname].matrix_local
        samples = [i * LOOP / 200 for i in range(201)]
        seen = [shown(move(t) @ centre) for t in samples]
        keys = {i * LOOP / 4: seen[i * 50] for i in range(5)}
        for i in range(1, len(samples)):
            if seen[i] != seen[i - 1]:
                keys[samples[i] - EPS] = seen[i - 1]
                keys[samples[i]] = seen[i]
        last = None
        for t in sorted(keys):
            world = C @ move(t) @ Ci
            loc, rot, _ = (rest.inverted() @ world @ rest).decompose()
            if last is not None:
                rot.make_compatible(last)
            last = rot
            s = 1.0 if keys[t] else 0.001
            pb.location, pb.rotation_quaternion, pb.scale = loc, rot, (s, s, s)
            for path in ("location", "rotation_quaternion", "scale"):
                pb.keyframe_insert(path, frame=t * FPS)
    return arm, obj


def export(arm, obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=True, export_animation_mode="ACTIVE_ACTIONS",
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_yup=True, export_apply=False, export_materials="EXPORT",
                              export_image_format="NONE", export_skins=True)
    with open(path) as f:
        doc = json.load(f)
    doc["animations"][0]["name"] = "loop"
    with open(path, "w") as f:
        json.dump(doc, f, separators=(",", ":"))


def preview(out):
    """The shapes laid out, and a belt over a step and round a corner."""
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_cavity = True
    scene.render.resolution_x, scene.render.resolution_y = 1400, 800
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 13.0
    cam.location = (5.5, -8.0, 9.0)
    cam.rotation_euler = (math.radians(45), 0, 0)
    scene.camera = cam
    scene.frame_set(int(LOOP * FPS * 0.3))
    scene.render.filepath = out
    bpy.ops.render.render(write_still=True)


bpy.ops.wm.read_homefile(use_empty=True)
bpy.context.scene.render.fps = FPS
# Linear between keys: a rib goes at the belt's speed, not easing in and out.
bpy.context.preferences.edit.keyframe_new_interpolation_type = "LINEAR"
M = {
    "frame": material("Frame", (0.32, 0.33, 0.36), 0.7, 0.45),
    "belt": material("Belt", (0.09, 0.09, 0.10), 0.0, 0.9),
    "rib": material("Rib", (0.20, 0.20, 0.22), 0.0, 0.9),
    "axle": material("Axle", (0.62, 0.63, 0.66), 1.0, 0.35),
    "paint": material("Paint", (0.95, 0.72, 0.12), 0.0, 0.6),
    "hood": material("Hood", (0.42, 0.44, 0.48), 0.7, 0.4),
}
os.makedirs(OUT, exist_ok=True)
rigs = {}
for name, ((still, moving), mirror) in SHAPES.items():
    arm, obj = build(name, still, moving, M, mirror)
    export(arm, obj, os.path.join(OUT, f"{name}.gltf"))
    rigs[name] = arm
# Laid out in the .blend: each shape alone along the bottom, and above them a
# belt that goes up a step, along, down again and round a corner.
for i, name in enumerate(SHAPES):
    rigs[name].location = (i * 1.3, -1.5, 0)
line = [("extractor", 0, 3, 0, -90), ("sorter", 7, 3, 0, -90), ("belt_out", 1, 3, 0, -90), ("belt_up", 2, 3, 0, -90), ("belt_mid", 3, 3, 1, -90),
        ("belt_down", 4, 3, 0, -90), ("belt_mid", 5, 3, 0, -90), ("belt_mid", 6, 3, 0, -90)]
for name, x, y, z, turn in line:
    arm = rigs[name].copy()
    arm.animation_data.action = rigs[name].animation_data.action
    bpy.context.scene.collection.objects.link(arm)
    body = next(o for o in bpy.data.objects if o.parent is rigs[name]).copy()
    bpy.context.scene.collection.objects.link(body)
    body.parent = arm
    body.modifiers["rig"].object = arm
    arm.location = (x, y, z)
    arm.rotation_euler = (0, 0, math.radians(turn))
args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if args:
    preview(os.path.join(args[0], "conveyor.png"))
bpy.ops.wm.save_as_mainfile(filepath=BLEND)
print(f"conveyor: {len(SHAPES)} models in {OUT}")
