# The dragon knight, a player body from Meshy (assets/meshy/knight: concept.png,
# and knight_parts.glb, image-to-3D in smart topology: a hundred separate
# pieces at 12k triangles), on a skeleton of its own proportions with the
# robot's clips:
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_knight_meshy.py"
#
# The mole of 2026-11-07 was bent onto the robot's skeleton -- arms and legs
# stretched to the robot's -- and looked it. Here the skeleton is bent onto the
# knight instead: the robot's bones, same names, same orientations, moved to
# where the knight's joints are (K_*). A clip turns bones, so it plays on any
# lengths; the translations it also carries are each moved by what its bone
# moved at rest (the hips' by how much shorter the legs are). The clips are
# written into the file here (graft), a constant channel as one key, which
# keeps it under the 1 MB a media file may weigh.
#
# Each piece follows one bone whole, as the robot's parts do, unless it is big
# and spans several (the torso, the cape): then its vertices take the nearest
# bone of their part (an arm's never the cape's), blurred. The knight stands in
# an A-pose and the robot's arms hang straight down, so the arms are first
# turned down about their joints by those bones. The texture is read only to
# colour each face flat (COLOURS): the red plates tint_1 and the gold trim
# tint_2, which the player dyes; steel and black stay.
#
# Writes mods/robot/robot_knight.gltf + .bin, and robot_knight_arms (the arms
# seen in first person, on robot_arms.gltf's rig and clips, their shoulders
# where the robot's are in view).
import json
import math
import os
import struct
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Quaternion, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_species import blurred, export, fresh, keep_tools  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "robot")
SRC = os.path.join(ROOT, "assets", "meshy", "knight", "knight_parts.glb")
NAME = "robot_knight"

S = 1.15   # the knight's file is 1.36 tall: 1.56 with its horns, the robot's head
# Its joints, read off front and side renders of that file (x either side,
# depth 0): shoulder, elbow, wrist and fingertips of the A-pose arm, the hip
# joint, knee and ankle, the neck and the top of the horns.
K_SHOULDER, K_ELBOW, K_WRIST, K_TIP = (0.183, 0.896), (0.27, 0.81), (0.426, 0.678), (0.557, 0.522)
K_HIP, K_KNEE, K_ANKLE = (0.087, 0.61), 0.36, 0.07   # the hip under the tassets, below the belt
K_NECK, K_TOP = 1.06, 1.36
SLEEVE = (0.05, 0.042, 0.05)   # radius of the first-person sleeves: upper arm, forearm, cuff
FP_SLIM = 0.6   # first-person arms this much thinner about their bones
FIST, KNUCKLES = 80, (0.075, 0.115)   # degrees a finger folds at each joint, and where they are down the hand
ROBOT_ARM = (1.234 - 0.942, 0.942 - 0.663)   # the robot's upper arm and forearm, head to head
ARM_OUT, ARM_FORWARD = 20, 8   # degrees the rest arm hangs out and ahead
TRIS = 8000   # what the pieces are decimated to
ACROSS = 0.2   # a piece with this share on a second bone spans a joint and bends
RIGID_MAX = 400   # a piece of up to this many vertices follows one bone whole
ONE_COLOUR = 0.7   # a piece this much one colour is all of it
# The flat colours a face takes, sRGB, the texture's own (k-means of it), a
# name's first one its colour (the red a little brighter than painted, which
# reads near black in the game's light): the red plates and the gold trim
# are the player's primary and secondary.
COLOURS = [
    ("tint_1", (0.52, 0.09, 0.10)), ("tint_1", (0.32, 0.05, 0.07)), ("tint_1", (0.40, 0.20, 0.16)),
    ("tint_2", (0.67, 0.60, 0.40)), ("tint_2", (0.65, 0.59, 0.39)), ("tint_2", (0.55, 0.48, 0.33)),
    ("Steel", (0.42, 0.42, 0.44)), ("Steel", (0.43, 0.38, 0.32)), ("Steel", (0.29, 0.30, 0.29)),
    ("Dark", (0.16, 0.15, 0.14)), ("Dark", (0.22, 0.22, 0.20)), ("Dark", (0.14, 0.12, 0.11)),
    ("Skin", (0.83, 0.66, 0.59)),   # only on the face (colours)
]
CAPE_Y = -0.09   # behind this (scaled, facing +Y) a point under the hips is cape, not leg

