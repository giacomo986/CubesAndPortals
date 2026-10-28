--!strict
-- cinnabar: dust, a lever, a button, a torch, a repeater, a comparator, a
-- lamp, a piston and a sticky piston, and the cinnabar block.
--
-- Cinnabar (the item mods/ore's ore drops) right clicked on the top of a block
-- is laid there as dust. A lever -- a stick over a stone -- turns on and off; a
-- button -- a stone -- is on for a second. A cinnabar torch -- cinnabar over a
-- stick -- is on unless the block it stands on is powered. A repeater takes
-- power in from the side that faced whoever put it down and gives it out,
-- fifteen again, on the other, 0.1 to 0.4 s later (a right click steps it). A
-- comparator reads what is behind it -- power, or how full a chest or a
-- furnace is -- and gives it out if nothing at its sides is stronger, or less
-- what is at its sides (a right click switches). A lamp lights when powered. A
-- piston pushes up to twelve blocks away from its placer when powered and
-- takes its head back when not; a sticky one pulls the block in front back
-- with it.
--
-- Power, as in the classic block game but smaller:
--   * a lever or button that is on, a lit torch, a cinnabar block, a repeater
--     that is on and a comparator giving something out are sources; dust beside
--     one has what it gives (fifteen, or the comparator's level);
--   * dust carries it on, one less a cell, on the level or a step up or down;
--   * a solid block is STRONGLY powered under a lever or button that is on,
--     over a lit torch and in front of a repeater or comparator that is on, and
--     dust beside it has fifteen; it is WEAKLY powered under or beside powered
--     dust (The classic block game: only the block the dust points into), which lights a
--     lamp, turns a torch off and moves a piston but feeds no dust.
--
-- Dust and lamps follow at once; torches, repeaters, comparators and pistons
-- change a moment later, from a queue this mod keeps -- which is what lets a
-- torch and a repeater make a clock. A torch that changes more than eight
-- times in three seconds burns out for five, as the classic block game's does.
--
-- The engine knows nothing of any of this: every block placed or broken, and
-- every change from the queue, works the circuit round it out again, in Lua,
-- over a box sixteen cells each way (and again round whatever that changed at
-- the box's edge, as far as it goes). An observer watches the block it was put
-- against and pulses out of its other side when anything changes it -- a hand,
-- a mod, water running in. A hopper passes things down, or into the block it was put against. A
-- piston faces out of the face it was put against, any of six ways, and pushes
-- the players and creatures standing in its way along with the blocks.
local function node(name: string, r: number, g: number, b: number, solid: boolean, light: number,
                    turn: string, secs: number?): number
    return core.register_node(name, r, g, b, solid, light, turn, 0, secs or 0.0, "")
end

local DUST_OFF = node("cinnabar:dust", 120, 20, 16, false, 0, "")
local DUST_ON  = node("cinnabar:dust_on", 250, 40, 20, false, 0, "")
-- A flat model over the floor (tools/make_block_textures.py), and a box a
-- sixteenth tall to aim at it by.
for i, id in { DUST_OFF, DUST_ON } do
    local file = if i == 1 then "dust" else "dust_on"
    core.node_model(id, core.register_model(`cinnabar:{file}`, `{file}.gltf`))
    core.node_box(id, 0, 0, 0, 1, 1 / 16, 1)
    core.node_drop_item(id, "ore:cinnabar")
end

local LEVER    = node("cinnabar:lever", 120, 110, 100, true, 0, "upright", 0.3)
local LEVER_ON = node("cinnabar:lever_on", 120, 110, 100, true, 0, "upright", 0.3)
-- A stone base, and the handle leaning back off and forward on.
core.node_box(LEVER, 0.3, 0, 0.25, 0.7, 0.15, 0.75)
core.node_box(LEVER, 0.45, 0.15, 0.3, 0.55, 0.65, 0.4)
core.node_box(LEVER_ON, 0.3, 0, 0.25, 0.7, 0.15, 0.75)
core.node_box(LEVER_ON, 0.45, 0.15, 0.6, 0.55, 0.65, 0.7)
core.node_drop(LEVER_ON, LEVER)

local BUTTON    = node("cinnabar:button", 128, 128, 128, true, 0, "", 0.3)
local BUTTON_ON = node("cinnabar:button_on", 128, 128, 128, true, 0, "", 0.3)
core.node_box(BUTTON, 0.3, 0, 0.35, 0.7, 0.2, 0.65)
core.node_box(BUTTON_ON, 0.3, 0, 0.35, 0.7, 0.1, 0.65)
core.node_drop(BUTTON_ON, BUTTON)

-- Walked through, and aimed at by its box (a block that is not solid is).
local TORCH     = node("cinnabar:torch", 220, 40, 30, false, 7, "")
local TORCH_OFF = node("cinnabar:torch_off", 110, 30, 25, false, 0, "")
core.node_box(TORCH, 0.4375, 0, 0.4375, 0.5625, 0.625, 0.5625)
core.node_box(TORCH_OFF, 0.4375, 0, 0.4375, 0.5625, 0.625, 0.5625)
core.node_drop(TORCH_OFF, TORCH)

-- Slabs with a pin at the output end, turned with the cell: the front (the
-- side that faced whoever put it down) is the input.
local REPEATER      = node("cinnabar:repeater", 150, 150, 150, true, 0, "upright", 0.2)
local REPEATER_ON   = node("cinnabar:repeater_on", 150, 150, 150, true, 0, "upright", 0.2)
local COMPARATOR    = node("cinnabar:comparator", 140, 140, 150, true, 0, "upright", 0.2)
local COMPARATOR_ON = node("cinnabar:comparator_on", 160, 140, 150, true, 0, "upright", 0.2)
for _, id in { REPEATER, REPEATER_ON, COMPARATOR, COMPARATOR_ON } do
    core.node_box(id, 0, 0, 0, 1, 0.125, 1)
    core.node_box(id, 0.4375, 0.125, 0.125, 0.5625, 0.4, 0.25)
end
core.node_drop(REPEATER_ON, REPEATER)
core.node_drop(COMPARATOR_ON, COMPARATOR)

local LAMP    = node("cinnabar:lamp", 150, 100, 60, true, 0, "", 0.3)
local LAMP_ON = node("cinnabar:lamp_on", 250, 210, 140, true, 15, "", 0.3)
core.node_drop(LAMP_ON, LAMP)

-- A piston is a cube; extended, it is the back three quarters of its cell and
-- its head is the next cell's far quarter and a rod. Wallmounted: it faces out
-- of the face it was put against -- up off a floor, out of a wall towards
-- whoever put it there -- and pushes that way, as the classic block game's faces its placer.
local PISTON      = node("cinnabar:piston", 160, 130, 90, true, 0, "wallmounted", 1.0)
local PISTON_ON   = node("cinnabar:piston_on", 160, 130, 90, true, 0, "wallmounted", 1.0)
local STICKY      = node("cinnabar:sticky_piston", 130, 160, 90, true, 0, "wallmounted", 1.0)
local STICKY_ON   = node("cinnabar:sticky_piston_on", 130, 160, 90, true, 0, "wallmounted", 1.0)
local HEAD        = node("cinnabar:piston_head", 160, 130, 90, true, 0, "wallmounted", 1.0)
local STICKY_HEAD = node("cinnabar:sticky_piston_head", 130, 160, 90, true, 0, "wallmounted", 1.0)
-- Boxes with the front at +Z, which the cell's face turns: the push goes +Z.
core.node_box(PISTON_ON, 0, 0, 0, 1, 1, 0.75)
core.node_box(STICKY_ON, 0, 0, 0, 1, 1, 0.75)
for _, id in { HEAD, STICKY_HEAD } do
    core.node_box(id, 0, 0, 0.75, 1, 1, 1)
    core.node_box(id, 0.375, 0.375, -0.25, 0.625, 0.625, 0.75)
end

-- An observer watches the block it was put against and, when that changes,
-- gives a pulse out of its other side a tick later: wallmounted, its front the
-- side the pulse leaves by.
local OBSERVER    = node("cinnabar:observer", 100, 100, 110, true, 0, "wallmounted", 1.0)
local OBSERVER_ON = node("cinnabar:observer_on", 130, 100, 110, true, 0, "wallmounted", 1.0)
core.node_drop(OBSERVER_ON, OBSERVER)

-- A hopper: five slots, passing one thing on every 0.4 s from the container
-- above it (and what lies on it) into the one it points at -- the block it was
-- put against, so down off a floor and sideways into a wall. Never up: put
-- under a ceiling it points down. Locked while powered.
--
-- Two Blender models (assets/blender/hopper.blend): the bowl always open to the
-- sky, the spout down or out of one side. One model turned by the cell would
-- tip the bowl over for a side, so a side hopper is a block of its own, which
-- core:placed swaps in -- the same item, the same drop.
local HOPPER             = node("cinnabar:hopper", 80, 80, 86, true, 0, "wallmounted", 2.0)
local HOPPER_LOCKED      = node("cinnabar:hopper_locked", 70, 70, 76, true, 0, "wallmounted", 2.0)
local HOPPER_SIDE        = node("cinnabar:hopper_side", 80, 80, 86, true, 0, "wallmounted", 2.0)
local HOPPER_SIDE_LOCKED = node("cinnabar:hopper_side_locked", 70, 70, 76, true, 0, "wallmounted", 2.0)
local HOPPERS = { [HOPPER] = true, [HOPPER_LOCKED] = true, [HOPPER_SIDE] = true, [HOPPER_SIDE_LOCKED] = true }
do
    local down = core.register_model("cinnabar:hopper", "hopper.gltf")
    local side = core.register_model("cinnabar:hopper_side", "hopper_side.gltf")
    -- In the hand and on the ground (tools/make_held_models.py, which also
    -- draws its icon, assets/textures/cinnabar_hopper.png).
    core.register_model("cinnabar:hopper_held", "hopper_held.gltf")
    for id in HOPPERS do
        core.node_model(id, if id == HOPPER or id == HOPPER_LOCKED then down else side)
        if id ~= HOPPER then core.node_drop(id, HOPPER) end
    end
end
local HOPPER_SLOTS = 5
core.node_drop(PISTON_ON, PISTON)
core.node_drop(STICKY_ON, STICKY)
core.node_drop(HEAD, PISTON)
core.node_drop(STICKY_HEAD, STICKY)

local BLOCK = core.node_id("compact:cinnabar")

core.recipe_shaped("cinnabar:lever", 1,
    "timber:stick",
    "rocks:cobblestone")
core.recipe_shapeless("cinnabar:button", 1, "core:stone")
core.recipe_shaped("cinnabar:torch", 1,
    "ore:cinnabar",
    "timber:stick")
core.recipe_shaped("cinnabar:repeater", 1,
    "cinnabar:torch  ore:cinnabar  cinnabar:torch",
    "core:stone      core:stone    core:stone")
-- The classic block game's has inferno quartz in the middle; the overworld here has none.
core.recipe_shaped("cinnabar:comparator", 1,
    "-               cinnabar:torch  -",
    "cinnabar:torch  ore:cinnabar    cinnabar:torch",
    "core:stone      core:stone      core:stone")
core.recipe_shaped("cinnabar:lamp", 1,
    "-             ore:cinnabar  -",
    "ore:cinnabar  core:glass    ore:cinnabar",
    "-             ore:cinnabar  -")
core.recipe_shaped("cinnabar:piston", 1,
    "timber:planks  timber:planks       timber:planks",
    "rocks:cobblestone  furnace:iron_ingot  rocks:cobblestone",
    "rocks:cobblestone  ore:cinnabar        rocks:cobblestone")
-- A slimeball in the classic block game; there are no slimes here, and an egg is sticky.
core.recipe_shaped("cinnabar:sticky_piston", 1,
    "animals:egg",
    "cinnabar:piston")
-- The classic block game's has quartz; the overworld here has none, so a second cinnabar.
core.recipe_shaped("cinnabar:observer", 1,
    "rocks:cobblestone  rocks:cobblestone  rocks:cobblestone",
    "ore:cinnabar       ore:cinnabar       rocks:cobblestone",
    "rocks:cobblestone  rocks:cobblestone  rocks:cobblestone")
core.recipe_shaped("cinnabar:hopper", 1,
    "furnace:iron_ingot  -                   furnace:iron_ingot",
    "furnace:iron_ingot  chest:box           furnace:iron_ingot",
    "-                   furnace:iron_ingot  -")

local DUST = { [DUST_OFF] = true, [DUST_ON] = true }
local PISTONS = { [PISTON] = true, [PISTON_ON] = true, [STICKY] = true, [STICKY_ON] = true }
local PART = {
    [DUST_OFF] = true, [DUST_ON] = true, [LEVER] = true, [LEVER_ON] = true,
    [BUTTON] = true, [BUTTON_ON] = true, [TORCH] = true, [TORCH_OFF] = true,
    [REPEATER] = true, [REPEATER_ON] = true, [COMPARATOR] = true, [COMPARATOR_ON] = true,
    [LAMP] = true, [LAMP_ON] = true, [PISTON] = true, [PISTON_ON] = true, [STICKY] = true,
    [STICKY_ON] = true, [HEAD] = true, [STICKY_HEAD] = true, [OBSERVER] = true,
    [OBSERVER_ON] = true, [HOPPER] = true, [HOPPER_LOCKED] = true, [HOPPER_SIDE] = true,
    [HOPPER_SIDE_LOCKED] = true,
}
-- A wallmounted cell's front: +X, -X, +Y, -Y, +Z, -Z for faces 0..5.
local DIR = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }
local SIDES = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }
local ALL6 = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }
-- The front of an upright block after 0..3 quarter turns: +Z, +X, -Z, -X.
local FRONT = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }
local R = 16
local TICK = 0.1   -- The classic block game's cinnabar tick

