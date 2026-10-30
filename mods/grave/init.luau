--!strict
-- grave: in a world without a plaza (a survival world, core.plaza() nil) a
-- player who dies leaves the 36 squares of their bag in a grave where it
-- happened -- what they wear stays on them -- and the way back to it is drawn
-- on their map (maps:route). A right click opens it; emptied, it is gone; broken,
-- what is left falls out.
--
-- The engine keeps the bag on a death on purpose (ServerSessions::kill_body:
-- there was no way back to where it happened). The route on the map is that
-- way back, and a survival world is where losing the place should cost more
-- than the walk.

local GRAVE = core.register_node("grave:stone", 120, 118, 112, true)
local SLOTS = 36

if core.plaza() ~= nil then
    return   -- a world with a plaza keeps the engine's rule
end

-- Where the graves are, "x,y,z;x,y,z", to take an emptied one away.
local graves: { string } = {}
do
    local kept = core.storage_get("graves")
    if kept ~= "" then
        graves = string.split(kept, ";")
    end
end
local function keep()
    core.storage_set("graves", table.concat(graves, ";"))
end

-- The cell for a grave: where the body was, or the first air above it; the
-- ground over a body that fell out of the world or into deep water.
local function cell_at(x: number, y: number, z: number): (number, number, number)
    local gx, gz = math.floor(x), math.floor(z)
    local gy = math.floor(y)
    if gy < 2 then
        gy = core.terrain_height(gx, gz)
    end
    for _ = 1, 12 do
        if core.get_block(gx, gy, gz) == 0 then
            break
        end
        gy += 1
    end
    return gx, gy, gz
end

core.event.on("core:died", function(ev)
    local x, y, z = cell_at(ev.x, ev.y, ev.z)
    core.set_block(x, y, z, GRAVE)
    core.container_make(x, y, z, SLOTS)
    if core.bag_to_container(ev.player, x, y, z) == 0 then
        core.container_clear(x, y, z)
        core.set_block(x, y, z, 0)
        return
    end
    table.insert(graves, `{x},{y},{z}`)
    keep()
    local px, _, pz = core.player_pos(ev.player)
    core.event.emit("maps:route", { player = ev.player, label = "Tomba",
                                    points = `{math.floor(px or x)},{math.floor(pz or z)};{x},{z}` })
    core.notice(ev.player, `La tua borsa e' rimasta in una tomba a {x} {y} {z}: la strada e' sulla mappa.`)
end)

core.event.on("core:used", function(ev)
    if ev.id ~= GRAVE then
        return
    end
    ev.cancel = true
    core.container_show(ev.x, ev.y, ev.z, SLOTS, ev.player)
end)

core.event.on("core:pre_break", function(ev)
    if ev.id == GRAVE then
        core.container_spill(ev.x, ev.y, ev.z)
        core.container_clear(ev.x, ev.y, ev.z)
    end
end)

-- Every two seconds: a grave with nothing left in it goes.
local since = 0.0
core.event.on("core:tick", function(ev)
    since += ev.dt
    if since < 2.0 or #graves == 0 then
        return
    end
    since = 0.0
    for i = #graves, 1, -1 do
        local p = string.split(graves[i], ",")
        local x, y, z = tonumber(p[1]) or 0, tonumber(p[2]) or 0, tonumber(p[3]) or 0
        local empty = core.get_block(x, y, z) == GRAVE
        if empty then
            for slot = 1, SLOTS do
                if core.container_item(x, y, z, slot) ~= 0 then
                    empty = false
                    break
                end
            end
        end
        if empty or core.get_block(x, y, z) ~= GRAVE then
            if core.get_block(x, y, z) == GRAVE then
                core.container_clear(x, y, z)
                core.set_block(x, y, z, 0)
            end
            table.remove(graves, i)
            keep()
        end
    end
end)
