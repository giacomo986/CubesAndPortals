# The assault

*[Leggi in italiano](README.it.md)*

The final assault on Basoalth's dimension, played together. Two mods: `mods/assault_core` (server
only: the valley, the fortresses, the core's hall, the escape tunnel) and `mods/assault_game`
(`side = both`: the phases, the credits and the shop, the hall, Basoalth, the escape on the server;
the HUD and effects on the client). Weapons, grenades, bots and machines are the front's
(`games/front`, `front_game`), copied and turned to a game against the computer.

## How to get there

`tools/track.sh` (or the title menu) starts the sandbox with its rooms; the assault's door, a gate
of black basalt with two violet embers, stands at 90.5 21 76.5 (`mods/gates`, the sandbox's
seventeenth mouth). Like the front, in the finished game it opens only to whoever has beaten the
Host in the laboratory (`FRONT_OPEN = true` in `mods/keys/init.luau` opens both, to try them;
`false` is the game as it ships). In a survival world the assault stands behind portals scattered
past 1500 from the spawn, shut until the ending; every garrison has 10% more for each step of the
site's difficulty, and the victory is the level done.

The world is not saved (no `--world-file`): when nobody has been there for half a minute its
columns go back to what the mapgen makes, and the dimension is whole again. The match's state is
reset 20 s after the last player leaves.

**Trying only the escape**: `tools/escape.sh` starts a server with only the assault's world and a
client; three seconds after you are in, `tools/assault_escape` skips the fortresses and the hall
(event `assault:to_escape`): you are in the hall, the core breaks, Basoalth rises, the floor
collapses and the jeeps wait in the tunnel. It starts again every time somebody comes back into an
empty world.

## The valley

128 nodes wide, 1080 to the north; to the east a magma rift along which the escape tunnel runs.
All round, up to 110 nodes out, a chain of mountains rising as it goes away (up to 48 over the
ground) and nine volcanoes: a cone 66-90 high, a crater full of magma and three flows winding down
towards the valley (`VOLCANOES` in `assault_core`). A lava river crosses the valley between the
bastion and the battery (z 590-610, winding): it comes down a gorge in the western mountains, runs
three blocks below the ground between basalt banks and falls into the rift to the east. It is
crossed on the road's bridge (9 wide, parapets and lamps) or, at the sides (x 22 and 104), by
jumping across three basalt islets one or two blocks apart. The lava (`assault_core:lava`) kills
players and soldiers and destroys machines; bots driving avoid it. The sky is red
(`core.world_storm`, protocol 108): the sun is still there, but the clouds are the sky's colour and
turn in vortices here and there: each turns whole (a turn every half minute or so) wrapped in two
spiral arms, with a clear eye in the middle. Some threaten a tornado: a dark wall in the middle and
under it a funnel coming down towards the valley and going back up (down to 44, never to the
ground), every minute and a half or so. Cost: with llvmpipe in Xephyr the camp goes from 25.7 to
21 FPS.

| phase | where (x, z) | what | the land round it | garrison (1 player, +2 for each more) |
|---|---|---|---|---|
| camp | 64, 22 | a concrete yard, the arrival door, the crates | meadow | -- |
| 1 the outpost | 64, 250 | a log palisade, two turrets, a hut | dead marsh: mud, pools, reeds, dry trees | 8, two machine guns |
| 2 the bastion | 64, 480 | stone walls, towers, a keep | burnt forest: ash, charred trunks, embers | 11, two machine guns, two mortars, a tank |
| 3 the battery | 64, 710 | three bunkers, trenches, two howitzers, anti-aircraft | a field of glowing violet crystals | 14, two machine guns, a helicopter |
| 4 the fortress | 64, 905 | basalt walls 12 high (indestructible), the gate to blow up | basalt split by magma, ribs of bone | 18, two tanks, mortars, anti-aircraft, a helicopter |
| the hall | 64, 976 | behind the fortress: four pylons, the core, the throne | -- | waves from doors in the side walls |

The legion is of three kinds, chosen for how they fight: **zombies** (behind cover, slow, 28
health), **demons** (on the attack, fast, 18) and **cyborgs** (on watch with the sniper rifle and
the rocket launcher, 26).

