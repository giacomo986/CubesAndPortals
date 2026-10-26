--!strict
-- A chest the world was born with, for the persistence gate.
--
-- The gate it serves runs the real server twice against one world file and asks
-- whether what is in a chest is still there the second time. That needs a chest
-- with something in it and no player to put it there, which is what
-- `container_make` and `container_add` are for: making a container and looking
-- into one are two acts, and only the second one needs somebody's screen.
--
-- It counts before it fills, and only fills when the count is zero. A mod that
-- stocked the chest on every load would pass this gate with the whole storage
-- layer taken out.
--
-- The work happens on the first tick and not at load time, because the item
-- form of a block does not exist yet while mods are loading: the engine adopts
-- one item per block AFTER every mod has had its say, so `core.item_id` for a
-- block name answers 0 up there. Recorded in docs/M7_FINDINGS.md.

local LOOT = core.register_node("loot:box", 200, 170, 40, true, 4)

local X, Y, Z = 8, 20, 8
local SLOTS = 27

core.set_block(X, Y, Z, LOOT)

local done = false

core.event.on("core:tick", function(ev)
    if done then return end
    done = true

    core.container_make(X, Y, Z, SLOTS)

    local stone = core.item_id("core:stone")
    local held = core.container_count(X, Y, Z, stone)

    if held == 0 then
        local left = core.container_add(X, Y, Z, stone, 40)
        core.log(`loot: stocked the chest, {left} did not fit`)
    end

    core.log(`loot: chest holds {core.container_count(X, Y, Z, stone)}`)
end)
