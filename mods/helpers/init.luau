--!strict
-- helpers: little robots that work for whoever built them.
--
-- A robot is made at a crafting table (six iron, a cinnabar and a chest) and
-- put down with a right click on the ground. It is its maker's: a right click
-- on it with a tool in the hand gives it that tool -- an axe, a pick, a
-- shovel -- (the one it had comes back), and with anything else opens its
-- PROGRAM, a list of up to six orders it carries out one after the other,
-- round and round, about the place it was put down (its base, within 12):
--
--   Chop trees      with an axe: fells the nearest tree, trunk and crown,
--                   keeping the logs and a sapling or two
--   Plant saplings  puts the saplings it has on grass or dirt, apart
--   Dig soil        with a shovel: the dirt, grass, sand and gravel over its
--                   base's level, so the ground about it is levelled
--   Dig stone       with a pick: the same for stone and rock
--   Find ores       with a pick: tunnels to the nearest ore under and about
--                   it, steps down and all, and brings it out (gold, diamond,
--                   cinnabar and emerald want an iron pick); gravel or sand
--                   over a tunnel is propped with a cobblestone, a cave under
--                   it bridged with one, and it climbs out a cell at a time,
--                   a cobblestone under its feet, as a player gets out of a hole
--   Quarry          with a pick: a pit five by five just east of its base,
--                   a layer at a time, 24 deep at most, and round it a stair
--                   a step down for each layer -- the way back up, for the
--                   robot with its load and for whoever goes down after it
--   Sort chests     puts each thing in the chest about its base that has most
--                   of it already, and what it carries too
--   Cook food       takes raw food and fuel from the chests to the furnace,
--                   and what is cooked back to a chest
--   Light up        torches from the chests by its base on the dark ground
--                   up to 100 from it, nearest first, on a lattice that
--                   lights each cell once (where the ground allows; else
--                   where a torch lights the most still dark), so the
--                   creatures of the night stay away
--   Tend animals    shears the sheep about its base, the wool in its bag,
--                   and feeds two animals of a kind near each other (wheat
--                   or seeds from its bag or the chests) so they breed
--   Build my blueprint  its maker's laid-out blueprint (mods/blueprints),
--                   bottom up: what is in the way dug out, each block from
--                   its bag or the chests by its base, put with its turn;
--                   what it has none of it tells its maker, once a minute
--   Follow me       walks after its maker, and does nothing else
--
-- The world anchor (iron, cinnabar and a diamond), given like a tool, keeps
-- the ground about the robot and about its base in memory with no player
-- near, so it works on far from everybody -- and comes back out after a
-- restart wherever it was left.
--
-- What it gathers it carries, and puts in the nearest chest by its base when
-- it has twenty-four things. When it cannot reach that chest, or finds itself
-- far from its base or under it, it goes home, digging its way if it must. "Pick up" on the panel gives the robot back as
-- an item, with what it carries and its tools. Struck down, it leaves all of
-- that where it fell.
--
-- The model is tools/make_villager_models.py's helper, on the robot's own
-- skeleton, so it chops, digs and mines with the robot's clips and its tools
-- show in its hand. Each robot is a coroutine that says its program in order
-- and yields while it walks or works; robots are kept in the mod's storage.

local MODEL = core.register_model("helpers:helper", "helper.gltf")
core.register_mob("helpers:helper", MODEL, 30, 2.6, "worker", 0, 1.6, 1.0, "", 1, 0, true)

local ITEM = "helpers:robot"
core.register_item(ITEM, 1, "", 0, 1.0, "")
core.recipe_shaped(ITEM, 1,
    "furnace:iron_ingot  ore:cinnabar  furnace:iron_ingot",
    "furnace:iron_ingot  chest:box     furnace:iron_ingot",
    "furnace:iron_ingot  -             furnace:iron_ingot")

core.register_item("helpers:anchor", 1, "", 0, 1.0, "")
core.recipe_shaped("helpers:anchor", 1,
    "furnace:iron_ingot  ore:cinnabar  furnace:iron_ingot",
    "ore:cinnabar        ore:diamond   ore:cinnabar",
    "furnace:iron_ingot  ore:cinnabar  furnace:iron_ingot")

local RANGE, FULL, STEPS = 12, 24, 6
local AIR = 0

-- ---------------------------------------------------------------- the orders

local ORDERS = {
    { key = "chop", text = "Chop trees", tool = "axe" },
    { key = "plant", text = "Plant saplings" },
    { key = "soil", text = "Dig soil", tool = "shovel" },
    { key = "stone", text = "Dig stone", tool = "pick" },
    { key = "ores", text = "Find ores", tool = "pick" },
    { key = "quarry", text = "Quarry", tool = "pick" },
    { key = "sort", text = "Sort chests" },
    { key = "cook", text = "Cook food" },
    { key = "light", text = "Light up" },
    { key = "tend", text = "Tend animals" },
    { key = "build", text = "Build my blueprint" },
    { key = "follow", text = "Follow me" },
}
local ORDER: { [string]: { key: string, text: string, tool: string? } } = {}
for _, o in ORDERS do
    ORDER[o.key] = o
end

-- Which tool an item is, for the robot's three hands.
local TOOL_OF = { ["tools:axe"] = "axe", ["tools:pick"] = "pick", ["furnace:iron_pick"] = "pick",
                  ["tools:shovel"] = "shovel", ["helpers:anchor"] = "anchor" }
-- And every material's (mods/tools, mods/furnace).
for _, k in { "pick", "axe", "shovel" } do
    TOOL_OF[`tools:wood_{k}`] = k
    for _, m in { "iron", "gold", "diamond" } do
        TOOL_OF[`furnace:{m}_{k}`] = k
    end
end

-- ---------------------------------------------------------------- the world

local LOGS = { "trees:trunk", "timber:log", "timber:mossy_log", "woods:birch_log", "woods:spruce_log",
               "woods:jungle_log", "woods:acacia_log", "woods:dark_oak_log" }
local LEAVES = { "trees:leaves", "woods:birch_leaves", "woods:spruce_leaves", "woods:jungle_leaves",
                 "woods:acacia_leaves", "woods:dark_oak_leaves" }
local SOIL = { "core:dirt", "core:grass", "core:sand", "core:snow", "gravel:gravel", "rocks:red_sand",
               "rocks:coarse_dirt", "rocks:podzol", "rocks:mycelium", "rocks:clay" }
local STONE = { "core:stone", "rocks:cobblestone", "rocks:mossy_cobblestone", "rocks:andesite",
                "rocks:diorite", "rocks:granite", "rocks:deepslate", "rocks:tuff", "rocks:calcite",
                "rocks:sandstone", "tools:granite" }
local ORES = { "ore:coal_ore", "ore:copper_ore", "ore:iron", "ore:lapis_ore", "ore:gold",
               "ore:cinnabar_ore", "ore:diamond_ore", "ore:emerald_ore" }
local IRON_ONLY = { ["ore:gold"] = true, ["ore:cinnabar_ore"] = true, ["ore:diamond_ore"] = true,
                    ["ore:emerald_ore"] = true }
local CHESTS = { "chest:box", "chest:double_l", "chest:double_r" }
local FURNACES = { "furnace:furnace", "furnace:lit" }
local RAW = { "pets:fish", "pets:carp", "pets:trout", "pets:cod", "pets:eel", "pets:tuna",
              "animals:beef", "animals:pork", "animals:egg" }
local COOKED = { "pets:cooked_fish", "animals:steak", "animals:cooked_pork", "animals:fried_egg" }
local FUEL = { "ore:coal", "furnace:charcoal", "timber:planks", "trees:trunk", "timber:log" }

local function set_of(names: { string }): { [number]: boolean }
    local s = {}
    for _, n in names do
        local i = core.node_id(n)
        if i ~= 0 then
            s[i] = true
        end
    end
    return s
