--!strict
-- Relics: three rare things built long ago, at most one in a square of 384
-- blocks and in one square of two at all, by what the ground is there.
--
-- A DESERT TEMPLE, on dry desert: a stepped sandstone pyramid with a hall in
-- it. On the hall's back wall, four coloured tiles; on its floor, four glyphs
-- that turn colour with a right click. When the glyphs read as the tiles do,
-- the seal in the middle of the floor gives way onto a vault and its chest.
--
-- A WRECK, on a shallow sea floor: a broken hull of planks round a mast. Its
-- logbook (right click) says how many paces from the mast, and which ways,
-- the captain buried the chest -- two blocks down. Count, and dig.
--
-- A SUNKEN RUIN, deep under water: a ring of broken pillars round a sealed
-- chamber, three empty pedestals, and three urns. An urn broken gives a shard;
-- a shard on every pedestal breaks the seal.
--
-- Where a relic stands is a function of the seed and its square (relic_of),
-- asked of the generator, so a click finds its relic again without anything
-- kept. A seal is a block nothing digs (dig_level 99). What a chest holds is
-- decided once, as mods/ruins does it.

local SEAL = core.register_node("relics:seal", 70, 60, 50, true, 0, "", 99, 1000.0, "stone")
local TILES, GLYPHS = {}, {}
for i, rgb in { { 190, 50, 40 }, { 220, 180, 50 }, { 60, 150, 70 }, { 50, 90, 190 } } do
    TILES[i] = core.register_node(`relics:tile_{i}`, rgb[1], rgb[2], rgb[3], true, 3, "", 99, 1000.0, "stone")
    GLYPHS[i] = core.register_node(`relics:glyph_{i}`, rgb[1], rgb[2], rgb[3], true, 6, "", 99, 1000.0, "stone")
end
local LOGBOOK = core.register_node("relics:logbook", 140, 90, 50, true, 0, "", 0, 1.0, "wood")
core.node_box(LOGBOOK, 0.2, 0, 0.2, 0.8, 0.25, 0.8)
local PEDESTAL      = core.register_node("relics:pedestal", 120, 140, 130, true, 0, "", 99, 1000.0, "stone")
local PEDESTAL_FULL = core.register_node("relics:pedestal_full", 120, 220, 200, true, 10, "", 99, 1000.0, "stone")
for _, id in { PEDESTAL, PEDESTAL_FULL } do
    core.node_box(id, 0.15, 0, 0.15, 0.85, 0.8, 0.85)
end
local URN = core.register_node("relics:urn", 150, 90, 60, true, 0, "", 0, 0.4, "glass")
core.node_box(URN, 0.25, 0, 0.25, 0.75, 0.7, 0.75)
core.register_item("relics:shard", 16)
core.register_model("relics:shard_held", "shard_held.gltf")   -- tools/make_held_models.py
core.node_drop_item(URN, "relics:shard")

local SQUARE = 384   -- a multiple of 32: a column is in one square only
local MARGIN = 48    -- how far inside its square a relic stands
local PLAZA_LO: number, PLAZA_HI: number = math.huge, -math.huge
do   -- core.plaza(): nil in a world without one (survival), with nothing to keep clear
    local lo, hi = core.plaza()
    if lo and hi then
        PLAZA_LO, PLAZA_HI = lo, hi
    end
end

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

type Relic = { kind: string, x: number, z: number, y: number, h: number }

