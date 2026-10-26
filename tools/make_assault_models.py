# The assault's own things (games/assault, assault_game), built with the
# helpers of tools/make_fps_models.py and the front's soldier
# (tools/make_front_models.py). The guns, the allies and the machines are the
# front's, copied into the mod.
#
#   flatpak run org.blender.Blender --background --python tools/make_assault_models.py
#
#   zombie, demon, cyborg
#                   the legion, three kinds on the front soldier's rig and
#                   clips (a gun in their hands): a dead soldier in rags, a
#                   red demon on hooves, a plated cyborg with a cyan visor
#   basoalth        Basoalth on his throne, 2.6 high: black armour, a horned
#                   crown, a cape, violet eyes and a violet heart in the
#                   chest. Clips idle, absorb (arms up, drawing the core in)
#   basoalth_giant  the same thirteen times as large (34 high), for the
#                   escape: idle, walk, smash (a fist up and down), reel (hit
#                   on the head: thrown back), rise (grown out of nothing),
#                   death (back into the magma); and what the tunnel's traps
#                   do to him: burn (in a valve's lava: stamping, beating at
#                   his legs, his head thrown back), trip (over fallen rocks:
#                   down on a knee and his hands, and up), blast (a gas tank
#                   in his face: thrown back, arms up across it)
#
#   ... -- --giant   only basoalth_giant (the others as they are)
#   builder         an allied builder on the same rig, in a hard hat and a
#                   high-visibility vest, with a clip "build" (hammering)
#   wrench_held     the spanner in its hand
#   core            the hall's core: a violet crystal in three rings; idle turns
#   pylon           one of the four that feed its shield: a basalt column, coils,
#                   a glowing head; idle hums
#   pylon_dead      the same broken off, dark
import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_fps_models as d  # noqa: E402
import make_front_models as fm  # noqa: E402

OUT = os.path.join(d.ROOT, "games", "assault", "mods", "assault_game")
BLEND = os.path.expanduser("~/kart_assets/assault.blend")


def basoalth(name, k, clips, x):
    """Rigged as the demons are, every length times k."""
    def s(v):
        return tuple(c * k for c in v)
    bones = [(n, par, s(h)) for n, par, h in d.biped(name, 1.25, 1.75, 2.25, 2.1, 0.38)]
    rg = d.rig(f"{name}_rig", bones)
    p = d.Parts()
    for sx, b in ((-1, "leg_L"), (1, "leg_R")):
        lx = sx * 0.16
        p.limb(b, "legion_armour", s((lx, 0, 1.25)), s((lx, 0, 0.12)), 0.22 * k, 0.24 * k)
        p.box(b, "legion_trim", s((lx, 0.05, 0.08)), s((0.22, 0.38, 0.16)))           # sabatons
        p.box(b, "legion_trim", s((lx, 0.12, 0.7)), s((0.2, 0.06, 0.18)))            # knee
    p.box("hips", "legion_armour", s((0, 0, 1.3)), s((0.5, 0.3, 0.22)))
    p.box("hips", "legion_trim", s((0, 0.0, 1.18)), s((0.56, 0.34, 0.06)))
    p.box("chest", "legion_armour", s((0, 0, 1.75)), s((0.66, 0.36, 0.7)))
    p.box("chest", "legion_trim", s((0, 0.17, 1.8)), s((0.5, 0.06, 0.5)))
    p.ball("chest", "basoalth_glow", s((0, 0.21, 1.82)), 0.11 * k, (1, 0.5, 1.2), 2)   # the heart
    for sx in (-1, 1):                                                                # pauldrons, spiked
        p.ball("chest", "legion_trim", s((sx * 0.42, 0, 2.08)), 0.17 * k, (1.2, 1, 0.8), 1)
        p.spike("chest", "legion_bone", s((sx * 0.48, 0, 2.18)), s((sx * 0.62, 0, 2.5)), 0.05 * k)
    p.box("chest", "legion_cape", s((0, -0.22, 1.5)), s((0.62, 0.04, 1.2)), (0.08, 0, 0))   # the cape
    p.box("head", "legion_skin", s((0, 0.02, 2.36)), s((0.26, 0.26, 0.3)))
    p.box("head", "legion_armour", s((0, -0.02, 2.4)), s((0.3, 0.28, 0.34)))          # the helm
    p.box("head", "basoalth_glow", s((-0.07, 0.15, 2.4)), s((0.06, 0.02, 0.03)))       # eyes
    p.box("head", "basoalth_glow", s((0.07, 0.15, 2.4)), s((0.06, 0.02, 0.03)))
    for i in range(5):                                                                # the crown
        a = (i - 2) * 0.45
        p.spike("head", "legion_bone", s((math.sin(a) * 0.14, math.cos(a) * 0.1, 2.56)),
                s((math.sin(a) * 0.2, math.cos(a) * 0.12, 2.8 - abs(i - 2) * 0.06)), 0.035 * k)
    for sx in (-1, 1):                                                                # horns
        p.spike("head", "legion_bone", s((sx * 0.15, 0, 2.5)), s((sx * 0.38, -0.05, 2.75)), 0.05 * k)
    for sx, b in ((-1, "arm_L"), (1, "arm_R")):
        p.limb(b, "legion_armour", s((sx * 0.4, 0, 2.08)), s((sx * 0.46, 0.05, 1.32)), 0.17 * k)
        p.box(b, "legion_trim", s((sx * 0.46, 0.05, 1.3)), s((0.16, 0.18, 0.18)))      # gauntlet
        p.box(b, "basoalth_glow", s((sx * 0.46, 0.13, 1.3)), s((0.08, 0.02, 0.08)))
    mesh = p.build(f"{name}_mesh")
    d.skin(mesh, rg)
    for clip_name, seconds, keys, loop in clips:
        d.clip(rg, clip_name, seconds, keys, loop=loop)
    d.export([rg, mesh], os.path.join(OUT, f"{name}.gltf"), True)
    d.put_aside([rg], x)


