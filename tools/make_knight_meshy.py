# The dragon knight, a player body from Meshy (assets/meshy/knight: concept.png,
# and knight.gltf as tools/meshy_prop.py left it), on a skeleton of its own
# proportions with the robot's clips:
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
# The knight stands in an A-pose and the robot's arms hang straight down, so
# the arms are first turned down about their joints: each vertex takes the
# nearest bone of its own part (an arm's never the cape's), blurred at the
# joints, and is moved by those bones from the A-pose to the rest pose. The
# texture stays as Meshy painted it: no tint, the player's colours do not dye
# this one.
#
# Writes mods/robot/robot_knight.gltf + .bin + .png, and robot_knight_arms
# (the arms seen in first person, on robot_arms.gltf's rig and clips, their
# shoulders where the robot's are in view) sharing that texture.
import json
import os
import struct
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_species import blurred, export, fresh, keep_tools  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "robot")
SRC = os.path.join(ROOT, "assets", "meshy", "knight", "knight.gltf")
NAME = "robot_knight"

S = 1.15   # the knight's file is 1.36 tall: 1.56 with its horns, the robot's head
# Its joints, read off front and side renders of that file (x either side,
# depth 0): shoulder, elbow, wrist and fingertips of the A-pose arm, the hip
# joint, knee and ankle, the neck and the top of the horns.
K_SHOULDER, K_ELBOW, K_WRIST, K_TIP = (0.20, 0.95), (0.33, 0.77), (0.47, 0.67), (0.60, 0.53)
K_HIP, K_KNEE, K_ANKLE = (0.085, 0.40), 0.24, 0.07
K_NECK, K_TOP = 1.02, 1.36
HAND_SQUEEZE = 0.6   # how much of the claws' sideways fan is closed
CAPE_Y = -0.09   # behind this (scaled, facing +Y) a point under the hips is cape, not leg

BONES = {
    "torso": ["hips", "spine", "chest", "neck", "head"],
    "arm_L": ["upper_arm_L", "forearm_L", "hand_L"], "arm_R": ["upper_arm_R", "forearm_R", "hand_R"],
    "leg_L": ["thigh_L", "shin_L", "foot_L"], "leg_R": ["thigh_R", "shin_R", "foot_R"],
}


def k(x, z, side=1):
    """A knight joint in the rig's world."""
    return Vector((side * x * S, 0.0, z * S))


def arm_pose(sx):
    """The A-pose arm's joints and the rest arm's, shoulder to fingertip."""
    a = [k(*j, sx) for j in (K_SHOULDER, K_ELBOW, K_WRIST, K_TIP)]
    r = [a[0].copy()]
    for i in range(3):
        r.append(r[-1] - Vector((0, 0, (a[i + 1] - a[i]).length)))
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


def load():
    """The knight in the rig's world, faced to +Y."""
    bpy.ops.import_scene.gltf(filepath=SRC)
    ob = next(o for o in bpy.context.selected_objects if o.type == "MESH")
    mw = ob.matrix_world.copy()
    for v in ob.data.vertices:
        p = mw @ v.co
        v.co = Vector((-p.x * S, -p.y * S, p.z * S))
    ob.matrix_world.identity()
    # The file is flat-shaded, every corner its own vertex: welded and smooth,
    # or with its weights it is over a megabyte (UVs live on the corners).
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.to_mesh(ob.data)
    bm.free()
    for f in ob.data.polygons:
        f.use_smooth = True
    img = next(n.image for n in ob.data.materials[0].node_tree.nodes if n.type == "TEX_IMAGE")
    img.name = NAME
    ob.data.materials[0].name = NAME
    return ob


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


def body(rig):
    """The knight, its arms turned down, bound to the moved rig."""
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
    return ob, where, weights


def arm_part(ob, where, weights):
    """The arms of the knight at rest, as plain data: they outlive the scene."""
    me = ob.data
    keep = {v.index for v in me.vertices if where[v.index].startswith("arm")}
    faces = [f for f in me.polygons if all(v in keep for v in f.vertices)]
    used = sorted({v for f in faces for v in f.vertices})
    index = {v: i for i, v in enumerate(used)}
    uv = me.uv_layers.active.data
    return {
        "co": [me.vertices[v].co.copy() for v in used],
        "w": [weights[v] for v in used],
        "faces": [[index[v] for v in f.vertices] for f in faces],
        "uv": [[tuple(uv[i].uv) for i in f.loop_indices] for f in faces],
    }


def arms(rig, data, shift):
    """The arms seen in first person, on robot_arms.gltf's rig: the knight's,
    moved by `shift` so its shoulders are where the robot's are in view."""
    me = bpy.data.meshes.new(NAME + "_arms")
    me.from_pydata([c + shift for c in data["co"]], [], data["faces"])
    layer = me.uv_layers.new(name="UVMap")
    for f, uvs in zip(me.polygons, data["uv"]):
        for li, uv in zip(f.loop_indices, uvs):
            layer.data[li].uv = uv
        f.use_smooth = True
    mat = bpy.data.materials.new(NAME)
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(os.path.join(OUT, NAME + ".png"))
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    me.materials.append(mat)
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

    for im in doc.get("images", []):   # named after the file it came from
        im["uri"] = NAME + ".png"
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
    placed = place_rig(rig)
    tools_moved(mesh, placed["hand_R"][1])
    mesh.name = NAME + "_tools"
    ob, where, weights = body(rig)
    data = arm_part(ob, where, weights)
    path = os.path.join(OUT, NAME + ".gltf")
    export(rig, [ob, mesh], path, False)
    for f in os.listdir(OUT):   # the exporter names the texture after the file it came from
        if f == "knight.png":
            os.replace(os.path.join(OUT, f), path[:-5] + ".png")
    finish(path, robot_path, K_HIP[1] * S / 0.73)

    arms_path = os.path.join(OUT, "robot_arms.gltf")
    rig, mesh = fresh(arms_path)
    keep_tools(mesh)
    robot_shoulder = rig.matrix_world @ rig.data.bones["upper_arm_R"].head_local
    shift = Vector((0, robot_shoulder.y, robot_shoulder.z)) - Vector((0, 0, placed["upper_arm_R"][0][0].z))
    moved = move(rig, {n: (placed[n][0][0] + shift, placed[n][0][1] + shift)
                       for n in BONES["arm_L"] + BONES["arm_R"]})
    tools_moved(mesh, moved["hand_R"][1])
    path = os.path.join(OUT, NAME + "_arms.gltf")
    export(rig, [arms(rig, data, shift), mesh], path, False)
    for f in os.listdir(OUT):
        if f in ("knight.png", "robot_knight_1.png"):
            os.remove(os.path.join(OUT, f))
    finish(path, arms_path, 1.0)
    print(f"{NAME}.png: {os.path.getsize(os.path.join(OUT, NAME + '.png'))} bytes")


if __name__ == "__main__":
    main()
