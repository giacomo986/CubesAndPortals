--!strict
-- inferno: a world in the Inferno's manner. Caverns of scoria between a floor
-- and a ceiling of bedrock, seas of lava at the bottom, glowrock hanging from
-- the roof, ash sand on the shores, quartz in the rock, and fortresses: lanes
-- of brick that bridge the caverns and tunnel through the rock between them.
-- No sun and a red sky (core.world_sky), so the only light is the lava's and the
-- glowrock's, and the creatures of mods/mobs come out of the dark everywhere.
--
-- The engine makes none of the ground (core.mapgen_core(false)): the stage below
-- is the whole world. It is reached through the world's first opening, which
-- stands in a chamber with a frame of obsidian round it.

local SCORIA = core.register_node("inferno:scoria", 110, 36, 34, true, 0, "", 0, 0.6, "stone")
local ASH_SAND  = core.register_node("inferno:ash_sand", 84, 64, 52, true, 0, "", 0, 0.7, "soil")
local GLOWROCK  = core.register_node("inferno:glowrock", 240, 200, 110, true, 15, "", 0, 0.4, "")
local QUARTZ     = core.register_node("inferno:quartz_ore", 150, 70, 64, true, 0, "", 1, 2.0, "stone")
local BRICKS     = core.register_node("inferno:bricks", 50, 22, 28, true, 0, "", 1, 3.0, "stone")
local OBSIDIAN   = core.register_node("inferno:obsidian", 22, 16, 34, true, 0, "", 1, 12.0, "stone")
local LAVA       = core.register_node("inferno:lava", 230, 100, 20, false, 15)
core.node_liquid(LAVA, 250)

-- Lava that flows, in the manner of mods/lakes's water and slower: a level for
-- each cell of its reach, the source 5 and four levels of flow after it.
local LAVA_SOURCE = 5
local LAVA_FLOW: { number } = {}
local LAVA_LEVEL: { [number]: number } = { [LAVA] = LAVA_SOURCE }
local LAVA_FLOW_NAMES: { string } = {}
for level = 1, LAVA_SOURCE - 1 do
    local name = `inferno:lava_flow{level}`
    local id   = core.register_node(name, 230, 100, 20, false, 15)
    core.node_liquid(id, 250, level * 2)
    LAVA_FLOW[level] = id
    LAVA_LEVEL[id]   = level
    table.insert(LAVA_FLOW_NAMES, name)
end

local BEDROCK = core.node_id("core:bedrock")

core.world_sky(58, 12, 10)
core.mapgen_core(false)
-- The sandbox's mods are loaded here too, so that what a player carries in is
-- worth something; their lakes, trees and ores are not this world's ground.
for _, mod in { "inferno", "inferno_heart", "inferno_track", "track_game" } do
    core.mapgen_only(mod)
end

-- The Inferno's own creatures, beside mods/mobs's zombie and skeleton, drawn by
-- tools/make_inferno_models.py. What they leave, and nothing yet uses it.
core.register_item("inferno:gold_nugget", 64, "", 0, 1.0, "")
core.register_item("inferno:wraith_tear", 64, "", 0, 1.0, "")
core.register_item("inferno:cinder_rod", 64, "", 0, 1.0, "")
local FIREBALL = core.register_model("inferno:fireball", "fireball.gltf")

-- A brute: the zombie's body and clips in its own colours, a little quicker
-- and harder.
core.register_mob("inferno:brute", core.register_model("inferno:brute", "brute.gltf"), 20, 2.4,
                  "melee", 4, 1.6, 1.2, "inferno:gold_nugget", 1)

-- A wraith floats six over whoever it has seen and throws fireballs from as far
-- as 24: slow ones, that a player can step out of the way of.
core.register_mob("inferno:wraith", core.register_model("inferno:wraith", "wraith.gltf"), 10, 1.6,
                  "archer", 5, 24.0, 3.0, "inferno:wraith_tear", 1, FIREBALL, false, 6.0, true)

-- A cinder hovers two over them and throws fireballs from 16, and comes out only
-- on the bricks of a fortress.
core.register_mob("inferno:cinder", core.register_model("inferno:cinder", "cinder.gltf"), 20, 1.8,
                  "archer", 4, 16.0, 2.0, "inferno:cinder_rod", 1, FIREBALL, false, 2.0, true,
                  "inferno:bricks")

