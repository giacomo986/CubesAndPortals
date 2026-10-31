# Writing a mod

*[Leggi in italiano](MODDING.it.md)*

Status: M2 under way. This document describes what **works today**, not what is planned; for the
whole plan see `DESIGN.md` §5 and §6.

A mod is a directory inside `mods/` with two files:

```
mods/beacons/
    mod.conf      <- the manifest
    init.luau     <- the code
```

No toolchain. `init.luau` is a text file you open in an editor and it runs. `--!strict` at the top
turns types on, but it is not required (DESIGN.md §5.5).

---

## mod.conf

`key = value` format, lines starting with `#` are comments.

| key | default | meaning |
|---|---|---|
| `name` | — (required) | the mod's name, and the namespace of its blocks |
| `version` | `0.0.0` | `1`, `1.2` or `1.2.3`; what is missing is zero |
| `api` | the oldest served | the whole number of the mod API the mod is written for |
| `description` | empty | one line |
| `entry` | `init.luau` | the file loaded |
| `enabled` | `true` | `false` = there but not loaded |
| `capabilities` | empty | a comma-separated list, see below |
| `depends` | empty | mods that must be loaded first, with or without a version |
| `memory_kb` | 32768 | the ceiling of the VM's heap |
| `interrupt_budget` | 2000000 | safepoints a single call |

**An unknown key is a load error, not a warning.** With `capabilties = os` written by mistake, the
mod does not start: a manifest that reads as "no capabilities" because of a typo is exactly how a
permission system ends up lying.

A directory without `mod.conf` is not a mod that gets everything: it is not a mod.

---

## Versions

`version` is **three numbers**, not a free string: `1`, `1.2` and `1.2.3` are all versions, and
what is not written counts as zero. Everything else -- a letter, an empty component, a fourth
number -- is a load error.

The reason is that a version is there to be **compared**. As a string `1.10` comes before `1.9`,
which is the opposite of what whoever writes `>= 1.9` asks for.

### `depends`, with or without a version

```
depends = spike >= 0.2, guard
```

A name alone means "any version", and that is what every `depends` in this repository meant before
versions could be compared. The operators are five: `>=`, `>`, `=`, `<=`, `<`. Spaces as you like:
`spike>=0.2`, `spike >=0.2` and `spike >= 0.2` are the same thing.

Two different refusals, because they are two different problems:

```
mod 'beacons' not loaded: depends on 'spike', which is not loaded
mod 'beacons' not loaded: depends on spike >= 2.0.0, and the one loaded is 0.2.0
```

The first sends you to install a mod, the second to update the one there is. Saying the first when
the truth is the second is an afternoon lost.

### `api`, that is which engine the mod is written for

`api` is **a whole number**, not a version. It says which version of the mod API the mod was
written for. This build serves only one -- `api = 1` -- and refuses any other:

```
mod 'futuro' not loaded: written for mod API 2, and this build serves 1 to 1
```

A mod that does not declare it is taken for the **oldest** this build serves. It is the only
assumption that cannot quietly promote an old mod onto a new API: the day the oldest served goes
up, that mod is refused and told to declare what it wants, instead of failing at line 40.

The log line says it, and says too when nobody told it:

```
mod 'cave' 0.1.0 (api 1) loaded, capabilities: none
mod 'vecchio' 0.1.0 (api 1, assumed) loaded, capabilities: none
```

The same rule holds on reload: a `mod.conf` changed to an API this build does not serve is **not**
loaded, and the version from before goes on running.

---

## Things that happen to cells by themselves

Two kinds, and they are not the same thing:

```lua
-- ABM: every `interval` seconds, every loaded cell holding one of these
-- blocks has a chance of one in `chance` of the action going off.
core.register_abm("timber:moss", {"timber:log"}, 2.0, 4, function(cell)
    core.set_block(cell.x, cell.y, cell.z, MOSSY)
end)

-- LBM: once per matching cell **when the chunk is loaded**, and never
-- again. It is what is needed to fix a world written by an old version
-- of the mod.
core.register_lbm("timber:clean", {"timber:mossy_log"}, function(cell)
    core.set_block(cell.x, cell.y, cell.z, LOG)
end)
```

The action receives **a table** -- `cell.x`, `cell.y`, `cell.z`, `cell.id` -- which is the shape
of every handler of this engine, from an event to a mapgen stage. The list of blocks can be a
table or a single string.

**They are registered while the mod loads**, like `core.register_mapgen`: asking for one from a
tick handler is refused, because it would be a modifier that never runs. A name already taken is
refused, and ABMs and LBMs share the namespace. An `interval` that is not positive or a `chance`
under one are refused. A block no mod has registered **disables** that modifier and says so in the
log naming it: an ABM on a block that does not exist would sweep the world for ever finding
nothing.

**What it costs, and why it cannot make the tick late.** A world of 512 is 512 chunks and 16.7
million cells. An ABM does not read them: every chunk carries a bit per block id *modulo 64*, so a
chunk that cannot hold the ABM's blocks is skipped with an AND. What is left is read cell by cell,
and each step has a **budget of 2 ms**: if the sweep does not fit, it stops where it is and resumes
at the next tick, and the interval restarts only from a **finished** sweep. A mod asking for more
than that runs slower than it asked instead of making the server late, and the log says so:

```
modifiers: 3 action(s) over 262144 cell(s) in 0.008 s, 0 sweep(s) still going
  timber:moss           3 fired of     262144 tested, 0.008 s
```

**Determinism.** The chance is a **hash** of the seed, the sweep's number and the cell's
coordinates, not a generator whose state depends on how the budget cut the sweep. Two runs of the
same seed do the same thing, and the chunks are visited in sorted order for the same reason.

**An action that errors costs its mod**, like an event handler, and that modifier is not called
again. The others go on in the same step.

**What an LBM does not promise:** if the budget runs out halfway through a chunk's modifiers, that
chunk goes back in the queue and those that had already run run there again. An LBM has to be
written so that running twice on a cell already fixed is harmless -- which is what "loading" means
for a modifier that can be interrupted.

---
## Blocks that turn

A block declares **what the cell's facing byte means**, and it means nothing if it does not
declare it:

| `turn` | values | what it is |
|---|---|---|
| — (nothing) | 0 | does not turn |
| `upright` | 0..3 | quarter turns about the vertical: a chest, a furnace |
| `facedir` | 0..23 | any of a cube's twenty-four orientations: a log lying down, a stair |
| `wallmounted` | 0..5 | the **face** the front points to: a ladder, a sign |

```lua
local LOG = core.register_node("timber:log", 126, 92, 54, true, 0, "facedir", 0, 1.6, "wood")
core.set_block(x, y, z, LOG)
core.set_facing(x, y, z, 12)     -- lying along x
```

`facedir` is `axis * 4 + turn`, where the axis says **where the block's +Y ends up** -- 0 +Y,
1 +Z, 2 -Z, 3 +X, 4 -X, 5 -Y -- and the turn is quarter turns about that new up. **0..3 are
exactly the four turns that existed**, because axis 0 tilts nothing: every byte in every world
file keeps the meaning it had. The axes' order is the starting engine's facedir `param2`, the
engine this project comes from: a convention already tried, taken because it is good. The
compatibility layer it was chosen for is not being made any more (DESIGN.md §7.5), and the order
stays this one anyway -- changing it now would mean rewriting the byte of every turned cell in
every world file.

`wallmounted` is a **face index** in this order: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z. A cell
nobody turned is 0, that is +X; placing always writes the clicked face, so it shows only on a cell
written with `core.set_block` and never turned.

**Placing gives four turns, not twenty-four.** An `upright` or `facedir` block put down by a
player turns towards them, from their **position** (the server has that and not the look); a
`wallmounted` one points out of the face that was clicked. The other twenty orientations exist for
a mod -- `core.set_facing` -- and for the tool that turns a cell already placed (`mods/screwdriver`).

**A byte the block does not allow is refused**, not masked: a chest turned to 7 is the mod
writer's mistake, and `core.set_facing` says so in the log with the mod's name. When drawing, a
cell with a byte its block does not allow is drawn **unturned** instead of reading outside a
table.

**The id never changes.** A block that turns is **one** block: the id `register_node` returned is
the one in the world however it is turned, so a handler compares `ev.id` directly.

**Parts turn with the faces' turn.** A block made of boxes (`core.node_box`) lying down has its
boxes turned by the same matrix that decides which image goes on which face: shape and images
cannot disagree about how it is set.

---

## A mod as a single file

A directory is not a thing you can hand to somebody: nothing says which files belong to it,
nothing says they all arrived, and nothing can be checked before running somebody else's Lua. A
**package** is that file.

```sh
cnp_modpack pack   mods/ore            # ore-0.1.0.cnpmod, from the mod.conf inside
cnp_modpack list   ore-0.1.0.cnpmod    # what it holds, and whether it is whole
cnp_modpack unpack ore-0.1.0.cnpmod ./ore
```

A `.cnpmod` put in the mods' folder loads like a directory:

```
mods/
    cave/                 <- a directory
    ore-0.1.0.cnpmod      <- a package
```

The engine checks it **whole**, unpacks it into a staging folder and from there on it is a mod
like the others -- the same manifest, the same order, the same refusals. Half a mod is worse than
no mod: it would register half its blocks and leave a world full of them, and whoever looks at it
cannot know which half.

The format, in full, is in [`src/modpack/mod_package.h`](../src/modpack/mod_package.h). What
matters when writing mods:

- **The mod's name is not written on the package**: it is in the `mod.conf` inside, the only place
  a manifest is read from. The tool decides the *file*'s name from there. A name outside is a name
  that can lie.
- **A package is canonical**: the files are sorted by path and their bytes are contiguous, so
  packing the same mod twice gives the same file byte for byte. Measured: `mods/ore` gives the same
  `.cnpmod` every time.
- **A path that leaves the mod is refused** -- `..`, an absolute path, a backslash, a colon -- and
  three times: when the package is written, when it is read and when it is unpacked.
- **What is not inside**: files starting with a dot, at every level. A `.git` is not part of a mod.
- **A package is not hot reloaded.** Hot reload watches the unpacked files' modification times,
  which do not change: you edit a directory, you ship a package.
- **Two mods cannot have the same name.** A package beside the directory of the same mod is two
  copies registering the same blocks: the second is refused saying where the first comes from.

---

## Capabilities

Luau starts much smaller than Lua already: `io`, `package`, `loadstring`, `dofile`,
`os.execute`, `os.getenv` **do not exist**. There is nothing to remove because they were never
there. What is left and a mod does not get by default:

| capability | what it grants | status |
|---|---|---|
| `os` | `os.clock`, `os.date`, `os.difftime`, `os.time` | granted |
| `debug` | the `debug` table | granted |
| `native`, `exec`, `fs:read <dir>`, `net:http <host>` | — | **refused**: this build grants them to nobody |

An unknown capability is a load error. One declared but not grantable is a load error **with its
name in the message** -- not a successful load with a surprise at run time.

The globals not granted are **removed from the global table before the sandbox is frozen** and
before a line of the mod runs. They are not hidden nor empty: they are absent. A mod can check it
itself:

```lua
assert(os == nil, "this mod did not declare os")
```

`os` is behind a capability because the tick must be able to be deterministic for seed + input
(DESIGN.md §5.7), and a mod that reads the clock is not.

---

## A VM per mod

Each mod has its own `lua_State`, its own memory quota and its own interrupt budget. The
consequences are checked by `tests/script_isolation_test.cpp` (it runs in `ctest`, without a GPU):

- A mod in an **infinite loop** is interrupted (`script exceeded its interrupt budget`) and
  disabled. The others go on running **in the same tick**.
- A mod that **runs out of memory** gets a Lua error, it does not kill the process. It is the
  lesson of M1's 512 world, where the OOM killer came first.
- **Globals are not shared**: two mods can both have `elapsed` and `on_step` without seeing each
  other.
- A mod that fails is disabled once, not retried every frame.

The 60 s timeout on the test is not red tape: if the interrupt stops firing, the test does not
fail -- it runs for ever inside the bad mod's `while true do end`. Checked by setting that budget
to zero.

---

## Load order

Alphabetical, then reordered by `depends`. It really matters: `beacons` comes before `spike` in
alphabetical order, and at the first run with two mods `core.node_id("spike:copper")` returned
`0`. With `depends = spike` it returns the right id.

A missing dependency is a refusal that names it. A cycle does not block loading: the mods
involved are refused one by one, each saying what it was waiting for.

---

## Hot reload

Change a file of a mod and save: within half a second the mod is reloaded, **without restarting
the game and without generating the world again**. `F5` forces a reload of every mod.

What reload promises:

- **The new code runs, the old one goes.** A new VM, new globals.
- **A version that does not compile does not cost you the mod.** The new VM is built and run
  *before* the old one is destroyed: if it fails, the version that worked goes on running, and
  the error goes to the log.
- **Block ids stay the same.** The world is full of those blocks; renumbering them would mean
  rewriting every chunk.
- **A block the mod no longer defines becomes an orphan**: id kept, bright magenta texture, still
  solid (nobody should fall through a floor that was there). The same deal the classic block games
  make.
- **Capabilities do not widen by themselves.** The manifest is read and weighed again: adding
  `capabilities = native` and saving makes the reload fail, it does not grant it.
- **The other mods are not touched**: the same VM, the same state, no tick lost.

### State that survives reload

```lua
function on_save(): string
    return tostring(counter)      -- the mod decides what its state is
end

function on_load(state: string?)
    counter = tonumber(state) or 0
end
```

A string, not a table: a generic serializer would have to have an opinion about functions, cycles
and userdata, and would be wrong about all three. The limit is 64 KB, and going over it is an
error -- not a silent truncation.

---

## Typed event bus

An event's schema is **declared**. Emitting a wrong field is an error naming the field, not a
`nil` somebody reads three handlers later.

```lua
core.event.define("guard:alarm", {
    fields = { x = "number", y = "number", z = "number" },
    cancellable = false,
})

core.event.on("core:pre_place", 10, function(ev)
    if ev.x >= 36 and ev.x <= 46 then
        ev.cancel = true                       -- cancel
    elseif ev.id == core.node_id("core:grass") then
        ev.id = core.node_id("core:stone")     -- change
    end
end)

core.event.emit("guard:alarm", { x = 40, y = 21, z = 40 })
```

