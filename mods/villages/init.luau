--!strict
-- Villages and towns, and the paths between them.
--
-- The world is cut into regions of 256 blocks. One region in three has a
-- settlement near its middle: a village of two to four streets, or now and then
-- a town of four long streets with cross streets, paved in cobblestone, with a
-- tower. Every settlement starts at a square with a well; along its streets
-- stand houses, farms and a smithy, each with its door to the street, and
-- lamps in the gaps between them. A house has a bed, a crafting table and a
-- chest for whoever lives in it (mods/villagers). Streets and paths follow
-- the land smoothed over five cells, raised on their own block where the
-- land dips, and climb every step by a stair (or a slab) of what they are
-- paved with -- gravel, cobblestone, smooth stone, coarse dirt, planks. A path of coarse dirt leaves every settlement
-- for the nearest one in the regions around it. It goes round the sea and the
-- ocean, and crosses a river or a lake on a bridge: planks arched up over the
-- water by stairs, a fence along each side, logs down to the bed under it.
--
-- A settlement is wider than a column (32), so it is shaped like mods/ruins and
-- not like mods/trees: the whole plan of a region -- streets, plots, lamps --
-- is worked out once from the seed, core.terrain_height and core.biome, kept,
-- and every column of the region builds only its own cells of it. Nothing here
-- reads a block of another column, which may not have been made yet. The plan
-- of a region stays inside it (its centre is at least 80 from the edge and
-- nothing reaches further than 64 from the centre); a path does not, so a
-- column looks at the settlements of the 5 by 5 regions around its own.

local REGION = 256    -- a multiple of 32, so a column is in one region only
local SEA    = 18     -- mods/lakes fills the ground below this with water

-- Where the water stands over a column: the sea's level, or a lake's up in the
-- hills (core.water_level); a column whose ground is under it is wet.
local function water_top(x: number, z: number): number
    local w = core.water_level(x, z)
    return if w == 0 then SEA else math.max(w, SEA)
end
local function wet(x: number, z: number): boolean
    return core.terrain_height(x, z) < water_top(x, z)
end
-- A gorge (core:gorge) is crossed at the height of its rims: the highest
-- ground six out either way.
local function rim(x: number, z: number): number
    local r = core.terrain_height(x, z)
    for _, d in { { 6, 0 }, { -6, 0 }, { 0, 6 }, { 0, -6 }, { 4, 4 }, { -4, -4 }, { 4, -4 }, { -4, 4 } } do
        r = math.max(r, core.terrain_height(x + d[1], z + d[2]))
    end
    return r
end
local PLAZA_LO, PLAZA_HI = 20, 100
local SEED: number? = nil   -- the world's, as the first column was handed it; /paese needs it

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

-- Block ids, looked up at the first column: every mod this one depends on has
-- registered its blocks by then.
local N: { [string]: number } = {}
local function id(name: string): number
    local v = N[name]
    if v == nil then
        v = core.node_id(name)
        N[name] = v
    end
    return v
end

-- A village hall's bell (below): which beds are the hall's.
local BELL = core.register_node("villages:bell", 214, 172, 52, true, 0, "", 0, 1.5, "metal")
core.node_box(BELL, 0.25, 0.0, 0.25, 0.75, 0.6, 0.75)
-- And whose the beds of the other keepers' buildings are (mods/villagers):
-- the barracks' banner (the watch), the merchant's scales, the builder's
-- plans; a smithy's anvil is the crafter's.
local BANNER = core.register_node("villages:banner", 168, 36, 36, true, 0, "", 0, 1.0, "wood")
core.node_box(BANNER, 0.4, 0.0, 0.4, 0.6, 1.0, 0.6)
local SCALES = core.register_node("villages:scales", 190, 160, 70, true, 0, "", 0, 1.0, "metal")
core.node_box(SCALES, 0.2, 0.0, 0.2, 0.8, 0.4, 0.8)
local PLANS = core.register_node("villages:plans", 70, 110, 170, true, 0, "", 0, 0.5, "wood")
core.node_box(PLANS, 0.1, 0.0, 0.1, 0.9, 0.15, 0.9)

-- ------------------------------------------------------------ settlements

type Village = { x: number, z: number, y: number, town: boolean, desert: boolean, cold: boolean, h: number, rx: number,
                 rz: number }

local VILLAGE: { [string]: Village | false } = {}

-- The settlement of a region, or nil. A function of the seed and the region.
local function village_of(rx: number, rz: number, seed: number): Village?
    local key = `{rx},{rz}`
    local cached = VILLAGE[key]
    if cached ~= nil then
        return if cached then cached else nil
    end

    local v: Village? = nil
    local h = hash(rx, rz, seed + 0x71A6E)
    -- One region in three, and never the one the plaza is in.
    if h % 3 == 0 and not (rx == 0 and rz == 0) then
        local x = rx * REGION + 80 + bit32.rshift(h, 4) % 97
        local z = rz * REGION + 80 + bit32.rshift(h, 12) % 97
        local biome = core.biome(x, z)
        local y = core.terrain_height(x, z)
        local far = x + 64 < PLAZA_LO - 16 or x - 64 > PLAZA_HI + 16 or z + 64 < PLAZA_LO - 16
            or z - 64 > PLAZA_HI + 16
        -- A town wants room: ground within 6 of the square 40 out every way.
        local town = bit32.rshift(h, 20) % 4 == 0
        for _, d in { { 40, 0 }, { -40, 0 }, { 0, 40 }, { 0, -40 } } do
            if math.abs(core.terrain_height(x + d[1], z + d[2]) - y) > 6 then
                town = false
            end
        end
        if far and biome ~= "core:peaks" and y > SEA and not wet(x, z) then
            v = { x = x, z = z, y = y, town = town,
                  desert = biome == "core:desert", cold = biome == "core:tundra", h = h, rx = rx, rz = rz }
        end
    end

    VILLAGE[key] = v or false
    return v
end

-- ------------------------------------------------------------ plans

type Rect = { x0: number, z0: number, x1: number, z1: number }

-- A building: its kind, the lowest corner of its footprint, its size in its
-- own frame (w along the street, d away from it), the turn t that takes that
-- frame into the world -- the door is on the side a stair's back or a door's
-- panel is at that turn: -Z, -X, +Z, +X for 0..3 -- and the floor it stands on.
type Building = { kind: string, x0: number, z0: number, w: number, d: number, t: number, floor: number, h: number,
                  ver: number?, mark: string? }

-- tips: the cell just past the end of each street leaving the square, where a
-- path comes in.
type Plan = { v: Village, streets: { Rect }, buildings: { Building }, lamps: { { number } }, tips: { { number } },
              paved: { [string]: { number } } }

-- The ground about a cell, smoothed: the mean of the ground (or the water's
-- surface) over the 5 by 5 cells round it, so a street or a path rises and
-- falls with the land and not with every bump of it.
local GROUND: { [string]: number } = {}
local function ground_at(x: number, z: number): number
    local key = `{x},{z}`
    local g = GROUND[key]
    if g == nil then
        g = math.max(core.terrain_height(x, z), water_top(x, z))
        GROUND[key] = g
    end
    return g
end
local function smooth_ground(x: number, z: number): number
    local sum = 0
    for dz = -2, 2 do
        for dx = -2, 2 do
            sum += ground_at(x + dx, z + dz)
        end
    end
    return math.max(math.floor(sum / 25 + 0.5), SEA)
end

