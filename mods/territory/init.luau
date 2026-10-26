--!strict
-- territory: ground a player owns.
--
-- A BOUNDARY STONE (8 cobblestone round an emerald) founds it: 5x5 cells round
-- the stone, which grow by one cell every 5 seconds while one of its owners is
-- on it -- the free cell on its edge nearest to where they stand, never
-- further than 64 from the stone (128x128 at most). Right click on the stone:
-- its founder sees how big it is, adds the players standing near, takes
-- owners off. Only the founder breaks it, and with it the territory goes.
--
-- On it, its owners walk 10% faster, dig 20% faster, take 15% less from a
-- blow and heal half as fast again; anybody else walks 10% slower, digs 20%
-- slower, takes 15% more and does not heal -- on a village's ground too. What
-- somebody who is not an owner takes there, breaks there or opens there is
-- marked with the territory's number (core.owned_cell): stolen.
--
-- A village sells lots (mods/villagers): `territory:lot` makes a fixed
-- territory, with no stone and no growth, of the player who bought it.
--
-- The cells are the engine's (core.owned_cell, in memory) and the mod's: a
-- key per territory in core.storage, its cells a 128x128 bitmap round where
-- it was founded. Owners are names, as a village's trust is.
--
-- Its owners see its edge as posts of light, green; anybody else's red, a
-- village's yellow: the server sends each player the owners of the ground
-- round them, the client finds the edges and draws them. F4 hides them and
-- shows them again.

local side = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local R = 16                -- the ground round a player whose edges are drawn
local GRID = 2 * R + 1

-- The ground round a player as a string of 2-bit classes, 6 bits a character:
-- 0 nobody's, 1 theirs, 2 somebody else's, 3 a village's.
local ALPHA = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
local function encode(bits: { number }, n: number): string
    local out = table.create(math.ceil(n / 6))
    for i = 0, n - 1, 6 do
        local v = 0
        for b = 0, 5 do
            v = v * 2 + (bits[i + b + 1] or 0)
        end
        table.insert(out, string.sub(ALPHA, v + 1, v + 1))
    end
    return table.concat(out)
end
local function decode(s: string, n: number): { number }
    local bits = table.create(n, 0)
    for i = 1, #s do
        local v = (string.find(ALPHA, string.sub(s, i, i), 1, true) or 1) - 1
        for b = 5, 0, -1 do
            local k = (i - 1) * 6 + (5 - b) + 1
            if k <= n then
                bits[k] = bit32.band(bit32.rshift(v, b), 1)
            end
        end
    end
    return bits
end

