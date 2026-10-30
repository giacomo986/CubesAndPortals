--!strict
-- Something somebody built, a long time ago.
--
-- A brick tower, seven wide and nine tall, with a doorway, a window on every
-- side, bricks fallen out of its walls and a lamp still burning on top: at most
-- one in every square of 128 blocks, on ground flat enough to have stood on,
-- never on the plaza. Something to see from a hill and walk to.
--
-- A structure is the other shape of stage from mods/trees. A tree is small and
-- there are thousands, so every column walks the trees beside it. A tower is
-- rare and the square it may stand in is exactly four columns a side, so a
-- column only ever asks about ONE square -- its own -- and the tower is placed
-- well inside it, so it never reaches a column of another square.

local MOSSY = core.register_node("ruins:mossy_brick", 104, 112, 74, true, 0, "", 0, 4.0, "stone")
local LAMP  = core.register_node("ruins:lamp", 250, 214, 130, true, 12, "", 0, 1.0, "glass")

local SQUARE = 128   -- a multiple of 32, so a column is in one square only
local SIDE   = 7
local TALL   = 9
local MARGIN = 16    -- how far inside its square a tower stands

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

type Tower = { x: number, z: number, floor: number, door: number }

-- Where this square's tower stands, or nil. A function of the seed and the
-- square, asked of the generator and never of the world.
local function tower_of(sx: number, sz: number, seed: number): Tower?
    local h = hash(sx, sz, seed + 0x7041)
    if bit32.band(h, 1) == 0 then
        return nil   -- one square in two has none
    end

    local span = SQUARE - 2 * MARGIN - SIDE
    local x = sx * SQUARE + MARGIN + bit32.band(bit32.rshift(h, 1), 0xFF) % span
    local z = sz * SQUARE + MARGIN + bit32.band(bit32.rshift(h, 9), 0xFF) % span

    -- Not on the plaza, nor close enough to crowd it.
    if x + SIDE >= PLAZA_LO - 8 and x <= PLAZA_HI + 8 and z + SIDE >= PLAZA_LO - 8
        and z <= PLAZA_HI + 8 then
        return nil
    end

    local biome = core.biome(x, z)
    if biome == "core:peaks" then
        return nil
    end

    -- Flat enough: the four corners within three blocks of each other. The floor
    -- is the LOWEST of them, so no corner stands in the air; the ones above it
    -- are dug into the hill.
    local lowest, highest = 999, -999
    for _, c in { { 0, 0 }, { SIDE - 1, 0 }, { 0, SIDE - 1 }, { SIDE - 1, SIDE - 1 } } do
        local g = core.terrain_height(x + c[1], z + c[2])
        lowest = math.min(lowest, g)
        highest = math.max(highest, g)
    end
    if highest - lowest > 3 then
        return nil
    end

    -- Not in a lake: mods/lakes fills the ground below 18 with water.
    if lowest < 18 then
        return nil
    end

    return { x = x, z = z, floor = lowest, door = bit32.band(bit32.rshift(h, 17), 3) }
end

-- What is in a tower's chest, and how much of it: coal, now and then a few
-- ingots, and a pick in one tower in three. A reason to walk to the tower and
-- go inside it, and the only iron pick in the world that was not smelted.
--
-- A chest is stocked once, and the mod's storage remembers it. The world file
-- -- its chests and the storage both -- is read after the region round the
-- spawn is generated, so a chest made there cannot yet tell a looted chest
-- from a new one: it waits for core:world_ready, and every chest after that is
-- decided at once. Nothing is put in a chest that already holds some of it,
-- which stops a column generated twice in one run from topping it up.
local SLOTS = 27