-- Heights for a set of cells, each the smoothed ground cut down wherever it
-- stands more than 1 above a neighbour in the set, so that whoever walks it
-- never meets a step they cannot climb -- and every step of 1 is a stair
-- (pave). Lowest first, so a cell is final when its level is reached.
local function carve(cells: { [string]: { number } })
    local buckets: { [number]: { string } } = {}
    local lo, hi = math.huge, -math.huge
    for key, c in cells do
        local g = smooth_ground(c[1], c[2])
        if wet(c[1], c[2]) then
            g = math.max(g, water_top(c[1], c[2]) + 4)   -- a bridge's deck, cut down to its banks by stairs below
        end
        if core.biome(c[1], c[2]) == "core:gorge" then
            g = math.max(g, rim(c[1], c[2]))   -- over a gorge, at its rims
        end
        c[3] = g
        buckets[g] = buckets[g] or {}
        table.insert(buckets[g], key)
        lo, hi = math.min(lo, g), math.max(hi, g)
    end
    for level = lo, hi do
        local bucket = buckets[level]
        if bucket then
            for _, key in bucket do
                local c = cells[key]
                if c[3] == level then
                    for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
                        local n = cells[`{c[1] + d[1]},{c[2] + d[2]}`]
                        if n and n[3] > level + 1 then
                            n[3] = level + 1
                            buckets[level + 1] = buckets[level + 1] or {}
                            table.insert(buckets[level + 1], `{n[1]},{n[2]}`)
                        end
                    end
                end
            end
        end
    end
end

local PLAN: { [string]: Plan } = {}

-- A settlement's ground, for mods/villagers to make its territory of: the box
-- round all its streets and buildings, told when it is planned and again for
-- every one kept at core:world_ready (a village whose columns are all made is
-- not planned again).
core.event.define("villages:extent", {
    fields = { rx = "number", rz = "number", x1 = "number", z1 = "number", x2 = "number", z2 = "number" },
    cancellable = false,
})
local told_extent: { [string]: boolean } = {}
local function tell_extent(rx: number, rz: number, x1: number, z1: number, x2: number, z2: number)
    core.event.emit("villages:extent", { rx = rx, rz = rz, x1 = x1, z1 = z1, x2 = x2, z2 = z2 })
end

-- Which plan a village is laid out by, kept per village from the first time
-- it is planned after the storage is read: a village half made by one plan
-- gets its other columns from the same one when the plan changes. 1 had no
-- hall and no family or two-storey houses; 2 has them, every keeper's bed in
-- the hall; 3 sleeps them by their work -- the mayor in the hall, the watch
-- in barracks at a street's end, the crafter over the smithy, the merchant
-- and the builder in a house each by the square. Before the storage is
-- read (columns made before core:world_ready) the plan is today's. A village planned before keys were kept (2026-10-04) has none and
-- gets today's.
local PLAN_NOW = 3
local ready = false   -- the mod's storage has been read (core:world_ready)
local function plan_version(v: Village): number
    if not ready then
        return PLAN_NOW
    end
    local key = `plan {v.rx},{v.rz}`
    local kept = tonumber(core.storage_get(key))
    if kept == nil then
        kept = PLAN_NOW
        core.storage_set(key, tostring(kept))
    end
    return kept
end

local function overlaps(a: Rect, b: Rect): boolean
    return a.x0 <= b.x1 and b.x0 <= a.x1 and a.z0 <= b.z1 and b.z0 <= a.z1
end

local DIRS = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } }

-- The turn whose door side is the way (dx, dz) points.
local function turn_of(dx: number, dz: number): number
    if dz < 0 then return 0 elseif dx < 0 then return 1 elseif dz > 0 then return 2 else return 3 end
end

local function kind_for(v: Village, r: number, smithy: boolean, tower: boolean, ver: number): (string, number, number)
    local roll = r % 20
    if v.town and not tower and roll < 2 then
        return "tower", 5, 5
    elseif not smithy and roll < 4 then
        return "smithy", 7, 7
    elseif roll < (if v.town then 6 else 8) then
        return "farm", 9, 7
    end
    -- A house for a family, three beds round one table; a house of two
    -- storeys, a ladder up, a bed and a table on each; or one bed.
    local sort = if ver >= 2 then bit32.rshift(r, 7) % 5 else 4
    if sort == 0 then
        return "family", 7, 7
    elseif sort == 1 then
        return "tall", 7, 7
    end
    return "house", if bit32.rshift(r, 5) % 2 == 0 then 5 else 7, if bit32.rshift(r, 6) % 3 == 0 then 7 else 5
end

