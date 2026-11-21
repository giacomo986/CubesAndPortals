--!strict
-- balloon: a hot-air balloon, carried folded in the bag.
--
-- Right click the ground with one in the hand and you stand in its basket
-- under the envelope (core.body_balloon): jump held lifts you at two and a half
-- metres a second, crouch lets you down, and otherwise it sinks slowly; the
-- keys push it along at a walk and the wind takes it east. Any height: over
-- the clouds, the world below. Landed, right click a block to climb out and
-- fold it back into the bag. Everybody sees it (core.player_wear,
-- tools/make_flight_models.py): stood in the basket, hands on the rim (the
-- robot's clip "balloon_idle", tools/make_flight_clips.py), a pilot flame
-- low over the burner -- and while it rises the right hand up on the valve
-- ("balloon_burn") and the burner roaring (balloon_burn.gltf). A descent in it
-- is not a fall.

local BALLOON = "balloon:balloon"
core.register_item(BALLOON, 1)
core.register_model("balloon:balloon_held", "balloon_held.gltf")   -- tools/make_held_models.py
local ENVELOPE = core.register_model("balloon:envelope", "balloon.gltf")
local BURNING = core.register_model("balloon:envelope_burning", "balloon_burn.gltf")
core.recipe_shaped(BALLOON, 1,
    "animals:wool  animals:wool  animals:wool",
    "animals:wool  ore:coal      animals:wool",
    "timber:planks timber:planks timber:planks")

local SLOT = 7   -- of core.player_wear's ten: mods/glider has 8, carts and the pony 9, boats 10

-- Who is aboard, and how they were seen last: where (y), and whether the
-- burner is on, held on for a while after the rise stops so it does not
-- flicker on and off.
type Ride = { y: number, burning: boolean, hold: number }
local aboard: { [number]: Ride } = {}

-- The burner's roar (core.sound_loop, tools/make_loop_sounds.py), going round
-- over the basket while it burns and moved with it: by player, its loop id.
local BURNER_UP = 2.6   -- the flame, over the head of whoever is in the basket
local roaring: { [number]: number } = {}
local function roar(p: number, on: boolean)
    local id = roaring[p]
    if not on then
        if id then
            core.sound_loop_stop(id)
            roaring[p] = nil
        end
        return
    end
    local x, y, z = core.player_pos(p)
    if x == nil or y == nil or z == nil then
        return
    end
    if id then
        core.sound_loop_move(id, x, y + BURNER_UP, z)
    else
        local got = core.sound_loop("balloon:burner", x, y + BURNER_UP, z, 24, 0.6)
        roaring[p] = if got ~= 0 then got else nil
    end
end

local function grounded(p: number): boolean
    local x, y, z = core.player_pos(p)
    if x == nil or y == nil or z == nil then
        return false
    end
    local id = core.get_block(math.floor(x), math.floor(y - 0.1), math.floor(z))
    return id ~= 0 and core.node_solid(id)
end

local function leave(p: number)
    aboard[p] = nil
    roar(p, false)
    core.body_balloon(p, false)
    core.player_wear(p, SLOT, 0)
    core.player_clip(p, "")
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
    aboard[p] = { y = ev.y + 1, burning = false, hold = 0 }
    core.body_balloon(p, true)
    core.player_wear(p, SLOT, ENVELOPE)
    core.player_clip(p, "balloon_idle")
    core.player_place(p, ev.x + 0.5, ev.y + 1, ev.z + 0.5)
    core.notice(p, "Jump to rise, crouch to come down; landed, right click a block to get out")
end)

-- Dead, the balloon comes down with them: back in the bag where they wake.
core.event.on("core:died", function(ev)
    if aboard[ev.player] then
        leave(ev.player)
    end
end)

core.event.on("core:tick", function(ev)
    local here: { [number]: boolean } = {}
    for _, p in core.players() do
        here[p] = true
        local r = aboard[p]
        local _, y = core.player_pos(p)
        if r and y and ev.dt > 0 then
            -- Rising faster than a metre a second: the burner is on.
            local rising = (y - r.y) / ev.dt > 1.0
            r.y = y
            r.hold = if rising then 0.5 else r.hold - ev.dt
            local burning = r.hold > 0
            if burning ~= r.burning then
                r.burning = burning
                core.player_wear(p, SLOT, if burning then BURNING else ENVELOPE)
                core.player_clip(p, if burning then "balloon_burn" else "balloon_idle")
            end
            roar(p, burning)
        end
    end
    for p in aboard do
        if not here[p] then
            aboard[p] = nil
        end
    end
    for p in roaring do
        if not aboard[p] then
            roar(p, false)
        end
    end
end)
