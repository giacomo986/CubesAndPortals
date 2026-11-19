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
4. One shared library for the front and the assault (Structure). Done 2026-11-19.
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
- Distribution: macOS, real Windows, signing, app id, OPEN-39, OPEN-52 (below).
- Track key: 32.0 s is the scripted driver's pace, possibly too hard by hand.
- Lab: 1000 points, 36 a shotgun shot, orbs of 3 are guesses.

## Test rig (closes many entries below)

- Left click: done 2026-11-19. `--click-at x,y,z[,s]` walks up to a block,
  looks at it and holds the left button (let go when the block is gone);
  `--left-at t[,s]` holds it wherever the player looks. Both go down the
  platform layer (`plat_script_mouse`), so mods hear `mouse_left`, the dig and
  the swing run. Seen under Xephyr: a sandbox block dug, yafps' server told
  `fire1` and `fire0`. Network loop only. Still to use it on: the prop hunt's
  blow, the track's power-ups, the chambers' gun, the bees' harvest, the arpg's
  held button.
- Audio: done 2026-11-19. `--record-audio FILE.wav` opens the device in a timed
  run and writes the mix (stereo, 48 kHz, before the options' volume), saying
  where in the file the run starts; `tools/null_sink.sh` runs it on a sink
  nobody hears; `tools/audio_check.py` gives the level in time and fails on
  `--expect T0-T1` silent or `--silent T0-T1` loud, and lists `--seams`. Seen:
  a dug block heard 0.4 s after the button went up, silence before. Still to
  go through: the thirty "not heard" entries below, one by one. yafps' shot
  needs a gun in the hand first (the run holds none, so `fire` returns).
- A 1920 x 1080 Xephyr screen for client checks: on a smaller one the HUD's top
  lines fall off (`Xephyr :5 -screen 1920x1080 -ac -br -noreset`).

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

## From a player's comment (TODO comments from youtube.md)

Done and seen in Xephyr: the torch held (tools/make_torch_models.py) and on a floor, a wall or a
ceiling (mods/torches, side = both: the face clicked found by a ray from the eye); a block that
gives light held in either hand lights round the player (WorldRenderer::hand_light, the world
shader); the left hand (Inventory::kOffhandSlot, H swaps, a square on the bag's figure and beside
the hotbar, drawn in hand_L in first and third person); Options > Controls moves the game's keys
and mods' own (core.register_key), kept in the options file as bind.<slot> / bind.mod.<name>;
assets/lang/pt.txt, every line of it.txt and en.txt (pt_BR picks it from the environment).

Then (2026-11-14): protocol 124 carries a person's left hand (EntityState::off_item), drawn on
everybody's body; the hands' light is a box of cells round the eye (update_hand_light, 48 a
side), spread a level a cell from every lit hand seen -- round corners, stopped by what is
opaque -- read by the world shader smoothed between cells, by the models and the arms, and in the
classic light; the Controls page takes the middle and side mouse buttons ("MOUSE3".."MOUSE5",
core.register_key too), the hotbar's 1-9 and the tools' F keys; README.pt.md.

Left:
- ESC stays ESC, and the left and right buttons stay the game's clicks (not on the Controls page).
- The hands' light is worked out in a 48-cell box round the eye: a lit hand further than ~10
  nodes from its edge loses some of its reach.
- pt.txt wants a Brazilian player's read; the commenter offered.

## Platform game and arcade shooter (drafts)

Roughed out 2026-11-13 (`games/platformer`, `games/shmup`), with three cameras
for a world's code: `core.side_view`, `core.top_view`, `core.top_view_at`. The
same day: a gate each on the plaza's south edge (`mods/gates`, a portal between
rooms: the sandbox's 28 mouths are all taken), both worlds in the rooms file
(written by the title menu and by `tools/track.sh`), pictures and sounds
(`tools/make_platformer_assets.py`, `tools/make_shmup_assets.py`), the
shooter's planes in liveries of their own (`tools/make_shmup_models.py`), and
prize blocks that give a coin.

Checked in Xephyr, the keys sent by xdotool: the pipe, the camera's swing,
A D W in the level, coins, a pig's beat, two prize blocks bumped; a game
started, the fighter slid about, shots, darts shot down to wave 2; from the
plaza through each gate and, for the platform game, back; a world started
from the title menu (`--play-world`) writes both rooms and builds both gates.

Both:
- Not heard: Xephyr's client opens no audio device (see Test rig).
- Not played by two; not played through by hand to the end. Neither gives a key.
- Both rooms start as soon as anybody is on the plaza (every gate is within
  160 nodes of it) and so never stop while somebody is there. Two of the
  sandbox's parked second mouths (17-23) are lent to their gates.
- A survival world has no gate to either: `mods/gates` builds them on a plaza.
- The cursor is free and drawn over both cameras, as over core.iso_view's.

Arcade shooter, 2026-11-14 (`games/shmup/README.md`): a hangar on an island,
F boards a parked fighter, a take-off with the camera swinging from behind to
overhead (core.fixed_view every frame; the cameras' distance now goes to 64,
the field is seen from 50), islands, crests and clouds drawn scrolling by the
client over a bare sea, planes kept to the middle 48 of 80, eight power-ups
(speed, turbo, power, rapid fire, homing missiles, diagonal fire, laser,
wingmen), ten waves, a flying fortress with six turrets and a shield at the
tenth, MISSION COMPLETE.
- Seen in Xephyr, keys by xdotool: the hangar, boarding, the whole take-off,
  waves 4-7 with every plane dropping (TEST_DROPS), diagonal and rapid fire
  picked up, the bomber; the fortress from TEST_WAVE 9 -- shield down and back,
  turrets firing, laser, two wingmen, the win, back on foot in the hangar, the
  high score kept.
- Not seen: a missile on the screen (they flew: the fortress fell faster with
  them); a game from wave 1 to 10 by hand; two players; the gate from the
  plaza into the hangar (the room was started by tools' flags, born in it).
- Same day: an islet with a tank now and then (the islet is the client's, told
  once), planes going down into the sea (a model drawn by the client: turning,
  smaller, smoke, a ring), and the win -- off the top of the screen, a landing
  with the camera behind, on foot. All seen in Xephyr. The falls and the
  landing are drawn after `if phase == "foot"`: whoever is in the hangar sees
  none of it. Not seen since: the fighter falling with its last life only.
- Several players, 2026-11-14: whoever boards joins the game going; per pilot
  past the first, planes take +60%, +50% of them come, they fire +25% faster
  (an estimate, not played). Seen with two clients in two Xephyrs: the second
  joining at wave 3, both fighters on both screens, PILOTS 2, own lives and
  score each. Not seen: the fortress with two; three or four pilots. Each
  machine draws its own islands: two players do not see the same ones.
- Not balanced: the fortress took a life every ~4 s from a fighter weaving
  blind; with every power-up it fell in ~20 s. Power-ups last the whole game.
- A take-off is seen only by its pilot: the others see them stand by the
  parked fighter, then a fighter over the field.
- Up to ~85 shot messages a second from one fully armed fighter (one a shot).
- A saved shmup world keeps the old carrier: delete it (tools/shmup.sh --fresh).
- 2026-11-15, a second level and fewer power-ups: waves 11-20 after the
  fortress, an aircraft carrier at the 20th (500 hp, four turrets, a plane
  launched every 1.7 s, 1 s when half gone, at most 12 up); four new kinds
  (plated planes in files of five along four paths, sweepers, divers,
  gunships); a power-up from 1 plane in 15 (was 6), a whole file, a gunship,
  every other tank, two from a bomber (was three), none from an ace.
  Seen in Xephyr: the carrier launching and firing, sunk, the win and the
  landing (TEST_WAVE 19); the fortress down into LEVEL 2 and waves 11-15
  (TEST_WAVE 9) -- both with shots that took 40, since put back to 1.
  Not played: either level by hand at real strength, so the new drop rate and
  the carrier's 500 are estimates; the carrier with two pilots. `--room
  shmup` is born facing the doorway: walk backwards (S) to the fighters.

Platform game (2026-11-13: two levels, a bonus room, round pipes from
Blender, three creatures from Meshy -- sprout, spikeshell, buzzer, 120
credits; then into 1-1 by a scaling hole that halves the body, a pipe that is
a real shaft into the bonus room and out of it, small/big/fire with mushroom,
flower and fireballs, the creatures turned the right way, and a screen that
scrolls with slack and smoothly):
- Seen in Xephyr, keys by xdotool, some from a body put there by a test line
  since taken out: the drop through the hole and the camera's swing; S on the
  tall pipe, down the shaft into the bonus room and up the far one; a power-up
  block, the mushroom out of it and taken, the growing flicker; a fireball
  killing a sprout; a hit while big back to small with the lives kept; a
  small jump measured at 2.0-2.4.
- Not checked by hand: either level end to end (1-2 never walked whole); a
  sprout or a buzzer landed on; the flower picked up from a block (only given
  by a test line); the half-way flags; 1-2's pole and the way back to the
  room; the bridge pipe open after 1-1; a big body in the shafts.
- Not heard: none of the sounds (Xephyr's client opens no audio device).
- 2026-11-13, from play: the pipes' models were half a cell off (a block's
  model is drawn from its cell's corner, not its middle): rebuilt. A body at
  half size hung from a ledge as a body at full size does, 1.85 under its edge
  and taking edges 1 to 2.1 over its feet: the hands now go with
  core.body_scale (core/physics/player.cpp), and a small body steps up onto
  nothing a node high. Seen: the small robot hanging from the tall pipe's lip.
  The server's check (body_at_ledge) still uses a full body's reach: wider, so
  it lets more by, never less.
  Also from play: a big body stuck under the bonus room's hanging pipe -- the
  shaft was shut on its head (it waited for the feet, not the head, to be out
  of the mouth) -- and a big body stopping short of the lying mouth, which
  then never took it up. Both fixed and seen with a big body: down, out, up.
- 2026-11-15: the fire has an aura (a new `"aura"` kind of core.render.fx, drawn on
  this machine's own body only: other players' fire is not shown); a hit with the
  fire loses it and leaves the body big; the pole is walked through, the body slides
  down it and walks by itself (core.walk, which side_view now obeys) into the castle,
  where a portal leads to an alcove behind the next level's start; 1-3, a castle:
  lava, embers, bars of fire, and the furnace (16 heat: a fireball 1, a landing on
  it while its fire is out 4). The bridge pipe leads to the furthest level open.
  Seen in Xephyr from bodies put there by test lines: the aura, both hits, pole ->
  castle -> portal -> 1-2, a tour of 1-3, a bar burning a body down to a lost life,
  coals, four fireballs and a landing striking the furnace, the gate opening, the
  last castle back to the room. Not played by hand: 1-3 end to end, so its numbers
  (embers 2.6 s, bars 2.2 and 1.6 rad/s, the furnace's pace and 16) are guesses.
  A saved platformer world keeps the old levels' blocks: start it `--fresh`.
  Not in a CHANGELOG entry yet: for the next alpha.
- 2026-11-15, later: the pole stands on a base again; the body hangs on it, slides
  down onto the base (1.3 nodes a second), steps off behind it and walks into the
  picture's depth to the castle, which now stands behind the walkway (z 2..4) with
  its door -- the portal -- facing the camera; the way out is a door in the wall
  behind the next level's start. core.walk under side_view now walks any way,
  off the plane too. Seen in Xephyr: 1-1's pole to 1-2. Not seen: the last
  castle's door back to the room (the run was cut short two nodes from it).
- 2026-11-15: the room's door lost its way home for good after a trip far into
  the levels (seen by Giacomo, then made again with the old server): its mouth
  is given back when every player is 160 away, the virtual portals (the hole,
  the castles) took openings 0 and 1 meanwhile, and schedule_mouths read those
  as the world's own pair, leaving no room for a mouth. A virtual portal's
  opening no longer counts as the pair (src/server/main.cpp); seen in rooms-file
  mode: given back in 1-3, given again on return.
- 2026-11-15: a power-up block gives one power-up per player in the level, out
  one after another (0.7 s apart), each as its player would be given; a body
  takes none that would change nothing for it. Seen with two clients: two
  mushrooms, the first player took one, the second the other.
- 2026-11-15: breakable bricks (platformer:bricks, the floating rows and ledges;
  walls and castles are platformer:brick and stay): a big body's head breaks
  one, so does a body grown into it by a mushroom; a small body bumps it. Back
  when the level is new. Checked by server test lines: small bump keeps it,
  growing under the 1-1 ledge broke the two over the head, a big bump broke
  one, a new start put them back. The four flying pieces were not seen (the
  screenshots missed the moment).
- 2026-11-15: every touch of the furnace throws the body off it (8 along, 7 up;
  a landing on it 6 and 10), also while nothing can hurt the body, so it never
  stands inside the stove; not after a hit that cost a life. Seen from the
  server: a big body walked into it lost the size and landed 7 nodes off.
- Other bosses thought of for 1-3, should the furnace not do (2026-11-15, none built):
  a **bell-ringer** -- a huge bell on a chain that swings and drops to crush, struck
  by making it bang into prize blocks; a **lava dragon** -- a head and neck rising
  in turn out of three lava pits, landed on when it dips to bite; a **spiked king**
  -- a giant spikeshell rolling wall to wall, tipped over by bumping the ledge it
  crosses from below, then landed on.
- Bodies and models are dark under a roof (1-2, 1-3): the robot and the furnace
  read as silhouettes against the wall, though the blocks about them are lit.
- The creatures are rigid: no walk, no death -- a puff of smoke where one was.
  They walk a beat and neither fall into pits nor turn at walls. Landing on
  one stops the body's run (core.player_launch sets all of its speed).
- A pipe's model takes the bodies' light, not its cell's, and is darker than
  the lit blocks beside it; from the room's glass the bonus room under 1-1 is
  in plain sight.
- Whoever joins the room directly faces the doorway, not the glass.
- core.side_view keeps the body on its plane by walking it back: something
  that throws it far off (a blast) is not undone at once.
- `assets/meshy/platformer` (three .glb, 9.8 MB, with each one's prompt) is
  not in the repository yet: in, or out as contrib/lizard's is, is Giacomo's.

Arcade shooter:
- No power-ups, no bombs, one gun. The numbers (speeds, 0.14 s a shot, the
  bomber's 40) are guesses. Weavers, aces and the bomber not seen in play.
- A hit is a box about the plane, not its shape; shots are checked 20 times a
  second.
- core.top_view flies the body while it is on and F does nothing; getting out
  on the deck on foot is not possible.
- The picture is 16:9's: on a narrower screen the field's sides are cut off.

## Defects

Seen by players first:
- Models at join: fixed 2026-11-19, by script only. A draw that misses a
  model queues it (ModelStore::entry) and the reading serves that queue before
  the table's order: in the sandbox 31 of 882 were read ahead for what was in
  sight. A model block whose model is not read yet draws its boxes, not
  nothing. Not looked at by eye: how long a creature near the spawn stays a
  box now (the reading of the rest took 19 s at Xephyr's 11 fps).
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
- Lizard man (Meshy, 2026-11-11, an add-on in `contrib/lizard`, not in the game):
  seen in a client, on the look pages (title and tools/track.sh's world, its
  three styles) and in the sandbox and the track room of track.sh: walking on
  the borrowed clips, the tail wagging slowly and trailing out of a turn; cap,
  top hat, scarf and frill on. The bow tie and medal in Blender only, and the
  tail's lift at a run is not looked at from the side. Its source
  `contrib/lizard/meshy/lizard.glb` (16.6 MB) is not in the repository.
  Since 2026-11-12 it says `rooms = all`, and every room of a server started
  from the title screen has it, not only the sandbox and the pocket halls;
  each server is told its own number for it (4 on one with only it, 5 with
  another add-on before it, the robot where it is missing), checked against
  two servers with --addon-species. A client that cannot read an add-on's
  body or arms counts one species fewer than the server and draws the next
  add-on's players one off.
- Guns in first person on every species (2026-11-14): in the front and the assault the species'
  arms -- human, mole, frog, and the add-ons fox and lizard -- play the world's arms' clips made
  again for hands (`robot_arms_hands`, tools/make_front_hands.py: hand and forearm one stiff
  piece turned from the gun, nothing turning at the wrist) and the guns are drawn out of that
  file (draw_arms; docs/MODDING.md). Seen in a client in the assault alone, stills of every
  species with rifle, pistol, lmg, sniper and launcher at rest, aimed, firing and reloading and
  the grenade at rest and thrown. Not seen: a clip in motion, the knight, a seat in a machine,
  the front with a gun in hand (only its lobby), yafps and the lab since the file was made
  small enough to be served. Known: the left hand is a fist under the gun, not fingers round
  it; the shoulders are wherever the hands leave them, so an upper arm may come into the view
  in a wide swing (a reload, the throw). The file is 938 KB of the 1 MiB a server hands out:
  a gun or a clip more in tools/make_front_models.py may take it over, and the tool then
  stops.
- Fox man's arms (2026-11-14): the palms were turned in by wringing the forearm 90 degrees
  elbow to wrist, which showed at rest (Giacomo, on the look page); now the whole arm is turned
  about its length, its top drawn in at the shoulder. Seen in Blender at rest and on eight
  clips; not yet in a client.
- Fox man (Meshy image-to-3D, 2026-11-14, an add-on in `contrib/fox`, not in the game):
  seen in Blender at rest and on eight of the robot's clips, with its styles and the six
  accessories, and in a client in tools/track.sh's sandbox, in first person and from behind in
  third. Its head is not Meshy's (a torn shell) but the closed solid round its points, and no ray
  from outside meets the inside of the body at rest (a check with the back faces culled; a few do
  where a joint folds). Not looked at in the game: its front, the look page, the styles and
  accessories on it, the tail's wag. Giacomo has not judged it against the picture yet.
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
  Move what is shared into one library both load. Done 2026-11-19:
  `games/front/mods/front_game/battle.luau` (the guns, grenades, blasts, the
  bots' shared moves and every machine), and assault_game's `battle.luau` is a
  link to it; both `require("@self/battle")` and say what differs with a few
  values and hooks (`battle.server`, its `Game` type). front_game's init.luau
  is 1201 lines (was 3251), assault_game's 3426 (was 5438), battle.luau 2348.
  What stayed apart: a bot's mind (`think`, `step_bots`, where it goes), a bot
  at the wheel (`M.bot_drive`), the fleet (`M.start`), each game's rules, and
  the whole player's machine (its HUD, its drawing, what it hears): the client
  half is still two copies, a smaller job for later. The engine packed a linked
  file as `../../../front/...` and did not offer the mod to players
  (src/modpack/mod_package.cpp now keeps the name it has in the mod; mods_play
  fails on a world's code not offered), and `tools/typecheck.sh` follows a link
  as the engine does.
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
2. Virtual mouths. Done 2026-11-12 for portals made while the server runs
   (`src/server/room_portals.*`, `schedule_mouths`): a portal between rooms
   holds a mouth only while a player is within 160 of either end, nearest
   first; the mouths to hand out are those past the world's own and the
   world's own a mod parked (`core.portal_park`: gates' second ways in on a
   plaza, pocket's mouths with no door). Server only, no protocol change.
   Seen in a client: through a frame of room b into room a's, on a lent mouth;
   the lender placed again and the portal moved to another mouth, the far end
   following. A survival sandbox's gates park every mouth and arch
   no site near a player wants (2026-11-19, `mods/gates`), so it has mouths
   to lend; seen by script: both mouths parked once at the start. Missing:
   portals to other servers still hold a mouth each (Limits); a player
   arriving in the half second a lent mouth changes hands comes out of
   whatever holds that index; a room that is a pair of openings and no mouth
   cannot have one.
3. A portal to another room from the frame's menu. Done 2026-11-12
   (`mods/portal`): "other rooms: yes" by whoever has the `portals`
   permission, then the other rooms' frames listed to link to; kept by each
   room's mod, a link made from the far room taken within two seconds. Seen in
   a client by script (the clicks sent as events, the crossing walked), the
   menu itself not looked at. Missing: only rooms that load `mods/portal` have
   frames (sandbox, inferno, pocket halls); such a frame is empty, with no
   membrane, while nobody is near; who may cross; tickets between rooms
   required and bound to the room (see Waiting for a decision).
4. Rooms from a file, started and stopped while the server runs. First part
   done 2026-11-12: `--rooms-file` (`[room NAME]` and its flags), a room started
   when a player nears a portal linked into it, joins it by name or by
   `/room start`, saved and stopped after a minute alone or by `/room stop`,
   `/room add|remove|list` writing the file; a stopped room started again from
   its world file. Seen with a client: the start from a portal, the crossing,
   the stop when left. Second part done 2026-11-12: a template (`[room
   home-*]`, a room made from it for any name that begins so, its world file
   and seed from its name); a room that is down gives its place up when all 64
   are taken (not tried: no run had 64); the personal room of each player
   (`mods/home`, "my room" in a frame's menu, `mods/portal`): the builder of a
   standing frame, 1 x 2, links it to the door of `home-<key>`, moves the way
   in by saying it of another frame; the title screen's worlds
   (`rooms.conf`, written by the client) and `tools/track.sh` have the
   template. Seen with a client by script (the click sent as an event): the
   room made, the walk through the frame out of the room's door, the stop
   when left, the start again by name, a name with no world refused; the
   title screen's world starting with its file. Seen with a client under
   Xephyr 2026-11-13 (/tmp/cnp_home): the frame's menu with "my room", the
   room started from it, the walk in, out of the room's door, back in through
   the door and out of the frame; the menu kept saying "it opens in a moment"
   once open (fixed). A level of zones laid in a client's world, seen only in
   part (the test's camera cannot look down). Missing: the client's title screen and
   `tools/track.sh` still write their own list of the rooms they start with;
   new rules (PvP, who may build, flight, a fixed
   hour); one page for all the rooms (each room's World page is its own:
   an admin stands in a room to set it). Since 2026-11-12 rules are
   declared by the mod that reads them (`rules:declare`: hunger, weather,
   grave) and every themed world loads `mods/rules` (a link in its mods),
   so `/difficulty`, `/rule` and the World page are in each room, kept in
   that room's `<file>.difficulty` and `.rules`: seen by script (the twelve
   rooms load it, the list of rules on a server), no page looked at, and
   which themed worlds' creatures heed the difficulty not gone through. Of
   the personal room:
   its door stands where the world put it, 3 north of the spawn, on a plaza
   in a world that has one (the player moves the other end only); a frame
   broken while its builder is inside: `/myroom leave` (2026-11-14), out to the
   first room's spawn (core.room_send, protocol 125); who may cross and look:
   `/myroom` whitelist or blacklist and look yes|no (core.room_access), held at
   every frame leading there and by the room, seen with two clients
   (/tmp/cnp_home: red membrane, look without going in, let in once removed
   from the list, the way out after the frame was broken); frames of one
   player in two rooms both wish for the door and the first has it; gates,
   keys and pocket load there as in the sandbox and lead nowhere; the
   template's mods are those of the client's last start.
5. The zone (`mods/zones`). Done 2026-11-11, by script only, in no client: a box
   saved with its doorways (`zones:doorway` in a side) and markers, turned and
   mirrored with every block's facing; a level laid from a seed, an "end" zone
   last; `zones:lay` / `zones:mark` / `zones:laid`; a player's zones private
   until `/zone approve`. 196608 cells saved and read back in one call, so
   OPEN-42 does not bite at 64 x 48 x 64. Missing: a level laid by a mapgen
   stage for a survival slot (it is written at run time, 4000 cells a tick);
   zones shipped with the game: none yet, the way is there since 2026-11-13
   (`tools/zones_ship.py` writes `mods/zones/shipped.luau` out of a world
   file; seen by script: a zone made by hand in a store, shipped, laid on a
   server that never saved it); doorway kinds other than "door"; levels on more
   than one floor are only what the zones' own doorway heights give.
6. The workshop room: an editor in the game, a "try" button, an admin's approval.
   Decided 2026-11-19: one per player, a template `workshop-*` like `home-*`
   (flat, flying, blocks without end, no survival rules), entered from the
   frame's menu ("my workshop") or `/workshop`, left with `/workshop leave`; a
   wand picks the box by two corners, a panel sets name, world, role, d, weight,
   mirror with Save, Try, Propose; Try is anybody's in their own workshop (a
   level of their private zones and the approved ones laid beside, the player
   put at its start, Back to where they were), elsewhere still an admin's; a
   zone proposed is a status between private and approved, the admins told
   (mod_notice) and given a list with try, approve, refuse; saved over, it is
   private again.
   Done 2026-11-19 (`mods/workshop`, `mods/zones`), seen by script only, in no
   client: /workshop from the sandbox started workshop-KEY and sent the player
   (core.room_send now starts a room of the file); in it the flat floor, no
   hunger, no weather, peaceful; the wand's two corners, Save (save and the
   three sets), Try (a level on the trying ground, the player at its start
   marker), Back, a second Try after the first was taken away; a block dug gave
   nothing (driven by a scratch mod emitting core:used and core:ui_click: the
   probe has no panel). The admin told on coming in, show, refuse, the maker
   told. Not seen: the panel and the drawn box on a screen; a block put down
   coming back (the probe places nothing). Then, the same day: "my workshop"
   (or "leave the workshop") in every frame's menu, through "workshop:go" (seen
   by script: the click from the sandbox started the room and sent the player);
   the workshops' template says `no-plaza` (a new flag: core.plaza() nil, so the
   mods set no tables, boxes or benches on the floor -- seen: 16 blocks round
   the spawn without it, none with it); the owner refused by name was the
   session's name read after the room's rule asked (the name is --owner's),
   now set before it -- seen: --owner bruno let into anna's workshop, and told
   it is somebody else's. Missing: a list's "show" lays beside the admin
   wherever they stand.
   6b. Tools made for each kind of world. Chambers decided and done
   2026-11-19 (with 7's chambers): the workshop loads the worlds' block mods
   (games/workshop/mods, links; chambers_core leaves its world's rules out in
   a workshop) and a brush puts down a palette's blocks, the bag untouched;
   joined by rule, not by hand: each button, push button, target tile and
   turret (marker "mob") holds the nearest door, a door nothing holds opens
   as somebody comes near (chambers_game, "levels of zones"; zones:watch /
   zones:block say where the blocks went). Seen by script (a zone built by a
   scratch mod, saved, approved, laid by /zone try, a site's keys:level and
   the series): the door held by a body on the button and shut again, the
   exit done, a site's way out built, the pit back to the start, a level laid
   again over its last without what it held twice; a plate's throw worked
   out towards the target (not flown: the probe does not move). Not seen:
   the brush and the palette in a client, a turret of a zone, a push
   button's clock, the vent's cube in a level, the more button pressed by
   hand (its call made from a scratch command). Left: the dissolving field
   and the fixed portals in zones; Try in the workshop lays the geometry
   only (chambers_game is not loaded there); no chambers zones shipped yet.
   Arpg done 2026-11-19 (with 7's arpg): its blocks in the workshop
   (arpg_core leaves its world's rules out there), and a site's lands of the
   approved arpg zones when there are any (else L.add_level as before): one
   area at the site's level, a pack at each "mob", the lord and four at
   "exit" (down: keys:cleared and a portal to the start), a chest at "loot",
   a thing to break at "trigger"; a zone's floor is its lowest layer, laid at
   GROUND - 1 (everything in the arpg stands at GROUND). Seen by script: a
   zone saved, approved, a site's keys:level laid it, the player at its start,
   the chest, the pack, the lord and its four, the barrel. Not seen: the lord
   killed (the probe does not fight), the brush's arpg blocks in a client.
   Left: lands laid again after a restart over the last without clearing
   (the same seed and zones lay the same; others could leave walls); an
   admin's /zone try adds an area each time; corridors between zones are the
   zones' own doorways, no waypoint in zoned lands. Found on the way: a
   workshop with the sandbox's mods and the chambers' and the arpg's whole
   came to 1082 models, past the 1024 a welcome carries, and refused every
   client; a world's block mod now stops after its blocks in a workshop (888
   models there now). Each world added there has to keep to that.
   Backrooms done 2026-11-19 (with 7's): their blocks in the workshop (the
   backrooms mod returns after its blocks there), and a fresh site's level of
   the approved backrooms zones on Level 0 (backrooms_exit, kept as
   "zslot:N" in the plan's shape): in at "start", the way out four west, the
   radio to "exit". Seen by script: laid, the player at the start, the way
   out's hole, the exit found (keys:cleared). Not seen in a client; the
   lights are the zones' own blocks, no rule.
   Parkour done 2026-11-19 (with 7's): its blocks in the workshop (the mod
   returns after its blocks there), a fresh site's course of the approved
   parkour zones at LF ("zslot:N": start, summit, pads): pads nearest the
   start first, "exit" the summit. Seen by script: laid, three pads, a fall
   back to the last pad, the summit done. Not measured: whether a zone's
   jumps can be made (the builder tries them in the workshop, where the
   parkour moves are not on: parkour_core is not loaded there); the summit's
   doorway opens on the summit itself.
   Next (Inferno: fortress pieces; front and assault: bases and points).
7. Each world onto zones: chambers (done 2026-11-19, 6b), arpg (done 2026-11-19, 6b), backrooms (done 2026-11-19, 6b), parkour (done 2026-11-19, 6b), Inferno, then the
   front and the assault after their shared library (Structure).

Tickets between rooms: decided and done 2026-11-19 (protocol 127). A room past
the server's first takes a body only with a ticket signed for it by name (the
room is under the signature, ticket_challenge; "" signs what it always did, and
the first room takes that too), with or without --require-ticket; joined
straight it is only looked into. The client now asks a ticket for a room of
its own server. Seen: the real client (Xephyr, --mouths 0) crossed a into b on
b's ticket and was given one back; the probe's new --room b refused, --room a
let in; a ticket signed for "a" refused at "b" (protocol_test). Not seen:
core.room_send's redirect followed by a client (the probe follows none).

Rooms starting while others run: fixed 2026-11-19. A room started from the
rooms file held the rooms' lock for its whole start; an inferno room stopped
the two rooms already up for 2.9 s. It now lets go after each mod it loads
(ModHost::set_between_mods), after each startup column its mods' stages
generate, and while it builds its media; the rooms the server starts with
still start one after another (the first's store is everybody's
core.shared_storage). Measured: the worst tick of the rooms already up
214-258 ms (one column's five stages), down from 2.9 s.

A player's own room is a room of the server with a world of its own, one a
player (not the pocket hall, which stays what it is): made the first time its
portal is loaded, let go when nobody can reach it. Built 2026-11-12 (4).

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
  under `#version 300 es`, tryable on the desktop. Assessed 2026-11-16, not
  started: libraries all build on the NDK (not installed); 22 shaders are
  `330 core` with 23 `gl_ClipDistance` uses, `glDrawBuffer` has no ES3 twin;
  world servers start by fork/execv (Android forbids exec from app data: ship
  as `libcnp_server.so` or run in a thread); 150 MB of assets/mods/games read
  as files (extract on first run, asset pack past Play's 150 MB); mouse and
  keyboard only (touch controls need design); likely lighter profiles on
  mobile GPUs.
- Before any release: `ALL_OPEN` and `FRONT_OPEN` false in `mods/keys` (both
  false today), a new `kEngineVersion`, CHANGELOG in both languages, a tag.