local function plan_of(v: Village, seed: number): Plan
    local key = `{v.rx},{v.rz}`
    local cached = PLAN[key]
    if cached then
        return cached
    end
    local ver = plan_version(v)

    local plan: Plan = { v = v, streets = {}, buildings = {}, lamps = {}, tips = {}, paved = {} }
    local taken: { Rect } = {}

    -- The square, nine across, and the streets leaving it.
    local square = { x0 = v.x - 4, z0 = v.z - 4, x1 = v.x + 4, z1 = v.z + 4 }
    table.insert(plan.streets, square)
    table.insert(taken, square)

    type Arm = { sx: number, sz: number, ux: number, uz: number, len: number }

    -- A street ends where the ground climbs or falls more than 6 from the
    -- square, or the mountains start.
    local function trimmed(sx: number, sz: number, ux: number, uz: number, len: number): number
        for k = 0, len - 1 do
            local x, z = sx + ux * k, sz + uz * k
            if math.abs(core.terrain_height(x, z) - v.y) > 6 or core.biome(x, z) == "core:peaks" then
                return k
            end
        end
        return len
    end

    local arms: { Arm } = {}
    local mask = bit32.band(bit32.rshift(v.h, 22), 15)
    if v.town then
        mask = 15
    elseif mask == 0 or mask == 1 or mask == 2 or mask == 4 or mask == 8 then
        mask = bit32.bor(mask, 5)   -- at least two
    end
    for i, d in DIRS do
        if bit32.band(mask, bit32.lshift(1, i - 1)) ~= 0 then
            local len = if v.town then 40 + hash(v.x, i, seed) % 17 else 24 + hash(v.x, i, seed) % 17
            local sx, sz = v.x + d[1] * 5, v.z + d[2] * 5
            len = trimmed(sx, sz, d[1], d[2], len)
            if len >= 8 then
                table.insert(arms, { sx = sx, sz = sz, ux = d[1], uz = d[2], len = len })
                table.insert(plan.tips, { sx + d[1] * len, sz + d[2] * len })
            end
        end
    end
    if v.town then
        -- Cross streets, 12 each way, 18 and 36 along every arm.
        for i = 1, #arms do
            local a = arms[i]
            for _, at in { 18, 36 } do
                if at < a.len - 6 then
                    local cx, cz = a.sx + a.ux * at, a.sz + a.uz * at
                    for _, s in { 1, -1 } do
                        local px, pz = -a.uz * s, a.ux * s
                        local len = trimmed(cx + px * 2, cz + pz * 2, px, pz, 12)
                        if len >= 8 then
                            table.insert(arms, { sx = cx + px * 2, sz = cz + pz * 2, ux = px, uz = pz, len = len })
                        end
                    end
                end
            end
        end
    end
    for _, a in arms do
        local ex, ez = a.sx + a.ux * (a.len - 1), a.sz + a.uz * (a.len - 1)
        local r = { x0 = math.min(a.sx, ex) - 1, z0 = math.min(a.sz, ez) - 1, x1 = math.max(a.sx, ex) + 1,
                    z1 = math.max(a.sz, ez) + 1 }
        table.insert(plan.streets, r)
        table.insert(taken, r)
    end

    -- Plots along both sides of every street, doors to the street.
    local smithy, tower, mine, hall, barracks = false, false, false, false, ver < 3
    local homes = if ver >= 3 then { "villages:scales", "villages:plans" } else {}
    for ai, a in arms do
        for _, s in { 1, -1 } do
            local px, pz = -a.uz * s, a.ux * s
            local along = 1
            while along < a.len do
                local r = hash(v.x + ai * 31 + s, v.z + along, seed + 0x9107)
                local kind, w, d = kind_for(v, r, smithy, tower, ver)
                local mark: string? = nil
                -- Every village has its hall, on the first plot it fits, and its
                -- mine, on the first plot clear of the square.
                -- Then the merchant's and the builder's houses by it, and the
                -- watch's barracks at a street's far end, where the village
                -- is come into.
                if not hall and ver >= 2 then
                    kind, w, d = "hall", 9, 7
                elseif #homes > 0 then
                    kind, w, d, mark = "house", 5, 5, homes[1]
                elseif not smithy and ver >= 3 then
                    kind, w, d = "smithy", 7, 7   -- the crafter's
                elseif not barracks and along >= a.len - 20 then
                    kind, w, d = "barracks", 9, 7
                elseif not mine and along >= 10 then
                    kind, w, d = "mine", 7, 7
                end
                if along + w > a.len then
                    break
                end
                -- Corners of the footprint: `along` to along+w-1, 3 to 3+d-1 out.
                local ax, az = a.sx + a.ux * along, a.sz + a.uz * along
                local bx, bz = ax + a.ux * (w - 1) + px * (3 + d - 1), az + a.uz * (w - 1) + pz * (3 + d - 1)
                ax, az = ax + px * 3, az + pz * 3
                local rect = { x0 = math.min(ax, bx), z0 = math.min(az, bz), x1 = math.max(ax, bx), z1 = math.max(az, bz) }
                local wide = { x0 = rect.x0 - 1, z0 = rect.z0 - 1, x1 = rect.x1 + 1, z1 = rect.z1 + 1 }

                local ok = true
                for _, o in taken do
                    if overlaps(wide, o) then
                        ok = false
                        break
                    end
                end

                -- The floor is the ground before the door, so the door opens onto
                -- the street; no corner more than 3 above or below it, none in water.
                local mid = along + (w - 1) // 2
                local floor = core.terrain_height(a.sx + a.ux * mid + px * 2, a.sz + a.uz * mid + pz * 2)
                if ok then
                    for _, c in { { rect.x0, rect.z0 }, { rect.x1, rect.z0 }, { rect.x0, rect.z1 }, { rect.x1, rect.z1 } } do
                        local g = core.terrain_height(c[1], c[2])
                        if g < SEA or wet(c[1], c[2]) or math.abs(g - floor) > 3 then
                            ok = false
                        end
                    end
                end

                if ok and floor > SEA then
                    table.insert(plan.buildings, { kind = kind, x0 = rect.x0, z0 = rect.z0, w = w, d = d,
                                                   t = turn_of(-px, -pz), floor = floor, h = r, ver = ver,
                                                   mark = mark })
                    if mark ~= nil then
                        table.remove(homes, 1)
                    end
                    barracks = barracks or kind == "barracks"
                    table.insert(taken, wide)
                    smithy = smithy or kind == "smithy"
                    tower = tower or kind == "tower"
                    mine = mine or kind == "mine"
                    hall = hall or kind == "hall"
                    along += w
                    -- A lamp in the gap, on the doorstep row.
                    local lx, lz = a.sx + a.ux * along + px * 2, a.sz + a.uz * along + pz * 2
                    local free = true
                    for _, st in plan.streets do
                        if lx >= st.x0 and lx <= st.x1 and lz >= st.z0 and lz <= st.z1 then
                            free = false
                        end
                    end
                    if free and along % 2 == 0 then
                        table.insert(plan.lamps, { lx, lz })
                    end
                    along += 2
                else
                    along += 2
                end
            end
        end
    end

    -- Every cell of the streets and the square, at heights carved to walk.
    for _, st in plan.streets do
        for z = st.z0, st.z1 do
            for x = st.x0, st.x1 do
                plan.paved[`{x},{z}`] = { x, z }
            end
        end
    end
    carve(plan.paved)

    core.log(`villages: {if v.town then "town" else "village"} at {v.x} {v.y} {v.z}, `
        .. `{#plan.buildings} buildings, {#arms} streets, {#plan.lamps} lamps`)
    PLAN[key] = plan

    local x1, z1, x2, z2 = v.x, v.z, v.x, v.z
    for _, st in plan.streets do
        x1, z1, x2, z2 = math.min(x1, st.x0), math.min(z1, st.z0), math.max(x2, st.x1), math.max(z2, st.z1)
    end
    for _, b in plan.buildings do
        local wide = b.t % 2 == 0
        x1, z1 = math.min(x1, b.x0), math.min(z1, b.z0)
        x2 = math.max(x2, b.x0 + (if wide then b.w else b.d) - 1)
        z2 = math.max(z2, b.z0 + (if wide then b.d else b.w) - 1)
    end
    if not told_extent[key] then
        told_extent[key] = true
        tell_extent(v.rx, v.rz, x1, z1, x2, z2)
        if ready then
            core.storage_set(`extent {key}`, `{x1},{z1},{x2},{z2}`)
            local all = core.storage_get("extents")
            if not string.find(`;{all};`, `;{key};`, 1, true) then
                core.storage_set("extents", if all == "" then key else `{all};{key}`)
            end
        end
    end
    return plan
end

-- ------------------------------------------------------------ paths

-- A path: the points it turns at, first and last where it leaves the two
-- settlements, and every cell it paves as { x, z, height }.
type Path = { points: { { number } }, cells: { [string]: { number } } }


local PATH: { [string]: Path | false } = {}

-- Where a path comes into a settlement: past the end of its street that is
-- nearest the other one, so it joins a street rather than a back wall. The
-- square if no street left it.
local function door_of(v: Village, other: Village, seed: number): (number, number)
    local best_x, best_z, best_d = v.x, v.z, math.huge
    for _, t in plan_of(v, seed).tips do
        local d = (t[1] - other.x) ^ 2 + (t[2] - other.z) ^ 2
        if d < best_d then
            best_x, best_z, best_d = t[1], t[2], d
        end
    end
    return best_x, best_z
end

-- The path between two settlements, found on a grid of 8 by 8 cells over the
-- box round the two ends (96 more every way, which keeps it within a region
-- of the squares): the cheapest way by A*, where a step costs its length, more
-- for climbing, and more again over water (a bridge), and a cell of the peaks,
-- the sea or the ocean cannot be crossed. nil if they wall one square off from
-- the other.
local CELL = 8
local MARGIN = 96

