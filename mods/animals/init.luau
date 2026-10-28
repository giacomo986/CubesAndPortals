--!strict
-- animals: a pig, a cow, a sheep and a chicken, in the manner of the classic block game's.
-- Made in assets/blender/animals.blend by tools/make_animal_models.py, boxes
-- measured in its pixels, with the clips idle, walk and run.
--
-- Wild, and never tamed: pets with nothing that tames them (core/ecs/mob.h,
-- fight "pet"). So they come out by day on sunlit grass, stroll, stop and look
-- at a player who comes near, fight nobody and never burn -- and leave what
-- they are for when they die. Struck, they panic: run from the blow for a few
-- seconds.
--
-- A sheep is shorn with shears (two iron ingots corner to corner): its wool
-- drops at its feet, it panics, and it wears sheep_shorn.gltf for five minutes
-- until the fleece is back; the shears last 238 of that. A chicken lays an egg
-- every two to five minutes, and has one in it when it dies.
--
-- Fed, a grown one is in love for half a minute and goes to another of its
-- kind in love (core.mob_breed), hearts turning over its head: where they meet
-- a young one comes out, half the size with a big head, that keeps near the
-- first of the two and is grown up in twenty minutes. A cow and a sheep eat
-- wheat (mods/farm), a pig and a chicken seeds -- one grass block in eight
-- leaves one when it is dug.
--
-- The meat and the eggs are food: a right click holding one, at anything in
-- reach, eats it -- a little back for raw, more for cooked (mods/furnace).
local PIG     = core.register_model("animals:pig", "pig.gltf")
local COW     = core.register_model("animals:cow", "cow.gltf")
local SHEEP   = core.register_model("animals:sheep", "sheep.gltf")
local CHICKEN = core.register_model("animals:chicken", "chicken.gltf")
local SHORN   = core.register_model("animals:sheep_shorn", "sheep_shorn.gltf")
local HEARTS  = core.register_model("animals:hearts", "hearts.gltf")
local YOUNG = {
    pig     = core.register_model("animals:pig_baby", "pig_baby.gltf"),
    cow     = core.register_model("animals:cow_baby", "cow_baby.gltf"),
    sheep   = core.register_model("animals:sheep_baby", "sheep_baby.gltf"),
    chicken = core.register_model("animals:chicken_baby", "chicken_baby.gltf"),
}

core.register_item("animals:pork", 64, "", 0, 1.0, "")
core.register_item("animals:beef", 64, "", 0, 1.0, "")
core.register_item("animals:wool", 64, "", 0, 1.0, "")
core.register_item("animals:feather", 64, "", 0, 1.0, "")
core.register_item("animals:leather", 64, "", 0, 1.0, "")
core.register_item("animals:egg", 16, "", 0, 1.0, "")
core.register_item("animals:seeds", 64, "", 0, 1.0, "")
core.register_item("animals:cooked_pork", 64, "", 0, 1.0, "")
core.register_item("animals:steak", 64, "", 0, 1.0, "")
core.register_item("animals:fried_egg", 64, "", 0, 1.0, "")
core.register_item("animals:shears", 1, "", 0, 1.0, "", 0, "", 0, 238)
core.recipe_shaped("animals:shears", 1,
    "-                   furnace:iron_ingot",
    "furnace:iron_ingot  -")

-- Slow grazers next to a pet (3.2): a player walks away from any of them.
core.register_mob("animals:pig", PIG, 10, 2.0, "pet", 0, 1.6, 1.0, "animals:pork", 2, 0, false, 0, false,
                  "core:grass")
core.register_mob("animals:cow", COW, 10, 1.8, "pet", 0, 1.6, 1.0, "animals:beef", 2, 0, false, 0, false,
                  "core:grass")
core.register_mob("animals:sheep", SHEEP, 8, 2.0, "pet", 0, 1.6, 1.0, "animals:wool", 1, 0, false, 0, false,
                  "core:grass")
core.register_mob("animals:chicken", CHICKEN, 4, 2.2, "pet", 0, 1.6, 1.0, "animals:feather", 1, 0, false, 0,
                  false, "core:grass")

core.mob_shear("animals:sheep", "animals:shears", "animals:wool", 2, SHORN, 300)
core.mob_lays("animals:chicken", "animals:egg", 120, 300)
-- And a cow's hide, 0 to 2 of it, which is what a book is bound in (mods/cane).
core.mob_drop("animals:cow", "animals:leather", 2)

-- Wheat is mods/farm's: without it a cow and a sheep do not breed.
local FOODS = { cow = "farm:wheat", sheep = "farm:wheat", pig = "animals:seeds", chicken = "animals:seeds" }
for kind, food in FOODS do
    core.mob_breed(`animals:{kind}`, food, YOUNG[kind], 1200, HEARTS)
