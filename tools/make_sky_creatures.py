# The sky islands' two creatures, from Meshy, given wings that beat:
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_sky_creatures.py"
#
#   manta   games/skies/mods/skies/manta.gltf: the Cloud Manta, 12 m from
#           wing tip to wing tip; idle and walk a slow beat, run a fast one,
#           attack a dive (the body tips nose down), death (the wings fold up
#           and it rolls over).
#   squall  games/skies/mods/skies/squall.gltf: the small storm cloud it
#           calls, 1.1 m across; the same clips, quicker.
#
# The sources are tools/meshy_prop.py's output in ~/.cache/cnp_look/defsky/out
# (concept image, image-to-3D meshy-6, then meshy_prop at --height 2.5 / 0.9,
# --origin centre): rigid, so here each gets three bones -- body, wing_l,
# wing_r -- and every vertex is weighted to a wing by how far it is from the
# middle (none inside WING0 of the half span, all past WING1). Meshy draws a
# creature facing -Y; turned half round, it faces +Y like every creature, its
# lowest point on z = 0.
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import importlib.util  # noqa: E402

_spec = importlib.util.spec_from_file_location("make_pet_models", os.path.join(HERE, "make_pet_models.py"))
pets = importlib.util.module_from_spec(_spec)
sys.modules["make_pet_models"] = pets
_spec.loader.exec_module(pets)

SRC = os.path.expanduser("~/.cache/cnp_look/defsky/out")
OUT = os.path.join(HERE, "..", "games", "skies", "mods", "skies")

#        name      scale  wing0 wing1 flap  beat (s) fast (s)
KINDS = [("manta", 0.6, 0.18, 0.55, 0.30, 2.4, 1.1),
         ("squall", 0.6, 0.35, 0.75, 0.70, 0.5, 0.25)]


def build(name, scale, w0, w1, flap, beat, fast):
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.scene.render.fps = pets.FPS
    bpy.ops.import_scene.gltf(filepath=os.path.join(SRC, f"{name}.gltf"))
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    mesh = bpy.context.view_layer.objects.active
    mesh.rotation_euler.z += math.pi
    mesh.scale = [s * scale for s in mesh.scale]
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    zs = [v.co.z for v in mesh.data.vertices]
    xs = [abs(v.co.x) for v in mesh.data.vertices]
    for v in mesh.data.vertices:
        v.co.z -= min(zs)
    half = max(xs)
    mid = (max(zs) - min(zs)) / 2

    body = mesh.vertex_groups.new(name="body")
    left = mesh.vertex_groups.new(name="wing_l")
    right = mesh.vertex_groups.new(name="wing_r")
    for v in mesh.data.vertices:
        t = (abs(v.co.x) / half - w0) / (w1 - w0)
        t = max(0.0, min(1.0, t))
        t = t * t * (3 - 2 * t)
        if t < 1.0:
            body.add([v.index], 1.0 - t, "REPLACE")
        if t > 0.0:
            (left if v.co.x < 0 else right).add([v.index], t, "REPLACE")

    root = w0 * half
    arm = pets.rig(f"{name}_rig", [("body", None, (0, 0, mid)), ("wing_l", "body", (-root, 0, mid)),
                                   ("wing_r", "body", (root, 0, mid))])
    pets.skin(mesh, arm)

    # A bone points up: its own Z is the world's back, so a turn about it
    # raises a wing (wing_l by -a, wing_r by +a); its own X is the world's X,
    # so a turn about it tips the nose down (+) or up.
    def beat_keys(a, steps=8):
        out = {"wing_l": [], "wing_r": []}
        for i in range(steps):
            t = i / steps
            r = a * math.sin(t * 2 * math.pi)
            out["wing_l"].append((t, (0, 0, -r), None))
            out["wing_r"].append((t, (0, 0, r), None))
        return out

    idle = beat_keys(flap * 0.7)
    idle["body"] = [(0.0, (0, 0, 0), (0, 0, 0)), (0.25, (0, 0, 0), (0, 0, -0.05 * mid)),
                    (0.75, (0, 0, 0), (0, 0, 0.05 * mid))]
    pets.clip(arm, name, "idle", beat * 1.3, idle)
    pets.clip(arm, name, "walk", beat, beat_keys(flap))
    pets.clip(arm, name, "run", fast, beat_keys(flap * 1.2))
    attack = beat_keys(flap * 0.4)
    attack["body"] = [(0.0, (0, 0, 0), None), (0.3, (0.5, 0, 0), None), (0.7, (0.5, 0, 0), None)]
    pets.clip(arm, name, "attack", 0.9, attack)
    pets.clip(arm, name, "shoot", 0.9, attack)
    # Dying: the wings fold up over the back, then it rolls onto its side and
    # sinks; the last key holds.
    death = {"wing_l": [(0.0, (0, 0, 0), None), (0.35, (0, 0, -1.2), None), (1.0, (0, 0, -1.2), None)],
             "wing_r": [(0.0, (0, 0, 0), None), (0.35, (0, 0, 1.2), None), (1.0, (0, 0, 1.2), None)],
             "body": [(0.0, (0, 0, 0), (0, 0, 0)), (0.35, (0, 0, 0), (0, 0, 0)),
                      (1.0, (0, 0, math.pi / 2), (0, -mid * 0.8, 0))]}
    pets.clip(arm, name, "death", 1.6, death, loop=False)

    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    arm.select_set(True)
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = arm
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, f"{name}.gltf")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=True, export_animation_mode="NLA_TRACKS",
                              export_force_sampling=True, export_optimize_animation_size=False,
                              export_yup=True, export_apply=False, export_materials="EXPORT",
                              export_image_format="AUTO", export_skins=True, export_def_bones=False)
    print(f"sky creature: {name} span {2 * half:.2f} m, height {max(zs) - min(zs):.2f} m -> {path}")


for k in KINDS:
    build(*k)
