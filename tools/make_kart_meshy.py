# The kart parts made with Meshy: a dragon set, one variant for each of the
# garage's five slots (kart_<slot>_dragon), beside tools/make_kart_parts.py's.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_kart_meshy.py" [-- SRC_DIR]
#
# SRC_DIR (default ~/.cache/cnp_look/kart/src) holds <slot>_dragon.glb, Meshy's
# image-to-3D (meshy-6) of the concepts c_<slot>.png -- 54 MB, so kept out of the
# repository. Each one is welded, decimated, turned and fitted into the box its
# slot's primitive parts fill, on the rig make_kart_parts.py builds; every face
# takes a flat colour by the hue of Meshy's texture under it (blue, orange, grey,
# dark, black), and those become the kart's materials -- tint_1/2/3 the player
# paints, the rest as made -- so a /garage paint recolours a dragon part as it
# does the others. The texture itself is not kept. The wheel is one wheel made
# four times (the left ones mirrored), the pod one pod mirrored; the pipes get
# their own flame_1/2 joints at the horns' mouths.
#
# Renders ~/.cache/cnp_look/kart/dragon_sheet.png: the kart wearing all five, from
# four sides. Look at it before the files are kept.
import json
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_kart_parts as mk  # noqa: E402  (the rig, its bones, the materials, the export)
import make_robot_extras as rx  # noqa: E402
import meshy_prop as mp  # noqa: E402  (decimate)

SRC = os.path.expanduser("~/.cache/cnp_look/kart/src")
LOOK = os.path.expanduser("~/.cache/cnp_look/kart")
KART = os.path.join(mk.ROOT, "assets", "blender", "kart_unsplit", "track_core_kart.gltf")

# Each part: its source, the turn about Z (degrees) that puts its front to +Y,
# the box it is fitted into (min and max corner, the rig's metres: Z up, +Y the
# nose, the road at 0), "fit" uniform or "stretch" (each axis to the box), the
# bone it follows, the triangles to keep, and which material each colour becomes.
PARTS = {
    "nose": dict(turn=180, box=((-0.32, 0.27, 0.02), (0.32, 1.0, 0.36)), fit="uniform", bone="nose",
                 tris=2400, mats=dict(blue="tint_1", orange="tint_2", grey="Kart_White", dark="Kart_Soot",
                                      black="Kart_Soot")),
    "pods": dict(turn=90, box=((0.30, -0.32, 0.04), (0.52, 0.38, 0.26)), fit="stretch", bone="pods",
                 tris=1400, mirror=True, mats=dict(blue="tint_1", orange="tint_2", grey="Kart_Chrome",
                                                    dark="Kart_Soot", black="Kart_Soot")),
    # On the kart's middle, between the seat's back and the wing: "stretch", because
    # at its own proportions it is too deep for what is free there.
    "pipes": dict(turn=180, box=((-0.245, -0.695, 0.24), (0.245, -0.485, 0.62)), fit="stretch", bone="pipes",
                  tris=1800, mats=dict(blue="tint_1", orange="tint_2", grey="Kart_Chrome", dark="Kart_Soot",
                                       black="Kart_Soot")),
    # Its foot on the rear bumper's top (the bumper's middle is 0.75 back, its top 0.204 up).
    "wing": dict(turn=0, box=((-0.56, -0.887, 0.196), (0.56, -0.607, 0.776)), fit="uniform", bone="body",
                 tris=1800, mats=dict(blue="tint_1", orange="tint_2", grey="tint_3", dark="Kart_Soot",
                                      black="Kart_Soot")),
    # Not of the set: the garage's shark nose, from its own concept. Stretched to a nose's width and height.
    "nose_shark": dict(turn=180, box=((-0.26, 0.27, 0.02), (0.26, 1.0, 0.46)), fit="stretch", bone="nose",
                       tris=2400, src="nose_shark.glb", name="kart_nose_shark",
                       mats=dict(blue="tint_1", orange="tint_2", grey="Kart_White", dark="Kart_Soot",
                                 black="Kart_Soot")),
    # One wheel, its hub out to +X; placed on each wheel_* bone below.
    "wheel": dict(turn=90, box=None, fit="stretch", bone=None, tris=1100,
                  mats=dict(blue="tint_3", orange="tint_2", grey="Kart_Chrome", dark="Kart_Tire",
                            black="Kart_Tire")),
}
CLASSES = ["blue", "orange", "grey", "dark", "black"]


