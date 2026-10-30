--!strict
-- gates: a structure round every opening onto a themed world, built to look
-- like the world behind it -- a chequered arch for the track, obsidian and
-- glowrock for the Inferno, a techbase door for the arena, a ruined arch among
-- graves for the arpg lands, white panels for the test chambers, a doorway in
-- yellow wallpaper for the backrooms, an orange and black scaffold with steps
-- up its side and a bar over it for the parkour course, a dead frame of dark
-- steel with seven pedestals for the laboratory, a sandbagged bunker door
-- under a red and a blue flag for the front, a black basalt gate with
-- violet embers for the assault.
--
-- Which world an opening leads to is the room it names (core.portal_room), so
-- a sandbox started with the arena as its first opening gets the arena's gate
-- there. The first two openings stay where the server put them (the spawn
-- faces the first, mods/infernoportal frames the second); the rest are put
-- about the plaza here. ponytail: fixed spots on the plaza; scattered over the
-- whole map later, which wants a place found per seed.
--
-- Built every start, as mods/infernoportal builds its frame: only the gate's
-- own cells and a clear way through the hole are written.

local id = core.node_id
local AIR = 0
local WHITE, BLACK, GRAY = id("colors:white_concrete"), id("colors:black_concrete"), id("colors:gray_concrete")
local LIGHT_GRAY, RED, YELLOW = id("colors:light_gray_concrete"), id("colors:red_concrete"), id("colors:yellow_concrete")
local ORANGE, BLUE = id("colors:orange_concrete"), id("colors:light_blue_concrete")
local ORANGE_WOOL = id("colors:orange_wool")
local RED_TERRA, BLACK_TERRA = id("colors:red_terracotta"), id("colors:black_terracotta")
local OBSIDIAN, GLOW, LAMP = id("infernoportal:obsidian"), id("ruins:lamp"), id("spike:lamp")
local BRICKS, MOSSY, COBBLE = id("rocks:stone_bricks"), id("ruins:mossy_brick"), id("rocks:cobblestone")
local MOSSY_COBBLE, SMOOTH, DEEPSLATE = id("rocks:mossy_cobblestone"), id("rocks:smooth_stone"), id("rocks:deepslate")
local COARSE, BROWN = id("rocks:coarse_dirt"), id("colors:brown_concrete")
local GREEN, SANDBAG = id("colors:green_concrete"), id("colors:yellow_terracotta")

-- Put(u, v, w, block): u along the opening, v up from its lowest cell, w out
-- of its front. The hole is u 0, v 0 and 1, w 0.
type Put = (number, number, number, number) -> ()

-- The ring of blocks round the 1x2 hole, corners too.
local function frame(put: Put, block: (number, number) -> number)
    for v = -1, 2 do
        for u = -1, 1 do
            if u ~= 0 or v == -1 or v == 2 then put(u, v, 0, block(u, v)) end
        end
    end
end

local function chequer(u: number, v: number): number
    return if (u + v) % 2 == 0 then WHITE else BLACK
end

