--!strict
-- A furnace: what ore is for.
--
-- Put fuel and something that smelts in it, and every two seconds one of them
-- turns into what it makes: raw iron into an iron ingot, sand into glass, a log
-- into charcoal. A piece of coal lasts eight, a log one and a half (FUELS).
-- While it is doing that it is lit, and gives light. When it runs out of
-- either, it goes dark again. A block of iron is nine ingots and does not smelt.
--
-- It is a chest with a rule on it, and the rule is a modifier and not a timer
-- in on_step: the engine already sweeps the loaded world for blocks of a kind
-- (core/world/modifiers.h), and a furnace nobody is near is a furnace in a
-- column that is not in memory -- which is a furnace that stops, exactly as a
-- player walking away from one would expect it not to. It picks up where it left
-- off when somebody comes back.

-- Two blocks and not one with a flag: light is a property of a block, and a
-- furnace that glows while it burns is a different block while it burns. Both
-- turn to face whoever put them down, and the swap keeps the turn.
local FURNACE = core.register_node("furnace:furnace", 112, 106, 100, true, 0, "upright", 0, 3.0,
                                   "stone")
local LIT     = core.register_node("furnace:lit", 150, 112, 84, true, 13, "upright", 0, 3.0,
                                   "stone")

-- Drawn as Blender models (assets/blender/chest_furnace.blend): a brick oven
-- with an iron-framed firebox, and, lit, the same oven with fire in it -- five
-- flames on bones whose clip "loop" plays round and round, and embers that give
-- their own light (an emissive material is drawn unshaded). A full cell of
-- boxes under each, so it is walked into and aimed at like the cube it was.
core.node_model(FURNACE, core.register_model("furnace:furnace", "furnace.gltf"))
core.node_model(LIT, core.register_model("furnace:lit", "furnace_lit.gltf"))
core.node_box(FURNACE, 0, 0, 0, 1, 1, 1)
core.node_box(LIT, 0, 0, 0, 1, 1, 1)
-- In the hand, on the ground and in the bag: the same oven, small
-- (tools/make_held_models.py makes it out of furnace.gltf, and its icon,
-- assets/textures/furnace_furnace.png).
core.register_model("furnace:furnace_held", "furnace_held.gltf")

-- A lit furnace is a furnace that is burning, not a thing to carry about.
core.node_drop(LIT, FURNACE)

-- What it makes, that is not already a block.
core.register_item("furnace:iron_ingot", 64, "", 0, 1.0, "")

-- Gold smelts to an ingot too.
core.register_item("furnace:gold_ingot", 64, "", 0, 1.0, "")
core.register_item("furnace:copper_ingot", 64, "", 0, 1.0, "")

-- And what the ingot is for: a pick that digs what the stone pick digs, faster,
-- and one level deeper -- the gold and the diamond in mods/ore want level 2.
core.register_item("furnace:iron_pick", 1, "", 2, 9.0, "stone", 0, "", 0, 250)

local SLOTS = 9

-- What burns, and how many things one of it smelts: The classic block game's numbers. What
-- is left of a fuel once it is in is kept in the cell ("heat"), so a log's half
-- smelt is not lost. First in the list is burned first.
local FUELS = {
    { "ore:coal", 8 },
    { "furnace:charcoal", 8 },
    { "compact:coal", 80 },
    { "inferno:cinder_rod", 12 },   -- only where games/inferno is loaded
    { "timber:log", 1.5 }, { "timber:mossy_log", 1.5 }, { "trees:trunk", 1.5 },
    { "woods:birch_log", 1.5 }, { "woods:spruce_log", 1.5 }, { "woods:jungle_log", 1.5 },
    { "woods:acacia_log", 1.5 }, { "woods:dark_oak_log", 1.5 },
    { "timber:planks", 1.5 }, { "woods:birch_planks", 1.5 }, { "woods:spruce_planks", 1.5 },
    { "woods:jungle_planks", 1.5 }, { "woods:acacia_planks", 1.5 }, { "woods:dark_oak_planks", 1.5 },
    { "timber:stick", 0.5 },
}

-- Charcoal: what a log smelts to, and a fuel as good as coal.
core.register_item("furnace:charcoal", 64, "", 0, 1.0, "")
core.register_model("furnace:charcoal_held", "charcoal_held.gltf")
-- It lights a torch as coal does.
core.recipe_shaped("torches:torch", 4, "furnace:charcoal", "timber:stick")

