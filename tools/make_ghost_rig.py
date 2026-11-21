# The cursed ghost's skeleton and clips (mods/ghost/ghost.gltf).
#
#   flatpak run org.blender.Blender --background --python tools/make_ghost_rig.py -- \
#       ~/.cache/cnp_look/ghost/out/ghost.gltf mods/ghost
#
# IN is the rigid model tools/meshy_prop.py made of the Meshy ghost (1.7 tall,
# its face to -Y in Blender, +Z in the game). A skeleton made for its shape:
#
#   root
#    `- hips -- spine -- chest -- head          the body, up the middle of the cloak
#        |                 |- upperarm_R -- forearm_R -- hand_R   reaching ahead at the chest
#        |                 `- upperarm_L -- forearm_L -- hand_L   the claws over the belly
#        `- tail1 -- tail2                      the tatters it trails behind and below
#
# Every vertex is weighed to its three nearest bones by distance along them
# (1 / (d + 3 cm)^4): the mesh is a decimated Meshy shell, not closed, which
# Blender's heat weights do not take. The clips, each made of sines so a loop
# meets its start, are rotations about the armature's own axes at each bone's
# head (+X leans a bone forward, a hanging one back):
#
#   idle   2.4 s  floats, sways, the claws flexing, the tatters waving
#   fly    1.2 s  leaning in, arms reaching, the tatters streaming behind
#   hold   1.0 s  clutching down at whoever it holds, its head bobbing a laugh
#   death  1.2 s  once: arms flung up, head back, rising -- under the mod's dissolve
#
# OUT_DIR/ghost.gltf + .bin + .png are written over (each under 1 MB), and
# ~/.cache/cnp_look/ghost_rig_sheet.png shows each clip at four moments.
import math
import os
import sys

import bpy
from mathutils import Euler, Matrix, Vector

FPS = 24
LOOK = os.path.expanduser("~/.cache/cnp_look")
MAX_MEDIA = 1 << 20

# name, parent, head, tail (Blender: X right of the ghost's own left, -Y its front, Z up)
BONES = [
    ("root", None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.3)),
    ("hips", "root", (0.08, 0.05, 0.55), (0.07, 0.05, 0.78)),
    ("spine", "hips", (0.07, 0.05, 0.78), (0.04, 0.08, 1.02)),
    ("chest", "spine", (0.04, 0.08, 1.02), (0.03, 0.08, 1.26)),
    ("head", "chest", (0.03, 0.08, 1.26), (0.06, 0.12, 1.70)),
    ("upperarm_R", "chest", (0.24, 0.06, 1.20), (0.44, -0.14, 1.15)),
    ("forearm_R", "upperarm_R", (0.44, -0.14, 1.15), (0.47, -0.42, 1.12)),
    ("hand_R", "forearm_R", (0.47, -0.42, 1.12), (0.49, -0.58, 1.08)),
    ("upperarm_L", "chest", (-0.20, 0.06, 1.18), (-0.44, 0.04, 0.96)),
    ("forearm_L", "upperarm_L", (-0.44, 0.04, 0.96), (-0.36, -0.26, 0.83)),
    ("hand_L", "forearm_L", (-0.36, -0.26, 0.83), (-0.30, -0.40, 0.72)),
    ("tail1", "hips", (0.08, 0.05, 0.55), (0.22, 0.22, 0.28)),
    ("tail2", "tail1", (0.22, 0.22, 0.28), (0.37, 0.39, 0.0)),
]
DEFORM = [b[0] for b in BONES if b[0] != "root"]


def args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(argv) != 2:
        sys.exit("usage: make_ghost_rig.py -- IN.gltf OUT_DIR")
    return os.path.abspath(os.path.expanduser(argv[0])), os.path.abspath(os.path.expanduser(argv[1]))


