--!strict
-- bed: The classic block game's bed, two cells long.
--
-- Three wool over three planks. Put down, it is the foot where it was put and
-- the head in the next cell away from whoever put it, or nothing if that cell
-- is taken; broken by either end, both go and it gives one bed. Right clicked,
-- it is where its sleeper comes back when they die (core.player_spawn). At
-- night -- dusk to dawn -- the sleeper is laid on it on their back, head on the
-- pillow (core.player_lie), and lies there until they walk off it or the night
-- ends; when everybody in the world is lying in a bed the clock goes to dawn. A
-- creature that fights within 8 nodes keeps anybody from lying down, as in
-- The classic block game.
--
-- The foot wears the whole bed (bed.gltf, tools/make_bed_gltf.py), as a door's
-- bottom wears the whole door; the head wears a triangle nobody sees and is
-- there to be a cell a body stands on and a hand breaks.
--
-- The spot is kept in the mod's storage by player name and given back the
-- first time the player is seen after a restart, if the bed is still there. A
-- bed broken sends whoever slept in it back to the world's spawn.
local TALL = 9 / 16

local function half(name: string, model: string): number
    local id = core.register_node(name, 170, 40, 40, true, 0, "upright", 0, 0.4, "wood")
    core.node_model(id, core.register_model(name, model))
    core.node_box(id, 0, 0, 0, 1, TALL, 1)
    return id
end

local BED  = half("bed:bed", "bed.gltf")        -- the foot, and the item
local HEAD = half("bed:bed_head", "bed_half.gltf")
core.node_drop(HEAD, BED)

core.recipe_shaped("bed:bed", 1,
    "animals:wool   animals:wool   animals:wool",
    "timber:planks  timber:planks  timber:planks")

local DAWN = 0.25

-- The way from the foot to the head for a foot turned `facing`: its back.
local BACK = { [0] = { 0, -1 }, [1] = { -1, 0 }, [2] = { 0, 1 }, [3] = { 1, 0 } }

-- A cell of a bed: its foot, and the way to the head. nil if it is not a bed,
-- or half of one.
local function foot_of(x: number, y: number, z: number): (number?, number?, number?, number?, number?)
    local id = core.get_block(x, y, z)
    if id == BED then
        local b = BACK[core.get_facing(x, y, z)]
        return x, y, z, b[1], b[2]
    elseif id == HEAD then
        local b = BACK[core.get_facing(x, y, z)]
        if core.get_block(x - b[1], y, z - b[2]) == BED then
            return x - b[1], y, z - b[2], b[1], b[2]
        end
    end
    return nil, nil, nil, nil, nil
end

local function key(player: number): string
    return "bed:" .. core.player_name(player)
end

local function spot(s: string): (number?, number?, number?)
    local x, y, z = string.match(s, "^(-?%d+) (-?%d+) (-?%d+)$")
    if x == nil then
        return nil, nil, nil
    end
    return tonumber(x), tonumber(y), tonumber(z)
end

local function night(): boolean
    local t = core.time_of_day()
    return t > 0.77 or t < 0.23
end

-- Put down: the head behind the foot, or the bed back in the hand's stead.
core.event.on("core:placed", function(ev)
    if ev.id ~= BED then
        return
    end
    local facing = core.get_facing(ev.x, ev.y, ev.z)
    local b = BACK[facing]
    local hx, hz = ev.x + b[1], ev.z + b[2]
    if core.get_block(hx, ev.y, hz) ~= 0 then
        core.set_block(ev.x, ev.y, ev.z, 0)
        core.drop_item(ev.x, ev.y, ev.z, core.item_id("bed:bed"), 1)
        core.notice(ev.player, "No room for the head of the bed")
        return
    end
    core.set_block(hx, ev.y, hz, HEAD)
    core.set_facing(hx, ev.y, hz, facing)
end)

-- Who is lying in a bed now: player -> the middle of the bed and how long ago
-- they lay down. The first second does not count their position: the move onto
-- the bed reaches the server's idea of where they are a few ticks later.
local lying: { [number]: { number } } = {}

local function get_up(p: number)
    lying[p] = nil
    core.player_lie(p, false)
end