BONES = {
    "torso": ["hips", "spine", "chest", "neck", "head"],
    "arm_L": ["upper_arm_L", "forearm_L", "hand_L"], "arm_R": ["upper_arm_R", "forearm_R", "hand_R"],
    "leg_L": ["thigh_L", "shin_L", "foot_L"], "leg_R": ["thigh_R", "shin_R", "foot_R"],
}


def k(x, z, side=1):
    """A knight joint in the rig's world."""
    return Vector((side * x * S, 0.0, z * S))


def arm_pose(sx, out=None):
    """The A-pose arm's joints and the rest arm's, shoulder to fingertip: the
    rest arm hangs OUT degrees off straight down (and FORWARD ahead), so the
    hands clear the tassets; bones still point straight down, which leaves
    every clip's arm that much out."""
    out = ARM_OUT if out is None else out
    a = [k(*j, sx) for j in (K_SHOULDER, K_ELBOW, K_WRIST, K_TIP)]
    down = Vector((sx * math.sin(math.radians(out)), math.sin(math.radians(ARM_FORWARD if out else 0)),
                   -math.cos(math.radians(out)))).normalized()
    # the robot's upper arm and forearm, the knight's own hand: its plates sit on
    # arms as long as the ones every clip was made for (a hand on a kart's wheel,
    # a tool where the robot holds it), the gaps a dark sleeve (sleeves)
    lengths = (ROBOT_ARM[0], ROBOT_ARM[1], (a[3] - a[2]).length)
    r = [a[0].copy()]
    for i in range(3):
        r.append(r[-1] + down * lengths[i])
    return a, r


def lerp_z(z):
    """A torso height of the robot's, where the knight has it."""
    pts = [(0.73, K_HIP[1] * S), (1.234, K_SHOULDER[1] * S), (1.287, K_NECK * S), (1.565, K_TOP * S * 0.97)]
    for (r0, k0), (r1, k1) in zip(pts, pts[1:]):
        if z <= r1 or (r1, k1) == pts[-1]:
            return k0 + (z - r0) * (k1 - k0) / (r1 - r0)


def place_rig(rig):
    """The robot's bones moved onto the knight, each keeping its direction."""
    mw = rig.matrix_world
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    old = {b.name: (mw @ b.head, mw @ b.tail) for b in rig.data.edit_bones}
    new = {}
    for n in BONES["torso"]:
        h, t = old[n]
        new[n] = (Vector((0, 0, lerp_z(h.z))), Vector((0, 0, lerp_z(t.z))))
    for side, sx in (("L", -1), ("R", 1)):
        _, r = arm_pose(sx)
        robot = [old["upper_arm_" + side][0], old["forearm_" + side][0], old["hand_" + side][0],
                 old["hand_" + side][0] - Vector((0, 0, 0.21))]
        for i, n in enumerate(BONES["arm_" + side]):
            h, t = old[n]
            scale = (r[i + 1] - r[i]).length / (robot[i + 1] - robot[i]).length
            new[n] = (r[i], r[i] + (t - h) * scale)
        joints = LEGS[side][1]
        for i, n in enumerate(BONES["leg_" + side]):
            h, t = old[n]
            if i < 2:
                scale = (joints[i] - joints[i + 1]).length / (h - old[BONES["leg_" + side][i + 1]][0]).length
            else:
                scale = 1.0
            new[n] = (joints[i], joints[i] + (t - h) * scale)
    bpy.ops.object.mode_set(mode="OBJECT")
    return move(rig, new)


def move(rig, new):
    """Bones put where `new` says (world head, tail), the ones it does not name
    (tools, jet flame, antenna) carried by their parent; what each moved."""
    mw = rig.matrix_world
    inv = mw.inverted()
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    eb = rig.data.edit_bones
    old = {b.name: (mw @ b.head, mw @ b.tail) for b in eb}
    new = {n: v for n, v in new.items() if n in old}
    for b in eb:   # parents come before children in the list
        if b.name not in new and b.parent is not None and b.parent.name in new:
            d = new[b.parent.name][0] - old[b.parent.name][0]
            new[b.name] = (old[b.name][0] + d, old[b.name][1] + d)
    for b in eb:
        if b.name in new:
            roll = b.roll
            b.head, b.tail = inv @ new[b.name][0], inv @ new[b.name][1]
            b.roll = roll
    bpy.ops.object.mode_set(mode="OBJECT")
    return {n: (new[n], new[n][0] - old[n][0]) for n in new}


