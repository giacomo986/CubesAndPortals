--!strict
-- villagers: land, the mayor and the builder: lots, houses, what is kept. Part of the mod (init.luau says the order).

local common = require("./common")
local types = require("./types")
type Order = types.Order
type House = types.House
local AID = common.AID
local AIR = common.AIR
local id = common.id
local KEEPING = common.KEEPING
local lots = common.lots

-- ------------------------------------------- land, the mayor and the builder

-- A village sells land through its MAYOR (right click): a lot of 7 by 7 for
-- 8 rubies, levelled and with a way to it, for the buyer to build on; or a
-- house on one for 20. The lot is the buyer's (mods/territory: `territory:lot`)
-- and the village's ground grows to take it in. Its BUILDER levels it, puts
-- the house up block by block and lays a way of cobblestone, with steps, from
-- the door to the nearest street. What it builds with comes from the
-- village's woodcutters (logs), miners (cobblestone)
-- and diggers (earth); a village without one of them, or where nothing has
-- come for two minutes, asks the buyer, and anybody may hand the builder what
-- it lacks (right click) -- or tell it to find it itself: then the gatherers
-- fetch any kind the order lacks once their own is in, and the builder goes
-- too, for a kind nobody in the village gathers first. Six orders a village at a time, kept in
-- storage.

local LOT = 8
local FLOOR_PRICE, GARDEN_PRICE = 12, 6   -- a second storey, a garden, on a house bought
local GUARD_DAY = 6   -- a guard of the village hired for a player's territory (mods/territory)
local W = 7
local ROAD_BLOCKS = { "gravel:gravel", "rocks:cobblestone", "rocks:smooth_stone", "rocks:coarse_dirt",
                      "steps:gravel_stair", "steps:cobblestone_stair", "steps:smooth_stone_stair",
                      "steps:coarse_dirt_stair" }
local LOG_NAMES = { "trees:trunk", "timber:log", "timber:mossy_log", "woods:birch_log", "woods:spruce_log",
                    "woods:jungle_log", "woods:acacia_log", "woods:dark_oak_log" }
local LEAF_NAMES = { "trees:leaves", "woods:birch_leaves", "woods:spruce_leaves", "woods:jungle_leaves",
                     "woods:acacia_leaves", "woods:dark_oak_leaves" }
-- What the ground is where nobody built: a lot goes only on it.
local NATURAL = { "core:grass", "core:dirt", "core:sand", "core:snow", "core:stone", "rocks:podzol",
                  "rocks:red_sand", "rocks:clay", "rocks:andesite", "rocks:diorite", "rocks:granite" }

-- What the builder asks for, three kinds, and the things that are each.
local MATS = { "wood", "stone", "earth" }
local SAID = { wood = "logs", stone = "cobblestone", earth = "earth" }
local BRINGS = { wood = "woodcutter", stone = "miner", earth = "digger" }
local ITEMS: { [string]: { string } } = { wood = table.clone(LOG_NAMES),
                                          -- And the deep's: a mine's galleries are dug at y 5-6,
                                          -- in deepslate and tuff, which brought nothing.
                                          stone = { "rocks:cobblestone", "rocks:mossy_cobblestone", "core:stone",
                                                    "rocks:andesite", "rocks:diorite", "rocks:granite",
                                                    "rocks:deepslate", "rocks:tuff", "rocks:calcite",
                                                    "rocks:dripstone_block" },
                                          earth = { "core:dirt", "core:grass", "rocks:coarse_dirt" } }
local function mat_of(item: string): string?
    for m, list in ITEMS do
        if table.find(list, item) then
            return m
        end
    end
    return nil
end
-- What each block costs: planks and their kin a quarter of a log.
local COST = { ["timber:planks"] = { "wood", 0.25 }, ["timber:log"] = { "wood", 1 },
               ["steps:spruce_stair"] = { "wood", 0.25 }, ["woods:spruce_planks"] = { "wood", 0.25 },
               ["doors:door"] = { "wood", 0.5 }, ["rocks:cobblestone"] = { "stone", 1 },
               ["steps:cobblestone_stair"] = { "stone", 1 }, ["core:dirt"] = { "earth", 1 },
               ["rocks:stone_bricks"] = { "stone", 1 }, ["steps:stair"] = { "stone", 1 },
               ["craft:brick"] = { "stone", 1 }, ["steps:dark_oak_stair"] = { "wood", 0.25 },
               ["woods:dark_oak_planks"] = { "wood", 0.25 }, ["building:oak_fence_gate"] = { "wood", 0.5 },
               ["farm:soil"] = { "earth", 1 }, ["timber:rungs"] = { "wood", 0.25 },
               ["rocks:coarse_dirt"] = { "earth", 1 } }
