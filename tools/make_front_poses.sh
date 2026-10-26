#!/bin/sh
# The front's gun poses (tools/make_front_poses.py) made in Blender and added
# to the robot and its three species, which share its skeleton.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$HOME/kart_assets/front_poses"
flatpak run org.blender.Blender --background --python "$ROOT/tools/make_front_poses.py" 2>&1 |
    grep -E "front pose|Error" || true
CLIPS="$(cat "$OUT/clips.txt")"
for t in robot robot_human robot_mole robot_frog; do
    # shellcheck disable=SC2086
    python3 "$ROOT/tools/gltf_add_clips.py" "$ROOT/mods/robot/$t.gltf" "$OUT/front_poses.gltf" $CLIPS
done
