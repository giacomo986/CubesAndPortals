# The models of games/chambers besides the guns (tools/make_portal_gun.py),
# made in Blender:
#
#   cube.gltf        the storage cube in the manner of the storage cube: a
#                    worker creature 0.7 across, its feet at 0, skinned to one
#                    bone that its "death" clip shrinks to nothing (every
#                    creature has one: it fizzles).
#   door_shut.gltf   the automatic door, two cells wide, two tall and one deep,
#   door_open.gltf   drawn from its lower-left cell: two leaves meeting in the
#                    middle in a frame, and open only the frame, the leaves
#                    gone into the wall.
#   door_1..4.gltf   the door between the two, its leaves a fifth, two, three
#                    and four fifths of the way into the wall: the frames
#                    chambers_game steps through as a door opens and shuts (a
#                    block's model does not move).
#   button_up.gltf   the floor button, up and pressed: two cells by two, drawn
#   button_down.gltf from its lower corner cell, a red cap in a dark groove on
#                    a white plinth.
#   pushbutton.gltf  the push button on its stalk, a red dome on a dark head.
#   plate.gltf       the launch plate, two cells by two, drawn from its
#                    lower corner cell: a dark body in the floor, a yellow and
#                    black border, a grey plate hinged at its back with a
#                    white arrow the way it throws (+X).
#   hidden.gltf      a triangle inside a cell, for the door's other three cells
#                    (a block with a model draws its model; these draw nothing).
#   vent.gltf        the vent a cube drops out of, worn by a ceiling cell: a
#                    plate on the ceiling and a white tube hanging a metre and
#                    a bit into the room, open at the bottom.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_chambers_models.py"
#
# A block's model is authored in its cell, 0..1 each way with its feet at 0
# (glTF's x, y up, z); a creature's about its middle, its feet at 0. Blender is
# Z up: glTF (x, y, z) is Blender (x, -z, y). Leaves the work in
# ~/kart_assets/chambers.blend.
import json
import math
import os

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
CORE = os.path.join(ROOT, "games", "chambers", "mods", "chambers_core")
GAME = os.path.join(ROOT, "games", "chambers", "mods", "chambers_game")
BLEND = os.path.expanduser("~/kart_assets/chambers.blend")


