--!strict
-- Declared `os`, so it has `os` -- while `good` and `runaway`, in their own VMs
-- in the same process, do not.

assert(os ~= nil, "timekeeper declared os and did not get it")
assert(debug == nil, "timekeeper did not declare debug and got it anyway")

local t0 = os.clock()
core.log("timekeeper: loaded, os.clock works")

function on_step(dt: number)
    if os.clock() < t0 then
        core.log("timekeeper: time ran backwards")
    end
end
