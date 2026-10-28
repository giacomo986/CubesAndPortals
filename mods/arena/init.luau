--!strict
-- arena: a scored fight between players, in a ring round a GONG.
--
-- A gong (gold round iron) stands anywhere; one is put up once by the
-- spawn's square, beside the chess table. Right click it: the standing --
-- rounds won and knock-downs, kept for good -- and, while no round is on,
-- "Strike the gong": a round of three minutes begins for everybody in the
-- ring (10 round the gong, 6 up or down) and whoever walks in after.
--
-- In a round a player knocked down by another inside the ring is the
-- other's point; the engine puts the fallen back on their feet with their
-- bag (it is never spilled) and this mod puts them back on the ring's edge.
-- Every half minute the score is said to those in the ring; at the end the
-- winner is named to everybody near, and the standing grows.

local GONG = core.register_node("arena:gong", 200, 160, 60, true, 4, "upright", 0, 3.0, "stone")
core.node_box(GONG, 0.1, 0, 0.4, 0.2, 1, 0.6)
core.node_box(GONG, 0.8, 0, 0.4, 0.9, 1, 0.6)
core.node_box(GONG, 0.1, 0.9, 0.4, 0.9, 1, 0.6)
core.node_box(GONG, 0.22, 0.2, 0.46, 0.78, 0.76, 0.54)
core.register_model("arena:gong_held", "gong_held.gltf")   -- tools/make_held_models.py
core.recipe_shaped("arena:gong", 1,
    "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot",
    "furnace:iron_ingot furnace:gold_ingot furnace:iron_ingot",
    "furnace:iron_ingot -                  furnace:iron_ingot")

local RING, UP = 10, 6
local ROUND = 180   -- seconds

type Round = { x: number, y: number, z: number, left: number, said: number, score: { [string]: number } }
local rounds: { [string]: Round } = {}
local back: { { any } } = {}   -- the fallen, to be put on the ring's edge: player, round, ticks

-- The standing: "name=wins,kills;..."
local function standing(): { [string]: { number } }
    local out = {}
    for part in string.gmatch(core.storage_get("standing"), "[^;]+") do
        local n, w, k = string.match(part, "^(.+)=(%d+),(%d+)$")
        if n then
            out[n] = { tonumber(w) :: number, tonumber(k) :: number }
        end
    end
    return out
end
local function keep(t: { [string]: { number } })
    local parts = {}
    for n, v in t do
        table.insert(parts, `{n}={v[1]},{v[2]}`)
    end
    core.storage_set("standing", table.concat(parts, ";"))
end

local function inside(p: number, r: Round): boolean
    local x, y, z = core.player_pos(p)
    return x ~= nil and (x - r.x - 0.5) ^ 2 + (z - r.z - 0.5) ^ 2 <= RING * RING and math.abs(y - r.y) <= UP
end

local function in_ring(r: Round): { number }
    local out = {}
    for _, p in core.players() do
        if inside(p, r) then
            table.insert(out, p)
        end
    end
    return out
end

local function said(r: Round): string
    local list = {}
    for n, k in r.score do
        table.insert(list, { n, k })
    end
    table.sort(list, function(a, b) return a[2] > b[2] end)
    local parts = {}
    for i, e in list do
        if i > 5 then
            break
        end
        table.insert(parts, `{e[1]} {e[2]}`)
    end
    return if #parts > 0 then table.concat(parts, ", ") else "nobody yet"
end

