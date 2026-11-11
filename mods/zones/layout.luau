--!strict
-- A level laid out of zones: which zone goes where, turned and mirrored how, so
-- that doorways meet and no two zones share a cell. Nothing here reads or
-- writes the world: the same zones, seed, count and difficulty give the same
-- level on any machine, which is what lets a mapgen stage work a level out
-- again for each column it is asked for.
--
-- A zone's own frame: x across (w), y up (h), z along (d), its lowest corner at
-- 0 0 0. A side face is 0 +x, 1 -z, 2 -x, 3 +z -- in that order because a
-- quarter turn of the zone takes each to the next (+x to -z, as the engine's
-- upright turn does), so a face turned t is (face + t) % 4.

export type Conn = {
    x: number, y: number, z: number,   -- the doorway's lowest cell, on the zone's edge
    face: number,                      -- which side it opens out of
    w: number, h: number,              -- cells across and up
    kind: string,                      -- only doorways of one kind meet
    seal: string,                      -- the block that shuts one nothing meets
}
export type Mark = { kind: string, x: number, y: number, z: number, facing: number }
export type Zone = {
    id: string, owner: string, status: string, world: string,
    w: number, h: number, d: number,
    dmin: number, dmax: number, weight: number, mirror: boolean, role: string,
    conns: { Conn }, marks: { Mark },
    palette: { string }, cells: { number },
}
export type Placed = { zone: Zone, x: number, y: number, z: number, t: number, m: boolean }
export type Open = { x: number, y: number, z: number, face: number, w: number, h: number, kind: string, seal: string }
export type Level = {
    placed: { Placed }, open: { Open },
    x1: number, y1: number, z1: number, x2: number, y2: number, z2: number,
}

local L = {}

L.DIRS = { [0] = { 1, 0 }, { 0, -1 }, { -1, 0 }, { 0, 1 } }

-- Where a cell of a zone is in the world, the zone placed as p says.
function L.to_world(p: Placed, lx: number, ly: number, lz: number): (number, number, number)
    local w, d = p.zone.w, p.zone.d
    if p.m then
        lx = w - 1 - lx
    end
    if p.t == 0 then
        return p.x + lx, p.y + ly, p.z + lz
    elseif p.t == 1 then
        return p.x + lz, p.y + ly, p.z + w - 1 - lx
    elseif p.t == 2 then
        return p.x + w - 1 - lx, p.y + ly, p.z + d - 1 - lz
    end
    return p.x + d - 1 - lz, p.y + ly, p.z + lx
end

-- How much ground a placed zone takes, across x and along z.
function L.span(p: Placed): (number, number)
    if p.t % 2 == 0 then
        return p.zone.w, p.zone.d
    end
    return p.zone.d, p.zone.w
end

function L.face(p: Placed, face: number): number
    if p.m and face % 2 == 0 then
        face = (face + 2) % 4
    end
    return (face + p.t) % 4
end

-- A zone's doorway as it stands in the world: its lowest cell there and the
-- side it opens out of.
function L.doorway(p: Placed, c: Conn): Open
    local along_x = c.face % 2 == 1   -- a doorway in a -z or +z side runs along x
    local ax, ay, az = L.to_world(p, c.x, c.y, c.z)
    local bx, _, bz = L.to_world(p, c.x + (if along_x then c.w - 1 else 0), c.y,
        c.z + (if along_x then 0 else c.w - 1))
    return { x = math.min(ax, bx), y = ay, z = math.min(az, bz), face = L.face(p, c.face),
             w = c.w, h = c.h, kind = c.kind, seal = c.seal }
end

-- A generator of whole numbers from a seed: xorshift, in the 32 bits bit32
-- keeps exact.
local function dice(seed: number): (number) -> number
    local s = bit32.bxor(math.floor(seed) % 4294967296, 0x9E3779B9)
    if s == 0 then
        s = 1
    end
    return function(n: number): number
        s = bit32.bxor(s, bit32.lshift(s, 13))
        s = bit32.bxor(s, bit32.rshift(s, 17))
        s = bit32.bxor(s, bit32.lshift(s, 5))
        return s % n
    end
end

-- The zones in the order to try them: drawn one at a time, a zone of weight 3
-- three times as likely to come before one of weight 1.
local function drawn(pool: { Zone }, roll: (number) -> number): { Zone }
    local left = table.clone(pool)
    local out: { Zone } = {}
    while #left > 0 do
        local total = 0
        for _, z in left do
            total += math.max(1, z.weight)
        end
        local at = roll(total)
        for i, z in left do
            at -= math.max(1, z.weight)
            if at < 0 then
                table.insert(out, z)
                table.remove(left, i)
                break
            end
        end
    end
    return out
end

local function overlaps(placed: { Placed }, p: Placed): boolean
    local w, d = L.span(p)
    for _, q in placed do
        local qw, qd = L.span(q)
        if p.x < q.x + qw and q.x < p.x + w and p.z < q.z + qd and q.z < p.z + d
            and p.y < q.y + q.zone.h and q.y < p.y + p.zone.h then
            return true
        end
    end
    return false
