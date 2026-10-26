--!strict
-- animals: a pig, a cow, a sheep and a chicken, in the manner of the classic block game's.
-- Made in ~/kart_assets/animals.blend by tools/make_animal_models.py, boxes
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
