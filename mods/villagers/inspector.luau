--!strict
-- villagers: the inspector and the bag: stolen goods, trust, and the lives set with the lots and chests. Part of the mod (init.luau says the order).

local common = require("./common")
local types = require("./types")
type V = types.V
local act = common.act
local age = common.age
local AID = common.AID
local bed_mark = common.bed_mark
local HOLDS = common.HOLDS
local id = common.id
local is_night = common.is_night
local KEEPING = common.KEEPING
local nearest = common.nearest
local patrol = common.patrol
local people = common.people
local save_trust = common.save_trust
local standable = common.standable
local stolen_count = common.stolen_count
local tag_of = common.tag_of
local trust = common.trust
local trust_of = common.trust_of
local village_at = common.village_at
local wait = common.wait
local walk = common.walk
local watch = common.watch
local where = common.where

-- ------------------------------------------------ the inspector and the bag

local CAUGHT = "villagers:caught"
core.ui_define(CAUGHT, 12, 4.6)
core.ui_label(CAUGHT, 0.4, 0.3, "The inspector found stolen goods in your bag.")
core.ui_label(CAUGHT, 0.4, 1.1, "Give them back? The village will not trust you again.")
core.ui_button(CAUGHT, "back", 0.4, 2.4, 5.4, 1.4, "Give them back")
core.ui_button(CAUGHT, "keep", 6.2, 2.4, 5.4, 1.4, "Refuse")

-- Who is being asked, by which village, until when.
local asked: { [number]: { rk: string, by: V, til: number } } = {}
local looked: { [string]: number } = {}   -- "rk name": not looked in again before

local function turn_hostile(p: number, rk: string)
    local t = trust_of(rk, core.player_name(p))
    t.strikes, t.hostile, t.last = math.max(t.strikes, 2), true, age()
    save_trust(rk)
    core.notice(p, "Thief! The watch is after you while you are in this village.")
end

-- What was stolen, out of the bag and into the chest nearest the inspector.
local function give_back(p: number, v: V)
    local tag = tag_of(v.rk :: string)
    local list = core.bag_tagged(p, tag)
    core.take_tagged(p, tag)
    local px, py, pz = core.mob_pos(v.id)
    local x, y, z = math.floor(px or v.hx), math.floor(py or v.hy), math.floor(pz or v.hz)
    local cx, cy, cz = nearest({ "chest:box" }, x, y, z, 40, 8)
    for i = 1, #list, 2 do
        local left = list[i + 1]
        if cx ~= nil and cy ~= nil and cz ~= nil then
            left = core.container_add(cx, cy, cz, list[i], left)
        end
        if left > 0 then
            core.drop_item(cx or x, (cy or y) + 1, cz or z, list[i], left)
        end
    end
end

core.event.on("core:ui_click", function(ev)
    local a = asked[ev.player]
    if ev.panel ~= CAUGHT or a == nil then
        return
    end
    asked[ev.player] = nil
    core.ui_show(CAUGHT, false, ev.player)
    if ev.id == "back" then
        give_back(ev.player, a.by)
        local t = trust_of(a.rk, core.player_name(ev.player))
        t.strikes, t.last = math.max(t.strikes, 1), age()
        save_trust(a.rk)
        core.notice(ev.player, "You gave it all back. This village does not trust you any more.")
    else
        turn_hostile(ev.player, a.rk)
    end
end)

local function inspect(v: V, p: number)
    local rk = v.rk :: string
    local name = core.player_name(p)
    local t = trust_of(rk, name)
    looked[`{rk} {name}`] = common.clock + (if t.strikes > 0 then 40 else 150)
    act(v, "gather", "", 1.6, nil, nil)
    local n = stolen_count(p, rk)
    if n == 0 then
        core.notice(p, if t.strikes > 0 then "The inspector looks through your bag, slowly. Nothing, this time."
            else "The inspector looks through your bag: all in order.")
    elseif t.strikes == 0 then
        core.notice(p, `The inspector found {n} stolen thing{if n == 1 then "" else "s"} on you.`)
        asked[p] = { rk = rk, by = v, til = common.clock + 25 }
        core.ui_show(CAUGHT, true, p)
        while asked[p] ~= nil and common.clock < (asked[p] :: any).til do
            wait(v, 0.5)
        end
        if asked[p] ~= nil then   -- no answer is an answer
            asked[p] = nil
            core.ui_show(CAUGHT, false, p)
            turn_hostile(p, rk)
        end
    else
        turn_hostile(p, rk)
    end
