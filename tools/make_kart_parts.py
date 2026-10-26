# The track's kart, split into parts a player may swap, and the parts to swap
# them for.
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_kart_parts.py"
#
# Imports the kart as it was made (assets/blender/kart_unsplit/track_core_kart.gltf,
# the export of assets/blender/kart.blend) and gives its rig a
# bone for each part that can be replaced -- `nose` (the nose cone and the
# front bumper), `pods` (the side pods), `pipes` (the exhaust) -- with that
# part's vertices bound to it, and a `flame_1` joint at the end of the pipe,
# pointing out of it, where the client draws the nitro's flame. The four
# wheel_* bones already hold the wheels. Its colours become tint_1 (the
# bodywork), tint_2 (the nose and the bumpers) and tint_3 (the rims), so a
# player's paint (core.player_wear with the kart itself) recolours it.
#
# Then every part in PARTS, exported as kart_<part>_<variant>.gltf on the same
# rig: the client draws one worn by a driver on the kart's bones and leaves out
# the kart's own bones named <part> or <part>_* (vehicle_part in
# src/client/main.cpp) -- so kart_wheel_slick replaces the four wheels, and
# kart_wing_*, which the kart has none of, only adds. A part with flame_*
# joints of its own (the pipes) burns there instead of at the kart's.
#
# Shapes are the robot extras' (tools/make_robot_extras.py): metres, Z up, the
# kart facing +Y, the road at z = 0. Leaves the work in assets/blender/kart_parts.blend.
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_robot_extras as rx  # noqa: E402  (its shapes, materials and P)

P = rx.P
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "games", "track", "mods", "track_core")

# The kart's materials, renamed to the colours a player paints. The rest keep
# their names and their colours.
RENAME = {"Kart_Secondary": "tint_1", "Kart_Accent": "tint_2", "Kart_Rim": "tint_3"}
MATS = {
    "tint_1": ((0.05, 0.25, 0.80), 0.0, 0.4),
    "tint_2": ((0.85, 0.18, 0.03), 0.0, 0.35),
    "tint_3": ((0.70, 0.70, 0.72), 1.0, 0.25),
    "Kart_Tire": ((0.02, 0.02, 0.02), 0.0, 0.9),
    "Kart_Chrome": ((0.85, 0.86, 0.88), 1.0, 0.15),
    "Kart_Soot": ((0.03, 0.03, 0.03), 0.3, 0.7),
    "Kart_White": ((0.92, 0.92, 0.90), 0.0, 0.5),
    "Kart_Lamp": ((1.00, 0.85, 0.35), 0.0, 0.2),
}

# The bones added to the kart: name, parent, head, tail.
BONES = [
    ("nose", "body", (0, 0.6, 0.15), (0, 0.8, 0.15)),
    ("pods", "body", (0, 0.0, 0.15), (0, 0.2, 0.15)),
    ("pipes", "body", (0.3, -0.5, 0.3), (0.3, -0.6, 0.3)),
    ("flame_1", "pipes", (0.34, -0.70, 0.34), (0.34, -0.85, 0.36)),
]

# The wheels' middles, and the tyres' radius and width (the kart's own).
WHEELS = {"FL": ((-0.515, 0.55, 0.13), 0.13, 0.13), "FR": ((0.515, 0.55, 0.13), 0.13, 0.13),
          "RL": ((-0.55, -0.5, 0.14), 0.14, 0.18), "RR": ((0.55, -0.5, 0.14), 0.14, 0.18)}


