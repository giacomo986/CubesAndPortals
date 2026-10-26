# The wild animals: a pig, a cow, a sheep and a chicken in the classic block game's
# manner, boxes measured in its pixels (1/16 of a node), rigged and animated
# in Blender and exported for mods/animals.
#
#   blender --background --python tools/make_animal_models.py
#   (or run inside Blender, the Text editor or the MCP bridge)
#
# Starts from an empty file, saves it as ~/kart_assets/animals.blend and
# writes pig.gltf, cow.gltf, sheep.gltf, sheep_shorn.gltf and chicken.gltf,
# a young one of each at half the size with a bigger head (pig_baby.gltf and
# so on), and hearts.gltf, worn over one in love. Built the way
# tools/make_pet_models.py builds the dog and the cat, with its helpers: front
# +Y, feet on z = 0, every bone pointing up with no roll. The clips are the
# ones the engine asks a creature for: idle, walk, run.
import importlib.util
import sys
import os

import bpy

sys.dont_write_bytecode = True   # no __pycache__ left in tools/
HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("make_pet_models", os.path.join(HERE, "make_pet_models.py"))
pets = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pets)

OUT = os.path.join(HERE, "..", "mods", "animals")
BLEND = os.path.expanduser("~/kart_assets/animals.blend")

# A pixel, and what a model's names end in: the young ones are the same boxes
# at half the size, named pig_baby and so on.
U = 1 / 16
SUFFIX = ""


def px(bone, mat, x0, y0, z0, x1, y1, z1):
    """A box in pixels: x across, y front to back, z up."""
    return (bone, mat, (x0 * U, y0 * U, z0 * U), (x1 * U, y1 * U, z1 * U))


def legs(mat, hx, fy, by, h, w=4, foot=None):
    """Four legs `h` pixels tall and `w` wide, centred at x = +-hx and at
    y = fy (front) and by (back). A `foot` material caps the bottom 2."""
    out, bones = [], []
    for name, x, y in (("leg_FL", hx, fy), ("leg_FR", -hx, fy), ("leg_BL", hx, by), ("leg_BR", -hx, by)):
        out.append(px(name, mat, x - w / 2, y - w / 2, 0, x + w / 2, y + w / 2, h))
        if foot:
            out.append(px(name, foot, x - w / 2 - 0.05, y - w / 2 - 0.05, 0, x + w / 2 + 0.05, y + w / 2 + 0.05, 2))
        bones.append((name, "body", (x * U, y * U, h * U)))
    return out, bones


def eyes(z0, z1, y, x_in, x_out, white=True):
    """Two eyes on the face at y: a white outer pixel and a dark inner one."""
    out = []
    for s in (1, -1):
        out.append(px("head", "eye", s * x_in, y, z0, s * (x_in + 1), y + 0.2, z1))
        if white:
            out.append(px("head", "eye_white", s * (x_in + 1), y, z0, s * x_out, y + 0.2, z1))
    return [(b, m, tuple(map(min, lo, hi)), tuple(map(max, lo, hi))) for b, m, lo, hi in out]


def big_head(boxes, bones):
    """A young one's head, half again as big about its neck: The classic block game's
    young are mostly head. The grown ones are left as they are."""
    if not SUFFIX:
        return boxes
    pivot = next(at for name, _, at in bones if name == "head")
    grow = lambda corner: tuple(c + (v - c) * 1.5 for v, c in zip(corner, pivot))
    return [(b, m, grow(lo), grow(hi)) if b == "head" else (b, m, lo, hi) for b, m, lo, hi in boxes]