local LAVA_TOP = 12     -- the seas: every open cell at or under this is lava
local S        = 4      -- the density is worked out every S cells and blended between
local FLOOR    = 30     -- where the arrival chamber stands
local PX, PZ   = 32, 32 -- and the cell its opening is in

-- The arrival: the first opening, facing +Z. The second is buried where nobody
-- goes: a world here has one door, back where the player came from.
core.set_portal(0, PX + 0.5, FLOOR + 1, PZ + 0.5)
core.set_portal(1, 8.5, 3, 8.5)

-- A 32-bit multiply that stays exact in a double (mods/ore says why).
local function mul32(a: number, b: number): number
    local hi = math.floor(a / 65536)
    local lo = a % 65536
    return ((hi * b) % 65536 * 65536 + lo * b) % 4294967296
end

local function hash(x: number, y: number, z: number, salt: number): number
    local h = mul32(x + 0x9E3779B9, 374761393)
    h = bit32.bxor(h, mul32(y + 0x7F4A7C15, 2654435761))
    h = bit32.bxor(h, mul32(z + 0x85EBCA6B, 668265263))
    h = bit32.bxor(h, mul32(salt % 4294967296, 2246822519))
    h = bit32.bxor(h, bit32.rshift(h, 13))
    return mul32(h, 1274126177)
end

local function unit(x: number, y: number, z: number, salt: number): number
    return hash(x, y, z, salt) / 4294967296
end

local function smooth(t: number): number
    return t * t * (3 - 2 * t)
end

-- Value noise in three dimensions: random numbers on a lattice `l` apart across
-- and `h` apart up, blended smoothly between.
local function noise3(x: number, y: number, z: number, l: number, h: number, salt: number): number
    local fx, fy, fz = x / l, y / h, z / l
    local ix, iy, iz = math.floor(fx), math.floor(fy), math.floor(fz)
    local tx, ty, tz = smooth(fx - ix), smooth(fy - iy), smooth(fz - iz)
    local function at(a: number, b: number, c: number): number
        return unit(ix + a, iy + b, iz + c, salt)
    end
    local x00 = at(0, 0, 0) + (at(1, 0, 0) - at(0, 0, 0)) * tx
    local x10 = at(0, 1, 0) + (at(1, 1, 0) - at(0, 1, 0)) * tx
    local x01 = at(0, 0, 1) + (at(1, 0, 1) - at(0, 0, 1)) * tx
    local x11 = at(0, 1, 1) + (at(1, 1, 1) - at(0, 1, 1)) * tx
    local y0 = x00 + (x10 - x00) * ty
    local y1 = x01 + (x11 - x01) * ty
    return y0 + (y1 - y0) * tz
end

-- How much rock there is at a point: two octaves of noise. Above the threshold
-- of that height is rock, under it a cavern.
local function density(x: number, y: number, z: number, seed: number): number
    return 0.62 * noise3(x, y, z, 22, 13, seed + 11) + 0.38 * noise3(x, y, z, 9, 6, seed + 29)
end

-- Rock closes in near the floor and the roof, and the middle is open.
local function threshold(y: number, height: number): number
    local t = 0.5
    if y < 8 then t -= (8 - y) * 0.06 end
    if y > height - 12 then t -= (y - (height - 12)) * 0.07 end
    return t
end

-- A fortress lane: every 96 nodes a lane of brick runs along one axis at a height
-- of its own, in stretches of 64 of which two in three are built.
local LANE = 96

local function lane(k: number, salt: number): (number, number)
    local h = hash(k, 0, 0, salt)
    return k * LANE + 24 + h % 40, 24 + bit32.rshift(h, 8) % 12   -- first cell across, deck height
end

local function built(k: number, along: number, salt: number): boolean
    return hash(k, math.floor(along / 64), 1, salt) % 3 ~= 0
end

local world_h = 64   -- how tall this world is, as the first stage run says

