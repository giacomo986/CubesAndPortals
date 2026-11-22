--!strict
-- doors: a wooden door two cells tall. Right click opens it or shuts it.
--
-- Four blocks, because a model and a shape are per block: the bottom and the
-- top, each shut and open. The bottom wears the whole door (door.gltf, two
-- cells tall, made in assets/blender/chest_furnace.blend and exported by
-- tools/export_block_models.py); the top wears a triangle hidden inside it, and
-- is there so the upper cell is a wall a body is stopped by and a pick breaks.
--
-- Shut, the door is a panel against the back of its cell; open, it has swung
-- to the left side on its hinges. core.node_door tells the engine each half's
-- shut and open blocks, which is what lets a way through the world
-- (core.find_path) go through an open door, stop at a shut one, and -- for a
-- creature that opens doors -- open it. A zombie does not: a shut door keeps
-- it out.
--
-- Open, it stops nobody: its panel against the side of the doorway left a
-- body two centimetres to come through by, and one that came a little off the
-- middle -- a villager turning into the doorway -- was held at it. Still
-- aimed at and broken by its box. A door shut on whoever stands in it does
-- not hold them either: the engine lets a body out of a door's box it is
-- already in.
local function door(name: string, model: string, open: boolean): number
    local id = core.register_node(name, 150, 108, 62, not open, 0, "upright", 0, 1.5, "wood")
    core.node_model(id, core.register_model(name, model))
    if open then
        core.node_box(id, 0.0, 0.0, 0.0, 0.1875, 1.0, 1.0)
    else
        core.node_box(id, 0.0, 0.0, 0.0, 1.0, 1.0, 0.1875)
    end
    return id
end

local DOOR     = door("doors:door", "door.gltf", false)
local TOP      = door("doors:door_top", "door_half.gltf", false)
local OPEN     = door("doors:door_open", "door_open.gltf", true)
local OPEN_TOP = door("doors:door_open_top", "door_half.gltf", true)

core.node_door(DOOR, OPEN)
core.node_door(TOP, OPEN_TOP)

-- Whatever half is broken, it gives the door back, and nobody holds a half.
core.node_drop(TOP, DOOR)
core.node_drop(OPEN, DOOR)
core.node_drop(OPEN_TOP, DOOR)

-- Six planks, two across and three up.
core.recipe_shaped("doors:door", 1,
    "timber:planks timber:planks",
    "timber:planks timber:planks",
    "timber:planks timber:planks")

-- Each half's other half, and which of the four it becomes opened or shut.
local IS_TOP  = { [TOP] = true, [OPEN_TOP] = true }
local TOGGLED = { [DOOR] = OPEN, [OPEN] = DOOR, [TOP] = OPEN_TOP, [OPEN_TOP] = TOP }

local function put(x: number, y: number, z: number, id: number, facing: number)
    core.set_block(x, y, z, id)
    core.set_facing(x, y, z, facing)
end

-- Put down where there is room for the top: two cells, or none.
core.event.on("core:pre_place", function(ev)
    if ev.id == DOOR and core.get_block(ev.x, ev.y + 1, ev.z) ~= 0 then
        ev.cancel = true
    end
end)

core.event.on("core:placed", function(ev)
    if ev.id == DOOR then
        put(ev.x, ev.y + 1, ev.z, TOP, core.get_facing(ev.x, ev.y, ev.z))
    end
end)

-- Both halves open, or both shut, keeping their turn; with the sound of it
-- (tools/make_hatch_sounds.py).
local function toggle(x: number, y: number, z: number)
    for cy = y, y + 1 do
        local id = core.get_block(x, cy, z)
        if TOGGLED[id] ~= nil then
            put(x, cy, z, TOGGLED[id], core.get_facing(x, cy, z))
        end
    end
    core.play_sound(if core.get_block(x, y, z) == OPEN then "doors:open" else "doors:shut", x + 0.5, y + 1, z + 0.5)
end

