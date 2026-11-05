# The pistons of mods/cinnabar, made from scratch: six block models, each a cell
# of the world with its front -- the way it pushes -- at +Z, which the cell's
# wallmounted face turns (mods/cinnabar/init.luau).
#
#   flatpak run org.blender.Blender --background --python tools/make_piston_models.py [-- PREVIEW_DIR]
#
#   piston / sticky_piston        drawn in, a whole cube: a stone body bound in
#                                 iron and a wooden head plate (green with
#                                 slime on the sticky one) on its front
#   piston_on / sticky_piston_on  pushed out: the body, the back three quarters
#                                 of its cell, an iron collar with the rod's
#                                 hole where the plate was
#   piston_head / sticky_..._head the next cell: the plate on its far quarter
#                                 and the rod back through the body's quarter
#   piston_2 .. piston_5          pistons reaching two to five cells, one red
#   (sticky_, _on)                band round the body for each cell; pushed out
#                                 they wear piston_long_head (its rod ending
#                                 at its cell) and a piston_rod in every cell between
#
# Sizes are sixteenths of a cell, as the boxes in init.luau. Leaves the work in
# assets/blender/piston.blend; PREVIEW_DIR gets a picture of each from three
# quarters in front.
import math
import os
import sys

import bpy
from mathutils import Matrix

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "cinnabar")
BLEND = os.path.join(ROOT, "assets", "blender", "piston.blend")


def material(name, rgb, metal=0.0, rough=0.85):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1.0)
    return m


bpy.ops.wm.read_factory_settings(use_empty=True)
M = {
    "stone": material("piston_stone", (0.40, 0.40, 0.41)),
    "stone_dark": material("piston_stone_dark", (0.30, 0.30, 0.31)),
    "iron": material("piston_iron", (0.62, 0.62, 0.66), 0.6, 0.4),
    "iron_dark": material("piston_iron_dark", (0.26, 0.26, 0.29), 0.6, 0.5),
    "hole": material("piston_hole", (0.05, 0.05, 0.06), 0.0, 0.9),
    "wood": material("piston_wood", (0.66, 0.48, 0.27)),
    "wood_dark": material("piston_wood_dark", (0.36, 0.24, 0.12)),
    "slime": material("piston_slime", (0.42, 0.80, 0.30), 0.0, 0.25),
    "cinnabar": material("piston_cinnabar", (0.80, 0.10, 0.06), 0.0, 0.5),
}


def build(name, parts):
    """One object from boxes ((x0, y0, z0), (x1, y1, z1), material) in sixteenths
    of the game's cell -- x, up, front -- placed about the cell's middle, the
    origin of the glTF node at (0.5, 0.5, 0.5) as the hopper's."""
    mesh = bpy.data.meshes.new(name)
    verts, faces, mats = [], [], []
    names = list(dict.fromkeys(p[2] for p in parts))
    for a, b, m in parts:
        # Game (x, up, front) to Blender (x, -front, up), about the cell's middle.
        g = [(x / 16 - 0.5, -(z / 16 - 0.5), y / 16 - 0.5)
             for z in (a[2], b[2]) for y in (a[1], b[1]) for x in (a[0], b[0])]
        # Corner index = x + 2y + 4z in game terms; the faces wound outwards.
        o = len(verts)
        verts += g
        # Blender's handedness flips with front -> -Y, so each face below is
        # wound as seen from outside after that flip.
        for f in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)):
            faces.append(tuple(o + i for i in f))
            mats.append(names.index(m))
    mesh.from_pydata(verts, [], faces)
    for i, poly in enumerate(mesh.polygons):
        poly.material_index = mats[i]
    for m in names:
        mesh.materials.append(M[m])
    # Whatever the winding came out as, outward it is.
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    obj.location = (0.5, -0.5, 0.5)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def ring(x0, y0, x1, y1, z0, z1, w, m):
    """A square frame w wide round (x0, y0)-(x1, y1), z0..z1 deep."""
    return [((x0, y0, z0), (x1, y0 + w, z1), m), ((x0, y1 - w, z0), (x1, y1, z1), m),
            ((x0, y0 + w, z0), (x0 + w, y1 - w, z1), m), ((x1 - w, y0 + w, z0), (x1, y1 - w, z1), m)]