def wheels(kind):
    out = []
    for w, ((x, y, z), r, wd) in WHEELS.items():
        bone, out_x = f"wheel_{w}", (1 if x > 0 else -1)
        side = (0, 90, 0)
        if kind == "slick":   # wide, smooth, a coloured band round the wall
            wd *= 1.3
            out += [P("cyl", bone, "Kart_Tire", (x, y, z), (r, r, wd), side),
                    P("cyl", bone, "tint_3", (x + out_x * wd * 0.5, y, z), (r * 0.62, r * 0.62, 0.012), side),
                    P("torus", bone, "tint_2", (x + out_x * wd * 0.5, y, z), (r * 0.8, r * 0.8, r * 0.8),
                      side, R=1.0, t=0.06)]
        elif kind == "offroad":   # knobbly, deep tread, a dished rim
            wd *= 1.15
            out += [P("cyl", bone, "Kart_Tire", (x, y, z), (r * 0.9, r * 0.9, wd), side),
                    P("cyl", bone, "tint_1", (x + out_x * wd * 0.5, y, z), (r * 0.55, r * 0.55, 0.02), side),
                    P("cyl", bone, "Kart_Soot", (x + out_x * wd * 0.52, y, z), (r * 0.15, r * 0.15, 0.03), side)]
            for i in range(12):
                a = 2 * math.pi * i / 12
                out.append(P("box", bone, "Kart_Tire", (x, y + r * 0.95 * math.cos(a), z + r * 0.95 * math.sin(a)),
                             (wd * (0.95 if i % 2 else 0.7), 0.05, 0.035), (math.degrees(a) - 90, 0, 0)))
        elif kind == "retro":   # narrow, a white wall, chrome spokes
            wd *= 0.8
            out += [P("cyl", bone, "Kart_Tire", (x, y, z), (r, r, wd), side),
                    P("torus", bone, "Kart_White", (x + out_x * wd * 0.5, y, z), (r * 0.78, r * 0.78, r * 0.78),
                      side, R=1.0, t=0.12),
                    P("cyl", bone, "Kart_Chrome", (x + out_x * wd * 0.45, y, z), (r * 0.18, r * 0.18, 0.03), side)]
            for i in range(8):
                a = 180 * i / 8
                out.append(P("box", bone, "Kart_Chrome", (x + out_x * wd * 0.45, y, z), (0.008, r * 1.25, 0.008),
                             (a, 0, 0)))
    return out


