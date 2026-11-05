--!strict
-- villagers: who gathers: woodcutters, miners and diggers. Part of the mod (init.luau says the order).

local common = require("./common")
local types = require("./types")
type V = types.V
local act = common.act
local add = common.add
local AID = common.AID
local AIR = common.AIR
local beside = common.beside
local BRINGS = common.BRINGS
local deliver = common.deliver
local failed = common.failed
local free = common.free
local GATHER = common.GATHER
local ground_at = common.ground_at
local has_kind = common.has_kind
local id = common.id
local is_in = common.is_in
local LEAF_NAMES = common.LEAF_NAMES
local live = common.live
local LOG_NAMES = common.LOG_NAMES
local mat_of = common.mat_of
local MATS = common.MATS
local MINE_REACH = common.MINE_REACH
local mines = common.mines
local monster_in = common.monster_in
local NATURAL = common.NATURAL
local nearest = common.nearest
local pace = common.pace
local people = common.people
local reach = common.reach
local region_of = common.region_of
local SAID = common.SAID
local save_orders = common.save_orders
local sleeps_now = common.sleeps_now
local standable = common.standable
local stroll = common.stroll
local take = common.take
local to_treasury = common.to_treasury
local TOOLING = common.TOOLING
local tree_log = common.tree_log
local walk = common.walk
local wanted = common.wanted
local wanting = common.wanting
local watch = common.watch
local WORK = common.WORK

-- ------------------------------------------------------------ who gathers

-- Takes a load to the builder: what the order lacks of it, the rest kept.
local function bring(v: V, m: string)
    local rk = region_of(v.hx, v.hz)
    local b = watch[`{rk} builder 1`]
    if b ~= nil then
        reach(v, b.hx, b.hy, b.hz)
        act(v, "gather", "", 1.6, b.hx, b.hz)
    end
    local o = wanting(rk)
    if o == nil then
        return
    end
    local brought = 0
    for item, n in table.clone(v.inv) do
        if mat_of(item) == m then
            local given = deliver(o, m, n)
            take(v, item, given)
            brought += given
        end
    end
    save_orders(rk)
    core.log(`villagers: {rk}: a {v.kind} brought {brought} {SAID[m]} ({o.have[m]} of {o.need[m]})`)
end

local function carrying(v: V, m: string): number
    local n = 0
    for item, c in v.inv do
        if mat_of(item) == m then
            n += c
        end
    end
    return n
end

-- Whether there is something to bring, and how much makes a load.
local function load_for(v: V, m: string): number?
    local _, lack = wanting(region_of(v.hx, v.hz))
    local n = lack[m]
    if n == nil then
        return nil
    end
    -- A gatherer's load; the builder, fetching for its own order, a stack.
    return math.min(n, if v.kind == "builder" then 64 else 12)
end

