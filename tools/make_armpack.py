# Builds the arm pack -- a metal backpack with two jointed arms rising from
# its top, over the shoulders, each with a claw that holds one tool more -- on
# the robot's own rig, animates it, and exports it.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_armpack.py"
#
# The way tools/make_robot_extras.py makes the extras: imports
# mods/robot/robot.gltf so the bones are the ones the game poses, adds the
# pack's own (pack_*, the robot has none of them, so in the game they follow
# "chest"), builds each part bound whole to one bone, and exports
# mods/armpack/armpack.gltf with its clips:
#
#   armpack_idle      48 frames, a loop: the arms breathe, the claws flex
#   armpack_swing_L   14 frames: the left arm strikes forward with its tool
#   armpack_swing_R   14 frames: the right one
#   armpack_work      28 frames: both, in turn
#   armpack_unfold    18 frames: from folded flat on the pack to ready
#   armpack_fold      18 frames: back down
#
# And mods/armpack/armpack_arms.gltf, the arms as the wearer sees them: the
# same arms on robot_arms.gltf's rig (the eye at the origin, looking down +Y
# in Blender), reaching from behind the head to the top corners of the view,
# with the same clips but the fold. The client draws it with the first-person
# arms (draw_arms) and moves their tool_* parts into its claws.
#
# pack_tool_L / pack_tool_R are where each claw holds its tool: the tool is
# drawn there the way a held one is drawn at the hand, as the robot holds it
# at rest. Leaves the work in ~/kart_assets/armpack.blend and armpack_arms.blend, with a pick and a
# sword taken from the robot's own tools in the claws (not exported), and
# stills of each clip in /tmp/armpack_*.png. Shapes are in world space at
# rest, metres, Z up and the robot facing +Y.
import math
import os
import sys

import bmesh
import bpy
from mathutils import Euler, Matrix, Quaternion, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_robot_extras import material, shape   # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "armpack")

# Body, arms and lights are its three colours (tint_1..3), as the backpack's.
MATS = {
    "tint_1": ((0.55, 0.58, 0.62), 0.6, 0.35),
    "tint_2": ((0.12, 0.13, 0.16), 0.2, 0.5),
    "tint_3": ((1.00, 0.55, 0.05), 0.0, 0.4),
    "Pack_Joint": ((0.30, 0.31, 0.33), 1.0, 0.25),
}

# Which model is being made: "body", worn on the robot, or "eye", seen by
# the wearer (set by make()).
VIEW = "body"
ROOT_BONE = {"body": "chest", "eye": "fp_root"}


# Where each arm is at rest, side s = -1 (left, -X) or 1 (right).
def joints(s):
    if VIEW == "eye":   # from behind the head to the top corners of the view
        return dict(base=Vector((s * 0.14, -0.30, -0.26)), shoulder=Vector((s * 0.14, -0.30, -0.20)),
                    elbow=Vector((s * 0.32, -0.08, 0.14)), wrist=Vector((s * 0.35, 0.46, 0.11)),
                    palm=Vector((s * 0.35, 0.52, 0.11)), grip=Vector((s * 0.35, 0.60, 0.11)))
    return dict(base=Vector((s * 0.12, -0.25, 1.36)), shoulder=Vector((s * 0.12, -0.25, 1.42)),
                elbow=Vector((s * 0.26, -0.34, 1.66)), wrist=Vector((s * 0.38, -0.15, 1.71)),
                palm=Vector((s * 0.39, -0.09, 1.71)), grip=Vector((s * 0.39, -0.01, 1.71)))


