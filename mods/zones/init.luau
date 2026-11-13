--!strict
-- zones: reusable pieces of a level.
--
-- A ZONE is a box of the world saved with what makes it a piece of a level:
--
--   its blocks, each with its turn;
--   its DOORWAYS, where another zone may join: a rectangle of zones:doorway
--     blocks in one of its four sides, saved as the hole it marks;
--   its MARKERS, the places a world puts its own things: zones:start, exit,
--     mob, loot, key and trigger, each saved as air with which it was and the
--     way it faced;
--   what it is for: the world it belongs to ("chambers", "arpg"...), whether it
--     starts a level, ends one or is a room on the way, the difficulties it
--     suits, how often it comes up, and whether it may be laid mirrored.
--
-- A LEVEL is zones joined doorway to doorway, turned and mirrored as they need,
-- worked out from a seed (layout.luau) and written into the world a few
-- thousand cells a tick. A doorway nothing joined is shut with the block that
-- was under it when the zone was saved.
--
-- A zone is its maker's (core.player_key) and only in levels asked for in
-- their name, until an admin approves it: then it is in everybody's.
--
--   /zone                                    the zones, yours and the approved
--   /zone save WORLD NAME X1 Y1 Z1 X2 Y2 Z2 [start|room|end]
--   /zone set ID d MIN MAX | weight N | mirror on|off | role start|room|end
--   /zone delete ID                          yours, or anybody's for an admin
--   /zone approve ID   /zone private ID      an admin's
--   /zone try WORLD [COUNT [SEED [D]]]       an admin's: a level laid beside you
--
-- Another mod asks for a level with "zones:lay" and hears "zones:mark" for
-- every marker in it, then "zones:laid" once it stands.
--
-- Kept in the server's shared storage, so every room has the same zones:
-- "index", the ids; "z ID", a zone's head; "z ID 1".., its palette and cells.
--
-- A zone may also come with the game (shipped.luau): approved, nobody's, and
-- there on every server. One saved under the same id stands in for it on that
-- server; deleted, the game's is back. tools/zones_ship.py writes the file.
local T = require("./turns")
local L = require("./layout")
-- The zones that come with the game: id -> { head, body }, the two texts a zone
-- is in the store. Written by tools/zones_ship.py out of a world file.
local SHIPPED: { [string]: { string } } = require("./shipped")

type Zone = L.Zone
type Conn = L.Conn
type Mark = L.Mark

local DOORWAY = core.register_node("zones:doorway", 230, 60, 200, true, 0, "", 0, 0.2, "stone")
local MARKS = { "start", "exit", "mob", "loot", "key", "trigger" }
local COLOURS: { [string]: { number } } = {
    start = { 80, 220, 90 }, exit = { 240, 220, 60 }, mob = { 220, 60, 60 },
    loot = { 240, 160, 40 }, key = { 250, 240, 160 }, trigger = { 80, 160, 240 },
}
local mark_of: { [number]: string } = {}
for _, kind in MARKS do
    local c = COLOURS[kind]
    mark_of[core.register_node(`zones:{kind}`, c[1], c[2], c[3], true, 0, "upright", 0, 0.2, "stone")] = kind
end

local MAX_W, MAX_H = 64, 48   -- a zone's box, at most
local PART = 3500             -- a storage value holds 4096 bytes
local PER_TICK = 4000         -- cells written a tick while a level is laid

core.event.define("zones:lay", {
    fields = { tag = "string", world = "string", owner = "string", x = "number", y = "number", z = "number",
               seed = "number", d = "number", count = "number" },
    cancellable = false,
})
core.event.define("zones:mark", {
    fields = { tag = "string", kind = "string", x = "number", y = "number", z = "number", facing = "number" },
    cancellable = false,
})
core.event.define("zones:laid", {
    fields = { tag = "string", pieces = "number", x1 = "number", y1 = "number", z1 = "number",
               x2 = "number", y2 = "number", z2 = "number" },
    cancellable = false,
})

-- ------------------------------------------------------------- the store