end
local IS: { [string]: { [number]: boolean } } = {}
local function is(kind: string, names: { string }, id: number): boolean
    local s = IS[kind]
    if s == nil then
        s = set_of(names)
        IS[kind] = s
    end
    return s[id] == true
end

-- What a tunnel may go through: the ground as the world made it.
local function diggable(id: number): boolean
    return id == AIR or is("soil", SOIL, id) or is("stone", STONE, id) or is("ores", ORES, id)
        or core.node_solid(id) ~= 1 and core.node_name(id):sub(1, 7) == "plants:"
end

local function free(x: number, y: number, z: number): boolean
    local a = core.get_block(x, y, z)
    return core.node_solid(a) ~= 1 and core.node_name(a):sub(1, 6) ~= "lakes:" and core.node_name(a) ~= "rocks:lava"
end
local function standable(x: number, y: number, z: number): boolean
    return core.node_solid(core.get_block(x, y - 1, z)) == 1 and free(x, y, z) and free(x, y + 1, z)
end

local function beside(x: number, y: number, z: number, fx: number, fz: number): (number?, number?, number?)
    local best, bx, by, bz = math.huge, nil, nil, nil
    for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
        for _, sy in { y, y - 1, y + 1, y - 2 } do
            local cx, cz = x + d[1], z + d[2]
            if standable(cx, sy, cz) then
                local dd = (cx - fx) ^ 2 + (cz - fz) ^ 2 + math.abs(sy - y)
                if dd < best then
                    best, bx, by, bz = dd, cx, sy, cz
                end
            end
        end
    end
    return bx, by, bz
end

local function find(names: { string }, x: number, y0: number, y1: number, z: number, r: number,
                    keep: (number, number, number) -> boolean, fx: number, fy: number, fz: number)
    : (number?, number?, number?)
    local best, bx, by, bz = math.huge, nil, nil, nil
    for _, name in names do
        if core.node_id(name) ~= 0 then
            local found = core.find_nodes(x - r, y0, z - r, x + r, y1, z + r, name)
            for i = 1, #found, 3 do
                local cx, cy, cz = found[i], found[i + 1], found[i + 2]
                local d = (cx - fx) ^ 2 + (cy - fy) ^ 2 + (cz - fz) ^ 2
                if d < best and keep(cx, cy, cz) then
                    best, bx, by, bz = d, cx, cy, cz
                end
            end
        end
    end
    return bx, by, bz
end

-- ---------------------------------------------------------------- the robots

type R = {
    n: number, owner: string, id: number,
    bx: number, by: number, bz: number,           -- its base
    lx: number, ly: number, lz: number,           -- where it was last seen
    program: { string }, running: boolean, step: number,
    inv: { [string]: number }, tools: { [string]: string },
    co: thread?, wake: number, walking: boolean,
    tried: { [string]: number }, said_full: boolean,
    ring: number?, said_dark: boolean?, kept: number?,
}

local robots: { [number]: R } = {}
local next_n = 1
local clock = 0.0
local dirty: { [number]: boolean } = {}

local function count(r: R): number
    local n = 0
    for _, c in r.inv do
        n += c
    end
    return n
end
local function add(r: R, item: string, n: number)
    if item ~= "" and n > 0 then
        r.inv[item] = (r.inv[item] or 0) + n
        dirty[r.n] = true
    end
end
local function take(r: R, item: string, n: number)
    r.inv[item] = (r.inv[item] or 0) - n
    if r.inv[item] <= 0 then
        r.inv[item] = nil
    end
    dirty[r.n] = true
end

local function wait(r: R, seconds: number)
    r.wake = clock + seconds
    coroutine.yield()
end

local function walk(r: R, x: number, y: number, z: number, limit: number?): boolean
    core.mob_act(r.id, "", "")
    if core.mob_goto(r.id, x, y, z) == 0 then
        return false
    end
    r.walking = true
    r.wake = clock + (limit or 30)
    coroutine.yield()
    r.walking = false
    local px, py, pz = core.mob_pos(r.id)
    if px == nil or py == nil or pz == nil then
        return false
    end
    local there = math.abs(px - (x + 0.5)) < 0.8 and math.abs(pz - (z + 0.5)) < 0.8 and math.abs(py - y) < 1.2
    if not there then
        r.tried[`{x},{y},{z}`] = clock + 90
        core.mob_goto(r.id, math.floor(px), math.floor(py + 0.01), math.floor(pz))
    end
    return there
end

local function failed(r: R, x: number, y: number, z: number): boolean
    return (r.tried[`{x},{y},{z}`] or 0) > clock
end

local function reach(r: R, x: number, y: number, z: number): boolean
    if failed(r, x, y, z) then
        return false
    end
    local px, _, pz = core.mob_pos(r.id)
    local sx, sy, sz = beside(x, y, z, px or x, pz or z)
    if sx == nil or sy == nil or sz == nil then
        r.tried[`{x},{y},{z}`] = clock + 90
        return false
    end
    local ok = walk(r, sx, sy, sz)
    if not ok then
        r.tried[`{x},{y},{z}`] = clock + 90
    end
    return ok
end

local function act(r: R, clip: string, item: string, seconds: number, fx: number, fz: number)
    core.mob_face(r.id, fx + 0.5, fz + 0.5)
    core.mob_act(r.id, clip, item)
    wait(r, seconds)
    core.mob_act(r.id, "", "")
end

-- A cell broken as a hand would break it: what it gives goes in the robot.
local function dig(r: R, x: number, y: number, z: number)
    local was = core.get_block(x, y, z)
    if was == AIR then
        return
    end
    core.set_block(x, y, z, AIR)
    add(r, core.node_yield(was), 1)
end

-- ---------------------------------------------------------------- chests

local function chests(r: R): { { number } }
    local out = {}
    for _, name in CHESTS do
        local found = core.find_nodes(r.bx - RANGE, r.by - 4, r.bz - RANGE, r.bx + RANGE, r.by + 4,
                                      r.bz + RANGE, name)
        for i = 1, #found, 3 do
            table.insert(out, { found[i], found[i + 1], found[i + 2] })
        end
    end
    table.sort(out, function(a, b)
        return a[1] * 1e6 + a[3] * 1e3 + a[2] < b[1] * 1e6 + b[3] * 1e3 + b[2]
    end)
    return out
end

-- The chest by the base that has most of an item; the first one with room
-- when none has any.
local function best_chest(list: { { number } }, iid: number): { number }?
    local best, most = nil, 0
    for _, c in list do
        local n = core.container_count(c[1], c[2], c[3], iid)
        if n > most then
            best, most = c, n
        end
    end
    if best == nil then
        for _, c in list do
            if core.container_fill(c[1], c[2], c[3]) < 1 then
                return c
            end
        end
    end
    return best
end

-- What it carries, into the chests by its base. True when it emptied.
local function unload(r: R): boolean
    if next(r.inv) == nil then
        return true
    end
    local list = chests(r)
    if #list == 0 then
        return false
    end
    for item, n in table.clone(r.inv) do
        local iid = core.item_id(item)
        local c = if iid ~= 0 then best_chest(list, iid) else nil
        if c ~= nil and reach(r, c[1], c[2], c[3]) then
            act(r, "gather", "", 1.0, c[1], c[3])
            take(r, item, n - core.container_add(c[1], c[2], c[3], iid, n))
        end
    end
    return next(r.inv) == nil
end

-- Home to its base when it cannot reach its chests (below, after the work).
local go_home: (R) -> ()

