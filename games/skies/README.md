# The cloud world

*[Leggi in italiano](README.it.md) · [Leia em português](README.pt.md)*

Islands without end over a sea of cloud: villages of owl men, sky crystals, updrafts, and at the
end of the sky road the Cloud Manta, a boss for one player or several. Whoever is there when it
falls gets the storm glider, theirs to carry out. One mod, `mods/skies` (`side = both`):
`layout.luau` the world, `init.luau` the ground and the game on the server, and on a player's
machine the sea of cloud, the updrafts, the shots and the Manta's health. The sandbox's mods are
linked in (robot, tools, archery, armour, the glider...), so your bag and weapons come with you.

## How to get there

Its gate is on the plaza's south edge, at 69.5 21 26.5: a white arch like a cloud with planks
before it (`mods/gates`). Like the platform game it is in the rooms file: it starts when somebody
comes near the gate and stops a minute after the last one left. You come out of a doorway on the
home island, at the origin, looking north up the sky road.

## The world

A cell of 56 by 56 at a time, each a function of the seed and the cell (`layout.luau`):

* **Islands** in most cells, 5 to 11 wide, 30 to 42 high; some bridged to their east or north
  neighbour (planks three wide, log posts). Some have a tree; many have **sky crystals** standing on
  the grass, picked by hand.
* **The sky road**: an island in every cell straight north of home, a bridge from each to the
  next, up to the Manta's islands, 420 north.
* **Villages**: one in three of the wider islands has two huts (planks, logs, a wool roof) and three
  owl men.
* **Updrafts** in a good share of the cells, over the gaps, never over an island: each wanders
  slowly, 9 nodes about where it stands. Drawn as pale blue puffs spiralling up, streaks of light
  running up through them and a ring on the clouds. Whoever is in one (3.2 from its middle) is
  thrown up at 15.5 m/s to 58: from 24 to 58 in 3.5 s. A way back from a fall, and with a glider
  a way up.
* **The sea of cloud** from 6 to 22: the sky's own clouds drawn a second time, lower and nearly
  whole (`core.cloud_sea`, new in the engine). A body that falls past 18 is put back where it last
  stood (on the nearest island when that is gone) and loses 4.

Nobody digs or builds here; a sky crystal is the one thing a hand takes.

## The owl men

From Meshy (a concept picture approved, image to 3D, the auto-rig's walk and run;
`tools/make_meshy_boss.py owlman` gives the rest: idle with the head turning, wave, a wing sweep,
glide, hurt, death). They walk about their island and now and then glide to another within 75
(the "glide" clip, on an arc), and live there for a while. A right click on one: it turns and
waves, and says how far the Manta's islands are and how updrafts help; and it trades:

| give | get |
|---|---|
| 2 sky crystals | a glider |
| 6 iron ingots | 3 sky crystals |
| 5 sky crystals | a diamond |
| 3 sky crystals | 16 planks |

A village fills when somebody comes within 80 and empties when nobody is within 130.

## The Cloud Manta

12 m from wing tip to wing tip; 400 health, and 60% more for each player more near its islands.
While it is fought, its health is a bar at the top of the screen of everybody within 160.

1. **High**: it circles at 58 round its five islands, out of reach. Every 6 s it blows a gust at
   somebody on a bridge (said a moment before): a ball of wind seen flying from it, and whoever is
   still within 3.5 of where it lands is thrown 12 m/s away. A right click on a lightning rod calls
   a bolt down onto the rod and from the rod into it (seen, with a flash of the sky): 12% of its
   health, and squalls. It comes down 3 with every rod. The first rod starts the fight.
2. **Low**: four rods struck, it falls onto the centre island and swoops round it 3 to 8 over the
   grass. Now weapons tell; standing under it costs 3 a second and a push (a ring of wind). Every
   12 s it calls squalls.
3. **The fall**: at a third of its health it rises again, the rods are lit again and the bridges
   fall away in about 20 s. Everybody without a glider is handed one: two rods struck, reached by
   gliding from the centre (the updrafts between the islands help), finish it.

While it is fought **three owl men fly in** and circle the islands, striking the nearest squall
(a gold streak), or, low, pecking at the Manta.

When it falls everybody near gets the **storm glider** (`glider:storm_glider`, `mods/glider`): a
glider like the other, and every 1.5 s aloft a gust lifts it, so it sinks far less and goes much
further. A minute later the bridges and the rods are back. `/manta` says how many times you have
beaten it. Nobody within 220 of its islands for a minute: it all goes back to the start.

## How it was made

The Manta and the squall are from Meshy (`tools/meshy_prop.py`, then `tools/make_sky_creatures.py`:
three bones, wings beating, a dive, a death). They first flew tail first: the importer leaves an
object in quaternions, where a turn written to `rotation_euler` is not read; the script now turns
the vertices themselves. `tools/make_sky_sounds.py` makes the gust, the Manta's calls and the
squalls' crackle; the thunder is `mods/weather`'s.

The world is 64 high like every room: islands, clouds and the Manta are all under that.

## Limits

* Not played by hand: the gusts on a bridge, fighting it low, gliding in phase 3, the storm
  glider's lift, an owl man's panel and trades with real clicks.
* A plank taken from a falling bridge goes at once; it is not seen falling.
* `core.player_launch` takes at most 16 m/s an axis: an updraft lifts at 15.5.
