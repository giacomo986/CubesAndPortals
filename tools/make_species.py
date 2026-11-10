# Builds the people a player may be besides the robot -- a human, a mole-man
# and a frog-man -- on the robot's own rig, with the robot's clips, and the
# styles each one may pick.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_species.py"
#
# Each one is the robot with its body taken off and a new one put on the same
# bones: mods/robot/robot.gltf is imported, every face not bound to a tool_*
# bone deleted (the tools stay, so a held pick still shows), the species' parts
# added, each bound whole to one bone the way tools/make_robot_extras.py binds
# an extra, and the lot exported with every clip as mods/robot/robot_<species>.gltf.
# The same parts on the arm bones, moved onto robot_arms.gltf's rig, are
# robot_<species>_arms.gltf, the arms seen in first person; and each style
# (hair, a miner's lamp, a crown...) is robot_<species>_<style>.gltf, drawn
# worn like an extra. The client picks them by name off the player's livery
# (net::kLiverySpeciesShift, kLiveryStyleShift).
#
# 1.8 tall like the robot, the eye at 1.5, so the camera, reach and the
# robot's extras sit where they did. The materials tint_1..3 are each one's
# primary, secondary and accent, dyed by the player; their colours here are
# what an undyed one looks like. Z up, facing +Y, metres, at rest.
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gltf_add_clips   # noqa: E402
from make_robot_extras import MATS, P, extras, material, shape   # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "robot")

# The arm bones in robot_arms.gltf are the body's moved by this (the eye at the
# origin): upper_arm_L's head is (-0.265, 0, 1.234) on the body and
# (-0.265, -0.159, -0.398) there.
TO_ARMS = Vector((0.0, -0.159, -1.632))
ARM_BONES = {"upper_arm_L", "forearm_L", "hand_L", "upper_arm_R", "forearm_R", "hand_R"}

# Colour, metallic, roughness; and a glow, for a lamp.
FIXED = {
    "Skin": ((0.87, 0.62, 0.48), 0.0, 0.6),
    "Hair": ((0.24, 0.14, 0.07), 0.0, 0.8),
    "Lip": ((0.62, 0.30, 0.28), 0.0, 0.6),
    "Eye_White": ((0.95, 0.95, 0.93), 0.0, 0.3),
    "Eye_Dark": ((0.03, 0.03, 0.04), 0.0, 0.2),
    "Eye_Gold": ((0.95, 0.75, 0.15), 0.0, 0.25),
    "Sole": ((0.12, 0.10, 0.09), 0.0, 0.9),
    "Metal": ((0.75, 0.75, 0.78), 1.0, 0.35),
    "Gold": ((1.00, 0.76, 0.20), 1.0, 0.3),
    "Fur": ((0.22, 0.19, 0.18), 0.0, 0.95),
    "Pink": ((0.93, 0.56, 0.60), 0.0, 0.6),
    "Claw": ((0.93, 0.88, 0.74), 0.0, 0.4),
    "Mouth": ((0.45, 0.10, 0.14), 0.0, 0.6),
    "Spot": ((0.10, 0.28, 0.08), 0.0, 0.6),
    "Helmet": ((0.95, 0.72, 0.08), 0.0, 0.4),
    "Glass": ((0.35, 0.65, 0.80), 0.3, 0.1),
    "Leaf": ((0.20, 0.55, 0.20), 0.0, 0.7),
    "Petal": ((0.98, 0.70, 0.85), 0.0, 0.6),
    "Lamp": ((1.00, 0.95, 0.70), 0.0, 0.2, 4.0),
}


def mirrored(parts):
    """Each part named for a _L bone, and its twin on the right."""
    out = []
    for p in parts:
        out.append(p)
        if p["bone"].endswith("_L"):
            q = dict(p, bone=p["bone"][:-2] + "_R", c=(-p["c"][0], *p["c"][1:]))
            q["r"] = (p["r"][0], -p["r"][1], -p["r"][2])
            out.append(q)
    return out