core.register_mapgen("inferno:caverns", function(ctx)
    local H, seed = ctx.height, ctx.seed
    world_h = H
    local x0, z0 = ctx.min_x, ctx.min_z
    local W = ctx.max_x - ctx.min_x + 1

    -- The density on the coarse lattice, then blended to every cell.
    local ns, ny = W / S + 1, math.floor(H / S) + 1
    local d: { number } = {}
    for i = 0, ns - 1 do
        for k = 0, ns - 1 do
            for j = 0, ny - 1 do
                d[(i * ns + k) * ny + j + 1] = density(x0 + i * S, j * S, z0 + k * S, seed)
            end
        end
    end

    local cut: { number } = {}
    for y = 0, H - 1 do cut[y + 1] = threshold(y, H) end

    -- What each cell became, for the passes after: 0 air, 1 rock, 2 lava.
    local cell: { number } = {}
    local function idx(dx: number, y: number, dz: number): number
        return (dx * W + dz) * H + y + 1
    end

    for dx = 0, W - 1 do
        local i, tx = dx // S, (dx % S) / S
        for dz = 0, W - 1 do
            local k, tz = dz // S, (dz % S) / S
            local x, z = x0 + dx, z0 + dz
            local b00 = (i * ns + k) * ny + 1
            local b10 = ((i + 1) * ns + k) * ny + 1
            local b01 = (i * ns + k + 1) * ny + 1
            local b11 = ((i + 1) * ns + k + 1) * ny + 1
            core.set_block(x, 0, z, BEDROCK)
            core.set_block(x, H - 1, z, BEDROCK)
            cell[idx(dx, 0, dz)] = 1
            cell[idx(dx, H - 1, dz)] = 1
            for y = 1, H - 2 do
                local j, ty = y // S, (y % S) / S
                local a = d[b00 + j] + (d[b10 + j] - d[b00 + j]) * tx
                local b = d[b01 + j] + (d[b11 + j] - d[b01 + j]) * tx
                local c = d[b00 + j + 1] + (d[b10 + j + 1] - d[b00 + j + 1]) * tx
                local e = d[b01 + j + 1] + (d[b11 + j + 1] - d[b01 + j + 1]) * tx
                local lo = a + (b - a) * tz
                local hi = c + (e - c) * tz
                local v = lo + (hi - lo) * ty
                if v > cut[y + 1] then
                    cell[idx(dx, y, dz)] = 1
                    core.set_block(x, y, z, if hash(x, y, z, seed + 5) % 90 == 0 then QUARTZ else SCORIA)
                elseif y <= LAVA_TOP then
                    cell[idx(dx, y, dz)] = 2
                    core.set_block(x, y, z, LAVA)
                else
                    cell[idx(dx, y, dz)] = 0
                end
            end
        end
    end

    -- Ash sand on the shores of the seas, glowrock hanging from the roof.
    for dx = 0, W - 1 do
        for dz = 0, W - 1 do
            local x, z = x0 + dx, z0 + dz
            local sandy = noise3(x, 0, z, 14, 1, seed + 41) > 0.56
            for y = LAVA_TOP + 1, H - 3 do
                local here, up = cell[idx(dx, y, dz)], cell[idx(dx, y + 1, dz)]
                if sandy and here == 1 and up == 0 and y <= LAVA_TOP + 7 then
                    core.set_block(x, y, z, ASH_SAND)
                    if cell[idx(dx, y - 1, dz)] == 1 then core.set_block(x, y - 1, z, ASH_SAND) end
                elseif here == 0 and up == 1 and y >= H - 26 then
                    local h = hash(x, y, z, seed + 53)
                    if h % 45 == 0 then
                        -- A cluster, down from the roof: this cell and a few
                        -- under and beside it, as the bits of the hash say.
                        core.set_block(x, y, z, GLOWROCK)
                        for n = 1, 3 do
                            if bit32.band(bit32.rshift(h, 8 + n), 1) == 1 and cell[idx(dx, y - n, dz)] == 0 then
                                core.set_block(x, y - n, z, GLOWROCK)
                            end
                        end
                        for n, off in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
                            local ax, az = dx + off[1], dz + off[2]
                            if ax >= 0 and ax < W and az >= 0 and az < W and
                                bit32.band(bit32.rshift(h, 16 + n), 1) == 1 and cell[idx(ax, y, az)] == 0 then
                                core.set_block(x + off[1], y, z + off[2], GLOWROCK)
                            end
                        end
                    end
                end
            end
        end
    end

    -- A fall of lava out of a cavern wall, now and then: a source in the rock
    -- beside the open air, and the first flow in the air, which the modifier
    -- carries down from there. One chance in two a column, so the flows stay
    -- few enough to sweep.
    do
        local h = hash(x0, 0, z0, seed + 97)
        if h % 2 == 0 then
            local dx, dz = 2 + bit32.rshift(h, 4) % (W - 4), 2 + bit32.rshift(h, 12) % (W - 4)
            for y = H - 12, LAVA_TOP + 8, -1 do
                if cell[idx(dx, y, dz)] == 1 and cell[idx(dx + 1, y, dz)] == 0 and
                    cell[idx(dx + 1, y - 1, dz)] == 0 then
                    core.set_block(x0 + dx, y, z0 + dz, LAVA)
                    core.set_block(x0 + dx + 1, y, z0 + dz, LAVA_FLOW[LAVA_SOURCE - 1])
                    break
                end
            end
        end
    end

    -- The fortresses: a deck five wide at the lane's height, a rail on each side,
    -- room over it to walk (a tunnel where it goes through the rock), and a
    -- pillar down to whatever holds it up every sixteen nodes.
    local function deck(x: number, y: number, z: number, across: number, pillar: boolean,
                        ax: boolean)
        local rail = across == 0 or across == 4
        core.set_block(x, y, z, BRICKS)
        for up = 1, 4 do
            core.set_block(x, y + up, z, if rail and up == 1 then BRICKS else 0)
        end
        core.set_block(x, y + 5, z, if cell[idx(x - x0, y + 5, z - z0)] == 1 then BRICKS else 0)
        if pillar and not rail then
            local py = y - 1
            while py >= 1 and cell[idx(x - x0, py, z - z0)] ~= 1 do
                core.set_block(x, py, z, BRICKS)
                py -= 1
            end
        end
    end
    for k = math.floor(z0 / LANE) - 1, math.floor(ctx.max_z / LANE) do
        local first, y = lane(k, seed + 71)
        for across = 0, 4 do
            local z = first + across
            if z >= z0 and z <= ctx.max_z then
                for x = x0, ctx.max_x do
                    if built(k, x, seed + 71) then deck(x, y, z, across, x % 16 == 0, true) end
                end
            end
        end
    end
    for k = math.floor(x0 / LANE) - 1, math.floor(ctx.max_x / LANE) do
        local first, y = lane(k, seed + 83)
        for across = 0, 4 do
            local x = first + across
            if x >= x0 and x <= ctx.max_x then
                for z = z0, ctx.max_z do
                    if built(k, z, seed + 83) then deck(x, y, z, across, z % 16 == 0, false) end
                end
            end
        end
    end

    -- The arrival: a chamber round the opening, a floor under it, glowrock in
    -- its corners, and the frame of obsidian.
    for x = math.max(x0, PX - 8), math.min(ctx.max_x, PX + 8) do
        for z = math.max(z0, PZ - 8), math.min(ctx.max_z, PZ + 8) do
            for y = FLOOR - 3, FLOOR - 1 do core.set_block(x, y, z, SCORIA) end
            for y = FLOOR, FLOOR + 6 do core.set_block(x, y, z, 0) end
            if (x == PX - 8 or x == PX + 8) and (z == PZ - 8 or z == PZ + 8) then
                core.set_block(x, FLOOR + 6, z, GLOWROCK)
            end
        end
    end
    -- And four ways out of it, one each way, 48 nodes: a tunnel through the
    -- rock, and a walkway of brick where it crosses a cavern or the lava.
    for _, dir in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
        for along = 9, 56 do
            for side = -1, 1 do
                local x = PX + dir[1] * along + dir[2] * side
                local z = PZ + dir[2] * along + dir[1] * side
                if x >= x0 and x <= ctx.max_x and z >= z0 and z <= ctx.max_z then
                    if cell[idx(x - x0, FLOOR - 1, z - z0)] ~= 1 then
                        core.set_block(x, FLOOR - 1, z, BRICKS)
                    end
                    for y = FLOOR, FLOOR + 3 do core.set_block(x, y, z, 0) end
                end
            end
        end
    end

    local function frame(x: number, y: number, z: number)
        if x >= x0 and x <= ctx.max_x and z >= z0 and z <= ctx.max_z then
            core.set_block(x, y, z, OBSIDIAN)
        end
    end
    for y = FLOOR - 1, FLOOR + 2 do
        frame(PX - 1, y, PZ)
        frame(PX + 1, y, PZ)
    end
    frame(PX, FLOOR - 1, PZ)
    frame(PX, FLOOR + 2, PZ)
end)

