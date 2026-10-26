--!strict
-- Memory quota, from the mod's side. Without one, this is how a mod takes the
-- whole process down with the OOM killer -- which is what a 512-chunk world
-- taught us the hard way in M1.

core.log("hog: loaded")

local kept: {string} = {}

function on_step(dt: number)
    -- Strings, not table slots, so the allocator sees real growth rather than
    -- one amortised reallocation.
    for i = 1, 200000 do
        table.insert(kept, string.rep("x", 64))
    end
end
