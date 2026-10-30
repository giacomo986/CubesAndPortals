--!strict
-- woods: The classic block game's other five trees -- birch, spruce, jungle, acacia and
-- dark oak -- each with its log, its planks and its leaves.
--
-- The plains are cut into regions of about fifty cells, and a region is one
-- kind of forest: birch, dark oak, jungle or acacia, or none, where mods/trees'
-- oaks stand alone. Spruces grow on the tundra, some with podzol under them.
-- A birch is tall and slim, a spruce a cone, a jungle tree twice a tree's
-- height with vines hanging off it, an acacia leans and spreads a flat crown,
-- and a dark oak stands on a trunk two by two under a crown seven across.
--
-- A log makes four planks, two planks four sticks, four planks a crafting
-- table, whichever the wood.
local WOODS = { "birch", "spruce", "jungle", "acacia", "dark_oak" }
local RGB = {
    birch = { { 216, 215, 210 }, { 196, 176, 118 }, { 104, 140, 64 } },
    spruce = { { 74, 54, 34 }, { 114, 84, 50 }, { 52, 88, 58 } },
    jungle = { { 88, 68, 30 }, { 160, 116, 80 }, { 48, 150, 32 } },
    acacia = { { 104, 98, 88 }, { 170, 92, 50 }, { 104, 128, 40 } },
    dark_oak = { { 60, 46, 26 }, { 66, 43, 20 }, { 46, 96, 30 } },
}

local LOG: { [string]: number } = {}
local LEAF: { [string]: number } = {}
for _, w in WOODS do
    local c = RGB[w]
    LOG[w] = core.register_node(`woods:{w}_log`, c[1][1], c[1][2], c[1][3], true, 0, "", 0, 1.6, "wood")
    core.register_node(`woods:{w}_planks`, c[2][1], c[2][2], c[2][3], true, 0, "", 0, 1.5, "wood")
    LEAF[w] = core.register_node(`woods:{w}_leaves`, c[3][1], c[3][2], c[3][3], true, 0, "", 0, 0.3, "")
    core.recipe_shapeless(`woods:{w}_planks`, 4, `woods:{w}_log`)
    core.recipe_shaped("timber:stick", 4, `woods:{w}_planks`, `woods:{w}_planks`)
    core.recipe_shaped("craft:table", 1, `woods:{w}_planks woods:{w}_planks`, `woods:{w}_planks woods:{w}_planks`)
end
core.recipe_shaped("craft:table", 1, "timber:planks timber:planks", "timber:planks timber:planks")

-- ---------------------------------------------------------------- growing

local PLAZA_LO: number, PLAZA_HI: number = math.huge, -math.huge   -- the engine's plaza, kept clear
do   -- core.plaza(): nil in a world without one (survival), with nothing to keep clear
    local lo, hi = core.plaza()
    if lo and hi then
        PLAZA_LO, PLAZA_HI = lo, hi
    end
end
local SEA = 18
local REACH = 3

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