Types allowed in fields: `number`, `string`, `boolean`. Three, not anything: a bus that carries
anything is a bus whose schema means nothing.

- **Priority**: a number, default 0, run from the lowest to the highest; on a tie whoever
  registered first wins. Declaring it is the point -- load order is not an order.
- **Cancelling** stops the *following* handlers. Whoever came before has already spoken.
- **Changing** the payload changes it for the handlers after **and for the engine**:
  `core:pre_place` with `ev.id` rewritten places the rewritten block.
- **Cross-VM**: a mod emits, another listens, neither calls the other.
- A handler that errors **disables its own mod and that is all**: the event goes on to the other
  handlers.
- Listening to an event nobody declared is a load error that names it. Declaring another mod's
  event again is refused.
- An event emitted needs **all** its fields (`event 'gates:ask' takes 5 field(s), got 4`).

### Engine events

The core uses the same bus, with the same schemas, with no privileged paths (DESIGN.md §6.3).

| event | fields | cancellable |
|---|---|---|
| `core:pre_place` | `x, y, z, id, player` | yes |
| `core:placed` | `x, y, z, id, player` | no |
| `core:pre_break` | `x, y, z, id, player` | yes |
| `core:broken` | `x, y, z, id, player` | no |
| `core:ui_click` | `panel, id, player, text` | no |
| `core:used` | `x, y, z, id, player, held` | yes |
| `core:tick` | `dt` | no |
| `core:hit` | `player, target, damage, killed` | no |
| `core:watched` | `x, y, z` | no |
| `core:world_ready` | — | no |
| `core:net_message` | `player, mod, text` | no |
| `core:command` | `player, name, args` | no — only to the mod that declared it |
| `core:page_open` | `player, page` | no — only to the mod that registered the page |
| `core:page_action` | `player, page, id, text` | no — only to the mod that showed the page |
| `core:input` | `action, player` | yes — only on a client |
| `core:far_moved` | `index, x, y, z, axis, sign, half_w, half_h` | no — only on a server of rooms |
| `core:died` | `player, x, y, z` | no — only on a server; the body is already back at its bed or the spawn, x y z is where it died |

`core:used` is a right click on a block within reach, **with anything in hand**. It names the block
hit, not the face beside it, and it is emitted **before** placing: a handler that sets
`ev.cancel = true` has **claimed** the click, and the placing behind it does not happen. That is how
a chest opens with a block in hand instead of making you put it away -- and without that `cancel`
the same click would open the chest and build on top of it. With **Shift** held and a placeable
block in hand, `core:used` is not emitted: the click places and that is all, against a chest or a
table too.

`held` is what whoever clicked **had in hand**, by name, empty for an empty hand -- and `id` is the
block **clicked**. A pedestal that read `id` to know what it was being offered was reading itself.
The click is the only moment the engine knows what is in hand: `Inventory` keeps a selected slot,
but on the server it is written only when a placing succeeds, so between two placings it is the
last one's slot and not the hand. That is why `held` is on the event and not a call
`core.held_item(player)` that would answer with something true a while ago.

`text` is what the player wrote in a field (`core.ui_field`) when they pressed Enter, `""` for a
button: a client's text, up to 128 bytes, to be treated as untrusted.

`player` is who did it: a number to pass back to `core.ui_show` to put a panel on **that** screen
and on no other. A mod does not make it up and does not keep a table of players. In the
single-process client the field is there just the same and always has the same value, so a mod
written for a server runs there without knowing where it is.

`core:tick` is the tick hook for whoever prefers to subscribe instead of defining an `on_step`
global. Since 2026-10-06 it also reaches a world's code on a connected player's machine, once a
frame with its `dt` (a clock to show, a first message to the server as soon as there is one);
`on_step` and the rest stay the server's. A `both` mod that registers a tick for the server only
puts it inside `if serving`. An emit with no handler costs a hash lookup and an empty vector.

`core:world_ready` is **where everything that looks at the world goes**, and it is not a
convenience. A mod is loaded **before** the world file is opened -- the file is checked against
the *final* block table, so it cannot be opened until every mod has registered its own -- so a mod
reading the world while it loads is reading what the mapgen made, not what was saved. A clock that
read its own count at load read zero every time, and seeing nothing there built again over what was
about to be loaded on top of it. `core:world_ready` is the engine saying: now the world is what it
will be. It is emitted once only, and also on a server with no world file -- a mod waiting for it
must not wait for ever.

`core:net_message` is what arrived **on a mod's channel** -- see `core.net` below. The `mod` field
is the channel read back as a name. The event reaches **only the handlers of that channel's mod**
(since 2026-09-25): a message sent by `hunger` is not heard by `enchant`. Before, it reached
everybody, and on the client the bars of `mods/hunger` and `mods/enchant` swapped their values.

`core:hit` is a player's blow **that landed** on a body (a creature, a mod's entity with health):
`target` is the body's id, `damage` the points taken, `killed` whether it was the last. Emitted
after the fact, on the server and in the client that is its own server. `mods/hunger` counts it as
exertion, `mods/enchant` as experience for a kill.

`core:far_moved`: an opening of another room of the same server, leading to this world's opening
`index`, was moved with `core.portal_place`; the other fields are its new shape. It arrives at the
start of the next tick. It is there to move this end too (`games/inferno` builds the matching frame
that way). A server at another address does not hear it. The columns round every opening are always
generated, even with no players near.

`core:watched` is a write to a cell the mod asked to watch with `core.watch_cell` -- see below. It
arrives at the start of the next tick, once a cell a tick.

`core:died`: a player died at `x, y, z` and has already been put back at their bed or the spawn,
the bag still on them (`ServerSessions::kill_body` says why); a mod that wants a death to cost it
takes it here (`mods/grave`, `core.bag_to_container`).

Known limit: if a version of a mod calls `define` and then fails to load, the changed schema stays.
The handlers do not -- they are taken away with the VM that registered them (without that step the
process segfaults at the first emit after a reload; checked).

---

## The API available today

**Complete reference: [API.md](API.md)** -- generated, with signatures, types and defaults. For
the editor's type checker: [api.d.luau](api.d.luau).

```lua
core.set_block(x, y, z, id)
core.get_block(x, y, z) -> id
core.register_node(name, r?, g?, b?, solid?, light?, turn?, level?, hardness?) -> id
core.node_id(name) -> id            -- 0 if it does not exist
core.node_name(id) -> name
core.set_facing(x, y, z, turn)      -- 0 .. core.node_turns(id) - 1
core.node_turns(id) -> n            -- 1, 4, 6 or 24: how many turns that block has
core.loaded(x, y, z) -> 1 | 0       -- 0: column not in memory, get_block reads air
core.light(x, y, z) -> 0..15        -- light now: the blocks', or the sky's for the hour
core.get_facing(x, y, z) -> turn
core.set_portal(index, x, y, z)     -- on the server: the world's openings, only while mods load
core.portal_open(x, y, z, axis, sign, turn, half_w, half_h, colour?) -> id|-1  -- on the server, when it listens: -1 before, try again; as many as you like: the 32 openings go to those nearest the players
                                    -- colour: how it looks shut while it leads nowhere: 0 green (fixed, default), 1 blue, 2 orange (portal gun)
core.portal_close(x, y, z) -> 0..15|-1                             -- by centre, not by index
core.portal_link(a, b) -> 1|0       -- a and b lead to each other; b = -1 unlinks a
core.log(message)
core.play_sound("mod:thing", x, y, z) -> 1|0  -- to everybody in the world; file mods/<mod>/<thing>.ogg on the client

core.take_item(player, name, count?) -> taken    -- up to `count`, says how many
core.give_item(player, name, count?) -> fitted   -- what does not fit stays the mod's
core.bag_to_container(player, x, y, z) -> moved  -- the bag's 36 squares, as they are, into a container

core.players() -> { id }                         -- who has a body in this world
core.player_pos(id) -> (x, y, z) | nil           -- the FEET, or nothing if not there
core.player_name(id) -> name                     -- "" if not there; stays the same on coming back, the id does not
core.player_look(id) -> (dx, dy, dz) | nil       -- where they look, length 1
core.player_place(id, x, y, z, dx?, dy?, dz?) -> 1 | 0  -- moves and turns a body, like a portal
core.ride(id, host, x?, y?, z?) -> 1 | 0         -- seated on host's body; host 0 = gets off
core.monsters_near(x, y, z, r) -> n              -- fighting creatures within r
core.mob_health(id, hp) -> 1 | 0                 -- a creature's health and its whole
core.raycast(x, y, z, dx, dy, dz, max?) -> (bx, by, bz, distance) | nil

core.meta_set(x, y, z, key, value) -> 1 | 0      -- empty value = delete
core.meta_get(x, y, z, key) -> string            -- "" if there is nothing
core.storage_set(key, value) -> 1 | 0            -- the mod's, in the world file; empty = delete
core.storage_get(key) -> string                  -- "" if there is nothing
core.shared_storage_set(key, value) -> 1 | 0     -- the same, one for every room (still the mod's own)
core.shared_storage_get(key) -> string
core.notice(player, text) -> 1 | 0               -- a sentence on the screen; 0 = everybody
core.chat(player, text) -> 1 | 0                 -- a line in the chat (T); 0 = everybody
core.command(name, help, perm?) -> 1 | 0        -- declares /name; arrives as core:command
core.has_perm(player, perm) -> 1 | 0            -- does the player's role reach perm?
core.role(player) -> string                      -- guest, player, helper, moderator, admin, owner
core.is_muted(player) -> 1 | 0                   -- is their chat held (/mute)?
core.page_register(name, title, perm?) -> 1 | 0  -- a page that /page NAME opens
core.page_show(player, name, markup) -> 1 | 0    -- shows a page to a player

core.find_nodes(x1,y1,z1, x2,y2,z2, name) -> { x,y,z, x,y,z, ... }

core.register_mapgen(name, fn)                   -- at load, not after
core.terrain_height(x, z) -> y                   -- the ground according to the generator
core.water_level(x, z) -> y                      -- how high the water stands: 18 over the sea, a high lake its own;
                                                 -- the ground where the column is dry, 0 with no generator
core.biome(x, z) -> name                         -- the column's climate, according to the generator:
                                                 -- core:plains/desert/tundra/peaks, or where the ground is
                                                 -- relative to the sea: core:ocean/sea/river/lake/beach,
                                                 -- or core:gorge
core.seed() -> n                                 -- the world's seed (its low 31 bits), anywhere
core.plaza() -> lo, hi | nil                     -- the plaza's bounds, nil in a world without one
core.world_spawn() -> x, y, z | nil              -- where a player arrives
core.node_liquid(id, opacity?) -> 1 | 0          -- you swim in it, and light goes through it

core.container_take(x, y, z, item, count?) -> taken  -- from a container, from the first slot

core.ecs.component(name, { "field", ... })       -- numeric fields, declared
core.ecs.phase(name, order)                      -- a point in the tick, and a number
core.ecs.system(name, phase, component, fn)      -- fn(batch), once for many
core.ecs.spawn(component, x, y, z) -> id | nil
core.ecs.set(id, field, value) -> bool           -- "x"/"y"/"z" or a mod's field
core.ecs.get(id, field) -> number | nil
core.ecs.kill(id) -> bool
core.ecs.clip(id, name) -> bool                  -- a clip of its model; "" hands it back to movement
core.ecs.box(id, half, high) -> bool             -- how big it is for a blow
core.ecs.count(component) -> how many

core.net.open() -> channel | nil                 -- at load, not after
core.net.send(player, text) -> bool              -- 0 = everybody in this world

core.render.pass(name, order, fn)                -- only on a client
core.render.box(x, y, z, w, h, d, r, g, b, a)    -- only inside a pass
core.render.model(id, x, y, z, yaw?, scale?, pitch?, roll?)   -- a model of the world, only inside a pass
core.render.fx(kind, x, y, z, size?, r?, g?, b?, a?, t?, x2?, y2?, z2?)   -- a light effect, only inside a pass
core.overcast(how_much)                          -- grey sky and clouds, 0..1: the weather (mods/weather)
core.node_foliage(id, on?)                       -- a block that takes the season's colour
core.foliage(r, g, b, how_much)                  -- the season's colour on the foliage, 0..1

core.hud.pass(name, order, fn)                   -- on the screen, once a frame
core.hud.text(x, y, text, size?, align?, r, g, b, a)
core.hud.rect(x, y, w, h, r, g, b, a)            -- only inside a hud pass
core.hud.image(name, x, y, w, h, a)              -- a PNG of the world's media
core.hud.item(name, x, y, side)                  -- an item's square, as in the bag
core.hud.mouse() -> x, y                         -- the cursor, in fractions of the screen

core.event.define(name, { fields = {...}, cancellable = bool })
core.event.on(name, priority?, function) -> ref
core.event.emit(name, { fields }) -> cancelled
```

`register_node` returns the id assigned: **a mod never sees a number it did not ask for**, and no
part of the engine knows any more what stone's id is. Registering the same name twice returns the
same id -- it is the hook hot reload rests on.

`on_step(dt)`, if defined as a global, is called once a frame; `on_save`/`on_load` serve the
reload. A mod without `on_step` pays nothing a frame but the global's lookup.

`meta_set`/`meta_get` are **what a cell remembers**: it survives closing the world, and goes when
the cell turns to air -- breaking a block takes its state with it, so the block placed after does
not inherit the one before's. Changing the *id* does not clear it: a button going from up to down
is the same cell with the same state.

**Strings, and only strings**, on purpose. A pedestal that saves id 41 is saving whatever 41 means
the next time the world opens, because ids are assigned in order of registration; a **name**
survives a mod being added. Whoever wants a number writes `tostring` and reads `tonumber`, which is
a conversion you can see. Limits: field name 1-64 bytes, value ≤ 256 bytes, 16 fields a cell, 65536
cells a world -- refused, not truncated, with the reason in the log.

`storage_set`/`storage_get` are **what a mod remembers**, not tied to a cell: strings under a name,
only the writing mod's, saved in the world file with the chunks (only the keys changed). The file
is read after the mods have loaded, so read it again from `core:world_ready` on. With no world
file, and in the client playing alone, they last as long as the process runs; the code a world
sends to the client has none (0 and ""). Limits: key 1-64 bytes, value ≤ 4096 bytes, 4 MiB of keys
and values per mod (until 2026-09-28 it was 1024 keys). `games/track` keeps every player's best lap
there (`best:<name>`) and the board (`board`).