local function stock(x: number, y: number, z: number, h: number)
    local key = `chest {x} {y} {z}`
    if core.storage_get(key) ~= "" then
        return
    end
    local loot = {
        { "ore:coal", 3 + h % 5 },
        { "furnace:iron_ingot", bit32.band(bit32.rshift(h, 3), 3) },
        { "furnace:iron_pick", if bit32.band(bit32.rshift(h, 5), 3) == 0 then 1 else 0 },
    }

    local held = ""
    for _, pair in loot do
        local name, count = pair[1], pair[2] :: number
        local item = core.item_id(name :: string)
        if item ~= 0 and count > 0 and core.container_count(x, y, z, item) == 0 then
            core.container_add(x, y, z, item, count)
        end

        -- Read back, not counted as it went in: what the log says is what the
        -- chest holds, so a loot table that does not reach the chest says so.
        if item ~= 0 then
            held ..= `{if held == "" then "" else ", "}{core.container_count(x, y, z, item)} {name}`
        end
    end
    core.storage_set(key, "1")
    core.log(`ruins: chest at {x} {y} {z} holds {held}`)
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

local function stock_chest(t: Tower, ctx)
    local chest = core.node_id("chest:box")
    if chest == 0 then
        return   -- a world without mods/chest gets a tower with nothing in it
    end

    local x, z = t.x + 1, t.z + 1
    if x < ctx.min_x or x > ctx.max_x or z < ctx.min_z or z > ctx.max_z then
        return   -- the corner is in another column of this square
    end

    local y = t.floor
    core.set_block(x, y, z, chest)
    core.container_make(x, y, z, SLOTS)

    local h = hash(t.x, t.z, ctx.seed + 0x10A7)
    if ready then
        stock(x, y, z, h)
    else
        table.insert(waiting, { x, y, z, h })
    end
end

core.register_mapgen("ruins:towers", function(ctx)
    local brick = core.node_id("craft:brick")
    local glass = core.node_id("core:glass")
    if brick == 0 then
        return
    end

    local t = tower_of(math.floor(ctx.min_x / SQUARE), math.floor(ctx.min_z / SQUARE), ctx.seed)
    if t == nil then
        return
    end
    if t.x + SIDE - 1 < ctx.min_x or t.x > ctx.max_x or t.z + SIDE - 1 < ctx.min_z or t.z > ctx.max_z then
        return   -- in this square, not in this column
    end

    local mid = (SIDE - 1) // 2
    local top = t.floor + TALL

    for dz = 0, SIDE - 1 do
        for dx = 0, SIDE - 1 do
            local x, z = t.x + dx, t.z + dz
            if x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z then
                local wall = dx == 0 or dz == 0 or dx == SIDE - 1 or dz == SIDE - 1

                -- A foundation down to the ground this column actually has, and
                -- a floor.
                for y = core.terrain_height(x, z) - 1, t.floor - 1 do
                    core.set_block(x, y, z, brick)
                end

                for y = t.floor, top do
                    local id = 0
                    if wall then
                        id = brick

                        -- The doorway, two tall, in the middle of one side.
                        local on_door_side = (t.door == 0 and dz == 0) or (t.door == 1 and dz == SIDE - 1)
                            or (t.door == 2 and dx == 0) or (t.door == 3 and dx == SIDE - 1)
                        local along = if dz == 0 or dz == SIDE - 1 then dx else dz
                        if on_door_side and along == mid and y < t.floor + 2 then
                            id = 0
                        elseif along == mid and y == t.floor + 5 then
                            id = glass   -- a window on every side
                        elseif y == top and (dx + dz) % 2 == 1 then
                            id = 0       -- the battlement
                        else
                            -- Fallen out, more near the top than near the
                            -- ground; and moss on some of what is left.
                            local wear = hash(x * 131 + y, z, ctx.seed + 0x5EA)
                            local chance = 3 + (y - t.floor)
                            if wear % 40 < chance - 2 then
                                id = 0
                            elseif wear % 7 == 0 then
                                id = MOSSY
                            end
                        end
                    elseif y == top - 1 and dx == mid and dz == mid then
                        id = LAMP        -- still burning, on a last floor that is gone
                    end
                    core.set_block(x, y, z, id)
                end
            end
        end
    end

    -- And a chest in the corner, with what whoever built this left behind.
    --
    -- After the walls: the loop above writes air through the whole inside of
    -- the tower, so a chest placed during it would be taken straight back out.
    stock_chest(t, ctx)
end)