-- Value noise, 0..1, smooth across cells of `period`, with a third of a finer
-- octave: patches with ragged round edges, where hash(x // n, z // n) made
-- squares (a desert of red and yellow squares, seen from above as a board).
local function smooth(x: number, z: number, period: number, salt: number): number
    local function one(p: number, s: number): number
        local gx, gz = x // p, z // p
        local fx, fz = (x % p) / p, (z % p) / p
        local sx, sz = fx * fx * (3 - 2 * fx), fz * fz * (3 - 2 * fz)
        local function c(a: number, b: number): number
            return (hash(a, b, s) % 4096) / 4096
        end
        local a = c(gx, gz) + (c(gx + 1, gz) - c(gx, gz)) * sx
        local b = c(gx, gz + 1) + (c(gx + 1, gz + 1) - c(gx, gz + 1)) * sx
        return a + (b - a) * sz
    end
    return 0.7 * one(period, salt) + 0.3 * one(math.max(2, period // 3), salt + 0x5EED)
end

local function owns(ctx, x: number, z: number): boolean
    return x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z
end

-- Only into air: two crowns that meet do not eat each other.
local function leaf(ctx, x: number, y: number, z: number, id: number)
    if owns(ctx, x, z) and y < ctx.height and core.get_block(x, y, z) == 0 then
        core.set_block(x, y, z, id)
    end
end

local function log(ctx, x: number, y: number, z: number, id: number)
    if owns(ctx, x, z) and y < ctx.height then
        core.set_block(x, y, z, id)
    end
end

-- A layer of leaves of radius r about (x, z), its corners off.
local function layer(ctx, x: number, y: number, z: number, r: number, id: number, h: number)
    for dz = -r, r do
        for dx = -r, r do
            local corner = r > 0 and math.abs(dx) == r and math.abs(dz) == r
            if not corner or bit32.band(bit32.rshift(h, (dx * 7 + dz * 3 + y) % 31), 1) == 1 and r < 2 then
                leaf(ctx, x + dx, y, z + dz, id)
            end
        end
    end
end

local function birch(ctx, x: number, g: number, z: number, h: number)
    local top = g + 5 + h % 3
    for y = g, top - 1 do
        log(ctx, x, y, z, LOG.birch)
    end
    layer(ctx, x, top - 2, z, 2, LEAF.birch, h)
    layer(ctx, x, top - 1, z, 2, LEAF.birch, h // 3)
    layer(ctx, x, top, z, 1, LEAF.birch, h)
    leaf(ctx, x, top + 1, z, LEAF.birch)
end

local function spruce(ctx, x: number, g: number, z: number, h: number)
    local top = g + 7 + h % 3
    for y = g, top - 1 do
        log(ctx, x, y, z, LOG.spruce)
    end
    leaf(ctx, x, top, z, LEAF.spruce)
    leaf(ctx, x, top + 1, z, LEAF.spruce)
    local rs = { 1, 1, 2, 1, 2, 2 }
    for i, r in rs do
        layer(ctx, x, top - i, z, r, LEAF.spruce, h)
    end
    -- Podzol round the foot of some.
    if h % 3 == 0 then
        local PODZOL = core.node_id("rocks:podzol")
        for dz = -2, 2 do
            for dx = -2, 2 do
                if PODZOL ~= 0 and owns(ctx, x + dx, z + dz) and dx * dx + dz * dz <= 5 then
                    local gy = core.terrain_height(x + dx, z + dz) - 1
                    local b = core.node_name(core.get_block(x + dx, gy, z + dz))
                    if b == "core:grass" or b == "core:snow" or b == "core:dirt" then
                        core.set_block(x + dx, gy, z + dz, PODZOL)
                    end
                end
            end
        end
    end
end

local function jungle(ctx, x: number, g: number, z: number, h: number)
    local top = g + 9 + h % 5
    for y = g, top - 1 do
        log(ctx, x, y, z, LOG.jungle)
    end
    layer(ctx, x, top - 2, z, 2, LEAF.jungle, h)
    layer(ctx, x, top - 1, z, 2, LEAF.jungle, h // 5)
    layer(ctx, x, top, z, 1, LEAF.jungle, h)
    -- Vines down the trunk's sides and off the crown's edge, each facing the
    -- block it hangs on (wallmounted: the face it was put on).
    local VINE = core.node_id("plants:vine")
    if VINE == 0 then
        return
    end
    -- wallmounted faces 0..5 are a front of +X, -X, +Y, -Y, +Z, -Z: a vine in
    -- the cell east of the trunk fronts east, away from what it hangs on.
    for i, d in { { 1, 0, 0 }, { -1, 0, 1 }, { 0, 1, 4 }, { 0, -1, 5 } } do
        if bit32.band(bit32.rshift(h, i * 3), 3) ~= 0 then
            local vx, vz = x + d[1], z + d[2]
            for y = top - 3, g + 2 + (h // (i * 11)) % 3, -1 do
                if owns(ctx, vx, vz) and core.get_block(vx, y, vz) == 0 then
                    core.set_block(vx, y, vz, VINE)
                    core.set_facing(vx, y, vz, d[3])
                end
            end
        end
    end
end

local function acacia(ctx, x: number, g: number, z: number, h: number)
    local lean = ({ { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } })[1 + h % 4]
    local cx, cz = x, z
    local y = g
    for _ = 1, 3 do
        log(ctx, cx, y, cz, LOG.acacia)
        y += 1
    end
    for _ = 1, 2 do
        cx += lean[1]
        cz += lean[2]
        log(ctx, cx, y, cz, LOG.acacia)
        y += 1
    end
    layer(ctx, cx, y, cz, 2, LEAF.acacia, h)
    layer(ctx, cx, y + 1, cz, 1, LEAF.acacia, h)
end

local function dark_oak(ctx, x: number, g: number, z: number, h: number)
    local top = g + 6 + h % 3
    for y = g, top - 1 do
        for _, d in { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } } do
            log(ctx, x + d[1], y, z + d[2], LOG.dark_oak)
        end
    end
    for dy = -1, 1 do
        local r = if dy < 1 then 3 else 2
        for dz = -r, r + 1 do
            for dx = -r, r + 1 do
                local far = math.abs(dx - 0.5) + math.abs(dz - 0.5) > r + 1.5
                if not far then
                    leaf(ctx, x + dx, top + dy, z + dz, LEAF.dark_oak)
                end
            end
        end
    end
end

local GROW = { birch = birch, jungle = jungle, acacia = acacia, dark_oak = dark_oak }
local REGIONS = { "", "birch", "dark_oak", "jungle", "acacia", "", "birch" }
-- How much thinner than one column in eleven each kind stands: a birch wood
-- and an acacia savanna are open, a dark oak wood and a jungle are not.
local THIN = { birch = 2, dark_oak = 1, jungle = 1, acacia = 5, spruce = 1 }

core.register_mapgen("woods:grow", function(ctx)
    for z = ctx.min_z - REACH, ctx.max_z + REACH do
        for x = ctx.min_x - REACH, ctx.max_x + REACH do
            local h = hash(x, z, ctx.seed + 0x3009)
            local in_plaza = x >= PLAZA_LO - REACH and x <= PLAZA_HI + REACH and z >= PLAZA_LO - REACH
                             and z <= PLAZA_HI + REACH
            if not in_plaza and h % 11 == 0 then
                local biome = core.biome(x, z)
                local kind = ""
                if biome == "core:tundra" and h % 33 == 0 then
                    kind = "spruce"
                elseif biome == "core:plains" then
                    -- A wood's kind by region: squares of 48 bent out of
                    -- true by up to 20 either way, so its edge wanders.
                    local wx = x + math.floor((smooth(x, z, 32, ctx.seed + 0x3E62) - 0.5) * 40)
                    local wz = z + math.floor((smooth(x, z, 32, ctx.seed + 0x3E63) - 0.5) * 40)
                    kind = REGIONS[1 + hash(wx // 48, wz // 48, ctx.seed + 0x3E61) % #REGIONS]
                end
                if kind ~= "" and (h // 11) % THIN[kind] == 0 then
                    local g = core.terrain_height(x, z)
                    local under = core.node_name(core.get_block(x, g - 1, z))
                    if g >= SEA and g + 16 < ctx.height and (under == "core:grass" or under == "core:snow"
                        or under == "core:dirt" or under == "rocks:podzol") then
                        if kind == "spruce" then
                            spruce(ctx, x, g, z, h // 11)
                        else
                            GROW[kind](ctx, x, g, z, h // 11)
                        end
                    end
                end
            end
        end
    end
end)