def load(path):
    """Meshy's glb as one mesh, its transforms applied; the rest of the scene left alone."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    mw = ob.matrix_world.copy()
    ob.parent = None
    ob.matrix_world = mw
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in new:
        if o is not ob and o.name in bpy.data.objects:
            bpy.data.objects.remove(o)
    return ob


def classify(rgb):
    """The colour class of each sRGB colour, by hue, saturation and value."""
    mx, mn = rgb.max(1), rgb.min(1)
    sat = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0)
    r, g, b = rgb.T
    hue = np.where(b >= np.maximum(r, g), 0.6, 0.05)   # blue-ish or warm: all the concepts have
    out = np.full(len(rgb), 2)
    out[(sat >= 0.3) & (hue > 0.3)] = 0
    out[(sat >= 0.3) & (hue <= 0.3)] = 1
    grey = sat < 0.3
    out[grey & (mx < 0.45)] = 3
    out[grey & (mx < 0.2)] = 4
    return out


def face_classes(ob):
    """Each face's colour class, read off its texture at the face's middle."""
    me = ob.data
    mat = me.materials[0]
    img = next(n.image for n in mat.node_tree.nodes if n.type == "TEX_IMAGE")
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, img.channels)[:, :, :3]
    uv = np.zeros(len(me.loops) * 2, dtype=np.float32)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    start = np.zeros(len(me.polygons), dtype=np.int32)
    total = np.zeros(len(me.polygons), dtype=np.int32)
    me.polygons.foreach_get("loop_start", start)
    me.polygons.foreach_get("loop_total", total)
    mid = np.add.reduceat(uv, start) / total[:, None]
    x = np.clip((mid[:, 0] % 1.0) * w, 0, w - 1).astype(int)
    y = np.clip((mid[:, 1] % 1.0) * h, 0, h - 1).astype(int)
    return classify(px[y, x])


def turned(ob, deg):
    rot = Matrix.Rotation(math.radians(deg), 4, "Z")
    ob.data.transform(rot)


def fit(ob, box, mode):
    vs = np.array([v.co[:] for v in ob.data.vertices])
    lo, hi = vs.min(0), vs.max(0)
    blo, bhi = np.array(box[0]), np.array(box[1])
    s = (bhi - blo) / (hi - lo)
    if mode == "uniform":
        s = np.full(3, s.min())
    # middle of the box in x and y, standing on its floor
    t = np.array([(blo[0] + bhi[0]) / 2, (blo[1] + bhi[1]) / 2, blo[2]])
    o = np.array([(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]])
    for v in ob.data.vertices:
        v.co = Vector((np.array(v.co[:]) - o) * s + t)


def low_with_classes(high, cls, tris):
    """A decimated copy of high whose faces carry the class of the high face nearest."""
    low = high.copy()
    low.data = high.data.copy()
    bpy.context.scene.collection.objects.link(low)
    mp.decimate(low, tris)
    tree = BVHTree.FromObject(high, bpy.context.evaluated_depsgraph_get())
    lab = np.array([cls[tree.find_nearest(p.center)[2]] for p in low.data.polygons])
    # two passes of a majority filter over the edge neighbours: no lone specks
    bm = bmesh.new()
    bm.from_mesh(low.data)
    bm.faces.ensure_lookup_table()
    for _ in range(2):
        new = lab.copy()
        for f in bm.faces:
            n = [lab[g.index] for e in f.edges for g in e.link_faces if g is not f]
            if n:
                vals, counts = np.unique(n, return_counts=True)
                if counts.max() >= 2 and vals[counts.argmax()] != lab[f.index]:
                    new[f.index] = vals[counts.argmax()]
        lab = new
    bm.free()
    bpy.data.objects.remove(high)
    return low, lab


def bake_materials(ob, lab, mats):
    me = ob.data
    me.materials.clear()
    used = []
    for c in CLASSES:
        m = mats[c]
        if m not in used:
            used.append(m)
            me.materials.append(bpy.data.materials[m])
    idx = np.array([used.index(mats[CLASSES[c]]) for c in lab], dtype=np.int32)
    me.polygons.foreach_set("material_index", idx)
    while me.uv_layers:
        me.uv_layers.remove(me.uv_layers[0])
    me.shade_smooth()
    me.set_sharp_from_angle(angle=math.radians(40))


def copy_into(bm, me, matrix, flip):
    """Appends me, transformed, to bm; returns the new verts."""
    other = bmesh.new()
    other.from_mesh(me)
    bmesh.ops.transform(other, matrix=matrix, verts=other.verts)
    if flip:
        bmesh.ops.reverse_faces(other, faces=other.faces)
    tmp = bpy.data.meshes.new("tmp")
    other.to_mesh(tmp)
    other.free()
    before = len(bm.verts)
    bm.from_mesh(tmp)
    bpy.data.meshes.remove(tmp)
    bm.verts.ensure_lookup_table()
    return bm.verts[before:]


def assemble(rig, name, pieces, mats_of):
    """One object of pieces [(mesh, matrix, flip, bone)], every vertex on its bone."""
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    groups = {g.name: g.index for g in ob.vertex_groups}
    bm = bmesh.new()
    dl = bm.verts.layers.deform.verify()
    for src, matrix, flip, bone in pieces:
        for v in copy_into(bm, src, matrix, flip):
            v[dl][groups[bone]] = 1.0
    bm.to_mesh(me)
    bm.free()
    for m in mats_of:
        me.materials.append(m)
    ob.parent = rig
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob


def make(rig, slot):
    spec = PARTS[slot]
    # Only the pipes burn: a flame_* joint in any other part is a flame the client lights there.
    mk.edit_bones(rig, [], drop=[b.name for b in rig.data.bones if b.name.startswith("flame_")])
    high = load(os.path.join(SRC, spec.get("src", f"{slot}_dragon.glb")))
    cls = face_classes(high)
    turned(high, spec["turn"])
    if spec["box"]:
        fit(high, spec["box"], spec["fit"])
    low, lab = low_with_classes(high, cls, spec["tris"])
    if slot == "wheel":   # the tread's highlights read as grey: out past the rim it is all tyre
        c = np.array([p.center[:] for p in low.data.polygons])
        mid = (c.min(0) + c.max(0)) / 2
        rad = np.hypot(c[:, 1] - mid[1], c[:, 2] - mid[2])
        lab[rad > 0.66 * rad.max()] = CLASSES.index("black")
    bake_materials(low, lab, spec["mats"])
    me, mats = low.data, list(low.data.materials)
    if slot == "wheel":
        pieces = []
        for w, ((x, y, z), r, wd) in mk.WHEELS.items():
            vs = np.array([v.co[:] for v in me.vertices])
            lo, hi = vs.min(0), vs.max(0)
            size = Vector((wd * 1.15, 2 * r, 2 * r))
            sc = Matrix.Diagonal(Vector([size[i] / (hi[i] - lo[i]) for i in range(3)]).to_4d())
            centre = Matrix.Translation(-Vector((lo + hi) / 2))
            side = Matrix.Diagonal((1 if x > 0 else -1, 1, 1, 1))
            pieces.append((me, Matrix.Translation((x, y, z)) @ side @ sc @ centre, x < 0, f"wheel_{w}"))
    else:
        pieces = [(me, Matrix.Identity(4), False, spec["bone"])]
        if spec.get("mirror"):
            pieces.append((me, Matrix.Diagonal((-1, 1, 1, 1)), True, spec["bone"]))
    bpy.data.objects.remove(low)
    ob = assemble(rig, spec.get("name", f"kart_{slot}_dragon"), pieces, mats)
    if slot == "pipes":
        horns(rig, ob)
    return ob


def horns(rig, ob):
    """flame_1 and flame_2 at the two horns' mouths, pointing up and back."""
    vs = np.array([v.co[:] for v in ob.data.vertices])
    mid = (vs[:, 0].min() + vs[:, 0].max()) / 2
    flames = []
    for i, side in enumerate((vs[:, 0] < mid, vs[:, 0] >= mid)):
        s = vs[side]
        top = s[s[:, 2] > s[:, 2].max() - 0.025]
        head = Vector(top.mean(0))
        tail = head + Vector((0, -0.6, 0.8)).normalized() * 0.15
        flames.append((f"flame_{i + 1}", "pipes", tuple(head), tuple(tail)))
    mk.edit_bones(rig, flames, drop=[b.name for b in rig.data.bones if b.name.startswith("flame_")])
    print("pipes flames:", [tuple(round(c, 3) for c in f[2]) for f in flames])