def quadruped(prefix, colours, boxes, bones, walk_s=0.9, swing=0.5):
    prefix += SUFFIX
    boxes = big_head(boxes, bones)
    arm = pets.rig(f"{prefix}_rig", [("root", None, (0, 0, 0))] + bones)
    mats = {k: pets.material(f"{prefix}_{k}", v) for k, v in colours.items()}
    mesh = pets.boxes_mesh(f"{prefix}_mesh", boxes, mats)
    pets.skin(mesh, arm)
    pets.clip(arm, prefix, "idle", 3.0, {
        "head": [(0.0, (0.0, 0.0, 0.0), None), (0.3, (0.0, 0.0, 0.0), None),
                 (0.45, (0.25, 0.0, 0.0), None), (0.7, (0.25, 0.0, 0.0), None)],   # a look at the grass
    })

    def gait(a, bob):
        return {
            "leg_FL": [(0.0, (a, 0, 0), None), (0.5, (-a, 0, 0), None)],
            "leg_BR": [(0.0, (a, 0, 0), None), (0.5, (-a, 0, 0), None)],
            "leg_FR": [(0.0, (-a, 0, 0), None), (0.5, (a, 0, 0), None)],
            "leg_BL": [(0.0, (-a, 0, 0), None), (0.5, (a, 0, 0), None)],
            "root": [(0.0, (0, 0, 0), (0, 0, 0)), (0.25, (0, 0, 0), (0, bob, 0)),
                     (0.5, (0, 0, 0), (0, 0, 0)), (0.75, (0, 0, 0), (0, bob, 0))],
            "head": [(0.0, (0.05, 0, 0), None), (0.25, (-0.03, 0, 0), None),
                     (0.5, (0.05, 0, 0), None), (0.75, (-0.03, 0, 0), None)],
        }
    pets.clip(arm, prefix, "walk", walk_s, gait(swing, 0.01))
    pets.clip(arm, prefix, "run", walk_s * 0.6, gait(swing * 1.6, 0.03))
    pets.tip_over(arm, [mesh])
    return arm, mesh


def pig():
    c = {"skin": (0.94, 0.62, 0.62), "dark": (0.86, 0.50, 0.52), "snout": (0.96, 0.72, 0.72),
         "nostril": (0.55, 0.30, 0.32), "eye": (0.08, 0.06, 0.06), "eye_white": (0.95, 0.95, 0.95),
         "hoof": (0.60, 0.38, 0.36)}
    b, lb = legs("skin", 3, 5, -5, 6, foot="hoof")
    b += [
        px("body", "skin", -5, -8, 6, 5, 8, 14),
        px("body", "dark", -3, -6, 14, 3, 2, 14.2),       # a darker back
        px("head", "skin", -4, 7, 8, 4, 15, 16),
        px("head", "snout", -2, 15, 9, 2, 16, 12),
        px("head", "nostril", -1.5, 16, 10, -0.5, 16.2, 11),
        px("head", "nostril", 0.5, 16, 10, 1.5, 16.2, 11),
        *eyes(13, 14, 15, 2, 4),
        px("body", "skin", -0.5, -9, 11, 0.5, -8, 13),    # a curl of tail
    ]
    return quadruped("pig", c, b, [("body", "root", (0, -5 * U, 6 * U)),
                                   ("head", "body", (0, 8 * U, 12 * U))] + lb, walk_s=0.8)


def cow():
    c = {"hide": (0.36, 0.24, 0.16), "patch": (0.93, 0.92, 0.88), "muzzle": (0.80, 0.70, 0.62),
         "horn": (0.88, 0.86, 0.78), "eye": (0.06, 0.05, 0.04), "eye_white": (0.95, 0.95, 0.95),
         "nostril": (0.30, 0.22, 0.18), "hoof": (0.18, 0.14, 0.12), "udder": (0.93, 0.62, 0.64)}
    b, lb = legs("hide", 4, 6, -6, 12, foot="hoof")
    b += [
        px("body", "hide", -6, -9, 12, 6, 9, 22),
        px("body", "patch", -6.1, -5, 15, 6.1, 0, 21),        # white patches through the flanks
        px("body", "patch", -3, 3, 21.9, 4, 7, 22.1),          # and one on the back
        px("body", "udder", -2, -6, 11, 2, -2, 12),
        px("head", "hide", -4, 8, 16, 4, 14, 24),
        px("head", "patch", -1.5, 13.9, 19, 1.5, 14.1, 24),    # a cinder down the face
        px("head", "muzzle", -3, 14, 16, 3, 15, 20),
        px("head", "nostril", -2, 15, 18, -1, 15.2, 19),
        px("head", "nostril", 1, 15, 18, 2, 15.2, 19),
        *eyes(21, 22, 14, 2, 4),
        px("head", "horn", -5, 11, 23, -4, 12, 26),
        px("head", "horn", 4, 11, 23, 5, 12, 26),
        px("head", "hide", -6, 10, 21, -4, 11, 22),            # ears
        px("head", "hide", 4, 10, 21, 6, 11, 22),
        px("body", "hide", -0.5, -10, 12, 0.5, -9, 21),        # tail
        px("body", "patch", -0.6, -10.1, 11, 0.6, -8.9, 13),
    ]
    return quadruped("cow", c, b, [("body", "root", (0, -6 * U, 12 * U)),
                                   ("head", "body", (0, 9 * U, 18 * U))] + lb, walk_s=1.0, swing=0.4)


