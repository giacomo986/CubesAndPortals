--!strict
-- plants: what grows in the classic block game's overworld besides trees and wheat.
--
-- Tall grass and ferns, twelve flowers, dead bushes, brown and red mushrooms,
-- cobwebs, vines, lily pads, cactus, pumpkins and melons. Each flat one is
-- drawn as two pictures crossed in an X (tools/make_voxel_blocks.py) and
-- walked through; each is knocked off, as its item, when what it stood on is
-- dug out. Tall grass gives a seed one time in eight, a dead bush sticks, a
-- cobweb string, a melon three to seven slices.
--
-- A cobweb holds a body in it to a tenth of a walk. A cactus pricks whoever
-- touches it, one point every half second, grows on sand to three tall, and a
-- vine is climbed like a ladder.
local function plant(name: string, rgb: { number }, light: number?): number
    local id = core.register_node(`plants:{name}`, rgb[1], rgb[2], rgb[3], false, light or 0, "", 0, 0.0, "")
    core.node_model(id, core.register_model(`plants:{name}`, `{name}.gltf`))
    core.node_box(id, 0.2, 0, 0.2, 0.8, 0.8, 0.8)
    return id
end

local FLOWERS = { "dandelion", "poppy", "blue_orchid", "allium", "azure_bluet", "red_tulip", "orange_tulip",
                  "white_tulip", "pink_tulip", "oxeye_daisy", "cornflower", "lily_of_the_valley" }

local TALL = plant("tall_grass", { 90, 150, 60 })
local FERN = plant("fern", { 70, 130, 50 })
local FLOWER: { number } = {}
for _, f in FLOWERS do
    table.insert(FLOWER, plant(f, { 200, 60, 60 }))
end
local DEAD = plant("dead_bush", { 140, 100, 50 })
local BROWN = plant("brown_mushroom", { 150, 110, 80 }, 1)
local RED = plant("red_mushroom", { 200, 30, 30 })
local WEB = plant("cobweb", { 235, 235, 240 })
core.node_box(WEB, 0, 0, 0, 1, 1, 1)

-- Against the face it was put on, climbed like mods/timber's rungs.
local VINE = core.register_node("plants:vine", 60, 120, 40, false, 0, "wallmounted", 0, 0.2, "")
core.node_model(VINE, core.register_model("plants:vine", "vine.gltf"))
core.node_box(VINE, 0, 0, 0, 1, 1, 0.0625)
core.node_climbable(VINE)

-- Lies on the water and is stood on.
local LILY = core.register_node("plants:lily_pad", 50, 120, 40, true, 0, "", 0, 0.0, "")
core.node_model(LILY, core.register_model("plants:lily_pad", "lily_pad.gltf"))
core.node_box(LILY, 0, 0, 0, 1, 0.09, 1)

local CACTUS = core.register_node("plants:cactus", 70, 130, 50, true, 0, "", 0, 0.4, "")
core.node_box(CACTUS, 0.0625, 0, 0.0625, 0.9375, 1, 0.9375)
local PUMPKIN = core.register_node("plants:pumpkin", 226, 130, 30, true, 0, "upright", 0, 1.0, "wood")
local MELON = core.register_node("plants:melon", 120, 170, 40, true, 0, "", 0, 1.0, "wood")

core.register_item("plants:melon_slice", 64, "", 0, 1.0, "")
core.node_drop_item(DEAD, "timber:stick")
core.node_drop_item(WEB, "archery:string")
core.recipe_shaped("plants:melon", 1,
    "plants:melon_slice plants:melon_slice plants:melon_slice",
    "plants:melon_slice plants:melon_slice plants:melon_slice",
    "plants:melon_slice plants:melon_slice plants:melon_slice")

-- What stands on a block and goes with it.
local STANDS: { [number]: boolean } = { [TALL] = true, [FERN] = true, [DEAD] = true, [BROWN] = true,
                                        [RED] = true, [CACTUS] = true }
for _, id in FLOWER do
    STANDS[id] = true
end

core.event.on("core:broken", function(ev)
    if ev.id == TALL and math.random() < 0.125 then
        core.give_item(ev.player, "animals:seeds", 1)
    elseif ev.id == MELON and core.take_item(ev.player, "plants:melon", 1) == 1 then
        -- Its item stays for the recipe; breaking it gives three to seven slices.
        core.give_item(ev.player, "plants:melon_slice", math.random(3, 7))
    end
    local y = ev.y + 1
    while STANDS[core.get_block(ev.x, y, ev.z)] do
        local id = core.get_block(ev.x, y, ev.z)
        core.set_block(ev.x, y, ev.z, 0)
        local item = if id == DEAD then "timber:stick" else core.node_name(id)
        core.drop_item(ev.x, y, ev.z, core.item_id(item), 1)
        y += 1
    end
end)

-- A cactus grows on sand, a cell every so often, to three tall.
local SAND = core.node_id("core:sand")
core.register_abm("plants:cactus_grows", { "plants:cactus" }, 30.0, 6, function(cell)
    if core.get_block(cell.x, cell.y + 1, cell.z) ~= 0 then
        return
    end
    local tall = 1
    while tall < 3 and core.get_block(cell.x, cell.y - tall, cell.z) == CACTUS do
        tall += 1
    end
    if tall < 3 and (core.get_block(cell.x, cell.y - tall, cell.z) == SAND
        or core.node_name(core.get_block(cell.x, cell.y - tall, cell.z)) == "rocks:red_sand") then
        core.set_block(cell.x, cell.y + 1, cell.z, CACTUS)
    end
end)

