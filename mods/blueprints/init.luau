--!strict
-- blueprints: a build copied, carried as an item, laid out and built again.
--
-- An EMPTY BLUEPRINT (paper and lapis) is used on two corners of a box, at
-- most 32 wide and long and 24 high: what is in it, block and turn, is copied,
-- and the empty one becomes a BLUEPRINT of it. A blueprint is an item like
-- any other -- given, put in a chest, sold -- and knows which one it is by its
-- number (core.held_id); its blocks are kept here, in storage.
--
-- Used, a blueprint opens its panel: lay it out with its corner on the block
-- clicked, turned to where the player looks; copy it (an empty blueprint
-- spent); take the layout away. Laid out, it is a GHOST on the player's
-- screen (core.ghost_cell): each block still to put, seen through in its
-- colour (core.node_colour, which a player's machine may not know), gone as the right block goes in; done when none is left. Built by
-- hand, or:
--
--   by the village's builder (mods/villagers), who asks for the layout's
--   cells with "blueprints:ask" and gets them as "blueprints:cells";
--   by a robot ordered to Build (mods/helpers), the same way.
--
-- One layout a player.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local MAX_W, MAX_H = 32, 24
local PART = 3800   -- a storage value is at most 4096 bytes

core.event.define("blueprints:ask", { fields = { id = "number", x = "number", y = "number", z = "number",
                                                  t = "number", owner = "string" }, cancellable = false })
core.event.define("blueprints:cells", { fields = { id = "number", x = "number", y = "number", z = "number",
                                                    t = "number", owner = "string", w = "number", d = "number",
                                                    cells = "string" }, cancellable = false })

if serving then
    core.net.open()
    core.register_item("blueprints:empty", 16, "", 0, 1.0, "")
    core.register_item("blueprints:plan", 1, "", 0, 1.0, "")
    -- In the hand: a rolled sheet, blank or drawn on (tools/make_held_models.py).
    core.register_model("blueprints:empty_held", "empty_held.gltf")
    core.register_model("blueprints:plan_held", "plan_held.gltf")
    core.recipe_shapeless("blueprints:empty", 2, "cane:paper cane:paper ore:lapis")

    -- ---------------------------------------------------------- the store

    -- A blueprint: its size, and its cells x fastest, then z, then y, as
    -- indices into its palette of "name facing" ("" air).
    type Plan = { w: number, h: number, d: number, palette: { string }, cells: { number } }
    local cache: { [number]: Plan } = {}

    local function save(id: number, p: Plan)
        -- Runs: "index*count" or "index", comma separated, in parts.
        local runs = {}
        local i = 1
        while i <= #p.cells do
            local j = i
            while j < #p.cells and p.cells[j + 1] == p.cells[i] do
                j += 1
            end
            table.insert(runs, if j > i then `{p.cells[i]}*{j - i + 1}` else tostring(p.cells[i]))
            i = j + 1
        end
        local text = table.concat(runs, ",")
        local parts = math.max(1, math.ceil(#text / PART))
        for k = 1, parts do
            core.storage_set(`bp {id} {k}`, string.sub(text, (k - 1) * PART + 1, k * PART))
        end
        core.storage_set(`bp {id}`, `{p.w} {p.h} {p.d} {parts}|{table.concat(p.palette, ";")}`)
        cache[id] = p
    end

    local function load(id: number): Plan?
        if cache[id] then
            return cache[id]
        end
        local head = core.storage_get(`bp {id}`)
        local w, h, d, parts, pal = string.match(head, "^(%d+) (%d+) (%d+) (%d+)|(.*)$")
        if w == nil then
            return nil
        end
        local text = {}
        for k = 1, tonumber(parts) :: number do
            table.insert(text, core.storage_get(`bp {id} {k}`))
        end
        local cells = {}
        for run in string.gmatch(table.concat(text), "[^,]+") do
            local v, n = string.match(run, "^(%d+)%*(%d+)$")
            if v then
                for _ = 1, tonumber(n) :: number do
                    table.insert(cells, tonumber(v) :: number)
                end
            else
                table.insert(cells, tonumber(run) :: number)
            end
        end
        local p: Plan = { w = tonumber(w) :: number, h = tonumber(h) :: number, d = tonumber(d) :: number,
                          palette = string.split(pal :: string, ";"), cells = cells }
        cache[id] = p
        return p
    end

    -- A box of the world, copied.
    local function copy(x1: number, y1: number, z1: number, x2: number, y2: number, z2: number): Plan
        local p: Plan = { w = x2 - x1 + 1, h = y2 - y1 + 1, d = z2 - z1 + 1, palette = { "" }, cells = {} }
        local index: { [string]: number } = { [""] = 1 }
        for y = y1, y2 do
            for z = z1, z2 do
                for x = x1, x2 do
                    local b = core.get_block(x, y, z)
                    local key = if b == 0 then "" else `{core.node_name(b)} {core.get_facing(x, y, z)}`
                    local i = index[key]
                    if i == nil then
                        table.insert(p.palette, key)
                        i = #p.palette
                        index[key] = i
                    end
                    table.insert(p.cells, i - 1)
                end
            end
        end
        return p
    end

    -- ------------------------------------------------------ in the world

    -- Where a cell of the plan goes, laid out with its corner box at x0, z0
    -- turned t: villagers' own frame (its `at`), so its builder and this
    -- agree. The turn of a block turned upright, or wall-mounted, turns too.
    local WALL_FACE = { [0] = 4, 0, 5, 1 }
    local FACE_TURN = { [4] = 0, [0] = 1, [5] = 2, [1] = 3 }
    local function place(p: Plan, x0: number, z0: number, t: number, lx: number, lz: number): (number, number)
        if t == 0 then
            return x0 + lx, z0 + lz
        elseif t == 1 then
            return x0 + lz, z0 + p.w - 1 - lx
        elseif t == 2 then
            return x0 + p.w - 1 - lx, z0 + p.d - 1 - lz
        end
        return x0 + p.d - 1 - lz, z0 + lx
    end
    local function turned(name: string, f: number, t: number): number
        local turns = core.node_turns(core.node_id(name))
        if turns == 4 then
            return (f + t) % 4
        elseif turns == 6 and FACE_TURN[f] ~= nil then
            return WALL_FACE[(FACE_TURN[f] + t) % 4]
        end
        return f
    end

    -- Every cell of a plan laid out: x, y, z, name ("" air), facing.
    local function cells_of(p: Plan, x0: number, y0: number, z0: number, t: number): { { any } }
        local out = {}
        local i = 1
        for ly = 0, p.h - 1 do
            for lz = 0, p.d - 1 do
                for lx = 0, p.w - 1 do
                    local key = p.palette[p.cells[i] + 1] or ""
                    i += 1
                    local x, z = place(p, x0, z0, t, lx, lz)
                    if key == "" then
                        table.insert(out, { x, y0 + ly, z, "", 0 })
                    else
                        local name, f = string.match(key, "^(%S+) (%d+)$")
                        table.insert(out, { x, y0 + ly, z, name, turned(name :: string, tonumber(f) :: number, t) })
                    end
                end
            end
        end
        return out
    end

    -- The answer to "blueprints:ask": "x,y,z,name,facing;..." with air as "".
    core.event.on("blueprints:ask", function(ev)
        local p = load(ev.id)
        if p == nil then
            return
        end
        local list = {}
        for _, c in cells_of(p, ev.x, ev.y, ev.z, ev.t) do
            table.insert(list, `{c[1]},{c[2]},{c[3]},{c[4]},{c[5]}`)
        end
        core.event.emit("blueprints:cells", { id = ev.id, x = ev.x, y = ev.y, z = ev.z, t = ev.t, owner = ev.owner,
                                              w = if ev.t % 2 == 0 then p.w else p.d,
                                              d = if ev.t % 2 == 0 then p.d else p.w,
                                              cells = table.concat(list, ";") })
    end)

    -- -------------------------------------------------------- layouts

    type Site = { id: number, x: number, y: number, z: number, t: number, left: number }
    local sites: { [string]: Site } = {}   -- by player name
    local function site_of(name: string): Site?
        local s = sites[name]
        if s == nil then
            local id, x, y, z, t = string.match(core.storage_get(`site {name}`), "^(%d+) (%-?%d+) (%-?%d+) (%-?%d+) (%d)$")
            if id then
                s = { id = tonumber(id) :: number, x = tonumber(x) :: number, y = tonumber(y) :: number,
                      z = tonumber(z) :: number, t = tonumber(t) :: number, left = -1 }
                sites[name] = s
            end
        end
        return s
    end
    local function player_named(name: string): number?
        for _, q in core.players() do
            if core.player_name(q) == name then
                return q
            end
        end
        return nil
    end

    local function send(p: number, text: string)
        if drawing then
            core.event.emit("blueprints:shown", { text = text })
        else
            core.net.send(p, text)
        end
    end

    -- How many solid cells of a layout are not yet right.
    local function left_of(s: Site): number
        local plan = load(s.id)
        if plan == nil then
            return 0
        end
        local n = 0
        for _, c in cells_of(plan, s.x, s.y, s.z, s.t) do
            if c[4] ~= "" then
                local want = core.node_id(c[4])
                n += if want ~= 0 and core.get_block(c[1], c[2], c[3]) ~= want then 1 else 0
            end
        end
        return n
    end

    -- The ghost: every solid cell not yet right, in messages of at most 500.
    local function show(p: number)
        send(p, "c")
        local s = site_of(core.player_name(p))
        local plan = s and load(s.id)
        if s == nil or plan == nil then
            return
        end
        local line, left = "g", 0
        for _, c in cells_of(plan, s.x, s.y, s.z, s.t) do
            if c[4] ~= "" then
                local want = core.node_id(c[4])
                if want ~= 0 and core.get_block(c[1], c[2], c[3]) ~= want then
                    left += 1
                    local item = ` {c[1]},{c[2]},{c[3]},{math.max(1, core.node_colour(want))}`
                    if #line + #item > 500 then
                        send(p, line)
                        line = "g"
                    end
                    line ..= item
                end
            end
        end
        if line ~= "g" then
            send(p, line)
        end
        s.left = left
    end

    -- A block put or taken inside a layout: its ghost follows.
    local function touched(x: number, y: number, z: number)
        for name, s in sites do
            local plan = load(s.id)
            local q = player_named(name)
            if plan and q then
                local fw, fd = if s.t % 2 == 0 then plan.w else plan.d, if s.t % 2 == 0 then plan.d else plan.w
                if x >= s.x and x < s.x + fw and z >= s.z and z < s.z + fd and y >= s.y and y < s.y + plan.h then
                    show(q)
                    if s.left == 0 then
                        core.notice(q, "The blueprint is built.")
                        sites[name] = nil
                        core.storage_set(`site {name}`, "")
                        send(q, "c")
                    end
                end
            end
        end
    end
    core.event.on("core:placed", function(ev) touched(ev.x, ev.y, ev.z) end)
    core.event.on("core:broken", function(ev) touched(ev.x, ev.y, ev.z) end)
    -- And what a builder or a robot puts, now and then; and a player back
    -- (s.left is -1 for a layout read from storage), their ghost again.
    local since = 0
    core.event.on("core:tick", function(ev)
        since += ev.dt
        if since < 3 then
            return
        end
        since = 0
        for _, q in core.players() do
            local name = core.player_name(q)
            local s = site_of(name)
            if s and left_of(s) ~= s.left then
                local before = s.left
                show(q)
                if s.left == 0 and before ~= 0 then
                    core.notice(q, "The blueprint is built.")
                    sites[name] = nil
                    core.storage_set(`site {name}`, "")
                    send(q, "c")
                end
            end
        end
    end)

    -- ------------------------------------------------------- the hands

    local corner: { [number]: { number } } = {}
    local panel: { [number]: { id: number, x: number, y: number, z: number, t: number } } = {}

    core.event.on("core:used", function(ev)
        if ev.held == "blueprints:empty" then
            ev.cancel = true
            local a = corner[ev.player]
            if a == nil then
                corner[ev.player] = { ev.x, ev.y, ev.z }
                core.notice(ev.player, "One corner; now the other.")
                return
            end
            corner[ev.player] = nil
            local x1, x2 = math.min(a[1], ev.x), math.max(a[1], ev.x)
            local y1, y2 = math.min(a[2], ev.y), math.max(a[2], ev.y)
            local z1, z2 = math.min(a[3], ev.z), math.max(a[3], ev.z)
            if x2 - x1 >= MAX_W or z2 - z1 >= MAX_W or y2 - y1 >= MAX_H then
                core.notice(ev.player, `At most {MAX_W} by {MAX_H} high by {MAX_W}.`)
                return
            end
            local id = (tonumber(core.storage_get("next")) or 1)
            if id > 131071 then
                core.notice(ev.player, "No more blueprints can be made in this world.")
                return
            end
            core.storage_set("next", tostring(id + 1))
            save(id, copy(x1, y1, z1, x2, y2, z2))
            core.take_item(ev.player, "blueprints:empty", 1)
            core.give_item(ev.player, "blueprints:plan", 1, id)
            core.notice(ev.player, `Copied: {x2 - x1 + 1} by {y2 - y1 + 1} high by {z2 - z1 + 1}.`)
            core.log(`blueprints: {core.player_name(ev.player)} copied #{id} from {x1} {y1} {z1} to {x2} {y2} {z2}`)
            return
        end
        if ev.held ~= "blueprints:plan" then
            return
        end
        ev.cancel = true
        local id = core.held_id(ev.player)
        local plan = load(id)
        if plan == nil then
            core.notice(ev.player, "This blueprint is blank.")
            return
        end
        -- Turned so it is entered from the player's side.
        local px, _, pz = core.player_pos(ev.player)
        local dx, dz = ev.x + 0.5 - (px or ev.x), ev.z + 0.5 - (pz or ev.z)
        local t = if math.abs(dx) > math.abs(dz) then (if dx > 0 then 1 else 3) else (if dz > 0 then 0 else 2)
        panel[ev.player] = { id = id, x = ev.x, y = ev.y + 1, z = ev.z, t = t }
        local solid = 0
        for _, c in plan.cells do
            solid += if c ~= 0 then 1 else 0
        end
        local name = `blueprints:panel{ev.player}`
        local s = site_of(core.player_name(ev.player))
        core.ui_define(name, 13, if s then 7.4 else 6.0)
        core.ui_label(name, 0.4, 0.3, `A blueprint: {plan.w} by {plan.h} high by {plan.d}, {solid} blocks.`)
        core.ui_button(name, "lay", 0.4, 1.0, 12.2, 1.2, `Lay it out here, at {ev.x} {ev.y + 1} {ev.z}`)
        core.ui_button(name, "copy", 0.4, 2.4, 12.2, 1.2, "Copy it onto an empty blueprint")
        local y = 3.8
        if s then
            core.ui_button(name, "clear", 0.4, y, 12.2, 1.2, "Take the layout away")
            y += 1.4
        end
        core.ui_button(name, "close", 0.4, y, 12.2, 1.1, "Close")
        core.ui_show(name, true, ev.player)
    end)

    core.event.on("core:ui_click", function(ev)
        if ev.panel ~= `blueprints:panel{ev.player}` then
            return
        end
        core.ui_show(ev.panel, false, ev.player)
        local at = panel[ev.player]
        panel[ev.player] = nil
        local who = core.player_name(ev.player)
        if at == nil then
            return
        end
        if ev.id == "lay" then
            sites[who] = { id = at.id, x = at.x, y = at.y, z = at.z, t = at.t, left = -1 }
            core.storage_set(`site {who}`, `{at.id} {at.x} {at.y} {at.z} {at.t}`)
            show(ev.player)
            core.notice(ev.player, "Laid out: build it by hand, or ask the village's builder or a robot.")
        elseif ev.id == "copy" then
            if core.take_item(ev.player, "blueprints:empty", 1) == 0 then
                core.notice(ev.player, "That needs an empty blueprint.")
                return
            end
            core.give_item(ev.player, "blueprints:plan", 1, at.id)
        elseif ev.id == "clear" then
            sites[who] = nil
            core.storage_set(`site {who}`, "")
            send(ev.player, "c")
        end
    end)

    -- A player's layout, asked for by name: the builder's and the robot's way
    -- in. Answered as "blueprints:cells" with that layout's cells.
    core.event.define("blueprints:layout", { fields = { owner = "string" }, cancellable = false })
    core.event.on("blueprints:layout", function(ev)
        local s = site_of(ev.owner)
        if s then
            core.event.emit("blueprints:ask", { id = s.id, x = s.x, y = s.y, z = s.z, t = s.t, owner = ev.owner })
        end
    end)

end

if drawing then
    local function took(text: string)
        if text == "c" then
            core.ghost_clear()
        elseif string.sub(text, 1, 1) == "g" then
            for x, y, z, id in string.gmatch(text, "(%-?%d+),(%-?%d+),(%-?%d+),(%d+)") do
                core.ghost_cell(tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number, tonumber(id) :: number)
            end
        end
    end
    if serving then
        core.event.define("blueprints:shown", { fields = { text = "string" }, cancellable = false })
        core.event.on("blueprints:shown", function(ev) took(ev.text) end)
    else
        core.event.on("core:net_message", function(ev) took(ev.text) end)
    end
end
