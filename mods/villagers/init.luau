--!strict
-- villagers: the people who live in the sandbox's houses.
--
-- Every bed with a crafting table within five cells of it is a HOME -- the
-- houses mods/villages builds, or one a player builds -- and each home has one
-- inhabitant, of one of three kinds made in Blender on the robot's skeleton
-- (tools/make_villager_models.py):
--
--   human  a farmer: reaps the ripe wheat near home, sows it again, tills
--          the empty soil, and bakes bread out of three sheaves.
--   toad   a fisher: walks to the water's edge, casts (the float of
--          mods/pets sits on the water) and now and then brings up a fish.
--   mole   a forager: picks melons, mushrooms and seeds from the grass.
--
-- Whatever its work, each one keeps the same day: works from dawn, cooks what
-- it caught or picked at the village's furnace (or its own table when there is
-- none) around noon, eats when it is hungry, puts what it does not need in the
-- chest of its house (the nearest to its bed), and at dusk goes home to sleep in its bed. Struck down, it is
-- back in its bed five minutes later.
--
-- A village keeps a WATCH, at the house nearest its middle: guards in iron
-- with swords, archers in green hoods, and an inspector with a ledger who
-- walks up to whoever comes by and looks in their bag. What a player takes
-- out of a village's chests, reaps in its fields or breaks off its houses is
-- marked stolen (core.owned_area). Caught with it a first time, they may give
-- it back -- it goes into the village's chest -- and the village trusts them
-- no more; refused, or caught again, the guards and archers go for them while
-- they are in the village, until they die. Three days without a theft caught
-- and the village forgets. Each village marks with its own number, and only
-- its own inspector asks after it; what a player built there is theirs. By
-- night the watch fights whatever comes out of the dark within 8 of the
-- village.
--
-- The engine walks them (a "worker" creature, core.mob_goto) and shows what
-- they do (core.mob_act: a clip and a thing in the hand); this file is what
-- they decide. Each inhabitant is a coroutine that says its day in order and
-- yields while it walks or works.

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
-- and its crafter, who makes the tools and torches the others use.
for _, k in { "mayor", "builder", "merchant", "crafter" } do
    core.register_mob(`villagers:{k}`, core.register_model(`villagers:{k}`, `{k}.gltf`), 20, 2.4, "worker", 0,
        1.6, 1.0, "", 1, 0, true)
end
local HOLDS = { guard = "tools:sword", archer = "archery:bow", inspector = "", mayor = "", builder = "",
                merchant = "", crafter = "" }

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

type V = {
    key: string, hx: number, hy: number, hz: number, kind: string,
    id: number, back_at: number, inv: { [string]: number }, fed_at: number,
    co: thread?, wake: number, walking: boolean,
    tx: number, ty: number, tz: number,   -- where it was sent
    tried: { [string]: number },          -- cells it could not reach, until then
    rk: string?, hunting: number?,        -- the watch's: its village, who it is after,
    prey: number?,                        -- and what creature
    task: string?,                        -- what a gatherer is fetching (wood, stone, earth)
    climbing: boolean?,                   -- on its way up or down a mine's shaft
    uses: number?,                        -- works done with the tool in hand
    supplied: number?,                    -- when it last went to the chests for what it lacks
    bed: { number }?,                     -- a keeper's: its bed, and the sign of the building
    bed_mark: string?,                    -- it is in ("" none; see bed_mark)
    sleeping: boolean?,
}

local people: { [string]: V } = {}
local clock = 0.0

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
    v.wake = clock + seconds
    coroutine.yield()
end

-- Up a mine's shaft first when it is down there and going up (set with the
-- mines, below).
local out_of_mine: (V, number) -> () = function(_, _) end

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
            v.tried[`{x},{y},{z}`] = clock + 120
            return false
        end
        x, y, z = sx, sy, sz
    end
    if not v.climbing then
        out_of_mine(v, y)
    end
    core.mob_act(v.id, "", HOLDS[v.kind] or "")
    local ax, ay, az = core.mob_pos(v.id)
    if core.mob_goto(v.id, x, y, z) == 0 then
        return false
    end
    v.tx, v.ty, v.tz = x, y, z
    v.walking = true
    v.wake = clock + (limit or 30)
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
        v.tried[`{x},{y},{z}`] = clock + 120
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
local supply: (V) -> boolean = function(_) return false end

local function failed(v: V, x: number, y: number, z: number): boolean
    return (v.tried[`{x},{y},{z}`] or 0) > clock
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
        v.tried[`{x},{y},{z}`] = clock + 120
        return false
    end
    local ok = walk(v, sx, sy, sz)
    if not ok then
        v.tried[`{x},{y},{z}`] = clock + 120
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
    v.fed_at = clock
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
        elseif clock - v.fed_at > HUNGRY_EVERY and food_of(v) ~= nil then
            eat(v)
        elseif supply(v) then
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

local watch_life: (V) -> () = life   -- set below, with the watch

local function live(v: V)
    if HOLDS[v.kind] ~= nil then
        watch_life(v)
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
    v.wake = clock
    v.walking = false
    v.hunting, v.prey = nil, nil
    v.co = coroutine.create(function() live(v) end)
    if v.kind == "mayor" or v.kind == "builder" or v.kind == "merchant" then
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

-- A village's treasury, a storage key: every emerald paid to it (land,
-- houses, guards, its goods) and every one its miners dig; what it buys from
-- players is paid out of it and it buys nothing with it empty. Emeralds come
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
                    fed_at = clock, wake = 0, walking = false, tx = 0, ty = 0, tz = 0, tried = {} }
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
    marks = { "villages:bell", "villages:banner", "villages:scales", "villages:plans", "building:anvil" },
    -- The signs of the keepers' small houses (order_post, below).
    keep = { keep_scales = "villages:scales", keep_plans = "villages:plans", keep_anvil = "building:anvil" },
    -- Which sign each keeper's bed should be under.
    role = { mayor = "villages:bell", builder = "villages:plans", merchant = "villages:scales",
             crafter = "building:anvil", inspector = "villages:banner", guard = "villages:banner",
             archer = "villages:banner" },
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
local age_at_start = 0.0
local function age(): number
    return age_at_start + clock
end

-- Each village's plan as mods/villages laid it out: the box round all its
-- streets and buildings, which its ground always covers -- the homes alone
-- left out a farm, a tower, an empty house at a street's end.
local extents: { [string]: { number } } = {}
local EDGE = 4   -- and this much round them
core.event.on("villages:extent", function(ev)
    extents[`{ev.rx},{ev.rz}`] = { ev.x1, ev.z1, ev.x2, ev.z2 }
end)

-- Each village's box, the corners last given to core.owned_area.
type Box = { x1: number, z1: number, x2: number, z2: number, y: number, homes: { V } }
local villages: { [string]: Box } = {}
local owned: { [string]: string } = {}
-- The lots a village sold (below), which its ground grows to take in.
local lots: { [string]: { { number } } } = {}
-- Its mine (mods/villages): the column of rungs of its shaft, top and bottom,
-- found once; its ground takes in the galleries dug from it.
local mines: { [string]: { number } } = {}
local mine_looked: { [string]: number } = {}
local MINE_REACH = 16
local function find_mine(rk: string, b: Box)
    if mines[rk] ~= nil or (mine_looked[rk] or -999) > clock - 60 then
        return
    end
    mine_looked[rk] = clock
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
    villages = now
    for rk, b in now do
        AID.err.saw(rk, b)
    end
end

local function village_at(x: number, z: number): string?
    for rk, b in villages do
        if x >= b.x1 and x <= b.x2 and z >= b.z1 and z <= b.z2 then
            return rk
        end
    end
    return nil
end

-- What a village thinks of a player, by name: how many times caught, when
-- last, and whether its watch is after them. Kept a key per village.
type Trust = { strikes: number, last: number, hostile: boolean }
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
    local b = villages[rk]
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

-- ------------------------------------------------ the inspector and the bag

local CAUGHT = "villagers:caught"
core.ui_define(CAUGHT, 12, 4.6)
core.ui_label(CAUGHT, 0.4, 0.3, "The inspector found stolen goods in your bag.")
core.ui_label(CAUGHT, 0.4, 1.1, "Give them back? The village will not trust you again.")
core.ui_button(CAUGHT, "back", 0.4, 2.4, 5.4, 1.4, "Give them back")
core.ui_button(CAUGHT, "keep", 6.2, 2.4, 5.4, 1.4, "Refuse")

-- Who is being asked, by which village, until when.
local asked: { [number]: { rk: string, by: V, til: number } } = {}
local looked: { [string]: number } = {}   -- "rk name": not looked in again before

local function turn_hostile(p: number, rk: string)
    local t = trust_of(rk, core.player_name(p))
    t.strikes, t.hostile, t.last = math.max(t.strikes, 2), true, age()
    save_trust(rk)
    core.notice(p, "Thief! The watch is after you while you are in this village.")
end

-- What was stolen, out of the bag and into the chest nearest the inspector.
local function give_back(p: number, v: V)
    local tag = tag_of(v.rk :: string)
    local list = core.bag_tagged(p, tag)
    core.take_tagged(p, tag)
    local px, py, pz = core.mob_pos(v.id)
    local x, y, z = math.floor(px or v.hx), math.floor(py or v.hy), math.floor(pz or v.hz)
    local cx, cy, cz = nearest({ "chest:box" }, x, y, z, 40, 8)
    for i = 1, #list, 2 do
        local left = list[i + 1]
        if cx ~= nil and cy ~= nil and cz ~= nil then
            left = core.container_add(cx, cy, cz, list[i], left)
        end
        if left > 0 then
            core.drop_item(cx or x, (cy or y) + 1, cz or z, list[i], left)
        end
    end
end

core.event.on("core:ui_click", function(ev)
    local a = asked[ev.player]
    if ev.panel ~= CAUGHT or a == nil then
        return
    end
    asked[ev.player] = nil
    core.ui_show(CAUGHT, false, ev.player)
    if ev.id == "back" then
        give_back(ev.player, a.by)
        local t = trust_of(a.rk, core.player_name(ev.player))
        t.strikes, t.last = math.max(t.strikes, 1), age()
        save_trust(a.rk)
        core.notice(ev.player, "You gave it all back. This village does not trust you any more.")
    else
        turn_hostile(ev.player, a.rk)
    end
end)

local function inspect(v: V, p: number)
    local rk = v.rk :: string
    local name = core.player_name(p)
    local t = trust_of(rk, name)
    looked[`{rk} {name}`] = clock + (if t.strikes > 0 then 40 else 150)
    act(v, "gather", "", 1.6, nil, nil)
    local n = stolen_count(p, rk)
    if n == 0 then
        core.notice(p, if t.strikes > 0 then "The inspector looks through your bag, slowly. Nothing, this time."
            else "The inspector looks through your bag: all in order.")
    elseif t.strikes == 0 then
        core.notice(p, `The inspector found {n} stolen thing{if n == 1 then "" else "s"} on you.`)
        asked[p] = { rk = rk, by = v, til = clock + 25 }
        core.ui_show(CAUGHT, true, p)
        while asked[p] ~= nil and clock < (asked[p] :: any).til do
            wait(v, 0.5)
        end
        if asked[p] ~= nil then   -- no answer is an answer
            asked[p] = nil
            core.ui_show(CAUGHT, false, p)
            turn_hostile(p, rk)
        end
    else
        turn_hostile(p, rk)
    end
end

-- The next player in the village it has not looked at lately.
local function to_inspect(v: V): number?
    local px, _, pz = core.mob_pos(v.id)
    if px == nil or pz == nil then
        return nil
    end
    local best, who = 32 * 32, nil
    for _, p in core.players() do
        local x, _, z = where(p)
        local name = core.player_name(p)
        if x ~= nil and z ~= nil and village_at(x, z) == v.rk and (looked[`{v.rk} {name}`] or 0) < clock
            and asked[p] == nil and not trust_of(v.rk :: string, name).hostile then
            local d = (x - px) ^ 2 + (z - pz) ^ 2
            if d < best then
                best, who = d, p
            end
        end
    end
    return who
end

local builder_life: (V) -> () = patrol   -- set below, with the lots
local crafter_life: (V) -> () = patrol   -- set below, with the chests

-- When one of the keepers sleeps: at night, but the guards and archers in
-- two shifts -- the odd ones by day, the even ones by night -- so the
-- village is watched round the clock.
local function sleeps_now(v: V): boolean
    local night = is_night()
    if v.kind == "guard" or v.kind == "archer" then
        if AID.ev.siege[v.rk :: string] then
            return false   -- all of the watch up for a siege
        end
        local i = tonumber(string.match(v.key, "(%d+)$")) or 1
        return (i % 2 == 0) == night
    end
    return night
end

-- In its bed while its sleep lasts. False with no bed to go to.
local function rest(v: V): boolean
    local b = v.bed
    if b == nil or not walk(v, b[1], b[2], b[3], 40) then
        return false
    end
    local px, py, pz = nearest({ "bed:bed_head" }, b[1], b[2], b[3], 1, 0)
    if px ~= nil and py ~= nil and pz ~= nil then
        core.mob_face(v.id, px + 0.5, pz + 0.5)
    end
    core.mob_act(v.id, "lie", "")
    v.sleeping = true
    while sleeps_now(v) do
        wait(v, 5)
    end
    v.sleeping = false
    core.mob_act(v.id, "", HOLDS[v.kind] or "")
    return true
end

watch_life = function(v: V)
    if v.kind == "builder" then
        builder_life(v)
        return
    end
    if v.kind == "crafter" then
        crafter_life(v)
        return
    end
    if v.kind == "mayor" or v.kind == "merchant" then
        -- By the door of the house at the village's middle, to be found.
        while true do
            if sleeps_now(v) and rest(v) then
                continue
            end
            local x, y, z = v.hx, v.hy, v.hz
            for r = 1, 3 do
                for _, d in { { r, 0 }, { -r, 0 }, { 0, r }, { 0, -r } } do
                    if standable(v.hx + d[1], v.hy, v.hz + d[2]) then
                        x, z = v.hx + d[1], v.hz + d[2]
                    end
                end
            end
            walk(v, x, y, z, 20)
            wait(v, 8 + math.random() * 8)
        end
    end
    while true do
        if sleeps_now(v) and rest(v) then
            continue
        end
        local p = if v.kind == "inspector" then to_inspect(v) else nil
        if p ~= nil then
            -- Up to them, a few goes, as they may be walking off.
            for _ = 1, 4 do
                local x, y, z = where(p)
                local mx, _, mz = core.mob_pos(v.id)
                if x == nil or y == nil or z == nil or mx == nil or mz == nil then
                    break
                end
                if (x - mx) ^ 2 + (z - mz) ^ 2 < 2.6 * 2.6 then
                    core.mob_face(v.id, x, z)
                    inspect(v, p)
                    break
                end
                walk(v, math.floor(x), math.floor(y + 0.01), math.floor(z), 4)
            end
        elseif v.kind == "inspector" and is_night() then
            walk(v, v.hx, v.hy, v.hz, 40)
            wait(v, 10)
        else
            patrol(v)
        end
        wait(v, 0.5)
    end
end

-- A village's watch, at the home nearest its middle: more in a town.
local function muster()
    for rk, b in villages do
        if watch[`{rk} inspector 1`] == nil then
            local mx, mz = (b.x1 + b.x2) / 2, (b.z1 + b.z2) / 2
            local post, best = b.homes[1], math.huge
            for _, h in b.homes do
                local d = (h.hx - mx) ^ 2 + (h.hz - mz) ^ 2
                if d < best then
                    post, best = h, d
                end
            end
            local big = #b.homes >= 8
            core.log(`villagers: {rk}: watch, mayor and builder at {post.hx} {post.hy} {post.hz}`)
            for kind, n in { guard = if big then 3 else 2, archer = if big then 2 else 1, inspector = if big then 2 else 1,
                             mayor = 1, builder = 1, merchant = 1, crafter = 1 } do
                for i = 1, n do
                    local key = `{rk} {kind} {i}`
                    watch[key] = { key = key, hx = post.hx, hy = post.hy, hz = post.hz, kind = kind, id = 0,
                                   back_at = 0, inv = {}, fed_at = 0, wake = 0, walking = false,
                                   tx = 0, ty = 0, tz = 0, tried = {}, rk = rk }
                end
            end
        elseif watch[`{rk} crafter 1`] == nil then
            -- A village mustered before there were crafters gets one.
            local m = watch[`{rk} mayor 1`] or watch[`{rk} inspector 1`]
            local key = `{rk} crafter 1`
            watch[key] = { key = key, hx = m.hx, hy = m.hy, hz = m.hz, kind = "crafter", id = 0,
                           back_at = 0, inv = {}, fed_at = 0, wake = 0, walking = false,
                           tx = 0, ty = 0, tz = 0, tried = {}, rk = rk }
        end
    end
