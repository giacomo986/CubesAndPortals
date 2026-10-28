# The railway's blocks as models (mods/building): straight track, the right
# and left corners, the slopes up and down, the powered rail off and on, and
# the switch set straight and set to turn.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_rail_models.py"
#
# Saves assets/blender/rails.blend and exports mods/building/<name>.gltf.
#
# Each inside the unit cell, feet at 0, the way the engine rides it
# (core.node_conveyor's `rail`, chute_at): the track runs along glTF Z, its
# two rails 0.17 and 0.83 across (the cart's wheels, tools/make_cart_model.py),
# their tops 1/16 over the cell's floor. A corner is two quarter arcs about the
# cell's corner at glTF (x 1, z 0), from the side x = 1 to the side z = 0 (the
# right one; the left is it mirrored). A slope rises a whole cell, towards
# glTF -Z (up) or +Z (down), over an embankment. Blender is Z up and glTF is
# Y up: glTF (x, y, z) is Blender (x, -z, y), so the cell is Blender x 0..1,
# y -1..0.
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "building")
BLEND = os.path.join(ROOT, "assets", "blender", "rails.blend")

TOP = 1 / 16          # the rails' top over the bed
TIE = 0.026           # a sleeper's thickness
RAILS = (0.17, 0.83)


def material(name, rgb, metal, rough, glow=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Metallic"].default_value = metal
    p.inputs["Roughness"].default_value = rough
    if glow > 0:
        p.inputs["Emission Color"].default_value = (*rgb, 1)
        p.inputs["Emission Strength"].default_value = glow
    return m


MATS = {
    "wood": ("Rail_sleeper", (0.36, 0.24, 0.15), 0.0, 0.9),
    "steel": ("Rail_steel", (0.42, 0.43, 0.46), 0.85, 0.45),
    "head": ("Rail_head", (0.70, 0.71, 0.74), 0.9, 0.25),
    "plate": ("Rail_plate", (0.16, 0.15, 0.15), 0.8, 0.6),
    "gold": ("Rail_gold", (0.80, 0.60, 0.22), 0.9, 0.3),
    "contact": ("Rail_contact", (0.30, 0.07, 0.05), 0.4, 0.6),
    "contact_on": ("Rail_contact_on", (1.0, 0.36, 0.08), 0.2, 0.4, 4.0),
    "bank": ("Rail_bank", (0.38, 0.36, 0.33), 0.0, 1.0),
    "lever": ("Rail_lever", (0.22, 0.22, 0.24), 0.8, 0.5),
    "green": ("Rail_green", (0.15, 0.85, 0.30), 0.0, 0.5, 2.0),
    "amber": ("Rail_amber", (1.0, 0.65, 0.10), 0.0, 0.5, 2.0),
}


class Model:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.slots = []

    def slot(self, key):
        if key not in self.slots:
            self.slots.append(key)
        return self.slots.index(key)

    def box(self, centre, size, frame, key):
        # A box `size` (across, along, up) at `centre`, turned by `frame` (3x3, columns across, along, up).
        ret = bmesh.ops.create_cube(self.bm, size=1.0)
        s = self.slot(key)
        for v in ret["verts"]:
            local = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
            v.co = Vector(centre) + frame @ local
        for f in {f for v in ret["verts"] for f in v.link_faces}:
            f.material_index = s

    def sweep(self, points, frames, profile, key):
        # A profile (across, up pairs, a closed loop) swept along points with their frames.
        s = self.slot(key)
        rings = []
        for p, fr in zip(points, frames):
            rings.append([self.bm.verts.new(p + fr.col[0] * a + fr.col[2] * u) for a, u in profile])
        n = len(profile)
        for r0, r1 in zip(rings, rings[1:]):
            for i in range(n):
                j = (i + 1) % n
                f = self.bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
                f.material_index = s
        for ring, flip in ((rings[0], True), (rings[-1], False)):
            f = self.bm.faces.new(list(reversed(ring)) if flip else ring)
            f.material_index = s

    def finish(self):
        me = bpy.data.meshes.new(self.name)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(me)
        self.bm.free()
        for key in self.slots:
            spec = MATS[key]
            me.materials.append(material(*spec))
        ob = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(ob)
        return ob


# The rail's section, across x up, its foot on the sleepers: foot, web, head.
PROFILE_STEEL = [(-0.034, 0.0), (0.034, 0.0), (0.034, 0.007), (0.009, 0.010), (0.009, 0.022),
                 (-0.009, 0.022), (-0.009, 0.010), (-0.034, 0.007)]
PROFILE_HEAD = [(-0.022, 0.0), (0.022, 0.0), (0.022, TOP - TIE - 0.022), (-0.022, TOP - TIE - 0.022)]


def frame(along, up_hint=Vector((0, 0, 1))):
    t = along.normalized()
    a = t.cross(up_hint).normalized()     # across, to the right of the run
    u = a.cross(t).normalized()
    return Matrix((a, t, u)).transposed()


def track(m, path, rails_key="steel", head_key="head", ties=5, contact=None, tie_len=0.92):
    # `path(s)` -> (centre on the bed, the run's direction, across to the right) for s 0..1.
    n = 16
    pts = [path(i / n) for i in range(n + 1)]
    for side in (0, 1):
        ps, fs = [], []
        for c, along, across in pts:
            fr = frame(along)
            off = (RAILS[side] - 0.5)
            base = c + across * off + fr.col[2] * TIE
            ps.append(base)
            fs.append(fr)
        m.sweep(ps, fs, PROFILE_STEEL, rails_key)
        m.sweep([p + f.col[2] * 0.022 for p, f in zip(ps, fs)], fs, PROFILE_HEAD, head_key)
    for i in range(ties):
        c, along, across = path((i + 0.5) / ties)
        fr = frame(along)
        m.box(c + fr.col[2] * (TIE / 2), (tie_len, 0.12, TIE), fr, "wood")
        for side in (0, 1):
            off = (RAILS[side] - 0.5)
            m.box(c + across * off + fr.col[2] * (TIE + 0.003), (0.1, 0.07, 0.006), fr, "plate")
    if contact:
        for i in range(n):
            c0, along, _ = pts[i]
            c1 = pts[i + 1][0]
            fr = frame(along)
            mid = (c0 + c1) / 2
            m.box(mid + fr.col[2] * (TIE + 0.008), (0.05, (c1 - c0).length + 0.002, 0.016), fr, contact)


def straight(s):
    # glTF z from 0 to 1 is Blender y from 0 to -1: the run goes -y.
    return Vector((0.5, -s, 0.0)), Vector((0, -1, 0)), Vector((1, 0, 0))


def slope(up):
    def path(s):
        # Rising towards glTF -Z (Blender +y) for up: the height at glTF z is 1 - z.
        z = s                      # glTF z
        h = (1 - z) if up else z
        along = Vector((0, -1, -1)) if up else Vector((0, -1, 1))
        return Vector((0.5, -z, h)), along, Vector((1, 0, 0))
    return path


def corner(mirror):
    # Two arcs about glTF (1, 0), from the side x = 1 to the side z = 0: the
    # run comes in by z... the centreline is radius 0.5.
    def path(s):
        th = s * math.pi / 2
        x, z = 1 - 0.5 * math.sin(th), 0.5 * math.cos(th)
        dx, dz = -math.cos(th), -math.sin(th)
        if mirror:
            x, dx = 1 - x, -dx
        c = Vector((x, -z, 0.0))
        along = Vector((dx, -dz, 0.0))
        across = Vector((-along.y, along.x, 0.0)) if not mirror else Vector((along.y, -along.x, 0.0))
        return c, along, across
    return path


def bank(m, up):
    # An embankment under a slope: a wedge of stone to the rails' bed.
    s = m.slot("bank")
    vs = []
    for x in (0.06, 0.94):
        if up:
            vs.append([m.bm.verts.new((x, 0, 0)), m.bm.verts.new((x, -1, 0)), m.bm.verts.new((x, 0, 1))])
        else:
            vs.append([m.bm.verts.new((x, 0, 0)), m.bm.verts.new((x, -1, 0)), m.bm.verts.new((x, -1, 1))])
    a, b = vs
    for f in ((a[0], a[1], a[2]), (b[2], b[1], b[0]), (a[0], b[0], b[1], a[1]), (a[1], b[1], b[2], a[2]),
              (a[2], b[2], b[0], a[0])):
        face = m.bm.faces.new(f)
        face.material_index = s


def lever(m, thrown):
    # A ground frame by the track's right, its lever forward (straight) or back
    # (set to turn), and a disc that shows which: green or amber.
    fr = Matrix.Identity(3)
    m.box((0.955, -0.5, 0.05), (0.07, 0.2, 0.1), fr, "lever")
    tilt = Matrix.Rotation(-0.6 if thrown else 0.6, 3, "X")
    m.box(Vector((0.955, -0.5, 0.1)) + tilt @ Vector((0, 0, 0.13)), (0.02, 0.02, 0.26), tilt, "lever")
    m.box(Vector((0.955, -0.5, 0.1)) + tilt @ Vector((0, 0, 0.27)), (0.04, 0.04, 0.04), tilt, "lever")
    m.box((0.955, -0.32, 0.28), (0.012, 0.012, 0.28), fr, "lever")
    m.box((0.955, -0.32, 0.43), (0.05, 0.1, 0.1), fr, "amber" if thrown else "green")


def build():
    out = {}
    m = Model("rail"); track(m, straight); out["rail"] = m.finish()
    m = Model("powered_rail"); track(m, straight, "gold", "gold", contact="contact"); out["powered_rail"] = m.finish()
    m = Model("powered_rail_on"); track(m, straight, "gold", "gold", contact="contact_on"); out["powered_rail_on"] = m.finish()
    m = Model("rail_right"); track(m, corner(False), tie_len=0.8); out["rail_right"] = m.finish()
    m = Model("rail_left"); track(m, corner(True), tie_len=0.8); out["rail_left"] = m.finish()
    for name, up in (("rail_up", True), ("rail_down", False)):
        m = Model(name)
        bank(m, up)
        track(m, slope(up), ties=6)
        out[name] = m.finish()
    for name, thrown in (("switch", False), ("switch_on", True)):
        m = Model(name)
        track(m, straight)
        track(m, corner(False), ties=3, tie_len=0.8)
        lever(m, thrown)
        out[name] = m.finish()
    return out


def main(export=True):
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for me in list(bpy.data.meshes):
        bpy.data.meshes.remove(me)
    models = build()
    # Side by side to look at, a cell apart; exported each at the origin.
    for i, ob in enumerate(models.values()):
        ob.location = (i * 1.3, 0, 0)
    if not export:
        return models
    os.makedirs(OUT, exist_ok=True)
    for name, ob in models.items():
        at = ob.location.copy()
        ob.location = (0, 0, 0)
        for o in bpy.data.objects:
            o.select_set(o is ob)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, name + ".gltf"), export_format="GLTF_SEPARATE",
                                  use_selection=True, export_yup=True, export_apply=True)
        ob.location = at
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    return models


if __name__ == "__main__":
    main()
