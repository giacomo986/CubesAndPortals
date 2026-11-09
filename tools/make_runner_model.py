# The runner: a zombie that sprints, climbs and dives at you (mods/mobs).
#
# Made from the zombie in assets/blender/mobs.blend: its own copy of the rig and
# the mesh, its own colours, a sprint in place of "run" and a "lunge" for the
# dive. The rest of its clips are the zombie's. The file is saved with it.
#
#   flatpak run --command=blender org.blender.Blender assets/blender/mobs.blend \
#       --background --python "$PWD/tools/make_runner_model.py"
import importlib.util
import math
import os
import sys

import bpy
from mathutils import Euler, Quaternion, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "mobs")

# Made again from nothing: a run of this script leaves no second copy behind.
for n in ("Runner_Rig", "Runner_Mesh"):
    if n in bpy.data.objects:
        bpy.data.objects.remove(bpy.data.objects[n])
for block in (bpy.data.actions, bpy.data.materials):
    for item in list(block):
        if item.name.lower().startswith("runner_"):
            block.remove(item)

zr, zm = bpy.data.objects["Zombie_Rig"], bpy.data.objects["Zombie_Mesh"]
rig = zr.copy()
rig.data = zr.data.copy()
rig.name = "Runner_Rig"
mesh = zm.copy()
mesh.data = zm.data.copy()
mesh.name = "Runner_Mesh"
mesh.parent = rig
# Setting the parent drops this, and the rig is scaled: without it the mesh comes
# out a third bigger than the bones it hangs on and every joint bends off its pivot.
mesh.matrix_parent_inverse = zm.matrix_parent_inverse.copy()
for m in mesh.modifiers:
    if m.type == "ARMATURE":
        m.object = rig
for c in zr.users_collection:
    c.objects.link(rig)
    c.objects.link(mesh)

# Greyer skin, a shirt the colour of old blood, black trousers, pale eyes.
COLOURS = {"z_skin": (0.50, 0.50, 0.46), "z_skin_dark": (0.34, 0.33, 0.31), "z_eye": (0.95, 0.90, 0.35),
           "z_shirt": (0.42, 0.09, 0.08), "z_shirt_dark": (0.27, 0.05, 0.05), "z_pants": (0.10, 0.10, 0.12),
           "z_pants_dark": (0.06, 0.06, 0.08), "z_hair": (0.05, 0.05, 0.05)}
for i, mat in enumerate(mesh.data.materials):
    new = mat.copy()
    new.name = "runner_" + mat.name[2:]
    if mat.name in COLOURS:
        bsdf = next(n for n in new.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
        bsdf.inputs[0].default_value = (*COLOURS[mat.name], 1.0)
    mesh.data.materials[i] = new

SIDES = ("L", "R")
pb = rig.pose.bones
for b in pb:
    b.location = (0.0, 0.0, 0.0)   # whatever the zombie was left posed at


def basis(name, q_armature):
    """A rotation given in the armature's axes, as the bone's own."""
    rest = rig.data.bones[name].matrix_local.to_quaternion()
    return rest.inverted() @ q_armature @ rest


def key(action_name, length, pose):
    """An action from pose(t) -> {bone: rotation in the armature's axes}, t in 0..1."""
    action = bpy.data.actions.new(action_name)
    rig.animation_data.action = action
    for f in range(0, length + 1, 2):
        for name, q in pose(f / length).items():
            pb[name].rotation_quaternion = basis(name, q)
            pb[name].keyframe_insert("rotation_quaternion", frame=f, group=name)
    rig.animation_data.action = None
    return action


def rot_x(deg):
    return Euler((math.radians(deg), 0.0, 0.0)).to_quaternion()


def aim(name, want, parent_q):
    """What turns this bone, under a parent turned by parent_q, to point along `want`."""
    b = rig.data.bones[name]
    v = (b.tail_local - b.head_local).normalized()
    return parent_q.inverted() @ v.rotation_difference(Vector(want).normalized())


def sprint(t):
    s = math.sin(t * math.tau)
    out = {"hips": rot_x(-16.0), "spine": rot_x(-6.0), "chest": rot_x(-4.0), "neck": rot_x(14.0), "head": rot_x(10.0)}
    for side, sign in zip(SIDES, (1.0, -1.0)):
        swing = s * sign
        out[f"thigh_{side}"] = rot_x(16.0 + 52.0 * swing)
        out[f"shin_{side}"] = rot_x(-(38.0 + 38.0 * max(0.0, -swing + 0.2)))
        out[f"foot_{side}"] = rot_x(10.0)
        out[f"upper_arm_{side}"] = rot_x(10.0 - 60.0 * swing)
        out[f"forearm_{side}"] = rot_x(70.0)
    return out


def lunge(t):
    # Flat out through the air, hands first, the legs trailing.
    flail = math.sin(t * math.tau) * 0.10
    body = rot_x(-72.0)
    out = {"hips": body, "spine": rot_x(4.0), "chest": rot_x(4.0), "neck": rot_x(24.0), "head": rot_x(10.0)}
    chest = body @ rot_x(8.0)
    for side, sign in zip(SIDES, (-1.0, 1.0)):   # outwards: L is at -X
        out[f"upper_arm_{side}"] = aim(f"upper_arm_{side}", (sign * (0.16 + flail), 1.0, 0.04 + flail), chest)
        out[f"forearm_{side}"] = Quaternion()
        out[f"hand_{side}"] = Quaternion()
        leg = 0.45 if side == "L" else 0.25
        out[f"thigh_{side}"] = aim(f"thigh_{side}", (sign * 0.16, -1.0, -leg - flail), body)
        out[f"shin_{side}"] = rot_x(-24.0)
        out[f"foot_{side}"] = rot_x(-34.0)
    return out


ad = rig.animation_data
for t in list(ad.nla_tracks):
    if t.name == "run":
        ad.nla_tracks.remove(t)
for name, length, pose in (("run", 16, sprint), ("lunge", 16, lunge)):
    action = key(f"runner_{name}", length, pose)
    track = ad.nla_tracks.new()
    track.name = name
    track.strips.new(name, 0, action)

for b in pb:
    b.rotation_quaternion = Quaternion()
    b.location = (0.0, 0.0, 0.0)

spec = importlib.util.spec_from_file_location("make_pet_models", os.path.join(ROOT, "tools", "make_pet_models.py"))
pets = importlib.util.module_from_spec(spec)
sys.modules["make_pet_models"] = pets
spec.loader.exec_module(pets)
pets.tip_over(rig, [mesh])

ad.action = None
for t in ad.nla_tracks:
    t.mute = False
for o in bpy.context.view_layer.objects:
    o.select_set(False)
saved = rig.location.copy()
rig.location = (0.0, 0.0, 0.0)
bpy.context.view_layer.update()
for o in (rig, mesh):
    o.hide_set(False)
    o.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, "runner.gltf"), export_format="GLTF_SEPARATE", use_selection=True,
                          export_animations=True, export_animation_mode="NLA_TRACKS",
                          export_force_sampling=True, export_optimize_animation_size=False,
                          export_yup=True, export_apply=False, export_materials="EXPORT",
                          export_image_format="NONE", export_skins=True, export_def_bones=False)
rig.location = saved
bpy.ops.wm.save_mainfile()
print("runner: tracks", [t.name for t in ad.nla_tracks])