def sheep(shorn=False):
    """In its fleece, or shorn: the skin under it, and no cap or trousers."""
    c = {"wool": (0.93, 0.92, 0.89), "wool_dark": (0.85, 0.84, 0.80), "face": (0.80, 0.70, 0.62),
         "eye": (0.06, 0.05, 0.04), "eye_white": (0.95, 0.95, 0.95), "hoof": (0.35, 0.30, 0.27),
         "nose": (0.70, 0.50, 0.50), "skin": (0.86, 0.76, 0.68)}
    b, lb = legs("face", 3, 5, -5, 12, foot="hoof")
    if shorn:
        b += [px("body", "skin", -4, -7, 12, 4, 7, 19),
              px("body", "wool", -1.5, -5, 19, 1.5, 3, 19.5)]      # what the shears left
    else:
        for x in (3, -3):
            for y in (5, -5):   # wool trousers at the top of each leg
                name = ("leg_F" if y > 0 else "leg_B") + ("L" if x > 0 else "R")
                b.append(px(name, "wool", x - 2.5, y - 2.5, 7, x + 2.5, y + 2.5, 12))
        b += [
            px("body", "wool", -5, -8, 11, 5, 8, 21),
            px("body", "wool_dark", -5.1, -6, 13, 5.1, -4, 20),     # the tufts of a fleece
            px("body", "wool_dark", -5.1, 1, 12, 5.1, 3, 19),
            px("body", "wool_dark", -3, -3, 20.9, 2, -1, 21.1),
            px("head", "wool", -3.5, 7, 19, 3.5, 12, 22),           # a woolly cap
        ]
    b += [
        px("head", "face", -3, 7, 15, 3, 14, 21),
        px("head", "nose", -1, 14, 16, 1, 14.2, 17),
        *eyes(18, 19, 14, 1, 3),
        px("head", "face", -4.5, 9, 18, -3, 11, 19),            # ears
        px("head", "face", 3, 9, 18, 4.5, 11, 19),
    ]
    return quadruped("sheep_shorn" if shorn else "sheep", c, b, [("body", "root", (0, -5 * U, 12 * U)),
                                     ("head", "body", (0, 8 * U, 17 * U))] + lb, walk_s=0.9, swing=0.4)


