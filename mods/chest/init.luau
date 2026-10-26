--!strict
-- A chest: an inventory that belongs to a CELL.
--
-- What this mod proves is one sentence. A bag belongs to a session and goes
-- with it; this belongs to the block, stays where it was put, and two people
-- can have their hands in it at the same time -- there is ONE inventory on the
-- server and both of them send moves against it.
--
-- How to see it: right click the chest block near the spawn, with anything in
-- hand or nothing. `core:used` fires before the placement and this handler
-- CLAIMS the click with `ev.cancel`, so the block in hand stays in the bag.

-- Rotatable: a chest has a FRONT, so putting one down turns the CELL towards
-- whoever put it there. One name, one id: the turn is a byte on the cell, which
-- core.get_facing reads, and the handlers below compare `ev.id` against CHEST
-- directly.
--
-- It was four ids until OPEN-27 -- the base and three quarter turns -- and the
-- price was on every handler here: the id in the world was one of the four, so
-- comparing against CHEST stopped claiming three placements in four and those
-- chests opened only with empty hands. That is a defect this mod actually had.
local CHEST = core.register_node("chest:box", 150, 110, 60, true, 4, "upright")

-- What it looks like is a Blender model, chest.gltf (made in
-- assets/blender/chest_furnace.blend): planks, corner posts, iron straps and a
-- latch, and a lid on a hinge bone whose clip is called "open". The renderer
-- holds that clip at however far open the cell is, so the lid swings the way
-- the boxes' lid always did.
--
-- The boxes are still the block: what a body walks into and what an aim finds,
-- and what a client without the model draws. The lid is NAMED, which is what
-- swings when the boxes are what is drawn.
core.node_model(CHEST, core.register_model("chest:box", "chest.gltf"))
-- In the hand and in the bag: the same chest, small (tools/make_held_models.py
-- makes it out of chest.gltf, and its icon, assets/textures/chest_box.png).
core.register_model("chest:box_held", "box_held.gltf")
core.node_box(CHEST, 0.0625, 0.000, 0.0625, 0.9375, 0.625, 0.9375)
core.node_box(CHEST, 0.0625, 0.625, 0.0625, 0.9375, 0.875, 0.9375, "lid")

-- ------------------------------------------------------------ a double chest
--
-- Two chests side by side, facing the same way, are ONE chest of twice the
-- room. Put the second one down beside the first and they join.
--
-- One container, at one of the two cells -- the PRIMARY, which is always the
-- chest that was there first, so what was in it stays in it: its container
-- grows from 27 slots to 54 and every stack keeps its slot. The other cell is
-- the SECONDARY: a block of its own, so a body is stopped by it and a pick
-- breaks it, but it draws nothing -- the primary's model is two cells wide and
-- covers it (chest_half.gltf is one triangle hidden inside that body).
--
-- Four ids, because a model is per block and where the other half is is part
-- of what is drawn. Named by the side the OTHER cell is on, seen from in front:
--   double_r  primary, the secondary on its right   (chest_double_r.gltf, x 0..2)
--   double_l  primary, the secondary on its left    (chest_double_l.gltf, x -1..1)
--   half_l    secondary, the primary on its left
--   half_r    secondary, the primary on its right
-- Every one of them breaks into an ordinary chest (core.node_drop), so nobody
-- ever holds half a double chest.
local SLOTS = 27   -- three rows of nine, so its columns line up with the bag's
local DOUBLE_SLOTS = 54

local function part(name: string, model: string, x0: number, x1: number): number
    local id = core.register_node(name, 150, 110, 60, true, 4, "upright")
    core.node_model(id, core.register_model(name, model))
    core.node_box(id, x0, 0.000, 0.0625, x1, 0.625, 0.9375)
    core.node_box(id, x0, 0.625, 0.0625, x1, 0.875, 0.9375, "lid")
    core.node_drop(id, CHEST)
    return id
end

local DOUBLE_R = part("chest:double_r", "chest_double_r.gltf", 0.0625, 1.0)
local DOUBLE_L = part("chest:double_l", "chest_double_l.gltf", 0.0, 0.9375)
local HALF_L   = part("chest:half_l", "chest_half.gltf", 0.0, 0.9375)
local HALF_R   = part("chest:half_r", "chest_half.gltf", 0.0625, 1.0)

-- A chest's right, in the world, for each of its four turns: the model's +x
-- turned the way the mesher turns it (+x goes to -z at one quarter turn).
local RIGHT = { [0] = { 1, 0 }, [1] = { 0, -1 }, [2] = { -1, 0 }, [3] = { 0, 1 } }

local function put(x: number, y: number, z: number, id: number, facing: number)
    core.set_block(x, y, z, id)
    core.set_facing(x, y, z, facing)
end

