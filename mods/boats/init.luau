--!strict
-- Boats: a hull of five planks, carried in the bag. Right click the water (or
-- the bottom under it) with one in the hand and you sit in it on the top:
-- the keys push it at a sprint, twice a swim, drifting (core.body_boat). Right
-- click a shore to step out onto it, or run aground: either way the boat goes
-- back in the bag. Everybody sees the hull, worn on the feet (core.player_wear),
-- and the body sat in it (the drive_idle clip).

local BOAT = "boats:boat"
core.register_item(BOAT, 1)
local HULL = core.register_model(BOAT, "boat.gltf")
core.recipe_shaped(BOAT, 1,
    "timber:planks -             timber:planks",
    "timber:planks timber:planks timber:planks")

local WATER = core.node_id("lakes:water")
local SLOT = 10       -- of core.player_wear's ten; nothing else in the sandbox dresses
local AGROUND = 10    -- ticks on land before the boat is put back in the bag

local aboard: { [number]: number } = {}   -- player -> ticks on land

local function leave(p: number)
    aboard[p] = nil
    core.body_boat(p, false)
    core.player_wear(p, SLOT, 0)
    core.player_clip(p, "")
    if core.give_item(p, BOAT, 1) < 1 then
        local x, y, z = core.player_pos(p)
        core.drop_item(math.floor(x), math.floor(y), math.floor(z), core.item_id(BOAT), 1)
    end
end

-- The top of the water at a column: the first cell over y that is not water.
local function top_of(x: number, y: number, z: number): number
    while core.get_block(x, y + 1, z) == WATER do
        y += 1
    end
    return y + 1
end

core.event.on("core:used", function(ev)
    local p = ev.player
    if aboard[p] then
        -- A shore: a block with room over it to stand.
        if ev.id ~= WATER and core.get_block(ev.x, ev.y + 1, ev.z) == 0 and core.get_block(ev.x, ev.y + 2, ev.z) == 0 then
            leave(p)
            core.player_place(p, ev.x + 0.5, ev.y + 1, ev.z + 0.5)
            ev.cancel = true
        end
        return
    end
    if ev.held ~= BOAT then
        return
    end
    local wy = if ev.id == WATER then ev.y elseif core.get_block(ev.x, ev.y + 1, ev.z) == WATER then ev.y + 1 else nil
    ev.cancel = true
    if wy == nil then
        core.notice(p, "A boat goes on the water")
        return
    end
    if core.take_item(p, BOAT, 1) < 1 then
        return
    end
    aboard[p] = 0
    core.body_boat(p, true)
    core.player_wear(p, SLOT, HULL)
    core.player_clip(p, "drive_idle")
    core.player_place(p, ev.x + 0.5, top_of(ev.x, wy, ev.z) - 0.35, ev.z + 0.5)
end)

core.event.on("core:tick", function()
    if next(aboard) == nil then
        return
    end
    local here = {}
    for _, p in core.players() do
        here[p] = true
    end
    for p, n in aboard do
        if not here[p] then   -- gone: the boat with them
            aboard[p] = nil
            continue
        end
        local x, y, z = core.player_pos(p)
        local fx, fy, fz = math.floor(x), math.floor(y), math.floor(z)
        if core.get_block(fx, fy, fz) == WATER or core.get_block(fx, fy - 1, fz) == WATER then
            aboard[p] = 0
        elseif n + 1 >= AGROUND then
            leave(p)
        else
            aboard[p] = n + 1
        end
    end
end)
