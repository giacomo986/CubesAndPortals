# The front's things, built in Blender with the helpers of
# tools/make_fps_models.py, rigged, animated and exported into games/front.
#
#   flatpak run org.blender.Blender --background --python tools/make_front_models.py
#
# Starts from an empty file and leaves the work in assets/blender/front.blend.
#
#   robot_arms   (into front_core, beside the robot) the arms in first person
#                holding the front's guns, a bone tool_<gun> each: pistol,
#                rifle, lmg and grenade. Clips idle_, raise_ and fire_<gun>;
#                fire_grenade is the throw, the grenade gone from the fist as
#                it leaves.
#   <gun>_held   each gun on its own about its grip, barrel +Y: in a hand seen
#                by others. grenade_held is also the grenade in flight.
#   soldier_red, soldier_blue
#                the bots, front +Y, feet on z = 0, a rifle in their arms.
#                Clips idle, walk, run, fire (looped while it shoots), throw
#                and death.
import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_fps_models as d  # noqa: E402

ROOT = d.ROOT
OUT = os.path.join(ROOT, "games", "front", "mods", "front_game")
BLEND = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "front.blend")
HIDDEN = d.HIDDEN


# ---------------------------------------------------------------- the guns

def rifle(p, b, flash=None):
    p.box(b, "gunmetal", (0, 0.06, 0.02), (0.05, 0.26, 0.07))            # receiver
    p.box(b, "polymer", (0, 0.27, 0.02), (0.056, 0.17, 0.06))            # handguard
    p.cyl(b, "steel", (0, 0.43, 0.035), 0.012, 0.18, "Y")                 # barrel
    p.cyl(b, "gunmetal", (0, 0.525, 0.035), 0.017, 0.03, "Y")            # muzzle
    p.box(b, "olive", (0, -0.17, 0.005), (0.045, 0.2, 0.075), (0.08, 0, 0))   # stock
    p.box(b, "polymer", (0, -0.005, -0.055), (0.034, 0.045, 0.1), (-0.25, 0, 0))
    p.box(b, "gunmetal", (0, 0.1, -0.075), (0.03, 0.065, 0.12), (0.25, 0, 0))   # magazine
    p.box(b, "steel", (0, 0.04, 0.07), (0.012, 0.05, 0.03))              # sight
    p.box(b, "steel", (0, 0.33, 0.06), (0.008, 0.012, 0.02))
    if flash:
        p.ball(flash, "flash", (0, 0.57, 0.035), 0.055, (1.1, 1.7, 1.1), 1)


def lmg(p, b, flash=None):
    p.box(b, "gunmetal", (0, 0.07, 0.02), (0.07, 0.3, 0.09))             # receiver
    p.box(b, "steel", (0, 0.1, 0.075), (0.06, 0.18, 0.02))               # feed cover
    p.cyl(b, "steel", (0, 0.42, 0.03), 0.017, 0.36, "Y")                  # barrel
    p.cyl(b, "gunmetal", (0, 0.3, 0.03), 0.026, 0.12, "Y", seg=10)       # its jacket
    p.box(b, "olive", (-0.06, 0.08, -0.04), (0.07, 0.12, 0.11))          # the box of rounds
    p.box(b, "brass", (-0.03, 0.08, 0.035), (0.03, 0.02, 0.02))
    p.box(b, "polymer", (0, -0.19, 0.0), (0.05, 0.22, 0.08), (0.06, 0, 0))   # stock
    p.box(b, "polymer", (0, -0.005, -0.06), (0.036, 0.045, 0.1), (-0.25, 0, 0))
    p.box(b, "steel", (0, 0.06, 0.12), (0.025, 0.1, 0.012))              # carry handle
    for sx in (-1, 1):                                                   # the bipod, folded
        p.limb(b, "steel", (sx * 0.015, 0.5, 0.0), (sx * 0.02, 0.32, -0.01), 0.01)
    if flash:
        p.ball(flash, "flash", (0, 0.63, 0.03), 0.075, (1.4, 1.8, 1.4), 1)


def grenade(p, b):
    p.ball(b, "olive", (0, 0.0, -0.05), 0.045, (1, 1, 1.15), 2)
    p.box(b, "steel", (0.0, 0.0, 0.0), (0.02, 0.02, 0.025))               # its head
    p.limb(b, "steel", (0.012, 0.0, 0.0), (0.03, 0.0, -0.07), 0.008)      # the lever
    p.cyl(b, "steel", (-0.018, 0.0, 0.0), 0.01, 0.003, "X", seg=8)        # the ring


def sniper(p, b, flash=None):
    """A bolt rifle with a long barrel, a scope on top and a cheek rest."""
    p.box(b, "wood", (0, 0.05, 0.015), (0.055, 0.34, 0.075))               # the stock's fore end
    p.box(b, "wood", (0, -0.22, -0.005), (0.05, 0.26, 0.1), (0.08, 0, 0))  # the butt
    p.box(b, "wood", (0, -0.17, 0.06), (0.04, 0.12, 0.03))                 # cheek rest
    p.box(b, "gunmetal", (0, 0.08, 0.05), (0.04, 0.2, 0.04))               # the action
    p.cyl(b, "steel", (0, 0.46, 0.05), 0.012, 0.6, "Y")                     # the barrel
    p.cyl(b, "gunmetal", (0, 0.77, 0.05), 0.02, 0.05, "Y")                 # its brake
    p.box(b, "polymer", (0, -0.005, -0.055), (0.034, 0.045, 0.1), (-0.3, 0, 0))   # grip
    p.limb(b, "steel", (0.035, 0.05, 0.06), (0.06, 0.05, 0.03), 0.012)     # the bolt
    p.cyl(b, "polymer", (0, 0.1, 0.115), 0.025, 0.28, "Y", seg=12)         # the scope
    p.cyl(b, "polymer", (0, 0.27, 0.115), 0.034, 0.07, "Y", seg=12)
    p.cyl(b, "glass", (0, 0.305, 0.115), 0.03, 0.004, "Y", seg=12)
    for y in (0.03, 0.16):
        p.box(b, "steel", (0, y, 0.085), (0.02, 0.02, 0.04))               # its rings
    if flash:
        p.ball(flash, "flash", (0, 0.82, 0.05), 0.07, (1.3, 1.9, 1.3), 1)


def launcher(p, b, flash=None):
    """A tube on the shoulder: the grip under its middle, a sight on the left,
    a rocket's nose in its mouth."""
    p.cyl(b, "olive", (0, 0.12, 0.11), 0.055, 1.1, "Y", seg=14)          # the tube
    p.cyl(b, "gunmetal", (0, 0.68, 0.11), 0.062, 0.06, "Y", seg=14)      # its mouth
    p.cyl(b, "gunmetal", (0, -0.44, 0.11), 0.068, 0.08, "Y", seg=14)     # and its back
    p.cyl(b, "olive", (0, 0.66, 0.11), 0.03, 0.08, "Y", r2=0.0, seg=10)  # the rocket's nose
    p.box(b, "polymer", (0, -0.005, -0.055), (0.034, 0.045, 0.1), (-0.25, 0, 0))   # grip
    p.box(b, "polymer", (0, 0.03, 0.04), (0.04, 0.06, 0.05))
    p.box(b, "polymer", (0, 0.28, -0.03), (0.032, 0.04, 0.08))           # front grip
    p.box(b, "steel", (-0.075, 0.2, 0.15), (0.03, 0.08, 0.05))           # the sight
    if flash:
        p.ball(flash, "flash", (0, 0.76, 0.11), 0.09, (1.5, 1.6, 1.5), 1)


