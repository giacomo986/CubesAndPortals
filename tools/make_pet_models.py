# The pets: a dog and a cat, built out of boxes in Blender, rigged and animated,
# and exported for mods/pets.
#
#   blender --background --python tools/make_pet_models.py
#   (or run inside Blender, the Text editor or the MCP bridge)
#
# Starts from an empty file, saves it as ~/kart_assets/pets.blend and writes
# dog.gltf, dog_tame.gltf, cat.gltf and cat_tame.gltf: the tame ones are the
# same body with a red collar round the neck, which is how a player tells
# their own pet from a wild one. And bobber.gltf and bobber_bite.gltf, the
# float of the rod that catches the cat's fish, afloat and pulled under.
#
# Front +Y, feet on z = 0, like the creatures of mods/mobs. Every bone points
# up (+Z) with no roll, so a bone's own X is the world's X -- a leg swings and
# a body pitches about X -- and its own Y is the world's Z, which is what a
# tail wags about. The clips are NLA tracks named as the engine asks for them:
# idle, walk, run and sit (played while the pet sits, as Working).
import math
import os

import bpy
from mathutils import Matrix

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "pets")
BLEND = os.path.expanduser("~/kart_assets/pets.blend")
FPS = 24


def material(name, rgb):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.9
    m.diffuse_color = (*rgb, 1.0)
    return m


def boxes_mesh(name, boxes, mats):
    """boxes: [(bone, material name, (x0, y0, z0), (x1, y1, z1))]. One mesh,
    each box its own eight corners, weighted whole to its bone."""
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
    """bones: [(name, parent, (x, y, z))], each pointing up 0.1."""
    arm = bpy.data.armatures.new(name)
    obj = bpy.data.objects.new(name, arm)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    for bname, parent, (x, y, z) in bones:
        eb = arm.edit_bones.new(bname)
        eb.head = (x, y, z)
        eb.tail = (x, y, z + 0.1)
        eb.roll = 0.0
        if parent:
            eb.parent = arm.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in obj.pose.bones:
        pb.rotation_mode = "XYZ"
    return obj


def skin(mesh, arm):
    mesh.parent = arm
    mod = mesh.modifiers.new("Armature", "ARMATURE")
    mod.object = arm


def clip(arm, prefix, name, seconds, keys, loop=True):
    """keys: {bone: [(t in 0..1, (rx, ry, rz), (lx, ly, lz) or None)]}, looped:
    t = 1 is set to what t = 0 is. Not looped (a death), the last key holds."""
    act = bpy.data.actions.new(f"{prefix}_{name}")
    arm.animation_data_create()
    arm.animation_data.action = act
    frames = max(1, round(seconds * FPS))
    for pb in arm.pose.bones:
        pb.rotation_euler = (0, 0, 0)
        pb.location = (0, 0, 0)
    for bone, ks in keys.items():
        pb = arm.pose.bones[bone]
        end = ks[0] if loop else ks[-1]
        for t, rot, loc in ks + [(1.0, end[1], end[2])]:
            pb.rotation_euler = rot
            pb.keyframe_insert("rotation_euler", frame=1 + t * frames)
            if loc is not None:
                pb.location = loc
                pb.keyframe_insert("location", frame=1 + t * frames)
    # Every bone keyed at the ends, so a clip says where each one is.
    for pb in arm.pose.bones:
        if pb.name not in keys:
            pb.rotation_euler = (0, 0, 0)
            for f in (1, 1 + frames):
                pb.keyframe_insert("rotation_euler", frame=f)
    arm.animation_data.action = None
    track = arm.animation_data.nla_tracks.new()
    track.name = name
    track.strips.new(name, 1, act)