-- Where a player comes back, for whoever else wants to know (mods/maps' compass):
-- told when a bed sets it, when it is given back on joining, and with `on` false
-- when the bed is gone.
core.event.define("bed:home", {
    fields = { player = "number", x = "number", y = "number", z = "number", on = "boolean" },
    cancellable = false,
})

core.event.on("core:used", function(ev)
    local fx, fy, fz, bx, bz = foot_of(ev.x, ev.y, ev.z)
    if fx == nil or fy == nil or fz == nil or bx == nil or bz == nil then
        return
    end
    ev.cancel = true
    core.player_spawn(ev.player, fx + 0.5, fy + 1, fz + 0.5)
    core.storage_set(key(ev.player), `{fx} {fy} {fz}`)
    core.event.emit("bed:home", { player = ev.player, x = fx, y = fy, z = fz, on = true })

    -- The middle of the two cells, on the mattress.
    local mx, my, mz = fx + 0.5 + bx * 0.5, fy + TALL, fz + 0.5 + bz * 0.5
    if not night() then
        core.notice(ev.player, "Spawn point set. You can sleep only at night")
    elseif core.monsters_near(mx, fy, mz, 8) > 0 then
        core.notice(ev.player, "You may not rest now; there are monsters nearby")
    else
        core.player_lie(ev.player, true, mx, my, mz, bx, bz)
        lying[ev.player] = { mx, my, mz, 0 }
        core.notice(ev.player, "Sleeping")
    end
end)

-- Broken by either end: the other goes too, giving nothing -- the one broken
-- gave the bed -- and whoever slept in it has lost their spot.
core.event.on("core:pre_break", function(ev)
    local fx, fy, fz, bx, bz = foot_of(ev.x, ev.y, ev.z)
    if fx == nil or fy == nil or fz == nil or bx == nil or bz == nil then
        return
    end
    if ev.id == BED then
        core.set_block(fx + bx, fy, fz + bz, 0)
    else
        core.set_block(fx, fy, fz, 0)
    end
    for _, p in core.players() do
        local x, y, z = spot(core.storage_get(key(p)))
        if x == fx and y == fy and z == fz then
            core.player_spawn(p, 0, 0, 0, false)
            core.storage_set(key(p), "")
            core.event.emit("bed:home", { player = p, x = 0, y = 0, z = 0, on = false })
            core.notice(p, "Your bed is gone")
        end
        local l = lying[p]
        if l and math.abs(l[1] - (fx + 0.5 + bx * 0.5)) < 0.1 and math.abs(l[3] - (fz + 0.5 + bz * 0.5)) < 0.1 then
            get_up(p)
        end
    end
end)

-- Up when they walk off the bed, when it is day, or when they leave; and when
-- everybody left in the world has lain down for three seconds, it is dawn.
local all_asleep_for = 0
local function wake_and_count(dt: number)
    local present: { [number]: boolean } = {}
    local all, asleep = 0, 0
    for _, p in core.players() do
        present[p] = true
        all += 1
        local bed = lying[p]
        if bed then
            bed[4] += dt
            local x, y, z = core.player_pos(p)
            local off = not x or math.abs(x - bed[1]) > 0.6 or math.abs(z - bed[3]) > 0.6
                or math.abs(y - bed[2]) > 0.6
            if not night() or (off and bed[4] > 1) then
                get_up(p)
            else
                asleep += 1
            end
        end
    end
    for p in lying do
        if not present[p] then
            lying[p] = nil
        end
    end
    all_asleep_for = if all > 0 and asleep == all then all_asleep_for + dt else 0
    if all_asleep_for >= 3 then
        all_asleep_for = 0
        core.set_time(DAWN + 0.01)
        core.notice(0, "Everybody slept; good morning")
        for p in lying do
            get_up(p)
        end
    end
end

-- Given back once per connection: a player id is new each time they join.
local seen: { [number]: boolean } = {}
core.event.on("core:tick", function(ev)
    wake_and_count(ev.dt)
    for _, p in core.players() do
        if not seen[p] then
            seen[p] = true
            local x, y, z = spot(core.storage_get(key(p)))
            if x and y and z then
                -- A column not loaded yet may well hold the bed.
                if core.loaded(x, y, z) == 0 or core.get_block(x, y, z) == BED then
                    core.player_spawn(p, x + 0.5, y + 1, z + 0.5)
                    core.event.emit("bed:home", { player = p, x = x, y = y, z = z, on = true })
                else
                    core.storage_set(key(p), "")
                end
            end
        end
    end
end)