def material(name, rgb, metal=0.0, rough=0.6, glow=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    if glow > 0:
        bsdf.inputs["Emission Color"].default_value = (*rgb, 1.0)
        bsdf.inputs["Emission Strength"].default_value = glow
    m.diffuse_color = (*rgb, 1.0)
    return m


def placed(bm, at=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    m = Matrix.Translation(at) @ Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()
    m = m @ Matrix.Diagonal((*scale, 1.0))
    bmesh.ops.transform(bm, matrix=m, verts=bm.verts)
    return bm


def box(lo, hi, bevel=0.0, segs=2):
    """Blender coordinates, corner to corner."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    size = [h - l for l, h in zip(lo, hi)]
    bmesh.ops.scale(bm, vec=size, verts=bm.verts)
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=bm.edges[:] + bm.verts[:], offset=bevel, segments=segs,
                        affect="EDGES", profile=0.5)
    return placed(bm, at=[(l + h) / 2 for l, h in zip(lo, hi)])


def disc(r, thick, segs=24, **kw):
    """A disc lying in XY, `thick` along Z."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r, radius2=r, depth=thick)
    return placed(bm, **kw)


def tube(r_out, r_in, length, segs=28, **kw):
    """Hollow, along Z from 0 down to -length."""
    bm = bmesh.new()
    rows = []
    for z in (0.0, -length):
        rows.append(([bm.verts.new((math.cos(a) * r_out, math.sin(a) * r_out, z)) for a in
                      (2 * math.pi * k / segs for k in range(segs))],
                     [bm.verts.new((math.cos(a) * r_in, math.sin(a) * r_in, z)) for a in
                      (2 * math.pi * k / segs for k in range(segs))]))
    (ot, it), (ob, ib) = rows
    for k in range(segs):
        n = (k + 1) % segs
        bm.faces.new((ot[k], ot[n], ob[n], ob[k]))   # outside
        bm.faces.new((it[n], it[k], ib[k], ib[n]))   # inside
        bm.faces.new((ot[n], ot[k], it[k], it[n]))   # top rim
        bm.faces.new((ob[k], ob[n], ib[n], ib[k]))   # bottom rim
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return placed(bm, **kw)


def heart(size, thick):
    """A heart in XY, `thick` along Z: two discs and a point."""
    bm = bmesh.new()
    r = size * 0.3
    for cx in (-r * 0.85, r * 0.85):
        bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=r, radius2=r, depth=thick,
                              matrix=Matrix.Translation((cx, r * 0.4, 0)))
    pts = [(-size * 0.53, r * 0.25), (size * 0.53, r * 0.25), (0, -size * 0.6)]
    vs = [bm.verts.new((x, y, z)) for z in (-thick / 2, thick / 2) for x, y in pts]
    bm.faces.new(vs[:3][::-1])
    bm.faces.new(vs[3:])
    for k in range(3):
        n = (k + 1) % 3
        bm.faces.new((vs[k], vs[n], vs[3 + n], vs[3 + k]))
    return bm


def mesh_object(name, parts, mats):
    objs = []
    for bm, mat in parts:
        me = bpy.data.meshes.new(name + "_part")
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mats[mat])
        o = bpy.data.objects.new(name + "_part", me)
        bpy.context.scene.collection.objects.link(o)
        objs.append(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    return obj


def export(objs, path, anims=False, skins=False):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=anims, export_yup=True, export_apply=False,
                              export_materials="EXPORT", export_image_format="NONE",
                              export_skins=skins)


MATS = None


def mats():
    global MATS
    if MATS is None:
        MATS = {
            "grey": material("AP Grey", (0.62, 0.64, 0.67), metal=0.3, rough=0.45),
            "dark": material("AP Dark", (0.20, 0.21, 0.23), metal=0.5, rough=0.5),
            "light": material("AP Light", (0.86, 0.87, 0.88), rough=0.4),
            "pink": material("AP Pink", (0.93, 0.42, 0.66), rough=0.5, glow=0.3),
            "white": material("AP White", (0.93, 0.94, 0.95), rough=0.35),
            "black": material("AP Black", (0.06, 0.06, 0.07), rough=0.5),
            "orange": material("AP Orange", (1.0, 0.52, 0.08), rough=0.5),
            "glass": material("AP Glass", (0.55, 0.70, 0.78), metal=0.2, rough=0.1),
            "red": material("AP Red", (0.78, 0.10, 0.07), rough=0.35),
            "redlight": material("AP Red Light", (1.0, 0.25, 0.15), rough=0.3, glow=1.5),
            "yellow": material("AP Yellow", (0.95, 0.74, 0.10), rough=0.5),
        }
    return MATS


# ------------------------------------------------------------------ the cube

