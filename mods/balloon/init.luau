--!strict
-- balloon: a hot-air balloon, carried folded in the bag.
--
-- Right click the ground with one in the hand and you stand in its basket
-- under the envelope (core.body_balloon): jump held lifts you at two and a half
-- metres a second, crouch lets you down, and otherwise it sinks slowly; the
-- keys push it along at a walk and the wind takes it east. Any height: over
-- the clouds, the world below. Landed, right click a block to climb out and
-- fold it back into the bag. Everybody sees it (core.player_wear,
-- tools/make_flight_models.py). A descent in it is not a fall.

local BALLOON = "balloon:balloon"
core.register_item(BALLOON, 1)
core.register_model("balloon:balloon_held", "balloon_held.gltf")   -- tools/make_held_models.py
local ENVELOPE = core.register_model("balloon:envelope", "balloon.gltf")
core.recipe_shaped(BALLOON, 1,
    "animals:wool  animals:wool  animals:wool",
    "animals:wool  ore:coal      animals:wool",
    "timber:planks timber:planks timber:planks")

local SLOT = 7   -- of core.player_wear's ten: mods/glider has 8, carts and the pony 9, boats 10

local aboard: { [number]: boolean } = {}

local function grounded(p: number): boolean
    local x, y, z = core.player_pos(p)
    if x == nil or y == nil or z == nil then
        return false
    end
    local id = core.get_block(math.floor(x), math.floor(y - 0.1), math.floor(z))
    return id ~= 0 and core.node_solid(id) == 1
end

local function leave(p: number)
    aboard[p] = nil
    core.body_balloon(p, false)
    core.player_wear(p, SLOT, 0)
    if core.give_item(p, BALLOON, 1) < 1 then
        local x, y, z = core.player_pos(p)
        if x and y and z then
            core.drop_item(math.floor(x), math.floor(y), math.floor(z), core.item_id(BALLOON), 1)
        end
    end
end

core.event.on("core:used", function(ev)
    local p = ev.player
    if aboard[p] then
        ev.cancel = true
        if grounded(p) then
            leave(p)
        else
            core.notice(p, "Come down first: crouch to let the balloon down")
        end
        return
    end
    if ev.held ~= BALLOON then
        return
    end
    ev.cancel = true
    if core.get_block(ev.x, ev.y + 1, ev.z) ~= 0 or core.get_block(ev.x, ev.y + 2, ev.z) ~= 0 then
        core.notice(p, "A balloon needs open ground")
        return
    end
    if core.take_item(p, BALLOON, 1) < 1 then
        return
    end
    aboard[p] = true
    core.body_balloon(p, true)
    core.player_wear(p, SLOT, ENVELOPE)
    core.player_place(p, ev.x + 0.5, ev.y + 1, ev.z + 0.5)
    core.notice(p, "Jump to rise, crouch to come down; landed, right click a block to get out")
end)

-- Dead, the balloon comes down with them: back in the bag where they wake.
core.event.on("core:died", function(ev)
    if aboard[ev.player] then
        leave(ev.player)
    end
end)

core.event.on("core:tick", function(_)
    local here: { [number]: boolean } = {}
    for _, p in core.players() do
        here[p] = true
    end
    for p in aboard do
        if not here[p] then
            aboard[p] = nil
        end
    end
end)