def bones():
    out = []
    for side, s in (("L", -1), ("R", 1)):
        j = joints(s)
        up = Vector((0, 0, 0.05))
        out += [
            (f"pack_base_{side}", ROOT_BONE[VIEW], j["base"], j["shoulder"]),
            (f"pack_upper_{side}", f"pack_base_{side}", j["shoulder"], j["elbow"]),
            (f"pack_fore_{side}", f"pack_upper_{side}", j["elbow"], j["wrist"]),
            (f"pack_hand_{side}", f"pack_fore_{side}", j["wrist"], j["palm"]),
            (f"pack_finger_{side}_a", f"pack_hand_{side}", j["palm"] + Vector((0, 0, 0.036)),
             j["palm"] + Vector((0, 0.10, 0.03))),
            (f"pack_finger_{side}_b", f"pack_hand_{side}", j["palm"] - Vector((0, 0, 0.036)),
             j["palm"] + Vector((0, 0.10, -0.03))),
            (f"pack_tool_{side}", f"pack_hand_{side}", j["grip"], j["grip"] + up),
        ]
    return out


def P(k, bone, m, c, s=(1, 1, 1), r=(0, 0, 0), **kw):
    return dict(k=k, bone=bone, m=m, c=tuple(c), s=s, r=r, **kw)


def rod(bone, m, a, b, r):
    """A cylinder from a to b, r thick."""
    d = b - a
    q = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_euler("ZYX")
    return P("cyl", bone, m, (a + b) / 2, (r, r, d.length), [math.degrees(x) for x in q])


def parts():
    out = []
    if VIEW == "body":
        # The pack: a body, the plate on the back, a lid, three vents and a lamp.
        out += [
            P("box", "chest", "tint_1", (0, -0.235, 1.12), (0.34, 0.17, 0.40)),
            P("box", "chest", "tint_2", (0, -0.148, 1.12), (0.30, 0.02, 0.34)),
            P("box", "chest", "tint_2", (0, -0.235, 1.33), (0.32, 0.16, 0.03)),
            P("ball", "chest", "tint_3", (0, -0.322, 1.255), (0.022, 0.012, 0.022)),
        ] + [P("box", "chest", "tint_2", (0, -0.322, 0.98 + 0.045 * i), (0.20, 0.012, 0.018)) for i in range(3)]
    for s in (-1, 1):
        side = "L" if s < 0 else "R"
        j = joints(s)
        if VIEW == "body":   # a strap over the shoulder and down the front
            out += [P("box", "chest", "tint_2", (s * 0.13, -0.04, 1.255), (0.05, 0.22, 0.025)),
                    P("box", "chest", "tint_2", (s * 0.13, 0.152, 1.10), (0.05, 0.02, 0.29))]
        out += [
            # The turret the arm stands on, its joint, the upper arm and two pistons.
            P("cyl", f"pack_base_{side}", "tint_2", j["base"], (0.07, 0.07, 0.06)),
            P("ball", f"pack_upper_{side}", "Pack_Joint", j["shoulder"], (0.055, 0.055, 0.055)),
            rod(f"pack_upper_{side}", "tint_1", j["shoulder"], j["elbow"], 0.038),
            rod(f"pack_upper_{side}", "Pack_Joint", j["shoulder"] + Vector((s * 0.045, 0.01, 0.02)),
                j["elbow"] + Vector((s * 0.035, 0.03, -0.04)), 0.014),
            rod(f"pack_upper_{side}", "Pack_Joint", j["shoulder"] + Vector((-s * 0.04, 0.01, 0.02)),
                j["elbow"] + Vector((-s * 0.03, 0.03, -0.04)), 0.012),
            P("cyl", f"pack_upper_{side}", "tint_3", j["shoulder"].lerp(j["elbow"], 0.55),
              (0.044, 0.044, 0.04), None, along=(j["shoulder"], j["elbow"])),
            # The elbow, the forearm and the wrist.
            P("ball", f"pack_fore_{side}", "Pack_Joint", j["elbow"], (0.048, 0.048, 0.048)),
            rod(f"pack_fore_{side}", "tint_1", j["elbow"], j["wrist"], 0.032),
            P("cyl", f"pack_fore_{side}", "tint_3", j["elbow"].lerp(j["wrist"], 0.5),
              (0.04, 0.04, 0.04), None, along=(j["elbow"], j["wrist"])),
            P("ball", f"pack_hand_{side}", "Pack_Joint", j["wrist"], (0.038, 0.038, 0.038)),
            # The claw: a palm and two fingers, each a knuckle and a hooked tip.
            P("box", f"pack_hand_{side}", "tint_2", j["palm"] - Vector((0, 0.012, 0)), (0.075, 0.065, 0.10)),
        ]
        for f, z in (("a", 1), ("b", -1)):
            root = j["palm"] + Vector((0, 0.012, z * 0.036))
            out += [
                P("box", f"pack_finger_{side}_{f}", "tint_2", root + Vector((0, 0.045, 0)), (0.045, 0.085, 0.02),
                  (z * 12, 0, 0)),
                P("box", f"pack_finger_{side}_{f}", "tint_3", root + Vector((0, 0.092, -z * 0.016)),
                  (0.048, 0.028, 0.04), (z * 30, 0, 0)),
            ]
    return out


