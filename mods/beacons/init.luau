--!strict
-- Second mod, second VM. Everything here is deliberately named the same as
-- something in spike/init.luau -- COPPER, elapsed, on_step, build -- because two
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

local elapsed: number = 0.0
local lit: number = -1

local function build()
    for i = 0, COUNT - 1 do
        local x = BASE_X + i * 2
        for h = 0, HEIGHT - 1 do
            core.set_block(x, BASE_Y + h, BASE_Z, PILLAR)
        end
    end
    core.log(`beacons: pillar={PILLAR} beacon={BEACON}, {COUNT} pillars up, `
        .. `os.clock at load {string.format("%.3f", started)}s (spike:copper is {COPPER})`)
end

build()

-- An event from another mod, handled in this VM. beacons has never heard of
-- core:pre_place: it hears that guard refused something, and answers by lighting
-- every beacon at once. Cross-mod behaviour with neither mod calling the other.
core.event.on("guard:alarm", function(ev)
    for i = 0, COUNT - 1 do
        core.set_block(BASE_X + i * 2, BASE_Y + HEIGHT, BASE_Z, BEACON)
    end
    lit = -1
    core.log(`alarm from guard at {ev.x} {ev.y} {ev.z}: all beacons lit`)
end)

-- One beacon alight at a time, walking along the row. Cheap, visible, and it
-- proves the mod keeps stepping while another mod in another VM is being
-- disabled for misbehaving.
function on_step(dt: number)
    elapsed += dt

    local want = math.floor(elapsed * 1.5) % COUNT
    if want == lit then
        return
    end

    if lit >= 0 then
        core.set_block(BASE_X + lit * 2, BASE_Y + HEIGHT, BASE_Z, 0)
    end
    core.set_block(BASE_X + want * 2, BASE_Y + HEIGHT, BASE_Z, BEACON)
    lit = want
end