## How it plays

* **A fortress** is taken by standing at its flag (10 nodes) with nobody of the legion within 20:
  the capture rises by 10 a second for each ally, up to three. Taken, the flag turns blue, what is
  left of the garrison runs, the base moves there (you respawn there, the crates refill) and the
  next one arms itself. A fortress's gate has to be blown up with grenades, rockets or a tank.
* **Credits**: one purse for everybody. An enemy killed by a player 10, by an allied bot 5, a
  fortress 150, a pylon 100; and a slow supply, 5 every 5 seconds while the fighting is on
  (fortresses and hall).
* **The base** is on the open ground north of the last fortress taken, on the road to the next:
  there you respawn, the crates refill, fallen allied soldiers come back in waves every 15 s and
  allied machines (bought ones too) each have their place in the car park. An allied machine with
  no players aboard, standing for 40 s away from the car park (stuck, in a hole), goes back to it.
  The outpost and the bastion have a gap to the north too, the battery's trenches one on the road.
* **Shop** (B within 30 nodes of the base): five soldiers more (150, up to 100), a builder (120, up
  to 6), the light machine gun (120), the sniper rifle (120) and the rocket launcher (200) for
  everybody; upgrades in levels, each dearer than the last: weapons +15% damage (150, x1.6, up to
  8), machines +25% toughness (180, x1.6, up to 6), blast armour -15% (140, x1.7, up to 4); a jeep
  (100), a tank (350), a helicopter (500). You start with rifle, pistol, grenades, four soldiers and
  two builders.
* **Bonus zones**: at the sides of each fortress (x 10 and 118) a clearing with a beacon, held by
  three of the legion; its beam is seen from afar, violet or blue. Taken (standing at the beacon
  with no enemy within 14), +50 credits and +3 to every supply. One ally in three goes to take them
  on its own.
* **Counterattacks**: every 100-140 s, while the fighting for a fortress is on, a jeep comes out of
  it (two with more players, or sometimes) full of fresh soldiers making for your base, drops them
  there and they fight.
* **Builders**: unarmed and tougher allies (50 health), with the wrench: they go to the craters
  behind the front (bases taken, the road) and put them back as they were. Two at the start, more
  in the shop (up to 6). Half of them see first to building for the fortress to take: a ladder on
  the south face of what stands 12 and 22 nodes from the middle (a wall, tower or bunker 3-16 high
  with room on top) and four nests of sandbags 25 nodes before the area. A builder standing still
  for 3 s takes a step aside; a job it cannot reach in 25 s it leaves for 45 s; a dead one's job is
  free again. Tried: at the outpost two ladders and four nests.
* **Barracks**: allied soldiers and builders come out of a plank barracks 21 nodes west of the
  base, its door to the east. The camp's is always there; the others appear when the base moves
  there (`assault:barracks`), and from then on mapgen and repairs count them as part of the
  dimension. Open: not tried whether a client that came in later sees those of the bases taken.
* **The gate**: before the fortress's gate, within 30 nodes, one ally on foot at a time goes up to
  7 nodes and sets off a grenade in the nearest gate cell left in the middle, every 3.5 s; if it
  does not manage in 15 s another takes over. Tried: open in about 45 s. `ponytail`: the grenade
  is put in the gate, not thrown (thrown it bounced back, laid at the feet it fell into the crater).
* **Respawn**: whoever dies (shot, blown up, fallen, in the magma) comes back with every weapon
  bought and magazines, reserves and grenades full. The deaths the engine gives are recognised by
  the jump to the respawn point.
* **Tornadoes**: while the fighting for a fortress is on, every 45-85 s (the first after 30) one
  comes down, at most two at once, on the open ground between the base and the fortress; it wanders
  at 1.5-2.5 m/s for 45-75 s. Within 14 nodes it sucks in and spins players, soldiers of both sides
  and machines (tanks barely); at its middle (3.5) it throws up and out and takes 7, every 2 s.
  Tried: a player rises 4.4 nodes, is thrown 10, sucked in and dead at the third throw; four
  soldiers dead in a few seconds. A player's throw stays under 16 m/s an axis (`net::Push::kMax`).
  The cloud pass draws it (`core.storm_funnel`): a funnel from the clouds' belly to the ground,
  dust at its foot. Cost: with llvmpipe in Xephyr two tornadoes near bring the camp to 11 FPS (21
  without).
