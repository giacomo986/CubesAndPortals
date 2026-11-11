--!strict
-- What a cell's facing byte becomes when the zone it is in is turned about the
-- vertical or mirrored, for each way a block may turn (docs/MODDING.md, "Blocks
-- that turn").
--
-- The two tables are worked out from the engine's own matrices
-- (src/core/media/face_tiles.h, rot_for): Y[o + 1] is the orientation whose
-- matrix is one quarter turn about +Y after o's -- the turn that takes +X to
-- -Z, the one a zone's turn counts -- and MIRROR[o + 1] the one whose matrix is
-- o's seen in a mirror across x (M * R * M). A mirrored block is right only
-- when its own shape is the same either side of its middle, which a stair, a
-- log, a chest and a door's frame are.
local Y = { 1, 2, 3, 0, 13, 14, 15, 12, 17, 18, 19, 16, 9, 10, 11, 8, 5, 6, 7, 4, 23, 20, 21, 22 }
local MIRROR = { 0, 3, 2, 1, 4, 7, 6, 5, 8, 11, 10, 9, 16, 19, 18, 17, 12, 15, 14, 13, 20, 23, 22, 21 }

-- Wall-mounted is a face: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z.
local WALL_Y = { 5, 4, 2, 3, 0, 1 }
local WALL_MIRROR = { 1, 0, 2, 3, 4, 5 }

local T = {}

-- The facing of a block that turns `turns` ways (core.node_turns), facing f in
-- its zone, once the zone is mirrored (first) and turned t quarters.
function T.turned(turns: number, f: number, t: number, mirror: boolean): number
    if turns == 4 or turns == 24 then
        if f < 0 or f >= turns then
            return f
        end
        if mirror then
            f = MIRROR[f + 1]
        end
        for _ = 1, t % 4 do
            f = Y[f + 1]
        end
        return f
    elseif turns == 6 then
        if f < 0 or f > 5 then
            return f
        end
        if mirror then
            f = WALL_MIRROR[f + 1]
        end
        for _ = 1, t % 4 do
            f = WALL_Y[f + 1]
        end
        return f
    end
    return f   -- it does not turn, or turns a way this does not know (a height)
end

return T
