# The Inferno's two floating creatures, the wraith and the cinder, built out of
# boxes in Blender, rigged and animated, and exported into games/inferno/mods/inferno.
#
#   blender --background --python tools/make_inferno_creatures.py
#   ... --python tools/make_inferno_creatures.py -- --guardian   (only the fortress's guardian)
#
# They were still boxes written as JSON (tools/make_inferno_models.py, which
# keeps the fireball and the brute); here they move. Front +Y, standing on the
# origin, like every creature: the same shapes and colours as before.
#
#   wraith   a pale cube over nine tentacles that sway; it pulls back to spit
#           a fireball; dying, it tips over, sinks, and its tentacles curl up.
#   cinder   a head over a smoking core, and three rings of rods going round it,
#           the middle one the other way; faster while it shoots; dying, the
#           rods fall out of the air and scatter and the head drops after them.
#
# Clips: idle, walk, run, shoot, death (docs/MODDING.md, Creature). Leaves the
# work in ~/kart_assets/inferno.blend.
import importlib.util
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("make_pet_models", os.path.join(HERE, "make_pet_models.py"))
pets = importlib.util.module_from_spec(_spec)
sys.modules["make_pet_models"] = pets
_spec.loader.exec_module(pets)

OUT = os.path.join(HERE, "..", "games", "inferno", "mods", "inferno")
BLEND = os.path.expanduser("~/kart_assets/inferno.blend")
TAU = 2 * math.pi


def material(name, rgb, glow=False):
    m = pets.material(name, rgb)
    if glow:
        bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
        bsdf.inputs["Emission Color"].default_value = (*rgb, 1.0)
        bsdf.inputs["Emission Strength"].default_value = 1.0
    return m


def gl(lo, hi):
    """A box as make_inferno_models.py wrote it -- glTF's axes, front -z, y up --
    in Blender's: front +y, z up."""
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    return (x0, -z1, y0), (x1, -z0, y1)


