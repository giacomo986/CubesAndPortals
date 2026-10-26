# The art of mods/cards, made in Blender: the four suits of the Neapolitan
# deck (denari, coppe, spade, bastoni) and the twelve figures (fante, cavallo,
# re of each suit), modelled from primitives and rendered flat -- two tones
# and a black outline, as the old woodcut decks are printed -- with a clear
# background, to ~/kart_assets/cards/art/<name>.png. tools/make_cards.py lays
# them out on the forty card faces.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_card_art.py"
#
# Blender is Z up; every picture is taken from the front (-Y), orthographic.
import math
import os

import bpy
from mathutils import Vector

OUT = os.path.expanduser("~/kart_assets/cards/art")

# The deck's few inks.
INK = {
    "gold": (0.95, 0.72, 0.12),
    "red": (0.80, 0.10, 0.08),
    "blue": (0.10, 0.25, 0.70),
    "green": (0.10, 0.50, 0.20),
    "skin": (0.98, 0.80, 0.62),
    "steel": (0.75, 0.80, 0.85),
    "wood": (0.55, 0.32, 0.12),
    "leaf": (0.25, 0.65, 0.20),
    "white": (0.97, 0.95, 0.90),
    "brown": (0.40, 0.22, 0.10),
    "black": (0.08, 0.06, 0.05),
    "grey": (0.85, 0.83, 0.80),
}


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for m in list(bpy.data.meshes):
        bpy.data.meshes.remove(m)
    for c in list(bpy.data.curves):
        bpy.data.curves.remove(c)


def ink(name):
    """A flat material of two tones: lit and in shade, nothing between."""
    key = "CA " + name
    m = bpy.data.materials.get(key)
    if m:
        return m
    rgb = INK[name]
    m = bpy.data.materials.new(key)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    emit = nt.nodes.new("ShaderNodeEmission")
    diff = nt.nodes.new("ShaderNodeBsdfDiffuse")
    torgb = nt.nodes.new("ShaderNodeShaderToRGB")
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.interpolation = "CONSTANT"
    e = ramp.color_ramp.elements
    e[0].position, e[1].position = 0.0, 0.18
    e[0].color = (rgb[0] * 0.62, rgb[1] * 0.62, rgb[2] * 0.62, 1)
    e[1].color = (*rgb, 1)
    nt.links.new(diff.outputs[0], torgb.inputs[0])
    nt.links.new(torgb.outputs[0], ramp.inputs[0])
    nt.links.new(ramp.outputs[0], emit.inputs[0])
    nt.links.new(emit.outputs[0], out.inputs[0])
    m.diffuse_color = (*rgb, 1)
    return m


def put(obj, colour, smooth=True):
    obj.data.materials.clear()
    obj.data.materials.append(ink(colour))
    if smooth and hasattr(obj.data, "polygons"):
        for p in obj.data.polygons:
            p.use_smooth = True
    return obj


def last():
    return bpy.context.active_object


def cyl(r, depth, loc, rot=(0, 0, 0), colour="gold", verts=48, r2=None):
    if r2 is None:
        bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=depth, location=loc, rotation=rot, vertices=verts)
    else:
        bpy.ops.mesh.primitive_cone_add(radius1=r, radius2=r2, depth=depth, location=loc, rotation=rot,
                                        vertices=verts)
    return put(last(), colour, smooth=False)


def ball(r, loc, colour, scale=(1, 1, 1)):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=loc, segments=32, ring_count=16)
    o = last()
    o.scale = scale
    return put(o, colour)


def box(size, loc, colour, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=rot)
    o = last()
    o.scale = size
    return put(o, colour, smooth=False)


def torus(r, minor, loc, rot=(math.pi / 2, 0, 0), colour="gold"):
    bpy.ops.mesh.primitive_torus_add(major_radius=r, minor_radius=minor, location=loc, rotation=rot,
                                     major_segments=48, minor_segments=12)
    return put(last(), colour)


def lathe(profile, loc, colour, segs=48):
    """A solid turned about Z from a (radius, height) profile, bottom first."""
    me = bpy.data.meshes.new("lathe")
    verts, faces = [], []
    n = len(profile)
    for s in range(segs):
        a = 2 * math.pi * s / segs
        for r, z in profile:
            verts.append((r * math.cos(a), r * math.sin(a), z))
    for s in range(segs):
        t = (s + 1) % segs
        for i in range(n - 1):
            faces.append((s * n + i, t * n + i, t * n + i + 1, s * n + i + 1))
    me.from_pydata(verts, [], faces)
    o = bpy.data.objects.new("lathe", me)
    bpy.context.collection.objects.link(o)
    o.location = loc
    return put(o, colour)


