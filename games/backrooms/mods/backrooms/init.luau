--!strict
-- backrooms: an endless maze of rooms nobody built, three levels of it stacked
-- in the world's 64 cells of height:
--
--   Level 0      (floor 20)  yellow wallpaper, damp carpet, a hum of tube
--                            lights, some of them dead, some flickering
--   The poolrooms (floor 36) white tiles, pools of every size, lit from
--                            under the water, slides down into them
--   Level 1      (floor 4)   a concrete warehouse, puddles, few lamps
--
-- Every level is a grid of rooms eight cells across; a wall between two rooms
-- is there or not by a hash of where it is, and a wall that is there always
-- has a doorway in it, so every room can be reached from every other. Runs of
-- four rooms in a row become a corridor three wide, walled along its sides
-- with a door where the grid's walls have theirs (in the poolrooms a flooded
-- channel). Now and then a block of two rooms by two is one tall hall up into
-- the gap over its level: galleries round its walls, one floor over another,
-- parapets along their edges and a flight of stairs up to each. Some rooms go
-- up or down: a stairwell of two flights a turn climbs the sixteen cells to
-- the level above and comes out through its floor; a hole in a floor drops to
-- the level below, into a pool (water ends a fall).
--
-- Rarely -- one chunk column in three, on two of the three levels -- a room
-- holds a lit doorway with a portal in it, and the same room on another level
-- holds the other end: the two share x and z, so where it comes out is where
-- it went in, one level up or down. Its frame is dressed as the level it leads
-- to. Only the pairs near somebody are open (core.portal_open has 16
-- openings, two of them the world's own way back to the sandbox).
--
-- The layout is a function of x and z alone, not of the seed: the backrooms
-- are the same for everybody, and the tick can find a portal without having
-- seen a mapgen stage run (a world loaded from its file runs none).

local AIR = 0
local C = 8          -- a room's side, wall to wall
local REGION = 32    -- one chunk column; at most one portal pair in each
local PORTAL_CHANCE = 330   -- of 1000 regions

local function node(name: string, rgb: { number }, light: number?, solid: boolean?): number
    local s = if solid == nil then true else solid
    return core.register_node(`backrooms:{name}`, rgb[1], rgb[2], rgb[3], s, light or 0, "", 0, 0, "stone")
end
local function model(name: string): number
    return core.register_model(`backrooms:{name}`, `{name}.gltf`)
end

-- Level 0.
local WALLPAPER = node("wallpaper", { 196, 178, 96 })
local CARPET    = node("carpet", { 150, 128, 70 })
local CEILING   = node("ceiling", { 206, 198, 160 })
local TUBE      = node("tube", { 250, 248, 220 }, 14)
local TUBE_OFF  = node("tube_off", { 150, 150, 140 })   -- a dead one, and a flicker's dark half
-- The poolrooms.
local TILE      = node("tile", { 232, 236, 238 })
local POOL_TILE = node("pool_tile", { 150, 206, 222 })
local POOL_EDGE = node("pool_edge", { 210, 226, 232 })  -- the coping round a pool
local POOL_LAMP = node("pool_lamp", { 236, 246, 255 }, 15)
local WATER     = node("water", { 90, 170, 210 }, 0, false)
core.node_liquid(WATER, 90)
-- A slide: a chute down a cell, in high at +z and out low at -z (facing 0
-- heads -z), its bed a quarter over the cell where it comes in and where the
-- next one meets it. A fast belt running down (core.node_conveyor 5, 1.25, -1)
-- is a chute the engine rides a body down, sat inside it on the bed's middle
-- line (chute_at, src/core/physics): not solid, or its stepped boxes would
-- have the body walk down stairs over it. Drawn as a chute (slide.gltf,
-- tools/make_backrooms_props.py).
local SLIDE = core.register_node("backrooms:slide", 70, 160, 220, false, 0, "upright", 0, 0, "stone")
core.node_conveyor(SLIDE, 5, 1.25, -1)
core.node_model(SLIDE, model("slide"))
-- A quarter turn of a slide, down a cell like the straight: in through the
-- cell's +x side heading -x (a) or its -x side heading +x (b), out through its
-- front heading -z, at facing 0 (turned like the straight). The engine tells
-- a turn by the slide a cell up that runs into its side, and rides it round a
-- quarter circle about the corner the two sides share.
local function slide_turn(name: string): number
    local id = core.register_node(`backrooms:{name}`, 70, 160, 220, false, 0, "upright", 0, 0, "stone")
    core.node_conveyor(id, 5, 1.25, -1)
    core.node_model(id, model(name))
    return id
end
local TURN_A = slide_turn("slide_turn_a")
local TURN_B = slide_turn("slide_turn_b")
-- A ladder of steel rails, against the +z side of its cell before it is
-- turned (facing 1 puts it against +x, 3 against -x): up a slide's tower,
-- and out of every pool. Drawn as ladder.gltf.
local LADDER = core.register_node("backrooms:ladder", 190, 196, 200, true, 0, "upright", 0, 0, "metal")
core.node_box(LADDER, 0.1, 0, 0.8, 0.9, 1, 1)
core.node_climbable(LADDER)
core.node_model(LADDER, model("ladder"))
-- A chest, rare, in a room with nothing else in it: mods/chest's model, and
-- what is in it decided the first time somebody opens it (the compasses').
local CHEST = core.register_node("backrooms:chest", 150, 110, 60, true, 0, "upright")
core.node_model(CHEST, model("chest"))
core.node_box(CHEST, 0.0625, 0.000, 0.0625, 0.9375, 0.625, 0.9375)
core.node_box(CHEST, 0.0625, 0.625, 0.0625, 0.9375, 0.875, 0.9375, "lid")
local CHEST_CHANCE = 25   -- of 1000 rooms with nothing in them
-- Level 1.
local CONCRETE  = node("concrete", { 128, 128, 124 })
local SLAB      = node("slab", { 96, 96, 94 })
local LAMP      = node("lamp", { 255, 214, 150 }, 14)
local LAMP_OFF  = node("lamp_off", { 110, 100, 86 })
local PUDDLE    = node("puddle", { 70, 80, 84 }, 0, false)
core.node_liquid(PUDDLE, 120, 1)
-- A portal's doorway: dark, its lintel lit.
local FRAME     = node("frame", { 44, 40, 36 })
local GLOW      = node("glow", { 255, 250, 230 }, 9)

-- A stair: half a cell, and the back half of the cell over it; it climbs
-- towards +z (facing 0), +x (1), -z (2) or -x (3). A body walks up it half a
-- cell at a time. One for each level, in its floor: carpeted, tiled, bare.
local function stair(name: string, rgb: { number }): number
    local id = core.register_node(`backrooms:{name}`, rgb[1], rgb[2], rgb[3], true, 0, "upright", 0, 0, "stone")
    core.node_box(id, 0, 0, 0, 1, 0.5, 1)
    core.node_box(id, 0, 0.5, 0.5, 1, 1, 1)
    return id
end
local STAIR_CARPET   = stair("stair_carpet", { 150, 128, 70 })
local STAIR_TILE     = stair("stair_tile", { 232, 236, 238 })
local STAIR_CONCRETE = stair("stair_concrete", { 120, 120, 116 })

-- What each level is made of. F is the first cell of air over its floor, h how
-- many cells of air up to its ceiling, `tall` the ceiling of its halls (up
-- into the gap over it); `wall`, `corr` and `hall` are how many in a thousand
-- walls are there, rows are corridors and blocks are halls.
type Level = {
    name: string, F: number, h: number, tall: number, floor: number, wall_block: number, ceil: number,
    wall: number, corr: number, hall: number, lamp: number, lamp_off: number?, lamps: { { number } },
    dark: number, stair: number,
}
local LEVELS: { Level } = {
    { name = "Level 0", F = 21, h = 5, tall = 33, floor = CARPET, wall_block = WALLPAPER, ceil = CEILING,
      wall = 760, corr = 220, hall = 90, lamp = TUBE, lamp_off = TUBE_OFF,
      lamps = { { 2, 3 }, { 2, 4 }, { 6, 3 }, { 6, 4 } }, dark = 160, stair = STAIR_CARPET },
    { name = "The poolrooms", F = 37, h = 9, tall = 62, floor = TILE, wall_block = TILE, ceil = TILE,
      wall = 360, corr = 110, hall = 120, lamp = POOL_LAMP,
      lamps = { { 2, 2 }, { 6, 2 }, { 2, 6 }, { 6, 6 }, { 4, 4 } }, dark = 0, stair = STAIR_TILE },
    { name = "Level 1", F = 5, h = 6, tall = 19, floor = SLAB, wall_block = CONCRETE, ceil = CONCRETE,
      wall = 420, corr = 300, hall = 90, lamp = LAMP, lamp_off = LAMP_OFF,
      lamps = { { 4, 4 }, { 4, 5 } }, dark = 250, stair = STAIR_CONCRETE },
}
local PAIRS = { { 1, 2 }, { 1, 3 }, { 2, 3 } }

-- The level a body's feet are in, by height.
local function level_at(y: number): number
    return if y >= 33 then 2 elseif y >= 17 then 1 else 3
end

-- mods/ore's hash: exact in a double, a function of its arguments alone.
local function mul32(a: number, b: number): number
    local hi = math.floor(a / 65536)
    local lo = a % 65536
    return ((hi * b) % 65536 * 65536 + lo * b) % 4294967296
end
local function hash(x: number, z: number, salt: number): number
    local h = mul32(x + 0x9E3779B9, 374761393)
    h = bit32.bxor(h, mul32(z + 0x85EBCA6B, 668265263))
    h = bit32.bxor(h, mul32(salt % 4294967296, 2246822519))
    h = bit32.bxor(h, bit32.rshift(h, 13))
    return mul32(h, 1274126177)
end

-- The portal pair of a region, or nil: the room it is in and its two levels.
-- Never in the room at the origin, where the way back to the sandbox is.
type Site = { cx: number, cz: number, a: number, b: number, x: number, z: number }
local function site(rx: number, rz: number): Site?
    local h = hash(rx, rz, 4099)
    if h % 1000 >= PORTAL_CHANCE then
        return nil
    end
    local per = REGION // C
    local cx = rx * per + bit32.rshift(h, 10) % per
    local cz = rz * per + bit32.rshift(h, 14) % per
    if cx == 0 and cz == 0 then
        return nil
    end
    local pair = PAIRS[bit32.rshift(h, 18) % 3 + 1]
    -- The hole is the room's cell (4, 3), facing +z; the portal's plane four
    -- tenths into it from its front.
    return { cx = cx, cz = cz, a = pair[1], b = pair[2], x = cx * C + 4.5, z = cz * C + 3.6 }
end

-- A room of the portal pair's, on every level.
local function site_room(cx: number, cz: number): boolean
    local s = site((cx * C) // REGION, (cz * C) // REGION)
    return s ~= nil and s.cx == cx and s.cz == cz
end

-- What joins a room to the level above or below it, the same room on every
-- level: "" for nothing, or one of VERTICAL's.
type Vertical = { stairs: boolean, lo: number, hi: number }
local VERTICAL: { [string]: Vertical } = {
    stairs_1 = { stairs = true, lo = 3, hi = 1 },    -- Level 1 up to Level 0
    stairs_0 = { stairs = true, lo = 1, hi = 2 },    -- Level 0 up to the poolrooms
    hole_0   = { stairs = false, lo = 3, hi = 1 },   -- Level 0 down to Level 1
    hole_p   = { stairs = false, lo = 1, hi = 2 },   -- the poolrooms down to Level 0
}
local function vertical(cx: number, cz: number): string
    if (cx == 0 and cz == 0) or site_room(cx, cz) then
        return ""
    end
    local r = hash(cx, cz, 7717) % 1000
    return if r < 22 then "stairs_1" elseif r < 44 then "stairs_0" elseif r < 64 then "hole_0"
        elseif r < 84 then "hole_p" else ""
end

-- Whether a room is one of a hall's four: blocks of two by two from an even
-- room, none of them the origin's, a portal's or one going up or down (their
-- shafts are in the gap the hall takes).
local function hall(L: number, cx: number, cz: number): boolean
    local bx, bz = cx - cx % 2, cz - cz % 2
    if hash(bx, bz, L * 211 + 9) % 1000 >= LEVELS[L].hall then
        return false
    end
    for dz = 0, 1 do
        for dx = 0, 1 do
            local x, z = bx + dx, bz + dz
            if (x == 0 and z == 0) or site_room(x, z) or vertical(x, z) ~= "" then
                return false
            end
        end
    end
    return true
end

-- A room in a corridor: "x" runs along x, in runs of four rooms; "z" along z;
-- "" a room. Never a room that goes up or down, a hall's, a portal's or the
-- origin's.
local function kind(L: number, cx: number, cz: number): string
    if (cx == 0 and cz == 0) or vertical(cx, cz) ~= "" or site_room(cx, cz) or hall(L, cx, cz) then
        return ""
    end
    local corr = LEVELS[L].corr
    if hash(cx // 4, cz, L * 13 + 1) % 1000 < corr then
        return "x"
    elseif hash(cx, cz // 4, L * 17 + 3) % 1000 < corr then
        return "z"
    end
    return ""
end

-- A wall between two rooms: there or not, and its doorway, cells g0..g1 along
-- it. `across` 0 is a wall along z at x = cx * C, between rooms cx - 1 and
-- cx; 1 one along x at z = cz * C, between cz - 1 and cz. Where a corridor
-- runs through it, it is there, and the corridor is its doorway; inside a
-- hall it is not there.
local function wall(L: number, cx: number, cz: number, across: number): (boolean, number, number)
    if (if across == 0 then cx else cz) % 2 == 1 and hall(L, cx, cz) then
        return false, 0, 0
    end
    local along = if across == 0 then "x" else "z"
    local before = if across == 0 then kind(L, cx - 1, cz) else kind(L, cx, cz - 1)
    if before == along or kind(L, cx, cz) == along then
        return true, 3, 5
    end
    local h = hash(cx * 2 + across, cz, L * 7919)
    local d = 1 + bit32.rshift(h, 10) % 5
    return h % 1000 < LEVELS[L].wall, d, d + 1
end

-- What stands in a room, by a hash of it: "pool", "lap", "big", "slide",
-- "pillar", "puddle", or "".
local function feature(L: number, cx: number, cz: number): string
    local r = hash(cx, cz, L * 104729 + 17) % 1000
    if L == 2 then
        return if r < 120 then "slide" elseif r < 300 then "pool" elseif r < 380 then "lap"
            elseif r < 440 then "big" else ""
    elseif L == 1 then
        return if r < 90 then "pillar" else ""
    end
    return if r < 260 then "puddle" elseif r < 360 then "pillar" else ""
end

-- A doorway with a portal in it, standing in a room: the hole one cell wide
-- and two tall at (hx, F, hz), a back behind it and a frame round it, dressed
-- as level `to`. along_x: the opening runs along x and faces +z; otherwise it
-- runs along z and faces +x.
local function doorway(F: number, hx: number, hz: number, along_x: boolean, to: number)
    local dress = LEVELS[to].wall_block
    local function put(u: number, v: number, w: number, b: number)
        if along_x then
            core.set_block(hx + u, F + v, hz + w, b)
        else
            core.set_block(hx + w, F + v, hz + u, b)
        end
    end
    for v = 0, 3 do
        for u = -1, 1 do
            put(u, v, -1, FRAME)
            put(u, v, 0, if v == 3 then GLOW elseif u == 0 and v < 2 then AIR else dress)
        end
        put(-2, v, 0, FRAME)
        put(2, v, 0, FRAME)
    end
    for w = 1, 4 do
        for u = -1, 1 do
            for v = 0, 2 do
                put(u, v, w, AIR)
            end
        end
    end
end

local function turned(x: number, y: number, z: number, b: number, facing: number)
    core.set_block(x, y, z, b)
    core.set_facing(x, y, z, facing)
end

-- Which way each facing of an upright block heads (core.node_conveyor's):
-- 0 -z, 1 -x, 2 +z, 3 +x. A turn of k quarters adds k to the facing.
local HEAD: { [number]: { number } } = { [0] = { 0, -1 }, [1] = { -1, 0 }, [2] = { 0, 1 }, [3] = { 1, 0 } }

-- A slide laid from (x, y, z) down one cell for every heading in `heads`: a
-- straight piece where the heading is the one before, a quarter turn where it
-- changes (turn a comes in heading one more than it leaves, b one less).
-- Returns the cell after the last, where a body is thrown: its pool's.
local function lay(x: number, y: number, z: number, heads: { number }): (number, number, number)
    local prev = heads[1]
    for _, f in heads do
        local b = if f == prev then SLIDE elseif prev == (f + 1) % 4 then TURN_A else TURN_B
        turned(x, y, z, b, f)
        x += HEAD[f][1]
        z += HEAD[f][2]
        y -= 1
        prev = f
    end
    return x, y, z
end

-- A pool `depth` deep in level L's floor, x1..x2 by z1..z2: shelled round and
-- under, a ladder out of its -x end. In the poolrooms its shell is blue tile
-- with lights in it under the water, and coping round its edge.
local function pool(L: number, x1: number, z1: number, x2: number, z2: number, depth: number)
    local lv = LEVELS[L]
    local F = lv.F
    local shell = if L == 2 then POOL_TILE else lv.wall_block
    core.replace_nodes(x1 - 1, F - 1 - depth, z1 - 1, x2 + 1, F - 2, z2 + 1, AIR, shell)
    core.replace_nodes(x1, F - depth, z1, x2, F - 2, z2, shell, WATER)
    core.replace_nodes(x1, F - 1, z1, x2, F - 1, z2, lv.floor, WATER)
    if L == 2 then
        for _, s in { { x1 - 1, z1 - 1, x2 + 1, z1 - 1 }, { x1 - 1, z2 + 1, x2 + 1, z2 + 1 },
                      { x1 - 1, z1, x1 - 1, z2 }, { x2 + 1, z1, x2 + 1, z2 } } do
            core.replace_nodes(s[1], F - 1, s[2], s[3], F - 1, s[4], lv.floor, POOL_EDGE)
        end
        for x = x1 + 1, x2 - 1, 3 do
            for _, z in { z1 - 1, z2 + 1 } do
                if core.get_block(x, F - 2, z) == POOL_TILE then
                    core.set_block(x, F - 2, z, POOL_LAMP)
                end
            end
        end
    end
    local zl = (z1 + z2) // 2
    for y = F - depth, F do
        turned(x1, y, zl, LADDER, 3)
    end
end

-- A flight of the level's stairs along +x, from the floor a body stands on at
-- y over (x, z) up `rise` cells: the first step at x + 1.
local function flight(L: number, x: number, y: number, z: number, rise: number)
    for k = 1, rise do
        turned(x + k, y + k - 1, z, LEVELS[L].stair, 1)
    end
end

-- One level over one column.
local function make(L: number, ctx: any, here: Site?)
    local lv = LEVELS[L]
    local F, h = lv.F, lv.h
    core.replace_nodes(ctx.min_x, F - 1, ctx.min_z, ctx.max_x, F - 1, ctx.max_z, AIR, lv.floor)
    core.replace_nodes(ctx.min_x, F + h, ctx.min_z, ctx.max_x, F + h, ctx.max_z, AIR, lv.ceil)
    local top = F + h - 1
    for cz = ctx.min_z // C, ctx.max_z // C do
        for cx = ctx.min_x // C, ctx.max_x // C do
            local x0, z0 = cx * C, cz * C
            local in_hall = hall(L, cx, cz)
            -- The corner post and the two walls on this room's low sides.
            if not (in_hall and cx % 2 == 1 and cz % 2 == 1) then
                core.replace_nodes(x0, F, z0, x0, top, z0, AIR, lv.wall_block)
            end
            local on, g0, g1 = wall(L, cx, cz, 0)
            if on then
                for k = 1, C - 1 do
                    if k < g0 or k > g1 then
                        core.replace_nodes(x0, F, z0 + k, x0, top, z0 + k, AIR, lv.wall_block)
                    end
                end
            end
            on, g0, g1 = wall(L, cx, cz, 1)
            if on then
                for k = 1, C - 1 do
                    if k < g0 or k > g1 then
                        core.replace_nodes(x0 + k, F, z0, x0 + k, top, z0, AIR, lv.wall_block)
                    end
                end
            end
            -- A hall's walls on its high sides are on this room's low lines,
            -- and may be in this column and not in the hall's: up to its
            -- ceiling from here (a stage writes only its own column).
            if not in_hall then
                if hall(L, cx - 1, cz) then
                    core.replace_nodes(x0, F + h, z0, x0, lv.tall, z0 + C, AIR, lv.wall_block)
                    core.replace_nodes(x0, F + h, z0, x0, F + h, z0 + C, lv.ceil, lv.wall_block)
                end
                if hall(L, cx, cz - 1) then
                    core.replace_nodes(x0, F + h, z0, x0 + C, lv.tall, z0, AIR, lv.wall_block)
                    core.replace_nodes(x0, F + h, z0, x0 + C, F + h, z0, lv.ceil, lv.wall_block)
                end
            end
            -- A corridor: its two sides filled, a door through each where
            -- the grid's wall on that side has its doorway, a light in the
            -- middle; in the poolrooms the floor between is water.
            local k = kind(L, cx, cz)
            if k ~= "" then
                local function box(a0: number, a1: number, b0: number, b1: number, from: number, to: number)
                    if k == "x" then
                        core.replace_nodes(x0 + a0, F, z0 + b0, x0 + a1, top, z0 + b1, from, to)
                    else
                        core.replace_nodes(x0 + b0, F, z0 + a0, x0 + b1, top, z0 + a1, from, to)
                    end
                end
                box(1, 7, 1, 2, AIR, lv.wall_block)
                box(1, 7, 6, 7, AIR, lv.wall_block)
                local across = if k == "x" then 1 else 0
                local _, a0, a1 = wall(L, cx, cz, across)
                box(a0, a1, 1, 2, lv.wall_block, AIR)
                _, a0, a1 = wall(L, if k == "x" then cx else cx + 1, if k == "x" then cz + 1 else cz, across)
                box(a0, a1, 6, 7, lv.wall_block, AIR)
                core.set_block(x0 + 4, F + h, z0 + 4, lv.lamp)
                if L == 2 then
                    if k == "x" then
                        pool(L, x0 + 1, z0 + 3, x0 + 7, z0 + 5, 2)
                    else
                        pool(L, x0 + 3, z0 + 1, x0 + 5, z0 + 7, 2)
                    end
                end
            end
            -- The lights; a dead room's tubes are there and dark.
            if not in_hall then
                local lit = hash(cx, cz, L * 31 + 5) % 1000 >= lv.dark
                local b = if lit then lv.lamp else lv.lamp_off
                if b then
                    for _, at in lv.lamps do
                        core.set_block(x0 + at[1], F + h, z0 + at[2], b)
                    end
                end
            end
            local portal = here ~= nil and here.cx == cx and here.cz == cz and (here.a == L or here.b == L)
            local what = if portal or k ~= "" or in_hall or vertical(cx, cz) ~= "" or (cx == 0 and cz == 0)
                then "" else feature(L, cx, cz)
            if what == "pillar" then
                core.replace_nodes(x0 + 3, F, z0 + 3, x0 + 4, top, z0 + 4, AIR, lv.wall_block)
            elseif what == "puddle" then
                local r = hash(cx, cz, 61)
                local px, pz = x0 + 2 + r % 3, z0 + 2 + bit32.rshift(r, 4) % 3
                core.replace_nodes(px, F, pz, px + 2, F, pz + 1, AIR, PUDDLE)
            elseif what == "pool" then
                pool(L, x0 + 2, z0 + 2, x0 + 6, z0 + 6, 2)
            elseif what == "big" then
                pool(L, x0 + 1, z0 + 1, x0 + 7, z0 + 7, 3)
            elseif what == "lap" then
                -- A lane the length of the room, deep at one end.
                if hash(cx, cz, 97) % 2 == 0 then
                    pool(L, x0 + 1, z0 + 3, x0 + 7, z0 + 5, 3)
                else
                    pool(L, x0 + 3, z0 + 1, x0 + 5, z0 + 7, 3)
                end
            elseif what == "slide" then
                -- A tower with a ladder up its side and a slide off it into a
                -- pool: straight down the room to a pool across its low end,
                -- or turning a corner to one along its west side.
                if hash(cx, cz, 389) % 2 == 0 then
                    pool(L, x0 + 1, z0 + 1, x0 + 7, z0 + 2, 3)
                    core.replace_nodes(x0 + 4, F, z0 + 7, x0 + 4, F + 3, z0 + 7, AIR, POOL_TILE)
                    for y = F, F + 4 do
                        turned(x0 + 3, y, z0 + 7, LADDER, 1)
                    end
                    lay(x0 + 4, F + 3, z0 + 6, { 0, 0, 0, 0 })
                else
                    pool(L, x0 + 1, z0 + 2, x0 + 3, z0 + 7, 3)
                    core.replace_nodes(x0 + 6, F, z0 + 7, x0 + 6, F + 3, z0 + 7, AIR, POOL_TILE)
                    for y = F, F + 4 do
                        turned(x0 + 7, y, z0 + 7, LADDER, 3)
                    end
                    lay(x0 + 6, F + 3, z0 + 6, { 0, 1, 1, 1 })
                end
            end
            if what == "" and not portal and k == "" and not in_hall and vertical(cx, cz) == ""
                and not (cx == 0 and cz == 0) and hash(cx, cz, L * 613 + 29) % 1000 < CHEST_CHANCE then
                turned(x0 + 6, F, z0 + 6, CHEST, hash(cx, cz, L * 613 + 31) % 4)
            end
            if portal and here then
                doorway(F, x0 + 4, z0 + 3, true, if here.a == L then here.b else here.a)
            end
        end
    end
end

-- A hall: the block of two rooms by two from (bx, bz), up to the level's
-- `tall` ceiling. Galleries three wide round its walls, one floor over
-- another, a parapet along their inner edge; a flight of stairs up to each
-- from the one below (along +x, by the south gallery for the first, by the
-- north for the next); lamps in the ceiling and in the galleries' floors,
-- which light both ways. In the poolrooms, a pool in the middle.
local function build_hall(L: number, bx: number, bz: number)
    local lv = LEVELS[L]
    local F, h, top = lv.F, lv.h, lv.tall
    local x0, z0 = bx * C, bz * C
    local X1, Z1 = x0 + 2 * C, z0 + 2 * C
    -- The rooms' ceiling and lamps out; the outer walls up to the new one.
    for x = x0 + 1, X1 - 1 do
        for z = z0 + 1, Z1 - 1 do
            core.set_block(x, F + h, z, AIR)
        end
    end
    for _, ln in { { x0, z0, X1, z0 }, { x0, Z1, X1, Z1 }, { x0, z0, x0, Z1 }, { X1, z0, X1, Z1 } } do
        core.replace_nodes(ln[1], F + h, ln[2], ln[3], F + h, ln[4], lv.ceil, lv.wall_block)
        core.replace_nodes(ln[1], F + h, ln[2], ln[3], top - 1, ln[4], AIR, lv.wall_block)
    end
    core.replace_nodes(x0, top, z0, X1, top, Z1, AIR, lv.ceil)
    for _, a in { 4, 8, 12 } do
        for _, b in { 4, 8, 12 } do
            core.set_block(x0 + a, top, z0 + b, lv.lamp)
        end
    end
    -- The galleries, and what leaves a gap in their parapets.
    local floors: { number } = {}
    local y = F + h
    while y + 4 <= top do
        table.insert(floors, y)
        y += h + 1
    end
    local function gallery(u: number, v: number): boolean
        return u <= 3 or u >= 13 or v <= 3 or v >= 13
    end
    for j, fy in floors do
        local gap: { [number]: boolean } = {}
        local function open(u: number, v: number)
            gap[u * 32 + v] = true
        end
        -- Where this floor's flight comes up, and where the next one leaves.
        local last = 3 + h + 1
        local lane = if j % 2 == 1 then 4 else 12
        for u = last - 1, math.min(last, 13) do
            open(u, if lane == 4 then 3 else 13)
            open(u, lane)
        end
        open(3, if lane == 4 then 12 else 4)
        if L == 2 and j == 1 then
            open(13, 11)   -- onto the slide's platform
        end
        for u = 1, 15 do
            for v = 1, 15 do
                if gallery(u, v) then
                    core.set_block(x0 + u, fy, z0 + v, lv.floor)
                    local edge = (u == 3 or u == 13) and v >= 3 and v <= 13
                        or (v == 3 or v == 13) and u >= 3 and u <= 13
                    if edge and not gap[u * 32 + v] then
                        core.set_block(x0 + u, fy + 1, z0 + v, lv.wall_block)
                    end
                end
            end
        end
        for _, at in { { 2, 2 }, { 2, 8 }, { 2, 14 }, { 14, 2 }, { 14, 8 }, { 14, 14 }, { 8, 2 }, { 8, 14 } } do
            core.set_block(x0 + at[1], fy, z0 + at[2], lv.lamp)
        end
        -- The flight up to it: from the ground for the first, from the
        -- gallery under it for the rest, starting at the west one.
        local from = if j == 1 then F else floors[j - 1] + 1
        flight(L, x0 + 3, from, z0 + lane, fy + 1 - from)
    end
    if L == 2 then
        -- A pool in the middle, and a slide into it off the first gallery:
        -- from a platform by the east one, down the east side, round the
        -- south-east corner, along the south side and round into the pool,
        -- ten cells for the gallery's ten.
        pool(L, x0 + 6, z0 + 6, x0 + 10, z0 + 10, 2)
        local fy = floors[1]
        core.set_block(x0 + 12, fy, z0 + 11, lv.floor)
        lay(x0 + 12, fy, z0 + 10, { 0, 0, 0, 0, 0, 1, 1, 1, 1, 2 })
    end
end

-- A stairwell from a room of level `lo` up to the same room of `hi`, sixteen
-- cells over it: two flights a turn along z, side by side (x 1..2 climbing
-- +z, x 5..6 climbing -z), a landing across each end, the lower level's
-- ceiling open over the room and the shaft walled and lit up to the floor
-- above; the last flight comes up through a hole in that floor. Under the
-- flights the room is still a room: a body walks under them. Its stairs and
-- landings are the lower level's.
local function stairwell(x0: number, z0: number, lo: number, hi: number)
    local L, U = LEVELS[lo], LEVELS[hi]
    local F, rise = L.F, U.F - L.F
    local roof = F + L.h
    for x = x0 + 1, x0 + 7 do
        for z = z0 + 1, z0 + 7 do
            core.set_block(x, roof, z, AIR)
        end
    end
    for _, ln in { { 0, 0, 8, 0 }, { 0, 8, 8, 8 }, { 0, 0, 0, 8 }, { 8, 0, 8, 8 } } do
        core.replace_nodes(x0 + ln[1], roof, z0 + ln[2], x0 + ln[3], U.F - 2, z0 + ln[4], AIR, L.wall_block)
    end
    for y = roof + 2, U.F - 2, 4 do
        core.set_block(x0, y, z0 + 4, L.lamp)
        core.set_block(x0 + 8, y, z0 + 4, L.lamp)
    end
    for z = z0 + 2, z0 + 5 do
        for x = x0 + 1, x0 + 2 do
            core.set_block(x, U.F - 1, z, AIR)
        end
    end
    local t = 0
    local function step(lane: number, lz: number, facing: number): boolean
        if t >= rise then
            return false
        end
        t += 1
        for x = x0 + lane, x0 + lane + 1 do
            turned(x, F + t - 1, z0 + lz, L.stair, facing)
        end
        return true
    end
    local function landing(a: number, b: number, lz: number)
        for x = x0 + a, x0 + b do
            core.set_block(x, F + t - 1, z0 + lz, L.floor)
        end
    end
    while t < rise do
        for lz = 2, 6 do
            step(1, lz, 0)
        end
        if step(1, 7, 0) then
            landing(3, 6, 7)
        end
        for lz = 6, 2, -1 do
            step(5, lz, 2)
        end
        if step(5, 1, 2) then
            landing(1, 4, 1)
        end
    end
end

-- A hole two by two in the floor of a room of level `hi`, walled down
-- through the gap with a lamp halfway, into a pool in the same room of `lo`.
local function hole(x0: number, z0: number, lo: number, hi: number)
    local L, U = LEVELS[lo], LEVELS[hi]
    core.replace_nodes(x0 + 2, L.F + L.h + 1, z0 + 2, x0 + 5, U.F - 2, z0 + 5, AIR, U.wall_block)
    core.replace_nodes(x0 + 3, L.F + L.h + 1, z0 + 3, x0 + 4, U.F - 2, z0 + 4, U.wall_block, AIR)
    core.set_block(x0 + 2, (L.F + L.h + U.F) // 2, z0 + 3, U.lamp)
    for x = x0 + 3, x0 + 4 do
        for z = z0 + 3, z0 + 4 do
            core.set_block(x, U.F - 1, z, AIR)
            core.set_block(x, L.F + L.h, z, AIR)
        end
    end
    pool(lo, x0 + 2, z0 + 2, x0 + 5, z0 + 5, 2)
end

-- The world's own two openings back to the sandbox: the first in the room at
-- the origin on Level 0 (a player comes in there and stands four in front of
-- it), the second in the same room of the poolrooms, facing +x -- found only
-- by somebody who has found a way up.
local P0 = { x = 4, z = 2 }
local P1 = { x = 2, z = 4 }

-- Nothing of the engine's terrain, and no sky: light is the lamps'.
core.mapgen_core(false)
core.world_sky(18, 16, 10)

core.register_mapgen("backrooms:rooms", function(ctx)
    local here = site(ctx.min_x // REGION, ctx.min_z // REGION)
    for L = 1, #LEVELS do
        make(L, ctx, here)
        for cz = ctx.min_z // C, ctx.max_z // C, 2 do
            for cx = ctx.min_x // C, ctx.max_x // C, 2 do
                if hall(L, cx, cz) then
                    build_hall(L, cx, cz)
                end
            end
        end
    end
    -- After every level, or a level made later would floor a hole over.
    for cz = ctx.min_z // C, ctx.max_z // C do
        for cx = ctx.min_x // C, ctx.max_x // C do
            local v = VERTICAL[vertical(cx, cz)]
            if v then
                (if v.stairs then stairwell else hole)(cx * C, cz * C, v.lo, v.hi)
            end
            -- A stairwell's shaft walls on its high sides, from the room
            -- whose low lines they are (the column may not be the shaft's).
            for _, n in { { cx - 1, cz, 0 }, { cx, cz - 1, 1 } } do
                local w = VERTICAL[vertical(n[1], n[2])]
                if w and w.stairs then
                    local Lw, Uw = LEVELS[w.lo], LEVELS[w.hi]
                    local x0, z0 = cx * C, cz * C
                    local x1, z1 = if n[3] == 0 then x0 else x0 + C, if n[3] == 0 then z0 + C else z0
                    core.replace_nodes(x0, Lw.F + Lw.h, z0, x1, Uw.F - 2, z1, AIR, Lw.wall_block)
                end
            end
        end
    end
    if ctx.min_x <= 0 and ctx.max_x >= 0 and ctx.min_z <= 0 and ctx.max_z >= 0 then
        doorway(LEVELS[1].F, P0.x, P0.z, true, 1)
        doorway(LEVELS[2].F, P1.x, P1.z, false, 2)
    end
end)

core.set_portal(0, P0.x + 0.5, LEVELS[1].F + 1, P0.z + 0.6)
core.set_portal(1, P1.x + 0.6, LEVELS[2].F + 1, P1.z + 0.5)

-- A player is the sandbox's robot (mods/robot, linked here as games/arpg's).
core.player_model(core.register_model("backrooms:robot", "robot.gltf"))
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
    core.register_model("backrooms:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
core.register_model("backrooms:robot_arms", "robot_arms.gltf")
core.eye_height(1.5)
core.movement(4.8, 8, 0, 8, -25)

-- What lives there (tools/make_backrooms_creatures.py, Blender): a Bacteria
-- comes out of Level 0's dead rooms, on its carpet, and a Smiler out of
-- Level 1's dark, on its slab; nothing in the poolrooms. They come out where
-- the light is 7 or less (core.register_mob), so a lit room is safe. Their
-- sounds are ElevenLabs' (bacteria_say.ogg, smiler_hurt.ogg, ...).
--                                                        life speed fight   dmg reach every
core.register_mob("backrooms:bacteria", core.register_model("backrooms:bacteria", "bacteria.gltf"),
                  30, 3.4, "melee", 4, 1.8, 0.9, "", 1, 0, false, 0, false, "backrooms:carpet")
core.register_mob("backrooms:smiler", core.register_model("backrooms:smiler", "smiler.gltf"),
                  20, 2.6, "melee", 3, 1.6, 1.2, "", 1, 0, false, 0, false, "backrooms:slab")
-- What they leave, now and then (0 or 1 each, core.mob_drop): a Bacteria a
-- filament, a Smiler a tooth of its grin. Two of each, in the bag's grid, are
-- the compass to the way out.
local FILAMENT, TOOTH = "backrooms:filament", "backrooms:smiler_tooth"
core.register_item(FILAMENT, 16, "", 0, 1.0, "")
core.register_item(TOOTH, 16, "", 0, 1.0, "")
core.register_model(`{FILAMENT}_held`, "filament_held.gltf")   -- tools/make_held_models.py
core.register_model(`{TOOTH}_held`, "smiler_tooth_held.gltf")
core.mob_drop("backrooms:bacteria", FILAMENT, 1)
core.mob_drop("backrooms:smiler", TOOTH, 1)

-- Nobody digs or builds their way out.
core.event.on("core:pre_break", function(ev)
    ev.cancel = true
end)
core.event.on("core:pre_place", function(ev)
    ev.cancel = true
end)

-- ------------------------------------------------------------- the portals

local NEAR = 40      -- a pair is open while somebody is this close, across
local MAX_OPEN = 7   -- 16 openings, two of them the world's
type Open = { s: Site, ya: number, yb: number }
local open: { [string]: Open } = {}
local where: { [number]: number } = {}   -- the level each player was last in
local wait = 0.0

-- Whether a body is on the last piece of a slide -- the next cell down is
-- water -- and about to land in its pool: the splash.
local function slide_end(x: number, y: number, z: number): boolean
    local bx, by, bz = math.floor(x), math.floor(y - 0.1), math.floor(z)
    for sy = by - 1, by do
        local b = core.get_block(bx, sy, bz)
        if b == SLIDE or b == TURN_A or b == TURN_B then
            local out = HEAD[core.get_facing(bx, sy, bz)]
            return core.get_block(bx + out[1], sy - 1, bz + out[2]) == WATER
        end
    end
    return false
end

-- ------------------------------------------------------------ the compasses

-- Two compasses, neither given: the one to the way in is found, now and then,
-- in a chest; the one to the way out is made of what the creatures leave. A
-- right click with one in
-- hand (on any block: core:used) says which way, how far and, from another
-- level, by what to go up or down -- the nearest stairwell or hole, since a
-- level is crossed only there. The way in is the room at the origin of Level
-- 0, in front of its portal; the way out is backrooms_exit's room on Level 1
-- (room 12, -9).
local COMPASS_IN, COMPASS_OUT = "backrooms:compass_in", "backrooms:compass_out"
core.register_item(COMPASS_IN, 1, "", 0, 1.0, "")
core.register_item(COMPASS_OUT, 1, "", 0, 1.0, "")
core.register_model(`{COMPASS_IN}_held`, "compass_in_held.gltf")    -- tools/make_held_models.py
core.register_model(`{COMPASS_OUT}_held`, "compass_out_held.gltf")
core.recipe_shapeless(COMPASS_OUT, 1, `{FILAMENT} {FILAMENT} {TOOTH} {TOOTH}`)
-- And the torch, rarer still in the chests: lit with a right click anywhere,
-- it stuns the creatures in its beam a moment (mods/backrooms_hand).
local TORCH = "backrooms:torch"
core.register_item(TORCH, 1, "", 0, 1.0, "")
core.register_model(`{TORCH}_held`, "torch_held.gltf")   -- tools/make_held_models.py
type Goal = { what: string, x: number, z: number, L: number }
local GOALS: { [string]: Goal } = {
    [COMPASS_IN]  = { what = "L'entrata", x = P0.x + 0.5, z = P0.z + 3.5, L = 1 },
    [COMPASS_OUT] = { what = "L'uscita", x = 12 * C + 4.5, z = -9 * C + 4.5, L = 3 },
}
local RANK = { [3] = 1, [1] = 2, [2] = 3 }   -- the levels from the bottom up
local SEARCH = 16                             -- rooms each way a stair or a hole is looked for

-- Which way (x, z) is from a player, by where they look, and how far.
local function bearing(p: number, x: number, z: number): string
    local px, _, pz = core.player_pos(p)
    local lx, _, lz = core.player_look(p)
    if px == nil or pz == nil or lx == nil or lz == nil then
        return ""
    end
    local dx, dz = x - px, z - pz
    local d = math.sqrt(dx * dx + dz * dz)
    if d < 3 then
        return "qui"
    end
    local fl = math.max(math.sqrt(lx * lx + lz * lz), 0.001)
    local fx, fz = lx / fl, lz / fl
    -- The look's right is (-fz, fx).
    local a = math.deg(math.atan2(dx * -fz + dz * fx, dx * fx + dz * fz))
    local SIDES = { "davanti", "davanti a destra", "a destra", "dietro a destra", "dietro",
                    "dietro a sinistra", "a sinistra", "davanti a sinistra" }
    return `{math.floor(d + 0.5)} m, {SIDES[math.floor((a + 22.5) / 45) % 8 + 1]}`
end

-- The nearest room from (x, z) with a way from level L up (or down) a level:
-- its middle, and "scale" or "buco". Down is a hole in L's floor or a
-- stairwell that comes up through it.
local function way(L: number, up: boolean, x: number, z: number): (number?, number, string)
    local rx, rz = x // C, z // C
    local best, bx, bz, by = math.huge, 0, 0, ""
    for cz = rz - SEARCH, rz + SEARCH do
        for cx = rx - SEARCH, rx + SEARCH do
            local v = VERTICAL[vertical(cx, cz)]
            if v and (if up then v.stairs and v.lo == L else v.hi == L) then
                local mx, mz = cx * C + 4.5, cz * C + 4.5
                local d = (mx - x) ^ 2 + (mz - z) ^ 2
                if d < best then
                    best, bx, bz, by = d, mx, mz, if v.stairs then "scale" else "buco"
                end
            end
        end
    end
    if best == math.huge then
        return nil, 0, ""
    end
    return bx, bz, by
end

local function read_compass(p: number, g: Goal)
    local x, y, z = core.player_pos(p)
    if x == nil or y == nil or z == nil then
        return
    end
    local L = level_at(y)
    if L == g.L then
        local b = bearing(p, g.x, g.z)
        core.notice(p, if b == "qui" then `{g.what} e' qui.` else `{g.what}: {b}.`)
        return
    end
    local steps = RANK[g.L] - RANK[L]
    local up = steps > 0
    local levels = if math.abs(steps) == 1 then "un livello" else "due livelli"
    local wx, wz, by = way(L, up, x, z)
    local head = `{g.what} e' {levels} {if up then "sopra" else "sotto"}`
    if wx == nil then
        core.notice(p, `{head}. Nessuna via per {if up then "salire" else "scendere"} qui vicino.`)
    elseif by == "scale" then
        core.notice(p, `{head}. Scale per {if up then "salire" else "scendere"}: {bearing(p, wx, wz)}.`)
    else
        core.notice(p, `{head}. Un buco nel pavimento: {bearing(p, wx, wz)}.`)
    end
end

-- A chest's first opening fills it, by a hash of where it is: the compass to
-- the way in in two chests out of five, the torch in one in eight, and a
-- filament or a tooth or two.
local CHEST_SLOTS = 9
local function fill_chest(x: number, y: number, z: number)
    if core.meta_get(x, y, z, "filled") ~= "" then
        return
    end
    core.meta_set(x, y, z, "filled", "1")
    core.container_make(x, y, z, CHEST_SLOTS)
    local h = hash(x, z, y * 7 + 5051)
    if h % 100 < 40 then
        core.container_add(x, y, z, core.item_id(COMPASS_IN), 1)
    end
    if bit32.rshift(h, 16) % 8 == 0 then
        core.container_add(x, y, z, core.item_id(TORCH), 1)
    end
    local bit = if level_at(y + 1) == 3 then TOOTH elseif level_at(y + 1) == 1 then FILAMENT
        elseif bit32.rshift(h, 8) % 2 == 0 then TOOTH else FILAMENT
    core.container_add(x, y, z, core.item_id(bit), 1 + bit32.rshift(h, 12) % 2)
end

core.event.on("core:used", function(ev)
    if ev.id == CHEST then
        ev.cancel = true
        fill_chest(ev.x, ev.y, ev.z)
        core.container_show(ev.x, ev.y, ev.z, CHEST_SLOTS, ev.player)
        return
    end
    local g = GOALS[ev.held]
    if g then
        ev.cancel = true
        read_compass(ev.player, g)
    end
end)

-- Level 0's hum: the tube lights over whoever is there, an 8 s drone played
-- again as it ends.
local HUM = 8
local clock = 0.0
local hum_at = 0.0
local splashed: { [number]: number } = {}   -- not again before this clock

-- Tubes that flicker: now and then a lit lamp of a room by somebody on Level
-- 0 or Level 1 goes dark for a tenth of a second to half a second, once or
-- twice. Written while the world runs, so each costs a relight of its cell
-- (core.set_block): a few a second at most.
type Flick = { x: number, y: number, z: number, on: number, at: number, left: number }
local flicks: { [string]: Flick } = {}
local flick_wait = 0.0
local seed = 1
local function rand(n: number): number
    seed = (seed * 16807) % 2147483647   -- Park-Miller: exact in a double
    return seed % n
end

local function flicker(dt: number)
    for key, f in flicks do
        if clock >= f.at then
            if core.get_block(f.x, f.y, f.z) == f.on then
                core.set_block(f.x, f.y, f.z, LEVELS[level_at(f.y - 1)].lamp_off or f.on)
                f.at = clock + 0.08 + rand(30) / 100
            else
                core.set_block(f.x, f.y, f.z, f.on)
                f.left -= 1
                f.at = clock + 0.05 + rand(20) / 100
                if f.left <= 0 then
                    flicks[key] = nil
                end
            end
        end
    end
    flick_wait -= dt
    if flick_wait > 0 then
        return
    end
    flick_wait = 0.3
    for _, p in core.players() do
        local x, y, z = core.player_pos(p)
        if x == nil or y == nil or z == nil or rand(100) >= 35 then
            continue
        end
        local L = level_at(y)
        local lv = LEVELS[L]
        if lv.lamp_off == nil then
            continue
        end
        local cx, cz = (x // C) + rand(5) - 2, (z // C) + rand(5) - 2
        local at = lv.lamps[rand(#lv.lamps) + 1]
        local lx, ly, lz = cx * C + at[1], lv.F + lv.h, cz * C + at[2]
        local key = `{lx},{ly},{lz}`
        if flicks[key] == nil and core.get_block(lx, ly, lz) == lv.lamp then
            flicks[key] = { x = lx, y = ly, z = lz, on = lv.lamp, at = clock, left = 1 + rand(2) }
        end
    end
end

core.event.on("core:tick", function(ev)
    clock += ev.dt
    if clock >= hum_at then
        hum_at = clock + HUM
        for _, p in core.players() do
            local x, y, z = core.player_pos(p)
            if x and y and z and level_at(y) == 1 then
                core.play_sound("backrooms:hum", x, LEVELS[1].F + LEVELS[1].h - 1, z)
            end
        end
    end
    flicker(ev.dt)
    for _, p in core.players() do
        local x, y, z = core.player_pos(p)
        if x and y and z and slide_end(x, y, z) then
            if (splashed[p] or 0) < clock then
                core.play_sound("backrooms:splash", x, y - 1, z)
            end
            splashed[p] = clock + 1
        end
    end
    wait -= ev.dt
    if wait > 0 then
        return
    end
    wait = 0.5
    local wanted: { [string]: Site } = {}
    local count = 0
    for _, p in core.players() do
        local x, y, z = core.player_pos(p)
        if x == nil or y == nil or z == nil then
            continue
        end
        local L = level_at(y)
        if where[p] ~= L then
            if where[p] ~= nil then
                core.notice(p, LEVELS[L].name)
            else
                core.notice(p, "Si dice che qualcuno abbia lasciato delle casse, qui. E che le creature tengano i pezzi di una bussola.")
            end
            where[p] = L
        end
        local rx, rz = x // REGION, z // REGION
        for drz = -2, 2 do
            for drx = -2, 2 do
                local s = site(rx + drx, rz + drz)
                if s and (s.x - x) ^ 2 + (s.z - z) ^ 2 < NEAR * NEAR then
                    wanted[`{s.cx},{s.cz}`] = s
                end
            end
        end
    end
    for key, o in open do
        if wanted[key] == nil then
            core.portal_close(o.s.x, o.ya, o.s.z)
            core.portal_close(o.s.x, o.yb, o.s.z)
            open[key] = nil
        else
            count += 1
        end
    end
    for key, s in wanted do
        if open[key] ~= nil or count >= MAX_OPEN then
            continue
        end
        local ya, yb = LEVELS[s.a].F + 1, LEVELS[s.b].F + 1
        local ia = core.portal_open(s.x, ya, s.z, 0, 1, 0, 0.5, 1.0)
        if ia < 0 then
            continue   -- no world yet, or no opening free: next time
        end
        local ib = core.portal_open(s.x, yb, s.z, 0, 1, 0, 0.5, 1.0)
        if ib < 0 then
            core.portal_close(s.x, ya, s.z)
            continue
        end
        core.portal_link(ia, ib)
        open[key] = { s = s, ya = ya, yb = yb }
        count += 1
    end
end)
