--!strict
-- Somewhere to go when you dig, and somewhere to go from there.
--
-- Everything under the grass was solid: ore gave a reason to dig and the
-- digging itself was still a straight shaft through uniform rock, with nothing
-- to find, nothing to follow and nowhere a torch was worth carrying. This mod
-- cuts tunnels through the stone from a mapgen stage, the same
-- core.register_mapgen mods/ore uses.
--
-- And the tunnels are JOINED, on purpose (TODO.md OPEN-35). Every square of 32
-- by 32 columns has a point deep under it, and a tunnel from it wanders to the
-- point of the square beside it along x -- always -- and to the one along z, one
-- square in two. So the caves under a stretch of the world are one network: a
-- player who finds one and follows it can walk under the whole stretch, which is
-- what makes a cave a place rather than a dent.
--
-- It runs BEFORE mods/ore, because the mod directory loads in name order and
-- `cave` sorts before `ore`. That is not an accident to be tidied up later: ore
-- only fills cells that are still stone, so carving first means a seam can end
-- up in a cave WALL, which is the one place a player sees ore without having
-- guessed where it was.
--
-- What it does not do: reach the surface, touch the floor of the world, or
-- carve anything that is not stone. The last of those is the one rule that
-- keeps this clear of the biome filler, the bedrock and whatever an earlier
-- stage built.

-- A 32-bit multiply that stays exact in a double. The product of two 32-bit
-- numbers is 64 bits and a double holds 53, so `a * b` would round away the low
-- bits -- which are the only ones a hash has. Same function as mods/ore, and
-- deliberately copied rather than shared: a mod is a directory, and a mod that
-- reached into another mod's file would be a dependency nothing declares.
local function mul32(a: number, b: number): number
    local hi = math.floor(a / 65536)
    local lo = a % 65536
    return ((hi * b) % 65536 * 65536 + lo * b) % 4294967296
end

-- A hash of two numbers and the seed, and of nothing else: no clock, no
-- math.random, nothing read from a global another stage could have moved. The
-- server and a client that generates its own world have to agree cell for cell.
local function hash(x: number, z: number, salt: number): number
    local h = mul32(x + 0x9E3779B9, 374761393)
    h = bit32.bxor(h, mul32(z + 0x85EBCA6B, 668265263))
    h = bit32.bxor(h, mul32(salt % 4294967296, 2246822519))
    h = bit32.bxor(h, bit32.rshift(h, 13))
    return mul32(h, 1274126177)
end

-- The squares, and the point of each one is chosen inside it.
local REGION = 32

-- How far a tunnel may stray sideways from the box between the two points it
-- joins. Enough to wander, and bounded, because the bound is what tells a column
-- which squares' tunnels can reach it at all.
local STRAY = 6

-- So a tunnel between two squares stays within the squares one either side of
-- them, and a column asks the squares within this many of its own.
local REACH = 2

-- The roof stays this far under the ground. A cave that broke the surface would
-- be a hole in the world seen from above, and it is also the thing that makes
-- the finer walk of TODO.md CLOSED-23 worth having: a cave that reaches the sky
-- is the same piece of air as the sky.
local ROOF = 6

