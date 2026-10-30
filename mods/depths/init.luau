--!strict
-- depths: things built under the ground.
--
-- The world is cut into regions of 128 blocks, and about one region in two has
-- something under it, some six to twelve blocks down:
--
--   * an ABANDONED MINE: a hall of planks with a table and a chest, and from
--     it galleries three wide and three high running out up to forty blocks,
--     some branching, propped every five by two logs and a beam of planks,
--     planks laid along the floor here and there, torches, cobwebs in the
--     corners and a chest at the end of some of them;
--   * a CRYPT: a hall of stone bricks, mossy in places, with a lamp in its
--     ceiling and a spawner in its middle, and four tombs off it, each with a
--     coffin of slabs, two of them with a chest.
--
-- Each goes up to the surface by a stair cut through the rock, so it can be
-- found from above as well as dug into from a cave.
--
-- Like mods/villages, a structure is planned once from the seed and
-- core.terrain_height -- a list of boxes and blocks -- and every column builds
-- only its own cells of it. It stays inside its region (its middle is at least
-- 56 from the edge and nothing reaches further than 52), never closer than 4
-- under the ground (a gallery that would come nearer stops there), never under
-- the sea, and it is not built on the plaza.

local REGION = 128
local SEA = 18
local PLAZA_LO: number, PLAZA_HI: number = math.huge, -math.huge
do   -- core.plaza(): nil in a world without one (survival), with nothing to keep clear
    local lo, hi = core.plaza()
    if lo and hi then
        PLAZA_LO, PLAZA_HI = lo, hi
    end
end

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

local N: { [string]: number } = {}
local function id(name: string): number
    local v = N[name]
    if v == nil then
        v = core.node_id(name)
        N[name] = v
    end
    return v
end

-- ------------------------------------------------------------ plans