local function k(x: number, y: number, z: number): string
    return `{x},{y},{z}`
end

local function front_of(x: number, y: number, z: number): (number, number)
    local f = FRONT[core.get_facing(x, y, z) % 4 + 1]
    return f[1], f[2]
end

local function dir_of(x: number, y: number, z: number): (number, number, number)
    local d = DIR[core.get_facing(x, y, z) % 6 + 1]
    return d[1], d[2], d[3]
end

-- An observer watches its cell through the engine (core.watch_cell): every
-- mod's write to it, this one's too, comes back as core:watched; a player's
-- placing and breaking come as core:placed and core:broken.
local changed: (number, number, number) -> () = function(_, _, _) end
local function put(x: number, y: number, z: number, id: number)
    if core.get_block(x, y, z) ~= id then
        core.set_block(x, y, z, id)
    end
end

local function watch_for(x: number, y: number, z: number, on: boolean)
    local d = DIR[core.get_facing(x, y, z) % 6 + 1]
    core.watch_cell(x - d[1], y - d[2], z - d[3], on)
end

-- A block power goes into: anything solid that is not one of these parts. A
-- lamp is (a lamp beside powered dust lights either way).
local function conducts(id: number): boolean
    if id == 0 or (PART[id] and id ~= LAMP and id ~= LAMP_ON) or id == BLOCK then
        return false
    end
    return core.node_solid(id) == 1
