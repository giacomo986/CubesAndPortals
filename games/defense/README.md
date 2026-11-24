# The tower defense

*[Leggi in italiano](README.it.md) · [Leia em português](README.pt.md)*

Monsters from the other worlds come through a rift and walk to a crystal; you stop them with
turrets made of what you mined, barricades that lengthen their way, and your own weapons. Alone or
together. Two mods: `mods/defense_core` (the field, the rift, the crystal, the benches, the doorway
home) and `mods/defense_game` (the waves, the turrets, the barricades, the records). The sandbox's
mods are linked in (robot, tools, ores, archery, armour, the glider...), so your bag, weapons and
armour come with you, and what the waves give goes home with you.

## How to get there

Its gate is on the plaza's south edge, at 25.5 21 26.5: a stretch of stone wall with a crystal's
light over the hole (`mods/gates`). Like the platform game and the arcade shooter it is in the
rooms file: it starts when somebody comes near the gate and stops a minute after the last one
left. You come out of a doorway in the field's south wall, by the crystal, looking up the field at
the rift.

## The field

49 wide and 73 long, walled with stone bricks five high. At the north end the rift, a frame of
black stone with violet light in it, where the monsters come out; at the south end the crystal on
its pedestal, two benches either side of the doorway. Day, always: the clock stands at noon.

## How it plays

* **Start**: right click the crystal. Everybody here gets 24 barricades (and the first time ever
  planks, iron and cobblestone, enough for an arrow turret), and the first wave comes in 30 s (a right
  click on the crystal sends it at once).
* **Waves**: 5 + 2 a wave monsters, half as many again for each player more, each 12% tougher than
  the last wave's, and 35% for each player more. Zombies and demons; from wave 4 cyborgs too. Every
  fifth wave a boss: the arena's Guardian, then the crypt's Butcher, in turn. A monster at the
  crystal takes 1 (a cyborg 2, a boss 10) of its 20; at 0 the game is over and the field is
  cleared. A monster with a player within 3.5 fights them for up to 10 s, and goes back to its way when
  they are 8 off; near the crystal it fights nobody and makes for it.
* **Between waves**: 25 s, or a right click on the crystal. Every player here gets iron, planks,
  cobblestone and 8 barricades; from wave 2 ice, from wave 3 cinder rods, from wave 4 gold and
  (every other wave) rubies, every fifth diamonds. They are yours to keep.
* **Barricades**: put down on the field's grass between the crystal and the rift, two high; the
  monsters walk round them. One that would shut the way is taken back into the bag. Broken, it
  gives itself back.
* **Turrets**: right click a bench, pick one; it goes into the bag, to be put down on the field like
  a block. A right click on one shows its level and upgrades it (up to 3). Broken, it comes back at
  level 1.

| turret | costs | upgrade (to 2, to 3) | what it does (levels 1/2/3) |
|---|---|---|---|
| arrow | 4 planks, 2 iron, 4 cobblestone | 2 iron, 4 iron | the monster nearest the crystal within 10/11/12: 5/7/10 every 0.8/0.65/0.5 s |
| fire brazier | 6 cobblestone, 2 cinder rods, 2 iron | 2 cinder rods, 3 | every monster within 3.5/4/4.5: 3/5/7 a second |
| frost | 4 ice, 2 iron | 3 ice, 5 | slows every monster within 6/7/8 to 55/45/35% of its pace |
| lightning | 2 gold, 1 ruby, 2 iron | 1 ruby, 2 | 9/12/16 within 9/10/11, and on to 3/4/5 more within 4 of each, three quarters each time, every 1.6/1.4/1.2 s |
| portal | 1 diamond, 1 gold, 4 cobblestone | 1 diamond, 2 | sends the monster nearest the crystal within 5/6/7 back to the rift, every 10/8/6 s; never a boss |

* **Records**: the best wave of every player, and the field's with who held it. `/defense` or a
  right click on the crystal during a game shows them.
* Nobody here for 20 s: the game is over and the field cleared.

## What a player's machine draws

`defense_game` runs on both sides (`side = both`). The block under a turret is seen through
(`core.node_glass`): it is what stops bodies and takes the clicks. Each player's machine draws the
turret's model on it (`core.render.model`), a little bigger at each level, turned towards its last
target and easing round, kicked back as it fires; the arrow and lightning turrets look slowly round
when nothing has come for a while. Each has a light of its own: the brazier's flame, the frost
crystal's glow, the lightning turret's red sphere crackling, the portal's swirl. What they fire is
seen on its way (`core.render.fx`): an arrow's streak and sparks where it strikes, a ring of fire
and flames on every monster burnt, frost spreading on the ground, lightning leaping from monster to
monster, a swirl where a monster is taken and another at the rift where it comes out. The server
says each turret (`T`), each one gone (`R`), the field cleared (`C`) and each shot (`F`, the
turret and the points struck).

## How it was made

The turrets are from Meshy: a concept picture each (text to image), image to 3D (meshy-6), then
`tools/meshy_prop.py --height 1.4`. Over every turret an invisible solid cell, so no monster steps
up on one and over a barricade. The monsters are the assault's (zombie, demon, cyborg), the arena's
Guardian and the crypt's Butcher, copied in with their sounds (a mod whose code goes to players may hold no link out of itself);
the turrets' sounds are the arcade
shooter's.

A monster sent the whole length of the field at once (`core.mob_goto`) runs out of search on a
field of barricades and stops halfway; so the way is worked out once (`core.find_path`, whenever a
barricade or a turret goes down or up) as stops every six cells, and each monster walks from stop
to stop.

## Limits

* Turrets and barricades go with the game: a game over clears the field.
* The sandbox's crafting table (`mods/craft`) stands on the field at 39 20 47, sunk in the grass.