-- And a fence, whatever its shape.
local function cost_of(name: string): { any }?
    return COST[name] or (if name:sub(1, 18) == "building:oak_fence" then { "wood", 0.25 } else nil)
end

local function set_of(names: { string }): { [number]: boolean }
    local out = {}
    for _, n in names do
        if id(n) ~= 0 then
            out[id(n)] = true
        end
    end
    return out
end
local SETS: { [string]: { [number]: boolean } } = {}
local function is_in(kind: string, names: { string }, block: number): boolean
    SETS[kind] = SETS[kind] or set_of(names)
    return SETS[kind][block] == true
end

-- A log that is a tree's: up its trunk to a crown of leaves. A house's or
-- the hall's corner is logs too, with a roof on it, and is somebody's wall:
-- taken for a tree, the builder cut the hall's corner for a lot and a
-- woodcutter felled it.
--
-- A mine's headframe is logs too, behind the top rungs of its shaft, and a
-- woodcutter felled one of them for a sapling (2026-10-10). So a tree is:
-- not timber:log (the world grows trunks and woods' logs; timber's are only
-- ever built -- a corner, a headframe, a bridge's pier), standing on the
-- world's own ground, nothing built against its trunk (a rung, a door, a
-- wall), no roof or floor within four over it, and a crown within 3.
local function tree_log(x: number, y: number, z: number): boolean
    local here = core.get_block(x, y, z)
    if not is_in("logs", LOG_NAMES, here) or here == id("timber:log") or here == id("timber:mossy_log") then
        return false
    end
    local foot, top = y, y
    while foot > y - 24 and is_in("logs", LOG_NAMES, core.get_block(x, foot - 1, z)) do
        foot -= 1
    end
    while top < y + 24 and is_in("logs", LOG_NAMES, core.get_block(x, top + 1, z)) do
        top += 1
    end
    if not is_in("natural", NATURAL, core.get_block(x, foot - 1, z)) then
        return false
    end
    for ty = foot, top do
        for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
            local b = core.get_block(x + d[1], ty, z + d[2])
            local name = core.node_name(b)
            if name == "timber:rungs" or name:sub(1, 6) == "doors:" or (b ~= AIR and core.node_solid(b) == 1
                and not (is_in("logs", LOG_NAMES, b) or is_in("leaves", LEAF_NAMES, b) or is_in("natural", NATURAL, b))) then
                return false
            end
        end
    end
    for dy = 1, 4 do
        local over = core.get_block(x, top + dy, z)
        if core.node_solid(over) == 1 and not is_in("leaves", LEAF_NAMES, over) then
            return false
        end
    end
    for dy = -1, 3 do
        for dz = -3, 3 do
            for dx = -3, 3 do
                if is_in("leaves", LEAF_NAMES, core.get_block(x + dx, top + dy, z + dz)) then
                    return true
                end
            end
        end
    end
    return false
end

-- The ground's top at a column, from y down: past air, plants, trunks and
-- leaves. Its height and block.
local function ground_at(x: number, y: number, z: number): (number?, number)
    for gy = y + 12, y - 16, -1 do
        local b = core.get_block(x, gy, z)
        if core.node_solid(b) == 1 and not is_in("logs", LOG_NAMES, b) and not is_in("leaves", LEAF_NAMES, b) then
            return gy, b
        elseif core.node_name(b):sub(1, 6) == "lakes:" then
            return nil, b
        end
    end
    return nil, AIR
end

local function turn_of(dx: number, dz: number): number
    if dz < 0 then return 0 elseif dx < 0 then return 1 elseif dz > 0 then return 2 else return 3 end
end

-- The face a wallmounted block (timber:rungs) looks out of for each upright
-- turn: the front, +Z unturned, goes to +Z, +X, -Z, -X.
local WALL_FACE = { [0] = 4, 0, 5, 1 }

local function at(o: Order, lx: number, lz: number): (number, number)
    if o.t == 0 then
        return o.x0 + lx, o.z0 + lz
    elseif o.t == 1 then
        return o.x0 + lz, o.z0 + o.w - 1 - lx
    elseif o.t == 2 then
        return o.x0 + o.w - 1 - lx, o.z0 + o.d - 1 - lz
    end
    return o.x0 + o.d - 1 - lz, o.z0 + lx
end
-- Its footprint in the world, corners.
local function rect_of(o: Order): (number, number, number, number)
    local fw, fd = if o.t % 2 == 0 then o.w else o.d, if o.t % 2 == 0 then o.d else o.w
    return o.x0, o.z0, o.x0 + fw - 1, o.z0 + fd - 1
end

