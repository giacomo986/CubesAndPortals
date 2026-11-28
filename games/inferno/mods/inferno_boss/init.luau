--!strict
-- inferno_boss: the guardian of inferno_heart as a boss fight. Whoever is
-- within NEAR of a guardian -- the hall's, or a survival level's -- sees its
-- health over the screen. And every WAVE_EVERY seconds (WAVE_LOW under half
-- its health) it throws a ball of fire: FLIGHT seconds in an arc, to where
-- the nearest player stood when it was thrown (marked on the floor all the
-- while), and from where it lands a wall of fire
-- runs out every way at WAVE_SPEED to WAVE_REACH. Whoever it passes with
-- their feet on the floor is burnt for WAVE_DAMAGE; a jump over it is the
-- way past. (2026-11-28: the first version, a ring from its own feet in 0.8 s
-- at 8 m/s, came at a player below it too fast to do anything.)
--
-- `side = both`: the server finds the guardians, keeps the waves' time and
-- burns; a player's machine draws the bar and the fire.

local KIND = "inferno_heart:guardian"
local NEAR = 40
local WAVE_EVERY, WAVE_LOW = 12.0, 8.0
local FLIGHT = 1.3                     -- the ball in the air: the warning
local WAVE_SPEED, WAVE_REACH = 4.5, 16.0
local WALL = 1.2                       -- how high the wall is drawn
local WAVE_DAMAGE = 5
local BAND = 0.9          -- how thick the wave is, either side of its edge
local OVER = 1.0          -- feet this far over the guardian's are over the wave

local side = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local heard: (string) -> () = function(_) end
local function tell(player: number, text: string)
    if drawing then
        heard(text)
    else
        core.net.send(player, text)
    end
end

if serving then
    core.net.open()

    type Wave = { x: number, y: number, z: number, t: number, burnt: { [number]: boolean } }   -- x y z: where it lands
    type Boss = { next: number, wave: Wave? }
    local bosses: { [number]: Boss } = {}
    local told: { [number]: string } = {}   -- player -> the last "hp" line they were sent
    local clock, every = 0.0, 0.0

    core.event.on("core:tick", function(ev)
        local dt = ev.dt :: number
        clock += dt
        every += dt

        -- The waves on their way, every tick: who they pass.
        for id, b in bosses do
            local w = b.wave
            if w == nil then continue end
            local r = (clock - w.t - FLIGHT) * WAVE_SPEED
            if r > WAVE_REACH then
                b.wave = nil
            elseif r > 0 then
                for _, p in core.players() do
                    local x, y, z = core.player_pos(p)
                    if x ~= nil and not w.burnt[p] then
                        local d = math.sqrt((x - w.x) ^ 2 + ((z :: number) - w.z) ^ 2)
                        if math.abs(d - r) < BAND and (y :: number) - w.y < OVER then
                            w.burnt[p] = true
                            core.damage(p, WAVE_DAMAGE)
                        end
                    end
                end
            end
        end

        if every < 0.25 then return end
        every = 0.0

        -- The guardians near somebody, and each one's bar to those near it.
        local seen: { [number]: boolean } = {}
        local players = core.players()
        for _, p in players do
            local x, y, z = core.player_pos(p)
            local line = "hp"
            if x ~= nil then
                for _, id in core.mob_ids(x, y :: number, z :: number, NEAR) do
                    if core.mob_kind(id) == KIND then
                        local hp, whole = core.mob_life(id)
                        if hp ~= nil then
                            seen[id] = true
                            line = `hp {hp} {whole}`
                            if bosses[id] == nil then bosses[id] = { next = clock + WAVE_EVERY * 0.6 } end
                            break
                        end
                    end
                end
            end
            if told[p] ~= line then
                told[p] = line
                tell(p, line)
            end
        end
        for id in bosses do
            if not seen[id] then bosses[id] = nil end
        end
        for p in told do
            if not table.find(players, p) then told[p] = nil end
        end

        -- A throw: at the nearest player, landing short of them on their floor.
        for id, b in bosses do
            if b.wave ~= nil or clock < b.next then continue end
            local gx, gy, gz = core.mob_pos(id)
            if gx == nil then continue end
            local best, bd, tx, ty, tz = nil, math.huge, 0, 0, 0
            for _, p in players do
                local x, y, z = core.player_pos(p)
                if x ~= nil then
                    local d = (x - gx) ^ 2 + ((z :: number) - (gz :: number)) ^ 2
                    if d < bd then best, bd, tx, ty, tz = p, d, x, y :: number, z :: number end
                end
            end
            if best == nil or bd > NEAR * NEAR then continue end
            -- Where they stand now: 1.3 s to get out of the mark, or to jump as
            -- it lands. (Landing short of them put it at the guardian's feet
            -- whenever they fought close, 2026-11-28.)
            local lx, lz = tx, tz
            local hp, whole = core.mob_life(id)
            local low = hp ~= nil and (hp :: number) * 2 < (whole :: number)
            b.next = clock + FLIGHT + WAVE_REACH / WAVE_SPEED + (if low then WAVE_LOW else WAVE_EVERY)
            b.wave = { x = lx, y = ty, z = lz, t = clock, burnt = {} }
            for _, p in players do
                local x, _, z = core.player_pos(p)
                if x ~= nil and (x - gx) ^ 2 + ((z :: number) - (gz :: number)) ^ 2 < NEAR * NEAR then
                    tell(p, string.format("wave %.1f %.1f %.1f %.1f %.1f %.1f", gx, (gy :: number) + 2, gz :: number,
                                          lx, ty, lz))
                end
            end
        end
    end)
