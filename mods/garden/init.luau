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
local COARSE = id("rocks:coarse_dirt")
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

-- The flowers' cells, for the insects (filled by build).
local BLOOMS: { { number } } = {}
-- The ants' nest: a patch of bare earth, its middle where they go home to.
local NEST_X, NEST_Z = 108, 35

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
    fill(NEST_X - 1, NEST_X + 1, GY, GY, NEST_Z - 1, NEST_Z + 1, COARSE)
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
            local nest = math.abs(x - NEST_X) <= 1 and math.abs(z - NEST_Z) <= 1
            if pond or trunk or stone or near_g or nest then continue end
            local h = pick(x, z)
            if h < 30 then core.set_block(x, GY + 1, z, TALL)
            elseif h < 36 then core.set_block(x, GY + 1, z, FERN)
            elseif h < 46 then
                core.set_block(x, GY + 1, z, FLOWERS[h % #FLOWERS + 1])
                table.insert(BLOOMS, { x, z })
            end
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

-- ================================================================ insects
--
-- At their true size (tools/make_garden_insects.py): an ant 5 cm long, a
-- ladybird 3, a butterfly 9 across -- seen from a sixteenth of a body, a dog,
-- a tortoise and a kite. Workers (register_mob "worker"): they come out of
-- nothing on their own and never vanish; this mod spawns and drives them.
--   ants        from the nest to a flower and back, a short stop at each end;
--   ladybirds   a few cells this way or that, then a rest;
--   butterflies from flower to flower in a swaying arc (core.mob_place every
--               tick, hover: no gravity), settling on each for a while.
-- One killed comes back after RESPAWN seconds.
local ANT = core.register_model("garden:ant", "ant.gltf")
local LADY = core.register_model("garden:ladybird", "ladybird.gltf")
local FLY = core.register_model("garden:butterfly", "butterfly.gltf")
--                 name               model hp  speed  fight     dmg reach every drop n shot doors hover fireball
core.register_mob("garden:ant", ANT, 2, 0.12, "worker", 0, 0, 1, "", 1, 0, false, 0, false, "")
core.register_mob("garden:ladybird", LADY, 2, 0.05, "worker", 0, 0, 1, "", 1, 0, false, 0, false, "")
core.register_mob("garden:butterfly", FLY, 1, 0.5, "worker", 0, 0, 1, "", 1, 0, false, 0.5, false, "")

local COUNT = { ["garden:ant"] = 8, ["garden:ladybird"] = 3, ["garden:butterfly"] = 4 }
local RESPAWN = 30
local FLOWER_TOP = 0.75   -- a flower's model is 0.8 high: where a butterfly sits

type Bug = { kind: string, id: number, wait: number, home: boolean,
             -- a butterfly's flight: from, to, how far along, how long
             fx: number, fy: number, fz: number, tx: number, ty: number, tz: number,
             t: number, len: number, flying: boolean }
local bugs: { Bug } = {}
local seed = 12345
local function rand(): number   -- small and fixed, so a run is a run
    seed = (seed * 1103515245 + 12345) % 2147483648
    return seed / 2147483648
end
local function bloom(): { number }
    return BLOOMS[math.floor(rand() * #BLOOMS) + 1]
end
local function inside(x: number, z: number): boolean
    return x >= GX0 and x <= GX1 and z >= GZ0 and z <= GZ1
end

local function spawn(b: Bug): boolean
    local x, y, z
    if b.kind == "garden:ant" then
        x, y, z = NEST_X + 0.5, GY + 1, NEST_Z + 0.5
    elseif b.kind == "garden:butterfly" then
        local f = bloom()
        x, y, z = f[1] + 0.5, GY + 1 + FLOWER_TOP, f[2] + 0.5
    else
        x, y, z = GX0 + 2 + rand() * (GX1 - GX0 - 4), GY + 1, GZ0 + 2 + rand() * (GZ1 - GZ0 - 4)
    end
    local id = core.spawn_mob(b.kind, x, y, z)
    if id == nil or id <= 0 then return false end
    b.id, b.wait, b.home, b.flying = id, 1 + rand() * 3, true, false
    b.fx, b.fy, b.fz = x, y, z
    return true
end

local function someone_near(): boolean
    for _, p in core.players() do
        local x, _, z = core.player_pos(p)
        if x and math.abs(x - 116) < 96 and math.abs(z - 42) < 96 then return true end
    end
    return false
end

local function walker(b: Bug, dt: number)
    if core.mob_going(b.id) == 1 then return end
    b.wait -= dt
    if b.wait > 0 then return end
    b.wait = 1 + rand() * 3
    if b.kind == "garden:ant" then
        if #BLOOMS == 0 then return end
        if b.home then
            local f = bloom()
            core.mob_goto(b.id, f[1], GY + 1, f[2])
        else
            core.mob_goto(b.id, NEST_X, GY + 1, NEST_Z)
        end
        b.home = not b.home
    else
        local x, _, z = core.mob_pos(b.id)
        if x == nil then return end
        local nx = math.floor(x + (rand() - 0.5) * 6)
        local nz = math.floor(z + (rand() - 0.5) * 6)
        if inside(nx, nz) then core.mob_goto(b.id, nx, GY + 1, nz) end
        b.wait = 2 + rand() * 6
    end
end

local function flyer(b: Bug, dt: number)
    if not b.flying then
        b.wait -= dt
        if b.wait > 0 or #BLOOMS == 0 then return end
        local x, y, z = core.mob_pos(b.id)
        if x == nil then return end
        local f = bloom()
        b.fx, b.fy, b.fz = x, y, z
        b.tx, b.ty, b.tz = f[1] + 0.5, GY + 1 + FLOWER_TOP, f[2] + 0.5
        local d = math.sqrt((b.tx - x) ^ 2 + (b.tz - z) ^ 2)
        b.t, b.len, b.flying = 0, math.max(1.5, d / 0.6), true
        core.mob_act(b.id, "fly", "")
        return
    end
    b.t = math.min(b.len, b.t + dt)
    local u = b.t / b.len
    local s = u * u * (3 - 2 * u)
    -- Up and over in an arc, swaying side to side and bobbing as it flaps.
    local dx, dz = b.tx - b.fx, b.tz - b.fz
    local d = math.max(0.01, math.sqrt(dx * dx + dz * dz))
    local sway = math.sin(u * math.pi * 3) * 0.25 * math.sin(u * math.pi)
    local x = b.fx + dx * s - dz / d * sway
    local z = b.fz + dz * s + dx / d * sway
    local y = b.fy + (b.ty - b.fy) * s + math.sin(u * math.pi) * (0.6 + d * 0.08)
              + math.sin(b.t * 9) * 0.03
    core.mob_place(b.id, x, y, z)
    core.mob_face(b.id, x + dx, z + dz)
    if u >= 1 then
        b.flying = false
        b.wait = 3 + rand() * 6
        core.mob_act(b.id, "", "")
    end
end

local started = false
core.event.on("core:tick", function(ev)
    if not started then
        if #BLOOMS == 0 or not someone_near() then return end
        started = true
        for kind, n in COUNT do
            for i = 1, n do
                -- The ants out of the nest one by one, a column along the trail.
                local first = if kind == "garden:ant" then (i - 1) * 3 else 0
                table.insert(bugs, { kind = kind, id = 0, wait = first, home = true, fx = 0, fy = 0, fz = 0,
                                     tx = 0, ty = 0, tz = 0, t = 0, len = 1, flying = false })
            end
        end
    end
    for _, b in bugs do
        if b.id > 0 and core.mob_pos(b.id) == nil then
            b.id, b.wait = 0, RESPAWN   -- killed: back after a while
        end
        if b.id == 0 then
            b.wait -= ev.dt
            if b.wait <= 0 and not spawn(b) then b.wait = 1 end
        elseif b.kind == "garden:butterfly" then
            flyer(b, ev.dt)
        else
            walker(b, ev.dt)
        end
    end
end)
