# The mule of a village's caravan (mods/villagers): a grey-brown quadruped
# with long ears, a short mane and a pack saddle with a pannier on each side,
# built the way tools/make_animal_models.py builds the cow, with its helpers
# (idle, walk, run and death clips).
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_mule_model.py"
#
# Writes mods/villagers/mule.gltf.
import importlib.util
import os
import sys

import bpy

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("make_animal_models", os.path.join(HERE, "make_animal_models.py"))
an = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(an)
px, legs, eyes, U = an.px, an.legs, an.eyes, an.U

OUT = os.path.join(HERE, "..", "mods", "villagers")


def mule():
    c = {"coat": (0.42, 0.36, 0.31), "belly": (0.70, 0.64, 0.56), "muzzle": (0.78, 0.72, 0.64),
         "mane": (0.16, 0.13, 0.11), "eye": (0.06, 0.05, 0.04), "eye_white": (0.95, 0.95, 0.95),
         "hoof": (0.14, 0.12, 0.10), "leather": (0.40, 0.24, 0.12), "canvas": (0.72, 0.62, 0.42),
         "rope": (0.62, 0.52, 0.34)}
    b, lb = legs("coat", 4, 7, -7, 13, w=3, foot="hoof")
    b += [
        px("body", "coat", -5, -10, 13, 5, 10, 22),
        px("body", "belly", -4, -8, 12.8, 4, 8, 13.2),
        # The pack saddle and its panniers, one each side, and the rope over.
        px("body", "leather", -5.5, -5, 21.5, 5.5, 4, 23),
        px("body", "canvas", -9, -5, 14, -5, 4, 21),
        px("body", "canvas", 5, -5, 14, 9, 4, 21),
        px("body", "leather", -9.2, -5.2, 19, -4.8, 4.2, 20),
        px("body", "leather", 4.8, -5.2, 19, 9.2, 4.2, 20),
        px("body", "rope", -3, -4, 23, 3, 3, 25),            # a roll of goods on top
        px("body", "rope", -0.5, -4.5, 22.8, 0.5, 3.5, 25.3),
        # The neck rising to the head, long ears, the mane.
        px("head", "coat", -2.5, 8, 18, 2.5, 13, 27),
        px("head", "coat", -3, 12, 23, 3, 21, 29),
        px("head", "muzzle", -2.5, 19, 22.5, 2.5, 22, 26),
        *eyes(26, 27, 21, 1, 3),
        px("head", "coat", -3, 13, 29, -1.5, 14.5, 34),
        px("head", "coat", 1.5, 13, 29, 3, 14.5, 34),
        px("head", "mane", -1, 8, 26, 1, 14, 29.5),
        px("body", "mane", -0.6, -11, 15, 0.6, -10, 22),     # tail
        px("body", "mane", -1, -11.5, 12, 1, -10.5, 16),
    ]
    return an.quadruped("mule", c, b, [("body", "root", (0, -7 * U, 13 * U)),
                                       ("head", "body", (0, 10 * U, 21 * U))] + lb, walk_s=0.9, swing=0.45)


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = an.pets.FPS
    arm, mesh = mule()
    an.pets.export(arm, [mesh], os.path.join(OUT, "mule.gltf"))
    print("mule in", os.path.normpath(OUT))