local THEMES: { [string]: (Put) -> () } = {
    -- The track: a start arch, red and white posts, a chequered banner and the
    -- grey of a road through it.
    pista = function(put)
        frame(put, chequer)
        for v = 0, 3 do
            put(-3, v, 0, if v % 2 == 0 then RED else WHITE)
            put(3, v, 0, if v % 2 == 0 then WHITE else RED)
        end
        for v = 4, 5 do
            for u = -3, 3 do put(u, v, 0, chequer(u, v)) end
        end
        put(-3, 6, 0, LAMP)
        put(3, 6, 0, LAMP)
        for w = -4, 4 do
            for u = -2, 2 do
                if w ~= 0 or math.abs(u) == 2 then
                    put(u, -1, w, if math.abs(u) == 2 then WHITE else GRAY)
                end
            end
        end
    end,

    -- The Inferno: the obsidian frame mods/infernoportal lights, on scorched
    -- ground, between two broken pillars crowned with glowrock.
    inferno = function(put)
        frame(put, function() return OBSIDIAN end)
        for w = -3, 3 do
            for u = -3, 3 do
                if u * u + w * w <= 10 and not (w == 0 and math.abs(u) <= 1) then
                    put(u, -1, w, if (u * 7 + w * 3) % 5 == 0 then ORANGE
                        elseif (u + w) % 3 == 0 then BLACK_TERRA else RED_TERRA)
                end
            end
        end
        for v = 0, 2 do put(-3, v, 0, OBSIDIAN) end
        put(-3, 3, 0, GLOW)
        for v = 0, 4 do put(3, v, 0, OBSIDIAN) end
        put(3, 5, 0, GLOW)
    end,

    -- The arena: a techbase door, deepslate and black with hazard stripes, a
    -- red light over it.
    arena = function(put)
        frame(put, function() return BLACK end)
        for v = -1, 4 do
            for _, u in { -2, 2 } do put(u, v, 0, if v % 2 == 0 then YELLOW else BLACK) end
            for _, u in { -3, 3 } do put(u, v, 0, DEEPSLATE) end
        end
        for u = -3, 3 do
            put(u, 3, 0, DEEPSLATE)
            put(u, 4, 0, if u == 0 then RED else DEEPSLATE)
            put(u, 5, 0, if math.abs(u) == 2 then LAMP else GRAY)
        end
        for w = -3, 3 do
            for u = -3, 3 do
                if not (w == 0 and math.abs(u) <= 3) then
                    put(u, -1, w, if math.abs(w) == 2 then (if u % 2 == 0 then YELLOW else BLACK) else GRAY)
                end
            end
        end
    end,

    -- The arpg lands: a pointed arch of mossy brick, half fallen, among graves.
    arpg = function(put)
        frame(put, function(u, v) return if (u + v) % 3 == 0 then MOSSY else BRICKS end)
        for v = -1, 3 do put(-2, v, 0, if v % 2 == 0 then BRICKS else MOSSY) end
        for v = -1, 1 do put(2, v, 0, MOSSY) end
        put(-1, 3, 0, BRICKS)
        put(0, 3, 0, COBBLE)
        put(0, 4, 0, MOSSY)
        put(-2, 4, 0, GLOW)
        put(2, 2, 0, GLOW)
        for w = -3, 3 do
            for u = -3, 3 do
                if u * u + w * w <= 12 and not (w == 0 and math.abs(u) <= 2) then
                    put(u, -1, w, if (u + 2 * w) % 3 == 0 then MOSSY_COBBLE
                        elseif (u * w) % 2 == 0 then COARSE else COBBLE)
                end
            end
        end
        for _, g in { { -3, 3 }, { 3, 2 }, { -3, -3 }, { 3, -2 } } do
            put(g[1], 0, g[2], SMOOTH)
            put(g[1], 1, g[2], SMOOTH)
        end
    end,

    -- The test chambers: a white panelled wall, the hole edged in grey, a blue
    -- stripe on one side and an orange one on the other, a light strip above.
    chambers = function(put)
        frame(put, function() return LIGHT_GRAY end)
        for v = -1, 4 do
            for _, u in { -3, -2, 2, 3 } do put(u, v, 0, WHITE) end
        end
        for v = 0, 2 do
            put(-2, v, 0, BLUE)
            put(2, v, 0, ORANGE)
        end
        for u = -1, 1 do
            put(u, 3, 0, WHITE)
            put(u, 4, 0, LAMP)
        end
        for w = -2, 2 do
            for u = -2, 2 do
                if w ~= 0 then put(u, -1, w, if (u + w) % 2 == 0 then WHITE else LIGHT_GRAY) end
            end
        end
    end,

    -- The backrooms: a doorway in a stretch of yellow wall, a tube light over
    -- it, damp brown carpet in front.
    backrooms = function(put)
        frame(put, function() return LIGHT_GRAY end)
        for v = -1, 3 do
            for _, u in { -3, -2, 2, 3 } do put(u, v, 0, YELLOW) end
        end
        for u = -1, 1 do put(u, 3, 0, YELLOW) end
        for u = -3, 3 do put(u, 4, 0, if math.abs(u) <= 1 then LAMP else YELLOW) end
        for w = -2, 2 do
            for u = -2, 2 do
                if w ~= 0 then put(u, -1, w, BROWN) end
            end
        end
    end,

    -- The parkour course: an orange and black striped scaffold, a stair of
    -- grey blocks climbing its left side, a bar over the top with an orange
    -- rope hanging from it, and grey ground in front.
    parkour = function(put)
        frame(put, function(u, v) return if (u + v) % 2 == 0 then ORANGE else BLACK end)
        for v = -1, 4 do
            put(-3, v, 0, if v % 2 == 0 then ORANGE else BLACK)
            put(3, v, 0, if v % 2 == 0 then BLACK else ORANGE)
        end
        for u = -3, 3 do put(u, 5, 0, GRAY) end
        for k = 0, 3 do
            for v = -1, k - 1 do put(-4 - (3 - k), v, 1, LIGHT_GRAY) end
        end
        for v = 2, 4 do put(2, v, 1, ORANGE_WOOL) end
        put(2, 5, 1, GRAY)
        for w = -2, 2 do
            for u = -2, 2 do
                if w ~= 0 then put(u, -1, w, GRAY) end
            end
        end
    end,

    -- The laboratory: a dead frame of dark steel, and the seven pedestals of
    -- mods/keys in front of it, four on the left of the way in, three on the
    -- right. Only in a world that has the laboratory (and so mods/keys).
    laboratorio = function(put)
        local FRAME, PEDESTAL = id("keys:frame"), id("keys:pedestal")
        frame(put, function() return FRAME end)
        for v = -1, 4 do
            put(-2, v, 0, FRAME)
            put(2, v, 0, FRAME)
        end
        for v = 3, 4 do
            for u = -2, 2 do put(u, v, 0, FRAME) end
        end
        for w = -1, 5 do
            for u = -4, 4 do
                if w ~= 0 or math.abs(u) >= 2 then put(u, -1, w, if math.abs(u) <= 1 then FRAME else DEEPSLATE) end
            end
        end
        for w = 1, 4 do put(-3, 0, w, PEDESTAL) end
        for w = 1, 3 do put(3, 0, w, PEDESTAL) end
    end,

    -- The front: a concrete bunker door in a wall of sandbags, a red flag on
    -- one side and a blue one on the other, trodden ground before it.
    front = function(put)
        frame(put, function() return LIGHT_GRAY end)
        for v = -1, 3 do
            put(-2, v, 0, GRAY)
            put(2, v, 0, GRAY)
        end
        for u = -2, 2 do put(u, 3, 0, GRAY) end
        for u = -5, 5 do
            if math.abs(u) >= 3 then
                for v = 0, if math.abs(u) == 5 then 0 else 1 do put(u, v, 0, SANDBAG) end
            end
        end
        for _, f in { { -4, RED }, { 4, id("colors:blue_concrete") } } do
            for v = 2, 6 do put(f[1], v, 0, GRAY) end
            put(f[1] + (if f[1] < 0 then -1 else 1), 6, 0, f[2])
            put(f[1] + (if f[1] < 0 then -1 else 1), 5, 0, f[2])
        end
        for w = -4, 4 do
            for u = -3, 3 do
                if w ~= 0 or math.abs(u) >= 2 then put(u, -1, w, if (u * 3 + w * 5) % 4 == 0 then GREEN else COARSE) end
            end
        end
    end,

    -- The assault: a gate of black basalt between two towers with Basoalth's
    -- violet light on them, an iron portcullis raised over the hole, scorched
    -- ground before it.
    assalto = function(put)
        local BASALT, EMBER = id("colors:black_concrete"), id("colors:purple_concrete")
        frame(put, function() return GRAY end)
        for v = -1, 5 do
            for _, u in { -3, -2, 2, 3 } do put(u, v, 0, BASALT) end
        end
        for u = -1, 1 do
            put(u, 3, 0, BASALT)
            put(u, 2, 0, GRAY)
        end
        for u = -3, 3, 2 do put(u, 6, 0, BASALT) end
        put(-3, 7, 0, EMBER)
        put(3, 7, 0, EMBER)
        for w = -4, 4 do
            for u = -3, 3 do
                if w ~= 0 or math.abs(u) >= 2 then put(u, -1, w, if (u * 5 + w * 3) % 4 == 0 then RED_TERRA else BLACK_TERRA) end
            end
        end
    end,
}

