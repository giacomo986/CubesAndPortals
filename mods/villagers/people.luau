--!strict
-- villagers: the world, the people, their day and works, homes and the watch. Part of the mod (init.luau says the order).

local common = require("./common")
local types = require("./types")
type V = types.V
type Box = types.Box
type Trust = types.Trust

local KINDS = { "human", "toad", "mole", "woodcutter", "miner", "digger" }
for _, k in KINDS do
    local model = core.register_model(`villagers:{k}`, `{k}.gltf`)
    -- Never fights, opens doors to get in and out of its house.
    core.register_mob(`villagers:{k}`, model, 20, 2.4, "worker", 0, 1.6, 1.0, "", 1, 0, true)
end

local JOB = { human = "farmer", toad = "fisher", mole = "forager", woodcutter = "woodcutter", miner = "miner",
              digger = "digger" }

-- The watch: workers like the rest, that fight when sent (core.mob_hunt,
-- core.mob_attack) -- the guard with its sword, the archer with arrows. Strong:
-- most of a blow goes through armour, and a guard fells a player in diamond
-- in four (8 a blow, 0.6 of it piercing: 6 of 20 in diamond, 8 in nothing).
local ARROW = core.model_id("mobs:arrow")
core.register_mob("villagers:guard", core.register_model("villagers:guard", "guard.gltf"), 60, 3.6,
    "worker", 8, 2.0, 0.8, "", 1, 0, true)
core.mob_pierce("villagers:guard", 0.6)
core.register_mob("villagers:archer", core.register_model("villagers:archer", "archer.gltf"), 40, 3.2,
    "worker", 6, 16.0, 1.4, "", 1, ARROW, true)
core.mob_pierce("villagers:archer", 0.5)
core.register_mob("villagers:inspector", core.register_model("villagers:inspector", "inspector.gltf"), 20,
    2.6, "worker", 0, 1.6, 1.0, "", 1, 0, true)
-- And the village's mayor, who sells its land, its builder, its merchant
-- and its crafter, who makes the tools and torches the others use; and its
-- museum's curator (museum.luau).
for _, k in { "mayor", "builder", "merchant", "crafter", "curator" } do
    core.register_mob(`villagers:{k}`, core.register_model(`villagers:{k}`, `{k}.gltf`), 20, 2.4, "worker", 0,
        1.6, 1.0, "", 1, 0, true)
end
local HOLDS = { guard = "tools:sword", archer = "archery:bow", inspector = "", mayor = "", builder = "",
                merchant = "", crafter = "", curator = "" }

-- The tool each worker works with, by kind. It wears out and comes from the
-- village's chests, where the crafter puts new ones; without one, every work
-- takes twice as long. TOOL is the one a worker starts with.
local TOOL = { human = "farm:hoe", woodcutter = "tools:axe", miner = "tools:pick", digger = "tools:shovel" }

-- And every material of it, best first: a worker takes the best the chests
-- hold. Its material says how long a work takes (pace) and how many works it
-- lasts (uses: stone's 60, the rest in proportion to the classic block game's
-- durability). One table, because the main chunk is at Luau's 200 locals.
local TOOLING = {
    ladder = {
        human = { "furnace:diamond_hoe", "furnace:iron_hoe", "furnace:gold_hoe", "tools:stone_hoe", "farm:hoe" },
        woodcutter = { "furnace:diamond_axe", "furnace:iron_axe", "furnace:gold_axe", "tools:axe", "tools:wood_axe" },
        miner = { "furnace:diamond_pick", "furnace:iron_pick", "furnace:gold_pick", "tools:pick", "tools:wood_pick" },
        digger = { "furnace:diamond_shovel", "furnace:iron_shovel", "furnace:gold_shovel", "tools:shovel",
                   "tools:wood_shovel" },
    } :: { [string]: { string } },
    pace = { wood = 1.5, stone = 1, iron = 0.7, gold = 0.6, diamond = 0.5 } :: { [string]: number },
    uses = { wood = 27, stone = 60, iron = 115, gold = 15, diamond = 715 } :: { [string]: number },
    of = nil :: any,
    material = nil :: any,
}
-- What a tool is made of, from its name.
TOOLING.material = function(item: string): string
    for _, m in { "diamond", "iron", "gold", "wood", "stone" } do
        if string.find(item, m, 1, true) then
            return m
        end
    end
    return if item == "farm:hoe" then "wood" else "stone"
end
-- The tool a worker has, the best of them; nil for none.
TOOLING.of = function(v: any): string?
    for _, t in TOOLING.ladder[v.kind] or {} do
        if (v.inv[t] or 0) > 0 then
            return t
        end
    end
    return nil
end

-- ---------------------------------------------------------------- the world

local AIR = 0
local function id(name: string): number
    return core.node_id(name)
end

-- A cell a body stands in: two cells it can be in over one it stands on.
local function free(x: number, y: number, z: number): boolean
    local a = core.get_block(x, y, z)
    return core.node_solid(a) ~= 1 and core.node_name(a):sub(1, 6) ~= "lakes:"
end
local function standable(x: number, y: number, z: number): boolean
    return core.node_solid(core.get_block(x, y - 1, z)) == 1 and free(x, y, z) and free(x, y + 1, z)
end

-- Beside a cell, where a body stands to work on it: the nearest of the four
-- sides, at its level or one up or down.
local function beside(x: number, y: number, z: number, fx: number, fz: number): (number?, number?, number?)
    local best, bx, by, bz = math.huge, nil, nil, nil
    for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
        for dy = 0, 1 do
            for _, sy in { y + dy, y - dy } do
                local cx, cz = x + d[1], z + d[2]
                if standable(cx, sy, cz) then
                    local dd = (cx - fx) ^ 2 + (cz - fz) ^ 2
                    if dd < best then
                        best, bx, by, bz = dd, cx, sy, cz
                    end
                end
            end
        end
    end
    return bx, by, bz