-- What a plan is made of, built in order: a box of one block (or of a mix,
-- decided cell by cell from the cell and the plan's hash), a block in one
-- cell, turned or not, and a chest with what it holds.
type Op = { k: string, x0: number, y0: number, z0: number, x1: number, y1: number, z1: number,
            b: string, f: number?, loot: { { any } }? }
type Plan = { ops: { Op }, x0: number, z0: number, x1: number, z1: number, h: number }

local function box(plan: Plan, x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: string)
    table.insert(plan.ops, { k = "box", x0 = math.min(x0, x1), y0 = math.min(y0, y1), z0 = math.min(z0, z1),
                             x1 = math.max(x0, x1), y1 = math.max(y0, y1), z1 = math.max(z0, z1), b = b })
end
local function put(plan: Plan, x: number, y: number, z: number, b: string, f: number?)
    table.insert(plan.ops, { k = "at", x0 = x, y0 = y, z0 = z, x1 = x, y1 = y, z1 = z, b = b, f = f })
end
local function chest(plan: Plan, x: number, y: number, z: number, loot: { { any } })
    table.insert(plan.ops, { k = "chest", x0 = x, y0 = y, z0 = z, x1 = x, y1 = y, z1 = z, b = "chest:box",
                             loot = loot })
end

-- Whether a cell is at least 4 under the ground, and the ground over it is
-- not under water.
local function deep(x: number, y: number, z: number): boolean
    local g = core.terrain_height(x, z)
    return g >= SEA + 1 and g >= core.water_level(x, z) and y <= g - 5
end

-- A turn whose high side faces (dx, dz), as mods/villages turns its stairs.
local function turn_of(dx: number, dz: number): number
    if dz < 0 then return 0 elseif dx < 0 then return 1 elseif dz > 0 then return 2 else return 3 end
end

-- A stair from (x, y, z) -- the floor of the room it leaves, outside its wall
-- -- up along (dx, dz) to the surface: three wide, four high, a step a cell.
-- Not made if it would come up in water or run out of the region.
local function stair_up(plan: Plan, x: number, y: number, z: number, dx: number, dz: number, step: string,
                        floor: string)
    local px, pz = -dz, dx
    local cells = {}
    for k = 0, 48 do
        local cx, cz = x + dx * k, z + dz * k
        local fy = y + k
        local g = core.terrain_height(cx, cz)
        if g < SEA + 1 or g < core.water_level(cx, cz) then
            return   -- under a river or a lake, or coming up in one
        end
        table.insert(cells, { cx, cz, fy })
        if fy >= g then
            for _, c in cells do
                box(plan, c[1] - px, c[3], c[2] - pz, c[1] + px, c[3] + 3, c[2] + pz, "air")
                box(plan, c[1] - px, c[3] - 1, c[2] - pz, c[1] + px, c[3] - 1, c[2] + pz, floor)
                if c[3] < fy then
                    put(plan, c[1], c[3], c[2], step, turn_of(dx, dz))
                    put(plan, c[1] - px, c[3], c[2] - pz, step, turn_of(dx, dz))
                    put(plan, c[1] + px, c[3], c[2] + pz, step, turn_of(dx, dz))
                end
            end
            return
        end
    end
end

local MINE_LOOT = { { "ore:coal", 12 }, { "furnace:iron_ingot", 5 }, { "torches:torch", 8 }, { "farm:bread", 4 },
                    { "furnace:iron_pick", 1 }, { "timber:planks", 10 }, { "furnace:gold_ingot", 3 },
                    { "ore:cinnabar", 8 }, { "ore:diamond", 1 } }
local CRYPT_LOOT = { { "mobs:bone", 10 }, { "furnace:gold_ingot", 5 }, { "ore:emerald", 3 }, { "ore:diamond", 2 },
                     { "archery:arrow", 12 }, { "furnace:iron_ingot", 4 }, { "torches:torch", 6 } }

-- An abandoned mine round (cx, y, cz).
local function mine(plan: Plan, cx: number, y: number, cz: number, h: number)
    -- The hall: 9 by 9, four high, a floor of planks, a post in each corner.
    box(plan, cx - 4, y, cz - 4, cx + 4, y + 3, cz + 4, "air")
    box(plan, cx - 4, y - 1, cz - 4, cx + 4, y - 1, cz + 4, "timber:planks")
    for _, c in { { -4, -4 }, { 4, -4 }, { -4, 4 }, { 4, 4 } } do
        box(plan, cx + c[1], y, cz + c[2], cx + c[1], y + 3, cz + c[2], "timber:log")
    end
    put(plan, cx - 3, y, cz - 3, "craft:table")
    chest(plan, cx + 3, y, cz - 3, MINE_LOOT)
    put(plan, cx - 3, y, cz + 3, "torches:torch")
    put(plan, cx + 3, y, cz + 3, "torches:torch")

    -- A gallery from (sx, sz) along (ux, uz), `len` long at most: it stops
    -- where the rock over it would be thinner than 4. Returns how far it went.
    local function gallery(sx: number, sz: number, ux: number, uz: number, len: number, salt: number): number
        local px, pz = -uz, ux
        local went = 0
        for k = 0, len - 1 do
            local x, z = sx + ux * k, sz + uz * k
            if not (deep(x, y + 2, z) and deep(x + px, y + 2, z + pz) and deep(x - px, y + 2, z - pz)) then
                break
            end
            went = k + 1
        end
        if went < 3 then
            return 0
        end
        local ex, ez = sx + ux * (went - 1), sz + uz * (went - 1)
        box(plan, sx - px, y, sz - pz, ex + px, y + 2, ez + pz, "air")
        for k = 0, went - 1 do
            local x, z = sx + ux * k, sz + uz * k
            local r = hash(x, z, salt)
            if k % 5 == 2 then
                -- A prop: two logs and a beam of planks over the way.
                put(plan, x + px, y, z + pz, "timber:log")
                put(plan, x + px, y + 1, z + pz, "timber:log")
                put(plan, x - px, y, z - pz, "timber:log")
                put(plan, x - px, y + 1, z - pz, "timber:log")
                box(plan, x - px, y + 2, z - pz, x + px, y + 2, z + pz, "timber:planks")
            elseif r % 4 == 0 then
                put(plan, x, y - 1, z, "timber:planks")
            end
            if k % 10 == 7 then
                put(plan, x + px, y, z + pz, "torches:torch")
            elseif r % 7 == 1 then
                put(plan, x - px, y + 2, z - pz, "plants:cobweb")
            elseif r % 9 == 2 then
                put(plan, x + px, y + 2, z + pz, "plants:cobweb")
            end
        end
        if hash(ex, ez, salt + 1) % 3 == 0 then
            chest(plan, ex, y, ez, MINE_LOOT)
        end
        return went
    end

    local entrance = false
    for i, d in { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } } do
        local r = hash(cx + i, cz, h)
        if r % 4 ~= 0 or i == 1 then
            local len = 16 + r % 25
            local went = gallery(cx + d[1] * 5, cz + d[2] * 5, d[1], d[2], len, h + i)
            -- A branch either side, from somewhere along it.
            if went >= 16 then
                local at = 8 + bit32.rshift(r, 8) % (went - 12)
                local bx, bz = cx + d[1] * (5 + at), cz + d[2] * (5 + at)
                for _, s in { 1, -1 } do
                    if bit32.rshift(r, 12 + s) % 2 == 0 then
                        gallery(bx - d[2] * s * 2, bz + d[1] * s * 2, -d[2] * s, d[1] * s, 8 + bit32.rshift(r, 16) % 13,
                            h + i * 7 + s)
                    end
                end
            end
            -- The way up, from the end of the first gallery that has one.
            if not entrance and went >= 6 then
                local ex, ez = cx + d[1] * (5 + went), cz + d[2] * (5 + went)
                local before = #plan.ops
                stair_up(plan, ex, y, ez, d[1], d[2], "steps:oak_stair", "timber:planks")
                entrance = #plan.ops > before
            end
        end
    end
