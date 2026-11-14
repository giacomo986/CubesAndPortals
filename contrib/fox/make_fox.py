# The fox man, a player body from Meshy (meshy/concept.png and meshy/fox.glb
# beside this file: tools/meshy_generate.py image, smart topology, an A-pose,
# 12486 triangles in sixteen separate pieces with a texture), on the robot's
# own skeleton. An add-on, not the game's (mod.conf, init.luau here). From the
# checkout's top:
#
#   flatpak run org.blender.Blender --background --python "$PWD/contrib/fox/make_fox.py"
#
# Meshy made it of pieces that are not joined -- the legs with the hips, each
# arm with its glove, the vest, the belt, the head, each ear, the tail -- so
# each is brought onto the robot's joints whole, as contrib/lizard's parts
# are: the body grown about the hips until the shoulders are at the robot's
# height (and a little wider, the shoulders out to the robot's), each arm
# turned down from the A-pose about its shoulder, stretched to the robot's
# upper arm and forearm, turned whole about its length so the palm faces the
# thigh (not wrung along the forearm: that showed), and its hand closed
# into a fist round where the robot holds a tool, each leg stood upright under
# its hip and stretched to the robot's thigh and shin. A clip then puts a hand
# or a foot where it puts the robot's.
#
# Meshy's pieces are not closed. Each is open where it went into another (an
# arm at the shoulder, the vest at its hem, the tail at its root), and a pose
# that bends there shows its inside: every such end takes a lid. The head is
# worse, a shell torn into shards with gaps at the brow, the crown and the
# back, and is made again: the closed, faceted solid round its points
# (reskulled), its eyes and nose brought out onto it.
#
# No texture is kept. Each face takes one flat material, read off Meshy's
# texture inside the face (colour) and held to what its piece may be painted:
# the fur tint_1, the muzzle, chest and ears' insides tint_2 and the vest
# tint_3, which the player dyes; gloves, boots and ear tips, the belt, its
# buckle, the eyes and the nose stay. Thinned to TRIS triangles, the head and
# the vertices on a border between two colours the last to go
# (tools/make_mole_meshy.py's way).
#
# Writes beside this file, none of them with clips (the client plays the
# player model's on them: borrow_clips in src/client/main.cpp):
#   robot_fox               the body, without its tail
#   robot_fox_arms          the arms seen in first person, on robot_arms.gltf's rig
#   robot_fox_worn          the tail, always worn, on WAGS wag_* links that swing it
#   robot_fox_style_patch   the styles: a patch over one eye,
#   robot_fox_style_quiver  and a quiver of arrows on the back
#   robot_fox_<extra>       the robot's six extras, measured onto its head and neck
# and views of them at rest to build/look/fox_*.png (FOX_LOOK names another place).
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
sys.path.insert(0, os.path.join(ROOT, "tools"))   # the game's own tools, for the rig and the export
from make_robot_extras import MATS as EXTRA_MATS, P, extras, material  # noqa: E402
from make_species import ARM_BONES, TO_ARMS, blurred, build, export, fresh, keep_tools  # noqa: E402

GAME = os.path.join(ROOT, "mods", "robot")   # the robot it is made on
OUT = os.environ.get("FOX_OUT", HERE)
SRC = os.path.join(HERE, "meshy", "fox.glb")
LOOK = os.environ.get("FOX_LOOK", os.path.join(ROOT, "build", "look"))
NAME = "robot_fox"

TRIS = int(os.environ.get("FOX_TRIS", 7000))   # the most triangles the body keeps
BORDER = 3.0   # how much longer a vertex on a colour's border is kept
FACET = 22     # degrees: a fold sharper than this is an edge
WAGS = 4       # the tail's links, wag_1 at the hips to wag_4 at the tip

# The fox's own joints, read off front and side renders of the file, in shares
# of its height: x either side of its middle, z up from its soles.
F_SHOULDER, F_WRIST = (0.105, 0.675), (0.24, 0.50)
F_ELBOW = 0.5   # how far down the arm, shoulder to wrist
F_HIP, F_KNEE, F_ANKLE = (0.072, 0.43), 0.24, (0.118, 0.055)   # the ankle's x: the legs stand apart
F_NECK = 0.74   # above it is the head
ARM_Y = -0.025  # the arms' middle, behind the body's
PALM = 90       # degrees the hands are turned in, from facing ahead
SHOULDER_END, SHOULDER_NARROW = 0.3, 0.3   # the arm's top, this share of it, is drawn in to this much of its width
GRIP = 0.045    # metres under the wrist a tool's handle is (tool_* on hand_R, mods/robot/robot.gltf)
PALM_OUT = 0.04   # the hand's middle this far out from the handle: its palm lies on it
CURL = 200      # degrees the fingertips are bent round the handle
HAND = {"long": 0.2}   # metres wrist to fingertips, measured off the file (load)
WIDE = 1.2      # the chest this much wider: the shoulders out to the robot's
# The robot's: shoulder (x, z), elbow and wrist heights; hip (x, z), knee and
# ankle heights.
R_SHOULDER, R_ELBOW, R_WRIST = (0.265, 1.234), 0.942, 0.663
R_HIP, R_KNEE, R_ANKLE = (0.119, 0.73), 0.398, 0.093
S = (R_SHOULDER[1] - R_HIP[1]) / (F_SHOULDER[1] - F_HIP[1])   # metres a height, above the hips

