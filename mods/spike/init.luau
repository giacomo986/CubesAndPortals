--!strict
-- M0 asks for "a mod that places blocks and moves a portal at runtime". This is
-- that mod, and it doubles as the load for the scripting half of criterion 3:
-- on_step runs every frame and does enough work to be measurable.
--
-- Written in typed Luau on purpose. DESIGN.md 5.5 draws a line: typing is
-- available, never required. Nothing here needs a toolchain -- it is a .luau
-- file that opens in a text editor and runs.

-- This mod's manifest declares no capabilities, so `os` and `debug` are not in
-- its VM -- not shadowed, not empty, absent. Checking it here rather than in a
-- comment means the day somebody widens the default, this line says so.
assert(os == nil, "spike declared no capabilities but can see os")
assert(debug == nil, "spike declared no capabilities but can see debug")

-- Blocks the mod declares for itself. It never sees a numeric id it did not
-- ask for: register_node hands one back, node_id looks up somebody else's.
local COPPER = core.register_node("spike:copper", 200, 120, 70)
local SLATE  = core.register_node("spike:slate", 70, 75, 95)
local LAMP   = core.register_node("spike:lamp", 240, 230, 150, true, 14)

local STONE = core.node_id("core:stone")
local FRAME = core.node_id("core:frame")

-- Which portal the mod moves, and where the engine says that portal is.
--
-- ASKED, not written down. These used to be the literals 79.5, 22.0, 60.5 --
-- the spike scene's portal B -- and every frame the mod parked the portal
-- there. In that one scene it was invisible; in every other one it dragged the
-- opening across the world, and because the host only moved the renderer's copy
-- the portal ended up DRAWN at 79.50 22.00 60.57 and TELEPORTING from
-- 60.50 21.50 60.00. A mod that writes absolute coordinates is hardcoding the
-- scene it was written against.
local PORTAL_INDEX = 1
local PORTAL_X = core.get_portal(PORTAL_INDEX, 0)
local PORTAL_Y = core.get_portal(PORTAL_INDEX, 1)
local PORTAL_Z = core.get_portal(PORTAL_INDEX, 2)

-- Placed where the bench's first shot is looking, so the mod's work shows up in
-- the screenshots instead of only in a log line.
local BASE_X = 44
local BASE_Y = 20
local BASE_Z = 44

local elapsed: number = 0.0
local placed: number = 0

-- Blocks placed at load: a stepped tower, obviously built rather than generated.
local function build_tower()
    for step = 0, 3 do
        local height = 5 - step
        for h = 0, height - 1 do
            -- Its own blocks, in a pattern that makes them identifiable.
            local id = if h == height - 1 then LAMP elseif (step % 2) == 0 then COPPER else SLATE
            core.set_block(BASE_X + step, BASE_Y + h, BASE_Z, id)
            placed += 1
        end
    end
    -- The blinking lamp's cell, which a world saved before it stopped may hold.
    core.set_block(BASE_X, BASE_Y + 8, BASE_Z, 0)
    core.log(`spike mod: registered copper={COPPER} slate={SLATE} lamp={LAMP}, `
        .. `placed {placed} blocks (core:stone is {STONE}, core:frame is {FRAME})`)
end

if core.plaza() ~= nil then   -- on the plaza, and only where there is one
    build_tower()
end

-- Called once per frame by the host.
function on_step(dt: number)
    elapsed += dt

    -- Move portal B back and forth along its own plane. The portal is engine
    -- state, so this is the mod reaching into something the engine renders
    -- every frame, which is the part worth proving.
    --
    -- The amplitude is small on purpose: the wall around the portal does not
    -- follow it, so a large swing slides the opening out of its own hole. That
    -- the two are independent is the honest state of things in M0 -- a portal
    -- is not yet attached to the blocks that frame it.
    local offset = math.sin(elapsed * 0.6) * 0.4
    core.set_portal(PORTAL_INDEX, PORTAL_X, PORTAL_Y, PORTAL_Z + offset)

    -- A little steady work so the per-frame cost is not measuring an empty
    -- function call: read a column. It used to toggle a lamp on top of it
    -- twice a second too, which on the spawn plaza was a place and a break
    -- sound without end.
    local probe = 0
    for h = 0, 7 do
        if core.get_block(BASE_X, BASE_Y + h, BASE_Z) ~= 0 then
            probe += 1
        end
    end
end
