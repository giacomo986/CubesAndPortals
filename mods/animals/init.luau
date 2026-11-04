--!strict
-- animals: a pig, a cow, a sheep and a chicken, in the manner of the classic block game's.
-- Made in assets/blender/animals.blend by tools/make_animal_models.py, boxes
-- measured in its pixels, with the clips idle, walk and run.
--
-- Wild, and never tamed: pets with nothing that tames them (core/ecs/mob.h,
-- fight "pet"). So they come out by day on sunlit grass, stroll, stop and look
-- at a player who comes near, fight nobody and never burn -- and leave what
-- they are for when they die. Struck, they panic: run from the blow for a few
-- seconds.
--
-- A sheep is shorn with shears (two iron ingots corner to corner): its wool
-- drops at its feet, it panics, and it wears sheep_shorn.gltf for five minutes
-- until the fleece is back; the shears last 238 of that. A chicken lays an egg
-- every two to five minutes, and has one in it when it dies.
--
-- Fed, a grown one is in love for half a minute and goes to another of its
-- kind in love (core.mob_breed), hearts turning over its head: where they meet
-- a young one comes out, half the size with a big head, that keeps near the
-- first of the two and is grown up in twenty minutes. A cow and a sheep eat
-- wheat (mods/farm), a pig and a chicken seeds -- one grass block in eight
-- leaves one when it is dug.
--
-- The meat and the eggs are food: a right click holding one, at anything in
-- reach, eats it -- a little back for raw, more for cooked (mods/furnace).
local PIG     = core.register_model("animals:pig", "pig.gltf")
local COW     = core.register_model("animals:cow", "cow.gltf")
local SHEEP   = core.register_model("animals:sheep", "sheep.gltf")
local CHICKEN = core.register_model("animals:chicken", "chicken.gltf")
local SHORN   = core.register_model("animals:sheep_shorn", "sheep_shorn.gltf")
local HEARTS  = core.register_model("animals:hearts", "hearts.gltf")
local YOUNG = {
    pig     = core.register_model("animals:pig_baby", "pig_baby.gltf"),
    cow     = core.register_model("animals:cow_baby", "cow_baby.gltf"),
    sheep   = core.register_model("animals:sheep_baby", "sheep_baby.gltf"),
    chicken = core.register_model("animals:chicken_baby", "chicken_baby.gltf"),
}

core.register_item("animals:pork", 64, "", 0, 1.0, "")
core.register_item("animals:beef", 64, "", 0, 1.0, "")
core.register_item("animals:wool", 64, "", 0, 1.0, "")
core.register_item("animals:feather", 64, "", 0, 1.0, "")
core.register_item("animals:leather", 64, "", 0, 1.0, "")
core.register_item("animals:egg", 16, "", 0, 1.0, "")
core.register_item("animals:seeds", 64, "", 0, 1.0, "")
core.register_item("animals:cooked_pork", 64, "", 0, 1.0, "")
core.register_item("animals:steak", 64, "", 0, 1.0, "")
core.register_item("animals:fried_egg", 64, "", 0, 1.0, "")
core.register_item("animals:shears", 1, "", 0, 1.0, "", 0, "", 0, 238)
core.recipe_shaped("animals:shears", 1,
    "-                   furnace:iron_ingot",
    "furnace:iron_ingot  -")

-- Slow grazers next to a pet (3.2): a player walks away from any of them.
core.register_mob("animals:pig", PIG, 10, 2.0, "pet", 0, 1.6, 1.0, "animals:pork", 2, 0, false, 0, false,
                  "core:grass")
core.register_mob("animals:cow", COW, 10, 1.8, "pet", 0, 1.6, 1.0, "animals:beef", 2, 0, false, 0, false,
                  "core:grass")
core.register_mob("animals:sheep", SHEEP, 8, 2.0, "pet", 0, 1.6, 1.0, "animals:wool", 1, 0, false, 0, false,
                  "core:grass")
core.register_mob("animals:chicken", CHICKEN, 4, 2.2, "pet", 0, 1.6, 1.0, "animals:feather", 1, 0, false, 0,
                  false, "core:grass")

