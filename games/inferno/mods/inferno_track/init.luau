--!strict
-- inferno_track: the Inferno's side of games/track's Inferno track. A kart driven
-- through frame A over there (the track's second opening, --portal-to
-- @inferno/2) comes out of frame B here, this world's third opening (a mouth
-- back onto A, --portal-to @pista/1); it drives west from B along a road cut
-- through the caverns -- bridged over the lava, tunnelled through the rock, a
-- rail of brick either side -- south, east and north, and east into frame C,
-- this world's second opening, a mouth that comes out of the track's D
-- (--portal-to @pista/2). track_game runs here too and
-- times the two checkpoints on this side; the kart and its lap come with the
-- driver (core.player_carried).
--
-- The road is a mapgen stage after the caverns, so it is there in whatever
-- the caverns made: a Inferno made before this mod has none where it had
-- already been generated.

local GROUND = 20   -- the road's cells, as on the track: the road under them

-- The track's blocks that a kart and track_game read here, by the track's own
-- names: its road, the lines painted on it, the posts and beams of a line,
-- and the kart, which is what a driver wears (the body comes only into a room
-- that knows its model).
local ROAD = core.register_node("track_core:inferno_road", 60, 26, 30)
core.register_node("track_core:start", 235, 235, 235)
core.register_node("track_core:mark", 240, 200, 40)
core.register_node("track_core:wall", 200, 60, 50)
core.register_node("track_core:light", 250, 240, 210, true, 14)
local KART = core.register_node("track_core:kart", 40, 45, 55, true, 0, "upright", 2)
core.node_model(KART, core.register_model("track_core:kart", "track_core_kart.gltf"))
core.node_box(KART, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)

local SCORIA = core.node_id("inferno:scoria")
local BRICKS     = core.node_id("inferno:bricks")
local OBSIDIAN   = core.node_id("inferno:obsidian")
local GLOWROCK  = core.node_id("inferno:glowrock")

-- The road, 8 wide, in the order it is driven: west from B, south, east,
-- north, east into C.
local ROADS = {
    { -217, -141, -8, -1 },
    { -217, -210, -56, -1 },
    { -217, -175, -56, -49 },
    { -182, -175, -56, -25 },
    { -182, -141, -32, -25 },
}

-- The two frames: the column the opening stands in and the first z of its
-- hole (8 across, 8 up from the road). Both face west, -x: a kart comes out of
-- B heading west, and drives into C heading east.
local FRAME_B = { -140, -8 }
local FRAME_C = { -140, -32 }

local function on_road(x: number, z: number): boolean
    for _, r in ROADS do
        if x >= r[1] and x <= r[2] and z >= r[3] and z <= r[4] then
            return true
        end
    end
    return false
end

core.register_mapgen("inferno_track:road", function(ctx)
    local x0, x1 = math.max(ctx.min_x, -219), math.min(ctx.max_x, -130)
    local z0, z1 = math.max(ctx.min_z, -58), math.min(ctx.max_z, 1)
    if x0 > x1 or z0 > z1 then
        return
    end
    local function put(x: number, y: number, z: number, id: number)
        if x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z then
            core.set_block(x, y, z, id)
        end
    end

    for x = x0, x1 do
        for z = z0, z1 do
            if on_road(x, z) then
                -- Eight of air over the road, a deck of scoria under it,
                -- and now and then a pillar of brick down to the bedrock.
                core.set_block(x, GROUND - 1, z, ROAD)
                core.set_block(x, GROUND - 2, z, SCORIA)
                for y = GROUND, GROUND + 7 do
                    core.set_block(x, y, z, 0)
                end
                if x % 16 == 0 and z % 16 == 0 then
                    for y = 1, GROUND - 3 do
                        core.set_block(x, y, z, BRICKS)
                    end
                end
            elseif on_road(x - 1, z) or on_road(x + 1, z) or on_road(x, z - 1) or on_road(x, z + 1)
                or on_road(x - 1, z - 1) or on_road(x + 1, z + 1) or on_road(x - 1, z + 1)
                or on_road(x + 1, z - 1) then
                -- The rail: two of brick, a glowrock in it every six, so the
                -- road is lit in the dark between the caverns.
                core.set_block(x, GROUND - 1, z, BRICKS)
                core.set_block(x, GROUND, z, BRICKS)
                core.set_block(x, GROUND + 1, z, if (x + z) % 6 == 0 then GLOWROCK else BRICKS)
            end
        end
    end

    -- A frame, and behind it (east) a closed tunnel eight long: a kart runs a
    -- few nodes past C's plane before the crossing is answered.
    for _, f in { FRAME_B, FRAME_C } do
        local fx, fz = f[1], f[2]
        for k = 0, 9 do
            local x = fx + k
            for z = fz - 1, fz + 8 do
                for y = GROUND - 1, GROUND + 8 do
                    local hole = z >= fz and z <= fz + 7 and y >= GROUND and y <= GROUND + 7
                    if hole and k < 9 then
                        put(x, y, z, 0)
                    elseif y == GROUND - 1 then
                        put(x, y, z, if z >= fz and z <= fz + 7 then ROAD else OBSIDIAN)
                    else
                        put(x, y, z, if k == 0 then OBSIDIAN else SCORIA)
                    end
                end
            end
        end
    end
end)

-- C and B, until the tick makes them the frames' size: the world's second
-- and third openings.
core.set_portal(1, FRAME_C[1] + 0.5, GROUND + 4, FRAME_C[2] + 4)
core.set_portal(2, FRAME_B[1] + 0.5, GROUND + 4, FRAME_B[2] + 4)

-- Both the frames' size, facing west: every time the server starts, once it
-- has somewhere to put them.
local frames_open = false
local swept = false
core.event.on("core:tick", function(_)
    -- A world made while mods/infernoportal still framed this world's second
    -- opening has a 3x4 of its obsidian floating in C's hole: out of it, once
    -- C's cells are loaded.
    if not swept and core.loaded(FRAME_C[1], GROUND, FRAME_C[2]) ~= 0 then
        swept = true
        local STRAY = core.node_id("infernoportal:obsidian")
        local n = 0
        for y = GROUND, GROUND + 7 do
            for z = FRAME_C[2], FRAME_C[2] + 7 do
                if STRAY ~= 0 and core.get_block(FRAME_C[1], y, z) == STRAY then
                    core.set_block(FRAME_C[1], y, z, 0)
                    n += 1
                end
            end
        end
        if n > 0 then core.log(`inferno_track: {n} stray obsidian out of C`) end
    end
    if frames_open then
        return
    end
    if core.portal_place(1, FRAME_C[1] + 0.5, GROUND + 4, FRAME_C[2] + 4, 1, -1, 4, 4) == 0 then
        return
    end
    frames_open = true
    core.portal_place(2, FRAME_B[1] + 0.5, GROUND + 4, FRAME_B[2] + 4, 1, -1, 4, 4)
end)