end

type Cell = { x: number, y: number, z: number }

local function found(cx: number, cy: number, cz: number, names: { string }): { Cell }
    local out = {}
    for _, name in names do
        local f = core.find_nodes(cx - R, cy - R, cz - R, cx + R, cy + R, cz + R, name)
        for i = 1, #f, 3 do
            table.insert(out, { x = f[i], y = f[i + 1], z = f[i + 2] })
        end
    end
    return out
end

-- ------------------------------------------------------------------ the queue
-- What a torch, repeater, comparator or piston is to become, and when.
type Change = { x: number, y: number, z: number, want: number, level: number, at: number }
local queue: { [string]: Change } = {}
local clock = 0
local burnt: { [string]: number } = {}          -- a torch out until then
local toggles: { [string]: { number } } = {}    -- when each torch last changed

local function schedule(c: Cell, want: number, level: number, delay: number)
    local at = k(c.x, c.y, c.z)
    local q = queue[at]
    if q and q.want == want and q.level == level then
        return
    end
    queue[at] = { x = c.x, y = c.y, z = c.z, want = want, level = level, at = clock + delay }
end

-- Comparators reading a container, and the container's revision when last read.
local watched: { [string]: { x: number, y: number, z: number, rev: number, bx: number, bz: number } } = {}