def join(objs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    o = bpy.context.active_object
    o.name = name
    return o


# ------------------------------------------------------------------ the suits
# Each made about the origin, about one unit tall, facing -Y.

def coin(loc=(0, 0, 0), s=1.0):
    x, y, z = loc
    parts = [
        cyl(0.5 * s, 0.08 * s, loc, rot=(math.pi / 2, 0, 0), colour="gold"),
        torus(0.47 * s, 0.035 * s, (x, y - 0.04 * s, z), colour="gold"),
        torus(0.30 * s, 0.025 * s, (x, y - 0.04 * s, z), colour="red"),
        cyl(0.14 * s, 0.10 * s, (x, y - 0.02 * s, z), rot=(math.pi / 2, 0, 0), colour="red"),
    ]
    for k in range(12):   # the sun's rays between the rings
        a = 2 * math.pi * k / 12
        parts.append(box((0.05 * s, 0.03 * s, 0.13 * s), (x + 0.22 * s * math.sin(a), y - 0.05 * s,
                                                          z + 0.22 * s * math.cos(a)), "gold", rot=(0, a, 0)))
    for k in range(8):    # bosses round the rim
        a = 2 * math.pi * k / 8 + math.pi / 8
        parts.append(ball(0.04 * s, (x + 0.39 * s * math.sin(a), y - 0.05 * s, z + 0.39 * s * math.cos(a)), "red"))
    return parts


def cup(loc=(0, 0, 0), s=1.0):
    x, y, z = loc
    prof = [(0.0, -0.5), (0.26, -0.5), (0.27, -0.46), (0.10, -0.40), (0.06, -0.30), (0.05, -0.12),
            (0.10, -0.08), (0.06, -0.04), (0.08, 0.0), (0.22, 0.10), (0.30, 0.28), (0.31, 0.45),
            (0.28, 0.46), (0.27, 0.30), (0.0, 0.30)]
    body = lathe([(r * s, h * s) for r, h in prof], loc, "gold")
    return [body,
            torus(0.12 * s, 0.03 * s, (x, y, z - 0.08 * s), rot=(0, 0, 0), colour="red"),
            torus(0.29 * s, 0.025 * s, (x, y, z + 0.33 * s), rot=(0, 0, 0), colour="red"),
            ball(0.05 * s, (x, y - 0.30 * s, z + 0.17 * s), "red"),
            ball(0.04 * s, (x - 0.18 * s, y - 0.21 * s, z + 0.17 * s), "blue"),
            ball(0.04 * s, (x + 0.18 * s, y - 0.21 * s, z + 0.17 * s), "blue")]


def sword(loc=(0, 0, 0), s=1.0, tilt=0.0):
    """A straight sword, point up, `tilt` radians off the upright."""
    x, y, z = loc
    blade = bpy.data.meshes.new("blade")
    w, t = 0.055, 0.015
    v = [(-w, -t, -0.18), (w, -t, -0.18), (w * 0.8, -t, 0.62), (0, -t, 0.72), (-w * 0.8, -t, 0.62),
         (-w, t, -0.18), (w, t, -0.18), (w * 0.8, t, 0.62), (0, t, 0.72), (-w * 0.8, t, 0.62)]
    v = [(a * s, b * s, c * s) for a, b, c in v]
    f = [(0, 1, 2, 3, 4), (9, 8, 7, 6, 5), (0, 5, 6, 1), (1, 6, 7, 2), (2, 7, 8, 3), (3, 8, 9, 4), (4, 9, 5, 0)]
    blade.from_pydata(v, [], f)
    o = bpy.data.objects.new("blade", blade)
    bpy.context.collection.objects.link(o)
    parts = [put(o, "steel", smooth=False),
             box((0.06 * s, 0.03 * s, 0.70 * s), (0, -0.02 * s, 0.24 * s), "blue"),   # fuller, painted
             box((0.42 * s, 0.07 * s, 0.06 * s), (0, 0, -0.20 * s), "gold"),
             ball(0.045 * s, (-0.22 * s, 0, -0.20 * s), "gold"),
             ball(0.045 * s, (0.22 * s, 0, -0.20 * s), "gold"),
             cyl(0.04 * s, 0.22 * s, (0, 0, -0.34 * s), colour="red", verts=16),
             ball(0.07 * s, (0, 0, -0.48 * s), "gold")]
    parts[1].scale = (0.02 * s, 0.01 * s, 0.70 * s)
    g = join(parts, "sword")
    g.rotation_euler = (0, tilt, 0)
    g.location = (x, y, z)
    return [g]


def club(loc=(0, 0, 0), s=1.0, tilt=0.0):
    """A knotted cudgel, thick end up, with a sprig of leaves."""
    x, y, z = loc
    prof = [(0.0, -0.5), (0.05, -0.5), (0.06, -0.3), (0.075, -0.1), (0.09, 0.1), (0.11, 0.3),
            (0.12, 0.42), (0.08, 0.5), (0.0, 0.51)]
    parts = [lathe([(r * s, h * s) for r, h in prof], (0, 0, 0), "wood")]
    for k, (hz, side) in enumerate([(-0.25, 1), (-0.02, -1), (0.2, 1), (0.36, -1)]):
        parts.append(ball(0.045 * s, (side * 0.08 * s, -0.02 * s, hz * s), "brown", scale=(1.2, 1, 0.8)))
    for side, hz in [(1, 0.05), (-1, 0.28)]:
        lf = ball(0.08 * s, (side * 0.19 * s, -0.03 * s, hz * s), "leaf", scale=(1.6, 0.35, 0.7))
        lf.rotation_euler = (0, -side * 0.6, 0)
        parts.append(lf)
    g = join(parts, "club")
    g.rotation_euler = (0, tilt, 0)
    g.location = (x, y, z)
    return [g]


SUITS = {"denari": coin, "coppe": cup, "spade": sword, "bastoni": club}


# --------------------------------------------------------------- the figures
# A figure stands with its feet at z = 0 and is about 3 units tall; `hand` is
# where the right hand (the picture's left) holds its suit.

ROBES = {"denari": ("green", "red"), "coppe": ("red", "blue"), "spade": ("blue", "gold"),
         "bastoni": ("red", "green")}


def head(z, beard, hat, colour):
    parts = [ball(0.24, (0, 0, z), "skin", scale=(0.9, 0.9, 1.05)),
             ball(0.03, (-0.08, -0.21, z + 0.04), "black"), ball(0.03, (0.08, -0.21, z + 0.04), "black")]
    hair = ball(0.25, (0, 0.03, z + 0.06), "brown", scale=(0.95, 0.95, 1.0))
    parts.append(hair)
    if beard:
        parts.append(ball(0.2, (0, -0.07, z - 0.17), "brown", scale=(0.95, 0.7, 1.1)))
    if hat == "crown":
        parts.append(cyl(0.2, 0.16, (0, 0, z + 0.28), colour="gold", verts=24))
        for k in range(6):
            a = 2 * math.pi * k / 6
            parts.append(cyl(0.05, 0.14, (0.19 * math.sin(a), -0.19 * math.cos(a), z + 0.42), colour="gold",
                             verts=4, r2=0.0))
        parts.append(ball(0.035, (0, -0.2, z + 0.28), "red"))
    elif hat == "cap":
        parts.append(cyl(0.27, 0.07, (0, 0, z + 0.22), colour=colour, verts=24))
        parts.append(ball(0.2, (0, 0, z + 0.24), colour, scale=(1, 1, 0.55)))
        feather = ball(0.06, (0.18, 0.0, z + 0.40), "white", scale=(0.6, 0.6, 2.4))
        feather.rotation_euler = (0, 0.5, 0)
        parts.append(feather)
    return parts


def body(robe, trim, long):
    """The trunk and the arms; long robe for a king, tunic and legs for a page."""
    parts = []
    if long:
        parts.append(cyl(0.62, 1.7, (0, 0, 0.85), colour=robe, verts=32, r2=0.32))
        parts.append(torus(0.6, 0.05, (0, 0, 0.06), rot=(0, 0, 0), colour=trim))
        parts.append(box((0.12, 0.05, 1.6), (0, -0.5, 0.85), trim, rot=(0.33, 0, 0)))
        # the mantle down the back and over the shoulders
        parts.append(cyl(0.72, 1.6, (0, 0.15, 0.95), colour=trim if trim != "gold" else "blue", verts=32,
                         r2=0.42))
    else:
        parts.append(cyl(0.48, 0.9, (0, 0, 1.25), colour=robe, verts=32, r2=0.32))
        parts.append(torus(0.47, 0.05, (0, 0, 0.82), rot=(0, 0, 0), colour=trim))
        parts.append(box((0.5, 0.06, 0.08), (0, -0.33, 1.1), trim))   # the belt
        for side in (-1, 1):
            parts.append(cyl(0.11, 0.8, (side * 0.17, 0, 0.42), colour=trim, verts=16))   # hose
            parts.append(ball(0.13, (side * 0.18, -0.08, 0.04), "black", scale=(0.9, 1.6, 0.5)))
    parts.append(ball(0.36, (0, 0, 1.72), robe, scale=(1.25, 0.8, 0.55)))   # shoulders
    # the left arm (picture's right) down at the side, a hand at the hip
    parts.append(cyl(0.1, 0.75, (0.48, -0.05, 1.35), rot=(0, 0.35, 0), colour=robe, verts=16))
    parts.append(ball(0.09, (0.6, -0.08, 1.0), "skin"))
    # the right arm raised forward to hold the suit
    parts.append(cyl(0.1, 0.6, (-0.5, -0.12, 1.55), rot=(0, -0.75, 0), colour=robe, verts=16))
    parts.append(ball(0.1, (-0.72, -0.2, 1.35), "skin"))
    return parts


def figure(kind, suit):
    robe, trim = ROBES[suit]
    parts = []
    if kind == "re":
        parts += body(robe, trim, True)
        parts += head(2.15, True, "crown", robe)
        hand = (-0.78, -0.35, 1.35)
    else:
        parts += body(robe, trim, False)
        parts += head(2.1, False, "cap", trim)
        hand = (-0.78, -0.35, 1.35)
    held(suit, hand)
    return parts


def held(suit, hand):
    hx, hy, hz = hand
    if suit == "denari":
        coin((hx - 0.15, hy - 0.1, hz + 0.45), 0.7)
    elif suit == "coppe":
        cup((hx - 0.05, hy - 0.1, hz + 0.33), 0.75)
    elif suit == "spade":
        sword((hx, hy - 0.1, hz + 0.55), 1.4, tilt=-0.15)
    else:
        club((hx, hy - 0.1, hz + 0.45), 1.3, tilt=-0.2)


def horse(coat):
    """A horse walking to the picture's left, its back at about z 1.6."""
    p = []
    p.append(ball(0.75, (0.1, 0, 1.45), coat, scale=(1.35, 0.6, 0.62)))                 # barrel
    neck = cyl(0.25, 0.9, (-0.75, 0, 1.95), rot=(0, -0.7, 0), colour=coat, verts=24, r2=0.18)
    p.append(neck)
    hd = ball(0.3, (-1.15, 0, 2.3), coat, scale=(1.3, 0.6, 0.6))
    hd.rotation_euler = (0, 0.6, 0)
    p.append(hd)
    p.append(ball(0.04, (-1.2, -0.17, 2.42), "black"))
    for sx in (-0.95, -1.1):   # ears
        p.append(cyl(0.05, 0.16, (sx, 0, 2.6), colour=coat, verts=8, r2=0.0))
    mane = ball(0.18, (-0.62, 0.05, 2.12), "black", scale=(2.6, 0.5, 0.7))
    mane.rotation_euler = (0, -0.75, 0)
    p.append(mane)
    for lx, lift in [(-0.55, 0.25), (-0.35, 0.0), (0.6, 0.0), (0.8, 0.15)]:   # legs, one fore leg raised
        leg = cyl(0.09, 1.0, (lx, 0, 0.62 + lift * 0.5), rot=(0, lift, 0), colour=coat, verts=12)
        p.append(leg)
        p.append(cyl(0.11, 0.1, (lx + lift * 0.4, 0, 0.1 + lift), colour="black", verts=12))
    tail = ball(0.12, (1.15, 0.05, 1.3), "black", scale=(0.8, 0.5, 2.8))
    tail.rotation_euler = (0, -0.4, 0)
    p.append(tail)
    # the saddle cloth
    p.append(box((0.8, 1.0, 0.5), (0.15, 0, 1.55), "red"))
    p[-1].scale = (0.85, 0.95, 0.55)
    return p


def knight(suit):
    robe, trim = ROBES[suit]
    p = horse("white" if suit in ("coppe", "bastoni") else "brown")
    # the rider: trunk, head with a plumed cap, the right arm raised
    p.append(cyl(0.36, 0.8, (0.1, -0.1, 2.3), colour=robe, verts=24, r2=0.25))
    p.append(ball(0.27, (0.1, -0.1, 2.68), robe, scale=(1.2, 0.8, 0.5)))
    p.append(cyl(0.12, 0.6, (0.2, -0.62, 1.55), rot=(0, 0.3, 0), colour=trim, verts=12))   # leg
    p.append(ball(0.12, (0.1, -0.66, 1.25), "black", scale=(1.6, 1, 0.6)))
    for part in head(3.05, False, "cap", trim):
        part.location.x += 0.1
        part.location.y -= 0.1
    p.append(cyl(0.09, 0.6, (-0.22, -0.3, 2.65), rot=(0, -0.9, 0), colour=robe, verts=12))
    p.append(ball(0.09, (-0.45, -0.38, 2.8), "skin"))
    held(suit, (-0.5, -0.45, 2.8))


# -------------------------------------------------------------- the pictures

def stage(w, h, ortho):
    scn = bpy.context.scene
    try:
        scn.render.engine = "BLENDER_EEVEE"
    except TypeError:
        pass
    scn.render.resolution_x, scn.render.resolution_y = w, h
    scn.render.resolution_percentage = 100
    scn.render.film_transparent = True
    scn.render.use_freestyle = True
    scn.render.line_thickness_mode = "ABSOLUTE"
    scn.render.line_thickness = max(1.5, w / 260)
    scn.view_settings.view_transform = "Standard"
    vl = scn.view_layers[0]
    vl.use_freestyle = True
    fs = vl.freestyle_settings
    if not fs.linesets:
        fs.linesets.new("lines")
    ls = fs.linesets[0]
    ls.select_by_visibility = True
    ls.select_silhouette = ls.select_border = ls.select_crease = True
    ls.linestyle.color = (0.05, 0.03, 0.02)
    ls.linestyle.thickness = max(1.5, w / 260)
    cam = bpy.data.objects.get("CA Camera")
    if cam is None:
        cam = bpy.data.objects.new("CA Camera", bpy.data.cameras.new("CA Camera"))
    if cam.name not in scn.collection.objects:
        scn.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = ortho
    cam.rotation_euler = (math.pi / 2, 0, 0)
    scn.camera = cam
    sun = bpy.data.objects.get("CA Sun")
    if sun is None:
        sun = bpy.data.objects.new("CA Sun", bpy.data.lights.new("CA Sun", "SUN"))
    if sun.name not in scn.collection.objects:
        scn.collection.objects.link(sun)
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(55), 0, math.radians(-35))
    return cam


