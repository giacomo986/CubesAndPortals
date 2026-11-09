--!strict
-- The room the viewer stands in (tools/windowlab.sh): a white platform in the
-- sky and one mouth, its front to +z, the viewer five nodes in front of it and
-- put back there every second. Everything to look at is on the other side.
local FLOOR = core.register_node("windowlab_here:floor", 225, 225, 230, true, 0, "", 0, 1.0, "stone")
local Y = 40

local function fill(x0: number, x1: number, y0: number, y1: number, z0: number, z1: number, b: number)
    for x = x0, x1 do
        for y = y0, y1 do
            for z = z0, z1 do core.set_block(x, y, z, b) end
        end
    end
end

core.event.on("core:world_ready", function()
    fill(24, 41, Y, Y, 34, 56, FLOOR)
end)

local since, placed = 0.0, false
core.event.on("core:tick", function(ev)
    since += ev.dt
    if not placed then placed = core.portal_place(0, 32.5, Y + 3, 40.5, 0, 1, 3, 2) == 1 end
    if since < 1 then return end
    since = 0
    for _, p in core.players() do
        core.player_place(p, 32.5, Y + 1, 48.5, 0, 0, -1)
    end
end)
