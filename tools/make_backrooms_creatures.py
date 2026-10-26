# The two things that live in the backrooms, built out of boxes in Blender,
# rigged and animated, and exported into games/backrooms/mods/backrooms.
#
#   blender --background --python tools/make_backrooms_creatures.py
#
# Front +Y, standing on the origin, like every creature.
#
#   bacteria  Level 0's: a black stick figure two metres tall, arms to its
#             knees ending in three long fingers, two white points for eyes.
#             It twitches standing, lopes walking, lashes with both arms;
#             dying, it staggers and goes over stiff onto its face.
#   smiler    Level 1's: a face in the dark -- a black head that is hardly
#             there, a wide white grin and two white eyes that glow. It bobs,
#             tilts as it comes, lunges with its grin open; dying, the grin
#             shuts and it sinks to the floor, turning away.
#
# Clips: idle, walk, run, attack, death (docs/MODDING.md, Creature). Leaves
# the work in ~/kart_assets/backrooms.blend.
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

OUT = os.path.join(HERE, "..", "games", "backrooms", "mods", "backrooms")
BLEND = os.path.expanduser("~/kart_assets/backrooms.blend")


def material(name, rgb, glow=0.0):
    m = pets.material(name, rgb)
    if glow:
        bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
        bsdf.inputs["Emission Color"].default_value = (*rgb, 1.0)
        bsdf.inputs["Emission Strength"].default_value = glow
    return m