PARTS = {
    # Wheels, all four.
    "wheel_slick": wheels("slick"),
    "wheel_offroad": wheels("offroad"),
    "wheel_retro": wheels("retro"),
    # Noses, the cone and the bumper in front.
    "nose_wedge": [   # low and flat, sloping to a splitter blade
        P("box", "nose", "tint_1", (0, 0.6, 0.1), (0.62, 0.62, 0.08)),
        P("box", "nose", "tint_1", (0, 0.58, 0.17), (0.5, 0.62, 0.05), (9, 0, 0)),
        P("box", "nose", "tint_2", (0, 0.58, 0.2), (0.08, 0.6, 0.02), (9, 0, 0)),
        P("box", "nose", "tint_3", (0, 0.95, 0.05), (1.18, 0.12, 0.018)),
        P("box", "nose", "tint_3", (0.58, 0.92, 0.08), (0.02, 0.16, 0.08)),
        P("box", "nose", "tint_3", (-0.58, 0.92, 0.08), (0.02, 0.16, 0.08)),
    ],
    "nose_ram": [   # a heavy bull bar with two lamps
        P("box", "nose", "tint_1", (0, 0.6, 0.17), (0.5, 0.6, 0.2)),
        P("box", "nose", "tint_2", (0, 0.6, 0.275), (0.3, 0.5, 0.02)),
        P("cyl", "nose", "Kart_Chrome", (0, 0.96, 0.12), (0.035, 0.035, 1.24), (0, 90, 0)),
        P("cyl", "nose", "Kart_Chrome", (0, 0.96, 0.24), (0.03, 0.03, 0.9), (0, 90, 0)),
    ] + [P("cyl", "nose", "Kart_Chrome", (x, 0.93, 0.18), (0.025, 0.025, 0.16)) for x in (-0.42, -0.15, 0.15, 0.42)]
      + [P("cyl", "nose", "Kart_Lamp", (x, 0.91, 0.2), (0.055, 0.055, 0.03), (-90, 0, 0)) for x in (-0.3, 0.3)],
    "nose_shark": [   # a rounded snout with a row of teeth
        P("ball", "nose", "tint_1", (0, 0.62, 0.16), (0.3, 0.36, 0.14)),
        P("box", "nose", "tint_2", (0, 0.88, 0.09), (0.62, 0.06, 0.05)),
        P("ball", "nose", "Kart_Soot", (0.15, 0.82, 0.21), (0.035, 0.035, 0.035)),
        P("ball", "nose", "Kart_Soot", (-0.15, 0.82, 0.21), (0.035, 0.035, 0.035)),
    ] + [P("cone", "nose", "Kart_White", (x, 0.92, 0.1), (1, 1, 1), (90, 0, 0), r1=0.022, r2=0.0, d=0.05, seg=6)
         for x in (-0.24, -0.16, -0.08, 0.0, 0.08, 0.16, 0.24)],
    # Side pods.
    "pods_rocket": [   # a rocket on each side
        p for s in (-1, 1) for p in (
            P("cyl", "pods", "tint_1", (s * 0.41, 0.03, 0.15), (0.085, 0.085, 0.56), (-90, 0, 0)),
            P("cone", "pods", "tint_3", (s * 0.41, 0.39, 0.15), (1, 1, 1), (-90, 0, 0), r1=0.085, r2=0.0, d=0.16),
            P("cone", "pods", "Kart_Soot", (s * 0.41, -0.29, 0.15), (1, 1, 1), (-90, 0, 0), r1=0.06, r2=0.085,
              d=0.08),
            P("box", "pods", "tint_2", (s * 0.41, -0.16, 0.25), (0.012, 0.14, 0.1)),
        )],
    "pods_slab": [   # flat swept panels with an intake
        p for s in (-1, 1) for p in (
            P("box", "pods", "tint_1", (s * 0.4, 0.04, 0.14), (0.2, 0.62, 0.15), (0, 0, s * 3)),
            P("box", "pods", "tint_2", (s * 0.4, 0.04, 0.218), (0.21, 0.5, 0.012), (0, 0, s * 3)),
            P("box", "pods", "Kart_Soot", (s * 0.4, 0.355, 0.14), (0.15, 0.012, 0.08)),
        )],
    # Exhausts: their own flame_* joints, where the nitro burns.
    "pipes_twin": [
        P("cyl", "pipes", "Kart_Chrome", (x, -0.62, z), (0.028, 0.028, 0.3), (-90, 0, 0)) for x, z in
        ((0.22, 0.3), (0.34, 0.3))] + [
        P("torus", "pipes", "Kart_Soot", (x, -0.775, z), (0.03, 0.03, 0.03), (90, 0, 0), R=1.0, t=0.25) for x, z in
        ((0.22, 0.3), (0.34, 0.3))],
    "pipes_stack": [   # three short stacks up and back off the engine
        P("cyl", "pipes", "Kart_Chrome", (x, -0.47, 0.5), (0.022, 0.022, 0.22), (30, 0, 0))
        for x in (0.19, 0.25, 0.31)],
    "pipes_jet": [   # a turbine nozzle behind the seat
        P("cone", "pipes", "Kart_Chrome", (0, -0.66, 0.32), (1, 1, 1), (90, 0, 0), r1=0.085, r2=0.12, d=0.2),
        P("torus", "pipes", "tint_2", (0, -0.57, 0.32), (0.09, 0.09, 0.09), (90, 0, 0), R=1.0, t=0.18),
        P("cyl", "pipes", "Kart_Soot", (0, -0.755, 0.32), (0.1, 0.1, 0.02), (90, 0, 0)),
        P("box", "pipes", "Kart_Chrome", (0, -0.52, 0.25), (0.05, 0.08, 0.12)),
    ],
    # Wings, which the kart has none of.
    "wing_spoiler": [
        P("box", "body", "tint_1", (0, -0.8, 0.66), (1.0, 0.18, 0.025), (-6, 0, 0)),
        P("box", "body", "tint_2", (0.5, -0.8, 0.63), (0.02, 0.24, 0.13)),
        P("box", "body", "tint_2", (-0.5, -0.8, 0.63), (0.02, 0.24, 0.13)),
        P("box", "body", "tint_3", (0.24, -0.76, 0.43), (0.025, 0.05, 0.46)),
        P("box", "body", "tint_3", (-0.24, -0.76, 0.43), (0.025, 0.05, 0.46)),
    ],
    "wing_twin": [   # two decks
        P("box", "body", "tint_1", (0, -0.8, 0.56), (0.9, 0.16, 0.022), (-4, 0, 0)),
        P("box", "body", "tint_1", (0, -0.86, 0.72), (0.9, 0.14, 0.022), (-10, 0, 0)),
        P("box", "body", "tint_2", (0.46, -0.82, 0.64), (0.02, 0.24, 0.24)),
        P("box", "body", "tint_2", (-0.46, -0.82, 0.64), (0.02, 0.24, 0.24)),
        P("box", "body", "tint_3", (0, -0.74, 0.38), (0.04, 0.05, 0.38)),
    ],
    "wing_fin": [   # a shark's fin behind the seat
        P("cone", "body", "tint_1", (0, -0.66, 0.6), (0.3, 1.0, 1.0), (-35, 0, 0), r1=0.2, r2=0.0, d=0.5, seg=3),
        P("box", "body", "tint_2", (0, -0.6, 0.38), (0.05, 0.3, 0.06)),
    ],
}

