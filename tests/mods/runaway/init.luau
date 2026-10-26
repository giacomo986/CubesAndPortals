--!strict
-- The mod DESIGN.md 5.6 is written against: an infinite loop must cost this mod
-- its life, not the server's tick.

core.log("runaway: loaded")

local sink = 0

function on_step(dt: number)
    while true do
        sink += 1
    end
end
