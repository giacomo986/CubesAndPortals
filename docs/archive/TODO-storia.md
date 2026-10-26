# Open items

Bugs and known gaps that are not fixed, with the symptom, how to reproduce, and
the hypothesis. `DESIGN.md` M-1 asks for this file so that an open bug lives
here and not in the memory of whoever is working on it; the rule is worth
keeping now that the engine has open items of its own.

An item leaves this file when it is fixed. A test comes with the fix only when
the item is in one of the areas the suite still guards (see below).

**The tests were cut down to the fundamental ones, 2026-09-13.** Development had
become more tests than features: 81 ctest entries, ~33.5k lines in `tests/` and
~9.6k lines of `--*-check` modes in `src/client/main.cpp`. Thirteen are left and
run in about 3 seconds: `protocol`, `chunk_codec` and `crypto` (input from the
network), `storage`, `migration` and `persistence` (a saved world),
`portal_math`, `portal_traverse`, `physics_box` and `portal_continuity` (the
portal and collision), `script_isolation` (the mod sandbox), `worldgen` and
`two_process_join` (client and server agree on the world). Everything else is
checked by playing. The "Gated in ..." lines and sabotage lists below name gates
that no longer exist; they are history, like the M11 lines.

**The original engine's compatibility layer is cancelled, 2026-09-12.** M11 was ten weeks of
`compat_layer/` and a second LuaJIT backend, to run its games and the original engine's mod
library on this engine, and it was the written mitigation against the project's
first risk: the contents. It is not being done. The original engine was the inspiration and
The earlier fork is where the experience comes from; it is not the target.

What M11 asked of the ENGINE was built before the decision fell and stands on its
own: `param2` facedir/wallmounted as a trait (CLOSED-27), the ABM/LBM scheduler
(OPEN-42), and drawtype nodebox+mesh (CLOSED-43). What falls with it: the
formspec parser, `[combine`/`[colorize`, VoxelManip, mapgen aliases, the
translation of `register_node`, and the LuaJIT backend -- whose first reason was
running Lua 5.1.

**What it leaves uncovered, said rather than left to be found:** DESIGN.md's
first risk is that the project runs aground on contents, and its mitigation is
gone. **The scope was chosen the same day** (DESIGN.md §0, M12, M13): a
Classic block-game sandbox whose portals lead to other worlds -- other servers, with
their own mods -- including worlds that are a different game, a stunt-racer
race or an arena-shooter FPS. That is not a smaller scope than the one §7.5 called
fatal without somebody else's contents. What keeps it standing is that each mode
is a small world of its own behind a portal, not a part of the sandbox. M13
lists what the engine lacks for it, read off the code: a world's behaviour does
not reach a client, the validator refused horizontal speed above 22.4 m/s (a
world declares its own since OPEN-44), a hit is checked by distance only
(OPEN-15), and a mod's entities are seen by nobody (OPEN-20). Every "Milestone.
M11" line below is history: it says why a thing was built, not that more of it
is coming.

**A world may send Luau to a client, and the client may refuse it, 2026-09-13.**
M13's open decision is taken (DESIGN.md §5.6): client-side code travels from the
world, in a VM per mod per session with client-only capabilities -- no files, no
network, no processes -- a memory quota and an interrupt budget. Authority stays
with the server, so refusing gains a player nothing and costs them what the world
meant to show; a refused world is still played.

**M10 is suspended, 2026-09-12.** Three of its four are done and in this file --
the mod package and its versions (OPEN-39), a build that travels (OPEN-40), and
the API documentation asked whether it is the API (OPEN-41). What is left is
Windows and the two bundle formats, and they wait: the decision is that nothing
gets packaged for other machines until the game is declared finished and
playable. Work goes to what the game itself is missing -- and not to M11, which
is cancelled, see above. OPEN-39, OPEN-40 and OPEN-41 keep their milestone lines; this
is the reason none of them is being finished today.

---

## CLOSED-31 -- every block breaks instantly, and a bare hand breaks all of them

**Asked for, not a defect.** Blocks should have a HARDNESS, and digging one
should need a tool of at least some level.

**Where it stands today.** Breaking is instant and binary. `NodeDef` has one
field about it -- `bool breakable` -- and `ServerSessions::plan_edit` asks it
once:

```cpp
if (!m_registry->is_breakable(out.had)) {
    why = "that block cannot be broken";
    return false;
}
```

There is no time, no progress, and nothing about what is in the hand. The floor
of the world is the only block in the game that says no, and it says no to
everybody for ever. `ItemDef` has no notion of a tool either: a name, a stack
size, the block it places, an icon. `register_tool` appears in DESIGN.md §M11's
list of what a mod of the original engine calls, and nothing here answers it yet.

**Two properties and they are not the same one.**

  - **Hardness** is how LONG a block takes: a number on the block, divided by
    how fast the held tool digs that kind of block. It makes breaking a thing
    that happens over frames, which is a state the server has to own -- a
    client that decided when a block broke would be a client that breaks
    bedrock in one frame.
  - **Tool level** is whether it can be dug AT ALL: below the level, the block
    does not break however long you hold. That is the rule that makes a
    progression, and it is a refusal, so it goes where `breakable` already is.

The second is cheap and the first is not, and they are worth doing in that
order: a level is a comparison in `plan_edit` with the answer it already sends
back, while a duration means a dig in progress -- who is digging what, since
when, what happens when they look away, what happens when the block changes
under them, and what the other players see. That is a small protocol of its own.

**What it touches.** `NodeDef` (hardness, required level), `ItemDef` (dig level
and speed, per group of block), `plan_edit` and the single-process `try_break`
so the rule is written once and not twice, the Lua API and its docs, and the HUD
if a dig shows progress. `mods/chest` and the palette blocks are what would
declare the first real values.

**Why it matters beyond the feature.** Right now a bare hand breaks stone as
fast as it breaks leaves, which means the item table has no reason to grow and
crafting has nothing to be FOR. Hardness is what makes a pickaxe worth making.

**Half done, 2026-09-08: the level.** A block declares `dig_level` and an item
declares what level it digs to; a hand holding less does not get the block out
at all, however long it is held. Zero is a bare hand and it is what every block
and every item that says nothing is, so this changed no block that existed.

One rule, written once. `net::tool_digs` and `net::refused_for_tool` sit beside
`refused_by_mod` and for the same reason: the networked server refuses in
`plan_edit`, the single-process client refuses in `break_into_bag`, and a player
given two different sentences for one rule is a player learning two games. The
sentence names both numbers -- "that needs a level 2 tool, and this one is level
0" -- because "you need a better tool" sends somebody to try every tool they own.

AFTER `breakable` and not folded into it, on both paths. The floor of the world
is not a very hard block, it is the floor, and refusing it with a sentence about
finding a better tool would send a player looking for something that does not
exist.

A defect this turned up on the way: the single-process loop played the breaking
SOUND before the break and unconditionally, which was harmless while every block
came out for every hand. It plays it on success now, and says the refusal
through `ui::Notice` the way a refused placement already did.

Gated in `net_loopback` (a bare hand refused, a tool one level short refused --
which is the case a level exists for, and a rule that only asked "holding
something" would let through -- the right tool allowed, a block asking for
nothing still breaking bare-handed, and bedrock refused with the best tool in
the game), in `hud` for the single-process loop, and in `mod_reload` for the
binding, because a level the engine keeps and `register_node` cannot carry is a
rule nothing can reach from where content lives. Six sabotages, all red on
distinct checks: the server not asking, holding anything counting as enough, the
client not asking, the level swallowing `breakable`, the binding dropping what
the mod said, and the comparison the wrong way round.

**Declared, not hidden.** The block table on the wire is NAMES: a client rebuilds
each definition from its own build, so a block only the server knows has
`dig_level` 0 on that client. The server is the authority and refuses, so the
world is right; what the client cannot do is predict the refusal. And it is a
level and not a kind -- a level 2 axe digs stone as well as a level 2 pickaxe.

**And the hardness, 2026-09-08.** A block declares `hardness` in seconds and an
item declares `dig_speed`; `net::dig_seconds` divides them, and it is one
function because the server times a dig with it and both client loops time their
own with it. A block that says nothing takes no time, which is every block that
existed.

**No new message, and the stopwatch is the server's.** The FIRST swing at a cell
starts a dig and changes nothing; a swing before the seconds are up changes
nothing either; the one after them breaks the block. So a dig is two Edits and
not sixty, the protocol did not move, and a client that hurried its own clock
sends its second Edit early and is told nothing. `Session` keeps where it is
digging, since when, and WHICH BLOCK -- a cell that became something else under
a dig is a different dig.

The in-progress answer is an EMPTY sentence, and that is the second place in
this engine where empty means "this click is not finished" rather than "you were
told no" (the first is a right click with an empty hand). It is a deliberate
exception to "every request gets an answer": the player is not waiting on this
reply, they are waiting on their own clock, and the block coming away IS the
reply. Answering every swing would put a notice on screen twice a second.

The holder's half is `dig_step` in `core/voxel/dig.h`, used by both client loops.
A block with no hardness completes on the PRESS frame only, which is what keeps
one click to one block when the button is held over a floor of them.

And a bar under the crosshair, because holding a button for two seconds with
nothing on screen is indistinguishable from a game that has stopped working. At
0 it draws nothing at all, so in a world where no mod sets a hardness it never
appears.