LEGS = {}   # side: (joints as made, joints straight below the hip), measure_legs


def measure_legs(ob):
    """Each leg's own axis, from the mesh: the middle of what is round the shin
    in two bands (the cape behind left out), drawn up to the hip's height. The
    bones go straight down from that hip, and the leg is turned onto them, so
    its plates sit round the bone they turn about."""
    for side, sx in (("L", -1), ("R", 1)):
        def middle(z, half):
            ps = [v.co for v in ob.data.vertices
                  if v.co.x * sx > 0.02 and v.co.y > CAPE_Y * S and abs(v.co.z - z) < half]
            return sum(ps, Vector()) / len(ps)
        # two bands of the greave: the knee's cop and the foot stick out ahead
        knee, ankle = middle((K_KNEE - 0.08) * S, 0.025), middle((K_ANKLE + 0.07) * S, 0.025)
        knee.y = ankle.y = (knee.y + ankle.y) / 2   # upright seen from the side
        d = knee - ankle

        def at(z):
            return ankle + d * ((z - ankle.z) / d.z)
        made = [at(K_HIP[1] * S), at(K_KNEE * S), at(K_ANKLE * S)]
        rest = [made[0].copy()]
        for i in range(2):
            rest.append(rest[-1] - Vector((0, 0, (made[i + 1] - made[i]).length)))
        LEGS[side] = (made, rest)
        print(f"leg {side}: hip {tuple(round(c, 3) for c in made[0])}, ankle {tuple(round(c, 3) for c in made[2])}")


def segment(p, a, b):
    d = b - a
    s = max(0.0, min(1.0, (p - a).dot(d) / d.length_squared))
    return (p - (a + d * s)).length


def load():
    """The knight in the rig's world, faced to +Y, each face's colour read off
    the texture (corners and middle), the texture then dropped."""
    bpy.ops.import_scene.gltf(filepath=SRC)
    ob = next(o for o in bpy.context.selected_objects if o.type == "MESH")
    me = ob.data
    img = next(n.image for n in me.materials[0].node_tree.nodes if n.type == "TEX_IMAGE")
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]   # sRGB as stored
    uv = me.uv_layers.active.data
    cols = []
    for f in me.polygons:
        pts = [uv[i].uv for i in f.loop_indices]
        pts.append(sum(pts, Vector((0, 0))) / len(pts))
        cols.append(np.mean([px[min(h - 1, int(p.y * h)), min(w - 1, int(p.x * w))] for p in pts], axis=0))
    mw = ob.matrix_world.copy()
    co = [mw @ v.co for v in me.vertices]
    lo = Vector([min(c[i] for c in co) for i in range(3)])
    hi = Vector([max(c[i] for c in co) for i in range(3)])
    scale = K_TOP / (hi.z - lo.z)   # its own size, to the height K_* were read at
    mid = (lo + hi) / 2
    for v, c in zip(me.vertices, co):
        p = Vector(((c.x - mid.x) * scale, (c.y - mid.y) * scale, (c.z - lo.z) * scale))
        v.co = Vector((-p.x * S, -p.y * S, p.z * S))
    ob.matrix_world.identity()
    me.materials.clear()
    while me.uv_layers:
        me.uv_layers.remove(me.uv_layers[0])
    bm = bmesh.new()   # split at the texture's seams: made whole, its pieces stay apart
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.to_mesh(me)
    bm.free()
    assert len(me.polygons) == len(cols)
    # Fewer triangles, or with the clips the file is over a megabyte: each
    # face's colour goes to the vertices it keeps, read back after.
    layer = me.color_attributes.new("c", "FLOAT_COLOR", "CORNER")
    for f, c in zip(me.polygons, cols):
        for li in f.loop_indices:
            layer.data[li].color = (*c, 1.0)
    have = len(me.polygons)
    if have > TRIS:
        mod = ob.modifiers.new("dec", "DECIMATE")
        mod.ratio = TRIS / have
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.modifier_apply(modifier=mod.name)
    layer = me.color_attributes["c"]
    cols = [np.mean([layer.data[li].color[:3] for li in f.loop_indices], axis=0) for f in me.polygons]
    me.color_attributes.remove(layer)
    return ob, np.array(cols)


