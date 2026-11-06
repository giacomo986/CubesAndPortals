--!strict
-- garden: a fenced garden of ordinary blocks east of the plaza, seen huge.
--
-- On the plaza (near 31 20 43) stand a LITTLE DOOR (D, door-sized, in a hedge) and a low
-- glass CASE in the ground, framed in planks, that shows the garden from
-- above in miniature.
--   D -> G   G is an opening an eighth of a block high on the garden floor,
--            both scaling (core.portal_scale): through D you come out of G a
--            sixteenth of your size, the grass as tall as trees; back into G,
--            out of D as big as before.
--   T -> S   T lies under the case's glass, S hangs over the garden facing
--            down, twelve times as wide: looking into T is looking down from
--            S, the garden twelve times smaller. Look only: the glass keeps
--            bodies out of T (and is put back if broken).
-- Only on a plaza; a scaling pair works within one world (the crossing into
-- another room does not carry the body's size).

if core.plaza() == nil then return end

local id = core.node_id
local AIR = 0
local GRASS, DIRT, STONE = id("core:grass"), id("core:dirt"), id("core:stone")
local GLASS, WATER = id("core:glass"), id("lakes:water")
local HEDGE, TRUNK = id("trees:leaves"), id("trees:trunk")
local PLANKS = id("woods:dark_oak_planks")
local TALL, FERN = id("plants:tall_grass"), id("plants:fern")
local RED_CAP, BROWN_CAP = id("plants:red_mushroom"), id("plants:brown_mushroom")
local FLOWERS = { id("plants:dandelion"), id("plants:poppy"), id("plants:cornflower"), id("plants:oxeye_daisy"),
                  id("plants:allium"), id("plants:red_tulip"), id("plants:white_tulip") }

-- The plaza's ground, the top block (core.terrain_height is the first air).
local Y = core.terrain_height(31, 42) - 1
-- The garden: x 104..127, z 30..53 inside its hedge, on a ground of its own
-- as high as the land round its middle.
local GX0, GX1, GZ0, GZ1 = 104, 127, 30, 53
local GY = core.terrain_height(116, 42) - 1
-- The case (x 30..33, z 42..45, glass x 31..32, z 43..44) and the door's
-- hedge behind it (z 39, the hole at x 31).
local TX0, TZ0 = 30, 42
local DX, DZ = 31, 39

local function fill(x0: number, x1: number, y0: number, y1: number, z0: number, z1: number, b: number)
    for x = x0, x1 do
        for y = y0, y1 do
            for z = z0, z1 do core.set_block(x, y, z, b) end
        end
    end
end

-- Small and fixed, so a hash of the cell picks what grows where.
local function pick(x: number, z: number): number
    return bit32.bxor(x * 73856093 % 4294967296, z * 19349663 % 4294967296) % 100
end

local function build()
    -- The garden: dirt under grass, a hedge three high round it, air above.
    fill(GX0 - 1, GX1 + 1, GY - 3, GY - 1, GZ0 - 1, GZ1 + 1, DIRT)
    fill(GX0 - 1, GX1 + 1, GY, GY, GZ0 - 1, GZ1 + 1, GRASS)
    fill(GX0 - 1, GX1 + 1, GY + 1, GY + 16, GZ0 - 1, GZ1 + 1, AIR)
    for x = GX0 - 1, GX1 + 1 do
        for z = GZ0 - 1, GZ1 + 1 do
            if x < GX0 or x > GX1 or z < GZ0 or z > GZ1 then fill(x, x, GY + 1, GY + 3, z, z, HEDGE) end
        end
    end
    -- A pond in one corner, a fallen trunk, a stone.
    fill(GX1 - 6, GX1 - 2, GY, GY, GZ1 - 6, GZ1 - 2, WATER)
    fill(GX0 + 3, GX0 + 9, GY + 1, GY + 1, GZ1 - 4, GZ1 - 4, TRUNK)
    fill(GX0 + 14, GX0 + 15, GY + 1, GY + 1, GZ0 + 4, GZ0 + 5, STONE)
    -- Grass, ferns and flowers, thinner round G (x 115, z 41..45) so it is
    -- seen from its front.
    for x = GX0, GX1 do
        for z = GZ0, GZ1 do
            -- Not by reading the cells: at world_ready a column not yet in
            -- memory reads air.
            local pond = x >= GX1 - 6 and x <= GX1 - 2 and z >= GZ1 - 6 and z <= GZ1 - 2
            local trunk = z == GZ1 - 4 and x >= GX0 + 3 and x <= GX0 + 9
            local stone = x >= GX0 + 14 and x <= GX0 + 15 and z >= GZ0 + 4 and z <= GZ0 + 5
            local near_g = math.abs(x - 115) <= 1 and z >= 41 and z <= 45
            if pond or trunk or stone or near_g then continue end
            local h = pick(x, z)
            if h < 30 then core.set_block(x, GY + 1, z, TALL)
            elseif h < 36 then core.set_block(x, GY + 1, z, FERN)
            elseif h < 46 then core.set_block(x, GY + 1, z, FLOWERS[h % #FLOWERS + 1]) end
        end
    end
    -- G's landmarks: a red mushroom and a brown one either side of it.
    core.set_block(114, GY + 1, 43, RED_CAP)
    core.set_block(116, GY + 1, 43, BROWN_CAP)

    -- The case: a glass floor framed in planks, flush with the ground; T
    -- just under the glass's top face (deeper, the frame's sides hide it
    -- from a few steps away), walked over.
    fill(TX0, TX0 + 3, Y, Y, TZ0, TZ0 + 3, PLANKS)
    fill(TX0 + 1, TX0 + 2, Y, Y, TZ0 + 1, TZ0 + 2, GLASS)
    -- The little door: a hedge five wide, three high, the hole in it.
    fill(DX - 2, DX + 2, Y + 1, Y + 3, DZ, DZ, HEDGE)
    fill(DX, DX, Y + 1, Y + 2, DZ, DZ, AIR)
end
core.event.on("core:world_ready", build)

-- The glass is the only thing between a body and T.
core.event.on("core:broken", function(ev)
    if ev.y == Y and ev.x >= TX0 + 1 and ev.x <= TX0 + 2 and ev.z >= TZ0 + 1 and ev.z <= TZ0 + 2 then
        core.set_block(ev.x, ev.y, ev.z, GLASS)
    end
end)

-- The openings exist a few ticks after the server listens: asked again
-- until they open.
local opened = false
local function open_all(): boolean
    local d = core.portal_open(DX + 0.5, Y + 2.0, DZ + 0.5, 0, 1, 0, 0.5, 1.0)
    local g = core.portal_open(115.5, GY + 1 + 1 / 16, 43.5, 0, 1, 0, 0.5 / 16, 1 / 16)
    local t = core.portal_open(TX0 + 2.0, Y + 0.95, TZ0 + 2.0, 2, 1, 0, 1.0, 1.0)
    local s = core.portal_open((GX0 + GX1 + 1) / 2, GY + 14.0, (GZ0 + GZ1 + 1) / 2, 2, -1, 0, 12.0, 12.0)
    if d < 0 or g < 0 or t < 0 or s < 0 then return false end
    core.portal_link(d, g)
    core.portal_scale(d, true)
    core.portal_scale(g, true)
    core.portal_link(t, s)
    core.portal_scale(t, true)
    core.portal_scale(s, true)
    core.log(`garden: door {d} -> {g}, case {t} -> {s}`)
    return true
end
core.event.on("core:tick", function(_)
    if not opened then opened = open_all() end
end)

-- A sixteenth of a body walks a sixteenth as fast: the garden was minutes
-- wide. Whoever comes in through D walks and runs four times as fast as that
-- (core.body_movement, whose speeds shrink with the body), and as before once
-- out of D. A crossing is seen as a jump from one end to the other between two
-- ticks: no event says it, and no call reads a body's size.
local BOOST = 4
local WALK, SPRINT, FLY, JUMP, GRAVITY = 4.8, 8.0, 14.0, 8.0, -25.0   -- the engine's (movement.h)
local last: { [number]: { number } } = {}
local small: { [number]: boolean } = {}
local function near(x: number, y: number, z: number, px: number, py: number, pz: number, r: number): boolean
    return math.abs(x - px) < r and math.abs(y - py) < 2 and math.abs(z - pz) < r
end
core.event.on("core:tick", function(_)
    local here: { [number]: boolean } = {}
    for _, p in core.players() do
        here[p] = true
        local x, y, z = core.player_pos(p)
        if x == nil then continue end
        local was = last[p]
        last[p] = { x, y, z }
        if was == nil or math.abs(x - was[1]) + math.abs(z - was[3]) < 20 then continue end
        if near(x, y, z, 115.5, GY + 1, 43.6, 1) and near(was[1], was[2], was[3], DX + 0.5, Y + 1, DZ + 0.5, 2) then
            small[p] = true
            core.body_movement(p, WALK * BOOST, SPRINT * BOOST, FLY, JUMP, GRAVITY)
        elseif small[p] and near(x, y, z, DX + 0.5, Y + 1, DZ + 0.6, 1) then
            small[p] = nil
            core.body_movement(p, WALK, SPRINT, FLY, JUMP, GRAVITY)
        end
    end
    -- Gone players' ids are not reused for them: forgotten.
    for p in last do
        if not here[p] then last[p] = nil; small[p] = nil end
    end
end)
