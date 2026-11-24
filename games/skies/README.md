# The sky islands

*[Leggi in italiano](README.it.md) · [Leia em português](README.pt.md)*

Six islands over a sea of cloud, bridges between them, and the Cloud Manta: a boss for one player
or several, and the storm glider for whoever is there when it falls, theirs to carry out. One mod,
`mods/skies` (`layout.luau` the islands and the bridges, `init.luau` the ground and the fight).
The sandbox's mods are linked in (robot, tools, archery, armour, the glider...), so your bag and
weapons come with you.

## How to get there

Its gate is on the plaza's south edge, at 69.5 21 26.5: a white arch like a cloud with planks
before it (`mods/gates`). Like the platform game it is in the rooms file: it starts when somebody
comes near the gate and stops a minute after the last one left. You come out of a doorway on the
home island, looking north over the others.

## The islands

| island | where (x y z) | what |
|---|---|---|
| home | 0 40 -92 | the doorway, a glider rack |
| south | 0 40 -46 | a lightning rod |
| centre | 0 40 0 | the widest (13), a glider rack; where the Manta is fought |
| north, east, west | 0 42 46, 46 38 0, -46 44 0 | a lightning rod each |

Bridges of planks, three wide, with log posts every four: home to south, and the centre to each of
the four; the north and west ones have a gap of two to jump. Below, at 14, the clouds; a body that
falls past 20 is put back where it last stood (on the nearest island when that bridge is gone) and
loses 4. The racks give a glider (`mods/glider`) to whoever has none.

## The Cloud Manta

12 m from wing tip to wing tip; 400 health, and 60% more for each player more.

1. **High**: it circles at 58 round the islands, out of reach. Every 6 s it blows a gust at
   somebody on a bridge (said a moment before): thrown 12 m/s away from it. A right click on a
   lightning rod calls a bolt into it (12% of its health) and two squalls (one more for each
   player); it comes down 3 with every rod. The first rod starts the fight.
2. **Low**: four rods struck, it falls onto the centre island and swoops round it 3 to 8 over the
   grass. Now it can be struck with weapons; standing under it costs 3 a second and a push. Every
   12 s it calls squalls (one, and one more for every two players).
3. **The fall**: at a third of its health it rises again, the rods are lit again and the bridges
   fall away in about 20 s. Everybody without a glider is handed one: two rods struck, reached by
   gliding from the centre, finish it (17% each).

When it falls everybody in the world gets the **storm glider** (`glider:storm_glider`): a glider
like the other, and every 1.5 s aloft a gust lifts it (`core.player_launch`, 6 m/s up), so it sinks
far less and goes much further. It works in every world that has `mods/glider`. A minute later the
bridges and the rods are back and the Manta circles again. `/manta` says how many times you have
beaten it. Nobody here for 30 s: it all goes back to the start.

## How it was made

The Manta and the squall are from Meshy (a concept picture, image to 3D with meshy-6,
`tools/meshy_prop.py`); `tools/make_sky_creatures.py` gives each three bones (body and two wings,
every vertex weighted to a wing by how far it is from the middle) and the clips: idle, walk and run
(the wings beating), attack and shoot (a dive), death (the wings fold and it rolls over).
`tools/make_sky_sounds.py` makes the gust, the Manta's calls and the squalls' crackle; the thunder
is `mods/weather`'s.

The Manta is a worker that hovers: the mod puts it where it should be every tick
(`core.mob_place`), and blows that land on it outside the low phase are given back.

The world is 64 high like every room: the islands, the clouds and the Manta are all under that.

## Limits

* Not played by hand: the gusts, the gaps, the gliding in phase 3 and the storm glider's lift were
  not tried with real keys (the lift is server pushes on a body the client moves).
* A plank taken from a falling bridge goes at once; it is not seen falling.
