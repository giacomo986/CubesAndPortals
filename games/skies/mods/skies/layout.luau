--!strict
-- The cloud world's layout, for the mapgen, the game and the players' machines
-- alike (init.luau): islands without end over a sea of cloud, a cell of CELL x
-- CELL at a time and each a function of the seed and the cell, never stored.
--
--   * home, at the origin: the doorway from the plaza and a glider rack;
--   * the sky road: an island in every cell straight north of home, a bridge
--     from each to the next, up to the Cloud Manta's islands;
--   * the Manta's islands (ARENA), a fixed cross of five: the centre where it
--     is fought and four with a lightning rod, bridges between them;
--   * everywhere else most cells hold an island, some of them bridged to their
--     east or north neighbour; one in four of the wider ones is a village of
--     owl men (huts of planks and wool), some have a tree, many sky crystals;
--   * updrafts in a third of the cells, over the gap between islands, that
--     wander slowly round where they stand (L.updraft_at).
--
-- The world is 64 high like every room: islands 30 to 44, the sea of cloud
-- 6 to 22 (drawn by each player's machine, core.cloud_sea), the Manta at 58.

export type Island = { name: string, x: number, y: number, z: number, r: number, rod: boolean,
                       village: boolean, tree: boolean, salt: number }
export type Cell = { x: number, y: number, z: number, b: string }
export type CellData = { island: Island?, cells: { Cell }, updraft: { number }? }

local L = {}

L.CELL = 56
L.CLOUD_BASE, L.CLOUD_TOP = 6, 22   -- the sea of cloud (core.cloud_sea)
L.CATCH_Y = 18                      -- below this a body is taken back to where it last stood
L.UP_R = 3.2                        -- an updraft's reach
L.UP_TOP = 58                       -- it lifts up to here

L.HOME = { name = "home", x = 0, y = 36, z = 0, r = 9, rod = false, village = false, tree = false, salt = 0 } :: Island

-- The Manta's islands, far north of home along the sky road.
L.ARENA_Z = 420
local AZ = L.ARENA_Z
L.ISLANDS = {
    { name = "south",  x = 0,   y = 40, z = AZ - 46, r = 7,  rod = true },
    { name = "centre", x = 0,   y = 40, z = AZ,      r = 13, rod = false },
    { name = "north",  x = 0,   y = 42, z = AZ + 46, r = 7,  rod = true },
    { name = "east",   x = 46,  y = 38, z = AZ,      r = 7,  rod = true },
    { name = "west",   x = -46, y = 44, z = AZ,      r = 7,  rod = true },
} :: { Island }
for _, s in L.ISLANDS do
    s.village, s.tree, s.salt = false, false, 0
end
L.CENTRE = L.ISLANDS[2]
-- Where nothing of the endless world is put: the Manta's islands and round them.
local ARENA_X0, ARENA_X1, ARENA_Z0, ARENA_Z1 = -80, 80, AZ - 70, AZ + 70

-- ----------------------------------------------------------------- hashing

local SEED = 0
function L.set_seed(s: number) SEED = s % 2147483647 end

local function mul32(a: number, b: number): number
    local lo = bit32.band(a, 0xFFFF) * b
    local hi = bit32.rshift(a, 16) * b
    return bit32.band(lo + bit32.lshift(bit32.band(hi, 0xFFFF), 16), 0xFFFFFFFF)
end

-- 0..1, of a cell and a salt.
local function hash(x: number, z: number, salt: number): number
    local h = bit32.bxor(mul32(bit32.band(x, 0xFFFFFFFF), 0x27D4EB2D), mul32(bit32.band(z, 0xFFFFFFFF), 0x165667B1))
    h = bit32.bxor(h, mul32(bit32.band(salt + SEED, 0xFFFFFFFF), 0x9E3779B1))
    h = bit32.bxor(h, bit32.rshift(h, 15))
    h = mul32(h, 0x85EBCA6B)
    h = bit32.bxor(h, bit32.rshift(h, 13))
    return h / 4294967296
end
L.hash = hash

-- How far an island's edge is from its middle, that way: a little uneven.
function L.edge(s: Island, dx: number, dz: number): number
    return s.r + 1.2 * math.sin(math.atan2(dz, dx) * 3 + s.salt)
end

-- ----------------------------------------------------------------- the cells

local function in_arena(x: number, z: number): boolean
    return x > ARENA_X0 and x < ARENA_X1 and z > ARENA_Z0 and z < ARENA_Z1
end

local ROAD_LAST = math.floor((AZ - 46 - 40) / L.CELL)   -- the last road cell north of home

local function is_road(cx: number, cz: number): boolean
    return cx == 0 and cz >= 1 and cz <= ROAD_LAST
end

-- The island of a cell, if it has one (not cached: cheap).
local function island_of(cx: number, cz: number): Island?
    if cx == 0 and cz == 0 then return L.HOME end
    local ox, oz = cx * L.CELL, cz * L.CELL
    local road = is_road(cx, cz)
    if not road and (hash(cx, cz, 1) > 0.72 or in_arena(ox + L.CELL / 2, oz + L.CELL / 2)) then return nil end
    if not road and cz >= 0 and cx == 0 then return nil end   -- the road's own column
    local r = if road then 7 + math.floor(hash(cx, cz, 2) * 3) else 5 + math.floor(hash(cx, cz, 2) * 7)
    local x = if road then math.floor((hash(cx, cz, 3) - 0.5) * 10) else ox + 12 + math.floor(hash(cx, cz, 3) * (L.CELL - 24))
    local z = oz + 12 + math.floor(hash(cx, cz, 4) * (L.CELL - 24))
    if in_arena(x, z) then return nil end
    local y = 30 + math.floor(hash(cx, cz, 5) * 12)
    local village = not road and r >= 8 and hash(cx, cz, 6) < 0.3
    return { name = `isle {cx} {cz}`, x = x, y = y, z = z, r = r, rod = false, village = village,
             tree = not village and hash(cx, cz, 7) < 0.55, salt = hash(cx, cz, 8) * 6 }
end

local function floor_div(a: number, b: number): number return math.floor(a / b) end

-- A bridge's cells, from island a to island b: planks three wide on the line
-- between their middles, from edge to edge, a log post either side every four.
local function bridge(a: Island, b: Island, out: { Cell }, deck: { Cell }?)
    local dx, dz = b.x - a.x, b.z - a.z
    local len = math.sqrt(dx * dx + dz * dz)
    local ux, uz = dx / len, dz / len
    local seen: { [string]: boolean } = {}
    local t = a.r - 2
    local step = 0
    while t <= len - b.r + 2 do
        local f = t / len
        local y = math.floor(a.y + (b.y - a.y) * f + 0.5)
        local px, pz = a.x + 0.5 + ux * t, a.z + 0.5 + uz * t
        for w = -1.2, 1.2, 0.6 do
            local x, z = math.floor(px - uz * w), math.floor(pz + ux * w)
            local k = `{x},{z}`
            if not seen[k] then
                seen[k] = true
                local c = { x = x, y = y, z = z, b = "timber:planks" }
                table.insert(out, c)
                if deck then table.insert(deck, c) end
            end
        end
        if step % 8 == 0 then
            for _, w in { -2.2, 2.2 } do
                local x, z = math.floor(px - uz * w), math.floor(pz + ux * w)
                table.insert(out, { x = x, y = y, z = z, b = "timber:log" })
                table.insert(out, { x = x, y = y + 1, z = z, b = "timber:log" })
            end
        end
        t += 0.5
        step += 1
    end
end

-- A village's huts: five by five, logs at the corners, planks between, a
-- door to the south, a roof of brown wool.
local function huts(s: Island, out: { Cell })
    for k, off in { { -4, -2 }, { 3, 1 } } do
        local hx, hz = s.x + off[1], s.z + off[2]
        for dx = -2, 2 do
            for dz = -2, 2 do
                local edge = math.abs(dx) == 2 or math.abs(dz) == 2
                local corner = math.abs(dx) == 2 and math.abs(dz) == 2
                for dy = 1, 3 do
                    local door = dz == -2 and dx == 0 and dy <= 2
                    if edge and not door then
                        table.insert(out, { x = hx + dx, y = s.y + dy, z = hz + dz,
                                            b = if corner then "timber:log" else "timber:planks" })
                    end
                end
                table.insert(out, { x = hx + dx, y = s.y + 4, z = hz + dz, b = "colors:brown_wool" })
            end
        end
        table.insert(out, { x = hx, y = s.y + 5, z = hz, b = if k == 1 then "colors:white_wool" else "colors:brown_wool" })
    end
end

local function tree(s: Island, out: { Cell })
    local tx, tz = s.x + 2, s.z - 2
    for dy = 1, 4 do table.insert(out, { x = tx, y = s.y + dy, z = tz, b = "trees:trunk" }) end
    for dx = -2, 2 do
        for dz = -2, 2 do
            for dy = 4, 6 do
                if math.abs(dx) + math.abs(dz) + (dy - 4) <= 3 and not (dx == 0 and dz == 0 and dy == 4) then
                    table.insert(out, { x = tx + dx, y = s.y + dy, z = tz + dz, b = "trees:leaves" })
                end
            end
        end
    end
end

-- Sky crystals on an island's grass: a few, where the hash says.
local function crystals(s: Island, out: { Cell })
    for i = 1, 3 do
        if hash(s.x, s.z, 20 + i) < 0.6 then
            local a = hash(s.x, s.z, 30 + i) * math.pi * 2
            local d = 2 + hash(s.x, s.z, 40 + i) * (s.r - 4)
            table.insert(out, { x = math.floor(s.x + math.cos(a) * d), y = s.y + 1,
                                z = math.floor(s.z + math.sin(a) * d), b = "skies:crystal" })
        end
    end
end

local cache: { [string]: CellData } = {}

-- A cell's island, and every cell of what stands on it and of the bridges
-- that start there (to the east and north neighbours).
function L.cell(cx: number, cz: number): CellData
    local key = `{cx},{cz}`
    local c = cache[key]
    if c then return c end
    local s = island_of(cx, cz)
    local cells: { Cell } = {}
    if s then
        if s.village then huts(s, cells) end
        if s.tree then tree(s, cells) end
        if s ~= L.HOME then crystals(s, cells) end
        -- The road: north to the next, the last to the Manta's south island.
        if is_road(cx, cz) or (cx == 0 and cz == 0) then
            local n = island_of(0, cz + 1)
            if n then bridge(s, n, cells) elseif cz == ROAD_LAST then bridge(s, L.ISLANDS[1], cells) end
        else
            for _, d in { { 1, 0, 11 }, { 0, 1, 12 } } do
                local o = island_of(cx + d[1], cz + d[2])
                if o and o ~= L.HOME and math.abs(o.y - s.y) <= 6 and hash(cx, cz, d[3]) < 0.45 then
                    bridge(s, o, cells)
                end
            end
        end
    end
    -- An updraft in a third of the cells: in the gap, never over an island
    -- wherever it wanders (9 about where it stands) -- the first of the
    -- cell's corners and edges that is clear of every island near.
    local up: { number }? = nil
    if hash(cx, cz, 13) < 0.45 then
        local near: { Island } = {}
        for i = -1, 1 do
            for j = -1, 1 do
                local o = island_of(cx + i, cz + j)
                if o then table.insert(near, o) end
            end
        end
        for _, f in { { 0.15, 0.15 }, { 0.85, 0.85 }, { 0.15, 0.85 }, { 0.85, 0.15 }, { 0.5, 0.1 }, { 0.1, 0.5 } } do
            local ux, uz = cx * L.CELL + f[1] * L.CELL, cz * L.CELL + f[2] * L.CELL
            local clear = not in_arena(ux, uz)
            for _, o in near do
                if (o.x - ux) ^ 2 + (o.z - uz) ^ 2 < (o.r + 12) ^ 2 then clear = false end
            end
            if clear then
                up = { ux, uz, hash(cx, cz, 14) * 6.28 }
                break
            end
        end
    end
    c = { island = s, cells = cells, updraft = up }
    cache[key] = c
    return c
end

-- The cells whose islands or bridges may reach into x0..x1, z0..z1.
function L.cells_over(x0: number, z0: number, x1: number, z1: number): { CellData }
    local out = {}
    for cx = floor_div(x0, L.CELL) - 1, floor_div(x1, L.CELL) + 1 do
        for cz = floor_div(z0, L.CELL) - 1, floor_div(z1, L.CELL) + 1 do
            table.insert(out, L.cell(cx, cz))
        end
    end
    return out
end

-- The island a column is on, if any: the endless world's or the Manta's.
function L.island_at(x: number, z: number): Island?
    for _, s in L.ISLANDS do
        local dx, dz = x - s.x, z - s.z
        if dx * dx + dz * dz <= L.edge(s, dx, dz) ^ 2 then return s end
    end
    local cx, cz = floor_div(x, L.CELL), floor_div(z, L.CELL)
    for i = -1, 1 do
        for j = -1, 1 do
            local s = L.cell(cx + i, cz + j).island
            if s then
                local dx, dz = x - s.x, z - s.z
                if dx * dx + dz * dz <= L.edge(s, dx, dz) ^ 2 then return s end
            end
        end
    end
    return nil
end

-- The islands within r of a point.
function L.islands_near(x: number, z: number, r: number): { Island }
    local out = {}
    for _, s in L.ISLANDS do
        if (s.x - x) ^ 2 + (s.z - z) ^ 2 <= r * r then table.insert(out, s) end
    end
    for _, c in L.cells_over(x - r, z - r, x + r, z + r) do
        local s = c.island
        if s and (s.x - x) ^ 2 + (s.z - z) ^ 2 <= r * r then table.insert(out, s) end
    end
    return out
end

-- The Manta's islands' bridges, kept apart: they fall in its third phase.
L.ARENA_CELLS = {} :: { Cell }
L.DECK = {} :: { Cell }
for _, pair in { { 1, 2 }, { 2, 3 }, { 2, 4 }, { 2, 5 } } do
    bridge(L.ISLANDS[pair[1]], L.ISLANDS[pair[2]], L.ARENA_CELLS, L.DECK)
end
for _, s in L.ISLANDS do
    if s ~= L.CENTRE then crystals(s, L.ARENA_CELLS) end
end

-- The updrafts near a point at a time: wandering slowly, 9 nodes about where
-- they stand, a turn every few minutes. x, z of each.
function L.updrafts_near(x: number, z: number, r: number, t: number): { number }
    local out = {}
    local function add(ax: number, az: number, ph: number)
        local ux = ax + 9 * math.sin(t * 0.021 + ph)
        local uz = az + 9 * math.cos(t * 0.017 + ph * 1.3)
        if (ux - x) ^ 2 + (uz - z) ^ 2 <= r * r then
            table.insert(out, ux)
            table.insert(out, uz)
        end
    end
    for _, c in L.cells_over(x - r, z - r, x + r, z + r) do
        local u = c.updraft
        if u then add(u[1], u[2], u[3]) end
    end
    -- In the Manta's islands, one in each gap between the centre and a rod.
    for i = 1, 4 do
        local a = (i - 0.5) * math.pi / 2
        add(L.CENTRE.x + math.cos(a) * 30, L.CENTRE.z + math.sin(a) * 30, i * 1.7)
    end
    return out
end

return L
