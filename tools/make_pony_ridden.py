# The pony as worn while ridden (mods/animals, core.player_wear): the boxes of
# tools/make_pony_model.py on bones of its own -- pony_body under the robot
# rig's "root" (the feet), pony_head and four legs under it -- with three clips
# the client plays on the bones a wearer has not got (draw_worn), picked by how
# fast the rider goes: worn_idle (a nod and a swish), worn_walk (a four-beat
# walk, diagonal legs together) and worn_run (a canter, the body rocking).
# Front is +Y, sizes in sixteenths of a node as there.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_pony_ridden.py"
#
# Writes mods/animals/pony_ridden.gltf.
import math
import os

import bmesh
import bpy
from mathutils import Quaternion, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "animals")
U = 1 / 16
FPS = 24

COLOURS = {"coat": (0.52, 0.30, 0.16), "belly": (0.60, 0.38, 0.22), "muzzle": (0.30, 0.20, 0.14),
           "mane": (0.10, 0.07, 0.05), "eye": (0.06, 0.05, 0.04), "hoof": (0.16, 0.13, 0.10),
           "blaze": (0.94, 0.92, 0.88), "leather": (0.36, 0.20, 0.10), "iron": (0.70, 0.70, 0.72)}
LEGS = {"pony_leg_FL": (3.5, 7), "pony_leg_FR": (-3.5, 7), "pony_leg_BL": (3.5, -7), "pony_leg_BR": (-3.5, -7)}
# name, parent, head, tail (px)
BONES = [("pony_body", "root", (0, 0, 10), (0, 4, 10)),
         ("pony_head", "pony_body", (0, 10, 15), (0, 14, 19))] + \
        [(n, "pony_body", (x, y, 7), (x, y, 1)) for n, (x, y) in LEGS.items()]
BOXES = []
for n, (x, y) in LEGS.items():
    BOXES.append((n, "coat", (x - 1.5, y - 1.5, 2), (x + 1.5, y + 1.5, 7)))
    BOXES.append((n, "hoof", (x - 1.55, y - 1.55, 0), (x + 1.55, y + 1.55, 2)))
BOXES += [
    ("pony_body", "coat", (-5, -10, 7), (5, 10, 14)),
    ("pony_body", "belly", (-4, -8, 6.8), (4, 8, 7.2)),
    ("pony_body", "leather", (-5.4, -3, 13.5), (5.4, 3, 14.6)),
    ("pony_body", "leather", (-5.6, -2.5, 9), (-5, 2.5, 13.5)),
    ("pony_body", "leather", (5, -2.5, 9), (5.6, 2.5, 13.5)),
    ("pony_body", "iron", (-6.2, -0.8, 7.5), (-5.4, 0.8, 8.2)),
    ("pony_body", "iron", (5.4, -0.8, 7.5), (6.2, 0.8, 8.2)),
    ("pony_body", "mane", (-0.8, -11, 8), (0.8, -10, 14)),
    ("pony_body", "mane", (-1.2, -11.6, 5), (1.2, -10.6, 9.5)),
    ("pony_head", "coat", (-2.5, 8, 11), (2.5, 13, 19)),
    ("pony_head", "coat", (-3, 12, 16), (3, 20, 22)),
    ("pony_head", "muzzle", (-2.5, 18, 15.5), (2.5, 21.5, 19)),
    ("pony_head", "blaze", (-0.8, 19.9, 17), (0.8, 20.2, 21.5)),
    ("pony_head", "eye", (-3.05, 17, 19), (-2.95, 18, 20)),
    ("pony_head", "eye", (2.95, 17, 19), (3.05, 18, 20)),
    ("pony_head", "coat", (-2.6, 13, 22), (-1.4, 14.2, 24.5)),
    ("pony_head", "coat", (1.4, 13, 22), (2.6, 14.2, 24.5)),
    ("pony_head", "mane", (-0.8, 8, 17), (0.8, 13.5, 23)),
]


def material(name, rgb):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Roughness"].default_value = 0.85
    return m


# Each clip: seconds round, and the turn of each bone (about its own X: a leg
# forward is +) and the body's lift at a phase 0..1.
def walk(ph):
    w = 2 * math.pi * ph
    a = 0.42
    return {"pony_leg_FL": a * math.sin(w), "pony_leg_BR": a * math.sin(w),
            "pony_leg_FR": -a * math.sin(w), "pony_leg_BL": -a * math.sin(w),
            "pony_head": 0.06 * math.sin(2 * w), "pony_body": 0.0}, 0.012 * abs(math.sin(w))


