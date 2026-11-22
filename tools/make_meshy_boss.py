# The arpg's two big creatures from Meshy, each on the skeleton Meshy's
# auto-rig gave it (assets/meshy/<name>/walk.glb: the textured body, its
# bones and a walk; run.glb: a run), with the rest of its clips made here:
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_meshy_boss.py" -- butcher|custode
#
#   butcher  games/arpg/mods/arpg_core/butcher.gltf: a hulking brute 2.9 tall
#            with the cleaver of tools/make_arpg_gear_meshy.py in its right
#            fist; idle, walk, run, attack (a chop from over the shoulder),
#            hurt, death
#   custode  games/arpg/mods/arpg_crypt/custode.gltf: the crypt's lich, 2.6
#            tall, a staff with a blue stone in its right hand; idle, walk,
#            run, shoot (the staff thrust at the foe), attack, hurt, death
#
# A creature is drawn front +Y, feet on z = 0, so the body is turned half round
# (Meshy's faces -Y) and sized; Meshy's clips carry translations in its own
# centimetres, scaled with it. The body is welded, decimated to TRIS, unwrapped
# afresh and its colour baked from the untouched one onto TEX px, reduced to
# COLOURS (tools/meshy_prop.py): every file under the 1 MB a media file may
# weigh. A pose is written as turns about the WORLD's axes at rest (x the
# creature's left to right, y back to front, z up), each bone's turned into
# its own: Meshy's bones point every which way.
#
# A look: ~/.cache/cnp_look/<name>_clips.png, a frame of each clip.
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from meshy_prop import bake  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
LOOK = os.path.expanduser("~/.cache/cnp_look")
FPS = 30
TRIS, TEX, COLOURS = 6000, 512, 64
D = math.radians

BOSSES = {
    "butcher": {"height": 2.9, "out": ("games", "arpg", "mods", "arpg_core"), "held": "cleaver"},
    "custode": {"height": 2.6, "out": ("games", "arpg", "mods", "arpg_crypt"), "held": "staff"},
}


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


def fcurves(act):
    """An action's curves, of every layer and slot (Blender 4.4's layered actions)."""
    if hasattr(act, "fcurves"):
        return list(act.fcurves)
    return [fc for layer in act.layers for strip in layer.strips for bag in strip.channelbags for fc in bag.fcurves]


def select(objs, active=None):
    for o in bpy.data.objects:
        o.select_set(o in objs)
    bpy.context.view_layer.objects.active = active or objs[0]


def load(name, height):
    src = os.path.join(ROOT, "assets", "meshy", name)
    objs = import_glb(os.path.join(src, "walk.glb"))
    arm = next(o for o in objs if o.type == "ARMATURE")
    body = next(o for o in objs if o.type == "MESH" and o.vertex_groups)
    for o in objs:
        if o not in (arm, body):
            bpy.data.objects.remove(o)
    walk = arm.animation_data.action
    walk.name = "walk"
    run_objs = import_glb(os.path.join(src, "run.glb"))
    run_arm = next(o for o in run_objs if o.type == "ARMATURE")
    run = run_arm.animation_data.action
    run.name = "run"
    run.use_fake_user = True
    for o in run_objs:
        bpy.data.objects.remove(o)
    arm.animation_data.action = None

    # Turned half round and sized: the transform put into the bones and the body.
    k = height / body.dimensions.z
    arm.rotation_mode = "XYZ"   # the importer leaves it in quaternions, where rotation_euler is not read
    arm.rotation_euler.z += math.pi
    arm.scale *= k
    select([arm])
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    select([body])
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    lo = min((body.matrix_world @ v.co).z for v in body.data.vertices)
    # A bone's translation was in the armature's units, centimetres (its
    # object was scaled 0.01), and is now in metres of the sized body.
    f = k * 0.01
    for act in (walk, run):
        for fc in fcurves(act):
            if fc.data_path.endswith(".location"):
                for kp in fc.keyframe_points:
                    for pt in (kp.co, kp.handle_left, kp.handle_right):
                        pt.y *= f
    return arm, body, walk, run, lo


