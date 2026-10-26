# l'Ospite, the thing from between the worlds that waits in the engineer's
# laboratory (games/lab, the end of the game), and what goes with it. The
# laboratory moves them itself (they are its own entities, not creatures);
# the engine plays l'Ospite's clips by how fast it goes (idle, walk, run), as
# it does for any body. Built with the helpers of tools/make_fps_models.py.
#
#   flatpak run org.blender.Blender --background --python tools/make_ospite.py
#
#   ospite         a dark mass three nodes high: a cracked core lit violet from
#                  inside, a ring of eyes round it, a maw under them, tentacles
#                  hanging below, origin at their tips. Rigged: body, jaw and
#                  three bones down each of seven tentacles; clips idle, walk
#                  and run (the tentacles swaying, a wave running down each,
#                  faster and wider as it goes faster), shoot, attack, laser
#                  (tentacles raised to point ahead, trembling as it fires),
#                  summon (tentacles thrown wide, rising), death
#   ospite_shield  a cage of six rings of glowing rods round it (phase one)
#   ospite_orb     the sphere of its own energy it throws
#   lab_shotgun.png, lab_pistol.png, lab_chaingun.png (games/lab/textures)
#                  the base's guns as icons
import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_fps_models as d  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "games", "lab", "mods", "lab_core")


TENTACLES = 7
SEG = ((0.55, 1.25), (0.75, 0.85), (0.9, 0.45), (1.0, 0.05))   # (radius, height) down a tentacle


def tentacle_points(i):
    a = i * 2 * math.pi / TENTACLES
    return [(math.cos(a + 0.12 * k) * r, math.sin(a + 0.12 * k) * r, z) for k, (r, z) in enumerate(SEG)]