core.mob_shear("animals:sheep", "animals:shears", "animals:wool", 2, SHORN, 300)
core.mob_lays("animals:chicken", "animals:egg", 120, 300)
-- And a cow's hide, 0 to 2 of it, which is what a book is bound in (mods/cane).
core.mob_drop("animals:cow", "animals:leather", 2)

-- Wheat is mods/farm's: without it a cow and a sheep do not breed.
local FOODS = { cow = "farm:wheat", sheep = "farm:wheat", pig = "animals:seeds", chicken = "animals:seeds" }
for kind, food in FOODS do
    core.mob_breed(`animals:{kind}`, food, YOUNG[kind], 1200, HEARTS)
end

-- Seeds: one grass block in eight, dug, leaves one where it was.
local GRASS = core.node_id("core:grass")
core.event.on("core:broken", function(ev)
    if ev.id == GRASS and math.random(8) == 1 then
        core.drop_item(ev.x, ev.y, ev.z, core.item_id("animals:seeds"), 1)
    end
end)

-- Food: how many points each gives back, of twenty. With mods/hunger loaded
-- it fills the hunger bar instead, and this handler never sees the click.
local EATS: { [string]: number } = {
    ["animals:pork"] = 3, ["animals:beef"] = 3, ["animals:egg"] = 1,
    ["animals:cooked_pork"] = 8, ["animals:steak"] = 8, ["animals:fried_egg"] = 4,
}
core.event.on("core:used", function(ev)
    local gives = EATS[ev.held]
    if gives == nil then
        return
    end
    ev.cancel = true   -- eaten, not used on what is there
    if core.health(ev.player) >= 20 then
        core.notice(ev.player, "Not hungry")
    elseif core.take_item(ev.player, ev.held, 1) > 0 then
        core.heal(ev.player, gives)
    end
end)

-- What each item looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
for _, n in { "beef", "steak", "pork", "cooked_pork", "egg", "fried_egg", "feather",
              "leather", "seeds", "shears", "wool", "milk", "cheese", "cake" } do
    core.register_model(`animals:{n}_held`, `{n}_held.gltf`)
end

-- ------------------------------------------------------------- herding
--
-- DYED SHEEP: a dye of mods/colors used on a sheep turns it that colour
-- (core.mob_become: shorn stays shorn, a lamb a lamb); shorn, it gives wool
-- of its colour, and its lambs are its colour. White dye makes it white
-- again. They bleat as a sheep does (core.mob_voice) and never come out on
-- their own. Models: tools/make_dyed_sheep.py.
--
-- MILK: an empty bucket (mods/lakes) used on a cow fills with it; drunk, it
-- gives four points back and the bucket. A bucket of milk makes two cheeses,
-- and with three wheat, two sugar canes and an egg a cake; either way the
-- bucket is left in its square (core.item_returns).
--
-- A TROUGH keeps a STABLE: right click it with wheat or seeds to fill it (16
-- at most). Every ten seconds the animals within 8 of it are kept (never sent
-- away, saved with the world: core.mob_keep), and two grown ones of a kind
-- that eats what is in it are put in love (core.mob_love), at two of its food
-- -- until there are eight of that kind. A kept animal is let go with a
-- right click holding a stick, and no trough keeps it again for ten minutes.
local DYED = { "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray", "light_gray",
               "cyan", "purple", "blue", "brown", "green", "red", "black" }
for _, c in DYED do
    local kind = `animals:sheep_{c}`
    core.register_mob(kind, core.register_model(kind, `sheep_{c}.gltf`), 8, 2.0, "pet", 0, 1.6, 1.0,
                      `colors:{c}_wool`, 1, 0, false, 0, false, "animals:never")
    core.mob_shear(kind, "animals:shears", `colors:{c}_wool`, 2, SHORN, 300)
    core.mob_breed(kind, "farm:wheat", core.register_model(`{kind}_baby`, `sheep_{c}_baby.gltf`), 1200, HEARTS)
    core.mob_voice(kind, "animals:sheep")
end

core.register_item("animals:milk", 1, "", 0, 1.0, "")
core.register_item("animals:cheese", 64, "", 0, 1.0, "")
core.register_item("animals:cake", 16, "", 0, 1.0, "")
core.item_returns("animals:milk", "lakes:bucket")
core.recipe_shapeless("animals:cheese", 2, "animals:milk")
core.recipe_shaped("animals:cake", 1,
    "farm:wheat      animals:milk farm:wheat",
    "cane:sugar_cane animals:egg  cane:sugar_cane",
    "-               farm:wheat   -")