# ---------------------------------------------------------------- the legion
#
# Three kinds on the front soldier's bones and clips (fm.soldier's body),
# their arms reaching the same grip so the gun in their hands sits right:
# the right hand at (0.12, 0.28, 1.12), the left at (-0.04, 0.42, 1.12).

def arms(p, sleeve, hand, w=0.11):
    p.limb("arm_R", sleeve, (0.22, 0, 1.4), (0.2, 0.28, 1.14), w)
    p.limb("arm_L", sleeve, (-0.22, 0, 1.4), (-0.08, 0.36, 1.12), w)
    p.box("arm_R", hand, (0.12, 0.28, 1.12), (0.08, 0.09, 0.08))
    p.box("arm_L", hand, (-0.04, 0.42, 1.12), (0.08, 0.09, 0.08))


def zombie_body(p):
    """A dead soldier walking: grey-green skin, the uniform in rags, the ribs
    out on one side, no helmet, a lolling head with sunken glowing eyes."""
    for sx, b in ((-1, "leg_L"), (1, "leg_R")):
        lx = sx * 0.11
        p.limb(b, "rag", (lx, 0, 0.88), (lx + sx * 0.02, 0, 0.42), 0.15, 0.16)
        p.limb(b, "rot_skin", (lx + sx * 0.02, 0, 0.44), (lx, 0.02, 0.1), 0.11, 0.12)   # bare shin
        p.box(b, "polymer", (lx, 0.03, 0.05), (0.14, 0.24, 0.09))
    p.box("hips", "rag", (0, 0, 0.9), (0.36, 0.2, 0.14))
    p.box("chest", "rag", (0.06, 0, 1.17), (0.32, 0.24, 0.46))
    p.box("chest", "rot_skin", (-0.15, 0.0, 1.17), (0.12, 0.2, 0.42))                 # the torn side
    for k in range(4):
        p.box("chest", "legion_bone", (-0.2, 0.06, 1.0 + k * 0.1), (0.04, 0.12, 0.025))   # ribs
    p.box("chest", "rag", (0.12, 0.13, 0.98), (0.14, 0.02, 0.16), (0.3, 0, 0.2))      # a hanging flap
    p.box("head", "rot_skin", (0.02, 0.03, 1.55), (0.2, 0.21, 0.23), (0.15, 0, 0.18))
    p.box("head", "rot_dark", (0.02, 0.0, 1.67), (0.21, 0.22, 0.03))                  # matted hair
    p.box("head", "zombie_eye", (-0.04, 0.135, 1.58), (0.035, 0.01, 0.025))
    p.box("head", "zombie_eye", (0.07, 0.135, 1.57), (0.035, 0.01, 0.025))
    p.box("head", "rot_dark", (0.02, 0.13, 1.49), (0.1, 0.01, 0.03))                  # the jaw hanging
    arms(p, "rot_skin", "rot_dark", 0.1)
    p.box("arm_L", "rag", (-0.2, 0.05, 1.33), (0.14, 0.14, 0.16))                    # a sleeve stump


