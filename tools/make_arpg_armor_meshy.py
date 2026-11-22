# What a character of games/arpg wears -- helms, armour, gloves, boots, belts
# and the amulet -- remade from Meshy, on the robot and on each species:
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_arpg_armor_meshy.py" [-- base ...]
#
# Five rows of the inventory's painted pieces (tools/make_arpg_armor_strips.py,
# assets/meshy/arpg/strips/<row>.png) were each made one model by Meshy's
# image-to-3D in smart topology (assets/meshy/arpg/<row>.glb). Each is cut into
# its pieces by where they stand across the row (strips/spans.json), a pair of
# gloves or boots into its two halves; a piece's faces go to a copy, and a low
# one of it is decimated, unwrapped and its colour baked (tools/meshy_prop.py),
# as the held gear of tools/make_arpg_gear_meshy.py.
#
# Each piece is then fitted onto make_arpg_gear.py's own piece of that base for
# that body (assets/meshy/arpg/fit_ref/gear_<base>[_<species>].gltf, kept as it
# was): turned to face +Y, stretched each way to the old piece's box -- a
# glove or a boot onto the old one's half on its side -- and each vertex bound
# to the bones of the old piece's vertex nearest it. So it sits where the old
# one sat, on every body the old one was fitted to, and bends as it did.
# Writes games/arpg/mods/arpg_core/gear_<base>[_<species>].gltf + .bin, and
# gear_<base>.png beside them. A look: ~/.cache/cnp_look/armor_meshy_sheet.png.
import json
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from meshy_prop import bake, decimate, texture  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "assets", "meshy", "arpg")
REF = os.path.join(SRC, "fit_ref")
OUT = os.path.join(ROOT, "games", "arpg", "mods", "arpg_core")
LOOK = os.path.expanduser("~/.cache/cnp_look")
TRIS, TEX, COLOURS = 2200, 256, 48
# How much wider (x, y) than the old piece's box a piece is fitted to: a belt
# round the robot's hips was hidden under its pelvis, the old box being the
# hips' own.
WIDER = {"sash": 1.25, "belt": 1.25, "heavy_belt": 1.3}
PAIRS = {"leather_gloves", "chain_gloves", "gauntlets", "boots", "chain_boots", "greaves"}
BODIES = ("", "_human", "_mole", "_frog")


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


def take(ob, keep, name):
    """A copy of ob's faces whose middle `keep` says yes to."""
    me = ob.data.copy()
    cp = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(cp)
    bm = bmesh.new()
    bm.from_mesh(me)
    gone = [f for f in bm.faces if not keep(f.calc_center_median())]
    bmesh.ops.delete(bm, geom=gone, context="FACES")
    bm.to_mesh(me)
    bm.free()
    return cp


def ranges(ob, spans):
    """A range of x per piece of a row: the cuts at the middles of the gaps,
    mapped from the picture's pixels onto the model's x."""
    xs = np.array([v.co.x for v in ob.data.vertices])
    lo, hi = xs.min(), xs.max()
    p0, p1 = spans[0][0], spans[-1][1]
    at = lambda px: lo + (px - p0) / (p1 - p0) * (hi - lo)
    cuts = [at((a[1] + b[0]) / 2) for a, b in zip(spans, spans[1:])]
    bounds = [lo - 1] + cuts + [hi + 1]
    return list(zip(bounds, bounds[1:]))


def old_piece(base, body):
    """The old piece's vertices at rest (n x 3), for each its bones and weights,
    and the old piece itself as one mesh in world space (to wrap onto)."""
    objs = import_glb(os.path.join(REF, f"gear_{base}{body}.gltf"))
    pts, groups = [], []
    me = bpy.data.meshes.new("old")
    bm = bmesh.new()
    for o in objs:
        if o.type != "MESH" or not o.vertex_groups:
            continue
        names = {g.index: g.name for g in o.vertex_groups}
        for v in o.data.vertices:
            pts.append(tuple(o.matrix_world @ v.co))
            groups.append([(names[g.group], g.weight) for g in v.groups if g.weight > 0.01])
        part = o.data.copy()
        part.transform(o.matrix_world)
        bm.from_mesh(part)
        bpy.data.meshes.remove(part)
    for o in objs:
        bpy.data.objects.remove(o, do_unlink=True)
    bm.to_mesh(me)
    bm.free()
    target = bpy.data.objects.new("old", me)
    bpy.context.scene.collection.objects.link(target)
    return np.array(pts), groups, target