def pieces(me):
    """Which piece each vertex is: Meshy's parts are not joined to each other."""
    near = [[] for _ in me.vertices]
    for e in me.edges:
        a, b = e.vertices
        near[a].append(b)
        near[b].append(a)
    piece = [-1] * len(me.vertices)
    n = 0
    for start in range(len(me.vertices)):
        if piece[start] >= 0:
            continue
        stack = [start]
        piece[start] = n
        while stack:
            for j in near[stack.pop()]:
                if piece[j] < 0:
                    piece[j] = n
                    stack.append(j)
        n += 1
    return piece, n


def part(p):
    side, sx = ("L", -1) if p.x < 0 else ("R", 1)
    a, _ = arm_pose(sx)
    d = a[3] - a[0]
    t = (p - a[0]).dot(d) / d.length_squared
    radial = ((p - a[0]) - d * t).length
    # past the pauldrons nothing is torso: a claw splayed off the axis is still the hand's
    if (t > 0.15 and radial < 0.12 * S) or (t > 0.8 and radial < 0.18 * S) or abs(p.x) > 0.36 * S:
        return "arm_" + side
    if p.z < K_HIP[1] * S + 0.02 and p.y > CAPE_Y * S:
        return "leg_" + side
    return "torso"


def colours(me, cols, piece):
    """A flat material for each face: the nearest of COLOURS, then within each
    piece the commonest among its neighbours where it is a speck, and a piece
    that is mostly one colour all of it."""
    names = [k for k, _ in COLOURS]
    ref = np.array([c for _, c in COLOURS])
    dist = ((cols[:, None, :] - ref[None]) ** 2).sum(-1)
    skin = [i for i, (n, _) in enumerate(COLOURS) if n == "Skin"]
    for f in me.polygons:   # skin is the face under the helmet's brim, nowhere else
        if not (f.center.z > (K_NECK - 0.05) * S and f.center.y > 0.0):
            dist[f.index, skin] = np.inf
    label = list(np.argmin(dist, axis=1))
    vface = [[] for _ in me.vertices]
    for f in me.polygons:
        for v in f.vertices:
            vface[v].append(f.index)
    for _ in range(2):
        new = []
        for f in me.polygons:
            count = {}
            for v in f.vertices:
                for g in vface[v]:
                    count[label[g]] = count.get(label[g], 0) + 1
            best = max(count, key=count.get)
            new.append(best if count[best] > 2 * count.get(label[f.index], 0) else label[f.index])
        label = new
    of = {}
    for f in me.polygons:
        of.setdefault(piece[f.vertices[0]], []).append(f.index)
    for faces in of.values():
        count = {}
        for i in faces:
            count[label[i]] = count.get(label[i], 0) + 1
        best = max(count, key=count.get)
        if count[best] >= ONE_COLOUR * len(faces):
            for i in faces:
                label[i] = best
    used = sorted(set(label))
    for i in used:
        name = COLOURS[i][0]
        rgb = next(c for n, c in COLOURS if n == name)   # a name's first colour is its colour
        # the robot's file came with a tint_1 and a tint_2 of its own: ours replace them
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        m.use_nodes = True
        bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
        lin = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in rgb]
        bsdf.inputs["Base Color"].default_value = (*lin, 1.0)
        bsdf.inputs["Metallic"].default_value = 0.0
        bsdf.inputs["Roughness"].default_value = 0.6
        m.diffuse_color = (*lin, 1.0)
    order = list(dict.fromkeys(names[i] for i in used))
    for n in order:
        me.materials.append(bpy.data.materials[n])
    for f, lab in zip(me.polygons, label):
        f.material_index = order.index(names[lab])


