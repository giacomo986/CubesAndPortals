# The platform game

*[Leggi in italiano](README.it.md)*

A platform game seen from the side, in the manner of the eighties' ones: three levels, a bonus room, a boss,
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
camera from behind them out to the side as they fall. The pipe, under a blue lintel, leads to
the furthest level open to you: 1-2 once 1-1 is cleared, 1-3 once 1-2 is (kept by name in the
world's storage).

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
at a time, which bounce along the ground and kill any creature; while you have them an aura of red flame burns round the body (the engine's
`"aura"` effect, `core.render.fx`). Hit with the fire you lose it and stay big; hit while big you go back
to small and nothing hits you for two seconds; hit while small, a life.

Three lives. A fall or a hit while small costs one and puts you back, small, at the last flag
passed (the start, or the blue one half way); with none left you are back in the room.

| creature | | landed on |
|---|---|---|
| sprout | a cactus on two feet, hopping along its beat | dies, and you bounce |
| buzzer | a purple bug flying up and down as it goes | dies, and you bounce |
| spikeshell | a ball of red spikes, crawling | a hit: jump over it, or burn it |

The floating bricks (the rows beside prize blocks, the ledges -- not the walls and castles) break:
bumped from below by a big body, or grown into when a mushroom makes a small body big under them,
they fly off in four pieces; a small body only bumps them. A prize block bumped from below with the head gives a coin, or its power-up, once -- one power-up
for every player in the level, out one after another, each a mushroom or a flower as its player
would be given; one that would change nothing for you is not taken, and goes on to somebody else; they are one
deep, in the level's own plane. Coins are counted. **The bonus room**: in 1-1 the pipe three high
with a step before it goes down -- stand on it and press S. You sink into its mouth and fall down
the pipe, which goes on through the ground (its front cut away, so the camera follows you down it)
and hangs from the ceiling of a room of blue bricks with three rows of coins. Walk into the pipe
lying at the room's far end and you go up its other half, through the ground again, and out of
the next pipe of the level. The pole at a
level's end stops the clock and says time and coins: the body hangs on it, slides down onto its base, steps off behind the base and walks by itself
(`core.walk`) into the picture's depth, to the door of the castle standing behind the walkway. The
door is a portal facing the camera: it lets out of a door in the wall behind the next level's start,
from where the body walks forward onto the level's plane, with the lives, the coins and the size as
they are; the last castle's door leads back to the room. A level
is as new whenever nobody is in it.

1-2 is underground: blue bricks underfoot and overhead with lamps in the roof, a dark wall behind,
pits with ledges in them, a low stretch, three ledges climbing, and out into the open for the pole.

1-3 is a castle: grey stone, lava in the pits (touching it costs a life), and what burns -- a hit,
like a creature's, that no fireball puts out:

| | |
|---|---|
| ember | leaps out of the lava every 2.6 s, almost three nodes over the floor |
| bar of fire | four or six balls in a line turning about a block: pass when it points away |
| coal | what the boss spits: it falls where you stood |

At the castle's end, past the half-way flag and a power-up block, is the hall of the **furnace**: an
iron stove three nodes tall. It wakes when somebody comes in, walks at the nearest body and spits
three coals; then its fire goes out for two and a half seconds (it turns grey-blue) and it stands
still. Touching it is a hit and throws you off it, except landing on its head while it is out: that takes 4 of its 16
heat, throws you clear and lights it again, a little quicker. A fireball takes 1, at any time. Put
out, it goes up in smoke and the gate behind it opens onto the last pole.

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
