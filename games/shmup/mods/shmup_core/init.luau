--!strict
-- shmup_core: the world of the arcade shooter (games/shmup/mods/shmup has the
-- game). A sea, a carrier's deck with the doorway home on it, and the field
-- over the sea -- one screen, 82 across and 52 up, at the height bodies fly
-- at, shut in by walls nobody sees. And the body: a fighter, which flies.
-- The planes are tools/make_shmup_models.py's -- the front's plane and bomber
-- in liveries of their own -- and are here because shmup is also a player's
-- machine's code, whose files all go to players. The pictures and the sounds
-- are tools/make_shmup_assets.py's.
--
-- The picture's top is +z (the game's camera, core.top_view_at with yaw
-- 180), so its right is -x. Opening 0 faces +z, up the screen: a fighter
-- that arrives is on the deck at the bottom, nose to the field.

local SEA    = core.register_node("shmup_core:sea", 30, 96, 190)
local FOAM   = core.register_node("shmup_core:foam", 150, 200, 235)
local SAND   = core.register_node("shmup_core:sand", 222, 200, 130)
local GREEN  = core.register_node("shmup_core:green", 60, 150, 70)
local DECK   = core.register_node("shmup_core:deck", 96, 100, 108)
local LINE   = core.register_node("shmup_core:line", 240, 220, 60, true, 6)
local HULL   = core.register_node("shmup_core:hull", 60, 64, 72)
local FRAME  = core.register_node("shmup_core:frame", 34, 34, 36)
local LAMP   = core.register_node("shmup_core:lamp", 255, 240, 200, true, 14)
-- The field's walls: solid, and glass that hides nothing.
local WALL   = core.register_node("shmup_core:wall", 255, 255, 255)
core.node_glass(WALL, 0)

core.player_model(core.register_model("shmup_core:fighter", "fighter.gltf"))
for _, m in { "foe_dart", "foe_weaver", "foe_ace", "bomber" } do
    core.register_model(`shmup_core:{m}`, `{m}.gltf`)
end
core.eye_height(1.0)
-- walk, sprint, fly, jump, gravity. Flying is what the game is played at:
-- 24 m/s crosses the screen in three and a half seconds (sprint doubles it).
core.movement(6, 6, 24, 0, -25)

core.mapgen_core(false)

-- As games/shmup/mods/shmup has to know them too.
local CX, CZ = 64, 66      -- the middle of the screen
local FLY    = 30          -- where a fighter's feet are
local SEA_Y  = 20
local HALF_W, HALF_H = 41, 26

local BOXES: { { number } } = {}
local function box(x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: number)
    table.insert(BOXES, { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1),
                           math.max(x0, x1), math.max(y0, y1), math.max(z0, z1), b })
end

-- The sea, wider than the picture, and a few islands on it.
box(CX - 70, SEA_Y, CZ - 44, CX + 70, SEA_Y, CZ + 44, SEA)
for _, i in { { -30, 12, 7 }, { 22, -8, 5 }, { 38, 20, 4 }, { -12, -14, 3 }, { -44, -6, 4 }, { 6, 22, 6 } } do
    local x, z, r = CX + i[1], CZ + i[2], i[3]
    box(x - r - 1, SEA_Y, z - r, x + r + 1, SEA_Y, z + r, FOAM)
    box(x - r, SEA_Y, z - r - 1, x + r, SEA_Y, z + r + 1, FOAM)
    box(x - r, SEA_Y + 1, z - r + 1, x + r, SEA_Y + 1, z + r - 1, SAND)
    box(x - r + 1, SEA_Y + 1, z - r, x + r - 1, SEA_Y + 1, z + r, SAND)
    if r > 3 then box(x - r + 2, SEA_Y + 2, z - r + 2, x + r - 2, SEA_Y + 2, z + r - 2, GREEN) end
end

-- The carrier at the bottom of the picture: a deck at the height fighters
-- fly at, its hull down to the water, the doorway home at its stern.
local DZ0, DZ1 = CZ - 34, CZ - 25      -- z 32 .. 41
box(CX - 4, SEA_Y + 1, DZ0, CX + 4, FLY - 2, DZ1, HULL)
box(CX - 5, FLY - 1, DZ0, CX + 5, FLY - 1, DZ1, DECK)
box(CX, FLY - 1, DZ0 + 2, CX, FLY - 1, DZ1, LINE)
for v = 0, 3 do
    for u = -2, 2 do
        local hole = u == 0 and v < 2
        box(CX + u, FLY + v, DZ0, CX + u, FLY + v, DZ0, FRAME)
        box(CX + u, FLY + v, DZ0 + 1, CX + u, FLY + v, DZ0 + 1, if hole then 0 elseif v == 3 and u == 0 then LAMP else FRAME)
    end
end

-- The walls: round the field, and round the deck that leads into it.
local X0, X1 = CX - HALF_W - 1, CX + HALF_W + 1
local Z0, Z1 = CZ - HALF_H + 1, CZ + HALF_H + 2      -- the field's bottom row is the deck's last
for y = FLY - 1, FLY + 3 do
    box(X0, y, Z0, X0, y, Z1, WALL)
    box(X1, y, Z0, X1, y, Z1, WALL)
    box(X0, y, Z1, X1, y, Z1, WALL)
    box(X0, y, Z0, CX - 7, y, Z0, WALL)
    box(CX + 7, y, Z0, X1, y, Z0, WALL)
    if y >= FLY then
        box(CX - 7, y, DZ0, CX - 7, y, Z0, WALL)
        box(CX + 7, y, DZ0, CX + 7, y, Z0, WALL)
        box(CX - 7, y, DZ0 - 1, CX + 7, y, DZ0 - 1, WALL)
    end
end

core.register_mapgen("shmup_core:field", function(ctx)
    for _, b in BOXES do
        local x0, x1 = math.max(b[1], ctx.min_x), math.min(b[4], ctx.max_x)
        local z0, z1 = math.max(b[3], ctx.min_z), math.min(b[6], ctx.max_z)
        if x0 <= x1 and z0 <= z1 then
            for x = x0, x1 do
                for z = z0, z1 do
                    for y = b[2], b[5] do core.set_block(x, y, z, b[7]) end
                end
            end
        end
    end
end)

-- Portal 0 in the doorway at the stern, facing +z: up the screen. Started
-- with a --portal-to (tools/shmup.sh) that mouth is the way home. Started
-- from the rooms file, as the game does it, the world has no mouth of its
-- own: the doorway is said to the other rooms as "door"
-- (core.room_portal_set), which the gate on the sandbox's plaza links to
-- (mods/gates).
core.set_portal(0, CX + 0.5, FLY + 1, DZ0 + 1.6)
local door_said = false
core.event.on("core:tick", function(_)
    if door_said then return end
    door_said = true
    for k = 0, 1 do
        if core.portal_room(k) == "" then core.portal_park(k) end
    end
    if core.portal_room(0) == "" then
        door_said = core.room_portal_set("door", CX + 0.5, FLY + 1, DZ0 + 1.5, 0, 1, 0.5, 1.0) == 1
    end
end)

core.event.on("core:pre_break", function(ev) ev.cancel = true end)
core.event.on("core:pre_place", function(ev) ev.cancel = true end)
