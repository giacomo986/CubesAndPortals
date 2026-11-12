--!strict
-- Portals built out of blocks, the way yaportal built them in the original engine and the
-- Inferno's are built everywhere: a rectangle of portal:frame round a hole of air
-- opens a portal when its last block goes in, or when the last thing inside it
-- is dug out. Standing up, 1 to 8 wide and 2 to 8 tall inside; or lying down as
-- a floor or a ceiling, 1 to 8 by 1 to 8. The front faces whoever finished it,
-- and a lying one is turned so its picture stands upright for them.
-- Every frame gets a name, "portal 1", "portal 2", as yaportal's did -- the
-- menu's first line is a field, so a player may call it anything else -- and leads
-- to one other frame: the last one finished that led nowhere, when it is
-- finished, or whichever its menu links it to later. A right click on the frame
-- opens that menu: its name, where it leads, the other frames to link it to,
-- and buttons to turn the front round, turn it a quarter, or close it; and
-- what the engine's openings can be besides a door: the colour it shows while
-- it leads nowhere, one that SCALES (core.portal_scale: with an other end of
-- another size, what crosses comes out that much bigger or smaller), and a
-- WINDOW (core.portal_window: it shows what its other end faces and nothing
-- crosses). A frame is its builder's (core.player_key): they and whoever has
-- the `portals` permission set it up, anybody else's right click says where
-- it leads. A frame from before frames had owners, or built by somebody with
-- no key, is anybody's, as every frame was.
--
-- On a server of several rooms, whoever has the `portals` permission may make
-- a standing frame lead to ANOTHER ROOM: "other rooms: yes" in its menu says
-- it to the other rooms (core.room_portal_set), and the menu then lists the
-- frames those rooms say, each a button that links the two. Such a frame has
-- no opening of this mod's: the server gives it one of the world's mouths
-- while a player is near either end, and takes it back after, so the hole is
-- empty with nobody about. A link made or undone from the other room is seen
-- here within two seconds and kept.
--
-- A world holds as many frames as are built: core.portal_open's portals are
-- virtual, and only the ones near a player (and what they lead to) take one
-- of the world's openings at a time. Breaking a block of a frame closes it; a right click on a
-- whole frame that is closed opens it again. What is open, and what leads where,
-- is kept across a restart (core.storage_set) and opened again on the first
-- tick, which is when a server's openings exist.

local FRAME = core.register_node("portal:frame", 70, 120, 230, true, 5, "", 0, 1.0, "stone")

-- Eight frames round an ingot: the iron is what makes it more than a wall.
core.recipe_shaped("portal:frame", 8,
    "core:frame core:frame         core:frame",
    "core:frame furnace:iron_ingot core:frame",
    "core:frame core:frame         core:frame")

local MAX = 8   -- the widest hole, either way, as yaportal had it

type Frame = {
    x: number, y: number, z: number,      -- the hole's lowest cell
    ex: number, ey: number, ez: number,   -- its size in cells; 1 across its own plane
    axis: number,                         -- 0 XY, 1 YZ, 2 XZ, as core.portal_open takes it
    sign: number,                         -- which way along the normal its front faces
    rot: number,                          -- quarter turns about the front
    slot: number,                         -- which of the world's openings it is
    name: string,                         -- "portal 3": what the menus call it
    link: string?,                        -- the name of the frame it leads to
    owner: string,                        -- the key of whoever finished it; "" is anybody's
    colour: number,                       -- shown while it leads nowhere: 0 green, 1 blue, 2 orange
    scales: boolean,                      -- core.portal_scale
    window: boolean,                      -- it only shows what its link faces (core.portal_window)
    away: boolean,                        -- said to the other rooms (core.room_portal_set)
    far_room: string,                     -- the room and the frame there it leads to, or ""
    far_name: string,
}

-- The frames that are open now, as many as there are.
local frames: { Frame } = {}

local function named(name: string?): Frame?
    for _, g in frames do
        if g.name == name then
            return g
        end
    end
    return nil
end

-- A name nobody has had in this world: a counter kept with the frames, so a
-- frame closed and another built never share one.
local function new_name(): string
    local n = (tonumber(core.storage_get("count")) or 0) + 1
    core.storage_set("count", tostring(n))
    return `portal {n}`
end

-- A cell of plane `axis`, `u` along its first in-plane direction and `v` along
-- its second, counted from x, y, z. The first is across; the second is up for a
-- standing frame and z for a lying one.
local function cell(axis: number, x: number, y: number, z: number, u: number, v: number): (number, number, number)
    if axis == 0 then
        return x + u, y + v, z
    elseif axis == 1 then
        return x, y + v, z + u
    end
    return x + u, y, z + v
end

local function is_frame(block: number, axis: number, x: number, y: number, z: number, u: number, v: number): boolean
    local cx, cy, cz = cell(axis, x, y, z, u, v)
    return core.get_block(cx, cy, cz) == block
end

local STEPS = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }

