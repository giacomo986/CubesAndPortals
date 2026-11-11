# The lizard man, a player body from Meshy (meshy/lizard.glb, beside this
# file: image-to-3D in high detail with a texture, one mesh of 336792
# triangles), on the robot's own skeleton with the robot's clips. An add-on,
# not the game's: see README.md here. From the checkout's top:
#
#   flatpak run org.blender.Blender --background --python "$PWD/contrib/lizard/make_lizard.py"
#
# Meshy drew it as if of small cubes, but its mesh is not: no face is flat, the
# arms stand out in an A-pose and the pattern of its scales is only in the
# texture. So it is made of cubes here. Its parts -- the body with head and
# tail, each arm, each leg -- are cut apart where they join and brought onto
# the robot's joints: the body grown about the hips until the shoulders are at
# the robot's height, each arm turned down from the A-pose about its shoulder
# and stretched to the robot's upper arm and forearm, each leg set under the
# robot's hip and stretched to its thigh and shin. Then, at rest, where every
# limb is upright, the whole is filled with cubes CELL across: a cube where its
# middle is inside a part, of the colour the texture has nearest it, following
# the bone its height puts it on. Each bone's cubes are a closed shell of their
# own -- a bent elbow shows a face, not a hole -- and faces of one colour side
# by side are one face.
#
# The colours are flat materials: the skin tint_1, the belly tint_2 and the
# harness tint_3, which the player dyes; the dark scales and the eyes stay.
#
# Writes robot_lizard.gltf + .bin and robot_lizard_arms (the arms seen in
# first person, on robot_arms.gltf's rig and clips) beside this file, and four
# views of it at rest to build/look/lizard_*.png. It has no styles, and wears
# the robot's extras where the robot does.
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
sys.path.insert(0, os.path.join(ROOT, "tools"))   # the game's own tools, for the rig and the export
from make_robot_extras import material  # noqa: E402
from make_species import ARM_BONES, TO_ARMS, export, fresh, keep_tools  # noqa: E402

GAME = os.path.join(ROOT, "mods", "robot")   # the robot it is made on
OUT = HERE
SRC = os.path.join(HERE, "meshy", "lizard.glb")
LOOK = os.path.join(ROOT, "build", "look")
NAME = "robot_lizard"

CELL = 0.04   # metres a cube

# The lizard's own joints, read off front and side renders of the file, in
# shares of its height: x either side of its middle, z up from its soles. Its
# body's middle is BODY_Y ahead of the file's (the snout and the tail reach
# as far each way).
L_SHOULDER, L_WRIST = (0.134, 0.723), (0.338, 0.397)
L_ELBOW = 0.49   # how far down the arm, shoulder to wrist
L_HIP, L_KNEE, L_ANKLE = (0.088, 0.435), 0.24, 0.045
L_NECK = 0.795   # above it is the head
BODY_Y = 0.18
ARM_X, ARM_Z = 0.118, 0.30   # further out than this, above that: an arm
TAIL_Y, TAIL_Z = -0.095, 0.50   # further back than this, under that: the tail
# The robot's: shoulder (x, z), elbow and wrist heights; hip (x, z), knee and
# ankle heights.
R_SHOULDER, R_ELBOW, R_WRIST = (0.265, 1.234), 0.942, 0.663
R_HIP, R_KNEE, R_ANKLE = (0.119, 0.73), 0.398, 0.093
S = (R_SHOULDER[1] - R_HIP[1]) / (L_SHOULDER[1] - L_HIP[1])   # metres a height, above the hips

