# The saddled ladybird of mods/garden, as big as a pony at a body's own scale.
#
#   ladybird_ridden   worn while ridden (core.player_wear), its bones under the
#                     robot rig's "root" with tools/make_pony_ridden.py's rig
#                     and export: worn_idle, worn_walk, worn_run.
#   ladybird_tame     the same creature standing, tamed: idle, walk, run, death.
#   ladybird_tame_small   that one a sixteenth as big, for the garden.
#
# Sizes are in sixteenths of a node, the pony's, so a rider's clips fit: the
# saddle stands where the pony's does (14.6 up). Front is +Y.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_ladybird_mount.py"
import importlib.util
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))


def load(name):
    spec = importlib.util.spec_from_file_location(name, os.path.join(HERE, name + ".py"))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


pets = load("make_pet_models")
ridden = load("make_pony_ridden")

U = 1 / 16
COLOURS = {"red": (0.80, 0.06, 0.04), "black": (0.03, 0.03, 0.03), "white": (0.95, 0.95, 0.92),
           "leather": (0.36, 0.20, 0.10), "iron": (0.70, 0.70, 0.72)}
LEG_Y = (7.5, -3.5, -9.5)   # clear of where the rider's legs hang
HIP = 7.0
LEGS = [f"leg_{side}{i + 1}" for i in range(3) for side in "lr"]

BOXES = [
    ("body", "black", (-4.2, -10, 5), (4.2, 9.5, 6.4)),                # underside
    ("body", "red", (-5, -11, 6.4), (5, 10, 11)),                      # the shell, in three steps
    ("body", "red", (-4.5, -10, 11), (4.5, 9, 13)),
    ("body", "red", (-3.2, -7.5, 13), (3.2, 6, 14)),
    ("body", "black", (-0.3, -11.05, 7), (0.3, -3, 14.05)),            # where the wing cases meet
    ("body", "black", (-0.3, 3, 11), (0.3, 10.05, 14.05)),
    ("head", "black", (-3.4, 9.5, 5.5), (3.4, 14.5, 11)),
    ("head", "white", (-3.0, 14.4, 8.5), (-1.3, 14.7, 10.3)),
    ("head", "white", (1.3, 14.4, 8.5), (3.0, 14.7, 10.3)),
    ("ant_l", "black", (-2.2, 13.5, 10.5), (-1.7, 18.5, 11)),
    ("ant_r", "black", (1.7, 13.5, 10.5), (2.2, 18.5, 11)),
    ("body", "leather", (-5.4, -3, 13.5), (5.4, 3, 14.6)),             # the pony's saddle
    ("body", "leather", (-5.6, -2.5, 9), (-5, 2.5, 13.5)),
    ("body", "leather", (5, -2.5, 9), (5.6, 2.5, 13.5)),
    ("body", "iron", (-6.2, -0.8, 7.5), (-5.4, 0.8, 8.2)),
    ("body", "iron", (5.4, -0.8, 7.5), (6.2, 0.8, 8.2)),
]
for x, y, z0, z1 in ((3.6, 6.5, 11.2, 13.1), (3.5, -7.5, 11.2, 13.1), (5.05, 5.5, 8, 10), (5.05, -7, 8, 10)):
    for s in (-1, 1):
        wide = 1.1 if abs(x) < 5 else 0.05
        BOXES.append(("body", "black", (min(s * x - wide, s * x + wide), y - 1.3, z0),
                      (max(s * x - wide, s * x + wide), y + 1.3, z1)))
for name, y in zip(("1", "2", "3"), LEG_Y):
    for side, s in (("l", -1), ("r", 1)):
        b = f"leg_{side}{name}"
        BOXES.append((b, "black", (min(s * 3.5, s * 8), y - 0.6, HIP - 0.6), (max(s * 3.5, s * 8), y + 0.6, HIP + 0.6)))
        BOXES.append((b, "black", (min(s * 8, s * 9.2), y - 0.6, 0), (max(s * 8, s * 9.2), y + 0.6, HIP + 0.6)))


def wave(ph, k=1.0, off=0.0):
    return k * math.sin(2 * math.pi * ph + off)


# Two tripods: l1 r2 l3 against r1 l2 r3, sweeping about the up axis of the hip.
def step(ph, swing):
    out = {}
    for i in range(3):
        for side, s in (("l", -1), ("r", 1)):
            first = (i % 2 == 0) == (side == "l")
            out[f"leg_{side}{i + 1}"] = wave(ph, swing if first else -swing) * s
    return out


