# API di scripting

<!-- GENERATO da tools/api_gen a partire da src/script/api.luau: non modificare a mano. -->

Tutto sotto `core`. I tipi sono quelli che il type checker vede in
[`api.d.luau`](api.d.luau); un argomento con un default e' opzionale.

**Quello che non e' qui.** Le chiamate che prendono tabelle o funzioni
sono scritte a mano in `src/script/script_luau.cpp` e documentate in
[MODDING.md](MODDING.md); il generatore rifiuta di girare se una manca da li'.
Sono: `core.register_mapgen`, `core.register_abm`, `core.register_lbm`, `core.event.define`, `core.event.on`, `core.event.emit`, `core.ecs.component`, `core.ecs.phase`, `core.ecs.system`, `core.ecs.spawn`, `core.ecs.set`, `core.ecs.get`, `core.ecs.kill`, `core.ecs.visible`, `core.ecs.model`, `core.ecs.health`, `core.ecs.clip`, `core.ecs.box`, `core.ecs.count`, `core.craft.make`, `core.craft.keys`, `core.craft.pose`, `core.craft.push`, `core.craft.mount`, `core.craft.aim`, `core.craft.seat`, `core.net.open`, `core.net.send`, `core.render.pass`, `core.render.box`, `core.render.model`, `core.render.fx`, `core.hud.pass`, `core.hud.text`, `core.hud.rect`, `core.hud.aspect`, `core.hud.image`, `core.hud.item`, `core.hud.mouse`.

## `core.set_block`

```luau
core.set_block(x: number, y: number, z: number, id: number) -> ()
```

Sets one block. Out-of-world coordinates are ignored, not an error. The cell is left facing 0: this call says which block, not which way round, and a cell keeps no turn of the block that was in it. Say the turn with core.set_facing after it.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `id` | number | — | Block id, as returned by register_node or node_id. 0 is air. |

## `core.set_facing`

```luau
core.set_facing(x: number, y: number, z: number, facing: number) -> ()
```

Turns one cell. A block with a FRONT wears its pictures moved round by this, and placing one turns the cell to face the player -- so this is the same byte, written by a mod instead. It is on the CELL and not on the block: every cell has one, and it means nothing to a block with no front.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `facing` | number | — | 0 to core.node_turns(block) - 1; anything else is refused, and the log says so. |

## `core.get_facing`

```luau
core.get_facing(x: number, y: number, z: number) -> number
```

How far one cell is turned, 0 to core.node_turns(its block) - 1. Zero outside the world, the same answer as for a cell nobody has turned.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.get_block`

```luau
core.get_block(x: number, y: number, z: number) -> number
```

Reads one block. Anything outside the world reads as air (0).

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.set_portal`

```luau
core.set_portal(index: number, x: number, y: number, z: number) -> ()
```

Moves a portal to a new centre. Every copy of that opening follows -- both ends of the renderer's pair AND the one the body walks through. Miss any of them and the portal is drawn in one place and teleports from another. On a server it places the world's two openings (--portals), and only while the mods load: the pair goes to every client, and one moved afterwards would be the same lie. Later calls are ignored, and said once.

| argomento | tipo | default | note |
|---|---|---|---|
| `index` | number | — | 0 is portal A, 1 is portal B; 2 to 7 a third to eighth --portal-to. |
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.get_portal`

```luau
core.get_portal(index: number, axis: number) -> number
```

Where a portal is now, one axis at a time. Use it to move a portal RELATIVE to where it already is: a mod that writes absolute coordinates instead is hardcoding the scene it was written against.

| argomento | tipo | default | note |
|---|---|---|---|
| `index` | number | — | 0 is portal A, 1 is portal B; 2 to 7 a third to eighth --portal-to. |
| `axis` | number | — | 0 is x, 1 is y, 2 is z. |

## `core.portal_room`

```luau
core.portal_room(index: number) -> string
```

The room of this server that opening leads to (--portal-to @ROOM), or an empty string: no such opening, or one onto another server. Known while the mods load, so a mod can build each opening a gate that looks like the world behind it.

| argomento | tipo | default | note |
|---|---|---|---|
| `index` | number | — | One of the world's openings that lead to other worlds (--portal-to), 0 to 7. |

## `core.portal_place`

```luau
core.portal_place(index: number, x: number, y: number, z: number, axis: number, sign: number, half_w: number?, half_h: number?) -> number
```

Moves one of the world's two openings while the world runs, on a server: a frame a player built and lit becomes the way to the world that opening leads to, and the old place is a frame with nothing in it. Every player is sent the change, and a crossing from the far world comes out here. Returns 1, or 0 on a world that is not running yet, in a single-process client, or for an index or a shape out of range.

| argomento | tipo | default | note |
|---|---|---|---|
| `index` | number | — | 0 to 7: one of the world's openings that lead to other worlds (--portal-to); 2 to 7 only when they do. |
| `x` | number | — | The centre of the opening. |
| `y` | number | — |  |
| `z` | number | — |  |
| `axis` | number | — | 0 XY (it faces along z), 1 YZ (along x). |
| `sign` | number | — | 1 or -1: which way its front faces. |
| `half_w` | number | `0.5` | Half its width, up to 16: 0.5 is one node wide. |
| `half_h` | number | `1` | Half its height, up to 16: 1.0 is two nodes tall. |

## `core.portal_open`

```luau
core.portal_open(x: number, y: number, z: number, axis: number, sign: number, rot: number, half_w: number, half_h: number, colour: number?) -> number
```

Opens a portal while the world runs, on a server. It becomes the world's first free opening of 16 -- or, when an opening in the same plane already has this centre, that one takes the new shape, so a frame turned or flipped from its menu stays the same opening. It leads to the opening it is linked with (core.portal_link); until something links it, opening i leads to i xor 1, so 0 and 1 are a pair, 2 and 3, and so on. Every player is sent the change. In a world whose openings 0 and 1 lead to another world (--portal-to), a pair opened here besides them is still this world's own: crossed by the server, drawn by the client (the nearest two in sight). Returns which opening it is, or -1: all 32 in use, a shape out of range, no world to open it in yet (the openings exist once the server listens -- in a room, a few ticks after the first core:tick, so ask again until it opens), or the single-process client, whose two openings are its scene's.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — | The centre of the opening. |
| `y` | number | — |  |
| `z` | number | — |  |
| `axis` | number | — | The plane it lies in: 0 is XY (it faces along z), 1 is YZ (along x), 2 is XZ (a floor or a ceiling). |
| `sign` | number | — | 1 or -1: which way along that axis its front faces. |
| `rot` | number | — | 0..3 quarter turns of the opening about its front. |
| `half_w` | number | — | Half its width, along its own right, after the turn. At most 16. |
| `half_h` | number | — | Half its height, along its own up. At most 16. |
| `colour` | number | `0` | What it shows while it leads nowhere: 0 green (a fixed portal), 1 blue, 2 orange (a portal gun's). |

## `core.portal_link`

```luau
core.portal_link(a: number, b: number) -> number
```

Makes two openings lead to each other, on a server: what is seen in one is seen from the other, and a body walking into one comes out of the other. Whatever either led to before leads nowhere now. An index with no opening may be linked, so a mod can link before it opens. Returns 1, or 0 for an index out of range or no server.

| argomento | tipo | default | note |
|---|---|---|---|
| `a` | number | — | An opening, as portal_open returned it. |
| `b` | number | — | The opening it leads to, or -1 for none. |

## `core.portal_close`

```luau
core.portal_close(x: number, y: number, z: number) -> number
```

Closes the opening with that centre, on a server: nothing is drawn in it or crosses it, and the other end leads nowhere until something is opened again. By centre and not by index, so a mod closes the portal it opened and not whatever has taken that index since. Returns which opening it was, or -1.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — | The centre portal_open was given. |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.portal_seal`

```luau
core.portal_seal(player: number, index: number, sealed: boolean) -> number
```

Shuts one mouth to one player, or opens it again, on a server: sealed, that player sees a red membrane across it and does not walk through; opened, the membrane parts onto the world behind it. Everybody else is not touched -- a way out that opens for whoever passed the trial behind it. Forgotten when the player leaves this world: seal again on arrival. Returns 1, or 0 for no such player or an index that is not a mouth.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |
| `index` | number | — | A mouth onto another room (--portal-to), 0..15. |
| `sealed` | boolean | — |  |

## `core.log`

```luau
core.log(message: string) -> ()
```

Writes a line to the engine log, prefixed with the mod's name.

| argomento | tipo | default | note |
|---|---|---|---|
| `message` | string | — |  |

## `core.register_node`

```luau
core.register_node(name: string, r: number?, g: number?, b: number?, solid: boolean?, light_source: number?, turn: string?, dig_level: number?, hardness: number?, material: string?) -> number
```

Declares a block and returns its id. Registering the same name again keeps the id, which is what makes a hot reload possible without renumbering a world.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | Namespaced, like "mymod:copper". |
| `r` | number | `180` |  |
| `g` | number | `180` |  |
| `b` | number | `180` |  |
| `solid` | boolean | `true` | Stops movement and light. |
| `light_source` | number | `0` | 0..15, block light emitted. |
| `turn` | string | `` | What a cell's facing byte MEANS for this block, and nothing means it does not turn. "upright": 0..3 quarter turns about the vertical, which is a chest or a furnace. "facedir": 0..23, any orientation of a cube -- a log lying down, a stair. "wallmounted": 0..5, the FACE the block's front points at -- a ladder, a sign. Placing a block turns the cell to face whoever put it down, or, for a wallmounted one, out of the face they clicked; core.set_facing writes the byte and core.get_facing reads it. The id in the world is the one this call returned whichever way the block faces, so a handler compares against it directly. An unknown word is a load error, not a block that quietly does not turn. |
| `dig_level` | number | `0` | What a hand must be holding for this to come out at all. 0 is a bare hand, which is what every block is unless it says otherwise; a block asking for 2 does not break for a level 1 tool however long anybody holds the button. A LEVEL and not a kind: a level 2 axe digs this as well as a level 2 pick does. Separate from breakable -- the floor of the world is not a very hard block, and no tool ever takes it out. |
| `hardness` | number | `0` | How many SECONDS this takes to dig with a bare hand, divided by the tool's dig_speed. Zero is instant, which is what every block is unless it says otherwise. The player holds the button; the server times it. |
| `material` | string | `` | What this block is MADE of -- "stone", "soil", "wood" -- which is what a tool is for. A tool's dig_level and dig_speed count against the material it names in register_item's `digs` and are a bare hand's against everything else, so a shovel is quick through soil and no help at all in front of stone. Empty is a block that does not say, and it is dug by any tool for whatever that tool is worth. |

## `core.node_model`

```luau
core.node_model(id: number, model: number) -> number
```

Draws a block as a MODEL -- the same glTF a body wears -- and returns the model it now wears, or 0 if the world has no such model.

The model is the picture and the boxes are the block: a body is stopped by core.node_box's boxes and a ray finds those, never the mesh, because a mesh is a thousand triangles and a collision is a handful of boxes. A block that wants a shape under its model declares both.

It is turned about the middle of its cell by the cell's facing byte, like a box is, so a model authored inside the unit cube with its feet at 0 stands where the block is and faces the way it was put down. A client that has not got the file draws the boxes instead, and a block with none draws nothing: never a cube, because a cube is what it is not.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block, by the id register_node returned. |
| `model` | number | — | A model id from core.register_model, or 0 to put the block back to being drawn from its own table. |

## `core.node_drop`

```luau
core.node_drop(id: number, as: number) -> number
```

What breaking a block gives, when that is another block's item: half of a double chest gives a chest, a lit furnace gives a furnace. A block that drops as another has no item of its own -- nobody can hold one, and the creative page does not list it. Returns 1, or 0 for an id that is not a block.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block, by the id register_node returned. |
| `as` | number | — | The block whose item breaking it gives, or 0 for its own. |

## `core.node_drop_item`

```luau
core.node_drop_item(id: number, item: string) -> number
```

What breaking a block gives, when that is an item and not a block: diamond ore gives a diamond. Like node_drop, the block then has no item of its own. Returns 1, or 0 for an id that is not a block.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block, by the id register_node returned. |
| `item` | string | — | The item breaking it gives, by name. |

## `core.watch_cell`

```luau
core.watch_cell(x: number, y: number, z: number, on: boolean?) -> number
```

Asks to hear when anything writes that cell: a mod (water running into it, wheat growing in it), a player placing or breaking, the engine (a door a creature opens). Heard as core:watched {x, y, z}, once per cell per tick, at the start of the next core:tick. Watches are this process's and not saved: ask again when the chunk loads (an LBM). Returns 1.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `on` | boolean | `true` |  |

## `core.node_solid`

```luau
core.node_solid(id: number) -> number
```

