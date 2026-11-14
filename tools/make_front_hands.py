# The first-person arms that carry guns -- the front's and the assault's
# (tools/make_front_models.py), yafps's and the lab's (tools/make_fps_models.py)
# -- with their clips made again for arms that end in hands.
#
#   flatpak run org.blender.Blender --background --python tools/make_front_hands.py
#
# (Blender for its Python's numpy only; python3 runs it where numpy is.)
#
# The robot's fist is a ball on its wrist, so its clips only ask the wrist to
# be where the gun's grip is: the forearm comes in from the side, square to
# the barrel, and the wrist is turned 100 degrees and more to it. On a
# species' arms -- a hand that goes on from the forearm, a palm, fingers --
# that is a hand folded back on its arm; and any turn at all at a wrist
# shows, where a species' skin has no vertex shared between hand and forearm
# (the human's, the lizard's): the ring between them is sheared. So
# robot_arms_hands is the same file with the arm bones of every clip that
# holds something worked out again, frame by frame, from where the gun is in
# the robot's clip, as one stiff piece from elbow to knuckles:
#
#   the right hand  turned as the gun (HAND_R), its grip point (GRIP_AT along
#                   the hand, where the sandbox's rig holds a tool) on the
#                   middle of the gun's grip: along the barrel, the palm on
#                   the grip's side, the thumb up;
#   the left hand   its grip point where the robot's left fist is, turned
#                   under the gun (HAND_L) while that is at the gun, and the
#                   same way to the view while it is away (a magazine
#                   fetched, a pistol held in one hand);
#   the forearms    straight on from the hands, no turn at the wrists at
#                   all; the elbows hinges bent by ELBOW; the shoulders
#                   wherever that puts them, which is out of the view.
#
# wrist_R is hand_R again here, as in the sandbox's arms; the robot's own arms'
# mesh is left out (only the guns are drawn out of this file), and the gun (the
# bone "gun" under it) and the shells in the left fist are given the moves
# that keep them where the robot's clips have them. The client moves every
# species' arms with these clips and draws the guns out of this file, in a
# world that has it beside its arms (draw_arms in src/client/main.cpp:
# "<player model>_arms_hands"); the robot keeps robot_arms.
#
# Run again after tools/make_front_models.py or tools/make_fps_models.py. The
# assault's file is a link to the front's, the lab's to yafps's.
import json
import math
import os

import numpy as np

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
# Every robot_arms that carries guns: the front's (the assault's is a link to
# it) and yafps's (the lab's is a link to that). A file's guns are its tool_*
# bones.
WHERE = [os.path.join(ROOT, "games", "front", "mods", "front_core"),
         os.path.join(ROOT, "games", "yafps", "mods", "yafps_core")]
K = 1.26                              # the guns' size in the hand (tools/make_fps_models.py)
GRIP = (0.0, -0.01, -0.055)           # the middle of a gun's grip, in the gun's frame
GRIP_AT = 0.046                       # the point a hand holds by, along its bone (tool_* in the sandbox's arms)

# A hand's own frame, as the sandbox's rig has it: y along the hand to the
# knuckles, z to the thumb's side, the right palm facing -x and the left +x.
# The gun's: x right, y along the barrel, z up.
def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])


def hand(along, palm):
    """A hand's turn in the gun's frame: its length `along`, its x as near
    `palm` as that leaves."""
    y = np.array(along, dtype=float)
    y /= np.linalg.norm(y)
    x = np.array(palm, dtype=float)
    x -= (x @ y) * y
    x /= np.linalg.norm(x)
    return np.column_stack([x, y, np.cross(x, y)])


# The right hand in the gun's frame: along the barrel, a little up and in,
# the palm to the left, on the grip. Its forearm goes straight on behind it,
# back and down to the right of the view.
HAND_R = hand((-0.15, 0.9, 0.35), (1, 0, 0))
# The left under the gun: forward, up and to the right, the palm up and to
# the right, the gun lying in it; its forearm comes up from the bottom left.
HAND_L = hand((0.35, 0.75, 0.5), (1, 0, 1))
# The gun's frame for a left hand away from the gun, in the eye's space (x
# right, y up, -z ahead): straight ahead.
AHEAD = np.array([[1.0, 0, 0], [0, 0, 1], [0, -1, 0]])
ELBOW = (math.radians(40), math.radians(25))   # how far the elbows are bent, right and left: the upper arms
                                               # go on back out of the view, not across its bottom