# ---------------------------------------------------------------- the human
HUMAN_TINTS = {"tint_1": (0.20, 0.42, 0.75), "tint_2": (0.22, 0.24, 0.32), "tint_3": (0.45, 0.25, 0.12)}
HUMAN = mirrored([
    # A skull, a jaw narrowing to the chin, ears, a nose, a smile.
    P("ball", "head", "Skin", (0, -0.005, 1.515), (0.150, 0.160, 0.150)),
    P("ball", "head", "Skin", (0, 0.035, 1.435), (0.118, 0.118, 0.100)),
    P("ball", "head", "Skin", (0, 0.085, 1.395), (0.060, 0.055, 0.045)),
    P("ball", "head", "Skin", (-0.144, -0.01, 1.478), (0.017, 0.028, 0.038), (0, 0, -15)),
    P("ball", "head", "Skin", (0.144, -0.01, 1.478), (0.017, 0.028, 0.038), (0, 0, 15)),
    P("ball", "head", "Skin", (0, 0.145, 1.468), (0.017, 0.022, 0.024)),
    P("ball", "head", "Eye_White", (-0.056, 0.125, 1.505), (0.034, 0.018, 0.030)),
    P("ball", "head", "Eye_White", (0.056, 0.125, 1.505), (0.034, 0.018, 0.030)),
    P("ball", "head", "Eye_Dark", (-0.056, 0.140, 1.503), (0.017, 0.007, 0.019)),
    P("ball", "head", "Eye_Dark", (0.056, 0.140, 1.503), (0.017, 0.007, 0.019)),
    P("ball", "head", "Hair", (-0.058, 0.138, 1.550), (0.034, 0.010, 0.007), (0, -2, 0)),
    P("ball", "head", "Hair", (0.058, 0.138, 1.550), (0.034, 0.010, 0.007), (0, 2, 0)),
    P("torus", "head", "Lip", (0, 0.150, 1.445), (0.042, 0.030, 0.034), (90, 0, 0), R=1.0, t=0.2, arc=(230, 310)),
    P("cone", "neck", "Skin", (0, -0.005, 1.33), r1=0.062, r2=0.052, d=0.12),
    # A shirt (primary) with a collar, worn out over trousers (secondary);
    # shoes (accent).
    P("ball", "chest", "tint_1", (0, 0.0, 1.205), (0.215, 0.106, 0.105)),
    P("cone", "chest", "tint_1", (0, 0.0, 1.10), (1, 0.58, 1), r1=0.165, r2=0.198, d=0.22),
    P("cyl", "chest", "tint_1", (0, 0.0, 1.238), (0.062, 0.078, 0.42), (0, 90, 0)),
    P("torus", "chest", "tint_1", (0, 0.0, 1.300), (0.070, 0.066, 0.070), R=1.0, t=0.30),
    P("cone", "spine", "tint_1", (0, 0.0, 0.95), (1, 0.60, 1), r1=0.172, r2=0.165, d=0.22),
    P("cone", "hips", "tint_1", (0, 0.0, 0.83), (1, 0.62, 1), r1=0.196, r2=0.172, d=0.09),
    P("ball", "hips", "tint_2", (0, 0.0, 0.775), (0.168, 0.102, 0.095)),
    # A round shoulder and a short sleeve to above the elbow, a bare arm
    # tapering to the wrist.
    P("ball", "upper_arm_L", "tint_1", (-0.258, 0, 1.232), (0.066, 0.068, 0.066)),
    P("cone", "upper_arm_L", "tint_1", (-0.265, 0, 1.13), r1=0.063, r2=0.064, d=0.20),
    P("cone", "upper_arm_L", "Skin", (-0.265, 0, 1.00), r1=0.047, r2=0.047, d=0.10),
    P("ball", "forearm_L", "Skin", (-0.265, 0, 0.942), (0.050, 0.052, 0.052)),
    P("cone", "forearm_L", "Skin", (-0.265, 0.004, 0.80), r1=0.036, r2=0.050, d=0.27),
    # A hand: a palm, four fingers curled as one, a thumb forward.
    P("ball", "hand_L", "Skin", (-0.265, 0.008, 0.625), (0.030, 0.046, 0.052)),
    P("ball", "hand_L", "Skin", (-0.262, 0.012, 0.572), (0.026, 0.040, 0.036)),
    P("ball", "hand_L", "Skin", (-0.250, 0.056, 0.610), (0.016, 0.018, 0.034), (25, 0, 0)),
    # A ball at the hip, so a raised leg opens no gap; a knee; legs that taper.
    P("ball", "thigh_L", "tint_2", (-0.110, 0, 0.735), (0.092, 0.098, 0.090)),
    P("cone", "thigh_L", "tint_2", (-0.113, 0, 0.575), r1=0.078, r2=0.094, d=0.34),
    P("ball", "shin_L", "tint_2", (-0.116, 0.004, 0.405), (0.074, 0.076, 0.070)),
    P("cone", "shin_L", "tint_2", (-0.117, 0, 0.255), r1=0.064, r2=0.074, d=0.30),
    # A shoe: a heel, a rounded toe, a sole under both.
    P("ball", "foot_L", "tint_3", (-0.118, 0.0, 0.075), (0.062, 0.072, 0.062)),
    P("ball", "foot_L", "tint_3", (-0.118, 0.085, 0.058), (0.064, 0.125, 0.050)),
    P("cyl", "foot_L", "Sole", (-0.118, 0.072, 0.012), (0.066, 0.140, 0.024)),
])

