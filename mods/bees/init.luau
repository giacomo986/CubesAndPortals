--!strict
-- bees: wild nests and hives, honey and honeycomb.
--
-- A NEST hangs among the flowers of a meadow here and there (a mapgen stage,
-- after mods/plants has put the flowers); a HIVE is made of planks round
-- honeycomb. Either fills with honey by itself, four steps from empty to
-- full, a step now and then for each flower within six cells of it -- none,
-- no honey -- and not at all in winter (farm:season, told by mods/weather).
-- A full one shows its honey and buzzes louder.
--
-- Right click a full one with an empty flask (mods/alchemy) for a bottle of
-- honey, with anything else for two honeycombs; either way it starts again.
-- Robbed without a torch burning under it within three cells -- the smoke --
-- the bees sting: two points. Breaking a nest stings the same, and leaves
-- one honeycomb; a hive is taken whole.
--
-- A bottle of honey is food (mods/hunger: 6 and 1.2, the flask back);
-- honeycomb brews vigour at a still (mods/alchemy) and three of it between
-- planks make a hive. And the bees go round the fields: now and then a crop
-- within six cells of a working nest or hive grows a stage.
--
-- By day, out of winter, bees fly round a working nest or hive while a
-- player is near (below).
--
-- Kept bees -- the smoker, the net, a queen, the apiary and its upgrades,
-- special honeys and the press -- are apiary.luau's.
--
-- Pictures and sounds: tools/make_bees_assets.py; the bee
-- tools/make_bee_model.py; the held models tools/make_held_models.py.

local common = require("./common")

-- The smoker's smoke is drawn where there is drawing (smoke.luau); all the
-- rest is the server's.
local side = core.side()
if side ~= "server" then
    require("./smoke")
end
if side == "client" then
    return
end
core.net.open()

local FULL_AT = 4    -- honey steps from empty to full
local REACH = 6      -- flowers and crops this far round count
local SMOKE = 3      -- a torch this far under it calms the bees

local function node(name: string, r: number, g: number, b: number): number
    return core.register_node(name, r, g, b, true, 0, "", 0, 0.8, "wood")
end
local NEST      = node("bees:nest", 205, 160, 70)
local NEST_FULL = node("bees:nest_full", 225, 175, 60)
local HIVE      = node("bees:hive", 190, 150, 95)
local HIVE_FULL = node("bees:hive_full", 205, 160, 80)
core.node_drop(HIVE_FULL, HIVE)
core.node_drop_item(NEST, "bees:honeycomb")
core.node_drop_item(NEST_FULL, "bees:honeycomb")

core.register_item("bees:honeycomb", 64, "", 0, 1.0, "")
core.register_item("bees:honey_bottle", 16, "", 0, 1.0, "")
core.register_model("bees:honeycomb_held", "honeycomb_held.gltf")
core.register_model("bees:honey_bottle_held", "honey_bottle_held.gltf")

core.recipe_shaped("bees:hive", 1,
    "timber:planks  timber:planks  timber:planks",
    "bees:honeycomb bees:honeycomb bees:honeycomb",
    "timber:planks  timber:planks  timber:planks")

-- Empty and full of each, both ways.
local EMPTY_OF = { [NEST] = NEST, [NEST_FULL] = NEST, [HIVE] = HIVE, [HIVE_FULL] = HIVE }
local FULL_OF = { [NEST] = NEST_FULL, [HIVE] = HIVE_FULL }
local IS_FULL = { [NEST_FULL] = true, [HIVE_FULL] = true }

local FLOWERS = { "dandelion", "poppy", "blue_orchid", "allium", "azure_bluet", "red_tulip", "orange_tulip",
                  "white_tulip", "pink_tulip", "oxeye_daisy", "cornflower", "lily_of_the_valley" }

local season = "spring"
core.event.on("farm:season", function(ev)
    season = ev.name
end)

local function honey_of(x: number, y: number, z: number): number
    return tonumber(core.meta_get(x, y, z, "honey")) or 0
end

local function flowers_near(x: number, y: number, z: number): number
    local n = 0
    for _, f in FLOWERS do
        n += #core.find_nodes(x - REACH, y - 3, z - REACH, x + REACH, y + 3, z + REACH, `plants:{f}`) // 3
    end
    return n
end

local function players_near(x: number, y: number, z: number, r: number): boolean
    for _, p in core.players() do
        local px, py, pz = core.player_pos(p)
        if px and (px - x) ^ 2 + (py - y) ^ 2 + (pz - z) ^ 2 < r * r then
            return true
        end
    end
    return false
end

