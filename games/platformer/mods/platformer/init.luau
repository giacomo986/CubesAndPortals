--!strict
-- platformer: a platform game seen from the side, in the manner of the
-- eighties' ones. Two levels, a bonus room, power-ups, their rules, and the
-- camera.
--
-- A player arrives in a ROOM (the doorway home behind them) whose far wall
-- is glass: through it, 21 nodes off and side on, is the first level -- a
-- walkway three deep running to the right (-x), with its pits, pipes, coins
-- and creatures, up to a flag and a castle. A door in the room's left wall
-- gives onto a bridge with a HOLE in it and a green pipe. The hole is a
-- portal that scales (core.portal_scale): whoever drops in falls out of the
-- sky over the start of 1-1 at half their height -- small, as the game wants
-- them -- while their machine swings the camera out to the side
-- (core.side_view). The pipe leads to 1-2 once 1-1 has been cleared. In a
-- level A and D walk, W or the space bar jumps, S crouches, Shift runs, F or
-- the left button throws a fireball, E gives up and goes back to the room.
--
--   small, big, fire: a player starts small; a mushroom out of a prize block
--   makes them big, a flower (out of the same blocks, to whoever is big
--   already) gives them fireballs; hit while big or fire they go back to
--   small and nothing hits them for two seconds; hit while small, a life
--   three lives; a fall or a hit while small costs one and puts the body back,
--   small, at the last flag passed (the start, or the one half way); none
--   left is the room
--   a sprout (a cactus on two feet) and a buzzer (a bug that flies up and
--   down) are killed by landing on them, which bounces; a spikeshell is not
--   -- it is jumped over; a fireball kills any of them
--   a prize block bumped from below with the head gives a coin, once, or the
--   power-up it holds
--   the tall pipe of 1-1 (a step before it) goes down, with S on top of it:
--   the body sinks down the pipe through the ground into a bonus room full
--   of coins, the camera after it; the pipe lying at the room's far end goes
--   up the same way, out of the next pipe of the level
--   the pole at a level's end stops the clock: 1-1's castle leads on to
--   1-2, with the lives, the coins and the size as they are; 1-2's to the room
--
-- Everything is measured against platformer_core's body at full size: a jump
-- is 2.4 up and 4.4 along at a walk, 7.2 at a run. A small body (scale 0.5,
-- core.body_scale) is given numbers twice as big (core.body_movement), which
-- shrink with it, so it runs and jumps the same. The levels lie on the plane
-- z = 0.5 (the cells z -1 .. 1; prize blocks, coins and pipe shafts in the
-- plane's own cell only), and `s` below is how far along it a thing is: x is
-- START_X - s, so that the picture's right is the way on. 1-2 starts at
-- s = 200.
--
-- The pipes, the mushroom, the flower and the fireball are models
-- (tools/make_platformer_models.py, Blender) and the creatures Meshy's
-- (tools/meshy_generate.py, tools/meshy_prop.py), all in platformer_core; the
-- pictures and the sounds are tools/make_platformer_assets.py's.
--
-- `side = both`: the server builds the room and the levels and keeps the
-- rules; a player's machine moves the camera and draws the count.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local G       = 20      -- the first cell of air over every floor
local PLANE   = 0.5     -- the levels' plane, z
local START_X = 10      -- the cell x of s = 0
local VOID_Y  = 11      -- below this a body has fallen
local SMALL   = 0.5     -- a small body's scale

-- Filled in below by whichever halves this process runs.
local heard: (string) -> () = function(_) end
local asked: (number, string) -> () = function(_, _) end

-- What the server tells one player. Sent when there is a network between
-- them, handed straight to the drawing half when this process is both.
local function tell(player: number, text: string)
    if drawing then heard(text) else core.net.send(player, text) end
end

-- ===================================================================== server

if serving then
    core.net.open()

    local AIR = 0
    local function node(name: string, rgb: { number }, light: number?, solid: boolean?): number
        return core.register_node(`platformer:{name}`, rgb[1], rgb[2], rgb[3], solid ~= false, light or 0, "", 0, 0, "stone")
    end
    -- The levels' blocks give a little light of their own: the camera looks
    -- at the faces the sun never reaches (it leans +z, daylight.cpp), which
    -- would be in shade at every hour, and a flat picture suits the game.
    local L = 13
    local GRASS  = node("grass", { 96, 176, 64 }, L)
    local DIRT   = node("dirt", { 150, 92, 50 }, L)
    local BRICK  = node("brick", { 190, 92, 44 }, L)
    local BLUE   = node("blue_brick", { 52, 96, 170 }, L)      -- 1-2 and the bonus room
    local CAVE   = node("cave", { 40, 54, 104 }, 11)           -- the wall behind them
    local PRIZE  = node("prize", { 244, 196, 40 }, 13)
    local SPENT  = node("spent", { 150, 104, 60 }, L)   -- a prize block once bumped
    local PIPE   = node("pipe", { 40, 168, 60 }, L)     -- the one on the bridge, of blocks
    local PIPEIN = node("pipe_dark", { 12, 40, 18 })
    local STONE  = node("stone", { 150, 150, 156 }, L)
    local WALL   = node("wall", { 150, 150, 156 })
    local FLOOR  = node("floor", { 112, 96, 84 })
    local FRAME  = node("frame", { 34, 34, 36 })
    local LAMP   = node("lamp", { 255, 240, 200 }, 14)
    local POLE   = node("pole", { 230, 230, 230 }, L)
    local FLAG   = node("flag", { 220, 40, 40 }, 13)
    local HALF   = node("half_flag", { 60, 120, 230 }, 13)
    local CLOUD  = node("cloud", { 245, 248, 252 }, 15)
    local HILL   = node("hill", { 70, 150, 70 }, 10)
    local GLASS  = node("glass", { 200, 230, 240 })
    core.node_glass(GLASS, 60)
    -- A coin: not solid, a thin disc standing in its cell across the way, lit.
    local COIN = node("coin", { 255, 214, 40 }, 10, false)
    core.node_box(COIN, 0.25, 0.15, 0.4, 0.75, 0.85, 0.6)
    -- A pipe in a level is round and two cells wide: each piece is one cell's
    -- model, built about that cell's +x wall, and every other cell of the pipe
    -- holds a block that stops a body and is not seen. Each has a twin that
    -- stops nothing, which a shaft is made of while a body goes through it.
    local function hidden(name: string, solid: boolean): number
        local id = node(name, { 40, 168, 60 }, 0, solid)
        core.node_glass(id, 0)
        return id
    end
    local HIDDEN, HIDDEN_OPEN = hidden("hidden", true), hidden("hidden_open", false)
    local function piece(name: string, solid: boolean): number
        local id = node(if solid then name else `{name}_open`, { 40, 168, 60 }, 0, solid)
        core.node_model(id, core.model_id(`platformer_core:{name}`))
        return id
    end
    local PIPE_BODY, PIPE_TOP = piece("pipe_body", true), piece("pipe_top", true)
    local PIPE_HANG, PIPE_SIDE = piece("pipe_hang", true), piece("pipe_side", true)
    local OPEN: { [number]: number } = {
        [HIDDEN] = HIDDEN_OPEN, [PIPE_BODY] = piece("pipe_body", false),
        [PIPE_TOP] = piece("pipe_top", false), [PIPE_HANG] = piece("pipe_hang", false),
    }
    -- What a creature, a power-up or a fireball stands on.
    local function solid_at(x: number, y: number): boolean
        local b = core.get_block(math.floor(x), math.floor(y), 0)
        return b ~= AIR and b ~= COIN and b ~= HIDDEN_OPEN and core.node_solid(b) == 1
    end

    -- Nothing of the engine's terrain: the room and the levels hang in the sky.
    core.mapgen_core(false)

    -- ---------------------------------------------------------------- shapes
    -- Boxes of cells, ends included: { x0, y0, z0, x1, y1, z1, block }.
    local BOXES: { { number } } = {}
    local function box(x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: number)
        table.insert(BOXES, { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1),
                               math.max(x0, x1), math.max(y0, y1), math.max(z0, z1), b })
    end

    -- What is being laid out: which level, and where its s = 0 is.
    local level, O = 1, 0
    local function X(s: number): number return START_X - (O + s) end
    -- A piece of the level from s0 to s1 along it, y0 to y1, the walkway's
    -- three cells deep unless said.
    local function lev(s0: number, s1: number, y0: number, y1: number, b: number, z0: number?, z1: number?)
        box(X(s1), y0, z0 or -1, X(s0), y1, z1 or 1, b)
    end
    -- Ground from s0 to s1: grass over two of earth, or all of `b`.
    local function ground(s0: number, s1: number, b: number?)
        lev(s0, s1, G - 3, G - 2, b or DIRT)
        lev(s0, s1, G - 1, G - 1, b or GRASS)
    end
    -- A pipe two wide standing from y0 to y1 at s, s + 1 (its middle is x =
    -- X(s)): solid and unseen all three deep, the models in the plane's cell
    -- of s + 1. `low` is what the lowest cell is: a body, or the lip of one
    -- hanging from a ceiling. Returns the cells of the plane, for a shaft.
    local function shaft(s: number, y0: number, y1: number, low: number?): { { number } }
        lev(s, s + 1, math.max(y0, G), y1, HIDDEN)
        local cells = {}
        for y = y0, y1 do
            local top = if y == y1 then PIPE_TOP elseif y == y0 and low then low else PIPE_BODY
            lev(s, s, y, y, HIDDEN, 0, 0)
            lev(s + 1, s + 1, y, y, top, 0, 0)
            table.insert(cells, { X(s), y, HIDDEN })
            table.insert(cells, { X(s + 1), y, top })
        end
        return cells
    end
    local function pipe(s: number, h: number)
        shaft(s, G, G + h - 1)
    end
    -- Prize blocks from s0 to s1 at y: one deep, in the plane's own cell. A
    -- power-up's holds a mushroom or a flower instead of a coin.
    local POWERS: { [string]: boolean } = {}
    local function prizes(s0: number, s1: number, y: number)
        lev(s0, s1, y, y, PRIZE, 0, 0)
    end
    local function power(s: number, y: number)
        prizes(s, s, y)
        POWERS[`{X(s)},{y}`] = true
    end
    -- Coins in the plane's own cell, from s0 to s1 at height y: { x, y, z, level }.
    local COINS: { { number } } = {}
    local function coins(s0: number, s1: number, y: number)
        for s = s0, s1 do table.insert(COINS, { X(s), y, 0, level }) end
    end
    -- Stairs of n steps, rising (up = true) or falling along s.
    local function stairs(s: number, n: number, up: boolean, b: number?)
        for k = 1, n do
            local h = if up then k else n + 1 - k
            lev(s + k - 1, s + k - 1, G, G + h - 1, b or STONE)
        end
    end
    -- The creatures: { level, kind, x of one end of its beat, x of the other, y }.
    local SPROUT, SPIKE, BUZZER = 1, 2, 3
    local FOES: { { number } } = {}
    local function foe(kind: number, s0: number, s1: number, y: number?)
        table.insert(FOES, { level, kind, X(s1) + 0.5, X(s0) + 0.5, y or G })
    end
    -- The pole, its flag, and the castle behind it; a wall at the very end.
    local function finish(s: number)
        lev(s, s, G, G + 9, POLE, 0, 0)
        lev(s, s, G, G, STONE)
        lev(s + 1, s + 1, G + 8, G + 9, FLAG, 0, 0)
        lev(s + 4, s + 9, G, G + 4, BRICK)
        lev(s + 5, s + 8, G + 5, G + 6, BRICK)
        lev(s + 6, s + 7, G, G + 1, PIPEIN, -1, -1)
        lev(s + 11, s + 11, G - 3, G + 10, BRICK)
    end

    -- ------------------------------------------------------------- the room
    -- Inside x -9 .. 7, z -29 .. -22, four high. The doorway home is in the
    -- back wall (z -30) facing +z -- the level -- and the wall in front of
    -- whoever arrives (z -21) is glass.
    box(-10, G - 1, -30, 8, G - 1, -21, FLOOR)
    box(-10, G + 4, -30, 8, G + 4, -21, WALL)
    box(-10, G, -30, 8, G + 3, -30, WALL)
    box(-10, G, -30, -10, G + 3, -21, WALL)
    box(8, G, -30, 8, G + 3, -21, WALL)
    box(-9, G, -21, 7, G + 3, -21, GLASS)
    for x = -8, 6, 2 do box(x, G + 4, -27, x, G + 4, -24, LAMP) end
    -- The doorway: a black frame one into the room, the hole at (0, G, -29).
    for v = 0, 3 do
        for u = -2, 2 do
            local hole = u == 0 and v < 2
            box(u, G + v, -29, u, G + v, -29, if hole then AIR elseif v == 3 and u == 0 then LAMP else FRAME)
        end
    end
    -- The door in the left wall (+x, the picture's left), the bridge outside
    -- it, a hole two by two in the bridge -- yellow round its rim, a pit of
    -- three under it, the portal across its middle -- and at the bridge's
    -- end a pipe, its mouth two wide and two high under a blue lintel, to 1-2.
    local HOLE = { 11.0, G - 2.5, -21.0 }    -- the floor portal's centre
    local PIPE2 = { 15, 16 }                  -- the cells x of the pipe's mouth
    box(8, G, -26, 8, G + 1, -25, AIR)
    box(9, G - 1, -27, 17, G - 1, -19, WALL)
    box(9, G - 4, -23, 12, G - 1, -19, PRIZE)
    box(10, G - 3, -22, 11, G - 1, -21, AIR)
    box(PIPE2[1] - 1, G, -19, PIPE2[2] + 1, G + 3, -18, PIPE)
    box(PIPE2[1], G, -19, PIPE2[2], G + 1, -19, AIR)
    box(PIPE2[1], G, -18, PIPE2[2], G + 1, -18, PIPEIN)
    box(PIPE2[1], G + 2, -19, PIPE2[2], G + 2, -19, HALF)
    -- Where the hole lets out: a ceiling portal half its size high over the
    -- start of 1-1, a cloud over it.
    local SKY = { START_X + 0.5, G + 18.5, PLANE }
    box(START_X, G + 19, 0, START_X, G + 19, 0, CLOUD)

    -- ------------------------------------------------------------ level 1-1
    level, O = 1, 0
    lev(-3, -3, G - 3, G + 10, BRICK)             -- the wall behind the start
    ground(-2, 22)
    coins(6, 8, G + 1)
    prizes(12, 12, G + 3)                         -- a row to bump the head on,
    power(13, G + 3)                              -- a power-up in its middle
    prizes(14, 14, G + 3)
    lev(16, 19, G + 1, G + 1, BRICK)              -- a ledge two up
    coins(16, 19, G + 2)
    pipe(21, 1)

    ground(26, 30)                                -- a pit of three: a walk's jump
    coins(23, 25, G + 2)
    ground(34, 52)                                -- and another
    foe(SPROUT, 36, 43)
    lev(45, 45, G, G, STONE)                      -- a step up to a pipe three high,

    -- The bonus room, under the ground from s 34 to 52: blue bricks all round
    -- but for the side the camera looks in by, seven high inside, coins in
    -- three rows. The pipe three high comes down through the ground and hangs
    -- from its ceiling; the one lying at its far end turns up through the
    -- ground and comes out as the next pipe of the level, two high. Where
    -- either goes through the ground the front of the ground is cut away, so
    -- the camera sees the body go down and up it.
    local BY = G - 13                             -- the room's first cell of air
    lev(34, 52, BY - 1, G - 4, BLUE)
    lev(34, 52, BY - 1, G - 4, CAVE, 2, 2)
    lev(35, 51, BY, G - 7, AIR)
    for _, k in { 36, 38, 40, 42, 44 } do lev(k, k, G - 6, G - 6, LAMP) end
    for row = 1, 3 do coins(36, 44, BY + row) end
    local DOWN = shaft(46, G - 7, G + 2, PIPE_HANG)
    lev(46, 47, G - 6, G - 1, AIR, -1, -1)
    lev(49, 49, BY + 1, BY + 1, HIDDEN, 0, 0)     -- the lying mouth, facing back
    lev(49, 49, BY, BY, PIPE_SIDE, 0, 0)
    local UP = shaft(50, BY, G + 1)
    lev(50, 51, G - 6, G - 1, AIR, -1, -1)
    coins(50, 51, G + 3)
    local DOWN_X, DOWN_TOP = X(46), G + 3          -- the middle of the pipe going down, and its top
    local UP_X, UP_TOP, UP_MOUTH = X(50), G + 2, 48.4   -- the mouth's face is at s 48.5

    lev(55, 56, G - 1, G - 1, BRICK)              -- a pit of eight, two ledges in it
    lev(58, 59, G, G, BRICK)
    coins(55, 56, G + 1)
    coins(58, 59, G + 2)

    ground(61, 90)
    lev(62, 62, G, G + 3, POLE, 2, 2)             -- the flag half way, behind the walkway
    lev(63, 63, G + 3, G + 3, HALF, 2, 2)
    foe(SPROUT, 65, 69)
    lev(70, 75, G + 1, G + 1, BRICK)
    coins(70, 75, G + 2)
    power(72, G + 4)
    prizes(73, 73, G + 4)
    foe(SPIKE, 76, 80)
    pipe(82, 2)

    stairs(91, 5, true)                           -- up five, a pit of four, down five
    ground(91, 95)
    coins(96, 99, G + 7)
    foe(BUZZER, 96, 99, G + 5.5)
    stairs(100, 5, false)
    ground(100, 142)
    foe(SPROUT, 106, 115)
    coins(108, 112, G + 1)

    stairs(118, 7, true)                          -- the last stairs, and the pole
    lev(125, 125, G, G + 6, STONE)
    finish(132)

    -- Behind the walkway, for the picture only: hills and clouds.
    for _, h in { { 4, 3 }, { 28, 2 }, { 40, 4 }, { 66, 3 }, { 84, 2 }, { 110, 4 } } do
        for k = 0, h[2] - 1 do
            lev(h[1] + k, h[1] + 2 * h[2] - 2 - k, G + k - 3, G + k, HILL, 4, 4)
        end
    end
    for _, c in { { 2, 8 }, { 18, 10 }, { 33, 7 }, { 50, 9 }, { 64, 11 }, { 80, 8 }, { 97, 12 }, { 116, 9 }, { 134, 11 } } do
        lev(c[1], c[1] + 4, G + c[2], G + c[2], CLOUD, 7, 7)
        lev(c[1] + 1, c[1] + 3, G + c[2] + 1, G + c[2] + 1, CLOUD, 7, 7)
    end

    -- ------------------------------------------------------------ level 1-2
    -- Underground: blue bricks underfoot and overhead, a dark wall behind.
    level, O = 2, 200
    lev(-3, -3, G - 3, G + 8, BLUE)               -- the wall behind the start
    lev(-3, 112, G + 7, G + 8, BLUE)              -- the roof, as far as the way out
    lev(-3, 118, G - 3, G + 8, CAVE, 2, 2)
    for k = 2, 110, 6 do lev(k, k, G + 7, G + 7, LAMP) end   -- lamps in the roof
    ground(-2, 20, BLUE)
    pipe(-2, 2)
    prizes(8, 8, G + 3)
    power(9, G + 3)
    prizes(10, 11, G + 3)
    coins(8, 11, G + 1)
    foe(SPROUT, 12, 18)

    ground(24, 40, BLUE)                          -- a pit of three, then steps to drop off
    stairs(30, 3, true, BLUE)
    coins(33, 35, G + 3)
    foe(SPIKE, 34, 39)

    lev(43, 43, G - 1, G - 1, BRICK)              -- a pit of seven, two ledges and a buzzer
    lev(45, 46, G, G, BRICK)
    coins(43, 43, G + 1)
    coins(45, 46, G + 2)
    foe(BUZZER, 42, 47, G + 3.2)

    ground(48, 80, BLUE)
    lev(50, 50, G, G + 3, POLE, 1, 1)             -- the flag half way, against the wall
    lev(51, 51, G + 3, G + 3, HALF, 1, 1)
    lev(56, 62, G + 3, G + 6, BLUE)               -- a low stretch, coins all along it
    coins(56, 62, G + 1)
    foe(SPROUT, 64, 70)
    pipe(72, 2)
    foe(SPIKE, 74, 79)
    lev(80, 80, G, G, BLUE)                       -- a step, and a pit of four off it

    ground(85, 143, BLUE)
    lev(88, 89, G + 1, G + 1, BRICK)              -- three ledges climbing
    lev(91, 92, G + 2, G + 2, BRICK)
    lev(94, 96, G + 3, G + 3, BRICK)
    coins(88, 89, G + 2)
    coins(91, 92, G + 3)
    coins(94, 96, G + 4)
    foe(BUZZER, 90, 97, G + 1.4)
    prizes(101, 101, G + 3)
    power(102, G + 3)
    prizes(103, 103, G + 3)
    foe(SPROUT, 100, 108)
    foe(SPIKE, 109, 116)

    stairs(120, 6, true, BLUE)                    -- out into the open, and the pole
    lev(126, 126, G, G + 5, BLUE)
    finish(132)
    for _, c in { { 121, 10 }, { 134, 12 } } do
        lev(c[1], c[1] + 4, G + c[2], G + c[2], CLOUD, 7, 7)
        lev(c[1] + 1, c[1] + 3, G + c[2] + 1, G + c[2] + 1, CLOUD, 7, 7)
    end

    -- name, where it starts, the flag half way and the pole, all as s.
    local LEVELS = {
        { name = "1-1", start = 0, half = 62, goal = 132 },
        { name = "1-2", start = 200, half = 250, goal = 332 },
    }

    core.register_mapgen("platformer:level", function(ctx)
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
        for _, c in COINS do
            if c[1] >= ctx.min_x and c[1] <= ctx.max_x and c[3] >= ctx.min_z and c[3] <= ctx.max_z then
                core.set_block(c[1], c[2], c[3], COIN)
            end
        end
    end)

    -- Portal 0 in the room's doorway (facing +z, the spawn four in front of
    -- it). Started with a --portal-to (tools/platformer.sh) that mouth is the
    -- way home. Started from the rooms file, as the game does it, the world
    -- has no mouth of its own: the doorway is said to the other rooms as
    -- "door" (core.room_portal_set), which the gate on the sandbox's plaza
    -- links to (mods/gates). There is no second way out yet.
    core.set_portal(0, 0.5, G + 1, -28.4)
    local door_said = false
    local function say_door()
        door_said = true
        for k = 0, 1 do
            if core.portal_room(k) == "" then core.portal_park(k) end
        end
        if core.portal_room(0) == "" then
            door_said = core.room_portal_set("door", 0.5, G + 1, -28.5, 0, 1, 0.5, 1.0) == 1
        end
    end
    -- The hole: a floor two by two facing up, linked to a ceiling one by one
    -- facing down, both scaling. A room's portals exist a few ticks after its
    -- first, so asked for until they open.
    local hole_open = false
    local function open_hole()
        local a = core.portal_open(HOLE[1], HOLE[2], HOLE[3], 2, 1, 0, 1.0, 1.0)
        local b = core.portal_open(SKY[1], SKY[2], SKY[3], 2, -1, 0, 0.5, 0.5)
        if a < 0 or b < 0 then return end
        core.portal_link(a, b)
        core.portal_scale(a, true)
        core.portal_scale(b, true)
        core.log(`platformer: the hole {a} -> {b}`)
        hole_open = true
    end

    core.event.on("core:pre_break", function(ev) ev.cancel = true end)
    core.event.on("core:pre_place", function(ev) ev.cancel = true end)

    -- ------------------------------------------------------------- shafts
    -- A pipe a body goes through: its cells in the plane made into the twins
    -- that stop nothing while anybody is in it, and put back after.
    local using: { [{ { number } }]: number } = {}
    local function shaft_open(cells: { { number } })
        using[cells] = (using[cells] or 0) + 1
        if using[cells] > 1 then return end
        for _, c in cells do core.set_block(c[1], c[2], 0, OPEN[c[3]]) end
    end
    local function shaft_close(cells: { { number } })
        using[cells] = math.max(0, (using[cells] or 1) - 1)
        if using[cells] > 0 then return end
        for _, c in cells do core.set_block(c[1], c[2], 0, c[3]) end
    end

    -- ---------------------------------------------------------------- rules
    local ROOM = { 0.5, G, -25.5 }                         -- where a body is put back in the room
    local FOE, ITEM, FIRE = "platformer:foe", "platformer:item", "platformer:fire"
    core.ecs.component(FOE, { "a", "b", "dir", "dead", "kind", "base", "t" })
    core.ecs.component(ITEM, { "kind", "vx", "vy", "rise", "top" })
    core.ecs.component(FIRE, { "vx", "vy", "t", "owner" })
    local function model(name: string): number return core.model_id(`platformer_core:{name}`) end
    -- model, pace, how tall, whether landing on it kills it
    local KINDS = {
        [SPROUT] = { model = model("sprout"), pace = 1.6, tall = 1.2, soft = true },
        [SPIKE]  = { model = model("spikeshell"), pace = 1.0, tall = 1.0, soft = false },
        [BUZZER] = { model = model("buzzer"), pace = 2.2, tall = 0.9, soft = true },
    }
    local MUSHROOM, FLOWER = 1, 2
    local ITEM_MODEL = { [MUSHROOM] = model("mushroom"), [FLOWER] = model("flower") }

    -- power: 0 small, 1 big, 2 fire. scaling: the steps of a body growing or
    -- shrinking still to come, { when, scale }. shaft: "down" or "up" while
    -- the body goes through a pipe.
    type Run = { level: number, lives: number, coins: number, flag: number, t: number, y: number, vy: number,
                 safe: number, power: number, scale: number, scaling: { { number } }, gun: number,
                 shaft: string?, won: number? }
    local runs: { [number]: Run } = {}        -- who is in a level
    local taken: { [number]: boolean } = {}   -- the coins taken, by index
    local bumped: { [string]: { number } } = {}   -- the prize blocks spent: "x,y" -> { x, y, level }
    local foes: { [number]: number } = {}     -- the creatures alive: id -> level
    local items: { [number]: number } = {}    -- power-ups out: id -> level
    local fires: { [number]: number } = {}    -- fireballs: id -> who threw it
    local shown: { [number]: number } = {}    -- which way in a player on the bridge was last told of
    local clock = 0.0
    local timed = false

    -- A body that size, given the numbers that make it run and jump as a
    -- body at full size does (core.body_movement's shrink with it).
    local function set_scale(p: number, r: Run?, scale: number)
        -- Only when it changes: out of the hole the portal has made the body
        -- small already, and saying it again there put it back where the
        -- server last had it, in the hole.
        if math.abs(core.body_size(p) - scale) > 0.001 then core.body_scale(p, scale) end
        core.body_movement(p, 5.5 / scale, 9 / scale, 0, 12 / scale, -30 / scale)
        if r then r.scale = scale end
    end
    -- Growing and shrinking as the old consoles did: a few flickers between
    -- the two sizes, then the new one.
    local function resize(r: Run, to: number)
        local from = r.scale
        local mid = (from + to) / 2
        r.scaling = {}
        for i, sc in { mid, from, mid, from, mid, to } do
            table.insert(r.scaling, { clock + (i - 1) * 0.09, sc })
        end
    end

    local function flag_at(n: number, flag: number): (number, number, number)
        local lv = LEVELS[n]
        return START_X - (if flag == 1 then lv.start else lv.half) + 0.5, G, PLANE
    end

    local function count(p: number, r: Run)
        tell(p, `hud {r.lives} {r.coins} {r.power}`)
    end

    local function busy(n: number): boolean
        for _, r in runs do
            if r.level == n then return true end
        end
        return false
    end

    -- A level as new: every coin back, every prize block whole, every
    -- creature on its beat, no power-up out.
    local function reset_level(n: number)
        for i in taken do
            local c = COINS[i]
            if c[4] == n then
                core.set_block(c[1], c[2], c[3], COIN)
                taken[i] = nil
            end
        end
        for key, b in bumped do
            if b[3] == n then
                core.set_block(b[1], b[2], 0, PRIZE)
                bumped[key] = nil
            end
        end
        for id, of in foes do
            if of == n then
                core.ecs.kill(id)
                foes[id] = nil
            end
        end
        for id, of in items do
            if of == n then
                core.ecs.kill(id)
                items[id] = nil
            end
        end
        for _, f in FOES do
            if f[1] == n then
                local id = core.ecs.spawn(FOE, f[4], f[5], PLANE)
                if id ~= nil then   -- nil with no session table: entities need a listening server
                    core.ecs.set(id, "a", f[3])
                    core.ecs.set(id, "b", f[4])
                    core.ecs.set(id, "dir", -1)
                    core.ecs.set(id, "dead", 0)
                    core.ecs.set(id, "kind", f[2])
                    core.ecs.set(id, "base", f[5])
                    core.ecs.set(id, "t", 0)
                    core.ecs.visible(id, true)
                    core.ecs.model(id, KINDS[f[2]].model)
                    core.ecs.box(id, 0.5, KINDS[f[2]].tall)
                    foes[id] = n
                end
            end
        end
    end

    -- Into a level at its start: from the room with three lives and small,
    -- or on from the level before with what the run has. `place`: put there
    -- (a pipe, the pole); without it the body is there already (the hole).
    local function start(p: number, n: number, from: Run?, place: boolean)
        if not busy(n) then reset_level(n) end
        local x, y, z = flag_at(n, 1)
        local r: Run = { level = n, lives = if from then from.lives else 3, coins = if from then from.coins else 0,
                         flag = 1, t = 0, y = y, vy = 0, safe = clock + 1, power = if from then from.power else 0,
                         scale = if from then from.scale else SMALL, scaling = {}, gun = 0 }
        runs[p] = r
        set_scale(p, r, r.scale)
        if place then
            core.player_place(p, x, y, z, -1, 0, 0)
            core.play_sound("platformer:pipe", x, y + 1, z)
        end
        core.player_spawn(p, x, y, z, true)
        tell(p, `in {LEVELS[n].name}`)
        count(p, r)
    end

    local function leave(p: number, why: string)
        local r = runs[p]
        if r and r.shaft == "down" then shaft_close(DOWN) elseif r and r.shaft == "up" then shaft_close(UP) end
        runs[p] = nil
        set_scale(p, nil, 1)
        core.player_spawn(p, 0, 0, 0, false)
        core.player_place(p, ROOM[1], ROOM[2], ROOM[3], 0, 0, 1)
        tell(p, "out")
        if why ~= "" then core.notice(p, why) end
    end

    -- A life lost: back to the last flag, small, or out with none left.
    local function lose(p: number, r: Run)
        r.lives -= 1
        local px, py, pz = core.player_pos(p)
        if px ~= nil then
            core.play_sound(if r.lives <= 0 then "platformer:over" else "platformer:hurt", px, math.max(py, VOID_Y + 4), pz)
        end
        if r.shaft == "down" then shaft_close(DOWN) elseif r.shaft == "up" then shaft_close(UP) end
        r.shaft = nil
        if r.lives <= 0 then
            leave(p, "Game over. Drop into the hole again to start over.")
            return
        end
        r.power, r.scaling = 0, {}
        set_scale(p, r, SMALL)
        local x, y, z = flag_at(r.level, r.flag)
        core.player_place(p, x, y, z, -1, 0, 0)
        r.safe = clock + 1.5
        r.y, r.vy = y, 0
        count(p, r)
    end

    -- Hit: big or fire goes back to small, small loses a life.
    local function hurt(p: number, r: Run)
        if r.power == 0 then
            lose(p, r)
            return
        end
        r.power = 0
        resize(r, SMALL)
        r.safe = clock + 2.0
        local px, py, pz = core.player_pos(p)
        if px then core.play_sound("platformer:shrink", px, py + 0.5, pz) end
        tell(p, "safe 2")
        count(p, r)
    end

    -- A creature done for: gone after a puff, seen by everybody in the level.
    local function squash(id: number, level_of: number, x: number, y: number)
        core.ecs.set(id, "dead", clock + 0.1)
        core.play_sound("platformer:stomp", x, y, PLANE)
        for q, other in runs do
            if other.level == level_of then tell(q, string.format("puff %.1f %.1f", x, y)) end
        end
    end

    local function throw(p: number, r: Run)
        if r.power ~= 2 or clock < r.gun or r.shaft then return end
        local n = 0
        for _, who in fires do
            if who == p then n += 1 end
        end
        if n >= 2 then return end
        local x, y = core.player_pos(p)
        local dx = core.player_look(p)
        if x == nil or dx == nil then return end
        local dir = if dx < 0 then -1 else 1
        local id = core.ecs.spawn(FIRE, x + dir * 0.5, y + 0.9 * r.scale, PLANE)
        if id == nil then return end
        core.ecs.set(id, "vx", dir * 11)
        core.ecs.set(id, "vy", -3)
        core.ecs.set(id, "t", 0)
        core.ecs.set(id, "owner", p)
        core.ecs.visible(id, true)
        core.ecs.model(id, model("fireball"))
        fires[id] = p
        r.gun = clock + 0.25
        core.play_sound("platformer:fire", x, y + 1, PLANE)
    end

    asked = function(p: number, text: string)
        local r = runs[p]
        if text == "quit" and r ~= nil then
            leave(p, "")
        elseif text == "fire" and r ~= nil then
            throw(p, r)
        end
    end
    core.event.on("core:net_message", function(ev) asked(ev.player, ev.text) end)

    -- The creatures on their beats: a sprout hops along, a spikeshell
    -- crawls, a buzzer flies up and down as it goes.
    local function step_foes(dt: number)
        for id in foes do
            local x = core.ecs.get(id, "x")
            if x == nil then
                foes[id] = nil
                continue
            end
            local dead = core.ecs.get(id, "dead")
            if dead > 0 then
                if clock >= dead then
                    core.ecs.kill(id)
                    foes[id] = nil
                end
                continue
            end
            local kind = core.ecs.get(id, "kind")
            local dir, t = core.ecs.get(id, "dir"), core.ecs.get(id, "t") + dt
            x += dir * KINDS[kind].pace * dt
            if x <= core.ecs.get(id, "a") then dir = 1 elseif x >= core.ecs.get(id, "b") then dir = -1 end
            local base = core.ecs.get(id, "base")
            core.ecs.set(id, "x", x)
            core.ecs.set(id, "y", if kind == SPROUT then base + math.abs(math.sin(t * 7)) * 0.18
                                  elseif kind == BUZZER then base + math.sin(t * 2.2) * 1.1 else base)
            core.ecs.set(id, "dir", dir)
            core.ecs.set(id, "t", t)
            -- The creatures' models face +z at yaw 0, against a body's -z:
            -- turned half round more than a body walking that way would be.
            core.ecs.set(id, "yaw", -dir * math.pi / 2)
        end
    end

    -- A power-up rises out of its block, then a mushroom slides along the
    -- way on, falling off edges and turning at walls; a flower stays.
    local function step_items(dt: number)
        for id, of in items do
            local x, y = core.ecs.get(id, "x"), core.ecs.get(id, "y")
            if x == nil or y == nil then
                items[id] = nil
                continue
            end
            local rise = core.ecs.get(id, "rise")
            if rise > 0 then
                rise = math.max(0, rise - dt)
                core.ecs.set(id, "rise", rise)
                core.ecs.set(id, "y", core.ecs.get(id, "top") - rise / 0.6)
                continue
            end
            if core.ecs.get(id, "kind") ~= MUSHROOM then continue end
            local vx, vy = core.ecs.get(id, "vx"), core.ecs.get(id, "vy")
            if solid_at(x + (if vx < 0 then -0.35 else 0.35), y + 0.3) then vx = -vx end
            x += vx * dt
            if solid_at(x, y - 0.02) and vy <= 0 then
                vy = 0
                y = math.floor(y - 0.02) + 1
            else
                vy -= 30 * dt
                y += vy * dt
            end
            if y < VOID_Y then
                core.ecs.kill(id)
                items[id] = nil
                continue
            end
            core.ecs.set(id, "x", x)
            core.ecs.set(id, "y", y)
            core.ecs.set(id, "vx", vx)
            core.ecs.set(id, "vy", vy)
            core.ecs.set(id, "yaw", 0)
        end
    end

    -- Fireballs: thrown down and along, bouncing off the ground, gone at a
    -- wall, a creature or after two and a half seconds.
    local function step_fires(dt: number)
        for id, who in fires do
            local x, y = core.ecs.get(id, "x"), core.ecs.get(id, "y")
            local t = core.ecs.get(id, "t")
            if x == nil or y == nil or t == nil then
                fires[id] = nil
                continue
            end
            t += dt
            local vx, vy = core.ecs.get(id, "vx"), core.ecs.get(id, "vy")
            vy -= 40 * dt
            local nx, ny = x + vx * dt, y + vy * dt
            local gone = t > 2.5 or ny < VOID_Y
            if not gone and solid_at(nx, ny) then
                if vy < 0 and not solid_at(nx, y + 0.05) then
                    ny, vy = math.floor(ny) + 1, 8   -- off the ground
                else
                    gone = true                       -- into a wall
                end
            end
            if not gone then
                for fid, of in foes do
                    local fx, fy = core.ecs.get(fid, "x"), core.ecs.get(fid, "y")
                    if fx and core.ecs.get(fid, "dead") == 0 then
                        local k = KINDS[core.ecs.get(fid, "kind")]
                        if math.abs(nx - fx) < 0.6 and ny > fy - 0.3 and ny < fy + k.tall then
                            squash(fid, of, fx, fy + k.tall * 0.5)
                            gone = true
                            break
                        end
                    end
                end
            end
            if gone then
                core.ecs.kill(id)
                fires[id] = nil
                for q, r in runs do
                    if q == who or r.level == (runs[who] and runs[who].level or 0) then
                        tell(q, string.format("spark %.1f %.1f", nx, ny + 0.2))
                    end
                end
            else
                core.ecs.set(id, "x", nx)
                core.ecs.set(id, "y", ny)
                core.ecs.set(id, "vx", vx)
                core.ecs.set(id, "vy", vy)
                core.ecs.set(id, "t", t)
            end
        end
    end

    core.event.on("core:tick", function(ev)
        local dt = ev.dt
        clock += dt
        if not door_said then say_door() end
        if not hole_open then open_hole() end
        if not timed then
            -- Always noon: the levels are lit by the sun, and a clock is their own.
            timed = true
            core.set_time(0.5)
            core.set_day_cycle(false)
        end

        step_foes(dt)
        step_items(dt)
        step_fires(dt)

        local here: { [number]: boolean } = {}
        for _, p in core.players() do
            here[p] = true
            local x, y, z = core.player_pos(p)
            if x == nil then continue end
            local r = runs[p]
            if r == nil then
                -- Out of the hole into the level's plane (the portal has made
                -- the body small already), or into the pipe at the end of the
                -- bridge, for whoever has cleared 1-1.
                if z > -2 and z < 3 and y > VOID_Y then
                    start(p, if START_X + 0.5 - x >= 190 then 2 else 1, nil, false)
                    continue
                end
                local at = 0
                if x > 9.5 and x < 12.5 and z > -22.5 and z < -18.5 then at = 1 end
                if x > PIPE2[1] - 0.2 and x < PIPE2[2] + 1.2 and z > -21.5 and z < -17.5 and y < G + 2 then
                    at = 2
                    if z > -19.7 and core.storage_get(`cleared:{core.player_name(p)}`) == "1" then
                        start(p, 2, nil, true)
                        continue
                    end
                end
                if at ~= shown[p] then
                    if at == 1 then
                        core.notice(p, "Level 1-1: drop into the hole")
                    elseif at == 2 then
                        core.notice(p, if core.storage_get(`cleared:{core.player_name(p)}`) == "1"
                            then "Level 1-2" else "Level 1-2: clear 1-1 first")
                    end
                end
                shown[p] = at
                continue
            end
            local lv = LEVELS[r.level]
            local vy = (y - r.y) / math.max(dt, 0.001)
            local was = r.vy
            r.y, r.vy = y, vy
            while r.scaling[1] and clock >= r.scaling[1][1] do
                set_scale(p, r, r.scaling[1][2])
                table.remove(r.scaling, 1)
            end
            local tall = 1.8 * r.scale   -- the body's height, feet to the top of the head
            if r.won then
                if clock >= r.won then
                    if r.level < #LEVELS then start(p, r.level + 1, r, true) else leave(p, "") end
                end
                continue
            end
            r.t += dt
            local s = START_X + 0.5 - x

            -- Through a pipe: held to its middle and moved along it, the
            -- camera after the body, and the pipe whole again behind it.
            if r.shaft == "down" then
                -- Shut again only once the head is out of the hanging mouth
                -- (its cell is G - 7): shut on a big body's head, the pipe's
                -- blocks held it there.
                if y + tall < G - 7 - 0.1 then
                    r.shaft = nil
                    shaft_close(DOWN)
                elseif y > DOWN_TOP - 1.2 * r.scale then
                    -- Sinking into the mouth, slowly: a launch sets the body's
                    -- speed up to 0 at least (a push never throws it down), so
                    -- one a tick holds it to a sink; below that it falls.
                    core.player_launch(p, (DOWN_X - x) * 6, 0, (PLANE - z) * 6, 0.5)
                end
                continue
            elseif r.shaft == "up" then
                if y > UP_TOP + 0.3 then
                    r.shaft = nil
                    shaft_close(UP)
                    core.play_sound("platformer:pipe", x, y + 1, PLANE)
                else
                    core.player_launch(p, (UP_X - x) * 6, 8, (PLANE - z) * 6, 0.5)
                end
                continue
            elseif r.level == 1 and math.abs(x - DOWN_X) < 0.9 and math.abs(y - DOWN_TOP) < 0.35
                and core.player_crouching(p) then
                r.shaft = "down"
                shaft_open(DOWN)
                core.play_sound("platformer:pipe", x, y + 1, PLANE)
                continue
            elseif r.level == 1 and y < BY + 1.2 and s + 0.3 * r.scale > UP_MOUTH and s < 52 then
                -- Against the lying mouth: the front of the body at it, whatever its size.
                r.shaft = "up"
                shaft_open(UP)
                core.player_place(p, UP_X, BY + 0.05, PLANE, -1, 0, 0)
                core.play_sound("platformer:pipe", x, y + 1, PLANE)
                continue
            end

            -- Coins: the cells the body is in.
            for i, c in COINS do
                -- The coin's cell against the body's box, a little round both:
                -- a small body is shorter than a cell.
                if not taken[i] and c[4] == r.level and math.abs(x - (c[1] + 0.5)) < 0.5 + 0.2 * r.scale
                    and c[2] < y + tall + 0.15 and c[2] + 1 > y - 0.1 then
                    taken[i] = true
                    core.set_block(c[1], c[2], c[3], AIR)
                    core.play_sound("platformer:coin", c[1] + 0.5, c[2] + 0.5, PLANE)
                    r.coins += 1
                    count(p, r)
                end
            end

            -- A prize block: the head come up against its underside -- rising
            -- a tick ago, and the top of the head a hair under the block.
            local head = y + tall
            local by = math.floor(head + 0.25)
            if was > 0.5 and by - head < 0.25 then
                for _, bx in { math.floor(x - 0.29 * r.scale), math.floor(x + 0.29 * r.scale) } do
                    if core.get_block(bx, by, 0) == PRIZE then
                        local key = `{bx},{by}`
                        core.set_block(bx, by, 0, SPENT)
                        bumped[key] = { bx, by, r.level }
                        core.play_sound("platformer:bump", bx + 0.5, by, PLANE)
                        if POWERS[key] then
                            -- A mushroom to a small body, a flower to a big one.
                            local kind = if r.power == 0 then MUSHROOM else FLOWER
                            local id = core.ecs.spawn(ITEM, bx + 0.5, by, PLANE)
                            if id ~= nil then
                                core.ecs.set(id, "kind", kind)
                                core.ecs.set(id, "vx", -2.5)
                                core.ecs.set(id, "vy", 0)
                                core.ecs.set(id, "rise", 0.6)
                                core.ecs.set(id, "top", by + 1)
                                core.ecs.visible(id, true)
                                core.ecs.model(id, ITEM_MODEL[kind])
                                items[id] = r.level
                            end
                            core.play_sound("platformer:power_up", bx + 0.5, by + 1, PLANE)
                        else
                            core.play_sound("platformer:coin", bx + 0.5, by + 1, PLANE)
                            tell(p, `pop {bx} {by + 1}`)
                            r.coins += 1
                            count(p, r)
                        end
                        break
                    end
                end
            end

            -- A power-up touched.
            for id, of in items do
                if of ~= r.level or core.ecs.get(id, "rise") > 0 then continue end
                local ix, iy = core.ecs.get(id, "x"), core.ecs.get(id, "y")
                if ix and math.abs(x - ix) < 0.4 + 0.3 * r.scale and iy > y - 0.8 and iy < y + tall then
                    local kind = core.ecs.get(id, "kind")
                    core.ecs.kill(id)
                    items[id] = nil
                    core.play_sound("platformer:grow", x, y + 1, PLANE)
                    if r.scale < 1 then resize(r, 1) end
                    r.power = math.max(r.power, if kind == FLOWER then 2 else 1)
                    count(p, r)
                end
            end

            if r.flag == 1 and s >= lv.half then
                r.flag = 2
                core.play_sound("platformer:flag", x, y + 1, z)
                core.notice(p, "Half-way flag: you start again from here.")
            end

            if s >= lv.goal - 1.0 then   -- the pole stops a body at 0.8 short of its middle
                r.won = clock + 4
                core.play_sound("platformer:win", x, y + 1, z)
                core.storage_set(`cleared:{core.player_name(p)}`, "1")
                tell(p, `win {math.floor(r.t * 10)} {r.coins} {lv.name}`)
                continue
            end

            -- Fallen out of the level -- the bonus room is lower still, and is not.
            if y < VOID_Y and not (r.level == 1 and s > 33 and s < 53 and y > BY - 2) then
                lose(p, r)
                continue
            end

            -- Creatures: a soft one landed on from above dies and the body
            -- bounces; walked into, or a spikeshell touched at all, a hit.
            if clock >= r.safe then
                for id, of in foes do
                    if of ~= r.level then continue end
                    local fx, fy = core.ecs.get(id, "x"), core.ecs.get(id, "y")
                    if fx == nil or core.ecs.get(id, "dead") > 0 then continue end
                    local k = KINDS[core.ecs.get(id, "kind")]
                    if math.abs(x - fx) < 0.55 + 0.3 * r.scale and y > fy - tall + 0.1 and y < fy + k.tall + 0.1 then
                        if k.soft and vy < -1 and y > fy + 0.35 * k.tall then
                            squash(id, of, fx, fy + k.tall * 0.5)
                            core.player_launch(p, 0, 9, 0, 0.6)
                            r.safe = clock + 0.3
                        else
                            hurt(p, r)
                        end
                        break
                    end
                end
            end
        end
        for p in runs do
            if not here[p] then runs[p] = nil end
        end
        for p in shown do
            if not here[p] then shown[p] = nil end
        end
    end)
