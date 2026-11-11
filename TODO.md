# Open items

What is missing or wrong today, one short entry each. An entry leaves this file
when it is fixed or no longer matters. How each feature was built, measured and
fixed is in the archive, under the same OPEN-n / CLOSED-n ids that comments in
`src/` still cite:

- [`docs/archive/TODO-storia.md`](docs/archive/TODO-storia.md): up to 2026-09-13.
- [`docs/archive/TODO-diario-2026-10-09.md`](docs/archive/TODO-diario-2026-10-09.md):
  2026-09-13 to 2026-10-09.
- [`docs/archive/TODO-diario-2026-11-09.md`](docs/archive/TODO-diario-2026-11-09.md):
  2026-10-09 to 2026-11-09 (the ending, survival, the sandbox list, front,
  assault, admin, launcher, circuits), with every number.

Rewritten 2026-11-09 after a review of the whole game. The review's finding:
the game is wide (13 worlds, 84 mods) and most of the last month was checked
headless or by script. What it lacks most is not features but hands and ears on
what is built, and three or four structural fixes that each close many entries.

---

## Suggested order

1. The next package needs a new version first: 0.15.0-alpha.2 speaks 122.
2. The test rig (section below): a scripted left click and recorded audio. They
   turn most of "To play" and "To hear" into things a script can check.
3. Models at join (first defect below): one cause behind three entries, and the
   first thing a new player sees.
4. One shared library for the front and the assault (Structure).
5. Giacomo plays and listens through the two lists; what he finds comes back here.
6. Then the additions, in the order he picks.

## Waiting for a decision (Giacomo)

- Arpg: Emberwing at level 11 is lost about as often as won without the Barrows'
  gear. Needs real play before any number moves.
- Front: PvP (two players against each other never played), and a limit on how
  uneven the sides get now that players pick.
- OPEN-35's rest, each a design choice: a seam when a stage changes between runs;
  `core.get_block` reading air on a column not in memory; 20 ms a tick on columns
  on a listening server; the welcome's hash covering the region only; a furnace's
  clock starting again past a restart.
- A world without `--require-ticket` lets a modified client walk into a room by
  naming it; a ticket for one room is good at every room of the server. Proposal:
  tickets always required between rooms, and bound to the room.
- Distribution: macOS, real Windows, signing, app id, OPEN-39, OPEN-52 (below).
- Track key: 32.0 s is the scripted driver's pace, possibly too hard by hand.
- Lab: 1000 points, 36 a shotgun shot, orbs of 3 are guesses.

## Test rig (closes many entries below)

- The timed client has `--use-at` (a right click) and no left click or swing:
  add one. It would check the prop hunt's blow, the track's power-ups, the
  chambers' gun, the bees' harvest, the arpg's held button.
- "Not heard" is written some thirty times. The cause is the timed client, which
  opens no audio device, not Xephyr: let it open one on a null sink and record
  it (`pw-record` is installed). A script can then check that a sound plays, how
  loud, and whether a loop has a seam. Whether it sounds good stays Giacomo's.
- A 1920 x 1080 Xephyr screen for client checks: on a smaller one the HUD's top
  lines fall off.

## To play by hand (built, checked by script only)

The ending:
- Parkour tower: seven stations, the chimneys' exits, the rope from a beam 13
  up; not re-climbed since ledges are grabbed there.
- Chambers 07-15 with the mouse: aiming, cubes, 11's long fall, 12's turrets,
  13's field, 14-15's stacks; the look out of 11's wall since the change.
- Arena: the seven waves through to the Guardian.
- Inferno: the heart with four braziers lit by hand; a wraith's fireball
  setting a fire; a frame lit in the sandbox since the 1:8 change.
- Arpg: the crypt, the pale shape of a body behind walls, the held buttons.
- Backrooms: drops and the compass craft; the chute at a real frame rate.
- Lab: every phase by hand (generators, the climb, valves, torch, frost, the
  tunnel by kart); the pit's parkour route.

Survival: the bounce pad; the Inferno's level; a chambers stretch.

Track: a drift (Ctrl); a loop and a tall loop; a wall ride; power-ups by the
left click; the item box tool; the dragon pipes' flames in a lap; a kart got
into in the Inferno by somebody who walked there; the kart's first-person view
with the knight.