`shared_storage_set`/`shared_storage_get` are the same, but **one for every room** of the server:
in the first one's world file (`--room`, the sandbox), which every room reads and writes. What
belongs to the player wherever they are goes there -- hunger (`hunger`), experience (`enchant`),
the keys seen (`keys`), the counter numbering backpacks (`backpack`) -- and not what belongs to a
place (a bed, a table), which stays in `storage_set`. A value held in memory must be read again
when the player comes back: in the meantime it may have changed in another room. On a server of a
single world it is the same store as `storage_set`. A backpack's contents (a carried container,
below `kCarriedBelow`, at the backpack's number) are moved by the server itself into the room the
carrier enters.

`notice` puts a sentence on a player's screen for a few seconds, where the reason for a refused
change goes. It is for whoever **changes** what the player did instead of refusing it: a mod that
replaces the block placed (`ev.id = ...`) must say so, or the player sees a block they did not have
in hand.

A handler that writes into the payload a key the event does not have -- `ev.cancelled` instead of
`ev.cancel` -- is reported in the log, once per mod, event and key.

`players`/`player_pos` are **who is here and where**. Two calls and not one: "who is here" and
"where is that one" are different questions, and a single one returning a list of positions would
answer the button and leave the pedestal empty-handed, which has to give something to a player
**with a name**. Observers -- a portal looking in -- **are not here**: they have no avatar or
inventory (DESIGN §4.4), so there is nobody to put on a button.

`player_pos` returns **the feet**, and it is the position the movement authority *accepted*, not
the last one a client claimed. For someone not in this world it returns nothing:

```lua
local x, y, z = core.player_pos(id)
if x == nil then continue end   -- they left between the two calls
```

Not `0, 0, 0`: the origin is a real place, and a mod measuring a distance there would find everyone
standing on the button in the corner of the world.

These two are also the first to use **two new return shapes** of the generator -- a list and
several values. Before M8 `core.*` was "scalars in, one scalar out", and the alternative was writing
them as five calls (`player_count`, `player_at`, `player_x/y/z`) where two do.

`take_item`/`give_item` are **a player's bag**, by name and not by id for the same reason as
`meta_*`. Both return **how many actually moved**, which is the only honest answer: a bag may not
have what is asked, and it may be full. What does not fit with `give_item` **is not dropped and not
lost**: nothing happens to it, and the mod decides -- `core.drop_item` puts it on the ground where a
broken block's stack lands, and doing nothing leaves it to the player to make room. An engine that
threw it away would decide for every mod at once. The player is told: the bag is sent to them again
the next tick, so they are not drawn holding what a mod took away.

`find_nodes` is **the cells a mod did not place**: the equivalent of the starting engine's LBM. A mod
that put down its own blocks could just remember where; this is for the ones a *player* put down,
or the mapgen. Flat triples and not a table per cell, which would be an allocation per cell on the
one call made over a whole region: `#found // 3` is how many there are.

Two limits, declared and not hidden. At most **512 positions** come back, so a search returning
exactly 512 probably found more and should be redone on a smaller box. And a box of more than **a
million cells** is **refused**: the search runs inside the tick, and the interrupt budget that stops
a runaway mod counts safepoints in the VM, not the time the engine spends on the mod's behalf. A
**name nobody registered** is refused too instead of answering "there are none": a typo that
answers zero is a mod that does nothing and a report saying the search is broken. Both refusals
return an **empty list** -- a list has no room for a sentence -- and the reason goes in the log.

### Entities the mod owns: `core.ecs`

```lua
core.ecs.component("turret:pellet", { "vx", "vy", "vz", "age" })
core.ecs.phase("turret:motion", 100)
core.ecs.phase("turret:impact", 200)

core.ecs.system("turret:fly", "turret:motion", "turret:pellet", function(b)
    for i = 1, b.n do
        b.x[i] += b.vx[i] * b.dt
        b.age[i] += b.dt
    end
end)

local e = core.ecs.spawn("turret:pellet", 32.5, 22.0, 32.5)
core.ecs.set(e, "vx", 0.4)
```

The batch `b` a system receives has `b.n` (how many entities), `b.dt`, `b.tick`, `b.id` and **a
column per field**: `b.x`, `b.y`, `b.z` and the component's fields. They are arrays, indexed from
1, and **what is written into them is read back**: a system that moved something has moved it.

Three things this does and a Lua table walked in `on_step` does not:

- **they are the engine's entities.** A pellet is an entity in the world where mobs and stacks on
  the ground live. A second store would be two id spaces for one thing.
- **one call for many entities.** 512 entities, one call. A call per entity spends more time
  crossing the boundary than inside.
- **phases ordered by a number.** Two mods that have never met run in a declared order instead of
  the order they loaded in. Leave the numbers wide on purpose: whoever wants to run between two
  must not have to ask anyone to renumber.

Limits, declared: a system names **one** component and gets whoever has it -- there is no join
between two, and whoever wants a second reads it by id, which is a call per entity and gives back
exactly what the batch was there to avoid. Fields are **numbers**; strings live on a cell
(`meta_set`) or inside the mod. A batch is at most **4096** entities, so a mod with more gets more
calls. And entities live in the world that owns the session table: an offline server (`--ticks`
without `--listen`) has none.

A phase declared twice with **two different numbers** is an error naming both mods: keeping the
first silently would put one of the two in an order it did not ask for without ever telling it.

### A channel per mod: `core.net`

```lua
local channel = core.net.open()         -- while the mod loads, not after
core.event.on("core:net_message", function(ev)
    core.net.send(ev.player, `received: {ev.text}`)
end)
```

The message **does not carry the mod's name**: the name is the **channel**, which is what the
handshake handed out. A name in the payload would be a second answer to "whose is this".

Asked for at **load**, because the numbers go out in the welcome: a mod asking after the first
client connected would have a number that client was never told, and `core.net.open` in that case
answers nothing and writes it in the log. A message is a string of at most **512 bytes**.

On the client side the number is **the server's**: `core.net.open` answers nothing until there is a
connection, and a client mod does not need it -- it sends with `core.net.send` and hears with
`core:net_message`. `player` is ignored on that side, because a client talks to the server and
that is all.

### Drawing: `core.render`

```lua
core.render.pass("hologram:light", 100, function()
    core.render.box(34.5, 22.0, 32.5, 1.5, 1.5, 1.5, 255, 0, 255, 255)
end)
```

A **pass** is not the `core:tick` event: a tick handler runs *between* frames, with no camera set
and nothing to draw into, and it runs once a frame -- while a portal draws this world again from
another camera. A hologram drawn from a tick would be missing through every opening. A pass runs
inside the render, **once per camera**, and drawing is the only thing it can do: `core.render.box`
outside a pass is refused and says so.

`core.render.model(id, x, y, z, yaw, scale)` draws at the same point of the frame a model of the
world (an id from `core.model_id`), still in its rest pose, feet at `x, y, z` and turned by `yaw`
about the vertical (and first, if given, tilted by `pitch` about its x and `roll` about its z: a
card held in hand, `mods/cards`): something only this machine knows the place of, like the cube in
a player's hands in `games/chambers` (which the server does not send them, `core.mob_unseen`).
Returns false for a model this client does not have.

