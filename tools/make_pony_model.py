#!/usr/bin/env python3
# The pony of mods/animals, a mount: short legs and a long barrel at a rider's
# hips, so a body standing over it has its legs hidden in its flanks and its
# feet where stirrups would be -- the pony is worn under the rider's feet while
# ridden (core.player_wear), the same model a free one walks in. A bay coat, a
# dark mane and tail, a white blaze, a saddle; built the way
# tools/make_animal_models.py builds the cow, with its helpers (idle, walk,
# run and death clips).
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_pony_model.py"
#
# Writes mods/animals/pony.gltf.
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

OUT = os.path.join(HERE, "..", "mods", "animals")


def pony():
    c = {"coat": (0.52, 0.30, 0.16), "belly": (0.60, 0.38, 0.22), "muzzle": (0.30, 0.20, 0.14),
         "mane": (0.10, 0.07, 0.05), "eye": (0.06, 0.05, 0.04), "eye_white": (0.95, 0.95, 0.95),
         "hoof": (0.16, 0.13, 0.10), "blaze": (0.94, 0.92, 0.88), "leather": (0.36, 0.20, 0.10),
         "iron": (0.70, 0.70, 0.72)}
    b, lb = legs("coat", 3.5, 7, -7, 7, w=3, foot="hoof")
    b += [
        px("body", "coat", -5, -10, 7, 5, 10, 14),
        px("body", "belly", -4, -8, 6.8, 4, 8, 7.2),
        # The saddle, its flaps and stirrups.
        px("body", "leather", -5.4, -3, 13.5, 5.4, 3, 14.6),
        px("body", "leather", -5.6, -2.5, 9, -5, 2.5, 13.5),
        px("body", "leather", 5, -2.5, 9, 5.6, 2.5, 13.5),
        px("body", "iron", -6.2, -0.8, 7.5, -5.4, 0.8, 8.2),
        px("body", "iron", 5.4, -0.8, 7.5, 6.2, 0.8, 8.2),
        # The neck rising to the head, the ears, the mane down the neck.
        px("head", "coat", -2.5, 8, 11, 2.5, 13, 19),
        px("head", "coat", -3, 12, 16, 3, 20, 22),
        px("head", "muzzle", -2.5, 18, 15.5, 2.5, 21.5, 19),
        px("head", "blaze", -0.8, 19.9, 17, 0.8, 20.2, 21.5),
        *eyes(19, 20, 19.9, 1.5, 3),
        px("head", "coat", -2.6, 13, 22, -1.4, 14.2, 24.5),
        px("head", "coat", 1.4, 13, 22, 2.6, 14.2, 24.5),
        px("head", "mane", -0.8, 8, 17, 0.8, 13.5, 23),
        px("body", "mane", -0.8, -11, 8, 0.8, -10, 14),     # tail
        px("body", "mane", -1.2, -11.6, 5, 1.2, -10.6, 9.5),
    ]
    return an.quadruped("pony", c, b, [("body", "root", (0, -7 * U, 7 * U)),
                                       ("head", "body", (0, 10 * U, 15 * U))] + lb, walk_s=0.8, swing=0.5)


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = an.pets.FPS
    arm, mesh = pony()
    an.pets.export(arm, [mesh], os.path.join(OUT, "pony.gltf"))
    print("pony in", os.path.normpath(OUT))
