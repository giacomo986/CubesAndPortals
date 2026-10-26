--!strict
-- assault_core: the world of the assault, Basoalth's dimension. A valley 128
-- nodes wide running north from the camp the allies come into, four
-- strongholds along its road one after another -- an outpost of logs, a
-- bastion of stone, a battery of concrete bunkers, and the fortress of black
-- basalt with the hall of the core at its back -- and east of the valley a
-- chasm of magma, along which runs the road of the escape. What happens on it
-- (the fight, the credits, the core, Basoalth) is assault_game's.
--
-- Nothing of the engine's terrain, as the front. The world is not kept on
-- disk (tools/track.sh starts its room with no --world-file): what nobody has
-- been near for a while comes back as this makes it, so the dimension is
-- whole again once everybody has gone. The escape road is broken while
-- people are on it, and built again for the next try: it is a list of boxes
-- (TRACK), laid by the mapgen and again on "assault:track".

local GRASS    = core.register_node("assault_core:grass", 92, 104, 62)
local DIRT     = core.register_node("assault_core:dirt", 104, 78, 58)
local STONE    = core.register_node("assault_core:stone", 104, 100, 104)
local BEDROCK  = core.register_node("assault_core:bedrock", 34, 30, 36)
local RIDGE    = core.register_node("assault_core:ridge", 74, 66, 70)
local ROAD     = core.register_node("assault_core:road", 128, 116, 100)
local SANDBAG  = core.register_node("assault_core:sandbag", 176, 156, 108)
local LOGS     = core.register_node("assault_core:logs", 112, 80, 50)
local PLANKS   = core.register_node("assault_core:planks", 146, 108, 68)
local WALL     = core.register_node("assault_core:wall", 126, 120, 114)
local CONCRETE = core.register_node("assault_core:concrete", 150, 150, 146)
local BASALT   = core.register_node("assault_core:basalt", 46, 40, 50)
local LAMP     = core.register_node("assault_core:lamp", 250, 226, 170, true, 14)
local EMBER    = core.register_node("assault_core:ember", 200, 70, 230, true, 12)   -- Basoalth's light
local ALLY     = core.register_node("assault_core:ally", 50, 110, 200)               -- the allies' colour
local FOE      = core.register_node("assault_core:foe", 120, 30, 140)                -- Basoalth's
-- Tough (assault_game's TOUGH): no shell opens them.
local KEEP     = core.register_node("assault_core:keep", 30, 26, 36)          -- the fortress's walls
local GATE     = core.register_node("assault_core:gate", 84, 84, 92)          -- its gate: not tough, blown in
local SEAL     = core.register_node("assault_core:seal", 140, 50, 190, true, 10)   -- the hall's door, shut
local HALL     = core.register_node("assault_core:hall", 54, 46, 64)          -- the hall's walls and floor
local MAGMA    = core.register_node("assault_core:magma", 255, 104, 28, true, 15)
-- The river's (below): it kills whatever goes into it (assault_game).
local LAVA     = core.register_node("assault_core:lava", 255, 84, 18, true, 15)
local TRACK    = core.register_node("assault_core:track", 52, 52, 58)
local HAZARD   = core.register_node("assault_core:hazard", 226, 182, 40)
local FRAME    = core.register_node("assault_core:frame", 110, 40, 150)      -- the way out's frame
-- The lands round each stronghold (BIOME below).
local MUD      = core.register_node("assault_core:mud", 70, 72, 42)
local BOG      = core.register_node("assault_core:bog", 40, 52, 34)
local REED     = core.register_node("assault_core:reed", 112, 118, 58, false)
local DEADWOOD = core.register_node("assault_core:deadwood", 74, 62, 50)
local ASH      = core.register_node("assault_core:ash", 96, 92, 90)
local CHAR     = core.register_node("assault_core:char", 32, 28, 26)
local CINDER   = core.register_node("assault_core:cinder", 236, 96, 30, true, 9)
local SHARD    = core.register_node("assault_core:shard", 116, 92, 136)
local CRYSTAL  = core.register_node("assault_core:crystal", 196, 120, 255, true, 9)
local BONE     = core.register_node("assault_core:bone", 222, 212, 190)
core.node_glass(CRYSTAL, 170)
-- What the allies' builders put against a wall to go over it (assault_game):
-- steel rails against the +z side of the cell, unturned -- the south face of
-- a wall, the way the allies come. Drawn as backrooms' ladder.
local LADDER   = core.register_node("assault_core:ladder", 190, 196, 200, true, 0, "upright", 0, 0, "metal")
core.node_box(LADDER, 0.1, 0, 0.8, 0.9, 1, 1)
core.node_climbable(LADDER)
core.node_model(LADDER, core.register_model("assault_core:ladder", "ladder.gltf"))

-- What a player is here: the sandbox's robot (mods/robot, linked), with the
-- front's arms (its guns).
core.player_model(core.register_model("assault_core:robot", "robot.gltf"))
for _, extra in {"cap", "tophat", "headphones", "scarf", "bowtie", "medal",
                  "human", "human_arms", "human_short", "human_long", "human_beard",
                  "mole", "mole_arms", "mole_lamp", "mole_goggles", "mole_glasses",
                  "frog", "frog_arms", "frog_spots", "frog_crown", "frog_lily",
                  "human_cap", "human_tophat", "human_headphones", "human_scarf", "human_bowtie", "human_medal",
                  "mole_cap", "mole_tophat", "mole_headphones", "mole_scarf", "mole_bowtie", "mole_medal",
                  "frog_cap", "frog_tophat", "frog_headphones", "frog_scarf", "frog_bowtie", "frog_medal"} do
    core.register_model("assault_core:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
core.register_model("assault_core:robot_arms", "robot_arms.gltf")
core.eye_height(1.5)

core.movement(4.8, 8.5, 0, 8, -25)
core.mapgen_core(false)
-- Basoalth's sky: red, its clouds whirling, the sun still up.
core.world_storm(205, 52, 40)

-- The places assault_game has to know too, kept in step with its own copy.
local GROUND = 20      -- flat ground is air from here up
local W      = 128     -- the valley, 0..W-1 on x
local L      = 1080    -- and 0..L-1 on z
local MID    = 64
local CAMP   = { MID, 22 }
local FORTS  = { { MID, 250 }, { MID, 480 }, { MID, 710 }, { MID, 905 } }
-- The fortress: outer walls x 20..108, z 870..1012; the courtyard to z 940,
-- the hall behind it (inside x 26..102, z 943..1009, roof at GROUND + 24).
local HALL_Z0, HALL_Z1, HALL_ROOF = 943, 1009, GROUND + 24

-- The way in from the sandbox: opening 0 (facing +z) at the camp's south end.
core.set_portal(0, MID + 0.5, GROUND + 1, 6.5)
-- The way out at the end of the escape: opening 1 (facing +x, the server's
-- way for every opening past the first) in a frame on the end's rock.
core.set_portal(1, 163.5, GROUND + 1, 12.5)

-- ===================================================================== boxes
-- Every building is boxes { x0, y0, z0, x1, y1, z1, block }, laid in order
-- by the mapgen over the ground; 0 is air.

local BOXES: { { number } } = {}
local TRACK_BOXES: { { number } } = {}
local function box(list, x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: number)
    table.insert(list, { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1),
                         math.max(x0, x1), math.max(y0, y1), math.max(z0, z1), b })
end
local function put(...) box(BOXES, ...) end
local function road(...) box(TRACK_BOXES, ...) end

-- ------------------------------------------------------------ the camp
-- Concrete yard, sandbag ring, the allies' colour in the floor, a frame round
-- the way in, crates (the shop's, assault_game) by the flagpole.
do
    local cx, cz = CAMP[1], CAMP[2]
    put(cx - 20, GROUND - 1, cz - 16, cx + 20, GROUND - 1, cz + 16, CONCRETE)
    for i = -20, 20, 5 do put(cx + i, GROUND - 1, cz - 16, cx + i, GROUND - 1, cz + 16, ALLY) end
    for _, z in { cz - 16, cz + 16 } do
        put(cx - 20, GROUND, z, cx - 4, GROUND, z, SANDBAG)
        put(cx + 4, GROUND, z, cx + 20, GROUND, z, SANDBAG)
    end
    put(cx - 20, GROUND, cz - 16, cx - 20, GROUND, cz + 16, SANDBAG)
    put(cx + 20, GROUND, cz - 16, cx + 20, GROUND, cz + 16, SANDBAG)
    put(cx - 3, GROUND, 5, cx + 3, GROUND + 4, 5, WALL)            -- behind the way in
    put(cx - 1, GROUND, 6, cx + 1, GROUND + 3, 6, WALL)
    put(cx, GROUND, 6, cx, GROUND + 1, 6, 0)
    for _, c in { { -20, -16 }, { 20, -16 }, { -20, 16 }, { 20, 16 } } do
        put(cx + c[1], GROUND + 1, cz + c[2], cx + c[1], GROUND + 1, cz + c[2], LAMP)
    end
    put(cx + 6, GROUND, cz, cx + 6, GROUND + 6, cz, WALL)           -- the flag
    put(cx + 7, GROUND + 4, cz, cx + 9, GROUND + 6, cz, ALLY)
    put(cx + 4, GROUND, cz - 3, cx + 5, GROUND, cz - 2, PLANKS)     -- the crates
end

-- ------------------------------------------------------------ 1 the outpost
-- A palisade of logs in a ring, a gate south and one north; two watchtowers inside the
-- gate, a hut, and a line of sandbags out in front.
do
    local cx, cz = FORTS[1][1], FORTS[1][2]
    for a = 0, 95 do
        local t = a / 96 * 2 * math.pi
        local x = cx + math.floor(math.cos(t) * 18 + 0.5)
        local z = cz + math.floor(math.sin(t) * 18 + 0.5)
        if math.abs(x - cx) > 3 then put(x, GROUND, z, x, GROUND + 3, z, LOGS) end   -- gates south and north
    end
    for _, sx in { -1, 1 } do
        local tx, tz = cx + sx * 9, cz - 10
        put(tx - 1, GROUND, tz - 1, tx + 1, GROUND + 5, tz + 1, LOGS)
        put(tx - 1, GROUND, tz, tx + 1, GROUND + 4, tz, 0)
        put(tx, GROUND, tz - 1, tx, GROUND + 4, tz + 1, 0)
        put(tx - 2, GROUND + 5, tz - 2, tx + 2, GROUND + 5, tz + 2, PLANKS)     -- its platform
        for _, c in { { -2, -2 }, { 2, -2 }, { -2, 2 }, { 2, 2 } } do
            put(tx + c[1], GROUND + 6, tz + c[2], tx + c[1], GROUND + 6, tz + c[2], LOGS)
        end
        put(tx + 1, GROUND, tz, tx + 1, GROUND + 4, tz, LOGS)                    -- the ladder side
    end
    put(cx - 5, GROUND, cz + 4, cx + 5, GROUND + 3, cz + 11, PLANKS)            -- the hut
    put(cx - 4, GROUND, cz + 5, cx + 4, GROUND + 2, cz + 10, 0)
    put(cx - 1, GROUND, cz + 4, cx + 1, GROUND + 1, cz + 4, 0)
    put(cx - 6, GROUND + 4, cz + 3, cx + 6, GROUND + 4, cz + 12, LOGS)
    put(cx + 6, GROUND + 1, cz + 7, cx + 6, GROUND + 1, cz + 8, 0)
    for x = cx - 22, cx + 22 do
        if math.abs(x - cx) > 3 and x % 7 ~= 0 then put(x, GROUND, cz - 30, x, GROUND, cz - 30, SANDBAG) end
    end
    put(cx, GROUND, cz, cx, GROUND + 6, cz, LOGS)                               -- the flagpole
end

-- ------------------------------------------------------------ 2 the bastion
-- Stone walls round a square, a gate in the south wall and one in the north, a tower on each
-- corner and a keep in the middle with a way up onto its roof.
do
    local cx, cz = FORTS[2][1], FORTS[2][2]
    local x0, x1, z0, z1 = cx - 24, cx + 24, cz - 18, cz + 18
    put(x0, GROUND, z0, x1, GROUND + 4, z1, WALL)
    put(x0 + 2, GROUND, z0 + 2, x1 - 2, GROUND + 4, z1 - 2, 0)
    put(x0 + 1, GROUND + 5, z0, x1 - 1, GROUND + 5, z0, WALL)                   -- crenels, south
    for x = x0 + 1, x1 - 1, 2 do put(x, GROUND + 5, z0, x, GROUND + 5, z0, 0) end
    put(cx - 3, GROUND, z0, cx + 3, GROUND + 3, z0 + 1, 0)                      -- the gate
    put(cx - 3, GROUND, z1 - 1, cx + 3, GROUND + 3, z1, 0)                      -- and the way on, north
    for _, c in { { x0, z0 }, { x1, z0 }, { x0, z1 }, { x1, z1 } } do
        put(c[1] - 2, GROUND, c[2] - 2, c[1] + 2, GROUND + 7, c[2] + 2, WALL)
        put(c[1] - 1, GROUND, c[2] - 1, c[1] + 1, GROUND + 6, c[2] + 1, 0)
        put(c[1] - 2, GROUND + 7, c[2] - 2, c[1] + 2, GROUND + 7, c[2] + 2, STONE)
    end
    put(x0 + 2, GROUND + 3, z0 + 2, x1 - 2, GROUND + 3, z0 + 3, PLANKS)          -- a walk behind the south wall
    for i = 0, 2 do put(cx - 10 - i, GROUND + i, z0 + 4, cx - 10 - i, GROUND + i, z0 + 4, STONE) end
    put(cx - 7, GROUND, cz - 2, cx + 7, GROUND + 6, cz + 8, WALL)                -- the keep
    put(cx - 6, GROUND, cz - 1, cx + 6, GROUND + 5, cz + 7, 0)
    put(cx - 2, GROUND, cz - 2, cx + 2, GROUND + 2, cz - 2, 0)
    put(cx - 8, GROUND + 7, cz - 3, cx + 8, GROUND + 7, cz + 9, STONE)
    for i = 0, 5 do put(cx + 5, GROUND + i, cz + 6 - i, cx + 5, GROUND + i, cz + 6 - i, STONE) end
    put(cx + 5, GROUND + 6, cz, cx + 5, GROUND + 7, cz, 0)
    put(cx, GROUND + 8, cz + 3, cx, GROUND + 13, cz + 3, WALL)                   -- the flagpole
end

-- ------------------------------------------------------------ 3 the battery
-- Three concrete bunkers in a line, slits facing south, trenches of sandbags
-- in front, gun pits behind (the howitzers, assault_game) and a lookout.
do
    local cx, cz = FORTS[3][1], FORTS[3][2]
    for _, bx in { cx - 30, cx, cx + 30 } do
        put(bx - 7, GROUND, cz - 5, bx + 7, GROUND + 3, cz + 5, CONCRETE)
        put(bx - 6, GROUND, cz - 4, bx + 6, GROUND + 2, cz + 4, 0)
        put(bx - 5, GROUND + 2, cz - 5, bx + 5, GROUND + 2, cz - 5, 0)            -- the slit
        put(bx - 1, GROUND, cz + 5, bx + 1, GROUND + 1, cz + 5, 0)                -- the door, north
        put(bx - 8, GROUND + 4, cz - 6, bx + 8, GROUND + 4, cz + 6, CONCRETE)
    end
    for _, tz in { cz - 16, cz - 26 } do
        for x = cx - 44, cx + 44 do
            if (x + tz) % 11 > 1 and math.abs(x - cx) > 4 then put(x, GROUND, tz, x, GROUND, tz, SANDBAG) end
        end
    end
    for _, gx in { cx - 16, cx + 16 } do                                          -- the gun pits
        for a = 0, 23 do
            local t = a / 24 * 2 * math.pi
            local x, z = gx + math.floor(math.cos(t) * 5 + 0.5), cz + 16 + math.floor(math.sin(t) * 5 + 0.5)
            if a % 6 ~= 0 then put(x, GROUND, z, x, GROUND, z, SANDBAG) end
        end
    end
    put(cx + 42, GROUND, cz + 12, cx + 44, GROUND + 9, cz + 14, CONCRETE)         -- the lookout
    put(cx + 43, GROUND, cz + 13, cx + 43, GROUND + 9, cz + 13, 0)
    put(cx + 41, GROUND + 10, cz + 11, cx + 45, GROUND + 10, cz + 15, CONCRETE)
    put(cx, GROUND + 5, cz, cx, GROUND + 10, cz, WALL)                            -- the flagpole
end

-- ------------------------------------------------------------ 4 the fortress
-- Black walls twelve high round the courtyard and the hall: no shell opens
-- them (KEEP), only the gate (GATE) in the south one is blown in. The hall's
-- door (SEAL) opens when the courtyard is taken (assault_game). Towers at the
-- corners, a gatehouse, braziers of Basoalth's light.
do
    local x0, x1, z0, z1 = 20, 108, 870, 1012
    put(x0, GROUND - 1, z0, x1, GROUND + 11, z1, KEEP)
    put(x0 + 3, GROUND, z0 + 3, x1 - 3, GROUND + 11, 940, 0)                     -- the courtyard
    put(x0 + 3, GROUND - 1, z0 + 3, x1 - 3, GROUND - 1, 940, BASALT)
    for x = x0 + 4, x1 - 4, 6 do put(x, GROUND - 1, z0 + 3, x, GROUND - 1, 940, FOE) end
    for x = x0, x1, 3 do put(x, GROUND + 12, z0, x, GROUND + 12, z0, KEEP) end   -- crenels
    put(x0 + 3, GROUND + 8, z0 + 3, x1 - 3, GROUND + 8, z0 + 4, BASALT)          -- the wall walk
    for i = 0, 7 do put(x0 + 6 + i, GROUND + i, z0 + 5, x0 + 6 + i, GROUND + i, z0 + 5, BASALT) end
    for i = 0, 7 do put(x1 - 6 - i, GROUND + i, z0 + 5, x1 - 6 - i, GROUND + i, z0 + 5, BASALT) end
    -- The gate: six wide, seven high, iron.
    put(MID - 3, GROUND, z0, MID + 3, GROUND + 6, z0 + 2, GATE)
    put(MID - 6, GROUND + 7, z0 - 2, MID + 6, GROUND + 14, z0 + 3, KEEP)         -- the gatehouse over it
    put(MID - 6, GROUND, z0 - 2, MID - 4, GROUND + 6, z0 - 1, KEEP)
    put(MID + 4, GROUND, z0 - 2, MID + 6, GROUND + 6, z0 - 1, KEEP)
    for _, c in { { x0, z0 }, { x1, z0 }, { x0, z1 }, { x1, z1 } } do
        put(c[1] - 3, GROUND - 1, c[2] - 3, c[1] + 3, GROUND + 15, c[2] + 3, KEEP)
        put(c[1], GROUND + 16, c[2], c[1], GROUND + 16, c[2], EMBER)
    end
    for _, bx in { MID - 8, MID + 8 } do put(bx, GROUND + 12, z0 - 1, bx, GROUND + 12, z0 - 1, EMBER) end
    -- The courtyard: a few blocks of cover, the way up to the walk, the flag.
    for _, c in { { -30, 890 }, { 30, 890 }, { -14, 905 }, { 14, 905 }, { -30, 922 }, { 30, 922 }, { 0, 925 } } do
        put(MID + c[1] - 2, GROUND, c[2] - 1, MID + c[1] + 2, GROUND + 1, c[2] + 1, BASALT)
    end
    put(MID, GROUND, 905, MID, GROUND + 7, 905, BASALT)
    -- The hall: its own floor, walls and roof inside the outer ones.
    put(x0 + 6, GROUND - 1, HALL_Z0, x1 - 6, GROUND - 1, HALL_Z1, HALL)
    put(x0 + 6, GROUND, HALL_Z0, x1 - 6, HALL_ROOF - 1, HALL_Z1, 0)
    put(x0, GROUND + 12, HALL_Z0 - 3, x1, HALL_ROOF + 1, z1, KEEP)              -- walls up to the roof
    put(x0 + 6, GROUND + 12, HALL_Z0, x1 - 6, HALL_ROOF - 1, HALL_Z1, 0)
    put(x0 + 3, GROUND - 1, 941, x1 - 3, GROUND + 11, 942, KEEP)                -- between courtyard and hall
    put(MID - 4, GROUND, 941, MID + 4, GROUND + 7, 942, SEAL)                    -- the door
    for x = x0 + 8, x1 - 8, 10 do
        for z = HALL_Z0 + 6, HALL_Z1 - 6, 10 do put(x, HALL_ROOF, z, x, HALL_ROOF, z, EMBER) end
    end
    -- Lamps along its walls, low and high: it is roofed, and embers alone left it dark.
    for x = x0 + 9, x1 - 9, 8 do
        for _, y in { GROUND + 4, GROUND + 14 } do
            put(x, y, HALL_Z0, x, y, HALL_Z0, LAMP)
            put(x, y, HALL_Z1, x, y, HALL_Z1, LAMP)
        end
    end
    for z = HALL_Z0 + 4, HALL_Z1 - 4, 8 do
        for _, y in { GROUND + 4, GROUND + 14 } do
            put(x0 + 6, y, z, x0 + 6, y, z, LAMP)
            put(x1 - 6, y, z, x1 - 6, y, z, LAMP)
        end
    end
    -- The core's ring in the floor, the four pylons' feet, the throne's
    -- balcony on the north wall, the doors the hall's guard comes out of.
    for a = 0, 47 do
        local t = a / 48 * 2 * math.pi
        local x, z = MID + math.floor(math.cos(t) * 9 + 0.5), 976 + math.floor(math.sin(t) * 9 + 0.5)
        put(x, GROUND - 1, z, x, GROUND - 1, z, EMBER)
    end
    for _, c in { { 38, 956 }, { 90, 956 }, { 38, 997 }, { 90, 997 } } do
        put(c[1] - 2, GROUND - 1, c[2] - 2, c[1] + 2, GROUND, c[2] + 2, BASALT)
    end
    put(MID - 8, GROUND + 7, HALL_Z1 - 4, MID + 8, GROUND + 7, HALL_Z1, BASALT)
    put(MID - 2, GROUND + 8, HALL_Z1 - 1, MID + 2, GROUND + 12, HALL_Z1, KEEP)   -- the throne
    for _, sx in { -1, 1 } do
        -- Two doors in each side wall (x 20..25) onto a passage inside it.
        local wx = MID + sx * 39
        put(wx, GROUND, 966, wx + sx, GROUND + 2, 968, 0)
        put(wx, GROUND, 984, wx + sx, GROUND + 2, 986, 0)
        put(wx + sx * 2, GROUND, 960, wx + sx * 3, GROUND + 2, 992, 0)
        put(wx + sx * 2, GROUND - 1, 960, wx + sx * 3, GROUND - 1, 992, HALL)
    end
    -- Cover in the hall: low basalt blocks.
    for _, c in { { -24, 966 }, { 24, 966 }, { -24, 988 }, { 24, 988 }, { -12, 956 }, { 12, 956 }, { -12, 998 }, { 12, 998 } } do
        put(MID + c[1] - 1, GROUND, c[2] - 1, MID + c[1] + 1, GROUND + 1, c[2] + 1, BASALT)
    end
    -- The garage behind the north wall, three bays; its door into the hall
    -- stays shut (KEEP) until Basoalth breaks it (assault_game).
    put(46, GROUND - 1, 1012, 82, GROUND + 6, 1032, KEEP)
    put(47, GROUND, 1013, 81, GROUND + 5, 1031, 0)
    put(47, GROUND - 1, 1013, 81, GROUND - 1, 1031, CONCRETE)
    for _, bx in { 54, 74 } do put(bx, GROUND, 1018, bx, GROUND + 5, 1031, BASALT) end
    put(MID - 4, GROUND, 1032, MID + 4, GROUND + 4, 1032, 0)                       -- out to the road
    for _, bx in { 50, MID, 78 } do put(bx, GROUND + 5, 1022, bx, GROUND + 5, 1022, LAMP) end
end

-- The bonus grounds (assault_game's A.ZONES): a clearing on each side of
-- every stronghold, a beacon of basalt with the legion's cloth over a lamp;
-- held by the allies they add to the credits coming in.
for _, fz in { 250, 480, 710, 905 } do
    for _, zx in { 10, 118 } do
        put(zx - 7, GROUND - 1, fz - 7, zx + 7, GROUND - 1, fz + 7, CONCRETE)
        put(zx - 7, GROUND, fz - 7, zx + 7, GROUND + 12, fz + 7, 0)
        for a = 0, 3 do
            local cx, cz = zx + (if a % 2 == 0 then -6 else 6), fz + (if a < 2 then -6 else 6)
            put(cx, GROUND, cz, cx, GROUND, cz, SANDBAG)
        end
        put(zx, GROUND, fz, zx, GROUND + 2, fz, BASALT)
        put(zx, GROUND + 3, fz, zx, GROUND + 3, fz, LAMP)
        put(zx - 1, GROUND + 4, fz, zx + 1, GROUND + 5, fz, FOE)
    end
end

-- The legion's cloth on every stronghold's pole (assault_game turns it blue).
for _, p in { { MID, 250, GROUND + 4 }, { MID, 483, GROUND + 11 }, { MID, 710, GROUND + 8 }, { MID, 905, GROUND + 5 } } do
    put(p[1] + 1, p[3], p[2], p[1] + 2, p[3] + 2, p[2], FOE)
end

-- ===================================================================== the escape tunnel
-- When the core breaks, Basoalth rises a giant, the hall's floor falls in and
-- the escape runs through a tunnel big enough for him to walk in after the
-- jeeps (24 wide, 44 high, rock round it): north under the garage, east under
-- the ridge, then south down the chasm in turns right and left, climbing and
-- going down -- a block every eight nodes or more, which a jeep takes --
-- up to the rock at the far end with the way out. TUBE is its middle line,
-- each corner x, z and the height a body stands at there (assault_game's
-- A.PATH, kept in step). What lies under the hall is laid only when the floor
-- falls in (COLLAPSE, "assault:collapse"); the rest is the dimension's
-- (TRACK_BOXES), laid again before every try ("assault:track").
local TUBE = { { 64, 976, 2 }, { 64, 1044, 2 }, { 104, 1044, 4 }, { 104, 1068, 4 }, { 172, 1068, 0 },
               { 172, 960, 8 }, { 156, 960, 8 }, { 156, 850, -2 }, { 188, 850, -2 }, { 188, 720, 10 },
               { 156, 720, 10 }, { 156, 600, -2 }, { 188, 600, -2 }, { 188, 470, 10 }, { 160, 470, 10 },
               { 160, 340, -2 }, { 184, 340, -2 }, { 184, 200, 10 }, { 172, 200, 10 }, { 172, 20, 20 } }
local IN, SHELL, HIGH = 12, 14, 44   -- half wide inside, half wide with its rock, high inside
local LANE = 6                       -- the road's half width; magma either side of it, two down
local MOUTH_Z = 34                   -- south of here the last stretch is open, over the rock
local HALL_N  = 1013                 -- north of the hall's wall: the tunnel's from the start
local REFLOOR: { { number } } = {}   -- its floor alone, for "assault:track"
do
    local shells, insides, floors = {}, {}, {}
    for i = 1, #TUBE - 1 do
        local a, b = TUBE[i], TUBE[i + 1]
        local alongx = a[2] == b[2]
        local len = if alongx then math.abs(b[1] - a[1]) else math.abs(b[2] - a[2])
        local dir = if alongx then (if b[1] > a[1] then 1 else -1) else (if b[2] > a[2] then 1 else -1)
        -- Four nodes a slice, each at its own height; the corners' slices run
        -- on past the corner to meet the next stretch.
        for k = 0, len - 1, 4 do
            -- Level for IN either side of a corner, where the next stretch's
            -- end runs over this one's; the climb or the drop between.
            local f0 = if i > 1 then IN else 0
            local f1 = if i < #TUBE - 1 then IN else 0
            local u = math.clamp((k + 2 - f0) / math.max(1, len - f0 - f1), 0, 1)
            local y = a[3] + math.floor((b[3] - a[3]) * u + 0.5)
            local last = k + 4 >= len
            local base = if alongx then a[1] else a[2]
            -- How far past each end it runs: to the next stretch's far wall
            -- at a corner, its inside to that wall's inside.
            local function span(ext: number): (number, number)
                local lo, hi = k, if last then len else k + 3
                if k == 0 and i > 1 then lo = -ext end
                if last and i < #TUBE - 1 then hi = len + ext end
                local c0, c1 = base + dir * lo, base + dir * hi
                -- Under the hall: its own, laid when the floor falls in.
                if i == 1 then
                    if dir > 0 then c0 = math.max(c0, HALL_N) else c1 = math.max(c1, HALL_N) end
                end
                return c0, c1
            end
            if i == 1 and base + dir * (if last then len else k + 3) < HALL_N then continue end
            local function add(list, w: number, ext: number, y0: number, y1: number, block: number)
                local c0, c1 = span(ext)
                if alongx then
                    table.insert(list, { math.min(c0, c1), y0, a[2] - w, math.max(c0, c1), y1, a[2] + w, block })
                else
                    table.insert(list, { a[1] - w, y0, math.min(c0, c1), a[1] + w, y1, math.max(c0, c1), block })
                end
            end
            local c0, c1 = span(IN)
            local open = not alongx and math.max(c0, c1) <= MOUTH_Z
            if not open then
                add(shells, SHELL, SHELL, y - 4, y + HIGH + 2, RIDGE)
                add(insides, IN, IN, y, y + HIGH - 1, 0)
            else
                add(insides, IN, IN, y, y + 8, 0)
            end
            -- The road in the middle; either side of it, down to the walls, a
            -- channel of magma two under it -- the tunnel drains the
            -- dimension's fire. What the giant breaks (three deep) is laid
            -- again before every try: REFLOOR.
            for _, list in { floors, REFLOOR } do
                add(list, IN, IN, y - 3, y - 3, MAGMA)
                add(list, IN, IN, y - 2, y - 1, 0)
                add(list, LANE, IN, y - 3, y - 2, RIDGE)
                add(list, LANE, IN, y - 1, y - 1, TRACK)
            end
            -- The edges in the hazard's stripes; lamps down both walls, and
            -- Basoalth's light higher up, or nothing would be seen down here.
            local at = base + dir * k
            local function dot(off: number, yy: number, block: number)
                if alongx then
                    table.insert(floors, { at, yy, a[2] + off, at, yy, a[2] + off, block })
                else
                    table.insert(floors, { a[1] + off, yy, at, a[1] + off, yy, at, block })
                end
            end
            -- Not near a corner: there this stretch's wall is the next one's inside.
            local walled = (i == 1 or k >= IN + 2) and (i == #TUBE - 1 or k <= len - IN - 2)
            for _, sd in { -1, 1 } do
                dot(sd * LANE, y - 1, HAZARD)
                if not open and walled then
                    if k % 8 == 0 then dot(sd * (IN + 1), y + 4, LAMP) end
                    if k % 16 == 0 then dot(sd * (IN + 1), y + 22, EMBER) end
                end
            end
        end
    end
    -- Every corner road from wall to wall: a jeep cutting it stays out of
    -- the magma.
    for i = 2, #TUBE - 1 do
        local v = TUBE[i]
        for _, list in { floors, REFLOOR } do
            box(list, v[1] - IN, v[3] - 3, v[2] - IN, v[1] + IN, v[3] - 2, v[2] + IN, RIDGE)
            box(list, v[1] - IN, v[3] - 1, v[2] - IN, v[1] + IN, v[3] - 1, v[2] + IN, TRACK)
        end
    end
    for _, list in { shells, insides, floors } do
        for _, bx in list do table.insert(TRACK_BOXES, bx) end
    end
    -- The end: a rock in the chasm, the frame round opening 1 facing +x.
    box(REFLOOR, 158, GROUND - 1, 4, 186, GROUND - 1, MOUTH_Z + 2, TRACK)
    road(158, GROUND - 4, 4, 186, GROUND - 1, MOUTH_Z + 2, RIDGE)
    road(158, GROUND - 1, 4, 186, GROUND - 1, MOUTH_Z + 2, TRACK)
    road(163, GROUND, 10, 163, GROUND + 3, 15, FRAME)
    road(163, GROUND, 12, 163, GROUND + 1, 12, 0)
    road(164, GROUND - 1, 10, 166, GROUND - 1, 15, FRAME)
end

-- The hall's floor fallen in: a pit down to the tunnel's floor, heaps of what
-- fell, and the north wall gone where the tunnel goes on.
local COLLAPSE: { { number } } = {}
do
    local fy = TUBE[1][3]
    box(COLLAPSE, 28, fy, 945, 100, GROUND - 1, 1007, 0)
    box(COLLAPSE, 28, fy - 1, 945, 100, fy - 1, 1007, TRACK)
    box(COLLAPSE, MID - IN, fy, 1007, MID + IN, fy + HIGH - 1, HALL_N, 0)
    box(COLLAPSE, MID - IN, fy - 1, 1007, MID + IN, fy - 1, HALL_N, TRACK)
    for _, c in { { 38, 953 }, { 90, 953 }, { 36, 999 }, { 92, 999 }, { 52, 950 }, { 78, 1002 } } do
        for k = 0, 3 do
            box(COLLAPSE, c[1] - 3 + k, fy + k, c[2] - 3 + k, c[1] + 3 - k, fy + k, c[2] + 3 - k, BASALT)
        end
    end
end

-- ===================================================================== the ground

-- Hills where nothing stands: a sum of waves faded out near the camp, the
-- strongholds and the road between them, as the front's.
local function flat_weight(x: number, z: number): number
    local near = math.min(math.abs(x - MID) - 5, math.abs(z - CAMP[2]) - 22)
    for _, f in FORTS do near = math.min(near, math.max(math.abs(x - f[1]) - 34, math.abs(z - f[2]) - 30)) end
    if z > 865 then near = -1 end                                    -- the fortress and beyond: flat
    return math.clamp(near / 16, 0, 1)
end

local function top(x: number, z: number): number
    local h = math.sin(x * 0.083) * 2.0 + math.cos(z * 0.047) * 2.2 + math.sin((x - z) * 0.029) * 2.5
    return GROUND - 1 + math.floor(math.max(0, h + 2.4) * flat_weight(x, z))
end

-- ------------------------------------------------------------ the lands
-- Round each stronghold its own land, the edges between them wavering:
--   1 the camp's        grass
--   2 the outpost's     a dead marsh: mud, pools of bog, reeds, dead trees
--   3 the bastion's     a burnt forest: ash, charred trunks, cinders still glowing
--   4 the battery's     a crystal waste: violet shards, spikes of glowing crystal
--   5 the fortress's    basalt cracked with magma, ribs of bone
-- Nothing stands on the road, in a stronghold or at the camp (flat_weight).
local EDGES = { 140, 360, 590, 810 }
local GROUNDS = { GRASS, MUD, ASH, SHARD, BASALT }

local function hash(x: number, z: number): number
    local v = math.sin(x * 12.9898 + z * 78.233) * 43758.5453
    return v - math.floor(v)
end

local function biome(x: number, z: number): number
    local zz = z + math.sin(x * 0.07) * 9 + math.sin(z * 0.05 + x * 0.02) * 6
    local b = 1
    for i, e in EDGES do
        if zz >= e then b = i + 1 end
    end
    return b
end

-- Where something may stand: off the road and out of the strongholds.
local function wild(x: number, z: number): boolean
    if z > 865 then return x < 17 or x > 111 end
    return flat_weight(x, z) > 0.3
end

-- The trunk (or spike) of the 8x8 cell x, z is in: its column, its height,
-- and which way its one arm goes (1..4), or nil when the cell has none.
local ARM = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }
local function trunk_of(cx: number, cz: number): (number?, number?, number, number)
    local h = hash(cx * 3.1, cz * 7.7)
    local tx, tz = cx * 8 + 1 + math.floor(hash(cx, cz) * 6), cz * 8 + 1 + math.floor(hash(cz, cx) * 6)
    local b = biome(tx, tz)
    local dense = ({ 0, 0.55, 0.7, 0.45, 0.25 })[b]
    if h > dense or not wild(tx, tz) then return nil, nil, 0, 0 end
    return tx, tz, 4 + math.floor(hash(tx, tz) * 5), 1 + math.floor(h * 97) % 4
end

-- What stands on column x, z over its top t.
local function dress(x: number, z: number, t: number, b: number, put: (number, number) -> ())
    if b == 1 or not wild(x, z) then return end
    -- The trunk standing here, or an arm of one beside.
    local cx, cz = x // 8, z // 8
    local tx, tz, h, _ = trunk_of(cx, cz)
    if tx == x and tz == z then
        local stuff = ({ 0, DEADWOOD, CHAR, CRYSTAL, BONE })[b]
        for y = t + 1, t + h do put(y, stuff) end
        return
    end
    for _, n in ARM do
        local nx, nz = x - n[1], z - n[2]
        local ox, oz, oh, oarm = trunk_of(nx // 8, nz // 8)
        if ox == nx and oz == nz and ARM[oarm][1] == n[1] and ARM[oarm][2] == n[2] then
            if b == 2 or b == 3 then
                put(t + oh - 1, if b == 2 then DEADWOOD else CHAR)
            elseif b == 4 then
                for y = t + 1, t + oh // 2 do put(y, CRYSTAL) end
            elseif b == 5 then
                for y = t + 1, t + 2 do put(y, BONE) end
            end
            return
        end
    end
    local r = hash(x, z)
    if b == 2 then
        if r < 0.07 then
            put(t + 1, REED)
            if r < 0.03 then put(t + 2, REED) end
        end
    elseif b == 3 and r < 0.012 then
        put(t, CINDER)
    elseif b == 4 and r < 0.02 then
        put(t + 1, CRYSTAL)
    end
end

-- The top block of column x, z in land b: a pool of bog in the marsh, magma
-- in the cracks of the basalt.
local function ground(x: number, z: number, b: number): number
    if b == 2 and math.sin(x * 0.19 + math.sin(z * 0.11) * 3) * math.cos(z * 0.17) > 0.55 then return BOG end
    if b == 5 and math.abs(math.sin(x * 0.21 + math.sin(z * 0.13) * 2.5)) < 0.05 and wild(x, z) then return MAGMA end
    return GROUNDS[b]
end

-- Inside the valley, the ridge round it, the chasm east of it.
local function kind(x: number, z: number): string
    if x >= 0 and x < W and z >= 0 and z < L then return "valley" end
    if x >= 136 and x <= 207 and z >= -8 and z <= L + 7 then return "chasm" end
    return "ridge"
end

-- ------------------------------------------------------------ the river
-- A river of lava across the valley between the bastion and the battery,
-- wavering: out of the western mountains down a gorge, over the edge into
-- the chasm at the east. Three under the ground, banks of basalt; a bridge
-- on the road in the middle, and either side a line of islets to jump.
local RIVER_HALF = 6
local function river_z(x: number): number
    return math.floor(600 + math.sin(x * 0.05) * 6 + math.sin(x * 0.013 + 1) * 4 + 0.5)
end
do
    local function at(x: number): number return river_z(x) end
    local z = at(MID)
    put(MID - 4, GROUND - 1, z - RIVER_HALF - 3, MID + 4, GROUND - 1, z + RIVER_HALF + 3, CONCRETE)
    put(MID - 4, GROUND - 2, z - RIVER_HALF - 3, MID + 4, GROUND - 2, z + RIVER_HALF + 3, WALL)
    for _, sx in { -5, 5 } do
        put(MID + sx, GROUND, z - RIVER_HALF - 1, MID + sx, GROUND, z + RIVER_HALF + 1, WALL)
        put(MID + sx, GROUND + 1, z - RIVER_HALF - 1, MID + sx, GROUND + 1, z - RIVER_HALF - 1, LAMP)
        put(MID + sx, GROUND + 1, z + RIVER_HALF + 1, MID + sx, GROUND + 1, z + RIVER_HALF + 1, LAMP)
    end
    -- The islets: two by two, their tops level with the ground, a jump of
    -- one or two between them.
    for _, x in { 22, 104 } do
        local c = at(x)
        for _, dz in { -5, -1, 3 } do
            put(x, GROUND - 4, c + dz, x + 1, GROUND - 1, c + dz + 1, BASALT)
        end
    end
end

-- ------------------------------------------------------------ the mountains
-- Round the valley and the chasm a range of mountains, higher the further
-- out, out to REACH; and in it volcanoes -- a cone, a crater full of magma,
-- and magma running down its sides towards the valley, wavering.
local REACH = 110
local VOLCANOES = {   -- x, z, the foot's radius, the peak over the ground
    { -45, 160, 55, 70 }, { -55, 470, 65, 86 }, { -40, 760, 55, 72 }, { -50, 1010, 60, 80 },
    { 64, -62, 52, 66 }, { 70, 1150, 55, 78 },
    { 268, 300, 55, 76 }, { 272, 640, 65, 90 }, { 262, 960, 55, 74 },
}
local CRATER = 7

-- How far x, z is from the valley or the chasm, whichever is nearer.
local function outside(x: number, z: number): number
    local function rect(x0: number, x1: number, z0: number, z1: number): number
        local dx = math.max(x0 - x, 0, x - x1)
        local dz = math.max(z0 - z, 0, z - z1)
        return math.sqrt(dx * dx + dz * dz)
    end
    return math.min(rect(0, W - 1, 0, L - 1), rect(136, 207, -8, L + 7))
end

-- The mountains' top at x, z, and whether it is magma.
local function mountain(x: number, z: number): (number, boolean)
    local d = outside(x, z)
    local n = math.sin(x * 0.11 + math.sin(z * 0.07) * 2) * 4 + math.sin(z * 0.09 + x * 0.03) * 4
    local h = GROUND + 14 + math.min(d * 1.1, 34) + n * math.min(1, d / 10)
    local lava = false
    for _, v in VOLCANOES do
        local dx, dz = x - v[1], z - v[2]
        local r = math.sqrt(dx * dx + dz * dz)
        if r >= v[3] then continue end
        local peak = GROUND + v[4]
        if r < CRATER then
            -- The crater: a lake of magma a little under the rim.
            if peak - 4 > h then h, lava = peak - 4, true end
        else
            local cone = peak - (peak - GROUND) * (r - CRATER) / (v[3] - CRATER)
            if cone > h then
                h, lava = cone, false
                -- Three runs of it down the side that looks at the valley.
                local toward = math.atan2(MID - v[1], L / 2 - v[2])
                local a = math.atan2(dx, dz)
                for k = -1, 1 do
                    local want = toward + k * 0.8 + math.sin(r * 0.12 + k * 2) * 0.2
                    local off = math.abs((a - want + math.pi) % (2 * math.pi) - math.pi)
                    if off * r < 1.3 + r * 0.03 then lava = true end
                end
            end
        end
    end
    return math.floor(h), lava
end

local function apply(list: { { number } }, ctx)
    for _, b in list do
        if b[4] < ctx.min_x or b[1] > ctx.max_x or b[6] < ctx.min_z or b[3] > ctx.max_z then continue end
        for x = math.max(b[1], ctx.min_x), math.min(b[4], ctx.max_x) do
            for z = math.max(b[3], ctx.min_z), math.min(b[6], ctx.max_z) do
                for y = b[2], b[5] do core.set_block(x, y, z, b[7]) end
            end
        end
    end
end

-- Column x, z as the dimension makes it, each cell through put(y, block):
-- the ground and what stands on it, before the buildings' boxes.
local function column(x: number, z: number, put: (number, number) -> ())
    put(GROUND - 32, BEDROCK)
    local k = kind(x, z)
    if k == "chasm" then
        for y = GROUND - 31, GROUND - 30 do put(y, MAGMA) end
    elseif k == "ridge" then
        local t, lava = mountain(x, z)
        -- The river's gorge: down out of the western mountains, and through
        -- the rock between the valley and the chasm.
        local gorge = math.abs(z - river_z(x)) <= RIVER_HALF and (x < 0 or (x >= W and x < 136))
        local ly = GROUND - 3 + (if x < 0 then math.floor(-x * 0.6) else 0)
        if gorge and ly < t then
            for y = GROUND - 31, ly - 1 do put(y, BASALT) end
            put(ly, LAVA)
        else
            for y = GROUND - 31, t - 1 do put(y, RIDGE) end
            put(t, if lava then MAGMA elseif t > GROUND + 40 then BASALT else RIDGE)
        end
    elseif math.abs(z - river_z(x)) <= RIVER_HALF + 2 then
        for y = GROUND - 31, GROUND - 9 do put(y, STONE) end
        put(GROUND - 8, BEDROCK)
        if math.abs(z - river_z(x)) <= RIVER_HALF then
            for y = GROUND - 7, GROUND - 4 do put(y, BASALT) end
            put(GROUND - 3, LAVA)
        else
            for y = GROUND - 7, GROUND - 1 do put(y, BASALT) end   -- its banks
        end
    else
        local t = top(x, z)
        for y = GROUND - 31, GROUND - 9 do put(y, STONE) end
        put(GROUND - 8, BEDROCK)
        for y = GROUND - 7, t - 3 do put(y, STONE) end
        for y = t - 2, t - 1 do put(y, DIRT) end
        local onroad = math.abs(x - MID) <= 2 and z > 38 and z < 870
        local b = biome(x, z)
        put(t, if onroad then ROAD else ground(x, z, b))
        if not onroad then dress(x, z, t, b, put) end
    end
end


-- ------------------------------------------------------------ the barracks
-- The allies' barracks, one by each base (assault_game's A.BASE, the camp
-- first), west of it: planks round a room five by three, a door east onto
-- the base, a concrete roof with the allies' colour. Not there until
-- assault_game raises it ("assault:barracks", k; 0 takes them all back to
-- the camp's alone) as the base becomes the allies', and from then on the
-- dimension as it is made: the mapgen and the builders' mending lay it.
local BASES = { CAMP, { MID, 276 }, { MID, 506 }, { MID, 742 }, { MID, 915 } }
local BARRACKS: { { { number } } } = {}
for k, b in BASES do
    local list = {}
    local x0, x1, z0, z1 = b[1] - 24, b[1] - 18, b[2] - 3, b[2] + 1
    box(list, x0, GROUND - 3, z0, x1, GROUND - 1, z1, CONCRETE)        -- its footing
    box(list, x0, GROUND, z0, x1, GROUND + 7, z1, 0)                   -- room for it
    box(list, x0, GROUND, z0, x1, GROUND + 2, z1, PLANKS)
    box(list, x0 + 1, GROUND, z0 + 1, x1 - 1, GROUND + 2, z1 - 1, 0)
    box(list, x1, GROUND, b[2] - 1, x1, GROUND + 1, b[2] - 1, 0)       -- the door
    box(list, x0, GROUND + 3, z0, x1, GROUND + 3, z1, CONCRETE)
    box(list, x0, GROUND + 3, b[2] - 1, x1, GROUND + 3, b[2] - 1, ALLY)
    BARRACKS[k] = list
end
local raised: { [number]: boolean } = { [1] = true }
local collapsed = false   -- the hall's floor fallen in (COLLAPSE, "assault:collapse")
local EVERYWHERE = { min_x = -1e9, max_x = 1e9, min_z = -1e9, max_z = 1e9 }

core.register_mapgen("assault_core:dimension", function(ctx)
    if ctx.max_x < -REACH or ctx.min_x > 207 + REACH or ctx.max_z < -REACH or ctx.min_z > L + REACH then return end
    for x = math.max(-REACH, ctx.min_x), math.min(207 + REACH, ctx.max_x) do
        for z = math.max(-REACH, ctx.min_z), math.min(L + REACH, ctx.max_z) do
            column(x, z, function(y, id) core.set_block(x, y, z, id) end)
        end
    end
    apply(BOXES, ctx)
    apply(TRACK_BOXES, ctx)
    for k in raised do apply(BARRACKS[k], ctx) end
    if collapsed then apply(COLLAPSE, ctx) end
end)

-- The hall's floor falling in (assault_game, as Basoalth rises): on, laid
-- and made again so by the mapgen; off, the hall as it was -- every column
-- under and round it put back as the dimension makes it.
core.event.define("assault:collapse", { fields = { on = "boolean" }, cancellable = false })
core.event.on("assault:collapse", function(ev)
    collapsed = ev.on
    if ev.on then
        apply(COLLAPSE, EVERYWHERE)
        return
    end
    for x = 26, 102 do
        for z = 943, HALL_N do
            local col: { [number]: number } = {}
            column(x, z, function(y, id) col[y] = id end)
            for _, list in { BOXES, TRACK_BOXES } do
                for _, b in list do
                    if x >= b[1] and x <= b[4] and z >= b[3] and z <= b[6] then
                        for y = b[2], b[5] do col[y] = b[7] end
                    end
                end
            end
            for y = TUBE[1][3] - 1, TUBE[1][3] + HIGH do
                local want = col[y] or 0
                if core.get_block(x, y, z) ~= want then core.set_block(x, y, z, want) end
            end
        end
    end
end)

core.event.define("assault:barracks", { fields = { k = "number" }, cancellable = false })
core.event.on("assault:barracks", function(ev)
    if ev.k == 0 then
        raised = { [1] = true }
    elseif BARRACKS[ev.k] then
        raised[ev.k] = true
        apply(BARRACKS[ev.k], EVERYWHERE)
    end
end)

-- A builder's work (assault_game): every cell within r of x, y, z put back
-- as the dimension made it -- the ground, what stood on it, the buildings --
-- but the fortress's gate and the hall's door, which stay as the fight left
-- them. Asked with core.event.emit("assault:repair", { x, y, z, r }).
core.event.define("assault:repair", { fields = { x = "number", y = "number", z = "number", r = "number" }, cancellable = false })
core.event.on("assault:repair", function(ev)
    local R = math.ceil(ev.r)
    local bx, by, bz = math.floor(ev.x), math.floor(ev.y), math.floor(ev.z)
    for x = bx - R, bx + R do
        for z = bz - R, bz + R do
            if (x - bx) ^ 2 + (z - bz) ^ 2 > ev.r * ev.r then continue end
            local col: { [number]: number } = {}
            column(x, z, function(y, id) col[y] = id end)
            local lists = { BOXES, TRACK_BOXES }
            for k in raised do table.insert(lists, BARRACKS[k]) end
            for _, list in lists do
                for _, b in list do
                    if x >= b[1] and x <= b[4] and z >= b[3] and z <= b[6] then
                        for y = b[2], b[5] do col[y] = b[7] end
                    end
                end
            end
            for y = by - R, by + R do
                if (x - bx) ^ 2 + (y - by) ^ 2 + (z - bz) ^ 2 > ev.r * ev.r then continue end
                local want = col[y] or 0
                if want == GATE or want == SEAL then continue end
                if core.get_block(x, y, z) ~= want then core.set_block(x, y, z, want) end
            end
        end
    end
end)

-- The escape road again as it was, asked by assault_game before every try:
-- every cell its boxes cover, over what the giant broke. Spread over ticks.
core.event.define("assault:track", { fields = {}, cancellable = false })
local relay = 0   -- the next box to lay, 0 for none
core.event.on("assault:track", function(_)
    relay = 1
end)
core.event.on("core:tick", function(_)
    if relay == 0 then return end
    local cells = 0
    while relay <= #REFLOOR and cells < 6000 do
        local b = REFLOOR[relay]
        for x = b[1], b[4] do
            for z = b[3], b[6] do
                for y = b[2], b[5] do core.set_block(x, y, z, b[7]) end
            end
        end
        cells += (b[4] - b[1] + 1) * (b[6] - b[3] + 1) * (b[5] - b[2] + 1)
        relay += 1
    end
    if relay > #REFLOOR then relay = 0 end
end)

