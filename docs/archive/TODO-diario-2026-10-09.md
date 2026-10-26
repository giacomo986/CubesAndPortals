# Open items

What is known to be missing or wrong today, one short entry each. An entry
leaves this file when it is fixed or no longer matters. The long history of
every item -- closed ones included, with measurements and the reasoning behind
each fix -- is in [`docs/archive/TODO-storia.md`](docs/archive/TODO-storia.md),
under the same OPEN-n / CLOSED-n ids that comments in `src/` still cite.

Items that only concerned tests removed on 2026-09-13 (OPEN-7 `hud`, OPEN-9
`bad_link`, OPEN-22, OPEN-36 `dual_session`) were dropped with the tests.

---

## Portals and movement

**A kart's nitro (protocol 70, 2026-09-26).** Shift with the throttle down
pushes a kart with 1.5 times its engine and lets it go 1.35 times its top
speed (24 to 32.4 m/s on the track), burning a gauge in 3 s that fills again
in 20; let go, the top comes back down at 6 m/s^2. It is in the chassis both
ends step (vehicle.cpp), the gauge on the Player so a chassis put down afresh
does not fill it, shift a bit in a drive record, and the server's allowance
for a body that drives is its top times kNitroTop. A blue bar over the health.
physics_box: the server's copy burns the same nitro to the last bit; 24.00
flat out, 32.40 with it, empty at 7.98 s (3 s of shift). Scripted loop with
`--walk-boost` (shift on straights): 18.31, 18.27, 18.43 s against 20.3-20.8,
0 corrections. Closed 2026-10-04: a correction of a body that drives carries
the server's gauge (Correct::nitro, protocol 83) and the client takes it.
Driven by hand the same day (xdotool in Xephyr, real keys): into a kart
from the paddock, W then W+shift, the bar at about 40% after 1.8 s of
shift, 0 corrections.
Fixed 2026-10-04: the scripted loop had become 27-30 s (20.3-20.8
recorded), bisected to 649f9f0, which put the Inferno's two openings by the
loop's west straight: drawing them costs 50-85 ms a frame in Xephyr, and
player_update cut every frame to 50 ms before the kart's own accumulator
(and that capped at 4 steps), so in a slow frame the kart lost time --
24 m/s on its dial, 8 m/s over the ground. The kart now gets the whole
frame and catches up to 15 steps: loop 20.65, 20.85, 21.07 s, 18.86 with
nitro, 0 corrections. Then the openings themselves: each mouth drew the
whole far world into a full-screen target, clouds marched per pixel included,
for an opening a few pixels wide. The pass is now cut to the pixels the
opening covers (`opening_rect` in portal_render.cpp: the projection cropped,
viewport and scissor on that rectangle), which also narrows the frustum the
far chunks are culled by. Loop, A/B in turns on the same loaded machine (load
16): 2053 and 2072 frames in 90 s against 1418 and 1484 (+42%, about 23 fps
against 16), "openings" at most 30-39 ms against 62-69; laps 20.97/20.67 s,
0 corrections; the Inferno's images seen whole from 60 m and from 30 m.

