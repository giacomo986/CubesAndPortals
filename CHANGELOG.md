# Changes

*[Leggi in italiano](CHANGELOG.it.md)*

One entry for every alpha handed out, the newest at the top. How they are numbered and how an
alpha is made: [docs/BUILDING.md](docs/BUILDING.md#versions).

## Not released yet

- The Flatpak updates itself: installed once from the repository
  (`flatpak install --user https://giacomo986.github.io/CubesAndPortals/cubes-and-portals.flatpakref`),
  `flatpak update` brings every new release. The repository is signed and lives on GitHub Pages;
  the releases' `.flatpak` still installs, but one installed from a file is never updated.
- Alpha and beta releases come out marked as pre-releases.
- The documentation and this changelog are in English, each with its Italian twin beside it
  (`<name>.it.md`).

## 0.10.0-alpha.1 — 2026-10-31

Protocol 111 (110 the rails, 111 up to 24 mouths a room): it does not play with 0.9's servers,
nor their clients with its own. It reads 0.9.0-alpha.4's worlds; a new world can be sandbox or
survival.

- **Survival** mode (new world menu, `--survival`): no plaza, a random spawn on land, the themed
  worlds behind scattered portals with a difficulty of 1-10 growing with the distance, a level
  for every portal (parkour and backrooms generated; arena, Inferno, front and assault scaled), a
  hidden way out that opens when the level is done, a reward chest on the way back, keys only
  from difficulty 4, the laboratory somewhere at random. Rumours and directions (paid, from the
  mayor) about nearby portals. No creative catalogue and the bag in a grave on death. New APIs
  `core.seed`, `core.plaza`, `core.bag_to_container`, the `core:died` event; `/perm PERMISSION
  nobody`.
- The catalogue (C) asks the `creative` permission (moderator and up; in one's own world the
  owner).
- Carts on rails: curves, slopes up and down, rails powered by cinnabar (stations when off),
  switches. Rails are run both ways and curves look curved; the wheels clatter on the joints.
- Waystones: one a village beside the well, craftable for territories; paying an emerald every
  200 nodes opens a real portal to a stone touched, for five minutes.
- The creative panel shows the backpack, jetpack and armpack again: a carried container refused
  everything worn on the back, the catalogue's squares too.
- Paths between villages made again: a network (each village to its neighbours), soft curves, a
  surface by the land (sandstone, cobbles, gravel, beaten earth) with ragged edges, signposts at
  the ends, milestones with a lantern every 100 blocks. Worlds already explored keep their old
  paths.
- Caravans from villages not visited yet, with their land's goods, along the paths; the merchant
  tells the way aloud and marks it on the map. `/village incoming`.
- Village events: merchant and mule caravans between villages (bought from or escorted), rare
  night sieges with a warning and a reward, a harvest feast at the end of summer with stalls,
  music and fireworks (new `mods/fireworks`). Announcements with the bell and on the board;
  `/village` for admins. `farm:season` carries the days left too.
- The chess table also plays Italian draughts and the mill; the arena's gong starts three-minute
  PvP rounds with a score and a standing. New API `core.body_player`.
- Models in hand and icons for every new item and block that lacked them (chairs in the
  inventory are no longer squares of planks).
- Decoration: stained glass and panes in the sixteen colours, walls, glazed, chequered and stone
  tiles, chairs (you sit on them, and the body does not turn with the camera) and tables of every
  wood, carpets, iron and glass lanterns. You get off a cart with F too. `core.player_clip` takes a
  direction the body stays turned to.
- Railways remade in Blender: real sleepers and rails, arched curves, slopes on embankments,
  powered rails that light up, switches with a lever; a real mine cart; the player climbs in over
  the edge and rides seated; in first person the cart and the hands on its edges are seen, and the
  cart keeps to the rails' direction when the view turns. A player now also sees on themselves the
  pose and what they wear for a server's mod (the OwnPose message).
- Blueprints: copy a building, trace it as a ghost, have it built by hand, by the village's
  builder or by a robot. New APIs: `core.held_id`, `core.held_set_id`, `id` in `core.give_item`,
  `core.ghost_cell`, `core.ghost_clear`, `core.node_colour`.
- Village errands: six kinds, a board beside the well, paid by the treasury, a discount from the
  merchant after five.
- Breeding: dyed sheep (coloured wool), milk (cheese and cake; the bucket stays), troughs that
  make a pen, the robot's "Mind the animals" order. New APIs: `core.mob_kind`, `mob_love`,
  `mob_keep`, `mob_shear_now`, `mob_become`, `mob_voice`; `core:used_mob` on animals too.
- Fishing by water: perch, carp, trout, cod, eels, tuna and a rare goldfish; all are cooked and
  the frog fisher buys them.
- Alchemy: a still, flasks and five potions (night sight, swiftness, stoneskin, mending, haste,
  water breathing). You drown: fifteen seconds of air under water, then health is lost. New APIs:
  `core.night_sight`, and a source for `core.player_factors`, so different mods' effects add up.
- The Hollows under the bedrock: glowing caverns, lava, crystals, lurkers and a guardian. The
  world now holds blocks down to y -64.
- A chunk under the player arrives only after the one above it: no more caverns seen from above
  through ground not arrived yet.
- Rare structures with a riddle: desert temples, wrecks, sunken ruins.
- The compass that points home and the map that fills as you explore.
- Portals no longer have a maximum a world: the server keeps active the 32 nearest the players.
  Portal frames no longer stop at 16.
- The boat rocks on the waves. Boat and cart have an icon.
- Stone bricks are made with smooth stone: the old recipe was the bricks'.
- The sandbox has weather: rain, snow on the tundra and the peaks, storms with lightning and
  thunder. Rain waters the fields.
- Seasons: leaves that change colour, fields still in winter without lamps, snow everywhere in
  winter. Merchants double food in winter and wood in autumn.
- Boats: five planks, right click on water to get in, on a bank to get out. They go at twice
  swimming speed, steer with A and D, dodge creatures, bounce off each other and the oars are heard.

## 0.9.0-alpha.4 — 2026-10-27

Protocol 109, like 0.9.0-alpha.2: it plays with its servers and reads its worlds. 0.9.0-alpha.3
did not come out: the macOS package would not build.

- Whoever keeps a server has a full administration: roles, bans, kick, mute, an access list, a
  console, RCON, scheduled restarts, maintenance, a message of the day, rules to accept, block
  rollback, backups and webhooks. The manual is [docs/ADMIN.md](docs/ADMIN.md).
- Friend servers share bans, presence, chat and the backpack: whoever goes through a portal to a
  friend carries the same backpack.
- The Multiplayer list shows each server's players, version and message of the day.
- The title says when a newer alpha is on GitHub.
- The README is in English too.

## 0.9.0-alpha.2 — 2026-10-26

Protocol 109, like 0.9.0-alpha.1: it plays with its servers and reads its worlds.

- The game speaks Italian and English: Options, Language picks the language (at first the
  system's). A new language is a file in `assets/lang/`.
- In Multiplayer the account is the server's: you pick the server, then a name and password for it.
- In the assault's escape the bots aboard shoot at the traps at the right moment.
- The spawn's plaza is quiet: no more blinking lamp nor beacons lighting in turn.
- The packages no longer hold third-party models, and are built from a clone of the repository;
  the code is public, under the MIT licence, and the models' Blender sources are in
  `assets/blender/`.

## 0.9.0-alpha.1 — 2026-10-25

Protocol 109: it does not play with 0.8.0-alpha.1's servers. It reads its worlds.

- Online, in worlds with many soldiers (the assault, the front), soldiers and players no longer
  jump from one place to another: bodies travel in small packets, so a lost packet stops a few for
  a moment; whoever leaves the view disappears at once instead of standing still; up to 96 are seen.
- The server writes in its log when it does not keep up, and why.

## 0.8.0-alpha.1 — 2026-10-25

Protocol 108: it does not play with 0.7.0-alpha.5's servers. It reads its worlds.

- A new world, **the assault**, played together against Basoalth (from the sandbox's plaza, at
  90.5 21 76.5): four fortresses to take one after another, credits and a shop, allies coming out
  of the barracks, builders who repair, put ladders on the walls and nests of sandbags, bots that
  blow up the fortress's gate; the core's hall with its pylons; Basoalth growing into a giant, the
  floor collapsing and the escape by jeep through a lava tunnel with turns, rises and falls,
  valves, boulders and tanks to hit to slow him down.
- The assault's valley: a red sky with clouds in turning vortices, tornadoes that suck in and
  throw away, a chain of volcanoes with lava flows, a lava river with a bridge and islets.
- Whoever dies comes back with every weapon and full ammunition.
- In machines: steps have longer ramps and are climbed without stopping (in every world); as a
  passenger you shoot with your own weapon and 1-9 picks it; seated you sit with the weapon, at a
  turret with your hands on the weapon, turned where you look.
- Machines and objectives no longer disappear when many soldiers are about.
- Behind glass you see what is there (before, bodies passed in front of it); the clouds are no
  longer dotted.
- Karts: drifting with Ctrl, tyre marks on the road, the nitro's flames, and a garage to make your
  own kart.
- The ruins: a town, three zones, classes and items with properties; barrels, crates and urns to
  break; spells and blows with new effects.
- Parkour: the rope goes where the crosshair is, in third person too.
- Bag and creative with smaller squares and the bar below; a catalogue chest.
- The illustrated deck of cards has new cards of coins, cups and swords.
- Footsteps are quieter.

## 0.7.0-alpha.5 — 2026-10-18

Protocol 100: it does not play with 0.7.0-alpha.4's servers. It reads its worlds.

- You enter the front from a gallery on the ridge, halfway between the bases and high over the
  field, with a window to watch the battle from. Two portals, a red one and a blue one, lead into
  the base's bunker: the one you cross picks the team. From the bunker you go back to the gallery.
- The bots' battle starts even if there is only somebody watching from the gallery.
- Machines are solid: two machines collide instead of passing through each other, whoever is on
  foot does not get into the hull, climbs on it and is carried.
- Whoever drives sees the machine move at once, without waiting for the server: the client
  simulates it and lines it up with the server's.
- Soldiers at fixed weapons (anti-aircraft gun, machine gun, mortar) look where the weapon points.
- Fixed a server crash when two machines were born at the same instant.
- The illustrated deck of cards has the 2, the 3 and the 4 of coins too.

## 0.7.0-alpha.4 — 2026-10-18

Protocol 99, like 0.7.0-alpha.3: it plays with its servers and reads its worlds.

- At the front you aim: with the right button the weapon comes up to the middle of the view, the
  shots go where you look and you walk slowly. A new sniper rifle with a scope. From the hip the
  shots spread. Weapons are seen held, pointed, fired and reloaded from outside too.
- In third person the shots go where the crosshair is, even with a soldier or a machine in front;
  going from first to third person and back the crosshair stays on the same point.
- The bot soldiers are more within reach: they see only ahead, wait a moment before firing, fire
  less accurate bursts and do less damage. They fight in four ways (the sentry with the sniper
  rifle, the assaulter with the machine gun, the careful one who takes cover, the anti-tank one
  with the rocket launcher), keep their weapon while it has rounds and it is seen in their hands.
- Bots use the machines: they drive jeeps and tanks, fly the helicopter, the plane and the
  bomber, take nests, mortars, howitzers and anti-aircraft guns, and get into machines their
  comrades drive, players too.
- New machines: the bomber (a row of six bombs, with the point of impact marked, and a turret on
  its back) and the anti-aircraft gun, which locks on to helicopters and planes and launches
  missiles that chase them.
- In machines: F puts you in the nearest free seat (from behind the jeep you get in at the back)
  and 1-9 change seat without changing the weapon in hand. The machines' windows are transparent.
- Glass panes are transparent.
- The front will be the prize of whoever beats the Host in the laboratory; for now it stays open to
  everybody.

## 0.7.0-alpha.3 — 2026-10-18

Protocol 99: it does not play with 0.7.0-alpha.2's servers.

- A new world, the front: a battlefield between a red base and a blue one, with three points to
  capture (farm, village, bunker on the hill) and 150 tickets a team. You enter by the bunker door
  between the sandbags on the plaza. Bot soldiers fill the teams up to 8.
- At the front: automatic rifle and machine gun, pistol, grenades and rocket launcher. Explosions
  dig the ground and bring down what is left hanging.
- Machines: four-seat jeeps with a machine gun, tanks that climb steps and break through walls,
  helicopters with rockets, planes with machine guns and bombs, machine-gun nests, mortars and
  howitzers that fire where the crosshair points. You get in and out with F, 1-9 change seat, the
  mouse aims and steers what flies. A destroyed machine burns, sheds its parts and sinks.
- Seated in a machine the body stays fixed to it; at the jeep's wheel the hands are on the wheel,
  and in first person only the arms are seen.
- Cards: cards played, taken, drawn and dealt fly onto the table one at a time; the cursor on a
  played card lifts it and says who played it.
- Fixed: after getting out of a machine or dying in it, the server could stop accepting movement
  (grenades that did not leave, shots that did not arrive).

## 0.7.0-alpha.2 — 2026-10-17

Protocol 97: it does not play with 0.7.0-alpha.1's servers.

- Multiplayer: the other players and the creatures move smoothly, no longer in jerks, sliding from
  one position received to the next.
- Multiplayer: whoever flies is seen flying (no longer walking when flying low).
- Flying: the body leans in the direction it flies and glides when let go.
- Track: icons for the jetpack and the timer's tools.
- Cards: the suit of clubs, the back and the ace of coins illustrated.

## 0.7.0-alpha.1 — 2026-10-17

Protocol 96: it does not play with 0.6.0-alpha.2's servers.

- Character: besides the robot, a human, a mole man and a frog man, each with three styles (short
  hair, long hair, beard; miner's lamp, goggles, glasses; spots, crown, water lily), the three
  colours and the accessories from before. The robot's skeleton and clips, arms of their own in
  first person. The accessories are remade to fit each species (the frog wears the cap
  backwards); an accessory on the head takes the place of the lamp, crown or lily.
- Karts: backpack, armour and the thing in hand are on the driver, no longer on the kart (the
  jetpack ended up on the head and in first person covered the view).
- Karts: you get out with F, no longer with the right button, which made you get out by mistake.
- Characters sculpted in Blender: the human, mole and frog are continuous smooth surfaces (no
  longer glued primitives) that bend at elbows and knees; the hair covers the nape. Medal and the
  scarf's ends swing only forward, without going into the chest.
- Tools: pickaxe, axe, shovel, sword and hoe in wood, stone, iron, gold and diamond, each with its
  own shape; they wear with use.
- Armpack: a metal backpack with two jointed arms, a tool in each claw; it digs with the best of
  hand and arms and adds up the swords.
- Villages: every worker takes the best tool of their trade from the chests, the crafter makes
  iron tools, and iron and diamond ones are bought.
- Track editor: B (or the "pick from afar" button) picks from afar; instead of the hotbar a grid
  with every piece, click to try one, W to keep it. A/D no longer jam when removing banked curves
  and walls, which used to stay placed.
- Track pieces: they are placed in the ground too, which is taken away (and 3 blocks over the
  road); only another piece stops them.
- Jetpack: the sound is much lower and rises a little while moving.
- ARPG: the arrival room is safe, creatures stay out; W A S D interrupt walking by click.

## 0.6.0-alpha.2 — 2026-10-14

Plays with 0.6.0-alpha.1's servers (protocol 93); saved worlds are read.

- Track: power-up boxes after every finish line, as in kart games: boost, triple boost, banana,
  shell, homing shell, ink, star, lightning. Two are kept.
- Track editor (in /pezzi): right click on a piece offers those that fit the open end, Undo removes
  the last one, Close circuit closes the circuit by itself, level first. Pieces in hand and on the
  ground are seen in miniature.
- Track: the boxes' tool, the creative panel and the jetpack to fly over it.
- Track: loops twice as tall; the kart no longer trips coming out of loops.
- Light against the dark: F8 shows where creatures come out, robots light them up, an anchor keeps
  them at work.
- Helpers: a quarry with its staircase to get out, and Close on the program's panel.
- Portals: between rooms of the same server no ticket, and an expired ticket is renewed instead of
  refused.

## 0.6.0-alpha.1 — 2026-10-12

Does not play with 0.5's servers (protocol 93); saved worlds are still read.

- The robot is customised: Robot, in the start menu or in pause, picks a primary, secondary and
  accent colour (16 colours) and an accessory on the head (cap, top hat, headphones) and one at the
  neck (scarf, bow tie, medal), with the robot turning beside it. The other players see it, also
  when it is changed mid-game.
- New light: true darkness, soft light, lamps lighting the faces turned to them, sun and moon with
  shadows. The light from before stays in the options (Light: classic), and shadows can be lowered
  or turned off.
- Full screen: F11 or in the options.
- Cards: at the table the robot sits and holds the cards in its hand.

## 0.5.0-alpha.1 — 2026-10-11

Does not play with 0.4's servers (protocol 91); saved worlds are still read, and the zones already
generated keep the ground they had.

- World: oceans, seas, beaches, rivers, high lakes and gorges, with bridges where the paths cross
  them; underground abandoned mines and crypts.
- The worlds' ways out are sealed in red until that world is won; the arches on the plaza lead to
  each world's way out.
- The Inferno loads the sandbox's game mods: what you carry in the bag counts there too; hunger,
  experience and keys are the same in every room.
- Track: a new track with three loops, the camera following the kart in the loops, and no green
  flash crossing the Inferno.
- The Inferno track: an extra obsidian frame removed from the exit portal; getting off and back on
  the kart in the Inferno you come back to the track drawn right, and the chosen view stays.
- Chess: eight variants. Parkour: always running, and you grab ledges.
- A new world starts in about 25 seconds (it was 80), with a bar on the title.

## 0.4.0-alpha.1 — 2026-10-10

The first alpha handed out.

- Packages: Linux (tarball, AppImage, Flatpak), Windows (zip), macOS (`.dmg`, never tried on a Mac).
- The version is written in the bottom right corner of the menu: give it when reporting a bug.
- On Windows: no stack dump on a crash, and going back to the menu the password has to be typed
  again.
