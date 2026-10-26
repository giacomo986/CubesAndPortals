# The things a character of games/arpg breaks for what is inside: a barrel, a
# crate and an urn, each made of pieces that fly apart.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_arpg_props.py"
#
# Each is a rig of its own -- a root and one bone a piece, every piece bound
# whole to its bone -- with three clips: "idle" (standing), "hurt" (a jolt when
# struck) and "death" (the pieces thrown out and up, falling, tumbling and
# shrinking to nothing in a second, under the 1.2 s the client fades a dead
# creature in). arpg_core registers them as workers, arpg_game puts them about
# the zones; exported as games/arpg/mods/arpg_core/prop_<kind>.gltf. Z up,
# metres, the floor at z 0. Leaves the work in ~/kart_assets/arpg_props.blend.
import math
import os
import random

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "games", "arpg", "mods", "arpg_core")
FPS = 24
DEATH = 1.0   # seconds

MATS = {
    "wood": ((0.47, 0.31, 0.16), 0.0, 0.85),
    "dark_wood": ((0.30, 0.19, 0.10), 0.0, 0.85),
    "iron": ((0.36, 0.36, 0.38), 0.9, 0.5),
    "clay": ((0.62, 0.34, 0.20), 0.0, 0.9),
    "glaze": ((0.20, 0.32, 0.36), 0.2, 0.4),
    "rope": ((0.62, 0.52, 0.34), 0.0, 0.95),
}


def material(name):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    rgb, metal, rough = MATS[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Metallic"].default_value = metal
    p.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1)
    return m


# Shapes: each fills a bmesh with one piece and returns its verts.
def box(bm, lo, hi):
    ret = bmesh.ops.create_cube(bm, size=1.0)
    c = [(lo[i] + hi[i]) / 2 for i in range(3)]
    s = [hi[i] - lo[i] for i in range(3)]
    for v in ret["verts"]:
        v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
    return ret["verts"]


def turned_box(bm, centre, size, yaw):
    ret = bmesh.ops.create_cube(bm, size=1.0)
    m = Matrix.Translation(centre) @ Matrix.Rotation(yaw, 4, "Z")
    for v in ret["verts"]:
        v.co = m @ Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    return ret["verts"]


def ring(bm, z0, z1, r_in, r_out, segs, a0=0.0, a1=2 * math.pi):
    """A band round the Z axis from angle a0 to a1: an annulus extruded from z0 to z1."""
    verts = []
    rows = []
    n = segs
    for i in range(n + 1):
        a = a0 + (a1 - a0) * i / n
        c, s = math.cos(a), math.sin(a)
        rows.append([bm.verts.new((r * c, r * s, z)) for r, z in
                     ((r_in, z0), (r_out, z0), (r_out, z1), (r_in, z1))])
    for i in range(n):
        a, b = rows[i], rows[i + 1]
        for k in range(4):
            j = (k + 1) % 4
            bm.faces.new((a[k], b[k], b[j], a[j]))
    if a1 - a0 < 2 * math.pi - 1e-6:   # the cut ends of a shard
        bm.faces.new(rows[0])
        bm.faces.new(list(reversed(rows[-1])))
    for r in rows:
        verts += r
    return verts


def disc(bm, z0, z1, r, segs):
    ret = bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r, radius2=r, depth=z1 - z0)
    for v in ret["verts"]:
        v.co.z += (z0 + z1) / 2
    return ret["verts"]


# The pieces of each: (name, material, maker). A piece's bone is at its middle.
def barrel():
    out = []
    staves = 10
    for i in range(staves):
        a0, a1 = i * 2 * math.pi / staves + 0.01, (i + 1) * 2 * math.pi / staves - 0.01
        mid = (a0 + a1) / 2
        out.append((f"stave{i}", "wood", lambda bm, a0=a0, a1=a1: ring(bm, 0.02, 0.88, 0.27, 0.32, 2, a0, a1)))
        # the bulge: a second, shorter band over the middle of the stave
        out.append((f"belly{i}", "wood", lambda bm, a0=a0, a1=a1: ring(bm, 0.22, 0.68, 0.30, 0.345, 2, a0, a1)))
    for k, z in enumerate((0.14, 0.74)):
        out.append((f"hoop{k}", "iron", lambda bm, z=z: ring(bm, z, z + 0.05, 0.325, 0.36, 16)))
    out.append(("lid", "dark_wood", lambda bm: disc(bm, 0.80, 0.84, 0.29, 14)))
    return out


