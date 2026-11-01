--!strict
-- farm: The classic block game's wheat, from the grass to the bread.
--
-- A hoe, right clicked on grass or dirt with air over it, tills it into soil.
-- Seeds (mods/animals, one grass block in eight leaves one) right clicked on
-- soil are sown on it. Wheat grows through four stages on its own -- about a
-- minute and a half a stage -- and broken grown gives a sheaf and one to three
-- seeds; broken young, the one seed back. Three wheat in a row are bread. A cow
-- and a sheep are bred with wheat (mods/animals).
--
-- Soil with water four cells from it grows wheat twice as fast as soil with
-- none. As in the classic block game, dry soil with nothing sown on it turns back to dirt,
-- wheat grows only in light 9 or more (core.light: a torch, or the sky by
-- day), and a player who jumps or falls onto soil tramples it back to dirt,
-- crop and all. A crop is not solid, so it is walked through, and has a box
-- its height, which is what it is aimed at by. Its pictures and models are
-- tools/make_farm_assets.py.
--
-- Melons and pumpkins too: a melon slice is a melon seed, a pumpkin four
-- pumpkin seeds, sown on soil as wheat is. The stem grows through four stages
-- by wheat's rules, and grown it puts a fruit (mods/plants' block) on a free
-- cell beside it over dirt, grass or soil -- one at a time: a stem with its
-- fruit beside it waits for that to be picked. Broken, a stem gives its seed
-- back.
local SOIL = core.register_node("farm:soil", 92, 62, 40, true, 0, "", 0, 0.6, "soil")
core.node_drop(SOIL, core.node_id("core:dirt"))

-- As tall as tools/make_farm_assets.py draws each stage.
local HEIGHTS = { 5 / 16, 8 / 16, 11 / 16, 15 / 16 }
local STAGES = {}
for n = 0, 3 do
    local id = core.register_node(`farm:wheat_{n}`, 180, 170, 60, false, 0, "", 0, 0.0, "")
    core.node_model(id, core.register_model(`farm:wheat_{n}`, `wheat_{n}.gltf`))
    core.node_box(id, 0.1, 0, 0.1, 0.9, HEIGHTS[n + 1], 0.9)
    core.node_drop_item(id, if n == 3 then "farm:wheat" else "animals:seeds")
    STAGES[n + 1] = id
end
local RIPE = STAGES[4]

-- What each growing thing gives when it goes (broken, its soil gone, popped):
-- by id, the item.
local DROPS: { [number]: string } = {}
for n, id in STAGES do
    DROPS[id] = if n == 4 then "farm:wheat" else "animals:seeds"
end

-- The stems: kind -> its four stages, its seed and the fruit it gives.
type Stem = { stages: { number }, seed: string, fruit: number }
local STEMS: { [string]: Stem } = {}
local STEM_OF: { [number]: Stem } = {}   -- each stage's id -> its stem
local STEM_HEIGHTS = { 4 / 16, 7 / 16, 10 / 16, 13 / 16 }
for _, kind in { "melon", "pumpkin" } do
    local stem: Stem = { stages = {}, seed = `farm:{kind}_seeds`, fruit = core.node_id(`plants:{kind}`) }
    core.register_item(stem.seed, 64, "", 0, 1.0, "")
    for n = 0, 3 do
        local id = core.register_node(`farm:{kind}_stem_{n}`, 90, 160, 50, false, 0, "", 0, 0.0, "")
        core.node_model(id, core.register_model(`farm:{kind}_stem_{n}`, `{kind}_stem_{n}.gltf`))
        core.node_box(id, 0.2, 0, 0.2, 0.8, STEM_HEIGHTS[n + 1], 0.8)
        core.node_drop_item(id, stem.seed)
        stem.stages[n + 1] = id
        STEM_OF[id] = stem
        DROPS[id] = stem.seed
    end
    STEMS[kind] = stem
end
core.recipe_shapeless("farm:melon_seeds", 1, "plants:melon_slice")
core.recipe_shapeless("farm:pumpkin_seeds", 4, "plants:pumpkin")

core.register_item("farm:hoe", 1, "", 0, 1.0, "", 0, "", 0, 59)   -- the wooden one; tools and furnace make the rest
core.register_item("farm:wheat", 64, "", 0, 1.0, "")
core.register_item("farm:bread", 64, "", 0, 1.0, "")