def body(rig, ob, cols):
    """The knight, its arms turned down and legs straightened, bound to the
    moved rig: a small piece follows one bone whole, as the robot's parts do; a
    big one (the torso, the cape) bends, its vertices each the nearest bone of
    their part, blurred."""
    me = ob.data
    piece, count = pieces(me)
    colours(me, cols, piece)
    mw = rig.matrix_world
    seg = {b.name: (mw @ b.head_local, mw @ b.tail_local) for b in rig.data.bones}
    # The arms' bones where the A-pose has them, and each one's move to rest.
    moves, straight = {}, {}   # to the rest arm, and to one hanging straight down
    for side, sx in (("L", -1), ("R", 1)):
        a, r = arm_pose(sx)
        _, r0 = arm_pose(sx, 0)
        for i, n in enumerate(BONES["arm_" + side]):
            seg[n] = (a[i], a[i + 1])
            moves[n] = (a[i], r[i], (a[i + 1] - a[i]).rotation_difference(r[i + 1] - r[i]))
            straight[n] = (a[i], r0[i], (a[i + 1] - a[i]).rotation_difference(r0[i + 1] - r0[i]))
        made, rest = LEGS[side]
        for i, n in enumerate(BONES["leg_" + side]):   # the foot follows the shin's ankle
            j = min(i, 1)
            turn = (made[j + 1] - made[j]).rotation_difference(rest[j + 1] - rest[j])
            seg[n] = (made[i], made[i + 1]) if i < 2 else (made[2], made[2] + Vector((0, 0.15, -0.05)))
            moves[n] = straight[n] = (made[i], rest[i], turn)
    where = [part(v.co) for v in me.vertices]
    nearest = [min(BONES[w], key=lambda b: segment(v.co, *seg[b])) for v, w in zip(me.vertices, where)]
    members = [[] for _ in range(count)]
    for i, p in enumerate(piece):
        members[p].append(i)
    rigid = set()
    for p, vs in enumerate(members):
        tally = {}
        for i in vs:
            tally[nearest[i]] = tally.get(nearest[i], 0) + 1
        best = max(tally, key=tally.get)
        # a piece that runs across a joint (Meshy made bracer and gauntlet one)
        # bends there instead
        bones = {nearest[i] for i in vs}
        across = len(bones) > 1 and sorted(tally.values())[-2] >= ACROSS * len(vs)
        if not across and (len(vs) <= RIGID_MAX or tally[best] >= 0.85 * len(vs)):
            # the bone nearest the piece's middle, among its part's: a bracer
            # reaching the wrist is the forearm's, though its cuff is near the hand
            mid = sum((me.vertices[i].co for i in vs), Vector()) / len(vs)
            region = max({where[i] for i in vs}, key=lambda r: sum(where[i] == r for i in vs))
            best = min(BONES[region], key=lambda b: segment(mid, *seg[b]))
            rigid.add(p)
            for i in vs:
                nearest[i] = best
    # The helmet is the head's whole: a body drawn headless (driving, seen
    # from its own eye) hides the head bone and what hangs on it, nothing else.
    neck = K_NECK * S
    for p, vs in enumerate(members):
        mid = sum((me.vertices[i].co for i in vs), Vector()) / len(vs)
        if mid.z > neck and abs(mid.x) < 0.17 * S:
            rigid.add(p)
            for i in vs:
                nearest[i] = "head"
    weights = [{b: 1.0} for b in nearest]
    blur = blurred(me, weights, passes=4)   # edges stay inside a piece
    weights = [w if piece[i] in rigid else blur[i] for i, w in enumerate(weights)]
    for i, v in enumerate(me.vertices):   # and a big piece's top above the collar
        if piece[i] not in rigid and where[i] == "torso" and v.co.z > neck + 0.04:
            weights[i] = {"head": 1.0}
    def moved(co, w, moves):
        p = Vector()
        for bone, x in w.items():
            if bone in moves:
                a, r, turn = moves[bone]
                p += (r + turn @ (co - a)) * x
            else:
                p += co * x
        return p
    hanging = [fist(moved(v.co, w, straight), w, 0) for v, w in zip(me.vertices, weights)]
    for v, w in zip(me.vertices, weights):
        v.co = fist(moved(v.co, w, moves), w)
    for f in me.polygons:
        f.use_smooth = True
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    for i, w in enumerate(weights):
        for bone, x in w.items():
            ob.vertex_groups[bone].add([i], x, "REPLACE")
    ob.name = NAME
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    bpy.context.view_layer.objects.active = ob
    try:   # sharp where a piece folds, smooth across its curves
        bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40))
    except Exception as e:
        print("smooth by angle:", e)
    return ob, weights, hanging