def tip_over(arm, meshes, seconds=1.0, side=1.0, drop=0.0, name="death"):
    """The "death" clip every creature is expected to have (docs/MODDING.md,
    Creature): over onto its side, as a creature dies in the sandbox, lifted by
    the half of it that would otherwise go into the floor, and `drop` further
    down for one that floats. The client plays it once while the body fades
    out. Worked in the world's axes on the root bones -- front +Y, up +Z -- so
    it fits any rig, made here or not; replaces a "death" already there."""
    ad = arm.animation_data_create()
    for tr in list(ad.nla_tracks):
        if tr.name == name:
            ad.nla_tracks.remove(tr)
    mutes = [(tr, tr.mute) for tr in ad.nla_tracks]
    for tr, _ in mutes:
        tr.mute = True
    kept = ad.action
    ad.action = None
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()

    origin = arm.matrix_world.to_translation()
    lift = max(abs((m.matrix_world @ v.co - origin).x) for m in meshes for v in m.data.vertices)
    W = arm.matrix_world.to_3x3().to_4x4()   # the rig's turn and size, not where it stands
    roots = [pb for pb in arm.pose.bones if pb.parent is None]
    act = bpy.data.actions.new(f"{arm.name}_{name}")
    ad.action = act
    frames = max(1, round(seconds * FPS))
    for t, angle, u in ((0.0, 0.0, 0.0), (0.12, 0.3, 0.1), (0.35, math.pi / 2, 1.0), (1.0, math.pi / 2, 1.0)):
        world = Matrix.Translation((0, 0, (lift - drop) * u)) @ Matrix.Rotation(side * angle, 4, "Y")
        turn = W.inverted() @ world @ W
        for pb in roots:
            rest = pb.bone.matrix_local
            pb.matrix_basis = rest.inverted() @ turn @ rest
            rot = "rotation_quaternion" if pb.rotation_mode == "QUATERNION" else "rotation_euler"
            for path in ("location", rot):
                pb.keyframe_insert(path, frame=1 + t * frames)
    ad.action = kept
    for tr, m in mutes:
        tr.mute = m
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    track = ad.nla_tracks.new()
    track.name = name
    track.strips.new(name, 1, act)


def animal(prefix, s):
    """s: the sizes that make a dog a dog and a cat a cat."""
    hip = s["leg"]                      # legs meet the body here
    fy, by = s["front_y"], s["back_y"]  # where the front and back legs stand
    lx = s["leg_x"]
    bones = [
        ("root", None, (0, 0, 0)),
        ("body", "root", (0, by, hip)),               # pitches about its back hips
        ("head", "body", (0, s["neck_y"], s["neck_z"])),
        ("tail", "body", (0, s["tail_y"], s["tail_z"])),
        ("leg_FL", "body", (lx, fy, hip)),
        ("leg_FR", "body", (-lx, fy, hip)),
        ("leg_BL", "body", (lx, by, hip)),
        ("leg_BR", "body", (-lx, by, hip)),
    ]
    arm = rig(f"{prefix}_rig", bones)
    mats = {k: material(f"{prefix}_{k}", v) for k, v in s["colours"].items()}
    boxes = list(s["boxes"])
    w = s["leg_w"] / 2
    for bone, x, y in (("leg_FL", lx, fy), ("leg_FR", -lx, fy), ("leg_BL", lx, by), ("leg_BR", -lx, by)):
        boxes.append((bone, "leg", (x - w, y - w, 0.0), (x + w, y + w, hip + 0.02)))
        boxes.append((bone, "paw", (x - w - 0.005, y - w - 0.005, 0.0), (x + w + 0.005, y + w + 0.005, 0.05)))
    mesh = boxes_mesh(f"{prefix}_mesh", boxes, mats)
    skin(mesh, arm)
    collar = boxes_mesh(f"{prefix}_collar", s["collar"], {"collar": material("collar", (0.75, 0.08, 0.06)),
                                                         "tag": material("tag", (0.95, 0.8, 0.2))})
    skin(collar, arm)

    # The clips. A leg swings +a forward; the pairs across the body swing together.
    a, r = s["swing"], s["run_swing"]
    wag = [(0.0, (-0.35, 0.0, 0.0), None), (0.25, (-0.35, 0.35, 0.0), None),
           (0.5, (-0.35, 0.0, 0.0), None), (0.75, (-0.35, -0.35, 0.0), None)]
    clip(arm, prefix, "idle", 2.0, {
        "tail": [(0.0, (-0.3, 0.0, 0.0), None), (0.5, (-0.3, 0.12, 0.0), None)],
        "head": [(0.0, (0.0, 0.0, 0.0), None), (0.5, (0.06, 0.0, 0.08), None)],
    })

    def gait(swing, bob):
        return {
            "leg_FL": [(0.0, (swing, 0, 0), None), (0.5, (-swing, 0, 0), None)],
            "leg_BR": [(0.0, (swing, 0, 0), None), (0.5, (-swing, 0, 0), None)],
            "leg_FR": [(0.0, (-swing, 0, 0), None), (0.5, (swing, 0, 0), None)],
            "leg_BL": [(0.0, (-swing, 0, 0), None), (0.5, (swing, 0, 0), None)],
            "root": [(0.0, (0, 0, 0), (0, 0, 0)), (0.25, (0, 0, 0), (0, bob, 0)),
                     (0.5, (0, 0, 0), (0, 0, 0)), (0.75, (0, 0, 0), (0, bob, 0))],
            "tail": wag,
        }
    clip(arm, prefix, "walk", s["walk_s"], gait(a, 0.015))
    clip(arm, prefix, "run", s["walk_s"] * 0.6, gait(r, 0.04))

    # Sitting: the body up at the front about its back hips, lowered onto its
    # haunches, the back legs folded forward under it and the front ones
    # straight down again. The root's location is in its own space: its Y
    # is the world's Z.
    t = s["sit_tilt"]
    sit = {
        "root": [(0.0, (0, 0, 0), (0, -s["sit_drop"], 0))],
        "body": [(0.0, (t, 0, 0), None)],
        "leg_BL": [(0.0, (math.pi / 2 - t, 0, 0), None)],
        "leg_BR": [(0.0, (math.pi / 2 - t, 0, 0), None)],
        "leg_FL": [(0.0, (-t, 0, 0), None)],
        "leg_FR": [(0.0, (-t, 0, 0), None)],
        "head": [(0.0, (-t * 0.6, 0, 0), None)],
        "tail": [(0.0, (0.4, 0.0, 0.0), None), (0.5, (0.4, 0.25, 0.0), None)],
    }
    clip(arm, prefix, "sit", 2.0, sit)
    tip_over(arm, [mesh])
    return arm, mesh, collar