-- Builds a theme round the hole at (hx, y0, hz) -- its lowest cell -- with a
-- clear way through; facing +Z for the first opening, +X for every other.
local function build(theme: (Put) -> (), hx: number, y0: number, hz: number, along_z: boolean)
    local put: Put = if along_z
        then function(u, v, w, b) core.set_block(hx + u, y0 + v, hz + w, b) end
        else function(u, v, w, b) core.set_block(hx + w, y0 + v, hz + u, b) end
    for w = -2, 2 do
        for v = 0, 2 do
            for u = -1, 1 do
                if w ~= 0 or u == 0 then put(u, v, w, AIR) end
            end
        end
    end
    theme(put)
end

local PLAZA = core.plaza() ~= nil

if PLAZA then
    -- Where the gates past the first two stand, by room: round the spawn, which is
    -- in front of the first opening (40.5 21 44.5, facing +Z).
    local SPOTS: { [string]: { number } } = {
        pista = { 52.5, 32.5 }, inferno = { 60.5, 60.5 }, arena = { 28.5, 64.5 },
        arpg = { 78.5, 34.5 }, chambers = { 84.5, 84.5 }, backrooms = { 16.5, 28.5 },
        parkour = { 16.5, 52.5 }, laboratorio = { 62.5, 76.5 }, front = { 90.5, 58.5 },
        assalto = { 90.5, 76.5 },
    }

    -- Mouths 8 to 14 are mods/keys' arches; 15 is the front's and 16 the
    -- assault's, which hold no key and so have a gate and no arch.
    for _, k in { 0, 1, 2, 3, 4, 5, 6, 7, 15, 16 } do
        local room = core.portal_room(k)
        local theme = THEMES[room]
        if theme == nil then continue end
        if k >= 2 then
            local spot = SPOTS[room]
            -- Past the eighth the server has no place of its own for a mouth: the
            -- plaza's height is the first one's.
            core.set_portal(k, spot[1], core.get_portal(if k >= 8 then 0 else k, 1), spot[2])
        end
        local hx = math.floor(core.get_portal(k, 0))
        local y0 = math.floor(core.get_portal(k, 1)) - 1
        local hz = math.floor(core.get_portal(k, 2))
        build(theme, hx, y0, hz, k == 0)
        core.log(`gates: opening {k} to '{room}' at {hx} {y0} {hz}`)
    end
    return
