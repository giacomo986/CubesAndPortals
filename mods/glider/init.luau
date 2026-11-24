--!strict
-- glider: a hang glider of wool over sticks.
--
-- Held in the hand, it is ready: step off a height and, once falling, the
-- wing opens over you (core.body_glide) and carries you where you look at 9
-- m/s, sinking a metre and a half a second -- look down to dive faster, up to
-- slow it; crouch folds it and you drop. The ground, or the glider put away,
-- ends it; a glide down to the ground is not a fall (the engine's). Everybody
-- sees the wing over you while you are off the ground (core.player_wear,
-- tools/make_flight_models.py), and you hung prone under it, your hands on
-- its bar (the robot's clip "glide", tools/make_flight_clips.py).

local GLIDER = "glider:glider"
core.register_item(GLIDER, 1)
-- The Cloud Manta's glider (games/skies: whoever is there when it falls gets
-- one): no recipe, a glider like the other, and the storm's winds under it --
-- every LIFT_EVERY seconds aloft a gust lifts it (core.player_launch), so it
-- sinks about a third as fast and goes three times as far.
local STORM = "glider:storm_glider"
core.register_item(STORM, 1)
core.register_model("glider:storm_glider_held", "glider_held.gltf")
local LIFT_EVERY, LIFT = 1.5, 6.0
core.register_model("glider:glider_held", "glider_held.gltf")   -- tools/make_held_models.py
local WING = core.register_model("glider:wing", "glider.gltf")
core.recipe_shaped(GLIDER, 1,
    "animals:wool animals:wool animals:wool",
    "timber:stick timber:stick timber:stick",
    "-            timber:stick -")

local SLOT = 8   -- of core.player_wear's ten: mods/boats has 10, carts and the pony 9

local ready: { [number]: boolean } = {}   -- holding it: body_glide on
local lift: { [number]: number } = {}     -- a storm glider's wait for its next gust
local worn: { [number]: boolean } = {}    -- the wing drawn over them

-- Nothing under the feet for a node and a half: off the ground.
local function aloft(p: number): boolean
    local x, y, z = core.player_pos(p)
    if x == nil or y == nil or z == nil then
        return false
    end
    local fx, fz = math.floor(x), math.floor(z)
    for dy = 0, 1 do
        local id = core.get_block(fx, math.floor(y - 0.1) - dy, fz)
        if id ~= 0 and core.node_solid(id) then
            return false
        end
    end
    return true
end

core.event.on("core:tick", function(ev)
    local here: { [number]: boolean } = {}
    for _, p in core.players() do
        here[p] = true
        local held = core.held_item(p)
        local holding = held == GLIDER or held == STORM
        if holding ~= (ready[p] == true) then
            core.body_glide(p, holding)
            ready[p] = holding or nil
        end
        local wear = holding and aloft(p)
        if wear ~= (worn[p] == true) then
            core.player_wear(p, SLOT, if wear then WING else 0)
            core.player_clip(p, if wear then "glide" else "")
            worn[p] = wear or nil
        end
        if wear and held == STORM then
            lift[p] = (lift[p] or LIFT_EVERY) - ev.dt
            if (lift[p] :: number) <= 0 then
                lift[p] = LIFT_EVERY
                core.player_launch(p, 0, LIFT, 0, 0.1, true)
            end
        else
            lift[p] = nil
        end
    end
    for p in ready do
        if not here[p] then
            ready[p], worn[p], lift[p] = nil, nil, nil
        end
    end
end)