-- Lava flows, once a second: mods/lakes's rule for water. Down when it can
-- (arriving with its whole reach again), sideways one level weaker onto
-- something that holds it up, and a flow nothing feeds any more drains.
local SIDES = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }

local function lava_at(x: number, y: number, z: number): number
    return LAVA_LEVEL[core.get_block(x, y, z)] or 0
end

local function lava_spreads(x: number, y: number, z: number, level: number): boolean
    if level <= 1 then return false end
    local below = if y > 0 then core.get_block(x, y - 1, z) else -1
    return below ~= 0 and (level == LAVA_SOURCE or LAVA_LEVEL[below] == nil)
end

local function step_lava(x: number, y: number, z: number)
    local level = lava_at(x, y, z)
    if level == 0 then return end
    local below = if y > 0 then core.get_block(x, y - 1, z) else -1
    if below == 0 then
        core.set_block(x, y - 1, z, LAVA_FLOW[LAVA_SOURCE - 1])
        return
    end
    if (LAVA_LEVEL[below] or LAVA_SOURCE) < LAVA_SOURCE - 1 then
        core.set_block(x, y - 1, z, LAVA_FLOW[LAVA_SOURCE - 1])
    end
    if not lava_spreads(x, y, z, level) then return end
    for _, side in SIDES do
        local nx, nz = x + side[1], z + side[2]
        local there = core.get_block(nx, y, nz)
        if there == 0 or (LAVA_LEVEL[there] or LAVA_SOURCE) < level - 1 then
            core.set_block(nx, y, nz, LAVA_FLOW[level - 1])
        end
    end