EATS["animals:cheese"] = 4
EATS["animals:cake"] = 10

local let_go: { [number]: number } = {}   -- id -> until when no trough keeps it
local clock = 0

local function is_sheep(kind: string): boolean
    return kind == "animals:sheep" or string.find(kind, "^animals:sheep_") ~= nil
end

local EATS_FROM: { [string]: string } = { cow = "farm:wheat", pig = "animals:seeds", chicken = "animals:seeds" }
local function food_of(kind: string): string?
    if is_sheep(kind) then
        return "farm:wheat"
    end
    local k = string.match(kind, "^animals:(%a+)$")
    return if k then EATS_FROM[k] else nil
end

core.event.on("core:used_mob", function(ev)
    local kind = core.mob_kind(ev.target)
    local colour = string.match(ev.held, "^colors:(.+)_dye$")
    if colour and is_sheep(kind) then
        local to = if colour == "white" then "animals:sheep" else `animals:sheep_{colour}`
        if to ~= kind and core.take_item(ev.player, ev.held, 1) == 1 then
            core.mob_become(ev.target, to)
        end
    elseif food_of(kind) and ev.held == "timber:stick" then
        core.mob_keep(ev.target, false)
        let_go[ev.target] = clock + 600
        core.notice(ev.player, "Let go: it wanders where it will")
    elseif kind == "animals:cow" and ev.held == "lakes:bucket" then
        if core.take_item(ev.player, "lakes:bucket", 1) == 1 then
            core.give_item(ev.player, "animals:milk", 1)
        end
    end
end)

-- Drunk before mods/hunger takes a click as eating (its handler is at -5).
core.event.on("core:used", -10, function(ev)
    if ev.held ~= "animals:milk" then
        return
    end
    ev.cancel = true
    if core.take_item(ev.player, "animals:milk", 1) == 1 then
        core.give_item(ev.player, "lakes:bucket", 1)
        core.heal(ev.player, 4)
    end
end)

local TROUGH = core.register_node("animals:trough", 150, 110, 70, true, 0, "upright", 0, 1.5, "wood")
local FILLED = core.register_node("animals:trough_full", 170, 150, 70, true, 0, "upright", 0, 1.5, "wood")
for _, id in { TROUGH, FILLED } do
    core.node_box(id, 0, 0, 0.1, 1, 0.1, 0.9)
    core.node_box(id, 0, 0.1, 0.1, 1, 0.55, 0.2)
    core.node_box(id, 0, 0.1, 0.8, 1, 0.55, 0.9)
    core.node_box(id, 0, 0.1, 0.2, 0.1, 0.55, 0.8)
    core.node_box(id, 0.9, 0.1, 0.2, 1, 0.55, 0.8)
end
core.node_box(FILLED, 0.1, 0.1, 0.2, 0.9, 0.45, 0.8)
core.node_drop(FILLED, TROUGH)
core.register_model("animals:trough_held", "trough_held.gltf")   -- tools/make_held_models.py
core.recipe_shaped("animals:trough", 1,
    "timber:planks farm:wheat    timber:planks",
    "timber:planks timber:planks timber:planks")

-- The troughs, by "x,y,z", in storage so a stable outlives a restart.
local troughs: { [string]: boolean } = {}
for k in string.gmatch(core.storage_get("troughs"), "[^;]+") do
    troughs[k] = true
end
local function save_troughs()
    local list = {}
    for k in troughs do
        table.insert(list, k)
    end
    core.storage_set("troughs", table.concat(list, ";"))
end

