# The garden's insects (mods/garden), built out of boxes in Blender, rigged and
# animated, at their true size: a few centimetres, so a player shrunk to a
# sixteenth (11 cm tall) meets an ant as big as a dog.
#
#   flatpak run org.blender.Blender --background --python tools/make_garden_insects.py
#   (or run inside Blender, the Text editor or the MCP bridge)
#
# Front +Y, feet on z = 0, like every creature; bones as tools/make_pet_models.py
# makes them (pointing up, no roll: own X is the world's X, own Y the world's
# up, own Z the world's back).
#
#   ant        5 cm, near black: head, thorax, gaster, six legs in a tripod
#              gait, antennae that feel about. Dies onto its side.
#   ladybird   3 cm, a red dome with black spots and a black head; six short
#              legs. Dies onto its side.
#   butterfly  9 cm across, orange wings with black tips. Sits with its wings
#              up and slowly opening; flies flapping fast ("fly"); dying, it
#              shuts its wings and falls on its side.
#
# Clips: idle, walk (and fly), death (docs/MODDING.md, Creatures). Leaves the
# work in assets/blender/garden.blend.
import importlib.util
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else \
    os.path.join(os.path.expanduser("~"), "dev", "Cubes and portals", "tools")
_spec = importlib.util.spec_from_file_location("make_pet_models", os.path.join(HERE, "make_pet_models.py"))
pets = importlib.util.module_from_spec(_spec)
sys.modules["make_pet_models"] = pets
_spec.loader.exec_module(pets)

OUT = os.path.join(HERE, "..", "mods", "garden")
BLEND = os.path.join(HERE, "..", "assets", "blender", "garden.blend")


def legs(prefix, ys, z_hip, reach, thick, mat):
    """Six legs, a femur out to the side and a tibia down to the ground, each
    pair at one of `ys`; their bones at the hips."""
    boxes, bones = [], []
    for i, y in enumerate(ys):
        for side, s in (("l", -1), ("r", 1)):
            b = f"{prefix}{side}{i + 1}"
            hip = s * reach * 0.25
            knee = s * reach
            x0, x1 = sorted((hip, knee))
            boxes.append((b, mat, (x0, y - thick, z_hip - thick), (x1, y + thick, z_hip + thick)))
            k0, k1 = sorted((knee, knee + s * thick * 2))
            boxes.append((b, mat, (k0, y - thick, 0.0), (k1, y + thick, z_hip)))
            bones.append((b, "body", (hip, y, z_hip)))
    return boxes, bones


def tripod(prefix, swing, lift):
    """A walk's keys: l1 r2 l3 sweep forward while r1 l2 r3 sweep back, and the
    forward three lifted; turning about the world's up (own Y)."""
    keys = {}
    for i in range(3):
        for side, s in (("l", -1), ("r", 1)):
            first = (i % 2 == 0) == (side == "l")
            a = swing if first else -swing
            up = (lift if first else 0.0) * -s
            keys[f"{prefix}{side}{i + 1}"] = [(0.0, (0, s * a, up), None), (0.5, (0, -s * a, -up * 0.0), None)]
    return keys


def ant():
    mats = {"chitin": pets.material("ant_chitin", (0.10, 0.06, 0.05)),
            "dark": pets.material("ant_dark", (0.04, 0.03, 0.03))}
    boxes = [
        ("gaster", "chitin", (-0.0070, -0.0290, 0.0060), (0.0070, -0.0090, 0.0175)),
        ("body", "dark", (-0.0015, -0.0095, 0.0090), (0.0015, -0.0070, 0.0120)),   # the waist
        ("body", "chitin", (-0.0040, -0.0075, 0.0085), (0.0040, 0.0080, 0.0150)),
        ("head", "chitin", (-0.0060, 0.0080, 0.0090), (0.0060, 0.0200, 0.0190)),
        ("head", "dark", (-0.0050, 0.0200, 0.0090), (-0.0015, 0.0240, 0.0110)),    # mandibles
        ("head", "dark", (0.0015, 0.0200, 0.0090), (0.0050, 0.0240, 0.0110)),
        ("ant_l", "dark", (-0.0055, 0.0170, 0.0185), (-0.0040, 0.0320, 0.0200)),
        ("ant_r", "dark", (0.0040, 0.0170, 0.0185), (0.0055, 0.0320, 0.0200)),
    ]
    lb, lbones = legs("leg_", (0.0050, 0.0, -0.0050), 0.0110, 0.0150, 0.0008, "dark")
    bones = [("body", None, (0, 0, 0.0110)), ("gaster", "body", (0, -0.0080, 0.0120)),
             ("head", "body", (0, 0.0080, 0.0140)), ("ant_l", "head", (-0.0045, 0.0170, 0.0190)),
             ("ant_r", "head", (0.0045, 0.0170, 0.0190))] + lbones
    arm = pets.rig("ant_rig", bones)
    mesh = pets.boxes_mesh("ant", boxes + lb, mats)
    pets.skin(mesh, arm)
    feel = {"ant_l": [(0.0, (0.2, 0.2, 0), None), (0.5, (-0.1, -0.3, 0), None)],
            "ant_r": [(0.0, (-0.1, 0.3, 0), None), (0.5, (0.2, -0.2, 0), None)]}
    pets.clip(arm, "ant", "idle", 1.6, {**feel, "gaster": [(0.0, (0, 0, 0), None), (0.5, (0.08, 0, 0), None)]})
    walk = tripod("leg_", 0.45, 0.25)
    walk.update({"ant_l": [(0.0, (0.15, 0.25, 0), None), (0.5, (0.15, -0.25, 0), None)],
                 "ant_r": [(0.0, (0.15, 0.25, 0), None), (0.5, (0.15, -0.25, 0), None)],
                 "body": [(0.0, (0, 0, 0), (0, 0, 0.0004)), (0.25, (0, 0, 0), (0, 0, 0)),
                          (0.5, (0, 0, 0), (0, 0, 0.0004)), (0.75, (0, 0, 0), (0, 0, 0))]})
    pets.clip(arm, "ant", "walk", 0.4, walk)
    pets.clip(arm, "ant", "run", 0.25, walk)
    pets.tip_over(arm, [mesh])
    return arm, mesh


