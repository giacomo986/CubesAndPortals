--!strict
-- The sky islands' layout, for the mapgen and the game alike (init.luau): six
-- islands over a sea of cloud and the bridges between them, every cell of a
-- bridge listed so the game can bring them down and put them back.
--
--          north (rod)
--              |
--   west ---- centre ---- east       one bridge each way from the centre;
--   (rod)      |          (rod)      the north and west ones have a gap of
--          south (rod)               two to jump
--              |
--            home (the doorway, the glider rack)

export type Island = { name: string, x: number, y: number, z: number, r: number, rod: boolean }
export type Cell = { x: number, y: number, z: number, b: string }

local L = {}

L.ISLANDS = {
    { name = "home",   x = 0,   y = 40, z = -92, r = 8,  rod = false },
    { name = "south",  x = 0,   y = 40, z = -46, r = 7,  rod = true },
    { name = "centre", x = 0,   y = 40, z = 0,   r = 13, rod = false },
    { name = "north",  x = 0,   y = 42, z = 46,  r = 7,  rod = true },
    { name = "east",   x = 46,  y = 38, z = 0,   r = 7,  rod = true },
    { name = "west",   x = -46, y = 44, z = 0,   r = 7,  rod = true },
} :: { Island }

L.CLOUD_Y = 14     -- the sea of cloud (a world is 64 high: all of it under that)
L.CATCH_Y = 20     -- below this a body is taken back to where it last stood

-- How far an island's edge is from its middle, that way: a little uneven.
function L.edge(i: number, s: Island, dx: number, dz: number): number
    return s.r + 1.2 * math.sin(math.atan2(dz, dx) * 3 + i)
end

-- The island a column is on, if any.
function L.island_at(x: number, z: number): (number, Island?)
    for i, s in L.ISLANDS do
        local dx, dz = x - s.x, z - s.z
        if dx * dx + dz * dz <= L.edge(i, s, dx, dz) ^ 2 then return i, s end
    end
    return 0, nil
end

-- The bridges: island a to island b (both along an axis), and the gap.
local BRIDGES = { { 1, 2, 0 }, { 2, 3, 0 }, { 3, 4, 2 }, { 3, 5, 0 }, { 3, 6, 2 } }

-- Every cell of every bridge: planks three wide, a log post either side every
-- four. `deck` the planks alone, which are what falls.
L.CELLS = {} :: { Cell }
L.DECK = {} :: { Cell }
for _, br in BRIDGES do
    local a, b = L.ISLANDS[br[1]], L.ISLANDS[br[2]]
    local along_x = a.z == b.z
    local from = if along_x then a.x else a.z
    local to = if along_x then b.x else b.z
    local step = if to > from then 1 else -1
    local len = math.abs(to - from)
    local t0, t1 = a.r - 2, len - b.r + 2
    local mid = (t0 + t1) // 2
    for t = t0, t1 do
        local f = t / len
        local y = math.floor(a.y + (b.y - a.y) * f + 0.5)
        local gap = br[3] > 0 and t >= mid and t < mid + br[3]
        for w = -1, 1 do
            local x = if along_x then from + t * step else a.x + w
            local z = if along_x then a.z + w else from + t * step
            if not gap then
                local c = { x = x, y = y, z = z, b = "timber:planks" }
                table.insert(L.CELLS, c)
                table.insert(L.DECK, c)
            end
        end
        if t % 4 == 0 and not gap then
            for _, w in { -2, 2 } do
                local x = if along_x then from + t * step else a.x + w
                local z = if along_x then a.z + w else from + t * step
                table.insert(L.CELLS, { x = x, y = y, z = z, b = "timber:log" })
                table.insert(L.CELLS, { x = x, y = y + 1, z = z, b = "timber:log" })
            end
        end
    end
end

return L
