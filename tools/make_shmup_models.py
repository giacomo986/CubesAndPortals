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
                       ("foe_grey", (0.24, 0.26, 0.30))]:
        d.M[mname] = d.material(mname, rgb)
    d.M["window"] = d.material("window", (0.55, 0.75, 0.85), rough=0.1, alpha=0.3)
    spin = [("spin", 0.15, f.turn("prop", "y"))]
    for x, (name, make) in enumerate([("fighter", fighter), ("foe_dart", dart), ("foe_weaver", weaver),
                                      ("foe_ace", ace)]):
        f.rigged_out(name, f.PLANE_BONES, make, spin, 12.0 * x)
    props = {}
    props.update(f.turn("prop_L", "y"))
    props.update(f.turn("prop_R", "y"))
    f.rigged_out("bomber", f.BOMBER_BONES, bomber, [("spin", 0.15, props)], 60.0)
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("shmup models written to", os.path.normpath(OUT))