end

-- ================================================================== survival
--
-- A world without a plaza (core.plaza() nil): the worlds are SITES scattered
-- over the land, one in most cells of CELL x CELL, each a gate of its world's
-- theme and, beside it, the arch its way out comes back by. A site is a
-- function of the seed and its cell, never stored: its world, where it stands,
-- and how hard what is behind it is (d, 1 to 10), growing with the distance
-- from the spawn -- a ring every RING nodes, one up or down now and then. A
-- row of ten lamps over the gate, d of them lit, says it from afar.
--
-- The server has one mouth per world (and one arch), so the sites share it:
-- every half second each world's mouth and arch are moved (core.portal_place)
-- to its site nearest a player, within NEAR. Whoever walks in carries the site
-- with them (core.player_carry "site=... d=... s=... t=..."), for the world
-- behind to build that site's level; coming back, they are put in front of
-- their own site whatever the mouth did meanwhile.
--
-- The laboratory is one site, 1500 to 2200 from the spawn, and keeps its
-- mouth (mods/keys locks it as on the plaza). The front and the assault stand
-- in about one cell in nine past 1500, sealed until the ending (mods/keys).

local CELL, RING, NEAR = 256, 300, 48
local WORLDS = { "pista", "inferno", "arena", "arpg", "chambers", "backrooms", "parkour" }
local SEED = core.seed()
local SX, _, SZ = core.world_spawn()
local spawn_x, spawn_z = SX or 0, SZ or 0

local function mul32(a: number, b: number): number
    local hi = math.floor(a / 65536)
    local lo = a % 65536
    return ((hi * b) % 65536 * 65536 + lo * b) % 4294967296
end

local function hash(x: number, z: number, salt: number): number
    local h = mul32(x + 0x9E3779B9, 374761393)
    h = bit32.bxor(h, mul32(z + 0x85EBCA6B, 668265263))
    h = bit32.bxor(h, mul32(salt % 4294967296, 2246822519))
    h = bit32.bxor(h, bit32.rshift(h, 13))
    return mul32(h, 1274126177)
end

export type Site = { id: string, x: number, y: number, z: number, room: string, d: number }

