--!strict
-- cane: sugar cane, paper and books, as in the classic block game.
--
-- Cane grows on the shores: a stage after mods/lakes puts one to three of it
-- on a grass or sand column at the water's level beside a lake, one shore
-- column in four. It grows a cell taller now and then, three tall at most,
-- on anything; broken, it gives itself and takes whatever cane is on top with
-- it. Three cane in a row are three paper; three paper and a leather (a cow's,
-- mods/animals) are a book. Unlike the classic block game it may be planted anywhere, water
-- or none. Its model is tools/make_block_textures.py.
local CANE = core.register_node("cane:sugar_cane", 120, 200, 90, false, 0, "", 0, 0.0, "")
core.node_model(CANE, core.register_model("cane:sugar_cane", "sugar_cane.gltf"))
core.node_box(CANE, 0.2, 0, 0.2, 0.8, 1, 0.8)

core.register_item("cane:paper", 64, "", 0, 1.0, "")
core.register_item("cane:book", 64, "", 0, 1.0, "")
core.recipe_shaped("cane:paper", 3, "cane:sugar_cane cane:sugar_cane cane:sugar_cane")
core.recipe_shapeless("cane:book", 1, "cane:paper cane:paper cane:paper animals:leather")

local SEA = 18   -- mods/lakes' water level
local GRASS, SAND = core.node_id("core:grass"), core.node_id("core:sand")

-- The two functions mods/trees carries, for the same reason.
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

core.register_mapgen("cane:shores", function(ctx)
    for z = ctx.min_z, ctx.max_z do
        for x = ctx.min_x, ctx.max_x do
            -- A shore: ground at the water's level, a lake beside it. Asked of
            -- the generator's ground, so the neighbour need not be made yet.
            if core.terrain_height(x, z) == SEA then
                local wet = false
                for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
                    if core.terrain_height(x + d[1], z + d[2]) < SEA then
                        wet = true
                    end
                end
                local h = hash(x, z, ctx.seed + 0xCA7E)
                local under = core.get_block(x, SEA - 1, z)
                if wet and h % 4 == 0 and (under == GRASS or under == SAND) then
                    for y = SEA, SEA + (h // 4) % 3 do
                        if core.get_block(x, y, z) ~= 0 then
                            break
                        end
                        core.set_block(x, y, z, CANE)
                    end
                end
            end
        end
    end
end)

-- A cell taller, one cane in eight a minute, three tall at most.
core.register_abm("cane:grow", { "cane:sugar_cane" }, 60.0, 8, function(cell)
    if core.get_block(cell.x, cell.y + 1, cell.z) ~= 0 then
        return
    end
    local tall = 1
    while tall < 3 and core.get_block(cell.x, cell.y - tall, cell.z) == CANE do
        tall += 1
    end
    if tall < 3 then
        core.set_block(cell.x, cell.y + 1, cell.z, CANE)
    end
end)

-- What stood on a cane that was broken, or on the block under it, goes too.
core.event.on("core:broken", function(ev)
    local y = ev.y + 1
    while core.get_block(ev.x, y, ev.z) == CANE do
        core.set_block(ev.x, y, ev.z, 0)
        core.drop_item(ev.x, y, ev.z, core.item_id("cane:sugar_cane"), 1)
        y += 1
    end
end)

-- What each item looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
for _, n in { "book", "paper" } do
    core.register_model(`cane:{n}_held`, `{n}_held.gltf`)
end