-- Where the other cell of a double chest is, by which of the four it is, or
-- nil for a chest on its own. `dir` is +1 for its right and -1 for its left.
local OTHER_SIDE = { [DOUBLE_R] = 1, [DOUBLE_L] = -1, [HALF_L] = -1, [HALF_R] = 1 }

local function other(x: number, y: number, z: number, id: number): (number?, number?)
    local dir = OTHER_SIDE[id]
    if dir == nil then
        return nil, nil
    end
    local r = RIGHT[core.get_facing(x, y, z) % 4]
    return x + dir * r[1], z + dir * r[2]
end

-- The cell the container is at, for any of the five.
local function primary(x: number, y: number, z: number, id: number): (number, number)
    if id == HALF_L or id == HALF_R then
        local ox, oz = other(x, y, z, id)
        return ox :: number, oz :: number
    end
    return x, z
end

-- A chest just put down beside another that faces the same way: the two join.
-- The one that was there is the primary, whichever side it is on.
core.event.on("core:placed", function(ev)
    if ev.id ~= CHEST then
        return
    end
    local facing = core.get_facing(ev.x, ev.y, ev.z) % 4
    local r = RIGHT[facing]
    for _, dir in { 1, -1 } do
        local qx, qz = ev.x + dir * r[1], ev.z + dir * r[2]
        if core.get_block(qx, ev.y, qz) == CHEST and core.get_facing(qx, ev.y, qz) % 4 == facing then
            -- dir 1: the old chest is on the new one's right, so the new one
            -- is on the old one's left.
            if dir == 1 then
                put(qx, ev.y, qz, DOUBLE_L, facing)
                put(ev.x, ev.y, ev.z, HALF_R, facing)
            else
                put(qx, ev.y, qz, DOUBLE_R, facing)
                put(ev.x, ev.y, ev.z, HALF_L, facing)
            end
            core.container_make(qx, ev.y, qz, DOUBLE_SLOTS)
            core.log(`chest: joined at {qx} {ev.y} {qz} and {ev.x} {ev.y} {ev.z}, {DOUBLE_SLOTS} slots`)
            return
        end
    end
end)

-- Beside the console switch, on the ground.
local CX, CY, CZ = 41, 20, 47

core.set_block(CX, CY, CZ, CHEST)

local function is_chest(id: number): boolean
    return id == CHEST or OTHER_SIDE[id] ~= nil
end

core.event.on("core:used", function(ev)
    if not is_chest(ev.id) then
        return
    end

    -- The cell, not the player: whoever used it looks into the chest that is
    -- there. Two players using it get the same one, and either half of a
    -- double chest opens the primary's.
    local px, pz = primary(ev.x, ev.y, ev.z, ev.id)
    local slots = if ev.id == CHEST then SLOTS else DOUBLE_SLOTS
    if core.container_show(px, ev.y, pz, slots, ev.player) == 0 then
        core.log("chest: could not open the one at " .. ev.x .. " " .. ev.y .. " " .. ev.z)
        return
    end

    -- Claimed. Without this the same click would open the chest AND build
    -- whatever is in hand on top of it, so a chest could only be opened with
    -- empty hands -- which is a player putting down what they are carrying to
    -- open a box.
    ev.cancel = true
end)

-- Breaking it pours what was inside on the ground and then takes the chest.
--
-- Two calls and two acts, in that order. Clearing alone swallowed everything in
-- it, which is a block that eats a stack for being broken -- and the stacks it
-- ate were the only ones in the world that could not be got back, now that a
-- chest survives a restart. What falls out is an ordinary drop: a second before
-- anybody may pick it up, and gone five minutes later.
--
-- Half of a double chest, and the other half is an ordinary chest again. The
-- SECONDARY broken: the primary keeps its first 27 slots and the rest fall
-- out. The PRIMARY broken: the container was at its cell, so all of it falls
-- and the chest left is empty.
core.event.on("core:pre_break", function(ev)
    if not is_chest(ev.id) then
        return
    end

    local px, pz = primary(ev.x, ev.y, ev.z, ev.id)
    local fell
    if ev.id == HALF_L or ev.id == HALF_R then
        fell = core.container_spill(px, ev.y, pz, SLOTS)
    else
        fell = core.container_spill(px, ev.y, pz)
        core.container_clear(px, ev.y, pz)
    end

    local ox, oz = other(ev.x, ev.y, ev.z, ev.id)
    if ox ~= nil and oz ~= nil and is_chest(core.get_block(ox, ev.y, oz)) then
        put(ox, ev.y, oz, CHEST, core.get_facing(ox, ev.y, oz))
    end
    core.log(`chest: broken at {ev.x} {ev.y} {ev.z}, {fell} stack(s) on the ground`)
end)

core.log(`chest: 'chest:box' at {CX} {CY} {CZ}, {SLOTS} slots; two side by side are one of {DOUBLE_SLOTS}`)