-- The hole of a whole frame of `block` that the air at x, y, z belongs to, in
-- plane `axis`, or nil. The air is filled across the plane from there, four ways; more
-- than MAX across either way is the open sky and not a hole. What it filled has
-- to be a full rectangle, and every cell round it, corners too, a frame.
local function hole(block: number, axis: number, x: number, y: number, z: number): Frame?
    if core.get_block(x, y, z) ~= 0 then
        return nil
    end

    local function key(u: number, v: number): number
        return (u + 32) * 128 + (v + 32)
    end

    local seen: { [number]: boolean } = { [key(0, 0)] = true }
    local stack: { number } = { 0, 0 }
    local count = 0
    local u0, u1, v0, v1 = 0, 0, 0, 0

    while #stack > 0 do
        local v = table.remove(stack) :: number
        local u = table.remove(stack) :: number
        count += 1
        u0, u1 = math.min(u0, u), math.max(u1, u)
        v0, v1 = math.min(v0, v), math.max(v1, v)
        if u1 - u0 >= MAX or v1 - v0 >= MAX then
            return nil
        end
        for _, d in STEPS do
            local nu, nv = u + d[1], v + d[2]
            local k = key(nu, nv)
            if not seen[k] then
                seen[k] = true
                local cx, cy, cz = cell(axis, x, y, z, nu, nv)
                if core.get_block(cx, cy, cz) == 0 then
                    table.insert(stack, nu)
                    table.insert(stack, nv)
                end
            end
        end
    end

    local w, h = u1 - u0 + 1, v1 - v0 + 1
    if count ~= w * h then
        return nil   -- something stands in the hole
    end
    if axis ~= 2 and h < 2 then
        return nil   -- a standing hole one node tall fits nobody
    end
    for u = u0 - 1, u1 + 1 do
        if not is_frame(block, axis, x, y, z, u, v0 - 1) or not is_frame(block, axis, x, y, z, u, v1 + 1) then
            return nil
        end
    end
    for v = v0, v1 do
        if not is_frame(block, axis, x, y, z, u0 - 1, v) or not is_frame(block, axis, x, y, z, u1 + 1, v) then
            return nil
        end
    end

    local lx, ly, lz = cell(axis, x, y, z, u0, v0)
    local ex, ey, ez = 1, 1, 1
    if axis == 0 then
        ex, ey = w, h
    elseif axis == 1 then
        ez, ey = w, h
    else
        ex, ez = w, h
    end
    return { x = lx, y = ly, z = lz, ex = ex, ey = ey, ez = ez, axis = axis, sign = 1, rot = 0,
             slot = -1, name = "", owner = "", colour = 0, scales = false, window = false,
             away = false, far_room = "", far_name = "" }
end

-- The whole frame the `block` at x, y, z is part of, or nil: a hole next to
-- it, straight or corner-wise, in any of the three planes.
local function around(x: number, y: number, z: number, block: number): Frame?
    for axis = 0, 2 do
        for du = -1, 1 do
            for dv = -1, 1 do
                if du ~= 0 or dv ~= 0 then
                    local cx, cy, cz = cell(axis, x, y, z, du, dv)
                    local f = hole(block, axis, cx, cy, cz)
                    if f then
                        return f
                    end
                end
            end
        end
    end
    return nil
end

-- Whether x, y, z is one of the cells round f's hole, corners included.
local function on_ring(f: Frame, x: number, y: number, z: number): boolean
    local at, lo, size = { x, y, z }, { f.x, f.y, f.z }, { f.ex, f.ey, f.ez }
    local across = if f.axis == 0 then 3 elseif f.axis == 1 then 1 else 2
    local inside = true
    for i = 1, 3 do
        if i == across then
            if at[i] ~= lo[i] then
                return false
            end
        else
            if at[i] < lo[i] - 1 or at[i] > lo[i] + size[i] then
                return false
            end
            if at[i] < lo[i] or at[i] >= lo[i] + size[i] then
                inside = false
            end
        end
    end
    return not inside
end

local function centre(f: Frame): (number, number, number)
    return f.x + f.ex / 2, f.y + f.ey / 2, f.z + f.ez / 2
end

-- Across and along, in cells, before any turn.
local function size(f: Frame): (number, number)
    if f.axis == 0 then
        return f.ex, f.ey
    elseif f.axis == 1 then
        return f.ez, f.ey
    end
    return f.ex, f.ez
end

-- Opens f as it is now, or reshapes it where it stands: the engine keeps the
-- opening that already has this centre. A quarter turn swaps which half is the
-- width, or the opening would stop matching the hole.
local function apply(f: Frame): number
    local cx, cy, cz = centre(f)
    local w, h = size(f)
    if f.rot % 2 == 1 then
        w, h = h, w
    end
    -- Said to the other rooms as it stands now; and one that leads to another
    -- room has no opening from here: the server gives it a mouth (0, not -1:
    -- it is not refused).
    if f.away then
        core.room_portal_set(f.name, cx, cy, cz, f.axis, f.sign, w / 2, h / 2)
        if f.far_room ~= "" then
            core.portal_close(cx, cy, cz)
            f.slot = -1
            return 0
        end
    end
    f.slot = core.portal_open(cx, cy, cz, f.axis, f.sign, f.rot, w / 2, h / 2, f.colour)
    -- An opening given a shape is that shape and nothing else: whether it
    -- scales is said again, and a window is by whoever calls this (wire).
    if f.slot >= 0 and f.scales then
        core.portal_scale(f.slot, true)
    end
    return f.slot
end

-- One row a frame: its name, its shape, the name it leads to, and whose it is
-- with what it was set to (a row from before 2026-11-11 stops at the third),
-- then whether the other rooms are told of it and the room and frame it leads to.
local function save()
    local rows = {}
    for _, f in frames do
        table.insert(rows, `{f.name}|{f.x} {f.y} {f.z} {f.ex} {f.ey} {f.ez} {f.axis} {f.sign} `
            .. `{f.rot}|{f.link or ""}|{if f.owner == "" then "-" else f.owner} {f.colour} `
            .. `{if f.scales then 1 else 0} {if f.window then 1 else 0}`
            .. `|{if f.away then 1 else 0}|{f.far_room}|{f.far_name}`)
    end
    core.storage_set("frames", table.concat(rows, ";"))
end

-- Tells the engine how f and the frame it leads to are joined, after either
-- was opened or reshaped (apply): a door both ways, or a window from the one
-- that is a window onto the other.
local function wire(f: Frame)
    if f.slot < 0 then
        return   -- it leads to another room: the server's to join
    end
    local g = named(f.link)
    if g == nil then
        core.portal_link(f.slot, -1)
    elseif f.window then
        core.portal_window(f.slot, g.slot)
    elseif g.window then
        core.portal_window(g.slot, f.slot)
    else
        core.portal_link(f.slot, g.slot)
    end
end

-- Whether that player sets f up: its builder, whoever the server lets set
-- portals up, and anybody at all for a frame that is nobody's.
local function may_edit(player: number, f: Frame): boolean
    return f.owner == "" or f.owner == core.player_key(player) or core.has_perm(player, "portals") == 1
end

-- f leads to g, and g to f; whatever either led to before leads nowhere. g nil
-- unlinks f. The engine keeps the same pairs by opening (core.portal_link).
-- A window is one way: the end that is one shows what the other faces. A frame
-- left with no other end is no window any more, and neither is the far end of
-- one that becomes a window itself.
local function link(f: Frame, g: Frame?)
    for _, h in frames do
        if h.link == f.name or (g and h.link == g.name) then
            h.link = nil
            if h ~= f and h ~= g then
                h.window = false
                apply(h)   -- its opening forgets the window it was half of
            end
        end
    end
    f.link = if g then g.name else nil
    if g then
        g.link = f.name
        if f.window then
            g.window = false
        end
        apply(g)
    else
        f.window = false
    end
    apply(f)
    wire(f)
