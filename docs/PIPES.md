# Pipes

*[Leggi in italiano](PIPES.it.md)*

The complete guide to the `pipes` mod (`mods/pipes/`): pipes that carry items from one container
to another, and routed networks that sort them, store them and deliver them on request. The model
is the Transportation module of the classic tech mod
([projectredwiki.com/wiki/Transportation](https://projectredwiki.com/wiki/Transportation)), cut
down to what matters.

The first part is for players, the second ([How it works inside](#how-it-works-inside)) for anyone
who wants to know exactly what happens, or to write a mod that talks to it.

---

## In short

- **Pipes** join the pipes on their six sides by themselves. Items travel in them as a small
  glowing cube.
- The **item pipe** is simple: a stack with somewhere to go passes through it on its way; a lost
  stack, where three or more pipes meet, picks at random.
- The **junction pipe** is smart: it sends each stack the shortest way to where it is going.
- The **interface pipe** joins a container (chest, furnace…) and holds up to four **chips**, which
  decide what comes out and what goes in.
- The **request pipe** shows in a grid of icons what the network holds and what it can craft, and
  sends you what you ask for.
- The **automatic crafting table** and the **automatic furnace**, level-2 blocks beside the pipes,
  craft and smelt for the network, in chains too.
- The **pipe wrench** turns an interface or a request pipe to its next container, as the
  screwdriver does.
- The **firewall pipe** does not let through the items put in it.
- The **latency pipe** slows things and ways avoid it; the **valve pipe** lets things through only
  while cinnabar powers it.
- The **crafting pipe** (and its **extension pipes**) gives a machine recipes picked on a panel,
  with no thing needed to name them.
- The **pipe router** shows what a network is.
- More of one chip in a square give an interface a higher **priority**.
- The cube's colour says where it is going: **blue** to a responder, **purple** to a terminator,
  **orange** to an overflow, **white** requested, **green** lost.

---

## The pipes

Every pipe is a core with an arm towards each neighbouring pipe (and, for the interface and the
request pipe, towards their container). The shape updates by itself when a pipe or a container is
put down or taken away beside it. Breaking a pipe, whatever its shape, always gives back the same
item. They break by hand in 0.3 seconds.

### They are solid

Pipes stop bodies: a player or a creature bumps into one instead of going through. A pipe is 0.625
of a node high (the core runs from 0.375 to 0.625), more than the 0.55 that is walked up, so **a
pipe on the floor is jumped over**. It can be stood on.

Only the pipe's boxes are solid: the rest of the cell is air, there is room to pass beside it, and
**light goes through** (in the engine only a whole cube is opaque). An item dropped on a pipe rests
on it; the crosshair and the outline follow the pipe's shape.

### Item pipe

No logic. It carries stacks from one pipe to the next and never touches a container. A stack that is
going somewhere **follows its way** through crossings of item pipes too: the shortest way is worked
out whole when it sets off. Only a **lost** stack (green), where three or more pipes meet, goes on
any way but the one it came from, at random; at a dead end it turns back.

It does anywhere. A junction is for where a stack must be able to **change its mind**: find its way
again if the old one is broken, or look for a destination when it has none.

### Junction pipe

The routed pipe. A stack that reaches it:

1. if it already has a destination it can still reach, sets off the shortest way there;
2. otherwise looks for a new one (see [Where a stack goes](#where-a-stack-goes));
3. if it finds none, turns **green** and wanders.

It is where lost stacks find a destination again.

### Interface pipe

A junction joined to **one** container beside it: the first found among its six sides, in the
order +x, −x, +y, −y, +z, −z. A **right click with the screwdriver or the pipe wrench** turns it to the next side that
has a container; the pipe's arm turns towards the one chosen. If the container goes, the pipe
moves to the first other one it finds.

A right click (without a pipe in hand) opens its **four rows of nine squares**:

| square 1 | squares 2–9 |
|---|---|
| the **chip** | its **filter**: up to eight items, one a square |

All four rows work at once and on their own. A row with no chip in its first square does nothing.
In a filter only *which* item is there counts, not how many.

Breaking the interface drops its chips and filters on the ground. Its squares are "raw", so no
extractor or hopper of another mod takes the chips out.

A neighbouring container must not be a pipe: the interface and the firewall have squares of their
own, but they do not count as another interface's container.

### Request pipe

A right click opens the **Request pipe** panel. It shows everything the network's **broadcasters**
offer, each item's total summed over every container, and everything the network **can make** out
of what it holds.

```
┌──────────────────────────────────────────────┐
│ Request pipe: 36 kinds, 391 in all           │
│ ┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐         │
│ │64││64││48││20││ 4││ 1││ 1││  ││  │  ← 9 × 4 │
│ └──┘└──┘└──┘└──┘└──┘└─*┘└──┘└─*┘└─*┘         │
│  …                                           │
│ [<] 1 / 4 [>]  [By quantity]  [All]          │
│ Search: [_______________________] [Clear]    │
│ Ask for: 8     [1] [8] [Stack] [____]        │
└──────────────────────────────────────────────┘
```

`*` = the wrench: it can be crafted.

- **The grid**, at the top: **9 × 4** icons like the bag's. The **name** shows under the pointer.
  The number at the top left is **how many there are** in the store. A small **wrench at the
  bottom right** means the network **can craft it** from what it holds with its
  [machines](#the-machines): with no machines there are none. A thing that can only be
  crafted has no number. **A click** on the icon (or on the wrench) asks for it.
- **‹ / ›** and the page number move through the grid, 36 at a time.
- **By name / By quantity** changes the order (by quantity: what you have most of first).
- **All / In store / Craftable** chooses what to show.
- **Search**: write part of the name or of the id (`sand`, `ore:`, `planks`…) and press Enter;
  case does not matter. **Clear** takes it away.
- **Ask for**: how much a click asks for. **1**, **8**, **Stack** (a whole stack of that thing: 64,
  16 or 1 depending on what it is), or a number written in the box and confirmed with Enter (up to
  1024).

Order and amount are kept when you move to another request pipe too. The panel closes with Esc, or
by itself if you go more than 8 nodes away or the pipe is broken.

**What a click does.**

1. What was asked for is taken from the broadcasters: the nearest first, then the next.
2. If the store has not enough, what is missing **is made by the network's
   [machines](#the-machines)**, in chains too (logs to planks to sticks). A recipe that makes more
   than one at a time may send a few more than were asked for. If not all of it can be made, the
   most that can be is.

Everything comes as a **white** cube:

- into the container beside the pipe, if there is one (chosen as for the interface, screwdriver or
  wrench included);
- otherwise it falls **onto the pipe** itself, to be picked up.

If that container is full, what does not fit sets off again like any other stack, to a responder, a
terminator or an overflow. The notice says how many set off (**"N on the way"**, **"N on the way,
M being made"**) or **"Nothing to send"**. What has to be made comes when the machines are done.

### Firewall pipe

A right click opens **nine squares**: the items put there **may not enter** the firewall. Everything
else goes through it as through a junction. Ways are worked out around it: a stack of a blocked
item goes round another way if there is one; if not, that part of the network does not exist for
it. Requests and broadcasters obey it too. Breaking it drops what is in it.

---

### Latency pipe

A **slow** item pipe: a stack goes through it at a third of its speed, and in working out ways it
counts as **8 cells**. Ways go round it when there is another not much longer: a way to push the
traffic onto another branch, or to slow a stretch on purpose.

### Valve pipe

A pipe **opened by cinnabar**: open while something of `mods/cinnabar` gives power beside it (a
lever on, a button pressed, a lit torch, lit dust, a repeater or comparator on, a cinnabar block),
shut otherwise.

- **Shut**, nothing passes, and ways take it for a wall: they go round, and an extractor does not
  send what could only go through it.
- A stack **already on its way** through it **stops in front** and waits for it to open.
- **Open**, it is a pipe like any other.

With a cinnabar clock it is a timed lock; with a comparator reading a chest, a tap that shuts when
the chest is full.

---

## The machines

A network makes things **only with its machines**, two **level-2** blocks: they cost diamonds,
interface pipes and a chip, and are dug only with an iron pick or better. They go **beside any
pipe** of the network: the pipe reaches an arm to the machine's brass port.

| machine | what it does | how long it takes |
|---|---|---|
| **Automatic crafting table** | the crafting table's recipes | 1 s a recipe |
| **Automatic furnace** | what a furnace smelts, burning fuel from the network (a coal every 8 smelts, as the furnace) | 2 s a thing |

**Recipe chips.** A machine **can do nothing by itself**: it makes only what the **recipe chips**
beside it say.

- **Where they go:** in a row of an **interface pipe touching the machine**. The things in that
  row's filter are its recipes: for the table what it is to make, for the furnace what it is to get
  by smelting (iron ingots, glass…).
- **How many recipes:** up to 8 a row and 4 rows an interface, and a machine may have several
  interfaces round it.
- **The filter:** each recipe takes one of the thing, as any filter does.
- **In chains:** **every step needs its chip**. For an iron pick made from logs, the table needs
  planks, sticks and the pick, and the furnace iron ingots. Without planks on the chip, sticks
  cannot be made, nor the pick.

The interface with the recipe chips is usually also the pipe that brings the machine its
ingredients.

**Crafting and extension pipes.** In place of an interface with recipe chips, a **crafting pipe**
can go beside the machine:

- **Its recipes:** a right click opens its panel, with its **9 recipes** at the top (a click takes
  one off) and under them a search and a grid of what the machine can make (a click adds one).
  **No thing is needed to name one**: it is picked from the list.
- **More recipes:** each **extension pipe** joined to the crafting pipe, or to an extension already
  joined, adds 9 more, up to 16 extensions. A right click on an extension opens the same panel,
  with its own.
- **As any pipe**, the crafting pipe carries the ingredients too.

**Recursive.** What you ask for is planned down to what the store holds, through up to **6
recipes in a chain**. For an iron pick, with a log, three raw iron and a coal in store, the plan is:

1. the furnace smelts **3 iron ingots**, burning the coal;
2. the table makes **4 planks** from the log;
3. then **4 sticks** from 2 planks;
4. then the **pick** from 3 ingots and 2 sticks.

**They really work.**

- **Ingredients:** they leave the broadcasters as **white** cubes and travel the pipes to the
  machine that uses them.
- **Working:** while it works a machine **lights up**: the table's lamp turns green, the furnace
  shows its fire and an amber lamp.
- **Intermediate products:** they go to the machine of the next step (the ingots from the furnace
  to the table). The final product goes to the request pipe.
- **Contents:** a right click on a machine shows what has come to it and is not used yet.

**Leftovers.** Recipes often make more than is needed: a log gives 4 planks, but 2 are needed. When
a machine has stood idle for 5 seconds, what is left in it goes back to the network: to a
responder, a terminator or an overflow that takes it, otherwise into the store of the nearest
broadcaster.

**Several machines.** Each step goes to the machine nearest the request pipe that has the recipe
chip for that product, counting the work it already has queued. Each machine does one step at a time, the first in its queue that
already has everything. Two machines of one kind share the work of several requests.

**When it fails.** If a step that is waiting for nothing more from other steps goes without its
ingredients for **90 seconds**, with its machine idle, the job is given up and whoever asked is
told. The same happens if a machine is taken away. What had already come to the machines goes back
to the network as leftovers.

---

## The chips

They go in the first square of a row of an interface pipe. They stack to 16.

**Priority.** How many of one chip are in the square is that interface's **priority** for it.
Between two responders (or terminators, or overflows) that take a thing, the one with the higher
priority wins, even if farther; with equal priority, the nearer. Broadcasters too: the one with
more chips is taken from first.

| chip | colour | what it does | uses the filter? |
|---|---|---|---|
| **Extractor** | — | once a second sends up to **8** of the first item in the container that has somewhere to go | yes: only those items; empty = everything |
| **Responder** | blue, fast | takes its filter's items | yes: empty = nothing |
| **Dynamic responder** | blue, fast | takes the items the container **already holds** | no |
| **Terminator** | purple, slow | takes its filter's items, but only once **no responder** has room | yes: empty = nothing |
| **Overflow** | orange, slowest | takes **anything** nobody else takes | no |
| **Broadcaster** | — | offers the container's contents to request pipes and stock keepers | no |
| **Stock keeper** | white (incoming) | keeps **a full stack** of each filter item in the container, asked from the broadcasters | yes: it is the list |
| **Recipe** | — | on an interface touching a [machine](#the-machines): what that machine can make | yes: they are the recipes |

The **blank chip** does nothing: it is what all the others are made from.

### Extractor

Every second it looks through the container's squares in order (up to 54) and, for the first item
that passes the filter **and has a destination** on the network, takes up to 8 and sends them. An
item with nowhere to go is skipped, so a mixed chest does not jam on something nobody wants. It
never sends to its own container.

It takes only from "store" and "product" squares: everything from a chest, only the finished
product from a furnace (not the fuel, nor what is cooking). Several extractor rows on one interface
send several stacks a second.

### Responders, terminator, overflow

These are the three **classes** of destination, in order of priority: 1 responder (dynamic too),
2 terminator, 3 overflow. An interface takes things only while its container is **not full**; with
several chips, the best class among those that take the item counts.

The **dynamic responder** looks at the whole container: once there is one of an item, the rest
comes to it. Handy for a store that sorts itself: put the first one in by hand.

### Broadcaster

Makes the container a store the network can see. It offers what is in the "store" and "product"
squares (from a furnace, the finished product). With several broadcasters, the one nearest the
asker is taken from first.

### Stock keeper

Once a second, for each item in the filter: if the container holds less than a full stack of it
(counting what is already on its way), it asks the broadcasters for the difference. With sand in
the filter, say, the chest always has 64 sand while the network has some.

---

## The pipe router

A tool (glass, cinnabar, gold, a blank chip and iron). A **right click** with the router on a pipe
or a machine opens a page about its network:

- how many pipes, nodes and links it has, and what kinds the pipes are;
- the chips in its interfaces;
- the valves open and shut, and the firewalls;
- each machine, with how many recipes it knows and how many steps it has to do;
- how many things and how many stacks are on their way.

---

## Where a stack goes

When a stack looks for a destination (leaving an extractor, or at a junction when it has none),
the network's interfaces are looked at **from the nearest to the farthest** (the shortest ways from
there are already worked out, see [The graph](#the-graph)). The winner is:

1. the **nearest responder** that takes the item;
2. if there is none, the **nearest terminator**;
3. if there is none, the **nearest overflow**;
4. if not, nothing: the stack turns **green** and wanders.

"Nearest" is in pipes travelled, not as the crow flies, and firewalls that block the item are
walls.

The stack follows the way found. If on arrival the container has no room after all (something else
filled it meanwhile), what is left **looks again from there**, leaving out the interfaces that
already refused it. If the way is broken (a pipe taken away, a new firewall), the stack goes on as it
can, and at the next junction works out a way to the same destination again.

A stack that reaches the edge of a part of the world that is not loaded **waits there** until it is
in memory again, instead of taking the air for the end of the pipe and falling out.

### Lost stacks

A green stack wanders at random, but at every routed pipe it tries again to find a destination:
as soon as somebody has room, it goes there. A stack that has been travelling for more than **a
minute** and finds itself with no way **falls out** where it is (onto the pipe). A stack whose pipe
ahead is broken falls in the cell it came from.

### Speeds

In nodes a second: responder **6**, requested **4**, terminator **2.5**, lost **2**, overflow
**1.2**. Colour and speed change when the destination does.

---

## Recipes

Cinnabar for the original's redstone, dyes for its coloured illumar.

| item | how many | recipe (rows of the 3×3 grid) |
|---|---|---|
| Item pipe | 16 | stone · glass · stone |
| Junction pipe | 16 | red dye · cinnabar · green dye / diamond · glass · diamond / green dye · cinnabar · red dye |
| Interface pipe | 1 | cinnabar · gold · cinnabar / gold · junction pipe · gold / cinnabar · gold · cinnabar |
| Request pipe | 1 | cinnabar · junction pipe · diamond |
| Firewall pipe | 1 | cobblestone · junction pipe · cinnabar |
| Blank chip | 1 | gold ingot · paper · cinnabar |
| Automatic crafting table | 1 | diamond · stock keeper chip · diamond / interface pipe · crafting table · interface pipe / iron · gold · iron |
| Automatic furnace | 1 | diamond · extractor chip · diamond / interface pipe · furnace · interface pipe / iron · cinnabar · iron |
| Latency pipe | 8 | eight item pipes round sand |
| Valve pipe | 1 | — · cinnabar · — / iron · item pipe · iron |
| Crafting pipe | 1 | cinnabar · crafting table · cinnabar / gold · junction pipe · gold / cinnabar · gold · cinnabar |
| Extension pipe | 1 | paper · junction pipe · paper |
| Pipe router | 1 | cinnabar · glass · cinnabar / gold · blank chip · gold / — · iron · — |
| Pipe wrench | 1 | iron · — · iron / — · iron · — / — · iron · — (iron ingots) |

Every other chip: **dye · cinnabar · dye / X · blank chip · X / dye · cinnabar · dye**, with:

| chip | dye | X |
|---|---|---|
| Responder | orange | iron ingot |
| Dynamic responder | cyan | orange dye |
| Overflow | green | iron ingot |
| Terminator | purple | grey dye |
| Extractor | cyan | iron ingot |
| Broadcaster | magenta | gold ingot |
| Stock keeper | blue | diamond |
| Recipe | yellow | crafting table |

---

## Ready-made layouts

**Emptying one chest into another.** Chest A — interface with an *extractor* — pipes — interface
with an *overflow* — chest B.

**Sorting.** An input chest with an *extractor*; along the pipe, a chest for each material with a
*responder* that has that material in its filter; at the end an *overflow* for the rest.
Junctions at every crossing.

**A store you ask.** Several chests, each with a *broadcaster* interface (and a *dynamic
responder*, so what comes in goes back where it already is). A *request pipe* beside the chest you
work from.

**Automatic supply.** Beside the furnace or the bench, a chest with a *stock keeper* interface
filtered on coal, sand… The network keeps it full from the broadcasters.

**Automatic furnace.** An interface with an *extractor* on the furnace takes only the finished
product away and sends it to the responders.

### The demo on the plaza

On the plaza, east of the spawn (x 65–77, z 45–67), there are three working networks with signs that
explain every part:

- **Sorting with pipes**: cobblestone to a responder (blue), made to go round by a firewall that
  names it; dirt to a terminator (purple); planks, which nobody asks for, to an overflow (orange).
- **Requests with pipes**: a chest with a broadcaster, a request pipe, a stock keeper keeping a
  stack of sand.
- **Crafting with machines** (z 60–67): a store of logs, raw iron, coal, cobblestone and sand; an
  automatic crafting table with a recipe chip for planks, sticks, iron pick and crafting table; an
  automatic furnace with a recipe chip for iron ingots, glass, stone and charcoal. Ask the request
  pipe for an iron pick: the furnace smelts the ingots while the table makes planks and sticks,
  then the pick comes to the chest beside the pipe. The machines face the spawn, so they are seen
  lighting up.

Every 15 seconds the supplies are topped up and the end chests emptied. A pressure plate builds it
all again as it was.

---

## Limits

- **Stacks in transit survive a restart**: they are saved every 3 seconds and put back in the pipes
  where they were. What set off from a container in the last 3 seconds before the stop was still in
  that container when the world was saved.
- **The machines' jobs do not resume** after a restart, but **nothing is lost**: what the machines
  had already been brought goes back to the store as soon as they find themselves idle, and
  products already on their way arrive. The request has to be made again.
- A network has **65536 pipes** at most in its graph: in a bigger one, what lies beyond is not
  found.
- A network works where the world is loaded. Where stacks are on their way or machines have work
  to do, the world **stays loaded** until they are done, with nobody near. A network standing idle
  far from everybody does not wake by itself, though: chips are an ABM, as every machine of the
  game.
- Things are made only with machines, only with what the broadcasters offer and up to 6 recipes in
  a chain. An ingredient that leaves something behind (the bucket of a bucket of milk) leaves it in
  the machine, which sends it back to the store.

---

## How it works inside

### Blocks and shapes

Each of the five kinds is registered as **64 blocks**, one per combination of the six arms:
`pipes:<kind>` (no arm) and `pipes:<kind>_<mask>`, with bit `d−1` of the mask for direction `d` in
the order +x, −x, +y, −y, +z, −z. The shapes are `core.node_box`es: the core `[0.375 … 0.625]³`
plus an arm to the edge of the cell for each bit. Every shape `node_drop`s as shape 0.

The blocks are `solid = true`: the engine stops bodies on the boxes and not on the cell, and a
block with boxes is never opaque, so light goes through. Stacks in transit are the mod's own ECS
entities, moved by hand: solidity does not touch them.

`refresh(cell)` works out the mask again (neighbouring pipes, plus the container's side for the
interface and request pipes) and swaps the block if needed; `core:placed` and `core:broken` call it
on the cell and its six neighbours. The container's side is kept in the cell's `side` meta.

### A stack in transit

A stack (`Stack`) has the item, how many, the cell it comes from and the one it goes to, the
progress `t` between them, its speed, its way (a list of cells) or none, its destination as a
`"x,y,z"` key, its age and the interfaces that already refused it. Each `core:tick` it moves
`speed × dt` cells; each time `t` reaches 1 it enters the next cell (`arrive`), up to 4 cells a
tick.

On arriving in a cell:

1. it is no longer a pipe → the stack falls in the previous cell;
2. it is over 60 s old and has no way → it falls there;
3. it is the destination → `deliver`: `container_add` into the interface's container (or, for a
   request with no container, `drop_item`); the rest marks the cell as refused and looks again;
4. it has a way and the next cell can still be passed → it follows it, through crossings of item
   pipes too;
5. otherwise: in a routed pipe `reroute` (first towards the old destination, then a new one with
   `route`, then green), in an item pipe `wander`.

### The graph

A network is not searched cell by cell each time a stack needs a way: it is boiled down once into a
**graph**.

- **Nodes**: the cells where something is decided. They are every routed pipe (junction,
  interface, request, firewall) and every item pipe that is not the middle of a run: three
  neighbours or more, or one (an end), or none.
- **Edges**: the runs of item pipe between two nodes, with their cells and length. A network of a
  thousand pipes becomes a few dozen nodes.

`build` makes it with one breadth-first visit (up to `MAX_NET` = 65536 cells).
`ways_of(net, node, item)` runs **Dijkstra** from a node over the graph, an edge weighing its
length in cells. It gives each node's distance, edge and previous node, and the interfaces in order
of distance. The result is **kept** in the network, once for each set of firewalls that hold the
item back (usually none, so one per starting node; at most `MAX_ROUTES` = 4096, then it starts
again). `way` turns the kept result into the list of cells the stack travels.

`route` goes through the interfaces in order of distance: it stops at the first responder and
remembers the first terminator and the first overflow. `class_of` gives an interface's class for
an item (nil if the container is full, `container_fill ≥ 1`). The choice between interfaces is
made live, because chips, filters and free room change all the time. The graph holds only what
changes when the pipes do.

**When it is updated.** Every cell of the network, and every cell beside one, is watched with
`core.watch_cell`, which reports any write (a player's, a mod's `core.set_block`, a piston's…) as
`core:watched`. If a cell of the network changes *kind* of pipe or stops being a pipe, or a pipe
appears in a cell beside it, the graph is **thrown away**. A pipe that only changes shape is the
same pipe and does not count. The new graph is built when next asked for, not at once, so a
hundred changes in a row cost one build. `core:placed`, `core:broken` and `pipes:refresh` throw it
away at once, without waiting for the event. A part of the world loaded back, which `watch_cell`
does not see, throws away the graphs through it as soon as it is in memory: eighteen LBMs
(`pipes:loaded_1` … `_18`, one for each 32 of the nine pipes' 576 shapes) call `touched` on every pipe
loaded. A graph older than `STALE_AFTER` (30 s) is built again anyway, as a safety net. The
watched cells are never let go, because the watch is one set for every mod and an observer of
`mods/cinnabar` might be watching the same cell.

**Container caches.** An interface's chip rows, a firewall's items and what a broadcaster's
container offers are read again only when that container's `core.container_revision` changes.

**What it is worth.** On a 30×30 grid of pipes with 20 extractors on chests of things nobody wants
(the worst case: every second each extractor looks for a way for 20 items), 60 seconds of play
cost 28.3 s of ABM with the cell-by-cell search and cost 0.69 s with the graph: from 23.9 to 0.9 ms
a tick.

### The chips

Two ABMs (`pipes:chips` and `pipes:chips_2`, as an ABM names at most 32 blocks) go over every
interface each second and for each row run the extractor and the stock keeper. The other chips are
passive: `class_of` reads them when something looks for a destination, `broadcasters` when
something asks.

`broadcasters(to, item)` takes from the asker's kept Dijkstra the interfaces with a broadcaster,
nearest first (one container with two broadcasters counts once). For each, the way back is only
worked out if something actually sets off from there. `request` takes from each (with `container_take`
on "store", then "product") a whole stack at a time until it has the number asked, and sends white
stacks with their destination already set. Stock keepers keep count of what they asked for and has
not yet arrived in `pending[cell][item]`, which goes down on delivery or when the stack changes
destination.

### The request panel

An engine panel (`core.ui_define`, `ui_item`, `ui_button`, `ui_field`): the client draws the icons
with their name as a tooltip, the mod only says what is where. A panel has at most **128
elements** and the server keeps **64** for every mod together. So:

- the grid is **9 × 4** and each square costs up to three elements (icon, number, wrench): 108,
  plus 15 of controls, makes 123;
- there are **`PANELS` = 6** panels, `pipes:request_1` … `_6`, handed in turn to whoever opens a
  request pipe. When they are all in use, the one looked at longest ago is closed on its player.

For each player a *view* remembers the pipe open, the search, the order, what to show, the page,
the amount and which item is in which square. `draw_request` declares the panel again after every
click. Clicks come back as `core:ui_click`: `i<n>`/`w<n>` (square n: icon or wrench), `prev`,
`next`, `order`, `show`, `find` (with the text), `clear`, `a1`, `a8`, `astack`, `amount` (with the
text), `core:esc`. Each counts only if the pipe is still there and the player within 8 nodes;
otherwise the panel closes. Written text loses its control characters and is cut to 40 bytes; the
amount is held between 1 and `MAX_ASK` = 1024.

`stock` counts each thing only from the broadcasters it can really come from: in a network with
firewalls, it looks for each thing at the Dijkstra ways worked out for it (the request's own, kept),
so the panel shows only what a request gets.

### The machines and the plans

**The recipes.** The crafting table's come from the engine: `core.recipe_count()`,
`core.recipe_result(i)`, `core.recipe_yield(i)` and `core.recipe_items(i)` (ingredients by name, one
word a square, no shape). The furnace's come from `mods/furnace`: to the event `furnace:ask` it
answers at once with a `furnace:smelt {from, to}` for each smelt and a `furnace:fuel {item, smelts}`
for each fuel. `ways_to_make()` reads them **once**, at the first request, and keeps them by product
id as *ways* `{kind = "craft" | "smelt", makes, yield, needs}`.

**The plan.** `plan(store, item, q, machines)` is a depth-first search:

- `need(id, q, step)` takes from the store what it can (noting it in `step.uses`, which will be
  brought to the machine) and asks `make` for the rest;
- `make(id, q, parent)` tries the ways of making it that the machines present can do, `⌈q ÷
  yield⌉` times, and for a smelt the first fuel there is enough of;
- every take from the store goes into a log, and a failed attempt is **undone** back to its mark; a
  thing already being made further up the chain is not tried again (no cycles);
- limits: `MAX_DEPTH` = 6 recipes in a chain, `PLAN_BUDGET` = 4000 attempts a plan.

The steps come out in the order they can be done: children before their parent. Each step knows
where its product goes (`parent`) and how many children it waits for (`children`).

`makeable` asks for a *dry* plan of one of each product, and the panel uses it for the wrench. With
a store of 638 kinds and both machines, 286 things come out makeable and drawing the panel costs
about 3.5 ms.

If the plan for the whole amount fails, `craft` finds by halving the largest amount that can be
made.

**A machine's recipes.** `abilities(net)` looks, for each machine of the network, at the interface
pipes on its six sides and their rows with a recipe chip (`rows_of`, kept until the container
changes). It gives `[kind][product]` = the machines that can make it. `plan` tries only the ways
whose product is there, and `makeable` and `craft` start from it: the panel and the jobs see a
chip put in or taken out at once.

**The job.** `craft` gives each step, among the machines with its chip, to the one with the lowest score (distance
from the request pipe + 16 for each step already queued). Then it sends each machine, from the
broadcasters, what its step takes from the store. The cubes carry `into` = the machine's cell, and
`deliver` puts them into its container.

**A machine's turn.** A `core:tick` handler goes over the machines with work:

- if one is under way and its time is up, it delivers the product: into its own container if the
  parent is on the same machine, otherwise shipped to the parent's machine, or to the request pipe
  for the last step;
- otherwise it looks for the first step in its queue that has everything it needs (`needs`) in the
  container, takes it out and lights the machine (it swaps the block between `_on` and off, keeping
  its facing);
- if a step with no children pending has waited more than `WAIT_FOR` (90 s, counted from the last
  thing to reach the machine), the job is given up;
- a machine idle for `IDLE_FLUSH` (5 s) sends its leftovers back, a stack a turn.

**Machines in the graph.** A machine is not a pipe. `build` finds it among the cells beside the
network and records it in `net.machines` with the pipe that feeds it, and that pipe becomes a node
of the graph. Machines do not count as containers for interfaces and request pipes. Going on and
off does not throw the graph away, as it is the same machine.

**Recipes' leftovers.** `ways_to_make` asks the engine, for each ingredient, `core.item_leaves(name)`
(what `core.item_returns` said is left): when a step starts, the machine puts into its container
what the spent ingredients leave, and sends it back to the store when it is idle.

**After a restart.** Jobs are not saved. An LBM (`pipes:machines`) finds the machines again as their
chunks load, turns off one left on and records it: idle and with no work, it sends what it holds
back to the store. Stacks that were going to a machine are saved with their `into` and get there,
then go back the same way.

### Stacks through a restart

Every `SAVE_EVERY` (3 s) `save_transit` writes into the mod's storage (`core.storage_set`, saved in
the world file) every stack in transit: `item|count|cell from|cell to|destination|into|asked`,
separated by `;`, in as many values as needed (`transit_1`, …, at most 4 KB each; `transit_n` says
how many), and empties those no longer needed. At `core:world_ready` `restore_transit` puts them
back in the pipes: they set off from the cell they were in, with no way but with their destination,
and find their way at the first routed pipe. The stock keepers' counts (`pending`) are made again
from the stacks asked for. A stack whose cell ahead is not in memory (`core.loaded`) waits.

**The models.** `tools/make_pipes_machines.py` (Blender, `--background`) builds the four models --
`autocrafter`, `autocrafter_on`, `autofurnace`, `autofurnace_on` -- exports them into `mods/pipes`
and leaves the work in `assets/blender/pipes_machines.blend`. The icons and the held models come
from `tools/make_held_models.py` (`ONLY=pipes:autocrafter,pipes:autofurnace`).

### For mod writers

- **`pipes:refresh {x, y, z}`**: works a cell's shape out again. Emit it after putting pipes down
  with `core.set_block`, which raises no `core:placed`. The network's graph updates by itself
  (`core.watch_cell`): the event is for the shape.
- **`pipes:clear {x0, z0, x1, z1}`**: makes the stacks in transit in that rectangle (any y) vanish
  without falling out. For whoever builds a network again (the demo uses it).
- An interface or firewall pipe put down with `core.set_block` has no squares: make them with
  `core.container_make(x, y, z, 36)` (interface) or `9` (firewall). The demo shows how to fill them.
- Any block with a container (`core.container_make`) works beside an interface; the squares' roles
  (`core.container_roles`) decide where the extractor and the broadcaster take from.

The numbers to change are at the top of `init.luau`: `MAX_NET`, `STALE_AFTER`, `MAX_ROUTES`,
`PANELS`, `MAX_ASK`, `LOST_FOR`, `TAKE`, the speeds in `CLASS`, `REQUESTED`, `LOST`, and for the machines
`MAX_DEPTH`, `CRAFT_SECONDS`, `SMELT_SECONDS`, `WAIT_FOR`, `IDLE_FLUSH`, `PLAN_BUDGET`, `SAVE_EVERY`,
and for the new pipes `LATENCY_COST`, `LATENCY_SLOW`, `RECIPE_SLOTS`, `MAX_EXTENSIONS`, `KEEP_EVERY`.

### The new pipes, inside

- **Valves:** always nodes of the graph (`net.valves`). `ways_of` adds every shut valve to the
  nodes left out and to the cache's key, so opening or shutting one picks another set of ways
  without throwing the graph away. `valve_open` looks at the six neighbours. `arrive` stops a stack
  in front of a shut valve on its way.
- **Latency:** an edge's length is the sum of its cells, `LATENCY_COST` for a latency pipe; in the
  tick a stack goes at `LATENCY_SLOW` of its speed while in one.
- **Priority:** each chip row has `prio` = how many of that chip the interface holds (kept with the
  rows). `class_of` gives class and priority; `route` looks at every interface and keeps for each
  class the highest priority, the nearest among equals; `broadcasters` sorts by priority, then
  distance.
- **Crafting pipes:** the recipes are in the cell's meta `r1` … `r9` (item names).
  `recipes_of_pipe` gathers them with those of the extensions reached, and `abilities` gives them
  to the machine the pipe touches. The panel (`pipes:recipes_1` … `_3`, in turn) offers the
  products of the ways of the served machine's kind (`served_kind`).
- **Router:** `show_network` writes the page `pipesnet`.
- **Work far off:** every `KEEP_EVERY` (20 s), `core.keep_loaded` on the columns of every stack on
  its way and every machine with work, which stay in memory the next 30 s.
