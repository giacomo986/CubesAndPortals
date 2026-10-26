--!strict
-- track_core: the world of a timed track (DESIGN.md M13). Its blocks, a walled
-- loop of road as a mapgen stage, where the world's two openings stand, and how
-- fast a body runs here. Which lines count, in what order and how fast is
-- track_game's.
--
-- The loop is a square 88 nodes a side around the middle of a 128 world, seven
-- nodes wide, between walls two high: a jump here clears 1.3, so the only way
-- round is the road.

local ROAD  = core.register_node("track_core:road", 52, 54, 60)
local BASE  = core.register_node("track_core:base", 40, 40, 44)
local WALL  = core.register_node("track_core:wall", 200, 60, 50)
local LIGHT = core.register_node("track_core:light", 250, 240, 210, true, 14)
core.register_node("track_core:start", 235, 235, 235)
core.register_node("track_core:mark", 240, 200, 40)
-- The paddock and the ways from it to each track: packed ground and not road,
-- but somewhere a kart may drive on a lap as on a track (track_game).
local PADDOCK = core.register_node("track_core:paddock", 128, 110, 82)

-- You arrive here ON FOOT: a walk, a sprint and a jump, and flying only with
-- the jetpack on the back (mods/jetpack lets a body fly or not by that). The
-- kart's numbers are track_game's, which is what puts a player in one
-- (core.body_drive) and takes them out again.
core.movement(4.8, 8, 14, 8, -25)

-- This project's own kart, made in Blender (games/track/CREDITS.md): 1.70 by
-- 2.36 m -- scaled with the robot, which sits in it -- nose to -z, painted by its materials alone -- no textures. Its bones
-- (steer_FL/FR, wheel_FL/FR/RL/RR, steering_wheel) are what the client turns on
-- a body that drives (docs/MODDING.md); a parked one stands still.
--
-- What a player drives. track_game puts it on a body that gets in a kart and
-- takes it off again when it gets out; the client sits the player's own model
-- in it, which is how a kart has a driver.
local KART_MODEL = core.register_model("track_core:kart", "track_core_kart.gltf")
-- And the parts a driver may put on it instead of its own (track_game's
-- /garage; tools/make_kart_parts.py): the client draws one worn by a driver on
-- the kart's bones, in place of the kart's bones of the same part.
for _, part in {"wheel_slick", "wheel_offroad", "wheel_retro", "nose_wedge", "nose_ram", "nose_shark",
                "pods_rocket", "pods_slab", "pipes_twin", "pipes_stack", "pipes_jet",
                "wing_spoiler", "wing_twin", "wing_fin"} do
    core.register_model("track_core:kart_" .. part, "kart_" .. part .. ".gltf")
end