def frame(cam, margin=1.04):
    """Points the camera at the middle of everything there is and fits it."""
    bpy.context.view_layer.update()
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for o in bpy.context.scene.objects:
        if o.type != "MESH":
            continue
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector(map(min, lo, w))
            hi = Vector(map(max, hi, w))
    mid = (lo + hi) / 2
    scn = bpy.context.scene
    aspect = scn.render.resolution_x / scn.render.resolution_y
    cam.location = (mid.x, lo.y - 10, mid.z)
    # ortho_scale is the picture's longer side
    dx, dz = hi.x - lo.x, hi.z - lo.z
    cam.data.ortho_scale = (max(dx, dz * aspect) if aspect >= 1 else max(dz, dx / aspect)) * margin


def shoot(name, w, h):
    os.makedirs(OUT, exist_ok=True)
    scn = bpy.context.scene
    scn.render.filepath = os.path.join(OUT, name + ".png")
    scn.render.image_settings.file_format = "PNG"
    scn.render.image_settings.color_mode = "RGBA"
    bpy.ops.render.render(write_still=True)


def make(which=None):
    for suit, fn in SUITS.items():
        if which and suit not in which:
            continue
        clear()
        cam = stage(256, 256, 1.2)
        fn()
        frame(cam, 1.06)
        shoot(suit, 256, 256)
    for suit in SUITS:
        for kind in ("fante", "cavallo", "re"):
            name = f"{kind}_{suit}"
            if which and name not in which:
                continue
            clear()
            cam = stage(400, 560, 3.5)
            if kind == "cavallo":
                knight(suit)
            else:
                figure(kind, suit)
            frame(cam, 1.04)
            shoot(name, 400, 560)


if __name__ == "__main__":
    make()