-- When it carries a full load: emptied, or said once that it cannot be.
local function full(r: R): boolean
    if count(r) < FULL then
        return false
    end
    if unload(r) then
        r.said_full = false
        return false
    end
    if #chests(r) > 0 then
        go_home(r)
        if unload(r) then
            r.said_full = false
            return false
        end
    end
    if not r.said_full then
        r.said_full = true
        for _, p in core.players() do
            if core.player_name(p) == r.owner then
                core.notice(p, `Robot {r.n} is full and has no chest with room near its base`)
            end
        end
    end
    return true
end

-- ---------------------------------------------------------------- the work

-- Each returns true when it did something, false when there was nothing to do.
local WORK: { [string]: (R) -> boolean } = {}

local function where(r: R): (number, number, number)
    local x, y, z = core.mob_pos(r.id)
    return math.floor(x or r.bx), math.floor((y or r.by) + 0.01), math.floor(z or r.bz)
end

WORK.chop = function(r: R): boolean
    local px, py, pz = where(r)
    local x, y, z = find(LOGS, r.bx, r.by - 3, r.by + 6, r.bz, RANGE, function(x, y, z)
        return not failed(r, x, y, z) and not is("logs", LOGS, core.get_block(x, y - 1, z))
            and core.node_solid(core.get_block(x, y - 1, z)) == 1
    end, px, py, pz)
    if x == nil or y == nil or z == nil or not reach(r, x, y, z) then
        return false
    end
    act(r, "chop", r.tools.axe or "tools:axe", 3.6, x, z)
    -- The tree comes down whole: its trunk, and the crown about its top.
    local top = y
    while top < y + 24 and is("logs", LOGS, core.get_block(x, top, z)) do
        dig(r, x, top, z)
        top += 1
    end
    -- A trunk grown in the ground leaves a hole: filled, as the ground was.
    local around = 0
    for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
        if core.node_solid(core.get_block(x + d[1], y, z + d[2])) == 1 then
            around += 1
        end
    end
    if around >= 3 then
        core.set_block(x, y, z, core.node_id("core:dirt"))
    end
    local saplings = 0
    for ly = top - 4, top + 2 do
        for lz = z - 3, z + 3 do
            for lx = x - 3, x + 3 do
                if is("leaves", LEAVES, core.get_block(lx, ly, lz)) then
                    core.set_block(lx, ly, lz, AIR)
                    saplings += 1
                end
            end
        end
    end
    if core.node_id("trees:sapling") ~= 0 then
        add(r, "trees:sapling", math.min(2, math.ceil(saplings / 12)))
    end
    return true
end

WORK.plant = function(r: R): boolean
    if (r.inv["trees:sapling"] or 0) == 0 then
        return false
    end
    local grass, dirt = core.node_id("core:grass"), core.node_id("core:dirt")
    for _ = 1, 24 do
        local x = r.bx + math.random(-RANGE, RANGE)
        local z = r.bz + math.random(-RANGE, RANGE)
        for y = r.by + 4, r.by - 4, -1 do
            local below = core.get_block(x, y - 1, z)
            if (below == grass or below == dirt) and core.get_block(x, y, z) == AIR then
                local clear = true
                for dy = 0, 6 do
                    clear = clear and core.get_block(x, y + dy, z) == AIR
                end
                local crowded = #core.find_nodes(x - 3, y - 1, z - 3, x + 3, y + 6, z + 3, "trees:sapling") > 0
                for _, log in LOGS do
                    if core.node_id(log) ~= 0 then
                        crowded = crowded or #core.find_nodes(x - 3, y - 1, z - 3, x + 3, y + 6, z + 3, log) > 0
                    end
                end
                if clear and not crowded and not failed(r, x, y, z) and reach(r, x, y, z) then
                    act(r, "gather", "trees:sapling", 1.6, x, z)
                    if core.get_block(x, y, z) == AIR then
                        core.set_block(x, y, z, core.node_id("trees:sapling"))
                        take(r, "trees:sapling", 1)
                    end
                    return true
                end
                break
            end
        end
    end
    return false
end

-- The ground over its base's level, one exposed block at a time.
local function level(r: R, names: { string }, kind: string, clip: string, tool: string): boolean
    local px, py, pz = where(r)
    local x, y, z = find(names, r.bx, r.by, r.by + 6, r.bz, RANGE, function(x, y, z)
        return not failed(r, x, y, z) and free(x, y + 1, z)
    end, px, py, pz)
    if x == nil or y == nil or z == nil or not reach(r, x, y, z) then
        return false
    end
    act(r, clip, tool, 1.4, x, z)
    if is(kind, names, core.get_block(x, y, z)) then
        dig(r, x, y, z)
    end
    return true
end

WORK.soil = function(r: R): boolean
    return level(r, SOIL, "soil", "dig", r.tools.shovel or "tools:shovel")
end

WORK.stone = function(r: R): boolean
    return level(r, STONE, "stone", "mine", r.tools.pick or "tools:pick")
end

-- What falls when what holds it up is dug: propped with a stone instead, or
-- a tunnel under gravel fills again as fast as it is dug.
local FALLS = { "gravel:gravel", "core:sand", "rocks:red_sand" }

-- One cell of a tunnel towards (tx, ty, tz): two high, across or a step
-- down, a stone put down where there is no floor; and up the way a player
-- climbs out of a hole -- the cell over its head dug, up a cell, a stone put
-- under its feet. "moved" when it got one cell on, "blocked" when the way is
-- not one it may dig (water, lava, what a player built).
local function prop(x: number, y: number, z: number)
    local above = core.get_block(x, y, z)
    if above ~= AIR and table.find(FALLS, core.node_name(above)) then
        core.set_block(x, y, z, core.node_id("rocks:cobblestone"))
    end
end

local function tunnel_step(r: R, tx: number, ty: number, tz: number, tool: string): string
    local x, y, z = where(r)
    local clip = if TOOL_OF[tool] == "pick" then "mine" else "dig"
    local function open_up(c: { number }): boolean
        local over = core.node_name(core.get_block(c[1], c[2] + 1, c[3]))
        if not diggable(core.get_block(c[1], c[2], c[3])) or over:sub(1, 6) == "lakes:" or over == "rocks:lava" then
            return false
        end
        if core.get_block(c[1], c[2], c[3]) ~= AIR then
            act(r, clip, tool, 1.2, c[1], c[3])
            dig(r, c[1], c[2], c[3])
        end
        prop(c[1], c[2] + 1, c[3])
        return true
    end
    if ty > y then
        if not open_up({ x, y + 2, z }) then
            return "blocked"
        end
        core.mob_place(r.id, x + 0.5, y + 1.02, z + 0.5)
        core.set_block(x, y, z, core.node_id("rocks:cobblestone"))
        wait(r, 0.4)
        return "moved"
    end
    local dx, dz = tx - x, tz - z
    local sx, sz = 0, 0
    if math.abs(dx) >= math.abs(dz) and dx ~= 0 then
        sx = if dx > 0 then 1 else -1
    elseif dz ~= 0 then
        sz = if dz > 0 then 1 else -1
    else
        sx = 1
    end
    local sy = if ty < y - 1 then -1 else 0
    local nx, ny, nz = x + sx, y + sy, z + sz
    local cells = { { nx, ny, nz }, { nx, ny + 1, nz } }
    if sy < 0 then
        table.insert(cells, { nx, ny + 2, nz })
    end
    local under = core.node_name(core.get_block(nx, ny - 1, nz))
    if under:sub(1, 6) == "lakes:" or under == "rocks:lava" then
        return "blocked"
    end
    for _, c in cells do
        if not open_up(c) then
            return "blocked"
        end
    end
    if core.node_solid(core.get_block(nx, ny - 1, nz)) ~= 1 then
        core.set_block(nx, ny - 1, nz, core.node_id("rocks:cobblestone"))   -- a cave under the way: bridged
    end
    return if walk(r, nx, ny, nz, 8) then "moved" else "blocked"
end