core.event.on("core:placed", function(ev)
    if ev.id == TROUGH then
        troughs[`{ev.x},{ev.y},{ev.z}`] = true
        save_troughs()
    end
end)
core.event.on("core:broken", function(ev)
    if ev.id == TROUGH or ev.id == FILLED then
        troughs[`{ev.x},{ev.y},{ev.z}`] = nil
        save_troughs()
    end
end)
core.event.on("core:used", function(ev)
    if ev.id ~= TROUGH and ev.id ~= FILLED then
        return
    end
    ev.cancel = true
    local food, n = string.match(core.meta_get(ev.x, ev.y, ev.z, "food"), "^(%S+) (%d+)$")
    local have = tonumber(n) or 0
    if ev.held ~= "farm:wheat" and ev.held ~= "animals:seeds" then
        core.notice(ev.player, if have > 0 then `The trough holds {have} {food == "farm:wheat" and "wheat" or "seeds"}`
            else "Fill it with wheat or seeds")
        return
    end
    if have > 0 and food ~= ev.held then
        core.notice(ev.player, "It holds something else")
        return
    end
    local put = core.take_item(ev.player, ev.held, math.min(16 - have, 64))
    if put > 0 then
        core.set_block(ev.x, ev.y, ev.z, FILLED)
        core.meta_set(ev.x, ev.y, ev.z, "food", `{ev.held} {have + put}`)
        troughs[`{ev.x},{ev.y},{ev.z}`] = true
        save_troughs()
    end
end)

local since = 0
core.event.on("core:tick", function(ev)
    clock += ev.dt
    since += ev.dt
    if since < 10 then
        return
    end
    since = 0
    for key in troughs do
        local x, y, z = string.match(key, "^(%-?%d+),(%-?%d+),(%-?%d+)$")
        local tx, ty, tz = tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number
        if core.loaded(tx, ty, tz) == 1 then
            local id = core.get_block(tx, ty, tz)
            if id ~= TROUGH and id ~= FILLED then
                troughs[key] = nil
                save_troughs()
            else
                local food, n = string.match(core.meta_get(tx, ty, tz, "food"), "^(%S+) (%d+)$")
                local have = tonumber(n) or 0
                local by_kind: { [string]: { number } } = {}
                for _, m in core.mob_ids(tx + 0.5, ty, tz + 0.5, 8, true) do
                    local kind = core.mob_kind(m)
                    if food_of(kind) and (let_go[m] or 0) <= clock then
                        core.mob_keep(m, true)
                        by_kind[kind] = by_kind[kind] or {}
                        table.insert(by_kind[kind], m)
                    end
                end
                for kind, ids in by_kind do
                    if have >= 2 and food_of(kind) == food and #ids < 8 then
                        local loved = 0
                        for _, m in ids do
                            if loved < 2 and core.mob_love(m) == 1 then
                                loved += 1
                            end
                        end
                        if loved > 0 then
                            have -= loved
                        end
                    end
                end
                if have <= 0 then
                    core.meta_set(tx, ty, tz, "food", "")
                    core.set_block(tx, ty, tz, TROUGH)
                else
                    core.meta_set(tx, ty, tz, "food", `{food} {have}`)
                end
            end
        end
    end
end)

-- ------------------------------------------------------------ the pony
-- A mount. Ponies graze on grass by day as the others do; right click one
-- holding a saddle and you ride it: the pony is worn under your feet
-- (core.player_wear: its own bones under your feet' bone, its legs playing
-- worn_walk or worn_run as you go -- tools/make_pony_ridden.py) and you sit
-- astride it, feet in the stirrups (ride_idle, ride: tools/make_ride_clips.py) and you go at its pace, a canter on sprint, and jump two
-- blocks. It is ridden, not walked (core.body_mount): it goes where you look,
-- A and D turn it, it backs up slowly and takes a moment to get going and to
-- stop. Ctrl (crouch), or a right click on the ground, gets off: it
-- stands there again and the saddle is back in the bag. Dying, or leaving, puts it down where you were.
local PONY = {
    MODEL = core.register_model("animals:pony", "pony.gltf"),   -- tools/make_pony_model.py
    RIDDEN = core.register_model("animals:pony_ridden", "pony_ridden.gltf"),   -- tools/make_pony_ridden.py
    SLOT = 9,                 -- of core.player_wear's ten (mods/boats has the 10th)
    -- Its pace: walk, canter, no flying, and a jump at 11 m/s against the
    -- world's 25 m/s^2 -- 2.4 up, over a wall two blocks high.
    WALK = 7.0, CANTER = 11.0, JUMP = 11.0,
    riding = {} :: { [number]: { x: number, y: number, z: number, clip: string } },
}
core.register_mob("animals:pony", PONY.MODEL, 16, 2.6, "pet", 0, 1.6, 1.0, "animals:leather", 1, 0, false, 0,
                  false, "core:grass")