-- Dry, over the sea, and level: the ground 3 out every way within 2 of the
-- middle's. The hole's lowest cell is the first air.
local function ground_at(x: number, z: number): number?
    local y = core.terrain_height(x, z)
    if y < 20 or core.water_level(x, z) ~= y or core.biome(x, z) == "core:peaks" then return nil end
    for _, d in { { 3, 0 }, { -3, 0 }, { 0, 3 }, { 0, -3 }, { 8, 0 }, { 8, 3 } } do
        local g = core.terrain_height(x + d[1], z + d[2])
        if math.abs(g - y) > 2 or core.water_level(x + d[1], z + d[2]) ~= g then return nil end
    end
    return y
end

local function difficulty(x: number, z: number, h: number): number
    local dist = math.sqrt((x - spawn_x) ^ 2 + (z - spawn_z) ^ 2)
    if dist < 250 then return 1 end
    local jitter = ({ -1, 0, 0, 1 })[bit32.rshift(h, 24) % 4 + 1]
    return math.clamp(1 + math.floor(dist / RING) + jitter, 1, 10)
end

-- The ones that are not a cell's: the laboratory, and one site of every world
-- round the spawn, the first 60 to 120 out and the last 750 to 950, so a new
-- player meets each world once, easy (d 1 to 3), before the cells' harder
-- ones. The first dry level place round a ring the seed points at.
local function placed(salt: number, r0: number, r1: number, room: string, d: number): Site?
    local h = hash(salt, salt, SEED)
    local a0 = (h % 3600) / 3600 * 2 * math.pi
    for i = 0, 95 do
        local r = r0 + (r1 - r0) * ((i * 7) % 16) / 15
        local a = a0 + i * 0.37
        local x = math.floor(spawn_x + math.cos(a) * r)
        local z = math.floor(spawn_z + math.sin(a) * r)
        local y = ground_at(x, z)
        if y then
            return { id = `{room}@{x},{z}`, x = x, y = y, z = z, room = room, d = d }
        end
    end
    return nil
end
local SPECIAL: { Site } = {}
do
    -- The parkour or the test chambers first, the rest in an order of the seed.
    local order = { if SEED % 2 == 0 then "parkour" else "chambers" }
    local rest = {}
    for _, w in WORLDS do
        if w ~= order[1] then table.insert(rest, w) end
    end
    for i = #rest, 2, -1 do
        local j = hash(i, 0, SEED + 0x5A1) % i + 1
        rest[i], rest[j] = rest[j], rest[i]
    end
    for _, w in rest do table.insert(order, w) end
    local RINGS = { { 60, 120 }, { 200, 330 }, { 330, 460 }, { 440, 620 }, { 560, 760 }, { 660, 860 }, { 750, 950 } }
    for i, w in order do
        local r = RINGS[i]
        local s = placed(0xF125 + i * 101, r[1], r[2], w, if r[1] < 250 then 1 elseif r[1] < 500 then 2 else 3)
        if s then table.insert(SPECIAL, s) end
    end
end
local FIRST_SITE: Site? = SPECIAL[1]
local LAB_SITE = placed(0x1AB, 1500, 2200, "laboratorio", 10)
if LAB_SITE then table.insert(SPECIAL, LAB_SITE) end

local function near_special(x: number, z: number): boolean
    for _, s in SPECIAL do
        if math.abs(s.x - x) < 60 and math.abs(s.z - z) < 60 then return true end
    end
    return false
end

local CACHE: { [string]: Site | false } = {}

-- The site of cell (cx, cz), or nil.
local function site_of(cx: number, cz: number): Site?
    local key = `{cx},{cz}`
    local hit = CACHE[key]
    if hit ~= nil then return if hit then hit else nil end
    local s: Site? = nil
    local h = hash(cx, cz, SEED + 0x517E)
    -- Five cells in six have one, at the first of six places in it the
    -- ground allows.
    if h % 6 ~= 0 and core.portal_room(0) ~= "" then
        -- Every world once in any seven cells along a row or a column.
        local room = WORLDS[(cx * 3 + cz * 5 + SEED % 7) % 7 + 1]
        for t = 0, 5 do
            local ht = hash(cx, cz, SEED + 0x517E + t * 7919)
            local x = cx * CELL + 40 + bit32.rshift(ht, 4) % (CELL - 80)
            local z = cz * CELL + 40 + bit32.rshift(ht, 12) % (CELL - 80)
            local dist = math.sqrt((x - spawn_x) ^ 2 + (z - spawn_z) ^ 2)
            if dist > 1500 and bit32.rshift(h, 20) % 9 == 0 then
                room = if bit32.band(h, 65536) ~= 0 then "front" else "assalto"
            end
            local y = ground_at(x, z)
            if y and not near_special(x, z) then
                s = { id = `{room}@{x},{z}`, x = x, y = y, z = z, room = room, d = difficulty(x, z, h) }
                break
            end
        end
    end
    CACHE[key] = s or false
    return s