end

-- The zone placed so that its doorway c meets the open doorway o, or nil when
-- no turn of it does. Mirrored first when m.
local function meet(z: Zone, c: Conn, o: Open, m: boolean): Placed?
    if c.w ~= o.w or c.h ~= o.h or c.kind ~= o.kind then
        return nil
    end
    local want = (o.face + 2) % 4
    local face = if m and c.face % 2 == 0 then (c.face + 2) % 4 else c.face
    local p: Placed = { zone = z, x = 0, y = 0, z = 0, t = (want - face) % 4, m = m }
    local at = L.doorway(p, c)
    local dir = L.DIRS[o.face]
    p.x, p.y, p.z = o.x + dir[1] - at.x, o.y - at.y, o.z + dir[2] - at.z
    return p
end

-- One zone of `pool` joined to one of the doorways still open, the newest
-- first; which of its doorways, and mirrored or not, in an order the dice pick.
local function join(placed: { Placed }, open: { Open }, pool: { Zone }, roll: (number) -> number): boolean
    for oi = #open, 1, -1 do
        local o = open[oi]
        for _, z in pool do
            local ways: { { number } } = {}
            for ci in z.conns do
                table.insert(ways, { ci, 0 })
                if z.mirror then
                    table.insert(ways, { ci, 1 })
                end
            end
            for i = #ways, 2, -1 do
                local j = roll(i) + 1
                ways[i], ways[j] = ways[j], ways[i]
            end
            for _, way in ways do
                local p = meet(z, z.conns[way[1]], o, way[2] == 1)
                if p and not overlaps(placed, p) then
                    table.insert(placed, p)
                    table.remove(open, oi)
                    for cj, other in z.conns do
                        if cj ~= way[1] then
                            table.insert(open, L.doorway(p, other))
                        end
                    end
                    return true
                end
            end
        end
    end
    return false
end

-- One try at a level: what it placed, the doorways left open, and whether it
-- ends as a level should -- on an "end" zone, when there are any.
local function attempt(starts: { Zone }, rooms: { Zone }, ends: { Zone }, roll: (number) -> number,
                       count: number): ({ Placed }, { Open }, boolean)
    local first: Placed = { zone = drawn(starts, roll)[1], x = 0, y = 0, z = 0, t = 0, m = false }
    local placed: { Placed } = { first }
    local open: { Open } = {}
    for _, c in first.zone.conns do
        table.insert(open, L.doorway(first, c))
    end
    local body = if #ends > 0 then count - 1 else count
    while #placed < body and join(placed, open, drawn(rooms, roll), roll) do
    end
    local ended = #ends == 0
    if not ended and #placed < count then
        ended = join(placed, open, drawn(ends, roll), roll)
    end
    return placed, open, ended
end

-- A level of up to `count` zones for difficulty d. The first is one whose role
-- is "start", the last one whose role is "end" when there are any, the rest
-- "room"; a zone joins the newest doorway it fits, so a level runs on rather
-- than bunching up. `open` are the doorways nothing met. A try that winds
-- round on itself and has no room left for its end is thrown away for the
-- next, sixteen at most, each from the seed: the first that is whole, or the
-- longest that at least ends.
function L.lay(zones: { Zone }, seed: number, count: number, d: number): Level?
    local starts: { Zone }, rooms: { Zone }, ends: { Zone } = {}, {}, {}
    for _, z in zones do
        if d >= z.dmin and d <= z.dmax then
            table.insert(if z.role == "start" then starts elseif z.role == "end" then ends else rooms, z)
        end
    end
    if #rooms == 0 then
        rooms = if #starts > 0 then starts else ends
    end
    if #starts == 0 then
        starts = rooms
    end
    if #starts == 0 then
        return nil
    end

    local placed: { Placed }, open: { Open } = {}, {}
    local best = -1
    for try = 0, 15 do
        local got, left, ended = attempt(starts, rooms, ends, dice(seed + try * 7919), count)
        local score = #got + (if ended then 1000 else 0)
        if score > best then
            placed, open, best = got, left, score
        end
        if ended and #got == count then
            break
        end
    end

    local level: Level = { placed = placed, open = open, x1 = 0, y1 = 0, z1 = 0, x2 = 0, y2 = 0, z2 = 0 }
    for i, p in placed do
        local w, dd = L.span(p)
        if i == 1 then
            level.x1, level.y1, level.z1 = p.x, p.y, p.z
            level.x2, level.y2, level.z2 = p.x + w - 1, p.y + p.zone.h - 1, p.z + dd - 1
        else
            level.x1, level.y1, level.z1 = math.min(level.x1, p.x), math.min(level.y1, p.y), math.min(level.z1, p.z)
            level.x2 = math.max(level.x2, p.x + w - 1)
            level.y2 = math.max(level.y2, p.y + p.zone.h - 1)
            level.z2 = math.max(level.z2, p.z + dd - 1)
        end
    end
    return level
end

return L