-- And what a player IS here, on foot and at the wheel: the sandbox's robot
-- (mods/robot, the same two files linked here, since a world's model is only
-- ever a file beside its own mod).
core.player_model(core.register_model("track_core:robot", "robot.gltf"))
-- The extras a player may pick to wear (tools/make_robot_extras.py): the
-- client draws "<player model>_<extra>" on their skeleton in their colours.
for _, extra in {"cap", "tophat", "headphones", "scarf", "bowtie", "medal",
                  -- and who a player may be instead (tools/make_species.py): the
                  -- client draws "<player model>_<species>", its arms and styles.
                  "human", "human_arms", "human_short", "human_long", "human_beard",
                  "mole", "mole_arms", "mole_lamp", "mole_goggles", "mole_glasses",
                  "frog", "frog_arms", "frog_spots", "frog_crown", "frog_lily",
                  -- the extras refitted to each species, worn instead of the robot's
                  "human_cap", "human_tophat", "human_headphones", "human_scarf", "human_bowtie", "human_medal",
                  "mole_cap", "mole_tophat", "mole_headphones", "mole_scarf", "mole_bowtie", "mole_medal",
                  "frog_cap", "frog_tophat", "frog_headphones", "frog_scarf", "frog_bowtie", "frog_medal"} do
    core.register_model("track_core:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
-- The power-ups on the road (track_game; tools/make_track_items.py): the box a
-- kart drives through for one, and what is dropped and thrown.
for _, m in {"item_box", "banana", "shell_green", "shell_red"} do
    core.register_model("track_core:" .. m, m .. ".gltf")
end
-- And their icons, and the ink's blot, as items nobody is given: a picture
-- reaches a player's machine as a block's or an item's (tools/make_track_icons.py),
-- and the HUD draws these by file name.
for _, i in {"item_boost", "item_banana", "item_shell", "item_homing", "item_star", "item_bolt",
             "item_ink", "ink_blot"} do
    core.register_item("track_core:" .. i, 1)
end
core.eye_height(1.5)   -- the robot's visor, as in mods/robot
core.register_model("track_core:robot_arms", "robot_arms.gltf")   -- first person

-- A kart nobody is in, parked. A block, because a right click on a block is
-- what the engine already carries to the server (core:used) -- there is no
-- clicking an entity yet -- and the model makes it the same kart you then
-- drive. The mesh is 1.7 by 2.4 and is drawn about the CORNER of its cell,
-- which is how a cell draws a model authored about its own middle, so the kart
-- you see straddles four cells around the block that is it.
--
-- Its box is that ONE cell and not the mesh, which is a deliberate lie about
-- its nose and its tail: a box that leaves its cell is a box only the server
-- knows about -- a ray finds it and a body is stopped by it only in the cell
-- the block is in -- and a kart reversing into a parked one was put back by
-- the server as "inside a solid block" while its own client had driven it
-- clean through. One cell is what both ends agree on, and it is the middle of
-- the kart, which is what a player aims at.
--
-- dig_level 2: a bare hand cannot take the track's karts apart.
local KART = core.register_node("track_core:kart", 40, 45, 55, true, 0, "upright", 2)
core.node_model(KART, KART_MODEL)
core.node_box(KART, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)

local GROUND = 20   -- the engine's plaza height: cells from here up are air
local LO, HI = 20, 108   -- the road's middle line, on x and on z
local HALF   = 3         -- seven wide
local WALL_H = 2
local CLEAR  = 16        -- air over the road; the hills beside it reach about 14

-- The bay with the way out, south of the start straight: opening 0 at its back
-- facing the road, so a player who joins spawns four nodes out, on the road.
-- Opening 1 is in the bay too, against its west wall facing in: on a server
-- with no --portal-to the two are a pair, and in the walled infield, where it
-- stood first, a player who walked into the bay would come out with nothing
-- leading back to the road.
--
-- Thirteen wide and open to the road along all of it. A kart comes out of
-- opening 0 facing across the road, and in a bay five wide it had no room to
-- turn: a scripted driver ended nose to the inner wall and then inched back
-- and forth in the bay. At a crawl the wheel turns a kart round in 1.5 nodes,
-- so here it turns inside the bay and leaves along the straight.
local BAY_X, BAY_Z = 30, 13
local BAY_HALF     = 7   -- the side walls, either side of BAY_X

-- The paddock: where a player arrives and the karts wait, south of the loop
-- and outside all three tracks, each of which it leads to by a way of its
-- own and none across another: north through the loop's outer wall, north
-- east to the grand circuit's start straight, south to the ring. The karts
-- stand in a row in its middle, about forty nodes from each way out.
local PAD_X0, PAD_X1, PAD_Z0, PAD_Z1 = 80, 167, -44, 9
local PAD_KZ = -14   -- the row the karts stand in
local SLOTS  = { 110, 116, 122, 128, 134, 140 }

-- The world's two openings: opening 0 in the paddock facing +z towards the
-- karts, so a player who joins comes out four nodes in front of it; opening 1
-- facing +x in frame A of the Inferno track (below), the mouth onto the Inferno
-- and made the frame's size once the server runs; opening 2 likewise in D.
core.set_portal(0, 92.5, GROUND + 1, -38.5)
core.set_portal(1, -23.5, GROUND + 4, -4)
core.set_portal(2, -23.5, GROUND + 4, -28)
-- And opening 3, the way out (mods/keys: a way out is not the way in): at the
-- paddock's east end, facing +x, in a chequered frame put up at world_ready.
core.set_portal(3, 160.6, GROUND + 1, -37.5)
core.event.on("core:world_ready", function()
    local CHECK, LAMP = core.node_id("track_core:start"), core.node_id("track_core:light")
    for y = GROUND, GROUND + 3 do
        for z = -39, -37 do core.set_block(159, y, z, CHECK) end
        core.set_block(160, y, -39, CHECK)
        core.set_block(160, y, -37, CHECK)
    end
    core.set_block(160, GROUND + 2, -38, CHECK)
    core.set_block(160, GROUND + 3, -38, LAMP)
    core.set_block(160, GROUND, -38, 0)
    core.set_block(160, GROUND + 1, -38, 0)
end)

core.register_mapgen("track_core:loop", function(ctx)
    local A, B = LO - HALF - 1, HI + HALF + 1   -- the outer wall
    local a, b = LO + HALF + 1, HI - HALF - 1   -- the inner wall

    -- One column at a time, and most columns of a world with no edge are
    -- nowhere near the track: they are left to the terrain.
    local x0, x1 = math.max(A, ctx.min_x), math.min(B, ctx.max_x)
    local z0, z1 = math.max(BAY_Z - 3, ctx.min_z), math.min(B, ctx.max_z)
    if x0 > x1 or z0 > z1 then
        return
    end

    local function bed(x: number, z: number)
        for y = GROUND - 6, GROUND - 2 do
            core.set_block(x, y, z, BASE)
        end
        core.set_block(x, GROUND - 1, z, ROAD)
        for y = GROUND, GROUND + CLEAR do
            core.set_block(x, y, z, 0)
        end
    end

    local function wall(x: number, z: number, height: number)
        bed(x, z)
        for dy = 0, height - 2 do
            core.set_block(x, GROUND + dy, z, WALL)
        end
        -- A light every eight cells along the top.
        core.set_block(x, GROUND + height - 1, z, if (x + z) % 8 == 0 then LIGHT else WALL)
    end

    for x = x0, x1 do
        for z = math.max(A, z0), z1 do
            local outer = x == A or x == B or z == A or z == B
            local inner = ((x == a or x == b) and z >= a and z <= b) or
                          ((z == a or z == b) and x >= a and x <= b)
            local infield = x > a and x < b and z > a and z < b
            if outer or inner then
                wall(x, z, WALL_H)
            elseif not infield then
                bed(x, z)
            end
        end
    end

    -- The bay, after the loop: its mouth is a gap in the outer wall.
    for x = math.max(BAY_X - BAY_HALF, ctx.min_x), math.min(BAY_X + BAY_HALF, ctx.max_x) do
        for z = z0, math.min(A, z1) do
            if (x == BAY_X - BAY_HALF or x == BAY_X + BAY_HALF or z == BAY_Z - 3) then
                wall(x, z, 3)
            else
                bed(x, z)
            end
        end
    end

end)

-- The karts waiting in the paddock (SLOTS, above): a player arrives on foot in
-- front of them and gets into whichever they click. Facing 2 points them north,
-- between the loop's way and the grand circuit's.

-- Put down once the world file has been read, and not by the mapgen stage
-- above: a stage runs when a column is MADE, and the bay of a world saved
-- before there were karts in it is read back from the file for ever -- a track
-- with nothing to get into (reported from play). This asks what is there
-- instead, so an empty slot gets a kart whenever the server starts: one driven
-- away and left somewhere stays where it was left, and one whose driver was in
-- it when the server stopped comes back here.
-- A stretch of road laid over whatever is there: every cell, because a column
-- nobody is near is not made yet and reads as air -- the writes wait for it
-- and go in over its hills.
-- Up to 40 over the road, past the hills: cut at CLEAR, a hill's top was left
-- hanging over the paddock.
-- `top` false only lays the ground, over a paddock laid before as road.
-- `ground` is what is laid: the paddock's earth unless said, road for a track.
local function pave(x0: number, x1: number, z0: number, z1: number, top: boolean?, ground: number?)
    for x = x0, x1 do
        for z = z0, z1 do
            core.set_block(x, GROUND - 1, z, ground or PADDOCK)
            if top ~= false then
                core.set_block(x, GROUND - 2, z, BASE)
                for y = GROUND, GROUND + 40 do
                    core.set_block(x, y, z, 0)
                end
            end
        end
    end
end

core.event.on("core:world_ready", function()
    -- Once per world, by the store: the paddock's corner read here may not be
    -- loaded yet, and read as air it paved again at every start -- clearing
    -- whatever players had built on it (seen with the off-road tool's posts).
    -- "1" is a paddock laid as road, before it was ground: only its top
    -- is laid again, so what was built on it stays.
    local was = core.storage_get("paved")
    local fresh = was == ""
    core.storage_set("paved", "2")
    if was ~= "2" then
        local top = fresh
        pave(PAD_X0, PAD_X1, PAD_Z0, PAD_Z1, top)
        -- North: across to the loop's outer wall, and through it onto the
        -- south straight.
        pave(88, 103, PAD_Z1 + 1, LO - HALF - 2, top)
        if fresh then
            for x = 88, 103 do
                for y = GROUND, GROUND + WALL_H - 1 do
                    core.set_block(x, y, LO - HALF - 1, 0)
                end
            end
        end
        -- North east: up to the grand circuit's start straight (its south
        -- edge, z 24), west of its start line.
        pave(144, 151, PAD_Z1 + 1, 23, top)
        -- South: down to the ring's north side, over its two flat squares
        -- (x 112..127; track_game's start line is at 113).
        pave(116, 123, -56, PAD_Z0 - 1, top)
    end
    -- Into a paddock laid just now whatever the cell says: its hill may still
    -- be there, the writes that clear it waiting for the column (see pave).
    for _, x in SLOTS do
        if fresh or core.get_block(x, GROUND, PAD_KZ) == 0 then
            core.set_block(x, GROUND, PAD_KZ, KART)
            core.set_facing(x, GROUND, PAD_KZ, 2)
        end
    end
end)

-- ------------------------------------------------------------ track pieces
--
-- Pieces of road as stunt racers build them (core.node_piece): a smooth road on
-- a grid of 8 nodes, snapped as it is put down, joined where the ends meet. A
-- slope down is a slope up put down the other way round, a left bank a right
-- one driven the other way. `/pezzi` puts a stack of each in the bag.
--
-- The shapes, as core.node_piece reads them: plan (1 straight, 2 a quarter
-- turn right), size in squares, then climb, the grade at each end (nodes per
-- square: 2 is 14 degrees, 4 is 27) and the bank at each end (nodes the left
-- edge stands above the right), and whether it has curbs.
local FILL = core.register_node("track_core:piece_fill", 60, 62, 70, true, 0, "", 0, 0.2)

local PIECES: { { any } } = {
    -- name              plan size climb g0 g1 b0 b1 curbs
    { "straight",          1, 1, 0, 0, 0, 0, 0, true },
    { "straight2",         1, 2, 0, 0, 0, 0, 0, true },
    { "turn1",             2, 1, 0, 0, 0, 0, 0, true },
    { "turn2",             2, 2, 0, 0, 0, 0, 0, true },
    { "turn3",             2, 3, 0, 0, 0, 0, 0, true },
    { "chicane_right",     3, 2, 0, 0, 0, 0, 0, true },   -- one square across in two
    { "chicane_left",      4, 2, 0, 0, 0, 0, 0, true },
    { "slope_start",       1, 1, 1, 0, 2, 0, 0, true },
    { "slope",             1, 1, 2, 2, 2, 0, 0, true },
    { "slope_end",         1, 1, 1, 2, 0, 0, 0, true },
    -- The bends into and out of 27 degrees, and a hill, over two squares:
    -- over one a crest threw a kart at speed into the air (a crest holds a
    -- kart while it bends less than gravity does at 24 m/s).
    { "steep_start",       1, 2, 4, 0, 4, 0, 0, true },
    { "steep",             1, 1, 4, 4, 4, 0, 0, true },
    { "steep_end",         1, 2, 4, 4, 0, 0, 0, true },
    { "rise",              1, 2, 2, 0, 0, 0, 0, true },   -- up 2, flat at both ends: half a hill
    -- Three squares, for the same reason: the edges rise and sink two nodes,
    -- and on a bank gravity holds a kart to the road by its cosine. Over two
    -- a kart on the outer lane at 24 m/s left the road for a third of a second.
    { "tilt_in",           1, 3, 0, 0, 0, 0, 4, true },
    { "tilt",              1, 1, 0, 0, 0, 4, 4, true },
    { "tilt_out",          1, 3, 0, 0, 0, 4, 0, true },
    { "bank_turn2",        2, 2, 0, 0, 0, 4, 4, true },
    { "bank_turn3",        2, 3, 0, 0, 0, 4, 4, true },
    { "kicker",            1, 1, 2, 0, 4, 0, 0, true },
    { "kicker_big",        1, 1, 3, 0, 6, 0, 0, true },
    -- Down two in one square, put down the other way round after a kicker:
    -- where a jump lands. Driven slowly over, it is a crest a kart leaves.
    { "landing",           1, 1, 2, 0, 0, 0, 0, true },
    { "platform",          1, 1, 0, 0, 0, 0, 0, false },
    { "platform_slope",    1, 1, 2, 2, 2, 0, 0, false },
    { "platform_tilt",     1, 1, 0, 0, 0, 4, 4, false },
    -- Wall rides: the road rolled about its middle up into a wall and back,
    -- the bank in degrees. Left stands its left edge up; a kart stays on it
    -- while it is fast (from 10 m/s, fully from 16), and falls off it slow.
    { "wall_in_left",      1, 2, 0, 0, 0, 0, 90, false, true },
    { "wall_left",         1, 1, 0, 0, 0, 90, 90, false, true },
    { "wall_out_left",     1, 2, 0, 0, 0, 90, 0, false, true },
    { "wall_in_right",     1, 2, 0, 0, 0, 0, -90, false, true },
    { "wall_right",        1, 1, 0, 0, 0, -90, -90, false, true },
    { "wall_out_right",    1, 2, 0, 0, 0, -90, 0, false, true },
    -- Turns to the left: the right ones' mirrors (plan 5). A left turn banked
    -- has its right edge up, the outside of it.
    { "turn2_left",        5, 2, 0, 0, 0, 0, 0, true },
    { "turn3_left",        5, 3, 0, 0, 0, 0, 0, true },
    { "bank_turn2_left",   5, 2, 0, 0, 0, -4, -4, true },
    -- Wide: a road two squares across (the last number), the same shapes.
    -- A wide piece is put down by the square of its right lane; its banks
    -- are eight nodes across sixteen, the same 27 degrees.
    { "wide_straight",     1, 1, 0, 0, 0, 0, 0, true, false, 2 },
    { "wide_straight2",    1, 2, 0, 0, 0, 0, 0, true, false, 2 },
    { "wide_straight3",    1, 3, 0, 0, 0, 0, 0, true, false, 2 },
    { "wide_turn2",        2, 2, 0, 0, 0, 0, 0, true, false, 2 },
    { "wide_turn3",        2, 3, 0, 0, 0, 0, 0, true, false, 2 },
    { "wide_turn2_left",   5, 2, 0, 0, 0, 0, 0, true, false, 2 },
    { "wide_turn3_left",   5, 3, 0, 0, 0, 0, 0, true, false, 2 },
    { "wide_rise",         1, 2, 2, 0, 0, 0, 0, true, false, 2 },
    { "wide_slope_start",  1, 1, 1, 0, 2, 0, 0, true, false, 2 },
    { "wide_slope",        1, 1, 2, 2, 2, 0, 0, true, false, 2 },
    { "wide_slope_end",    1, 1, 1, 2, 0, 0, 0, true, false, 2 },
    { "wide_tilt_in",      1, 3, 0, 0, 0, 0, 8, true, false, 2 },
    { "wide_tilt",         1, 1, 0, 0, 0, 8, 8, true, false, 2 },
    { "wide_tilt_out",     1, 3, 0, 0, 0, 8, 0, true, false, 2 },
    { "wide_bank_turn3",   2, 3, 0, 0, 0, 8, 8, true, false, 2 },
    { "wide_bank_turn3_left", 5, 3, 0, 0, 0, -8, -8, true, false, 2 },
    { "wide_kicker",       1, 1, 2, 0, 4, 0, 0, true, false, 2 },
    { "wide_landing",      1, 1, 2, 0, 0, 0, 0, true, false, 2 },
    { "wide_platform",     1, 1, 0, 0, 0, 0, 0, false, false, 2 },
    -- Loops (plan 6 out to the left of the way in, 7 to the right): the road
    -- goes up, over the top upside down and back down a lane beside where it
    -- went up, two squares long. A kart goes round while it is fast, held as
    -- on a wall ride; slow, it falls off the top. Up 8.5 on one lane and 11.1
    -- on two, so wide is put down where there are 12 nodes of sky. Three
    -- squares long on one lane is the tall loop, twice as high: 16.9 up.
    { "loop_left",         6, 2, 0, 0, 0, 0, 0, false },
    { "loop_right",        7, 2, 0, 0, 0, 0, 0, false },
    { "tall_loop_left",    6, 3, 0, 0, 0, 0, 0, false },
    { "tall_loop_right",   7, 3, 0, 0, 0, 0, 0, false },
    { "wide_loop_left",    6, 2, 0, 0, 0, 0, 0, false, false, 2 },
    { "wide_loop_right",   7, 2, 0, 0, 0, 0, 0, false, false, 2 },
    -- Adapters: a wide road down to one lane over two squares, keeping its
    -- right lane (plan 8) or its left (9); put down the other way round, one
    -- lane opens out into two.
    { "wide_to_right",     8, 2, 0, 0, 0, 0, 0, true, false, 2 },
    { "wide_to_left",      9, 2, 0, 0, 0, 0, 0, true, false, 2 },
}

local PIECE: { [string]: number } = {}
for _, p in PIECES do
    local id = core.register_node(`track_core:{p[1]}`, 64, 66, 76, true, 0, "upright", 0, 0.2)
    if core.node_piece(id, FILL, p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10] == true,
                       p[11] or 1) == 1 then
        PIECE[p[1]] = id
    end
end

-- And the two tools a track of pieces is timed with (track_game reads them):
-- a right click with one on a road puts a start line, or a checkpoint, across
-- it where it was clicked, crossed away from whoever clicked.
core.register_item("track_core:start_line", 1)
core.register_item("track_core:checkpoint", 1)
-- And one for where a lap may leave the road: two clicks, two opposite corners
-- of an area a kart may drive anywhere in (track_game).
core.register_item("track_core:offroad", 1)
-- And one for item boxes: a right click on a road puts one there, another on
-- the same cell takes it away (track_game).
core.register_item("track_core:box_tool", 1)
-- And the track editor (below): a right click on a piece opens the pieces that
-- can follow the track it belongs to, and carried, every piece put down by hand.
core.register_item("track_core:editor", 1)

core.command("pezzi", "-- a stack of every track piece, the track editor (B with it in hand picks the next piece from afar), and the start line, checkpoint, off-road and item box tools")
core.event.on("core:command", function(ev)
    if ev.name ~= "pezzi" then
        return
    end
    -- The tools first: there are more pieces than the bag has squares.
    core.give_item(ev.player, "track_core:start_line", 1)
    core.give_item(ev.player, "track_core:checkpoint", 1)
    core.give_item(ev.player, "track_core:offroad", 1)
    core.give_item(ev.player, "track_core:box_tool", 1)
    core.give_item(ev.player, "track_core:editor", 1)
    for _, p in PIECES do
        core.give_item(ev.player, `track_core:{p[1]}`, 16)
    end
end)

-- What is still to be put down: near where the world first loads, the hills
-- are there and not yet lit, a mod's writes to them wait until they are, and a
-- piece asked for meanwhile finds the hill it cleared still in its way. It is
-- asked again every two seconds, for a minute.
local pieces_left: { { number } } = {}
local pieces_tries = 0
local function place_pieces(say: boolean)
    local still = {}
    for _, q in pieces_left do
        if core.place_piece(q[1], q[2], q[3], q[4], q[5]) ~= 1 then
            table.insert(still, q)
        end
    end
    pieces_left = still
    if say and #still > 0 then
        for _, q in still do
            print(`track_core: track piece {core.node_name(q[4])} not down at {q[1]} {q[2]} {q[3]}`)
        end
    end
end
core.event.on("core:tick", function(_)
    if #pieces_left == 0 then
        return
    end
    pieces_tries += 1
    if pieces_tries % 40 == 0 then
        place_pieces(pieces_tries >= 40 * 30)
        if pieces_tries >= 40 * 30 then
            pieces_left = {}
        end
    end
end)

-- The demonstration: a ring of pieces south of the paddock, entered from it
-- over the two flat squares of its north side. Its east side is banked end to end, the
-- turns at both of its corners too, as an oval is; a kicker and a landing lead
-- into it, and on the other side the road rolls up into a wall along the
-- ring's outer edge and back down: a wall ride, taken fast. Put down once the world is read, like
-- the karts, and only when it is not there yet.
--
-- Squares of 8 from (RING_X, RING_Z): nine a side, the ring their outer row.
-- Driven clockwise from above (+z, -x, -z, +x), every corner a right turn. A
-- piece longer than a square is put down by its first. It stood in the loop's
-- infield first, which only the loop led to.
local RING_X, RING_Z = 96, -128   -- on the grid of 8: a piece snaps to it
local function square(gx: number, gz: number): (number, number)
    return RING_X + 8 * gx, RING_Z + 8 * gz
end
-- The byte for a heading: a piece is entered from the player's side.
local HEAD = { pz = 2, px = 3, mz = 0, mx = 1 }

local RING = {
    -- piece, square, heading, height above GROUND
    { "kicker", 2, 0, "px", 0 }, { "landing", 3, 0, "mx", 0 }, { "tilt_in", 4, 0, "px", 0 },
    { "bank_turn2", 7, 0, "px", 0 },
    { "tilt", 8, 2, "pz", 0 }, { "tilt", 8, 3, "pz", 0 }, { "tilt", 8, 4, "pz", 0 },
    { "tilt", 8, 5, "pz", 0 }, { "tilt", 8, 6, "pz", 0 }, { "bank_turn2", 8, 7, "pz", 0 },
    { "tilt_out", 6, 8, "mx", 0 }, { "straight", 3, 8, "mx", 0 }, { "straight", 2, 8, "mx", 0 },
    { "turn2", 1, 8, "mx", 0 },
    { "wall_in_left", 0, 6, "mz", 0 }, { "wall_left", 0, 4, "mz", 0 },
    { "wall_out_left", 0, 3, "mz", 0 },
    { "turn2", 0, 1, "mz", 0 },
}

core.event.on("core:world_ready", function()
    local fx, fz = square(8, 2)
    if core.get_block(fx, GROUND, fz) == PIECE.tilt then
        return
    end

    -- All of it cleared of the hills, the ring and what it goes
    -- round, on a floor of base five under the road: a bank is tipped about
    -- the middle of the road, and its low edge goes under it -- inside the
    -- ring the ground would stand beside that edge like a wall -- and a wall
    -- ride's foot four under it.
    -- Inside the ring, two under the road is enough for a bank's low edge.
    for gz = 0, 8 do
        for gx = 0, 8 do
            local x0, z0 = square(gx, gz)
            local ring = gx == 0 or gx == 8 or gz == 0 or gz == 8
            -- And level with the road behind the way in, where the north
            -- side is flat: a kart that comes in from the paddock too far
            -- runs onto ground, not into a pit.
            local behind = gz == 7 and (gx == 2 or gx == 3)
            local floor = if ring then GROUND - 6 elseif behind then GROUND - 1 else GROUND - 3
            for x = x0, x0 + 7 do
                for z = z0, z0 + 7 do
                    core.set_block(x, floor, z, BASE)
                    for y = GROUND - 5, floor - 1 do
                        core.set_block(x, y, z, BASE)
                    end
                    for y = floor + 1, GROUND + CLEAR do   -- every cell: see pave
                        core.set_block(x, y, z, 0)
                    end
                end
            end
        end
    end

    for _, r in RING do
        local x, z = square(r[2], r[3])
        table.insert(pieces_left, { x, GROUND + r[5], z, PIECE[r[1]], HEAD[r[4]] })
    end
    place_pieces(false)
end)

-- ---------------------------------------------------------- the grand circuit
--
-- A wide circuit outside the loop, east of it, reached from the paddock by a
-- road to its start straight. Laid by a turtle: each piece is put
-- down by the square of its right lane, and where the next one goes follows
-- from its shape -- so the list below is the whole circuit, and it closes on
-- itself (checked by hand in squares before it was written down). Driven
-- clockwise from above; on the way a hill, a jump, two banked turns with the
-- tilts into and out of them, and an S of turns on two sides.
local FWD = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }   -- +z, +x, -z, -x
local function right_of(t: number) return FWD[(t + 3) % 4 + 1] end   -- turned a quarter right