def sleeves(rig):
    """The dark sleeve down each rest arm's upper arm and forearm: what shows
    between the plates now that they sit on the robot's arm lengths."""
    co, faces, groups = [], [], []
    for side, sx in (("L", -1), ("R", 1)):
        _, r = arm_pose(sx)
        for bone, a, b, radius in ((f"upper_arm_{side}", r[0], r[1], SLEEVE[0] * 1.05),
                                   (f"forearm_{side}", r[1], r[2], SLEEVE[1] * 1.05)):
            tube(co, faces, a, b, radius)
            groups += [bone] * 16
    me = bpy.data.meshes.new(NAME + "_sleeves")
    me.from_pydata(co, [], faces)
    me.materials.append(bpy.data.materials["Dark"])
    for f in me.polygons:
        f.use_smooth = True
    ob = bpy.data.objects.new(NAME + "_sleeves", me)
    bpy.context.scene.collection.objects.link(ob)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    for i, g in enumerate(groups):
        ob.vertex_groups[g].add([i], 1.0, "REPLACE")
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob


def tube(co, faces, a, b, radius):
    """A closed eight-sided tube from a to b, into co and faces."""
    axis = (b - a).normalized()
    u = axis.orthogonal().normalized()
    v = axis.cross(u)
    start = len(co)
    for end in (a, b):
        for i in range(8):
            t = 2 * math.pi * i / 8
            co.append(end + (u * math.cos(t) + v * math.sin(t)) * radius)
    for i in range(8):
        j = (i + 1) % 8
        faces.append([start + i, start + j, start + 8 + j, start + 8 + i])
    faces.append([start + i for i in range(8)][::-1])
    faces.append([start + 8 + i for i in range(8)])


def fist(p, w, out=None):
    """The gauntlet closed: past the knuckles the fingers fold toward the
    palm (the body's side), at the knuckles and again halfway down, FIST
    degrees each, as much as the vertex is the hand's."""
    hand = w.get("hand_L", 0.0) + w.get("hand_R", 0.0)
    if not hand:
        return p
    sx = -1 if w.get("hand_L", 0.0) else 1
    _, r = arm_pose(sx, out)
    wrist, along = r[2], (r[3] - r[2]).normalized()
    inward = Vector((-sx, 0, 0))
    inward = (inward - along * inward.dot(along)).normalized()
    axis = along.cross(inward).normalized()
    q = p.copy()
    for at in KNUCKLES:   # each fold about its joint, the next joint folded with it
        pivot = wrist + along * at
        if (q - pivot).dot(along) > 0:
            turn = Quaternion(axis, math.radians(FIST))
            q = pivot + turn @ (q - pivot)
            along, inward = turn @ along, turn @ inward
            wrist = pivot - along * at
    return p.lerp(q, min(1.0, hand))


def arm_part(ob, weights, hanging):
    """The arms of the knight at rest, as plain data: they outlive the scene."""
    me = ob.data
    arm = {"upper_arm_L", "forearm_L", "hand_L", "upper_arm_R", "forearm_R", "hand_R"}
    keep = {v.index for v in me.vertices if max(weights[v.index], key=weights[v.index].get) in arm}
    # nothing above the shoulder: a pauldron's spikes would stand by the eye
    top = arm_pose(1, 0)[1][0].z - 0.03
    faces = [f for f in me.polygons if all(v in keep and hanging[v].z < top for v in f.vertices)]
    used = sorted({v for f in faces for v in f.vertices})
    index = {v: i for i, v in enumerate(used)}
    return {
        "co": [hanging[v] for v in used],
        "w": [weights[v] for v in used],
        "faces": [[index[v] for v in f.vertices] for f in faces],
        "mat": [me.materials[f.material_index].name for f in faces],
        "colours": {m.name: tuple(next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
                                  .inputs["Base Color"].default_value) for m in me.materials},
    }


