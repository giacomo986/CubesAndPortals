--!strict
-- armpack: a metal backpack with two jointed arms rising from its top and
-- reaching forward over the shoulders, a claw on each for one tool more.
--
-- armpack.gltf is made by tools/make_armpack.py on the robot's own rig: the
-- pack bound to "chest", the arms on bones of their own (pack_*), which the
-- robot has not got: the client plays the pack's own clips on them
-- (draw_worn) -- armpack_idle, and at work armpack_work or the one arm's
-- armpack_swing_L/R -- and draws what each claw holds at pack_tool_L/R.
-- armpack_arms.gltf is the same arms seen by the wearer, at the top corners
-- of the view (draw_arms).
--
-- Worn, it opens the bag's two arm squares (register_item's `arms`), drawn
-- over the figure's shoulders: one tool more in each, seen by everybody
-- (protocol 96). A dig uses whichever of the hand and the arms digs the block
-- best (net::best_dig_slot); a hit adds the arms' weapons to the hand's
-- (net::hit_damage). It does not come off while they hold anything.
core.register_item("armpack:armpack", 1, "", 0, 1.0, "", 0, "back", 0, 0, 2)   -- two arm squares
core.register_model("armpack:armpack", "armpack.gltf")
core.register_model("armpack:armpack_arms", "armpack_arms.gltf")   -- the arms in first person (draw_arms)
core.register_model("armpack:armpack_held", "armpack_held.gltf")   -- in the hand and on the ground (tools/make_held_models.py)

core.recipe_dye("armpack:armpack")

-- A backpack, iron for the arms, gold and copper for the joints.
core.recipe_shaped("armpack:armpack", 1,
    "furnace:iron_ingot   -                 furnace:iron_ingot",
    "furnace:iron_ingot   backpack:backpack furnace:iron_ingot",
    "furnace:copper_ingot furnace:gold_ingot furnace:copper_ingot")
