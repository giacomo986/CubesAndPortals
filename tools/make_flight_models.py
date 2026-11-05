# The glider and the balloon a body wears (mods/glider, mods/balloon), built
# on the robot's own rig and exported beside each mod.
#
#   blender --background --python tools/make_flight_models.py
#
# As tools/make_boat_model.py: mods/robot/robot.gltf is imported for its bones
# and every vertex is bound whole to "root", the feet -- a wing over the head
# or a basket round the feet turns with the body and does not swing with an
# arm. Front is +Y, up +Z, metres of the rig's world space.
#
#   glider   a delta wing over the head, red and white panels from the nose
#            back to the tips, a keel tube under it and the control frame
#            (two uprights and the bar) down in front of the chest.
#   balloon  a wicker basket round the feet, four ropes up to the envelope,
#            a burner in between, and the envelope: a ball of twelve gores,
#            yellow and red, its mouth open at the bottom. Under the burner a
#            valve handle the body's right hand pulls (balloon_burn,
#            tools/make_flight_clips.py), and over it the flame, on a bone of
#            its own ("flame", the robot has none) that its clips worn_idle,
#            worn_walk and worn_run grow and shrink: balloon.gltf's a pilot
#            flame flickering low, balloon_burn.gltf's -- worn while the
#            balloon rises -- the burner roaring up into the envelope.
import math
import os

import bmesh
import bpy
from mathutils import Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")


def material(name, rgb, rough=0.8, metal=0.0, glow=False):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metal
    if glow:   # its own light (the engine draws an emissive material at full)
        p.inputs["Emission Color"].default_value = (*rgb, 1)
        p.inputs["Emission Strength"].default_value = 1.0
    m.diffuse_color = (*rgb, 1)
    return m


def new_object(name, rig, mats):
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    for m in mats:
        me.materials.append(bpy.data.materials[m])
    return ob


def finish(ob, rig, bm, bone_of=None):
    """Every vertex bound whole to "root", or to bone_of(vertex)'s bone."""
    dl = bm.verts.layers.deform.verify()
    root = ob.vertex_groups["root"].index
    for v in bm.verts:
        g = bone_of(v) if bone_of else None
        v[dl][ob.vertex_groups[g].index if g else root] = 1.0
    bm.to_mesh(ob.data)
    bm.free()
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig


def box(bm, lo, hi, mat):
    ret = bmesh.ops.create_cube(bm, size=1.0)
    c = [(lo[i] + hi[i]) / 2 for i in range(3)]
    s = [hi[i] - lo[i] for i in range(3)]
    for v in ret["verts"]:
        v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
    for f in {f for v in ret["verts"] for f in v.link_faces}:
        f.material_index = mat


def rod(bm, a, b, r, mat):
    """A square tube from a to b."""
    a, b = Vector(a), Vector(b)
    ret = bmesh.ops.create_cube(bm, size=1.0)
    axis = b - a
    length = axis.length
    rot = Vector((0, 0, 1)).rotation_difference(axis.normalized()).to_matrix()
    for v in ret["verts"]:
        local = Vector((v.co.x * 2 * r, v.co.y * 2 * r, (v.co.z + 0.5) * length))
        v.co = a + rot @ local
    for f in {f for v in ret["verts"] for f in v.link_faces}:
        f.material_index = mat


def slab(bm, pts, thick, mat):
    """A flat polygon at its points' heights, `thick` deep."""
    lo = [bm.verts.new(Vector(p) - Vector((0, 0, thick / 2))) for p in pts]
    hi = [bm.verts.new(Vector(p) + Vector((0, 0, thick / 2))) for p in pts]
    faces = [bm.faces.new(list(reversed(lo))), bm.faces.new(hi)]
    n = len(pts)
    for i in range(n):
        faces.append(bm.faces.new((lo[i], lo[(i + 1) % n], hi[(i + 1) % n], hi[i])))
    for f in faces:
        f.material_index = mat