DOG = {
    "colours": {"fur": (0.86, 0.84, 0.80), "fur_dark": (0.62, 0.58, 0.54), "leg": (0.80, 0.78, 0.74),
                "paw": (0.55, 0.52, 0.48), "eye": (0.08, 0.06, 0.05), "nose": (0.1, 0.08, 0.08),
                "tongue": (0.85, 0.35, 0.4)},
    "leg": 0.38, "leg_w": 0.12, "leg_x": 0.11, "front_y": 0.20, "back_y": -0.26,
    "neck_y": 0.33, "neck_z": 0.62, "tail_y": -0.36, "tail_z": 0.62,
    "swing": 0.5, "run_swing": 0.85, "walk_s": 0.8, "sit_tilt": 0.5, "sit_drop": 0.2,
    "boxes": [
        ("body", "fur", (-0.18, -0.36, 0.36), (0.18, 0.12, 0.66)),
        ("body", "fur_dark", (-0.14, -0.30, 0.66), (0.14, 0.05, 0.68)),   # a darker back
        ("body", "fur", (-0.22, 0.06, 0.34), (0.22, 0.34, 0.74)),          # the ruff
        ("head", "fur", (-0.17, 0.34, 0.50), (0.17, 0.62, 0.82)),
        ("head", "fur", (-0.09, 0.62, 0.52), (0.09, 0.78, 0.66)),          # snout
        ("head", "nose", (-0.04, 0.78, 0.60), (0.04, 0.80, 0.66)),
        ("head", "tongue", (-0.04, 0.66, 0.49), (0.04, 0.74, 0.52)),
        ("head", "eye", (-0.13, 0.62, 0.70), (-0.06, 0.63, 0.75)),
        ("head", "eye", (0.06, 0.62, 0.70), (0.13, 0.63, 0.75)),
        ("head", "fur_dark", (-0.16, 0.42, 0.82), (-0.07, 0.48, 0.94)),    # ears
        ("head", "fur_dark", (0.07, 0.42, 0.82), (0.16, 0.48, 0.94)),
        ("tail", "fur", (-0.05, -0.64, 0.57), (0.05, -0.36, 0.67)),
        ("tail", "fur_dark", (-0.051, -0.66, 0.57), (0.051, -0.60, 0.67)),
    ],
    "collar": [("head", "collar", (-0.21, 0.33, 0.46), (0.21, 0.43, 0.70)),
               ("head", "tag", (-0.04, 0.43, 0.43), (0.04, 0.45, 0.51))],
}