`core.render.fx(kind, x, y, z, size, r, g, b, a, t)` draws a **light effect** with the engine's
shaders (`src/client/render/fx_render.cpp`): it adds to what is behind, walls cover it and it does
not cover other effects. `kind` is `"glow"`, `"flame"` (a rising flame), `"ring"` (a shock wave
laid on the ground), `"frost"` (ice shards on the ground), `"bolt"` (lightning from `x, y, z` to
`x2, y2, z2`), `"slash"` (a sword stroke's arc), `"sparks"` (sparks flying off), `"vortex"` or
`"jet"` (a flame leaving `x, y, z` towards `x2, y2, z2`, the karts' nitro). `size` is the radius in
nodes (for the bolt its width), `t` how much of its life has passed, 0 to 1: the shader grows,
sweeps and fades with it, and the mod only counts time. Returns false for a kind that does not
exist.

The number orders passes **among themselves**, not against the engine's drawing: there is a single
point in the frame where mods draw, after the opaque world and the bodies and before the openings.

Client only. On a server `core.render.pass` says "this process does not draw" instead of failing
generically.

What **is not there yet**: a `core.ecs` that can join two components. A mod's entities visible to
other players are there: `core.ecs.visible`, below.

### A block that wants a tool

`register_node(..., dig_level = 2)` declares a block that **does not come out** unless at least a
level-2 tool is in hand. Not "comes out more slowly": it does not come out, however long you hold.
`register_item(..., dig_level = 3)` declares the tool on the other side.

```lua
local HARD  = core.register_node("mymod:hard", 90, 90, 110, true, 0, false, 2)
local POINT = core.register_item("mymod:point", 1, "", 3)
```

Zero is the **bare hand**, and it is what holds for every block and item that says nothing -- that
is, everything that existed before levels did.

It is a **level, not a kind**: a level-2 axe digs stone like a level-2 pickaxe. Groups are another,
bigger thing; the level is the half that makes a progression.

And it is separate from `breakable`, which is a different no: the world's floor is not a very hard
block, it is the floor, and it does not come out for any tool that could ever exist. That is why the
refusal for the tool says what is needed -- "that needs a level 2 tool, and this one is level 0" --
and the floor's does not: sending someone to look for a better tool for bedrock is sending them to
look for something that does not exist.

### A block that takes time

`register_node(..., dig_level, hardness)` says how many **seconds** it takes by bare hand;
`register_item(..., dig_level, dig_speed)` says how many times faster a tool digs. Zero seconds is
instant, and it is what holds for every block that says nothing.

```lua
core.register_node("mymod:granite", 120, 120, 130, true, 0, false, 2, 4.0)
core.register_item("mymod:point", 1, "", 3, 4.0)   -- four times faster
```

Whoever digs holds the button. **The clock is the server's**: the first hit on a cell starts the
dig, the one after the seconds breaks it, and nothing happens in between -- two messages for a dig,
not sixty, and a client that hurries sends the second hit too early and gets nothing. Moving the aim
or letting go starts again from scratch, and so does a block changing underneath.

A bar appears under the crosshair while the dig goes. At zero nothing is drawn, so an instant block
never makes it appear.

**The engine's blocks declare their seconds**: stone 1.5, frame wood 0.9, glass 0.35, and dirt,
sand and snow under half a second. None of them asks for a tool -- `dig_level` stays 0 on all,
because tools are content and live in a mod, and an engine block that demanded one would be a block
nobody can dig with mods off. Bedrock has no hardness at all: a duration is not a refusal, and a
floor made impassable by being very slow is a floor someone drills through in an afternoon.

### A tool is for one kind of work

`register_node(..., material)` says what the block is **made of**; `register_item(..., digs)` says
what the tool **is for**. Level and speed hold against that material and are a bare hand's against
everything else -- and that is all that makes an axe different from a pickaxe.

```lua
core.register_node("mymod:granite", 120, 120, 130, true, 0, false, 1, 4.0, "stone")
core.register_item("mymod:pickaxe", 1, "", 1, 5.0, "stone")
core.register_item("mymod:shovel",  1, "", 1, 4.0, "soil")
```

The shovel above is **better than the pickaxe in every number it has** -- same level, and through
soil it goes four times faster than a hand -- and against granite it is worth nothing: against
`stone` it is level 0 and speed 1. A level says how good a tool is, a material says **at what**,
and without the second every tool is the same tool with a different number on it.

`register_item(..., damage)`, after `digs`, says how many points a blow given with that item in hand
takes off: 0 is a fist's (1). The blow carries the hotbar slot and the server reads the number from
**its** table, so a client cannot claim to hit harder.

```lua
core.register_item("tools:sword", 1, "", 0, 1.0, "", 4)   -- 4 of a body's 20
```

Empty on either side means "nobody said", and then the rule does not apply: a tool that names no
material is general (its level holds everywhere) and a block that does not say what it is made of is
dug by any tool for what it is worth. Those are the two ways of keeping quiet, and it is what every
block and item registered before materials existed is.

The refusal says so: "that needs a level 1 tool for stone, and this one is level 0 against stone".
The number alone would sound like a lie to someone holding a level-3 axe: the axe **is** level 3,
what it is not is a tool for this.

The engine's blocks use four names -- `stone`, `soil`, `wood`, `glass` -- and not one per block: a
material for every block is a table where every tool is its own tool, a list and not a rule.

### A block that is not a cube

`core.node_box(id, x0, y0, z0, x1, y1, z1, piece)` adds a box to a block's shape. One that declares
at least one **stops being a cube**: the mesher leaves its cell alone and the boxes are drawn by a
separate pass.

```lua
local CHEST = core.register_node("chest:box", 150, 110, 60, true, 4, true)
core.node_box(CHEST, 0.00, 0.00, 0.00, 1.00, 0.62, 1.00)
core.node_box(CHEST, 0.03, 0.62, 0.03, 0.97, 0.88, 0.97, "lid")
```

The coordinates are in **node space**: 0 is one wall of the cell, 1 the opposite one.

The boxes **are** the block, not a drawing of it: a body is stopped by them and not by the cell, a
ray finds them and goes through the rest of the cell, and the outline the player sees is drawn round
them. A block half a cell high is walked on half a node off the ground and you walk in the other
half of its cell. `mods/steps` is the piece of gameplay that puts it in hand: a slab and a stair.

You get on it **by walking**, up to 0.55 of a node: that is what makes a stair a stair. Higher you
jump, and a whole node is always a jump -- a cube stays a wall.

The shape **travels**: since protocol 27 the welcome carries it beside the block's name, so even a
client without your mod sees the right boxes and walks on them like one that has it. The server's
always wins, even on a name the client already knows.

### A piece of track: `core.node_piece`

Pieces of road as in stunt racing games: a smooth road on an 8-node grid, which snaps to the grid
when placed and joins the piece beside where the edges match. The block is the piece's corner; the
other cells are a "fill" block (boxes under the road, walked on, never drawn).

```lua
local FILL = core.register_node("track:fill", 60, 62, 70)
local BEND = core.register_node("track:banked_bend", 64, 66, 76, true, 0, "upright")
-- plan 2 = right bend, radius 2 squares, left bank 4 nodes higher
core.node_piece(BEND, FILL, 2, 2, 0, 0, 0, 4, 4, true)
core.place_piece(96, 20, 88, BEND, 2)   -- entry in the square of (96, 88), towards +z
```

The kart runs on the real road (a mesh, the same on client and server); a foot, a ray and the
server's checks see the fill cells, always under the road. Digging any cell takes the whole piece
away; aiming at it shows the outline of the whole piece, and with a piece in hand the piece itself,
see-through, where it will go (red if it would go over another piece or cover the player). A piece
takes the place of terrain and blocks it finds in its cells and empties the 3 cells over the road;
only another piece of track stops it; Z and X turn it a quarter before placing. In the bag a piece's
icon is its plan seen from above. `plan` 3 and 4 are a chicane, right and left. With `wall = true`
(last argument, straights only) bank0 and bank1 are degrees of roll of the road round its centre: at
90 it becomes a wall, and a kart stays on it as long as it goes fast (from 10 m/s, fully from 16).
`plan` 5 is a left bend; `width = 2` (last argument) makes the road two squares wide, and a wide
piece is placed from the square of its right lane. `plan` 6 and 7 are a loop (tight or wide, 2 or 3
squares, no kerbs): the road climbs, passes over upside down and comes down on the lane beside, to
the left (6) or right (7); the kart sticks to it as on a wall, and slow it falls from the top.
Unrolled, a loop is a chicane: the shift of a lane is steered. `plan` 8 and 9 (`width = 2` only)
narrow the wide road to its right or left lane in two squares; placed reversed they widen it. A
descent is a climb placed reversed. Complete example: `games/track/mods/track_core` (every piece and
the circuits it builds, `/pezzi` to have them in the bag, with the start line and checkpoint tools
`track_game` reads).

On the track (`games/track`) `/pezzi` first gives three tools, read by `track_game`:

- **start line**: right click on a road or a piece puts a start line across, as wide as the road (12
  a side at most), crossed going away from whoever clicked; white beam overhead. A new line is a new
  track with its own board (`customN`). Clicked again, it removes it with the whole track.
- **checkpoint**: the next checkpoint, yellow beam; clicked again, removes it.
- **offroad**: two clicks are two opposite corners of an area (256 a side at most) where during a
  lap the kart may be on anything; a glowing post at each corner, a click inside removes it.

And the **editor** (`track_core:editor`, from `track_core`): right click on a piece follows its
track backwards to the start and forwards to the open end, and opens a panel with the pieces that
attach there -- same width, same slope and same bank at the entry as the end leaves (the half a climb
is missing, the exit from a banked bend...), also placed reversed ("rev"). A click puts it down there
(`core.place_piece`) and the panel is redone from the new end; **undo** removes the last one placed
from the panel (`core.take_piece`); **close circuit** searches (A* on the grid squares, direction and
height) the pieces that lead from the open end back to the start -- straights, bends, chicanes and
`rise` both ways, never where something is, flat first and with climbs only if that is not enough,
and with adapters if the two ends have different widths -- and puts them down. Held in the bag, every
piece placed by hand opens the panel by itself. **B** with the editor in hand (or the panel's **pick
from afar** button, since with the free cursor it does not hear keys) picks **from afar**, with no
distance limit: the body stays where it is (`core.fixed_view`), the camera looks from where the start
of the piece on trial is, already put down; A and D remove it and try the next one that attaches
(skipping those that do not fit), **W** keeps it and tries the first from the new end, S removes the
last kept, F closes the circuit, B leaves removing the one on trial. In place of the hotbar there is a
grid with every piece (`core.hud.item`): lit those that attach to the end, dark the others, in yellow
the one on trial; a left click on a piece puts it on trial (clicked again, the other way round if it
fits both ways), outside the grid it does nothing. The keys are heard by `track_game`'s client half
and passed to `track_core` with the `track_core:pick` event; the view comes back with
`track_core:pick_view`. At every piece the track is found again with `core.piece_entry`, which says
from where you enter the piece of any cell.

A driving body (`core.body_drive`) has nitro: Shift with the throttle pushes it up to 1.35 times its
`sprint`, using up in 3 s a reserve that recharges in 20; the engine computes it the same on client
and server.

Outside a lap the kart goes where it likes; during a lap, off road, pieces, paddock and areas for
half a second, it is put back on the nearest road or flat piece. The paddock and the tracks' accesses
are packed dirt (`track_core:paddock`), allowed in a lap like the road. Lines and areas are in the
mod's save (`core.storage_set`).

### A block drawn as a model

`core.node_model(id, model)` has the block drawn as a **glTF** instead of a cube or boxes: it is the
same model a body wears, declared with `core.register_model`.

```lua
local M = core.register_model("steps:marker", "marker.gltf")
local B = core.register_node("steps:marker", 200, 170, 90, true, 0, "upright")
core.node_model(B, M)
core.node_box(B, 0.15, 0.0, 0.15, 0.85, 0.6, 0.85)   -- where the body stands
```

**The model is the figure, the boxes are the block.** Nobody asks a mesh where a body can stand or
what a ray found: a mesh is a thousand triangles, a collision is four boxes. A block that wants a
shape under the model declares both, like `collision_box` beside `drawtype = mesh` in the starting
engine.

The model is turned about the **cell's centre** by the facing byte, like a box, so a model drawn
inside the unit cube with its feet at 0 stands where the block is and faces the way it was placed. A
client without the file draws the boxes, and if there are none draws nothing: never a cube, because a
cube is the one thing that block is not.

Declared limits: a model block takes the bodies' directional light and not its cell's, it is not
animated and it is not cut by a portal.

What a shape does **not** change: a dropped item rests on the cell and not on the box. And what does
**not** travel with it: if the block is solid, its hardness, level and material stay the server's --
whoever lacks the mod finds them out when the server refuses. TODO.md OPEN-43.

The piece's name is not decorative: **a box with a name moves**. It turns upwards about its own
lower back edge while someone has that cell open, and comes back down when the last one looking in
closes it. The engine does not learn what a lid is: it only asks whether that box has a name.

Containers decide who opens and who closes: `core.container_show` puts the cell among the open ones,
and leaving the panel, walking too far or disconnecting puts it back among the closed. **It is the
CELL that is open, not the session** -- a lid only the one who clicked could see is not an
animation, it is a local effect.

**One bit** travels on the wire, when it changes: the movement between closed and open is drawn by
every client on its own, over a quarter of a second, at its own frame rate. A client entering while a
chest is already open does not know until it changes -- written in TODO OPEN-32 instead of hidden.

Three consequences, and none is a detail:

- **It stops hiding its neighbours.** A box filling six tenths of the cell covers nothing, so the
  block is no longer opaque whatever it had said. One truth only: a mod that forgot to turn opacity
  off would find a hole in the floor behind the chest.
- **The turn turns the SHAPE.** A cube is six squares on the cell's walls, and turning it can only
  mean moving the pictures among those six; a box has a shape, and turns with it. The face a box looks
  towards after the turn is the one whose picture it carries and the one that shades it.
- **Collisions stay the cube's.** The shape is how the block looks; physics has not been told. A
  chest six tenths high is walked on like a whole block.

It does not go into the chunk mesh because it would not fit: there positions are **bytes, one unit a
node, 0..32 inclusive**, and eighths would want 0..256, one value more than a byte holds. The visible
boxes are gathered instead into a single buffer, rebuilt every frame and drawn in a single call, with
**the same shader and the same atlas** as the cubes -- so light, per-face shading and the day cycle
are those, and not a second copy that drifts.

On a server, a client without that mod sees a **cube**: the block table on the wire is names, and
the shape does not travel yet.

### A liquid: `core.node_liquid`

```luau
local WATER = core.register_node("lakes:water", 52, 104, 178, false)
core.node_liquid(WATER, 150)      -- 0..254: how much of what is behind it hides
core.node_liquid(RIVULET, 150, 3) -- and how full the cell is, in eighths (8 = all)
```

The third argument, `fill`, is **how full a cell is** of that liquid, 1 to 8 eighths. It travels in
the block table (protocol 40), so a client without the mod draws every level at its height: a cell's
surface takes, at each corner, the height of the fullest of the liquid cells touching there (a cell
with more liquid above counts as full), and so it **slopes** from the high side to the low one and
two neighbours put the same height on the edge they share. The faces between two liquid cells are not
drawn: a lake is a surface, not a pile of see-through walls. A liquid no longer goes through the
cube mesher -- whose vertices are bytes, one unit a node -- but through cells with four heights the
renderer builds in float, as it does for boxes.

A liquid block **does not stop a body, does not stop light, is drawn see-through and is not dug**. A
body even partly inside it **swims**: the keys push it at half its pace, it sinks slowly (at most 2.5
m/s) and rises holding jump (3.5 m/s). These are numbers from `core/physics/movement.h`, the same the
server checks claims with: a body in a liquid counts as supported, so climbing out of a lake is not
flying even in a world where you do not fly. A block placed in a liquid cell **takes the place** of
the liquid.

The server tells every client that enters what every block does to a body and to light (protocol
38), so a client **without the mod** swims in the lake like one that has it -- before, a mod's
non-solid block was a wall for whoever lacked the mod.

What a liquid **does not do**: move by itself. The engine says a cell is liquid, not that it flows
-- there is no fluid simulation anywhere in `src/`. Whoever wants flowing water writes it as a mod,
and `mods/lakes` is the example: a **source** (`lakes:water`, full) and six levels of flowing water
(`lakes:flow1`..`lakes:flow6`, two to seven eighths), a block per level because a cell's level is the
block inside it; an ABM twice a second gives every flowing cell a step -- down if below there is air
or weaker flowing water, arriving at level 6 (a stream goes down a slope as far as the slope goes);
sideways one level weaker **only if something below holds it up**, into air and **into weaker
flowing water**, which it raises to its own level, and the new level carries on along that stream; it
can end up over water too, a lake's or a stream's, but there it stops, because flowing water over
other water just joins it -- and it **dries** what no longer has anything above it or anything wetter
beside it spreading. Sources are not swept: a lake of a thousand still cells costs nothing. Only two
things open the way for the water, a cell dug beside (`core:broken`) and a bucket poured.

### A block with a front

`register_node(..., rotatable = true)` declares a block that has a **front**. Placing it turns **the
cell**, not the id: the cell carries a byte saying by how many quarter turns about the vertical it is
turned, and the mesher reads that byte to decide which picture goes on which face. The id in the
world is the one `register_node` returned, always.

```lua
local OVEN = core.register_node("mymod:oven", 150, 110, 60, true, 0, true)

core.event.on("core:used", function(ev)
    if ev.id ~= OVEN then return end        -- the id is that one, however it is turned
    core.log(`oven turned by {core.get_facing(ev.x, ev.y, ev.z)}`)
end)
```

A mod can turn a cell itself with `core.set_facing(x, y, z, turn)` and read it with
`core.get_facing(x, y, z)`. The turn is 0..3; anything else is ignored. `core.set_block` says
**which** block, not how it is turned: the cell it rewrites goes back to 0, otherwise a chest built
where an oven was would face where the oven faced.

It was not always so. Until recently registering a block with a front registered **four ids** -- the
base and three quarter turns -- and the one in the world was one of the three extra, so a handler
comparing `ev.id` with the returned id did not recognise three placings out of four and the chest
opened only with an empty hand. It was a real defect, of this mod. The price was on every caller:
asking `core.node_base` before knowing what block it was. That call is gone, and so are the three
ids.

`core.find_nodes` searches a name and finds the cells whichever way they face. What went with the ids
is searching for an **orientation** -- the old `mymod:oven^2` found only that turn: when it is
needed, it will be one more argument to `find_nodes`, not four names.

The texture pack paints faces by name -- `mymod:oven:front` -- without knowing anything about turns:
see [TEXTURE_PACKS.md](TEXTURE_PACKS.md).

### A body with a face

`core.register_model(name, file)` declares a model and returns an id. The **name** is what travels
-- a body on the wire carries the id, and the world sends the table of names once in the greeting --
while the **file** is where the model is *on this machine*, beside the mod's `init.luau`:

```lua
local PERSON = core.register_model("mymod:person", "person.gltf")
core.player_model(PERSON)   -- a player's body
core.mob_model(PERSON)      -- and the creatures of --mobs
```

### Creatures

`core.register_mob` declares a **kind** of creature; the behaviour is the engine's
(`src/core/ecs/mob.h`). Two ways of fighting: `"melee"` closes in and strikes, `"archer"` stays at a
distance and shoots arrows as long as it sees the player.

```lua
local ZOMBIE = core.register_model("mymod:zombie", "zombie.gltf")
--                name            model   hp   speed fight    dmg  reach  every
core.register_mob("mymod:zombie", ZOMBIE, 20,  2.2, "melee",  3,    1.6,    1.0)
core.spawn_mob("mymod:zombie", x, y, z)   -- one, now, there
```

Creatures of every registered kind come out **in the dark** (light 7 or less) between 18 and 40
nodes from a player, up to six near each, aim for the nearest within 16 nodes and vanish when nobody
is within 72. The model plays clips by name like a player (`idle`, `walk`, `run`, `jump_loop`,
`swim`) and, while it strikes or draws its bow, `attack` or `shoot`. Server only: a local world has
no creatures. `mods/mobs` is the complete example.

A creature that dies (or moves out of range) does not vanish at once: the client leaves it where it
was and **dissolves** it over 1.2 s, playing the model's `death` clip once if it has one, still in its
last pose if not. Client side only, no extra message: the list of nearby bodies arrives whole in
every message.

**Every new creature must have its `death` clip**, in the model passed to `core.register_mob` and
in every variant it wears (tamed, shorn, young): a clip that plays once and holds the last pose,
about a second. The server checks it when the creature is registered and writes
`register_mob('...'): its model has no "death" clip` in the log if missing. The project's:

| creatures | how they die | from where |
|---|---|---|
| zombie, skeleton (`mods/mobs`), brute (`games/inferno`) | onto one side | `tools/export_mob_models.py` (the brute copies the zombie: then `tools/make_inferno_models.py`) |
| dog, cat (`mods/pets`), pig, cow, sheep, chicken (`mods/animals`), with young, shorn and tamed | onto one side | `tools/make_pet_models.py`, `tools/make_animal_models.py` |
| wraith, cinder (`games/inferno`) | the wraith leans, sinks and curls its tentacles; the cinder drops its sticks and then its head | `tools/make_inferno_creatures.py` |
| ghoul, imp, pinky, gazer (`games/yafps`) | each its own way | `tools/make_fps_models.py` |
| human, toad, mole (`mods/villagers`), helper robot (`mods/helpers`) | onto one side | `tools/make_villager_models.py` |
| bacteria, smiler (`games/backrooms`) | the bacteria staggers and falls stiff forward; the smiler shuts its grin and sinks to the ground turning | `tools/make_backrooms_creatures.py` |

In Blender the simplest is `tip_over(rig, [mesh])` from `tools/make_pet_models.py`: the creature
goes onto one side, lifted half its width so it does not sink into the floor, with any rig; `drop=`
makes it fall too, for one that flies. The clips the engine plays by name, all optional but `death`:
`idle`, `walk`, `run`, `jump_loop`, `swim`, `swim_idle`, `attack` (melee) or `shoot` (archer), `sit`
(a sitting animal), `death`; a worker the ones its mod names.

A kind with fight `"pet"` is a **domestic animal**: it attacks nobody, comes out **by day** on
sunlit ground (on `spawn_on`, if given), at most six near each player (counted apart from the night's
creatures), strolls and stops to look at whoever is within 4 nodes. `core.mob_tame` says what tames
it and what it wears after:

```lua
local DOG, DOG_TAME = core.register_model("mymod:dog", "dog.gltf"),
                      core.register_model("mymod:dog_tame", "dog_tame.gltf")
core.register_mob("mymod:dog", DOG, 16, 3.2, "pet", 0, 1.6, 1.0, "", 1, 0, false, 0, false, "core:grass")
core.mob_tame("mymod:dog", "mobs:bone", DOG_TAME)   -- the item, and the model with the collar
```

Right click with its item in hand: it uses one up and it is yours. From then it follows you (runs
if you get more than 5 nodes ahead, reappears beside you beyond 14), never vanishes, stays yours even
after a server restart (the engine saves it in the world file), and another right click makes it sit
(clip `sit`) or get up. It holds in the local world too. Clips: `idle`, `walk`, `run`, `sit`.
`mods/pets` is the complete example.

A kind with fight `"worker"` is a **worker**: it does not fight, does not come out on its own and
never vanishes; its mod spawns it (`core.spawn_mob` returns the id) and drives it. The engine walks
it and shows what it does:

```lua
local id = core.spawn_mob("mymod:farmer", x + 0.5, y, z + 0.5)
core.mob_goto(id, cx, cy, cz)          -- to that cell, by core.find_path's road
if core.mob_going(id) == 0 then        -- arrived (or stuck where there is no road)
    core.mob_face(id, wx, wz)
    core.mob_act(id, "gather", "farm:hoe")  -- looping clip and item in hand, to everybody (protocol 81)
end
core.mob_act(id, "", "")               -- idle and walk again
core.mob_place(id, x + 0.5, y + 1, z + 0.5)  -- put there, still: to climb where one does not walk
core.mob_remove(id)                    -- gone at once, without dying: the mod takes it back
```

A right click on a worker reaches the mod as `core:used_mob` (`player, target, held`); so does one
on any other creature the engine did nothing with (not shorn, fed or tamed): a cow milked, a sheep
dyed. `core.mob_kind(target)` says what creature it is. The item in hand is drawn like a player's:
the `<item>_held` model on the `hand_R` bone, or the model's own `tool_<name>` bone.
`core.node_yield(id)` says what a broken block gives, for whoever digs in a player's place.
`mods/villagers` and `mods/helpers` are the examples.

A `"pet"` without `core.mob_tame` is a **wild animal**: it comes out, grazes and leaves its drop,
but nobody tames it. Hit, it **panics**: it runs away from the blow for five seconds. `mods/animals`
(pig, cow, sheep, chicken) is the example.

```lua
-- right click with shears: 2 wool on the ground, shorn model for 300 s, and panic
core.mob_shear("mymod:sheep", "mymod:shears", "mymod:wool", 2, SHEEP_SHORN, 300)
-- an egg on the ground every 120-300 s, and one more among the drops when it dies
core.mob_lays("mymod:chicken", "mymod:egg", 120, 300)
-- fed grass it is in love for 30 s (HEARTS spin over its head): two near each other
-- make a young one (half size, its own model, stays near a parent, adult after
-- 1200 s); not again before 300 s
core.mob_breed("mymod:cow", "core:grass", COW_BABY, 1200, HEARTS)
```

Shears wear out if they have `durability` (register_item): a shearing uses one point.

Every creature's body (what it walks with, passes through gaps with and is hit on) is taken from its
model: as tall as the model, as wide as it is side to side, never more than 1.8 × 0.6. Two creatures
do not stand inside each other: they push apart until the distance between their centres is the sum
of their model half widths (these not limited to 0.3: a cow keeps 0.375).

Creatures find their way with `core.find_path`, which a mod can use for any walking body (an animal
following its owner, a villager going home):

```lua
-- from, to, and the body: 2 cells high, climbs 1, drops 3, swims; 2000 cells at most
local way = core.find_path(x1, y1, z1, x2, y2, z2, 2, 1, 3, true, 2000)
for i = 1, #way, 3 do
    local x, y, z = way[i], way[i + 1], way[i + 2]   -- the feet's cell, step by step
end
```

With no road, or the budget spent, the list leads to the **nearest** cell reached: check whether the
last one is the one you asked for.

The road reads blocks by their **shape**: a half block is a step climbed without jumping, a thin
panel (a ladder, an open door) lets you pass beside. `core.node_climbable(id)` makes a block a
**ladder**: whoever is inside climbs with jump and holds with crouch, and the road goes up and down
it. `core.node_door(closed, open)` says two blocks are **a door**: the road passes an open one and
stops before a closed one, except for a body that opens doors (`opens_doors` in `register_mob`).
`mods/doors` and `mods/timber`'s ladders are the examples.

Another mod's model is looked up by name with `core.model_id("theirmod:kart")`, as `core.node_id`
looks up a block: 0 if not there.

A file name that tries to leave the mod's folder -- with a slash, a backslash or a leading dot --
**is refused** and the call returns 0. A mod is content and content comes from wherever.

Declaring the same name twice **keeps the id**: a reload does not strip the bodies wearing it.

On the other side the client never sees that path. The world **serves** the model -- the `.gltf`
and everything it names beside itself -- by the same road as textures, and the client rebuilds it in
a folder of its own. What the world does not serve is looked for in the player's models folder
(`--models`), where `mymod:person` becomes `mymod_person.gltf`. A client with neither draws the box it
drew before -- never a hole where a player was.

A model travels **whole or not at all**: a `.gltf` without the buffer it names is not half a
character, it is a character with a hole, and whoever looks at it cannot know which file is missing.

A model made of **rigid parts** -- a vehicle with wheels, as Sketchfab exports -- is assembled from
its nodes: every mesh without a skeleton takes the transform of the node carrying it, rotations,
scales and mirrors included. A mesh with a skeleton is placed by its joints. A body's front is -Z: a
model exported nose towards +Z must be turned in the file. `games/track`'s kart, as it came out of
Sketchfab, already faces -Z.

The `mods/chest` mod is this, written in full: `chest:box` is `rotatable`, and its two handlers --
the one opening the chest and the one emptying it when it is broken -- compare `ev.id` with the id
`core.register_node` returned, and that is all. **A mod that adds a block shifts the ids of those
registered after it**: ids are positional. A world file written before keeps the names of its table,
and on opening its chunks are rewritten by name into the new one. Blocks no loaded mod registers any
more become magenta placeholders with their name (they cannot be held), and become themselves again
when the mod comes back; the server lists them at start. So an id held in a variable must be asked of
`core.register_node` at every start, never written by hand.

### Writing many cells at once

`core.set_block` writes the world and tells whoever is connected. How it tells depends on how many
cells of ONE chunk you changed in that tick:

* few -- one by one, nine bytes plus the reason, per session;
* more than 150 -- the chunk is taken out of what that session has and the streamer sends it again
  whole, computed from the world as it is now.

There is nothing to choose: the result is the same world in both cases. Measured: 640 cells of one
chunk are 640 messages against 1 chunk, and four hundred cells cost 6834 bytes as cells and 233 as a
chunk. A session the world is still arriving to receives nothing until it has arrived -- the cells
wait, they are not lost.

Lighting is not paid per cell: every place a mod writes from (a tick, an event, loading) is inside a
server `LightRun`, which redoes the light once at the end, for all the cells changed. An explosion
digging three hundred cells in a tick (`games/front`) is one recomputation, not three hundred. What
remains to pay is the call and the chunk sent again.

### Generating a part of the world: `core.register_mapgen`

A **stage** is a piece of world generation written in Lua. The engine registers one of its own --
`core:terrain` -- and a mod adds others:

```luau
local ORE = core.register_node("mine:iron", 180, 120, 90)

core.register_mapgen("mine:veins", function(ctx)
    -- ctx.seed, ctx.height, and the column to generate: ctx.min_x..ctx.max_x and
    -- ctx.min_z..ctx.max_z, inclusive, 32 by 32. There is nothing else inside,
    -- and nothing is written inside: a stage writes with core.set_block and
    -- reads the terrain with core.terrain_height.
    for z = ctx.min_z, ctx.max_z, 8 do
        for x = ctx.min_x, ctx.max_x, 8 do
            local ground = core.terrain_height(x, z)
            if ground > 12 then
                core.set_block(x, ground - 6, z, ORE)
            end
        end
    end
end)
```

**Below bedrock (y 0) there is room down to y -64** (`kWorldBottom`): the engine generates nothing
there, but a stage can build there, and those blocks are lit, saved and sent like the others; a body
dies only below y -80. `mods/deep` puts its hollows there.

**The world has no edge, and a stage runs once per column** of chunks (32×32 blocks, the whole
height), when someone comes near it for the first time. What it writes outside its column is
**discarded** without error: a structure straddling two columns is written by both, each its half,
and that is the only way it comes out whole in whatever order the two columns are generated. So a
structure starting in a neighbouring column (a vein, a tunnel, a tree's crown) must be *followed* from
here too: `mods/ore` looks at vein starts up to four columns back, `mods/cave` at the tunnels of the
squares up to four further on. And `core.get_block` outside the column reads what is there now, which
may be nothing: a stage decides from what is **inside** the column, or from functions of the seed.

Four rules, and they are what makes a stage different from a mod writing blocks at load:

* **The order of registration is the contract.** Stages run in the order they were asked for -- so
  in the mods' load order, which `depends` decides -- and a stage sees what those before it wrote.
* **You register while the mod loads, not after.** Stages run on every new column, for the server's
  whole life -- on the region round the spawn right after the mods load, on the rest when needed --
  but the list closes when the mods have finished loading: asking for one from `on_step` is
  **refused** with an error, instead of being accepted and running only on the columns generated from
  then on.
* **It must be deterministic.** Same seed, same world, on any machine. No clocks, no `math.random`
  without a seed, nothing depending on who is connected: a server and a client that disagree on what
  the terrain is are a desync before the first packet.
* **`core.set_block` inside a stage does not cost lighting.** The world is lit once at the end of
  the pass, and no chunk has to be sent again to anyone: it is the place to write tens of thousands of
  cells.

A stage that errors stops **its** mod and no other: what it had already written stays in the world,
the stages registered after run all the same, and the log says it was the stage.

`mods/ore` is this mechanism in production and not an example: it registers a stage burying coal
and iron in the stone, the iron at level 1 -- that is, behind `mods/tools`'s pickaxe. It is worth
reading for two things the example above does not show: the deterministic hash written by hand (a
double holds 53 exact bits and the product of two 32-bit numbers makes 64, so `a * b` throws away
exactly the low bits a hash needs) and the rule replacing **only** cells that are `core:stone`, which
is what keeps the stage out of the surface, the biome filler, the bedrock and what a stage registered
before built.

`mods/cave` is the second, and shows the thing mod order really decides: the mods folder loads in
name order, stages run in the order they were asked for, and `cave` comes before `ore`. So the
tunnels are dug and *then* ore fills the stone left -- and that is how a vein ends up in a cavern's
**wall** instead of inside a block nobody will ever look at. Swapping the two names changes the game
without changing a line of code: it is the contract this section speaks of, seen from outside.

The other two rules worth copying from there: one cavern per square of 32×32 columns, chosen
*inside* the square instead of "one column in N", because a small world with an unlucky hash would
be a world without caverns and a gate that passes only on some seeds; and the roof kept six blocks
under the surface, checked at every step against `core.terrain_height` of the column the tunnel is
in now and not the one it started from.

`games/backrooms` is the third, and a whole world made only of stages (`core.mapgen_core(false)`): a
grid of rooms of 8 that fits entirely inside a column of 32, so every room is written whole and no
structure has to be followed from a neighbouring column. Two things to copy. The plan is a function
of x and z and **not of the seed**, and that is why the tick can read it too: it finds where the
portals are without having seen the stage run, which in a world loaded from the file does not run.
And what crosses several levels (stairs, holes in the floor) is written **after** all the levels of
the column: `core.replace_nodes(…, AIR, floor)` of a level made later would close the hole opened
before.

A column already generated and then left (nobody has passed in a while) is not generated again: it
comes back from the world file, or from memory if the server has none. A stage changed between two
starts changes only the columns nobody has visited yet, and the border with the old ones shows. A mod
writing with `core.set_block` into a column not ready yet does not lose the write: it waits for the
column to be generated and lit, and then goes. `core.get_block` in a column that is not there answers
air. TODO.md OPEN-35 and OPEN-38 have the numbers.

### How a body moves: `core.movement`

```luau
-- walk, sprint, fly, jump, gravity
core.movement(12, 60, 20, 10, -40)
```

A world says how fast a body walks, runs, flies and jumps, and how hard it falls. It is **the
world's** and not the player's: every client that enters receives these numbers in the welcome and
moves with them, and the server judges positions with limits derived from the same numbers. So a body
at 60 m/s is not a cheat in a world that declared it, and one at 150 is.

* **While the mod loads.** From a tick it is too late and refused: the numbers go out in the
  handshake, and a world that changed them halfway would have players who were told others.
* **Refused, with the reason in the log:** a speed negative, not a number or over **256 m/s**; a
  gravity not a number or **pointing up**. Returns 1 if the numbers were taken, 0 if not.
* **Flying doubles with sprint**, as with the engine's numbers.
* **Terminal velocity, 60 m/s, is not declared**: it is what stops any fall whatever the gravity, on
  the client too.
* **If two mods call it the last loaded wins**, and the log says whose numbers it replaced.
* **A connected client uses the server's numbers**, even if one of its mods declares others: the
  server is the side that checks.

What is **not** there yet: speed per body -- a vehicle is a body that is not a player on foot -- and
a server that simulates movement instead of checking its end. TODO.md OPEN-44.

### How big a body is: `core.body_width`

```luau
core.body_width(1.4, 2.6)   -- a kart: 1.4 wide, 2.6 long
```

On foot, the side of the square box a body collides with, in nodes: 0.6 if the world says nothing.
The height stays the engine's, 1.8. The second number counts only in a world that drives
(`core.drive`): there the body is a **chassis on four wheels with suspension**, `width` wide and
`length` long (0: as long as it is wide), simulated as a rigid body (Jolt, `core/physics/vehicle.h`).
It turns with the kart, pitches under braking, leans in bends, and if it enters a bend too fast the
tyres do not hold and it slides wide. The chassis stays within 26 degrees of vertical and its wheels
take nothing steeper than 50 degrees as ground: a kart scraping a wall does not lie down on it or
climb it.

* **While the mod loads**, like `core.movement`: it goes out in the same welcome, and
  `core.movement` and `core.body_movement` keep it.
* **The server checks with a square box**: `width` wide on foot; for a driving body, the square that
  fits inside the chassis however it is turned, `width / 1.41`. Whoever comes out of a portal is
  pushed off the wall beside by half a width, or half a length if driving.
* **Being INSIDE a block** is the one question it asks differently: on foot it asks with that box,
  driving **with the centre only**. A chassis turns, leans and has its wheels outside its box, so the
  two ends do not agree on what it touches, and a refusal putting the kart back where the server's box
  is free leaves the chassis inside the wall it was scraping -- and the server refuses it again, at
  every claim (TODO.md). A kart can lean on a wall; one with its centre inside is still put back.
* **Refused:** a width under 0.2 or over 4, a length other than 0 under 0.2 or over 8, or a number
  that is not a number.
* **A body wider than a portal in the floor does not pass through**: the server exempts only the
  opening's node, and the wide box sinks into the nodes beside as well.

**Whoever does not fly does not rise.** In a world that drives (`core.drive`), or that declares fly
0, the server refuses a body that rests on nothing and is higher than where it last rested by more
than a jump (v² / 2g, plus 0.3). A jump, a step and a landing on a higher block pass; a climb at jump
speed over the track's two-node walls does not.

### A body different from the others: `core.body_movement`

```luau
core.event.on("core:net_message", function(ev)
    if ev.text == "get in the car" then
        -- player, walk, sprint, fly, jump, gravity
        core.body_movement(ev.player, 12, 150, 20, 10, -40)
    end
end)
```

`core.movement` says how the world's bodies move; this says how **one** body moves, and can be
called whenever you like: a vehicle, a power-up, a slowed prisoner. The player receives the numbers in
a message of their own, and from then the server checks that body with them.

* **A second of slack.** The client keeps moving with the old numbers until the message arrives, so
  for a second the server accepts the faster of old and new: slowing someone does not correct the
  steps they had already taken. The price, stated: a slowed body has a second in which it can still
  go at the old speed.
* **The defaults are the engine's numbers, not the world's.** To put a body back as the world
  declared it, pass those.
* **Refused, with the reason in the log:** the numbers `core.movement` refuses, a player not in the
  world, and someone only watching.
* **How high, the seventh number (`rise`).** Without it, a body that does not fly can be at most the
  top of its jump above where it rested (for a kart, plus the chassis's lift off the wheels). With it,
  what the world says, up to 64: the track passes 10, and a kart getting there bounces as against a
  ceiling. Who must stay inside a course is decided by the mod, not by height: `games/track` puts back
  on the nearest road a kart that has been off it for half a second (`core.player_place`).

What is **not** there yet: the server does not check steering and inertia. And a body's numbers do
not cross a portal to another world, where that world's welcome holds, and are not saved. TODO.md
OPEN-47.

### Only one gets in the kart: `core.body_drive` and `core.body_model`

```luau
core.event.on("core:used", function(ev)
    if ev.id ~= KART then return end
    ev.cancel = true
    core.set_block(ev.x, ev.y, ev.z, 0)                       -- the kart is now them
    core.body_drive(ev.player, 12, 30, 2.6, 0.6, 1.4, 2.6)    -- accel, brake, steer, grip, wide, long
    core.body_model(ev.player, KART_MODEL)                    -- and they look like this
end)
```

A cart is `core.body_cart(player, true)`: a block declared a rail
(`core.node_conveyor(id, speed, height, rise, true)`) does not drag whoever walks on it, but whoever
is in a cart runs along it like a chute, flat, uphill or downhill, turning where a rail at the same
level comes in from the side; `speed` is what it carries the cart at, 0 a brake (forward held starts
again). `mods/carts` and `mods/building`'s rails.

A boat is `core.body_boat(player, true)`: on a liquid the body floats with its feet 0.35 under the
surface, W/S row along the bow (forward up to `sprint`), A/D steer and the keel takes away the
sideways drift; out of the water it walks. The hull and the pose are the mod's (`core.player_wear`,
`core.player_clip(p, "drive_idle")`): `mods/boats`.

`core.drive` and `core.body_width` say how **the world** drives, and must be called while the mods
load; `core.body_drive` says how **a body** drives, whenever you like, and `accel` at 0 puts it back
on foot (passing the previous width too). `core.eye_height(h)` says how high the eye is above the
feet (1.62 if the world says nothing): the first-person camera and the point the hand's reach is
measured from, on client and server. It is called while the mods load, like `core.movement`, and
goes in the welcome (protocol 50). From 0.2 to 1.8: the body's box stays 1.8 high. `mods/robot` says
1.5, the robot's visor.

`core.parkour(true)` says that in this world bodies on foot do parkour (`core/physics/player.cpp`),
and goes in the welcome like the rest (protocol 87):

* **wall run**: in the air near the top of the jump, forward held at 4 m/s or more, with a wall
  beside (right or left of the direction you are going), you run along it flat, without gravity, for
  at most 5 nodes;
* **wall jump**: jump pressed in the air with a wall within 0.45 pushes off the wall, at 5.5 m/s and
  up with the world's jump; never twice in a row off the same side, so between two walls 2 apart you
  climb bouncing and on a single one you do not;
* **slide**: crouch pressed while running (faster than the pace) puts the body on its back, 0.8
  high, at 1.15 times sprint slowing down, for 0.9 s: it passes under a block 1 high; under a low
  ceiling it stays down and goes at crouching pace;
* **backflip**: jump with back held, from the ground, rises 2.2 nodes going backwards at 3 m/s: onto
  a step 2 high behind you;
* **rope**: `core.rope(true)`, from the world's code on the client, fires the rope from the eye up to
  40 nodes; the body swings round the point it hooks (the hands stay a rope's length away), the keys
  push the swing, crouch lengthens, sprint shortens, jump or `core.rope(false)` lets go.

In such a world hands do **not** grab ledges (there a wall 2 high is a backflip and a tall one is a
chimney), and the server allows what these moves do: 20 m/s of swing, any height, a body slid under a
block (`limits_for`). The robot has the clips for each: `wall_run_left`, `wall_run_right`,
`wall_jump`, `power_slide`, `backflip`, `rope_swing`. `games/parkour` is the world made for it.

`core.body_model` is `core.player_model` for one body. That is how you make a world where you arrive
on foot and get into a parked kart: `games/track`.

* **The speed numbers stay its own.** `core.body_drive` changes what the body *is*, not how fast it
  goes: `sprint` is top speed and `walk` reverse speed, and they change with `core.body_movement`,
  which in turn leaves the kart alone (a countdown at zero makes nobody get out).
* **Getting in lifts the body by 1.07** -- a chassis stands on its springs -- so a stop that also
  zeroes the jump gets corrected by the server: leave the world's jump.
* **What the player sees of their own body** with the camera behind (`core.view`) is
  `core.player_model`'s model: the welcome carries only one. Others see `core.body_model`'s. The
  world's code running on the client can call `core.body_model(0, id)` to change how **that machine**
  draws its own body (0 puts it back): `games/track` puts the kart there while driving.
* **Wheels that turn and steer.** A model with a skeleton animates them by itself, by bone name:
  `steer_*` steers about its own axis, `wheel_*` rolls about its own (child of a `steer_*`, steers
  too), `steering_wheel` turns a quarter turn at full lock. "Its own axis" is the bone's Y, that is
  where a Blender bone points. A wheel's radius is how high its centre is above the model's bottom.
  The client reads steering and metres travelled from how the body moves: nothing to declare, and it
  holds for other players too. A block wearing the model stays still.
* **Clips by name.** A model with animations plays one chosen from the body's movement, with a fifth
  of a second's crossfade between them: `idle`, `walk` (up to 5.5 m/s), `run`, `jump_loop` (in the
  air), `swim`/`swim_idle` (in a liquid), `slide` (on a downhill conveyor from 3 m/s up: a chute)
  and, for your own body, `fly`/`fly_hover` and while digging `mine`, `dig` or `chop` according to the
  block's material (`stone`, `soil`, `wood`). A clip the model lacks leaves the previous one.
* **Who drives.** In a body wearing a model with wheels (`wheel_*`) the client seats
  `core.player_model`'s model, at the same origin: it plays `kart_enter` once, then `drive_idle` bent
  towards `drive_left` or `drive_right` as far as the steering is turned. The driver's clips must
  therefore be made round the vehicle's origin. `mods/robot` and `games/track`'s kart are made so.
* **Vehicle parts.** A piece a mod has the driver wear (`core.player_wear`) named like the vehicle
  plus `_<part>_<variant>` (`track_core:kart_wheel_slick` on `track_core:kart`) goes on the vehicle's
  bones, not the rider's, and the vehicle bones named `<part>` or `<part>_*` are not drawn: the piece
  replaces them (the four `wheel_*`). The vehicle itself worn as a piece paints it: its tint colours
  the vehicle's `tint_1..3`. The nitro flames come out of the `flame_*` joints (along the bone) of the
  pieces that have them, otherwise of the vehicle's; the skid marks from the `wheel_R*` bones, on the
  road under them (on a track piece, its surface).
* **Preview in a panel.** `core.ui_model(panel, x, y, w, h, model, tint, pieces)` puts in a box of
  the panel a slowly turning model, framed by its size, with its colours and the pieces (ids separated
  by spaces) on it as above: `games/track`'s garage shows the chosen kart this way. It does not answer
  clicks (protocol 104). `tools/make_kart_parts.py` makes the kart split into parts (`nose`, `pods`,
  `pipes`) and its 14 pieces.
* **First-person arms.** A model registered with the player model's name plus `_arms`
  (`robot:robot` -> `robot:robot_arms`) is drawn in front of the eye when the view is first person.
  It is made in eye space: origin at the eye, forward -Z, up +Y. It plays `swing` while the left
  button is down and `idle` otherwise, and sways while walking. A block in hand is a cube of its faces
  in the palm of the `hand_R` bone; a track piece (`core.node_piece`) is the piece in miniature, its
  road's longer side 0.24 forearms; on the ground, the same piece in miniature. It holds in the local
  world too.
* **Glass.** A material with base colour alpha under 1 (`baseColorFactor`, in Blender the Principled
  BSDF's Alpha) is see-through: it is drawn after the rest of the model, blended and without writing
  depth (the front's vehicles' windows, 0.3). Texture alpha instead cuts out: under 0.5 the pixel is
  not there.
* **The player's colours and accessories.** The `tint_1`, `tint_2`, `tint_3` materials of the
  player's model (and its `_arms`) take the primary, secondary and accent chosen in the Robot menu
  (protocol 93, `Hello::livery`); with no choice they keep the file's colour. Accessories are models
  named after the player plus `_cap`, `_tophat`, `_headphones`, `_scarf`, `_bowtie`, `_medal`
  (`robot:robot_cap`), made on the player's skeleton and drawn as a worn piece; a world that does not
  register them simply does not show them. `tools/make_robot_extras.py` makes them. The species
  (protocol 94, two bits above the accessories: human, mole man, frog man) replaces the player's model
  with the one named after it plus `_human`, `_mole`, `_frog` (`robot:robot_mole`, with its `_arms`),
  on the same skeleton and with the same clips; the chosen style is a worn piece named
  `<species>_<style>` (`robot:robot_mole_lamp`). Accessories are the ones remade for the species
  (`robot:robot_mole_cap`) if the world registers them, otherwise the player's; a head accessory
  replaces the styles that are hats (lamp, crown, lily pad). A world that does not register the
  species shows the robot. `tools/make_species.py` makes them all: every body part (head, torso, arm,
  leg) is a single surface, primitives fused with a voxel remesh, smoothed and decimated, with weights
  blended at the joints; eyes and small details stay sharp. A `sway_*` bone swings only forwards, away
  from the chest. In a worn piece, a bone the player lacks follows its parent, and if named `sway_*`
  dangles too round where it is attached.
* **Tools in hand.** A `tool_<name>` bone shows only when the item in hand is named `<mod>:<name>`
  (`tool_pick` for `tools:pick`), and the other `tool_*` vanish: on the arms and on your own body in
  third person, and on other players (since protocol 48 the server sends everyone what each holds).
* **Items in hand.** If the world has a model named like the item plus `_held`
  (`core.register_model("ore:diamond_held", "diamond_held.gltf")`), whoever holds that item has it in
  the palm of `hand_R` in place of the cube: a rigid glTF, no skeleton, made round the grip (origin at
  the centre of the fist, +Y where a tool points, that is up in first person, +Z towards the eye in
  first person: a flat thing lies in the XY plane), in metres of the 1.357 robot and scaled with the
  forearm like the block. It holds on the arms, on your own body and on other players.
  `tools/make_held_models.py` makes the 41 of the project's mods and the icons of those that had
  none.
* **Worn armour.** An item with `wear` (`core.register_item`) goes in one of the bag's four worn
  slots (head, chest, legs, feet; shift-click with the bag open puts it on and takes it off). If the
  world has a model with the item's name (`core.register_model("armor:iron_helmet", ...)`), the body
  wears it: a glTF skinned on bones named like the player's, drawn with its pose. Since protocol 58
  the server sends everyone what each wears. `tools/make_armor_models.py` makes `mods/armor`'s sixteen
  on the robot's rig.
* **Crouching.** Ctrl held on foot (in flight it stays "go down"): pace at 0.35 of `walk`, no
  sprint, the eye at 0.8 of its height. The body plays `crouch_idle` or `crouch_walk`, and since
  protocol 51 others see it too.
* **Who works.** With the left button down the body plays its tool's clip: `mine` with the pickaxe,
  `dig` with the shovel, `chop` with the axe, `sword_slash` with the sword; bare-handed the one for
  the block's material. Since protocol 49 others see it too.

### An entity everybody sees: `core.ecs.visible`

```luau
local sign = core.ecs.spawn("mymod:sign", 32.5, 22.5, 30.5)
core.ecs.visible(sign, true)    -- from here every nearby player receives it
core.ecs.visible(sign, false)   -- and from here no more
```

A mod's entity is in the server's world and that is all, until the mod asks for others to see it.
From then it is sent to every nearby player by the same road that sends people, creatures and items
on the ground, **as a thing**: the client draws it as a box of its own, not as a person or a
creature, because the engine does not know what it is.

* **Only a mod's entities.** A player, a creature or an item on the ground already has a body the
  engine gave it, and `visible` refuses them both ways: a mod cannot hide a player.
* **Hiding it is not instant for whoever was looking**: a client forgets a body it no longer
  receives after five seconds of silence.
* **A model**: `core.ecs.model(id, core.register_model(...))` draws it with the model instead of the
  box (0 goes back to the box), feet at its y. Only on an entity made visible by `visible`.
* **Health**: `core.ecs.health(id, 3)` gives it 3, and from then a player's blow takes off as much as
  it takes off a creature; `core.ecs.health(id)` reads it. At 0 it stays where it is: what dying
  means the mod decides.
* **A clip**: `core.ecs.clip(id, "shoot")` plays that clip of its model in place of the one the
  engine picks from movement (idle, walk, run by speed); `core.ecs.clip(id, "")` gives it back to
  movement. `"death"` goes once and holds the last pose: call it, wait its length, then
  `core.ecs.kill`. It travels to clients (protocol 88).
* **How big for a blow**: `core.ecs.box(id, 1.1, 3.2)` -- half width round the centre, height from
  the feet up. Without it, a blow finds it only in a little 0.6 cube at its feet.
* **Where it looks**: `core.ecs.set(id, "yaw", a)` turns it like a player's gaze (0 looks towards
  -z); `core.ecs.get(id, "yaw")` reads it. "yaw" can no longer be the name of a component field.
* **It collides** with nothing. TODO.md OPEN-20.

### Vehicles: `core.craft`

A mod's entity made visible can become a **vehicle**: a jeep, a tank, a helicopter, a plane. The
engine moves it with Jolt (the same as the kart, `core/physics/craft.h`), server only, a fixed step at
60 Hz; players get on as passengers of a body (`core.ride`) and whoever drives is whoever the mod
listens to.

```luau
local id = core.ecs.spawn("front:vehicle", x, y, z)  -- the feet: the model's bottom
core.ecs.visible(id, true)
core.ecs.model(id, TANK)
core.ecs.set(id, "yaw", 0)                            -- before make: where it looks
core.craft.make(id, "tracked", 3000, 1.6, 0.6, 3.2, 4, 9, 0.9)
--               kind, kg, half width, half height, half length, m/s², m/s, rad/s
```

* **`core.craft.make(id, kind, mass, half_w, half_h, half_l, accel, top, turn)`**: `kind` is
  `"wheeled"` (four-wheel drive, the front two steer), `"tracked"` (two tracks, turns by slowing one,
  from still on the spot), `"rotor"` (pushed along its up, tilted towards where it must go) or
  `"wing"` (pushed along the nose, held up by how fast it goes: under 45% of `top` it stalls). The
  sizes are the hull in nodes. Made again, it starts from still.
* **`core.craft.keys(id, forward, right, up, yaw)`**: what the driver asks, -1 to 1, held until told
  otherwise. Wheels and tracks: throttle and steering. Rotor: `forward` and `right` tilt it, `up` is
  the collective (0 hovers), `yaw` turns it. Wing: `up` moves the throttle (and it stays where it was
  left), `forward` is the stick forward (nose down), `right` the roll, `yaw` the rudder. The player
  presses keys on their client: the mod sends them with `core.net` and passes them here.
* **`core.craft.pose(id)`**: a table `x y z yaw pitch roll vx vy vz speed grounded impact throttle`.
  `speed` is along the nose, `impact` the hardest blow taken since it was last asked, in m/s (a
  crash), and asking zeroes it.
* **`core.craft.push(id, vx, vy, vz)`**: added to its velocity: an explosion.
* **`core.craft.mount(id, on, x, y, z)`**: another visible entity carried on `on` at x, y, z in its
  frame (x right, y up, z forward): a turret on a hull, a barrel on the turret. `on` 0 releases it.
  Put back in place every tick after the vehicles, carrier before carried.
* **`core.craft.aim(id, yaw, pitch)`**: how far a mounted thing is turned on its carrier and raised;
  on an unmounted one, how it looks in the world (a mortar).
* **`core.craft.seat(player, id, x, y, z)`**: the player carried on that entity, at seat x, y, z in
  its frame, until `core.ride(player, 0)` gets them off or the entity vanishes. Seated, they look
  where they like (`core.player_look`) and their WASD does not move them.

Every vehicle is a Jolt world of its own with the blocks round it; the engine then separates two
overlapping vehicles (in proportion to mass) and pushes whoever is on foot out of the hull. Since
protocol 100 the hull goes to clients: a player's body does not go into it, it stands on it and is
carried with it. An entity's roll goes to clients since protocol 98.

**On the driver's client**, `make`, `keys` and `pose` run the vehicle there too, so it moves in the
frame the key is pressed and not a round trip later. `core.craft.make(id, ...)` holds only for the
vehicle the player is seated on (with `core.craft.seat`) and returns `false` until the seat has
arrived: call it again until it says `true`. From then `keys` are the ones the mod gives on the
server (the same, computed the same way) and `pose` is where the vehicle will be. The engine draws it
there, puts the player on it and carries with it what is on or in it. It compares it with the
server's poses: it corrects small gaps gently and puts it back where the server says if it strays
more than 3 nodes. Getting off the vehicle stops the prediction. The other `core.craft` on the client
do nothing. Example: `drive_here` in `games/front/mods/front_game`.

### Code a world sends to the client: `side`

```
# mod.conf
name = hud
side = both        # server | client | both; server if absent
```

```luau
if core.side() == "server" then
    core.event.on("core:net_message", function(ev) core.net.send(ev.player, "hello") end)
else
    core.render.pass("hud:crosshair", 100, function() core.render.box(0, 60, 0) end)
end
```

A mod says **where it runs**. `server` is every mod from before: it runs where the world is.
`client` the server does not run: it packs it and sends it to players as media. `both` runs in both
places, and it is what a mod needs to talk with its other half on its own channel -- a mod's channel
is its name, and the server gives it to the mods it runs. `core.side()` tells a half which it is:
`"server"`, `"client"`, or `"both"` in a process that is its own server.

On a player's machine a world's code:

* **does not write the world and registers nothing**: blocks, shapes, models, items, recipes,
  stages, modifiers, bags, chests, panels, health and movement are the server's. The function is
  there and does nothing, or gives an error saying why;
* **has no capability**, not even `os`: a mod declaring one is not loaded, and the log names it;
* **does not raise its own** memory quota or interrupt budget above the defaults;
* **dies with the session**: crossing a portal is entering another world, with its own code.

**The player can refuse it**: `--world-code refuse` for every world, or a world's signing key, in
hex, one per line in `world_code_refused.txt` in the media cache. A refused world is played all the
same: the server stays the authority, so refusing gives no advantage and only takes away what the
world wanted to show.

A world's code **draws on the screen and hears keys**: `core.hud` and `core:input`, below. What is
**not** there yet: a client-only mod talking to the server without a `both` half. TODO.md OPEN-49.

### On the screen and from the keys: `core.hud` and `core:input`

```luau
core.hud.pass("race:time", 100, function()
    core.hud.rect(0.5, 0.5, 0.026, 0.003, 223, 244, 255, 230)   -- crosshair
    core.hud.text(0.5, 0.9, `{laps} / 3`, 0.045)
end)

core.event.on("core:input", function(ev)
    if ev.action == "mouse_left" then
        ev.cancel = true            -- the click is the mod's: no digging, no blow
        core.net.send(0, "fire")
    end
end)
```

A **hud pass** is not a render pass: that runs once per camera, and a portal on the screen would draw
the time once for every world it shows. This runs **once per frame**, over the bar and under a panel.
`x, y` are fractions of the screen and are **the centre** of what is drawn (`align` 0 puts the text's
left end there, 2 the right); `w`, `h` and `size` are fractions of the **height**, so a square stays
square at any proportion. Outside a pass `text`, `rect` and `image` are refused and say so.

`core.hud.image(name, x, y, w, h, a)` draws an image of this world's media -- a PNG among the
textures of one of its mods, by file name -- centred at `x, y` and sized like `rect`; `a` is opacity,
0-255. Decoded once; a name the world does not have is said in the log once and draws nothing. The
server sends as media only the textures of **registered blocks and items**: a PNG that is only a
drawing for the HUD must be registered as an item nobody receives (`core.register_item`), and is
called by its file name. `games/track` does so with the power-up icons and the ink splat
(`track_core:item_*`, `track_core_item_boost.png`). The size of the world's other textures (32 px):
the atlas takes only one, and an image of another size is a red square in the bag.

`core.hud.item(name, x, y, side)` draws an item's square as the bag draws it (a track piece as its
plan), centre at `x, y` and side in fractions of the height; false for an item the world does not
have. `core.hud.mouse()` says where the cursor is, in fractions of the screen like `text`'s `x, y`
(nothing where there is no screen): with `core.fixed_view`'s camera the cursor is free and clicks
reach `core:input` anyway, so a mod makes a grid to click on (the track editor).

`core.hud.aspect()` is the screen's width divided by height (0 where there is no screen). It is for
putting something **aligned with the engine's bars**, whose sizes are fractions of the height:
`mods/hunger`'s hunger bar is as wide as the hotbar, and each notch's centre goes at
`0.5 + dx / aspect`. The engine's sizes are in `src/client/ui/hud.cpp` (a slot is 0.075 of the
height, the gap between two 0.0075, a bar 0.0165 high).

`core.view(distance)` puts this world's camera **behind** the player, at that distance in nodes (0
is the eye, at most 16), with their body drawn in `core.player_model`'s model. It holds for the world
asking: going back to a world that said nothing returns to the eye. The V key does the same by hand.
`games/track` calls it with 5.

