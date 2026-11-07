# Architecture

*[Leggi in italiano](ARCHITECTURE.it.md)*

How the code is made today, and where a new feature goes. The why of the big choices is in
[`DESIGN.md`](DESIGN.md); the why of the small ones is nearly always in the comment at the top of
the file.

## Programs and libraries

```
cnp_client ─┬─ cnp_script ─┬─ cnp_core
            ├─ cnp_net ────┘
            └─ raylib (only under src/client/gfx and src/client/snd)
cnp_server ─── cnp_script, cnp_net, cnp_core      (no GPU, no window)
```

| Target | What it holds |
|---|---|
| `cnp_core` | Everything with neither a socket nor a GPU: voxels, light, mesher, mapgen, physics, portals, registries, items, ECS, media, protocol and the sessions' state machine |
| `cnp_net` | What touches the outside world: the ENet transport, encryption, the chunk codec, `ClientSession`, SQLite (`storage/world_db`) |
| `cnp_script` | The Luau runtime, `ModHost` (a VM per mod), the event bus, the code a world sends to the client |
| `cnp_modpack_lib` | The `.cnpmod` format |
| `cnp_client` | Window, render, input, audio, HUD; local or connected play |
| `cnp_server` | The headless server |
| `tools/` | `cnp_api_gen` (bindings), `cnp_modpack`, `cnp_bake_textures`, `cnp_netshape`, `cnp_clock_skew` |

## Rules the code keeps

- **No raylib above `gfx.h`, `platform.h` and `snd.h`.** It is what keeps the move to sokol open.
  `src/core` never includes anything graphical.
- **No global state per world.** Everything that belongs to a world is in a client `Session`
  (`src/client/main.cpp`), and the `Scene` holds more than one: that is how a portal shows another
  server.
- **The server is the authority, but it does not simulate the physics.** The client moves the
  body and sends a claim; the server **checks** it (`check_move` in `core/physics/movement.h`,
  called from `net/session.cpp`) and corrects. Digging, placing, blows, chests and crafting are the
  server's to decide.
- **A rule is written once.** When client and server have to agree (how long a dig takes, whether a
  box is inside a block, where one is born) the function is in `core/` and both call it.
- **Ids are given in registration order; names travel on the wire and on disk.** The client adopts
  the server's block table; the world file refuses a different table.
- **Media are addressed by content** (`core/media/content_hash.h`): a file's name is the hash of
  its bytes, and the client's cache is one for every world.
- **The engine emits events and has no private game rules.** Protecting an area, replacing a block,
  generating ores: they are mods.

## A map of `src/`

### `src/core`