# Colour (linear, as a glTF has it), metallic, roughness. The tints are what an
# undyed fox looks like.
MATS = {
    "tint_1": ((0.80, 0.22, 0.03), 0.0, 0.6), "tint_2": ((0.85, 0.74, 0.50), 0.0, 0.6),
    "tint_3": ((0.13, 0.33, 0.09), 0.0, 0.6),
    "Fox_Dark": ((0.14, 0.06, 0.045), 0.0, 0.7), "Fox_Leather": ((0.13, 0.05, 0.015), 0.0, 0.6),
    "Fox_Buckle": ((0.35, 0.37, 0.42), 0.8, 0.35), "Eye_Dark": ((0.003, 0.003, 0.004), 0.0, 0.2),
}


def colour(rgb):
    """The material of a face Meshy painted `rgb`."""
    r, g, b = (float(x) for x in rgb)
    v = max(r, g, b)
    if g > 1.12 * r and g > 1.1 * b:
        return "tint_3"
    if v < 0.3:
        return "Fox_Dark"
    if min(r, g, b) > 0.5 * v and v > 0.55:
        return "tint_2"
    return "tint_1"


def stretch(t, at, to, beyond):
    """t taken from the marks `at` to the marks `to`, straight between them,
    and by `beyond` a unit past either end."""
    if t < at[0]:
        return to[0] + (t - at[0]) * beyond
    for i in range(len(at) - 1):
        if t < at[i + 1]:
            return to[i] + (t - at[i]) * (to[i + 1] - to[i]) / (at[i + 1] - at[i])
    return to[-1] + (t - at[-1]) * beyond


def wide(z):
    """How much wider than its height's share the body is at z."""
    if z < F_HIP[1]:
        return 1.0
    if z < F_HIP[1] + 0.08:
        return 1.0 + (WIDE - 1.0) * (z - F_HIP[1]) / 0.08
    if z < F_SHOULDER[1]:
        return WIDE
    if z < F_NECK:
        return WIDE + (1.0 - WIDE) * (z - F_SHOULDER[1]) / (F_NECK - F_SHOULDER[1])
    return 1.0


def at_rest(p, part):
    """A point of one part (shares of the fox's height) where it is on the
    robot at rest, metres."""
    x, y, z = p
    if part.startswith("arm"):
        sx = -1.0 if x < 0 else 1.0
        # along the arm from the shoulder, and across it, outwards
        dx, dz = F_WRIST[0] - F_SHOULDER[0], F_WRIST[1] - F_SHOULDER[1]
        length = math.hypot(dx, dz)
        ux, uz = dx / length, dz / length
        ax, az = x * sx - F_SHOULDER[0], z - F_SHOULDER[1]
        t, n = ax * ux + az * uz, ax * -uz + az * ux
        # a hand its own size past the wrist
        down = stretch(t, [0.0, F_ELBOW * length, length], [0.0, R_SHOULDER[1] - R_ELBOW, R_SHOULDER[1] - R_WRIST], S)
        # Meshy's palms face ahead, as the picture's do: the arm is turned
        # about its length, whole, until they face the thighs, the thumbs
        # ahead, as the robot's hands hang. Whole -- an arm is its own piece,
        # its top inside the shoulder -- because a turn that grows along the
        # forearm wrings it, and a wrung arm shows standing still.
        turn = math.radians(PALM)
        out, fore = n * S, (y - ARM_Y) * S
        out, fore = out * math.cos(turn) - fore * math.sin(turn), out * math.sin(turn) + fore * math.cos(turn)
        # Turned, the arm's flat top stands out of the shoulder fore and aft
        # as two corners: it is drawn in to its middle there, a rounded end
        # inside the vest's armhole.
        top = max(0.0, min(1.0, t / (SHOULDER_END * length)))
        narrow = SHOULDER_NARROW + (1.0 - SHOULDER_NARROW) * top * top * (3.0 - 2.0 * top)
        out, fore = out * narrow, fore * narrow
        if down < 0.0:
            down *= 0.2   # and what is above the shoulder's joint flattened onto it: no fin over the vest
        # The hand is closed into a fist round where the robot's tools are
        # held: GRIP under the wrist, the handle running fore and aft. Its
        # palm is moved out to lie on the handle, and what is under the handle
        # (the fingers, the thumb with them) is bent round it, in to the
        # thigh and up again.
        past = (t - length) * S
        if past > 0.0:
            if past < GRIP:
                out += PALM_OUT * (past / GRIP) ** 2 * (3.0 - 2.0 * past / GRIP)
            else:
                round_it = max(0.004, out + PALM_OUT)
                bend = math.radians(CURL) * min(1.0, (past - GRIP) / max(1e-6, HAND["long"] - GRIP))
                out, down = round_it * math.cos(bend), R_SHOULDER[1] - R_WRIST + GRIP + round_it * math.sin(bend)
        return Vector((sx * (R_SHOULDER[0] + out), fore, R_SHOULDER[1] - down))
    if part == "legs" and z < F_HIP[1]:
        sx = -1.0 if x < 0 else 1.0
        # upright: the leg's own lean out, hip to ankle, taken off
        lean = (F_ANKLE[0] - F_HIP[0]) * min(1.0, (F_HIP[1] - z) / (F_HIP[1] - F_ANKLE[1]))
        up = stretch(z, [0.0, F_ANKLE[1], F_KNEE, F_HIP[1]], [0.0, R_ANKLE, R_KNEE, R_HIP[1]], S)
        return Vector(((x - sx * lean) * S, y * S, up))
    return Vector((x * S * wide(z), y * S, R_HIP[1] + (z - F_HIP[1]) * S))   # grown about the hips


