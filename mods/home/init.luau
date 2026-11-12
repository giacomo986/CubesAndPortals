--!strict
-- home: the door of a player's own room.
--
-- A server whose rooms file has the template [room home-*] gives every player
-- a room of their own, a whole world: "home-" and the start of their key. It
-- is made the first time a frame of theirs is told to lead to it (mods/portal,
-- "my room" in the frame's menu) and somebody comes near that frame, and it
-- saves and stops a minute after nobody is in it or near a way into it.
--
-- This mod does nothing anywhere else. In such a room it says one portal to
-- the other rooms, "door", one wide and two tall, three steps north of the
-- spawn and facing it: what the player's frame links to, and the way back out
-- to wherever that frame stands now. The frame round it is put up, on a floor
-- of its own, the first time a player is in the room; breaking it changes
-- nothing but the look, the door is where it was said.
--
-- ponytail: one door, where the world put it. The player moves the OTHER end
-- (any frame of theirs, one at a time); moving this end wants a block to carry.

local ROOM = core.room()
if string.sub(ROOM, 1, 5) ~= "home-" or core.side() == "client" then
    return
end

local FRAME = core.register_node("home:frame", 150, 112, 64, true, 3, "", 0, 1.5, "wood")
local FLOOR = core.register_node("home:floor", 176, 136, 84, true, 0, "", 0, 2.0, "wood")

-- The hole's lowest cell, once the world says where it starts.
local hx: number?, hy: number?, hz: number? = nil, nil, nil
local said, built = false, core.storage_get("built") == "1"

local function say(): boolean
    if hx == nil then
        local at = core.storage_get("door")
        local x, y, z = string.match(at, "^(-?%d+) (-?%d+) (-?%d+)$")
        if x then
            hx, hy, hz = tonumber(x), tonumber(y), tonumber(z)
        else
            local sx, sy, sz = core.world_spawn()
            if sx == nil or sy == nil or sz == nil then
                return false
            end
            hx, hy, hz = math.floor(sx), math.floor(sy), math.floor(sz) - 3
            core.storage_set("door", `{hx} {hy} {hz}`)
        end
    end
    local x, y, z = hx :: number, hy :: number, hz :: number
    -- Standing in the XY plane, its front towards the spawn (+z).
    if core.room_portal_set("door", x + 0.5, y + 1.0, z + 0.5, 0, 1, 0.5, 1.0) == 0 then
        return false
    end
    core.log(`home: the door of {ROOM} is at {x} {y} {z}`)
    return true
end

-- The frame round the hole, a floor under it and in front, and the air a body
-- coming out needs.
local function build()
    local x, y, z = hx :: number, hy :: number, hz :: number
    for dx = -1, 1 do
        for dz = 0, 3 do
            core.set_block(x + dx, y - 1, z + dz, FLOOR)
            for dy = 0, 2 do
                if dz > 0 then
                    core.set_block(x + dx, y + dy, z + dz, 0)
                end
            end
        end
        for dy = 0, 2 do
            core.set_block(x + dx, y + dy, z, if dx == 0 and dy < 2 then 0 else FRAME)
        end
    end
    core.storage_set("built", "1")
    built = true
end

core.event.on("core:tick", function(_)
    if not said then
        said = say()
    end
    if said and not built and #core.players() > 0 then
        build()
    end
end)
