# Builds the two machines of mods/pipes -- the automatic crafting table and the
# automatic furnace -- and leaves the work in assets/blender/pipes_machines.blend.
#
#   flatpak run org.blender.Blender --background --python tools/make_pipes_machines.py [-- PREVIEW.png]
#
# Level-two blocks: what a pipe network crafts and smelts with (mods/pipes says
# how). Each in two states, as the mod swaps the block: standing (its lamp
# dark) and at work (its lamp lit, and the furnace's fire showing):
#
#   autocrafter.gltf, autocrafter_on.gltf   -- the crafting table of
#       mods/craft grown up: the same wooden top with its 3x3 grid, on an iron
#       housing with a vent and a lamp, and over the grid a gantry with a
#       claw that does the crafting.
#   autofurnace.gltf, autofurnace_on.gltf   -- the furnace of mods/furnace
#       in an iron frame: its brick body, an iron door with a window and a
#       grate, a gauge and a lamp beside it, a feed hopper on top and a
#       chimney behind.
#
# Both have a brass port in the middle of each side and the back, where the
# arm of a pipe comes in (a pipe's core is 6/16 to 10/16 high).
#
# Authored in the unit cube in the game's own axes -- x across, u up, d the
# depth of the cell with the front at d = 1 (glTF +Z: an upright block faces
# whoever put it down) -- and put into Blender's axes by `at` below. The
# colours are those of mods/craft's table and mods/furnace's furnace.
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "pipes")
BLEND = os.path.join(ROOT, "assets", "blender", "pipes_machines.blend")


def material(name, rgb, metal=0.0, rough=0.8, glow=None):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    if glow is not None:
        bsdf.inputs["Emission Color"].default_value = (*glow, 1.0)
        bsdf.inputs["Emission Strength"].default_value = 1.0
    m.diffuse_color = (*rgb, 1.0)
    return m


M = {}


def make_materials():
    M.update(make_material_table())


def make_material_table():
    return {
    "wood_dark": material("wood_dark", (0.36, 0.22, 0.11)),
    "wood": material("wood", (0.62, 0.40, 0.20)),
    "wood_light": material("wood_light", (0.80, 0.60, 0.36)),
    "iron": material("iron", (0.30, 0.30, 0.33), metal=0.6, rough=0.5),
    "iron_dark": material("iron_dark", (0.17, 0.17, 0.19), metal=0.6, rough=0.55),
    "steel": material("steel", (0.46, 0.48, 0.52), metal=0.5, rough=0.45),
    "brass": material("brass", (0.78, 0.58, 0.24), metal=0.7, rough=0.35),
    "hole": material("hole", (0.05, 0.05, 0.06)),
    "mortar": material("mortar", (0.22, 0.21, 0.20)),
    "stone_a": material("stone_a", (0.50, 0.49, 0.47)),
    "stone_b": material("stone_b", (0.42, 0.41, 0.40)),
    "stone_c": material("stone_c", (0.57, 0.55, 0.52)),
    "soot": material("soot", (0.07, 0.06, 0.06)),
    "dial": material("dial", (0.88, 0.86, 0.80)),
    "lamp_off": material("lamp_off", (0.16, 0.22, 0.16)),
    "lamp_green": material("lamp_green", (0.35, 1.0, 0.45), glow=(0.35, 1.0, 0.45)),
    "lamp_amber": material("lamp_amber", (1.0, 0.62, 0.15), glow=(1.0, 0.62, 0.15)),
    "fire_outer": material("fire_outer", (1.0, 0.45, 0.08), glow=(1.0, 0.45, 0.08)),
    "fire_core": material("fire_core", (1.0, 0.85, 0.35), glow=(1.0, 0.85, 0.35)),
    "ember": material("ember", (0.95, 0.25, 0.05), glow=(0.95, 0.25, 0.05)),
    }


IN = 0.006   # how far a part stands back from a face it would otherwise share


def at(x, u, d):
    """The game's cell axes to Blender's: glTF (x, y up, z front) is Blender (x, -y, z)."""
    return Vector((x, -d, u))


