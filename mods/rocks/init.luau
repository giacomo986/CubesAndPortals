--!strict
-- rocks: the rest of the classic block game's ground.
--
-- Stone breaks into COBBLESTONE, which is what the stone tools, the piston and
-- the furnace's own stone are made of now; cobblestone smelts back to stone,
-- stone to smooth stone, four stone make four stone bricks. Under the ground:
-- andesite, diorite and granite in blobs through the stone; deepslate below
-- y 5-7, tuff in it; pockets of dripstone with its points hanging from the
-- ceilings of caves and standing on their floors; sculk in the deep; rare
-- geodes -- a shell of calcite, a lining of amethyst and clusters of it
-- growing into the hollow -- and lava lying in the bottom of the caves. On the
-- surface: sandstone under sand, red sand in the deserts, coarse dirt and
-- mycelium in the plains, ice on the lakes of the tundra, and clay on lake beds.
--
-- Lava lights its surroundings (15), is swum in slowly, and burns: four points
-- every half second to a body in it.
local function node(name: string, rgb: { number }, secs: number, material: string, light: number?): number
    return core.register_node(name, rgb[1], rgb[2], rgb[3], true, light or 0, "", 0, secs, material)
end

local STONE = core.node_id("core:stone")
local DIRT = core.node_id("core:dirt")
local SAND = core.node_id("core:sand")
local GRASS = core.node_id("core:grass")

local COBBLE     = node("rocks:cobblestone", { 122, 122, 124 }, 2.0, "stone")
local MOSSY      = node("rocks:mossy_cobblestone", { 110, 124, 100 }, 2.0, "stone")
local ANDESITE   = node("rocks:andesite", { 134, 134, 136 }, 1.5, "stone")
local DIORITE    = node("rocks:diorite", { 200, 200, 200 }, 1.5, "stone")
local GRANITE    = node("rocks:granite", { 150, 104, 86 }, 1.5, "stone")
local DEEPSLATE  = node("rocks:deepslate", { 74, 74, 80 }, 3.0, "stone")
local TUFF       = node("rocks:tuff", { 108, 110, 102 }, 1.5, "stone")
local CALCITE    = node("rocks:calcite", { 222, 224, 220 }, 0.75, "stone")
local AMETHYST   = node("rocks:amethyst_block", { 136, 90, 190 }, 1.5, "stone")
local DRIPSTONE  = node("rocks:dripstone_block", { 142, 110, 92 }, 1.5, "stone")
local SCULK      = node("rocks:sculk", { 14, 36, 42 }, 0.6, "")
local CLAY       = node("rocks:clay", { 160, 166, 178 }, 0.6, "soil")
local SANDSTONE  = node("rocks:sandstone", { 218, 204, 150 }, 0.8, "stone")
local RED_SAND   = node("rocks:red_sand", { 190, 102, 36 }, 0.5, "soil")
local COARSE     = node("rocks:coarse_dirt", { 120, 86, 58 }, 0.5, "soil")
local PODZOL     = node("rocks:podzol", { 100, 70, 30 }, 0.5, "soil")
local MYCELIUM   = node("rocks:mycelium", { 120, 100, 120 }, 0.6, "soil")
local ICE        = node("rocks:ice", { 150, 190, 250 }, 0.5, "glass")
local BRICKS     = node("rocks:stone_bricks", { 122, 122, 124 }, 1.5, "stone")
local SMOOTH     = node("rocks:smooth_stone", { 160, 160, 162 }, 2.0, "stone")
local LAVA       = core.register_node("rocks:lava", 230, 110, 10, false, 15, "", 0, 0.0, "")

-- Hanging from a ceiling and standing on a floor; a cluster of amethyst.
local POINT_DOWN = core.register_node("rocks:pointed_dripstone", 150, 120, 100, false, 0, "", 0, 0.8, "stone")
local POINT_UP   = core.register_node("rocks:dripstone_up", 150, 120, 100, false, 0, "", 0, 0.8, "stone")
local CLUSTER    = core.register_node("rocks:amethyst_cluster", 190, 120, 230, false, 5, "", 0, 1.5, "stone")
core.node_model(POINT_DOWN, core.register_model("rocks:pointed_dripstone", "pointed_dripstone.gltf"))
core.node_model(POINT_UP, core.register_model("rocks:dripstone_up", "dripstone_up.gltf"))
core.node_model(CLUSTER, core.register_model("rocks:amethyst_cluster", "amethyst_cluster.gltf"))
for _, id in { POINT_DOWN, POINT_UP, CLUSTER } do
    core.node_box(id, 0.3, 0, 0.3, 0.7, 1, 0.7)
