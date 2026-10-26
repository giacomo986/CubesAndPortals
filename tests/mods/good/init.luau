--!strict
-- The control mod. It does something observable every step so the test can tell
-- "still running" from "silently stopped".

local ID = core.register_node("good:block", 10, 200, 10)
local ticks = 0

core.log("good: loaded")

function on_step(dt: number)
    ticks += 1
    -- Writes its own tick count into the world, which the stub host records.
    core.set_block(ticks, 0, 0, ID)
end
