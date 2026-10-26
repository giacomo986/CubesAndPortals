--!strict
-- Something growing on the ground.
--
-- The grass went on for ever with nothing standing on it but the stone pillars
-- the engine scatters for parallax. This mod grows trees from a mapgen stage:
-- a trunk and a crown, on the plains and more thinly on the tundra, nowhere in
-- the desert or on the peaks, and never on the plaza the portals stand on.
--
-- It is also the example of the one thing about a stage that is not obvious
-- now that the world is made a column at a time (TODO.md OPEN-35): a tree
-- standing in one column puts its crown into the next. So every column walks
-- the trees of the columns beside it too, and writes only its own cells of
-- them -- and whether a tree grows is decided from the seed, the column,
-- core.biome and core.terrain_height, never from reading blocks in a column
-- that may not have been made yet.

local TRUNK  = core.register_node("trees:trunk", 104, 78, 50, true, 0, "", 0, 1.6, "wood")
local LEAVES = core.register_node("trees:leaves", 58, 122, 46, true, 0, "", 0, 0.3, "")

-- The crown is this far from the trunk at its widest, which is how far into a
-- neighbouring column a tree reaches.
local REACH = 2

-- The engine's plaza, x and z from 20 to 100: flat by construction, and where
-- the portals, the spawn and every mod's scene stand.
local PLAZA_LO, PLAZA_HI = 20, 100

-- mods/lakes fills the ground below this with water, and a tree does not grow
-- at the bottom of a lake.
local SEA = 18

-- A 32-bit multiply that stays exact in a double, and a hash of two numbers and
-- the seed. The same two functions mods/ore and mods/cave carry, copied rather
-- than shared: a mod is a directory.
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

-- One column in this many grows a tree, by climate. Absent means never.
local ONE_IN = { ["core:plains"] = 40, ["core:tundra"] = 120 }

local function owns(ctx, x: number, z: number): boolean
    return x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z
end

core.register_mapgen("trees:grow", function(ctx)
    for z = ctx.min_z - REACH, ctx.max_z + REACH do
        for x = ctx.min_x - REACH, ctx.max_x + REACH do
            local h = hash(x, z, ctx.seed + 0x7EE5)
            local in_plaza = x >= PLAZA_LO and x <= PLAZA_HI and z >= PLAZA_LO and z <= PLAZA_HI

            -- The cheap question first: most columns are not a tree whatever
            -- their climate, and the hash says so without asking the generator.
            if not in_plaza and h % 40 == 0 then
                local one_in = ONE_IN[core.biome(x, z)]
                if one_in ~= nil and (h // 40) % (one_in // 40) == 0 then
                    local ground = core.terrain_height(x, z)
                    local tall   = 4 + bit32.band(bit32.rshift(h, 12), 0x3)
                    local top    = ground + tall

                    if ground >= SEA and top + 2 < ctx.height then
                        -- The crown: two wide layers round the top of the trunk
                        -- and a narrow one over it. A corner of a wide layer is
                        -- left off on some trees and not on others, so a grove
                        -- is not one tree printed forty times. Leaves only fill
                        -- air: two crowns that meet do not eat each other.
                        for dy = -2, 1 do
                            local r = if dy < 0 then REACH else 1
                            for dz = -r, r do
                                for dx = -r, r do
                                    local corner = math.abs(dx) == r and math.abs(dz) == r
                                    local bit    = 16 + (dy + 2) * 4 + (dx + 1) + (dz + 1) * 2
                                    local keep   = not corner or
                                                   (dy < 0 and bit32.band(bit32.rshift(h, bit % 32), 1) == 1)
                                    local cx, cy, cz = x + dx, top + dy, z + dz
                                    if keep and owns(ctx, cx, cz) and core.get_block(cx, cy, cz) == 0 then
                                        core.set_block(cx, cy, cz, LEAVES)
                                    end
                                end
                            end
                        end

                        -- And the trunk, through whatever was there: its own
                        -- column is the one place a tree may overwrite, so a
                        -- stone pillar the engine scattered where a tree grows
                        -- becomes the tree, and the crown is never left floating
                        -- over a pillar.
                        if owns(ctx, x, z) then
                            for y = ground, top - 1 do
                                core.set_block(x, y, z, TRUNK)
                            end
                        end
                    end
                end
            end
        end
    end
end)

-- A sapling: planted on grass or dirt (by hand, or by a helper robot, mods/
-- helpers), it grows into a tree of this mod's own shape some minutes on, if
-- there is room over it. Felling a tree is where they come from.
local SAPLING = core.register_node("trees:sapling", 96, 170, 60, false, 0, "", 0, 0.0, "")
core.node_model(SAPLING, core.register_model("trees:sapling", "sapling.gltf"))
core.node_box(SAPLING, 0.25, 0, 0.25, 0.75, 0.8, 0.75)

local GROWS_ON = { [core.node_id("core:grass")] = true, [core.node_id("core:dirt")] = true }

core.event.on("core:pre_place", function(ev)
    if ev.id == SAPLING and not GROWS_ON[core.get_block(ev.x, ev.y - 1, ev.z)] then
        ev.cancel = true
        core.notice(ev.player, "A sapling grows on grass or dirt")
    end
end)

-- Every 60 s one sapling in four tries: about four minutes. Only into air,
-- and not when something stands over it lower than the crown would be.
core.register_abm("trees:sapling_grows", { "trees:sapling" }, 60.0, 4, function(cell)
    local x, y, z = cell.x, cell.y, cell.z
    local tall = 4 + (x * 7 + z * 13) % 3
    for dy = 1, tall + 1 do
        if core.get_block(x, y + dy, z) ~= 0 then
            return
        end
    end
    local top = y + tall
    for dy = -2, 1 do
        local r = if dy < 0 then REACH else 1
        for dz = -r, r do
            for dx = -r, r do
                if not (math.abs(dx) == r and math.abs(dz) == r and dy >= 0)
                    and core.get_block(x + dx, top + dy, z + dz) == 0 then
                    core.set_block(x + dx, top + dy, z + dz, LEAVES)
                end
            end
        end
    end
    for ty = y, top - 1 do
        core.set_block(x, ty, z, TRUNK)
    end
end)
