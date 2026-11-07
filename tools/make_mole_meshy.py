# The mole-man remade from a Meshy model (assets/meshy/mole/mole_meshy.glb,
# from assets/meshy/mole/concept.png) instead of tools/make_species.py's
# primitives, on the same rig and with the same clips:
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_mole_meshy.py"
#
# Meshy's mole stands in an A-pose with short legs; its torso and head already
# sit where the rig's are once it is scaled by SCALE about its shoulders, so
# they stay as they are, while each arm is turned down onto its bones and
# lengthened, and each leg lengthened down to the ground (WARP). Every vertex
# then takes the bones of its own part (a torso's never a thigh's), blurred
# so the joints bend. The texture is read once, to give each face the flat
# material its colour is nearest (COLOURS): denim, shirt and patch become
# tint_1..3, the player's dye, as on every other body.
#
# Writes mods/robot/robot_mole.gltf (on its own rig and 71 clips, the tools
# kept) and robot_mole_arms.gltf (the arms seen in first person, on
# robot_mole_arms.gltf's rig).
import json
import math
import os
import shutil
import subprocess
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_species import MOLE_TINTS, TO_ARMS, blurred, export, fresh, keep_tools, materials  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "robot")
SRC = os.path.join(ROOT, "assets", "meshy", "mole", "mole_meshy.glb")
PROBE = "--probe" in sys.argv   # renders only, to ~/.cache/cnp_look

# Meshy's model, faced to +Y and scaled so its shoulders are the rig's.
SCALE, LIFT = 0.625, 1.078
# On it (after SCALE): where its shoulders and wrists are, the belly's
# underside where the legs come out, and the cuffs, under which a leg is kept
# as made.
SHOULDER = Vector((0.265, 0.0, 1.234))
WRIST = Vector((0.50, 0.0, 0.997))
CROTCH, CUFF, GROUND = 0.655, 0.55, 0.484
# On the rig: the arm hangs from SHOULDER straight down ARM long; a leg's
# trouser runs from CROTCH down to KNEE_TOP above the ground.
ARM, KNEE_TOP = 1.234 - 0.663, CUFF - GROUND
ARM_OUT = 0.06      # an arm drawn this far out from its bone, clear of the belly
LEG_GIRTH = 1.3     # a stretched trouser leg made this much thicker

# Flat colours the texture is read into (sRGB, the texture's own, k-means of
# it): the nearest wins.
COLOURS = [
    ("Fur", (0.10, 0.09, 0.13)), ("Fur", (0.15, 0.13, 0.17)),
    ("Ivory", (0.74, 0.69, 0.65)), ("Ivory", (0.87, 0.81, 0.73)),   # a claw on a paw, the shirt elsewhere
    ("Pink", (0.89, 0.62, 0.63)), ("Pink", (0.83, 0.58, 0.58)),
    ("tint_1", (0.21, 0.34, 0.46)), ("tint_1", (0.19, 0.31, 0.43)), ("tint_1", (0.25, 0.38, 0.49)),
    ("tint_1", (0.39, 0.48, 0.57)),
    ("tint_3", (0.62, 0.18, 0.14)),
    ("Gold", (0.50, 0.41, 0.32)),
]


def load():
    """Meshy's mesh, its texture read into per-face colours, in rig space."""
    bpy.ops.import_scene.gltf(filepath=SRC)
    ob = next(o for o in bpy.context.selected_objects if o.type == "MESH")
    me = ob.data
    img = next(n.image for n in me.materials[0].node_tree.nodes if n.type == "TEX_IMAGE")
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]
    uv = me.uv_layers.active.data
    cols = []
    for f in me.polygons:   # the corners and the middle, averaged
        pts = [uv[i].uv for i in f.loop_indices]
        pts.append(sum(pts, Vector((0, 0))) / len(pts))
        cols.append(np.mean([px[min(h - 1, int(p.y * h)), min(w - 1, int(p.x * w))] for p in pts], axis=0))
    mw = ob.matrix_world.copy()
    for v in me.vertices:   # faced to +Y: x and y turned over
        p = mw @ v.co
        v.co = Vector((-p.x * SCALE, -p.y * SCALE, p.z * SCALE + LIFT))
    ob.matrix_world.identity()
    me.materials.clear()
    me.uv_layers.remove(me.uv_layers.active)
    # Split at the texture's seams, which the warp would open: made whole.
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.to_mesh(me)
    bm.free()
    assert len(me.polygons) == len(cols)
    return ob, np.array(cols)


def part(p):
    """Which part a point of the made model is: torso, arm_L/R or leg_L/R."""
    side = "L" if p.x < 0 else "R"   # facing +Y, the left is -x
    if p.z < CROTCH:
        return "leg_" + side
    a = Vector((-SHOULDER.x if side == "L" else SHOULDER.x, 0, SHOULDER.z))
    d = Vector((-WRIST.x if side == "L" else WRIST.x, 0, WRIST.z)) - a
    t = (p - a).dot(d) / d.length_squared
    radial = ((p - a) - d * t).length
    if (t > 0.08 and radial < 0.13) or (t > 0.9 and radial < 0.22):
        return "arm_" + side
    return "torso"


