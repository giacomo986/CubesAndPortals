# The mole-man, a player body from Meshy (assets/meshy/mole: concept.png and
# the image-to-3D model mole_parts.glb -- smart topology, T-pose, 12403
# triangles), on a skeleton of ITS OWN proportions with the robot's clips:
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_mole_meshy.py"
#
# A mole is short in the leg and big in the head, and stretched onto the
# robot's joints it is no mole (tried 2026-11-07 and 2026-11-12). So the
# skeleton is bent onto the mole instead: the robot's bones, same names, same
# orientations, moved to where the mole's joints are (K_*). A clip turns
# bones, so it plays on any lengths; the translations it also carries are each
# moved by what its bone moved at rest (the hips' by how much shorter the legs
# are). The clips are written into the file here (graft), a constant channel
# as one key, which keeps it under the 1 MB a media file may weigh. What it
# costs: hands and feet are not where the robot's are -- a kart's wheel, a
# ladder's rung -- by what the limbs are shorter.
#
# The mole stands in a T-pose and the robot's arms hang straight down, so the
# arms are first turned down about their joints: each vertex takes the nearest
# bone of its own part, blurred at the joints, and is moved by those bones from
# the T-pose to the rest pose.
#
# No texture is kept (a third try, 2026-11-12, baked Meshy's to 512 px on a
# 4200-triangle body: ragged borders, too dark, and nothing a player could
# dye). Each face takes one flat material, read off Meshy's texture inside
# the face and by where the face is (label): the dungarees tint_1, the shirt
# tint_2 and the pocket's patch tint_3, which the player dyes, and Fur, Pink,
# Claw and Eye_Dark, tools/make_species.py's.
#
# Writes mods/robot/robot_mole.gltf + .bin and robot_mole_arms (the arms seen
# in first person, on robot_arms.gltf's rig and clips, their shoulders where
# the robot's are in view); to the directory MOLE_OUT names instead, if set.
import json
import math
import os
import struct
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_species import MOLE_TINTS, blurred, export, fresh, keep_tools, materials  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ROBOT = os.path.join(ROOT, "mods", "robot")
OUT = os.environ.get("MOLE_OUT", ROBOT)
SRC = os.path.join(ROOT, "assets", "meshy", "mole", "mole_parts.glb")
NAME = "robot_mole"

S = 1.5   # the file is 1 tall: the mole stands 1.5, the robot's eyes
# Its joints, read off front and side renders of that file (x either side,
# depth 0): shoulder, elbow, wrist and claw tips of the T-pose arm, the hip
# joint, knee and ankle, the neck and the top of the head.
K_SHOULDER, K_ELBOW, K_WRIST, K_TIP = (0.146, 0.692), (0.314, 0.692), (0.466, 0.692), (0.692, 0.692)
K_HIP, K_KNEE, K_ANKLE = (0.093, 0.34), 0.20, 0.07
K_NECK, K_TOP = 0.766, 1.0
TRIS = int(os.environ.get("MOLE_TRIS", 7000))   # what the body is decimated to
EYE = (0.052, 0.1, 0.885, 0.017)   # an eye: x either side, how far ahead, height, radius (K units)
FACET = 22     # degrees: a fold sharper than this is an edge
ARM_OUT = 16   # degrees the rest arm hangs out
HAND_SQUEEZE = 0.0   # the claws lie along the arm: hanging, they point down
ARM_X, ARM_Z = 0.155, 0.60   # past this to the side and over this height it is arm: the T-pose

BONES = {
    "torso": ["hips", "spine", "chest", "neck", "head"],
    "arm_L": ["upper_arm_L", "forearm_L", "hand_L"], "arm_R": ["upper_arm_R", "forearm_R", "hand_R"],
    "leg_L": ["thigh_L", "shin_L", "foot_L"], "leg_R": ["thigh_R", "shin_R", "foot_R"],
}


def k(x, z, side=1):
    """A mole joint in the rig's world."""
    return Vector((side * x * S, 0.0, z * S))


def arm_pose(sx):
    """The T-pose arm's joints and the rest arm's, shoulder to claw tip."""
    a = [k(*j, sx) for j in (K_SHOULDER, K_ELBOW, K_WRIST, K_TIP)]
    # The rest arm hangs ARM_OUT degrees off straight down, clear of a belly
    # wider than the shoulders; the bones still point straight down, which
    # leaves every clip's arm that much out.
    down = Vector((sx * math.sin(math.radians(ARM_OUT)), 0.0, -math.cos(math.radians(ARM_OUT))))
    r = [a[0].copy()]
    for i in range(3):
        r.append(r[-1] + down * (a[i + 1] - a[i]).length)
    return a, r


def lerp_z(z):
    """A torso height of the robot's, where the mole has it."""
    pts = [(0.73, K_HIP[1] * S), (1.234, K_SHOULDER[1] * S), (1.287, K_NECK * S), (1.565, K_TOP * S * 0.97)]
    for (r0, k0), (r1, k1) in zip(pts, pts[1:]):
        if z <= r1 or (r1, k1) == pts[-1]:
            return k0 + (z - r0) * (k1 - k0) / (r1 - r0)


