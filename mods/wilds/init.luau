--!strict
-- wilds: two rare lands, a reason to go far.
--
-- The plains are cut into regions of about 160 nodes, their edges bent so they
-- wander; one region in nine is a SWAMP where the land is low (up to five
-- over the sea), one in thirteen a MUSHROOM GROVE.
--
--   swamp   mud for grass, shallow pools in the hollows with lily pads, reeds
--           on the banks, willows (a trunk under a wide crown hung with
--           vines), and two or three nodes down PEAT: dug, a slow fuel
--           (mods/furnace) -- what a swamp has that nothing else does.
--   grove   mycelium for grass, small mushrooms, and giant ones: a pale stem
--           four to seven tall under a red or a brown cap, GLOWCAPS hanging
--           under it (light 10, a lamp nothing else gives). Two brown and
--           a red mushroom make a stew (mods/hunger: 6 points).
local function node(name: string, r: number, g: number, b: number, solid: boolean, light: number?, secs: number?,
                    sound: string?): number
    return core.register_node(name, r, g, b, solid, light or 0, "", 0, secs or 0.6, sound or "")
end
local MUD = node("wilds:mud", 72, 56, 40, true, 0, 0.6, "soil")
local PEAT = node("wilds:peat_block", 54, 40, 30, true, 0, 0.8, "soil")
local MYCELIUM = node("wilds:mycelium", 120, 104, 120, true, 0, 0.6, "soil")
local STEM = node("wilds:mushroom_stem", 214, 206, 190, true, 0, 0.8, "wood")
local RED_CAP = node("wilds:red_cap", 190, 40, 36, true, 0, 0.5, "wood")
local BROWN_CAP = node("wilds:brown_cap", 140, 104, 76, true, 0, 0.5, "wood")
local GLOW = node("wilds:glowcap", 150, 230, 210, false, 10, 0.2)
core.node_box(GLOW, 0.25, 0.4, 0.25, 0.75, 1, 0.75)
local REED = core.register_node("wilds:reed", 120, 150, 70, false, 0, "", 0, 0.0, "")
core.node_model(REED, core.model_id("plants:tall_grass"))
core.node_box(REED, 0.2, 0, 0.2, 0.8, 0.9, 0.8)
core.node_drop(MYCELIUM, core.node_id("core:dirt"))

core.register_item("wilds:peat", 64, "", 0, 1.0, "")
core.node_drop_item(PEAT, "wilds:peat")
core.register_item("wilds:mushroom_stew", 1, "", 0, 1.0, "")
for _, n in { "peat", "mushroom_stew" } do
    core.register_model(`wilds:{n}_held`, `{n}_held.gltf`)   -- tools/make_held_models.py
end
core.recipe_shapeless("wilds:mushroom_stew", 1, "plants:brown_mushroom", "plants:brown_mushroom", "plants:red_mushroom")
core.recipe_shapeless("plants:brown_mushroom", 2, "wilds:brown_cap")
core.recipe_shapeless("plants:red_mushroom", 2, "wilds:red_cap")

local WATER = core.node_id("lakes:water")
local LILY = core.node_id("plants:lily_pad")
local VINE = core.node_id("plants:vine")
local TRUNK = core.node_id("trees:trunk")
local LEAVES = core.node_id("woods:dark_oak_leaves")
local BROWN = core.node_id("plants:brown_mushroom")
local RED = core.node_id("plants:red_mushroom")
local SEA = 18
local REGION = 160
local REACH = 4   -- the widest thing grown off its column: a cap's or a crown's half

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
local function smooth(x: number, z: number, p: number, s: number): number
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

local PLAZA_LO: number, PLAZA_HI: number = math.huge, -math.huge
do
    local lo, hi = core.plaza()
    if lo and hi then
        PLAZA_LO, PLAZA_HI = lo - 24, hi + 24
    end
end

