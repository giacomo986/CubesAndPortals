# The models of mods/cards, made in Blender:
#
#   table.gltf      a card table one cell across: four legs, a walnut frame and
#                   green felt whose top is at TOP. Its origin is the cell's
#                   corner, as a node's model.
#   card<n>.gltf    the forty cards face up: the face (c<n>.png, from
#                   tools/make_cards.py) on top and the back under it, CW wide
#                   and CH long, the face's head towards glTF -Z, feet at 0
#                   about their middle.
#   dim<n>.gltf     the same with the face darkened (d<n>.png): a card that
#                   may not be played now.
#   back.gltf       a card face down: the back on top.
#   ecard<n>.gltf, edim<n>.gltf, eback.gltf
#                   the same for the illustrated deck (ec<n>.png ...).
#
#   flatpak run org.blender.Blender --background --python "$PWD/tools/make_card_models.py"
#
# Blender is Z up: glTF (x, y, z) is Blender (x, -z, y) -- so the head of a
# card is Blender +Y.
import os

import bmesh
import bpy

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
MOD = os.path.join(ROOT, "mods", "cards")

TOP = 0.775    # the felt's top above the cell's floor
CW, CH, CT = 0.1, 0.16, 0.0015   # a card's width, length and thickness


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for m in list(bpy.data.meshes):
        bpy.data.meshes.remove(m)


def flat(name, rgb, rough=0.6):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*rgb, 1.0)
    return m


def printed(name, png):
    """A material that is that picture, read from the mod."""
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Roughness"].default_value = 0.7
    tex = nt.nodes.new("ShaderNodeTexImage")
    path = os.path.join(MOD, png)
    img = bpy.data.images.get(png) or bpy.data.images.load(path)
    img.name = png
    tex.image = img
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    return m


def export(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", use_selection=True,
                              export_animations=False, export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_image_format="AUTO", export_skins=False,
                              export_texture_dir="")


def card(name, top_png, back_png="back.png"):
    """A thin box: top_png on the top face, the back under it, white edges."""
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    x, y = CW / 2, CH / 2
    lo = [bm.verts.new(v) for v in ((-x, -y, 0), (x, -y, 0), (x, y, 0), (-x, y, 0))]
    hi = [bm.verts.new(v) for v in ((-x, -y, CT), (x, -y, CT), (x, y, CT), (-x, y, CT))]
    top = bm.faces.new(hi)
    top.material_index = 0
    for loop, t in zip(top.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
        loop[uv].uv = t
    bottom = bm.faces.new(list(reversed(lo)))
    bottom.material_index = 1
    # seen from below the back is mirrored left to right, as a real one is
    for loop, t in zip(bottom.loops, ((1, 1), (0, 1), (0, 0), (1, 0))):
        loop[uv].uv = t
    for i in range(4):
        j = (i + 1) % 4
        side = bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
        side.material_index = 2
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    for m in (printed("CA " + top_png, top_png), printed("CA " + back_png, back_png), flat("CA Edge", (0.95, 0.93, 0.88))):
        me.materials.append(m)
    return o


def box(lo, hi, mat):
    bpy.ops.mesh.primitive_cube_add(size=1)
    o = bpy.context.active_object
    o.scale = [h - l for l, h in zip(lo, hi)]
    o.location = [(l + h) / 2 for l, h in zip(lo, hi)]
    o.data.materials.append(mat)
    return o


def table():
    walnut = flat("CA Walnut", (0.26, 0.15, 0.08), 0.5)
    felt = flat("CA Felt", (0.07, 0.30, 0.15), 0.95)
    parts = []
    for x in (0.06, 0.88):
        for y in (-0.06, -0.88):
            parts.append(box((x, y - 0.06, 0.0), (x + 0.06, y, 0.70), walnut))
    parts.append(box((0.08, -0.92, 0.62), (0.92, -0.08, 0.70), walnut))
    parts.append(box((0.0, -1.0, 0.70), (1.0, 0.0, 0.76), walnut))
    parts.append(box((0.0, -1.0, 0.76), (1.0, -0.97, 0.79), walnut))   # the rim round the felt
    parts.append(box((0.0, -0.03, 0.76), (1.0, 0.0, 0.79), walnut))
    parts.append(box((0.0, -0.97, 0.76), (0.03, -0.03, 0.79), walnut))
    parts.append(box((0.97, -0.97, 0.76), (1.0, -0.03, 0.79), walnut))
    parts.append(box((0.03, -0.97, 0.76), (0.97, -0.03, TOP), felt))
    bpy.ops.object.select_all(action="DESELECT")
    for o in parts:
        o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    t = bpy.context.active_object
    t.name = "table"
    return t


def make():
    clear()
    export(table(), os.path.join(MOD, "table.gltf"))
    for deck in ("", "e"):
        for n in range(1, 41):
            for kind, png in (("card", "c"), ("dim", "d")):
                clear()
                export(card(f"{deck}{kind}{n}", f"{deck}{png}{n}.png", f"{deck}back.png"),
                       os.path.join(MOD, f"{deck}{kind}{n}.gltf"))
        clear()
        export(card(f"{deck}back", f"{deck}back.png", f"{deck}back.png"), os.path.join(MOD, f"{deck}back.gltf"))


if __name__ == "__main__":
    make()