def placed_shape(bm, part):
    """shape(), with the two ways a rod says how it turns."""
    if part.get("along"):
        a, b = part["along"]
        q = Vector((0, 0, 1)).rotation_difference((b - a).normalized()).to_euler("ZYX")
        part = dict(part, r=[math.degrees(x) for x in q])
    return shape(bm, part)


def build(rig):
    me = bpy.data.meshes.new("Armpack_Mesh")
    ob = bpy.data.objects.new("Armpack_Mesh", me)
    bpy.context.scene.collection.objects.link(ob)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    groups = {g.name: g.index for g in ob.vertex_groups}
    used = []
    bm = bmesh.new()
    dl = bm.verts.layers.deform.verify()
    for part in parts():
        verts = placed_shape(bm, part)
        if part["m"] not in used:
            used.append(part["m"])
        for v in verts:
            v[dl][groups[part["bone"]]] = 1.0
        for f in {f for v in verts for f in v.link_faces}:
            f.material_index = used.index(part["m"])
            f.smooth = part["k"] in ("ball", "cyl")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    for m in used:
        me.materials.append(bpy.data.materials[m])
    bev = ob.modifiers.new("bevel", "BEVEL")
    bev.width, bev.segments, bev.limit_method = 0.006, 2, "ANGLE"
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob


def add_bones(rig):
    to_rig = rig.matrix_world.inverted()
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    for name, parent, head, tail in bones():
        b = rig.data.edit_bones.new(name)
        b.head = to_rig @ head
        b.tail = to_rig @ tail
        b.roll = 0
        b.parent = rig.data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")


def tools_in_claws(robot, rig):
    """The robot's own pick and sword, as the claws would hold them: their
    vertices moved from the hand to each grip, turned with nothing."""
    out = []
    for side, tool in (("R", "tool_pick"), ("L", "tool_sword")):
        g = robot.vertex_groups[tool].index
        keep = {v.index for v in robot.data.vertices if any(e.group == g and e.weight > 0.5 for e in v.groups)}
        bm = bmesh.new()
        bm.from_mesh(robot.data)
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.index not in keep], context="VERTS")
        # In the rig's own space, which is scaled: the mesh's vertices are.
        grip = rig.matrix_world.inverted() @ joints(-1 if side == "L" else 1)["grip"]
        shift = grip - rig.data.bones[tool].head_local
        for v in bm.verts:
            v.co += shift
        me = bpy.data.meshes.new(f"Preview_{tool}")
        bm.to_mesh(me)
        bm.free()
        for m in robot.data.materials:
            me.materials.append(m)
        ob = bpy.data.objects.new(f"Preview_{tool}", me)
        bpy.context.scene.collection.objects.link(ob)
        ob.parent = rig
        ob.parent_type = "BONE"
        ob.parent_bone = f"pack_tool_{side}"
        # A bone parent is at the bone's tail: back to where it was made.
        tail = rig.data.bones[f"pack_tool_{side}"].matrix_local.copy()
        tail.translation = rig.data.bones[f"pack_tool_{side}"].tail_local
        ob.matrix_parent_inverse = tail.inverted()
        out.append(ob)
    # The robot's hand empty, for the pictures: it has every tool at once.
    bm = bmesh.new()
    bm.from_mesh(robot.data)
    dl = bm.verts.layers.deform.verify()
    tools = {robot.vertex_groups[t].index for t in ("tool_pick", "tool_sword", "tool_axe", "tool_shovel")}
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if any(g in tools and w > 0.5 for g, w in v[dl].items())],
                     context="VERTS")
    bm.to_mesh(robot.data)
    bm.free()
    return out


