# What a character of games/arpg holds -- every weapon, shield and orb --
# remade from Meshy: four painted sheets of them in a row (assets/meshy/arpg_icons/
# blades, axes, bows, off .png, also the inventory's pictures) each made one
# model by image-to-3D in smart topology (assets/meshy/arpg/sheet_*.glb), cut
# here into its things, each fitted where tools/make_arpg_gear.py's own piece
# was, and exported over it as games/arpg/mods/arpg_core/gear_<base>.gltf
# (+ .bin + .png):
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_arpg_gear_meshy.py" [-- base ...]
#
# What is worn (helms, armour, gloves, boots, belts, the amulet) stays
# make_arpg_gear.py's, which must have run first: its piece of each base is
# what the new one is fitted to -- the same box along the held axis, the same
# bone of the robot's rig (hand_R, hand_L, forearm_L), so a clip that swings a
# sword swings this one the same.
#
# A sheet is one mesh: things side by side along x, standing up z, painted
# face to -y. Its things are told apart by the gaps along x where no vertex
# is; each one's faces go to a copy (the high one, the sheet's UVs and its
# 2048 picture), and a low one of it is decimated to TRIS, unwrapped afresh and
# the colour baked onto it at TEX px reduced to COLOURS (tools/meshy_prop.py's
# bake and texture): under the 1 MB a media file may weigh, and flat colours
# like the game's blocks. A look: ~/.cache/cnp_look/gear_meshy_sheet.png.
#
# And the butcher's cleaver (axes, sixth), as assets/meshy/arpg/cleaver.glb,
# standing up z from its handle's end: tools/make_butcher.py puts it in the
# Butcher's hand.
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from meshy_prop import bake, decimate, texture  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "assets", "meshy", "arpg")
OUT = os.path.join(ROOT, "games", "arpg", "mods", "arpg_core")
LOOK = os.path.expanduser("~/.cache/cnp_look")
TRIS, TEX, COLOURS = 1400, 128, 32

SHEETS = {
    "blades": ["short_sword", "long_sword", "broad_sword", "war_sword", "dagger", "kris"],
    "axes": ["hand_axe", "battle_axe", "great_axe", "club", "war_hammer", "cleaver"],
    "bows": ["short_bow", "long_bow", "war_bow", "hunters_bow", "staff", "battle_staff", "wand", "bone_wand"],
    "off": ["buckler", "kite_shield", "tower_shield", "orb", "eye_orb"],
}
BOWS = {"short_bow", "long_bow", "war_bow", "hunters_bow"}
SHIELDS = {"buckler", "kite_shield", "tower_shield"}
ORBS = {"orb", "eye_orb"}


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


# Where each thing is across its sheet's picture, in pixels (tools/make_arpg_icons.py
# --blobs): a cut goes in the emptiest x near the middle of each gap, mapped
# onto the model, whose x runs from the first thing's left to the last's right.
SPANS = {
    "blades": [(164, 248), (324, 464), (540, 656), (708, 896), (960, 1024), (1112, 1220)],
    "axes": [(36, 192), (216, 464), (484, 744), (756, 904), (924, 1172), (1192, 1340)],
    "bows": [(76, 188), (244, 332), (392, 520), (564, 700), (744, 840), (888, 1000), (1080, 1116), (1216, 1288)],
    "off": [(32, 260), (292, 548), (572, 808), (832, 1056), (1092, 1332)],
}


def split(ob, sheet):
    """Of a sheet's mesh, a range of x per thing, left to right."""
    spans = SPANS[sheet]
    xs = np.array([v.co.x for v in ob.data.vertices])
    lo, hi = xs.min(), xs.max()
    p0, p1 = spans[0][0], spans[-1][1]
    bins = 2000
    hist, edges = np.histogram(xs, bins=bins, range=(lo, hi))
    cuts = []
    for a, b in zip(spans, spans[1:]):
        guess = lo + ((a[1] + b[0]) / 2 - p0) / (p1 - p0) * (hi - lo)
        i = int((guess - lo) / (hi - lo) * bins)
        w = max(2, int(bins * (b[0] - a[1]) / (p1 - p0) * 0.8))
        window = range(max(0, i - w), min(bins, i + w + 1))
        best = min(window, key=lambda k: (hist[k], abs(k - i)))
        cuts.append((edges[best] + edges[best + 1]) / 2)
    bounds = [lo - 1] + cuts + [hi + 1]
    return list(zip(bounds, bounds[1:]))