-- Which land a column is: "swamp", "grove" or "".
local function land(x: number, z: number, seed: number): string
    if x >= PLAZA_LO and x <= PLAZA_HI and z >= PLAZA_LO and z <= PLAZA_HI then
        return ""
    end
    if core.biome(x, z) ~= "core:plains" then
        return ""
    end
    local wx = x + math.floor((smooth(x, z, 48, seed + 0x71D1) - 0.5) * 60)
    local wz = z + math.floor((smooth(x, z, 48, seed + 0x71D2) - 0.5) * 60)
    local r = hash(wx // REGION, wz // REGION, seed + 0x71D0)
    if r % 9 == 0 then
        return if core.terrain_height(x, z) <= SEA + 5 then "swamp" else ""
    elseif r % 13 == 1 then
        return "grove"
    end
    return ""
end

core.register_mapgen("wilds:lands", function(ctx)
    local function owns(x: number, z: number): boolean
        return x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z
    end
    local function put(x: number, y: number, z: number, id: number)
        if owns(x, z) and y > 0 and y < ctx.height and id ~= 0 then
            local was = core.get_block(x, y, z)
            if was == 0 or was == LEAVES or was == core.node_id("plants:tall_grass") then
                core.set_block(x, y, z, id)
            end
        end
    end
    for z = ctx.min_z - REACH, ctx.max_z + REACH do
        for x = ctx.min_x - REACH, ctx.max_x + REACH do
            local kind = land(x, z, ctx.seed)
            if kind ~= "" then
                local g = core.terrain_height(x, z)   -- the first cell over the ground
                local h = hash(x, z, ctx.seed + 0x71D3)
                local top = core.node_name(core.get_block(x, g - 1, z))
                local ground = top == "core:grass" or top == "core:dirt"
                if kind == "swamp" and ground then
                    if owns(x, z) then
                        core.set_block(x, g - 1, z, MUD)
                        core.set_block(x, g - 2, z, MUD)
                        core.set_block(x, g - 3, z, PEAT)
                        if h % 3 == 0 then
                            core.set_block(x, g - 4, z, PEAT)
                        end
                        -- A pool in a hollow: the mud one down, water over it.
                        if g <= SEA + 2 and smooth(x, z, 9, ctx.seed + 0x71D4) > 0.55 then
                            core.set_block(x, g - 1, z, WATER)
                            if h % 7 == 0 and core.get_block(x, g, z) == 0 then
                                core.set_block(x, g, z, LILY)
                            end
                        elseif h % 5 == 0 and core.get_block(x, g, z) == 0 then
                            core.set_block(x, g, z, REED)
                        end
                    end
                    -- A willow, now and then: a trunk under a wide, low crown
                    -- with vines hanging to the water.
                    if h % 97 == 0 and g > SEA then
                        local t = 4 + h // 97 % 2
                        for y = g, g + t do
                            put(x, y, z, TRUNK)
                        end
                        for dz = -3, 3 do
                            for dx = -3, 3 do
                                local d = dx * dx + dz * dz
                                if d <= 10 then
                                    put(x + dx, g + t, z + dz, LEAVES)
                                    if d <= 4 then
                                        put(x + dx, g + t + 1, z + dz, LEAVES)
                                    end
                                    if d >= 7 and hash(x + dx, z + dz, ctx.seed + 0x71D5) % 3 == 0 then
                                        for y = g + t - 1, g + 1, -1 do
                                            put(x + dx, y, z + dz, VINE)
                                        end
                                    end
                                end
                            end
                        end
                    end
                elseif kind == "grove" and ground then
                    if owns(x, z) then
                        core.set_block(x, g - 1, z, MYCELIUM)
                        if h % 9 == 0 and core.get_block(x, g, z) == 0 then
                            core.set_block(x, g, z, if h % 2 == 0 then BROWN else RED)
                        end
                    end
                    -- A giant mushroom: a stem, a cap of radius 2 or 3, glowcaps under it.
                    if h % 61 == 0 then
                        local t = 4 + h // 61 % 4
                        local red = h // 7 % 2 == 0
                        local r = if red then 2 else 3
                        for y = g, g + t - 1 do
                            put(x, y, z, STEM)
                        end
                        for dz = -r, r do
                            for dx = -r, r do
                                if dx * dx + dz * dz <= r * r + 1 then
                                    put(x + dx, g + t, z + dz, if red then RED_CAP else BROWN_CAP)
                                    local edge = dx * dx + dz * dz >= (r - 1) * (r - 1) + 1
                                    if red and edge then
                                        put(x + dx, g + t - 1, z + dz, RED_CAP)   -- a red one is a dome
                                    elseif not red and edge and hash(x + dx, z + dz, ctx.seed + 0x71D6) % 3 == 0 then
                                        put(x + dx, g + t - 1, z + dz, GLOW)
                                    end
                                end
                            end
                        end
                        if red then
                            put(x + 1, g + t - 2, z, GLOW)
                            put(x - 1, g + t - 2, z, GLOW)
                        end
                    end
                end
            end
        end
    end
end)
