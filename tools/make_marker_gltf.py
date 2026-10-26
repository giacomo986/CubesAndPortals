#!/usr/bin/env python3
# The model mods/steps wears, written rather than modelled.
#
# A block that is drawn as a MODEL is M11's other drawtype, and a mechanism
# nothing ships with is a mechanism nobody has run (TODO.md OPEN-43). What ships
# has to be a real glTF and does not have to be art: this is a leaning spike
# inside the unit cube, six triangles, with its own normals and a buffer
# embedded as base64 so the whole model is ONE file that travels as one file.
#
# Leaning at -Z on purpose. A shape symmetric about the vertical looks the same
# whichever way it is turned, and a gate built on one would call a turn that
# does nothing a turn that works -- the same trap mods/timber and the parts gate
# already record.
#
# usage: make_marker_gltf.py [out.gltf]
import base64, json, struct, math

base = [(0.15, 0.0, 0.15), (0.85, 0.0, 0.15), (0.85, 0.0, 0.85), (0.15, 0.0, 0.85)]
apex = (0.5, 0.95, 0.22)   # leaning at -Z, so a quarter turn is visible

tris = []
for i in range(4):
    a, b = base[i], base[(i + 1) % 4]
    tris.append((a, b, apex))
tris.append((base[0], base[2], base[1]))
tris.append((base[0], base[3], base[2]))

# Wound so that every face's normal points AWAY from the middle of the model.
# Backface culling is on for a model with no material saying otherwise, so a
# face wound the other way is a face nobody outside can see -- which is how this
# first drew nothing at all and looked like a shader that could not handle a
# mesh with no skeleton.
def centroid(t):
    return tuple(sum(v[a] for v in t) / 3.0 for a in range(3))

middle = (0.5, 0.4, 0.5)
fixed = []
for t in tris:
    n = None
    c = centroid(t)
    ux, uy, uz = (t[1][a] - t[0][a] for a in range(3))
    vx, vy, vz = (t[2][a] - t[0][a] for a in range(3))
    nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
    outward = sum(n_ * (c[a] - middle[a]) for a, n_ in enumerate((nx, ny, nz)))
    fixed.append(t if outward > 0 else (t[0], t[2], t[1]))
tris = fixed

def normal(t):
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = t
    ux, uy, uz = bx - ax, by - ay, bz - az
    vx, vy, vz = cx - ax, cy - ay, cz - az
    nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
    l = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
    return (nx / l, ny / l, nz / l)

verts = []
for t in tris:
    n = normal(t)
    for v in t:
        verts.append((v, n))

vbytes = b"".join(struct.pack("<3f3f2f", *v, *n, 0.0, 0.0) for v, n in verts)
idx = list(range(len(verts)))
ibytes = b"".join(struct.pack("<H", i) for i in idx)
pad = (4 - len(ibytes) % 4) % 4
buf = vbytes + ibytes + b"\x00" * pad

stride = 8 * 4
gltf = {
    "asset": {"version": "2.0", "generator": "cubes-and-portals tools/make_marker_gltf.py"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"name": "marker", "mesh": 0}],
    "meshes": [{"name": "marker", "primitives": [
        {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3}]}],
    "buffers": [{"byteLength": len(buf),
                 "uri": "data:application/octet-stream;base64," + base64.b64encode(buf).decode()}],
    "bufferViews": [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(vbytes), "byteStride": stride,
         "target": 34962},
        {"buffer": 0, "byteOffset": len(vbytes), "byteLength": len(ibytes), "target": 34963},
    ],
    "accessors": [
        {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": len(verts),
         "type": "VEC3", "min": [0.15, 0.0, 0.15], "max": [0.85, 0.95, 0.85]},
        {"bufferView": 0, "byteOffset": 12, "componentType": 5126, "count": len(verts),
         "type": "VEC3"},
        {"bufferView": 0, "byteOffset": 24, "componentType": 5126, "count": len(verts),
         "type": "VEC2"},
        {"bufferView": 1, "componentType": 5123, "count": len(idx), "type": "SCALAR"},
    ],
}
import sys, os
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "mods", "steps", "marker.gltf")
open(out, "w").write(
    json.dumps(gltf, indent=1) + "\n")
print("triangles", len(tris), "vertices", len(verts))
