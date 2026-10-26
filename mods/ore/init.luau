--!strict
-- Something to find in the stone.
--
-- Everything between the bedrock and the surface was one block -- core:stone,
-- all of it, everywhere -- so a pickaxe had nothing to look for and a tool
-- level was a gate onto an empty room. This mod buries the classic block game's ores in it,
-- from a mapgen stage: the same core.register_mapgen a mod anywhere else would
-- call, because a mechanism nothing that ships uses is a mechanism nobody has
-- run. Their pictures are tools/make_ore_textures.py.

-- Every ore of the classic block game that the stone here can hold, each a rule and not a
-- colour:
--
--   dig    the pick it wants. 0 is a bare hand, 1 the stone pick (mods/tools),
--          2 the iron pick (mods/furnace) -- The classic block game's wood/stone/iron, with
--          the hand standing in for wood.
--   drop   what breaking it gives, and how many. Coal ore gives coal, iron
--          ore raw iron to smelt, cinnabar ore four or five cinnabar; an ore
--          with no drop is dug out as itself.
--   every  one column in this many starts a vein of it.
--   low, high, depth  the cells a vein may start in: between low and high, and
--          at least depth under the column's own surface. The world is 64 tall
--          with the ground near 20, so the classic block game's -64..320 comes in at about
--          one block for eight: diamond and cinnabar in the bottom few layers,
--          coal and iron up into the mountains, emerald only in them.
--   len    how many cells the vein walks.
--
-- The iron and gold blocks keep the names they had before there were eight
-- ores, and so does diamond: a saved world is full of them, and a renamed
-- block is a magenta one. Coal could not keep its name, because "ore:coal" is
-- the coal a furnace burns and a chest in mods/ruins holds; the item took it,
-- and core.node_alias below carries the old seams over.
local ORES = {
    { name = "ore:coal_ore",     rgb = { 58, 54, 52 },    dig = 0, secs = 2.5,
      drop = "ore:coal", min = 1, max = 1,
      every = 24, low = 4, high = 44, depth = 3, len = 6 },
    { name = "ore:copper_ore",   rgb = { 190, 120, 90 },  dig = 1, secs = 4.5,
      drop = "ore:raw_copper", min = 2, max = 5,
      every = 40, low = 6, high = 20, depth = 4, len = 6 },
    { name = "ore:iron",         rgb = { 198, 152, 112 }, dig = 1, secs = 5.0,
      drop = "ore:raw_iron", min = 1, max = 1,
      every = 36, low = 2, high = 40, depth = 5, len = 5 },
    { name = "ore:lapis_ore",    rgb = { 60, 90, 170 },   dig = 1, secs = 5.0,
      drop = "ore:lapis", min = 4, max = 9,
      every = 150, low = 2, high = 14, depth = 6, len = 4 },
    { name = "ore:gold",         rgb = { 222, 190, 70 },  dig = 2, secs = 5.5,
      drop = "ore:raw_gold", min = 1, max = 1,
      every = 120, low = 2, high = 12, depth = 8, len = 4 },
    { name = "ore:cinnabar_ore", rgb = { 170, 60, 60 },   dig = 2, secs = 5.5,
      drop = "ore:cinnabar", min = 4, max = 5,
      every = 80, low = 1, high = 8, depth = 8, len = 5 },
    { name = "ore:diamond_ore",  rgb = { 120, 214, 220 }, dig = 2, secs = 6.0,
      drop = "ore:diamond", min = 1, max = 1,
      every = 220, low = 1, high = 7, depth = 10, len = 3 },
    -- One block at a time and only under the peaks, as in the classic block game's
    -- mountains; common there, because the peaks are few.
    { name = "ore:emerald_ore",  rgb = { 70, 190, 110 },  dig = 2, secs = 6.0,
      drop = "ore:emerald", min = 1, max = 1,
      every = 12, low = 14, high = 60, depth = 4, len = 1, biome = "core:peaks" },
}

for _, ore in ORES do
    ore.id = core.register_node(ore.name, ore.rgb[1], ore.rgb[2], ore.rgb[3], true, 0, "",
                                ore.dig, ore.secs, "stone")
    core.register_item(ore.drop, 64, "", 0, 1.0, "")
    core.node_drop_item(ore.id, ore.drop)
end

-- Coal seams saved when the block was "ore:coal" load as coal ore. And iron
-- and gold ore carried in a bag from before they dropped raw metal has no item
-- now: it loads as the raw metal, which smelts.
core.node_alias("ore:coal", "ore:coal_ore")
core.item_alias("ore:iron", "ore:raw_iron")
core.item_alias("ore:gold", "ore:raw_gold")

-- node_drop_item gives one; the rest of a handful of cinnabar or lapis comes
-- here, into the bag, and onto the ground where the bag is full.
core.event.on("core:broken", function(ev)
    for _, ore in ORES do
        if ev.id == ore.id and ore.max > 1 then
            local more = math.random(ore.min, ore.max) - 1
            local left = more - core.give_item(ev.player, ore.drop, more)
            if left > 0 then
                core.drop_item(ev.x, ev.y, ev.z, core.item_id(ore.drop), left)
            end
            return
        end
    end
end)

