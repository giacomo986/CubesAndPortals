--!strict
-- front_core: the world of the front, a battlefield in the manner of large
-- team shooters. Two bases at the ends of a field 256 nodes a side, three
-- points to take across its middle -- a farm, a village, a bunker on a hill --
-- walls and sandbags between them, and hills to hide behind. What happens on
-- it (teams, guns, bots, vehicles) is front_game's.
--
-- Nothing of the engine's terrain: the field is all one stage, so its hills
-- are where the bases and the points can count on flat ground. Everything but
-- BEDROCK and the RIDGE round the edge can be blown up (front_game's TOUGH).

local GRASS    = core.register_node("front_core:grass", 96, 132, 64)
local DIRT     = core.register_node("front_core:dirt", 120, 88, 60)
local STONE    = core.register_node("front_core:stone", 118, 118, 116)
local BEDROCK  = core.register_node("front_core:bedrock", 40, 40, 44)
local RIDGE    = core.register_node("front_core:ridge", 88, 84, 80)
local ROAD     = core.register_node("front_core:road", 140, 130, 112)
local SANDBAG  = core.register_node("front_core:sandbag", 186, 164, 112)
local BRICK    = core.register_node("front_core:brick", 150, 74, 56)
local WALL     = core.register_node("front_core:wall", 132, 128, 120)
local CONCRETE = core.register_node("front_core:concrete", 160, 160, 156)
local PLANKS   = core.register_node("front_core:planks", 150, 112, 70)
local ROOF     = core.register_node("front_core:roof", 110, 48, 40)
local RED      = core.register_node("front_core:red", 190, 40, 36)
local BLUE     = core.register_node("front_core:blue", 40, 70, 190)
local LAMP     = core.register_node("front_core:lamp", 250, 230, 180, true, 14)
-- The gallery's (below) and its portals' frames: front_game's TOUGH, so no
-- shell opens the gallery or the back of a portal.
local GALLERY  = core.register_node("front_core:gallery", 120, 122, 126)
local PANE     = core.register_node("front_core:pane", 190, 215, 230)
local FRAME_R  = core.register_node("front_core:frame_red", 170, 30, 28)
local FRAME_B  = core.register_node("front_core:frame_blue", 30, 56, 170)
core.node_glass(PANE, 60)

-- What a player is here: the sandbox's robot (mods/robot, linked).
core.player_model(core.register_model("front_core:robot", "robot.gltf"))
for _, extra in {"cap", "tophat", "headphones", "scarf", "bowtie", "medal",
                  "human", "human_arms", "human_short", "human_long", "human_beard",
                  "mole", "mole_arms", "mole_lamp", "mole_goggles", "mole_glasses",
                  "frog", "frog_arms", "frog_spots", "frog_crown", "frog_lily",
                  "human_cap", "human_tophat", "human_headphones", "human_scarf", "human_bowtie", "human_medal",
                  "mole_cap", "mole_tophat", "mole_headphones", "mole_scarf", "mole_bowtie", "mole_medal",
                  "frog_cap", "frog_tophat", "frog_headphones", "frog_scarf", "frog_bowtie", "frog_medal"} do
    core.register_model("front_core:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
core.register_model("front_core:robot_arms", "robot_arms.gltf")
core.eye_height(1.5)

-- The sandbox's walk, a little more sprint; no flying but in a machine.
core.movement(4.8, 8.5, 0, 8, -25)
core.mapgen_core(false)

local GROUND = 20    -- the plaza's height: flat ground is air from here up
local SIZE   = 256   -- the field, 0..SIZE-1 on x and z
local MID    = 128

-- The places front_game has to know too: the bases' middles and the points.
-- Kept in step with front_game's own copy.
local RED_BASE  = { MID, 24 }
local BLUE_BASE = { MID, SIZE - 24 }
local POINTS    = { { 48, MID }, { MID, MID }, { 208, MID } }

-- The gallery: a room on the west ridge, halfway between the bases and 16
-- over the field, glazed along the field's side to watch the fight from.
-- The way in from the sandbox comes out in it (opening 0, facing +z, at its
-- south end), and two portals in its back wall lead to the bases: through the
-- red one a player fights for red, through the blue one for blue (front_game).
-- Its floor is air from FLOOR up; inside x -8..1 and z MID-10..MID+10.
local FLOOR = GROUND + 16
core.set_portal(0, -2.5, FLOOR + 1, MID - 9.5)

-- Each side's pair: the gallery's end on its back wall facing +x, the base's
-- on the back wall of its bunker facing the bunker's door, out of the way of
-- the machines parked in the yard. Both 2 wide and 2.5 tall.
local DOORS = {
    { gallery = { -8, FLOOR + 1.25, MID - 5 }, base = { RED_BASE[1], GROUND + 1.25, RED_BASE[2] - 12 }, sign = 1 },
    { gallery = { -8, FLOOR + 1.25, MID + 5 }, base = { BLUE_BASE[1], GROUND + 1.25, BLUE_BASE[2] + 13 }, sign = -1 },
}

-- Flat round the places a battle needs flat, hills elsewhere: a sum of waves
-- (no clock, no random: every machine gets the same field) faded to nothing
-- near the bases, the points and the roads between.
local function flat_weight(x: number, z: number): number
    local near = 1e9
    for _, p in { RED_BASE, BLUE_BASE, POINTS[1], POINTS[2], POINTS[3] } do
        near = math.min(near, math.max(math.abs(x - p[1]), math.abs(z - p[2])) - 22)
    end
    near = math.min(near, math.abs(x - MID) - 4)           -- the road north and south
    near = math.min(near, math.abs(z - MID) - 4)           -- and east and west
    return math.clamp(near / 16, 0, 1)
end

local function hill(x: number, z: number): number
    local h = math.sin(x * 0.071) * 2.2 + math.cos(z * 0.053) * 2.0 + math.sin((x + z) * 0.031) * 2.5
    -- The bunker's hill: point C stands four up on a mound.
    local dc = math.sqrt((x - POINTS[3][1]) ^ 2 + (z - POINTS[3][2]) ^ 2)
    local mound = math.max(0, 4 - math.max(0, dc - 14) * 0.25)
    return math.floor(math.max(0, h + 2.5) * flat_weight(x, z) + mound)
end

-- The ground's top cell (the grass) at x, z.
local function top(x: number, z: number): number
    return GROUND - 1 + hill(x, z)
end

core.register_mapgen("front_core:field", function(ctx)
    if ctx.max_x < -8 or ctx.min_x > SIZE + 7 or ctx.max_z < -8 or ctx.min_z > SIZE + 7 then
        return
    end
    local function set(x: number, y: number, z: number, id: number)
        if x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z then
            core.set_block(x, y, z, id)
        end
    end
    local function box(ax: number, bx: number, ay: number, by: number, az: number, bz: number, id: number)
        for x = math.max(ax, ctx.min_x), math.min(bx, ctx.max_x) do
            for z = math.max(az, ctx.min_z), math.min(bz, ctx.max_z) do
                for y = ay, by do
                    core.set_block(x, y, z, id)
                end
            end
        end
    end

    -- ------------------------------------------------------------ ground
    for x = math.max(-8, ctx.min_x), math.min(SIZE + 7, ctx.max_x) do
        for z = math.max(-8, ctx.min_z), math.min(SIZE + 7, ctx.max_z) do
            core.set_block(x, GROUND - 8, z, BEDROCK)
            if x < 0 or z < 0 or x >= SIZE or z >= SIZE then
                -- The ridge round the field: too steep to climb out.
                for y = GROUND - 7, GROUND + 14 do
                    core.set_block(x, y, z, RIDGE)
                end
            else
                local t = top(x, z)
                for y = GROUND - 7, t - 3 do
                    core.set_block(x, y, z, STONE)
                end
                for y = t - 2, t - 1 do
                    core.set_block(x, y, z, DIRT)
                end
                local road = (math.abs(x - MID) <= 2 and z > 30 and z < SIZE - 30)
                    or (math.abs(z - MID) <= 2 and x > 30 and x < SIZE - 30)
                core.set_block(x, t, z, if road then ROAD else GRASS)
            end
        end
    end

    -- ------------------------------------------------------------ bases
    -- A concrete yard with a sandbag ring, a bunker at the back and a lamp
    -- on each corner; the team's colour on its flagpole and in its floor.
    for _, b in { { RED_BASE, RED, 1 }, { BLUE_BASE, BLUE, -1 } } do
        local cx, cz, colour, f = b[1][1], b[1][2], b[2], b[3]
        box(cx - 20, cx + 20, GROUND - 1, GROUND - 1, cz - 14, cz + 14, CONCRETE)
        for i = -20, 20, 5 do
            box(cx + i, cx + i, GROUND - 1, GROUND - 1, cz - 14, cz + 14, colour)
        end
        for x = cx - 20, cx + 20 do
            for _, z in { cz - 14, cz + 14 } do
                if math.abs(x - cx) > 4 then box(x, x, GROUND, GROUND, z, z, SANDBAG) end
            end
        end
        for z = cz - 14, cz + 14 do
            for _, x in { cx - 20, cx + 20 } do
                box(x, x, GROUND, GROUND, z, z, SANDBAG)
            end
        end
        -- The bunker: concrete, open towards the field.
        local back = cz - 11 * f
        box(cx - 6, cx + 6, GROUND, GROUND + 3, back - 2, back + 2, CONCRETE)
        box(cx - 5, cx + 5, GROUND, GROUND + 2, back - 1, back + 1, 0)
        box(cx - 2, cx + 2, GROUND, GROUND + 1, back + 2 * f, back + 2 * f, 0)
        for _, c in { { -20, -14 }, { 20, -14 }, { -20, 14 }, { 20, 14 } } do
            box(cx + c[1], cx + c[1], GROUND + 1, GROUND + 1, cz + c[2], cz + c[2], LAMP)
        end
        box(cx, cx, GROUND, GROUND + 6, cz, cz, WALL)
        box(cx + 1, cx + 3, GROUND + 5, GROUND + 6, cz, cz, colour)
    end

    -- ------------------------------------------------------------ point A: the farm
    -- Low stone walls round fields, a barn of planks.
    do
        local px, pz = POINTS[1][1], POINTS[1][2]
        for _, w in { { -18, 18, -18, -18 }, { -18, 18, 18, 18 }, { -18, -18, -18, 18 }, { 18, 18, -18, 18 } } do
            for x = px + w[1], px + w[2] do
                for z = pz + w[3], pz + w[4] do
                    if (x + z) % 9 ~= 0 then box(x, x, GROUND, GROUND + 1, z, z, WALL) end
                end
            end
        end
        box(px - 6, px + 6, GROUND, GROUND + 4, pz + 4, pz + 12, PLANKS)
        box(px - 5, px + 5, GROUND, GROUND + 3, pz + 5, pz + 11, 0)
        box(px - 2, px + 2, GROUND, GROUND + 2, pz + 4, pz + 4, 0)
        box(px - 7, px + 7, GROUND + 5, GROUND + 5, pz + 3, pz + 13, ROOF)
    end

    -- ------------------------------------------------------------ point B: the village
    -- Brick houses either side of the crossroads: doors on the road, windows,
    -- a flat roof to stand on.
    do
        local px, pz = POINTS[2][1], POINTS[2][2]
        local HOUSES = { { -16, -16 }, { -16, 8 }, { 8, -16 }, { 8, 8 }, { -26, -6 }, { 18, -6 } }
        for i, h in HOUSES do
            local x0, z0 = px + h[1], pz + h[2]
            local x1, z1 = x0 + 8, z0 + 7
            local high = if i % 2 == 0 then 7 else 4
            box(x0, x1, GROUND, GROUND + high, z0, z1, BRICK)
            box(x0 + 1, x1 - 1, GROUND, GROUND + high - 1, z0 + 1, z1 - 1, 0)
            if high > 4 then
                box(x0 + 1, x1 - 1, GROUND + 4, GROUND + 4, z0 + 1, z1 - 1, PLANKS)
                box(x0 + 1, x0 + 1, GROUND + 4, GROUND + 4, z0 + 1, z0 + 2, 0)   -- a hole up
            end
            box(x0, x1, GROUND + high + 1, GROUND + high + 1, z0, z1, ROOF)
            -- A door on whichever side faces the crossroads, windows on the rest.
            local dx = if h[1] < 0 then x1 else x0
            box(dx, dx, GROUND, GROUND + 1, z0 + 3, z0 + 4, 0)
            for _, wz in { z0 + 2, z0 + 5 } do
                local wx = if h[1] < 0 then x0 else x1
                box(wx, wx, GROUND + 2, GROUND + 2, wz, wz, 0)
            end
            box(x0 + 2, x0 + 2, GROUND + 2, GROUND + 2, z0, z0, 0)
            box(x0 + 6, x0 + 6, GROUND + 2, GROUND + 2, z1, z1, 0)
        end
        -- The square's well and its lamp.
        box(px + 3, px + 5, GROUND, GROUND, pz + 3, pz + 5, WALL)
        box(px + 4, px + 4, GROUND, GROUND, pz + 4, pz + 4, 0)
        box(px + 3, px + 3, GROUND + 1, GROUND + 3, pz + 3, pz + 3, WALL)
        box(px + 3, px + 3, GROUND + 4, GROUND + 4, pz + 3, pz + 3, LAMP)
    end

    -- ------------------------------------------------------------ point C: the bunker on the hill
    do
        local px, pz = POINTS[3][1], POINTS[3][2]
        local g = top(px, pz) + 1
        box(px - 7, px + 7, g, g + 3, pz - 5, pz + 5, CONCRETE)
        box(px - 6, px + 6, g, g + 2, pz - 4, pz + 4, 0)
        box(px - 5, px + 5, g + 2, g + 2, pz - 5, pz - 5, 0)   -- the slits, both ways
        box(px - 5, px + 5, g + 2, g + 2, pz + 5, pz + 5, 0)
        box(px - 7, px - 7, g, g + 1, pz - 1, pz + 1, 0)       -- the door, west
        for a = 0, 31 do
            local r = 12
            local x = px + math.floor(math.cos(a / 32 * 2 * math.pi) * r + 0.5)
            local z = pz + math.floor(math.sin(a / 32 * 2 * math.pi) * r + 0.5)
            if a % 8 ~= 0 then
                local t = top(x, z)
                box(x, x, t + 1, t + 1, z, z, SANDBAG)
            end
        end
    end

    -- ------------------------------------------------------------ the gallery
    box(-9, 2, FLOOR - 1, FLOOR + 4, MID - 11, MID + 11, GALLERY)
    box(-8, 1, FLOOR, FLOOR + 3, MID - 10, MID + 10, 0)
    for z = MID - 10, MID + 10 do
        -- Posts three and eight either side of the middle: none in front.
        box(2, 2, FLOOR, FLOOR + 3, z, z, if math.abs(z - MID) % 5 == 3 then GALLERY else PANE)
    end
    for _, z in { MID - 8, MID - 3, MID + 3, MID + 8 } do
        box(-4, -4, FLOOR + 4, FLOOR + 4, z, z, LAMP)
        box(-8, -8, FLOOR + 3, FLOOR + 3, z, z, LAMP)
    end
    -- Legs under the part that stands out over the field.
    for _, z in { MID - 10, MID, MID + 10 } do
        box(2, 2, top(2, z) + 1, FLOOR - 2, z, z, GALLERY)
    end
    -- Each portal's frame in its colour, and its half of the floor.
    for i, d in DOORS do
        local colour = if i == 1 then FRAME_R else FRAME_B
        local gz = d.gallery[3]
        box(-9, -9, FLOOR, FLOOR + 3, gz - 2, gz + 1, colour)
        box(-8, -6, FLOOR - 1, FLOOR - 1, gz - 2, gz + 1, colour)
        -- At the base: the bunker's back wall behind it in the team's colour.
        local bx, bz = d.base[1], d.base[3]
        local wz = if d.sign > 0 then bz - 1 else bz
        box(bx - 2, bx + 1, GROUND, GROUND + 3, wz, wz, colour)
    end

    -- ------------------------------------------------------------ the lines
    -- Two walls across the field, a third of the way from each base: stone,
    -- two high, gaps for the roads and here and there. A tank goes through
    -- where there is no gap.
    for _, lz in { 86, SIZE - 86 } do
        for x = 8, SIZE - 9 do
            local gap = math.abs(x - MID) <= 3 or x % 37 < 3
            if not gap then
                local t = top(x, lz)
                box(x, x, t + 1, t + 2, lz, lz, WALL)
            end
        end
    end
    -- Sandbag nests along the roads.
    for _, s in { { MID - 6, 60 }, { MID + 6, 60 }, { MID - 6, SIZE - 60 }, { MID + 6, SIZE - 60 },
                  { 80, MID - 6 }, { 176, MID + 6 } } do
        local sx, sz = s[1], s[2]
        for dx = -2, 2 do
            for dz = -2, 2 do
                if math.max(math.abs(dx), math.abs(dz)) == 2 and dz ~= 0 then
                    local t = top(sx + dx, sz + dz)
                    set(sx + dx, t + 1, sz + dz, SANDBAG)
                end
            end
        end
    end
end)

-- The portals open once the world runs (portal_open says -1 until then): each
-- gallery end linked with its base's.
local doors_open = false
core.event.on("core:tick", function(ev)
    if doors_open then return end
    for _, d in DOORS do
        local g, b = d.gallery, d.base
        local ig = core.portal_open(g[1], g[2], g[3], 1, 1, 0, 1.0, 1.25)
        if ig < 0 then return end
        local ib = core.portal_open(b[1], b[2], b[3], 0, d.sign, 0, 1.0, 1.25)
        if ib < 0 then
            core.portal_close(g[1], g[2], g[3])
            return
        end
        core.portal_link(ig, ib)
    end
    doors_open = true
end)
