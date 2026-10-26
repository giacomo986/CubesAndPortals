--!strict
-- parkour_core: the body a player has in the parkour world, the sandbox's
-- robot (mods/robot, linked here), and how it moves: on foot, no flying, and
-- parkour (core.parkour) -- wall runs, kicks off walls, slides, backflips and
-- the rope. Apart from games/parkour/mods/parkour because that one is also a
-- player's machine's code, which may not link out of its mod.
core.player_model(core.register_model("parkour_core:robot", "robot.gltf"))
-- The extras a player may pick to wear (tools/make_robot_extras.py): the
-- client draws "<player model>_<extra>" on their skeleton in their colours.
for _, extra in {"cap", "tophat", "headphones", "scarf", "bowtie", "medal",
                  -- and who a player may be instead (tools/make_species.py): the
                  -- client draws "<player model>_<species>", its arms and styles.
                  "human", "human_arms", "human_short", "human_long", "human_beard",
                  "mole", "mole_arms", "mole_lamp", "mole_goggles", "mole_glasses",
                  "frog", "frog_arms", "frog_spots", "frog_crown", "frog_lily",
                  -- the extras refitted to each species, worn instead of the robot's
                  "human_cap", "human_tophat", "human_headphones", "human_scarf", "human_bowtie", "human_medal",
                  "mole_cap", "mole_tophat", "mole_headphones", "mole_scarf", "mole_bowtie", "mole_medal",
                  "frog_cap", "frog_tophat", "frog_headphones", "frog_scarf", "frog_bowtie", "frog_medal"} do
    core.register_model("parkour_core:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
core.register_model("parkour_core:robot_arms", "robot_arms.gltf")
core.eye_height(1.5)
core.movement(4.8, 8, 0, 8, -25)
core.parkour(true)