CAT = {
    "colours": {"fur": (0.93, 0.62, 0.28), "fur_dark": (0.72, 0.40, 0.14), "leg": (0.93, 0.62, 0.28),
                "paw": (0.98, 0.95, 0.9), "eye": (0.25, 0.8, 0.3), "nose": (0.95, 0.55, 0.6),
                "chin": (0.98, 0.95, 0.9)},
    "leg": 0.22, "leg_w": 0.08, "leg_x": 0.07, "front_y": 0.22, "back_y": -0.22,
    "neck_y": 0.30, "neck_z": 0.40, "tail_y": -0.30, "tail_z": 0.40,
    "swing": 0.55, "run_swing": 0.9, "walk_s": 0.6, "sit_tilt": 0.7, "sit_drop": 0.12,
    "boxes": [
        ("body", "fur", (-0.12, -0.32, 0.21), (0.12, 0.30, 0.42)),
        ("body", "fur_dark", (-0.121, -0.20, 0.30), (0.121, -0.14, 0.422)),   # stripes
        ("body", "fur_dark", (-0.121, -0.02, 0.30), (0.121, 0.04, 0.422)),
        ("body", "fur_dark", (-0.121, 0.16, 0.30), (0.121, 0.22, 0.422)),
        ("head", "fur", (-0.13, 0.28, 0.30), (0.13, 0.50, 0.52)),
        ("head", "chin", (-0.07, 0.50, 0.31), (0.07, 0.56, 0.39)),           # muzzle
        ("head", "nose", (-0.025, 0.56, 0.36), (0.025, 0.57, 0.39)),
        ("head", "eye", (-0.10, 0.50, 0.42), (-0.04, 0.51, 0.46)),
        ("head", "eye", (0.04, 0.50, 0.42), (0.10, 0.51, 0.46)),
        ("head", "fur_dark", (-0.12, 0.34, 0.52), (-0.05, 0.40, 0.60)),     # ears
        ("head", "fur_dark", (0.05, 0.34, 0.52), (0.12, 0.40, 0.60)),
        ("tail", "fur", (-0.03, -0.70, 0.38), (0.03, -0.30, 0.44)),
        ("tail", "fur_dark", (-0.031, -0.72, 0.38), (0.031, -0.62, 0.44)),
    ],
    "collar": [("head", "collar", (-0.15, 0.27, 0.27), (0.15, 0.34, 0.42)),
               ("head", "tag", (-0.025, 0.34, 0.25), (0.025, 0.36, 0.30))],
}


def export(arm, objs, path):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    for o in [arm] + objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS",
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_yup=True, export_apply=False, export_materials="EXPORT",
                              export_image_format="NONE", export_skins=True,
                              export_def_bones=False)


def bobber(name, sink, at_x):
    """A rod's float, a block model (mods/pets): red over white, inside its
    cell with the middle of the cell at x, y = 0.5, -0.5 in Blender (the cell's
    middle once Y is up) and the water's surface at z = 0. `sink` lowers it for
    a bite."""
    c, w = 0.5, 0.07
    boxes = [("", "white", (c - w, -c - w, -0.07 - sink), (c + w, -c + w, 0.04 - sink)),
             ("", "red", (c - w, -c - w, 0.04 - sink), (c + w, -c + w, 0.14 - sink)),
             ("", "red", (c - 0.015, -c - 0.015, 0.14 - sink), (c + 0.015, -c + 0.015, 0.22 - sink))]
    obj = boxes_mesh(name, boxes, {"white": material("bobber_white", (0.95, 0.95, 0.95)),
                                   "red": material("bobber_red", (0.85, 0.1, 0.08))})
    obj.vertex_groups.clear()
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, f"{name}.gltf"), export_format="GLTF_SEPARATE",
                              use_selection=True, export_animations=False, export_yup=True,
                              export_materials="EXPORT", export_image_format="NONE")
    obj.location.x = at_x   # beside the pets in the file


if __name__ == "__main__":   # tools/make_animal_models.py borrows the helpers above
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = FPS
    made = {"dog": animal("dog", DOG), "cat": animal("cat", CAT)}
    os.makedirs(OUT, exist_ok=True)
    for name, (arm, mesh, collar) in made.items():
        # Side by side in the file, each exported where it stands: the origin.
        export(arm, [mesh], os.path.join(OUT, f"{name}.gltf"))
        export(arm, [mesh, collar], os.path.join(OUT, f"{name}_tame.gltf"))
    made["cat"][0].location.x = 1.2
    bobber("bobber", 0.0, 2.2)
    bobber("bobber_bite", 0.14, 2.6)
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("dog, dog_tame, cat, cat_tame, bobber, bobber_bite in", os.path.normpath(OUT))