| Folder | Main files |
|---|---|
| `voxel/` | `chunk` (32³, id + light + facing), `mesher` (greedy, 12-byte vertex with smooth and directional light), `mesh_pool` (meshing on threads), `light` (flood fill of sun and blocks), `raycast`, `dig` (the client side of a dig), `find` |
| `worldgen/` | `worldgen` (stages in order, a column of chunks at a time; `world_hash`), `biome`, `noise`, `spawn` (in front of an opening, on the plaza, or on dry land for a survival world) |
| `world/` | `columns` (which columns are in memory: blocks, then light, then let go), `daylight` (the world's clock; `sky_light_at`: sun and moon for drawing), `modifiers` (ABM/LBM), `node_meta` (what a cell remembers) |
| `registry/` | `node_registry` (blocks), `node_shape` (a block's boxes, turned), `core_nodes` (the engine's blocks), `model_registry` |
| `physics/` | `aabb` (the player's box against the world), `movement` (limits and validation), `fall`, `rigid` (boxes that tumble: one Jolt world per ECS world, the blocks round each box as static ground; `core.mob_tumble`) |
| `portal/` | `portal_math` (oblique near plane, clip plane), `portal_traverse` (where a body comes out, facing which way), `portal_clip` (cutting the solids), `portal_tuning` (the six numbers of `config/portal.cfg`) |
| `item/` | `item_registry`, `inventory`, `container` (chests), `recipe`, `craft_grid`, `item_codec` (on disk) |
| `ecs/` | `components` (Flecs), `health`, `drop`, `mob`, `lua_ecs` (the mods' components and systems) |
| `media/` | `content_hash`, `media_set` (server side), `media_cache` (client side), `png`, `gltf`, `texture_pack`, `face_tiles`, `tile_bake` |
| `net/` | `protocol` (messages, version, handshake), `session` (`ServerSessions`: everything the server does with a peer, without a socket), `client_session`, `interest` (which chunks and in what order), `chunk_codec`, `transport` (ENet), `crypto`, `bytes` |
| `storage/` | `world_db` (SQLite: chunks, chests, bags, metadata, migrations), `column_keeper` (the columns coming in, lit and let go; server and lone client) |
| `ui/` | `panel` (panels declared by mods, layout and hit test) |
| `audio/` `diag/` `math/` | Attenuation and tones; profiler and stack dump; vectors, matrices, frustum |

### `src/script`

| File | |
|---|---|
| `script.h`, `script_luau.cpp` | The border with Luau: VM, quotas, interruption, capabilities taken away before the mod starts, the `script_host` table of functions the engine offers |
| `api.luau` → `api_generated.inc` | The API's signatures, **once**; the C++ glue, `docs/api.d.luau` and `docs/API.md` are generated (`cnp_bindings`) |
| `mod_host` | Loads `mod.conf`, versions and dependencies, a VM per mod, hot reload, `side = server/client/both` |
| `event_bus` | Events with a schema, priorities, cancelling |
| `world_code` | The code a world sends to the client: its name in the media, the package, a narrowed host table |

### `src/client`

| File | |
|---|---|
| `main.cpp` | `Session` (a world) and `Scene` (every world), the mods' host functions on the client side (`make_client_host`), `run_interactive` (local play), `run_network` (connected play), `run_bench`, `run_dual_bench`, `run_soak`, `run_verify`, and `main()` with the flags; the title menu and `serve_world` (a world of one's own, sandbox or survival) |
| `player.cpp` | The player's movement: input, gravity, collision, step |
| `gfx/` `snd/` | The only place that includes raylib |
| `render/` | `world_render` (chunks and atlas), `portal_render` (targets, recursion, `draw_closed`: closed portals' membrane and its opening), `body_render`, `model_render` and `model_store` (glTF), `selection`, `mod_pass` (the mods' pass), `shaders.h` |
| `ui/hud.cpp` | Hotbar, bag, panels, dig bar, notices |

### `src/server/main.cpp`

Flags, making the world (the region round the spawn, mods, file), the mods' host functions on the
server side, and the 20 Hz tick: the peers' messages, the mods' and ECS systems' steps, ABMs,
columns generated or read back round whoever is there and let go where nobody is, chunk
streaming by interest, entities, health, time, doors between servers.

**Difficulty** is one number of the room (`ServerState::difficulty`, 0 peaceful .. 3 hard), kept
as `<world file>.difficulty` so the title menu reads and writes it without a server. Peaceful has
two gates only: the natural spawner is skipped and `host_spawn_mob` refuses a kind that `fights()`;
turning it on removes those about. Easy and hard scale the spawn cap and the blow a creature lands
on a player (the hurt callback of `step_mobs`). What a mod does in its own fights (sieges, an
altar's boss) it asks `core.peaceful()` for, and `core.mob_leave` gives a kind's drops with no
creature. The world's **rules** (`core.rule`) are names and values the engine keeps in
`<world file>.rules` and gives no meaning: each mod reads its own.

**The world has no edge.** A column of chunks comes into memory when a session gets within four
columns of it: from the world file if it is there, from the generator if not (core and the mods'
stages, written only inside the column). It has its light when the eight round it have their
blocks, and only then is it sent, saved and swept by ABMs. It leaves after thirty seconds more than
eight columns from every session and from everything on it (stacks, mobs, the mods' things): if
it has its light it is kept in the file, or in memory compressed if it changed and there is no
file, and whoever had its chunks gets `ChunksGone`. These rules are one class,
`core/storage/column_keeper`, used by the server and by the client playing alone; only the hooks
change (which stages, whom to tell).

## How a game runs

**Local.** `cnp_client` generates the world with the same `Mapgen` as the server, loads the mods of
`mods/` into the `Session`, and `run_interactive` runs input, `player_update`, mods, meshing and
render in the same process.

**Connected.** `ClientSession::connect` opens the encrypted connection and sends `Hello`. The
server answers with `Welcome`: seed, the world's hash, features, the mods' channels, the block
table with shapes and models, the movement limits. The client adopts the table, downloads the media
it does not have, loads the world's code unless it refuses it, and from then on receives chunks
(the `Chunk` channel, where `ChunksGone` with those to let go comes too), bodies (`Entity`,
unreliable) and events (`Event`); it sends position claims, digs, placements, blows and clicks.
The channels from `kModChannelBase` (6) up are the mods'.

**Crossing.** A local portal moves the body with `portal_traverse`; the server decides it and
sends a `Teleport` with the exit velocity (`exit_velocity` in `core/net/session.cpp`): the entry
velocity turned by the map, with the energy evened out only between the entry point mapped through
the pair and the exit point, never between the two openings' heights. A portal to another server
uses a signed ticket: the client already has an observer session open on that world (to draw it
inside the opening) and promotes it to the active session. If it cannot, the server sends a
`Redirect` and the client reconnects.

**Rooms.** A server can hold several worlds on the same port (`--room`, protocol 56): each is
`serve_room` on a thread of its own, and the `Router` hands each room its peers' events. One room
at a time holds the common lock (`TurnLock` in `src/server/main.cpp`) and lets it go only while it
waits for the network; the lock is handed on **in the order it was asked for**, and a room behind
on its ticks lets it go once a poll anyway. With a `std::mutex` a busy room (the Inferno making
columns) took it back before the others woke up, and the track's players went unanswered for more
than a second. Each room writes in the log how long it held it at most. A room's mouths (up to 24
since protocol 111) are its ways onto the others.

## How light is drawn

The server works out only the levels 0..15 (sun and blocks, flood fill in `core/voxel/light.cpp`)
and sends them with the chunks; all the rest is the client's, and the game (spawn, `core.light`)
goes on reading the levels and `daylight_at`.

- **Mesher** (`core/voxel/mesher.cpp`): for each corner of a face the mean of the four cells that
  touch there, already through `light_curve` (brightness, not level). The blocks' light is turned
  towards the face: the direction is the flood fill's slope (uphill towards the lamp), full in
  front, 0.6 edge-on, 0.25 from behind (`kGrazing`, `kAway`, `kNormalBias`). That is why the
  snapshot has a border of two cells and not one. A glowing block lights itself.
- **The world's shader** (`render/shaders.h`): sky = sky visibility × (ambient by the face's
  direction + sun × N·L × shadow); then the brighter of sky and lamps, × AO, over a floor of 2%
  (`kLightFloor`).
- **Shadows** (`WorldRenderer::draw_shadows`): an orthographic shadow map round the camera, held
  on the texel grid, with leaves cut out by the atlas's alpha. The main view draws it, and each
  view inside a portal for its own virtual camera, before opening its own target
  (`gfx_shadow_create`: depth only, compared in hardware).
- **Sun and moon** in the clouds' pass; **bodies and things in hand** take the light of their cell
  (`WorldRenderer::cell_light`).
- **Classic** (`set_classic`, Options → Lighting): the old formula, from the packed light byte the
  vertex still carries; no shadows and no sun.

## Traps already fallen into

Each of these was a defect found in play, and each rule below is what keeps it from coming back.
When a change touches one of them, check it the way the line says.

- **rlgl unbinds our shader behind our back.** `rlSetBlendMode` on a change of mode first draws
  raylib's own batch, and if that batch holds anything (debug text, 2D) it ends with
  `glUseProgram(0)`: the next draw goes out with no program and lands nowhere. `gfx_state_set`
  binds `g.current_shader` again after it. Any new call into rlgl that may flush its batch
  (`rlDrawRenderBatch*`, `rlSetBlendMode`, `rlSetShader`) between binding a shader and drawing
  must do the same. Symptom: glass (the translucent pass) missing in some scenes and not others,
  directly and through portals (2026-11-02). Check: the test chambers' glass with F3 on, and
  `tools/portallab.sh` `/lab view`.
- **A turn waits with its block.** `core.set_facing` on a column not taking writes yet is deferred
  and checked when it is put in, on the block that is there then -- never against the cell as it is
  before (the air a door replaces). Symptom: doors and track pieces turned the wrong way now and
  then. Check: the server log of `tools/track.sh` on fresh worlds has no "is not a turn".
- **A crossing has one point of the body.** The FEET through a floor (they reach it first), the
  EYE through a wall and a ceiling (`crossing_point`, `core/portal/portal_traverse.cpp`). Measured
  at the feet, a ceiling took the head 1.6 into the block first. Check: `/lab head`, a jump under
  the roof crosses.
- **The exit continues the picture.** `exit = trigger + view` (config/portal.cfg) is right for a
  crossing by the eye. Out of a floor into a wall the eye is still its height above the plane, so
  the exit is where the image had it, as far back as a body fits in the opening's node. Check: the
  jump between where the opening showed the eye and where it is put -- 0.01 wall to wall, 0.92
  floor to wall (the rest is wall); a change that raises either is a regression.
- **Between two horizontal openings the heading is kept** (`portal_carry_look`): only the tilt
  follows the map, and the roll settles back level (`settle_roll`, eased, 0.25-0.6 s). The look
  is never turned on foot after a crossing; only the horizon is. Check: `/lab bounce`, the yaw
  stays what it was on every crossing.
- **A remote body glides from where it had got to.** `glide` (client `main.cpp`) starts each new
  stretch from the point the old one had reached, never from where the body was drawn last frame
  with the clock reset: with a message every frame (under ~20 fps) that restart left every other
  body standing where it was first seen (2026-11-07). Check: the garden's insects move in Xephyr.
- **Small bodies keep their box under the feet.** `body_box` lifts the box by at most
  `height - min(0.1, height/2)`; the old "height - 0.1" went negative under 0.2 high and hung the
  box 8 cm below the feet. A creature is never under 0.01 × 0.01 (`size_mob_kind`, the client's aim
  box too). A scaling opening's tunnel is as deep as its half height allows (`entrance_depth`),
  or a shrunk eye stands inside the tube and sees sky. Check: `/lab tiny`, the garden's ants on
  the grass.
- **A dedicated scene before a guess.** `tools/portallab.sh` builds portal cases where every
  number can be read (sunlit, no other mods). When a defect shows in a game but not there, the
  difference between the two is the cause: list it before changing anything.

## Where a feature goes

| What | Where |
|---|---|
| A new Lua function | Its signature in `src/script/api.luau`; the host function in `src/server/main.cpp` and, if it is wanted locally too, in `make_client_host` of `src/client/main.cpp`; two lines in `docs/MODDING.md` (and `MODDING.it.md`). `docs/API.md` regenerates by itself |
| An engine event | The schema among the engine's events (`define_core_events` in the client, the event table in the server's `main()`) and the `emit` where it happens |
| A game rule | First ask whether it can be a mod. If it is the engine's: the function in `src/core/`, called by the server (`net/session.cpp`) and by the local client |
| A network message | A struct with `encode`/`decode` in `core/net/protocol.h`, `kProtocol` +1 with a line saying what changes, handling in `net/session.cpp` and `net/client_session.cpp`. A decoder must refuse truncated input: the `protocol` test tries it |
| Saved data | `core/storage/world_db`, with `kWorldSchemaVersion` +1 and a migration from the previous format (the `migration` test) |
| Content (blocks, items, mapgen, mobs) | A mod in `mods/`, see [`MODDING.md`](MODDING.md) |
| Graphics | `src/client/render/`, through `gfx_*`; if a new primitive is needed, in `gfx.h` and `gfx_raylib.cpp` |

## Mods the game includes

| Mod | |
|---|---|
| `spike` | The M0 spike's mod: blocks, a portal that moves |
| `beacons` | Pulsing glowing pillars, with the `os` capability |
| `guard` | Protects the portals' plaza by cancelling `core:pre_place` |
| `palette` | Twenty coloured blocks and a brush |
| `console` | A panel declared in Lua |
| `chest`, `craft`, `tools` | Chests, recipes, tools with level and hardness |
| `steps` | A slab, a stair and a block drawn as a model |
| `timber` | Logs that turn, rungs on the clicked face, moss with ABM/LBM |
| `cave`, `ore`, `trees`, `lakes`, `ruins` | Joined caves, ores, trees, lakes and ruined towers as mapgen stages, a column at a time |
| `depths` | Abandoned mines and crypts underground, planned by region and built a column at a time |
| `portal` | Portals built of blocks: a frame of `portal:frame` round a hole of air, standing or lying, opens an opening; right click on the frame for the menu |
| `furnace` | A furnace: coal smelts raw ore into ingots, sand into glass, and food; it keeps the smelting's experience and gives it to whoever opens it (`furnace:xp`) |
| `creative` | A creative inventory on a panel, key C (`side = both`); it asks the `creative` permission |
| `doors` | Two-block doors, also for whoever finds a way (`core.find_path`) |
| `mobs`, `robot` | Zombies, skeletons, the robot that is the player's body |
| `animals`, `pets` | Pigs, cows, sheep, chickens (food, wool, eggs, leather, breeding); cat, dog, fishing rod |
| `armor` | Sixteen pieces of armour in the four worn slots |
| `infernoportal` | Obsidian and the Inferno's portal built by hand |
| `farm` | Hoe, tilled soil, wheat in four stages, bread, melon and pumpkin stems |
| `bed` | Bed: respawn; at night you lie down and with everybody in bed it is dawn, not with monsters near (`core.player_spawn`, `core.set_time`, `core.monsters_near`) |
| `screwdriver` | Screwdriver: turns a block already placed (`core.node_turns`) |
| `compact` | Blocks of nine ingots, gems or dusts |
| `cinnabar` | Dust, lever, button, torch, repeater, comparator, lamp, pistons, observer, hopper; the circuit is solved in Lua with a queue of delayed changes |
| `torches`, `cane` | Torches; sugar cane on the banks, paper, books |
| `hunger` | Hunger with saturation and exhaustion, a bar the mod draws (`side = both`) |
| `enchant` | Experience and an enchanting table with a random draw and a hint (`side = both`) |
| `gates`, `keys`, `grave` | The worlds' doors (on the plaza, or in a survival world the scattered sites and their mouths); the seven keys, the slots and the level protocol behind the sites; the bag's grave in a world without a plaza |

A world with a way of playing of its own is in `games/<name>/`, with `mods/` and `textures/` to
hand to the server (`worlds/` stays ignored: it is where a game leaves its saves). `games/yafps`:
the previous fork's FPS arena, rewritten -- `yafps_core` (blocks, the arena as a stage, movement)
and `yafps_game` (`side = both`: targets, the blow decided by the server, HUD). `games/track`: the
timed kart track. `games/inferno`: scoria caverns, lava, fortresses, brutes, wraiths and cinders.
`tools/track.sh` serves them all from one server, as the sandbox's rooms.
