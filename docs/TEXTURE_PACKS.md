# Texture packs

*[Leggi in italiano](TEXTURE_PACKS.it.md)*

A pack is **a folder of images** and, when needed, a sheet saying which image each block wears.

```sh
./build/src/client/cnp_client --textures assets/packs/kenney
```

More than one at a time, and **F7** moves to the next while the game runs:

```sh
./build/src/client/cnp_client --textures assets/textures --textures assets/packs/kenney
```

`--no-textures` selects none: every block stays the placeholder the engine bakes from
`colour`/`checker`/`opacity`. It is the state the gate measures everything else against.

---

## Without a sheet

A folder without `pack.txt` is a pack all the same: its files are named like the blocks, with the
colon replaced by an underscore.

    core:stone   ->  core_stone.png
    beacons:beacon -> beacons_beacon.png

That is how `assets/textures` is made, the one the repo ships, and it never needed a sheet. **An
empty alias table must behave like the old rule, not like an error.**

## One face at a time

A block can wear **one** image, or one on top / one below / one at the side, or six different
ones. **Which files exist** decides it, not a declaration:

| file | who wears it |
|---|---|
| `core_stone.png` | all six faces |
| `core_grass_top.png` | the top face (+Y) |
| `core_grass_bottom.png` | the bottom one (−Y) |
| `core_grass_side.png` | the four vertical ones |
| `core_forno_front.png` | +Z |
| `core_forno_back.png` | −Z |
| `core_forno_right.png` | +X |
| `core_forno_left.png` | −X |

Each face looks for **its own file, then its group's, then the block's plain one**. So:

- a pack that adds nothing behaves exactly as before;
- a pack that adds `core_grass_top.png` splits the block in two without anybody having declared
  that grass has a top;
- a face missing both its own file and its group's ends up on the plain file, not on the
  placeholder.

A declaration would be a second place where the truth lives, and the two would disagree the first
time a pack shipped a file the node table knows nothing of.

**One tile per distinct image, not per block.** Two blocks asking for the same file share a tile;
a block with top, bottom and sides costs three. The ceiling is **256**, because the tile's index
is eight bits in the vertex: images that do not fit are counted and told, never wrapped silently.

In the sheet the face is the key's third part:

```ini
core:grass:top    = grass_top.png
core:grass:side   = dirt_grass.png
core:grass:bottom = dirt.png
```

The words allowed are `top`, `bottom`, `side`, `front`, `back`, `left`, `right` and `all` (which is
the plain file). A key is read as a face **only if a block name is still left in front of it**: a
mod registering `mod:top` does not get its block read as the top of a block called `mod`.

## How a texture ends up on a face

A texture has a top and a left; a slice of a chunk does not. The rule, for all six faces:

> **the image's u goes right looking at the face from outside, its v goes down.**

Down and not up, because `v = 0` is the first row in memory, which is the file's top row.

| face | right | down |
|---|---|---|
| +X | −Z | −Y |
| −X | +Z | −Y |
| +Z | +X | −Y |
| −Z | −X | −Y |
| +Y | +X | +Z |
| −Y | +X | −Z |

The first four rows are `cross(forward, up)` with `up` the world's: derived, not chosen. The last
two have no up of their own, and the convention is that both are read with **+X to the right**.

The `pack_draw` gate puts a block alone in the air with a tile of four differently coloured corners
and photographs it from all six faces: red top left, green top right, blue bottom left, yellow
bottom right. Each of the three ways of getting it wrong moves a different pair -- flipped swaps
red and blue, mirrored swaps red and green, turned by 90° sends red to any corner.

---

## Directional blocks

Six different faces are not enough by themselves: a block whose front is always +Z turns its back
on three players in four. A block declared **`rotatable`** is turned when placed, so its front
faces whoever put it down.

```lua
core.register_node("mymod:forno", 150, 110, 60, true, 0, true)
--                                                        ^ rotatable
```

In the pack:

```ini
mymod:forno        = pietra.png
mymod:forno:front  = bocca.png
```

**A block with a front is one block.** The turn is a byte on the **cell** -- `Chunk` keeps a plane
of facings beside the blocks' and the light's, it travels in the same compressed frame and ends up
in the same world file -- and the mesher reads it when deciding which image goes on which face. The
pack knows nothing of it: it paints the faces by name and that is all.

They were **four ids** until recently, one per quarter turn, and the three extra wore the first
one's images permuted. They cost no tiles and no bytes on the wire, and instead cost a rule on
every caller: whoever asked "what block is this" had to ask for the base. Now the id in the world
is the one `register_node` returned, always, and `core.find_nodes` looks for a name and finds the
cells whichever way they face.

The turn is decided from the **position** of whoever places, not from where they are looking: the
server has the first and not the second, and a rule told two ways is two games. The front cannot
face up or down: four turns about the vertical, not twenty-four -- but that is now a limit of what
placing writes, not of the block table, and that is why it was worth changing.

---

## With a sheet: `pack.txt`

It is needed the moment the art is somebody else's. No downloaded pack names its files
`core_stone.png`, and renaming eighty-five files before being able to try them is why nothing gets
tried.

```ini
# comment; ; is fine too
name = Kenney Voxel Pack

core:stone = stone.png
core:grass = grass_top.png
core:dirt  = dirt.png
```

- The key **with a colon** is a block -- or an **item**: the same namespaced name counts, and the
  sheet does not learn that items are not blocks. Without a colon it is a property of the pack:
  today only `name` is read, the others are ignored.