class Model:
    """One mesh, built part by part, each part with its material."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.mats = []

    def _mat(self, key):
        if key not in self.mats:
            self.mats.append(key)
        return self.mats.index(key)

    def _finish(self, geom, key, bevel=0.0):
        faces = [g for g in geom if isinstance(g, bmesh.types.BMFace)]
        for f in faces:
            f.material_index = self._mat(key)
        if bevel > 0:
            edges = list({e for f in faces for e in f.edges})
            res = bmesh.ops.bevel(self.bm, geom=edges, offset=bevel, segments=1, affect="EDGES",
                                  profile=0.5, clamp_overlap=True)
            for f in res["faces"]:
                f.material_index = self._mat(key)

    def box(self, x0, u0, d0, x1, u1, d1, key, bevel=0.0):
        r = bmesh.ops.create_cube(self.bm, size=1.0)
        vs = r["verts"]
        lo, hi = at(x0, u0, d1), at(x1, u1, d0)   # d grows to the front, Blender y the other way
        for v in vs:
            v.co = Vector((lo.x if v.co.x < 0 else hi.x, lo.y if v.co.y < 0 else hi.y,
                           lo.z if v.co.z < 0 else hi.z))
        self._finish(list({f for v in vs for f in v.link_faces}), key, bevel)

    def cyl(self, centre, axis, r, length, key, segs=16, r2=None, bevel=0.0):
        """A cylinder (or a cone, with r2) about `centre` (x, u, d), along "x", "u" or "d"."""
        r = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=segs,
                                  radius1=r, radius2=r if r2 is None else r2, depth=length)
        turn = {"x": Matrix.Rotation(math.radians(90), 4, "Y"),
                "d": Matrix.Rotation(math.radians(90), 4, "X"),
                "u": Matrix.Identity(4)}[axis]
        bmesh.ops.transform(self.bm, matrix=Matrix.Translation(at(*centre)) @ turn, verts=r["verts"])
        self._finish(list({f for v in r["verts"] for f in v.link_faces}), key, bevel)

    def frustum(self, cx, cd, u0, u1, w0, w1, key):
        """A square funnel standing on (cx, cd): w0 across at u0, w1 at u1."""
        bm = self.bm
        lo = [bm.verts.new(at(cx + sx * w0 / 2, u0, cd + sd * w0 / 2)) for sx, sd in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        hi = [bm.verts.new(at(cx + sx * w1 / 2, u1, cd + sd * w1 / 2)) for sx, sd in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        faces = [bm.faces.new(list(reversed(lo))), bm.faces.new(hi)]
        for i in range(4):
            j = (i + 1) % 4
            faces.append(bm.faces.new((lo[i], lo[j], hi[j], hi[i])))
        bmesh.ops.recalc_face_normals(bm, faces=faces)
        self._finish(faces, key)

    def port(self, side):
        """A brass flange with a dark mouth in the middle of a side, where a pipe comes in."""
        u = 0.5
        if side == "left":
            self.cyl((0.035, u, 0.5), "x", 0.17, 0.05, "brass", bevel=0.006)
            self.cyl((0.008, u, 0.5), "x", 0.11, 0.012, "hole")
        elif side == "right":
            self.cyl((0.965, u, 0.5), "x", 0.17, 0.05, "brass", bevel=0.006)
            self.cyl((0.992, u, 0.5), "x", 0.11, 0.012, "hole")
        elif side == "back":
            self.cyl((0.5, u, 0.035), "d", 0.17, 0.05, "brass", bevel=0.006)
            self.cyl((0.5, u, 0.008), "d", 0.11, 0.012, "hole")
        for k in range(4):   # four bolts round the flange
            a = math.radians(45 + 90 * k)
            o1, o2 = 0.135 * math.cos(a), 0.135 * math.sin(a)
            if side == "left":
                self.cyl((0.005, u + o1, 0.5 + o2), "x", 0.018, 0.012, "iron_dark", segs=6)
            elif side == "right":
                self.cyl((0.995, u + o1, 0.5 + o2), "x", 0.018, 0.012, "iron_dark", segs=6)
            else:
                self.cyl((0.5 + o2, u + o1, 0.005), "d", 0.018, 0.012, "iron_dark", segs=6)

    def object(self):
        mesh = bpy.data.meshes.new(self.name)
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-6)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for key in self.mats:
            mesh.materials.append(M[key])
        for p in mesh.polygons:
            p.use_smooth = False
        ob = bpy.data.objects.new(self.name, mesh)
        return ob


# ---------------------------------------------------------------- the automatic crafting table

def autocrafter(on):
    m = Model("Autocrafter_on" if on else "Autocrafter")
    # The iron housing, on a plinth, with a post at each corner.
    m.box(0.02, 0.0, 0.02, 0.98, 0.07, 0.98, "iron_dark", bevel=0.01)
    m.box(0.07, 0.07, 0.07, 0.93, 0.66, 0.93, "steel", bevel=0.008)
    for x, d in ((0.03, 0.03), (0.85, 0.03), (0.03, 0.85), (0.85, 0.85)):
        m.box(x, 0.0, d, x + 0.12, 0.68, d + 0.12, "iron", bevel=0.012)
    # The front: a panel, a vent of slats, and the lamp that says it is at work.
    m.box(0.18, 0.12, 0.925, 0.82, 0.60, 0.945, "iron_dark", bevel=0.005)
    for k in range(5):
        u = 0.16 + k * 0.06
        m.box(0.24, u, 0.94, 0.62, u + 0.03, 0.955, "steel")
    m.box(0.67, 0.44, 0.94, 0.77, 0.54, 0.96, "iron", bevel=0.004)
    m.cyl((0.72, 0.49, 0.965), "d", 0.035, 0.02, "lamp_green" if on else "lamp_off", segs=12)
    m.box(0.67, 0.18, 0.94, 0.77, 0.36, 0.955, "brass", bevel=0.004)   # a nameplate
    # The worktop of mods/craft's table: a dark frame and its 3x3 grid inlaid.
    m.box(0.0, 0.66, 0.0, 1.0, 0.75, 1.0, "wood_dark", bevel=0.012)
    side, gap, x0 = 0.22, 0.03, 0.135
    for i in range(3):
        for j in range(3):
            a, b = x0 + i * (side + gap), x0 + j * (side + gap)
            m.box(a, 0.75, b, a + side, 0.765, b + side, "wood_light" if (i + j) % 2 == 0 else "wood")
    # The gantry over it: two posts at the back, a beam, an arm out over the
    # grid and a claw at its end.
    for x in (0.05, 0.87):
        m.box(x, 0.75, 0.06, x + 0.08, 1.0, 0.14, "iron", bevel=0.01)
    m.box(0.05 + IN, 0.92, 0.06 + IN, 0.95 - IN, 0.99, 0.14 - IN, "iron", bevel=0.01)
    m.box(0.46, 0.925, 0.12, 0.54, 0.98, 0.56, "steel", bevel=0.008)
    m.cyl((0.5, 0.95, 0.52), "u", 0.045, 0.07, "brass", segs=12)
    m.box(0.475, 0.83, 0.495, 0.525, 0.92, 0.545, "steel")
    m.box(0.42, 0.80, 0.46, 0.58, 0.84, 0.58, "iron_dark", bevel=0.006)
    for x in (0.425, 0.555):
        m.box(x, 0.775, 0.48, x + 0.02, 0.80, 0.56, "iron")
    for s in ("left", "right", "back"):
        m.port(s)
    return m.object()


# ---------------------------------------------------------------- the automatic furnace

def autofurnace(on):
    m = Model("Autofurnace_on" if on else "Autofurnace")
    # The brick body, its courses offset, behind the iron frame.
    m.box(0.06, 0.06, 0.06, 0.94, 0.86, 0.94, "mortar")
    rows, h = 5, 0.8 / 5
    for r in range(rows):
        u0 = 0.06 + r * h
        shift = 0.0 if r % 2 == 0 else 0.15
        xs = [0.06] + [0.06 + shift + k * 0.3 for k in range(1, 4)] + [0.94]
        xs = sorted({min(max(x, 0.06), 0.94) for x in xs})
        for k in range(len(xs) - 1):
            key = ("stone_a", "stone_b", "stone_c")[(r + k) % 3]
            a, b = xs[k] + 0.006, xs[k + 1] - 0.006
            if b - a < 0.04:
                continue
            m.box(a, u0 + 0.006, 0.935, b, u0 + h - 0.006, 0.95, key)                 # front
            m.box(0.05, u0 + 0.006, 1 - b, 0.065, u0 + h - 0.006, 1 - a, key)          # left
            m.box(0.935, u0 + 0.006, a, 0.95, u0 + h - 0.006, b, key)                  # right
    # The iron frame: a plinth, a cap, a post at each corner and a band.
    # A hair inside the corner posts' outer faces, never on them: two faces on
    # one plane flicker (z-fighting), whichever wins changing with the view.
    m.box(IN, 0.0, IN, 1 - IN, 0.07, 1 - IN, "iron_dark", bevel=0.01)
    m.box(IN, 0.84, IN, 1 - IN, 0.92, 1 - IN, "iron", bevel=0.012)
    for x, d in ((0.0, 0.0), (0.92, 0.0), (0.0, 0.92), (0.92, 0.92)):
        m.box(x, 0.0, d, x + 0.08, 0.9, d + 0.08, "iron", bevel=0.01)
    m.box(IN, 0.25, IN, 1 - IN, 0.29, 0.955, "iron_dark")   # a band round the middle, behind the door
    # The door: an iron frame, a window on the fire, a grate over it, a handle.
    m.box(0.22, 0.12, 0.945, 0.78, 0.58, 0.97, "iron", bevel=0.006)
    m.box(0.29, 0.19, 0.965, 0.71, 0.48, 0.975, "fire_outer" if on else "soot")
    if on:
        m.box(0.36, 0.196, 0.971, 0.64, 0.33, 0.978, "fire_core")
        m.box(0.316, 0.194, 0.972, 0.684, 0.23, 0.98, "ember")
    for k in range(5):
        x = 0.31 + k * 0.09
        m.box(x, 0.19, 0.975, x + 0.022, 0.48, 0.99, "iron_dark")
    m.box(0.29, 0.51, 0.97, 0.71, 0.55, 0.985, "iron_dark")
    m.box(0.66, 0.30, 0.97, 0.74, 0.34, 1.01, "brass", bevel=0.004)
    # A gauge on the left of the door's top, the lamp on its right.
    m.cyl((0.22, 0.72, 0.955), "d", 0.085, 0.03, "brass", segs=20, bevel=0.004)
    m.cyl((0.22, 0.72, 0.972), "d", 0.065, 0.008, "dial", segs=20)
    needle = 35 if on else 140
    a = math.radians(needle)
    for t in range(4):
        f = (t + 0.5) / 4 * 0.055
        m.box(0.22 + f * math.cos(a) - 0.006, 0.72 + f * math.sin(a) - 0.006, 0.975,
              0.22 + f * math.cos(a) + 0.006, 0.72 + f * math.sin(a) + 0.006, 0.981, "soot")
    m.box(0.72, 0.66, 0.945, 0.84, 0.78, 0.965, "iron", bevel=0.004)
    m.cyl((0.78, 0.72, 0.97), "d", 0.04, 0.02, "lamp_amber" if on else "lamp_off", segs=12)
    # On top: the feed hopper in the middle, the chimney behind on the right.
    m.box(0.3, 0.92, 0.32, 0.7, 0.95, 0.72, "iron_dark", bevel=0.006)
    m.frustum(0.5, 0.52, 0.95, 1.06, 0.24, 0.40, "iron")
    m.box(0.33, 1.035, 0.35, 0.67, 1.052, 0.69, "hole")       # under the rim, not on it
    m.cyl((0.8, 1.02, 0.2), "u", 0.07, 0.2, "iron_dark", segs=14)
    m.cyl((0.8, 1.13, 0.2), "u", 0.085, 0.03, "iron", segs=14, bevel=0.004)
    m.cyl((0.8, 1.152, 0.2), "u", 0.05, 0.008, "ember" if on else "soot", segs=14)
    for s in ("left", "right", "back"):
        m.port(s)
    return m.object()


def export(ob, path):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    kept = ob.location.copy()
    ob.location = (0, 0, 0)
    bpy.context.view_layer.update()
    ob.select_set(True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_yup=True, export_apply=False,
                              export_materials="EXPORT", export_image_format="NONE")
    ob.location = kept


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    make_materials()
    coll = bpy.data.collections.new("PipesMachines")
    bpy.context.scene.collection.children.link(coll)
    made = []
    for i, (name, build) in enumerate((("autocrafter", lambda: autocrafter(False)),
                                       ("autocrafter_on", lambda: autocrafter(True)),
                                       ("autofurnace", lambda: autofurnace(False)),
                                       ("autofurnace_on", lambda: autofurnace(True)))):
        ob = build()
        coll.objects.link(ob)
        export(ob, os.path.join(OUT, name + ".gltf"))
        ob.location.x = i * 1.6   # laid out side by side in the .blend
        made.append(ob)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)

    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if args:
        preview(made, args[0])
    print(f"{len(made)} machine models in mods/pipes, the work in {BLEND}")


def preview(objs, path):
    """The four side by side, from the front and a little to the left."""
    sc = bpy.context.scene
    cam = bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    mid = Vector((0.5 + 1.6 * 1.5, -0.5, 0.5))
    cam.location = mid + Vector((-1.0, -7.4, 3.4))
    cam.rotation_euler = (mid - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 38
    sun = bpy.data.objects.new("PreviewSun", bpy.data.lights.new("PreviewSun", "SUN"))
    sc.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(55), math.radians(-15), math.radians(-35))
    sun.data.energy = 3.0
    w = bpy.data.worlds.new("PreviewWorld")
    sc.world = w
    w.use_nodes = True
    bg = next(n for n in w.node_tree.nodes if n.type == "BACKGROUND")
    bg.inputs[0].default_value = (0.35, 0.38, 0.45, 1)
    bg.inputs[1].default_value = 0.8
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            sc.render.engine = eng
            break
        except TypeError:
            continue
    sc.render.resolution_x, sc.render.resolution_y = 1500, 560
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    for o in (cam, sun):
        bpy.data.objects.remove(o)


main()