MAX_FILE = 1024 * 1024                # kMaxMediaBytes (src/core/media/media_limits.h): a bigger file is not served
ON_GUN = (0.11, 0.24)                 # the left fist this near the gun's line is on it; this far, off


def quat_matrix(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def matrix_quat(m):
    t = m[0, 0] + m[1, 1] + m[2, 2]
    if t > 0:
        s = math.sqrt(t + 1.0) * 2
        q = ((m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s, 0.25 * s)
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = math.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
        q = (0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s, (m[2, 1] - m[1, 2]) / s)
    elif m[1, 1] > m[2, 2]:
        s = math.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
        q = ((m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s, (m[0, 2] - m[2, 0]) / s)
    else:
        s = math.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
        q = ((m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s, (m[1, 0] - m[0, 1]) / s)
    q = np.array(q)
    return q / np.linalg.norm(q)


def unit(v):
    return v / np.linalg.norm(v)


def slerp_matrix(a, b, u):
    """From turn a to turn b, u of the way."""
    qa, qb = matrix_quat(a), matrix_quat(b)
    if qa @ qb < 0:
        qb = -qb
    q = qa * (1 - u) + qb * u   # near enough between hands' turns, and normalised
    return quat_matrix(q / np.linalg.norm(q))


def smooth(lo, hi, x):
    u = min(1.0, max(0.0, (x - lo) / (hi - lo)))
    return u * u * (3 - 2 * u)


def main(HERE):
    SRC, OUT = os.path.join(HERE, "robot_arms.gltf"), os.path.join(HERE, "robot_arms_hands.gltf")
    g = json.load(open(SRC))
    data = bytearray(open(os.path.join(HERE, g["buffers"][0]["uri"]), "rb").read())
    nodes = g["nodes"]
    index = {n["name"]: i for i, n in enumerate(nodes)}
    GUN_NAMES = [n["name"][5:] for n in nodes if n["name"].startswith("tool_")]
    parent = {c: i for i, n in enumerate(nodes) for c in n.get("children", [])}

    def read(i):
        a = g["accessors"][i]
        view = g["bufferViews"][a["bufferView"]]
        n = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}[a["type"]]
        at = view.get("byteOffset", 0) + a.get("byteOffset", 0)
        return np.frombuffer(bytes(data), dtype="<f4", count=n * a["count"], offset=at).reshape(a["count"], n)

    def write(rows):
        rows = np.asarray(rows, dtype="<f4")
        while len(data) % 4:
            data.append(0)
        g["bufferViews"].append({"buffer": 0, "byteOffset": len(data), "byteLength": rows.size * 4})
        data.extend(rows.tobytes())
        g["accessors"].append({"bufferView": len(g["bufferViews"]) - 1, "componentType": 5126,
                               "count": len(rows), "type": "VEC3" if rows.shape[1] == 3 else "VEC4"})
        return len(g["accessors"]) - 1

    def local(i, pose):
        n = nodes[i]
        m = np.eye(4)
        m[:3, :3] = quat_matrix(pose.get((i, "rotation"), n.get("rotation", (0, 0, 0, 1)))) @ np.diag(
            pose.get((i, "scale"), n.get("scale", (1, 1, 1))))
        m[:3, 3] = pose.get((i, "translation"), n.get("translation", (0, 0, 0)))
        return m

    def world(name, pose):
        i = index[name]
        m = local(i, pose)
        while i in parent:
            i = parent[i]
            m = local(i, pose) @ m
        return m

    size = np.linalg.norm(world("fp_root", {})[:3, 0])               # the arms' size in the eye's space
    upper = np.linalg.norm(nodes[index["forearm_R"]]["translation"]) * size
    fore = np.linalg.norm(nodes[index["wrist_R"]]["translation"]) * size
    grip_at = np.array([0, GRIP_AT, 0]) * size
    gun_grip = np.array(GRIP) * K          # in the eye's space's size: the gun bone has the armature's

    def arm(wrist, hand_w, bent):
        """A shoulder and the two bones' turns for a hand turned so with its
        wrist there: the forearm the hand's own turn, the elbow a hinge about
        their x bent by ELBOW. Nothing turns at the wrist, and the shoulder
        is wherever that leaves it -- out of the view."""
        upper_w = hand_w @ rot_x(-bent)
        elbow = wrist - hand_w[:, 1] * fore
        return elbow - upper_w[:, 1] * upper, upper_w, hand_w

    hand_r, upper_r, fore_r = index["wrist_R"], index["upper_arm_R"], index["forearm_R"]
    hand_l, upper_l, fore_l = index["hand_L"], index["upper_arm_L"], index["forearm_L"]
    gun, shells = index["gun"], index["shells"]
    made = 0
    for clip in g["animations"]:
        if not any(clip["name"].endswith(n) or f"_{n}_" in clip["name"] for n in GUN_NAMES):
            continue   # nothing in the fist: the robot's own moves will do
        chans = {(c["target"]["node"], c["target"]["path"]): c for c in clip["channels"]}
        keys = {k: read(clip["samplers"][c["sampler"]]["output"]) for k, c in chans.items()}
        times = clip["samplers"][chans[(hand_r, "rotation")]["sampler"]]["input"]
        count = g["accessors"][times]["count"]
        assert all(len(v) == count for v in keys.values()), clip["name"]
        new = {(b, p): [] for b in (upper_r, upper_l) for p in ("translation", "rotation")}
        for b in (fore_r, fore_l, hand_r, hand_l):
            new[(b, "rotation")] = []
        for b in (gun, shells):
            new[(b, "translation")], new[(b, "rotation")] = [], []
        for k in range(count):
            pose = {key: rows[k] for key, rows in keys.items()}
            root = world("fp_root", pose)
            turn = root[:3, :3] / size
            g_w = world("gun", pose)
            g_r = g_w[:3, :3] / np.linalg.norm(g_w[:3, 0])
            shells_w = world("shells", pose)

            # The right hand on the grip.
            hr = g_r @ HAND_R
            wrist = g_w[:3, 3] + g_r @ gun_grip - hr @ grip_at
            s_r, ur, fr = arm(wrist, hr, ELBOW[0])

            # The left where the robot's fist is, under the gun while it is at it.
            l_w = world("hand_L", pose)
            l_r = l_w[:3, :3] / np.linalg.norm(l_w[:3, 0])
            fist = l_w[:3, 3] + l_r @ grip_at
            rel = (g_r.T @ (fist - g_w[:3, 3])) / K
            off = math.hypot(rel[0], rel[2], rel[1] - min(0.9, max(-0.5, rel[1])))
            on = 1.0 - smooth(ON_GUN[0], ON_GUN[1], off * K)
            hl = slerp_matrix(AHEAD @ HAND_L, g_r @ HAND_L, on)
            wrist_l = fist - hl @ grip_at
            s_l, ul, fl = arm(wrist_l, hl, ELBOW[1])

            for up, fo, ha, s, u_w, f_w, h_w in ((upper_r, fore_r, hand_r, s_r, ur, fr, hr),
                                                (upper_l, fore_l, hand_l, s_l, ul, fl, hl)):
                new[(up, "translation")].append(np.linalg.inv(root) @ np.append(s, 1.0))
                new[(up, "rotation")].append(matrix_quat(turn.T @ u_w))
                new[(fo, "rotation")].append(matrix_quat(u_w.T @ f_w))
                new[(ha, "rotation")].append(matrix_quat(f_w.T @ h_w))
            # What the hands carry, where it was.
            for bone, was, h_w, at in ((gun, g_w, hr, wrist), (shells, shells_w, hl, wrist_l)):
                new[(bone, "translation")].append(h_w.T @ (was[:3, 3] - at) / size)
                new[(bone, "rotation")].append(matrix_quat(h_w.T @ was[:3, :3] / np.linalg.norm(was[:3, 0])))
        for (bone, path), rows in new.items():
            rows = np.array(rows)[:, :3] if path == "translation" else np.array(rows)
            if path == "rotation":           # the short way from each key to the next
                for k in range(1, len(rows)):
                    if rows[k] @ rows[k - 1] < 0:
                        rows[k] = -rows[k]
            clip["samplers"].append({"input": times, "interpolation": "LINEAR", "output": write(rows)})
            if (bone, path) in chans:
                chans[(bone, path)]["sampler"] = len(clip["samplers"]) - 1
            else:
                clip["channels"].append({"sampler": len(clip["samplers"]) - 1,
                                         "target": {"node": bone, "path": path}})
        made += 1

    nodes[hand_r]["name"] = "hand_R"

    # Only what is still used goes into the new buffer -- the keys given up
    # would take the file over what a server hands out (MAX_FILE).
    for clip in g["animations"]:
        used = sorted({c["sampler"] for c in clip["channels"]})
        clip["samplers"] = [clip["samplers"][i] for i in used]
        for c in clip["channels"]:
            c["sampler"] = used.index(c["sampler"])
    # The robot's own arms are never drawn out of this file: their mesh goes
    # (the first, with tint_1 in it), and the guns and the shells stay.
    body = next(i for i, m in enumerate(g["meshes"])
                if any(g["materials"][pr["material"]]["name"] == "tint_1" for pr in m["primitives"]))
    for n in nodes:
        if n.get("mesh") == body:
            del n["mesh"]
            n.pop("skin", None)
        elif n.get("mesh", -1) > body:
            n["mesh"] -= 1
    del g["meshes"][body]
    keep = set()
    for m in g["meshes"]:
        for prim in m["primitives"]:
            keep.update(prim["attributes"].values())
            keep.update(v for t in prim.get("targets", []) for v in t.values())
            if "indices" in prim:
                keep.add(prim["indices"])
    keep.update(sk["inverseBindMatrices"] for sk in g["skins"] if "inverseBindMatrices" in sk)
    for clip in g["animations"]:
        for smp in clip["samplers"]:
            keep.update((smp["input"], smp["output"]))
    assert not g.get("images"), "an image in the buffer: not kept"
    packed, views, number = bytearray(), [], {}
    for i in sorted(keep):
        acc = g["accessors"][i]
        view = g["bufferViews"][acc["bufferView"]]
        assert "byteStride" not in view and "sparse" not in acc
        width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[acc["type"]]
        long = acc["count"] * width * {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}[acc["componentType"]]
        at = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
        while len(packed) % 4:
            packed.append(0)
        new_view = {"buffer": 0, "byteOffset": len(packed), "byteLength": long}
        if "target" in view:
            new_view["target"] = view["target"]
        packed.extend(data[at:at + long])
        views.append(new_view)
        number[i] = len(number)
        acc = dict(acc, bufferView=len(views) - 1)
        acc.pop("byteOffset", None)
        g["accessors"][i] = acc
    g["accessors"] = [g["accessors"][i] for i in sorted(keep)]
    g["bufferViews"] = views
    for m in g["meshes"]:
        for prim in m["primitives"]:
            prim["attributes"] = {k: number[v] for k, v in prim["attributes"].items()}
            for t in prim.get("targets", []):
                for k in t:
                    t[k] = number[t[k]]
            if "indices" in prim:
                prim["indices"] = number[prim["indices"]]
    for sk in g["skins"]:
        if "inverseBindMatrices" in sk:
            sk["inverseBindMatrices"] = number[sk["inverseBindMatrices"]]
    for clip in g["animations"]:
        for smp in clip["samplers"]:
            smp["input"], smp["output"] = number[smp["input"]], number[smp["output"]]
    data = packed
    assert len(data) <= MAX_FILE, f"{len(data)} bytes: more than a server hands out"
    g["buffers"][0] = {"byteLength": len(data), "uri": "robot_arms_hands.bin"}
    json.dump(g, open(OUT, "w"), separators=(",", ":"))
    open(OUT[:-5] + ".bin", "wb").write(bytes(data))
    print(f"front hands: {made} clips made again into {os.path.relpath(OUT, ROOT)}, "
          f"{len(data)} bytes, no turn at a wrist")


for where in WHERE:
    main(where)
