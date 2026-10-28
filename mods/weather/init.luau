--!strict
-- weather: rain, snow where it is cold, and storms with lightning.
--
-- The server keeps one sky for the world: clear for ten to twenty minutes,
-- then rain (or, one time in four, a storm) for three to eight. What falls on
-- a player is the sky over their biome: snow in the tundra and on the peaks,
-- nothing in the desert, rain anywhere else. Every two seconds it tells each
-- player's machine what falls there; the machine draws it round the body
-- (core.render.box, stopped by whatever is overhead) and greys its sky
-- (core.overcast), fading both in and out.
--
-- While it rains the fields under the open sky are watered (farm:rain), and
-- snow settles a layer thick on the ground of the cold biomes near players. A
-- storm strikes the ground near a player every few seconds: a bolt everybody
-- sees, thunder from where it fell, eight points off anybody within three
-- nodes of it.
--
--   /weather [clear|rain|storm] [minutes]   -- admin: the sky now, or for how long
local CLEAR_MIN, CLEAR_MAX = 600, 1200   -- seconds
local WET_MIN, WET_MAX = 180, 480
local BOLT_MIN, BOLT_MAX = 6, 18
local BOLT_HURT, BOLT_REACH = 8, 3
local SNOWS = { ["core:tundra"] = true, ["core:peaks"] = true }

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

-- What this machine draws: rain, snow, storm, dry or none, and the last bolt.
local falling = "none"
local bolt_x, bolt_y, bolt_z, bolt_age = 0, 0, 0, 99

local function heard(text: string)
    local kind = string.match(text, "^w (%a+)$")
    if kind then
        falling = kind
        return
    end
    local x, y, z = string.match(text, "^b (%S+) (%S+) (%S+)$")
    if x then
        bolt_x, bolt_y, bolt_z, bolt_age = tonumber(x) or 0, tonumber(y) or 0, tonumber(z) or 0, 0
    end
end

