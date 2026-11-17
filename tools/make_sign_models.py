# Builds the signs of mods/building, one set per wood, and leaves the work in
# assets/blender/signs.blend.
#
#   flatpak run org.blender.Blender --background --python tools/make_sign_models.py [-- PREVIEW.png]
#
# Three shapes, as the mod has three blocks a wood:
#
#   sign_<wood>_wall.gltf    a board on a wall: against the back of its cell,
#                            its front out of the face it was put on.
#   sign_<wood>_stand.gltf   a board on a post, square to the cell.
#   sign_<wood>_diag.gltf    the same turned an eighth of a turn: its front
#                            towards +X+Z at rest. With the four quarter turns
#                            of an upright block, the two make the eight ways a
#                            standing sign can face.
#   sign_<wood>_hang.gltf    a board hanging from a ceiling on two chains,
#   sign_<wood>_hang_diag.gltf  square and an eighth round, as the standing ones.
#
# A board is the wood's colour (the average of its planks' picture), framed by a
# darker rim standing a hair proud of the face, inside which mods/building
# writes the sign's words in the world (core.render.text): the face is where
# the BOARD_* numbers below say, and the mod uses the same numbers.
#
# Authored in the unit cube in the game's axes -- x across, u up, d the depth
# with the front at d = 1 -- and put into Blender's axes by `at`.
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "mods", "building")
BLEND = os.path.join(ROOT, "assets", "blender", "signs.blend")

# The planks' average colours (assets/textures: timber_planks.png and woods_*_planks.png).
WOODS = {
    "oak": (0.548, 0.445, 0.273), "birch": (0.673, 0.604, 0.404), "spruce": (0.391, 0.288, 0.171),
    "jungle": (0.55, 0.398, 0.274), "acacia": (0.584, 0.315, 0.171), "dark_oak": (0.225, 0.146, 0.067),
}

# In 16ths of a node, as mods/building's boxes are.
BOARD_W = (1, 15)          # across
STAND_U = (8, 16)          # a standing board, up
WALL_U = (4, 13)           # a wall board, up
HANG_U = (6, 14)           # a hanging board, up; its chains go on to the ceiling
THICK = 1                  # a board's thickness
RIM = 0.55                 # the rim's width, 16ths
PROUD = 0.12               # how far the rim stands out of the face, 16ths
IN = 0.006                 # a part stands this far back from a face it would share (no z-fighting)


def material(name, rgb, rough=0.85):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Metallic"].default_value = 0.0
    bsdf.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1.0)
    return m


def at(x, u, d):
    """The game's cell axes to Blender's: glTF (x, y up, z front) is Blender (x, -y, z)."""
    return Vector((x, -d, u))


class Model:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.mats = []

    def box(self, x0, u0, d0, x1, u1, d1, mat):
        r = bmesh.ops.create_cube(self.bm, size=1.0)
        lo, hi = at(x0, u0, d1), at(x1, u1, d0)
        for v in r["verts"]:
            v.co = Vector((lo.x if v.co.x < 0 else hi.x, lo.y if v.co.y < 0 else hi.y,
                           lo.z if v.co.z < 0 else hi.z))
        if mat not in self.mats:
            self.mats.append(mat)
        for f in {f for v in r["verts"] for f in v.link_faces}:
            f.material_index = self.mats.index(mat)

    def turn(self, degrees):
        """About the cell's upright middle; positive turns the front (+Z) towards +X."""
        centre = at(0.5, 0, 0.5)
        rot = Matrix.Rotation(math.radians(degrees), 4, "Z")
        bmesh.ops.transform(self.bm, matrix=Matrix.Translation(centre) @ rot @ Matrix.Translation(-centre),
                            verts=self.bm.verts)

    def object(self):
        mesh = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for m in self.mats:
            mesh.materials.append(m)
        return bpy.data.objects.new(self.name, mesh)


def board(m, u0, u1, d_back, mats):
    """A board from u0 to u1 (16ths), its back at d_back (nodes), its rim on the front."""
    s = 1 / 16
    x0, x1 = BOARD_W[0] * s, BOARD_W[1] * s
    a, b = u0 * s, u1 * s
    front = d_back + THICK * s
    m.box(x0, a, d_back, x1, b, front, mats["board"])
    r, p = RIM * s, PROUD * s
    m.box(x0 + IN, a + IN, front - IN, x1 - IN, a + r, front + p, mats["rim"])          # bottom
    m.box(x0 + IN, b - r, front - IN, x1 - IN, b - IN, front + p, mats["rim"])          # top
    m.box(x0 + IN, a + r, front - IN, x0 + r, b - r, front + p, mats["rim"])            # left
    m.box(x1 - r, a + r, front - IN, x1 - IN, b - r, front + p, mats["rim"])            # right


