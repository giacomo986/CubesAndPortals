# The two portal guns of games/chambers, held and as pictures, modelled in
# Blender: the gun that shoots blue (chambers_game:gun_blue) and the whole one
# (chambers_game:gun). A white shell over the back half, a black core and
# barrel, three black prongs opening round a glowing tip, the grip under it;
# a light strip down each side, blue on the first gun, blue and orange on the
# second.
#
#   flatpak run org.blender.Blender --background --python tools/make_portal_gun.py
#
# Held as tools/make_held_models.py holds things: the origin is the middle of
# the fist, +Y the way the gun points, +Z the side seen in first person; sizes
# in metres of the robot, scaled by its forearm. Also writes each gun's 32x32
# picture into games/chambers/textures, and leaves the work in
# ~/kart_assets/portal_gun.blend.
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Euler, Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
MOD = os.path.join(ROOT, "games", "chambers", "mods", "chambers_game")
TEXTURES = os.path.join(ROOT, "games", "chambers", "textures")
BLEND = os.path.expanduser("~/kart_assets/portal_gun.blend")


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


def box(size, bevel=0.0, **kw):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=size, verts=bm.verts)
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=bm.edges[:] + bm.verts[:], offset=bevel, segments=3,
                        affect="EDGES", profile=0.5)
    return placed(bm, **kw)


def cyl(r, length, segs=16, r2=None, cap=True, **kw):
    """Along +Y, from y = 0 to `length`; r2 the far end's radius (a cone)."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=cap, segments=segs, radius1=r,
                          radius2=r if r2 is None else r2, depth=length)
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(math.radians(-90), 3, "X"))
    bmesh.ops.translate(bm, verts=bm.verts, vec=(0, length / 2, 0))
    return placed(bm, **kw)


def ball(r, segs=16, rings=10, **kw):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=r)
    return placed(bm, **kw)


def shell(**kw):
    """The white back: a capsule, flattened under, wider at the back."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=20, v_segments=12, radius=1.0)
    for v in bm.verts:
        # Long along Y, the back (low y) fuller than the front.
        t = (v.co.y + 1) / 2                       # 0 at the back, 1 at the front
        w = 1.0 - 0.35 * t
        v.co.x *= 0.062 * w
        v.co.z *= 0.068 * w
        v.co.y *= 0.15
        if v.co.z < -0.02:                          # flat under, where the grip is
            v.co.z = -0.02 + (v.co.z + 0.02) * 0.4
    return placed(bm, **kw)