def warp(p, where, leg_mid):
    """Where a point of the made model goes on the rig."""
    if where == "torso":
        return p.copy()
    side = where[-1]
    sx = -1 if side == "L" else 1
    if where.startswith("arm"):
        a = Vector((sx * SHOULDER.x, 0, SHOULDER.z))
        d = Vector((sx * WRIST.x, 0, WRIST.z)) - a
        length = d.length
        d.normalize()
        turn = d.rotation_difference(Vector((0, 0, -1)))
        along = (p - a).dot(d)
        rest = turn @ ((p - a) - d * along)
        t = along / length
        down = t * ARM if t <= 1 else ARM + (t - 1) * length   # the paw kept as made
        out = ARM_OUT * min(1.0, max(0.0, t / 0.3))
        return a + rest + Vector((sx * out, 0, -down))
    mid = leg_mid[side]
    if p.z < CUFF:   # a foot and its cuff, as made, put on the ground
        z, girth = p.z - GROUND, LEG_GIRTH
    else:
        u = (p.z - CUFF) / (CROTCH - CUFF)
        z = KNEE_TOP + u * (CROTCH - KNEE_TOP)
        girth = LEG_GIRTH + (1.0 - LEG_GIRTH) * max(0.0, (u - 0.6) / 0.4)
    off = Vector((p.x - mid.x, p.y - mid.y, 0)) * girth
    return Vector((mid.x + off.x, mid.y + off.y, z))


def neighbours(me):
    near = [[] for _ in me.vertices]
    for e in me.edges:
        a, b = e.vertices
        near[a].append(b)
        near[b].append(a)
    return near


def segment(co, rig, bone):
    """How far `co` (world) is from the bone, head to tail."""
    b = rig.data.bones[bone]
    h, t = rig.matrix_world @ b.head_local, rig.matrix_world @ b.tail_local
    d = t - h
    s = max(0.0, min(1.0, (co - h).dot(d) / d.length_squared)) if d.length_squared else 0.0
    return (co - (h + d * s)).length


BONES = {
    "torso": ["hips", "spine", "chest", "neck", "head"],
    "arm_L": ["upper_arm_L", "forearm_L", "hand_L"], "arm_R": ["upper_arm_R", "forearm_R", "hand_R"],
    "leg_L": ["thigh_L", "shin_L", "foot_L"], "leg_R": ["thigh_R", "shin_R", "foot_R"],
}


def body(rig):
    """The Meshy mole, warped onto the rig, coloured and bound."""
    ob, cols = load()
    me = ob.data
    where = [part(v.co) for v in me.vertices]
    leg_mid = {}
    for side in "LR":
        ps = [v.co for v, w in zip(me.vertices, where) if w == "leg_" + side and CUFF < v.co.z < CROTCH]
        leg_mid[side] = sum(ps, Vector()) / len(ps)
    # The warp, its seams eased: each vertex's move averaged with its
    # neighbours' a few times, which leaves a part moved whole unchanged.
    moves = [warp(v.co, w, leg_mid) - v.co for v, w in zip(me.vertices, where)]
    near = neighbours(me)
    for _ in range(6):
        moves = [m * 0.5 + sum((moves[j] for j in near[i]), Vector()) * (0.5 / max(1, len(near[i])))
                 for i, m in enumerate(moves)]
    for v, m in zip(me.vertices, moves):
        v.co += m

    # A flat material for each face, the commonest among its neighbours' kept
    # (a speck of another colour is lost), ivory a claw on a paw or foot.
    names = [k for k, _ in COLOURS]
    ref = np.array([c for _, c in COLOURS])
    dist = ((cols[:, None, :] - ref[None]) ** 2).sum(-1)
    # Where a colour may be; elsewhere it is a blend at an edge: brass only
    # on the buttons, the patch only low on the dungarees, the head all fur
    # and nose.
    for f in me.polygons:
        torso = all(where[v] == "torso" for v in f.vertices)
        z = f.center.z
        banned = set()
        if not (torso and 0.95 < z < 1.25 and f.center.y > 0.05):
            banned.add("Gold")
        if z > 1.0:
            banned.add("tint_3")
        if torso and z > 1.36:
            banned |= {"tint_1", "Ivory"}
        if any(where[v].startswith("arm") for v in f.vertices):
            banned |= {"tint_1", "tint_3"}   # an arm is sleeve, fur and paw
        dist[f.index, [i for i, k in enumerate(names) if k in banned]] = np.inf
    label = [names[i] for i in np.argmin(dist, axis=1)]
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
    used = []
    for f, lab in zip(me.polygons, label):
        if lab == "Ivory":
            parts = {where[v] for v in f.vertices}
            low = f.center.z < 0.15
            lab = "Claw" if low or any(p.startswith("arm") for p in parts) and f.center.z < 0.8 else "tint_2"
        if lab not in used:
            used.append(lab)
        f.material_index = used.index(lab)
        f.use_smooth = True
    for m in used:
        me.materials.append(bpy.data.materials[m])

    # The bones: each vertex its own part's nearest, blurred across joints.
    weights = [{min(BONES[w], key=lambda b: segment(v.co, rig, b)): 1.0} for v, w in zip(me.vertices, where)]
    weights = blurred(me, weights, passes=6)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    for i, w in enumerate(weights):
        for bone, x in w.items():
            ob.vertex_groups[bone].add([i], x, "REPLACE")
    ob.name = "robot_mole"
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob, where