def sheet(rig, kart, obs, name="dragon_sheet"):
    for o in bpy.data.objects:
        if o.type == "MESH":
            o.hide_render = True
    for o in [kart] + obs:
        o.hide_set(False)
        o.hide_render = False
    # the kart's own nose, pods and pipes give way, as in the client
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "MATERIAL"
    sc.display.shading.show_cavity = True
    sc.render.resolution_x = sc.render.resolution_y = 512
    sc.world = bpy.data.worlds.new("w")
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.lens = 45
    shots = []
    for k, yaw in enumerate((40, 140, 220, 320)):
        a = math.radians(yaw)
        cam.location = Vector((3.2 * math.sin(a), 3.2 * math.cos(a), 1.7))
        cam.rotation_euler = (Vector((0, 0, 0.25)) - cam.location).to_track_quat("-Z", "Y").to_euler()
        p = os.path.join(LOOK, f"dragon_v{k}.png")
        sc.render.filepath = p
        bpy.ops.render.render(write_still=True)
        shots.append(p)
    big = np.zeros((1024, 1024, 4))
    for k, p in enumerate(shots):
        im = bpy.data.images.load(p)
        r, c = 1 - k // 2, k % 2
        big[r * 512:(r + 1) * 512, c * 512:(c + 1) * 512] = np.array(im.pixels[:]).reshape(512, 512, 4)
        os.remove(p)
    out = bpy.data.images.new(name, 1024, 1024)
    out.pixels = big.ravel().tolist()
    out.filepath_raw = os.path.join(LOOK, f"{name}.png")
    out.file_format = "PNG"
    out.save()
    print("sheet:", out.filepath_raw)