d.GUNS = {
    #            where the grip top is      the left hand: on what, where (gun frame)
    "pistol":  {"at": (0.27, 0.6, -0.225), "left": None},
    "rifle":   {"at": (0.23, 0.5, -0.235), "left": (None, (0, 0.27, -0.03))},
    "lmg":     {"at": (0.22, 0.5, -0.25), "left": (None, (0, 0.24, -0.05))},
    "grenade": {"at": (0.27, 0.6, -0.26), "left": None},
    "launcher": {"at": (0.2, 0.45, -0.3), "left": (None, (0, 0.28, -0.07))},
    "sniper":  {"at": (0.22, 0.5, -0.24), "left": (None, (0, 0.3, -0.02))},
}
d.PARTS = {
    "flash_pistol": ("tool_pistol", (0, 0.21, 0.03)),
    "flash_rifle": ("tool_rifle", (0, 0.57, 0.035)),
    "flash_lmg": ("tool_lmg", (0, 0.63, 0.03)),
    "flash_launcher": ("tool_launcher", (0, 0.76, 0.11)),
    "flash_sniper": ("tool_sniper", (0, 0.82, 0.05)),
}
d.ARMS_OUT = os.path.join(ROOT, "games", "front", "mods", "front_core")


def front_guns(p):
    d.pistol(p, "tool_pistol", "flash_pistol")
    rifle(p, "tool_rifle", "flash_rifle")
    lmg(p, "tool_lmg", "flash_lmg")
    grenade(p, "tool_grenade")
    launcher(p, "tool_launcher", "flash_launcher")
    sniper(p, "tool_sniper", "flash_sniper")


# Aimed down the sights (the right button): where each gun's grip top goes so
# its sight is on the eye's line, straight ahead -- the sight's height over the
# grip, times the guns' size in the hand, below the eye. The sniper's is
# lowered out of the picture: its scope is drawn by the HUD.
ADS = {
    "pistol":   (0.04, 0.36, -0.1),
    "rifle":    (0.03, 0.28, -0.125),
    "lmg":      (0.04, 0.3, -0.18),
    "launcher": (0.095, 0.4, -0.19),
    "sniper":   (0.18, 0.3, -0.6),
}


def front_clips(capture, moved, flash, sub_matrix, free_left, Q_R):
    curve, Z3 = d.curve, (0, 0, 0)

    # Held aimed: the gun from its place to the sight line, its own turn
    # (d.AIM) taken off so it points down the middle of the view; and a shot
    # from there, the kick smaller than from the hip.
    def aimed_at(gun, extra=Z3, turn=Z3):
        dat = tuple(a - b + e for a, b, e in zip(ADS[gun], d.GUNS[gun]["at"], extra))
        drot = tuple(-a + t for a, t in zip(d.AIM, turn))
        if gun == "sniper":
            drot = (-0.9 + turn[0], -d.AIM[1], 0.4)
        return dat, drot

    # Aiming brings the right shoulder in under the cheek (fp_arms reads
    # d.SHOULDER_R as it captures): from where it holds a gun at the hip the
    # hand fell 7 cm short of a grip on the sight line.
    from mathutils import Vector
    hip_shoulder = d.SHOULDER_R
    d.SHOULDER_R = Vector((0.3, 0.32, -0.52))
    for gun in ADS:
        capture(f"aim_{gun}", 0.3, lambda t, g=gun: moved(g, t, *aimed_at(g)))
    for gun, kick, every in (("pistol", 0.06, 0.28), ("rifle", 0.025, 0.1), ("lmg", 0.03, 0.1),
                             ("sniper", 0.0, 1.2), ("launcher", 0.06, 2.5)):
        flash_name = f"flash_{gun}"
        capture(f"fire_{gun}_aim", every, lambda t, g=gun, k=kick, e=every, fl=flash_name: moved(
            g, t, *aimed_at(g, curve(t, [(0, Z3), (min(0.03, e / 3), (0, -k, k * 0.2)), (e, Z3)]),
                            curve(t, [(0, Z3), (min(0.03, e / 3), (k * 1.5, 0, 0)), (e, Z3)])),
            flash(fl, t, 0, 0.045) if g != "sniper" else {}))
    d.SHOULDER_R = hip_shoulder

    # Reloading: the gun tipped over and raised to look at, the left hand to
    # the magazine, down to the pouch, back with a fresh one, home -- as the
    # robot does seen from outside (tools/make_front_poses.py), over the time
    # front_game gives each gun.
    from mathutils import Matrix, Vector
    mags = {"pistol": (0, -0.01, -0.11), "rifle": (0, 0.1, -0.1), "lmg": (-0.06, 0.08, -0.06),
            "sniper": (0, 0.08, -0.03), "launcher": (0, -0.44, 0.11)}
    times = {"pistol": 1.2, "rifle": 2.0, "lmg": 4.0, "sniper": 3.0, "launcher": 2.5}
    pocket = free_left((-0.4, 0.5, -0.75), (0.2, 0, 0.3))

    def on_gun(A, local):
        return A @ Matrix.Translation(d.K * (Vector(local) - d.GRIP)) @ Q_R

    for gun, total in times.items():
        def state(t, g=gun, total=total):
            u = t / total
            tilt = curve(u, [(0, 0.0), (0.12, 1.0), (0.85, 1.0), (1.0, 0.0)])
            st = moved(g, t, (0, 0.02 * tilt, 0.05 * tilt), (0.2 * tilt, 0, -0.5 * tilt))
            support = d.GUNS[g]["left"]

            def left(A, u=u, g=g, support=support):
                mag = on_gun(A, mags[g])
                home = on_gun(A, support[1]) if support else pocket
                if u < 0.25:
                    return d.blend(home, mag, u / 0.25)
                if u < 0.45:
                    return d.blend(mag, pocket, (u - 0.25) / 0.2)
                if u < 0.65:
                    return d.blend(pocket, mag, (u - 0.45) / 0.2)
                if u < 0.85:
                    return mag
                return d.blend(mag, home, (u - 0.85) / 0.15)
            st["left"] = left
            return st
        capture(f"reload_{gun}", total, state)

    # The bolt rifle from the hip: a heavy kick, and the bolt worked.
    capture("fire_sniper", 1.2, lambda t: moved(
        "sniper", t, curve(t, [(0, Z3), (0.06, (0, -0.08, 0.03)), (0.35, Z3), (0.6, (0.03, -0.03, -0.05)),
                              (0.95, (0.03, -0.03, -0.05)), (1.2, Z3)]),
        curve(t, [(0, Z3), (0.06, (0.35, 0, 0.04)), (0.35, Z3), (0.6, (0.1, 0, -0.25)),
                  (0.95, (0.1, 0, -0.25)), (1.2, Z3)]),
        flash("flash_sniper", t, 0, 0.06)))
    capture("fire_pistol", 0.28, lambda t: moved(
        "pistol", t, curve(t, [(0, Z3), (0.05, (0, -0.03, 0.01)), (0.28, Z3)]),
        curve(t, [(0, Z3), (0.05, (0.22, 0, 0.03)), (0.28, Z3)]),
        flash("flash_pistol", t, 0, 0.05)))
    # Automatic: one shot's kick, played again for every round.
    capture("fire_rifle", 0.1, lambda t: moved(
        "rifle", t, curve(t, [(0, Z3), (0.03, (0, -0.02, 0.006)), (0.1, Z3)]),
        curve(t, [(0, Z3), (0.03, (0.06, 0, 0.01)), (0.1, Z3)]),
        flash("flash_rifle", t, 0, 0.045)))
    capture("fire_lmg", 0.1, lambda t: moved(
        "lmg", t, curve(t, [(0, Z3), (0.03, (0.004, -0.025, 0.008)), (0.1, Z3)]),
        curve(t, [(0, Z3), (0.03, (0.07, 0.01, 0.015)), (0.1, Z3)]),
        flash("flash_lmg", t, 0, 0.045)))
    # The throw: back over the shoulder, then over and forward, and the fist
    # is empty from the moment it opens; then the next one comes up.
    capture("fire_grenade", 0.9, lambda t: moved(
        "grenade", t,
        curve(t, [(0, Z3), (0.25, (0.02, -0.25, 0.22)), (0.42, (0.0, 0.18, 0.05)),
                  (0.55, (0.02, 0.05, -0.3)), (0.7, (0.04, -0.09, -0.45)), (0.9, Z3)]),
        curve(t, [(0, Z3), (0.25, (-0.9, 0, 0.3)), (0.42, (0.6, 0, 0.1)), (0.55, (0.9, 0, 0)),
                  (0.7, (-0.9, 0, 0.3)), (0.9, Z3)]),
        {"tool_grenade": {"s": HIDDEN}} if 0.42 <= t < 0.7 else {}))
    # The rocket: a shove back into the shoulder, the flash, and the next one
    # put in from behind (the tube lowered and raised again).
    capture("fire_launcher", 2.5, lambda t: moved(
        "launcher", t,
        curve(t, [(0, Z3), (0.06, (0, -0.08, 0.02)), (0.4, Z3), (0.8, (0.05, -0.05, -0.25)),
                  (2.0, (0.05, -0.05, -0.25)), (2.5, Z3)]),
        curve(t, [(0, Z3), (0.06, (0.25, 0, 0.02)), (0.4, Z3), (0.8, (-0.6, 0.2, 0.2)),
                  (2.0, (-0.6, 0.2, 0.2)), (2.5, Z3)]),
        flash("flash_launcher", t, 0, 0.08)))