end

-- Seeds: one grass block in eight, dug, leaves one where it was.
local GRASS = core.node_id("core:grass")
core.event.on("core:broken", function(ev)
    if ev.id == GRASS and math.random(8) == 1 then
        core.drop_item(ev.x, ev.y, ev.z, core.item_id("animals:seeds"), 1)
    end
end)

-- Food: how many points each gives back, of twenty. With mods/hunger loaded
-- it fills the hunger bar instead, and this handler never sees the click.
local EATS: { [string]: number } = {
    ["animals:pork"] = 3, ["animals:beef"] = 3, ["animals:egg"] = 1,
    ["animals:cooked_pork"] = 8, ["animals:steak"] = 8, ["animals:fried_egg"] = 4,
}
core.event.on("core:used", function(ev)
    local gives = EATS[ev.held]
    if gives == nil then
        return
    end
    ev.cancel = true   -- eaten, not used on what is there
    if core.health(ev.player) >= 20 then
        core.notice(ev.player, "Not hungry")
    elseif core.take_item(ev.player, ev.held, 1) > 0 then
        core.heal(ev.player, gives)
    end
end)

-- What each item looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
for _, n in { "beef", "steak", "pork", "cooked_pork", "egg", "fried_egg", "feather",
              "leather", "seeds", "shears", "wool" } do
    core.register_model(`animals:{n}_held`, `{n}_held.gltf`)
end

-- ------------------------------------------------------------- herding
--
-- DYED SHEEP: a dye of mods/colors used on a sheep turns it that colour
-- (core.mob_become: shorn stays shorn, a lamb a lamb); shorn, it gives wool
-- of its colour, and its lambs are its colour. White dye makes it white
-- again. They bleat as a sheep does (core.mob_voice) and never come out on
-- their own. Models: tools/make_dyed_sheep.py.
--
-- MILK: an empty bucket (mods/lakes) used on a cow fills with it; drunk, it
-- gives four points back and the bucket.
--
-- A TROUGH keeps a STABLE: right click it with wheat or seeds to fill it (16
-- at most). Every ten seconds the animals within 8 of it are kept (never sent
-- away, saved with the world: core.mob_keep), and two grown ones of a kind
-- that eats what is in it are put in love (core.mob_love), at two of its food
-- -- until there are eight of that kind.
local DYED = { "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray", "light_gray",
               "cyan", "purple", "blue", "brown", "green", "red", "black" }
for _, c in DYED do
    local kind = `animals:sheep_{c}`
    core.register_mob(kind, core.register_model(kind, `sheep_{c}.gltf`), 8, 2.0, "pet", 0, 1.6, 1.0,
                      `colors:{c}_wool`, 1, 0, false, 0, false, "animals:never")
    core.mob_shear(kind, "animals:shears", `colors:{c}_wool`, 2, SHORN, 300)
    core.mob_breed(kind, "farm:wheat", core.register_model(`{kind}_baby`, `sheep_{c}_baby.gltf`), 1200, HEARTS)
    core.mob_voice(kind, "animals:sheep")
end

core.register_item("animals:milk", 1, "", 0, 1.0, "")

local function is_sheep(kind: string): boolean
    return kind == "animals:sheep" or string.find(kind, "^animals:sheep_") ~= nil
end

core.event.on("core:used_mob", function(ev)
    local kind = core.mob_kind(ev.target)
    local colour = string.match(ev.held, "^colors:(.+)_dye$")
    if colour and is_sheep(kind) then
        local to = if colour == "white" then "animals:sheep" else `animals:sheep_{colour}`
        if to ~= kind and core.take_item(ev.player, ev.held, 1) == 1 then
            core.mob_become(ev.target, to)
        end
    elseif kind == "animals:cow" and ev.held == "lakes:bucket" then
        if core.take_item(ev.player, "lakes:bucket", 1) == 1 then
            core.give_item(ev.player, "animals:milk", 1)
        end
    end
end)

-- Drunk before mods/hunger takes a click as eating (its handler is at -5).
core.event.on("core:used", -10, function(ev)
    if ev.held ~= "animals:milk" then
        return
    end
    ev.cancel = true
    if core.take_item(ev.player, "animals:milk", 1) == 1 then
        core.give_item(ev.player, "lakes:bucket", 1)
        core.heal(ev.player, 4)
    end
end)