local GRAND = {
    -- piece, how: "up" climbs as it goes, "down" is an up piece (or an
    -- adapter) driven the other way round, "r"/"l" a turn
    { "wide_straight3" }, { "wide_rise", "up" }, { "wide_straight2" }, { "wide_rise", "down" },
    { "wide_tilt_in" }, { "wide_bank_turn3", "r" },
    { "wide_tilt_out" }, { "wide_turn3_left", "l" }, { "wide_straight2" }, { "wide_turn3", "r" },
    { "wide_straight2" }, { "wide_straight2" }, { "wide_turn3", "r" },
    { "wide_straight3" }, { "wide_kicker", "up" }, { "wide_landing", "down" }, { "wide_straight2" },
    { "wide_straight2" }, { "wide_tilt_in" }, { "wide_bank_turn3", "r" },
    { "wide_tilt_out" }, { "wide_straight2" }, { "wide_turn3_left", "l" }, { "wide_straight2" },
    { "wide_turn3", "r" }, { "wide_straight2" }, { "wide_turn3", "r" },
}
local GRAND_X, GRAND_Z = 18, 4   -- the first piece's right lane, in squares; heading +x

-- The loops track, west of the loop: two lanes up a wide loop, down to one
-- lane, a tall loop out to the left, then a tall one out to the right and two
-- of the low ones, left and right, and back out to two.
-- Driven from (LOOPS_X, LOOPS_Z) heading +z; reached from the Inferno track's
-- road, north over the flat side of its last straight.
local LOOPS = {
    { "wide_straight3" }, { "wide_loop_right" }, { "wide_straight" }, { "wide_turn3", "r" },
    { "wide_to_right" }, { "straight" }, { "tall_loop_left" }, { "straight" }, { "turn2", "r" },
    { "tall_loop_right" }, { "loop_left" }, { "loop_right" }, { "turn2", "r" },
    { "straight2" }, { "straight2" }, { "wide_to_left", "down" }, { "wide_straight2" },
    { "wide_turn3", "r" },
}
local LOOPS_X, LOOPS_Z = -1, 4