-- This square's relic, or nil.
local function relic_of(sx: number, sz: number, seed: number): Relic?
    local h = hash(sx, sz, seed + 0x2E71C)
    if bit32.band(h, 1) == 0 then
        return nil
    end
    local span = SQUARE - 2 * MARGIN
    local x = sx * SQUARE + MARGIN + bit32.rshift(h, 1) % span
    local z = sz * SQUARE + MARGIN + bit32.rshift(h, 12) % span
    if x + 30 >= PLAZA_LO - 16 and x - 30 <= PLAZA_HI + 16 and z + 30 >= PLAZA_LO - 16 and z - 30 <= PLAZA_HI + 16 then
        return nil
    end
    local g, w = core.terrain_height(x, z), core.water_level(x, z)
    if w - g >= 7 then
        return { kind = if bit32.band(h, 2) == 0 then "ruin" else "wreck", x = x, z = z, y = g, h = h }
    elseif w - g >= 3 then
        return { kind = "wreck", x = x, z = z, y = g, h = h }
    elseif w <= g and core.biome(x, z) == "core:desert" then
        local lo, hi = 999, -999
        for _, c in { { 0, 0 }, { 12, 0 }, { 0, 12 }, { 12, 12 } } do
            local t = core.terrain_height(x + c[1], z + c[2])
            lo, hi = math.min(lo, t), math.max(hi, t)
        end
        if hi - lo <= 4 then
            return { kind = "temple", x = x, z = z, y = lo, h = h }
        end
    end
    return nil
end

-- ------------------------------------------------------------ the chests

local SLOTS = 27
local LOOT = { { "ore:emerald", 4, 8 }, { "furnace:gold_ingot", 2, 6 }, { "furnace:iron_ingot", 3, 8 },
               { "ore:diamond", 0, 2 }, { "ore:lapis", 4, 12 } }

local function stock(x: number, y: number, z: number, h: number)
    local key = `chest {x} {y} {z}`
    if core.storage_get(key) ~= "" then
        return
    end
    for i, l in LOOT do
        local item = core.item_id(l[1])
        local n = l[2] + bit32.rshift(h, i * 3) % (l[3] - l[2] + 1)
        if item ~= 0 and n > 0 and core.container_count(x, y, z, item) == 0 then
            core.container_add(x, y, z, item, n)
        end
    end
    core.storage_set(key, "1")
end

local ready = false
local waiting: { { number } } = {}
core.event.on("core:world_ready", function()
    ready = true
    for _, c in waiting do
        stock(c[1], c[2], c[3], c[4])
    end
    waiting = {}
end)

local function chest_at(x: number, y: number, z: number, h: number)
    local chest = core.node_id("chest:box")
    if chest == 0 then
        return
    end
    core.set_block(x, y, z, chest)
    core.container_make(x, y, z, SLOTS)
    if ready then
        stock(x, y, z, h)
    else
        table.insert(waiting, { x, y, z, h })
    end
end

-- ------------------------------------------------------------ the shapes

-- Where the parts of each relic are, from its origin: the generator builds
-- them there and a click looks for them there.
local function glyph_cells(r: Relic): { { number } }   -- x, y, z, the colour wanted
    local out = {}
    for i, dx in { 4, 5, 7, 8 } do
        table.insert(out, { r.x + dx, r.y, r.z + 7, 1 + bit32.rshift(r.h, 14 + i * 2) % 4 })
    end
    return out
end
local function pedestal_cells(r: Relic): { { number } }
    return { { r.x + 3, r.y, r.z }, { r.x - 3, r.y, r.z }, { r.x, r.y, r.z + 3 } }
end
-- The buried chest: paces east and south of the mast, each 4 to 12 either way.
local function treasure_of(r: Relic): (number, number)
    local ex = (4 + bit32.rshift(r.h, 16) % 9) * (if bit32.band(r.h, 4) == 0 then 1 else -1)
    local sz = (4 + bit32.rshift(r.h, 20) % 9) * (if bit32.band(r.h, 8) == 0 then 1 else -1)
    return ex, sz
end

-- The world's seed, which only a stage is told: kept, for the clicks of a
-- run that makes no new column.
local SEED = tonumber(core.storage_get("seed")) or 0