def held_guns():
    for name, make in [("pistol", lambda p: d.pistol(p, None)),
                       ("rifle", lambda p: rifle(p, None)),
                       ("lmg", lambda p: lmg(p, None)),
                       ("grenade", lambda p: grenade(p, None)),
                       ("launcher", lambda p: launcher(p, None)),
                       ("sniper", lambda p: sniper(p, None))]:
        p = d.Parts()
        make(p)
        obj = p.build(f"{name}_held")
        d.export([obj], os.path.join(OUT, f"{name}_held.gltf"), False)
        d.put_aside([obj], -4.0 - len(name) * 0.1)


# ---------------------------------------------------------------- the soldiers

def soldier(name, team, x, body=None, extra=None):
    """The ghoul's frame (tools/make_fps_models.py) as a living soldier: a
    helmet and a vest in the team's colour, both hands on a gun. The gun is
    not its own: it is what it holds (core.mob_act's item, the gun's
    <gun>_held model), drawn by the client at hand_R -- placed so a held
    model's grip lands where the fist is, and sized by hand_R's distance
    from its parent (forearm_R, 0.24 back: 0.24 / 0.21, the rifle's old 1.15)."""
    rg = d.rig(f"{name}_rig", d.biped(name, 0.86, 1.15, 1.5, 1.4, 0.22))
    d.add_bone(rg, "flash", "arm_R", (0.03, 0.76, 1.17))
    d.add_bone(rg, "forearm_R", "arm_R", (0.03, -0.0315, 1.2275))
    d.add_bone(rg, "hand_R", "forearm_R", (0.03, 0.2085, 1.2275))
    p = d.Parts()
    if body:   # another body on the same bones (tools/make_assault_models.py)
        body(p)
    for sx, b in ((-1, "leg_L"), (1, "leg_R")) if not body else ():
        lx = sx * 0.11
        p.limb(b, "khaki", (lx, 0, 0.88), (lx, 0, 0.1), 0.15, 0.16)
        p.box(b, "polymer", (lx, 0.03, 0.05), (0.15, 0.25, 0.1))          # boots
    if not body:
        p.box("hips", "khaki", (0, 0, 0.9), (0.36, 0.2, 0.14))
        p.box("hips", "polymer", (0, 0, 0.97), (0.38, 0.22, 0.04))           # belt
        p.box("chest", "khaki", (0, 0, 1.17), (0.42, 0.24, 0.46))
        p.box("chest", team, (0, 0.0, 1.2), (0.44, 0.27, 0.34))              # vest
        for sx in (-0.1, 0.0, 0.1):
            p.box("chest", "olive", (sx, 0.14, 1.12), (0.07, 0.03, 0.09))    # pouches
        p.box("head", "skin", (0, 0.02, 1.55), (0.2, 0.2, 0.22))
        p.box("head", team, (0, 0.0, 1.67), (0.25, 0.25, 0.08))              # helmet
        p.box("head", team, (0, 0.0, 1.62), (0.27, 0.27, 0.025))             # its brim
        p.box("head", "polymer", (-0.05, 0.12, 1.56), (0.04, 0.01, 0.02))
        p.box("head", "polymer", (0.05, 0.12, 1.56), (0.04, 0.01, 0.02))
        p.limb("arm_R", "khaki", (0.22, 0, 1.4), (0.2, 0.28, 1.14), 0.11)
        p.limb("arm_L", "khaki", (-0.22, 0, 1.4), (-0.08, 0.36, 1.12), 0.11)
        p.box("arm_R", "glove", (0.12, 0.28, 1.12), (0.07, 0.08, 0.07))
        p.box("arm_L", "glove", (-0.04, 0.42, 1.12), (0.07, 0.08, 0.07))
    mesh = p.build(f"{name}_mesh")
    d.skin(mesh, rg)
    f = d.Parts()
    f.ball("flash", "flash", (0.03, 0.88, 1.21), 0.07, (1, 1.6, 1), 1)
    fl = f.build(f"{name}_flash")
    d.skin(fl, rg)

    hidden = {"flash": {"s": 0.0}}
    held = {"arm_L": [(0.0, {})], "arm_R": [(0.0, {})]}
    d.clip(rg, "idle", 2.4, {
        "chest": [(0.0, {}), (0.5, {"r": (0.04, 0, 0.04)})],
        "head": [(0.0, {}), (0.5, {"r": (0.05, 0, -0.25)})],
    }, hidden, loop=True)
    g = d.gait(0.45, 0.0, 0.025)
    g.update(held)
    d.clip(rg, "walk", 1.0, g, hidden, loop=True)
    g = d.gait(0.85, 0.0, 0.05, 0.22)
    g.update(held)
    d.clip(rg, "run", 0.6, g, hidden, loop=True)
    # A burst, looped while it shoots (core.mob_act): three kicks and flashes.
    keys = {"arm_R": [(0.0, {})], "arm_L": [(0.0, {})], "flash": [(0.0, {"s": 0.0})]}
    for i in range(3):
        t0 = i / 3
        keys["arm_R"] += [(t0 + 0.02, {"r": (0.08, 0, 0)}), (t0 + 0.15, {})]
        keys["arm_L"] += [(t0 + 0.02, {"r": (0.08, 0, 0)}), (t0 + 0.15, {})]
        keys["flash"] += [(t0 + 0.01, {"s": 1.0}), (t0 + 0.06, {"s": 1.0}), (t0 + 0.08, {"s": 0.0})]
    d.clip(rg, "fire", 0.36, keys, hidden, loop=True)
    # The throw: the left arm over the shoulder and forward, the rifle lowered.
    d.clip(rg, "throw", 0.9, {
        "chest": [(0.0, {}), (0.3, {"r": (-0.15, 0, 0.3)}), (0.5, {"r": (0.25, 0, -0.2)}), (0.9, {})],
        "arm_L": [(0.0, {}), (0.3, {"r": (2.6, 0, 0.2)}), (0.5, {"r": (-0.6, 0, 0)}), (0.9, {})],
        "arm_R": [(0.0, {}), (0.3, {"r": (-0.5, 0, 0)}), (0.9, {})],
    }, hidden)
    # Seated in a machine (the front's bots): down onto the seat, legs out in
    # front, the hips 0.3 over where the feet stood -- a seat is a cushion's
    # top less 0.3, as for the robot's drive_idle.
    d.clip(rg, "sit", 1.0, {
        "hips": [(0.0, {"l": (0, -0.6, 0)})],
        "leg_L": [(0.0, {"r": (1.45, 0, -0.05)})],
        "leg_R": [(0.0, {"r": (1.45, 0, 0.05)})],
        "arm_L": [(0.0, {})], "arm_R": [(0.0, {})],
    }, hidden, loop=True)
    # Knees first, then face down, the gun falling out of its arms' aim.
    d.clip(rg, "death", 1.0, {
        "hips": [(0.0, {}), (0.35, {"l": (0, -0.35, 0)})],
        "leg_L": [(0.0, {}), (0.35, {"r": (1.4, 0, 0)})],
        "leg_R": [(0.0, {}), (0.35, {"r": (1.3, 0, 0.1)})],
        "root": [(0.0, {}), (0.35, {}), (0.8, {"r": (-1.3, 0, 0), "l": (0, -0.1, 0)})],
        "chest": [(0.0, {}), (0.3, {"r": (0.4, 0, 0)})],
        "head": [(0.0, {}), (0.4, {"r": (0.4, 0, 0.3)})],
        "arm_L": [(0.0, {}), (0.5, {"r": (-0.9, 0, -0.3)})],
        "arm_R": [(0.0, {}), (0.5, {"r": (-1.1, 0, 0.3)})],
    }, hidden)
    if extra:   # clips of its own (tools/make_assault_models.py)
        extra(rg, hidden)
    d.export([rg, mesh, fl], os.path.join(OUT, f"{name}.gltf"), True)
    d.put_aside([rg], x)