def demon_body(p):
    """A demon of the pit: red hide, legs bent back to hooves, a black
    harness, shoulders of horn, a horned head with burning eyes, a tail."""
    for sx, b in ((-1, "leg_L"), (1, "leg_R")):
        lx = sx * 0.12
        p.limb(b, "demon_hide", (lx, 0, 0.88), (lx, 0.1, 0.5), 0.17, 0.18)           # thigh forward
        p.limb(b, "demon_hide", (lx, 0.1, 0.5), (lx, -0.06, 0.16), 0.12, 0.13)        # shank back
        p.box(b, "demon_horn", (lx, 0.0, 0.07), (0.13, 0.18, 0.14))                   # hoof
    p.box("hips", "demon_hide", (0, 0, 0.9), (0.38, 0.22, 0.16))
    p.box("hips", "polymer", (0, 0, 0.97), (0.4, 0.24, 0.05))
    p.limb("hips", "demon_hide", (0, -0.12, 0.92), (0, -0.45, 0.62), 0.06)             # tail
    p.spike("hips", "demon_horn", (0, -0.45, 0.62), (0, -0.62, 0.5), 0.05)
    p.box("chest", "demon_hide", (0, 0, 1.18), (0.48, 0.27, 0.48))
    p.box("chest", "demon_dark", (0, 0.13, 1.22), (0.3, 0.03, 0.3))                   # chest plate
    p.limb("chest", "polymer", (-0.22, 0.14, 1.38), (0.18, 0.14, 0.98), 0.05, 0.02)   # harness
    for sx in (-1, 1):
        p.spike("chest", "demon_horn", (sx * 0.24, 0, 1.4), (sx * 0.36, -0.05, 1.58), 0.05)
    p.box("head", "demon_hide", (0, 0.03, 1.56), (0.22, 0.22, 0.22))
    p.box("head", "demon_dark", (0, 0.13, 1.5), (0.18, 0.04, 0.07))                   # muzzle
    for sx in (-1, 1):
        p.spike("head", "demon_horn", (sx * 0.09, 0, 1.66), (sx * 0.2, -0.08, 1.86), 0.04)
        p.box("head", "demon_eye", (sx * 0.055, 0.14, 1.59), (0.05, 0.01, 0.025))
    arms(p, "demon_hide", "demon_dark", 0.12)
    for sx, b, hx in ((1, "arm_R", 0.12), (-1, "arm_L", -0.04)):
        p.spike(b, "demon_horn", (hx, 0.3 if sx > 0 else 0.44, 1.1), (hx, 0.36 if sx > 0 else 0.5, 1.06), 0.02)  # claws


def cyborg_body(p):
    """A cyborg of the legion: grey plating over black, a cyan visor, a
    glowing core in the chest, the right arm all machine with pistons."""
    for sx, b in ((-1, "leg_L"), (1, "leg_R")):
        lx = sx * 0.11
        p.limb(b, "cyber_under", (lx, 0, 0.88), (lx, 0, 0.1), 0.14, 0.15)
        p.box(b, "cyber_plate", (lx, 0.05, 0.62), (0.16, 0.06, 0.3))                  # thigh plate
        p.box(b, "cyber_plate", (lx, 0.05, 0.3), (0.15, 0.06, 0.22))                  # shin plate
        p.box(b, "cyber_plate", (lx, 0.04, 0.05), (0.16, 0.27, 0.1))                  # boot
    p.box("hips", "cyber_under", (0, 0, 0.9), (0.36, 0.2, 0.14))
    p.box("hips", "cyber_plate", (0, 0, 0.97), (0.4, 0.23, 0.05))
    p.box("chest", "cyber_under", (0, 0, 1.17), (0.42, 0.24, 0.46))
    p.box("chest", "cyber_plate", (0, 0.03, 1.22), (0.46, 0.25, 0.32))
    p.box("chest", "cyber_glow", (0, 0.16, 1.22), (0.1, 0.02, 0.1))                    # the core
    p.box("chest", "cyber_plate", (0, -0.16, 1.24), (0.3, 0.1, 0.3))                  # back pack
    p.cyl("chest", "cyber_under", (0.1, -0.2, 1.48), 0.025, 0.2)                      # antenna
    p.box("head", "cyber_plate", (0, 0.02, 1.56), (0.22, 0.22, 0.24))
    p.box("head", "cyber_glow", (0, 0.135, 1.58), (0.18, 0.012, 0.04))                # visor
    p.box("head", "cyber_under", (0, 0.13, 1.49), (0.12, 0.01, 0.06))                 # grille
    p.box("head", "cyber_plate", (0, 0.0, 1.69), (0.16, 0.18, 0.03))
    arms(p, "cyber_under", "cyber_plate", 0.1)
    p.limb("arm_R", "cyber_plate", (0.22, 0, 1.4), (0.2, 0.15, 1.27), 0.14)           # upper arm plate
    p.cyl("arm_R", "steel", (0.24, 0.08, 1.3), 0.02, 0.22, rot=(0.9, 0, 0))           # piston
    p.box("arm_L", "cyber_plate", (-0.22, 0.02, 1.38), (0.15, 0.15, 0.1))             # shoulder