def cube():
    m = mats()
    S = 0.35
    parts = [(box((-S + 0.03, -S + 0.03, 0.03), (S - 0.03, S - 0.03, 2 * S - 0.03), bevel=0.035, segs=3),
              "grey")]
    # The eight corners, capped dark.
    c = 0.13
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (0, 1):
                x0 = -S if sx < 0 else S - c
                y0 = -S if sy < 0 else S - c
                z0 = 0 if sz == 0 else 2 * S - c
                parts.append((box((x0, y0, z0), (x0 + c, y0 + c, z0 + c), bevel=0.02), "dark"))
    # A pale round plate on each face, a pink heart on it, upright: on a side
    # its top towards the cube's top, on the top and the bottom along +Y.
    def on_face(bm, n, out):
        z = Vector(n)
        y = Vector((0, 0, 1)) if abs(z.z) < 0.5 else Vector((0, 1, 0))
        x = y.cross(z)
        basis = Matrix((x, y, z)).transposed().to_4x4()
        bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector((0, 0, S)) + z * out) @ basis, verts=bm.verts)
        return bm
    for n in ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)):
        parts.append((on_face(disc(0.15, 0.02), n, S - 0.005), "light"))
        parts.append((on_face(heart(0.16, 0.012), n, S + 0.012), "pink"))
    obj = mesh_object("cube", parts, m)
    bpy.ops.object.shade_smooth()

    # One bone, every vertex on it; "death" scales it to nothing in 0.4 s.
    arm_data = bpy.data.armatures.new("cube_rig")
    arm = bpy.data.objects.new("cube_rig", arm_data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    b = arm_data.edit_bones.new("root")
    b.head = (0, 0, 0)
    b.tail = (0, 0, 0.35)
    bpy.ops.object.mode_set(mode="OBJECT")
    vg = obj.vertex_groups.new(name="root")
    vg.add(list(range(len(obj.data.vertices))), 1.0, "REPLACE")
    obj.parent = arm
    mod = obj.modifiers.new("rig", "ARMATURE")
    mod.object = arm
    scene = bpy.context.scene
    scene.render.fps = 24
    arm.animation_data_create()
    act = bpy.data.actions.new("death")
    arm.animation_data.action = act
    pb = arm.pose.bones["root"]
    pb.scale = (1, 1, 1)
    pb.keyframe_insert("scale", frame=1)
    pb.scale = (0.01, 0.01, 0.01)
    pb.keyframe_insert("scale", frame=1 + round(0.4 * 24))
    pb.scale = (1, 1, 1)
    path = os.path.join(GAME, "cube.gltf")
    export([arm, obj], path, anims=True, skins=True)
    # The clip by the name the engine plays it by.
    g = json.load(open(path))
    for a in g.get("animations", []):
        a["name"] = "death"
    json.dump(g, open(path, "w"), separators=(",", ":"))
    return [arm, obj]


# ------------------------------------------------------------------ the door
# In Blender: x 0..2 across, z 0..2 up, y 0..-1 deep (glTF z 0..1).

DOOR_FRAMES = 4   # between shut and open


def door(f, name):
    """`f` 0 shut .. 1 open: the leaves gone that far into the wall."""
    m = mats()
    parts = []
    t = 0.12   # the frame's thickness, lining the doorway
    parts += [(box((0, -1, 0), (t, 0, 2)), "dark"), (box((2 - t, -1, 0), (2, 0, 2)), "dark"),
              (box((0, -1, 2 - t), (2, 0, 2)), "dark"), (box((0, -1, 0), (2, 0, 0.04)), "dark")]
    # A white trim round the frame's mouth on both faces.
    for y in (-0.02, -0.98):
        parts += [(box((t, y - 0.02, 2 - t - 0.06), (2 - t, y + 0.02, 2 - t)), "white"),
                  (box((t, y - 0.02, 0.04), (t + 0.06, y + 0.02, 2 - t)), "white"),
                  (box((2 - t - 0.06, y - 0.02, 0.04), (2 - t, y + 0.02, 2 - t)), "white")]
    if f < 1:
        slide = f * (1.0 - t)   # how far each leaf has gone into the wall
        for side in (0, 1):
            # The leaf where it is, cut at the frame: what is in the wall is
            # not drawn.
            d = -slide if side == 0 else slide
            x0, x1 = (t + d, 1.0 + d) if side == 0 else (1.0 + d, 2 - t + d)
            v0, v1 = max(x0, t), min(x1, 2 - t)
            parts.append((box((v0, -0.56, 0.04), (v1, -0.44, 2 - t), bevel=0.01), "grey"))
            # A black and orange band across, a round window above it.
            parts.append((box((v0, -0.58, 0.85), (v1, -0.42, 1.0)), "black"))
            parts.append((box((v0, -0.585, 0.9), (v1, -0.415, 0.95)), "orange"))
            cx = (x0 + x1) / 2
            if t + 0.18 <= cx <= 2 - t - 0.18:
                for y, rot in ((-0.565, 90), (-0.435, 90)):
                    parts.append((disc(0.18, 0.02, at=(cx, y, 1.42), rot=(rot, 0, 0)), "dark"))
                    parts.append((disc(0.13, 0.025, at=(cx, y, 1.42), rot=(rot, 0, 0)), "glass"))
            # The leaf's edge, where they meet when shut.
            e = x1 if side == 0 else x0
            if t < e < 2 - t:
                parts.append((box((e - 0.015, -0.58, 0.04), (e + 0.015, -0.42, 2 - t)), "dark"))
    obj = mesh_object(name, parts, m)
    export([obj], os.path.join(CORE, name + ".gltf"))
    return obj


# ------------------------------------------------------------------ the buttons
# The floor button is two cells by two, drawn from its lower corner as the
# plate is: x 0..2, y 0..-2, z up. A white plinth as high as the blocks' boxes
# (0.15, up or pressed alike, so nothing on it jumps as it goes down), a dark
# groove round a red cap that stands 0.12 over it and sinks to 0.02 pressed,
# four dark feet at the corners, a glowing ring on the cap.
BUTTON_TOP = 0.15


def button(down):
    m = mats()
    c = (1.0, -1.0)
    cap = BUTTON_TOP + (0.02 if down else 0.12)
    parts = [
        (box((0.03, -1.97, 0.0), (1.97, -0.03, BUTTON_TOP), bevel=0.04, segs=3), "white"),  # the plinth
        (disc(0.86, 0.02, segs=48, at=(*c, BUTTON_TOP + 0.005)), "dark"),                   # the groove
        (disc(0.92, 0.01, segs=48, at=(*c, BUTTON_TOP + 0.002)), "grey"),                    # its lip
        (disc(0.74, cap - BUTTON_TOP + 0.01, segs=48,
              at=(*c, (cap + BUTTON_TOP - 0.01) / 2)), "red"),                               # the cap
        (disc(0.62, 0.012, segs=48, at=(*c, cap + 0.002)), "red"),
        (tube(0.56, 0.48, 0.012, segs=48, at=(*c, cap + 0.009)), "redlight"),               # its ring
    ]
    for x, y in ((0.0, 0.0), (2.0, 0.0), (0.0, -2.0), (2.0, -2.0)):                         # the feet
        sx, sy = (0.03 if x == 0 else 1.75), (-0.25 if y == 0 else -1.97)
        parts.append((box((sx, sy, BUTTON_TOP - 0.02), (sx + 0.22, sy + 0.22, BUTTON_TOP + 0.01),
                          bevel=0.01), "dark"))
    name = "button_down" if down else "button_up"
    obj = mesh_object(name, parts, m)
    export([obj], os.path.join(CORE, name + ".gltf"))
    return obj


def pushbutton():
    m = mats()
    parts = [
        (disc(0.3, 0.05, at=(0.5, -0.5, 0.025)), "dark"),                    # the foot
        (tube(0.1, 0.0, 0.8, at=(0.5, -0.5, 0.85)), "white"),               # the stalk
        (disc(0.24, 0.1, at=(0.5, -0.5, 0.9)), "dark"),                      # the head
        (disc(0.25, 0.02, at=(0.5, -0.5, 0.86)), "white"),
        (disc(0.16, 0.06, at=(0.5, -0.5, 0.97)), "red"),                     # the button
        (disc(0.11, 0.02, at=(0.5, -0.5, 1.0)), "redlight"),
    ]
    obj = mesh_object("pushbutton", parts, m)
    bpy.ops.object.shade_smooth()
    export([obj], os.path.join(CORE, "pushbutton.gltf"))
    return obj


# ------------------------------------------------------------------ the plate
# Two cells by two, drawn from its lower corner: x 0..2, y 0..-2, z 0..1, the
# top flush with the floor at z 1.

def plate():
    m = mats()
    parts = [(box((0.0, -2.0, 0.0), (2.0, 0.0, 0.98)), "dark")]
    # The border, yellow and black by turns.
    n, w = 8, 0.16
    for k in range(n):
        a, b = 2.0 * k / n, 2.0 * (k + 1) / n
        c = "yellow" if k % 2 == 0 else "black"
        parts += [(box((a, -w, 0.98), (b, 0.0, 1.0)), c), (box((a, -2.0, 0.98), (b, -2.0 + w, 1.0)), c),
                  (box((0.0, -b, 0.98), (w, -a, 1.0)), c), (box((2.0 - w, -b, 0.98), (2.0, -a, 1.0)), c)]
    # The plate, a hinge at its back, the arrow.
    parts.append((box((0.24, -1.76, 0.98), (1.76, -0.24, 1.02), bevel=0.01), "grey"))
    parts.append((box((0.2, -1.8, 0.96), (0.3, -0.2, 1.04)), "dark"))
    arrow = [(0.55, -0.88), (1.15, -0.88), (1.15, -0.62), (1.55, -1.0), (1.15, -1.38), (1.15, -1.12),
             (0.55, -1.12)]
    bm = bmesh.new()
    vs = [bm.verts.new((x, y, z)) for z in (1.02, 1.03) for x, y in arrow]
    k = len(arrow)
    bm.faces.new(vs[:k][::-1])
    bm.faces.new(vs[k:])
    for i in range(k):
        j = (i + 1) % k
        bm.faces.new((vs[i], vs[j], vs[k + j], vs[k + i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    parts.append((bm, "white"))
    obj = mesh_object("plate", parts, m)
    export([obj], os.path.join(CORE, "plate.gltf"))
    return obj


def hidden():
    bm = bmesh.new()
    vs = [bm.verts.new(p) for p in ((0.5, -0.5, 0.5), (0.501, -0.5, 0.5), (0.5, -0.5, 0.501))]
    bm.faces.new(vs)
    obj = mesh_object("hidden", [(bm, "dark")], mats())
    export([obj], os.path.join(CORE, "hidden.gltf"))
    return obj


# ------------------------------------------------------------------ the vent
# Worn by a ceiling cell: x, y 0..1 across, z 0..1 its cell; the tube hangs
# from the cell's floor (z 0) down to z -1.15.

def vent():
    m = mats()
    parts = [
        (box((0.02, -0.98, 0.0), (0.98, -0.02, 0.06), bevel=0.01), "white"),          # the plate
        (disc(0.37, 0.02, at=(0.5, -0.5, 0.07)), "dark"),                            # its roof
        (tube(0.42, 0.36, 1.15, at=(0.5, -0.5, 0.0)), "white"),                      # the tube
        (tube(0.36, 0.34, 1.13, at=(0.5, -0.5, 0.0)), "dark"),                       # its inside
        (tube(0.45, 0.33, 0.1, at=(0.5, -0.5, -1.05)), "dark"),                      # the collar
        (tube(0.44, 0.40, 0.04, at=(0.5, -0.5, -0.5)), "dark"),                      # a band
        (tube(0.445, 0.40, 0.025, at=(0.5, -0.5, -0.8)), "orange"),
    ]
    obj = mesh_object("vent", parts, m)
    bpy.ops.object.shade_smooth()
    export([obj], os.path.join(CORE, "vent.gltf"))
    return obj


bpy.ops.wm.read_factory_settings(use_empty=True)
made = cube() + [door(0.0, "door_shut"), door(1.0, "door_open")]
made += [door(k / (DOOR_FRAMES + 1), f"door_{k}") for k in range(1, DOOR_FRAMES + 1)]
made += [button(False), button(True), pushbutton(), plate(), hidden(), vent()]
# Laid out apart in the work file.
for i, o in enumerate(o for o in made if o.parent is None):
    o.location.x += i * 3
os.makedirs(os.path.dirname(BLEND), exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=BLEND)
print("chambers models:", ", ".join(o.name for o in made))