def look(path):
    """Front, side and back renders of what is in the scene."""
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.color_type = "MATERIAL"
    sc.render.resolution_x, sc.render.resolution_y = 600, 700
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type, cam.data.ortho_scale = "ORTHO", 2.1
    for name, loc, rot in (("front", (0, 5, 0.9), (math.pi / 2, 0, math.pi)),
                           ("side", (5, 0, 0.9), (math.pi / 2, 0, math.pi / 2)),
                           ("back", (0, -5, 0.9), (math.pi / 2, 0, 0))):
        cam.location, cam.rotation_euler = loc, rot
        sc.render.filepath = f"{path}_{name}.png"
        bpy.ops.render.render(write_still=True)


def with_clips(name, build):
    """`build` writes mods/robot/<name>.gltf without clips; the clips the file
    had are put back as they were (tools/gltf_add_clips.py), not re-exported
    from Blender, which writes half again as many channels."""
    path = os.path.join(OUT, name + ".gltf")
    keep = os.path.join(bpy.app.tempdir, "clips")
    os.makedirs(keep, exist_ok=True)
    for ext in (".gltf", ".bin"):
        shutil.copy(path[:-5] + ext, os.path.join(keep, name + ext))
    with open(path) as f:
        clips = [a["name"] for a in json.load(f)["animations"]]
    build(path)
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "gltf_add_clips.py"), path,
                    os.path.join(keep, name + ".gltf"), *clips], check=True)
    for f in (path, path[:-5] + ".bin"):
        assert os.path.getsize(f) < 1024 * 1024 - 4096, f"{f}: over the 1 MB a server serves"


def arm_part(ob, where):
    """The arms of the bound body, as plain data: they outlive the scene."""
    me = ob.data
    keep = {v.index for v in me.vertices if where[v.index].startswith("arm")}
    faces = [f for f in me.polygons if all(v in keep for v in f.vertices)]
    used = sorted({v for f in faces for v in f.vertices})
    index = {v: i for i, v in enumerate(used)}
    names = {g.index: g.name for g in ob.vertex_groups}
    return {
        "co": [me.vertices[v].co.copy() for v in used],
        "w": [{names[g.group]: g.weight for g in me.vertices[v].groups} for v in used],
        "faces": [[index[v] for v in f.vertices] for f in faces],
        "mat": [me.materials[f.material_index].name for f in faces],
    }


def arms(rig, data):
    """The arms on robot_mole_arms.gltf's rig: moved by TO_ARMS, onto its bones."""
    me = bpy.data.meshes.new("robot_mole_arms")
    me.from_pydata([c + TO_ARMS for c in data["co"]], [], data["faces"])
    used = list(dict.fromkeys(data["mat"]))
    for m in used:
        me.materials.append(bpy.data.materials[m])
    for f, m in zip(me.polygons, data["mat"]):
        f.material_index = used.index(m)
        f.use_smooth = True
    ob = bpy.data.objects.new("robot_mole_arms", me)
    bpy.context.scene.collection.objects.link(ob)
    bones = {b.name for b in rig.data.bones}
    for b in bones:
        ob.vertex_groups.new(name=b)
    for i, w in enumerate(data["w"]):   # a shoulder's chest weight has no bone here
        w = {k: x for k, x in w.items() if k in bones}
        total = sum(w.values())
        for k, x in w.items():
            ob.vertex_groups[k].add([i], x / total, "REPLACE")
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob


def main():
    rig, mesh = fresh(os.path.join(OUT, "robot_mole.gltf"))
    materials(MOLE_TINTS)
    keep_tools(mesh)
    mesh.name = "robot_mole_tools"
    ob, where = body(rig)
    if PROBE:
        look(os.path.expanduser("~/.cache/cnp_look/mole"))
        return
    data = arm_part(ob, where)
    with_clips("robot_mole", lambda path: export(rig, [ob, mesh], path, False))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, "assets", "blender", "robot_mole.blend"))

    rig, mesh = fresh(os.path.join(OUT, "robot_mole_arms.gltf"))
    materials(MOLE_TINTS)
    keep_tools(mesh)
    with_clips("robot_mole_arms", lambda path: export(rig, [arms(rig, data), mesh], path, False))


if __name__ == "__main__":
    main()