core.register_mapgen("relics:build", function(ctx)
    if SEED ~= ctx.seed then
        SEED = ctx.seed
        core.storage_set("seed", tostring(SEED))
    end
    local sandstone, planks = core.node_id("rocks:sandstone"), core.node_id("timber:planks")
    local bricks, smooth = core.node_id("rocks:stone_bricks"), core.node_id("rocks:smooth_stone")
    local log = core.node_id("trees:trunk")
    if sandstone == 0 or planks == 0 then
        return
    end
    local r = relic_of(math.floor(ctx.min_x / SQUARE), math.floor(ctx.min_z / SQUARE), ctx.seed)
    if r == nil then
        return
    end
    local function inside(x: number, z: number): boolean
        return x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z
    end
    local function put(x: number, y: number, z: number, id: number)
        if inside(x, z) then
            core.set_block(x, y, z, id)
        end
    end
    local F = r.y

    if r.kind == "temple" then
        for dz = 0, 12 do
            for dx = 0, 12 do
                local x, z = r.x + dx, r.z + dz
                if inside(x, z) then
                    local step = math.min(dx, dz, 12 - dx, 12 - dz)
                    for y = core.terrain_height(x, z) - 1, F + step do
                        core.set_block(x, y, z, sandstone)
                    end
                    local hall = dx >= 3 and dx <= 9 and dz >= 3 and dz <= 9
                    local door = dx == 6 and dz <= 3
                    for y = F, F + 2 do
                        if hall or (door and y <= F + 1) then
                            core.set_block(x, y, z, 0)
                        end
                    end
                end
            end
        end
        for i, dx in { 4, 5, 7, 8 } do
            put(r.x + dx, F + 1, r.z + 10, TILES[glyph_cells(r)[i][4]])
        end
        local torch = core.node_id("torches:torch")   -- still burning by the back wall
        if torch ~= 0 then
            put(r.x + 3, F, r.z + 9, torch)
            put(r.x + 9, F, r.z + 9, torch)
        end
        for i, g in glyph_cells(r) do
            -- Each starts one turn or more away from what it should read.
            put(g[1], g[2], g[3], GLYPHS[(g[4] + 1 + bit32.rshift(r.h, 24 + i) % 3 - 1) % 4 + 1])
        end
        -- The vault under the floor's middle: a shell of seal, a chest inside.
        for dz = 3, 7 do
            for dx = 4, 8 do
                for y = F - 4, F - 1 do
                    local shell = dx == 4 or dx == 8 or dz == 3 or dz == 7 or y == F - 4 or y == F - 1
                    put(r.x + dx, y, r.z + dz, if shell then SEAL else 0)
                end
            end
        end
        if inside(r.x + 6, r.z + 6) then
            chest_at(r.x + 6, F - 3, r.z + 6, hash(r.x, r.z, ctx.seed + 0xC4E5))
        end

    elseif r.kind == "wreck" then
        -- A hull along x round the mast at (x + 5, z + 2): planks, holed.
        for dz = 0, 4 do
            for dx = 0, 10 do
                local x, z = r.x + dx, r.z + dz
                if inside(x, z) then
                    local g = core.terrain_height(x, z)
                    local side = dz == 0 or dz == 4 or dx == 0 or dx == 10
                    put(x, g, z, planks)
                    if side then
                        for y = g + 1, g + 3 do
                            if hash(x * 31 + y, z, ctx.seed + 0x3EC) % 4 ~= 0 then
                                core.set_block(x, y, z, planks)
                            end
                        end
                    end
                end
            end
        end
        local mx, mz = r.x + 5, r.z + 2
        if inside(mx, mz) and log ~= 0 then
            local g = core.terrain_height(mx, mz)
            for y = g + 1, g + 7 - bit32.rshift(r.h, 5) % 3 do
                core.set_block(mx, y, mz, log)
            end
        end
        put(r.x + 8, core.terrain_height(r.x + 8, r.z + 2) + 1, r.z + 2, LOGBOOK)
        local ex, sz = treasure_of(r)
        local tx, tz = mx + ex, mz + sz
        if inside(tx, tz) then
            chest_at(tx, core.terrain_height(tx, tz) - 3, tz, hash(tx, tz, ctx.seed + 0xC4E5))
        end

    else   -- a sunken ruin round (x, z)
        for dz = -6, 6 do
            for dx = -6, 6 do
                if dx * dx + dz * dz <= 40 then
                    put(r.x + dx, F - 1, r.z + dz, smooth)
                end
            end
        end
        for i = 0, 7 do
            local a = i / 8 * math.pi * 2
            local x, z = r.x + math.floor(math.cos(a) * 5 + 0.5), r.z + math.floor(math.sin(a) * 5 + 0.5)
            for y = F, F + 1 + hash(x, z, ctx.seed + 0x9111) % 5 do
                put(x, y, z, bricks)
            end
        end
        for dz = -1, 1 do
            for dx = -1, 1 do
                for y = F, F + 2 do
                    put(r.x + dx, y, r.z + dz, SEAL)
                end
            end
        end
        if inside(r.x, r.z) then
            chest_at(r.x, F + 1, r.z, hash(r.x, r.z, ctx.seed + 0xC4E5))
        end
        for _, c in pedestal_cells(r) do
            put(c[1], c[2], c[3], PEDESTAL)
        end
        for i = 1, 3 do
            local a = (i + bit32.rshift(r.h, 7) % 8 / 8) / 3 * math.pi * 2
            local d = 4 + i % 2
            put(r.x + math.floor(math.cos(a) * d + 0.5), F, r.z + math.floor(math.sin(a) * d + 0.5), URN)
        end
    end
end)

