--!strict
-- bees: kept bees. Part of the mod (init.luau requires it).
--
-- The SMOKER (iron, coal and wool): right click a nest, a hive or an apiary
-- with it and a cloud of smoke puffs out over it (smoke.luau), and its bees
-- are calm for a minute -- robbed, broken or caught from in that time they
-- do not sting, as with a torch under them.
--
-- The NET (a stick and string): right click a wild nest with it and you catch
-- its QUEEN; the colony goes with her, the nest falls to a honeycomb. An
-- apiary full of honey with a frame in it swarms into the net as well: a new
-- queen, and its honey gone with the swarm.
--
-- The APIARY (planks, wax and a frame) is a hive made to be kept: empty until
-- a queen is put in it (right click with one). Then it fills like a hive --
-- flowers round it, none in winter -- to six steps, and its bees never sting.
-- Right clicked with an upgrade it takes it in, and gives everything back when
-- broken:
--   frame     up to three: each a step more of honey every time it works
--   wrap      of wool round it: it works through the winter
--   infuser   gold and glass: its honey takes after what is round it, within
--             four cells -- glowcaps or glowmoss make GLOW honey, lava EMBER
--             honey, amethyst or the deep's crystal CRYSTAL honey
-- Full, a flask takes a bottle of its honey (special or plain), anything else
-- three honeycombs.
--
-- The PRESS (iron over planks) works what the bees make: right click it with
--   honeycomb       two of wax (a furnace melts one to one as well)
--   glow honey      two honey lamps, a block giving light
--   ember honey     three ember wax, a furnace fuel worth twenty smelts
--   crystal honey   two amethyst shards, and one time in four a diamond
-- the flask given back. Wax makes frames, wraps and apiaries.

local common = require("./common")

local APIARY_FULL_AT = 6
local INFUSE_REACH = 4
local CALM_FOR = 60.0   -- seconds a smoker's puff keeps bees calm

-- ---------------------------------------------------------------- things

local function item(name: string, stack: number)
    core.register_item(name, stack, "", 0, 1.0, "")
    core.register_model(`{name}_held`, `{string.match(name, ":(.+)$")}_held.gltf`)   -- tools/make_held_models.py
end
item("bees:smoker", 1)
item("bees:net", 1)
item("bees:queen", 16)
item("bees:wax", 64)
item("bees:frame", 16)
item("bees:wrap", 16)
item("bees:infuser", 16)
item("bees:ember_wax", 64)
item("bees:glow_honey", 16)
item("bees:ember_honey", 16)
item("bees:crystal_honey", 16)

local APIARY      = core.register_node("bees:apiary", 200, 165, 110, true, 0, "", 0, 0.8, "wood")
local APIARY_FULL = core.register_node("bees:apiary_full", 215, 170, 90, true, 0, "", 0, 0.8, "wood")
core.node_drop(APIARY_FULL, APIARY)
local PRESS = core.register_node("bees:press", 150, 120, 90, true, 0, "", 0, 1.5, "wood")
core.register_node("bees:honey_lamp", 250, 190, 90, true, 14, "", 0, 0.4, "glass")

common.TOOLS = { ["bees:smoker"] = true, ["bees:net"] = true }

-- Every recipe's things are this mod's or its depends', but the iron, gold,
-- wool, string and glass: those once every mod is in, and where they are
-- not (a world without the furnace or the sheep), the same thing made of
-- what this mod's depends always have -- a torch in a box of planks, straw
-- for wool and string, a flask for glass, logs for iron.
core.recipe_shaped("bees:frame", 2,
    "timber:stick timber:stick timber:stick",
    "timber:stick bees:wax     timber:stick",
    "timber:stick timber:stick timber:stick")
core.recipe_shaped("bees:apiary", 1,
    "timber:planks timber:planks timber:planks",
    "bees:wax      bees:frame    bees:wax",
    "timber:planks timber:planks timber:planks")