def fit_box(src_lo, src_hi, dst_lo, dst_hi):
    """Each axis of one box onto the other's."""
    s = [(dst_hi[i] - dst_lo[i]) / max(1e-6, src_hi[i] - src_lo[i]) for i in range(3)]
    return lambda p: np.array([dst_lo[i] + (p[i] - src_lo[i]) * s[i] for i in range(3)])


def worn(rig, low, base, body):
    """A copy of the baked low piece fitted onto the old piece for `body`, bound."""
    old, groups, target = old_piece(base, body)
    ob = bpy.data.objects.new(f"gear_{base}{body}", low.data.copy())
    bpy.context.scene.collection.objects.link(ob)
    vs = np.array([v.co[:] for v in ob.data.vertices])
    # Faced forward: Meshy's front is -Y, a body's +Y.
    vs[:, 0] *= -1
    vs[:, 1] *= -1
    parts = [(np.ones(len(vs), bool), np.ones(len(old), bool))]
    if base in PAIRS:
        mid = (vs[:, 0].min() + vs[:, 0].max()) / 2
        parts = [(vs[:, 0] >= mid, old[:, 0] >= 0), (vs[:, 0] < mid, old[:, 0] < 0)]
    out = vs.copy()
    for new_sel, old_sel in parts:
        a, b = vs[new_sel], old[old_sel]
        blo, bhi = b.min(0), b.max(0)
        k = WIDER.get(base, 1.0)
        mid = (blo + bhi) / 2
        blo = np.array([mid[0] + (blo[0] - mid[0]) * k, mid[1] + (blo[1] - mid[1]) * k, blo[2]])
        bhi = np.array([mid[0] + (bhi[0] - mid[0]) * k, mid[1] + (bhi[1] - mid[1]) * k, bhi[2]])
        f = fit_box(a.min(0), a.max(0), blo, bhi)
        out[new_sel] = np.array([f(p) for p in a])
    for v, p in zip(ob.data.vertices, out):
        v.co = Vector(p)
    # A painted piece is a relief: stretched to the old box, most of it lay
    # inside the body. Whatever is inside the old piece (which hugs the body)
    # is put on its surface, a little out; what is outside stays.
    wrap = ob.modifiers.new("wrap", "SHRINKWRAP")
    wrap.target = target
    wrap.wrap_method = "NEAREST_SURFACEPOINT"
    wrap.wrap_mode = "OUTSIDE_SURFACE"
    wrap.offset = 0.014
    for x in bpy.data.objects:
        x.select_set(x is ob)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=wrap.name)
    bpy.data.objects.remove(target)
    out = np.array([v.co[:] for v in ob.data.vertices])
    for bn in rig.data.bones:
        ob.vertex_groups.new(name=bn.name)
    vg = {g.name: g for g in ob.vertex_groups}
    for i, p in enumerate(out):
        j = int(((old - p) ** 2).sum(1).argmin())
        for name, w in groups[j]:
            if name in vg:
                vg[name].add([i], w, "REPLACE")
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob


def export(rig, ob, name):
    for x in bpy.data.objects:
        x.select_set(False)
    ob.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, name + ".gltf"), export_format="GLTF_SEPARATE",
                              use_selection=True, export_animations=False, export_skins=True, export_apply=True,
                              export_yup=True, export_image_format="AUTO")
    for ext in (".gltf", ".bin"):
        assert os.path.getsize(os.path.join(OUT, name + ext)) < 1 << 20, f"{name}{ext} is over 1 MB"
    print(f"ARMOR {name}: {len(ob.data.polygons)} triangles")