# Each key: frame, then per bone a list of (world axis at rest, degrees) turns.
def turn(rig, bone, spins):
    rest = rig.data.bones[bone].matrix_local.to_quaternion()
    q = Quaternion()
    for axis, deg in spins:
        q = Quaternion(Vector(axis), math.radians(deg)) @ q
    return rest.inverted() @ q @ rest


def clip(rig, name, length, keys, loop=False):
    act = bpy.data.actions.new(name)
    rig.animation_data_create()
    rig.animation_data.action = act
    for pb in rig.pose.bones:
        pb.rotation_mode = "QUATERNION"
    for frame, pose in keys:
        for bone, spins in pose.items():
            pb = rig.pose.bones[bone]
            pb.rotation_quaternion = turn(rig, bone, spins)
            pb.keyframe_insert("rotation_quaternion", frame=frame)
    if loop:   # the last frame is the first again
        for bone, spins in keys[0][1].items():
            pb = rig.pose.bones[bone]
            pb.rotation_quaternion = turn(rig, bone, spins)
            pb.keyframe_insert("rotation_quaternion", frame=length)
    act.use_fake_user = True
    act.frame_range = (keys[0][0], length)
    track = rig.animation_data.nla_tracks.new()
    track.name = name
    track.strips.new(name, int(keys[0][0]), act)
    track.mute = True
    rig.animation_data.action = None
    return act


X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)


def arm(side, upper=0.0, out=0.0, fore=0.0, hand=0.0, grip=0.0):
    """A pose of one arm: upper and fore forward (+) or back, the upper out
    from the head (+), the hand pitched, the claw closed (+) or open."""
    s = -1 if side == "L" else 1
    return {
        f"pack_upper_{side}": [(X, upper), (Y, s * out)],
        f"pack_fore_{side}": [(X, fore)],
        f"pack_hand_{side}": [(X, hand)],
        f"pack_finger_{side}_a": [(X, -grip)],
        f"pack_finger_{side}_b": [(X, grip)],
    }


def both(**kw):
    return {**arm("L", **kw), **arm("R", **kw)}


STRIKES = {
    "body": (dict(upper=25, out=8, fore=45, hand=20, grip=14), dict(upper=-35, out=-4, fore=-50, hand=-25, grip=14)),
    "eye": (dict(upper=14, out=6, fore=25, hand=15, grip=14), dict(upper=-12, out=-4, fore=-38, hand=-30, grip=14)),
}

# Down the back of the pack, the claws by the vents and the tools along the legs.
FOLDED = dict(upper=-175, out=8, fore=-10, hand=85, grip=20)