-- What its squares are for, a row each: what it smelts, what it burns, what it
-- made -- and each row takes only its own (core.container_accepts, below the
-- two lists): ore in the first, fuel in the second, and nobody puts anything in
-- the third. Wood is both, a log to charcoal in the first row and a fire in the
-- second, and the first row is never burned.
for _, id in { FURNACE, LIT } do
    core.container_roles(id, "raw raw raw fuel fuel fuel product product product")
end

-- In order, and a list and not a table keyed by name: the order a furnace tries
-- things in has to be the same on every run, and a table's is not.
-- The third number is the classic block game's experience for one: kept in the furnace and
-- given to whoever next opens it, as furnace:xp (mods/enchant counts it).
local SMELTS = {
    { "ore:raw_iron", "furnace:iron_ingot", 0.7 },
    { "ore:raw_gold", "furnace:gold_ingot", 1.0 },
    { "ore:raw_copper", "furnace:copper_ingot", 0.7 },
    { "core:sand", "core:glass", 0.1 },
    -- mods/rocks, mods/plants and mods/colors, skipped when they are not loaded.
    { "rocks:red_sand", "core:glass", 0.1 },
    { "rocks:cobblestone", "core:stone", 0.1 },
    { "core:stone", "rocks:smooth_stone", 0.1 },
    { "rocks:clay_ball", "rocks:brick", 0.3 },
    { "rocks:clay", "colors:terracotta", 0.35 },
    { "plants:cactus", "colors:green_dye", 1.0 },
    -- mods/animals's meat and eggs, cooked; skipped when it is not loaded.
    { "animals:pork", "animals:cooked_pork", 0.35 },
    { "animals:beef", "animals:steak", 0.35 },
    { "animals:egg", "animals:fried_egg", 0.35 },
    { "pets:fish", "pets:cooked_fish", 0.35 },
    -- Any log, to charcoal (mods/woods's, mods/trees's, when they are loaded).
    { "timber:log", "furnace:charcoal", 0.15 }, { "timber:mossy_log", "furnace:charcoal", 0.15 },
    { "trees:trunk", "furnace:charcoal", 0.15 },
    { "woods:birch_log", "furnace:charcoal", 0.15 }, { "woods:spruce_log", "furnace:charcoal", 0.15 },
    { "woods:jungle_log", "furnace:charcoal", 0.15 }, { "woods:acacia_log", "furnace:charcoal", 0.15 },
    { "woods:dark_oak_log", "furnace:charcoal", 0.15 },
}

-- Each row takes only its own: the ore row what smelts, the fuel row what burns.
do
    local raw, fuel = {}, {}
    for _, s in SMELTS do table.insert(raw, s[1]) end
    for _, f in FUELS do table.insert(fuel, f[1]) end
    for _, id in { FURNACE, LIT } do
        core.container_accepts(id, "raw", table.concat(raw, " "))
        core.container_accepts(id, "fuel", table.concat(fuel, " "))
    end
end

core.event.define("furnace:xp", {
    fields = { player = "number", points = "number" },
    cancellable = false,
})

-- Eight bricks round a hole, the way the frame is eight stone round one.
core.recipe_shaped("furnace:furnace", 1,
    "craft:brick craft:brick craft:brick",
    "craft:brick -           craft:brick",
    "craft:brick craft:brick craft:brick")

-- Three ingots in a V, which is where everybody looks for it. The bucket is
-- mods/lakes' item -- it is water's tool and water's rules are there -- but the
-- iron is here, and a recipe names items by name and not by whose they are.
core.recipe_shaped("lakes:bucket", 1,
    "furnace:iron_ingot -                  furnace:iron_ingot",
    "-                  furnace:iron_ingot -")

core.recipe_shaped("furnace:iron_pick", 1,
    "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot",
    "-                  timber:stick       -",
    "-                  timber:stick       -")