-- A vein walks one cell a step, and only ever towards +x, +z, up or down. So a
-- vein that starts this many columns before the one being generated can still
-- reach into it, and the stage has to walk those starts too: a seam across a
-- column's edge comes out whole whichever of the two columns is made first.
local REACH = 0
for _, ore in ORES do
    REACH = math.max(REACH, ore.len - 1)
end

-- A 32-bit multiply that stays exact in a double.
--
-- Here rather than `a * b` because the product of two 32-bit numbers is 64 bits
-- and a double holds 53 exactly: past that the low bits -- which are the only
-- ones a hash is for -- are rounded away. Splitting the left side into halves
-- keeps every partial product under 2^48.
local function mul32(a: number, b: number): number
    local hi = math.floor(a / 65536)
    local lo = a % 65536
    return ((hi * b) % 65536 * 65536 + lo * b) % 4294967296
end

-- A hash of one column and the seed, and of nothing else.
--
-- This function is the whole determinism rule in one place: no clock, no
-- math.random, nothing read from a global another stage could have moved. The
-- same seed has to make the same world on the server and on a client that
-- generates its own, or the two disagree about the terrain before the first
-- packet.
local function hash(x: number, z: number, salt: number): number
    local h = mul32(x + 0x9E3779B9, 374761393)
    h = bit32.bxor(h, mul32(z + 0x85EBCA6B, 668265263))
    h = bit32.bxor(h, mul32(salt % 4294967296, 2246822519))
    h = bit32.bxor(h, bit32.rshift(h, 13))
    return mul32(h, 1274126177)
end

-- A vein is a short run and not one cell, because a seam a player can follow is
-- what makes digging a direction instead of a lottery. Every cell is a
-- neighbour of the one before it -- the walk takes one step per cell along
-- whichever axis the next two bits pick -- so the run is always connected.
--
-- Only a cell that is STONE becomes ore, and that one condition is what keeps
-- this stage out of everything else's way: not the surface, not the biome's
-- filler, not the bedrock floor, and not whatever a stage registered before
-- this one has already built there. And only a cell in the column being made:
-- the rest of the walk is followed and not written.
local function vein(ctx, x: number, y: number, z: number, id: number, len: number,
                    stone: number, walk: number): number
    local placed = 0
    local cx, cy, cz = x, y, z

    for i = 0, len - 1 do
        -- Never y 0: that is the floor of the world, and a floor with a hole in
        -- it is not a hard mode.
        if cy < 1 or cy >= ctx.height then
            break
        end

        if cx >= ctx.min_x and cx <= ctx.max_x and cz >= ctx.min_z and cz <= ctx.max_z
            and core.get_block(cx, cy, cz) == stone
            -- And under its OWN column's ground: a walk that climbs, or steps
            -- sideways under a slope, would otherwise come out on a hillside.
            and cy < core.terrain_height(cx, cz) - 1 then
            core.set_block(cx, cy, cz, id)
            placed += 1
        end

        local step = bit32.band(bit32.rshift(walk, i * 2), 0x3)
        if step == 0 then
            cx += 1
        elseif step == 1 then
            cz += 1
        elseif step == 2 then
            cy += 1
        else
            cy -= 1
        end
    end

    return placed
end

core.register_mapgen("ore:veins", function(ctx)
    -- Asked for by name rather than registered here: the stone this fills is
    -- the engine's, and a mod that registered its own would be burying ore in a
    -- block nothing else has.
    local stone = core.node_id("core:stone")
    if stone == 0 then
        return
    end

    for z = ctx.min_z - REACH, ctx.max_z do
        for x = ctx.min_x - REACH, ctx.max_x do
            -- The ground as the generator made it, not as this mod guesses it,
            -- asked once per column and only if some ore starts here.
            local top: number? = nil
            for i, ore in ORES do
                -- A salt of its own per ore, so no two pick the same columns.
                local h = hash(x, z, ctx.seed + i * 0x9E3779B)
                if h % ore.every == 0 and (not ore.biome or core.biome(x, z) == ore.biome) then
                    top = top or core.terrain_height(x, z)
                    local high = math.min(ore.high, (top :: number) - ore.depth)
                    if high >= ore.low then
                        -- The low bits chose the column; the next ones the
                        -- height, and the top ones the walk.
                        local y = ore.low + math.floor(h / 4096) % (high - ore.low + 1)
                        vein(ctx, x, y, z, ore.id, ore.len, stone, bit32.rshift(h, 20))
                    end
                end
            end
        end
    end
end)

-- What each item looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
for _, n in { "coal", "raw_copper", "raw_iron", "raw_gold", "lapis", "cinnabar", "diamond", "emerald" } do
    core.register_model(`ore:{n}_held`, `{n}_held.gltf`)
end