def take(ob, x0, x1, name):
    """A copy of the faces of ob whose middle is between x0 and x1."""
    me = ob.data.copy()
    cp = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(cp)
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(me)
    gone = [f for f in bm.faces if not (x0 <= f.calc_center_median().x < x1)]
    bmesh.ops.delete(bm, geom=gone, context="FACES")
    bm.to_mesh(me)
    bm.free()
    return cp


def box_of(objs):
    pts = [o.matrix_world @ v.co for o in objs if o.type == "MESH" for v in o.data.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def old_piece(base):
    """make_arpg_gear.py's piece of a base: its box at rest and the bone it follows."""
    new = import_glb(os.path.join(OUT, f"gear_{base}.gltf"))
    # (the importer's own sphere for drawing bones has no vertex groups)
    meshes = [o for o in new if o.type == "MESH" and o.vertex_groups]
    lo, hi = box_of(meshes)
    count = {}
    for o in meshes:
        names = {g.index: g.name for g in o.vertex_groups}
        for v in o.data.vertices:
            for g in v.groups:
                if g.weight > 0.5:
                    count[names[g.group]] = count.get(names[g.group], 0) + 1
    for o in new:
        bpy.data.objects.remove(o, do_unlink=True)
    return lo, hi, max(count, key=count.get)


def placing(base, slo, shi, olo, ohi):
    """The matrix from a thing on the sheet (box slo..shi) to where the old piece
    was (box olo..ohi): see the top of the file for each kind's axes."""
    sd, od = shi - slo, ohi - olo
    sc = (slo + shi) / 2
    oc = (olo + ohi) / 2
    if base in SHIELDS:
        # sheet x -> -y, z -> z, its painted face (-y) -> -x; fitted inside the old box
        s = min(od.y / sd.x, od.z / sd.z)
        rot = Matrix(((0, 1, 0), (-1, 0, 0), (0, 0, 1))).to_4x4()
        back = Vector((ohi.x - s * sd.y / 2, oc.y, oc.z))   # its back on the forearm, where the old one's was
        return Matrix.Translation(back) @ rot @ Matrix.Scale(s, 4) @ Matrix.Translation(-sc)
    if base in ORBS:
        # face forward (+y): turned half round
        s = max(od) / max(sd)
        rot = Matrix.Rotation(math.pi, 4, "Z")
        return Matrix.Translation(oc) @ rot @ Matrix.Scale(s, 4) @ Matrix.Translation(-sc)
    # Held along +y from the fist, up the sheet (z) along it: sheet x -> z
    # (across the blade, up), sheet y -> x; its foot where the old one's was.
    s = od.y / sd.z
    rot = Matrix(((0, 1, 0), (0, 0, 1), (1, 0, 0))).to_4x4()   # (x, y, z) -> (y, z, x)
    if base in BOWS:
        # the string (the sheet's +x) to +z, towards the archer when drawn, as the old one's
        at = Vector((oc.x, oc.y, ohi.z - s * sd.x / 2))
    else:
        at = Vector((oc.x, oc.y, oc.z))
    return Matrix.Translation(at) @ rot @ Matrix.Scale(s, 4) @ Matrix.Translation(-sc)


def bound(rig, ob, bone):
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    g = ob.vertex_groups[bone]
    g.add([v.index for v in ob.data.vertices], 1.0, "REPLACE")
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig


def export_gear(rig, ob, base):
    for x in bpy.data.objects:
        x.select_set(False)
    ob.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = ob
    path = os.path.join(OUT, f"gear_{base}.gltf")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_skins=True, export_apply=True, export_yup=True,
                              export_image_format="AUTO")
    for ext in (".gltf", ".bin", ".png"):
        size = os.path.getsize(os.path.join(OUT, f"gear_{base}{ext}"))
        assert size < 1 << 20, f"gear_{base}{ext} is over 1 MB"
    print(f"GEAR {base}: {len(ob.data.polygons)} triangles")


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
    made = []
    for sheet, names in SHEETS.items():
        if only and not set(only) & set(names):
            continue
        src = [o for o in import_glb(os.path.join(SRC, f"sheet_{sheet}.glb")) if o.type == "MESH"][0]   # no rig there
        src.parent = None
        bpy.context.view_layer.objects.active = src
        for x in bpy.data.objects:
            x.select_set(x is src)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        for (x0, x1), base in zip(split(src, sheet), names):
            if only and base not in only:
                continue
            high = take(src, x0, x1, base + "_high")
            slo, shi = box_of([high])
            if base == "cleaver":
                # standing up z, its handle's end at the origin, a metre tall
                m = Matrix.Scale(1 / (shi.z - slo.z), 4) @ Matrix.Translation(
                    -Vector(((slo.x + shi.x) / 2, (slo.y + shi.y) / 2, slo.z)))
            else:
                olo, ohi, bone = old_piece(base)
                m = placing(base, slo, shi, olo, ohi)
            high.data.transform(m)
            low = bpy.data.objects.new("gear_" + base, high.data.copy())
            bpy.context.scene.collection.objects.link(low)
            decimate(low, TRIS)
            img = bake(high, low, "gear_" + base, TEX)
            os.makedirs(LOOK, exist_ok=True)
            texture(img, COLOURS, os.path.join(LOOK, f"gear_{base}.png"))
            img.filepath_raw = os.path.join(OUT, f"gear_{base}.png")
            img.save()
            if base == "cleaver":
                for x in bpy.data.objects:
                    x.select_set(x is low)
                bpy.ops.export_scene.gltf(filepath=os.path.join(SRC, "cleaver.glb"), export_format="GLB",
                                          use_selection=True, export_animations=False, export_yup=True)
                print("CLEAVER written")
                bpy.data.objects.remove(low)
                continue
            bound(rig, low, bone)
            export_gear(rig, low, base)
            made.append(low)
        bpy.data.objects.remove(src)
    # A look at them all on the robot at rest, side by side in pairs.
    if made:
        look(rig, robot, made)