-- Through the ground to a cell, a tunnel step at a time; true when there.
local function dig_to(r: R, tx: number, ty: number, tz: number, tool: string, steps: number): boolean
    for _ = 1, steps do
        local x, y, z = where(r)
        if math.abs(x - tx) + math.abs(z - tz) <= 0 and math.abs(y - ty) <= 1 then
            return true
        end
        if tunnel_step(r, tx, ty, tz, tool) ~= "moved" then
            return false
        end
    end
    return false
end

-- Back out of a tunnel to where it went in: walked when the way is open,
-- dug and climbed when not.
local function back_out(r: R, trail: { { number } }, tool: string)
    local c = trail[1]
    if c ~= nil and not walk(r, c[1], c[2], c[3], 15) then
        dig_to(r, c[1], c[2], c[3], tool, 120)
    end
end

-- To the nearest ore, digging a tunnel, and back out of it with what it found.
WORK.ores = function(r: R): boolean
    local iron = r.tools.pick == "furnace:iron_pick" or r.tools.pick == "furnace:diamond_pick"
    local px, py, pz = where(r)
    local ox, oy, oz = find(ORES, r.bx, r.by - 24, r.by + 8, r.bz, RANGE + 4, function(x, y, z)
        return not failed(r, x, y, z) and (iron or not IRON_ONLY[core.node_name(core.get_block(x, y, z))])
    end, px, py, pz)
    if ox == nil or oy == nil or oz == nil then
        return false
    end
    local pick = r.tools.pick or "tools:pick"
    local trail: { { number } } = {}
    for _ = 1, 80 do
        local x, y, z = where(r)
        local dx, dy, dz = ox - x, oy - y, oz - z
        if math.abs(dx) + math.abs(dz) <= 1 and dy >= -1 and dy <= 1 then
            act(r, "mine", pick, 2.0, ox, oz)
            if is("ores", ORES, core.get_block(ox, oy, oz)) then
                dig(r, ox, oy, oz)
            end
            -- The rest of a seam, while it is here.
            for _, d in { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } } do
                local sx, sy, sz = ox + d[1], oy + d[2], oz + d[3]
                local b = core.get_block(sx, sy, sz)
                if is("ores", ORES, b) and (iron or not IRON_ONLY[core.node_name(b)])
                    and math.abs(sx - x) <= 2 and math.abs(sz - z) <= 2 then
                    act(r, "mine", pick, 1.6, sx, sz)
                    dig(r, sx, sy, sz)
                end
            end
            back_out(r, trail, pick)
            return true
        end
        table.insert(trail, { x, y, z })
        -- An ore just over its head is reached from beside, not by a step up.
        if tunnel_step(r, ox, oy, oz, pick) ~= "moved" or count(r) >= FULL + 16 then
            r.tried[`{ox},{oy},{oz}`] = clock + 600
            back_out(r, trail, pick)
            return true
        end
    end
    back_out(r, trail, pick)
    return true
end

-- A pit five by five east of the base, the cells x+2..x+6, z-2..z+2, its
-- layer k at the base's level less k. Round it, in the ring x+1..x+7,
-- z-3..z+3, a stair: step k is the k-th cell of the ring from the one beside
-- the base, its floor level with the pit's after layer k and three cells
-- clear over it, so the pit is never deeper than the stair. A step it may
-- not dig (water, lava, what a player built) is where the quarry stops.
-- ponytail: one turn of the stair, 24 layers; a second turn would need its
-- steps checked against the first one's headroom.
local QUARRY = 24

local function stair(r: R): { { number } }
    local x0, x1, z0, z1 = r.bx + 1, r.bx + 7, r.bz - 3, r.bz + 3
    local out = {}
    for z = r.bz, z1 do table.insert(out, { x0, z }) end
    for x = x0 + 1, x1 do table.insert(out, { x, z1 }) end
    for z = z1 - 1, z0, -1 do table.insert(out, { x1, z }) end
    for x = x1 - 1, x0, -1 do table.insert(out, { x, z0 }) end
    for z = z0 + 1, r.bz - 1 do table.insert(out, { x0, z }) end
    return out
end

-- The ground as the world made it, with no water or lava beside it to let in.
local function quarriable(x: number, y: number, z: number): boolean
    local id = core.get_block(x, y, z)
    if id == AIR or not diggable(id) then
        return false
    end
    for _, d in { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } } do
        local n = core.node_name(core.get_block(x + d[1], y + d[2], z + d[3]))
        if n:sub(1, 6) == "lakes:" or n == "rocks:lava" then
            return false
        end
    end
    return true
end

WORK.quarry = function(r: R): boolean
    local pick = r.tools.pick or "tools:pick"
    local steps = stair(r)
    local px, py, pz = where(r)
    for k = 1, QUARRY do
        local s, y = steps[k], r.by - k
        -- The step first, top down, so the way up is always there.
        for h = 2, 0, -1 do
            if not free(s[1], y + h, s[2]) then
                if not quarriable(s[1], y + h, s[2]) or not reach(r, s[1], y + h, s[2]) then
                    return false
                end
                act(r, "mine", pick, 1.4, s[1], s[2])
                dig(r, s[1], y + h, s[2])
                prop(s[1], y + 3, s[2])
                return true
            end
        end
        if core.node_solid(core.get_block(s[1], y - 1, s[2])) ~= 1 then
            core.set_block(s[1], y - 1, s[2], core.node_id("rocks:cobblestone"))   -- a cave under the step
        end
        -- Then the layer, the nearest cell first; the next layer only when
        -- nothing of this one is left to take.
        local left, best, cx, cz = false, math.huge, nil, nil
        for x = r.bx + 2, r.bx + 6 do
            for z = r.bz - 2, r.bz + 2 do
                if quarriable(x, y, z) then
                    left = true
                    local d = (x - px) ^ 2 + (y - py) ^ 2 + (z - pz) ^ 2
                    if d < best and not failed(r, x, y, z) then
                        best, cx, cz = d, x, z
                    end
                end
            end
        end
        if cx ~= nil and cz ~= nil then
            if not reach(r, cx, y, cz) then
                return false
            end
            act(r, "mine", pick, 1.4, cx, cz)
            if quarriable(cx, y, cz) then
                dig(r, cx, y, cz)
            end
            return true
        end
        if left then
            return false
        end
    end
    return false
end

-- ---------------------------------------------------------------- torches

-- A creature comes out only where the light is 7 or less (core/ecs/mob.h); a
-- torch's 14 falls a step a cell, so it keeps off every cell within six steps
-- of it, counted along x, y and z.
local TORCH, LIT_RANGE, LIT_REACH = "torches:torch", 100, 6

-- The ground a creature could stand on in a column about y: the first floor
-- from above with two cells of room over it, not a treetop, not under water.
local function ground(x: number, y: number, z: number): number?
    local run = 0
    for gy = y + 12, y - 16, -1 do
        local id = core.get_block(x, gy, z)
        if core.node_solid(id) == 1 then
            if run >= 2 and not is("leaves", LEAVES, id) and not is("logs", LOGS, id) and free(x, gy + 1, z) then
                return gy + 1
            end
            run = 0
        else
            run += 1
        end
    end
    return nil
end

local function dark(x: number, y: number, z: number): boolean
    return core.block_light(x, y, z) <= 7
end

-- Where a torch can go: air, or grass and flowers it pulls up first.
local function torch_room(x: number, y: number, z: number): boolean
    local id = core.get_block(x, y, z)
    return id == AIR or core.node_solid(id) ~= 1 and core.node_name(id):sub(1, 7) == "plants:"
end