Front and assault: the plane by hand; the tank's unsticking lift; two players.

Sandbox:
- Pocket hut bought from the mayor and built; a fifth hall wanted at once.
- Rewards after the ending: rope gun, portal gun, kart anywhere.
- Bees: harvest, sting, net, apiary upgrades, press.
- Prop hunt: a blow finding a hider, changing block, put back on the edge.
- Museum plinths filled; `core:arrived` on a real crossing between servers;
  friend bag copy between two servers; /mute with a muted player.
- Melon and pumpkin from seed; a chess variant played through; a pony or a kart
  through a local portal pair; walking into a portal that slides past; the
  chase camera at an opening; a zombie hanging (the clip is the robot's).
- A backpack with things in it crossing into the Inferno (OPEN-53).
- OPEN-38 past its 64 MB.

## To hear

Every synthesised sound: rain loop and thunder, village bell and music, oars,
cart wheels, waystone, relic grind, garden insects, crypt, arpg props, track
power-ups, torch beam, turrets, the backrooms' radio, the front's guns and
engines, the Inferno's ambience, the chess blast, creatures' attack and shoot.

Missing: the assault's own sounds (it uses the front's), the yafps base, the
poolrooms, the balloon's burner.

## Defects

Seen by players first:
- For about 10 s after joining, creatures are boxes, rails and other model
  blocks are not drawn at all, and held models are missing, while the client
  reads 456+ models a few a frame. Fix once: models near the player first, and
  a block's boxes drawn until its model is there.
- For about 4 s after a teleport, cave walls whose nearer chunks have not
  arrived show black in the sky.
- OPEN-2: a crossing may wedge the player in the exit block. Never reproduced;
  untried: an exit more than three higher, an arrival inside another body, a
  crossing during the previous one's camera warp.
- Stacks on the ground are not saved.
- Parked karts are drawn as made, not as their last driver's; one node solid.
- Cards: a seat behind a wall leaves the body where it stood and the fan at the
  seat; the fists hide the fan's bottom edge.
- Chess: a game of two left half way waits for ever.
- Prop hunt: a hider is hit by the 0.6 x 1.8 body box, not by the cube; a block
  with boxes is drawn as a cube.
- Front: a craft of mass 2500 barely moves on the ground (800 is fine).
- Windows: a crashed client kills its server unsaved.

Engine:
- `relight_around` may give false sunlight in caves taller than 16. Not checked.
- A freely turned portal: its spin does not push a body; edge-on facing up, the
  lowest strip is black (the floor cell across the plane is not clipped).
- Floor into wall: the eye still jumps 0.92 at the crossing.
- More than 32 portals wanted at once: the farthest pair may be half live.
- A moving mouth onto another world is not carried between messages; one moving
  opening is a Portals message a tick to everybody.
- OPEN-38: the temporary file of a world with no file only grows.
- A sound loop restarts with a seam of a frame (a Music stream would not); it is
  not heard through a portal; a dead mod's loops go on.
- Coloured light: columns saved before it have none until relit; stained glass
  does not filter; sky light is not tinted; cost not measured on the real GPU.
- A tumbling box asleep is woken only by a block changing within 3; one resting
  tilted is stood on as if flat.
- The client's far-key check sits behind the ticket check.
- /mute knows words, not meaning: a mod that rewords them gets through.

Mods:
- Blueprints: the ghost is sent whole on each change; a scaffold tower stays up
  if the server stops mid-build.
- Passport: a letter to a friend server that is down is lost (no queue).
- Sky: a meteorite can land in a cave under the player.
- Garden: ants overlap in file; a creature wandering into the door far from
  players despawns.
- Errands: the carry's goal and the explore's place are coordinates only, though
  `maps:route` can draw them; the board shows and does not take.
- Paths: a sign where a path ends on a bridge is left out.
- Caravans: a new village sends none until its chests fill (kept so).
- Helpers: no cap on columns all robots keep loaded (OPEN-58); the quarry is one
  turn of stair, 24 deep, always east (OPEN-56); F8 shows water beds, and Light
  up lights only the surface (OPEN-57).