-- What a house is made of, by its style.
local STYLES = {
    wood = { wall = "timber:planks", corner = "timber:log", roof = "steps:spruce_stair",
             ridge = "woods:spruce_planks" },
    stone = { wall = "rocks:stone_bricks", corner = "rocks:cobblestone", roof = "steps:stair",
              ridge = "rocks:stone_bricks" },
    brick = { wall = "craft:brick", corner = "timber:log", roof = "steps:dark_oak_stair",
              ridge = "woods:dark_oak_planks" },
}
local SIZES = { small = { 5, 5, 14 }, medium = { 7, 7, 20 }, large = { 9, 7, 30 } }   -- w, d, rubies
-- A pocket hut (mods/pocket): a small house of wood, three by three inside,
-- whose doorway is a pocket door onto a hall of the buyer's own. Sold only
-- where there are pocket halls to open on.
STYLES.pocket = STYLES.wood
AID.pocket_price = 30   -- rubies; a field and not a local: a main chunk has 200
local function pockets_here(): boolean
    if core.node_id("pocket:door") == 0 then return false end
    for k = 0, 27 do
        if core.portal_room(k) == "pockets" then return true end
    end
    return false
end

local function fence_mask(ring: { [string]: boolean }, x: number, z: number): string
    local mask = 0
    for bitv, o in { [1] = { 1, 0 }, [2] = { -1, 0 }, [4] = { 0, 1 }, [8] = { 0, -1 } } do
        if ring[`{x + o[1]},{z + o[2]}`] then
            mask += bitv
        end
    end
    return if mask == 0 then "building:oak_fence" else `building:oak_fence_{mask}`
end

