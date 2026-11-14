--!strict
-- platformer_core: the body a player has in the platform world -- the
-- sandbox's robot (mods/robot, linked here) -- and how it moves: a jump 2.4
-- nodes high that a run carries 7 along, which is what the level's pits and
-- steps are measured against (games/platformer/mods/platformer), and no
-- flying. The level's pipes and creatures are here too (below).
core.player_model(core.register_model("platformer_core:robot", "robot.gltf"))
-- The extras a player may pick to wear (tools/make_robot_extras.py): the
-- client draws "<player model>_<extra>" on their skeleton in their colours.
for _, extra in {"cap", "tophat", "headphones", "scarf", "bowtie", "medal",
                  -- and who a player may be instead (tools/make_species.py): the
                  -- client draws "<player model>_<species>", its arms and styles.
                  "human", "human_arms", "human_short", "human_long", "human_beard",
                  "mole", "mole_arms", "mole_lamp", "mole_goggles", "mole_glasses",
                  "frog", "frog_arms", "frog_spots", "frog_crown", "frog_lily",
                  -- the knight from Meshy (tools/make_knight_meshy.py), with no styles
                  "knight", "knight_arms",
                  -- the extras refitted to each species, worn instead of the robot's
                  "human_cap", "human_tophat", "human_headphones", "human_scarf", "human_bowtie", "human_medal",
                  "mole_cap", "mole_tophat", "mole_headphones", "mole_scarf", "mole_bowtie", "mole_medal",
                  "frog_cap", "frog_tophat", "frog_headphones", "frog_scarf", "frog_bowtie", "frog_medal"} do
    core.register_model("platformer_core:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
core.register_model("platformer_core:robot_arms", "robot_arms.gltf")
-- The pipes and the power-ups (tools/make_platformer_models.py, Blender) and the creatures
-- (Meshy: tools/meshy_generate.py, then tools/meshy_prop.py), here and not in
-- games/platformer/mods/platformer with the blocks that wear them: that mod
-- is world code, whose every file goes to players as its own.
for _, m in { "pipe_body", "pipe_top", "pipe_hang", "pipe_side", "mushroom", "flower", "fireball",
               "sprout", "spikeshell", "buzzer" } do
    core.register_model(`platformer_core:{m}`, `{m}.gltf`)
end
core.eye_height(1.5)
-- walk, sprint, fly, jump, gravity: the top of a jump is 12^2 / (2 * 30) = 2.4
-- up and 0.8 s long -- 4.4 nodes at a walk, 7.2 at a run.
core.movement(5.5, 9, 0, 12, -30)
