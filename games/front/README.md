# The front

*[Leggi in italiano](README.it.md)*

A battlefield in the style of the big team shooters: two bases, three points to take, red against
blue. Two mods: `mods/front_core` (server only: the field, the blocks, how bodies move) and
`mods/front_game` (`side = both`: teams, weapons, grenades, bots and points on the server, the HUD
and the effects on the client).

## How to get there

`tools/track.sh` (or the title menu) starts the sandbox with its rooms; the front's door, a bunker
door between sandbags under a red and a blue flag, stands at 90.5 21 58.5 (`mods/gates`, the
sandbox's sixteenth mouth). You come into the **gallery**: a room on the western ridge, halfway
between the two bases and 16 nodes above the field, with a window to watch the battle from. Who is
in the gallery only watches: on no team, unseen by the bots. On the back wall are two portals, a
red one and a blue one, and inside each you see its base. Walking into the red one you come out in
the red base's bunker and fight for the reds; the same with the blue. The bunker's portal takes you
back to the gallery and out of the fight; from there you can choose again. The door you came in by,
at the gallery's south end, leads back to the plaza. The front gives no key.

In the finished game the door opens only to whoever has beaten the Host in the laboratory
(`mods/keys`: the laboratory sends `keys:ending`, kept for every room as `ending:<who>`).
`FRONT_OPEN = true` in `mods/keys/init.luau` opens it to everybody, to try it; `false` is the game
as it ships. In a survival world the front stands behind portals scattered past 1500 from the
spawn, shut until the ending, and the site's difficulty sets the bots a team (4 + d / 2) and the
tickets (100 + 10 d); a round won by the player's team is the level done.

## The field

256 × 256 nodes closed in by a ridge. The red base to the south (128, 24), the blue to the north
(128, 232): a concrete yard with the team's stripes, sandbags, a bunker and the flagpole. In the
middle, on the road between the bases:

| point | where | what |
|---|---|---|
| A | 48, 128 | a farm between stone walls, a wooden barn |
| B | 128, 128 | the village at the crossroads: six brick houses, a square with a well |
| C | 208, 128 | a concrete bunker on a hill, sandbags all round |

Two stone walls 2 high cross the field at z 86 and 170, with gaps on the road and here and there.
Everything can be blown up, except the floor and the ridge.

## How it plays

* **Tickets**: 150 a team. Each death costs one; the team holding more points makes the other lose
  a ticket every 4 s (2 s with all three). At zero the team loses the round, and 10 s later
  another begins.
* **Points**: whoever is within 10 nodes counts; each soldier more than the other team pushes the
  capture on by 10 a second, at 100 the point is taken and the flag changes colour.
* **Weapons** (in hand from the start): the rifle and the machine gun fire while the left button
  is held, the pistol and the sniper rifle a shot a press, R reloads. A head counts double. From
  the hip the shots spread (the crosshair shows it); with the **right** button you aim: the weapon
  comes up to the middle of the view, the view narrows, the shots go where you look and you walk at
  1.8 m/s. The sniper rifle, aimed, looks through its scope (5x) and does not miss. No friendly
  fire. In your own base's yard ammunition and grenades refill.
* **Grenades**: they bounce, go off after 3 s, dig a sphere of 2.6 and bring down what is left
  hanging over the hole.
* **Bots**: each team is filled up to 8 with soldiers the mod commands (you see the weapon they
  hold); they go from point to point and now and then throw a grenade. They see only ahead (a cone
  of 110 degrees, within 48 nodes, 90 with the sniper rifle) or within 4 nodes; one that is hit
  turns towards the shot. A moment passes before the first shot (0.45-1.1 s), they fire in
  bursts, wide at first and then tighter, and a shot of theirs does three quarters of a player's.
  Four ways of fighting, in turn: the **sentry** stops now and then to look around, prefers the
  sniper rifle and takes the nests, mortars and howitzers it finds; the **assaulter** runs at
  whoever it sees with the machine gun; the **careful** one goes to cover first, leans out for a
  burst with the rifle and goes back behind; the **anti-tank** one starts with the rocket
  launcher. The bots' mortars and howitzers fire at what a bot of the team saw in the last 3 s.
  Each keeps its weapon while it has rounds, then moves to the next (in the end a pistol picked up).
* **Bots in machines**: they drive jeeps and tanks to their point, waiting a moment for nearby
  comrades to get in; the jeep drops them at the point, the tank stays and stops to fire its gun
  at whoever it sees within 70 (a little off, every 5.5-7 s). They fly the helicopter (18 over the
  point, rockets at what is ahead), the plane and the bomber (round over the point, bombs where they
  would fall on what the team sees). They take the weapons and free seats of a machine of their
  team standing nearby, even one a player drives, and get out if nobody drives; they take the
  anti-aircraft gun when an enemy flies near. They leave jeeps and tanks to a player of the team
  within 25 nodes.

## Models

`tools/make_front_models.py` (Blender, with the helpers of `tools/make_fps_models.py`): the
robot's first-person arms with pistol, rifle, machine gun and grenade; the weapons in hand; the red
and blue soldiers with idle, walk, run, fire, throw and death.

## Machines and heavy weapons

You get in and out with **F** near a machine, on the nearest free seat (from behind the jeep you
get in at the back); **1-9** change seat, while driving too, without changing the weapon in hand.
`/mezzo <type>` puts a machine in front of you, for testing. The first seat drives and has the
machine's weapon; WASD drives on the ground, space and shift climb and descend, the mouse steers
what flies.

| | where | seats | weapons |
|---|---|---|---|
| jeep | two a base | 4 | machine gun at the back |
| tank | one a base | 2 | gun (the driver), machine gun on the turret; breaks through walls by pushing against them |
| helicopter | west of the base | 3 | rockets (the pilot), machine gun at the door |
| plane | on the road north of the base | 1 | machine guns, bombs with the right button |
| bomber | at the end of the north road, behind the plane | 2 | the pilot drops a row of six bombs (a red pole marks where they would fall), a machine-gun turret on its back |
| anti-aircraft gun | two in the base's yard | 1 | missiles only at what flies: hold the sight on an enemy helicopter or plane for 1.2 s (LOCKING), then LOCKED and four missiles that chase it |
| machine-gun nest | at the sandbags along the roads | 1 | |
| mortar, howitzer | in the bases | 1 | fire where the sight looks (a red pole marks it) |

The infantry has the rocket launcher too. A destroyed machine explodes: the wreck burns, sheds its
parts (the tank's turret flies off, the jeep overturns) and sinks in 5 s; a new one comes back home
after 30 s.

## Still to do

Two machines do not collide with each other, and a player on foot walks through a machine:
TODO.md, "The front's machines". A bot in a machine is put back in its seat by the mod every tick
(the engine's seats are players').

## Sounds

`tools/make_front_sounds.py` synthesises them (shots, explosions, engines, the rotor, the
soldiers' voices) into ogg files beside the mod.