-- The nearest dark ground to the base, ring by ring out to LIT_RANGE; where
-- the last call stopped is where the next one goes on. Looking is spread over
-- the ticks, a few hundred columns at a time.
local function next_dark(r: R): (number?, number?, number?)
    local seen = 0
    local k = r.ring or 0
    while k <= LIT_RANGE do
        r.ring = k
        -- With the anchor, ground not in memory is asked for and the ring
        -- looked at again once it is; without, it is passed over.
        local asked = false
        for i = 0, math.max(8 * k, 1) - 1 do
            -- The ring's cells in order: the four sides, 2k each.
            local side, t = i // math.max(2 * k, 1), i % math.max(2 * k, 1)
            local dx, dz
            if side == 0 then dx, dz = -k + t, -k
            elseif side == 1 then dx, dz = k, -k + t
            elseif side == 2 then dx, dz = k - t, k
            else dx, dz = -k, k - t end
            if dx * dx + dz * dz <= LIT_RANGE * LIT_RANGE then
                local x, z = r.bx + dx, r.bz + dz
                if r.tools.anchor ~= nil and core.loaded(x, r.by, z) == 0 then
                    asked = core.keep_loaded(x, z, 0) > 0 or asked
                end
                local y = ground(x, r.by, z)
                if y ~= nil and dark(x, y, z) and not failed(r, x, y, z) then
                    return x, y, z
                end
                seen += 1
                if seen % 400 == 0 then
                    wait(r, 0)
                end
            end
        end
        if asked then
            wait(r, 1)
        else
            k += 1
        end
    end
    r.ring = 0
    return nil, nil, nil
end

-- On flat ground the cells a torch keeps clear are a diamond, and diamonds
-- of six tile the plane with their middles on the lattice of (6, 7) and
-- (7, -6) from the base: 85 cells a torch and none lit twice. The lattice
-- point whose diamond holds (dx, dz), when a torch can go there.
local function on_lattice(r: R, dx: number, dy: number, dz: number): (number?, number?, number?)
    local px, pz = dx - r.bx, dz - r.bz
    local a, b = (6 * px + 7 * pz) / 85, (7 * px - 6 * pz) / 85
    for _, i in { math.floor(a), math.ceil(a) } do
        for _, j in { math.floor(b), math.ceil(b) } do
            local x, z = r.bx + 6 * i + 7 * j, r.bz + 7 * i - 6 * j
            if math.abs(x - dx) + math.abs(z - dz) <= LIT_REACH then
                local y = ground(x, dy, z)
                if y ~= nil and math.abs(x - dx) + math.abs(y - dy) + math.abs(z - dz) <= LIT_REACH
                    and torch_room(x, y, z) and not failed(r, x, y, z)
                    and (x - r.bx) ^ 2 + (z - r.bz) ^ 2 <= LIT_RANGE ^ 2 then
                    return x, y, z
                end
                return nil, nil, nil
            end
        end
    end
    return nil, nil, nil
end

-- Where the lattice cannot help (a hill, a wall, a torch there already that
-- the ground hides): where a torch lights most of the dark about (dx, dy, dz), among the cells
-- that light that one too: one torch to as many dark cells as it can have.
local function best_spot(r: R, dx: number, dy: number, dz: number): (number?, number?, number?)
    local W = 2 * LIT_REACH
    local h: { [number]: { [number]: number } } = {}
    local dk: { [number]: { [number]: boolean } } = {}
    for x = dx - W, dx + W do
        h[x], dk[x] = {}, {}
        for z = dz - W, dz + W do
            local y = ground(x, dy, z)
            if y ~= nil then
                h[x][z], dk[x][z] = y, dark(x, y, z)
            end
        end
    end
    local best, bx, by, bz = 0, nil, nil, nil
    for x = dx - LIT_REACH, dx + LIT_REACH do
        for z = dz - LIT_REACH, dz + LIT_REACH do
            local y = h[x][z]
            if y ~= nil and math.abs(x - dx) + math.abs(y - dy) + math.abs(z - dz) <= LIT_REACH
                and torch_room(x, y, z) and (x - r.bx) ^ 2 + (z - r.bz) ^ 2 <= LIT_RANGE ^ 2 then
                local n = 0
                for cx = x - LIT_REACH, x + LIT_REACH do
                    for cz = z - LIT_REACH, z + LIT_REACH do
                        local cy = h[cx][cz]
                        if cy ~= nil and dk[cx][cz]
                            and math.abs(cx - x) + math.abs(cy - y) + math.abs(cz - z) <= LIT_REACH then
                            n += 1
                        end
                    end
                end
                if n > best then
                    best, bx, by, bz = n, x, y, z
                end
            end
        end
    end
    return bx, by, bz
end

-- Far off in hops of a dozen cells over the ground, a way core.mob_goto
-- finds each time; true when it is within 16 of (x, z).
local function travel(r: R, x: number, z: number): boolean
    for _ = 1, 16 do
        local px, py, pz = where(r)
        local d = math.sqrt((x - px) ^ 2 + (z - pz) ^ 2)
        if d <= 16 then
            return true
        end
        local hx, hz = math.floor(px + (x - px) * 12 / d), math.floor(pz + (z - pz) * 12 / d)
        local hy = ground(hx, py, hz)
        if hy == nil or not walk(r, hx, hy, hz, 15) then
            return false
        end
    end
    return false
end

-- Torches from the chests by its base when it has none; said once when there are none.
local function fetch_torches(r: R): boolean
    if (r.inv[TORCH] or 0) > 0 then
        return true
    end
    local iid = core.item_id(TORCH)
    if iid == 0 then
        return false
    end
    if not travel(r, r.bx, r.bz) then
        return false
    end
    for _, c in chests(r) do
        if core.container_count(c[1], c[2], c[3], iid) > 0 and reach(r, c[1], c[2], c[3]) then
            act(r, "gather", "", 1.0, c[1], c[3])
            add(r, TORCH, core.container_take(c[1], c[2], c[3], iid, 16))
            r.said_dark = false
            return (r.inv[TORCH] or 0) > 0
        end
    end
    if not r.said_dark then
        r.said_dark = true
        for _, p in core.players() do
            if core.player_name(p) == r.owner then
                core.notice(p, `Robot {r.n} has no torches to light up with: put some in a chest near its base`)
            end
        end
    end
    return false
end

WORK.light = function(r: R): boolean
    local dx, dy, dz = next_dark(r)
    if dx == nil or dy == nil or dz == nil or not fetch_torches(r) then
        return false
    end
    local x, y, z = on_lattice(r, dx, dy, dz)
    if x == nil then
        x, y, z = best_spot(r, dx, dy, dz)
    end
    if x == nil or y == nil or z == nil then
        r.tried[`{dx},{dy},{dz}`] = clock + 600
        return true
    end
    if not travel(r, x, z) or not reach(r, x, y, z) then
        r.tried[`{dx},{dy},{dz}`] = clock + 600
        return true
    end
    act(r, "gather", TORCH, 0.8, x, z)
    if torch_room(x, y, z) and core.node_solid(core.get_block(x, y - 1, z)) == 1 then
        dig(r, x, y, z)
        core.set_block(x, y, z, core.node_id(TORCH))
        take(r, TORCH, 1)
    end
    return true
end

-- Home to its base when the way there is cut -- a tunnel fallen in, the
-- ground dug from under it, a robot picked up somewhere and put down in a
-- hole: along a way if there is one, else digging one with whatever it has.
go_home = function(r: R)
    if standable(r.bx, r.by, r.bz) and walk(r, r.bx, r.by, r.bz, 20) then
        return
    end
    dig_to(r, r.bx, r.by, r.bz, r.tools.pick or r.tools.shovel or "", 200)
end

local function lost(r: R): boolean
    local x, y, z = where(r)
    return math.abs(x - r.bx) > RANGE + 4 or math.abs(z - r.bz) > RANGE + 4 or y < r.by - 3
end