end

-- The next player in the village it has not looked at lately.
local function to_inspect(v: V): number?
    local px, _, pz = core.mob_pos(v.id)
    if px == nil or pz == nil then
        return nil
    end
    local best, who = 32 * 32, nil
    for _, p in core.players() do
        local x, _, z = where(p)
        local name = core.player_name(p)
        if x ~= nil and z ~= nil and village_at(x, z) == v.rk and (looked[`{v.rk} {name}`] or 0) < common.clock
            and asked[p] == nil and not trust_of(v.rk :: string, name).hostile then
            local d = (x - px) ^ 2 + (z - pz) ^ 2
            if d < best then
                best, who = d, p
            end
        end
    end
    return who
end

common.builder_life = patrol   -- set below, with the lots
common.crafter_life = patrol   -- set below, with the chests

-- When one of the keepers sleeps: at night, but the guards and archers in
-- two shifts -- the odd ones by day, the even ones by night -- so the
-- village is watched round the clock.
local function sleeps_now(v: V): boolean
    local night = is_night()
    if v.kind == "guard" or v.kind == "archer" then
        if AID.ev.siege[v.rk :: string] then
            return false   -- all of the watch up for a siege
        end
        local i = tonumber(string.match(v.key, "(%d+)$")) or 1
        return (i % 2 == 0) == night
    end
    return night
end

-- In its bed while its sleep lasts. False with no bed to go to.
local function rest(v: V): boolean
    local b = v.bed
    if b == nil or not walk(v, b[1], b[2], b[3], 40) then
        return false
    end
    local px, py, pz = nearest({ "bed:bed_head" }, b[1], b[2], b[3], 1, 0)
    if px ~= nil and py ~= nil and pz ~= nil then
        core.mob_face(v.id, px + 0.5, pz + 0.5)
    end
    core.mob_act(v.id, "lie", "")
    v.sleeping = true
    while sleeps_now(v) do
        wait(v, 5)
    end
    v.sleeping = false
    core.mob_act(v.id, "", HOLDS[v.kind] or "")
    return true
end

common.watch_life = function(v: V)
    if v.kind == "builder" then
        common.builder_life(v)
        return
    end
    if v.kind == "crafter" then
        common.crafter_life(v)
        return
    end
    if v.kind == "mayor" or v.kind == "merchant" then
        -- By the door of the house at the village's middle, to be found.
        while true do
            if sleeps_now(v) and rest(v) then
                continue
            end
            local x, y, z = v.hx, v.hy, v.hz
            for r = 1, 3 do
                for _, d in { { r, 0 }, { -r, 0 }, { 0, r }, { 0, -r } } do
                    if standable(v.hx + d[1], v.hy, v.hz + d[2]) then
                        x, z = v.hx + d[1], v.hz + d[2]
                    end
                end
            end
            walk(v, x, y, z, 20)
            wait(v, 8 + math.random() * 8)
        end
    end
    while true do
        if sleeps_now(v) and rest(v) then
            continue
        end
        local p = if v.kind == "inspector" then to_inspect(v) else nil
        if p ~= nil then
            -- Up to them, a few goes, as they may be walking off.
            for _ = 1, 4 do
                local x, y, z = where(p)
                local mx, _, mz = core.mob_pos(v.id)
                if x == nil or y == nil or z == nil or mx == nil or mz == nil then
                    break
                end
                if (x - mx) ^ 2 + (z - mz) ^ 2 < 2.6 * 2.6 then
                    core.mob_face(v.id, x, z)
                    inspect(v, p)
                    break
                end
                walk(v, math.floor(x), math.floor(y + 0.01), math.floor(z), 4)
            end
        elseif v.kind == "inspector" and is_night() then
            walk(v, v.hx, v.hy, v.hz, 40)
            wait(v, 10)
        else
            patrol(v)
        end
        wait(v, 0.5)
    end