-- And the rest of the ladder, in the shapes of mods/tools: wood, stone, then
-- these. Iron digs to 2 (the gold and the diamond); gold is the classic block
-- game's gold -- the fastest there is and no deeper than wood; diamond is
-- fast and one level past anything the stone holds yet. The pick sets the
-- speed and the axe and shovel dig at four fifths of it, as stone's do.
local METALS = {
    { name = "iron", from = "furnace:iron_ingot", level = 2, speed = 9.0, damage = 5, lasts = 250 },
    { name = "gold", from = "furnace:gold_ingot", level = 0, speed = 15.0, damage = 3, lasts = 32 },
    { name = "diamond", from = "ore:diamond", level = 3, speed = 12.0, damage = 6, lasts = 1561 },
}
for _, m in METALS do
    local h = m.from
    if m.name ~= "iron" then   -- the iron pick is above, and older
        core.register_item(`furnace:{m.name}_pick`, 1, "", m.level, m.speed, "stone", 0, "", 0, m.lasts)
        core.recipe_shaped(`furnace:{m.name}_pick`, 1, `{h} {h} {h}`, "- timber:stick -", "- timber:stick -")
    end
    core.register_item(`furnace:{m.name}_axe`, 1, "", m.level, m.speed * 0.8, "wood", 0, "", 0, m.lasts)
    core.register_item(`furnace:{m.name}_shovel`, 1, "", m.level, m.speed * 0.8, "soil", 0, "", 0, m.lasts)
    core.register_item(`furnace:{m.name}_sword`, 1, "", 0, 1.0, "", m.damage, "", 0, m.lasts)
    core.register_item(`furnace:{m.name}_hoe`, 1, "", 0, 1.0, "", 0, "", 0, m.lasts)
    core.recipe_shaped(`furnace:{m.name}_hoe`, 1, `{h} {h}`, "- timber:stick", "- timber:stick")
    core.recipe_shaped(`furnace:{m.name}_axe`, 1, `{h} {h} -`, `{h} timber:stick -`, "- timber:stick -")
    core.recipe_shaped(`furnace:{m.name}_shovel`, 1, `- {h} -`, "- timber:stick -", "- timber:stick -")
    core.recipe_shaped(`furnace:{m.name}_sword`, 1, `- {h} -`, `- {h} -`, "- timber:stick -")
    for _, k in { "pick", "axe", "shovel", "sword", "hoe" } do
        if k ~= "pick" or m.name ~= "iron" then
            core.register_model(`furnace:{m.name}_{k}_held`, `{m.name}_{k}_held.gltf`)
        end
    end
end

local function is_furnace(id: number): boolean
    return id == FURNACE or id == LIT
end

-- Right click: look inside. Claimed, like the chest, or the same click would
-- open it and build on top of it.
core.event.on("core:used", function(ev)
    if not is_furnace(ev.id) then
        return
    end
    -- What it has smelted since it was last opened, in whole points; the
    -- fraction stays for next time.
    local kept = tonumber(core.meta_get(ev.x, ev.y, ev.z, "xp")) or 0
    if kept >= 1 and ev.player > 0 then
        local whole = math.floor(kept)
        core.event.emit("furnace:xp", { player = ev.player, points = whole })
        core.meta_set(ev.x, ev.y, ev.z, "xp", tostring(kept - whole))
    end
    core.container_show(ev.x, ev.y, ev.z, SLOTS, ev.player)
    ev.cancel = true
end)

-- Broken: what was inside falls out, then the furnace goes.
core.event.on("core:pre_break", function(ev)
    if not is_furnace(ev.id) then
        return
    end
    core.container_spill(ev.x, ev.y, ev.z)
    core.container_clear(ev.x, ev.y, ev.z)
end)

-- Asked for by name when first needed and not at load: the item form of a block
-- is minted after every mod has loaded, so core.item_id up here answers 0. A
-- name nobody registered stays 0 and is skipped.
local fuel_ids: { { number } } = {}
local smelt_ids: { { number } } = {}

local function resolve(): boolean
    if #fuel_ids > 0 then
        return true
    end
    for _, f in FUELS do
        local id = core.item_id(f[1])
        if id ~= 0 then
            table.insert(fuel_ids, { id, f[2] })
        end
    end
    for _, pair in SMELTS do
        local from, to = core.item_id(pair[1]), core.item_id(pair[2])
        if from ~= 0 and to ~= 0 then
            table.insert(smelt_ids, { from, to, pair[3] })
        end
    end
    return #fuel_ids > 0
end

-- What to smelt, out of the ore row.
local function pick_input(x: number, y: number, z: number): { number }?
    for _, s in smelt_ids do
        if core.container_count(x, y, z, s[1], "raw") > 0 then return s end
    end
    return nil