`core.engine_hud_on()` says whether the engine's hotbar and bars are drawn in this world: a mod
drawing a bar beside them (`mods/hunger`, `mods/enchant`) hides it with them when a world's code has
said `core.engine_hud(false)`.

`core.fixed_view(x, y, z, distance, yaw, pitch, fov, seconds)` is `core.iso_view`'s camera held on
**a point** instead of the head: it flies from the eye there in `seconds`, the cursor is free
(`core.cursor_ray`), the body stays still (the keys do not walk) and is not seen; `distance` 0 brings
it back to the eye. It is `mods/chess`'s view over the board.

`core:input` arrives when the player **presses** and when they **let go**, while playing:
`mouse_left`, `mouse_right`, and the keys `key_w`, `key_a`, `key_s`, `key_d`, `key_space`,
`key_shift`, `key_ctrl`, `key_f`, `key_r`, `key_e`, `key_q`, `key_v`, `key_c`, `key_b`, `key_f4`,
`key_f8`, `key_i`, `key_k`, `key_1` … `key_9`; the release is the same name with `_up` (`key_w_up`,
`mouse_left_up`). With a chest open over the bag `wheel_up` and `wheel_down` arrive too, the wheel
(`mods/creative` scrolls the rows). A held key is one pressed and not yet released. A handler setting
`ev.cancel = true` on a mouse **press** has **claimed** it: the engine does not dig, strike, place or
use with that press, until the button comes back up. Keys are only heard: the engine walks, flies and
opens the bag with its own anyway (`mods/creative` opens its list with C). The exception is `key_1` …
`key_9`: claimed, they do not change the item in hand (the front uses them for vehicle seats). Client
only: the server sees no keys, it sees what a mod's client half decided to send -- so a `both` mod
listens to `core:input` only on the side that is not `"server"`.

