--!strict
-- A slab and a stair: blocks that are not the cell they live in.
--
-- `core.node_box` has existed since the chest, and until OPEN-43 it drew a
-- shape and nothing else -- a body walked into the empty half of a cell as if
-- it were stone, and stood half a node above a slab on nothing. Now the boxes
-- are where a body may be and what an aim finds, and this is the mod that puts
-- some in the world: a mechanism nothing ships with is a mechanism nobody has
-- run, which is what mods/tools said about hardness and mods/timber about the
-- twenty-four turns.
--
-- Two shapes and they are not the same lesson:
--
--   SLAB   one box, half a cell tall. The simplest thing a cube cannot be, and
--          the one that shows immediately: you stand on it half a node up.
--   STAIR  two boxes and a FRONT, so the shape has to be turned with the cell.
--          Its low step and its high step are the same cell at the same height,
--          which is a difference nothing that reads cells can see.

-- Stone, so the pick is the tool for both and a shovel is no help -- the rule
-- mods/tools declared. A slab is quicker than the full block it is cut from,
-- which is the only place in this game where a shape means a number.
local SLAB = core.register_node("steps:slab", 150, 146, 138, true, 0, "", 0, 0.9, "stone")
core.node_box(SLAB, 0.0, 0.0, 0.0, 1.0, 0.5, 1.0)

-- "facedir": all twenty-four orientations, of which a placement aims four --
-- the quarter turns. A stair on its side or upside down is something a mod or a
-- tool writes with core.set_facing, because there is no way for a player to aim
-- at it with one click.
local STAIR = core.register_node("steps:stair", 150, 146, 138, true, 0, "facedir", 0, 1.2,
                                 "stone")

-- The low step is the whole floor of the cell; the high one is the back half of
-- what is above it. Unturned, "back" is -z, and the turn takes it round with
-- the block: what a player sees is where they are stopped, and both of those
-- come from these two boxes.
core.node_box(STAIR, 0.0, 0.0, 0.0, 1.0, 0.5, 1.0)
core.node_box(STAIR, 0.0, 0.5, 0.0, 1.0, 1.0, 0.5)

-- Makeable, like the pick: three stone in a row gives six slabs, and six slabs
-- in the stair's own shape give four stairs. Names and not ids -- the item form
-- of a block is minted after every mod has loaded (mods/craft says why).
core.recipe_shaped("steps:slab", 6, "core:stone core:stone core:stone")

core.recipe_shaped("steps:stair", 4,
    "steps:slab -          -",
    "steps:slab steps:slab -",
    "steps:slab steps:slab steps:slab")

-- Stairs of wood and of sandstone, as the classic block game's: six of the block in the
-- stair's shape give four. The same two boxes; each wears its block's picture
-- (assets/textures/pack.txt). What a village roofs its houses with.
for _, s in {
    { "oak", "timber:planks", { 176, 136, 84 }, "wood" },
    { "spruce", "woods:spruce_planks", { 114, 84, 50 }, "wood" },
    { "dark_oak", "woods:dark_oak_planks", { 66, 43, 21 }, "wood" },
    { "sandstone", "rocks:sandstone", { 216, 202, 155 }, "stone" },
} do
    local c = s[3] :: { number }
    local id = core.register_node(`steps:{s[1]}_stair`, c[1], c[2], c[3], true, 0, "facedir", 0, 1.2,
                                  s[4] :: string)
    core.node_box(id, 0.0, 0.0, 0.0, 1.0, 0.5, 1.0)
    core.node_box(id, 0.0, 0.5, 0.0, 1.0, 1.0, 0.5)
    local b = s[2] :: string
    core.recipe_shaped(`steps:{s[1]}_stair`, 4, `{b} - -`, `{b} {b} -`, `{b} {b} {b}`)
end

-- And the ways a village is paved with, so its streets and paths climb by
-- stairs of their own stone and earth (mods/villages): a stair and a slab of
-- each. Cobblestone and smooth stone are cut as stone is; gravel and coarse
-- dirt are not cut by anybody, and broken they give back the block.
for _, s in {
    { "cobblestone", "rocks:cobblestone", { 122, 118, 116 }, "stone", true },
    { "smooth_stone", "rocks:smooth_stone", { 160, 160, 160 }, "stone", true },
    { "gravel", "gravel:gravel", { 132, 126, 122 }, "soil", false },
    { "coarse_dirt", "rocks:coarse_dirt", { 110, 80, 56 }, "soil", false },
    { "oak", "timber:planks", { 176, 136, 84 }, "wood", true },
} do
    local c = s[3] :: { number }
    local b = s[2] :: string
    if s[1] ~= "oak" then
        local stair = core.register_node(`steps:{s[1]}_stair`, c[1], c[2], c[3], true, 0, "facedir", 0, 1.2,
                                         s[4] :: string)
        core.node_box(stair, 0.0, 0.0, 0.0, 1.0, 0.5, 1.0)
        core.node_box(stair, 0.0, 0.5, 0.0, 1.0, 1.0, 0.5)
        if s[5] then
            core.recipe_shaped(`steps:{s[1]}_stair`, 4, `{b} - -`, `{b} {b} -`, `{b} {b} {b}`)
        elseif core.node_id(b) ~= 0 then
            core.node_drop(stair, core.node_id(b))
        end
    end
    local slab = core.register_node(`steps:{s[1]}_slab`, c[1], c[2], c[3], true, 0, "", 0, 0.9, s[4] :: string)
    core.node_box(slab, 0.0, 0.0, 0.0, 1.0, 0.5, 1.0)
    if s[5] then
        core.recipe_shaped(`steps:{s[1]}_slab`, 6, `{b} {b} {b}`)
    elseif core.node_id(b) ~= 0 then
        core.node_drop(slab, core.node_id(b))
    end