-- The cross-section: a plus, two blocks tall. Five cells and not nine, because
-- the corners of a 3x3 are what turn a tunnel into a room -- and two tall
-- because a player is 1.8 and a passage they cannot stand up in is a passage
-- they walk through backwards looking at the ceiling.
local CROSS = { { 0, 0 }, { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }

type Point = { x: number, y: number, z: number }

-- The point deep under a square, or nil for a square whose ground is too low
-- to hold a tunnel under a roof.
local function point_of(rx: number, rz: number, seed: number): Point?
    local h = hash(rx, rz, seed + 0x5EED)
    local x = rx * REGION + bit32.band(h, 0x1F)
    local z = rz * REGION + bit32.band(bit32.rshift(h, 5), 0x1F)

    -- Deep, and deeper under some squares than others. Measured DOWN from the
    -- ground this column actually has, not from a height this mod guessed.
    local y = 3 + bit32.band(bit32.rshift(h, 10), 0x7)
    local top = core.terrain_height(x, z)
    if y + 1 + ROOF > top then
        y = top - ROOF - 1
    end
    if y < 2 then
        return nil
    end
    return { x = x, y = y, z = z }
end

type Path = { cells: { number }, min_x: number, max_x: number, min_z: number, max_z: number }

-- The tunnel from `a` to `b`: most steps towards `b` along whichever way is
-- furthest, some sideways, some up or down -- never out of the box between the
-- two grown by STRAY, and never more steps than it takes to get there three
-- times over. A cell is kept only where the roof is ROOF under the ground and
-- the floor is off the bottom of the world; the walk goes on under a valley too
-- low to hold it and comes out the other side.
local function walk(a: Point, b: Point, salt: number): Path
    local lo_x, hi_x = math.min(a.x, b.x) - STRAY, math.max(a.x, b.x) + STRAY
    local lo_z, hi_z = math.min(a.z, b.z) - STRAY, math.max(a.z, b.z) + STRAY

    local p: Path = { cells = {}, min_x = a.x, max_x = a.x, min_z = a.z, max_z = a.z }
    local cx, cy, cz = a.x, a.y, a.z
    local steps = 3 * (math.abs(b.x - a.x) + math.abs(b.y - a.y) + math.abs(b.z - a.z)) + 16

    for i = 0, steps - 1 do
        local ground = core.terrain_height(cx, cz)
        if cy + 1 + ROOF > ground then
            cy = ground - ROOF - 1
        end
        if cy < 2 then
            cy = 2
        end
        if cy + 1 + ROOF <= ground then
            table.insert(p.cells, cx)
            table.insert(p.cells, cy)
            table.insert(p.cells, cz)
            p.min_x = math.min(p.min_x, cx)
            p.max_x = math.max(p.max_x, cx)
            p.min_z = math.min(p.min_z, cz)
            p.max_z = math.max(p.max_z, cz)
        end

        local dx, dy, dz = b.x - cx, b.y - cy, b.z - cz
        if dx == 0 and dz == 0 and dy == 0 then
            break
        end

        local g = hash(cx * 65521 + cz, i, salt)
        local pick = bit32.band(g, 0x7)
        local side = if bit32.band(bit32.rshift(g, 3), 1) == 1 then 1 else -1

        local nx, ny, nz = cx, cy, cz
        if pick <= 4 or (dx == 0 and dz == 0) then
            -- Towards b, along the longer way; up or down once it is above b.
            if math.abs(dx) >= math.abs(dz) and dx ~= 0 then
                nx += if dx > 0 then 1 else -1
            elseif dz ~= 0 then
                nz += if dz > 0 then 1 else -1
            else
                ny += if dy > 0 then 1 else -1
            end
        elseif pick == 5 then
            -- Sideways: across the way it is going.
            if math.abs(dx) >= math.abs(dz) then
                nz += side
            else
                nx += side
            end
        elseif pick == 6 then
            ny += if dy ~= 0 then (if dy > 0 then 1 else -1) else side
        else
            -- Along the shorter way, which is what bends it.
            if math.abs(dx) < math.abs(dz) and dx ~= 0 then
                nx += if dx > 0 then 1 else -1
            elseif dz ~= 0 then
                nz += if dz > 0 then 1 else -1
            else
                nx += side
            end
        end

        if nx >= lo_x and nx <= hi_x and nz >= lo_z and nz <= hi_z then
            cx, cy, cz = nx, ny, nz
        else
            cy = ny
        end
    end

    return p
end

-- A square's tunnels, worked out once. The world is made a column at a time, and
-- every column within REACH squares of a tunnel walks it again -- the path is a
-- function of the seed and the squares and not of the world, so walking it once
-- and keeping it is the same thing, many times cheaper.
local paths: { [string]: { Path } } = {}
local kept = 0

local function tunnels_of(rx: number, rz: number, seed: number): { Path }
    local key = `{rx},{rz},{seed}`
    local known = paths[key]
    if known then
        return known
    end

    local out: { Path } = {}
    local here = point_of(rx, rz, seed)
    if here then
        local east = point_of(rx + 1, rz, seed)
        if east then
            table.insert(out, walk(here, east, hash(rx, rz, seed + 0xCA7E)))
        end
        if bit32.band(hash(rx, rz, seed + 0x2105), 1) == 1 then
            local south = point_of(rx, rz + 1, seed)
            if south then
                table.insert(out, walk(here, south, hash(rz, rx, seed + 0xCA7F)))
            end
        end
    end

    -- Forgotten wholesale past a bound: a player who walks for an hour would
    -- otherwise carry every square they ever passed.
    if kept >= 4096 then
        paths = {}
        kept = 0
    end
    paths[key] = out
    kept += 1
    return out
end

core.register_mapgen("cave:tunnels", function(ctx)
    -- Asked for by name: the stone this hollows out is the engine's, and a mod
    -- that registered its own would be carving a block nothing else has.
    local stone = core.node_id("core:stone")
    if stone == 0 then
        return
    end

    local col_rx = math.floor(ctx.min_x / REGION)
    local col_rz = math.floor(ctx.min_z / REGION)

    for rz = col_rz - REACH, col_rz + REACH do
        for rx = col_rx - REACH, col_rx + REACH do
            for _, p in tunnels_of(rx, rz, ctx.seed) do
                -- Only a tunnel that comes within a cell of this column has
                -- anything to cut in it.
                if #p.cells > 0 and p.max_x + 1 >= ctx.min_x and p.min_x - 1 <= ctx.max_x
                    and p.max_z + 1 >= ctx.min_z and p.min_z - 1 <= ctx.max_z then
                    for i = 1, #p.cells, 3 do
                        local cx, cy, cz = p.cells[i], p.cells[i + 1], p.cells[i + 2]
                        for dy = 0, 1 do
                            for k = 1, #CROSS do
                                local x = cx + CROSS[k][1]
                                local y = cy + dy
                                local z = cz + CROSS[k][2]

                                -- Never y 0: that is the floor of the world, and
                                -- a floor with a hole in it is a player falling
                                -- for ever (TODO.md OPEN-16).
                                if x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z
                                    and z <= ctx.max_z and y >= 1 and y < ctx.height
                                    and core.get_block(x, y, z) == stone then
                                    core.set_block(x, y, z, 0)   -- core:air, id 0 in every registry
                                end
                            end
                        end
                    end
                end
            end
        end
    end
end)
