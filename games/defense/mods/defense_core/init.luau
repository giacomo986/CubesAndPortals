--!strict
-- defense_core: the tower defense's world (games/defense/mods/defense_game has
-- the game). One field of grass 49 wide and 73 long walled round with stone
-- bricks: at its north end a rift in a frame of black stone, where the
-- monsters come through; at its south end the crystal on its pedestal, two
-- benches to build at, and behind it the doorway home in the wall.
--
-- The sandbox's mods are linked here (robot, tools, ores, archery, the
-- glider...), so what a player carries in is worth something and what the
-- waves give is carried out; their lakes, trees and ores are not this
-- world's ground (core.mapgen_only).
--
-- Opening 0 faces +z from the doorway: whoever arrives looks up the field at
-- the crystal and, far off, the rift.

local id = core.node_id
local DIRT, STONE = id("core:dirt"), id("core:stone")
-- Grass of its own, drawn as the sandbox's: on core:grass the sandbox's
-- animals come out, and a sheep on the field is a monster to nobody.
local GRASS = core.register_node("defense_core:turf", 96, 150, 60, true, 0, "", 0, 999, "soil")
local BRICKS, COBBLE = id("rocks:stone_bricks"), id("rocks:cobblestone")
local SMOOTH = id("rocks:smooth_stone")
local BLACK, PURPLE = id("colors:black_concrete"), id("colors:purple_concrete")

local RIFT    = core.register_node("defense_core:rift", 150, 60, 220, false, 12)
core.node_glass(RIFT, 150)
local CRYSTAL = core.register_node("defense_core:crystal", 90, 220, 255, true, 15, "", 0, 999, "glass")
local BENCH   = core.register_node("defense_core:bench", 150, 110, 70, true, 0, "", 0, 999, "wood")
local LAMP    = core.register_node("defense_core:lamp", 255, 230, 170, true, 15, "", 0, 999, "glass")

core.mapgen_core(false)
core.mapgen_only("defense_core")
core.eye_height(1.5)

-- As defense_game has to know them too: core.event "defense:layout" is not
-- worth it for six numbers, so they are written twice and said here.
local G = 20                    -- the grass's top: feet stand at G + 1
local X0, X1 = 0, 48            -- the field, walls outside it
local Z0, Z1 = 0, 72
local CX = 24
local CRYSTAL_Z = 6             -- the crystal's cell (two high, on a pedestal)
local RIFT_Z = 70               -- the rift's frame

local BOXES: { { number } } = {}
local function box(x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: number)
    table.insert(BOXES, { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1),
                           math.max(x0, x1), math.max(y0, y1), math.max(z0, z1), b })
end

-- The ground, wider than the field so the walls stand on something.
box(X0 - 8, G - 12, Z0 - 8, X1 + 8, G - 4, Z1 + 8, STONE)
box(X0 - 8, G - 3, Z0 - 8, X1 + 8, G - 1, Z1 + 8, DIRT)
box(X0 - 8, G, Z0 - 8, X1 + 8, G, Z1 + 8, GRASS)
-- The walls: five high, a lamp every eight along their top.
box(X0 - 1, G + 1, Z0 - 1, X0 - 1, G + 5, Z1 + 1, BRICKS)
box(X1 + 1, G + 1, Z0 - 1, X1 + 1, G + 5, Z1 + 1, BRICKS)
box(X0 - 1, G + 1, Z0 - 1, X1 + 1, G + 5, Z0 - 1, BRICKS)
box(X0 - 1, G + 1, Z1 + 1, X1 + 1, G + 5, Z1 + 1, BRICKS)
for x = X0, X1, 8 do
    box(x, G + 6, Z0 - 1, x, G + 6, Z0 - 1, LAMP)
    box(x, G + 6, Z1 + 1, x, G + 6, Z1 + 1, LAMP)
end
for z = Z0, Z1, 8 do
    box(X0 - 1, G + 6, z, X0 - 1, G + 6, z, LAMP)
    box(X1 + 1, G + 6, z, X1 + 1, G + 6, z, LAMP)
end
-- The doorway home in the south wall: a frame of smooth stone round a hole
-- one wide and two high.
for v = 0, 3 do
    for u = -2, 2 do
        local hole = u == 0 and v < 2
        box(CX + u, G + 1 + v, Z0 - 1, CX + u, G + 1 + v, Z0 - 1, if hole then 0 else SMOOTH)
        box(CX + u, G + 1 + v, Z0 - 2, CX + u, G + 1 + v, Z0 - 2, SMOOTH)
    end
end
-- The crystal: a pedestal of cobblestone, three wide, and the crystal two
-- high on it.
box(CX - 1, G + 1, CRYSTAL_Z - 1, CX + 1, G + 1, CRYSTAL_Z + 1, COBBLE)
box(CX, G + 2, CRYSTAL_Z, CX, G + 3, CRYSTAL_Z, CRYSTAL)
-- The benches, either side of the doorway.
box(CX - 5, G + 1, 2, CX - 5, G + 1, 2, BENCH)
box(CX + 5, G + 1, 2, CX + 5, G + 1, 2, BENCH)
-- The rift: a frame of black stone seven wide and eight high in the north
-- wall's way, violet light in it, and violet ground before it.
box(CX - 3, G + 1, RIFT_Z, CX + 3, G + 8, RIFT_Z, BLACK)
box(CX - 2, G + 1, RIFT_Z, CX + 2, G + 6, RIFT_Z, RIFT)
box(CX - 3, G, RIFT_Z - 3, CX + 3, G, RIFT_Z - 1, PURPLE)

core.register_mapgen("defense_core:field", function(ctx)
    for _, b in BOXES do
        local x0, x1 = math.max(b[1], ctx.min_x), math.min(b[4], ctx.max_x)
        local z0, z1 = math.max(b[3], ctx.min_z), math.min(b[6], ctx.max_z)
        if x0 <= x1 and z0 <= z1 then
            for x = x0, x1 do
                for z = z0, z1 do
                    for y = b[2], b[5] do core.set_block(x, y, z, b[7]) end
                end
            end
        end
    end
end)

-- Opening 0 in the doorway, facing +z. Started with a --portal-to that mouth
-- is the way home; started from the rooms file, as the game does it, the
-- doorway is said to the other rooms as "door" (core.room_portal_set), which
-- the gate on the sandbox's plaza links to (mods/gates).
core.set_portal(0, CX + 0.5, G + 2, Z0 - 0.4)
local door_said = false
core.event.on("core:tick", function(_)
    if door_said then return end
    door_said = true
    core.set_day_cycle(false)
    core.set_time(0.5)
    for k = 0, 1 do
        if core.portal_room(k) == "" then core.portal_park(k) end
    end
    if core.portal_room(0) == "" then
        door_said = core.room_portal_set("door", CX + 0.5, G + 2, Z0 - 0.5, 0, 1, 0.5, 1.0)
    end
end)
