--!strict
-- mobs: the creatures. Made in assets/blender/mobs.blend and exported by
-- tools/export_mob_models.py.
--
-- Both stand on the robot's skeleton -- the same bones, 1.8 tall, facing the
-- way it faces -- so the clips the client picks for any body by name play on
-- them too: idle, walk, run, jump_loop, swim, swim_idle. And each has the one
-- it fights with, played while it swings or draws:
--
--   zombie    arms held out in every clip; "attack" swings both down.
--   skeleton  a bow in the left hand and an arrow that shows only while it is
--             drawn; "shoot" draws, looses and nocks the next one.
--
-- What they do is the engine's (core/ecs/mob.h): they come out in the dark
-- near players, go for the nearest within 16 nodes, burn under the open sky by
-- day, and go away when nobody is near. This file says only what each kind is.
local ZOMBIE   = core.register_model("mobs:zombie", "zombie.gltf")
local SKELETON = core.register_model("mobs:skeleton", "skeleton.gltf")
local ARROW    = core.register_model("mobs:arrow", "arrow.gltf")

-- What they leave. Nothing uses either yet: the proof a night was fought.
core.register_item("mobs:flesh", 64, "", 0, 1.0, "")
core.register_item("mobs:bone", 64, "", 0, 1.0, "")

-- Slower than a walking player (4.8), so a player who sees one coming can
-- leave; three a blow, once a second, against a player's twenty.
core.register_mob("mobs:zombie", ZOMBIE, 20, 2.2, "melee", 3, 1.6, 1.0, "mobs:flesh", 1)

-- Keeps six to twelve nodes away and shoots every two seconds from up to
-- fifteen, while it can see who it is shooting at.
core.register_mob("mobs:skeleton", SKELETON, 16, 2.4, "archer", 3, 15.0, 2.0, "mobs:bone", 2, ARROW)

-- A server started with --mobs puts that many zombies round the spawn.
core.mob_model(ZOMBIE)

-- What each item looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
for _, n in { "bone", "flesh" } do
    core.register_model(`mobs:{n}_held`, `{n}_held.gltf`)
end