# ---------------------------------------------------------------- the machines
#
# Each machine is built about the middle of the bottom of its hull, front +Y,
# as core.craft puts it: what its physics calls the base. The parts that turn
# by the mod's aim (a turret, a barrel, a gun) are models of their own about
# their pivot, mounted by core.craft.mount; the parts that only spin (wheels,
# rotors, a propeller) are bones of the machine, turned by its "drive" or
# "spin" clip. Every builder takes `m`, which turns a material's name into the
# one used: the burnt wreck is the same machine through m = burnt.

def plain(name):
    return name


def burnt(name):
    return "flash" if name == "flash" else "burnt"


def rigid_out(name, make, x):
    p = d.Parts()
    make(p)
    obj = p.build(name)
    d.export([obj], os.path.join(OUT, f"{name}.gltf"), False)
    d.put_aside([obj], x)


def rigged_out(name, bones, make, clips, x, aims=None):
    rg = d.rig(f"{name}_rig", [("root", None, (0, 0, 0))] + bones)
    if aims:   # bones that point along an axis of their own and not up
        from mathutils import Vector
        bpy.context.view_layer.objects.active = rg
        bpy.ops.object.mode_set(mode="EDIT")
        for bname, axis in aims.items():
            eb = rg.data.edit_bones[bname]
            eb.tail = eb.head + Vector(axis).normalized() * 0.1
            eb.roll = 0.0
        bpy.ops.object.mode_set(mode="OBJECT")
        for pb in rg.pose.bones:
            pb.rotation_mode = "XYZ"
    p = d.Parts()
    make(p, "root")
    mesh = p.build(f"{name}_mesh")
    d.skin(mesh, rg)
    for clip_name, seconds, keys in clips:
        d.clip(rg, clip_name, seconds, keys)
    d.export([rg, mesh], os.path.join(OUT, f"{name}.gltf"), True)
    d.put_aside([rg], x)


def turn(bone, axis, seconds_unused=None):
    """A bone turned once round about a world axis over its clip, in quarters
    so the turn is never read the short way back."""
    tau = 2 * math.pi
    i = {"x": 0, "z": 1, "y": 2}[axis]   # world x, up, front -> the bone's x, y, -z
    sign = -1 if axis == "y" else 1
    keys = []
    for k in range(5):
        r = [0.0, 0.0, 0.0]
        r[i] = sign * tau * k / 4
        keys.append((k / 4, {"r": tuple(r)}))
    return {bone: keys}


# --- the jeep: 1.0 x 0.45 x 2.0 half, wheels of 0.405 (core.craft's wheeled)
JEEP_WHEEL_R = 0.405
# Not wheel_*: a model with bones of that name is a kart to the client, which
# turns them itself and sits the world's player model in it.
JEEP_WHEELS = {"tire_FL": (-0.85, 1.445), "tire_FR": (0.85, 1.445),
               "tire_RL": (-0.85, -1.445), "tire_RR": (0.85, -1.445)}


# The steering wheel, under the robot's hands as drive_idle holds them (0.2
# either side, 0.15 ahead of and 0.66 over its feet), the driver's feet at
# the seat's (-0.45, 0.42, 1.1); its column leans back to the driver. The
# client turns a bone named steering_wheel about the way it points as the
# body turns (wheel_pose), and steer_* about up: the front wheels hang from
# those.
JEEP_WHEEL_AT = (-0.45, 0.57, 1.76)
JEEP_COLUMN = (0.0, -0.6, 0.8)


def steering_wheel(p, bone, m=plain):
    from mathutils import Matrix, Vector
    axis = Vector(JEEP_COLUMN).normalized()
    was = p.pre
    p.pre = was @ Matrix.Translation(Vector(JEEP_WHEEL_AT)) @ Vector((0, 0, 1)).rotation_difference(axis).to_matrix().to_4x4()
    for k in range(12):                                                    # the rim
        a = k * math.pi / 6
        p.box(bone, m("polymer"), (0.2 * math.cos(a), 0.2 * math.sin(a), 0), (0.06, 0.11, 0.04), (0, 0, a))
    for a in (0.0, 2.1, 4.2):                                               # the spokes
        p.limb(bone, m("steel"), (0, 0, 0), (0.19 * math.cos(a), 0.19 * math.sin(a), 0), 0.025)
    p.cyl(bone, m("gunmetal"), (0, 0, 0), 0.045, 0.05, "Z", seg=10)        # the hub
    p.cyl(bone, m("steel"), (0, 0, -0.3), 0.03, 0.6, "Z", seg=8)           # the column
    p.pre = was