def body(front, reach=1):
    """The stone body from the back to `front` (12, three quarters): inset half a
    sixteenth so the iron stands proud of it, iron posts down its four long
    edges, an iron band round the back and round the front, and on the back an
    iron plate with a cinnabar eye -- where the power goes in."""
    # No two faces of the model on one plane, the way they face or not: the
    # stone stops a sixteenth short of the front band's face, so neither the
    # band nor the collar over it fight with it.
    p = [((0.5, 0.5, 0.5), (15.5, 15.5, front - 1), "stone")]
    # Darker stone courses across each side, so it reads as stone set in iron.
    for y in (5, 10):
        p += [((0.4, y, 1), (15.6, y + 0.4, front - 2), "stone_dark")]
    p += ring(0, 0, 16, 16, 0, 1.5, 1.5, "iron")                 # back band
    p += ring(0, 0, 16, 16, front - 2, front, 1.5, "iron")       # front band
    if reach > 1:
        # How far it reaches, told by red bands round the body: one a cell.
        for i in range(reach):
            z0, z1 = 2 + (front - 4) * (i + 0.5) / reach - 0.35, 2 + (front - 4) * (i + 0.5) / reach + 0.35
            # Between the corner posts, not over them: flush with their faces.
            p += [((1.5, 0, z0), (14.5, 1, z1), "cinnabar"), ((1.5, 15, z0), (14.5, 16, z1), "cinnabar"),
                  ((0, 1.5, z0), (1, 14.5, z1), "cinnabar"), ((15, 1.5, z0), (16, 14.5, z1), "cinnabar")]
    for x, y in ((0, 0), (14.5, 0), (0, 14.5), (14.5, 14.5)):    # corner posts
        p += [((x, y, 1.5), (x + 1.5, y + 1.5, front - 2), "iron_dark")]
    p += [((4, 4, 0.2), (12, 12, 0.5), "iron_dark"),
          ((6.5, 6.5, 0), (9.5, 9.5, 0.2), "cinnabar")]
    return p


def plate(sticky, z0=12):
    """The head plate on z0..16: three planks with dark seams between, on a dark
    backing; the sticky one's front is a pad of slime, running over the edge."""
    face = 15 if sticky else 16
    p = [((0.25, 0.25, z0), (15.75, 15.75, z0 + 0.75), "wood_dark")]
    for y0, y1 in ((0, 5.2), (5.4, 10.6), (10.8, 16)):
        p += [((0, y0, z0 + 0.75), (16, y1, face), "wood")]
    p += [((0, 5.2, z0 + 0.75), (16, 5.4, face - 0.2), "wood_dark"),
          ((0, 10.6, z0 + 0.75), (16, 10.8, face - 0.2), "wood_dark")]
    if sticky:
        p += [((0.5, 0.5, 15), (15.5, 15.5, 16), "slime"),
              # drips over the top and down the sides
              ((3, 16, 14), (6, 16.05, 15.5), "slime"), ((10, 16, 13.5), (12, 16.05, 15.5), "slime"),
              ((-0.05, 9, 14), (0, 12, 15.5), "slime"), ((16, 4, 13.8), (16.05, 7, 15.5), "slime")]
    return p


def piston(sticky, reach=1):
    # The head drawn in: the plate against the body, the rod hidden inside.
    return body(12, reach) + plate(sticky, 12)


def piston_on(reach=1):
    # The plate gone: the collar's face round a dark hole, the rod's way out.
    # The hole's floor is wider than the opening, so its sides are hidden in
    # the collar rather than lying on the collar's own inner faces.
    p = body(12, reach) + ring(1.5, 1.5, 14.5, 14.5, 11, 12, 4.5, "iron_dark") + \
        [((5.8, 5.8, 10.5), (10.2, 10.2, 11.4), "hole")]
    if reach > 1:
        # The rod out of the hole to the end of the cell: the rods in front
        # take it on from there, end to end and not overlapping.
        p += [((6.5, 6.5, 11), (9.5, 9.5, 16), "iron")]
    return p