end

-- ------------------------------------------------------------------ a model
--
-- The other drawtype M11 asks for: a block drawn as a glTF instead of as a cube
-- or as boxes. `marker.gltf` is beside this file -- written by
-- tools/make_marker_gltf.py rather than modelled, because what this has to
-- prove is the pipeline and not the art -- and it is one file: its buffer is
-- embedded, so nothing has to travel beside it.
--
-- The MODEL is the picture and the BOXES are the block. This one declares both:
-- a spike to look at and a low box to walk into, because a mesh is a thousand
-- triangles and a collision is a handful of boxes. The original engine says the same thing
-- with `collision_box` beside `drawtype = mesh`.
local MARKER_MODEL = core.register_model("steps:marker", "marker.gltf")

local MARKER = core.register_node("steps:marker", 200, 170, 90, true, 0, "upright", 0, 0.6,
                                  "wood")
core.node_model(MARKER, MARKER_MODEL)
core.node_box(MARKER, 0.15, 0.0, 0.15, 0.85, 0.6, 0.85)

-- ------------------------------------------------------------- the set piece
--
-- In the plaza, clear of the other mods' corners: the crafting table, the chest
-- and the bench are between x 37 and 41, the beacons at 50 42, the spike at
-- 44 44, the palette at 60 44.
local BASE_X = 50
local SLAB_Z = 47
local STAIR_Z = 45

-- On the ground the mapgen made, not at a height this file guessed, and its OWN
-- blocks are not ground: a world loaded from a file already has the set piece
-- in it, and a mod that counted its own blocks would stack a second one on top,
-- half a node higher every restart. That defect happened to mods/tools and
-- tests/persistence.sh is what caught it.
local function ground(x: number, z: number): number
    for y = 40, 1, -1 do
        local id = core.get_block(x, y, z)
        if id ~= 0 and id ~= SLAB and id ~= STAIR and id ~= MARKER then
            return y
        end
    end
    return -1
end

-- On world_ready and not at load: the world file is read AFTER the mods
-- register their blocks, so a set piece built at load is laid back over by the
-- stored chunks.
core.event.on("core:world_ready", function()
    -- The sandbox's set piece only: the Inferno loads this mod for what it
    -- holds, and mods/guard's square is what makes a world the sandbox.
    if core.node_id("guard:cloud") == 0 then
        return
    end
    -- Three slabs in a row, which is a ledge to step onto rather than one block
    -- to look at. Half a node is below the jump this engine gives a player, so
    -- it is walked onto -- and, until something gives a body a step height, it
    -- is jumped onto rather than strolled onto. TODO.md OPEN-43 says so.
    local slabs = 0
    for dx = 0, 2 do
        local gy = ground(BASE_X + dx, SLAB_Z)
        if gy > 0 then
            core.set_block(BASE_X + dx, gy + 1, SLAB_Z, SLAB)
            slabs += 1
        end
    end

    -- And a stair, turned to 0 by this mod rather than by a placement: a set
    -- piece that came out facing whichever way is a set piece nothing can make
    -- a claim about. Facing 0 puts its high step at -z, so the two halves of
    -- one cell are at z and z + 1 -- which is what tests/shape.sh asks the
    -- server about.
    local sy = ground(BASE_X, STAIR_Z)
    if sy > 0 then
        core.set_block(BASE_X, sy + 1, STAIR_Z, STAIR)
        core.set_facing(BASE_X, sy + 1, STAIR_Z, 0)
    end

    -- And the marker, beside the stair, turned so that a client that draws it
    -- unturned is drawing something visibly different.
    local my = ground(BASE_X + 2, STAIR_Z)
    if my > 0 then
        core.set_block(BASE_X + 2, my + 1, STAIR_Z, MARKER)
        core.set_facing(BASE_X + 2, my + 1, STAIR_Z, 1)
    end

    core.log(`steps: {slabs} slab(s) at {BASE_X} {SLAB_Z}, a stair at {BASE_X} {STAIR_Z}, ` ..
             `a marker at {BASE_X + 2} {STAIR_Z}`)
end)