end

local function lava_fed(x: number, y: number, z: number, level: number): boolean
    if lava_at(x, y + 1, z) > 0 then return true end
    for _, side in SIDES do
        local nx, nz = x + side[1], z + side[2]
        local there = lava_at(nx, y, nz)
        if there > level and lava_spreads(nx, y, nz, there) then return true end
    end
    return false
end

core.register_abm("inferno:lava_flow", LAVA_FLOW_NAMES, 1.0, 1, function(cell)
    local level = LAVA_LEVEL[cell.id] or 0
    if level == 0 then return end
    if not lava_fed(cell.x, cell.y, cell.z, level) then
        core.set_block(cell.x, cell.y, cell.z, 0)
        return
    end
    step_lava(cell.x, cell.y, cell.z)
end)

-- A dig beside or under lava opens a way for it.
core.event.on("core:broken", function(ev)
    if core.get_block(ev.x, ev.y, ev.z) ~= 0 then return end
    if lava_at(ev.x, ev.y + 1, ev.z) > 0 then step_lava(ev.x, ev.y + 1, ev.z) end
    for _, side in SIDES do
        local nx, nz = ev.x + side[1], ev.z + side[2]
        if lava_at(nx, ev.y, nz) > 0 then step_lava(nx, ev.y, nz) end
    end
end)

-- Lava burns: a body in it loses three points every half second.
local burn = 0.0
core.event.on("core:tick", function(ev)
    burn += ev.dt
    if burn < 0.5 then return end
    burn = 0.0
    for _, id in core.players() do
        local x, y, z = core.player_pos(id)
        if x then
            local fx, fy, fz = math.floor(x), math.floor(y + 0.1), math.floor(z)
            if lava_at(fx, fy, fz) > 0 or lava_at(fx, fy + 1, fz) > 0 then
                core.damage(id, 3)
            end
        end
    end
end)

-- The way in follows the frame that leads here. When a player lights a frame in
-- the world whose second opening comes out of this one's first
-- (mods/infernoportal), this world hears it (core:far_moved) and builds a frame
-- of the same size, turned the same way, at the matching place -- a node here
-- for every SCALE there, as the Inferno always was -- at the height where it
-- breaks the least: rock and lava dug out of the hole and of a cell in front and
-- behind, and rock where the obsidian goes, each one; lava three, so it stays out
-- of the seas; bedrock never. A floor of obsidian under the cells in front and
-- behind where there is none. Old frames stay where they were.
local SCALE = 8

type Gate = { x: number, y: number, z: number, w: number, h: number, axis: number, sign: number }

-- Every cell of a frame at (x, y, z) -- the hole's lowest cell on its low side
-- -- and what it has to be: "frame", "air" or "floor".
local function each_cell(g: Gate, fn: (number, number, number, string) -> ())
    local ax, az = if g.axis == 0 then 1 else 0, if g.axis == 0 then 0 else 1
    local nx, nz = az, ax
    for i = -1, g.w do
        for up = -1, g.h do
            local inside_i, inside_up = i >= 0 and i < g.w, up >= 0 and up < g.h
            local x, y, z = g.x + ax * i, g.y + up, g.z + az * i
            fn(x, y, z, if inside_i and inside_up then "air" else "frame")
            if inside_i and (inside_up or up == -1) then
                for _, d in { -1, 1 } do
                    fn(x + nx * d, y, z + nz * d, if inside_up then "air" else "floor")
                end
            end
        end
    end