def builder_body(p):
    """An allied builder: overalls, an orange high-visibility vest with grey
    stripes, a yellow hard hat, a tool belt, gloves."""
    for sx, b in ((-1, "leg_L"), (1, "leg_R")):
        lx = sx * 0.11
        p.limb(b, "overall", (lx, 0, 0.88), (lx, 0, 0.1), 0.15, 0.16)
        p.box(b, "polymer", (lx, 0.03, 0.05), (0.15, 0.26, 0.1))
    p.box("hips", "overall", (0, 0, 0.9), (0.36, 0.2, 0.14))
    p.box("hips", "leather", (0, 0, 0.96), (0.4, 0.24, 0.06))                      # tool belt
    for sx in (-0.14, 0.14):
        p.box("hips", "leather", (sx, 0.12, 0.9), (0.08, 0.04, 0.1))               # pouches
    p.box("chest", "overall", (0, 0, 1.17), (0.42, 0.24, 0.46))
    p.box("chest", "hivis", (0, 0, 1.2), (0.44, 0.26, 0.36))                        # vest
    for z in (1.1, 1.26):
        p.box("chest", "stripe", (0, 0, z), (0.45, 0.27, 0.03))
    p.box("head", "skin", (0, 0.02, 1.55), (0.2, 0.2, 0.22))
    p.box("head", "hardhat", (0, 0.0, 1.68), (0.26, 0.26, 0.08))                    # hard hat
    p.box("head", "hardhat", (0, 0.04, 1.64), (0.3, 0.32, 0.02))                    # its brim
    p.box("head", "polymer", (-0.05, 0.12, 1.56), (0.04, 0.01, 0.02))
    p.box("head", "polymer", (0.05, 0.12, 1.56), (0.04, 0.01, 0.02))
    arms(p, "overall", "glove")


def builder_clips(rg, hidden):
    """build: the right arm up and down with the tool, bent over the work."""
    keys = {"chest": [(0.0, {"r": (0.35, 0, 0)})], "head": [(0.0, {"r": (0.2, 0, 0)})],
            "arm_R": [(0.0, {"r": (0.2, 0, 0)}), (0.4, {"r": (1.6, 0, 0)}), (0.55, {"r": (-0.2, 0, 0)}),
                      (1.0, {"r": (0.2, 0, 0)})],
            "arm_L": [(0.0, {"r": (0.6, 0, 0.2)})]}
    d.clip(rg, "build", 0.7, keys, hidden, loop=True)


def wrench():
    """The builder's spanner, about its grip, +Y along it (a held model)."""
    p = d.Parts()
    p.box(None, "steel", (0, 0.12, 0), (0.035, 0.3, 0.02))
    p.box(None, "steel", (0, 0.3, 0), (0.1, 0.06, 0.025))
    p.box(None, "steel", (-0.03, 0.34, 0), (0.025, 0.04, 0.025))
    p.box(None, "steel", (0.03, 0.34, 0), (0.025, 0.04, 0.025))
    p.box(None, "red", (0, 0.0, 0), (0.04, 0.12, 0.03))                              # grip
    obj = p.build("wrench_held")
    d.export([obj], os.path.join(OUT, "wrench_held.gltf"), False)
    d.put_aside([obj], 60.0)