if serving then
    core.net.open()
    local STONE = core.register_node("territory:boundary_stone", 120, 130, 120, true, 0, "", 1, 3.0, "stone")
    core.recipe_shaped("territory:boundary_stone", 1,
        "rocks:cobblestone rocks:cobblestone rocks:cobblestone",
        "rocks:cobblestone ore:emerald       rocks:cobblestone",
        "rocks:cobblestone rocks:cobblestone rocks:cobblestone")

    core.event.define("territory:lot", {
        fields = { player = "string", x1 = "number", z1 = "number", x2 = "number", z2 = "number" },
        cancellable = false,
    })
    -- A guard hired from a village's mayor (mods/villagers) for the territory
    -- its player founded: cancelled when they have none, and the mayor gives
    -- the money back.
    core.event.define("territory:hire", {
        fields = { player = "number", days = "number" },
        cancellable = true,
    })

    -- A territory's number: 32768..65535, clear of mods/backpack's (counted up
    -- from 1) and of the villages' (65536..131071).
    local FIRST, LAST = 32768, 65535
    local VILLAGE = 65536
    local REACH = 64
    local START = 2             -- 5x5
    local GROW = 5.0
    local SPAWN_FREE = 24       -- nobody founds this near the world's spawn

    type T = {
        tag: number,
        x: number, y: number, z: number,   -- the stone, or the middle of a lot
        stone: boolean,
        founder: string,
        members: { string },
        cells: { [number]: boolean },
        count: number,
        frontier: { [number]: boolean },
        grow: number,
    }
    local by_tag: { [number]: T } = {}
    local dirty: { [number]: boolean } = {}

    -- A cell's key within its territory: -64..63 each way from the stone.
    local function key(t: T, x: number, z: number): number?
        local dx, dz = x - t.x, z - t.z
        if dx < -REACH or dx >= REACH or dz < -REACH or dz >= REACH then
            return nil
        end
        return (dx + REACH) * (2 * REACH) + (dz + REACH)
    end
    local function cell_of(t: T, k: number): (number, number)
        return t.x + k // (2 * REACH) - REACH, t.z + k % (2 * REACH) - REACH
    end

    local function is_member(t: T, name: string): boolean
        if t.founder == name then
            return true
        end
        for _, m in t.members do
            if m == name then
                return true
            end
        end
        return false
    end

    local function save(t: T)
        local bits = table.create(4 * REACH * REACH, 0)
        for k in t.cells do
            bits[k + 1] = 1
        end
        core.storage_set(`terr {t.tag}`, `{t.x},{t.y},{t.z};{if t.stone then 1 else 0};{t.founder};`
            .. `{table.concat(t.members, ",")};{encode(bits, 4 * REACH * REACH)}`)
    end
    local function save_list()
        local tags = {}
        for tag in by_tag do
            table.insert(tags, tostring(tag))
        end
        table.sort(tags)
        core.storage_set("list", table.concat(tags, ","))
    end

    -- The cells beside the territory that could be its next.
    local function edge_of(t: T, k: number)
        local x, z = cell_of(t, k)
        for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
            local n = key(t, x + d[1], z + d[2])
            if n ~= nil and not t.cells[n] then
                t.frontier[n] = true
            end
        end
    end

    local function add_cell(t: T, x: number, z: number): boolean
        local k = key(t, x, z)
        if k == nil or t.cells[k] then
            return false
        end
        -- Nobody else's, and clear of the spawn; a lot is cut out of its
        -- village's ground.
        local sx, _, sz = core.world_spawn()
        if t.stone and sx ~= nil and sz ~= nil and math.abs(x - sx) <= SPAWN_FREE and math.abs(z - sz) <= SPAWN_FREE then
            return false
        end
        local was = core.owned_at(x, z)
        if was ~= 0 and was ~= t.tag and (t.stone or was < VILLAGE) then
            return false
        end
        t.cells[k] = true
        t.count += 1
        t.frontier[k] = nil
        core.owned_cell(x, z, t.tag)
        edge_of(t, k)
        dirty[t.tag] = true
        return true
    end

    local function new_tag(): number?
        for tag = FIRST, LAST do
            if by_tag[tag] == nil then
                return tag
            end
        end
        return nil
    end

    local function make(tag: number, x: number, y: number, z: number, stone: boolean, founder: string): T
        local t: T = { tag = tag, x = x, y = y, z = z, stone = stone, founder = founder, members = {},
                       cells = {}, count = 0, frontier = {}, grow = 0 }
        by_tag[tag] = t
        return t
    end

    -- Who is one of whose owners, said to the engine: player id -> tag -> name.
    local applied: { [number]: { [number]: boolean } } = {}

    local function apply_members()
        for _, p in core.players() do
            local name = core.player_name(p)
            local a = applied[p]
            if a == nil then
                a = {}
                applied[p] = a
            end
            for tag, t in by_tag do
                local m = is_member(t, name)
                if m ~= (a[tag] == true) then
                    core.owned_member(p, tag, m)
                    a[tag] = if m then true else nil
                end
            end
            for tag in a do
                if by_tag[tag] == nil then
                    a[tag] = nil
                end
            end
        end
    end

    local function dissolve(t: T)
        for k in t.cells do
            local x, z = cell_of(t, k)
            core.owned_cell(x, z, 0)
        end
        by_tag[t.tag] = nil
        dirty[t.tag] = nil
        core.storage_set(`terr {t.tag}`, "")
        save_list()
        for p, a in applied do
            if a[t.tag] then
                core.owned_member(p, t.tag, false)
                a[t.tag] = nil
            end
        end
    end

    local function founded_by(name: string): T?
        for _, t in by_tag do
            if t.stone and t.founder == name then
                return t
            end
        end
        return nil
    end

    -- ------------------------------------------------------------ the stone

    core.event.on("core:pre_place", function(ev)
        if ev.id ~= STONE then
            return
        end
        local name = core.player_name(ev.player)
        -- A name with a key behind it is a player's own; one without, anybody's.
        if core.player_key(ev.player) == "" then
            core.notice(ev.player, "Land is kept under a player's key: join with one to found a territory.")
            ev.cancel = true
            return
        end
        local sx, _, sz = core.world_spawn()
        if sx ~= nil and sz ~= nil and math.abs(ev.x - sx) <= SPAWN_FREE and math.abs(ev.z - sz) <= SPAWN_FREE then
            core.notice(ev.player, "The ground round the world's spawn is everybody's.")
            ev.cancel = true
            return
        end
        if founded_by(name) ~= nil then
            core.notice(ev.player, "You have a boundary stone already: break it to found elsewhere.")
            ev.cancel = true
            return
        end
        for dx = -START - 1, START + 1 do
            for dz = -START - 1, START + 1 do
                if core.owned_at(ev.x + dx, ev.z + dz) ~= 0 then
                    core.notice(ev.player, "This ground is somebody's already.")
                    ev.cancel = true
                    return
                end
            end
        end
        if new_tag() == nil then
            core.notice(ev.player, "No more territories can be founded in this world.")
            ev.cancel = true
        end
    end)

    core.event.on("core:placed", function(ev)
        if ev.id ~= STONE then
            return
        end
        local tag = new_tag() :: number
        local t = make(tag, ev.x, ev.y, ev.z, true, core.player_name(ev.player))
        for dx = -START, START do
            for dz = -START, START do
                add_cell(t, ev.x + dx, ev.z + dz)
            end
        end
        save(t)
        save_list()
        dirty[tag] = nil
        apply_members()
        core.notice(ev.player, "Your territory: it grows while you stay on it.")
    end)

    local function stone_at(x: number, y: number, z: number): T?
        for _, t in by_tag do
            if t.stone and t.x == x and t.y == y and t.z == z then
                return t
            end
        end
        return nil
    end

    core.event.on("core:pre_break", function(ev)
        if ev.id ~= STONE then
            return
        end
        local t = stone_at(ev.x, ev.y, ev.z)
        if t ~= nil and t.founder ~= core.player_name(ev.player) then
            core.notice(ev.player, `This boundary stone is {t.founder}'s.`)
            ev.cancel = true
        end
    end)

    core.event.on("core:broken", function(ev)
        if ev.id ~= STONE then
            return
        end
        local t = stone_at(ev.x, ev.y, ev.z)
        if t ~= nil then
            dissolve(t)
            core.notice(ev.player, "The territory is no more.")
        end
    end)

    -- The founder's panel: one per player, made again each time it opens.
    local open_on: { [number]: T } = {}
    local function panel_of(p: number): string
        return `territory:panel{p}`
    end
    local function show(p: number, t: T)
        local name = panel_of(p)
        core.ui_define(name, 12, 3.6 + #t.members * 1.1)
        core.ui_label(name, 0.4, 0.3, `{t.founder}'s territory: {t.count} cells.`)
        for i, m in t.members do
            local y = 1.0 + (i - 1) * 1.1
            core.ui_label(name, 0.4, y + 0.3, m)
            core.ui_button(name, `rm {i}`, 7.6, y, 4.0, 0.9, "Take off")
        end
        local y = 1.2 + #t.members * 1.1
        core.ui_button(name, "add", 0.4, y, 6.4, 1.4, "Add the players near")
        core.ui_button(name, "close", 7.2, y, 4.4, 1.4, "Close")
        open_on[p] = t
        core.ui_show(name, true, p)
    end

    core.event.on("core:used", function(ev)
        if ev.id ~= STONE then
            return
        end
        local t = stone_at(ev.x, ev.y, ev.z)
        if t == nil then
            return
        end
        ev.cancel = true
        if t.founder ~= core.player_name(ev.player) then
            core.notice(ev.player, `{t.founder}'s territory: {t.count} cells.`)
            return
        end
        show(ev.player, t)
    end)

    core.event.on("core:ui_click", function(ev)
        local t = open_on[ev.player]
        if t == nil or ev.panel ~= panel_of(ev.player) then
            return
        end
        if ev.id == "close" or by_tag[t.tag] ~= t then
            open_on[ev.player] = nil
            core.ui_show(ev.panel, false, ev.player)
            return
        end
        if ev.id == "add" then
            local fx, fy, fz = core.player_pos(ev.player)
            local added = 0
            for _, p in core.players() do
                local x, y, z = core.player_pos(p)
                local name = core.player_name(p)
                if fx and fy and fz and x and y and z and not is_member(t, name) and #t.members < 6
                    and (x - fx) ^ 2 + (y - fy) ^ 2 + (z - fz) ^ 2 <= 64 then
                    table.insert(t.members, name)
                    added += 1
                end
            end
            if added == 0 then
                core.notice(ev.player, "Nobody near who is not an owner (6 at most).")
            end
        else
            local i = tonumber(string.match(ev.id, "^rm (%d+)$") or "")
            if i ~= nil and t.members[i] ~= nil then
                table.remove(t.members, i)
            end
        end
        save(t)
        apply_members()
        show(ev.player, t)
    end)

    -- ------------------------------------------------------------- the lots

    core.event.on("territory:lot", function(ev)
        local tag = new_tag()
        if tag == nil then
            return
        end
        local x1, x2 = math.min(ev.x1, ev.x2), math.max(ev.x1, ev.x2)
        local z1, z2 = math.min(ev.z1, ev.z2), math.max(ev.z1, ev.z2)
        local t = make(tag, (x1 + x2) // 2, 0, (z1 + z2) // 2, false, ev.player)
        for x = x1, x2 do
            for z = z1, z2 do
                add_cell(t, x, z)
            end
        end
        save(t)
        save_list()
        dirty[tag] = nil
        apply_members()
    end)

    -- ------------------------------------------------------ kept and loaded

    core.event.on("core:world_ready", function()
        for part in string.gmatch(core.storage_get("list"), "[^,]+") do
            local tag = tonumber(part)
            local v = if tag then core.storage_get(`terr {tag}`) else ""
            local x, y, z, st, founder, members, bits =
                string.match(v, "^(-?%d+),(-?%d+),(-?%d+);(%d);([^;]*);([^;]*);(.*)$")
            if tag and x and y and z and st and founder and members and bits then
                local t = make(tag, tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number,
                    st == "1", founder)
                for m in string.gmatch(members, "[^,]+") do
                    table.insert(t.members, m)
                end
                local b = decode(bits, 4 * REACH * REACH)
                for k = 0, 4 * REACH * REACH - 1 do
                    if b[k + 1] == 1 then
                        t.cells[k] = true
                        t.count += 1
                        local cx, cz = cell_of(t, k)
                        core.owned_cell(cx, cz, tag)
                    end
                end
                for k in t.cells do
                    edge_of(t, k)
                end
            end
        end
    end)

    -- ------------------------------------------- growth, effects and edges

    -- What being somewhere does to a body, in core.player_factors' order.
    local EFFECT = {
        own = { 1.1, 1.2, 0.85, 1.5 },
        other = { 0.9, 0.8, 1.15, 0 },
        free = { 1, 1, 1, 1 },
    }
    type Seen = { state: string, tag: number, sent: string, cx: number, cz: number,
                  stolen: { [number]: number } }
    local seen: { [number]: Seen } = {}

    local function state_of(name: string, tag: number): string
        local t = by_tag[tag]
        if t ~= nil then
            return if is_member(t, name) then "own" else "other"
        end
        return if tag >= VILLAGE then "other" else "free"
    end

    local function where_said(name: string, tag: number): string
        local t = by_tag[tag]
        if t ~= nil then
            return if is_member(t, name) then "Your territory" else `{t.founder}'s territory`
        end
        return if tag >= VILLAGE then "A village's ground" else "Free land"
    end

    local function class_of(name: string, tag: number): number
        if tag == 0 then
            return 0
        end
        local t = by_tag[tag]
        if t ~= nil then
            return if is_member(t, name) then 1 else 2
        end
        return if tag >= VILLAGE then 3 else 0
    end

    -- The owners of the ground round a player, to their client, when it changed.
    local function send_edges(p: number, name: string, x: number, y: number, z: number, s: Seen)
        local bits = table.create(GRID * GRID * 2, 0)
        local any = false
        for dx = -R, R do
            for dz = -R, R do
                local c = class_of(name, core.owned_at(x + dx, z + dz))
                if c ~= 0 then
                    any = true
                    local i = ((dx + R) * GRID + (dz + R)) * 2
                    bits[i + 1], bits[i + 2] = c // 2, c % 2
                end
            end
        end
        local text = if any then `{x},{y},{z}|{encode(bits, GRID * GRID * 2)}` else "-"
        if text ~= s.sent then
            s.sent = text
            core.net.send(p, text)
        end
    end

    local clock, every, edges = 0.0, 0.0, 0.0
    local SAID_EFFECT = {
        own = ": walk +10%, dig +20%, blows -15%, healing x1.5",
        other = ": walk -10%, dig -20%, blows +15%, no healing",
        free = "",
    }

    -- ------------------------------------------------------- hired guards

    -- A guard of the villages (mods/villagers' model and kind), bought by the
    -- day from a mayor: it stands round its stone and goes for whoever is on
    -- the territory carrying what they stole from it. World age kept in
    -- storage, as the days are counted on it.
    local DAY = 1200
    local age_at_start = 0.0
    type Guard = { til: number, id: number, back_at: number, next_walk: number }
    local guards: { [number]: Guard } = {}

    local function age(): number
        return age_at_start + clock
    end

    local function save_guard(tag: number)
        local gd = guards[tag]
        core.storage_set(`guard {tag}`, if gd then tostring(math.floor(gd.til)) else "")
    end

    core.event.on("core:world_ready", function()
        age_at_start = tonumber(core.storage_get("age")) or 0
        for tag in by_tag do
            local til = tonumber(core.storage_get(`guard {tag}`))
            if til ~= nil then
                guards[tag] = { til = til, id = 0, back_at = 0, next_walk = 0 }
            end
        end
    end)

    core.event.on("territory:hire", function(ev)
        local t = founded_by(core.player_name(ev.player))
        if t == nil then
            ev.cancel = true
            core.notice(ev.player, "A guard watches a territory: found one with a boundary stone first.")
            return
        end
        local gd = guards[t.tag] or { til = age(), id = 0, back_at = 0, next_walk = 0 }
        gd.til = math.max(gd.til, age()) + ev.days * DAY
        guards[t.tag] = gd
        save_guard(t.tag)
        core.notice(ev.player, `A guard of the village watches your territory for {ev.days} day(s) more.`)
    end)

    local function ground_near(x: number, y: number, z: number): number?
        for gy = y + 6, y - 8, -1 do
            if core.node_solid(core.get_block(x, gy - 1, z)) == 1 and core.node_solid(core.get_block(x, gy, z)) ~= 1
                and core.node_solid(core.get_block(x, gy + 1, z)) ~= 1 then
                return gy
            end
        end
        return nil
    end

    local function step_guards()
        for tag, gd in guards do
            local t = by_tag[tag]
            if t == nil or age() > gd.til then
                if gd.id ~= 0 then
                    core.mob_remove(gd.id)
                end
                guards[tag] = nil
                save_guard(tag)
                if t ~= nil then
                    for _, p in core.players() do
                        if is_member(t, core.player_name(p)) then
                            core.notice(p, "Your guard's days are over: hire it again from a mayor.")
                        end
                    end
                end
            elseif gd.id ~= 0 and core.mob_pos(gd.id) == nil then
                gd.id, gd.back_at = 0, clock + 300   -- struck down: another in five minutes
            elseif gd.id == 0 then
                local near = false
                for _, p in core.players() do
                    local x, _, z = core.player_pos(p)
                    if x and z and (x - t.x) ^ 2 + (z - t.z) ^ 2 < 64 * 64 then
                        near = true
                    end
                end
                if near and clock >= gd.back_at and core.loaded(t.x, t.y, t.z) == 1 then
                    local gy = ground_near(t.x + 1, t.y, t.z) or t.y + 1
                    gd.id = core.spawn_mob("villagers:guard", t.x + 1.5, gy, t.z + 0.5)
                    if gd.id ~= 0 then
                        core.mob_act(gd.id, "", "tools:sword")
                    end
                end
            else
                -- A thief on the territory, the nearest; else a round about the stone.
                local who, best = 0, math.huge
                local mx, _, mz = core.mob_pos(gd.id)
                for _, p in core.players() do
                    local x, _, z = core.player_pos(p)
                    if x and z and mx and mz and core.owned_at(math.floor(x), math.floor(z)) == tag
                        and not is_member(t, core.player_name(p)) and #core.bag_tagged(p, tag) > 0 then
                        local d = (x - mx) ^ 2 + (z - mz) ^ 2
                        if d < best then
                            who, best = p, d
                        end
                    end
                end
                core.mob_hunt(gd.id, who)
                if who == 0 and clock >= gd.next_walk then
                    gd.next_walk = clock + 6 + math.random() * 6
                    for _ = 1, 6 do
                        local x, z = t.x + math.random(-8, 8), t.z + math.random(-8, 8)
                        local gy = core.owned_at(x, z) == tag and ground_near(x, t.y, z) or nil
                        if gy ~= nil then
                            core.mob_goto(gd.id, x, gy, z)
                            break
                        end
                    end
                end
            end
        end
    end

    local grown = false
    core.event.on("core:tick", function(ev)
        clock += ev.dt
        every += ev.dt
        edges += ev.dt
        if every < 0.5 then
            return
        end
        local dt = every
        every = 0
        apply_members()

        local here: { [number]: boolean } = {}
        local inside: { [number]: { number } } = {}   -- tag -> a member's x, z, id
        local thieves: { { number } } = {}            -- player, tag, how many marked
        for _, p in core.players() do
            here[p] = true
            local px, py, pz = core.player_pos(p)
            if px ~= nil and py ~= nil and pz ~= nil then
                local x, y, z = math.floor(px), math.floor(py), math.floor(pz)
                local name = core.player_name(p)
                local tag = core.owned_at(x, z)
                local s = seen[p]
                if s == nil then
                    s = { state = "free", tag = 0, sent = "", cx = x, cz = z, stolen = {} }
                    seen[p] = s
                end
                local st = state_of(name, tag)
                if st ~= s.state then
                    local e = EFFECT[st]
                    core.player_factors(p, e[1], e[2], e[3], e[4])
                    s.state = st
                end
                if tag ~= s.tag then
                    core.notice(p, where_said(name, tag))
                    s.tag = tag
                    core.net.send(p, `S|{where_said(name, tag)}{SAID_EFFECT[st]}`)
                end
                -- Taken from somebody's territory: the thief is told, and its
                -- owners who are here.
                local t = by_tag[tag]
                if t ~= nil and st == "other" then
                    local list, n = core.bag_tagged(p, tag), 0
                    for i = 2, #list, 2 do
                        n += list[i]
                    end
                    if n > (s.stolen[tag] or 0) then
                        core.notice(p, `Stolen: what you take from {t.founder}'s territory is marked.`)
                        for _, o in core.players() do
                            if o ~= p and is_member(t, core.player_name(o)) then
                                core.notice(o, `{name} took {n - (s.stolen[tag] or 0)} thing(s) from your territory.`)
                            end
                        end
                    end
                    s.stolen[tag] = n
                    if n > 0 then
                        table.insert(thieves, { p, tag, n })
                    end
                end
                if st == "own" and inside[tag] == nil and by_tag[tag] ~= nil then
                    inside[tag] = { x, z, p }
                end
                if edges >= 1.0 or grown then
                    send_edges(p, name, x, y, z, s)
                end
            end
        end
        if edges >= 1.0 or grown then
            edges, grown = 0, false
        end
        -- A thief back on the ground they stole from while an owner is on it
        -- too: what they took goes back to that owner.
        for _, th in thieves do
            local p, tag = th[1], th[2]
            local owner = inside[tag]
            local t = by_tag[tag]
            if owner ~= nil and t ~= nil then
                local list = core.bag_tagged(p, tag)
                core.take_tagged(p, tag)
                local back = 0
                for i = 1, #list, 2 do
                    local item, n = core.item_name(list[i]), list[i + 1]
                    local fit = core.give_item(owner[3], item, n)
                    if fit < n then
                        local ox, oy, oz = core.player_pos(owner[3])
                        core.drop_item(math.floor(ox or owner[1]), math.floor(oy or t.y), math.floor(oz or owner[2]),
                            list[i], n - fit)
                    end
                    back += n
                end
                core.notice(p, `Caught: {back} stolen thing(s) went back to {core.player_name(owner[3])}.`)
                core.notice(owner[3], `You took back {back} thing(s) from {core.player_name(p)}.`)
                local s = seen[p]
                if s ~= nil then
                    s.stolen[tag] = 0
                end
            end
        end
        step_guards()
        for p in seen do
            if not here[p] then
                seen[p] = nil
                applied[p] = nil
            end
        end

        for tag, t in by_tag do
            local at = inside[tag]
            if t.stone and at ~= nil and t.count < 4 * REACH * REACH then
                t.grow += dt
                while t.grow >= GROW do
                    t.grow -= GROW
                    -- The free cell on the edge nearest the owner standing on it.
                    local best, bk = math.huge, nil
                    for k in t.frontier do
                        local cx, cz = cell_of(t, k)
                        local d = (cx - at[1]) ^ 2 + (cz - at[2]) ^ 2
                        if d < best then
                            best, bk = d, k
                        end
                    end
                    if bk == nil then
                        break
                    end
                    local cx, cz = cell_of(t, bk)
                    if add_cell(t, cx, cz) then
                        grown = true
                    else
                        t.frontier[bk] = nil   -- somebody else's: never ours
                        t.grow += GROW
                        if t.grow > 4 * GROW then
                            t.grow = 0
                        end
                    end
                end
            end
        end

        if math.floor(clock / 30) ~= math.floor((clock - dt) / 30) then
            core.storage_set("age", tostring(math.floor(age())))
        end
        if math.floor(clock / 10) ~= math.floor((clock - dt) / 10) then
            for tag in dirty do
                local t = by_tag[tag]
                if t ~= nil then
                    save(t)
                end
            end
            dirty = {}
        end
    end)
end

if drawing then
    -- Posts on the edge of owned ground round this player, from the server's
    -- classes: green theirs, red somebody else's, yellow a village's.
    local COLOURS = { { 90, 255, 120 }, { 255, 80, 70 }, { 255, 220, 80 } }
    local posts: { { number } } = {}

    local function ground(x: number, y: number, z: number): number?
        for gy = y + 6, y - 8, -1 do
            if core.node_solid(core.get_block(x, gy - 1, z)) == 1 and core.node_solid(core.get_block(x, gy, z)) ~= 1 then
                return gy
            end
        end
        return nil
    end

    -- Where this player is and what it does to them, top of the screen.
    local where = ""
    core.hud.pass("territory:where", 40, function()
        if where ~= "" and core.engine_hud_on() then
            core.hud.text(0.5, 0.035, where, 0.022, 1, 235, 235, 220, 230)
        end
    end)

    core.event.on("core:net_message", function(ev)
        if ev.text:sub(1, 2) == "S|" then
            where = if ev.text == "S|Free land" then "" else ev.text:sub(3)
            return
        end
        posts = {}
        local x, y, z, s = string.match(ev.text, "^(-?%d+),(-?%d+),(-?%d+)|(.*)$")
        if x == nil or y == nil or z == nil or s == nil then
            return
        end
        local ox, oy, oz = tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number
        local bits = decode(s, GRID * GRID * 2)
        local function class(dx: number, dz: number): number
            if dx < -R or dx > R or dz < -R or dz > R then
                return -1
            end
            local i = ((dx + R) * GRID + (dz + R)) * 2
            return bits[i + 1] * 2 + bits[i + 2]
        end
        for dx = -R, R do
            for dz = -R, R do
                local c = class(dx, dz)
                if c > 0 then
                    local edge = false
                    for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
                        local n = class(dx + d[1], dz + d[2])
                        if n ~= -1 and n ~= c then
                            edge = true
                        end
                    end
                    if edge then
                        local gy = ground(ox + dx, oy, oz + dz)
                        if gy ~= nil then
                            local col = COLOURS[c]
                            table.insert(posts, { ox + dx + 0.5, gy + 0.6, oz + dz + 0.5, col[1], col[2], col[3] })
                        end
                    end
                end
            end
        end
    end)

    local shown = true
    core.event.on("core:input", function(ev)
        if ev.action == "key_f4" then
            shown = not shown
        end
    end)

    core.render.pass("territory:edges", 100, function()
        if not shown then
            return
        end
        for _, p in posts do
            core.render.box(p[1], p[2], p[3], 0.1, 1.2, 0.1, p[4], p[5], p[6], 200)
        end
    end)
end