if serving then
    core.net.open()
    local LAYER = core.register_node("weather:snow", 244, 248, 252, false, 0, "", 0, 0.15, "soil")
    core.node_box(LAYER, 0.0, 0.0, 0.0, 1.0, 0.125, 1.0)

    local sky = "clear"
    local left = math.random(CLEAR_MIN, CLEAR_MAX)
    local told: { [number]: string } = {}
    local since, bolt_in = 0, BOLT_MAX

    local function send(p: number, text: string)
        if drawing then
            heard(text)
        else
            core.net.send(p, text)
        end
    end

    local function set_sky(what: string, secs: number)
        if (what == "clear") ~= (sky == "clear") then
            core.event.emit("farm:rain", { on = what ~= "clear" })
        end
        sky, left = what, secs
        core.log(`weather: {what} for {math.floor(secs / 60)} min`)
    end

    -- What falls on a column, from the sky over it.
    local function falls(x: number, z: number): string
        if sky == "clear" then
            return "none"
        end
        local b = core.biome(x, z)
        if SNOWS[b] then
            return "snow"
        elseif b == "core:desert" then
            return "dry"
        end
        return sky
    end

    -- The first cell from the top that is not air, from y + up down to y - down.
    local function ground(x: number, y: number, z: number, up: number, down: number): (number?, number)
        for gy = y + up, y - down, -1 do
            local id = core.get_block(x, gy, z)
            if id ~= 0 then
                return gy, id
            end
        end
        return nil, 0
    end

    local function strike(p: number)
        local px, py, pz = core.player_pos(p)
        if px == nil or py == nil or pz == nil then
            return
        end
        local a, r = math.random() * math.pi * 2, 10 + math.random() * 30
        local x, z = math.floor(px + math.cos(a) * r), math.floor(pz + math.sin(a) * r)
        local gy = ground(x, math.floor(py), z, 30, 30)
        if gy == nil then
            return
        end
        local bx, by, bz = x + 0.5, gy + 1, z + 0.5
        for _, q in core.players() do
            send(q, `b {bx} {by} {bz}`)
            local qx, qy, qz = core.player_pos(q)
            if qx and qy and qz and (qx - bx) ^ 2 + (qy - by) ^ 2 + (qz - bz) ^ 2 < BOLT_REACH ^ 2 then
                core.damage(q, BOLT_HURT)
            end
        end
        core.play_sound("weather:thunder", bx, by, bz)
    end

    -- A few columns near a player in the snow get their layer.
    local function settle(px: number, py: number, pz: number)
        for _ = 1, 6 do
            local x, z = math.floor(px) + math.random(-24, 24), math.floor(pz) + math.random(-24, 24)
            if SNOWS[core.biome(x, z)] then
                local gy, id = ground(x, math.floor(py), z, 24, 16)
                if gy and id ~= LAYER and core.node_solid(id) == 1 then
                    core.set_block(x, gy + 1, z, LAYER)
                end
            end
        end
    end

    core.event.on("core:tick", function(ev)
        left -= ev.dt
        if left <= 0 then
            if sky == "clear" then
                set_sky(if math.random(4) == 1 then "storm" else "rain", math.random(WET_MIN, WET_MAX))
            else
                set_sky("clear", math.random(CLEAR_MIN, CLEAR_MAX))
            end
        end
        since += ev.dt
        bolt_in -= ev.dt
        if since < 2 then
            return
        end
        since = 0
        local now: { [number]: string } = {}
        local stormy: { number } = {}
        for _, p in core.players() do
            local x, y, z = core.player_pos(p)
            if x and y and z then
                local kind = falls(math.floor(x), math.floor(z))
                now[p] = kind
                if told[p] ~= kind then
                    send(p, `w {kind}`)
                end
                if kind == "storm" then
                    table.insert(stormy, p)
                elseif kind == "snow" then
                    settle(x, y, z)
                end
            end
        end
        told = now
        if bolt_in <= 0 then
            bolt_in = math.random(BOLT_MIN, BOLT_MAX)
            if #stormy > 0 then
                strike(stormy[math.random(#stormy)])
            end
        end
    end)

    -- Snow carried out of the cold melts.
    core.register_abm("weather:melt", { "weather:snow" }, 30.0, 4, function(cell)
        if not SNOWS[core.biome(cell.x, cell.z)] then
            core.set_block(cell.x, cell.y, cell.z, 0)
        end
    end)

    core.command("weather", "[clear|rain|storm] [minutes] -- the sky now, or set it", "admin")
    core.event.on("core:command", function(ev)
        if ev.name ~= "weather" then
            return
        end
        local what, mins = string.match(ev.args, "^%s*(%a*)%s*(%d*)%s*$")
        if what == nil or what == "" then
            core.chat(ev.player, `Weather: {sky}, {math.ceil(left / 60)} min left.`)
        elseif what == "clear" or what == "rain" or what == "storm" then
            local n = tonumber(mins)
            set_sky(what, if n then n * 60
                elseif what == "clear" then math.random(CLEAR_MIN, CLEAR_MAX)
                else math.random(WET_MIN, WET_MAX))
            bolt_in = 2
            core.chat(ev.player, `Weather: {sky} for {math.ceil(left / 60)} min.`)
        else
            core.chat(ev.player, "Usage: /weather [clear|rain|storm] [minutes]")
        end
    end)
end

if drawing then
    core.event.on("core:net_message", function(ev)
        heard(ev.text)
    end)

    local R, TOP, FALL = 10, 14, 22   -- around the body, above its eyes, how far a drop falls
    local GREY = { rain = 0.75, storm = 1.0, snow = 0.6, dry = 0.25, none = 0 }
    local amount, clock, base_y = 0, 0, -1000
    local cover: { [number]: number } = {}

    -- Where a drop over this column stops: on the first thing under the sky.
    local function stop_at(x: number, z: number, py: number): number
        local key = x * 1000003 + z
        local y = cover[key]
        if y == nil then
            y = py - FALL
            for gy = py + TOP, py - FALL, -1 do
                if core.get_block(x, gy, z) ~= 0 then
                    y = gy + 1
                    break
                end
            end
            cover[key] = y
        end
        return y
    end

    -- A number in [0, 1) for drop i, the same every frame.
    local function hash(i: number, k: number): number
        local v = math.sin(i * 12.9898 + k * 78.233) * 43758.5453
        return v - math.floor(v)
    end

    local refresh = 0
    core.event.on("core:tick", function(ev)
        clock += ev.dt
        bolt_age += ev.dt
        local want = GREY[falling] or 0
        amount += math.clamp(want - amount, -ev.dt * 0.15, ev.dt * 0.15)
        local flash = if bolt_age < 0.15 then 0.7 else 0
        core.overcast(math.max(0, amount - flash))
        refresh += ev.dt
        if refresh > 3 then
            refresh = 0
            cover = {}
        end
    end)

    core.render.pass("weather:fall", 40, function()
        if bolt_age < 0.4 then
            core.render.fx("bolt", bolt_x, bolt_y + 70, bolt_z, 0.6, 220, 230, 255, 255,
                           bolt_age / 0.4, bolt_x, bolt_y, bolt_z)
        end
        local snow = falling == "snow"
        local wet = falling == "rain" or falling == "storm" or snow
        local strength = amount / math.max(0.01, GREY[falling] or 1)
        if not wet or strength <= 0.02 then
            return
        end
        local me = core.players()[1]
        if me == nil then
            return
        end
        local px, py, pz = core.player_pos(me)
        if px == nil or py == nil or pz == nil then
            return
        end
        local fy = math.floor(py)
        if math.abs(fy - base_y) > 3 then
            base_y, cover = fy, {}
        end
        local count = math.floor((if snow then 360 elseif falling == "storm" then 600 else 450) * math.min(1, strength))
        local speed = if snow then 1.6 else 13
        local span = R * 2
        for i = 1, count do
            local x = px + (hash(i, 1) * span - px) % span - R
            local z = pz + (hash(i, 2) * span - pz) % span - R
            local y = py + TOP - (clock * speed * (0.85 + hash(i, 3) * 0.3) + hash(i, 4) * FALL) % FALL
            if snow then
                x += math.sin(clock * 1.3 + i) * 0.3
                z += math.cos(clock * 1.1 + i * 0.7) * 0.3
            end
            if y > stop_at(math.floor(x), math.floor(z), base_y) then
                if snow then
                    core.render.box(x, y, z, 0.07, 0.07, 0.07, 245, 248, 255, 230)
                else
                    core.render.box(x, y, z, 0.03, 0.6, 0.03, 175, 195, 225, 170)
                end
            end
        end
    end)
end
