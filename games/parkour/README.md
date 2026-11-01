# The parkour course

*[Leggi in italiano](README.it.md)*

A course hanging in the sky, ten stations in a row along -x, each one a move of the engine's
parkour (`core.parkour`, `core.rope`). Two mods: `mods/parkour_core` (server only: the robot and
how it moves) and `mods/parkour` (`side = both`: the course on the server, the rope gun on the
client).

## How to get there

`tools/track.sh` (or the title menu) starts the sandbox with its rooms; the plaza's seventh gate,
a scaffold striped orange and black with a staircase of grey blocks and a hanging rope, stands at
16.5 21 52.5 (`mods/gates`). You come out on the course's plaza, in front of the doorway home;
another doorway home is at the top of the tower. On arrival you are given the rope gun (one, however
many you had). In a survival world every parkour site has a course of its own (see below).

## The stations

Each station starts with an orange pad: whoever stands on it reads what to do, and whoever falls
into the void (below y 12) goes back to the last pad touched.

| | where (x) | what | how |
|---|---|---|---|
| 1 | -22 .. -15 | a pit 8 wide, a brick wall on the right | run up, jump beside the wall, hold forward: 5 nodes along the wall (a running jump clears 5) |
| 2 | -35 .. -32 | a wall with a hole 1 high | run and crouch: a slide |
| 3 | -50 .. -41 | a step 2 high | back to the step, back + jump: a backflip |
| 4 | -60 .. -51 | two walls 2 apart, 10 and 7 high | jump against one wall and jump again: kicks up to the top of the lower wall, at 30 |
| 5 | -88 .. -71 | a pit 18 wide under a steel beam at 42 | rope gun (right) on the beam, forward to swing, jump to let go on the far side |
| 6 | -140 .. -103 | a pit 37 wide under two beams at 42, 19 apart | hook the first, let go high in the swing, hook the second before falling, let go over the landing |
| 7 | -163 .. -153 | a brick wall on the right and one across the way at its end, over a pit | run along the right wall; where it meets the one across, jump: the run turns onto that one (an inside corner) and lands round it, the way on 5 further +z |
| 8 | -217 .. -175 | two beams round a corner: the first across the way, the second along it, off to the left | from the first look at the second; its swing turns the line +z onto the landing, the way on 26 further +z |
| 9 | -282 .. -218 | three beams in an S: across, along off to the left, across again | look at the next beam each time; landing 21 further +z |
| 10 | -310 .. -275 | three beams in a U: across, along off to the left, across off to the right | look at the next beam each time; the last swings you back +x onto the landing, and the way on goes west from it, under the beams, 25 further +z |

Station 6 tried in the engine's physics, by search over the timings (rope after the jump, hold,
release, second rope, hold): 6011 of 22218 land, all of them on both beams. Station 7: jumped at
the takeoff's edge or two nodes before it, the run meets the corner inside its five nodes, turns
at the jump and lands at z +6; without the jump the body falls. The jump may come up to 1.5 nodes
before the corner (kept 0.4 s): the second press lands across 0.5 s of timings, 137 of 400, where it
had to come after the run stopped (0.33 s, 88 of 400; earlier was a kick off the right wall). The run
only starts hugging the right wall. Stations 8 and 9 tried the same
way, the body looking at the next beam (the keys pump the swing the way it looks): 1392 of 8464
timings land on 8, 337 of 4096 on 9 and 358 of 4096 on 10, and none with a beam left out.

## The tower and the Key of the Summit

From the U's way on (pad 11) the tower begins: the five moves again,
one after another and climbing, with the same cells as the stations above (so the same jumps).
Each station has its pad, and whoever falls goes back to the last one touched.

| | station | floor |
|---|---|---|
| 1 | chimney | 29 → 36 |
| 2 | wall run | 36 |
| 3 | backflip onto the step | 36 → 38 |
| 4 | chimney | 38 → 45 |
| 5 | slide | 45 |
| 6 | rope, beam at 58 | 45 |
| 7 | chimney | 45 → 52 |

At the top (x -452 .. -442, z 79) the summit's pad gives the **Key of the Summit**
(`keys:parkour`, mods/keys) to whoever does not carry it already, and the doorway back to the
sandbox is there (the world's second portal, moved from the rope's landing).

## A survival site's course

Behind a survival site (mods/keys' `keys:level`) the course is the site's own, in its slot:
the stations above as pieces, plus a plain jump, a jump a node up, three pillars, a bounce pad and
slabs that give way, shuffled by the site's seed, 3 + d of them, with gaps sized to the body's
jump (60% of it at d 1, 85% at d 10). The summit's pad says the level is done, and a doorway past
it leads down to the start, where the hidden way out has opened. The two beams, the corner and
the beams round a corner, in an S and in a U come in from d 7, with every piece.

## The rope gun

Right button: shoots the rope where you look (up to 40 nodes) and hangs you on it. Forward and back
pump the swing, crouch lets rope out, sprint takes it in, jump or the left button lets go. The rope
is drawn as a row of small orange cubes from the hands to the hook.

## How it is made

- The course is a list of boxes of cells (`box`), written by a mapgen stage (`parkour:course`)
  column by column, clipped to each one's edges; no engine terrain (`core.mapgen_core(false)`).
- Textures: `tools/make_parkour_textures.py`. The rope gun in hand and its icon:
  `tools/make_rope_gun.py` (Blender, assets/blender/rope_gun.blend).
- The robot's clips (wall runs to the right and to the left, the kick off a wall, the slide, the
  backflip, the swing on the rope) are in assets/blender/robot18.blend and added to
  `mods/robot/robot.gltf` with `tools/gltf_add_clips.py`.