end

local function facing(f: Frame): string
    if f.axis == 2 then
        return if f.sign > 0 then "up" else "down"
    end
    return `{if f.sign > 0 then "+" else "-"}{if f.axis == 0 then "z" else "x"}`
end

-- The menu. One panel for the mod, about the frame last clicked: a panel's
-- contents are the same on every screen it is on, so two players setting up
-- two frames at once see the one clicked last.
local PANEL = "portal:menu"
local shown: Frame? = nil
local viewer = 0

local function where(f: Frame): string
    local x, y, z = centre(f)
    return `{math.floor(x)} {math.floor(y)} {math.floor(z)}`
end

-- The frames the other rooms say, as core.room_portals lists them for f.
type RoomOffer = { room: string, name: string, state: string, why: string }
local function far_offers(f: Frame): { RoomOffer }
    local out: { RoomOffer } = {}
    for line in string.gmatch(core.room_portals(f.name), "[^\n]+") do
        local w = string.split(line, "\t")
        if w[1] == "P" then
            table.insert(out, { room = w[2] or "", name = w[3] or "", state = w[4] or "", why = w[5] or "" })
        end
    end
    return out
end

local function menu()
    local f = shown
    if f == nil then
        return
    end

    -- The others, two to a row, each a button that links this frame to it.
    local others: { Frame } = {}
    for _, g in frames do
        if g ~= f and not g.away then
            table.insert(others, g)
        end
    end
    table.sort(others, function(a: Frame, b: Frame): boolean
        return a.name < b.name
    end)
    -- To other rooms: for whoever sets the server's portals up, on a server
    -- that has rooms, and a frame that stands.
    local rooms = viewer ~= 0 and core.has_perm(viewer, "portals") == 1 and core.room() ~= "" and f.axis ~= 2
    local offers = if f.away then far_offers(f) else {}
    local rows = if f.away then #offers else math.ceil(#others / 2)
    if core.ui_define(PANEL, 13.0, 7.8 + rows + (if rooms or f.away then 1.1 else 0)) == 0 then
        return
    end

    local w, h = size(f)
    core.ui_box(PANEL, 0.2, 0.2, 12.6, 0.9, 40, 44, 56)
    core.ui_field(PANEL, "name", 0.3, 0.3, 7.0, 0.7, f.name)
    core.ui_label(PANEL, 7.6, 0.45, `{w} x {h}, {if f.axis == 2 then "lying" else "standing"}`)
    core.ui_label(PANEL, 0.5, 1.4, `front facing {facing(f)}, turned {f.rot * 90} degrees; `
        .. "click the name to change it")

    local other = named(f.link)
    if f.away and f.far_room ~= "" then
        local st = core.room_portal_state(f.name)
        if string.sub(st, 1, 6) == "linked" then
            if string.sub(st, -4) == "live" then
                core.ui_label(PANEL, 0.5, 2.0, `leads to {f.far_name}, in the room {f.far_room}`)
            else
                core.ui_label(PANEL, 0.5, 2.0, `leads to {f.far_name}, in the room {f.far_room}: shut while nobody is near`)
            end
        else
            core.ui_label(PANEL, 0.5, 2.0, `waits for {f.far_name}, in the room {f.far_room}`)
        end
    elseif f.away then
        core.ui_label(PANEL, 0.5, 2.0, "leads nowhere: link it to a frame of another room below")
    elseif other and f.window then
        core.ui_label(PANEL, 0.5, 2.0, `a window onto {other.name}, at {where(other)}: nothing crosses`)
    elseif other and other.window then
        core.ui_label(PANEL, 0.5, 2.0, `{other.name}, at {where(other)}, looks out of this one: nothing crosses`)
    elseif other then
        core.ui_label(PANEL, 0.5, 2.0, `leads to {other.name}, at {where(other)}`)
    else
        core.ui_label(PANEL, 0.5, 2.0, "leads nowhere: link it to another frame below")
    end

    core.ui_button(PANEL, "flip", 0.5, 2.9, 6.0, 0.9, "turn the front round")
    core.ui_button(PANEL, "turn", 6.7, 2.9, 5.8, 0.9, "turn a quarter")
    core.ui_button(PANEL, "shut", 0.5, 4.1, 6.0, 0.9, "close the portal")
    core.ui_button(PANEL, "close", 6.7, 4.1, 5.8, 0.9, "done")

    -- What it is besides a door: each button says what it is now and a click
    -- makes it the next.
    if f.colour == 1 then
        core.ui_button(PANEL, "colour", 0.5, 5.3, 3.9, 0.9, "colour: blue")
    elseif f.colour == 2 then
        core.ui_button(PANEL, "colour", 0.5, 5.3, 3.9, 0.9, "colour: orange")
    else
        core.ui_button(PANEL, "colour", 0.5, 5.3, 3.9, 0.9, "colour: green")
    end
    core.ui_button(PANEL, "scales", 4.6, 5.3, 3.9, 0.9, if f.scales then "scales: yes" else "scales: no")
    core.ui_button(PANEL, "window", 8.7, 5.3, 3.8, 0.9, if f.window then "a window: yes" else "a window: no")

    local y = 6.4
    if rooms or f.away then
        core.ui_button(PANEL, "rooms", 0.5, y, 12.0, 0.8,
            if f.away then "other rooms: yes -- they may link to it" else "other rooms: no")
        y += 1.1
    end
    if f.away then
        if #offers == 0 then
            core.ui_label(PANEL, 0.5, y + 0.2, "no other room has a frame said to the other rooms")
        end
        for i, o in offers do
            local tag = if o.state == "yours" then "> " elseif o.state == "busy" then "(linked elsewhere) " else ""
            local text = `{tag}{o.name}, in {o.room}`
            if o.why ~= "" then
                text ..= ` -- {o.why}`
            end
            core.ui_button(PANEL, `far:{o.room}\t{o.name}`, 0.5, y + (i - 1), 12.0, 0.8, text)
        end
    elseif #others == 0 then
        core.ui_label(PANEL, 0.5, y + 0.2, "no other frame is open to link it to")
    else
        core.ui_label(PANEL, 0.5, y + 0.2, "link it to:")
        core.ui_button(PANEL, "unlink", 6.7, y, 5.8, 0.8, "nothing")
        for i, g in others do
            local col, row = (i - 1) % 2, (i - 1) // 2
            local mark = if g == other then "> " else ""
            core.ui_button(PANEL, `link:{g.name}`, 0.5 + col * 6.2, y + 1.0 + row, 6.0, 0.8,
                `{mark}{g.name} ({where(g)})`)
        end
    end

    if viewer ~= 0 then
        core.ui_show(PANEL, true, viewer)
    end
