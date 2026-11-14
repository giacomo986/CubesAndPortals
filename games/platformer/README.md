# The platform game

*[Leggi in italiano](README.it.md)*

A platform game seen from the side, in the manner of the eighties' ones: two levels, a bonus room,
power-ups, their rules and the camera. Two mods: `mods/platformer_core` (server only: the robot, how it jumps,
the models of the pipes, the power-ups and the creatures) and `mods/platformer` (`side = both`: the room, the
levels and the rules on the server; the camera and the count on a player's machine).

## How to get there

In the game (the title menu, or `tools/track.sh`) its gate is on the plaza's south edge, at
40.5 21 26.5: a green pipe in a brick wall under a prize block (`mods/gates`). The world is not one
the server starts with: it is in the rooms file, starts when somebody comes near the gate and stops
a minute after the last one left; the gate is a portal between rooms that borrows a mouth.
`tools/platformer.sh` starts the sandbox with only this world, behind its first opening
(`tools/platformer.sh -- --room platformer` is born in it). Either way you come out in a **room**
whose far wall is glass: beyond it, side on, is the first level. The door on the left gives onto a
bridge with a hole in it, ringed with prize blocks, and a green pipe. **The hole is level 1-1**: it
is a portal that scales (`core.portal_scale`), two by two, onto one of one by one in the sky over the
level's start -- looking into it from the bridge you see the level from above. Whoever drops in
falls out of that sky at half their height, as the game wants them, and their machine swings the
camera from behind them out to the side as they fall. The pipe, under a blue lintel, is 1-2, and
lets in whoever has cleared 1-1 (kept by name in the world's storage).

## In a level

The camera is at the side (`core.side_view`: `docs/MODDING.md`) and follows as a platform game's
screen does: you move a little either way before the picture scrolls, then it scrolls smoothly; a
jump moves it less than a fall. The keys are a platform game's:

| key | |
|---|---|
| A, D | walk left and right |
| W or space | jump (2.4 up; 4.4 along at a walk, 7.2 at a run) |
| S | crouch; on top of the tall pipe of 1-1, go down it |
| Shift | run |
| F or left button | throw a fireball, with the flower |
| E | give up: back to the room |

**Small, big, fire.** You start small: half the robot's height, and with numbers twice as big
(`core.body_movement`, which shrink with the body) so you run and jump as far as at full size. A
power-up prize block gives a mushroom to a small body and a flower to a big one: the mushroom rises
out of the block, slides along the way on and falls off edges, and touched makes you big (the body
flickers between the sizes, then grows); the flower stays on its block and gives fireballs, two
at a time, which bounce along the ground and kill any creature. Hit while big or fire you go back
to small and nothing hits you for two seconds; hit while small, a life.

Three lives. A fall or a hit while small costs one and puts you back, small, at the last flag
passed (the start, or the blue one half way); with none left you are back in the room.

| creature | | landed on |
|---|---|---|
| sprout | a cactus on two feet, hopping along its beat | dies, and you bounce |
| buzzer | a purple bug flying up and down as it goes | dies, and you bounce |
| spikeshell | a ball of red spikes, crawling | a hit: jump over it, or burn it |

A prize block bumped from below with the head gives a coin, or its power-up, once; they are one
deep, in the level's own plane. Coins are counted. **The bonus room**: in 1-1 the pipe three high
with a step before it goes down -- stand on it and press S. You sink into its mouth and fall down
the pipe, which goes on through the ground (its front cut away, so the camera follows you down it)
and hangs from the ceiling of a room of blue bricks with three rows of coins. Walk into the pipe
lying at the room's far end and you go up its other half, through the ground again, and out of
the next pipe of the level. The pole at a
level's end stops the clock and says time and coins: 1-1's leads on to 1-2 with the lives and coins
as they are, 1-2's back to the room. A level is as new whenever nobody is in it.

1-2 is underground: blue bricks underfoot and overhead with lamps in the roof, a dark wall behind,
pits with ledges in them, a low stretch, three ledges climbing, and out into the open for the pole.

## How it is made

The levels lie on the plane z = 0.5 (three cells deep, so they read as solid), running towards -x:
the camera is on their -z side looking +z, which puts the way on at the picture's right, and the
room is behind the camera, where it is never in the picture. 1-2 starts 200 further along the same
plane. Everything is a list of boxes in `mods/platformer/init.luau` (`ground`, `pipe`, `prizes`,
`stairs`, `coins`, `foe`, `finish`), written by one mapgen stage. The levels' blocks give a little
light of their own: the camera looks at the faces the sun never reaches.

- **Pipes** are models built in Blender (`tools/make_platformer_models.py`): a pipe is round and
  two cells wide, so each piece is one cell's model (`core.node_model`) built about that cell's +x
  wall, and the pipe's other cells hold a block that stops a body and is not seen. Every piece has
  a twin that stops nothing: a shaft a body goes through is made of those while it is in it. A
  body sinking into a pipe is held by a launch every tick (`core.player_launch` never pushes a body
  down: it sets its speed to 0 at least), then falls; going up it is launched every tick.
- **The mushroom, the flower and the fireball** are Blender's too, in the same script.
- **Creatures** are Meshy's: `tools/meshy_generate.py` (text to 3D by Meshy's API: a preview, a
  refine, the .glb into `assets/meshy/platformer`, with the line that asked for it beside it) and
  then `tools/meshy_prop.py`, which sizes, decimates and bakes each into `platformer_core`. They
  are rigid -- no clips -- and are the mod's entities (`core.ecs.visible`), moved by the server;
  touching and landing on them is worked out there too, from the positions players claim.
- **Pictures and sounds** are written by `tools/make_platformer_assets.py`.

## Not there yet

See `TODO.md`, "Platform game and arcade shooter (drafts)".
