--!strict
-- A tool, and a block that will not come out without one.
--
-- The engine grew a clock and a level in OPEN-31 and then nothing declared a
-- value for either, which is the state this mod ends: a hardness nobody writes
-- down is a stopwatch nobody starts. The engine's own blocks now take seconds
-- (see core_nodes.cpp), and every one of them still yields to a bare hand --
-- what a LEVEL is for is here, because a tool is content and content is a mod.
--
-- Three things and they are the three halves of the rule:
--
--   GRANITE  a block a bare hand does not get out however long it is held. The
--            refusal names the level it wants, so nobody goes off trying every
--            tool they own.
--   PICK     an item that digs to that level and five times as fast, so the
--            same granite takes under a second instead of four.
--   BENCH    where the first ones come from, because a block that needs a tool
--            in a world with no tool in it is a block nobody can ever break.
--
-- And a fourth thing, which is what makes the other three a game and not a
-- ladder: the SHOVEL. It is a better tool than the pick by every number it has
-- -- level 1, four times a hand -- and it does not open the granite at all,
-- because it is for soil. A level says how good a tool is; a material says what
-- it is good AT, and without the second one every tool is the same tool with a
-- different number on it.

-- Level 1, four seconds, and made of stone. Four is long enough that the bar
-- under the crosshair is worth looking at and short enough that nobody puts the
-- mouse down.
local GRANITE = core.register_node("tools:granite", 122, 118, 132, true, 0, "", 1, 4.0,
                                   "stone")

-- The bench itself is soft, and asks for nothing: whatever went wrong, a player
-- can always take this apart with their hands. Wood, like the frame it is made
-- of, so an axe would be the tool for it if anybody wrote one.
local BENCH = core.register_node("tools:bench", 138, 96, 52, true, 0, "", 0, 1.2, "wood")

-- max_stack 1, places nothing, digs to level 1 at five times a hand's speed,
-- and all of that only against STONE. A tool is the first item in this game
-- that is not the item form of a block: it puts nothing down, and what it is
-- for is the three things on the end.
core.register_item("tools:pick", 1, "", 1, 5.0, "stone", 0, "", 0, 131)

-- The same tool with one word changed, and it is a different tool. Four times a
-- hand through soil -- the grass, dirt, sand and snow the engine ships -- and a
-- bare hand in front of the granite, level 1 or not.
core.register_item("tools:shovel", 1, "", 1, 4.0, "soil", 0, "", 0, 131)

-- And makeable, which is the point of having crafting at all. The shapes are
-- the ones anybody who has played this kind of game already knows: a head
-- across the top and a stick handle under the middle of it.
--
-- The handle used to be two core:frame, which is sixteen stone for the shaft of
-- a shovel. A stick is mods/timber's, out of planks out of a log, so a tool now
-- costs a tree as well as the ground -- which is the chain that game teaches in
-- its first two minutes.
core.recipe_shaped("tools:pick", 1,
    "rocks:cobblestone rocks:cobblestone  rocks:cobblestone",
    "-            timber:stick  -",
    "-            timber:stick  -")

-- One stone over the same handle: a shovel is the cheaper half of the shape.
core.recipe_shaped("tools:shovel", 1,
    "-  rocks:cobblestone  -",
    "-  timber:stick  -",
    "-  timber:stick  -")

-- And the third of the three, which is new: an axe. Level 1 like the pick and
-- four times a hand, and it is for WOOD -- so the tool a player makes first out
-- of the tree they need for every other tool is the tool that makes the next
-- tree cheap. Two heads and a shoulder, the shape that game uses, and its
-- mirror is not this recipe.
core.register_item("tools:axe", 1, "", 1, 4.0, "wood", 0, "", 0, 131)
core.recipe_shaped("tools:axe", 1,
    "rocks:cobblestone rocks:cobblestone  -",
    "rocks:cobblestone timber:stick  -",
    "-            timber:stick  -")

-- And a sword, the fourth thing the robot carries. It digs nothing better than
-- a hand and hits four times as hard as one: 4 of a body's 20. Two stone over
-- a stick, the shape that game uses.
core.register_item("tools:sword", 1, "", 0, 1.0, "", 4, "", 0, 131)
core.recipe_shaped("tools:sword", 1,
    "-  rocks:cobblestone  -",
    "-  rocks:cobblestone  -",
    "-  timber:stick  -")

-- The same four in wood, the first rung: planks where the stone goes, so the
-- tree alone makes a tool. Level 0 -- a hand's, which is what opens the coal
-- and not the granite -- at twice a hand's speed, and a sword for 3. The iron,
-- gold and diamond ones are mods/furnace's, where the ingots are.
core.register_item("tools:wood_pick", 1, "", 0, 2.0, "stone", 0, "", 0, 59)
core.register_item("tools:wood_axe", 1, "", 0, 2.0, "wood", 0, "", 0, 59)
core.register_item("tools:wood_shovel", 1, "", 0, 2.0, "soil", 0, "", 0, 59)
core.register_item("tools:wood_sword", 1, "", 0, 1.0, "", 3, "", 0, 59)
core.recipe_shaped("tools:wood_pick", 1,
    "timber:planks timber:planks timber:planks",
    "-             timber:stick  -",
    "-             timber:stick  -")