def bacteria():
    bones = [("root", None, (0, 0, 0)), ("hips", "root", (0, 0, 1.0)), ("torso", "hips", (0, 0, 1.0)),
             ("head", "torso", (0, 0, 1.68)),
             ("arm_l", "torso", (0.2, 0, 1.6)), ("arm_r", "torso", (-0.2, 0, 1.6)),
             ("leg_l", "hips", (0.11, 0, 1.0)), ("leg_r", "hips", (-0.11, 0, 1.0))]
    arm = pets.rig("bacteria_rig", bones)
    mats = {"skin": material("bac_skin", (0.025, 0.022, 0.02)),
            "eyes": material("bac_eyes", (1.0, 1.0, 0.96), 4.0)}
    boxes = [("torso", "skin", (-0.14, -0.07, 1.0), (0.14, 0.07, 1.62)),
             ("torso", "skin", (-0.22, -0.06, 1.55), (0.22, 0.06, 1.64)),   # shoulders
             ("head", "skin", (-0.03, -0.03, 1.62), (0.03, 0.03, 1.76)),    # neck
             ("head", "skin", (-0.1, -0.09, 1.74), (0.1, 0.1, 2.02)),
             ("head", "eyes", (-0.06, 0.1, 1.9), (-0.03, 0.115, 1.93)),
             ("head", "eyes", (0.03, 0.1, 1.9), (0.06, 0.115, 1.93))]
    for side, x in (("l", 0.2), ("r", -0.2)):
        boxes.append((f"arm_{side}", "skin", (x - 0.035, -0.035, 0.6), (x + 0.035, 0.035, 1.62)))
        for k, dx in enumerate((-0.03, 0.0, 0.03)):   # three long fingers
            boxes.append((f"arm_{side}", "skin", (x + dx - 0.012, -0.012 + 0.02 * (k - 1), 0.3),
                          (x + dx + 0.012, 0.012 + 0.02 * (k - 1), 0.62)))
    for side, x in (("l", 0.11), ("r", -0.11)):
        boxes.append((f"leg_{side}", "skin", (x - 0.045, -0.045, 0.0), (x + 0.045, 0.045, 1.02)))
        boxes.append((f"leg_{side}", "skin", (x - 0.05, -0.02, 0.0), (x + 0.05, 0.14, 0.05)))   # foot
    mesh = pets.boxes_mesh("bacteria_mesh", boxes, mats)
    pets.skin(mesh, arm)

    def gait(swing, lean, bob):
        return {
            "torso": [(0.0, (-lean, 0, 0.08), None), (0.5, (-lean, 0, -0.08), None)],
            "hips": [(0.0, (0, 0, 0), (0, 0, 0)), (0.25, (0, 0, 0), (0, bob, 0)),
                     (0.5, (0, 0, 0), (0, 0, 0)), (0.75, (0, 0, 0), (0, bob, 0))],
            "leg_l": [(0.0, (swing, 0, 0), None), (0.5, (-swing, 0, 0), None)],
            "leg_r": [(0.0, (-swing, 0, 0), None), (0.5, (swing, 0, 0), None)],
            "arm_l": [(0.0, (-swing * 0.8, 0, 0.05), None), (0.5, (swing * 0.8, 0, 0.05), None)],
            "arm_r": [(0.0, (swing * 0.8, 0, -0.05), None), (0.5, (-swing * 0.8, 0, -0.05), None)],
            "head": [(0.0, (lean, 0.1, 0), None), (0.5, (lean, -0.1, 0), None)],
        }
    # Standing it twitches: the head snaps aside and back, the fingers jerk.
    pets.clip(arm, "bacteria", "idle", 2.4, {
        "torso": [(0.0, (-0.15, 0, 0), None), (0.5, (-0.2, 0, 0.04), None)],
        "head": [(0.0, (0.1, 0, 0), None), (0.42, (0.1, 0, 0), None), (0.45, (0.1, 0.7, 0.35), None),
                 (0.6, (0.1, 0.7, 0.35), None), (0.63, (0.1, 0, 0), None)],
        "arm_l": [(0.0, (0, 0, 0.04), None), (0.7, (0, 0, 0.04), None), (0.72, (0.25, 0, 0.12), None),
                  (0.8, (0, 0, 0.04), None)],
        "arm_r": [(0.0, (0, 0, -0.04), None), (0.2, (0.2, 0, -0.1), None), (0.25, (0, 0, -0.04), None)],
    })
    pets.clip(arm, "bacteria", "walk", 1.1, gait(0.5, 0.2, 0.03))
    pets.clip(arm, "bacteria", "run", 0.6, gait(0.9, 0.5, 0.06))
    # Both arms up and down at whoever it reaches.
    pets.clip(arm, "bacteria", "attack", 0.7, {
        "torso": [(0.0, (-0.2, 0, 0), None), (0.35, (0.15, 0, 0), None), (0.55, (-0.55, 0, 0), None),
                  (0.8, (-0.2, 0, 0), None)],
        "arm_l": [(0.0, (0, 0, 0), None), (0.35, (2.6, 0, 0.2), None), (0.55, (0.9, 0, 0.1), None),
                  (0.8, (0, 0, 0), None)],
        "arm_r": [(0.0, (0, 0, 0), None), (0.35, (2.6, 0, -0.2), None), (0.55, (0.9, 0, -0.1), None),
                  (0.8, (0, 0, 0), None)],
        "head": [(0.0, (0, 0, 0), None), (0.35, (0.3, 0, 0), None), (0.55, (-0.3, 0, 0), None)],
    })
    # A stagger back, then it goes over forward onto its face, stiff, its
    # arms flung out ahead and its head turned aside. A bone pointing up turns
    # back for a positive X: forward is negative, and an arm that hangs from
    # its shoulder swings forward for a positive one.
    pets.clip(arm, "bacteria", "death", 1.2, {
        "root": [(0.0, (0, 0, 0), (0, 0, 0)), (0.15, (0.12, 0, 0), (0, 0, 0)),
                 (0.6, (-1.5, 0, 0), (0, 0.04, 0)), (0.72, (-1.57, 0, 0), (0, 0.1, 0)),
                 (1.0, (-1.57, 0, 0), (0, 0.1, 0))],
        "arm_l": [(0.0, (0, 0, 0), None), (0.15, (-0.3, 0, 0.2), None), (0.45, (3.0, 0, 0.3), None),
                  (1.0, (3.2, 0, 0.35), None)],
        "arm_r": [(0.0, (0, 0, 0), None), (0.15, (-0.3, 0, -0.2), None), (0.45, (2.9, 0, -0.4), None),
                  (1.0, (3.2, 0, -0.45), None)],
        "head": [(0.0, (0, 0, 0), None), (0.15, (0.4, 0, 0), None), (0.7, (0, 1.0, 0), None),
                 (1.0, (0, 1.2, 0), None)],
    }, loop=False)
    return arm, mesh