def place_rig(rig):
    """The robot's bones moved onto the mole, each keeping its direction."""
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
        hip = k(K_HIP[0], K_HIP[1], sx)
        joints = [hip, Vector((hip.x, 0, K_KNEE * S)), Vector((hip.x, 0, K_ANKLE * S))]
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


def segment(p, a, b):
    d = b - a
    s = max(0.0, min(1.0, (p - a).dot(d) / d.length_squared))
    return (p - (a + d * s)).length


def label(c, n, rgb):
    """The material of a face at `c` looking along `n`, Meshy's colour there `rgb`."""
    r, g, b = (float(x) for x in rgb)
    v = max(r, g, b)
    sat = (v - min(r, g, b)) / max(v, 1e-6)
    x, z = abs(c.x) / S, c.z / S
    if r > 0.25 and r > 1.8 * g and r > 1.8 * b:
        # The pocket's red is on the bib; Meshy left patches of it on the seat
        # and the nape, which are the dungarees'.
        return "tint_3" if n.y > 0.3 and z > K_HIP[1] and x < 0.12 else "tint_1"
    if b > 1.12 * r and v > 0.08:
        return "tint_1"
    if v < 0.2 or (sat < 0.22 and v < 0.5):
        return "Fur"
    if r > 1.25 * g:
        return "Pink"
    if x > K_WRIST[0] - 0.02 or z < K_ANKLE + 0.03:
        return "Claw"
    return "tint_2" if z < K_NECK + 0.03 else "Pink"


def load():
    """The mole in the rig's world, faced to +Y, each face its flat material."""
    bpy.ops.import_scene.gltf(filepath=SRC)
    ob = next(o for o in bpy.context.selected_objects if o.type == "MESH")
    me = ob.data
    img = next(n.image for n in me.materials[0].node_tree.nodes if n.type == "TEX_IMAGE")
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]   # sRGB as stored
    mw = ob.matrix_world.copy()
    co = [mw @ v.co for v in me.vertices]
    lo = Vector([min(c[i] for c in co) for i in range(3)])
    hi = Vector([max(c[i] for c in co) for i in range(3)])
    scale = K_TOP / (hi.z - lo.z)   # its own size, to the height K_* were read at
    mid = (lo + hi) / 2
    for v, c in zip(me.vertices, co):
        v.co = Vector((-(c.x - mid.x) * scale * S, -(c.y - mid.y) * scale * S, (c.z - lo.z) * scale * S))
    ob.matrix_world.identity()
    bm = bmesh.new()   # split at the texture's seams: made whole
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    me.update()
    uv = me.uv_layers.active.data
    labels = []
    for f in me.polygons:
        pts = [uv[i].uv for i in f.loop_indices]
        m = sum(pts, Vector((0, 0))) / len(pts)
        pts = [m] + [(p + m) / 2 for p in pts]   # inside the face: its corners sit on the borders
        rgb = np.median([px[int(p.y % 1.0 * h) % h, int(p.x % 1.0 * w) % w] for p in pts], axis=0)
        labels.append(label(f.center, f.normal, rgb))
    # A speck takes the colour round it.
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
                    count[labels[g]] = count.get(labels[g], 0) + 1
            best = max(count, key=count.get)
            new.append(best if count[best] > 2 * count.get(labels[f.index], 0) else labels[f.index])
        labels = new
    order = list(dict.fromkeys(labels))
    me.materials.clear()
    for n in order:
        me.materials.append(bpy.data.materials[n])
    for f, n in zip(me.polygons, labels):
        f.material_index = order.index(n)
    while me.uv_layers:
        me.uv_layers.remove(me.uv_layers[0])
    print("MOLE faces", len(me.polygons), {n: labels.count(n) for n in order})
    if len(me.polygons) > TRIS:
        mod = ob.modifiers.new("dec", "DECIMATE")
        mod.ratio = TRIS / len(me.polygons)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return ob


def eyes(rig):
    """Two black beads: Meshy's are black paint on black fur."""
    x, y, z, r = EYE
    obs = []
    for sx in (-1, 1):
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=r * S, location=(sx * x * S, y * S, z * S))
        ob = bpy.context.object
        ob.data.materials.append(bpy.data.materials["Eye_Dark"])
        ob.vertex_groups.new(name="head").add(range(len(ob.data.vertices)), 1.0, "REPLACE")
        ob.parent = rig
        ob.matrix_parent_inverse = rig.matrix_world.inverted()
        ob.modifiers.new("rig", "ARMATURE").object = rig
        obs.append(ob)
    return obs


def part(p):
    side = "L" if p.x < 0 else "R"
    if abs(p.x) > ARM_X * S and p.z > ARM_Z * S:
        return "arm_" + side
    if p.z < K_HIP[1] * S + 0.02:
        return "leg_" + side
    return "torso"