def crate():
    S, T = 0.40, 0.05   # half its side, a plank's thickness
    out = []
    for k, (lo, hi) in enumerate([((-S, -S, 0.0), (S, S, T)), ((-S, -S, 0.8 - T), (S, S, 0.8))]):
        out.append((f"lid{k}", "wood", lambda bm, lo=lo, hi=hi: box(bm, lo, hi)))
    sides = [((-S, -S, T), (S, -S + T, 0.8 - T)), ((-S, S - T, T), (S, S, 0.8 - T)),
             ((-S, -S + T, T), (-S + T, S - T, 0.8 - T)), ((S - T, -S + T, T), (S, S - T, 0.8 - T))]
    for k, (lo, hi) in enumerate(sides):
        # two planks a side, a gap between them
        for j, (z0, z1) in enumerate(((T, 0.39), (0.41, 0.8 - T))):
            lo2, hi2 = (lo[0], lo[1], z0), (hi[0], hi[1], z1)
            out.append((f"side{k}_{j}", "wood", lambda bm, lo2=lo2, hi2=hi2: box(bm, lo2, hi2)))
    for k, (x, y) in enumerate(((-1, -1), (1, -1), (-1, 1), (1, 1))):
        lo = (x * S - 0.035 * (x > 0) - 0.005 * (x < 0), y * S - 0.035 * (y > 0) - 0.005 * (y < 0), 0.0)
        lo = (min(x * S, x * S - x * 0.04) - 0.005, min(y * S, y * S - y * 0.04) - 0.005, 0.0)
        hi = (max(x * S, x * S - x * 0.04) + 0.005, max(y * S, y * S - y * 0.04) + 0.005, 0.8)
        out.append((f"post{k}", "dark_wood", lambda bm, lo=lo, hi=hi: box(bm, lo, hi)))
    out.append(("strap0", "iron", lambda bm: box(bm, (-S - 0.01, -0.03, -0.005), (S + 0.01, 0.03, 0.805))))
    return out


def urn():
    out = []
    # The body: shards, three rows of five, the widest in the middle.
    rows = ((0.02, 0.20, 0.17, 0.24), (0.20, 0.42, 0.24, 0.27), (0.42, 0.58, 0.27, 0.20))
    for r, (z0, z1, ra, rb) in enumerate(rows):
        n = 5
        for i in range(n):
            a0, a1 = (i + 0.5 * r) * 2 * math.pi / n + 0.01, (i + 1 + 0.5 * r) * 2 * math.pi / n - 0.01
            rad = (ra + rb) / 2
            mat = "glaze" if r == 1 else "clay"
            out.append((f"shard{r}_{i}", mat, lambda bm, z0=z0, z1=z1, rad=rad, a0=a0, a1=a1:
                        ring(bm, z0, z1, rad - 0.035, rad, 3, a0, a1)))
    out.append(("foot", "clay", lambda bm: ring(bm, 0.0, 0.04, 0.0, 0.16, 12)))
    out.append(("neck", "clay", lambda bm: ring(bm, 0.58, 0.68, 0.12, 0.16, 12)))
    out.append(("lip", "glaze", lambda bm: ring(bm, 0.68, 0.71, 0.11, 0.19, 12)))
    out.append(("cord", "rope", lambda bm: ring(bm, 0.60, 0.625, 0.16, 0.175, 12)))
    return out


PROPS = {"barrel": barrel, "crate": crate, "urn": urn}


def centre_of(verts):
    c = Vector()
    for v in verts:
        c += v.co
    return c / max(1, len(verts))


