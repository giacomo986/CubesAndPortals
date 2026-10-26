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
-- and buttons to turn the front round, turn it a quarter, or close it.
--
-- A world has 16 openings (core.portal_open), so a seventeenth frame is told
-- there is no room. Breaking a block of a frame closes it; a right click on a
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
}

-- The frames that are open now: 16 at most, the world's openings.
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

local function is_frame(axis: number, x: number, y: number, z: number, u: number, v: number): boolean
    local cx, cy, cz = cell(axis, x, y, z, u, v)
    return core.get_block(cx, cy, cz) == FRAME
end

local STEPS = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }

-- The hole of a whole frame that the air at x, y, z belongs to, in plane
-- `axis`, or nil. The air is filled across the plane from there, four ways; more
-- than MAX across either way is the open sky and not a hole. What it filled has
-- to be a full rectangle, and every cell round it, corners too, a frame.
local function hole(axis: number, x: number, y: number, z: number): Frame?
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
        if not is_frame(axis, x, y, z, u, v0 - 1) or not is_frame(axis, x, y, z, u, v1 + 1) then
            return nil
        end
    end
    for v = v0, v1 do
        if not is_frame(axis, x, y, z, u0 - 1, v) or not is_frame(axis, x, y, z, u1 + 1, v) then
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
             slot = -1, name = "" }
end

-- The whole frame the frame block at x, y, z is part of, or nil: a hole next to
-- it, straight or corner-wise, in any of the three planes.
local function around(x: number, y: number, z: number): Frame?
    for axis = 0, 2 do
        for du = -1, 1 do
            for dv = -1, 1 do
                if du ~= 0 or dv ~= 0 then
                    local cx, cy, cz = cell(axis, x, y, z, du, dv)
                    local f = hole(axis, cx, cy, cz)
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
    f.slot = core.portal_open(cx, cy, cz, f.axis, f.sign, f.rot, w / 2, h / 2)
    return f.slot
end

-- One row a frame: its name, its shape, and the name it leads to.
local function save()
    local rows = {}
    for _, f in frames do
        table.insert(rows, `{f.name}|{f.x} {f.y} {f.z} {f.ex} {f.ey} {f.ez} {f.axis} {f.sign} `
            .. `{f.rot}|{f.link or ""}`)
    end
    core.storage_set("frames", table.concat(rows, ";"))
end

-- f leads to g, and g to f; whatever either led to before leads nowhere. g nil
-- unlinks f. The engine keeps the same pairs by opening (core.portal_link).
local function link(f: Frame, g: Frame?)
    for _, h in frames do
        if h.link == f.name or (g and h.link == g.name) then
            h.link = nil
        end
    end
    f.link = if g then g.name else nil
    if g then
        g.link = f.name
    end
    core.portal_link(f.slot, if g then g.slot else -1)
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

local function menu()
    local f = shown
    if f == nil then
        return
    end

    -- The others, two to a row, each a button that links this frame to it.
    local others: { Frame } = {}
    for _, g in frames do
        if g ~= f then
            table.insert(others, g)
        end
    end
    table.sort(others, function(a: Frame, b: Frame): boolean
        return a.name < b.name
    end)
    local rows = math.ceil(#others / 2)
    if core.ui_define(PANEL, 13.0, 6.6 + rows) == 0 then
        return
    end

    local w, h = size(f)
    core.ui_box(PANEL, 0.2, 0.2, 12.6, 0.9, 40, 44, 56)
    core.ui_field(PANEL, "name", 0.3, 0.3, 7.0, 0.7, f.name)
    core.ui_label(PANEL, 7.6, 0.45, `{w} x {h}, {if f.axis == 2 then "lying" else "standing"}`)
    core.ui_label(PANEL, 0.5, 1.4, `front facing {facing(f)}, turned {f.rot * 90} degrees; `
        .. "click the name to change it")

    local other = named(f.link)
    if other then
        core.ui_label(PANEL, 0.5, 2.0, `leads to {other.name}, at {where(other)}`)
    else
        core.ui_label(PANEL, 0.5, 2.0, "leads nowhere: link it to another frame below")
    end

    core.ui_button(PANEL, "flip", 0.5, 2.9, 6.0, 0.9, "turn the front round")
    core.ui_button(PANEL, "turn", 6.7, 2.9, 5.8, 0.9, "turn a quarter")
    core.ui_button(PANEL, "shut", 0.5, 4.1, 6.0, 0.9, "close the portal")
    core.ui_button(PANEL, "close", 6.7, 4.1, 5.8, 0.9, "done")

    if #others == 0 then
        core.ui_label(PANEL, 0.5, 5.4, "no other frame is open to link it to")
    else
        core.ui_label(PANEL, 0.5, 5.4, "link it to:")
        core.ui_button(PANEL, "unlink", 6.7, 5.2, 5.8, 0.8, "nothing")
        for i, g in others do
            local col, row = (i - 1) % 2, (i - 1) // 2
            local mark = if g == other then "> " else ""
            core.ui_button(PANEL, `link:{g.name}`, 0.5 + col * 6.2, 6.2 + row, 6.0, 0.8,
                `{mark}{g.name} ({where(g)})`)
        end
    end

    if viewer ~= 0 then
        core.ui_show(PANEL, true, viewer)
    end
end

-- Opens a whole frame that is not open yet, its front towards whoever finished
-- it. The open frame, or nil when there was no room.
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
        if g.link == nil then
            lone = g
        end
    end
    f.name = new_name()
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
    local f = around(ev.x, ev.y, ev.z)
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
        local f = hole(axis, ev.x, ev.y, ev.z)
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
        local whole = around(ev.x, ev.y, ev.z)
        if whole == nil then
            return
        end
        f = try_open(whole, ev.player)
    end

    ev.cancel = true
    if f then
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
        f.rot = (f.rot + 1) % 4
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
        save()
        core.notice(ev.player, `{was} is now called {name}`)
        core.log(`{was} renamed {name}`)
        menu()
        return
    elseif ev.id == "unlink" then
        link(f, nil)
        save()
        menu()
        return
    elseif string.sub(ev.id, 1, 5) == "link:" then
        local g = named(string.sub(ev.id, 6))
        if g and g ~= f then
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
        if #parts == 3 and #n == 9 then
            local f: Frame = { x = n[1], y = n[2], z = n[3], ex = n[4], ey = n[5], ez = n[6],
                               axis = n[7], sign = n[8], rot = n[9], slot = -1, name = parts[1] }
            if apply(f) >= 0 then
                table.insert(frames, f)
                leads[f] = parts[3]
            end
        end
    end
    for f, to in leads do
        link(f, named(to))
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
