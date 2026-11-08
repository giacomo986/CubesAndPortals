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
-- G's frame: a doorframe of planks round the inch-high opening, in its cell
-- (115, GY + 1, 43); jambs and lintel a thirty-second of a node thick.
-- Seen huge from inside, a door of a house.
local FRAME = core.register_node("garden:frame", 74, 52, 32, true, 0, "", 0, 1.5, "wood")
-- Half as deep as G's doorway: a face on its clip plane (a sixteenth's 1/32
-- off the middle) was drawn as a black line across the door's picture.
local T, D = 1 / 32, 1 / 64
core.node_box(FRAME, 0.5 - 1 / 32 - T, 0, 0.5 - D, 0.5 - 1 / 32, 1 / 8 + T, 0.5 + D)
core.node_box(FRAME, 0.5 + 1 / 32, 0, 0.5 - D, 0.5 + 1 / 32 + T, 1 / 8 + T, 0.5 + D)
core.node_box(FRAME, 0.5 - 1 / 32, 1 / 8, 0.5 - D, 0.5 + 1 / 32, 1 / 8 + T, 0.5 + D)
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
    core.set_block(115, GY + 1, 43, FRAME)

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
    elseif ev.x == 115 and ev.y == GY + 1 and ev.z == 43 then
        core.set_block(ev.x, ev.y, ev.z, FRAME)   -- and G's frame
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

-- Small, a body walks as a body at 1 does for its size, a sixteenth as fast:
-- Giacomo chose the same feel as at full size (2026-11-07) over a boost.

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

