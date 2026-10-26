--!strict
-- Recipes: what turns some things into another thing.
--
-- Two kinds, and the difference is the whole of the API. A SHAPED recipe is a
-- picture: rows of item names, `-` for a cell that has to be empty, and the
-- picture matches anywhere in the grid but is not its own mirror. A SHAPELESS
-- one is a list: those things, that many cells, arranged however.
--
-- Names and not ids, and that is not a style choice. The item form of a block
-- is minted by the engine AFTER every mod has loaded, so `core.item_id` up here
-- answers 0 for every block in the world -- ids taken at load time would be a
-- table of recipes for nothing. The engine resolves the names when a grid is
-- matched, and says at startup if one of them names something nobody
-- registered.

-- A block of this mod's own, so there is something to make that is not already
-- lying about in the world.
local BRICK = core.register_node("craft:brick", 190, 80, 60, true, 4)

-- Four stone in a square. Written as two rows of two; it matches in any of the
-- four places a two-by-two fits in a three-by-three, because the engine stores
-- a pattern as its own bounding box.
core.recipe_shaped("craft:brick", 4, "core:stone core:stone", "core:stone core:stone")

-- A gap that is part of the shape. The middle cell has to be EMPTY -- a hole is
-- not "anything", it is nothing, and a matcher that ignored it would let this
-- be made with rubbish in the middle.
core.recipe_shaped("core:frame", 1,
    "core:stone core:stone core:stone",
    "core:stone -          core:stone",
    "core:stone core:stone core:stone")

-- And one with no shape at all: a brick and a pane, anywhere in the grid, in
-- either order.
core.recipe_shapeless("core:glass", 2, "craft:brick core:glass")

core.log(`craft: 'craft:brick' at {BRICK}, three recipes -- two shaped, one not`)

-- A crafting table: a block that makes a player's grid three by three.
--
-- Same shape as the chest, and for the same reason: a right click is a USE
-- first and a placement second, so this claims the click with `ev.cancel` --
-- without that, the same click would open the table AND build on top of it.
--
-- The grid is the PLAYER's and not the block's. Two people at one table are two
-- grids, because what is half-arranged in one belongs to whoever arranged it.
local TABLE = core.register_node("craft:table", 150, 105, 55, true, 4, "upright")

-- Drawn as a Blender model, table.gltf (~/kart_assets/chest_furnace.blend,
-- collection CraftTable): a workbench on four legs with a shelf, a 3x3 grid
-- inlaid in its top, and a saw and a hammer hung on its front, which faces
-- whoever put it down. A full cell of box under it, so it is walked into and
-- aimed at like the cube it was.
core.node_model(TABLE, core.register_model("craft:table", "table.gltf"))
-- In the hand and in the bag: the same table, small (tools/make_held_models.py
-- makes it out of table.gltf, and its icon, assets/textures/craft_table.png).
core.register_model("craft:table_held", "table_held.gltf")
core.node_box(TABLE, 0, 0, 0, 1, 1, 1)

local TX, TY, TZ = 39, 20, 47

core.set_block(TX, TY, TZ, TABLE)

core.event.on("core:used", function(ev)
    if ev.id ~= TABLE then
        return
    end

    if core.craft_show(ev.x, ev.y, ev.z, ev.player) == 0 then
        return
    end

    ev.cancel = true
end)

-- And one you can make, out of four bricks. Two by two, so it fits in the grid
-- everybody already carries -- a table you need a table to make is a table
-- nobody ever gets.
core.recipe_shaped("craft:table", 1, "craft:brick craft:brick", "craft:brick craft:brick")

core.log(`craft: 'craft:table' at {TX} {TY} {TZ}, right click it for a 3x3`)