- A block the sheet does not name keeps its own file name. The two rules live together: you can
  name half a pack and let the other half name itself.
- Two blocks can wear the same image.
- A refused line **does not cost the lines after it**.

### The target is a file name, and that is all

No `/`, no `\`, no `..`, no leading dot, and no more than 128 characters. A line breaking the rule
is **counted and dropped**, and the block stays with its own name -- not with half a path.

It is not a convenience. A pack is content, content here also comes **from the wire**, and a sheet
that could name `../../../etc/passwd` is a sheet that reads everything the player can read. The
`texture_pack` gate tries eight different ways of getting out of the folder, one per check,
because a parser that stops at `/` catches four and lets the others through.

---

## Dressing an ITEM

A tool places nothing, so it has no block to borrow an image from: without a line in the sheet it
stays a small grey square in the bag. The line is like all the others.

```ini
tools:pick   = pick_iron.png
tools:shovel = shovel_iron.png
```

The item asks for **one file only**, under its own name (`tools_pick.png` for `tools:pick`): it has
no faces, no top or side, and no chain to fall back along. If nobody drew it, it takes nothing --
and then the bag falls back on the **block the item places**, which is what makes a stack of stone
look like stone without anybody drawing an icon for it.

Items enter the atlas **after** all the blocks, and that is not a detail: wanting an image for an
item must not move a block's tile, because the tiles' numbers are inside vertices already built.

And they share the same budget of 256 and the same deduplication: an item painted with a file a
block already wears **does not cost a second tile**.

## A tile per IMAGE, not per name

If the sheet sends six blocks to the same `flat.png`, the atlas asks for six different names
(`core_stone.png`, `core_grass.png`, …) and receives the same image six times: they cost **one
tile**. The deduplication is on the decoded content, not on the file name, so it also catches two
different files holding the same image.

It matters because the atlas holds **256 tiles** -- a tile's index is eight bits in the vertex --
and thirty-nine blocks by six faces are already 234. Measured: the gate's pack went from six tiles
to one.

---

## The tile size is the pack's

The atlas has **one** tile size, and the pack decides it: the images are decoded **before** the
atlas exists.

- The size most files agree on wins; on a tie, the larger. Not "the first": which file comes first
  is the registration order, so with that rule adding any mod would change the whole world's
  resolution.
- A file at a size the rest of the pack does not use is **skipped, not rescaled**, and its block
  goes back to the placeholder. It is said on the error line.
- Tiles must be square, between 4 and 512 px, and the whole atlas does not go over 4096 px: past
  that, it goes back to 32 px and says so.
- Placeholders are baked at the chosen size too, so a 128 px pack does not have half a world at 32.

Kenney at 128 px over 39 blocks:

```
textures: 'Kenney Voxel Pack' (1 of 1) from assets/packs/kenney -- 19 name(s) mapped, 0 refused
atlas: 8 x 8 tiles at 128px (1024px square), 19 from the pack, 20 generated
```

The twenty generated were the `palette:hue*`, flat colour samples the sheet left as they were on
purpose (they have pictures of their own in `assets/textures` since 2026-10-30).

---

## From the wire

A server can wear a pack (`cnp_server --textures <folder>`) and the sheet is resolved **on the side
that owns the folder**. The name the client asks for stays always the block's, `core_stone.png`:
the protocol does not learn that packs exist.

For the same reason **F7 does not touch a remote world**. A world somebody else is serving is
drawn with what *that* server sent; dressing it with a pack of this machine would be the picture of
the wrong game, and would look right.

### The pack travels as a pack too

Besides the images under the engine's names, a server wearing a pack serves **the pack itself**:
the sheet under `pack.manifest`, and every file the sheet names under `pack.file.<its name>`. The
client rebuilds the folder in the media cache, loads it and puts it in the F7 rotation -- so you can
**keep** a world's art, wear it elsewhere, or take it off. The client says so when it happens:

    pack: this world's 'a pack over the wire' kept -- 1 file(s), 2 name(s), F7 to wear it here or anywhere

The folder is named after the **sheet's content**: the same pack from two worlds is one folder,
and a changed pack is a new folder instead of half an old one. It costs no blob -- media are
addressed by content, so a second name for bytes already there is a row in a table -- and it costs
no protocol version: a world without a pack does not offer that name, and the negotiation ends
there.

A file the sheet names and the server cannot read is counted and is not an error: the server's
atlas falls back on the next name too, so a client without that file behaves the same way. That
equivalence is the promise.

---

## What it does not do yet

- **Only one shipped block is directional.** `chest:box` is `rotatable`, and Kenney's sheet paints
  its front (`chest:box:front = oven.png`). Every other block of the stock game has six equal faces
  or at most a top, a bottom and a side.
- **Only four turns, about the vertical.** A block cannot face up or down. The byte on the cell
  would hold twenty-four: placing is what writes only four.
- **No mipmaps and no linear filter.** The atlas is `GFX_FILTER_NEAREST`, and at 128 px this shows
  in the distance more than it did at 32.
- **No texture animations, no random variants per block.**
- **The stock pack does not dress the tools.** `assets/textures` is this engine's generated art, and
  holds neither `tools_pick.png` nor `tools_shovel.png`: with the default pack the two tools are
  small squares. The Kenney pack dresses them, because its archive's `PNG/Items` folder is drawn at
  128 px like the tiles -- the same size, nothing to resize.