end

if drawing then
    local hp, whole = -1, 1
    local clock = 0.0
    local waves: { { number } } = {}   -- from x y z, lands x y z, when it was told

    heard = function(text: string)
        local w = string.split(text, " ")
        if w[1] == "hp" then
            hp, whole = tonumber(w[2]) or -1, math.max(1, tonumber(w[3]) or 1)
        elseif w[1] == "wave" then
            local v = {}
            for i = 2, 7 do v[i - 1] = tonumber(w[i]) or 0 end
            v[7] = clock
            table.insert(waves, v)
        end
    end
    if not serving then
        core.event.on("core:net_message", function(ev) heard(ev.text :: string) end)
    end
    core.event.on("core:tick", function(ev) clock += ev.dt :: number end)

    core.render.pass("inferno_boss:wave", 100, function()
        local left = {}
        for _, v in waves do
            local fx, fy, fz, lx, ly, lz, t0 = v[1], v[2], v[3], v[4], v[5], v[6], v[7]
            local t = clock - t0
            if t < FLIGHT then
                -- The ball in its arc, a trail behind it, and where it will land.
                local u = t / FLIGHT
                local function at(q: number): (number, number, number)
                    return fx + (lx - fx) * q, fy + (ly - fy) * q + 4 * q * (1 - q), fz + (lz - fz) * q
                end
                local x, y, z = at(u)
                core.render.fx("flame", x, y - 0.6, z, 1.8, 255, 140, 40, 255, (t * 2) % 1)
                core.render.fx("glow", x, y, z, 3.0, 255, 110, 30, 220, 0.5)
                for k = 1, 6 do
                    local q = math.max(0, u - k * 0.035)
                    local ax, ay, az = at(q)
                    core.render.fx("flame", ax, ay - 0.4, az, 1.3 - k * 0.15, 255, 120, 30, 220 - k * 30,
                                   (t * 2 + k * 0.2) % 1)
                end
                -- The mark where it will land, growing brighter.
                core.render.fx("ring", lx, ly + 0.05, lz, 1.5, 255, 90, 20, math.floor(80 + 170 * u), u)
                core.render.fx("glow", lx, ly + 0.1, lz, 1.2, 255, 80, 20, math.floor(60 + 120 * u), 0.5)
                table.insert(left, v)
            else
                local r = (t - FLIGHT) * WAVE_SPEED
                if r <= WAVE_REACH then
                    if r < 1.5 then
                        core.render.fx("sparks", lx, ly + 0.3, lz, 2.0, 255, 160, 60, 255, r / 1.5)
                    end
                    local fade = 1 - r / WAVE_REACH * 0.5
                    core.render.fx("ring", lx, ly + 0.05, lz, r, 255, 110, 30, math.floor(200 * fade), (t * 2) % 1)
                    -- The wall: flames all round, two high up to WALL.
                    local n = math.max(10, math.floor(r * 3))
                    for k = 0, n - 1 do
                        local a = k / n * 2 * math.pi
                        local x, z = lx + math.cos(a) * r, lz + math.sin(a) * r
                        core.render.fx("flame", x, ly + 0.35, z, 0.6, 255, 120, 30, math.floor(230 * fade),
                                       (t * 1.7 + k * 0.13) % 1)
                        core.render.fx("flame", x, ly + WALL - 0.3, z, 0.45, 255, 170, 60, math.floor(180 * fade),
                                       (t * 1.3 + k * 0.29) % 1)
                    end
                    table.insert(left, v)
                end
            end
        end
        waves = left
    end)

    core.hud.pass("inferno_boss:bar", 100, function()
        if hp < 0 then return end
        local f = math.max(0, hp / whole)
        core.hud.rect(0.5, 0.06, 0.5, 0.025, 40, 20, 10, 220)
        core.hud.rect(0.5 + 0.25 * (f - 1) / core.hud.aspect(), 0.06, 0.5 * f, 0.02, 240, 100, 30, 255)
        core.hud.text(0.5, 0.1, "Guardian of the Heart", 0.03)
    end)
end
