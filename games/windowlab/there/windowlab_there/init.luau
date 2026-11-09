--!strict
-- The far room of tools/windowlab.sh, with nobody in it: a green platform, a
-- red wall behind, and six creatures marching to and fro in front of its mouth,
-- three nearer and three further off. A window must show all six, moving, cut
-- by the frame of the mouth; a body behind the wall must not show.
local FLOOR = core.register_node("windowlab_there:floor", 80, 170, 90, true, 0, "", 0, 1.0, "stone")
local WALL = core.register_node("windowlab_there:wall", 190, 50, 45, true, 0, "", 0, 1.0, "stone")
local Y = 40
core.register_mob("windowlab_there:walker", core.model_id("mobs:zombie"), 20, 1.6, "worker", 0, 0, 1, "", 0, 0, false, 0, false, "")

local function fill(x0: number, x1: number, y0: number, y1: number, z0: number, z1: number, b: number)
    for x = x0, x1 do
        for y = y0, y1 do
            for z = z0, z1 do core.set_block(x, y, z, b) end
        end
    end
end

core.event.on("core:world_ready", function()
    fill(22, 43, Y, Y, 34, 62, FLOOR)
    fill(22, 43, Y + 1, Y + 7, 60, 60, WALL)
end)

local walkers: { { id: number, z: number, east: boolean } } = {}
local placed, since = false, 0.0
core.event.on("core:tick", function(ev)
    if not placed then placed = core.portal_place(0, 32.5, Y + 3, 40.5, 0, 1, 3, 2) == 1 end
    since += ev.dt
    if since < 2 then return end
    since = 0
    if #walkers == 0 then
        for i = 0, 5 do
            local z = 45 + (i % 3) * 4
            local id = core.spawn_mob("windowlab_there:walker", 27.5 + i * 2, Y + 1, z + 0.5)
            if id ~= 0 then table.insert(walkers, { id = id, z = z, east = i % 2 == 0 }) end
        end
        core.log(`windowlab: {#walkers} walkers`)
    end
    for _, w in walkers do
        if core.mob_going(w.id) == 0 then
            w.east = not w.east
            core.mob_goto(w.id, if w.east then 38 else 27, Y + 1, w.z)
        end
    end
end)