Then, from play: driving in first person the view turned with the kart's
heading only. It turns with the whole kart now -- nose up a climb, side down
a bank or up a wall -- by the turn the kart's model is drawn with
(`kart_turn`, the camera's up and forward), the mouse's pitch on top. Seen
scripted (`--first-person`, a new client flag that keeps every world at the
eye) stopped on the ring's banked corner: the horizon tilted with the road,
the kart's own wheel square in the picture, 0 corrections.
And the eye in first person had the eased height of the camera behind
(Scene::drive_eye_y, 60 ms) added to the driver's head, so it bobbed a moment
behind the kart it sat in (from play); it is the head's own now, fixed to the
kart as the kart is drawn. Not measured: a still cannot show a lag.

**A track editor (protocol 69, 2026-09-26).** A piece in the bag is drawn
as its plan from above, lighter where it is higher (a wall standing up as a
line). A piece in hand is shown where it would go as itself, seen through,
red where something is in the way or where it would lie over the player; Z and
X turn it a quarter (`Edit::turn`, pieces only), and the server refuses a piece
over any body, not only its anchor cell (a turned piece put down over its
placer stuck them: 272 corrections). `/pezzi` gives two tools first: a right
click on a road with the start line tool puts a start line across it (as far
as the road goes, 12 a side at most; crossed away from whoever clicked, a
white beam over it), with the checkpoint tool the next checkpoint (a yellow
beam); the same tool on its own line takes it away. One such track per
server, kept in the store, each new start line a new board (key `customN`).
Off the road only counts on a lap: before a start line is crossed a kart
drives anywhere, so pieces built out in the country can be reached, and a
kart off its lap is put back on flat piece road at its own height (within 24)
before the loop's. Scripted in the paddock over a stored start line and
checkpoint: laps 9.18 and 8.78 s (custom2), 0 corrections; loop 20.85 s.
Open:
- The drive into the grass: seen 2026-10-04 west of the paddock, below.
- Closed 2026-10-04: a player's track whose route crosses another track's
  start line laps. On a lap, another start line only puts that track by
  (`alt`): the lap's own next gate first and it goes on; that track's first
  checkpoint first and it is the one lapped, timed from its line ("took the
  line onto"). Off a lap a start line switches at once, as before. Scripted
  over a stored track on the loop's roads: across the loop's start line,
  laps 20.66 and 19.60 s (none in 90 s before); and from a loop lap onto a
  track started after the loop's line, switched at its first checkpoint, lap
  20.62 s; 0 corrections.
- Fixed 2026-10-04: a piece is over a body by its cells, not its box
  (session.cpp and the red ghost in the client alike), so a turn's empty
  inner corner is free. Built and the suite passes; not placed by hand.
- Fixed 2026-10-04: a start line or checkpoint spreads over road and
  pieces, not the paddock's earth; clicked on the paddock it is 7 wide, as
  the loop's road (headless: 7 at x 120 on the paddock, 25 before; 16..24
  on the loop's start, as before).

Then an off-road tool (same day, `/pezzi` gives it third): two right clicks
are two opposite corners of an area (up to 256 a side) where a kart on a lap
may drive on anything, so a track may leave the road; a lit post two high at
each corner, a click inside an area takes it away, kept in the store
(`areas`). Found on the way: track_core paved the paddock again at every
start whenever its corner's chunk was not loaded yet, clearing everything
built on it (the posts, and pieces); it paves once per world now (store key
`paved`). Posts seen to survive a restart. Open:
- Closed 2026-10-04: a kart hops onto a step by itself. Driving into
  blocks whose top is 0.2-1.05 above the feet with 1.5 free over it, it
  jumps just high enough (`step_ahead` in vehicle.cpp: rays against the
  blocks' body only, so pieces' ramps are no step, and the server's copy
  finds the same step); looked for 0.2 s of road ahead, so at 16 m/s it is
  over the step as it gets there. A track's walls are two high and stay
  walls. physics_box: up two terraces to y 6, stopped at a wall two high;
  at 33 and 57 degrees and from rest too. Scripted in Xephyr: west off the
  paddock, down onto the sand, up its one-block terraces onto the Inferno
  road, 0 corrections; loop 20.82 s. The paddock's other edges are cliffs
  (cut 40 high) and stay so. The hop is the jump's: from 2.0 s flat out the
  kart is in the air for the step, not eased up it.

Then the paddock and the three ways from it to the tracks were made ground
and not road: `track_core:paddock`, packed earth (its own texture), which
track_game counts as the track on a lap -- a player's track may run through
it -- but not as road to put a kart back on. A world paved as road before
(store `paved` = "1") gets only its top layer laid again, keeping what was
built on it; seen converted with the off-road posts still standing. Fresh
world: loop laps 20.35 and 20.77 s, a player's track over the paddock 8.91
and 8.75 s, never off the road, 0 corrections.

The Inferno track's side in the world (paddock to frame A, D back east and
north) is laid with the paddock's own `pave`, and so became earth with it:
it is road again (a world made meanwhile gets its top laid once more, store
`inferno_road`). And the cavern was built again whenever frame A's cell was
not loaded at world_ready -- built once per world now (store
`inferno_built`), as the paddock. Fresh world: Inferno laps 32.5, 34.2 and
33.8 s, 0 corrections. On the world made meanwhile, 57.1 and 43.0 s at 14
fps with the machine loaded (load 16): valid laps, not a like-for-like time.

**The Inferno track through the real Inferno (protocol 72, 2026-09-26).** The
cavern was the track's own world; now the lap crosses rooms. A world may have
up to four mouths (`net::kMaxMouths`, was two): Portals sends a count and that
many, the server takes four --portal-to and core.set_portal/portal_place take
index 0-3, and the client keeps mouths past the second in
`Session::more_mouths` (opening, link, trigger, the world shown) with a target
each (`Scene::mouth_rt`); a mouth may come out of any of the far world's 16
openings. The pista has three mouths -- sandbox, A onto the Inferno's B
(@inferno/2), D onto its C (@inferno/1) -- and the Inferno three -- sandbox, C onto
D (@pista/2), B onto A (@pista/1): each frame is a way there and back, drawn
from both sides. A body that drives goes with its player into a room that
knows its model (take_carried: movement and model by name), and the client
does the same the moment it arrives (carry_kart), with the body's motion: the
driver played kart_enter again on the first arrival in the Inferno (reported
from play), because the robot's clips are per world and that world had never
seen it driving. A body first seen already in a kart is sat in it, for others
too. And the crossing frame drew no body at all -- the frame's bodies were
gathered in the world just left -- now the one arrived in adds its own. And
the first arrival in the Inferno by hand started the kart again from a stop:
"corrected: a kart driven by the numbers of a body on foot" -- the server took
the kart on under a new generation of rules and filed the one the client was
still claiming by as the room's foot rules; now it takes the rules on under
the client's generation (adopt_body_movement). The scripts never showed it:
their first claim came after the new rules had arrived. `core.player_carry` /
`core.player_carried` carry a line with it (track_game: track, next gate,
elapsed), and `core.body_taken` tells the room left that the kart went too,
so it is not parked there (it was: the driver was last seen in front of A, not
past it). track_game runs in both rooms and times only its room's gates
(`inferno = true`). `inferno_track` cuts the road through the caverns as a mapgen
stage: 8 of air, a deck bridged over the lava, a brick rail with glowrock.
Found on the way: the client tested a mouth only where the feet were each
frame, and a kart at 13 fps (1.8 m a frame) jumped the front of the slab and
drove into the tunnel behind A, once in two laps; it now walks the step as the
server does (`trigger_along`, moved to core/portal). Scripted from the paddock
(Xephyr, client 13 fps): two runs of two laps, 49.4 54.8 and 48.9 56.6 s, 6
crossings each, 0 corrections, never off the road, the kart gone with its
driver every time; seen in screenshots: out of B in the kart, the eye at 22.98
before and after; recorded at 10 fps through the crossing, the kart in every
frame and the driver sat in it. 13 tests pass. Open:
- Laps 49-57 s, not the cavern's 31-34: the client draws the real caverns and
  now three mouths at 13 fps, and the scripted driver takes the corners slowly.
  Not measured on an unloaded machine.
- A lap across a crossing skips the time between the last tick in one room and
  the first in the other (a tick or two): the line carries the elapsed time.
- A window's stream is centred on the world's `portal_a`, so a world seen
  through two mouths is streamed round one of them.
- The crossing frame shows no other bodies of the world arrived in (they
  come the next frame).
- A player walking back on foot into a room whose session was last a kart is
  reset to foot rules (restore_player); written, not exercised.
- A inferno.world generated before this has no road where its chunks were
  already made (the road is a mapgen stage): `--fresh` for it. A pista.world
  made before keeps the old cavern behind nothing.
- Lava falls and the Inferno's creatures reach the road. Getting out in the
  Inferno parks a kart there.

**A track through the Inferno (2026-09-26).** `track_core` builds a fourth
track: west from the paddock, through an 8x8 frame of obsidian (A) into a
cavern of scoria west of the loop (road bridged over lava, glowrock,
pillars), out of a second frame (C) back into the world (D) and round to the
start line. `track_game` times it (key `inferno`): a start line, two checkpoints
in the Inferno and one back in the world, every line painted. The Inferno is a
cavern of this world, not games/inferno's room: a kart and its lap do not cross
rooms. Two engine fixes it needed: a world whose openings 0/1 lead to another
room (the pista) now also crosses (server) and draws (client, the nearest two
in sight, `Session::local_links`) the pairs a mod opens besides them -- before,
both were skipped whenever the world had a mouth; and `core.portal_open` fails
on the first tick of a room, so track_core retries until it opens. Behind
each frame is a closed tunnel eight long: the crossing is the server's, and a
kart at 24 m/s is a few nodes past the plane before it is moved -- against a
back half a node behind it, its middle never crossed. Scripted from the
paddock: three laps 31.2, 34.2, 33.9 s, 6 crossings, 0 corrections, never off
the road. Open:
- No client-side crossing for a mod's pair: for a round trip the camera is in
  the tunnel behind the frame before the server moves the kart.
- Through frame A from 20 nodes, the far end of the cavern shows the sky: its
  chunks are not on the client yet.
- The mouths' images do not show a local pair inside them, nor the other way.

**Track pieces, as stunt racers build them (protocol 65, 2026-09-26).**
`core.node_piece` makes a block a piece of smooth road on a grid of 8 nodes:
straight or quarter turn (radius 1-3 squares), with a climb, a grade at each
end and a bank at each end (`core/track/piece.h`). The anchor block holds the
turn; fill blocks (`NodeTurn::Height`, the byte how full, in 32nds) are the
cells under the road that a foot, a ray and the server see, always below it
(worst sample 0.02 under). The kart drives on the road itself, a Jolt
MeshShape built from the same numbers at both ends. The chassis may now tip
as far as the road under its least steep wheel (plus the old 0.45), and leave
a ramp as fast upward as the road was lifting it; on the flat both are the old
numbers. `track_core` has 22 pieces (`/pezzi` gives them) and builds a ring in
the infield: banked turns, tilts, a table-top jump, a hill, reached through a
gap in the inner wall. Measured: physics_box drives a tilt into a 27-degree
banked turn, 2.57 up the bank, the server's copy the same to the last bit;
scripted laps of the loop 20.4 and 20.0 s, 0 corrections; around the ring 0
corrections.

Then from play (same day): "tra i blocchi curvi il kart compenetra la pista e
scatta di nuovo in posizione". It was the kart taking off: a bank tipped about
its LOW edge made the middle of the road climb two nodes into every bank (and
fall two out of it), and at 15 m/s a kart flew off that crest and landed into
the road -- 0.39 under it, the vertical speed changing 2-3 m/s in one step. A
bank now tips about the middle of the road (the low edge goes up to 1.75 under
the piece's corner, fill cells with it: `kPieceBelow`), the tilts into and out
of a bank are two squares long, the pieces within reach are ONE mesh with
their corners snapped to a 1024th (no step where two meet), and the tilt cone
opens to the steepest wheel's road and closes gently. physics_box now drives a
run, a two-square tilt, a banked turn and out at 18 m/s: 0.000 under the road,
the vertical speed changing at most 0.50 in a step. Laps 20.7 and 20.4 s, the
ring 90 s, 0 corrections. A drive record's missing chunks are 32 bits now (the reach counts
the corners of pieces up to 23 behind it). Open:
- On foot a player stands up to 1/32 plus a sample under the drawn road (the
  fill cells are 32nds of a node): about 3 cm, not seen.
- `core.set_block` on a fill cell leaves the rest of the piece: a mod's raw
  write, and only a player's break takes a piece whole.
- Loops not done.
- A wall ride was not driven by hand, and the scripted driver (`--walk-path`)
  cannot ride one: it steers for points on the ground, and on a wall steering
  is up and down it. In play it came off the demo's wall into the pit.
- The placement preview and the outline were not seen in a screenshot: a
  scripted run cannot type `/pezzi`.

Then, also from play: the rear wheels went into the road at a join and the
view jolted up and down. The kart and the camera were placed on `pos`, the
feet the server checks -- the highest point a wheel touches, which jumps from
wheel to wheel over a join -- and the model was tipped about it. They are
placed on the chassis now (`Player::body_base`, under its centre along its
own up), the camera's height following it over 60 ms. Found on the way, and
there before the pieces: the driver was drawn as the robot standing beside
its own kart, because the client's `core.model_id` looked in a model store not
yet read and said 0; it asks the server's table (Welcome::models) first now.
The crests that still threw a kart: tilts are three squares, the hill and the
bends into 27 degrees two, and a climb flat at both ends and a bank ease by a
half cosine (it bends least for its height). Driven straight at 24 m/s over
the hill, the 27-degree climb and descent and a tilt in and out: 2 steps
without a wheel down on the middle lane, 3 two nodes to the side (a third of
a second before). The demo ring is an oval now: its east side banked from
corner to corner, a kicker and landing into it, a hill opposite. Laps 20.8 and
20.4 s, the ring 90 s, 0 corrections.

Then the rest of what was open (protocol 66): a chicane, one square across in
two or three, right (plan 3) or left (4); a piece outlined whole whichever of
its cells is aimed at, and one in the hand outlined where it would go, green
or red; a piece drawn while its road may be on screen and not only while its
corner's chunk is; and another player's kart sat as its chassis sits -- its
entity carries the server's copy's pitch, roll and base (flag 16, five
floats; protocol_test refuses it on a creature and fifty nodes off). Seen from
a second client, the driver's kart leaning with the ring's bank; driver and
watcher 0 corrections.

Wall rides (protocol 67). A piece may roll its road about its middle up to
a wall and back (`PieceShape::wall`, bank0/bank1 in degrees, straights). A
wheel takes a road steeper than 50 degrees as ground only on a track piece's
road, now a body of its own, and measured from the chassis's up; a wall of
blocks is still no ground. On a road steeper than 34 degrees a fast kart is
held to it: from 10 m/s, fully from 16, pressed in by 1.5 of its weight and
with three quarters of its weight's pull along the road taken away. Behind a
standing wall, fill cells, so a body on it is over the track. physics_box: a
driver keeping to the middle at 18 m/s rolls 89 degrees, comes down on the
middle of the road, the server's copy the same to the bit; at 8 m/s it falls
off. Rolled about a line over the road, the lane moved away under a kart
going straight; with the whole weight's pull taken away a kart nose up drove
over the top of the wall. Laps 20.8 and 20.4 s, 0 corrections.

Wide pieces and a grand circuit (protocol 68). A piece may be two squares
across (`PieceShape::width`): straights, turns, climbs, tilts, banked turns,
jumps, platforms -- not chicanes or wall rides. Turns to the left are their
own plan (5), the right turn's mirror, so a wide road's lanes are right
driven either way; a piece is put down by its right lane. `track_core` lays a
wide circuit east of the loop, reached by a gap in the loop's east wall: 27
pieces by a turtle that works out where each next one goes, with a hill, a
jump, two banked turns and two S-bends. Found on the way, in the engine: a
write a mod made to a column not yet ready was replayed without its facing, so
a piece put down there came back unturned with empty cells under its road --
fixed where the waiting writes are replayed. And the circuit's clearing wrote
air only where a block already was, which in a column not made yet is
nowhere; it writes every cell now. track_game puts a kart that left the
circuit back on its nearest flat road. Driven scripted: a full lap of the
circuit, 150 s, 0 corrections and 0 times off the road; the loop 20.8 and
20.4 s.

Three tracks timed, and a paddock that leads to each (2026-09-26). The karts
and the world's two openings are in a paddock south of the loop, outside all
three tracks, with a way of its own to each: north through the loop's south
wall, north east to the grand circuit's start straight, south to the ring --
moved out of the loop's infield, which only the loop led to. The old ways in
(the inner wall's gap, the east wall's) are gone. track_game times every
track: a start line and three checkpoints each, a player lapping whichever
start line they crossed last, best laps and a board per track (the loop's
under the keys it had). Found on the way: the ring's corner was off the grid
of 8, so every piece snapped four nodes away from the squares cleared for it;
and karts put in a paddock whose hills were still there when the world was
read found a hill in their cell and were not put down. Scripted from the
paddock: the grand circuit 55.7 s, the ring 23.7 and 25.7 s (put back five
times on its wall ride, which the scripted driver cannot take), the loop
20.1 s; 0 corrections. A world saved before this keeps its karts in the bay
and its ring in the infield: start it with --fresh.

**The server drives a kart with its client's keys (protocol 46, 2026-09-22).**
A client's claim carries what its chassis did since three claims back: where
it was put down afresh, and the throttle, wheel and jump of every 60 Hz step,
with the chunks each rebuild of the blocks around it was made without. The
server puts its own copy down there and steps it (`vehicle_put`,
`vehicle_step`), and a claim more than 0.35 from the copy is corrected to the
copy, its speed and heading, both ends putting the chassis down afresh. A copy
that agrees stands for every rule check_move applies, and for inertia and the
wheel, which none of them saw: OPEN-47's check, OPEN-46 and the rise for a kart.
A chassis moved by anything but its wheels is now made afresh rather than moved
(a moved one kept its engine's revs, which no copy sees). Every placement --
correction, teleport, portal, respawn -- is numbered, and a claim made before
the client took the last one is dropped instead of refused again. The copy is
the client's kart to the last bit when both have the same blocks: through the
wire, 242 records, and 6 rebuilds short of a chunk (`physics_box`; the same
records into other keys end 23.86 off; built with the chunk the client lacked,
0.35 off in 2.5 s). Scripted, getting in and two laps: 20 starts of 10 s and
two runs of 75 s, 0 corrections, laps 20.88 and 20.44 s, 21.04 and 20.42 s,
1000 claims agreed with a run; client and server traced step by step
identical for 3231 steps. On the way, GCC 13.3 at -O3 was found building each
block with a shape in a row with the corners of the one before it, and the
first from stack garbage -- a NaN now and then, and a kart through the road:
the 2 starts of 148 corrected at getting in that no log had caught, the
parked karts being a row. vehicle.cpp is built without the straight-line vectoriser. Open:
- On foot is still check_move's: the budget, the walk, the rise.
- Bit for bit on one machine and one build. Our own trig and rounding in the
  chassis (`std::atan`, `std::asin`, `std::round`) is the libm's, and a client
  built elsewhere may part from its server in the last bit.
- A chunk a client says it had not got is taken as air for 5 s after it was
  sent: a modified client drives through the walls of ground it has just been
  sent. A block that changed in flight, not a whole chunk, is not covered.
- Claims lost past three in a row put the chassis down afresh where the claim
  is ("the server lost the thread"), felt as a jolt; no lossy link was run.
- No modified client was run against it: the refusals seen were real
  disagreements, found and fixed above.

**A kart touching anything was corrected every claim (2026-09-20).** Reported
from play three times over -- "girando mi sono buggato in un muro", then "tocco
il muro di lato, il kart prima si inclina e poi scala il muro" with a frame of
it up the wall of the east straight and 122 corrections on the HUD. Three
things were wrong, and the first two are fixed where the physics is:

- A chassis could be tipped a whole radian (`kMaxTilt` was 1.0, 57 degrees).
  Touching a wall at speed rolled it onto that side until its wheels found the
  wall's face as ground and it drove up it. The cone is 0.45 rad now, about 26
  degrees: a kart leans a few in a bend and pitches a few under braking.
- A wheel took ANY surface as ground: the cylinder tester Jolt was asked for has
  no slope limit. It is a sphere the size of a wheel now, refusing anything
  steeper than 50 degrees, so a wall is no more ground to a kart than the sky.
- The server refused a position for being inside a solid block by a box, and the
  box it uses is not the shape the client simulates -- an upright square against
  a chassis that turns, tips and has its wheels outside it. When the two
  disagreed the refusal put the body back where the SERVER's box was clear and
  the CHASSIS was a third of a node inside the block, from which the solver
  needed several steps to climb out, and every claim it sent meanwhile was
  refused again. A tolerance does not fix a standoff: at 5 cm of slop the claims
  came back 5 mm over the line, at 15 cm they came back 5 mm over it again,
  hundreds of corrections a second either way. A body that drives is now checked
  as a POINT (`MovementLimits::solid_half`), which cannot start that loop and
  still refuses the cheat that matters: a kart whose middle is inside the wall it
  is cutting the corner through.

Measured on a scratch world with a straight beside a wall and karts parked on
it: the drive that gave 115 corrections before gives 1 -- and that one is a body
on FOOT, sinking 0.14 into the ground in the step where it becomes a kart (fixed
2026-09-21, below). Six seconds of throttle and full lock against a tree, a head-on crash at
24 m/s and a one-node step taken flat out: 0. The climb itself was never
reproduced in the small; what is fixed is the mechanism the report describes.

**A kart thrown above what a jump reaches (2026-09-20).** The other half of the
same report: after a crash the log says "1.87 above where it last stood,
standing on nothing, and a body here cannot fly: a jump reaches 1.28", over and
over -- 200 corrections, the kart stuck against the wall of the east straight.
The client's own clamp kept a driving body inside the world's speeds and inside
the terminal fall speed, and said nothing about being thrown UP; the server's
rule is that a body which cannot fly may be a jump above where it last stood
(v^2 / 2g, 1.28 on the track). The clamp now caps the chassis's upward speed at
`rules.jump`, which makes the rise it can reach exactly what the server allows,
with the 0.3 tolerance to spare. A jump the keys ask for is set to that same
speed before the step, so it is not what gets clipped.

Not reproduced in the small: a head-on crash into a wall at 24 m/s, a one-node
step across the road at full speed (the kart stops dead against it, it does not
climb), six seconds of throttle against a tree and driving into a parked kart
all gave 0 corrections both before and after. What the cap does is make the
refused rise unreachable from the physics rather than mend the case that was
seen.

**Every jump in a kart was corrected (2026-09-21, from play).** A kart's feet
are where its wheels touch, and once the wheels leave the ground the chassis's
bottom, 0.55 higher: a jump whose top is 1.28 put them 1.76 up, over the 1.58
the server allowed. For a body that drives the rise is now the jump's top plus
that 0.55 plus 0.6 of headroom (`kChassisLift`, `kDriveHeadroom`), 2.43, and
the server allows 0.3 over it; and off the ground the chassis meets 2.43 over
where it last stood as a ceiling and bounces off it at half its speed, so
nothing past it reaches the server. Measured offline: one jump 1.86 standing or
at 24 m/s, jump held 1.92 standing and 2.23 at 24 m/s (from springs the last
landing still has pressed), none at the ceiling; with 0.25 of headroom the
held hops met it at exactly 2.08 and bounced, 6 in 5 s. Played in Xephyr with
real keys: in, 3 jumps and 3 s of jump held, 0 corrections.

Then the track took the height for itself (asked for from play, with power-ups
in mind): a world may say how high over where it stood a body may be,
`core.body_movement`'s seventh number `rise` (protocol 45, at most 64), and
the track's karts have 10, a ceiling they bounce off. What keeps a kart on the
track is no longer its walls -- offline, 1 hop in 8 steering into one ended on
top of it -- but the road: half a second with no road as the first solid thing
under it (on a wall, in the grass inside the loop) and `track_game` puts it
back on the nearest road it fits on, standing, facing the way it went
(`core.player_place`). Checked with a scratch mod putting a driving kart on the
inner wall and 12 over the grass: put back at 106.5 51.5 and at 79.5 106.5,
0 corrections, and the laps went on (24.35 s with both, then 20.46). On the
way: a body put where a mod had just taken a block away found it still there
on its side when the server was still streaming chunks to it, which holds a
session's block changes back -- the chassis in the parked kart it got into,
pushed into the road, 11 to 52 corrections one start in ten. A player a mod
moves is now sent what it is owed first, and the move after it on the same
channel. Open:
- R, while driving, puts the kart back two nodes past the last line it crossed,
  facing the way the lap runs, the clock still going (three nodes short of the
  start before the first lap). Played in Xephyr with real keys: before the
  start to 37.5 20.5, past it to 42.5 20.5, 0 corrections. Since 2026-09-25 a
  scripted run presses it too (`--press-r-at S`, the client's networked loop):
  past the second checkpoint back to 62.5 108.5, past the third to 20.5 62.5,
  after a lap to 42.5 20.5 and the next lap run in 20.81 s, 0 corrections in
  each -- on a server whose third room was the track again, see below.
- **The scripted lap did not finish with the inferno room in** (found and fixed
  2026-09-25). The client gave the server up as silent 5 s after walking into
  the track's opening, 3 runs of 3. No room held the rooms' lock for long; the
  track and the sandbox WAITED for it up to 1.1 s: a room behind on its ticks
  (the inferno, making columns at 48 ms each) polled with no time to wait,
  returned without letting go, and when it did, std::mutex let it take the lock
  back before a waiting room woke. Now a room behind lets go once per poll, and
  the lock is a ticket lock (`TurnLock`) handed on in the order it was asked
  for. Each room prints the longest it held the lock once ticking (50-63 ms on
  this run). `tools/track.sh` as it is: 12 runs of 12, 0 corrections, laps
  19.60-21.09 s.

**Getting into a kart dropped the connection (2026-09-21).** Found by the
scripted driver once it could get in (`--use-at x,y,z`: walk up to that block
and right click it before `--walk-path`): in 7 runs of 21 the client gave the
track server up as silent 5 s after getting in. Not the kart: ENet's packet
throttle. It throws unreliable packets away once the round trip grows past its
own jitter, and a client going from 60 to 40 FPS took a 6 ms loopback to 17 ms
and the throttle to 0 -- every heartbeat, entity update and position claim
dropped for 4 s and more. It is off now (`transport.cpp`, on both ends): 0
drops in 44 runs since. And three ways getting in was corrected, each 1 run in
5 to 10: a chassis held by the countdown drifted 0.000001 through Jolt's centre
of mass against a server allowing 0 (30 corrections), and a wheel pressed into
the road put the feet 0.06 to 0.13 inside it (1 or 2). A held body now keeps its
x and z exactly, and feet inside the cell under them are put on its top when
that is less than a wheel away. 0 corrections in the 24 runs since, getting in
and two laps: 20.51 and 20.47 s, 20.77 and 20.11, 20.90 and 19.94.

**Weight on the ground, classic portal-puzzle air, and karts as rigid bodies (2026-09-19).**
A body on foot closes on what the keys ask at 5/s on the ground (0.60 s to a
sprint, 1.56 nodes of slide from one), 2/s in water, 3/s flying. In the air it
moves by the classic FPS air acceleration, as portal puzzle games play it (10/s, 30 units/s = 0.75 m/s
along the keys), never past the sprint or the speed it had: a standing jump with
W lands 0.48 forward, a running jump with nothing pressed keeps its speed, 30
strafed hops topped at 8.00 m/s. A fall that would land within a node of a floor
opening is pulled to its centre at up to 15 m/s^2. A body that drives is a Jolt
Physics chassis (5.6.0, deterministic build) on four sprung wheels, the size of
`core.body_width(width, length)` (protocol 44), in a Jolt world of its own
holding the blocks within 8 nodes as boxes (115 boxes, 0.6-0.9 ms to build in
Debug, rebuilt every 2 nodes or 0.25 s). Track numbers: 8 m/s after 1 s, 24 by
4 s, full lock at 21 m/s takes 1.1 rad/s of the 1.6 the wheel asks and slides.
Measured: a 7-point walk on a server, 0 corrections (14.6 s, 13.0 before); the
scripted driver on the track in Release, laps of 20.775 and 20.442 s, 0
corrections (21.0 before); four crossings sandbox -> track, 0 corrections, and
one there and back, 0. Still open: other players' karts are drawn level (only
yaw is on the wire) and do not collide with yours; a ramp that throws a kart
more than its rise (10 on the track) bounces off the ceiling, the server's
copy with it (above);
the single-process client pushes an arrival clear by 0.3 whatever the body; in
a Debug client one arrival in three on the track was corrected three times
(0.24, the kart rolling on while the countdown's hold was still in the post);
the scripted driver does not drive after a crossing (the same at 5aeaf49); a
timed client on a sandbox with only `--portals` is corrected hundreds of times
at 42.50 18.9 48.50 after its second crossing (the same at 5aeaf49).

**Portals built from blocks (2026-09-19).** `mods/portal`: a rectangle of
`portal:frame` round a hole of air opens one of the world's openings when its
last block goes in (or the last thing in the hole is dug out) -- standing, 1x2 to
8x8 inside, or lying as a floor or ceiling, 1x1 to 8x8. A right click on the frame
opens its menu: flip the front, turn a quarter, close. Engine side:
`core.portal_open` / `core.portal_close` on the server. Checked with a scratch
mod building frames and `cnp_join_probe`: a probe flying through a standing frame
at y 49 and one coming down into a 2x2 floor frame were both crossed by the
server to the other end; flip, turn and close reach the client as new versions of
the openings; after a restart the open frames open again. A crossing that tips
the horizon (a frame turned a quarter, a floor into a wall) now levels the view
at half a turn a second on a server too, with the mouse in screen axes until it
has, as yaportal's roll_targets did; connected, the roll stayed for good and the
mouse with it (reported from play). And the body now goes through a portal by
the same map the picture is drawn with: the fork's horizontal exception reflected
the body whenever an end was horizontal, so diving into a floor frame with the
picture upright landed upside down and rolled half a turn back, and diving along
its side came out looking 20 to 90 degrees off the picture (reported from play;
all 576 orientation pairs now agree, `portal_traverse`). Walking down a floor
frame's edge no longer sticks: the floor shift, which lowers a body into what
holds it up, took the feet over the opening as enough, and a body whose side
rested on the frame sank into it and was put back by the server every claim (30
corrections in two seconds, the view shaking; reported from play, reproduced in
Xephyr with real keys). It now needs the whole box over the opening: the same
walk gave 0 corrections along the frame, and a walk into the hole still crossed.
A Ctrl+click floor portal (one node) sinks a player only within 0.2 of its
middle, which is where the server already let one through.

**More than a pair, and the far end streamed (protocol 42, 2026-09-19).** A
world has 16 openings; Portals carries which exist in two bytes and, for each,
the one it leads to. `core.portal_link(a, b)` pairs two (whatever either led to
leads nowhere), and the server crosses a body from any opening to its partner.
`mods/portal` names every frame ("portal 3") and links a new one to the last
that led nowhere; its menu lists the other frames to link it to, as yaportal's
did, and a restart keeps names and links (the old two-row save is read once).
Checked on a server with a scratch mod and `cnp_join_probe`: four frames, the
probe through portal 3 (opening 2) came out of portal 4 (opening 3); linked to
portal 1 from the menu it came out of portal 1; after a restart the same. The
client draws the four nearest in sight, and every portal pass now skips an
opening out of the camera's frustum (and, below the first level, one it is
behind). In Xephyr's software GL a world pass is ~20 ms: 49 FPS with no frame in
sight, 11 with four. And a player's interest has windows now (DESIGN.md 3.3's
second centre): the far end of every linked opening within 128 blocks is owed
its 5x5x5 chunks, after the player's own as near, and its columns are made.
Probe at the spawn, other end 400 blocks along x: 9 of the 9 chunks round it
held (0 before); in the client the doorway shows that place's trees before
anybody crosses. After a crossing out of a standing opening looking steeper
than 45 degrees, the view turns out of it at half a turn a second, until the
mouse moves: fallen into a floor frame looking ahead, out of a wall looking 90
degrees down, level and facing out 0.53 s later (Xephyr, real keys). A frame
is renamed from its menu (protocol 43): a panel may hold a field
(`core.ui_field`), a click puts the keyboard in it -- every game key but ESC,
Enter and Backspace reads as up meanwhile, in `plat_key_*` -- and Enter sends
what it holds as `text` on `core:ui_click`. Played in Xephyr with real keys:
"portal 7" became "Città…" (a US keymap has no à; with `setxkbmap it` à è ò
arrived and were drawn), an "e" typed went into the name and did not open the
bag, ESC left the field without the pause menu, and the saved rows carry the
new name on both the frame and the one linked to it. Open:
- The picture through a floor or ceiling portal is upright from one side only.
  Since 2026-09-19 the mod turns a lying frame for whoever finished it, heading
  from where they stood to the middle (the portal's up times its sign along that
  heading: tilt 0 for every standing or lying other end, pitch 10 to 89; played,
  a frame finished from the south came out turned 180 and the fall into it landed
  level). From any other side it tips, and the roll settles it. Floor to floor
  (and ceiling to ceiling) is always half a turn: diving down and coming up is a
  somersault.
- A fifth opening in sight is not drawn: it shows what is behind its frame.
  Four is two render targets more than a pair; more wants smaller targets for
  far openings.
- Mouths (openings leading to another server) are still openings 0 and 1 only.
- The single-process client never opens a frame: its two openings are its scene's.
- Ctrl+click ("portal end") moves an opening out of its frame, and the frame's
  menu goes on describing where it was.
- A frame taken apart by anything but a dig (a mod's set_block) stays open.

**OPEN-2 -- a crossing may wedge the player in the exit block.** Reported from
play, never reproduced. The known cause (an arrival resolver that stepped up
whole nodes) is fixed. Places still untried: a destination more than three nodes
higher than the source, an arrival inside another body, a crossing taken while
the camera warp from the previous one is still settling.

**OPEN-45 -- above ~120 m/s a body outruns the streamed ground.** Up to 120 m/s
at least 121 nodes of ground stay ahead; at 256 m/s only 64 (4 chunks a tick
against 144 owed a second). Options: an interest radius that grows with speed, a
centre led ahead of the body, a bigger budget. Choose when a track exists.

**OPEN-46 -- a claim whose path turns back on an axis is refused.** The server
replays a claim five ways (straight, x first, z first, across then down, up
then across). A path out along z, across and back is found by none of them.
Nothing on foot does that in 50 ms; a vehicle at 160 m/s can. Since protocol
46 a kart's claims are judged by the server's copy of it and not walked.

**OPEN-47 -- vehicles.** Since 2026-09-13 `core.drive(accel, brake, turn, drag)`
makes every body in a world a vehicle (protocol 33): a throttle and a wheel, and
since 2026-09-19 a Jolt chassis on sprung wheels (above), no flight. The server checks
the top speed with no flight (a kart world's cap is its top speed, not a flight
at a sprint), and since 2026-09-22 drives its own copy of the kart with the
client's keys (above), which is how fast the speed grows and the turn. Tried and removed:
with synthetic claims a budget built on arrival times let an instant 0 to
24 m/s through (0 refused of 140, the distance an unchecked body covers) and
corrected an honest kart 4 times in 140 claims at 120 ms of jitter. Checking
inertia wants the client's send time in each claim -- or, decided 2026-09-19,
the server replaying the client's keys and dt through the same player_update
and vehicle_update, now in core/physics for that (Jolt is built deterministic
for it); that also closes OPEN-46 and the rise check. A vehicle per body is
`core.body_drive` (since 2026-09-20). Since 2026-09-25 a seat: `core.ride(player,
host, x, y, z)` (protocol 63, `Ride`) carries one player on another's body, x
to its right, z to its front, turning with its chassis or its look; the
server puts the rider there every tick and ignores their claims, and sends
the seat on the entity channel, which the rider's client eases to; host 0
lets them off with a placement. Two clients: the rider held 1 node to the
right of a host walking an L, and was let off with one correction ("got
off"), the host with none. Nothing in `games/track` sits a passenger yet.
The single-process client refuses `core.body_movement` and `core.body_drive`
for an id that is not its player. On purpose
(2026-09-19): a body's own numbers last as long as its session. They do not
cross a portal, because the world on the other side is another server's and
decides how bodies move there. They are not kept across a reconnect or saved,
because the mod that set them keeps its side in memory, keyed by session.
`track` holds a body at the start and remembers only while the server runs, so
numbers the engine put back would leave a body held that no mod remembers
holding. A mod meets a returning player as a new one and sets its numbers again.
The original engine drops a player's physics override on rejoin for the same reason.

**OPEN-44 -- closed 2026-09-25.** A connected client loads no mods of its
own, only the world's code, and there `core.movement`, `core.drive`,
`core.body_movement` and `core.body_drive` now return 0 with a line in the
log saying the welcome's numbers are the ones (they returned 0 in silence).

**A flight down is not a fall (protocol 39, 2026-09-15).** The server watches
falls from positions alone, so flying down to the ground cost the whole descent:
from y 50 to 20 is 27 damage against a bar of 20, and a player exploring by air
died every time they landed (three times in one session of `tools/play.sh`). A
position claim now carries a flying flag; on a world where that body may fly the
fall starts again where it is. Checked with `cnp_join_probe --travel 40`: 25
nodes down flying, alive; the same descent with `--walked-down`, dead. The
single-process client's copy of the rule was run the same way
(`cnp_client --play-seconds 40 --fly-down 25`, which flies up and comes down
with nobody at the keyboard): 25 nodes down flying, 20/20 left; the same descent
with `--walked-down` fell 25, cost 22 and died. Open: the flag is taken at its
word where flying is allowed, which buys a modified client only what flying down
slowly already gave it. Since 2026-09-19 water breaks a fall too, on the server
and in the single-process client: a body whose feet are in a liquid cell starts
its fall again there. In play, flight turned off 17 nodes over the lake at 92
144: full bar after the splash (the same drop cost 14 before).

## World, blocks and content

**Ladders and trapdoors as in the classic block game (2026-10-04).** Found: mods/villages
(mine shafts) and mods/villagers (a storey's ladder) turned `timber:rungs` as
an upright block, `(2 + t) % 4`, but its byte is a wallmounted FACE: for half
the turns a ladder lay flat, a floor at every cell of a shaft. Both now map the
turn to a face; an LBM (`timber:rungs_on_walls`) puts a column with a flat or
unhung rung onto the wall most of it has, so old worlds mend as they load
(seen: a column at facing 2 went to 1, onto its wall). A body on a ladder
climbs when it walks into it or its wall (player_update; the probe reaches
this frame's step, since at 13 fps a blocked step left the body 0.13 short).
Ladders go only on walls: put on a floor or ceiling they go onto the wall the
placer faces, else back in the bag; breaking the wall drops them. A Blender
model (tools/make_ladder_model.py: rails, four rungs, nails), held as one, with
a bag picture. Trapdoors: `facedir`, bottom or top half by where the look
leaves the cell (floor, ceiling, upper or lower half of a wall's side), hinged
on the wall clicked or away from the placer, 0.125 thick like a ladder; a
right click or cinnabar beside opens and shuts them (an ABM, on power changes
only); open over rungs or a vine they are `*_trapdoor_rung`, climbed. Seen in
Xephyr (13-19 fps): walked up a 5-rung ladder and over its wall; up a shaft
and through its open top-half hatch onto the floor above; a ladder clicked
onto the floor went onto the wall; trapdoors placed top on a wall, bottom on a
floor, opened and shut by hand; a lever opened a hatch; 0 corrections.
Closed the same day: a rung with nothing solid behind it falls, however its
wall went (an ABM, `timber:rungs_fall`, in place of the player-only
core:broken; the demonstration's rungs got a log each); an open trapdoor is
the climbable variant exactly while a ladder or vine is under it (its
cinnabar sweep swaps it); doors open on cinnabar beside either half and shut
when it goes; doors and trapdoors sound on opening and shutting
(tools/make_hatch_sounds.py, synthesised). Seen in a scratch scene: a mod's
set_block took a ladder's wall and the rung fell within a second; a hatch's
ladder taken and put back flipped it between open and climbable; a lever
opened a door and shut it again. Villagers: the way through the world reads
a thin block standing across the middle of its cell above a step -- a fence
post and its arms, a pane, iron bars -- as a wall nobody stands on (kind
Post), so a way no longer cuts past a lone post, where bodies were held
against it (seen: round a post by a wall both ways, never through its cell).
A miner goes down and up the mine's shaft on its own feet (walk, with the old
placing only when the walk fails); seen on a replica shaft, 14 rungs down in
about 5 s and back up the same. Reported then: a miner dug through the wall
behind its shaft's rungs, they fell, and it had no way up. Now dig_cell and
the galleries never take a block a rung hangs on (that way ends there), and
before going down and before coming up a miner mends the shaft (mend_shaft):
into the foot of it first, centred in it, then every cell from the bottom to
the top a rung on the wall most of them are on, with cobblestone put back
behind any that lost theirs. Seen on a replica: wall behind the bottom two
rungs dug away, both fell; mended (4 blocks) and up and out in 6 s. Mended
from the dug gallery itself it walled itself in, hence the walk to the foot.
Closed after: a gallery that meets an obstacle -- something built, a wall a
ladder hangs on, water or lava -- is dug round it (detour): a cell aside,
along past it (1 or 2 cells long) and back onto the way, the side first by
who the miner is; with no such way round, that way ends. Where it gets off
the shaft is chosen after the mend (chosen before, it was the cell the mend
walled, and the miner was put inside the cobblestone). A villager left in a
gallery the mend closed, with no way to the shaft's foot, is put there
(through the mod's own people: core.mob_ids does not list workers). Seen with
a real miner of a scratch village (a home on a bed hashing to "miner", a
replica mine with planks in two ways and a chest at the top, kept day): it
mended the shaft (4 blocks), dug round the planks in the -z and -x ways and
round the wall behind the rungs in the +z way, dug the +x way's 15 cells,
and went up to the chest and back down twice. Closed after: the rescue of a
second villager seen headless (a villager put in a pocket behind the shaft's
wall, the wall mended: "let creature 502 out of a walled gallery").

**Doors, the builder's large house, a builder told to find it itself (2026-10-04).**
Reported from play (worlds/arpg): villagers stuck at doors, a door that
would not open or shut with somebody in it, a large house whose door opened
onto a wall, a builder waiting on the buyer. Found: a villager coming into a
doorway a little off its middle was held by the OPEN door's panel (2 cm to
spare), pushed sideways, and the engine shut the door as soon as it was 2
off -- open, shut, open, for good (headless room test, a step and a diagonal:
stuck before, through in 4 s after). Now an open door is not solid
(mods/doors), the engine shuts a door a creature opened only once it has
stood in its cell (or is 6 off) and nobody, player or creature, stands in it,
and a body a door's box is shut on walks out of it (move_box ignores a door
box it already overlaps; inside_solid does not count doors). The client's
right button on a door with a body in its doorway is the door's, not the
body's. The large house's wall between the rooms is on the door's column
(w // 2) and its way through was in the middle: now right behind the door
(seen on a copy of the reported world: a 9x7 stone house built, a villager
walked in from outside to the left room in 6 s). Every house gets a doorstep
of three cells before its door. The builder's panel has "Find it yourself":
the order keeps `self` (16th field), gatherers fetch any kind it lacks when
their own is not lacking (a kind nobody gathers first), and the builder
fetches too, a kind no gatherer of the village fetches first. Then, asked the same day: a
creature no nearer the next cell of its way for 3 s avoids the cell just
ahead of it for 20 s (PathBody::avoid, up to 8, also kept out of
path_smooth) and plans again; blocked off the middle of its cell it goes
back to the middle before the sideways detour, which in a corridor sent it
back the way it came, for good (headless: a glass pane across a corridor
with a way round, stuck for ever before, round it and there in 11 s after;
the 4 door rooms as before). F4 shows and hides territory edges
(mods/territory, `key_f4` heard by mods; not tried in the client yet).
Closed after (2026-10-04): a creature that opens doors shuts behind it one
somebody left open on its way, as the classic block game's villagers do (mob.cpp
Step::open_door; path_smooth makes every door a corner, or a straight
segment went through it unseen): headless, a walker through an open door
shut it a second after passing. A panel along a side of a cell, full across
it and taller than a step (a ladder's, an open trapdoor's), is a wall to a
way crossing that side (pathfind Info::sides, find_path and path_smooth):
headless on a village's shaft, down 15 cells and back up as before, and from
a pocket dug behind the bottom rungs no way through them.

**Merchant, mine chest, hired guards, houses to choose (2026-10-03).**
mods/villagers: a MERCHANT per village (purple robe, new model with "death")
sells what the village's chests hold -- the mine's and its houses', never a
player's -- 18 goods in bundles at a base price where the village has four
bundles, up to twice that with less, half with more, and buys what a player
holds at half (seen: 8 coal bought for 2, sold back for 1, back in the chest).
The mine hut has a chest (mods/villages); miners keep the ore they meet and
put it there, and with no order dig for the village's stock (stone and ore)
until it is full. The mayor sells a guard for a player's territory, 6
emeralds a day (`territory:hire`): mods/territory spawns a villagers:guard
by the stone, walks it round and sends it after a non-owner on the territory
carrying what they stole from it (seen hitting a thief); and a thief standing
on the territory while an owner is there loses what they took to that owner
(seen: Bruno's marked stack gone to Anna). The client shows where its player
is and what it does to them at the top of the screen. Houses come small 5x5
(14), medium 7x7 (20) or large 9x7 with two rooms (30), of wood, stone bricks
or bricks; on a house bought, a second storey with a ladder (12) and a fenced
garden of wheat with water beside it (6), each its own order on the house
kept in storage (seen: a small stone house, then its storey, then its garden,
built). The builder walks at most 6 s to a block before its scaffolding.
Closed after (2026-10-04, agreed design): a village TREASURY (storage key
`treasury <rk>`) takes every emerald paid to the village (merchant, lots,
houses, extensions, guards) and every one its miners dig; the merchant and
the workers pay out of it and buy nothing with it empty. Emeralds enter the
world only as ore. Every worker trades too (right click, after what it
carries): it sells from the village's chests what its job brings in and buys
what its job needs at full price (TRADES: torches and a pick for the miner,
seeds and a hoe for the farmer, saplings and an axe for the woodcutter...).
`territory:hire` is cancellable now: a player with no territory gets the
mayor's money back and the treasury nothing. Headless on seed 1: 32
cobblestone bought for 2 (treasury 2), the miner bought 16 torches for 2
(treasury 0), then refused them; a guard with no territory left emeralds and
treasury as they were; a right click on a miner opened its panel. The mayor
shows the treasury. Closed the same day (agreed design): a village starts
with 32; its people take freely from its chests, only strangers pay. Every
worker has a tool (hoe, axe, pick, shovel) that wears out in 60 works, twice
as slow without one, and fetches a new one from the chests (with torches for
a miner, who now lights its galleries only with torches it has, and food
when hungry). A CRAFTER per village (new model, with "death") makes tools
and torches from the chests' cobblestone, coal, sticks, planks and logs, two
of each tool the village's kinds use, 16 torches. Every keeper (mayor,
builder, merchant, crafter, inspectors, guards, archers) has a bed: new
villages have a HALL by the square (mods/villages: three storeys, a ladder,
15 beds, a bell that keeps its beds from being homes); a village without
beds enough has its builder put one up as its own order. Keepers sleep at
night, guards and archers in two shifts (odd ones by day), and asleep only a
thief wakes them. Villages also have family houses (three beds round a
table) and houses of two storeys (a ladder, a bed and a table on each); a
player's second storey gets its own bed and table. build() now puts only a
cell's last block of the plan: a build taken up again after a restart dug
out its walls and rebuilt them. Headless on seed 1 (new world): the hall
generated (seen in Xephyr), 11 keepers in its beds, 7 asleep at night and
guard 1 by day; a walker up the hall's ladder to a top-floor bed in 4 s;
the crafter made 2 hoes; a hoe worn out, two remade, the farmer took one
from the chests 115 s later. The hall emptied as an old village's: the
builder ordered one, built it (1461 cells, about 10 min), all 11 keepers in
its beds. Closed after (2026-10-04): the jam at the hall's ladder was not
the crowd but its top. A creature on a ladder climbed only until its feet
were 0.1 under the next cell, slid back, and hung below the floor's edge,
not on the ground, so neither stepping nor jumping it; it now climbs until
its feet are 0.1 over it (mob.cpp). Seed 1, day of 240 s, three nights
each: 8 of 8 abed in about 50 s on the second and third night, 100 s on
the first; before, never all 8 (6 or 7 by morning). And a village keeps
the plan it was laid out by (storage `plan <rx>,<rz>`, PLAN_NOW 2 in
mods/villages; 1 has no hall and no family or two-storey houses), so a
plan changed later no longer gives a half-made village a different plan
in its new columns (headless: the village at -161 116 planned by 1 and by
2). Open: villages planned before the keys were kept have none and get
today's plan -- which one they were started by is not written anywhere;
columns made before core:world_ready use today's plan.
Fixed the same day, from play: the builder cut the hall's corner to build
a player's house -- every log was a tree to mods/villagers, and a
building's corners are logs. A log is a tree's now only with nothing
solid on its column's top and leaves within 3 (`tree_log`): for the
builder's "somebody built this", its lots and the woodcutter's trees.
Seed 1, the village at -161 116: 285 natural trunks all trees (acacias'
crowns to one side too), its 29 corner columns none.
Then (agreed from play, 2026-10-04): keepers no longer all sleep in the
hall but by their work, each under the sign of its building (`bed_mark`,
mods/villagers): the mayor in the hall (bell), the watch in barracks at a
street's end (banner, 9 beds), the crafter in a room over the smithy
(anvil), the merchant and the builder in a small house each by the square
(scales, plans). Plan 3 of mods/villages makes all five (a smithy in
every village now); a keeper whose sign has no bed sleeps where it can
(an old hall's beds first) and the builder is ordered that building
(`order_post`, once each), on a lot clear of the village's other orders
-- they went to one lot. Seed 1: a new village, every keeper under its
own sign, none ordered; the same village on plan 2, everyone in the old
hall and four orders on four lots, and the crafter's house put up from its
plan at 60 s had the crafter in its bed 4.5 s later. Then the builder (2026-10-05):
in 15 minutes of day it had been brought nothing -- the village had no
woodcutter or miner, and its own orders waited for gatherers or a buyer
that were not there. The village's orders now take from its chests first
and the builder fetches the rest itself, a stack a trip; the keepers'
buildings are wood, and the village's own ways trodden earth (the cobbled
way was most of their stone). And a mine's galleries are dug at y 5-6, in
deepslate and tuff, which counted as nothing: every miner too came up
with only what ordinary rock it met. Deep rock is stone now: 29 in 2
minutes against 11. Seed 1 on plan 2, always day: the crafter's house
built at 14.5 minutes (23 before the deep rock; never before the chests),
the barracks' wood and stone gathered by 25 minutes, earth still to dig
(about 8 a minute, the builder walking 40 out for it). Open: mods/villagers is at 194 of Luau's 200 locals in its main chunk --
new state goes in a table (KEEPING), or the mod does not load.

**Mayor, builder and gatherers (2026-10-03).** mods/villagers: per village a
mayor (sells a 7x7 lot for 8 emeralds or a house for 20; the lot becomes the
buyer's territory through `territory:lot` and the village's box grows to take
it) and a builder, at the house nearest the middle. An order is kept in
storage with the ground heights it was sold on, so its plan (level, floor,
walls, roof, bed/table/chest/torch, a 2-wide cobblestone way with stairs to the
nearest street) is worked out again identically after a restart and blocks
already right are skipped. The bill (logs, cobblestone, earth) comes from
woodcutters (fell a tree, plant a sapling), miners (a stair cut into open
ground 8-16 from home) and diggers (earth off the top outside the village),
new homes' kinds (1 in 9 each); missing a kind, or nothing brought for two
minutes, the builder asks the buyer, and anybody hands materials over from a
panel on the builder. New models in tools/make_villager_models.py (mayor,
builder, woodcutter, miner, digger, all with "death"). Beds on a player's
ground are not homes. Seed 1, village by (-161 116): a house bought, 169
things handed over, built with its way and stairs to the street; then a lot: the miner cut a
stair quarry and brought 13 of its 20 cobblestone, the rest and the earth
(no digger there) handed over after the builder asked, and it was levelled
with its way.
Closed the same day: every village is generated with a MINE (mods/villages,
the first plot 10 along a street: a headframe hut over a shaft of rungs 14
down to a 5x4 gallery); villagers finds it by the column of rungs, its ground
takes in 16 round the shaft, and a miner goes down (a climb, then put at the
bottom: a creature's body does not take rungs) and digs 2-high galleries the
4 ways and branches every 4 cells, torches every 6 -- seen bringing 12 then
13 cobblestone. Villages made before keep the stair quarry. A worker that has
not moved on a walk is put a few cells along core.find_path's way (one stood
jumping inside its house); the builder, stuck, is put on the standable cell
nearest the block. The way to a lot is a search round buildings and other
lots (26 cells round a house seen) instead of x-then-z. Six orders a village,
a storage key each. A woodcutter seen bringing 14 logs. The mayor sells only
to a player with a key (core.player_key). Closed after (2026-10-04): watched
to the end headless on seed 1 -- a small wood house (494 cells, a way of 27)
with everything handed over, built in 150 s, put on its scaffolding 3 times;
every cell as planned but the logs gone mossy (timber's moss ABM) and two of
the way's headroom cells left under the house's own eave, 3 over the way.

**Territories (2026-10-03, protocol 82).** `mods/territory`: a boundary stone
founds a player's territory (5x5), which grows a cell every 5 s while an owner
stands on it, the free edge cell nearest them, within 64 of the stone; a panel
on the stone adds the players near as owners. Engine: `core.owned_cell` (single
columns, before any area), `core.owned_at`, `core.owned_member` (an owner's
takings are not marked) and `core.player_factors(p, walk, dig, hurt, regen)`,
walk folded into the body's numbers and dig sent in BodyMovement (protocol 82),
so other mods' body_movement/player_regen do not undo a territory's effect.
Owners 1.1/1.2/0.85/1.5, strangers 0.9/0.8/1.15/0 (also on a village's ground).
Kept as a 128x128 bitmap per territory in mod storage. Played on seed 1 at the
plaza: 47 cells after ~110 s, green posts for Anna, red for Bruno, 0
corrections walking in and out, the same territory after a restart. Closed
the same day: only a player with a key founds one (core.player_key -- on a
world with a file a name belongs to the key that first took it, so names with
keys are identities); none within 24 of the world's spawn (core.world_spawn),
nor grows into it; a thief is told what they took is marked and the owners
there are told who took how much. `core.swap_item` keeps a stack's tag:
mods/rocks swapped broken stone for cobblestone with take/give and lost the
mark, in villages too (seen: 1 of 5 marked before, 5 of 5 after). A watch
for a player's territory came with the guards for hire (2026-10-03).

**Theft and the village watch (2026-10-03).** `core.owned_area(x1, z1, x2,
z2, tag)`: what a player takes out of a container there (moves and
shift-clicks), breaks there, or a container spills there, carries `tag` in the
low bits of the stack's tag; `core.bag_tagged` / `core.take_tagged` read and
take it back. Tagged stacks now merge with stacks of the same tag
(`Inventory::add`), and `container_spill` keeps each stack whole (it used to
drop wear, enchantments and dyes). `core.mob_hunt(id, player)`: a worker goes
for one player as its kind fights (melee, or shoots when given a shot_model).
mods/villagers: per village (homes +12) a watch of guards, archers and an
inspector (models in tools/make_villager_models.py, "attack"/"shoot"/"death");
first catch offers give-back (to the nearest chest) and costs trust, refusal or
a second catch sends the watch while the player is in the village, until they
die; forgotten after 3 days (world age kept in mod storage). Played on seed 1
at the village by (-157 121): marked on break, inspected, given back, caught
again, killed by a guard, back at the plaza. Closed the same day: a block a
player places on owned ground is marked in its cell (node meta
`core:placed`) and breaks, or opens as a chest, unmarked; every village marks
with its own number (one per region, 65536..131071) and only its inspector
asks after it; owned areas said before the sessions exist are kept and handed
over. `core.mob_attack(id, mob)`: a worker goes for another creature (blows,
or arrows that strike creatures); the watch fights what comes out of the dark
within 8 of its village -- seen killing a zombie and a skeleton at night.
`core.mob_pierce(name, part)`: that part of a kind's blows and arrows goes
through armour; guards 8 a blow at 0.6 (20 hp: 3 blows bare, 4 in diamond),
archers 6 at 0.5. Not played in armour: the numbers are the formula's.

**Village houses and ways (2026-10-03).** Every house gets a chest (front
corner across from the table) for its inhabitant. Streets and paths take the
ground averaged over 5x5 cells (cached per cell), are filled up with their
block where they run over the land, and every 1-step between neighbours of a
way is a stair of the way's block facing the higher one, a slab where more
than one is higher (mods/steps: gravel, cobblestone, smooth stone, coarse dirt
stairs and slabs, an oak slab; gravel and dirt ones break to their block).
Seed 1 around (-161 116): 63 stairs in the first regions generated. Worlds
generated before keep their old ways.

**Inhabitants and helper robots (2026-10-02, protocol 81).** A fourth way to
fight, `"worker"`: never spawned or despawned by the engine, driven by its mod
(`core.mob_goto`/`mob_going`/`mob_act`/`mob_face`/`mob_remove`, right click as
`core:used_mob`, `core.node_yield` for what a dig gives). Protocol 81 lets a
Mob hold an item and carry a clip name (flag 2 on a Mob). Models: four bodies
on a copy of the robot rig (`tools/make_villager_models.py`, ~1.3 MB each with
19 clips), with the robot's tool bones. `mods/villagers` and `mods/helpers`
are a coroutine each per body. Played on seed 1's village at -161 116: 8 homes
(5 humans, 2 moles, 1 toad); in 3 minutes the humans reaped 13 sheaves and baked
bread, the toad caught and cooked 7 fish, all 6 asleep in bed within 35 s of
dusk. A robot felled 7 trees and planted saplings, cooked 11 fish through a
furnace, tunnelled 9 cells down to iron and coal and back out.
A creature could not swim up a one-block bank (fixed in step_mobs: it jumps
from water too). Closed 2026-10-03: gravel and sand over a tunnel are propped
with cobblestone and caves under it bridged; a robot climbs by
`core.mob_place` (a cell up, a stone under it) instead of jumping up a
two-high staircase, where the creature physics kept getting stuck; a
robot that cannot reach its chests, or is far from or under its base, digs
its way home (one buried 9 cells down came up and emptied 31 things into the
chest); homes are a storage key per 256-region (~180 a region, 400 regions),
an old single "homes" key read once and moved. Open: villagers do not trade
yet (design).

**Conveyor belt (2026-09-29, protocol 78).** `mods/conveyor`: one item, eight
blocks (straight with a drum at none/out/in/both ends, curve left/right, ramp
up/down), each cell's shape from its neighbours at its level and a level up
and down: on core:placed/core:broken at once, and an ABM every second for what
no event says (screwdriver, another mod's set_block). Models from
`tools/make_conveyor.py` (~/kart_assets/conveyor.blend): ribs and arrows are
bones of a 0.5 s "loop" clip moving half a cell, hidden while off the flat of
the belt, so a line is in step and seamless. Held model and icon from
make_held_models.py. The carrying is the engine's: `core.node_conveyor(id,
speed, height, rise)`, and `belt_under` (core/physics/aabb.h) finds the belt
under a stack, a player (client prediction; the belt's numbers travel in bit
16 of a block's body flags) or a creature. A stack is also pulled to the
middle across, which takes it round a turn, and held on the belt's top, which
takes it up and down a ramp; a solid cell ahead stops it, off the end it falls.
Checked on a headless server: 3.5 cells to a turn in 3.85 s, 6.5 cells over a
ramp up, two cells a block up and a ramp down in 6.85 s, stopped by a wall;
shapes out/mid/up/mid/mid/down/mid/in for that line, a pair redrawn after a
set_facing with no event. Seen in Xephyr over --connect: a player carried 1.8
cells in ~2 s with 0 corrections, a pig carried, the ribs moving. 13/13 tests.
Open:
- The loop is made for 1 m/s: a mod that changes SPEED has ribs out of step.
- A belt a mod writes where no belt was reshapes its neighbours only through
  the ABM, up to a second late.

**A sorter and an extractor (2026-09-29).** `mods/conveyor`. The sorter takes
the stack lying on it (core.drop_at) and moves it just past its edge onto the
belt left, ahead or right (core.drop_send): its three squares are filters in
that order (core.container_item), anything unnamed goes round the unfiltered
ways that have a belt, and a way whose first cell holds a stack waits. The
extractor is a belt's first cell under a hood: one thing a second
(core.container_drop, stack as it is) out of a container beside, behind, over
or under it -- never a sorter's filters -- and not while its own cell holds
one. Models in make_conveyor.py (the sorter's turntable turns a quarter a
loop), held models and icons in make_held_models.py. Checked on a headless
server: chest of 30 stone, 30 dirt, 30 glass, filters stone/dirt/-: all 30 of
each out the right way, nothing piled at the sorter after 30 s, the chest
empty. Seen in Xephyr. 13/13 tests. Both limits it left are fixed below.

**Square roles, and stacks that keep on belts (2026-09-29, protocol 79).**
`core.container_roles(block, "raw raw raw fuel ...")`: what each square of a
container in a cell of that block is for -- raw (materia prima), fuel
(materiale di consumo), product (prodotto finito), store (magazzino, the
default and every chest). Held in NodeDef::slot_roles and put on the bag by
the ContainerSet from the block at its cell on every lookup, never saved.
Machinery goes by it: out only of store and product (container_drop,
container_move's source, so a hopper too), in to any but product
(Inventory::add's default, so container_add, container_move's target,
collect_drops and shift-click); container_add takes a role, and only the
block fills product ("product"); Inventory::fits refuses a player's move into
a product square. The open container's roles travel with it (a byte a slot
after the Container slots) and the panel draws a bar of each role's colour
under its squares, with a legend. The furnace is a row each: ore, coal, what
it made. And a stack no longer ages on a belt, nor on a block that says
core.node_keeps_drops (the sorter). Checked on a headless server: container_add
by role lands in the right row, a bad role refused; a furnace of 3 raw iron and
3 coal beside an extractor put exactly 3 ingots on the belt, never ore or coal;
after 310 s a stack on a belt against a wall and one waiting on a sorter were
still there, one on the ground gone. Seen in Xephyr: the furnace's three
coloured rows and legend, ingots dragged out of product, refused dragged back.
13/13 tests. Open:
- Raw and fuel are labels for mods: nothing yet puts ore in raw and coal in
  fuel by itself (a hopper fills the first free square that is not product),
  and the furnace smelts from any square, so a furnace filled any old way
  still burns.

**The furnace's recipes checked (2026-09-29).** Reported: a furnace filled with
"iron" and coal did nothing. The saved world (worlds/track/sandbox.world,
container at 29 20 51) held compact:iron -- a block of nine ingots, which has no
recipe here or in the classic block game -- with ore:coal and a birch log. Every recipe was
then run on a headless server and in the one-process client: all 13 made what
they should. What the check turned up, and fixed: wood burned nowhere and a log
smelted to nothing. Now FUELS is the classic block game's (coal and charcoal 8 smelts,
compact:coal 80, logs and planks 1.5, a stick 0.5, a cinder rod 12 where the
inferno is loaded), the heat left over is kept in the cell ("heat"), every log
smelts to furnace:charcoal (new item, held model and icon from
make_held_models.py, and a torch from it), and the ore comes out of the raw row
and the fuel out of the fuel row (container_count and container_take take a
role), each falling back to the other row so a furnace filled any old way still
burns. Checked: 17 recipes 2 for 2 with one coal; 8 raw iron on one coal, 8
ingots; three logs alone, one charcoal (the second log burned); three sticks,
one ingot and half a heat left; the reported furnace now makes charcoal of its
log and leaves the iron block. 13/13 tests. Open:
- "the same shape is already made by craft" at startup, already in the
  reported session's log: two crafting recipes share a shape; not looked into.

**The furnace's rows keep to themselves, and a smelt shows its progress
(2026-09-29, protocol 80).** `core.container_accepts(block, role, names)`: what
the squares of a role take (SlotRules in core/item/slot_rules.h, held with the
roles on NodeDef and put on the bag by the ContainerSet); Inventory::fits and
::add go by it, so a hand, a shift-click, a hopper and container_add alike.
The furnace's ore row takes what smelts, its fuel row what burns, a log both;
it smelts only out of the ore row and burns only out of the fuel row. A smelt
is 2 s: started with ore, a free product square and a whole smelt's heat,
made at the end if the ore is still there (the cell keeps "cook", "what",
"heat"; a restarted clock restarts the smelt). `core.container_progress(x, y,
z, seconds)` keeps a start and a length on the container; the open container's
state carries how far it is and its rate (two floats), and the panel draws a
bar under the squares with a percentage, green when full. Checked on a headless
server: raw iron refused by the fuel row, coal by the ore row, glass by both;
8 raw iron and a coal, 8 ingots in 20 s; three logs in the ore row alone, none
burned; a log to smelt and two to burn, one charcoal. Seen in Xephyr: coal
dragged onto an ore square stays in the bag, the bar at 63% with the first
ingot out. 13/13 tests. Open:
- A furnace filled before this keeps what is already in a wrong row: it is
  not burned from the ore row nor smelted from the fuel row, and has to be
  moved by hand.
- No gauge of the heat left, as the classic block game's flame.

**The furnace as a thing you hold (2026-09-29).** It had no picture and was
held as a plain grey cube. make_held_models.py can now make an item's model
out of the block's own glTF (from_block: its meshes in their rest pose, joined,
turned to face the eye at a held block's 0.16; an imported object turns by
quaternion, so it is set to Euler for the icon): furnace_held.gltf and
assets/textures/furnace_furnace.png, three quarters from above like the
hopper's. And on the ground every block drawn as a model (furnace, hopper,
belts, sorter, extractor) is now drawn as its own model, standing, a quarter
of its size -- its "_held" model faces the eye and lay on its back there. Seen
in Xephyr: in the hand, in the hotbar, and a furnace and a hopper dropped.
The chest (box_held.gltf, chest_box.png) and the crafting table
(table_held.gltf, craft_table.png) the same way, from chest.gltf and
table.gltf. Seen in Xephyr: all three in the hotbar and in the hand, the chest
and the table dropped. 13/13 tests.

**Sounds from files (2026-09-28, protocol 77).** Clips made with ElevenLabs,
trimmed to mono .ogg in `assets/sounds/`: break (4 takes), place (2), hurt (2)
picked at random, tone.h bake kept as fallback; step by the material under the
feet (soil, stone, wood; every 1.8 nodes, peaks at -6 dB); splash into water;
portal on every crossing; chest on opening; jetpack a seamless 1.65 s loop held
while flying at 0.25 and stopped on landing (snd_loop). Mods: core.play_sound
("mod:thing", x, y, z) goes to everybody in the world as net::Sound; the client
reads mods/<mod>/<thing>.ogg|.wav, then assets/sounds/<mod>_<thing>.ogg, silent
without. Checked: `audio: 15 sound(s)`, a test mod's sound over --connect
(server sent, client loaded it), a bad name refused, 13/13 tests. Options have
a Volume row (- / + by 10%, master volume, kept in the options file as
`volume`); seen in Xephyr going 100% to 80%. Open:
Creatures: every kind sounds by name from core/ecs/mob.cpp (MobVoice) -- say
every 6..20 s, hurt when its health drops, death in reap_dead, attack, shoot --
sent as net::Sound, files mods/<mod>/<thing>_<what>.ogg: 26 for zombie,
skeleton, pig, cow, sheep, chicken, dog, cat (348 KB, loudnorm -18). Seen over
--connect from night to dawn: say, hurt and death of zombie and skeleton
(burning), say of the animals and pets.
- Not listened to in game; attack and shoot not yet seen fire (nobody was
  reached in the run).
- A sound file is not served as media: a remote client needs its own copy.


**The flight pack or the backpack, worn (protocol 76, 2026-09-28).** The
flight pack came off the robot (mods/robot/robot.gltf lost its 408 triangles;
~/kart_assets/robot18.blend has it as Jetpack_Mesh, and its export is the
same 408 triangles at the same places) and is mods/jetpack, worn in a fifth
worn square, "back" (kWornBack; a bag saved with four loads as it was).
Flying needs it: core.player_fly(player, allowed) keeps a player's flying
speed aside and sends 0, across body_movement, and the client drops out of
the air at 0. A body that may fly has its "last stood" follow it, so one
grounded in the air falls from there: 37.5 to 23.0, 0 corrections (before:
held 3 above the takeoff, 300+ corrections). climb, climb_idle, hang,
hang_shimmy and pull_up held the flame at full size: 0 now, in the .blend too.
mods/backpack is the other thing for the back: 27 slots opened with B (a new
key_b for mods), one container per BACKPACK at a CARRIED cell
(y <= kCarriedBelow, never out of reach; it takes nothing worn on a back),
kept across a restart. A stack has a tag now (ItemStack::tag, bag format 5,
in every slot message): 17 bits a mod numbers with (core.worn_id,
worn_set_id; the backpack's number, given the first time it is opened) and
three colours of 5 bits. core.recipe_dye(item) and core.item_dye(dye, 1..16):
the thing and one to three dyes at a table make it again, same number, with
those colours, drawn on its tint_1..3 materials (worn and held, by anybody:
an Entity's flag 128 carries a held thing's tint; seen from a second client)
and as three marks on its icon. ESC takes any mod's panel off (a click with
id "core:esc", which its mod hears); the creative panel has a search box.
Seen in Xephyr, networked: two backpacks, two contents; red/blue/yellow on
the back; a backpack refused inside one; ESC and "pack" (2 items) in the
creative panel, local too. Both have a
held model and a 32x32 icon (make_held_models.py), worn models on the robot's
rig (tools/export_robot_packs.py). The crafting table is a Blender model too
(mods/craft/table.gltf). core.worn_item(player, square) reads a worn square.
Seen in Xephyr, networked: no flight without it, flight with it, a fall when
it comes off in the air, the backpack's stone there after a server restart.
Shift-click with nothing open puts a pack on, swaps it for the one worn, or
takes it off (Inventory::swap_worn), local and networked. 13/13 tests pass.
Open:
- A mod reload does not undo core.recipe_dye (RecipeSet keeps it).
- In games/ (track, yafps, inferno) the robot has no pack and flying is
  whatever those games say: they do not load mods/jetpack.

**A mod keeps 4 MiB (2026-09-28).** `core.storage_set`'s limit is 4 MiB of
keys and values a mod (`kMaxStoreBytesPerMod`) instead of 1024 keys: a key a
chest or a player fits by the hundred thousand (331185 keys of ~8 bytes, then
refused with the reason), and a mod writing without end still stops.

**Villages and towns (2026-09-28).** `mods/villages`, a mapgen stage run after
every other: one region of 256 in three has a settlement 80-176 into it. The
region's plan (square and well, 2-4 streets cut short where the ground leaves
the square's height by 6 or the peaks start, plots along them with the door to
the street and the floor at the doorstep, lamps in the gaps) is worked out once
from the seed, core.terrain_height and core.biome and each column builds its own
cells of it. Houses, farms, a smithy with a stocked chest; a town (1 in 4, and
only on ground within 6 of the square 40 out every way) has cobbled streets,
cross streets, brick and stone houses and a tower. Paths: each settlement to
its nearest neighbour an A* over 8-block cells can reach without the peaks, 2
wide, cleared of trees a block either side, planks over water. `/paese [città]`.
Seed 1, `--world 1024`: 6 villages, 5-14 buildings; 11 paths found, 27-57 cells
each; 12 stages over 1156 columns in 64.1 s against 56.1 s without the mod,
1.95 s against 1.92 s at startup. Seen in Xephyr: a village of 14 buildings
with a plank bridge over its pond, a town of 28 buildings, a path through a
spruce forest. Then (same day) what was open:
- Roofs by where a house stands: spruce stairs on oak, dark oak on the spruce
  houses of the tundra and on brick, sandstone in the desert, stone on stone.
  `mods/steps` has oak, spruce, dark oak and sandstone stairs, the classic block game's
  recipe, each in its block's picture.
- Paths and streets are walkable: a set's heights are the ground cut down
  wherever a cell stands more than 1 above a neighbour (`carve`). Seed 1,
  `--world 1024`: 33 sets, 15557 cells, 151 cut down, 0 steps over 1 after.
- A path leaves from the end of the street nearest the other settlement and
  skips the cells inside either settlement's houses.
- A smithy's chest is stocked once, remembered in the mod's storage, and a
  chest in the startup region waits for core:world_ready to ask. Two starts on
  one world file, `--world 1024`: 1 chest stocked, then 0.
- 12 stages over 1156 columns in 56.7 s (56.1 without the mod).
Open:
- No villagers: to be designed first.

**The creative panel as pictures (2026-09-28).** `mods/creative` shows 90
items a page (15 x 6) as squares with each item's picture, its name as a
tooltip under the pointer ("rocks:red_sand" reads "Red sand"). Engine: a panel
element `Item` (`core.ui_item`, protocol 74) drawn with the bag's own
`draw_slot`, clicked like a button. Seen in Xephyr: 325 items over 4 pages,
every one with a picture, the tooltip over pink concrete powder, a click giving
64 of it.

**The classic block game's missing blocks, and blocks that fall (2026-09-28).** Six mods,
pictures and plant models from `tools/make_voxel_blocks.py` (PIL, fixed
seeds; `assets/textures/pack.txt` points shared pictures at one file):
`rocks` (stone breaks into cobblestone, andesite/diorite/granite, deepslate
under y 5-7, tuff, calcite and amethyst geodes, dripstone with points, sculk,
clay, sandstone, red sand, coarse dirt, podzol, mycelium, ice, stone bricks,
smooth stone, lava in the cave floors that burns 4 a half second), `plants`
(tall grass, fern, 12 flowers, dead bush, mushrooms, cobweb that slows, vines
climbed, lily pads, cactus that pricks and grows, pumpkin, melon), `woods`
(birch, spruce, jungle with vines, acacia, dark oak 2x2; forests by region),
`colors` (16 dyes, wool, terracotta, concrete, powder that falls and sets in
water), `building` (bookshelf, TNT lit by flint and steel or powered cinnabar,
fences/gates/panes/bars that join, trapdoors, signs read on the HUD, anvil that
falls and mends, cauldron, hay, rails), `dungeons` (rooms with a spawner,
loot chests, cobwebs). Engine: `core.fall`/`core.node_falls` (ecs::Falling,
sent as a Drop with bit 32, protocol 73, drawn as the whole block; lands in the
first free cell, or as its item), `core:landed`, `core.node_glass`,
`core.replace_nodes`, `core.repair_held`; a tile index is 11 bits (2048 tiles;
it was 8, and 379 pictures had no tile). Mapgen, Release: 0.12 s to 0.37 s for
the 36 startup columns. Seen in Xephyr: every block on a showcase, a column of
gravel and sand falling, TNT set off by a lever, an anvil and concrete powder
landing, fences joined, a gate opened, a sign written and read, a cauldron
filled, a spawner's zombies killing an idle player, forests, geodes, cave lava,
ice. Open: a model per plant cell is a draw call each (tall grass is 1 in 12
columns for that); no minecart for the rails; a sign's text is on the HUD, not
on the board; the Kenney pack has no pictures for the new blocks.

**Gravel, and blocks that fall (2026-09-27).** `mods/gravel`: dug like dirt
(0.55 s by hand, a shovel's), one in ten gives `gravel:flint` instead. In the
stone as pockets of radius 1.6-2.85 (one column in 48 holds a middle), and on
the lake beds in patches three wide over sand, dirt or drowned grass: of 400
water columns of seed 1 sampled, 73 on gravel, 178 sand, 87 grass, 62 snow.
Gravel and sand fall when taken from under or put down over nothing, through
air and water, at once (no falling entity); a cell left under water takes the
water. Seen in Xephyr: a column of three gravel and a sand on dirt came down
when the dirt was dug; gravel under a lake. Open: find_nodes answers at most
512 cells, so counting a region needs smaller boxes.

**Bow and arrows as in the classic block game (2026-09-27).** `mods/archery`: the right
button held with a bow draws it (only with an arrow in the bag), letting go
looses it; power (t^2 + 2t) / 3 of a one-second draw, 60 m/s times it, ceil(6
x power) damage and a critical on a full draw; nothing under a tenth. Engine:
a player's arrow is the skeletons' arrow with an archer (`loose_player_arrow`,
`core.shoot_arrow`): 20 m/s^2, 0.99 a tick of drag, strikes creatures and other
players but never its archer, stays 60 s where it lands and is picked up by
walking up to it (`collect_drops`); the bow wears one of its 384 a shot. On the
player's machine the bow bends in three frames with the arrow on the string
(`core.held_model`), the view narrows up to 15% (`core.zoom`), and the archer
walks at a fifth. Recipes as the classic block game's (bow, 4 arrows, 4 string from a wool);
flint from gravel (below); skeletons leave 0-2 arrows.
Models and icons from `tools/make_held_models.py`. Seen in Xephyr, server and
local: the draw in three frames and the zoom, a pig knocked back by one arrow
and gone after the second, an arrow stuck in the ground and two picked up
(13 to 15). Open: the robot has no aiming pose, so in third person the bow is
only carried; player arrows fly only in a world with creatures registered
(they are stepped with them).

**Drops as in the classic block game (2026-09-27).** A stack on the ground is the thing
itself, 0.12 off the ground, bobbing 0.06 and turning (`drop_body`,
`draw_drop`, one path for the networked and the local client): its `_held`
model or worn piece fitted to 0.5, a tool as its `tool_*` bone of the player
model stood on its handle, a block a 0.25 cube of its own tiles; the old box
only for an item with none of these. The shovel's handle now runs up the
pick's axis and the axe's blade faces the swing (-Z of the bone), turned in
`mods/robot/robot*.gltf` directly. Seen in Xephyr: 4 tools, stone, dirt, fish,
egg, iron helmet; the axe in first person has its blade away from the eye.
The same turn is saved in `~/kart_assets/robot18.blend`, `robot_tools_arms.blend`
and `robot.blend` (Robot_Mesh and FP_Mesh; bone-space bounds match the
.gltf). The loose `Tool_*` part objects in those files are left as they were.

**Ores as in the classic block game (2026-09-25).** `mods/ore` buries eight ores, each
dropping its item: coal ore gives `ore:coal`, iron/gold/copper raw metal to
smelt, cinnabar 4-5, lapis 4-9 (extra count from `core:broken`, since
`core.node_drop_item` gives one). In the worldgen test's 64x64 world: 760 coal,
470 copper, 520 iron, 55 lapis, 119 gold, 233 cinnabar, 29 diamond, 35 emerald
cells, none on the surface, diamond at y <= 10. A chunk as stored and streamed
(same codec) costs 1538 bytes on average against 1183 with the four ores of
before and 1025 with none (the 32 startup chunks of seed 1); generating the 36
startup columns took 0.49 s against 0.26. Coal seams saved when the block was
`ore:coal` load as `ore:coal_ore` (`core.node_alias`, read only when a stored
block table is carried over; checked on worlds/play/mondo.world: no
placeholder, against `ore:coal` magenta without it). Played in Xephyr with an
iron pick: all eight dug, each drop right (cinnabar 9 from two, lapis 17 from
two); the coal icon was too dark to see on a slot and is lighter. Open: old
stacks of iron/gold ore blocks (the blocks have no item of their own now) no
longer smelt.

**Every item has a model in the hand (2026-09-25).** The 41 items that were
drawn as nothing when held (all but the four tools, which are robot bones, and
the armour, which has its worn model) are Blender models made by
`tools/make_held_models.py`: `<item>_held.gltf` beside each mod, registered as
the model `<item>_held`. The client draws it rigid in hand_R's palm, scaled by
the forearm like a held block, in first person and on every robot
(`draw_held_block`); `Scene::hand` and `Body::hand` now carry the full item
name. Seen in Xephyr in first and third person: bucket, lighter, diamond,
emerald, fish, feather, beef, bone, stick, brush. Served: sandbox 94 models
(was 53), inferno 14, 0 skipped. Open: a held armour piece still shows nothing;
`furnace:iron_pick` and `farm:hoe` swing by the block's material, not as a pick.

**Wheat (2026-09-25).** `mods/farm`: a hoe tills grass or dirt into
`farm:soil`, seeds sow on it, wheat grows through four stages (an ABM, 30 s,
one in three: ~90 s a stage), ripe gives a sheaf and 1-3 seeds, three wheat in
a row are bread (5 back). A cow and a sheep now breed with wheat instead of
grass. A crop is a model of four crossed quads (`tools/make_farm_assets.py`)
on a block that is NOT solid with a box its height: the engine now lets a ray
meet a non-solid block by its boxes (`raycast.cpp`), so it is aimed at and
walked through. Played in Xephyr: tilled, sown, ripe broken for a sheaf and a
seed, walked through the row with 0 corrections. Ten sown on a server alone:
the first ripe at 150 s, all ten at 450 s. Since then: soil with no water
within four cells (on its level) grows at half the rate, and a crop with no
soil under it pops off as its drop at the next sweep (seen: 70 s). Bread eaten
in Xephyr (hurt to 10, one bread, back up); bread and the hoe crafted on the
table by hand, and two cows fed wheat made a calf (the server now logs a
birth). Since then (2026-09-25), as in the classic block game: bare soil with no water
goes back to dirt (`farm:dry`, 30 s, one in four), wheat grows only in light 9
or more (`core.light(x, y, z)`, new: block light or the sky's scaled by the
hour), and a player landing on soil from more than 0.75 above tramples it,
crop and all. Measured on a server: bare soil was dirt within 60 s; a crop in
a stone box (light 0) stayed at stage 0 for 1080 s while one beside a torch
(light 13) was ripe by 240 s; the sky gives 15 at noon. In Xephyr: walking
the length of a sown strip left it all soil; dropped from 3.5 above, the soil
under the player became dirt.

**Old ore in bags and chests (2026-09-25).** `core.item_alias(old, new)`,
read by `ItemRegistry::adopt_stored` only: `mods/ore` maps `ore:iron` and
`ore:gold`, which have had no item since they dropped raw metal, to the raw
metal. Checked by a chest filled with 7 `ore:iron` under the mods of before
8ac1bef and opened under today's: 7 `ore:raw_iron`. No saved world here held
any.

**Beds (2026-09-25).** `mods/bed`: three wool over three planks, one cell
(The classic block game's is two). A right click sets where its sleeper comes back after
dying -- `core.player_spawn(player, x, y, z, on)`, per session, kept by the
mod in its storage by name and given back on the next join if the bed is
still there -- and from dusk to dawn ends the night for everybody
(`core.time_of_day`, `core.set_time`). Checked: a server alone at 0.90 is at
0.26 after the click; in Xephyr, killed after the click, put back on the bed
(64.5 21 62.5) 3 runs of 4; then 15 runs of the scripted client clicking the
bed (`--use-at`) and a scratch mod killing it -- 10 at once, 5 after being
hurt, healed and 40 s -- all put back on the bed. The one run that came back
at the world's spawn was not reproduced. Since then (2026-09-25): at night
the click puts the sleeper on the bed (`core.player_place`), where they lie
until they walk off it or it is day, and the night ends when everybody in
the world has lain down for 3 s; a creature that fights within 8 nodes keeps
anybody from lying down (`core.monsters_near`, new). In Xephyr: on the bed at
64.5 20.56 60.5, the clock went from 0.906 to 0.261 three seconds later; with
a zombie 2.8 away, "You may not rest now; there are monsters nearby".
Since 2026-09-28 a bed is two cells, as the classic block game's: the foot where it is put
and the head (`bed:bed_head`) in the next cell away from whoever put it, or
the bed back on the ground if that cell is taken; broken by either end both
go and it gives one bed. The foot wears the whole bed (`bed.gltf`, legs,
frame, red blanket, sheet and pillow, `tools/make_bed_gltf.py`). And the
sleeper lies on it: `core.player_lie(player, on, x, y, z, head_x, head_z)`,
protocol 75 (`Lie` to their client, bit 64 of an Entity with the yaw the way
the head points to everybody else). Drawn as the robot tipped a quarter turn
about its feet (`lay`, the kart's pitch), the eye on the pillow; walking off
gets them up on their screen at once and the mod tells the rest. Seen in
Xephyr with two clients at 0.85: in bed the eye went from 22.06 to 20.88, the
robot lay on its back with its head on the pillow, to itself (V) and to the
other player; walking off, "gets up". Villages put their beds two long.
Its pose is the robot's `sleep` clip, made in Blender on Robot_Rig in
`~/kart_assets/robot18.blend` from idle's first frame: head turned aside, the
left forearm across the chest, the right arm by the side, the right knee up,
feet fallen outwards, and a breath in the chest every 2.5 s; exported alone and
added with `tools/gltf_add_clips.py`. Seen in Xephyr from the sleeper (V) and
from the other player.

**Storage blocks (2026-09-25).** `mods/compact`: nine coal, copper, iron or
gold ingots, lapis, cinnabar, diamonds or emeralds on the table are a block,
the block alone the nine back. Pictures `tools/make_block_textures.py`. Seen
in Xephyr; nine iron ingots crafted into a block and back by hand.

**Cinnabar (2026-09-25).** `mods/cinnabar`: dust, lever, button, cinnabar
torch, repeater (a right click steps its delay 1-4), comparator (reads power or
how full a chest or furnace is behind it -- `core.container_fill`, new --
compare or subtract its sides; a right click switches), lamp, piston and
sticky piston (push up to 12 blocks, break what is not solid in the way,
leave bedrock, obsidian, chests and furnaces; a sticky one pulls one back),
and the cinnabar block (mods/compact). Dust carries 15 cells, level or a step;
solid blocks are strongly or weakly powered as in the classic block game, except that dust
powers every block beside it. Dust and lamps follow at once; torches,
repeaters, comparators and pistons change 0.1 s (a repeater 0.1-0.4 s) later
from a queue in the mod, so clocks work; a torch changing more than 8 times
in 3 s burns out for 5. Every change works the circuit out again over a box
16 each way, and on from its edge (16 boxes at most). Checked on a server
alone: a repeater of delay 4 lights its lamp 0.45 s after the lever; a torch
and repeater clock changes 10 times in 6 s, and burns out at delay 1; a piston
pushes three blocks and takes its head back; a sticky one pulls its block
back; a comparator behind a chest of one stack gives 1 and lights a lamp.
Seen in Xephyr: the NOT gate before and after its lever, the piston out with
two blocks pushed. Since then (2026-09-25): pistons are wallmounted, facing
out of the face they were put against, any of six ways, and push the players
(`core.player_place`) and creatures (`core.push_mobs`, new) standing in the
line; an observer (wallmounted) watches the block it was put against and gives
a two-tick pulse out of its front when it changes; a hopper (5 slots) takes
what lies on it (`core.collect_drops`, new), then one item every 0.4 s from the
container above, and passes one on into the block it was put against
(`core.container_move`, new, keeps wear and enchantment), locked while
powered; a comparator watches its container's revision
(`core.container_revision`, new), so a chest filled by hand is read. Checked on
a server: a piston facing up lifts two blocks, an observer lights a lamp for
two ticks when the block it watches is placed, a hopper passes 5 stone from a
chest above to a chest below and takes 3 dirt dropped on it, a comparator
lights when stone is added to its chest, a hopper beside a lever locks. In
Xephyr: a cow lifted by a piston facing up, the player pushed a node by a
piston beside them, 0 corrections. Since then: an observer watches its cell
through the engine (`core.watch_cell`, `core:watched`, new), so any mod's write
is seen -- on a server one pulse when another mod grew the wheat it watches;
a pushed player slides the node over four ticks (0.25 a tick in Xephyr, 0
corrections); a circuit is worked out box by box for as long as edges change
(256 boxes at most, said in the log) -- a lamp 201 cells from its lever
through 13 repeaters lit 1.6 s after it. The watch is kept where every
server write goes (`write_block`): a mod's, a player's edit, a door a
creature opens -- a lamp pulsed 0.2 s when a player dug the grass an observer
watched, in Xephyr. Watches are not saved; an LBM asks again for each
observer when its chunk loads, and after a restart a write to a watched cell
lit the lamp 0.1 s later. A box's picture is read with its first row at the BOTTOM of
the box.

**Hunger, torches, cane, books and enchanting (2026-09-25).**
- `mods/hunger` (side both): twenty points of food and a hidden saturation,
  The classic block game's numbers: walking 0.01 a metre, sprinting 0.1, a jump 0.05, a
  block broken 0.005, a blow landed 0.1 (`core:hit`, a new event), a point of
  health regained 6; four of that take a point of saturation, or of food when
  it is gone. Food gives both (bread 5 and 6, cooked meat 8 and 12.8, ...).
  Under 18 health does not come back (`core.player_regen`); at 0 a point of
  health every 4 s, down to 1. Kept by name in the mod's storage. The bar is
  drawn by the mod (`core.hud.aspect`). Measured in Xephyr: 10 s walking 0.48,
  10 jumps 0.50, 8 s sprinting 5.6 (a point of saturation); walking is told
  from sprinting over one-second windows because the server hears positions in
  bursts. Starving checked with food drained fast: health 20 to 1, held there.
- `mods/torches`: coal over a stick is 4, light 14, on the floor only.
- `mods/cane`: a stage after the lakes puts 1-3 cane on grass or sand at the
  water's level beside a lake, 1 shore column in 4 (173 cells in a 256 world
  of seed 1); it grows to 3 tall; 3 cane are 3 paper, 3 paper and a leather a
  book. Cows leave 0-2 leather (`core.mob_drop`, new). Seen in Xephyr: a
  one-hit cow left 2 beef and a leather.
- `mods/enchant` (side both): experience from ore dug (coal 1, cinnabar 2,
  lapis 3, diamond and emerald 5), creatures killed (3, `core:hit`) and
  smelting (mods/furnace keeps the classic block game's 0.7 an ingot, 0.1 glass, 0.35 food
  in the furnace's meta and gives it, as the event `furnace:xp`, to whoever
  next opens it); levels as the classic block game's; a green bar over the hunger row. The
  table offers I, II, III: level L costs L lapis and L levels and draws one
  kind that fits the thing in hand at L (or one less, 3 times in 10) and, 2 in
  10 a level, a second; nothing is lowered. A stack keeps up to four kinds at
  once (`ItemStack::ench`, two bits a kind; protocol 62, bag format 4, format 3
  converted): Efficiency digs 1 + 0.5 a level faster, Sharpness a point
  harder, Unbreaking wears one use in level + 1, Protection a point of armour.
  Measured: stone 0.35 s a block, 0.15 with Efficiency III; out of a program
  against the engine's library, a chestplate of durability 240 lasts 240, 480,
  737, 974 uses at Unbreaking 0-3 (it lasted for ever until the draw stopped
  repeating itself -- fixed) and a blow of 10 takes 8, 7, 7, 6 at Protection
  0-3; in Xephyr, 40 points from a furnace and draws giving Efficiency I, then
  Efficiency II with Unbreaking II. Found on the way: hunger and experience
  read each other's messages (`core:net_message` reaches every mod's handler;
  both now check `ev.mod`), and hunger charged a join's jump from 0 to 20
  health as regeneration. Since then the table shows a clue for each level
  (the main kind and level), from a seed of the player's own (the mod's
  storage) that moves on only after something is enchanted, so the clue is
  what is given: in Xephyr "I: Efficiency I?" gave Efficiency I and the clues
  changed after. The seeds a level apart first gave three identical clues --
  xorshift's first numbers from close seeds are close; the first 8 are
  thrown away now. The panel is built per player (`enchant:choose<id>`).

**OPEN-35 -- the world is made as players go; what it still lacks.** Since
2026-09-15 the server's world has no edge. It generates the region of `--world`
(128) and a ring of columns round it before the mods load; any other column
(32 by 32, full height) is read from the world file or generated when a session
comes within 4 columns, lit once the 8 around it have blocks, and only then
sent, saved or swept by an ABM (`core/world/columns.h`). Every stage, C++ or
Lua, runs once per column and writes only inside it (`ctx.min_x..max_z`);
`mods/ore`, `mods/cave` and both games walk what reaches in from beside.
`core.biome(x, z)` is new and `mods/trees` grows trees with it (364 over the 36
startup columns of seed 1, none on the plaza). Since 2026-09-15 (third pass) the
content the entry listed as missing ships: `mods/lakes` (water below y 18: 9.1%
of the columns of a 1024 region, at most 11 deep), `mods/ruins` (a brick tower in
24 of its 64 squares of 128), `mods/furnace` (coal smelts iron ore into ingots
and sand into glass, one every two seconds -- 3 coal, 2 ore and a sand became 2
ingots and a glass, and it went dark), and `mods/cave` joins every square's
tunnel to the next square's along x, and along z one square in two (6486 cells
cut in a 128 world, 4937 before). New engine calls: `core.node_liquid`,
`core.container_take`; protocol 38 tells every client what each block does to a
body, so a client without the mod swims too. Swimming checked with a real client
on a server where nobody flies: a scripted walk through a shallow lake went into
the water and out of it twice and reached its last point, 0 corrections on
either side. Played by hand on 2026-09-19 (real keys and mouse, see "A played
session" below): swam out of the lake at 92 140 onto its shore holding Space;
the lake 9 deep at 40 -104 was not tried. Measured, Release,
shipped mods: a client flying 900 blocks at 12 m/s had 0 corrections and ground
under it at the end; 408 columns filled at 1.7 ms each, 333 lit at 3.4 ms each
(10.8 before the light pass looked a chunk up per chunk rather than per cell).
Debug lights a column in 31 ms. Still missing:
- Since 2026-09-17 water MOVES. A lake is made of sources (`lakes:water`,
  level 7, never swept, so a thousand still cells cost nothing); what comes out
  of one is flowing water, `lakes:flow1` to `lakes:flow6`, one block per level.
  An ABM twice a second gives every flow one step -- down into air under it (and
  what falls arrives at level 6, so a waterfall spreads at the bottom as far as
  it would have at the top), or sideways one level weaker -- and drains any flow
  with nothing wetter above or beside it. Two things open a way for water and
  nothing else does: a dig beside or under it (`core:broken`, which steps the
  water and not the hole) and a bucket poured. Measured offline, seed 1: a
  source on a stone table wetted 5 cells after 0 s, 42 after 1 s, 52 after 2 s
  and 68 after 4 s, the middle row reading 3 4 5 6 7 6 5 2 1; the source taken
  away, it was down to 47 after 2 s, 20 after 4 s and dry after 6 s. Into a
  natural lake's shore: a pit of 60 cells behind the shore went 1 wet, 3, 19,
  53, 59 over 7 s. The flow ABM cost 0.24 s of CPU over 13 s of world with 68
  wet cells in it (1400 fires a second at its busiest), and a settled pool
  writes nothing because every cell it would write into already holds water.
- Since 2026-09-17 a bucket (`lakes:bucket`, `lakes:bucket_water`, three iron
  ingots in a V -- the recipe is in mods/furnace, where the iron is) fills from
  the first source along the player's look and pours a source into the last air
  cell before whatever stops it, and what it pours starts flowing at once. The
  look is walked cell by cell in Lua rather than asked of `core.raycast`, which
  finds the first SOLID block and so answers with the lake bed. Checked in the
  single-process client with a mod that clicks for the player: a source two
  nodes from the eye went dry when the bucket filled, came back when it was
  poured, spread a level-6 flow beside itself, and a second pour with an empty
  bag changed nothing. Since 2026-09-19, from play: a right click that reaches
  no solid block within 6 nodes but does reach water is sent as a use of the
  water (`raycast(..., liquids)`), so a bucket fills from a lake whose bed is
  out of reach -- before, the click never left the client; the bucket's own
  walk reaches 6 nodes like the hand, not 5; and the cell a bucket empties is
  stepped like a dig, so the lake closes over it instead of keeping a square
  hole. Seen with real clicks: filled at 92 17 141 and 92 17 147, poured back,
  the surface whole again. Water is not conserved (a lake feeding a channel
  does not fall), as in the classic block game, on purpose. Since 2026-09-25 a flow is waded
  and not swum unless its surface is more than half a node over the feet
  (`box_in_liquid` reads the fill; a full cell counts as before). A cave under
  a lake does not happen: mods/cave keeps six of rock under the ground, and
  seeds 1 and 7 at 1024 across have no source over air; a dig up into a bed
  opens the way as any dig does.
  Since 2026-09-19 `core.take_item` and `core.give_item` go to the hand
  first (`Inventory::take_by_item`, `give_to_hand`), and the server's bag
  follows the slot of every click: with two full buckets, pouring the held
  one empties that one and leaves the other full -- seen in play.
- A furnace smelts only while its column is in memory; how far the cave network
  actually connects was not measured.
- Since 2026-09-17 a tower holds a chest in its inside corner (`chest:box`, so
  mods/chest opens it and pouring it out on a break is already handled): coal,
  now and then ingots, and an iron pick in one tower in four, from the tower's
  own hash. Measured on a 512 region, seed 1: three towers, holding
  6 coal + 2 ingots, 5 coal + 1 ingot + 1 pick, and 7 coal + 1 pick, each read
  back out of the container rather than counted as it went in. Opened in play
  on 2026-09-19: the tower at 147 173, chest at 150 18 178, 6 coal and 2
  ingots, both dragged into the bag. Since 2026-09-28 a chest is stocked once and the mod's
  storage remembers it; one made in the region `--world` generates, before the
  file is read, waits for core:world_ready to ask. Two starts on one world
  file, `--world 512`: 3 chests stocked (6 coal + 2 ingots, 5 coal + 1 ingot +
  1 pick, 7 coal + 1 pick), then 0. With no world file nothing is kept, the
  chests included, so each start stocks them afresh.
- A stage changed between two runs changes only the columns nobody visited yet,
  and the seam shows.
- `core.get_block` on a column not in memory reads air; since 2026-09-25
  `core.loaded(x, y, z)` tells that air from a real one (mods/bed uses it). A
  write waits for its column and is applied when it is lit.
- A listening server spends 20 ms a tick on columns and at least one step; in
  Debug a step is 31 ms, so ticks stretch while a player crosses new ground.
- The welcome's hash and `world hash` cover the region only.

**OPEN-38 -- letting go of the world.** Since 2026-09-15 a column untouched for
30 s and more than 8 columns from every session is let go: a lit one is kept
(in the world file, or encoded in memory without one, about 2 KB a column) and
every session that holds its chunks is told. Protocol 37 `ChunksGone`, on the
chunk channel, also carries what a session's own interest forgets; the client
erases those chunks and their meshes. Measured, the same flight: the client held
198 chunks at most and let 468 go; the server ended with 170 columns in memory
and a 1.69 MB world file. With no world file, a client joining at the spawn
after that region was let go got its ground back from memory.

Since 2026-09-15 (second pass) the same rules are one class,
`core/storage/column_keeper.h`, used by the server and by the single-process
client, which has no edge either. Without a world file only a column something
changed is kept (an unsaved chunk after it was lit): the same flight kept 4
columns in 8.8 KB, against 332 in 660 KB. A stack, a mob or a mod's thing keeps
its column in memory and gets one made if it walks into one. At most 12 columns
are let go a call. Measured, Release, `cnp_client --soak 90 --soak-far 16`
(1440 blocks out of the box, editing every frame): 178 columns in memory from
40 s on, RSS +0.1 MB from 30 s to the end, p99 7.3 ms, worst frame 53 ms,
columns at most 7.5 ms of a frame. Found on the way: the client batched the light
of a stage's writes, relighting every new column a second time in boxes merged
hundreds of blocks wide (150-230 ms a frame, and every such column then looked
changed and was kept).

Since 2026-09-17 a write brings back the blocks its relight will read
(`ColumnKeeper::wrote`): a relight reaches sixteen blocks, a column that was let
go reads as air, and sunlight poured in from the side into the border of a lit
column and was saved that way. It is a lit column whose ring has gone idle --
a mod or a modifier still writing where nobody is, thirty seconds on. Measured
with a mod that keeps a lamp blinking in an air pocket at 1985 10 100, two
blocks from its column's border, on a listening server nobody joined: with the
ring let go, the sunlight at 1984 10 100 went from 0 to 14 (15 in the column
that had gone), and with the fix it stays 0. The four corners of the relight's
box name every column it reaches, they are filled and not lit, and they go idle
and are let go again; the server's stats line counts them ("brought back for a
relight to read", 3 in that run). Still missing:
- Without a world file the changed columns stay in memory for the life of the
  process.
- Since 2026-09-25 a player's own edit in a local world (`try_place`,
  `try_break`) goes through the keeper like a mod's write.

**A way through the blocks (2026-09-24).** `core/voxel/pathfind.h`: A*
over the cells a body could stand in, for any body -- height in cells, how
far it climbs and falls in one move, whether water holds it up -- to the 8
cells round, never through a wall's corner, bounded by a budget of cells;
with no way it returns the way to the closest cell. `core.find_path` gives it
to mods as a flat x y z list. The creatures plan with it every 0.75 s (600
cells) and jump the step when the next cell is higher; the sidestep is left
for when there is no plan. Checked with a throwaway bench (not kept, per the
test rule): open floor, round a wall through its one gap, the closest cell
when there is none, a one-node step up and not two, a drop of three and not
five, no corner cutting, head room, water with and without swimming; a
hopeless search looks at ~900 cells in 1.1 ms. Seen in Xephyr: a zombie
behind a wall with one gap walks to the gap and round to the player. Open:
cells only (a slab is a full block to it), no doors or ladders, no cache of
plans between creatures.

**Hanging from ledges and pulling up (2026-09-24).** In the air, pushing
towards a wall whose top the hands reach (1.0 to 2.1 over the feet, two open
cells over it, open over the head), a player grabs it: the feet hang 1.85
under the edge against the wall's face (find_ledge, core/physics/aabb.h).
Hanging, a new press of jump pulls up and over in 0.6 s (up first, then onto
it, never through the block's corner), left and right move along the ledge
at 1.2 m/s while there is ledge, crouch lets go (no regrab for 0.5 s). A jump
at a wall two high takes it on the way up. The server counts a body at a
ledge as held (body_at_ledge) and a grab ends a fall. The robot has `hang`,
`hang_shimmy` and `pull_up` (added with tools/gltf_add_clips.py); somebody
else is seen hanging when nothing is under their feet and a ledge is by
their hands. Seen in Xephyr: a jump at a two-high wall, hanging, a second
jump up onto it, 0 corrections; and at a wall three high, one jump to hang
at the top of it and a second up, also standing still against the wall with
no key but jump (a body touching the wall it faces grabs without pushing).
Since the same day the arms seen in first
person have `hang`, `pull_up` and `climb` (robot_arms.gltf, added with
tools/gltf_add_clips.py), the view turns at most 135 degrees either way from
the wall while hanging or pulling up (270 in all), and a body hanging --
one's own and, told by the side its ledge is on, anybody else's -- faces its
wall whatever the camera does. Seen in Xephyr in first person: the hands on
the edge, the view stopping short of looking back either way, the arms
pushing down as it pulled up. The arms stay on the ledge as the view turns
(drawn for a view facing the wall and turned into this one). Open: creatures
do not hang.

**Climbing onto a block one high (2026-10-02).** On the ground, walking into
a block whose top is over a step (0.55) and at most 1.0 above the feet, with
room on it, crouch not held: the same pull as from a ledge, from the ground in
0.35 s (kStepUpTime). The robot has `step_up`, made in robot18.blend from
pull_up's last half (hands on the edge, pushing down, knees up) and added with
tools/gltf_add_clips.py; first person keeps the usual arms. Seen in Xephyr on
a server: up the stone pyramid four steps in a row, 0 corrections; with Ctrl
held it stops against the step. Somebody else's is told by where they are,
as a hang is: rising, nothing under the feet, and a block's top over them by
at most a node along the axis they face; the clip then plays out. Somebody
else dropping off a block no longer flashes `hang` (a body falling faster
than 1 m/s is not holding a ledge). Seen in Xephyr from a second client, the
climber walked by --walk-path: step_up facing the block, 0 corrections. A jump
at a block one high is drawn as a step up.
Fixed 2026-10-04: a stair was climbed as a block from its front too --
find_ledge took the top of all its boxes. It now takes only the boxes the body
walks into (across its width, within the hands' reach): a stair is a half step
from its front, a block one high from its back (physics_box checks both).

**Turned shapes and climbing clips (2026-09-24).** The way through the
world reads each block turned the way its cell is (node_shape_box, the boxes
bodies collide with), once per block and orientation, and by QUARTER of the
cell: how high the boxes stacked from the floor reach, where a box above that
stack begins (a ceiling), and what is stood on from the cell above. A move up
is walked when the feet rise at most a step to the floor over there on the
side they come in by, and another to the rest of it: a stair from its front
is two half steps, from its side or back a jump; an upside-down slab is stood
on and not walked under; a slab on its side is half a wall. Bench 33/33;
0.49 us a cell. The robot, the zombie and the skeleton have `climb` and
`climb_idle`, played on a ladder off the ground (climbing while moving up or
down, holding otherwise; a model without them falls back to jump_loop). The
robot's two were added to its .gltf by `tools/gltf_add_clips.py`, which adds
named clips from one glTF to another by joint name: robot18.blend no longer
exports the committed robot.gltf as it was. Seen in Xephyr: the robot
climbing in third person and standing at the top; a zombie coming up the
ladder with its arms over its head.

**Real clouds (2026-09-24).** Above y 30 a placed block no longer turns into
`guard:cloud` (still registered, for worlds that hold some). The sky has a layer
of cumulus between y 96 and 112 instead: kCloudFS marches 12 steps per sky pixel
through a 4-octave noise field whose columns rise from a flat grey belly to a
lit dome, drifting on the server's wind (still while a gate captures). It writes the depth of the first cloud it meets, so hills hide it
and, from above, it hides the ground and the lakes. Faded into the sky's own
colour by 420 nodes, darkened at night, tinted at dusk, faded by the water in
front; off in a world with a fixed sky (the Inferno). Seen in Xephyr from the
plaza and from y 145. portal_continuity back to 0/242900 at 1.5 blocks: a step
in the density at the base had put 2 lone pixels through the opening. The wind
is the server's: ServerSessions keeps how far it has carried the clouds
(kWindSpeed, 1.2 nodes/s) and sends it with the time (protocol 59, Time grows to
7 bytes); a client runs it on at the same speed between tellings. F3 shows it.
Two clients on one server, the second joined 15 s later, screenshots 7 ms apart:
both `wind 399.3`, the server's 333 s of uptime. A single-process world runs its
own. Open: the wind restarts at 0 with the server (not saved); a sky pixel costs
~48 noise lookups, not measured on a slow GPU.

**Pets, and the kart and flight in first person (2026-09-24, from play).**
`mods/pets`: a dog and a cat, boxes in Blender (`tools/make_pet_models.py`,
`~/kart_assets/pets.blend`, clips idle/walk/run/sit, a `_tame` twin with a red
collar). Engine: fight `"pet"` (core/ecs/mob.h) and `core.mob_tame`. Pets come
out in sunlight 12+ (a third of the cap, counted apart from the night's
creatures, which now pick only among kinds that fit the light), stroll, stop
and look at a player within 4, never burn. A right click on a body is a Hit
with `use` set (protocol 60, kMinProtocol 60): `use_pet` tames a wild one with
its item (one spent, the bag sent back) or sits/stands your own; the reach is
strike's, now `ServerSessions::in_reach`. Tamed: follows past 2.5, runs past 5,
put at the owner's feet past 14 or 6 up/down; sitting is Working, which plays
`sit`. Seen in Xephyr: a dog tamed with a bone (2 -> 1), collar on, following,
sat and stayed while the player walked off; a cat strolling.
First person: in a kart the body is drawn at the eye with the rider's head
hidden, the eye at the head bone (+0.12) and looking 0.3 rad down, so the
wheel and the hands on it (drive_idle/left/right, as in third person) show;
the floating arms are not drawn. In flight or any time off the ground the
arms no longer bob with the walk.
Then, the same day: tamed pets are kept in the world file, one "pet:N" key
each in the store the mods keep theirs in (under "core": "kind x y z sitting
owner"), written at every save and at the stop, put back when the server
starts ("1 tamed pet(s) back"; seen following its owner again after a
restart). A creature is aimed at by its model's box (`creature_box`: its
height, square as wide as its longest reach) instead of 1.8. A local world
tames and sits them with the same `use_pet`. The cat takes `pets:fish`,
caught with `pets:rod` (three sticks corner to corner; a right click on water
along the look, one in three, a cast per 1.5 s; seen: 2 fish in 8 casts).
Icons for rod, fish, bone and flesh (`tools/make_pet_textures.py`).
And then: a kind's body comes from its model's file (`size_mob_kind`: as tall
as the model, as wide side to side, never more than 1.8 x 0.3), so a dog is
0.94 tall and walks, paths (one cell of room) and is struck as that;
`move_body` and `body_box` take a height, a player's by default. The local
world aims at creatures by their model's box too. Fishing is a cast and a
wait: a float (`pets:bobber`, a non-solid block drawn by a Blender model) on
the water, a bite 3-10 s later (pulled under, "A bite!"), 1.2 s to reel in;
late, it floats again; early, nothing. Floats left by a stop are cleared by
an LBM. Seen: a fish reeled in on a bite, a bite missed and floated again, an
early reel with nothing. `core.held_item(player)` says what a player holds
(the server's Hold; the local bag), and a float goes when the rod leaves the
hand (seen: slot 2 pressed, float gone). Seen in Xephyr: a wild dog spawned
in a corridor one node high under glass walked two nodes along it and stood
looking at the player (a 1.8 body spawned there cannot move).

**Wild animals (2026-09-24).** `mods/animals`: a pig, a cow, a sheep and a
chicken, boxes in the classic block game's pixels (1/16 node) built in Blender by
`tools/make_animal_models.py` (`~/kart_assets/animals.blend`, clips
idle/walk/run; the chicken flaps as it runs), reusing the pet script's helpers.
They are pets with no `mob_tame`: out by day on sunlit grass, stroll, never
burn, drop pork x2, beef x2, wool, a feather (icons in make_pet_textures.py).
Engine: the day's cap is now the whole cap (6 near each player), not a third.
Seen in Xephyr: 6 came out by day on seed 1, cows and chickens walking.
Then: panic, shearing, eggs. A wild pet struck or shorn runs from there for
`kPanic` 5 s at 1.6x, a new heading every 0.7-1.3 s (`Brain::scared`, set by
`knock_back`). `core.mob_shear` (a `Fleece` on the body, so
`ServerSessions::use_body` needs no kinds): `animals:shears` (two iron ingots
corner to corner, not spent) leave 2 wool, `sheep_shorn.gltf` for 300 s.
`core.mob_lays`: a chicken lays `animals:egg` every 120-300 s and drops one
more at death (`Drops::also`). Item names for all kinds resolved in one place,
`find_mob_items`. Seen in Xephyr: sheep shorn, went bare and ran, 2 wool
picked up; chicken with test laying 3-5 s dropped eggs; killed, feather+egg in
the bag.
Then breeding, food, wear. `core.mob_breed` (a `Breed` on the body): fed, a
grown wild pet is in love `kLove` 30 s, goes to the nearest of its kind in
love within 8, and where they meet (< 1 node) a young one comes out (`Brain::
grown_at`, half the body, `_baby.gltf` models at half the pixels, no drops, no
eggs, no shearing, grown in 1200 s); neither again for 300 s. Cow and sheep
eat `core:grass`, pig and chicken `animals:seeds` (1 in 8 grass blocks dug).
Movement now uses the body's own size (`Brain::half/height`), not the kind's.
Food in mods/animals: a right click (core:used) eats raw pork/beef +3, egg +1,
cooked (mods/furnace smelts them) +8, fried egg +4; "Not hungry" at 20.
`Inventory::wear_slot`: shears (durability 238) spend one per shearing.
Seen in Xephyr: two cows fed grass met and a calf came out between them; a
cow struck by a stray click ran off (panic from a blow); health 10 -> 13 ->
16 eating two pork. Later seen too: 17 grass blocks dug by hand gave 1 seed;
one shearing put the green wear bar under the shears; a calf with `grow` set
to 20 s by a test mod was a grown cow 20 s on.
Then: hearts, a parent, big heads. In love, a `Hearts` body (a Thing wearing
`hearts.gltf`, three pixel hearts) rides over its head, bobbing and turning,
and goes with the love (`mob_breed`'s new `hearts_model`). A young one keeps
`Brain::parent`, the first of the two, and trots back (1.3x) when 3-24 nodes
off. `_baby` models have the head 1.5x about the neck. Seen in Xephyr: hearts
over a fed cow, then both; a big-headed calf between them; the hearts gone a
moment after (11 -> 9 bodies); the calf by one cow for 30 s while the other
wandered. Open: the body is clamped to 0.3 half-width like every creature.
Then creatures stop standing in each other: each tick every pair closer than
the sum of their `spread` (the model's own half width, 0.1-1.0; halved for a
young one) is pushed apart at `kCrowdPush` 6 m/s per metre of overlap, through
`move_body` so a wall still holds. Who touches whom is found through columns
of `kCrowdCell` 2 m (twice the widest spread), each creature weighed against
the 3x3 columns round it: linear in the creatures, not quadratic.
Lovers now breed within 2*spread + 1.5 (The classic block game's 3 blocks), since the crowd
never lets them touch and a third may stand between. Seen in Xephyr: 4 cows
spawned on one spot stood side by side 5 s later; two of them fed, with one
between, made a calf. With the columns: the same, and two pigs spawned 4 cm apart
across the line between two columns (x = 64) came apart. Open: a circle, not the body's length: two cows nose to
tail still overlap a little; players are not pushed, nor push.

**A step up is smooth to the eye (2026-09-24, from play).** Walking onto a
slab or a stair lifted the body half a node in one frame and the camera with
it. The physics is unchanged (the server checks it); the view lags instead:
`StepSmooth` (core/physics/player.h) draws the eye that much lower and pulls
it up with a critically damped spring, most of it in 0.15 s. Only a rise on
the ground, both sides of the move, of no more than a step. The body in third
person rides with the eye. physics_box_test walks a stair of four half-node
steps at 60 Hz: the body rises 0.501 in a frame, the eye at most 0.073. Open:
not driven by hand in Xephyr; a step DOWN is still a short fall, not smoothed;
other players are not smoothed beyond their interpolation.

**Slabs, ladders and doors, and a way through all three (2026-09-24).**
A block may be climbed (`core.node_climbable`, travels since protocol 53): a
body in its cell has no gravity, rises 3 m/s with jump, holds with crouch,
slides 1.5 m/s otherwise; the validator counts a ladder as holding a body up
and a climb down is not a fall. `timber:rungs` is now a ladder (a thin panel
against what it was put on; seven sticks in an H make three). `mods/doors`:
a wooden door two cells tall (Blender, `door.gltf`/`door_open.gltf`), six
planks, opened and shut with a right click, both halves together. A door is
two blocks the engine knows as one (`core.node_door`). The way through the
world reads blocks by shape: full, low (a slab: stood on, a half step up
without a jump), thin (a ladder's panel, an open door: a body fits beside
it), door; it goes up and down ladders, through open doors, and treats a
shut one as a wall unless the body opens doors (`register_mob`'s
`opens_doors`; the creature opens it, both halves, when it gets there).
Throwaway bench 27/27 (the 17 before, and a slab staircase with no jump, a
ceiling over a slab, a ladder up and down and not for a body that does not
climb, a shut door, an opener, the door as a corner, an open door). 0.45 us
a cell looked at, against 0.35 before shapes. Seen in Xephyr: a player
climbed a ladder 3 m/s onto a tower with 0 corrections; a zombie climbed the
ladder and came over its top; a zombie walked up slabs onto a platform two
high; a shut door kept a zombie out of a closed room for 24 s, and a kind
that opens doors opened it. (Shutting it behind, turned shapes, stairs and
climbing clips: done since, above.)

**The way through the blocks, faster and straighter (2026-09-24).** Blocks
read through a cache of chunk pointers and the searched cells kept in a dense
list with an open-addressed index, both reused between searches: 1.03 to 0.35
us a cell looked at (140-cell way through a 96x96 field of pillars, 4.0 to
1.5 ms). `hurry` weights the estimate: 1.2 looks at 3.5x fewer cells for a
way 0.4..2.5% longer, 1.5 at 12x fewer for 2..4%. `path_smooth` keeps only
the corners: a leg is straight when the cells under the body's middle and
both its sides (0.3 either way) are standable at one level. A diagonal that
climbs or drops now needs the corner cells clear over the whole rise. The
creatures plan in a hurry (1.5) and steer at the corners; `core.find_path`
takes `hurry` and `smooth`. Throwaway bench 17/17 (the earlier 13 plus
smoothing on open floor, round a wall, past a pillar, a step kept, hurry).
Seen in Xephyr: the 17-cell way round a wall became 3 corners, and the zombie
walked them.

**Creatures that hunt (2026-09-24).** `core.register_mob` declares a kind
(model, health, speed, "melee" or "archer", damage, reach, rate, drop, arrow
model), `core.spawn_mob` places one; `mods/mobs` registers the zombie (melee,
20 hp, 2.2 m/s, 3 a blow every second) and the skeleton (archer, 16 hp, 3 an
arrow every 2 s from up to 15). The engine (`core/ecs/mob.cpp`, server only)
drops them onto the ground with move_body, jumps them over a node, sidesteps
for 1.2 s when a wall stops them, goes for the nearest player within 16,
swings within reach or keeps an archer 6..12 away shooting while it sees them;
an arrow is a Thing that flies under gravity, hurts the first player it
meets or sticks in a block for 8 s. Every 2 s creatures come out where light
is 7 or less, 18..40 from a player, up to 6 near each; any 72 from every
player goes (`--no-natural-mobs` turns the coming out off). A creature swings
or draws with the Working tag (protocol 52), which plays its "attack" or
"shoot". `--mobs N` now puts N of the first kind round the spawn. Seen in
Xephyr on a server: a zombie walks up and swings, a skeleton draws and its
arrows land, a player standing still dies and comes back; at midnight six
came out in 40 s. Open: creatures in a column not in memory wait there
(drops, knockback, sun and local worlds: done since, above).

**The creatures' round closed (protocol 54, 2026-09-24).** A zombie leaves
`mobs:flesh`, a skeleton two `mobs:bone` (no use for either yet). A blow
knocks back both ways: a creature struck by a player goes 4 m/s away and 3 up
(`knock_back`, about a node and a half), a player struck by a creature gets
the same as `net::Push`, a speed their own physics spends (an arrow half of
it; not in a kart or hanging). Under the open sky by day (daylight over a
half, sun light 15 at the head, not in water) a creature loses a point a
second. A creature that opened a door shuts it once two nodes past it and
nobody stands in it. And all of it in a local world: the kinds, core.spawn_mob,
coming out and going, hitting them, their models and clips -- the server's
code (make_mob_kind, swap_door and step_mobs, shared). kMinProtocol is 54: a
53 client dropped the server on the unknown Push. Seen in Xephyr: local, a
3-hp box struck three times dies and leaves its stack; a zombie at noon dies
19.5 s after coming out; a door opened at t=5 was shut at t=6 with the
creature through; on a server a zombie and a skeleton knocked a player from
z 64.5 to 71 and back, 4 deaths in 25 s, 0 corrections.

Then (protocol 55, the minimum too): a creature in the sun is Burning,
streamed as 4 in the Entity flags, and drawn with three small flames rising
round it; one with nobody to go for walks to the nearest cell within 8 whose
head is out of the sun (looked for every 2 s) and stops burning there; a kind
with no damage neither hurts nor knocks. Seen: a zombie at noon 20 nodes off
lived 30 s among the trees, and died in 20 s with the shade search switched
off; flames on a creature local and over the wire; a harmless kind beside a
player for 10 s left them where they stood, 0 corrections. Not done: the
local creatures are one ECS for every world, but no local scene has both
creatures and a second world (--dual loads no mods; it crashed at start on a
second world it never made, fixed). Open: a creature chasing somebody burns
rather than hides.

**A zombie and a skeleton archer (2026-09-24), models only.** `mods/mobs`,
made in `~/kart_assets/mobs.blend` on the robot's skeleton (same bones, 1.8
tall) and exported by `tools/export_mob_models.py`, clips as NLA tracks. Both
have idle, walk, run, jump_loop, swim, swim_idle (the robot's, the zombie's
with its arms held out); the zombie adds `attack`, the skeleton `aim` and
`shoot` (a bow in the left hand, an arrow shown only while drawn). `--mobs`
creatures now wear the zombie. Seen in Xephyr: eight zombies, then eight
skeletons, walking their circle on a server. Nothing plays attack/aim/shoot
yet: there is one kind of creature, a wanderer on a closed-form circle, with
no AI, no kinds, no arrows (all three since, above). The creatures seen at
(0, 20, 0) were a test that passed a file to `--world`, which is the world's
extent. The hit box was 1.4 tall against the 1.8 model; it is 1.8 now.

**Chest and furnace are Blender models (2026-09-24).** `chest.gltf`, a
two-cell `chest_double_{l,r}.gltf` and `furnace{,_lit}.gltf`, made in
`~/kart_assets/chest_furnace.blend` and exported by
`tools/export_block_models.py`, which also names the clips `open`/`loop`. A block model holds its `open` clip at how far
the cell is open and plays `loop` on the clock; an emissive material is drawn
unshaded; the rest takes its cell's light by the rule a box does. Two chests
side by side, same facing, join: the older one's container
grows 27 -> 54 in place (`container_make` now grows, never shrinks), the other
cell is `chest:half_*` wearing one hidden triangle. `core.node_drop` makes the
halves and `furnace:lit` break into a chest and a furnace. Seen in Xephyr,
local and networked: join by placing, 54-slot panel (its squares shrink to
fit above the bag), lid swing, break back to a single, fire moving. Found on
the way: in a local world `core.node_model` read the Scene as a Session and no
model block had ever been drawn there. Breaking the secondary half keeps the first 27 slots and spills the rest
(`container_spill` takes a first slot and cuts the container there). Limits:
breaking the primary spills all 54; a third chest beside a double does not join; the double's other half
is found by facing, so `core.set_facing` on one half breaks the pair.

**Armour and worn squares (protocol 58, 2026-09-24).** A player's bag is 40
squares: the 36 and four worn ones (head, chest, legs, feet), a column left of
the bag. `core.register_item` takes `wear`, `armor` and `durability`; only
the right piece goes in a worn square, `add()` never fills one, shift-click
with nothing open puts one on or takes it off. Blows are cut 4% a point up to
20, never below 1, and every worn piece spends a quarter of the blow (at
least 1) until it breaks; falls are neither. A stack carries `used` (bag
format 2, protocol 58), drawn as a strip on the square; an armour bar sits
over the health bar. `mods/armor`: 16 pieces with the classic block game's points and
durability. `mods/ore` adds gold (1 column in 128) and diamond ore (1 in 256)
in the bottom 10 blocks at level 2 (the iron pick is level 2 now);
`core.node_drop_item` makes diamond ore break into `ore:diamond`. A worn item
is drawn as the model of its own name on the wearer's skeleton, bone by bone
by name: 16 glTFs by `tools/make_armor_models.py` on the robot's rig. Seen in
Xephyr: local world, the wrong piece refused, shift-click on, wood set on the
robot in third person, armour bar 7; on a server, a second client seeing the
first in a diamond helmet, iron chestplate, gold leggings and iron boots, the
bag kept across a server restart. Open:
- The strip for wear was not seen on screen: no creature hit a worn piece in
  the runs above (the rule is checked in storage_test).
- The first-person arms show no pauldron; a chestplate's back is open around
  the jet pack.
- Tools do not wear out, though the field is there.

**OPEN-43 -- what block shapes and model blocks still lack.** Since
2026-09-25 a block that is not solid is aimed at by its boxes (a crop, dust),
which covers what a separate selection box was wanted for so far; a separate
one would change what travels in protocol 36's shapes and was left.
- A model block is not cut at a portal. It poses once per cell per frame.
- Shape, turn and model travel, since protocol 36 hardness, dig level and
  material (and a tool's level, speed and material), and since protocol 38
  whether a block is solid, opaque or a liquid and its opacity. The client does
  not refuse a dig it could now predict.
- `selection_box` is not separate from `node_box` (see above: not needed yet).
- Since 2026-09-25 a dropped item rests on the boxes, and a box that sticks out
  of its cell stops a body in the cell it sticks into (`box_hits_world` looks
  at the ring of cells round a body for a block whose boxes leave its cell).

**CLOSED-27 leftovers -- turning blocks.** Since 2026-09-25 `mods/screwdriver`
(an iron ingot over a stick) turns a placed block to its next turn, through
as many as it has (`core.node_turns(id)`, new: 1, 4, 6 or 24). A wallmounted
block cannot be rotated about its own front. Placement picks one of four
turns from the placer's position only.

**CLOSED-31 leftover -- one material per block and per tool.** No set of groups
with a level each, so a block cannot be both soft and stony and a tool cannot be
good at two jobs.

**CLOSED-34 leftover -- closed 2026-09-25.** Every shipped item has a picture:
the last five with none -- stick, both buckets, palette brush, iron pick --
and the screwdriver are `tools/make_item_textures.py` (tools_pick.png's style;
the iron pick is it with an iron head). That missed four, found on 2026-09-25:
`inferno:cinder_rod`, `inferno:wraith_tear`, `inferno:gold_nugget` and
`infernoportal:lighter` are now Blender renders of their held models
(`tools/make_held_models.py`).

**A played session (2026-09-19).** Half an hour of `tools/play.sh --fresh`
driven with real input: a nested X server (`Xephyr :5`), `xdotool` for keys
and mouse (focus the window first, or the game pauses on the first move; hold a
key 100 ms with keydown/keyup, since `xdotool key` fits in one frame and raylib
misses it) and
`import -window root` for frames, F3's lines read off each frame for where the
player is. Found and fixed: a bucket did nothing over deep water, a fall into a
lake cost as much as onto rock, there was no crosshair, every pictureless item
was the same grey square, F3's lines were written over the chest grid and a
mod's panel, and the engine's stone pillars stood out of every lake and beside
every tower (they now stand on the plaza only; worldgen's pinned hash changed
with them). Everything it left open was fixed the same day: the bucket taking
the wrong one, the opening under water, `furnace:burn`, and the view not turned
back to where it looked on a rejoin.

**OPEN-37 -- single-pixel pinholes.** From one exact camera pose at 1920x1080,
lone pixels of sky show through rock at triangle edges. Not chunk seams. Only
worth a projection jitter if a player ever reports it.

**Frame drops from the Inferno through an opening (2026-09-26).** Reported as a
huge drop while digging grass; not the animals. The Inferno's lava spreads
without pause (about 90 cells a second), every change reached the client
through the observer session behind the plaza's second opening, and each one
cost a 33-cubed relight on the frame thread: up to 298 in one frame, 0.5-2 s
frames in the Debug build the session was played with, 95-139 ms in Release.
The client now skips changes that alter no light (`BlockChange::had`, lava to
lava of another level) and lights the rest through a `LightBatch`, as the
server does: the worst relight in a frame went to 2.4 ms (Release), no frame
over 150 ms in Debug. `CNP_HITCH_MS=<ms>` prints every longer frame with its
profiler sections.

**OPEN-51 -- one held-block cache for every body.** `Scene::held_body` holds
one mesh and one shader; two players in view holding different blocks rebuild
it (a shader compile) for each of them every frame. Seen only by reading the
code; a cache per key when a server with several players shows it.

**Chess (2026-10-06).** `mods/chess`: a table one cell across with the board on
it and 12 Staunton pieces, made in Blender (`tools/make_chess_models.py`, a
lathe for each and a side profile for the knight's head). Right click sits:
`core.fixed_view` (new) flies the camera in 1.2 s to 1 node above the board from
the player's side, the cursor free and the body still and not drawn, the engine
HUD off; "Alzati" in the right-click menu flies it back (ESC is the pause
menu, as anywhere). Pieces drawn by `core.render.model`, picked on the board's plane
from `core.cursor_ray`; drag or click-click; the server checks every move with
the same rules and keeps the games in storage. Rules checked headless: perft 3
from the start 8902, Kiwipete 48 / 2039, fool's mate, en passant, castling,
promotion, stalemate. Seen in Xephyr on a server and in the single-process
client, boards facing 0 and 3: e2-e4 clicked, e7-e5 dragged, check shown on
the king, W held does not walk, ESC back to the eye.
Then the same day: promotion asks (a panel: queen, rook, bishop, knight),
threefold repetition draws by itself (as fifty moves does), and right click
while sitting is a menu -- offer a draw (on the other player's screen at once;
it stands until answered or until they move; one per move), accept or decline
it, resign (asked twice), and before the first move or after the last a clock
(none, 3+2, 5, 10 minutes; it starts after white's first move; out of time
loses unless the other side cannot mate at all). Alone at a board "Patta" ends
it at once and resigning is the side to move's. Panels are per player
(chess:m<id>): a panel's contents are its mod's. Two engine changes came out of
it: world code on a connected client hears `core:tick` now (it never did: the
first "hi" for the boards near a joining player was never sent, so pieces
showed only once somebody sat), and `core.engine_hud_on()` lets hunger,
experience and the territory line hide with the engine HUD. The chess HUD
sits under the board and the clocks beside it, clear of F3's text. Seen in
Xephyr with two clients (:5 white, :6 black): a draw offered, declined,
refused again in the same move, offered back and accepted; knights out and back
twice drawn by repetition; a8=N from the panel; resign; a 5-second clock run
out ("Tempo scaduto al bianco"); the menu and clocks in the single-process
client too.
Then: ESC is the game's pause menu again (`key_escape` taken out of the engine:
nothing else used it) and getting up is "Alzati" in the menu, for watchers too.
The positions counted for repetition are kept in storage with the board (4
knight moves, server restarted, 4 more: drawn), an en passant square counts
only when a pawn can take there, and the font draws accents after all (Latin-1:
"si è alzato" on screen). A clock stops while the side to move has got up from
a game of two (4:48 held still for seconds, "orologio fermo, il bianco si è
alzato") and is saved as it stopped; and once two have played a game, the one
who stays plays only their own colour (black tried d2-d4: refused). Open:
- A table is laid on the spawn's square once a world (storage "plaza"): put
  by hand into worlds/track it was lost to a `--fresh`. Not in the
  single-process client, which has no world spawn to read.
- Nothing makes a player who got up come back: a game of two left half way
  waits, clock stopped, until someone sits or it is resigned.

**Cards (2026-10-06).** `mods/cards`: a card table one cell across and the
Neapolitan deck of forty, for briscola, scopa and tresette, two or four players
(partners facing). The art is Blender's: suits and twelve figures modelled from
primitives, two-tone shading with Freestyle outlines (`tools/make_card_art.py`),
laid out on 200x320 faces by PIL (`tools/make_cards.py`), each card a thin
glTF with its face on top and the back under (`tools/make_card_models.py`).
Same frame as chess: right click sits on the side one stands, `core.fixed_view`
above the table, `core.render.model` for every card, the hand picked on the
felt's plane from `core.cursor_ray`. Empty seats are the table's own players
(simple rules of thumb per game); the server deals and tells each player only
their own hand. Checked headless (bots only, all six games): 60 hands of
briscola at two and four, every one 40 cards and 120 points; whole matches of
scopa (to 11) and tresette (to 21) at two and four, 40 cards a hand, 10 or 11
points a tresette hand. Seen in Xephyr in the single-process client (briscola
and tresette at four, scopa at two with the panel for two ways of taking, the
count at the end of a hand) and on a server with a client (briscola at two at
the plaza's table, facing 2, seat 2).
Then the same day: briscola for three (the two of cups out, 39 cards) and
cinquiglio for five (briscola chiamata: the auction, the called card's holder a
secret partner until it is played, alone when it is one's own; 2/1/1 paid or
won, 4 alone). Seats are evenly round the table for the game (`yaw_of`), not
the four sides. Every game is kept in the mod's storage at every change
(`enc`/`dec`, under 1 KB a table; 4096 is the limit): a tresette at four
played on, the server stopped and started, the same hands and trick back.
Tresette's accusi are counted from the hand as dealt and shown during the first
trick. On a player's turn the cards they may not play are darkened (their own
models, d<n>) and the others come forward. That took the model table past
the welcome's 256 (the client was refused: "this server has 265 models"), so
protocol 86 carries 1024 (kMaxModelTable; the count was a u16 already); and the
mod's world code past one media file (5.7 MB with the decks, 1 MB the limit:
not offered, and nothing of the cards ran on a joined client), so a world's
code package leaves out models, pictures and sounds (`world_code_pack`), which
reach the client as media of their own: 9 mods offered, 58.5 KB.
Then a second deck, chosen in the menu: illustrated by ElevenLabs
(gpt-image-2), a few pictures a day on the free plan, listed with their prompts
in `tools/elevenlabs_deck.json`; `tools/make_cards.py` keys out a picture's
plain background and lays it on the faces, a card not drawn yet keeping the
Blender art, the back blue meanwhile. 6 of 17 drawn on 2026-10-06 (the kings
of coins, cups and swords; the jack and horse of coins, the jack of cups): the
free plan gave three, then "daily image generation limit". 9 of 17 on
2026-10-09 (the horse of cups, the jack and horse of swords; three again, ~185
credits each; the models exported in the open Blender through the MCP come
out identical, only the faces' PNGs change). The 6 and the 9 are
underlined, since one upside down is the other. Cinquiglio's bid and call are one row of buttons now, short
enough to leave the hand in sight, and the menu two games a row. Seen on a
server with a client: 346 models, the illustrated deck's backs and king,
tresette's darkened cards, the bid row over a visible hand.
The table's players count cards where it was measured to pay, 1000 hands a
test against the same bot without it, the seats swapped by a random dealer
(a fixed rotation of the dealer gave the first team 534-449 between equal
bots, a harness artefact): tresette leading a card nothing can beat only in
the last six tricks with the stock gone 684-291 at two and 572-383 at four
(always: 399-601, worse); scopa's risk of leaving a sweepable table by the
cards still out 469-392. Tried and taken out, no gain or worse: briscola
leading a safe ace or three when no trumps are out (471-491), loading a
partner only when their card holds (worse at four), and searching the last
three tricks of briscola for two exactly (504-467 against 509-461 between
equal bots). Rules checked headless for all eight games (40 or 39 cards a
hand, 120 points, cinquiglio zero-sum, kept state read back equal). Seen in
Xephyr: briscola for three, the auction panel, a cinquiglio hand to its count,
tresette's gold edges on a server. Fixed the same evening: on Giacomo's machine no table answered a right click
in tools/track.sh -- the server sat him down (the tables were in its storage)
but the client never moved. A world's code was loaded the first frame
`media_complete()` said yes, which it does before the offer arrives (the offer
is empty then); loaded against nothing, no mod ran and it was never tried
again. With the decks the offer is 2000-odd files and lands after the first
frame of a fast client; his log showed it ("models: 346 named ... 0 from it",
no "world code:" line). It waits for the offer now (`media_version() > 0`).
Xephyr's 15 fps never lost the race, with the old client either.
tools/track.sh keeps the client's output in worlds/track/client.log, and both
table mods log when they are told to sit.
Open:
- The illustrated deck: 8 pictures to draw (tools/elevenlabs_deck.json): the
  jack, horse and king of clubs, the four suit emblems, the back.

## Saving

**A title screen, and worlds kept by name (2026-09-22).** `cnp_client` with no
flags opens on Play, Multiplayer, Options and Quit; Play lists the saved worlds
(last played first, five a page) or makes one from a name and a seed (digits,
any text hashed, or none for a random one). Multiplayer keeps a list of servers
(`host`, or `host:port`, 25301 when no port is given) in `servers`, one a line,
added from a field and forgotten with the row's `x`. Opening the page knocks on
each in turn, one at a time (`ServerProbe`): ENet connects and exchanges keys,
so an answer means a server that is up and speaking this transport, and ENet's
round trip is the ping beside it. The dot is grey before, amber while, green
with the ping, red when nothing answered in 3 s. A join that fails goes back to
that list with the reason instead of leaving the game (a run told where to go by
`--connect` still ends, with the same codes as before). Nothing the menu opens is
anonymous: before Multiplayer or Host (Play too, until 2026-09-26) it asks for a name and a password
(at least 6 characters), and those two ARE the identity --
`sign_key_from_password`, Argon2id over the password salted with the name, 64
MiB and 3 passes, into the seed of the signing pair. Measured: 132 ms, the same
two words give the same key twice, another password another key. The name is
kept in `player` and the password nowhere, so it is typed once a run; the
servers the client starts are given `--require-identity`; and a server now
refuses a name another identity is wearing (`BadName`). With a world file that
is not only about who is connected: a `names` table gives each name to the
identity that first joined under it, and whoever comes back with another key is
refused (`WorldDb::name_owner` / `claim_name`, asked through
`ServerSessions::set_name_check`, so nothing in core/net knows about storage).
Measured: 'tizio' claimed by one key, refused for a second, still refused after
the server was restarted, still let in for the first, and the second key joined
freely as 'caio'. The client keeps the other half: the list of servers stores
the name it played under at each address (`servers`, a tab after the address)
and shows it on the row ("as primo"); going back to a server whose name is not
the one typed at the start asks for THAT name's password, and a "Playing as X"
button changes who this client is. Measured end to end: joined as 'primo',
came back as 'secondo', the row asked for primo's password, the wrong one was
refused by the server, the right one gave the same identity as the first day
and was let in.
Host a server opens the
same list of worlds with a port beside it (25301 by default) and starts that
world's server on every address this machine has instead of the loopback; the
pause menu then says "Hosting at 192.168.1.118:25301" -- this machine's first
IPv4 that is not the loopback -- because by then the title screen is gone and
the player has to read that address out. A world is a directory in
`~/.local/share/cubes-and-portals/worlds/`, served by a `cnp_server` the
client starts on 127.0.0.1 (`--bind`, new) and pins by the key it prints. The
client stops it with SIGTERM on Quit, and `PR_SET_PDEATHSIG` does if the client
dies. The kernel sends that signal once per thread of the client, and the
second one killed the server before it saved: `SA_RESETHAND` is on SIGINT only
now. `--quick` skips the menu for the single-process world, and so do
`--connect`, the scenes and the measurements. Played in Xephyr: made "New Prova"
with seed 42, went in, quit, 142 chunks saved; opened it again from the list,
162 chunks read back; the client killed with -9, the server saved and closed;
a seed that no longer matched the file showed the server's refusal on the list,
3 runs of 3. With three servers listed -- one up, one on a closed port, one
name that resolves to nothing -- the list showed green 1 ms, red and red;
joining the closed one came back to the list saying "timed out -- no answer in
10 seconds", and the one that was up was then joined from the same list. Hosted
the same world on 25301: the socket is 0.0.0.0:25301 (a world of one's own is
127.0.0.1 on a random port), a second client joined it by the address the pause
menu showed and the server logged both players, and hosting again on a port
already in use came back to the list saying "could not bind UDP port 25301".
With a world already being served, a second client asking for the same one is
told "another server is already running on world.db": a server takes `flock` on
`<world file>.lock` before it generates anything and holds it for its life,
because SQLite would happily let two servers open one world and write over each
other's chunks. Joining a hosted world as 'giacomo' from another identity was
refused with "somebody else is playing as 'giacomo' here", and the same client
under another name joined. Found on the way: the client read the
last run's log before the new server emptied it, took its "listening" line for
the new server's and froze joining a server that was failing (the log is
emptied before the fork now); and a player whose first choice was Multiplayer
had no `~/.local/share/cubes-and-portals` yet, so their `player.key` could not
be written and the client stopped -- the directory is made when the menu opens.
Back to the title screen (2026-09-23): a run that began there has "Quit to
title" in the pause menu. It is the client started again with the same
arguments once everything has shut down as Quit does -- the world's server
saved and stopped, the connection closed -- and the identity is handed to the
new run down an inherited pipe, so the password is not asked again. Played in
Xephyr, three times over: back on the title screen each time, the world's server
gone and its 142 chunks saved, Play straight to the list of worlds, joined
again as the same identity; plain Quit still leaves with nothing running.
A world is renamed or deleted from the list (2026-09-23): each row has an
Edit, a page with its name, Rename and Delete, and Delete wants a second click.
Neither touches a world a server holds -- the one this client started, one
hosted from another client, one run from a terminal -- found by trying the
server's own lock (`world.db.lock`) and letting go. Played in Xephyr: a world
with a server running on it refused with "a server is running on it"; another
deleted after the second click; "New World" renamed "Casa mia" and played, its
162 chunks and its player read back from the file.
Somebody else's server pinned on first use (2026-09-23): a row of the list
keeps, after the address and the name, the key the server showed at the first
join (`servers`: address, name, key, tab-separated), and every join after pins
it. Played in Xephyr: joined a server, it was restarted at the same address
with another key file, and the next join from the list was refused -- "its key
changed: x it and add it again to trust it"; dropped and added again, it joined
and pinned the new key, and the join after that said "server key pinned and
matched".
Names a server's owner can see and free, and the menu never still
(2026-09-23). `cnp_server --world-file F --names` lists who owns each name
(name, key fingerprint, since when) and `--free-name NAME` lets one go; both
take the world's lock first, so not while a server has it. The next identity
to join under a freed name takes it and inherits the old one's place and bag
(`freed_names`, read by `claim_name`) -- a forgotten password recovered, or a
name handed on. Measured on a world where 'tester' was ACFC..: freed, joined as
'tester' with a new key 3221..; names, players and player_items all 3221..
after, freed_names empty. In the menu the knock resolves a name on a thread of
its own (`start_lookup`), and the identity is made on another (`std::async`):
"making your identity ..." is drawn while Argon2id runs, a name that resolves
to nothing reads red and `localhost:25396` green at 1 ms.
Play asks for nothing (2026-09-26): a world of one's own is joined with
`player.key` in the data directory, made on first use, under the saved name
("Player" when there is none); its server is on the loopback, where a password
proves nothing. Multiplayer and Host still ask for a name and a password, also
after Play (`TitleMenu::local_identity`, carried down the Quit-to-title pipe as
a leading 'L'/'P'). A world played before under a password knows the name as
that identity's and refused the new key ("'pancione' is another player's name
here"): before starting a world of this machine's, the client frees its own
name there when another key holds it (`WorldDb::list_names` / `free_name`, as
`--free-name`), so the key joining now inherits the bag and the place. The same
for Host, or hosting after Play would be refused. The list of worlds has
"+ Create a new world" as a big green button on top, apart from "Or play a
saved world:" and the rows. Played in Xephyr: Play went straight to the list,
"Puzzonia" (played before as 'pancione' under a password) was joined with the
inventory it had.
Found with it: while a join settles nothing polls the window, so the release
that started it was still there when the menu came back and joined the same
server again by itself; a click in the menu is now a press and a release the
menu saw. Open:
- A server's key is trusted the first time it is seen (as ssh does): a join
  already intercepted on that first day pins the interceptor. A changed key is
  forgiven only by dropping the row and adding it again; nothing shows the key.
- The join itself still resolves a name on the frame thread (only the knock
  does it on its own), and the menu is not drawn while a join settles anyway.
- A server run without encryption never answers a knock and reads as red.
- A password cannot be changed by the player: it IS the key. A forgotten one
  is recovered only through a server's owner (`--free-name`), world by world;
  on a world whose owner is not asked it is a new player there.
- A player cannot give up their own name; only the server's owner can free it.
- An anonymous player (a server without `--require-identity`) is let in under
  any name nobody owns, and owns nothing by it.
- A world with no file remembers nothing: there a name is only held while its
  player is connected.
- The only rule about a password is that it is 6 characters or more.
- Anybody at this machine's keyboard is the owner of its worlds: starting one
  takes the player's name back from whatever identity had it there.
- Multiplayer and Host after Play not yet played through in Xephyr: that they
  ask for the password is read from the code.
- A field is typed into at its end, with no way to select what is in it: the
  remembered name has to be backspaced away to type another.
- The address shown is the first non-loopback IPv4 this machine has; on a
  machine with several networks it may not be the one the other player can
  reach, and nothing opens a port on a router.
- A join that fails after the welcome has been adopted leaves that world's
  blocks in the scene; the next join adopts over them. Only seen going the
  other way (refused or timed out), where nothing is adopted.
- Quit to title closes the window and opens it again: it is the client run
  afresh (`execv`), not the game torn down in place.
- A deleted world is gone: no bin, only the confirming second click.
- The port is random (49152 to 65151); a clash is a world that says it could
  not bind, and the next try picks another.
- The menu is in English, like the pause menu.

**Water that vanished with the tilt of the camera (reported from play,
2026-09-18), and water drawn at its level.** A pool vanished for some angles of
the camera and came back for others. The cause was not the water: a block that
wears a MODEL (mods/steps' marker, on the plaza) is drawn by the model's own
shader in the middle of the world pass, and everything translucent after it --
all the water and glass in sight -- was drawn with that shader still bound. So
the pool went whenever the marker was in the frame. `WorldRenderer::draw` binds
its shader again before the alpha pass. Seen in captured frames from a real
client on a real server: a pool poured on a table in front of the spawn, with
the marker above it, was absent before and drawn after.

On the way, the occlusion walk stopped pruning itself by the frustum. Its
argument was convexity, but it steps face to face through air two chunks agree
about, and where a line of sight crosses a chunk edge the step can have to go
through a chunk beside the line -- rock there, or out of frame -- and the chunk
past it went undrawn. No frame was found where that hid something; on the
spawn of the play world the same 62 chunks are drawn either way, and the frame
went from 1.02 to 1.14 ms (985 to 876 FPS). F2 now turns occlusion culling off
in play, and the HUD says so, for telling the next vanishing apart.

And water has a height (protocol 40): `core.node_liquid(id, opacity, fill)` says
how full a cell is in eighths, the mesher hands the renderer each liquid cell
with the height of its four corners -- the fullest liquid cell meeting at that
corner, a cell with liquid over it counting as full -- and the renderer builds
it in floats, so a surface slopes from the full side to the empty one with no
crack along a shared edge and no wall between two levels. mods/lakes' sources
are full and its six levels two to seven eighths. Seen in a frame: a source at
one end of a sunken basin spreads as a sheet that thins towards the edges.

Since 2026-09-18 (second pass, asked for from play with a screenshot of a lake
that had run across a whole valley):
- **Less water, but water that still runs** (third pass, 2026-09-18, after a
  stream was reported dying three steps down a slope and a cascade was
  reported missing faces). Flowing water spreads sideways only onto something
  that holds it up -- over air it falls, over water it stays -- and never into a
  cell that would stand on water. A fall arrives at level 6 again: a pass that
  let it keep its level made a stream die a few steps down, and was taken back
  out. A source still spreads over water, on purpose: the top of a lake is
  sources on sources, and a hole dug in its side at the waterline would never
  fill. A flow standing on a SOURCE is fed only from above, so the sheets older
  rules poured across lakes dry up; asking that of a flow on any water dried
  the top of every cascade -- it stands on what it pours -- and the cascade
  under it, and the edge made it again: 3 893 cells switching on and off at the
  foot of a staircase, 660 writes a second at rest, and a client relighting
  every one of them at 6 FPS. That was the cascade's missing faces: in any one
  frame part of it was not there. Measured, a source on the top of twelve steps
  hanging over the plaza: 11 032 cells in 40 s and growing under the first
  rules, 249 under the timid ones, now 795, settled by 20 s, with 73 water
  writes in the 30 s after that (the other mods make 147 on their own). On a
  stone table, the source taken away, everything dry in 42 s. And a block
  changed at a chunk's edge or corner now marks every chunk meeting there, not
  just the ones across a face: a liquid's corner and a cube's ambient
  occlusion are read off the cells round a corner.
- **Streams that meet join** (asked for from play, 2026-09-18: a stream stopped
  dead at the first flowing water). Flowing water spreads into a WEAKER flow as
  well as into air and raises it, and the raised cell raises the ones after it,
  so the new level runs on down the other stream and past its old end; a fall
  landing on a weaker flow brings it to 6. A flow may now stand on any water,
  a stream's or a lake's source, and goes no further there -- so a stream
  reaching a lake, or a one-node step over another stream, pours into it
  instead of stopping at the edge. What makes that safe is fed(): a flow is
  kept from the side only by a wetter neighbour that SPREADS (step_water's own
  test), not by any wetter neighbour, so the old special case "a flow on a
  source is fed only from above" is gone and a row of flows lying on a lake
  still dries. Measured offline, seed 1, a thin stream `7 6 5 4 3 2 1` met from
  the side, from above and over a step: `7 6 5 4 4 5 4 3 2 1`, `... 4 4 5 6 5 4
  3 2 1`, `7 6 5 5 6 5 4 3 2 1`, each settled within 10 s; the new sources
  taken away, all three back to `7 6 5 4 3 2 1` by 25 s; a stream on a shore
  one node over a lake ends with a level-2 cell on the lake, and three flows
  laid on the lake with nothing spreading into them were gone by 5 s; 0 water
  writes at rest in the 20 s after. The twelve-step staircase: 724 wet cells
  instead of 669 (a curtain falling on a step now lifts its water to 6 across
  the whole width), settled by 20 s as before. A bucket poured on a lake now
  leaves four level-6 flows round it on the surface. Not done: a raised stream
  that loses its feeder dries and is refilled from the old one instead of
  stepping down in place.
- **Under water, lighter; wet or dry per pixel, by the water's real shape
  (2026-09-19).** Everything the world pass draws, and the sky behind it,
  fades into the water's colour by how much of the line a pixel SEES runs
  through water -- half by ten nodes, never more than three quarters -- and is
  tinted over the first quarter node of it. The line starts at the near plane,
  not at the eye. The water is not a plane at one height: the renderer keeps
  every cell of water within 32 nodes across and 16 up and down of the eye, with
  its surface's corners as the mesher made them, in a texture, and each pixel
  walks its line through those cells -- so a waterfall beside the eye splits the
  view down the middle and a stream's sloping top is where it is drawn. The walk
  checked against dense sampling on 300 random lines through random sloped
  cells: 0.0004 nodes apart at worst. Seen in frames from a real client: eye
  1 cm under a pool, dry above a line 154 px over the horizon (1080 px, near
  plane 0.05), and 154 px under it from 1 cm over; eye 1 cm outside a falling
  curtain, looking along it, the left 42% of the screen in the water (806 px,
  as worked out) and the rest dry; on a flat pool the same pixels as a plane.
  Cost on an RX 5700 at 1080p, eye at a pool's surface: 646 fps, against 1488
  with the plane (1447 with no water near); the walk is cut to the cells that
  hold water, chunks are drawn front to back and the sky is faded only where no
  block covers it.
- **The three things the last pass left.** Each face of a liquid is lit by the
  cell it looks into, like a cube's. The surface is drawn from below as well --
  faces are culled from behind, and a swimmer looking up was looking at the
  sky. And running water's picture moves: across the top the way the corners
  fall, straight down its sides; a full cell lies still. The direction rides in
  a vertex byte that was padding, and the clock wraps every hundred seconds.

Still missing: only the water's top is lit smoothly (2026-09-19) -- its sides,
like every cube's face, take one light each; a diagonal run's picture
jumps a fraction of a tile when the clock wraps; the selection outline seen
through water is not faded (bodies, models, mod passes and openings are, since
2026-09-19); since 2026-09-19 a portal's own camera has the water around
the far end too (a box per opening, so two ends apart do not rebuild one box
every frame), and every shader starts the line at the near plane as a plane
in the world rather than a distance -- a portal's is the far opening, oblique.
Seen at noon: an opening above a lake onto one under it showed the lake bed
crisp grey (92, 92, 97) before and faded (39, 73, 110) after, the same murk
as the lake looked at from the far opening itself (33-41, 74-75, 99-122); the
other way, the dry side seen from under water through the opening, is only
faded by the water in front of it. Its cost on a real GPU was not measured;
the
sky is faded but not tinted; water further than the box (32 across, 16 up or down) is not faded,
except that a line leaving the box inside water counts the rest as water; a
line skimming a big lake costs up to ~130 texel reads a pixel, and a weak GPU
may want the walk to skip empty bricks.

**A saved world from before block names were kept.** Since 2026-09-19 a world
file keeps its block table by name: a mod adding a block is carried over, and a
mod taken out leaves magenta placeholders that come back when it does. A file
saved before that, under another table, is still refused. A placeholder is
re-registered on every start in the order the cells are met, so its id can move
and the chunks holding it are rewritten each time.

**CLOSED-10 leftovers -- what saving still does not do.** Since 2026-09-13 the
server writes the whole world once at startup, then every `--save-every` seconds
(default 30) only the chunks changed since (12 chunks in 2.3 ms; the whole world
is 7 ms at 128 and 25 ms at 256), and saves on SIGINT/SIGTERM. A player's
position is kept by identity (schema 3) and handed back after the first claim.
Checked: saving every second and reloading gives the same world hash at 128 and
256. Still missing:
- A crash loses up to one interval. Since 2026-09-25 a save is one transaction
  (chunks, chests, cell memory and the mods' store, each a savepoint in it), so
  a crash leaves them in step.
- Chests and cell memory are rewritten whole when they change; since
  2026-09-25 a save that finds them as last written (a sum of entry hashes)
  writes neither.
- Stacks on the ground are not saved (they expire after five minutes anyway).
- Since 2026-09-25 a player whose saved spot is now solid starts on top of
  what was built there (up to 32 above), and at the spawn only past that.
  Anyone who arrives by ticket starts at the portal.

## Mods and scripting

**A world of test chambers in the manner of classic portal puzzle games (2026-10-05, protocol 84).**
`games/chambers` (chambers_core: the blocks; chambers_game, side both: the
chambers and what works in them), `tools/chambers.sh` (sandbox and chambers
on one server, as arpg.sh), `tools/make_chambers_assets.py` (textures, the
cube with a one-bone skin and its "death" clip, the two guns held). Agreed
design in the memory: sequential chambers, white panels only, cubes through
portals, a progressive gun. A lobby from the sandbox, six chambers (fixed
portals; cube, vent and floor button; blue gun and a chasm with a fixed
orange; full gun, floor and wall portals, a ledge with the button; aerial
launch plates; a push button and a 12 s clock) and a last room with the way
home. Left blue, right orange; right at a cube picks it up, a click drops
it, left throws it. Engine: creatures cross a world's own openings
(`ServerSessions::cross_mobs`, the players' trigger; one standing on a floor
opening goes in; out where a body of its size fits, the speed turned);
`core.mob_velocity`; `core.player_launch` (a Push that SETS the speed, and
the crossing's grace); every block's light in the welcome -- a client
relighting round a door counted a mod's lamps as dark and a chamber went
black; and the floor shift and funnel for a world's own pairs where its
mouths lead to other worlds. Checked by a scripted server run with a client
in Xephyr (the clicks called with an aim given): through the fixed pair; a
blue shot and through it past the chasm; a floor blue and a wall orange, a
fall through them flung out over the ledge; a cube dropped into the floor
portal out onto the ledge, and on the button opening the door; the plates
over the pit; the clock's door open 12 s. Open:
- Not played by hand with the mouse: aiming, picking a cube up by right
  click, and which chamber is too easy or too hard are untried.
- After core.player_place the client seemed to keep only the yaw of the look
  given (the scripted aims had to be passed in); not checked further.
- Closed 2026-10-05: 03's fixed orange leads, tick by tick, to the blue of
  whoever is nearest their own (of those shot in 03 by players with no
  orange). Two clients: both shot blue on opposite walls; A by theirs, the
  orange went to A's and A came out of it; then to B's, and B came out.
- Closed 2026-10-05: the second launch plate threw harder than the first. A
  launch's Push sets the speed, but the client summed the Pushes of a frame
  into one and lost the flag (ClientSession): a launch now replaces them.
  Measured: from the first plate 93.8 to 101.6 in 1.0 s, from the second
  102.3 to 110.3 (the far floor is 109..112), 2.8 high.
- Since 2026-10-05 (asked from play) a portal stands INSIDE the wall: the
  panels it is shot at become shells a sixteenth thick, open to the room
  (chambers_core:shell_low/high/one, wallmounted), the plane 0.4 into the
  wall's cell; back to panels when it moves. The fixed ones too. A body
  1.8 tall in a hole 2 tall stopped with its head on the wall over it when
  it jumped in, so before a wall opening the client fits its collision box
  into the hole (portal_wall_shift: bottom raised, top lowered, narrowed,
  the eye inside the frame), the server exempts the wall round the opening,
  and a creature moving at one, across it, goes in. Scripted: a jump into a
  gun's blue in 03 (feet 0.3 up) came out of the orange; the whole run as
  before, 0 corrections.
- Fixed 2026-10-05 (from play): a cube in the hands shook. The server put
  it before the player each tick from where it last heard them, in bursts
  and behind their own camera, and it fell a little between. Now its
  carrier's client is not sent it (core.mob_unseen, new) and draws it
  itself, 1.6 before its own eye, the model's boxes; everybody else sees the
  server's. Seen in Xephyr held steady before the view.
- The portal guns are modelled in Blender (tools/make_portal_gun.py, run
  with the flatpak's --background): a white shell, black core and barrel,
  three prongs round a glowing tip, the grip under it, a blue strip each
  side (and an orange one on the whole gun); held pointing into the screen;
  the pictures rendered from them.
- Then (2026-10-05): the gun swung 20 degrees so it points along the view;
  and in Blender (tools/make_chambers_models.py) the storage cube (rounded,
  dark corners, a pink heart on a pale plate on each face, one bone, its
  "death" clip), the automatic door -- two cells wide, two tall, one deep,
  two leaves with a black and orange band and a round window in a frame,
  drawn from its lower cell and turned (upright 3), its other cells hidden
  blocks that stop a body, open the frame alone (core.node_door pairs) --
  and the vent: a white tube hanging from a ceiling cell, the cube put in
  it. The cube in the hands is the model now: core.render.model (new, a
  world's model at rest in a render pass). All the chambers' doors are
  these; the scripted run as before.
- Then (2026-10-05): an older world is built again when its chambers plan
  (storage "chambers", now "5") is not the mod's: its fixed portals had a
  solid panel in front (a shell's facing, deferred before its block, was
  refused) and its doors drew black (the old door blocks). Turned blocks
  written outside mapgen are now checked each step until block and facing
  both hold; shells left by shots are panels again at the start.
- Then (2026-10-05): the low shell of a wall portal has no floor. Its lip, a
  sixteenth over the room's floor, stopped a body walking in at the wall's
  face (z 0.303 before chamber 01's blue, until a jump); now both ways
  through chamber 01's pair walk straight in and out.
- Then (2026-10-05): the shells are not drawn, only collided with; their sides
  stood in front of the picture inside the opening, where the engine's own
  doorway sides now show the far end. And a body that walks backwards into a
  wall portal looking down keeps its look (aim_after_crossing turns the view
  out of the wall only when it does not come out looking back at the opening,
  which a fall never does).
- Then (2026-10-05): a body in a wall portal's hole no longer locks. The box
  fitted to the hole narrows as the body moves off its middle and lets go at
  a tenth; sliding sideways to 0.37 off it gave the body its whole width back
  inside the frame, and nothing moved it again. Now the fit is never widened
  or raised into a wall (update_floor_shift). Checked on the physics alone: a
  body slid to 24.127 in chamber 01's orange hole stayed there for good
  before, and now backs out.
- Then (2026-10-05): crossings of a world's own pairs, which the server
  decides on a move a round trip old, no longer jolt the view: the client
  puts the body where the opening's image had its eye now (the map, plus
  `view`) and looks where the map sends its own look, when a body fits there
  within 0.75 of the server's place -- 0.42 ahead of it walking into chamber
  01's blue. Out of a floor the whole box is kept inside the opening (a wall
  opening's eye height landed on a 1x1's edge, the box half over the floor,
  stuck), no step up over the floor, and the server gives at least the speed
  to rise half a node over its face: walked from chamber 01's blue into a
  floor portal, out at 18.6, up to 20.4, off the hole, three times running.
  The client plays no sound at a crossing.
- Then (2026-10-05): a body caught inside an opening's own block -- its eye
  (not the feet: that would jolt the view) between a tenth and nine tenths of its depth,
  within its sides, not moving out -- crosses whether or not the trigger is
  armed (portal_swallows, on the server's crossing of a world's pairs). Out of
  chamber 04's floor portal and straight back in, the arrival's seed kept the
  trigger off and the body stood in the floor, 214 corrections. Placed still
  in a floor hole at 18.6: out of the wall opening at once, 0 corrections; a
  body coming out of a floor is not taken back.
- Then (2026-10-05): the way home (opening 1) is set into the west wall as
  the fixed pairs are, its plane nine tenths in, the wall's cells hollowed;
  the client fits the body to a mouth's hole too (update_floor_shift), and a
  scripted walk goes in and comes out in the sandbox. In Blender
  (tools/make_chambers_models.py): the floor button up and pressed, the push
  button on its stalk, the launch plate (two by two from its corner cell, a
  yellow and black border, the arrow the way it throws), and the door's
  frames between shut and open: a block's model does not move, so the door
  steps through door_1..4 at 0.06 s, its leaves sliding into the wall, shut
  to a body until open. Chambers plan 6 (the plates' cells, the recess).
- Then (2026-10-05): a floor's or a ceiling's portal is two panels long, as
  a wall's is two high (yaportal's gun4): the long way the way the shooter
  looks, its up that way (rot 0/2 for +Z/-Z; 3/1 for +X/-X on a floor, 1/3
  on a ceiling, whose basis is the floor's turned over), the panel hit and
  the next one on, or the one behind. Its cells are chambers_core:shell_flat,
  the back alone (two shell_one would stand a wall between them); the rims
  drawn with the half sizes swapped for a quarter turn. Shot in chamber 04
  looking +Z: (75,19,5..6), rot 2 -> centre 75.5 6.0; a body let down into it
  came out of the orange over the ledge. Open: out of a wall portal and
  falling back past it, the hands take the hole's lower lip as a ledge.
- Then (2026-10-05): a crossing keeps the energy it came in with: the exit
  speed is the one the last step's average had at the step's middle height,
  less the height the exit puts the body at, the direction the map's. Two
  floor openings in chamber 04 three apart, dropped from 26: apexes 26.0,
  24.5, 22.9, 21.5, 20.4 and held there by the lift out of a floor; now 26.0,
  25.9, 25.8, 25.7 ... 24.8 after 13. Open: that tenth a bounce. Taking the
  client's own speed on its side changed nothing measurable; the rest needs
  the client to cross a pair itself, or its speed in the claim.
- Then (2026-10-05): the same for a mouth onto another world. The arrival
  hands over its speed as a pair's crossing does (exit_velocity, one function
  for both: the energy at the height, the lift out of a floor), from the
  client's own speed at its own height; and a body caught in a mouth's block
  by the eye walks in (the client's mouth trigger asks portal_swallows too,
  and the world of arrival takes such a pose). Placed still in the way
  home's hole at 136.9: out in the sandbox at 61.1 20.0 60.5. The
  single-process client's own crossings still have neither.

**A point-and-click world in the manner of isometric action RPGs (2026-10-02).** `games/arpg`,
reached from the sandbox through a stone arch (`tools/arpg.sh`, two rooms as
fps.sh: sandbox and arpg, two mouths each way): a flat waste of ruins, graves
and braziers under a sun held before dawn (time reset to 0.222 every 2 s,
daylight 0.41, so nothing burns), seen from above. Engine calls for it:
`core.iso_view`, `core.cursor_ray`, `core.walk` (per world, so a walk does not
cross a portal), `core.engine_hud`, `core.mobs_near`, `core.mob_ids` /
`core.mob_pos` (a creature followed by id) and `core.slow_mobs`. A mod's .ogg
and .wav files are now served as media (`sound.<mod>.<file>`) and a client
hears a world's sounds from them first: the zombies and skeletons here are
heard on a machine without the mod. Left click walks, round walls by the
corners the server finds (`find_path`, sent as "way": world code on a player's
machine gets no path search), or up to a creature and strikes every 0.55 s;
right click casts fireball, frost nova (slows to 0.4 for 4 s), chain lightning
or teleport (1-4); kills give experience, levels, globes, gold and potions for
the belt (Q life, R mana, 8 of each). Seen in Xephyr: a click behind an arch
pillar walked round it by (62.5, 55.5); sandbox -> arpg -> sandbox through the
arches, iso view and free cursor in arpg only; 0 corrections. Open:
Then (2026-10-02): imps have a voice of their own (say, hurt, death, shoot;
synthesised by `tools/make_imp_sounds.py`); the way after a creature is found
again as soon as it has moved a node from the last one (at most every 0.15 s,
else every 0.5 s); gold buys at a merchant's stall by the spawn (5-8: life and
mana potions at 30, +2 to blows and +15 mana at 120 a rank, more each time);
and a character (level, experience, gold, potions, ranks) is kept per player
name in the mod's storage, saved every 5 s and on leaving: bought 3 things for
390 gold, restarted the server, came back with 110. Open:
- Kept by name, which is a label: two players of one name share a character.
- The merchant is a stall, not a person: villagers wait for a design.
The skeletons that looked lying down at the screen's edges were standing: a
70-degree view from that high stretches what is at the corners. core.iso_view
takes a field of view now (the engine's comes back on leaving the world), and
arpg looks from 30 nodes at 40 degrees: the same ground in view, flatter, and
upright at every edge.

Then a spawn room (2026-10-02): both arches stand in the walls of a small
stone room with a roof, fire in its corners and a door two wide; a player
arrives there and sees from the eye (the robot's arms in hand), and walking out
of the door the camera rises to above in 1.6 s, the player in the middle of the
picture all the way ("THE WASTE"); walking back in brings it down in 1.2 s.
core.iso_view takes `seconds`, and 0 for the distance means back to the eye. The
camera looks from the south-west now (yaw 135): the door faces +Z, so W held
through the door keeps walking out instead of turning back in, which with the
old yaw 45 walked the player into the room's wall. The stall moved west of the
door, off the way out. Seen in Xephyr: sandbox -> room (eye) -> out (rising,
then above) -> back in (eye) -> through the west arch to the sandbox.

From play: a robot stood in the arpg room seen through the sandbox's portal,
after walking back out of it. Only the world played in has its bodies gathered
every frame; the one just left kept the last frame's, this body among them. A
crossing now empties the left world's bodies. Open:
- A window onto another world shows no bodies at all (players, creatures):
  it never did before it was walked through either.

**Chat and commands (protocol 64, 2026-09-25).** T opens the chat, / opens it
with the slash; Enter says the line to everybody in the world as `<name> text`,
ESC drops it, Up and Down walk the lines already sent. Lines fade 10 s after
they arrive; open, the last ten come back. The engine answers `/help` and
`/list`; a mod declares `/name` with `core.command` and hears it as
`core:command`, only that mod (`mods/whisper`: `/me`, `/msg`). The server drops
a line from a window, one with control characters, over 256 bytes, or sooner
than 0.25 s after the last. Checked in Xephyr with two clients and in the
single-process client. Open:
- Per world, not per server: a room's players do not read another room's chat.
- No permissions, so no `/tp`, `/give`, `/time`: every command is everybody's.
- No Tab completion, no cursor inside the line (only Backspace at the end).

**OPEN-21 -- closed 2026-09-25.** An offline server (`--ticks` without
`--listen`) gives a mod's entities a flecs world of its own for the run, so
`core.ecs.spawn` answers there too (a system moved a spawned one 9 cells in 9
ticks). The client's own world (`scene.mod_ecs`) was already there.

**OPEN-50 -- closed 2026-09-25.** `core:net_message` goes only to the handlers
of the mod whose channel it came in on (`EventBus::emit`'s `only_owner`), on
the server and the client. In Xephyr a `both` scratch mod heard its own
messages and not hunger's, which it sends on every join.

**OPEN-42 -- modifiers (ABM/LBM) limits.** One Lua call per cell that FIRES, no
batching -- the cells tested are C++, and that is where the time goes
(`farm:dry`: 22 fires, 2.2 million cells, 0.16 s). Since 2026-09-25 a sweep
visits the chunks nearest a player first (`Modifiers::set_near`), and a chunk
that loads mid-sweep joins that sweep at its end; two offline runs still give
the same world hash. Found in a 27-minute played session (2026-09-19): `furnace:burn`
tested 4.9 billion cells for 0 fires and cost 17.7 s -- `furnace:lit` got id 64,
the bit of air in a mask of 64. The mask is 1024 bits since: on the saved play
world, 30 million cells a minute went to 0, and two furnaces set by a mod were
still found (10 fires in 10 s, 163 840 cells tested, one chunk).

**OPEN-20 leftovers -- visible mod entities.** A thing collides with nothing,
as no body does with any other in this engine (players, creatures): a moving
box the client predicts against and the server judges against is the kart's
problem over again. Since 2026-09-25 the server sends the bodies near a
player every tick even when there are none, and a message with fewer than
the 64 it can hold is all of them, so a hidden, killed or distant thing goes
from a client at once and not after five seconds. Since 2026-09-25 `core.ecs.health(id, hp)` gives a visible one health
that a player's hit takes points off (the hit path already struck anything with
a Health); at 0 it is left to the mod. Punched in Xephyr: 3, 2, 1, 0, broken by
the mod. Since 2026-09-25 `core.ecs.model(id, model)`
draws a visible one as a model (`ecs::Wears`, which the entity message already
carried): the `yafps` targets are two crossed red and white boards and the
drones a flat thing with rotors (`tools/make_yafps_models.py`), seen in Xephyr.
Its hit test still assumes a box, which the target's boards now fill. Since
2026-09-25 the single-process client draws a mod's visible things too, box or
model (seen: four around the player).

**M13 -- the yafps arena as a world.** `games/yafps` (two mods, 4 textures) is
The earlier fork's yafps rewritten: walled arena as a mapgen stage, four targets and
two orbiting drones with respawn, a 12-round rifle the server fires, crosshair
and ammo on the HUD. Left out: sounds (core.play_sound now exists, unused here), particles, the
sky, the "target down" chat line (a HUD line instead).
Since 2026-09-13 a portal from the sandbox leads into it and back: two servers
with `--portals --portal-to` at each other, and a client with no mods of its own
comes out at 59.9 20 48.1 in front of the arena's opening 0 (`core.set_portal`,
honoured while the mods load), gets the world code, crosshair and `12 / 12`, and
8 s later walks back out at 40.5 20 45.1 in the sandbox with no HUD left; both
sessions kept, no reconnect. Run by hand (`--cross-back S` on the client), not in
ctest. Since 2026-09-14 the `grep` criterion held: nothing under `src/` says
weapon, ammo, vehicle, car, kart or checkpoint. It no longer does, and what
broke it is a mechanism and not a rule of a game: `core/physics/vehicle.*` is
the chassis on four sprung wheels a driving body is, and the doc of
`core.drive` and `core.body_drive` says "kart" where it says what the call
makes. No src file knows a lap, a weapon or a checkpoint.

**M13 -- the yafps arena as a modern arena shooter (2026-09-27).** The targets and
drones are gone. `tools/fps.sh` puts a sandbox and the arena in one server;
the sandbox's opening 0 leads in. A UAC hall under Hell's red sky (52x52, walls
9 high, a gate platform, four towers, eight lit columns, hellrock cracks;
`tools/make_fps_textures.py`). Four guns as items, each a `tool_<gun>` bone of
the first-person arms, with clips played by `core.arms_clip`: pistol (no ammo),
pump shotgun (9 pellets), chaingun (12/s, barrels spin), plasma cannon (0.6 s charge,
a ball that bursts for 140 in 7 nodes, then 32 tracers across the player's 90
degrees). Waves through five floor gates: ghouls (rifle), imps (fireballs),
pinkies (charge), gazers (hover 3.5, fireballs), all `core.register_mob`,
seeing 64 nodes (`core.mob_sight`) and never out of the dark on their own
(`spawn_on` a block no world has). Ammo and medkits back on their pads each wave,
a health orb where a demon dies, the chaingun at wave 2 and the plasma cannon at wave 4.
Every model is Blender (`tools/make_fps_models.py`, ~/kart_assets/fps.blend).
Engine: `core.shoot` / `core.blast` hurt creatures and mod things (never a
player), `core.arms_clip` on the client, and `core.held_item` now answers on a
client (the network loop has no local bag; world code gets it too).
Seen in Xephyr: every gun fires and animates, kills counted and orbs dropped,
PLASMA burst killed 2, wave 5 brought all four kinds. Open: no sounds (core.play_sound
exists now, unused here); the held guns in the robot's hand as others see them are unchecked (V
did not switch view under xdotool); a fireball's push can shove a player standing
at the spawn back through opening 0, so the south side has no gate; guns carried
back to the sandbox are items it does not know.

**M13 -- the yafps base: rooms, the robot's arms, the super shotgun
(2026-09-27).** The arena is now a base dug out of rock (88x88): an entry room,
a hub open to the sky with a ledge five up on two sides, a foundry with
catwalks and a bridge at five, a reactor with a stair round its column up to
ten, an armoury, corridors between them and two loops back to the foundry;
seven gates. In first person the robot's own arms (mods/robot) hold the guns:
each gun a bone under the right hand, the hands put on grip, pump, barrels or
front grip by IK in Blender and every frame baked, with clips idle_/raise_/
fire_<gun> (the client plays `idle_<item>` when the arms have it). The super
shotgun: 20 pellets for 2 shells, and a reload that breaks it open, fetches two
shells from the left hand's pocket and snaps it shut. Fixed: the ghouls's
shots fly straight (`fireball`); a one-shot arms clip ended by fading out of
itself, which replayed its start -- the pump shotgun's second flash after the
pump. The arms file is baked at 30 fps: at 60 it was 1.2 MB, over the 1 MB a
model may be. Open: a player who dies comes back in the entry room, where the
demons that followed them wait at the door.

**A creature fades out when it dies (2026-09-27).** The client keeps the
last pose of a creature that is no longer in the server's list and fades it
out over 1.2 s (`fade_the_gone`, a `uAlpha` in the model shader, blended with
depth still written), playing its model's `death` clip once if it has one. No
protocol change: the entity list is whole in every message. The four demons
have one each (tools/make_fps_models.py): the imp falls on its back, the
ghoul to his knees and on his face, the pinky rolls on its side, the
gazer drops out of the air. Seen in Xephyr: an imp and a ghoul shot,
both translucent and falling, gone after a second. A creature that only walks
out of reach fades the same way.
Since the same day every creature has one, and every new one must
(docs/MODDING.md, Creature; the server logs a registered kind whose model has
none): zombie, skeleton and brute, the pets, the four animals with their young,
shorn and tame models go over on their side (`tip_over` in
tools/make_pet_models.py, in the world's axes so it fits the hand-made rig of
mobs.blend too); the wraith and the cinder are rigged in Blender now
(tools/make_inferno_creatures.py) with idle, walk, run, shoot and death clips --
tentacles that sway, rings of rods that turn -- in the same boxes as before.

**M13 -- the track is entered on foot, and a kart is got into (2026-09-20).**
The world's numbers are a walk now (`core.movement(4.8, 8, 0, 8, -25)`, no
flying): a player crosses in on foot and finds four karts parked in the bay,
`track_core:kart` blocks wearing the same .gltf a driver wears. They are put
down at `core:world_ready` and not by a mapgen stage: a stage runs when a column
is MADE, so a world saved before there were karts read its bay back from the
file for ever and had none (reported from play, 2026-09-20). Every start fills
whichever of the four slots is empty; one left somewhere else stays there. A right click on
one (`core:used`) is `core.body_drive` + `core.body_model` for that player and
the block is taken away: they drive, they are 1.4 by 2.6, everybody sees a kart
and the camera goes behind it; a right click while driving (claimed on the
player's machine, `core:input`) puts them back on foot and parks the kart two
nodes to one side, on the ground. Run by hand in Xephyr on two servers: crossed
in, got in, the countdown, drove out of the bay and back in, got out, got in
again -- 0 corrections in the whole session. Still open:
- A model drawn from a cell is only SOLID and only CLICKABLE through that cell
  (the ray and the collision read a cell's own boxes). A kart's box is therefore
  the one node at its middle and its nose and tail are drawn through: with the
  mesh's box (`-0.8 .. 1.25`) a kart reversing into a parked one was put back by
  the server as "inside a solid block" while its own client drove clean through.
- Since 2026-09-21 getting in puts the body where the kart was drawn, facing
  where it pointed (`core.player_place`, a Teleport as a portal sends), and a
  kart is parked facing the way it was driven. Seen in frames: the kart driven
  out of the row the others stand in. Getting out and back in was not run: the
  scripted driver can click a block, not send the right click that gets out.
- A kart nosed into a wall within about 30 degrees of head on does not drive
  out forwards: its nose is flat on the wall and any turn presses the other
  corner in, as with any car. Reverse. Since 2026-09-21 the chassis slides on
  what it touches (friction 0, Jolt's was 0.2), and from 40 degrees on it
  scrapes along the wall at 24 m/s; before, it squared up and stuck anywhere
  within 50 degrees. Measured offline: a flat floor and a wall, 2 s of throttle
  into it, then 4 s of throttle with full lock either way.
- Getting in raises the body 1.07 (a chassis on its springs): the countdown's
  hold keeps the kart's jump so the server tolerates it, and a hold of zero
  everywhere cost 8 corrections in the first second.

**M13 -- the timed track.** `games/track` (two mods, 6 textures): a walled loop
of road 88 nodes a side, a start line and three checkpoints timed by the server
(each crossing placed between two positions, not on a step), time, checkpoints
and last/best lap on the HUD, running at 24 m/s declared by the world. Reached
from the sandbox like the arena: a client with no mods crossed in, ran two laps
with `--walk-path` -- 14.154 s and 14.092 s, 0 corrections -- and walked back
out; seen in captured frames. Found on the way: a client refused world code whose
`depends` named a server-only mod; a dependency the world did not send is now
skipped. Still missing:
- The kart (`core.player_model`, CC-BY, `games/track/CREDITS.md`) is driven
  since 2026-09-13 (`core.drive(12, 30, 2.6, 0.6)`, seen from behind with
  `core.view(5)`): a scripted driver using the same keys ran two laps in
  22.02 and 22.06 s with 0 corrections (14.1 s on foot). Since 2026-09-14 the
  tyres lose grip past half the top speed (`drive_turn_rate`: 1.3 rad/s at
  24 m/s, a radius of 18.5 m for 9.2 m before) and the camera trails the turn
  (0.3 rad on a steady bend, at most 0.5). Through the corners' middle points
  the driver ran 21.03, 21.01 and 21.04 s, 0 corrections -- which does not show
  the grip, since it takes a right angle under 12 m/s. The server does not
  check its inertia or its turn (OPEN-47). Since 2026-09-14 the body is as wide
  as the kart's body (`core.body_width(1.4)`, protocol 35; at 1.6, the width of
  the rear tyres only, a kart stopped with a gap beside a wall, reported from
  play): a square box, so the nose of the 2.6-long kart reaches 0.6 of a node
  into a wall it drives straight at. Two laps at 1.4 on three servers: 24.725
  and 22.863 s, 0 corrections.
  Since 2026-09-23 the kart is the project's own Blender model (1.28 by 1.78,
  no textures, a rig with steering and wheel bones) and the body matches it,
  1.3 by 1.8: two scripted laps 19.311 and 19.591 s, 0 corrections, and 10
  short runs with 0. Since the same day its wheels roll and steer and the
  steering wheel turns, by bone name (`wheel_pose`), for every body wearing it:
  read off how the body moves, not off the wire. Seen in frames through a right
  bend, 0 corrections. Open: a remote body moves 20 times a second, so its
  wheels roll in steps; full lock is one number for every world (2 rad/s of
  turn); a parked kart (a block) stays in its bind pose.
  Since the same day every player is the project's robot (`mods/robot` in the
  sandbox, linked into `track_core`), playing its clips by name from how its
  body moves -- idle, walk, run, jump_loop, swim, fly, mine/dig/chop while
  digging -- and a body in a kart has the robot sat in it (kart_enter, then
  drive_idle leaning into drive_left/right). Two scripted laps 19.183 and
  19.541 s, 0 corrections; seen in frames walking, standing and at the wheel.
  Open: walk and run play at their own pace whatever the speed (feet slide);
  a remote body cannot show flying or digging (nothing on the wire says so,
  it jumps instead); crouch_*, jump_start/land, kart_exit and the sword clips
  are never chosen; a robot is not cut at a portal (OPEN-29).
  Since the same day the robot's four tools, cut out of its mesh in Blender,
  are the pick, shovel and axe icons (a render, `assets/textures/tools_*.png`
  at 32 and 128 in the local Kenney pack), and first person has arms
  (`mods/robot/robot_arms.gltf`, "idle" and "swing") holding what is in the
  hand. Found on the way: a connected world's tools were always drawn as a
  word, because the atlas wrote icons into this process's item table and the
  hotbar reads the server's; it writes the server's now. Seen in frames: the
  icons in the hotbar, the pick in the hand, the swing while digging.
  Since the same day a block in the hand is a cube of its own tiles in the
  palm, `tools:sword` is an item (two stone over a stick, its icon a render
  too), the arms bob as the body walks and swing while the left button is
  down, and a local world loads its mods' models, so it has the robot and the
  arms as well. Seen in frames: stone in the hand in a local world, the sword
  on a server, the hand 9 px down mid-step.
  Since the same day a hit carries the hotbar slot it was struck with
  (protocol 47) and an item says what it takes off (`register_item`'s
  `damage`, looked up in the server's table): the sword is 4, a fist 1.
  Measured with --mobs 40 and a --roam client holding the sword: 18 blows
  landed, every one for 4, a 6-point mob down in two. And the robot seen
  from behind holds the block that is in the hand.
  Since the same day everybody sees what everybody else holds (protocol 48):
  Hold tells the server when the hand changes square and a Player's Entity
  `item` is what is in it. Seen in frames with two clients: the other
  player's robot holding stone in its right hand. Open: a tool held by
  somebody else points forward from the hand and is hidden behind their body
  when seen from behind.
  Since the same day everybody sees everybody else work (protocol 49): Hold
  also says whether the left button is down on something, and each Entity
  carries it; a body that works plays the clip of its tool (pick mine, shovel
  dig, axe chop, sword sword_slash; with no tool, by the block's material for
  one's own body and mine for anybody else's). Seen in frames with two
  clients: the other robot's arm up mid-swing while it held the button down
  with a pick.
  Since the same day a world says how high the eye is (`core.eye_height`,
  MovementRules::eye, protocol 50): the camera, reach and every ray a hand
  casts, on both ends, portal triggers included. The robot is 1.8 tall now,
  the same robot scaled in Blender by 1.8 / 1.357, with its eye at its visor,
  1.5; the kart is scaled with it so the robot still sits in it (1.70 by 2.36,
  the body 1.7 by 2.4). Two scripted laps 20.764 and 20.309 s (19.2-19.9 in
  the 1.3 by 1.8 kart), 0 corrections; first person seen at feet + 1.50.
  The first-person arms are the big robot's too (scaled about the eye, so the
  picture is the same), and a block in a hand is sized by the forearm.
  Since the same day Ctrl held on foot crouches (it still descends in
  flight): the walk at 0.35 of its pace, no sprint, the eye at 0.8 of its
  height, and the robot plays crouch_idle / crouch_walk. Hold carries it and
  each Entity shows it (flags 1 working, 2 crouching, protocol 51), and the
  server measures a crouched body's reach from the lowered eye. Measured: eye
  21.50 standing and 21.20 crouched (feet 20, eye 1.5); 4.8 m walked in a
  second against 1.85 crouched; the other robot seen crouched from a second
  client. Open: a crouch does not keep a body from walking off an edge, and
  the box stays 1.8 tall (a crouch fits under nothing lower).
  A body wider than a floor portal cannot sink through one: the server exempts
  only the opening's node, and a joined-directly timed client that places one
  under the kart was corrected over a thousand times.
- The view from behind (`core.view`, key V) is not portal-aware: a camera
  behind a player who has just walked out of an opening can sit on the far
  side of it. In a local world the player's own body is the box, because a
  local world dresses nobody yet. Aim still comes from the eye, not the camera.
- Flying over the walls is refused by the server since 2026-09-14: a body that
  cannot fly (it drives, or its world says fly 0) and stands on nothing may be
  at most a jump (v^2 / 2g, 1.28 on the track) plus 0.3 above where it last
  stood. With synthetic claims: a climb at 8 m/s is refused at 1.60 up, over
  the two-node wall too; an honest jump, a jump onto a block and a second one
  from its top are accepted, and a body allowed to fly climbs unrefused. A body
  still airborne when a portal's second of grace runs out is judged from where
  it was at that moment, not from where it last stood.
- Since 2026-09-14 a player who arrives is held for a three-second countdown
  (`core.body_movement` to zero, drawn 3 2 1 GO), best laps are kept by name
  (`core.player_name`) for as long as the server runs, and everybody's HUD shows
  the fastest five. Seen in play on three servers: a client crossed in from
  the sandbox, drove out of the bay and ran two laps in 20.713 and 20.029 s
  with 0 corrections; a second client under the same name crossed in, the
  server logged "is back, best 20.029 s", and its frames show the countdown,
  BEST 0:20.03 and the board. The same run again gave 28.915 and 22.732 s:
  the scripted driver's laps vary by up to 9 s, not looked into. Since
  2026-09-19 kept across a restart with `core.storage_set`, a key a name: a
  scripted lap of 21.084 s, the server stopped and started again, and on the
  rejoin it logged "racer is back, best 21.084 s".
- A held body coasted on under its drag and was corrected (0.047 of a node in a
  claim allowed nothing); the client now keeps a driving body's speed within
  its numbers whatever the keys.
- Since 2026-09-14 the bay is 13 wide and open to the road along all of it
  (it was 5, and the scripted driver ended nose to the inner wall). What kept
  the driver there: it hopped whenever it moved under a quarter of a node in
  half a second, and a kart in the air does not steer. Turning round inside
  the bay, and driving straight back in after arriving, were not run.
- Joined directly (no `--portal-to`) the bay's two openings are a pair; walking
  from one to the other was not run -- the timed client moves the openings
  itself there.

**M13 -- one sandbox, two worlds.** Since 2026-09-14 each opening has a
destination of its own (protocol 34, `--portal-to` given twice). Run by hand on
three servers: a client with no mods walked into the sandbox's opening 1, drove
a lap of the track, drove back in and came out of opening 1 at 61.1 20 60.3,
then walked into opening 0, came out in the arena and back at 40.5 20 45.1 --
four crossings, no reconnect, the window left behind kept open and streamed
(`--mouths 1,0 --cross-back 6 --walk-path ...` on the client). Found on the
way: the world behind a second mouth was never meshed, so its door showed sky
(0 vertices with its portal pass running); now 124320, and a frame from the
plaza shows the track's red and white wall through opening 1. Still missing:
- Two mouths per world (openings 0 and 1), so a sandbox leads to two worlds at
  most.
- Tickets are asked one mouth at a time: the answer does not name the opening.

**Rooms: several worlds on one server (protocol 56, 2026-09-24).** `--room
NAME` starts a world of its own on the same server and port -- its own mods,
world file and openings; what comes before the first `--room` is every room's.
Each room is the server this program always was (serve_room), on a thread,
and one lock keeps them to one at a time, let go only while a room waits for
the network. A Router reads the room a Hello names (empty: the first; an
unknown one is refused as NoSuchRoom) and hands that peer's events to it. The
rooms share one identity and one ticket key and trust it, so
`--portal-to @pista/0` needs no keys. A mouth or a redirect onto a room has no
host: the client reaches it at the address it reached the server by, and names
a world by address and room (the Welcome says the room's own name). A player's
bag and health are one across rooms: becoming a body in a room takes them from
the room they are a body in (take_carried), bytes by name, so an item a room
does not know comes back unharmed; otherwise the room file that kept the bag
last. `tools/track.sh` is one server with sandbox and track. Seen: sandbox to
track and two laps, 20.73 and 20.30 s, 0 corrections; 10 short runs, each
crossed and in a kart with 0 corrections; there and back with 1 pick, 10 stone
and 13 health carried both ways (15 on the way back, regenerated); after a
restart, joining the track, the bag read from the newest file. Also fixed on
the way: a player arriving through a mouth from ANY other server now gets
their bag here (it was empty, and saved empty over the one this world kept).
Open: a ticket for one room is good at every room of the server (one key); a
stack picked up in the room just left, in the instant before the client
says so, stays there; log lines do not say which room they are from; a
bag kept twice in one second by two rooms before a restart may read back the
older.

**A Inferno (protocol 57, 2026-09-24).** `games/inferno`: scoria caverns
from two octaves of 3D value noise, worked out every 4 cells and blended, rock
closing in at the floor and the roof of bedrock; lava in every open cell up to
y 12 (it burns: 3 every half second), ash sand on its shores, glowrock in
clusters from the roof, quartz in the rock; fortress lanes of brick every 96
nodes along x and z, decks with rails, tunnels through the rock and pillars down
to what holds them; an arrival chamber at 32 30 32 with an obsidian frame and
four tunnels out that turn to walkways over the caverns. Two engine calls for it:
`core.world_sky(r, g, b)` (a sky of one colour, no sun: in the Welcome) and
`core.mapgen_core(false)` (no engine ground; the region made before the mods
loaded is emptied). mods/mobs and mods/robot are linked in, and in the dark the
creatures come out everywhere. A third room of `tools/track.sh`, through the
sandbox's second opening. 32 ms a column (36 in 1.16 s). Seen in Xephyr: the
chamber and its frame with the sandbox through the opening; caverns, lava seas,
glowrock and a fortress; lava killed a player who walked off a walkway; there
and back from the sandbox with 0 corrections. Fixed on the way: a client that
joined inside rock (the middle of a world with no plaza) was refused every claim
and never moved to the spawn, which waited for an accepted one; and the spawn in
front of an opening was always at y 20. Open: no inferno portal a player builds
(the frames of mods/portal stay in their world); no wraiths, cinders or brutes;
lava does not flow; the sandbox's openings have no frame (all four done since,
below).

**The Inferno, finished (2026-09-24).** A portal a player builds:
`mods/infernoportal` in the sandbox adds obsidian (eight stone round a coal
make two) and a lighter (an iron ingot and a coal); a frame of obsidian round
a hole 1 wide and 2 tall, either way, lit with a right click, moves the world's
second opening there (`core.portal_place`, new: one of the two openings moved
while the world runs, 0 without --portals), and the way back from the Inferno
comes out of it; a block of it broken puts the way back in the plaza, whose
opening stands in a frame of obsidian too; kept across a restart. Since
2026-09-25 the hole may be any size from 1x2 up to 8x8 (`core.portal_place`
takes half_w/half_h); a crossing from a bigger opening into a smaller one
(8x8 into the Inferno's 1x2) is clamped inside the small hole by
portal_traverse, a jump in the view that same-size crossings never get. Not
walked in the client yet: a lit 3x4 frame is the check. A world from the title
menu (Play, Host) now runs as tools/track.sh does -- rooms sandbox, pista and
inferno, pista.db and inferno.db beside world.db, same identity -- so the frame
has a Inferno to open onto; without games/ beside the program it is the lone
world it was. Startup waits for all three rooms (the Inferno's mapgen ~5 s). A lit frame now has its twin in the Inferno
(2026-09-26): the sandbox's portal_place tells the room its opening leads to
(core:far_moved, rooms of one server only), the openings' columns are always
made, and games/inferno builds an obsidian frame of the same size and turn at
x/8, z/8, at the height that breaks least (rock 1, lava 3, bedrock never, floor
under the cells in front and behind 0.25). Measured headless with a probe mod:
a 3x4 at 200 100 went to 24 36 12, 1.5 broken; told again after a restart, the
same frame, 0 broken. Still one way to the Inferno at a time: a world has two
openings that lead away, so lighting a new frame moves it. Lava flows,
mods/lakes's rule, once a second and four cells of reach, and falls out of
cavern walls. Brutes (the zombie in pink and gold), wraiths (float 6 over,
fireballs from 24) and cinders (float 2 over, fireballs from 16, only on a
fortress's bricks), by three new register_mob arguments -- `hover` (flies: no
gravity, straight at the player), `fireball` (shots straight, slower, gone
where they land) and `spawn_on`. Their models are boxes written by
tools/make_inferno_models.py (the brute is the zombie's .gltf recoloured on
its .bin). Seen in Xephyr: a frame built in front of the spawn, lit by a real
right click, showing the Inferno, walked through to the Inferno; a brute, a
cinder and a wraith and their fireballs, a player dead in 4 s among them;
through the plaza's framed opening and back, 0 corrections. Open: a wraith is
drawn 1.8 tall because every creature's box is; the fireballs do not set fire
or break blocks; gold, tears and rods have no use yet.

**A gate to every themed world (protocol 85, 2026-10-06).** A world may have
eight mouths (`kMaxMouths`, was four). `tools/track.sh` and a world from the
title menu are one server with six rooms: the sandbox's openings 0-4 lead to
pista, inferno, arena (yafps), arpg and chambers, each of those back to its own
opening (`@sandbox/2` ...). `mods/gates` builds a structure round each one by
the room it leads to (`core.portal_room`, new): a chequered start arch, the
obsidian frame with glowrock pillars on scorched ground, a techbase door with
hazard stripes, a ruined arch among graves, a white panelled wall with a blue
and an orange stripe; openings 2-4 are put about the plaza (arena 28.5 64.5,
arpg 78.5 34.5, chambers 84.5 84.5). The client opens and draws only the two
mouths nearest the body (`kMouthsShown`), and meshes only their worlds (mouths
past the second were never meshed before). Seen in Xephyr: all five gates with
their world in the hole; from the spawn 2 windows (pista, arena), not 5;
walked into the pista's and out in the paddock. 13/13 tests. Open:
- A window once open stays open (and streams): a Scene cannot drop a world yet.
- A mouth not drawn shows the plaza through its hole, not a picture.
- Fixed spots on the plaza; scattered over the map later.
- Holes are 1x2: a kart does not fit the pista's.

**OPEN-49 -- code a world sends to clients.** Since 2026-09-13 world code draws
over the screen (`core.hud`) and hears presses (`core:input`, which can claim a
mouse button from the engine), and a server mod can fire a shot from its own
view of a player (`core.player_look`, `core.raycast`). `games/yafps` runs on it:
a client with no mods of its own joins, gets 3.8 KB of code, and the server's
shots count down 12 rounds and reload; seen in a captured frame (crosshair,
ammo, targets). Still missing:
- A client-only mod cannot talk to the server; it needs a `both` half.
- Since 2026-09-25 input is both mouse buttons and 22 keys (WASD, space, shift,
  ctrl, F, R, E, Q, V, C, 1-9), each pressed and let go (`key_w_up`); held is
  pressed and not yet let go. In Xephyr with real clicks and keys a `both`
  scratch mod heard mouse_left, mouse_left_up, key_w, key_w_up, key_2, key_2_up.
- Since 2026-09-25 the HUD draws images (`core.hud.image`, a PNG of the
  world's media, seen); the text is still the built-in font.
- Since 2026-09-25 world code loads in `depends` order (`ModHost::load_dirs`,
  what a mods directory goes through).
- A world with no signing key can only be refused by the global flag, and the
  refusal list is a file with no screen.
- World code is loaded once per session; new media mid-session need a rejoin.

**A creative inventory, and what a panel cannot do (2026-09-17).** `mods/creative`
lists every item in the world on a panel -- walked off the item table with the
new `core.item_count`, `core.item_name` and `core.item_stack`, so a mod added
tomorrow is in the list tomorrow and an orphaned item is not in it at all -- and
a click hands over a full slot of it. 73 items over 4 pages of 21 in the shipped
set; seen in a captured frame with the panel up, and driven by a mod that emits
`core:ui_click` for every button (a stack of stone, a pick, a bucket, a button
id naming nothing, a page turn and a close, all survived). What it cannot do,
which is the panel model and not this mod: buttons are TEXT, so an item is its
name and not its picture; and a panel's contents belong to the mod rather than
to a player, so two players browsing at once turn each other's pages. Opened by
right clicking a green box at 42 20 47, or, since 2026-09-19, with C: creative
is `side = both`, and its half on the player's machine hears `key_c` and asks
the server.

**OPEN-41 -- closed 2026-09-25.** `tools/api_gen` reads every
`lua_pushcfunction(L, fn, "core.x")` in `script_luau.cpp` and stops the build
when one is not in `docs/MODDING.md`; API.md lists them. It stopped the build
the same day, over `core.hud.image`.

**Two worlds to make (2026-10-07).** The parkour world: done 2026-10-09, below.

**The parkour world (2026-10-09).** `games/parkour` (its guide:
games/parkour/README.md), the sandbox's seventh gate (orange and black
scaffold, 16.5 21 52.5) in `tools/track.sh` and the title menu. Engine:
`core.parkour` (MovementRules::parkour, protocol 87) turns on, for bodies on
foot, a level wall run of at most 5 nodes, a kick off a wall within 0.45 (not
the same side twice running), a slide 0.8 tall under one block, a backflip of
2.2 nodes up and back; `core.rope` (client) hangs the body on a rope shot 40
nodes, swung by the keys. No ledge grabs there. The server allows 20 m/s,
any height and the 0.8 box (`limits_for`, MovementLimits::height). Six robot
clips made in Blender over MCP (wall_run_left/right, wall_jump, power_slide,
backflip, rope_swing), a rope gun modelled there too. Measured in a headless
sim of player_update: wall run 5.03 nodes across a pit of 8, slide through a
hole 1 high, backflip onto a step 2 high, chimney 7 up from its middle, rope
across 18 at up to 17 m/s. Seen in Xephyr: all five moves and their clips,
0 corrections; a fall puts the body back on its pad. 13/13 tests pass. Open:
- Nobody else sees a rope (only this machine draws its own), a backflip or a
  kick: others' clips are guessed from motion (wall run, slide only).
- Walking through the sandbox's seventh gate was not done by hand (20 fps,
  the mouse misses the door); the server lists the mouth and the client looks
  through it.
- The validator allows any height and the 0.8 box to anybody in a parkour
  world, not only to a body making those moves.

**The backrooms (2026-10-09).** `games/backrooms` (its guide:
games/backrooms/README.md), reached from the sandbox's
sixth gate (yellow wallpaper, 16.5 21 28.5) in `tools/track.sh` and the title
menu. Three levels stacked in the 64 cells: Level 0 (yellow, floor 20), the
poolrooms (floor 36, pools and slides) and Level 1 (concrete, floor 4). Rooms
of 8 on a grid; every wall that is there has a doorway, so all is connected.
The layout is a hash of x and z, not of the seed. One chunk column in three
holds a portal pair between two levels at the same x z; only pairs within 40
of a player are opened (7 at most). Seen in Xephyr: a crossing from Level 0
came out in the poolrooms, and the level's name is shown on arrival. The
slides push the body every tick (`core.player_launch`): the conveyor alone
left it standing where two ramp cells meet; down a slide in about 1.5 s, into
the pool. Open:
- The way out is only the room at the origin of Level 0 and of the poolrooms.
- Lights flicker since the same day (Halls, pools, light and slides, below).

**Corridors, stairs and holes in the backrooms (2026-10-09).** Runs of four
rooms become corridors three wide (Level 0 22%, Level 1 30%, the poolrooms 11%
as flooded channels), a door in each side where the grid's wall has its own,
so the grid stays connected. One room column in about twelve goes up or down:
stairwells (Level 1 to Level 0, Level 0 to the poolrooms) climb 16 cells in
two flights a turn of half-cell stairs and come out through a 2x4 hole in the
floor above; holes (Level 0 to Level 1, the poolrooms to Level 0) drop through
a walled shaft into a pool. Seen in Xephyr, walking with W: every flight
climbs (the first stopped at the head height of the floor above until the
hole was made a cell longer); a drop from y 21 to the pool of Level 1, health
20 before and after. Open: nothing fences a stair hole or a drop hole; the
stairs are walked, not checked by a test.

**Halls, pools, light and slides in the backrooms (2026-10-09).** Halls: one
block of 2x2 rooms in about ten is one room up to the gap's ceiling (19, 33,
62), galleries three wide round it, one or two floors, parapets, a flight of
the level's stairs up to each. Pools: square, room-wide and lane pools, 2 or 3
deep, a steel ladder out of every one, coping and underwater lights in the
poolrooms. Light: dead rooms show dead tubes and lamps, Level 0's tubes and
Level 1's lamps flicker round each player (set_block, a few a second), shafts
and halls lit. Stairs dressed per level (carpet, tile, concrete). Blender: the
slide's chute and the ladder (tools/make_backrooms_props.py), and the robot's
`slide` clip (robot18.blend, added with tools/gltf_add_clips.py), which the
client plays over a downhill belt of 3 m/s or more (`slides`). Found and
fixed: a hall's high walls and a stairwell's high shaft walls can be in the
next chunk column, where the stage's writes are dropped -- the neighbouring
room raises them now. Seen in Xephyr: the halls of the three levels, a flight
in a hall climbed (+x, facing 1), the big pool and its ladder, a stairwell's
lit shaft, the robot sitting with its arms up down the chute (V). The client
runs at 4-7 fps there and slow motion with it: walking does 1.3 m/s, not 4.8,
and the slide is as slow. Open: no doors onto the galleries from next door.
Same day: slides turn and come down from the galleries. Two quarter-turn
pieces (slide_turn_a/b, a chute on a quarter circle in Blender, a 4x4 grid of
boxes falling with it), laid with the straights from a list of headings
(`lay`). A slide room is straight or turns a corner into a pool along its west
side; every poolrooms hall has one off its first gallery, ten cells round the
east and south sides into its pool. The push follows the circle round a turn
and pulls back to the chute's middle: pushed along the mean of the way in and
out, a body ran on across the next pieces and off them onto the floor. Seen in
Xephyr down both, into the water at the end.

**What lives in the backrooms, and what it sounds like (2026-10-09).** Two
creatures, boxes rigged and animated in Blender
(`tools/make_backrooms_creatures.py`, run headless; looked at through the
Blender MCP): the Bacteria (Level 0, out of dead rooms on the carpet, 30
life, 3.4 m/s, blows of 4) and the Smiler (Level 1's dark, on the slab, a
glowing grin that opens to bite). Clips idle, walk, run, attack, death; no
"death" warning in the log. Seen in Xephyr: both drawn, they came at the
player and hit. Nine sounds made with ElevenLabs (text to sound, 50 credits
each, 600 with three retries after a rate limit): each creature's say, hurt,
death (and the Bacteria's attack), a splash at the foot of a slide, and Level
0's hum -- given 1 s, looped to 8 s with ffmpeg and played over each player
there every 8 s. Open: the Smiler's say is 0.5 s, the hum a loop of one
second; the poolrooms are quiet.

## Distribution (M10, suspended until the game is declared playable)

**OPEN-40 -- no Windows build, no AppImage or Flatpak.** Linux tarball only
(`tools/dist/package-linux.sh`).

**OPEN-39 -- a mod package has no source.** `.cnpmod` is verified against its own
hash, which proves it arrived whole and says nothing about who made it. No index,
no signatures, no trust decision.

**OPEN-52 -- no Android or browser build (lowest priority).** Desktop GL 3.3
only: `PLATFORM "Desktop"`, GLFW on X11, every shader `#version 330 core`, and
portals clip with `gl_ClipDistance`, which GLES 3 / WebGL2 do not promise.
raylib 6.0 already has `PLATFORM=Android` and `PLATFORM=Web`, and ENet, zstd,
SQLite and Luau build for both. Worth doing first either way: DESIGN.md 2.4
"strada B" (clip by `discard`, shaders under `#version 300 es`), which can be
tried on the desktop.
- Android: NDK toolchain and APK packaging; touch controls (stick, drag to
  look, buttons), all in `gfx/platform_raylib.cpp` where input already lives;
  assets through `AAssetManager`, worlds, keys and config under the app's
  private directory; chat and UI with the on-screen keyboard.
- Browser (Emscripten): no UDP, so ENet cannot connect -- multiplayer needs a
  WebSocket (or WebTransport) transport and a server that speaks it; the
  in-process singleplayer server needs no network and could come first.
  `std::thread` (client, server, `mesh_pool`) needs pthreads and COOP/COEP
  headers; the `WindowShouldClose()` loop must become
  `emscripten_set_main_loop` (or ASYNCIFY, slower); worlds and keys need
  IDBFS or OPFS to outlive the tab.