# The flat colours a face takes: the texture's own (k-means of it, sRGB), by
# material; what each material is in the file, as the species' are.
COLOURS = [
    ("tint_1", (0.40, 0.63, 0.30)), ("tint_1", (0.43, 0.65, 0.32)), ("tint_1", (0.37, 0.60, 0.27)),
    ("tint_1", (0.31, 0.55, 0.22)),
    ("Spot", (0.22, 0.45, 0.17)), ("Spot", (0.13, 0.38, 0.13)),
    ("tint_2", (0.97, 0.93, 0.60)),
    ("tint_3", (0.74, 0.36, 0.07)), ("tint_3", (0.90, 0.55, 0.25)),
    ("Eye_Dark", (0.02, 0.05, 0.01)),
]
MATS = {
    "tint_1": (0.40, 0.63, 0.30), "tint_2": (0.95, 0.92, 0.62), "tint_3": (0.80, 0.38, 0.08),
    "Spot": (0.10, 0.28, 0.08), "Eye_Dark": (0.03, 0.03, 0.04),
}
NAMES = list(MATS)
PARTS = ["body", "arm_L", "arm_R", "leg_L", "leg_R"]


def load():
    """The file's triangles: corners in shares of its height, faced to +Y, the
    soles at 0 and the body's middle over the origin; and each one's colour,
    as an index into NAMES."""
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=SRC)
    ob = next(o for o in bpy.context.scene.objects if o.type == "MESH")
    me = ob.data
    img = next(n.image for n in me.materials[0].node_tree.nodes
               if n.type == "TEX_IMAGE" and any(l.to_socket.name == "Base Color" for o in n.outputs for l in o.links))
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]
    nv, nf = len(me.vertices), len(me.polygons)
    co = np.empty(nv * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3) @ np.array(ob.matrix_world.to_3x3()).T + np.array(ob.matrix_world.translation)
    lo, hi = co.min(0), co.max(0)
    height = hi[2] - lo[2]
    mid = (lo + hi) / 2
    p = np.stack([-(co[:, 0] - mid[0]), -(co[:, 1] - mid[1]), co[:, 2] - lo[2]], axis=1) / height
    p[:, 1] -= BODY_Y
    tri = np.empty(nf * 3, np.int32)
    me.polygons.foreach_get("vertices", tri)
    tri = tri.reshape(-1, 3)
    uv = np.empty(len(me.loops) * 2, np.float32)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 3, 2).mean(1)   # every face a triangle, its loops in order
    col = px[np.clip((uv[:, 1] * h).astype(int), 0, h - 1), np.clip((uv[:, 0] * w).astype(int), 0, w - 1)]
    ref = np.array([c for _, c in COLOURS])
    near = ((col[:, None, :] - ref[None]) ** 2).sum(-1).argmin(1)
    label = np.array([NAMES.index(COLOURS[i][0]) for i in near])
    return p[tri], label   # (faces, 3 corners, xyz)


def part_of(c):
    """Which part a point of the lizard is in, by number (PARTS)."""
    x, y, z = c[:, 0], c[:, 1], c[:, 2]
    out = np.zeros(len(c), np.int32)
    arm = (np.abs(x) > ARM_X) & (z > ARM_Z) & (z < L_NECK)
    out[arm & (x < 0)], out[arm & (x > 0)] = 1, 2
    tail = (y < TAIL_Y) & (z < TAIL_Z)
    leg = (z < L_HIP[1]) & ~tail & ~arm & (np.abs(x) > 0.04)
    out[leg & (x < 0)], out[leg & (x > 0)] = 3, 4
    return out


def stretch(t, at, to):
    """t taken from the marks `at` to the marks `to`, straight between them
    and as the nearest stretch goes beyond either end."""
    out = np.empty_like(t)
    lo = t < at[0]
    out[lo] = to[0] + (t[lo] - at[0]) * S
    for i in range(len(at) - 1):
        m = (t >= at[i]) & ((t < at[i + 1]) | (i == len(at) - 2))
        out[m] = to[i] + (t[m] - at[i]) * (to[i + 1] - to[i]) / (at[i + 1] - at[i])
    return out