-- A crop round it a stage on: wheat, and the melon and pumpkin stems (the
-- fruit a grown stem puts down stays mods/farm's to do).
local CROPS = { "farm:wheat_*", "farm:melon_stem_*", "farm:pumpkin_stem_*" }
local function pollinate(x: number, y: number, z: number)
    local found: { { number } } = {}
    for _, name in CROPS do
        local f = core.find_nodes(x - REACH, y - 3, z - REACH, x + REACH, y + 3, z + REACH, name)
        for i = 1, #f, 3 do
            table.insert(found, { f[i], f[i + 1], f[i + 2] })
        end
    end
    if #found == 0 then
        return
    end
    local c = found[math.random(#found)]
    local name = core.node_name(core.get_block(c[1], c[2], c[3]))
    local base, digit = string.match(name, "^(.-_)(%d)$")
    local stage = tonumber(digit) or 3
    if base and stage < 3 then
        local next_id = core.node_id(`{base}{stage + 1}`)
        if next_id ~= 0 then
            core.set_block(c[1], c[2], c[3], next_id)
        end
    end
end

core.register_abm("bees:work", { "bees:nest", "bees:hive", "bees:nest_full", "bees:hive_full" }, 30.0, 1,
    function(cell)
        local x, y, z = cell.x, cell.y, cell.z
        if season == "winter" then
            return
        end
        local id = core.get_block(x, y, z)
        local flowers = flowers_near(x, y, z)
        if flowers == 0 then
            return
        end
        if players_near(x, y, z, 12) then
            core.play_sound("bees:buzz", x + 0.5, y + 0.5, z + 0.5)
        end
        if math.random(2) == 1 then
            pollinate(x, y, z)
        end
        if IS_FULL[id] or math.random() > math.min(1, flowers / 6) then
            return
        end
        local honey = honey_of(x, y, z) + 1
        core.meta_set(x, y, z, "honey", tostring(honey))
        if honey >= FULL_AT then
            core.set_block(x, y, z, FULL_OF[id])
            core.meta_set(x, y, z, "honey", tostring(FULL_AT))
        end
    end)

-- A torch under it, within three cells and one to either side -- or a
-- smoker's puff in the last minute (apiary.luau, common.calmed).
local function smoked(x: number, y: number, z: number): boolean
    return common.calmed(x, y, z) or #core.find_nodes(x - 1, y - SMOKE, z - 1, x + 1, y - 1, z + 1, "torches:*") > 0
end

local function sting(player: number, x: number, y: number, z: number)
    core.play_sound("bees:sting", x + 0.5, y + 0.5, z + 0.5)
    core.damage(player, 2)
    core.notice(player, "The bees sting! A smoker, or a torch under them, would calm them")
end

core.event.on("core:used", function(ev)
    local empty = EMPTY_OF[ev.id]
    if empty == nil or common.TOOLS[ev.held] then   -- the smoker and the net are apiary.luau's
        return
    end
    ev.cancel = true
    if not IS_FULL[ev.id] then
        local honey = honey_of(ev.x, ev.y, ev.z)
        core.notice(ev.player, if season == "winter" then "The bees sleep through the winter"
                               else `Honey {honey} of {FULL_AT}: the more flowers round it, the sooner`)
        return
    end
    if ev.held == "alchemy:flask" then
        if core.take_item(ev.player, "alchemy:flask", 1) == 0 then
            return
        end
        core.give_item(ev.player, "bees:honey_bottle", 1)
    else
        core.drop_item(ev.x, ev.y + 1, ev.z, core.item_id("bees:honeycomb"), 2)
    end
    core.set_block(ev.x, ev.y, ev.z, empty)
    core.meta_set(ev.x, ev.y, ev.z, "honey", "0")
    if not smoked(ev.x, ev.y, ev.z) then
        sting(ev.player, ev.x, ev.y, ev.z)
    end
end)

core.event.on("core:broken", function(ev)
    if (ev.id == NEST or ev.id == NEST_FULL) and ev.player > 0 and not smoked(ev.x, ev.y, ev.z) then
        sting(ev.player, ev.x, ev.y, ev.z)
    end
    if EMPTY_OF[ev.id] then
        core.meta_set(ev.x, ev.y, ev.z, "honey", "")
    end
end)

-- ---------------------------------------------------------------- the bees seen

-- By day, out of winter, a nest or hive with flowers round it and a player
-- within 20 has bees round it: three, four when it is full, each going round
-- it on a loop of its own -- out to the flowers' distance and back, up and
-- down -- facing the way it flies. They are entities everybody sees
-- (core.ecs.visible) wearing tools/make_bee_model.py's bee, its wings
-- beating; at most 40 in the world, made and dropped as players come and go.
local BEE_MODEL = core.register_model("bees:bee", "bee.gltf")
core.ecs.component("bees:bee", { "t", "r", "w", "p" })   -- age, loop size, speed, where on it
local MAX_BEES = 40
local SEEN = 20          -- a player this near brings a hive's bees out
local LOOK_EVERY = 1.0   -- seconds between looks for hives near players
local FLOWERS_FOR = 30.0 -- seconds a hive's count of flowers is kept

type Swarm = { x: number, y: number, z: number, bees: { number } }
local swarms: { [string]: Swarm } = {}
local flowers_seen: { [string]: { n: number, at: number } } = {}
local flying = 0
local clock, looked = 0.0, 0.0

local function out_by_day(): boolean
    local t = core.time_of_day()
    return season ~= "winter" and t > 0.24 and t < 0.76
end

local function land(key: string)
    local s = swarms[key]
    if s == nil then
        return
    end
    for _, b in s.bees do
        core.ecs.kill(b)
    end
    flying -= #s.bees
    swarms[key] = nil
end

local function take_off(key: string, x: number, y: number, z: number, n: number)
    local s: Swarm = { x = x + 0.5, y = y, z = z + 0.5, bees = {} }
    for _ = 1, n do
        if flying >= MAX_BEES then
            break
        end
        local b = core.ecs.spawn("bees:bee", s.x, s.y + 1, s.z)
        if b then
            core.ecs.set(b, "t", math.random() * 20)
            core.ecs.set(b, "r", 1.2 + math.random() * 2.3)
            core.ecs.set(b, "w", (0.6 + math.random() * 0.6) * (if math.random(2) == 1 then 1 else -1))
            core.ecs.set(b, "p", math.random() * 6.283)
            core.ecs.visible(b, true)
            core.ecs.model(b, BEE_MODEL)
            table.insert(s.bees, b)
            flying += 1
        end
    end
    swarms[key] = s
end

local function has_flowers(key: string, x: number, y: number, z: number): boolean
    local f = flowers_seen[key]
    if f == nil or clock - f.at > FLOWERS_FOR then
        f = { n = flowers_near(x, y, z), at = clock }
        flowers_seen[key] = f
    end
    return f.n > 0
end

-- Which hives should have bees out now, and making it so.
local function look()
    local want: { [string]: { number } } = {}
    if out_by_day() then
        for _, p in core.players() do
            local px, py, pz = core.player_pos(p)
            if px then
                px, py, pz = math.floor(px), math.floor(py), math.floor(pz)
                local f = core.find_nodes(px - SEEN, py - 10, pz - SEEN, px + SEEN, py + 10, pz + SEEN, "bees:*")
                for i = 1, #f, 3 do
                    local id = core.get_block(f[i], f[i + 1], f[i + 2])
                    if EMPTY_OF[id] or common.colony(f[i], f[i + 1], f[i + 2], id) then
                        want[`{f[i]},{f[i + 1]},{f[i + 2]}`] = { f[i], f[i + 1], f[i + 2] }
                    end
                end
            end
        end
    end
    for key in swarms do
        if want[key] == nil then
            land(key)
        end
    end
    for key, c in want do
        if swarms[key] == nil and has_flowers(key, c[1], c[2], c[3]) then
            local id = core.get_block(c[1], c[2], c[3])
            take_off(key, c[1], c[2], c[3], if IS_FULL[id] or common.full(id) then 4 else 3)
        end
    end
end

core.event.on("core:tick", function(ev)
    clock += ev.dt
    if clock - looked >= LOOK_EVERY then
        looked = clock
        look()
    end
    for _, s in swarms do
        for _, b in s.bees do
            local t = (core.ecs.get(b, "t") or 0) + ev.dt
            core.ecs.set(b, "t", t)
            local r, w, p = core.ecs.get(b, "r") or 2, core.ecs.get(b, "w") or 1, core.ecs.get(b, "p") or 0
            -- Round the hive, the loop breathing in and out, bobbing.
            local a = p + t * w
            local reach = r * (0.65 + 0.35 * math.sin(t * 0.7 + p))
            local x = s.x + math.cos(a) * reach
            local z = s.z + math.sin(a) * reach
            local y = s.y + 0.7 + 0.5 * math.sin(t * 1.9 + p) + 0.15 * math.sin(t * 7.3)
            -- Facing along the loop: the way it turns, w's sign.
            local dx, dz = -math.sin(a) * w, math.cos(a) * w
            core.ecs.set(b, "x", x)
            core.ecs.set(b, "y", y)
            core.ecs.set(b, "z", z)
            core.ecs.set(b, "yaw", math.atan2(dx, -dz))
        end
    end
end)

-- What apiary.luau works with.
common.NEST, common.NEST_FULL, common.FULL_AT = NEST, NEST_FULL, FULL_AT
common.season = function(): string
    return season
end
common.flowers_near = flowers_near
common.players_near = players_near
common.pollinate = pollinate
common.smoked = smoked
common.sting = sting
require("./apiary")

-- ---------------------------------------------------------------- wild nests

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

-- One flower in sixty becomes a nest, in the meadows mods/plants has made.
core.register_mapgen("bees:nests", function(ctx)
    local FLOWER: { [number]: boolean } = {}
    for _, f in FLOWERS do
        FLOWER[core.node_id(`plants:{f}`)] = true
    end
    for z = ctx.min_z, ctx.max_z do
        for x = ctx.min_x, ctx.max_x do
            local top = core.terrain_height(x, z)
            if FLOWER[core.get_block(x, top, z)] and hash(x, z, ctx.seed + 0xB335) % 60 == 0 then
                core.set_block(x, top, z, NEST)
            end
        end
    end
end)

core.log("bees: nests in the meadows, hives of planks and honeycomb, honey for flasks")