AXIS = (0, 1, 0)   # the hip's bone points up: its own Y


def worn(turns, lift, head=0.0, ants=0.0):
    out = {"lb_" + k: (AXIS, v) for k, v in turns.items()}
    out["lb_head"] = (AXIS, head)
    out["lb_ant_l"] = (AXIS, ants)
    out["lb_ant_r"] = (AXIS, -ants)
    out["lb_body"] = 0.0
    return out, lift


def walk(ph):
    return worn(step(ph, 0.42), 0.01 * abs(wave(2 * ph)), wave(ph, 0.05), wave(ph, 0.2))


def run(ph):
    return worn(step(ph, 0.7), 0.03 * abs(wave(ph)), wave(ph, 0.1), wave(2 * ph, 0.35))


def idle(ph):
    return worn({k: 0.0 for k in LEGS}, 0.006 * wave(ph), wave(ph, 0.14), wave(2 * ph, 0.28))


def make_ridden():
    ridden.OUT = os.path.join(HERE, "..", "mods", "garden")
    ridden.NAME = "ladybird_ridden"
    ridden.BODY = "lb_body"
    ridden.COLOURS = COLOURS
    ridden.BONES = [("lb_body", "root", (0, 0, 8), (0, 4, 8)),
                    ("lb_head", "lb_body", (0, 10, 8), (0, 10, 12)),
                    ("lb_ant_l", "lb_head", (-2, 13.5, 10.7), (-2, 13.5, 14.7)),
                    ("lb_ant_r", "lb_head", (2, 13.5, 10.7), (2, 13.5, 14.7))]
    for name, y in zip(("1", "2", "3"), LEG_Y):
        for side, s in (("l", -1), ("r", 1)):
            ridden.BONES.append((f"lb_leg_{side}{name}", "lb_body", (s * 3.5, y, HIP), (s * 3.5, y, HIP + 4)))
    ridden.BOXES = [("lb_" + b, m, lo, hi) for b, m, lo, hi in BOXES]
    ridden.CLIPS = [("worn_idle", idle, 3.0), ("worn_walk", walk, 0.8), ("worn_run", run, 0.42)]
    ridden.main()


def make_tame(name, scale, side):
    m = U * scale
    mats = {k: pets.material("lb_" + k, v) for k, v in COLOURS.items()}
    boxes = [(b, c, tuple(v * m for v in lo), tuple(v * m for v in hi)) for b, c, lo, hi in BOXES]
    bones = [("body", None, (0, 0, 8 * m)), ("head", "body", (0, 10 * m, 8 * m)),
             ("ant_l", "head", (-2 * m, 13.5 * m, 10.7 * m)), ("ant_r", "head", (2 * m, 13.5 * m, 10.7 * m))]
    for n, y in zip(("1", "2", "3"), LEG_Y):
        for sd, s in (("l", -1), ("r", 1)):
            bones.append((f"leg_{sd}{n}", "body", (s * 3.5 * m, y * m, HIP * m)))
    arm = pets.rig(name + "_rig", bones)
    mesh = pets.boxes_mesh(name, boxes, mats)
    pets.skin(mesh, arm)

    def keys(fn, n=8):
        ks = {}
        for f in range(n):
            ph = f / n
            for bone, turn in fn(ph).items():
                ks.setdefault(bone, []).append((ph, (0, turn, 0), None))
        return ks

    feel = lambda ph, k: {"head": wave(ph, 0.1 * k), "ant_l": wave(2 * ph, 0.28 * k), "ant_r": -wave(2 * ph, 0.28 * k)}
    pets.clip(arm, name, "idle", 3.0, keys(lambda ph: feel(ph, 1.0)))
    pets.clip(arm, name, "walk", 0.8, keys(lambda ph: {**step(ph, 0.42), **feel(ph, 0.5)}))
    pets.clip(arm, name, "run", 0.42, keys(lambda ph: {**step(ph, 0.7), **feel(ph, 1.0)}))
    pets.tip_over(arm, [mesh], side=-1.0)
    arm.location.x = side
    pets.export(arm, [mesh], os.path.join(HERE, "..", "mods", "garden", name + ".gltf"))


def main():
    make_ridden()   # opens its own empty file with the robot's rig in it
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = pets.FPS
    make_tame("ladybird_tame", 1.0, 0.0)
    for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.armatures, bpy.data.actions):
        for block in list(coll):
            coll.remove(block)
    make_tame("ladybird_tame_small", 1 / 16, 0.0)
    print("ladybird mount models written")


main()