IDLE = ("idle", 3.0, {
    "chest": [(0.0, {}), (0.5, {"r": (0.03, 0, 0)})],
    "head": [(0.0, {}), (0.5, {"r": (-0.05, 0, 0.1)})],
    "arm_L": [(0.0, {}), (0.5, {"r": (0.05, 0, -0.05)})],
    "arm_R": [(0.0, {}), (0.5, {"r": (0.05, 0, 0.05)})],
}, True)

ABSORB = ("absorb", 2.0, {
    "arm_L": [(0.0, {"r": (2.6, 0, -0.5)}), (0.5, {"r": (2.8, 0, -0.7)})],
    "arm_R": [(0.0, {"r": (2.6, 0, 0.5)}), (0.5, {"r": (2.8, 0, 0.7)})],
    "head": [(0.0, {"r": (-0.4, 0, 0)})],
    "chest": [(0.0, {"r": (-0.15, 0, 0)}), (0.5, {"r": (-0.22, 0, 0)})],
}, True)


def giant_clips(k):
    walk = d.gait(0.4, 0.3, 0.03 * k, 0.1)
    return [
        IDLE,
        ("walk", 2.2, walk, True),
        ("smash", 1.6, {
            "arm_R": [(0.0, {}), (0.45, {"r": (2.9, 0, 0.3)}), (0.75, {"r": (-0.6, 0, 0.1)}), (1.0, {})],
            "chest": [(0.0, {}), (0.45, {"r": (-0.25, 0, 0.2)}), (0.75, {"r": (0.45, 0, -0.1)}), (1.0, {})],
            "head": [(0.0, {}), (0.45, {"r": (-0.3, 0, 0)}), (0.75, {"r": (0.3, 0, 0)}), (1.0, {})],
        }, False),
        ("reel", 3.5, {
            "head": [(0.0, {}), (0.12, {"r": (-0.7, 0, 0.4)}), (0.6, {"r": (-0.4, 0, -0.3)}), (1.0, {})],
            "chest": [(0.0, {}), (0.12, {"r": (-0.35, 0, 0)}), (0.6, {"r": (-0.2, 0, 0.2)}), (1.0, {})],
            "arm_L": [(0.0, {}), (0.15, {"r": (1.6, 0, -0.9)}), (0.7, {"r": (1.2, 0, -0.5)}), (1.0, {})],
            "arm_R": [(0.0, {}), (0.15, {"r": (2.2, 0, 0.6)}), (0.7, {"r": (1.6, 0, 0.3)}), (1.0, {})],
        }, False),
        ("rise", 3.0, {
            "root": [(0.0, {"s": 0.08}), (0.7, {"s": 1.05}), (1.0, {"s": 1.0})],
            "arm_L": [(0.0, {"r": (2.6, 0, -0.6)}), (0.7, {"r": (2.9, 0, -1.0)}), (1.0, {})],
            "arm_R": [(0.0, {"r": (2.6, 0, 0.6)}), (0.7, {"r": (2.9, 0, 1.0)}), (1.0, {})],
            "head": [(0.0, {"r": (-0.5, 0, 0)}), (1.0, {})],
        }, False),
        # On this rig a chest, head or hips turned by a negative x leans
        # forwards, a leg or an arm by a positive one swings forwards.
        ("burn", 1.0, {
            "leg_L": [(0.0, {"r": (0.8, 0, 0)}), (0.5, {"r": (-0.2, 0, 0)}), (1.0, {"r": (0.8, 0, 0)})],
            "leg_R": [(0.0, {"r": (-0.2, 0, 0)}), (0.5, {"r": (0.8, 0, 0)}), (1.0, {"r": (-0.2, 0, 0)})],
            "arm_L": [(0.0, {"r": (-0.5, 0, -0.4)}), (0.25, {"r": (1.4, 0, -0.9)}), (0.5, {"r": (-0.3, 0, -0.3)}),
                      (0.75, {"r": (1.2, 0, -0.7)}), (1.0, {"r": (-0.5, 0, -0.4)})],
            "arm_R": [(0.0, {"r": (1.3, 0, 0.8)}), (0.25, {"r": (-0.4, 0, 0.3)}), (0.5, {"r": (1.4, 0, 0.9)}),
                      (0.75, {"r": (-0.3, 0, 0.4)}), (1.0, {"r": (1.3, 0, 0.8)})],
            "chest": [(0.0, {"r": (-0.25, 0, 0.15)}), (0.5, {"r": (-0.35, 0, -0.15)}), (1.0, {"r": (-0.25, 0, 0.15)})],
            "head": [(0.0, {"r": (0.6, 0, 0.2)}), (0.5, {"r": (0.7, 0, -0.2)}), (1.0, {"r": (0.6, 0, 0.2)})],
            "root": [(0.0, {"l": (0, 0, 0)}), (0.25, {"l": (0, 0.04 * k, 0)}), (0.5, {"l": (0, 0, 0)}),
                     (0.75, {"l": (0, 0.04 * k, 0)}), (1.0, {"l": (0, 0, 0)})],
        }, True),
        ("trip", 4.0, {
            "root": [(0.0, {}), (0.12, {"l": (0, -0.1 * k, 0)}), (0.3, {"l": (0, -0.32 * k, 0)}),
                     (0.6, {"l": (0, -0.32 * k, 0)}), (0.82, {"l": (0, -0.1 * k, 0)}), (1.0, {})],
            "hips": [(0.0, {}), (0.12, {"r": (-0.3, 0, 0)}), (0.3, {"r": (-0.55, 0, 0)}), (0.6, {"r": (-0.5, 0, 0.05)}),
                     (0.82, {"r": (-0.2, 0, 0)}), (1.0, {})],
            "chest": [(0.0, {}), (0.12, {"r": (-0.3, 0, 0)}), (0.3, {"r": (-0.35, 0, 0)}), (0.6, {"r": (-0.3, 0, -0.1)}),
                      (0.85, {"r": (-0.1, 0, 0)}), (1.0, {})],
            "leg_L": [(0.0, {}), (0.12, {"r": (0.9, 0, 0)}), (0.3, {"r": (1.1, 0, 0)}), (0.6, {"r": (1.0, 0, 0)}),
                      (0.82, {"r": (0.5, 0, 0)}), (1.0, {})],
            "leg_R": [(0.0, {}), (0.12, {"r": (0.1, 0, 0)}), (0.3, {"r": (-0.2, 0, 0)}), (0.6, {"r": (-0.2, 0, 0)}),
                      (0.82, {"r": (0.0, 0, 0)}), (1.0, {})],
            "arm_L": [(0.0, {}), (0.12, {"r": (1.6, 0, -0.4)}), (0.3, {"r": (0.7, 0, -0.2)}), (0.6, {"r": (0.7, 0, -0.2)}),
                      (0.85, {"r": (0.3, 0, 0)}), (1.0, {})],
            "arm_R": [(0.0, {}), (0.12, {"r": (1.6, 0, 0.4)}), (0.3, {"r": (0.7, 0, 0.2)}), (0.6, {"r": (0.7, 0, 0.2)}),
                      (0.85, {"r": (0.3, 0, 0)}), (1.0, {})],
            "head": [(0.0, {}), (0.3, {"r": (0.5, 0, 0)}), (0.6, {"r": (0.4, 0, 0.2)}), (1.0, {})],
        }, False),
        ("blast", 2.5, {
            "chest": [(0.0, {}), (0.08, {"r": (0.6, 0, 0)}), (0.35, {"r": (-0.25, 0, 0)}), (0.6, {"r": (-0.2, 0, 0.1)}),
                      (1.0, {})],
            "head": [(0.0, {}), (0.08, {"r": (0.7, 0, 0)}), (0.35, {"r": (-0.4, 0, 0)}), (0.6, {"r": (-0.3, 0, -0.2)}),
                     (1.0, {})],
            "arm_L": [(0.0, {}), (0.08, {"r": (2.6, 0, -1.1)}), (0.35, {"r": (2.0, 0, 0.9)}), (0.7, {"r": (1.9, 0, 0.7)}),
                      (1.0, {})],
            "arm_R": [(0.0, {}), (0.08, {"r": (2.6, 0, 1.1)}), (0.35, {"r": (2.0, 0, -0.9)}), (0.7, {"r": (1.9, 0, -0.7)}),
                      (1.0, {})],
            "hips": [(0.0, {}), (0.08, {"r": (0.2, 0, 0)}), (0.35, {"r": (-0.15, 0, 0)}), (1.0, {})],
            "root": [(0.0, {}), (0.35, {"l": (0, -0.06 * k, 0)}), (1.0, {})],
            "leg_L": [(0.0, {}), (0.08, {"r": (0.5, 0, 0)}), (0.35, {"r": (-0.2, 0, 0)}), (1.0, {})],
            "leg_R": [(0.0, {}), (0.08, {"r": (0.3, 0, 0)}), (0.35, {"r": (0.4, 0, 0)}), (1.0, {})],
        }, False),
        ("death", 8.0, {
            "chest": [(0.0, {}), (0.3, {"r": (-0.5, 0, 0.2)}), (1.0, {"r": (-0.9, 0, 0.3)})],
            "head": [(0.0, {}), (0.3, {"r": (-0.6, 0, 0)}), (1.0, {"r": (-0.8, 0, 0.4)})],
            "arm_L": [(0.0, {}), (0.3, {"r": (2.8, 0, -0.8)}), (1.0, {"r": (2.4, 0, -1.2)})],
            "arm_R": [(0.0, {}), (0.3, {"r": (2.8, 0, 0.8)}), (1.0, {"r": (2.0, 0, 1.4)})],
            "root": [(0.0, {}), (1.0, {"l": (0, -0.2 * k, 0)})],
        }, False),
    ]


