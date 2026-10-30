--!strict
-- Two things the event bus is for, in one mod:
--
--   1. cancelling      -- no building inside the portal plaza;
--   2. its own event   -- guard:alarm, which another mod (beacons) listens to
--                         from its own VM.
--
-- None of this required a line of engine code: the core emits core:pre_place
-- and honours the answer, exactly as it would for any mod.

-- No longer placed by anything: above y 30 used to turn into it, and the sky
-- has real clouds now. Still registered so worlds that hold some keep them.
core.register_node("guard:cloud", 235, 240, 250)

-- The plaza around portal A. Half-open box in block coordinates.
local X0, X1 = 36, 46
local Z0, Z1 = 36, 46

core.event.define("guard:alarm", {
    fields = { x = "number", y = "number", z = "number" },
    cancellable = false,
})

local refusals = 0

-- Priority 10: late enough that a mod wanting the last word can sit at 20, low
-- enough to run before anything that only watches. Stating the number is the
-- point -- load order is not an ordering.
local PLAZA = core.plaza() ~= nil   -- a survival world has no plaza to guard

core.event.on("core:pre_place", 10, function(ev)
    if PLAZA and ev.x >= X0 and ev.x <= X1 and ev.z >= Z0 and ev.z <= Z1 then
        ev.cancel = true
        refusals += 1
        core.log(`refused a block at {ev.x} {ev.y} {ev.z} -- the plaza is protected`)

        -- Somebody else's problem now: whoever cares can listen.
        core.event.emit("guard:alarm", { x = ev.x, y = ev.y, z = ev.z })
    end
end)

core.event.on("core:broken", function(ev)
    core.log(`{core.node_name(ev.id)} broken at {ev.x} {ev.y} {ev.z}`)
end)

-- The tick as an event rather than as a global. Logs once, so a headless bench
-- run shows that core:tick actually fires.
local elapsed = 0.0
local announced = false

core.event.on("core:tick", function(ev)
    elapsed += ev.dt
    if not announced and elapsed >= 5.0 then
        announced = true
        core.log(`core:tick has run for 5 seconds; {refusals} refusals so far`)
    end
end)

core.log("guard: watching core:pre_place, core:broken and core:tick")

function on_save(): string
    return `{refusals}`
end

function on_load(state: string?)
    core.log(`guard: reloaded, carrying over "{state}"`)
end