def glider(rig):
    ob = new_object("glider", rig, ["Sail_red", "Sail_white", "Tube"])
    bm = bmesh.new()
    nose, tail = (0.0, 1.1, 2.25), (0.0, -0.35, 2.3)
    tip_l, tip_r = (-2.3, -0.75, 2.05), (2.3, -0.75, 2.05)
    mid_l, mid_r = (-1.15, 0.18, 2.17), (1.15, 0.18, 2.17)
    # Four panels, red outside and white in.
    slab(bm, [nose, mid_l, tip_l], 0.03, 0)
    slab(bm, [nose, tail, mid_l], 0.03, 1)
    slab(bm, [nose, mid_r, tail], 0.03, 1)
    slab(bm, [nose, tip_r, mid_r], 0.03, 0)
    slab(bm, [mid_l, tail, tip_l], 0.03, 1)
    slab(bm, [mid_r, tip_r, tail], 0.03, 1)
    # The keel and the leading edges.
    rod(bm, (0, 1.15, 2.2), (0, -0.4, 2.25), 0.025, 2)
    rod(bm, nose, tip_l, 0.022, 2)
    rod(bm, nose, tip_r, 0.022, 2)
    # The control frame: two uprights from the keel down to the bar in front
    # of the chest, where the hands hold it.
    rod(bm, (0, 0.35, 2.2), (-0.45, 0.45, 1.15), 0.02, 2)
    rod(bm, (0, 0.35, 2.2), (0.45, 0.45, 1.15), 0.02, 2)
    rod(bm, (-0.47, 0.45, 1.15), (0.47, 0.45, 1.15), 0.022, 2)
    finish(ob, rig, bm)
    return ob


FLAME_AT = 2.70   # the burner's top, where the flame stands


def cone(bm, base, r, h, sides, mat):
    """A cone standing on `base`, its point h over it."""
    ring = [bm.verts.new((base[0] + r * math.cos(2 * math.pi * i / sides),
                          base[1] + r * math.sin(2 * math.pi * i / sides), base[2])) for i in range(sides)]
    tip = bm.verts.new((base[0], base[1], base[2] + h))
    faces = [bm.faces.new(list(reversed(ring)))]
    for i in range(sides):
        faces.append(bm.faces.new((ring[i], ring[(i + 1) % sides], tip)))
    for f in faces:
        f.material_index = mat


def flame_bone(rig):
    """A bone "flame" on the root at the burner's top, pointing up."""
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    eb = rig.data.edit_bones.new("flame")
    eb.head = rig.matrix_world.inverted() @ Vector((0, 0, FLAME_AT))
    eb.tail = rig.matrix_world.inverted() @ Vector((0, 0, FLAME_AT + 0.3))
    eb.parent = rig.data.edit_bones["root"]
    bpy.ops.object.mode_set(mode="OBJECT")


def balloon(rig):
    flame_bone(rig)
    ob = new_object("balloon", rig, ["Wicker", "Wicker_rim", "Rope", "Envelope_yellow", "Envelope_red", "Tube",
                                     "Flame_outer", "Flame_inner"])
    bm = bmesh.new()
    w, h = 0.62, 0.95
    box(bm, (-w, -w, -0.05), (w, w, 0.02), 0)                 # floor
    box(bm, (-w, -w, 0.0), (-w + 0.06, w, h), 0)              # sides
    box(bm, (w - 0.06, -w, 0.0), (w, w, h), 0)
    box(bm, (-w, -w, 0.0), (w, -w + 0.06, h), 0)
    box(bm, (-w, w - 0.06, 0.0), (w, w, h), 0)
    box(bm, (-w - 0.03, -w - 0.03, h), (w + 0.03, w + 0.03, h + 0.07), 1)   # rim
    top = 3.2   # the envelope's mouth
    for sx in (-1, 1):
        for sy in (-1, 1):
            rod(bm, (sx * w, sy * w, h), (sx * 0.9, sy * 0.9, top), 0.015, 2)
    box(bm, (-0.15, -0.15, 2.45), (0.15, 0.15, FLAME_AT), 5)    # the burner
    # The valve handle hung under it, in reach of the right hand.
    rod(bm, (0.08, 0.08, 2.45), (0.17, 0.12, 1.78), 0.012, 5)
    box(bm, (0.13, 0.08, 1.68), (0.21, 0.16, 1.78), 1)
    # The flame, full size: the bone's clips shrink it.
    cone(bm, (0, 0, FLAME_AT), 0.17, 0.75, 8, 6)
    cone(bm, (0, 0, FLAME_AT + 0.01), 0.09, 0.5, 8, 7)
    # The envelope: twelve gores round, eleven rings up, the bottom ring open.
    cz, r = 5.4, 2.6
    gores, rings = 12, 11
    rows = []
    for j in range(rings + 1):
        phi = math.pi * (0.18 + 0.82 * j / rings)              # from the mouth up to the top
        z = cz - r * math.cos(phi)
        rr = r * math.sin(phi)
        if j == rings:
            rows.append([bm.verts.new((0, 0, cz + r))])
            break
        rows.append([bm.verts.new((rr * math.cos(2 * math.pi * i / gores),
                                   rr * math.sin(2 * math.pi * i / gores), z)) for i in range(gores)])
    for j in range(rings):
        lo, hi = rows[j], rows[j + 1]
        for i in range(gores):
            k = (i + 1) % gores
            f = bm.faces.new((lo[i], lo[k], hi[k], hi[i])) if len(hi) > 1 else bm.faces.new((lo[i], lo[k], hi[0]))
            f.material_index = 3 if i % 2 == 0 else 4
    flames = {6, 7}
    in_flame = {v for f in bm.faces if f.material_index in flames for v in f.verts}
    finish(ob, rig, bm, lambda v: "flame" if v in in_flame else None)
    ob.data.materials[3].use_backface_culling = False
    return ob