# Where a pipes part burns: its flame_* joints, head and tail.
FLAMES = {
    "pipes_twin": [((0.22, -0.78, 0.3), (0.22, -0.93, 0.3)), ((0.34, -0.78, 0.3), (0.34, -0.93, 0.3))],
    "pipes_stack": [((x, -0.53, 0.6), (x, -0.6, 0.72)) for x in (0.19, 0.25, 0.31)],
    "pipes_jet": [((0, -0.77, 0.32), (0, -0.92, 0.32))],
}


def edit_bones(rig, add, drop=()):
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    eb = rig.data.edit_bones
    for name in drop:
        if name in eb:
            eb.remove(eb[name])
    for name, parent, head, tail in add:
        b = eb.new(name)
        b.head, b.tail = Vector(head), Vector(tail)
        b.parent = eb[parent]
    bpy.ops.object.mode_set(mode="OBJECT")


def split_kart(rig, kart):
    """Binds the nose, the pods and the pipe to their bones, island by island."""
    me = kart.data
    for b in ("nose", "pods", "pipes"):
        kart.vertex_groups.new(name=b)
    groups = {g.name: g.index for g in kart.vertex_groups}
    body = groups["body"]
    bm = bmesh.new()
    bm.from_mesh(me)
    dl = bm.verts.layers.deform.verify()
    seen = set()
    counts = {}
    for v in bm.verts:
        if v.index in seen:
            continue
        island, todo = [], [v]
        seen.add(v.index)
        while todo:
            x = todo.pop()
            island.append(x)
            for e in x.link_edges:
                y = e.other_vert(x)
                if y.index not in seen:
                    seen.add(y.index)
                    todo.append(y)
        if any(max(x[dl].items(), key=lambda t: t[1])[0] != body for x in island):
            continue   # the wheels, the knuckles, the steering wheel
        mat = me.materials[island[0].link_faces[0].material_index].name
        c = sum((x.co for x in island), Vector()) / len(island)
        part = None
        if mat in ("Kart_Accent", "Kart_Secondary") and c.y > 0.25:
            part = "nose"
        elif mat == "Kart_Secondary" and abs(c.x) > 0.3 and -0.3 < c.y < 0.4:
            part = "pods"
        elif mat == "Kart_Metal" and c.y < -0.5 and c.z > 0.27 and c.x > 0.2:
            part = "pipes"
        if part is None:
            continue
        counts[part] = counts.get(part, 0) + 1
        for x in island:
            x[dl].clear()
            x[dl][groups[part]] = 1.0
    bm.to_mesh(me)
    bm.free()
    print("kart parts (islands):", counts)


def build(rig, name, parts):
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
        verts = rx.shape(bm, part)
        if part["m"] not in used:
            used.append(part["m"])
        for v in verts:
            v[dl][groups[part["bone"]]] = 1.0
        for f in {f for v in verts for f in v.link_faces}:
            f.material_index = used.index(part["m"])
            f.smooth = part["k"] in ("ball", "torus", "cyl", "cone") and part.get("seg", 24) > 4
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    for m in used:
        me.materials.append(bpy.data.materials[m])
    ob.parent = rig
    ob.modifiers.new("rig", "ARMATURE").object = rig
    return ob


def export(rig, ob, name):
    for x in bpy.data.objects:
        x.select_set(False)
    ob.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(
        filepath=os.path.join(OUT, f"{name}.gltf"), export_format="GLTF_SEPARATE",
        use_selection=True, export_animations=False, export_skins=True,
        export_apply=True, export_yup=True)


def main():
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "kart_unsplit", "track_core_kart.gltf"))
    rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    kart = next(o for o in bpy.data.objects if o.type == "MESH" and o.parent == rig)
    rig.data.pose_position = "REST"
    edit_bones(rig, BONES)
    split_kart(rig, kart)
    for k, (rgb, metal, rough) in MATS.items():
        rx.material(k, rgb, metal, rough)
    for old, new in RENAME.items():
        kart.data.materials[kart.data.materials.find(old)] = bpy.data.materials[new]
    export(rig, kart, "track_core_kart")
    kart.hide_set(True)

    for name, parts in PARTS.items():
        flames = [(f"flame_{i + 1}", "pipes", h, t) for i, (h, t) in enumerate(FLAMES.get(name, []))]
        edit_bones(rig, flames, drop=[b.name for b in rig.data.bones if b.name.startswith("flame_")])
        ob = build(rig, f"kart_{name}", parts)
        export(rig, ob, f"kart_{name}")
        ob.hide_set(True)

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "blender", "kart_parts.blend"))


main()