def animate(rig):
    acts = []
    # Idle: the arms breathe out of step, the claws flex.
    keys = []
    for i, f in enumerate((1, 13, 25, 37)):
        a = math.sin(i * math.pi / 2)
        b = math.cos(i * math.pi / 2)
        keys.append((f, {**arm("L", upper=3 * a, out=3 * b, fore=-5 * a, hand=4 * b, grip=6 + 6 * a),
                         **arm("R", upper=-3 * a, out=-3 * b, fore=5 * a, hand=-4 * b, grip=6 - 6 * a)}))
    acts.append(clip(rig, "armpack_idle", 49, keys, loop=True))
    # A strike: raise and cock, then down and forward hard, then back. +X
    # turns a bone's tip from +Y towards +Z: on the body the upper arm points
    # up and back, so forward is negative; in the eye's model it points
    # forward, and down is negative there too.
    cock, hit = STRIKES[VIEW]

    def strike(side, at):
        return [(at, arm(side, grip=6)), (at + 4, arm(side, **cock)), (at + 7, arm(side, **hit)),
                (at + 13, arm(side, grip=6))]

    for side in ("L", "R"):
        acts.append(clip(rig, f"armpack_swing_{side}", 14, strike(side, 1)))
    acts.append(clip(rig, "armpack_work", 28, strike("L", 1) + strike("R", 15)))
    if VIEW == "eye":
        return acts
    acts.append(clip(rig, "armpack_unfold", 18, [
        (1, both(**FOLDED)),
        (8, both(upper=-60, out=15, fore=-80, hand=10, grip=-15)),
        (14, both(upper=8, out=-3, fore=10, hand=-6, grip=-20)),
        (18, both(grip=6)),
    ]))
    acts.append(clip(rig, "armpack_fold", 18, [
        (1, both(grip=6)),
        (6, both(upper=-40, out=12, fore=-60, hand=10, grip=-10)),
        (18, both(**FOLDED)),
    ]))
    return acts


def stills(rig, acts):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.light = "STUDIO"
    scene.render.resolution_x, scene.render.resolution_y = 640, 640
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    target = Vector((0, -0.1, 1.25))
    views = (("back", (1.3, -2.0, 1.9)), ("side", (2.6, 0.1, 1.5)))
    if VIEW == "eye":   # the wearer's own view, 16:9 and 70 degrees high
        scene.render.resolution_x, scene.render.resolution_y = 960, 540
        cam.data.sensor_fit = "VERTICAL"
        cam.data.angle = math.radians(70)
        cam.data.clip_start = 0.01
        views = (("eye", None),)
    for view, eye in views:
        if eye is None:
            cam.location = (0, 0, 0)
            cam.rotation_euler = (math.radians(90), 0, 0)
        else:
            cam.location = eye
            cam.rotation_euler = (target - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
        for act in acts:
            rig.animation_data.action = act
            start, end = (int(x) for x in act.frame_range)
            for k, f in enumerate((start, (start + end) // 2, end)):
                scene.frame_set(f)
                scene.render.filepath = f"/tmp/armpack_{view}_{act.name}_{k}.png"
                bpy.ops.render.render(write_still=True)
    rig.animation_data.action = None
    scene.frame_set(1)


def make(view):
    global VIEW
    VIEW = view
    src, mesh, out = {"body": ("robot.gltf", "Robot_Mesh", "armpack"),
                      "eye": ("robot_arms.gltf", "FP_Mesh", "armpack_arms")}[view]
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, "mods", "robot", src))
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    robot = bpy.data.objects[mesh]
    for a in list(bpy.data.actions):   # the robot's clips are the robot's
        bpy.data.actions.remove(a)
    if rig.animation_data:
        for t in list(rig.animation_data.nla_tracks):
            rig.animation_data.nla_tracks.remove(t)
    add_bones(rig)
    for k, (rgb, metal, rough) in MATS.items():
        material(k, rgb, metal, rough)
    pack = build(rig)
    preview = tools_in_claws(robot, rig)
    acts = animate(rig)

    for x in bpy.data.objects:
        x.select_set(False)
    pack.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = pack
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=os.path.join(OUT, out + ".gltf"), export_format="GLTF_SEPARATE",
        use_selection=True, export_animations=True, export_animation_mode="ACTIONS",
        export_skins=True, export_apply=True, export_yup=True)

    if "--no-stills" not in sys.argv:
        stills(rig, acts)
    for ob in preview:
        ob.hide_render = False
    bpy.ops.wm.save_as_mainfile(filepath=os.path.expanduser(f"~/kart_assets/{out}.blend"))


make("eye")
make("body")   # last, so the file Blender is left with is the worn one