def look(rig, robot, made):
    """Each piece on the robot at rest, close, from its right and front: a sheet to judge them by."""
    import bmesh
    scene = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = next((e for e in engines if "EEVEE" in e), "CYCLES")
    for o in robot:
        if o.type == "MESH":
            o.hide_render = False
            # the robot's own tools (bones tool_*) out of the way
            names = {g.index: g.name for g in o.vertex_groups}
            bm = bmesh.new()
            bm.from_mesh(o.data)
            dl = bm.verts.layers.deform.verify()
            gone = [v for v in bm.verts if any(names.get(k, "").startswith("tool") for k, w in v[dl].items() if w > 0.5)]
            bmesh.ops.delete(bm, geom=gone, context="VERTS")
            bm.to_mesh(o.data)
            bm.free()
    world = bpy.data.worlds.new("w")
    world.color = (0.35, 0.35, 0.38)
    scene.world = world
    light = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    light.data.energy = 3
    light.rotation_euler = (0.7, 0.2, 0.6)
    scene.collection.objects.link(light)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    S = 256
    scene.render.resolution_x = scene.render.resolution_y = S
    shots = []
    for k, ob in enumerate(made):
        for i, o in enumerate(made):
            o.hide_render = i != k
        lo, hi = box_of([ob])
        c = (lo + hi) / 2
        r = max(0.35, (hi - lo).length * 0.9)
        cam.location = c + Vector((1.0, 0.6, 0.45)).normalized() * r * 2.2
        cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler()
        p = os.path.join(LOOK, f"gear_meshy_{k}.png")
        scene.render.filepath = p
        bpy.ops.render.render(write_still=True)
        shots.append(p)
    cols = 6
    rows = (len(shots) + cols - 1) // cols
    out = bpy.data.images.new("sheet", S * cols, S * rows)
    px = np.zeros((S * rows, S * cols, 4), dtype=np.float32)
    for i, p in enumerate(shots):
        im = bpy.data.images.load(p)
        a = np.array(im.pixels[:], dtype=np.float32).reshape(S, S, 4)
        r, c = rows - 1 - i // cols, i % cols
        px[r * S:(r + 1) * S, c * S:(c + 1) * S] = a
    out.pixels = px.ravel()
    out.filepath_raw = os.path.join(LOOK, "gear_meshy_sheet.png")
    out.file_format = "PNG"
    out.save()
    print("LOOK", out.filepath_raw)


main()