HUMAN_STYLES = {
    # Short hair: a shell over the skull, the face and the ears cut out of it
    # (cut=True: taken away, not added), a quiff over the brow.
    "short": [
        P("ball", "head", "Hair", (0, -0.018, 1.528), (0.162, 0.170, 0.160)),
        P("ball", "head", "Hair", (0, -0.090, 1.430), (0.125, 0.085, 0.085)),
        P("ball", "head", "Hair", (0.02, 0.090, 1.635), (0.110, 0.072, 0.036), (12, 0, 0)),
        P("box", "head", "Hair", (0, 0.175, 1.42), (0.34, 0.24, 0.32), (-10, 0, 0), cut=True),
        P("box", "head", "Hair", (0, 0.0, 1.30), (0.40, 0.40, 0.12), cut=True),
        P("ball", "head", "Hair", (-0.175, 0.0, 1.47), (0.035, 0.05, 0.055), cut=True),
        P("ball", "head", "Hair", (0.175, 0.0, 1.47), (0.035, 0.05, 0.055), cut=True),
    ],
    # Long hair: the same shell, falling to the shoulders behind and beside
    # the face, with a fringe.
    "long": [
        P("ball", "head", "Hair", (0, -0.018, 1.528), (0.165, 0.173, 0.162)),
        P("ball", "head", "Hair", (0, -0.095, 1.39), (0.16, 0.095, 0.19)),
        P("ball", "head", "Hair", (-0.135, 0.015, 1.42), (0.045, 0.10, 0.15)),
        P("ball", "head", "Hair", (0.135, 0.015, 1.42), (0.045, 0.10, 0.15)),
        P("ball", "head", "Hair", (0, 0.092, 1.612), (0.118, 0.068, 0.028), (16, 0, 0)),
        P("box", "head", "Hair", (0, 0.195, 1.41), (0.235, 0.22, 0.30), (-8, 0, 0), cut=True),
        P("box", "head", "Hair", (0, 0.0, 1.20), (0.45, 0.45, 0.12), cut=True),
    ],
    # A full beard and a moustache, and the head shaved.
    "beard": [
        P("ball", "head", "Hair", (0, 0.060, 1.405), (0.125, 0.115, 0.080)),
        P("ball", "head", "Hair", (0, 0.156, 1.449), (0.050, 0.018, 0.011)),
        P("ball", "head", "Hair", (-0.126, 0.05, 1.455), (0.022, 0.038, 0.070)),
        P("ball", "head", "Hair", (0.126, 0.05, 1.455), (0.022, 0.038, 0.070)),
    ],
}

# ------------------------------------------------------------- the mole-man
MOLE_TINTS = {"tint_1": (0.25, 0.38, 0.62), "tint_2": (0.80, 0.68, 0.45), "tint_3": (0.78, 0.18, 0.12)}
MOLE = mirrored([
    # Velvet fur, round cheeks, a long snout with a pink nose and two teeth
    # under it, eyes like pinpricks. No whiskers.
    P("ball", "head", "Fur", (0, 0.01, 1.48), (0.19, 0.19, 0.165)),
    P("ball", "head", "Fur", (-0.075, 0.11, 1.43), (0.075, 0.075, 0.065)),
    P("ball", "head", "Fur", (0.075, 0.11, 1.43), (0.075, 0.075, 0.065)),
    P("cone", "head", "Fur", (0, 0.215, 1.44), (1, 1, 1), (-90, 0, 0), r1=0.105, r2=0.045, d=0.17),
    P("ball", "head", "Pink", (0, 0.305, 1.443), (0.048, 0.034, 0.042)),
    P("box", "head", "Claw", (-0.012, 0.25, 1.397), (0.018, 0.01, 0.028)),
    P("box", "head", "Claw", (0.012, 0.25, 1.397), (0.018, 0.01, 0.028)),
    P("ball", "head", "Eye_Dark", (-0.085, 0.165, 1.515), (0.020, 0.014, 0.020)),
    P("ball", "head", "Eye_Dark", (0.085, 0.165, 1.515), (0.020, 0.014, 0.020)),
    P("ball", "head", "Fur", (-0.15, -0.03, 1.57), (0.035, 0.02, 0.03)),
    P("ball", "head", "Fur", (0.15, -0.03, 1.57), (0.035, 0.02, 0.03)),
    P("cyl", "neck", "Fur", (0, 0, 1.32), (0.085, 0.085, 0.1)),
    # A round body in dungarees (primary) over a shirt (secondary), buttons
    # and a pocket (accent).
    P("ball", "chest", "tint_2", (0, 0, 1.15), (0.215, 0.165, 0.18)),
    P("cyl", "chest", "tint_2", (0, 0.0, 1.235), (0.068, 0.085, 0.40), (0, 90, 0)),
    # The bib swells the chest; the straps go over the shoulders; a pocket.
    P("ball", "chest", "tint_1", (0, 0.085, 1.10), (0.14, 0.10, 0.14)),
    P("torus", "chest", "tint_1", (-0.085, 0.0, 1.17), (0.14, 0.16, 0.35), (0, 90, 0), R=1.0, t=0.1, arc=(80, 280)),
    P("torus", "chest", "tint_1", (0.085, 0.0, 1.17), (0.14, 0.16, 0.35), (0, 90, 0), R=1.0, t=0.1, arc=(80, 280)),
    P("box", "spine", "tint_3", (0, 0.198, 0.99), (0.11, 0.03, 0.08)),
    P("ball", "chest", "tint_3", (-0.085, 0.176, 1.185), (0.018, 0.012, 0.018)),
    P("ball", "chest", "tint_3", (0.085, 0.176, 1.185), (0.018, 0.012, 0.018)),
    P("ball", "spine", "tint_1", (0, 0.01, 0.97), (0.235, 0.19, 0.19)),
    P("ball", "hips", "tint_1", (0, 0, 0.80), (0.215, 0.165, 0.12)),
    # A shirt sleeve, a furry forearm, big digging hands: a pink paw and five
    # pale claws.
    P("ball", "upper_arm_L", "tint_2", (-0.255, 0, 1.228), (0.076, 0.078, 0.076)),
    P("cone", "upper_arm_L", "tint_2", (-0.265, 0, 1.115), r1=0.060, r2=0.072, d=0.24),
    P("ball", "forearm_L", "Fur", (-0.265, 0, 0.945), (0.058, 0.058, 0.058)),
    P("cone", "forearm_L", "Fur", (-0.265, 0, 0.81), r1=0.050, r2=0.058, d=0.27),
    P("ball", "hand_L", "Pink", (-0.265, 0.02, 0.61), (0.060, 0.095, 0.082)),
] + [P("cone", "hand_L", "Claw", (-0.265, 0.02 + 0.035 * (i - 2), 0.505), (1, 1, 1), (180, 0, 0),
       r1=0.017, r2=0.003, d=0.075) for i in range(5)] + [
    # Trouser legs with a turn-up (accent), pink feet with three claws.
    P("ball", "thigh_L", "tint_1", (-0.11, 0, 0.735), (0.100, 0.105, 0.095)),
    P("cone", "thigh_L", "tint_1", (-0.112, 0, 0.575), r1=0.084, r2=0.098, d=0.34),
    P("ball", "shin_L", "tint_1", (-0.113, 0.004, 0.405), (0.082, 0.084, 0.075)),
    P("cone", "shin_L", "tint_1", (-0.113, 0, 0.29), r1=0.076, r2=0.082, d=0.24),
    P("cyl", "shin_L", "tint_3", (-0.113, 0, 0.175), (0.086, 0.086, 0.05)),
    P("cyl", "shin_L", "Fur", (-0.113, 0, 0.12), (0.058, 0.058, 0.08)),
    P("ball", "foot_L", "Pink", (-0.115, 0.07, 0.05), (0.082, 0.15, 0.05)),
] + [P("cone", "foot_L", "Claw", (-0.115 + 0.04 * (i - 1), 0.235, 0.03), (1, 1, 1), (-90, 0, 0),
       r1=0.017, r2=0.003, d=0.075) for i in range(3)])