core.register_item("animals:saddle", 1, "", 0, 1.0, "")
core.register_model("animals:saddle_held", "saddle_held.gltf")   -- tools/make_held_models.py
core.recipe_shaped("animals:saddle", 1,
    "animals:leather  animals:leather     animals:leather",
    "-                furnace:iron_ingot  -")

function PONY.off(p: number, x: number?, y: number?, z: number?)
    local was = PONY.riding[p]
    if was == nil then
        return
    end
    PONY.riding[p] = nil
    local px, py, pz = x or was.x, y or was.y, z or was.z
    core.spawn_mob("animals:pony", px, py, pz)
    if core.player_pos(p) ~= nil then
        core.player_wear(p, PONY.SLOT, 0)
        core.player_clip(p, "")
        core.body_mount(p, false)
        core.body_movement(p)
        if core.give_item(p, "animals:saddle", 1) < 1 then
            core.drop_item(math.floor(px), math.floor(py), math.floor(pz), core.item_id("animals:saddle"), 1)
        end
    else
        core.drop_item(math.floor(px), math.floor(py), math.floor(pz), core.item_id("animals:saddle"), 1)
    end
end

core.event.on("core:used_mob", function(ev)
    if core.mob_kind(ev.target) ~= "animals:pony" or ev.held ~= "animals:saddle" or PONY.riding[ev.player] then
        return
    end
    local x, y, z = core.mob_pos(ev.target)
    if x == nil or core.take_item(ev.player, "animals:saddle", 1) < 1 then
        return
    end
    core.mob_remove(ev.target)
    PONY.riding[ev.player] = { x = x, y = y, z = z, clip = "ride_idle" }
    core.player_clip(ev.player, "ride_idle")   -- astride (tools/make_ride_clips.py)
    core.player_place(ev.player, x, y, z)
    core.player_wear(ev.player, PONY.SLOT, PONY.RIDDEN)
    core.body_movement(ev.player, PONY.WALK, PONY.CANTER, 0, PONY.JUMP)
    core.body_mount(ev.player, true)
    core.notice(ev.player, "Riding: Ctrl, or right click the ground, to get off")
end)

-- Off onto the ground clicked, beside it.
core.event.on("core:used", -20, function(ev)
    if PONY.riding[ev.player] and core.get_block(ev.x, ev.y + 1, ev.z) == 0 then
        ev.cancel = true
        local x, y, z = core.player_pos(ev.player)
        PONY.off(ev.player, ev.x + 0.5, ev.y + 1, ev.z + 0.5)
        if x then
            core.player_place(ev.player, x, y, z)
        end
    end
end)

core.event.on("core:died", function(ev)
    PONY.off(ev.player, ev.x, ev.y, ev.z)
end)

local pony_since = 0
core.event.on("core:tick", function(ev)
    -- Ctrl (crouch) gets off where you are, the pony beside you.
    for p in PONY.riding do
        if core.player_crouching(p) then
            local x, y, z = core.player_pos(p)
            if x then
                local lx, _, lz = core.player_look(p)
                local len = math.max(0.01, math.sqrt((lx or 0) ^ 2 + (lz or 0) ^ 2))
                PONY.off(p, x - (lz or 0) / len * 1.2, y, z + (lx or 0) / len * 1.2)
            else
                PONY.off(p)
            end
        end
    end
    pony_since += ev.dt
    if pony_since < 0.5 then
        return
    end
    pony_since = 0
    local here: { [number]: boolean } = {}
    for _, p in core.players() do
        here[p] = true
        local r = PONY.riding[p]
        if r then
            local x, y, z = core.player_pos(p)
            if x then
                -- Under way or standing: the rider's clip as the pony's legs go.
                local clip = if (x - r.x) ^ 2 + (z - r.z) ^ 2 > 0.3 * 0.3 then "ride" else "ride_idle"
                if clip ~= r.clip then
                    r.clip = clip
                    core.player_clip(p, clip)
                end
                r.x, r.y, r.z = x, y, z
            end
        end
    end
    for p in PONY.riding do
        if not here[p] then
            PONY.off(p)   -- left riding: the pony stays where they were
        end
    end
end)