def main():
    only = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    bpy.ops.wm.read_factory_settings(use_empty=True)
    robot = [o for o in import_glb(os.path.join(ROOT, "mods", "robot", "robot.gltf"))
             if o.type != "MESH" or o.vertex_groups]
    rig = next(o for o in robot if o.type == "ARMATURE")
    rig.data.pose_position = "REST"
    for o in robot:
        if o.type == "MESH":
            o.hide_set(True)
            o.hide_render = True
    with open(os.path.join(SRC, "strips", "spans.json")) as f:
        meta = json.load(f)
    made = []
    for row, names in meta["rows"].items():
        if only and not set(only) & set(names):
            continue
        src = [o for o in import_glb(os.path.join(SRC, f"{row}.glb")) if o.type == "MESH"][0]
        src.parent = None
        for x in bpy.data.objects:
            x.select_set(x is src)
        bpy.context.view_layer.objects.active = src
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        for (x0, x1), base in zip(ranges(src, meta["spans"][row]), names):
            if only and base not in only:
                continue
            high = take(src, lambda c, x0=x0, x1=x1: x0 <= c.x < x1, base + "_high")
            low = bpy.data.objects.new("low_" + base, high.data.copy())
            bpy.context.scene.collection.objects.link(low)
            decimate(low, TRIS)
            img = bake(high, low, "gear_" + base, TEX)
            texture(img, COLOURS, os.path.join(OUT, f"gear_{base}.png"))
            for body in BODIES:
                ob = worn(rig, low, base, body)
                export(rig, ob, f"gear_{base}{body}")
                if body == "":
                    made.append(ob)
                else:
                    bpy.data.objects.remove(ob)
            low.hide_render = True
            low.hide_set(True)
        bpy.data.objects.remove(src)
    if made:
        look(robot, made)


def look(robot, made):
    """Every piece on the robot at rest, from its front-right: a sheet to judge them by."""
    scene = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = next((e for e in engines if "EEVEE" in e), "CYCLES")
    for o in robot:
        if o.type == "MESH":
            o.hide_render = False
            names = {g.index: g.name for g in o.vertex_groups}
            bm = bmesh.new()
            bm.from_mesh(o.data)
            dl = bm.verts.layers.deform.verify()
            gone = [v for v in bm.verts if any(names.get(k, "").startswith("tool") for k, w in v[dl].items() if w > 0.5)]
            bmesh.ops.delete(bm, geom=gone, context="VERTS")
            bm.to_mesh(o.data)
            bm.free()
    world = bpy.data.worlds.new("w")
    world.color = (0.8, 0.8, 0.82)
    scene.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 4.5
    sun.rotation_euler = (0.9, 0.1, 2.6)
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.location = (1.9, 2.6, 1.5)
    cam.rotation_euler = (Vector((0, 0, 0.75)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    S = 256
    scene.render.resolution_x = scene.render.resolution_y = S
    shots = []
    for k, ob in enumerate(made):
        for i, o in enumerate(made):
            o.hide_render = i != k
        pts = [ob.matrix_world @ v.co for v in ob.data.vertices]
        lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
        hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
        c = (lo + hi) / 2
        r = max(0.3, (hi - lo).length)
        cam.location = c + Vector((0.7, 1.0, 0.35)).normalized() * r * 1.9
        cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler()
        p = os.path.join(LOOK, f"armor_meshy_{k}.png")
        scene.render.filepath = p
        bpy.ops.render.render(write_still=True)
        shots.append(p)
    cols = 6
    rows = (len(shots) + cols - 1) // cols
    out = bpy.data.images.new("sheet", S * cols, S * rows)
    px = np.zeros((S * rows, S * cols, 4), dtype=np.float32)
    for i, p in enumerate(shots):
        a = np.array(bpy.data.images.load(p).pixels[:], dtype=np.float32).reshape(S, S, 4)
        r, c = rows - 1 - i // cols, i % cols
        px[r * S:(r + 1) * S, c * S:(c + 1) * S] = a
    out.pixels = px.ravel()
    out.filepath_raw = os.path.join(LOOK, "armor_meshy_sheet.png")
    out.file_format = "PNG"
    out.save()
    print("LOOK", out.filepath_raw)


main()
