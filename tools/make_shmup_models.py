# The arcade shooter's planes, built in Blender out of the front's
# (tools/make_front_models.py: the same fuselage, wings and propeller, the
# same "spin" clip) in liveries of their own, so that from overhead the
# player's fighter and each kind of plane coming down the screen are told
# apart at a glance. Exported into games/shmup/mods/shmup_core.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_shmup_models.py"
#
# Starts from an empty file and leaves the work in assets/blender/shmup.blend.
#
#   fighter      the player's: silver, red wing tips and fin, a yellow cowling
#   foe_dart     olive, as the front's plane is
#   foe_weaver   orange, a white band across each wing
#   foe_ace      black, red bands
#   bomber       the front's bomber in dark grey, red wing tips
#   wingman      the fighter at half size, light blue: a player's support plane
#   mega         the last wave's flying fortress: 40 across, six engines, a
#                ring for each of its six turrets and a glowing core
#   turret       one of them, two barrels along its front: the game turns it
#   tank         the front's tank in one piece, for an islet: the game turns it
import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_fps_models as d  # noqa: E402
import make_front_models as f  # noqa: E402

ROOT = d.ROOT
OUT = os.path.join(ROOT, "games", "shmup", "mods", "shmup_core")
BLEND = os.path.join(ROOT, "assets", "blender", "shmup.blend")


def livery(body):
    """The front's `m`: olive becomes this plane's own colour."""
    return lambda name: body if name in ("olive", "dark_olive") else name


def bands(p, root, colour, x, w):
    """A band across each wing, x out from the middle and w wide, over and under."""
    for sx in (-1, 1):
        p.box(root, colour, (sx * x, 0.4, 0.55), (w, 1.64, 0.16))


def fighter(p, root):
    f.plane(p, root, livery("ace_silver"))
    bands(p, root, "ace_red", 4.4, 1.2)                                     # the wing tips
    p.box(root, "ace_red", (0, -3.05, 1.85), (0.1, 0.92, 0.32))             # the fin's top
    p.cyl(root, "ace_yellow", (0, 2.55, 0.75), 0.52, 0.3, "Y", seg=14)      # the cowling's ring


def dart(p, root):
    f.plane(p, root, f.plain)


def weaver(p, root):
    f.plane(p, root, livery("foe_orange"))
    bands(p, root, "foe_white", 3.0, 0.7)


def ace(p, root):
    f.plane(p, root, livery("foe_black"))
    bands(p, root, "ace_red", 2.6, 0.9)
    p.box(root, "ace_red", (0, -3.05, 1.85), (0.1, 0.92, 0.32))


def bomber(p, root):
    f.bomber(p, root, livery("foe_grey"))
    for sx in (-1, 1):
        p.box(root, "ace_red", (sx * 8.2, 0.6, 1.5), (1.6, 2.24, 0.2))


def wingman(p):
    f.plane(p, None, livery("wing_blue"))
    bands(p, None, "foe_white", 4.4, 1.2)


# Where the mega's turrets stand (x, y forward), as games/shmup/mods/shmup
# has them (its z is -y), and its engines.
MEGA_TURRETS = [(-15, 1.5), (-8, 1.5), (8, 1.5), (15, 1.5), (0, 7), (0, -6)]
MEGA_ENGINES = [-16, -11, -5.5, 5.5, 11, 16]
MEGA_BONES = [(f"prop_{i}", "root", (x, 5.2, 2.3)) for i, x in enumerate(MEGA_ENGINES)]


