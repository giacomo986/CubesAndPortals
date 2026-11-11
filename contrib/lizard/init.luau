--!strict
-- lizard: a species for the player, from outside the game.
--
-- A species is a body "<player model>_<name>" on the player model's rig, and
-- its arms seen in first person "<player model>_<name>_arms": registered under
-- those names, the client offers it on the look page after the game's own and
-- draws whoever picked it with it (src/client/main.cpp, addon_species). Made
-- by make_lizard.py, beside this file. It has no styles and wears the robot's
-- extras.
core.register_model("robot:robot_lizard", "robot_lizard.gltf")
core.register_model("robot:robot_lizard_arms", "robot_lizard_arms.gltf")
