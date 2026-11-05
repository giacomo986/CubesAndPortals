--!strict
-- villagers: the people who live in the sandbox's houses.
--
-- Every bed with a crafting table within five cells of it is a HOME -- the
-- houses mods/villages builds, or one a player builds -- and each home has one
-- inhabitant, of one of three kinds made in Blender on the robot's skeleton
-- (tools/make_villager_models.py):
--
--   human  a farmer: reaps the ripe wheat near home, sows it again, tills
--          the empty soil, and bakes bread out of three sheaves.
--   toad   a fisher: walks to the water's edge, casts (the float of
--          mods/pets sits on the water) and now and then brings up a fish.
--   mole   a forager: picks melons, mushrooms and seeds from the grass.
--
-- Whatever its work, each one keeps the same day: works from dawn, cooks what
-- it caught or picked at the village's furnace (or its own table when there is
-- none) around noon, eats when it is hungry, puts what it does not need in the
-- chest of its house (the nearest to its bed), and at dusk goes home to sleep in its bed. Struck down, it is
-- back in its bed five minutes later.
--
-- A village keeps a WATCH, at the house nearest its middle: guards in iron
-- with swords, archers in green hoods, and an inspector with a ledger who
-- walks up to whoever comes by and looks in their bag. What a player takes
-- out of a village's chests, reaps in its fields or breaks off its houses is
-- marked stolen (core.owned_area). Caught with it a first time, they may give
-- it back -- it goes into the village's chest -- and the village trusts them
-- no more; refused, or caught again, the guards and archers go for them while
-- they are in the village, until they die. Three days without a theft caught
-- and the village forgets. Each village marks with its own number, and only
-- its own inspector asks after it; what a player built there is theirs. By
-- night the watch fights whatever comes out of the dark within 8 of the
-- village.
--
-- The engine walks them (a "worker" creature, core.mob_goto) and shows what
-- they do (core.mob_act: a clip and a thing in the hand); this file is what
-- they decide. Each inhabitant is a coroutine that says its day in order and
-- yields while it walks or works.
--
-- The mod is in several files, each a chunk with its own 200 locals (a main
-- chunk may hold no more), run in this order. What one file gives the next
-- is in common.luau's table: the names a later file reads, set at the end of
-- the file that made them, and the few that change as the game goes (the
-- clock, the villages), kept there and read there by everybody.

require("./people")
require("./inspector")
require("./land")
require("./mayor")
require("./trade")
require("./errands")
require("./events")
require("./crafts")
require("./gather")
require("./clock")