MOLE_STYLES = {
    # A miner's helmet with its lamp lit.
    "lamp": [
        P("dome", "head", "Helmet", (0, 0.0, 1.55), (0.205, 0.205, 0.15)),
        P("cyl", "head", "Helmet", (0, 0.0, 1.552), (0.225, 0.225, 0.016)),
        P("cyl", "head", "Metal", (0, 0.19, 1.62), (0.04, 0.04, 0.04), (-80, 0, 0)),
        P("ball", "head", "Lamp", (0, 0.212, 1.624), (0.032, 0.012, 0.032)),
    ],
    # Digging goggles, pushed up nowhere: on the eyes, on a strap.
    "goggles": [
        P("torus", "head", "Sole", (0, 0.01, 1.515), (0.192, 0.192, 0.192), R=1.0, t=0.08),
        P("cyl", "head", "Metal", (-0.075, 0.175, 1.515), (0.04, 0.04, 0.03), (-90, 0, 0)),
        P("cyl", "head", "Metal", (0.075, 0.175, 1.515), (0.04, 0.04, 0.03), (-90, 0, 0)),
        P("cyl", "head", "Glass", (-0.075, 0.19, 1.515), (0.032, 0.032, 0.006), (-90, 0, 0)),
        P("cyl", "head", "Glass", (0.075, 0.19, 1.515), (0.032, 0.032, 0.006), (-90, 0, 0)),
    ],
    # Little round spectacles on the snout.
    "glasses": [
        P("torus", "head", "Gold", (-0.05, 0.215, 1.5), (0.035, 0.035, 0.035), (-90, 0, 0), R=1.0, t=0.15),
        P("torus", "head", "Gold", (0.05, 0.215, 1.5), (0.035, 0.035, 0.035), (-90, 0, 0), R=1.0, t=0.15),
        P("cyl", "head", "Glass", (-0.05, 0.215, 1.5), (0.032, 0.032, 0.003), (-90, 0, 0)),
        P("cyl", "head", "Glass", (0.05, 0.215, 1.5), (0.032, 0.032, 0.003), (-90, 0, 0)),
        P("box", "head", "Gold", (0, 0.215, 1.505), (0.03, 0.006, 0.006)),
    ],
}