def wraith():
    legs = []
    for ix in (-0.45, 0.0, 0.45):
        for iz in (-0.45, 0.0, 0.45):
            legs.append((ix, iz, 0.35 + 0.2 * ((ix * 7 + iz * 13) % 1.0)))
    bones = [("root", None, (0, 0, 0)), ("body", "root", (0, 0, 1.2))]
    bones += [(f"tentacle_{i}", "body", (ix, -iz, 0.6)) for i, (ix, iz, _) in enumerate(legs)]
    arm = pets.rig("wraith_rig", bones)
    mats = {"body": material("g_body", (0.94, 0.94, 0.94)), "legs": material("g_legs", (0.86, 0.86, 0.86)),
            "face": material("g_face", (0.35, 0.35, 0.38)), "tear": material("g_tear", (0.62, 0.66, 0.72))}
    boxes = [("body", "body", *gl((-0.75, 0.6, -0.75), (0.75, 1.8, 0.75)))]
    for lo, hi in (((-0.45, 1.35, -0.78), (-0.2, 1.45, -0.75)), ((0.2, 1.35, -0.78), (0.45, 1.45, -0.75)),
                   ((-0.25, 0.85, -0.78), (0.25, 1.05, -0.75))):
        boxes.append(("body", "face", *gl(lo, hi)))
    for lo, hi in (((-0.38, 1.12, -0.77), (-0.3, 1.35, -0.75)), ((0.3, 1.12, -0.77), (0.38, 1.35, -0.75))):
        boxes.append(("body", "tear", *gl(lo, hi)))
    for i, (ix, iz, length) in enumerate(legs):
        boxes.append((f"tentacle_{i}", "legs",
                      *gl((ix - 0.07, 0.6 - length, iz - 0.07), (ix + 0.07, 0.6, iz + 0.07))))
    mesh = pets.boxes_mesh("wraith_mesh", boxes, mats)
    pets.skin(mesh, arm)

    def sway(amount, bob):
        keys = {"body": [(0.0, (0, 0, 0), (0, 0, 0)), (0.5, (0.03, 0, 0), (0, bob, 0))]}
        for i in range(len(legs)):
            p = (i * 0.37) % 1.0   # each its own phase
            keys[f"tentacle_{i}"] = [(0.0, (amount * math.sin(TAU * p), 0, 0), None),
                                     (0.25, (amount * math.sin(TAU * (p + 0.25)), 0, 0), None),
                                     (0.5, (amount * math.sin(TAU * (p + 0.5)), 0, 0), None),
                                     (0.75, (amount * math.sin(TAU * (p + 0.75)), 0, 0), None)]
        return keys
    pets.clip(arm, "wraith", "idle", 3.0, sway(0.25, 0.06))
    pets.clip(arm, "wraith", "walk", 2.0, sway(0.35, 0.08))
    pets.clip(arm, "wraith", "run", 1.2, sway(0.45, 0.1))
    shoot = sway(0.3, 0.0)
    shoot["body"] = [(0.0, (0, 0, 0), (0, 0, 0)), (0.35, (-0.3, 0, 0), (0, 0.05, 0.12)),
                     (0.5, (0.25, 0, 0), (0, 0, -0.1)), (0.8, (0, 0, 0), (0, 0, 0))]
    pets.clip(arm, "wraith", "shoot", 1.0, shoot)
    death = {"root": [(0.0, (0, 0, 0), (0, 0, 0)), (0.2, (0.2, 0, 0), (0, 0.1, 0)),
                      (0.8, (0.9, 0, 0.3), (0, -0.9, 0)), (1.0, (0.9, 0, 0.3), (0, -0.9, 0))]}
    for i in range(len(legs)):
        curl = 1.3 if i % 2 else -1.3
        death[f"tentacle_{i}"] = [(0.0, (0, 0, 0), None), (0.5, (curl, 0, curl * 0.3), None),
                                  (1.0, (curl, 0, curl * 0.3), None)]
    pets.clip(arm, "wraith", "death", 1.0, death, loop=False)
    return arm, mesh


