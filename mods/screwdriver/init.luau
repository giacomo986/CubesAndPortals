--!strict
-- screwdriver: The original engine's, for a block that is already placed.
--
-- Right clicked on a block with a front, it goes to the next turn that block
-- may be in (core.node_turns): a quarter turn for a chest or a furnace, the
-- next face for a ladder or a piston, and on through all twenty-four for a
-- log. A block with no front is left alone. An iron ingot over a stick.
core.register_item("screwdriver:screwdriver", 1, "", 0, 1.0, "")

core.recipe_shaped("screwdriver:screwdriver", 1,
    "furnace:iron_ingot",
    "timber:stick")

core.event.on("core:used", function(ev)
    if ev.held ~= "screwdriver:screwdriver" then
        return
    end
    ev.cancel = true
    local turns = core.node_turns(ev.id)
    if turns > 1 then
        core.set_facing(ev.x, ev.y, ev.z, (core.get_facing(ev.x, ev.y, ev.z) + 1) % turns)
    end
end)

-- What it looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
core.register_model("screwdriver:screwdriver_held", "screwdriver_held.gltf")