# ------------------------------------------------------------- the frog-man
FROG_TINTS = {"tint_1": (0.30, 0.62, 0.20), "tint_2": (0.90, 0.88, 0.58), "tint_3": (0.85, 0.35, 0.12)}
FROG = mirrored([
    # A wide flat head, eyes bulging on top, a mouth from ear to ear.
    P("ball", "head", "tint_1", (0, 0.03, 1.45), (0.215, 0.20, 0.13)),
    P("ball", "head", "tint_2", (0, 0.06, 1.39), (0.175, 0.15, 0.07)),
    P("torus", "head", "Mouth", (0, 0.03, 1.425), (0.212, 0.197, 0.212), R=1.0, t=0.03, arc=(25, 155)),
    P("ball", "head", "tint_1", (-0.10, 0.08, 1.56), (0.072, 0.072, 0.072)),
    P("ball", "head", "tint_1", (0.10, 0.08, 1.56), (0.072, 0.072, 0.072)),
    P("ball", "head", "Eye_Gold", (-0.10, 0.112, 1.578), (0.054, 0.054, 0.054)),
    P("ball", "head", "Eye_Gold", (0.10, 0.112, 1.578), (0.054, 0.054, 0.054)),
    P("ball", "head", "Eye_Dark", (-0.10, 0.160, 1.580), (0.034, 0.008, 0.015)),
    P("ball", "head", "Eye_Dark", (0.10, 0.160, 1.580), (0.034, 0.008, 0.015)),
    P("ball", "head", "Eye_Dark", (-0.03, 0.218, 1.49), (0.009, 0.006, 0.006)),
    P("ball", "head", "Eye_Dark", (0.03, 0.218, 1.49), (0.009, 0.006, 0.006)),
    P("cyl", "neck", "tint_1", (0, 0, 1.33), (0.095, 0.095, 0.08)),
    # A pear of a body, its belly pale (secondary), in swimming
    # shorts (accent).
    P("ball", "chest", "tint_1", (0, 0, 1.17), (0.20, 0.15, 0.165)),
    P("cyl", "chest", "tint_1", (0, 0.0, 1.235), (0.055, 0.07, 0.40), (0, 90, 0)),
    P("ball", "spine", "tint_1", (0, 0.015, 1.00), (0.225, 0.18, 0.20)),
    P("ball", "spine", "tint_2", (0, 0.105, 1.02), (0.135, 0.115, 0.165)),
    P("ball", "hips", "tint_3", (0, 0, 0.80), (0.215, 0.15, 0.115)),
    # Long thin arms, three long fingers ending in round pads.
    P("ball", "upper_arm_L", "tint_1", (-0.255, 0, 1.23), (0.062, 0.064, 0.062)),
    P("cone", "upper_arm_L", "tint_1", (-0.265, 0, 1.10), r1=0.046, r2=0.056, d=0.27),
    P("ball", "forearm_L", "tint_1", (-0.265, 0, 0.945), (0.047, 0.047, 0.047)),
    P("cone", "forearm_L", "tint_1", (-0.265, 0, 0.80), r1=0.036, r2=0.046, d=0.29),
    P("ball", "hand_L", "tint_1", (-0.265, 0.01, 0.615), (0.036, 0.058, 0.055)),
] + [x for i in range(3) for x in (
    P("cyl", "hand_L", "tint_1", (-0.265, 0.01 + 0.042 * (i - 1), 0.535), (0.013, 0.013, 0.09), (-14 * (i - 1), 0, 0)),
    P("ball", "hand_L", "tint_2", (-0.265, 0.01 + 0.054 * (i - 1), 0.488), (0.021, 0.021, 0.019)))] + [
    # Strong thighs in the shorts' legs, thin shins, a webbed foot with three
    # toes and their pads.
    P("ball", "thigh_L", "tint_3", (-0.118, 0, 0.735), (0.100, 0.108, 0.09)),
    P("cone", "thigh_L", "tint_3", (-0.118, 0, 0.66), r1=0.100, r2=0.104, d=0.15),
    P("cone", "thigh_L", "tint_1", (-0.12, 0, 0.555), r1=0.068, r2=0.090, d=0.30),
    P("ball", "shin_L", "tint_1", (-0.12, 0.004, 0.405), (0.070, 0.072, 0.066)),
    P("cone", "shin_L", "tint_1", (-0.12, 0, 0.255), r1=0.050, r2=0.068, d=0.30),
    P("ball", "foot_L", "tint_1", (-0.12, 0.0, 0.07), (0.058, 0.07, 0.06)),
    P("ball", "foot_L", "tint_1", (-0.12, 0.10, 0.032), (0.105, 0.155, 0.03)),
] + [P("ball", "foot_L", "tint_2", (-0.12 + 0.07 * (i - 1), 0.235 - 0.025 * abs(i - 1), 0.03), (0.028, 0.028, 0.024))
     for i in range(3)])

FROG_STYLES = {
    # Dark spots on the head and back.
    "spots": [P("ball", "head", "Spot", (x, y, z), (0.03, 0.03, 0.012), (rx, ry, 0))
              for x, y, z, rx, ry in ((-0.06, -0.02, 1.575, -10, -15), (0.07, -0.06, 1.565, 10, 20),
                                      (0.0, -0.13, 1.53, 40, 0), (-0.14, -0.06, 1.51, 20, -50),
                                      (0.15, 0.02, 1.52, 0, 55))] +
             [P("ball", "chest", "Spot", (x, -0.162, z), (0.04, 0.012, 0.035))
              for x, z in ((-0.08, 1.20), (0.07, 1.12), (-0.02, 1.04))],
    # A frog prince's crown.
    "crown": [P("cyl", "head", "Gold", (0, -0.07, 1.585), (0.085, 0.085, 0.05))] +
             [P("cone", "head", "Gold", (0.075 * math.cos(a * 1.2566 + 1.5708),
                                         -0.07 + 0.075 * math.sin(a * 1.2566 + 1.5708), 1.64), (1, 1, 1),
                r1=0.024, r2=0.002, d=0.07) for a in range(5)],
    # A lily pad worn as a hat, a flower on it.
    "lily": [
        P("cyl", "head", "Leaf", (0, 0.0, 1.585), (0.2, 0.2, 0.012), (4, 0, 0)),
        P("cone", "head", "Petal", (0, -0.03, 1.615), (1, 1, 1), r1=0.05, r2=0.02, d=0.05, seg=6),
        P("ball", "head", "Helmet", (0, -0.03, 1.63), (0.018, 0.018, 0.01)),
    ],
}