def at_rest(p, part):
    """Points of one part where they are on the robot at rest, metres."""
    x, y, z = p[:, 0], p[:, 1], p[:, 2]
    if part == 0:   # grown about the hips
        return np.stack([x * S, y * S, R_HIP[1] + (z - L_HIP[1]) * S], axis=1)
    sx = -1.0 if part in (1, 3) else 1.0
    if part in (1, 2):
        # along the arm from the shoulder, and across it, outwards
        d = np.array([L_WRIST[0] - L_SHOULDER[0], L_WRIST[1] - L_SHOULDER[1]])
        length = float(np.hypot(*d))
        u = d / length
        ax, az = x * sx - L_SHOULDER[0], z - L_SHOULDER[1]
        t, n = ax * u[0] + az * u[1], ax * -u[1] + az * u[0]
        down = stretch(t, [0.0, L_ELBOW * length, length],
                       [0.0, R_SHOULDER[1] - R_ELBOW, R_SHOULDER[1] - R_WRIST])
        # a hand its own size past the wrist
        past = t > length
        down[past] = R_SHOULDER[1] - R_WRIST + (t[past] - length) * S
        return np.stack([sx * (R_SHOULDER[0] + n * S), y * S, R_SHOULDER[1] - down], axis=1)
    up = stretch(z, [0.0, L_ANKLE, L_KNEE, L_HIP[1]], [0.0, R_ANKLE, R_KNEE, R_HIP[1]])
    low = z < 0.0
    up[low] = z[low] * S
    return np.stack([sx * (R_HIP[0] + (x * sx - L_HIP[0]) * S), y * S, up], axis=1)


# Which way to look out from a point to tell whether it is inside: none along
# an axis, where a ray would run down the flat of a cube's side.
RAYS = [Vector(d).normalized() for d in ((1, 0.013, 0.007), (-1, 0.011, -0.009), (0.012, 1, 0.006),
                                         (-0.008, -1, 0.011), (0.007, -0.012, 1))]


def inside(tree, c):
    """Whether c is inside the surface: a ray out of it crosses the surface an
    odd number of times. A part is open where it was cut from the next, and a
    ray through the cut counts wrong, so most of five say."""
    votes = 0
    for d in RAYS:
        at, n = c, 0
        while n < 64:
            hit = tree.ray_cast(at, d)[0]
            if hit is None:
                break
            n += 1
            at = hit + d * 1e-4
        votes += n & 1
    return votes * 2 > len(RAYS)