def jeep(p, root, m=plain, wheel_bone="steering_wheel"):
    steering_wheel(p, wheel_bone or root, m)
    for bone, (x, y) in JEEP_WHEELS.items():
        p.cyl(bone, m("tire"), (x, y, JEEP_WHEEL_R), JEEP_WHEEL_R, 0.3, "X", seg=14)
        p.cyl(bone, m("steel"), (x * 1.08, y, JEEP_WHEEL_R), 0.16, 0.06, "X", seg=10)
        for k in range(8):                                                   # the tread's lugs
            a = k * math.pi / 4
            p.box(bone, m("polymer"), (x, y + math.cos(a) * JEEP_WHEEL_R, JEEP_WHEEL_R + math.sin(a) * JEEP_WHEEL_R),
                  (0.32, 0.09, 0.07), (a, 0, 0))
    p.box(root, m("olive"), (0, 0, 0.95), (1.85, 3.9, 0.45))                # the tub
    p.box(root, m("olive"), (0, 1.35, 1.25), (1.75, 1.2, 0.3))              # the bonnet
    p.box(root, m("dark_olive"), (0, 1.96, 1.0), (1.6, 0.06, 0.4))          # the grille
    for sx in (-0.55, 0.55):
        p.cyl(root, m("glass"), (sx, 1.99, 1.08), 0.09, 0.04, "Y", seg=10)  # lamps
    for sx in (-1, 1):                                                       # the wings
        p.box(root, m("olive"), (sx * 0.85, 1.45, 0.95), (0.4, 1.0, 0.08))
        p.box(root, m("olive"), (sx * 0.85, -1.45, 0.95), (0.4, 1.0, 0.08))
    for sx in (-0.8125, 0.8125):                                             # the screen's frame
        p.box(root, m("steel"), (sx, 0.72, 1.7), (0.075, 0.05, 0.55))
    for sz in (1.45, 1.95):
        p.box(root, m("steel"), (0, 0.72, sz), (1.55, 0.05, 0.05))
    p.box(root, m("window"), (0, 0.72, 1.7), (1.55, 0.03, 0.45))
    for sx in (-0.45, 0.45):                                                 # the seats
        p.box(root, m("polymer"), (sx, 0.15, 1.3), (0.55, 0.55, 0.15))
        p.box(root, m("polymer"), (sx, -0.1, 1.6), (0.55, 0.12, 0.5))
    p.box(root, m("polymer"), (0, -1.2, 1.3), (1.4, 0.6, 0.15))
    for sx in (-0.85, 0.85):                                                 # the roll bar
        p.limb(root, m("steel"), (sx, -0.5, 1.15), (sx, -0.5, 2.1), 0.07)
    p.limb(root, m("steel"), (-0.85, -0.5, 2.1), (0.85, -0.5, 2.1), 0.07)
    p.limb(root, m("steel"), (0, -0.9, 1.15), (0, -0.9, 1.95), 0.08)        # the gun's post
    p.cyl(root, m("tire"), (0, -1.98, 1.05), 0.36, 0.22, "Y", seg=14)      # the spare


def jeep_build(m):
    def make(p, root):
        jeep(p, root, m)
    return make


# --- the tank: 1.6 x 0.6 x 3.2 half, on tracks (core.craft's tracked)
def tank_hull(p, m=plain):
    p.box(None, m("olive"), (0, 0, 1.25), (3.0, 5.6, 0.75))                  # the hull
    p.box(None, m("olive"), (0, 2.95, 1.05), (3.0, 0.9, 0.5), (0.5, 0, 0))   # the glacis
    p.box(None, m("dark_olive"), (0, -2.7, 1.45), (2.6, 0.7, 0.35))          # the engine deck
    for k in range(5):
        p.box(None, m("polymer"), (0, -2.45 - k * 0.12, 1.64), (2.2, 0.05, 0.02))
    for sx in (-1, 1):
        x = sx * 1.4
        p.box(None, m("tire"), (x, 0, 0.55), (0.6, 6.4, 0.9))               # the track
        p.box(None, m("tire"), (x, 3.25, 0.7), (0.6, 0.3, 0.6), (0.6, 0, 0))
        p.box(None, m("tire"), (x, -3.25, 0.7), (0.6, 0.3, 0.6), (-0.6, 0, 0))
        for k in range(6):                                                   # its wheels
            p.cyl(None, m("steel"), (sx * 1.72, 2.4 - k * 0.96, 0.4), 0.33, 0.06, "X", seg=12)
        p.box(None, m("olive"), (x, 0, 1.06), (0.7, 6.4, 0.06))              # the fender
    p.box(None, m("steel"), (-0.9, 2.3, 1.68), (0.5, 0.35, 0.06))            # the driver's hatch


def tank_turret(p, m=plain):
    p.box(None, m("olive"), (0, -0.2, 0.35), (2.2, 2.6, 0.7))
    p.box(None, m("olive"), (0, 1.15, 0.3), (1.6, 0.5, 0.55), (0.35, 0, 0))   # its front
    p.box(None, m("dark_olive"), (0, -1.65, 0.4), (2.0, 0.5, 0.55))          # the bustle
    # The commander's hatch, open: a rim round the hole (whoever stands in it
    # is the hole's filling) and the lid stood up behind it.
    for k in range(10):
        a = k * 2 * math.pi / 10
        p.box(None, m("steel"), (-0.45 + 0.33 * math.cos(a), -0.4 + 0.33 * math.sin(a), 0.74),
              (0.1, 0.22, 0.1), (0, 0, a))
    p.cyl(None, m("olive"), (-0.45, -0.85, 1.05), 0.32, 0.08, "Y", seg=14)   # the lid
    for sx in (-1, 1):
        p.box(None, m("dark_olive"), (sx * 1.18, -0.3, 0.4), (0.15, 1.6, 0.4))
        for k in range(3):                                                   # smoke dischargers
            p.cyl(None, m("gunmetal"), (sx * 1.0, 0.85, 0.55 + k * 0.1), 0.05, 0.25, "Y", seg=8)


def tank_gun(p, m=plain):
    p.box(None, m("olive"), (0, 0.1, 0), (0.9, 0.5, 0.5))                    # the mantlet
    p.cyl(None, m("gunmetal"), (0, 1.9, 0), 0.1, 3.2, "Y", seg=14)          # the barrel
    p.cyl(None, m("gunmetal"), (0, 1.4, 0), 0.15, 0.6, "Y", seg=14)         # its fume extractor
    p.cyl(None, m("gunmetal"), (0, 3.55, 0), 0.14, 0.2, "Y", seg=14)        # the muzzle


# --- the machine gun every seat and nest uses: about its pivot, barrel +Y
def mg(p, m=plain):
    p.box(None, m("gunmetal"), (0, 0.05, 0.03), (0.14, 0.6, 0.16))
    p.cyl(None, m("steel"), (0, 0.75, 0.04), 0.03, 0.8, "Y", seg=10)
    p.cyl(None, m("gunmetal"), (0, 0.55, 0.04), 0.05, 0.35, "Y", seg=10)
    p.box(None, m("olive"), (-0.15, 0.05, 0.0), (0.14, 0.22, 0.2))          # rounds
    for sx in (-0.08, 0.08):                                                 # spade grips
        p.limb(None, m("polymer"), (sx, -0.25, 0.03), (sx, -0.4, 0.03), 0.04)
    p.box(None, m("steel"), (0, 0.0, -0.1), (0.08, 0.08, 0.12))             # the pintle


def mg_tripod(p, m=plain):
    for a in (0, 2.1, 4.2):
        p.limb(None, m("steel"), (0, 0, 0.95), (0.6 * math.sin(a), 0.6 * math.cos(a), 0), 0.05)
    p.cyl(None, m("gunmetal"), (0, 0, 0.92), 0.07, 0.14, "Z", seg=10)