WORK.sort = function(r: R): boolean
    local did = next(r.inv) ~= nil and unload(r)
    local list = chests(r)
    -- One stack a time: the first thing not in the chest that has most of it.
    for _, c in list do
        for slot = 1, 54 do
            local iid = core.container_item(c[1], c[2], c[3], slot)
            if iid ~= 0 then
                local best = best_chest(list, iid)
                if best ~= nil and best ~= c and core.container_count(best[1], best[2], best[3], iid)
                    > core.container_count(c[1], c[2], c[3], iid)
                    and core.container_fill(best[1], best[2], best[3]) < 1 then
                    if reach(r, c[1], c[2], c[3]) then
                        act(r, "gather", "", 1.0, c[1], c[3])
                        local got = core.container_take(c[1], c[2], c[3], iid, 64)
                        local name = core.item_name(iid)
                        add(r, name, got)
                        if reach(r, best[1], best[2], best[3]) then
                            act(r, "gather", name, 1.0, best[1], best[3])
                            take(r, name, got - core.container_add(best[1], best[2], best[3], iid, got))
                        end
                    end
                    return true
                end
            end
        end
    end
    return did
end

local function ids(names: { string }): { number }
    local out = {}
    for _, n in names do
        local i = core.item_id(n)
        if i ~= 0 then
            table.insert(out, i)
        end
    end
    return out
end

WORK.cook = function(r: R): boolean
    local px, py, pz = where(r)
    local fx, fy, fz = find(FURNACES, r.bx, r.by - 4, r.by + 4, r.bz, RANGE, function()
        return true
    end, px, py, pz)
    if fx == nil or fy == nil or fz == nil then
        return false
    end
    local list = chests(r)
    -- What is cooked, out to a chest: four at a time, or the last of it.
    local raw_in = 0
    for _, iid in ids(RAW) do
        raw_in += core.container_count(fx, fy, fz, iid, "raw")
    end
    for _, iid in ids(COOKED) do
        local done = core.container_count(fx, fy, fz, iid, "product")
        if done > 0 and (done >= 4 or raw_in == 0) and reach(r, fx, fy, fz) then
            act(r, "gather", "", 1.0, fx, fz)
            add(r, core.item_name(iid), core.container_take(fx, fy, fz, iid, 64, "product"))
            unload(r)
            return true
        end
    end
    -- Raw food and fuel in, from what it carries or from a chest.
    for _, want in { { RAW, "raw" }, { FUEL, "fuel" } } do
        local names, role = want[1] :: { string }, want[2] :: string
        local has = 0
        for _, iid in ids(names) do
            has += core.container_count(fx, fy, fz, iid, role)
        end
        if has < 4 then
            for _, iid in ids(names) do
                local name = core.item_name(iid)
                if (r.inv[name] or 0) == 0 then
                    for _, c in list do
                        if core.container_count(c[1], c[2], c[3], iid) > 0 and reach(r, c[1], c[2], c[3]) then
                            act(r, "gather", "", 1.0, c[1], c[3])
                            add(r, name, core.container_take(c[1], c[2], c[3], iid, 8))
                            break
                        end
                    end
                end
                local n = r.inv[name] or 0
                if n > 0 and reach(r, fx, fy, fz) then
                    act(r, "gather", name, 1.0, fx, fz)
                    take(r, name, n - core.container_add(fx, fy, fz, iid, n, role))
                    return true
                end
            end
        end
    end
    return false
end

-- The animals round its base (mods/animals): every grown sheep shorn, the
-- wool in its bag; and, with wheat or seeds in its bag or a chest, two of a
-- kind that eats it put in love while there are fewer than eight of that
-- kind. A sheep or a pair it has seen to is left alone a while.
local FOOD_OF = { ["animals:cow"] = "farm:wheat", ["animals:pig"] = "animals:seeds",
                  ["animals:chicken"] = "animals:seeds" }
WORK.tend = function(r: R): boolean
    local herd: { [string]: { number } } = {}
    for _, m in core.mob_ids(r.bx + 0.5, r.by, r.bz + 0.5, RANGE, true) do
        local kind = core.mob_kind(m)
        if string.find(kind, "^animals:sheep") then
            FOOD_OF[kind] = "farm:wheat"
        end
        if FOOD_OF[kind] then
            herd[kind] = herd[kind] or {}
            table.insert(herd[kind], m)
        end
    end
    local function go(m: number): boolean
        local x, y, z = core.mob_pos(m)
        return x ~= nil and reach(r, math.floor(x), math.floor(y + 0.01), math.floor(z))
    end
    for kind, list in herd do
        if string.find(kind, "^animals:sheep") then
            for _, m in list do
                if (r.tried[`shear {m}`] or 0) <= clock and go(m) then
                    r.tried[`shear {m}`] = clock + 120
                    local x, _, z = core.mob_pos(m)
                    act(r, "gather", "", 1.0, math.floor(x or r.bx), math.floor(z or r.bz))
                    local n = core.mob_shear_now(m, false)
                    if n > 0 then
                        add(r, if kind == "animals:sheep" then "animals:wool"
                            else `colors:{string.sub(kind, #"animals:sheep_" + 1)}_wool`, n)
                        return true
                    end
                end
            end
        end
    end
    for kind, list in herd do
        local food = FOOD_OF[kind]
        if #list >= 2 and #list < 8 and (r.tried[`feed {kind}`] or 0) <= clock then
            if (r.inv[food] or 0) < 2 then
                local iid = core.item_id(food)
                for _, c in chests(r) do
                    if iid ~= 0 and core.container_count(c[1], c[2], c[3], iid) >= 2 and reach(r, c[1], c[2], c[3]) then
                        act(r, "gather", "", 1.0, c[1], c[3])
                        add(r, food, core.container_take(c[1], c[2], c[3], iid, 8))
                        break
                    end
                end
            end
            if (r.inv[food] or 0) >= 2 then
                r.tried[`feed {kind}`] = clock + 60
                -- Two within six of each other, both fed from where it stands
                -- by the first: fed apart, one's love is over before the
                -- other's starts.
                for i = 1, #list - 1 do
                    local ax, _, az = core.mob_pos(list[i])
                    for j = i + 1, #list do
                        local bx, _, bz = core.mob_pos(list[j])
                        if ax and bx and (ax - bx) ^ 2 + (az - bz) ^ 2 < 36 and go(list[i]) then
                            act(r, "gather", food, 1.0, math.floor(ax), math.floor(az))
                            local fed = core.mob_love(list[i]) + core.mob_love(list[j])
                            take(r, food, fed)
                            if fed > 0 then
                                return true
                            end
                        end
                    end
                end
            end
        end
    end
    return false
end