-- A ladybird to ride. Right click one with a saddle and it swells to a pony's
-- size against you and carries you (core.player_wear and core.body_mount, as
-- mods/animals' pony; its clips from tools/make_ladybird_mount.py), and it is
-- tame from then on: saddled, it waits where you get off (Ctrl, or a right
-- click on the ground) and a right click rides it again. Ridden out through the
-- little door it comes out as big as you do, a ladybird a good metre long.
-- Tame ones are kept in the mod's storage: the engine saves no creatures.
local MOUNT = {
    RIDDEN = core.register_model("garden:ladybird_ridden", "ladybird_ridden.gltf"),
    SLOT = 9,               -- the pony's: nobody rides two
    WALK = 7.0, CANTER = 11.0, JUMP = 11.0,
    BIG = "garden:ladybird_tame", SMALL = "garden:ladybird_tame_small",
    riding = {} :: { [number]: { x: number, y: number, z: number, clip: string } },
    tame = {} :: { { x: number, y: number, z: number, small: boolean, id: number } },
    since = 0,
    loaded = false,
}
core.register_mob(MOUNT.BIG, core.register_model(MOUNT.BIG, "ladybird_tame.gltf"), 16, 1, "worker", 0, 0, 1, "", 1, 0, false, 0, false, "")
core.register_mob(MOUNT.SMALL, core.register_model(MOUNT.SMALL, "ladybird_tame_small.gltf"), 16, 0.1, "worker", 0, 0, 1, "", 1, 0, false, 0, false, "")

function MOUNT.save()
    local out = {}
    for _, t in MOUNT.tame do
        table.insert(out, string.format("%.2f %.2f %.2f %d", t.x, t.y, t.z, if t.small then 1 else 0))
    end
    core.storage_set("tame", table.concat(out, ";"))
end

function MOUNT.load()
    for _, line in string.split(core.storage_get("tame"), ";") do
        local w = string.split(line, " ")
        if #w == 4 then
            table.insert(MOUNT.tame, { x = tonumber(w[1]) or 0, y = tonumber(w[2]) or 0, z = tonumber(w[3]) or 0,
                                       small = w[4] == "1", id = 0 })
        end
    end
end

function MOUNT.on(p: number, x: number, y: number, z: number)
    MOUNT.riding[p] = { x = x, y = y, z = z, clip = "ride_idle" }
    core.player_clip(p, "ride_idle")
    core.player_place(p, x, y, z)
    core.player_wear(p, MOUNT.SLOT, MOUNT.RIDDEN)
    core.body_movement(p, MOUNT.WALK, MOUNT.CANTER, 0, MOUNT.JUMP)
    core.body_mount(p, true)
    core.notice(p, "Riding: Ctrl, or right click the ground, to get off")
end

function MOUNT.off(p: number, x: number?, y: number?, z: number?)
    local was = MOUNT.riding[p]
    if was == nil then return end
    MOUNT.riding[p] = nil
    local small = core.body_size(p) ~= 0 and core.body_size(p) < 0.5
    table.insert(MOUNT.tame, { x = x or was.x, y = y or was.y, z = z or was.z, small = small, id = 0 })
    MOUNT.save()
    if core.player_pos(p) ~= nil then
        core.player_wear(p, MOUNT.SLOT, 0)
        core.player_clip(p, "")
        core.body_mount(p, false)
        core.body_movement(p)
    end
end

core.event.on("core:used_mob", function(ev)
    local kind, p = core.mob_kind(ev.target), ev.player
    if MOUNT.riding[p] then return end
    local tamed = kind == MOUNT.BIG or kind == MOUNT.SMALL
    if not tamed and (kind ~= "garden:ladybird" or ev.held ~= "animals:saddle") then return end
    local x, y, z = core.mob_pos(ev.target)
    if x == nil then return end
    if tamed then
        if (kind == MOUNT.SMALL) ~= (core.body_size(p) < 0.5) then
            core.notice(p, "This ladybird is not your size")
            return
        end
        for i, t in MOUNT.tame do
            if t.id == ev.target then table.remove(MOUNT.tame, i); break end
        end
        MOUNT.save()
    elseif core.take_item(p, "animals:saddle", 1) < 1 then
        return
    end
    core.mob_remove(ev.target)
    MOUNT.on(p, x, y, z)
end)

core.event.on("core:used", -20, function(ev)
    if MOUNT.riding[ev.player] and core.get_block(ev.x, ev.y + 1, ev.z) == 0 then
        ev.cancel = true
        local x, y, z = core.player_pos(ev.player)
        MOUNT.off(ev.player, ev.x + 0.5, ev.y + 1, ev.z + 0.5)
        if x then core.player_place(ev.player, x, y, z) end
    end
end)

core.event.on("core:died", function(ev) MOUNT.off(ev.player, ev.x, ev.y, ev.z) end)

core.event.on("core:tick", function(ev)
    if not MOUNT.loaded then   -- the storage is not there while mods load
        MOUNT.loaded = true
        MOUNT.load()
    end
    for p in MOUNT.riding do
        if core.player_crouching(p) then
            local x, y, z = core.player_pos(p)
            local lx, _, lz = core.player_look(p)
            if x and lx then
                local len, k = math.max(0.01, math.sqrt(lx * lx + lz * lz)), 1.2 * core.body_size(p)
                MOUNT.off(p, x - lz / len * k, y, z + lx / len * k)
            else
                MOUNT.off(p)
            end
        end
    end
    MOUNT.since += ev.dt
    if MOUNT.since < 0.5 then return end
    MOUNT.since = 0
    local gone = false
    for i = #MOUNT.tame, 1, -1 do
        local t = MOUNT.tame[i]
        if t.id == 0 then
            if core.loaded(math.floor(t.x), math.floor(t.y), math.floor(t.z)) == 1 then
                t.id = core.spawn_mob(if t.small then MOUNT.SMALL else MOUNT.BIG, t.x, t.y, t.z) or 0
            end
        elseif core.mob_pos(t.id) == nil then
            table.remove(MOUNT.tame, i)   -- killed
            gone = true
        end
    end
    if gone then MOUNT.save() end
    local here = {}
    for _, p in core.players() do
        here[p] = true
        local r = MOUNT.riding[p]
        local x, y, z = core.player_pos(p)
        if r and x then
            local far = 0.3 * core.body_size(p)
            local clip = if (x - r.x) ^ 2 + (z - r.z) ^ 2 > far * far then "ride" else "ride_idle"
            if clip ~= r.clip then
                r.clip = clip
                core.player_clip(p, clip)
            end
            r.x, r.y, r.z = x, y, z
        end
    end
    for p in MOUNT.riding do
        if not here[p] then MOUNT.off(p) end
    end
end)

local COUNT = { ["garden:ant"] = 8, ["garden:ladybird"] = 3, ["garden:butterfly"] = 4 }
local RESPAWN = 30
local FLOWER_TOP = 0.75   -- a flower's model is 0.8 high: where a butterfly sits

type Bug = { kind: string, id: number, wait: number, home: boolean,
             -- a butterfly's flight: from, to, how far along, how long
             fx: number, fy: number, fz: number, tx: number, ty: number, tz: number,
             t: number, len: number, flying: boolean,
             -- an ant's lane: how far to one side of the straight road it keeps
             lane: number }
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

-- An ant walks a straight road, a lane of its own to one side of it (mob_goto
-- goes by cell centres: the whole column on one line, overlapping), and only
-- to a flower it can reach so: no trunk, stone, hedge or pond on the way.
local ANT_SPEED = 0.15
local BLOCKS_ANT = { [TRUNK] = true, [STONE] = true, [HEDGE] = true }
local function clear_road(x0: number, z0: number, x1: number, z1: number): boolean
    local d = math.sqrt((x1 - x0) ^ 2 + (z1 - z0) ^ 2)
    for k = 0, math.ceil(d / 0.25) do
        local u = if d > 0 then math.min(1, k * 0.25 / d) else 1
        local x, z = math.floor(x0 + (x1 - x0) * u), math.floor(z0 + (z1 - z0) * u)
        if BLOCKS_ANT[core.get_block(x, GY + 1, z)] or core.get_block(x, GY, z) == WATER then return false end
    end
    return true
end
local function ant(b: Bug, dt: number)
    if not b.flying then
        b.wait -= dt
        if b.wait > 0 or #BLOOMS == 0 then return end
        local x, _, z = core.mob_pos(b.id)
        if x == nil then return end
        local tx, tz
        if b.home then
            local f = bloom()
            tx, tz = f[1] + 0.5, f[2] + 0.5
        else
            tx, tz = NEST_X + 0.5 + (rand() - 0.5) * 1.5, NEST_Z + 0.5 + (rand() - 0.5) * 1.5
        end
        if not clear_road(x, z, tx, tz) then b.wait = 0.5; return end   -- another flower, soon
        b.fx, b.fz, b.tx, b.tz = x, z, tx, tz
        b.t, b.len, b.flying = 0, math.max(0.5, math.sqrt((tx - x) ^ 2 + (tz - z) ^ 2) / ANT_SPEED), true
        b.home = not b.home
        core.mob_act(b.id, "walk", "")
        return
    end
    b.t = math.min(b.len, b.t + dt)
    local u = b.t / b.len
    local dx, dz = b.tx - b.fx, b.tz - b.fz
    local d = math.max(0.01, math.sqrt(dx * dx + dz * dz))
    -- Into its lane and out of it at the ends, wandering a little as ants do.
    local side = (b.lane + math.sin(b.t * 2.3 + b.lane * 40) * 0.012) * math.sin(u * math.pi)
    local x = b.fx + dx * u - dz / d * side
    local z = b.fz + dz * u + dx / d * side
    core.mob_place(b.id, x, GY + 1, z)
    core.mob_face(b.id, x + dx, z + dz)
    if u >= 1 then
        b.flying = false
        b.wait = 1 + rand() * 3
        core.mob_act(b.id, "", "")
    end
end

local function walker(b: Bug, dt: number)
    if b.kind == "garden:ant" then return ant(b, dt) end
    if core.mob_going(b.id) == 1 then return end
    b.wait -= dt
    if b.wait > 0 then return end
    b.wait = 1 + rand() * 3
    do
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
                                     tx = 0, ty = 0, tz = 0, t = 0, len = 1, flying = false,
                                     lane = ((i - 1) % 5 - 2) * 0.035 })
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