# --- the helicopter: 1.0 x 0.8 x 3.0 half, on skids (core.craft's rotor)
def heli(p, root, m=plain):
    p.box(root, m("olive"), (0, 0.6, 0.95), (1.8, 2.8, 1.3))                 # the cabin
    p.box(root, m("window"), (0, 2.25, 0.95), (1.5, 0.7, 1.0))               # the nose
    p.box(root, m("dark_olive"), (0, 0.4, 1.7), (1.2, 1.8, 0.3))             # the engine
    p.cyl(root, m("olive"), (0, -2.4, 1.3), 0.25, 4.2, "Y", r2=0.12, seg=12) # the boom
    p.box(root, m("olive"), (0, -4.4, 1.75), (0.08, 0.6, 0.9))               # the fin
    p.box(root, m("olive"), (0, -3.9, 1.3), (1.4, 0.4, 0.06))                # the tailplane
    for sx in (-1, 1):                                                        # skids and struts
        p.cyl(root, m("steel"), (sx * 0.8, 0.5, 0.06), 0.05, 3.2, "Y", seg=8)
        for y in (-0.3, 1.2):
            p.limb(root, m("steel"), (sx * 0.8, y, 0.06), (sx * 0.6, y, 0.35), 0.06)
        p.cyl(root, m("gunmetal"), (sx * 1.15, 0.7, 0.75), 0.17, 1.1, "Y", seg=10)   # rocket pods
        p.limb(root, m("steel"), (sx * 0.9, 0.7, 0.8), (sx * 1.05, 0.7, 0.8), 0.06)
    p.cyl(root, m("steel"), (0, 0.6, 1.95), 0.08, 0.3, "Z", seg=8)           # the mast
    p.cyl("rotor", m("gunmetal"), (0, 0.6, 2.12), 0.18, 0.12, "Z", seg=10)
    for a in (0, math.pi / 2):                                                # four blades
        p.box("rotor", m("rotor"), (0, 0.6, 2.15), (9.0, 0.28, 0.04), (0, 0, a))
    p.cyl("tail_rotor", m("gunmetal"), (0.2, -4.45, 1.85), 0.06, 0.1, "X", seg=8)
    p.box("tail_rotor", m("rotor"), (0.25, -4.45, 1.85), (0.04, 0.16, 1.3))


HELI_BONES = [("rotor", "root", (0, 0.6, 2.1)), ("tail_rotor", "root", (0.2, -4.45, 1.85))]


def heli_build(m):
    def make(p, root):
        heli(p, root, m)
    return make


# --- the plane: 0.8 x 0.6 x 3.0 half (core.craft's wing); the wings are wider
def plane(p, root, m=plain):
    p.cyl(root, m("olive"), (0, 0, 0.75), 0.5, 5.0, "Y", seg=14)             # the fuselage
    p.cyl(root, m("olive"), (0, 2.8, 0.75), 0.5, 0.6, "Y", r2=0.3, seg=14)   # the cowling
    p.cyl(root, m("olive"), (0, -2.9, 0.85), 0.45, 0.8, "Y", r2=0.12, seg=14)
    p.box(root, m("olive"), (0, 0.4, 0.55), (10.0, 1.6, 0.12))               # the wings
    p.box(root, m("olive"), (0, -2.9, 0.9), (3.4, 0.8, 0.08))                # the tailplane
    p.box(root, m("olive"), (0, -3.05, 1.45), (0.08, 0.9, 1.1))              # the fin
    p.ball(root, m("window"), (0, 0.1, 1.2), 0.4, (0.9, 1.8, 0.8), 2)        # the canopy
    for sx in (-1, 1):
        p.limb(root, m("steel"), (sx * 0.9, 0.9, 0.5), (sx * 1.0, 0.95, 0.15), 0.07)   # the gear
        p.cyl(root, m("tire"), (sx * 1.0, 0.95, 0.15), 0.15, 0.1, "X", seg=12)
        p.cyl(root, m("gunmetal"), (sx * 2.2, 0.9, 0.48), 0.05, 0.6, "Y", seg=8)     # the guns
    p.cyl("prop", m("gunmetal"), (0, 3.15, 0.75), 0.15, 0.2, "Y", r2=0.02, seg=12)
    for a in (0, math.pi * 2 / 3, math.pi * 4 / 3):
        p.box("prop", m("rotor"), (0, 3.1, 0.75), (0.12, 0.04, 2.2), (0, a, 0))


PLANE_BONES = [("prop", "root", (0, 3.1, 0.75))]


def plane_build(m):
    def make(p, root):
        plane(p, root, m)
    return make


# --- the bomber: 1.2 x 0.9 x 4.8 half (core.craft's wing), two engines, a
# bomb bay under the middle and a gun turret on its back (the "mg" part).
def bomber(p, root, m=plain):
    p.cyl(root, m("olive"), (0, 0.2, 1.3), 0.75, 8.0, "Y", seg=16)          # the fuselage
    p.cyl(root, m("olive"), (0, -4.6, 1.45), 0.6, 1.6, "Y", r2=0.15, seg=16) # the tail cone
    p.ball(root, m("window"), (0, 4.4, 1.3), 0.75, (1, 1.1, 1), 2)           # the glazed nose
    p.ball(root, m("window"), (0, 2.6, 2.0), 0.35, (1, 1.8, 0.7), 2)         # the cockpit
    p.box(root, m("dark_olive"), (0, 0.4, 0.55), (0.9, 2.6, 0.06))           # the bay doors
    p.box(root, m("olive"), (0, 0.6, 1.5), (18.0, 2.2, 0.16))                # the wings
    p.box(root, m("olive"), (0, -4.8, 1.6), (6.0, 1.2, 0.1))                 # the tailplane
    for sx in (-1, 1):
        p.box(root, m("olive"), (sx * 2.9, -4.9, 2.3), (0.1, 1.0, 1.4))      # twin fins
        p.cyl(root, m("olive"), (sx * 3.2, 1.4, 1.35), 0.45, 2.6, "Y", seg=14)          # the nacelles
        p.cyl(root, m("gunmetal"), (sx * 3.2, 2.75, 1.35), 0.3, 0.3, "Y", r2=0.12, seg=12)
        p.limb(root, m("steel"), (sx * 3.2, 1.2, 0.95), (sx * 3.2, 1.2, 0.3), 0.09)     # the gear
        p.cyl(root, m("tire"), (sx * 3.2, 1.2, 0.3), 0.3, 0.2, "X", seg=14)
    p.cyl(root, m("tire"), (0, -4.6, 0.75), 0.15, 0.12, "X", seg=10)        # the tail wheel
    p.cyl(root, m("gunmetal"), (0, -1.5, 2.08), 0.4, 0.12, "Z", seg=14)     # the turret ring
    for side, bone in ((-1, "prop_L"), (1, "prop_R")):
        x = side * 3.2
        for a in (0, math.pi * 2 / 3, math.pi * 4 / 3):
            p.box(bone, m("rotor"), (x, 2.95, 1.35), (0.12, 0.04, 2.0), (0, a, 0))


BOMBER_BONES = [("prop_L", "root", (-3.2, 2.95, 1.35)), ("prop_R", "root", (3.2, 2.95, 1.35))]


def bomber_build(m):
    def make(p, root):
        bomber(p, root, m)
    return make


# --- the anti-air launcher: a ring on the ground, and four tubes on a cradle
# that turns and rises (the "aa_launcher" part, about its pivot).
def aa_base(p, m=plain):
    p.cyl(None, m("gunmetal"), (0, 0, 0.1), 0.9, 0.2, "Z", seg=16)
    p.cyl(None, m("olive"), (0, 0, 0.6), 0.25, 0.8, "Z", seg=12)             # the post
    p.box(None, m("polymer"), (0, -0.9, 0.5), (0.4, 0.3, 0.06))             # the step