-- The layout's cells, as mods/blueprints answers "blueprints:layout".
local LAYOUT: { last: { any }?, said: { [number]: number } } = { last = nil, said = {} }
core.event.on("blueprints:cells", function(ev)
    local cells = {}
    for x, y, z, name, f in string.gmatch(ev.cells, "(%-?%d+),(%-?%d+),(%-?%d+),([^,]*),(%d+)") do
        table.insert(cells, { tonumber(x), tonumber(y), tonumber(z), name, tonumber(f) })
    end
    LAYOUT.last = { owner = ev.owner, cells = cells }
end)
WORK.build = function(r: R): boolean
    LAYOUT.last = nil
    core.event.emit("blueprints:layout", { owner = r.owner })
    local g = LAYOUT.last
    if g == nil or g.owner ~= r.owner then
        return false
    end
    local missing: { [string]: number } = {}
    local list = chests(r)
    -- Every cell is reached from the ground OUTSIDE the layout -- inside, it
    -- walls itself in -- at most three off its column, by the layout's bottom.
    -- ponytail: a hand reaches some four up from there; a taller blueprint's
    -- top waits for scaffolding nobody builds.
    local base = if #g.cells > 0 then g.cells[1][2] else 0
    local inside: { [string]: boolean } = {}
    for _, c in g.cells do
        inside[`{c[1]},{c[3]}`] = true
    end
    local function stand(x: number, z: number): boolean
        local key = `{x},{base},{z}`
        if (r.tried[key] or 0) > clock then
            return false
        end
        local px, _, pz = core.mob_pos(r.id)
        local best, bx, by, bz = math.huge, 0, 0, 0
        for dx = -3, 3 do
            for dz = -3, 3 do
                local cx, cz = x + dx, z + dz
                if not inside[`{cx},{cz}`] then
                    for _, sy in { base, base + 1, base - 1 } do
                        if standable(cx, sy, cz) then
                            local d = dx * dx + dz * dz + ((cx - (px or cx)) ^ 2 + (cz - (pz or cz)) ^ 2) * 0.01
                            if d < best then
                                best, bx, by, bz = d, cx, sy, cz
                            end
                        end
                    end
                end
            end
        end
        if best == math.huge or not walk(r, bx, by, bz) then
            r.tried[key] = clock + 90
            return false
        end
        return true
    end
    for _, c in g.cells do
        local x, y, z, name, f = c[1], c[2], c[3], c[4], c[5]
        local want = if name == "" then AIR else core.node_id(name)
        local now = core.get_block(x, y, z)
        if now ~= want and (r.tried[`{x},{base},{z}`] or 0) <= clock then
            if now ~= AIR then
                if stand(x, z) then
                    act(r, "dig", "", 0.6, x, z)
                    dig(r, x, y, z)
                    return true
                end
            elseif want ~= AIR then
                if (r.inv[name] or 0) == 0 then
                    local iid = core.item_id(name)
                    for _, ch in list do
                        if iid ~= 0 and core.container_count(ch[1], ch[2], ch[3], iid) > 0 and reach(r, ch[1], ch[2], ch[3]) then
                            act(r, "gather", "", 1.0, ch[1], ch[3])
                            add(r, name, core.container_take(ch[1], ch[2], ch[3], iid, 16))
                            break
                        end
                    end
                end
                if (r.inv[name] or 0) == 0 then
                    missing[name] = (missing[name] or 0) + 1
                elseif stand(x, z) then
                    act(r, "gather", name, 0.4, x, z)
                    if core.get_block(x, y, z) == AIR then
                        core.set_block(x, y, z, want)
                        core.set_facing(x, y, z, f)
                        take(r, name, 1)
                    end
                    return true
                end
            end
        end
    end
    if next(missing) ~= nil and (LAYOUT.said[r.n] or 0) <= clock then
        LAYOUT.said[r.n] = clock + 60
        local parts = {}
        for name, n in missing do
            table.insert(parts, `{n} {(string.gsub(string.gsub(name, "^.*:", ""), "_", " "))}`)
        end
        for _, q in core.players() do
            if core.player_name(q) == r.owner then
                core.notice(q, `Your robot needs, for the blueprint: {table.concat(parts, ", ")}`)
            end
        end
    end
    return false
end

WORK.follow = function(r: R): boolean
    for _, p in core.players() do
        if core.player_name(p) == r.owner then
            local x, y, z = core.player_pos(p)
            local rx, ry, rz = core.mob_pos(r.id)
            if x ~= nil and y ~= nil and z ~= nil and rx ~= nil and ry ~= nil and rz ~= nil
                and (x - rx) ^ 2 + (z - rz) ^ 2 > 9 then
                core.mob_goto(r.id, math.floor(x), math.floor(y + 0.01), math.floor(z))
            end
        end
    end
    wait(r, 1.0)
    return true
end

-- ---------------------------------------------------------------- a program