def ladybird():
    mats = {"red": pets.material("lady_red", (0.80, 0.06, 0.04)),
            "black": pets.material("lady_black", (0.03, 0.03, 0.03)),
            "white": pets.material("lady_white", (0.95, 0.95, 0.92))}
    boxes = [
        ("body", "black", (-0.0100, -0.0110, 0.0035), (0.0100, 0.0100, 0.0060)),   # underside
        ("body", "red", (-0.0120, -0.0140, 0.0060), (0.0120, 0.0120, 0.0120)),
        ("body", "red", (-0.0100, -0.0120, 0.0120), (0.0100, 0.0100, 0.0160)),
        ("body", "red", (-0.0060, -0.0080, 0.0160), (0.0060, 0.0060, 0.0180)),
        ("body", "black", (-0.0006, -0.0140, 0.0118), (0.0006, 0.0122, 0.0183)),   # where the wings meet
        ("head", "black", (-0.0060, 0.0110, 0.0045), (0.0060, 0.0180, 0.0110)),
        ("head", "white", (-0.0050, 0.0170, 0.0080), (-0.0025, 0.0183, 0.0100)),
        ("head", "white", (0.0025, 0.0170, 0.0080), (0.0050, 0.0183, 0.0100)),
    ]
    for x, y in ((0.0080, 0.0040), (0.0080, -0.0060), (0.0040, -0.0100)):
        for s in (-1, 1):
            cx = s * x
            boxes.append(("body", "black", (cx - 0.0016, y - 0.0016, 0.0160), (cx + 0.0016, y + 0.0016, 0.0164)))
    for s in (-1, 1):
        boxes.append(("body", "black", (s * 0.003 - 0.0014, -0.0014, 0.0180), (s * 0.003 + 0.0014, 0.0014, 0.0184)))
    for s in (-1, 1):   # one spot each side low on the shell's flank
        boxes.append(("body", "black", (s * 0.0121 - 0.0002 * (1 - s), -0.0030, 0.0075),
                      (s * 0.0121 + 0.0002 * (1 + s), 0.0000, 0.0100)))
    lb, lbones = legs("leg_", (0.0060, 0.0005, -0.0050), 0.0045, 0.0110, 0.0007, "black")
    bones = [("body", None, (0, 0, 0.0080)), ("head", "body", (0, 0.0110, 0.0080))] + lbones
    arm = pets.rig("ladybird_rig", bones)
    mesh = pets.boxes_mesh("ladybird", boxes + lb, mats)
    pets.skin(mesh, arm)
    pets.clip(arm, "ladybird", "idle", 2.0, {"head": [(0.0, (0, 0.15, 0), None), (0.5, (0, -0.15, 0), None)]})
    walk = tripod("leg_", 0.5, 0.25)
    walk["body"] = [(0.0, (0, 0, 0.03), None), (0.5, (0, 0, -0.03), None)]
    pets.clip(arm, "ladybird", "walk", 0.35, walk)
    pets.clip(arm, "ladybird", "run", 0.25, walk)
    pets.tip_over(arm, [mesh], side=-1.0)
    return arm, mesh


