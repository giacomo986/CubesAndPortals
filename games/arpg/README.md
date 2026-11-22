# The wastes (arpg)

*[Leggi in italiano](README.it.md) · [Leia em português](README.pt.md)*

A point-and-click isometric action RPG: a town, three zones of areas and corridors, the crypt
under the barrows. The mods:

- `arpg_core`: blocks, the player's body, the creatures (the Butcher among them), the town's
  people, what a character wears and holds (`gear_*.gltf`).
- `arpg_game`: the layout, classes, skills, items, merchants, quests, materials, the HUD.
- `arpg_crypt`: the crypt's three floors, the Keeper of the tombs, the way out.
- `arpg_icons`: the inventory's pictures (served as the files of one model, see below).

`tools/arpg.sh --fresh` starts a sandbox and this world on one server; through the stone arch.

## Playing

Left button: walk, strike, talk, pick up. Right button: the skill 1-4 chose. **Q** life potion,
**R** mana potion, **B** town portal, **I** bag, **C** character, **K** skills.

- **The belt**: three slots by the globes, each with its key, how many are left, lit when used;
  the life potion's slot beats when life is under a third. Potions and gold are taken by walking
  over them (or clicking the potion's name); with the belt full (8 of each) a message says so.
- **Items**: each has a painted picture, richer the rarer: normal plain, magic a blue glow, rare
  a yellow glow and a spark, unique a gold frame with red gems, set a green frame.
- **Materials**: iron ore, leather hide, bone, arcane dust, ruby, soul shard, ember essence, drake
  scale. They drop from creatures (each kind its own), barrels, crates and urns, chests, champions
  (rubies), named ones and lords (soul shards; Emberwing's drake scales), are taken by walking
  over them and kept in the pouch under the bag (99 of each). Today quests ask for them;
  crafting, enchanting and upgrading are to come.
- **Merchants** have three tabs: BUY, BUY BACK (the last twelve things sold here, at what was
  paid for them) and QUEST.
- **Quests**: the gate warden, the smith, the jeweller and the alchemist each have a chain, one at
  a time, offered once a character is of its level: kill so many of a kind (or champions, or
  named ones), collect so many of a material (handed over), or the Butcher. A `!` over a giver
  means a quest to take, `?` one to hand in; the quests taken are listed on the left. A quest
  pays gold, experience and an item of at least magic or rare.
- **Outlines**: the creature under the pointer has a red rim (a town's person a pale gold one),
  the one being fought a gold rim that beats: plain what a click will strike and what is being
  struck (`core.outline`, `core.cursor_creature`, an engine feature any world may use).
- **Kill quests** count every name of a kind: a skeleton is a bone archer further on, and the
  quest's panel lists them (COUNTS). A lord already killed when its quest is taken comes back to
  its lair.
- **The Butcher**: the Grey Waste's lord before its lord, in its den (area b1, level 3), with four
  of the dead: huge, quick, a cleaver that hits hard.
- **The crypt**: three floors down stairs (half steps), each sealed until its dead are down. The
  Keeper waits on the third; when it falls the seal east of the floor breaks, and a corridor leads
  to a roofed room where the camera comes down to the eye, with the portal home in its east wall.

## Models and pictures, and how to make them again

All from Meshy (credits of the account in `~/.claude.json`, see `tools/meshy_generate.py`), then
Blender (`flatpak run org.blender.Blender --background --python ...`). The sources are kept in
`assets/meshy/`.

| What | Tool | From |
|---|---|---|
| Inventory pictures (`arpg_icons/icon_*.png`, `icons.gltf`) | `python3 tools/make_arpg_icons.py` | painted sheets `assets/meshy/arpg_icons/*.png` |
| Held weapons, shields, orbs; the Butcher's cleaver | `tools/make_arpg_gear_meshy.py` | `assets/meshy/arpg/sheet_*.glb` (image-to-3D of the sheets) |
| Worn pieces, robot and each species | `python3 tools/make_arpg_armor_strips.py`, then `tools/make_arpg_armor_meshy.py` | `assets/meshy/arpg/<row>.glb`, fitted onto `assets/meshy/arpg/fit_ref/` |
| The zones' chests (shut, open) and the town's stash, as models of their blocks | `tools/make_arpg_chests.py` | `assets/meshy/arpg/chests.glb` (image-to-3D of `chests.png`) |
| The Butcher, the Keeper | `tools/make_meshy_boss.py -- butcher\|custode` | `assets/meshy/<name>/walk.glb`, `run.glb` (Meshy's rig) |

- The pictures are not items: items share the blocks' atlas, whose tile size the pictures vote
  on, and two hundred at 96 px took it over (the world went magenta). They are named by a model
  of one triangle instead and drawn as `model.arpg_icons_all.icon_<name>.png`.
- A held piece is fitted where `tools/make_arpg_gear.py`'s own was (the same box along the held
  axis, the same bone); that script's pieces are the reference now, kept in `fit_ref`.
- A worn piece is stretched onto the old piece's box for that body (a glove or a boot onto its
  side's half), put just outside its surface (a painted piece is a relief, and most of it sank
  into the body), and each vertex takes the bones of the old vertex nearest it.
- The bosses: turned to face +Y and sized (Butcher 2.9 m, Keeper 2.6 m), what they hold placed as
  it should look with the arms hanging, welded, decimated to 6000 triangles, the colour baked to
  512 px; Meshy's walk and run kept, idle, attack (and the Keeper's shoot), hurt and death keyed
  here as turns about the world's axes. A look at every clip: `~/.cache/cnp_look/<name>_clips.png`.
- Each file stays under the 1 MB a media file may weigh; the scripts check.

## Known limits

In [TODO.md](../../TODO.md) (Mods, "Arpg"): the materials only feed quests; textured gear no
longer takes the unique/set trim tint; worn pieces look a little crumpled where wrapped; the
bosses' struck box is their T-pose's; the Butcher's numbers are guesses.
