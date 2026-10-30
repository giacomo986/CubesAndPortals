--!strict
-- Second mod, second VM. Everything here is deliberately named the same as
-- something in spike/init.luau -- COPPER, build -- because two
-- mods sharing one environment would collide on exactly these names, and the
-- test for isolation is that nothing here disturbs the tower spike builds.

-- This mod declared `capabilities = os` in its manifest, so the global is here.
-- spike declared nothing and, in its own VM, `os` is nil. The grant is per mod,
-- not per process.
local started: number = os.clock()

local PILLAR = core.register_node("beacons:pillar", 60, 150, 140)
local BEACON = core.register_node("beacons:beacon", 255, 240, 180, true, 14)

-- Same local name spike uses for a different block; different VM, different id,
-- no interference.
local COPPER = core.node_id("spike:copper")

local BASE_X = 50
local BASE_Y = 20
local BASE_Z = 42

local HEIGHT = 4
local COUNT = 3

local function build()
    for i = 0, COUNT - 1 do
        local x = BASE_X + i * 2
        for h = 0, HEIGHT - 1 do
            core.set_block(x, BASE_Y + h, BASE_Z, PILLAR)
        end
        -- Dark at the start. A beacon used to walk along the row, lit and put
        -- out one and a half times a second, and on the spawn plaza that was a
        -- place and a break sound without end; now they light only on an alarm.
        core.set_block(x, BASE_Y + HEIGHT, BASE_Z, 0)
    end
    core.log(`beacons: pillar={PILLAR} beacon={BEACON}, {COUNT} pillars up, `
        .. `os.clock at load {string.format("%.3f", started)}s (spike:copper is {COPPER})`)
end

if core.plaza() ~= nil then   -- on the plaza, and only where there is one
    build()
end

-- An event from another mod, handled in this VM. beacons has never heard of
-- core:pre_place: it hears that guard refused something, and answers by lighting
-- every beacon at once. Cross-mod behaviour with neither mod calling the other.
core.event.on("guard:alarm", function(ev)
    for i = 0, COUNT - 1 do
        core.set_block(BASE_X + i * 2, BASE_Y + HEIGHT, BASE_Z, BEACON)
    end
    core.log(`alarm from guard at {ev.x} {ev.y} {ev.z}: all beacons lit`)
end)