end

-- A village's watch, at the home nearest its middle: more in a town.
local function muster()
    for rk, b in common.villages do
        if watch[`{rk} inspector 1`] == nil then
            local mx, mz = (b.x1 + b.x2) / 2, (b.z1 + b.z2) / 2
            local post, best = b.homes[1], math.huge
            for _, h in b.homes do
                local d = (h.hx - mx) ^ 2 + (h.hz - mz) ^ 2
                if d < best then
                    post, best = h, d
                end
            end
            local big = #b.homes >= 8
            core.log(`villagers: {rk}: watch, mayor and builder at {post.hx} {post.hy} {post.hz}`)
            for kind, n in { guard = if big then 3 else 2, archer = if big then 2 else 1, inspector = if big then 2 else 1,
                             mayor = 1, builder = 1, merchant = 1, crafter = 1 } do
                for i = 1, n do
                    local key = `{rk} {kind} {i}`
                    watch[key] = { key = key, hx = post.hx, hy = post.hy, hz = post.hz, kind = kind, id = 0,
                                   back_at = 0, inv = {}, fed_at = 0, wake = 0, walking = false,
                                   tx = 0, ty = 0, tz = 0, tried = {}, rk = rk }
                end
            end
        elseif watch[`{rk} crafter 1`] == nil then
            -- A village mustered before there were crafters gets one.
            local m = watch[`{rk} mayor 1`] or watch[`{rk} inspector 1`]
            local key = `{rk} crafter 1`
            watch[key] = { key = key, hx = m.hx, hy = m.hy, hz = m.hz, kind = "crafter", id = 0,
                           back_at = 0, inv = {}, fed_at = 0, wake = 0, walking = false,
                           tx = 0, ty = 0, tz = 0, tried = {}, rk = rk }
        end
    end
end

-- A bed for each of a village's keepers: the beds of its ground that are
-- nobody's home and no player's, the hall's first, the nearest its middle
-- next; a bed broken is given up. Not kept: worked out again as it loads.
local KEEPERS = { "mayor", "builder", "merchant", "crafter", "inspector", "guard", "archer" }
local beds_looked: { [string]: number } = {}
-- Returns the signs a keeper found no bed under: the buildings missing.
local function bed_keepers(rk: string): { [string]: boolean }
    local b = common.villages[rk]
    local missing: { [string]: boolean } = {}
    local taken: { [string]: boolean } = {}
    local keepers: { V } = {}
    for _, kind in KEEPERS do
        for i = 1, 3 do
            local v = watch[`{rk} {kind} {i}`]
            if v ~= nil then
                local bd = v.bed
                if bd ~= nil and core.loaded(bd[1], bd[2], bd[3]) == 1
                    and core.get_block(bd[1], bd[2], bd[3]) ~= id("bed:bed") then
                    v.bed, bd = nil, nil
                end
                if bd ~= nil then
                    taken[`{bd[1]},{bd[2]},{bd[3]}`] = true
                    if v.bed_mark == nil then
                        v.bed_mark = bed_mark(bd[1], bd[2], bd[3]) or ""
                    end
                end
                table.insert(keepers, v)
            end
        end
    end
    if b == nil then
        return missing
    end
    local mx, mz = (b.x1 + b.x2) / 2, (b.z1 + b.z2) / 2
    local free = {}
    local f = AID.find(b.x1 - KEEPING.reach, b.y - 12, b.z1 - KEEPING.reach, b.x2 + KEEPING.reach, b.y + 16, b.z2 + KEEPING.reach, "bed:bed")
    for i = 1, #f, 3 do
        local x, y, z = f[i], f[i + 1], f[i + 2]
        local key = `{x},{y},{z}`
        local owner = core.owned_at(x, z)
        if not people[key] and not taken[key] and not (owner >= 32768 and owner < 65536) then
            local m = bed_mark(x, y, z) or ""
            table.insert(free, { x, y, z, (if m ~= "" then 0 else 1e6) + (x - mx) ^ 2 + (z - mz) ^ 2, m })
        end
    end
    table.sort(free, function(a, c) return a[4] < c[4] end)
    local function take_bed(v: V, k: number)
        local fb = table.remove(free, k)
        if v.bed ~= nil then
            -- Its old bed is somebody else's to have.
            local o = v.bed
            table.insert(free, { o[1], o[2], o[3], 1e9, v.bed_mark or "" })
        end
        v.bed, v.bed_mark = { fb[1], fb[2], fb[3] }, fb[5]
    end
    -- First a bed under its own sign, moving out of another's; then, while
    -- its own building is missing, whatever bed there is.
    for _, v in keepers do
        local want = KEEPING.role[v.kind]
        if v.bed_mark ~= want then
            local k = nil
            for n, fb in free do
                if fb[5] == want then
                    k = n
                    break
                end
            end
            if k ~= nil then
                take_bed(v, k)
            else
                missing[want] = true
            end
        end
    end
    -- An old hall's beds first (there are fifteen), then beds under no
    -- sign, and another keeper's last.
    local function rank(fb): number
        return if fb[5] == "villages:bell" then 0 elseif fb[5] == "" then 1 else 2
    end
    for _, v in keepers do
        if v.bed == nil and #free > 0 then
            local k = 1
            for n, fb in free do
                if rank(fb) < rank(free[k]) then
                    k = n
                end
            end
            take_bed(v, k)
        end
    end
    return missing