end

-- Stone bricks, a mossy one in four: what a crypt is made of.
local function crypt(plan: Plan, cx: number, y: number, cz: number, h: number)
    -- The hall: 9 by 9 inside, five high, in a shell of brick.
    box(plan, cx - 5, y - 1, cz - 5, cx + 5, y + 5, cz + 5, "brick")
    box(plan, cx - 4, y, cz - 4, cx + 4, y + 4, cz + 4, "air")
    put(plan, cx, y + 5, cz, "ruins:lamp")
    put(plan, cx, y, cz, "dungeons:spawner")
    for _, c in { { -4, -4 }, { 4, -4 }, { -4, 4 }, { 4, 4 } } do
        box(plan, cx + c[1], y, cz + c[2], cx + c[1], y + 4, cz + c[2], "rocks:stone_bricks")
        put(plan, cx + c[1] - math.sign(c[1]), y + 4, cz + c[2] - math.sign(c[2]), "plants:cobweb")
    end
    -- A tomb off every side: a way three long, a room 5 by 5, a coffin of
    -- slabs along its back, and in two of them a chest at its foot.
    local exits: { { number } } = {}
    for i, d in { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } } do
        local px, pz = -d[2], d[1]
        local wx, wz = cx + d[1] * 5, cz + d[2] * 5          -- the hall's wall
        local mx, mz = cx + d[1] * 11, cz + d[2] * 11        -- the tomb's middle
        box(plan, wx + d[1] - px * 2, y - 1, wz + d[2] - pz * 2, wx + d[1] * 3 + px * 2, y + 4, wz + d[2] * 3 + pz * 2,
            "brick")
        box(plan, mx - 3, y - 1, mz - 3, mx + 3, y + 4, mz + 3, "brick")
        box(plan, wx - px, y, wz - pz, wx + d[1] * 3 + px, y + 2, wz + d[2] * 3 + pz, "air")
        box(plan, mx - 2, y, mz - 2, mx + 2, y + 3, mz + 2, "air")
        local bx, bz = mx + d[1] * 2, mz + d[2] * 2          -- against the back wall
        box(plan, bx - px, y, bz - pz, bx + px, y, bz + pz, "steps:slab")
        put(plan, mx + d[1], y + 3, mz + d[2], "torches:torch")
        if (h + i) % 2 == 0 then
            chest(plan, mx + px * 2, y, mz + pz * 2, CRYPT_LOOT)
        end
        table.insert(exits, { mx + d[1] * 3, mz + d[2] * 3, d[1], d[2] })
    end
    -- Up from the back of one tomb.
    local e = exits[1 + h % 4]
    stair_up(plan, e[1] + e[3], y, e[2] + e[4], e[3], e[4], "steps:stair", "rocks:cobblestone")
    box(plan, e[1] - e[4], y, e[2] + e[3], e[1] + e[4], y + 2, e[2] - e[3], "air")