core.recipe_shaped("farm:hoe", 1,
    "timber:planks  timber:planks",
    "-              timber:stick",
    "-              timber:stick")
core.recipe_shaped("farm:bread", 1, "farm:wheat farm:wheat farm:wheat")

local TILLS = { [core.node_id("core:grass")] = true, [core.node_id("core:dirt")] = true }

core.event.on("core:used", function(ev)
    if core.get_block(ev.x, ev.y + 1, ev.z) ~= 0 then
        return
    end
    if (ev.held == "farm:hoe" or string.match(ev.held, "_hoe$") ~= nil) and TILLS[ev.id] then
        core.set_block(ev.x, ev.y, ev.z, SOIL)
        core.wear_held(ev.player, 1)
        ev.cancel = true
    elseif (ev.held == "animals:seeds" or ev.held == "farm:melon_seeds" or ev.held == "farm:pumpkin_seeds")
        and ev.id == SOIL then
        if core.take_item(ev.player, ev.held, 1) > 0 then
            local first = if ev.held == "animals:seeds" then STAGES[1]
                else STEMS[if ev.held == "farm:melon_seeds" then "melon" else "pumpkin"].stages[1]
            core.set_block(ev.x, ev.y + 1, ev.z, first)
        end
        ev.cancel = true
    elseif ev.held == "farm:bread" then
        ev.cancel = true
        if core.health(ev.player) >= 20 then
            core.notice(ev.player, "Not hungry")
        elseif core.take_item(ev.player, ev.held, 1) > 0 then
            core.heal(ev.player, 5)
        end
    end
end)

-- Rain, told by mods/weather (which may not be in this world): soil under the
-- open sky is watered while it lasts, outside the desert where none falls.
core.event.define("farm:rain", { fields = { on = "boolean" }, cancellable = false })
local raining = false
core.event.on("farm:rain", function(ev)
    raining = ev.on
end)

-- The season, told by mods/weather too: in autumn a crop grows half as often,
-- in winter only under a lamp (block light 9 or more: a greenhouse). Told
-- again every midnight, with the whole days left of it (0 on its last).
core.event.define("farm:season", { fields = { name = "string", left = "number" }, cancellable = false })
local season = "spring"
core.event.on("farm:season", function(ev)
    season = ev.name
end)

local function rained_on(x: number, y: number, z: number): boolean
    if not raining or core.biome(x, z) == "core:desert" then
        return false
    end
    for dy = 2, 24 do
        if core.get_block(x, y + dy, z) ~= 0 then
            return false
        end
    end
    return true
end

-- Water within four cells of the soil, on its level, as the classic block game's farmland
-- wants it; or rain.
local function watered(x: number, y: number, z: number): boolean
    return #core.find_nodes(x - 4, y, z - 4, x + 4, y, z + 4, "lakes:water") > 0 or rained_on(x, y, z)
end

-- Every 30 s one crop in three goes up a stage: 90 s a stage on average with
-- water near, twice that without. A crop whose soil has gone some way other
-- than being dug (a mod, a flood) pops off as what it would have dropped.
local SIDES = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }
local BEARS = { [core.node_id("core:dirt")] = true, [core.node_id("core:grass")] = true, [SOIL] = true }