def head(sticky):
    # The plate on this cell's far quarter and the rod back through the body's,
    # thick enough to see, with a collar where it meets the plate.
    return plate(sticky, 12) + [((6.5, 6.5, -4), (9.5, 9.5, 10.5), "iron"),
                                ((5.5, 5.5, 10.5), (10.5, 10.5, 12), "iron_dark")]


def long_head(sticky):
    # The head of one reaching further: its rod stops at its own cell's back,
    # where the last length of rod ends.
    return plate(sticky, 12) + [((6.5, 6.5, 0), (9.5, 9.5, 10.5), "iron"),
                                ((5.5, 5.5, 10.5), (10.5, 10.5, 12), "iron_dark")]


def rod():
    # One length of a long reaching piston's rod, its own cell end to end,
    # with a red joint ring where the next length takes it on.
    return [((6.5, 6.5, 0), (9.5, 9.5, 14), "iron"),
            ((6, 6, 14), (10, 10, 16), "cinnabar")]


MODELS = {
    "piston": piston(False),
    "sticky_piston": piston(True),
    "piston_on": piston_on(),
    "sticky_piston_on": piston_on(),
    "piston_head": head(False),
    "sticky_piston_head": head(True),
    # The ones reaching two to five cells: their own bodies, the heads the
    # same as the one-cell piston's, and a length of rod for every cell between.
    **{f"{pre}piston_{r}": piston(pre == "sticky_", r) for r in (2, 3, 4, 5) for pre in ("", "sticky_")},
    **{f"piston_{r}_on": piston_on(r) for r in (2, 3, 4, 5)},
    "piston_long_head": long_head(False),
    "sticky_piston_long_head": long_head(True),
    "piston_rod": rod(),
}

for name, parts in MODELS.items():
    obj = build(name, parts)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, f"{name}.gltf"),
                              export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_yup=True, export_materials="EXPORT",
                              export_image_format="NONE", export_skins=False)

# Laid out in the .blend: the drawn-in ones apart, the pushed-out ones with
# their heads in front of them as in the world.
layout = {"piston": (0, 0), "sticky_piston": (2, 0), "piston_on": (4, 0), "piston_head": (4, -1),
          "sticky_piston_on": (6, 0), "sticky_piston_head": (6, -1),
          **{f"{pre}piston_{r}": (r * 2 - 4, 4 + (pre != "")) for r in (2, 3, 4, 5) for pre in ("", "sticky_")},
          **{f"piston_{r}_on": (r * 2 - 4, 7) for r in (2, 3, 4, 5)}, "piston_rod": (10, 7)}
for name, (x, y) in layout.items():
    bpy.data.objects[name].location = (x + 0.5, y - 0.5, 0.5)

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if args:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_cavity = True
    scene.render.resolution_x = scene.render.resolution_y = 512
    scene.render.film_transparent = True
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type = "ORTHO"
    scene.collection.objects.link(cam)
    scene.camera = cam
    shots = {"front": ((1, -1, 0.8), 2.2), "back": ((-1, 1, 0.8), 2.2)}
    for name in [n for n in MODELS if n in ("piston", "sticky_piston", "piston_on", "sticky_piston_on")
                 or n in ("piston_5", "sticky_piston_3", "piston_rod")]:
        for o in bpy.data.objects:
            o.hide_render = o.type == "MESH" and o.name != name and not (
                name.endswith("_on") and o.name == name.replace("_on", "_head"))
        if name.endswith("_head"):
            continue
        target = bpy.data.objects[name].location.copy()
        if name.endswith("_on"):
            target.y -= 0.5
        for shot, (d, scale) in shots.items():
            cam.data.ortho_scale = scale * (1.3 if name.endswith("_on") else 1)
            v = __import__("mathutils").Vector(d).normalized()
            cam.location = target + v * 6
            cam.rotation_euler = (-v).to_track_quat("-Z", "Y").to_euler()
            scene.render.filepath = os.path.join(args[0], f"{name}_{shot}.png")
            bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)
    for o in bpy.data.objects:
        o.hide_render = False

bpy.ops.wm.save_as_mainfile(filepath=BLEND)
print(f"{len(MODELS)} piston models")