core.event.on("core:world_ready", function()
    local function have(...: string): boolean
        for _, n in { ... } do
            if core.item_id(n) == 0 then
                return false
            end
        end
        return true
    end
    -- The first of these whose things all exist.
    local function recipe(name: string, count: number, choices: { { string } })
        for _, rows in choices do
            local needs = {}
            for _, row in rows do
                for w in string.gmatch(row, "%S+") do
                    if w ~= "-" then
                        table.insert(needs, w)
                    end
                end
            end
            if have(table.unpack(needs)) then
                core.recipe_shaped(name, count, rows[1], rows[2], rows[3])
                return
            end
        end
    end
    recipe("bees:smoker", 1, {
        { "-                  furnace:iron_ingot -",
          "furnace:iron_ingot ore:coal           furnace:iron_ingot",
          "-                  animals:wool       -" },
        { "-             torches:torch -",
          "timber:planks timber:stick  timber:planks",
          "timber:planks timber:planks timber:planks" } })
    recipe("bees:net", 1, {
        { "-            archery:string archery:string",
          "-            archery:string archery:string",
          "timber:stick -              -" },
        { "-            farm:wheat farm:wheat",
          "-            farm:wheat farm:wheat",
          "timber:stick -          -" } })
    recipe("bees:wrap", 1, {
        { "animals:wool animals:wool animals:wool",
          "animals:wool bees:wax     animals:wool",
          "animals:wool animals:wool animals:wool" },
        { "farm:wheat farm:wheat farm:wheat",
          "farm:wheat bees:wax   farm:wheat",
          "farm:wheat farm:wheat farm:wheat" } })
    recipe("bees:infuser", 1, {
        { "-          furnace:gold_ingot -",
          "core:glass bees:honeycomb     core:glass",
          "-          furnace:gold_ingot -" },
        { "-        alchemy:flask  -",
          "bees:wax bees:honeycomb bees:wax",
          "-        alchemy:flask  -" } })
    recipe("bees:press", 1, {
        { "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot",
          "timber:planks      timber:stick       timber:planks",
          "timber:planks      timber:planks      timber:planks" },
        { "timber:log    timber:log   timber:log",
          "timber:planks timber:stick timber:planks",
          "timber:planks timber:planks timber:planks" } })
end)

-- ---------------------------------------------------------------- calm

local clock = 0.0
local calm_until: { [string]: number } = {}
core.event.on("core:tick", function(ev)
    clock += ev.dt
end)

-- Smoke seen by everybody near: a puff at the cell's middle, drawn on each
-- player's machine within 48 (smoke.luau).
local function smoke(x: number, y: number, z: number)
    calm_until[`{x},{y},{z}`] = clock + CALM_FOR
    for _, p in core.players() do
        local px, py, pz = core.player_pos(p)
        if px and (px - x) ^ 2 + (py - y) ^ 2 + (pz - z) ^ 2 < 48 * 48 then
            if core.side() ~= "server" and common.puff then
                common.puff(x + 0.5, y + 0.6, z + 0.5)
            else
                core.net.send(p, `smoke {x + 0.5} {y + 0.6} {z + 0.5}`)
            end
        end
    end
    core.play_sound("bees:puff", x + 0.5, y + 0.5, z + 0.5)
end

function common.calmed(x: number, y: number, z: number): boolean
    local t = calm_until[`{x},{y},{z}`]
    return t ~= nil and t > clock
end

-- ---------------------------------------------------------------- the apiary

local function meta_n(x: number, y: number, z: number, key: string): number
    return tonumber(core.meta_get(x, y, z, key)) or 0
end

local function is_apiary(id: number): boolean
    return id == APIARY or id == APIARY_FULL
end

-- A cell that has bees in it besides the wild ones: an apiary with its queen.
function common.colony(x: number, y: number, z: number, id: number): boolean
    return is_apiary(id) and meta_n(x, y, z, "queen") > 0
end
function common.full(id: number): boolean
    return id == APIARY_FULL
end

-- What its honey would be: the first of these found round it, with an
-- infuser in it. Each kind's bottle, and the blocks that make it.
local KINDS: { { string | { string } } } = {
    { "bees:glow_honey", { "deep:glowcap", "deep:glowmoss" } },
    { "bees:ember_honey", { "rocks:lava", "inferno:fire" } },
    { "bees:crystal_honey", { "rocks:amethyst_block", "rocks:amethyst_cluster", "deep:crystal" } },
}
local KIND_SAID = { ["bees:glow_honey"] = "glow honey", ["bees:ember_honey"] = "ember honey",
                    ["bees:crystal_honey"] = "crystal honey", ["bees:honey_bottle"] = "plain honey" }