end

-- Opens a whole frame that is not open yet, its front towards whoever finished
-- it. The open frame, or nil when the engine refused it.
local function try_open(f: Frame, player: number): Frame?
    for _, g in frames do
        if g.x == f.x and g.y == f.y and g.z == f.z and g.axis == f.axis then
            return g
        end
    end

    local px, py, pz = core.player_pos(player)
    if px ~= nil and py ~= nil and pz ~= nil then
        local cx, cy, cz = centre(f)
        local ahead = if f.axis == 0 then pz - cz elseif f.axis == 1 then px - cx else py - cy
        f.sign = if ahead < 0 then -1 else 1

        -- A floor or a ceiling has no up of its own, and which way its picture
        -- stands is the quarter turn. Turned so that whoever finished it, going
        -- in the way they are facing -- from where they stand to the middle --
        -- sees it upright: the portal's up times its sign along that heading,
        -- measured for every standing or lying other end. Floor to floor (and
        -- ceiling to ceiling) is half a turn whatever this does.
        if f.axis == 2 then
            local dx, dz = cx - px, cz - pz
            if math.abs(dx) > math.abs(dz) then
                f.rot = if dx < 0 then 1 else 3
            else
                f.rot = if dz * f.sign >= 0 then 0 else 2
            end
        end
    end

    if apply(f) < 0 then
        core.notice(player, "the frame is whole, but all of this world's openings are in use")
        return nil
    end

    -- Two frames make a portal, as they did when a world had two: the newest
    -- one that leads nowhere is the other end. Nothing to lead to leaves it
    -- leading nowhere, until the next frame or its menu.
    local lone: Frame? = nil
    for _, g in frames do
        if g.link == nil and not g.away then
            lone = g
        end
    end
    f.name = new_name()
    f.owner = core.player_key(player)
    table.insert(frames, f)
    link(f, lone)
    save()

    if lone then
        core.notice(player, `{f.name} is open and leads to {lone.name} -- right click its frame `
            .. "to set it up")
    else
        core.notice(player, `{f.name} is open -- build another frame for its other end`)
    end
    core.log(`{f.name} open at {f.x} {f.y} {f.z}, {f.ex} x {f.ey} x {f.ez}, facing {facing(f)}, `
        .. `turned {f.rot * 90}, opening {f.slot}, leads to {f.link or "nothing"}`)
    return f
end

local function shut(i: number, player: number, why: string)
    local f = frames[i]
    link(f, nil)
    if f.away then
        core.room_portal_close(f.name)
    end
    core.portal_close(centre(f))
    table.remove(frames, i)
    save()
    core.notice(player, `{f.name} closed: {why}`)
    core.log(`{f.name} at {f.x} {f.y} {f.z} closed: {why}`)
    if shown == f then
        shown = nil
        if viewer ~= 0 then
            core.ui_show(PANEL, false, viewer)
            viewer = 0
        end
    end
end

core.event.on("core:placed", function(ev)
    if ev.id ~= FRAME then
        return
    end
    local f = around(ev.x, ev.y, ev.z, FRAME)
    if f then
        try_open(f, ev.player)
    end
end)

core.event.on("core:broken", function(ev)
    if ev.id == FRAME then
        for i = #frames, 1, -1 do
            if on_ring(frames[i], ev.x, ev.y, ev.z) then
                shut(i, ev.player, "its frame is broken")
            end
        end
        return
    end

    -- The last thing in a whole frame's hole, dug out.
    for axis = 0, 2 do
        local f = hole(FRAME, axis, ev.x, ev.y, ev.z)
        if f then
            try_open(f, ev.player)
            return
        end
    end
end)

-- Claimed only for a whole frame. A right click on a frame block that is not
-- part of one is how the next block of it goes in.
core.event.on("core:used", function(ev)
    if ev.id ~= FRAME then
        return
    end

    local f: Frame? = nil
    for _, g in frames do
        if on_ring(g, ev.x, ev.y, ev.z) then
            f = g
        end
    end
    if f == nil then
        local whole = around(ev.x, ev.y, ev.z, FRAME)
        if whole == nil then
            return
        end
        f = try_open(whole, ev.player)
    end

    ev.cancel = true
    if f and not may_edit(ev.player, f) then
        local other = named(f.link)
        core.notice(ev.player, if other then `{f.name} leads to {other.name}` else `{f.name} leads nowhere`)
    elseif f then
        shown = f
        viewer = ev.player
        menu()
    end
end)

