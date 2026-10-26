--!strict
-- jetpack: the flight pack, taken off the robot (mods/robot/robot.gltf no
-- longer has it) and worn on its own in the back square of the bag.
--
-- jetpack.gltf is the pack as it was on the robot -- the box, its panel, the
-- two nozzles and the two flames -- Jetpack_Mesh in assets/blender/robot18.blend
-- (tools/export_robot_packs.py), on the robot's own skeleton, so it is
-- posed bone by bone with its wearer: the flames hang on "jet_flame", which
-- the robot's fly and fly_hover clips grow and every other clip keeps at
-- nothing. And it is what flies: without it on the back, F does nothing and a
-- player in the air falls (core.player_fly).
core.register_item("jetpack:jetpack", 1, "", 0, 1.0, "", 0, "back")
core.register_model("jetpack:jetpack", "jetpack.gltf")
core.register_model("jetpack:jetpack_held", "jetpack_held.gltf")   -- in the hand (tools/make_held_models.py)

-- Where the furnace's ingots are, once every mod has loaded (no `depends`:
-- the track world has no furnace and takes the pack from the creative panel).
core.event.on("core:world_ready", function()
    if core.item_id("furnace:iron_ingot") == 0 then
        return
    end
    core.recipe_shaped("jetpack:jetpack", 1,
        "furnace:iron_ingot furnace:gold_ingot furnace:iron_ingot",
        "furnace:iron_ingot -                  furnace:iron_ingot",
        "furnace:iron_ingot -                  furnace:iron_ingot")
end)

-- Looked at every tick, and said only when it changes: take it off in the air
-- and down you come.
local flies: { [number]: boolean } = {}
core.event.on("core:tick", function(ev)
    local here: { [number]: boolean } = {}
    for _, p in core.players() do
        here[p] = true
        local on = core.worn_item(p, "back") == "jetpack:jetpack"
        if flies[p] ~= on and core.player_fly(p, on) == 1 then
            flies[p] = on
        end
    end
    for p in flies do
        if not here[p] then
            flies[p] = nil
        end
    end
end)
