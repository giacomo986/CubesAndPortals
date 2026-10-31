# The backrooms

*[Leggi in italiano](README.it.md)*

An endless maze of rooms nobody built, on three stacked levels, with rare doors leading from one
level to another. The world is `mods/backrooms/init.luau` (server), plus `backrooms_exit` (the
way out) and `backrooms_hand` (the torch, server and client).

## How to get there

`tools/track.sh` (or the title menu) starts the sandbox with its rooms; the plaza's sixth gate, a
wall of yellow wallpaper with a strip light over it, stands at 16.5 21 28.5 (`mods/gates`). You
come out in the room at the origin of Level 0, four steps in front of the portal home. A second
portal home is in the same room of the poolrooms, facing +x: only whoever has climbed up there
finds it.

## The three levels

The world is 64 cells high; the levels are one over the other. F is the first cell of air over the
floor.

| level | F | air | materials | lights |
|---|---|---|---|---|
| Level 1 | 5 | 6 | concrete, slabs, puddles | 2 warm lamps a room, 25% of the rooms dark |
| Level 0 | 21 | 5 | yellow wallpaper, carpet, suspended ceiling | 4 strip lights a room, 16% of the rooms dark |
| Poolrooms | 37 | 9 | white tiles, blue pools | 5 lamps a room, never dark |

The atria rise into the void above their level: ceiling at 19 in Level 1, at 33 in Level 0, at 62
in the poolrooms.

The level's name comes up on the screen as you enter it (`level_at`: from the feet, y ≥ 33
poolrooms, ≥ 17 Level 0, below that Level 1). Nothing is dug and nothing is built.

## How it is generated

A mapgen stage (`backrooms:rooms`) a column of chunks (32×32). The plan is a function of x and z
only, not of the seed: the backrooms are the same for everybody, and the tick finds the portals
without having seen the mapgen run (a world loaded from its file runs none).

- **Rooms.** A grid of rooms 8 cells across. The wall between two rooms is there or not by a hash
  (Level 0 76%, Level 1 42%, poolrooms 36%); if it is there it always has a door 2 wide, so every
  room reaches every other.
- **Corridors.** Rows of 4 rooms along x (or along z) become a corridor 3 wide and 32 long, its
  sides solid, a door on each side where the grid's wall has its own, a light in the middle (Level
  0 22%, Level 1 30%, poolrooms 11%, there a flooded channel).
- **Atria.** Now and then a block of 2×2 rooms is one space as high as the atria's ceiling (Level
  0 9%, Level 1 9%, poolrooms 12% of the blocks): galleries 3 wide along the walls, one floor over
  another (one in Level 0 and Level 1, two in the poolrooms), a parapet on the inner edge, a flight
  of stairs along +x to climb to each gallery (the first from the ground along the south gallery,
  the second from the west gallery along the north), lamps in the ceiling and in the galleries'
  floors, lighting above and below. In the poolrooms there is a pool in the middle. Never over
  rooms with stairs, holes or portals.
- **Things in a room.** Level 0: pillars; Level 1: puddles and pillars; poolrooms: pools and slides.
- **Pools.** Square (2 deep), as big as the room or a lane the room's length (3), channels in the
  corridors (2), and those under the holes. Each has a shell, a steel ladder to climb out on its
  -x side and, in the poolrooms, a rim of wide tiles and lights in the walls under water.