def run(ph):
    w = 2 * math.pi * ph
    return {"pony_leg_FL": 0.75 * math.sin(w), "pony_leg_FR": 0.75 * math.sin(w - 0.5),
            "pony_leg_BL": -0.7 * math.sin(w - 1.2), "pony_leg_BR": -0.7 * math.sin(w - 1.7),
            "pony_head": 0.12 * math.sin(w + 0.6), "pony_body": 0.07 * math.sin(w)}, 0.035 * (1 + math.sin(w)) / 2


def idle(ph):
    w = 2 * math.pi * ph
    return {"pony_head": 0.05 * math.sin(w) - 0.03, "pony_body": 0.0, "pony_leg_FL": 0.0, "pony_leg_FR": 0.0,
            "pony_leg_BL": 0.0, "pony_leg_BR": 0.0}, 0.004 * math.sin(w)


CLIPS = [("worn_idle", idle, 3.0), ("worn_walk", walk, 0.8), ("worn_run", run, 0.45)]
NAME, BODY = "pony_ridden", "pony_body"   # tools/make_ladybird_mount.py sets these and the tables above


def main():
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, "mods", "robot", "robot.gltf"))
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    if rig.animation_data:
        rig.animation_data.action = None
        for tr in list(rig.animation_data.nla_tracks):
            rig.animation_data.nla_tracks.remove(tr)
    rig.data.pose_position = "POSE"
    Wi = rig.matrix_world.inverted()
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    eb = rig.data.edit_bones
    for name, parent, head, tail in BONES:
        b = eb.new(name)
        b.head = Wi @ Vector([c * U for c in head])
        b.tail = Wi @ Vector([c * U for c in tail])
        b.roll = 0.0
        b.parent = eb[parent]
    bpy.ops.object.mode_set(mode="OBJECT")

    names = list(COLOURS)
    me = bpy.data.meshes.new(NAME)
    ob = bpy.data.objects.new(NAME, me)
    bpy.context.scene.collection.objects.link(ob)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    group = {g.name: g.index for g in ob.vertex_groups}
    bm = bmesh.new()
    dl = bm.verts.layers.deform.verify()
    for bone, mat, lo, hi in BOXES:
        ret = bmesh.ops.create_cube(bm, size=1.0)
        c = [(lo[i] + hi[i]) / 2 * U for i in range(3)]
        s = [(hi[i] - lo[i]) * U for i in range(3)]
        for v in ret["verts"]:
            v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
            v[dl][group[bone]] = 1.0
        for f in {f for v in ret["verts"] for f in v.link_faces}:
            f.material_index = names.index(mat)
    bm.to_mesh(me)
    bm.free()
    for n in names:
        me.materials.append(material("Pony_" + n, COLOURS[n]))
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig

    # The clips, as NLA tracks named as they are.
    rig.animation_data_create()
    for clip, fn, seconds in CLIPS:
        act = bpy.data.actions.new(clip)
        rig.animation_data.action = act
        frames = round(FPS * seconds)
        for f in range(frames + 1):
            turns, lift = fn(f / frames)
            for bone, angle in turns.items():
                pb = rig.pose.bones[bone]
                pb.rotation_mode = "QUATERNION"
                pb.rotation_quaternion = Quaternion(*angle) if isinstance(angle, tuple) else Quaternion((1, 0, 0), angle)
                pb.keyframe_insert("rotation_quaternion", frame=f + 1)
            body = rig.pose.bones[BODY]
            body.location = (0, 0, 0)
            body.location[2] = lift   # the bone points +Y, roll 0: its own Z is up
            body.keyframe_insert("location", frame=f + 1)
        track = rig.animation_data.nla_tracks.new()
        track.name = clip
        track.strips.new(clip, 1, act)
        rig.animation_data.action = None
    bpy.context.scene.render.fps = FPS
    for x in bpy.data.objects:
        x.select_set(False)
    ob.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(
        filepath=os.path.join(OUT, NAME + ".gltf"), export_format="GLTF_SEPARATE",
        use_selection=True, export_animations=True, export_animation_mode="NLA_TRACKS", export_skins=True,
        export_apply=True, export_yup=True)


if __name__ == "__main__":
    main()