end

-- A bed for each of a village's keepers: the beds of its ground that are
-- nobody's home and no player's, the hall's first, the nearest its middle
-- next; a bed broken is given up. Not kept: worked out again as it loads.
local KEEPERS = { "mayor", "builder", "merchant", "crafter", "inspector", "guard", "archer" }
local beds_looked: { [string]: number } = {}
-- Returns the signs a keeper found no bed under: the buildings missing.
local function bed_keepers(rk: string): { [string]: boolean }
    local b = villages[rk]
    local missing: { [string]: boolean } = {}
    local taken: { [string]: boolean } = {}
    local keepers: { V } = {}
    for _, kind in KEEPERS do
        for i = 1, 3 do
            local v = watch[`{rk} {kind} {i}`]
            if v ~= nil then
                local bd = v.bed
                if bd ~= nil and core.loaded(bd[1], bd[2], bd[3]) == 1
                    and core.get_block(bd[1], bd[2], bd[3]) ~= id("bed:bed") then
                    v.bed, bd = nil, nil
                end
                if bd ~= nil then
                    taken[`{bd[1]},{bd[2]},{bd[3]}`] = true
                    if v.bed_mark == nil then
                        v.bed_mark = bed_mark(bd[1], bd[2], bd[3]) or ""
                    end
                end
                table.insert(keepers, v)
            end
        end
    end
    if b == nil then
        return missing
    end
    local mx, mz = (b.x1 + b.x2) / 2, (b.z1 + b.z2) / 2
    local free = {}
    local f = AID.find(b.x1 - KEEPING.reach, b.y - 12, b.z1 - KEEPING.reach, b.x2 + KEEPING.reach, b.y + 16, b.z2 + KEEPING.reach, "bed:bed")
    for i = 1, #f, 3 do
        local x, y, z = f[i], f[i + 1], f[i + 2]
        local key = `{x},{y},{z}`
        local owner = core.owned_at(x, z)
        if not people[key] and not taken[key] and not (owner >= 32768 and owner < 65536) then
            local m = bed_mark(x, y, z) or ""
            table.insert(free, { x, y, z, (if m ~= "" then 0 else 1e6) + (x - mx) ^ 2 + (z - mz) ^ 2, m })
        end
    end
    table.sort(free, function(a, c) return a[4] < c[4] end)
    local function take_bed(v: V, k: number)
        local fb = table.remove(free, k)
        if v.bed ~= nil then
            -- Its old bed is somebody else's to have.
            local o = v.bed
            table.insert(free, { o[1], o[2], o[3], 1e9, v.bed_mark or "" })
        end
        v.bed, v.bed_mark = { fb[1], fb[2], fb[3] }, fb[5]
    end
    -- First a bed under its own sign, moving out of another's; then, while
    -- its own building is missing, whatever bed there is.
    for _, v in keepers do
        local want = KEEPING.role[v.kind]
        if v.bed_mark ~= want then
            local k = nil
            for n, fb in free do
                if fb[5] == want then
                    k = n
                    break
                end
            end
            if k ~= nil then
                take_bed(v, k)
            else
                missing[want] = true
            end
        end
    end
    -- An old hall's beds first (there are fifteen), then beds under no
    -- sign, and another keeper's last.
    local function rank(fb): number
        return if fb[5] == "villages:bell" then 0 elseif fb[5] == "" then 1 else 2
    end
    for _, v in keepers do
        if v.bed == nil and #free > 0 then
            local k = 1
            for n, fb in free do
                if rank(fb) < rank(free[k]) then
                    k = n
                end
            end
            take_bed(v, k)
        end
    end
    return missing
end

-- What a player takes here is marked: told the first time it is.
local carried: { [string]: number } = {}
local function notice_theft()
    for _, p in core.players() do
        local x, _, z = where(p)
        local rk = if x ~= nil and z ~= nil then village_at(x, z) else nil
        if rk ~= nil then
            local n = stolen_count(p, rk)
            if n > (carried[`{p} {rk}`] or 0) then
                core.notice(p, "Stolen: what you took from this village is marked, and its inspector will know it.")
            end
            carried[`{p} {rk}`] = n
        end
    end
end

-- What comes out of the dark into a village (or near it): the nearest to x, z.
local NEAR = 8
local function monster_in(rk: string, x: number, y: number, z: number): number?
    local b = villages[rk]
    if b == nil then
        return nil
    end
    for _, m in core.mob_ids(x, y, z, 40) do
        local mx, _, mz = core.mob_pos(m)
        if mx ~= nil and mz ~= nil and mx >= b.x1 - NEAR and mx <= b.x2 + NEAR and mz >= b.z1 - NEAR
            and mz <= b.z2 + NEAR then
            return m
        end
    end
    -- And a siege's raiders (workers, which core.mob_ids leaves out).
    for m, r in AID.ev.raiders do
        local mx, _, mz = core.mob_pos(m)
        if r[1] == rk and mx ~= nil and mz ~= nil and (mx - x) ^ 2 + (mz - z) ^ 2 < 40 * 40 then
            return m
        end
    end
    return nil
end

-- A body hunted that is suddenly far away died and was put back (or went
-- through a portal): either way the watch lets it go.
local last_at: { [number]: { number } } = {}
local function hunted_gone()
    for _, p in core.players() do
        local x, y, z = where(p)
        local was = last_at[p]
        if x ~= nil and y ~= nil and z ~= nil then
            if was ~= nil and (x - was[1]) ^ 2 + (y - was[2]) ^ 2 + (z - was[3]) ^ 2 > 24 * 24 then
                for rk, by in trust do
                    local t = by[core.player_name(p)]
                    if t ~= nil and t.hostile then
                        t.hostile = false
                        save_trust(rk)
                    end
                end
            end
            last_at[p] = { x, y, z }
        end
    end
end

-- ------------------------------------------- land, the mayor and the builder

-- A village sells land through its MAYOR (right click): a lot of 7 by 7 for
-- 8 emeralds, levelled and with a way to it, for the buyer to build on; or a
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

-- A lot's frame, as mods/villages turns a building: its door on side t of
-- (0..3: -Z, -X, +Z, +X), local x along the front (w), local z inwards (d).
-- Kinds: "lot" (levelled), "house" (style, w by d, storeys), "floor" (a
-- storey on a house already built) and "garden" (a fenced plot of wheat).
type Order = {
    kind: string, buyer: string, x0: number, z0: number, t: number, floor: number,
    stage: string, g: { number }, road: { { number } }, need: { [string]: number },
    have: { [string]: number }, since: number, asked: number,
    w: number, d: number, style: string, storeys: number,
    self: boolean?,   -- the builder told to find what it lacks itself
}
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
local SIZES = { small = { 5, 5, 14 }, medium = { 7, 7, 20 }, large = { 9, 7, 30 } }   -- w, d, emeralds

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
        hput(door, F, 0, "doors:door", 0)
        hput(door, F + 1, 0, "doors:door_top", 0)
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
    local b = villages[rk]
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


-- The houses a village built for players, for what they add to them.
type House = { x0: number, z0: number, t: number, floor: number, w: number, d: number, style: string,
               storeys: number, garden: boolean, buyer: string }
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
    o.since = clock
    return given
end

-- ------------------------------------------------------------ the mayor

local function panel_of(p: number, who: string): string
    return `villagers:{who}{p}`
end
local selling: { [number]: string } = {}   -- player -> the village they talk to

local function show_mayor(p: number, rk: string)
    local name = panel_of(p, "mayor")
    core.ui_define(name, 13, 10.4)
    local waiting = #(orders[rk] or {})
    core.ui_label(name, 0.4, 0.3, "The mayor: land in this village, for emeralds.")
    core.ui_label(name, 0.4, 1.0, (if waiting > 0 then `The builder has {waiting} order(s) before yours.`
        else "The builder is free.") .. ` The treasury holds {treasury(rk)} emeralds.`)
    core.ui_button(name, "lot", 0.4, 1.9, 12.2, 1.2, `A lot of 7 by 7, levelled, a way to it: {LOT} emeralds`)
    core.ui_button(name, "house", 0.4, 3.3, 12.2, 1.2, "A house on a lot of its own...")
    core.ui_button(name, "extend", 0.4, 4.7, 12.2, 1.2, "More for the house you have here...")
    core.ui_button(name, "guard", 0.4, 6.1, 12.2, 1.2, `A guard for your territory, a day: {GUARD_DAY} emeralds`)
    core.ui_button(name, "errands", 0.4, 7.5, 12.2, 1.2, "Errands for the village...")
    core.ui_button(name, "close", 0.4, 8.9, 12.2, 1.1, "Not now")
    selling[p] = rk
    core.ui_show(name, true, p)
end

-- A house's size (rows) and style (columns).
local function show_houses(p: number, rk: string)
    local name = panel_of(p, "mayor")
    core.ui_define(name, 15, 6.6)
    core.ui_label(name, 0.4, 0.3, "Which house? Built of wood, stone bricks or bricks.")
    local y = 1.0
    for _, size in { "small", "medium", "large" } do
        local sz = SIZES[size]
        core.ui_label(name, 0.4, y + 0.35, `{size} {sz[1]}x{sz[2]}, {sz[3]}`)
        for i, style in { "wood", "stone", "brick" } do
            core.ui_button(name, `h {size} {style}`, 5.4 + (i - 1) * 3.1, y, 3.0, 1.1, style)
        end
        y += 1.3
    end
    core.ui_button(name, "close", 0.4, y + 0.3, 14.2, 1.1, "Not now")
    selling[p] = rk
    core.ui_show(name, true, p)
end

local function show_extend(p: number, rk: string)
    local name = panel_of(p, "mayor")
    core.ui_define(name, 13, 5.2)
    core.ui_label(name, 0.4, 0.3, "For the house you have in this village:")
    core.ui_button(name, "x floor", 0.4, 1.0, 12.2, 1.2, `A second storey, a ladder up: {FLOOR_PRICE} emeralds`)
    core.ui_button(name, "x garden", 0.4, 2.4, 12.2, 1.2, `A fenced garden of wheat beside it: {GARDEN_PRICE} emeralds`)
    core.ui_button(name, "close", 0.4, 3.8, 12.2, 1.1, "Not now")
    selling[p] = rk
    core.ui_show(name, true, p)
end