def stand(m, mats):
    s = 1 / 16
    board(m, STAND_U[0], STAND_U[1], 0.5 - THICK * s / 2, mats)
    m.box(7.5 * s, 0.0, 7.5 * s, 8.5 * s, STAND_U[0] * s + IN, 8.5 * s, mats["post"])


def hang(m, mats):
    """A board under a ceiling, on two chains: links of iron up from its top."""
    s = 1 / 16
    board(m, HANG_U[0], HANG_U[1], 0.5 - THICK * s / 2, mats)
    for x in (3.5, 12.5):
        for k in range(2):
            u0 = HANG_U[1] * s + IN + k * 1.0 * s
            # a link across, the next along: a chain seen from any side
            if k % 2 == 0:
                m.box(x * s - 0.5 * s, u0, 7.75 * s, x * s + 0.5 * s, u0 + 1.0 * s, 8.25 * s, mats["chain"])
            else:
                m.box(x * s - 0.25 * s, u0, 7.5 * s, x * s + 0.25 * s, u0 + 1.0 * s, 8.5 * s, mats["chain"])


def build(wood, rgb):
    # A board lit in the world reads darker than the planks' picture: lighter
    # by a quarter, so dark ink on it is read (and light ink on the two dark woods).
    rgb = tuple(min(1.0, c * 1.25) for c in rgb)
    dark = tuple(c * 0.62 for c in rgb)
    mats = {"board": material(f"{wood}_board", rgb), "rim": material(f"{wood}_rim", dark),
            "post": material(f"{wood}_post", tuple(c * 0.78 for c in rgb)),
            "chain": material("chain_iron", (0.32, 0.32, 0.35), rough=0.5)}
    made = {}
    m = Model(f"sign_{wood}_wall")
    board(m, WALL_U[0], WALL_U[1], 0.0, mats)
    made["wall"] = m.object()
    m = Model(f"sign_{wood}_stand")
    stand(m, mats)
    made["stand"] = m.object()
    m = Model(f"sign_{wood}_diag")
    stand(m, mats)
    m.turn(45)
    made["diag"] = m.object()
    m = Model(f"sign_{wood}_hang")
    hang(m, mats)
    made["hang"] = m.object()
    m = Model(f"sign_{wood}_hang_diag")
    hang(m, mats)
    m.turn(45)
    made["hang_diag"] = m.object()
    return made


def export(ob, path):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    kept = ob.location.copy()
    ob.location = (0, 0, 0)
    bpy.context.view_layer.update()
    ob.select_set(True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_yup=True, export_apply=False,
                              export_materials="EXPORT", export_image_format="NONE")
    ob.location = kept


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    coll = bpy.data.collections.new("Signs")
    bpy.context.scene.collection.children.link(coll)
    objs = []
    for i, (wood, rgb) in enumerate(WOODS.items()):
        for j, (shape, ob) in enumerate(build(wood, rgb).items()):
            coll.objects.link(ob)
            export(ob, os.path.join(OUT, f"sign_{wood}_{shape}.gltf"))
            ob.location = (j * 1.4, -i * 1.4, 0)
            objs.append(ob)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if args:
        preview(args[0])
    print(f"{len(objs)} sign models in mods/building, the work in {BLEND}")


def preview(path):
    sc = bpy.context.scene
    cam = bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    mid = Vector((1.9, -3.5, 0.5))
    cam.location = mid + Vector((-3.0, -9.5, 7.5))
    cam.rotation_euler = (mid - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 40
    sun = bpy.data.objects.new("PreviewSun", bpy.data.lights.new("PreviewSun", "SUN"))
    sc.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(50), math.radians(-10), math.radians(-30))
    sun.data.energy = 3.0
    w = bpy.data.worlds.new("PreviewWorld")
    sc.world = w
    w.use_nodes = True
    bg = next(n for n in w.node_tree.nodes if n.type == "BACKGROUND")
    bg.inputs[0].default_value = (0.35, 0.38, 0.45, 1)
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            sc.render.engine = eng
            break
        except TypeError:
            continue
    sc.render.resolution_x, sc.render.resolution_y = 1000, 900
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


main()