end
core.node_drop(POINT_UP, POINT_DOWN)

core.node_glass(ICE, 190)
core.node_liquid(LAVA, 235)

core.register_item("rocks:clay_ball", 64, "", 0, 1.0, "")
core.register_item("rocks:brick", 64, "", 0, 1.0, "")
core.register_item("rocks:amethyst_shard", 64, "", 0, 1.0, "")

-- What breaking gives.
core.node_drop(PODZOL, DIRT)
core.node_drop(MYCELIUM, DIRT)
core.node_drop_item(CLUSTER, "rocks:amethyst_shard")
core.node_falls(RED_SAND)

-- Broken, stone gives cobblestone and clay four clay balls. Swapped after the
-- engine has given the block's own item and not with core.node_drop, which
-- would leave stone and clay with no item for a recipe or a furnace to name.
-- core.swap_item keeps the stack's tag: stone stolen breaks to stolen cobblestone.
local function swap(ev, own: string, give: string, n: number)
    local fit = core.swap_item(ev.player, own, give, n)
    if fit >= 0 and fit < n then
        core.drop_item(ev.x, ev.y, ev.z, core.item_id(give), n - fit)
    end
end

core.event.on("core:broken", function(ev)
    if ev.id == STONE then
        swap(ev, "core:stone", "rocks:cobblestone", 1)
    elseif ev.id == CLAY then
        swap(ev, "rocks:clay", "rocks:clay_ball", 4)
    elseif ev.id == CLUSTER then
        core.give_item(ev.player, "rocks:amethyst_shard", 3)
    elseif ev.id == ICE and core.get_block(ev.x, ev.y - 1, ev.z) ~= 0 then
        -- Ice melts into the water it was, when there is something under it.
        local water = core.node_id("lakes:water")
        if water ~= 0 then
            core.set_block(ev.x, ev.y, ev.z, water)
        end
    end
    -- A point of dripstone does not hang from nothing, nor stand on nothing.
    if core.get_block(ev.x, ev.y - 1, ev.z) == POINT_DOWN then
        core.set_block(ev.x, ev.y - 1, ev.z, 0)
        core.drop_item(ev.x, ev.y - 1, ev.z, core.item_id("rocks:pointed_dripstone"), 1)
    end
    if core.get_block(ev.x, ev.y + 1, ev.z) == POINT_UP then
        core.set_block(ev.x, ev.y + 1, ev.z, 0)
        core.drop_item(ev.x, ev.y + 1, ev.z, core.item_id("rocks:pointed_dripstone"), 1)
    end
end)

core.recipe_shaped("rocks:stone_bricks", 4, "core:stone core:stone", "core:stone core:stone")
core.recipe_shaped("rocks:sandstone", 1, "core:sand core:sand", "core:sand core:sand")
core.recipe_shaped("rocks:clay", 1, "rocks:clay_ball rocks:clay_ball", "rocks:clay_ball rocks:clay_ball")
core.recipe_shaped("rocks:amethyst_block", 1, "rocks:amethyst_shard rocks:amethyst_shard",
                   "rocks:amethyst_shard rocks:amethyst_shard")
core.recipe_shaped("rocks:dripstone_block", 1, "rocks:pointed_dripstone rocks:pointed_dripstone",
                   "rocks:pointed_dripstone rocks:pointed_dripstone")
core.recipe_shaped("rocks:coarse_dirt", 4, "core:dirt gravel:gravel", "gravel:gravel core:dirt")
core.recipe_shaped("craft:brick", 1, "rocks:brick rocks:brick", "rocks:brick rocks:brick")

-- Lava burns what stands in it.
local burn = 0
core.event.on("core:tick", function(ev)
    burn += ev.dt
    if burn < 0.5 then
        return
    end
    burn = 0
    for _, p in core.players() do
        local x, y, z = core.player_pos(p)
        if x ~= nil then
            local fx, fz = math.floor(x), math.floor(z)
            if core.get_block(fx, math.floor(y), fz) == LAVA or core.get_block(fx, math.floor(y + 1), fz) == LAVA then
                core.damage(p, 4)
            end
        end
    end
end)