Gated in `dig` (the seconds themselves, an instant block not mining a trench, a
hard one at nothing/half/done, letting go, looking away, the aim sliding to the
next block, and the block changing underneath), `net_loopback` (the server's
clock: first swing, early swing, the swing that lands, a dig not carrying across
cells) and `hud` (the bar's FILL by its colour). Seven sabotages, all red. The
seventh -- the bar drawn full whatever the progress -- could not be run on the
day, because every gpu test on this machine had stopped being able to create a
window (see OPEN-6); it was recorded as unproven rather than counted, and it was
run the next session once windows came back: the bar's fill read "0 pixel(s) at
nothing, 2236 at a half and 2236 at the end", and the check failed. Suite 65/65
with it restored.

**Two things about the gates, said rather than left.** The `dig` test was
written AFTER the code it covers, so it never ran red -- it is proved by its
seven sabotages and not by a red run, which is weaker and is the only gate in
this repo built that way. And the bar's first check was green over nothing: it
counted "how much of the screen changed", and the bar's box and border are drawn
at full width whatever the progress is, so a half and a finished dig both read
0.11%. It counts the fill's own colour now -- 1118 pixels at a half, 2236 at the
end.

**And the values, 2026-09-08 -- which is what closes it.** The two halves above
are a stopwatch nobody started: no block in this game and no mod in `mods/` set
a hardness, so digging went on being instant everywhere while three gates about
the mechanism stayed green, because every one of them registered a block of its
own to measure.

The engine's blocks now declare seconds -- stone 1.5, the frame 0.9, glass 0.35,
grass 0.6, and dirt, sand and snow under half a second. `dig_level` stays 0 on
every one of them: a level is a refusal, tools are content, and an engine block
that demanded one would be a block nobody could dig with the mods turned off.
Bedrock declares no hardness either, because a duration is not a refusal.

`mods/tools` is where the other half lives: `tools:granite` asks for level 1 and
takes four seconds, `tools:pick` digs to level 1 at five times a hand's speed,
and a bench in the plaza hands over the first one -- a block that needs a tool
in a world with no tool in it is a block nobody can break. The pick is also a
recipe, three stone over two frames, which is the first thing in this game that
crafting is FOR.

**The gate is the game and not a fixture**, which is the whole point:
`tests/tools.sh` starts a real server on the real `mods/` directory and digs
with a headless body. Measured: the ground under the spawn (`core:grass`) comes
out in 0.72 s and seven swings, granite does not come out at all bare-handed
however long it is held -- "that needs a level 1 tool, and this one is level 0"
-- and it comes out in 0.92 s and nine swings once the bench has handed over a
pick. Beside it, `dig` asks the shipped table properties rather than reading
back constants: nothing instant, nothing asking for a tool, stone strictly the
slowest, loose ground under half a second, and not one number pasted onto
everything.

**Two defects this turned up, both of them in things nothing else was asking.**
A refusal on the client was wiped by ANY BlockChange, so on a world with a mod
in it -- which is every world this game ships -- twelve refusals in a row read
back as an empty string, because `mods/beacons` had changed a cell somewhere
else in between. It is cleared by news about the CELL it was about now. And the
mod's own set piece was not idempotent: it found its ground by looking for the
first non-air block, so a world loaded from a file already had the outcrop in it
and the mod stacked a second one on top, one block higher every restart --
caught by `persistence`, which hashes the world across a restart.

**What it cost elsewhere, said rather than hidden.** Five gates broke, all of
them for the same reason and none of them wrong: they swung once at a block and
a block now takes seconds. `net_loopback` (three sites), the bag probe, the
portal-1 probe and the graphical client's scripted hands all HOLD the button
now, the way a player does -- the client's sequence resends its Edit every 0.15 s
while the dig runs, which is what `dig_step` does for the frame loop.

Seven sabotages, all red on the check they should be: stone losing its hardness,
one number pasted onto every block, an engine block asking for a tool, bedrock
made unbreakable by being very slow, granite asking for no tool, the pick
digging to level 0, the bench handing over nothing, granite coming out in a
frame (caught on the seconds AND on the swing count), the refusal not naming a
level, a refusal wiped by anybody's block change, and the set piece not
idempotent. One of them -- the refusal sentence -- did not build the first time
it was written, and was redone rather than counted.

**A trap this gate walked into first.** The probe swings on a timer, so the
SECONDS it measures have a floor: the first version read 0.21 s against a
threshold of 0.20 and went green over a world where nothing had a hardness at
all. It counts swings as well now, and a swing count has no floor -- a block
that comes away on the first Edit took no time, whatever the clock says.

**And the kinds, 2026-09-08 -- the limitation this entry closed with.** A level
said how good a tool was and nothing said what it was good AT, so a level 2 axe
opened stone as well as a level 2 pickaxe. A block now says what it is MADE of
(`NodeDef::material`) and a tool says what it is FOR (`ItemDef::digs`), and a
tool's level and speed are its own against that material and a bare hand's
against everything else.

Two functions, `net::tool_level_for` and `net::tool_speed_for`, beside the rules
that were already there and for the same reason: the networked server asks them
in `plan_edit`, the single-process client asks them in `break_into_bag` and
`dig_need`, and a rule written twice is two games. Both are total, and both ends
of "nobody said" mean the rule does not apply: a tool naming no material is a
general one, a block that does not say what it is made of is dug by anything for
whatever it is worth -- which is every block and every item registered before
materials existed, so declaring them changed nothing that shipped.

A STRING and not an interned id, deliberately: it is read twice per dig and
never in a loop over cells, so a table both registries and the wire agreed on
would buy nothing and cost a schema.

The engine's blocks name four materials -- stone, soil, wood, glass -- and not
one each: a material per block is a table where every tool is its own tool,
which is a list and not a rule. Bedrock names none, because nothing is ever the
right tool for the floor of the world.

`mods/tools` grew the tool that makes the point: `tools:shovel` is BETTER than
the pick by every number it has (level 1, four times a hand) and it does not
open the granite at all, because it is for soil. Measured through a real server
in `tests/tools.sh`: the same `core:grass` cell takes 0.72 s bare-handed, 0.71 s
with the pick -- which is the pick being no help at all in soil -- and 0.31 s
with the shovel; the shovel leaves the granite where it is and is told "that
needs a level 1 tool for stone, and this one is level 0 against stone". The
sentence carries the material because the number alone reads as a lie to
somebody holding a level 3 axe: the axe IS level 3.

Twelve sabotages, all red on the check they should be: the material ignored, only
an exact match counting (so nothing that said nothing works), the speed
forgetting what the tool is for, two engine blocks made the same work, the
refusal dropping the material, the pick made general again, the shovel made a
stone tool, the granite made of nothing, the single-process client using the raw
level, its clock using the raw speed, and the binding dropping the material at
each of two hosts.

**Said rather than counted: two of these gates were written after the code.**
The `mod_reload` binding check and the `hud` material checks passed the moment
they were written, so they never ran red -- they are proved by their sabotages
(M10, M11, M13) and not by a red run. The `dig` and `tools` halves did run red
first, against a build where the rules ignored the material.

**What is left.** A block has ONE material and a tool is for ONE material.
The original engine's groups are a SET on each side with a number per group, which is what
would let a block be both soft and stony, or a tool be good at two things. That
is a bigger thing and nothing here needs it yet: what this closes is that every
tool was the same tool with a different number on it.

**Milestone.** M12 contents, and the tool half is on M11's road: `param2` and
drawtypes are named there and `register_tool` is in the same list.

---

## CLOSED-33 -- the dig bar stayed on screen after the button came up

**From play, 2026-09-09.** Holding the button half way through a dig and then
letting go left the bar under the crosshair where it was, full of a dig nobody
was doing, until the next dig started.

**Nothing drew it wrongly.** `dig_step` sets the progress on every call and
zeroes it when nothing is being dug; the frame loop simply stopped CALLING it.
Its dig branch is inside `cursor grabbed && no panel && aimed && button down`,
so releasing the button, aiming at the sky or opening the bag left the last
value it had been handed sitting in `Scene::dig_progress`.

**So "not digging" is now something a frame says.** `dig_frame` takes the four
ways a frame is not a dig -- the button up, nothing under the crosshair, a panel
holding the mouse, something else claiming the click -- and `dig_idle` puts the
state and the bar back to nothing. Both loops call them in the same place, right
after the raycast, before anything asks whether this frame IS a dig.

Gated twice: in `dig` on the rule and the sequence (half a dig, then the button
up, and the bar reads nothing with no dig left running), and in `hud` on the
PIXELS, which is the shape the bug had -- 1118 pixels of bar while digging and
1118 after letting go, against 1118 and 0 now. Two sabotages red: an idle frame
that changes nothing, and a `dig_frame` that says yes to everything.

**Not gated: the two call sites.** The rule is gated and the wiring is read. A
gate for it would have to drive the real frame loop with synthetic input, and
this repo has no way to do that yet.

**Milestone.** None.

---

## CLOSED-32 -- the chest opens with no animation

**Asked for, not a defect.** Right clicking a chest should show its lid opening.

**Where it stands today.** `mods/chest` claims the click and calls
`core.container_show`, which puts the panel on screen at once. The chest itself
is a static cube: six faces, one of them wearing the front picture since
CLOSED-24, and nothing about it moves. Closing it is the panel disappearing.

**What is missing is not a lid.** It is that a BLOCK in this engine has no way
to be anything but a cube of six faces, and no way to change over time. The
mesher emits one quad per exposed face from the block table; there is no
sub-block geometry, no per-block animation, and nothing that would carry
"this chest is 40% open" to the people watching. Bodies animate -- `ModelRender`
has joints and bones, and a character wears a .gltf -- and blocks do not.

**So there are two shapes for this and they cost very differently.**

  - **A block that is drawn as a model.** The chest stops being a cube and
    becomes a .gltf with a hinge joint, drawn by the pass that already draws
    bodies. Everything needed exists on the client since CLOSED-28; what does
    not exist is a block saying "draw me as this model instead of as a cube",
    the mesher leaving a hole where it stands, and the state of the animation
    reaching other players.
  - **A nodebox that moves**, which is DESIGN.md §2.6's own next step for the
    vertex format ("sub-block geometry will need a finer fixed point; the field
    is already a byte, so that is a scale change rather than a format change")
    and M11's drawtype work.

**And a smaller question inside it.** Who sees it. A chest is opened by one
player and stands in a world other players are looking at, so "open" is state on
the CELL and not on the session -- the same shape as the facing byte, and the
same argument: a lid that only the person who clicked can see is not an
animation, it is a local effect.

**Half done, 2026-09-11: the shape.** A block can be drawn as BOXES now, which
is the half the entry above calls "what it has no way to do is be DRAWN". The
chest is a body and a lid, and nothing moves yet.

**Neither of the two shapes above, and the reason is the vertex format.** A
nodebox in the chunk mesh was the plan on paper -- DESIGN.md 2.6 calls a finer
fixed point "a scale change rather than a format change" -- and it is off by
one: positions there are bytes, one unit per node, 0..32 INCLUSIVE, so eighths
want 0..256 and a byte holds 256 values ending at 255. Widening the field costs
every chunk in the world memory it does not need for a handful of chests, which
is the cost M9 spent itself removing.

So the boxes are not in the chunk mesh at all. The mesher leaves such a cell
alone and hands it over -- `PartCell`, where it is, what it is, which way it is
turned and how lit it is -- and the client collects the visible ones into ONE
buffer, rebuilt per frame and drawn in one call, with the SAME shader and the
same atlas the cubes use. A second shader would be a second place for the
daylight rule, the tile arithmetic and the face shading to live.

  - **A block with boxes stops being opaque**, whatever it said. One truth, in
    the registry: the alternative is every mod remembering to turn opacity off
    beside every shape, and the one that forgets gets a hole in the floor
    behind its chest.
  - **A turn turns the SHAPE.** A cube is six quads on the walls of its cell, so
    turning it can only move pictures between them; a box has a shape, and the
    face it ends up looking along is the one it wears and the one that shades
    it.
  - **Collisions are still the cube's.** A shape is what a block looks like, and
    physics has not been told. Written down rather than discovered.

`core.node_box(id, x0,y0,z0, x1,y1,z1, part)` is how a mod says it, one box at a
time, and the part's NAME is what a hinge will hang on when something moves.

Gated in `mesher` (no cube in either stream, the neighbour keeps the face it
would have hidden, the cell handed over once with its turn and its light, and a
chunk of ordinary blocks hands over nothing), in `mod_reload` (the binding
carries the boxes and the name of the piece), and in a new `parts_draw`, which
counts pixels: something is drawn at the cell, it covers LESS of the frame than
the cube it replaced -- 1.23% against 5.79% -- turning the cell changes the
picture, and a chunk whose only content is a box still draws it, which used to
be dropped for having no mesh. Six sabotages red.

**Said rather than left.** One line is belt and braces and a sabotage of it
stays green: the mesher asks "is this block boxed" before it asks about opacity,
and the registry has already forced a boxed block to be non-opaque, so the two
answers coincide. They come apart the day a translucent nodebox exists.

**And the lid moves, 2026-09-11.** A NAMED box swings: up about its own
back-bottom edge while somebody has the cell open, down when the last of them
closes it. The engine never learns what a lid is -- it asks whether the box was
given a name, which is the same name a mod already had to write to say which
piece was which.

**What travels is a BIT, and it is about the CELL.** `MsgType::CellOpen`
(protocol 25) says "somebody has that cell open" or "nobody has, any more", to
whoever has the chunk -- the same rule a block change follows. The swing between
the two is each client's own picture, a quarter of a second at whatever frame
rate that machine has, so the wire carries one message per lid and not sixty.

Three decisions inside that, each of them the cheap answer being wrong:

  - **Not a field on BlockChange.** Nothing about the block changed; a client
    that treated it as one would relight the cell and rebuild the chunk mesh
    every time a lid moved.
  - **Not stored with the cell.** A chest is open while somebody is looking
    inside it, which is true of a moment and not of a world -- so it is not in
    the chunk, not in the world file, and a client that joins while a chest
    stands open is told nothing until it changes. Said here rather than hidden.
  - **Asked once a tick, not sent from every place a container opens or
    closes.** Those are the click, the close message, walking out of reach, the
    container erased under somebody, a session dropped and a session
    redirected; the one that forgot would leave a chest standing open with
    nobody in it. `ServerSessions::open_cell_changes()` diffs instead, which is
    also what lets a gate ask it without a socket.

Gated in `protocol` (the message and a byte that is neither open nor shut being
refused), in `net_loopback` (opening says so once, says nothing more while it
stands open, and says so again when the last one closes), in `tools` end to end
through the real server and the real chest mod (the cell goes open on a right
click and shut on close), and in `parts_draw` on pixels: an open cell is not
drawn like a shut one, the shape reaches HIGHER open -- row 431 against row 502
-- half way is neither, and the body of the chest moves in exactly 0 pixels
while the lid does. Five sabotages red.

**A gate that was green over nothing, again.** The lid checks passed a sabotage
that swung EVERY box instead of the named one: a chest tipping over as one piece
also reaches higher, also differs from shut, and also has a half way. The body
check is what separates them -- 0 pixels moved against 20340 when everything
swings.

**What is left.** The two lines that wire the tween into each client loop are
read and not gated, the same limitation the dig bar's have. And a networked
client without the mod still draws a cube, because the block table on the wire
is names: the SHAPE does not travel, which is the same hole as OPEN-28 in
another place.

**Milestone.** M12 contents, resting on M11's drawtypes.

---

## CLOSED-30 -- flying off the edge of the world took the world with it

**Symptom, as reported from play, with a screenshot.** Fly away from the island
and the whole island disappears, leaving the portal hanging in an empty sky. The
F3 line in the shot says it exactly:

    eye -35.40 31.26 96.09
    chunks 0 drawn / 0 culled    73128 verts    world 32 chunks 2.4 MB

Zero drawn and zero CULLED. Not chunks thrown away -- chunks never considered.
And 73128 verts still in memory, so nothing had been unloaded: the frame simply
did not draw any of it. The portal survived because it is drawn by a pass of its
own, which is why it was the one thing left.

**What it was.** The occlusion walk starts at the chunk the camera is IN and
will not cross a chunk it has never been told about -- a rule with a measured
reason behind it (treating the unknown as see-through cost 2.5 ms a frame
wandering through empty space beyond the world). With the eye at x = -35 in a
world 128 wide, the camera's chunk is not one this client has, and neither are
its six neighbours. So the walk ended where it began, `m_reached` held one
chunk, and every mesh in the world was dropped as unreachable.

**The fix.** When the camera's own chunk is unknown, the camera is in open
space, and open space outside the world is all one piece -- so the walk is
seeded with every known chunk that TOUCHES it, entered through the face that
touches. The walk then carries on normally, which keeps the culling instead of
giving it up: a room sealed in rock is still not entered from outside.

Assuming that space is connected to the camera can only over-draw -- an unknown
pocket sealed inside the world would be seeded as if it were the sky -- and
over-drawing costs draw calls where the other direction costs a hole. The same
asymmetry the rest of the walk is built on.

**Gated in `frame_budget`**, from the eye height the screenshot was taken at,
looking back at the world: the same view drawn with the walk and without it,
which is the rule that catches a hole. Red first, and it reproduced the
screenshot exactly -- `0 chunk(s) drawn with the walk, 297 without, 1242564
pixel(s) differ`.

**And two green sabotages, which is the part worth reading.** Seeding chunks
with no unknown neighbour instead of with one, and entering them through the
opposite face, both came back GREEN. Neither is right, and on that world neither
was observable: it is two chunk layers tall with ten-block hills, so from
outside there is a way round every occluder, the walk culls nothing (297 either
way), and "reached the right chunks" and "reached everything" are one
measurement. That is OPEN-23 wearing a different face.

So the gate got something with two answers in it: the seven chunks of stone the
checks above already build, with the middle one hollowed out. A chunk of air
whose six neighbours are solid rock is unreachable from outside by any path, and
reachable by a seeding rule that is too generous. `occluded_chunks()` says which
happened -- 1 sealed away when it is right, 0 under either sabotage -- and all
four sabotages are red now.

**Not claimed:** that the seeding SAVES anything from out there. On this world it
draws the same 297 chunks the frustum alone reaches, and the saving on a world
with chunk-scale occluders is not measured here. What is gated is that there is
no hole.

**Milestone.** None.

---

## CLOSED-34 -- the bag drew anonymous squares and never said what anything was

**From play, 2026-09-09, two halves of one complaint.** A pick and a shovel are
two identical grey squares, and a stack of somebody else's mod's blocks is a
colour: the picture says what a thing looks like and nothing said what it IS.

**The name, first.** `ui::name_under` asks a panel which square a point is in --
the panel's own hit test, so nothing recomputes the grid -- and
`ui::draw_item_tooltip` draws it beside the cursor, clamped back onto the screen
at the edges. Drawn only with an empty hand: while a stack is being dragged the
answer would be whatever square it is passing over, which is not what the player
is asking about. Bag, chest, craft grid and the result square, first answer wins.

**And the picture.** An item asks for one file under its own name --
`tools_pick.png` for `tools:pick` -- and wears the tile it gets, in the same
atlas the blocks are in and out of the same budget of 256. So a pack dresses a
tool with one line of its manifest and the engine never learns a second kind of
name. Three rules that are not conveniences:

  - **Items are planned AFTER every block.** Wanting a picture for an item must
    not move a block's tile: those numbers are in vertices already built.
  - **One picture, one tile.** An item whose file a block already wears shares
    that block's tile, which is what makes the item form of stone cost nothing.
  - **No placeholder.** A block with no file gets one baked from its colour; an
    item has no colour, so an item nobody drew gets NOTHING -- and the bag falls
    back to the block it places, and to the plain square for a tool.

`add_items_to_media` serves the same files from a world, so this reaches a
networked player and not only the single-process one.

**Measured, in the game that ships.** With `--textures assets/packs/kenney`:
"items: 44 in all, 18 with a picture of their own, 3 that place no block", and
by name, `tools:pick` drawn, `tools:shovel` drawn, `palette:brush` a blank
square -- nobody drew that one. Kenney's own archive has the art: its `PNG/Items`
folder is 128 px, exactly the tile size that pack agrees on, so pick_iron.png and
shovel_iron.png went in without being resized.

Gated in `face_tiles` (an item's own file, the sharing, the ceiling, and that no
block's tile moved), in `media` (the world serves the square somebody drew, under
the engine's name, and costs no extra blob), in `pack_draw` (a pack dresses an
item and the NUMBER comes back in the item table), and in `hud` (the name under
the cursor, the box it is drawn in, and an item wearing its own picture rather
than the blank square). Seven sabotages red.

**Two things about the gates, said rather than left.** The tooltip's width check
was green over nothing TWICE: first it compared how much of a fixed band beside
the cursor changed and read 19.0% for both of two ten-character names; then, with
a long name, it still passed a box drawn at a fixed width -- 13.0% against 19.7%,
over a threshold of 1.5x by two tenths of a point -- because the name's shadow is
drawn in black whether or not a box is under it. It measures the box along a row
three pixels below the top of what appeared now, which is border and background
with no letters in it: 108 px against 609 px of letters when the box is nailed to
a fixed width. And one sabotage stayed green legitimately -- deleting the
empty-square check in `name_under` changes nothing, because kNoItem's own name is
empty and the line below already answers "". It is belt and braces, and the code
says so.

**What is left.** The pack that ships dresses no tools: `assets/textures` is this
engine's generated art and has no `tools_pick.png`, so with the default textures
the two tools are still squares. The Kenney pack dresses them, and any pack can.

**Milestone.** M12 contents.

---

## CLOSED-29 -- a mod's block write costs a full relight, every time

**Closed, 2026-09-12, by both of the things the entry asked for, and they are
independent.**

**The skip is exact and not a heuristic.** `compute_lighting` reads two things
about a block and only two -- whether it is opaque, and what it emits -- so two
blocks that agree about both are two blocks the light cannot tell apart.
`light_unchanged` is that comparison, and it lives in `voxel/light.h` because
"what the light depends on" is that file's business: a caller's copy of the list
would be a second opinion that stops agreeing the day the pass reads a third
thing. Stone written over stone, or one wall becoming another, now costs nothing
at all.

**And the run is lit once.** `LightBatch` collects the cells whose light may
have changed and lights them at the end, in BOXES -- because the pass takes a
box, so cells near each other cost one pass between them, and because one box
over everything would mean a mod writing at two ends of the world lit the middle
too. At most sixteen boxes, so the work is bounded whatever a mod does; past
that the cheapest pair is merged, which over-lights and never under-lights.

**The decision is a comparison and not a threshold.** A cell joins the box it
costs least to join, measured in the cells a FLUSH would have to light -- the
box grown by the light radius on every side. The first version compared bare
volumes, and joined a cell five hundred blocks away to a box because a line of
501 cells looks cheap: expanded, that pass is 580000 cells against the 36000 of
a box of its own. The gate has that case in it now ("two writes far apart are
two boxes"), and it went red on the first version.

**Measured on 776 writes** -- a 640-cell wall, eight lamps in the open, a roof,
and 64 writes the light cannot tell apart: **1.808 s a pass each against
0.015 s batched**, over 220528 cells in four boxes.

**And the batch is not only faster, it is more nearly RIGHT**, which was not the
claim this set out to make. The gate's first version compared the batched light
against the per-write light and went red. The per-write path is not a reference:
a box pass seeds the emitters INSIDE its box, so a later pass over an
overlapping region erases light that came from a lamp outside its own box. The
reference is a full pass over the world after the writes, and against that one
the **batched run differs in 0 cells and the relight-per-write in 626**. That is
the pre-existing shape of `relight_around` rather than something this change
introduced -- bigger boxes have fewer edges, so batching reduces the error it
inherits.

**A fixture that could not see the margin, found by a sabotage that stayed
green.** With the pass told not to grow its box by the light radius -- which
should leave a seam of stale light at every edge -- the gate was still green:
the writes were a one-cell-thick wall and lamps buried in stone, and sunlight
falls straight DOWN, so a vertical wall shades nothing beside it and a lamp in
solid rock lights nothing at all. The fixture has lamps in the open and a roof
now, and that sabotage is red.

**Where the runs are.** The server opens one around the mod directory loading
and one around each tick, which is every place mod code can write a cell; a
player's edit is one cell and still relights on the spot. The flush happens
before anything reads the light -- the chunk encoder and the streamer run after
the tick, never inside it -- so no client can be sent a chunk whose light is a
tick stale.

**And the single-process client had the opposite defect.** Its `host_set_block`
wrote the cell and relit NOTHING, so a mod's lamp was dark there exactly as it
was dark on every networked client before CLOSED-17. Per write it cannot be: a
pass around one cell is about 3 ms and mods/beacons writes one every frame. So
that loop batches and flushes at most four times a second -- 12 ms of every
second -- and the compromise is stated rather than hidden: a mod's light lands
within a quarter of a second of the write rather than in the same frame.

**And a second green sabotage that was dead code.** Refusing to start a new box
at the cap made the merge below it unreachable: at sixteen boxes a far-away cell
was joined to whichever box was nearest instead. A cell that costs more to join
than it costs alone gets its own box now, at the cap as well, and the cap is
restored by merging the two boxes cheapest to merge -- which is what the merge
was written for.

Gated in a new `light_batch`: what the light depends on, the equality against a
full pass, the cost, and how a batch keeps its boxes. Five sabotages, all red on
distinct checks -- the skip asking about opacity and not about emission, a box
not grown by the light radius, boxes decided on their bare volume, the cap not
enforced, and a flush that leaves its boxes behind. Three of those five were
green first, and each of the three bought a check.

See OPEN-29 below for the measurement this started from.

**Milestone.** None. M9 is performance and this was not in it; but M11's
compatibility layer is a mod that writes cells by the thousand, and 1.9 ms a
cell is what that would have cost.

---

## CLOSED-43 -- a block's shape was a picture of a shape

**Closed, in five commits.** `NodeDef::parts` reached the mesher and nothing
else, so a mod's slab was a picture: stood on half a node above itself, with the
empty half of its cell a wall, selected by aiming at the sky over it, and built
on at the top of the cell. That is what M11 means by `drawtype nodebox`, and
`yaportal` alone has fourteen of them.

  - **The boxes decide where a body goes and what an aim finds.**
    `core/registry/node_shape.h` answers where a cell's boxes are, turned the
    way the cell is, with the mesher's own rotation; the collision resolver, the
    server's movement validator and the raycast read it, where they had been
    three copies of "is this cell solid". 177 957 probes against a slab, a shelf
    and a cube and 81 000 over all twenty-four orientations of a two-box step:
    0 disagreements with what would be drawn.
  - **The game has them in it.** `mods/steps` -- a slab and a stair, stone,
    makeable -- and `tests/shape.sh`, which asks a real server on the real
    `mods/` directory: standing on the slab's top is accepted, half a node lower
    is refused, and the stair's two steps are the same cell at the same height
    with opposite answers. The outline is drawn around the boxes: 5729 pixels
    around a cube against 2814 around its boxes.
  - **A body walks onto one**, up to `kStepHeight` 0.55, and not up a whole
    node: a cube stays a wall. The step sets the body down on the surface --
    4.501 on a slab whose top is 4.5 -- and is refused against a ceiling.
  - **The shape travels** (protocol 27). The block table was names only, so a
    client without that mod saw a magenta cube and its own collision refused to
    stand on a slab the server accepts. 163 bytes for 6 blocks in a welcome of
    1835, and the server's shape wins over a name the client knows.
  - **And a block can be a MODEL**, which is M11's other drawtype: `NodeDef::
    model`, `core.node_model`, drawn through the bodies' glTF pipeline and
    turned about the middle of the cell. `mods/steps/marker.gltf` ships one.

**Twenty-four sabotages across the five**, and three checks that were green over
nothing and had to be rewritten: the outline's turn (the block turns in both
frames, so the pixels an outline covers move whatever the outline does), the
step's headroom (proved by the wrong half of itself), and one case that stood
its body inside a wall. Two defects in the model pipeline came out of the last
commit, both about a model with no skeleton -- which every block's model is.

**What stays open, and each of them is a sentence somebody can act on:** a
dropped item lands on the cell and not on the box; `selection_box` is not
separate from `node_box`; a box that leaves its cell does not stop anybody in
the neighbouring one; only the shape, the turn and the model travel, so a mod's
non-solid block is still a wall to a client that has never heard of it; and a
model block takes the bodies' directional light, does not animate, and is not
cut at a portal. They are in OPEN-43 below, which is the entry this closes.

**Milestone.** M11 asked for it, and this finished the engine's own list for that
milestone: `param2` facedir/wallmounted as a trait (CLOSED-27), an ABM/LBM
scheduler (OPEN-42) and `drawtype nodebox+mesh` (this). M11 itself was cancelled
the same day -- see the note at the top of this file -- and none of the three
was wasted: a block with a front, a world where something happens on its own,
and a block that is not a cube are things this game wanted anyway.

---

## OPEN-1 — a portal's carried speed will fail the server's speed check

**Symptom.** None yet in play. A crossing that turns a fall into forward motion
produces a horizontal speed the server's movement validator refuses.

**Where.** `MovementLimits::max_horizontal` is `kFlySpeed` (14.0), and
`movement.cpp` allows `max_horizontal * tolerance` = 14.0 × 1.6 = **22.4 m/s**.
A fall reaches `kTerminalFallSpeed` = **60 m/s**, and a floor-to-wall crossing
maps that straight onto the horizontal axis — see `docs/M4_FINDINGS.md` 20.1,
where a 11.67 m/s fall comes out as 11.67 m/s forward. Anything above about a
1.6-second fall clears the cap.

**Reproduce.** Not reproducible today, and that is the point: the network client
keeps portals off (`Scene::portals` is false when connected), so nothing sends a
post-crossing position to a server. It becomes reproducible the moment M5 or M6
turns them on — fall four nodes into a floor portal, come out of a wall portal,
and watch the server correct the position back.

**Hypothesis.** Already designed, and by the same route: this is the fork's
BUG-10 ("moved too fast" post-swap). `DESIGN.md` 3.4 answers it with a **grace
token** — the swap packet carries a token that suppresses the speed check for N
ticks on the destination server. What is missing is not the idea, it is that
nothing has been built or measured.

**What would close it.** The token, and a gate that crosses with enough carried
speed to break the cap and shows the server accepting the position — plus the
sabotage that removes the token and shows it refusing. Raising
`max_horizontal` instead would be the wrong fix: it is the number that stops a
speed cheat, and a portal is not a reason to stop checking.

**Milestone.** M5 or M6, whichever first sends a player across a portal to a
server.

**Closed, mechanism only.** The grace is in: `MovementState::grace_until`,
`MovementLimits::portal_grace` (1.0 s), and `ServerSessions::teleport`, which is
the only thing that opens the window -- there is no message a client can send
that produces one. It suppresses the two speed checks and nothing else, so a
graced claim inside a block is still refused. Measured with the window open and
shut in `chunk_codec` (twenty blocks in a tick, 60 blocks a second sideways, the
same after expiry, and inside a block) and over a real socket in `net_loopback`,
where the same half second of carried speed is accepted with the grace and
refused without it. See `docs/M6_FINDINGS.md` 1.

What is NOT done, and why this line stays worth reading: the server has no
portals. Nothing calls `teleport` on behalf of a crossing yet, and the client
keeps portals off while connected, so nothing but the gates exercises the path.
Server-side openings are the next piece.


---

## OPEN-2 — "sometimes the crossing wedges me in an exit block"

**Symptom.** Reported from play alongside a crossing that sometimes did nothing
at all. The second half of that report is fixed and measured
(`docs/M5_FINDINGS.md` 11.2 and 11.3); this half is not reproduced.

**Where.** The suspected mechanism is the one that was corrected: with the exit
a tenth of a node outside the destination block, two tenths of the player's box
are inside it, and the arrival resolver used to answer an occupied arrival by
stepping UP whole nodes -- which puts those two tenths against the wall beside
the hole. A step that lands there and is accepted leaves the player standing
inside the wall, which is what being wedged feels like. That path is gone, but
"the thing I fixed would have caused it" is not the same as having seen it.

**Reproduce.** Not reproduced. `dual_session` section 7 sweeps 110 crossings --
every entry offset a fortieth of a node apart, three approaches, both
directions -- and counts the outcome: landed outside solid but unable to move
half a node in two seconds. It reads 0. Section 8 obstructs the arrival, flat
and inside a wall, and reads 0 there too.

**What would close it.** A case that shows it. The likeliest places the sweep
does not cover: a destination whose terrain is higher than the source's by more
than three nodes, an arrival inside a body rather than a block (there is no
body-versus-player collision yet), and a crossing taken while the camera warp
from a previous one is still settling.

**Milestone.** M5. The counter stays in the gate either way -- if it comes
back, something fails on its own.

**Update.** The report was refined: it happened in the `--oriented` scene, along
with the view snapping after the crossing. Both are explained and measured by
the arrival resolver -- see `docs/M5_FINDINGS.md` 12: in that scene every
crossing was refused outright before the fix, and the old resolver's only answer
to an occupied arrival was to move the player up whole nodes, which hands the
camera a warp that size. `portal_orientation` sections 9 and 10 now measure
both: 0 refusals, 0 wedged, and a camera warp of 0.23 nodes against a threshold
of half a node. This item stays open only for the wedging that has still never
been observed directly.

## OPEN-3 — "the view direction still snaps after a crossing"

**Symptom.** Reported from play a second time, in the `--oriented` scene, after
the arrival resolver was fixed: the direction the player is looking jumps some
time after the teleport rather than at it.

**Where.** Unknown. The three things that can move a view without being asked
are known and two of them are cleared by measurement: the roll animation moves
the horizon and not the direction (3,00-3,15 degrees a frame, by design), and
the camera warp moves the eye and not the direction. The third is the pitch
limit at 89 degrees, which is untested against a real crossing and is exactly
where an exit out of an opening lying on its side leaves the look.

**Reproduce.** Not reproduced in a gate. `portal_orientation` section 9 walks
nine poses through the crossing frame by frame and reads at most 0,02 degrees a
frame -- but it drives the player with a scripted input at a fixed 60 frames a
second and a still mouse, which is three things a player does not have.

**What would close it.** A frame from a real session with a large `unexplned`.
`--log-view` prints one line a frame for two seconds around every crossing, and
the six frames before it; `docs/M5_FINDINGS.md` 12.4 says what the columns mean
and section 11 of `portal_orientation` measures that the trace reads a 30 degree
snap as 30 degrees and invents none for the pitch limit or the roll.

**Milestone.** M5.

**Closed.** It was the pointer warp. Grabbing the cursor warps the pointer to
the centre of the window and the platform reports that as a mouse delta on the
frames after; two frames of it were dropped at startup and never again, so every
ESC out and back in threw the view by however far the pointer had wandered while
it was free -- up to 2050 pixels, which is 5.1 radians. That is why it happened
"every so often" rather than every crossing, why it went "towards the exit
direction" (the pointer wanders to the middle of the screen, and taking it back
puts the view back where it was before the mouse was let go), and why no gate
ever saw it: no gate presses ESC. Fixed by making the drop a type with a name,
`LookSettle` in client/player.h, since the defect was an int re-armed in one
place and not the other. `portal_orientation` section 16 checks it, sabotage
included: 0 pixels reach the view with the re-arm, 2050 without. Confirmed in
play -- 14 crossings, 6775 lines, no events of any kind, worst unasked turn 5.55
degrees against a threshold of 12. See `docs/M5_FINDINGS.md` 12.9.

**Update.** Asked whether the crate that slides through the opening could be
doing it. Measured: the same walk with and without the crate differs by exactly
zero in position, angles and camera warp over 400 frames, 59 of them with the
crate in two worlds at once, and the gate goes red when the crate is made to
push the player (`docs/M5_FINDINGS.md` 12.5). The trace grew a `crate` column
anyway, so a log can say it rather than a gate.

The same log said the trace itself was blind: the client runs uncapped at about
0.30 ms a frame, so a 120 FRAME window after a crossing was 36 milliseconds
wide. The window is two seconds now and section 13 checks it at both frame
rates (12.6).

## OPEN-4 -- the arrival resolver lifts the camera two thirds of a node

**Symptom.** Measured, not reported: a real session in the `--oriented` scene
produced camera warps of 0.651 and 0.692 nodes, both vertical, absorbed over the
0.15 s the camera is given. That is the eye rising two thirds of a block in a
sixth of a second, which is a lurch whether or not the look direction moves.

**Where.** The arrival resolver answers an occupied arrival by lifting the
player to `floor(y) + 1`, a whole node top. When the arrival lands a third of
the way into a node, the lift is the other two thirds and the camera gets all of
it. `portal_traverse.cpp`, the resolver loop; the camera side is `kCamWarpTime`
in the client.

**Reproduce.** Not by a gate today, and that is the finding.
`portal_orientation` section 10 walks six hand-written poses through the same
pair and its worst warp is 0.23 nodes against a threshold of 0.5 -- green
because the poses do not reach where a player reaches, not because the engine
keeps the warp small. Play reached three times that.

**What would close it.** Poses that cover what a player does -- approach angle,
speed, whether they are falling -- so that the gate produces the 0.69 the log
did, and then a resolver that lifts to what is actually needed rather than to
the next whole node, or a camera time that scales with the distance. The
threshold stays at half a node.

**Milestone.** M5. See `docs/M5_FINDINGS.md` 12.7.

**Closed.** Both halves. The poses first: `portal_orientation` section 17 sweeps
the approaches rather than listing them -- five offsets across the opening, five
ways of arriving, both directions, fifty crossings -- and reaches 1.420 nodes,
more than play produced. That displacement is geometry and not a defect: the two
openings sit half a node apart in height, and the destination is on its side, so
entering 0.8 to the SIDE means leaving 0.8 DOWN. Coming out of the part of the
opening you went into is what should happen.

What a player feels is the speed, and that is what got bounded: the catch-up
time is now `clamp(distance / 2.5 nodes a second, 0.15 s, 0.60 s)` instead of a
flat 0.15 s. Worst camera speed over the sweep falls from 9.47 nodes a second to
2.50, just under a walk. Giving the camera longer also leaves it behind the body
for longer, so the sweep checks where it sits while it catches up: 0 frames
inside a block, 230 with the camera position sabotaged to three times the
displacement. See `docs/M5_FINDINGS.md` 12.10.

---

## OPEN-5 -- a portal placed on the block you are standing on buries you

**Symptom.** Measured, in a real session against a real server: placing a
horizontal opening on the top face of the block under the player's own feet
produced **97 corrections in five seconds**, all of them `that position is
inside a solid block`, with the player parked at y 19.96 on top of a block whose
top is at y 20.

**Where.** Two rules that are each correct on their own. The client's floor
shift (`portal_floor_shift`, PORTAL_SYSTEM 2.3) raises the bottom of the
collision box so a player can sink through the solid node that holds a floor
portal up -- without it a floor portal cannot be fallen through at all. The
server's movement check refuses a claimed position inside a solid node, and it
refuses it even under a portal grace, deliberately: "a teleport that has to
break the world's own rule is not a teleport this server performs".

So the client sinks on purpose and the server refuses on purpose, and the player
is stuck being corrected once per claim forever.

**Reproduce.** Connect to `cnp_server --portals`, aim straight down at the block
you are standing on, place an opening. The scripted placement in the network
client gate does exactly this if the offset search is removed -- that is how it
was found.

**Why it did not appear before.** Until openings became server state a network
client had no portal anywhere near the player, so the floor shift never engaged.
It is not a regression in the shift; it is the first time a player could put a
floor portal under their own feet.

**What would close it.** The server has to know about the shift, because it is
the one making the rule. The narrow version: a claim inside a solid node is
accepted when that node is the one directly under a horizontal opening the
server holds, and only between the opening plane and the trigger depth. The
wrong version is turning `refuse_inside_solid` off during a grace, which would
make every teleport a licence to stand in a wall.

**Milestone.** M6. See `docs/M6_FINDINGS.md` 3.5.

**Closed.** Three defects stacked, and each one hid the next.

The opening was in the wrong cell: a floor portal is the node that was CLICKED,
not the cell in front of it, because something has to hold it up. In the cell
above, its surface lands a node over the player's head and they stand half a
node behind the plane of a portal at their own feet.

`SolidExemption` then makes the node holding it up passable for the movement
check -- one node, entirely contained, and nothing else. Measured headless and
over a socket, both halves, with the exemption removed as the sabotage.

And a placement stopped seeding the trigger record: seeding means "you have
already used this one", which for a portal placed at your own feet means you
sink and never cross.

Measured end to end: 1 crossing, 0 corrections while sinking, against 0
crossings and 97 before. See `docs/M6_FINDINGS.md` 4.

---

## CLOSED-6 -- the graphical client hangs about once in twenty runs

**Closed on evidence, 2026-09-11, with no cause.** It has not been seen since
the runs recorded below, and the scenario that used to produce it -- one client
through `cnp_netshape` at 60 ms one way, 20 ms of jitter, 2% loss -- has been
run sixty times without a hang: forty with the rebuilt watchdog armed and twenty
with none at all, which is the exact configuration that hung 2 in 5. At the old
rate the twenty alone should have hung about eight times.

**Nothing here claims a fix**, and it is closed anyway, which needs saying
because this file's own rule is that "it seems to work now" does not close a
line. What closes it is the other half of that rule: something fails on its own
if it comes back. `--watchdog <seconds>` is that something. Every test that runs
the graphical client passes it, a healthy run there renders about five thousand
frames a second, and fifteen seconds without one aborts with a symbolised stack
instead of sitting there until ctest kills it. So the next sighting arrives as a
stack rather than as a red with nothing in it, and it will be a different
investigation from this one -- which is the honest shape of this closure: the
fault was never diagnosed, it was made unable to hide.

Three things below are carried forward rather than left as history. The watchdog
no longer signals periodically, so it is no longer a suspect in its own
measurements. A client that never exits is not automatically this entry -- the
one stack ever captured was a `--play-seconds` flag accepted and ignored on the
single-process loop, which from outside looks exactly the same. And the
2026-09-08 face, every gpu test failing at window creation for an afternoon and
none the next morning, has no reproducer and is not known to be this at all; if
it returns, capture the display from outside the test before rerunning anything.

**Milestone.** Housekeeping; none depended on it. What follows is what it said.

## OPEN-6 -- the graphical client hangs about once in twenty runs

**Symptom.** `cnp_client --connect ... --play-seconds 8` occasionally never
exits. ctest kills it at its timeout and reports a red that says nothing about
the code. Seen three times: `net_client` during a serial `ctest`, `dual_session`
during `ctest -j4`, and once in ten consecutive runs of `tests/net_client.sh`.

**Where: not known.** What has been ruled out matters more than what is
suspected, because the first diagnosis was wrong:

- **Not contention between the gpu tests.** One of the three happened in a
  SERIAL run, where only one gpu test exists at a time. And deliberately loading
  the machine -- four extra graphical clients rendering throughout -- did not
  reproduce it: `ctest -L gpu -j4` stayed 8/8 at the same 8.4 s wall time.
- **Not the media cache.** Reproduced with a warm cache, with a fresh empty one,
  and the one run with `--no-media-cache` finished (failing its media gates, as
  it should).
- **Not a wedged network session.** The server logs the join every time.

**The trap, recorded because it cost an hour.** The last line in a hung run's
log is always the same one, which reads like the place it stopped. It is not:
the client's stdout is a FILE, so it is fully buffered, and a process killed by
a timeout loses whatever had not been flushed. Under `stdbuf -oL` the same
scenario ran 15 times without a hang and 25 more after that -- which is either
the buffering changing the timing or the flake simply being rarer than the
sample. Either way, a log tail from a killed client is not evidence about where
it was.

**It gets much worse with latency.** Through `cnp_netshape` at 60 ms one way
with 2% loss: **2 hangs in 5 runs**, against about 1 in 50 on loopback. That is
the strongest lead there is -- whatever it is waits on the network, and the
frame loop stops with it: the shaper recorded about two datagrams a second from
a client that should be claiming a position twenty times a second.

**It does not happen under a debugger.** Six runs of the same shaped scenario
under `gdb -batch -ex run`, all six finished. `/proc/sys/kernel/yama/ptrace_scope`
also refuses attaching to it after the fact, so the two obvious ways in are both
closed: attaching is forbidden and being there from the start makes it go away.

**Everything that watches it makes it stop.** Three different observers now,
and all three hide it: running under `gdb` (6 shaped runs, all finished),
line-buffering its output with `stdbuf -oL` (15 then 25 runs, none), and arming
the watchdog below (28 shaped runs, none) -- against 2 hangs in 5 shaped runs
with none of them. That is not a diagnosis, but it is a shape: whatever this is
depends on timing finely enough that a signal once a second is enough to
disturb it.

**The instrument is built, so the next one leaves evidence.**
`src/core/diag/stack_dump.{h,cpp}`: SIGQUIT makes the process write its own
stack and carry on, and `--watchdog <seconds>` makes it write the stack and
abort when the frame loop stops moving. Verified by making `watchdog_beat()` a
no-op: the client exits 134 with a symbolised stack instead of hanging. Every
test that runs the graphical client now passes `--watchdog 15` -- a healthy run
there renders about five thousand frames a second, so fifteen seconds without
one is unambiguous.

So the next occurrence is a failure with a stack attached rather than a ctest
timeout with nothing in it. Until that stack exists there is still nothing to
fix: two samples taken from a HEALTHY client both landed in X11 round trips
(`xcb_wait_for_reply64`, `glXSwapBuffers`), which is where a client spends its
time and is not evidence about the hang.

**A mechanism for "everything that watches it makes it stop", found by reading
rather than by measuring.** `--watchdog` arms a repeating one-second `SIGALRM`:

    itimerval t; t.it_interval.tv_sec = 1;      // core/diag/stack_dump.cpp
    ::setitimer(ITIMER_REAL, &t, nullptr);

`SA_RESTART` does not cover `poll` and `select` -- they return `EINTR` whatever
the flag says. So if this hang is a wait inside one of those, the watchdog
breaks it once a second, and the 28 clean shaped runs are a property of the
INSTRUMENT rather than evidence about the code. It fits the one number recorded
above: about two datagrams a second from a client that should send twenty.

This is a hypothesis and not a finding. It has not been tested, because the hang
has not been reproduced since:

- **12 runs**, a single client through `cnp_netshape` at 60 ms / 2% / 20 ms of
  jitter, no watchdog, watched only by reading `/proc/<pid>/task/*/wchan` from
  another process -- which is the one kind of observer that cannot perturb it.
  All 12 finished.
- **6 runs** of the real `bad_link` scenario (two servers, 200 ms +/- 30, 20%
  loss, 40 s) with `--watchdog` stripped and the same passive watcher. None hung.
  One went red for a different reason, and that one is OPEN-9's missing output.

**The watchdog no longer signals periodically.** It is a thread that sleeps and
checks the beat counter, and the one signal it ever sends goes out after it has
already decided the loop is stuck -- to the main thread by name, so the stack
that gets written is the loop's and not whichever driver thread the kernel
picked. Gated by `watchdog`, whose third section is the property itself: a two
second `poll` with the watchdog armed is not cut short. Putting the old
`setitimer` back turns that check red.

So the instrument no longer hides the fault. Whether the fault comes back is not
known yet: it has not been seen since, with or without.

**Sixty more runs of the scenario that used to fail two in five, and none of
them hung.** The shaped scenario -- one client through `cnp_netshape` at 60 ms
one way, 20 ms of jitter, 2% loss, eight seconds of play -- run twice:

- **40 runs with the rebuilt watchdog armed.** All finished, 8-9 s each.
- **20 runs with no watchdog at all**, which is the exact configuration that
  produced 2 hangs in 5, with a 60 s `timeout` around the client so a hang would
  show as one. All finished.

At the old rate the control alone should have hung about eight times. So the
fault is gone or is now far rarer than it was, and this is the first evidence
that separates "the instrument was hiding it" from "it is not happening": the
watchdog no longer breaks a `poll`, and the run without one agrees with the run
with one.

What is NOT claimed: that anything here fixed it. Nothing was changed with this
in mind, and the commits in between touched the server's liveness rules, the
watchdog itself and the reliability of two messages. Any of those could have
moved it, and a story about which one would be a story.

**A stack, at last -- and of something else (2026-09-06).** A client that would
not exit was caught while M9's crash gate was being sabotaged, and SIGQUIT got
what OPEN-6 has been waiting for since M7:

    main -> run_interactive -> ui::draw_hotbar -> gfx_2d_text
         -> DrawTextEx -> GetGlyphIndex

The main thread was R (running), not blocked -- 13 threads, twelve of them
asleep in futexes, one drawing the hotbar. So it was rendering frames happily.
It is NOT this entry: `--play-seconds` was passed only to run_network, so on the
single-process loop it did nothing and the client ran until somebody closed the
window. A flag accepted and ignored. Fixed, and run_interactive honours it now.

Recorded here because the SYMPTOM is identical to this entry's -- a client that
never exits and a gate that waits for ever -- and anyone reading a future
sighting should rule this out first. Every run recorded above used `--connect`,
which went through run_network, so none of them was this.

**Next step.** Leave it open and keep running. The 30-minute exit test
(`tests/long_game.sh`) is now the longest single run this suite has, and it is
where a hang of this kind would show up next.

**And a different face, 2026-09-08, which may not be this entry at all.** Not
once in twenty: EVERY gpu-labelled test failing, on a clean tree, all of them in
the same place --

    WARNING: GLFW: Failed to determine Monitor to center Window
    WARNING: SYSTEM: Failed to initialize platform
    failed to create window

`xdpyinfo` answers normally on the same display and no cnp_client is left
running, so the X server is alive and nothing is holding a window. It came on
part way through an afternoon during which the gpu tests had been opening and
closing a window every few seconds for hours -- the whole `-j4` suite had been
green (65 of 65) an hour before, and the change under it was in the mesher and
the dig rule, which cannot reach GLFW.

The watchdog is what turns it into a readable failure instead of a hang, and it
does: every one of them aborts with the stack above rather than sitting there.

**What it means for what was measured.** The runs recorded against OPEN-31's
hardness half were taken before this started, except the seventh sabotage of the
bar, which could not be run at all -- said in that entry rather than counted as
green. Whether this is OPEN-6 grown worse or a second thing is not known: what
is different is the RATE (all of them, repeatably) and that it is window
creation and not a run that starts and never ends.

**It went away by itself, next session.** Nothing was changed and nothing was
restarted by hand: `ctest -L gpu -j2` came back 18 of 18, and the full suite 65
of 65 in 187 s. So the missing sabotage was run and is red, and this face has
no reproducer -- an afternoon of every gpu test failing at window creation,
then a morning of none. It is written down as a sighting, not a diagnosis: if
it returns, the thing to capture BEFORE rerunning anything is what the display
looked like from outside the test (a bare GLFW window from a two-line program,
`glxinfo`, the compositor's own log), because by the time it lifts there is
nothing left to ask.

**Milestone.** Housekeeping; no milestone depends on it. It costs a rerun about
once in twenty, and it trains people to rerun instead of read, which is the
actual damage.

---

## OPEN-7 -- one unexplained `hud` red, and one explained

**Symptom.** Two `ctest` runs have reported `hud (Failed)` where a rerun of the
same gate passed. The second is now understood and was not the gate at all; the
first still is not.

**No output, for the first one.** `ctest` overwrites
`build/Testing/Temporary/LastTest.log` on every invocation, and the run that
reproduced nothing overwrote the run that failed. So there is no record of WHICH
check failed, which is the only thing that would say whether it was the gate, the
renderer, or the window.

**One of the two is now explained, and it was not the gate.** The second red
came from a stale BINARY: a sabotage script restored the sources it had edited
and did not rebuild, so the next `ctest` ran a deliberately broken client. The
failing line was `letting go outside the panel throws that square's stack into
the world`, which is exactly what that session's last sabotage broke. The tree
was clean, `git diff` said so, and the build directory disagreed.

The lesson is a rule: **a sabotage that restores sources must rebuild.** Every
script under the scratchpad now ends with `cmake --build`. This is the second
time it has cost a red -- the first was a `hud` failure inside a suite that
passed standalone, for the same reason.

The FIRST red is still unexplained. Its output was overwritten before it was
read, and that run also followed a batch of sabotages, so it is consistent with
the same cause -- but consistent is not proved, and a red nobody explained is
not a red that was not there.

**Suspected, for the first one only: contention between the gpu tests, unproven.** The suite runs partly
in parallel -- 153 s*proc of gpu tests and 140 s*proc of slow ones inside 223 s
of wall clock -- and `hud` is the shortest gpu test in it at 0.27 s: it opens a
window, draws, reads pixels back and exits. A test that short is the most
exposed to another window taking the focus or to the compositor. OPEN-6 ruled
gpu contention out for the client hang, so it is not ruled out here by analogy;
it is just untested.

**What to do before hunting it.** Make `hud` write its verdict where a rerun
cannot erase it -- a file per run under the build tree, and the frame it read as
a PNG next to it. Every measurement it prints already exists; only the place it
goes is wrong. Until then the next red is another red with nothing in it.

**Half of that is done, and it is not this half.** The eight SHELL gates now keep
the run that failed (`tests/keep_logs.sh`, gated by `keep_logs`). `hud` is not
one of them -- it is `cnp_client --hud-check`, a C++ gate whose output goes to
stdout and from there into the file ctest overwrites on the next run. So this
entry is unchanged: the next `hud` red is still a red with nothing in it.

**Milestone.** None depends on it. One unexplained red is a defect nobody has
found yet, not a defect that is not there.

## CLOSED-8 -- one panel per world, and nobody knows who pressed the button

**Closed.** `ui::PlayerId`, a `player` field on the block and click events, and a
`viewer` argument to `ui_show`. The declaration is shared and the visibility is
not: `PanelSet` holds a row per (panel, viewer), `topmost_for(v)` answers per
screen, `revision_for(v)` is the world's plus theirs, and `drop_player` forgets a
session that leaves. Measured with two real clients on one server -- one has the
panel, the other has nothing. M7_FINDINGS 12. What follows is what it said.

## OPEN-8 -- one panel per world, and nobody knows who pressed the button

**What exists.** A mod declares a panel, the server sends it, a client draws it,
and a click comes back on `core:ui_click`. Protocol 12, M7_FINDINGS 11.

**What is missing, and it is one thing wearing three hats.** `ui_show` has no
argument for WHO should see the panel, so it goes to everybody in the world;
`core:ui_click` carries a panel and a button and not who pressed it, so a mod
cannot answer one player; and the client has no way to close a panel, because a
close it decided by itself would be a second truth about a panel the server still
believes is open.

All three are the same absence: the Lua API has no notion of a player. Events
carry numbers and strings, never a person.

**Why it is not a bug.** A world with one player -- the shape everything has been
measured in -- behaves correctly. It is a limit that shows up the moment two
people stand in the same world, and it shows up as a mod's menu appearing on a
stranger's screen.

**What it costs.** A player handle in the API: a value events can carry, `ui_show`
can take, and `PanelSet` can key on. That turns "the panel that is up" into "the
panel that is up for this session", which is a `panel_rev` per session against a
per-player set rather than a world one -- the session field already exists.

**Milestone.** M7 can be declared without it; the exit test is one player. The
first mod that wants a chest wants it.

## CLOSED-10 -- containers were memory, and a broken chest ate what was in it

**Closed, in three commits.**

**Saved.** A `containers` table and a `player_items` table in the world file,
one `Inventory` codec that stores stacks BY NAME, and a name this build no
longer knows minted as an orphan so the stack keeps its identity. A bag is keyed
by the IDENTITY and not by the name -- DESIGN.md 4.4 -- so a bag is not
something anybody takes by typing yours. M7_FINDINGS 16 and 17.

**Broken chests pour.** `core.container_spill` puts every stack on the ground
and `core.container_clear` takes the box away, in that order, on top of
`core.drop_item`. A stack on the ground is gone in five minutes, counted from
when it landed.

**Distance.** `container_in_reach` -- reach and not a distance of its own, so
the rule is "a chest you can no longer reach is a chest you are no longer in".
One free function, because the server and the single-process client both need
it and this area has already paid three times for a rule written twice.

**And a fourth, found reading the code while fixing those.** Erasing a container
left every session that had it open still pointing at that CELL, so the next
chest built there opened by itself on their screen. `erase_container` is now one
call that does both. M7_FINDINGS 18.

**What is still not saved:** a player's POSITION (the `players` table has been
there since M3 and nothing in the server binary writes it), and stacks on the
ground. There is no periodic save either: `kill -9` loses every chest change
since the server started and everything a player has picked up since they
joined.

## OPEN-9 -- one uncaptured `bad_link` red after the hands sequence was re-timed

**Symptom.** Adding `core:used` changed the scripted edit sequence in
`run_network`: the empty-hand step measures an ABSENCE, so it cannot finish when
its answer arrives. The first version waited `kEditAnswer` (6 s) and `bad_link`
went red immediately and repeatably, with the client leaving after 2.4 s and no
crossing at all -- exactly what the comment on the scripted walk warns about,
and a real regression I caused.

**Fixed, and then one more.** The step went LAST and got a window of its own
(`kUseSilence`, 2 s). After that: 13 runs of `bad_link`, twelve green and one
red at the full 44 s -- so not an early exit, a different symptom from the
regression. Then 4 more captured `-j4` runs of the whole suite, all green.

**No output for it.** I checked the summary line rather than capturing the run,
so there is no record of WHICH check failed. That is the same mistake OPEN-7
records, made again.

**What it is not.** It is not the 6-second regression: that reproduced every
time and is gone. It may be OPEN-6 (the client hanging about once in twenty
runs) wearing a different face, or it may be a third thing.

**The next step is done, and it caught this red again.** Every shell gate now
keeps the run that failed: `tests/keep_logs.sh`, `CNP_LOG_DIR` set by ctest to
`<build>/test-logs`, and a gate on the gate (`keep_logs`) with both halves --
a failing run leaves its files, a passing one leaves nothing.

**And here is the missing output.** Six runs of `bad_link` with the watchdog
taken off, one red, and this is which checks failed:

    FAIL  no crossing happened, so none of the crossing checks ran
    FAIL  the block under the player came from the server and is solid
    FAIL  and streaming into this process
    FAIL  a refused crossing is answered, not waited out
          left after 40.0s, 168715 frames, 32 chunks, 0 corrections, 0.23 MB in
          heartbeats: longest gap 2.08 s, 6 lost; round trip 396 ms

**Measured again, 2026-09-11, and it is much worse than "one in thirteen".**
Three runs on a tree with new work on it came back two red, which looked like a
regression -- so the previous commit was built in a worktree of its own and run
four times: two red there as well, the same check and the same shape ("standing
still was never corrected", 10 corrections applied, the client leaving at the
full 40 s). So it is not what was being written that day, and on this machine
this gate is now failing about half the time rather than once in thirteen. What
changed between the two rates is not known; the machine is the same one whose
gpu tests stopped being able to open a window for an afternoon (OPEN-6).

**So it is not OPEN-6 wearing a different face.** The frame loop never stopped:
168715 frames in 40 seconds is about 4200 a second, which is a healthy client.

**Still there, 2026-09-12, at about the same rate.** Two full `-j2` suite runs
while OPEN-43 was being written came back with one red each and not the same one:
`dual_session` (OPEN-36) in the first, this in the second, both green on a rerun.
Then three runs of `bad_link` alone: two green, one red, the same check --
"standing still was never corrected". That is the rate this entry already
records, and the work of that day touched `inside_solid`, which is what answers a
correction: worth saying that the rate did not move.

Three more full runs later the same day, after the shape started travelling: two
green and one with this red again, 13 corrections applied and the client leaving
at the full 40 s, with "the block under the player came from the server and is
solid" beside it -- which is the world not arriving over a link dropping a fifth
of everything. Six suite runs that day, two reds, both here or in OPEN-36.
The shaper carried about as much as it did on the green runs (294 KB against
317 KB), so the data was on the wire.

**Again, 2026-09-13, while OPEN-44 was being written.** One full suite run, one
red, this one: "standing still was never corrected", 3 corrections applied, the
client leaving at the full 40 s, logs kept in
`build/test-logs/bad_link-20260913-084033-18662`. Then three runs of `bad_link`
alone: three green. Worth saying why that day's work cannot be it rather than
only that the rate did not move: OPEN-44 DOUBLED the horizontal cap -- a
validator that allows more refuses a still body less, not more -- and a body
standing still takes one resolver step per update exactly as before, because
`move_body` splits only a delta longer than half a node.

**Again, 2026-09-13, after OPEN-15, OPEN-20 and OPEN-48.** One full run, one red,
this one again: "standing still was never corrected", 4 corrections applied, the
client leaving at 40 s, logs in `build/test-logs/bad_link-20260913-112718-61407`.
Then three runs of `bad_link` alone: one green, and two red with a face this
entry had not recorded -- the far world's window timing out through the relay
("nothing behind the opening at 127.0.0.2:25612 (timed out); trying again in
2 s"), a third session opened, and the checks about who each server is and
about reconnecting red with it.

That day's work had changed the entity message and every body's id, so the
commit before it was built in a worktree of its own and the two were run
alternately, five each. **Before: 3 red of 5** -- two "standing still was never
corrected" and one "the world arrived in was not streamed a second time".
**After: 1 red of 5** -- "the world arrived in was not streamed a second time"
again. The window timeout did not come back on either. Five a side is not a
rate; it is enough to say the work did not make this gate worse, and that the
faces are the same on both sides of it. Nothing that day touched a correction: a strike's ray, a mod's
visible entity and the width of an entity id are not what refuses a claim.

**Correction: "32 chunks" is not a world that failed to arrive, it is the whole
world.** This entry read that number as a world stuck part-way in. It is not: a
later run's server log says `32 chunks stored (32 loaded)`, so a client holding
32 chunks is holding everything there is. What is left unexplained here is
narrower than what was written above -- no solid ground under the player and the
far world never streaming -- and neither of those is "the world never came".

**Next step.** Reproduce it with the logs kept -- the gate does that now -- and
read the client's own log, which was the thing that had been getting deleted:
how many chunk messages arrived against how many were installed, and whether the
player was standing in a chunk that never came or in one that came and was
dropped.

**Two more captured reds (2026-09-07), while the entity model field was being
built, and a THIRD face.** The numbers first, because the shape of them is the
finding: **three full-suite runs, two red**; the same build **five for five
green** running `bad_link` alone; and green again running tests 46 to 51 in
order. The two reds only ever happened with the whole suite in front of them.

The first red, with the logs kept in `bad_link-20260907-210359-49363`:

    FAIL  standing still was never corrected
          left after 40.0s, 202011 frames, 32 chunks, 7 corrections applied
          heartbeats: longest gap 5.21 s across 2 session(s), 20 lost

Seven corrections against a player who did not move, on a run where the crossing
itself worked. Neither of the two shapes above.

The second, in `bad_link-20260907-211336-52897`, is the one worth reading:

    world 2: connecting, 0 chunks, 0 blocks in its table
    FAIL  every world being looked into is still connected

That is the SECOND session to server B, opened under its other name
(127.0.0.2:25612 against the 127.0.0.1:25612 the client already had), stuck in
`connecting` for the whole run. It is also why the two checks below it failed:
"the sessions did not announce a server identity" and "2 windows" are
consequences of a handshake that never finished, not separate faults -- a client
cannot recognise a server by the key in a welcome it never received.

**What this rules out, and what it does not.** The change that day added two
bytes to every body on the Entity channel (protocol 22). It is not that: the
same build is five for five alone, and a `bad_link` run carries three bodies,
so those two bytes are six bytes on a link that was dropping twenty per cent of
several hundred packets. What the reds have in common is the whole suite --
about eight minutes of GPU tests -- running first. That points at the machine
and not at the change, and it is still not a diagnosis.

**More of it, measured while M8 was being built (2026-09-06), and a second
face.** Closing OPEN-17 turned this red four times in five, which was a
regression and is fixed (CLOSED-17). What is left afterwards is this entry
again: **two reds in seventeen runs**, roughly the rate recorded above, on a
build whose change that day touched no networking at all.

The failure looks like this now, which is NOT the shape recorded further up --
there the crossing never happened:

    FAIL  standing still was never corrected
          left after 40.0s, 156307 frames, 32 chunks, 9 corrections applied
          heartbeats: longest gap 5.05 s, 21 lost; round trip 403 ms
          corrected to 64.00 20.00 64.00: that position is inside a solid block

Every correction names the same position and the same reason, and 64 20 64 is
where a client stands before its world has arrived: the ground under it is
solid, so the server refuses the claim until the chunk the client is standing in
reaches it. So this face of OPEN-9 is a world that arrived slowly, not one that
never arrived -- and twenty-one lost heartbeats on a 400 ms link says why.

**Two more reds, on the day M8's second half was built (2026-09-06).** One
under `ctest -j4` and one from `ctest -R bad_link`, which runs it alone -- then
three green runs in a row on the same binary. Same signature as the face below,
verbatim: `standing still was never corrected`, every correction naming
64.00 20.12 64.00 "inside a solid block", 16 lost heartbeats over a 3.12 s gap.
Nothing that day went near the networking, so this is the rate rather than a
regression -- and being red ALONE says it is not contention with the other
gates.

**A THIRD face, and a worse rate, measured while M9 was being opened
(2026-09-06).** Not a correction and not a missing crossing -- a liveness
timeout:

    disconnected: failed -- the server stopped answering for 10 seconds
                  (round trip 405 ms)
    left after 21.3s, 66960 frames, 32 chunks, 8 corrections applied
    FAIL  stayed connected for the whole run
    FAIL  standing still was never corrected
    FAIL  and both connections are still up -- neither of them reconnected

The client left at 21.3 s of a 40 s run, having decided the server had gone.
66960 frames in 21 seconds is 3200 a second, so the frame loop was healthy: this
is the link or the server's answering, not the client stopping.

**Isolated against the change that was in the tree, and it is not that change.**
The schema-migration work was stashed, the tree rebuilt, and bad_link run four
times: 4 green. Unstashed, rebuilt, four more: 4 green. That does not tell the
two builds apart -- at this rate four runs is not enough to -- but it does say
the change is not what makes it red, and the migration path does not even run
for the fresh world file this gate makes.

**The rate today was about one in five**, over roughly fifteen runs across `-j4`
suites and runs on their own, against the two-in-seventeen recorded above. One
of the reds was `ctest -R bad_link`, which runs it ALONE, so it is not
contention with the other gates.

**A FOURTH face, captured during M9's profiler work (2026-09-06).** Nothing
like the other three: the crossing worked, the link held for the whole 40 s, and
the frame loop was healthy at 147287 frames. What failed is the re-stream check:

    left after 40.0s, 147287 frames, 32 chunks, 0 corrections applied,
      0.04 MB in / 0.04 MB out
    crossed once: arrived at 61.10 20.00 61.07, playing on 127.0.0.1:25613 and
      watching 127.0.0.1:25612; that world had 32 chunks before the crossing and
      32 after, 4 of which it already had
    FAIL  and the world arrived in was not streamed a second time
    FAIL  the client exited 1

`chunks_repeated() == 4`. The chunk channel is reliable, so ENet does not
deliver the same chunk twice however much the relay drops -- which means the
SERVER sent those four a second time, and that is the interest set being
recomputed rather than the link misbehaving. Same suspect as the narrowed step
below, from the other end: `was_sent` is the record that says it already went.

**Next step, narrowed.** Do not correct a claim against a chunk the SESSION has
not been sent yet: the server knows what it has streamed (`PlayerInterest::
was_sent`, which CLOSED-17 now uses for exactly this kind of question). A player
who cannot see the ground cannot be expected to stand on it. Gate it on a client
that claims a position inside a chunk it has not been sent and is not corrected
for it.

**One more capture, 2026-09-08.** Red inside a full `ctest -j4` while CLOSED-26
was being finished, on the same check as ever:

    FAIL  standing still was never corrected

and every other check in that run green -- the world arrived, the crossing
happened, both connections stayed up. Log kept:
`build/test-logs/bad_link-20260908-105736-61329`.

It was asked of the change that had just been made, because that change makes
the client re-mesh the WHOLE world when a world's media arrive -- a stall of
exactly the kind that produces a correction. **Five standalone runs after it:
five green, no log kept.** So the red belongs to the loaded suite, which is the
profile every other capture in this entry has, and not to the re-mesh. Recorded
rather than argued away.

**And a FIFTH face, 2026-09-08, inside a full `ctest -j4` while OPEN-27's mesher
half was being closed.** Not a correction, not a lost link, not a re-stream:

    the opening leads to 127.0.0.1:25613 opening 1; its far end is drawn at
      79.50 22.00 60.50 and that server says 0.00 0.00 0.00
    FAIL  and the far end of the link is where THAT server says the opening it names is
    FAIL  and the world drawn inside the opening is the one that session streams into
    FAIL  one opening, drawn, because the other end is not in this world
    world 1 meshed 0 vertices
    FAIL  and that world was meshed, so the opening is not a hole full of sky
          heartbeats: longest gap 3.06 s across 2 session(s), 11 lost

The far server's session was open, streaming, and had sent its 43-name block
table and all 39 of its files -- but its OPENINGS list was still empty when the
gate looked, so the window had nothing to point at and nothing to mesh. Every
check below the first is a consequence of that one. Log kept:
`build/test-logs/bad_link-20260908-143820-104428`.

It was asked of the change in the tree, because that change is in the mesher and
one of the failing lines says `meshed 0 vertices`. It is not: the vertices are
zero because the window had no world to draw, not because meshing produced
nothing -- world 0 in the same run meshed and drew normally, and `mesher`,
`mesh_pool`, `visibility`, `pack_draw` and `api_shapes` were all green in that
same suite. **Five standalone runs after it: five green, no log kept.** Same
profile as every other capture here: red under the loaded suite, green alone.

**A SIXTH face the same day, in the `-j4` suite that closed OPEN-27's placement
half.** Not a correction, not a re-stream, not an empty openings list -- a
handshake that never finished:

    could not join: timed out -- no answer in 15 seconds, still handshaking
                    (round trip 394 ms)

The FIRST connection joined normally, and the server's own log says so:
`'player' joined as player, protocol 24, handshake 388 ms`. The second server
never logged a join at all. The relay in front of it was dropping 35.1% of what
it carried to the client at the moment (`to the client 37 in, 24 forwarded, 13
dropped`), against the 20% it is configured for. Log kept:
`build/test-logs/bad_link-20260908-145638-112862`.

Asked of the change in the tree, which is a protocol bump: it is not that. Both
ends are the same build, both say 24, and one of the two joins succeeded on it.
**Five standalone runs after it: five green, no log kept.** Same profile again.

**And the fifth face again, in the `-j4` suite that closed OPEN-31's tool
half.** The same four checks, the same cause -- the far server's openings list
still empty when the gate looked -- with 14 heartbeats lost on a 402 ms link.
Log kept: `build/test-logs/bad_link-20260908-184314-189471`. Not re-run
standalone. The change under it is a dig refusal and does not go near portals.

**A SEVENTH face, in the `-j4` suite that closed OPEN-27.** A crossing refusal
that was never answered:

    0 crossing(s) refused, answered in never; 1 waited out the 10.0 s wait instead
    FAIL  a refused crossing is answered, not waited out
    FAIL  and the answer arrives in a round trip, not at the end of the wait
          left after 40.0s, 194387 frames, 0 corrections applied
          heartbeats: longest gap 3.20 s across 2 session(s), 16 lost

Everything else in the run green -- both worlds arrived, both crossings
happened, both links held. Log kept:
`build/test-logs/bad_link-20260908-162250-158177`.

**Not re-run standalone**, which every other capture here was: the five runs
were interrupted deliberately and this is recorded as it stands rather than
written up as if it had been isolated. What can be said is that the change under
it is the block table and the facing plane, and this failure is a lost answer on
a link losing sixteen heartbeats -- the same shape as the rest of this entry.

**Half of it was real, and a mod made it reproducible, 2026-09-11.** `mods/ore`
speckles the stone, which makes the world compress worse and stream slower, and
with it in the shipped set this gate went from occasionally red to red SIX times
out of six -- always the capture written above, the player at y -14264 with air
under them and one correction applied.

The cause was a hold that waited for the wrong thing. The client held the player
still while `world.chunk_count() == 0`, which is not the same as holding them
until there is ground: the first chunk to arrive is not necessarily the one
being stood on, and the player falls through the gap between the two. On
loopback the gap is nothing, which is why every other gate was green about it.
It now holds until the chunk UNDER the player has arrived, latched on the
session so it releases once and stays released -- a player who later walks off
the edge of a finite world still falls, which is OPEN-16 and not this, and a
crossing into a world that has not streamed is held again on arrival.

The hold also stopped arguing with the server while it lasts. It used to put the
player back at a height this loop CHOSE, once a frame, which the server answers
with a correction for as long as the hold lasts -- 22 of them in one run. It
holds them where the frame found them now, so a correction that arrives during
the hold is what the next frame holds.

Measured on this machine, each run the whole gate:

| what was in the tree                    | runs | red |
|-----------------------------------------|------|-----|
| `mods/ore` at one column in eight, old hold | 6 | 6 |
| no `mods/ore`, old hold                 | 4    | 1   |
| `mods/ore` at one in eight, hold fixed  | 15   | 7   |
| no `mods/ore`, hold fixed               | 5    | 1   |
| `mods/ore` at one in thirty-two, hold fixed | 6 | 0 |

The two "hold fixed" rows span both versions of the hold -- the chunk under the
player, and then not arguing with the server while it lasts -- because neither
version separated from the other at these sample sizes.

**The last row is what shipped**, and the middle rows are why the mod's density
is a measurement rather than a taste: ore speckles the stone, speckled stone
compresses worse, and the far end of this gate's link carried 70.1 KB against
45.8 without it. Thinned to one column in thirty-two it carries 55.7 KB.

**What is left of this entry is a different check.** The reds in the fixed rows
are not the capture above: the player is standing on grass, nothing was
corrected, and what fails is "the world arrived in was not streamed a second
time" -- 32 chunks before the crossing and 32 after, 8 of which it already had.
So the far world had not finished streaming through the portal before the player
walked into it, and the chunks it was still owed arrive afterwards and are
counted as a second streaming. The client has a wait for the world of arrival
and did not use it ("0 waited for the world of arrival"), so whether this is a
defect in that wait or a check that assumes a watch has finished is the next
thing to decide -- and it is not what this entry was opened about.

**The sixth face again, 2026-09-12, in the full suite that closed OPEN-23 and
shipped `mods/cave`** -- and on the FIRST connection this time, which the
2026-09-08 capture had joining normally:

    could not join: timed out -- no answer in 15 seconds, still handshaking
                    (round trip 393 ms)

**The server says that join worked.** `a.log`: `'player' joined as player,
protocol 25, 1 features agreed, identity ab7e:56cd:1bd5:23da, handshake 387 ms`.
So the server's half of the handshake finished and the client's did not, and
what was lost is on the way BACK -- which is the direction the relay was
dropping 35.1% in (`to the client 37 in, 24 forwarded, 13 dropped`, 9.0 KB
against the 0.5 KB going the other way). The second server never saw a packet
at all, so every check under the join is a consequence of it. Log kept:
`build/test-logs/bad_link-20260912-103035-56412`.

**The drop counts are identical to the 2026-09-08 capture, and that is not a
finding.** Both relays take a fixed seed (11 and 22, printed in `na.log`), and a
handshake that dies is the same handful of packets meeting the same sequence of
coin flips. What varies between runs is the timing, not the pattern.

**And "red in the suite, green alone" did not hold today.** Ten standalone runs
after it, **two red**: one with three corrections applied and one on the usual
check --

    FAIL  standing still was never corrected
          left after 40.0s, 161118 frames, 32 chunks, 3 corrections applied
          heartbeats: longest gap 4.10 s across 2 session(s), 15 lost;
          round trip 413 ms, so the silence window is 10.3 s

-- with the crossing itself working in both. Logs kept:
`bad_link-20260912-103408-59593` and `bad_link-20260912-103952-61363`. Every
earlier capture in this entry that was re-run alone came back five for five
green, and this one did not, so the "loaded suite" profile is weaker than it
reads above: the rate alone is about one in five, which is the rate this entry
has recorded all along.

**And the second-session face again, later the same day**, in the serial suite
that measured OPEN-37. The crossing worked and the world arrived; what failed
is the session to server B under its OTHER name (127.0.0.2:25612 against the
127.0.0.1:25612 the client already had), which never finished connecting -- so
"every world being looked into is still connected", the identity checks and "2
windows" all fell out of that one handshake, exactly as the 2026-09-07 capture
above describes. Three sessions in the run, 21.6% dropped to the client on the
busy relay. Log kept: `bad_link-20260912-111502-70081`.

**And once more under `-j2`, 2026-09-12**, in the suite run that gated mod
versioning: the chunks never arrived, so the crossing never happened and the
three client-side checks about the ground under the player went with it --
longest heartbeat gap 6.13 s across 2 sessions, 17 lost, 21.2% dropped to the
client on the busy relay. Three runs of `bad_link` alone straight afterwards,
all green. The same one-in-five under load this entry has recorded all along,
and the change in the tree that run was carrying is mod manifests: it moves no
packet and no mod failed to load in the kept log
(`bad_link-20260912-113722-78894`).

**Asked of the change in the tree, and it is not that.** The finer occlusion
walk is client-side drawing and touches no packet, and `mods/cave` makes this
gate's link carry FEWER bytes rather than more -- 50.5/50.8/52.3 KB with the
caves against 53.8/52.9/56.5 without, three runs each, measured for OPEN-35.
That is the opposite sign from `mods/ore`, which is the one content change that
did make this gate worse.

**Milestone.** None depends on it. M7's exit test -- thirty minutes without a
crash -- was blocked on OPEN-6 when this line was written. OPEN-6 is closed on
evidence (CLOSED-6, 2026-09-11), so the long run is not held by it any more, and
`tests/long_game.sh` is where the thirty minutes is answered rather than here:
this entry measures a shaped link, not a long one.

---

## CLOSED-12 -- the server gave up on a join the client was still making

**Closed.** `join_patience()`: a session that has not spoken since its welcome is
not quiet, it is waiting, and it gets a window derived from the client's own
constant rather than from a second one chosen beside it --
`kJoinSilenceHandshakes = kHandshakeRtts * 1.5`, applied to how long THIS
session's handshake took, which is the only look at the link the server has and
was measured on that very connection (377 ms on a 400 ms link). The half is the
margin: the server has to outlast the client, not tie with it, because the
client is the side that can retry and it cannot retry a server that has already
forgotten it.

`spoke_since_welcome` is a field rather than `last_heard_at > since`, which is
true today only because of the order two assignments happen in a hundred lines
apart.

Seven sabotages. The loopback gate slows the HANDSHAKE instead of the link -- a
client that sits a second before its hello is one the server measures at 1006 ms
-- and then asks `silent()` at synthetic times instead of waiting twelve seconds
out, because what is being checked is the rule and not the clock.
M7_FINDINGS 29. What follows is what the entry said.

## OPEN-12 -- the server gives up on a join the client is still making

**Symptom.** `bad_link` went red inside a `ctest -j4` run, and this time the
gate kept its logs (OPEN-9's fix, on its second run). What they say:

    client:  could not join: timed out -- no answer in 15 seconds,
             still handshaking (round trip 382 ms)
    server:  'player' joined as player, protocol 18, 1 features agreed
    server:  dropped 'player': nothing heard for 12 s
    shaper:  to the client  42 in, 25 forwarded, 16 dropped (38.1%)

So the join was not refused and it did not fail on the way in: the server
accepted the hello, logged the join, sent the welcome -- and then dropped the
player for saying nothing, while the client was still waiting for the welcome
that would tell it it could speak.

**Two patiences that do not agree, and the shorter one belongs to the side that
cannot retry.** `kSessionSilence` is 12 s and the client's handshake window is
15 s, and the two constants live in different files and were chosen apart. The
server always gives up first, so the client's window can never do anything: by
the time it expires there has been nobody at the other end for three seconds.

**And the rule under the constant is wrong, not just the number.**
`ServerSessions::silent()` starts counting the moment a session becomes Ready,
and its comment says why that is safe -- "a playing client sends a position
claim twenty times a second". True of a client that is PLAYING. This one had not
been told it could. A client that has not received its welcome has nothing to
say, and the welcome is the biggest message this protocol sends: a block table
and a media offer, fragmented, on a link that lost 38% of what crossed it in
that run.

So this is not a threshold to widen. The measurement is wrong: silence should be
counted from the first word after the welcome, and until that word arrives the
server's patience has to outlast the client's own -- the side that can retry is
the client, and it cannot retry a server that has already forgotten it.

**Not OPEN-6 and not OPEN-9.** OPEN-6 is a frame loop that stops; this client's
loop was fine and its window was up. OPEN-9's red had a successful join and no
crossing. Three different failures on the same gate, and this is the first one
of the three with its evidence attached.

**Next step.** Give a session that has not been heard from since its welcome the
join patience rather than the playing one, with both derived from one place
instead of two, and gate it: a welcome that takes longer than `kSessionSilence`
to arrive must not cost the player their seat.

**Milestone.** M7's exit test is thirty minutes without a crash; a join that
drops on a bad link is not that, but it is the same family.

---

## CLOSED-13 -- the server dropped a session while it was still owed an answer

**Closed.** `net::quiet_for(session, now)`: a session's silence is counted from
the LATER of the last thing heard from it and the last moment this end still
owed it reliable data. `Transport::owed_bytes(peer)` is the measurement -- ENet's
`reliableDataInTransit` plus what is queued behind a window that is full -- and
the server's tick tells the session table, once a tick, which peers it still
owes. `ServerSessions::silent()` asks `quiet_for` instead of doing the
subtraction itself, and the drop log line now prints the two functions the
decision actually used rather than a constant it stopped reading the day
`join_patience` arrived.

**What stops this being a rule that never lets go.** It and ENet's peer timeout
divide the two ways a peer can stop answering, and they divide them exactly:
with nothing outstanding ENet never times out at all, so this rule decides; with
something outstanding this rule stands aside and ENet's window -- twice as wide,
set by `hold_peer` -- is what frees the seat. No gap and no overlap.

**Bytes, and not `enet_peer_has_outgoing_commands`.** That counts a ping waiting
for its acknowledgement, and ENet pings twice a second, so a rule built on it
would never have let go of anything.

**And the gate found a second thing, which had to be fixed for the first to work
at all.** `Time` went out reliably, once a second, to every session. That left
the server permanently owing every peer an answer: `quiet_for` would have
returned about one second for ever and nobody would ever have been dropped
again. It goes out unreliable now, on the same rule as the heartbeat -- a message
whose value expires before its own retransmission could arrive must not be sent
reliably. One lost costs a second of stale sky, 1/1200 of a day, against the
1/16 of a day one light level is worth. The loopback harness was also sending
the heartbeat itself reliably where the real server sends it unreliable; that is
fixed, and the gate that catches the whole class is "owes it nothing new through
three seconds of heartbeats".

**And making `Time` unreliable had a price, which the suite charged at once.**
`net_loopback` went red once in eight runs under `ctest -j4`, on a check about
the hour rather than about silence: that check stops the day (`--day-length` a
million seconds) so it can compare a number, and `kTellEvery` is a fraction of a
DAY -- so the next telling was 833 seconds away and a single lost one stayed lost.
While the message was reliable that could not happen. So the rule was missing,
not the threshold wrong: `tell_interval(day_length)` is the smaller of a fraction
of that world's day and one second, in one place, and `step_time` is back to one
accumulator counted in seconds -- the fraction of a day was the same rule written
a second time. Two properties over twenty thousand day lengths from half a second
to nearly three hours: the sky is never more than one `kTellEvery` stale, and a
lost hour is never lost for more than a second.

**What is measured and what is not.** The arithmetic is checked over synthetic
times in `protocol_test`; the plumbing under it -- a real transport saying what it
owes a real peer, and a real session table told in time for the seat to survive
-- is checked over a real connection in `net_loopback_test`. The drop log line
has no gate: no scenario produces a drop. It cannot drift, though, because both
numbers in it come from the two functions the decision reads.

M7_FINDINGS 31. What follows is what the entry said.

## OPEN-13 -- the server drops a session while it is still owed an answer

**Symptom.** Fourteen runs of `bad_link` in a row with `--watchdog` off, one red,
and the gate kept it (`bad_link-20260905-101657-12772`). The frame loop was
healthy -- 188349 frames in 40 s -- so this is not OPEN-6, and the world arrived,
so it is not OPEN-9 either.

    client:   walked into the opening; asking 127.0.0.1:25613 to take us out of
              its opening 1, waiting up to 10.0 s for an answer (round trip 401 ms)
    client:   the world through the opening did not answer in 10.0 s; staying here
    server B: 'player' joined as observer, protocol 18, handshake 1673 ms
    server B: 'player' came out of this world's opening 1 at 61 20 61
    server B: dropped 'player': nothing heard for 12 s

**The server answered, and then dropped the peer that was waiting for the
answer.** "came out of this world's opening 1 at 61 20 61" is B granting the
crossing; the client never received it. The answer goes out reliably, so ENet was
still retransmitting it -- and the drop is what ended those retransmissions. A
server that gives up on a peer destroys the answer it is still trying to deliver.

**Why the client said nothing.** Deliberately. `src/client/main.cpp` stops
sending viewpoint claims to a session the moment a crossing is asked for on it --
"not to one that is on its way to being a body" -- because a claim then describes
a portal centre, and a server that has granted the crossing would read it as a
position. So between asking and being answered the client is silent BY DESIGN,
and the server counted that silence as absence.

**Not OPEN-12 again.** `join_patience` covers a session that has not spoken since
its welcome. This one had spoken: it joined, it redeemed, it asked to cross. So
it got the player's flat twelve seconds and its 1673 ms handshake bought it
nothing.

**So the rule is wrong, not the number.** A server must not count against a peer
a silence the server itself caused. It has the evidence to know: ENet knows
exactly what reliable data it is still waiting to have acknowledged.

**Milestone.** M7's exit test is thirty minutes without a crash, and this is a
seat lost on a bad link rather than a crash -- but it is the same family as
OPEN-12, one step further along.

---

## OPEN-14 -- the client's wait for a crossing and the link's retransmission were chosen apart

**Symptom, from the same run as OPEN-13.** The client gives a crossing 10.0 s to
be answered. On that link -- 400 ms round trip, 38% of packets dropped -- ENet's
retransmission of the answer backs off by doubling, so a reliable message can
easily be in flight for longer than ten seconds. The client gave up while the
answer was still on its way.

**The shape is OPEN-12's, on a different pair.** Two windows chosen in two
places, neither derived from the other, and the one that expires first belongs
to the side that decides. Unlike OPEN-12 nobody loses a seat here -- the client
stays where it is, which is the right thing to do when an answer does not come --
but the crossing gate reads it as a refusal that was never answered, and on a bad
enough link a crossing can never succeed.

**Next step.** Derive the client's crossing wait from the round trip it has
measured, the way `heartbeat_window()` and `join_patience` already do, rather
than from a constant chosen at a desk. Then gate it: at every link, the wait must
outlast what ENet will spend retransmitting the answer.

**Milestone.** None depends on it.

---

## OPEN-15 -- the client casts a ray and the server checks a distance

**Not a symptom: a limit, found by writing the rule and stated rather than left
to be discovered.** A strike names an entity (`net::Hit`) and the server accepts
it if that entity is within `place_reach` of the eye of the position it accepted.
The client is what actually decides WHICH body -- `aimed_body` casts a ray
against each body's box and takes the nearest.

So the two ends ask different questions. A modified client can name a body it is
not looking at, or one standing behind a wall, as long as it is close enough.
Nothing worse than that: it cannot reach further than an arm, it cannot hit
faster than `ecs::kHitCooldown`, and it cannot say what a blow is worth.

`ray_hits_box` was put in `core/physics` rather than in the client for exactly
this reason -- so that the day the server asks the same question, it asks it with
the same code. Today it does not, and `ServerSessions::strike` says so in a
comment instead of pretending otherwise.

**Why it was not closed now.** The server would have to cast the ray itself,
against the world as well as against the bodies -- "is there a wall between them"
is a voxel raycast per swing, and a swing is up to one per player every 0.4 s.
That is affordable and it is not free, and it needs a gate that says what it
costs before it is written.

**Next step.** Measure a voxel raycast at reach length on this world, then have
`strike` cast the same ray the client casts and refuse a body it does not reach
-- with the gate being a client that names a body around a corner and is refused.

**Milestone.** None depends on it. M7's exit test is thirty minutes without a
crash; this costs nobody a crash.

---

## CLOSED-16 -- a world with no openings had nowhere to put a body that died

**Closed.** The spawn is set outside `if (portals)` now: in front of the first
opening when there is one, and in the middle of the plaza when there is not
(`spawn_in_plaza`). Both go through one sideways search (`free_beside`), because
two copies of a search are two searches that agree until one of them changes.

Gated where the defect lives -- in the BINARY. `long_game.sh` and
`net_client.sh` both start a server with no openings and insist it says where
the dead come back; `worldgen_test` checks the point itself over 24 seeds (free,
solid underneath, inside the world) and then puts a column in the way to make
the sideways step actually happen, since on every one of those seeds the middle
of the plaza was already clear.

**And the seat is gone too.** What was left after the fix above was a
`ServerSessions` constructed without a spawn: it healed a dead body and left it
where it fell, and the binary could no longer produce that state while the type
still could. It cannot now. The spawn is a constructor argument and
`SpawnPoint` has no `known` flag to be false, so `kill_body`'s early return --
"nowhere to come back to: the health is all of it" -- has no case to serve and
is not there. The server derives the point BEFORE it builds the table, which is
where it had to move to; `set_spawn` stays, and is now a world changing its mind
rather than a world being told for the first time.

Gated in `net_loopback`: a table nobody has called `set_spawn` on kills a body,
moves it to the point the HARNESS handed the constructor, and brings a body back
from below the world. The checks read that point from the harness's own constant
and not from `sessions.spawn()` -- measured: the first version asked the table
where its spawn was and then checked the body arrived there, and a constructor
sabotaged into ignoring its argument left it green with a body at 0 0 0.

And in the binary, where the shape of the defect was: `net_client.sh` and
`long_game.sh` now check the POINT the server prints and not just that it
printed a line. The line used to be guarded by `spawn().known`, so its presence
meant something; a table that cannot be built without a spawn prints it always,
and a check that only greps for it passes on a server handed 0 0 0. Five
sabotages -- the constructor ignoring its argument, `kill_body` refusing to move
a body below the world, the binary handing over zeros, `spawn_in_plaza` landing
outside the world, and `set_spawn` doing nothing.

M7_FINDINGS 33.3b. What follows is what the entry said.

## OPEN-16 -- a player who leaves the world falls for ever

**Symptom.** The first 30-minute run of `tests/long_game.sh`. From 1080 s to
1800 s every counter is frozen -- 22 broke, 21 built, 136 swung, 40 turned --
while the frame loop runs at 6000 fps and corrections climb by nine a minute:

    corrected to 15.14 -130580.30 110.86: moved 8.68 blocks vertically in 0.05s
    left after 1800.0s, 10948579 frames, 246 corrections applied
    stood at -3.06 -130822.35 97.95, block below is 'core:air' (NOT SOLID)

The roaming player walked off the edge of a 128-node world at about eight
minutes and spent the other twenty-two falling.

**The rule that should have caught it is there and is right.** `session.cpp`:

    if (v.y < kBelowWorld) kill_body(s, now);   // -16

**And it cannot fire on a server started without --portals.** `kill_body` makes
the body whole and then returns early when there is no spawn -- "nowhere to come
back to: the health is all of it" -- and the server only called `set_spawn`
inside `if (portals)`. So a body below the world is killed and healed on every
claim, never moved, and falls for ever.

**The gate on that rule sets the spawn itself.** `net_loopback`, "falling out of
the bottom of the world puts the body back", calls `set_spawn` in its own setup.
So it never once asked what a server WITHOUT one does. A right rule, gated, and
unreachable in the configuration every graphical test uses.

**Next step.** Give every world a spawn, portals or not, and gate it on the
binary rather than on the rule: a server started with no openings must still say
where the dead come back.

**Milestone.** M7's exit test. The first thirty minute run measured twenty-two
minutes of a falling player and would have passed on exit code alone.

---

## OPEN-11 -- a mod can substitute a block and the player is never told

**Symptom, as reported from play.** "I don't understand why the blocks above a
certain height turn white." A stone tower built up from the plaza is stone up to
y=30 and white from y=31.

**What it is.** Not a defect. `mods/guard` does exactly what its own header
says:

```lua
local CLOUD = core.register_node("guard:cloud", 235, 240, 250)
local CLOUD_Y = 30

core.event.on("core:pre_place", 10, function(ev)
    if ev.y > CLOUD_Y then
        ev.id = CLOUD    -- a substitution, not a refusal
        clouds += 1
    end
end)
```

Above y=30 whatever you place becomes `guard:cloud`, whose colour is
(235, 240, 250). It is the demo of the third thing the event bus is for -- a
handler REWRITING the payload rather than only refusing it -- and it works.

**The gap.** There is no way for the player to find out. A refusal has been
readable on screen since M7_FINDINGS 20 (`ui::Notice`); a substitution says
nothing at all. The mod increments a counter and stays quiet, so a player who
places stone and gets cloud has no explanation short of reading a mod's source.

That is the same class of defect as the one 20 closed: a decision made
elsewhere that the player is subject to and cannot read.

**Two ways to close it.**

1. `core.notice(player, text)` for mods, so `guard` can say "above thirty this
   builds in cloud" and it appears the way a refusal does. This is already
   declared open in M7_FINDINGS 20.7, and it is the one to prefer: the mod knows
   the reason and the engine does not.
2. The engine notices by itself that `core:pre_place` changed `ev.id` and says
   so. Automatic, but noisy, and sometimes wrong -- a mod may substitute on
   purpose without wanting to announce it.

**Cost of the investigation, recorded because it is the argument for doing 1.**
Three rounds of screenshots and a headless probe of the light, the ambient
occlusion and the tile index of every vertex of a 60-block tower -- all of which
came back uniform, because the engine was behaving perfectly. What was missing
was a sentence.

**What it did produce.** The F3 line now names the block under the crosshair and
prints its light and tile (commit ffc923f). That is how the two readings that
identified the boundary -- y=30 stone, y=31 cloud -- were taken.

**Milestone.** M7 UI. Nothing is blocked on it.

---

## CLOSED-17 -- a block a mod changes never reached anybody in the world

**Closed.** `host_set_block` and the player's edit now go through one function:

```cpp
void write_block(ServerState &s, int x, int y, int z, BlockId id)
{
    s.world.set_block(x, y, z, id);
    s.block_writes++;
    relight_around(s.world, s.registry, x, y, z);
}
```

Two ways to change a block were two sets of consequences that agreed until one
of them was edited, and this is the second half of the defect: the write was not
relit either. The world is lit ONCE, before the mods load, so a lamp a mod
placed at load was dark in every chunk ever streamed from that server.

Who is told is where they still differ, on purpose. A player's edit is answered
on the spot because the player is waiting for it. A mod's write goes on a list
drained once a tick -- positions, deduplicated, with the id read from the world
when the message goes out -- because a handler may write a hundred cells and a
hundred sends from inside a handler is a handler that can stall the loop. And
nothing is queued at all before the socket exists: at load every mod is building
its scene, and those cells are in the chunk whoever joins is sent.

Gated in `tests/mod_write.sh`, headless and in the default set, from the only
side that can see it: a client in the world while a mod changes it, and a client
that arrives after a mod has lit something. Three sabotages red, one green on
purpose -- taking the deduplication out costs messages and changes no answer, so
a gate that reddened there would be measuring the wrong thing.

**What it cost, and what that cost taught.** Closing this turned `bad_link` --
M6's exit test -- from green into four reds in five runs, against a historical
rate of about one in six (OPEN-9). Measured rather than argued: with the queue
alone disabled and everything else standing, ten runs of ten green. Four things
were needed to get it back to eight of eight:

  1. `Channel::Block`, so a lost block change does not hold the heartbeat behind
     it. Protocol 19 -> 20. Both kinds of change on ONE channel, because two
     changes to one cell have to keep their order and ENet orders within a
     channel and not across.
  2. Only sending what actually changed. A mod says what it WANTS a cell to be,
     every tick; `mods/spike` writes the same cell twenty times a second and
     alters it twice. Eighty messages a second became ten.
  3. Not telling a session about a cell in a chunk it has not been sent -- the
     chunk is encoded from the world as it is now, so it carries the change.
  4. What a session is owed WAITING while chunks are still being sent to it. The
     world arriving comes first; the victims in every red run were a portal
     window whose far world never finished streaming, and a player corrected
     against a position claimed while their world was on its way.

Each of the first three left it red. Only the four together are green.

**What is left, declared.** A mod that rewrites thousands of cells in one tick
sends thousands of nine-byte messages, and a session owed more than 8192 of them
has the oldest dropped with a line in the log. Above a few hundred cells in one
chunk it would be cheaper to forget that chunk in every session's
`PlayerInterest` and let the streamer send it again, which needs a per-chunk
`forget()` that does not exist. Open as OPEN-19.

M8_FINDINGS 1.5. What follows is what the entry said.

## OPEN-17 -- a block a mod changes never reaches anybody in the world

**Symptom, from the first run of M8's exit test.** The `clock` mod counted
thirteen times in twenty seconds -- thirteen lines in the server log -- while
the probe standing in that world read the four display cells and found every one
of them unchanged since the chunk arrived.

**Cause, and it is one function.** `src/server/main.cpp`:

```cpp
void host_set_block(void *ud, int x, int y, int z, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    s->world.set_block(x, y, z, (BlockId)id);
    s->block_writes++;
}
```

It writes the world and tells nobody. A player's own edit is answered and
broadcast (`ServerSessions::block_change_message`, sent on the Event channel);
a mod's write has no such line anywhere. Whoever is connected goes on seeing
whatever the chunk held when it was streamed to them, until they reconnect.

**Why nothing caught it.** Mods run on a server and on a client that owns its
own world, and NOT on a networked client -- `client/main.cpp` says so and is
right to. On a local client `host_set_block` writes the world that same process
draws, so every mod in `mods/` looks like it works. `mods/beacons` has been
lighting a different beacon every frame since M2 and no connected player has
ever seen one.

**Next step.** Send a block change to everybody who has that chunk, from the
same place the player's edit is sent from -- and gate it on a mod write rather
than on a player edit, which is the case that already has cover.

**Milestone.** M3 shipped it; M8's exit test cannot pass with it open, since
three of the four classic portal-puzzle blocks say what they are doing by changing a block.

---

## OPEN-18 -- a handler that misspells `cancel` is not told

**Symptom, from writing a mod.** A handler wrote `ev.cancelled = true` where the
field is `ev.cancel`. The mod ran, logged that it had claimed the click, and the
block was placed anyway.

**What it is.** The payload handed to a handler is a plain Lua table, so writing
an unknown key is not an error -- it is a new key, which nothing reads. Only
`cancel`, and the declared fields, mean anything on the way back out.

**Why it is worth a line.** The schema exists to make the bus checkable
(DESIGN.md 6.3): a field name that is not in the schema is refused ON THE WAY
IN. On the way OUT nothing is checked at all, so exactly one class of typo -- in
the one field that changes what the engine does next -- is silent.

**Next step.** After a handler returns, compare the keys of the payload table
against the schema plus `cancel`, and log the ones that belong to neither,
naming the mod and the event. A log and not a refusal: a handler is allowed to
keep its own bookkeeping on that table, and taking that away to catch a typo
costs more than the typo.

**Milestone.** None depends on it.

---

## OPEN-29 -- a mod's block write costs a full relight, every time

**Not a symptom: a measurement, taken while closing OPEN-19 and stated rather
than left for whoever writes a mapgen mod.** `host_set_block` calls
`relight_around` for every cell, so a mod pays a lighting pass per write:

    640 cells written at load, 1.36 s total against a 0.12 s baseline
    32768 cells (one whole chunk), 69 s

That is about 1.9 ms a cell, and it is the same number whether the cell is
underground stone becoming stone -- where nothing about the light can have
changed -- or a lamp in the open.

**Why it is not just slow.** It is what makes a mod that builds anything at load
unusable: a structure of ten thousand blocks is twenty seconds of a server that
has not started listening yet. And it is what stops OPEN-19's overflow path from
being gated end to end, because eight thousand writes take longer than the
streaming they would have to happen during.

**Next step.** Two things, either of which helps on its own: relight ONCE for a
run of writes rather than per write (a mod's writes arrive in a batch already --
`mod_changed` is that batch), and skip the pass entirely when neither the old
nor the new block emits or blocks light. Gate it on the same fixture OPEN-19
uses, with the wall clock of a 640-cell rewrite as the number.

See CLOSED-29 above for what it cost, what the gate found, and the three
sabotages that stayed green.

**Milestone.** None. M9 is performance and this is not in it; the mods in this
repo write a handful of cells a tick.

---

## CLOSED-19 -- a mod that rewrites a chunk sent it a cell at a time

**Closed.** What one session is owed is split by CHUNK before anything is sent:
below `kCellsWorthAChunk` (150) the cells go out as they did, and above it the
chunk is dropped from that session's `PlayerInterest` and the streamer sends it
again -- encoded from the world as it is now, so it carries every one of those
cells and nothing has to be listed. `PlayerInterest::forget(ChunkPos)` is the
new call; `forget_distant()` could not be asked for this, because it decides by
distance and a chunk rewritten under a player is as near as a chunk gets.

**And the half of it that was a hole rather than a cost.** Past `kMaxOwedCells`
the queue was emptied and nothing else, so that client stayed wrong about those
cells until something re-sent the chunk, and nothing did. It now forgets the
chunks those cells were in before emptying, so they arrive. That path is only
reachable for a session whose world is still arriving -- everything else is
drained every tick and a rewritten chunk never reaches the cap.

**Measured, over a socket.** A mod rewrites 640 cells of one chunk in one tick
under a player standing in it: 0 cells listed, 1 chunk re-sent, and the client
ends up holding all 640. Before the change the same run sent 640 messages. In
`chunk_codec`, four hundred cells in one chunk cost 6834 bytes as cells and 233
as the chunk.

Gated in `chunk_codec` (forgetting one chunk, and what the split does to a
chunk rewritten wholesale beside two lonely cells in another) and in
`chunk_rewrite` -- a new two-process gate, because the decision is in the
server's drain loop and the proof is in a client's world. Five sabotages:
`forget` doing nothing, nothing ever being worth a chunk, the chunk AND its
cells both going out, every single cell becoming a chunk, and the server
ignoring the list it was handed. The first and the last both turned the
client-side check red -- "the client holds 0 of 640" -- which is what says that
check is not vacuous.

**What is NOT gated, declared.** The overflow path itself. Reaching it needs one
session to be owed more than 8192 cells while its world is still arriving, and a
mod's block write costs about 1.9 ms (see OPEN-29), so 8192 of them is sixteen
seconds of server time -- longer than the streaming it would have to overlap
with. The mechanism it uses is the one `chunk_rewrite` exercises; the
arithmetic around it is not.

M7_FINDINGS has no entry for this. What follows is what the entry said.

## OPEN-19 -- a mod that rewrites a chunk sends it a cell at a time

**Not a symptom: a cost, measured while closing CLOSED-17 and stated rather
than left to be found by somebody's mapgen mod.** Every cell a mod changes while
players are connected goes out as its own nine-byte `BlockChange`, one per
session. A mod that rewrites a 32x32x32 chunk in one tick sends 32768 of them to
each of up to eight sessions.

**The number that decides it.** A chunk on the wire is about 1.4 KB typical
(README: 3.00 MB of world becomes 0.01 MB), so somewhere around a hundred and
fifty changed cells in one chunk it stops being cheaper to send them
individually. Above that the right message is the chunk.

**And it is now also the overflow path.** A session is owed at most 8192 changed
cells; past that the oldest are dropped and the log says so once. That client is
then wrong about those cells until something else re-sends the chunk, which is
exactly the hole this entry is about.

**Next step.** Group the drained cells by chunk; below the threshold send them
as now, above it drop that chunk from every ready session's `PlayerInterest` so
the streamer sends it again. That needs `PlayerInterest::forget(ChunkPos)`,
which does not exist -- there is `forget_distant()`, and it decides by distance.
Gate it on a mod that rewrites a chunk and a client that ends up with the same
world either way, and on the byte count being the smaller of the two.

**Milestone.** None depends on it. No mod in this repo writes at that rate.

---

## CLOSED-23 -- the walk is chunk-shaped, and this world is two chunks tall

**Closed, 2026-09-12: the walk carries the piece of a chunk's air it is in.**
`compute_visibility` kept the fifteen bits and threw the regions away, and the
fifteen bits are the UNION of what every region can do. In the shape this world
actually makes -- 64 blocks tall against a chunk's 32, so two chunk layers, and
every chunk of the lower one holding the open sky over the terrain -- that union
says every face of every chunk sees every other, however much rock is in it.

**What it buys, measured.** A camera in a corridor at y = 6 draws **16 chunks
against the 201 the frustum reaches**, with the same picture to the pixel. At
the mouth of the older bore, 10 against 125. From the SURFACE it still drops
nothing, and that is not a failure but the other half of this entry's own
sentence: an occluder has to be about a chunk before a walk at chunk scale can
see it, and the mapgen's hills are ten blocks.

**Three things are kept per region, and each is one decision.**

  - **The faces it reaches.** This is what the walk leaves by. A cave that
    reaches -x and +x leaves by those and not by the sky's +y, which is the
    whole of this entry in one line.
  - **A cell mask per face**, a bit per cell of the 32x32 plane. Two chunks
    agree about the plane between them cell by cell, so a crossing is an
    OVERLAP and not a face: a cave leaving by +x enters the cave on the other
    side and not the sky above it, however open that face is as a whole. Without
    this the walk is exactly the old one -- measured, 201 chunks of 201.
  - **Where the region is, to 4x4x4 cells.** The walk has to know which piece
    the camera is standing in, and the exact answer is a region id per cell:
    32 KB a chunk, kept for ever, to answer one question a frame. Two regions
    sharing a block of 64 cells are both taken and the walk leaves by the faces
    of both, which over-draws. Without this the camera's own chunk leaves by all
    six faces and the walk spreads from the first step: measured, 200 of 201.

**What that weighs**, printed by the gate rather than asserted here: a region is
**72 bytes** and each face it reaches another **136**. A chunk of ordinary
ground has a handful of both, which is why the exact per-cell answer above was
not worth its 32 KB.

**A region that reaches no face is not numbered.** A sealed room is not a way
through a chunk and nothing crossing the chunk can enter one, so it costs
nothing and is not counted against the cap -- which makes the cap a cap on the
ways THROUGH a chunk rather than on the holes in it.

**Sixty-four ways through one chunk, one per bit of the mask the walk carries.**
A chunk with more keeps its fifteen bits and says nothing finer, so the walk
crosses that one chunk the old way and goes on carrying regions everywhere else.
Over-draw, which is the direction this file is allowed to be wrong in.

**An eye in rock takes the old answer.** No region holds it, and without the
blocks this cannot tell rock from a room sealed away from every face -- so it
leaves by all six faces as it always did. A camera buried in stone is a
sixtieth of a second from being in the open and its near plane may already be.

**What it costs.** The walk was 0.023 ms a frame and is now **0.095 ms quiet,
0.112 ms in the storm, 0.142 ms at p99** -- four times, against a drawing cost
it still rounds to nothing beside, and the profiler row that says so was already
there.

Gated in `visibility` -- a chunk of air is one region reaching six faces, a
chunk of stone none, a sealed room none, a bore under the open sky is TWO
regions that both reach -x and are told apart by which cells of that face they
hold, and sixty-five bores keep no regions and the same fifteen bits -- and in
`frame_budget`, which builds a corridor three blocks square through the ground
and stands in it. Five sabotages, all red: the camera's region ignored, the
crossing asking the face instead of the cells, the cell mask indexed with its
two axes swapped, a sealed room numbered, and the cap not applied.

**And a note for whoever runs the next ones.** A sabotage put back with `mv`
keeps the original file's timestamp, which is OLDER than the object built from
the sabotaged copy -- so ninja rebuilds nothing and the next run measures the
sabotage still in place. Two gates went red here for exactly that reason and
nothing was wrong with them. Touch the file after restoring it.

**A sixth sabotage stayed green, and it is worth the line.** Letting the first
arrival at a chunk win -- never expanding it again when a second way in finds
regions the first did not -- changed no picture and no count. The reason is
geometry: a second arrival by a different face is a BENT path, and a bent path
is one nothing can see down, so the pixels do not move. It can still hide a
chunk, when a bent path reaches one before the straight path does and poisons
the entry, and the rule is kept for that. No scene here builds it.

**And it found three pixels.** See OPEN-37: single pixels of sky through solid
rock, which are in the picture with the walk off as well -- drawing the whole
frustum put a chunk behind them and drawing only what can be seen does not.
They are a coincidence between one pixel grid and a projected edge, not a gap
in the rock and not the chunk seams this file first suspected.

**Milestone.** None, as it never had one. It is what OPEN-35 wanted before caves.

---

## OPEN-23 -- the walk is chunk-shaped, and this world is two chunks tall

**Not a symptom: a limit of what M9 built, measured rather than left for
somebody to discover.** Occlusion culling asks its question a chunk at a time,
and a chunk is 32 blocks. kWorldHeight is 64. So every world this engine makes
is TWO chunk layers, and every chunk of the lower one reaches the open sky above
the terrain.

**What that costs.** From a camera standing on the surface the walk drops
nothing at all -- 131 chunks drawn with it and 131 without, measured. The
mapgen's hills are about ten blocks and an occluder has to be about thirty-two
before this can see it. What the walk buys is real and it is elsewhere: sealed
in rock it draws 4 chunks instead of 129, and in front of a chunk-sized wall 2
instead of 154.

**Next step, when something asks for it.** Either a taller world, which is a
change to a constant the server and the client agree on and therefore to the
protocol's world description, or a finer walk -- regions inside a chunk rather
than the chunk as a whole, which is the same flood fill keeping its region ids
instead of throwing them away. The second is cheaper than it sounds and is what
would make caves worth having.

**Milestone.** None. M9's exit test passes without it.

---

## OPEN-20 -- a mod's entities are invisible to everybody but the mod

**Not a symptom: a limit of what M8 built, stated rather than left to be found
by whoever writes the second mod that owns entities.**

`core.ecs.spawn` makes a flecs entity with a `Position` and nothing else. What
a client is sent is "the entities near them with a `Body`" (`session.cpp`, the
each over `Position, Look, Body`), so a mod's thing is in the world, is stepped
by the mod's systems, collides with nothing and is drawn by nobody.

**How the gate's mod works around it, which is the honest workaround for now.**
`tests/mods_sentry` sends its pellet count to the players on its own channel and
lets a client mod draw what it likes with `core.render.box`. That is a real
answer for a mod that wants to draw its own things its own way, and it is not an
answer for a mod that wants the engine's interpolation, its interest management
and its portal clipping.

**Next step.** `core.ecs.visible(id, on)`, adding and removing `Body` plus a
`Look`. The interest path then carries them with no change at all, which is the
claim `core/ecs/components.h` makes about `Body` being a query and not a list of
exceptions -- so the gate is a mod that spawns ten entities, a client that is
sent ten bodies it has no mod for, and the count of `Entities` on the wire.

**Milestone.** None depends on it. M8's exit tests pass without it.

---

## OPEN-21 -- a mod's entities need a server that is listening

**Also a limit, and a trap for a mod author rather than a defect in a running
game.** `LuaEcs` is handed the flecs world that `net::ServerSessions` owns,
because that is the one the mobs and the drops are in and a second entity store
would be two id spaces for one idea. The session table is built when the socket
is, and the offline run -- `cnp_server --ticks N` with no `--listen` -- never
builds one.

**What that costs.** A mod that spawns in an offline run gets nothing back from
`core.ecs.spawn` -- correctly, since there is nowhere to put it -- and its
systems never run. The offline run is a mapgen hash run and not a game, so
nothing in this repo notices; a mod author testing with it would.

**Next step.** The world wants to belong to whoever owns the game rather than to
the session table: `ServerState` holding it and `ServerSessions` taking a
reference. That is a change to `net::ServerSessions`'s constructor and to every
place that calls `sessions.ecs()`, which is not a thing to do on the way past.

**Milestone.** None. Worth doing when something else needs the world before the
socket.

---

## OPEN-22 -- a mod that asks for a network channel too late is refused, and nothing checks it

**A refusal with no gate.** `host_net_open` returns 0 and logs "too late -- this
world is already listening" for a mod that calls `core.net.open()` after the
socket is up, because the channel numbers go out in the welcome and a client
that has already joined was never told about a new one.

That path is not exercised: it needs a mod that calls `net.open` from inside a
tick, and `tests/mods_sentry` calls it where a mod should. So the refusal is
written, reasoned about, and unmeasured -- which is exactly the shape of
find_nodes' unknown-name refusal, which a sabotage found GREEN this milestone
(docs/M8_FINDINGS.md 6.2).

**Next step.** A second fixture mod that opens a channel from `on_step`, and a
check on the sentence. Small, and the reason it is not done here is that it
wants a mod whose only purpose is to be wrong.

**Milestone.** None.

---

## OPEN-28 -- the wire says what a body looks like and nothing says it yet

**The field is there and nothing fills it.** Protocol 22 gives every body an
`EntityState::model` -- an index into a model table the welcome carries -- and
`ecs::Wears` is the component the streaming query reads it off. A drop may not
wear one, because a stack on the ground is drawn as the stack it is. All of it
is gated: `protocol` for the round trip and the refusal, `net_loopback` for a
creature that reaches a client wearing what it was given, for the table arriving
beside the block table, and for the stream still flowing afterwards.

**The client half is done.** `ModelStore` (client/render/model_store.h) turns a
world's model table into files inside ONE directory -- `--models <dir>`, one
file per name, and a name that could climb out of that directory is refused and
counted -- and `draw_bodies` draws the character instead of the box for a body
whose model this client has. A model it has not got is still the box, which is
the one thing that must never become a hole where a player was. Gated in
`model_draw`, both loops of `draw_bodies`, six sabotages.

**And the server end is done.** `ModelRegistry` (core/registry/model_registry.h)
holds the names and, beside each, the file it is in ON THAT MACHINE.
`core.register_model(name, file)` declares one from a mod -- the file resolved
inside the mod's own directory, and a name that could leave it refused by
`model_file_is_plain`, which is one function and not one copy per host --
`core.player_model` and `core.mob_model` say who wears it, avatars are dressed
when they are made, and a creature is dressed at its spawn rather than a tick
later. Gated in `mod_reload` (the Lua path and the id kept across a reload),
`net_loopback` (a player's body and a creature arriving dressed) and `sentry`
(the server publishing what its mod declared, and the table reaching a client).
Six sabotages.

**And a model does not travel.** The table carries NAMES. What a model is made
of -- a .gltf, its buffer, its textures -- is a file on the machine that has it,
and the media path has never been asked to carry one. A .gltf names its buffer
in a `uri` relative to itself, so "a model travels" is not "a file travels":
found here, by a gate that copied the .gltf into a directory without its .bin
and loaded nothing.

**Closed.** The files travel. `add_models_to_media` puts a model's .gltf and
everything it names beside itself into the same set a world's textures are in,
under flat names both sides derive from the model's NAME
(`model.<name>.model.gltf`, `model.<name>.<what the .gltf calls it>`); the
client rebuilds each one as a directory named by the .gltf's content hash and
loads it from there, because a .gltf names its buffer relative to itself and a
loader handed the JSON alone loads nothing. `load_gltf` did not have to change:
what changed is that the client writes the folder the loader expects.

A model is served WHOLE or not at all, on both sides. Half a model is a
character with a hole in it, and nobody looking at one can tell which file went
missing.

Measured end to end: a client with no models folder joins a world whose mod
declares one and reports `models: 1 named by this world, 1 from it, 0 from this
machine`.

Gated in `gltf` (what a model is made of, and a uri that leaves its folder),
`media` (a world serving one whole, and skipping the two that are not),
`sentry` (both files over a socket to a client that never had them),
`net_client` (the graphical client rebuilding it from what arrived) and
`model_draw` (it draws, and half a model leaves the box). Five sabotages.

**And the defect that found the gate.** The first naming scheme was
`model/<name>/<file>`, and `MediaOffer::decode` refuses any media name with a
separator in it -- so one model in a world cost the client the WHOLE offer,
every texture with it, silently. Nothing caught it: the media gate built its set
in the same process and never put it on a wire. It does now, and that check
would have failed on the first run.

**Milestone.** None. It is the first half of what M11 needs for entity models.

---

## OPEN-24 -- nothing that ships is a block with a front

**The mechanism is there and nothing uses it.** A block registered `rotatable`
is four ids, is turned to face whoever places it, wears its base's pictures
permuted, and is found by `find_nodes` under the base's name. It is gated in the
single-process path (pack_draw, four sides, 35% of the frame magenta facing the
placer and 0% from behind) and over a socket (sentry, both turrets come back as
`sentry:turret^3`). The only block in this game with a front, `chest:box`, is
NOT rotatable.

**Why.** `mods/chest` compares an event's `ev.id` against the id
`core.register_node` handed it. Turn the chest on and three placements out of
four stop opening -- the id in the world is one of the three extra ones. The
API for the fix exists now (`core.node_base`), and the mod has not been changed
to use it.

**Next step.** `core.node_base(ev.id) == CHEST` in mods/chest, then
`rotatable = true`, then a pack entry `chest:box:front`. Kenney's manifest
already paints one. Adding three ids after `chest:box` shifts every block id a
mod registers after it, which is refused by world_db for an existing save -- so
it lands with a schema note, not quietly.

**Closed.** `mods/chest` registers `chest:box` rotatable, and both its handlers
ask `core.node_base(ev.id)` instead of comparing the id they were handed -- so a
chest opens, and pours out what was in it when broken, whichever way it faces.

The gate is in `hud`, beside the two checks that were already there for a chest
nobody had turned: a right click on `chest:box^2` is claimed by the mod, and
breaking `chest:box^1` puts what was inside on the ground and takes the box
away. Every one of those lines carries "and this really is a turn" on the SAME
check rather than in a check of its own, because `turned()` on a block with no
front hands back the id it was given: the first version printed four greens
against the plain chest and proved nothing. Six reds before the change. Four
sabotages after, all red -- each handler put back to `ev.id` (two reds and
three), the `rotatable` flag taken off (six), and `host_node_base` handing the
id straight back (three).

The price is the one written above, and it was measured rather than assumed: a
world file written before the chest had a front is refused by name, "world file
was made by a build with a different block table (file 13513395974957729754,
this server 5695901526538081087) -- the stored chunks would be read as the wrong
blocks". That is `content_hash` in `world_db.cpp`, which is the same rule the
handshake applies across a network, and it is why the three extra ids cannot
turn somebody's world into the wrong blocks quietly.

**Milestone.** None.

---

## CLOSED-27 -- a block can be turned about the vertical and nowhere else

**Closed.** A block with a front is ONE block. The turn is a byte on the CELL --
`Chunk`'s facing plane, in the same compressed frame as the blocks and the
light, in the same world file, and read by the mesher when it picks which
picture goes on which face. Registering `rotatable` adds one row to the block
table where it used to add four, measured by `pack_draw`: **41 to 42**, against
44 to 48 before.

Three commits, each gated first and sabotage-checked: the cell remembers
(`5ccc00f`, protocol 22 -> 23), the mesher reads it (`8830ad4`), the placement
writes it (`2468c1d`, protocol 23 -> 24), and the ids retire (this one). Twenty
sabotages across the four, and two of them came back GREEN -- both times the
gate's fault, and both worth more than the sabotage.

**The price, measured rather than assumed.** The block table loses three rows
and its content hash changes, so a world file written by a build before this is
refused BY NAME:

    server: world file was made by a build with a different block table
            (file 5695901526538081087, this server 13513395974957729754)
            -- the stored chunks would be read as the wrong blocks

Those are the same two numbers CLOSED-24 quoted, the other way round: adding the
chest's three turns is what moved the hash then, and taking them away moves it
back. It is the same rule the handshake applies across a network, and it is why
three fewer ids cannot turn somebody's world into the wrong blocks quietly.

**And a defect this found, which is the part worth reading.** The server did not
load a world file through `install_chunk`. It loaded into a second `World` and
copied each chunk across cell by cell -- `set` and `set_light` -- which was
`install_chunk`'s body written a second time. It dropped the FACINGS from the
day cells got them: every turn in every world file read back as 0, with the
blocks arriving perfectly, so a chest saved facing the player came back facing
+Z and nothing said anything. The fix is not a third line in that loop: the loop
is gone, and `load_all_chunks` installs into the world the server runs.

It was found because a sabotage of a DIFFERENT rule came back green. Chasing
that green meant giving a mod `core.set_facing`, which meant a fixture could
turn a cell, which meant `sentry` could ask whether a turn survives a world
file -- and it did not. Neither the sabotage nor the defect would have been
found by the other route.

**What did not survive, said out loud.** Searching for an ORIENTATION.
`find_nodes` on `mymod:forno^2` used to find only that turn; there is no such
name now. Nothing asked for it, and when something does it is a facing argument
on `find_nodes` and not four names.

**Still four turns and not twenty-four.** The byte holds twenty-four; the
placement writes four. That is now a limit of what `turn_towards` decides rather
than of the block table, which is the whole reason this was worth doing.

**And the twenty-four landed, 2026-09-12.** A block declares what the byte
MEANS, in four kinds: nothing, `upright` (0..3, the four turns that existed),
`facedir` (0..23, any orientation of a cube) and `wallmounted` (0..5, the FACE
the block's front points at). `NodeTurn` on the block, and one function --
`node_cell_orientation` -- turns a cell's byte into one of the twenty-four, so
the difference between the kinds is in one place and not on the mesher's hot
path.

**0..3 are exactly the four turns that existed**, and that is the reason the
numbering is what it is rather than a coincidence to be grateful for: an
orientation is `axis * 4 + turn` where the axis says where the block's own +Y
ends up, axis 0 is no tilt, and so every facing byte in every world file and
every chunk already on the wire keeps the meaning it had. The axis order is
The original engine's facedir order, so M11's compatibility layer can pass a `param2`
through instead of carrying a table of its own.

**Derived, not written down.** The 24 x 6 permutation is built at compile time
out of integer rotation matrices -- every entry is -1, 0 or 1, because these are
quarter turns of a cube and floating point would buy rounding where there is
nothing to round. The inverse the mesher asks (`face_source`) is built by
turning the forward table round rather than by inverting matrices, so the two
cannot disagree. What the gate checks is therefore not one entry but the
PROPERTIES a set of cube rotations has: twenty-four distinct permutations,
opposite faces staying opposite, the inverse being the inverse, and every one
being a rotation and not a reflection -- which is the only check that catches a
mirror, since a mirror permutes the six faces and keeps opposites too.

**The boxes turn by the same matrix.** A block made of `core.node_box` parts
turned about the vertical by a sine and a cosine indexed by two turns; it is the
orientation's own matrix now, about the middle of the cell in all three axes.
For the four upright turns that is the same arithmetic to the bit -- y comes
back to itself -- and the gate checks that the matrix agrees with the face
permutation for all twenty-four, because a block's shape and its pictures
disagreeing about which way it is lying is the defect nobody would think to look
for.

**A placement still writes four, and a wallmounted one writes a face.** The
clicked face IS on the wire -- the cell a placement targets is derived from it
-- so a wallmounted block goes down pointing out of the surface it was attached
to, with no new field. A facedir block placed by a player gets one of the four
upright turns, because a placement has no way to say "on its side" that a player
could aim; the other twenty are for a mod and for whatever tool later turns a
cell somebody has already placed. Gated in `net_loopback` through `plan_edit`:
six faces clicked, six bytes exactly the face, and a block that does not turn
left at 0 whichever face was clicked.

**A byte the block does not allow is refused, and drawing one is not a crash.**
`core.set_facing` refuses it in both hosts and says so with the mod's name on
it; the wire refuses anything above 23 at parse time, since the block table is
not on that side of the parse; and the mesher draws a cell whose byte its block
does not allow UNTURNED rather than reading past a table. Protocol 25 -> 26: the
field is the same byte and the same width, and more of its values are legal, so
a 25 peer handed a 7 refuses the message -- which is what a version number
exists to turn into a refused handshake instead of a refused cell.

**And a defect the gate found in the change itself.** `tile_for` began with
`if (facing == 0) return d.tex[face]`, which had been true for as long as a byte
of 0 meant "unturned". For a wallmounted cell 0 means "the front points at +X",
which is a turn -- so the fast path drew six of them wrong. The wallmounted half
of the mesher gate is what said so, and the shortcut is on the resolved
ORIENTATION now.

**Two gates' fixtures had to declare a turn, which is the rule working.** The
mesher's six-sided block and the client's `parts:boxy` both turned cells without
the block ever saying it turned -- `tile_for` permuted whatever byte it was
handed. Both went red until they declared a kind, and that is the behaviour
change worth naming: a block that declares nothing does not turn however its
cells are written.

**And a gate that could not see turning at all.** `worldgen`'s stub host had no
`set_facing`, so every cell a mod turned came out unturned there. It has one now
-- with the same refusal the two real hosts apply -- and that is what let the
shipped mod be checked against the WORLD.

**Something that ships turns, 2026-09-12.** `mods/timber`: a log declared
`facedir` and laid on three axes by `core.set_facing`, and a panel of rungs
declared `wallmounted` and mounted on all six faces. Gated in `worldgen` against
the world rather than against the mod's account of itself -- the set of
orientations found on the cells, not a count, because "five logs are turned" is
true of five logs turned the same way, which is what a byte masked into range
would produce. It also checks that no cell in the world carries a byte its block
does not allow.

**The placeholder art had to learn a face for this to be visible.** A generated
placeholder is the same on all six faces, so a log lying down looked exactly
like a log standing up: measurable in a gate and invisible in the game.
`bake_face_tile` draws the rings of a cut log on a facedir block's top and a
bullseye on a wallmounted block's front, and the baker writes those files for
those two kinds only -- an upright block's top and bottom do not move however it
is turned, so marking a chest would cost two tiles to say nothing.

Nine sabotages, all red on distinct checks: the tilt for one axis built the
wrong way round, a mirror in place of a rotation, the turn composed after the
tilt instead of before, a wallmounted byte read as an orientation, every byte
allowed for every block, a wallmounted cell not resolved through its face, a
placement ignoring the clicked face, the mesher keeping its `facing == 0`
shortcut, and the boxes turning about the vertical only.

**Two of them were GREEN first, and both bought a check.** Composing the turn
and the tilt the other way round survived every property the gate had: it is a
different LABELLING of the same twenty-four rotations, so they stay distinct,
proper and opposite-preserving, and the mesher's expectations are derived from
the same table and agree with themselves. What catches it is the one claim the
properties cannot make -- that the axis half of the number says where the
block's TOP goes, for all four turns -- which is the definition of the encoding
and now a check. The other was the boxes: nothing drew a facedir block made of
boxes, so replacing the orientation with `turn & 3` changed no picture in any
gate. `parts_draw` has one now, shot standing up and laid on the x axis.

**What is still not there.** Nothing turns a cell that is already placed -- a
screwdriver is an item and a mod can write the byte, and neither exists yet.
The four spins about a wallmounted block's own front are not distinguishable:
the byte is a face, so a sign mounted on a wall cannot be upside down. And the
placement's four turns come from the placer's POSITION, which is still the only
thing the server has.

**Milestone.** M11 lists `param2` facedir and wallmounted as traits the engine
owes the compatibility layer. This is that, and it is what a stair, a log and a
piston need before a mod of the original engine can ask for one.

---

## OPEN-27 -- a block can be turned about the vertical and nowhere else

**Four turns, not twenty-four.** A block's front can point along +Z, +X, -Z or
-X. It cannot point up or down, and the block cannot be laid on its side. That
covers a furnace and a chest and not a piston, a stair or a log lying down.

**Where.** `turned_face` in `src/core/media/face_tiles.cpp` walks a ring of the
four upright faces, and `NodeDef::turn` holds four ids. Twenty-four orientations
would be twenty-four ids per block, which is where the extra-ids approach stops
being cheap -- twenty-four rows for one block, and `atlas_tiles_for` sized from
a table that is mostly rotations.

**Next step.** This is the point where a byte per cell wins: `Chunk` grows a
facing plane, the codec grows a third uniform/palette section, `world_db` bumps
its schema and the protocol goes 22 -> 23, and the rotation stops being ids at
all. Worth doing with the next protocol bump rather than one of its own: 22
already went out for what a body looks like (see OPEN-28).

**Half done, 2026-09-08: the cell remembers.** `Chunk` has the facing plane,
`World::get_facing`/`set_facing` reach it, it rides in the same compressed frame
as the blocks and the light, and it is on disk. Protocol 22 -> 23.

The plane is FLAGGED on the encoding byte rather than being a fourth encoding,
so a message without facings is byte for byte the message this codec produced
before them: every chunk already in a world file decodes with no facings and is
right, which is why `world_db` did NOT have to bump its schema after all. The
wire still moves, because an old decoder handed a flagged byte refuses it as an
unknown encoding -- and refusing is what the version range is for.

A chunk nothing has turned costs nothing: still a header and no payload
(20 bytes), still no plane allocated. One that is turned is 58 bytes in the
gate's own case and pays exactly one byte a cell in memory.

And the trap the LIGHT fell into in M2, on the plane added after it: a chunk of
one block and one light value whose cells are turned differently is not one
value, and the uniform path would have lost all 32768 facings. Checked.

Gated in `chunk_codec` (a chunk of mixed facings round trips; one block, one
light and a turned cell does not go as a header; an untouched chunk is still 20
bytes and allocates nothing) and in `storage` (turned cells come back turned
after a save and a load, and untouched ones come back untouched). Five
sabotages: the plane not written, the uniform path forgetting it, the palette
decoder dropping it, the flag never set, and `memory_bytes` charging for a plane
that is not there -- that last one went GREEN first, because the check compared
two chunks that both had no plane. It compares against `sizeof(Chunk)` and
against a chunk that IS turned now.

**Half of the rest, 2026-09-08: the mesher reads it.** `ChunkSnapshot` carries
the plane, and the picture a face wears is resolved at MESH time from the cell's
own byte -- `tile_for` in `mesher.cpp`, the inverse of `turned_face`, which is
three quarter turns of it. A block with a front no longer has to be four blocks
for the mesher's sake.

The permutation moving to mesh time has a consequence greedy meshing had to be
told about: two neighbouring cells of the same block turned differently no
longer wear the same pictures, and merging them into one quad loses one of the
two turns. `Face` therefore carries the resolved TILE and merges on that -- not
on the facing byte, which is a strictly stronger question and the wrong one: the
top and the bottom of a block do not move when it turns about the vertical, so
splitting on the byte would give up a quad on every roof of turned blocks for
nothing. Measured on the gate's own pair: 2 quads on +Z, 1 on +Y.

`turned_face` became inline for this. It is asked once per exposed face of every
turned cell now, and the alternative to inlining it was a second copy of the
turn ring beside the hot path -- the block table's own hazard applied to face
numbering.

A chunk nothing turned still costs nothing: the snapshot's plane is EMPTY, not
39304 zeroes, which is the rule `Chunk` already keeps one layer down, and
`facing_at()` answers 0 for it so the mesher has one path and not two. A
snapshot is 118 KB and is refilled rather than reallocated, so the plane is
cleared on every fill -- otherwise the last chunk's turns show up in this one,
on cells nobody turned.

Gated in the new `mesher` test, 31 checks: all FOUR turns of a six-sided block,
because a permutation applied the wrong way round is right for turn 0 and turn 2
and wrong for the other two; the two-turn merge; a plane of zeroes meshing byte
for byte the same as no plane; the snapshot carrying what the World holds; and
the plane appearing and disappearing as a kept snapshot is refilled. Six
sabotages, all red on distinct checks: turned the wrong way (8), the merge
dropping the tile (1), the snapshot never filling the plane (2), the refill not
clearing it (1), `facing_at` always 0 (14), and a plane allocated for every
chunk (2).

**And the placement writes it, 2026-09-08.** Putting a block down turns the
CELL. `EditPlan` carries a facing, decided from the placer's POSITION -- which
is the authority's own state, since there is no look direction on the wire and
inventing one for this would be a field a modified client fills in -- and the
single-process client derives the same byte from the same function, so the two
put a block down the same way round. A block with no front is left at 0 rather
than turned to face nobody: a facing on a block that does not use one would
still permute its side pictures, and a mod that gave one four different sides
would find them spinning under it.

Protocol 23 -> 24: `BlockChange` carries the byte. It has to, and this is the
sentence that says why: a placement is a SINGLE CELL message, so a client told
only the block would draw it facing whichever way that cell happened to be
turned already, and stay wrong until the whole chunk was sent again -- which for
the chunk a player is standing in may be never. 23 and 24 are a day apart and
both of them are this entry; they are two numbers because 23 shipped as a commit
a build could be sitting on, and one number meaning two wire formats is the
failure a version exists to prevent.

`write_block` takes the facing and does not default it, and the turn counts as
a CHANGE: writing the same block over a turned cell is not "nothing happened".
That is what `core.set_block` needs -- it says which block and not which way
round, so a cell it writes over keeps no turn of the block that was there, or a
chest built where a furnace stood faces whichever way the furnace faced.

**A green sabotage, and what it was worth.** That last rule went GREEN when
sabotaged: no gate reached it. The defect was real and reachable -- a mod
rewriting a cell a player had turned -- and the only writer of turns was
placement, into air, so no fixture could set one up. The fix was not to drop the
check but to make the property reachable: **`core.set_facing` and
`core.get_facing`** now let a mod turn a cell, which is a thing OPEN-27 owed the
Lua API anyway. `mod_write` measures three things that did not exist before it:
a cell a mod turned BEFORE the socket existed arrives turned (3, inside the
chunk); the same block written over it takes the turn away (0); and a cell
turned while somebody is standing in the world arrives as a change (0 then 2),
with a 9-quarter-turn asked straight afterwards and ignored.

Gated in `protocol` (the byte round trips, on an answer and on a refusal, and a
facing that is not a quarter turn is refused), `sentry` (both turrets over a
socket come back as `sentry:turret` turned 3 -- the block's own name and a byte,
not a fourth name meaning the same block), `pack_draw` (four sides through
place_from_bag: one id, four turns, the front on screen from each and from
nobody else -- and +Z again at the end, on a cell the side before left at 3,
because a placement that only wrote the turn when there was one to write would
put the block down wearing the last player's) and `mod_write` as above. Nine
sabotages, all red on distinct checks: the server not turning the cell, the byte
not going on the wire, the client not writing it, the world written without it,
the turn not counting as a change, the client placing without it, a wild facing
let through, a mod's turn reaching nobody, and a wild facing masked into range
instead of refused.

**And the ids retire, 2026-09-08.** `NodeDef::turn`, `base`, `facing`,
`base_of`, `turned`, `is_turned`, the three extra registrations, `host_node_base`
in three hosts and `core.node_base` are all gone, and `mods/chest` compares
`ev.id` against the id `core.register_node` handed it -- which is the comparison
that was a defect in this very mod when a chest was four ids. `NodeShape` is a
name and nothing else, `find_nodes` is one comparison, `item_registry` and
`media_set` no longer skip anything, and the planner's permutation went with the
three sections of `face_tiles` that gated it -- `mesher` gates the permutation
now, on all four turns.

See CLOSED-27 above for what it cost, what it found, and what did not survive.

**Milestone.** None.

---

## CLOSED-26 -- two names for one picture cost two tiles

**Closed.** `plan_tiles` no longer asks "is there art at this name" but "what
picture is at this name": the callback returns an identity instead of a bool,
and the plan is keyed on that. The client makes the identity out of the DECODED
pixels -- deliberately the other way round from ContentHash's own rule, which
hashes bytes as they travel because the media cache must not decode what it
stores. Here the question is different: two encodings of one picture must cost
one tile, and that is the case a pack actually produces.

Measured with the gate's own pack, whose manifest sends six blocks to one
flat.png: **six tiles before, one after**, with all six blocks still wearing it
on all six faces and the seventh -- whose file is 100px in a 128px pack -- still
on its own placeholder.

**And the trap it walked into.** A rebuild RENUMBERS the tiles, and the numbers
are in the vertices, so every mesh built against the old atlas wears somebody
else's picture until it is built again. That was the caller's job to remember;
of the nine callers, the two on the media path -- where a world's textures
arrive after its chunks -- did not. Nothing had ever caught it because one tile
per file NAME meant the numbering hardly moved. One tile per PICTURE moves it
whenever a pack aliases two blocks to one file, and the gate came back with a
correct plan, a correct atlas, and 0.0% of the pack's colour on the screen.

So `rebuild_atlas` takes the world and says the meshes are stale itself. `init`
uses a private `build_atlas` instead, because at init there are no meshes to be
stale -- which is the only case where not saying it is right.

**And only when they MOVED.** The first version dirtied the world on every
rebuild, which put a re-mesh of the whole world on the media path -- the tick a
lossy link is busiest, and `bad_link` went red on the run after it. The
numbering only moves when the set of distinct pictures changes: measured in
`pack_draw`, bare to a pack is 32 chunks stale and the same pack again is 0.
Media arriving is the second case, so it now costs nothing.

That check reads the WORLD and not the renderer's flag, because the first
version of it read the flag: a rebuild that set the flag right and dirtied the
world anyway left it green. Measured -- and the count has to be taken before the
gate's own `sync` eats it, and kept in a local, because `select` resets the shot
it writes into afterwards. Both of those cost a green that meant nothing.

Gated in `face_tiles` (six names for one picture, one tile; and two names for
two pictures, still two -- a dedup that collapses everything is not a dedup) and
in `pack_draw`, which decodes real PNGs and photographs the result: the six
share one tile, all six wear it on all six faces, and the pack is on the world
after a rebuild the caller does not dirty. Five sabotages: the plan keyed back
on the name, the client using the name as the identity, the rebuild not saying
the meshes are stale, every picture given the same identity, and the identity
made of the size alone.

**Milestone.** None. The entry said "worth doing the first time an atlas runs
out"; it was done the first time something needed the seam anyway.

M7_FINDINGS has no entry for this. What follows is what the entry said.

## OPEN-26 -- two names for one picture cost two tiles

**Deduplication is by NAME, not by content.** `plan_tiles` gives one tile per
distinct file name the engine asked for. A pack whose manifest sends six blocks
to the same `flat.png` asks under six different engine names -- `core_stone.png`,
`core_grass.png` and so on -- so it gets six identical tiles.

**Where.** `plan_tiles` in `src/core/media/face_tiles.cpp` keys `by_file` on the
name it was given. The resolved name is not visible to it: the manifest is
applied inside the `MediaSource`, which is the seam that keeps the wire from
learning about packs (OPEN-25) and is worth keeping.

**Why it matters.** The atlas holds 256 tiles, because a tile index is eight
bits in the vertex. Thirty-nine blocks with six faces each is 234 -- there is
not much room, and identical tiles spend it.

**Next step.** The media are already content-addressed (`ContentHash`), so the
answer exists: hash the DECODED picture and key on that instead of the name.
It also catches two genuinely different files that happen to hold the same
picture, which the name can never catch.

**Milestone.** None. Worth doing the first time an atlas runs out.

---

## CLOSED-25 -- a pack did not travel as a pack

**Closed.** A world's pack goes out a second time, as ITSELF: the manifest under
`pack.manifest` and each file it points at under `pack.file.<its own name>`.
The client rebuilds the folder from those into the media cache -- named by the
manifest's content, so the same pack from two worlds is one folder and a changed
pack is a new one -- loads it, and puts it in the F7 rotation. A player can now
keep a world's art, wear it somewhere else, or take it off.

**It costs no blob and no protocol version.** The media set is
content-addressed, so a second NAME for bytes already in it is one row in a
table -- measured: the pack's own name and the engine's name resolve to the same
hash. And the negotiation is that a world with no pack offers no such name,
which is why the feature bit the entry asked for is not there: there is nothing
to agree about.

The names are FLAT for the reason the models learned the hard way --
`MediaOffer::decode` refuses a separator and one bad name costs the whole offer,
every texture with it -- and `pack_media_file` checks the rule again **on the
way back**, because the name came off somebody else's server and a client that
trusted the prefix would write wherever the string said.

A target the manifest names and the server cannot read is counted, not fatal:
the server's own atlas falls through on it too, so a client without it behaves
the same. That equivalence is the claim, and it is what the gate measures --
the rebuilt folder keeps the dangling alias rather than tidying it away.

Gated in `media` (the names both ways, the manifest and the file served, the
same blob as the engine's name, the offer round trip, and a rebuilt folder that
loads as the same pack with the same aliases) and in `net_client`, over a
socket: a world wearing a pack the client has never had, the client keeping it
by the name the pack calls itself, the folder holding the manifest and the file
it names, and that file byte for byte the server's. Five sabotages: the server
not serving it, the client not keeping it, the folder built with the manifest
alone, the way back trusting the prefix, and the folder not named by the
manifest.

**Milestone.** None.

M7_FINDINGS has no entry for this. What follows is what the entry said.

## OPEN-25 -- a pack does not travel as a pack

**A server wearing a pack serves its files, not the pack.** `build_media_set`
resolves the manifest on the side that owns the folder and offers each file
under the BLOCK's name, so a client sees `core_stone.png` and never learns that
a pack existed. That is deliberate for the protocol -- it is why the wire did
not have to change -- but it means a player cannot download a world's pack, keep
it, or put it on a different world, and F7 has nothing to cycle to on a world
that is not local.

**Where.** `src/core/media/media_set.cpp` (`build_media_set`) and
`world_media()` in the client. The media cache is content-addressed and already
holds every file; what is missing is a name for the SET and the manifest as a
served file of its own.

**Next step.** Offer `pack.txt` as one more media name, and let the client
rebuild the folder out of the cache. It costs a protocol feature bit and not a
version bump, because a server that does not offer it is a server with no pack.

**Milestone.** None.

---

## OPEN-35 -- the world is one box, generated whole, and nothing is buried in it

**Not a symptom: what the generator is, written down before a mod asks it for
more.** `Mapgen::generate` walks every column of the world -- `0..extent` in x
and z, the whole box -- and writes it before the server starts listening. There
is no generation per chunk, nothing generated on demand, and no world larger
than the box: `extent` defaults to 128 and `kWorldHeight` is 64.

**What ships.** Two stages, `core:terrain` and `core:scatter`, and four
climates -- plains, desert, tundra and peaks -- each of them a surface block, a
filler and a depth. The terrain is two octaves of value noise blended with the
biome's own base and relief, flattened to a hard-coded plaza (x and z from 20 to
100) because the portal scene stands on it and a continuity gate depends on it
staying put. The scatter is stone pillars, about one column in eighty, and they
exist to give the eye parallax rather than to be content.

**What is not there at all.** No caves, no ores, no water, no trees, no
structures. Everything between the bedrock and the filler is `core:stone`, which
means a pickaxe has nothing to find and OPEN-31's hardness has nothing to be a
progression through: a tool level is a gate onto an empty room.

**And a mod cannot add a stage.** M3 asked for "mapgen deterministico con stage
registrabili" and half of that is missing. `Mapgen::add_stage` takes a
`std::function`, so every caller is C++ -- the client's own `session:opening`,
`client:portal_walls` and `client:landmarks`. There is no `core.register_*` for
a stage, a biome or an ore, and `docs/API.md` has no word for any of them.

**The box is load-bearing, which is why the last step below is the expensive
one.** `world_hash` walks `extent` by `height` by `extent` and is what the
determinism gate compares; `--world` is a number the server and the client have
to agree on; OPEN-23 is this same constant seen from the culling side, where 64
blocks is two chunk layers and the occlusion walk therefore drops nothing on the
surface; and OPEN-16 is what a player finds at the edge of it.

**Next step, cheapest first, each worth doing on its own.**

  - **A stage a mod can register.** The promise M3 made, and the one item here
    that needs no new engine machinery: a name and a Lua function, called in
    registration order, holding the same determinism rule the C++ stages keep.
    It is also what lets everything below be written outside the engine.
  - **Ores and caves, as stages.** What makes digging a game rather than a
    chore. Caves want OPEN-23's finer walk first, or a player sealed in rock
    pays for drawing a cavern the chunk test cannot see into.
  - **Generation per chunk, on demand.** The one that is not cheap: it touches
    `world_hash`, the world file's schema, the protocol's world description and
    the streaming, and it is what an unbounded world actually means. Worth
    doing when something asks for it and not before.

**The first of those is done, 2026-09-11: a stage a mod registers.**
`core.register_mapgen(name, fn)` and `core.terrain_height(x, z)`. The stage is
handed the seed and the size of the world, writes with `core.set_block`, and
reads the ground with the generator's own height function rather than a second
derivation of the same noise.

**When it runs, which is three decisions and not one.** After the whole mod
directory has loaded, because what orders these is the order they were ASKED
for -- a stage that ran on registration would run before a mod later in the
directory had said anything. Before the world file is read, because a stored
chunk wins over a generated one and a stage is part of generating the world, not
part of starting the server. And before `core:world_ready`, which is the event
for a mod that wants to look at the finished world; a stage is not that, it is
what the world is made of.

**`core.set_block` inside a stage does not relight**, and that is OPEN-29's own
advice taken in the one place where a mod writes cells in the thousands: the
whole world is lit once when the pass is over, so the 1.9 ms a cell that entry
measures is not paid. A server whose mods register no stage does not pay for
that second lighting pass either.

**Two refusals, and they are the point rather than the tidying.** A stage asked
for after generation -- from a tick handler, say -- is refused with a sentence
saying which of the two orders went wrong. So is a name another stage already
has. The cheap answer in both cases is to accept it and never run it, which is a
mod doing nothing and never being told why.

**What a stage that throws costs.** Its own mod, and nothing else: what it had
already written stays in the world, every stage after it still runs, and the log
says it was the stage rather than the load. Half a stage is visible, which is
the honest state to leave a world in and is said here rather than discovered.

**What did NOT change.** The `mapgen_hash` in the welcome is still taken before
the mods load, so what a stage builds is not in it -- the same position that
entry has always had about anything a mod writes, and the reason is unchanged: a
client can reproduce the world a seed makes and cannot reproduce somebody else's
mods. And a world with a file still gets its stages run and then written over,
every start, exactly as the core stages do.

Gated in `worldgen` against three fixture mods (`tests/mods_mapgen`: one stage
that works, one that asks too late, one that throws) -- eleven checks covering
the registration order, the seed reaching the stage, the ground coming from
`core.terrain_height`, both refusals, the blast radius, and two runs of the same
mods over one seed making the same world -- and in a new `mapgen_stage`, which
asks the real server binary, because the gate above answers
`core.register_mapgen` the way the engine does and the way THAT goes wrong is
the fixture and the server drifting apart. Eight sabotages, all red on distinct
checks: the stages never run, a throw stopping the pass, registration never
closed, the stages run backwards, `terrain_height` always 0, the stage handed
seed 0, a duplicate name accepted, and a failed stage not costing its mod.

**A check that was weaker than its sentence.** "A world whose mods registered a
stage is not the world without them" stayed GREEN with the stage pass turned off
entirely -- those mods also register blocks and write one cell from a tick, so
the hash differs either way. It says what it actually measures now, and what
catches a pass that never ran is the count the server prints and the checks
under it.

**And something that ships registers one, 2026-09-11.** `mods/ore` was the
answer to "nothing that SHIPS registers a stage", which was this entry's own
version of OPEN-24: a mechanism nothing runs is a mechanism nobody has run. It
buries two ores in the stone from one Lua stage -- coal, one column in
thirty-two, a bare hand at 2.5 s; iron, one column in sixty-four, dig level 1
and in the bottom sixteen blocks, which a hand never gets however long it is
held. That second one is what OPEN-31 was missing: the tool level had one
outcrop in the plaza to be a progression through, and now it has a reason to
dig. What comes out is the block, because there is no furnace in this game and
the mod does not pretend there is.

**The density is a measurement and not a taste**, which is the part of this
worth carrying. Every ore cell is a break in an otherwise uniform run of stone,
so a chunk full of them compresses worse and streams slower. The first version
put coal in one column in eight: 7279 cells, and the far end of `bad_link`'s
shaped link carried 70.1 KB against 45.8 without the mod. At one in thirty-two
it is 1891 cells and 55.7 KB, and that gate stopped being the thing this content
was measured by. The numbers and what else came out of that are in OPEN-9.

Its determinism is a hash of the column and the seed and nothing else, with a
32-bit multiply written out by hand -- a double holds 53 bits exactly and the
product of two 32-bit numbers is 64, so `a * b` would round away the low bits,
which are the only ones a hash has. Only a cell that is `core:stone` becomes
ore, which is the one condition that keeps the stage clear of the surface, the
biome filler, the bedrock floor and whatever an earlier stage built.

Gated in `worldgen` against the SHIPPED mod directory, and against the world
rather than against what the mod says it did: both ores are in the stone, and
none of it is on the surface. Two sabotages, red on distinct checks -- the stage
never registered, and the ore placed at the top of the column instead of under
it.

**And it found two things that were already wrong.** `mapgen_stage` asked a
server with no `--mods` flag whether it said anything about stages, and a server
with no `--mods` flag loads the directory that SHIPS -- so what that check called
"a server with no mods" was a server with all of them, and it passed only for as
long as nothing in `mods/` registered a stage. It asks `--no-mods` now, and asks
the stage-free half of the question of a mod directory whose mods register no
stage. The other is OPEN-9's: a world that compresses worse streams slower, and
that turned an intermittent red into six out of six by widening a window the
client was already falling through.

**And the caves, 2026-09-12.** `mods/cave` is the second thing that ships with
a stage, and it is the one the finer walk was the condition for: a cave under
the open sky is a second piece of air in the same chunk, and a walk that added
the two together drew the whole world from inside the cave. CLOSED-23 landed, so
this could.

**One cave per square of 32 columns, chosen inside the square.** Not "one column
in N", which is what a vein is: a vein is small and a world holds thousands of
them, so chance averages out, while a cave is ninety-six steps long and a world
holds a few dozen -- and a hash that happened to pick none of the columns of a
small world is a world with no caves and a gate that passes on most seeds. This
way the count is a property of the world's size: **16 tunnels and 4937 cells on
a world of 128, 256 tunnels and 95044 cells on one of 512**, which is 0.57% of
it.

**The passage is a plus two cells tall**, five cells across and not nine: the
corners of a 3x3 are what turn a tunnel into a room. Every cell it cuts is one
of a vertical pair, and that is what the gate asks -- an exact claim, not a
share.

**Five in eight steps go the way the tunnel is going**, one sideways, one down,
one up. The bias is what makes it a tunnel rather than a blob, and the rest is
what makes it worth following.

**The roof is held six blocks under the ground the tunnel is under NOW**, asked
of `core.terrain_height` at every step and not once at the start: a tunnel that
kept its depth would surface on the far side of a hill. And only a cell that is
`core:stone` is cut, which is the same one rule mods/ore keeps and what holds
this clear of the surface, the biome's filler, the bedrock and whatever an
earlier stage built.

**It loads before `ore` and that is the design.** The mod directory loads in
name order, stages run in the order they were asked for, and `cave` sorts before
`ore` -- so the tunnels are cut and the ore then fills the stone that is left,
which is how a seam ends up in a cave WALL instead of inside a block nobody will
ever look at. The price is visible in the ore's own count: 22317 cells of coal
against 22869 without the caves, the difference being stone that is now air.

**What it costs to make and to send.** The stage adds 0.05 s to a world of 512
(1.37 s against 1.32 to generate and light it). On `bad_link`'s shaped link, the
far end carried **50.5, 50.8 and 52.3 KB with the caves and 53.8, 52.9 and 56.5
without** -- three runs each. The cave's cost does not show above that gate's
own spread, and the sign is the wrong way round for it to be a cost at all,
which is the opposite of what OPEN-9 found for speckled ore and is why it was
measured rather than assumed.

Gated in `worldgen` against TWO worlds -- the same seed with the core stages
alone and with the mods, cell by cell -- so what is measured is the world and
not the mod's account of itself. Three sabotages red: the stage never
registered, the roof not held at every step, and the passage cut one cell high.

**Three guards are not exercised, and saying which is the point.** Carving
anything instead of only `core:stone`, sparing the floor of the world, and
measuring the depth from the ground rather than from zero all leave the world
bit for bit the same -- this generator's terrain is about 20 blocks tall and the
tunnels start between 3 and 10, so they are never near the floor, never near the
filler, and the clamp that would pull them down never fires. They are kept
because the first of them is what stops a cave eating another stage's work and
the others are what a lower terrain would need, and they are written here
because a guard nothing can fail is a guard nobody will notice going.

**What is left of this step.** Nothing that turns ore into anything, which is a
furnace and a mod. Caves that MEET -- two tunnels from neighbouring squares can
cross, and nothing measures whether they do or connects them on purpose. And the
client's copy of the pass -- the single-process loop runs stages too, so a mod
behaves the same in both -- is read and not gated, the same limitation the dig
bar's two lines and the chest lid's tween have.

**Milestone.** M12, the contents. It was M11 as well, because DESIGN.md listed
mapgen aliases among what a mod of the original engine expects and "il gioco esistente avvia, genera un
mondo" was that milestone's third test step. M11 is cancelled (see the top of
this file), and the half that survives is the half that was never about
compatibility: a world only C++ can shape is a world a mod cannot make its own,
whoever wrote the mod.

---

## OPEN-37 -- a pinhole that one pixel grid can see and no other

**Symptom.** From a camera in the bore of `frame_budget`, single pixels show
the sky through solid rock. Each has rock on all eight sides of it, and they
lie on two lines about thirty pixels apart:

        F bore    18 chunk(s) drawn with the walk, 125 without, 3 pixel(s) differ

**Not made by the walk, only uncovered by it, and that is now measured rather
than argued.** The same view drawn with the walk OFF has the same ten lone
pixels; what the walk changes is only what is behind them -- a chunk that
happened to fill the pinhole, or the clear colour. Three of the ten are the
three that differ.

**What it is, 2026-09-12: the pixel grid and nothing in the rock.** The same
view, same camera, same world, drawn at seven resolutions of the same 16:9
shape -- so a screen position 0..1 is the same RAY in all seven:

| grid        | lone pixels |
|-------------|-------------|
| 960 x 540   | 0           |
| 1280 x 720  | 0           |
| 1600 x 900  | 0           |
| 1920 x 1080 | 10          |
| 2240 x 1260 | 0           |
| 2560 x 1440 | 0           |
| 2880 x 1620 | 0           |

and, at 1920x1080, with the eye nudged along the axis it is standing on:

| eye off the axis | lone pixels |
|------------------|-------------|
| 0                | 10          |
| 0.0001 block     | 0           |
| 0.001 block      | 0           |
| 0.01 block       | 0           |

A ten-thousandth of a block is about a fiftieth of a pixel out there, and no
surface in the picture moves by as much as one.

**What that rules out is a WIDTH.** A gap in the geometry -- a missing sliver, a
T-junction crack, two surfaces that do not quite meet -- has one, so a finer
grid puts more pixel centres inside it: it shows at every resolution and grows
with the grid. This shows at one grid and vanishes on the two either side of it.
Whatever is left is zero-width: a pixel centre landing exactly ON a projected
edge and being claimed by neither of the two triangles that share it. That is a
thing this camera arranges rather than suffers -- it stands exactly at the
centre of a voxel and looks exactly along +x, so every edge in front of it
projects symmetrically about the middle of the screen, and at one grid those
lines pass exactly through pixel centres.

**And it is NOT where two chunks meet**, which is what this entry said when it
was opened. Anything thirty blocks down the bore -- the nearest chunk boundary
-- is within half a block of the view axis, which is 1° off it, and lands within
about fifteen pixels of the middle of the screen. These pixels are four hundred
and more from it, so they are on geometry a block or two from the eye, inside
the camera's own chunk.

**The chunk-seam crack, made real as a sabotage, to see what one looks like.**
Every chunk's mesh shifted a five-hundredth of a block along x -- so every
chunk boundary is a real gap of real width: **3 lone pixels at 960x540, 5 at
1920x1080, 13 at 2880x1620**. Present at every grid and growing with it, which
is the signature the thing in this entry does not have. The camera nudge did
NOT tell those two apart -- a gap that narrow is sub-pixel wherever the eye
stands, so it read zero off-axis as well -- and it is printed rather than
checked for that reason.

**How the gate stands.** `frame_budget` keeps two rules about this view. No
differing pixel may TOUCH another, which is what a chunk the walk wrongly
dropped would be -- a wall gone from a room, sky through rock -- and which a
pinhole cannot be. And the coarse grid and the fine grid must both see nothing
through it, which is red on both halves under the sabotage above. The count at
1920x1080 is printed beside them and deliberately not checked: a check that
asserts a defect is still there goes red the day somebody fixes it.

**What is not covered, and saying it is the point.** The detector finds a pixel
brighter than all eight of its neighbours by a wide margin, so a detector that
found NOTHING would pass this check as well. What stops that going unnoticed is
the count printed next to it, and there is no scene here where a lone bright
pixel is known to exist on purpose.

**What is still unexplained.** Which edges, exactly -- the bore is cut through a
world that `mods/cave` has tunnelled, so the geometry a block from the eye is
not the clean 1x1 corridor the scene reads as. And why 1920x1080 of the seven,
which is a question about where a projected edge falls to the ulp and not about
the rock.

**Next step, and it is not a fix.** One pixel, at one resolution, from a camera
standing exactly on a lattice point: worth knowing, not worth turning the
mesher inside out for. If a player ever reports it, the cheap mitigation is a
fraction-of-a-pixel jitter in the projection, which costs every frame in the
game to hide something that takes an exact pose to see. What this entry is for
now is the measurement above, so the next person does not re-suspect the chunk
seams.

**Milestone.** None.

---

## OPEN-36 -- `dual_session` went over the meshing budget once, under load

**Symptom.** One red in a `-j2` suite run, 2026-09-11:

    the worst tick spent 5.61 ms meshing across both worlds, of a 4.0 ms budget
    FAIL  and the two of them together stay inside one budget, not one each

Everything else in that gate green, including the two checks either side of it
(the passive world meshes, and at a lower priority than the active one).

**Not reproduced alone.** Three runs of `dual_session` on its own straight
afterwards, all green at 1.12 s. It has been green in every other suite run on
this tree, `-j2` included.

**What it is not known to be.** A budget measured in milliseconds on a machine
running another gate beside it is a budget measured against whatever that other
gate was doing, so this may be load and nothing else. It may also be a real
overrun that load makes visible, which is the same sentence OPEN-9 spent a week
on -- so it is written down rather than dismissed, with one observation and no
hypothesis.

**Next step.** If it comes back, note what was running next to it: ctest prints
the interleaving, and the two gates that were in flight are what separates load
from an overrun. A budget check that only holds when the machine is otherwise
idle should say so in its own text.

**Milestone.** None.

---

## OPEN-38 -- nothing is ever unloaded, and the world is a box so nothing has had to be

**Not a symptom: a limit of what exists, written down rather than left for
whoever first makes a world that does not fit.**

**What there IS is a radius on the WIRE.** `PlayerInterest`
(`src/core/net/interest.h`) decides what the server SENDS: four chunks of
radius, two vertically, a forget margin of two chunks so a player pacing across
a boundary does not resend one every step, four chunks and 128 KB a tick. The
look direction orders the queue and never decides what is in it, because a
player who spins around should not wait for the world behind them.

**What there is NOT is anything that unloads.**

  - `World` has no way to remove a chunk. `m_chunks` only grows.
  - The client installs every chunk that arrives and keeps it
    (`client_session.cpp`, `MsgType::Chunk`). There is no message on the wire
    that says a chunk is gone.
  - `forget_distant()` drops the server's record that it SENT a chunk, so the
    chunk is sent again when the player comes back. It tells the client nothing.
  - The renderer's `m_meshes` and `m_visibility` only grow, and are cleared
    when the renderer is destroyed and at no other time. A chunk a hundred
    blocks behind the player keeps its GPU buffer.

**Why it has never mattered.** The world is a box of fixed side generated whole
before the first tick: 512 x 64 x 512 is **512 chunks and 37.9 MB**, all
resident from the start on the server and, for a client that stays long enough,
all of it there too. Nothing arrives late and nothing can grow, so "drop the
distant ones" has never had a case to answer.

**What limits the work today is drawing and not residency.** The view range
(512 blocks, per session, and a passive session gets less), the frustum, and the
occlusion walk. All three decide what is DRAWN this frame. None of them frees a
byte.

**What it would take, and the part that is not obvious.** A residency radius on
the client -- drop the chunk, free the mesh, drop the visibility entry -- and
generation per chunk on the server, which is OPEN-35's last bullet and the
expensive one. The part worth knowing before starting: the two halves have to
agree about what the client holds. The server resends a chunk only when its own
`forget_distant()` has dropped it, so a client that dropped a chunk the server
still believes it sent is a hole the player walks into, and the hysteresis that
makes the server's half cheap is exactly what makes the disagreement possible.
Either the client's radius is strictly inside the server's forget radius, or the
client has to say what it has dropped.

**And one that is a physics bug rather than a graphics one.** A chunk dropped
while the player is standing in it is a player falling through the world --
OPEN-16's shape in another place. Whatever the radius is, it cannot be allowed
to reach the chunk under the feet.

**The walk is already right about this.** It does not cross a chunk it has never
been told about, and an unloaded chunk is one it has not been told about, so a
hole in the loaded set stops the walk instead of sending it wandering. That rule
was bought and paid for by M9 -- treating the unknown as see-through cost
2.5 ms a frame -- and does not have to be bought again.

**Next step.** Nothing asks for this until a world is bigger than memory, and
that is the day generation per chunk lands. Do it with that and not before: a
residency radius over a world that is entirely resident anyway is machinery
with no failure to catch, and it would be written without the one measurement
that should size it.

**Milestone.** None today. It becomes M11's and M12's, on the day a world this
engine is asked to hold is one somebody else's game made.

---

## OPEN-39 -- a mod is a directory, and nothing about it travels

**Not a symptom: what a mod IS today, written down before M11 asks it to carry
somebody else's game.** A mod is a directory under `mods/` with a `mod.conf` and
an entry file. It is found by listing that directory, loaded in name order and
then reordered by `depends`, and the only way to give one to somebody is to give
them the directory.

**What that leaves out, in the order it bites.**

  - **Nothing says which engine a mod was written for.** The Lua API grows every
    milestone, and M11 will change it rather than only add to it. A mod written
    against the old shape fails on the line that calls what is gone -- forty
    lines in, halfway through registering its blocks, with half a mod in the
    world.
  - **A version could not be compared.** `version` was a free string, printed in
    the load line and read by nobody, so `depends` could only ask for a NAME.
    A mod needing the version of another that has the function it calls had no
    way to say so.
  - **A mod does not travel as one piece.** There is no archive, no manifest of
    what is inside it, nothing that says the files arrived whole. This is the
    same sentence OPEN-25 wrote about texture packs and OPEN-28 wrote about
    models, and both were closed by making the thing travel.

**Next step, cheapest first.**

  - **Versions and an API number**, which is parsing and refusals and touches no
    format.
  - **A package that travels**: one file, a name and a version on the outside,
    and the engine loading a mod out of it without the directory ever existing.
  - **Where a package comes FROM**, which is not asked for by anything in this
    repository and is where a mod format usually grows a server, a signature and
    a trust decision. Not now.

**The first of those is done, 2026-09-12.** `version` is three numbers, `api` is
one, and `depends` can ask for a range.

**A version is three numbers because a version gets compared.** `1`, `1.2` and
`1.2.3` are all versions and what is not written is zero; a letter, an empty
component or a fourth number is a refusal at load, not a value somebody later
reads as "no version". As a string `1.10` sorts before `1.9`, which is the
opposite of what a mod asking for `>= 1.9` means, and that is the whole reason
this is not `std::string` any more.

**`depends = spike >= 0.2, guard`**, with five comparisons and any spacing. A
bare name still means any version, because that is what every `depends` in this
repository meant before today and a manifest that loaded yesterday has to load
today -- including `depends = spike guard`, which the old whitespace-splitting
list accepted.

**Two failures, two sentences.** "Depends on 'spike', which is not loaded" sends
somebody to install a mod. "Depends on spike >= 2.0.0, and the one loaded is
0.2.0" sends them to upgrade the one they have. Printing the first when the
truth is the second is an afternoon lost, which is the same reason the dig-level
refusal names both levels rather than saying "you need a better tool".

**`api` is a whole number and not a version**, and it is the one a refusal is
made on. `kModApi` is what this build speaks, `kModApiOldest` the oldest it still
speaks, and while they are equal there is exactly one API: a mod declaring any
other is refused with both numbers in the sentence. It is deliberately not the
engine's version string, for the reason `core/version.h` already gives about the
protocol -- a string is written by a person and can be wrong, a number derived
from what the API actually is cannot.

**A manifest that says no `api` is taken for the OLDEST**, not for the current
one, and the load line says `(api 1, assumed)` so the log tells the two apart.
Taking it for the current one would silently promote every old mod onto a new
API on the day one lands; taking it for the oldest means that on the day
`kModApiOldest` rises, that mod is refused and told to say what it wants.

**The same rule refuses a load and a reload**, in one function, for the reason
the capabilities are already re-read on reload: a mod that could edit its
manifest into something this build does not serve and have it swap in would be
a rule it is on both sides of. A reload refused keeps the version that was
running, which is what a reload refused has always done here.

**And something that ships asks for a version.** `mods/beacons` depends on
`spike >= 0.2` -- it reads `spike`'s blocks by name, and `spike` is 0.2.0. All
ten shipped mods declare `api = 1`, which is this entry's answer to OPEN-24's
sentence: a mechanism nothing ships with is a mechanism nobody has run.

Gated in a new `mod_package`, in three layers that are not interchangeable: the
parsing, where a typo has to become a refusal; the ordering, where `1.9.0 <
1.10.0` is the thing a string gets wrong; and the loading, against directories
written by the test and against the mods that SHIP, because the sentence a
refusal prints is the product here. Eight sabotages, all red on distinct checks:
the version compared as text, a fourth number accepted, a dependency's version
never checked, the API never refused, a reload not asking, the manifest
forgetting it was told the API, an attached operator read as part of the name,
and a dependency name taken as whatever was left over.

**And the second is done, 2026-09-12: a mod travels.** `.cnpmod` is a mod as
one file -- `src/modpack/mod_package.h` -- and `tools/modpack` is what makes
one: `cnp_modpack pack mods/ore` writes `ore-0.1.0.cnpmod`, `list` says what is
inside and whether it is whole, `unpack` gives the directory back. A package
dropped in the mods directory loads: `load_dir` verifies it, unpacks it into a
cache directory and reads it from there, so everything downstream -- the
manifest, the order, the refusals, the reload -- is the one path it always was.

**Its own library, under both and over neither.** `cnp_modpack_lib` knows
nothing about a VM, a registry or a world, because the engine READS packages and
a tool WRITES them and neither should have to link the other. What is under it
is zstd, already linked for the chunks on the wire, and monocypher's BLAKE2b,
which is the hash `core/media/content_hash.h` uses for a file a world serves --
the same 32 bytes, so a package hash and a media hash mean the same kind of
thing. It is what let `src/script` stay free of `core/`, which mod_host.h has
been claiming since M2.

**One zstd frame over all the files, not one per file.** A mod is a dozen small
text files that compress against each other, and compressing them apart would
throw that away to buy random access nothing wants: a mod is unpacked whole or
not at all. `mods/ore` is 9047 bytes of files in 3379 bytes of package.

**The header does not carry the mod's name or version**, and that is the
decision most likely to be questioned later. They are in the mod.conf inside,
which is the only place a manifest is read from: a name in two places is two
names, and the one on the outside is the one that can be a lie. What the outside
is for is deciding whether the inside is intact. The TOOL reads the manifest to
name the file, which is why it links the manifest parser and the format does
not.

**A package is canonical.** Entries sorted by path, their bytes contiguous and
covering the payload exactly, so packing one directory twice gives the same file
to the byte -- gated, and measured on `mods/ore` through the real tool. Without
it a package hash would say "who packed it" rather than "what is in it", which
is the whole of why anybody would compare two.

**Verified whole before a line of it is read**, which is the same rule OPEN-28
settled for models and for the same reason: half a mod registers half its blocks
and leaves a world full of them, and nobody looking at that world can tell which
half it got.

**Every field in a package is somebody else's claim**, and this is the first
thing in this engine that arrives from somewhere else, so the refusals are the
product: not a package at all, a header cut short, a payload cut off, a byte
glued on the end, a format number from a later build, no files, a file larger
than the cap, an index that is not sorted or does not account for every byte of
the payload, contents that do not hash to the header's claim, a path that walks
out of the mod, and a package with no mod.conf in it. A path is checked three
times -- written, read, unpacked -- because the one that writes to somebody's
disk cannot be the one that trusts a caller who did not come through the parse.

**Two mods may not share a name**, which could not happen before: a directory
listing has one entry per name, and a package beside the directory of the same
mod is two copies registering the same blocks. The second is refused naming
where the first came from, and the load order is a TOTAL order -- name, then
where the files are -- because `std::sort` is free to put equal elements either
way round and "which copy loaded" must not be a coin flipped again every start.

**The cache is not next to the package.** A mod shipped inside an installed game
sits in a directory nobody can write to, which is the case M10 is about, so
packages unpack into the system's temporary space by default and
`set_package_cache_dir` moves that. The directory is named by the package's name
AND its hash, so two versions are two directories rather than one somebody half
overwrote, and a package that has not changed is unpacked once.

**A package does not hot reload.** The watcher compares the modification times
of the unpacked files and they do not change. That is the honest shape of it --
a directory is what you edit, a package is what you ship -- and it is written
here rather than discovered.

Gated in `mod_package` for the format (the round trip, the determinism, every
refusal above) and in a new `mod_pack.sh` for the binaries, which is the same
reason `mapgen_stage` exists beside `worldgen`: the format can stay right while
nothing that ships can read a package. That gate packs `mods/ore` with the tool,
hands the server a mods directory holding nothing but the file, and compares
WORLD HASHES -- `e9fbdf860bd4129d` from the directory and from the package,
which is the claim that the packed mod RAN rather than merely loaded, since its
mapgen stage is what the hash is over.

Ten sabotages on the format, all red on distinct checks: the payload hash never
verified, a path in the index trusted, unpack trusting whoever parsed, the index
free to be unsorted or full of holes, the stored size unchecked, the writer not
sorting, a later format read anyway, a package with no mod.conf accepted, the
mods directory not looking for packages, and two mods allowed to share a name.

**And two of those stayed GREEN in the shell gate, which is why it now tampers
with two bytes and not one.** A byte changed in the PAYLOAD stops zstd before
the hash is ever reached, so that check was answering "a corrupt package is
refused" and not "the contents are checked against the header" -- it changes a
byte of the hash field as well now, and that one is red without the check. The
other is the writer's sort, which that gate cannot see because the directory
reader already sorts; the format gate catches it with ten reds.

**What is NOT covered.** The load order is still decided by name and `depends`
alone -- a version requirement is checked when the mod starts, not used to
choose between two copies of a mod, because there is nowhere for a second copy
to be. And nothing on the wire carries any of this: mods are the server's, a
client adopts a block table by name, and a client never learns what mods made
the world it is in.

**What is left of this entry.** Where a package comes FROM: there is no index,
no server, no signature and no trust decision, and nothing in this repository
asks for one. A package is a file somebody hands you, verified against its own
hash -- which says it arrived whole, and says nothing at all about who made it.
That is the point at which a mod format usually grows the rest, and it is worth
doing when something asks rather than before.

**Milestone.** M10, first of its four: "formato pacchetto mod + versionamento",
both halves. What is left of M10 is static builds, AppImage and Flatpak, and the
API documentation read as a release document rather than as a generated file.

---

## OPEN-40 -- a copied binary reads somebody else's mods

**Not a symptom: what a build WAS, found on the way to making one that
travels.** Every path this engine ships with was baked in at compile time:

```cpp
const char *mods_dir = CNP_SOURCE_ROOT "/mods";
std::string textures_dir = CNP_SOURCE_ROOT "/assets/textures";
```

`CNP_SOURCE_ROOT` is `CMAKE_SOURCE_DIR`, so a binary copied anywhere reads mods
out of the directory it was BUILT in. On another machine that directory does not
exist and the mods silently are not there. On the same machine it does exist,
which is worse: a copy of the game reads a checkout somebody is editing, and
does it without saying so.

**Fixed, 2026-09-12: next to the program first.** `core/paths.h` answers where a
build's files are, in the order of how much somebody meant it -- the flag, then
what is beside the executable, then a couple of directories up from it (a build
tree has its binaries a few levels down from the files it ships), then the tree
it was built from. `/proc/self/exe` and not `argv[0]`: argv[0] is whatever the
caller felt like passing, and resolving data out of it would follow a PATH
lookup into a directory nobody meant.

**And it says which one it chose.** `mods from <dir> (next to the program)` or
`(the tree it was built from)`, printed by both the server and the client. A
build reading somebody else's checkout has to be visible in its own log rather
than under an strace, because the two cases look identical from the outside and
differ by a machine's worth of consequences.

**The C++ runtime goes in.** `-DCNP_PORTABLE=ON` links libstdc++ and libgcc
statically, because that is the one library a distribution cannot assume: built
against a newer one, the program dies at startup on an older machine with a
message about a GLIBCXX version that names neither the program nor the fix. What
stays dynamic is the system's -- libc, and the GL and X11 the driver owns, since
linking those in is not portability, it is a program carrying its own graphics
driver. The server comes out with four lines of `ldd` and the client with
twelve, all of them the machine's.

**And a tarball.** `tools/dist/package-linux.sh` puts the three programs,
`mods/` and `assets/textures` in one directory under the names the binaries look
for beside themselves -- that layout IS the contract with `core/paths.h` -- and
tars it: 8.4 MB for `cubes-and-portals-0.3.0-m3-linux-x86_64`. It REFUSES a
build that links libstdc++ dynamically, naming the flag, because a tarball that
runs here and dies there is the failure this whole step is about.

Gated in a new `dist`, which is the first gate here that does not run a binary
from its own build tree: it packs, unpacks the tarball somewhere else, runs the
server from the root of the filesystem -- neither of the two -- and checks that
the log says it read the mods next to it, that the world it makes is the world
the build tree makes, and that nothing either program links points back into the
build directory. The first check reads both ways round: on a build that carries
its own runtime the packaging must ACCEPT, and on one that does not it must
refuse.

Four sabotages, all red: a program that cannot find itself, the built-from tree
winning over what is beside the program, a tarball with no mods in it, and the
packaging accepting a build that cannot travel.

**And one check that cannot fail on its own, said because it cannot.** "The
world it makes is the world the build tree makes" stayed GREEN under the two
sabotages that made the unpacked server fall back to the source tree -- of
course it did: those are the same mods. That check says the mods it found are
the same ones and not merely some; what says it found them NEXT TO ITSELF is the
line it prints, and that is the check that went red.

**What is NOT covered.** Windows is not built here at all. AppImage and Flatpak
are not made: `appimagetool` and `flatpak-builder` are not on this machine, and
what they would wrap is the directory this already produces. The client is not
run by the gate, because it needs a window and this runs where there is not one
-- so what is measured is the server, and the client is packed and checked for
its dependencies rather than started.

**Milestone.** M10, the second of its four. What is left is Windows, the two
bundle formats, and the API documentation read as a release document rather than
as a file the generator writes.

---

## OPEN-41 -- the documentation of the API was never asked whether it was true

**Not a symptom: what the documentation of this API rests on, written down
because the answer was "nobody checks".** `tools/api_gen` writes `docs/API.md`
and `docs/api.d.luau` out of `src/script/api.luau`, so everything that goes
through the generator is documented by construction -- adding a function there
is six lines and the documentation follows.

Five things do not go through it, because they take tables and functions rather
than scalars: `core.event.*`, `core.ecs.*`, `core.net.*`, `core.render.*` and
`core.register_mapgen`. They are written by hand in `script_luau.cpp` and
documented in the prose of `MODDING.md`, and prose does not follow anything.
That is the shape of a call that exists and is written down nowhere -- and of
the other direction, which rots more quietly: a call that was removed leaves a
paragraph telling somebody to use it, and the first mod to try gets "attempt to
call a nil value" from a line the documentation told them to write.

**Asked, 2026-09-12, and the answer was yes.** A new `api_docs` gate creates a
VM the way the engine does, walks the `core` table **a mod actually sees** --
after the capabilities were removed and the globals frozen, so it is the API and
not a list kept beside it -- and checks both directions. All **60** names are
written down in one of the three files a mod author reads, and every heading in
the generated reference still exists.

**It is walked inside the VM**, by a chunk that returns the names through
`on_save`, rather than enumerated from C++. What is being asked is what a mod
can call, and any list assembled on the C++ side would be a second declaration
of the thing under test.

**A heading is a call or a family.** `API.md` ends with a section per
hand-written family saying where the prose is, so `core.event` is a heading with
a TABLE behind it rather than something to call. Both count as alive; a heading
that is neither means the committed file is older than the build.

Three sabotages, all red on distinct checks: a hand-written binding added and
documented nowhere, one family call taken out of `MODDING.md`, and a heading in
`API.md` with nothing behind it.

**What it does NOT check.** Whether what the documentation SAYS is true -- only
that the name is there. A paragraph describing the old behaviour of a call that
still exists passes this, and nothing but reading catches it. And the four
families are named in the gate itself, so a fifth hand-written family is
something somebody has to add there; that is a decision made once rather than a
thing that happens quietly, and it is written here because the gate cannot tell
the difference.

**Milestone.** M10, the third of its four. What is left of M10 is Windows and
the two bundle formats, and both are machines this one is not.

---

## OPEN-42 -- nothing happened to a cell unless somebody touched it

**Not a symptom: what this engine could not do, written down on the way to
doing it.** Every change to a cell came from a player's click, a mod's tick
handler, or the generator. Grass did not spread onto dirt, a furnace could not
cook, water could not flow, and a world opened again was exactly the world that
was closed. A mod that wanted any of it had to walk its own list of cells from
`on_step`, which is a list that does not survive a restart and a walk nobody
can budget.

**Done, 2026-09-12: two kinds of modifier.** `core.register_abm(name, nodes,
interval, chance, action)` runs every `interval` seconds on each loaded cell
holding one of its blocks, with a one-in-`chance` chance each time.
`core.register_lbm(name, nodes, action)` runs once per matching cell of a chunk
WHEN THE CHUNK LOADS, and never again. Two calls and not one with a flag,
because they are two different things to a mod author and a flag would let a
typo swap them.

**Three properties decide whether a world can be left running with one**, and
all three are in `core/world/modifiers.h` rather than in whoever calls it.

  - **It must not read every cell.** A world of 512 is 512 chunks and 16.7
    million cells. Every chunk now carries a bit per block id MODULO 64
    (`Chunk::id_bits`), so a chunk that cannot hold an ABM's blocks is skipped
    with one AND. Measured in the gate: one cell of the block in one of 64
    chunks is found by reading **32768 cells of 2097152**. Conservative in two
    directions and both deliberate -- ids collide modulo 64, and a bit is never
    cleared, so a set bit means MAYBE and the cost of being wrong is a scan that
    finds nothing. Eight bytes a chunk and one OR per write; a list of the
    distinct ids would be a scan of that list on every write, and the mapgen
    writes sixteen million of them.
  - **It must not be able to make the tick late.** A step is given 2 ms and
    stops when they are spent, keeping its place -- chunk index and cell index
    -- and the interval is counted only from a sweep that FINISHED. So a mod
    that asks for more than the budget allows runs slower than it asked rather
    than making the server late, and `behind()` says how many sweeps are still
    going instead of leaving it to be guessed. Gated on 200000 cells of one
    block with a budget of 1 ms: three steps, none of them over, and the sweep
    completes.
  - **What it does to a world must be the same on two runs of one seed.** The
    chance is a hash of the seed, the sweep number and the cell's coordinates --
    not a generator whose state depends on how the budget cut the sweep up. The
    gate runs the same world with a whole budget and with almost none and checks
    that the cells fired are the same ones in the same order. The chunks are
    walked in sorted order for the same reason, which is what
    `World::chunk_positions_sorted` is for: an unordered_map's order is not a
    property of the world.

**The clock is the world's, not the wall's.** A sweep due "one second later"
has to mean twenty ticks later on every machine, or two runs of one seed are
two worlds.

**An action that errors costs its own mod** and that modifier stops being
called, which is the bargain an event handler already makes. The others keep
running in the same step, gated.

**What an LBM does not promise, said rather than discovered.** If the budget
runs out in the middle of one chunk's loading modifiers, that chunk goes back in
the queue and the ones that already ran over it run again. An LBM has to be
written so that running twice over a cell it has already fixed is harmless --
which is what "loading" means for a modifier that can be interrupted.

**The action takes ONE table** -- `cell.x`, `cell.y`, `cell.z`, `cell.id` --
which is the shape every handler in this engine takes, from an event to a mapgen
stage. Found the honest way: the first version of `mods/timber` read four
arguments and the server said `invalid argument #1 to 'core.get_facing' (number
expected, got table)` on its first tick.

**And something that ships uses both.** `mods/timber` grows moss on a log every
two seconds with a chance of one in four, and scrapes it off again when the
world is reopened. It acts on its own blocks and on nothing else, which is the
whole of its blast radius: five cells in a world, all of them placed by that
mod, so no other gate's world moves under it.

Gated in a new `modifiers` (the scheduler, against a C++ action: registration
refusals, the sweep, the chunks skipped, the share a chance fires, the
determinism, the budget, the LBM's once-per-cell, and an action that throws) and
in a new `abm.sh`, which asks the real server -- because an LBM cannot be
demonstrated without a world file, and a world file needs a binary. That gate
grows moss over 300 ticks, writes the world, opens it again, and checks that the
LBM ran over exactly the cells the ABM had changed and that the second reopen
finds less to do.

Nine sabotages, all red on distinct checks: the bits never asked so every chunk
is read, the budget never checked, the chance drawn from the cell alone, a
sweep's interval counted from when it was due instead of when it finished, an
LBM run on every step rather than on a load, an action that failed called again,
registration never closing, a name nothing registered accepted, and a write that
does not record its id in the chunk's bits.

**What is NOT there.** No batching: the action is one call per cell, and a mod
firing on thousands of cells a second pays for thousands of crossings into Lua.
The ECS systems of M8 take columns for exactly that reason, and the same could
be done here the day a number asks for it. There is no "run this on the cells
around a player first", so a sweep is fair rather than useful -- The original engine's own
ABMs are limited to active blocks near players, and this engine has no such set
yet. And a chunk that arrives mid-sweep is not visited by that sweep.

**Milestone.** M11, which lists an ABM/LBM scheduler among what the engine owes
the compatibility layer. It is also what OPEN-31's hardness wanted next: a
furnace that cooks is an ABM, and there was nothing to write one with.

---

## OPEN-43 -- a block's shape was a picture of a shape

**Not a symptom: what a nodebox WAS here, written down on the way to making it
one.** `NodeDef::parts` arrived with the chest: a block may declare boxes inside
its cell, and the mesher stops meshing that cell as a cube and hands it to the
pass that draws boxes. That is where it ended. The collision resolver, the
server's movement validator and the raycast all went on reading the cell.

So a slab was a picture of a slab. You stood on air half a node above it; the
empty half of its cell was a wall you could not walk into; aiming at the sky
over it selected it; and a block built on it went on top of the CELL, half a
node in the air. Nothing was wrong on screen, which is why this survived the
chest -- a chest fills nearly all of its cell, so being wrong by the shape of it
is being wrong by a few centimetres nobody walks into.

**Why it blocks M11.** DESIGN.md M11 lists `drawtype nodebox+mesh` among what
the engine owes the compatibility layer, and the second gradino of its exit test
is `yaportal`, which has fourteen nodeboxes in it. A nodebox whose shape is only
drawn is not a nodebox: it is a cube wearing one.

**Done, 2026-09-12: the boxes decide where a body goes and what an aim finds.**
`core/registry/node_shape.h` is the one place that answers "where are this
cell's boxes", in world coordinates, turned the way the cell is turned. The
rotation is `orientation_matrix` -- the mesher's own, from OPEN-27 -- because a
box is corners in the cell and a table of face permutations cannot turn corners.
A block with NO boxes is one box the size of the cell, so the callers have one
path and not two, and that is every block this game ships but one.

Three callers read it, and they had been three copies of "is this cell solid":

  - **`box_hits_world`**, which is where a player can walk. Through
    `node_blocks_box`, which the collision resolver and the validator now share
    -- one rule, because a shape the walker knows about and the validator does
    not is a player corrected for standing where the game let them stand.
  - **`inside_solid`**, which is what the server ACCEPTS, and what a portal's
    arrival accepts. It keeps its own margins -- a twentieth of a node at the
    feet and the scalp, so a body settled into the floor is not called buried --
    and those margins are now a box it hands over rather than a cell range it
    walks.
  - **`raycast`**, which is what a player is aiming at. A cube is still met the
    moment the ray is in its cell; a shape has its boxes tested, and a ray that
    misses all of them carries on to whatever is behind. The FACE comes from the
    box and not from the cell boundary -- the top of a slab is not the top of
    its cell, and the face is what decides where the next block is built.

**Gated in a new `node_shape`, and the gate does not trust the engine for the
answer.** It works the boxes out itself, from `orientation_matrix`, and asks
whether the three of them agree with what would be DRAWN. Measured: 177 957
probes against a slab, a shelf and a cube with 0 disagreements; 81 000 probes
over all twenty-four orientations of a two-box step with 0; and the older
walker-against-validator sweep from `physics_box_test`, 925 101 positions around
a slab, 0 of them called solid where the collision box hits nothing.

Eight sabotages, all red: collision reading the cell instead of the boxes, the
validator left asking `is_solid`, the shape not turned with the cell, the turn
applied transposed, a ray stopping at the cell of a shape, the face taken from
the cell boundary always, a cube no longer stopping anybody, and a shape keeping
only its first box. Two of them -- the dropped turn and the transposed turn --
go red on the same three checks, which is said rather than dressed up: a wrong
rotation is a wrong rotation, and no check here separates them.

**And the game has them in it, 2026-09-12 -- which is the half a gate about the
mechanism cannot see.** `node_shape` registers its own slab, exactly as the
three hardness gates registered their own blocks while the game went on breaking
everything in one frame (OPEN-31). The only block in `mods/` with boxes was the
chest, and a chest fills nearly all of its cell, so being wrong by its shape is
being wrong by a few centimetres.

`mods/steps` is the other half: `steps:slab`, one box half a cell tall, and
`steps:stair`, two boxes and a front, both made of stone so `mods/tools`' pick
is the tool for them and both makeable -- three stone in a row gives six slabs,
six slabs in the stair's own shape give four stairs. A set piece in the plaza at
x 50, and the stair turned to 0 by the mod rather than by a placement, because a
set piece that came out facing whichever way is a set piece nothing can make a
claim about.

**The gate is the game**: `tests/shape.sh` starts a real server on the real
`mods/` directory and claims positions with a headless body. Standing on the
slab's top, half a node into its cell, is ACCEPTED -- that is the position a
world of cubes refuses. A twentieth of a node lower, inside the box, is still
refused with "that position is inside a solid block". And the stair's two steps
are the same cell at the same height, one node apart in z: the low one accepted,
the high one refused, which is a pair nothing that reads cells can tell apart.
Four sabotages, all red on their own check: the slab declaring no box, the stair
losing its high step, the stair set down turned, and nothing of the mod's
reaching the world.

**And the outline is around the boxes, same day.** The selection shell was drawn
around the cell, so aiming at a slab drew a box a whole node tall whose top half
enclosed nothing -- the game telling a player they are pointing at something they
are not. It now draws one shell per box, from the same `node_shape.h`.

Gated in `parts_draw` (gpu), where the difference a target makes to a frame IS
the outline, because the block is drawn either way. Measured: 5729 pixels, rows
460..685, around a cube; 2814 pixels, rows 498..653, around the boxes in the same
cell -- it starts at the top of the SHAPE and not at the top of the cell. A
quarter turn moves the shape 58.5 pixels across the screen and its outline 59.5.

**A check that could not fail, and how it was found.** The turn was first checked
by asking whether the outline's box of pixels was in different columns after a
quarter turn. A sabotage that drew the outline unturned PASSED it: the block
turns in both frames, so the pixels the outline covers move a little whatever the
outline does. It is a number against a number now -- the outline must move at
least half as far as the shape -- and the same sabotage moves it 20.0 pixels
against 58.5 and goes red. Two sabotages here, both red: the outline back around
the cell (three checks) and the outline ignoring the cell's turn (one).

**And a body walks onto one, same day.** A shape half a node tall is below what
a jump clears, so a slab was a wall to be hopped and a stair was a row of them --
which is not what a stair is. `kStepHeight` is 0.55 and it belongs to the
RESOLVER rather than to the shapes: what it reads is how high the obstacle is,
and a cube is an obstacle a node high whatever its boxes say. The original engine calls it
`stepheight`.

A blocked horizontal step is tried again a step height up and the body is set
back DOWN onto whatever is under it, by halving the drop -- so one step onto a
slab whose top is 4.5 leaves the feet at 4.501 rather than floating at 4.55 and
settling over the next few frames. Only while standing on something, because a
body that climbs half a node at a time in mid air is a body climbing a wall. And
never through a ceiling: BOTH the rise and the far side are checked, in that
order.

Five sabotages, each red on its own check: nothing stepping up at all, a step
that clears a whole node, a body climbing in mid air, the headroom never asked,
and the body left a step above the surface.

**The headroom check needed a second case to be gated at all.** The first
ceiling case put a block directly over the slab, and removing the rise check
changed nothing there -- the far side was blocked anyway, so the rule was proved
by the wrong half of itself. The case that catches it is a ceiling over the
BODY with the far side clear, where a step that only asked about the far side
carries the body through the ceiling's edge. Its control is the same step with
the ceiling taken away, which must be taken, or the check is a body that could
not move for some other reason. That control caught a third mistake on the way:
the first version of the case stood the body inside the wall this gate builds at
z = 14, and a body embedded in stone does not move whatever the rule says.

**And the shape TRAVELS, same day -- which is what the two commits above were
missing and neither could see.** The block table on the wire is names: a client
rebuilds each definition out of its own build, and a name it has never heard of
becomes a magenta solid cube. So every shape in this entry was a shape only a
client running the mod had. A player joining a server with `mods/steps` on it
saw a cube where the slab is, and their own collision -- which is what resolves
their movement before anything is sent -- refused to put them on top of it. The
server would have accepted them there. They never asked.

Found by pointing the gate at it: the probe in `tests/shape.sh` runs no mods, so
asking what ITS table says about the slab is asking what travelled. It said zero
boxes, no turn, and "on the slab blocked, in the slab blocked".

**Protocol 27.** The welcome carries a shape beside the name, for the blocks
that have one and for the blocks that TURN -- a facing byte a client cannot read
is a stair drawn and collided with unturned. Nearly every block says nothing:
the game that ships sends **163 bytes for 6 blocks, in a welcome of 1835**.

**The server's shape wins**, including over a name the client knows. Two copies
of one mod that disagree about a box are a player standing where the server says
they cannot, and the world you are in is the one that decides what is in it.

Corners travel as floats and not as a fixed point, which is the one number here
worth defending: the collision skin is a ten-thousandth of a node, so rounding a
box to hundredths would put the two ends of the wire a hundred times further
apart than the tolerance they judge a position by. Caps: 1024 blocks with a
shape, 16 boxes each, and a box's part name at 32 characters -- about 400 KB at
the ceiling, the same ceiling the name table already has.

Five sabotages, each red on its own check: the server sending no shapes, the
server forgetting the turn, the client throwing them away, a box's corners
rounded on the wire, and a shape allowed to name a block the table has not got.
The last is gated in `protocol` rather than in `shape`, because it is a message
a stranger can send and not a thing this server does.

**A defect this turned up on the way**, and the reason the whole welcome failed
to parse rather than one field: the box count was written as a byte and read as
a u16, because `ByteReader::count` reads a u16. A length written one width and
read another does not fail where it is written -- it parses as garbage from
there on, and what the client said was "the welcome does not parse".

**And a block can be drawn as a MODEL, 2026-09-12 -- the other half of M11's
`drawtype nodebox+mesh`.** `NodeDef::model` is an id in the world's model table,
the same table a body's model comes from, and `core.node_model(id, model)` is
what a mod calls. The mesher asks one question -- is this block a cube -- and a
block wearing a model is not one, so its cell is left out of the chunk and handed
over exactly as a block made of boxes is. The renderer draws the model there,
turned about the middle of the cell by the cell's facing byte, with the same
integer rotation the boxes use.

**The model is the picture and the boxes are the block.** A mesh is a thousand
triangles and a collision is a handful of boxes, so nothing asks a model where a
body may go or what a ray found: a block that wants a shape under its model
declares boxes as well, which is what the original engine's `collision_box` beside
`drawtype = mesh` is for. `mods/steps` ships both on `steps:marker`.

**And it ships**, because a mechanism nothing ships with is a mechanism nobody
has run. `mods/steps/marker.gltf` is written by `tools/make_marker_gltf.py`
rather than modelled -- six triangles, its own normals, its buffer embedded as
base64 so the whole model is ONE file that travels as one file. It leans at -Z
on purpose: a shape symmetric about the vertical looks the same whichever way it
is turned, and a gate built on one would call a turn that does nothing a turn
that works.

**Two defects this found in the model pipeline, both of them about a model with
no skeleton -- which every block's model is.** The loader binds an unskinned
vertex to joint 0 with weight 1, and nothing uploaded joint 0, so the vertex
shader read three rows of zeroes and the whole model collapsed to the origin:
every static glTF this engine has ever been handed drew NOTHING, and the only
models it had been handed were rigged characters. `ModelRenderer::draw` uploads
one identity joint when there is no skeleton. The second was in the model this
gate ships: its side faces were wound inwards, so back-face culling hid them --
`tools/make_marker_gltf.py` now orients every face away from the middle, and the
symptom was indistinguishable from the first defect.

Gated in `parts_draw` (gpu): a block wearing a model is drawn at its cell, is
not the cube it replaced, turns with the cell, and -- with the model taken out
of the store, which is a client that has not got the file -- draws NOTHING
there rather than a cube, because a cube is the one thing it is not. In `mesher`
(headless): a model block puts no cube in either stream, hands its cell over
with the turn, and stops being opaque. In `shape` (a real server): the block's
model NAME reached a client that has never loaded that mod -- `steps:marker`,
with a box under it -- and the welcome is 1926 bytes, 208 of them the shapes of
7 blocks.

Four sabotages, each red on its own check: a model block meshed as a cube, the
model drawn unturned, an unskinned model given no joint, and the mod not
declaring the model at all.

**A gate that had to be loosened, said rather than quietly changed.**
`net_client` matched the line "models: 1 named by this world, 1 from it..."
exactly, and a second model arriving correctly turned it red. How many models a
world names is CONTENT; what that gate is about is where they came from. It
reads the four numbers now and asks that all of them came off the wire, none off
this machine, and none were refused.

**What is NOT there yet.**

  - **A model block is lit by nothing.** The model shader has one directional
    light of its own, like a body, so a statue in a cave is as bright as one in
    the sun -- the cell's light is in the hand-over and nothing reads it.
  - **A model is not cut at a portal** and does not animate: both are the
    bodies' limits, inherited whole by making a block wear what a body wears.
  - **Only the shape and the turn travel.** Whether a block is SOLID does not,
    so a mod's non-solid decoration is still a wall to a client that has never
    heard of it, and its hardness, its dig level and its material are still the
    server's alone -- CLOSED-31 said the same about `dig_level` and it is the
    same sentence. The server refuses correctly in every case; what the client
    cannot do is predict the refusal.
  - **A dropped item lands on the cell**, not on the box: `core/ecs/drop.cpp`
    asks the registry whether the cell below is solid.
  - **`selection_box` is not separate from `node_box`.** The original engine lets a block be
    aimed at through a box other than the one it collides with; here there is
    one list and it is all three things.
  - **A box that leaves its cell is drawn and not collided with by the
    neighbour**, because both the collision walk and the raycast ask the cells a
    body or a ray is in. Nothing declares such a box today.

**Milestone.** M11, which asked for it and is cancelled -- see CLOSED-43 and the
note at the top of this file. The work stands: a slab you can stand on is a slab
this game wanted.

---

## OPEN-44 -- a world could not say how fast a body moves

**Not a symptom: the first piece of DESIGN.md M13, and two defects found on the
way to it.** The speeds and the gravity were constants in
`core/physics/movement.h`, read directly by the client and by the validator: one
set of numbers for every world. The worlds behind a portal are meant to be other
games, and a race wants a body faster than anything here flies. The client moved
by its numbers and the server checked by its own, and what kept the two in
agreement was that nobody could change either.

**Done, 2026-09-13: `core.movement(walk, sprint, fly, jump, gravity)`.** A mod
declares it while it loads. `MovementRules` holds the five numbers. The server
puts them in the welcome -- protocol 28, five floats after the shapes, to every
client whether it adopts the tables or not -- and works its validator's limits
out of them with `limits_for`: the fastest the keys push a body, which is the
walk, the sprint, or the flight doubled for sprint. The client adopts them into
the session of the world they belong to, over anything its own mods said, and
`player_update` takes them as an argument. Per session: walking through a portal
into a race is walking into the race's speeds.

**Refused, and said.** A speed that is negative, not a number, or over
`kMaxDeclaredSpeed` = 256 m/s; a gravity that is not a number or points up.
Refused from a mod with a log line naming the reason, and from the wire by the
welcome not parsing, because a welcome is a message a stranger can send. Upward
gravity is refused and strength is not: a fall is capped, a rise is not. A
declaration after the mod directory has loaded -- from a tick -- is refused as
too late, at the moment a mapgen stage is; not at the moment the socket opens,
as it is for a channel, because an offline run never opens one. Two mods that
both declare: the last loaded wins and the log names whose numbers it replaced.

**The first defect: flying at a sprint was a cheat.**
`MovementLimits::max_horizontal` was `kFlySpeed`, 14, and the client doubles that
while sprint is held. The check is a budget, so it hid: 28 m/s against a ceiling
of 22.4 spends a full budget in four seconds. Measured before the fix, an
unmodified client holding two keys for eight seconds of claims: **17 of 160
refused, the first after 3.80 s**; after it, 0. The cap is
`kFlySpeed * kFlySprintFactor`, and the factor lives beside the speeds instead of
as a literal in the client.

`net_loopback`'s grace check went red on that fix, which is it doing its job: it
carried a fixed thirty blocks, past a ceiling of 22.4 and a legal half second
under 44.8. It derives the distance from the limits now, ten blocks past the
most a budget can hold.

**The second: a step longer than the body is wide went through walls.**
`move_box` tests where a step ENDS, not what it crosses, so a box six tenths wide
misses a wall a node thick once one step is longer than 1.6 nodes -- 32 m/s over
the client's longest update of 0.05 s. Measured: a step of three nodes from
x = 9.5 ends at 12.5, on the far side of the wall, and a step of one node goes
through a pane a sixteenth thick. Nothing that shipped moved a body that fast
sideways; a declared speed does. `move_body` walks a delta in steps of at most
`kMaxStep` = 0.5 node, which cannot clear any box however thin, and at most
`kMaxSteps` = 64 of them; `player_update` calls it.

Its limit, said here rather than found in play: a blocked step is refused whole,
so a body at speed stops up to half a node short of what it runs into --
**10.50 against a face at 10.70**. The resolver always did that, by a walking
step of about a tenth of a node.

**And a fall is capped on the client.** `kTerminalFallSpeed` said "nothing in the
client accelerates past it" and nothing clamped anything: it held because a
world 64 tall at -25 m/s^2 tops out at 56.6 m/s. A world with its own gravity
does not, and neither does a floor portal under a ceiling portal. Measured: two
seconds under -1000 m/s^2, never faster than 60.000 m/s.

**Gated in five places.**

  - **`player_rules`** (new, headless, with `player.cpp` compiled in). Half a
    second on foot: 2.400 nodes by the engine's numbers, 20.000 walking and
    30.000 sprinting by a race's (walk 40, sprint 60). Flying at a sprint with
    fly 10: 10.000. 0.2 s of fall: -5.000 against -10.000 at twice the gravity. A
    jump of 20 leaves at 20.000. The terminal cap. And 200 m/s into a wall stops
    at z = 41.500, in front of its face at 41.
  - **`physics_box`**: the defect measured and checked to be STILL there in
    `move_box` -- a gate that stopped seeing it would prove nothing with its next
    line -- and `move_body` stopping in front of the wall and of the pane.
  - **`chunk_codec`**: the fly-sprint stream; `limits_for(MovementRules{})` equal
    to the defaults exactly; four seconds at 60 m/s refused **0 of 80** where the
    world declared a sprint of 60 and **7** where it did not; at 120 m/s, **1** --
    a budget is generous, and that 1 is the measurement, not a threshold.
  - **`protocol`**: the five numbers round trip unrounded, and a welcome with a
    NaN speed, a negative one, one over 256, or gravity upwards is refused.
  - **`movement`** (new, a real server). `tests/mods_movement/race` is refused
    gravity upwards and then taken at walk 12, sprint 60, fly 20, jump 10,
    gravity -40; `late` asks from a tick and is refused as too late. A client
    with no mods is told the race's numbers, and a session bound to tables of
    its own adopts them. Claims at 60 m/s for five seconds: **0 refused of 98**
    there. At 150 m/s: **21 of 98**. A server with `--no-mods` tells the
    engine's numbers and refuses the same 60 m/s claims **14 of 99**.

**Sabotages, each alone, each red on its own check.** Through the real server
(`movement`): the server keeping the engine's limits (claims at 60 m/s refused
13 of 99 in the world that declared them); the server taking any numbers a mod
declares (gravity upwards answered 1); the server never calling a declaration
too late (the tick's answered 1); the client not adopting the welcome's numbers
(the bound session still holds its walk of -1); and the welcome sending the
engine's numbers instead of the world's (told and adopted 4.80, 8.00, 14.00 --
and in `protocol`, the round trip and all four refusals). Headless: the
validator's cap back to the flying speed alone (the fly-sprint stream, and the
defaults no longer equal to `limits_for(MovementRules{})`); `limits_for`
ignoring the declared speeds (60 m/s refused where the world declared it); the
decode not checking the numbers (all four refusals); `move_body` taking one step
(the wall and the pane); no terminal cap on the client; walking at the engine's
walk whatever the world said; and `player_update` walking with `move_box`.

**That last one stayed green at first, and the gate was wrong, not the rule.**
The 200 m/s case started at z = 60, where the second ten-node step lands INSIDE
the wall and is refused whether the steps are walked or not -- so the body
stopped at 50 either way and the check passed over nothing. It starts at 51.5
now: one step lands at 41.5, just short, and the next at 31.5, wholly past. The
case is a wall between two landing points, and with `move_box` back the body is
through it.

**A sabotage run that could not fail, and how it was found.** The first run of
the script that applies these reported six of the unit sabotages GREEN -- not
caught. The binaries live under a path with spaces in it and the script did not
quote them, so the test never started, printed no FAIL, and "no FAIL" read as
"not caught". What gave it away: two of the six undo fixes whose absence the
same checks had measured red earlier that day (the fly-sprint cap, and the
single step through a wall), and a check does not stop seeing a defect because
a script put it back. The script now refuses to give a
verdict on a run that printed no `ok` line.

**What is NOT there yet.**

  - **Per body.** The rules are the world's. A mod cannot make one body faster
    than another, and a vehicle is a body that is not a player on foot; DESIGN.md
    M13 point 2 asks for both.
  - **The server checks the end of a claim, not its path.** A claim is judged
    against a budget and against the solids where it ENDS, so a claim whose line
    crosses a wall is not refused for the wall. That was true before; a world that
    declares 256 m/s makes the gap between two claims 12.8 nodes wide.
  - **Streaming at speed is not measured.** `PlayerInterest` sends four chunks of
    radius and 128 KB a tick, and whether a body at a race's speed outruns it is
    a number nobody has taken.
  - **The client's copy of `core.movement` is read and not gated** -- the
    single-process loop runs mods too -- the limitation `core.register_mapgen`'s
    client copy has.
  - **No graphical run is gated at a declared speed.** The scenes pass the
    session's rules to `player_update` (35 call sites, each taking
    `s.act().movement`), and no gpu check asks whether a window moves by them.
  - **A connected client whose own mods declare movement** has its numbers
    replaced by the welcome's, and nothing tells that mod.

**Milestone.** M13, point 2 of DESIGN.md.

---

## OPEN-45 -- at 256 m/s a body outruns the ground the server streams to it

**Not a symptom: a measurement taken before anything was changed, because
OPEN-44 let a world declare speeds nothing had been asked about.** A server sends
each player the chunks within `PlayerInterest`'s radius -- four chunks, 128
nodes, two layers up and down -- nearest first with the ones ahead preferred, at
most four chunks and 128 KB a tick. Four a tick is eighty a second. Whether that
keeps up with a race had never been measured.

**How it is measured.** `cnp_net_loopback_test --streaming`: a fresh client
joins a server on a world 512 wide whose mod (`tests/mods_streaming/fast`)
declares a sprint of 256 m/s, so no claim is refused and the server's idea of
where the body is follows it. It stands at x = 16 for three seconds, then makes
one pass to x = 496 in the air at y = 60, a claim every twentieth of a second.
After each claim: how many nodes of GROUND -- the bottom layer of chunks, which
every generated column has -- lie ahead on the path in the client's world, and
whether the chunk under the body is there at all.

**The first run measured the wrong thing, and said so by being too tidy.** The
lowest ground ahead came out at 16 nodes at every speed: that was the last
claim, 16 nodes from the edge of the world. Samples where the answer is the
world ending are left out of the lowest now.

**Measured, 2026-09-13:**

    speed      chunks in the pass   ground ahead, lowest   median          own chunk missing
     30 m/s    198                  127 nodes (4.22 s)     141 (4.71 s)    0
     60 m/s    198                  125 nodes (2.09 s)     139 (2.32 s)    0
    120 m/s    198                  121 nodes (1.01 s)     137 (1.14 s)    0
    256 m/s    144                   64 nodes (0.25 s)      95 (0.37 s)    0

Up to 120 m/s the streaming keeps up and what limits a player is the RADIUS:
128 nodes is a second of warning at 120 m/s. At 256 m/s the budget does: the
leading edge of the interest is nine columns of two layers a chunk crossed, and
eight chunks a second crossed is 144 chunks a second owed against 80 sent -- the
pass ended with 144 of the 198 it had crossed on the client, and a quarter of a
second of ground ahead at the lowest. The chunk under the body was never missing,
at any speed, because the chunk a body is in always goes first.

**Gated in `streaming`** on the real server: at 60 and 120 m/s no claim refused,
the ground under the body always there, and never less than 96 nodes of it ahead
(the fourth chunk ahead may be in flight; the third may not) -- measured 124 and
121. At 256 m/s the ground under the body always there, and the ground ahead
PRINTED rather than checked: that number is this entry, and a threshold under it
would be a number picked to stay green over a limit (in the gate's run, 60 nodes
at the lowest, 97 median).

**Two sabotages, both red.** One chunk a tick instead of four: 79 nodes ahead at
the lowest at 60 m/s, 18 at 120, and at 256 m/s the body over ground that had not
arrived 9 times. The server's interest not following an accepted claim: the
ground under the body missing 157 times at 60 m/s, 79 at 120, 37 at 256, and 0
nodes ahead -- the gate cannot be passed by a server that only streams the start.

**What is NOT done, and the two ways it could go.** Nothing about the streaming
was changed. A race that wants more than a second of warning at 120 m/s wants a
radius that grows with speed, or an interest centre led ahead of the body by its
velocity -- which spends the same budget further forward instead of beside and
behind. A race at 256 m/s wants a bigger budget as well, and the budget is the
number that protects everybody else on the link. None of the three is worth
choosing before a track exists to say how far ahead a player needs to see.

**Milestone.** M13.

---

## OPEN-46 -- the server judged where a claim ended, not what it went through

**Not a symptom: a limit OPEN-44 made wider.** `check_move` refused a claim that
was too far for the budget or that ended inside a solid. A claim on the far side
of a wall, with both ends in the open and a distance the budget allowed, was
accepted. On foot the budget kept that to a fraction of a node; a world that
declares 256 m/s makes it 12.8 nodes a claim, which is most walls.

**Done, 2026-09-13: the server walks the claim.** Not along a straight line: that
refuses honest players, because a body rounding the end of a wall between two
claims, or stepping up a stair, goes where the straight line between its two
positions does not. The server runs the client's own resolver, `move_body`,
from the last accepted position five ways -- the delta as one move; x first and
then z; z first and then x; across and then down; up and then across -- and
accepts the claim if any of them arrives.

**"Arrives" is within `kMaxStep` on every axis, and that is derived, not
picked.** `move_body` refuses a blocked step whole and so stops up to half a node
short of what it runs into (OPEN-44); and a body on the far side of any box,
however thin, is at least six tenths -- its width -- from where a resolver on the
near side stops. So half a node of slack cannot hide a wall. It also means a
claim no longer than half a node on every axis is not walked at all, since it
arrives standing still: that is every claim of a player on foot, and the reason
this cannot correct anybody who walks.

**The last two ways were found by the suite, not by thinking.** The first
version walked three ways, all of which move vertically in the same steps as the
horizontal or before it. `four_clients`' walker steps off dirt columns: a quarter
of a node sideways and a whole node down in one claim, and every walk tried to
drop while its box was still over the column. Across-then-down finds it, and
up-then-across is the same case climbing.

**And a body the server has just placed is not walked from.**
`MovementState::placed` is set wherever the server puts a body itself -- a
crossing's arrival, a window becoming a player, a teleport, a respawn -- and
cleared by the first accepted claim. That claim is judged by where it ends and
by its budget, which was every claim's rule before this entry; the walking starts
from the next one. A client may not know the spot the server chose, and its
first claim is wherever it thinks it is. The price, said: in that one claim a
modified client can still cross a wall inside its budget.

**Three gates were teleporting, and were fixed rather than the rule loosened.**
Every one of them passed while a claim was judged by its end, and every one was
a body going where no body goes:

  - `four_clients`' walker made its first claim ON its circle, twelve nodes from
    where it had just stood, across a dirt column: "32.00 22.00 32.00 to 44.00
    23.00 32.00", four players, one correction each. It walks out to the circle
    first now, at walking pace.
  - `net_loopback`'s walk away from a chest began with a claim two and a half
    nodes up through the ground (34.50 19.50 30.50 to 32.40 22.00 32.00). The
    server puts the body at the walk's start first; walking away is what that
    check is about, and still is. The walk away from the table the same.
  - And both walks went THROUGH a dirt column near x = 42 on their line: with
    ends judged, the claims inside it were refused and the ones past it taken.
    The body stood at 41.60 with 96 corrections and every check after it failed.
    The test clears its walk's own ground now.

That is what the refusal's sentence carrying both positions is for: all three
were read off it.

**Left alone where the resolver does not know the world:** a claim whose sweep
comes near a node holding a floor portal up is judged by its end, as before --
the body is allowed to sink into that node and the resolver would call it a wall.
And a claim inside the server's own grace window is not walked, because the
server moved that body itself.

**The limit, stated.** A path that turns back on an axis between two claims --
out along z round the far end of a wall, across, and back -- is not one the five
walks find, and is refused as though it had gone through. Nothing on foot turns
back within a twentieth of a second; a body at 160 m/s swerving round an obstacle
in one claim can, and would be corrected.

**Found on the way: a test that did not own its ground.** The unit case "round
the end of a wall" failed, and it was the test that was wrong. The plaza is flat
but not empty -- the scatter stands a column of dirt on about one column in
eighty, 282 cells of them between y 20 and 24 -- and the wall had been put beside
one at (40, 20..21, 35). The body could not have gone round, and the rule said
so. The case clears its own ground now.

**Gated.** In `chunk_codec`, on a wall a node thick and four tall: eight nodes
straight across it in one claim refused; the same with the server's grace
accepted; round the end of it in one claim accepted, though the straight line
goes through it; up over a wall one node high and on across accepted; and the
first claim again with the wall taken away accepted, so what is refused is the
wall and not the distance. In `movement`, on a real server: eight nodes across
a wall the race's mod stands in the air refused ("went through something solid"),
the same eight beside it accepted.

**First four sabotages; three red, one green, and the green is said.** No path check at
all: red in `chunk_codec` and on the real server ("through the wall: accepted").
Without the x-first way round: red on the corner case. Without the z-first way:
red on the case that rises over the low wall. Each walk is therefore the only
one that finds its case.

Arriving within the skin (0.05) instead of within a step: **green, not caught.**
No case here needs the half node, and it is not for want of looking: a legal
claim is never past what a straight walk to it runs into, so one of the three
walks reaches every honest claim these checks make EXACTLY, and the resolver's
stopping short only happens to a walk that overshoots an obstacle. The slack is
kept because the derivation above says a wall cannot hide in it, and because
what the server walks from is a position a client reached in steps of its own
size, not the server's. It is written here because a guard nothing can fail is a
guard nobody will notice going.

**Three more sabotages, for what the suite added.** Without across-then-down:
red on the unit case of stepping off a block, and in `four_clients`, all four
players corrected again. Walking the first claim after a placement: red on its
unit case -- and NOT in `four_clients` any more, because its walker now walks out
to its circle instead of jumping there, which is the fix and also the reason
that gate no longer watches the mark. Without up-then-across: **green, not
caught.** Rising over the low wall is found by the z-first walk, which carries
the vertical with it, and no case here climbs diagonally onto something. It is
kept for the symmetry with across-then-down, and said here, beside the
half-node slack, as the second guard in this entry nothing exercises.

**The whole suite, after all of it: 80 tests, 80 green** (646.6 s), including
`four_clients`, `net_loopback`, `bad_link`, `long_game` and `soak` -- the gates
that exist to say an honest player is never corrected.

**Milestone.** M13.

---

## OPEN-47 -- every body in a world moved by the world's numbers

**Not a symptom: the second half of DESIGN.md M13 point 2.** OPEN-44 let a world
declare how its bodies move, and every body in it moved by those numbers. A
vehicle, a power-up or a slowed prisoner is ONE body different from the rest,
and there was no way to say so.

**Done, 2026-09-13: `core.body_movement(player, walk, sprint, fly, jump,
gravity)`.** Any time, unlike `core.movement`: it is one body and not the
welcome. The server's `Session` holds the body's own `MovementRules`, copied
from the world's at the join, and the validator checks each claim against
`limits_of` that session -- the world's tolerance, burst, grace and reach, with
the speeds worked out from this body's numbers. The body is told in a message of
its own: `BodyMovement`, type 37, protocol 29, reliable, the same five floats as
the welcome's and refused on the same terms on the way in. The client writes them
where `use()` put the world's movement, over them.

**The second after a change.** The client goes on moving by the old numbers
until the message lands, so for `portal_grace` -- one second, and the same second
for the same reason: the server changed something about a body its client has
not been told about -- the body is checked against whichever of the old and the
new are faster, speed by speed. A second change inside the window keeps the
widest of all three. The price, said: a body a mod slows down has a second in
which it may still move at its old speed.

**Refused, and said:** the numbers `core.movement` refuses, a player who is not
in this world, and somebody with no body. The defaults are the ENGINE's numbers,
not the world's, which the API documentation says: a mod putting a body back to
what the world declared passes those.

**Measured, in `movement` on a real server.** The world declares a sprint of 60.
The race's mod gives one body a sprint of 150 when asked on its channel, and the
body is told: `given walk 12.00 sprint 150.00 fly 20.00 jump 10.00 gravity
-40.00`. Claims at 200 m/s for three seconds: **0 refused of 59** -- the same world
refuses a body nobody gave anything at 150 m/s, 23 or so of 98. Slowed back to
the engine's numbers and told; claims at 200 m/s at once, inside the second:
**0 of 12**; a second later, for three seconds: **43 of 59**. Numbers for player
99999: refused,
"there is no player 99999 with a body in this world". In `protocol`: the message
round trips unrounded, and all four spoilt numbers the welcome refuses are
refused here too.

**A gate that was wrong on its first run, said.** The check that the body was
given its numbers grepped for lines starting "given " and took the last, which
was "given back ..." -- so it compared the slowed numbers with the fast ones and
went red over a body that had been given exactly what it was asked. It greps
"given walk " now.

**A gate that could not tell a body's numbers from the world's, found by a
sabotage.** The body first claimed 140 m/s for two seconds. With every body
checked against the WORLD's limits instead of its own, the check that those
claims were accepted stayed green: the world's sprint of 60 is a budget of 96
nodes a second with a second of it in hand, and 140 m/s for two seconds fits
inside that. Only the slowed check went red. The probe claims 200 m/s for three
seconds now, which the world's budget refuses inside the first second and the
body's sprint of 150 (240 a second) does not.

**Five sabotages, each red, on the corrected gate.** Every body checked against
the world's limits: the fast claims refused, 23 of 58, and the claims just after
slowing, 4 of 12. The body never told: "asked fast: NOT told", and the client
still holding the world's sprint of 60. The client not adopting the message:
told, and still holding 60 -- the message arrived and went nowhere. No second
after a change: the claims already under way refused, 4 of 12. The host taking
numbers for a player who is not here: "nobody answered 1".

**The whole suite: 80 tests, 80 green** (649.8 s), run before the gate was
corrected; the correction changes the probe and the gate's script and nothing
the suite runs besides `movement`, which is green on its own afterwards.

**What is NOT there yet.**

  - **A vehicle.** A body with other numbers is still a player on foot: no
    steering, no inertia, no seats, nothing that is not a walking body going
    faster.
  - **A body's numbers do not cross a portal.** The next world's welcome says how
    bodies move there, and a mod in the world left behind has no say over it. Nor
    do they survive a reconnect, and they are not saved.
  - **The client's single-process copy** of `core.body_movement` ignores `player`
    -- there is one body in that process -- and is read and not gated, like the
    other client copies.

**Milestone.** M13.

---

## OPEN-15 closed, 2026-09-13 -- the server casts the ray

What OPEN-15 above says was missing is in: `ServerSessions::strike`, after the
reach check, casts a voxel ray from the eye of the position the server accepted
towards the middle of the named body, finds where the ray enters that body's box
(`ray_hits_box`, the mob's box or a player's), and refuses the swing when
`raycast` meets a solid block before that point: "there is something solid
between you and that, N nodes out".

**Towards the middle, because a Hit carries a target and no aim point.** A server
that took the aim point from the client would be taking exactly what this rule is
about. The price, said: a body whose middle is hidden and whose head shows over a
wall is refused.

**Gated in `net_loopback`**, in the zoo: a second body standing still four nodes
the other side of the player, a wall of stone between the two, within reach and
named correctly -- the swing takes nothing and is refused for the wall; the wall
taken away, the same swing lands.

**The first version of the gate could not see its own reason.** It swung from the
zoo's player, which had already been refused several times, and the server's log
keeps a session's first refusal and then one in a hundred -- so the swing was
refused for the wall exactly as it should have been, and the check that the log
said why went red. It swings from a player who has not been refused anything yet.

**Not measured, and said:** what the cast costs. One voxel raycast per swing that
passes every other check, at most one a player every 0.4 s, at a reach of 7.

**Sabotaged, red:** the server casting no ray at a strike -- the swing at the body
behind the wall takes a point, and the log has nothing about a wall.

---

## OPEN-20 closed, 2026-09-13 -- a mod's entity others can see

`core.ecs.visible(id, on)` gives an entity a mod spawned a `Body`, a `Look` and a
`Thing` tag, so the streaming query that already sends people, creatures and
stacks sends it too, with no change to that query; off takes them away. It
travels as a kind of its own -- `EntityKind::Thing`, protocol 30 -- and the
client draws it as a box of its own colour rather than guessing that it is a
person or a creature.

**Only a mod's own.** An entity that already has a body the engine gave it -- a
player, a mob, a stack -- is refused both ways, so a mod cannot hide a player or
give one a second body. That guard is not gated: from Lua a mod has no way to
learn a player's entity id, so nothing today can reach it, and it is written
here rather than left to be found.

**Gated in `sentry`.** The mod makes three signs visible once a player is there
-- signs and not pellets, because pellets fly and die and a count of them would
be a count of the clock -- and a client with no mods is sent all three, as
things; asked on the mod's channel to hide one, it does, and after the client's
five seconds of forgetting two are left.

**Two things that were wrong in the gate before the engine was.** The sign's
field was first called `n`, which is one of the names a system's batch already
uses: the mod was refused at load, and every check in `sentry` went red with it.
And the count was read with `[0-9]*`, which also matched "things in sight after
hiding 2" -- zero digits and then the rest of the line -- and read the word
"sight" as the number.

**What is NOT there yet:** a thing has no health, so a swing at one is refused as
"that cannot be hurt"; it collides with nothing; a mod cannot dress it in a model
from Lua; and hiding one reaches a client that was watching it only after that
client's five seconds of silence.

**Sabotaged, red:** `visible` giving a body and no `Look`, which the streaming
query needs, so nothing arrives -- 0 things seen; and a thing streamed without its
kind, so it arrives as a person -- 0 things seen again.

**And a sabotage that found the gate short.** Hiding a thing by taking away its
tag and leaving its body stayed GREEN: the sign went on being sent, as a person,
and a count of THINGS still went from three to two. The probe counts every body
it is sent as well now, and the gate asks for one fewer after the hiding.
With that check, the same sabotage is red: "bodies in sight went from 3 to 3 --
the hidden sign is still being sent".

---

## OPEN-48 -- a body was named on the wire by half its id

**Found by OPEN-15's gate, not looked for.** That gate destroys a body it spawned.
Afterwards a swing at the stack a creature left was refused as "there is nothing
with that id", from a player who had never been refused anything. Measured:
**the stack's id on the wire was 492, the entity alive as sent was not, and the
living one was 4294967788 -- index 492, generation 1.**

An entity id in flecs is an index and a generation, and the generation is the top
half. `EntityState::id` and `Hit::target` were 32 bits. So the first time anything
died and its index was reused -- a creature killed, a stack picked up -- the id a
client was sent named the dead body and not the living one, and a client aiming at
the living one sent back a number the server could not find. Nothing had noticed,
because nothing had struck a body standing on a reused index until a gate
destroyed one first.

**Fixed, 2026-09-13: the whole id, both ways.** `EntityState::id` and
`Hit::target` are 64 bits (protocol 31); the server streams `e.id()` and not half
of it, and `strike` and `driver_of` take the whole id; the client's `RemotePlayer`,
`aimed_body` and `send_hit` carry it. What it costs: a Hit is 9 bytes and not 5,
and every body in an entity message 4 more, which is 256 bytes at the most a
message carries. One more truncation was in the loopback gate itself, in the
check that nobody can strike themselves, which would have passed on a recycled
avatar for the wrong reason.

**The other road, not taken, and why.** The server could have looked a 32-bit id
up by index and taken whatever is alive there. A swing at a body that had just
died would then land on whatever took its index, which is worse than a refusal.

**Gated:** `protocol` round trips 4294967788 in an entity message and in a Hit;
`net_loopback`'s stack check is the reused index itself and is green again.

**Sabotaged, red:** a body streamed by half its id again -- "the player is sent
the other player" and "the watcher sees the people who are in there" go red with
it, as well as the stack: once ids are 64 bits, a client comparing a streamed
32-bit id with a whole one finds nobody.

**The whole suite: 80 tests, 79 green** (653.8 s). The red was `bad_link`'s
"standing still was never corrected", OPEN-9's face -- 4 corrections, the client
leaving at the full 40 s. Alone, and then against the
commit before this work built in a worktree: 3 red of 5 before, 1 of 5 after,
the same faces on both -- see OPEN-9.

**Milestone.** M13.

---

## OPEN-49 -- a world's code could not reach a player's machine

**Not a symptom: DESIGN.md §5.6's decision, built.** A world may send Luau to a
client, and the client may refuse it (2026-09-13). Before this, a connected
client ran no mods at all: the host table and the mod directory were behind
`if (!networked)`, with a comment saying that a client-side mod API -- one that
draws and listens and does not write -- was a separate thing nobody had built.

**Done, 2026-09-13.**

  - **Where a mod runs** is `side = server | client | both` in its mod.conf,
    `server` when it says nothing. A `ModHost` knows its own side: a server
    refuses to run a client's mod, and a client running a world's code refuses a
    server's. `core.side()` tells a `both` mod which half it is.
  - **The offer.** A server packs every client or both mod whole, in the
    `.cnpmod` format, and adds it to its media as `worldcode.<mod>.cnpmod` -- a
    media file like any picture, at most 1 MB like any other.
  - **The loader** is `src/script/world_code`, one function the client and the
    headless probe both call: the offered names that are code, the bytes
    fetched, the package verified and unpacked into the media cache, the mod
    loaded. So the gate runs the loader that ships.
  - **The sandbox.** `world_code_host()` takes a full host table and removes
    everything that writes the world or registers content -- blocks, facing,
    portals, nodes, shapes, models, items, recipes, stages, modifiers, bags,
    chests, panels, drops, health and movement. What is left reads, draws,
    listens, and talks on the mod's own channel. A call to something removed does
    nothing, or raises a Lua error saying why. `set_world_code(true)` grants no
    capability of any kind -- a manifest declaring even `os` is refused, the
    capability named -- and holds memory and interrupt budgets at the defaults.
  - **On the client** the code loads into the ModHost of the world the player is
    in, once, when that world's media are complete, and is stepped every frame.
    It dies with the session: a crossing is another world with its own code. The
    host table every mod in the process gets moved into `make_client_host()`.
  - **The refusal** is `--world-code refuse`, or the world's signing key in hex
    on a line of `world_code_refused.txt` in the media cache. A refused world is
    still played; nothing of its code is unpacked on the machine.

**Found on the way: one name refuses a whole offer.** The first media name was
`worldcode/hud.cnpmod`. A media offer with a slash in ANY name is refused by the
client's parser, whole -- textures, models and all -- so the probe was offered
nothing, and every check after the offer was red for it. The prefix is
`worldcode.` now, the way a model's files are named. The parser's rule is right;
what is worth saying is its reach: a server that ever puts one bad name in its
media stops every client from drawing its world.

**Gated in `worldcode`**, a real server on `tests/mods_worldcode` and a client
with no mods: the server offers its two client mods and not `keeper`, a server
mod with no side; it runs `hud` (both) and not `greedy` (client). A player who
allows world code is offered two and runs one: `hud` runs knowing it is the
client, `greedy` is refused for declaring `os`, the block `hud` asked to write
was not written, its hello reached its server half and the answer came back. A
player who refuses joins anyway, runs none of it, and the server hears world
code exactly once, from the player who allowed it.

**Sabotages, each red on its own check.** S32 the server offers no world code:
offered 0, and every check after it red. S33 the client ignores the refusal:
refused 0 running 1, and the server hears world code 2 times. S34 world code
keeps `set_block`: 1 block written. S35 world code gets the capabilities it
declares: running 2, and `greedy` reads the clock of the player's machine. S36
the server runs a client's mod: red on that check alone. S37 the server offers
its own mods too: 3 offered, `keeper` among them.

**The suite:** 81 of 81 green, 667.83 s, with the host table moved out of the
client's main and the texture and model draws that go through it among them.

**What is NOT there yet.**

  - **A client-only mod cannot talk to the server.** A channel is a mod's name,
    handed out to the mods a server runs; a mod that only runs on clients has
    nobody on the other end, and needs a `both` half.
  - **No HUD and no input for world code.** It can draw boxes in a render pass
    and listen for events; it cannot put text on the screen or ask for a key.
  - **The client's own path is read and not gated.** The gate is the probe, with
    stub functions restricted by the same `world_code_host()`; the client's
    Scene-bound table, restricted the same way, is not run in a window by any
    check.
  - **A world with no signing key** cannot be refused on its own, only by the
    flag, and the list is a file with no screen in front of it.
  - **World code loads once a session.** A world whose media change while a
    player is in it does not send them new code until they come back.

**Milestone.** M13.


## Closed 2026-09-19 -- a saved world did not survive a mod adding a block

Found in play on 2026-09-17: the first `tools/play.sh` after the bucket and the
six water levels shipped refused the world ("world file was made by a build
with a different block table"). The refusal was right -- ids are handed out in
registration order, so a block added to an old mod shifts every id after it --
but there was no way out except `--fresh`.

A world file now has a `blocks` table (id, name), written with the content hash.
When the hash differs, `WorldDb::open` rewrites every stored chunk from the
file's table into the server's by name, then records the new table and hash, all
in one transaction. It refuses, writing nothing, only when a stored cell holds a
block the server has no name for; a mod taken out whose blocks were never placed
costs nothing. No schema bump: an older build can open the file only under the
same table, which it never changes.

Checked in `storage`: a table with a block registered ahead of the core ones
rewrites 5 of the test world's chunks and every cell of 64 x 64 x 64 holds the
same block by name; the file then opens under the new table with nothing to
rewrite; a table without `core:stone` is refused naming it and the file is left
as it was; the original table brings back the same world hash. On a real
server, seed 1, `ore:tin` added ahead of `ore:coal` in a copy of `mods/`: 17 of
32 stored chunks carried over, and back again once it was taken out; with `ore`
and `furnace` removed: refused, naming `ore:coal ore:iron`. The play world in
`worlds/play/` opens as before and gains its table on that first open.

## OPEN-50 closed, 2026-09-19 -- the pause menu forgot its options

Mouse sensitivity and debug info are kept in
`$XDG_CONFIG_HOME/cubes-and-portals/options` (else `~/.config/...`), one
`key value` per line. Both play loops read the file when they start and write
it when they end, in interactive play only, so a scripted run neither reads a
player's settings nor overwrites them. Third person is not kept on purpose: it
belongs to the world (`core.view`), and a crossing back into a world that said
nothing is the eye again. A networked world still does not pause, also on
purpose: the server is not one player's.

Checked with real clients and a scratch config directory. With no file, the
local client writes `look_scale 1`, `debug 1` on exit. With `2.5`/`0` it reads
those values and writes them back unchanged (a failed read would have written
the defaults). A networked client on a local server did the same with `1.7`/`0`.

## OPEN-14 closed, 2026-09-19 -- the client stops timing a crossing

Since 2026-08-23 the client waited for an answer for its own silence window,
max(5 s, 25 x round trip). The request and its answer are both reliable, though,
and the far server's ENet may spend up to 3 x 2 x max(12 s, 25 x round trip)
retransmitting the answer before it drops the peer (Transport::hold_peer): 72 s
on loopback, against a client that gave up after 5 s. The server answers every
ChangeScope it can read, with a Teleport or a ScopeDenied, so a client that gave
up on a yes was left standing in a world the server had just moved it out of.

The wait is gone. A pending crossing ends on the answer, yes or no, or when the
far session stops playing. `kCrossingAnswer`, `ClientSession::crossing_window`
and `discard_teleport` are deleted. What is left: a far server that never
answers a request it could read would keep that crossing pending until its
session ends. That is a server bug, and the client has no way to cross that
mouth meanwhile.

Checked with `tests/bad_link.sh` as it was before the suite was cut (from git,
run by hand). Clean link: the crossing goes through and every check the client
makes passes. At 200 ms +/- 30 each way and 20% loss (a 424 ms round trip, 276
packets dropped on the way to the client), the crossing was answered yes in
2300 ms and every check passed.

Later the same day: a world holding blocks no loaded mod registers opens instead
of being refused. Each such name met in a stored cell is registered as a
placeholder (`NodeRegistry::register_unknown`: magenta, checkered, solid, an
orphan nobody can hold), which is also what `adopt_node_table` now uses on a
client. The server lists them at startup, and the mod coming back makes them
themselves again. `persistence` checks it on the binary: `--no-mods` on a world
made with mods opens it, naming `ore:coal trees:leaves trees:trunk ore:iron ...`,
and the next run with the mods ends with the same world hash (6191bffa79345c5a).
In a frame from a real client, a server without `trees` on a copy of the play
world drew every leaf magenta.

## OPEN-11 and OPEN-18 closed, 2026-09-19 -- what a mod could not say, and what it was not told

OPEN-11. `core.notice(player, text)` puts a sentence on a player's screen, where
the reason for a refused edit goes; player 0 is everybody in the world. On a
server it is a new message, `Notice` (protocol 41, event channel, capped at
512 bytes like `ModData`), queued with what mods say on their own channels. In
the single-process client it writes the screen directly. `mods/guard` now says
why a block placed above y 30 came out as cloud. Checked with a real client on
a real server and a scratch mod that greets every arrival: the client logged
`notice: hello from a mod`, and a frame shows it above the hotbar.

OPEN-18. After a handler returns, the payload table is walked. The first key
that is neither a field nor `cancel` (and `cancel` only on a cancellable event)
is logged once per mod, event and key: `mod 'probe': handler for 'core:tick' set
'cancelled', which the event does not have`. On a cancellable event the line
adds "(to cancel, set 'cancel')".

## Closed 2026-09-19 -- the view on rejoin, a mod's memory, a key for creative

The CLOSED-10 leftover "look direction is not restored". A player's row keeps
where they looked (`look_x`, `look_y`, `look_z`, added to an older file when it
is opened, so no schema bump: an older build names the columns it reads), and
the Teleport that hands their place back already carried a look. Checked on a
copy of the played world: left looking at (-0.87, -0.29, -0.39); rejoined with
the mouse untouched, and the server moved the client there with that look.

A mod's memory. `core.storage_set(key, value)` / `core.storage_get(key)`, per
mod, in the world file's new `mod_store` table, read before `core:world_ready`
and saved with the chunks. Limits: key 1-64 bytes, value up to 4096 bytes, 1024
keys per mod, refused with the reason in the log. Checked with a probe mod over
two runs of one world file: 0 then 1. `games/track` keeps its best laps there,
as "name time name time ...". The server loads a stored row without errors, and
the parser reads it back. A lap was not driven.

C opens `mods/creative`'s list. `core:input` now also hears `key_c`, and
creative is `side = both`: the player's machine hears the key and asks over the
mod's channel, and a process that is its own server opens the list directly.
Checked in Xephyr with keys held 100 ms (a press and release inside one frame
is missed by raylib, a harness problem): E opened the bag and C the list,
three runs out of three.

Later the same day, the three limits that commit left:

- The mod store is saved by the keys that changed (ModStore::take_touched), an
  upsert or a delete each, not rewritten whole; a save that fails owes them
  again. A probe bumping a counter every tick and erasing one key, saved every
  second: the counter went 100 then 300 over runs of 100 and 200 ticks, and the
  erased key was in the file at 100 ticks and gone at 200.
- The single-process client keeps a mod store in memory for as long as it
  runs, as it does node_meta; a world's code on a player's machine gets none.
- games/track keeps each best under `best:<name>`, read when the player
  arrives, and the board under `board`, read at core:world_ready. Driven: a
  scripted lap of 21.084 s on a track server with a world file and a portal
  leading away (without one the scripted client puts an opening under the kart
  and is corrected for the rest of the run), the server stopped and started
  again, and the rejoin logged "racer is back, best 21.084 s".
