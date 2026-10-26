# The rope gun of games/parkour (parkour:rope_gun), held and as a picture,
# modelled in Blender: a dark body with a grip under it, a drum of orange rope
# on its left side and a three-pronged steel hook in the muzzle.
#
#   flatpak run org.blender.Blender --background --python tools/make_rope_gun.py
#
# (or run through the Blender MCP server). Held as tools/make_portal_gun.py
# holds its guns: the origin is the middle of the fist, +Y the way the gun
# points, +Z the side seen in first person. Writes rope_gun_held.gltf into the
# mod, its 32x32 picture into games/parkour/textures, and leaves the work in
# assets/blender/rope_gun.blend.
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Euler, Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
MOD = os.path.join(ROOT, "games", "parkour", "mods", "parkour")
TEXTURES = os.path.join(ROOT, "games", "parkour", "textures")
BLEND = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "rope_gun.blend")


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


def build():
    mats = {
        "body": material("RG Body", (0.10, 0.11, 0.13), metal=0.4, rough=0.45),
        "steel": material("RG Steel", (0.62, 0.64, 0.67), metal=0.9, rough=0.3),
        "rope": material("RG Rope", (1.0, 0.45, 0.06), rough=0.8),
        "grip": material("RG Grip", (0.20, 0.20, 0.22), rough=0.9),
        "light": material("RG Light", (0.2, 1.0, 0.45), glow=2.5),
    }
    parts = [
        # The body, long along the barrel.
        (box((0.06, 0.24, 0.07), bevel=0.01, at=(0, 0.06, 0.035)), "body"),
        # The grip under it, where the fist is, raked back, and a trigger guard.
        (box((0.034, 0.05, 0.11), bevel=0.008, at=(0, -0.01, -0.035), rot=(-12, 0, 0)), "grip"),
        (box((0.01, 0.05, 0.008), at=(0, 0.04, -0.02)), "steel"),
        # The barrel out of the front, and its muzzle ring.
        (cyl(0.022, 0.08, at=(0, 0.18, 0.04)), "steel"),
        (cyl(0.03, 0.02, at=(0, 0.25, 0.04)), "body"),
        # The drum of rope on the left flank, its axle across.
        (cyl(0.045, 0.03, segs=20, at=(-0.045, 0.05, 0.035), rot=(0, 0, 90)), "rope"),
        (cyl(0.05, 0.006, segs=20, at=(-0.03, 0.05, 0.035), rot=(0, 0, 90)), "steel"),
        (cyl(0.05, 0.006, segs=20, at=(-0.066, 0.05, 0.035), rot=(0, 0, 90)), "steel"),
        # A light on top that says it is loaded.
        (box((0.012, 0.05, 0.008), at=(0, 0.02, 0.073)), "light"),
        # The hook in the muzzle: a shank and three prongs bent back.
        (cyl(0.008, 0.06, at=(0, 0.26, 0.04)), "steel"),
    ]
    for k in range(3):
        a = math.radians(90 + 120 * k)
        cx, cz = math.cos(a) * 0.02, math.sin(a) * 0.02
        parts.append((box((0.008, 0.045, 0.008), at=(cx, 0.305, 0.04 + cz),
                          rot=(-math.sin(a) * 40, 0, math.cos(a) * 40)), "steel"))

    objs = []
    for bm, mat in parts:
        me = bpy.data.meshes.new("rope_gun_part")
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mats[mat])
        o = bpy.data.objects.new("rope_gun_p", me)
        bpy.context.scene.collection.objects.link(o)
        objs.append(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = "rope_gun"
    bpy.ops.object.shade_smooth()
    return obj


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    # As the portal gun: the barrel into the screen, a little up, the grip down.
    obj.rotation_euler = (math.radians(-80), math.radians(-20), 0)
    bpy.ops.export_scene.gltf(filepath=os.path.join(MOD, "rope_gun_held.gltf"),
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


# A clean scene, without a factory reset (which would take a live session
# and its MCP server with it).
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
gun = build()
export(gun)
gun.rotation_euler = (0, 0, 0)
os.makedirs(TEXTURES, exist_ok=True)
icon(gun, os.path.join(TEXTURES, "parkour_rope_gun.png"))
gun.rotation_euler = (0, 0, 0)
os.makedirs(os.path.dirname(BLEND), exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=BLEND)
print("rope gun: done")
