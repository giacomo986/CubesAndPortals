--!strict
-- Asking for a mapgen stage after the world has been generated (TODO OPEN-35).
--
-- In time while the mod loads; too late from on_step. The mod records which by
-- putting a block in the world, because a log line is not something a gate can
-- fail on.

local REFUSED = core.register_node("late:refused", 30, 30, 30)
local TAKEN   = core.register_node("late:taken", 250, 250, 250)

-- Two answers, two cells. Both are written by a stage rather than at load,
-- because the second question is only answered once the mod is loaded.
local X, Y, Z    = 3, 50, 3
local DX, DY, DZ = 3, 52, 3

-- Read by the stage below and written after it is registered: an upvalue and
-- not a global, because the answer does not exist yet when the stage is
-- declared and the stage does not run until long after it does.
local dup_taken = false

-- In time: registered while the mod is loading.
core.register_mapgen("late:in_time", function(ctx)
    -- The name is taken, and the engine said so. Two stages sharing a name
    -- would be two lines in a log nobody can tell apart, so the second is
    -- refused -- recorded here the same way the late one is.
    core.set_block(DX, DY, DZ, if dup_taken then TAKEN else REFUSED)
end)

dup_taken = pcall(function()
    core.register_mapgen("late:in_time", function(ctx) end)
end)

local asked = false

function on_step(dt: number)
    if asked then
        return
    end
    asked = true

    -- pcall, because the refusal comes back as an error: the alternative is a
    -- return code a mod can ignore, and a stage silently not registered is
    -- exactly what this is about.
    local ok = pcall(function()
        core.register_mapgen("late:too_late", function(ctx) end)
    end)

    core.set_block(X, Y, Z, if ok then TAKEN else REFUSED)
end