* **As a passenger** (a seat without the machine's weapon) you shoot with your own weapon and
  1-9 choose it; they change seat only at the wheel or at a weapon of the machine. At the start of
  every escape attempt everybody's weapons are full again (there are no crates on the road).
* **Test credits**: `tools/track.sh --fresh` puts `tools/assault_test` in the room too, and every
  match starts with 100000 credits.
* **In machines**: whoever is not driving and not at a weapon of the machine shoots with their own
  (players and bots), and aims with the right button. Seated bots are carried by the engine
  (`core.mob_ride`) along with the machine.
* **The hall**: the four pylons feed the core's shield; brought down, the core is open for 18 s,
  then the pylons light up again, stronger. Basoalth throws orbs from the throne, which a shot puts
  out.
* **The rise**: the core broken, Basoalth takes its energy, grows to 34 metres and breaks through
  the roof; then the hall's floor collapses and everybody falls (unhurt: health comes back full
  before and after) among heaps of rubble, to the bottom of the escape tunnel.
* **The escape**: three jeeps at the bottom of the tunnel, one drives and the others shoot; alone,
  a bot drives or takes the machine gun. The tunnel (`assault_core`, `TUBE`; `A.PATH` in
  `assault_game`) is 24 wide and 44 high, because the giant walks in it upright: north under the
  garage, east under the ridge, then down the rift in turns to the right and to the left (15
  right-angled turns, level for 12 nodes round each corner), rises and falls between -2 and 10
  (never more than a block every 8 nodes), to the rock with the way out, in the open. Lamps along
  the walls. It is a drain for the lava: the road is 13 wide in the middle, at the sides up to the
  walls the magma runs two blocks lower (the turns are road from wall to wall, so whoever cuts the
  corner does not fall in); whoever ends up in it, on foot or in a jeep, is lost. After 14 s the
  giant moves and the floor collapses behind him (9 m/s, up to 14, +6 when one is more than 50 m
  ahead); every few seconds a punch opens a hole on one side (marked red 1.6 s before), and his
  orbs chase the jeeps. Every hit on him slows him (a machine gun keeping him under fire brings him
  to about three quarters of his pace, two to under two thirds, never under 40%; the head counts
  double) and enough hits on the head make him stagger for 3.5 s. All caught: only the floor is
  made again (in about a second), the jeeps come back 2 s later and you try again from the bottom of
  the tunnel. A jeep on the rock at the end, and the door home opens for everybody.
* **Traps in the tunnel**: every 110 m or so, in turn, a target to hit, made again at every
  attempt (bots aboard shoot at them too, at the right moment: the boulders at once, the valve with
  him within 150 m, the tank within 55): the magma **valve** on a wall (40 health) opens a jet of
  lava across the tunnel for 25 s, and Basoalth passing through it goes at 45% for 5 s; the
  **cluster of boulders** under the ceiling (60) falls into the channels, and he trips on it and
  staggers 4 s; the **gas tank** in a channel (50) explodes at once, and if he is within 40 m it
  slows him a lot and makes him stagger 2.5 s (too early, it is wasted). For each Basoalth has his
  animation (`tools/make_assault_models.py -- --giant`): in the lava he stamps his feet and beats
  his legs with his head thrown back (burn), on the boulders he bends forward to the ground and gets
  up again (trip), the tank throws him back and he raises his arms before his face (blast). Tried
  by hitting each at 25 m: with a bot driving you get to about 900 m instead of 500. Tried with a
  bot driving and nobody shooting: about 1100 of the 1440 m before being caught (500 before the
  steps had ramps three cells long, `core/physics/craft.cpp`).

## The models

`tools/make_assault_models.py` (Blender): zombies, demons and cyborgs, Basoalth and the giant, the
core, the pylons. The rest is the front's (`tools/make_front_models.py`) and the laboratory's (orbs
and shield, `tools/make_ospite.py`).