def arms(rig, data, robot):
    """The arms seen in first person, on robot_arms.gltf's rig as it is: the
    knight's arm hanging straight, carried whole so its wrist is the robot's
    (the shoulder falls behind the eye, out of view) and slimmed (FP_SLIM), so
    the hands are where the robot's are and hold what it holds there without
    the bracers filling the view."""
    mw = rig.matrix_world
    fp = {b.name: mw @ b.head_local for b in rig.data.bones}
    co = []
    for c, w in zip(data["co"], data["w"]):
        p = Vector()
        for bone, x in w.items():
            if bone not in fp or bone not in robot:
                side = "L" if c.x < 0 else "R"   # a shoulder's chest share
                p += (c + fp["upper_arm_" + side] - robot["upper_arm_" + side][0]) * x
                continue
            anchor, at = robot[bone]
            local = c - anchor
            local.x *= FP_SLIM
            local.y *= FP_SLIM
            p += (fp[at] + local) * x
        co.append(p)
    # Meshy's bracers are open shells: seen from the elbow, as the eye sees
    # them, a ring with nothing in it. A closed dark sleeve down each of the
    # robot's arm bones fills them, as the robot's own arm is one tube.
    faces, mats, weights = list(data["faces"]), list(data["mat"]), list(data["w"])
    for side in "LR":
        hand = fp[f"hand_{side}"]
        for bone, a, b, radius in ((f"upper_arm_{side}", fp[f"upper_arm_{side}"], fp[f"forearm_{side}"], SLEEVE[0]),
                                   (f"forearm_{side}", fp[f"forearm_{side}"], hand, SLEEVE[1]),
                                   # and a plug in the gauntlet's open cuff
                                   (f"hand_{side}", hand + Vector((0, 0, 0.01)), hand - Vector((0, 0, 0.07)), SLEEVE[2])):
            n = len(faces)
            tube(co, faces, a, b, radius)
            weights += [{bone: 1.0}] * 16
            mats += ["Dark"] * (len(faces) - n)
    data = dict(data, faces=faces, mat=mats, w=weights)
    me = bpy.data.meshes.new(NAME + "_arms")
    me.from_pydata(co, [], data["faces"])
    order = list(dict.fromkeys(data["mat"]))
    for n in order:
        m = bpy.data.materials.get(n) or bpy.data.materials.new(n)
        m.use_nodes = True
        bsdf = next(x for x in m.node_tree.nodes if x.type == "BSDF_PRINCIPLED")
        bsdf.inputs["Base Color"].default_value = data["colours"][n]
        bsdf.inputs["Metallic"].default_value = 0.0
        bsdf.inputs["Roughness"].default_value = 0.6
        me.materials.append(m)
    for f, n in zip(me.polygons, data["mat"]):
        f.material_index = order.index(n)
        f.use_smooth = True
    ob = bpy.data.objects.new(NAME + "_arms", me)
    bpy.context.scene.collection.objects.link(ob)
    bones = {b.name for b in rig.data.bones}
    for b in bones:
        ob.vertex_groups.new(name=b)
    for i, w in enumerate(data["w"]):   # a shoulder's chest weight has no bone here
        w = {k: x for k, x in w.items() if k in bones} or {"fp_root": 1.0}
        total = sum(w.values())
        for k, x in w.items():
            ob.vertex_groups[k].add([i], x / total, "REPLACE")
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    bpy.context.view_layer.objects.active = ob
    try:
        bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40))
    except Exception as e:
        print("smooth by angle:", e)
    return ob


def floats(doc, bins, ai):
    acc = doc["accessors"][ai]
    view = doc["bufferViews"][acc["bufferView"]]
    n = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}[acc["type"]]
    off = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    return [list(struct.unpack_from(f"<{n}f", bins, off + 4 * n * i)) for i in range(acc["count"])]