local function life(r: R)
    while true do
        local did = false
        if r.running and #r.program > 0 then
            if table.find(r.program, "follow") then
                WORK.follow(r)
                did = true
            else
                r.step = (r.step % #r.program) + 1
                local order = ORDER[r.program[r.step]]
                if order and (order.tool == nil or r.tools[order.tool] ~= nil) then
                    -- The same order again while it finds something to do,
                    -- eight times at most, then the next one.
                    for _ = 1, 8 do
                        if full(r) or not r.running or not WORK[order.key](r) then
                            break
                        end
                        did = true
                        wait(r, 0.2)
                    end
                end
            end
        end
        if not did then
            if r.running and r.program[1] ~= "follow" then
                if (next(r.inv) ~= nil and not unload(r) and #chests(r) > 0) or lost(r) then
                    go_home(r)
                end
            end
            wait(r, 3)
        end
    end
end

-- ---------------------------------------------------------------- keeping them

local function save(r: R)
    local inv, tools = {}, {}
    for item, n in r.inv do
        table.insert(inv, `{item}={n}`)
    end
    for _, t in r.tools do
        table.insert(tools, t)
    end
    core.storage_set(`r{r.n}`, table.concat({
        r.owner, `{r.bx},{r.by},{r.bz}`, `{r.lx},{r.ly},{r.lz}`, table.concat(r.program, ","),
        if r.running then "1" else "0", table.concat(inv, ","), table.concat(tools, ",") }, "|"))
    dirty[r.n] = nil
end

local function save_index()
    local ns = {}
    for n in robots do
        table.insert(ns, tostring(n))
    end
    table.sort(ns)
    core.storage_set("robots", table.concat(ns, ","))
    core.storage_set("next", tostring(next_n))
end

local function start(r: R)
    -- Where it was, or the first cell over it with room, never in the ground.
    local y = r.ly
    while y < r.ly + 8 and not (free(r.lx, y, r.lz) and free(r.lx, y + 1, r.lz)) do
        y += 1
    end
    local nid = core.spawn_mob("helpers:helper", r.lx + 0.5, y + 0.05, r.lz + 0.5)
    r.id = nid
    r.wake = clock
    r.walking = false
    r.co = if nid ~= 0 then coroutine.create(function() life(r) end) else nil
end

local function make(owner: string, x: number, y: number, z: number): R
    local r: R = { n = next_n, owner = owner, id = 0, bx = x, by = y, bz = z, lx = x, ly = y, lz = z,
                   program = {}, running = false, step = 0, inv = {}, tools = {}, wake = 0,
                   walking = false, tried = {}, said_full = false }
    next_n += 1
    robots[r.n] = r
    return r
end

core.event.on("core:world_ready", function()
    next_n = tonumber(core.storage_get("next")) or 1
    for ns in string.gmatch(core.storage_get("robots"), "%d+") do
        local n = tonumber(ns) :: number
        local f = string.split(core.storage_get(`r{n}`), "|")
        if #f >= 7 then
            local b = string.split(f[2], ",")
            local l = string.split(f[3], ",")
            local r = make(f[1], tonumber(b[1]) or 0, tonumber(b[2]) or 0, tonumber(b[3]) or 0)
            robots[r.n], r.n = nil, n
            robots[n] = r
            next_n -= 1
            r.lx, r.ly, r.lz = tonumber(l[1]) or r.bx, tonumber(l[2]) or r.by, tonumber(l[3]) or r.bz
            r.program = if f[4] ~= "" then string.split(f[4], ",") else {}
            r.running = f[5] == "1"
            for _, kv in (if f[6] ~= "" then string.split(f[6], ",") else {}) do
                local item, c = string.match(kv, "^(.-)=(%d+)$")
                if item and c then
                    r.inv[item] = tonumber(c) :: number
                end
            end
            for _, t in (if f[7] ~= "" then string.split(f[7], ",") else {}) do
                if TOOL_OF[t] then
                    r.tools[TOOL_OF[t]] = t
                end
            end
        end
    end
end)

-- Put down: a right click on the ground with one in the hand.
core.event.on("core:used", function(ev)
    if ev.held ~= ITEM then
        return
    end
    ev.cancel = true
    if not free(ev.x, ev.y + 1, ev.z) or not free(ev.x, ev.y + 2, ev.z) then
        return
    end
    if core.take_item(ev.player, ITEM, 1) ~= 1 then
        return
    end
    local r = make(core.player_name(ev.player), ev.x, ev.y + 1, ev.z)
    start(r)
    save(r)
    save_index()
    core.notice(ev.player, `Robot {r.n}: give it tools with a right click, and right click it for its program`)
end)

-- ---------------------------------------------------------------- the panel

local function short(item: string): string
    return (string.gsub(string.gsub(item, "^.*:", ""), "_", " "))
end

local function panel(r: R, player: number)
    local name = `helpers:robot{r.n}`
    core.ui_define(name, 15, 15.6)
    core.ui_label(name, 0.4, 0.3, `Robot {r.n} -- {if r.running then "working" else "stopped"}`)
    core.ui_button(name, "close", 11.0, 0.2, 3.6, 0.8, "Close")
    core.ui_label(name, 0.4, 1.1, "Program, in order:")
    for i = 1, STEPS do
        local o = ORDER[r.program[i] or ""]
        if o then
            local need = if o.tool and not r.tools[o.tool] then `  (needs a {o.tool})` else ""
            core.ui_label(name, 0.6, 1.4 + i * 0.9, `{i}. {o.text}{need}`)
            core.ui_button(name, `del{i}`, 6.6, 1.2 + i * 0.9, 0.8, 0.8, "x")
        end
    end
    core.ui_label(name, 8.2, 1.1, "Add:")
    for i, o in ORDERS do
        core.ui_button(name, `add_{o.key}`, 8.2, 1.2 + i * 0.9, 6.2, 0.8, o.text)
    end
    local tools = {}
    for _, t in r.tools do
        table.insert(tools, short(t))
    end
    table.sort(tools)
    core.ui_label(name, 0.4, 12.8, `Tools: {if #tools > 0 then table.concat(tools, ", ") else "none"}` ..
        `   Carrying: {count(r)}`)
    core.ui_button(name, "run", 0.4, 14.0, 3.2, 1.0, if r.running then "Stop" else "Run")
    core.ui_button(name, "clear", 3.8, 14.0, 3.2, 1.0, "Clear")
    core.ui_button(name, "base", 7.2, 14.0, 3.6, 1.0, "Base here")
    core.ui_button(name, "pickup", 11.0, 14.0, 3.6, 1.0, "Pick up")
    core.ui_show(name, true, player)
end

local function restart(r: R)
    r.step = 0
    r.wake = clock
    if r.id ~= 0 then
        core.mob_act(r.id, "", "")
        local x, y, z = core.mob_pos(r.id)
        if x ~= nil and y ~= nil and z ~= nil then
            core.mob_goto(r.id, math.floor(x), math.floor(y + 0.01), math.floor(z))
        end
        r.co = coroutine.create(function() life(r) end)
    end
end

local function give_or_drop(player: number, item: string, n: number, x: number, y: number, z: number)
    local fit = core.give_item(player, item, n)
    if fit < n then
        core.drop_item(x, y, z, core.item_id(item), n - fit)
    end
end

core.event.on("core:ui_click", function(ev)
    local n = tonumber(string.match(ev.panel, "^helpers:robot(%d+)$") or "")
    local r = if n then robots[n] else nil
    if r == nil or core.player_name(ev.player) ~= r.owner then
        return
    end
    if ev.id == "close" then
        core.ui_show(ev.panel, false, ev.player)
        return
    end
    local add_key = string.match(ev.id, "^add_(%a+)$")
    local del = tonumber(string.match(ev.id, "^del(%d)$") or "")
    if add_key and ORDER[add_key] and #r.program < STEPS then
        table.insert(r.program, add_key)
        restart(r)
    elseif del and r.program[del] then
        table.remove(r.program, del)
        restart(r)
    elseif ev.id == "run" then
        r.running = not r.running
        restart(r)
    elseif ev.id == "clear" then
        r.program = {}
        r.running = false
        restart(r)
    elseif ev.id == "base" then
        local x, y, z = core.mob_pos(r.id)
        if x ~= nil and y ~= nil and z ~= nil then
            r.bx, r.by, r.bz = math.floor(x), math.floor(y + 0.01), math.floor(z)
            core.notice(ev.player, `Robot {r.n} works about here now`)
        end
    elseif ev.id == "pickup" then
        local x, y, z = core.mob_pos(r.id)
        x, y, z = x or r.lx, y or r.ly, z or r.lz
        core.mob_remove(r.id)
        core.ui_show(ev.panel, false, ev.player)
        give_or_drop(ev.player, ITEM, 1, x, y, z)
        for item, c in r.inv do
            give_or_drop(ev.player, item, c, x, y, z)
        end
        for _, t in r.tools do
            give_or_drop(ev.player, t, 1, x, y, z)
        end
        robots[r.n] = nil
        core.storage_set(`r{r.n}`, "")
        save_index()
        return
    end
    save(r)
    panel(r, ev.player)
end)

core.event.on("core:used_mob", function(ev)
    for _, r in robots do
        if r.id == ev.target then
            local who = core.player_name(ev.player)
            if who ~= r.owner then
                core.notice(ev.player, `This robot is {r.owner}'s`)
                return
            end
            local slot = TOOL_OF[ev.held]
            if slot ~= nil then
                if core.take_item(ev.player, ev.held, 1) == 1 then
                    local had = r.tools[slot]
                    r.tools[slot] = ev.held
                    if had then
                        local x, y, z = core.mob_pos(r.id)
                        give_or_drop(ev.player, had, 1, x or r.lx, y or r.ly, z or r.lz)
                    end
                    local t = short(ev.held)
                    core.notice(ev.player, `Robot {r.n} has {if string.find("aeiou", t:sub(1, 1), 1, true) then "an" else "a"} {t} now`)
                    save(r)
                end
                return
            end
            panel(r, ev.player)
            return
        end
    end
end)

-- ---------------------------------------------------------------- the clock

local step_at, save_at = 0.0, 0.0
core.event.on("core:tick", function(ev)
    clock += ev.dt
    if clock < step_at then
        return
    end
    step_at = clock + 0.2
    for _, r in robots do
        if r.tools.anchor ~= nil and clock >= (r.kept or 0) then
            r.kept = clock + 10
            core.keep_loaded(r.lx, r.lz, 24)
            core.keep_loaded(r.bx, r.bz, RANGE + 4)
        end
        if r.id == 0 then
            -- Not out yet: once its place is in memory.
            if core.loaded(r.lx, r.ly, r.lz) == 1 then
                start(r)
            end
        else
            local x, y, z = core.mob_pos(r.id)
            if x == nil or y == nil or z == nil then
                -- Struck down: it, what it carried and its tools are left where it fell.
                local iid = core.item_id(ITEM)
                core.drop_item(r.lx, r.ly, r.lz, iid, 1)
                for item, c in r.inv do
                    core.drop_item(r.lx, r.ly, r.lz, core.item_id(item), c)
                end
                for _, t in r.tools do
                    core.drop_item(r.lx, r.ly, r.lz, core.item_id(t), 1)
                end
                robots[r.n] = nil
                core.storage_set(`r{r.n}`, "")
                save_index()
            else
                r.lx, r.ly, r.lz = math.floor(x), math.floor(y + 0.5), math.floor(z)
                if r.co ~= nil and (clock >= r.wake or (r.walking and core.mob_going(r.id) == 0)) then
                    local ok, err = coroutine.resume(r.co)
                    if not ok then
                        core.log(`helpers: robot {r.n}: {err}`)
                        r.co = coroutine.create(function() life(r) end)
                    end
                end
            end
        end
    end
    if clock >= save_at then
        save_at = clock + 20
        for _, r in robots do
            save(r)
        end
    end
end)
