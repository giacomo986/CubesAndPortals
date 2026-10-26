--!strict
-- A log and a panel of rungs, which are here for their TURNS.
--
-- `facedir` is one of twenty-four orientations of a cube: the log can stand up
-- or lie down along either horizontal axis, which four quarter turns about the
-- vertical could not say. `wallmounted` is a face -- where the block's front
-- points -- so the rungs go on whichever surface was clicked.
--
-- The turn is a byte on the CELL and not a block of its own, so both of these
-- are one id each however they are lying. The placeholder art is marked on the
-- faces that matter (the log's ends carry rings, the rungs carry a bullseye),
-- because a placeholder is otherwise the same on all six faces and an
-- orientation nobody can see is an orientation nobody will check.

local LOG   = core.register_node("timber:log",   126, 92, 54,  true, 0, "facedir",     0, 1.6, "wood")
local RUNGS = core.register_node("timber:rungs", 158, 122, 74, true, 0, "wallmounted", 0, 0.9, "wood")
local MOSSY = core.register_node("timber:mossy_log", 96, 112, 58, true, 0, "facedir", 0, 1.6, "wood")

-- The rungs are a LADDER: a thin panel against whatever they were put on --
-- unturned the front is +z, so the back, the wall, is -z, and the turn takes
-- it round with the face that was clicked -- and a body in their cell climbs
-- (core.node_climbable): jump or walking into them goes up, crouch holds on,
-- and otherwise it slides slowly down. Creatures climb them too, and find_path goes up them.
core.node_box(RUNGS, 0.0, 0.0, 0.0, 1.0, 1.0, 0.125)
core.node_climbable(RUNGS)
-- Drawn as a ladder, two rails and four rungs inside that box, and held as
-- one (tools/make_ladder_model.py, in Blender, which draws its picture too).
core.node_model(RUNGS, core.register_model("timber:rungs", "rungs.gltf"))
core.register_model("timber:rungs_held", "rungs_held.gltf")

-- A ladder hangs on a WALL, as in the classic block game: put on a floor or a ceiling it
-- goes onto the wall the placer faces (or another one beside it), and with
-- none it is not put down; it falls off when its wall is broken. The byte is
-- the face the front points at: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z, and the
-- wall is the cell behind, against the normal.
local NORMAL = { [0] = { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }
local SIDES = { 0, 1, 4, 5 }

local function holds(x: number, y: number, z: number): boolean
    local b = core.get_block(x, y, z)
    return b ~= RUNGS and core.node_solid(b) == 1
end

local function hung(x: number, y: number, z: number, f: number): boolean
    local n = NORMAL[f]
    return f ~= 2 and f ~= 3 and holds(x - n[1], y - n[2], z - n[3])
end

-- A ladder's column: the rungs above and below it, unbroken.
local function column(x: number, y: number, z: number): (number, number)
    local lo, hi = y, y
    while lo > y - 32 and core.get_block(x, lo - 1, z) == RUNGS do lo -= 1 end
    while hi < y + 32 and core.get_block(x, hi + 1, z) == RUNGS do hi += 1 end
    return lo, hi
end

-- The side most of a column has a wall on, so a shaft's rungs are on one wall
-- all the way -- of the sides `at` has one on, when it is given. Nil for none.
local function most_walled(x: number, lo: number, hi: number, z: number, at: number?): number?
    local best, most = nil, 0
    for _, f in SIDES do
        if at == nil or hung(x, at, z, f) then
            local n = 0
            for y = lo, hi do
                if hung(x, y, z, f) then n += 1 end
            end
            if n > most then best, most = f, n end
        end
    end
    return best
end

core.event.on("core:placed", function(ev)
    if ev.id ~= RUNGS then
        return
    end
    local f = core.get_facing(ev.x, ev.y, ev.z)
    if f ~= 2 and f ~= 3 then
        return
    end
    -- On a floor or a ceiling: onto the wall ahead of the placer.
    local prefer: number? = nil
    local lx, _, lz = core.player_look(ev.player)
    if lx ~= nil and lz ~= nil then
        prefer = if math.abs(lx) >= math.abs(lz) then (if lx > 0 then 1 else 0) else (if lz > 0 then 5 else 4)
    end
    local lo, hi = column(ev.x, ev.y, ev.z)
    local to = if prefer ~= nil and hung(ev.x, ev.y, ev.z, prefer) then prefer
               else most_walled(ev.x, lo, hi, ev.z, ev.y)
    if to ~= nil then
        core.set_facing(ev.x, ev.y, ev.z, to)
        return
    end
    core.set_block(ev.x, ev.y, ev.z, 0)
    if core.give_item(ev.player, "timber:rungs", 1) < 1 then
        core.drop_item(ev.x, ev.y, ev.z, core.item_id("timber:rungs"), 1)
    end
end)

-- And what a log turns into, which is where every tool in this world starts.
--
-- A plank is a block; a stick is not. A stick is the second item in the game
-- that is not the item form of a block (after mods/tools' pick), and it exists
-- for one reason: a tool is a head and a HANDLE, and without a handle every
-- recipe here had to use core:frame -- eight stone for the shaft of a shovel.
core.register_node("timber:planks", 176, 136, 84, true, 0, "", 0, 1.0, "wood")
core.register_item("timber:stick", 64, "", 0, 1.0, "")

-- A log makes four planks, two planks make four sticks: the chain the classic block game
-- taught everybody, one shapeless and one shaped. The shaped one is two cells
-- of the same thing one above the other, which is not the same recipe as two
-- side by side -- a shape is a picture and its mirror is a different shape.
core.recipe_shapeless("timber:planks", 4, "timber:log")
core.recipe_shaped("timber:stick", 4, "timber:planks", "timber:planks")

-- Seven sticks in an H make three ladders.
core.recipe_shaped("timber:rungs", 3,
    "timber:stick -            timber:stick",
    "timber:stick timber:stick timber:stick",
    "timber:stick -            timber:stick")

-- Where the demonstration stands. Beside the palette wall, on the plaza the
-- portal scene keeps flat, so it is in front of somebody who starts the game
-- and does not have to go looking.
local BASE_X = 58
local BASE_Y = 20
local BASE_Z = 40

-- One log per axis, turned by core.set_facing rather than by being placed:
-- a placement gives a block one of the four upright turns, because that is all
-- a player can aim at. The other twenty exist for a mod, and this is a mod.
--
-- 0 is standing up, 4 and 8 lie along z, 12 and 16 lie along x -- the
-- orientation is axis * 4 + turn, and the axis says where the block's own +Y
-- ends up.
local LYING = { 0, 4, 12, 8, 16 }

local function build()
    for i, orientation in LYING do
        local x = BASE_X + (i - 1)
        core.set_block(x, BASE_Y, BASE_Z, LOG)
        core.set_facing(x, BASE_Y, BASE_Z, orientation)
    end

    -- And the rungs on all six faces, each pointing the way a click on that
    -- surface would have pointed it, on a log behind it so it does not fall.
    -- The byte IS a face index: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z.
    for face = 0, 5 do
        local z, n = BASE_Z + face * 2, NORMAL[face]
        core.set_block(BASE_X - n[1], BASE_Y + 2 - n[2], z - n[3], LOG)
        core.set_block(BASE_X, BASE_Y + 2, z, RUNGS)
        core.set_facing(BASE_X, BASE_Y + 2, z, face)
    end
end

build()

-- And what happens to those cells on their own.
--
-- An ABM runs every `interval` seconds on each loaded cell holding one of its
-- blocks, with a one-in-`chance` chance each time. The action is handed ONE
-- table -- `cell.x`, `cell.y`, `cell.z`, `cell.id` -- which is the shape every
-- handler in this engine takes, from an event to a mapgen stage.
--
-- A log grows moss; the cell keeps its orientation, because the moss is on the
-- wood and not a different way of lying down.
--
-- It runs on this mod's OWN blocks and on nothing else, which is the whole of
-- its blast radius: five cells in a world, all of them placed above.
core.register_abm("timber:moss", {"timber:log"}, 2.0, 4, function(cell)
    local facing = core.get_facing(cell.x, cell.y, cell.z)
    core.set_block(cell.x, cell.y, cell.z, MOSSY)
    core.set_facing(cell.x, cell.y, cell.z, facing)
end)

-- And an LBM runs once per matching cell of a chunk WHEN THE CHUNK LOADS, and
-- never again. Scraping the moss off is exactly the shape of thing it is for: a
-- world opened again starts clean, and the moss grows back while somebody is
-- there to watch it.
core.register_lbm("timber:clean", {"timber:mossy_log"}, function(cell)
    local facing = core.get_facing(cell.x, cell.y, cell.z)
    core.set_block(cell.x, cell.y, cell.z, LOG)
    core.set_facing(cell.x, cell.y, cell.z, facing)
end)

-- Rungs that villages and houses put down before 2026-10-04 were turned as
-- if they were a chest (an upright turn), which for half of them is a face
-- up or down: a floor at every step of a mine's shaft. A column with one of
-- those, or one with no wall behind it, goes onto its wall when it loads.
-- The rungs of this mod's own demonstration are in the air and stay.
-- A ladder whose wall is gone falls, however it went: broken by a player,
-- blown up, or taken by a mod.
core.register_abm("timber:rungs_fall", { "timber:rungs" }, 1.0, 1, function(cell)
    local n = NORMAL[core.get_facing(cell.x, cell.y, cell.z)]
    if n ~= nil and not holds(cell.x - n[1], cell.y - n[2], cell.z - n[3]) then
        core.set_block(cell.x, cell.y, cell.z, 0)
        core.drop_item(cell.x, cell.y, cell.z, core.item_id("timber:rungs"), 1)
    end
end)

core.register_lbm("timber:rungs_on_walls", { "timber:rungs" }, function(cell)
    local x, z = cell.x, cell.z
    local lo, hi = column(x, cell.y, z)
    local bad = false
    for y = lo, hi do
        if not hung(x, y, z, core.get_facing(x, y, z)) then bad = true end
    end
    local to = if bad then most_walled(x, lo, hi, z) else nil
    if to ~= nil then
        for y = lo, hi do
            core.set_facing(x, y, z, to)
        end
    end
end)

-- Rebuilt on a reload, because the cells are the whole of what this mod is: an
-- edit to LYING above should show up without the world being regenerated.
function on_load(state: string?)
    build()
end

-- What it looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
core.register_model("timber:stick_held", "stick_held.gltf")
