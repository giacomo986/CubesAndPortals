--!strict
-- robot: what a player looks like here. Made for this project in Blender and
-- split from its .glb into .gltf + .bin; its clips (idle, walk, run, jump_loop,
-- swim, fly, mine, dig, chop, drive_*, ...) are played by the client by name.
core.player_model(core.register_model("robot:robot", "robot.gltf"))
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
    core.register_model("robot:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
-- Without the flight pack it had on its back: that is mods/jetpack now, worn
-- in the back square of the bag. Its flame bone "jet_flame" is still here and
-- still posed by the clips, which is what the pack's flames hang on.

-- 1.8 tall like the body's box, and its visor is at 1.5: that is where the
-- camera is in first person, and what reach is measured from.
core.eye_height(1.5)

-- And the arms seen in first person: the player model's name plus "_arms" is
-- what the client looks for. Holding a pick, a shovel or an axe shows it.
core.register_model("robot:robot_arms", "robot_arms.gltf")