# Where each species wears the robot's extras (tools/make_robot_extras.py,
# which says what each number is), exported as robot_<species>_<extra>.gltf;
# the client wears those on a species and the robot's own on a robot. Measured
# against the species' head, neck and chest, hair and all.
WEARS = {
    "human": dict(
        cap=dict(c=(0.0, 0.004), z=1.585, rx=0.157, ry=0.180, h=0.125),
        hat=dict(c=(0.0, 0.02), z=1.618, r=0.145, h=0.24),
        ears=dict(x=0.158, y=-0.01, z=1.478, top=1.705, cup=0.72, mic=0.15),
        neck=dict(y=0.0, z=1.322, rx=0.078, ry=0.074, tube=0.034, lean=0),
        bow=dict(y=0.098, z=1.292, s=0.8),
        medal=dict(x=-0.10, y=0.112, z=1.235, lean=0),
    ),
    "mole": dict(
        cap=dict(c=(0.0, 0.01), z=1.572, rx=0.165, ry=0.165, h=0.09),
        hat=dict(c=(0.0, 0.005), z=1.612, r=0.118, h=0.25),
        ears=dict(x=0.19, y=0.0, z=1.49, top=1.67, cup=0.85, mic=0.17),
        neck=dict(y=0.0, z=1.312, rx=0.112, ry=0.106, tube=0.038, lean=11),
        bow=dict(y=0.118, z=1.297, s=0.9),
        medal=dict(x=-0.085, y=0.19, z=1.235, lean=0),
    ),
    "frog": dict(
        # Hats sit behind the eyes; the cap backwards, its peak clear of them.
        cap=dict(c=(0.0, -0.075), z=1.553, rx=0.105, ry=0.095, h=0.075, back=True),
        hat=dict(c=(0.0, -0.085), z=1.565, r=0.07, h=0.17),
        ears=dict(x=0.213, y=-0.045, z=1.45, top=1.60, cup=0.8, mic=0.2),
        neck=dict(y=0.0, z=1.316, rx=0.122, ry=0.116, tube=0.036, lean=9),
        bow=dict(y=0.116, z=1.302, s=0.9),
        medal=dict(x=-0.09, y=0.135, z=1.245, lean=18),
    ),
}


SPECIES = {
    "human": (HUMAN_TINTS, HUMAN, HUMAN_STYLES),
    "mole": (MOLE_TINTS, MOLE, MOLE_STYLES),
    "frog": (FROG_TINTS, FROG, FROG_STYLES),
}


def materials(tints):
    for k, (rgb, metal, rough, *glow) in FIXED.items():
        m = material(k, rgb, metal, rough)
        if glow:
            p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
            p.inputs["Emission Color"].default_value = (*rgb, 1)
            p.inputs["Emission Strength"].default_value = glow[0]
    for k, rgb in tints.items():
        material(k, rgb, 0.0, 0.6)


# Sculpting: a body is not drawn as the primitives it is described by but as
# one surface per piece of it (a head, a torso, an arm, a leg), the
# primitives fused by a voxel remesh, smoothed and thinned -- the way a figure
# is blocked out and smoothed in sculpt mode. Each face takes the material of
# the primitive nearest it and each vertex that primitive's bone, and the
# weights are blurred so an elbow or a knee bends rather than breaks. What is
# small or must stay sharp (eyes, a mouth, claws, metal) is kept as made.
PIECES = {"head": "head", "neck": "head", "chest": "torso", "spine": "torso", "hips": "torso"}
VOXEL, SMOOTH, KEEP = 0.009, 18, 0.09   # metres; smoothing passes; faces kept after thinning
SHARP = {"Eye_White", "Eye_Dark", "Eye_Gold", "Lip", "Mouth", "Metal", "Claw", "Gold", "Glass", "Sole",
         "Lamp", "Petal"}


def piece_of(bone):
    return PIECES.get(bone, bone.replace("upper_arm", "arm").replace("forearm", "arm").replace("hand", "arm")
                      .replace("thigh", "leg").replace("shin", "leg").replace("foot", "leg"))


def least(part):
    """The part's thinnest extent, metres."""
    k, sc = part["k"], part["s"]
    if k in ("ball", "dome"):
        return 2 * min(sc)
    if k == "cyl":
        return min(2 * sc[0], 2 * sc[1], sc[2])
    if k == "cone":
        return min(2 * part["r2"] * min(sc[:2]) + 0.02, part["d"] * sc[2])
    if k == "torus":
        return 2 * part["t"] * part["R"] * min(sc)
    return min(sc)


def distance(part, co):
    """Roughly how far `co` (world) is outside the part; negative inside."""
    turn = Euler([math.radians(a) for a in part["r"]], "ZYX").to_matrix()
    q = turn.transposed() @ (co - Vector(part["c"]))
    sc, k = Vector(part["s"]), part["k"]
    if k in ("ball", "dome"):
        d = (Vector((q.x / sc.x, q.y / sc.y, q.z / sc.z)).length - 1) * min(sc)
        return max(d, -q.z) if k == "dome" else d
    if k == "torus":
        u = Vector((q.x / sc.x, q.y / sc.y, q.z / sc.z))
        return (Vector((math.hypot(u.x, u.y) - part["R"], u.z)).length - part["t"] * part["R"]) * min(sc)
    if k in ("cyl", "cone"):
        r = 1.0 if k == "cyl" else (part["r1"] + part["r2"]) / 2
        h = 0.5 if k == "cyl" else part["d"] / 2
        radial = (math.hypot(q.x / sc.x, q.y / sc.y) - r) * min(sc.x, sc.y)
        along = abs(q.z) - h * sc.z
    else:   # a box
        e = Vector((abs(q.x) - sc.x / 2, abs(q.y) - sc.y / 2, abs(q.z) - sc.z / 2))
        out = Vector((max(e.x, 0), max(e.y, 0), max(e.z, 0))).length
        return out + min(max(e.x, e.y, e.z), 0)
    return Vector((max(radial, 0), max(along, 0))).length + min(max(radial, along), 0)