# K scales it all (the Inferno's guardian is one 1.7 times as big, darker,
# with four rods a ring more: tools/make_inferno_creatures.py --guardian).
def cinder(K=1.0, name="cinder", head=(0.95, 0.72, 0.18), rods=(1.0, 0.78, 0.25), per_ring=4):
    def sc(box):
        return tuple(tuple(c * K for c in v) for v in box)
    rings = tuple((y * K, r * K, t) for y, r, t in ((1.05, 0.55, 0.0), (0.7, 0.42, 0.4), (0.3, 0.3, 0.8)))
    bones = [("root", None, (0, 0, 0)), ("head", "root", (0, 0, 1.3 * K)), ("core", "root", (0, 0, 0.5 * K))]
    bones += [(f"ring_{i}", "root", (0, 0, y)) for i, (y, _, _) in enumerate(rings)]
    arm = pets.rig(f"{name}_rig", bones)
    m = "b" if name == "cinder" else name   # the cinder's materials keep their names
    mats = {"head": material(f"{m}_head", head), "eyes": material(f"{m}_eyes", (0.2, 0.12, 0.05)),
            "core": material(f"{m}_core", (0.35, 0.22, 0.12)), "rods": material(f"{m}_rods", rods, True)}
    boxes = [("head", "head", *sc(gl((-0.25, 1.3, -0.25), (0.25, 1.8, 0.25)))),
             ("head", "eyes", *sc(gl((-0.18, 1.52, -0.27), (-0.06, 1.6, -0.25)))),
             ("head", "eyes", *sc(gl((0.06, 1.52, -0.27), (0.18, 1.6, -0.25)))),
             ("core", "core", *sc(gl((-0.1, 0.5, -0.1), (0.1, 1.3, 0.1))))]
    for i, (y, r, turn) in enumerate(rings):
        for k in range(per_ring):
            a = turn + k * 2 * math.pi / per_ring
            cx, cz = math.cos(a) * r, math.sin(a) * r
            w = 0.06 * K
            boxes.append((f"ring_{i}", "rods", *gl((cx - w, y, cz - w), (cx + w, y + 0.42 * K, cz + w))))
    mesh = pets.boxes_mesh(f"{name}_mesh", boxes, mats)
    pets.skin(mesh, arm)

    # A bone points up with no roll, so a turn about the world's up is about
    # its own Y. A whole turn over the clip: its end is its start again.
    def spinning(turns, bob):
        bob *= K
        keys = {"head": [(0.0, (0, 0, 0), (0, 0, 0)), (0.25, (0, 0.1, 0), (0, bob, 0)),
                         (0.5, (0, 0, 0), (0, 0, 0)), (0.75, (0, -0.1, 0), (0, bob, 0)),
                         (1.0, (0, 0, 0), (0, 0, 0))]}
        for i in range(3):
            way = -1 if i == 1 else 1
            keys[f"ring_{i}"] = [(k / 4, (0, way * turns * TAU * k / 4, 0), (0, bob * (0.5 if k % 2 else 0), 0))
                                 for k in range(5)]
        return keys
    pets.clip(arm, name, "idle", 3.0, spinning(1, 0.05), loop=False)
    pets.clip(arm, name, "walk", 2.0, spinning(1, 0.07), loop=False)
    pets.clip(arm, name, "run", 1.2, spinning(1, 0.08), loop=False)
    pets.clip(arm, name, "shoot", 1.0, spinning(2, 0.0), loop=False)
    death = {"head": [(0.0, (0, 0, 0), (0, 0, 0)), (0.3, (-0.3, 0, 0.2), (0, 0.1 * K, 0)),
                      (0.8, (0.6, 0, 0.5), (0, -1.1 * K, 0)), (1.0, (0.6, 0, 0.5), (0, -1.1 * K, 0))],
             "core": [(0.0, (0, 0, 0), (0, 0, 0)), (0.7, (1.2, 0, 0), (0, -0.3 * K, 0)),
                      (1.0, (1.4, 0, 0), (0, -0.35 * K, 0))]}
    for i, (y, _, _) in enumerate(rings):
        # Down to the floor, a little further out, tipped.
        way = -1 if i == 1 else 1
        death[f"ring_{i}"] = [(0.0, (0, 0, 0), (0, 0, 0)), (0.25, (0.2 * way, way * 1.5, 0), (0, 0.05, 0)),
                              (0.7, (0.5 * way, way * 2.2, 0.3), (0, 0.06 * K - y, 0)),
                              (1.0, (0.5 * way, way * 2.2, 0.3), (0, 0.06 * K - y, 0))]
    pets.clip(arm, name, "death", 1.0, death, loop=False)
    return arm, mesh


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = pets.FPS
    bpy.context.preferences.edit.keyframe_new_interpolation_type = "LINEAR"   # rods turn evenly
    os.makedirs(OUT, exist_ok=True)
    if "--guardian" in sys.argv:
        # The heart of the fortress's guardian, alone (games/inferno, mods/keys).
        arm, mesh = cinder(1.7, "cinder_guardian", (0.55, 0.18, 0.12), (1.0, 0.35, 0.1), 6)
        pets.export(arm, [mesh], os.path.join(OUT, "..", "inferno_heart", "cinder_guardian.gltf"))
        print("the cinder guardian in", os.path.normpath(OUT))
        sys.exit(0)
    for i, (name, make) in enumerate((("wraith", wraith), ("cinder", cinder))):
        arm, mesh = make()
        pets.export(arm, [mesh], os.path.join(OUT, f"{name}.gltf"))
        arm.location.x = 2.5 * i
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("wraith and cinder in", os.path.normpath(OUT))