local function path_between(a: Village, b: Village, seed: number): Path?
    if a.rx > b.rx or (a.rx == b.rx and a.rz > b.rz) then
        a, b = b, a
    end
    local key = `{a.rx},{a.rz}:{b.rx},{b.rz}`
    local cached = PATH[key]
    if cached ~= nil then
        return if cached then cached else nil
    end

    local ax, az = door_of(a, b, seed)
    local bx, bz = door_of(b, a, seed)
    local x0 = math.min(ax, bx) - MARGIN
    local z0 = math.min(az, bz) - MARGIN
    local nx = (math.max(ax, bx) + MARGIN - x0) // CELL + 1
    local nz = (math.max(az, bz) + MARGIN - z0) // CELL + 1

    local height: { [number]: number } = {}   -- false for the peaks, by cell index
    local damp: { [number]: boolean } = {}     -- under water: a bridge
    local function ground(i: number): number
        local g = height[i]
        if g == nil then
            local x, z = x0 + (i % nx) * CELL + CELL // 2, z0 + (i // nx) * CELL + CELL // 2
            damp[i] = wet(x, z)
            local b = core.biome(x, z)
            g = if b == "core:peaks" or b == "core:ocean" or b == "core:sea" then -1 else core.terrain_height(x, z)
            if b == "core:gorge" then
                g, damp[i] = rim(x, z), true   -- a bridge across it
            end
            height[i] = g
        end
        return g
    end
    local function cell(x: number, z: number): number
        return ((z - z0) // CELL) * nx + (x - x0) // CELL
    end

    local from, to = cell(ax, az), cell(bx, bz)
    local tx, tz = to % nx, to // nx
    local function guess(i: number): number
        return math.sqrt((i % nx - tx) ^ 2 + (i // nx - tz) ^ 2)
    end

    -- A binary heap of { f, cell }.
    local heap: { { number } } = {}
    local function push(f: number, i: number)
        table.insert(heap, { f, i })
        local k = #heap
        while k > 1 and heap[k // 2][1] > heap[k][1] do
            heap[k], heap[k // 2] = heap[k // 2], heap[k]
            k //= 2
        end
    end
    local function pop(): { number }
        local top = heap[1]
        heap[1] = heap[#heap]
        table.remove(heap)
        local k = 1
        while true do
            local l, r, m = 2 * k, 2 * k + 1, k
            if l <= #heap and heap[l][1] < heap[m][1] then m = l end
            if r <= #heap and heap[r][1] < heap[m][1] then m = r end
            if m == k then break end
            heap[k], heap[m] = heap[m], heap[k]
            k = m
        end
        return top
    end

    local cost: { [number]: number } = { [from] = 0 }
    local came: { [number]: number } = {}
    push(guess(from), from)
    while #heap > 0 do
        local top = pop()
        local i = top[2]
        if i == to then
            break
        end
        local ix, iz = i % nx, i // nx
        local gi = ground(i)
        for dz = -1, 1 do
            for dx = -1, 1 do
                local jx, jz = ix + dx, iz + dz
                if (dx ~= 0 or dz ~= 0) and jx >= 0 and jz >= 0 and jx < nx and jz < nz then
                    local j = jz * nx + jx
                    local gj = ground(j)
                    if gj >= 0 then
                        local step = if dx ~= 0 and dz ~= 0 then 1.414 else 1.0
                        local c = cost[i] + step + math.abs(gj - gi) ^ 2 * 0.25 + (if damp[j] then 3 else 0)
                        if c < (cost[j] or math.huge) then
                            cost[j] = c
                            came[j] = i
                            push(c + guess(j), j)
                        end
                    end
                end
            end
        end
    end

    local found: Path | false = false
    if cost[to] then
        local pts = { { bx, bz } }
        local i = came[to]
        while i ~= nil and i ~= from do
            table.insert(pts, 1, { x0 + (i % nx) * CELL + CELL // 2, z0 + (i // nx) * CELL + CELL // 2 })
            i = came[i]
        end
        table.insert(pts, 1, { ax, az })

        -- Two wide along the points, except inside a house of either end.
        local houses: { Rect } = {}
        for _, v in { a, b } do
            for _, h in plan_of(v, seed).buildings do
                local wide = h.t % 2 == 0
                table.insert(houses, { x0 = h.x0, z0 = h.z0, x1 = h.x0 + (if wide then h.w else h.d) - 1,
                                       z1 = h.z0 + (if wide then h.d else h.w) - 1 })
            end
        end
        local cells: { [string]: { number } } = {}
        for k = 1, #pts - 1 do
            local px, pz, qx, qz = pts[k][1], pts[k][2], pts[k + 1][1], pts[k + 1][2]
            local steps = math.ceil(math.sqrt((qx - px) ^ 2 + (qz - pz) ^ 2) * 2)
            for t = 0, steps do
                local x = math.floor(px + (qx - px) * t / steps)
                local z = math.floor(pz + (qz - pz) * t / steps)
                for _, c in { { x, z }, { x + 1, z }, { x, z + 1 } } do
                    local inside = false
                    for _, r in houses do
                        if c[1] >= r.x0 and c[1] <= r.x1 and c[2] >= r.z0 and c[2] <= r.z1 then
                            inside = true
                        end
                    end
                    if not inside then
                        cells[`{c[1]},{c[2]}`] = { c[1], c[2] }
                    end
                end
            end
        end
        carve(cells)
        found = { points = pts, cells = cells }
        core.log(`villages: path {a.x} {a.z} to {b.x} {b.z}, {#pts} points`)
    end
    PATH[key] = found
    return if found then found else nil
end

local LINK: { [string]: Path | false } = {}

-- A settlement's path: to the nearest one in the eight regions round it that a
-- path can reach without crossing the peaks.
local function link(v: Village, seed: number): Path?
    local key = `{v.rx},{v.rz}`
    local cached = LINK[key]
    if cached ~= nil then
        return if cached then cached else nil
    end
    local near: { Village } = {}
    for dz = -1, 1 do
        for dx = -1, 1 do
            local o = village_of(v.rx + dx, v.rz + dz, seed)
            if o and o ~= v then
                table.insert(near, o)
            end
        end
    end
    table.sort(near, function(a: Village, b: Village): boolean
        return (a.x - v.x) ^ 2 + (a.z - v.z) ^ 2 < (b.x - v.x) ^ 2 + (b.z - v.z) ^ 2
    end)
    local found: Path | false = false
    for _, o in near do
        local p = path_between(v, o, seed)
        if p then
            found = p
            break
        end
    end
    LINK[key] = found
    return if found then found else nil
end

-- ------------------------------------------------------------ building blocks

local function owns(ctx, x: number, z: number): boolean
    return x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z
end

-- What a way paved with a block climbs by: a stair and a slab of the same.
local STEPS_OF = {
    ["gravel:gravel"] = "gravel", ["rocks:cobblestone"] = "cobblestone",
    ["rocks:smooth_stone"] = "smooth_stone", ["rocks:coarse_dirt"] = "coarse_dirt", ["timber:planks"] = "oak",
}

-- A cell of a street or a path with its surface at h (see carve): its top
-- block paved, or planks on the water, the ground made up under it where
-- the way runs higher than the land, and the air above it cleared of grass,
-- trees and whatever ground was cut away. Where a neighbour in `set` is a
-- step higher, a stair of the way's own block climbs to it -- its high half
-- towards that neighbour -- or a slab where more than one way is higher.
local function fence(mask: number): number
    return id(if mask == 0 then "building:oak_fence" else `building:oak_fence_{mask}`)
end

-- Over water, or four and more over the ground (a gorge), a cell is a
-- bridge's: its deck of planks, the water or the air left under it but for a
-- log down to the ground now and then, and on each side of it that is no part
-- of the way and as low, a fence on a plank of its own.
local function low(x: number, z: number, h: number): boolean
    return wet(x, z) or h - core.terrain_height(x, z) >= 4
end
local function rail(set: { [string]: { number } }, x: number, z: number, h: number): boolean
    if set[`{x},{z}`] or not low(x, z, h) then
        return false
    end
    for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
        local n = set[`{x + d[1]},{z + d[2]}`]
        if n and low(n[1], n[2], n[3]) then
            return true
        end
    end
    return false
end

local function pave(ctx, x: number, z: number, block: number, h: number, set: { [string]: { number } })
    local g = core.terrain_height(x, z)
    local bridge = low(x, z, h)
    if bridge then
        block = id("timber:planks")   -- a bridge
        for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
            local rx, rz = x + d[1], z + d[2]
            if owns(ctx, rx, rz) and rail(set, rx, rz, h) then
                local mask = 0
                for bitv, o in { [1] = { 1, 0 }, [2] = { -1, 0 }, [4] = { 0, 1 }, [8] = { 0, -1 } } do
                    if rail(set, rx + o[1], rz + o[2], h) then
                        mask += bitv
                    end
                end
                core.set_block(rx, h - 1, rz, block)
                core.set_block(rx, h, rz, fence(mask))
                for y = h + 1, h + 4 do
                    core.set_block(rx, y, rz, 0)
                end
            end
        end
    end
    if not owns(ctx, x, z) then
        return
    end
    if not bridge then
        for y = g, h - 2 do
            core.set_block(x, y, z, block)
        end
    elseif hash(x, z, 0xB41D6E) % 5 == 0 then
        for y = g, h - 2 do
            core.set_block(x, y, z, id("timber:log"))   -- a pier
        end
    end
    core.set_block(x, h - 1, z, block)
    for y = h, math.max(g, h) + 8 do
        core.set_block(x, y, z, 0)
    end

    local up: { { number } } = {}
    for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
        local n = set[`{x + d[1]},{z + d[2]}`]
        if n and n[3] == h + 1 then
            table.insert(up, d)
        end
    end
    local kind = STEPS_OF[core.node_name(block)]
    if #up == 0 or kind == nil then
        return
    end
    if #up == 1 and id(`steps:{kind}_stair`) ~= 0 then
        core.set_block(x, h, z, id(`steps:{kind}_stair`))
        core.set_facing(x, h, z, turn_of(up[1][1], up[1][2]))
    elseif id(`steps:{kind}_slab`) ~= 0 then
        core.set_block(x, h, z, id(`steps:{kind}_slab`))
    end
end

-- A cell of a building's own frame in the world.
local function at(b: Building, lx: number, lz: number): (number, number)
    if b.t == 0 then
        return b.x0 + lx, b.z0 + lz
    elseif b.t == 1 then
        return b.x0 + lz, b.z0 + b.w - 1 - lx
    elseif b.t == 2 then
        return b.x0 + b.w - 1 - lx, b.z0 + b.d - 1 - lz
    end
    return b.x0 + b.d - 1 - lz, b.z0 + lx
end

type Put = (lx: number, y: number, lz: number, block: number, facing: number?) -> ()

local function putter(ctx, b: Building): Put
    return function(lx: number, y: number, lz: number, block: number, facing: number?)
        local x, z = at(b, lx, lz)
        if owns(ctx, x, z) then
            core.set_block(x, y, z, block)
            if facing then
                -- A wallmounted block's byte is the face it looks out of
                -- (timber:rungs), not a turn: the front, +Z unturned, goes to
                -- +Z, +X, -Z, -X.
                local t = (facing + b.t) % 4
                core.set_facing(x, y, z, if core.node_turns(block) == 6 then ({ [0] = 4, 0, 5, 1 })[t] else t)
            end
        end
    end
end

-- The ground under a footprint levelled to the floor: a foundation where the
-- ground is lower, the hill taken away where it is higher, and air up to `top`.
local function level(ctx, b: Building, ring: number, inside: number, top: number)
    for lz = 0, b.d - 1 do
        for lx = 0, b.w - 1 do
            local x, z = at(b, lx, lz)
            if owns(ctx, x, z) then
                local g = core.terrain_height(x, z)
                for y = g - 1, b.floor - 2 do
                    core.set_block(x, y, z, ring)
                end
                local edge = lx == 0 or lz == 0 or lx == b.w - 1 or lz == b.d - 1
                core.set_block(x, b.floor - 1, z, if edge then ring else inside)
                for y = b.floor, top do
                    core.set_block(x, y, z, 0)
                end
            end
        end
    end
end

-- What a building is made of, by where it stands: walls, corners, floor,
-- foundation, and the stairs and the ridge of its roof.
type Stuff = { wall: number, corner: number, floor: number, base: number, roof: number, ridge: number }

local function stuff_of(v: Village, b: Building): Stuff
    if v.desert then
        return { wall = id("rocks:sandstone"), corner = id("rocks:smooth_stone"), floor = id("rocks:sandstone"),
                 base = id("rocks:sandstone"), roof = id("steps:sandstone_stair"), ridge = id("rocks:smooth_stone") }
    elseif b.kind == "smithy" or b.kind == "tower" or (v.town and b.h % 3 == 0) then
        return { wall = id("rocks:stone_bricks"), corner = id("rocks:cobblestone"), floor = id("timber:planks"),
                 base = id("rocks:cobblestone"), roof = id("steps:stair"), ridge = id("rocks:stone_bricks") }
    elseif v.town and b.h % 3 == 1 then
        return { wall = id("craft:brick"), corner = id("timber:log"), floor = id("timber:planks"),
                 base = id("rocks:cobblestone"), roof = id("steps:dark_oak_stair"), ridge = id("woods:dark_oak_planks") }
    elseif v.cold then
        return { wall = id("woods:spruce_planks"), corner = id("woods:spruce_log"), floor = id("woods:spruce_planks"),
                 base = id("rocks:cobblestone"), roof = id("steps:dark_oak_stair"), ridge = id("woods:dark_oak_planks") }
    end
    return { wall = id("timber:planks"), corner = id("timber:log"), floor = id("timber:planks"),
             base = id("rocks:cobblestone"), roof = id("steps:spruce_stair"), ridge = id("woods:spruce_planks") }
end

-- Four walls `tall` high with the door in the middle of the front and a window
-- on every side, every fourth row.
local function walls(b: Building, put: Put, s: Stuff, tall: number)
    local pane_front = id(if b.t % 2 == 0 then "building:glass_pane_3" else "building:glass_pane_12")
    local pane_side = id(if b.t % 2 == 0 then "building:glass_pane_12" else "building:glass_pane_3")
    local door_x = b.w // 2
    for lz = 0, b.d - 1 do
        for lx = 0, b.w - 1 do
            local front = lz == 0 or lz == b.d - 1
            local side = lx == 0 or lx == b.w - 1
            if front or side then
                for y = b.floor, b.floor + tall - 1 do
                    local row = (y - b.floor) % 4
                    local block = if front and side then s.corner else s.wall
                    if not (front and side) and row == 1 and y < b.floor + tall - 1 then
                        if front and (lx == 1 or lx == b.w - 2) and lx ~= door_x then
                            block = pane_front
                        elseif side and lz == b.d // 2 then
                            block = pane_side
                        end
                    end
                    put(lx, y, lz, block)
                end
            end
        end
    end
    put(door_x, b.floor, 0, id("doors:door"), 0)
    put(door_x, b.floor + 1, 0, id("doors:door_top"), 0)
end

-- A pitched roof of stairs, ridge along the street, gables filled with wall.
local function roof(b: Building, put: Put, s: Stuff, eave: number)
    local stair = s.roof
    local k = 0
    while true do
        local f, bk = -1 + k, b.d - k
        local y = eave + k
        if f > bk then
            break
        end
        for lx = 0, b.w - 1 do
            if f == bk then
                put(lx, y, f, s.ridge)
            else
                put(lx, y, f, stair, 2)
                put(lx, y, bk, stair, 0)
            end
        end
        if f < bk then
            for lz = f + 1, bk - 1 do
                if lz >= 0 and lz < b.d and y >= eave + 1 then
                    put(0, y, lz, s.wall)
                    put(b.w - 1, y, lz, s.wall)
                end
            end
        end
        k += 1
    end
end

-- What is in a smithy's chest: a smith's things. Nothing is put in a chest
-- that already holds some of it, as in mods/ruins.
local LOOT = { { "furnace:iron_ingot", 5 }, { "ore:coal", 8 }, { "farm:bread", 3 }, { "furnace:iron_pick", 1 },
               { "timber:stick", 6 }, { "furnace:gold_ingot", 2 } }

local function stock(x: number, y: number, z: number, h: number)
    core.container_make(x, y, z, 27)
    for i = 0, 2 + h % 3 do
        local pick = LOOT[1 + bit32.rshift(h, i * 3) % #LOOT]
        local item = core.item_id(pick[1])
        if item ~= 0 and core.container_count(x, y, z, item) == 0 then
            core.container_add(x, y, z, item, 1 + bit32.rshift(h, i * 2 + 1) % pick[2])
        end
    end
end

-- A chest is stocked once. The world file is read after the region round the
-- spawn is generated, so a chest made there cannot yet tell whether it was
-- stocked in an earlier run: it waits for core:world_ready, when the mod's
-- storage has been read, and every chest after that is decided at once.
-- One storage key a chest, some 25 bytes of the 4 MiB a mod may keep.
local waiting: { { number } } = {}

local function stock_once(x: number, y: number, z: number, h: number)
    local key = `chest {x} {y} {z}`
    if core.storage_get(key) ~= "" then
        return
    end
    stock(x, y, z, h)
    core.storage_set(key, "1")
    core.log(`villages: chest at {x} {y} {z} stocked`)
end

core.event.on("core:world_ready", function()
    ready = true
    local early: { Village } = {}
    for _, plan in PLAN do
        table.insert(early, plan.v)
    end
    table.clear(PLAN)   -- planned again, each by the plan it keeps
    table.clear(told_extent)
    for _, key in string.split(core.storage_get("extents"), ";") do
        local rx, rz = string.match(key, "^(-?%d+),(-?%d+)$")
        local a, b, c, d = string.match(core.storage_get(`extent {key}`), "^(-?%d+),(-?%d+),(-?%d+),(-?%d+)$")
        if rx and a then
            tell_extent(tonumber(rx) :: number, tonumber(rz) :: number, tonumber(a) :: number, tonumber(b) :: number,
                tonumber(c) :: number, tonumber(d) :: number)
        end
    end
    -- Those planned before the storage was read, kept now.
    for _, v in early do
        if SEED ~= nil then
            plan_of(v, SEED :: number)
        end
    end
    for _, c in waiting do
        stock_once(c[1], c[2], c[3], c[4])
    end
    waiting = {}
end)

local function build(ctx, v: Village, b: Building)
    local put = putter(ctx, b)
    local s = stuff_of(v, b)
    local F = b.floor

    if b.kind == "farm" then
        -- A fenced field of ripe wheat round a ditch of water, a gate to the street.
        level(ctx, b, s.base, id("farm:soil"), F + 3)
        local ring: { [string]: boolean } = {}
        for lz = 0, b.d - 1 do
            for lx = 0, b.w - 1 do
                if lx == 0 or lz == 0 or lx == b.w - 1 or lz == b.d - 1 then
                    local x, z = at(b, lx, lz)
                    ring[`{x},{z}`] = true
                end
            end
        end
        for lz = 0, b.d - 1 do
            for lx = 0, b.w - 1 do
                local x, z = at(b, lx, lz)
                if lx == 0 or lz == 0 or lx == b.w - 1 or lz == b.d - 1 then
                    put(lx, F - 1, lz, id("core:dirt"))
                    if lz == 0 and lx == b.w // 2 then
                        put(lx, F, lz, id("building:oak_fence_gate"), 0)
                    else
                        local mask = 0
                        for bitv, o in { [1] = { 1, 0 }, [2] = { -1, 0 }, [4] = { 0, 1 }, [8] = { 0, -1 } } do
                            if ring[`{x + o[1]},{z + o[2]}`] then
                                mask += bitv
                            end
                        end
                        put(lx, F, lz, fence(mask))
                    end
                elseif lz == b.d // 2 then
                    put(lx, F - 1, lz, id("lakes:water"))
                else
                    put(lx, F, lz, id("farm:wheat_3"))
                end
            end
        end
        return
    end

    if b.kind == "mine" then
        -- The village's mine: a headframe of logs and planks over a shaft of
        -- rungs going 14 down to a gallery cut in the rock, where its miners
        -- dig (mods/villagers finds it by the column of rungs), and a chest
        -- for what they bring up.
        local B = F - 14
        level(ctx, b, s.base, s.floor, F + 6)
        walls(b, put, s, 3)
        for lz = 0, b.d - 1 do
            for lx = 0, b.w - 1 do
                put(lx, F + 3, lz, s.floor)
            end
        end
        put(b.w - 2, F, 1, id("torches:torch"))
        put(1, F, 1, id("torches:torch"))
        -- The mine's chest, where its miners leave what they dig.
        local cx, cz = at(b, b.w - 2, b.d - 2)
        if owns(ctx, cx, cz) then
            core.set_block(cx, F, cz, id("chest:box"))
            core.set_facing(cx, F, cz, (2 + b.t) % 4)
            core.container_make(cx, F, cz, 27)
        end
        local sx, sz = b.w // 2, 4
        -- The gallery: 5 wide, 4 deep, 3 high, floored, lit, its back wall whole.
        for lz = 1, 4 do
            for lx = 1, b.w - 2 do
                put(lx, B - 1, lz, id("rocks:cobblestone"))
                for y = B, B + 2 do
                    put(lx, y, lz, 0)
                end
            end
        end
        for lx = 1, b.w - 2 do
            for y = B - 1, F - 1 do
                put(lx, y, 5, id("rocks:cobblestone"))
            end
        end
        put(1, B + 1, 2, id("torches:torch"))
        put(b.w - 2, B + 1, 2, id("torches:torch"))
        -- The shaft, a log behind its top rungs in the hut, rungs all the way.
        put(sx, F, 5, s.corner)
        put(sx, F + 1, 5, s.corner)
        for y = B, F + 1 do
            if y >= B + 3 and y < F then
                for _, o in { { -1, 0 }, { 1, 0 }, { 0, -1 } } do
                    put(sx + o[1], y, sz + o[2], id("rocks:cobblestone"))
                end
            end
            put(sx, y, sz, id("timber:rungs"), 2)
        end
        return
    end

    if b.kind == "hall" and (b.ver or 2) >= 3 then
        -- The village's hall: the mayor's. Two storeys, a ladder up the back
        -- corner; below, its bell by the door, a table and the chest; above,
        -- the mayor's bed and table.
        level(ctx, b, s.base, s.floor, F + 8 + b.d)
        walls(b, put, s, 8)
        roof(b, put, s, F + 7)
        local rx, rz = b.w - 2, b.d - 2
        for lz = 1, b.d - 2 do
            for lx = 1, b.w - 2 do
                if not (lx == rx and lz == rz) then
                    put(lx, F + 3, lz, s.floor)
                end
            end
        end
        for y = F, F + 4 do
            put(rx, y, rz, id("timber:rungs"), 2)
        end
        put(1, F, 1, id("villages:bell"))
        put(b.w // 2, F, b.d - 2, id("craft:table"))
        put(b.w // 2, F, b.d // 2, id("torches:torch"))
        put(1, F + 4, b.d - 3, id("bed:bed"), 2)
        put(1, F + 4, b.d - 2, id("bed:bed_head"), 2)
        put(3, F + 4, b.d - 2, id("craft:table"))
        put(b.w // 2, F + 4, 2, id("torches:torch"))
        local cx, cz = at(b, b.w - 2, 1)
        if owns(ctx, cx, cz) then
            core.set_block(cx, F, cz, id("chest:box"))
            core.set_facing(cx, F, cz, (3 + b.t) % 4)
            core.container_make(cx, F, cz, 27)
        end
        return
    end

    if b.kind == "hall" or b.kind == "barracks" then
        -- A village's hall of plan 2, every keeper's bed in it: three storeys,
        -- a ladder up the back corner, three beds on the ground and six on
        -- each floor above, no table, so none of them is a home; its bell by
        -- the door says whose they are. The barracks are its first two
        -- storeys, the watch's, with a banner for the bell.
        local S = if b.kind == "hall" then 3 else 2
        level(ctx, b, s.base, s.floor, F + 4 * S + b.d)
        walls(b, put, s, 4 * S)
        roof(b, put, s, F + 4 * S - 1)
        local rx, rz = b.w - 2, b.d - 2
        for k = 1, S - 1 do
            for lz = 1, b.d - 2 do
                for lx = 1, b.w - 2 do
                    if not (lx == rx and lz == rz) then
                        put(lx, F + 4 * k - 1, lz, s.floor)
                    end
                end
            end
        end
        for y = F, F + 4 * (S - 1) do
            put(rx, y, rz, id("timber:rungs"), 2)
        end
        for k = 0, S - 1 do
            local y = F + 4 * k
            for _, lx in { 1, 3, 5 } do
                put(lx, y, b.d - 3, id("bed:bed"), 2)
                put(lx, y, b.d - 2, id("bed:bed_head"), 2)
                if k > 0 then
                    put(lx, y, 2, id("bed:bed"), 0)
                    put(lx, y, 1, id("bed:bed_head"), 0)
                end
            end
            put(b.w // 2, y, b.d - 2, id("torches:torch"))
        end
        put(1, F, 1, id(if b.kind == "hall" then "villages:bell" else "villages:banner"))
        local cx, cz = at(b, b.w - 2, 1)
        if owns(ctx, cx, cz) then
            core.set_block(cx, F, cz, id("chest:box"))
            core.set_facing(cx, F, cz, (3 + b.t) % 4)
            core.container_make(cx, F, cz, 27)
        end
        return
    end

    -- Plan 3's smithy has the crafter's room over it.
    local room = b.kind == "smithy" and (b.ver or 2) >= 3
    local storeys = if b.kind == "tall" or room then 2 else 1
    local tall = if b.kind == "tower" then 12 else 4 * storeys
    level(ctx, b, s.base, s.floor, F + tall + b.d)
    walls(b, put, s, tall)

    if b.kind == "tower" then
        -- A flat top with battlements and a light that can be seen from afar.
        for lz = 0, b.d - 1 do
            for lx = 0, b.w - 1 do
                put(lx, F + tall, lz, s.wall)
                local edge = lx == 0 or lz == 0 or lx == b.w - 1 or lz == b.d - 1
                if edge and (lx + lz) % 2 == 0 then
                    put(lx, F + tall + 1, lz, s.wall)
                end
            end
        end
        put(b.w // 2, F + tall + 1, b.d // 2, id("torches:torch"))
        put(1, F, b.d - 2, id("torches:torch"))
        return
    end

    roof(b, put, s, F + tall - 1)

    -- Inside: a torch, and by kind a bed and a table, or a smith's corner.
    put(if b.kind == "house" then b.w - 2 else b.w // 2, F, if b.kind == "house" then b.d - 2 else b.d // 2,
        id("torches:torch"))
    if b.kind == "smithy" then
        put(1, F, b.d - 2, id("furnace:furnace"), 2)
        put(b.w - 2, F, 1, id("building:anvil"))
        if room then
            -- Up the ladder in the back corner, the crafter's bed, no table:
            -- the anvil under it says whose it is.
            local rx, rz = b.w - 2, b.d - 2
            for lz = 1, b.d - 2 do
                for lx = 1, b.w - 2 do
                    if not (lx == rx and lz == rz) then
                        put(lx, F + 3, lz, s.floor)
                    end
                end
            end
            for y = F, F + 4 do
                put(rx, y, rz, id("timber:rungs"), 2)
            end
            put(3, F + 4, b.d - 3, id("bed:bed"), 2)
            put(3, F + 4, b.d - 2, id("bed:bed_head"), 2)
            put(1, F + 4, 1, id("torches:torch"))
        end
        local x, z = at(b, 1, 1)
        if owns(ctx, x, z) then
            core.set_block(x, F, z, id("chest:box"))
            core.set_facing(x, F, z, (1 + b.t) % 4)
            core.container_make(x, F, z, 27)
            if ready then
                stock_once(x, F, z, b.h)
            else
                table.insert(waiting, { x, F, z, b.h })
            end
        end
    else
        -- The bed along the side wall, its head (its back, turned 2) at the
        -- back; a family's three side by side. Two storeys: a floor, a
        -- ladder up to it in the back corner, and up there two beds and a
        -- table of their own.
        put(1, F, b.d - 3, id("bed:bed"), 2)
        put(1, F, b.d - 2, id("bed:bed_head"), 2)
        if b.kind == "family" then
            for _, lx in { 3, 5 } do
                put(lx, F, b.d - 3, id("bed:bed"), 2)
                put(lx, F, b.d - 2, id("bed:bed_head"), 2)
            end
        elseif b.kind == "tall" then
            local rx, rz = b.w - 2, b.d - 2
            for lz = 1, b.d - 2 do
                for lx = 1, b.w - 2 do
                    if not (lx == rx and lz == rz) then
                        put(lx, F + 3, lz, s.floor)
                    end
                end
            end
            for y = F, F + 4 do
                put(rx, y, rz, id("timber:rungs"), 2)
            end
            for _, lx in { 1, 3 } do
                put(lx, F + 4, b.d - 3, id("bed:bed"), 2)
                put(lx, F + 4, b.d - 2, id("bed:bed_head"), 2)
            end
            put(1, F + 4, 1, id("craft:table"))
            put(b.w // 2, F + 4, b.d // 2, id("torches:torch"))
        end
        put(1, F, 1, id("craft:table"))
        -- A keeper's house: its sign of whose it is (mods/villagers).
        if b.mark ~= nil then
            put(b.w - 2, F, b.d - 3, id(b.mark))
        end
        -- And the chest of whoever lives here (mods/villagers keeps what it
        -- does not need in it), in the front corner across from the table,
        -- clear of the way in from the door.
        local cx, cz = at(b, b.w - 2, 1)
        if owns(ctx, cx, cz) then
            core.set_block(cx, F, cz, id("chest:box"))
            core.set_facing(cx, F, cz, (3 + b.t) % 4)
            core.container_make(cx, F, cz, 27)
        end
    end
end

-- ------------------------------------------------------------ the stage

-- What a tree is made of, which the village clears from round its streets and
-- houses. Its own column's cells only: they are there, the trees' stage ran.
local TREE: { [number]: boolean } = {}
local function trees_known()
    if next(TREE) then
        return
    end
    for _, name in { "trees:trunk", "trees:leaves" } do
        TREE[id(name)] = true
    end
    for _, w in { "birch", "spruce", "jungle", "acacia", "dark_oak" } do
        TREE[id(`woods:{w}_log`)] = true
        TREE[id(`woods:{w}_leaves`)] = true
    end
    TREE[0] = nil
end

-- The trees of one cell of this column taken away, trunk and crown.
local function fell(ctx, x: number, z: number)
    if not owns(ctx, x, z) then
        return
    end
    local g = core.terrain_height(x, z)
    for y = g, g + 16 do
        if TREE[core.get_block(x, y, z)] then
            core.set_block(x, y, z, 0)
        end
    end
end


core.register_mapgen("villages:build", function(ctx)
    if id("timber:planks") == 0 then
        return
    end
    trees_known()
    local seed = ctx.seed
    SEED = seed
    local rx, rz = math.floor(ctx.min_x / REGION), math.floor(ctx.min_z / REGION)

    -- Paths first, then the streets over them, then the buildings over both.
    local drawn: { [Path]: boolean } = {}
    local cells: { { number } } = {}
    local path_of_cell: { [{ number }]: { [string]: { number } } } = {}
    for dz = -2, 2 do
        for dx = -2, 2 do
            local v = village_of(rx + dx, rz + dz, seed)
            local p = if v then link(v, seed) else nil
            if p and not drawn[p] then
                drawn[p] = true
                for _, c in p.cells do
                    if c[1] >= ctx.min_x - 1 and c[1] <= ctx.max_x + 1 and c[2] >= ctx.min_z - 1
                        and c[2] <= ctx.max_z + 1 then
                        table.insert(cells, c)
                        path_of_cell[c] = p.cells
                    end
                end
            end
        end
    end

    -- A path is cleared of trees a block either side, as a street is further out.
    local felled: { [string]: boolean } = {}
    for _, c in cells do
        for dz = -1, 1 do
            for dx = -1, 1 do
                local key = `{c[1] + dx},{c[2] + dz}`
                if not felled[key] then
                    felled[key] = true
                    fell(ctx, c[1] + dx, c[2] + dz)
                end
            end
        end
    end
    local dirt = id("rocks:coarse_dirt")
    for _, c in cells do
        local p = path_of_cell[c] or {}
        pave(ctx, c[1], c[2], dirt, c[3], p)
    end

    local v = village_of(rx, rz, seed)
    if v == nil then
        return
    end
    local plan = plan_of(v, seed)
    local road = if v.town then id("rocks:cobblestone") elseif v.desert then id("rocks:smooth_stone") else id("gravel:gravel")

    -- Trees out of the way first: 3 round every street and building, so no
    -- spruce hangs its crown into the square or over a roof.
    local around: { Rect } = {}
    for _, st in plan.streets do
        table.insert(around, { x0 = st.x0 - 3, z0 = st.z0 - 3, x1 = st.x1 + 3, z1 = st.z1 + 3 })
    end
    for _, b in plan.buildings do
        local wide = b.t % 2 == 0
        table.insert(around, { x0 = b.x0 - 3, z0 = b.z0 - 3, x1 = b.x0 + (if wide then b.w else b.d) + 2,
                               z1 = b.z0 + (if wide then b.d else b.w) + 2 })
    end
    for _, r in around do
        for z = math.max(r.z0, ctx.min_z), math.min(r.z1, ctx.max_z) do
            for x = math.max(r.x0, ctx.min_x), math.min(r.x1, ctx.max_x) do
                fell(ctx, x, z)
            end
        end
    end

    for _, c in plan.paved do
        pave(ctx, c[1], c[2], road, c[3], plan.paved)
    end

    -- The well in the middle of the square.
    local wy = core.terrain_height(v.x, v.z)
    for dz = -1, 1 do
        for dx = -1, 1 do
            local x, z = v.x + dx, v.z + dz
            if owns(ctx, x, z) then
                local centre = dx == 0 and dz == 0
                for y = wy - 5, wy - 1 do
                    core.set_block(x, y, z, if centre and y > wy - 5 then id("lakes:water") else id("rocks:cobblestone"))
                end
                core.set_block(x, wy, z, if centre then 0 else id("rocks:cobblestone"))
                local post = dx ~= 0 and dz ~= 0
                core.set_block(x, wy + 1, z, if post then fence(0) else 0)
                core.set_block(x, wy + 2, z, if post then fence(0) else 0)
                core.set_block(x, wy + 3, z, id("steps:slab"))
            end
        end
    end

    for _, l in plan.lamps do
        if owns(ctx, l[1], l[2]) then
            local g = core.terrain_height(l[1], l[2])
            if g > SEA and not wet(l[1], l[2]) then
                core.set_block(l[1], g, l[2], fence(0))
                core.set_block(l[1], g + 1, l[2], fence(0))
                core.set_block(l[1], g + 2, l[2], id("torches:torch"))
            end
        end
    end

    for _, b in plan.buildings do
        local wide = b.t % 2 == 0
        local x1 = b.x0 + (if wide then b.w else b.d) - 1
        local z1 = b.z0 + (if wide then b.d else b.w) - 1
        if x1 + 1 >= ctx.min_x and b.x0 - 1 <= ctx.max_x and z1 + 1 >= ctx.min_z and b.z0 - 1 <= ctx.max_z then
            build(ctx, v, b)
        end
    end
end)

-- ------------------------------------------------------------ /paese

core.command("paese", "[città] -- takes you to the square of the nearest village or town, or of the nearest town")

core.event.on("core:command", function(ev)
    if ev.name ~= "paese" then
        return
    end
    local px, _, pz = core.player_pos(ev.player)
    if px == nil then
        return
    end
    local seed = SEED
    if seed == nil then
        return
    end
    local rx, rz = math.floor(px / REGION), math.floor(pz / REGION)
    local town = ev.args ~= ""
    local reach = if town then 8 else 4
    local best: Village? = nil
    local best_d = math.huge
    for dz = -reach, reach do
        for dx = -reach, reach do
            local v = village_of(rx + dx, rz + dz, seed)
            if v and (v.town or not town) then
                local d = (v.x - px) ^ 2 + (v.z - pz) ^ 2
                if d < best_d then
                    best, best_d = v, d
                end
            end
        end
    end
    if best == nil then
        core.chat(ev.player, `No {if town then "town" else "village"} within {reach} regions.`)
        return
    end
    core.player_place(ev.player, best.x + 3.5, core.terrain_height(best.x + 3, best.z + 3) + 0.5, best.z + 3.5)
    core.chat(ev.player, `{if best.town then "Town" else "Village"} at {best.x} {best.z}.`)
end)
