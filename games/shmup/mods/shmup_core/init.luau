--!strict
-- shmup_core: the world of the arcade shooter (games/shmup/mods/shmup has the
-- game). A sea; an island with a hangar on it, the doorway home in its back
-- wall, four fighters parked in it and an apron down to the water; and, out
-- over the sea, the field -- one screen, 82 across and 62 up, at the height
-- fighters fly at, shut in by walls nobody sees. A player arrives on foot,
-- the sandbox's robot (mods/robot, linked here); the game makes them a
-- fighter when they board one.
-- The planes are tools/make_shmup_models.py's and are here because shmup is
-- also a player's machine's code, whose files all go to players. The pictures
-- and the sounds are tools/make_shmup_assets.py's.
--
-- The picture's top is +z (the game's camera, core.top_view_at with yaw
-- 180), so its right is -x. Opening 0 faces +z: whoever arrives looks down
-- the hangar at the fighters and, past its open front, the sea.

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

core.player_model(core.register_model("shmup_core:robot", "robot.gltf"))
-- What a player may wear or be instead, as in every world (mods/robot).
for _, extra in {"cap", "tophat", "headphones", "scarf", "bowtie", "medal",
                  "human", "human_arms", "human_short", "human_long", "human_beard",
                  "mole", "mole_arms", "mole_lamp", "mole_goggles", "mole_glasses",
                  "frog", "frog_arms", "frog_spots", "frog_crown", "frog_lily",
                  "knight", "knight_arms",
                  "human_cap", "human_tophat", "human_headphones", "human_scarf", "human_bowtie", "human_medal",
                  "mole_cap", "mole_tophat", "mole_headphones", "mole_scarf", "mole_bowtie", "mole_medal",
                  "frog_cap", "frog_tophat", "frog_headphones", "frog_scarf", "frog_bowtie", "frog_medal"} do
    core.register_model("shmup_core:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
core.register_model("shmup_core:robot_arms", "robot_arms.gltf")
for _, m in { "fighter", "wingman", "foe_dart", "foe_weaver", "foe_ace", "bomber", "mega", "turret", "tank",
                 "foe_diver", "foe_sweeper", "foe_plated", "foe_gunship", "carrier" } do
    core.register_model(`shmup_core:{m}`, `{m}.gltf`)
end
core.eye_height(1.5)
-- walk, sprint, fly, jump, gravity. Nobody flies on foot: the game gives a
-- body its flying when it boards a fighter (core.body_movement).
core.movement(6, 9, 0, 8, -25)

core.mapgen_core(false)

-- As games/shmup/mods/shmup has to know them too.
local CX, CZ = 64, 140     -- the middle of the screen
local FLY    = 30          -- where a fighter's feet are
local SEA_Y  = 20
local GROUND = SEA_Y + 1   -- the island's top blocks: feet stand at GROUND + 1
local HALF_W, HALF_H = 41, 31

local BOXES: { { number } } = {}
local function box(x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: number)
    table.insert(BOXES, { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1),
                           math.max(x0, x1), math.max(y0, y1), math.max(z0, z1), b })
end

-- The sea, wider than the picture and bare: the islands and the crests that
-- go by under a fighter are drawn by its player's machine (the game), so that
-- they scroll. The one island that is blocks is the base, south of the
-- picture's bottom edge and never in it.
box(CX - 110, SEA_Y, 0, CX + 110, SEA_Y, CZ + 80, SEA)
box(CX - 37, SEA_Y, 5, CX + 37, SEA_Y, 80, FOAM)
box(CX - 35, GROUND, 7, CX + 35, GROUND, 78, SAND)
box(CX - 33, GROUND, 9, CX + 33, GROUND, 76, GREEN)

-- The hangar: x 38..90, z 21..45, open to the north (up the screen) onto an
-- apron that runs to the water. A lane's line from each parked fighter out.
local HX0, HX1, HZ0, HZ1 = CX - 26, CX + 26, 21, 45
local ROOF = GROUND + 8
box(HX0, GROUND, HZ0, HX1, GROUND, 78, DECK)
for i = 0, 3 do
    local x = CX - 18 + i * 12
    box(x, GROUND, 30, x, GROUND, 78, LINE)
end
box(HX0, GROUND + 1, HZ0, HX0, ROOF, HZ1, HULL)
box(HX1, GROUND + 1, HZ0, HX1, ROOF, HZ1, HULL)
box(HX0, GROUND + 1, HZ0, HX1, ROOF, HZ0, HULL)
box(HX0, ROOF - 2, HZ1, HX1, ROOF, HZ1, HULL)            -- over the open front
-- The roof is trusses with the sky between them: the daylight is what
-- lights the floor. A band of lamps along each wall as well.
for z = HZ0, HZ1, 3 do box(HX0, ROOF, z, HX1, ROOF, z, FRAME) end
for x = HX0 + 2, HX1 - 2, 4 do box(x, GROUND + 4, HZ0, x, GROUND + 4, HZ0, LAMP) end
for z = HZ0 + 2, HZ1 - 2, 4 do
    box(HX0, GROUND + 4, z, HX0, GROUND + 4, z, LAMP)
    box(HX1, GROUND + 4, z, HX1, GROUND + 4, z, LAMP)
end
-- The doorway home in the back wall.
local DZ0 = HZ0 - 1
for v = 0, 3 do
    for u = -2, 2 do
        local hole = u == 0 and v < 2
        box(CX + u, GROUND + 1 + v, DZ0, CX + u, GROUND + 1 + v, DZ0, FRAME)
        box(CX + u, GROUND + 1 + v, HZ0, CX + u, GROUND + 1 + v, HZ0, if hole then 0 elseif v == 3 and u == 0 then LAMP else FRAME)
    end
end
-- Nobody walks out of the front: a fighter leaves by it, in the game's take-off.
box(HX0 + 1, GROUND + 1, HZ1 + 1, HX1 - 1, ROOF - 3, HZ1 + 1, WALL)

-- The walls round the field.
local X0, X1 = CX - HALF_W - 1, CX + HALF_W + 1
local Z0, Z1 = CZ - HALF_H - 1, CZ + HALF_H + 1
for y = FLY - 1, FLY + 3 do
    box(X0, y, Z0, X0, y, Z1, WALL)
    box(X1, y, Z0, X1, y, Z1, WALL)
    box(X0, y, Z1, X1, y, Z1, WALL)
    box(X0, y, Z0, X1, y, Z0, WALL)
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

-- Portal 0 in the doorway in the hangar's back wall, facing +z. Started
-- with a --portal-to (tools/shmup.sh) that mouth is the way home. Started
-- from the rooms file, as the game does it, the world has no mouth of its
-- own: the doorway is said to the other rooms as "door"
-- (core.room_portal_set), which the gate on the sandbox's plaza links to
-- (mods/gates).
core.set_portal(0, CX + 0.5, GROUND + 2, HZ0 + 0.6)
local door_said = false
core.event.on("core:tick", function(_)
    if door_said then return end
    door_said = true
    for k = 0, 1 do
        if core.portal_room(k) == "" then core.portal_park(k) end
    end
    if core.portal_room(0) == "" then
        door_said = core.room_portal_set("door", CX + 0.5, GROUND + 2, HZ0 + 0.5, 0, 1, 0.5, 1.0)
    end
end)

core.event.on("core:pre_break", function(ev) ev.cancel = true end)
core.event.on("core:pre_place", function(ev) ev.cancel = true end)