end

-- The nearest of some blocks to (x, y, z), within r across and h up and down.
local function nearest(names: { string }, x: number, y: number, z: number, r: number, h: number,
                       skip: ((number, number, number) -> boolean)?): (number?, number?, number?)
    local best, bx, by, bz = math.huge, nil, nil, nil
    for _, name in names do
        local found = core.find_nodes(x - r, y - h, z - r, x + r, y + h, z + r, name)
        for i = 1, #found, 3 do
            local fx, fy, fz = found[i], found[i + 1], found[i + 2]
            local d = (fx - x) ^ 2 + (fy - y) ^ 2 * 2 + (fz - z) ^ 2
            if d < best and not (skip and skip(fx, fy, fz)) then
                best, bx, by, bz = d, fx, fy, fz
            end
        end
    end
    return bx, by, bz
end

-- ---------------------------------------------------------------- the people


local people: { [string]: V } = {}
common.clock = 0.0

local function count(v: V): number
    local n = 0
    for _, c in v.inv do
        n += c
    end
    return n
end
local function add(v: V, item: string, n: number)
    if item ~= "" and n > 0 then
        v.inv[item] = (v.inv[item] or 0) + n
    end
end
local function take(v: V, item: string, n: number): boolean
    if (v.inv[item] or 0) < n then
        return false
    end
    v.inv[item] -= n
    if v.inv[item] == 0 then
        v.inv[item] = nil
    end
    return true
end

-- What the inside of a coroutine waits on.
local function wait(v: V, seconds: number)
    v.wake = common.clock + seconds
    coroutine.yield()
end

-- Up a mine's shaft first when it is down there and going up (set with the
-- mines, below).
common.out_of_mine = function(_, _) end

-- Helpers kept in one table, not a local each: this file's main chunk is at
-- Luau's 200 locals. Doors (mods/doors): each half, shut and open, and what
-- it becomes opened; and a search of a box of any size.
local AID = { opened = {} :: { [number]: number }, is = {} :: { [number]: boolean } }
function AID.known()
    if next(AID.is) then
        return
    end
    local shut, top = core.node_id("doors:door"), core.node_id("doors:door_top")
    local open, open_top = core.node_id("doors:door_open"), core.node_id("doors:door_open_top")
    AID.opened[shut], AID.opened[top] = open, open_top
    for _, d in { shut, top, open, open_top } do
        AID.is[d] = true
    end
    AID.is[0] = nil
end
function AID.at(x: number, y: number, z: number): boolean
    AID.known()
    return AID.is[core.get_block(x, y, z)] == true