-- ---------------------------------------------------------------- where it lies

local function mul32(a: number, b: number): number
    local hi = math.floor(a / 65536)
    local lo = a % 65536
    return ((hi * b) % 65536 * 65536 + lo * b) % 4294967296
end

-- mods/ore's hash of a column and the seed, so every machine makes the same world.
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

-- Blobs: one column in `every` holds a middle, `lo`..`hi` up (hi counted down
-- from the column's surface when negative), of radius `r` to `r` + 1.25, and
-- only cells holding one of `into` are changed.
local BLOBS = {
    { id = ANDESITE, every = 60, r = 2.2, lo = 8, hi = -4, into = { [STONE] = true } },
    { id = DIORITE, every = 60, r = 2.2, lo = 8, hi = -4, into = { [STONE] = true } },
    { id = GRANITE, every = 60, r = 2.2, lo = 8, hi = -4, into = { [STONE] = true } },
    { id = TUFF, every = 70, r = 1.8, lo = 1, hi = 7, into = { [DEEPSLATE] = true } },
    { id = DRIPSTONE, every = 110, r = 2.4, lo = 4, hi = 14, into = { [STONE] = true } },
    { id = SCULK, every = 140, r = 2.0, lo = 1, hi = 7, into = { [DEEPSLATE] = true, [TUFF] = true } },
}
local R = 4   -- the widest reach of anything below into a neighbouring column

local function owns(ctx, x: number, z: number): boolean
    return x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z
end

-- One call per kind of cell it may take (core.replace_nodes), not one per cell.
local function blob(ctx, cx: number, cy: number, cz: number, r: number, id: number, into: { [number]: boolean })
    local n, v = math.floor(r), math.floor(r / 1.3)
    for from in into do
        core.replace_nodes(cx - n, math.max(1, cy - v), cz - n, cx + n, cy + v, cz + n, from, id, true)
    end
end

-- A geode: calcite round amethyst round a hollow, clusters on the lining.
local function geode(ctx, cx: number, cy: number, cz: number)
    for dz = -4, 4 do
        for dx = -4, 4 do
            if owns(ctx, cx + dx, cz + dz) then
                for dy = -4, 4 do
                    local d = math.sqrt(dx * dx + dy * dy + dz * dz)
                    local x, y, z = cx + dx, cy + dy, cz + dz
                    if y >= 1 and d <= 4 then
                        core.set_block(x, y, z, if d <= 2.2 then 0 elseif d <= 3.1 then AMETHYST else CALCITE)
                    end
                end
            end
        end
    end
    -- Clusters grow up off the lining's floor and hang off its roof.
    for dz = -2, 2 do
        for dx = -2, 2 do
            if owns(ctx, cx + dx, cz + dz) and hash(cx + dx, cz + dz, ctx.seed + 0x6E0D) % 3 == 0 then
                for y = cy - 3, cy + 3 do
                    if core.get_block(cx + dx, y, cz + dz) == 0 and core.get_block(cx + dx, y - 1, cz + dz) == AMETHYST then
                        core.set_block(cx + dx, y, cz + dz, CLUSTER)
                        break
                    end
                end
            end
        end
    end
end

core.register_mapgen("rocks:ground", function(ctx)
    local WATER = core.node_id("lakes:water")

    -- Deepslate under a wavy line, before anything is put into it: to y 5
    -- everywhere, and a cell or two higher by squares of four.
    core.replace_nodes(ctx.min_x, 1, ctx.min_z, ctx.max_x, 5, ctx.max_z, STONE, DEEPSLATE)
    for z = ctx.min_z, ctx.max_z, 4 do
        for x = ctx.min_x, ctx.max_x, 4 do
            local line = 5 + hash(x // 4, z // 4, ctx.seed + 0xDEE9) % 3
            if line > 5 then
                core.replace_nodes(x, 6, z, math.min(x + 3, ctx.max_x), line, math.min(z + 3, ctx.max_z),
                                   STONE, DEEPSLATE)
            end
        end
    end

    -- Blobs and geodes, whose middles may be a few columns away.
    for z = ctx.min_z - R, ctx.max_z + R do
        for x = ctx.min_x - R, ctx.max_x + R do
            for i, b in BLOBS do
                local h = hash(x, z, ctx.seed + 0x70C5 + i * 7919)
                if h % b.every == 0 then
                    local top = core.terrain_height(x, z)
                    local hi = if b.hi < 0 then top + b.hi else b.hi
                    if hi > b.lo then
                        local y = b.lo + math.floor(h / 4096) % (hi - b.lo + 1)
                        blob(ctx, x, y, z, b.r + (math.floor(h / 1048576) % 6) / 4, b.id, b.into)
                    end
                end
            end
            local g = hash(x, z, ctx.seed + 0x6E0DE)
            if g % 900 == 0 then
                local top = core.terrain_height(x, z)
                if top > 16 then
                    geode(ctx, x, 5 + math.floor(g / 4096) % (top - 14), z)
                end
            end
        end
    end

    for z = ctx.min_z, ctx.max_z do
        for x = ctx.min_x, ctx.max_x do
            local top = core.terrain_height(x, z)
            -- The bottom of the caves is lava.
            for y = 1, 3 do
                if core.get_block(x, y, z) == 0 then
                    local under = core.get_block(x, y - 1, z)
                    if under ~= 0 and under ~= LAVA then
                        core.set_block(x, y, z, LAVA)
                    end
                end
            end

            local surface = core.get_block(x, top - 1, z)
            if surface == SAND then
                -- Sandstone under the sand; red sand in some deserts.
                local red = smooth(x, z, 36, ctx.seed + 0x2ED) > 0.6 and core.biome(x, z) == "core:desert"
                local y = top - 1
                while y > 1 and core.get_block(x, y, z) == SAND do
                    if red then
                        core.set_block(x, y, z, RED_SAND)
                    end
                    y -= 1
                end
                for yy = y, math.max(1, y - 2), -1 do
                    if core.get_block(x, yy, z) == STONE or core.get_block(x, yy, z) == DIRT then
                        core.set_block(x, yy, z, SANDSTONE)
                    end
                end
            elseif surface == GRASS then
                if core.biome(x, z) == "core:plains" then
                    if smooth(x, z, 10, ctx.seed + 0xC0A) > 0.82 then
                        core.set_block(x, top - 1, z, COARSE)
                    elseif smooth(x, z, 12, ctx.seed + 0xC0B) > 0.86 then
                        core.set_block(x, top - 1, z, MYCELIUM)
                    end
                end
            end
            -- The lakes: clay on some beds, ice on the tundra's -- up in the
            -- hills too, where the water stands higher (core.water_level).
            local wl = math.max(core.water_level(x, z), 18)
            if top < wl and WATER ~= 0 then
                local bed = core.get_block(x, top - 1, z)
                if (bed == SAND or bed == DIRT or bed == GRASS) and hash(x // 4, z // 4, ctx.seed + 0xC1A) % 5 == 0 then
                    core.set_block(x, top - 1, z, CLAY)
                end
                -- A lake or a river is not a climate (core:lake, core:river): it
                -- freezes when the land round it is the tundra.
                local b = if core.get_block(x, wl - 1, z) == WATER then core.biome(x, z) else ""
                if b == "core:tundra" or ((b == "core:lake" or b == "core:river") and (function()
                    for _, d in { { 16, 0 }, { -16, 0 }, { 0, 16 }, { 0, -16 }, { 32, 0 }, { -32, 0 }, { 0, 32 }, { 0, -32 } } do
                        if core.biome(x + d[1], z + d[2]) == "core:tundra" then
                            return true
                        end
                    end
                    return false
                end)()) then
                    core.set_block(x, wl - 1, z, ICE)
                end
            end
        end
    end

    -- Points of dripstone off the ceilings and floors of the caves the pockets
    -- were cut by.
    local drip = core.find_nodes(ctx.min_x, 2, ctx.min_z, ctx.max_x, ctx.height - 2, ctx.max_z, "rocks:dripstone_block")
    for i = 1, #drip, 3 do
        local x, y, z = drip[i], drip[i + 1], drip[i + 2]
        if core.get_block(x, y - 1, z) == 0 and hash(x, z + y, ctx.seed + 0xD41) % 2 == 0 then
            core.set_block(x, y - 1, z, POINT_DOWN)
        elseif core.get_block(x, y + 1, z) == 0 and hash(x + y, z, ctx.seed + 0xD42) % 2 == 0 then
            core.set_block(x, y + 1, z, POINT_UP)
        end
    end
end)