def chicken():
    c = {"feather": (0.97, 0.97, 0.95), "shade": (0.86, 0.86, 0.84), "beak": (0.95, 0.66, 0.15),
         "wattle": (0.85, 0.10, 0.10), "eye": (0.05, 0.05, 0.05), "leg": (0.95, 0.66, 0.15)}
    bones = [("body", "root", (0, 0, 5 * U)),
             ("head", "body", (0, 3 * U, 9 * U)),
             ("leg_L", "body", (1.5 * U, 0, 5 * U)),
             ("leg_R", "body", (-1.5 * U, 0, 5 * U)),
             ("wing_L", "body", (3 * U, 0, 10 * U)),
             ("wing_R", "body", (-3 * U, 0, 10 * U))]
    b = [
        px("body", "feather", -3, -4, 5, 3, 4, 11),
        px("body", "shade", -2, -5, 8, 2, -4, 12),              # a tail of feathers
        px("head", "feather", -2, 2, 9, 2, 5, 15),
        px("head", "beak", -2, 5, 12, 2, 7, 14),
        px("head", "wattle", -1, 5, 10, 1, 6, 12),
        px("head", "eye", 2, 3, 13, 2.2, 4, 14),
        px("head", "eye", -2.2, 3, 13, -2, 4, 14),
        px("wing_L", "shade", 3, -3, 6, 4, 3, 10),
        px("wing_R", "shade", -4, -3, 6, -3, 3, 10),
    ]
    for name, x in (("leg_L", 1.5), ("leg_R", -1.5)):
        b.append(px(name, "leg", x - 0.5, -0.5, 0, x + 0.5, 0.5, 5))
        b.append(px(name, "leg", x - 1.5, -1, 0, x + 1.5, 2, 0.5))    # the toes
    name = "chicken" + SUFFIX
    b = big_head(b, bones)
    arm = pets.rig(f"{name}_rig", [("root", None, (0, 0, 0))] + bones)
    mats = {k: pets.material(f"{name}_{k}", v) for k, v in c.items()}
    mesh = pets.boxes_mesh(f"{name}_mesh", b, mats)
    pets.skin(mesh, arm)

    # A wing flaps about its own Z (the world's -Y): the left one up is -, the right one +.
    def flap(a):
        return {"wing_L": [(0.0, (0, 0, 0), None), (0.5, (0, 0, -a), None)],
                "wing_R": [(0.0, (0, 0, 0), None), (0.5, (0, 0, a), None)]}
    pets.clip(arm, name, "idle", 2.0, {
        "head": [(0.0, (0, 0, 0), None), (0.2, (0.5, 0, 0), None), (0.3, (0, 0, 0), None),
                 (0.6, (0, 0, 0.3), None)],   # a peck, then a look aside
    })

    def gait(a, bob, wings):
        k = {"leg_L": [(0.0, (a, 0, 0), None), (0.5, (-a, 0, 0), None)],
             "leg_R": [(0.0, (-a, 0, 0), None), (0.5, (a, 0, 0), None)],
             "head": [(0.0, (0.15, 0, 0), None), (0.25, (-0.1, 0, 0), None),
                      (0.5, (0.15, 0, 0), None), (0.75, (-0.1, 0, 0), None)],
             "root": [(0.0, (0, 0, 0), (0, 0, 0)), (0.25, (0, 0, 0), (0, bob, 0)),
                      (0.5, (0, 0, 0), (0, 0, 0)), (0.75, (0, 0, 0), (0, bob, 0))]}
        if wings:
            k.update(flap(wings))
        return k
    pets.clip(arm, name, "walk", 0.5, gait(0.6, 0.01, 0))
    pets.clip(arm, name, "run", 0.3, gait(0.9, 0.02, 1.0))   # flapping as it runs
    pets.tip_over(arm, [mesh])
    return arm, mesh


def hearts():
    """Three red hearts, flat, over an animal in love (mods/animals): not
    rigged, the engine turns and bobs the body that wears it."""
    shape = [".XX.XX.", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X..."]
    boxes = []
    for cx, cz, p in ((-0.2, 0.0, 0.028), (0.0, 0.1, 0.036), (0.2, 0.03, 0.028)):
        for row, line in enumerate(shape):
            for col, c in enumerate(line):
                if c == "X":
                    x, z = cx + (col - 3.5) * p, cz + (5 - row) * p
                    boxes.append(("", "heart", (x, -p / 2, z), (x + p, p / 2, z + p)))
    obj = pets.boxes_mesh("hearts", boxes, {"heart": pets.material("heart", (0.9, 0.08, 0.15))})
    obj.vertex_groups.clear()
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, "hearts.gltf"), export_format="GLTF_SEPARATE",
                              use_selection=True, export_animations=False, export_yup=True,
                              export_materials="EXPORT", export_image_format="NONE")
    obj.location = (0, 1.5, 1.5)   # over the rest in the file


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = pets.FPS
    os.makedirs(OUT, exist_ok=True)
    made = (("pig", pig), ("cow", cow), ("sheep", sheep), ("sheep_shorn", lambda: sheep(True)),
            ("chicken", chicken))
    for i, (name, make) in enumerate(made):
        arm, mesh = make()
        pets.export(arm, [mesh], os.path.join(OUT, f"{name}.gltf"))
        arm.location.x = 1.6 * i   # side by side in the file, each exported at the origin
    U, SUFFIX = 1 / 32, "_baby"
    for i, (name, make) in enumerate(made[:3] + made[4:]):
        arm, mesh = make()
        pets.export(arm, [mesh], os.path.join(OUT, f"{name}_baby.gltf"))
        arm.location.x, arm.location.y = 1.6 * i, -1.5
    hearts()
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("pig, cow, sheep, sheep_shorn, chicken, their _baby and hearts in", os.path.normpath(OUT))