-- Right click on either half.
core.event.on("core:used", function(ev)
    if TOGGLED[ev.id] == nil then
        return
    end
    toggle(ev.x, if IS_TOP[ev.id] then ev.y - 1 else ev.y, ev.z)
    ev.cancel = true
end)

-- Cinnabar beside either half opens it, and it shuts when the power goes; a
-- hand still opens and shuts it while the power stays as it is. What is ON
-- is looked up the first time, so this mod need not load after cinnabar.
local ON: { [number]: boolean }? = nil
-- Lit gates of mods/circuits: they give power out of their front only (the
-- way their facing points), so a door is opened by one that faces it.
local GATE_ON: { [number]: boolean } = {}
local GATE_FRONT = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }
local function powered(x: number, y: number, z: number): boolean
    if ON == nil then
        local on: { [number]: boolean } = {}
        for _, n in { "cinnabar:lever_on", "cinnabar:button_on", "cinnabar:torch", "cinnabar:dust_on",
                      "cinnabar:repeater_on", "cinnabar:comparator_on", "cinnabar:observer_on",
                      "compact:cinnabar", "cinnabar:plate_on" } do
            local id = core.node_id(n)
            if id ~= 0 then
                on[id] = true
            end
        end
        -- Lit dust in every shape it is drawn in (mods/cinnabar).
        for a = 0, 2 do for b = 0, 2 do for c = 0, 2 do for d = 0, 2 do
            local id = core.node_id(`cinnabar:dust_on_{a}{b}{c}{d}`)
            if id ~= 0 then
                on[id] = true
            end
        end end end end
        -- Alloy wire and insulated wire that is lit, in every shape and
        -- colour (mods/circuits).
        local shapes = { "none", "end", "line", "corner", "tee", "cross" }
        for _, s in shapes do
            local id = core.node_id(`circuits:alloy_on_{s}`)
            if id ~= 0 then
                on[id] = true
            end
            for _, c in { "white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray",
                          "light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black" } do
                local wid = core.node_id(`circuits:wire_{c}_on_{s}`)
                if wid ~= 0 then
                    on[wid] = true
                end
            end
        end
        local riser = core.node_id("circuits:alloy_on_riser")
        if riser ~= 0 then
            on[riser] = true
        end
        for _, g in { "buffer", "not", "or", "nor", "and", "nand", "xor", "xnor", "rs_latch", "toggle",
                      "d_latch", "pulse", "timer", "counter", "random", "light" } do
            local id = core.node_id(`circuits:gate_on_{g}`)
            if id ~= 0 then
                GATE_ON[id] = true
            end
        end
        ON = on
    end
    local on = ON :: { [number]: boolean }
    for _, d in { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } } do
        local nx, ny, nz = x + d[1], y + d[2], z + d[3]
        local id = core.get_block(nx, ny, nz)
        if on[id] then
            return true
        end
        if GATE_ON[id] and d[2] == 0 then
            local f = GATE_FRONT[core.get_facing(nx, ny, nz) % 4 + 1]
            if nx + f[1] == x and nz + f[2] == z then
                return true
            end
        end
    end
    return false
end
local POWERED: { [string]: boolean } = {}
core.register_abm("doors:powered", { "doors:door", "doors:door_open" }, 0.25, 1, function(cell)
    local key = `{cell.x},{cell.y},{cell.z}`
    local now = powered(cell.x, cell.y, cell.z) or powered(cell.x, cell.y + 1, cell.z)
    if now == (POWERED[key] == true) then
        return
    end
    POWERED[key] = if now then true else nil
    if (cell.id == OPEN) ~= now then
        toggle(cell.x, cell.y, cell.z)
    end
end)

-- Broken: the other half goes with it, giving nothing -- the one broken gave
-- the door.
core.event.on("core:pre_break", function(ev)
    if TOGGLED[ev.id] == nil then
        return
    end
    local other = if IS_TOP[ev.id] then ev.y - 1 else ev.y + 1
    if TOGGLED[core.get_block(ev.x, other, ev.z)] ~= nil then
        core.set_block(ev.x, other, ev.z, 0)
    end
end)

core.log("doors: six planks make a door; right click opens and shuts it")
