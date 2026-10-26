--!strict
-- A mod that generates part of the world (TODO OPEN-35).
--
-- Everything here is a function of the seed and the column, which is the one
-- rule a stage has to keep: the same world has to come out on the server and on
-- whatever generates it again. Nothing reads a clock, and nothing reads a
-- global that another stage could have changed.

local MARK  = core.register_node("spire:mark", 220, 60, 200)
local CROWN = core.register_node("spire:crown", 60, 200, 220)

-- Where the two stages fight over one cell. Well above any terrain, so this
-- check is about the ORDER and not about how tall the mountains came out.
local DUEL_X, DUEL_Y, DUEL_Z = 7, 40, 7

-- The column the stage stands a spire on. Outside the plaza (x and z in 20..100
-- are flat by construction), so that core.terrain_height has something to say.
local SITE_X, SITE_Z = 10, 10

core.register_mapgen("spire:columns", function(ctx)
    -- First: the loser of the duel. If this block is what is there at the end,
    -- the stages ran in the wrong order.
    core.set_block(DUEL_X, DUEL_Y, DUEL_Z, MARK)

    -- The generator's own ground, asked for rather than guessed. A spire that
    -- computed its own height would be a second opinion about where the ground
    -- is: see core.terrain_height.
    local ground = core.terrain_height(SITE_X, SITE_Z)
    for h = 0, 3 do
        core.set_block(SITE_X, ground + h, SITE_Z, MARK)
    end

    -- And something that depends on the seed, so that two different seeds
    -- cannot come out as the same world by accident.
    local wobble = ctx.seed % 5
    core.set_block(SITE_X + 2, ground + wobble, SITE_Z + 2, MARK)
end)

core.register_mapgen("spire:caps", function(ctx)
    -- Second: the winner. Registered after spire:columns and therefore run
    -- after it, over the top of what it wrote.
    core.set_block(DUEL_X, DUEL_Y, DUEL_Z, CROWN)

    -- On top of the spire the stage before this one built, found by reading the
    -- world rather than by repeating its arithmetic -- which is what "a stage
    -- sees what the ones before it wrote" is for.
    local y = 0
    while y < ctx.height - 1 and core.get_block(SITE_X, y, SITE_Z) ~= 0 do
        y += 1
    end
    core.set_block(SITE_X, y, SITE_Z, CROWN)
end)
