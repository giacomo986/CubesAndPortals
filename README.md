# Cubes and Portals

*[Leggi in italiano](README.it.md)*

[![A video of the game](https://img.youtube.com/vi/WdtsBnpiZpA/hqdefault.jpg)](https://www.youtube.com/watch?v=WdtsBnpiZpA)

**[A video of the game](https://www.youtube.com/watch?v=WdtsBnpiZpA)**

**Contents**

- [Status](#status)
- [Platforms](#platforms)
- [Download](#download)
- [Building from source](#building-from-source)
- [Contributing](#contributing)
- [Use of AI](#use-of-ai)
- [The game](#the-game)
  - [The plaza and its worlds](#the-plaza-and-its-worlds)
  - [The sandbox](#the-sandbox)
  - [The engine](#the-engine)
- [Playing and testing: reference](#playing-and-testing-reference)
  - [Controls](#controls)
  - [The title menu](#the-title-menu)
  - [Client (cnp_client)](#client-cnp_client)
  - [Server (cnp_server)](#server-cnp_server)
  - [Server administration](#server-administration)
  - [Tools](#tools)
  - [Tests](#tests)
- [Technology](#technology)
- [Documentation](#documentation)
- [License](#license)

A voxel sandbox written from scratch, where **portals are part of the engine**: a portal shows
another world, even one running on another server, and you walk through it without a loading
screen. From the spawn plaza, portals lead to worlds that are whole other games: a kart track, an
arena shooter, an isometric action RPG, portal puzzle chambers, the backrooms, a parkour course,
and a co-op assault. You can play alone or with others, and every world is made of Luau mods that
anyone can read and change.

This is a **collaborative project, open to contributions**: code, worlds, mods, texture packs,
translations, bug reports and playtesting are all welcome (see [Contributing](#contributing)).

Most of the in-depth documentation (`docs/`, the worlds' guides, the changelog) is written in
Italian; code, comments, `TODO.md` and commit messages are in English. Issues and pull requests
are fine in either language.

## Status

**Playable alpha.** There is a sandbox (an endless world with biomes, caves, villages with their
inhabitants and economy, animals and monsters, circuits, chess and cards), the seven worlds behind
the plaza's portals, the final laboratory, two more worlds after it, and multiplayer. Plenty is
still unfinished or rough: what is missing or broken is in [`TODO.md`](TODO.md), and what changed
in each release is in [`CHANGELOG.md`](CHANGELOG.md).

While the game is in alpha, a new version may not read the worlds saved by the previous one, or
play with its servers. The changelog says so whenever it happens.

## Platforms

| System | Packages | State |
|---|---|---|
| **Linux** x86_64 | `.tar.gz`, `.AppImage`, `.flatpak` | The main platform: the game is developed and tested here. The tarball needs glibc 2.38 or newer (Ubuntu 24.04, Fedora 39 and later) |
| **Windows** 10 or newer, x86_64 | `.zip` | Works, but has mostly been tested under wine. Known gaps: no stack dump on a crash, and a client that crashes takes its local server down without saving |
| **macOS** 11 or newer (Apple silicon) | `.dmg` | The package builds, but **has never been tried on a Mac** |

The client needs a graphics card with **OpenGL 3.3**; on Linux it runs on X11, or on Wayland
through XWayland. The server (`cnp_server`) has no window and runs anywhere the rest does.
Reports from real Windows and macOS machines are especially welcome.

## Download

Ready-made packages are on the **[Releases](../../releases)** page. For every version (a `v*`
tag) GitHub Actions builds them for all three systems and publishes them as a pre-release.

- **Linux**: unpack the `.tar.gz` anywhere and run `./cnp_client`; or make the `.AppImage`
  executable (`chmod +x`) and run it; or install the `.flatpak` with
  `flatpak install --user <file>.flatpak`, after which it shows up in the desktop menu.
- **Windows**: unpack the `.zip` and double-click `cnp_client.exe`.
- **macOS**: open the `.dmg` and drag the app wherever you like.

The packages are not signed. On the first run, Windows SmartScreen asks for confirmation ("More
info", then "Run anyway"); on macOS, right-click the app and choose Open.

**Updates.** The game checks for a new version by itself: when the title menu opens, it asks
GitHub for the latest release of this repository (that request is all it sends, nothing about
the player). If there is a newer one, a "New version …: download" button appears and opens the
Releases page. Turn it off in Options, Updates, or for a single run with `--no-update-check`.

Saved worlds and options live in `~/.local/share/cubes-and-portals` and
`~/.config/cubes-and-portals` on Linux, in `~/.var/app/io.github.giacomoperin.CubesAndPortals`
with the Flatpak, and in `%APPDATA%\cubes-and-portals` on Windows.

## Building from source

Every library the game uses (raylib, Luau, ENet, zstd, SQLite, Monocypher, flecs, Jolt) is in
`third_party/`, as a git submodule, and is built together with the game. You need **CMake 3.25+**
and a C++20 compiler (**GCC 13** or Clang). On Linux (Ubuntu/Debian):

```sh
sudo apt install build-essential cmake ninja-build git \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libasound2-dev

git clone --recursive https://github.com/giacomo986/CubesAndPortals.git
cd CubesAndPortals
cmake -S . -B build -G Ninja
cmake --build build -j
./build/src/client/cnp_client
```

A clone made without `--recursive` can be completed with
`git submodule update --init --recursive`.

The build makes three programs:

| | |
|---|---|
| `build/src/client/cnp_client` | the game: menus, saved worlds, multiplayer |
| `build/src/server/cnp_server` | a world for other players, without a window |
| `build/tools/cnp_modpack` | packs a mod into a single `.cnpmod` file |

A development build reads `mods/`, `games/` and `assets/` from the checkout, so run it from
there. The tests run with `ctest --test-dir build` (13 tests, a few seconds).

The players' packages are made by the scripts in `tools/dist/` and land in
`<build directory>/dist/`. The Windows package is cross-compiled **from Linux** with zig; the
macOS one has to be built **on a Mac**. All the details (CMake options, packages, versioning)
are in [`docs/BUILDING.md`](docs/BUILDING.md).

## Contributing

The project is open to everyone, and there are many ways to help:

- **play and report** bugs, crashes and anything confusing, by opening an issue: what happened,
  what you expected, your system and the game's version (bottom right of the title menu), and the
  log if you can;
- **translate**: a new language is a new file in `assets/lang/` (see `it.txt`);
- **make mods, worlds and texture packs**: see [`docs/MODDING.md`](docs/MODDING.md),
  [`docs/API.md`](docs/API.md) and [`docs/TEXTURE_PACKS.md`](docs/TEXTURE_PACKS.md); the Blender
  sources of the models are in [`assets/blender/`](assets/blender/README.md);
- **work on the engine**: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) explains how the code is
  laid out and where a new feature goes; [`TODO.md`](TODO.md) lists what is missing.

How to propose a change:

1. for a large change (a new world, a change to the network protocol or the save format), open
   an issue first and talk it over;
2. a pull request does **one thing**, and its description says what changes, why, and how you
   tested it (in game and/or with `ctest`);
3. the code builds without new warnings and the tests pass;
4. new code looks like the code around it (names, comments, style), and the documentation of
   the part you touched is updated with it;
5. textures, models and sounds are your own or under a compatible licence, stated in a
   `CREDITS.md` in the mod's folder (like [`games/track/CREDITS.md`](games/track/CREDITS.md)).
   No names, trademarks or material from commercial games.

**Every contribution is accepted, as long as it is neither malicious nor messy.**

- *Malicious*: malware, backdoors, collecting players' data, code that hides what it does,
  material copied without the right to it, offensive content. Such a pull request is closed, and
  its author does not open others.
- *Messy*: huge pull requests that mix unrelated things, mass reformatting, dead code, changes
  without a description, a broken build, generated code nobody has read. Such a pull request is
  not rejected on principle: you will be asked to split it or tidy it up.

By contributing you agree that your contribution is distributed under the project's licence
(MIT).

## Use of AI

This project is developed with **extensive use of AI**, and says so openly:

- **Code, documentation and commit messages** are largely written by Claude (Anthropic), through
  Claude Code, under the direction of the project's maintainer, who decides what to build,
  plays the game and approves every change. Commits written this way say so with a
  `Co-Authored-By: Claude` line.
- **3D models, textures and nearly all the sounds** are generated by Python (and Blender) scripts
  in `tools/`, themselves written with AI: no assets are taken from other games.
- **The card deck's illustrations** and a few sounds are generated with ElevenLabs.

Contributions made with the help of AI are welcome on the same terms as any other: whoever opens
the pull request answers for it, has read, understood and tested it, and says so in the
description. A model's output pasted in without checking counts as messy.

## The game

### The plaza and its worlds

You start on a plaza. Around it, seven portals, each inside a structure that looks like the world
behind it (`mods/gates`), lead to seven worlds, all rooms of the same server (`tools/track.sh`,
or any world started from the title menu):

- **The track** (`games/track`): rigid-body karts (Jolt) with suspension and nitro, driven by the
  server from the player's keys. Five timed tracks with leaderboards, one through the real
  Inferno and one with five loops. Track pieces like the stunt racers' (low, high and wide loops,
  wide-to-narrow adapters), and an editor to build new tracks: it offers the pieces that fit the
  open end, undoes, and closes the circuit by itself. Item boxes along the road (boost, bananas,
  green and red shells, star, lightning, ink), drifting with tyre marks, flames from the
  exhausts, and a garage (`/garage`) to choose colours and 14 parts of your kart.
- **The Inferno** (`games/inferno`): scoria caves, flowing lava, fortresses, brutes, wraiths and
  cinders. You can also get there through an obsidian portal you build yourself.
- **The base** (`games/yafps`): a modern arena shooter with five weapons and waves of demons.
- **The wastes** (`games/arpg`): a point-and-click isometric action RPG. A town with merchants and
  a stash, three zones of areas and corridors, waypoints and a town portal, three classes with
  four skills each, levels, and normal, magic, rare, unique and set items that show on the
  character, with their animations.
- **The test chambers** (`games/chambers`): six portal puzzle chambers, a portal gun that grows
  with you, cubes, buttons and launch pads.
- **The backrooms** (`games/backrooms`): three endless levels with stairs, pits, pools, slides
  and two creatures; rare crates hold the way back. Guide in
  [`games/backrooms/README.md`](games/backrooms/README.md).
- **The parkour course** (`games/parkour`): wall runs, bounces, slides, rolls and a rope gun.
  Guide in [`games/parkour/README.md`](games/parkour/README.md).

**The seven keys** (`mods/keys`, the game's ending). The engineer who built the portals locked
the Host in the laboratory and split the lock into seven keys, one in each world, behind a
trial. The eighth portal on the plaza, a dark steel frame with seven pedestals, leads to the
**laboratory** (`games/lab`) and turns away anyone who has not held all seven keys. Each world is
left through an exit at the end of its trial, and each key waits on a golden pedestal beside it.
In the laboratory, the Host is beaten with a little of every world: portal gun, rifle, a parkour
climb, valves under the lava, a torch in the dark, frost, and finally a kart escape through a
tunnel and the credits. After that the sandbox stays free, and two more worlds open:

- **The front** (`games/front`): a battlefield in the style of the big team shooters, two bases
  and three points to take, red against blue, with bots and vehicles.
- **The assault** (`games/assault`): the final co-op assault on Basoalth's dimension: four
  fortresses, a shop, allies and builders, a boss who grows into a giant, and an escape by jeep
  through a lava tunnel.

### The sandbox

**The land.** An endless world: plains, deserts, tundra and mountains a few hundred blocks wide,
oceans and shallow seas with beaches, rivers that run to the sea, mountain lakes, gorges with a
stream at the bottom, caves that connect, ores in veins inside the stone, and a day/night cycle
with clouds you can fly over. Underground there are abandoned mines and crypts with chests, and a
stairway back up.

**Villages** (`mods/villages`, `mods/villagers`). Villages and towns with a square, streets,
houses, fields, a forge and lamps, joined by paths that cross rivers on bridges. They are
inhabited by three species: human farmers, frog fishers and mole gatherers, each with a home, a
job, meals and a bed. Every village has:

- a watch (guards, archers, and an inspector who checks bags for stolen goods: theft is marked,
  and a thief is forgiven once, then hunted);
- a mayor who sells plots and houses, and a builder who builds them block by block with materials
  brought by the village's lumberjacks, miners and diggers;
- a merchant and workers who trade in emeralds, with prices set by supply, and a treasury;
- a mine, a craftsman who remakes tools, and homes for every role.

**Territories** (`mods/territory`): a boundary stone founds a territory that grows while its
owners stand in it. Inside, owners are faster and tougher, strangers weaker; what strangers take
there is marked stolen.

**Boats** (`mods/boats`). Five planks make a boat: right click the water to sit in it and row at
twice a swim, right click a shore (or run aground) to step out with it back in the bag.

**Weather** (`mods/weather`). Clear skies, then rain or a storm for a few minutes: the sky
and the clouds go grey, rain falls round you and stops at roofs, the fields under the open sky
are watered. In the tundra and on the peaks it snows and the snow settles; in the desert
nothing falls. A storm strikes the ground near players, with thunder. Seasons of five days each:
the leaves turn orange in autumn and dull in winter (spruce stays green), summer storms more,
spring rains more, the fields slow in autumn and in winter grow only under lamps, and in winter
it snows everywhere but the desert. Traders charge double for food in winter and for wood in
autumn.

**Creatures.** Zombies and skeleton archers come out in the dark and burn in the sun; pigs, cows,
sheep and chickens graze, breed and grow; a dog and a cat can be tamed and follow you
(`mods/mobs`, `mods/animals`, `mods/pets`).

**Survival and crafting.** Bag and hotbar, crafting, chests (two side by side make a double one),
a furnace with input, fuel and output rows, tools in five materials that wear out, armour in
four, hunger, experience and an enchanting table, beds that skip the night, farming (hoe, seeds,
wheat, bread, water and light), fishing, sugar cane, paper and books, doors, ladders and
trapdoors.

**Machines.** Cinnabar circuits (`mods/cinnabar`): dust, levers, buttons, torches, repeaters,
comparators, lamps, pistons, observers and hoppers, so clocks and logic work. Conveyor belts
(`mods/conveyor`) that merge, turn and climb, with sorters and extractors. Helper robots
(`mods/helpers`) you program with up to six orders: chop trees, plant, dig, mine, sort chests,
cook, quarry, light up the land, follow you.

**Games on the plaza.** Chess (`mods/chess`) with every classic rule, a clock and nine variants
(960, king of the hill, three-check, racing kings, losers, atomic, crazyhouse, horde). Cards
(`mods/cards`) with the forty-card Neapolitan deck: briscola, cinquiglio, scopa and tresette, for
two to five players, with the table playing the empty seats.

**Characters and gear.** Choose robot, human, mole man or frog man, a style, three colours and
two accessories, all seen by everyone. On your back: a jetpack to fly, a leather backpack with
27 slots of its own, or a metal backpack with two jointed arms that each hold a tool. The
creative catalogue (C) has a search box.

### The engine

- **Voxels.** 32³ chunks, greedy meshing on threads, 12-byte vertices, flood-fill light from the
  sun and from blocks, ambient occlusion, frustum and occlusion culling, an on-screen profiler
  (F6).
- **Light.** Real darkness: a cave without torches is black, and night is nearly so. Sun and moon
  cast shadows (leaves too); torches light the faces turned towards them. The old light is still
  there: Options, Light: classic.
- **Portals.** A seamless image of the real view, recursion up to three levels, all 24
  orientations (floor, wall, ceiling, turned), bodies cut as they cross, and the horizon levelled
  after a tilted crossing. You keep your speed through a portal, turned with it. Portals are
  **built in game** (`mods/portal`): a frame around a hole opens when its last block goes in, up
  to 16 per world. An opening without its other end is closed by an animated membrane.
- **Worlds and servers.** Two worlds in one process, portals to **another server** with signed
  tickets, and several worlds (rooms) on one server and port. The world has no edge: columns are
  generated or loaded as players get near, and dropped when nobody is.
- **Network.** A headless server at 20 ticks a second, ENet + zstd, X25519 + XChaCha20-Poly1305
  encryption, key-based player identity, content-addressed media with a disk cache shared between
  worlds. The client **adopts** the server's block table; the server validates movement and casts
  the rays of every shot.
- **Modding.** Luau, one VM per mod with a memory quota, an interrupt budget and capabilities. Hot
  reload (F5), a typed event bus, a scriptable ECS, ABM/LBM, mapgen stages, blocks as boxes or
  models, per-world and per-body movement, `.cnpmod` packages with versions and dependencies. A
  world can send **Luau code to the client**, in a sandbox without files or network, and the
  player can refuse it. Guide in [`docs/MODDING.md`](docs/MODDING.md).
- **Saving.** SQLite: chunks, chests, bags, players' positions and the mods' storage. Only what
  changed is written, in one transaction, and the server saves when stopped with Ctrl+C. Blocks
  are stored by name, so adding a mod never forces a new world.

The full plan and its decisions are in [`docs/DESIGN.md`](docs/DESIGN.md).

## Playing and testing: reference

After building ([above](#building-from-source)), from the checkout:

```sh
ctest --test-dir build                       # 13 tests, ~3 s; -LE gpu skips the one that opens a window

./build/src/client/cnp_client                # the title menu
./build/src/client/cnp_client --quick        # quick start: a local test world, nothing saved

./build/src/server/cnp_server --listen 25301 --ticks 0 --world 128 --world-file world.db
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name player

# The arena shooter: a sandbox and the arena on one server
tools/fps.sh --fresh

# The arena alone: the client has no mods, the server sends them
./build/src/server/cnp_server --listen 25302 --mods games/yafps/mods --textures games/yafps/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25302 --name player

# The action RPG: a sandbox and the arpg world on one server, behind a stone arch
tools/arpg.sh --fresh

# The timed track: with --portals you spawn on the road, in front of the start
./build/src/server/cnp_server --listen 25303 --portals --mods games/track/mods --textures games/track/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25303 --name player

# A sandbox with two portals, to the arena and to the track, on three servers.
# Each leads back to the opening it was entered from (/0, /1). Without --ticks 0 a server stops after 50 s
./build/src/server/cnp_server --listen 25301 --ticks 0 --portals --portal-to 127.0.0.1:25302/0 --portal-to 127.0.0.1:25303/0
./build/src/server/cnp_server --listen 25302 --ticks 0 --portals --portal-to 127.0.0.1:25301/0 --mods games/yafps/mods --textures games/yafps/textures
./build/src/server/cnp_server --listen 25303 --ticks 0 --portals --portal-to 127.0.0.1:25301/1 --mods games/track/mods --textures games/track/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name player

# The same on one server, as rooms: sandbox, track and inferno (tools/track.sh does this, with every world)
./build/src/server/cnp_server --listen 25301 --ticks 0 --portals \
    --room sandbox --portal-to @pista/0 --portal-to @inferno/0 \
    --room pista --mods games/track/mods --textures games/track/textures --portal-to @sandbox/0 --portal-to @inferno/2 --portal-to @inferno/1 \
    --room inferno --mods games/inferno/mods --textures games/inferno/textures --fallback-textures assets/textures --portal-to @sandbox/1 --portal-to @pista/2 --portal-to @pista/1
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name player   # --room inferno to go straight there
```

### Controls

| | |
|---|---|
| WASD, Space, F | move, jump, fly (with a jetpack) |
| Left / right click | break / place (with an empty hand, use the block) |
| Wheel or 1-9, E, C, Q | hotbar slot, bag, creative catalogue, drop |
| T, / | chat, chat with a command (`/help`, `/list`) |
| V, F11, ESC | third person, full screen, pause menu |
| F3, F4, F8 | info, territory borders, cells where monsters can spawn |
| F5, F6, F7, F2 | reload mods, profiler, next texture pack, occlusion culling off |

Options are kept between runs. `CNP_HITCH_MS=30 cnp_client ...` prints every frame longer than
30 ms with the profiler's sections.

### The title menu

**Play** starts a world of your own on a local server, with the key in
`~/.local/share/cubes-and-portals/player.key`; the list shows the saved worlds, the green button
on top creates a new one (name and seed), and Edit renames or deletes one. **Multiplayer** keeps
a list of servers, each with a coloured dot (grey untried, amber trying, green with its ping, red
if it did not answer). The account belongs to the server: you pick a server, then give a name and
password for it, from which your identity is derived (Argon2id) instead of being kept in a file;
the server's key is pinned at the first join. **Host a server** opens your worlds to others on a
port of your choice, and the pause menu shows the address to give them. **Character** picks
species, style, colours and accessories. **Options** holds mouse, volume, light, shadows, full
screen, language and the update check.

**Translations** are `assets/lang/<code>.txt`, one line `text as written = translation` with `{}`
for the parts that vary. They apply to everything the client draws: menus, mods' panels and
notices, chat and HUD text. A missing line leaves the text as it is.

### Client (`cnp_client`)

| Flag | What it does |
|---|---|
| (none) | The title menu (above) |
| `--quick` | Quick start, no menu: a single-process local world that is not saved |
| `--connect HOST` `--port N` `--name NAME` | Join a server instead of making a local world |
| `--livery N` | What the player looks like, for a join with `--connect` |
| `--server-key HEX` | Accept only the server with this key (the server prints it at startup) |
| `--player-key FILE` | Where the player's identity is (default `player.key`, created on first run) |
| `--watch HOST:PORT` | Look into another world as an observer (repeatable) |
| `--world-code refuse` | Do not run the code that worlds send to the client |
| `--no-update-check` | Do not ask GitHub for a newer release (like Options, Updates: off, but for this run only) |
| `--media-cache DIR` `--no-media-cache` `--media-cache-size MB` | Cache of downloaded media |
| `--textures DIR` (repeatable) `--no-textures` | Texture packs; with more than one, F7 cycles them |
| `--models DIR` `--no-models` `--model FILE.gltf` | Where to look for models; a character in front of the spawn |
| `--mods DIR` `--seed N` `--world N` | Local world: mods, seed, side in blocks |
| `--day-length S` `--time T` | Day length in seconds, starting time (0..1) |
| `--dual` `--facing` `--oriented` | Test scenes: two worlds, two facing portals, a portal lying down |
| `--recursion N` `--portal-scale F` `--portal-config FILE` | Recursion depth (0-3), render target resolution, portal shape ([`config/portal.cfg`](config/portal.cfg)) |
| `--log-view` | Trace the view around every crossing |
| `--bench` `--dual-bench` `--soak S` | Measurements: FPS and scripting cost, cost of the second world, automatic flight |
| `--verify` | The portal continuity test (run by `ctest`) |
| `--play-seconds S` `--watchdog S` | Exit after S seconds; dump the stack if a frame does not arrive within S seconds |
| `--shot S` | A screenshot in `bench/play.png` after S seconds of a timed run |
| `--fly-down N` `--walked-down` | In a timed run: fly up N blocks and come back down, reporting what landing costs |

The full list is in `main()` in [`src/client/main.cpp`](src/client/main.cpp).

### Server (`cnp_server`)

| Flag | What it does |
|---|---|
| `--listen PORT` | Accept players. Without it, generate the world, run `--ticks` ticks and print its hash |
| `--bind HOST` | The address to listen on (default all of them); the client's menu uses 127.0.0.1 |
| `--world N` `--seed N` `--world-file FILE` | Side of the region generated at startup (default 128; the world has no edge), seed (default 1), SQLite file to save to. One server per file: a second one finds the lock and exits |
| `--save-every S` | How often to write what changed (default 10 s; 0 only on shutdown). Ctrl+C or SIGTERM save and stop |
| `--mods DIR` `--no-mods` | The world's mods |
| `--textures DIR` `--no-textures` | The media the world serves |
| `--day-length S` `--time T` | Day/night cycle |
| `--key-file FILE` `--no-encryption` | The server's identity; encryption off (testing only) |
| `--max-players N` `--require-identity` `--no-observers` | Seats, players required to have a key, no observers from other worlds. A name belongs to whoever used it first, kept in the world file |
| `--names` `--free-name NAME` | With `--world-file` and no server running on it: who owns each name, or free one. Whoever joins next with that name inherits the old owner's bag and position |
| `--owner NAME` | Whoever plays under NAME with an identity owns the server. The client passes it for the worlds it starts |
| `--config FILE` | The flags from a file, one `key value` (or `key = value`) a line, `[room NAME]` for a room, `#` comments; `key = true` is the bare flag. Read where it stands, so flags after it win |
| `--rcon PORT` `--rcon-password PW` | Admin commands over TCP on 127.0.0.1 only: `tools/admin.sh -p PORT ban bob 2h`. Put the password in the config file, not on the command line |
| `--motd TEXT` | The message of the day, said in chat to each player coming in (also `/motd`) |
| `--restart-at HH:MM` | A restart every day, with ten minutes of warnings; under `tools/serve.sh`, which starts it again |
| `--backup-every 6h` `--backup-keep N` | Copies of every world file beside it (`NAME.backup-DATE`), made while the server runs; the newest N stay (5). `/backup` makes one now |
| `--max-per-address N` | At most N players from one address |
| `--watchdog S` | A room that has not finished a tick in S seconds aborts the server with its stack printed, for `tools/serve.sh` to start it again |
| `--federation PORT` | Friend servers (`/friend`): sealed, signed lines on this TCP port, under the ticket key (`--ticket-key`, or WORLD.sign) |
| `--rules-file FILE` `--no-update-check` | Rules each player accepts before playing; no asking GitHub for a newer release ([`docs/ADMIN.md`](docs/ADMIN.md)) |
| `--webhook URL` | Admin actions, start, stop and reports POSTed there as JSON ([`docs/ADMIN.md`](docs/ADMIN.md#webhooks)) |
| `--report FILE` `--since 7d` `--json` | No server: what a world file remembers -- players, play time, most at once, joins by hour, rooms, who was turned away, staff actions, restarts and crashes, crossings in from other servers |
| `--portals` `--portal-to HOST:PORT[/N]` | A pair of server-managed openings; `--portal-to` says where the first leads (and which opening `N` you come out of); repeated, the second, third and fourth |
| `--ticket-key FILE` `--portal-peer-key HEX` `--trust HEX` `--require-ticket` | Signed tickets for crossing between servers |
| `--mobs N` `--ticks N` `--quiet` | Test mobs, ticks before exiting, less logging |
| `--room NAME` | A room: one more world on the same server and port. What follows applies to that room only (`--mods`, `--world-file`, `--portal-to`...), what comes before the first `--room` to all of them. `--portal-to @NAME[/N]` leads to another room; bag and health are shared between rooms. The client picks a room with `--room NAME` |

### Server administration

Typed in chat by a player whose role allows it, on the server's stdin without the slash, over
RCON (`tools/admin.sh`), or with the buttons of the `/admin` page. Roles (guest, player,
helper, moderator, admin, owner, also per room), bans, mutes, the whitelist, warnings,
reports and notes, rules to accept, alt accounts, restarts with warnings and maintenance, the message of the day, visits and
`--report`, `/inspect` and `/rollback` of what a player changed, backups, webhooks, `/bag` and
`/give`, and friend servers that share bans, presence, a chat and one bag for each player.

**Every command, permission, flag and `server.conf` key is in [`docs/ADMIN.md`](docs/ADMIN.md).**
`/help` in the game lists the ones you may use.

`tools/serve.sh -l logs build-release/src/server/cnp_server --config server.conf` keeps a
server running across restarts and crashes, with a log a day.

### Tools

| | |
|---|---|
| `tools/play.sh` (`local`, `--fresh`) | Builds in Release, starts server and client with every mod and a world saved in `worlds/play/` |
| `tools/track.sh` (`--fresh`) | A server with the sandbox and every world as rooms, as the title menu starts it (worlds and logs in `worlds/track/`) |
| `tools/arpg.sh`, `tools/fps.sh`, `tools/chambers.sh` (`--fresh`) | The sandbox and one world, for working on that world |
| `build/tools/cnp_modpack pack mods/ore` | A mod in a `.cnpmod` file, which loads from `mods/` like a directory |
| `tools/dist/package-*.sh` | The packages for Linux, Windows and macOS ([`docs/BUILDING.md`](docs/BUILDING.md)) |
| `build/tools/cnp_netshape` | A UDP relay with delay and loss, to try slow links |
| `build/tools/libcnp_clock_skew.so` | An `LD_PRELOAD` that shifts a process's clock, for the tickets |
| `tools/two_machines.sh` | Two servers in containers, with real MTU and NAT |
| `tools/admin.sh` (`-p PORT`) | One admin command to a running server over RCON; the password in `CNP_RCON_PASSWORD` |
| `tools/serve.sh` (`-l DIR` `-k DAYS` `-w URL`) | Keeps a server running across restarts and crashes, a log a day; crashes POSTed to the URL |
| `build/tools/cnp_bake_textures` | Regenerates the blocks' placeholder textures |

### Tests

Thirteen, chosen because they guard things that break silently and cost dearly: input from the
network (`protocol`, `chunk_codec`, `crypto`), saved worlds (`storage`, `migration`,
`persistence`), portal maths and collisions (`portal_math`, `portal_traverse`, `physics_box`,
`portal_continuity` on the GPU), the mods' sandbox (`script_isolation`), and client and server
agreeing on the world (`worldgen`, `two_process_join`). The rest is checked by playing. Why each
one exists is in [`tests/CMakeLists.txt`](tests/CMakeLists.txt).

## Technology

| | |
|---|---|
| Graphics | raylib 6.0 behind a thin `gfx_*` interface, OpenGL 3.3 core |
| Languages | C++20 for the engine, Luau 0.733 for scripting |
| Network | ENet 1.3.18, zstd 1.5.7, Monocypher 4.0.3 |
| Physics | Jolt (vehicles) and the engine's own (bodies, blocks, portals) |
| Storage | SQLite 3.50.4 |
| ECS | Flecs 4.1.6 |

## Documentation

The documents are in Italian.

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): how the code is laid out today and where a new
  feature goes.
- [`docs/DESIGN.md`](docs/DESIGN.md): feasibility study, decisions, milestone plan, risks.
- [`docs/MODDING.md`](docs/MODDING.md): writing a mod; [`docs/API.md`](docs/API.md): the Luau
  API, generated from `src/script/api.luau`.
- [`docs/TEXTURE_PACKS.md`](docs/TEXTURE_PACKS.md): making a texture pack.
- [`docs/BUILDING.md`](docs/BUILDING.md): building, packages and versions.
- [`TODO.md`](TODO.md) (English): what is missing and what is broken.
- [`docs/archive/`](docs/archive/README.md): the history, milestone by milestone, with the
  measurements.

## License

The code is under the MIT licence ([`LICENSE`](LICENSE)). The libraries in `third_party/` each
have their own licence, in their folder.