def cubes(tris, label, bones):
    """The lizard as cubes: for every cell with a part round its middle, the
    bone it follows and its material, in two grids; and the grid's corner."""
    part = part_of(tris.mean(1))
    rest = np.empty_like(tris)
    for k in range(len(PARTS)):
        m = part == k
        rest[m] = at_rest(tris[m].reshape(-1, 3), k).reshape(-1, 3, 3)
    lo = np.floor(rest.reshape(-1, 3).min(0) / CELL).astype(int) - 1
    hi = np.ceil(rest.reshape(-1, 3).max(0) / CELL).astype(int) + 1
    lo[2] = 0   # the soles on the ground
    size = hi - lo
    bone = np.full(size, -1, np.int32)

    kd = KDTree(len(rest))
    for i, c in enumerate(rest.mean(1)):
        kd.insert(c, i)
    kd.balance()

    heights = {b: bones[b].z for b in ("spine", "chest", "head")}
    for k in range(len(PARTS)):
        m = part == k
        tree = BVHTree.FromPolygons([tuple(c) for c in rest[m].reshape(-1, 3).tolist()],
                                    [(i, i + 1, i + 2) for i in range(0, int(m.sum()) * 3, 3)])
        plo = np.floor(rest[m].reshape(-1, 3).min(0) / CELL).astype(int) - lo
        phi = np.ceil(rest[m].reshape(-1, 3).max(0) / CELL).astype(int) - lo
        plo[2] = max(plo[2], 0)
        for i in range(plo[0], phi[0]):
            for j in range(plo[1], phi[1]):
                for kk in range(plo[2], phi[2]):
                    c = Vector(((lo[0] + i + 0.5) * CELL, (lo[1] + j + 0.5) * CELL, (lo[2] + kk + 0.5) * CELL))
                    if not inside(tree, c):
                        continue
                    if k == 0:
                        name = ("head" if c.z > heights["head"] else "chest" if c.z > heights["chest"]
                                else "spine" if c.z > heights["spine"] else "hips")
                        if c.y < TAIL_Y * S and c.z < R_HIP[1] + (TAIL_Z - L_HIP[1]) * S:
                            name = "hips"   # the tail, whatever its height
                    elif k in (1, 2):
                        name = ("upper_arm" if c.z > R_ELBOW else "forearm" if c.z > R_WRIST else "hand") + "_" + "LR"[k - 1]
                    else:
                        name = ("thigh" if c.z > R_KNEE else "shin" if c.z > R_ANKLE else "foot") + "_" + "LR"[k - 3]
                    bone[i, j, kk] = BONE_NAMES.index(name)
        print(f"{PARTS[k]}: {int(m.sum())} triangles")
    print(f"{int((bone >= 0).sum())} cubes of {CELL * 100:g} cm in a grid of {tuple(size)}")
    # The colour of each side of each cube that shows: what most of the five
    # triangles nearest that side's middle are.
    mats = {}
    half = {1: 1.0, -1: 0.0}
    for a in range(3):
        for sign in (1, -1):
            there = np.full_like(bone, -1)
            src = [slice(None)] * 3
            dst = [slice(None)] * 3
            src[a], dst[a] = (slice(1, None), slice(0, -1)) if sign > 0 else (slice(0, -1), slice(1, None))
            there[tuple(dst)] = bone[tuple(src)]
            mat = np.zeros(size, np.int32)
            for i, j, kk in np.argwhere((bone >= 0) & (there != bone)):
                c = [lo[0] + i + 0.5, lo[1] + j + 0.5, lo[2] + kk + 0.5]
                c[a] += half[sign] - 0.5
                count = {}
                for _, fi, _ in kd.find_n(Vector(c) * CELL, 5):
                    count[label[fi]] = count.get(label[fi], 0) + 1
                mat[i, j, kk] = max(count, key=count.get)
            mats[(a, sign)] = mat
    return bone, mats, lo


BONE_NAMES = ["hips", "spine", "chest", "head",
              "upper_arm_L", "forearm_L", "hand_L", "upper_arm_R", "forearm_R", "hand_R",
              "thigh_L", "shin_L", "foot_L", "thigh_R", "shin_R", "foot_R"]