def build(src):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=src)
    mesh = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
    mesh.name = "ghost"

    data = bpy.data.armatures.new("ghost_rig")
    arm = bpy.data.objects.new("ghost_rig", data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, parent, head, tail in BONES:
        eb = data.edit_bones.new(name)
        eb.head, eb.tail = Vector(head), Vector(tail)
        if parent:
            eb.parent = data.edit_bones[parent]
        eb.use_deform = name != "root"
    bpy.ops.object.mode_set(mode="OBJECT")

    # Weights by distance to each bone's segment, the three nearest.
    segs = {name: (Vector(h), Vector(t)) for name, _, h, t in BONES if name != "root"}
    groups = {name: mesh.vertex_groups.new(name=name) for name in DEFORM}
    for v in mesh.data.vertices:
        p = mesh.matrix_world @ v.co
        near = []
        for name, (a, b) in segs.items():
            ab = b - a
            t = max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
            d = (a + ab * t - p).length
            near.append((d, name))
        near.sort()
        ws = [(1.0 / (d + 0.03) ** 4, name) for d, name in near[:3]]
        total = sum(w for w, _ in ws)
        for w, name in ws:
            groups[name].add([v.index], w / total, "REPLACE")
    mesh.parent = arm
    mod = mesh.modifiers.new("rig", "ARMATURE")
    mod.object = arm
    return arm, mesh


def clip(arm, name, seconds, pose, loop=True):
    """pose(t) -> {bone: (rx, ry, rz) degrees about the armature's axes, or
    (rx, ry, rz, (lx, ly, lz)) with a move too}, t 0..1 over the clip."""
    ad = arm.animation_data_create()
    act = bpy.data.actions.new(f"ghost_{name}")
    ad.action = act
    frames = max(1, round(seconds * FPS))
    steps = 12 if loop else 8
    for k in range(steps + 1):
        t = k / steps
        keys = pose(t)
        for pb in arm.pose.bones:
            pb.rotation_mode = "QUATERNION"
            # The bone's own axes in the armature's, without where it is: a
            # turn about its head, a move along the armature's axes.
            rest = pb.bone.matrix_local.to_3x3()
            r = keys.get(pb.name, (0, 0, 0))
            turn = Euler(tuple(math.radians(a) for a in r[:3]), "XYZ").to_matrix()
            basis = (rest.inverted() @ turn @ rest).to_4x4()
            if len(r) > 3:
                basis.translation = rest.inverted() @ Vector(r[3])
            pb.matrix_basis = basis
            pb.keyframe_insert("rotation_quaternion", frame=1 + t * frames)
            pb.keyframe_insert("location", frame=1 + t * frames)
    ad.action = None
    track = ad.nla_tracks.new()
    track.name = name
    track.strips.new(name, 1, act)
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)


def s(t, cycles=1.0, phase=0.0):
    return math.sin(2 * math.pi * cycles * t + phase)


def idle(t):
    return {
        "hips": (3 * s(t), 4 * s(t, 1, 1), 0),
        "spine": (3 * s(t, 1, 0.5), 0, 2 * s(t, 1, 2)),
        "chest": (0, 3 * s(t, 1, 1.5), 0),
        "head": (6 * s(t, 1, 1), 6 * s(t, 1, 2), 8 * s(t)),
        "upperarm_R": (-8 * s(t), 6 * s(t, 1, 1), 0),
        "forearm_R": (10 * s(t, 1, 1), 0, 0),
        "hand_R": (15 * s(t, 2, 2), 0, 0),
        "upperarm_L": (8 * s(t, 1, 2), 0, 6 * s(t, 1, 1)),
        "forearm_L": (-10 * s(t, 1, 3), 0, 0),
        "hand_L": (15 * s(t, 2, 1), 0, 0),
        "tail1": (10 * s(t, 1, math.pi / 2), 12 * s(t), 0),
        "tail2": (16 * s(t, 1, math.pi), 18 * s(t, 1, math.pi / 2), 0),
    }


def fly(t):
    return {
        "hips": (15 + 3 * s(t), 3 * s(t, 1, 1), 0),
        "spine": (6, 0, 3 * s(t, 1, 1)),
        "chest": (6 + 3 * s(t, 1, 0.5), 0, 0),
        "head": (-18 + 5 * s(t, 1, 1), 0, 6 * s(t)),
        "upperarm_R": (-18 + 8 * s(t), 0, 0),
        "forearm_R": (-6 + 8 * s(t, 1, 1), 0, 0),
        "hand_R": (20 * s(t, 2), 0, 0),
        "upperarm_L": (-40 + 8 * s(t, 1, 2), 0, -15),
        "forearm_L": (-20 + 8 * s(t, 1, 3), 0, 0),
        "hand_L": (20 * s(t, 2, 1), 0, 0),
        "tail1": (28 + 8 * s(t, 1, math.pi / 2), 10 * s(t), 0),
        "tail2": (20 + 18 * s(t, 1, math.pi), 18 * s(t, 1, math.pi / 2), 0),
    }


