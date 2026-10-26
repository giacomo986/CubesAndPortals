--!strict
-- yafps_core: the world of the yafps arena, a UAC hall broken open to Hell in
-- the manner of modern arena shooters (DESIGN.md M13). The blocks, the arena, and
-- the movement numbers; yafps_game has the guns and the demons.
--
-- The arena is a mapgen STAGE and not something built when the world is ready.
-- A stage's writes are lit once at the end of generation; the same thousand
-- wall cells written from core:world_ready would each pay for their own light.
-- Pictures: tools/make_fps_textures.py.

local FLOOR    = core.register_node("yafps_core:floor", 78, 80, 86)
local BASE     = core.register_node("yafps_core:base", 46, 40, 40)
local WALL     = core.register_node("yafps_core:wall", 92, 96, 104)
local LIGHT    = core.register_node("yafps_core:light", 215, 235, 255, true, 14)
local TRIM     = core.register_node("yafps_core:trim", 220, 170, 30)
local HELLROCK = core.register_node("yafps_core:hellrock", 120, 40, 20, true, 9)
local GATE     = core.register_node("yafps_core:gate", 200, 40, 20, true, 11)
local PILLAR   = core.register_node("yafps_core:pillar", 70, 72, 78)

-- Hell's sky: always this red, no sun. Light comes from the blocks alone, and
-- nothing that came through the gates burns.
core.world_sky(70, 14, 10)