core.event.on("core:ui_click", function(ev)
    if ev.panel ~= PANEL then
        return
    end

    local f = shown
    if ev.id == "close" or f == nil then
        core.ui_show(PANEL, false, ev.player)
        if viewer == ev.player then
            viewer = 0
        end
        return
    end

    -- The one panel shows the frame clicked last, which may be somebody else's
    -- by now: a click from whoever may not set that one up closes their menu.
    if not may_edit(ev.player, f) then
        core.ui_show(PANEL, false, ev.player)
        return
    end

    viewer = ev.player
    if ev.id == "shut" then
        for i, g in frames do
            if g == f then
                shut(i, ev.player, "a right click on its frame opens it again")
                return
            end
        end
        return
    elseif ev.id == "flip" then
        f.sign = -f.sign
    elseif ev.id == "turn" then
        if f.away then
            core.notice(ev.player, "a portal to another room stands up and is not turned")
            menu()
            return
        end
        f.rot = (f.rot + 1) % 4
    elseif ev.id == "colour" then
        f.colour = (f.colour + 1) % 3
    elseif ev.id == "scales" then
        f.scales = not f.scales
        if f.scales then
            core.notice(ev.player, "it scales once its other end scales too: give both the same proportions")
        end
    elseif ev.id == "window" then
        local g = named(f.link)
        if g == nil then
            core.notice(ev.player, "link it to another frame first: a window shows what that one faces")
            menu()
            return
        end
        f.window = not f.window
        if f.window then
            g.window = false
        end
        apply(g)   -- as a door again, until wire says otherwise
    elseif ev.id == "name" then
        -- What a player typed: trimmed, without the two characters the saved
        -- rows are split on, and short enough for a button in the list.
        local name = string.gsub(string.gsub(ev.text, "[|;]", ""), "^%s*(.-)%s*$", "%1")
        local cut = utf8.offset(name, 33)   -- 32 characters, not a byte of one
        if cut then
            name = string.sub(name, 1, cut - 1)
        end
        if name == "" or name == f.name then
            menu()
            return
        end
        if named(name) then
            core.notice(ev.player, `there is already a frame called {name}`)
            menu()
            return
        end
        local was = f.name
        for _, g in frames do
            if g.link == was then
                g.link = name
            end
        end
        f.name = name
        if f.away then   -- the other rooms know it by its name: said again, linked again
            core.room_portal_close(was)
            apply(f)
            if f.far_room ~= "" then
                core.room_portal_link(f.name, f.far_room, f.far_name)
            end
        end
        save()
        core.notice(ev.player, `{was} is now called {name}`)
        core.log(`{was} renamed {name}`)
        menu()
        return
    elseif ev.id == "rooms" then
        if core.has_perm(ev.player, "portals") == 0 then
            return
        end
        if f.away then
            core.room_portal_close(f.name)
            f.away, f.far_room, f.far_name = false, "", ""
            core.log(`{f.name} no longer said to the other rooms`)
        elseif f.axis == 2 or f.rot ~= 0 then
            core.notice(ev.player, "a portal to another room stands up and is not turned")
            menu()
            return
        else
            link(f, nil)
            f.away = true
            core.log(`{f.name} said to the other rooms`)
        end
        apply(f)
        wire(f)
        save()
        menu()
        return
    elseif string.sub(ev.id, 1, 4) == "far:" then
        if core.has_perm(ev.player, "portals") == 0 or not f.away then
            return
        end
        local to = string.split(string.sub(ev.id, 5), "\t")
        local room, name = to[1] or "", to[2] or ""
        if room == f.far_room and name == f.far_name then
            room, name = "", ""   -- a click on the one it leads to lets go of it
        end
        local no = core.room_portal_link(f.name, room, name)
        if no ~= "" then
            core.notice(ev.player, no)
        else
            f.far_room, f.far_name = room, name
            apply(f)
            wire(f)
            save()
            core.log(if room == "" then `{f.name} leads to no room now` else `{f.name} now leads to {name} in room {room}`)
        end
        menu()
        return
    elseif ev.id == "unlink" then
        link(f, nil)
        save()
        menu()
        return
    elseif string.sub(ev.id, 1, 5) == "link:" then
        local g = named(string.sub(ev.id, 6))
        if f.away or (g and g.away) then
            -- one that leads to other rooms is no end of a pair here
        elseif g and g ~= f and g.link ~= nil and g.link ~= f.name and not may_edit(ev.player, g) then
            -- Linking takes g from whatever it led to: only for whoever sets g up.
            core.notice(ev.player, `{g.name} is somebody else's and leads elsewhere: they link it from their side`)
        elseif g and g ~= f then
            link(f, g)
            save()
            core.log(`{f.name} now leads to {g.name}`)
        end
        menu()
        return
    else
        return
    end

    apply(f)
    wire(f)
    save()
    menu()
end)

-- Opened again after a restart or a reload, on the first tick: a server's
-- openings do not exist before it. Each keeps its name and what it leads to;
-- which of the world's openings it takes does not matter any more.
local restored = false
core.event.on("core:tick", function(_)
    if restored then
        return
    end
    restored = true

    local kept = core.storage_get("frames")
    local leads: { [Frame]: string } = {}

    -- Kept before frames had names (2026-09-19): shapes alone, the pair first.
    -- Named now, and the first two lead to each other as they did.
    local old = core.storage_get("open")
    if kept == "" and old ~= "" then
        local shapes = string.split(old, ";")
        local names = {}
        for i in shapes do
            names[i] = new_name()
        end
        local rows = {}
        for i, shape in shapes do
            local partner = if i == 1 then names[2] elseif i == 2 then names[1] else nil
            table.insert(rows, `{names[i]}|{shape}|{partner or ""}`)
        end
        kept = table.concat(rows, ";")
        core.storage_set("open", "")
    end
    if kept == "" then
        return
    end
    for _, row in string.split(kept, ";") do
        local parts = string.split(row, "|")
        local n: { number } = {}
        for _, word in string.split(parts[2] or "", " ") do
            table.insert(n, tonumber(word) or 0)
        end
        if #parts >= 3 and #parts <= 7 and #n == 9 then
            local set = string.split(parts[4] or "", " ")
            local f: Frame = { x = n[1], y = n[2], z = n[3], ex = n[4], ey = n[5], ez = n[6],
                               axis = n[7], sign = n[8], rot = n[9], slot = -1, name = parts[1],
                               owner = if set[1] == nil or set[1] == "-" then "" else set[1],
                               colour = math.clamp(tonumber(set[2]) or 0, 0, 2),
                               scales = set[3] == "1", window = set[4] == "1",
                               away = parts[5] == "1", far_room = parts[6] or "", far_name = parts[7] or "" }
            if apply(f) >= 0 then
                table.insert(frames, f)
                leads[f] = parts[3]
            end
        end
    end
    for f, to in leads do
        if not f.away then
            link(f, named(to))
        end
    end
    -- And the links to other rooms, asked for again: the far frame may not be
    -- said yet, and the link waits for it (core.room_portal_link).
    for _, f in frames do
        if f.away and f.far_room ~= "" then
            core.room_portal_link(f.name, f.far_room, f.far_name)
        end
    end
    local pairs_open = 0
    for _, f in frames do
        if f.link then
            pairs_open += 1
        end
    end
    save()
    core.log(`portal: {#frames} frame(s) open again, {pairs_open // 2} linked pair(s)`)
end)

