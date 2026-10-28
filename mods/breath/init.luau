--!strict
-- breath: drowning.
--
-- With the eye in water a player has fifteen seconds of air, ten bubbles of
-- a second and a half; out of it they come back four a second. With none left
-- the water takes two points a second. A row of bubbles over the hunger bar
-- shows what is left while it is not full.
--
-- Another mod lets a player breathe under water for a while by emitting
-- "breath:water" with { player = id, seconds = n } (mods/alchemy's potion).

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local MAX = 10           -- bubbles
local LOSE = 1 / 1.5     -- bubbles a second under water
local GAIN = 4           -- bubbles a second out of it
local DROWN = 2          -- points a second with none left
local EYE = 1.62         -- the engine's eye height

local shown = MAX

core.event.define("breath:water", { fields = { player = "number", seconds = "number" }, cancellable = false })

if serving then
    core.net.open()

    -- mods/lakes's water, looked up once it has loaded.
    local WATER: { [number]: boolean }? = nil
    local function water(): { [number]: boolean }
        local w = WATER
        if w == nil then
            w = {}
            for _, name in { "lakes:water", "lakes:flow1", "lakes:flow2", "lakes:flow3",
                             "lakes:flow4", "lakes:flow5", "lakes:flow6" } do
                local id = core.node_id(name)
                if id ~= 0 then
                    w[id] = true
                end
            end
            WATER = w
        end
        return w
    end

    type Lungs = { air: number, told: number, gills: number, choke: number }
    local lungs: { [number]: Lungs } = {}

    core.event.on("breath:water", function(ev)
        local l = lungs[ev.player] or { air = MAX, told = MAX, gills = 0, choke = 0 }
        l.gills = math.max(l.gills, ev.seconds)
        lungs[ev.player] = l
    end)

    local function tell(p: number, n: number)
        if drawing then
            shown = n
        else
            core.net.send(p, tostring(n))
        end
    end

    core.event.on("core:tick", function(ev)
        local here: { [number]: boolean } = {}
        for _, p in core.players() do
            here[p] = true
            local x, y, z = core.player_pos(p)
            if x then
                local l = lungs[p]
                if l == nil then
                    l = { air = MAX, told = MAX, gills = 0, choke = 0 }
                    lungs[p] = l
                end
                l.gills = math.max(0, l.gills - ev.dt)
                local under = water()[core.get_block(math.floor(x), math.floor(y + EYE), math.floor(z))] == true
                if under and l.gills <= 0 then
                    l.air = math.max(0, l.air - LOSE * ev.dt)
                else
                    l.air = math.min(MAX, l.air + GAIN * ev.dt)
                end
                if l.air <= 0 then
                    l.choke += ev.dt
                    if l.choke >= 1 then
                        l.choke = 0
                        core.damage(p, DROWN)
                    end
                else
                    l.choke = 0
                end
                local n = math.ceil(l.air)
                if n ~= l.told then
                    l.told = n
                    tell(p, n)
                end
            end
        end
        for p in lungs do
            if not here[p] then
                lungs[p] = nil
            end
        end
    end)
end

if drawing then
    if not serving then
        core.event.on("core:net_message", function(ev)
            shown = tonumber(ev.text) or shown
        end)
    end

    -- Over the hunger bar (mods/hunger), by the engine's measures.
    local SLOT, PAD = 0.075, 0.0075
    local WIDE = 9 * SLOT + 8 * PAD
    local TALL = SLOT * 0.22
    local HEALTH_TOP = 1 - SLOT - 0.02 - PAD * 2 - TALL
    local Y = HEALTH_TOP - 4 * (TALL * 2) + TALL * 0.5
    core.hud.pass("breath:bubbles", 50, function()
        local aspect = core.hud.aspect()
        if aspect <= 0 or not core.engine_hud_on() or shown >= MAX then
            return
        end
        local step = WIDE / MAX
        local w = TALL * 1.3
        for i = 0, MAX - 1 do
            local cx = 0.5 + (WIDE / 2 - (i + 0.5) * step) / aspect
            if i < shown then
                core.hud.rect(cx, Y, w, w, 200, 235, 255, 230)
                core.hud.rect(cx, Y, w * 0.7, w * 0.7, 90, 160, 230, 255)
            else
                core.hud.rect(cx, Y, w, w, 70, 90, 110, 200)
            end
        end
    end)
end