def ospite():
    """Rigged: a body bone, the jaw, and a chain of three bones down each
    tentacle, so a wave can run along it."""
    c = 2.0   # the middle of the core over the origin
    bones = [("root", None, (0, 0, 0)), ("body", "root", (0, 0, c)), ("jaw", "body", (0, 0.8, c - 0.3))]
    for i in range(TENTACLES):
        pts = tentacle_points(i)
        bones += [(f"t{i}_1", "body", pts[0]), (f"t{i}_2", f"t{i}_1", pts[1]), (f"t{i}_3", f"t{i}_2", pts[2])]
    rg = d.rig("ospite_rig", bones)
    p = d.Parts()
    p.ball("body", "void", (0, 0, c), 1.0, (1, 1, 0.9), 3)
    # cracks: thin glowing slabs round the core
    for i in range(10):
        a = i * 2 * math.pi / 10
        x, y = math.cos(a) * 0.98, math.sin(a) * 0.98
        p.box("body", "violet", (x, y, c + 0.2 * math.sin(3 * a)), (0.06, 0.06, 0.7), (0.3 * math.sin(a), 0, a))
    # a ring of eyes, the front three larger
    for i in range(9):
        a = math.pi / 2 + (i - 4) * 0.32
        r = 0.98
        x, y = math.cos(a) * r, math.sin(a) * r
        big = 0.16 if abs(i - 4) <= 1 else 0.1
        p.ball("body", "eye_white", (x, y, c + 0.35), big, (1, 1, 1), 2)
        k = (r + big * 0.75) / r
        p.ball("body", "violet", (x * k, y * k, c + 0.35), big * 0.45, (1, 1, 1), 1)
    # the maw: a dark gape under the eyes, its upper teeth on the body, the
    # lower ones on the jaw
    p.ball("body", "mouth", (0, 0.86, c - 0.3), 0.42, (1.2, 0.5, 0.6), 2)
    for i in range(9):
        a = -1.1 + i * 0.275
        x = math.sin(a) * 0.42
        p.spike("body", "bone", (x, 0.98, c - 0.1), (x, 1.0, c - 0.3), 0.04)
        p.spike("jaw", "bone", (x, 0.96, c - 0.52), (x, 0.98, c - 0.34), 0.035)
    p.box("jaw", "void", (0, 0.85, c - 0.55), (0.8, 0.3, 0.12))
    # the tentacles: three segments each, thinner down, a glowing barb at the tip
    for i in range(TENTACLES):
        pts = tentacle_points(i)
        for k, w in enumerate((0.22, 0.16, 0.11)):
            p.limb(f"t{i}_{k + 1}", "void", pts[k], pts[k + 1], w, w)
            if k > 0:
                p.ball(f"t{i}_{k + 1}", "void", pts[k], w * 0.62, sub=1)
        tip = pts[3]
        p.spike(f"t{i}_3", "violet", tip, (tip[0] * 1.08, tip[1] * 1.08, tip[2] - 0.35), 0.07)
    mesh = p.build("ospite_mesh")
    d.skin(mesh, rg)

    def sway(amp, bob, jaw):
        """Every tentacle swinging round its own circle, each a seventh of a
        turn behind the last, and the swing running down it a little late
        at every joint, wider towards the tip: a wave along each."""
        keys = {"body": [(t / 8, {"l": (0, bob * math.sin(2 * math.pi * t / 8), 0),
                                  "r": (0, 0.06 * math.sin(2 * math.pi * t / 8), 0)}) for t in range(8)],
                "jaw": [(0.0, {"r": (0.0, 0, 0)}), (0.5, {"r": (jaw, 0, 0)})]}
        for i in range(TENTACLES):
            phase = i * 2 * math.pi / TENTACLES
            for k, scale in enumerate((0.6, 0.85, 1.1)):
                keys[f"t{i}_{k + 1}"] = [
                    (t / 8, {"r": (amp * scale * math.sin(2 * math.pi * t / 8 + phase - k * 0.7), 0,
                                   amp * scale * math.cos(2 * math.pi * t / 8 + phase - k * 0.7))})
                    for t in range(8)]
        return keys

    d.clip(rg, "idle", 3.2, sway(0.22, 0.08, 0.12), loop=True)
    d.clip(rg, "walk", 1.8, sway(0.35, 0.12, 0.2), loop=True)
    d.clip(rg, "run", 1.0, sway(0.55, 0.16, 0.35), loop=True)
    shoot = sway(0.3, 0.05, 0.0)
    shoot["jaw"] = [(0.0, {}), (0.3, {"r": (0.6, 0, 0)}), (0.7, {"r": (0.6, 0, 0)}), (1.0, {})]
    d.clip(rg, "shoot", 1.0, shoot)
    d.clip(rg, "attack", 1.0, shoot)
    # The laser: every tentacle swings up to point ahead (+Y, the way it
    # faces), the front ones furthest; held while the beams aim, a tremble as
    # they fire, the maw open; then back.
    laser = {"body": [(0.0, {}), (0.15, {"r": (-0.25, 0, 0)}), (0.85, {"r": (-0.25, 0, 0)}), (1.0, {})],
             "jaw": [(0.0, {}), (0.55, {"r": (0.1, 0, 0)}), (0.62, {"r": (0.7, 0, 0)}), (0.85, {"r": (0.7, 0, 0)}),
                     (1.0, {})]}
    for i in range(TENTACLES):
        a = i * 2 * math.pi / TENTACLES
        front = math.sin(a) > 0.3
        up = 1.45 if front else 0.9
        for k in range(3):
            aim = (up if k == 0 else 0.15 if front else 0.35, 0, 0)
            shake = (aim[0] + 0.06, 0, 0.05)
            laser[f"t{i}_{k + 1}"] = [(0.0, {}), (0.15, {"r": aim}), (0.55, {"r": aim}), (0.6, {"r": shake}),
                                      (0.65, {"r": aim}), (0.7, {"r": shake}), (0.75, {"r": aim}),
                                      (0.8, {"r": shake}), (0.85, {"r": aim}), (1.0, {})]
    d.clip(rg, "laser", 1.8, laser)
    # The summoning: the tentacles thrown wide all round, the body rising, the
    # maw open, a shudder; then back.
    summon = {"body": [(0.0, {}), (0.35, {"l": (0, 0.35, 0)}), (0.7, {"l": (0, 0.35, 0), "r": (0, 0.3, 0)}),
                       (1.0, {})],
              "jaw": [(0.0, {}), (0.3, {"r": (0.8, 0, 0)}), (0.75, {"r": (0.8, 0, 0)}), (1.0, {})]}
    for i in range(TENTACLES):
        a = i * 2 * math.pi / TENTACLES
        for k in range(3):
            w = (1.1, 0.3, 0.2)[k]
            out = (w * math.sin(a), 0, w * math.cos(a))
            summon[f"t{i}_{k + 1}"] = [(0.0, {}), (0.3, {"r": out}), (0.75, {"r": out}), (1.0, {})]
    d.clip(rg, "summon", 1.4, summon)
    # Dying: the tentacles curl up under it, the maw falls open, it sinks.
    death = {"body": [(0.0, {}), (0.6, {"l": (0, -0.6, 0), "r": (0.3, 0, 0.2)}),
                      (1.0, {"l": (0, -1.5, 0), "r": (0.5, 0, 0.3)})],
             "jaw": [(0.0, {}), (0.4, {"r": (0.8, 0, 0)})]}
    for i in range(TENTACLES):
        a = i * 2 * math.pi / TENTACLES
        for k in range(3):
            curl = (0.9 + 0.3 * k)
            death[f"t{i}_{k + 1}"] = [(0.0, {}), (0.7, {"r": (curl * math.sin(a), 0, -curl * math.cos(a))})]
    d.clip(rg, "death", 1.6, death)
    d.export([rg, mesh], os.path.join(OUT, "ospite.gltf"), True)
    d.put_aside([rg], 0.0)