def load():
    """The file's mesh, welded, each vertex where it is on the robot at rest;
    which part each vertex is of, each face's material, and the eyes' middles."""
    bpy.ops.import_scene.gltf(filepath=SRC)
    ob = next(o for o in bpy.context.selected_objects if o.type == "MESH")
    me = ob.data
    img = next(n.image for n in me.materials[0].node_tree.nodes if n.type == "TEX_IMAGE")
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]   # sRGB as stored
    mw = ob.matrix_world.copy()
    co = np.array([mw @ v.co for v in me.vertices])
    lo, hi = co.min(0), co.max(0)
    height = hi[2] - lo[2]
    p = np.stack([-co[:, 0], -co[:, 1], co[:, 2] - lo[2]], axis=1) / height   # faced to +Y
    # Its middle: the head and shoulders' across, the chest's fore and aft (the
    # tail reaches far back and to one side).
    cx = np.median(p[p[:, 2] > 0.62][:, 0])
    chest = p[(p[:, 2] > 0.5) & (p[:, 2] < 0.72) & (abs(p[:, 0] - cx) < 0.08)]
    cy = (chest[:, 1].min() + chest[:, 1].max()) / 2
    for v, q in zip(me.vertices, p):
        v.co = Vector((q[0] - cx, q[1] - cy, q[2]))
    ob.matrix_world.identity()
    bm = bmesh.new()   # split at the texture's seams: made whole, its pieces stay apart
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    # Its pieces are open where one went into another -- an arm at the
    # shoulder, the vest at its hem, the tail at its root -- and a pose that
    # bends there shows the inside: each such end is closed.
    drawn = len(bm.faces)
    lids = bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if len(e.link_faces) == 1], sides=0)["faces"]
    bmesh.ops.triangulate(bm, faces=lids)
    # What that leaves (a rim that touches itself): each turn of the rim,
    # walked the way its faces run, takes a fan from its middle.
    rim = {e.link_loops[0] for e in bm.edges if len(e.link_faces) == 1}
    while rim:
        lp = first = next(iter(rim))
        turn = []
        while True:
            turn.append(lp)
            rim.discard(lp)
            lp = lp.link_loop_next
            while len(lp.edge.link_faces) != 1:
                lp = lp.link_loop_radial_next.link_loop_next
            if lp is first:
                break
        if len(turn) == 3 and len({x.face for x in turn}) == 1:
            continue   # a triangle on its own, hanging by a corner
        mid = bm.verts.new(sum((x.vert.co for x in turn), Vector()) / len(turn))
        for x in turn:   # along the edge the other way from the face it has
            bm.faces.new((x.link_loop_next.vert, x.vert, mid))
    bm.faces.index_update()
    bm.faces.index_update()
    print("FOX closed", len(bm.faces) - drawn, "faces over open ends;", len([e for e in bm.edges if len(e.link_faces) == 1]), "edges left open")
    bm.to_mesh(me)
    bm.free()
    me.update()

    # Which piece each vertex is: Meshy's parts are not joined to each other.
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
    pts = np.array([v.co[:] for v in me.vertices])
    piece = np.array(piece)
    kind, fixed, eyes = {}, {}, []
    for k in range(n):
        q = pts[piece == k]
        lo, hi = q.min(0), q.max(0)
        size = hi - lo
        if lo[1] < -0.2:
            kind[k] = "tail"
        elif abs(q[:, 0]).max() > 0.25 and lo[2] > 0.3:
            kind[k] = "arm"
        elif lo[2] < 0.02:
            kind[k] = "legs"
        elif lo[2] >= F_NECK - 0.02:
            kind[k] = "head"
            if size.max() < 0.06:   # an eye, the nose
                fixed[k] = "Eye_Dark"
                if abs(q[:, 0].mean()) > 0.015:
                    eyes.append(k)
        else:
            kind[k] = "body"
            if 0.44 < lo[2] and hi[2] < 0.54:   # round the waist: the belt, or small and ahead, its buckle
                fixed[k] = "Fox_Leather" if size[0] > 0.15 else "Fox_Buckle"
        print(f"FOX piece {k}: {kind[k]} {fixed.get(k, '')} {len(q)} vertices")
    part = [kind[k] for k in piece]
    # The head itself, the piece of it in the middle (an ear is to one side):
    # made again below.
    skull = min((k for k in kind if kind[k] == "head" and k not in fixed), key=lambda k: abs(pts[piece == k][:, 0].mean()))
    # What a piece may be painted, and what it is where the texture says
    # another: the seams of Meshy's texture bleed one piece's colour on the next.
    may = {"arm": ("tint_1", "Fox_Dark"), "legs": ("tint_1", "tint_2", "Fox_Dark"), "tail": ("tint_1", "tint_2"),
           "head": ("tint_1", "tint_2", "Fox_Dark"), "body": ("tint_1", "tint_2", "tint_3")}

    uv = me.uv_layers.active.data
    labels = []
    for f in me.polygons:
        k = piece[f.vertices[0]]
        if k in fixed:
            labels.append(fixed[k])
            continue
        if f.index >= drawn:   # a lid: no texture is on it
            labels.append(None)
            continue
        at = [uv[i].uv for i in f.loop_indices]
        m = sum(at, Vector((0, 0))) / len(at)
        at = [m] + [(q + m) / 2 for q in at]   # inside the face: its corners sit on the borders
        name = colour(np.median([px[int(q.y % 1.0 * h) % h, int(q.x % 1.0 * w) % w] for q in at], axis=0))
        if name not in may[kind[k]]:
            name = "tint_1"
        labels.append(name)
    # The vest is two pieces of its own, green but for the chest in its
    # neck: no fur shows on either.
    for k in kind:
        if kind[k] == "body" and k not in fixed:
            mine = [f.index for f in me.polygons if piece[f.vertices[0]] == k]
            if sum(labels[i] == "tint_3" for i in mine) > 0.5 * len(mine):
                for i in mine:
                    if labels[i] == "tint_1":
                        labels[i] = "tint_3"
    vface = [[] for _ in me.vertices]
    for f in me.polygons:
        for v in f.vertices:
            vface[v].append(f.index)
    # A lid takes the colour of what it closes.
    while None in labels:
        new = list(labels)
        for f in me.polygons:
            if labels[f.index] is None:
                round_it = [labels[g] for v in f.vertices for g in vface[v] if labels[g] is not None]
                new[f.index] = max(set(round_it), key=round_it.count) if round_it else None
        assert new != labels, "a lid touches nothing"
        labels = new
    # A speck takes the colour round it.
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

    dx, dz = F_WRIST[0] - F_SHOULDER[0], F_WRIST[1] - F_SHOULDER[1]
    HAND["long"] = max(((abs(v.co.x) - F_SHOULDER[0]) * dx + (v.co.z - F_SHOULDER[1]) * dz) / math.hypot(dx, dz) - math.hypot(dx, dz)
                       for v, w in zip(me.vertices, part) if w == "arm") * S
    print(f"FOX hands: {HAND['long']:.3f} m from the wrist to the fingertips")
    rest = [at_rest(v.co, w) for v, w in zip(me.vertices, part)]
    # The tail hangs off to one side: turned about its root to hang straight
    # behind, in the middle, where its links are.
    tail = [i for i, w in enumerate(part) if w == "tail"]
    ys = sorted(tail, key=lambda i: -rest[i].y)
    root = sum((rest[i] for i in ys[:max(3, len(ys) // 10)]), Vector()) / max(3, len(ys) // 10)
    mid = sum((rest[i] for i in tail), Vector()) / len(tail)
    turn = math.atan2(-1.0, 0.0) - math.atan2(mid.y - root.y, mid.x - root.x)
    c, s = math.cos(turn), math.sin(turn)
    for i in tail:
        d = rest[i] - root
        rest[i] = Vector((c * d.x - s * d.y, root.y + s * d.x + c * d.y, rest[i].z))
    faces = [tuple(f.vertices) for f in me.polygons]
    rest, faces, labels, part, eyes = reskulled(rest, faces, labels, part, piece, skull, [k for k in fixed if kind[k] == "head"], eyes)
    print("FOX faces", len(faces), {n: labels.count(n) for n in dict.fromkeys(labels)}, "eyes", [tuple(round(x, 3) for x in e) for e in eyes])
    bpy.data.objects.remove(ob)
    return rest, faces, labels, part, eyes


def reskulled(rest, faces, labels, part, piece, skull, dots, eyes):
    """The head made again. Meshy's is a torn shell, shards with gaps between
    them that show its inside from the brow, the crown and the back: what
    takes its place is the closed, faceted solid round its points. Cream under
    the line from the cheeks' tips to the nose, in front, as the picture has
    it, and the fur's colour elsewhere. The eyes and the nose (`dots`, pieces
    of their own) are brought out onto it; the eyes' middles are returned."""
    mine = [i for i in range(len(rest)) if piece[i] == skull]
    bm = bmesh.new()
    for i in mine:
        bm.verts.new(rest[i])
    bmesh.ops.convex_hull(bm, input=bm.verts[:])
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    assert not [e for e in bm.edges if len(e.link_faces) != 2], "the head is not closed"
    bm.verts.index_update()
    hull = [v.co.copy() for v in bm.verts]
    tris = [tuple(v.index for v in f.verts) for f in bm.faces]
    normals = [f.normal.copy() for f in bm.faces]
    bm.free()
    nose = max(hull, key=lambda c: c.y)
    cheek = max(hull, key=lambda c: abs(c.x))
    # The plane through the nose and both cheeks' tips (the head is the same
    # either side): its height at a point.
    def line(c):
        return cheek.z + (nose.z - cheek.z) * (c.y - cheek.y) / (nose.y - cheek.y)
    paint = []
    for f, n in zip(tris, normals):
        c = sum((hull[v] for v in f), Vector()) / 3
        paint.append("tint_2" if c.z < line(c) - 0.004 and c.y > cheek.y - 0.02 and n.z < 0.35 and n.y > -0.3 else "tint_1")
    print("FOX head:", len(hull), "points,", len(tris), "faces,", paint.count("tint_2"), "of them cream")
    # An eye sunk in it comes out to its surface.
    tree = BVHTree.FromPolygons(hull, tris)
    middles = []
    for k in dots:
        own = [i for i in range(len(rest)) if piece[i] == k]
        lo = Vector([min(rest[i][a] for i in own) for a in range(3)])
        hi = Vector([max(rest[i][a] for i in own) for a in range(3)])
        c = (lo + hi) / 2
        at, n, _, _ = tree.find_nearest(c)
        if (c - at).dot(n) < 0:
            for i in own:
                rest[i] = rest[i] + (at - c)
            c = at
        if k in eyes:
            middles.append(c)
    base = len(rest)
    gone = set(mine)
    kept = [(f, n) for f, n in zip(faces, labels) if not gone.intersection(f)]
    rest = rest + hull
    part = ["gone" if i in gone else w for i, w in enumerate(part)] + ["head"] * len(hull)
    faces = [f for f, _ in kept] + [tuple(base + v for v in f) for f in tris]
    labels = [n for _, n in kept] + paint
    return (*only(rest, faces, labels, part, lambda w: w != "gone"), middles)


def only(rest, faces, labels, part, want):
    """The vertices whose part `want` says yes to, and the faces between them."""
    keep = [i for i, w in enumerate(part) if want(w)]
    index = {v: i for i, v in enumerate(keep)}
    fs = [(tuple(index[v] for v in f), n) for f, n in zip(faces, labels) if all(v in index for v in f)]
    return [rest[i] for i in keep], [f for f, _ in fs], [n for _, n in fs], [part[i] for i in keep]


def thin(name, co, faces, labels, part, tris):
    """A mesh of that, thinned to `tris` triangles: the vertices on a border
    between two colours are the last to go, and each face left takes the
    colour the surface had where it now lies. The object (a vertex group for
    each part) and its faces' materials."""
    me = bpy.data.meshes.new(name)
    me.from_pydata(co, [], faces)
    me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for w in dict.fromkeys(part):
        ob.vertex_groups.new(name="part_" + w).add([i for i, x in enumerate(part) if x == w], 1.0, "REPLACE")
    if len(faces) <= tris:
        return ob, labels
    was = BVHTree.FromPolygons([c.copy() for c in co], faces)
    vface = [[] for _ in co]
    for i, f in enumerate(faces):
        for v in f:
            vface[v].append(i)
    edge = ob.vertex_groups.new(name="border")
    for i, fs in enumerate(vface):
        if len({labels[g] for g in fs}) > 1 or part[i] == "head":   # the face is what is looked at
            edge.add([i], 1.0, "REPLACE")
    mod = ob.modifiers.new("dec", "DECIMATE")
    mod.ratio = tris / len(faces)
    mod.vertex_group, mod.invert_vertex_group, mod.vertex_group_factor = "border", True, BORDER
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=mod.name)
    ob.vertex_groups.remove(ob.vertex_groups["border"])
    new = []
    for f in me.polygons:
        pts = [me.vertices[v].co for v in f.vertices]
        m = sum(pts, Vector()) / len(pts)
        count = {}
        for q, share in [(m, 1.5)] + [((q + m) / 2, 1.0) for q in pts]:
            n = labels[was.find_nearest(q)[2]]
            count[n] = count.get(n, 0.0) + share
        new.append(max(count, key=count.get))
    # A triangle that juts into another colour, two of its three sides on it,
    # takes that colour: the borders run straighter.
    sides = {}
    for f in me.polygons:
        for key in f.edge_keys:
            sides.setdefault(key, []).append(f.index)
    for _ in range(3):
        nxt = list(new)
        for f in me.polygons:
            count = {}
            for key in f.edge_keys:
                for g in sides[key]:
                    if g != f.index:
                        count[new[g]] = count.get(new[g], 0) + 1
            best = max(count, key=count.get) if count else new[f.index]
            if best != new[f.index] and count[best] >= 2:
                nxt[f.index] = best
        new = nxt
    return ob, new


def parts_of(ob):
    """Which part each vertex of a thinned object is of."""
    names = {g.index: g.name[5:] for g in ob.vertex_groups if g.name.startswith("part_")}
    out = []
    for v in ob.data.vertices:
        gs = [g for g in v.groups if g.group in names]
        out.append(names[max(gs, key=lambda g: g.weight).group] if gs else "body")
    return out


def kept(ob, labels, weights):
    """A thinned object as plain data: it outlives the scene."""
    me = ob.data
    return {"co": [v.co.copy() for v in me.vertices], "faces": [tuple(f.vertices) for f in me.polygons],
            "mat": list(labels), "w": weights}


def bound(ob, bones):
    """The fox's body bound to the robot's bones, by where each vertex is at
    rest: the head whole, the rest by height, a joint blurred so it bends."""
    me = ob.data
    part = parts_of(ob)
    torso = sorted(((bones[b].z, b) for b in ("hips", "spine", "chest", "neck")))
    weights = []
    for v, w in zip(me.vertices, part):
        c = v.co
        side = "L" if c.x < 0 else "R"
        if w == "head":
            name = "head"
        elif w == "arm":
            name = ("upper_arm_" if c.z > R_ELBOW else "forearm_" if c.z > R_WRIST else "hand_") + side
        elif w == "legs" and c.z < R_HIP[1]:
            name = ("thigh_" if c.z > R_KNEE else "shin_" if c.z > R_ANKLE else "foot_") + side
        else:
            name = [b for z, b in torso if z <= c.z][-1] if c.z >= torso[0][0] else "hips"
        weights.append({name: 1.0})
    weights = blurred(me, weights, passes=3)
    return [{"head": 1.0} if w == "head" else x for x, w in zip(weights, part)], part


def linked(ob):
    """The tail bound to its links by how far along it each vertex is, and the
    links' joints: (name, parent, head, tail), world at rest, down its middle."""
    me = ob.data
    pts = [v.co for v in me.vertices]
    order = sorted(range(len(pts)), key=lambda i: -pts[i].y)
    n = max(3, len(pts) // 10)
    root = sum((pts[i] for i in order[:n]), Vector()) / n
    tip = sum((pts[i] for i in order[-n:]), Vector()) / n
    axis = tip - root
    t = [max(0.0, min(0.999, (c - root).dot(axis) / axis.length_squared)) for c in pts]
    weights = blurred(me, [{f"wag_{int(x * WAGS) + 1}": 1.0} for x in t], passes=2)
    marks = []
    for k in range(WAGS + 1):
        nearby = [c for c, x in zip(pts, t) if abs(x - k / WAGS) < 0.5 / WAGS]
        m = sum(nearby, Vector()) / len(nearby) if nearby else root + axis * (k / WAGS)
        marks.append((0.0, (root + axis * (k / WAGS)).y, m.z))
    joints, parent = [], "hips"
    for k in range(WAGS):
        joints.append((f"wag_{k + 1}", parent, marks[k], marks[k + 1]))
        parent = f"wag_{k + 1}"
    print("FOX tail:", len(pts), "vertices, links at", [tuple(round(x, 2) for x in j[2]) for j in joints])
    return weights, joints


def mesh(rig, name, data, offset=Vector(), want=None):
    """Kept data as a mesh bound to `rig`: the faces all of whose corners follow
    bones `want` has (all of them without), moved by `offset`."""
    bones = {b.name for b in rig.data.bones}
    keep = [i for i, w in enumerate(data["w"]) if want is None or all(b in want for b in w)]
    index = {v: i for i, v in enumerate(keep)}
    fs = [(tuple(index[v] for v in f), m) for f, m in zip(data["faces"], data["mat"]) if all(v in index for v in f)]
    me = bpy.data.meshes.new(name)
    me.from_pydata([data["co"][i] + offset for i in keep], [], [f for f, _ in fs])
    order = list(dict.fromkeys(m for _, m in fs))
    for m in order:
        me.materials.append(bpy.data.materials[m])
    for f, (_, m) in zip(me.polygons, fs):
        f.material_index = order.index(m)
    me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for b in bones:
        ob.vertex_groups.new(name=b)
    for i, v in enumerate(keep):
        w = {b: x for b, x in data["w"][v].items() if b in bones}
        assert w, f"{name}: a vertex with no bone of this rig"
        total = sum(w.values())
        for b, x in w.items():
            ob.vertex_groups[b].add([i], x / total, "REPLACE")
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    # The picture is of flat faces: an edge that folds stays sharp, and only
    # what curves gently is smoothed.
    for x in bpy.data.objects:
        x.select_set(False)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(FACET))
    print(f"FOX {name}: {len(me.polygons)} triangles, {len(me.vertices)} vertices")
    return ob


def look(tag, hide=()):
    """Four views of what the scene has at rest, to judge it by."""
    os.makedirs(LOOK, exist_ok=True)
    sc = bpy.context.scene
    for o in sc.objects:
        o.hide_render = o.name in hide
    sc.render.engine = "BLENDER_WORKBENCH"
    sh = sc.display.shading
    sh.light, sh.color_type, sh.show_specular_highlight = "STUDIO", "MATERIAL", False
    sc.view_settings.view_transform = "Standard"
    sc.world = bpy.data.worlds.new("w")
    sc.world.color = (0.55, 0.57, 0.6)
    sh.background_type = "WORLD"
    sc.render.resolution_x, sc.render.resolution_y = 700, 900
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type, cam.data.ortho_scale = "ORTHO", 2.3
    c = Vector((0, -0.15, 0.95))
    for name, d in (("front", (0, 1, 0)), ("side", (1, 0, 0)), ("back", (0, -1, 0)), ("quarter", (0.7, 0.7, 0.35))):
        d = Vector(d).normalized()
        cam.location = c + d * 6
        cam.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
        sc.render.filepath = os.path.join(LOOK, f"fox_{tag}_{name}.png")
        bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)


def rigged(path, joints=()):
    """A scene with that file's rig, the joints added to it (name, parent,
    head, tail; world at rest, put in the rig's own space, which is scaled:
    see make_species.py) and our materials; and the file's tools."""
    rig, tools = fresh(path)
    if joints:
        to_rig = rig.matrix_world.inverted()
        bpy.context.view_layer.objects.active = rig
        bpy.ops.object.mode_set(mode="EDIT")
        for name, parent, head, tail in joints:
            b = rig.data.edit_bones.new(name)
            b.head, b.tail = to_rig @ Vector(head), to_rig @ Vector(tail)
            b.parent = rig.data.edit_bones[parent]
        bpy.ops.object.mode_set(mode="OBJECT")
    for k, (rgb, metal, rough) in EXTRA_MATS.items():
        material(k, rgb, metal, rough)
    for k, (rgb, metal, rough) in MATS.items():   # the tints are the fox's, not the robot's
        material(k, rgb, metal, rough)
    return rig, tools


def measures(body, part, eyes):
    """Where the fox wears the robot's extras (make_robot_extras.py says what
    each number is) and its styles, measured off its body at rest."""
    co = np.array([c[:] for c in body["co"]])
    head = co[[i for i, w in enumerate(part) if w == "head"]]
    low = co[[i for i, w in enumerate(part) if w not in ("head", "arm")]]
    ear_z = R_HIP[1] + (0.815 - F_HIP[1]) * S   # the ears stand above this: the skull is under it
    skull = head[head[:, 2] < ear_z + 0.14]
    crown = skull[(skull[:, 2] > ear_z - 0.02) & (abs(skull[:, 0]) < 0.07)]   # between the ears
    top = float(crown[:, 2].max()) if len(crown) else ear_z + 0.1
    band = head[(head[:, 2] > ear_z - 0.08) & (head[:, 2] < ear_z)]   # round the brow
    sx = float(abs(band[:, 0]).max())
    back = float(band[:, 1].min())
    front = float(band[abs(band[:, 0]) < 0.05][:, 1].max())
    sy = (back + front) / 2

    def layer(z, x=0.1):
        return low[(abs(low[:, 2] - z) < 0.02) & (abs(low[:, 0]) < x)]
    nz = R_HIP[1] + (F_NECK - 0.02 - F_HIP[1]) * S
    n = layer(nz)
    bz, mz = nz - 0.07, nz - 0.19
    eye = max(eyes, key=lambda e: -e.x) if eyes else Vector((-0.07, front, ear_z - 0.05))   # its left one
    chest_z = R_SHOULDER[1] - 0.12
    ring = head[(abs(head[:, 2] - eye.z) < 0.025) & (head[:, 1] < eye.y)]   # round the head behind the eyes
    return dict(
        cap=dict(c=(0.0, sy), z=top - 0.03, rx=min(sx, 0.15) + 0.01, ry=(front - back) / 2 + 0.01, h=0.09),
        hat=dict(c=(0.0, sy), z=top - 0.01, r=0.6 * (front - back) / 2, h=0.22),
        ears=dict(x=sx, y=sy, z=ear_z - 0.05, top=top + 0.03, cup=0.75, mic=0.17),
        neck=dict(y=float(n[:, 1].mean()), z=nz, rx=float(abs(n[:, 0]).max()) + 0.02,
                  ry=float(n[:, 1].max() - n[:, 1].min()) / 2 + 0.02, tube=0.034, lean=6),
        bow=dict(y=float(layer(bz)[:, 1].max()) + 0.012, z=bz, s=0.8),
        medal=dict(x=-0.09, y=float(layer(mz, 0.14)[:, 1].max()) + 0.008, z=mz, lean=0),
    ), dict(eye=eye, brow=(float(abs(ring[:, 0]).max()), float(ring[:, 1].min() + eye.y) / 2, float(eye.y - ring[:, 1].min()) / 2), back=float(layer(chest_z, 0.12)[:, 1].min()), chest_z=chest_z)


def styles(m):
    """The styles, as tools/make_species.py's are: parts on the robot's bones."""
    e = m["eye"]
    sx, sy, ry = m["brow"]
    return {
        # A patch over its left eye, on a strap round the head.
        "patch": [
            P("ball", "head", "Fox_Dark", (e.x, e.y + 0.004, e.z), (0.042, 0.014, 0.042)),
            P("torus", "head", "Fox_Dark", (0.0, sy, e.z + 0.015), (sx + 0.006, ry + 0.01, sx), (0, 12, 0), R=1.0, t=0.03),
        ],
        # A quiver across the back, three arrows in it.
        "quiver": [
            P("cyl", "chest", "Fox_Leather", (0.05, m["back"] - 0.05, m["chest_z"]), (0.05, 0.05, 0.40), (8, 24, 0)),
            P("cyl", "chest", "Fox_Dark", (0.05 + 0.2 * math.sin(math.radians(24)) * 0.98, m["back"] - 0.05 - 0.2 * math.sin(math.radians(8)),
                                           m["chest_z"] + 0.2 * 0.98), (0.054, 0.054, 0.03), (8, 24, 0)),
        ] + [
            P("cyl", "chest", "tint_2", (0.05 + 0.115 + 0.022 * k, m["back"] - 0.08 - 0.02 * (k % 2), m["chest_z"] + 0.27),
              (0.006, 0.006, 0.16), (8, 24, 0)) for k in (-1, 0, 1)
        ] + [
            P("box", "chest", "tint_3", (0.05 + 0.14 + 0.022 * k, m["back"] - 0.085 - 0.02 * (k % 2), m["chest_z"] + 0.32),
              (0.004, 0.03, 0.06), (8, 24, 0)) for k in (-1, 0, 1)
        ],
    }


def main():
    robot_path = os.path.join(GAME, "robot.gltf")
    arms_path = os.path.join(GAME, "robot_arms.gltf")
    rig, _ = fresh(robot_path)
    bones = {b.name: rig.matrix_world @ b.head_local for b in rig.data.bones}
    rest, faces, labels, part, eyes = load()
    ob, lab = thin("body", *only(rest, faces, labels, part, lambda w: w != "tail"), TRIS)
    weights, body_part = bound(ob, bones)
    body = kept(ob, lab, weights)
    ob, lab = thin("tail", *only(rest, faces, labels, part, lambda w: w == "tail"), TRIS)
    weights, tail_joints = linked(ob)
    tail = kept(ob, lab, weights)
    wears, m = measures(body, body_part, eyes)
    print("FOX wears:", {k: {q: (tuple(round(x, 3) for x in w) if isinstance(w, tuple) else round(w, 3))
                             for q, w in v.items()} for k, v in wears.items()})
    made = []

    # The body without its tail, and without clips: the client plays the
    # player model's on it (borrow_clips, src/client/main.cpp).
    rig, tools = rigged(robot_path)
    keep_tools(tools)
    tools.name = NAME + "_tools"
    export(rig, [mesh(rig, NAME, body), tools], os.path.join(OUT, NAME + ".gltf"))
    made.append(NAME)

    # The arms seen in first person, on robot_arms.gltf's rig.
    rig, tools = rigged(arms_path)
    keep_tools(tools)
    export(rig, [mesh(rig, NAME + "_arms", body, TO_ARMS, ARM_BONES), tools], os.path.join(OUT, NAME + "_arms.gltf"))
    made.append(NAME + "_arms")

    # What it always wears: the tail, on wag_* links that swing it.
    rig, tools = rigged(robot_path, tail_joints)
    bpy.data.objects.remove(tools)
    export(rig, [mesh(rig, NAME + "_worn", tail)], os.path.join(OUT, NAME + "_worn.gltf"))
    made.append(NAME + "_worn")

    # The styles, "<that>_style_<name>".
    every = styles(m)
    for style, parts in every.items():
        rig, tools = rigged(robot_path)
        bpy.data.objects.remove(tools)
        export(rig, [build(rig, f"{NAME}_style_{style}", parts)], os.path.join(OUT, f"{NAME}_style_{style}.gltf"))
        made.append(f"{NAME}_style_{style}")

    # The robot's extras, made to its measure, with their sway_* bones.
    pieces, sway = extras(wears)
    for extra, parts in pieces.items():
        rig, tools = rigged(robot_path, sway)
        bpy.data.objects.remove(tools)
        export(rig, [build(rig, f"{NAME}_{extra}", parts)], os.path.join(OUT, f"{NAME}_{extra}.gltf"))
        made.append(f"{NAME}_{extra}")

    # To judge it by: everything at rest in one scene, then styles and extras on it.
    rig, tools = rigged(robot_path, tail_joints + sway)
    bpy.data.objects.remove(tools)
    mesh(rig, "body", body)
    mesh(rig, "tail", tail)
    look("rest")
    for style, parts in every.items():
        build(rig, style, parts)
    look("styles")
    for o in [o for o in bpy.data.objects if o.name in every]:
        bpy.data.objects.remove(o)
    for group in (("cap", "scarf", "medal"), ("tophat", "headphones", "bowtie")):
        for extra in group:
            build(rig, extra, pieces[extra])
        look("extras_" + group[0])
        for o in [o for o in bpy.data.objects if o.name in group]:
            bpy.data.objects.remove(o)
    for n in made:
        size = os.path.getsize(os.path.join(OUT, n + ".bin"))
        print(f"FOX wrote {n}: {size} bytes")
        assert size < 1024 * 1024 - 4096, f"{n}: over the 1 MB a server serves"


if __name__ == "__main__":
    main()