def butterfly():
    mats = {"orange": pets.material("fly_orange", (0.95, 0.45, 0.06)),
            "black": pets.material("fly_black", (0.03, 0.03, 0.03)),
            "white": pets.material("fly_white", (0.95, 0.95, 0.92))}
    boxes = [
        ("body", "black", (-0.0015, -0.0190, 0.0120), (0.0015, 0.0120, 0.0155)),
        ("body", "black", (-0.0022, 0.0120, 0.0115), (0.0022, 0.0165, 0.0160)),   # head
        ("body", "black", (-0.0035, 0.0160, 0.0150), (-0.0027, 0.0300, 0.0158)),  # antennae
        ("body", "black", (0.0027, 0.0160, 0.0150), (0.0035, 0.0300, 0.0158)),
        ("body", "black", (-0.0045, 0.0285, 0.0148), (-0.0027, 0.0305, 0.0162)),
        ("body", "black", (0.0027, 0.0285, 0.0148), (0.0045, 0.0305, 0.0162)),
    ]
    for side, s in (("wing_l", -1), ("wing_r", 1)):
        def wing(x0, x1, y0, y1, m):
            a, b = sorted((s * x0, s * x1))
            boxes.append((side, m, (a, y0, 0.0133), (b, y1, 0.0142)))
        wing(0.0015, 0.0340, 0.0000, 0.0220, "orange")    # forewing
        wing(0.0340, 0.0440, 0.0080, 0.0240, "black")     # its tip
        wing(0.0340, 0.0440, 0.0000, 0.0080, "orange")
        wing(0.0365, 0.0405, 0.0150, 0.0185, "white")     # a white spot in the black
        wing(0.0015, 0.0280, -0.0200, 0.0000, "orange")   # hindwing
        wing(0.0200, 0.0280, -0.0230, -0.0080, "black")
    for y in (0.006, 0.0, -0.006):   # three pairs of thin legs straight down
        for s in (-1, 1):
            boxes.append(("body", "black", (s * 0.0025 - 0.0004, y - 0.0004, 0.0), (s * 0.0025 + 0.0004, y + 0.0004, 0.0122)))
    bones = [("body", None, (0, 0, 0.0135)), ("wing_l", "body", (-0.0015, 0, 0.0137)),
             ("wing_r", "body", (0.0015, 0, 0.0137))]
    arm = pets.rig("butterfly_rig", bones)
    mesh = pets.boxes_mesh("butterfly", boxes, mats)
    pets.skin(mesh, arm)

    def wings(a):   # both up by `a` radians: own Z is the world's back
        return {"wing_l": (0, 0, -a), "wing_r": (0, 0, a)}

    def keys(seq):
        out = {"wing_l": [], "wing_r": []}
        for t, a in seq:
            w = wings(a)
            out["wing_l"].append((t, w["wing_l"], None))
            out["wing_r"].append((t, w["wing_r"], None))
        return out

    pets.clip(arm, "butterfly", "idle", 3.0, keys([(0.0, 1.35), (0.5, 0.7)]))
    fly = keys([(0.0, 1.1), (0.5, -0.6)])
    fly["body"] = [(0.0, (0, 0, 0), (0, 0, -0.002)), (0.5, (0, 0, 0), (0, 0, 0.002))]
    pets.clip(arm, "butterfly", "fly", 0.22, fly)
    pets.clip(arm, "butterfly", "walk", 0.22, fly)
    # Dying: the wings shut over its back, then over onto its side (about the
    # world's front, the body's own Z). Shut, the wings stand where the body
    # is, so lying it sinks into nothing and needs no lift.
    death = keys([(0.0, 0.5), (0.3, 1.5), (1.0, 1.5)])
    death["body"] = [(0.0, (0, 0, 0), None), (0.3, (0, 0, 0), None), (0.6, (0, 0, math.pi / 2), None),
                     (1.0, (0, 0, math.pi / 2), None)]
    pets.clip(arm, "butterfly", "death", 1.0, death, loop=False)
    return arm, mesh


def main():
    if bpy.app.background:
        bpy.ops.wm.read_homefile(use_empty=True)
    else:   # a live Blender (the MCP bridge): a new file would drop its window, so emptied
        for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.armatures, bpy.data.actions,
                     bpy.data.materials):
            for block in list(coll):
                coll.remove(block)
    bpy.context.scene.render.fps = pets.FPS
    made = []
    for i, (name, build) in enumerate((("ant", ant), ("ladybird", ladybird), ("butterfly", butterfly))):
        arm, mesh = build()
        pets.export(arm, [mesh], os.path.join(OUT, f"{name}.gltf"))
        arm.location.x = i * 0.08   # side by side in the .blend
        made.append(name)
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(BLEND))
    print("garden insects:", ", ".join(made))


main()