end

-- Every site within r of (x, z), the two special ones included.
local function sites_near(x: number, z: number, r: number): { Site }
    local out = {}
    for cz = math.floor((z - r) / CELL), math.floor((z + r) / CELL) do
        for cx = math.floor((x - r) / CELL), math.floor((x + r) / CELL) do
            local s = site_of(cx, cz)
            if s and (s.x - x) ^ 2 + (s.z - z) ^ 2 <= r * r then table.insert(out, s) end
        end
    end
    for _, s in SPECIAL do
        if (s.x - x) ^ 2 + (s.z - z) ^ 2 <= r * r then table.insert(out, s) end
    end
    return out
end

-- The mouths: each room's way in (0..7, 15, 16) and its arch (8..14, the way
-- out of it comes back by).
local MOUTH: { [string]: number } = {}
local ARCH: { [string]: number } = {}
for k = 0, 19 do
    local room = core.portal_room(k)
    if room ~= "" then
        if k <= 7 or k >= 15 then MOUTH[room] = k else ARCH[room] = k end
    end
end

-- The arch beside a gate, 8 along it: the frame mods/keys gives the plaza's.
local function arch(put: Put)
    local FRAME, LAMP_ON = id("keys:exit_frame"), id("keys:exit_lamp")
    for v = -1, 3 do
        for u = -1, 1 do
            if u ~= 0 or v == -1 or v >= 2 then put(u, v, 0, FRAME) end
        end
    end
    put(0, 3, 0, LAMP_ON)
end

local function lamps(put: Put, d: number)
    for i = 1, 10 do put(i - 6, 9, 0, if i <= d then GLOW else BLACK) end
end

-- Built by the stage as the columns are made: a site stands well inside its
-- cell, and cells are whole columns, so a column builds its own cell's site
-- (and a special one that falls in it), the engine keeping the writes to it.
core.register_mapgen("gates:sites", function(ctx)
    local found = {}
    local s = site_of(math.floor(ctx.min_x / CELL), math.floor(ctx.min_z / CELL))
    if s then table.insert(found, s) end
    for _, t in SPECIAL do table.insert(found, t) end
    for _, t in found do
        if t.x + 12 < ctx.min_x or t.x - 12 > ctx.max_x or t.z + 14 < ctx.min_z or t.z - 8 > ctx.max_z then
            continue
        end
        local theme = THEMES[t.room]
        if theme == nil then continue end
        -- A clearing first: the trees' stages ran before this one.
        build(function(put)
            for w = -5, 6 do
                for u = -6, 11 do
                    for v = 0, 14 do put(u, v, w, AIR) end
                end
            end
        end, t.x, t.y, t.z, false)
        build(theme, t.x, t.y, t.z, false)
        if t.room ~= "laboratorio" then
            build(function(put) lamps(put, t.d) end, t.x, t.y, t.z, false)
        end
        if ARCH[t.room] then build(arch, t.x, t.y, t.z + 8, false) end
    end
end)

-- The laboratory's mouth stays at its site (mods/keys' lock reads it there).
if LAB_SITE and MOUTH.laboratorio then
    core.set_portal(MOUTH.laboratorio, LAB_SITE.x + 0.5, LAB_SITE.y + 1, LAB_SITE.z + 0.5)
    core.log(`gates: the laboratory at {LAB_SITE.x} {LAB_SITE.y} {LAB_SITE.z}`)
end
-- The rest wait in the rock under the spawn until a site wants them.
for room, k in MOUTH do
    if room ~= "laboratorio" then core.set_portal(k, spawn_x + k * 3 + 0.5, 4, spawn_z + 0.5) end
