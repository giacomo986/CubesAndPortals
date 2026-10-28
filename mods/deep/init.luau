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
-- armour), takes a long fight.
core.register_mob("deep:guardian", GUARDIAN, 160, 2.0, "melee", 5, 2.4, 1.2, "deep:shard", 12)

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

-- The noise of one lattice size over this column, as a function of x, y, z.
local function lattice(ctx, sx: number, sy: number, sz: number, salt: number)
    local cache: { [number]: number } = {}
    return function(x: number, y: number, z: number): number
        local fx, fy, fz = x / sx, y / sy, z / sz
        local i, j, k = math.floor(fx), math.floor(fy), math.floor(fz)
        local u, v, w = fx - i, fy - j, fz - k
        u, v, w = u * u * (3 - 2 * u), v * v * (3 - 2 * v), w * w * (3 - 2 * w)
        local function c(a: number, b: number, d: number): number
            local key = ((a + 4096) * 512 + (b + 256)) * 8192 + (d + 4096)
            local val = cache[key]
            if val == nil then
                val = at(a, b, d, salt)
                cache[key] = val
            end
            return val
        end
        local x00 = c(i, j, k) + (c(i + 1, j, k) - c(i, j, k)) * u
        local x10 = c(i, j + 1, k) + (c(i + 1, j + 1, k) - c(i, j + 1, k)) * u
        local x01 = c(i, j, k + 1) + (c(i + 1, j, k + 1) - c(i, j, k + 1)) * u
        local x11 = c(i, j + 1, k + 1) + (c(i + 1, j + 1, k + 1) - c(i, j + 1, k + 1)) * u
        local y0 = x00 + (x10 - x00) * v
        local y1 = x01 + (x11 - x01) * v
        return y0 + (y1 - y0) * w
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
    local small = lattice(ctx, 8, 6, 8, ctx.seed + 0x5A1)
    local big = lattice(ctx, 24, 12, 24, ctx.seed + 0xB16)

    for z = ctx.min_z, ctx.max_z do
        for x = ctx.min_x, ctx.max_x do
            local d = math.sqrt((x - hol.x) ^ 2 + (z - hol.z) ^ 2)
            if d <= R then
                local wall = d > R - WALL
                local shut = (d / (R - WALL)) ^ 4 * 0.6            -- caves close towards the edge
                local da = math.sqrt((x - hol.ax) ^ 2 + (z - hol.az) ^ 2)
                local tunnel = to_segment(x, z, hol.x, hol.z, hol.ax, hol.az)
                for y = BOTTOM, TOP do
                    local id = SLATE
                    if wall or y == BOTTOM then
                        id = bedrock
                    else
                        local n = big(x, y, z) * 0.65 + small(x, y, z) * 0.35 - shut
                        local open = n > 0.56 and y >= BOTTOM + 3 and y <= TOP - 3
                        -- The chamber under the sinkhole, the arena, and the tunnel between.
                        if (d <= 7 and y >= CHAMBER and y <= CHAMBER + 7 - math.floor(d / 2))
                            or (da * da + ((y - ARENA_Y) * 1.6) ^ 2 <= ARENA_R * ARENA_R and y >= ARENA_Y - 4)
                            or (tunnel <= 1.6 and y >= CHAMBER + (ARENA_Y - 4 - CHAMBER) * math.clamp(1 - (d / ARENA_OFF), 0, 1)
                                and y <= CHAMBER + 2 + (ARENA_Y - 4 - CHAMBER) * math.clamp(1 - (d / ARENA_OFF), 0, 1)) then
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
                    core.set_block(x, y, z, id)
                end
                -- The sinkhole: open from the surface down to the chamber,
                -- through the bedrock, a pool at its foot.
                if math.abs(x - hol.x) <= 2 and math.abs(z - hol.z) <= 2 then
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

    -- What grows: on floors, glowcaps and moss; on ceilings, crystals.
    for z = ctx.min_z, ctx.max_z do
        for x = ctx.min_x, ctx.max_x do
            local d = math.sqrt((x - hol.x) ^ 2 + (z - hol.z) ^ 2)
            if d <= R - WALL and not (math.abs(x - hol.x) <= 2 and math.abs(z - hol.z) <= 2) then
                for y = BOTTOM + 2, TOP - 1 do
                    if core.get_block(x, y, z) == 0 then
                        local below, above = core.get_block(x, y - 1, z), core.get_block(x, y + 1, z)
                        local r = hash(x * 13 + y, z, ctx.seed + 0x6E0) % 100
                        if below == SLATE then
                            if r < 7 then
                                core.set_block(x, y, z, GLOWCAP)
                            elseif r < 40 then
                                core.set_block(x, y, z, MOSS)
                            end
                        elseif above == SLATE and r < 3 then
                            core.set_block(x, y, z, CRYSTAL)
                        end
                    end
                end
            end
            if math.abs(x - hol.ax) <= 0 and math.abs(z - hol.az) <= 0 then
                core.set_block(x, ARENA_Y - 4, z, SLATE)
                core.set_block(x, ARENA_Y - 3, z, ALTAR)
            end
        end
    end
end)

-- ------------------------------------------------------------ the guardian

local function hollow_at(x: number, z: number): Hollow?
    return hollow_of(math.floor(x / SQUARE), math.floor(z / SQUARE), SEED)
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
            if h then
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