def smiler():
    # The head hangs at the height of a face; the engine flies it there (hover).
    H = 1.5
    bones = [("root", None, (0, 0, 0)), ("head", "root", (0, 0, H)), ("jaw", "head", (0, 0, H - 0.05))]
    arm = pets.rig("smiler_rig", bones)
    mats = {"dark": material("smi_dark", (0.03, 0.03, 0.035)),
            "glow": material("smi_glow", (1.0, 1.0, 0.94), 6.0)}
    boxes = [("head", "dark", (-0.3, -0.25, H - 0.3), (0.3, 0.25, H + 0.3)),
             ("head", "glow", (-0.17, 0.25, H + 0.08), (-0.07, 0.27, H + 0.16)),   # eyes
             ("head", "glow", (0.07, 0.25, H + 0.08), (0.17, 0.27, H + 0.16))]
    # The grin: a curve of teeth, upper row on the head, lower on the jaw.
    for i in range(-5, 6):
        x = i * 0.045
        up = 0.012 * i * i / 5.0
        boxes.append(("head", "glow", (x - 0.02, 0.25, H - 0.1 + up), (x + 0.02, 0.27, H - 0.04 + up)))
        boxes.append(("jaw", "glow", (x - 0.02, 0.25, H - 0.17 + up * 1.2), (x + 0.02, 0.27, H - 0.11 + up * 1.2)))
    mesh = pets.boxes_mesh("smiler_mesh", boxes, mats)
    pets.skin(mesh, arm)

    def drift(seconds, bob, tilt):
        return {"head": [(0.0, (0, 0, -tilt), (0, 0, 0)), (0.25, (0.05, 0, 0), (0, bob, 0)),
                         (0.5, (0, 0, tilt), (0, 0, 0)), (0.75, (-0.05, 0, 0), (0, -bob, 0))],
                "jaw": [(0.0, (0, 0, 0), (0, 0, 0)), (0.5, (0, 0, 0), (0, -0.01, 0))]}
    pets.clip(arm, "smiler", "idle", 3.0, drift(3.0, 0.05, 0.06))
    pets.clip(arm, "smiler", "walk", 2.0, drift(2.0, 0.07, 0.12))
    pets.clip(arm, "smiler", "run", 1.2, drift(1.2, 0.08, 0.18))
    # Back, then at them with the grin open wide.
    pets.clip(arm, "smiler", "attack", 0.8, {
        "head": [(0.0, (0, 0, 0), (0, 0, 0)), (0.3, (0.25, 0, 0), (0, 0, 0.2)),
                 (0.5, (-0.2, 0, 0), (0, 0, -0.45)), (0.8, (0, 0, 0), (0, 0, 0))],
        "jaw": [(0.0, (0, 0, 0), (0, 0, 0)), (0.3, (0, 0, 0), (0, -0.04, 0)),
                (0.5, (0, 0, 0), (0, -0.14, 0)), (0.8, (0, 0, 0), (0, 0, 0))],
    })
    # The grin shuts, and the face sinks to the floor turning away.
    pets.clip(arm, "smiler", "death", 1.2, {
        "head": [(0.0, (0, 0, 0), (0, 0, 0)), (0.25, (0, 0.4, 0.1), (0, 0.05, 0)),
                 (0.85, (1.2, 2.4, 0.4), (0, -(H - 0.25), 0)), (1.0, (1.2, 2.4, 0.4), (0, -(H - 0.25), 0))],
        "jaw": [(0.0, (0, 0, 0), (0, 0, 0)), (0.25, (0, 0, 0), (0, 0.06, 0)), (1.0, (0, 0, 0), (0, 0.06, 0))],
    }, loop=False)
    return arm, mesh


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = pets.FPS
    os.makedirs(OUT, exist_ok=True)
    for i, (name, make) in enumerate((("bacteria", bacteria), ("smiler", smiler))):
        arm, mesh = make()
        pets.export(arm, [mesh], os.path.join(OUT, f"{name}.gltf"))
        arm.location.x = 1.5 * i
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("bacteria and smiler in", os.path.normpath(OUT))