def solid(name, parts):
    """The parts as one closed mesh object (overlapping, not yet fused)."""
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    bm = bmesh.new()
    for part in parts:
        shape(bm, part)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    return ob


def apply(ob, kind, **settings):
    mod = ob.modifiers.new(kind.lower(), kind)
    for k, v in settings.items():
        setattr(mod, k, v)
    with bpy.context.temp_override(object=ob, active_object=ob):
        bpy.ops.object.modifier_apply(modifier=mod.name)


def blurred(me, weights, passes=8):
    """Each vertex's bone weights averaged with its neighbours', `passes` times;
    then its four strongest, adding up to 1."""
    near = [[] for _ in me.vertices]
    for e in me.edges:
        a, b = e.vertices
        near[a].append(b)
        near[b].append(a)
    for _ in range(passes):
        new = []
        for i, w in enumerate(weights):
            mix = {k: 0.5 * x for k, x in w.items()}
            for j in near[i]:
                for k, x in weights[j].items():
                    mix[k] = mix.get(k, 0.0) + 0.5 * x / len(near[i])
            new.append(mix)
        weights = new
    out = []
    for w in weights:
        top = sorted(w.items(), key=lambda kv: -kv[1])[:4]
        total = sum(x for _, x in top)
        out.append({k: x / total for k, x in top if x / total > 0.01})
    return out


def sculpt(rig, name, parts, sharp=SHARP, offset=Vector()):
    """The parts, sculpted piece by piece, as mesh objects bound to `rig`."""
    parts = [dict(p, c=tuple(Vector(p["c"]) + offset)) for p in parts]
    paint = [p for p in parts if p.get("paint")]
    fine = [p for p in parts if not p.get("cut") and p not in paint and (p["m"] in sharp or least(p) < 0.07)]
    pieces = {}
    for p in parts:
        if p not in fine and p not in paint:
            pieces.setdefault(piece_of(p["bone"]), []).append(p)
    out = [build(rig, name + "_fine", fine)] if fine else []
    for ob in out:   # a uv sphere's poles are more than a pupil needs
        ob.modifiers.new("thin", "DECIMATE").ratio = 0.4
    for piece, ps in pieces.items():
        grown = [p for p in ps if not p.get("cut")]
        if not grown:
            continue
        ob = solid(f"{name}_{piece}", grown)
        ob.data.remesh_voxel_size = VOXEL
        with bpy.context.temp_override(object=ob, active_object=ob):
            bpy.ops.object.voxel_remesh()
        for c in [p for p in ps if p.get("cut")]:
            cutter = solid("cut", [c])
            apply(ob, "BOOLEAN", object=cutter, operation="DIFFERENCE", solver="EXACT")
            bpy.data.objects.remove(cutter)
        apply(ob, "SMOOTH", factor=0.6, iterations=SMOOTH)
        apply(ob, "DECIMATE", ratio=KEEP)
        me = ob.data
        used = []
        for p in grown + paint:
            if p["m"] not in used:
                used.append(p["m"])
        me.materials.clear()   # the boolean's empty slot
        for m in used:
            me.materials.append(bpy.data.materials[m])
        for f in me.polygons:
            near = min(grown, key=lambda p: distance(p, f.center))
            for p in paint:
                if p["bone"] in {q["bone"] for q in grown} and distance(p, f.center) < 0:
                    near = p
            f.material_index = used.index(near["m"])
            f.use_smooth = True
        for b in rig.data.bones:
            ob.vertex_groups.new(name=b.name)
        weights = [{min(grown, key=lambda p: distance(p, v.co))["bone"]: 1.0} for v in me.vertices]
        if len({p["bone"] for p in grown}) > 1:
            weights = blurred(me, weights)
        for i, w in enumerate(weights):
            for bone, x in w.items():
                ob.vertex_groups[bone].add([i], x, "REPLACE")
        ob.parent = rig
        ob.matrix_parent_inverse = rig.matrix_world.inverted()
        ob.modifiers.new("rig", "ARMATURE").object = rig
        out.append(ob)
    return out


def build(rig, name, parts, offset=Vector()):
    """The parts as one mesh bound to `rig`, moved by `offset` (world)."""
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for b in rig.data.bones:
        ob.vertex_groups.new(name=b.name)
    groups = {g.name: g.index for g in ob.vertex_groups}
    used = []
    bm = bmesh.new()
    dl = bm.verts.layers.deform.verify()
    for part in parts:
        verts = shape(bm, part)
        if part["m"] not in used:
            used.append(part["m"])
        for v in verts:
            v.co = v.co + offset
            v[dl][groups[part["bone"]]] = 1.0
        for f in {f for v in verts for f in v.link_faces}:
            f.material_index = used.index(part["m"])
            f.smooth = part["k"] in ("dome", "ball", "torus", "cyl", "cone") and part.get("seg", 24) > 4
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    for m in used:
        me.materials.append(bpy.data.materials[m])
    bev = ob.modifiers.new("bevel", "BEVEL")
    bev.width, bev.segments, bev.limit_method = 0.006, 2, "ANGLE"
    ob.parent = rig
    ob.matrix_parent_inverse = rig.matrix_world.inverted()
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob


def fresh(path):
    """An empty scene with that file in it: its rig and its mesh."""
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    rig.data.pose_position = "REST"
    for x in [o for o in bpy.data.objects if o.parent is None and o is not rig]:
        bpy.data.objects.remove(x)   # an add-on's start-up object
    mesh = next(o for o in bpy.data.objects if o.type == "MESH")
    return rig, mesh


def keep_tools(mesh):
    """Every face of `mesh` not bound to a tool_* bone, gone."""
    names = {g.index: g.name for g in mesh.vertex_groups}
    me = mesh.data
    bm = bmesh.new()
    bm.from_mesh(me)
    dl = bm.verts.layers.deform.verify()

    def tool(v):
        w = v[dl]
        return bool(w) and names[max(w.items(), key=lambda kv: kv[1])[0]].startswith("tool_")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not tool(v)], context="VERTS")
    bm.to_mesh(me)
    bm.free()


def export(rig, objects, path, clips=None):
    """The objects and the rig written to `path`; with the clips of the file
    `clips`, copied as they are (tools/gltf_add_clips.py). Blender is not let
    write them again: it keys every bone in every clip, and a clip that keys
    only the arms, played over the walk, would stop the legs."""
    for x in bpy.data.objects:
        x.select_set(False)
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    if len(objects) > 1:   # one mesh, as the robot is
        bpy.ops.object.join()
    rig.select_set(True)
    rig.data.pose_position = "POSE"
    bpy.ops.export_scene.gltf(
        filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
        export_animations=False, export_skins=True, export_apply=True, export_yup=True)
    if clips:
        with open(clips) as f:
            src = json.load(f)
        sys.argv = ["gltf_add_clips", path, clips] + [a["name"] for a in src["animations"]]
        gltf_add_clips.main()
        # A bone's turn at rest with the sign the clips' keys have (q and -q
        # are one turn, but a blend from one to the other goes the long way).
        rest = {n.get("name"): n["rotation"] for n in src["nodes"] if "rotation" in n}
        with open(path) as f:
            doc = json.load(f)
        for n in doc["nodes"]:
            r = rest.get(n.get("name"))
            if r and "rotation" in n and sum(a * b for a, b in zip(r, n["rotation"])) < 0:
                n["rotation"] = [-x for x in n["rotation"]]
        with open(path, "w") as f:
            json.dump(doc, f, separators=(",", ":"))
    # A server refuses a file over a megabyte (kMaxMediaBytes, media_limits.h),
    # and a player who picked this would be a robot with nothing said why.
    for f in (path, path[:-5] + ".bin"):
        assert os.path.getsize(f) < 1024 * 1024 - 4096, f"{f}: over the 1 MB a server serves"


def main():
    for species, (tints, parts, styles) in SPECIES.items():
        name = f"robot_{species}"
        # The body, with the robot's tools and every clip.
        rig, mesh = fresh(os.path.join(OUT, "robot.gltf"))
        materials(tints)
        keep_tools(mesh)
        body = sculpt(rig, name, parts)
        mesh.name = name + "_tools"
        export(rig, body + [mesh], os.path.join(OUT, name + ".gltf"), os.path.join(OUT, "robot.gltf"))
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", f"{name}.blend"))

        # The arms seen in first person, on robot_arms.gltf's rig and clips.
        rig, mesh = fresh(os.path.join(OUT, "robot_arms.gltf"))
        materials(tints)
        keep_tools(mesh)
        arms = sculpt(rig, name + "_arms", [p for p in parts if p["bone"] in ARM_BONES], offset=TO_ARMS)
        export(rig, arms + [mesh], os.path.join(OUT, name + "_arms.gltf"), os.path.join(OUT, "robot_arms.gltf"))

        # Each style, worn on the body's rig like an extra.
        for style, sparts in styles.items():
            rig, mesh = fresh(os.path.join(OUT, "robot.gltf"))
            materials(tints)
            bpy.data.objects.remove(mesh)
            obs = sculpt(rig, f"{name}_{style}", sparts, SHARP - {"Hair"} | {"Helmet", "Leaf", "Spot"})
            export(rig, obs, os.path.join(OUT, f"{name}_{style}.gltf"))

        # The robot's extras, made to this species' measure, with their sway_* bones.
        worn, bones = extras(WEARS[species])
        for extra in worn:
            rig, mesh = fresh(os.path.join(OUT, "robot.gltf"))
            for k, (rgb, metal, rough) in MATS.items():
                material(k, rgb, metal, rough)
            bpy.data.objects.remove(mesh)
            bpy.context.view_layer.objects.active = rig
            bpy.ops.object.mode_set(mode="EDIT")
            for bone, parent, head, tail in bones:
                b = rig.data.edit_bones.new(bone)
                b.head, b.tail = Vector(head), Vector(tail)
                b.parent = rig.data.edit_bones[parent]
            bpy.ops.object.mode_set(mode="OBJECT")
            ob = build(rig, f"{name}_{extra}", worn[extra])
            export(rig, [ob], os.path.join(OUT, f"{name}_{extra}.gltf"))


if __name__ == "__main__":
    main()