local function honey_kind(x: number, y: number, z: number): string
    if meta_n(x, y, z, "infuser") == 0 then
        return "bees:honey_bottle"
    end
    local r = INFUSE_REACH
    for _, k in KINDS do
        for _, name in k[2] :: { string } do
            if core.node_id(name) ~= 0
                and #core.find_nodes(x - r, y - r, z - r, x + r, y + r, z + r, name) > 0 then
                return k[1] :: string
            end
        end
    end
    return "bees:honey_bottle"
end

local function status(p: number, x: number, y: number, z: number)
    if meta_n(x, y, z, "queen") == 0 then
        core.notice(p, "An empty apiary: put a queen in it (catch one from a wild nest with a net)")
        return
    end
    local parts = { `honey {meta_n(x, y, z, "honey")} of {APIARY_FULL_AT}`,
                    `frames {meta_n(x, y, z, "frames")} of 3` }
    if meta_n(x, y, z, "wrap") > 0 then
        table.insert(parts, "wrapped for winter")
    end
    if meta_n(x, y, z, "infuser") > 0 then
        table.insert(parts, `infuser: {KIND_SAID[honey_kind(x, y, z)]}`)
    end
    if common.season() == "winter" and meta_n(x, y, z, "wrap") == 0 then
        table.insert(parts, "asleep for the winter")
    elseif common.flowers_near(x, y, z) == 0 then
        table.insert(parts, "no flowers round it")
    end
    core.notice(p, table.concat(parts, " · "))
end

local UPGRADES: { [string]: { string | number } } = {
    ["bees:frame"] = { "frames", 3 }, ["bees:wrap"] = { "wrap", 1 }, ["bees:infuser"] = { "infuser", 1 },
}

local function empty_it(x: number, y: number, z: number)
    core.set_block(x, y, z, APIARY)
    core.meta_set(x, y, z, "honey", "0")
end

local function use_apiary(ev)
    local p, x, y, z, held = ev.player, ev.x, ev.y, ev.z, ev.held
    local full = ev.id == APIARY_FULL
    local queen = meta_n(x, y, z, "queen") > 0
    local up = UPGRADES[held]
    if held == "bees:queen" then
        if queen then
            core.notice(p, "This apiary has its queen")
        elseif core.take_item(p, held, 1) > 0 then
            core.meta_set(x, y, z, "queen", "1")
            core.notice(p, "The queen is in: with flowers round it, the apiary fills")
        end
    elseif up then
        local key, most = up[1] :: string, up[2] :: number
        if meta_n(x, y, z, key) >= most then
            core.notice(p, "It has as many of those as it takes")
        elseif core.take_item(p, held, 1) > 0 then
            core.meta_set(x, y, z, key, tostring(meta_n(x, y, z, key) + 1))
            status(p, x, y, z)
        end
    elseif held == "bees:smoker" then
        smoke(x, y, z)
        core.notice(p, "The bees are calm")
    elseif not full then
        status(p, x, y, z)
    elseif held == "bees:net" then
        if meta_n(x, y, z, "frames") == 0 then
            core.notice(p, "It swarms only with a frame in it")
            return
        end
        empty_it(x, y, z)
        core.give_item(p, "bees:queen", 1)
        core.notice(p, "A swarm, and a new queen in the net")
    elseif held == "alchemy:flask" then
        if core.take_item(p, held, 1) == 0 then
            return
        end
        local kind = honey_kind(x, y, z)
        empty_it(x, y, z)
        core.give_item(p, kind, 1)
        if kind ~= "bees:honey_bottle" then
            core.notice(p, `A bottle of {KIND_SAID[kind]}: work it in a press`)
        end
    else
        empty_it(x, y, z)
        core.drop_item(x, y + 1, z, core.item_id("bees:honeycomb"), 3)
    end
end