end

-- What a player takes here is marked: told the first time it is.
local carried: { [string]: number } = {}
local function notice_theft()
    for _, p in core.players() do
        local x, _, z = where(p)
        local rk = if x ~= nil and z ~= nil then village_at(x, z) else nil
        if rk ~= nil then
            local n = stolen_count(p, rk)
            if n > (carried[`{p} {rk}`] or 0) then
                core.notice(p, "Stolen: what you took from this village is marked, and its inspector will know it.")
            end
            carried[`{p} {rk}`] = n
        end
    end
end

-- What comes out of the dark into a village (or near it): the nearest to x, z.
local NEAR = 8
local function monster_in(rk: string, x: number, y: number, z: number): number?
    local b = common.villages[rk]
    if b == nil then
        return nil
    end
    for _, m in core.mob_ids(x, y, z, 40) do
        local mx, _, mz = core.mob_pos(m)
        if mx ~= nil and mz ~= nil and mx >= b.x1 - NEAR and mx <= b.x2 + NEAR and mz >= b.z1 - NEAR
            and mz <= b.z2 + NEAR then
            return m
        end
    end
    -- And a siege's raiders (workers, which core.mob_ids leaves out).
    for m, r in AID.ev.raiders do
        local mx, _, mz = core.mob_pos(m)
        if r[1] == rk and mx ~= nil and mz ~= nil and (mx - x) ^ 2 + (mz - z) ^ 2 < 40 * 40 then
            return m
        end
    end
    return nil
end

-- A body hunted that is suddenly far away died and was put back (or went
-- through a portal): either way the watch lets it go.
local last_at: { [number]: { number } } = {}
local function hunted_gone()
    for _, p in core.players() do
        local x, y, z = where(p)
        local was = last_at[p]
        if x ~= nil and y ~= nil and z ~= nil then
            if was ~= nil and (x - was[1]) ^ 2 + (y - was[2]) ^ 2 + (z - was[3]) ^ 2 > 24 * 24 then
                for rk, by in trust do
                    local t = by[core.player_name(p)]
                    if t ~= nil and t.hostile then
                        t.hostile = false
                        save_trust(rk)
                    end
                end
            end
            last_at[p] = { x, y, z }
        end
    end
end

-- What the files after this one use of it.
common.bed_keepers = bed_keepers
common.beds_looked = beds_looked
common.hunted_gone = hunted_gone
common.monster_in = monster_in
common.muster = muster
common.notice_theft = notice_theft
common.rest = rest
common.sleeps_now = sleeps_now
