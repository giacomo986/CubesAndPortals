--!strict
-- pets: a dog and a cat, in the manner of the classic block game's wolf and cat. Made in
-- assets/blender/pets.blend by tools/make_pet_models.py, each twice: wild, and
-- tame with a red collar.
--
-- They come out by day on sunlit grass, up to two near each player, stroll
-- about and fight nobody. Right click one holding what it wants -- a bone for
-- the dog (mods/mobs), a fish for the cat -- and one is spent and
-- it is yours: it puts its collar on, follows you, is put beside you when you
-- get more than 14 nodes away, and never goes. Right click it again and it
-- sits (its "sit" clip) and stays; again, and it follows.
--
-- What they do is the engine's (core/ecs/mob.h, fight "pet"). The clips it
-- plays by name: idle, walk, run, sit.
local DOG      = core.register_model("pets:dog", "dog.gltf")
local DOG_TAME = core.register_model("pets:dog_tame", "dog_tame.gltf")
local CAT      = core.register_model("pets:cat", "cat.gltf")
local CAT_TAME = core.register_model("pets:cat_tame", "cat_tame.gltf")

-- Quick enough to keep up with a walking player (4.8) when far behind: a pet
-- runs at 1.6 times its speed more than five nodes off.
core.register_mob("pets:dog", DOG, 16, 3.2, "pet", 0, 1.6, 1.0, "mobs:bone", 1, 0, false, 0, false,
                  "core:grass")
core.register_mob("pets:cat", CAT, 10, 3.4, "pet", 0, 1.6, 1.0, "", 1, 0, false, 0, false,
                  "core:grass")

core.mob_tame("pets:dog", "mobs:bone", DOG_TAME)
core.mob_tame("pets:cat", "pets:fish", CAT_TAME)

-- The cat's fish, caught with a rod: three sticks corner to corner.
--
-- A right click on water with it -- the first water along the look, as far as
-- a hand reaches -- casts: a float (a block that stops nothing, drawn by
-- bobber.gltf) sits on the water over it. Three to ten seconds on, a fish
-- bites: the float is pulled under (bobber_bite.gltf) and "A bite!" is said,
-- and a right click in the next 1.2 s brings the fish up. Too late and the
-- float comes up again for the next one; a click before any bite reels in
-- empty. The float goes when the line is reeled in, the rod leaves the hand,
-- the player goes more than 16 nodes off or leaves, or anything else takes
-- its cell -- and a world
-- opened again clears any the stop left.
local ROD  = "pets:rod"
local FISH = "pets:fish"
core.register_item(ROD, 1, "", 0, 1.0, "")
core.register_item(FISH, 64, "", 0, 1.0, "")
-- Cooked in a furnace (mods/furnace), or by the inhabitants (mods/villagers).
core.register_item("pets:cooked_fish", 64, "", 0, 1.0, "")

-- What bites depends on the water the float is on (core.biome of its column):
-- perch ("pets:fish") and carp in a lake, trout in a river, cod and eels in
-- the sea, tuna out on the ocean, and anywhere, now and then, a goldfish.
-- Every one cooks to a cooked fish; the toad fishers buy them all.
local KINDS = { "carp", "trout", "cod", "eel", "tuna", "goldfish" }
for _, k in KINDS do
    core.register_item("pets:" .. k, 64, "", 0, 1.0, "")
    core.register_model(`pets:{k}_held`, `{k}_held.gltf`)   -- tools/make_fish_models.py
end
local WATERS: { [string]: { { any } } } = {   -- kind, weight
    lake  = { { "fish", 70 }, { "carp", 30 } },
    river = { { "trout", 70 }, { "fish", 30 } },
    sea   = { { "cod", 70 }, { "eel", 30 } },
    ocean = { { "tuna", 50 }, { "cod", 50 } },
}
local WATER_OF = { ["core:lake"] = "lake", ["core:river"] = "river", ["core:gorge"] = "river",
                   ["core:sea"] = "sea", ["core:beach"] = "sea", ["core:ocean"] = "ocean" }
local function catch(x: number, z: number): string
    if math.random() < 0.03 then
        return "goldfish"
    end
    local list = WATERS[WATER_OF[core.biome(x, z)] or "lake"]
    local r = math.random() * 100
    for _, k in list do
        r -= k[2]
        if r <= 0 then
            return k[1]
        end
    end
    return list[1][1]
end
core.register_model("pets:cooked_fish_held", "fish_held.gltf")
core.recipe_shaped(ROD, 1,
    "-             -             timber:stick",
    "-             timber:stick  -",
    "timber:stick  -             -")