def flame_clips(rig, big):
    """worn_idle, worn_walk and worn_run, all one flicker of the flame bone:
    a pilot flame low (0.15 to 0.3 of the full height) or the burner on (0.8
    to 1.25), 0.4 s round, never twice the same height in a row."""
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    rig.animation_data_create()
    rig.animation_data.action = None
    for tr in list(rig.animation_data.nla_tracks):
        rig.animation_data.nla_tracks.remove(tr)
    pb = rig.pose.bones["flame"]
    pb.rotation_mode = "QUATERNION"
    heights = [1.0, 0.55, 0.9, 0.3, 0.8, 0.0, 0.7, 0.45, 1.0]   # through the flicker, back to the start
    lo, hi = (0.8, 1.25) if big else (0.15, 0.3)
    for name in ("worn_idle", "worn_walk", "worn_run"):
        act = bpy.data.actions.new(f"balloon_{'burn' if big else 'pilot'}_{name}")
        rig.animation_data.action = act
        for i, k in enumerate(heights):
            up = lo + (hi - lo) * k
            wide = 0.75 + 0.25 * up / hi
            pb.scale = (wide, up, wide)   # the bone's Y is up the flame
            pb.keyframe_insert("scale", frame=1 + i * 1.2)
        rig.animation_data.action = None
        track = rig.animation_data.nla_tracks.new()
        track.name = name
        track.strips.new(name, 1, act)
    pb.scale = (1, 1, 1)


def export(ob, rig, path, animations=False):
    for x in bpy.data.objects:
        x.select_set(False)
    ob.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = ob
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=animations, export_animation_mode="NLA_TRACKS",
                              export_skins=True, export_apply=True, export_yup=True)
    print("wrote", path)


def main():
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, "mods", "robot", "robot.gltf"))
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    rig.data.pose_position = "REST"
    material("Sail_red", (0.80, 0.12, 0.10))
    material("Sail_white", (0.92, 0.90, 0.86))
    material("Tube", (0.55, 0.57, 0.60), rough=0.4, metal=0.8)
    material("Wicker", (0.62, 0.45, 0.24), rough=0.95)
    material("Wicker_rim", (0.42, 0.28, 0.14), rough=0.9)
    material("Rope", (0.80, 0.74, 0.60))
    material("Envelope_yellow", (0.95, 0.76, 0.15), rough=0.6)
    material("Envelope_red", (0.80, 0.15, 0.12), rough=0.6)
    material("Flame_outer", (1.0, 0.42, 0.08), glow=True)
    material("Flame_inner", (1.0, 0.88, 0.45), glow=True)
    g = glider(rig)
    export(g, rig, os.path.join(ROOT, "mods", "glider", "glider.gltf"))
    b = balloon(rig)
    flame_clips(rig, False)
    export(b, rig, os.path.join(ROOT, "mods", "balloon", "balloon.gltf"), animations=True)
    flame_clips(rig, True)
    export(b, rig, os.path.join(ROOT, "mods", "balloon", "balloon_burn.gltf"), animations=True)


main()