end

-- ================================================================ the player

if drawing then
    local inside = false
    local world = "1-1"
    local swing: number? = nil    -- seconds since told "in", until the camera is asked for
    local lives, coins, power = 3, 0, 0
    local safe_until = 0.0
    local now = 0.0
    local sparks: { { number } } = {}    -- a fireball gone: { x, y, age }
    local time = 0.0
    local banner, banner_left = "", 0.0
    local pops: { { number } } = {}      -- a coin out of a prize block: { x, y, age }
    local puffs: { { number } } = {}     -- a creature landed on: { x, y, age }
    local last_y, last_vy = 0.0, 0.0

    local function camera()
        -- 16 off and narrow, so the picture is flat; looking +z, which puts
        -- -x, the way on, on the right. The plane is where the body stands.
        -- Swung out over most of the fall from the sky into 1-1.
        core.side_view(16, 180, 50, 1.4)
        core.engine_hud(false)
        swing = nil
    end

    heard = function(text: string)
        local w = string.split(text, " ")
        if w[1] == "in" then
            -- From the room the camera swings out; from the level before it is there.
            if not inside then swing = 0 end
            inside, world, time, banner_left = true, w[2] or world, 0, 0
        elseif w[1] == "out" then
            inside, swing = false, nil
            core.side_view(0, 180, 0, 0.8)
            core.engine_hud(true)
        elseif w[1] == "hud" then
            lives, coins, power = tonumber(w[2]) or lives, tonumber(w[3]) or coins, tonumber(w[4]) or power
        elseif w[1] == "safe" then
            safe_until = now + (tonumber(w[2]) or 0)
        elseif w[1] == "spark" then
            table.insert(sparks, { tonumber(w[2]) or 0, tonumber(w[3]) or 0, 0 })
        elseif w[1] == "pop" then
            table.insert(pops, { (tonumber(w[2]) or 0) + 0.5, tonumber(w[3]) or 0, 0 })
        elseif w[1] == "puff" then
            table.insert(puffs, { tonumber(w[2]) or 0, tonumber(w[3]) or 0, 0 })
        elseif w[1] == "win" then
            local t = (tonumber(w[2]) or 0) / 10
            -- lang: "{} CLEARED   {} s   {} COINS"
            banner = string.format("%s CLEARED   %.1f s   %d COINS", w[4] or world, t, tonumber(w[3]) or 0)
            banner_left = 4
        end
    end
    if not serving then
        core.event.on("core:net_message", function(ev) heard(ev.text) end)
    end

    core.event.on("core:tick", function(ev)
        now += ev.dt
        if banner_left > 0 then banner_left -= ev.dt elseif inside then time += ev.dt end
        for i = #pops, 1, -1 do
            pops[i][3] += ev.dt
            if pops[i][3] > 0.45 then table.remove(pops, i) end
        end
        for i = #sparks, 1, -1 do
            sparks[i][3] += ev.dt
            if sparks[i][3] > 0.3 then table.remove(sparks, i) end
        end
        for i = #puffs, 1, -1 do
            puffs[i][3] += ev.dt
            if puffs[i][3] > 0.4 then table.remove(puffs, i) end
        end
        -- A jump is heard on this machine only: it is the one that knows when
        -- the feet left the ground -- going up fast now, and not a moment ago.
        local px, py, pz = core.player_pos(1)
        if px ~= nil and ev.dt > 0 then
            local vy = (py - last_y) / ev.dt
            if inside and vy > 6 and vy < 20 and last_vy < 2 then core.play_sound("platformer:jump", px, py + 1, pz) end
            last_y, last_vy = py, vy
        end
        if swing ~= nil then
            -- The camera's plane is where the body stands when it is asked
            -- for: wait until the server's place at the start has arrived
            -- (a second at most).
            swing += ev.dt
            if (pz ~= nil and math.abs(pz - PLANE) < 0.2) or swing > 1.0 then camera() end
        end
    end)

    -- The fireball's own key, F to start with, movable in Options > Controls
    -- (core.register_key); the left button throws too.
    core.register_key("platformer:fire", "F", "Throw a fireball")

    core.event.on("core:input", function(ev)
        if not inside then return end
        local function send(what: string)
            if serving then asked(ev.player, what) else core.net.send(0, what) end
        end
        if ev.action == "key_e" then
            send("quit")
        elseif ev.action == "key_platformer:fire" or ev.action == "mouse_left" then
            if ev.action == "mouse_left" then ev.cancel = true end   -- nothing is dug or hit in a level
            if power == 2 then send("fire") end
        elseif ev.action == "mouse_right" then
            ev.cancel = true
        end
    end)

    core.render.pass("platformer:pops", 100, function()
        for _, c in pops do
            local t = c[3] / 0.45
            core.render.box(c[1], c[2] + 0.5 + 1.6 * t * (2 - t), PLANE, 0.5, 0.6, 0.12, 255, 214, 40, 255)
        end
        for _, c in sparks do
            core.render.fx("sparks", c[1], c[2], PLANE - 0.6, 0.5, 255, 150, 40, 255, c[3] / 0.3)
        end
        for _, c in puffs do
            core.render.fx("smoke", c[1], c[2], PLANE - 0.6, 0.9, 235, 235, 235, 220, c[3] / 0.4)
        end
    end)

    core.hud.pass("platformer:hud", 100, function()
        if not inside then return end
        core.hud.text(0.04, 0.05, `WORLD {world}`, 0.04, 0, 255, 255, 255, 255)
        core.hud.text(0.35, 0.05, `COINS {coins}`, 0.04, 0, 255, 220, 60, 255)
        core.hud.text(0.65, 0.05, `LIVES {lives}`, 0.04, 0, 255, 120, 120, 255)
        core.hud.text(0.96, 0.05, string.format("TIME %.0f", time), 0.04, 2, 255, 255, 255, 255)
        if power == 2 then
            core.hud.text(0.5, 0.11, "FIRE", 0.035, 1, 255, 140, 40, 255)
        elseif power == 1 then
            core.hud.text(0.5, 0.11, "BIG", 0.035, 1, 255, 90, 90, 255)
        end
        if now < safe_until and now % 0.2 < 0.1 then
            core.hud.rect(0.5, 0.5, 4, 1, 255, 255, 255, 40)
        end
        core.hud.text(0.5, 0.95, if power == 2 then "A D walk   W jump   S down   Shift run   F fire   E leave"
                                 else "A D walk   W jump   S down   Shift run   E leave", 0.024, 1, 255, 255, 255, 200)
        if banner_left > 0 then
            core.hud.text(0.5, 0.35, banner, 0.06, 1, 255, 240, 120, 255)
        end
    end)
end