core.register_abm("bees:apiary", { "bees:apiary" }, 20.0, 1, function(cell)
    local x, y, z = cell.x, cell.y, cell.z
    if meta_n(x, y, z, "queen") == 0 then
        return
    end
    if common.season() == "winter" and meta_n(x, y, z, "wrap") == 0 then
        return
    end
    local flowers = common.flowers_near(x, y, z)
    if flowers == 0 then
        return
    end
    if common.players_near(x, y, z, 12) then
        core.play_sound("bees:buzz", x + 0.5, y + 0.5, z + 0.5)
    end
    if math.random(2) == 1 then
        common.pollinate(x, y, z)
    end
    if math.random() > math.min(1, flowers / 6) then
        return
    end
    local honey = meta_n(x, y, z, "honey") + 1 + meta_n(x, y, z, "frames")
    if honey >= APIARY_FULL_AT then
        core.set_block(x, y, z, APIARY_FULL)
        honey = APIARY_FULL_AT
    end
    core.meta_set(x, y, z, "honey", tostring(honey))
end)

-- ---------------------------------------------------------------- the press

local PRESSED: { [string]: { { string | number } } } = {
    ["bees:honeycomb"] = { { "bees:wax", 2 } },
    ["bees:glow_honey"] = { { "bees:honey_lamp", 2 } },
    ["bees:ember_honey"] = { { "bees:ember_wax", 3 } },
    ["bees:crystal_honey"] = { { "rocks:amethyst_shard", 2 } },
}

local function use_press(ev)
    local p, held = ev.player, ev.held
    local out = PRESSED[held]
    if out == nil then
        core.notice(p, if held == "bees:honey_bottle" then "Plain honey is for eating"
                       else "The press works honeycomb and special honey")
        return
    end
    if core.take_item(p, held, 1) == 0 then
        return
    end
    for _, o in out do
        core.give_item(p, o[1] :: string, o[2] :: number)
    end
    if held == "bees:crystal_honey" and math.random(4) == 1 then
        core.give_item(p, "ore:diamond", 1)
        core.notice(p, "A diamond in the crystal honey!")
    end
    if held ~= "bees:honeycomb" then
        core.give_item(p, "alchemy:flask", 1)
    end
    core.play_sound("bees:press", ev.x + 0.5, ev.y + 0.5, ev.z + 0.5)
end

-- ---------------------------------------------------------------- right clicks

core.event.on("core:used", function(ev)
    local p, x, y, z, held = ev.player, ev.x, ev.y, ev.z, ev.held
    if is_apiary(ev.id) then
        ev.cancel = true
        use_apiary(ev)
        return
    end
    if ev.id == PRESS then
        ev.cancel = true
        use_press(ev)
        return
    end
    -- The smoker and the net on a wild nest or a hive of planks.
    local name = core.node_name(ev.id)
    local wild = name == "bees:nest" or name == "bees:nest_full"
    if not (wild or name == "bees:hive" or name == "bees:hive_full") then
        return
    end
    if held == "bees:smoker" then
        ev.cancel = true
        smoke(x, y, z)
        core.notice(p, "The bees are calm, for a minute")
    elseif held == "bees:net" then
        ev.cancel = true
        if not wild then
            core.notice(p, "Their queen is deep in the hive: a net takes one from a wild nest")
            return
        end
        if not common.smoked(x, y, z) then
            common.sting(p, x, y, z)
        end
        core.set_block(x, y, z, 0)
        core.meta_set(x, y, z, "honey", "")
        core.drop_item(x, y, z, core.item_id("bees:honeycomb"), 1)
        core.give_item(p, "bees:queen", 1)
        core.notice(p, "The queen is in the net: put her in an apiary")
    end
end)

-- Broken, an apiary gives back its queen and its upgrades.
core.event.on("core:broken", function(ev)
    if not is_apiary(ev.id) then
        return
    end
    local x, y, z = ev.x, ev.y, ev.z
    for _, give in { { "queen", "bees:queen" }, { "frames", "bees:frame" }, { "wrap", "bees:wrap" },
                     { "infuser", "bees:infuser" } } do
        local n = meta_n(x, y, z, give[1])
        if n > 0 then
            core.drop_item(x, y, z, core.item_id(give[2]), n)
        end
        core.meta_set(x, y, z, give[1], "")
    end
    core.meta_set(x, y, z, "honey", "")
end)