-- A text of any length under one key: "key" holds how many parts, "key 1"..
-- the parts. Parts left over from a longer text are erased.
local function put_long(key: string, text: string)
    local was = tonumber(core.shared_storage_get(key)) or 0
    local parts = math.ceil(#text / PART)
    for k = 1, parts do
        core.shared_storage_set(`{key} {k}`, string.sub(text, (k - 1) * PART + 1, k * PART))
    end
    for k = parts + 1, was do
        core.shared_storage_set(`{key} {k}`, "")
    end
    core.shared_storage_set(key, if parts > 0 then tostring(parts) else "")
end

local function get_long(key: string): string
    local text = {}
    for k = 1, tonumber(core.shared_storage_get(key)) or 0 do
        table.insert(text, core.shared_storage_get(`{key} {k}`))
    end
    return table.concat(text)
end

-- Every room of the server runs this mod in a VM of its own over the one
-- store, so what is cached is checked against the store's stamp, which every
-- write moves on.
local cache: { [string]: Zone } = {}
local seen_stamp = ""

local function fresh()
    local stamp = core.shared_storage_get("stamp")
    if stamp ~= seen_stamp then
        seen_stamp = stamp
        cache = {}
    end
end

local function touch()
    seen_stamp = tostring((tonumber(core.shared_storage_get("stamp")) or 0) + 1)
    core.shared_storage_set("stamp", seen_stamp)
end

-- The ids in the store alone: what "index" is written from.
local function stored_ids(): { string }
    local text = get_long("index")
    return if text == "" then {} else string.split(text, ";")
end

local function ids(): { string }
    local list = stored_ids()
    local added = false
    for id in SHIPPED do
        if not table.find(list, id) then
            table.insert(list, id)
            added = true
        end
    end
    if added then
        table.sort(list)
    end
    return list
end

local function numbers(text: string, sep: string): { number }
    local out = {}
    for _, word in string.split(text, sep) do
        table.insert(out, tonumber(word) or 0)
    end
    return out
end

local function save_head(z: Zone)
    local conns, marks = {}, {}
    for _, c in z.conns do
        table.insert(conns, `{c.x},{c.y},{c.z},{c.face},{c.w},{c.h},{c.kind},{c.seal}`)
    end
    for _, m in z.marks do
        table.insert(marks, `{m.kind},{m.x},{m.y},{m.z},{m.facing}`)
    end
    put_long(`z {z.id}`, table.concat({
        "1", if z.owner == "" then "-" else z.owner, z.status, z.world, `{z.w} {z.h} {z.d}`,
        `{z.dmin} {z.dmax} {z.weight} {if z.mirror then 1 else 0} {z.role}`,
        table.concat(conns, ";"), table.concat(marks, ";"),
    }, "|"))
    cache[z.id] = z
    touch()
end

local function save(z: Zone)
    -- The cells as runs, "index*count" or "index", after the palette's line.
    local runs = {}
    local i = 1
    while i <= #z.cells do
        local j = i
        while j < #z.cells and z.cells[j + 1] == z.cells[i] do
            j += 1
        end
        table.insert(runs, if j > i then `{z.cells[i]}*{j - i + 1}` else tostring(z.cells[i]))
        i = j + 1
    end
    put_long(`c {z.id}`, table.concat(z.palette, ";") .. "\n" .. table.concat(runs, ","))
    local list = stored_ids()
    if not table.find(list, z.id) then
        table.insert(list, z.id)
        table.sort(list)
        put_long("index", table.concat(list, ";"))
    end
    save_head(z)
end

local function load(id: string): Zone?
    fresh()
    if cache[id] then
        return cache[id]
    end
    local head_text, body = get_long(`z {id}`), ""
    local shipped = head_text == "" and SHIPPED[id] ~= nil
    if shipped then
        head_text, body = SHIPPED[id][1], SHIPPED[id][2]
    else
        body = get_long(`c {id}`)
    end
    local head = string.split(head_text, "|")
    if #head ~= 8 or head[1] ~= "1" then
        return nil
    end
    local size, set = numbers(head[5], " "), string.split(head[6], " ")
    local z: Zone = {
        id = id, owner = if head[2] == "-" then "" else head[2], status = head[3], world = head[4],
        w = size[1], h = size[2], d = size[3],
        dmin = tonumber(set[1]) or 1, dmax = tonumber(set[2]) or 10, weight = tonumber(set[3]) or 1,
        mirror = set[4] == "1", role = set[5] or "room",
        conns = {}, marks = {}, palette = {}, cells = {},
    }
    if head[7] ~= "" then
        for _, row in string.split(head[7], ";") do
            local f = string.split(row, ",")
            table.insert(z.conns, { x = tonumber(f[1]) or 0, y = tonumber(f[2]) or 0, z = tonumber(f[3]) or 0,
                                    face = tonumber(f[4]) or 0, w = tonumber(f[5]) or 1, h = tonumber(f[6]) or 2,
                                    kind = f[7] or "door", seal = f[8] or "" })
        end
    end
    if head[8] ~= "" then
        for _, row in string.split(head[8], ";") do
            local f = string.split(row, ",")
            table.insert(z.marks, { kind = f[1], x = tonumber(f[2]) or 0, y = tonumber(f[3]) or 0,
                                    z = tonumber(f[4]) or 0, facing = tonumber(f[5]) or 0 })
        end
    end
    if shipped then
        z.owner, z.status = "", "approved"
    end
    local cut = string.find(body, "\n", 1, true)
    if cut == nil then
        return nil
    end
    z.palette = string.split(string.sub(body, 1, cut - 1), ";")
    for run in string.gmatch(string.sub(body, cut + 1), "[^,]+") do
        local v, n = string.match(run, "^(%d+)%*(%d+)$")
        if v then
            local index = tonumber(v) :: number
            for _ = 1, tonumber(n) :: number do
                table.insert(z.cells, index)
            end
        else
            table.insert(z.cells, tonumber(run) :: number)
        end
    end
    if #z.cells ~= z.w * z.h * z.d then
        core.log(`zones: {id} has {#z.cells} cells for a box of {z.w} x {z.h} x {z.d}: left out`)
        return nil
    end
    cache[id] = z
    return z
end

local function erase(id: string)
    put_long(`z {id}`, "")
    put_long(`c {id}`, "")
    local list = stored_ids()
    local at = table.find(list, id)
    if at then
        table.remove(list, at)
        put_long("index", table.concat(list, ";"))
    end
    cache[id] = nil
    touch()
end

-- The zones of a world a level asked for in `owner`'s name may use: the
-- approved, and that player's own.
local function usable(world: string, owner: string): { Zone }
    local out = {}
    for _, id in ids() do
        local z = load(id)
        if z and z.world == world and (z.status == "approved" or (owner ~= "" and z.owner == owner)) then
            table.insert(out, z)
        end
    end
    return out
end

-- ------------------------------------------------------- out of the world

-- The doorways in one side of a box: the zones:doorway cells there, grouped
-- into the rectangles they make. `cells` maps "u v" (along the side, and up)
-- to true. An error's text when a group is not a full rectangle.
local function rectangles(cells: { [string]: boolean }): ({ { number } }, string?)
    local out = {}
    local seen: { [string]: boolean } = {}
    for key in cells do
        if not seen[key] then
            local u0, v0 = string.match(key, "^(%-?%d+) (%-?%d+)$")
            local lo_u, hi_u = tonumber(u0) :: number, tonumber(u0) :: number
            local lo_v, hi_v = tonumber(v0) :: number, tonumber(v0) :: number
            local stack, count = { key }, 0
            seen[key] = true
            while #stack > 0 do
                local k = table.remove(stack) :: string
                local us, vs = string.match(k, "^(%-?%d+) (%-?%d+)$")
                local u, v = tonumber(us) :: number, tonumber(vs) :: number
                count += 1
                lo_u, hi_u, lo_v, hi_v = math.min(lo_u, u), math.max(hi_u, u), math.min(lo_v, v), math.max(hi_v, v)
                for _, step in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
                    local nk = `{u + step[1]} {v + step[2]}`
                    if cells[nk] and not seen[nk] then
                        seen[nk] = true
                        table.insert(stack, nk)
                    end
                end
            end
            if count ~= (hi_u - lo_u + 1) * (hi_v - lo_v + 1) then
                return {}, "a doorway is a full rectangle of doorway blocks"
            end
            table.insert(out, { lo_u, lo_v, hi_u - lo_u + 1, hi_v - lo_v + 1 })
        end
    end
    table.sort(out, function(a, b)
        return if a[1] ~= b[1] then a[1] < b[1] else a[2] < b[2]
    end)
    return out, nil
end

-- A box of the world as a zone, or why not.
local function capture(x1: number, y1: number, z1: number, x2: number, y2: number, z2: number): (Zone?, string)
    local w, h, d = x2 - x1 + 1, y2 - y1 + 1, z2 - z1 + 1
    if w > MAX_W or d > MAX_W or h > MAX_H then
        return nil, `a zone is at most {MAX_W} by {MAX_W} and {MAX_H} high`
    end
    local z: Zone = { id = "", owner = "", status = "private", world = "", w = w, h = h, d = d,
                      dmin = 1, dmax = 10, weight = 1, mirror = false, role = "room",
                      conns = {}, marks = {}, palette = { "" }, cells = {} }
    local index: { [string]: number } = { [""] = 1 }
    local sides: { { [string]: boolean } } = { {}, {}, {}, {} }   -- by face + 1
    local tally: { [number]: number } = {}

    for ly = 0, h - 1 do
        for lz = 0, d - 1 do
            for lx = 0, w - 1 do
                local b = core.get_block(x1 + lx, y1 + ly, z1 + lz)
                local key = ""
                if b == DOORWAY then
                    local on_x, on_z = lx == 0 or lx == w - 1, lz == 0 or lz == d - 1
                    if on_x == on_z then
                        return nil, if on_x then "a doorway block stands in a corner of the box: which side is it in?"
                            else "a doorway block stands inside the box: doorways are in its sides"
                    end
                    if on_x then
                        sides[(if lx == w - 1 then 0 else 2) + 1][`{lz} {ly}`] = true
                    else
                        sides[(if lz == 0 then 1 else 3) + 1][`{lx} {ly}`] = true
                    end
                elseif mark_of[b] then
                    table.insert(z.marks, { kind = mark_of[b], x = lx, y = ly, z = lz,
                                            facing = core.get_facing(x1 + lx, y1 + ly, z1 + lz) })
                elseif b ~= 0 then
                    key = `{core.node_name(b)} {core.get_facing(x1 + lx, y1 + ly, z1 + lz)}`
                end
                local i = index[key]
                if i == nil then
                    table.insert(z.palette, key)
                    i = #z.palette
                    index[key] = i
                end
                if key ~= "" then
                    tally[i] = (tally[i] or 0) + 1
                end
                table.insert(z.cells, i - 1)
            end
        end
    end

    -- What shuts a doorway nothing meets: the block under it, or the zone's
    -- commonest when the doorway stands on the box's floor or over a hole.
    local common, most = "", 0
    for i, n in tally do
        if n > most then
            common, most = string.match(z.palette[i], "^(%S+)") :: string, n
        end
    end
    for face = 0, 3 do
        local rects, why = rectangles(sides[face + 1])
        if why then
            return nil, why
        end
        for _, r in rects do
            local cx = if face == 0 then w - 1 elseif face == 2 then 0 else r[1]
            local cz = if face == 1 then 0 elseif face == 3 then d - 1 else r[1]
            local seal = common
            if r[2] > 0 then
                local under = z.palette[z.cells[(r[2] - 1) * w * d + cz * w + cx + 1] + 1]
                if under ~= "" then
                    seal = string.match(under, "^(%S+)") :: string
                end
            end
            table.insert(z.conns, { x = cx, y = r[2], z = cz, face = face, w = r[3], h = r[4],
                                    kind = "door", seal = seal })
        end
    end
    return z, ""
end

-- -------------------------------------------------------- into the world

type Job = {
    tag: string, level: L.Level, piece: number, cell: number,
    ox: number, oy: number, oz: number,
}
local jobs: { Job } = {}
local ids_of: { [string]: number } = {}   -- a palette entry's block, looked up once

local function block_of(name: string): number
    local id = ids_of[name]
    if id == nil then
        id = core.node_id(name)
        ids_of[name] = id
    end
    return id
end

-- Writes up to `budget` cells of a job; how many it wrote, and whether the
-- level stands whole.
local function work(job: Job, budget: number): (number, boolean)
    local wrote = 0
    while job.piece <= #job.level.placed do
        local p = job.level.placed[job.piece]
        local z = p.zone
        local per = z.w * z.d
        while job.cell <= #z.cells do
            if wrote >= budget then
                return wrote, false
            end
            local i = job.cell - 1
            local ly, rest = i // per, i % per
            local x, y, zz = L.to_world(p, rest % z.w, ly, rest // z.w)
            x, y, zz = x + job.ox, y + job.oy, zz + job.oz
            local key = z.palette[z.cells[job.cell] + 1]
            if key == "" then
                core.set_block(x, y, zz, 0)
            else
                local name, f = string.match(key, "^(%S+) (%d+)$")
                local id = block_of(name :: string)
                core.set_block(x, y, zz, id)
                local turns = core.node_turns(id)
                if turns > 1 then
                    core.set_facing(x, y, zz, T.turned(turns, tonumber(f) :: number, p.t, p.m))
                end
            end
            job.cell += 1
            wrote += 1
        end
        job.piece += 1
        job.cell = 1
    end

    -- Whole: the doorways nothing met are shut, and whoever asked hears where
    -- every marker is.
    for _, o in job.level.open do
        local along_x = o.face % 2 == 1
        local id = if o.seal ~= "" then block_of(o.seal) else 0
        for u = 0, o.w - 1 do
            for v = 0, o.h - 1 do
                core.set_block(job.ox + o.x + (if along_x then u else 0), job.oy + o.y + v,
                    job.oz + o.z + (if along_x then 0 else u), id)
            end
        end
    end
    for _, p in job.level.placed do
        for _, m in p.zone.marks do
            local x, y, zz = L.to_world(p, m.x, m.y, m.z)
            core.event.emit("zones:mark", { tag = job.tag, kind = m.kind, x = x + job.ox, y = y + job.oy,
                                            z = zz + job.oz, facing = T.turned(4, m.facing, p.t, p.m) })
        end
    end
    local lv = job.level
    core.event.emit("zones:laid", { tag = job.tag, pieces = #lv.placed,
                                    x1 = lv.x1 + job.ox, y1 = lv.y1 + job.oy, z1 = lv.z1 + job.oz,
                                    x2 = lv.x2 + job.ox, y2 = lv.y2 + job.oy, z2 = lv.z2 + job.oz })
    return wrote, true
end

-- A level of `world`'s zones with its first zone's corner at x, y, z; how many
-- zones it has, 0 when the world has none to use.
local function lay(tag: string, world: string, owner: string, x: number, y: number, z: number,
                   seed: number, d: number, count: number): number
    local level = L.lay(usable(world, owner), seed, math.clamp(count, 1, 64), d)
    if level == nil then
        core.event.emit("zones:laid", { tag = tag, pieces = 0, x1 = x, y1 = y, z1 = z, x2 = x, y2 = y, z2 = z })
        return 0
    end
    table.insert(jobs, { tag = tag, level = level, piece = 1, cell = 1, ox = x, oy = y, oz = z })
    return #level.placed
end

core.event.on("zones:lay", function(ev)
    lay(ev.tag, ev.world, ev.owner, math.floor(ev.x), math.floor(ev.y), math.floor(ev.z), ev.seed, ev.d, ev.count)
end)

core.event.on("core:tick", function(_)
    local left = PER_TICK
    while left > 0 and #jobs > 0 do
        local wrote, whole = work(jobs[1], left)
        left -= wrote
        if whole then
            table.remove(jobs, 1)
        else
            break
        end
    end
end)

-- ------------------------------------------------------------ the command

local function clean(word: string): string
    return string.sub(string.gsub(string.lower(word), "[^a-z0-9_]", ""), 1, 24)
end

local function admin(player: number): boolean
    return core.has_perm(player, "admin") == 1
end

local function mine(player: number, z: Zone): boolean
    local key = core.player_key(player)
    return admin(player) or (key ~= "" and z.owner == key)
end

local function line(z: Zone): string
    return `{z.id}: {z.w} x {z.h} x {z.d}, {z.role}, {#z.conns} doorway(s), {#z.marks} marker(s), `
        .. `d {z.dmin}-{z.dmax}, weight {z.weight}{if z.mirror then ", mirrored too" else ""}, {z.status}`
end

core.command("zone", "[save|set|delete|approve|private|try] -- pieces of levels, and levels laid out of them")
core.event.on("core:command", function(ev)
    if ev.name ~= "zone" then
        return
    end
    local a = string.split(string.gsub(ev.args, "^%s+", ""), " ")
    local verb, player = string.lower(a[1] or ""), ev.player
    local key = core.player_key(player)

    if verb == "" then
        local n = 0
        for _, id in ids() do
            local z = load(id)
            if z and (z.status == "approved" or mine(player, z)) then
                core.chat(player, line(z))
                n += 1
            end
        end
        if n == 0 then
            core.chat(player, "No zones yet. /zone save WORLD NAME X1 Y1 Z1 X2 Y2 Z2 [start|room|end]")
        end
        return
    end

    if verb == "save" then
        local world, name = clean(a[2] or ""), clean(a[3] or "")
        local n: { number } = {}
        for i = 4, 9 do
            table.insert(n, math.floor(tonumber(a[i] or "") or 0 / 0))
        end
        local role = string.lower(a[10] or "room")
        if world == "" or name == "" or #a < 9 or n[1] ~= n[1] or n[6] ~= n[6]
            or (role ~= "start" and role ~= "room" and role ~= "end") then
            core.chat(player, "/zone save WORLD NAME X1 Y1 Z1 X2 Y2 Z2 [start|room|end]")
            return
        end
        if key == "" and not admin(player) then
            core.chat(player, "A zone is somebody's: join with a key of your own to save one.")
            return
        end
        local id = `{world}.{name}`
        local old = load(id)
        if old and not mine(player, old) then
            core.chat(player, `{id} is somebody else's zone: pick another name.`)
            return
        end
        local z, why = capture(math.min(n[1], n[4]), math.min(n[2], n[5]), math.min(n[3], n[6]),
            math.max(n[1], n[4]), math.max(n[2], n[5]), math.max(n[3], n[6]))
        if z == nil then
            core.chat(player, `Not saved: {why}.`)
            return
        end
        -- Saved over, a zone keeps what it was set to and goes back to its maker
        -- alone: what was approved was the old one.
        if old then
            z.dmin, z.dmax, z.weight, z.mirror = old.dmin, old.dmax, old.weight, old.mirror
        end
        z.id, z.world, z.owner, z.role = id, world, key, role
        save(z)
        core.chat(player, `Saved {line(z)}`)
        core.log(`zones: {core.player_name(player)} saved {line(z)}`)
        return
    end

    if verb == "try" then
        if not admin(player) then
            core.chat(player, "Laying a level in this world is an admin's.")
            return
        end
        local world = clean(a[2] or "")
        local px, py, pz = core.player_pos(player)
        if world == "" or px == nil or py == nil or pz == nil then
            core.chat(player, "/zone try WORLD [COUNT [SEED [D]]]")
            return
        end
        local count, seed, d = tonumber(a[3] or "") or 6, tonumber(a[4] or "") or 1, tonumber(a[5] or "") or 1
        local n = lay(`try {core.player_name(player)}`, world, key, math.floor(px) + 4, math.floor(py),
            math.floor(pz), seed, d, count)
        if n == 0 then
            core.chat(player, `No zone of {world} to start a level with.`)
        else
            core.chat(player, `A level of {n} zone(s) is going up beside you.`)
        end
        return
    end

    -- The rest are about one zone.
    local z = load(a[2] or "")
    if z == nil then
        core.chat(player, `No zone called {a[2] or ""}: /zone lists them.`)
        return
    end
    if verb == "approve" or verb == "private" then
        if not admin(player) then
            core.chat(player, "Approving a zone is an admin's.")
            return
        end
        z.status = if verb == "approve" then "approved" else "private"
        save_head(z)
        core.chat(player, line(z))
        core.log(`zones: {core.player_name(player)} made {z.id} {z.status}`)
    elseif not mine(player, z) then
        core.chat(player, `{z.id} is somebody else's zone.`)
    elseif verb == "delete" then
        if get_long(`z {z.id}`) == "" then
            core.chat(player, `{z.id} comes with the game: it cannot be deleted, only saved over.`)
            return
        end
        erase(z.id)
        core.chat(player, if SHIPPED[z.id] then `{z.id} is the game's own again.` else `{z.id} is gone.`)
        core.log(`zones: {core.player_name(player)} deleted {z.id}`)
    elseif verb == "set" then
        local what = string.lower(a[3] or "")
        if what == "d" and tonumber(a[4] or "") and tonumber(a[5] or "") then
            z.dmin = math.clamp(math.floor(tonumber(a[4]) :: number), 1, 10)
            z.dmax = math.clamp(math.floor(tonumber(a[5]) :: number), z.dmin, 10)
        elseif what == "weight" and tonumber(a[4] or "") then
            z.weight = math.clamp(math.floor(tonumber(a[4]) :: number), 1, 20)
        elseif what == "mirror" and (a[4] == "on" or a[4] == "off") then
            z.mirror = a[4] == "on"
        elseif what == "role" and (a[4] == "start" or a[4] == "room" or a[4] == "end") then
            z.role = a[4]
        else
            core.chat(player, "/zone set ID d MIN MAX | weight N | mirror on|off | role start|room|end")
            return
        end
        -- What an admin approved is the zone as it was.
        if z.status == "approved" and not admin(player) then
            z.status = "private"
        end
        save_head(z)
        core.chat(player, line(z))
    else
        core.chat(player, "/zone [save|set|delete|approve|private|try]")
    end
end)
