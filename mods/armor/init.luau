--!strict
-- Armour: four pieces in four materials, made at a table in the shapes that
-- game uses, and worn in the four squares left of the bag (head to feet).
-- Each piece gives points; each point takes 4% off a blow, up to 20 (80%) --
-- a full diamond set. A fall is not armoured.

-- What each is made of, its points head, chest, legs, feet, and how much each
-- piece lasts per unit (The classic block game's: a helmet 11, a chestplate 16, leggings
-- 15, boots 13 times this). A worn piece spends a quarter of every blow.
local MATERIALS = {
    { "wood",    "timber:planks",      { 1, 3, 2, 1 }, 5 },
    { "gold",    "furnace:gold_ingot", { 2, 5, 3, 1 }, 7 },
    { "iron",    "furnace:iron_ingot", { 2, 6, 5, 2 }, 15 },
    { "diamond", "ore:diamond",        { 3, 8, 6, 3 }, 33 },
}
local LASTS = { 11, 16, 15, 13 }

local PIECES = { { "helmet", "head" }, { "chestplate", "chest" },
                 { "leggings", "legs" }, { "boots", "feet" } }

for _, m in MATERIALS do
    local mat, x = m[1], m[2]
    for i, p in PIECES do
        local name = `armor:{mat}_{p[1]}`
        core.register_item(name, 1, "", 0, 1.0, "", 0, p[2], m[3][i], LASTS[i] * m[4])
        -- And what it looks like on a robot: a model of the item's own name,
        -- bound to the wearer's bones by their names.
        core.register_model(name, `{mat}_{p[1]}.gltf`)
    end

    core.recipe_shaped(`armor:{mat}_helmet`, 1,
        `{x} {x} {x}`,
        `{x} -   {x}`)
    core.recipe_shaped(`armor:{mat}_chestplate`, 1,
        `{x} -   {x}`,
        `{x} {x} {x}`,
        `{x} {x} {x}`)
    core.recipe_shaped(`armor:{mat}_leggings`, 1,
        `{x} {x} {x}`,
        `{x} -   {x}`,
        `{x} -   {x}`)
    core.recipe_shaped(`armor:{mat}_boots`, 1,
        `{x} -   {x}`,
        `{x} -   {x}`)
end

core.log("armor: 16 pieces in wood, gold, iron and diamond")
