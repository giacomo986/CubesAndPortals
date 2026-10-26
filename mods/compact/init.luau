--!strict
-- compact: The classic block game's storage blocks. Nine of a thing on the crafting table
-- are one block of it, and the block alone is the nine again. Mostly a way to
-- carry a lot in one slot; a cinnabar block is also always powered
-- (mods/cinnabar).
local BLOCKS = {
    { "coal",     "ore:coal",             20, 20, 22,   0, 5.0 },
    { "copper",   "furnace:copper_ingot", 196, 110, 72, 1, 5.0 },
    { "iron",     "furnace:iron_ingot",   220, 220, 220, 1, 5.0 },
    { "gold",     "furnace:gold_ingot",   246, 208, 60, 2, 5.0 },
    { "lapis",    "ore:lapis",            34, 66, 170,  1, 3.0 },
    { "cinnabar", "ore:cinnabar",         170, 20, 16,  0, 5.0 },
    { "diamond",  "ore:diamond",          100, 220, 214, 2, 5.0 },
    { "emerald",  "ore:emerald",          40, 190, 90,  2, 5.0 },
}

for _, b in BLOCKS do
    local name = `compact:{b[1]}`
    core.register_node(name, b[3], b[4], b[5], true, 0, "", b[6], b[7], "stone")
    local item = b[2]
    local row = `{item} {item} {item}`
    core.recipe_shaped(name, 1, row, row, row)
    core.recipe_shapeless(item, 9, name)
end