- Furnace: an old one keeps what is in a wrong row; no heat gauge.
- Conveyor: ribs made for 1 m/s; a belt a mod writes reshapes late.
- Circuits: a net stops at 4096 cells; insulated wire and gates are not read by
  doors; dust enters a wire only through a repeater or a buffer.
- Track: the same item odds for everybody; shells do not climb loops; other
  karts do not collide with yours; nobody rides as a passenger (OPEN-47).
- Assault: tanks in the escape are behind the jeep when ripe.
- Dragon knight (switched off): no styles, gear not refitted, the cape passes
  through the legs.
- Arpg: weapons are the robot's on every species; nothing shows a ring.
- Zombie flesh and bones have no use.

## Limits, kept until somebody reports them

- Species bodies are near the 1 MB a server serves (.bin 937 / 959 / 978 KB for human, mole,
  frog; the clips alone are 646 KB). Every new robot clip eats that margin: the export asserts.

- OPEN-45: above about 120 m/s a body outruns the streamed ground.
- OPEN-46: a claim whose path turns back on an axis is refused.
- OPEN-47: the kart's server copy is bit for bit on one build only; no modified
  client was run against it.
- OPEN-29: a model block and a robot are not cut at a portal. A fifth opening in
  sight is not drawn. Floor to floor is a somersault.
- OPEN-37: single-pixel pinholes at one exact pose.
- OPEN-20: visible mod entities collide with nothing.
- OPEN-42: one Lua call per ABM cell, no batching.
- OPEN-43: one material per block and per tool.
- OPEN-49: a client-only mod cannot talk to the server; world code loads once a
  session.
- OPEN-54: meshing twice as slow with the new light; one shadow map, no
  cascades; box bodies cast none; 256 casters at most.
- OPEN-55: 16 dyes, not free colours.
- A window once open streams until closed after 30 s unwanted; 70 MB of VRAM a
  window. Gates are 1 x 2: a kart does not fit.
- A far portal between servers is a mouth: 24 a room, not virtual.
- Parkour: nobody else sees a rope, a backflip or a kick; a forged hook untried.
- A crash loses up to 10 s. A world from before block names were kept is refused.
- Identity: the first key seen is trusted and nothing shows it; fields have no
  cursor; chat has no Tab completion.
- Difficulty only in the sandbox room; world rules not on the title menu.
- Worlds made before a change keep the old ground (paths, cities, waystones,
  chests, colour): `tools/track.sh --fresh`.
- Translations: text a mod builds from tables (lore, the diary) and the engine's
  refusals are not in the files; turret lines are English with a translation.

## Structure

- `games/assault/mods/assault_game` (5391 lines) is a copy of
  `games/front/mods/front_game` (3156): 2005 of the front's 2361 distinct lines
  are in both. Every fix to bots, machines or guns is made twice or missed once.
  Move what is shared into one library both load.
- `mods/villagers/init.luau` is at 194 of Luau's 200 locals and the fishers'
  table is already copied because of it. Move the main chunk's state into one
  table before the next villager feature.
- `kMinProtocol` equals `kProtocol`: every alpha shuts out the one before. Fine
  now, with the launcher fetching the right build; to settle before a beta.
- `src/client/main.cpp` is 22139 lines and `src/server/main.cpp` 10236. Not a
  job for its own sake: when a part is next worked on (the timed client's
  script flags first), it moves out.
- 151 TODO/FIXME lines and 52 `ponytail:` notes in the code are not in this
  file: harvest once, keep what still holds.

## Portals, rooms and zones (plan of 2026-11-11, Giacomo's decisions in it)

Decided: a player makes and moves the portal of their own room (below); portals to other
worlds, and those worlds' rules, are an admin's. Rooms are made while the server
runs, when a portal to one is loaded, and let go when nothing leads to them. Bag
and health stay shared between rooms. A zone is a reusable piece of a level; a
player's zones are theirs alone until an admin approves them for everybody's
survival levels.

1. The frame's menu (`mods/portal`). Done 2026-11-11, by script only: owner,
   colour, scaling, window. Not seen in a client. Missing: a portal made or
   resized without a frame; who may CROSS a local portal (the engine seals
   mouths only, `core.portal_seal`).