local FLOAT = core.register_node("pets:bobber", 220, 40, 30, false)
local BITE  = core.register_node("pets:bobber_bite", 220, 40, 30, false)
core.node_model(FLOAT, core.register_model("pets:bobber", "bobber.gltf"))
core.node_model(BITE, core.register_model("pets:bobber_bite", "bobber_bite.gltf"))
core.register_lbm("pets:no_floats", { "pets:bobber", "pets:bobber_bite" }, function(cell)
    core.set_block(cell.x, cell.y, cell.z, 0)
end)

local REACH, STEP, LINE = 6.0, 0.2, 16.0
local BITE_FOR = 1.2

-- mods/lakes's water, looked up at the first cast: it may load after this.
local WATER: { [number]: boolean }? = nil
local function water(): { [number]: boolean }
    local w = WATER
    if w == nil then
        w = {}
        for _, name in { "lakes:water", "lakes:flow1", "lakes:flow2", "lakes:flow3",
                         "lakes:flow4", "lakes:flow5", "lakes:flow6" } do
            local id = core.node_id(name)
            if id ~= 0 then
                w[id] = true
            end
        end
        WATER = w
    end
    return w
end

-- The first water cell along a player's look, if nothing comes before it.
local function water_looked_at(player: number): (number?, number?, number?)
    local wet = water()
    local px, py, pz = core.player_pos(player)
    local lx, ly, lz = core.player_look(player)
    if px == nil or py == nil or pz == nil or lx == nil or ly == nil or lz == nil then
        return nil, nil, nil
    end
    local t = 0.0
    while t <= REACH do
        local x, y, z = math.floor(px + lx * t), math.floor(py + 1.62 + ly * t), math.floor(pz + lz * t)
        local id = core.get_block(x, y, z)
        if wet[id] then
            return x, y, z
        elseif id ~= 0 then
            return nil, nil, nil
        end
        t += STEP
    end
    return nil, nil, nil
end

type Cast = { x: number, y: number, z: number, bite_at: number, bite_until: number }
local casts: { [number]: Cast } = {}
local clock = 0.0

local function next_bite(): number
    return clock + 3 + math.random() * 7
end

local function reel_in(player: number)
    local c = casts[player]
    if c == nil then
        return
    end
    casts[player] = nil
    local id = core.get_block(c.x, c.y, c.z)
    if id == FLOAT or id == BITE then
        core.set_block(c.x, c.y, c.z, 0)
    end
end

core.event.on("core:tick", function(ev)
    clock += ev.dt
    local here = {}
    for _, p in core.players() do
        here[p] = true
    end
    for p, c in casts do
        local x, y, z = core.player_pos(p)
        local id = core.get_block(c.x, c.y, c.z)
        if not here[p] or x == nil or y == nil or z == nil or (id ~= FLOAT and id ~= BITE) or
            core.held_item(p) ~= ROD or (x - c.x) ^ 2 + (y - c.y) ^ 2 + (z - c.z) ^ 2 > LINE * LINE then
            reel_in(p)
        elseif c.bite_until > 0 and clock >= c.bite_until then
            c.bite_until, c.bite_at = 0, next_bite()   -- it got away: afloat again
            core.set_block(c.x, c.y, c.z, FLOAT)
        elseif c.bite_until == 0 and clock >= c.bite_at then
            c.bite_until = clock + BITE_FOR
            core.set_block(c.x, c.y, c.z, BITE)
            core.notice(p, "A bite!")
        end
    end
end)

core.event.on("core:used", function(ev)
    if ev.held ~= ROD then
        return
    end
    ev.cancel = true   -- a rod builds nothing
    local c = casts[ev.player]
    if c ~= nil then
        local biting = c.bite_until > 0 and clock < c.bite_until
        reel_in(ev.player)
        if biting then
            local kind = catch(c.x, c.z)
            if core.give_item(ev.player, "pets:" .. kind, 1) > 0 then
                core.notice(ev.player, if kind == "fish" then "A perch!" else `A {kind}!`)
            end
        end
        return
    end
    local x, y, z = water_looked_at(ev.player)
    if x == nil or y == nil or z == nil or core.get_block(x, y + 1, z) ~= 0 then
        return
    end
    core.set_block(x, y + 1, z, FLOAT)
    casts[ev.player] = { x = x, y = y + 1, z = z, bite_at = next_bite(), bite_until = 0 }
end)

-- What each item looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
for _, n in { "rod", "fish" } do
    core.register_model(`pets:{n}_held`, `{n}_held.gltf`)
end