def strip_kart(kart):
    """The kart without the bones a dragon set replaces (nose, pods, pipes, wheel_*),
    for the sheet only -- the client leaves them out the same way."""
    groups = {g.index: g.name for g in kart.vertex_groups}
    bm = bmesh.new()
    bm.from_mesh(kart.data)
    dl = bm.verts.layers.deform.verify()
    gone = [v for v in bm.verts if v[dl] and groups[max(v[dl].items(), key=lambda t: t[1])[0]] in
            ("nose", "pods", "pipes") or (v[dl] and groups[max(v[dl].items(), key=lambda t: t[1])[0]].startswith("wheel_"))]
    bmesh.ops.delete(bm, geom=gone, context="VERTS")
    bm.to_mesh(kart.data)
    bm.free()


def main():
    global SRC
    if "--" in sys.argv and len(sys.argv) > sys.argv.index("--") + 1:
        SRC = sys.argv[sys.argv.index("--") + 1]
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=KART)
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    kart = next(o for o in bpy.data.objects if o.type == "MESH" and o.parent == rig)
    rig.data.pose_position = "REST"
    mk.edit_bones(rig, mk.BONES)
    mk.split_kart(rig, kart)
    for k, (rgb, metal, rough) in mk.MATS.items():
        rx.material(k, rgb, metal, rough)
    for old, new in mk.RENAME.items():
        kart.data.materials[kart.data.materials.find(old)] = bpy.data.materials[new]
    for gm in json.load(open(KART)).get("materials", []):   # the sheet is drawn in the viewport's colours
        c = gm.get("pbrMetallicRoughness", {}).get("baseColorFactor")
        if c and gm["name"] in bpy.data.materials and gm["name"] not in mk.RENAME:
            bpy.data.materials[gm["name"]].diffuse_color = c
    kart.hide_set(True)

    obs = []
    for slot in PARTS:
        ob = make(rig, slot)
        name = PARTS[slot].get("name", f"kart_{slot}_dragon")
        mk.export(rig, ob, name)
        print(f"{name}: {len(ob.data.polygons)} triangles")
        ob.hide_set(True)
        obs.append(ob)
    strip_kart(kart)
    dragon = [o for o in obs if o.name != "kart_nose_shark"]
    sheet(rig, kart, dragon)
    sheet(rig, kart, [o for o in obs if o.name != "kart_nose_dragon"], "shark_sheet")


main()