def shell(bone, mats, lo, only=None):
    """The cubes' faces that show: one a bone and a colour, side by side ones
    merged into rectangles. A face between two cubes of one bone is inside and
    left out; one between two bones is kept, each bone's cubes closed. Quads as
    (four corners, bone, material)."""
    quads = []
    size = bone.shape
    for a in range(3):
        u, v = (a + 1) % 3, (a + 2) % 3
        for sign in (1, -1):
            for s in range(size[a]):
                here = np.take(bone, s, axis=a)
                colour = np.take(mats[(a, sign)], s, axis=a)
                t = s + sign
                there = np.take(bone, t, axis=a) if 0 <= t < size[a] else np.full_like(here, -1)
                show = (here >= 0) & (there != here)
                if only is not None:
                    show &= np.isin(here, only)
                if a == 1:   # np.take leaves the axes (0, 2): wanted (u, v) = (2, 0)
                    here, colour, show = here.T, colour.T, show.T
                key = np.where(show, here * 16 + colour, -1)
                done = np.zeros_like(show)
                for i in range(key.shape[0]):
                    for j in range(key.shape[1]):
                        k = key[i, j]
                        if k < 0 or done[i, j]:
                            continue
                        w = 1
                        while j + w < key.shape[1] and key[i, j + w] == k and not done[i, j + w]:
                            w += 1
                        h = 1
                        while i + h < key.shape[0] and (key[i + h, j:j + w] == k).all() and not done[i + h, j:j + w].any():
                            h += 1
                        done[i:i + h, j:j + w] = True
                        plane = s + (1 if sign > 0 else 0)
                        corners = []
                        for du, dv in ((0, 0), (h, 0), (h, w), (0, w)):
                            c = [0, 0, 0]
                            c[a], c[u], c[v] = plane, i + du, j + dv
                            corners.append(tuple(c))
                        if sign < 0:
                            corners.reverse()
                        quads.append((corners, int(k) // 16, int(k) % 16))
    return quads


def mesh(rig, name, quads, lo, offset=Vector()):
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    groups = {g.name: g.index for g in ob.vertex_groups}
    used = sorted({q[2] for q in quads})
    bm = bmesh.new()
    dl = bm.verts.layers.deform.verify()
    made = {}
    for corners, b, m in quads:
        vs = []
        for c in corners:
            v = made.get((c, b))
            if v is None:
                v = bm.verts.new(Vector(((lo[0] + c[0]) * CELL, (lo[1] + c[1]) * CELL, (lo[2] + c[2]) * CELL)) + offset)
                v[dl][groups[BONE_NAMES[b]]] = 1.0
                made[(c, b)] = v
            vs.append(v)
        f = bm.faces.new(vs)
        f.material_index = used.index(m)
        f.smooth = False
    bm.to_mesh(me)
    bm.free()
    for m in used:
        me.materials.append(bpy.data.materials[NAMES[m]])
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    print(f"{name}: {len(quads)} faces, {len(made)} corners")
    return ob


def look(ob, tag):
    """Four views of the object at rest, to judge it by."""
    os.makedirs(LOOK, exist_ok=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light, sc.display.shading.color_type = "STUDIO", "MATERIAL"
    sc.render.resolution_x, sc.render.resolution_y = 700, 900
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type, cam.data.ortho_scale = "ORTHO", 2.2
    c = Vector((0, -0.2, 0.9))
    for name, d in (("front", (0, 1, 0)), ("side", (1, 0, 0)), ("back", (0, -1, 0)), ("quarter", (0.7, 0.7, 0.35))):
        d = Vector(d).normalized()
        cam.location = c + d * 6
        cam.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
        sc.render.filepath = os.path.join(LOOK, f"lizard_{tag}_{name}.png")
        bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)


def materials():
    for k, rgb in MATS.items():
        material(k, rgb, 0.0, 0.6)


def main():
    tris, label = load()
    robot_path = os.path.join(GAME, "robot.gltf")
    rig, tools = fresh(robot_path)
    bones = {b.name: rig.matrix_world @ b.head_local for b in rig.data.bones}
    print("robot joints:", {k: tuple(round(c, 3) for c in v) for k, v in bones.items() if k in BONE_NAMES})
    bone, mats, lo = cubes(tris, label, bones)
    materials()
    keep_tools(tools)
    tools.name = NAME + "_tools"
    body = mesh(rig, NAME, shell(bone, mats, lo), lo)
    look(body, "rest")
    export(rig, [body, tools], os.path.join(OUT, NAME + ".gltf"), robot_path)

    arms_path = os.path.join(GAME, "robot_arms.gltf")
    rig, tools = fresh(arms_path)
    materials()
    keep_tools(tools)
    arm = [BONE_NAMES.index(b) for b in BONE_NAMES if b in ARM_BONES]
    arms = mesh(rig, NAME + "_arms", shell(bone, mats, lo, arm), lo, TO_ARMS)
    export(rig, [arms, tools], os.path.join(OUT, NAME + "_arms.gltf"), arms_path)
    for f in (NAME + ".gltf", NAME + ".bin", NAME + "_arms.gltf", NAME + "_arms.bin"):
        print(f"{f}: {os.path.getsize(os.path.join(OUT, f))} bytes")


if __name__ == "__main__":
    main()
