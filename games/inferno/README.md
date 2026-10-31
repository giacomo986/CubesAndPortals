# The Inferno

*[Leggi in italiano](README.it.md)*

A world of caverns between a floor and a roof of bedrock, seas of lava at the bottom, glowrock
hanging from the roof, and fortresses of brick whose lanes bridge the caverns. No sun: a red sky,
and the only light is the lava's and the glowrock's. The room `inferno` of `tools/track.sh`,
reached from the plaza's obsidian frame or from one a player builds.

Its own mods: `inferno` (the ground, the lava, the creatures, everything below),
`inferno_heart` (the trial for the Key of Fire), `inferno_track` (the kart road),
`inferno_ash` (`side = client`: ash and embers in the air) and `infernoportal` (shared with the
sandbox: the frames). The sandbox's gameplay mods are linked in too, so what a player carries is
worth something here.

## The ground

| country | where | what |
|---|---|---|
| scoria | everywhere else | caverns of scoria, quartz ore one cell in 90, ash sand on the shores |
| basalt fields | a noise over x and z | basalt instead of scoria, columns from the floor up to the roof (up to 30) |
| fungus forests | the same noise, the other end | stems 4 to 7 high under glowing caps five across |
| ash wastes | a second noise | ash sand over every floor, glowrock half as often |

Lava under y 12; it flows (a source and four levels) and burns 3 a half second. A fall of lava out
of a wall now and then. Fortresses: every 96 nodes a lane of brick five wide along x or z, two
stretches of 64 in three built, a chest every 64 along against the rail.

## What it gives

| from | gives | for |
|---|---|---|
| brute | gold nugget | nine an ingot (`furnace:gold_ingot`, its tools) |
| wraith | wraith tear | the still's fire resistance (`mods/alchemy`): 3 minutes, lava and fire do not burn |
| cinder | cinder rod | fuel; ground into 2 cinder powder: with an iron ingot a lighter, with a gold ingot and glass 2 ember lanterns (light 15) |
| quartz ore | quartz | four a quartz block; the middle of a comparator (`mods/cinnabar`) |
| fungus stem | 4 fungus planks | two planks, 4 sticks |
| a fortress chest | gold, rods, quartz, tears, obsidian, iron, sometimes gold ingots, a fire resistance, a diamond | filled the first time it is opened; one a player put down stays theirs |

Brewing needs a flask of water, and the Inferno has none: bring it from above.

## Fire

A creature's fireball that bursts on a block (engine event `core:burst`) sets a fire in the cell
in front of the face it struck: 3 s on rock, 8 on wood, fungus and the like; it never spreads,
and it burns 1 a half second whoever stands in it. Fire resistance keeps it off.

## The way in, and the way back (1:8)

A node here is eight in the world above. A frame of obsidian lit with the lighter in the sandbox
moves the plaza's way there, and the Inferno builds the twin at an eighth of the distance, at the
height where it breaks least. A frame lit here (the Inferno's own obsidian counts) moves the way
back into it, and the sandbox builds the twin eight times as far out, on the ground. A lit frame
broken: its side goes home (the plaza, or the arrival chamber at 32 30 32) and the other follows.
Each side ignores the far opening coming back to where it already maps.

## Sounds and air

Round each player every 6 to 14 s a rumble, lava bubbling below or a moan far off; a fire
catching (`tools/make_inferno_sounds.py`). On each player's machine 160 flakes of ash drift down
round the eye and one in nine is an ember going up.

## Files

- `tools/make_inferno_textures.py`: the blocks' pictures (`games/inferno/textures`)
- `tools/make_inferno_sounds.py`: the four sounds
- `tools/make_held_models.py` (`ONLY=inferno:quartz,...`): the items in the hand and their icons

## Tried (2026-10-31)

Headless and in Xephyr: the three countries seen; a fortress chest at 104 30 53 facing the lane,
filled with 9 nuggets, 2 rods, 6 quartz, obsidian and iron; a fire on a deck; 4 s in lava at 20
of 20 under fire resistance; a fireball into a wall told once with its face (a unit probe of the
engine); a frame lit at 36 30 32 giving a twin at 292 27 260, and both home after it was broken.
Not seen: a wraith's fireball setting a fire in play, a frame lit in the sandbox since the change,
the sounds heard.