end

local PLAN: { [string]: Plan | false } = {}

local function plan_of(rx: number, rz: number, seed: number): Plan?
    local key = `{rx},{rz}`
    local cached = PLAN[key]
    if cached ~= nil then
        return if cached then cached else nil
    end
    local found: Plan | false = false
    local h = hash(rx, rz, seed + 0xDE9745)
    if h % 2 == 0 then
        local cx = rx * REGION + 56 + bit32.rshift(h, 4) % 17
        local cz = rz * REGION + 56 + bit32.rshift(h, 9) % 17
        local y = 6 + bit32.rshift(h, 14) % 5
        local plaza = cx + 60 >= PLAZA_LO and cx - 60 <= PLAZA_HI and cz + 60 >= PLAZA_LO and cz - 60 <= PLAZA_HI
        -- The middle must be deep enough for the hall and its shell.
        local ok = not plaza
        for dz = -6, 6, 3 do
            for dx = -6, 6, 3 do
                ok = ok and deep(cx + dx, y + 6, cz + dz)
            end
        end
        if ok then
            local plan: Plan = { ops = {}, x0 = cx, z0 = cz, x1 = cx, z1 = cz, h = h }
            local kind = if bit32.rshift(h, 20) % 2 == 0 then "mine" else "crypt"
            if kind == "mine" then
                mine(plan, cx, y, cz, h)
            else
                crypt(plan, cx, y, cz, h)
            end
            for _, op in plan.ops do
                plan.x0, plan.z0 = math.min(plan.x0, op.x0), math.min(plan.z0, op.z0)
                plan.x1, plan.z1 = math.max(plan.x1, op.x1), math.max(plan.z1, op.z1)
            end
            core.log(`depths: {if kind == "mine" then "an abandoned mine" else "a crypt"} at {cx} {y} {cz}, `
                .. `{#plan.ops} parts`)
            found = plan
        end
    end
    PLAN[key] = found
    return if found then found else nil
end

-- ------------------------------------------------------------ the stage

local function stock(x: number, y: number, z: number, loot: { { any } }, h: number)
    core.container_make(x, y, z, 27)
    for i = 0, 3 + h % 3 do
        local pick = loot[1 + bit32.rshift(h, i * 3) % #loot]
        local item = core.item_id(pick[1])
        if item ~= 0 and core.container_count(x, y, z, item) == 0 then
            core.container_add(x, y, z, item, 1 + bit32.rshift(h, i * 2 + 1) % pick[2])
        end
    end
end

core.register_mapgen("depths:build", function(ctx)
    local rx, rz = math.floor(ctx.min_x / REGION), math.floor(ctx.min_z / REGION)
    local plan = plan_of(rx, rz, ctx.seed)
    if plan == nil or plan.x1 < ctx.min_x or plan.x0 > ctx.max_x or plan.z1 < ctx.min_z or plan.z0 > ctx.max_z then
        return
    end
    local brick, mossy = id("rocks:stone_bricks"), id("ruins:mossy_brick")
    for _, op in plan.ops do
        local x0, x1 = math.max(op.x0, ctx.min_x), math.min(op.x1, ctx.max_x)
        local z0, z1 = math.max(op.z0, ctx.min_z), math.min(op.z1, ctx.max_z)
        if x0 <= x1 and z0 <= z1 then
            if op.k == "box" then
                local single = if op.b == "air" then 0 elseif op.b == "brick" then -1 else id(op.b)
                for z = z0, z1 do
                    for x = x0, x1 do
                        for y = op.y0, op.y1 do
                            local b = single
                            if b < 0 then
                                b = if hash(x * 31 + y, z, plan.h) % 4 == 0 then mossy else brick
                            end
                            core.set_block(x, y, z, b)
                        end
                    end
                end
            else
                local b = id(op.b)
                if b ~= 0 then
                    core.set_block(op.x0, op.y0, op.z0, b)
                    if op.f then
                        core.set_facing(op.x0, op.y0, op.z0, op.f)
                    end
                    if op.k == "chest" and op.loot then
                        stock(op.x0, op.y0, op.z0, op.loot, hash(op.x0, op.z0, plan.h + op.y0))
                    end
                end
            end
        end
    end
end)