def core_model():
    rg = d.rig("core_rig", [("root", None, (0, 0, 0)), ("crystal", "root", (0, 0, 2.2)),
                            ("ring", "root", (0, 0, 2.2))])
    p = d.Parts()
    p.ball("crystal", "basoalth_glow", (0, 0, 2.2), 1.3, (1, 1, 1.5), 1)
    p.ball("crystal", "core_inner", (0, 0, 2.2), 0.7, (1, 1, 1.4), 2)
    for i, (rx, ry) in enumerate(((0, 0), (1.2, 0), (0, 1.2))):
        for j in range(16):
            a = j / 16 * 2 * math.pi
            x, y = math.cos(a) * 2.0, math.sin(a) * 2.0
            v = (x, y * math.cos(rx), y * math.sin(rx)) if i == 1 else ((x * math.cos(ry), y, x * math.sin(ry)) if i == 2 else (x, y, 0))
            p.box("ring", "legion_trim", (v[0], v[1], 2.2 + v[2]), (0.28, 0.28, 0.12), (0, 0, a))
    mesh = p.build("core_mesh")
    d.skin(mesh, rg)
    tau = 2 * math.pi
    d.clip(rg, "idle", 6.0, {
        "crystal": [(0.0, {}), (0.25, {"r": (0, tau / 4, 0)}), (0.5, {"r": (0, tau / 2, 0)}),
                    (0.75, {"r": (0, 3 * tau / 4, 0)}), (1.0, {"r": (0, tau, 0)})],
        "ring": [(0.0, {}), (0.25, {"r": (0.3, -tau / 4, 0)}), (0.5, {"r": (0, -tau / 2, 0)}),
                 (0.75, {"r": (-0.3, -3 * tau / 4, 0)}), (1.0, {"r": (0, -tau, 0)})],
    })
    d.export([rg, mesh], os.path.join(OUT, "core.gltf"), True)
    d.put_aside([rg], 40.0)