def aa_launcher(p, m=plain):
    p.box(None, m("olive"), (0, 0, 0), (0.9, 0.5, 0.2))                       # the cradle
    for sx in (-0.22, 0.22):
        for sz in (0.2, 0.45):
            p.cyl(None, m("olive"), (sx, 0.3, sz), 0.12, 1.7, "Y", seg=12)
            p.cyl(None, m("gunmetal"), (sx, 1.17, sz), 0.1, 0.04, "Y", seg=12)
    p.box(None, m("gunmetal"), (0.55, 0.1, 0.35), (0.12, 0.35, 0.18))        # the seeker
    p.ball(None, m("glass"), (0.55, 0.3, 0.35), 0.07, (1, 1, 1), 1)


def missile(p):
    p.cyl(None, "steel", (0, 0, 0), 0.07, 1.0, "Y", seg=10)
    p.cyl(None, "glass", (0, 0.58, 0), 0.07, 0.16, "Y", r2=0.0, seg=10)
    for a in (0, math.pi / 2):
        p.box(None, "olive", (0, -0.4, 0), (0.28, 0.14, 0.02), (0, a, 0))
        p.box(None, "olive", (0, 0.25, 0), (0.18, 0.08, 0.02), (0, a, 0))
    p.ball(None, "flash", (0, -0.6, 0), 0.1, (1, 2.4, 1), 1)


# --- the guns that stay put: a mortar, a howitzer
def mortar_base(p, m=plain):
    p.cyl(None, m("gunmetal"), (0, 0, 0.04), 0.35, 0.08, "Z", seg=14)        # the plate
    for sx in (-1, 1):                                                        # the bipod
        p.limb(None, m("steel"), (sx * 0.3, 0.7, 0), (0, 0.45, 0.65), 0.04)


def mortar_tube(p, m=plain):
    p.cyl(None, m("olive"), (0, 0.6, 0), 0.07, 1.2, "Y", seg=12)
    p.cyl(None, m("gunmetal"), (0, 1.2, 0), 0.085, 0.06, "Y", seg=12)


def howitzer(p, m=plain):
    for sx in (-1, 1):
        p.cyl(None, m("tire"), (sx * 1.0, 0, 0.55), 0.55, 0.25, "X", seg=16)
        p.limb(None, m("olive"), (sx * 0.2, 0, 0.6), (sx * 0.9, -2.6, 0.08), 0.14)   # the trails
    p.limb(None, m("steel"), (-1.0, 0, 0.55), (1.0, 0, 0.55), 0.1)           # the axle
    p.box(None, m("olive"), (0, 0.3, 1.05), (1.8, 0.08, 1.0))                # the shield
    p.box(None, m("olive"), (0, 0, 0.85), (0.6, 0.8, 0.35))                  # the cradle


def howitzer_barrel(p, m=plain):
    p.box(None, m("olive"), (0, -0.2, 0), (0.45, 1.0, 0.45))                 # the breech
    p.cyl(None, m("gunmetal"), (0, 1.6, 0), 0.1, 3.4, "Y", seg=14)
    p.cyl(None, m("gunmetal"), (0, 3.35, 0), 0.15, 0.25, "Y", seg=14)


# --- what flies: a shell, a rocket, a bomb (+Y the way it goes)
def shell(p):
    p.cyl(None, "brass", (0, 0, 0), 0.08, 0.35, "Y", seg=10)
    p.cyl(None, "gunmetal", (0, 0.25, 0), 0.08, 0.15, "Y", r2=0.0, seg=10)
    p.ball(None, "flash", (0, -0.25, 0), 0.08, (1, 1.5, 1), 1)


def rocket(p):
    p.cyl(None, "olive", (0, 0, 0), 0.06, 0.6, "Y", seg=10)
    p.cyl(None, "gunmetal", (0, 0.38, 0), 0.06, 0.16, "Y", r2=0.0, seg=10)
    for a in (0, math.pi / 2):
        p.box(None, "steel", (0, -0.25, 0), (0.25, 0.12, 0.02), (0, a, 0))
    p.ball(None, "flash", (0, -0.4, 0), 0.1, (1, 2.2, 1), 1)


def bomb(p):
    p.ball(None, "dark_olive", (0, 0, 0), 0.2, (1, 2.2, 1), 2)
    for a in (0, math.pi / 2):
        p.box(None, "steel", (0, -0.45, 0), (0.4, 0.12, 0.02), (0, a, 0))


def machines():
    wheel_spin = {}
    for bone in JEEP_WHEELS:
        wheel_spin.update(turn(bone, "x"))
    steer = [("steer_FL", "root", (-0.85, 1.445, JEEP_WHEEL_R)), ("steer_FR", "root", (0.85, 1.445, JEEP_WHEEL_R))]
    tires = [(b, "steer_" + b[-2:] if b[-2] == "F" else "root", (x, y, JEEP_WHEEL_R))
             for b, (x, y) in JEEP_WHEELS.items()]
    rigged_out("jeep", steer + tires + [("steering_wheel", "root", JEEP_WHEEL_AT)],
               jeep_build(plain), [("drive", 0.5, wheel_spin)], 20.0, {"steering_wheel": JEEP_COLUMN})
    spin = {}
    spin.update(turn("rotor", "z"))
    spin.update(turn("tail_rotor", "x"))
    rigged_out("heli", HELI_BONES, heli_build(plain), [("spin", 0.3, spin)], 30.0)
    rigged_out("plane", PLANE_BONES, plane_build(plain), [("spin", 0.15, turn("prop", "y"))], 40.0)
    props = {}
    props.update(turn("prop_L", "y"))
    props.update(turn("prop_R", "y"))
    rigged_out("bomber", BOMBER_BONES, bomber_build(plain), [("spin", 0.15, props)], 45.0)
    for name, make, x in [
            ("tank_hull", lambda p: tank_hull(p), 50.0), ("tank_turret", lambda p: tank_turret(p), 52.0),
            ("tank_gun", lambda p: tank_gun(p), 54.0), ("mg", lambda p: mg(p), 56.0),
            ("mg_tripod", lambda p: mg_tripod(p), 57.0), ("mortar_base", lambda p: mortar_base(p), 58.0),
            ("mortar_tube", lambda p: mortar_tube(p), 59.0), ("howitzer", lambda p: howitzer(p), 60.0),
            ("howitzer_barrel", lambda p: howitzer_barrel(p), 62.0),
            ("shell", shell, 64.0), ("rocket", rocket, 65.0), ("bomb", bomb, 66.0),
            ("aa_base", lambda p: aa_base(p), 67.0), ("aa_launcher", lambda p: aa_launcher(p), 68.0),
            ("missile", missile, 69.0)]:
        rigid_out(name, make, x)
    wrecks()


# ---------------------------------------------------------------- the wrecks
#
# Each machine burnt, rigged by its pieces, with a "death" clip of 5 s played
# once where it came down: the blast throws it about and its loose parts off,
# it settles, and then it sinks into the ground, which is how it goes -- the
# mod takes the entity away when the clip is over. Bone keys as d.clip's: r
# about the bone's x, up and back; l along right, up and back.

WRECK_SECONDS = 5.0


class Pieces(d.Parts):
    """Parts whose unboned primitives go to `as_bone`: the machines' builders
    name only the bones that spin, and a wreck's pieces fly apart."""

    def __init__(self):
        super().__init__()
        self.as_bone = "root"

    def _add(self, made, bone, mat, smooth):
        super()._add(made, bone if bone is not None else self.as_bone, mat, smooth)