def hang_matrix(arm, hang):
    """Rest to posed, armature space, of the right hand with the arms hanging
    as in `hang` (the idle's): what is held is placed as it should look then."""
    P = Poser(arm)
    for pb in arm.pose.bones:   # nothing of the last frame looked at (the walk's) left in the bones
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)
    for b, turns in hang.items():
        arm.pose.bones[b].rotation_quaternion = P.local(b, turns)
    bpy.context.view_layer.update()
    m = arm.pose.bones["RightHand"].matrix @ arm.data.bones["RightHand"].matrix_local.inverted()
    for pb in arm.pose.bones:
        pb.rotation_quaternion = (1, 0, 0, 0)
    bpy.context.view_layer.update()
    return m


def held_part(kind, arm, hang):
    """The thing in the right fist, as a mesh bound wholly to RightHand."""
    hand = arm.data.bones["RightHand"]
    fore = arm.data.bones["RightForeArm"]
    along = (hand.head_local - fore.head_local).normalized()   # down the forearm, out of the fist
    pose = hang_matrix(arm, hang)
    grip = pose @ (hand.head_local + along * 0.09)   # where the fist is, hanging
    if kind == "cleaver":
        objs = import_glb(os.path.join(ROOT, "assets", "meshy", "arpg", "cleaver.glb"))
        ob = next(o for o in objs if o.type == "MESH")
        L = 1.05   # a metre long in its file
        # Hanging, it points ahead of the fist and down, the edge down: its z
        # (along) -> y, its x (to the edge) -> -z, its y (thickness) -> -x;
        # gripped a sixth of the way up, tipped 35 degrees down.
        turn = Matrix(((0, -1, 0, 0), (0, 0, 1, 0), (-1, 0, 0, 0), (0, 0, 0, 1)))
        placed = (Matrix.Translation(grip) @ Matrix.Rotation(D(-35), 4, "X") @ turn @ Matrix.Scale(L, 4) @
                  Matrix.Translation((0, 0, -0.16)))
    else:
        bm = bmesh.new()
        bmesh.ops.create_cone(bm, cap_ends=True, segments=8, radius1=0.035, radius2=0.03, depth=2.0)
        top = bmesh.ops.create_icosphere(bm, subdivisions=2, radius=0.09)
        for v in top["verts"]:
            v.co.z += 1.05
        me = bpy.data.meshes.new("staff")
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new("staff", me)
        bpy.context.scene.collection.objects.link(ob)
        me.materials.append(plain_material("staff_wood", (0.25, 0.16, 0.08)))
        me.materials.append(plain_material("staff_stone", (0.35, 0.7, 1.0)))
        for p in me.polygons:
            p.material_index = 1 if p.center.z > 0.98 else 0
        objs = [ob]
        # Hanging, upright beside the body, two thirds of it above the fist.
        placed = Matrix.Translation(grip + Vector((0, 0.02, 0.33)))
    ob.data.transform(pose.inverted() @ placed)
    for o in objs:
        if o is not ob:
            bpy.data.objects.remove(o)
    ob.parent = None
    g = ob.vertex_groups.new(name="RightHand")
    g.add([v.index for v in ob.data.vertices], 1.0, "REPLACE")
    return ob


def plain_material(name, rgb):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*rgb, 1)
    return m