-- A ramp up from the ground to a piece's curb (0.4 over its cell's floor) in
-- steps of a tenth, which a wheel rolls up: a piece's edge is a step it stops
-- against.
local RAMP = {}
for i = 1, 4 do
    RAMP[i] = core.register_node(`track_core:ramp{i}`, 128, 110, 82)
    core.node_box(RAMP[i], 0.0, 0.0, 0.0, 1.0, 0.1 * i, 1.0)
end

local SIZE: { [string]: number } = {}
local PLAN: { [string]: number } = {}
local WIDTH: { [string]: number } = {}
local CLIMB: { [string]: number } = {}
for _, p in PIECES do
    SIZE[p[1]], PLAN[p[1]], WIDTH[p[1]], CLIMB[p[1]] = p[3], p[2], p[11] or 1, p[4]
end

-- A turtle on the grid of squares: the right lane's square it stands on, the
-- way it heads (an index of FWD) and its height.
type Turtle = { gx: number, gz: number, t: number, y: number }

-- One piece through a turtle: where it goes down (the square of its right lane,
-- the height and the facing core.place_piece takes), the turtle after it, and
-- its footprint as `rows` squares on, `a`..`b` across (left of the entry's
-- right lane positive) from where the turtle stood. A turn is "r" or "l" by its
-- plan; `back` puts a piece down the other way round -- a slope driven down,
-- a lane opening out into two -- by its own right lane at the far end.
local function step(st: Turtle, name: string, back: boolean?): ({ number }, Turtle, { number })
    local gx, gz, t, y = st.gx, st.gz, st.t, st.y
    local n, w, plan = SIZE[name], WIDTH[name], PLAN[name]
    local f, r = FWD[t + 1], right_of(t)
    local climb = CLIMB[name]
    if plan == 2 then
        -- A turn's footprint: n squares on, n across from the right lane (to
        -- the left of it for a right turn's inner lane start, see piece_origin).
        return { gx, y, gz, t + 2 },
               { gx = gx + f[1] * (n - w) + r[1] * (n - w + 1), gz = gz + f[2] * (n - w) + r[2] * (n - w + 1),
                 t = (t + 3) % 4, y = y },
               { n, -(n - w), w - 1 }
    elseif plan == 5 then
        return { gx, y, gz, t + 2 },
               { gx = gx + f[1] * (n - 1) - r[1] * n, gz = gz + f[2] * (n - 1) - r[2] * n, t = (t + 1) % 4, y = y },
               { n, 0, n - 1 }
    elseif plan == 6 or plan == 7 then
        -- A loop: the lane(s) in and the lane(s) beside them it comes out on.
        local by = if plan == 6 then w else -w
        return { gx, y, gz, t + 2 }, { gx = gx + f[1] * n - r[1] * by, gz = gz + f[2] * n - r[2] * by, t = t, y = y },
               if plan == 6 then { n, 0, 2 * w - 1 } else { n, -w, w - 1 }
    elseif (plan == 8 or plan == 9) and back then
        -- One lane opening out into two: the turtle is on that lane, the piece
        -- put down by its own right lane (this side's left) at the far end.
        -- 8 keeps its right lane, so here the new lane is on the right.
        local lx, lz = if plan == 8 then gx else gx - r[1], if plan == 8 then gz else gz - r[2]
        local nx, nz = gx, gz
        if plan == 8 then
            nx, nz = gx + r[1], gz + r[2]
        end
        return { lx + f[1] * (n - 1), y, lz + f[2] * (n - 1), t },
               { gx = nx + f[1] * n, gz = nz + f[2] * n, t = t, y = y },
               if plan == 8 then { n, -1, 0 } else { n, 0, 1 }
    end
    -- Straight on: a chicane one square across (3 right, 4 left), an adapter
    -- down to its right (8) or left (9) lane.
    local across = if plan == 3 then 1 elseif plan == 4 then -1 elseif plan == 9 then -1 else 0
    local rows = if plan == 3 then { n, -1, 0 } elseif plan == 4 or plan == 8 or plan == 9 then { n, 0, 1 }
                 else { n, 0, w - 1 }
    local put
    if back then
        -- By its own right lane: the far end of this one's left.
        put = { gx + f[1] * (n - 1) - r[1] * (w - 1), y - climb, gz + f[2] * (n - 1) - r[2] * (w - 1), t + 4 }
        y -= climb
    else
        put = { gx, y, gz, t + 2 }
        y += climb
    end
    return put, { gx = gx + f[1] * n + r[1] * across, gz = gz + f[2] * n + r[2] * across, t = t, y = y }, rows
end

-- The squares a piece's rows cover, from where the turtle stood.
local function squares(st: Turtle, rows: { number }): { { number } }
    local f, r = FWD[st.t + 1], right_of(st.t)
    local out = {}
    for k = 0, rows[1] - 1 do
        for j = rows[2], rows[3] do
            table.insert(out, { st.gx + f[1] * k - r[1] * j, st.gz + f[2] * k - r[2] * j })
        end
    end
    return out
end

-- A circuit laid by a turtle from the right lane of its first piece, in
-- squares, heading t (an index of FWD): `steps` is the whole circuit, and it
-- closes on itself (said when not). "down" in a step puts that piece down the
-- other way round.
local function lay(steps: { { string } }, x0: number, z0: number, t0: number, what: string)
    local st: Turtle = { gx = x0, gz = z0, t = t0, y = GROUND }

    -- A square cleared of the hills, deep enough for a bank's low edge under
    -- a tilted piece and on the ground under the rest.
    local function clear(sx: number, sz: number, deep: boolean)
        local floor = if deep then GROUND - 6 else GROUND - 1
        for x = sx * 8, sx * 8 + 7 do
            for z = sz * 8, sz * 8 + 7 do
                core.set_block(x, floor, z, BASE)
                -- Every cell and not only what is there now: a column far
                -- from anybody is not made yet, reads as air, and its hills
                -- come afterwards -- the writes wait for it and go in over
                -- them.
                -- Up to a tall loop's top, over CLEAR.
                for yy = floor + 1, GROUND + 18 do
                    core.set_block(x, yy, z, 0)
                end
            end
        end
    end

    for _, s in steps do
        local name = s[1]
        local deep = string.find(name, "tilt") ~= nil or string.find(name, "bank") ~= nil
        local put, after, rows = step(st, name, s[2] == "down")
        for _, q in squares(st, rows) do
            clear(q[1], q[2], deep)
        end
        table.insert(pieces_left, { put[1] * 8, put[2], put[3] * 8, PIECE[name], put[4] % 4 })
        st = after
    end
    if st.gx ~= x0 or st.gz ~= z0 or st.t ~= t0 or st.y ~= GROUND then
        print(`track_core: the {what} does not close ({st.gx} {st.gz} {st.t} {st.y})`)
    end
    place_pieces(false)
end

core.event.on("core:world_ready", function()
    if core.get_block(GRAND_X * 8, GROUND, GRAND_Z * 8) ~= PIECE.wide_straight3 then
        lay(GRAND, GRAND_X, GRAND_Z, 1, "grand circuit")
    end
    -- The loops track, once, by the store (its corner read here may not be
    -- loaded yet, as the paddock's).
    -- Laid again over a world that has the one before the tall loops ("2").
    local loops = core.storage_get("loops")
    if loops ~= "3" then
        lay(LOOPS, LOOPS_X, LOOPS_Z, 0, "loops track")
    end
    -- And the way to it: north from the Inferno track's road along the east
    -- side of the track (x 8..15, between it and the loop's wall), then west
    -- up a ramp onto the start straight's side, before its line. It went
    -- north beside frame A first, and a kart turning there drove into the
    -- Inferno.
    core.storage_set("loops", "3")
    if loops == "" then
        pave(8, 15, 0, 39, true)
        for i, x in { 11, 10, 9, 8 } do
            for z = 32, 39 do
                core.set_block(x, GROUND, z, RAMP[i])
            end
        end
    end
end)

-- ---------------------------------------------------------- the inferno track
--
-- A track half in the world and half in the Inferno -- games/inferno's room, a
-- world of its own on this server: west out of the paddock, through an 8x8
-- frame of obsidian (A) into the Inferno, round a road through its caverns
-- (games/inferno/mods/inferno_track), out of a second frame there (C) back into
-- the world by D, east and north to the start line again.
--
-- A is the world's second opening, a mouth onto the Inferno's third (B,
-- --portal-to @inferno/2), and D its third, a mouth onto the Inferno's second
-- (C, --portal-to @inferno/1): each way there and back, and drawn from both
-- sides. The kart goes with its driver, as the server takes a body that
-- drives into a room that knows its model, and track_game there times the
-- Inferno's side.
core.register_node("track_core:inferno_road", 60, 26, 30)
local OBSIDIAN    = core.register_node("track_core:obsidian", 22, 16, 34, true, 0, "", 2)
-- The cavern the Inferno was before it was the Inferno's room: a world made
-- then still has these in it.
core.register_node("track_core:scoria", 110, 36, 34)
core.register_node("track_core:glowrock", 240, 200, 110, true, 15)
core.node_liquid(core.register_node("track_core:lava", 230, 100, 20, false, 15), 250)

-- The two frames on this side: the column the opening stands in, the first z
-- of its hole (8 across, 8 up from the road); both face east, +x.
local FRAME_A = { -24, -8 }
local FRAME_D = { -24, -32 }

-- A frame, and behind it a closed tunnel eight long: a kart runs a few nodes
-- past the plane before the crossing is answered, and against a back to the
-- frame it stopped short.
local function frame(fx: number, z0: number)
    for k = 0, 9 do
        local x = fx - k
        for z = z0 - 1, z0 + 8 do
            for y = GROUND - 1, GROUND + 8 do
                local hole = z >= z0 and z <= z0 + 7 and y >= GROUND and y <= GROUND + 7
                if hole and k < 9 then
                    core.set_block(x, y, z, 0)
                elseif y == GROUND - 1 then
                    core.set_block(x, y, z, if z >= z0 and z <= z0 + 7 then ROAD else OBSIDIAN)
                else
                    core.set_block(x, y, z, OBSIDIAN)
                end
            end
        end
    end
end

-- The world's side: west from the paddock to A, back from D east, and north
-- between them to the start straight. Road: it is a track, not the paddock.
local function inferno_world_side(top: boolean)
    pave(-24, PAD_X0 - 1, -8, -1, top, ROAD)
    pave(-24, 70, -32, -25, top, ROAD)
    pave(63, 70, -32, -9, top, ROAD)
end

core.event.on("core:world_ready", function()
    -- A world made while the paddock's earth was laid here too: road again,
    -- the top only, once.
    local fixed = core.storage_get("inferno_road") == "1"
    core.storage_set("inferno_road", "1")
    -- Built once, by the store: frame A's cell read here may not be loaded
    -- yet, and read as air the frames were built again (as the paddock).
    local built = core.storage_get("inferno_built") == "1"
        or core.get_block(FRAME_A[1], GROUND + 8, FRAME_A[2]) == OBSIDIAN
    core.storage_set("inferno_built", "1")
    if built then
        if not fixed then
            inferno_world_side(false)
        end
        return
    end
    inferno_world_side(true)
    frame(FRAME_A[1], FRAME_A[2])
    frame(FRAME_D[1], FRAME_D[2])
end)

-- A and D the frames' size: not kept in the world file, so every time the
-- server starts, once it has somewhere to put them (not on the first ticks of
-- a room whose socket is not up yet). Without the Inferno's room beside it
-- (no third --portal-to) D is not a mouth and stays shut.
local frames_open = false
core.event.on("core:tick", function(_)
    if frames_open then
        return
    end
    if core.portal_place(1, FRAME_A[1] + 0.5, GROUND + 4, FRAME_A[2] + 4, 1, 1, 4, 4) == 0 then
        return
    end
    frames_open = true
    core.portal_place(2, FRAME_D[1] + 0.5, GROUND + 4, FRAME_D[2] + 4, 1, 1, 4, 4)
end)

-- ------------------------------------------------------------ the track editor
--
-- As track editors of stunt racers and coaster builders have it: a track is
-- carried on from its open end with the pieces that fit there, picked from a
-- panel, and closed into a circuit with one button. A right click with the
-- editor on any piece follows its track both ways -- back to where it starts,
-- on to where it is open -- and lists what fits the open end: a piece whose
-- entry has the width, grade and bank the end leaves (a slope's other half, a
-- bank's tilt out, a lane opening out after an adapter...). A click puts it
-- down there with core.place_piece, and the list is made again from the new
-- end. Undo takes the last piece put down back; Close circuit searches for the
-- pieces that lead from the open end back into the start (A*, below).
--
-- Each piece is followed with step(), the turtle the circuits above are laid
-- with, from where core.piece_entry says it is entered.

local EDITOR = "track_core:editor"
local NAME_OF: { [number]: string } = {}
for name, id in PIECE do
    NAME_OF[id] = name
end

-- What a road is like where a piece is entered or left: lanes, grade (nodes
-- per square, up positive) and bank (nodes, or degrees on a wall ride, the
-- left edge up positive). Driven the other way round, a piece's grades and
-- banks change sign and swap ends. A kicker's far end is a jump: anything
-- lands after it.
type Profile = { w: number, g: number?, b: number }
local SHAPE: { [string]: { any } } = {}
for _, p in PIECES do
    SHAPE[p[1]] = p
end
local function ends(name: string, back: boolean): (Profile, Profile)
    local p = SHAPE[name]
    local w, plan = p[11] or 1, p[2]
    local w_in, w_out = w, w
    if plan == 8 or plan == 9 then
        w_in, w_out = 2, 1
    end
    local free = string.find(name, "kicker") ~= nil
    if back then
        return { w = w_out, g = -p[6], b = -p[8] }, { w = w_in, g = -p[5], b = -p[7] }
    end
    return { w = w_in, g = p[5], b = p[7] }, { w = w_out, g = if free then nil else p[6], b = p[8] }
end
local function fits(out: Profile, entry: Profile): boolean
    return out.w == entry.w and out.b == entry.b and (out.g == nil or out.g == entry.g)
end

-- Every way a piece may be put down: forward, and the other way round where
-- that is another road (a slope down, a bank the other side up, one lane into
-- two). Turns, loops and chicanes are the same piece either way.
local WAYS: { { name: string, back: boolean } } = {}
for _, p in PIECES do
    table.insert(WAYS, { name = p[1], back = false })
    local a, b = ends(p[1], false)
    local plan = p[2]
    if (plan == 1 or plan == 8 or plan == 9) and (p[4] ~= 0 or a.g ~= b.g or a.b ~= b.b or plan ~= 1) then
        table.insert(WAYS, { name = p[1], back = true })
    end
end

local function same(a: Turtle, b: Turtle): boolean
    return a.gx == b.gx and a.gz == b.gz and a.t == b.t and a.y == b.y
end

-- The turtles a piece put down may be driven from: its own entry heading its
-- own way, and -- put down the other way round, by its own entry at the far
-- end -- the turtle that step() takes there (found by trying the few squares
-- around, since the far end is not where it is entered).
local BACKWARDS: { [string]: boolean } = {}
for _, way in WAYS do
    if way.back then
        BACKWARDS[way.name] = true
    end
end
local function travels(name: string, ex: number, ey: number, ez: number, facing: number): { { any } }
    local out = { { { gx = ex // 8, gz = ez // 8, t = (facing + 2) % 4, y = ey }, false } }
    if BACKWARDS[name] then
        local gx, gz = ex // 8, ez // 8
        for dx = -3, 3 do
            for dz = -3, 3 do
                for dy = -6, 6 do
                    local from: Turtle = { gx = gx + dx, gz = gz + dz, t = facing % 4, y = ey + dy }
                    local put = step(from, name, true)
                    if put[1] == gx and put[3] == gz and put[2] == ey and put[4] % 4 == facing % 4 then
                        table.insert(out, { from, true })
                    end
                end
            end
        end
    end
    return out
end

-- The piece with a cell in square (sx, sz) about height y, as core.piece_entry
-- says it, and the turtles it may be driven from. A few columns and heights:
-- a wall ride's road stands up across its square, and its cells with it.
local function piece_in(sx: number, sz: number, y: number): (string?, { { any } })
    for _, dy in { 0, -1, 1, -2, 2, -3, 3, -4, 4 } do
        for _, c in { { 4, 4 }, { 3, 3 }, { 3, 4 }, { 4, 3 }, { 0, 4 }, { 7, 4 }, { 4, 0 }, { 4, 7 } } do
            local ex, ey, ez, id, facing = core.piece_entry(sx * 8 + c[1], y + dy, sz * 8 + c[2])
            local name = ex and NAME_OF[id]
            if name ~= nil then
                return name, travels(name, ex, ey, ez, facing)
            end
        end
    end
    return nil, {}
end

-- The piece entered at a turtle, if one is there.
local function piece_at(st: Turtle): (string?, boolean)
    local name, ways = piece_in(st.gx, st.gz, st.y)
    for _, w in ways do
        if same(w[1], st) then
            return name, w[2]
        end
    end
    return nil, false
end

-- The piece that ends where a turtle stands, and the turtle that entered it:
-- its last square is the one behind.
local function piece_before(st: Turtle): (string?, boolean, Turtle?)
    local f = FWD[st.t + 1]
    local name, ways = piece_in(st.gx - f[1], st.gz - f[2], st.y)
    for _, w in ways do
        local _, after = step(w[1], name :: string, w[2])
        if same(after, st) then
            return name, w[2], w[1]
        end
    end
    return nil, false, nil
end

-- What a player is building: the track's first turtle (where Close circuit
-- leads back to), its open end, the road's profile there, and the pieces this
-- panel put down, last first off with Undo.
type Edit = { start: Turtle, start_in: Profile, at: Turtle, out: Profile, put: { { any } } }
local editing: { [number]: Edit } = {}

-- A whole track from one of its pieces: back to its start and on to its open
-- end, 200 pieces each way at most. A circuit already closed says so.
local function follow(st: Turtle, name: string, back: boolean): (Edit?, string?)
    local first, first_name, first_back = st, name, back
    for _ = 1, 200 do
        local n, rev, from = piece_before(first)
        if n == nil or from == nil then
            break
        end
        if same(from, st) then
            return nil, "This track is a circuit already."
        end
        first, first_name, first_back = from, n, rev
    end
    local at, last_name, last_back = st, name, back
    local _, after = step(at, name, back)
    for _ = 1, 200 do
        local n, rev = piece_at(after)
        if n == nil then
            break
        end
        if same(after, first) then
            return nil, "This track is a circuit already."
        end
        at, last_name, last_back = after, n, rev
        _, after = step(at, n, rev)
    end
    local start_in = ends(first_name, first_back)
    local _, out = ends(last_name, last_back)
    return { start = first, start_in = start_in, at = after, out = out, put = {} }, nil
end

local PANEL_W, COLS, STEP_UI, SIDE_UI = 14.4, 10, 1.4, 1.2

local function panel_of(p: number): string
    return `track_core:editor{p}`
end

-- The ways that fit the open end, in the order PIECES lists them.
local function fitting(e: Edit): { { name: string, back: boolean } }
    local out = {}
    for _, way in WAYS do
        local entry = ends(way.name, way.back)
        if fits(e.out, entry) then
            table.insert(out, way)
        end
    end
    return out
end

local function show(p: number)
    local e = editing[p]
    if e == nil then
        return
    end
    local list = fitting(e)
    local rows = math.max(1, (#list + COLS - 1) // COLS)
    local panel = panel_of(p)
    if core.ui_define(panel, PANEL_W, 3.2 + rows * STEP_UI) == 0 then
        return
    end
    core.ui_box(panel, 0.2, 0.2, PANEL_W - 0.4, 0.9, 40, 44, 56)
    core.ui_label(panel, 0.5, 0.45, `next piece -- {#list} fit, {#e.put} put down here`)
    for i, way in list do
        local col, row = (i - 1) % COLS, (i - 1) // COLS
        local x, y = 0.4 + col * STEP_UI, 1.35 + row * STEP_UI
        core.ui_item(panel, `w{i}`, x, y, SIDE_UI, SIDE_UI, `track_core:{way.name}`)
        if way.back then
            core.ui_label(panel, x + 0.05, y + 0.05, "rev")
        end
    end
    local by = 1.6 + rows * STEP_UI
    core.ui_button(panel, "undo", 0.4, by, 3.1, 0.9, "undo")
    core.ui_button(panel, "close_circuit", 3.7, by, 3.4, 0.9, "close circuit")
    core.ui_button(panel, "pick", 7.3, by, 3.4, 0.9, "pick from afar")
    core.ui_button(panel, "close", 10.9, by, 3.1, 0.9, "close panel")
    core.ui_show(panel, true, p)
end

local function start_from(p: number, x: number, y: number, z: number): boolean
    local ex, ey, ez, id, facing = core.piece_entry(x, y, z)
    local name = ex and NAME_OF[id]
    if name == nil then
        return false
    end
    -- Followed the way it runs, as put down.
    local e, why = follow({ gx = ex // 8, gz = ez // 8, t = (facing + 2) % 4, y = ey }, name, false)
    if e == nil then
        core.chat(p, why or "No track here")
        return true
    end
    editing[p] = e
    show(p)
    return true
end

core.event.on("core:used", function(ev)
    if ev.held ~= EDITOR then
        return
    end
    ev.cancel = true
    if not start_from(ev.player, ev.x, ev.y, ev.z) then
        core.chat(ev.player, "The track editor goes on a track piece: right click one")
    end
end)

-- Carried, every piece put down by hand opens the panel from it.
core.event.on("core:placed", function(ev)
    if NAME_OF[ev.id] ~= nil and core.bag_count(ev.player, EDITOR) > 0 then
        start_from(ev.player, ev.x, ev.y, ev.z)
    end
end)

-- Put down at the open end, and the end moved on.
local function extend(p: number, e: Edit, name: string, back: boolean): boolean
    local put, after = step(e.at, name, back)
    if core.place_piece(put[1] * 8, put[2], put[3] * 8, PIECE[name], put[4] % 4) ~= 1 then
        return false
    end
    table.insert(e.put, { put[1] * 8, put[2], put[3] * 8, e.at, e.out })
    local _, out = ends(name, back)
    e.at, e.out = after, out
    return true
end

-- The last piece put down with the editor taken back, and the end with it.
local function take_back(e: Edit): boolean
    local last = table.remove(e.put)
    if last == nil then
        return false
    end
    -- By a cell of it: the middle of its entry square at its height is not one
    -- for every piece (a turn's, a chicane's), and a piece left there walled
    -- off the open end -- A and D then found nothing that fits. So the cell
    -- whose piece is entered where this one was, searched for in that square.
    local x, y, z = last[1], last[2], last[3]
    if not (core.piece_entry(x + 4, y, z + 4) == x and core.take_piece(x + 4, y, z + 4) ~= 0) then
        for dy = -8, 8 do
            for dx = 0, 7 do
                for dz = 0, 7 do
                    local ex, ey, ez = core.piece_entry(x + dx, y + dy, z + dz)
                    if ex == x and ey == y and ez == z then
                        core.take_piece(x + dx, y + dy, z + dz)
                        e.at, e.out = last[4], last[5]
                        return true
                    end
                end
            end
        end
        print(`track_core: no piece entered at {x} {y} {z} to take back`)
    end
    e.at, e.out = last[4], last[5]
    return true
end

-- ------------------------------------------------- closing the circuit
--
-- A* over turtles {gx, gz, t, y} from the open end to the start, with the
-- flat pieces of the end's width and a rise either way to make up a height
-- (two squares for two nodes); a piece is refused where its squares are taken
-- -- by the path so far, or by anything solid at road height. Run a few
-- hundred turtles a tick in a coroutine, and given up past MAX_TURTLES or
-- MAX_PIECES. The width is part of where a turtle is: an adapter takes two
-- lanes down to one and, the other way round, one out to two. Flat first: the
-- rises only when no way round at the end's height closes it.
local MAX_TURTLES, MAX_PIECES, PER_TICK = 20000, 30, 300
local MOVES = {
    [1] = { { "straight", false }, { "straight2", false }, { "turn1", false }, { "turn2", false },
            { "turn3", false }, { "turn2_left", false }, { "turn3_left", false },
            { "chicane_right", false }, { "chicane_left", false }, { "rise", false }, { "rise", true },
            { "wide_to_right", true, 2 }, { "wide_to_left", true, 2 } },
    [2] = { { "wide_straight", false }, { "wide_straight2", false }, { "wide_straight3", false },
            { "wide_turn2", false }, { "wide_turn3", false }, { "wide_turn2_left", false },
            { "wide_turn3_left", false }, { "wide_rise", false }, { "wide_rise", true },
            { "wide_to_right", false, 1 }, { "wide_to_left", false, 1 } },
}

-- Whether a square at road height y has anything in it: every column, the
-- road's cell and the two over it -- a parked kart is one cell.
local function taken(sx: number, sz: number, y: number): boolean
    for x = sx * 8, sx * 8 + 7 do
        for z = sz * 8, sz * 8 + 7 do
            for dy = 0, 2 do
                if core.get_block(x, y + dy, z) ~= 0 then
                    return true
                end
            end
        end
    end
    return false
end

-- A heap of { f, node } by f.
local function push(h, f: number, node)
    table.insert(h, { f, node })
    local i = #h
    while i > 1 do
        local up = i // 2
        if h[up][1] <= h[i][1] then break end
        h[up], h[i] = h[i], h[up]
        i = up
    end
end
local function pop(h)
    local top = h[1]
    local last = table.remove(h)
    if #h > 0 then
        h[1] = last
        local i = 1
        while true do
            local l, r, m = 2 * i, 2 * i + 1, i
            if l <= #h and h[l][1] < h[m][1] then m = l end
            if r <= #h and h[r][1] < h[m][1] then m = r end
            if m == i then break end
            h[i], h[m] = h[m], h[i]
            i = m
        end
    end
    return top[2]
end

local function key(st: Turtle, w: number?): string
    return `{st.gx} {st.gz} {st.t} {st.y} {w or ""}`
end

-- The pieces from `from` to `goal`, or nil. A coroutine: yields every PER_TICK.
-- From width w to the goal's width wg; `climb` lets the rises in.
local function search(from: Turtle, goal: Turtle, w: number, wg: number, climb: boolean): { { any } }?
    if MOVES[w] == nil or MOVES[wg] == nil then
        return nil
    end
    local function h(st: Turtle, sw: number): number
        local d = math.abs(st.gx - goal.gx) + math.abs(st.gz - goal.gz)
        return math.max(d, math.abs(st.y - goal.y)) + (if st.t == goal.t then 0 else 2)
            + (if sw == wg then 0 else 2)
    end
    local open = {}
    local best: { [string]: number } = { [key(from, w)] = 0 }
    push(open, h(from, w), { st = from, w = w, g = 0, n = 0, parent = nil, move = nil, sq = {} })
    local seen = 0
    local full: { [string]: boolean } = {}   -- taken(), once a square and height
    local function blocked(sx: number, sz: number, y: number): boolean
        local k = `{sx} {sz} {y}`
        if full[k] == nil then
            full[k] = taken(sx, sz, y)
        end
        return full[k]
    end
    while #open > 0 do
        local node = pop(open)
        if same(node.st, goal) and node.w == wg then
            local path = {}
            while node.parent ~= nil do
                table.insert(path, 1, node.move)
                node = node.parent
            end
            return path
        end
        seen += 1
        if seen > MAX_TURTLES then
            return nil
        end
        if seen % PER_TICK == 0 then
            coroutine.yield()
        end
        if node.n < MAX_PIECES then
            for _, m in MOVES[node.w] do
                if not climb and CLIMB[m[1]] ~= 0 then
                    continue
                end
                local _, after, rows = step(node.st, m[1], m[2])
                -- The squares it covers: not on the path so far, nothing in
                -- them at the height the road goes in at, and not the goal's.
                local ok = true
                local mine = {}
                for _, q in squares(node.st, rows) do
                    local k = `{q[1]} {q[2]}`
                    local n = node
                    while ok and n ~= nil do
                        if n.sq[k] then ok = false end
                        n = n.parent
                    end
                    ok = ok and not (q[1] == goal.gx and q[2] == goal.gz)
                         and not blocked(q[1], q[2], math.min(node.st.y, after.y))
                    if not ok then break end
                    mine[k] = true
                end
                if ok then
                    -- Squares of road, a square more for a turn, and four
                    -- for a rise: over the hills is the last way round.
                    local cost = node.g + SIZE[m[1]]
                        + (if CLIMB[m[1]] ~= 0 then 4 elseif PLAN[m[1]] == 1 then 0 else 1)
                    local aw = m[3] or node.w
                    local k = key(after, aw)
                    if best[k] == nil or cost < best[k] then
                        best[k] = cost
                        push(open, cost + 1.5 * h(after, aw),
                             { st = after, w = aw, g = cost, n = node.n + 1, parent = node, move = m, sq = mine })
                    end
                end
            end
        end
    end
    return nil
end

local closing: { [number]: thread } = {}

local function close_circuit(p: number, e: Edit)
    if e.out.g ~= 0 or e.out.b ~= 0 or e.start_in.g ~= 0 or e.start_in.b ~= 0 then
        core.chat(p, "Close circuit needs flat, unbanked road at both ends")
        return
    end
    core.chat(p, "Looking for a way back to the start...")
    closing[p] = coroutine.create(function()
        local path = if e.at.y == e.start.y then search(e.at, e.start, e.out.w, e.start_in.w, false) else nil
        if path == nil then
            path = search(e.at, e.start, e.out.w, e.start_in.w, true)
        end
        if path == nil then
            core.chat(p, `No way back to the start within {MAX_PIECES} pieces`)
            return
        end
        for i, m in path do
            if not extend(p, e, m[1], m[2]) then
                core.chat(p, `Closed up to piece {i - 1} of {#path}: something is in the way`)
                show(p)
                return
            end
        end
        local names = {}
        for _, m in path do
            table.insert(names, if m[2] then `{m[1]} (other way round)` else m[1])
        end
        core.chat(p, `Circuit closed with {#path} pieces`)
        core.log(`track_core: circuit closed with {#path} pieces: {table.concat(names, ", ")}`)
        core.ui_show(panel_of(p), false, p)
        editing[p] = nil
    end)
end

core.event.on("core:tick", function(_)
    for p, co in closing do
        local ok, err = coroutine.resume(co)
        if not ok then
            core.log(`track_core: closing the circuit failed: {err}`)
        end
        if coroutine.status(co) == "dead" then
            closing[p] = nil
        end
    end
end)

core.event.on("core:ui_click", function(ev)
    local p = ev.player
    if ev.panel ~= panel_of(p) then
        return
    end
    local e = editing[p]
    if ev.id == "close" or ev.id == "core:esc" or e == nil then
        core.ui_show(panel_of(p), false, p)
        return
    end
    if ev.id == "undo" then
        if not take_back(e) then
            core.chat(p, "Nothing put down with the editor to take back")
            return
        end
        show(p)
    elseif ev.id == "close_circuit" then
        if closing[p] == nil then
            close_circuit(p, e)
        end
    elseif ev.id == "pick" then
        -- B, from the panel: an open panel frees the cursor, and with it free
        -- the client tells the mods no key.
        core.event.emit("track_core:pick", { player = p, what = "toggle" })
    else
        local i = tonumber(string.match(ev.id, "^w(%d+)$") or "")
        local way = i and fitting(e)[i]
        if way ~= nil then
            if not extend(p, e, way.name, way.back) then
                core.chat(p, "Another piece of track is in the way of that one")
            end
            show(p)
        end
    end
end)


-- ------------------------------------------------- picking from afar
--
-- B with the editor in hand (track_game's half on the player's machine sends
-- it here as track_core:pick): the body stands where it is, the camera is held
-- from there on the open end, however far, and the piece on trial is put down
-- there for real. A and D take it back and try the next one that fits (and
-- goes down: one in the way is passed over), a click on the grid of every
-- piece that track_game draws instead of the hotbar puts that one on trial
-- ("sel n"), W keeps it and tries
-- the first at the new end, S takes the last kept one back, F closes the
-- circuit, B again leaves -- the piece on trial taken back. Both events are
-- declared here and track_game listens to pick_view: mods load by name when
-- `depends` says nothing, and track_core comes first.
core.event.define("track_core:pick", { fields = { player = "number", what = "string" }, cancellable = false })
core.event.define("track_core:pick_view", { fields = { player = "number", text = "string" }, cancellable = false })

-- Which of the fitting ways is on trial at the open end (0: none could go down).
local picking: { [number]: number } = {}

-- Every piece by its place in PIECES, which is how the grid on the player's
-- machine names them: "list <first> name,name,..." in messages under 512 bytes,
-- sent when picking starts.
local PIECE_NO: { [string]: number } = {}
for i, p in PIECES do
    PIECE_NO[p[1]] = i
end
local function send_list(p: number)
    local first, names, len = 1, {}, 0
    for i, piece in PIECES do
        if len + #piece[1] > 400 then
            core.event.emit("track_core:pick_view", { player = p, text = `list {first} {table.concat(names, ",")}` })
            first, names, len = i, {}, 0
        end
        table.insert(names, piece[1])
        len += #piece[1] + 1
    end
    core.event.emit("track_core:pick_view", { player = p, text = `list {first} {table.concat(names, ",")}` })
end

-- The camera from where the player's eyes are on the start of the piece on
-- trial (the open end when there is none), and what the HUD says.
local function aim(p: number, e: Edit)
    local i = picking[p]
    local at = e.at
    local label = "nothing fits the open end here: S takes the last piece back"
    local list, sel = fitting(e), 0
    if i > 0 then
        local last = e.put[#e.put]
        at = last[4]
        list = fitting({ out = last[5] } :: any)
        local way = list[i]
        sel = PIECE_NO[way.name]
        label = `{i} / {#list}  {way.name}{if way.back then " (other way round)" else ""}`
    end
    -- And which pieces fit there, for the grid to light.
    local fits, seen = {}, {}
    for _, way in list do
        if not seen[way.name] then
            seen[way.name] = true
            table.insert(fits, PIECE_NO[way.name])
        end
    end
    local tx, ty, tz = at.gx * 8 + 4, at.y + 1, at.gz * 8 + 4
    local x, y, z = core.player_pos(p)
    if x == nil then
        return
    end
    y += 1.62
    local dx, dy, dz = tx - x, ty - y, tz - z
    local len = math.max(math.sqrt(dx * dx + dy * dy + dz * dz), 1)
    local yaw, pitch = math.deg(math.atan2(dx, -dz)), math.deg(math.asin(-dy / len))
    core.event.emit("track_core:pick_view", {
        player = p,
        text = string.format("%.2f %.2f %.2f %.2f %.1f %.1f|%s|%d|%s", tx, ty, tz, math.min(len, 40), yaw, pitch,
                             label, sel, table.concat(fits, ",")),
    })
end

-- The first way from i on, stepping by dir, that goes down at the open end.
local function trial(p: number, e: Edit, i: number, dir: number)
    local list = fitting(e)
    for k = 0, #list - 1 do
        local j = (i - 1 + k * dir) % #list + 1
        if extend(p, e, list[j].name, list[j].back) then
            picking[p] = j
            return
        end
    end
    picking[p] = 0
end

-- A piece picked on the grid, by its place in PIECES: put on trial the way it
-- fits, or its other way when it is on trial already and fits both ways.
local function choose(p: number, e: Edit, n: number)
    local piece = PIECES[n]
    if piece == nil then
        return
    end
    local i = picking[p]
    if i > 0 then
        take_back(e)
    end
    local list = fitting(e)
    local now = list[i]
    local start = if now ~= nil and now.name == piece[1] then i else 0
    for k = 1, #list do
        local j = (start + k - 1) % #list + 1
        if list[j].name == piece[1] and extend(p, e, list[j].name, list[j].back) then
            picking[p] = j
            return
        end
    end
    core.chat(p, `{piece[1]} does not fit the open end`)
    if i > 0 then
        trial(p, e, i, 1)
    end
end

local function stop_picking(p: number, e: Edit?)
    if e ~= nil and (picking[p] or 0) > 0 then
        take_back(e)
    end
    picking[p] = nil
    core.event.emit("track_core:pick_view", { player = p, text = "off" })
end

core.event.on("track_core:pick", function(ev)
    local p, what = ev.player, ev.what
    local e = editing[p]
    if picking[p] == nil then
        if what ~= "toggle" then
            return
        end
        if e == nil then
            core.chat(p, "Put a piece down with the editor carried, or right click one with it, first")
            return
        end
        core.ui_show(panel_of(p), false, p)
        send_list(p)
        trial(p, e, 1, 1)
        aim(p, e)
        return
    end
    if e == nil or what == "toggle" then
        stop_picking(p, e)
        return
    end
    local i = picking[p]
    if what == "next" or what == "prev" then
        if i > 0 then
            take_back(e)
        end
        trial(p, e, if what == "next" then i + 1 else i - 1, if what == "next" then 1 else -1)
    elseif what == "put" and i > 0 then
        trial(p, e, 1, 1)
    elseif string.sub(what, 1, 4) == "sel " then
        choose(p, e, tonumber(string.sub(what, 5)) or 0)
    elseif what == "undo" then
        if i > 0 then
            take_back(e)
        end
        if not take_back(e) then
            core.chat(p, "Nothing put down with the editor to take back")
        end
        trial(p, e, 1, 1)
    elseif what == "close" then
        stop_picking(p, e)
        if closing[p] == nil then
            close_circuit(p, e)
        end
        return
    end
    aim(p, e)
end)