-- An observer beside a cell that changed, looking at it, pulses a tick later.
changed = function(x: number, y: number, z: number)
    for _, d in ALL6 do
        local ox, oy, oz = x + d[1], y + d[2], z + d[3]
        local id = core.get_block(ox, oy, oz)
        if id == OBSERVER then
            local fx, fy, fz = dir_of(ox, oy, oz)
            -- It watches the cell behind its front.
            if ox - fx == x and oy - fy == y and oz - fz == z then
                schedule({ x = ox, y = oy, z = oz }, OBSERVER_ON, 0, TICK)
            end
        end
    end
end

local function level_of(x: number, y: number, z: number): number
    return tonumber(core.meta_get(x, y, z, "level")) or 0
end

local function delay_of(x: number, y: number, z: number): number
    return tonumber(core.meta_get(x, y, z, "delay")) or 1
end

-- The whole circuit round (cx, cy, cz), from the world as it is. Dust and lamps
-- are set; the rest is queued. Returns the cells it changed near the edge of
-- its box, where the circuit may go on.
local function solve(cx: number, cy: number, cz: number): { Cell }
    local sources = found(cx, cy, cz, { "cinnabar:lever_on", "cinnabar:button_on", "compact:cinnabar" })
    local torches = found(cx, cy, cz, { "cinnabar:torch", "cinnabar:torch_off" })
    local repeaters = found(cx, cy, cz, { "cinnabar:repeater", "cinnabar:repeater_on" })
    local comparators = found(cx, cy, cz, { "cinnabar:comparator", "cinnabar:comparator_on" })
    local pistons = found(cx, cy, cz, { "cinnabar:piston", "cinnabar:piston_on",
                                        "cinnabar:sticky_piston", "cinnabar:sticky_piston_on" })
    local dust = found(cx, cy, cz, { "cinnabar:dust", "cinnabar:dust_on" })
    local lamps = found(cx, cy, cz, { "cinnabar:lamp", "cinnabar:lamp_on" })
    local observers = found(cx, cy, cz, { "cinnabar:observer_on" })
    local hoppers = found(cx, cy, cz, { "cinnabar:hopper", "cinnabar:hopper_locked",
                                    "cinnabar:hopper_side", "cinnabar:hopper_side_locked" })
    local rails = found(cx, cy, cz, { "building:powered_rail", "building:powered_rail_on", "building:switch",
                                      "building:switch_on" })
    if #dust + #torches + #repeaters + #comparators + #pistons + #lamps + #hoppers + #rails == 0 then
        return {}
    end

    local src: { [string]: number } = {}       -- a source cell and what it gives
    local strong: { [string]: boolean } = {}
    local weak: { [string]: boolean } = {}
    local fed: { [string]: number } = {}       -- a cell a repeater or comparator gives into
    local function strongly(x: number, y: number, z: number)
        if conducts(core.get_block(x, y, z)) then
            strong[k(x, y, z)] = true
        end
    end

    for _, s in sources do
        src[k(s.x, s.y, s.z)] = 15
        if core.get_block(s.x, s.y, s.z) ~= BLOCK then
            strongly(s.x, s.y - 1, s.z)
        end
    end
    for _, t in torches do
        if core.get_block(t.x, t.y, t.z) == TORCH then
            src[k(t.x, t.y, t.z)] = 15
            strongly(t.x, t.y + 1, t.z)
        end
    end
    local function gives(c: Cell, level: number)
        if level <= 0 then
            return
        end
        local fx, fz = front_of(c.x, c.y, c.z)
        local ox, oz = c.x - fx, c.z - fz
        fed[k(ox, c.y, oz)] = math.max(fed[k(ox, c.y, oz)] or 0, level)
        strongly(ox, c.y, oz)
    end
    for _, r in repeaters do
        if core.get_block(r.x, r.y, r.z) == REPEATER_ON then
            gives(r, 15)
        end
    end
    for _, c in comparators do
        if core.get_block(c.x, c.y, c.z) == COMPARATOR_ON then
            gives(c, level_of(c.x, c.y, c.z))
        end
    end
    -- An observer mid-pulse gives fifteen out of its front.
    for _, o in observers do
        local dx, dy, dz = dir_of(o.x, o.y, o.z)
        local at = k(o.x + dx, o.y + dy, o.z + dz)
        fed[at] = 15
        strongly(o.x + dx, o.y + dy, o.z + dz)
    end

    -- Dust: what the source beside it gives, fifteen beside a strongly powered
    -- block, or what a repeater or comparator gives into it; one less a cell.
    local power: { [string]: number } = {}
    local work: { { number } } = {}
    local function feed(x: number, y: number, z: number, p: number)
        if p <= 0 or not DUST[core.get_block(x, y, z)] then
            return
        end
        local at = k(x, y, z)
        if (power[at] or 0) >= p then
            return
        end
        power[at] = p
        table.insert(work, { x, y, z, p })
    end
    local function each(set: { [string]: any }, fn: (number, number, number, any) -> ())
        for at, v in set do
            local x, y, z = string.match(at, "^(-?%d+),(-?%d+),(-?%d+)$")
            fn(tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number, v)
        end
    end
    each(src, function(x, y, z, level)
        for _, d in ALL6 do
            feed(x + d[1], y + d[2], z + d[3], level)
        end
    end)
    each(strong, function(x, y, z, _)
        for _, d in ALL6 do
            feed(x + d[1], y + d[2], z + d[3], 15)
        end
    end)
    each(fed, function(x, y, z, level)
        feed(x, y, z, level)
    end)
    local head = 1
    while head <= #work do
        local q = work[head]
        head += 1
        for _, d in SIDES do
            for dy = -1, 1 do
                feed(q[1] + d[1], q[2] + dy, q[3] + d[3], q[4] - 1)
            end
        end
    end
    each(power, function(x, y, z, _)
        for _, d in { { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } } do
            if conducts(core.get_block(x + d[1], y + d[2], z + d[3])) then
                weak[k(x + d[1], y + d[2], z + d[3])] = true
            end
        end
    end)

    -- How much power a cell holds, for whatever reads it: a source's level,
    -- dust's, fifteen for a powered block, what a repeater gives into it.
    local function strength(x: number, y: number, z: number): number
        local at = k(x, y, z)
        return math.max(src[at] or 0, power[at] or 0, fed[at] or 0,
                        if strong[at] or weak[at] then 15 else 0)
    end

    local edge: { Cell } = {}
    local function set(c: Cell, want: number)
        if core.get_block(c.x, c.y, c.z) ~= want then
            put(c.x, c.y, c.z, want)
            if math.max(math.abs(c.x - cx), math.abs(c.y - cy), math.abs(c.z - cz)) >= R - 2 then
                table.insert(edge, c)
            end
        end
    end
    for _, c in dust do
        set(c, if power[k(c.x, c.y, c.z)] then DUST_ON else DUST_OFF)
    end
    for _, c in lamps do
        local lit = (fed[k(c.x, c.y, c.z)] or 0) > 0
        for _, d in ALL6 do
            if strength(c.x + d[1], c.y + d[2], c.z + d[3]) > 0 then
                lit = true
                break
            end
        end
        set(c, if lit then LAMP_ON else LAMP)
    end

    -- mods/building's powered rail is on, and its switch turned right, beside
    -- power -- as a lamp is lit. A switch's own way is in its meta from when it
    -- was laid. By name: building loads after this mod.
    local PRAIL, PRAIL_ON = core.node_id("building:powered_rail"), core.node_id("building:powered_rail_on")
    local SWITCH, SWITCH_ON = core.node_id("building:switch"), core.node_id("building:switch_on")
    for _, c in rails do
        local on = (fed[k(c.x, c.y, c.z)] or 0) > 0
        for _, d in ALL6 do
            if strength(c.x + d[1], c.y + d[2], c.z + d[3]) > 0 then
                on = true
                break
            end
        end
        local facing = core.get_facing(c.x, c.y, c.z)
        local id = core.get_block(c.x, c.y, c.z)
        if id == SWITCH or id == SWITCH_ON then
            -- Turned, it is drawn as the right-hand turn it now is.
            local base = tonumber(core.meta_get(c.x, c.y, c.z, "base")) or facing
            local want = if on then (base + 3) % 4 else base
            if facing ~= want or id ~= (if on then SWITCH_ON else SWITCH) then
                set(c, if on then SWITCH_ON else SWITCH)
                core.set_facing(c.x, c.y, c.z, want)
            end
        elseif id ~= (if on then PRAIL_ON else PRAIL) then
            set(c, if on then PRAIL_ON else PRAIL)
            core.set_facing(c.x, c.y, c.z, facing)
        end
    end

    -- A hopper beside power is locked, at once.
    for _, h in hoppers do
        local on = false
        for _, d in ALL6 do
            if strength(h.x + d[1], h.y + d[2], h.z + d[3]) > 0 then
                on = true
                break
            end
        end
        local facing = core.get_facing(h.x, h.y, h.z)
        local want = if facing == 2 then (if on then HOPPER_LOCKED else HOPPER)
            else (if on then HOPPER_SIDE_LOCKED else HOPPER_SIDE)
        if core.get_block(h.x, h.y, h.z) ~= want then
            put(h.x, h.y, h.z, want)
            core.set_facing(h.x, h.y, h.z, facing)
        end
    end

    -- The rest, a tick or a few from now.
    local function want(c: Cell, now: number, to: number, level: number, delay: number)
        if now ~= to or (level > 0 and level_of(c.x, c.y, c.z) ~= level) then
            schedule(c, to, level, delay)
        else
            queue[k(c.x, c.y, c.z)] = nil
        end
    end
    for _, t in torches do
        local off = strength(t.x, t.y - 1, t.z) > 0 or (burnt[k(t.x, t.y, t.z)] or 0) > clock
        want(t, core.get_block(t.x, t.y, t.z), if off then TORCH_OFF else TORCH, 0, TICK)
    end
    for _, r in repeaters do
        local fx, fz = front_of(r.x, r.y, r.z)
        local on = strength(r.x + fx, r.y, r.z + fz) > 0
        want(r, core.get_block(r.x, r.y, r.z), if on then REPEATER_ON else REPEATER, 0,
             TICK * delay_of(r.x, r.y, r.z))
    end
    for _, c in comparators do
        local fx, fz = front_of(c.x, c.y, c.z)
        local bx, bz = c.x + fx, c.z + fz
        local fill = core.container_fill(bx, c.y, bz)
        -- Watched from here on: a chest filled by hand changes nothing round
        -- it, so the tick asks whether what is in it did.
        if fill >= 0 then
            watched[k(c.x, c.y, c.z)] = { x = c.x, y = c.y, z = c.z, rev = core.container_revision(bx, c.y, bz),
                                           bx = bx, bz = bz }
        end
        local rear = if fill > 0 then 1 + math.floor(fill * 14) else strength(bx, c.y, bz)
        -- The two sides, across the way it points.
        local side = math.max(strength(c.x + fz, c.y, c.z + fx), strength(c.x - fz, c.y, c.z - fx))
        local level = if core.meta_get(c.x, c.y, c.z, "mode") == "subtract"
            then math.max(0, rear - side) else (if rear >= side then rear else 0)
        local now = core.get_block(c.x, c.y, c.z)
        if level == 0 and now == COMPARATOR then
            queue[k(c.x, c.y, c.z)] = nil
        else
            want(c, now, if level > 0 then COMPARATOR_ON else COMPARATOR, level, TICK)
        end
    end
    for _, p in pistons do
        local px, py, pz = dir_of(p.x, p.y, p.z)
        local on = false
        for _, d in ALL6 do
            -- Not from the cell it pushes into: that is where its head goes.
            if not (d[1] == px and d[2] == py and d[3] == pz)
                and strength(p.x + d[1], p.y + d[2], p.z + d[3]) > 0 then
                on = true
                break
            end
        end
        local id = core.get_block(p.x, p.y, p.z)
        local sticky = id == STICKY or id == STICKY_ON
        want(p, id, if on then (if sticky then STICKY_ON else PISTON_ON)
                    else (if sticky then STICKY else PISTON), 0, TICK)
    end
    return edge