def make(kind, pieces):
    bpy.ops.wm.read_homefile(use_empty=True)
    random.seed(hash(kind) & 0xffff)
    arm = bpy.data.armatures.new(f"prop_{kind}_rig")
    rig = bpy.data.objects.new(f"prop_{kind}_rig", arm)
    bpy.context.scene.collection.objects.link(rig)
    me = bpy.data.meshes.new(f"prop_{kind}")
    ob = bpy.data.objects.new(f"prop_{kind}", me)
    bpy.context.scene.collection.objects.link(ob)

    bm = bmesh.new()
    dl = bm.verts.layers.deform.verify()
    centres = {}
    mats = []
    for name, mat, maker in pieces:
        verts = maker(bm)
        bm.verts.ensure_lookup_table()
        centres[name] = centre_of(verts)
        g = ob.vertex_groups.new(name=name)
        for v in verts:
            v[dl][g.index] = 1.0
        if mat not in mats:
            mats.append(mat)
        for f in {f for v in verts for f in v.link_faces}:
            f.material_index = mats.index(mat)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(material(m))

    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    root = arm.edit_bones.new("root")
    root.head, root.tail = (0, 0, 0), (0, 0, 0.2)
    for name, c in centres.items():
        b = arm.edit_bones.new(name)
        b.head, b.tail = c, c + Vector((0, 0, 0.08))
        b.parent = root
    bpy.ops.object.mode_set(mode="OBJECT")
    ob.parent = rig
    ob.modifiers.new("rig", "ARMATURE").object = rig

    rig.animation_data_create()
    for pb in rig.pose.bones:
        pb.rotation_mode = "QUATERNION"

    def key(frame):
        for pb in rig.pose.bones:
            pb.keyframe_insert("location", frame=frame)
            pb.keyframe_insert("rotation_quaternion", frame=frame)
            pb.keyframe_insert("scale", frame=frame)

    def reset():
        for pb in rig.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)

    clips = {}
    # idle: standing as made
    act = bpy.data.actions.new("idle")
    rig.animation_data.action = act
    reset(); key(1); key(13)
    clips["idle"] = act
    # hurt: a jolt, tipped away and back
    act = bpy.data.actions.new("hurt")
    rig.animation_data.action = act
    for f, ang in ((1, 0.0), (3, -0.16), (6, 0.08), (9, 0.0)):
        reset()
        rig.pose.bones["root"].rotation_quaternion = Euler((ang, 0, ang * 0.4)).to_quaternion()
        key(f)
    clips["hurt"] = act
    # death: every piece thrown out from the middle and up, falling, tumbling, gone
    act = bpy.data.actions.new("death")
    rig.animation_data.action = act
    frames = round(DEATH * FPS)
    throws = {}
    for name, c in centres.items():
        out = Vector((c.x, c.y, 0))
        out = out.normalized() if out.length > 1e-3 else Vector((random.uniform(-1, 1), random.uniform(-1, 1), 0)).normalized()
        speed = random.uniform(1.4, 2.6)
        throws[name] = (out * speed, random.uniform(1.5, 3.2), Vector((random.uniform(-1, 1) for _ in range(3))).normalized(),
                        random.uniform(6, 14))
    for f in range(frames + 1):
        t = f / FPS
        reset()
        for name, c in centres.items():
            pb = rig.pose.bones[name]
            v, up, axis, spin = throws[name]
            z = up * t - 4.9 * t * t
            ground = -c.z + 0.04
            z = max(z, ground)
            pos = Vector((v.x * t, v.y * t, z))
            # the bone points up its own Y: world Z is its Y, world Y its -Z
            pb.location = Vector((pos.x, pos.z, -pos.y))
            pb.rotation_quaternion = Matrix.Rotation(spin * t, 3, axis).to_quaternion()
            shrink = 1.0 if t < DEATH * 0.6 else max(0.0, 1.0 - (t - DEATH * 0.6) / (DEATH * 0.4))
            pb.scale = (shrink, shrink, shrink)
        key(f + 1)
    clips["death"] = act

    rig.animation_data.action = None
    for name, act in clips.items():
        tr = rig.animation_data.nla_tracks.new()
        tr.name = name
        tr.strips.new(name, 1, act)
    bpy.context.scene.render.fps = FPS
    for x in bpy.data.objects:
        x.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, f"prop_{kind}.gltf"), export_format="GLTF_SEPARATE",
                              use_selection=True, export_animations=True, export_animation_mode="NLA_TRACKS",
                              export_yup=True, export_skins=True)
    print(f"PROP {kind}: {len(centres)} pieces, {len(me.polygons)} faces")
    bpy.ops.wm.save_as_mainfile(filepath=os.path.expanduser(f"~/kart_assets/arpg_prop_{kind}.blend"))


for kind, maker in PROPS.items():
    make(kind, maker())
