--!strict
-- colors: The classic block game's sixteen dyes and what they colour -- wool, terracotta,
-- concrete and concrete powder.
--
-- The dyes come from the flowers of mods/plants as the classic block game's do, blue from
-- lapis, white from a bone, green from a cactus in the furnace, and the rest
-- mixed. Brown and black have no cocoa or squid here: a brown mushroom and a
-- lump of coal stand in for them. Wool is four string, and white wool and a
-- dye make wool of the dye's colour; eight terracotta round a dye make eight of
-- its colour; a dye with four sand and four gravel makes eight concrete powder,
-- which falls like sand and sets into concrete where it touches water.
local COLOURS = {
    { "white", { 234, 236, 236 } }, { "orange", { 240, 118, 20 } }, { "magenta", { 190, 68, 180 } },
    { "light_blue", { 58, 176, 218 } }, { "yellow", { 248, 198, 40 } }, { "lime", { 112, 186, 26 } },
    { "pink", { 238, 142, 172 } }, { "gray", { 62, 68, 72 } }, { "light_gray", { 142, 142, 136 } },
    { "cyan", { 22, 138, 146 } }, { "purple", { 122, 42, 172 } }, { "blue", { 52, 56, 158 } },
    { "brown", { 114, 72, 40 } }, { "green", { 84, 110, 28 } }, { "red", { 160, 38, 34 } },
    { "black", { 20, 20, 24 } },
}

core.register_node("colors:terracotta", 152, 94, 67, true, 0, "", 0, 1.25, "stone")

local SETS: { [number]: number } = {}   -- powder id -> concrete id
for i, entry in COLOURS do
    local c, rgb = entry[1], entry[2]
    local dye = `colors:{c}_dye`
    core.register_item(dye, 64, "", 0, 1.0, "")
    -- One of the engine's sixteen, in the same order (core.recipe_dye): what
    -- colours a backpack at a table.
    core.item_dye(dye, i)
    core.register_node(`colors:{c}_wool`, rgb[1], rgb[2], rgb[3], true, 0, "", 0, 0.8, "")
    core.register_node(`colors:{c}_terracotta`, rgb[1], rgb[2], rgb[3], true, 0, "", 0, 1.25, "stone")
    local concrete = core.register_node(`colors:{c}_concrete`, rgb[1], rgb[2], rgb[3], true, 0, "", 0, 1.8, "stone")
    local powder = core.register_node(`colors:{c}_concrete_powder`, rgb[1], rgb[2], rgb[3], true, 0, "", 0, 0.5, "soil")
    core.node_falls(powder)
    SETS[powder] = concrete

    if c ~= "white" then
        core.recipe_shapeless(`colors:{c}_wool`, 1, `colors:white_wool {dye}`)
    end
    core.recipe_shaped(`colors:{c}_terracotta`, 8,
        "colors:terracotta colors:terracotta colors:terracotta",
        `colors:terracotta {dye} colors:terracotta`,
        "colors:terracotta colors:terracotta colors:terracotta")
    core.recipe_shapeless(`colors:{c}_concrete_powder`, 8,
        `{dye} core:sand core:sand core:sand core:sand gravel:gravel gravel:gravel gravel:gravel gravel:gravel`)
end
core.recipe_shaped("colors:white_wool", 1, "archery:string archery:string", "archery:string archery:string")

-- Where each dye comes from.
local FROM = {
    { "white", "plants:lily_of_the_valley", 1 }, { "white", "mobs:bone", 3 },
    { "orange", "plants:orange_tulip", 1 }, { "magenta", "plants:allium", 1 },
    { "light_blue", "plants:blue_orchid", 1 }, { "yellow", "plants:dandelion", 1 },
    { "pink", "plants:pink_tulip", 1 }, { "light_gray", "plants:azure_bluet", 1 },
    { "light_gray", "plants:oxeye_daisy", 1 }, { "light_gray", "plants:white_tulip", 1 },
    { "blue", "plants:cornflower", 1 }, { "blue", "ore:lapis", 1 },
    { "brown", "plants:brown_mushroom", 1 }, { "red", "plants:poppy", 1 }, { "red", "plants:red_tulip", 1 },
    { "black", "ore:coal", 1 },
}
for _, f in FROM do
    core.recipe_shapeless(`colors:{f[1]}_dye`, f[3], f[2])
end
local MIX = {
    { "lime", 2, "colors:green_dye colors:white_dye" }, { "gray", 2, "colors:black_dye colors:white_dye" },
    { "cyan", 2, "colors:blue_dye colors:green_dye" }, { "purple", 2, "colors:blue_dye colors:red_dye" },
    { "orange", 2, "colors:red_dye colors:yellow_dye" }, { "pink", 2, "colors:red_dye colors:white_dye" },
    { "magenta", 2, "colors:purple_dye colors:pink_dye" }, { "light_blue", 2, "colors:blue_dye colors:white_dye" },
}
for _, m in MIX do
    core.recipe_shapeless(`colors:{m[1]}_dye`, m[2], m[3])
end

-- Powder sets where it touches water: put down beside it, or come to rest in it.
local function wet(x: number, y: number, z: number): boolean
    for _, d in { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } } do
        if string.find(core.node_name(core.get_block(x + d[1], y + d[2], z + d[3])), "^lakes:") then
            return true
        end
    end
    return false
end

local function set_if_wet(ev)
    local concrete = SETS[ev.id]
    if concrete ~= nil and core.get_block(ev.x, ev.y, ev.z) == ev.id and wet(ev.x, ev.y, ev.z) then
        core.set_block(ev.x, ev.y, ev.z, concrete)
    end
end
core.event.on("core:placed", set_if_wet)
core.event.on("core:landed", set_if_wet)