-- Every block of an order, in the order it is put: {x, y, z, name, facing?,
-- on the way?}. "" is air: dug out. Worked out again from the order whenever
-- needed, from the ground as it was when sold (o.g, o.road), so building is
-- the same after a restart and a block already right is skipped.
local function plan_of(o: Order): { { any } }
    if o.kind == "blueprint" then
        return AID.bp.plan(o)
    end
    local out = {}
    local road = false
    local function put(x: number, y: number, z: number, name: string, facing: number?)
        table.insert(out, { x, y, z, name, facing, road })
    end
    local function hput(lx: number, y: number, lz: number, name: string, facing: number?)
        local x, z = at(o, lx, lz)
        local t = if facing then (facing + o.t) % 4 else nil
        -- A wallmounted block's byte is the face it looks out of, not a turn.
        if t ~= nil and core.node_turns(id(name)) == 6 then
            t = WALL_FACE[t]
        end
        put(x, y, z, name, t)
    end
    local F, w, d = o.floor, o.w, o.d
    local keep = KEEPING.keep[o.kind]
    local building = o.kind == "house" or o.kind == "floor" or o.kind == "hall" or o.kind == "barracks"
        or keep ~= nil
    local storeys = if o.kind == "floor" then 2 else (o.storeys or 1)
    -- Fifteen beds for every keeper: a hall ordered before keepers had
    -- buildings of their own (three storeys); since, the mayor's alone.
    local beds = o.kind == "barracks" or (o.kind == "hall" and storeys >= 3)
    local eave = F + 4 * storeys - 1
    local st = STYLES[o.style] or STYLES.wood

    if o.kind == "floor" then
        -- On a house standing: its roof and what is over its top storey away,
        -- the eaves past its walls too; nothing below is touched.
        for lz = -1, d do
            for lx = 0, w - 1 do
                for y = eave + d // 2 + 2, F + 3, -1 do
                    hput(lx, y, lz, "")
                end
            end
        end
    else
        local top = if building then eave + d // 2 + 2 else F + (if o.kind == "garden" then 3 else 5)
        for lz = 0, d - 1 do
            for lx = 0, w - 1 do
                local x, z = at(o, lx, lz)
                local g = o.g[lz * w + lx + 1] or (F - 1)
                for y = top, F, -1 do
                    put(x, y, z, "")
                end
                for y = g + 1, F - 2 do
                    put(x, y, z, "core:dirt")
                end
                local edge = lx == 0 or lz == 0 or lx == w - 1 or lz == d - 1
                local under = "core:dirt"
                if building then
                    under = if edge then "rocks:cobblestone" else "timber:planks"
                elseif o.kind == "garden" and not edge then
                    under = if lx == w // 2 and lz == d // 2 then "lakes:water" else "farm:soil"
                end
                put(x, F - 1, z, under)
            end
        end
    end

    if building then
        local pane_front = if o.t % 2 == 0 then "building:glass_pane_3" else "building:glass_pane_12"
        local pane_side = if o.t % 2 == 0 then "building:glass_pane_12" else "building:glass_pane_3"
        local door = w // 2
        local split = if w >= 9 and o.kind == "house" then w // 2 else nil   -- a large house's two rooms
        for y = F, eave do
            local row = (y - F) % 4
            for lz = 0, d - 1 do
                for lx = 0, w - 1 do
                    local front, side = lz == 0 or lz == d - 1, lx == 0 or lx == w - 1
                    if (front or side) and not (lz == 0 and lx == door and y <= F + 1) then
                        local name = if front and side then st.corner else st.wall
                        if not (front and side) and row == 1 then
                            if front and (lx == 1 or lx == w - 2) then
                                name = pane_front
                            elseif side and lz == d // 2 then
                                name = pane_side
                            end
                        end
                        hput(lx, y, lz, name)
                    elseif split ~= nil and lx == split and not front and y < F + 3
                        and not (lz == 1 and y <= F + 1) then
                        -- The wall between the rooms, the way through right
                        -- behind the front door: it is on the door's column,
                        -- and with the way in its middle the door opened onto it.
                        hput(lx, y, lz, st.wall)
                    end
                end
            end
        end
        -- The floors between storeys, a ladder up through them at the back.
        for s2 = 1, storeys - 1 do
            local y = F + 4 * s2 - 1
            for lz = 1, d - 2 do
                for lx = 1, w - 2 do
                    if not (lx == w - 2 and lz == d - 2) then
                        hput(lx, y, lz, "timber:planks")
                    end
                end
            end
        end
        if storeys > 1 then
            for y = F, F + 4 * (storeys - 1) do
                hput(w - 2, y, d - 2, "timber:rungs", 2)
            end
        end
        -- The roof: stairs from the eaves up to a ridge, gables of its wall.
        local k = 0
        while -1 + k <= d - k do
            local f, bk, y = -1 + k, d - k, eave + k
            for lx = 0, w - 1 do
                if f == bk then
                    hput(lx, y, f, st.ridge)
                else
                    hput(lx, y, f, st.roof, 2)
                    hput(lx, y, bk, st.roof, 0)
                end
            end
            for lz = math.max(f + 1, 0), math.min(bk - 1, d - 1) do
                if y >= eave + 1 then
                    hput(0, y, lz, st.wall)
                    hput(w - 1, y, lz, st.wall)
                end
            end
            k += 1
        end
        if o.style ~= "pocket" then   -- a pocket hut's doorway is its pocket door, put when built
            hput(door, F, 0, "doors:door", 0)
            hput(door, F + 1, 0, "doors:door_top", 0)
        end
        if o.kind ~= "floor" then
            -- A doorstep: the cells before the door at the floor's height,
            -- clear over it, whether a way to a street was found or not --
            -- the lot's edge may stand up to three higher than its floor.
            for lx = door - 1, door + 1 do
                for y = F + 2, F, -1 do
                    hput(lx, y, -1, "")
                end
                hput(lx, F - 1, -1, "rocks:cobblestone")
            end
        end
        if beds then
            -- An old hall, or the watch's barracks (as mods/villages builds
            -- them): three beds on the ground and six on each floor, no
            -- table, and the bell or the banner that says whose they are.
            for k = 0, storeys - 1 do
                local y = F + 4 * k
                for _, lx in { 1, 3, 5 } do
                    hput(lx, y, d - 3, "bed:bed", 2)
                    hput(lx, y, d - 2, "bed:bed_head", 2)
                    if k > 0 then
                        hput(lx, y, 2, "bed:bed", 0)
                        hput(lx, y, 1, "bed:bed_head", 0)
                    end
                end
                hput(w // 2, y, d - 2, "torches:torch")
            end
            hput(1, F, 1, if o.kind == "hall" then "villages:bell" else "villages:banner")
            hput(w - 2, F, 1, "chest:box", 3)
        elseif o.kind == "hall" then
            -- The mayor's: its bell, a table and the chest below, its bed
            -- and table up the ladder.
            hput(1, F, 1, "villages:bell")
            hput(w // 2, F, d - 2, "craft:table")
            hput(w // 2, F, d // 2, "torches:torch")
            hput(w - 2, F, 1, "chest:box", 3)
            hput(1, F + 4, d - 3, "bed:bed", 2)
            hput(1, F + 4, d - 2, "bed:bed_head", 2)
            hput(3, F + 4, d - 2, "craft:table")
            hput(w // 2, F + 4, 2, "torches:torch")
        else
            hput(w - 2, F, if storeys > 1 then d - 3 else d - 2, "torches:torch")
            hput(1, F, d - 3, "bed:bed", 2)
            hput(1, F, d - 2, "bed:bed_head", 2)
            hput(1, F, 1, "craft:table")
            hput(w - 2, F, 1, "chest:box", 3)
            if storeys > 1 then
                -- Up the ladder, a second bed and its own table: two homes.
                hput(1, F + 4, d - 3, "bed:bed", 2)
                hput(1, F + 4, d - 2, "bed:bed_head", 2)
                hput(1, F + 4, 1, "craft:table")
            end
            if keep ~= nil then
                hput(w - 2, F, d - 3, keep)   -- whose it is
            end
            if o.kind == "keep_museum" then
                -- The cases the curator shows the collection on (museum.luau):
                -- along the back wall and down both sides, clear of the door.
                for _, c in { { 2, d - 2 }, { 3, d - 2 }, { 4, d - 2 }, { w - 2, 2 }, { w - 2, 3 }, { 1, 2 }, { 1, 3 } } do
                    hput(c[1], F, c[2], "villagers:plinth")
                end
            end
        end
        if split ~= nil then
            hput(split - 1, F + 2, 1, "torches:torch")
        end
    elseif o.kind == "garden" then
        -- A fence round it, its gate in the middle of the front; wheat sown.
        local ring: { [string]: boolean } = {}
        for lz = 0, d - 1 do
            for lx = 0, w - 1 do
                if lx == 0 or lz == 0 or lx == w - 1 or lz == d - 1 then
                    local x, z = at(o, lx, lz)
                    ring[`{x},{z}`] = true
                end
            end
        end
        for lz = 0, d - 1 do
            for lx = 0, w - 1 do
                local x, z = at(o, lx, lz)
                if ring[`{x},{z}`] then
                    if lz == 0 and lx == w // 2 then
                        hput(lx, F, lz, "building:oak_fence_gate", 0)
                    else
                        put(x, F, z, fence_mask(ring, x, z))
                    end
                elseif not (lx == w // 2 and lz == d // 2) then
                    put(x, F, z, "farm:wheat_0")
                end
            end
        end
    end

    -- The way, two wide, its walking height h: the ground made up under it,
    -- cobblestone on top, air over it, a stair up to each higher step.
    road = true
    for i, c in o.road do
        local x, z, g, h, wx, wz = c[1], c[2], c[3], c[4], c[5], c[6]
        for _, cell in { { x, z }, { x + wx, z + wz } } do
            local cx, cz = cell[1], cell[2]
            for y = h + 3, h, -1 do
                put(cx, y, cz, "")
            end
            for y = g + 1, h - 2 do
                put(cx, y, cz, "core:dirt")
            end
            -- The village's own ways are trodden earth, as the paths between
            -- villages: a cobbled way of 28 cells was most of the stone a
            -- keeper's house waited an hour for.
            put(cx, h - 1, cz, if o.buyer == "" then "rocks:coarse_dirt" else "rocks:cobblestone")
            local n = o.road[i + 1]
            local p = o.road[i - 1]
            if n ~= nil and n[4] == h + 1 then
                put(cx, h, cz, "steps:cobblestone_stair", turn_of(n[1] - x, n[2] - z))
            elseif p ~= nil and p[4] == h + 1 then
                put(cx, h, cz, "steps:cobblestone_stair", turn_of(p[1] - x, p[2] - z))
            end
        end
    end
    return out
end

-- What is still to be put, by kind, from the ground as it is now.
local function bill_of(o: Order): { [string]: number }
    local need = { wood = 0, stone = 0, earth = 0 }
    for _, p in plan_of(o) do
        local c = cost_of(p[4])
        if c ~= nil and core.get_block(p[1], p[2], p[3]) ~= id(p[4]) then
            need[c[1]] += c[2]
        end
    end
    for m, n in need do
        need[m] = math.ceil(n)
    end
    return need
end

-- Whether a w by d lot fits with its corner box at x0, z0 (a square of the
-- longer side, so it may face any way): bare ground no player owns, a cell
-- to spare round it, no more than 3 up or down. Its door faces (fx, fz).
-- The turn, the floor and the heights in its own frame, row by row.
local function try_spot(y: number, x0: number, z0: number, w: number, d: number, fx: number, fz: number)
    : (number?, number?, { number }?)
    local S = math.max(w, d)
    local g, lo, hi = {}, math.huge, -math.huge
    for dz = -1, S do
        for dx = -1, S do
            local x, z = x0 + dx, z0 + dz
            local owner = core.owned_at(x, z)
            local gy, gb = ground_at(x, y, z)
            if (owner >= 32768 and owner < 65536) or gy == nil or not is_in("natural", NATURAL, gb) then
                return nil
            end
            g[`{dx},{dz}`] = gy
            lo, hi = math.min(lo, gy), math.max(hi, gy)
        end
    end
    if hi - lo > 3 then
        return nil
    end
    local cx, cz = x0 + S / 2, z0 + S / 2
    local dx, dz = fx - cx, fz - cz
    local t = if math.abs(dz) >= math.abs(dx) then (if dz < 0 then 0 else 2) else (if dx < 0 then 1 else 3)
    local o = { x0 = x0, z0 = z0, t = t, w = w, d = d } :: any
    local heights, sum = {}, 0
    for lz = 0, d - 1 do
        for lx = 0, w - 1 do
            local x, z = at(o, lx, lz)
            local h = g[`{x - x0},{z - z0}`] :: number
            table.insert(heights, h)
            sum += h
        end
    end
    return t, math.floor(sum / #heights + 0.5) + 1, heights
end

-- Each village's orders, the builder's first.
local orders: { [string]: { Order } } = {}

-- Where a lot goes: round the village, nearest it first, its door to the
-- middle, clear of what its builder is still to build -- three of the
-- village's own orders went to one lot.
local function find_lot(rk: string, w: number, d: number): (number?, number?, number?, number?, { number }?)
    local b = common.villages[rk]
    if b == nil then
        return nil
    end
    local function free(x0: number, z0: number): boolean
        local S = math.max(w, d)
        for _, o in orders[rk] or {} do
            local x1, z1, x2, z2 = rect_of(o)
            if x0 - 1 <= x2 and x0 + S >= x1 and z0 - 1 <= z2 and z0 + S >= z1 then
                return false
            end
        end
        return true
    end
    local S = math.max(w, d)
    local mx, mz = (b.x1 + b.x2) / 2, (b.z1 + b.z2) / 2
    for r = -10, 16, 2 do
        local spots = {}
        for x0 = b.x1 - r - S, b.x2 + r, 3 do
            table.insert(spots, { x0, b.z1 - r - S })
            table.insert(spots, { x0, b.z2 + r })
        end
        for z0 = b.z1 - r - S, b.z2 + r, 3 do
            table.insert(spots, { b.x1 - r - S, z0 })
            table.insert(spots, { b.x2 + r, z0 })
        end
        for _, sp in spots do
            local t, floor, g = nil, nil, nil
            if free(sp[1], sp[2]) then
                t, floor, g = try_spot(b.y, sp[1], sp[2], w, d, mx, mz)
            end
            if t ~= nil then
                return sp[1], sp[2], t, floor, g
            end
        end
    end
    return nil
end

-- The way from a lot's door to the nearest street, round what is built and
-- other players' lots (a search over the columns, 4 ways, a step of at most
-- 2): each cell {x, z, ground, walking height, dx, dz of its second cell}.
local function road_of(o: Order): { { number } }
    local sx, sz = at(o, o.w // 2, -1)
    local street: { [string]: boolean } = {}
    for _, name in ROAD_BLOCKS do
        local f = core.find_nodes(sx - 40, o.floor - 8, sz - 40, sx + 40, o.floor + 6, sz + 40, name)
        for i = 1, #f, 3 do
            if core.node_solid(core.get_block(f[i], f[i + 1] + 1, f[i + 2])) ~= 1 then
                street[`{f[i]},{f[i + 2]}`] = true
            end
        end
    end
    local function inside(cx: number, cz: number): boolean
        local x1, z1, x2, z2 = rect_of(o)
        return cx >= x1 and cx <= x2 and cz >= z1 and cz <= z2
    end
    -- A column a way may take, and its ground: nil where it may not.
    local seen: { [string]: number | boolean } = {}
    local function ground(cx: number, cz: number): number?
        local k = `{cx},{cz}`
        local c = seen[k]
        if c == nil then
            c = false
            local owner = core.owned_at(cx, cz)
            local gy, gb = ground_at(cx, o.floor, cz)
            if gy ~= nil and not inside(cx, cz) and not (owner >= 32768 and owner < 65536)
                and (is_in("natural", NATURAL, gb) or is_in("road", ROAD_BLOCKS, gb)) then
                c = gy
                for y = gy + 1, gy + 3 do
                    local b = core.get_block(cx, y, cz)
                    if b ~= AIR and core.node_solid(b) == 1 and not is_in("natural", NATURAL, b)
                        and not tree_log(cx, y, cz) and not is_in("leaves", LEAF_NAMES, b) then
                        c = false   -- a wall, a fence, somebody's house
                        break
                    end
                end
            end
            seen[k] = c
        end
        return if c == false then nil else c :: number
    end
    local g0 = ground(sx, sz)
    if g0 == nil then
        return {}
    end
    local from: { [string]: string } = { [`{sx},{sz}`] = "" }
    local queue, head, goal = { { sx, sz } }, 1, nil
    while head <= #queue and head < 6000 do
        local c = queue[head]
        head += 1
        local cg = ground(c[1], c[2]) :: number
        for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
            local nx, nz = c[1] + d[1], c[2] + d[2]
            local k = `{nx},{nz}`
            if from[k] == nil and math.abs(nx - sx) <= 40 and math.abs(nz - sz) <= 40 then
                if street[k] then
                    from[k] = `{c[1]},{c[2]}`
                    goal = k
                    break
                end
                local ng = ground(nx, nz)
                if ng ~= nil and math.abs(ng - cg) <= 2 then
                    from[k] = `{c[1]},{c[2]}`
                    table.insert(queue, { nx, nz })
                end
            end
        end
        if goal ~= nil then
            break
        end
    end
    if goal == nil then
        return {}
    end
    -- Back from the street to the door, without the street's own cell.
    local path = {}
    local k = from[goal]
    while k ~= nil and k ~= "" do
        local x, z = string.match(k, "^(-?%d+),(-?%d+)$")
        table.insert(path, 1, { tonumber(x) :: number, tonumber(z) :: number })
        k = from[k]
    end
    local cells: { { number } } = {}
    for i, c in path do
        local n = path[i + 1] or path[i - 1] or c
        local run_x = n[1] ~= c[1]
        -- Its second cell across the way's run, on a side it may take.
        local wx, wz = if run_x then 0 else 1, if run_x then 1 else 0
        if ground(c[1] + wx, c[2] + wz) == nil then
            wx, wz = -wx, -wz
            if ground(c[1] + wx, c[2] + wz) == nil then
                wx, wz = 0, 0
            end
        end
        local g = ground(c[1], c[2]) :: number
        table.insert(cells, { c[1], c[2], g, g + 1, wx, wz })
    end
    if #cells < 1 then
        return {}
    end
    -- From the door's floor, never more than a step from the cell before; to
    -- the street's height at the far end, the same from there back.
    cells[1][4] = o.floor
    for i = 2, #cells do
        cells[i][4] = math.clamp(cells[i][4], cells[i - 1][4] - 1, cells[i - 1][4] + 1)
    end
    for i = #cells - 1, 2, -1 do
        cells[i][4] = math.clamp(cells[i][4], cells[i + 1][4] - 1, cells[i + 1][4] + 1)
    end
    return cells
end

-- ------------------------------------------------------------ kept


local houses: { [string]: { House } } = {}
local function save_houses(rk: string)
    local parts = {}
    for _, h in houses[rk] or {} do
        table.insert(parts, `{h.x0},{h.z0},{h.t},{h.floor},{h.w},{h.d},{h.style},{h.storeys},`
            .. `{if h.garden then 1 else 0},{h.buyer}`)
    end
    core.storage_set(`houses {rk}`, table.concat(parts, ";"))
end
local function house_of(rk: string, name: string): House?
    for _, h in houses[rk] or {} do
        if h.buyer == name then
            return h
        end
    end
    return nil
end


local function nums(list: { number }): string
    local out = table.create(#list)
    for _, n in list do
        table.insert(out, tostring(n))
    end
    return table.concat(out, ",")
end
local function mats(m: { [string]: number }): string
    return `{m.wood or 0},{m.stone or 0},{m.earth or 0}`
end
local function read_mats(s: string): { [string]: number }
    local a, b, c = string.match(s, "^(%d+),(%d+),(%d+)$")
    return { wood = tonumber(a) or 0, stone = tonumber(b) or 0, earth = tonumber(c) or 0 }
end

-- An order a key (its way can be long), and their number under "orders".
local MAX_ORDERS = 6
local function save_orders(rk: string)
    local was = tonumber(core.storage_get(`orders {rk}`)) or 0
    local list = orders[rk] or {}
    for i, o in list do
        local road = {}
        for _, c in o.road do
            table.insert(road, nums(c))
        end
        core.storage_set(`order {rk} {i}`, table.concat({ o.kind, o.buyer, tostring(o.x0), tostring(o.z0),
            tostring(o.t), tostring(o.floor), o.stage, nums(o.g), table.concat(road, ";"), mats(o.need),
            mats(o.have), tostring(o.w), tostring(o.d), o.style, tostring(o.storeys),
            if o.self then "1" else "0" }, "|"))
    end
    for i = #list + 1, was do
        core.storage_set(`order {rk} {i}`, "")
    end
    core.storage_set(`orders {rk}`, if #list > 0 then tostring(#list) else "")
end

local function save_lots(rk: string)
    local parts = {}
    for _, l in lots[rk] or {} do
        table.insert(parts, nums(l))
    end
    core.storage_set(`lots {rk}`, table.concat(parts, ";"))
end

core.event.on("core:world_ready", function()
    for rk in string.gmatch(core.storage_get("regions"), "[^;]+") do
        for part in string.gmatch(core.storage_get(`houses {rk}`), "[^;]+") do
            local f = string.split(part, ",")
            if #f == 10 then
                houses[rk] = houses[rk] or {}
                table.insert(houses[rk], { x0 = tonumber(f[1]) :: number, z0 = tonumber(f[2]) :: number,
                    t = tonumber(f[3]) :: number, floor = tonumber(f[4]) :: number, w = tonumber(f[5]) :: number,
                    d = tonumber(f[6]) :: number, style = f[7], storeys = tonumber(f[8]) :: number,
                    garden = f[9] == "1", buyer = f[10] })
            end
        end
        for part in string.gmatch(core.storage_get(`lots {rk}`), "[^;]+") do
            local l = {}
            for n in string.gmatch(part, "-?%d+") do
                table.insert(l, tonumber(n) :: number)
            end
            if #l == 4 then
                lots[rk] = lots[rk] or {}
                table.insert(lots[rk], l)
            end
        end
        local index = core.storage_get(`orders {rk}`)
        local parts = {}
        if tonumber(index) then
            for i = 1, tonumber(index) :: number do
                table.insert(parts, core.storage_get(`order {rk} {i}`))
            end
        else
            for part in string.gmatch(index, "[^#]+") do   -- all in one key, as kept before
                table.insert(parts, part)
            end
        end
        for _, part in parts do
            local f = string.split(part, "|")
            if #f == 11 or #f == 15 or #f == 16 then
                local g = {}
                for n in string.gmatch(f[8], "-?%d+") do
                    table.insert(g, tonumber(n) :: number)
                end
                local road = {}
                for c in string.gmatch(f[9], "[^;]+") do
                    local cell = {}
                    for n in string.gmatch(c, "-?%d+") do
                        table.insert(cell, tonumber(n) :: number)
                    end
                    table.insert(road, cell)
                end
                orders[rk] = orders[rk] or {}
                table.insert(orders[rk], { kind = f[1], buyer = f[2], x0 = tonumber(f[3]) :: number,
                    z0 = tonumber(f[4]) :: number, t = tonumber(f[5]) :: number, floor = tonumber(f[6]) :: number,
                    stage = f[7], g = g, road = road, need = read_mats(f[10]), have = read_mats(f[11]),
                    since = 0, asked = 0, w = tonumber(f[12]) or 7, d = tonumber(f[13]) or 7,
                    style = f[14] or "wood", storeys = tonumber(f[15]) or 1, self = f[16] == "1" })
            end
        end
    end
end)

-- The order of a village still waiting for something, and what it lacks.
local function wanting(rk: string): (Order?, { [string]: number })
    local o = (orders[rk] or {})[1]
    local lack = {}
    if o == nil or o.stage ~= "gather" then
        return nil, lack
    end
    for _, m in MATS do
        if (o.have[m] or 0) < (o.need[m] or 0) then
            lack[m] = o.need[m] - (o.have[m] or 0)
        end
    end
    return o, lack
end

local function lack_said(lack: { [string]: number }): string
    local parts = {}
    for _, m in MATS do
        if (lack[m] or 0) > 0 then
            table.insert(parts, `{lack[m]} {SAID[m]}`)
        end
    end
    return table.concat(parts, ", ")
end

local function deliver(o: Order, m: string, n: number): number
    local want = math.max(0, (o.need[m] or 0) - (o.have[m] or 0))
    local given = math.min(want, n)
    o.have[m] = (o.have[m] or 0) + given
    o.since = common.clock
    return given
end

-- What the files after this one use of it.
common.at = at
common.bill_of = bill_of
common.BRINGS = BRINGS
common.cost_of = cost_of
common.deliver = deliver
common.find_lot = find_lot
common.FLOOR_PRICE = FLOOR_PRICE
common.GARDEN_PRICE = GARDEN_PRICE
common.ground_at = ground_at
common.GUARD_DAY = GUARD_DAY
common.house_of = house_of
common.houses = houses
common.is_in = is_in
common.ITEMS = ITEMS
common.lack_said = lack_said
common.LEAF_NAMES = LEAF_NAMES
common.LOG_NAMES = LOG_NAMES
common.LOT = LOT
common.mat_of = mat_of
common.MATS = MATS
common.MAX_ORDERS = MAX_ORDERS
common.NATURAL = NATURAL
common.orders = orders
common.plan_of = plan_of
common.pockets_here = pockets_here
common.rect_of = rect_of
common.ROAD_BLOCKS = ROAD_BLOCKS
common.road_of = road_of
common.SAID = SAID
common.save_houses = save_houses
common.save_lots = save_lots
common.save_orders = save_orders
common.SIZES = SIZES
common.STYLES = STYLES
common.tree_log = tree_log
common.try_spot = try_spot
common.wanting = wanting