-- Every two seconds: what the server says each frame said to the other rooms
-- leads to. A link made from the other room, or let go of there, is taken
-- and kept; one still waiting for a room that has not said its frames is left.
local polled = 0.0
core.event.on("core:tick", function(ev)
    polled += ev.dt
    if polled < 2.0 or not restored then
        return
    end
    polled = 0.0
    local changed = false
    for _, f in frames do
        if f.away then
            local st = core.room_portal_state(f.name)
            local room, name = "", ""
            local word, rest = string.match(st, "^(%a+) (.*)$")
            if word == "linked" or word == "waiting" then
                local w = string.split(rest :: string, "\t")
                room, name = w[1] or "", w[2] or ""
            end
            if room ~= f.far_room or name ~= f.far_name then
                core.log(if room == "" then `{f.name} leads to no room now` else `{f.name} now leads to {name} in room {room}`)
                f.far_room, f.far_name = room, name
                apply(f)
                wire(f)
                changed = true
            end
        end
    end
    if changed then
        save()
        if shown ~= nil and viewer ~= 0 then
            menu()
        end
    end
end)

-- Portals to OTHER SERVERS: a ring of portal:server_frame, gold, round a hole
-- standing up. Anybody may build one; it leads nowhere -- nothing in the hole
-- -- until an admin sets it up. Their right click opens its menu: its name, the
-- friend servers (/friend, sharing portals) it is offered to, and the portals
-- those servers offer this one, each a button that links it there. The far
-- server takes the link when its portal is offered here, free and the same
-- size (core.far_portal_link says what the engine checks); then the hole is a
-- mouth onto that server, crossed on a ticket. Anybody else's right click says
-- where it leads. Three boxes in the menu let it do without one of the checks
-- each, with a warning: the other server heard from, a protocol in common,
-- holes of one size. Kept in core.storage "servers" and set up again on the
-- first ticks after a restart; the link itself the engine keeps in the world
-- file, and it lasts while both servers keep their signing keys, through
-- either being off.

local SERVER = core.register_node("portal:server_frame", 220, 170, 40, true, 7, "", 0, 1.0, "stone")
core.recipe_shaped("portal:server_frame", 8,
    "portal:frame portal:frame portal:frame",
    "portal:frame ore:ruby     portal:frame",
    "portal:frame portal:frame portal:frame")

type Far = { server: string, room: string, name: string }
local servers: { Frame } = {}
local offered: { [Frame]: { string } } = {}
local far_of: { [Frame]: Far } = {}       -- what it leads to, or asks for, as last read
local ignored: { [Frame]: { string } } = {}   -- the checks it does without

-- The checks a portal can do without, what the box says, and the warning.
local CHECKS = {
    { id = "offline", text = "do not need the other server online",
      warn = "the link waits and is kept while the other server is unreachable, maybe for ever" },
    { id = "protocol", text = "do not need a protocol in common",
      warn = "a client not speaking the other's protocol is refused at the crossing" },
    { id = "size", text = "do not need holes of one size",
      warn = "a body out of a smaller hole may come out in its frame and be stuck" },
}

local function server_named(name: string): Frame?
    for _, g in servers do
        if g.name == name then
            return g
        end
    end
    return nil
end

-- What a player typed for a name: trimmed, without the characters the saved
-- rows are split on, 32 characters at most.
local function clean(text: string): string
    local name = string.gsub(string.gsub(text, "[|;,\t]", ""), "^%s*(.-)%s*$", "%1")
    local cut = utf8.offset(name, 33)
    if cut then
        name = string.sub(name, 1, cut - 1)
    end
    return name
end

local function far_set(f: Frame): boolean
    local cx, cy, cz = centre(f)
    local w, h = size(f)
    return core.far_portal_set(f.name, cx, cy, cz, f.axis, f.sign, w / 2, h / 2,
        table.concat(offered[f] or {}, ","), table.concat(ignored[f] or {}, ",")) == 1
end

local function save_servers()
    local rows = {}
    for _, f in servers do
        local far = far_of[f]
        table.insert(rows, `{f.name}|{f.x} {f.y} {f.z} {f.ex} {f.ey} {f.ez} {f.axis} {f.sign}|`
            .. `{table.concat(offered[f] or {}, ",")}|`
            .. (if far then `{far.server}\t{far.room}\t{far.name}` else "")
            .. `|{table.concat(ignored[f] or {}, ",")}`)
    end
    core.storage_set("servers", table.concat(rows, ";"))
end

-- "linked", "asking" or "none", the far end, and why a link came undone.
local function far_state(f: Frame): (string, Far?, string)
    local st = core.far_portal_state(f.name)
    local word, rest = string.match(st, "^(%a+)%s?(.*)$")
    local parts = string.split(rest or "", "\t")
    if (word == "linked" or word == "asking") and #parts == 3 then
        return word, { server = parts[1], room = parts[2], name = parts[3] }, ""
    end
    return "none", nil, parts[2] or parts[1] or ""
end

-- The friend servers and what they offer, as core.far_portals says.
type Server = { name: string, status: string, engine: string }
type Offer = { server: string, room: string, name: string, state: string, why: string }
local function far_list(f: Frame): ({ Server }, { Offer })
    local list, offers = {}, {}
    for line in string.gmatch(core.far_portals(f.name), "[^\n]+") do
        local w = string.split(line, "\t")
        if w[1] == "S" then
            table.insert(list, { name = w[2], status = w[3] or "", engine = w[4] or "" })
        elseif w[1] == "P" then
            table.insert(offers, { server = w[2], room = w[3] or "", name = w[4] or "", state = w[5] or "",
                                   why = w[6] or "" })
        end
    end
    return list, offers
end

local SPANEL = "portal:server_menu"
local sshown: Frame? = nil
local sviewer = 0
local choices: { Offer } = {}    -- the link buttons on the menu now, by number
local friends_shown: { string } = {}