### A shot decided by the server: `core.player_look` and `core.raycast`

```luau
local x, y, z = core.player_pos(player)
local dx, dy, dz = core.player_look(player)
local _, _, _, wall = core.raycast(x, y + 1.62, z, dx, dy, dz, 60)
```

`player_look` is where a player looks **according to the server**: the gaze that arrived with the
last accepted position. With `player_pos` it is a shot the server fires without taking aim from the
client -- a client can only lie about *when* it pressed. `raycast` returns the first solid block
along the ray and at what distance, within `max` (at most 256), or nothing. **Blocks only**.

A projectile that hits **bodies** is `core.shoot`: the first body that can be hurt (a creature, or a
mod's entity with `core.ecs.health`) before the first solid block loses `damage` and is knocked back;
it returns at what distance and whether it died. `core.blast` is an explosion: every body within `r`,
half damage at the edge, and it returns how many it killed. Neither ever touches a player.

```luau
local dist, dead = core.shoot(x, y + 1.5, z, dx, dy, dz, 80, 6)   -- nothing if it missed
local killed = core.blast(x, y, z, 7, 140)
core.mob_sight("mymod:imp", 64)   -- how far a kind sees whom to chase (16)
```

On the client, `core.arms_clip("fire_shotgun")` plays once, from the start, a clip of the
first-person arms (the `<player>_arms` model), even if already playing, and then leaves them to the
engine: a weapon's shot, a weapon being raised. `core.held_item` on the client answers for this
machine's player (id 1).

Arms at rest play `idle_<item>` when they have it (`idle_shotgun` with `yafps_game:shotgun` in hand),
otherwise `idle`: every weapon is held its own way.

`games/yafps` is the whole example: a multi-room base in modern arena shooter style with pistol,
pump shotgun, double barrel, machine gun and plasma cannon (each weapon a `tool_<weapon>` bone under
the robot arms' right hand, the hands put on the grips with IK in `tools/make_fps_models.py`), waves
of demons made with `core.register_mob`, ammo and HUD.

### What exists: `core.item_count`, `core.item_name`, `core.item_stack`

```luau
for id = 1, core.item_count() - 1 do
    local name = core.item_name(id)
    if name ~= "" then
        print(name, core.item_stack(id))   -- how many fit in a slot
    end
end
```

`core.item_id` answers on a name the mod **already has**; these three answer the other way, "what is
there in this world". They are for a creative inventory, a search box, a mod listing what a world
holds -- things that from Lua could not even be asked.

`item_name` answers **empty** for an id nobody registered and for an **orphan**, an item whose mod has
gone: the id stays (a stack in a chest keeps being worth what it was) but it can no longer be given to
anyone, so walking from 1 to `item_count` skipping the empties gives exactly the items a player can
receive. `mods/creative` is the example: a panel with a button per item, and a click gives a full
stack -- `item_stack` and not 64, otherwise every tool would be wrong.

### Containers, cinnabar and other things added on 2026-09-25

A summary for whoever writes a mod; signatures and defaults in [API.md](API.md).

**Containers among themselves.** `core.container_fill(x, y, z)` says how full a container is, 0..1,
as a comparator reads it (every slot is worth its stack divided by how many fit in a stack), -1 if
there is none there. `core.container_revision` is a number that changes at every change of the
contents, by hand or by a mod: a comparator watches it every 0.25 s. `core.container_move(from, to,
count)` moves the first stack from one container to another **as it is** (wear and enchantments
included), and `core.collect_drops(cell, container)` puts into a container what lies in a cell: they
are the two halves of a hopper (`mods/cinnabar`).

**Watching a cell.** `core.watch_cell(x, y, z, on)` asks to know when **anything** writes that cell:
a mod (flowing water, growing wheat), a player placing or breaking, the engine (a door opened by a
creature). It is heard as `core:watched`. Watches are not saved: an observer asks for them again with
an LBM when its chunk loads.

```luau
core.register_lbm("mymod:rewatch", { "mymod:sensor" }, function(cell)
    core.watch_cell(cell.x - 1, cell.y, cell.z, true)
end)
core.event.on("core:watched", function(ev) --[[ the cell changed ]] end)
```

**Moving bodies.** `core.push_mobs(x, y, z, dx, dy, dz)` moves creatures with their feet in that
cell; a player is moved with `core.player_place` (the piston slides them in four ticks of 0.25).

**Blocks.** `core.node_solid(id)` answers 1 or 0 -- compare it with 1, because in Luau 0 is true too.
A `wallmounted` block looks out of the face it was placed against, in six directions: that is how
pistons, observers and hoppers point up and down too (+X, -X, +Y, -Y, +Z, -Z for faces 0..5, and
boxes are written with the front towards +Z). **A box's picture is read with the first row at the
bottom**: a torch drawn with its head in the first rows comes out upside down.

**Renaming.** `core.node_alias(old, new)` loads cells saved with the old name as `new`, instead of
the magenta placeholder; `core.item_alias` does the same for stacks in bags and chests. They are read
only when a world opens.

**Time, respawn, regeneration.** `core.time_of_day()` (0 midnight, 0.25 dawn, 0.5 noon, 0.75 dusk)
and `core.set_time(t)`, which every client hears at once: the bed. `core.player_spawn(player, x, y,
z, on)` where that player respawns, while connected. `core.player_regen(player, on)` stops or
restarts health coming back by itself: hunger.

**Creatures.** `core.mob_drop(name, item, max)` is a second drop, 0..max: the cow's hide.
`core.monsters_near(x, y, z, r)` counts the fighting creatures (not `"pet"`) within r: the bed does
not let you sleep if one is within 8.

**Light.** `core.light(x, y, z)` is how lit a cell is now, 0..15: the blocks' light or the sky's for
the hour (full at noon, a fifth at midnight, none under `core.world_sky`). Wheat grows from 9 up.

**Riding on another: `core.ride`.** `core.ride(player, host, x, y, z)` puts `player` on `host`'s
body, at x to its right, y up and z ahead (each within 8), and keeps them there turning with it: a
kart's passenger. The server puts them back in place every tick and ignores their positions; they
look, dig, build and use as always. `core.ride(player, 0)` gets them off where they are; they get off
by themselves if `host` leaves. Server only (protocol 63).

**Turning, and knowing what is there.** `core.node_turns(id)` says how many values `core.set_facing`
takes for that block: 1 without a front, 4 upright, 6 wall-mounted, 24 a log; `mods/screwdriver`
cycles them one per click. `core.loaded(x, y, z)` is 0 for a cell whose column is not in memory,
which `core.get_block` reads as air.

**Enchantments.** A stack holds up to four enchantments in a byte, two bits of level per kind: 1
Efficiency (digs 1 + 0.5 per level faster), 2 Sharpness (one more point per level), 3 Unbreaking
(one use is spent in level + 1), 4 Protection (one armour point per level, worn).
`core.enchant_held(player, level, kind)` puts a kind on the thing in hand (only if it is a single
thing) leaving the others; `core.held_enchant(player)` answers with the byte: kind k's level is
`(v // 4^(k-1)) % 4`.

**Events between mods.** A mod declares an event of its own and another listens, without either
calling the other: `mods/furnace` keeps the smelting experience in the furnace's meta and gives it
with `furnace:xp` to whoever opens it; `mods/enchant` (which depends on `furnace`) counts it.

```luau
core.event.define("furnace:xp", { fields = { player = "number", points = "number" }, cancellable = false })
core.event.emit("furnace:xp", { player = ev.player, points = 3 })
```

### Pages: `core.page_register` and `core.page_show`

A page is a web-page-like screen, laid out by the engine on the server and drawn with the same
elements as a panel: no new message on the wire. A mod registers it with a name; when a player opens
it (`/page name`, or a `page:name` link) the mod receives `core:page_open` and answers with
`core.page_show`. The markup, one construct per line:

```
# Title                   a bar
## Subtitle               with a line under it
text with [a link](page:other) inside, wrapping by itself
| name | role | [kick](cmd:/kick bob) |       table rows, columns as wide as needed
?[write here](act:send)                        a field: Enter runs the target
> quieter text
---
```

The targets: `page:NAME` opens another page, `cmd:/line` runs a command as the player who clicked
(with their permissions, as if they had typed it; after, the page reopens updated), `act:ID` reaches
the mod as `core:page_action`. In a field `{text}` becomes what was written. Beyond one screen "back"
and "next" appear. The text is measured with the widths of the client font's glyphs, so it wraps
where it is drawn. In the single-process client (`--quick`) mods' pages are there all the same, with
`/page`. The complete example is `mods/board`.

A player under `/mute` does not talk to others through mods either: while one of their commands, a
page action or a panel click runs, the mod's `core.chat` to anyone else is held back, and they read
once that they are muted. `core.is_muted` is only for answering better (as `/msg` does).

### How a function is added to the API

Not by hand. Signatures live **once** in `src/script/api.luau`, and `tools/api_gen` (a generator
written in Luau, no new toolchain) produces three files at build time:

| file | what it is for |
|---|---|
| `src/script/api_generated.inc` | the C++ glue: `luaL_check*`, defaults, return values, registration |
| `docs/api.d.luau` | the types for the language server |
| `docs/API.md` | this documentation |

Adding `core.node_name` cost six lines in the descriptive table plus the host function: the glue, the
types and the doc updated themselves. The real cost of bindings without FFI (DESIGN.md §5.4) is paid
with tooling, not by hand.

`core.event.*` and `core.register_mapgen` stay hand-written: they take tables or functions instead of
scalars, and a generator covering them would be bigger than what it replaces.

### Survival mode: levels behind the portals

A new world can be **sandbox** (the plaza with its portals) or **survival** (no plaza,
`cnp_server --survival` on the sandbox room, the world's `mode` file). **A mod never asks for the
mode**: it looks at the data. That way a fix to a mod holds for both.

- `core.plaza()` gives the plaza's `lo, hi` (20, 100) or `nil` where there is none: whoever kept it
  clear (trees, villages, ruins, ...) or built on it (steps, bench, chess, ...) follows this answer.
- `core.seed()` is the seed outside a mapgen stage too; `core.world_spawn()` answers already while
  mods load, in a world without a plaza.
- `core:died {player, x, y, z}`: a player died (already put back at the bed or spawn). `mods/grave`
  puts the bag in a grave there (`core.bag_to_container`).
- `core.mob_health(id, hp)`: a creature tougher than `core.register_mob` makes it.

**Sites** belong to `mods/gates`, a function of the seed and of 256-node cells: where they are, of
which world, with what difficulty `d` from 1 to 10 (one more every 300 nodes from the spawn). Every
world's mouths (two for the seven with a key) go to the sites nearest the players; whoever enters
carries the `core.player_carried` line:

```
site=<id> d=<1..10> s=<site seed> t=<room>
```

Other mods ask for sites with `gates:ask {x, z, r, tag, player}` and get at once
`gates:sites {tag, list = "room d x z;..."}`, without those `player` has already done
(`mods/villagers` makes rumours and directions of them). `maps:pin {player, label, x, z, colour}` puts
a point on the map (1 purple, 2 green).

**The level protocol** is in `mods/keys`, linked in every themed world. Whoever arrives from a site
gets a **slot** of that world (0 is the hand-written trial, always where it was). Slot `n` is a
2048 square starting at `x = 8192 + (n % 16) * 2048, z = (n // 16) * 2048`. A world with a level of
its own answers the events like this:

| event | emitted by | fields | what it means |
|---|---|---|---|
| `keys:level` | `mods/keys` | `player, slot, d, seed, ox, oz` | build the slot's level and take the player there; cancellable: whoever cancels it has taken it, otherwise the player does the world's trial |
| `keys:start` | the world | `player, slot, ex, ey, ez` | the exit: a hole 1 wide and 2 high at `ex ey ez`, facing +x, 8-20 from the start |
| `keys:cleared` | the world | `player, slot` | the level is done |
| `keys:earned` | the world | `player, key` | the world's trial is passed (holds for a level too) |
| `keys:reward` | `mods/keys` (sandbox) | `player, list` | the rewards won, to `mods/gates`, which puts them in a chest at the site |

**The hidden exit.** The two cells at `ex + 1` (in front of the hole) are a wall of the level, and
the cell above them, at `ex + 1, ey + 2`, is filled by `mods/keys` with an unlit lamp (`keys:hint`):
the only sign. At `keys:cleared` the two cells become air, the lamp lights and the world's first mouth
towards the sandbox goes into the hole, open only to whoever finished. The level brings the player
back with a one-way piece, usually a portal of the world itself (`core.portal_open`, opened only once
the level is done: two linked portals go both ways). A level with `d >= 4` gives the world's key;
every level gives a reward.

The world's mapgen stage recognises a slot's columns by x (`>= 8192`) and builds its plan, a function
of the slot, of `d` and of the seed saved in the mod's storage on arrival. So columns are made when
needed and stay in the world file. Examples: `games/parkour/mods/parkour` (stations and jumps),
`games/backrooms/mods/backrooms_exit`, `games/arpg/mods/arpg_game` (`L.add_level`),
`games/track/mods/track_core` (a circuit, `track_core:level` / `track_core:laid`),
`games/chambers/mods/chambers_game` (a copy of the row of chambers from x 20000).

---

## Minimal example

```lua
--!strict
local BLUE = core.register_node("mine:blue", 60, 90, 220)

for i = 0, 9 do
    core.set_block(50 + i, 20, 50, BLUE)
end

local t = 0.0
function on_step(dt: number)
    t += dt
end
```

with

```
name = mine
version = 0.1.0
capabilities =
```

The mods in `mods/` are working examples:

- `spike` -- no capability, a tower and a portal that moves every frame;
- `beacons` -- `capabilities = os`, `depends = spike`, a pulsing beacon;
- `palette` -- twenty blocks declared in a loop, which is also the proof that the atlas grows by
  itself (from 16 to 64 tiles) without touching anything in the engine;
- `guard` -- cancels `core:pre_place` inside the portal plaza, replaces the block above y=30, and
  raises an event of its own that `beacons` listens to from its own VM. No engine line was written for
  the rule "you do not build here";
- `portal` -- a frame of `portal:frame` round a hole of air, upright or lying, opens a portal
  (`core.portal_open`, as many frames as you like: the server gives the 32 real openings to those near
  players) and links it to the last one left alone (`core.portal_link`); right click on the frame opens
  its menu, with the other frames to link it to. Until it is linked the client closes it with an
  animated green membrane, which opens from the centre onto the picture when the other end arrives
  (0.9 s);
- `chambers_game` and `lab` -- the portal gun: every shot is a `core.portal_open` with the button's
  colour (1 blue, 2 orange), so the first portal shot is closed in its colour until the second exists;
- `games/track` (`track_core`) and `games/inferno` (`inferno_track`) -- the inferno track, between
  two rooms with three mouths each (up to 4 per world): the track's frame A (opening 1,
  `--portal-to @inferno/2`) leads to B, the inferno's opening 2, which leads back to A (`@pista/1`); C
  (the inferno's opening 1, `@pista/2`) leads to D, the track's opening 2, which leads back to C
  (`@inferno/1`). Openings 2 and 3 are set with `core.set_portal` and `core.portal_place` like the
  first two. A driving body (`core.body_drive`) passes into the other room still in its kart, if that
  room knows the model it wears, and with it the `core.player_carry` line (here: the lap in progress),
  which is read on the other side with `core.player_carried`; the room left knows from
  `core.body_taken` and does not park a kart that has gone. `track_game` runs in both rooms and times
  only its own gates (`inferno = true`). Behind a frame a kart crosses there must be room: the kart
  passes the plane by a few nodes before the crossing is answered;
- `tools` -- a block that asks for level 1, the pickaxe that opens it and the shovel that does not,
  with the recipes to make more. It is where a tool belongs: the engine puts seconds and materials on
  its own blocks and leaves levels and tools to content.