def pylon(dead):
    name = "pylon_dead" if dead else "pylon"
    rg = d.rig(f"{name}_rig", [("root", None, (0, 0, 0)), ("head", "root", (0, 0, 4.4))])
    p = d.Parts()
    p.box("root", "legion_armour", (0, 0, 0.3), (2.0, 2.0, 0.6))
    top = 2.4 if dead else 4.2
    p.box("root", "legion_armour", (0, 0, top / 2 + 0.3), (1.1, 1.1, top))
    for i in range(4 if not dead else 2):
        p.cyl("root", "core_inner" if not dead else "burnt", (0, 0, 1.2 + i * 0.8), 0.8, 0.18, seg=10)
    for sx in (-1, 1):
        for sy in (-1, 1):
            p.limb("root", "legion_trim", (sx * 0.9, sy * 0.9, 0.6), (sx * 0.5, sy * 0.5, top), 0.16)
    if dead:
        p.box("root", "burnt", (0.1, -0.1, top + 0.4), (0.9, 0.7, 0.5), (0.3, 0.4, 0.2))
        p.box("root", "burnt", (1.4, 0.6, 0.2), (0.8, 0.5, 0.4), (0.2, 0.6, 0.9))
    else:
        p.ball("head", "basoalth_glow", (0, 0, 5.0), 0.55, (1, 1, 1), 2)
        for i in range(4):
            a = i * math.pi / 2
            p.spike("head", "legion_bone", (math.cos(a) * 0.5, math.sin(a) * 0.5, 4.5),
                    (math.cos(a) * 0.75, math.sin(a) * 0.75, 5.5), 0.08)
    mesh = p.build(f"{name}_mesh")
    d.skin(mesh, rg)
    if dead:
        d.clip(rg, "idle", 1.0, {}, loop=True)
    else:
        d.clip(rg, "idle", 1.4, {"head": [(0.0, {}), (0.5, {"l": (0, 0.15, 0), "s": 1.1})]}, loop=True)
    d.export([rg, mesh], os.path.join(OUT, f"{name}.gltf"), True)
    d.put_aside([rg], 50.0 + (6.0 if dead else 0.0))


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = d.FPS
    bpy.context.preferences.edit.keyframe_new_interpolation_type = "LINEAR"
    os.makedirs(OUT, exist_ok=True)
    d.mats()
    for mname, rgb, kw in [("skin", (0.72, 0.54, 0.42), {}), ("legion_violet", (0.36, 0.10, 0.50), {}),
                           ("legion_cloth", (0.09, 0.08, 0.11), {}),
                           ("legion_armour", (0.08, 0.07, 0.10), {"metal": 0.6, "rough": 0.4}),
                           ("legion_trim", (0.30, 0.12, 0.40), {"metal": 0.7, "rough": 0.35}),
                           ("legion_bone", (0.80, 0.76, 0.66), {}), ("legion_skin", (0.40, 0.36, 0.42), {}),
                           ("legion_cape", (0.30, 0.04, 0.10), {}),
                           ("basoalth_glow", (0.78, 0.30, 1.00), {"glow": True}),
                           ("core_inner", (1.00, 0.80, 1.00), {"glow": True}),
                           ("burnt", (0.06, 0.05, 0.05), {})]:
        d.M[mname] = d.material(mname, rgb, **kw)
    for mname, rgb, kw in [("rag", (0.24, 0.25, 0.18), {}), ("rot_skin", (0.40, 0.46, 0.36), {}),
                           ("rot_dark", (0.16, 0.14, 0.12), {}), ("zombie_eye", (0.55, 1.00, 0.35), {"glow": True}),
                           ("demon_hide", (0.55, 0.10, 0.07), {}), ("demon_dark", (0.20, 0.05, 0.05), {}),
                           ("demon_horn", (0.86, 0.80, 0.66), {}), ("demon_eye", (1.00, 0.85, 0.15), {"glow": True}),
                           ("cyber_plate", (0.46, 0.48, 0.52), {"metal": 0.8, "rough": 0.3}),
                           ("cyber_under", (0.10, 0.10, 0.12), {"metal": 0.4}),
                           ("cyber_glow", (0.25, 0.95, 1.00), {"glow": True}),
                           ("overall", (0.18, 0.24, 0.40), {}), ("hivis", (1.00, 0.45, 0.05), {}),
                           ("stripe", (0.80, 0.82, 0.84), {"metal": 0.5}), ("hardhat", (1.00, 0.80, 0.08), {}),
                           ("leather", (0.36, 0.22, 0.10), {})]:
        d.M[mname] = d.material(mname, rgb, **kw)
    if "--giant" in sys.argv:
        basoalth("basoalth_giant", 13.0, giant_clips(13.0), 20.0)
        print("assault: basoalth_giant written to", os.path.normpath(OUT))
        sys.exit(0)
    fm.OUT = OUT
    fm.soldier("zombie", None, -6.0, zombie_body)
    fm.soldier("demon", None, -3.0, demon_body)
    fm.soldier("cyborg", None, 0.0, cyborg_body)
    fm.soldier("builder", None, 3.0, builder_body, builder_clips)
    wrench()
    basoalth("basoalth", 1.0, [IDLE, ABSORB], 10.0)
    basoalth("basoalth_giant", 13.0, giant_clips(13.0), 20.0)
    core_model()
    pylon(False)
    pylon(True)
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("assault models written to", os.path.normpath(OUT))