2. Virtual mouths: one scheduler gives the 28 mouths to the links nearest the
   players, in place of what `mods/gates`, `mods/pocket` and `mods/keys` each do.
   The sandbox uses all 28 today, so nothing built by hand has one. Closes "a far
   portal between servers is a mouth" under Limits.
3. A portal to another room from the frame's menu, as `core.far_portal_*` does
   between servers; tickets between rooms required and bound to the room (see
   Waiting for a decision).
4. Rooms from a file in the saved world, not from the list written in
   `src/client/main.cpp` and `tools/track.sh`; started and stopped while the
   server runs (the router's arrays are sized at start today); rules declared by
   each mod so the page lists them; difficulty in every room. Closes two entries
   under Limits.
5. The zone (`mods/zones`). Done 2026-11-11, by script only, in no client: a box
   saved with its doorways (`zones:doorway` in a side) and markers, turned and
   mirrored with every block's facing; a level laid from a seed, an "end" zone
   last; `zones:lay` / `zones:mark` / `zones:laid`; a player's zones private
   until `/zone approve`. 196608 cells saved and read back in one call, so
   OPEN-42 does not bite at 64 x 48 x 64. Missing: a level laid by a mapgen
   stage for a survival slot (it is written at run time, 4000 cells a tick);
   zones shipped with the game as files (a mod reads none: a tool has to turn
   the store into a `.luau`); doorway kinds other than "door"; levels on more
   than one floor are only what the zones' own doorway heights give.
6. The workshop room: an editor in the game, a "try" button, an admin's approval.
   6b. Tools made for each kind of world (chambers: panels, buttons, doors and
   triggers joined; arpg: areas, corridors, lairs; backrooms: rooms and lights;
   parkour: stations with jumps measured; Inferno: fortress pieces; front and
   assault: bases and points).
7. Each world onto zones: chambers, arpg, backrooms, parkour, Inferno, then the
   front and the assault after their shared library (Structure).

A player's own room is a room of the server with a world of its own, one a
player (not the pocket hall, which stays what it is): made the first time its
portal is loaded, let go when nobody can reach it.

## Additions proposed (2026-11-09), none designed

Small, on what exists:
- Errands' places and carries drawn on the map (`maps:route`).
- Bones to bone meal for crops, flesh to the pets or a trader.
- Signs read on the board itself, not the HUD.
- A cursor and Tab completion in chat and fields.
- Stained glass colouring the light through it.
- Loops in use: lava, kart engines, the balloon's burner, music by biome.
- Item odds by race order on the track; a ghost of the engineer's lap.

Medium:
- The first hour: check what a new player is told about gates, keys and the
  journal (`/story` and the 21 goals exist; not looked at in this review). If
  little: order the first goals as a guide (first tool, first gate, first key)
  and show the next one on the HUD.
- Circuits' rest: wires on walls and ceilings, bus gates, the integrated-circuit
  bench.
- Chambers: light bridges, beams, gels, angled panels; survival's levels from
  mixed and mirrored templates instead of a stretch of the row.
- Cards as items, so the museum can show them.
- Karts that collide, and a passenger seat (`core.ride` exists).

Large, each wants questions first:
- Front PvP as a mode of its own (see decisions).
- Survival's new pieces for the arpg, the front and the assault (plan 5b).
- An online mod catalogue with signed packages (with OPEN-39).

## Distribution (M10)

- OPEN-40: macOS never run (script and CI job written; the client needs Apple's
  SDK; the launcher cannot install a .dmg). Windows only under wine: the menu's
  clicks untried, no stack dump or watchdog, back-to-title asks the password
  again, curl through `_popen` may flash a console.
- Unsigned everywhere. App id `io.github.giacomoperin.CubesAndPortals` is a
  placeholder.
- Not seen: the publish job signing a real tag; the launcher's install from
  real GitHub and its install-first and refusal paths; the flatpak installed.
- OPEN-39: a `.cnpmod` is checked against its own hash only: no index, no
  signatures, no trust decision.
- OPEN-52: no Android or browser build (lowest). First step: clip by `discard`
  under `#version 300 es`, tryable on the desktop.
- Before any release: `ALL_OPEN` and `FRONT_OPEN` false in `mods/keys` (both
  false today), a new `kEngineVersion`, CHANGELOG in both languages, a tag.
