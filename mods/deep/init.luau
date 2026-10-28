--!strict
-- deep: the HOLLOWS, under the world's floor.
--
-- One square of 512 in two has a hollow: a sealed cavern country under its
-- bedrock, y -60 to -1, 80 blocks round its middle, walled in by bedrock (the
-- engine's world stops at y 0; this mod builds below it -- kWorldBottom). It
-- is reached by a SINKHOLE at its middle: a five-wide shaft from the surface
-- through the bedrock down to a chamber at -28, rungs down its north wall and
-- a pool at its foot to land in.
--
-- Inside, caverns wander through dark slate: GLOWCAPS and glowing moss on
-- their floors, CRYSTALS on their ceilings (a crystal dug gives a shard), lava
-- lakes at the bottom. A tunnel runs from the chamber to an ARENA forty blocks
-- off, where an altar stands; whoever comes near it wakes its GUARDIAN, a
-- giant of slate and crystal. Beaten, it drops its heart, and the altar is
-- quiet for good.
--
-- Built per column like mods/depths: everything is a function of the seed and
-- the square (hollow_of), so every column builds only its own cells. The
-- caverns are value noise on two lattices (8 x 6 x 8 and 24 x 12 x 24),
-- pulled shut towards the hollow's edge so they never reach its wall.

local SLATE   = core.register_node("deep:slate", 52, 50, 60, true, 0, "", 1, 2.5, "stone")
local GLOWCAP = core.register_node("deep:glowcap", 80, 210, 230, false, 14, "", 0, 0.2, "")
core.node_box(GLOWCAP, 0.4375, 0, 0.4375, 0.5625, 0.35, 0.5625)
core.node_box(GLOWCAP, 0.2, 0.35, 0.2, 0.8, 0.55, 0.8)
local MOSS    = core.register_node("deep:glowmoss", 60, 170, 120, false, 8, "", 0, 0.1, "")
core.node_box(MOSS, 0, 0, 0, 1, 1 / 16, 1)
local CRYSTAL = core.register_node("deep:crystal", 190, 120, 250, false, 12, "", 1, 1.5, "glass")
core.node_box(CRYSTAL, 0.35, 0.2, 0.35, 0.65, 1, 0.65)
core.node_box(CRYSTAL, 0.15, 0.5, 0.45, 0.4, 1, 0.6)
core.register_item("deep:shard", 64)
core.node_drop_item(CRYSTAL, "deep:shard")
local ALTAR   = core.register_node("deep:altar", 120, 80, 160, true, 9, "", 99, 1000.0, "stone")
core.node_box(ALTAR, 0.05, 0, 0.05, 0.95, 0.4, 0.95)
core.node_box(ALTAR, 0.25, 0.4, 0.25, 0.75, 1.0, 0.75)
core.register_item("deep:heart", 1)

local GUARDIAN = core.register_model("deep:guardian", "guardian.gltf")
-- Slower than a walk, hits like two zombies (four blows down a player without
-- armour), takes a long fight. Never out of the dark on its own: only its
-- altar wakes it.
core.register_mob("deep:guardian", GUARDIAN, 160, 2.0, "melee", 5, 2.4, 1.2, "deep:shard", 12,
                  0, false, 0, false, "deep:never")
-- What lives down here: lurkers, pale and quick. Not out of the dark as other
-- creatures come: the glowcaps leave little of a hollow at light 7 or less,
-- so this mod puts them on its slate round whoever is down here (below).
core.register_mob("deep:lurker", core.register_model("deep:lurker", "lurker.gltf"), 14, 3.0, "melee",
                  2, 1.6, 0.8, "deep:glowcap", 1, 0, false, 0, false, "deep:never")
local LURKERS = 4    -- at most, within 32 of a player in a hollow

local SQUARE = 512
local R = 80         -- a hollow's reach from its middle, wall included
local WALL = 3       -- the bedrock round it
local TOP, BOTTOM = -1, -61
local LAVA_BELOW = -51
local CHAMBER = -28  -- the sinkhole's floor
local ARENA_Y, ARENA_R, ARENA_OFF = -40, 10, 40
local PLAZA_LO, PLAZA_HI = 20, 100

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

type Hollow = { x: number, z: number, ax: number, az: number, h: number }

-- This square's hollow, or nil: its middle (where the sinkhole is) and its arena.
local function hollow_of(sx: number, sz: number, seed: number): Hollow?
    local h = hash(sx, sz, seed + 0xD33F)
    if bit32.band(h, 1) == 0 then
        return nil
    end
    local x = sx * SQUARE + R + 8 + bit32.rshift(h, 1) % (SQUARE - 2 * R - 16)
    local z = sz * SQUARE + R + 8 + bit32.rshift(h, 12) % (SQUARE - 2 * R - 16)
    if x + 12 >= PLAZA_LO and x - 12 <= PLAZA_HI and z + 12 >= PLAZA_LO and z - 12 <= PLAZA_HI then
        return nil
    end
    if core.water_level(x, z) > core.terrain_height(x, z) then
        return nil   -- not out of a lake
    end
    local a = bit32.rshift(h, 22) % 360 / 180 * math.pi
    return { x = x, z = z, ax = x + math.floor(math.cos(a) * ARENA_OFF), az = z + math.floor(math.sin(a) * ARENA_OFF), h = h }
end

-- Value noise at a lattice point, 0..1.
local function at(i: number, j: number, k: number, salt: number): number
    return hash(i * 7919 + j, k, salt) / 4294967296
end

-- The noise of one lattice size (sx, sy, sz) down one column, BOTTOM..TOP,
-- into out[y]. Down a column only y moves, so the lattice is blended in the
-- plane once per lattice layer and every cell is one blend between two
-- layers. `cache` keeps the lattice points the stage's other columns share.
local function column_noise(x: number, z: number, sx: number, sy: number, sz: number, salt: number,
                            cache: { [number]: number }, out: { number })
    local fx, fz = x / sx, z / sz
    local i, k = math.floor(fx), math.floor(fz)
    local u, w = fx - i, fz - k
    u, w = u * u * (3 - 2 * u), w * w * (3 - 2 * w)
    local function c(a: number, b: number, d: number): number
        local key = ((a + 4096) * 512 + (b + 256)) * 8192 + (d + 4096)
        local val = cache[key]
        if val == nil then
            val = at(a, b, d, salt)
            cache[key] = val
        end
        return val
    end
    local layer: { [number]: number } = {}
    for j = math.floor(BOTTOM / sy), math.floor(TOP / sy) + 1 do
        local x0 = c(i, j, k) + (c(i + 1, j, k) - c(i, j, k)) * u
        local x1 = c(i, j, k + 1) + (c(i + 1, j, k + 1) - c(i, j, k + 1)) * u
        layer[j] = x0 + (x1 - x0) * w
    end
    for y = BOTTOM, TOP do
        local fy = y / sy
        local j = math.floor(fy)
        local v = fy - j
        v = v * v * (3 - 2 * v)
        out[y - BOTTOM + 1] = layer[j] + (layer[j + 1] - layer[j]) * v
    end
end

-- Distance from a point to the segment a-b, in the plane.
local function to_segment(px: number, pz: number, ax: number, az: number, bx: number, bz: number): number
    local dx, dz = bx - ax, bz - az
    local t = math.clamp(((px - ax) * dx + (pz - az) * dz) / (dx * dx + dz * dz), 0, 1)
    local qx, qz = ax + dx * t - px, az + dz * t - pz
    return math.sqrt(qx * qx + qz * qz)
end

local SEED = tonumber(core.storage_get("seed")) or 0

core.register_mapgen("deep:hollows", function(ctx)
    if SEED ~= ctx.seed then
        SEED = ctx.seed
        core.storage_set("seed", tostring(SEED))
    end
    local cx, cz = (ctx.min_x + ctx.max_x) / 2, (ctx.min_z + ctx.max_z) / 2
    local hw = hollow_of(math.floor(ctx.min_x / SQUARE), math.floor(ctx.min_z / SQUARE), ctx.seed)
    if hw == nil then
        return
    end
    local hol = hw :: Hollow
    if math.sqrt((cx - hol.x) ^ 2 + (cz - hol.z) ^ 2) > R + 24 then
        return   -- no cell of this column is in it
    end
    local bedrock, lava = core.node_id("core:bedrock"), core.node_id("rocks:lava")
    local water, rungs = core.node_id("lakes:water"), core.node_id("timber:rungs")
    local cache_small, cache_big = {}, {}
    local small, big, col = table.create(TOP - BOTTOM + 1, 0), table.create(TOP - BOTTOM + 1, 0), table.create(TOP - BOTTOM + 1, 0)
    local salt_small, salt_big, salt_grow = ctx.seed + 0x5A1, ctx.seed + 0xB16, ctx.seed + 0x6E0

    for z = ctx.min_z, ctx.max_z do
        for x = ctx.min_x, ctx.max_x do
            local d = math.sqrt((x - hol.x) ^ 2 + (z - hol.z) ^ 2)
            if d <= R then
                local wall = d > R - WALL
                local shaft = math.abs(x - hol.x) <= 2 and math.abs(z - hol.z) <= 2
                local shut = (d / (R - WALL)) ^ 4 * 0.6            -- caves close towards the edge
                local da = math.sqrt((x - hol.ax) ^ 2 + (z - hol.az) ^ 2)
                local tunnel = to_segment(x, z, hol.x, hol.z, hol.ax, hol.az)
                local caves = not wall and 1 - shut > 0.56         -- noise can open a cell here
                if caves then
                    column_noise(x, z, 24, 12, 24, salt_big, cache_big, big)
                    column_noise(x, z, 8, 6, 8, salt_small, cache_small, small)
                end
                local drop = (ARENA_Y - 4 - CHAMBER) * math.clamp(1 - (d / ARENA_OFF), 0, 1)
                for y = BOTTOM, TOP do
                    local id = SLATE
                    if wall or y == BOTTOM then
                        id = bedrock
                    else
                        local open = caves and y >= BOTTOM + 3 and y <= TOP - 3
                            and big[y - BOTTOM + 1] * 0.65 + small[y - BOTTOM + 1] * 0.35 - shut > 0.56
                        -- The chamber under the sinkhole, the arena, and the tunnel between.
                        if (d <= 7 and y >= CHAMBER and y <= CHAMBER + 7 - math.floor(d / 2))
                            or (da * da + ((y - ARENA_Y) * 1.6) ^ 2 <= ARENA_R * ARENA_R and y >= ARENA_Y - 4)
                            or (tunnel <= 1.6 and y >= CHAMBER + drop and y <= CHAMBER + 2 + drop) then
                            open = true
                        end
                        -- A floor of three under the chamber and its pool: a body
                        -- dropped in lands, and does not sink on into a cave.
                        if d <= 8 and y < CHAMBER and y >= CHAMBER - 3 then
                            open = false
                        end
                        if open then
                            id = if y <= LAVA_BELOW then lava else 0
                        end
                    end
                    col[y - BOTTOM + 1] = id
                end
                -- What grows: on floors, glowcaps and moss; on ceilings, crystals.
                if not wall and not shaft then
                    for y = BOTTOM + 2, TOP - 1 do
                        local n = y - BOTTOM + 1
                        if col[n] == 0 then
                            -- A cheap hash: what grows needs no better.
                            local r = bit32.bxor(x * 73856093, y * 19349663, z * 83492791, salt_grow) % 100
                            if col[n - 1] == SLATE then
                                if r < 7 then
                                    col[n] = GLOWCAP
                                elseif r < 40 then
                                    col[n] = MOSS
                                end
                            elseif col[n + 1] == SLATE and r < 3 then
                                col[n] = CRYSTAL
                            end
                        end
                    end
                end
                if x == hol.ax and z == hol.az then
                    col[ARENA_Y - 4 - BOTTOM + 1] = SLATE
                    col[ARENA_Y - 3 - BOTTOM + 1] = ALTAR
                end
                for y = BOTTOM, TOP do
                    core.set_block(x, y, z, col[y - BOTTOM + 1])
                end
                -- The sinkhole: open from the surface down to the chamber,
                -- through the bedrock, a pool at its foot.
                if shaft then
                    local g = core.terrain_height(x, z)
                    for y = CHAMBER, g + 2 do
                        core.set_block(x, y, z, if y <= CHAMBER + 2 then water else 0)
                    end
                    if z == hol.z - 2 and x == hol.x and rungs ~= 0 then
                        for y = CHAMBER + 3, g - 1 do
                            core.set_block(x, y, z, rungs)
                            core.set_facing(x, y, z, 4)
                        end
                    end
                elseif math.abs(x - hol.x) <= 3 and math.abs(z - hol.z) <= 3 then
                    core.set_block(x, core.terrain_height(x, z) - 1, z, SLATE)   -- a rim to see it by
                end
            end
        end
    end
end)

-- ------------------------------------------------------------ the guardian

local function hollow_at(x: number, z: number): Hollow?
    return hollow_of(math.floor(x / SQUARE), math.floor(z / SQUARE), SEED)
end

-- A lurker somewhere 12 to 30 off on the slate of a cave floor, if one of a
-- few tries finds such a floor. The engine sends it away when nobody is near.
local function lurk(x: number, y: number, z: number)
    for _ = 1, 4 do
        local a, r = math.random() * 2 * math.pi, 12 + math.random() * 18
        local lx, lz = math.floor(x + math.cos(a) * r), math.floor(z + math.sin(a) * r)
        for ly = math.floor(y) + 8, math.floor(y) - 12, -1 do
            local here = core.get_block(lx, ly, lz)
            if (here == 0 or here == MOSS) and core.get_block(lx, ly + 1, lz) == 0
                and core.get_block(lx, ly - 1, lz) == SLATE then
                core.spawn_mob("deep:lurker", lx + 0.5, ly, lz + 0.5)
                return
            end
        end
    end
end

local awake: { [string]: number } = {}   -- an arena's key -> its guardian's id
local since = 0
core.event.on("core:tick", function(ev)
    since += ev.dt
    if since < 1 then
        return
    end
    since = 0
    for _, p in core.players() do
        local x, y, z = core.player_pos(p)
        if x and y < 0 then
            local h = hollow_at(x, z)
            if h and (x - h.x) ^ 2 + (z - h.z) ^ 2 < (R - WALL) ^ 2 then
                if math.random() < 0.25 and core.monsters_near(x, y, z, 32) < LURKERS then
                    lurk(x, y, z)
                end
                local key = `{h.ax},{h.az}`
                local near = (x - h.ax) ^ 2 + (z - h.az) ^ 2 + (y - ARENA_Y) ^ 2 <= 18 * 18
                local id = awake[key]
                if id and core.mob_pos(id) == nil then
                    -- Gone while somebody fought in its arena: beaten.
                    awake[key] = nil
                    core.storage_set("beaten " .. key, "1")
                    core.drop_item(h.ax, ARENA_Y - 2, h.az, core.item_id("deep:heart"), 1)
                    core.play_sound("deep:guardian_death", h.ax, ARENA_Y, h.az)
                    for _, q in core.players() do
                        local qx, _, qz = core.player_pos(q)
                        if qx and (qx - h.ax) ^ 2 + (qz - h.az) ^ 2 < 40 * 40 then
                            core.notice(q, "The guardian falls; its heart lies by the altar")
                        end
                    end
                elseif near and id == nil and core.storage_get("beaten " .. key) == ""
                    and core.get_block(h.ax, ARENA_Y - 3, h.az) == ALTAR then
                    local gid = core.spawn_mob("deep:guardian", h.ax + 3.5, ARENA_Y - 3, h.az + 0.5)
                    if gid ~= 0 then
                        awake[key] = gid
                        core.notice(p, "Something wakes by the altar")
                    end
                end
            end
        end
    end
end)
