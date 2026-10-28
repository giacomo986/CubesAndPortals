--!strict
-- fireworks: rockets that whistle up and burst in colour over a place.
--
-- A mod sets a show off on the server: core.event.emit("fireworks:show",
-- { x, y, z, seconds }) -- the villages' harvest feast (mods/villagers) does
-- in its evening. Every second or so a rocket goes up from near x, z: the
-- server picks where it bursts and its colour, plays the whistle and the bang
-- (whistle.ogg, bang.ogg: tools/make_village_sounds.py) and tells the players
-- near, "r x y z h r g b"; each client draws it rising (a glow) and its burst
-- (sparks, a glow, and stars falling away), so everyone sees the same show.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"
local RISE, BURST = 1.2, 1.8   -- seconds up, and the burst's
local clock = 0.0
-- This machine's rockets: x, y, z, height, r, g, b, launched at.
local rockets: { { number } } = {}
local function heard(text: string)
    local f = string.split(text, " ")
    if f[1] == "r" and #f == 8 then
        local r = {}
        for i = 2, 8 do
            r[i - 1] = tonumber(f[i]) or 0
        end
        r[8] = clock
        table.insert(rockets, r)
    end
end

core.event.define("fireworks:show", {
    fields = { x = "number", y = "number", z = "number", seconds = "number" },
    cancellable = false,
})

if serving then
    core.net.open()
    local COLOURS = { { 255, 80, 60 }, { 255, 200, 60 }, { 90, 200, 255 }, { 120, 255, 120 }, { 230, 110, 255 },
                      { 255, 255, 255 }, { 255, 140, 40 } }
    type Show = { x: number, y: number, z: number, left: number, next: number }
    local shows: { Show } = {}
    local bangs: { { number } } = {}   -- x, y, z, when
    local now = 0.0
    core.event.on("fireworks:show", function(ev)
        table.insert(shows, { x = ev.x, y = ev.y, z = ev.z, left = ev.seconds, next = 0 })
    end)
    core.event.on("core:tick", function(ev)
        now += ev.dt
        for i = #shows, 1, -1 do
            local s = shows[i]
            s.left -= ev.dt
            s.next -= ev.dt
            if s.left <= 0 then
                table.remove(shows, i)
            elseif s.next <= 0 then
                s.next = 0.6 + math.random() * 1.2
                local x, z = s.x + math.random(-6, 6) + 0.5, s.z + math.random(-6, 6) + 0.5
                local h = 16 + math.random() * 10
                local c = COLOURS[math.random(#COLOURS)]
                local text = `r {x} {s.y} {z} {math.floor(h * 10) / 10} {c[1]} {c[2]} {c[3]}`
                for _, p in core.players() do
                    local px, _, pz = core.player_pos(p)
                    if px and pz and (px - x) ^ 2 + (pz - z) ^ 2 < 192 * 192 then
                        if drawing then
                            heard(text)
                        else
                            core.net.send(p, text)
                        end
                    end
                end
                core.play_sound("fireworks:whistle", x, s.y, z)
                table.insert(bangs, { x, s.y + h, z, now + RISE })
            end
        end
        for i = #bangs, 1, -1 do
            local b = bangs[i]
            if now >= b[4] then
                core.play_sound("fireworks:bang", b[1], b[2], b[3])
                table.remove(bangs, i)
            end
        end
    end)
end

if drawing then
    if not serving then
        core.event.on("core:net_message", function(ev)
            heard(ev.text)
        end)
    end
    core.event.on("core:tick", function(ev)
        clock += ev.dt
    end)
    -- The stars of a burst: 28 directions, the same for every burst.
    local DIRS: { { number } } = {}
    for k = 1, 28 do
        local y = 1 - 2 * (k - 0.5) / 28
        local r = math.sqrt(1 - y * y)
        local a = k * 2.39996
        table.insert(DIRS, { math.cos(a) * r, y, math.sin(a) * r })
    end
    core.render.pass("fireworks:rockets", 70, function()
        for i = #rockets, 1, -1 do
            local r = rockets[i]
            local t = clock - r[8]
            if t > RISE + BURST then
                table.remove(rockets, i)
                continue
            end
            local x, y, z, h = r[1], r[2], r[3], r[4]
            if t < RISE then
                local k = t / RISE
                local up = y + h * (1 - (1 - k) ^ 2)
                core.render.fx("glow", x, up, z, 0.35, 255, 220, 160, 255, 0.5)
                core.render.box(x, up - 0.4, z, 0.06, 0.6, 0.06, 255, 200, 120, 200)
            else
                local k = (t - RISE) / BURST
                local top = y + h
                core.render.fx("sparks", x, top, z, 5 + 3 * k, r[5], r[6], r[7], 255, k)
                if k < 0.4 then
                    core.render.fx("glow", x, top, z, 4, r[5], r[6], r[7], 255, k / 0.4)
                end
                local out = 7 * (1 - (1 - k) ^ 2)
                local fall = 2.5 * k * k
                local a = math.floor(255 * (1 - k))
                for _, d in DIRS do
                    core.render.box(x + d[1] * out, top + d[2] * out - fall, z + d[3] * out, 0.18, 0.18, 0.18,
                        r[5], r[6], r[7], a)
                end
            end
        end
    end)
end