end

local function damage(g: Gate): number
    local cost = 0
    each_cell(g, function(x, y, z, want)
        local id = core.get_block(x, y, z)
        if id == BEDROCK then cost = math.huge
        elseif LAVA_LEVEL[id] then cost += 3
        elseif want == "floor" then
            if id == 0 then cost += 0.25 end
        elseif id ~= 0 and id ~= OBSIDIAN then cost += 1 end
    end)
    return cost
end

local function build(g: Gate)
    each_cell(g, function(x, y, z, want)
        local id = core.get_block(x, y, z)
        if want == "frame" then core.set_block(x, y, z, OBSIDIAN)
        elseif want == "air" then core.set_block(x, y, z, 0)
        elseif core.node_solid(id) == 0 then core.set_block(x, y, z, OBSIDIAN) end
    end)
end

local function open_gate(g: Gate)
    local ax = if g.axis == 0 then 1 else 0
    core.portal_place(0, g.x + (if ax == 1 then g.w / 2 else 0.5), g.y + g.h / 2,
                      g.z + (if ax == 1 then 0.5 else g.w / 2), g.axis, g.sign, g.w / 2, g.h / 2)
end

local wanted: Gate? = nil   -- heard, waiting for its ground to be made
local heard = false

core.event.on("core:far_moved", function(ev)
    if ev.index ~= 0 or ev.axis > 1 then return end
    local w, h = math.max(1, math.floor(ev.half_w * 2 + 0.5)), math.max(2, math.floor(ev.half_h * 2 + 0.5))
    local ax = if ev.axis == 0 then 1 else 0
    local g: Gate = {
        x = math.floor(ev.x / SCALE - ax * w / 2 + 0.5 * ax), y = FLOOR,
        z = math.floor(ev.z / SCALE - (1 - ax) * w / 2 + 0.5 * (1 - ax)),
        w = w, h = h, axis = ev.axis, sign = ev.sign,
    }
    wanted, heard = g, true
    -- There for now, so the engine makes the ground round it (an opening's
    -- columns are always made); the frame and the real height follow.
    open_gate(g)
end)

local function ground_made(g: Gate): boolean
    for _, c in { { -2, -2 }, { g.w + 1, -2 }, { -2, g.w + 1 }, { g.w + 1, g.w + 1 } } do
        if core.loaded(g.x + c[1], g.y, g.z + c[2]) == 0 then return false end
    end
    return true
end

local function saved_gate(): Gate?
    local v = string.split(core.storage_get("gate") or "", " ")
    if #v ~= 7 then return nil end
    return { x = tonumber(v[1]) or 0, y = tonumber(v[2]) or 0, z = tonumber(v[3]) or 0,
             w = tonumber(v[4]) or 1, h = tonumber(v[5]) or 2,
             axis = tonumber(v[6]) or 0, sign = tonumber(v[7]) or 1 }
end

local restored = false
core.event.on("core:tick", function(_)
    if not restored then
        restored = true
        local old = saved_gate()
        if old and not heard then open_gate(old) end
    end

    local g = wanted
    if g == nil or not ground_made(g) then return end
    wanted = nil
    local best, best_cost = FLOOR, math.huge
    -- The frame already built for this one, told again after a restart.
    local old = saved_gate()
    if old and old.x == g.x and old.z == g.z and old.w == g.w and old.h == g.h and old.axis == g.axis then
        best, best_cost = old.y, 0
    else
        for y = LAVA_TOP + 1, world_h - g.h - 3 do
            g.y = y
            local c = damage(g)
            if c < best_cost or (c == best_cost and math.abs(y - FLOOR) < math.abs(best - FLOOR)) then
                best, best_cost = y, c
            end
        end
    end
    g.y = best
    build(g)
    open_gate(g)
    core.storage_set("gate", `{g.x} {g.y} {g.z} {g.w} {g.h} {g.axis} {g.sign}`)
    core.log(`inferno: the way in is a {g.w}x{g.h} frame at {g.x} {g.y} {g.z}, {best_cost} broken`)
end)

-- What each item looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
for _, n in { "cinder_rod", "wraith_tear", "gold_nugget" } do
    core.register_model(`inferno:{n}_held`, `{n}_held.gltf`)
end