def build(name, strips):
    mats = {
        "white": material("PG White", (0.92, 0.93, 0.94), rough=0.35),
        "black": material("PG Black", (0.07, 0.07, 0.08), metal=0.6, rough=0.4),
        "grey": material("PG Grey", (0.42, 0.44, 0.47), metal=0.5, rough=0.45),
        "tip": material("PG Tip", (0.85, 0.95, 1.0), glow=3.0),
        "blue": material("PG Blue", (0.16, 0.51, 1.0), glow=2.5),
        "orange": material("PG Orange", (1.0, 0.55, 0.08), glow=2.5),
    }
    parts = [
        (shell(at=(0, 0.02, 0.035)), "white"),
        # The grip under the shell, where the fist is, raked back.
        (box((0.034, 0.05, 0.11), bevel=0.008, at=(0, -0.01, -0.035), rot=(-12, 0, 0)), "black"),
        # The core, out of the shell's front, and the barrel round it.
        (cyl(0.034, 0.12, at=(0, 0.11, 0.035)), "black"),
        (cyl(0.042, 0.035, at=(0, 0.13, 0.035)), "grey"),
        (cyl(0.026, 0.06, r2=0.02, at=(0, 0.23, 0.035)), "grey"),
        # The tip, glowing.
        (ball(0.022, at=(0, 0.29, 0.035)), "tip"),
        # A collar between the shell and the core.
        (cyl(0.05, 0.012, at=(0, 0.115, 0.035)), "grey"),
    ]
    # Three prongs round the tip, opening outwards.
    for k in range(3):
        a = math.radians(90 + 120 * k)
        cx, cz = math.cos(a) * 0.052, math.sin(a) * 0.052
        parts.append((box((0.012, 0.11, 0.016), bevel=0.003, at=(cx, 0.25, 0.035 + cz),
                          rot=(math.sin(a) * 15, 0, -math.cos(a) * 15)), "black"))
    # The light strips down each side of the shell.
    for i, colour in enumerate(strips):
        for side in (-1, 1):
            # On the shell's flank, where it is 0.054 wide each way.
            parts.append((box((0.006, 0.12, 0.008), at=(side * 0.052, -0.02, 0.042 - i * 0.016)), colour))

    meshes = []
    for bm, mat in parts:
        me = bpy.data.meshes.new(f"{name}_part")
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mats[mat])
        meshes.append(me)
    objs = []
    for me in meshes:
        o = bpy.data.objects.new(name + "_p", me)
        bpy.context.scene.collection.objects.link(o)
        objs.append(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    bpy.ops.object.shade_smooth()
    return obj


def export(obj, name):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    # Pointed into the screen, not up it as a tool is: the barrel (+Y) laid
    # forward, away from the eye (-Z), a little up; the grip down.
    # And swung right about the screen's up, so it points along the view and
    # not across it from the right hand.
    obj.rotation_euler = (math.radians(-80), math.radians(-20), 0)
    bpy.ops.export_scene.gltf(filepath=os.path.join(MOD, f"{name}_held.gltf"),
                              export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_yup=True, export_materials="EXPORT",
                              export_image_format="NONE", export_skins=False)


def render(obj, out, turn, size):
    scene = bpy.context.scene
    try:
        scene.render.engine = "BLENDER_WORKBENCH"
    except TypeError as e:
        print("workbench:", e)
    scene.display.shading.light = "STUDIO"
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = "BOTH"
    scene.display.shading.color_type = "MATERIAL"
    scene.render.film_transparent = True
    scene.render.resolution_x = scene.render.resolution_y = size
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    for o in scene.objects:
        o.hide_render = o is not obj
    obj.rotation_euler = [math.radians(a) for a in turn]
    bpy.context.view_layer.update()
    pts = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    centre = sum(pts, Vector()) / 8
    extent = max(max(p.x for p in pts) - min(p.x for p in pts), max(p.y for p in pts) - min(p.y for p in pts))
    cam = bpy.data.objects.get("IconCam")
    if cam is None:
        cam = bpy.data.objects.new("IconCam", bpy.data.cameras.new("IconCam"))
        scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = extent * 1.15
    cam.location = centre + Vector((0, 0, 1.0))
    cam.rotation_euler = (0, 0, 0)
    scene.camera = cam
    scene.render.filepath = out
    bpy.ops.render.render(write_still=True)


def icon(obj, out):
    """Corner to corner like the tools' pictures, 256 down to 32, outlined."""
    render(obj, out, (0, 0, -45), 256)
    img = bpy.data.images.load(out)
    img.scale(32, 32)
    px = np.array(img.pixels[:]).reshape(32, 32, 4)
    solid = px[:, :, 3] > 0.45
    px[:, :, 3] = solid.astype(float)
    edge = np.zeros_like(solid)
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        edge |= np.roll(solid, (dy, dx), axis=(0, 1))
    edge &= ~solid
    px[edge] = (0.10, 0.07, 0.07, 1.0)
    img.pixels[:] = px.ravel()
    img.filepath_raw = out
    img.file_format = "PNG"
    img.save()


bpy.ops.wm.read_factory_settings(use_empty=True)
guns = {"gun_blue": build("gun_blue", ["blue"]), "gun": build("gun", ["blue", "orange"])}
for name, obj in guns.items():
    export(obj, name)
    obj.rotation_euler = (0, 0, 0)
    icon(obj, os.path.join(TEXTURES, f"chambers_game_{name}.png"))
    preview = os.environ.get("PREVIEW")
    if preview:
        render(obj, os.path.join(preview, f"{name}_side.png"), (0, 0, -90), 512)
        render(obj, os.path.join(preview, f"{name}_three.png"), (-60, 0, -130), 512)
    obj.rotation_euler = (0, 0, 0)
os.makedirs(os.path.dirname(BLEND), exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=BLEND)
print("portal guns:", ", ".join(guns))