def graft(path, robot_path, hips_scale):
    """The robot's clips, written into the knight's file. Each translation key
    is moved by what its bone moved at rest, the hips' own move (a crouch, a
    jump) scaled to the knight's legs. A channel that never changes keeps one
    key: dropped, it would no longer say its clip moves that bone (pose() in
    src/core/media/gltf.cpp lays a clip over another only where it does)."""
    with open(robot_path) as f:
        robot = json.load(f)
    with open(os.path.join(os.path.dirname(robot_path), robot["buffers"][0]["uri"]), "rb") as f:
        rbin = f.read()
    rest_robot = {n["name"]: n.get("translation", [0, 0, 0]) for n in robot["nodes"] if "name" in n}
    with open(path) as f:
        doc = json.load(f)
    binp = os.path.join(os.path.dirname(path), doc["buffers"][0]["uri"])
    with open(binp, "rb") as f:
        bins = bytearray(f.read())
    nodes = {n.get("name"): i for i, n in enumerate(doc["nodes"])}
    written = {}   # the same keys once

    def put(keys, width, times):
        data = b"".join(struct.pack(f"<{width}f", *key) for key in keys)
        if data in written:
            return written[data]
        while len(bins) % 4:
            bins.append(0)
        doc["bufferViews"].append({"buffer": 0, "byteOffset": len(bins), "byteLength": len(data)})
        bins.extend(data)
        acc = {"bufferView": len(doc["bufferViews"]) - 1, "componentType": 5126, "count": len(keys),
               "type": {1: "SCALAR", 3: "VEC3", 4: "VEC4"}[width]}
        if times:   # an animation's input says its range
            acc["min"], acc["max"] = [min(k[0] for k in keys)], [max(k[0] for k in keys)]
        doc["accessors"].append(acc)
        written[data] = len(doc["accessors"]) - 1
        return written[data]

    doc["animations"] = []
    for anim in robot["animations"]:
        out = {"name": anim["name"], "channels": [], "samplers": []}
        for ch in anim["channels"]:
            name = robot["nodes"][ch["target"]["node"]].get("name")
            if name not in nodes:
                continue
            smp = anim["samplers"][ch["sampler"]]
            times = floats(robot, rbin, smp["input"])
            keys = floats(robot, rbin, smp["output"])
            what = ch["target"]["path"]
            if what == "translation":
                mine, theirs = doc["nodes"][nodes[name]].get("translation", [0, 0, 0]), rest_robot[name]
                scale = hips_scale if name == "hips" else 1.0
                keys = [[m + (v - t) * scale for v, m, t in zip(key, mine, theirs)] for key in keys]
            if all(max(abs(x - y) for x, y in zip(key, keys[0])) < 1e-6 for key in keys):
                times, keys = times[:1], keys[:1]
            out["samplers"].append({"input": put(times, 1, True), "output": put(keys, len(keys[0]), False),
                                    "interpolation": smp.get("interpolation", "LINEAR")})
            out["channels"].append({"sampler": len(out["samplers"]) - 1,
                                    "target": {"node": nodes[name], "path": what}})
        doc["animations"].append(out)
    doc["buffers"][0]["byteLength"] = len(bins)
    with open(binp, "wb") as f:
        f.write(bins)
    with open(path, "w") as f:
        json.dump(doc, f, separators=(",", ":"))


def tools_moved(mesh, by):
    for v in mesh.data.vertices:   # the tools, with the hand that holds them
        v.co += mesh.matrix_world.inverted().to_3x3() @ by


def finish(path, robot_path, hips_scale):
    graft(path, robot_path, hips_scale)
    for f in (path, path[:-5] + ".bin"):
        size = os.path.getsize(f)
        print(f"{os.path.basename(f)}: {size} bytes")
        assert size < 1024 * 1024 - 4096, f"{f}: over the 1 MB a server serves"


def main():
    robot_path = os.path.join(OUT, "robot.gltf")
    rig, mesh = fresh(robot_path)
    keep_tools(mesh)
    ob, cols = load()
    measure_legs(ob)
    placed = place_rig(rig)
    tools_moved(mesh, placed["hand_R"][1])
    mesh.name = NAME + "_tools"
    ob, weights, hanging = body(rig, ob, cols)
    data = arm_part(ob, weights, hanging)
    path = os.path.join(OUT, NAME + ".gltf")
    export(rig, [ob, sleeves(rig), mesh], path, False)
    finish(path, robot_path, K_HIP[1] * S / 0.73)

    # Where each arm bone's pieces hang from on the straight knight, and the
    # robot's bone that point goes to: the wrist, for all of them.
    knight = {}
    for side, sx in (("L", -1), ("R", 1)):
        _, r0 = arm_pose(sx, 0)
        up, fore, hand = BONES["arm_" + side]
        # one move for the whole arm: pieces of a bracer taken by different
        # bones would otherwise come apart
        knight[up] = knight[fore] = knight[hand] = (r0[2], hand)

    arms_path = os.path.join(OUT, "robot_arms.gltf")
    rig, mesh = fresh(arms_path)
    keep_tools(mesh)
    path = os.path.join(OUT, NAME + "_arms.gltf")
    export(rig, [arms(rig, data, knight), mesh], path, False)
    finish(path, arms_path, 1.0)


if __name__ == "__main__":
    main()