local function server_menu()
    local f = sshown
    if f == nil or sviewer == 0 then
        return
    end
    local list, offers = far_list(f)
    local mine = offered[f] or {}
    local function is_offered(name: string): boolean
        return table.find(mine, name) ~= nil
    end
    choices = {}
    for _, o in offers do
        if is_offered(o.server) and #choices < 12 then
            table.insert(choices, o)
        end
    end
    friends_shown = {}
    for _, s in list do
        if #friends_shown < 9 then
            table.insert(friends_shown, s.name)
        end
    end

    local frows = math.max(1, math.ceil(#friends_shown / 3))
    local skipped = ignored[f] or {}
    local ctop = 4.0 + frows
    local top = ctop + 0.6 + #CHECKS * 0.9 + #skipped * 0.6 + 0.4
    if core.ui_define(SPANEL, 18.0, top + 1.2 + math.max(1, #choices) * 0.9) == 0 then
        return
    end
    local w, h = size(f)
    core.ui_box(SPANEL, 0.2, 0.2, 17.6, 0.9, 56, 44, 20)
    core.ui_field(SPANEL, "name", 0.3, 0.3, 9.0, 0.7, f.name)
    core.ui_label(SPANEL, 9.6, 0.45, `{w} x {h}, to another server, facing {facing(f)}`)

    local st, far, why = far_state(f)
    if st == "linked" and far then
        core.ui_label(SPANEL, 0.5, 1.4, `leads to {far.name} on {far.server}`)
    elseif st == "asking" and far then
        core.ui_label(SPANEL, 0.5, 1.4, `asked {far.server} for {far.name}: waiting for it to take the link`)
    else
        core.ui_label(SPANEL, 0.5, 1.4, if why ~= "" then `leads nowhere: {why}` else "leads nowhere")
    end

    core.ui_button(SPANEL, "flip", 0.5, 2.1, 5.5, 0.8, "turn the front round")
    core.ui_button(SPANEL, "unlink", 6.2, 2.1, 5.5, 0.8, "unlink")
    core.ui_button(SPANEL, "close", 11.9, 2.1, 5.6, 0.8, "done")

    if #friends_shown == 0 then
        core.ui_label(SPANEL, 0.5, 3.3, "no friend server shares portals (/friend add ... portals, --federation)")
    else
        core.ui_label(SPANEL, 0.5, 3.3, "available to (click a server to add or take it off):")
        for i, name in friends_shown do
            local s = list[i]
            local col, row = (i - 1) % 3, (i - 1) // 3
            local mark = if is_offered(name) then "[x] " else "[ ] "
            local note = if s.status == "ok" then "" else " (not reachable)"
            core.ui_button(SPANEL, `offer:{i}`, 0.5 + col * 5.8, 3.9 + row, 5.6, 0.8, `{mark}{name}{note}`)
        end
    end

    core.ui_label(SPANEL, 0.5, ctop, "checks before a link (click to do without one):")
    for i, c in CHECKS do
        local off = table.find(skipped, c.id) ~= nil
        core.ui_button(SPANEL, `check:{i}`, 0.5, ctop + 0.6 + (i - 1) * 0.9, 17.0, 0.8,
            `{if off then "[x]" else "[ ]"} {c.text}`)
    end
    local wy = ctop + 0.6 + #CHECKS * 0.9
    for _, c in CHECKS do
        if table.find(skipped, c.id) then
            core.ui_label(SPANEL, 0.5, wy, `WARNING: {c.warn}`)
            wy += 0.6
        end
    end

    core.ui_label(SPANEL, 0.5, top, "their portals available to this server -- click one to connect:")
    if #choices == 0 then
        core.ui_label(SPANEL, 0.5, top + 0.8, if #mine == 0 then "tick a server above first"
            else "none yet: the server must offer a portal to this one too")
    end
    for i, o in choices do
        local tag = if o.state == "yours" then "> linked: "
            elseif o.state == "busy" then "busy: "
            elseif o.why ~= "" then "no: "
            else ""
        local text = `{tag}{o.name}, on {o.server}{if o.room ~= "" then ` ({o.room})` else ""}`
        if o.why ~= "" and o.state ~= "yours" then
            text ..= ` -- {o.why}`
        end
        core.ui_button(SPANEL, `link:{i}`, 0.5, top + 0.8 + (i - 1) * 0.9, 17.0, 0.8, string.sub(text, 1, 120))
    end
    core.ui_show(SPANEL, true, sviewer)
end

-- A whole server frame: standing, its front towards whoever finished it, and
-- a name. Nil for a lying one.
local function server_open(f: Frame, player: number): Frame?
    for _, g in servers do
        if g.x == f.x and g.y == f.y and g.z == f.z and g.axis == f.axis then
            return g
        end
    end
    if f.axis == 2 then
        core.notice(player, "a portal to another server stands up: build it as a wall, not a floor")
        return nil
    end
    local px, _, pz = core.player_pos(player)
    if px ~= nil and pz ~= nil then
        local cx, _, cz = centre(f)
        local ahead = if f.axis == 0 then pz - cz else px - cx
        f.sign = if ahead < 0 then -1 else 1
    end
    local n = (tonumber(core.storage_get("server_count")) or 0) + 1
    core.storage_set("server_count", tostring(n))
    f.name = `server portal {n}`
    if not far_set(f) then
        core.notice(player, "this server has no portals to other servers (no world running yet?)")
        return nil
    end
    table.insert(servers, f)
    offered[f] = {}
    save_servers()
    core.notice(player, `{f.name} is whole -- an admin's right click sets up where it leads`)
    core.log(`{f.name} built at {f.x} {f.y} {f.z}, {f.ex} x {f.ey} x {f.ez}, facing {facing(f)}`)
    return f
end

local function server_shut(i: number, player: number, why: string)
    local f = servers[i]
    core.far_portal_close(f.name)
    table.remove(servers, i)
    offered[f], far_of[f], ignored[f] = nil, nil, nil
    save_servers()
    core.notice(player, `{f.name} closed: {why}`)
    core.log(`{f.name} at {f.x} {f.y} {f.z} closed: {why}`)
    if sshown == f then
        sshown = nil
        if sviewer ~= 0 then
            core.ui_show(SPANEL, false, sviewer)
            sviewer = 0
        end
    end
end

core.event.on("core:placed", function(ev)
    if ev.id == SERVER then
        local f = around(ev.x, ev.y, ev.z, SERVER)
        if f then
            server_open(f, ev.player)
        end
    end
end)

core.event.on("core:broken", function(ev)
    if ev.id == SERVER then
        for i = #servers, 1, -1 do
            if on_ring(servers[i], ev.x, ev.y, ev.z) then
                server_shut(i, ev.player, "its frame is broken")
            end
        end
        return
    end
    for axis = 0, 1 do
        local f = hole(SERVER, axis, ev.x, ev.y, ev.z)
        if f then
            server_open(f, ev.player)
            return
        end
    end
end)

core.event.on("core:used", function(ev)
    if ev.id ~= SERVER then
        return
    end
    local f: Frame? = nil
    for _, g in servers do
        if on_ring(g, ev.x, ev.y, ev.z) then
            f = g
        end
    end
    if f == nil then
        local whole = around(ev.x, ev.y, ev.z, SERVER)
        if whole == nil then
            return
        end
        f = server_open(whole, ev.player)
    end
    ev.cancel = true
    if f == nil then
        return
    end
    if core.has_perm(ev.player, "admin") == 0 then
        local st, far = far_state(f)
        core.notice(ev.player, if st == "linked" and far then `{f.name} leads to {far.name} on {far.server}`
            else `{f.name} leads nowhere yet: an admin sets it up`)
        return
    end
    sshown = f
    sviewer = ev.player
    server_menu()
end)

core.event.on("core:ui_click", function(ev)
    if ev.panel ~= SPANEL then
        return
    end
    local f = sshown
    if ev.id == "close" or f == nil or core.has_perm(ev.player, "admin") == 0 then
        core.ui_show(SPANEL, false, ev.player)
        if sviewer == ev.player then
            sviewer = 0
        end
        return
    end
    sviewer = ev.player
    local id = ev.id
    if id == "flip" then
        f.sign = -f.sign
        far_set(f)
    elseif id == "unlink" then
        core.far_portal_link(f.name, "", "", "")
        far_of[f] = nil
    elseif id == "name" then
        local name = clean(ev.text)
        if name ~= "" and name ~= f.name then
            if server_named(name) then
                core.notice(ev.player, `there is already a portal called {name}`)
            elseif far_of[f] then
                core.notice(ev.player, "unlink it first: the far server knows it by its name")
            else
                core.far_portal_close(f.name)
                core.log(`{f.name} renamed {name}`)
                f.name = name
                far_set(f)
            end
        end
    elseif string.sub(id, 1, 6) == "offer:" then
        local name = friends_shown[tonumber(string.sub(id, 7)) or 0]
        if name then
            local mine = offered[f] or {}
            local at = table.find(mine, name)
            if at then
                table.remove(mine, at)
            else
                table.insert(mine, name)
            end
            offered[f] = mine
            far_set(f)
            core.log(`{f.name} offered to {if #mine == 0 then "nobody" else table.concat(mine, ", ")}`)
        end
    elseif string.sub(id, 1, 6) == "check:" then
        local c = CHECKS[tonumber(string.sub(id, 7)) or 0]
        if c then
            local mine = ignored[f] or {}
            local at = table.find(mine, c.id)
            if at then
                table.remove(mine, at)
                core.notice(ev.player, `{f.name} checks again: {c.text}`)
            else
                table.insert(mine, c.id)
                core.notice(ev.player, `WARNING: {c.warn}`)
            end
            ignored[f] = mine
            far_set(f)
            core.log(`{f.name} does without: {if #mine == 0 then "nothing" else table.concat(mine, ", ")}`)
        end
    elseif string.sub(id, 1, 5) == "link:" then
        local o = choices[tonumber(string.sub(id, 6)) or 0]
        if o then
            local no = core.far_portal_link(f.name, o.server, o.room, o.name)
            if no ~= "" then
                core.notice(ev.player, `cannot connect to {o.name} on {o.server}: {no}`)
            else
                far_of[f] = { server = o.server, room = o.room, name = o.name }
                core.notice(ev.player, `{f.name}: asked {o.server} for {o.name}`)
                core.log(`{f.name} asks {o.server} for {o.room}/{o.name}`)
            end
        end
    end
    save_servers()
    server_menu()
end)

-- Restored on the first ticks that have a world to open them in (their links
-- are the engine's, kept in the world file). Every two seconds what each
-- leads to is read back -- the far server takes a link, or lets go of it --
-- and an open menu shown again.
local servers_restored = false
local restore_tries = 0
local next_look = 0.0
local clock = 0.0
core.event.on("core:tick", function(ev)
    clock += ev.dt or 0.05
    if not servers_restored then
        restore_tries += 1
        local rows: { Frame } = {}
        for _, row in string.split(core.storage_get("servers"), ";") do
            local parts = string.split(row, "|")
            local n: { number } = {}
            for _, word in string.split(parts[2] or "", " ") do
                table.insert(n, tonumber(word) or 0)
            end
            if #parts >= 3 and #n == 8 then
                local f: Frame = { x = n[1], y = n[2], z = n[3], ex = n[4], ey = n[5], ez = n[6], axis = n[7],
                                   sign = n[8], rot = 0, slot = -1, name = parts[1] }
                offered[f] = if parts[3] == "" then {} else string.split(parts[3], ",")
                ignored[f] = if (parts[5] or "") == "" then {} else string.split(parts[5], ",")
                table.insert(rows, f)
            end
        end
        -- No world to open them in yet: asked again on the next tick.
        local first = rows[1]
        if first and not far_set(first) and restore_tries < 200 then
            return
        end
        servers_restored = true
        for i, f in rows do
            if i == 1 or far_set(f) then
                table.insert(servers, f)
            end
        end
        if #rows > 0 then
            core.log(`portal: {#servers} portal(s) to other servers set up again`)
        end
        return
    end
    if clock < next_look then
        return
    end
    next_look = clock + 2.0
    local changed = false
    for _, f in servers do
        local _, now_far = far_state(f)
        local was = far_of[f]
        if (now_far == nil) ~= (was == nil) or (now_far and was and (now_far.server ~= was.server
            or now_far.room ~= was.room or now_far.name ~= was.name)) then
            far_of[f] = now_far   -- taken, or let go, by the far side
            changed = true
        end
    end
    if changed then
        save_servers()
    end
    if sshown and sviewer ~= 0 then
        server_menu()
    end
end)
