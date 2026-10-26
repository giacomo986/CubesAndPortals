#!/usr/bin/env python3
"""Adds clips from one glTF to another, joints matched by name.

    tools/gltf_add_clips.py TARGET.gltf SOURCE.gltf CLIP [CLIP ...]

For a model whose .gltf can no longer be exported whole the way it was made --
the robot's mesh has moved on in its .blend since mods/robot/robot.gltf was
written -- but whose skeleton has not: export only the new clips from the
.blend (NLA tracks named as the clips) and add them here. A clip already in
the target under that name is replaced. Each channel goes to the target node
of the same name; a channel for a node the target does not have is dropped.
The keys are appended to the target's .bin; nothing else in it moves.
"""
import json
import os
import sys


def load(path):
    with open(path) as f:
        doc = json.load(f)
    bins = []
    for b in doc["buffers"]:
        with open(os.path.join(os.path.dirname(path), b["uri"]), "rb") as f:
            bins.append(bytearray(f.read()))
    return doc, bins


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    target_path, source_path, clips = sys.argv[1], sys.argv[2], sys.argv[3:]
    tgt, tbins = load(target_path)
    src, sbins = load(source_path)

    by_name = {}
    for i, n in enumerate(tgt["nodes"]):
        by_name.setdefault(n.get("name"), i)

    tbin = tbins[0]
    copied = {}   # source accessor -> target accessor

    def copy_accessor(ai):
        if ai in copied:
            return copied[ai]
        acc = dict(src["accessors"][ai])
        view = src["bufferViews"][acc["bufferView"]]
        data = sbins[view["buffer"]][view.get("byteOffset", 0):view.get("byteOffset", 0) + view["byteLength"]]
        while len(tbin) % 4:
            tbin.append(0)
        new_view = {"buffer": 0, "byteOffset": len(tbin), "byteLength": len(data)}
        if "byteStride" in view:
            new_view["byteStride"] = view["byteStride"]
        tbin.extend(data)
        tgt["bufferViews"].append(new_view)
        acc["bufferView"] = len(tgt["bufferViews"]) - 1
        tgt["accessors"].append(acc)
        copied[ai] = len(tgt["accessors"]) - 1
        return copied[ai]

    added = []
    for clip in clips:
        found = [a for a in src.get("animations", []) if a.get("name") == clip]
        if not found:
            sys.exit(f"{source_path} has no clip '{clip}'")
        anim = found[0]
        out = {"name": clip, "channels": [], "samplers": []}
        for ch in anim["channels"]:
            node_name = src["nodes"][ch["target"]["node"]].get("name")
            if node_name not in by_name:
                continue
            smp = anim["samplers"][ch["sampler"]]
            out["samplers"].append({"input": copy_accessor(smp["input"]),
                                    "output": copy_accessor(smp["output"]),
                                    "interpolation": smp.get("interpolation", "LINEAR")})
            out["channels"].append({"sampler": len(out["samplers"]) - 1,
                                    "target": {"node": by_name[node_name],
                                               "path": ch["target"]["path"]}})
        tgt.setdefault("animations", [])
        tgt["animations"] = [a for a in tgt["animations"] if a.get("name") != clip] + [out]
        added.append(f"{clip} ({len(out['channels'])} channels)")

    tgt["buffers"][0]["byteLength"] = len(tbin)
    with open(os.path.join(os.path.dirname(target_path), tgt["buffers"][0]["uri"]), "wb") as f:
        f.write(tbin)
    with open(target_path, "w") as f:
        json.dump(tgt, f, separators=(",", ":"))
    print(f"{target_path}: added {', '.join(added)}")


if __name__ == "__main__":
    main()