end
-- core.find_nodes over a box of any size, in tiles of 64 by 64: one search
-- looks at a million cells at most, and the box round a village of some size
-- is more -- refused whole, and its mine, its keepers' beds were not found.
function AID.find(x1: number, y1: number, z1: number, x2: number, y2: number, z2: number, name: string): { number }
    local out = {}
    for tx = math.min(x1, x2), math.max(x1, x2), 64 do
        for tz = math.min(z1, z2), math.max(z1, z2), 64 do
            local f = core.find_nodes(tx, y1, tz, math.min(tx + 63, math.max(x1, x2)), y2,
                math.min(tz + 63, math.max(z1, z2)), name)
            table.move(f, 1, #f, #out + 1, out)
        end
    end
    return out
end
-- Both halves of a shut door opened, keeping their turn.
function AID.open(x: number, y: number, z: number)
    AID.known()
    for cy = y - 1, y + 1 do
        local to = AID.opened[core.get_block(x, cy, z)]
        if to ~= nil and to ~= 0 then
            local f = core.get_facing(x, cy, z)
            core.set_block(x, cy, z, to)
            core.set_facing(x, cy, z, f)
        end
    end
end

-- To a cell, along the way the engine finds. True when it got there; false
-- after `limit` seconds, and the cell is not tried again for a while.
local function walk(v: V, x: number, y: number, z: number, limit: number?): boolean
    -- Never into a block (a chest, a table, a wall): to the free cell beside
    -- it nearest where it stands.
    if core.node_solid(core.get_block(x, y, z)) == 1 then
        local qx, _, qz = core.mob_pos(v.id)
        local sx, sy, sz = beside(x, y, z, qx or x, qz or z)
        if sx == nil or sy == nil or sz == nil then
            v.tried[`{x},{y},{z}`] = common.clock + 120
            return false
        end
        x, y, z = sx, sy, sz
    end
    if not v.climbing then
        common.out_of_mine(v, y)
    end
    core.mob_act(v.id, "", HOLDS[v.kind] or "")
    local ax, ay, az = core.mob_pos(v.id)
    if core.mob_goto(v.id, x, y, z) == 0 then
        return false
    end
    v.tx, v.ty, v.tz = x, y, z
    v.walking = true
    v.wake = common.clock + (limit or 30)
    coroutine.yield()
    v.walking = false
    local px, py, pz = core.mob_pos(v.id)
    if px == nil or py == nil or pz == nil then
        return false
    end
    local there = math.abs(px - (x + 0.5)) < 0.8 and math.abs(pz - (z + 0.5)) < 0.8 and math.abs(py - y) < 1.2
    if not there and ax ~= nil and ay ~= nil and az ~= nil and (px - ax) ^ 2 + (pz - az) ^ 2 < 1 then
        -- Stuck where it was (a step at a door its body will not take, a
        -- corner): put a few cells on along the way there is, then on foot --
        -- never past a door, which it would go through shut: no further than
        -- the cell before it, and if the door is the next cell and shut, it
        -- opens it (the engine shuts it behind, as one found open).
        local way = core.find_path(math.floor(px), math.floor(py + 0.01), math.floor(pz), x, y, z, 2, 1, 3, true, 2000)
        local i = math.min(4, #way // 3)
        for k = 2, i do
            local cx, cy, cz = way[k * 3 - 2], way[k * 3 - 1], way[k * 3]
            if AID.at(cx, cy, cz) or AID.at(cx, cy + 1, cz) then
                if k == 2 then
                    AID.open(cx, cy, cz)
                end
                i = k - 1
                break
            end
        end
        if i >= 2 then
            core.mob_place(v.id, way[i * 3 - 2] + 0.5, way[i * 3 - 1] + 0.02, way[i * 3] + 0.5)
            return false
        end
    end
    if not there then
        v.tried[`{x},{y},{z}`] = common.clock + 120
        core.mob_goto(v.id, math.floor(px), math.floor(py + 0.01), math.floor(pz))   -- stop trying
    end
    return there
end

local function act(v: V, clip: string, item: string, seconds: number, fx: number?, fz: number?)
    if fx ~= nil and fz ~= nil then
        core.mob_face(v.id, fx + 0.5, fz + 0.5)
    end
    core.mob_act(v.id, clip, item)
    wait(v, seconds)
    core.mob_act(v.id, "", HOLDS[v.kind] or "")
end

-- How many times as long a work takes: its tool worn by a use, or twice as
-- long without one.
local function pace(v: V): number
    if TOOL[v.kind] == nil then
        return 1
    end
    local tool = TOOLING.of(v)
    if tool == nil then
        return 2
    end
    local m = TOOLING.material(tool)
    v.uses = (v.uses or 0) + 1
    if (v.uses :: number) >= TOOLING.uses[m] then
        take(v, tool, 1)
        v.uses = 0
        core.log(`villagers: the {v.kind} at {v.key} wore out its {tool}`)
    end
    return TOOLING.pace[m]
end

-- To the village's chests for what it lacks -- its tool, torches down a mine,
-- food when hungry -- taken freely: the village's goods are its people's
-- (set with the chests, below). True when it went.
common.supply = function(_) return false end

local function failed(v: V, x: number, y: number, z: number): boolean
    return (v.tried[`{x},{y},{z}`] or 0) > common.clock
end

-- To work on a cell: to the side of it, or into it when it can be walked into.
local function reach(v: V, x: number, y: number, z: number): boolean
    if failed(v, x, y, z) then
        return false
    end
    if core.node_solid(core.get_block(x, y, z)) ~= 1 and standable(x, y, z) then
        return walk(v, x, y, z)
    end
    local px, _, pz = core.mob_pos(v.id)
    local sx, sy, sz = beside(x, y, z, px or x, pz or z)
    if sx == nil or sy == nil or sz == nil then
        v.tried[`{x},{y},{z}`] = common.clock + 120
        return false
    end
    local ok = walk(v, sx, sy, sz)
    if not ok then
        v.tried[`{x},{y},{z}`] = common.clock + 120
    end
    return ok
end

local function is_night(): boolean
    local t = core.time_of_day()
    return t < 0.23 or t > 0.78
end

local function home_table(v: V): (number?, number?, number?)
    return nearest({ "craft:table" }, v.hx, v.hy, v.hz, 5, 1)
end

-- ------------------------------------------------------------ the day's parts

local FOODS = { "pets:cooked_fish", "farm:bread", "plants:melon_slice", "animals:steak",
                "animals:cooked_pork", "animals:fried_egg" }
local COOKS = { ["pets:fish"] = "pets:cooked_fish", ["pets:carp"] = "pets:cooked_fish",
                ["pets:trout"] = "pets:cooked_fish", ["pets:cod"] = "pets:cooked_fish",
                ["pets:eel"] = "pets:cooked_fish", ["pets:tuna"] = "pets:cooked_fish", ["animals:beef"] = "animals:steak",
                ["animals:pork"] = "animals:cooked_pork", ["animals:egg"] = "animals:fried_egg" }
local HUNGRY_EVERY = 400   -- seconds: a third of a day

local function food_of(v: V): string?
    for _, f in FOODS do
        if (v.inv[f] or 0) > 0 then
            return f
        end
    end
    return nil
end

local function sleep(v: V)
    if walk(v, v.hx, v.hy, v.hz, 40) then
        -- Lying with its head on the pillow: the bed's other half.
        local px, py, pz = nearest({ "bed:bed_head" }, v.hx, v.hy, v.hz, 1, 0)
        if px ~= nil and py ~= nil and pz ~= nil then
            core.mob_face(v.id, px + 0.5, pz + 0.5)
        end
        core.mob_act(v.id, "lie", "")
    end
    while is_night() do
        wait(v, 5)
    end
    core.mob_act(v.id, "", HOLDS[v.kind] or "")
end

local function eat(v: V)
    local f = food_of(v)
    if f == nil then
        return
    end
    local tx, ty, tz = home_table(v)
    if tx ~= nil and ty ~= nil and tz ~= nil then
        reach(v, tx, ty, tz)
    end
    act(v, "eat", f, 4.8, tx, tz)
    take(v, f, 1)
    v.fed_at = common.clock
end

local function cook(v: V)
    local fx, fy, fz = nearest({ "furnace:furnace", "furnace:lit" }, v.hx, v.hy, v.hz, 40, 6)
    if fx == nil then
        fx, fy, fz = home_table(v)
    end
    if fx == nil or fy == nil or fz == nil or not reach(v, fx, fy, fz) then
        return
    end
    act(v, "cook", "", 6.4, fx, fz)
    for raw, done in COOKS do
        local n = v.inv[raw] or 0
        if n > 0 then
            take(v, raw, n)
            add(v, done, n)
        end
    end
    while (v.inv["farm:wheat"] or 0) >= 3 do
        take(v, "farm:wheat", 3)
        add(v, "farm:bread", 1)
    end
end

-- What it does not keep goes into the nearest chest near home.
local KEEP = 6
local function store(v: V)
    local cx, cy, cz = nearest({ "chest:box" }, v.hx, v.hy, v.hz, 32, 6)
    if cx == nil or cy == nil or cz == nil or not reach(v, cx, cy, cz) then
        return
    end
    act(v, "gather", "", 1.6, cx, cz)
    for item, n in table.clone(v.inv) do
        local spare = n - (if table.find(FOODS, item) or COOKS[item] then KEEP
            elseif item == TOOLING.of(v) then 1 elseif item == "torches:torch" and v.kind == "miner" then n else 0)
        if spare > 0 then
            local iid = core.item_id(item)
            local left = if iid ~= 0 then core.container_add(cx, cy, cz, iid, spare) else spare
            take(v, item, spare - left)
        end
    end
end

local function stroll(v: V)
    local a = math.random() * math.pi * 2
    local r = 3 + math.random() * 6
    local x, z = math.floor(v.hx + math.cos(a) * r), math.floor(v.hz + math.sin(a) * r)
    for dy = 3, -3, -1 do
        if standable(x, v.hy + dy, z) then
            walk(v, x, v.hy + dy, z, 15)
            break
        end
    end
    wait(v, 4 + math.random() * 6)
end

-- ---------------------------------------------------------------- the works

local WHEAT = { "farm:wheat_0", "farm:wheat_1", "farm:wheat_2", "farm:wheat_3" }

local function farm(v: V)
    -- Ripe wheat first: reaped, a sheaf and a seed or two, and sown again.
    local x, y, z = nearest({ "farm:wheat_3" }, v.hx, v.hy, v.hz, 24, 6, function(a, b, c)
        return failed(v, a, b, c)
    end)
    if x ~= nil and y ~= nil and z ~= nil then
        if reach(v, x, y, z) and core.get_block(x, y, z) == id("farm:wheat_3") then
            act(v, "gather", "", 2.0, x, z)
            core.set_block(x, y, z, id("farm:wheat_0"))
            add(v, "farm:wheat", 1)
            add(v, "animals:seeds", math.random(0, 1))
        end
        return
    end
    -- Bare soil: sown.
    x, y, z = nearest({ "farm:soil" }, v.hx, v.hy, v.hz, 24, 6, function(a, b, c)
        return core.get_block(a, b + 1, c) ~= AIR or failed(v, a, b + 1, c)
    end)
    if x ~= nil and y ~= nil and z ~= nil then
        if reach(v, x, y + 1, z) and core.get_block(x, y + 1, z) == AIR then
            act(v, "gather", "animals:seeds", 1.6, x, z)
            core.set_block(x, y + 1, z, id("farm:wheat_0"))
            take(v, "animals:seeds", 1)
        end
        return
    end
    -- Grass beside the field: tilled with the hoe.
    local sx, sy, sz = nearest({ "farm:wheat_1", "farm:wheat_2", "farm:wheat_0" }, v.hx, v.hy, v.hz, 24, 6)
    if sx ~= nil and sy ~= nil and sz ~= nil and math.random() < 0.3 then
        for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
            local gx, gz = sx + d[1], sz + d[2]
            if core.get_block(gx, sy - 1, gz) == id("core:grass") and core.get_block(gx, sy, gz) == AIR
                and reach(v, gx, sy, gz) then
                act(v, "dig", TOOLING.of(v) or "farm:hoe", 2.0 * pace(v), gx, gz)
                core.set_block(gx, sy - 1, gz, id("farm:soil"))
                return
            end
        end
    end
    stroll(v)
end

local function fish(v: V)
    -- What a toad brings up, by the water it fishes: mods/pets's own weights
    -- (kept in step by hand -- a mod cannot call another's function). Inside
    -- this function: the file is at Luau's 200 locals.
    local CATCHES: { [string]: { { any } } } = {
        ["core:river"] = { { "trout", 70 }, { "fish", 30 } }, ["core:gorge"] = { { "trout", 70 }, { "fish", 30 } },
        ["core:sea"] = { { "cod", 70 }, { "eel", 30 } }, ["core:beach"] = { { "cod", 70 }, { "eel", 30 } },
        ["core:ocean"] = { { "tuna", 50 }, { "cod", 50 } },
    }
    local function caught(x: number, z: number): string
        if math.random() < 0.03 then
            return "pets:goldfish"
        end
        local list = CATCHES[core.biome(x, z)] or { { "fish", 70 }, { "carp", 30 } }
        local r = math.random() * 100
        for _, k in list do
            r -= k[2]
            if r <= 0 then
                return "pets:" .. k[1]
            end
        end
        return "pets:" .. list[1][1]
    end

    -- A cell of open water with a bank beside it, level with the water.
    local water = core.find_nodes(v.hx - 24, v.hy - 8, v.hz - 24, v.hx + 24, v.hy + 3, v.hz + 24, "lakes:water")
    local px, _, pz = core.mob_pos(v.id)
    local best, wx, wy, wz, sx, sy, sz = math.huge, 0, 0, 0, 0, 0, 0
    for i = 1, #water, 3 do
        local x, y, z = water[i], water[i + 1], water[i + 2]
        if core.get_block(x, y + 1, z) == AIR then
            for _, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
                local bx, bz = x + d[1] * 1, z + d[2] * 1
                if standable(bx, y + 1, bz) and not failed(v, bx, y + 1, bz) then
                    local dd = (bx - (px or v.hx)) ^ 2 + (bz - (pz or v.hz)) ^ 2
                    if dd < best then
                        best, wx, wy, wz, sx, sy, sz = dd, x, y, z, bx, y + 1, bz
                    end
                end
            end
        end
    end
    if best == math.huge then
        stroll(v)
        return
    end
    if not walk(v, sx, sy, sz) then
        return
    end
    -- Its float on the water while it waits; the bite pulls it under.
    local FLOAT, BITE = id("pets:bobber"), id("pets:bobber_bite")
    for _ = 1, 3 do
        core.mob_face(v.id, wx + 0.5, wz + 0.5)
        core.mob_act(v.id, "fish", "pets:rod")
        wait(v, 1.2)
        local floated = core.get_block(wx, wy + 1, wz) == AIR
        if floated then
            core.set_block(wx, wy + 1, wz, FLOAT)
        end
        wait(v, 5 + math.random() * 8)
        if floated and core.get_block(wx, wy + 1, wz) == FLOAT then
            core.set_block(wx, wy + 1, wz, BITE)
        end
        wait(v, 0.8)
        if floated and (core.get_block(wx, wy + 1, wz) == BITE or core.get_block(wx, wy + 1, wz) == FLOAT) then
            core.set_block(wx, wy + 1, wz, AIR)
        end
        if math.random() < 0.7 then
            add(v, caught(wx, wz), 1)
        end
        if is_night() then
            break
        end
    end
    core.mob_act(v.id, "", HOLDS[v.kind] or "")
end

-- What a forager picks, and what it gives.
local PICKS = {
    ["plants:melon"] = function(): (string, number) return "plants:melon_slice", math.random(3, 6) end,
    ["plants:brown_mushroom"] = function(): (string, number) return "plants:brown_mushroom", 1 end,
    ["plants:red_mushroom"] = function(): (string, number) return "plants:red_mushroom", 1 end,
    ["plants:tall_grass"] = function(): (string, number) return "animals:seeds", math.random(0, 1) end,
    ["plants:fern"] = function(): (string, number) return "animals:seeds", math.random(0, 1) end,
}
local PICK_NAMES = {}
for name in PICKS do
    table.insert(PICK_NAMES, name)
end
table.sort(PICK_NAMES)

local function forage(v: V)
    local x, y, z = nearest(PICK_NAMES, v.hx, v.hy, v.hz, 28, 8, function(a, b, c)
        return failed(v, a, b, c)
    end)
    if x == nil or y == nil or z == nil then
        stroll(v)
        return
    end
    local was = core.get_block(x, y, z)
    if not reach(v, x, y, z) or core.get_block(x, y, z) ~= was then
        return
    end
    act(v, "gather", "", 2.4, x, z)
    if core.get_block(x, y, z) == was then
        core.set_block(x, y, z, AIR)
        local item, n = PICKS[core.node_name(was)]()
        add(v, item, n)
    end
end

local WORK = { farmer = farm, fisher = fish, forager = forage }

-- ---------------------------------------------------------------- a life

local function life(v: V)
    while true do
        if AID.ev.feasting(v) then
            -- at the feast, in the square
        elseif is_night() then
            sleep(v)
        elseif common.clock - v.fed_at > HUNGRY_EVERY and food_of(v) ~= nil then
            eat(v)
        elseif common.supply(v) then
            -- went to the chests
        elseif next(v.inv) ~= nil and (function()
                for raw in COOKS do
                    if (v.inv[raw] or 0) >= 3 then return true end
                end
                return (v.inv["farm:wheat"] or 0) >= 6
            end)() then
            cook(v)
        elseif count(v) > 24 then
            store(v)
        else
            WORK[JOB[v.kind]](v)
        end
        wait(v, 0.5)
    end
end

common.watch_life = life   -- set below, with the watch

local function live(v: V)
    if HOLDS[v.kind] ~= nil then
        common.watch_life(v)
    else
        life(v)
    end
end

local function come_out(v: V)
    local nid = core.spawn_mob(`villagers:{v.kind}`, v.hx + 0.5, v.hy + 1, v.hz + 0.5)
    if nid == 0 then
        return
    end
    v.id = nid
    v.wake = common.clock
    v.walking = false
    v.hunting, v.prey = nil, nil
    v.co = coroutine.create(function() live(v) end)
    if v.kind == "mayor" or v.kind == "builder" or v.kind == "merchant" or v.kind == "curator" then
        core.log(`villagers: the {v.kind} of {v.rk} is creature {nid}`)
    end
    if HOLDS[v.kind] ~= nil and HOLDS[v.kind] ~= "" then
        core.mob_act(v.id, "", HOLDS[v.kind])
    end
end

-- ---------------------------------------------------------------- homes

local function hash(x: number, y: number, z: number): number
    return (x * 73856093 + y * 19349663 + z * 83492791) % 1000003
end

-- A home's kind: a toad where there is water near, a mole now and then and
-- more often in the desert and the cold, a human otherwise.
local function kind_for(x: number, y: number, z: number): string
    local h = hash(x, y, z)
    local wet = #core.find_nodes(x - 16, y - 6, z - 16, x + 16, y + 2, z + 16, "lakes:water") > 0
    local biome = core.biome(x, z)
    if wet and h % 3 == 0 then
        return "toad"
    elseif h % 5 == 0 or ((biome == "core:desert" or biome == "core:tundra") and h % 2 == 0) then
        return "mole"
    end
    -- Those who bring the builder what it builds with: not in every village.
    return ({ "woodcutter", "miner", "digger" })[h % 9] or "human"
end

-- Homes are kept a key per region of 256 by 256 (a village is in one), and
-- a key that lists the regions: some 180 homes a region and 400 regions.
local REGION = 256
local function region_of(x: number, z: number): string
    return `{math.floor(x / REGION)},{math.floor(z / REGION)}`
end

-- A village's treasury, a storage key: every ruby paid to it (land,
-- houses, guards, its goods) and every one its miners dig; what it buys from
-- players is paid out of it and it buys nothing with it empty. Rubies come
-- into the world only as ore. A village starts with 32.
local function treasury(rk: string): number
    return tonumber(core.storage_get(`treasury {rk}`)) or 32
end
local function to_treasury(rk: string, n: number)
    core.storage_set(`treasury {rk}`, tostring(math.max(0, treasury(rk) + n)))
end

local function save_homes()
    local by_region: { [string]: { string } } = {}
    for key, v in people do
        local rk = region_of(v.hx, v.hz)
        by_region[rk] = by_region[rk] or {}
        table.insert(by_region[rk], `{key},{v.kind}`)
    end
    local regions = {}
    for rk in string.gmatch(core.storage_get("regions"), "[^;]+") do
        if not by_region[rk] then
            core.storage_set(`homes {rk}`, "")   -- every home there gone
        end
    end
    for rk, parts in by_region do
        table.sort(parts)
        table.insert(regions, rk)
        core.storage_set(`homes {rk}`, table.concat(parts, ";"))
    end
    table.sort(regions)
    core.storage_set("regions", table.concat(regions, ";"))
end

local function add_home(x: number, y: number, z: number, kind: string)
    local key = `{x},{y},{z}`
    if people[key] then
        return
    end
    people[key] = { key = key, hx = x, hy = y, hz = z, kind = kind, id = 0, back_at = 0,
                    inv = if TOOL[kind] then { [TOOL[kind]] = 1 } else {},
                    fed_at = common.clock, wake = 0, walking = false, tx = 0, ty = 0, tz = 0, tried = {} }
end

local function read_homes(list: string)
    for part in string.gmatch(list, "[^;]+") do
        local x, y, z, kind = string.match(part, "^(-?%d+),(-?%d+),(-?%d+),(%a+)$")
        if x and y and z and kind and JOB[kind] then
            add_home(tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number, kind)
        end
    end
end

core.event.on("core:world_ready", function()
    for rk in string.gmatch(core.storage_get("regions"), "[^;]+") do
        read_homes(core.storage_get(`homes {rk}`))
    end
    -- A world saved when every home was in the one key "homes".
    local old = core.storage_get("homes")
    if old ~= "" then
        read_homes(old)
        core.storage_set("homes", "")
        save_homes()
    end
end)

-- Whose a keeper's bed is, by the sign of the building it is in (mods/
-- villages) at most 4 across and up to 9 under it: the hall's bell the
-- mayor's, the barracks' banner the watch's, the scales the merchant's, the
-- plans the builder's, a smithy's anvil the crafter's. Those beds are not
-- homes.
-- (One table for the keepers' signs and buildings: the file is at Luau's
-- 200 locals.)
local KEEPING = {
    marks = { "villages:bell", "villages:banner", "villages:scales", "villages:plans", "building:anvil",
              "villagers:curio" },
    -- The signs of the keepers' small houses (order_post, below).
    keep = { keep_scales = "villages:scales", keep_plans = "villages:plans", keep_anvil = "building:anvil",
             keep_museum = "villagers:curio" },
    -- Which sign each keeper's bed should be under.
    role = { mayor = "villages:bell", builder = "villages:plans", merchant = "villages:scales",
             crafter = "building:anvil", inspector = "villages:banner", guard = "villages:banner",
             archer = "villages:banner", curator = "villagers:curio" },
    -- Where a village's keepers' buildings are looked for: its box (its
    -- homes and REACH round them) and this much further -- the hall stands
    -- by the square, which may be past the homes on one side.
    reach = 24,
    -- What a keeper missing its building has the builder put up (order_post):
    -- the hall, barracks, or a small house with the sign.
    posts = {
        ["villages:bell"] = { kind = "hall", w = 9, d = 7, what = "the village's hall" },
        ["villages:banner"] = { kind = "barracks", w = 9, d = 7, what = "the watch's barracks" },
        ["villages:scales"] = { kind = "keep_scales", w = 5, d = 5, what = "the merchant's house" },
        ["villages:plans"] = { kind = "keep_plans", w = 5, d = 5, what = "the builder's house" },
        ["building:anvil"] = { kind = "keep_anvil", w = 5, d = 5, what = "the crafter's house" },
        -- Seven by seven: wider than eight would be two storeys (order_post),
        -- and its sign more than 4 from the bed (bed_mark).
        ["villagers:curio"] = { kind = "keep_museum", w = 7, d = 7, what = "the village's museum" },
    },
}
local function bed_mark(x: number, y: number, z: number): string?
    for _, m in KEEPING.marks do
        if #core.find_nodes(x - 4, y - 9, z - 4, x + 4, y, z + 4, m) > 0 then
            return m
        end
    end
    return nil
end

-- Beds near players, and whether each is a home.
local function look_for_homes()
    local added = false
    for _, p in core.players() do
        local px, py, pz = core.player_pos(p)
        if px ~= nil and py ~= nil and pz ~= nil then
            px, py, pz = math.floor(px), math.floor(py), math.floor(pz)
            local beds = core.find_nodes(px - 40, py - 12, pz - 40, px + 40, py + 12, pz + 40, "bed:bed")
            for i = 1, #beds, 3 do
                local x, y, z = beds[i], beds[i + 1], beds[i + 2]
                local owner = core.owned_at(x, z)
                if not people[`{x},{y},{z}`] and not (owner >= 32768 and owner < 65536)   -- a player's
                    and #core.find_nodes(x - 5, y - 1, z - 5, x + 5, y + 1, z + 5, "craft:table") > 0
                    and bed_mark(x, y, z) == nil then
                    add_home(x, y, z, kind_for(x, y, z))
                    added = true
                end
            end
        end
    end
    if added then
        save_homes()
    end
end

local function near_player(v: V, r: number): boolean
    for _, p in core.players() do
        local px, py, pz = core.player_pos(p)
        if px ~= nil and py ~= nil and pz ~= nil and (px - v.hx) ^ 2 + (py - v.hy) ^ 2 + (pz - v.hz) ^ 2 < r * r then
            return true
        end
    end
    return false
end

-- ---------------------------------------------------------------- the watch

-- The number a village's goods carry once taken (core.owned_area), one per
-- region: the top half of the range, clear of mods/backpack's numbers counted
-- up from 1. Regions -128..127 each way, wrapped beyond (400 are in use).
local function tag_of(rk: string): number
    local rx, rz = string.match(rk, "^(-?%d+),(-?%d+)$")
    return 131071 - ((tonumber(rx) :: number + 128) % 256 * 256 + (tonumber(rz) :: number + 128) % 256)
end
local DAY = 1200
local FORGIVE = 3 * DAY
local REACH = 12   -- a village is its homes and this much round them

-- The world's age in seconds, across restarts: what three days are counted on.
common.age_at_start = 0.0
local function age(): number
    return common.age_at_start + common.clock
end

-- Each village's plan as mods/villages laid it out: the box round all its
-- streets and buildings, which its ground always covers -- the homes alone
-- left out a farm, a tower, an empty house at a street's end.
local extents: { [string]: { number } } = {}
local EDGE = 4   -- and this much round them
core.event.on("villages:extent", function(ev)
    extents[`{ev.rx},{ev.rz}`] = { ev.x1, ev.z1, ev.x2, ev.z2 }
end)

common.villages = {}
local owned: { [string]: string } = {}
-- The lots a village sold (below), which its ground grows to take in.
local lots: { [string]: { { number } } } = {}
-- Its mine (mods/villages): the column of rungs of its shaft, top and bottom,
-- found once; its ground takes in the galleries dug from it.
local mines: { [string]: { number } } = {}
local mine_looked: { [string]: number } = {}
local MINE_REACH = 16
local function find_mine(rk: string, b: Box)
    if mines[rk] ~= nil or (mine_looked[rk] or -999) > common.clock - 60 then
        return
    end
    mine_looked[rk] = common.clock
    local cols: { [string]: { number } } = {}
    local f = AID.find(b.x1 - 30, b.y - 24, b.z1 - 30, b.x2 + 30, b.y + 8, b.z2 + 30, "timber:rungs")
    for i = 1, #f, 3 do
        local k = `{f[i]},{f[i + 2]}`
        local c = cols[k] or { f[i], f[i + 1], f[i + 2], f[i + 1], 0 }
        c[2], c[4], c[5] = math.min(c[2], f[i + 1]), math.max(c[4], f[i + 1]), c[5] + 1
        cols[k] = c
    end
    for _, c in cols do
        if c[5] >= 10 then
            mines[rk] = { c[1], c[2], c[3], c[4] }   -- x, bottom, z, top
            core.log(`villagers: {rk}: its mine at {c[1]} {c[4]} {c[3]}, down to {c[2]}`)
            return
        end
    end
end

local function survey()
    local now: { [string]: Box } = {}
    for _, v in people do
        local rk = region_of(v.hx, v.hz)
        local b = now[rk]
        if b == nil then
            b = { x1 = v.hx, z1 = v.hz, x2 = v.hx, z2 = v.hz, y = v.hy, homes = {} }
            now[rk] = b
        end
        b.x1, b.z1 = math.min(b.x1, v.hx), math.min(b.z1, v.hz)
        b.x2, b.z2 = math.max(b.x2, v.hx), math.max(b.z2, v.hz)
        table.insert(b.homes, v)
    end
    for rk, b in now do
        b.x1, b.z1, b.x2, b.z2 = b.x1 - REACH, b.z1 - REACH, b.x2 + REACH, b.z2 + REACH
        local e = extents[rk]
        if e ~= nil then
            b.x1, b.z1 = math.min(b.x1, e[1] - EDGE), math.min(b.z1, e[2] - EDGE)
            b.x2, b.z2 = math.max(b.x2, e[3] + EDGE), math.max(b.z2, e[4] + EDGE)
        end
        find_mine(rk, b)
        local mn = mines[rk]
        if mn ~= nil then
            b.x1, b.z1 = math.min(b.x1, mn[1] - MINE_REACH), math.min(b.z1, mn[3] - MINE_REACH)
            b.x2, b.z2 = math.max(b.x2, mn[1] + MINE_REACH), math.max(b.z2, mn[3] + MINE_REACH)
        end
        for _, l in lots[rk] or {} do
            b.x1, b.z1 = math.min(b.x1, l[1] - 2), math.min(b.z1, l[2] - 2)
            b.x2, b.z2 = math.max(b.x2, l[3] + 2), math.max(b.z2, l[4] + 2)
        end
        local corners = `{b.x1},{b.z1},{b.x2},{b.z2}`
        -- Refused (0) before the server takes players: said again next time.
        if owned[rk] ~= corners and core.owned_area(b.x1, b.z1, b.x2, b.z2, tag_of(rk)) == 1 then
            local old = owned[rk]
            if old ~= nil then
                local a, c, d, e = string.match(old, "^(-?%d+),(-?%d+),(-?%d+),(-?%d+)$")
                core.owned_area(tonumber(a) :: number, tonumber(c) :: number, tonumber(d) :: number,
                    tonumber(e) :: number, 0)
            end
            owned[rk] = corners
        end
    end
    common.villages = now
    for rk, b in now do
        AID.err.saw(rk, b)
    end
end

local function village_at(x: number, z: number): string?
    for rk, b in common.villages do
        if x >= b.x1 and x <= b.x2 and z >= b.z1 and z <= b.z2 then
            return rk
        end
    end
    return nil
end

local trust: { [string]: { [string]: Trust } } = {}

local function save_trust(rk: string)
    local parts = {}
    for name, t in trust[rk] or {} do
        if t.strikes > 0 then
            table.insert(parts, `{t.strikes},{math.floor(t.last)},{if t.hostile then 1 else 0},{name}`)
        end
    end
    table.sort(parts)
    core.storage_set(`trust {rk}`, table.concat(parts, ";"))
end

local function trust_of(rk: string, name: string): Trust
    local by = trust[rk]
    if by == nil then
        by = {}
        trust[rk] = by
        for part in string.gmatch(core.storage_get(`trust {rk}`), "[^;]+") do
            local n, last, h, who = string.match(part, "^(%d+),(-?%d+),(%d),(.+)$")
            if n and last and h and who then
                by[who] = { strikes = tonumber(n) :: number, last = tonumber(last) :: number, hostile = h == "1" }
            end
        end
    end
    local t = by[name]
    if t == nil then
        t = { strikes = 0, last = 0, hostile = false }
        by[name] = t
    elseif t.strikes > 0 and age() - t.last > FORGIVE then
        t.strikes, t.hostile = 0, false   -- forgotten
        save_trust(rk)
    end
    return t
end

local function where(p: number): (number?, number?, number?)
    return core.player_pos(p)
end

-- The player the watch of that village is after now: one it is hostile to,
-- standing in it. The nearest of them to (x, z).
local function wanted(rk: string, x: number, z: number): number?
    local best, who = math.huge, nil
    for _, p in core.players() do
        local px, _, pz = where(p)
        if px ~= nil and pz ~= nil and village_at(px, pz) == rk and trust_of(rk, core.player_name(p)).hostile then
            local d = (px - x) ^ 2 + (pz - z) ^ 2
            if d < best then
                best, who = d, p
            end
        end
    end
    return who
end

local function stolen_count(p: number, rk: string): number
    local list = core.bag_tagged(p, tag_of(rk))
    local n = 0
    for i = 2, #list, 2 do
        n += list[i]
    end
    return n
end

local watch: { [string]: V } = {}

-- A cell on the ground near one of the village's homes, to walk to.
local function somewhere_in(rk: string): (number?, number?, number?)
    local b = common.villages[rk]
    if b == nil then
        return nil
    end
    local h = b.homes[math.random(1, #b.homes)]
    local a = math.random() * math.pi * 2
    local r = 4 + math.random() * 5
    local x, z = math.floor(h.hx + math.cos(a) * r), math.floor(h.hz + math.sin(a) * r)
    for dy = 4, -4, -1 do
        if standable(x, h.hy + dy, z) then
            return x, h.hy + dy, z
        end
    end
    return nil
end

local function patrol(v: V)
    local x, y, z = somewhere_in(v.rk :: string)
    if x ~= nil and y ~= nil and z ~= nil then
        walk(v, x, y, z, 20)
    end
    wait(v, 2 + math.random() * 5)
end

-- What the files after this one use of it.
common.act = act
common.add = add
common.age = age
common.AID = AID
common.AIR = AIR
common.ARROW = ARROW
common.bed_mark = bed_mark
common.beside = beside
common.come_out = come_out
common.DAY = DAY
common.extents = extents
common.failed = failed
common.food_of = food_of
common.FOODS = FOODS
common.free = free
common.HOLDS = HOLDS
common.HUNGRY_EVERY = HUNGRY_EVERY
common.id = id
common.is_night = is_night
common.JOB = JOB
common.KEEPING = KEEPING
common.live = live
common.look_for_homes = look_for_homes
common.lots = lots
common.MINE_REACH = MINE_REACH
common.mines = mines
common.near_player = near_player
common.nearest = nearest
common.pace = pace
common.patrol = patrol
common.people = people
common.reach = reach
common.region_of = region_of
common.save_homes = save_homes
common.save_trust = save_trust
common.somewhere_in = somewhere_in
common.standable = standable
common.stolen_count = stolen_count
common.stroll = stroll
common.survey = survey
common.tag_of = tag_of
common.take = take
common.to_treasury = to_treasury
common.TOOL = TOOL
common.TOOLING = TOOLING
common.treasury = treasury
common.trust = trust
common.trust_of = trust_of
common.village_at = village_at
common.wait = wait
common.walk = walk
common.wanted = wanted
common.watch = watch
common.where = where
common.WORK = WORK
