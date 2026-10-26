--!strict
-- A conveyor belt: what stands on it -- a stack, a player, a creature -- goes
-- the way it runs, which is away from whoever put it down. The carrying is the
-- engine's (core.node_conveyor), every tick; this mod decides what each cell
-- LOOKS like from its neighbours.
--
-- Eight blocks, one item (models from tools/make_conveyor.py,
-- ~/kart_assets/conveyor.blend, their ribs and arrows moving on the clock):
--   * straight, with a drum at each end that does not meet another belt, so
--     cells in a line draw as one belt;
--   * a quarter turn, when nothing feeds a cell from behind and a belt at its
--     side runs into it -- put a belt across the end of a line and the line
--     turns the corner into it. Stacks follow by themselves: the engine pulls
--     them to the middle of the cell they are on;
--   * a ramp up, when nothing is ahead at its level and a belt a block up and
--     ahead runs the same way; and a ramp down, when nothing feeds it at its
--     level and a belt a block up and behind runs into its column. A ramp's top
--     stands out of its cell, and steps under it are what a body walks up.
-- A player's placing and breaking reshape at once; anything else that changes a
-- belt (the screwdriver, another mod's core.set_block) within a second.
--
-- And two blocks that go with belts: a sorter and an extractor, below.

local SPEED = 1.0   -- m/s, one cell a second; the models' loops go at this
local TOP = 5 / 16  -- the belt's top over its cell's floor
local SHAPES = { "belt", "belt_in", "belt_out", "belt_mid", "belt_left", "belt_right", "belt_up", "belt_down" }

local ID: { [string]: number } = {}
local IS: { [number]: boolean } = {}
local NAMES: { string } = {}
for _, shape in SHAPES do
    local id = core.register_node(`conveyor:{shape}`, 40, 40, 44, true, 0, "upright", 0, 1.5)
    core.node_model(id, core.register_model(`conveyor:{shape}`, `{shape}.gltf`))
    if shape == "belt_up" or shape == "belt_down" then
        -- Four steps under the slope, a quarter each: in at z = 1, out at z = 0.
        for k = 0, 3 do
            local top = if shape == "belt_up" then TOP + 1 - (k + 1) / 4 else TOP + k / 4
            core.node_box(id, 0, 0, k / 4, 1, top, (k + 1) / 4)
        end
        if shape == "belt_up" then
            core.node_conveyor(id, SPEED, TOP, 1)
        else
            core.node_conveyor(id, SPEED, TOP + 1, -1)
        end
    else
        core.node_box(id, 0, 0, 0, 1, TOP, 1)
        core.node_conveyor(id, SPEED, TOP)
    end
    if shape ~= "belt" then core.node_drop(id, ID.belt) end
    ID[shape] = id
    IS[id] = true
    table.insert(NAMES, `conveyor:{shape}`)
end
-- In the hand and on the ground (tools/make_held_models.py, which also draws
-- its icon, assets/textures/conveyor_belt.png).
core.register_model("conveyor:belt_held", "belt_held.gltf")

-- A sorter: one way in (behind it) and three out -- left, ahead, right. Its
-- three squares are filters, in that order: a thing put in one sends that
-- item out that way. Anything no filter names goes out, in turn, through the
-- ways with no filter that have a belt. Nowhere to go, and it waits.
local SORTER = core.register_node("conveyor:sorter", 60, 60, 66, true, 0, "upright", 0, 1.5)
core.node_model(SORTER, core.register_model("conveyor:sorter", "sorter.gltf"))
core.node_box(SORTER, 0, 0, 0, 1, TOP, 1)
core.node_keeps_drops(SORTER)   -- what waits on it for its way out does not go off
core.register_model("conveyor:sorter_held", "sorter_held.gltf")

-- An extractor: the first cell of a belt, under a hood. It takes one thing a
-- second -- what a belt carries away -- out of a container beside, behind, over
-- or under it (from a store or a product square: a furnace's ingots and not its
-- coal), onto itself, and carries it on like a belt. Not while the last
-- one is still in its cell, so a belt that backs up stops it rather than
-- piling things up.
local EXTRACTOR = core.register_node("conveyor:extractor", 60, 60, 66, true, 0, "upright", 0, 1.5)
core.node_model(EXTRACTOR, core.register_model("conveyor:extractor", "extractor.gltf"))
core.node_box(EXTRACTOR, 0, 0, 0, 1, TOP, 1)
core.node_box(EXTRACTOR, 0, 0, 9 / 16, 1, 12 / 16, 1)   -- the hood, over the back half
core.node_conveyor(EXTRACTOR, SPEED, TOP)
core.register_model("conveyor:extractor_held", "extractor_held.gltf")

-- What runs like a belt, for the shapes of the belts round it.
local CARRIES: { [number]: boolean } = { [EXTRACTOR] = true }
for id in IS do CARRIES[id] = true end

-- The way a cell runs, by its facing: as the engine carries.
local RUN = { { 0, -1 }, { -1, 0 }, { 0, 1 }, { 1, 0 } }

local function run_of(x: number, y: number, z: number): (number?, number?)
    if not CARRIES[core.get_block(x, y, z)] then return nil end
    local r = RUN[core.get_facing(x, y, z) % 4 + 1]
    return r[1], r[2]
end

local function belt(x: number, y: number, z: number): boolean
    return CARRIES[core.get_block(x, y, z)] == true
end

-- Whether the belt at that cell runs (dx, dz).
local function heads(x: number, y: number, z: number, dx: number, dz: number): boolean
    local rx, rz = run_of(x, y, z)
    return rx == dx and rz == dz
end

local function shape_of(x: number, y: number, z: number): string
    local dx, dz = run_of(x, y, z)
    assert(dx and dz)
    local ahead = belt(x + dx, y, z + dz)
    if not ahead and heads(x + dx, y + 1, z + dz, dx, dz) then return "belt_up" end
    local fed = heads(x - dx, y, z - dz, dx, dz)
    if not fed and heads(x - dx, y + 1, z - dz, dx, dz) and not belt(x, y + 1, z) then return "belt_down" end
    -- Or the top of a ramp up behind and under this one, or a way out of a sorter.
    fed = fed or (heads(x - dx, y - 1, z - dz, dx, dz) and not belt(x, y - 1, z))
        or core.get_block(x - dx, y, z - dz) == SORTER
    if not fed then
        -- Left of a way (dx, dz) is (dz, -dx): the curve models are fed from there.
        if heads(x + dz, y, z - dx, -dz, dx) then return "belt_left" end
        if heads(x - dz, y, z + dx, dz, -dx) then return "belt_right" end
    end
    -- On into a belt that does not run straight back at this one, or down a ramp.
    local ax, az = run_of(x + dx, y, z + dz)
    local on = (ax ~= nil and not (ax == -dx and az == -dz)) or (not ahead and heads(x + dx, y - 1, z + dz, dx, dz))
        or core.get_block(x + dx, y, z + dz) == SORTER
    return if fed then (if on then "belt_mid" else "belt_in") else (if on then "belt_out" else "belt")
end

local function refresh(x: number, y: number, z: number)
    if not IS[core.get_block(x, y, z)] then return end
    local want = ID[shape_of(x, y, z)]
    if core.get_block(x, y, z) == want then return end
    local facing = core.get_facing(x, y, z)
    core.set_block(x, y, z, want)
    core.set_facing(x, y, z, facing)
end

-- A cell and those round it, a level up and down too: a belt put down or
-- taken away changes the ends, turns and ramps of its neighbours.
local function around(x: number, y: number, z: number)
    refresh(x, y, z)
    for dy = -1, 1 do
        refresh(x + 1, y + dy, z)
        refresh(x - 1, y + dy, z)
        refresh(x, y + dy, z + 1)
        refresh(x, y + dy, z - 1)
    end
end

core.event.on("core:placed", function(ev)
    if CARRIES[ev.id] or ev.id == SORTER then around(ev.x, ev.y, ev.z) end
    if ev.id == SORTER then core.container_make(ev.x, ev.y, ev.z, 3) end
end)
core.event.on("core:broken", function(ev)
    if CARRIES[ev.id] or ev.id == SORTER then around(ev.x, ev.y, ev.z) end
end)

-- A sorter's filters are a player's things: shown on a right click, and on the
-- ground when it goes.
core.event.on("core:used", function(ev)
    if ev.id ~= SORTER then return end
    core.container_show(ev.x, ev.y, ev.z, 3, ev.player)
    ev.cancel = true
end)
core.event.on("core:pre_break", function(ev)
    if ev.id ~= SORTER then return end
    core.container_spill(ev.x, ev.y, ev.z)
    core.container_clear(ev.x, ev.y, ev.z)
end)

local turn: { [string]: number } = {}   -- each sorter's last way out for the unnamed
core.register_abm("conveyor:sort", "conveyor:sorter", 0.25, 1, function(cell)
    local x, y, z = cell.x, cell.y, cell.z
    local item = core.drop_at(x, y, z)
    if item == 0 then return end
    local r = RUN[core.get_facing(x, y, z) % 4 + 1]
    local dx, dz = r[1], r[2]
    local outs = { { dz, -dx }, { dx, dz }, { -dz, dx } }   -- left, ahead, right: the filters' order
    local function free(o: { number }): boolean
        return core.drop_at(x + o[1], y, z + o[2]) == 0
    end
    local way: number? = nil
    for i = 1, 3 do
        if core.container_item(x, y, z, i) == item then
            way = if free(outs[i]) then i else nil
            if not way then return end   -- its way is backed up: it waits
            break
        end
    end
    if not way then
        local key = `{x},{y},{z}`
        local last = turn[key] or 0
        for n = 1, 3 do
            local i = (last + n - 1) % 3 + 1
            local o = outs[i]
            if core.container_item(x, y, z, i) == 0 and IS[core.get_block(x + o[1], y, z + o[2])] and free(o) then
                way = i
                turn[key] = i
                break
            end
        end
    end
    if not way then return end
    local o = outs[way]
    -- Just over the edge of the belt that way, and a little above it.
    core.drop_send(x, y, z, x + 0.5 + o[1] * 0.7, y + TOP + 0.2, z + 0.5 + o[2] * 0.7)
end)

core.register_abm("conveyor:extract", "conveyor:extractor", 1.0, 1, function(cell)
    local x, y, z = cell.x, cell.y, cell.z
    if core.drop_at(x, y, z) ~= 0 then return end
    local r = RUN[core.get_facing(x, y, z) % 4 + 1]
    local dx, dz = r[1], r[2]
    for _, o in { { -dx, 0, -dz }, { dz, 0, -dx }, { -dz, 0, dx }, { 0, 1, 0 }, { 0, -1, 0 } } do
        local sx, sy, sz = x + o[1], y + o[2], z + o[3]
        -- Not a sorter's filters, which are a container too. Out of the
        -- hood's mouth, a little ahead of the middle -- and only what may come
        -- out (core.container_roles): a furnace's product, not its ore or coal.
        if core.get_block(sx, sy, sz) ~= SORTER and core.container_fill(sx, sy, sz) > 0
            and core.container_drop(sx, sy, sz, x + 0.5 + dx * 0.15, y + 0.5, z + 0.5 + dz * 0.15, 1) > 0 then
            return
        end
    end
end)
-- What no event says: a belt turned by the screwdriver, or written by a mod.
core.register_abm("conveyor:shape", NAMES, 1.0, 1, function(cell)
    refresh(cell.x, cell.y, cell.z)
end)

-- Leather over iron and cinnabar: four belts.
core.recipe_shaped("conveyor:belt", 4,
    "animals:leather     animals:leather  animals:leather",
    "furnace:iron_ingot  ore:cinnabar     furnace:iron_ingot")

-- A sorter: a belt each way and cinnabar in the middle. An extractor: a
-- hopper over a belt.
core.recipe_shaped("conveyor:sorter", 1,
    "-              conveyor:belt  -",
    "conveyor:belt  ore:cinnabar   conveyor:belt")
core.recipe_shaped("conveyor:extractor", 1,
    "cinnabar:hopper",
    "conveyor:belt")

core.log(`conveyor: {#SHAPES} shapes of belt at {SPEED} m/s, a sorter and an extractor`)