-- A cobweb holds and a cactus pricks, checked twice a second.
local held: { [number]: boolean } = {}
local clock = 0
core.event.on("core:tick", function(ev)
    clock += ev.dt
    if clock < 0.5 then
        return
    end
    clock = 0
    for _, p in core.players() do
        local x, y, z = core.player_pos(p)
        if x ~= nil then
            local fx, fy, fz = math.floor(x), math.floor(y), math.floor(z)
            local in_web = core.get_block(fx, fy, fz) == WEB or core.get_block(fx, fy + 1, fz) == WEB
            if in_web ~= (held[p] == true) then
                held[p] = in_web or nil
                if in_web then
                    core.body_movement(p, 0.5, 0.5, 14, 1, -25)
                else
                    core.body_movement(p)
                end
            end
            -- A body is 0.6 wide: it touches a cactus in the next cell once
            -- it is closer than 0.3 + 1/16 to that side.
            for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
                local near = if d[1] ~= 0 then (if d[1] > 0 then fx + 1 - x else x - fx)
                                          else (if d[2] > 0 then fz + 1 - z else z - fz)
                if near < 0.4 and (core.get_block(fx + d[1], fy, fz + d[2]) == CACTUS
                    or core.get_block(fx + d[1], fy + 1, fz + d[2]) == CACTUS) then
                    core.damage(p, 1)
                    break
                end
            end
            if core.get_block(fx, fy - 1, fz) == CACTUS then
                core.damage(p, 1)
            end
        end
    end
end)

-- ---------------------------------------------------------------- where it grows

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

-- The engine's plaza, kept clear as mods/trees keeps it.
local PLAZA_LO, PLAZA_HI = 20, 100

core.register_mapgen("plants:grow", function(ctx)
    local GRASS = core.node_id("core:grass")
    local STONE = core.node_id("core:stone")
    local DEEP = core.node_id("rocks:deepslate")
    local MYC = core.node_id("rocks:mycelium")
    local RED_SAND = core.node_id("rocks:red_sand")
    local WATER = core.node_id("lakes:water")
    for z = ctx.min_z, ctx.max_z do
        for x = ctx.min_x, ctx.max_x do
            local top = core.terrain_height(x, z)
            local ground = core.get_block(x, top - 1, z)
            local h = hash(x, z, ctx.seed + 0x9A55)
            local biome = core.biome(x, z)
            local plaza = x >= PLAZA_LO and x <= PLAZA_HI and z >= PLAZA_LO and z <= PLAZA_HI
            if not plaza and core.get_block(x, top, z) == 0 then
                if ground == GRASS then
                    -- A flower patch is one kind across eight cells.
                    local patch = hash(x // 8, z // 8, ctx.seed + 0xF10)
                    if patch % 3 == 0 and h % 9 == 0 then
                        core.set_block(x, top, z, FLOWER[1 + (patch // 3) % #FLOWER])
                    elseif h % 12 == 0 then
                        core.set_block(x, top, z, TALL)
                    elseif h % 31 == 1 then
                        core.set_block(x, top, z, FERN)
                    elseif biome == "core:plains" and hash(x // 12, z // 12, ctx.seed + 0x9B7) % 9 == 0 then
                        if h % 97 == 2 then
                            core.set_block(x, top, z, PUMPKIN)
                        elseif h % 97 == 3 then
                            core.set_block(x, top, z, MELON)
                        end
                    end
                elseif ground == SAND or ground == RED_SAND then
                    if biome == "core:desert" and h % 70 == 0 then
                        core.set_block(x, top, z, DEAD)
                    elseif biome == "core:desert" and h % 110 == 1 then
                        -- Not beside another block, as the classic block game's will not grow.
                        for y = top, top + (h // 110) % 3 do
                            core.set_block(x, y, z, CACTUS)
                        end
                    end
                elseif ground == MYC and h % 5 == 0 then
                    core.set_block(x, top, z, if h % 2 == 0 then BROWN else RED)
                end
            end
            -- Lily pads on the lakes and the rivers (core:lake, core:river) of the
            -- plains: the land 16 out is plains.
            local wl = math.max(core.water_level(x, z), 18)
            if top < wl and WATER ~= 0 and h % 23 == 0 and (biome == "core:lake" or biome == "core:river")
                and core.get_block(x, wl - 1, z) == WATER and core.get_block(x, wl, z) == 0 then
                local plains = false
                for _, d in { { 16, 0 }, { -16, 0 }, { 0, 16 }, { 0, -16 } } do
                    plains = plains or core.biome(x + d[1], z + d[2]) == "core:plains"
                end
                if plains then
                    core.set_block(x, wl, z, LILY)
                end
            end
            -- Mushrooms on the floors of the dark caves.
            if h % 60 == 7 then
                for y = 4, top - 6 do
                    local under = core.get_block(x, y - 1, z)
                    if core.get_block(x, y, z) == 0 and (under == STONE or under == DEEP) then
                        core.set_block(x, y, z, if h % 2 == 0 then BROWN else RED)
                        break
                    end
                end
            end
        end
    end
end)