def shield(p):
    c = 2.0
    for k in range(6):
        z = c - 1.3 + k * 0.52
        r = 1.45 * math.sqrt(max(0.15, 1 - ((z - c) / 1.6) ** 2))
        for i in range(10):
            a = i * 2 * math.pi / 10 + k * 0.3
            p.box(None, "shield", (math.cos(a) * r, math.sin(a) * r, z), (0.05, 0.05, 0.4), (0, 0, a))


def orb(p):
    p.ball(None, "violet", (0, 0, 0.3), 0.3, sub=2)
    p.ball(None, "eye_white", (0, 0, 0.3), 0.16, sub=1)


gun_prev = []


def gun_icon(gun, out):
    """A gun lent by the base, as a 32x32 icon for the laboratory's hotbar:
    its held model seen from the side, flat-lit, outlined."""
    import numpy as np
    from mathutils import Vector
    for o in list(bpy.context.scene.objects):
        o.hide_render = True
    bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, f"{gun}_held.gltf"))
    gun = [o for o in bpy.context.selected_objects if o.type == "MESH"]
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "FLAT"
    scene.display.shading.color_type = "MATERIAL"
    scene.render.film_transparent = True
    scene.render.resolution_x = scene.render.resolution_y = 256
    pts = [o.matrix_world @ Vector(c) for o in gun for c in o.bound_box]
    centre = sum(pts, Vector()) / len(pts)
    extent = max(max(p[i] for p in pts) - min(p[i] for p in pts) for i in range(3))
    cam = bpy.data.objects.get("IconCam") or bpy.data.objects.new("IconCam", bpy.data.cameras.new("IconCam"))
    if cam.name not in scene.collection.objects:
        scene.collection.objects.link(cam)
    for o in gun_prev:
        o.hide_render = True
    gun_prev[:] = gun
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = extent * 1.15
    cam.location = centre + Vector((extent, 0, 0))
    cam.rotation_euler = (math.radians(90), 0, math.radians(90))
    scene.camera = cam
    scene.render.filepath = out
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(out)
    img.scale(32, 32)
    px = np.array(img.pixels[:]).reshape(32, 32, 4)
    solid = px[:, :, 3] > 0.45
    px[:, :, 3] = solid.astype(float)
    edge = np.zeros_like(solid)
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        edge |= np.roll(solid, (dy, dx), axis=(0, 1))
    px[edge & ~solid] = (0.1, 0.07, 0.07, 1.0)
    img.pixels[:] = px.ravel()
    img.save()


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    os.makedirs(OUT, exist_ok=True)
    d.OUT = OUT
    d.mats()
    d.M["void"] = d.material("void", (0.05, 0.03, 0.07))
    d.M["violet"] = d.material("violet", (0.70, 0.25, 1.00), glow=True)
    d.M["shield"] = d.material("shield", (0.45, 0.80, 1.00), glow=True)
    ospite()
    d.rigid("ospite_shield", shield, 4.0)
    d.rigid("ospite_orb", orb, 8.0)
    for gun in ("shotgun", "pistol", "chaingun"):
        gun_icon(gun, os.path.join(OUT, "..", "..", "textures", f"lab_{gun}.png"))
    print("l'Ospite written to", os.path.normpath(OUT))