- **Slides.** A blue channel going down a cell a cell, made of straight pieces (`slide.gltf`) and
  quarter-turn bends (`slide_turn_a.gltf` coming in from the +x side, `slide_turn_b.gltf` from the
  -x side; they go out ahead). The mod lays them as a list of directions (`lay`): where the
  direction changes it puts a bend, and returns the cell where the body is thrown into the water.
  - In rooms: a tower of tiles with the ladder and four cells of slide, straight to a pool along the
    room's low side, or with a bend to a pool along its west side (half and half).
  - In the poolrooms' atria: from the first gallery, from a platform beside the east gallery, ten
    cells down along the east side, a bend, along the south side, a bend and into the middle pool.

  A slide is a fast conveyor going down (`core.node_conveyor` 5, 1.25, -1), not solid: the engine
  takes the body down inside it, sitting on the line of its bed (`chute_at`,
  `src/core/physics/aabb.cpp`), straight or along the bend's quarter circle (recognised from the
  slide a cell above that comes in from the side), from 2 m/s up to 5, bringing it back to the
  channel's middle. The client's physics does it like any step, so without jerks; at the end of the
  channel the body flies on with the speed it has and falls into the pool, and the mod plays the
  splash. Jumping gets out. A fall inside a slide does not count. On a bend the body and the view
  turn with the channel; the others see whoever goes down sitting facing along the slide, whatever
  they look at (the server sends it with the channel's direction). The player going down plays the
  robot's `slide` clip (sitting, legs forward, arms up) and has the eye lower, going down slowly.
- **Stairs.** A stairwell climbs 16 cells from Level 1 to Level 0 and from Level 0 to the
  poolrooms: two flights side by side along z (x 1..2 climbing towards +z, x 5..6 towards -z) with
  a landing at each end, half-cell steps walked up, the ceiling open and the shaft walled up to
  the floor above, lamps in the shaft's walls every 4 cells. The last flight comes out of a 2×4
  opening in the floor of the room above. You walk under the flights.
- **Holes.** A 2×2 hole in the poolrooms' floor (to Level 0) or Level 0's (to Level 1), a walled
  shaft, and under it a pool 2 deep: the water cancels the fall (`session.cpp`, `fall_reset` in a
  liquid), you arrive with full health.
- **Portals.** In one column of chunks in three (`PORTAL_CHANCE`) a room has a lit door with a real
  portal, and the same room of another level has the other end: you come out over or under where
  you went in. The frame is dressed as the level it leads to. Only the pairs within 40 cells of a
  player open (`core.portal_open`, `core.portal_link`), at most 7: the openings are 16 and two are
  the world's.

About one room in twelve has stairs or a hole. The room at the origin and the portals' rooms always
stay ordinary rooms. Stairs and landings are the level's they start from: carpet with a brass edge
in Level 0, tiles with a blue edge in the poolrooms, concrete with a yellow line in Level 1.

**A rule of the stage:** a column writes only inside itself, and the walls on the high sides of an
atrium or a stairwell (x0 + 16, x0 + 8...) may be in the next column. The neighbouring room puts
them up, whose low sides they are: the first version of the atria lacked the high wall, and from
the atrium you saw the void between the levels.

## Light

Block light goes from 15 at the lamp and drops by 1 a block; the engine draws it linearly, with a
floor of 15%. The mod:

- puts a strip light or a lamp that is **off** where a room is dark (they are seen, they give no
  light);
- makes Level 0's strip lights and Level 1's lamps **flicker** round whoever is there: every 0.3 s,
  for each player, at 35% a lit lamp of a nearby room goes off for 0.08-0.38 s, once or twice.
  Each change makes the cell's light again (`core.set_block` while the world runs): a few a second;
- lights the corridors in the middle, the stairwells' and holes' shafts, the atria from the ceiling
  and the galleries' floors, the pools from under water.

Creatures come out only where the light is 7 or less, so in the dark rooms.

## Who lives there

Two creatures, `core.register_mob`, come out where the light is 7 or less:

| creature | where | health | speed | blow | how it dies |
|---|---|---|---|---|---|
| Bacteria | Level 0, on the carpet | 30 | 3.4 | 4 | staggers and falls stiff forward |
| Smiler | Level 1, on the slabs | 20 | 2.6 | 3 | shuts its grin and sinks to the ground turning |

Clips: idle, walk, run, attack, death. In the poolrooms there is nobody. Dying, a Bacteria now and
then leaves a filament, a Smiler a tooth: the pieces of the exit's compass (below).

## Sounds

ElevenLabs, text to sound (`eleven_text_to_sound_v2`, 50 credits each, flow
`2zuOw1wXkyasR0eBFnRn`), turned into mono ogg with ffmpeg. The engine plays the creatures' sounds by
name (`<creature>_say`, `_hurt`, `_death`, `_attack`); the mod plays the hum and the splash
(`core.play_sound`).

| file | prompt |
|---|---|
| `hum.ogg` | Fluorescent ceiling lights buzzing, steady electrical 60 Hz hum, empty office room, constant drone -- came out 1 s long, repeated to 8 s with ffmpeg; plays over every Level 0 player every 8 s |
| `splash.ogg` | Person jumping into an indoor swimming pool, big splash, tiled hall echo |
| `bacteria_say.ogg` | Creepy creature clicking and wet gurgling breath, distorted, close-mic, short |
| `bacteria_hurt.ogg` | Monster pained screech, short sharp distorted shriek |
| `bacteria_death.ogg` | Creature dying, long fading distorted wail ending in a dry rattle, then a body thud on carpet |
| `bacteria_attack.ogg` | Fast whoosh of a lashing claw swipe with a raspy hiss |
| `smiler_say.ogg` | Low eerie whispering giggle in a dark concrete room, reverb, unsettling |
| `smiler_hurt.ogg` | Short distorted gasp with electrical crackle, glitchy |
| `smiler_death.ogg` | Ghostly laugh slowing down and pitching down into silence, electrical fizzle out |

## Making the assets again

- **Textures:** `tools/make_backrooms_textures.py` (PIL) writes `textures/backrooms_*.png`.
- **Creatures:** `tools/make_backrooms_creatures.py`, boxes in Blender with an armature and clips,
  exported to `mods/backrooms/{bacteria,smiler}.gltf` and saved in
  `assets/blender/backrooms.blend`. From a terminal:
  `flatpak run org.blender.Blender --background --python "$PWD/tools/make_backrooms_creatures.py"`
  (the absolute path: the flatpak does not start from the project's folder). In an open Blender,
  through the MCP server: run the script without `__main__` and call `bacteria()` and `smiler()`
  after emptying the scene by hand (`read_homefile` in the open window loses the context).
- **Slide and ladder:** `tools/make_backrooms_props.py`, into `mods/backrooms/{slide,ladder}.gltf`
  and `assets/blender/backrooms_props.blend`. The ladder is not metallic: with nothing to reflect
  the engine drew it black.
- **Player:** the sandbox's robot, a link to `mods/robot`. The `slide` clip is in
  `assets/blender/robot18.blend` (action `slide`, from the `drive_idle` pose), exported alone and
  added to `mods/robot/robot.gltf` with `tools/gltf_add_clips.py`. The client picks it when the
  body is inside a slide (`slides` in `src/client/main.cpp`, with `chute_at`), for itself and for
  the others.
- **Compasses, filament, tooth:** `tools/make_held_models.py`
  (`ONLY="backrooms:compass_in,backrooms:compass_out,backrooms:filament,backrooms:smiler_tooth"`),
  models in hand in `mods/backrooms/*_held.gltf` and icons in `textures/backrooms_*.png`. The chest
  is a link to `mods/chest/chest.gltf`.

## What is missing

- Chests are only in the parts of the backrooms generated from now on: in a world already saved
  the rooms already visited stay without.
- The compasses are read only by clicking a block within reach (a click with a compass in hand has
  to be able to open a chest); the torch is not.
- The torch has no sound.
- Slides are not solid: on the ground you walk through the side of their last piece.
- No railing round the holes and the stairs' opening (inside the opening you fall 3 at most, under
  the damage threshold).
- The atria's galleries have no doors to the rooms nearby: you climb to them only by the inner
  stairs.
- In Xephyr the client runs at 4-7 FPS and everything is in slow motion (walking 1.3 m/s instead
  of 4.8): the slide looks slow for the same reason.
- You go back to the sandbox only from the rooms at the origin of Level 0 and of the poolrooms.
- The Smiler's call lasts half a second; in the poolrooms there is no sound.

## Credits

Code, blocks, textures, models and animations: this project. Sounds: generated with ElevenLabs
for this project. The robot: `mods/robot`, of this project.

## The compasses

Two compasses, and nobody gives them away:

- **The entrance's** (`backrooms:compass_in`, yellow face): found by chance in chests. A chest
  (`backrooms:chest`, the model of `mods/chest`) stands in about one empty room in forty (25 a
  thousand of the rooms with nothing in them, on all three levels), against the +x +z corner. It
  is filled the first time somebody opens it, by a hash of where it stands: the compass in two
  chests in five, the torch in one in eight, and one or two filaments (Level 0), teeth (Level 1) or
  either (poolrooms).
- **The exit's** (`backrooms:compass_out`, green face): made in the bag's grid with two
  **filaments** (`backrooms:filament`) and two **Smiler's teeth** (`backrooms:smiler_tooth`), in
  any order. A Bacteria leaves a filament one time in two, a Smiler a tooth (`core.mob_drop`, 0
  or 1).

## The torch

`backrooms:torch`, rare: in one chest in eight. Right click with the torch in hand, wherever you
look: a beam of light along the look, up to 14 nodes, stuns the Bacteria and Smilers it catches for
4 s (`core.slow_mobs` at 5%: they barely walk and do not strike); then it recharges for 6 s. A
click into the void does not reach the server as `core:used`, so the `backrooms_hand` mod
(`side = both`) takes it: its client half claims the right click with the torch in hand
(`core:input`) and sends it to the server, which lights the torch and tells every client where to
draw the beam. The model is the laboratory torch's.

## Reading the compasses

Right click with one in hand, on any block (`core:used`), and a sentence says where the entrance
is (the room at the origin of Level 0, in front of the portal) or the exit (`backrooms_exit`'s
room in Level 1): metres and the side relative to where you look ("23 m, ahead to the right").
From another level it says how many levels up or down and points to the nearest way within 16
rooms: stairs to go up, stairs or a hole to go down. On entering, a sentence hints at the chests
and the creatures.

## The exit and the Key of the Way Out

`mods/backrooms_exit`: in Level 1, in room (12, -9) -- x 96..104, z -72..-64, over a hundred
nodes from the origin, the same for everybody -- is the way out the engineer found: a room with a
door in each wall, green signs over the doors and, on a pedestal in the middle, the **Key of the
Way Out** (`keys:backrooms`, mods/keys). Nothing points to it but a radio: to whoever is in Level 1
it crackles from the exit's direction (`radio.ogg`, `tools/make_radio_sound.py`), more often the
nearer it is, and every 25 s a message says whether the signal is weak, clear or strong. Going down
to Level 1 is a checkpoint: whoever dies there comes back where they came down.

## A survival site's level

Behind a survival site (mods/keys' `keys:level`) the backrooms are the same, but the level is the
site's, in its slot: a start room in Level 0 with the hidden way out in its west wall under a dark
lamp, and an exit room like the world's own 48 + 24 d away, in Level 0 up to d 3 and in Level 1's
dark from d 4. The radio leads there; once there, a green doorway leads back to the start room,
where the wall has opened. From d 5 the lights round the player go out now and then for eight
seconds.