-- What a gatherer digs out; it keeps only what it gathers (a miner leaves
-- the earth over the stone), so it never carries enough to go and store it.
local GATHERS = { woodcutter = "wood", miner = "stone", digger = "earth" }
-- What a ladder hangs on: a rung beside it whose front points away from it
-- (wallmounted: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z). Taken away, the rung
-- falls -- and a miner who dug the wall behind its shaft's rungs had no way up.
local RUNG_NORMAL = { [0] = { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }
local function holds_rung(x: number, y: number, z: number): boolean
    local rungs = id("timber:rungs")
    for f = 0, 5 do
        local n = RUNG_NORMAL[f]
        if core.get_block(x + n[1], y + n[2], z + n[3]) == rungs and core.get_facing(x + n[1], y + n[2], z + n[3]) == f then
            return true
        end
    end
    return false
end

local function dig_cell(v: V, x: number, y: number, z: number)
    if holds_rung(x, y, z) then
        return
    end
    local was = core.get_block(x, y, z)
    if was ~= AIR and core.node_solid(was) == 1 then
        core.set_block(x, y, z, AIR)
        local item = core.node_yield(was)
        if mat_of(item) == (v.task or GATHERS[v.kind]) or (v.kind == "miner" and item:sub(1, 4) == "ore:") then
            add(v, item, 1)   -- a miner keeps the ore it meets, for the mine's chest
        end
    end
end

local function chop(v: V)
    local m = "wood"
    local load = load_for(v, m)
    if load == nil then
        stroll(v)
        return
    end
    if carrying(v, m) >= load then
        bring(v, m)
        return
    end
    local x, y, z = nearest(LOG_NAMES, v.hx, v.hy, v.hz, 32, 10, function(a, b, c)
        return failed(v, a, b, c) or is_in("logs", LOG_NAMES, core.get_block(a, b - 1, c))
            or core.node_solid(core.get_block(a, b - 1, c)) ~= 1 or not tree_log(a, b, c)
    end)
    if x == nil or y == nil or z == nil then
        stroll(v)
        return
    end
    if not reach(v, x, y, z) then
        return
    end
    act(v, "chop", TOOLING.of(v) or "tools:axe", 3.6 * pace(v), x, z)
    -- The tree comes down whole, crown and all, as a robot fells it.
    local top = y
    while top < y + 24 and is_in("logs", LOG_NAMES, core.get_block(x, top, z)) do
        dig_cell(v, x, top, z)
        top += 1
    end
    for ly = top - 4, top + 2 do
        for lz = z - 3, z + 3 do
            for lx = x - 3, x + 3 do
                if is_in("leaves", LEAF_NAMES, core.get_block(lx, ly, lz)) then
                    core.set_block(lx, ly, lz, AIR)
                end
            end
        end
    end
    -- And a sapling where it stood: only where the trunk did come down,
    -- on the world's own earth.
    local soil = core.node_name(core.get_block(x, y - 1, z))
    if id("trees:sapling") ~= 0 and core.get_block(x, y, z) == AIR
        and (soil == "core:grass" or soil == "core:dirt" or soil == "rocks:podzol" or soil == "core:snow") then
        core.set_block(x, y, z, id("trees:sapling"))
    end
end

-- Still in the village, with nothing to dig there: eight cells further out,
-- away from its middle (a creature's way gives up after 600 cells).
local function out_of_village(v: V)
    local px, _, pz = core.mob_pos(v.id)
    local fx, fz = math.floor(px or v.hx), math.floor(pz or v.hz)
    local b = common.villages[region_of(v.hx, v.hz)]
    local mx, mz = if b then (b.x1 + b.x2) / 2 else v.hx, if b then (b.z1 + b.z2) / 2 else v.hz
    local ox, oz = fx - mx, fz - mz
    local l = math.max(1, math.sqrt(ox * ox + oz * oz))
    local tx, tz = math.floor(fx + ox / l * 8), math.floor(fz + oz / l * 8)
    local g = ground_at(tx, v.hy, tz)
    if g ~= nil and standable(tx, g + 1, tz) then
        walk(v, tx, g + 1, tz, 20)
    else
        stroll(v)
    end
end

-- A mine's shaft made climbable again: every cell of it from the bottom to
-- the top a rung, each with solid rock behind it, on the wall most of its
-- rungs are on (or the one with the most rock behind). What a miner does
-- before it goes down, and when it finds itself at the bottom with no way up
-- -- a wall dug away, rungs fallen. Cobblestone from its bag if it has some.
local function mend_shaft(v: V, mn: { number })
    local x, z = mn[1], mn[3]
    local rungs, cobble = id("timber:rungs"), id("rocks:cobblestone")
    local votes = { [0] = 0, 0, 0, 0, 0, 0 }
    for y = mn[2], mn[4] do
        if core.get_block(x, y, z) == rungs then
            local f = core.get_facing(x, y, z)
            votes[f] = (votes[f] or 0) + 2
        end
        for _, f in { 0, 1, 4, 5 } do
            local n = RUNG_NORMAL[f]
            if core.node_solid(core.get_block(x - n[1], y, z - n[3])) == 1 then
                votes[f] += 1
            end
        end
    end
    local face, most = 5, -1
    for _, f in { 0, 1, 4, 5 } do
        if votes[f] > most then
            face, most = f, votes[f]
        end
    end
    local n = RUNG_NORMAL[face]
    local mended = 0
    -- Not walled in: at the shaft, it stands in the middle of it first, out
    -- of the cells the wall goes back into.
    local px, py, pz = core.mob_pos(v.id)
    if px and py and pz and math.abs(px - (x + 0.5)) < 1.5 and math.abs(pz - (z + 0.5)) < 1.5
        and py > mn[2] - 1 and py < mn[4] + 1 then
        core.mob_place(v.id, x + 0.5, math.floor(py + 0.01) + 0.02, z + 0.5)
    end
    for y = mn[2], mn[4] do
        local wx, wz = x - n[1], z - n[3]
        local w = core.get_block(wx, y, wz)
        if w == rungs or core.node_solid(w) ~= 1 then
            core.set_block(wx, y, wz, cobble)
            take(v, "rocks:cobblestone", 1)
            mended += 1
        end
        if core.get_block(x, y, z) ~= rungs or core.get_facing(x, y, z) ~= face then
            core.set_block(x, y, z, rungs)
            core.set_facing(x, y, z, face)
            mended += 1
        end
    end
    -- Somebody else in a gallery the mended wall closed: no way left to the
    -- shaft's foot from where it stands, so it is put there.
    if mended > 0 then
        for _, w in people do
            local other = w.id
            local ox, oy, oz = core.mob_pos(other)
            if other ~= v.id and other ~= 0 and ox and oy and oz and oy > mn[2] - 1.5 and oy < mn[2] + 3
                and math.abs(ox - x) < MINE_REACH + 4 and math.abs(oz - z) < MINE_REACH + 4 then
                local way = core.find_path(math.floor(ox), math.floor(oy + 0.01), math.floor(oz), x, mn[2], z,
                                           2, 1, 3, true, 2000)
                local n3 = #way
                if n3 < 3 or way[n3 - 2] ~= x or way[n3] ~= z or math.abs(way[n3 - 1] - mn[2]) > 1 then
                    if core.mob_place(other, x + 0.5, mn[2] + 0.02, z + 0.5) == 1 then
                        core.log(`villagers: {v.key} let creature {other} out of a walled gallery`)
                    end
                end
            end
        end
        act(v, "mine", TOOLING.of(v) or "tools:pick", 0.25 * mended, x, z)
        core.log(`villagers: {v.key} mended its mine's shaft at {x} {z}: {mended} block(s)`)
    end
end

-- In the village's mine: down the shaft, and galleries 2 high out from its
-- bottom, the 4 ways in turn, MINE_REACH long, then branches off each every
-- 4 cells, across; a torch every 6, a floor laid over a cave. When all are
-- dug, the quarry outside as before.
-- What a gallery does not go through, at the feet or the head: a wall a
-- ladder hangs on, something built, water or lava.
local function blocked(x: number, y: number, z: number): boolean
    for yy = y, y + 1 do
        local b = core.get_block(x, yy, z)
        local name = core.node_name(b)
        if holds_rung(x, yy, z) or name:sub(1, 6) == "lakes:" then
            return true
        end
        if b ~= AIR and core.node_solid(b) == 1 and not is_in("natural", NATURAL, b)
            and name:sub(1, 6) ~= "rocks:" and name:sub(1, 4) ~= "ore:" then
            return true
        end
    end
    return false
end

-- A cell of a gallery dug out, 2 high, with a floor under it.
local function dig_at(v: V, x: number, y: number, z: number)
    act(v, "mine", TOOLING.of(v) or "tools:pick", 2.4 * pace(v), x, z)
    dig_cell(v, x, y, z)
    dig_cell(v, x, y + 1, z)
    if core.node_solid(core.get_block(x, y - 1, z)) ~= 1 then
        core.set_block(x, y - 1, z, id("rocks:cobblestone"))
    end
end

-- Round what is in a gallery's way: from (sx, sz), where the miner stands,
-- going along (bx, bz), a cell aside, along past an obstacle 1 or 2 cells
-- long, and back onto the way. Which side first by who it is. How many cells
-- along the way it came out (2 or 3), or 0 when no such way round can be dug.
local function detour(v: V, sx: number, y: number, sz: number, bx: number, bz: number): number
    local sides = { { -bz, bx }, { bz, -bx } }
    if v.id % 2 == 1 then
        sides = { sides[2], sides[1] }
    end
    for long = 1, 2 do
        for _, p in sides do
            local cells = {}
            for k = 0, long + 1 do
                table.insert(cells, { sx + p[1] + bx * k, sz + p[2] + bz * k })
            end
            table.insert(cells, { sx + bx * (long + 1), sz + bz * (long + 1) })
            local clear = true
            for _, c in cells do
                if blocked(c[1], y, c[2]) then
                    clear = false
                    break
                end
            end
            if clear then
                for i, c in cells do
                    dig_at(v, c[1], y, c[2])
                    if i < #cells and not walk(v, c[1], y, c[2], 20) then
                        return 0
                    end
                end
                core.log(`villagers: {v.key} dug round something at {sx + bx} {y} {sz + bz}`)
                return long + 1
            end
        end
    end
    return 0
end

local galleries: { [string]: { number } } = {}   -- way (0..3), branch (0 the way itself), how far
local function dig_gallery(v: V, mn: { number }): boolean
    local x0, y, z0 = mn[1], mn[2], mn[3]
    local g = galleries[v.key] or { math.random(0, 3), 0, 1, 0 }   -- and how many ways done
    galleries[v.key] = g
    if g[3] > MINE_REACH - 2 then
        g[1], g[3], g[4] = (g[1] + 1) % 4, 1, g[4] + 1
        if g[4] % 4 == 0 then
            g[2] += 1
        end
        if g[2] > 3 then
            return false   -- all dug
        end
    end
    local dx, dz = ({ 1, 0, -1, 0 })[g[1] + 1], ({ 0, 1, 0, -1 })[g[1] + 1]
    -- A branch leaves the way at 4, 8, 12, turned a quarter.
    local ox, oz, bx, bz = x0, z0, dx, dz
    if g[2] > 0 then
        ox, oz = x0 + dx * 4 * g[2], z0 + dz * 4 * g[2]
        bx, bz = -dz, dx
    end
    local sx, sz = ox + bx * (g[3] - 1), oz + bz * (g[3] - 1)
    local nx, nz = ox + bx * g[3], oz + bz * g[3]
    local _, py = core.mob_pos(v.id)
    if py ~= nil and py > y + 3 then
        local hx, hy, hz = beside(x0, mn[4] - 1, z0, x0, z0)
        if hx == nil or hy == nil or hz == nil or not walk(v, hx, hy, hz, 40) then
            return true
        end
        -- Down the rungs on its own feet, off them at the bottom into the
        -- gallery; put there only if the walk does not get it there.
        -- Mended first: where it gets off is chosen in the shaft as it is.
        mend_shaft(v, mn)
        local fx, fy, fz = beside(x0, y, z0, x0 + dx, z0 + dz)
        fx, fy, fz = fx or x0, fy or y, fz or z0
        v.climbing = true
        local down = walk(v, fx, fy, fz, (mn[4] - y) * 0.6 + 10)
        v.climbing = false
        if not down then
            core.mob_place(v.id, fx + 0.5, fy + 0.02, fz + 0.5)
        end
    end
    if not (sx == x0 and sz == z0) and not walk(v, sx, y, sz, 40) then
        g[5] = (g[5] or 0) + 1
        if g[5] >= 3 then
            g[3], g[5] = MINE_REACH, 0   -- three times no way to it: the next
        end
        return true
    end
    g[5] = 0
    if blocked(nx, y, nz) then
        -- Round it, a cell to one side and back onto the way past it, when
        -- that much can be dug; otherwise this way ends here.
        local past = detour(v, sx, y, sz, bx, bz)
        if past == 0 then
            g[3] = MINE_REACH
        else
            g[3] += past
        end
        return true
    end
    dig_at(v, nx, y, nz)
    if g[3] % 6 == 0 and take(v, "torches:torch", 1) then
        core.set_block(nx, y + 1, nz, id("torches:torch"))
    end
    g[3] += 1
    return true
end

common.out_of_mine = function(v: V, to_y: number)
    local mn = mines[region_of(v.hx, v.hz)]
    local px, py, pz = core.mob_pos(v.id)
    if mn == nil or px == nil or py == nil or pz == nil or py > mn[4] - 5 or to_y < mn[4] - 5
        or math.abs(px - mn[1]) > MINE_REACH + 2 or math.abs(pz - mn[3]) > MINE_REACH + 2 then
        return
    end
    -- Back to the shaft, up its rungs, out beside it in the hut: walked,
    -- and put there only if the walk does not get it there.
    local hx, hy, hz = beside(mn[1], mn[4] - 1, mn[3], mn[1], mn[3])
    if hx == nil or hy == nil or hz == nil then
        return
    end
    -- Into the foot of the shaft first, then mended round it: mended from a
    -- gallery dug through its wall, it would wall itself in.
    v.climbing = true
    walk(v, mn[1], mn[2], mn[3], 30)
    v.climbing = false
    mend_shaft(v, mn)
    v.climbing = true
    local up = walk(v, hx, hy, hz, (mn[4] - py) * 0.6 + 15)
    v.climbing = false
    if not up then
        core.mob_place(v.id, hx + 0.5, hy + 0.02, hz + 0.5)
    end
end

-- Without a mine (a village made before they had one), a miner's quarry: a stair cut down into the ground away from the village,
-- a step a time (what it digs is cobblestone, and the earth above it), and a
-- new one in another way when it is 12 deep or meets what it may not dig.
local quarry: { [string]: { number } } = {}
-- What a miner carries up goes into the mine's chest: ore always, stone when
-- no order wants it. False when there is no chest or no room in it.
local function to_mine_chest(v: V, mn: { number }, stone_too: boolean): boolean
    local cx, cy, cz = nearest({ "chest:box" }, mn[1], mn[4] - 1, mn[3], 5, 2)
    if cx == nil or cy == nil or cz == nil or not reach(v, cx, cy, cz) then
        return false
    end
    act(v, "gather", "", 1.6, cx, cz)
    local room = true
    for item, n in table.clone(v.inv) do
        if item == "ore:ruby" then
            take(v, item, n)
            to_treasury(region_of(v.hx, v.hz), n)
        elseif item:sub(1, 4) == "ore:" or (stone_too and mat_of(item) == "stone") then
            local iid = core.item_id(item)
            local left = if iid ~= 0 then core.container_add(cx, cy, cz, iid, n) else n
            take(v, item, n)   -- what does not fit is left in the gallery
            room = room and left == 0
        end
    end
    return room
end

local function ore_carried(v: V): number
    local n = 0
    for item, c in v.inv do
        if item:sub(1, 4) == "ore:" then
            n += c
        end
    end
    return n
end

local chest_full: { [string]: number } = {}   -- rk: not mined for stock again before

local function mine(v: V)
    local m = "stone"
    local load = load_for(v, m)
    local rk = region_of(v.hx, v.hz)
    local mn = mines[rk]
    if load == nil then
        -- No order: with a mine, it digs for the village's stock all the same.
        if mn == nil or (chest_full[rk] or 0) > common.clock then
            stroll(v)
            return
        end
        if carrying(v, m) + ore_carried(v) >= 16 then
            if not to_mine_chest(v, mn, true) then
                chest_full[rk] = common.clock + 600
            end
            return
        end
    elseif carrying(v, m) >= load then
        bring(v, m)
        if mn ~= nil and ore_carried(v) > 0 then
            to_mine_chest(v, mn, false)
        end
        return
    end
    -- The galleries are the miners': anybody else sent for stone (the
    -- builder finding it itself) cuts a quarry outside -- a builder down the
    -- mine got stuck in it.
    if mn ~= nil and v.kind == "miner" and dig_gallery(v, mn) then
        return
    end
    local q = quarry[v.key]
    if q == nil or q[6] >= 12 then
        q = nil
        -- Near where it is (a creature's way gives up after 600 cells), and
        -- out of the village: a stroll out first when it is still in it.
        local px, _, pz = core.mob_pos(v.id)
        local fx, fz = math.floor(px or v.hx), math.floor(pz or v.hz)
        for _ = 1, 12 do
            local a = math.random(0, 3)
            local dx, dz = ({ 1, 0, -1, 0 })[a + 1], ({ 0, 1, 0, -1 })[a + 1]
            local r = 4 + math.random(0, 6)
            local sx, sz = fx + dx * r + math.random(-2, 2), fz + dz * r + math.random(-2, 2)
            local g = ground_at(sx, v.hy, sz)
            -- Open ground nobody owns, with room ahead to cut the stair into.
            if g ~= nil and core.owned_at(sx, sz) == 0 and standable(sx, g + 1, sz)
                and not failed(v, sx, g + 1, sz) and free(sx + dx, g + 1, sz + dz) then
                q = { sx, g + 1, sz, dx, dz, 0 }   -- where it stands, which way down, how deep
                break
            end
        end
        if q == nil then
            out_of_village(v)
            return
        end
        quarry[v.key] = q
    end
    local x, y, z, dx, dz = q[1], q[2], q[3], q[4], q[5]
    if not walk(v, x, y, z, 30) then
        quarry[v.key] = nil
        return
    end
    -- The next step: a cell along and one down, cleared three high.
    local nx, nz, ny = x + dx, z + dz, y - 1
    for yy = ny + 2, ny, -1 do
        local b = core.get_block(nx, yy, nz)
        if b ~= AIR and not (is_in("natural", NATURAL, b) or core.node_name(b):sub(1, 6) == "rocks:"
            or core.node_solid(b) ~= 1) then
            q[6] = 99   -- not the world's own ground: elsewhere
            return
        end
    end
    act(v, "mine", TOOLING.of(v) or "tools:pick", 2.4 * pace(v), nx, nz)
    for yy = ny + 2, ny, -1 do
        dig_cell(v, nx, yy, nz)
    end
    if core.node_solid(core.get_block(nx, ny - 1, nz)) ~= 1 then
        q[6] = 99
        return
    end
    q[1], q[2], q[3], q[6] = nx, ny, nz, q[6] + 1
end

-- A digger's pit: earth off the top of the ground, away from the village.
local function shovel(v: V)
    local m = "earth"
    local load = load_for(v, m)
    if load == nil then
        stroll(v)
        return
    end
    if carrying(v, m) >= load then
        bring(v, m)
        return
    end
    local px, _, pz = core.mob_pos(v.id)
    local fx, fz = px or v.hx, pz or v.hz
    for _ = 1, 8 do
        local a = math.random() * math.pi * 2
        local r = 3 + math.random() * 7
        local x, z = math.floor(fx + math.cos(a) * r), math.floor(fz + math.sin(a) * r)
        local g, gb = ground_at(x, v.hy, z)
        if g ~= nil and core.owned_at(x, z) == 0
            and (gb == id("core:grass") or gb == id("core:dirt")) then
            if reach(v, x, g, z) then
                act(v, "dig", TOOLING.of(v) or "tools:shovel", 2.0 * pace(v), x, z)
                dig_cell(v, x, g, z)
            end
            return
        end
    end
    out_of_village(v)
end

GATHER.wood, GATHER.stone, GATHER.earth = chop, mine, shovel

-- What a gatherer fetches: its own kind; but with the builder told to find
-- what it lacks itself and none of its own kind lacking, another the order
-- lacks -- one nobody in the village gathers first.
local function gather(v: V)
    local own = GATHERS[v.kind]
    local rk = region_of(v.hx, v.hz)
    local o, lack = wanting(rk)
    local m = own
    if o ~= nil and o.self and lack[own] == nil and next(lack) ~= nil then
        m = next(lack) :: string
        for _, k in MATS do
            if lack[k] ~= nil and not has_kind(rk, BRINGS[k]) then
                m = k
                break
            end
        end
    end
    v.task = m
    GATHER[m](v)
end
WORK.woodcutter, WORK.miner, WORK.digger = gather, gather, gather

-- One of them, a fifth of a second on.
local function step(v: V)
    if v.id == 0 then
        return
    end
    if core.mob_pos(v.id) == nil then
        -- Struck down: back in its bed in five minutes, empty handed.
        AID.ev.fell(v)
        v.id, v.co, v.inv, v.hunting, v.prey = 0, nil, {}, nil, nil
        v.back_at = common.clock + 300
        return
    end
    if v.kind == "guard" or v.kind == "archer" then
        -- A thief first, then whatever came out of the dark; the day's round
        -- again once neither is about.
        local px, py, pz = core.mob_pos(v.id)
        local p = wanted(v.rk :: string, px or v.hx, pz or v.hz)
        -- Asleep, only a thief wakes it: the other shift has the dark.
        local m = if p == nil and not sleeps_now(v) then monster_in(v.rk :: string, px or v.hx, py or v.hy, pz or v.hz)
            else nil
        if p ~= v.hunting or m ~= v.prey then
            core.mob_hunt(v.id, p or 0)
            core.mob_attack(v.id, m or 0)
            v.hunting, v.prey = p, m
            v.sleeping = false
            if p == nil and m == nil then
                v.co = coroutine.create(function() live(v) end)
                v.wake, v.walking = common.clock, false
            end
        end
        if p ~= nil or m ~= nil then
            return
        end
    end
    if v.co ~= nil and (common.clock >= v.wake or (v.walking and core.mob_going(v.id) == 0)) then
        local ok, err = coroutine.resume(v.co)
        if not ok then
            core.log(`villagers: {v.kind} at {v.key}: {err}`)
            v.co = coroutine.create(function() live(v) end)
        end
    end
end

-- What the files after this one use of it.
common.step = step