def mega(p, root):
    p.cyl(root, "mega_hull", (0, 0, 2.2), 2.0, 22.0, "Y", seg=18)               # the fuselage
    p.cyl(root, "mega_hull", (0, -12.5, 2.5), 1.7, 3.5, "Y", r2=0.4, seg=18)    # the tail cone
    p.ball(root, "window", (0, 11.0, 2.2), 2.0, (1, 1.2, 1), 2)                 # the glazed nose
    p.box(root, "mega_hull", (0, 1.5, 2.6), (40.0, 6.0, 0.4))                   # the wings
    p.box(root, "mega_dark", (0, 2.5, 2.5), (18.0, 9.0, 0.4))                   # and their roots
    p.box(root, "mega_hull", (0, -12.0, 2.8), (15.0, 3.2, 0.25))                # the tailplane
    for sx in (-1, 1):
        p.box(root, "mega_dark", (sx * 7.2, -12.6, 4.0), (0.25, 2.6, 2.6))      # twin fins
        p.box(root, "ace_red", (sx * 18.8, 1.5, 2.85), (2.4, 6.04, 0.12))       # the wing tips
        p.box(root, "ace_yellow", (sx * 3.4, 1.5, 2.85), (0.5, 9.04, 0.1))      # a band at each root
    p.box(root, "ace_red", (0, -3.0, 4.15), (0.8, 8.0, 0.12))                   # a stripe down its back
    for i, x in enumerate(MEGA_ENGINES):
        p.cyl(root, "mega_dark", (x, 3.4, 2.3), 0.7, 4.0, "Y", seg=12)          # the nacelles
        p.cyl(root, "gunmetal", (x, 5.5, 2.3), 0.4, 0.4, "Y", r2=0.15, seg=12)
        for a in (0, math.pi * 2 / 3, math.pi * 4 / 3):
            p.box(f"prop_{i}", "rotor", (x, 5.2, 2.3), (0.16, 0.05, 3.2), (0, a, 0))
    for x, y in MEGA_TURRETS:
        top = 4.2 if x == 0 else 2.8
        p.cyl(root, "gunmetal", (x, y, top + 0.1), 1.5, 0.2, "Z", seg=16)       # a ring for a turret
    p.ball(root, "mega_core", (0, 0.5, 4.2), 1.3, (1, 1, 0.6), 2)               # the core


def turret(p):
    p.cyl(None, "gunmetal", (0, 0, 0.25), 1.2, 0.5, "Z", seg=16)
    p.ball(None, "ace_red", (0, 0, 0.5), 0.9, (1, 1, 0.7), 2)
    for sx in (-1, 1):
        p.cyl(None, "foe_black", (sx * 0.35, 1.2, 0.7), 0.16, 2.0, "Y", seg=8)


def tank(p):
    from mathutils import Matrix
    f.tank_hull(p)
    p.pre = Matrix.Translation((0, 0, 1.63))
    f.tank_turret(p)
    p.pre = Matrix.Translation((0, 1.3, 2.0))
    f.tank_gun(p)


def rigid_out(name, make, x, scale=1.0):
    from mathutils import Matrix
    p = d.Parts(Matrix.Scale(scale, 4))
    make(p)
    obj = p.build(name)
    d.export([obj], os.path.join(OUT, f"{name}.gltf"), False)
    d.put_aside([obj], x)


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = d.FPS
    bpy.context.preferences.edit.keyframe_new_interpolation_type = "LINEAR"
    os.makedirs(OUT, exist_ok=True)
    f.OUT = OUT          # where make_front_models' rigged_out writes
    d.mats()
    for mname, rgb in [("tire", (0.05, 0.05, 0.05)), ("dark_olive", (0.17, 0.21, 0.12)),
                       ("rotor", (0.1, 0.1, 0.11)),
                       ("ace_silver", (0.80, 0.82, 0.84)), ("ace_red", (0.78, 0.08, 0.06)),
                       ("ace_yellow", (0.95, 0.78, 0.10)), ("foe_orange", (0.80, 0.36, 0.06)),
                       ("foe_white", (0.92, 0.92, 0.90)), ("foe_black", (0.07, 0.07, 0.09)),
                       ("foe_grey", (0.24, 0.26, 0.30)),
                       ("wing_blue", (0.45, 0.70, 0.92)), ("mega_hull", (0.30, 0.33, 0.42)),
                       ("mega_dark", (0.16, 0.17, 0.22))]:
        d.M[mname] = d.material(mname, rgb)
    d.M["mega_core"] = d.material("mega_core", (0.25, 0.85, 1.0), glow=True)
    d.M["window"] = d.material("window", (0.55, 0.75, 0.85), rough=0.1, alpha=0.3)
    spin = [("spin", 0.15, f.turn("prop", "y"))]
    for x, (name, make) in enumerate([("fighter", fighter), ("foe_dart", dart), ("foe_weaver", weaver),
                                      ("foe_ace", ace)]):
        f.rigged_out(name, f.PLANE_BONES, make, spin, 12.0 * x)
    props = {}
    props.update(f.turn("prop_L", "y"))
    props.update(f.turn("prop_R", "y"))
    f.rigged_out("bomber", f.BOMBER_BONES, bomber, [("spin", 0.15, props)], 60.0)
    rigid_out("wingman", wingman, 78.0, 0.5)
    rigid_out("turret", turret, 84.0)
    rigid_out("tank", tank, 90.0)
    mprops = {}
    for i in range(len(MEGA_ENGINES)):
        mprops.update(f.turn(f"prop_{i}", "y"))
    f.rigged_out("mega", MEGA_BONES, mega, [("spin", 0.15, mprops)], 110.0)
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("shmup models written to", os.path.normpath(OUT))