def hold(t):
    grab = 0.5 + 0.5 * s(t)
    return {
        "hips": (20, 0, 4 * s(t, 1, 1)),
        "spine": (10, 0, 0),
        "chest": (8 + 4 * s(t, 2), 0, 0),
        "head": (10 + 8 * s(t, 3), 0, 10 * s(t)),
        "upperarm_R": (20 + 20 * grab, 0, 0),
        "forearm_R": (20 + 25 * grab, 0, 0),
        "hand_R": (35 * grab, 0, 0),
        "upperarm_L": (-10 + 20 * grab, 0, 0),
        "forearm_L": (15 + 25 * grab, 0, 0),
        "hand_L": (35 * grab, 0, 0),
        "tail1": (20 + 12 * s(t, 1, math.pi / 2), 15 * s(t), 0),
        "tail2": (15 + 22 * s(t, 1, math.pi), 22 * s(t, 1, math.pi / 2), 0),
    }


def death(t):
    u = 1 - (1 - t) ** 2   # fast, then held
    return {
        "root": (0, 0, 0, (0, 0, 0.35 * u)),
        "hips": (-8 * u, 0, 0),
        "chest": (-15 * u, 0, 0),
        "head": (-35 * u, 0, 0),
        "upperarm_R": (-70 * u, 0, 0),
        "forearm_R": (-30 * u, 0, 0),
        "hand_R": (-30 * u, 0, 0),
        "upperarm_L": (-90 * u, 0, -20 * u),
        "forearm_L": (-30 * u, 0, 0),
        "hand_L": (-30 * u, 0, 0),
        "tail1": (40 * u, 0, 0),
        "tail2": (40 * u, 0, 0),
    }


CLIPS = [("idle", 2.4, idle, True), ("fly", 1.2, fly, True), ("hold", 1.0, hold, True),
         ("death", 1.2, death, False)]


def export(arm, mesh, out_dir):
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    mesh.select_set(True)
    path = os.path.join(out_dir, "ghost.gltf")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS",
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_yup=True, export_apply=False, export_materials="EXPORT",
                              export_image_format="AUTO", export_skins=True, export_def_bones=False)
    for ext in (".gltf", ".bin", ".png"):
        size = os.path.getsize(os.path.join(out_dir, "ghost" + ext))
        print(f"ghost{ext}: {size} bytes")
        assert size < MAX_MEDIA, f"ghost{ext} is over 1 MB"


def sheet(arm):
    """Each clip at four moments, from the front-left, in one picture."""
    scene = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = 256
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.45, 0.5, 0.55, 1)
    scene.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3
    sun.rotation_euler = (math.radians(50), 0, math.radians(-30))
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 2.4
    a = math.radians(-35)
    cam.location = (10 * math.sin(a), -10 * math.cos(a), 1.0)
    cam.rotation_euler = (math.radians(90), 0, a)
    scene.collection.objects.link(cam)
    scene.camera = cam
    import numpy as np
    rows = []
    ad = arm.animation_data
    for tr in ad.nla_tracks:
        tr.mute = True
    for name, seconds, _, _ in CLIPS:
        tr = ad.nla_tracks[name]
        tr.mute = False
        row = []
        for k in range(4):
            scene.frame_set(1 + round(seconds * FPS * k / 4 if name != "death" else seconds * FPS * k / 3))
            path = os.path.join(LOOK, f"ghost_rig_{name}{k}.png")
            scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            im = bpy.data.images.load(path)
            row.append(np.array(im.pixels[:]).reshape(256, 256, 4))
            os.remove(path)
        tr.mute = True
        rows.append(np.concatenate(row, axis=1))
    big = np.concatenate(rows[::-1], axis=0)
    out = bpy.data.images.new("ghost_rig_sheet", big.shape[1], big.shape[0])
    out.pixels = big.ravel().tolist()
    out.filepath_raw = os.path.join(LOOK, "ghost_rig_sheet.png")
    out.file_format = "PNG"
    out.save()
    print("sheet:", out.filepath_raw)


def main():
    src, out_dir = args()
    arm, mesh = build(src)
    for name, seconds, pose, loop in CLIPS:
        clip(arm, name, seconds, pose, loop)
    export(arm, mesh, out_dir)
    sheet(arm)


main()
