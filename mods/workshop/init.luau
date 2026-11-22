--!strict
-- workshop: where a player builds zones (mods/zones), tries them and proposes
-- them to the admins.
--
-- A server whose rooms file has the template [room workshop-*] gives every
-- player with a key a WORKSHOP: a room of their own, "workshop-" and the start
-- of their key, made the first time they say /workshop and stopped a minute
-- after they leave it. Only its owner goes in (core.room_access), and whoever
-- sets up the server's portals.
--
-- The workshop is flat: one floor, a line every 16 blocks, and nothing the
-- other mods' stages would grow on it. In it a player flies, takes no harm and
-- digs ten times as fast; a block put down is not used up, and one dug gives
-- nothing. There is no catalogue: the bag is the same in every room of the
-- server, so what a player builds with is what they carry.
--
-- THE WAND, given on the way in: a right click on a block is one corner, the
-- next the other; the box between them is drawn on the player's screen and the
-- panel opens. The panel names the zone and says what it is for -- the world,
-- start, room or end, the difficulties, how often, mirrored or not -- and
--
--   Save     saves the box as that zone (/zone save, then /zone set);
--   Try      lays a level of that world's zones, the approved and the
--            player's own, on the trying ground south of the spawn, the one
--            before taken away first, and puts the player at its start;
--   Back     where they were before they tried;
--   Propose  asks the admins to approve the zone (/zone propose).
--
-- THE BRUSH, given on the way in too: the blocks of a world's zones without
-- the bag (whose blocks would be had in every room). /workshop blocks, the
-- panel's Blocks or the brush on a block with none picked open the palette:
-- the world's own blocks (games/workshop/mods: chambers_core's) and the
-- zones' markers. A right click puts the one picked on the face clicked; a
-- floor's or a ceiling's thing (a launch plate, a vent, a target tile, a
-- pit's bottom) takes the place of the block clicked; a door is its four
-- cells across the way the player looks, a button or a plate its two by two.
--
--   /workshop          into your workshop, from any room
--   /workshop leave    out, to the spawn of the server's first room
--   /workshop blocks   the brush's palette, in the workshop

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local ROOM = core.room()
local IS_WORKSHOP = string.sub(ROOM, 1, 9) == "workshop-"

local FLOOR = 19             -- the floor's blocks; a body stands at FLOOR + 1, the spawn
local TRY_Z = -400           -- the trying ground: this far south of the spawn
local MAX_W, MAX_H = 64, 48  -- a zone's box, at most (mods/zones)

local FLOOR_ID = core.register_node("workshop:floor", 196, 198, 204, true, 0, "", 0, 0.5, "stone")
local LINE_ID = core.register_node("workshop:line", 150, 156, 168, true, 0, "", 0, 0.5, "stone")
local EDGE_ID = core.register_node("workshop:edge", 255, 200, 40, true, 0, "", 0, 0.2, "stone")

-- ---------------------------------------------------------- the box drawn

local function draw(text: string)
    if text == "c" then
        core.ghost_clear()
        return
    end
    local n = {}
    for v in string.gmatch(text, "%-?%d+") do
        table.insert(n, tonumber(v) :: number)
    end
    if string.sub(text, 1, 1) ~= "b" or #n ~= 6 then
        return
    end
    core.ghost_clear()
    local x1, y1, z1, x2, y2, z2 = n[1], n[2], n[3], n[4], n[5], n[6]
    -- The twelve edges.
    for x = x1, x2 do
        for _, c in { { y1, z1 }, { y1, z2 }, { y2, z1 }, { y2, z2 } } do
            core.ghost_cell(x, c[1], c[2], EDGE_ID)
        end
    end
    for y = y1, y2 do
        for _, c in { { x1, z1 }, { x1, z2 }, { x2, z1 }, { x2, z2 } } do
            core.ghost_cell(c[1], y, c[2], EDGE_ID)
        end
    end
    for z = z1, z2 do
        for _, c in { { x1, y1 }, { x1, y2 }, { x2, y1 }, { x2, y2 } } do
            core.ghost_cell(c[1], c[2], z, EDGE_ID)
        end
    end
end

if drawing then
    if serving then
        core.event.define("workshop:shown", { fields = { text = "string" }, cancellable = false })
        core.event.on("workshop:shown", function(ev) draw(ev.text) end)
    else
        core.event.on("core:net_message", function(ev) draw(ev.text) end)
    end
end

if not serving then
    return
end

core.net.open()
core.register_item("workshop:wand", 1, "", 0, 1.0, "")

local function send(p: number, text: string)
    if drawing then
        core.event.emit("workshop:shown", { text = text })
    else
        core.net.send(p, text)
    end
end

local function key_of(player: number): string
    return string.sub(string.gsub(string.lower(core.player_key(player)), "[^a-z0-9]", ""), 1, 24)
end

local function room_of(player: number): string
    local key = key_of(player)
    return if key == "" then "" else "workshop-" .. key
end

-- Only the owner goes in (and whoever has `portals`, which the server lets in
-- anyway). Said again by each room that sends somebody, and by the workshop
-- itself, so a restart forgets nothing.
local function shut(room: string)
    core.room_access(room, "*", false, false)
    core.room_access(room, "key:" .. string.sub(room, 10), true, true)
end

-- Into the player's workshop, or out of it with "leave": /workshop, and the
-- frame's menu (mods/portal) through "workshop:go".
local open_blocks: (p: number) -> () = function(_) end   -- the brush's palette, below

local function go(p: number, arg: string)
    if arg == "blocks" then
        if IS_WORKSHOP then
            open_blocks(p)
        else
            core.chat(p, "The palette is the workshop's: /workshop goes in.")
        end
        return
    end
    if arg == "leave" then
        if not IS_WORKSHOP then
            core.chat(p, "You are not in a workshop.")
        elseif not core.room_send(p, "") then
            core.chat(p, "There is nowhere to go out to.")
        end
        return
    end
    local room = room_of(p)
    if room == "" then
        core.chat(p, "A workshop is somebody's: join with a key of your own to have one.")
        return
    end
    if ROOM == room then
        core.chat(p, "You are in your workshop. /workshop leave goes out.")
        return
    end
    shut(room)
    if core.room_send(p, room) then
        core.chat(p, "Into your workshop...")
    else
        core.chat(p, "This server has no workshops: its rooms file needs a [room workshop-*].")
    end
end

core.command("workshop", "[leave|blocks] -- into your own workshop, where zones are built; out of it; its palette")
core.event.on("core:command", function(ev)
    if ev.name == "workshop" then
        go(ev.player, string.lower(string.match(ev.args, "^%s*(%S*)") or ""))
    end
end)
core.event.define("workshop:go", { fields = { player = "number", leave = "boolean" }, cancellable = false })
core.event.on("workshop:go", function(ev)
    go(ev.player, if ev.leave then "leave" else "")
end)

if not IS_WORKSHOP then
    return
end

-- ------------------------------------------------------------ the room

shut(ROOM)
core.set_peaceful(true)
core.set_rule("hunger", "off")
core.set_rule("weather_cycle", "off")

core.mapgen_core(false)
core.mapgen_only("workshop")
core.register_mapgen("workshop:floor", function(ctx)
    for x = ctx.min_x, ctx.max_x do
        for z = ctx.min_z, ctx.max_z do
            core.set_block(x, FLOOR, z, if x % 16 == 0 or z % 16 == 0 then LINE_ID else FLOOR_ID)
        end
    end
end)

local SX, _, SZ = core.world_spawn()
local spawn_x, spawn_z = math.floor(SX or 0), math.floor(SZ or 0)

-- Who is here, set up once: on the floor, flying, unhurt, the wand in hand.
local here: { [number]: boolean } = {}
local since = 0.0
core.event.on("core:tick", function(ev)
    since += ev.dt
    if since < 0.5 then
        return
    end
    since = 0
    local now: { [number]: boolean } = {}
    for _, p in core.players() do
        now[p] = true
        if not here[p] then
            here[p] = true
            if spawn_x == 0 and spawn_z == 0 then
                local x, _, z = core.world_spawn()
                spawn_x, spawn_z = math.floor(x or 0), math.floor(z or 0)
            end
            core.player_place(p, spawn_x + 0.5, FLOOR + 1, spawn_z + 0.5)
            core.player_spawn(p, spawn_x + 0.5, FLOOR + 1, spawn_z + 0.5)
            core.player_factors(p, 1, 10, 0, 1, "workshop")
            -- mods/jetpack owns flight where it is loaded.
            if not pcall(core.event.emit, "jetpack:free", { player = p, on = true }) then
                core.player_fly(p, true)
            end
            if core.bag_count(p, "workshop:wand") == 0 then
                core.give_item(p, "workshop:wand", 1)
            end
            if core.bag_count(p, "workshop:brush") == 0 then
                core.give_item(p, "workshop:brush", 1)
            end
            if room_of(p) == ROOM then
                core.chat(p, "Your workshop. The wand picks a zone's box by two corners; /workshop leave goes out.")
            else
                core.chat(p, "Somebody else's workshop: you are let in to look round. /workshop leave goes out.")
            end
        end
    end
    for p in here do
        if not now[p] then
            here[p] = nil
        end
    end
end)

-- A block put down comes back; one dug gives nothing. Only a block that was in
-- the hand comes back: a bucket's water is not an item to be had.
local putting: { [number]: string } = {}
core.event.on("core:pre_place", function(ev)
    if ev.player ~= 0 then
        local name = core.node_name(ev.id)
        putting[ev.player] = if core.held_item(ev.player) == name then name else nil
    end
end)
core.event.on("core:placed", function(ev)
    local name = putting[ev.player]
    if ev.player ~= 0 and name ~= nil and name == core.node_name(ev.id) then
        core.give_item(ev.player, name, 1)
    end
    putting[ev.player] = nil
end)
core.event.on("core:pre_break", function(ev)
    if ev.player ~= 0 then
        ev.cancel = true
        core.set_block(ev.x, ev.y, ev.z, 0)
    end
end)

-- ------------------------------------------------------------ the panel

local WORLDS = { "chambers", "arpg", "backrooms", "parkour", "inferno", "front", "assault" }
local ROLES = { "room", "start", "end" }

type Draft = {
    box: { number }?, corner: { number }?,
    name: string, world: number, role: number,
    dmin: number, dmax: number, weight: number, mirror: boolean,
    back: { number }?,
    brush: string,   -- the block the brush puts down, "" for none picked
}
local drafts: { [number]: Draft } = {}

local function draft(p: number): Draft
    local d = drafts[p]
    if d then
        return d
    end
    local fresh: Draft = { name = "", world = 1, role = 1, dmin = 1, dmax = 10, weight = 5, mirror = true,
                           brush = "" }
    drafts[p] = fresh
    return fresh
end

local function zone_id(d: Draft): string
    return `{WORLDS[d.world]}.{d.name}`
end

-- ------------------------------------------- the brush's palette

-- Each world's own blocks for its zones (what games/workshop/mods loads here),
-- those of them this workshop has; and every world's markers.
local PALETTE: { [string]: { string } } = {
    chambers = { "panel", "metal", "metal_slab", "floor", "light", "glass", "pit", "door", "button", "pushbutton",
                 "vent", "plate", "pedestal", "target", "emitter", "exit" },
    arpg = { "grass", "moss", "dirt", "rock", "cobble", "mossy", "flags", "planks", "roof", "awning", "grave",
             "brazier", "waystone", "cliff", "highland" },
    backrooms = { "wallpaper", "carpet", "ceiling", "tube", "tube_off", "stair_carpet", "tile", "pool_tile",
                  "pool_edge", "pool_lamp", "water", "slide", "ladder", "stair_tile", "concrete", "slab", "lamp",
                  "lamp_off", "puddle", "stair_concrete", "frame", "glow" },
    parkour = { "concrete", "brick", "steel", "hazard", "pad", "bounce", "crumble", "frame", "lamp" },
    inferno = { "scoria", "ash_sand", "glowrock", "bricks", "obsidian", "lava", "basalt", "quartz_block",
                "ember_lantern", "fungus_stem", "fungus_cap", "fungus_planks", "inferno_heart:brazier",
                "inferno_heart:gate" },
    front = { "grass", "dirt", "stone", "road", "sandbag", "brick", "wall", "concrete", "planks", "roof", "lamp",
              "red", "blue" },
    assault = { "grass", "dirt", "stone", "road", "sandbag", "logs", "planks", "wall", "concrete", "basalt", "lamp",
                "mud", "bog", "reed", "deadwood", "ash", "char", "cinder", "shard", "crystal", "bone", "ladder" },
}
-- A name with no ":" is the world's prefix's; one with it is whole.
local PREFIX: { [string]: string } = { chambers = "chambers_core:", arpg = "arpg_core:", backrooms = "backrooms:",
                                       parkour = "parkour:", inferno = "inferno:", front = "front_core:",
                                       assault = "assault_core:" }
local MARKERS = { "doorway", "start", "exit", "mob", "loot", "key", "trigger" }
-- What takes the place of the block clicked rather than going on its face.
local INTO = { ["chambers_core:plate"] = true, ["chambers_core:vent"] = true, ["chambers_core:target"] = true,
               ["chambers_core:pit"] = true }

local function choices(world: string): { string }
    local out = {}
    for _, n in PALETTE[world] or {} do
        local name = if string.find(n, ":", 1, true) then n else (PREFIX[world] or "") .. n
        if core.node_id(name) ~= 0 then
            table.insert(out, name)
        end
    end
    for _, n in MARKERS do
        table.insert(out, `zones:{n}`)
    end
    return out
end

local function show_blocks(p: number)
    local d = draft(p)
    local world = WORLDS[d.world]
    local list = choices(world)
    local name = `workshop:blocks{p}`
    local rows = math.ceil(#list / 3)
    core.ui_define(name, 13, 3.4 + rows * 1.0)
    core.ui_button(name, "world", 0.4, 0.3, 12.2, 0.9, `world: {world}`)
    core.ui_label(name, 0.4, 1.4, if d.brush == "" then "The brush puts down: nothing yet" else `The brush puts down: {d.brush}`)
    for i, n in list do
        local col, row = (i - 1) % 3, (i - 1) // 3
        local short = string.match(n, ":(.+)$") or n
        local mark = if string.sub(n, 1, 6) == "zones:" then "marker " else ""
        core.ui_button(name, `b:{n}`, 0.4 + col * 4.1, 2.0 + row * 1.0, 4.0, 0.85,
            if n == d.brush then `> {mark}{short}` else `{mark}{short}`)
    end
    core.ui_button(name, "close", 0.4, 2.2 + rows * 1.0, 12.2, 0.8, "Close")
    core.ui_show(name, true, p)
end

open_blocks = show_blocks


local function panel(p: number)
    local d = draft(p)
    local name = `workshop:panel{p}`
    core.ui_define(name, 13, 12.8)
    local b = d.box
    core.ui_label(name, 0.4, 0.3, if b then `The box: {b[4] - b[1] + 1} x {b[5] - b[2] + 1} high x {b[6] - b[3] + 1}`
                                  else "No box yet: the wand on two corners.")
    core.ui_label(name, 0.4, 1.15, "Name (Enter):")
    core.ui_field(name, "name", 4.0, 1.0, 8.6, 0.8, d.name)
    core.ui_button(name, "world", 0.4, 2.0, 6.0, 0.9, `world: {WORLDS[d.world]}`)
    core.ui_button(name, "role", 6.6, 2.0, 6.0, 0.9, `a {ROLES[d.role]} of the level`)
    core.ui_button(name, "dmin", 0.4, 3.1, 4.0, 0.9, `difficulty from {d.dmin}`)
    core.ui_button(name, "dmax", 4.5, 3.1, 4.0, 0.9, `to {d.dmax}`)
    core.ui_button(name, "weight", 8.6, 3.1, 4.0, 0.9, `how often: {d.weight}`)
    core.ui_button(name, "mirror", 0.4, 4.2, 12.2, 0.9, if d.mirror then "may be laid mirrored: yes" else "may be laid mirrored: no")
    core.ui_button(name, "save", 0.4, 5.6, 12.2, 1.1, if d.name == "" then "Save (name it first)" else `Save as {zone_id(d)}`)
    core.ui_button(name, "try", 0.4, 6.9, 12.2, 1.1, `Try a level of {WORLDS[d.world]}`)
    core.ui_button(name, "back", 0.4, 8.2, 12.2, 1.1, if d.back then "Back to where I was" else "Back (after a try)")
    core.ui_button(name, "propose", 0.4, 9.5, 12.2, 1.1, "Propose it to the admins")
    core.ui_button(name, "blocks", 0.4, 10.8, 12.2, 0.9, "Blocks: the brush's palette")
    core.ui_button(name, "close", 0.4, 11.8, 12.2, 0.8, "Close")
    core.ui_show(name, true, p)
end

core.event.on("core:used", function(ev)
    if ev.held ~= "workshop:wand" then
        return
    end
    ev.cancel = true
    local d = draft(ev.player)
    local a = d.corner
    if a == nil then
        d.corner = { ev.x, ev.y, ev.z }
        core.notice(ev.player, "One corner; now the other.")
        return
    end
    d.corner = nil
    local x1, x2 = math.min(a[1], ev.x), math.max(a[1], ev.x)
    local y1, y2 = math.min(a[2], ev.y), math.max(a[2], ev.y)
    local z1, z2 = math.min(a[3], ev.z), math.max(a[3], ev.z)
    if x2 - x1 >= MAX_W or z2 - z1 >= MAX_W or y2 - y1 >= MAX_H then
        core.notice(ev.player, `A zone is at most {MAX_W} by {MAX_H} high by {MAX_W}.`)
        return
    end
    d.box = { x1, y1, z1, x2, y2, z2 }
    send(ev.player, `b {x1} {y1} {z1} {x2} {y2} {z2}`)
    panel(ev.player)
end)

-- The trying ground: what the last try laid there, taken away before the next.
type Clear = { x1: number, y1: number, z1: number, x2: number, y2: number, z2: number, x: number, y: number, z: number }
local clearing: Clear? = nil
local waiting: { player: number, world: string, owner: string }? = nil
local laid: { [string]: boolean } = {}   -- the tags of tries going up, not yet stood at

local function last_try(): Clear?
    local n = {}
    for v in string.gmatch(core.storage_get("tried"), "%-?%d+") do
        table.insert(n, tonumber(v) :: number)
    end
    if #n ~= 6 then
        return nil
    end
    return { x1 = n[1], y1 = n[2], z1 = n[3], x2 = n[4], y2 = n[5], z2 = n[6], x = n[1], y = n[2], z = n[3] }
end

local function lay_try()
    local w = waiting
    if w == nil then
        return
    end
    waiting = nil
    local tag = `workshop {w.player}`
    laid[tag] = true
    core.event.emit("zones:lay", { tag = tag, world = w.world, owner = w.owner, x = spawn_x, y = FLOOR + 1,
                                   z = spawn_z + TRY_Z, seed = math.random(1, 1000000), d = 1, count = 6 })
end

core.event.on("core:tick", function(_)
    local c = clearing
    if c == nil then
        return
    end
    local budget = 6000
    while budget > 0 do
        if core.get_block(c.x, c.y, c.z) ~= 0 then
            core.set_block(c.x, c.y, c.z, 0)
        end
        budget -= 1
        c.x += 1
        if c.x > c.x2 then
            c.x = c.x1
            c.z += 1
            if c.z > c.z2 then
                c.z = c.z1
                c.y += 1
                if c.y > c.y2 then
                    clearing = nil
                    core.storage_set("tried", "")
                    lay_try()
                    return
                end
            end
        end
    end
end)

core.event.on("zones:mark", function(ev)
    if laid[ev.tag] and ev.kind == "start" then
        laid[ev.tag] = nil
        local p = tonumber(string.match(ev.tag, "%d+$") or "") or 0
        core.player_place(p, ev.x + 0.5, ev.y, ev.z + 0.5)
    end
end)

core.event.on("zones:laid", function(ev)
    if string.sub(ev.tag, 1, 9) ~= "workshop " then
        return
    end
    local p = tonumber(string.match(ev.tag, "%d+$") or "") or 0
    if ev.pieces == 0 then
        laid[ev.tag] = nil
        core.chat(p, "No zone of that world to start a level with: a level wants a start zone.")
        return
    end
    -- The floor under it stays: the level is taken away above it.
    core.storage_set("tried", `{ev.x1} {math.max(ev.y1, FLOOR + 1)} {ev.z1} {ev.x2} {ev.y2} {ev.z2}`)
    if laid[ev.tag] then
        -- No start marker in it: in front of its first zone.
        laid[ev.tag] = nil
        core.player_place(p, spawn_x + 0.5, FLOOR + 1, spawn_z + TRY_Z - 2.5)
    end
    core.chat(p, `A level of {ev.pieces} zone(s). Back, on the wand's panel, takes you to where you were.`)
end)

local function run(p: number, line: string)
    core.event.emit("zones:do", { player = p, line = line })
end

core.event.on("core:ui_click", function(ev)
    if ev.panel ~= `workshop:panel{ev.player}` then
        return
    end
    local p = ev.player
    local d = draft(p)
    if ev.id == "close" or ev.id == "core:esc" then
        core.ui_show(ev.panel, false, p)
        return
    elseif ev.id == "name" then
        d.name = string.sub(string.gsub(string.lower(ev.text), "[^a-z0-9_]", ""), 1, 24)
    elseif ev.id == "world" then
        d.world = d.world % #WORLDS + 1
    elseif ev.id == "role" then
        d.role = d.role % #ROLES + 1
    elseif ev.id == "dmin" then
        d.dmin = d.dmin % 10 + 1
        d.dmax = math.max(d.dmax, d.dmin)
    elseif ev.id == "dmax" then
        d.dmax = if d.dmax >= 10 then d.dmin else d.dmax + 1
    elseif ev.id == "weight" then
        d.weight = d.weight % 20 + 1
    elseif ev.id == "mirror" then
        d.mirror = not d.mirror
    elseif ev.id == "save" then
        local b = d.box
        if b == nil or d.name == "" then
            core.notice(p, if b == nil then "Pick the box first: the wand on two corners." else "Name it first.")
            return
        end
        local id = zone_id(d)
        run(p, `save {WORLDS[d.world]} {d.name} {b[1]} {b[2]} {b[3]} {b[4]} {b[5]} {b[6]} {ROLES[d.role]}`)
        run(p, `set {id} d {d.dmin} {d.dmax}`)
        run(p, `set {id} weight {d.weight}`)
        run(p, `set {id} mirror {if d.mirror then "on" else "off"}`)
    elseif ev.id == "propose" then
        if d.name == "" then
            core.notice(p, "Name it, and save it, first.")
            return
        end
        run(p, `propose {zone_id(d)}`)
    elseif ev.id == "try" then
        if clearing ~= nil or waiting ~= nil then
            core.notice(p, "A level is going up already.")
            return
        end
        if d.back == nil then
            local x, y, z = core.player_pos(p)
            if x and y and z then
                d.back = { x, y, z }
            end
        end
        waiting = { player = p, world = WORLDS[d.world], owner = core.player_key(p) }
        local last = last_try()
        if last then
            clearing = last
        else
            lay_try()
        end
        core.ui_show(ev.panel, false, p)
        return
    elseif ev.id == "blocks" then
        core.ui_show(ev.panel, false, p)
        show_blocks(p)
        return
    elseif ev.id == "back" then
        local b = d.back
        if b == nil then
            core.notice(p, "Nothing to go back to: Try first.")
            return
        end
        d.back = nil
        core.player_place(p, b[1], b[2], b[3])
        core.ui_show(ev.panel, false, p)
        return
    end
    panel(p)
end)

-- ------------------------------------------------------------ the brush

core.register_item("workshop:brush", 1, "", 0, 1.0, "")
local EYE = 1.5   -- mods/robot's
core.event.on("core:ui_click", function(ev)
    if ev.panel ~= `workshop:blocks{ev.player}` then
        return
    end
    local p = ev.player
    local d = draft(p)
    if ev.id == "close" or ev.id == "core:esc" then
        core.ui_show(ev.panel, false, p)
        return
    elseif ev.id == "world" then
        d.world = d.world % #WORLDS + 1
    elseif string.sub(ev.id, 1, 2) == "b:" then
        d.brush = string.sub(ev.id, 3)
        core.ui_show(ev.panel, false, p)
        core.notice(p, `The brush puts down {d.brush}.`)
        return
    end
    show_blocks(p)
end)

-- The face of cell (x, y, z) the player's eye looks at: its normal.
local function face(p: number, x: number, y: number, z: number): (number, number, number)
    local px, py, pz = core.player_pos(p)
    local dx, dy, dz = core.player_look(p)
    if px == nil or dx == nil then
        return 0, 1, 0
    end
    local len = math.sqrt(dx * dx + dy * dy + dz * dz)
    dx, dy, dz = dx / len, dy / len, dz / len
    local ox, oy, oz = px, (py :: number) + EYE, pz :: number
    -- Where the ray meets the cell's box: the nearest of the planes it comes at.
    local best, nx, ny, nz = math.huge, 0, 1, 0
    for _, f in { { 1, 0, 0, x + 1 }, { -1, 0, 0, x }, { 0, 1, 0, y + 1 }, { 0, -1, 0, y }, { 0, 0, 1, z + 1 },
                  { 0, 0, -1, z } } do
        -- Only a face the ray comes at from in front of it.
        if f[1] * dx + f[2] * dy + f[3] * dz < 0 then
            -- How far along the ray its plane is; the face is the one the hit
            -- lies closest to the inside of.
            local t = if f[1] ~= 0 then (f[4] - ox) / dx elseif f[2] ~= 0 then (f[4] - oy) / dy else (f[4] - oz) / dz
            local hx, hy, hz = ox + dx * t, oy + dy * t, oz + dz * t
            local slack = math.max(math.abs(hx - math.clamp(hx, x, x + 1)), math.abs(hy - math.clamp(hy, y, y + 1)),
                math.abs(hz - math.clamp(hz, z, z + 1)))
            if t > 0 and slack < best then
                best, nx, ny, nz = slack, f[1], f[2], f[3]
            end
        end
    end
    return nx, ny, nz
end

local function put(x: number, y: number, z: number, name: string, f: number)
    local id = core.node_id(name)
    core.set_block(x, y, z, id)
    if core.node_turns(id) > 1 then
        core.set_facing(x, y, z, f)
    end
end

core.event.on("core:used", function(ev)
    if ev.held ~= "workshop:brush" then
        return
    end
    ev.cancel = true
    local p = ev.player
    local d = draft(p)
    if d.brush == "" or core.node_id(d.brush) == 0 then
        show_blocks(p)
        return
    end
    local nx, ny, nz = face(p, ev.x, ev.y, ev.z)
    local x, y, z = ev.x + nx, ev.y + ny, ev.z + nz
    if INTO[d.brush] then
        x, y, z = ev.x, ev.y, ev.z
    end
    -- Across the way the player looks: an upright block's quarter turn.
    local lx, _, lz = core.player_look(p)
    local along_x = math.abs(lz or 0) > math.abs(lx or 0)
    local f = if along_x then 0 else 3   -- an upright block's second cell along +X, or +Z (chambers_game's ALONG)
    local b = d.brush
    if b == "chambers_core:door" then
        local ax, az = if along_x then 1 else 0, if along_x then 0 else 1
        put(x, y, z, b, f)
        put(x, y + 1, z, "chambers_core:door_part", f)
        put(x + ax, y, z + az, "chambers_core:door_part", f)
        put(x + ax, y + 1, z + az, "chambers_core:door_part", f)
    elseif b == "chambers_core:button" or b == "chambers_core:plate" then
        local part = if b == "chambers_core:button" then "chambers_core:button_part" else "chambers_core:plate_part"
        for _, o in { { 1, 0 }, { 0, 1 }, { 1, 1 } } do
            put(x + o[1], y, z + o[2], part, 0)
        end
        put(x, y, z, b, 0)
    else
        put(x, y, z, b, f)
    end
end)
