--!strict
-- sky: what happens in the sky now and then, over mods/weather's days.
--
--   A METEOR SHOWER, one clear night in five: shooting stars all night,
--   and now and then one that does not burn up -- it comes down in a long
--   streak 30 to 70 from a player, shakes the ground, digs a crater and
--   leaves a meteorite in it: a stone that gives a diamond and raw iron.
--   AN AURORA, one winter night in two: curtains of green light over the
--   north, seen from the cold (the tundra and the peaks).
--   AN ECLIPSE, one day in twelve: about three minutes when the moon covers
--   the sun, the sky goes dusk-blue and the land dark -- dark enough for the
--   creatures of the night to come out (core.overcast, through mods/weather's
--   weather:darken) and not to burn.
--
-- The server decides and tells each player's machine (core.net), which draws
-- (core.render.fx's "streak" and "aurora", core.sky_eclipse). /sky meteors,
-- aurora, eclipse or impact (admin) brings one on now; /sky clear ends them.
-- The meteorite's picture is tools/make_sky_assets.py's.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local COLD = { ["core:tundra"] = true, ["core:peaks"] = true }
local ECLIPSE_LONG = 0.15   -- of a day: three minutes of a twenty-minute day
local FADE = 0.2            -- of an eclipse, in and out

local function night(t: number): boolean
    return t > 0.8 or t < 0.2
end

-- How deep an eclipse that starts at `from` is at `t`, 0..1.
local function eclipse_at(from: number, t: number): number
    if from < 0 then
        return 0
    end
    local k = (t - from) / ECLIPSE_LONG
    if k <= 0 or k >= 1 then
        return 0
    end
    return math.min(1, k / FADE, (1 - k) / FADE)
end

-- What this machine draws, as the server says.
local shower, aurora, eclipse_from = false, false, -1.0
type Fall = { x: number, y: number, z: number, age: number }
local falls: { Fall } = {}
local FALL_TIME = 2.5   -- seconds a meteorite takes to come down

local function heard(text: string)
    local a, b, c = string.match(text, "^S (%d) (%d) (%S+)$")
    if a then
        shower, aurora, eclipse_from = a == "1", b == "1", tonumber(c) or -1
        return
    end
    local x, y, z = string.match(text, "^m (%S+) (%S+) (%S+)$")
    if x then
        table.insert(falls, { x = tonumber(x) or 0, y = tonumber(y) or 0, z = tonumber(z) or 0, age = 0 })
    end
end

if serving then
    core.net.open()
    local METEORITE = core.register_node("sky:meteorite", 70, 60, 64, true, 3, "", 2, 8.0, "stone")
    core.node_drop_item(METEORITE, "ore:diamond")

    local s_shower, s_aurora, s_eclipse = false, false, -1.0
    local season = "spring"
    core.event.on("farm:season", function(ev)
        season = ev.name
    end)

    local function send(p: number, text: string)
        if drawing then
            heard(text)
        else
            core.net.send(p, text)
        end
    end

    local function state(): string
        return `S {if s_shower then 1 else 0} {if s_aurora then 1 else 0} {s_eclipse}`
    end

    local PLAZA_LO: number, PLAZA_HI: number = math.huge, -math.huge
    do
        local lo, hi = core.plaza()
        if lo and hi then
            PLAZA_LO, PLAZA_HI = lo, hi
        end
    end

    -- The top of the ground at a column, near y: the first solid cell down.
    local function ground(x: number, y: number, z: number): number?
        for gy = y + 40, y - 40, -1 do
            local id = core.get_block(x, gy, z)
            if id ~= 0 and core.node_solid(id) == 1 then
                return gy
            end
        end
        return nil
    end

    local WATER = core.node_id("lakes:water")
    local BEDROCK = core.node_id("core:bedrock")

    -- A crater three across round the cell hit, the meteorite at its bottom;
    -- a blow to whoever is within six.
    local function land(x: number, y: number, z: number)
        for dx = -3, 3 do
            for dz = -3, 3 do
                for dy = -3, 1 do
                    if dx * dx + dz * dz + dy * dy * 1.6 <= 10 then
                        local id = core.get_block(x + dx, y + dy, z + dz)
                        if id ~= 0 and id ~= BEDROCK and core.container_fill(x + dx, y + dy, z + dz) < 0 then
                            core.set_block(x + dx, y + dy, z + dz, 0)
                        end
                    end
                end
            end
        end
        core.set_block(x, y - 2, z, METEORITE)
        if math.random(2) == 1 then
            core.set_block(x + (if math.random(2) == 1 then 1 else -1), y - 2, z, METEORITE)
        end
        core.play_sound("weather:thunder", x + 0.5, y, z + 0.5)
        for _, p in core.players() do
            local px, py, pz = core.player_pos(p)
            if px and py and pz then
                local d = math.sqrt((px - x) ^ 2 + (py - y) ^ 2 + (pz - z) ^ 2)
                if d < 6 then
                    core.damage(p, math.ceil(12 * (1 - d / 6)))
                end
            end
        end
        core.log(`sky: a meteorite came down at {x} {y} {z}`)
    end

    type Coming = { x: number, y: number, z: number, left: number }
    local coming: { Coming } = {}

    -- One that does not burn up, near player p: told now, landed when it
    -- has come down the whole of its streak.
    local function impact(p: number): boolean
        local px, py, pz = core.player_pos(p)
        if px == nil or py == nil or pz == nil then
            return false
        end
        for _ = 1, 8 do
            local a, r = math.random() * math.pi * 2, 30 + math.random() * 40
            local x, z = math.floor(px + math.cos(a) * r), math.floor(pz + math.sin(a) * r)
            local plaza = x >= PLAZA_LO - 8 and x <= PLAZA_HI + 8 and z >= PLAZA_LO - 8 and z <= PLAZA_HI + 8
            local gy = ground(x, math.floor(py), z)
            if not plaza and gy and core.owned_at(x, z) == 0 and core.get_block(x, gy + 1, z) ~= WATER
                and core.get_block(x, gy, z) ~= WATER then
                for _, q in core.players() do
                    send(q, `m {x + 0.5} {gy + 1} {z + 0.5}`)
                end
                table.insert(coming, { x = x, y = gy, z = z, left = FALL_TIME })
                return true
            end
        end
        return false
    end

    local function begin(what: string)
        if what == "meteors" then
            s_shower = true
        elseif what == "aurora" then
            s_aurora = true
        elseif what == "eclipse" then
            s_eclipse = core.time_of_day()
        end
        core.log(`sky: {what}`)
    end

    -- A meteorite broken gives raw iron besides its diamond.
    core.event.on("core:broken", function(ev)
        if ev.id == METEORITE then
            core.drop_item(ev.x, ev.y, ev.z, core.item_id("ore:raw_iron"), math.random(2, 4))
        end
    end)

    -- Read at the first tick, not here: while the mods load a saved world's
    -- clock is not set yet (0), and the first tick then "crossed" dawn and
    -- rolled the day's eclipse at every start -- one start in twelve, the
    -- creatures of the night out in the open as soon as somebody joined
    -- (reported from play, 2026-11-07).
    local last_t = -1.0
    local told: { [number]: string } = {}
    local since, next_impact = 0.0, 60.0
    core.event.on("core:tick", function(ev)
        local t = core.time_of_day()
        if last_t < 0 then
            last_t = t
        end
        -- Dusk: what the night brings. Dawn: the night's things end, and
        -- what the day brings.
        if last_t < 0.8 and t >= 0.8 then
            s_shower = math.random(5) == 1
            s_aurora = season == "winter" and math.random(2) == 1
            if s_shower then core.log("sky: a meteor shower tonight") end
            if s_aurora then core.log("sky: an aurora tonight") end
        elseif last_t < 0.2 and t >= 0.2 then
            s_shower, s_aurora = false, false
            if math.random(12) == 1 then
                s_eclipse = 0.35 + math.random() * 0.2
                core.log("sky: an eclipse today")
            end
        end
        last_t = t
        -- The eclipse's dark, as deep as it is now, on the server's day.
        local e = eclipse_at(s_eclipse, t)
        core.event.emit("weather:darken", { amount = e })
        if s_eclipse >= 0 and t > s_eclipse + ECLIPSE_LONG then
            s_eclipse = -1
        end
        for i = #coming, 1, -1 do
            local c = coming[i]
            c.left -= ev.dt
            if c.left <= 0 then
                table.remove(coming, i)
                land(c.x, c.y, c.z)
            end
        end
        since += ev.dt
        if since < 1 then
            return
        end
        since = 0
        local now = state()
        local players = core.players()
        local seen: { [number]: string } = {}
        for _, p in players do
            if told[p] ~= now then
                send(p, now)
            end
            seen[p] = now
        end
        told = seen
        if s_shower and night(t) and #players > 0 then
            next_impact -= 1
            if next_impact <= 0 then
                next_impact = 60 + math.random() * 120
                impact(players[math.random(#players)])
            end
        end
    end)

    core.command("sky", "[meteors|aurora|eclipse|impact|clear] -- bring one on now, or end them", "admin")
    core.event.on("core:command", function(ev)
        if ev.name ~= "sky" then
            return
        end
        local what = string.match(ev.args, "^%s*(%a*)%s*$") or ""
        if what == "meteors" or what == "aurora" or what == "eclipse" then
            begin(what)
            core.chat(ev.player, `Sky: {what}.`)
        elseif what == "impact" then
            core.chat(ev.player, if impact(ev.player) then "Sky: one is coming down." else "Sky: nowhere for it here.")
        elseif what == "clear" then
            s_shower, s_aurora, s_eclipse = false, false, -1
            core.chat(ev.player, "Sky: clear.")
        else
            core.chat(ev.player, `Sky: meteors {s_shower}, aurora {s_aurora}, eclipse {s_eclipse >= 0}. `
                .. "Usage: /sky [meteors|aurora|eclipse|impact|clear]")
        end
    end)
end

if drawing then
    core.event.on("core:net_message", function(ev)
        heard(ev.text)
    end)

    -- A shooting star: from where, which way, how far, how long, how bright.
    type Star = { x: number, y: number, z: number, dx: number, dy: number, dz: number,
                  len: number, life: number, age: number, size: number }
    local stars: { Star } = {}
    local clock, next_star, glow = 0.0, 0.0, 0.0
    local dim = 0.0   -- the eclipse, faded smooth

    local function me(): (number?, number?, number?)
        local p = core.players()[1]
        if p == nil then
            return nil, nil, nil
        end
        return core.player_pos(p)
    end

    core.event.on("core:tick", function(ev)
        clock += ev.dt
        local t = core.time_of_day()
        -- The eclipse: worked out here from the hour, so it follows the
        -- server's clock without being told every frame.
        dim = eclipse_at(eclipse_from, t)
        core.sky_eclipse(dim)
        local px, py, pz = me()
        if shower and night(t) and px and py and pz then
            next_star -= ev.dt
            if next_star <= 0 then
                next_star = 0.6 + math.random() * 2.4
                local a = math.random() * math.pi * 2
                local r = 90 + math.random() * 60
                local h = math.random() * math.pi * 2
                table.insert(stars, {
                    x = px + math.cos(a) * r, y = py + 70 + math.random() * 50, z = pz + math.sin(a) * r,
                    dx = math.cos(h), dy = -0.25 - math.random() * 0.3, dz = math.sin(h),
                    len = 25 + math.random() * 30, life = 0.5 + math.random() * 0.7, age = 0,
                    size = 0.5 + math.random() * 0.6 })
            end
        end
        for i = #stars, 1, -1 do
            stars[i].age += ev.dt
            if stars[i].age >= stars[i].life then
                table.remove(stars, i)
            end
        end
        for i = #falls, 1, -1 do
            falls[i].age += ev.dt
            if falls[i].age >= FALL_TIME + 1.5 then
                table.remove(falls, i)
            end
        end
        -- The ground lit as it lands.
        glow = 0
        for _, f in falls do
            local k = f.age - FALL_TIME
            if k >= 0 and k < 0.6 then
                glow = math.max(glow, 0.6 * (1 - k / 0.6))
            end
        end
        if glow > 0 then
            core.sky_flash(glow)
        end
    end)

    core.render.pass("sky:events", 30, function()
        for _, s in stars do
            local k = s.age / s.life
            local hx, hy, hz = s.x + s.dx * s.len * k, s.y + s.dy * s.len * k, s.z + s.dz * s.len * k
            local trail = math.min(12, s.len * k)
            core.render.fx("streak", hx - s.dx * trail, hy - s.dy * trail, hz - s.dz * trail, s.size,
                           230, 240, 255, 255, math.max(0, k - 0.7) / 0.3, hx, hy, hz)
        end
        -- A meteorite: down a long slant onto where it lands, burning orange,
        -- then a flash and a shock on the ground.
        for _, f in falls do
            if f.age < FALL_TIME then
                local k = f.age / FALL_TIME
                local sx, sy, sz = f.x - 90, f.y + 140, f.z - 50
                local hx, hy, hz = sx + (f.x - sx) * k, sy + (f.y - sy) * k, sz + (f.z - sz) * k
                local tx, ty, tz = hx - (f.x - sx) * 0.12, hy - (f.y - sy) * 0.12, hz - (f.z - sz) * 0.12
                core.render.fx("streak", tx, ty, tz, 2.2, 255, 150, 60, 255, 0, hx, hy, hz)
                core.render.fx("glow", hx, hy, hz, 3.5, 255, 190, 90, 255, 0)
            else
                local k = (f.age - FALL_TIME) / 1.5
                core.render.fx("ring", f.x, f.y, f.z, 7, 255, 170, 80, 255, k)
                core.render.fx("glow", f.x, f.y + 1, f.z, 5, 255, 200, 120, 255, k)
            end
        end
        -- The aurora: three curtains far over the north, swaying, seen by
        -- night from the cold.
        if aurora and night(core.time_of_day()) then
            local px, py, pz = me()
            if px and py and pz and COLD[core.biome(math.floor(px), math.floor(pz))] then
                for c = 0, 2 do
                    local base = py + 60 + c * 8
                    local far = 140 + c * 25
                    local prev_x: number?, prev_z: number? = nil, nil
                    for i = 0, 10 do
                        local u = i / 10
                        local x = px - 160 + u * 320 + c * 30
                        local z = pz - far + math.sin(u * 5 + clock * 0.07 + c * 2) * 25
                        if prev_x and prev_z then
                            core.render.fx("aurora", prev_x, base, prev_z, 45 - c * 6,
                                           70 + c * 20, 255 - c * 30, 150, 170, 0, x, base, z)
                        end
                        prev_x, prev_z = x, z
                    end
                end
            end
        end
    end)
end