core.recipe_shaped("tools:wood_axe", 1,
    "timber:planks timber:planks -",
    "timber:planks timber:stick  -",
    "-             timber:stick  -")
core.recipe_shaped("tools:wood_shovel", 1,
    "-  timber:planks  -",
    "-  timber:stick   -",
    "-  timber:stick   -")
core.recipe_shaped("tools:wood_sword", 1,
    "-  timber:planks  -",
    "-  timber:planks  -",
    "-  timber:stick   -")
-- Every tool wears out, the classic block game's numbers: a use a block that
-- took time, a use a blow (the engine spends them, EditPlan::wears). Wood
-- lasts 59, stone 131; mods/furnace's iron 250, gold 32, diamond 1561.
--
-- And a stone hoe, farm:hoe being the wooden one: mods/farm tills with any
-- item named *_hoe.
core.register_item("tools:stone_hoe", 1, "", 0, 1.0, "", 0, "", 0, 131)
core.recipe_shaped("tools:stone_hoe", 1,
    "rocks:cobblestone rocks:cobblestone",
    "-                 timber:stick",
    "-                 timber:stick")

-- Not bones of the robot like the stone four: a model each, in the palm
-- (tools/make_held_models.py).
for _, k in { "wood_pick", "wood_axe", "wood_shovel", "wood_sword", "stone_hoe" } do
    core.register_model(`tools:{k}_held`, `{k}_held.gltf`)
end

-- The set piece, in the plaza with the chest and the crafting table.
local BX, BZ = 37, 47
local GX, GZ = 37, 44

-- On the ground the mapgen made, not at a y this file guessed: the same mod
-- runs on a server and on a client that owns its own world, and a block placed
-- at a fixed height is a block buried in one of them.
--
-- Its OWN blocks are not ground. The first version of this counted them, so a
-- world loaded from a file already had the outcrop in it and the mod stacked a
-- second one on top -- one block higher every restart. Caught by
-- tests/persistence.sh, which hashes the world across a restart and saw it
-- move.
local function ground(x: number, z: number): number
    for y = 40, 1, -1 do
        local id = core.get_block(x, y, z)
        if id ~= 0 and id ~= GRANITE and id ~= BENCH then
            return y
        end
    end
    return -1
end

-- On world_ready and not at load: the world file is read AFTER the mods
-- register their blocks, so a set piece built at load is a set piece the stored
-- chunks are laid back over.
core.event.on("core:world_ready", function()
    -- The sandbox's set piece only: the Inferno loads this mod for what it
    -- holds, and mods/guard's square is what makes a world the sandbox.
    if core.node_id("guard:cloud") == 0 then
        return
    end
    local by = ground(BX, BZ)
    if by > 0 then
        core.set_block(BX, by + 1, BZ, BENCH)
    end

    -- An outcrop rather than one block, so breaking it is something a player
    -- does more than once and the second one is the one they time.
    local placed = 0
    for dx = 0, 1 do
        for dz = 0, 1 do
            local gy = ground(GX + dx, GZ + dz)
            if gy > 0 then
                core.set_block(GX + dx, gy + 1, GZ + dz, GRANITE)
                placed += 1
            end
        end
    end

    core.log(`tools: bench at {BX} {by + 1} {BZ}, {placed} granite beside it`)
end)

-- One pick per player, remembered by id. It resets when the server does, and
-- that is on purpose: the bench is where a first pick comes from, and the
-- recipe above is where the next one does.
local issued: { [number]: boolean } = {}

core.event.on("core:used", function(ev)
    if ev.id ~= BENCH then
        return
    end

    -- Claimed either way, like the chest and the table: a right click is a use
    -- before it is a placement, and without this the same click would hand over
    -- a pick AND build whatever was in the other hand on top of the bench.
    ev.cancel = true

    -- After the first pick it is a tool table: a grid of three by three to
    -- make the next tools on, as a crafting table is.
    if issued[ev.player] then
        core.craft_show(ev.x, ev.y, ev.z, ev.player)
        return
    end

    -- Both, and that is the demonstration: two tools in one bag, and which of
    -- them comes out depends on what is being dug and not on which is better.
    local fit = core.give_item(ev.player, "tools:pick", 1)
    if fit == 1 then
        issued[ev.player] = true
        core.give_item(ev.player, "tools:shovel", 1)
        core.log(`tools: gave a pick and a shovel to player {ev.player}`)
    else
        -- Not dropped and not lost: give_item leaves what does not fit alone,
        -- so the bench keeps offering until there is room for it.
        core.log(`tools: player {ev.player} has no room for a pick`)
    end
end)