-- What a player is here: the sandbox's robot (mods/robot, the same two files
-- linked here, as games/track does), and the arms seen in first person holding
-- yafps_game's guns (tools/make_fps_models.py). Here and not in yafps_game,
-- whose files go to players' machines as world code, which takes no links.
core.player_model(core.register_model("yafps_core:robot", "robot.gltf"))
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
    core.register_model("yafps_core:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
core.eye_height(1.5)   -- the robot's visor
core.register_model("yafps_core:robot_arms", "robot_arms.gltf")

-- Modern arena shooters are fast: 2.2 times the sandbox's walk, 1.6 times its jump
-- (walk 4.8, sprint 8, jump 8), as yafps asked of the original engine.
core.movement(10.5, 17.6, 14, 12.8, -25)

-- The engine's plaza is at this height and the spawn is in the middle of it,
-- so the base goes there: cells from GROUND up are air, GROUND - 1 is floor.
-- Every place below is given relative to (C, GROUND, C): x and z across, y up
-- from the floor, 0 the first cell of air.
local GROUND = 20
local C      = 64
local E      = 44   -- half the side of the whole base
local TOP    = 15   -- the rock the rooms are dug out of reaches this high

-- The world's two openings, when the server is started with --portals. Opening
-- 0 faces +Z in the entry room -- the mouth back to wherever this world's
-- --portal-to says -- and a joining player spawns four nodes in front of it.
-- Opening 1, the way out, is in the reactor (below).
core.set_portal(0, C - 3.5, GROUND + 1, C - 24.5)
-- Opening 1, the way out, faces +X in the reactor's south-east corner: out
-- past the Guardian, not back through the entry (keys: a way out is not the
-- way in).
core.set_portal(1, C + 34.6, GROUND + 1, C - 6.5)

-- The base, as yafps_game has to know it too (pickups and gates):
--
--   entry     x -8..4,   z -26..-16  the way in, a door north into the hub
--   hub       x -14..14, z -14..14   open to the red sky; a ledge five up along
--                                    the north and west walls, stairs to it, a
--                                    gate platform in the middle, four columns
--   foundry   x -12..12, z 20..36    north: a hellrock floor under catwalks five
--                                    up on both sides and a bridge across; a
--                                    corridor from the hub's floor and one from
--                                    its ledge to the west catwalk
--   reactor   x 20..36,  z -8..8     east: a column in the middle and a stair
--                                    going round it, up to ten
--   armoury   x -34..-20, z -10..10  west: low, crates
--   loops     from the armoury and the reactor, round to the foundry
core.register_mapgen("yafps_core:arena", function(ctx)
    if C + E < ctx.min_x or C - E > ctx.max_x or C + E < ctx.min_z or C - E > ctx.max_z then
        return
    end

    -- Every cell of a box, relative to (C, GROUND, C), bounds included, as far
    -- as this stage's columns reach.
    local function box(ax: number, bx: number, ay: number, by: number, az: number, bz: number, id: number)
        for x = math.max(C + ax, ctx.min_x), math.min(C + bx, ctx.max_x) do
            for z = math.max(C + az, ctx.min_z), math.min(C + bz, ctx.max_z) do
                for y = GROUND + ay, GROUND + by do
                    core.set_block(x, y, z, id)
                end
            end
        end
    end

    -- A room dug out h + 1 high, with lights in its ceiling every four cells.
    local function room(ax: number, bx: number, az: number, bz: number, h: number)
        box(ax, bx, 0, h, az, bz, 0)
        for x = ax + 2, bx - 1, 4 do
            for z = az + 2, bz - 1, 4 do
                box(x, x, h + 1, h + 1, z, z, LIGHT)
            end
        end
    end

    local function gate(x: number, z: number)
        box(x - 1, x + 1, -1, -1, z - 1, z + 1, GATE)
    end

    -- A flight of steps rising by one a cell from `from` towards `to` (one of
    -- x or z changing), `w` cells wide across.
    local function stairs(x: number, z: number, dx: number, dz: number, n: number, w: number)
        for k = 1, n do
            local sx, sz = x + dx * (k - 1), z + dz * (k - 1)
            local ex, ez = sx + (if dx == 0 then w - 1 else 0), sz + (if dz == 0 then w - 1 else 0)
            box(sx, ex, 0, k - 2, sz, ez, WALL)
            box(sx, ex, k - 1, k - 1, sz, ez, TRIM)
        end
    end

    -- Rock, floor under it, and clear air over it whatever the terrain put there.
    box(-E, E, -4, -2, -E, E, BASE)
    box(-E, E, -1, -1, -E, E, FLOOR)
    box(-E, E, 0, TOP, -E, E, WALL)
    box(-E, E, TOP + 1, TOP + 10, -E, E, 0)

    -- ---------------------------------------------------------------- hub
    box(-14, 14, 0, TOP + 10, -14, 14, 0)
    for i = -12, 12, 4 do
        box(i, i, 7, 7, 15, 15, LIGHT)
        box(i, i, 7, 7, -15, -15, LIGHT)
        box(15, 15, 7, 7, i, i, LIGHT)
        box(-15, -15, 7, 7, i, i, LIGHT)
    end
    for x = -14, 14 do
        for z = -14, 14 do
            if math.max(math.abs(x), math.abs(z)) == 11 then
                box(x, x, -1, -1, z, z, HELLROCK)
            end
        end
    end
    box(-14, 14, 0, 3, 11, 14, WALL)                 -- the ledge, north
    box(-14, 14, 4, 4, 11, 14, FLOOR)
    box(-10, 14, 4, 4, 11, 11, TRIM)
    box(-14, -11, 0, 3, -14, 10, WALL)               -- and west
    box(-14, -11, 4, 4, -14, 10, FLOOR)
    box(-11, -11, 4, 4, -14, 10, TRIM)
    box(-2, 2, 0, 3, 11, 14, 0)                      -- ways under them, north
    box(-14, -11, 0, 3, -1, 1, 0)                    -- and west
    stairs(-10, -6, 0, 1, 4, 2)                      -- up to the west ledge
    stairs(9, 9, -1, 0, 4, 2)                        -- and to the north one
    box(-3, 3, 0, 1, -3, 3, WALL)                    -- the gate platform
    box(-3, 3, 1, 1, -3, 3, TRIM)
    box(-2, 2, 1, 1, -2, 2, FLOOR)
    box(0, 0, 1, 1, 0, 0, GATE)
    for _, d in {{4, 0}, {-4, 0}, {0, 4}, {0, -4}} do
        local sx, sz = if d[1] == 0 then 1 else 0, if d[2] == 0 then 1 else 0
        box(d[1] - sx, d[1] + sx, 0, 0, d[2] - sz, d[2] + sz, TRIM)
    end
    for _, p in {{6, 6}, {6, -7}, {-7, 6}, {-7, -7}} do
        box(p[1], p[1] + 1, 0, 9, p[2], p[2] + 1, PILLAR)
        box(p[1], p[1] + 1, 10, 10, p[2], p[2] + 1, LIGHT)
    end
    gate(10, 3)
    gate(-6, 3)

    -- ---------------------------------------------------------------- entry
    room(-8, 4, -26, -16, 5)
    box(-3, 1, 0, 4, -15, -15, 0)
    box(-3, 1, -1, -1, -16, -14, TRIM)

    -- ---------------------------------------------------------------- foundry
    room(-12, 12, 20, 36, 11)
    box(-2, 2, 0, 4, 15, 19, 0)                      -- from the hub's floor
    box(-10, -8, 5, 8, 15, 19, 0)                    -- from its ledge
    box(-5, 5, -1, -1, 22, 34, HELLROCK)
    for _, sx in {-1, 1} do                          -- the catwalks
        local a, b = if sx < 0 then -12 else 8, if sx < 0 then -8 else 12
        local edge = if sx < 0 then -8 else 8
        box(a, b, 4, 4, 20, 36, FLOOR)
        box(edge, edge, 4, 4, 20, 36, TRIM)
        for _, z in {23, 31} do
            box(edge, edge, 0, 3, z, z, PILLAR)
        end
    end
    box(-7, 7, 4, 4, 27, 29, FLOOR)                  -- and the bridge
    box(-7, 7, 4, 4, 27, 27, TRIM)
    box(-7, 7, 4, 4, 29, 29, TRIM)
    stairs(6, 21, 0, 1, 4, 2)
    gate(0, 32)

    -- ---------------------------------------------------------------- reactor
    room(20, 36, -8, 8, 13)
    box(15, 19, 0, 3, -1, 1, 0)
    box(27, 29, 0, 13, -1, 1, PILLAR)
    box(27, 29, 4, 4, -1, 1, LIGHT)
    box(27, 29, 9, 9, -1, 1, LIGHT)
    -- A stair round the column: its height goes with the angle, from one by the
    -- door to ten, and a gap where it would come back down.
    for x = 23, 33 do
        for z = -5, 5 do
            local d = math.max(math.abs(x - 28), math.abs(z))
            if d >= 2 then
                local t = ((math.atan2(z, x - 28) - math.pi) % (2 * math.pi)) / (2 * math.pi)
                if t < 0.92 then
                    local top = math.min(10, 1 + math.floor(t / 0.92 * 10))
                    if top > 1 then
                        box(x, x, 0, top - 2, z, z, WALL)
                    end
                    box(x, x, top - 1, top - 1, z, z, TRIM)
                end
            end
        end
    end
    gate(34, 7)
    gate(22, -7)
    -- The way out's frame round opening 1 (its hole at 34, -7, facing +X).
    box(33, 33, 0, 3, -8, -6, PILLAR)
    box(34, 34, 0, 3, -8, -8, PILLAR)
    box(34, 34, 0, 3, -6, -6, PILLAR)
    box(34, 34, 2, 3, -7, -7, LIGHT)
    box(34, 34, 0, 1, -7, -7, 0)

    -- ---------------------------------------------------------------- armoury
    room(-34, -20, -10, 10, 5)
    box(-19, -15, 0, 3, -1, 1, 0)
    box(-26, -25, 0, 1, -5, -4, TRIM)
    box(-27, -26, 0, 0, 4, 5, TRIM)
    box(-33, -32, 0, 2, 3, 4, PILLAR)
    box(-23, -22, 0, 1, -9, -9, WALL)
    gate(-24, 6)
    gate(-31, -6)

    -- ---------------------------------------------------------------- loops
    box(-28, -26, 0, 3, 11, 28, 0)
    box(-28, -13, 0, 3, 26, 28, 0)
    box(26, 28, 0, 3, 9, 28, 0)
    box(13, 28, 0, 3, 26, 28, 0)
    for z = 13, 25, 4 do
        box(-27, -27, 4, 4, z, z, LIGHT)
        box(27, 27, 4, 4, z, z, LIGHT)
    end
    for x = -24, 24, 4 do
        if math.abs(x) >= 14 then
            box(x, x, 4, 4, 27, 27, LIGHT)
        end
    end
end)