local TROUGH = core.register_node("animals:trough", 150, 110, 70, true, 0, "upright", 0, 1.5, "wood")
local FILLED = core.register_node("animals:trough_full", 170, 150, 70, true, 0, "upright", 0, 1.5, "wood")
for _, id in { TROUGH, FILLED } do
    core.node_box(id, 0, 0, 0.1, 1, 0.1, 0.9)
    core.node_box(id, 0, 0.1, 0.1, 1, 0.55, 0.2)
    core.node_box(id, 0, 0.1, 0.8, 1, 0.55, 0.9)
    core.node_box(id, 0, 0.1, 0.2, 0.1, 0.55, 0.8)
    core.node_box(id, 0.9, 0.1, 0.2, 1, 0.55, 0.8)
end
core.node_box(FILLED, 0.1, 0.1, 0.2, 0.9, 0.45, 0.8)
core.node_drop(FILLED, TROUGH)
core.recipe_shaped("animals:trough", 1,
    "timber:planks farm:wheat    timber:planks",
    "timber:planks timber:planks timber:planks")

local EATS_FROM: { [string]: string } = { cow = "farm:wheat", pig = "animals:seeds", chicken = "animals:seeds" }
local function food_of(kind: string): string?
    if is_sheep(kind) then
        return "farm:wheat"
    end
    local k = string.match(kind, "^animals:(%a+)$")
    return if k then EATS_FROM[k] else nil
end

-- The troughs, by "x,y,z", in storage so a stable outlives a restart.
local troughs: { [string]: boolean } = {}
for k in string.gmatch(core.storage_get("troughs"), "[^;]+") do
    troughs[k] = true
end
local function save_troughs()
    local list = {}
    for k in troughs do
        table.insert(list, k)
    end
    core.storage_set("troughs", table.concat(list, ";"))
end

core.event.on("core:placed", function(ev)
    if ev.id == TROUGH then
        troughs[`{ev.x},{ev.y},{ev.z}`] = true
        save_troughs()
    end
end)
core.event.on("core:broken", function(ev)
    if ev.id == TROUGH or ev.id == FILLED then
        troughs[`{ev.x},{ev.y},{ev.z}`] = nil
        save_troughs()
    end
end)
core.event.on("core:used", function(ev)
    if ev.id ~= TROUGH and ev.id ~= FILLED then
        return
    end
    ev.cancel = true
    local food, n = string.match(core.meta_get(ev.x, ev.y, ev.z, "food"), "^(%S+) (%d+)$")
    local have = tonumber(n) or 0
    if ev.held ~= "farm:wheat" and ev.held ~= "animals:seeds" then
        core.notice(ev.player, if have > 0 then `The trough holds {have} {food == "farm:wheat" and "wheat" or "seeds"}`
            else "Fill it with wheat or seeds")
        return
    end
    if have > 0 and food ~= ev.held then
        core.notice(ev.player, "It holds something else")
        return
    end
    local put = core.take_item(ev.player, ev.held, math.min(16 - have, 64))
    if put > 0 then
        core.set_block(ev.x, ev.y, ev.z, FILLED)
        core.meta_set(ev.x, ev.y, ev.z, "food", `{ev.held} {have + put}`)
        troughs[`{ev.x},{ev.y},{ev.z}`] = true
        save_troughs()
    end
end)

local since = 0
core.event.on("core:tick", function(ev)
    since += ev.dt
    if since < 10 then
        return
    end
    since = 0
    for key in troughs do
        local x, y, z = string.match(key, "^(%-?%d+),(%-?%d+),(%-?%d+)$")
        local tx, ty, tz = tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number
        if core.loaded(tx, ty, tz) == 1 then
            local id = core.get_block(tx, ty, tz)
            if id ~= TROUGH and id ~= FILLED then
                troughs[key] = nil
                save_troughs()
            else
                local food, n = string.match(core.meta_get(tx, ty, tz, "food"), "^(%S+) (%d+)$")
                local have = tonumber(n) or 0
                local by_kind: { [string]: { number } } = {}
                for _, m in core.mob_ids(tx + 0.5, ty, tz + 0.5, 8, true) do
                    local kind = core.mob_kind(m)
                    if food_of(kind) then
                        core.mob_keep(m, true)
                        by_kind[kind] = by_kind[kind] or {}
                        table.insert(by_kind[kind], m)
                    end
                end
                for kind, ids in by_kind do
                    if have >= 2 and food_of(kind) == food and #ids < 8 then
                        local loved = 0
                        for _, m in ids do
                            if loved < 2 and core.mob_love(m) == 1 then
                                loved += 1
                            end
                        end
                        if loved > 0 then
                            have -= loved
                        end
                    end
                end
                if have <= 0 then
                    core.meta_set(tx, ty, tz, "food", "")
                    core.set_block(tx, ty, tz, TROUGH)
                else
                    core.meta_set(tx, ty, tz, "food", `{food} {have}`)
                end
            end
        end
    end
end)