def body(rig):
    """The mole, its arms turned down, bound to the moved rig."""
    ob = load()
    me = ob.data
    mw = rig.matrix_world
    seg = {b.name: (mw @ b.head_local, mw @ b.tail_local) for b in rig.data.bones}
    # The arms' bones where the A-pose has them, and each one's move to rest.
    moves = {}
    for side, sx in (("L", -1), ("R", 1)):
        a, r = arm_pose(sx)
        for i, n in enumerate(BONES["arm_" + side]):
            seg[n] = (a[i], a[i + 1])
            turn = (a[i + 1] - a[i]).rotation_difference(r[i + 1] - r[i])
            moves[n] = (a[i], r[i], turn)
    where = [part(v.co) for v in me.vertices]
    weights = [{min(BONES[w], key=lambda b: segment(v.co, *seg[b])): 1.0} for v, w in zip(me.vertices, where)]
    weights = blurred(me, weights, passes=4)
    for v, w in zip(me.vertices, weights):
        p = Vector()
        for bone, x in w.items():
            if bone in moves:
                a, r, turn = moves[bone]
                p += (r + turn @ (v.co - a)) * x
            else:
                p += v.co * x
        # The concept's open hands fan their claws out sideways: hanging,
        # they read as wings at the hips, so the fan is closed about the hand.
        hand = sum(x for bone, x in w.items() if bone.startswith("hand_"))
        if hand:
            r = moves["hand_L" if p.x < 0 else "hand_R"][1]
            p.x = r.x + (p.x - r.x) * (1.0 - HAND_SQUEEZE * hand)
        v.co = p
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    for i, w in enumerate(weights):
        for bone, x in w.items():
            ob.vertex_groups[bone].add([i], x, "REPLACE")
    ob.name = NAME
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    # The picture is of flat faces: an edge that folds stays sharp, and only
    # what curves gently is smoothed (all smooth, it was a mole of putty).
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(FACET))
    return ob, where, weights


def arm_part(ob, where, weights):
    """The arms of the mole at rest, as plain data: they outlive the scene."""
    me = ob.data
    keep = {v.index for v in me.vertices if where[v.index].startswith("arm")}
    faces = [f for f in me.polygons if all(v in keep for v in f.vertices)]
    used = sorted({v for f in faces for v in f.vertices})
    index = {v: i for i, v in enumerate(used)}
    return {
        "co": [me.vertices[v].co.copy() for v in used],
        "w": [weights[v] for v in used],
        "faces": [[index[v] for v in f.vertices] for f in faces],
        "mat": [me.materials[f.material_index].name for f in faces],
    }


def arms(rig, data, shift):
    """The arms seen in first person, on robot_arms.gltf's rig: the mole's,
    moved by `shift` so its shoulders are where the robot's are in view."""
    me = bpy.data.meshes.new(NAME + "_arms")
    me.from_pydata([c + shift for c in data["co"]], [], data["faces"])
    order = list(dict.fromkeys(data["mat"]))
    for n in order:
        me.materials.append(bpy.data.materials[n])
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
    ob.select_set(True)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(FACET))
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
    robot_path = os.path.join(ROBOT, "robot.gltf")
    rig, mesh = fresh(robot_path)
    materials(MOLE_TINTS)
    keep_tools(mesh)
    placed = place_rig(rig)
    for n in ("chest", "neck", "head"):   # what tools/make_species.py's WEARS and styles are moved by
        print(f"MOLE moved {n}: {tuple(round(c, 3) for c in placed[n][1])}, now at {tuple(round(c, 3) for c in placed[n][0][0])}")
    tools_moved(mesh, placed["hand_R"][1])
    mesh.name = NAME + "_tools"
    ob, where, weights = body(rig)
    data = arm_part(ob, where, weights)
    path = os.path.join(OUT, NAME + ".gltf")
    export(rig, [ob] + eyes(rig) + [mesh], path, False)
    finish(path, robot_path, K_HIP[1] * S / 0.73)

    arms_path = os.path.join(ROBOT, "robot_arms.gltf")
    rig, mesh = fresh(arms_path)
    materials(MOLE_TINTS)
    keep_tools(mesh)
    robot_shoulder = rig.matrix_world @ rig.data.bones["upper_arm_R"].head_local
    shift = Vector((0, robot_shoulder.y, robot_shoulder.z)) - Vector((0, 0, placed["upper_arm_R"][0][0].z))
    moved = move(rig, {n: (placed[n][0][0] + shift, placed[n][0][1] + shift)
                       for n in BONES["arm_L"] + BONES["arm_R"]})
    tools_moved(mesh, moved["hand_R"][1])
    path = os.path.join(OUT, NAME + "_arms.gltf")
    export(rig, [arms(rig, data, shift), mesh], path, False)
    finish(path, arms_path, 1.0)


if __name__ == "__main__":
    main()