-- A grown stem's fruit, on a free cell beside it, unless one is there already.
local function bear(x: number, y: number, z: number, stem: Stem)
    local free = {}
    for _, s in SIDES do
        local fx, fz = x + s[1], z + s[2]
        local here = core.get_block(fx, y, fz)
        if here == stem.fruit then return end
        if here == 0 and BEARS[core.get_block(fx, y - 1, fz)] then table.insert(free, { fx, fz }) end
    end
    if #free == 0 then return end
    local at = free[math.random(#free)]
    core.set_block(at[1], y, at[2], stem.fruit)
    if core.node_turns(stem.fruit) > 1 then core.set_facing(at[1], y, at[2], math.random(0, 3)) end
end

local CROPS = { "farm:wheat_0", "farm:wheat_1", "farm:wheat_2", "farm:wheat_3" }
for _, kind in { "melon", "pumpkin" } do
    for n = 0, 3 do table.insert(CROPS, `farm:{kind}_stem_{n}`) end
end
core.register_abm("farm:grow", CROPS,
    30.0, 3, function(cell)
        local id = core.get_block(cell.x, cell.y, cell.z)
        if core.get_block(cell.x, cell.y - 1, cell.z) ~= SOIL then
            core.set_block(cell.x, cell.y, cell.z, 0)
            core.drop_item(cell.x, cell.y, cell.z, core.item_id(DROPS[id] or "animals:seeds"), 1)
            return
        end
        local stem = STEM_OF[id]
        local grown = id == RIPE or (stem ~= nil and id == stem.stages[4])
        if core.light(cell.x, cell.y, cell.z) < 9
            or (season == "winter" and core.block_light(cell.x, cell.y, cell.z) < 9)
            or (season == "autumn" and math.random(2) == 1)
            or (not watered(cell.x, cell.y - 1, cell.z) and math.random(2) == 1) then
            return
        end
        if grown then
            if stem then bear(cell.x, cell.y, cell.z, stem) end
            return
        end
        local stages = if stem then stem.stages else STAGES
        for i = 1, 3 do
            if stages[i] == id then
                core.set_block(cell.x, cell.y, cell.z, stages[i + 1])
                return
            end
        end
    end)

-- Soil back to dirt, taking what grows on it along as what it would drop.
local function undo_soil(x: number, y: number, z: number)
    local above = core.get_block(x, y + 1, z)
    if DROPS[above] then
        core.set_block(x, y + 1, z, 0)
        core.drop_item(x, y + 1, z, core.item_id(DROPS[above]), 1)
    end
    core.set_block(x, y, z, core.node_id("core:dirt"))
end

-- Dry and bare, one soil in four goes back to dirt every 30 s: about two
-- minutes left alone. Sown or watered, it stays.
core.register_abm("farm:dry", { "farm:soil" }, 30.0, 4, function(cell)
    if core.get_block(cell.x, cell.y + 1, cell.z) == 0 and not watered(cell.x, cell.y, cell.z) then
        undo_soil(cell.x, cell.y, cell.z)
    end
end)

-- Trampling. The highest a player's feet were since they last stood on
-- something, and when they stand again on soil more than 0.75 below that --
-- a jump reaches 1.28 -- the soil is dirt. Walking on it does not do it.
local peak: { [number]: number } = {}
core.event.on("core:tick", function(_)
    local here: { [number]: number } = {}
    for _, p in core.players() do
        local x, y, z = core.player_pos(p)
        if x and y and z then
            local top = peak[p] or y
            local bx, by, bz = math.floor(x), math.floor(y + 0.01) - 1, math.floor(z)
            local standing = y - (by + 1) < 0.01 and core.node_solid(core.get_block(bx, by, bz)) == 1
            if standing then
                if top - y > 0.75 and core.get_block(bx, by, bz) == SOIL then
                    undo_soil(bx, by, bz)
                end
                top = y
            end
            here[p] = math.max(top, y)
        end
    end
    peak = here
end)

-- Grown wheat gives its sheaf (node_drop_item) and one to three seeds too. Soil
-- dug from under a crop takes the crop with it, as what it would have dropped.
core.event.on("core:broken", function(ev)
    if ev.id == SOIL then
        local above = core.get_block(ev.x, ev.y + 1, ev.z)
        if DROPS[above] then
            core.set_block(ev.x, ev.y + 1, ev.z, 0)
            core.drop_item(ev.x, ev.y + 1, ev.z, core.item_id(DROPS[above]), 1)
        end
    elseif ev.id == RIPE then
        local more = math.random(1, 3)
        local left = more - core.give_item(ev.player, "animals:seeds", more)
        if left > 0 then
            core.drop_item(ev.x, ev.y, ev.z, core.item_id("animals:seeds"), left)
        end
    end
end)

core.log("farm: a hoe tills grass, seeds grow into wheat, three wheat are bread; melon and pumpkin stems")

-- What each item looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
for _, n in { "bread", "hoe", "wheat", "melon_seeds", "pumpkin_seeds" } do
    core.register_model(`farm:{n}_held`, `{n}_held.gltf`)
end