def low_poly(body, tris):
    low = body.copy()
    low.data = body.data.copy()
    low.name = body.name + "_low"
    bpy.context.scene.collection.objects.link(low)
    bm = bmesh.new()
    bm.from_mesh(low.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    bm.to_mesh(low.data)
    bm.free()
    have = sum(len(p.vertices) - 2 for p in low.data.polygons)
    for m in list(low.modifiers):
        low.modifiers.remove(m)
    if have > tris:
        m = low.modifiers.new("dec", "DECIMATE")
        m.ratio = tris / have
        select([low])
        bpy.ops.object.modifier_apply(modifier=m.name)
    bm = bmesh.new()
    bm.from_mesh(low.data)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.to_mesh(low.data)
    bm.free()
    for p in low.data.polygons:
        p.use_smooth = True
    return low


def texture(img, colours, path):
    """The picture reduced to `colours` flat colours (k-means of a sample, every
    pixel then given its nearest, a slice at a time), and saved."""
    px = np.array(img.pixels[:], dtype=np.float32).reshape(-1, 4)
    rgb = px[:, :3]
    rng = np.random.default_rng(1)
    sample = rgb[rng.choice(len(rgb), min(len(rgb), 20000), replace=False)]
    c = sample[rng.choice(len(sample), colours, replace=False)].copy()
    for _ in range(14):
        lab = ((sample[:, None, :] - c[None]) ** 2).sum(2).argmin(1)
        for j in range(colours):
            if (lab == j).any():
                c[j] = sample[lab == j].mean(0)
    for a in range(0, len(rgb), 32768):
        part = rgb[a:a + 32768]
        rgb[a:a + 32768] = c[((part[:, None, :] - c[None]) ** 2).sum(2).argmin(1)]
    px[:, 3] = 1.0
    img.pixels = px.ravel().tolist()
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()


# ------------------------------------------------------------------ poses

class Poser:
    """Keys in world turns at rest: pose(t, {bone: (axis, degrees) or [..]}, loc={bone: (x, y, z)})."""

    def __init__(self, arm):
        self.arm = arm
        for pb in arm.pose.bones:
            pb.rotation_mode = "QUATERNION"

    def local(self, bone, turns):
        rest = self.arm.data.bones[bone].matrix_local.to_quaternion()
        q = Quaternion()
        for axis, deg in turns:
            q = Quaternion(Vector({"x": (1, 0, 0), "y": (0, 1, 0), "z": (0, 0, 1)}[axis]), D(deg)) @ q
        return rest.inverted() @ q @ rest

    def clip(self, name, seconds, keys, loop=False):
        """keys: [(t 0..1, {bone: [(axis, deg), ...]}, {bone: (x, y, z) world metres})]"""
        act = bpy.data.actions.new(name)
        act.use_fake_user = True
        self.arm.animation_data_create()
        self.arm.animation_data.action = act
        frames = max(1, round(seconds * FPS))
        bones = [pb.name for pb in self.arm.pose.bones]   # every one, or the last pose left in a bone is kept
        if loop:
            keys = list(keys) + [(1.0, keys[0][1], keys[0][2])]
        for t, rots, locs in keys:
            f = 1 + t * frames
            for b in bones:
                pb = self.arm.pose.bones[b]
                pb.rotation_quaternion = self.local(b, rots.get(b, []))
                rest = self.arm.data.bones[b].matrix_local.to_3x3()
                pb.location = rest.inverted() @ Vector(locs.get(b, (0, 0, 0)))
                pb.keyframe_insert("rotation_quaternion", frame=f)
                pb.keyframe_insert("location", frame=f)
        self.arm.animation_data.action = None
        return act


def merge(*poses):
    out = {}
    for p in poses:
        for b, turns in p.items():
            out[b] = out.get(b, []) + list(turns)
    return out


# How each one's arms hang at rest (the idle's, and what is held is placed for).
HANG = {
    "butcher": {"RightArm": [("y", 72)], "LeftArm": [("y", -72)], "RightForeArm": [("z", 25)],
                "LeftForeArm": [("z", -15)]},
    "custode": {"RightArm": [("y", 62), ("z", 25)], "RightForeArm": [("z", 55)], "LeftArm": [("y", -70)],
                "LeftForeArm": [("z", -20)]},
}


def butcher_clips(P):
    hang = HANG["butcher"]
    clips = []
    breathe = merge(hang, {"Spine01": [("x", -4)]})
    out = merge(hang, {"Spine01": [("x", -9)], "Head": [("x", 5)]})
    clips.append(P.clip("idle", 2.6, [(0.0, breathe, {}), (0.5, out, {})], loop=True))
    # The chop: the cleaver up over the right shoulder, the body wound back,
    # then down across in front, the body thrown after it.
    wind = merge({"RightArm": [("y", -55), ("z", -20)], "RightForeArm": [("z", 60)], "LeftArm": [("y", -60)],
                  "Spine01": [("z", 22), ("x", 10)], "Head": [("z", -10)]})
    hit = merge({"RightArm": [("y", 75), ("z", 50)], "RightForeArm": [("z", 5)], "LeftArm": [("y", -75), ("z", -20)],
                 "Spine01": [("z", -25), ("x", -28)], "Head": [("x", 10)]})
    clips.append(P.clip("attack", 1.0, [(0.0, hang, {}), (0.38, wind, {}), (0.55, hit, {}), (0.72, hit, {}),
                                        (1.0, hang, {})]))
    flinch = merge(hang, {"Spine01": [("x", 14)], "Head": [("x", 18)], "RightArm": [("y", -10)]})
    clips.append(P.clip("hurt", 0.35, [(0.0, hang, {}), (0.35, flinch, {}), (1.0, hang, {})]))
    clips.append(death(P, hang))
    return clips


def custode_clips(P):
    hang = HANG["custode"]
    clips = []
    a = merge(hang, {"Spine01": [("x", -3)], "Head": [("z", 6)]})
    b = merge(hang, {"Spine01": [("x", -7)], "Head": [("z", -6), ("x", -5)]})
    clips.append(P.clip("idle", 3.0, [(0.0, a, {}), (0.5, b, {})], loop=True))
    # The staff thrust at the foe, the free hand raised.
    raise_ = merge({"RightArm": [("y", 20), ("z", 60)], "RightForeArm": [("z", 20)], "LeftArm": [("y", -10), ("z", -40)],
                    "Spine01": [("x", 8)], "Head": [("x", 8)]})
    thrust = merge({"RightArm": [("y", 10), ("z", 85)], "RightForeArm": [("z", 0)], "LeftArm": [("y", -20), ("z", -70)],
                    "Spine01": [("x", -14)]})
    clips.append(P.clip("shoot", 1.2, [(0.0, hang, {}), (0.4, raise_, {}), (0.6, thrust, {}), (1.0, hang, {})]))
    swing = merge({"RightArm": [("y", -40)], "RightForeArm": [("z", 40)], "Spine01": [("z", 20)]})
    down = merge({"RightArm": [("y", 70), ("z", 45)], "Spine01": [("z", -15), ("x", -18)]})
    clips.append(P.clip("attack", 0.9, [(0.0, hang, {}), (0.35, swing, {}), (0.6, down, {}), (1.0, hang, {})]))
    flinch = merge(hang, {"Spine01": [("x", 12)], "Head": [("x", 20)]})
    clips.append(P.clip("hurt", 0.35, [(0.0, hang, {}), (0.35, flinch, {}), (1.0, hang, {})]))
    clips.append(death(P, hang))
    return clips


def death(P, hang):
    """It reels, drops to its knees and falls on its face; the last key held."""
    reel = merge(hang, {"Spine01": [("x", 18)], "Head": [("x", 25)], "RightArm": [("y", -20)],
                        "LeftArm": [("y", 20)]})
    knees = merge(hang, {"LeftUpLeg": [("x", 15)], "RightUpLeg": [("x", 15)], "LeftLeg": [("x", -100)],
                         "RightLeg": [("x", -100)], "Spine01": [("x", -10)], "Head": [("x", -20)]})
    # Face down: the hips turned forward, the thighs turned back as much
    # under them (a child's turn is after its parent's) so the knees stay put.
    face = merge(hang, {"Hips": [("x", -70)], "LeftUpLeg": [("x", 85)], "RightUpLeg": [("x", 85)],
                        "LeftLeg": [("x", -110)], "RightLeg": [("x", -110)], "Head": [("x", 30)],
                        "RightArm": [("y", 30)], "LeftArm": [("y", -30)]})
    h = P.arm.data.bones["Hips"].head_local.z
    return P.clip("death", 1.6, [(0.0, hang, {}), (0.2, reel, {"Hips": (0, -0.1, 0)}),
                                 (0.55, knees, {"Hips": (0, 0.1, -h * 0.45)}),
                                 (0.9, face, {"Hips": (0, 0.45, -h * 0.72)}),
                                 (1.0, face, {"Hips": (0, 0.45, -h * 0.74)})])


def nla(arm, acts):
    arm.animation_data_create()
    for act in acts:
        tr = arm.animation_data.nla_tracks.new()
        tr.name = act.name
        st = tr.strips.new(act.name, int(act.frame_range[0]), act)
        if hasattr(st, "action_slot") and len(act.slots):
            st.action_slot = act.slots[0]


def look(name, arm, mesh, acts):
    scene = bpy.context.scene
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    scene.render.engine = next((e for e in engines if "EEVEE" in e), "CYCLES")
    world = bpy.data.worlds.new("w")
    world.color = (0.4, 0.4, 0.42)
    scene.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (0.8, 0.1, 0.5)
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.location = (3.6, 4.6, 2.8)
    cam.rotation_euler = (Vector((0, 0, 1.1)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    S = 300
    scene.render.resolution_x = scene.render.resolution_y = S
    shots = []
    for tr in arm.animation_data.nla_tracks:
        tr.mute = True
    for act in acts:
        arm.animation_data.action = act
        if hasattr(arm.animation_data, "action_slot") and len(act.slots):
            arm.animation_data.action_slot = act.slots[0]
        a, b = act.frame_range
        for f in (a + (b - a) * 0.0, a + (b - a) * 0.45, b):
            scene.frame_set(int(f))
            p = os.path.join(LOOK, f"{name}_{act.name}_{int(f)}.png")
            scene.render.filepath = p
            bpy.ops.render.render(write_still=True)
            shots.append(p)
    arm.animation_data.action = None
    cols = 3
    rows = len(acts)
    out = bpy.data.images.new("sheet", S * cols, S * rows)
    px = np.zeros((S * rows, S * cols, 4), dtype=np.float32)
    for i, p in enumerate(shots):
        im = bpy.data.images.load(p)
        a = np.array(im.pixels[:], dtype=np.float32).reshape(S, S, 4)
        r, c = rows - 1 - i // cols, i % cols
        px[r * S:(r + 1) * S, c * S:(c + 1) * S] = a
    out.pixels = px.ravel()
    out.filepath_raw = os.path.join(LOOK, f"{name}_clips.png")
    out.file_format = "PNG"
    out.save()
    print("LOOK", out.filepath_raw)


def main():
    name = sys.argv[sys.argv.index("--") + 1]
    cfg = BOSSES[name]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = FPS
    arm, body, walk, run, lo = load(name, cfg["height"])
    # feet on the floor, the bones with them
    for v in body.data.vertices:
        v.co.z -= lo
    select([arm])
    bpy.ops.object.mode_set(mode="EDIT")
    for eb in arm.data.edit_bones:
        eb.head.z -= lo
        eb.tail.z -= lo
    bpy.ops.object.mode_set(mode="OBJECT")

    held = held_part(cfg["held"], arm, HANG[name])
    # One mesh, one picture: the body and what it holds, baked together.
    high = body
    for m in list(high.modifiers):
        high.modifiers.remove(m)
    select([high, held], high)
    bpy.ops.object.join()
    low = low_poly(high, TRIS)
    out = os.path.join(ROOT, *cfg["out"])
    img = bake(high, low, name, TEX)
    os.makedirs(LOOK, exist_ok=True)
    texture(img, COLOURS, os.path.join(out, f"{name}.png"))
    low.name = low.data.name = name
    low.parent = arm
    low.matrix_parent_inverse = arm.matrix_world.inverted()
    low.modifiers.new("rig", "ARMATURE").object = arm

    for pb in arm.pose.bones:   # nothing of the last frame looked at left in the bones
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)
    P = Poser(arm)
    made = butcher_clips(P) if name == "butcher" else custode_clips(P)
    acts = [walk, run] + made
    for act in (walk, run):
        act.use_fake_user = True
    nla(arm, acts)
    select([low, arm], low)
    path = os.path.join(out, f"{name}.gltf")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True, export_yup=True,
                              export_image_format="AUTO", export_apply=False, export_animations=True,
                              export_animation_mode="NLA_TRACKS", export_force_sampling=True,
                              export_optimize_animation_size=True, export_skins=True, export_def_bones=False)
    for ext in (".gltf", ".bin", ".png"):
        size = os.path.getsize(os.path.join(out, name + ext))
        print(f"{name}{ext}: {size} bytes")
        assert size < 1 << 20, f"{name}{ext} is over 1 MB"
    print(f"{name}: {len(low.data.polygons)} triangles; clips {[a.name for a in acts]}")
    look(name, arm, low, acts)


main()