end
for _, k in ARCH do core.set_portal(k, spawn_x + k * 3 + 0.5, 4, spawn_z + 4.5) end
if FIRST_SITE then
    core.log(`gates: survival, the first site ('{FIRST_SITE.room}') at {FIRST_SITE.x} {FIRST_SITE.y} {FIRST_SITE.z}`)
end

-- Who left by which site (shared storage "site:<who>"), so coming back by the
-- arch puts them in front of it -- the mouth may be at another site by then.
local function who(p: number): string
    local key = core.player_key(p)
    return if key ~= "" then key else "name:" .. core.player_name(p)
end

local at_site: { [string]: Site } = {}   -- each room's mouth: the site it is at now
local away: { [string]: Site } = {}      -- the site of somebody gone through, by who
local near: { [number]: Site } = {}      -- the site each player here stands at, if any
local who_is: { [number]: string } = {}  -- each player here, by who
local since = 0.0

local function place(room: string, s: Site)
    local k = MOUTH[room]
    if k == nil or room == "laboratorio" then return end
    if core.portal_place(k, s.x + 0.5, s.y + 1, s.z + 0.5, 1, 1, 0.5, 1.0) == 1 then
        at_site[room] = s
        local a = ARCH[room]
        if a then core.portal_place(a, s.x + 0.5, s.y + 1, s.z + 8.5, 1, 1, 0.5, 1.0) end
    end
end

core.event.on("core:tick", function(ev)
    since += ev.dt
    if since < 0.5 then return end
    since = 0.0

    local now: { [number]: boolean } = {}
    local want: { [string]: { site: Site, dist: number } } = {}
    for _, p in core.players() do
        now[p] = true
        local x, y, z = core.player_pos(p)
        if x == nil then continue end
        local me = who(p)
        if who_is[p] == nil then
            -- Just come: back from a world, put in front of the site left by.
            local line = core.player_carried(p)
            local id = string.match(line, "site=([^ ]+)")
            local gone = away[me]
            if gone and (id == nil or id == gone.id) and (x - gone.x) ^ 2 + (z - gone.z) ^ 2 > 12 * 12 then
                core.player_place(p, gone.x + 3.5, gone.y, gone.z + 0.5, -1, 0, 0)
            end
            away[me] = nil
        end
        for _, s in sites_near(x, z, NEAR) do
            local dist = (s.x - x) ^ 2 + (s.z - z) ^ 2
            local w = want[s.room]
            if w == nil or dist < w.dist then want[s.room] = { site = s, dist = dist } end
            -- Close enough to walk in: what the world behind needs to know.
            if dist < 6 * 6 and near[p] ~= s then
                core.player_carry(p, `site={s.id} d={s.d} s={hash(s.x, s.z, SEED) % 1000000} t={s.room}`)
                near[p] = s
            end
        end
        who_is[p] = me
    end
    -- Gone from here since the last look, standing at a site: through it.
    for p, me in who_is do
        if not now[p] then
            if near[p] then away[me] = near[p] end
            who_is[p], near[p] = nil, nil
        end
    end
    -- Somebody gone through keeps their world's mouth at their site (the arch
    -- brings them back there); else the site nearest anybody has it.
    for _, s in away do
        if want[s.room] == nil then want[s.room] = { site = s, dist = 0 } end
    end
    for room, w in want do
        local cur = at_site[room]
        if cur == nil or cur.id ~= w.site.id then place(room, w.site) end
    end
end)

-- For other mods (villagers' directions): the sites round a point.
--   core.event.emit("gates:ask", { x = .., z = .., r = .., tag = "..." })
-- answers at once with gates:sites { tag, list = "room d x z;room d x z;..." }.
core.event.define("gates:ask", { fields = { x = "number", z = "number", r = "number", tag = "string" },
                                 cancellable = false })
core.event.define("gates:sites", { fields = { tag = "string", list = "string" }, cancellable = false })
core.event.on("gates:ask", function(ev)
    local parts = {}
    for _, s in sites_near(ev.x, ev.z, math.min(ev.r, 3000)) do
        table.insert(parts, `{s.room} {s.d} {s.x} {s.z}`)
    end
    core.event.emit("gates:sites", { tag = ev.tag, list = table.concat(parts, ";") })
end)