local function show(p: number, key: string)
    local name = `arena:panel{p}`
    local t = standing()
    local list = {}
    for n, v in t do
        table.insert(list, { n, v[1], v[2] })
    end
    table.sort(list, function(a, b) return a[2] > b[2] or (a[2] == b[2] and a[3] > b[3]) end)
    local rows = math.min(#list, 8)
    core.ui_define(name, 12, 4.4 + math.max(rows, 1) * 0.8)
    core.ui_label(name, 0.4, 0.3, "The arena's standing: rounds won, knock-downs.")
    for i = 1, rows do
        core.ui_label(name, 0.6, 0.4 + i * 0.8, `{i}. {list[i][1]}: {list[i][2]} won, {list[i][3]} down`)
    end
    if rows == 0 then
        core.ui_label(name, 0.6, 1.2, "Nobody has fought here yet.")
    end
    local y = 1.4 + math.max(rows, 1) * 0.8
    local r = rounds[key]
    if r then
        core.ui_label(name, 0.4, y, `A round is on: {math.ceil(r.left)} s left -- {said(r)}`)
    else
        core.ui_button(name, `start {key}`, 0.4, y, 11.2, 1.1, "Strike the gong: a round of three minutes")
    end
    core.ui_button(name, "close", 0.4, y + 1.3, 11.2, 1.0, "Close")
    core.ui_show(name, true, p)
end

core.event.on("core:used", function(ev)
    if ev.id ~= GONG then
        return
    end
    ev.cancel = true
    show(ev.player, `{ev.x},{ev.y},{ev.z}`)
end)

core.event.on("core:ui_click", function(ev)
    if ev.panel ~= `arena:panel{ev.player}` then
        return
    end
    core.ui_show(ev.panel, false, ev.player)
    local key = string.match(ev.id, "^start (.+)$")
    if key == nil or rounds[key] then
        return
    end
    local x, y, z = string.match(key, "^(%-?%d+),(%-?%d+),(%-?%d+)$")
    local r: Round = { x = tonumber(x) :: number, y = tonumber(y) :: number, z = tonumber(z) :: number,
                       left = ROUND, said = ROUND, score = {} }
    rounds[key] = r
    core.play_sound("arena:gong", r.x + 0.5, r.y + 0.5, r.z + 0.5)
    for _, p in core.players() do
        local px, _, pz = core.player_pos(p)
        if px and (px - r.x) ^ 2 + (pz - r.z) ^ 2 < 48 * 48 then
            core.notice(p, "The gong: a round of three minutes in the ring round it!")
        end
    end
    core.log(`arena: a round at {key}, struck by {core.player_name(ev.player)}`)
end)

-- A knock-down in a ring during a round: the striker's point.
core.event.on("core:hit", function(ev)
    if not ev.killed then
        return
    end
    local victim = core.body_player(ev.target)
    if victim == 0 or victim == ev.player then
        return
    end
    for _, r in rounds do
        if inside(ev.player, r) then
            local who = core.player_name(ev.player)
            r.score[who] = (r.score[who] or 0) + 1
            core.notice(ev.player, `Down! {r.score[who]} this round`)
            core.notice(victim, `Knocked down by {who}`)
            table.insert(back, { victim, r, 2 })
            return
        end
    end
end)

local since = 0
core.event.on("core:tick", function(ev)
    -- The fallen back on the ring's edge, once the engine has stood them up.
    for i = #back, 1, -1 do
        local b = back[i]
        b[3] -= 1
        if b[3] <= 0 then
            local r = b[2] :: Round
            local a = math.random() * 2 * math.pi
            local x, z = r.x + 0.5 + math.cos(a) * (RING - 1), r.z + 0.5 + math.sin(a) * (RING - 1)
            core.player_place(b[1], x, r.y, z, r.x + 0.5 - x, 0, r.z + 0.5 - z)
            table.remove(back, i)
        end
    end
    since += ev.dt
    if since < 1 then
        return
    end
    local dt = since
    since = 0
    for key, r in rounds do
        r.left -= dt
        if r.said - r.left >= 30 and r.left > 1 then
            r.said = r.left
            for _, p in in_ring(r) do
                core.notice(p, `{math.ceil(r.left)} s left -- {said(r)}`)
            end
        end
        if r.left <= 0 then
            rounds[key] = nil
            local t = standing()
            local best, top = {}, 0
            for n, k in r.score do
                t[n] = t[n] or { 0, 0 }
                t[n][2] += k
                if k > top then
                    best, top = { n }, k
                elseif k == top and k > 0 then
                    table.insert(best, n)
                end
            end
            local line
            if top == 0 then
                line = "The round is over: nobody was knocked down."
            else
                for _, n in best do
                    t[n][1] += 1
                end
                line = `The round is over: {table.concat(best, " and ")} won with {top} -- {said(r)}`
            end
            keep(t)
            core.play_sound("arena:gong", r.x + 0.5, r.y + 0.5, r.z + 0.5)
            for _, p in core.players() do
                local px, _, pz = core.player_pos(p)
                if px and (px - r.x) ^ 2 + (pz - r.z) ^ 2 < 48 * 48 then
                    core.notice(p, line)
                end
            end
            core.log(`arena: {line}`)
        end
    end
end)

-- One by the spawn's square, once a world, across from the chess table.
local laid = core.storage_get("plaza") == "1"
core.event.on("core:tick", function()
    if laid then
        return
    end
    local sx, sy, sz = core.world_spawn()
    if sx == nil then
        return
    end
    local x0, y, z0 = math.floor(sx) - 4, math.floor(sy), math.floor(sz) - 4
    if core.loaded(x0, y, z0) == 0 then
        return
    end
    laid = true
    core.storage_set("plaza", "1")
    for _, d in { { 0, 0 }, { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } } do
        local x, z = x0 + d[1], z0 + d[2]
        if core.get_block(x, y, z) == 0 and core.get_block(x, y + 1, z) == 0 and core.get_block(x, y - 1, z) ~= 0 then
            core.set_block(x, y, z, GONG)
            core.log(`arena: a gong by the spawn's square at {x} {y} {z}`)
            return
        end
    end
end)