end

-- And on from the edge of each box that changed something there, for as long
-- as something changes at an edge -- each box centre worked out once a call,
-- so a line that folds back does not go round for ever. 256 boxes is the
-- ceiling: a circuit that long says so in the log rather than stall the tick.
local MAX_BOXES = 256
local function update(x: number, y: number, z: number)
    local todo: { Cell } = { { x = x, y = y, z = z } }
    local done: { [string]: boolean } = {}
    local head = 1
    while head <= #todo do
        if head > MAX_BOXES then
            core.log(`cinnabar: a circuit wider than {MAX_BOXES} boxes round {x} {y} {z}, left part done`)
            return
        end
        local c = todo[head]
        head += 1
        local at = k(c.x // 4, c.y // 4, c.z // 4)
        if not done[at] then
            done[at] = true
            for _, e in solve(c.x, c.y, c.z) do
                table.insert(todo, e)
            end
        end
    end
end

-- ------------------------------------------------------------------ pistons
local IMMOVABLE = {
    [core.node_id("core:bedrock")] = true,
    [core.node_id("infernoportal:obsidian")] = true,
    [PISTON_ON] = true, [STICKY_ON] = true, [HEAD] = true, [STICKY_HEAD] = true,
}
IMMOVABLE[0] = nil
local MAX_PUSH = 12

-- A block that holds things (a chest, a furnace) stays where it is: what it
-- holds is kept by its cell.
local function movable(x: number, y: number, z: number, id: number): boolean
    return id ~= 0 and not IMMOVABLE[id] and core.container_fill(x, y, z) < 0
end

-- What is in the way and not a block to push: a crop, a torch, dust, water.
-- Broken, as what it drops.
local function crushable(id: number): boolean
    return id ~= 0 and not IMMOVABLE[id] and core.node_solid(id) == 0
end

local function drop_as_item(x: number, y: number, z: number, id: number)
    local item = core.item_id(core.node_name(id))
    if item ~= 0 then
        core.drop_item(x, y, z, item, 1)
    end
end

local function move_block(fx: number, fy: number, fz: number, tx: number, ty: number, tz: number)
    local id = core.get_block(fx, fy, fz)
    local facing = core.get_facing(fx, fy, fz)
    local observer = id == OBSERVER or id == OBSERVER_ON
    if observer then
        watch_for(fx, fy, fz, false)
    end
    put(tx, ty, tz, id)
    core.set_facing(tx, ty, tz, facing)
    put(fx, fy, fz, 0)
    if observer then
        watch_for(tx, ty, tz, true)
    end
end

-- Whoever stands in that cell -- a player, a creature -- goes one on with the
-- push. Players by where their feet are; a head in the cell is not moved.
-- A pushed player slides the node over four ticks rather than jumping it.
type Slide = { p: number, left: number, dx: number, dy: number, dz: number }
local slides: { Slide } = {}

local function push_bodies(x: number, y: number, z: number, dx: number, dy: number, dz: number)
    for _, p in core.players() do
        local px, py, pz = core.player_pos(p)
        if px and math.floor(px) == x and math.floor(py + 0.01) == y and math.floor(pz) == z then
            table.insert(slides, { p = p, left = 4, dx = dx / 4, dy = dy / 4, dz = dz / 4 })
        end
    end
    core.push_mobs(x, y, z, dx, dy, dz)
end

local function slide_players()
    local keep = {}
    for _, sl in slides do
        local px, py, pz = core.player_pos(sl.p)
        if px then
            local lx, ly, lz = core.player_look(sl.p)
            core.player_place(sl.p, px + sl.dx, py + sl.dy, pz + sl.dz, lx or 0, ly or 0, lz or 1)
            sl.left -= 1
            if sl.left > 0 then
                table.insert(keep, sl)
            end
        end
    end
    slides = keep
end

-- Pushes and puts its head out: true if it could.
local function extend(p: Cell, sticky: boolean): boolean
    local dx, dy, dz = dir_of(p.x, p.y, p.z)
    -- The line in front, to the first gap.
    local n = 0
    local x, y, z = p.x + dx, p.y + dy, p.z + dz
    while true do
        local id = core.get_block(x, y, z)
        if id == 0 then
            break
        end
        if crushable(id) then
            put(x, y, z, 0)
            drop_as_item(x, y, z, id)
            break
        end
        if not movable(x, y, z, id) or n >= MAX_PUSH then
            return false
        end
        n += 1
        x += dx
        y += dy
        z += dz
    end
    -- Far end first, bodies before the block that would land on them.
    for i = n, 1, -1 do
        local fx, fy, fz = p.x + dx * i, p.y + dy * i, p.z + dz * i
        push_bodies(fx + dx, fy + dy, fz + dz, dx, dy, dz)
        move_block(fx, fy, fz, fx + dx, fy + dy, fz + dz)
        push_bodies(fx, fy, fz, dx, dy, dz)
    end
    push_bodies(p.x + dx, p.y + dy, p.z + dz, dx, dy, dz)
    local facing = core.get_facing(p.x, p.y, p.z)
    put(p.x + dx, p.y + dy, p.z + dz, if sticky then STICKY_HEAD else HEAD)
    core.set_facing(p.x + dx, p.y + dy, p.z + dz, facing)
    return true
end

local function retract(p: Cell, sticky: boolean)
    local dx, dy, dz = dir_of(p.x, p.y, p.z)
    local hx, hy, hz = p.x + dx, p.y + dy, p.z + dz
    local here = core.get_block(hx, hy, hz)
    if here == HEAD or here == STICKY_HEAD then
        put(hx, hy, hz, 0)
    end
    if sticky then
        local id = core.get_block(hx + dx, hy + dy, hz + dz)
        if movable(hx + dx, hy + dy, hz + dz, id) and core.node_solid(id) == 1 and not PISTONS[id] then
            move_block(hx + dx, hy + dy, hz + dz, hx, hy, hz)
        end
    end
end

-- ------------------------------------------------------------------ the queue, run
local function apply(c: Change)
    local id = core.get_block(c.x, c.y, c.z)
    local at = k(c.x, c.y, c.z)
    local facing = core.get_facing(c.x, c.y, c.z)
    if id == TORCH or id == TORCH_OFF then
        -- Burning out: more than eight changes in three seconds.
        local kept = {}
        for _, t in toggles[at] or {} do
            if clock - t < 3 then
                table.insert(kept, t)
            end
        end
        table.insert(kept, clock)
        toggles[at] = kept
        if #kept > 8 and c.want == TORCH then
            burnt[at] = clock + 5
            toggles[at] = {}
            put(c.x, c.y, c.z, TORCH_OFF)
            queue[at] = { x = c.x, y = c.y, z = c.z, want = TORCH, level = 0, at = clock + 5.05 }
            return
        end
        put(c.x, c.y, c.z, c.want)
    elseif id == REPEATER or id == REPEATER_ON or id == COMPARATOR or id == COMPARATOR_ON then
        -- Its settings stay with the cell; the level is what the comparator gives.
        put(c.x, c.y, c.z, c.want)
        core.set_facing(c.x, c.y, c.z, facing)
        core.meta_set(c.x, c.y, c.z, "level", if c.level > 0 then tostring(c.level) else "")
    elseif id == OBSERVER or id == OBSERVER_ON then
        put(c.x, c.y, c.z, c.want)
        core.set_facing(c.x, c.y, c.z, facing)
        if c.want == OBSERVER_ON then
            -- Two ticks on, as the classic block game's pulse is.
            queue[at] = { x = c.x, y = c.y, z = c.z, want = OBSERVER, level = 0, at = clock + 2 * TICK }
        end
    elseif PISTONS[id] then
        local sticky = id == STICKY or id == STICKY_ON
        if c.want == PISTON_ON or c.want == STICKY_ON then
            if not extend(c, sticky) then
                return   -- blocked: tried again at the next change round it
            end
        else
            retract(c, sticky)
        end
        put(c.x, c.y, c.z, c.want)
        core.set_facing(c.x, c.y, c.z, facing)
    else
        return   -- not there any more
    end
    update(c.x, c.y, c.z)
end

local watch_clock = 0

-- A button that is on, and when it goes off again.
local pressed: { [string]: { x: number, y: number, z: number, at: number } } = {}

core.event.on("core:tick", function(ev)
    clock += ev.dt
    slide_players()
    for at, b in pressed do
        if clock >= b.at then
            pressed[at] = nil
            if core.get_block(b.x, b.y, b.z) == BUTTON_ON then
                put(b.x, b.y, b.z, BUTTON)
                update(b.x, b.y, b.z)
            end
        end
    end
    -- A comparator whose container changed without anything round it doing so.
    watch_clock += ev.dt
    if watch_clock >= 0.25 then
        watch_clock = 0
        for at, w in watched do
            local id = core.get_block(w.x, w.y, w.z)
            if id ~= COMPARATOR and id ~= COMPARATOR_ON then
                watched[at] = nil
            elseif core.container_revision(w.bx, w.y, w.bz) ~= w.rev then
                update(w.x, w.y, w.z)
            end
        end
    end
    -- What is due, in the order it fell due; each may queue more.
    local due: { Change } = {}
    for at, c in queue do
        if clock >= c.at then
            table.insert(due, c)
            queue[at] = nil
        end
    end
    table.sort(due, function(a, b)
        return a.at < b.at
    end)
    for _, c in due do
        apply(c)
    end
end)

-- ------------------------------------------------------------------ hands
core.event.on("core:used", function(ev)
    if ev.id == LEVER or ev.id == LEVER_ON then
        local facing = core.get_facing(ev.x, ev.y, ev.z)
        put(ev.x, ev.y, ev.z, if ev.id == LEVER then LEVER_ON else LEVER)
        core.set_facing(ev.x, ev.y, ev.z, facing)
        update(ev.x, ev.y, ev.z)
        ev.cancel = true
    elseif ev.id == BUTTON then
        put(ev.x, ev.y, ev.z, BUTTON_ON)
        pressed[k(ev.x, ev.y, ev.z)] = { x = ev.x, y = ev.y, z = ev.z, at = clock + 1.0 }
        update(ev.x, ev.y, ev.z)
        ev.cancel = true
    elseif ev.id == BUTTON_ON then
        ev.cancel = true
    elseif HOPPERS[ev.id] then
        if core.container_fill(ev.x, ev.y, ev.z) < 0 then
            core.container_make(ev.x, ev.y, ev.z, HOPPER_SLOTS)
        end
        core.container_show(ev.x, ev.y, ev.z, HOPPER_SLOTS, ev.player)
        ev.cancel = true
    elseif ev.id == REPEATER or ev.id == REPEATER_ON then
        local d = delay_of(ev.x, ev.y, ev.z) % 4 + 1
        core.meta_set(ev.x, ev.y, ev.z, "delay", tostring(d))
        core.notice(ev.player, `Delay {d}`)
        ev.cancel = true
    elseif ev.id == COMPARATOR or ev.id == COMPARATOR_ON then
        local sub = core.meta_get(ev.x, ev.y, ev.z, "mode") ~= "subtract"
        core.meta_set(ev.x, ev.y, ev.z, "mode", if sub then "subtract" else "")
        core.notice(ev.player, if sub then "Subtract" else "Compare")
        update(ev.x, ev.y, ev.z)
        ev.cancel = true
    elseif ev.held == "ore:cinnabar" and not DUST[ev.id]
        and core.get_block(ev.x, ev.y + 1, ev.z) == 0 then
        if core.take_item(ev.player, ev.held, 1) > 0 then
            put(ev.x, ev.y + 1, ev.z, DUST_OFF)
            update(ev.x, ev.y + 1, ev.z)
        end
        ev.cancel = true
    end
end)

-- Any block placed or broken may be one power goes through or stands on.
core.event.on("core:watched", function(ev)
    changed(ev.x, ev.y, ev.z)
end)

-- A loaded observer asks again: watches are not saved.
core.register_lbm("cinnabar:observers", { "cinnabar:observer", "cinnabar:observer_on" }, function(cell)
    watch_for(cell.x, cell.y, cell.z, true)
end)

core.event.on("core:placed", function(ev)
    if ev.id == OBSERVER then
        watch_for(ev.x, ev.y, ev.z, true)
    end
    if ev.id == HOPPER then
        -- Faces 0..5 are +X, -X, +Y, -Y, +Z, -Z, and the hopper passes on into
        -- the opposite way: 2 (a floor) is down, 3 (a ceiling) would be up.
        local facing = core.get_facing(ev.x, ev.y, ev.z)
        if facing == 3 then
            core.set_facing(ev.x, ev.y, ev.z, 2)
        elseif facing ~= 2 then
            put(ev.x, ev.y, ev.z, HOPPER_SIDE)
            core.set_facing(ev.x, ev.y, ev.z, facing)
        end
        core.container_make(ev.x, ev.y, ev.z, HOPPER_SLOTS)
    end
    changed(ev.x, ev.y, ev.z)
    update(ev.x, ev.y, ev.z)
end)

-- A hopper's things fall out when it goes, as a chest's do.
core.event.on("core:pre_break", function(ev)
    if HOPPERS[ev.id] then
        core.container_spill(ev.x, ev.y, ev.z)
        core.container_clear(ev.x, ev.y, ev.z)
    end
end)

-- Every 0.4 s, what lies on a hopper, then one thing from the container over
-- it, then one thing on into the container it points at.
core.register_abm("cinnabar:hopper", { "cinnabar:hopper", "cinnabar:hopper_side" }, 0.4, 1, function(cell)
    local x, y, z = cell.x, cell.y, cell.z
    if core.container_fill(x, y, z) < 0 then
        core.container_make(x, y, z, HOPPER_SLOTS)   -- one placed by a mod, not by hand
    end
    core.collect_drops(x, y + 1, z, x, y, z)
    core.container_move(x, y + 1, z, x, y, z, 1)
    local dx, dy, dz = dir_of(x, y, z)
    core.container_move(x, y, z, x - dx, y - dy, z - dz, 1)
end)

core.event.on("core:pre_break", function(ev)
    if ev.id == OBSERVER or ev.id == OBSERVER_ON then
        watch_for(ev.x, ev.y, ev.z, false)
    end
end)

core.event.on("core:broken", function(ev)
    changed(ev.x, ev.y, ev.z)
    -- Dust or a torch on a block that goes goes with it.
    local above = core.get_block(ev.x, ev.y + 1, ev.z)
    if DUST[above] or above == TORCH or above == TORCH_OFF then
        put(ev.x, ev.y + 1, ev.z, 0)
        local what = if DUST[above] then "ore:cinnabar" else "cinnabar:torch"
        core.drop_item(ev.x, ev.y + 1, ev.z, core.item_id(what), 1)
    end
    -- A piston and its head go together, as one piston: the head broken
    -- drops the piston (node_drop) and takes the body with it.
    for _, d in ALL6 do
        local x, y, z = ev.x + d[1], ev.y + d[2], ev.z + d[3]
        local id = core.get_block(x, y, z)
        if ((ev.id == PISTON_ON or ev.id == STICKY_ON) and (id == HEAD or id == STICKY_HEAD))
            or ((ev.id == HEAD or ev.id == STICKY_HEAD) and (id == PISTON_ON or id == STICKY_ON)) then
            put(x, y, z, 0)
        end
    end
    core.meta_set(ev.x, ev.y, ev.z, "delay", "")
    core.meta_set(ev.x, ev.y, ev.z, "mode", "")
    core.meta_set(ev.x, ev.y, ev.z, "level", "")
    core.meta_set(ev.x, ev.y, ev.z, "base", "")
    update(ev.x, ev.y, ev.z)
end)
