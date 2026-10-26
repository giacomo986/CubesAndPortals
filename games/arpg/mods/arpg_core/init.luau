--!strict
-- arpg_core: the world of a point-and-click action game in the manner of isometric action RPGs
-- (DESIGN.md M13): its blocks, what a player is, the creatures and the town's
-- people. The ground itself -- the town, the zones, their corridors -- is
-- arpg_game's, which lays it out and walks it with the same table.

core.register_node("arpg_core:grass", 74, 82, 58)
core.register_node("arpg_core:moss", 60, 70, 50)
core.register_node("arpg_core:dirt", 96, 78, 58)
core.register_node("arpg_core:rock", 52, 50, 50)
core.register_node("arpg_core:cobble", 110, 106, 100)
core.register_node("arpg_core:mossy", 88, 98, 80)
core.register_node("arpg_core:grave", 150, 150, 156)
core.register_node("arpg_core:brazier", 255, 150, 40, true, 14)
core.register_node("arpg_core:waystone", 90, 140, 255, true, 12)
core.register_node("arpg_core:planks", 128, 92, 56)
core.register_node("arpg_core:awning", 150, 40, 40)
-- The high ground round every place a player can walk: a cliff's face and its
-- top, darker than the floor so the way through reads from above.
core.register_node("arpg_core:cliff", 44, 42, 46)
core.register_node("arpg_core:highland", 38, 46, 34)
-- The town: its square, its roofs, the stash and the chests out in the zones
-- (shut and opened).
core.register_node("arpg_core:flags", 132, 124, 112)
core.register_node("arpg_core:roof", 92, 44, 36)
core.register_node("arpg_core:stash", 170, 130, 50, true, 6)
core.register_node("arpg_core:coffer", 150, 110, 40, true, 5)
core.register_node("arpg_core:coffer_open", 80, 64, 40)

-- Nothing of the engine's terrain: the ground is arpg_game's alone.
core.mapgen_core(false)

-- A player is the sandbox's robot (mods/robot, linked here as games/track and
-- games/yafps do), seen from above.
core.player_model(core.register_model("arpg_core:robot", "robot.gltf"))
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
    core.register_model("arpg_core:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
core.eye_height(1.5)
core.register_model("arpg_core:robot_arms", "robot_arms.gltf")

-- The sandbox's walk and run; a jump, but no flying, and no hanging from a
-- ledge: from above, a body hanging off a wall is a body stuck halfway up it.
core.movement(4.8, 8, 0, 8, -25)
core.ledges(false)

-- The creatures. What they do is the engine's (core/ecs/mob.h); `spawn_on`
-- names a block no world has, so none of them comes out of the dark by itself:
-- arpg_game puts them where its encounters are. Every one has 1000 points:
-- arpg_game keeps each one's real life, which grows with the zone, and takes
-- off the share of the thousand its blow is of that. Their blows are what a
-- zone of the first level does, and arpg_game scales what a player takes by
-- where they stand.
local NEVER = "arpg_core:never"
local ZOMBIE   = core.register_model("arpg_core:zombie", "zombie.gltf")
local SKELETON = core.register_model("arpg_core:skeleton", "skeleton.gltf")
local IMP      = core.register_model("arpg_core:imp", "imp.gltf")
local ARROW    = core.register_model("arpg_core:arrow", "arrow.gltf")
local IMPFIRE  = core.register_model("arpg_core:fireball", "fireball.gltf")
--                  name                   model     hp    speed fight     dmg reach every drop n shot     doors hover fire   spawn_on
core.register_mob("arpg_core:zombie",     ZOMBIE,   1000, 2.0, "melee",  3,  1.7,  1.5, "", 1, 0,       false, 0,    false, NEVER)
core.register_mob("arpg_core:skeleton",   SKELETON, 1000, 2.4, "archer", 2,  14,   2.2, "", 1, ARROW,   false, 0,    false, NEVER)
core.register_mob("arpg_core:imp",        IMP,      1000, 3.2, "archer", 3,  12,   2.6, "", 1, IMPFIRE, false, 0,    true,  NEVER)
-- The two zones' lords: a zombie that hits like a door, and an imp that burns.
-- Their sounds are their kin's (links beside this file).
core.register_mob("arpg_core:grave_lord", ZOMBIE,   1000, 2.6, "melee",  7,  2.0,  1.4, "", 1, 0,       false, 0,    false, NEVER)
core.register_mob("arpg_core:ember_wing", IMP,      1000, 3.6, "archer", 8,  14,   2.0, "", 1, IMPFIRE, false, 0,    true,  NEVER)
for _, n in { "zombie", "skeleton", "imp", "grave_lord", "ember_wing" } do
    core.mob_sight(`arpg_core:{n}`, 16)
end

-- The town's people: workers that stand at their stalls (arpg_game spawns and
-- turns them) -- the smith, the jeweller, the alchemist, a guard at the gate.
-- The sandbox's villagers, linked from mods/villagers.
for kind, file in { smith = "crafter", jeweler = "merchant", alchemist = "mayor", guard = "guard" } do
    core.register_mob(`arpg_core:{kind}`, core.register_model(`arpg_core:{kind}`, `{file}.gltf`), 1000, 2.0,
                      "worker", 0, 1.6, 1.0, "", 1, 0, false)
end

-- What a character wears and holds, one model for every base of arpg_game's
-- items but the ring (tools/make_arpg_gear.py), on the robot's rig: arpg_game
-- dresses a player in those of what they have equipped (core.player_wear).
for _, base in { "short_sword", "long_sword", "broad_sword", "war_sword", "hand_axe", "battle_axe", "great_axe",
                 "club", "war_hammer", "dagger", "kris", "short_bow", "long_bow", "war_bow", "hunters_bow", "staff",
                 "battle_staff", "wand", "bone_wand", "buckler", "kite_shield", "tower_shield", "orb", "eye_orb",
                 "cap", "helm", "great_helm", "hood", "quilted_armor", "leather_armor", "chain_mail", "plate_mail",
                 "leather_gloves", "chain_gloves", "gauntlets", "boots", "chain_boots", "greaves", "sash", "belt",
                 "heavy_belt", "amulet" } do
    core.register_model(`arpg_core:gear_{base}`, `gear_{base}.gltf`)
end

-- What a character breaks for what is inside (tools/make_arpg_props.py): a
-- barrel, a crate, an urn. Workers that stand where arpg_game puts them, never
-- fight and never move; struck, they jolt ("hurt"), and broken their pieces fly
-- apart ("death"). Their sounds: tools/make_prop_sounds.py.
for _, kind in { "barrel", "crate", "urn" } do
    core.register_mob(`arpg_core:{kind}`, core.register_model(`arpg_core:{kind}`, `prop_{kind}.gltf`), 1000, 0,
                      "worker", 0, 1.0, 1.0, "", 1, 0, false)
end
