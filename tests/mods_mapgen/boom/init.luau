--!strict
-- A mapgen stage that fails halfway (TODO OPEN-35).
--
-- What this fixture is for is the blast radius: the block written before the
-- error stays, this mod stops, and every other mod's stage still runs.

local HALF = core.register_node("boom:half", 200, 200, 60)

local X, Y, Z = 5, 45, 5

core.register_mapgen("boom:stage", function(ctx)
    core.set_block(X, Y, Z, HALF)
    error("on purpose, so the gate can see what a failed stage costs")
end)