1 if that block stops bodies and light (register_node's solid), 0 if not or no block.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | A block id. |

## `core.ride`

```luau
core.ride(player: number, host: number, x: number?, y: number?, z: number?) -> number
```

A seat: puts `player` on `host`'s body and keeps them there, turning with it -- a passenger in a kart, one player on another's shoulders. While seated their keys move nothing; they look about, dig, build and use as ever. 0 for the host lets them off where they are, walking. Let off by itself when the host leaves or gets a seat of their own. Returns 1, or 0 refused: no such player, one on themselves, a host who is seated, a seat too far out. On a server; a world with one player has nobody to carry.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | Who sits. |
| `host` | number | — | Whose body they sit on, a player id; 0 gets off. |
| `x` | number | `0` | The seat, to the host's right. |
| `y` | number | `0` | Up from its feet. |
| `z` | number | `0` | To its front. Each within 8 nodes. |

## `core.loaded`

```luau
core.loaded(x: number, y: number, z: number) -> number
```

1 if the blocks of that cell are in memory, 0 if not: its column has not been made or read yet, or was let go. core.get_block reads such a cell as air, and this is how a mod tells that air from a real one.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.keep_loaded`

```luau
core.keep_loaded(x: number, z: number, r: number?) -> number
```

Keeps the columns over that square in memory for thirty seconds, made or read back if they are not, with nobody near: ask again to keep them longer. What a robot working far from every player stands on and walks to. Returns how many columns; 0 where nothing is let go (the single-process client).

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `z` | number | — |  |
| `r` | number | `0` | Cells round (x, z), at most 48. |

## `core.node_turns`

```luau
core.node_turns(id: number) -> number
```

How many turns a cell of that block may be in: core.set_facing takes 0 to this minus one. 1 for a block with no front, 4 for an upright one, 6 for a wallmounted one (a face each), 24 for one that lies any way. 0 for no block. What a screwdriver cycles through.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | A block id. |

## `core.light`

```luau
core.light(x: number, y: number, z: number) -> number
```

How lit that cell is now, 0 to 15: the brighter of the light blocks give it and the sky's, the sky's scaled by the hour (full at noon, a fifth at midnight) and none under a world_sky. What a crop needs to grow and a creature to come out is read against this. 0 for a cell not in memory or inside a solid block.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.block_light`

```luau
core.block_light(x: number, y: number, z: number) -> number
```

The light the light blocks give that cell, 0 to 15, the sky left out: what it has at midnight. A creature comes out of the dark only where the light is 7 or less. 0 for a cell not in memory or inside a solid block.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.node_alias`

```luau
core.node_alias(old: string, name: string) -> number
```

A block renamed: cells a world file saved under the old name load as this block instead of a magenta placeholder. Only a saved world's table reads it. Returns 1.

| argomento | tipo | default | note |
|---|---|---|---|
| `old` | string | — | The name a saved world may still hold. |
| `name` | string | — | The block that stands for it now. |

## `core.item_alias`

```luau
core.item_alias(old: string, name: string) -> number
```

An item renamed, or gone: stacks a world file saved under the old name load as this item instead of one nothing can use. Only saved stacks read it. Returns 1.

| argomento | tipo | default | note |
|---|---|---|---|
| `old` | string | — | The name a saved bag or chest may still hold. |
| `name` | string | — | The item that stands for it now. |

## `core.player_regen`

```luau
core.player_regen(player: number, on: boolean) -> number
```

Whether this player's health comes back on its own, as it does unless a mod says otherwise -- a hunger that has to be fed first. Returns 1, or 0 for nobody.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | The id every event carries in its `player` field. |
| `on` | boolean | — | false: health no longer comes back on its own. |

## `core.player_max_health`

```luau
core.player_max_health(player: number, max: number) -> number
```

How much health a full body of this player has: a world whose characters grow sturdier as they rise. What they have now is kept, and cut to the new maximum if it is over it; a death brings them back with all of it. In memory only, for as long as they are in this world. Returns 1, or 0 for nobody or a number out of range.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | The id every event carries in its `player` field. |
| `max` | number | — | Points of a full body, 1 to 1000; the engine's own is 20. |

## `core.time_of_day`

```luau
core.time_of_day() -> number
```

The world's clock, 0 to 1: 0 midnight, 0.25 dawn, 0.5 noon, 0.75 dusk.

## `core.set_time`

```luau
core.set_time(t: number) -> number
```

Sets the world's clock, and every client hears it at once: a bed that ends the night. Returns 1.

| argomento | tipo | default | note |
|---|---|---|---|
| `t` | number | — | 0 to 1, as time_of_day reads it; wrapped. |

## `core.player_spawn`

```luau
core.player_spawn(player: number, x: number, y: number, z: number, on: boolean?) -> number
```

Where this player comes back after dying, instead of the world's spawn -- a bed. Kept while they are connected; a mod that wants it across a restart stores it and sets it again. Returns 1, or 0 for nobody.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | The id every event carries in its `player` field. |
| `x` | number | — | Where their feet go when they die. |
| `y` | number | — |  |
| `z` | number | — |  |
| `on` | boolean | `true` | false forgets it: they come back at the world's spawn again. |

## `core.node_climbable`

```luau
core.node_climbable(id: number, on: boolean?) -> number
```

A body in a cell of this block climbs: no gravity, up while jump is held or while it walks into the ladder or its wall, still while crouching, sliding slowly down otherwise -- a ladder. Give it a thin core.node_box against the wall so a body can stand in its cell. Falling off one counts from where it let go. Creatures climb it too, and a way through the world (core.find_path) goes up and down it.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block, by the id register_node returned. |
| `on` | boolean | `true` |  |

## `core.node_conveyor`

```luau
core.node_conveyor(id: number, speed: number?, height: number?, rise: number?) -> number
```

A conveyor belt: a stack, a player or a creature standing on a cell of this block is carried the way the cell runs -- away from whoever put it down, which is facing 0 heading -z, 1 -x, 2 +z, 3 +x. A stack is also drawn to the middle of the belt across, so it follows a belt that turns a corner, and held on its top, so it goes up and down a ramp; something solid ahead stops it, off the end it falls. A ramp's top may stand out of its cell (height + rise over 1): give it core.node_box steps a body can walk up. A ramp down at 3 m/s or more is a slide: a player is not carried but rides it, sat on its line -- straight, or round a quarter circle where the slide a cell up comes into its side -- gathering speed up to `speed`, and flies off its end; make it not solid, so its boxes are not in the way. Returns 1, or 0 for an id that is not a block.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block, by the id register_node returned: an "upright" one. |
| `speed` | number | `1` | m/s. 0 makes it no belt again. |
| `height` | number | `1` | Its top over the cell's floor where things come in, in nodes. |
| `rise` | number | `0` | How much higher its top is where things leave: 1 a ramp up a block, -1 down one. |

## `core.node_door`

```luau
core.node_door(shut: number, open: number) -> number
```

Two blocks that are one door, shut and open: opening one is putting the other in its cell. A way through the world (core.find_path) goes through an open door and treats a shut one as a wall, unless the body can open doors -- then it goes through for a little more, and a creature that opens doors (register_mob) opens it when it gets there, and shuts it behind it once it has gone through and nobody stands in it. A body a door's box is shut on is let out of it. Each half of a door two cells tall is its own pair. Returns 1, or 0 for ids that are not two different blocks.

| argomento | tipo | default | note |
|---|---|---|---|
| `shut` | number | — | The door shut, by the id register_node returned. |
| `open` | number | — | The same door open. |

## `core.node_piece`

```luau
core.node_piece(id: number, fill: number, plan: number, size: number, climb: number?, grade0: number?, grade1: number?, bank0: number?, bank1: number?, curbs: boolean?, wall: boolean?, width: number?) -> number
```

Makes a block a piece of a track, as stunt racers build them: a smooth road on a grid of 8 nodes that a kart drives on, joined to the next piece where their ends meet. A player puts one down like any block -- it snaps to the grid and runs away from them, or turned a quarter for each Z (left) or X (right) they pressed with it in hand, and never over a body -- and breaking any of its cells takes the whole piece. A slope down is a slope up put down the other way round. Returns 1, or 0 for a shape that is no piece (it would leave the 18 cells above its corner, or dip under it).

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block that is the piece, by the id register_node returned. |
| `fill` | number | — | The block its other cells are: the boxes under its road that a foot stands on. Never drawn; breaking one breaks the piece. Several pieces may share one. |
| `plan` | number | — | 1 a straight, 2 a quarter turn to the right, 3 a chicane one square across to the right and 4 to the left, 5 a quarter turn to the left, 6 a loop that comes out a road's width to the left of where it went in and 7 to the right (2 or 3 squares long, flat, no curbs; up 8.5 nodes on one lane, 16.9 on one lane three squares long, 11.1 on two), 8 a wide road down to its right lane and 9 to its left (width 2, unbanked; put down the other way round it opens one lane out into two). |
| `size` | number | — | Squares of 8 nodes, 1 to 3: a straight's length, a turn's radius, a chicane's length (2 or 3). |
| `climb` | number | `0` | Nodes the road rises from its entry to its exit. |
| `grade0` | number | `0` | How steeply it leaves the entry, in nodes per square: 2 is 14 degrees, 4 is 27. |
| `grade1` | number | `0` | And how steeply it arrives at the exit. |
| `bank0` | number | `0` | Nodes the left edge stands above the right at the entry (negative: the right is higher). On 8 nodes of road 2 is 14 degrees, 4 is 27. |
| `bank1` | number | `0` | And at the exit. |
| `curbs` | boolean | `true` | A raised edge either side; false is a platform. |
| `wall` | boolean | `false` | A wall ride, straights only: bank0 and bank1 are then degrees the road is rolled about its own middle, 90 standing it up as a wall with its left edge four nodes up and its right four down, -90 the other way. A kart stays on a road steeper than 50 degrees while it is fast: pressed to it from 10 m/s, fully from 16. |
| `width` | number | `1` | Squares across: 1, or 2 for a road twice as wide -- straights and turns (a turn's size at least its width), not chicanes or wall rides. A wide piece is put down by the square of its right lane. |

## `core.place_piece`

```luau
core.place_piece(x: number, y: number, z: number, id: number, facing: number) -> number
```

Puts a track piece down the way a player does: entered by the grid square holding (x, z), at height y, running the way `facing` says. It takes the place of whatever is in its cells, ground or blocks, and empties the 3 cells over its road. Returns 1, or 0 when another track piece is in the way.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — | Any cell of the grid square the piece is entered by. |
| `y` | number | — |  |
| `z` | number | — |  |
| `id` | number | — | A block core.node_piece made a piece. |
| `facing` | number | — | 0 to 3, as a player's placement writes it: 2 is entered from -z heading +z, and each step turns it a quarter the other way round (1 heads -x, 0 heads -z, 3 heads +x). |

## `core.piece_entry`

```luau
core.piece_entry(x: number, y: number, z: number) -> (number?, number?, number?, number?, number?)
```

Where the track piece that cell belongs to is entered: the corner cell of its entry square (its right lane on a wide road), its height, its block and its facing -- the x, y, z, id and facing core.place_piece would put it down with again. Returns NOTHING for a cell of no piece.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — | Any cell of a track piece. |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.take_piece`

```luau
core.take_piece(x: number, y: number, z: number) -> number
```

Takes a whole track piece away, as breaking any of its cells does, and gives nothing to anybody. Returns the piece's block, or 0 for a cell of no piece.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — | Any cell of a track piece. |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.node_box`

```luau
core.node_box(id: number, x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, part: string?) -> number
```

Adds a box to a block's shape and returns how many it now has. A block with any boxes is no longer drawn as a cube: the mesher leaves its cell alone and the boxes are drawn on their own, so it also stops hiding its neighbours' faces.

Call it once per box, after register_node. Boxes are in node space and may leave the cell, though nothing yet draws a neighbour's cell for it -- a box that sticks out is drawn and is not lit by the cell it sticks into, and a body in that cell is stopped by it.

The boxes are the block: a body is stopped by them and not by the cell, a ray finds them and passes through the rest of the cell, a dropped item rests on them, and the outline a player sees is drawn around them. So a block half a cell tall is stood on half a node up, and the other half of its cell is air to walk through.

A body WALKS onto one up to 0.55 of a node high, which is what makes a stair a stair; higher than that has to be jumped, and a whole node always does.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block, by the id register_node returned. |
| `x0` | number | — | Corner of the box in node space: 0 is one wall of the cell, 1 the opposite one. |
| `y0` | number | — |  |
| `z0` | number | — |  |
| `x1` | number | — |  |
| `y1` | number | — |  |
| `z1` | number | — |  |
| `part` | string | `` | What this box IS, by a name the mod chooses -- "lid", "door". Empty for a box that is simply part of the shape.

A NAMED box is a piece that MOVES: it swings up about its own back-bottom edge while somebody has that cell open, and back down when the last of them closes it. The engine never learns what a lid is -- it only asks whether a box was given a name. |

## `core.node_name`

```luau
core.node_name(id: number) -> string
```

The name a block id was registered under. Empty for an id nobody registered. Useful in a log line: an id is meaningless to the person reading it.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — |  |

## `core.node_yield`

```luau
core.node_yield(id: number) -> string
```

The item breaking a block of this id gives, by name, as a player breaking it gets it (core.node_drop, core.node_drop_item): diamond ore gives a diamond. Empty for air and for a block that gives nothing.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — |  |

## `core.node_id`

```luau
core.node_id(name: string) -> number
```

Looks up a block registered by somebody else. Returns 0 if there is no such block, so a mod that needs another mod's blocks should declare `depends` in its manifest.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — |  |

## `core.terrain_height`

```luau
core.terrain_height(x: number, z: number) -> number
```

The generator's own ground height at a column: the y of the first cell of AIR above the terrain, so a tree goes at this y and the block it stands on is at this y minus one. Meant for a mapgen stage, and it answers outside one too -- the terrain is a function of the seed and the column, not of when it is asked.

Here so that a mod does not re-derive the noise. A mod computing its own height is a second opinion about where the ground is, and the day the generator changes only one of the two follows it.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `z` | number | — |  |

## `core.water_level`

```luau
core.water_level(x: number, z: number) -> number
```

Where the generator puts water over a column: the y of the first cell above it -- 18 over the sea, a river or a low lake, a lake's own level up in the hills -- or core.terrain_height where the column is dry. 0 where there is no generator to ask. mods/lakes fills the cells from the ground up to it.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `z` | number | — |  |

## `core.biome`

```luau
core.biome(x: number, z: number) -> string
```

The climate the generator gave a column, by name: `core:plains`, `core:desert`, `core:tundra` or `core:peaks`; or, where the ground lies against the water, `core:ocean`, `core:sea`, `core:river`, `core:lake`, `core:beach` or `core:gorge`. Empty where there is no generator to ask.

Like core.terrain_height, a function of the seed and the column and not of the world, so a mapgen stage may ask it about a column that has not been generated yet -- which is what a tree standing in the next column, with its crown in this one, needs to know.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `z` | number | — |  |

## `core.node_falls`

```luau
core.node_falls(id: number, on: boolean?) -> number
```

Makes a block fall, as the classic block game's sand and gravel do: whenever its cell or the cell under it is written -- a dig, a placing, a mod, a piston -- and nothing solid is under it, it lets go (core.fall), and a column of them goes together. Not while the world is generated. Returns 1, or 0 for an id that is not a block.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block, by the id register_node returned. |
| `on` | boolean | `true` |  |

## `core.node_glass`

```luau
core.node_glass(id: number, opacity: number?) -> number
```

Lets light through a block and shows what is behind it: ice, a pane, a cobweb. It still stops a body if it is solid. Returns 1, or 0 for an id that is not a block.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block, by the id register_node returned. |
| `opacity` | number | `160` | How much of what is behind it is hidden, 0..254: drawn in the alpha pass, as glass is (core:glass is 110). |

## `core.node_liquid`

```luau
core.node_liquid(id: number, opacity: number?, fill: number?) -> number
```

Makes a registered block a liquid: a body goes through it and swims in it -- it sinks slowly, rises while jump is held, and moves at half its pace -- light passes through it, it is drawn see-through, and nothing digs it out. Returns 1, or 0 for an id nobody registered.

The server tells every client that joins what each block does to a body, so a client without the mod swims in the lake too.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — |  |
| `opacity` | number | `150` | 0..254: how much of what is behind it is hidden. 0 hides nothing. |
| `fill` | number | `8` | How full a cell of it is, in eighths: 8 is a whole cell. Water running off a source is a block per level with less in each, and a client draws each at its own height with the surface sloping between neighbours. |

## `core.movement`

```luau
core.movement(walk: number?, sprint: number?, fly: number?, jump: number?, gravity: number?) -> number
```

Says how a body moves in THIS world: how fast it walks, sprints, flies and jumps, and how hard it falls. Returns 1 when the numbers were taken and 0 when they were refused, with the reason in the log.

Per world and not per player. Every client that joins is told these numbers in the welcome and moves by them, and what the server accepts as a position is worked out from them -- so a body faster than the engine's own numbers is not a cheat in a world that declared it, and a body faster than THESE is.

Call it while the mod loads. The numbers go out in the handshake, so a call once the mod directory has loaded -- from a tick, say -- is refused rather than applied to some players and not others. Two mods that both call it: the one loaded last wins, and the log names the one it replaced.

Refused: a speed that is negative, not a number, or over 256 m/s; a gravity that is not a number or points up. The terminal fall speed, 60 m/s, is the engine's and is not declared here: it is what bounds a fall however strong the gravity. Also refused, with the log saying so, in the code a connected world sends its clients: there the welcome's numbers are the ones, and so for core.drive, core.body_movement and core.body_drive.

| argomento | tipo | default | note |
|---|---|---|---|
| `walk` | number | `4.8` | m/s on the ground. |
| `sprint` | number | `8` | m/s on the ground with sprint held. |
| `fly` | number | `14` | m/s flying. Doubled while sprint is held, as it is with the engine's own number. |
| `jump` | number | `8` | m/s upwards, the moment a jump leaves the ground. |
| `gravity` | number | `-25` | m/s^2. Negative is down; upwards is refused. |

## `core.drive`

```luau
core.drive(accel: number?, brake: number?, turn: number?, drag: number?) -> number
```

Makes every body in THIS world drive: a kart on four wheels with springs, simulated as a rigid body (core.body_width says how wide and long). The keys stop setting a velocity: W is the throttle up to core.movement's sprint, S brakes and then reverses up to its walk, A and D turn the wheel -- not at all standing still, and less and less past half the top speed. The tyres decide how much of the turn the kart takes: driven into a bend too fast it slides wide and loses speed, and it pitches under braking and leans in the bend. Walls stop it, the drag fades the speed with nothing pressed, and nobody flies.

A chassis is kept within 26 degrees of upright and its wheels take nothing steeper than 50 degrees as ground, so a kart that grazes a wall does not lie against it and drive up it.

Checked by the server: a body is allowed its top speed and no flight, and it may rest against what it has driven into -- only its middle being inside a block is refused (core.body_width). NOT checked: how fast the speed grows, and the turn -- the server sees when a claim arrives and not when it was sent, and a check built on that let an instant 0 to 24 m/s through while correcting honest drivers on a jittery link (TODO.md OPEN-47).

Call it while the mod loads, like core.movement, and it goes out in the same welcome. Refused: a number that is negative or not a number, an accel or a brake over 256, a turn over 12.57 (two turns a second), a drag over 20.

| argomento | tipo | default | note |
|---|---|---|---|
| `accel` | number | `0` | m/s^2 with the throttle down. 0 is a body on foot. |
| `brake` | number | `0` | m/s^2 braking, before the body reverses. |
| `turn` | number | `0` | rad/s the wheel asks for at full lock, from 4 m/s up to half the top speed; half of it at the top speed. The tyres may give less. |
| `drag` | number | `0` | 1/s: how fast the speed fades with nothing pressed. |

## `core.body_width`

```luau
core.body_width(width: number?, length: number?) -> number
```

Says how big a body is in THIS world. On foot, the side of the square box it collides with, whichever way it faces; the height stays the engine's 1.8. A body that drives is a chassis `width` by `length` on four wheels, which turns with it: a kart 1.4 wide and 2.6 long is core.body_width(1.4, 2.6).

The server checks positions with a square box: `width` on foot, and for a body that drives the square that fits inside the chassis however it is turned, `width` / 1.41. Being INSIDE a block is the one question it asks differently: a body on foot is refused by that box, a body that drives by its middle alone. A chassis turns, tips and carries its wheels outside its box, so the two ends disagree about touching, and a refusal that puts a kart back where the server's box is clear leaves the chassis inside the wall it was scraping -- which the server then refuses again, every claim (TODO.md). A kart may rest against a wall; one whose middle is inside it is still put back.

An arrival out of a portal is pushed clear of the wall beside it by the width. Call it while the mod loads, like core.movement: it goes out in the same welcome. core.movement and core.body_movement keep it. Refused: a width under 0.2 or over 4, a length that is not 0 and under 0.2 or over 8, or not a number.

| argomento | tipo | default | note |
|---|---|---|---|
| `width` | number | `0.6` | Nodes: the side of the square box a body on foot collides with, and the width of a body that drives. |
| `length` | number | `0` | Nodes, nose to tail, of a body that drives (core.drive). 0 is as long as it is wide. |

## `core.eye_height`

```luau
core.eye_height(height: number) -> number
```

Says how high a body's eye is in THIS world: where the camera is in first person, and what reach and every ray a hand casts are measured from, on the client and on the server alike -- so it goes out in the welcome and has to be said while the mod loads, like core.movement. A world whose player model is not 1.8 tall says where that model's eyes are. The body's box stays 1.8 tall. Refused: under 0.2, over 1.8, or not a number.

| argomento | tipo | default | note |
|---|---|---|---|
| `height` | number | — | Nodes over the feet. The engine's is 1.62, in a body 1.8 tall. |

## `core.parkour`

```luau
core.parkour(on: boolean?) -> number
```

Says that bodies in THIS world do parkour: in the air, forward held fast along a wall beside them, they run along it level for up to 5 nodes; jump against a wall in the air kicks off it, away and up, so two walls are climbed by kicking from one to the other; crouch pressed at a run slides on the back, under a block; jump with back held is a backflip, onto a block two high behind; and core.rope hangs a body on a rope. Bodies there always run at core.movement's sprint, and hands grab ledges as anywhere else. The server allows what those do. It goes out in the welcome, so it is said while the mod loads, like core.movement. Returns 1, or 0 too late.

| argomento | tipo | default | note |
|---|---|---|---|
| `on` | boolean | `true` |  |

## `core.ledges`

```luau
core.ledges(on: boolean?) -> number
```

Whether hands grab a ledge in THIS world: in the air against a wall whose top they reach, a body hangs from it and pulls itself up. On unless a world says false -- one seen from above, where a body hanging off a wall is stuck halfway up it. In the welcome, so said while the mod loads. Returns 1, or 0 too late.

| argomento | tipo | default | note |
|---|---|---|---|
| `on` | boolean | `true` |  |

## `core.side`

```luau
core.side() -> string
```

Which side of a world this code is running on: "server"; "client", which is a world's code on a player's machine; or "both", a process that is its own server. A mod whose mod.conf says `side = both` runs in two places and asks this to know which half it is (TODO.md OPEN-49).

## `core.body_movement`

```luau
core.body_movement(player: number, walk: number?, sprint: number?, fly: number?, jump: number?, gravity: number?, rise: number?) -> number
```

Gives ONE player's body numbers of its own -- a runner, a power-up, a slowed prisoner -- and returns 1, or 0 when refused with the reason in the log. Unlike core.movement this may be called at any time: it is one body, and that player is sent the numbers in a message of their own.

The server checks that body against the new numbers at once, and for a second against whichever of the old and the new are faster, because the player's client goes on moving by the old ones until the message arrives: slowing somebody down does not correct the steps they had already taken.

The defaults are the engine's own numbers, not the world's: to put a body back to what the world declared, pass those. Refused: the numbers core.movement refuses, a player who is not in this world, and somebody only watching.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | The id every event carries in its `player` field. |
| `walk` | number | `4.8` | m/s on the ground. |
| `sprint` | number | `8` | m/s on the ground with sprint held. |
| `fly` | number | `14` | m/s flying, doubled while sprint is held. |
| `jump` | number | `8` | m/s upwards as a jump leaves the ground. |
| `gravity` | number | `-25` | m/s^2; negative is down, upwards is refused. |
| `rise` | number | `-1` | How far above where it last stood this body may be, in nodes, when it cannot fly; at most 64. Negative is the top of its jump. A kart meets it as a ceiling and bounces off it. |

## `core.body_drive`

```luau
core.body_drive(player: number, accel: number?, brake: number?, turn: number?, drag: number?, width: number?, length: number?) -> number
```

Puts ONE player in a kart, or back on foot: core.drive and core.body_width for a single body, and like core.body_movement it may be called at any time. Returns 1, or 0 when refused with the reason in the log.

The body keeps its speeds -- core.movement's sprint is still its top speed and its walk still the top speed in reverse -- and changes what it IS: four wheels and a chassis `width` by `length` while `accel` is above zero, a body on foot at 0. The player is sent the same message core.body_movement sends, and the server checks them by the new numbers from then on.

A world where everybody drives says core.drive instead, once, and one where you walk up to a kart and get in says this. Refused: the numbers core.drive and core.body_width refuse, and a player who is not in this world.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | The id every event carries in its `player` field. |
| `accel` | number | `0` | m/s^2 with the throttle down. 0 puts this body back on foot. |
| `brake` | number | `0` | m/s^2 braking, before the body reverses. |
| `turn` | number | `0` | rad/s the wheel asks for at full lock, as core.drive. |
| `drag` | number | `0` | 1/s: how fast the speed fades with nothing pressed. |
| `width` | number | `0.6` | Nodes across, as core.body_width. |
| `length` | number | `0` | Nodes nose to tail while it drives; 0 is as long as it is wide. |

## `core.register_item`

```luau
core.register_item(name: string, max_stack: number?, places: string?, dig_level: number?, dig_speed: number?, digs: string?, damage: number?, wear: string?, armor: number?, durability: number?, arms: number?) -> number
```

Declares an item and returns its id. Every registered block already has an item form of its own name, so this is for the things that are NOT blocks -- a tool, a stick, a bucket. Registering the same name again keeps the id, so a hot reload does not empty anybody's inventory.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | Namespaced, like "mymod:pick". |
| `max_stack` | number | `64` | How many fit in one slot. A tool sets 1. |
| `places` | string | `` | The block this puts down, by name. Empty for an item that places nothing. |
| `dig_level` | number | `0` | How hard a block this digs, against NodeDef's dig_level. 0 is what a bare hand does, and it is what an item that is really a block form is: a stack of stone in the hand digs no better than the hand. |
| `dig_speed` | number | `1` | How many times faster than a bare hand this digs, against a block's hardness. One is a bare hand. Zero or less reads as one: a tool that dug in no time at all would be a tool with no rule. |
| `digs` | string | `` | What this tool is FOR, by the same names a block uses in `material`: a pick digs "stone", a shovel digs "soil". Its dig_level and dig_speed are its own against that material and a bare hand's against anything else -- which is the whole of what makes an axe not a pickaxe. Empty is a general tool, worth its level and speed everywhere, and that is what every item that says nothing is. |
| `damage` | number | `0` | How many points a hit takes off a body while this is in the hand. 0 is a fist's, and it is what every item that says nothing hits for. |
| `wear` | string | `` | Where on a body this is worn: "head", "chest", "legs", "feet" or "back", which is the worn square of the bag it goes in. Empty is not worn. |
| `armor` | number | `0` | Armour points while worn. Each takes 4% off a blow, up to 20 (80%); a fall is not armoured. |
| `durability` | number | `0` | How much use it takes before it is gone: a worn piece spends a quarter of every blow, at least 1. 0 never wears out. |
| `arms` | number | `0` | For a piece worn on the back: how many arm squares of the bag it opens while worn, 0 to 2, each holding one tool more (mods/armpack). It cannot come off while they hold anything. |

## `core.register_model`

```luau
core.register_model(name: string, file: string) -> number
```

Declares a model and returns its id. The name is what travels -- a body on the wire carries the id, and the world sends the table of names once -- and the file is where the model is on THIS machine. Registering the same name again keeps the id, so a reload does not undress the bodies wearing it. A client that has not got the model draws the box it drew before, never a hole.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | Namespaced, like "mymod:persona". |
| `file` | string | — | The .gltf, beside this mod's init.luau. What a client CANNOT see: it resolves the name into a file of its own, in its own models folder. |

## `core.model_id`

```luau
core.model_id(name: string) -> number
```

Looks up a model registered by somebody else, as core.node_id looks up a block. Returns 0 if there is no such model, so a mod that dresses a body in another mod's model should declare `depends` in its manifest.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — |  |

## `core.player_model`

```luau
core.player_model(model: number) -> ()
```

What a player's body looks like in this world. One model for everybody: there is nothing yet that makes one player look different from another.

| argomento | tipo | default | note |
|---|---|---|---|
| `model` | number | — | An id from register_model. 0 is a box. |

## `core.body_model`

```luau
core.body_model(player: number, model: number?) -> number
```

What ONE player's body looks like, at any time: core.player_model for a single body, and what a world where you walk up to a kart and get in draws the difference with. Returns 1, or 0 when there is no such player here.

Everybody else sees it at once. What the PLAYER sees of their own body, when the camera is behind it (core.view), is still core.player_model's: the one model the welcome carries.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | The id every event carries in its `player` field. |
| `model` | number | `0` | An id from core.register_model. 0 is a box. |

## `core.view`

```luau
core.view(distance: number?) -> number
```

Where a player's camera sits in THIS world: at the eye, or behind and a little above it -- pulled in when a block is in the way -- with the player's own body drawn, wearing what core.player_model says. For a world's code on a player's machine; the key V switches it too, and a server has no camera to move.

Per world: a crossing into a world that said nothing is the eye again. Returns the distance it was, the key V's included: a scope that goes to the eye puts it back.

| argomento | tipo | default | note |
|---|---|---|---|
| `distance` | number | `0` | Nodes behind the player's head; 0 is the eye. At most 16. |

## `core.iso_view`

```luau
core.iso_view(distance: number?, yaw: number?, pitch: number?, fov: number?, seconds: number?) -> ()
```

A camera that looks down on the player from one fixed direction, as in isometric action RPGs, or (distance 0) back to the eye: the mouse does not turn it, the cursor is free over the world (core.cursor_ray says what is under it) and W A S D walk along the screen's axes, not the body's. The body is drawn and turns to the way it walks. For a world's code on a player's machine; per world, as core.view.

| argomento | tipo | default | note |
|---|---|---|---|
| `distance` | number | `0` | Nodes from the player's head to the camera; 0 turns it off. At most 40. |
| `yaw` | number | `45` | Degrees round from looking down -Z, as a player's yaw. |
| `pitch` | number | `50` | Degrees down from level, 10 to 89. |
| `fov` | number | `0` | Its vertical field of view in degrees, 10 to 100; 0 is the engine's 70. Narrow and far flattens the picture. |
| `seconds` | number | `0` | How long the camera takes to rise out of the eye to there, or back down to it when distance is 0, eased at both ends; 0 is a cut. At most 10. |

## `core.fixed_view`

```luau
core.fixed_view(x: number, y: number, z: number, distance: number?, yaw: number?, pitch: number?, fov: number?, seconds: number?) -> ()
```

A camera that leaves the eye and looks at one point from one fixed direction -- a board on a table, a map on a wall -- or (distance 0) flies back to the eye. core.iso_view's camera, held on the point instead of the head: the cursor is free (core.cursor_ray) and the body stands still, the keys walk nowhere. For a world's code on a player's machine; per world, as core.view.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — | The point the camera looks at, in the world. |
| `y` | number | — |  |
| `z` | number | — |  |
| `distance` | number | `0` | Nodes from the point to the camera; 0 goes back to the eye. At most 40. |
| `yaw` | number | `0` | Degrees round from looking down -Z, as a player's yaw. |
| `pitch` | number | `50` | Degrees down from level, 10 to 89. |
| `fov` | number | `0` | Its vertical field of view in degrees, 10 to 100; 0 is the engine's 70. |
| `seconds` | number | `0` | How long the camera takes to fly from the eye to there, or back, eased at both ends; 0 is a cut. At most 10. |

## `core.cursor_ray`

```luau
core.cursor_ray() -> (number?, number?, number?, number?, number?, number?)
```

The ray under the mouse cursor -- through the middle of the screen, the crosshair, while the game holds the mouse: where it starts (the camera) and its direction, length 1. With core.raycast it finds the cell clicked. NOTHING when there is no camera yet. For a world's code on a player's machine.

## `core.walk`

```luau
core.walk(dx: number?, dz: number?, speed: number?) -> ()
```

Walks this machine's body that way, as if a key were held, and turns it to face where it goes, until said otherwise; dx and dz both 0 stops it and gives the body back to the keys. A click-to-move world calls it every frame towards where it was clicked. For a world's code on a player's machine.

| argomento | tipo | default | note |
|---|---|---|---|
| `dx` | number | `0` | Which way, across the world; any length. |
| `dz` | number | `0` |  |
| `speed` | number | `1` | 0 to 1 of the world's walk; under 1 is for the last steps before a stop. |

## `core.engine_hud`

```luau
core.engine_hud(on: boolean?) -> ()
```

false hides the engine's hotbar, health and armour bars in THIS world, for a world that draws its own (core.hud.pass); the bag still opens. Per world. For a world's code on a player's machine.

| argomento | tipo | default | note |
|---|---|---|---|
| `on` | boolean | `true` |  |

## `core.engine_hud_on`

```luau
core.engine_hud_on() -> boolean
```

Whether the engine's hotbar and bars are drawn in this world now: a mod that draws a bar beside them (hunger, experience) hides it with them, when a world's code has said core.engine_hud(false). True where there is no screen.

## `core.arms_clip`

```luau
core.arms_clip(clip: string) -> ()
```

Plays that clip of the arms seen in first person once, from its start, even when it is already playing -- a shot, a reload, something brought up -- and then leaves the arms to the engine again (idle, a swing, a climb). A name the arms have no clip for does nothing. For a world's code on a player's machine.

| argomento | tipo | default | note |
|---|---|---|---|
| `clip` | string | — | A clip of the first-person arms, by name; "" stops one playing. |

## `core.zoom`

```luau
core.zoom(factor: number?) -> ()
```

Narrows or widens this machine's view: a bow drawn, a spyglass. Called as often as it changes, so a mod eases it in and out. For a world's code on a player's machine.

| argomento | tipo | default | note |
|---|---|---|---|
| `factor` | number | `1` | The field of view over the engine's 70 degrees, 0.2 to 1.5; 1 is back to it. |

## `core.rope`

```luau
core.rope(on: boolean?) -> number
```

Shoots this machine's rope from the eye along the look, up to 40 nodes, into the first solid block, and hangs the body on it: it swings about where it holds, the keys pump the swing, crouch lets rope out and sprint takes it in, jump lets go. false lets go. Only where the world does parkour (core.parkour). Returns 1 on a rope, 0 when nothing was hit or this world has no parkour. For a world's code on a player's machine.

| argomento | tipo | default | note |
|---|---|---|---|
| `on` | boolean | `true` |  |

## `core.held_model`

```luau
core.held_model(name: string?) -> ()
```

What the hand holds in first person, instead of the held item's own model: a bow as it bends while it is drawn. For a world's code on a player's machine.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | `` | A model by name, as register_model; "" is the item's own "<item>_held". |

## `core.world_sky`

```luau
core.world_sky(r: number, g: number, b: number) -> ()
```

A sky that is always this colour and has no sun: the Inferno's, a cave world's. The clock still runs, but nothing is lit by it -- light comes from blocks alone -- and the sky and the fog in the distance are this colour at every hour. Told to every client in the welcome (protocol 57). Call it while the mod loads.

| argomento | tipo | default | note |
|---|---|---|---|
| `r` | number | — | 0..255 |
| `g` | number | — |  |
| `b` | number | — |  |

## `core.world_storm`

```luau
core.world_storm(r: number, g: number, b: number) -> ()
```

A sky of this colour that keeps its sun and its hours -- lit by day, dark by night -- under clouds of the same colour, turning in whirls here and there: a world under a curse. The fog in the distance is the sky's. Told to every client in the welcome (protocol 108). Call it while the mod loads.

| argomento | tipo | default | note |
|---|---|---|---|
| `r` | number | — | 0..255 |
| `g` | number | — |  |
| `b` | number | — |  |

## `core.storm_funnel`

```luau
core.storm_funnel(slot: number, x: number, y: number, z: number, on: boolean?) -> ()
```

Draws a twister in this world's sky: a funnel of the clouds' colour coming down out of them to the ground at x, y, z, wide at the top and narrow at the foot, turning, with dust whirled up round its foot. Up to four at once; false takes that one away. Only drawn, nothing is pulled: what it does is the mod's. Called as it moves. For a world's code on a player's machine.

| argomento | tipo | default | note |
|---|---|---|---|
| `slot` | number | — | 0..3: which of the four |
| `x` | number | — |  |
| `y` | number | — | the ground under it: where its foot is |
| `z` | number | — |  |
| `on` | boolean | `true` |  |

## `core.mapgen_core`

```luau
core.mapgen_core(on: boolean) -> ()
```

false: the engine makes no ground of its own (core:terrain) and the world is whatever the mods' stages (register_mapgen) write into air. The region the engine had already made before the mods loaded is emptied first. Only while the mod loads; afterwards it is refused and said.

| argomento | tipo | default | note |
|---|---|---|---|
| `on` | boolean | — |  |

## `core.mapgen_only`

```luau
core.mapgen_only(mod: string) -> ()
```

Only the stages (register_mapgen) of the mods named this way make this world's ground, one mod per call; the stages of every other mod loaded here are dropped and said. Without a call every stage runs. For a world that loads the sandbox's mods for their items -- the Inferno -- without their lakes, trees and ores. Only while the mod loads.

| argomento | tipo | default | note |
|---|---|---|---|
| `mod` | string | — |  |

## `core.mob_model`

```luau
core.mob_model(model: number) -> ()
```

What a creature looks like. Read when one is spawned, so a mod that declares this after the world is full of mobs has dressed the next one and nobody else.

| argomento | tipo | default | note |
|---|---|---|---|
| `model` | number | — | An id from register_model. 0 is a box. |

## `core.register_mob`

```luau
core.register_mob(name: string, model: number, health: number?, speed: number?, fight: string?, damage: number?, reach: number?, every: number?, drop: string?, drop_count: number?, shot_model: number?, opens_doors: boolean?, hover: number?, fireball: boolean?, spawn_on: string?) -> number
```

A kind of creature. Creatures of every registered kind come out in the dark (light 7 or less) 18 to 40 nodes from a player, up to six near each, and go when no player is within 72, and burn a point a second under the open sky by day. They go for the nearest player within 16; a blow knocks them back, and theirs knock the player back. The model's clips are played by name like a player's -- idle, walk, run, jump_loop, swim -- and, while it swings or draws, "attack" or "shoot". Registering a name again replaces that kind. Returns the kind's number, or 0 if refused. The same in a local world as on a server. Its sounds are files beside the mod's init.luau, named after the kind: <thing>_say.ogg now and then, _hurt, _death, _attack as a blow lands and _shoot as an arrow leaves ("mobs:zombie" -> mods/mobs/zombie_hurt.ogg); one that is missing is silent.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | Namespaced, like "mymod:zombie". |
| `model` | number | — | What it wears: an id from register_model, 0 a box. |
| `health` | number | `20` |  |
| `speed` | number | `2` | m/s walking at a player. |
| `fight` | string | `melee` | "melee": walks up and hits. "archer": keeps its distance and shoots arrows while it can see the player. "pet": fights nobody, comes out by day and is tamed (core.mob_tame). "worker": fights nobody, never comes out or goes on its own; its mod spawns it and drives it (core.mob_goto, core.mob_act). |
| `damage` | number | `2` | A blow, or an arrow. |
| `reach` | number | `1.6` | How close a blow lands; how far an archer shoots. |
| `every` | number | `1` | Seconds between blows or arrows. |
| `drop` | string | `` | An item by name it leaves when it dies. |
| `drop_count` | number | `1` |  |
| `shot_model` | number | `0` | What an archer's arrow wears. |
| `opens_doors` | boolean | `false` | Opens a shut door (core.node_door) on its way instead of going round it. A zombie that does not is a door that keeps it out. |
| `hover` | number | `0` | Flies this high over whoever it goes for, and holds its height with nobody about: no gravity, straight at them, no way round what is in between. 0 walks. |
| `fireball` | boolean | `false` | An archer whose shots fly straight and slower, not falling, and are gone where they land. |
| `spawn_on` | string | `` | Comes out of the dark only standing on this block, by name -- a fortress's bricks. Empty is anywhere. |

## `core.mob_tame`

```luau
core.mob_tame(name: string, item: string, tamed_model: number?) -> number
```

What tames a pet. A pet (register_mob with fight "pet") comes out by day on sunlit ground (spawn_on, if given), up to six near each player, strolls and fights nobody. A player who right clicks it holding this item spends one and it is theirs: it follows them, is put beside them when they get more than 14 away, never goes, is kept in the world file across restarts, and sits (its "sit" clip) or stands up again at their right click. Returns 1, or 0 for a kind nobody registered or one that is not a pet.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | A kind from register_mob, fight "pet". |
| `item` | string | — | The item, by name, that tames it. |
| `tamed_model` | number | `0` | What it wears once tamed (a collar); 0 keeps its own. |

## `core.mob_shear`

```luau
core.mob_shear(name: string, item: string, drop: string, count: number?, shorn_model: number?, regrow: number?) -> number
```

What shears a creature. A player who right clicks it holding this item leaves `drop` on the ground where it stands; it wears its shorn model and cannot be shorn again for `regrow` seconds, and a wild pet panics -- runs from them for a few seconds, as it does from a blow. Returns 1, or 0 for a kind nobody registered.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | A kind from register_mob. |
| `item` | string | — | What shears it, by name. Not spent: a shearing uses one of its durability, if it has any. |
| `drop` | string | — | What it leaves on the ground when shorn, by name. |
| `count` | number | `1` |  |
| `shorn_model` | number | `0` | What it wears until it grows back; 0 keeps its own. |
| `regrow` | number | `300` | Seconds until it can be shorn again. |

## `core.mob_lays`

```luau
core.mob_lays(name: string, item: string, min_s: number, max_s: number) -> number
```

What a creature lays: one on the ground where it stands every min_s to max_s seconds, and one more among its drops when it dies. Returns 1, or 0 for a kind nobody registered.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | A kind from register_mob. |
| `item` | string | — | What it lays, by name. |
| `min_s` | number | — | Seconds between two, at least. |
| `max_s` | number | — | And at most. |

## `core.mob_drop`

```luau
core.mob_drop(name: string, item: string, max: number) -> number
```

One more thing a creature leaves when it dies, beside register_mob's drop: 0 to max of it -- a cow's leather. A young one leaves nothing. Returns 1, or 0 for a kind nobody registered.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | A kind from register_mob. |
| `item` | string | — | What else it leaves, by name. |
| `max` | number | — | 0 to this many of it, 64 at most. |

## `core.mob_sight`

```luau
core.mob_sight(name: string, nodes: number) -> number
```

How far off a creature of that kind notices a player and goes for them. An arena's demons that hunt the whole floor say 64. Returns 1, or 0 for a kind nobody registered.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | A kind from register_mob. |
| `nodes` | number | — | How far, 1 to 128. 16 when never said. |

## `core.shoot`

```luau
core.shoot(x: number, y: number, z: number, dx: number, dy: number, dz: number, max: number, damage: number) -> (number?, boolean?)
```

A bullet: the first body that can be hurt -- a creature, or a mod's entity with core.ecs.health -- that a ray from x, y, z meets before a solid block and within `max` loses `damage`, and a creature is knocked back from where it came. Returns how far along the ray it was struck and whether that killed it, or NOTHING when it struck no body. Never a player. On a server.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `dx` | number | — |  |
| `dy` | number | — |  |
| `dz` | number | — | The direction; it need not be of length 1. |
| `max` | number | — | How far, at most 256. |
| `damage` | number | — | Points it takes off, 1 to 1000. |

## `core.replace_nodes`

```luau
core.replace_nodes(x1: number, y1: number, z1: number, x2: number, y2: number, z2: number, from: number, to: number, round: boolean?) -> number
```

Every cell of `from` in the box (corners included) becomes `to`, as core.set_block would make it -- in a mapgen stage only in the column being made -- and the count comes back. One call where a stage would make one per cell: a host call costs microseconds, and a pocket is hundreds of cells.

| argomento | tipo | default | note |
|---|---|---|---|
| `x1` | number | — |  |
| `y1` | number | — |  |
| `z1` | number | — |  |
| `x2` | number | — |  |
| `y2` | number | — |  |
| `z2` | number | — |  |
| `from` | number | — | The block to replace, by id. |
| `to` | number | — | What it becomes, by id. |
| `round` | boolean | `false` | Only inside the ellipsoid the box holds: a blob of granite, a pocket of gravel. |

## `core.fall`

```luau
core.fall(x: number, y: number, z: number) -> number
```

The block in that cell lets go and falls, as the classic block game's sand does: the cell is air at once, and the block drops through air, water and anything else a body walks through, drawn whole on the way, until something solid stops it. It is put back there, turned as it was, or left as its item when that cell is taken; then core:landed says where and how many nodes it fell. Returns 1, or 0 for air, the floor of the world, or a cell not taking writes. On a server.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.repair_held`

```luau
core.repair_held(player: number, points: number) -> number
```

Gives back up to `points` of the durability the item in a player's hand has spent: an anvil's work. Returns how much it had spent before, 0 for an item that is new or never wears, or -1 for somebody who is not in this world.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `points` | number | — |  |

## `core.shoot_arrow`

```luau
core.shoot_arrow(player: number, speed: number, damage: number, model: number?, item: string?) -> number
```

Looses an arrow from a player's eye the way they are looking, as the classic block game's: it falls and slows, strikes the first creature or other player in its way -- never whoever loosed it -- and stays a minute where it lands, for anybody who walks up to it to pick up as `item`. The item in their hand spends one of its durability, as a bow does. Returns 1, or 0 for somebody who is not in this world. Taking the arrow out of the bag is the mod's (core.take_item).

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `speed` | number | — | m/s as it leaves the string, at most 100. The classic block game's full draw is 60. |
| `damage` | number | — | Points it takes off what it strikes, 1 to 1000. |
| `model` | number | `0` | What it looks like in flight; 0 a box. |
| `item` | string | `` | What it is picked up as once it has landed; "" nothing. |

## `core.blast`

```luau
core.blast(x: number, y: number, z: number, r: number, damage: number) -> number
```

A burst: every creature and every mod entity with health whose middle is within r loses `damage`, less further off, and a creature is thrown away from it. Walls do not shelter it. Returns how many it killed. Never a player. On a server.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `r` | number | — | How far it reaches, at most 32. |
| `damage` | number | — | Points at the centre; half of it at the edge. |

## `core.push_mobs`

```luau
core.push_mobs(x: number, y: number, z: number, dx: number, dy: number, dz: number) -> number
```

Moves every creature whose feet are in that cell by (dx, dy, dz): a piston's push. Returns how many moved.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `dx` | number | — |  |
| `dy` | number | — |  |
| `dz` | number | — |  |

## `core.monsters_near`

```luau
core.monsters_near(x: number, y: number, z: number, r: number) -> number
```

How many creatures that fight -- any kind registered with a fight other than "pet" -- have their feet within r of x, y, z. A bed asks it before letting anybody sleep.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `r` | number | — | How far, in nodes. |

## `core.mobs_near`

```luau
core.mobs_near(x: number, y: number, z: number, r: number) -> { number }
```

Where the creatures that fight (as core.monsters_near counts them) have their feet within r of x, y, z, the nearest first, as a FLAT list x, y, z, x, y, z ...; at most 32. What a click on a creature, or a spell that leaps from one to the next, aims with.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `r` | number | — | How far, in nodes. |

## `core.mob_ids`

```luau
core.mob_ids(x: number, y: number, z: number, r: number) -> { number }
```

The creatures core.mobs_near finds, by id, nearest first; at most 32. An id stays the same creature while it lives, so a mod can follow the one it chose with core.mob_pos.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `r` | number | — | How far, in nodes. |

## `core.mob_pos`

```luau
core.mob_pos(id: number) -> (number?, number?, number?)
```

Where that creature's feet are, or NOTHING once it is dead or gone.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | From core.mob_ids. |

## `core.mob_goto`

```luau
core.mob_goto(id: number, x: number, y: number, z: number) -> number
```

Sends a worker (register_mob with fight "worker") to that cell, along a way core.find_path finds, opening doors if its kind does; it stops in the middle of it. Where there is no way it walks as near as it can and keeps trying: core.mob_going says when it is there. Returns 1, or 0 for one that is gone or not a worker.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | A worker, from core.spawn_mob. |
| `x` | number | — |  |
| `y` | number | — | The cell its feet go in. |
| `z` | number | — |  |

## `core.mob_going`

```luau
core.mob_going(id: number) -> number
```

1 while a worker is still on its way to where core.mob_goto sent it; 0 once it is there, and for one that is gone.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — |  |

## `core.mob_act`

```luau
core.mob_act(id: number, clip: string, item: string?) -> number
```

What a worker is seen doing: its model plays that clip, looping, in place of idle and walk until it is told "", and the item is drawn in its hand as a player's is. Every client near it sees it (protocol 81). Returns 1, or 0 for one that is gone or not a worker.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | A worker, from core.spawn_mob. |
| `clip` | string | — | A clip of its model, by name; "" walks and stands as usual. |
| `item` | string | `` | What it holds, by name; "" nothing. |

## `core.mob_ride`

```luau
core.mob_ride(id: number, thing: number, x: number?, y: number?, z: number?) -> number
```

Carries a worker on a thing, at that place in the thing's frame, put there by the engine every tick right after the machines move (core.craft): a soldier seated in a jeep goes with it in the same tick, where a core.mob_place from the mod comes a tick behind. Its own walking is undone each tick; it still turns (core.mob_face) and acts (core.mob_act). Ends when the thing or the worker is gone. Returns 1, or 0 for no such thing.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | A worker, from core.spawn_mob. |
| `thing` | number | — | A mod's thing (core.ecs.spawn): a machine, or a part mounted on one. 0 lets the worker go. |
| `x` | number | `0` | To the thing's right. |
| `y` | number | `0` | Up, where its feet go. |
| `z` | number | `0` | Ahead of the thing. |

## `core.mob_place`

```luau
core.mob_place(id: number, x: number, y: number, z: number) -> number
```

Puts a worker there at once, stopped and off any errand: what climbs a shaft a step at a time, or brings one back from where its mod lost it. Nothing is checked: the cell is the mod's to have made room in. Returns 1, or 0 for one that is gone or not a worker.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | A worker, from core.spawn_mob. |
| `x` | number | — |  |
| `y` | number | — | Where its feet go. |
| `z` | number | — |  |

## `core.mob_velocity`

```luau
core.mob_velocity(id: number, vx: number, vy: number, vz: number) -> number
```

Sets a worker going at that speed: a thrown thing, flying on and falling as a knock does, and spent on the ground. Off any errand. Through one of the world's own openings it comes out of the other with its speed turned. Returns 1, or 0 for one that is gone or not a worker.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | A worker, from core.spawn_mob. |
| `vx` | number | — |  |
| `vy` | number | — | m/s, up positive. |
| `vz` | number | — |  |

## `core.mob_unseen`

```luau
core.mob_unseen(id: number, player: number) -> number
```

Leaves a worker out of one player's world: everybody else sees it, that player's client is told it is held and draws nothing for it (protocol 91) -- a thing they carry, which the world's code on their machine draws where their eye is, so it does not trail behind their view; held and not gone, so no death plays where it was picked up. Returns 1, or 0 for one that is gone or not a worker.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | A worker, from core.spawn_mob. |
| `player` | number | — | The player whose client is not sent it; 0 sends it to everybody again. |

## `core.mob_remove`

```luau
core.mob_remove(id: number) -> number
```

Takes a worker out of the world at once, leaving nothing and playing no death: its mod taking it back. Returns 1, or 0 for one that is gone or not a worker.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — |  |

## `core.mob_hurt`

```luau
core.mob_hurt(id: number, damage: number, from_x: number, from_z: number) -> number
```

One creature, by id, loses `damage` as core.shoot would take it off and is knocked back: a mod's own bullet that picked its target itself, such as a team's that passes its friends. Returns 2 when that killed it, 1 when struck, 0 for one that is gone or already dead. On a server.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | From core.spawn_mob or core.mob_ids. |
| `damage` | number | — | Points it takes off, 1 to 1000. |
| `from_x` | number | — | Where the blow came from, to knock it away. |
| `from_z` | number | — |  |

## `core.mob_face`

```luau
core.mob_face(id: number, x: number, z: number) -> number
```

Turns a worker to look at x, z: the water it fishes, the furnace it cooks at.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — |  |
| `x` | number | — |  |
| `z` | number | — |  |

## `core.mob_hunt`

```luau
core.mob_hunt(id: number, player: number) -> number
```

Sends a worker after a player, fighting as its kind was registered: it walks up and strikes with its damage, reach and every -- or, for a kind given a shot_model, keeps its distance and shoots. It forgets any errand (core.mob_goto) while it hunts, and stands where it is once called off or once that player is not in this world. A village's guards. Returns 1, or 0 for one that is gone or not a worker.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | A worker, from core.spawn_mob. |
| `player` | number | — | Who it goes for; 0 calls it off. |

## `core.mob_blow_every`

```luau
core.mob_blow_every(id: number, factor: number) -> number
```

How often this one creature strikes or shoots, against the rest of its kind: the weak of a world's first places. For as long as it lives. Returns 1, or 0 for one that is gone. On a server.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | A creature, from core.spawn_mob or core.mob_ids. |
| `factor` | number | — | Of its kind's time between blows, 0.25 to 4: 2 strikes half as often. |

## `core.slow_mobs`

```luau
core.slow_mobs(x: number, y: number, z: number, r: number, factor: number, seconds: number) -> number
```

The creatures that fight within r walk and strike at `factor` of their pace for a while: a frost spell. Returns how many.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `r` | number | — | How far, in nodes. |
| `factor` | number | — | Of their pace, 0.05 to 1: 0.4 is less than half. |
| `seconds` | number | — | For how long, at most 60. |

## `core.mob_breed`

```luau
core.mob_breed(name: string, item: string, baby_model: number?, grow: number?, hearts_model: number?) -> number
```

What breeds a wild pet. A player who right clicks a grown one holding this item spends one and it is in love for 30 s: it goes to another of its kind in love within 8 nodes, and where they meet a young one comes out. Neither is in love again for 300 s. A young one is half the size, keeps near the one that came to it first, leaves nothing when it dies, is not shorn and lays nothing until it grows up. Returns 1, or 0 for a kind nobody registered.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | A kind from register_mob, fight "pet". |
| `item` | string | — | What it is fed to breed, by name. One is spent. |
| `baby_model` | number | `0` | What a young one wears; 0 its kind's own. Its body is half the size either way. |
| `grow` | number | `1200` | Seconds until a young one is grown up. |
| `hearts_model` | number | `0` | Drawn over its head, turning, while it is in love; 0 nothing. |

## `core.spawn_mob`

```luau
core.spawn_mob(name: string, x: number, y: number, z: number) -> number
```

One creature of that kind, standing there. Returns its id -- the same number core.mob_ids and core.mob_pos use, and core:used_mob's target -- or 0 for a kind nobody registered or a world that is not running yet.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | A kind from register_mob. |
| `x` | number | — |  |
| `y` | number | — | Where the feet go. |
| `z` | number | — |  |

## `core.ui_define`

```luau
core.ui_define(name: string, w: number, h: number) -> number
```

Declares a panel, empty. Declaring the same name again starts it empty again, which is what a reload wants. Returns 1, or 0 with the reason in the log. A panel belongs to the mod that declared it and no other mod may touch it.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | Namespaced, like a block: 'mymod:menu'. |
| `w` | number | — | Width in cells. A cell is a hotbar slot; a panel is at most 30 across. |
| `h` | number | — | Height in cells. |

## `core.ui_label`

```luau
core.ui_label(panel: string, x: number, y: number, text: string) -> number
```

Puts text on a panel. A label is not clickable: a caption that answered clicks would send this mod events for something it never made clickable.

| argomento | tipo | default | note |
|---|---|---|---|
| `panel` | string | — |  |
| `x` | number | — | Cells from the panel's left edge. |
| `y` | number | — | Cells from its top edge. |
| `text` | string | — |  |

## `core.ui_item`

```luau
core.ui_item(panel: string, id: string, x: number, y: number, w: number, h: number, item: string) -> number
```

Puts an item's picture on a panel, in a square like a slot of the bag, with its name as a tooltip under the pointer. Clicking it emits core:ui_click with this id, as a button does.

| argomento | tipo | default | note |
|---|---|---|---|
| `panel` | string | — |  |
| `id` | string | — | What core:ui_click carries back. One without it is refused. |
| `x` | number | — |  |
| `y` | number | — |  |
| `w` | number | — |  |
| `h` | number | — |  |
| `item` | string | — | The item whose picture it shows, by name. |

## `core.ui_model`

```luau
core.ui_model(panel: string, x: number, y: number, w: number, h: number, model: number, tint: number?, pieces: string?) -> number
```

Puts a model on a panel, turning slowly in a box of its own, lit and dressed as everybody would see it: a preview of what a panel changes (track_game's garage). Not clickable.

| argomento | tipo | default | note |
|---|---|---|---|
| `panel` | string | — |  |
| `x` | number | — |  |
| `y` | number | — |  |
| `w` | number | — |  |
| `h` | number | — |  |
| `model` | number | — | A model from register_model (core.model_id). |
| `tint` | number | `0` | Its tint_1..3 colours and its pieces', five bits each, as core.player_wear's. |
| `pieces` | string | `` | Model ids, space apart, drawn on its bones as worn pieces; one named for it ("<model>_<part>_<variant>") takes the place of its bones of that part. |

## `core.ui_button`

```luau
core.ui_button(panel: string, id: string, x: number, y: number, w: number, h: number, text: string) -> number
```

Puts a button on a panel. Clicking it emits core:ui_click with the panel's name and this id.

| argomento | tipo | default | note |
|---|---|---|---|
| `panel` | string | — |  |
| `id` | string | — | What core:ui_click carries back. A button without one is refused. |
| `x` | number | — |  |
| `y` | number | — |  |
| `w` | number | — |  |
| `h` | number | — |  |
| `text` | string | — |  |

## `core.ui_field`

```luau
core.ui_field(panel: string, id: string, x: number, y: number, w: number, h: number, text: string) -> number
```

Puts a line of text the player may edit on a panel. A click puts the keyboard in it -- the game's keys stop answering until Enter, Esc or a click elsewhere -- and Enter emits core:ui_click with this id and what it holds in `text`. Whatever a player sends is theirs: a mod reading it reads untrusted text, up to 128 bytes.

| argomento | tipo | default | note |
|---|---|---|---|
| `panel` | string | — |  |
| `id` | string | — | What core:ui_click carries back. A field without one is refused. |
| `x` | number | — |  |
| `y` | number | — |  |
| `w` | number | — |  |
| `h` | number | — |  |
| `text` | string | — | What it holds before anybody types. At most 128 bytes. |

## `core.ui_box`

```luau
core.ui_box(panel: string, x: number, y: number, w: number, h: number, r: number?, g: number?, b: number?) -> number
```

A rectangle of colour, for grouping. Not clickable, for the same reason a label is not.

| argomento | tipo | default | note |
|---|---|---|---|
| `panel` | string | — |  |
| `x` | number | — |  |
| `y` | number | — |  |
| `w` | number | — |  |
| `h` | number | — |  |
| `r` | number | `60` |  |
| `g` | number | `62` |  |
| `b` | number | `70` |  |

## `core.ui_show`

```luau
core.ui_show(name: string, open: boolean?, player: number?) -> number
```

Puts a panel on one player's screen, or takes it off. `player` is the id the events carry -- 0 means every player in the world, which is a server notice and not somebody's chest. One panel is drawn at a time PER PLAYER: the one shown to them most recently. Only its own mod may open or close it.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — |  |
| `open` | boolean | `true` |  |
| `player` | number | `0` |  |

## `core.container_make`

```luau
core.container_make(x: number, y: number, z: number, slots: number) -> number
```

The container at that cell exists, made with that many slots if it was not there. Nobody is shown anything. Making a chest and looking into one are two acts and this is the first: a loot chest filled before the world is opened, or a dispenser stocked by the mod that placed it, has no player to show it to. An existing container is never made smaller; asking for more slots than it has grows it, every stack staying in its slot -- how a chest becomes half of a double chest.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `slots` | number | — |  |

## `core.container_add`

```luau
core.container_add(x: number, y: number, z: number, item: number, count: number, role: string?) -> number
```

Puts items into the container at that cell, filling partial stacks of the same item before taking an empty slot. Returns what did NOT fit, so a full chest hands the remainder back rather than swallowing it -- and a cell with no container hands back all of it. Anybody looking into the chest sees the change. What a block makes goes in with role "product": nothing else puts things there.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `item` | number | — |  |
| `count` | number | — |  |
| `role` | string | `` | Only into squares of that role (core.container_roles): "store", "raw", "fuel", "product". None: any square but a product one. |

## `core.container_roles`

```luau
core.container_roles(id: number, roles: string) -> number
```

What each square of a container in a cell of this block is for. "raw" (materia prima) what it works on, "fuel" (materiale di consumo) what it burns doing it, "product" (prodotto finito) what it made, "store" (magazzino) anything. "source" a catalogue's square: a player takes a copy and it keeps its own, and what is put into it is gone; "trash" a bin: what is put into it is gone. A carried container (y at or below -1073741824) takes the roles of the block whose id is its z. Machinery goes by it: things are taken out of only store and product squares (core.container_drop, core.container_move's source, a mod's extractor) and put into any but product ones (container_add with no role, container_move's target, collect_drops, a player's shift-click); and nobody puts anything into a product square by hand. A player sees each square tinted by its role. Returns 1, or 0 for an id that is not a block or a word that is not a role.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block, by the id register_node returned. |
| `roles` | string | — | One word a square, in order, split by spaces; squares past the last are "store". |

## `core.container_accepts`

```luau
core.container_accepts(id: number, role: string, items: string) -> number
```

What the squares of that role in a container of this block take: a furnace's ore row only what smelts, its coal row only what burns. Nothing else goes in -- not by a player's hand, a shift-click, a hopper or core.container_add. What is already there stays. Returns 1, or 0 for an id that is not a block or a role that is not one.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block, by the id register_node returned. |
| `role` | string | — | "store", "raw", "fuel" or "product". |
| `items` | string | — | Item names split by spaces; "" takes anything again. A name nobody registered is kept and matches nothing. |

## `core.container_progress`

```luau
core.container_progress(x: number, y: number, z: number, seconds: number) -> number
```

Something going on in the container at that cell -- a furnace smelting -- that takes that long from now: whoever has it open sees a bar under its squares fill over those seconds, full at the end. Returns 1, or 0 for a cell with no container.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `seconds` | number | — | How long it takes from now. 0: nothing going on. |

## `core.container_scroll`

```luau
core.container_scroll(x: number, y: number, z: number, pos: number, size: number) -> number
```

A scrollbar beside the container at that cell, for a catalogue longer than its squares (mods/creative): whoever has it open sees where in the list they are. Returns 1, or 0 for a cell with no container.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `pos` | number | — | Where the squares on show start in the whole list, 0..1. |
| `size` | number | — | How much of the list they are, 0..1. 0: no scrollbar. |

## `core.container_search`

```luau
core.container_search(x: number, y: number, z: number, text: string) -> number
```

A search box beside the container at that cell (mods/creative). What a player types in it comes back on every key as core:ui_click with panel "core:chest", id "search" and the text. Gone with the container (core.container_clear). Returns 1, or 0 for a cell with no container.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `text` | string | — | What the box holds. |

## `core.node_keeps_drops`

```luau
core.node_keeps_drops(id: number, on: boolean?) -> number
```

A stack lying on this block does not grow older, and is not gone after five minutes as one on the ground is -- a sorter, where things wait their turn. One on a belt (core.node_conveyor) never grows older anyway. Returns 1, or 0 for an id that is not a block.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | The block, by the id register_node returned. |
| `on` | boolean | `true` |  |

## `core.container_show`

```luau
core.container_show(x: number, y: number, z: number, slots: number, player: number) -> number
```

Opens the container at that cell on one player's screen, making it if there is not one. An existing container is never made smaller, and grows when asked for more (see container_make). The chest is at the CELL and not in the player: two people can have it open at once and both reach into it. `player` is the id the events carry, and 0 is nobody -- there is no chest for the world.

A cell at y -1073741824 or below is not in the world: a container there is CARRIED and never closes for being out of reach -- how mods/backpack keeps one per player.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `slots` | number | — |  |
| `player` | number | — |  |

## `core.container_close`

```luau
core.container_close(player: number) -> number
```

Takes whatever container that player has open off their screen. What is in it stays where it is, and anybody else looking at it goes on looking at it.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |

## `core.container_count`

```luau
core.container_count(x: number, y: number, z: number, item: number, role: string?) -> number
```

How many of an item the container at that cell holds, across every slot. Zero for a cell with no container, which is also the answer for a container that holds none.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `item` | number | — |  |
| `role` | string | `` | Only squares of that role (core.container_roles). None: every square. |

## `core.container_fill`

```luau
core.container_fill(x: number, y: number, z: number) -> number
```

How full the container at that cell is, 0 to 1: each slot counts as its stack over the most that item stacks to, as the classic block game's comparator reads a chest. -1 for a cell with no container.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.container_revision`

```luau
core.container_revision(x: number, y: number, z: number) -> number
```

A number that changes whenever what is in the container at that cell changes, by hand or by a mod; 0 for no container. A comparator watches it.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.container_move`

```luau
core.container_move(fx: number, fy: number, fz: number, tx: number, ty: number, tz: number, count: number?) -> number
```

Moves up to `count` of the first stack in one container into another, as it is -- worn or enchanted -- the way a hopper passes things on. Returns how many moved.

| argomento | tipo | default | note |
|---|---|---|---|
| `fx` | number | — |  |
| `fy` | number | — |  |
| `fz` | number | — |  |
| `tx` | number | — |  |
| `ty` | number | — |  |
| `tz` | number | — |  |
| `count` | number | `1` |  |

## `core.drop_at`

```luau
core.drop_at(x: number, y: number, z: number) -> number
```

The item id of the first stack lying in that cell, or 0 for none: what a sorter looks at before it decides where a stack goes (core.item_name names it).

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.drop_send`

```luau
core.drop_send(x: number, y: number, z: number, tx: number, ty: number, tz: number) -> number
```

Moves the first stack lying in cell (x, y, z) -- the one core.drop_at names -- to a point, as it is (worn, enchanted), falling from there onto whatever is under it. Returns its item id, or 0 when the cell had none.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `tx` | number | — | Where it goes: a point, not a cell. |
| `ty` | number | — |  |
| `tz` | number | — |  |

## `core.container_item`

```luau
core.container_item(x: number, y: number, z: number, slot: number) -> number
```

The item id in one slot of the container at that cell, or 0 for an empty slot, no such slot or no container: a filter a player set by putting a thing in a square.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `slot` | number | — | From 1. |

## `core.container_drop`

```luau
core.container_drop(x: number, y: number, z: number, tx: number, ty: number, tz: number, count: number?) -> number
```

Takes up to `count` of the first stack in the container at that cell and lays it on the ground at a point, as it is (worn, enchanted), falling from there -- what loads a belt out of a chest. Nobody may pick it up for a second. Returns how many.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `tx` | number | — | Where it goes: a point, not a cell. |
| `ty` | number | — |  |
| `tz` | number | — |  |
| `count` | number | `1` |  |

## `core.collect_drops`

```luau
core.collect_drops(x: number, y: number, z: number, tx: number, ty: number, tz: number) -> number
```

Puts the stacks lying in cell (x, y, z) into the container at (tx, ty, tz), as much of each as fits: a hopper taking in what falls on it. Returns how many items went in.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `tx` | number | — |  |
| `ty` | number | — |  |
| `tz` | number | — |  |

## `core.container_take`

```luau
core.container_take(x: number, y: number, z: number, item: number, count: number?, role: string?) -> number
```

Takes up to that many of an item out of the container at that cell, from the first slot on, and returns how many it took. Zero for a cell with no container. What a block that uses what is put in it -- a furnace eating its fuel -- needs, and what container_clear, which takes everything, is not.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `item` | number | — |  |
| `count` | number | `1` |  |
| `role` | string | `` | Only out of squares of that role (core.container_roles). None: any square. |

## `core.container_spill`

```luau
core.container_spill(x: number, y: number, z: number, from: number?) -> number
```

Everything in the container at that cell, on the ground where the cell is. Returns how many stacks fell. The container stays and is left EMPTY -- spilling and taking the chest away are two acts, so a mod that is breaking the block spills first and clears after. A stack on the ground is gone in five minutes, like any other drop.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `from` | number | `0` | The first slot that falls. Past 0 the container is also cut to that many slots -- how half of a double chest goes back to a chest. |

## `core.drop_item`

```luau
core.drop_item(x: number, y: number, z: number, item: number, count: number) -> number
```

One stack, on the ground at the middle of that cell, falling from there. Nobody may pick it up for a second and it is gone in five minutes. Returns 1 if it was dropped. This is what a dispenser is made of, and what a mod uses to put the contents of something it is destroying where a player can get them back.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `item` | number | — |  |
| `count` | number | — |  |

## `core.craft_show`

```luau
core.craft_show(x: number, y: number, z: number, player: number) -> number
```

Makes that player's craft grid a three-by-three, at that cell -- a crafting table. Their own grid is two-by-two and always there, in the corner of their bag; this is what standing at a table changes it to, and walking out of reach changes it back. The grid is EMPTIED both ways: the same nine squares are read as a 2x2 or a 3x3, so anything left in one would come out of the resize somewhere else. What was in it goes back in the bag, and what will not fit falls on the ground.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `player` | number | — |  |

## `core.craft_close`

```luau
core.craft_close(player: number) -> number
```

Back to the two-by-two in that player's own bag, emptying it the same way. What a player does by pressing escape; a mod does it when whatever it was they were standing at stops being a table.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |

## `core.damage`

```luau
core.damage(player: number, amount: number) -> number
```

Takes that many points off that player, and kills them if it empties them. Returns what they have LEFT, so a zero is how a mod knows it was the killing blow -- there is no second answer to read. A player who dies comes back at the world's spawn with a full bar and their bag: dying costs the place and not what they were carrying. Nothing happens to somebody who is only watching.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `amount` | number | — |  |

## `core.heal`

```luau
core.heal(player: number, amount: number) -> number
```

Gives points back, never above the maximum. Returns what they have after. A body left alone heals itself anyway, slowly; this is for whatever a mod thinks should be faster than that.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `amount` | number | — |  |

## `core.health`

```luau
core.health(player: number) -> number
```

What that player has left, without changing it. Zero for somebody who is not here and for somebody who is only watching -- both of which are 'no body', not 'a dead one'.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |

## `core.container_clear`

```luau
core.container_clear(x: number, y: number, z: number) -> number
```

The container at that cell, gone, and what was in it with it. For a block that stopped being a chest. What was inside is NOT dropped: a mod that wants it on the ground takes it out first.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.recipe_shaped`

```luau
core.recipe_shaped(makes: string, count: number, row1: string, row2: string?, row3: string?) -> number
```

A recipe with a shape. Each row is item names separated by spaces, and `-` is a cell that must be EMPTY -- a hole in the middle of a pattern is part of the shape. The pattern matches ANYWHERE in the grid, so it is written where it is convenient rather than in a corner; its mirror is a different shape. A row left short is padded with holes. Registering the same shape twice is refused, unless it is your own recipe coming back through a reload.

| argomento | tipo | default | note |
|---|---|---|---|
| `makes` | string | — |  |
| `count` | number | — |  |
| `row1` | string | — |  |
| `row2` | string | `` |  |
| `row3` | string | `` |  |

## `core.recipe_shapeless`

```luau
core.recipe_shapeless(makes: string, count: number, items: string) -> number
```

A recipe with no shape: those things, that many cells, anywhere in the grid. Names separated by spaces, and naming one twice means two cells of it. The same things in another order are the same recipe and are refused as one.

| argomento | tipo | default | note |
|---|---|---|---|
| `makes` | string | — |  |
| `count` | number | — |  |
| `items` | string | — |  |

## `core.recipe_dye`

```luau
core.recipe_dye(item: string) -> number
```

Makes a thing take colours at a table: it and one to three dyes (core.item_dye), anywhere in the grid, make the same one again -- its tag kept, so a thing with contents keeps them -- with its first colours those dyes in reading order and the rest as they were. Three colours a thing, each one of the sixteen; its models draw them on materials named tint_1, tint_2 and tint_3, and a slot shows them as three marks. Returns 1.

| argomento | tipo | default | note |
|---|---|---|---|
| `item` | string | — |  |

## `core.item_dye`

```luau
core.item_dye(item: string, dye: number) -> number
```

Makes an item a dye for core.recipe_dye. Returns 1, or 0 for a dye out of range.

| argomento | tipo | default | note |
|---|---|---|---|
| `item` | string | — |  |
| `dye` | number | — | Which of the sixteen, 1 to 16: white, orange, magenta, light blue, yellow, lime, pink, gray, light gray, cyan, purple, blue, brown, green, red, black. |

## `core.find_nodes`

```luau
core.find_nodes(x1: number, y1: number, z1: number, x2: number, y2: number, z2: number, name: string) -> { number }
```

Every cell in that box holding that block, as a FLAT list of numbers: x, y, z, x, y, z. `#found // 3` is how many there are.

Flat and not a list of tables because a table per cell is an allocation per cell, on the one call a mod makes over a whole region.

This is what a mod uses to find cells a PLAYER placed -- the original engine's equivalent is an LBM. A mod that put its own blocks down could as easily have remembered where they went; this is for the ones it did not put down.

Two limits, both stated rather than hidden. At most 512 positions come back, so a search that returns exactly 512 has probably found more and should be run over a smaller box. And a box of more than a million cells is REFUSED -- nothing comes back and the log says so -- because the search happens inside the tick and the interrupt budget that stops a runaway mod does not count time spent in the engine.

| argomento | tipo | default | note |
|---|---|---|---|
| `x1` | number | — |  |
| `y1` | number | — |  |
| `z1` | number | — |  |
| `x2` | number | — |  |
| `y2` | number | — |  |
| `z2` | number | — |  |
| `name` | string | — | A block name, like "core:stone". |

## `core.find_path`

```luau
core.find_path(x1: number, y1: number, z1: number, x2: number, y2: number, z2: number, height: number?, step_up: number?, drop: number?, swim: boolean?, max_nodes: number?, hurry: number?, smooth: boolean?) -> { number }
```

A way a body that walks could take from one cell to another, as a FLAT list: x, y, z of every cell its feet pass through after the first, the last being where it ends. Both ends are first dropped onto the ground under them. Moves go to the eight cells around, level, up to `step_up` higher or `drop` lower, never through the corner of a wall. With no way, or when `max_nodes` runs out, the list is the way to the cell that came CLOSEST -- check whether it ends where you asked. Empty when the start has no ground under it. The creatures chase players with this (core/voxel/pathfind.h), hurry 1.5 and smooth; about 0.35 us per cell looked at.

| argomento | tipo | default | note |
|---|---|---|---|
| `x1` | number | — |  |
| `y1` | number | — |  |
| `z1` | number | — |  |
| `x2` | number | — |  |
| `y2` | number | — |  |
| `z2` | number | — |  |
| `height` | number | `2` | Cells of room the body needs, feet up. |
| `step_up` | number | `1` | Cells it climbs in one move. |
| `drop` | number | `3` | Cells it will fall. |
| `swim` | boolean | `true` | Whether water holds it up; if not, water is a wall. |
| `max_nodes` | number | `2000` | Cells looked at before giving up, at most 20000. |
| `hurry` | number | `1` | 1 is the shortest way. More gives up shortness for speed: 1.5 looks at about a twelfth of the cells for a way a few percent longer. At most 4. |
| `smooth` | boolean | `false` | Only the corners: the cells where walking straight on from the last one would leave the level or cross a cell the body cannot stand in. For a body that steers at points rather than stepping cell by cell. |

## `core.players`

```luau
core.players() -> { number }
```

Everybody who is in this world with a body, as a list of the ids every event carries in its `player` field. An observer -- a portal looking in -- is NOT in it: DESIGN.md 4.4 gives them no avatar and no inventory, so there is nobody there to stand on a button or be handed anything.

The list is this moment's. Somebody in it may have left by the time the mod asks where they are, which is why core.player_pos answers with nothing rather than with the origin.

## `core.player_pos`

```luau
core.player_pos(player: number) -> (number?, number?, number?)
```

Where that player's FEET are -- the same position the server's movement authority accepted, not a claim a client made. Their eyes are about 1.6 above it and their box is half a node wide.

Returns NOTHING for a player who is not in this world, so `local x, y, z = core.player_pos(id)` gives nil and not 0 0 0 -- the origin is a real place, and a mod that measured a distance to it would find everybody standing on a button in the corner of the world.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |

## `core.player_key`

```luau
core.player_key(player: number) -> string
```

The fingerprint of the key that player proved at the door, or an empty string for somebody who came without one (or is not here). On a world with a file a name belongs to the key that first joined under it, so a name with a key behind it is an identity; a mod that hands out something to keep -- land, say -- asks for one.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |

## `core.world_spawn`

```luau
core.world_spawn() -> (number?, number?, number?)
```

Where a player arrives in this world, and comes back after dying without a bed.

## `core.player_name`

```luau
core.player_name(player: number) -> string
```

The name that player joined with, or an empty string for somebody who is not in this world. A label and not an identity -- two players may pick the same one -- but it is what a mod can show on a screen, and what it can keep a lap time under across a rejoin: the id is a connection's, and a rejoin is a new connection.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |

## `core.player_carry`

```luau
core.player_carry(player: number, text: string) -> number
```

A line this world keeps about a player's body, on a server of several rooms: when the player walks into an opening that leads to another room of the same server, the line goes with them and that room reads it with core.player_carried -- a lap in progress, or by which way out they left. A body that drives goes too, still driving, at its speed, into a room that knows the model it wears. In memory only. Returns 0 for a player with no body here, or a line too long.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |
| `text` | string | — | At most 256 bytes; empty forgets it. |

## `core.player_carried`

```luau
core.player_carried(player: number) -> string
```

The line core.player_carry keeps about that player's body -- here, or brought along from the room it drove in from -- or an empty string.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |

## `core.body_taken`

```luau
core.body_taken(player: number) -> number
```

1 when that player's body left this world with them: it drove into another room of this server, which took it (see core.player_carry), and what is left here is a window onto this world. 0 for a player who walked out on foot, left the server or is still here. For a mod that puts a kart back where a driver disappeared: not when the kart went too.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |

## `core.player_look`

```luau
core.player_look(player: number) -> (number?, number?, number?)
```

Which way that player is looking, as a direction of length 1. On a server it is the look that came with their last ACCEPTED position, so a shot a mod fires along it goes where the server believes and not where a client says. Their eye is core.player_pos plus 1.62.

Returns NOTHING for a player who is not in this world or has not moved yet.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |

## `core.player_launch`

```luau
core.player_launch(player: number, vx: number, vy: number, vz: number, seconds: number?) -> number
```

Throws a player: their body's speed becomes that, whatever it was (protocol 84), and for `seconds` the server lets them move as fast and as high as a crossing's grace does -- an launch plate. Returns 1, or 0 for a player with no body here or a speed out of range.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `vx` | number | — |  |
| `vy` | number | — | m/s, up positive; at most 16 on any axis. |
| `vz` | number | — |  |
| `seconds` | number | `2` | How long the server lets the body fly faster and higher than it could, 0..10. |

## `core.player_place`

```luau
core.player_place(player: number, x: number, y: number, z: number, look_x: number?, look_y: number?, look_z: number?) -> number
```

Moves a player's body and turns it, the way a portal does: the server puts them there and gives them a second in which their movement is not checked, for the frames before their client knows. Getting into a parked kart is this call -- the body goes where the kart stood, facing the way it faced. Returns 1, or 0 for a player who is not in this world or somebody only watching.

On a server the move goes out at the end of the tick, after the blocks the mods changed in it: a body put where a block was just taken away arrives to find it gone. Nothing checks the place is clear: put a body where it fits.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |
| `x` | number | — | Where the feet go. |
| `y` | number | — |  |
| `z` | number | — |  |
| `look_x` | number | `0` | Which way they face afterwards; any length. All three 0 keeps the look they have. |
| `look_y` | number | `0` |  |
| `look_z` | number | `0` |  |

## `core.player_lie`

```luau
core.player_lie(player: number, on: boolean, x: number?, y: number?, z: number?, head_x: number?, head_z: number?) -> number
```

Lays a player on their back, as in a bed: the body is put there as core.player_place puts it, drawn lying with its head the way (head_x, head_z) says -- to them and to everybody -- and their eye goes down to the pillow. They look about as they like. Walking off gets them up on their own screen; the mod says so to everybody else with on = false, which is also how a mod gets them up. Returns 1, or 0 for a player who is not in this world, is carried (core.ride), or somebody only watching.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |
| `on` | boolean | — | true lays them down; false gets them up where they are. |
| `x` | number | `0` | The middle of the body, on what it lies on: y is that surface. |
| `y` | number | `0` |  |
| `z` | number | `0` |  |
| `head_x` | number | `0` | Which way the head points, level; any length. |
| `head_z` | number | `0` |  |

## `core.player_clip`

```luau
core.player_clip(player: number, clip: string?) -> number
```

Has a body play a clip until told otherwise, "" to stop: holding cards at a table. On a server everybody else sees the body play it, over the walk and the idle; on a player's machine (only their own id) the arms seen in first person play the clip of the same name in the player model's _arms, and go on holding it while the camera is on a point at the eye (core.fixed_view with a distance under 0.1). Returns 1, or 0 for a player who is not here.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |
| `clip` | string | `` | A clip of the player model (and of its _arms), at most 32 bytes; "" for none. |

## `core.player_wear`

```luau
core.player_wear(player: number, slot: number, model: number?, tint: number?) -> number
```

Dresses a body in a model of a mod's own, drawn on their skeleton bone by bone as a worn piece is, for everybody to see (protocol 102): a sword bound whole to hand_R, a helm to head. Up to ten, apart from what the engine's bag has them wear; they stay until taken off or the player leaves. On a server everybody else sees them; on a player's machine (only their own id) its own drawing of the body wears them. Returns 1, or 0 for a player who is not here, a slot out of range or a model nobody registered.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | An id from core.players, or from an event. |
| `slot` | number | — | 1 to 10: which of the pieces a mod dresses them in. |
| `model` | number | `0` | A model from register_model, made on the player model's rig; 0 takes the piece off. |
| `tint` | number | `0` | Its tint_1..3 colours, five bits each from the lowest, 0 for the model's own or 1..16 a dye. |

## `core.play_sound`

```luau
core.play_sound(name: string, x: number, y: number, z: number) -> number
```

Plays a sound at a place, for everybody in this world, quieter with distance and heard through a portal the way round. Each client turns the name into a file of its own: mods/<mod>/<thing>.ogg (or .wav) beside the mod's init.luau, then assets/sounds/<mod>_<thing>.ogg. A client without the file hears nothing. Returns 1, or 0 for a name that is not "mod:thing".

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | "mymod:bell": lower case, digits and _, one colon. |
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |

## `core.raycast`

```luau
core.raycast(x: number, y: number, z: number, dx: number, dy: number, dz: number, max: number?) -> (number?, number?, number?, number?)
```

The first solid block a ray from x, y, z along dx, dy, dz runs into within `max`: its cell and how far along the ray. Returns NOTHING when it meets none.

Blocks only. A body or a mod's entity is the mod's to test, against the distance this returns: a target behind a wall is further than the wall.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `dx` | number | — |  |
| `dy` | number | — |  |
| `dz` | number | — | The direction; it need not be of length 1. |
| `max` | number | `64` | How far to look, at most 256. |

## `core.held_item`

```luau
core.held_item(player: number) -> string
```

The item in the square of the hotbar a player has chosen, by name, or an empty string for an empty hand or somebody who is not in this world. What they would click with now -- a rod whose line is out, put away, is a line to reel in.

On a player's machine, world code included, it answers for that machine's own player (id 1) and nobody else: what the hand already shows there.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |

## `core.worn_item`

```luau
core.worn_item(player: number, square: string) -> string
```

What a player wears in one worn square of their bag, by name, or an empty string for an empty square, a square that is not one of the five, or somebody who is not in this world.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `square` | string | — | "head", "chest", "legs", "feet" or "back": the worn square asked about. |

## `core.worn_id`

```luau
core.worn_id(player: number, square: string) -> number
```

Which one a worn thing is: the number a mod gave it with worn_set_id, 0 for one nobody numbered, an empty square or somebody not here. The number goes wherever the thing goes -- a chest, the ground, another player -- and is kept when it is dyed. mods/backpack keeps a backpack's contents under it.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `square` | string | — |  |

## `core.worn_set_id`

```luau
core.worn_set_id(player: number, square: string, id: number) -> number
```

Numbers a worn thing, its colours kept. Returns 1, or 0 for an empty square, somebody not here, or a number out of range.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `square` | string | — |  |
| `id` | number | — | 1 to 131071; 0 takes the number off. |

## `core.player_fly`

```luau
core.player_fly(player: number, allowed: boolean?) -> number
```

Whether one player may fly. Not allowed, their flying speed is kept aside and they are sent a speed of 0: F does nothing and a player in the air falls. Allowed again, the speed they had comes back. It holds across core.body_movement: while a player may not fly, the flying speed a mod gives them is the one kept aside. In memory only, like body_movement. Returns 1, or 0 for somebody with no body here.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `allowed` | boolean | `true` |  |

## `core.mob_attack`

```luau
core.mob_attack(id: number, mob: number) -> number
```

Sends a worker after another creature, fighting as core.mob_hunt does: blows, or for a kind given a shot_model arrows that strike creatures. A player it is hunting comes first; it lets go once the creature is dead. The watch of a village against what comes out of the dark. Returns 1, or 0 for one that is gone or not a worker.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — | A worker, from core.spawn_mob. |
| `mob` | number | — | The creature it goes for, from core.mob_ids; 0 calls it off. |

## `core.mob_pierce`

```luau
core.mob_pierce(name: string, part: number) -> number
```

How much of a kind's blows and arrows armour does not take: 0.6 of a guard's sword goes through a diamond chestplate as if it were not there. Call it after register_mob, which forgets it. Returns 1, or 0 for a kind nobody registered.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | A kind, as register_mob named it. |
| `part` | number | — | 0 to 1. |

## `core.owned_area`

```luau
core.owned_area(x1: number, z1: number, x2: number, z2: number, tag: number) -> number
```

Ground that belongs to somebody, every height of it: what a player takes from a container there, what they break there and what a container broken there spills comes out with this number as its tag (core.worn_id's), and so does not stack with the same thing come by honestly. A village's goods, marked stolen. In memory only: a mod says it again after the world loads. The same corners again change the number. Returns 1, or 0 for a number out of range.

| argomento | tipo | default | note |
|---|---|---|---|
| `x1` | number | — |  |
| `z1` | number | — |  |
| `x2` | number | — |  |
| `z2` | number | — |  |
| `tag` | number | — | 1 to 131071; 0 takes the area away. |

## `core.owned_cell`

```luau
core.owned_cell(x: number, z: number, tag: number) -> number
```

One column that belongs to somebody, as core.owned_area's ground, and before any area: a player's territory grown a cell at a time, or their lot inside a village. In memory only. Returns 1, or 0 for a number out of range.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `z` | number | — |  |
| `tag` | number | — | 1 to 131071; 0 takes the column away. |

## `core.owned_at`

```luau
core.owned_at(x: number, z: number) -> number
```

Whose a column is: the number core.owned_cell or core.owned_area gave it, a cell before an area, or 0 for nobody's.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `z` | number | — |  |

## `core.owned_member`

```luau
core.owned_member(player: number, tag: number, on: boolean) -> number
```

Makes a player one of the owners of the ground with that number: what they take, break or open there is not marked. Until they leave; a mod says it again when they come back. Returns 1, or 0 for nobody.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `tag` | number | — | 1 to 131071. |
| `on` | boolean | — | false: no longer one of its owners. |

## `core.player_factors`

```luau
core.player_factors(player: number, walk: number, dig: number, hurt: number, regen: number) -> number
```

How this player moves, digs, takes blows and heals against everybody else, 1 for as they are: on top of core.body_movement, core.player_regen and their tools, so a mod that slows them for a moment does not undo it. Their client is told walk and dig. In memory only. Returns 1, or 0 for nobody or a number out of range.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `walk` | number | — | Walking and sprinting, 0.25 to 4. |
| `dig` | number | — | Digging, 0.1 to 10. |
| `hurt` | number | — | Blows taken, after armour, 0 to 10. |
| `regen` | number | — | Health coming back, 0 (none) to 10. |

## `core.swap_item`

```luau
core.swap_item(player: number, from: string, to: string, n: number) -> number
```

What a broken block turns into in the bag (stone to cobblestone), keeping the stack's tag: something stolen stays stolen. Returns how many of `to` fit -- the rest is the mod's to drop -- or -1 when there was no `from` (or nobody).

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `from` | string | — | One of it comes out, a marked stack first. |
| `to` | string | — |  |
| `n` | number | — | How many of `to` go in, 1 to 64. |

## `core.bag_tagged`

```luau
core.bag_tagged(player: number, tag: number) -> { number }
```

What a player carries with that tag, as a FLAT list item id, count, item id, count ... (core.item_name names each), a stack at a time. Empty for nothing, or somebody not in this world.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `tag` | number | — |  |

## `core.take_tagged`

```luau
core.take_tagged(player: number, tag: number) -> number
```

Takes every stack with that tag out of a player's bag, worn squares included, and returns how many things it took. Giving back what was stolen.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `tag` | number | — | 1 to 131071. |

## `core.bag_count`

```luau
core.bag_count(player: number, item: string) -> number
```

How many of that item a player's bag holds, worn squares included, without taking any: 0 for none, an unknown item or somebody who is not in this world.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `item` | string | — | By name, like "core:stone". |

## `core.take_item`

```luau
core.take_item(player: number, item: string, count: number?) -> number
```

Takes that many of that item out of a player's bag and returns how many it actually took -- fewer than asked when they did not have that many, and 0 when they had none.

The hand first: the slot the player is holding is taken from before any other, so a mod answering a click takes the thing that was clicked with.

Up to `count` rather than all-or-nothing, and a mod that needs all of it checks the number: giving back what it took is one call it can see, where an engine that decided for it would be a rule nobody can read from Lua.

The player is told: their bag is sent again on the next tick, so what a mod took does not go on being drawn in their hand.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `item` | string | — | By name, like "core:stone". |
| `count` | number | `1` |  |

## `core.wear_held`

```luau
core.wear_held(player: number, amount: number?) -> number
```

Spends uses of what the player is holding, as a dig or a blow does, and it is gone when its durability runs out (Unbreaking spares some, as it does there). For a tool whose use is a mod's -- a hoe tilling. Returns 1 if the hand held something that wears, 0 otherwise.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | The id every event carries in its `player` field. |
| `amount` | number | `1` | How many uses it spends. |

## `core.enchant_held`

```luau
core.enchant_held(player: number, level: number, kind: number?) -> number
```

Sets one kind of enchantment on what the player is holding, if it is one thing, keeping the others. The stack keeps them, in a bag, a chest or on the ground. Returns that kind's level now, or -1 for nothing enchantable in the hand.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | The id every event carries in its `player` field. |
| `level` | number | — | 0 to 3; 0 takes an enchantment away. |
| `kind` | number | `1` | 1 Efficiency (digs 1 + 0.5 a level faster), 2 Sharpness (a point harder a level), 3 Unbreaking (wears one use in level + 1), 4 Protection (a point of armour a level, worn). A thing keeps each kind's level apart. |

## `core.held_enchant`

```luau
core.held_enchant(player: number) -> number
```

The enchantments of what the player holds, two bits a kind: kind k's level is (value // 4^(k-1)) % 4. 0 for none, -1 for an empty hand.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | The id every event carries in its `player` field. |

## `core.give_item`

```luau
core.give_item(player: number, item: string, count: number?) -> number
```

Puts that many of that item in a player's bag and returns how many FIT. An partial stack of the item is topped up first, then an empty hand, then the first empty slot -- so taking the held item and giving another swaps what is in the hand. What does not fit is not dropped and not lost: nothing happens to it, and the mod decides -- core.drop_item puts it on the ground where a broken block's stack goes, and doing nothing leaves the player to make room. An engine that dropped it would be deciding for every mod at once.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `item` | string | — |  |
| `count` | number | `1` |  |

## `core.meta_set`

```luau
core.meta_set(x: number, y: number, z: number, key: string, value: string) -> number
```

What a cell remembers, written down. Survives the world being closed and opened, and is thrown away when the cell becomes air -- breaking a block takes its state with it, so the next block placed there does not inherit it.

Strings, and only strings, on purpose: a pedestal that stores item id 41 is storing whatever 41 means the next time the world is opened, because ids are handed out in registration order. A NAME survives a mod being added. A mod that wants a number writes tostring and reads tonumber, which is a conversion it can see.

Returns 0 when the write is refused -- a field name or value too long, more than 16 fields on one cell, or a world already remembering 65536 cells -- and says which in the log.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `key` | string | — | 1 to 64 bytes. What the cell calls this. |
| `value` | string | — | At most 256 bytes. An EMPTY value erases the field -- which is what reading a field nobody wrote gives back, so there is one way to say "there is nothing here" rather than two a reader would have to tell apart. |

## `core.meta_get`

```luau
core.meta_get(x: number, y: number, z: number, key: string) -> string
```

What that cell remembers under that name, or an empty string if it remembers nothing. Not an error: most cells remember nothing and that is the right answer for them.

| argomento | tipo | default | note |
|---|---|---|---|
| `x` | number | — |  |
| `y` | number | — |  |
| `z` | number | — |  |
| `key` | string | — |  |

## `core.notice`

```luau
core.notice(player: number, text: string) -> number
```

Puts a sentence on a player's screen for a few seconds, where the reason for a refused edit goes. For a mod that changed what somebody did -- a block placed that came out as another one -- and wants them to know why, which cancelling says by itself and substituting does not. Returns 0 for nobody to tell or a sentence too long.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | The id every event carries in its `player` field; 0 is everybody in this world. |
| `text` | string | — | At most 512 bytes. |

## `core.chat`

```luau
core.chat(player: number, text: string) -> number
```

Puts a line in a player's chat, the one T opens, as it is: the engine adds no name in front. For an answer to a /command, or news everybody in the world should be able to read back. Returns 0 for nobody to tell or a line too long.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — | The id every event carries in its `player` field; 0 is everybody in this world. |
| `text` | string | — | At most 512 bytes, one line. |

## `core.command`

```luau
core.command(name: string, help: string, perm: string?) -> number
```

Declares a chat command, /name, for this mod. When a player types it, this mod and only this mod hears core:command with the player, the name and the rest of the line; the mod answers with core.chat. A player below `perm` is told no and the mod hears nothing. Returns 0 for a bad name, one the engine keeps (help, list, the admin commands), one another mod already declared, or a `perm` nobody has heard of. The single-process client ignores `perm`: its one player owns the world.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | 1 to 32 of a-z, 0-9, _ and -, without the slash. |
| `help` | string | — | One line for /help; may be empty. |
| `perm` | string | `` | What a player needs to run it: a permission (kick, ban, mute, creative...) or a role (helper, moderator, admin, owner). Empty for anybody. |

## `core.has_perm`

```luau
core.has_perm(player: number, perm: string) -> number
```

1 when this player's role on this server reaches `perm`, else 0. Roles go guest (no identity), player, helper, moderator, admin, owner, and are given with /role (a role given for one room counts in that room). In the single-process client its one player owns the world: always 1.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `perm` | string | — | A permission or a role, as for core.command. |

## `core.role`

```luau
core.role(player: number) -> string
```

This player's role on this server: guest, player, helper, moderator, admin or owner; empty for nobody. "owner" in the single-process client.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |

## `core.page_register`

```luau
core.page_register(name: string, title: string, perm: string?) -> number
```

Offers a page, the in-game web page: /page lists it and a page:NAME link opens it. When a player opens it this mod alone hears core:page_open with the player and the name, and answers with core.page_show. Returns 0 for a bad name, one the engine keeps (pages, admin, bans, whitelist, roles, perms, server, mods, here), one another mod registered, or a `perm` nobody has heard of.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — | 1 to 32 of a-z, 0-9, _ and -: what /page NAME and page:NAME links open. |
| `title` | string | — | The page's title bar, and its line in /page. |
| `perm` | string | `` | Who may open it, as for core.command; empty for anybody. |

## `core.page_show`

```luau
core.page_show(player: number, name: string, markup: string) -> number
```

Puts a page on one player's screen, laid out by the engine: wrapped text, buttons, tables, fields, and back/next when it runs past one screen. A link's target says what a click does: `page:NAME` opens another page, `cmd:/line` runs a chat command as that player (their role decides, as if they had typed it), and `act:ID` tells this mod core:page_action. A field's target has `{text}` replaced by what was typed, on Enter. After a cmd: link the page is opened again, so it shows what the command changed. ESC or x closes it. Returns 0 for nobody to show it to or markup too long. In the single-process client the page is shown the same way; in a connected world's code, always 0: its pages are the server's.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |
| `name` | string | — | The page's name: a registered one shows its title. |
| `markup` | string | — | At most 16 KiB. One construct a line: `# heading`, `## subheading`, text with `[label](target)` links, `| a | b |` table rows, `?[hint](target)` a field, `> quieter text`, `---` a rule, an empty line a gap. |

## `core.is_muted`

```luau
core.is_muted(player: number) -> number
```

1 when this player's chat is held (/mute). The engine already holds what a muted player's command, page action, click or mod message makes a mod say to anybody else, then or later: a core.chat, core.notice or page line carrying three of their words in a row (twelve letters or more, or all they said) goes to them alone, while they are muted and for an hour after. This is for an answer that says why, as /msg gives, and for a relay that should not wait for that. 0 in the single-process client.

| argomento | tipo | default | note |
|---|---|---|---|
| `player` | number | — |  |

## `core.storage_set`

```luau
core.storage_set(key: string, value: string) -> number
```

What this mod keeps across a restart: a string under a name, the mod's own -- another mod cannot read or write it. Saved in the world file with the chunks, only the keys that changed; a server with no world file, and the single-process client, keep it until they stop.

The file is read after the mods load, so a mod reads what it kept from core:world_ready on, not while it loads. Returns 0 when refused -- a key or value too long, or more than 4 MiB of keys and values -- and says which in the log; and in a world's code on a player's machine, which keeps nothing.

| argomento | tipo | default | note |
|---|---|---|---|
| `key` | string | — | 1 to 64 bytes. |
| `value` | string | — | At most 4096 bytes. An EMPTY value erases the key, as with core.meta_set. |

## `core.storage_get`

```luau
core.storage_get(key: string) -> string
```

What this mod kept under that name, or an empty string if nothing.

| argomento | tipo | default | note |
|---|---|---|---|
| `key` | string | — |  |

## `core.shared_storage_set`

```luau
core.shared_storage_set(key: string, value: string) -> number
```

storage_set, kept once for every room of the server instead of once per world: in the first room's file (--room order; the sandbox's), which every room reads and writes. For what belongs to a player wherever they are -- hunger, experience, a counter that numbers things carried between worlds -- and not for what belongs to a place, which stays in storage_set. On a server of one world it is the same store. Same limits and return as storage_set.

| argomento | tipo | default | note |
|---|---|---|---|
| `key` | string | — |  |
| `value` | string | — |  |

## `core.shared_storage_get`

```luau
core.shared_storage_get(key: string) -> string
```

What this mod kept under that name with shared_storage_set, or an empty string.

| argomento | tipo | default | note |
|---|---|---|---|
| `key` | string | — |  |

## `core.item_id`

```luau
core.item_id(name: string) -> number
```

Looks up an item by name, including the item form of a block. Returns 0 for nothing, which is also the id an empty inventory slot holds.

| argomento | tipo | default | note |
|---|---|---|---|
| `name` | string | — |  |

## `core.item_count`

```luau
core.item_count() -> number
```

How many item ids this world has handed out, so a mod can walk 1 to this and ask what each one is. It grows while mods load and does not shrink: an item whose mod went away keeps its id (ItemRegistry orphans it) so that a stack of it in somebody's chest still means what it meant.

Here because a mod cannot otherwise know what EXISTS -- item_id answers about a name it already has. A creative inventory, a search box or a mod that lists what a world holds needs the other direction.

## `core.item_name`

```luau
core.item_name(id: number) -> string
```

The name an item id was registered under, empty for an id nobody registered and for one whose mod is gone. Walking 1 to item_count and skipping the empty answers is every item a player could be handed.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — |  |

## `core.item_stack`

```luau
core.item_stack(id: number) -> number
```

How many of that item fit in one slot -- 64 for most things, 1 for a tool. What a mod handing over a full stack has to ask, rather than writing 64 and being wrong about every tool.

| argomento | tipo | default | note |
|---|---|---|---|
| `id` | number | — |  |

## `core.event`

Scritte a mano, non generate: prendono tabelle invece di scalari, e un
generatore che le coprisse sarebbe piu' grande delle tre funzioni che
sostituisce. Documentate in [MODDING.md](MODDING.md#event-bus-tipizzato).
