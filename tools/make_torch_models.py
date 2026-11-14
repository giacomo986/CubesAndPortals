# The torch's models, for mods/torches: in the hand, and put down on a floor,
# on a wall and under a ceiling.
#
#   blender --background --python tools/make_torch_models.py
#
# torch_held is made about the grip as every held thing is (see
# tools/make_held_models.py): the origin the middle of the fist, the stick
# along +Y out of it. The three placed ones are made inside the unit cube of
# their cell with its corner at the origin (core.node_model): game x, y, z is
# Blender x, -z... written below through G(), so the numbers read as the
# game's. The wall one leans out of the cell's -Z face, which a wallmounted
# block turns to whichever face it was put against.
#
# The flame is two glowing materials, so it is bright in the dark the way the
# light it gives is. Leaves the work in assets/blender/torches.blend.
import math
import os

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "torches")
BLEND = os.path.join(ROOT, "assets", "blender", "torches.blend")


def material(name, rgb, metal=0.0, rough=0.8, glow=False):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = metal
    bsdf.inputs["Roughness"].default_value = rough
    if glow:
        bsdf.inputs["Emission Color"].default_value = (*rgb, 1.0)
        bsdf.inputs["Emission Strength"].default_value = 1.0
    m.diffuse_color = (*rgb, 1.0)
    return m


def placed(bm, at=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    m = Matrix.Translation(at) @ Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()
    m = m @ Matrix.Diagonal((*scale, 1.0))
    bmesh.ops.transform(bm, matrix=m, verts=bm.verts)
    return bm


def box(size, **kw):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=size, verts=bm.verts)
    return placed(bm, **kw)


def cyl(r, length, segs=8, r2=None, **kw):
    """Along +Y, from y = 0 to `length`."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r,
                          radius2=r if r2 is None else r2, depth=length)
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(math.radians(-90), 3, "X"))
    bmesh.ops.translate(bm, verts=bm.verts, vec=(0, length / 2, 0))
    return placed(bm, **kw)


def flame(r, tall, **kw):
    """A drop of fire: a sphere drawn up into a point along +Y."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=8, radius=r)
    for v in bm.verts:
        if v.co.z > 0:
            t = v.co.z / r
            v.co.x *= 1.0 - t * 0.8
            v.co.y *= 1.0 - t * 0.8
            v.co.z *= 1.0 + t * (tall / r - 1.0)
    # Z up to Y up, so it points the way the stick does.
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(math.radians(-90), 3, "X"))
    return placed(bm, **kw)


def torch_parts(stick=0.34, r=0.022):
    """The torch upright about the bottom of its stick, along +Y: a stick, a
    lump of coal wrapped round its top and the flame over it."""
    return [
        (cyl(r, stick, segs=8), "wood"),
        (box((r * 3.0, 0.07, r * 3.0), at=(0, stick + 0.02, 0)), "coal"),
        (box((r * 2.2, 0.03, r * 2.2), at=(0, stick - 0.02, 0)), "band"),
        (flame(0.042, 0.10, at=(0, stick + 0.075, 0)), "fire"),
        (flame(0.024, 0.065, at=(0, stick + 0.07, 0)), "core"),
    ]


def build(name, parts, mats):
    whole = bmesh.new()
    order = []
    for bm, mname in parts:
        if mname not in order:
            order.append(mname)
        mi = order.index(mname)
        me = bpy.data.meshes.new("tmp")
        bm.to_mesh(me)
        bm.free()
        for p in me.polygons:
            p.material_index = mi
            p.use_smooth = False
        whole.from_mesh(me)
        bpy.data.meshes.remove(me)
    me = bpy.data.meshes.new(name)
    whole.to_mesh(me)
    whole.free()
    for mname in order:
        me.materials.append(mats[mname])
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def into_cell(parts, base, lean=0.0):
    """Parts made upright along +Y (Blender), stood in a cell: the bottom of the
    stick at `base` in the game's x, y, z, leant `lean` degrees out towards +Z
    (the game's). Blender's +Y is the game's -Z and its +Z the game's +Y."""
    out = []
    up = Matrix.Rotation(math.radians(90), 4, "X")       # Blender +Y up to +Z: the game's up
    tilt = Matrix.Rotation(math.radians(lean), 4, "X")   # top towards Blender -Y: the game's +Z
    at = Matrix.Translation((base[0], -base[2], base[1]))
    for bm, m in parts:
        bmesh.ops.transform(bm, matrix=at @ tilt @ up, verts=bm.verts)
        out.append((bm, m))
    return out


def export(obj, file):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, file), export_format="GLTF_SEPARATE",
                              use_selection=True, export_animations=False, export_yup=True,
                              export_materials="EXPORT", export_image_format="NONE",
                              export_skins=False)


bpy.ops.wm.read_homefile(use_empty=True)
M = {
    "wood": material("TorchWood", (0.55, 0.38, 0.22)),
    "coal": material("TorchCoal", (0.16, 0.14, 0.13), 0.0, 0.7),
    "band": material("TorchBand", (0.30, 0.20, 0.12)),
    "fire": material("TorchFire", (1.00, 0.62, 0.15), glow=True),
    "core": material("TorchCore", (1.00, 0.93, 0.55), glow=True),
    "iron": material("TorchIron", (0.30, 0.30, 0.33), 0.8, 0.45),
}

# In the hand: the stick from under the fist up, made about the grip.
held = [(placed(bm, at=(0, -0.10, 0)), m) for bm, m in torch_parts(0.28, 0.02)]
export(build("torch_held", held, M), "torch_held.gltf")

# On a floor: upright in the middle of the cell.
export(build("torch", into_cell(torch_parts(0.5, 0.0625 / 2), (0.5, 0.0, 0.5)), M), "torch.gltf")

# On a wall: out of the -Z face, a third of the way up, leaning out.
wall = into_cell(torch_parts(0.46, 0.0625 / 2), (0.5, 0.18, 0.04), lean=32.0)
wall.append((box((0.12, 0.03, 0.10), at=(0.5, -0.05, 0.2)), "iron"))   # the bracket (game 0.5, 0.2, 0.05)
export(build("torch_wall", wall, M), "torch_wall.gltf")

# Under a ceiling: an iron rod down from it, a ring, and the torch stood in it.
hang = into_cell(torch_parts(0.36, 0.0625 / 2), (0.5, 0.26, 0.5))
hang.append((box((0.03, 0.03, 0.46), at=(0.64, -0.5, 0.77)), "iron"))    # beside the flame, game y 0.54..1.0
hang.append((box((0.09, 0.09, 0.025), at=(0.5, -0.5, 0.56)), "iron"))    # the ring round the stick
hang.append((box((0.14, 0.025, 0.025), at=(0.57, -0.5, 0.56)), "iron"))  # its arm out to the rod
export(build("torch_ceiling", hang, M), "torch_ceiling.gltf")

os.makedirs(os.path.dirname(BLEND), exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=BLEND)
print("torch models written to", OUT)
