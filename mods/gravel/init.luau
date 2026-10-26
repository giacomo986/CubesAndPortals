--!strict
-- gravel: The classic block game's gravel.
--
-- Dug with a shovel like dirt, a little harder (0.55 s by hand against 0.45);
-- one in ten comes out as flint instead of itself, which is what an arrow's
-- head is (mods/archery). It lies in pockets in the stone, as the classic block game's does
-- underground, and in patches on the beds of the lakes.
--
-- And it falls, and so does sand: taken from under, or put down over nothing,
-- either falls to the first thing that holds it up, through air and water.
local GRAVEL = core.register_node("gravel:gravel", 132, 126, 122, true, 0, "", 0, 0.55, "soil")
local SAND   = core.node_id("core:sand")
local DIRT   = core.node_id("core:dirt")
local GRASS  = core.node_id("core:grass")
local STONE  = core.node_id("core:stone")
local SEA    = 18   -- mods/lakes' water line

core.register_item("gravel:flint", 64, "", 0, 1.0, "")
core.register_model("gravel:flint_held", "flint_held.gltf")

core.event.on("core:broken", function(ev)
    if ev.id == GRAVEL and math.random() < 0.1 and core.take_item(ev.player, "gravel:gravel", 1) == 1 then
        if core.give_item(ev.player, "gravel:flint", 1) == 0 then
            core.drop_item(ev.x, ev.y, ev.z, core.item_id("gravel:flint"), 1)
        end
    end
end)

-- Both fall (core.node_falls): the engine lets one go whenever nothing solid
-- is left under it, and draws it falling.
core.node_falls(GRAVEL)
core.node_falls(SAND)

-- ---------------------------------------------------------------- where it lies

-- A 32-bit multiply exact in a double, and a hash of a column and the seed:
-- mods/ore's, so the world is the same wherever it is made.
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

local EVERY = 48   -- one column in this many holds the middle of a pocket
local R     = 3    -- a pocket's radius at most

core.register_mapgen("gravel:pockets", function(ctx)
    if STONE == 0 then
        return
    end
    for z = ctx.min_z - R, ctx.max_z + R do
        for x = ctx.min_x - R, ctx.max_x + R do
            local h = hash(x, z, ctx.seed + 0x6A7E1)
            if h % EVERY == 0 then
                local top = core.terrain_height(x, z)
                if top > 8 then
                    local cy = 2 + math.floor(h / 4096) % (top - 7)
                    local r  = math.floor(1.6 + (math.floor(h / 1048576) % 16) / 12)   -- 1 .. 2
                    core.replace_nodes(x - r - 1, math.max(1, cy - r), z - r - 1, x + r + 1, cy + r, z + r + 1,
                                       STONE, GRAVEL, true)
                end
            end
        end
    end

    -- The lake beds, in patches three wide: the top of the ground under the
    -- water line, where it is sand, dirt or drowned grass.
    for z = ctx.min_z, ctx.max_z do
        for x = ctx.min_x, ctx.max_x do
            local ground = core.terrain_height(x, z)
            if ground < SEA and hash(math.floor(x / 3), math.floor(z / 3), ctx.seed + 0x6A7E2) % 3 == 0 then
                local b = core.get_block(x, ground - 1, z)
                if b == SAND or b == DIRT or b == GRASS then
                    core.set_block(x, ground - 1, z, GRAVEL)
                end
            end
        end
    end
end)