end

-- What to burn, out of the fuel row only: the ore row is never burned.
local function pick_fuel(x: number, y: number, z: number): { number }?
    for _, f in fuel_ids do
        if core.container_count(x, y, z, f[1], "fuel") > 0 then return f end
    end
    return nil
end

local function become(x: number, y: number, z: number, id: number, want: number)
    if id == want then
        return
    end
    local facing = core.get_facing(x, y, z)
    core.set_block(x, y, z, want)
    core.set_facing(x, y, z, facing)
end

-- A smelt takes COOK seconds, shown to whoever has it open as a bar filling
-- (core.container_progress). The cell remembers when the one going on ends
-- ("cook", on this mod's clock) and what it is ("what"): started when there is
-- ore, room for what it makes and a whole smelt's heat -- fuel burned from the
-- fuel row until there is -- and made at the end if the ore is still there.
-- Burned is burned: heat not used is kept for the next one ("heat").
local COOK = 2.0
local clock = 0.0
core.event.on("core:tick", function(ev)
    clock += ev.dt
end)

-- A product square empty or holding the same thing. Asked without writing:
-- an idle furnace asks four times a second, and a write is sent to whoever
-- has it open. A full stack there is found out at the end, and waits.
local function room_for(x: number, y: number, z: number, item: number): boolean
    for slot = 7, 9 do
        local it = core.container_item(x, y, z, slot)
        if it == 0 or it == item then return true end
    end
    return false
end

local function start(x: number, y: number, z: number): boolean
    local input = pick_input(x, y, z)
    if not input or not room_for(x, y, z, input[2]) then return false end
    local heat = tonumber(core.meta_get(x, y, z, "heat")) or 0
    while heat < 1 do
        local fuel = pick_fuel(x, y, z)
        if not fuel then break end
        core.container_take(x, y, z, fuel[1], 1, "fuel")
        heat += fuel[2]
    end
    core.meta_set(x, y, z, "heat", tostring(heat))
    if heat < 1 then return false end
    core.meta_set(x, y, z, "cook", tostring(clock + COOK))
    core.meta_set(x, y, z, "what", tostring(input[1]))
    core.container_progress(x, y, z, COOK)
    return true
end

local function stop(x: number, y: number, z: number)
    core.meta_set(x, y, z, "cook", "")
    core.container_progress(x, y, z, 0)
end

core.register_abm("furnace:burn", { "furnace:furnace", "furnace:lit" }, 0.25, 1, function(cell)
    if not resolve() then
        return
    end
    local x, y, z = cell.x, cell.y, cell.z
    local ends = tonumber(core.meta_get(x, y, z, "cook"))
    -- A clock that started again (a restart): the smelt starts again too.
    if ends and ends > clock + COOK then
        ends = clock + COOK
        core.meta_set(x, y, z, "cook", tostring(ends))
        core.container_progress(x, y, z, COOK)
    end

    local what = tonumber(core.meta_get(x, y, z, "what")) or 0
    local cooking = ends ~= nil
    if cooking and core.container_count(x, y, z, what, "raw") == 0 then
        stop(x, y, z)   -- the ore was taken out
        cooking = false
    elseif cooking and clock >= (ends :: number) then
        for _, s in smelt_ids do
            if s[1] == what then
                if core.container_add(x, y, z, s[2], 1, "product") == 0 then
                    core.container_take(x, y, z, s[1], 1, "raw")
                    core.meta_set(x, y, z, "heat", tostring((tonumber(core.meta_get(x, y, z, "heat")) or 1) - 1))
                    local kept = tonumber(core.meta_get(x, y, z, "xp")) or 0
                    core.meta_set(x, y, z, "xp", tostring(kept + s[3]))
                end
                break
            end
        end
        stop(x, y, z)
        cooking = start(x, y, z)
    elseif not cooking then
        cooking = start(x, y, z)
    end

    become(x, y, z, cell.id, if cooking then LIT else FURNACE)
end)

core.log(`furnace: {#SMELTS} things smelt, {#FUELS} burn (a coal, eight smelts), {COOK} s a smelt`)

-- What each item looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
for _, n in { "copper_ingot", "gold_ingot", "iron_ingot", "iron_pick" } do  -- and charcoal, above
    core.register_model(`furnace:{n}_held`, `{n}_held.gltf`)
end