-- ------------------------------------------------------------ the puzzles


local function relic_at(x: number, z: number): Relic?
    return relic_of(math.floor(x / SQUARE), math.floor(z / SQUARE), SEED)
end

local function opened(x: number, y: number, z: number, player: number, what: string)
    core.play_sound("relics:grind", x, y, z)
    core.notice(player, what)
end

core.event.on("core:used", function(ev)
    local p = ev.player
    if ev.id == LOGBOOK then
        ev.cancel = true
        local r = relic_at(ev.x, ev.z)
        if r == nil or r.kind ~= "wreck" then
            core.notice(p, "The ink has run: nothing can be read")
            return
        end
        local ex, sz = treasure_of(r)
        core.notice(p, `Captain's log: {math.abs(ex)} paces {if ex > 0 then "east" else "west"} and `
            .. `{math.abs(sz)} paces {if sz > 0 then "south" else "north"} of the mast, two deep`)
        return
    end

    local turn = table.find(GLYPHS, ev.id)
    if turn then
        ev.cancel = true
        core.set_block(ev.x, ev.y, ev.z, GLYPHS[turn % 4 + 1])
        core.play_sound("relics:grind", ev.x, ev.y, ev.z)
        local r = relic_at(ev.x, ev.z)
        if r == nil or r.kind ~= "temple" then
            return
        end
        for _, g in glyph_cells(r) do
            if core.get_block(g[1], g[2], g[3]) ~= GLYPHS[g[4]] then
                return
            end
        end
        if core.get_block(r.x + 6, r.y - 1, r.z + 5) == SEAL then
            core.set_block(r.x + 6, r.y - 1, r.z + 5, 0)
            opened(r.x + 6, r.y, r.z + 5, p, "The seal in the floor gives way")
        end
        return
    end

    if ev.id == PEDESTAL and ev.held == "relics:shard" then
        ev.cancel = true
        if core.take_item(p, "relics:shard", 1) < 1 then
            return
        end
        core.set_block(ev.x, ev.y, ev.z, PEDESTAL_FULL)
        core.play_sound("relics:grind", ev.x, ev.y, ev.z)
        local r = relic_at(ev.x, ev.z)
        if r == nil or r.kind ~= "ruin" then
            return
        end
        for _, c in pedestal_cells(r) do
            if core.get_block(c[1], c[2], c[3]) ~= PEDESTAL_FULL then
                core.notice(p, "The shard settles; the seal holds")
                return
            end
        end
        for dz = -1, 1 do
            for dx = -1, 1 do
                for y = r.y + 1, r.y + 2 do
                    if (dx ~= 0 or dz ~= 0 or y == r.y + 2) and core.get_block(r.x + dx, y, r.z + dz) == SEAL then
                        core.set_block(r.x + dx, y, r.z + dz, 0)
                    end
                end
            end
        end
        opened(r.x, r.y + 1, r.z, p, "The seal breaks open")
    elseif ev.id == PEDESTAL then
        ev.cancel = true
        core.notice(p, "An empty pedestal, shaped for a shard")
    end
end)