-- Everything sold goes the same way: paid for, an order queued, its ground
-- the buyer's and the village's.
local function queue(p: number, rk: string, o: Order, price: number, what: string)
    o.road = if o.kind ~= "floor" and o.kind ~= "garden" and o.kind ~= "blueprint" then road_of(o) else {}
    o.need = bill_of(o)
    orders[rk] = orders[rk] or {}
    table.insert(orders[rk], o)
    if o.kind ~= "floor" then
        local x1, z1, x2, z2 = rect_of(o)
        lots[rk] = lots[rk] or {}
        table.insert(lots[rk], { x1, z1, x2, z2 })
        if o.buyer ~= "" then
            core.event.emit("territory:lot", { player = o.buyer, x1 = x1, z1 = z1, x2 = x2, z2 = z2 })
        end
        save_lots(rk)
        survey()
    end
    save_orders(rk)
    local cx, cz = at(o, o.w // 2, o.d // 2)
    if p ~= 0 then
        core.notice(p, `Yours: {what} at {cx} {cz}. The builder needs {lack_said(o.need)}.`)
    end
    core.log(`villagers: {rk}: {if o.buyer == "" then "the village" else o.buyer} ordered {what} at {cx} {o.floor} {cz}, a way of {#o.road} cells; `
        .. `it needs {lack_said(o.need)} ({price} emeralds)`)
end

local function pay(p: number, rk: string, price: number): boolean
    local paid = core.take_item(p, "ore:emerald", price)
    if paid < price then
        if paid > 0 then
            core.give_item(p, "ore:emerald", paid)
        end
        core.notice(p, `That is {price} emeralds.`)
        return false
    end
    to_treasury(rk, price)
    return true
end

local function may_buy(p: number, rk: string): boolean
    if trust_of(rk, core.player_name(p)).strikes > 0 then
        core.notice(p, "The mayor will not sell to a thief.")
        return false
    end
    if core.player_key(p) == "" then
        core.notice(p, "The mayor sells land only to a player with a key of their own.")
        return false
    end
    if #(orders[rk] or {}) >= MAX_ORDERS then
        core.notice(p, "The builder has too much to do: come back later.")
        return false
    end
    return true
end

-- A lot (7 by 7), or a house of a size and a style.
local function sell(p: number, rk: string, kind: string, size: string?, style: string?)
    if not may_buy(p, rk) then
        return
    end
    local sz = SIZES[size or "medium"]
    local w, d = if kind == "house" then sz[1] else 7, if kind == "house" then sz[2] else 7
    local price = if kind == "house" then sz[3] else LOT
    local x0, z0, t, floor, g = find_lot(rk, w, d)
    if x0 == nil or z0 == nil or t == nil or floor == nil or g == nil then
        core.notice(p, "There is no room left round this village.")
        return
    end
    if not pay(p, rk, price) then
        return
    end
    local o: Order = { kind = kind, buyer = core.player_name(p), x0 = x0, z0 = z0, t = t, floor = floor,
                       stage = "gather", g = g, road = {}, need = {}, have = { wood = 0, stone = 0, earth = 0 },
                       since = clock, asked = 0, w = w, d = d, style = style or "wood", storeys = 1 }
    queue(p, rk, o, price, if kind == "house" then `a {size} {style} house` else "a lot")
end

-- A keeper with no bed in a building of its own sign -- a village made
-- before villages had them -- has its builder put that building up, the
-- village's order, after those it has: the hall (the mayor's), barracks
-- (the watch's), or a small house with the merchant's scales, the builder's
-- plans or the crafter's anvil. Once each: built or found, it is not asked
-- for again.
local function order_post(rk: string, mark: string)
    local post = KEEPING.posts[mark]
    local key = if mark == "villages:bell" then `hall {rk}` else `post {rk} {mark}`
    local b = villages[rk]
    if post == nil or b == nil or core.storage_get(key) ~= "" then
        return
    end
    -- All its ground loaded, every 16 across and up: a building not loaded
    -- yet is not one missing.
    local R = KEEPING.reach
    local x1, z1, x2, z2 = b.x1 - R, b.z1 - R, b.x2 + R, b.z2 + R
    for x = x1, x2 + 15, 16 do
        for z = z1, z2 + 15, 16 do
            for y = b.y - 12, b.y + 16 + 15, 16 do
                if core.loaded(math.min(x, x2), math.min(y, b.y + 16), math.min(z, z2)) ~= 1 then
                    return
                end
            end
        end
    end
    -- Found: a bed under that sign (an old smithy has its anvil and no bed).
    local f = AID.find(x1, b.y - 12, z1, x2, b.y + 16, z2, "bed:bed")
    for i = 1, #f, 3 do
        if bed_mark(f[i], f[i + 1], f[i + 2]) == mark then
            core.storage_set(key, "1")
            return
        end
    end
    local x0, z0, t, floor, g = find_lot(rk, post.w, post.d)
    if x0 == nil or z0 == nil or t == nil or floor == nil or g == nil then
        return
    end
    core.storage_set(key, "1")
    local o: Order = { kind = post.kind, buyer = "", x0 = x0, z0 = z0, t = t, floor = floor,
                       stage = "gather", g = g, road = {}, need = {}, have = { wood = 0, stone = 0, earth = 0 },
                       since = clock, asked = 0, w = post.w, d = post.d, style = "wood",
                       storeys = if post.w == 9 then 2 else 1 }
    queue(0, rk, o, 0, post.what)
end

-- A storey on the house they have here, or a garden beside it.
local function extend(p: number, rk: string, what: string)
    if not may_buy(p, rk) then
        return
    end
    local h = house_of(rk, core.player_name(p))
    if h == nil then
        core.notice(p, "You have no house in this village.")
        return
    end
    local o: Order = { kind = what, buyer = h.buyer, x0 = h.x0, z0 = h.z0, t = h.t, floor = h.floor,
                       stage = "gather", g = {}, road = {}, need = {}, have = { wood = 0, stone = 0, earth = 0 },
                       since = clock, asked = 0, w = h.w, d = h.d, style = h.style, storeys = h.storeys }
    if what == "floor" then
        if h.storeys >= 2 then
            core.notice(p, "Your house has its second storey already.")
            return
        end
        if not pay(p, rk, FLOOR_PRICE) then
            return
        end
        queue(p, rk, o, FLOOR_PRICE, "a second storey")
        return
    end
    if h.garden then
        core.notice(p, "Your house has its garden already.")
        return
    end
    -- Beside the house, a cell between: its back first, then its sides.
    local x1, z1, x2, z2 = rect_of(o)
    local hx, hz = (x1 + x2) / 2, (z1 + z2) / 2
    local tries = {}
    for k = -4, 4, 2 do
        table.insert(tries, { x1 + k, z1 - 6 })
        table.insert(tries, { x1 + k, z2 + 2 })
        table.insert(tries, { x1 - 6, z1 + k })
        table.insert(tries, { x2 + 2, z1 + k })
    end
    for _, sp in tries do
        local t, floor, g = try_spot(h.floor, sp[1], sp[2], 5, 5, hx, hz)
        if t ~= nil and floor ~= nil and g ~= nil then
            if not pay(p, rk, GARDEN_PRICE) then
                return
            end
            o.x0, o.z0, o.t, o.floor, o.g, o.w, o.d = sp[1], sp[2], t, floor, g, 5, 5
            queue(p, rk, o, GARDEN_PRICE, "a garden")
            return
        end
    end
    core.notice(p, "There is no room for a garden by your house.")
end

-- ------------------------------------------------------------ the builder

local function show_builder(p: number, rk: string)
    local o, lack = wanting(rk)
    local name = panel_of(p, "builder")
    core.ui_define(name, 13, if o == nil then 6.0 else 7.4)
    if o == nil then
        local busy = (orders[rk] or {})[1]
        core.ui_label(name, 0.4, 0.3, if busy then `Building {busy.buyer}'s {busy.kind}.` else "Nothing to build.")
        core.ui_button(name, "blueprint", 0.4, 1.4, 12.2, 1.2, "Build the blueprint I laid out...")
        core.ui_button(name, "close", 0.4, 2.8, 12.2, 1.2, "Close")
    else
        core.ui_label(name, 0.4, 0.3, `For {o.buyer}'s {o.kind} the builder still needs:`)
        core.ui_label(name, 0.4, 1.0, lack_said(lack))
        core.ui_button(name, "give", 0.4, 1.8, 12.2, 1.2, "Hand over what I have of it")
        if o.self then
            core.ui_label(name, 0.4, 3.35, "The village's workers are fetching it.")
        else
            core.ui_button(name, "self", 0.4, 3.2, 12.2, 1.2, "Find it yourself: the workers fetch it")
        end
        core.ui_button(name, "blueprint", 0.4, 4.6, 12.2, 1.2, "Build the blueprint I laid out...")
        core.ui_button(name, "close", 0.4, 6.0, 12.2, 1.1, "Close")
    end
    selling[p] = rk
    core.ui_show(name, true, p)
end

local function hand_over(p: number, rk: string)
    local o, lack = wanting(rk)
    if o == nil then
        return
    end
    local given = 0
    for m, n in lack do
        for _, item in ITEMS[m] do
            if n <= 0 then
                break
            end
            local took = core.take_item(p, item, n)
            n -= took
            given += deliver(o, m, took)
        end
    end
    save_orders(rk)
    core.notice(p, if given > 0 then `You handed over {given}.` else "You have none of it.")
end

core.event.on("core:ui_click", function(ev)
    local rk = selling[ev.player]
    if rk == nil or (ev.panel ~= panel_of(ev.player, "mayor") and ev.panel ~= panel_of(ev.player, "builder")) then
        return
    end
    core.ui_show(ev.panel, false, ev.player)
    selling[ev.player] = nil
    if ev.id == "guard" then
        local paid = core.take_item(ev.player, "ore:emerald", GUARD_DAY)
        if paid < GUARD_DAY then
            if paid > 0 then
                core.give_item(ev.player, "ore:emerald", paid)
            end
            core.notice(ev.player, `That is {GUARD_DAY} emeralds.`)
            return
        end
        if core.event.emit("territory:hire", { player = ev.player, days = 1 }) then
            core.give_item(ev.player, "ore:emerald", GUARD_DAY)
        else
            to_treasury(rk, GUARD_DAY)
        end
    elseif ev.id == "errands" then
        AID.err.show(ev.player, rk, "mayor")
    elseif ev.id == "lot" then
        sell(ev.player, rk, "lot")
    elseif ev.id == "house" then
        show_houses(ev.player, rk)
    elseif ev.id == "extend" then
        show_extend(ev.player, rk)
    elseif ev.id:sub(1, 2) == "h " then
        local size, style = string.match(ev.id, "^h (%a+) (%a+)$")
        if size and style and SIZES[size] and STYLES[style] then
            sell(ev.player, rk, "house", size, style)
        end
    elseif ev.id:sub(1, 2) == "x " then
        extend(ev.player, rk, ev.id:sub(3))
    elseif ev.id == "give" then
        hand_over(ev.player, rk)
    elseif ev.id == "blueprint" then
        AID.bp.order(ev.player, rk)
    elseif ev.id == "self" then
        local o, lack = wanting(rk)
        if o ~= nil then
            o.self = true
            o.since = clock
            save_orders(rk)
            core.notice(ev.player, `The village's workers will fetch {lack_said(lack)} for {if o.buyer == "" then "the village" else o.buyer}'s {o.kind}.`)
            core.log(`villagers: {rk}: the builder finds {lack_said(lack)} itself`)
        end
    end
end)

-- ------------------------------------------------------------ the merchant

-- The MERCHANT (right click) sells what the village has in its chests -- the
-- mine's and its houses', not a player's -- and buys what a player holds, for
-- emeralds. A bundle's price is its base where the village has four bundles
-- of it, up to twice that when it has less, down to half when it has more;
-- it buys at half its price (two bundles for one emerald where that is one).
-- What it buys goes into the village's chests, paid out of its treasury.
local GOODS = {   -- item, how many a bundle, base price in emeralds
    { "farm:bread", 4, 1 }, { "pets:cooked_fish", 4, 1 }, { "animals:steak", 4, 1 },
    { "farm:wheat", 12, 1 }, { "animals:wool", 8, 1 }, { "animals:leather", 4, 1 },
    { "trees:trunk", 8, 1 }, { "timber:planks", 32, 1 }, { "rocks:cobblestone", 32, 1 },
    { "core:stone", 24, 1 }, { "core:dirt", 32, 1 },
    { "ore:coal", 8, 1 }, { "ore:raw_copper", 8, 1 }, { "ore:raw_iron", 3, 1 },
    { "ore:lapis", 8, 1 }, { "ore:cinnabar", 8, 1 }, { "ore:raw_gold", 1, 1 }, { "ore:diamond", 1, 5 },
}

-- Every worker trades too (right click), as in the classic block game: it sells from the
-- village's chests what its trade brings in, and buys what its trade needs,
-- at the full price, out of the treasury.
local TRADES = {   -- job -> what it sells, what it buys (item, a bundle, base price)
    farmer = { { { "farm:wheat", 12, 1 }, { "farm:bread", 4, 1 } }, { { "animals:seeds", 16, 1 }, { "farm:hoe", 1, 2 },
                                                                       { "furnace:iron_hoe", 1, 5 }, { "furnace:diamond_hoe", 1, 15 } } },
    -- And buys every fish a player brings: the far waters' dearer, a goldfish
    -- dearest of all.
    fisher = { { { "pets:cooked_fish", 4, 1 }, { "pets:fish", 6, 1 } },
               { { "farm:bread", 4, 1 }, { "pets:carp", 6, 1 }, { "pets:trout", 4, 1 },
                 { "pets:cod", 4, 1 }, { "pets:eel", 3, 1 }, { "pets:tuna", 2, 1 }, { "pets:goldfish", 1, 4 } } },
    forager = { { { "plants:melon_slice", 8, 1 }, { "plants:brown_mushroom", 4, 1 }, { "plants:red_mushroom", 4, 1 },
                  { "animals:seeds", 16, 1 } }, { { "farm:bread", 4, 1 } } },
    woodcutter = { { { "trees:trunk", 8, 1 }, { "timber:planks", 32, 1 } }, { { "trees:sapling", 4, 1 }, { "tools:axe", 1, 3 },
                                                                            { "furnace:iron_axe", 1, 5 }, { "furnace:diamond_axe", 1, 15 } } },
    miner = { { { "rocks:cobblestone", 32, 1 }, { "ore:coal", 8, 1 }, { "ore:raw_copper", 8, 1 }, { "ore:raw_iron", 3, 1 },
                { "ore:lapis", 8, 1 }, { "ore:cinnabar", 8, 1 }, { "ore:raw_gold", 1, 1 }, { "ore:diamond", 1, 5 } },
              { { "torches:torch", 16, 1 }, { "tools:pick", 1, 3 },
                { "furnace:iron_pick", 1, 5 }, { "furnace:diamond_pick", 1, 15 } } },
    digger = { { { "core:dirt", 32, 1 } }, { { "tools:shovel", 1, 2 }, { "farm:bread", 4, 1 },
                                           { "furnace:iron_shovel", 1, 5 }, { "furnace:diamond_shovel", 1, 15 } } },
}

-- The village's chests: every chest in its ground that is not on a player's.
-- (Or in `box`, for a village nobody lives in yet: a caravan's.)
local function village_chests(rk: string, box: Box?): { { number } }
    local b = box or villages[rk]
    local out = {}
    if b == nil then
        return out
    end
    local f = AID.find(b.x1, b.y - 20, b.z1, b.x2, b.y + 10, b.z2, "chest:box")
    for i = 1, #f, 3 do
        local owner = core.owned_at(f[i], f[i + 2])
        if not (owner >= 32768 and owner < 65536) then
            table.insert(out, { f[i], f[i + 1], f[i + 2] })
        end
    end
    return out
end

local function stock_of(chests: { { number } }, item: string): number
    local iid, n = core.item_id(item), 0
    if iid == 0 then
        return 0
    end
    for _, c in chests do
        n += core.container_count(c[1], c[2], c[3], iid)
    end
    return n
end

-- The season (mods/weather, through farm:season) doubles what it makes
-- scarce: food in winter, when the fields stop, and wood in autumn, before it.
local DEAR = {
    winter = { ["farm:bread"] = true, ["farm:wheat"] = true, ["pets:cooked_fish"] = true, ["pets:fish"] = true,
               ["animals:steak"] = true, ["plants:melon_slice"] = true },
    autumn = { ["trees:trunk"] = true, ["timber:planks"] = true },
}
local season = "spring"
core.event.on("farm:season", function(ev)
    season = ev.name
end)

local function price_of(good: { any }, stock: number): number
    local f = math.clamp(good[2] * 4 / math.max(stock, 1), 0.5, 2)
    if DEAR[season] and DEAR[season][good[1]] then
        f *= 2
    end
    return math.max(1, math.floor(good[3] * f + 0.5))
end

local function said(item: string): string
    return (string.gsub(string.gsub(item, "^.*:", ""), "_", " "))
end
local function ems(n: number): string
    return `{n} emerald{if n == 1 then "" else "s"}`
end

-- A row: the good, how many, the price, and true when the player sells it.
local trading: { [number]: { rk: string, who: string, rows: { { any } } } } = {}

-- `who` is "merchant" or a job of TRADES.
local function show_trade(p: number, rk: string, who: string)
    local sells, buys = GOODS, {}
    if who ~= "merchant" then
        sells, buys = TRADES[who][1], TRADES[who][2]
    end
    local chests = village_chests(rk)
    local rows = {}
    for _, g in sells do
        local st = stock_of(chests, g[1])
        if st >= g[2] then
            table.insert(rows, { g, g[2], AID.err.discount(p, rk, AID.ev.price(rk, g[1], price_of(g, st))) })
        end
        if #rows >= 9 then
            break
        end
    end
    local selling_rows = #rows
    if who == "merchant" then
        local held = core.held_item(p)
        for _, g in GOODS do
            if g[1] == held then
                local price = price_of(g, stock_of(chests, g[1]))
                table.insert(rows, { g, if price >= 2 then g[2] else g[2] * 2, math.max(1, price // 2), true })
            end
        end
    else
        for _, g in buys do
            if core.item_id(g[1]) ~= 0 then
                table.insert(rows, { g, g[2], price_of(g, stock_of(chests, g[1])), true })
            end
        end
    end
    local name = panel_of(p, "merchant")
    local title = if who == "merchant" then "The merchant" else `The {who}`
    local errands = who == "merchant"
    core.ui_define(name, 14, 4.6 + #rows * 1.1 + (if errands then 1.2 else 0))
    core.ui_label(name, 0.4, 0.3, if selling_rows > 0 then `{title} sells what the village has:`
        else `{title} has nothing to sell today.`)
    for i, r in rows do
        local y = 0.9 + (i - 1) * 1.1
        if r[4] then
            core.ui_button(name, `sell {i}`, 0.4, y, 13.2, 1.0,
                `Sell {r[2]} {said(r[1][1])} for {ems(r[3])}`)
        else
            core.ui_button(name, `buy {i}`, 0.4, y, 13.2, 1.0, `{r[2]} {said(r[1][1])} for {ems(r[3])}`)
        end
    end
    local y = 1.3 + #rows * 1.1
    core.ui_label(name, 0.4, y, if who == "merchant" and #rows == selling_rows
        then "Hold what you want to sell to be offered a price."
        else `The village's treasury: {ems(treasury(rk))}.`)
    if errands then
        core.ui_button(name, "errands", 0.4, y + 0.9, 13.2, 1.1, "Errands...")
        y += 1.2
    end
    core.ui_button(name, "close", 0.4, y + 0.9, 13.2, 1.1, "Close")
    trading[p] = { rk = rk, who = who, rows = rows }
    core.ui_show(name, true, p)
end

local function buy(p: number, t: { rk: string, who: string, rows: { { any } } }, i: number)
    local r = t.rows[i]
    if r == nil or r[4] then
        return
    end
    local good, price = r[1], r[3]
    local chests = village_chests(t.rk)
    if stock_of(chests, good[1]) < good[2] then
        core.notice(p, "That is gone.")
        return
    end
    local paid = core.take_item(p, "ore:emerald", price)
    if paid < price then
        if paid > 0 then
            core.give_item(p, "ore:emerald", paid)
        end
        core.notice(p, `That is {ems(price)}.`)
        return
    end
    to_treasury(t.rk, price)
    local iid, want = core.item_id(good[1]), good[2]
    for _, c in chests do
        if want <= 0 then
            break
        end
        want -= core.container_take(c[1], c[2], c[3], iid, want)
    end
    local got = good[2] - want
    local fit = core.give_item(p, good[1], got)
    if fit < got then
        local x, y, z = core.player_pos(p)
        core.drop_item(math.floor(x or 0), math.floor(y or 0), math.floor(z or 0), iid, got - fit)
    end
    core.notice(p, `You bought {got} {said(good[1])}.`)
end

local function sell_to(p: number, t: { rk: string, who: string, rows: { { any } } }, i: number)
    local r = t.rows[i]
    if r == nil or not r[4] then
        return
    end
    local good, n, pay = r[1], r[2], r[3]
    if treasury(t.rk) < pay then
        core.notice(p, `The village has {ems(treasury(t.rk))} to pay with.`)
        return
    end
    local took = core.take_item(p, good[1], n)
    if took < n then
        if took > 0 then
            core.give_item(p, good[1], took)
        end
        core.notice(p, `It buys {n} at a time.`)
        return
    end
    local iid, left = core.item_id(good[1]), n
    for _, c in village_chests(t.rk) do
        if left <= 0 then
            break
        end
        left = core.container_add(c[1], c[2], c[3], iid, left)
    end
    if left > 0 then
        -- ponytail: no room in the village's chests, it is not bought; a store
        -- house for the village when that happens in play.
        core.give_item(p, good[1], left)
        if left == n then
            core.notice(p, "The village's chests are full.")
            return
        end
        pay = math.max(1, pay * (n - left) // n)
    end
    to_treasury(t.rk, -pay)
    core.give_item(p, "ore:emerald", pay)
    core.notice(p, `You sold {n - left} {said(good[1])} for {ems(pay)}.`)
end

core.event.on("core:ui_click", function(ev)
    local t = trading[ev.player]
    if t == nil or ev.panel ~= panel_of(ev.player, "merchant") then
        return
    end
    trading[ev.player] = nil
    core.ui_show(ev.panel, false, ev.player)
    local i = tonumber(string.match(ev.id, "^buy (%d+)$") or "")
    local j = tonumber(string.match(ev.id, "^sell (%d+)$") or "")
    if ev.id == "errands" then
        AID.err.show(ev.player, t.rk, "merchant")
    elseif i ~= nil then
        buy(ev.player, t, i)
        show_trade(ev.player, t.rk, t.who)
    elseif j ~= nil then
        sell_to(ev.player, t, j)
        show_trade(ev.player, t.rk, t.who)
    end
end)

-- Somebody to ask for what is missing: the buyer if here, else whoever is near.
local function ask(o: Order, v: V, lack: { [string]: number })
    local said = `The builder needs {lack_said(lack)} for {if o.buyer == "" then "the village" else o.buyer}'s {o.kind}: right click it to hand them `
        .. "over, or to tell it to find them itself."
    for _, p in core.players() do
        local x, _, z = core.player_pos(p)
        local mx, _, mz = core.mob_pos(v.id)
        if core.player_name(p) == o.buyer or (x and z and mx and mz and (x - mx) ^ 2 + (z - mz) ^ 2 < 16 * 16) then
            core.notice(p, said)
        end
    end
end

local function has_kind(rk: string, kind: string): boolean
    for _, v in people do
        if v.kind == kind and region_of(v.hx, v.hz) == rk then
            return true
        end
    end
    return false
end

-- ------------------------------------------- the village's goods, shared

-- Out of the village's chests and into them: the village's people take and
-- leave freely; only a stranger pays (the merchant, the workers' panels).
local function take_from(chests: { { number } }, item: string, n: number): number
    local iid, want = core.item_id(item), n
    if iid == 0 then
        return 0
    end
    for _, c in chests do
        if want <= 0 then
            break
        end
        want -= core.container_take(c[1], c[2], c[3], iid, want)
    end
    return n - want
end
local function put_into(chests: { { number } }, item: string, n: number): number
    local iid, left = core.item_id(item), n
    if iid == 0 then
        return n
    end
    for _, c in chests do
        if left <= 0 then
            break
        end
        left = core.container_add(c[1], c[2], c[3], iid, left)
    end
    return left
end

-- ------------------------------------------------------------ blueprints
--
-- A player's laid-out blueprint (mods/blueprints) built by the builder, as an
-- order of the kind "blueprint" -- its cells asked of mods/blueprints with
-- "blueprints:ask" whenever its plan is needed. Inside the village's ground
-- only. What the gatherers bring (wood, stone, earth: cost_of) the builder
-- needs as for a house; any other block (glass, wool, an ore block) the
-- player hands over, all of it, when the order is made; the labour is paid in
-- emeralds, four and one more for each forty blocks.
AID.bp = { got = {} :: { [string]: { any } } }
core.event.on("blueprints:cells", function(ev)
    local cells = {}
    for x, y, z, name, f in string.gmatch(ev.cells, "(%-?%d+),(%-?%d+),(%-?%d+),([^,]*),(%d+)") do
        table.insert(cells, { tonumber(x), tonumber(y), tonumber(z), name, if name == "" then nil else tonumber(f), false })
    end
    AID.bp.got[`{ev.id} {ev.x} {ev.y} {ev.z} {ev.t}`] = { cells = cells, w = ev.w, d = ev.d, id = ev.id,
                                                           x = ev.x, y = ev.y, z = ev.z, t = ev.t, owner = ev.owner }
    AID.bp.last = AID.bp.got[`{ev.id} {ev.x} {ev.y} {ev.z} {ev.t}`]
end)
AID.bp.plan = function(o: Order): { { any } }
    local id = tonumber(string.match(o.style, "^bp:(%d+)$") or "") or 0
    local key = `{id} {o.x0} {o.floor} {o.z0} {o.t}`
    if AID.bp.got[key] == nil then
        core.event.emit("blueprints:ask", { id = id, x = o.x0, y = o.floor, z = o.z0, t = o.t, owner = o.buyer })
    end
    local g = AID.bp.got[key]
    return if g then g.cells else {}
end
AID.bp.order = function(p: number, rk: string)
    local who = core.player_name(p)
    if not may_buy(p, rk) then
        return
    end
    AID.bp.last = nil
    core.event.emit("blueprints:layout", { owner = who })
    local g = AID.bp.last
    if g == nil or g.owner ~= who then
        core.notice(p, "Lay a blueprint out first (use it on the ground where it is to go).")
        return
    end
    local b = villages[rk]
    local fx2, fz2 = g.x + g.w - 1, g.z + g.d - 1
    if b == nil or g.x < b.x1 or g.z < b.z1 or fx2 > b.x2 or fz2 > b.z2 then
        core.notice(p, "The builder builds only on the village's ground.")
        return
    end
    -- What the player brings: every block the gatherers do not.
    local bring: { [string]: number } = {}
    local solid = 0
    for _, c in g.cells do
        if c[4] ~= "" then
            solid += 1
            if cost_of(c[4]) == nil and core.get_block(c[1], c[2], c[3]) ~= id(c[4]) then
                bring[c[4]] = (bring[c[4]] or 0) + 1
            end
        end
    end
    local short = {}
    for name, n in bring do
        local have = core.bag_count(p, name)
        if have < n then
            table.insert(short, `{n - have} {said(name)}`)
        end
    end
    if #short > 0 then
        core.notice(p, `First bring what the gatherers cannot: {table.concat(short, ", ")}.`)
        return
    end
    local price = 4 + solid // 40
    if not pay(p, rk, price) then
        return
    end
    for name, n in bring do
        core.take_item(p, name, n)
    end
    local w, d = if g.t % 2 == 0 then g.w else g.d, if g.t % 2 == 0 then g.d else g.w
    local o: Order = { kind = "blueprint", buyer = who, x0 = g.x, z0 = g.z, t = g.t, floor = g.y,
                       stage = "gather", g = {}, road = {}, need = {}, have = { wood = 0, stone = 0, earth = 0 },
                       since = clock, asked = 0, w = w, d = d, style = `bp:{g.id}`, storeys = 1 }
    queue(p, rk, o, price, "your blueprint")
end

-- ------------------------------------------------------------ errands
--
-- ERRANDS the mayor and the merchant post for strangers, at most three a
-- village, each paid in emeralds out of the treasury -- none is posted that
-- the treasury cannot pay, and its pay is set aside when it is taken. They
-- are seen on a NOTICE BOARD (villagers:board, by the well or put up by a
-- player in the village) and taken and handed in at the panel of whoever
-- gives them. Six kinds:
--
--   merchant  bring      what the village's chests are short of
--   merchant  procure    a rarer material it wants (diamonds, crystal...)
--   mayor     craft      tools for its people: an iron pick, a bow, torches
--   mayor     hunt       N creatures of the night killed inside the village
--   mayor     carry      a parcel to another village: paid on arriving there
--   mayor     explore    stand at a far place, then come back and say so
--
-- One taken at a time. Each lasts a day (DAY): one not taken goes, one taken
-- and not done goes back on the board, its pay back in the treasury -- and
-- nothing else is lost. Every one done is remembered by the village; after
-- five the merchant sells to that player at a fifth off.
-- (All in AID.err: this file's main chunk is at Luau's 200 locals.)

type Errand = { id: number, kind: string, giver: string, item: string, n: number, x: number, z: number,
                to: string, pay: number, posted: number, taker: string, taken: number, got: number }
AID.err = { list = {} :: { [string]: { Errand } }, known = {} :: { [string]: { number } }, next = 1,
            done = {} :: { [string]: { [string]: number } } }
do
local E = AID.err
E.PROCURE = { { "ore:diamond", 2, 9 }, { "ore:raw_gold", 4, 5 }, { "ore:lapis", 16, 4 }, { "deep:shard", 8, 6 },
              { "rocks:amethyst_shard", 8, 4 }, { "animals:leather", 8, 3 }, { "deep:glowcap", 6, 5 },
              { "pets:goldfish", 1, 5 }, { "ore:cinnabar", 12, 4 } }
E.CRAFT = { { "furnace:iron_pick", 1, 6 }, { "furnace:iron_sword", 1, 6 }, { "furnace:iron_axe", 1, 6 },
            { "archery:bow", 1, 4 }, { "archery:arrow", 32, 4 }, { "torches:torch", 32, 3 },
            { "furnace:iron_shovel", 1, 5 }, { "animals:shears", 1, 4 } }

E.said = function(e: Errand): string
    if e.kind == "bring" or e.kind == "procure" then
        return `bring {e.n} {said(e.item)}`
    elseif e.kind == "craft" then
        local what = said(e.item)
        return `make and bring {if e.n > 1 then e.n .. " " elseif string.find(what, "^[aeiou]") then "an " else "a "}{what}`
    elseif e.kind == "hunt" then
        return `kill {e.n} creatures of the night in the village ({e.got} so far)`
    elseif e.kind == "carry" then
        local k = E.known[e.to]
        return if k then `carry a parcel to the village at {k[1]} {k[2]}` else "carry a parcel"
    end
    return `stand at {e.x} {e.z}, then come back{if e.got > 0 then " (been there)" else ""}`
end

E.load = function(rk: string): { Errand }
    local l = E.list[rk]
    if l then
        return l
    end
    l = {}
    for part in string.gmatch(core.storage_get(`errands {rk}`), "[^;]+") do
        local f = string.split(part, "|")
        if #f >= 13 then
            table.insert(l, { id = tonumber(f[1]) :: number, kind = f[2], giver = f[3], item = f[4],
                              n = tonumber(f[5]) :: number, x = tonumber(f[6]) :: number, z = tonumber(f[7]) :: number,
                              to = f[8], pay = tonumber(f[9]) :: number, posted = tonumber(f[10]) :: number,
                              taker = f[11], taken = tonumber(f[12]) :: number, got = tonumber(f[13]) :: number })
        end
    end
    E.list[rk] = l
    return l
end

E.save = function(rk: string)
    local parts = {}
    for _, e in E.list[rk] or {} do
        table.insert(parts, table.concat({ e.id, e.kind, e.giver, e.item, e.n, e.x, e.z, e.to, e.pay,
                                           math.floor(e.posted), e.taker, math.floor(e.taken), e.got }, "|"))
    end
    core.storage_set(`errands {rk}`, table.concat(parts, ";"))
    core.storage_set("errand next", tostring(E.next))
end

E.done_by = function(rk: string, name: string): number
    local d = E.done[rk]
    if d == nil then
        d = {}
        for part in string.gmatch(core.storage_get(`errands done {rk}`), "[^;]+") do
            local n, who = string.match(part, "^(%d+),(.+)$")
            if n and who then
                d[who] = tonumber(n) :: number
            end
        end
        E.done[rk] = d
    end
    return d[name] or 0
end

-- A fifth off what the merchant sells, for whoever has done five errands here.
E.discount = function(p: number, rk: string, price: number): number
    return if E.done_by(rk, core.player_name(p)) >= 5 then math.max(1, math.floor(price * 0.8 + 0.5)) else price
end

-- The errand this player has taken, anywhere, and its village.
E.mine = function(name: string): (Errand?, string?)
    for rk in E.known do
        for _, e in E.load(rk) do
            if e.taker == name then
                return e, rk
            end
        end
    end
    return nil, nil
end

-- A new errand for a village, or nil when nothing fits or the treasury
-- cannot pay it.
E.make = function(rk: string): Errand?
    local b = villages[rk]
    local k = E.known[rk]
    if b == nil or k == nil then
        return nil
    end
    local open = E.load(rk)
    local has: { [string]: boolean } = {}
    for _, e in open do
        has[e.kind] = true
    end
    local kinds = { "bring", "procure", "craft", "hunt", "carry", "explore" }
    local kind = kinds[math.random(#kinds)]
    if has[kind] then
        return nil
    end
    local e: Errand = { id = E.next, kind = kind, giver = "mayor", item = "", n = 0, x = 0, z = 0, to = "",
                        pay = 0, posted = age(), taker = "", taken = 0, got = 0 }
    if kind == "bring" then
        local chests = village_chests(rk)
        local short = {}
        for _, g in GOODS do
            if core.item_id(g[1]) ~= 0 and stock_of(chests, g[1]) < g[2] then
                table.insert(short, g)
            end
        end
        if #short == 0 then
            return nil
        end
        local g = short[math.random(#short)]
        e.giver, e.item, e.n, e.pay = "merchant", g[1], g[2] * 2, g[3] * 2 + 1
    elseif kind == "procure" or kind == "craft" then
        local list = if kind == "procure" then E.PROCURE else E.CRAFT
        local g = list[math.random(#list)]
        if core.item_id(g[1]) == 0 then
            return nil
        end
        e.giver = if kind == "procure" then "merchant" else "mayor"
        e.item, e.n, e.pay = g[1], g[2], g[3]
    elseif kind == "hunt" then
        e.n = math.random(3, 6)
        e.pay = e.n
    elseif kind == "carry" then
        local best, bd = nil, math.huge
        for other, o in E.known do
            local d = math.sqrt((o[1] - k[1]) ^ 2 + (o[2] - k[2]) ^ 2)
            if other ~= rk and d >= 150 and d < bd then
                best, bd = other, d
            end
        end
        if best == nil then
            return nil
        end
        e.to, e.pay = best, math.max(3, math.floor(bd / 100) + 2)
    else
        local a, d = math.random() * 2 * math.pi, 300 + math.random() * 300
        e.x, e.z = math.floor(k[1] + math.cos(a) * d), math.floor(k[2] + math.sin(a) * d)
        e.pay = math.floor(d / 100) + 2
    end
    if treasury(rk) < e.pay then
        return nil
    end
    E.next += 1
    return e
end

-- Every few seconds: villages near players get errands up to three, and what has
-- lasted a day goes (posted) or goes back on the board (taken).
E.tick = function()
    local now = age()
    for rk in villages do
        local l = E.load(rk)
        local changed = false
        for i = #l, 1, -1 do
            local e = l[i]
            if e.taker == "" and now - e.posted > DAY then
                table.remove(l, i)
                changed = true
            elseif e.taker ~= "" and now - e.taken > DAY then
                to_treasury(rk, e.pay)
                for _, q in core.players() do
                    if core.player_name(q) == e.taker then
                        core.notice(q, `Not done in a day, and back on the board: {E.said(e)}`)
                        if e.kind == "carry" then
                            core.take_item(q, "villagers:parcel", 1)
                        end
                    end
                end
                e.taker, e.taken, e.got, e.posted = "", 0, 0, now
                changed = true
            end
        end
        if #l < 3 then
            local e = E.make(rk)
            if e then
                table.insert(l, e)
                changed = true
            end
        end
        if changed then
            E.save(rk)
        end
    end
    -- An explorer at the place; a carrier at the other village.
    for _, q in core.players() do
        local e, erk = E.mine(core.player_name(q))
        local to = e and E.known[e.to]
        local qx, _, qz = core.player_pos(q)
        if e and e.kind == "carry" and to and qx and (qx - to[1]) ^ 2 + (qz - to[2]) ^ 2 < 32 * 32
            and core.take_item(q, "villagers:parcel", 1) == 1 then
            E.finish(q, e, erk :: string)
        elseif e and e.kind == "explore" and e.got == 0 then
            local x, _, z = core.player_pos(q)
            if x and (x - e.x) ^ 2 + (z - e.z) ^ 2 < 12 * 12 then
                e.got = 1
                core.notice(q, "You are there: go back and tell the mayor.")
                local _, rk = E.mine(core.player_name(q))
                E.save(rk :: string)
            end
        end
    end
end

-- The board: every errand of the village, who gives it and who has it.
E.board = function(p: number, rk: string)
    local l = E.load(rk)
    local news = AID.ev.lines(rk)
    local top = #news * 1.1
    local name = panel_of(p, "board")
    core.ui_define(name, 15, 3.4 + top + math.max(1, #l) * 1.1)
    for i, n in news do
        core.ui_label(name, 0.4, 0.3 + (i - 1) * 1.1, n)
    end
    core.ui_label(name, 0.4, 0.3 + top, `The village's errands. The treasury holds {ems(treasury(rk))}.`)
    for i, e in l do
        local who = if e.taker == "" then `ask the {e.giver}` else `taken by {e.taker}`
        core.ui_label(name, 0.4, 0.2 + top + i * 1.1, `{E.said(e)}: {ems(e.pay)} ({who})`)
    end
    if #l == 0 then
        core.ui_label(name, 0.4, 1.3 + top, "Nothing today.")
    end
    core.ui_button(name, "close", 0.4, 1.9 + top + math.max(1, #l) * 1.1, 14.2, 1.1, "Close")
    core.ui_show(name, true, p)
end

-- The giver's panel: its errands to take, and the player's to hand in.
E.show = function(p: number, rk: string, giver: string)
    local who = core.player_name(p)
    local mine, mine_rk = E.mine(who)
    local rows = {}
    for _, e in E.load(rk) do
        if e.giver == giver and (e.taker == "" or e.taker == who) then
            table.insert(rows, e)
        end
    end
    local name = panel_of(p, "errands")
    core.ui_define(name, 15, 3.6 + math.max(1, #rows) * 1.2)
    local done = E.done_by(rk, who)
    core.ui_label(name, 0.4, 0.3, `Errands of the {giver}. Done here: {done}{if done >= 5 then " (a fifth off at the merchant)" else ""}.`)
    for i, e in rows do
        local label = if e.taker == who then `Hand in: {E.said(e)}` else `Take: {E.said(e)} -- {ems(e.pay)}`
        core.ui_button(name, `e {e.id}`, 0.4, 0.1 + i * 1.2, 14.2, 1.1, label)
    end
    if #rows == 0 then
        core.ui_label(name, 0.4, 1.3, "Nothing for you today.")
    end
    core.ui_button(name, "close", 0.4, 1.7 + math.max(1, #rows) * 1.2, 14.2, 1.1, "Close")
    E.talking = E.talking or {}
    E.talking[p] = { rk = rk, mine_rk = mine_rk }
    core.ui_show(name, true, p)
end

-- Errands done by a player, counted up (a siege held or a caravan escorted
-- counts too, mods/villagers' events).
E.credit = function(rk: string, who: string, n: number)
    E.done_by(rk, who)
    E.done[rk][who] = (E.done[rk][who] or 0) + n
    local parts = {}
    for n2, c in E.done[rk] do
        table.insert(parts, `{c},{n2}`)
    end
    core.storage_set(`errands done {rk}`, table.concat(parts, ";"))
end

-- Paid out of what was set aside, and remembered.
E.finish = function(p: number, e: Errand, rk: string)
    local who = core.player_name(p)
    core.give_item(p, "ore:emerald", e.pay)
    local l = E.load(rk)
    for i, x in l do
        if x == e then
            table.remove(l, i)
        end
    end
    E.save(rk)
    E.credit(rk, who, 1)
    core.notice(p, `Done: {ems(e.pay)}. Thank you.`)
    core.log(`villagers: {rk}: {who} did an errand ({e.kind}) for {e.pay}`)
end

E.click = function(p: number, id: number)
    local t = E.talking and E.talking[p]
    if t == nil then
        return
    end
    local who = core.player_name(p)
    local mine, mine_rk = E.mine(who)
    if mine and mine.id == id then
        -- Handing in.
        local e, rk = mine, mine_rk :: string
        if e.kind == "bring" or e.kind == "procure" or e.kind == "craft" then
            local got = core.take_item(p, e.item, e.n)
            if got < e.n then
                if got > 0 then
                    core.give_item(p, e.item, got)
                end
                core.notice(p, `That is {e.n} {said(e.item)}.`)
                return
            end
            local left = put_into(village_chests(rk), e.item, e.n)
            if left > 0 then
                local x, y, z = core.player_pos(p)
                core.drop_item(math.floor(x or 0), math.floor(y or 0), math.floor(z or 0), core.item_id(e.item), left)
            end
        elseif e.kind == "hunt" and e.got < e.n then
            core.notice(p, `{e.got} of {e.n} so far.`)
            return
        elseif e.kind == "explore" and e.got == 0 then
            core.notice(p, `You have not been to {e.x} {e.z} yet.`)
            return
        elseif e.kind == "carry" then
            core.notice(p, "The parcel is for the other village: it is paid there.")
            return
        end
        E.finish(p, e, rk)
        return
    end
    if mine then
        core.notice(p, `You have an errand already: {E.said(mine)}.`)
        return
    end
    for _, e in E.load(t.rk) do
        if e.id == id and e.taker == "" then
            if treasury(t.rk) < e.pay then
                core.notice(p, "The treasury cannot pay for that now.")
                return
            end
            to_treasury(t.rk, -e.pay)   -- set aside
            e.taker, e.taken, e.got = who, age(), 0
            if e.kind == "carry" then
                core.give_item(p, "villagers:parcel", 1)
            end
            E.save(t.rk)
            core.notice(p, `Taken: {E.said(e)}, within a day.`)
            return
        end
    end
end

core.register_item("villagers:parcel", 1, "", 0, 1.0, "")
core.register_model("villagers:parcel_held", "parcel_held.gltf")   -- tools/make_held_models.py
E.BOARD = core.register_node("villagers:board", 150, 112, 70, true, 0, "upright", 0, 1.5, "wood")
core.node_box(E.BOARD, 0.45, 0, 0.45, 0.55, 1.0, 0.55)
core.node_box(E.BOARD, 0.05, 0.55, 0.42, 0.95, 1.25, 0.58)
core.register_model("villagers:board_held", "board_held.gltf")   -- tools/make_held_models.py
core.recipe_shaped("villagers:board", 1,
    "timber:planks cane:paper   timber:planks",
    "-             timber:stick -")

core.event.on("core:world_ready", function()
    E.next = tonumber(core.storage_get("errand next")) or 1
    for part in string.gmatch(core.storage_get("known villages"), "[^;]+") do
        local rk, x, z = string.match(part, "^(.+)=(-?%d+),(-?%d+)$")
        if rk and x and z then
            E.known[rk] = { tonumber(x) :: number, tonumber(z) :: number }
        end
    end
end)

-- Villages seen, by their middle: where a parcel may go. Every one the
-- world has made (villages:extent), lived in or not yet.
core.event.on("villages:extent", function(ev)
    E.saw(`{ev.rx},{ev.rz}`, { x1 = ev.x1, z1 = ev.z1, x2 = ev.x2, z2 = ev.z2, y = 0, homes = {} })
end)
E.saw = function(rk: string, b: Box)
    if E.known[rk] == nil then
        E.known[rk] = { (b.x1 + b.x2) // 2, (b.z1 + b.z2) // 2 }
        local parts = {}
        for k, c in E.known do
            table.insert(parts, `{k}={c[1]},{c[2]}`)
        end
        core.storage_set("known villages", table.concat(parts, ";"))
    end
end

local since = 0.0
core.event.on("core:tick", function(ev)
    since += ev.dt
    if since >= 5 then
        since = 0
        E.tick()
    end
end)

-- A creature of the night killed in the village, at night, by a hunter.
core.event.on("core:hit", function(ev)
    if not ev.killed then
        return
    end
    local e, rk = E.mine(core.player_name(ev.player))
    if e == nil or e.kind ~= "hunt" or e.got >= e.n or not is_night() then
        return
    end
    local kind = core.mob_kind(ev.target)
    if kind == "" or string.find(kind, "^animals:") or string.find(kind, "^villagers:")
        or string.find(kind, "^helpers:") or string.find(kind, "^pets:") then
        return
    end
    local x, _, z = core.player_pos(ev.player)
    if x and village_at(x, z) == rk then
        e.got += 1
        core.notice(ev.player, if e.got >= e.n then "That is enough: go and tell the mayor."
            else `{e.got} of {e.n}.`)
        E.save(rk :: string)
    end
end)

core.event.on("core:used", function(ev)
    if ev.id ~= E.BOARD then
        return
    end
    ev.cancel = true
    local rk = village_at(ev.x, ev.z)
    if rk then
        E.board(ev.player, rk)
    else
        core.notice(ev.player, "A board, but no village here to post errands on it.")
    end
end)

core.event.on("core:ui_click", function(ev)
    if ev.panel == panel_of(ev.player, "board") then
        core.ui_show(ev.panel, false, ev.player)
    elseif ev.panel == panel_of(ev.player, "errands") then
        core.ui_show(ev.panel, false, ev.player)
        local id = tonumber(string.match(ev.id, "^e (%d+)$") or "")
        if id then
            E.click(ev.player, id)
        end
    end
end)
end

-- ------------------------------------------------------- the village's events
--
-- What happens in a village now and then (agreed 2026-10-28), each told on
-- its notice board, by the hall's bell (bell.ogg) and in a notice to the
-- players near:
--
--   SIEGE    rare -- a night in seven to ten a village, and only with a
--            player near: the mayor warns at dusk, and in the night three
--            waves of zombies and skeletons (villagers:raider, its archer)
--            come from the edge for its people and the players; the watch
--            fights them (monster_in) and keeps awake. No villager lost: the
--            players who stood there are paid 5 emeralds from the treasury
--            each, it counts as two errands done (the merchant's fifth off),
--            and any strike against them is forgotten.
--   CARAVAN  about once a day a village with a player near sends a merchant
--            and a mule (mule.gltf: tools/make_mule_model.py) to another
--            village, with real goods out of its chests -- what it has
--            plenty of and the other lacks. It walks while it is seen and is
--            carried on along the line when no one is near. On the road it
--            sells its cargo (right click), and whoever escorts it is paid on
--            its arrival; at night monsters come for it, and killed, its
--            goods are left where it fell. Arrived, its goods go into the
--            other village's chests and its price into its own treasury.
--            Caravans COME too, now and then, from a village near that nobody
--            has been to yet, with what its land is known for (EV.TYPICAL:
--            sandstone and cactus from the desert, spruce and leather from the
--            cold...). Every caravan walks the paths between the villages
--            (mods/villages' network, villages:route_ask), carried along them
--            too; its merchant tells the way to its village, aloud and on the
--            player's map (mods/maps' maps:route).
--   FEAST    the last day of summer (farm:season's `left`): stalls and
--            lanterns in the square (the board's, by the well), the people
--            gathered there, food at half price, music (music.ogg) and in
--            the evening fireworks (mods/fireworks). Put away at night.
--
-- /village siege|feast|caravan (admin) starts one now where the admin stands.

type Siege = { wave: number, next: number, lost: number, started: boolean, fought: { [string]: boolean } }
type Feast = { x: number, y: number, z: number, put: { { number } }, fired: boolean, played: number }
type Caravan = { from: string, to: string, x: number, y: number, z: number, goods: { [string]: number },
                 m: number, mule: number, escort: string, best: number, best_at: number, att_at: number,
                 route: { { number } }?, k: number }
AID.ev = { news = {} :: { [string]: { { any } } }, siege = {} :: { [string]: Siege },
           raiders = {} :: { [number]: { any } }, feast = {} :: { [string]: Feast }, caravans = {} :: { Caravan },
           asked = {} :: { [string]: number }, talk = {} :: { [number]: Caravan }, left = 99 }
do
local EV = AID.ev
local E = AID.err
core.register_mob("villagers:raider", core.model_id("mobs:zombie"), 20, 2.2, "worker", 3, 1.6, 1.0, "mobs:flesh", 1)
core.register_mob("villagers:raider_archer", core.model_id("mobs:skeleton"), 16, 2.4, "worker", 3, 15.0, 2.0,
    "mobs:bone", 2, ARROW)
core.register_mob("villagers:mule", core.register_model("villagers:mule", "mule.gltf"), 30, 2.6, "worker", 0,
    1.6, 1.0, "", 1)
core.mob_voice("villagers:raider", "mobs:zombie")
core.mob_voice("villagers:raider_archer", "mobs:skeleton")
core.event.on("farm:season", function(ev)
    EV.left = ev.left
end)

-- The players within r of a village's ground.
EV.near = function(rk: string, r: number): { number }
    local b, out = villages[rk], {}
    if b == nil then
        return out
    end
    for _, p in core.players() do
        local x, _, z = core.player_pos(p)
        if x and z and x > b.x1 - r and x < b.x2 + r and z > b.z1 - r and z < b.z2 + r then
            table.insert(out, p)
        end
    end
    return out
end

-- The node of that name in a village's ground nearest its middle.
EV.spot = function(rk: string, name: string): { number }?
    local b = villages[rk]
    if b == nil then
        return nil
    end
    local mx, mz = (b.x1 + b.x2) / 2, (b.z1 + b.z2) / 2
    local f = AID.find(b.x1, b.y - 12, b.z1, b.x2, b.y + 12, b.z2, name)
    local best, bd = nil, math.huge
    for i = 1, #f, 3 do
        local d = (f[i] - mx) ^ 2 + (f[i + 2] - mz) ^ 2
        if d < bd then
            best, bd = { f[i], f[i + 1], f[i + 2] }, d
        end
    end
    return best
end

-- A cell to stand in at x, z, near y; nil where nothing is loaded or none.
EV.ground = function(x: number, y: number, z: number): number?
    for dy = 0, 10 do
        for _, gy in { y + dy, y - dy } do
            if standable(x, gy, z) then
                return gy
            end
        end
    end
    return nil
end

EV.announce = function(rk: string, text: string)
    local l = EV.news[rk] or {}
    table.insert(l, 1, { text, age() + DAY })
    while #l > 3 do
        table.remove(l)
    end
    EV.news[rk] = l
    local bell = EV.spot(rk, "villages:bell")
    if bell then
        core.play_sound("villagers:bell", bell[1] + 0.5, bell[2] + 0.5, bell[3] + 0.5)
    end
    for _, p in EV.near(rk, 48) do
        core.notice(p, text)
    end
    core.log(`villagers: {rk}: {text}`)
end

-- What the board shows above the errands: the news of the last day.
EV.lines = function(rk: string): { string }
    local out = {}
    for _, n in EV.news[rk] or {} do
        if n[2] > age() then
            table.insert(out, n[1])
        end
    end
    return out
end

-- ------------------------------------------------------------ the raiders

EV.raider = function(name: string, x: number, y: number, z: number, rk: string, c: Caravan?): number
    local id = core.spawn_mob(name, x + 0.5, y, z + 0.5)
    if id ~= 0 then
        EV.raiders[id] = { rk, -1, false, c }   -- its village, whom it is after, a player?, its caravan
    end
    return id
end

-- After the nearest of a village's people, its watch, a caravan's merchant
-- or a player, within 24; among its homes when nothing is near.
EV.steer = function(id: number, r: { any })
    local x, y, z = core.mob_pos(id)
    if x == nil or y == nil or z == nil then
        EV.raiders[id] = nil
        return
    end
    local best, bd, player = 0, 24 * 24, false
    for _, p in core.players() do
        local px, py, pz = core.player_pos(p)
        if px and py and pz then
            local d = (px - x) ^ 2 + (py - y) ^ 2 + (pz - z) ^ 2
            if d < bd then
                best, bd, player = p, d, true
            end
        end
    end
    local function consider(mid: number)
        local mx, my, mz = core.mob_pos(mid)
        if mx and my and mz then
            local d = (mx - x) ^ 2 + (my - y) ^ 2 + (mz - z) ^ 2
            if d < bd then
                best, bd, player = mid, d, false
            end
        end
    end
    local c = r[4] :: Caravan?
    if c then
        if c.m ~= 0 then consider(c.m) end
        if c.mule ~= 0 then consider(c.mule) end
    else
        for _, list in { people, watch } do
            for _, v in list do
                if v.id ~= 0 and not v.sleeping then
                    consider(v.id)
                end
            end
        end
    end
    if best ~= r[2] or player ~= r[3] then
        r[2], r[3] = best, player
        core.mob_hunt(id, if player then best else 0)
        core.mob_attack(id, if player then 0 else best)
    end
    if best == 0 and villages[r[1]] and core.mob_going(id) == 0 then
        local gx, gy, gz = somewhere_in(r[1])
        if gx and gy and gz then
            core.mob_goto(id, gx, gy, gz)
        end
    end
end

-- How many of a village's (or a caravan's) raiders are still up.
EV.up = function(rk: string, c: Caravan?): number
    local n = 0
    for id, r in EV.raiders do
        if core.mob_pos(id) == nil then
            EV.raiders[id] = nil
        elseif r[1] == rk and r[4] == c then
            n += 1
        end
    end
    return n
end

-- ------------------------------------------------------------ the siege

EV.begin_siege = function(rk: string)
    EV.siege[rk] = { wave = 0, next = 0, lost = 0, started = false, fought = {} }
    core.storage_set(`siege {rk}`, tostring(math.floor(age())))
    EV.announce(rk, "The mayor rings the bell: monsters are gathering, and tonight they will come. "
        .. "Stand with the watch!")
end

-- One of a village's people fell (step): a siege counts it.
EV.fell = function(v: V)
    local rk = v.rk or village_at(v.hx, v.hz)
    local s = rk and EV.siege[rk]
    if s and s.started and v.kind ~= "guard" and v.kind ~= "archer" then
        s.lost += 1
        core.log(`villagers: {rk}: the {v.kind} at {v.hx} {v.hz} fell in the siege`)
    end
end

EV.wave = function(rk: string, s: Siege)
    local b = villages[rk]
    s.wave += 1
    if b == nil then
        return
    end
    s.next = clock + 50
    local n = 2 + s.wave + #EV.near(rk, 24) // 2   -- 3, 4, 5, and more for a crowd
    local sidex = math.random(0, 1) == 1
    local far = math.random(0, 1) == 1
    local put = 0
    for _ = 1, n * 4 do
        if put >= n then
            break
        end
        local x, z
        if sidex then
            x, z = if far then b.x2 + 6 else b.x1 - 6, math.random(b.z1, b.z2)
        else
            x, z = math.random(b.x1, b.x2), if far then b.z2 + 6 else b.z1 - 6
        end
        local y = EV.ground(x, b.y, z) or EV.ground(x, core.terrain_height(x, z) + 1, z)
        if y then
            EV.raider(if math.random(3) == 1 then "villagers:raider_archer" else "villagers:raider", x, y, z, rk)
            put += 1
        end
    end
    for _, p in EV.near(rk, 48) do
        core.notice(p, `The siege: wave {s.wave} of 3 comes from the {if sidex then (if far then "east" else "west")
            else (if far then "south" else "north")}.`)
    end
end

EV.end_siege = function(rk: string, s: Siege)
    for id, r in EV.raiders do
        if r[1] == rk and r[4] == nil then
            core.mob_remove(id)
            EV.raiders[id] = nil
        end
    end
    EV.siege[rk] = nil
    if not s.started then
        return
    end
    if s.lost > 0 then
        EV.announce(rk, `The siege is over. {s.lost} of the village fell.`)
        return
    end
    local names = {}
    for name in s.fought do
        table.insert(names, name)
        local pay = math.min(5, treasury(rk))
        to_treasury(rk, -pay)
        for _, q in core.players() do
            if core.player_name(q) == name then
                core.give_item(q, "ore:emerald", 5)
            end
        end
        E.credit(rk, name, 2)
        local t = trust_of(rk, name)
        if t.strikes > 0 then
            t.strikes, t.hostile = 0, false
            save_trust(rk)
        end
    end
    table.sort(names)
    EV.announce(rk, `The siege is over and no one was lost. The mayor thanks {if #names > 0
        then table.concat(names, ", ") else "the watch"}{if #names > 0 then ": 5 emeralds each" else ""}.`)
end

EV.run_siege = function(rk: string, s: Siege)
    if not s.started then
        local t = core.time_of_day()
        if is_night() then
            s.started, s.next = true, clock
        elseif t > 0.3 and t < 0.65 then
            EV.end_siege(rk, s)   -- warned and never came: a day went by
        end
        return
    end
    for _, p in EV.near(rk, 24) do
        s.fought[core.player_name(p)] = true
    end
    local up = EV.up(rk, nil)
    if s.wave < 3 and clock >= s.next then
        EV.wave(rk, s)
    elseif (s.wave >= 3 and up == 0) or not is_night() then
        EV.end_siege(rk, s)
    end
end

-- ------------------------------------------------------------ the feast

EV.FOOD = DEAR.winter
EV.price = function(rk: string, item: string, price: number): number
    return if EV.feast[rk] and EV.FOOD[item] then math.max(1, price // 2) else price
end

EV.begin_feast = function(rk: string)
    local b = villages[rk]
    local sq = EV.spot(rk, "villagers:board") or EV.spot(rk, "villages:bell")
    local mayor = watch[`{rk} mayor 1`]
    if sq == nil and mayor then
        sq = { mayor.hx, mayor.hy, mayor.hz }
    end
    if b == nil or sq == nil then
        return
    end
    core.storage_set(`feast {rk}`, tostring(math.floor(age() / DAY)))
    local f: Feast = { x = sq[1], y = sq[2], z = sq[3], put = {}, fired = false, played = 0 }
    local function put(x: number, y: number, z: number, name: string)
        local nid = core.node_id(name)
        if nid ~= 0 and core.get_block(x, y, z) == 0 then
            core.set_block(x, y, z, nid)
            table.insert(f.put, { x, y, z, nid })
        end
    end
    -- Four stalls round the square, a table with a lantern on it under an
    -- awning of wool on two posts, and a lantern on a post at each corner.
    local AWNING = { "colors:red_wool", "colors:white_wool", "colors:yellow_wool", "colors:blue_wool" }
    -- Only on ground level with the square, the three cells across clear
    -- three high: not on a roof, a slope or against a wall; nearer or
    -- further out where it is not.
    local function fits(x: number, y: number, z: number, ax: number, az: number): boolean
        if math.abs(y - f.y) > 2 then
            return false
        end
        for s = -1, 1 do
            local cx, cz = x + ax * s, z + az * s
            if not standable(cx, y, cz) or core.get_block(cx, y + 2, cz) ~= 0 then
                return false
            end
        end
        return true
    end
    for k, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
        local ax, az = if d[1] == 0 then 1 else 0, if d[1] == 0 then 0 else 1
        local x, y, z = 0, nil, 0
        for _, r in { 6, 5, 7, 4, 8 } do
            x, z = f.x + d[1] * r, f.z + d[2] * r
            y = EV.ground(x, f.y, z)
            if y and fits(x, y, z, ax, az) then
                break
            end
            y = nil
        end
        if y then
            put(x, y, z, "building:oak_table")
            put(x, y + 1, z, "building:lantern")
            for _, s in { -1, 1 } do
                put(x + ax * s, y, z + az * s, "building:oak_fence")
                put(x + ax * s, y + 1, z + az * s, "building:oak_fence")
            end
            for s = -1, 1 do
                put(x + ax * s, y + 2, z + az * s, AWNING[k])
            end
        end
    end
    for _, d in { { 4, 4 }, { -4, 4 }, { 4, -4 }, { -4, -4 } } do
        local x, z = f.x + d[1], f.z + d[2]
        local y = EV.ground(x, f.y, z)
        if y and math.abs(y - f.y) <= 2 then
            put(x, y, z, "building:oak_fence")
            put(x, y + 1, z, "building:lantern")
        end
    end
    EV.feast[rk] = f
    EV.announce(rk, "The harvest feast is today, in the square: food at half price, music, "
        .. "and fireworks tonight!")
end

EV.end_feast = function(rk: string, f: Feast)
    for i = #f.put, 1, -1 do
        local c = f.put[i]
        if core.get_block(c[1], c[2], c[3]) == c[4] then
            core.set_block(c[1], c[2], c[3], 0)
        end
    end
    EV.feast[rk] = nil
end

EV.run_feast = function(rk: string, f: Feast)
    local t = core.time_of_day()
    if t > 0.9 or t < 0.2 then
        EV.end_feast(rk, f)
        return
    end
    if clock >= f.played then
        f.played = clock + 12
        core.play_sound("villagers:music", f.x + 0.5, f.y + 1, f.z + 0.5)
    end
    if not f.fired and t >= 0.76 then
        f.fired = true
        core.event.emit("fireworks:show", { x = f.x, y = f.y + 2, z = f.z, seconds = 120 })
    end
end

-- A villager's day at the feast (life): to the square, a while there, and
-- back to work now and then.
EV.feasting = function(v: V): boolean
    local f = EV.feast[village_at(v.hx, v.hz) or ""]
    if f == nil or core.time_of_day() > 0.86 or math.random() < 0.25 then
        return false
    end
    local a = math.random() * 2 * math.pi
    local x, z = math.floor(f.x + 0.5 + math.cos(a) * (2 + math.random() * 3)),
        math.floor(f.z + 0.5 + math.sin(a) * (2 + math.random() * 3))
    local y = EV.ground(x, f.y, z)
    if y and walk(v, x, y, z, 30) then
        core.mob_face(v.id, f.x + 0.5, f.z + 0.5)
        wait(v, 10 + math.random() * 15)
    else
        wait(v, 3)
    end
    return true
end

-- ------------------------------------------------------------ the caravans

EV.save_caravans = function()
    local parts = {}
    for _, c in EV.caravans do
        local g = {}
        for item, n in c.goods do
            table.insert(g, `{item}={n}`)
        end
        table.insert(parts, table.concat({ c.from, c.to, math.floor(c.x), math.floor(c.y), math.floor(c.z),
                                            table.concat(g, ","), c.escort }, "|"))
    end
    core.storage_set("caravans", table.concat(parts, ";"))
end

core.event.on("core:world_ready", function()
    for part in string.gmatch(core.storage_get("caravans"), "[^;]+") do
        local f = string.split(part, "|")
        if #f >= 6 then
            local goods = {}
            for item, n in string.gmatch(f[6], "([^,=]+)=(%d+)") do
                goods[item] = tonumber(n) :: number
            end
            table.insert(EV.caravans, { from = f[1], to = f[2], x = tonumber(f[3]) :: number, y = tonumber(f[4]) :: number,
                                        z = tonumber(f[5]) :: number, goods = goods, m = 0, mule = 0, escort = f[7] or "",
                                        best = math.huge, best_at = 0, att_at = 0, k = 1 })
        end
    end
end)

EV.middle = function(rk: string): (number?, number?)
    local b = villages[rk]
    if b then
        return (b.x1 + b.x2) // 2, (b.z1 + b.z2) // 2
    end
    local k = E.known[rk]
    if k then
        return k[1], k[2]
    end
    return nil, nil
end

-- Sets a caravan off from a village, if another lived in is far enough and
-- its chests have plenty of something; true when it went.
EV.send = function(rk: string): boolean
    for _, c in EV.caravans do
        if c.from == rk then
            return false
        end
    end
    local mx, mz = EV.middle(rk)
    local merchant = watch[`{rk} merchant 1`]
    if mx == nil or mz == nil or merchant == nil then
        return false
    end
    local to, td = nil, math.huge
    for other in E.known do
        local ox, oz = EV.middle(other)
        if other ~= rk and ox and oz then
            local d = math.sqrt((ox - mx) ^ 2 + (oz - mz) ^ 2)
            if d >= 100 and d < 2000 and d < td then
                to, td = other, d
            end
        end
    end
    if to == nil then
        return false
    end
    local src = village_chests(rk)
    local tx, tz = EV.middle(to)
    local dst = if tx and villages[to] and core.loaded(tx, villages[to].y, tz) == 1 then village_chests(to) else nil
    local can = {}
    for _, g in GOODS do
        if stock_of(src, g[1]) >= g[2] + (g[2] + 1) // 2 and (dst == nil or stock_of(dst, g[1]) < g[2]) then
            table.insert(can, g)
        end
    end
    local goods, any = {}, false
    for _ = 1, 3 do
        if #can == 0 then
            break
        end
        local g = table.remove(can, math.random(#can))
        -- What it has over a bundle, kept at home: a bundle at most.
        local n = take_from(src, g[1], math.min(g[2], stock_of(src, g[1]) - g[2]))
        if n > 0 then
            goods[g[1]], any = n, true
        end
    end
    if not any then
        return false
    end
    local mxp, myp, mzp = merchant.hx, merchant.hy, merchant.hz
    if merchant.id ~= 0 then
        local x, y, z = core.mob_pos(merchant.id)
        if x and y and z then
            mxp, myp, mzp = math.floor(x), math.floor(y), math.floor(z)
        end
    end
    table.insert(EV.caravans, { from = rk, to = to, x = mxp, y = myp, z = mzp, goods = goods, m = 0, mule = 0,
                                escort = "", best = math.huge, best_at = clock, att_at = 0, k = 1 })
    EV.save_caravans()
    local things = {}
    for item, n in goods do
        table.insert(things, `{n} {said(item)}`)
    end
    table.sort(things)
    EV.announce(rk, `A caravan leaves for the village at {tx} {tz}, with {table.concat(things, ", ")}. `
        .. "Escort it and be paid when it gets there.")
    return true
end

-- What a village's land is known for, by its middle's climate: what a
-- caravan from a village nobody has been to brings (item, a bundle, price).
EV.TYPICAL = {
    ["core:desert"] = { { "rocks:sandstone", 32, 1 }, { "plants:cactus", 8, 1 }, { "core:sand", 32, 1 },
                        { "ore:raw_gold", 1, 1 } },
    ["core:tundra"] = { { "woods:spruce_log", 8, 1 }, { "animals:leather", 4, 1 }, { "animals:wool", 8, 1 },
                        { "pets:cooked_fish", 4, 1 } },
    ["core:beach"] = { { "pets:cooked_fish", 4, 1 }, { "pets:fish", 6, 1 }, { "core:sand", 32, 1 },
                       { "rocks:cobblestone", 32, 1 } },
    high = { { "ore:raw_iron", 3, 1 }, { "ore:coal", 8, 1 }, { "rocks:cobblestone", 32, 1 }, { "core:stone", 24, 1 } },
    other = { { "farm:wheat", 12, 1 }, { "farm:bread", 4, 1 }, { "trees:trunk", 8, 1 }, { "animals:wool", 8, 1 },
              { "animals:steak", 4, 1 } },
}
-- A good by its item: the merchant's (GOODS) or a land's.
EV.good = function(item: string): { any }?
    for _, g in GOODS do
        if g[1] == item then
            return g
        end
    end
    for _, list in EV.TYPICAL do
        for _, g in list do
            if g[1] == item then
                return g
            end
        end
    end
    return nil
end

EV.value = function(c: Caravan): number
    local v = 0
    for item, n in c.goods do
        local g = EV.good(item)
        if g then
            v += math.ceil(n / g[2]) * g[3]
        end
    end
    return v
end

-- The way between two villages along the paths (mods/villages), a point
-- every 4 nodes; {} where the paths do not join them.
EV.got = ""
core.event.on("villages:route", function(ev)
    EV.got = ev.points
end)
EV.way = function(from: string, to: string): { { number } }
    local ax, az = string.match(from, "^(-?%d+),(-?%d+)$")
    local bx, bz = string.match(to, "^(-?%d+),(-?%d+)$")
    local out = {}
    if ax and bx then
        EV.got = ""
        core.event.emit("villages:route_ask", { rx1 = tonumber(ax) :: number, rz1 = tonumber(az) :: number,
                                               rx2 = tonumber(bx) :: number, rz2 = tonumber(bz) :: number })
        for x, z in string.gmatch(EV.got, "(-?%d+),(-?%d+)") do
            table.insert(out, { tonumber(x) :: number + 0.5, tonumber(z) :: number + 0.5 })
        end
    end
    return out
end
EV.route_of = function(c: Caravan): { { number } }
    if c.route == nil then
        local r = EV.way(c.from, c.to)
        -- On it from the point nearest where the caravan is.
        local best, bd = 1, math.huge
        for k, q in r do
            local d = (q[1] - c.x) ^ 2 + (q[2] - c.z) ^ 2
            if d < bd then
                best, bd = k, d
            end
        end
        c.route, c.k = r, best
    end
    return c.route :: { { number } }
end
-- Where a caravan heads: `ahead` points on along its route, or the
-- village's middle off the end of it (or with none).
EV.aim = function(c: Caravan, ahead: number): (number, number)
    local r = EV.route_of(c)
    while c.k <= #r and (r[c.k][1] - c.x) ^ 2 + (r[c.k][2] - c.z) ^ 2 < 36 do
        c.k += 1
    end
    if c.k > #r then
        local tx, tz = EV.middle(c.to)
        return tx or c.x, tz or c.z
    end
    local q = r[math.min(#r, c.k + ahead)]
    return q[1], q[2]
end
-- How far it has still to go, along its route.
EV.remaining = function(c: Caravan, d: number): number
    local r = c.route
    if r == nil or c.k > #r then
        return d
    end
    return (#r - c.k) * 4 + math.sqrt((r[c.k][1] - c.x) ^ 2 + (r[c.k][2] - c.z) ^ 2)
end

-- A caravan from a village near that nobody has been to, to this one (lived
-- in, a player near): true when one set off.
EV.incoming = function(rk: string): boolean
    for _, c in EV.caravans do
        if c.to == rk then
            return false
        end
    end
    local mx, mz = EV.middle(rk)
    if mx == nil or mz == nil then
        return false
    end
    local can = {}
    for other, k in E.known do
        local d = math.sqrt((k[1] - mx) ^ 2 + (k[2] - mz) ^ 2)
        if villages[other] == nil and d >= 100 and d < 900 then
            table.insert(can, other)
        end
    end
    while #can > 0 do
        local from = table.remove(can, math.random(#can))
        local r = EV.way(from, rk)
        if #r > 0 then
            local k = E.known[from]
            local h = core.terrain_height(k[1], k[2])
            local land = EV.TYPICAL[core.biome(k[1], k[2])] or (if h > 40 then EV.TYPICAL.high else EV.TYPICAL.other)
            local list = table.clone(land)
            local goods, things = {}, {}
            for _ = 1, 3 do
                if #list == 0 then
                    break
                end
                local g = table.remove(list, math.random(#list))
                if core.item_id(g[1]) ~= 0 then
                    goods[g[1]] = g[2]
                    table.insert(things, `{g[2]} {said(g[1])}`)
                end
            end
            if next(goods) == nil then
                return false
            end
            table.insert(EV.caravans, { from = from, to = rk, x = r[1][1], y = core.terrain_height(r[1][1], r[1][2]),
                                        z = r[1][2], goods = goods, m = 0, mule = 0, escort = "", best = math.huge,
                                        best_at = clock, att_at = 0, route = r, k = 1 })
            EV.save_caravans()
            table.sort(things)
            core.log(`villagers: a caravan sets off from the village at {k[1]} {k[2]} for {rk}, `
                .. `{#r * 4} nodes along the paths, with {table.concat(things, ", ")}`)
            return true
        end
    end
    return false
end

-- The way to a caravan's own village, told to a player: aloud, and on
-- their map.
EV.tell_way = function(p: number, c: Caravan)
    local fx, fz = EV.middle(c.from)
    local px, _, pz = core.player_pos(p)
    if fx == nil or fz == nil or px == nil or pz == nil then
        return
    end
    local dx, dz = fx - px, fz - pz
    local dirs = { "north", "north-east", "east", "south-east", "south", "south-west", "west", "north-west" }
    local dir = dirs[math.floor((math.atan2(dx, -dz) / (2 * math.pi) * 8 + 0.5)) % 8 + 1]
    local far = math.floor(math.sqrt(dx * dx + dz * dz))
    local back = EV.way(c.to, c.from)
    if #back > 0 then
        -- From where the player stands: the route's point nearest them on.
        local best, bd = 1, math.huge
        for k, q in back do
            local d = (q[1] - px) ^ 2 + (q[2] - pz) ^ 2
            if d < bd then
                best, bd = k, d
            end
        end
        local parts = { `{math.floor(px)},{math.floor(pz)}` }
        for k = best, #back do
            table.insert(parts, `{math.floor(back[k][1])},{math.floor(back[k][2])}`)
        end
        table.insert(parts, `{fx},{fz}`)
        core.event.emit("maps:route", { player = p, label = `village {c.from}`, points = table.concat(parts, ";") })
    end
    core.notice(p, `"Our village is {far} nodes to the {dir}, at {fx} {fz}. `
        .. (if #back > 0 then `Keep to the path: I have marked it on your map."` else `There is no path: go by the sun."`))
end

EV.bodies_off = function(c: Caravan)
    for id, r in EV.raiders do
        if r[4] == c then
            core.mob_remove(id)
            EV.raiders[id] = nil
        end
    end
    for _, id in { c.m, c.mule } do
        if id ~= 0 then
            core.mob_remove(id)
        end
    end
    c.m, c.mule = 0, 0
end

EV.arrive = function(i: number, c: Caravan, tx: number, tz: number): boolean
    local b, e = villages[c.to], extents[c.to]
    if b == nil and e ~= nil then
        b = { x1 = e[1], z1 = e[2], x2 = e[3], z2 = e[4], y = core.terrain_height(tx, tz), homes = {} }
    end
    if b == nil or core.loaded(tx, b.y, tz) ~= 1 then
        return false   -- waits there to be seen unloading
    end
    local chests = village_chests(c.to, b)
    local things = {}
    for item, n in c.goods do
        local left = put_into(chests, item, n)
        if left > 0 then
            core.drop_item(tx, b.y + 1, tz, core.item_id(item), left)
        end
        table.insert(things, `{n} {said(item)}`)
    end
    local value = EV.value(c)
    to_treasury(c.from, value)
    to_treasury(c.to, -value)
    local fx, fz = EV.middle(c.from)
    local pay = 2 + math.floor(math.sqrt(((fx or tx) - tx) ^ 2 + ((fz or tz) - tz) ^ 2) / 200)
    for _, p in core.players() do
        local px, _, pz = core.player_pos(p)
        if core.player_name(p) == c.escort and px and pz and (px - c.x) ^ 2 + (pz - c.z) ^ 2 < 40 * 40 then
            core.give_item(p, "ore:emerald", pay)
            to_treasury(c.from, -pay)
            core.notice(p, `The caravan is in: {ems(pay)} for the escort.`)
            E.credit(c.from, c.escort, 1)
        end
    end
    EV.bodies_off(c)
    table.remove(EV.caravans, i)
    EV.save_caravans()
    table.sort(things)
    EV.announce(c.to, `A caravan from the village at {fx} {fz} has come in, with {table.concat(things, ", ")}.`)
    return true
end

EV.lost = function(i: number, c: Caravan)
    for item, n in c.goods do
        core.drop_item(math.floor(c.x), math.floor(c.y) + 1, math.floor(c.z), core.item_id(item), n)
    end
    EV.bodies_off(c)
    table.remove(EV.caravans, i)
    EV.save_caravans()
    EV.announce(c.from, `The caravan was lost on the road, at {math.floor(c.x)} {math.floor(c.z)}: its goods lie there.`)
end

EV.seen = function(x: number, z: number, r: number): boolean
    for _, p in core.players() do
        local px, _, pz = core.player_pos(p)
        if px and pz and (px - x) ^ 2 + (pz - z) ^ 2 < r * r then
            return true
        end
    end
    return false
end

-- The next stretch of the road: a cell `far` ahead that a way on foot (no
-- swimming) reaches, straight on if it can, else turning off by up to 75
-- degrees either side -- round a lake, a cliff, a wood -- and the one
-- straight on, to be lifted over, when none does.
EV.onward = function(id: number, x: number, y: number, z: number, ux: number, uz: number, far: number)
    local fx, fy, fz = math.floor(x), math.floor(y + 0.01), math.floor(z)
    local first = nil
    for _, a in { 0, 0.45, -0.45, 0.9, -0.9, 1.3, -1.3 } do
        local cx, cz = ux * math.cos(a) - uz * math.sin(a), ux * math.sin(a) + uz * math.cos(a)
        local nx, nz = math.floor(x + cx * far), math.floor(z + cz * far)
        local ny = EV.ground(nx, fy, nz)
        if ny and math.abs(ny - fy) <= 5 then
            first = first or { nx, ny, nz }
            if #core.find_path(fx, fy, fz, nx, ny, nz, 2, 1, 3, false, 2000) > 0 then
                core.mob_goto(id, nx, ny, nz)
                return
            end
        end
    end
    if first then
        core.mob_goto(id, first[1], first[2], first[3])
    end
end

-- Every second: on along the road, walking when seen, carried on when not.
EV.roads = function(dt: number)
    for i = #EV.caravans, 1, -1 do
        local c = EV.caravans[i]
        local tx, tz = EV.middle(c.to)
        if tx == nil or tz == nil then
            table.remove(EV.caravans, i)
            continue
        end
        local d = math.sqrt((tx - c.x) ^ 2 + (tz - c.z) ^ 2)
        if d < 12 then
            EV.arrive(i, c, tx, tz)
            continue
        end
        -- Its heading: along its route, a little ahead while it walks.
        local hx, hz = EV.aim(c, if c.m ~= 0 then 2 else 0)
        local dx, dz = hx - c.x, hz - c.z
        local hd = math.max(0.01, math.sqrt(dx * dx + dz * dz))
        dx, dz = dx / hd, dz / hd
        local left = EV.remaining(c, d)
        if c.m ~= 0 then
            local x, y, z = core.mob_pos(c.m)
            if x == nil or y == nil or z == nil then
                if core.loaded(math.floor(c.x), math.floor(c.y), math.floor(c.z)) == 1 then
                    EV.lost(i, c)   -- killed
                else
                    EV.bodies_off(c)
                end
                continue
            end
            c.x, c.y, c.z = x, y, z
            if not EV.seen(x, z, 96) then
                EV.bodies_off(c)
                continue
            end
            if left < c.best - 1 then
                c.best, c.best_at = left, clock
            elseif clock - c.best_at > 12 then
                -- Stuck: over whatever is in the way.
                local nx, nz = math.floor(x + dx * 5), math.floor(z + dz * 5)
                local ny = EV.ground(nx, math.floor(y), nz)
                if ny then
                    core.mob_place(c.m, nx + 0.5, ny, nz + 0.5)
                    core.log(`villagers: a caravan to {c.to} lifted on at {nx} {ny} {nz}`)
                end
                c.best_at = clock
            end
            if core.mob_going(c.m) == 0 then
                -- On the path where it has one; else the way it can find.
                local gx, gz = math.floor(hx), math.floor(hz)
                local gy = if c.k <= #(c.route or {}) then EV.ground(gx, math.floor(y), gz) else nil
                if gy and math.abs(gy - y) <= 6 then
                    core.mob_goto(c.m, gx, gy, gz)
                else
                    EV.onward(c.m, x, y, z, dx, dz, math.min(hd, 10))
                end
            end
            if c.mule ~= 0 then
                local ux, uy, uz = core.mob_pos(c.mule)
                if ux == nil or uy == nil or uz == nil then
                    c.mule = 0
                elseif (ux - x) ^ 2 + (uz - z) ^ 2 > 3 * 3 and core.mob_going(c.mule) == 0 then
                    local bx, bz = math.floor(x - dx * 2), math.floor(z - dz * 2)
                    core.mob_goto(c.mule, bx, EV.ground(bx, math.floor(y), bz) or math.floor(y), bz)
                end
            end
            if is_night() and clock >= c.att_at and EV.up("caravan", c) == 0 then
                -- Two out of the dark every minute or two, once the last are down.
                c.att_at = clock + 60 + math.random() * 60
                local put = 0
                for _ = 1, 8 do
                    local a = math.random() * 2 * math.pi
                    local rx, rz = math.floor(x + math.cos(a) * 14), math.floor(z + math.sin(a) * 14)
                    local ry = EV.ground(rx, math.floor(y), rz)
                    if ry and put < 2 then
                        EV.raider("villagers:raider", rx, ry, rz, "caravan", c)
                        put += 1
                    end
                end
            end
        else
            local step = math.min(hd, 2.2 * dt)
            c.x += dx * step
            c.z += dz * step
            local x, z = math.floor(c.x), math.floor(c.z)
            if EV.seen(c.x, c.z, 64) and core.loaded(x, math.floor(c.y), z) == 1 then
                local y = EV.ground(x, math.floor(c.y), z)
                if y == nil then
                    local h = core.terrain_height(x, z)
                    y = EV.ground(x, h + 1, z)
                end
                if y then
                    c.y = y
                    c.m = core.spawn_mob("villagers:merchant", x + 0.5, y, z + 0.5)
                    local bx, bz = math.floor(c.x - dx * 2), math.floor(c.z - dz * 2)
                    c.mule = core.spawn_mob("villagers:mule", bx + 0.5, EV.ground(bx, y, bz) or y, bz + 0.5)
                    c.best, c.best_at = left, clock
                end
            end
        end
    end
end

-- The caravan's panel: its cargo to buy, at the market's price, and the escort.
EV.show = function(p: number, c: Caravan)
    local rows = {}
    for item, n in c.goods do
        local g = EV.good(item)
        if g and n >= g[2] then
            table.insert(rows, { g, price_of(g, n) })
        end
    end
    local name = panel_of(p, "caravan")
    local tx, tz = EV.middle(c.to)
    local fx, fz = EV.middle(c.from)
    core.ui_define(name, 15, 6.2 + #rows * 1.2)
    core.ui_label(name, 0.4, 0.3, `A caravan from the village at {fx} {fz}, on its way to the one at {tx} {tz}.`)
    for i, r in rows do
        core.ui_button(name, `b {i}`, 0.4, 0.2 + i * 1.2, 14.2, 1.1, `Buy {r[1][2]} {said(r[1][1])}: {ems(r[2])}`)
    end
    local who = core.player_name(p)
    local y = 1.6 + #rows * 1.2
    if c.escort == who then
        core.ui_label(name, 0.4, y + 0.2, "You are its escort: stay with it to the end of the road.")
    elseif c.escort ~= "" then
        core.ui_label(name, 0.4, y + 0.2, `{c.escort} escorts it.`)
    else
        core.ui_button(name, "escort", 0.4, y, 14.2, 1.1, "Escort it (paid when it gets there)")
    end
    core.ui_button(name, "way", 0.4, y + 1.4, 14.2, 1.1, "Ask the way to its village")
    core.ui_button(name, "close", 0.4, y + 2.8, 14.2, 1.1, "Close")
    EV.talk[p] = c
    EV.rows = EV.rows or {}
    EV.rows[p] = rows
    core.ui_show(name, true, p)
end

core.event.on("core:used_mob", function(ev)
    for _, c in EV.caravans do
        if ev.target ~= 0 and (ev.target == c.m or ev.target == c.mule) then
            EV.show(ev.player, c)
            return
        end
    end
end)

core.event.on("core:ui_click", function(ev)
    if ev.panel ~= panel_of(ev.player, "caravan") then
        return
    end
    core.ui_show(ev.panel, false, ev.player)
    local c = EV.talk[ev.player]
    if c == nil or table.find(EV.caravans, c) == nil then
        return
    end
    if ev.id == "way" then
        EV.tell_way(ev.player, c)
        return
    end
    if ev.id == "escort" and c.escort == "" then
        c.escort = core.player_name(ev.player)
        EV.save_caravans()
        core.notice(ev.player, "You escort the caravan: stay with it to the end of the road.")
        return
    end
    local i = tonumber(string.match(ev.id, "^b (%d+)$") or "")
    local r = i and EV.rows[ev.player] and EV.rows[ev.player][i]
    if r == nil or (c.goods[r[1][1]] or 0) < r[1][2] then
        return
    end
    local got = core.take_item(ev.player, "ore:emerald", r[2])
    if got < r[2] then
        if got > 0 then
            core.give_item(ev.player, "ore:emerald", got)
        end
        core.notice(ev.player, `That is {ems(r[2])}.`)
        return
    end
    core.give_item(ev.player, r[1][1], r[1][2])
    c.goods[r[1][1]] -= r[1][2]
    if c.goods[r[1][1]] <= 0 then
        c.goods[r[1][1]] = nil
    end
    to_treasury(c.from, r[2])
    EV.save_caravans()
    EV.show(ev.player, c)
end)

-- ------------------------------------------------------------ the clock

core.command("village", "siege|feast|caravan|incoming -- start one now in the village you stand in", "admin")
core.event.on("core:command", function(ev)
    if ev.name ~= "village" then
        return
    end
    local x, _, z = core.player_pos(ev.player)
    local rk = x and z and village_at(x, z)
    local what = string.match(ev.args, "^%s*(%a+)")
    if rk == nil then
        core.chat(ev.player, "Not in a village.")
    elseif what == "siege" then
        EV.begin_siege(rk)
    elseif what == "feast" then
        EV.begin_feast(rk)
    elseif what == "caravan" then
        if not EV.send(rk) then
            core.chat(ev.player, "No caravan: none to go to, one out already, or nothing to spare.")
        end
    elseif what == "incoming" then
        if not EV.incoming(rk) then
            core.chat(ev.player, "No caravan: no village near unvisited and joined by the paths, or one coming already.")
        end
    else
        core.chat(ev.player, "Usage: /village siege|feast|caravan|incoming")
    end
end)

local every, sent = 0.0, 0.0
core.event.on("core:tick", function(ev)
    every += ev.dt
    if every < 1 then
        return
    end
    local dt = every
    every = 0
    local t = core.time_of_day()
    local today = math.floor(age() / DAY)
    for rk in villages do
        local s = EV.siege[rk]
        if s then
            EV.run_siege(rk, s)
        elseif t > 0.70 and t < 0.78 and EV.asked[rk] ~= today then
            EV.asked[rk] = today
            local last = tonumber(core.storage_get(`siege {rk}`)) or 0
            if #EV.near(rk, 32) > 0 and age() - last >= 7 * DAY and math.random(3) == 1 then
                EV.begin_siege(rk)
            end
        end
        local f = EV.feast[rk]
        if f then
            EV.run_feast(rk, f)
        elseif season == "summer" and EV.left == 0 and t > 0.3 and t < 0.7
            and core.storage_get(`feast {rk}`) ~= tostring(today) and #EV.near(rk, 64) > 0 then
            EV.begin_feast(rk)
        end
    end
    for id, r in EV.raiders do
        EV.steer(id, r)
    end
    EV.roads(dt)
    if clock >= sent then
        sent = clock + 60
        for rk in villages do
            if #EV.near(rk, 64) > 0 then
                if math.random(20) == 1 then
                    EV.send(rk)
                elseif math.random(20) == 1 then
                    EV.incoming(rk)
                end
            end
        end
    end
end)
end

supply = function(v: V): boolean
    if clock - (v.supplied or -999) < 30 then
        return false
    end
    v.supplied = clock
    local want: { { any } } = {}
    -- Every tool of its trade it might find, best first; it takes one.
    local ladder = TOOLING.ladder[v.kind] or {}
    if TOOL[v.kind] ~= nil and TOOLING.of(v) == nil then
        for _, t in ladder do
            table.insert(want, { t, 1 })
        end
    end
    if v.kind == "miner" and (v.inv["torches:torch"] or 0) < 4 then
        table.insert(want, { "torches:torch", 12 })
    end
    local hungry = clock - v.fed_at > HUNGRY_EVERY and food_of(v) == nil
    if hungry then
        for _, f in FOODS do
            table.insert(want, { f, 3 })
        end
    end
    local rk = if #want > 0 then village_at(v.hx, v.hz) else nil
    if rk == nil then
        return false
    end
    -- The nearest chest holding any of it.
    local px, _, pz = core.mob_pos(v.id)
    local best, bc = math.huge, nil
    for _, c in village_chests(rk) do
        for _, w in want do
            local iid = core.item_id(w[1])
            if iid ~= 0 and core.container_count(c[1], c[2], c[3], iid) > 0 then
                local d = (c[1] - (px or v.hx)) ^ 2 + (c[3] - (pz or v.hz)) ^ 2
                if d < best then
                    best, bc = d, c
                end
            end
        end
    end
    if bc == nil or not reach(v, bc[1], bc[2], bc[3]) then
        return false
    end
    act(v, "gather", "", 1.6, bc[1], bc[3])
    local got, fed, tooled = {}, false, false
    for _, w in want do
        local food = table.find(FOODS, w[1]) ~= nil
        local tool = table.find(ladder, w[1]) ~= nil
        if not (food and fed) and not (tool and tooled) then
            local n = take_from({ bc }, w[1], w[2])
            if n > 0 then
                add(v, w[1], n)
                table.insert(got, `{n} {w[1]}`)
                fed = fed or food
                if tool then
                    tooled = true
                    v.uses = 0
                end
            end
        end
    end
    if #got > 0 then
        core.log(`villagers: the {v.kind} at {v.key} took {table.concat(got, ", ")} from the village's chests`)
    end
    return true
end

-- What the CRAFTER makes for the village, and of what: the tools its
-- workers wear out and torches for its miners, two of each tool (16
-- torches) kept in its chests for whoever comes for them, only for the
-- kinds the village has. Sticks it makes of planks, planks of logs.
local MAKES = {
    -- Iron first, when the chests hold the ingots; stone otherwise. Two of a
    -- kind is two of any material (stocked, below).
    { "furnace:iron_pick", 1, { ["furnace:iron_ingot"] = 3, ["timber:stick"] = 2 }, "miner" },
    { "furnace:iron_axe", 1, { ["furnace:iron_ingot"] = 3, ["timber:stick"] = 2 }, "woodcutter" },
    { "furnace:iron_shovel", 1, { ["furnace:iron_ingot"] = 1, ["timber:stick"] = 2 }, "digger" },
    { "furnace:iron_hoe", 1, { ["furnace:iron_ingot"] = 2, ["timber:stick"] = 2 }, "human" },
    { "tools:pick", 1, { ["rocks:cobblestone"] = 3, ["timber:stick"] = 2 }, "miner" },
    { "tools:axe", 1, { ["rocks:cobblestone"] = 3, ["timber:stick"] = 2 }, "woodcutter" },
    { "tools:shovel", 1, { ["rocks:cobblestone"] = 1, ["timber:stick"] = 2 }, "digger" },
    { "farm:hoe", 1, { ["timber:planks"] = 2, ["timber:stick"] = 2 }, "human" },
    { "torches:torch", 4, { ["ore:coal"] = 1, ["timber:stick"] = 1 }, "miner" },
}
local KEEP_MADE = { ["torches:torch"] = 16 }

-- n of planks or sticks in the chests, made of what is there when short.
local function enough(chests: { { number } }, item: string, n: number): boolean
    while stock_of(chests, item) < n do
        if item == "timber:stick" then
            if not enough(chests, "timber:planks", 2) then
                return false
            end
            take_from(chests, "timber:planks", 2)
            put_into(chests, "timber:stick", 4)
        elseif item == "timber:planks" then
            local log = nil
            for _, l in LOG_NAMES do
                if stock_of(chests, l) > 0 then
                    log = l
                    break
                end
            end
            if log == nil then
                return false
            end
            take_from(chests, log, 1)
            put_into(chests, "timber:planks", 4)
        else
            return false
        end
    end
    return true
end

crafter_life = function(v: V)
    local rk = v.rk :: string
    while true do
        if sleeps_now(v) and rest(v) then
            continue
        end
        local chests = village_chests(rk)
        local made = false
        for _, m in MAKES do
            local ladder = TOOLING.ladder[m[4]]
            local stocked = 0
            if ladder ~= nil and table.find(ladder, m[1]) then
                for _, t in ladder do
                    stocked += stock_of(chests, t)
                end
            else
                stocked = stock_of(chests, m[1])
            end
            if #chests > 0 and has_kind(rk, m[4]) and stocked < (KEEP_MADE[m[1]] or 2) then
                local ok = true
                for item, n in m[3] do
                    ok = ok and enough(chests, item, n)
                end
                if ok then
                    -- At the anvil or a crafting table near its post.
                    local wx, wy, wz = nearest({ "building:anvil", "craft:table" }, v.hx, v.hy, v.hz, 24, 6)
                    if wx ~= nil and wy ~= nil and wz ~= nil then
                        reach(v, wx, wy, wz)
                    end
                    act(v, "gather", m[1], 3.0, wx, wz)
                    for item, n in m[3] do
                        take_from(chests, item, n)
                    end
                    local left = put_into(chests, m[1], m[2])
                    if left > 0 then
                        local px, py, pz = core.mob_pos(v.id)
                        core.drop_item(math.floor(px or v.hx), math.floor(py or v.hy), math.floor(pz or v.hz),
                            core.item_id(m[1]), left)
                    end
                    core.log(`villagers: {rk}: the crafter made {m[2]} {m[1]}`)
                    made = true
                    break
                end
            end
        end
        if not made then
            patrol(v)
            wait(v, 15)
        end
        wait(v, 0.5)
    end
end

-- Near enough a cell to work on it: within 5, or one walk to the standable
-- cell nearest it; where there is no way there (up a wall it is raising,
-- shut in by it), up its scaffolding: put there.
local function near(v: V, x: number, y: number, z: number): boolean
    local px, py, pz = core.mob_pos(v.id)
    if px == nil or py == nil or pz == nil or ((px - x - 0.5) ^ 2 + (pz - z - 0.5) ^ 2 < 36 and math.abs(py - y) < 6) then
        return true
    end
    local best, bx, by, bz = math.huge, nil, nil, nil
    for dx = -3, 3 do
        for dz = -3, 3 do
            for dy = 3, -4, -1 do
                local sx, sy, sz = x + dx, y + dy, z + dz
                local d = dx * dx + dz * dz + dy * dy * 0.5
                if d < best and standable(sx, sy, sz) and not failed(v, sx, sy, sz) then
                    best, bx, by, bz = d, sx, sy, sz
                end
            end
        end
    end
    if bx == nil or by == nil or bz == nil then
        return false
    end
    if not walk(v, bx, by, bz, 6) then
        core.mob_place(v.id, bx + 0.5, by + 0.02, bz + 0.5)
    end
    return true
end

local function build(v: V, rk: string, o: Order)
    -- A cell's last block in the plan only: a wall cleared and then put
    -- again was dug out and rebuilt on every build taken up again (a
    -- restart), and paid for twice.
    local plan = plan_of(o)
    local final: { [string]: number } = {}
    for i, p in plan do
        final[`{p[1]},{p[2]},{p[3]}`] = i
    end
    for i, p in plan do
        local x, y, z, name, facing = p[1], p[2], p[3], p[4], p[5]
        if final[`{x},{y},{z}`] ~= i then
            continue
        end
        local want = if name == "" then AIR else id(name)
        local now = core.get_block(x, y, z)
        -- A way does not go through what somebody built: it steps round.
        local built = p[6] and now ~= AIR and core.node_solid(now) == 1 and not is_in("natural", NATURAL, now)
            and not tree_log(x, y, z) and not is_in("leaves", LEAF_NAMES, now)
            and not is_in("road", ROAD_BLOCKS, now)
        if now ~= want and not built then
            near(v, x, y, z)
            local c = cost_of(name)
            if c ~= nil then
                o.have[c[1]] = math.max(0, (o.have[c[1]] or 0) - c[2])
            end
            act(v, if name == "" then "dig" else "chop", if name == "" then "" else name, 0.25, x, z)
            core.set_block(x, y, z, want)
            if facing ~= nil then
                core.set_facing(x, y, z, facing)
            end
            if name == "chest:box" then
                core.container_make(x, y, z, 27)
            end
        end
    end
end

-- How a kind of thing is fetched, by whoever fetches it (set with the gatherers).
local GATHER: { [string]: (V) -> () } = {}

builder_life = function(v: V)
    local rk = v.rk :: string
    -- The village's own order: out of its chests first, what they hold of
    -- what it lacks, every minute or so as they fill. Its people's goods
    -- are the village's (no gatherer brought a block to the hall's keepers'
    -- houses in fifteen minutes: there was none in the village).
    local chests_at = 0.0
    local function from_chests(o: Order, lack: { [string]: number }): boolean
        if o.buyer ~= "" or clock < chests_at then
            return false
        end
        chests_at = clock + 60
        local chests = village_chests(rk)
        local got = 0
        for m, n in lack do
            for _, item in ITEMS[m] or {} do
                local want = n - got
                if want <= 0 then
                    break
                end
                if stock_of(chests, item) > 0 then
                    local c = chests[1]
                    reach(v, c[1], c[2], c[3])
                    local took = take_from(chests, item, math.min(want, 64))
                    if took > 0 then
                        got += deliver(o, m, took)
                        core.log(`villagers: {rk}: the builder took {took} {item} from the village's chests`)
                    end
                end
            end
            got = 0
        end
        save_orders(rk)
        return true
    end
    while true do
        local o = (orders[rk] or {})[1]
        if o == nil or is_night() then
            if is_night() then
                if not rest(v) then
                    walk(v, v.hx, v.hy, v.hz, 40)
                    wait(v, 10)
                end
            else
                patrol(v)
            end
        elseif o.stage == "gather" then
            local _, lack = wanting(rk)
            if next(lack) == nil then
                core.log(`villagers: {rk}: building {if o.buyer == "" then "the village" else o.buyer}'s {o.kind}`)
                o.stage = "build"
                save_orders(rk)
            elseif from_chests(o, lack) then
                -- (taken what there was; looked at again next time round)
            elseif o.self or o.buyer == "" then
                -- Told to find it itself: it goes for what no gatherer of
                -- the village fetches first, then helps with the rest.
                local m = next(lack) :: string
                for _, k in MATS do
                    if lack[k] ~= nil and not has_kind(rk, BRINGS[k]) then
                        m = k
                        break
                    end
                end
                v.task = m
                GATHER[m](v)
            else
                local nobody = false
                for m in lack do
                    if not has_kind(rk, BRINGS[m]) then
                        nobody = true
                    end
                end
                if (nobody or clock - o.since > 120) and clock - o.asked > 60 then
                    o.asked = clock
                    ask(o, v, lack)
                end
                wait(v, 3)
            end
        else
            build(v, rk, o)
            core.log(`villagers: {rk}: {if o.buyer == "" then "the village" else o.buyer}'s {o.kind} built`)
            if o.kind == "house" then
                houses[rk] = houses[rk] or {}
                table.insert(houses[rk], { x0 = o.x0, z0 = o.z0, t = o.t, floor = o.floor, w = o.w, d = o.d,
                    style = o.style, storeys = 1, garden = false, buyer = o.buyer })
                save_houses(rk)
            elseif o.kind == "floor" or o.kind == "garden" then
                local h = house_of(rk, o.buyer)
                if h ~= nil then
                    if o.kind == "floor" then
                        h.storeys = 2
                    else
                        h.garden = true
                    end
                    save_houses(rk)
                end
            end
            table.remove(orders[rk], 1)
            save_orders(rk)
            for _, p in core.players() do
                if core.player_name(p) == o.buyer then
                    local cx, cz = at(o, o.w // 2, o.d // 2)
                    core.notice(p, ({ house = `Your house at {cx} {cz} is ready.`,
                        floor = "Your house's second storey is ready.", garden = "Your garden is sown.",
                        lot = `Your lot at {cx} {cz} is ready to build on.` })[o.kind] or "Done.")
                end
            end
        end
        wait(v, 0.5)
    end
end

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
    local b = villages[region_of(v.hx, v.hz)]
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

out_of_mine = function(v: V, to_y: number)
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
        if item == "ore:emerald" then
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
        if mn == nil or (chest_full[rk] or 0) > clock then
            stroll(v)
            return
        end
        if carrying(v, m) + ore_carried(v) >= 16 then
            if not to_mine_chest(v, mn, true) then
                chest_full[rk] = clock + 600
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
        v.back_at = clock + 300
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
                v.wake, v.walking = clock, false
            end
        end
        if p ~= nil or m ~= nil then
            return
        end
    end
    if v.co ~= nil and (clock >= v.wake or (v.walking and core.mob_going(v.id) == 0)) then
        local ok, err = coroutine.resume(v.co)
        if not ok then
            core.log(`villagers: {v.kind} at {v.key}: {err}`)
            v.co = coroutine.create(function() live(v) end)
        end
    end
end

-- ---------------------------------------------------------------- the clock

core.event.on("core:world_ready", function()
    age_at_start = tonumber(core.storage_get("age")) or 0
    survey()
end)

local looked_at, step_at, aged_at = 0.0, 0.0, 0.0
core.event.on("core:tick", function(ev)
    clock += ev.dt
    if clock >= aged_at then
        aged_at = clock + 30
        core.storage_set("age", tostring(math.floor(age())))
    end
    if clock >= looked_at then
        looked_at = clock + 4
        look_for_homes()
        local gone = false
        for key, v in people do
            -- A bed broken is a home gone, and its inhabitant with it.
            if core.loaded(v.hx, v.hy, v.hz) == 1 and core.get_block(v.hx, v.hy, v.hz) ~= id("bed:bed") then
                if v.id ~= 0 then
                    core.mob_remove(v.id)
                end
                people[key] = nil
                gone = true
            elseif v.id == 0 and clock >= v.back_at and core.loaded(v.hx, v.hy, v.hz) == 1 and near_player(v, 64) then
                come_out(v)
            end
        end
        if gone then
            save_homes()
        end
        survey()
        muster()
        for rk in villages do
            if clock >= (beds_looked[rk] or 0) then
                beds_looked[rk] = clock + 20
                for mark in bed_keepers(rk) do
                    order_post(rk, mark)
                end
            end
        end
        for key, v in watch do
            if villages[v.rk :: string] == nil then
                if v.id ~= 0 then
                    core.mob_remove(v.id)
                end
                watch[key] = nil
            elseif v.id == 0 and clock >= v.back_at and core.loaded(v.hx, v.hy, v.hz) == 1 and near_player(v, 64) then
                come_out(v)
            end
        end
    end
    if clock < step_at then
        return
    end
    step_at = clock + 0.2
    notice_theft()
    hunted_gone()
    for _, v in people do
        step(v)
    end
    for _, v in watch do
        step(v)
    end
end)

-- What one of them is up to, for whoever asks: right click.
core.event.on("core:used_mob", function(ev)
    for _, v in people do
        if v.id == ev.target then
            local things = {}
            for item, n in v.inv do
                table.insert(things, `{n} {(string.gsub(string.gsub(item, "^.*:", ""), "_", " "))}`)
            end
            table.sort(things)
            local what = { farmer = "a farmer", fisher = "a fisher", forager = "a forager",
                           woodcutter = "a woodcutter", miner = "a miner", digger = "a digger" }
            local x, _, z = core.player_pos(ev.player)
            local rk = if x ~= nil and z ~= nil then village_at(x, z) else nil
            if rk ~= nil and trust_of(rk, core.player_name(ev.player)).strikes > 0 then
                core.notice(ev.player, `The {v.kind} turns away from you: the village has not forgotten.`)
                return
            end
            core.notice(ev.player, `A {v.kind}, {what[JOB[v.kind]]}` ..
                (if #things > 0 then `, carrying {table.concat(things, ", ")}` else ""))
            local home = village_at(v.hx, v.hz)
            if home ~= nil and TRADES[JOB[v.kind]] ~= nil then
                show_trade(ev.player, home, JOB[v.kind])
            end
            return
        end
    end
    for _, v in watch do
        if v.id == ev.target and (v.kind == "mayor" or v.kind == "builder" or v.kind == "merchant") then
            local x, _, z = core.player_pos(ev.player)
            local rk = if x ~= nil and z ~= nil then village_at(x, z) else nil
            if rk ~= nil and trust_of(rk, core.player_name(ev.player)).strikes > 0 then
                core.notice(ev.player, `The {v.kind} turns away from you: the village has not forgotten.`)
            elseif v.kind == "mayor" then
                show_mayor(ev.player, v.rk :: string)
            elseif v.kind == "merchant" then
                show_trade(ev.player, v.rk :: string, "merchant")
            else
                show_builder(ev.player, v.rk :: string)
            end
            return
        end
        if v.id == ev.target then
            local what = { guard = "A guard of the village", archer = "An archer of the village",
                           inspector = "The village's inspector, who looks in the bags of passers-by",
                           crafter = "The village's crafter, who makes the tools and torches its people use" }
            core.notice(ev.player, what[v.kind])
            return
        end
    end
end)