def at(keys):
    """[(seconds, key)] as d.clip's [(0..1, key)]."""
    return [(t / WRECK_SECONDS, k) for t, k in keys]


SINK = {"root": at([(0, {}), (3.0, {}), (5.0, {"l": (0, -3.2, 0)})])}


def wreck_out(name, bones, make, keys, x):
    rg = d.rig(f"{name}_rig", [("root", None, (0, 0, 0))] + bones)
    p = Pieces()
    make(p)
    mesh = p.build(f"{name}_mesh")
    d.skin(mesh, rg)
    clip_keys = dict(SINK)
    clip_keys.update({b: at(k) for b, k in keys.items()})
    d.clip(rg, "death", WRECK_SECONDS, clip_keys)
    d.export([rg, mesh], os.path.join(OUT, f"{name}.gltf"), True)
    d.put_aside([rg], x)


def wrecks():
    from mathutils import Matrix

    # The tank: the hull jolts, the turret is thrown off and lands beside it.
    def tank(p):
        p.as_bone = "hull"
        tank_hull(p, burnt)
        p.as_bone = "turret"
        p.pre = Matrix.Translation((0, -0.3, 1.85))
        tank_turret(p, burnt)
        p.pre = p.pre @ Matrix.Translation((0, 1.1, 0.35))
        tank_gun(p, burnt)
        p.pre = Matrix.Identity(4)
    wreck_out("tank_wreck", [("hull", "root", (0, 0, 1.2)), ("turret", "hull", (0, -0.3, 1.85))], tank, {
        "hull": [(0, {}), (0.12, {"l": (0, 0.45, 0), "r": (0.1, 0, 0.06)}), (0.4, {"r": (-0.04, 0, 0.05)}),
                 (0.6, {"r": (0, 0, 0.04)})],
        "turret": [(0, {}), (0.5, {"l": (1.0, 3.8, 0.6), "r": (0.7, 2.2, 0.5)}),
                   (1.0, {"l": (2.2, 1.6, 1.2), "r": (1.3, 4.0, 1.0)}),
                   (1.25, {"l": (2.6, -1.45, 1.4), "r": (1.57, 4.6, 1.2)})],
    }, 70.0)

    # The jeep: up, over onto its back, its wheels off and rolling away.
    def jeep_parts(p):
        p.as_bone = "body"
        jeep(p, "body", burnt, wheel_bone=None)
    wheels = {}
    for bone, (wx, wy) in JEEP_WHEELS.items():
        sx, sy = (1 if wx > 0 else -1), (1 if wy > 0 else -1)
        wheels[bone] = [(0, {}), (0.4, {"l": (sx * 1.5, 1.4, -sy * 0.3), "r": (3.0, 0, 0)}),
                        (0.8, {"l": (sx * 2.4, -0.25, -sy * 0.6), "r": (6.0, 0, 1.3)})]
    wreck_out("jeep_wreck", [("body", "root", (0, 0, 1.0))] +
              [(b, "root", (wx, wy, JEEP_WHEEL_R)) for b, (wx, wy) in JEEP_WHEELS.items()], jeep_parts, {
        "body": [(0, {}), (0.35, {"l": (0, 2.0, 0), "r": (0.2, 0.3, 1.6)}),
                 (0.7, {"l": (0.3, 0.4, 0), "r": (0.15, 0.5, 3.14)}), (0.9, {"l": (0.3, 0.25, 0), "r": (0.1, 0.5, 3.0)})],
        **wheels,
    }, 75.0)

    # The helicopter: it spins round and lies down on its side; the rotor
    # comes off and away, the tail rotor drops.
    def heli_parts(p):
        p.as_bone = "body"
        heli(p, "body", burnt)
    wreck_out("heli_wreck", [("body", "root", (0, 0.6, 1.0)), ("rotor", "body", (0, 0.6, 2.1)),
                             ("tail_rotor", "body", (0.2, -4.45, 1.85))], heli_parts, {
        "body": [(0, {}), (0.6, {"r": (0.2, 2.5, 0.4)}), (1.1, {"l": (0, -0.3, 0), "r": (0.35, 4.2, 1.35)})],
        "rotor": [(0, {}), (0.4, {"l": (0, 2.5, 0), "r": (0, 5.0, 0.8)}), (0.9, {"l": (4.0, 1.0, 1.0), "r": (0.6, 9.0, 1.4)}),
                  (1.3, {"l": (5.0, -1.9, 1.5), "r": (0.9, 10.0, 1.5)})],
        "tail_rotor": [(0, {}), (0.5, {"l": (1.0, 0.5, 0)}), (1.0, {"l": (2.0, -1.5, 0)})],
    }, 80.0)

    # The plane: nose into the ground and over; the propeller flies on.
    def plane_parts(p):
        p.as_bone = "body"
        plane(p, "body", burnt)
    wreck_out("plane_wreck", [("body", "root", (0, 0, 0.75)), ("prop", "body", (0, 3.1, 0.75))], plane_parts, {
        "body": [(0, {}), (0.5, {"r": (-0.5, 0.3, 0.9)}), (1.0, {"l": (0, -0.1, 0), "r": (-0.25, 0.5, 2.6)})],
        "prop": [(0, {}), (0.3, {"l": (0, 0.5, -1.5)}), (0.8, {"l": (0, -0.6, -3.0)})],
    }, 85.0)

    # The bomber: belly down, a wing dropped, both propellers off.
    def bomber_parts(p):
        p.as_bone = "body"
        bomber(p, "body", burnt)
    wreck_out("bomber_wreck", [("body", "root", (0, 0, 1.3)), ("prop_L", "body", (-3.2, 2.95, 1.35)),
                               ("prop_R", "body", (3.2, 2.95, 1.35))], bomber_parts, {
        "body": [(0, {}), (0.5, {"r": (-0.2, 0.1, -0.5)}), (1.0, {"l": (0, -0.4, 0), "r": (-0.1, 0.2, -0.35)})],
        "prop_L": [(0, {}), (0.3, {"l": (-1.0, 0.6, -1.0)}), (0.8, {"l": (-2.5, -0.6, -2.0)})],
        "prop_R": [(0, {}), (0.4, {"l": (1.2, 0.8, -0.8)}), (0.9, {"l": (2.8, -0.6, -1.6)})],
    }, 90.0)


if __name__ == "__main__":
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = d.FPS
    bpy.context.preferences.edit.keyframe_new_interpolation_type = "LINEAR"
    os.makedirs(OUT, exist_ok=True)
    d.mats()
    for mname, rgb in [("khaki", (0.42, 0.38, 0.26)), ("skin", (0.72, 0.54, 0.42)),
                       ("team_red", (0.62, 0.10, 0.08)), ("team_blue", (0.10, 0.20, 0.62)),
                       ("tire", (0.05, 0.05, 0.05)), ("dark_olive", (0.17, 0.21, 0.12)),
                       ("glass", (0.45, 0.6, 0.7)), ("rotor", (0.1, 0.1, 0.11)),
                       ("burnt", (0.06, 0.05, 0.05))]:
        d.M[mname] = d.material(mname, rgb)
    d.M["window"] = d.material("window", (0.55, 0.75, 0.85), rough=0.1, alpha=0.3)
    d.fp_arms(front_guns, front_clips)
    held_guns()
    soldier("soldier_red", "team_red", 8.0)
    soldier("soldier_blue", "team_blue", 10.0)
    machines()
    os.makedirs(os.path.dirname(BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    print("front models written to", os.path.normpath(OUT))
