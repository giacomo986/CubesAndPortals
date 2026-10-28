--!strict
-- Boats: a hull of five planks, carried in the bag. Right click the water (or
-- the bottom under it) with one in the hand and you sit in it on the top:
-- W and S row it, at a sprint ahead -- twice a swim -- and A and D steer
-- (core.body_boat). Right click a shore to step out onto it, or run aground:
-- either way the boat goes back in the bag. Everybody sees the hull, worn on
-- the feet (core.player_wear), the body sat in it (the drive_idle clip), and
-- hears the oars. A boat under way shoves the creatures in its path aside,
-- and two boats that meet bounce apart.

local BOAT = "boats:boat"
core.register_item(BOAT, 1)
core.register_model("boats:boat_held", "boat_held.gltf")   -- small, in the hand (tools/make_held_models.py)
local HULL = core.register_model(BOAT, "boat.gltf")
core.recipe_shaped(BOAT, 1,
    "timber:planks -             timber:planks",
    "timber:planks timber:planks timber:planks")

local WATER = core.node_id("lakes:water")
local SLOT = 10       -- of core.player_wear's ten; nothing else in the sandbox dresses
local AGROUND = 10    -- ticks on land before the boat is put back in the bag

local aboard: { [number]: number } = {}   -- player -> ticks on land
local STROKE = 0.9    -- seconds between two strokes of the oars
local BUMP = 1.5      -- nodes apart at which two hulls touch
local UNDER_WAY = 1.5 -- m/s: slower than this rows no sound and shoves nothing

-- Per boat: where it was last tick, the oars' clock, and when it may bounce again.
local way: { [number]: { x: number, y: number, z: number, oars: number, bounce: number } } = {}

local function leave(p: number)
    aboard[p] = nil
    way[p] = nil
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

-- Under way: the oars heard, and what is in the way shoved off the bow.
local function row(p: number, x: number, y: number, z: number, dt: number)
    local w = way[p]
    if w == nil then
        w = { x = x, y = y, z = z, oars = 0, bounce = 0 }
        way[p] = w
    end
    local vx, vz = (x - w.x) / dt, (z - w.z) / dt
    w.x, w.y, w.z = x, y, z
    w.bounce = math.max(0, w.bounce - dt)
    local speed = math.sqrt(vx * vx + vz * vz)
    if speed < UNDER_WAY then
        w.oars = STROKE   -- the first stroke as soon as it moves off
        return
    end
    w.oars += dt
    if w.oars >= STROKE then
        w.oars = 0
        core.play_sound("boats:paddle", x, y, z)
    end
    -- The cell off the bow, at the water and over it: its creatures go to
    -- whichever side of the bow they are on.
    local ax, az = vx / speed, vz / speed
    local bx, bz = math.floor(x + ax * 1.2), math.floor(z + az * 1.2)
    local side = if (bx + 0.5 - x) * az - (bz + 0.5 - z) * ax > 0 then 1 else -1
    local sx, sz = math.round(az * side), math.round(-ax * side)
    for dy = 0, 1 do
        core.push_mobs(bx, math.floor(y) + dy, bz, sx, 0, sz)
    end
end

-- Two hulls that touch push off each other, each away from the other.
local function bounce(list: { number })
    for i = 1, #list do
        for j = i + 1, #list do
            local p, q = list[i], list[j]
            local wp, wq = way[p], way[q]
            if wp and wq and wp.bounce <= 0 and wq.bounce <= 0 then
                local dx, dz = wq.x - wp.x, wq.z - wp.z
                local d = math.sqrt(dx * dx + dz * dz)
                if d < BUMP then
                    if d < 0.01 then
                        dx, dz, d = 1, 0, 1
                    end
                    dx, dz = dx / d * 4, dz / d * 4
                    core.player_launch(p, -dx, 0, -dz, 0.5)
                    core.player_launch(q, dx, 0, dz, 0.5)
                    wp.bounce, wq.bounce = 0.5, 0.5
                    core.play_sound("boats:paddle", (wp.x + wq.x) / 2, wp.y, (wp.z + wq.z) / 2)
                end
            end
        end
    end
end

core.event.on("core:tick", function(ev)
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
            way[p] = nil
            continue
        end
        local x, y, z = core.player_pos(p)
        local fx, fy, fz = math.floor(x), math.floor(y), math.floor(z)
        if core.get_block(fx, fy, fz) == WATER or core.get_block(fx, fy - 1, fz) == WATER then
            aboard[p] = 0
            row(p, x, y, z, math.max(ev.dt, 0.001))
        elseif n + 1 >= AGROUND then
            leave(p)
        else
            aboard[p] = n + 1
        end
    end
    local afloat = {}
    for p in aboard do
        if way[p] then
            table.insert(afloat, p)
        end
    end
    bounce(afloat)
end)
