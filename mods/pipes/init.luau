--!strict
-- pipes: item pipes and routed networks, after the transportation module of
-- the classic tech mod (projectredwiki.com/wiki/Transportation).
--
-- PIPES. Every pipe joins the pipes on its six sides and carries stacks along
-- them, drawn as a small glowing cube in the colour of their aura.
--   * ITEM PIPE: no logic. It never touches a container, and where three or
--     more pipes meet in one, a stack goes on any way but back, at random.
--   * JUNCTION PIPE: the routed pipe. A stack that reaches it is sent on along
--     the shortest way to where it is going, or given somewhere to go. Put one
--     where item pipes cross, or stacks wander.
--   * INTERFACE PIPE: a junction joined to ONE container beside it (the first
--     found; a screwdriver turns it to the next). A right click opens its four
--     rows of chips: in each row the first square is the chip and the other
--     eight its filter. All four work at once.
--   * REQUEST PIPE: a right click lists what the broadcasters on its network
--     hold; ask for 1, 8 or 64 and it comes to the container beside the pipe,
--     or onto the ground there when it has none.
--   * FIREWALL PIPE: nine squares; what is put in them may not pass, and ways
--     are found around it.
--
-- CHIPS, in an interface pipe's rows:
--   * EXTRACTOR: once a second sends up to 8 of the first thing in its
--     container that has somewhere to go (only its filter's, if it has one).
--   * RESPONDER: takes its filter's things -- blue, fast.
--   * DYNAMIC RESPONDER: takes what its container already holds -- blue.
--   * TERMINATOR: its filter's, once no responder has room -- purple, slow.
--   * OVERFLOW: anything nobody else takes -- orange, slowest.
--   * BROADCASTER: offers its container to request pipes and stock keepers.
--   * STOCK KEEPER: keeps a stack of each filter thing in its container,
--     asked from the broadcasters.
-- Among chips of the same kind the nearest wins. A stack that finds no room
-- where it arrives looks again from there; one with nowhere at all to go
-- wanders (green), and after a minute falls out where it is.
--
-- Nothing in transit is saved: a restart drops it (a minute's worth at most).
-- ponytail: no crafting/extension pipes, valve, latency pipe, router utility or
-- chip priority yet; add them when a network needs them.

local MAX_NET = 1024       -- cells a search looks at; ponytail: bigger nets are cut there
local LOST_FOR = 60        -- seconds a lost stack wanders before it falls out
local TAKE = 8             -- what an extractor sends at a time
local ROWS, ROW = 4, 9     -- an interface's chips: four rows of a chip and eight filters
local FIREWALL_SLOTS = 9
local INV_SLOTS = 54       -- the squares of a neighbour's container looked through

local R = 2 / 16           -- half the tube's width: thinner than a stack, which shows round it
local DIRS = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }

type Kind = { name: string, routed: boolean, inv: boolean, rgb: { number }, ids: { [number]: number } }
local KINDS: { [string]: Kind } = {
    item = { name = "item_pipe", routed = false, inv = false, rgb = { 170, 190, 200 }, ids = {} },
    junction = { name = "junction_pipe", routed = true, inv = false, rgb = { 70, 110, 80 }, ids = {} },
    interface = { name = "interface_pipe", routed = true, inv = true, rgb = { 180, 150, 60 }, ids = {} },
    request = { name = "request_pipe", routed = true, inv = true, rgb = { 70, 100, 170 }, ids = {} },
    firewall = { name = "firewall_pipe", routed = true, inv = false, rgb = { 160, 60, 45 }, ids = {} },
}
local KIND_OF: { [number]: Kind } = {}
local NAMES: { [Kind]: { string } } = {}

for _, k in KINDS do
    NAMES[k] = {}
    for mask = 0, 63 do
        local name = if mask == 0 then `pipes:{k.name}` else `pipes:{k.name}_{mask}`
        local id = core.register_node(name, k.rgb[1], k.rgb[2], k.rgb[3], false, 0, "", 0, 0.3, "")
        local a, b = 0.5 - R, 0.5 + R
        core.node_box(id, a, a, a, b, b, b)
        local arms = { { b, a, a, 1, b, b }, { 0, a, a, a, b, b }, { a, b, a, b, 1, b },
                       { a, 0, a, b, a, b }, { a, a, b, b, b, 1 }, { a, a, 0, b, b, a } }
        for d = 1, 6 do
            if bit32.btest(mask, bit32.lshift(1, d - 1)) then
                local x = arms[d]
                core.node_box(id, x[1], x[2], x[3], x[4], x[5], x[6])
            end
        end
        k.ids[mask] = id
        KIND_OF[id] = k
        table.insert(NAMES[k], name)
    end
    for mask = 1, 63 do core.node_drop(k.ids[mask], k.ids[0]) end
end
-- Chips sit in an interface's squares, not in the hand's machinery: nobody's
-- extractor or hopper takes them out ("raw" squares are never taken from).
core.container_roles(KINDS.interface.ids[0], string.rep("raw ", ROWS * ROW))
core.container_roles(KINDS.firewall.ids[0], string.rep("raw ", FIREWALL_SLOTS))
for mask = 1, 63 do
    core.container_roles(KINDS.interface.ids[mask], string.rep("raw ", ROWS * ROW))
    core.container_roles(KINDS.firewall.ids[mask], string.rep("raw ", FIREWALL_SLOTS))
end

local CHIP: { [string]: number } = {}
for _, c in { "blank", "responder", "dynamic_responder", "overflow", "terminator", "extractor", "broadcaster", "stock_keeper" } do
    CHIP[c] = core.register_item(`pipes:{c}_chip`, 16, "", 0, 1.0, "")
end

local AURA: { [string]: number } = {}
for _, c in { "blue", "purple", "orange", "white", "green" } do
    AURA[c] = core.register_model(`pipes:aura_{c}`, `aura_{c}.gltf`)
end
-- What each class of destination looks like and how fast it goes, cells a second.
local CLASS = { { aura = "blue", speed = 6 }, { aura = "purple", speed = 2.5 }, { aura = "orange", speed = 1.2 } }
local REQUESTED = { aura = "white", speed = 4 }
local LOST = { aura = "green", speed = 2 }

core.ecs.component("pipes:stack", { "t" })

-- ---------------------------------------------------------------- cells
type Cell = { number }   -- x, y, z
local function key(c: Cell): string return `{c[1]},{c[2]},{c[3]}` end
local function kind_at(c: Cell): Kind? return KIND_OF[core.get_block(c[1], c[2], c[3])] end
local function near(c: Cell, d: number): Cell
    local o = DIRS[d]
    return { c[1] + o[1], c[2] + o[2], c[3] + o[3] }
end
local function has_container(c: Cell): boolean
    return core.container_fill(c[1], c[2], c[3]) >= 0 and kind_at(c) == nil
end

-- The side of an interface or request pipe its container is on: the one the
-- meta says while it still has a container, else the first that has one.
local function side_of(c: Cell): number?
    local s = tonumber(core.meta_get(c[1], c[2], c[3], "side"))
    if s and has_container(near(c, s)) then return s end
    for d = 1, 6 do
        if has_container(near(c, d)) then
            core.meta_set(c[1], c[2], c[3], "side", tostring(d))
            return d
        end
    end
    return nil
end
local function inv_of(c: Cell): Cell?
    local k = kind_at(c)
    if not k or not k.inv then return nil end
    local s = side_of(c)
    return if s then near(c, s) else nil
end

local function refresh(c: Cell)
    local k = kind_at(c)
    if not k then return end
    local mask = 0
    for d = 1, 6 do
        if kind_at(near(c, d)) then mask = bit32.bor(mask, bit32.lshift(1, d - 1)) end
    end
    if k.inv then
        local s = side_of(c)
        if s then mask = bit32.bor(mask, bit32.lshift(1, s - 1)) end
    end
    local want = k.ids[mask]
    if core.get_block(c[1], c[2], c[3]) ~= want then core.set_block(c[1], c[2], c[3], want) end
end
local function around(c: Cell)
    refresh(c)
    for d = 1, 6 do refresh(near(c, d)) end
end

-- ---------------------------------------------------------------- chips
type Row = { chip: number, filter: { [number]: boolean }, any: boolean }
local function rows_of(c: Cell): { Row }
    local out = {}
    for r = 0, ROWS - 1 do
        local chip = core.container_item(c[1], c[2], c[3], r * ROW + 1)
        if chip ~= 0 then
            local filter, any = {}, true
            for i = 2, ROW do
                local f = core.container_item(c[1], c[2], c[3], r * ROW + i)
                if f ~= 0 then
                    filter[f] = true
                    any = false
                end
            end
            table.insert(out, { chip = chip, filter = filter, any = any })
        end
    end
    return out
end

-- Which class of destination this interface is for that item: 1 a responder,
-- 2 a terminator, 3 an overflow, nil none or no room.
local function class_of(c: Cell, item: number): number?
    if kind_at(c) ~= KINDS.interface then return nil end
    local inv = inv_of(c)
    if not inv or core.container_fill(inv[1], inv[2], inv[3]) >= 1 then return nil end
    local best: number? = nil
    for _, r in rows_of(c) do
        local cl: number? = nil
        if r.chip == CHIP.responder and r.filter[item] then cl = 1
        elseif r.chip == CHIP.dynamic_responder and core.container_count(inv[1], inv[2], inv[3], item) > 0 then cl = 1
        elseif r.chip == CHIP.terminator and r.filter[item] then cl = 2
        elseif r.chip == CHIP.overflow then cl = 3 end
        if cl and (best == nil or cl < best) then best = cl end
    end
    return best
end

local function blocks(c: Cell, item: number): boolean
    if kind_at(c) ~= KINDS.firewall then return false end
    for i = 1, FIREWALL_SLOTS do
        if core.container_item(c[1], c[2], c[3], i) == item then return true end
    end
    return false
end

-- ---------------------------------------------------------------- searching
-- Breadth first from `from` over the pipes this item may pass; `visit` sees
-- each cell in order of distance and returns true to stop. The parents lead
-- back to `from`.
local function search(from: Cell, item: number, visit: (Cell) -> boolean): { [string]: Cell }
    local parent: { [string]: Cell } = { [key(from)] = from }
    local queue, head = { from }, 1
    while head <= #queue and head <= MAX_NET do
        local c = queue[head]
        head += 1
        if visit(c) then break end
        for d = 1, 6 do
            local n = near(c, d)
            local nk = key(n)
            if not parent[nk] and kind_at(n) and not blocks(n, item) then
                parent[nk] = c
                table.insert(queue, n)
            end
        end
    end
    return parent
end
-- The way from `from` to `to`, from a search started at `from`.
local function way(parent: { [string]: Cell }, from: Cell, to: Cell): { Cell }
    local path = { to }
    local c = to
    while key(c) ~= key(from) do
        c = parent[key(c)]
        table.insert(path, 1, c)
    end
    return path
end

-- Where an item should go from here: the nearest of the best class.
local function route(from: Cell, item: number, skip: { [string]: boolean }): ({ Cell }?, number?)
    local best: { [number]: Cell } = {}
    local parent = search(from, item, function(c)
        if not skip[key(c)] then
            local cl = class_of(c, item)
            if cl and not best[cl] then best[cl] = c end
        end
        return best[1] ~= nil
    end)
    for cl = 1, 3 do
        if best[cl] then return way(parent, from, best[cl]), cl end
    end
    return nil, nil
end

-- ---------------------------------------------------------------- stacks in transit
type Stack = { e: number?, item: number, n: number, a: Cell, b: Cell, t: number, speed: number,
               path: { Cell }?, i: number, dest: string?, requested: boolean, owed: number, age: number,
               skip: { [string]: boolean } }
local moving: { Stack } = {}
-- What stock keepers have asked for and not yet got: [cell key][item] = count.
local pending: { [string]: { [number]: number } } = {}

local function look(s: Stack, style: { aura: string, speed: number })
    s.speed = style.speed
    if s.e then core.ecs.model(s.e, AURA[style.aura]) end
end

local function place(s: Stack)
    if not s.e then return end
    local t = math.min(s.t, 1)
    core.ecs.set(s.e, "x", s.a[1] + 0.5 + (s.b[1] - s.a[1]) * t)
    core.ecs.set(s.e, "y", s.a[2] + 0.33 + (s.b[2] - s.a[2]) * t)
    core.ecs.set(s.e, "z", s.a[3] + 0.5 + (s.b[3] - s.a[3]) * t)
end

-- A stock keeper's request is no longer on its way.
local function settle(s: Stack)
    local p = if s.requested and s.dest then pending[s.dest] else nil
    if p and p[s.item] then
        p[s.item] = math.max(0, p[s.item] - s.owed)
        if p[s.item] == 0 then p[s.item] = nil end
    end
    s.requested = false
end
local function finish(s: Stack)
    if s.e then core.ecs.kill(s.e) end
    s.n = 0
    settle(s)
end
local function spill(s: Stack, c: Cell)
    core.drop_item(c[1], c[2], c[3], s.item, s.n)
    finish(s)
end

local function send(path: { Cell }, item: number, n: number, style: { aura: string, speed: number },
                    requested: boolean)
    local a = path[1]
    local s: Stack = { e = nil, item = item, n = n, a = a, b = path[2] or a, t = 0, speed = style.speed,
                       path = path, i = 1, dest = key(path[#path]), requested = requested, owed = n, age = 0, skip = {} }
    local e = core.ecs.spawn("pipes:stack", a[1] + 0.5, a[2] + 0.33, a[3] + 0.5)
    if e then
        s.e = e
        core.ecs.visible(e, true)
    end
    look(s, style)
    table.insert(moving, s)
end

-- The ways on from a cell but back, that this stack may take.
local function exits(c: Cell, back: Cell, item: number): { Cell }
    local out = {}
    for d = 1, 6 do
        local n = near(c, d)
        if key(n) ~= key(back) and kind_at(n) and not blocks(n, item) then table.insert(out, n) end
    end
    return out
end

-- Off its way: on at random, still knowing where it was going.
local function wander(s: Stack, c: Cell, back: Cell)
    s.path = nil
    local out = exits(c, back, s.item)
    s.a, s.b = c, if #out > 0 then out[math.random(#out)] else back
end

local function follow(s: Stack, c: Cell, path: { Cell })
    s.path, s.i, s.dest = path, 1, key(path[#path])
    s.a, s.b = c, path[2] or c
end

-- At a routed pipe: the way to where it was going, or somewhere new to go.
local function reroute(s: Stack, c: Cell, back: Cell)
    local dest = s.dest
    if dest then
        local parent = search(c, s.item, function(n) return key(n) == dest end)
        local x, y, z = string.match(dest, "^(-?%d+),(-?%d+),(-?%d+)$")
        if parent[dest] and x then return follow(s, c, way(parent, c, { tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number })) end
    end
    settle(s)
    local path, cl = route(c, s.item, s.skip)
    if path and cl then
        look(s, CLASS[cl])
        follow(s, c, path)
    else
        s.dest = nil
        look(s, LOST)
        wander(s, c, back)
    end
end

local function deliver(s: Stack, c: Cell, back: Cell)
    local inv = inv_of(c)
    if inv then
        s.n = core.container_add(inv[1], inv[2], inv[3], s.item, s.n)
    elseif s.requested and kind_at(c) == KINDS.request then
        core.drop_item(c[1], c[2], c[3], s.item, s.n)
        s.n = 0
    end
    if s.n == 0 then
        finish(s)
        return
    end
    -- No room after all: somewhere else, from here.
    settle(s)
    s.skip[key(c)] = true
    s.dest = nil
    reroute(s, c, back)
end

-- A stack has come into cell b from a.
local function arrive(s: Stack)
    local c, back = s.b, s.a
    s.t = 0
    local k = kind_at(c)
    if not k then return spill(s, back) end
    if s.age > LOST_FOR and not s.path then return spill(s, c) end
    if s.dest == key(c) then return deliver(s, c, back) end
    local path = s.path
    if path then
        local nxt = path[s.i + 2]
        s.i += 1
        if nxt and kind_at(nxt) and not blocks(nxt, s.item) then
            -- An item pipe where three or more meet sends it any way.
            if not k.routed then
                local out = exits(c, back, s.item)
                if #out >= 2 then
                    local pick = out[math.random(#out)]
                    if key(pick) ~= key(nxt) then
                        s.path = nil
                        s.a, s.b = c, pick
                        return
                    end
                end
            end
            s.a, s.b = c, nxt
            return
        end
        -- The way was broken: look again from here.
        s.path = nil
    end
    if k.routed then return reroute(s, c, back) end
    wander(s, c, back)
end

core.event.on("core:tick", function(ev)
    if #moving == 0 then return end
    local keep = {}
    for _, s in moving do
        s.age += ev.dt
        s.t += s.speed * ev.dt
        local steps = 0
        while s.n > 0 and s.t >= 1 and steps < 4 do
            arrive(s)
            steps += 1
        end
        if s.n > 0 then
            place(s)
            table.insert(keep, s)
        end
    end
    moving = keep
end)

-- ---------------------------------------------------------------- what chips do
local function take(inv: Cell, item: number, n: number): number
    local got = core.container_take(inv[1], inv[2], inv[3], item, n, "store")
    if got < n then got += core.container_take(inv[1], inv[2], inv[3], item, n - got, "product") end
    return got
end
local function offered(inv: Cell, item: number): number
    return core.container_count(inv[1], inv[2], inv[3], item, "store")
        + core.container_count(inv[1], inv[2], inv[3], item, "product")
end

local function extract(c: Cell, inv: Cell, r: Row)
    local skip = { [key(c)] = true }
    local tried: { [number]: boolean } = {}
    for slot = 1, INV_SLOTS do
        local item = core.container_item(inv[1], inv[2], inv[3], slot)
        if item ~= 0 and not tried[item] and (r.any or r.filter[item]) then
            tried[item] = true
            local path, cl = route(c, item, skip)
            if path and cl then
                local n = take(inv, item, TAKE)
                if n > 0 then
                    send(path, item, n, CLASS[cl], false)
                    return
                end
            end
        end
    end
end

-- The broadcasters on the network of `to`, nearest first, and the way from each.
type Offer = { inv: Cell, path: { Cell } }
local function broadcasters(to: Cell, item: number): { Offer }
    local found: { Cell } = {}
    local parent = search(to, item, function(c)
        if key(c) ~= key(to) and kind_at(c) == KINDS.interface then
            for _, r in rows_of(c) do
                if r.chip == CHIP.broadcaster then
                    table.insert(found, c)
                    break
                end
            end
        end
        return false
    end)
    local out = {}
    for _, c in found do
        local inv = inv_of(c)
        if inv then
            local path = way(parent, to, c)
            local back = {}
            for i = #path, 1, -1 do table.insert(back, path[i]) end
            table.insert(out, { inv = inv, path = back })
        end
    end
    return out
end

-- Sends up to n of an item from the broadcasters to `to`; how many set off.
local function request(to: Cell, item: number, n: number): number
    local sent = 0
    for _, o in broadcasters(to, item) do
        while sent < n do
            local got = take(o.inv, item, math.min(n - sent, core.item_stack(item)))
            if got == 0 then break end
            send(o.path, item, got, REQUESTED, true)
            sent += got
        end
        if sent >= n then break end
    end
    return sent
end

local function keep_stock(c: Cell, inv: Cell, r: Row)
    local k = key(c)
    pending[k] = pending[k] or {}
    local p = pending[k]
    for item in r.filter do
        local want = core.item_stack(item) - core.container_count(inv[1], inv[2], inv[3], item) - (p[item] or 0)
        if want > 0 then
            local got = request(c, item, want)
            if got > 0 then p[item] = (p[item] or 0) + got end
        end
    end
end

local function work(cell: { x: number, y: number, z: number })
    local c = { cell.x, cell.y, cell.z }
    local inv = inv_of(c)
    if not inv then return end
    for _, r in rows_of(c) do
        if r.chip == CHIP.extractor then extract(c, inv, r)
        elseif r.chip == CHIP.stock_keeper then keep_stock(c, inv, r) end
    end
end
-- An ABM names at most 32 blocks: the 64 shapes of an interface in two.
local names = NAMES[KINDS.interface]
core.register_abm("pipes:chips", table.move(names, 1, 32, 1, {}), 1.0, 1, work)
core.register_abm("pipes:chips_2", table.move(names, 33, 64, 1, {}), 1.0, 1, work)

-- ---------------------------------------------------------------- the request page
core.page_register("pipes", "Request pipe")
local asking: { [number]: Cell } = {}   -- the request pipe each player has open

local function shown_name(item: number): string
    local n = core.item_name(item)
    n = string.gsub(string.match(n, ":(.*)") or n, "_", " ")
    return string.upper(string.sub(n, 1, 1)) .. string.sub(n, 2)
end

local function show_request(player: number, c: Cell)
    asking[player] = c
    local total: { [number]: number } = {}
    for _, o in broadcasters(c, 0) do
        for slot = 1, INV_SLOTS do
            local item = core.container_item(o.inv[1], o.inv[2], o.inv[3], slot)
            if item ~= 0 and not total[item] then total[item] = 0 end
        end
        for item in total do total[item] += offered(o.inv, item) end
    end
    local page = { "# Request pipe" }
    local list = {}
    for item, n in total do
        if n > 0 then table.insert(list, { item = item, n = n, name = shown_name(item) }) end
    end
    table.sort(list, function(a, b) return a.name < b.name end)
    if #list == 0 then table.insert(page, "No broadcaster on this network offers anything.") end
    for _, e in list do
        table.insert(page, `| {e.name} | {e.n} | [1](act:{e.item}:1) | [8](act:{e.item}:8) | [64](act:{e.item}:64) |`)
    end
    core.page_show(player, "pipes", table.concat(page, "\n"))
end

core.event.on("core:page_open", function(ev)
    local c = asking[ev.player]
    if c and kind_at(c) == KINDS.request then
        show_request(ev.player, c)
    else
        core.page_show(ev.player, "pipes", "# Request pipe\nRight click a request pipe to ask its network for things.")
    end
end)

core.event.on("core:page_action", function(ev)
    local c = asking[ev.player]
    local item, n = string.match(ev.id, "^(%d+):(%d+)$")
    if not c or not item or kind_at(c) ~= KINDS.request then return end
    -- Only from beside the pipe: the page is a client's word.
    local px, py, pz = core.player_pos(ev.player)
    if not px or math.abs(px - c[1]) > 8 or math.abs(py - c[2]) > 8 or math.abs(pz - c[3]) > 8 then return end
    local id, count = tonumber(item) :: number, math.clamp(tonumber(n) :: number, 1, 64)
    local got = request(c, id, count)
    core.notice(ev.player, if got > 0 then `{got} on the way` else "Nothing to send")
    show_request(ev.player, c)
end)

-- ---------------------------------------------------------------- placing, using, breaking
local function is_pipe_item(name: string): boolean
    return string.sub(name, 1, 6) == "pipes:" and string.find(name, "_pipe", 1, true) ~= nil
end

core.event.on("core:placed", function(ev)
    local c = { ev.x, ev.y, ev.z }
    local k = KIND_OF[ev.id]
    if k == KINDS.interface then core.container_make(ev.x, ev.y, ev.z, ROWS * ROW) end
    if k == KINDS.firewall then core.container_make(ev.x, ev.y, ev.z, FIREWALL_SLOTS) end
    -- A pipe, or a chest put beside an interface: the shapes round it change.
    around(c)
end)
core.event.on("core:broken", function(ev)
    around({ ev.x, ev.y, ev.z })
end)
core.event.on("core:pre_break", function(ev)
    local k = KIND_OF[ev.id]
    if k == KINDS.interface or k == KINDS.firewall then
        core.container_spill(ev.x, ev.y, ev.z)
        core.container_clear(ev.x, ev.y, ev.z)
    end
end)

core.event.on("core:used", function(ev)
    local k = KIND_OF[ev.id]
    if not k or is_pipe_item(ev.held) then return end
    local c = { ev.x, ev.y, ev.z }
    if ev.held == "screwdriver:screwdriver" then
        if k.inv then
            -- The next side round that has a container.
            local s = tonumber(core.meta_get(ev.x, ev.y, ev.z, "side")) or 0
            for i = 1, 6 do
                local d = (s + i - 1) % 6 + 1
                if has_container(near(c, d)) then
                    core.meta_set(ev.x, ev.y, ev.z, "side", tostring(d))
                    break
                end
            end
            refresh(c)
        end
        ev.cancel = true
        return
    end
    if k == KINDS.interface then
        core.container_show(ev.x, ev.y, ev.z, ROWS * ROW, ev.player)
        ev.cancel = true
    elseif k == KINDS.firewall then
        core.container_show(ev.x, ev.y, ev.z, FIREWALL_SLOTS, ev.player)
        ev.cancel = true
    elseif k == KINDS.request then
        show_request(ev.player, c)
        ev.cancel = true
    end
end)

-- ---------------------------------------------------------------- recipes
-- Cinnabar for the wiki's redstone, dyes for its coloured illumar.
core.recipe_shaped("pipes:item_pipe", 16,
    "core:stone  core:glass  core:stone")
core.recipe_shaped("pipes:junction_pipe", 16,
    "colors:red_dye    ore:cinnabar  colors:green_dye",
    "ore:diamond       core:glass    ore:diamond",
    "colors:green_dye  ore:cinnabar  colors:red_dye")
core.recipe_shaped("pipes:interface_pipe", 1,
    "ore:cinnabar        furnace:gold_ingot   ore:cinnabar",
    "furnace:gold_ingot  pipes:junction_pipe  furnace:gold_ingot",
    "ore:cinnabar        furnace:gold_ingot   ore:cinnabar")
core.recipe_shaped("pipes:request_pipe", 1,
    "ore:cinnabar  pipes:junction_pipe  ore:diamond")
core.recipe_shaped("pipes:firewall_pipe", 1,
    "rocks:cobblestone  pipes:junction_pipe  ore:cinnabar")

core.recipe_shaped("pipes:blank_chip", 1,
    "furnace:gold_ingot  cane:paper  ore:cinnabar")
for _, c in { { "responder", "orange", "furnace:iron_ingot" }, { "dynamic_responder", "cyan", "colors:orange_dye" },
              { "overflow", "green", "furnace:iron_ingot" }, { "terminator", "purple", "colors:gray_dye" },
              { "extractor", "cyan", "furnace:iron_ingot" }, { "broadcaster", "magenta", "furnace:gold_ingot" },
              { "stock_keeper", "blue", "ore:diamond" } } do
    core.recipe_shaped(`pipes:{c[1]}_chip`, 1,
        `colors:{c[2]}_dye  ore:cinnabar     colors:{c[2]}_dye`,
        `{c[3]}             pipes:blank_chip  {c[3]}`,
        `colors:{c[2]}_dye  ore:cinnabar     colors:{c[2]}_dye`)
end

-- Another file's way to have a cell's shape worked out again (demo.luau, or a
-- mod that wrote pipes with core.set_block).
core.event.define("pipes:refresh", { fields = { x = "number", y = "number", z = "number" }, cancellable = false })
core.event.on("pipes:refresh", function(ev) refresh({ ev.x, ev.y, ev.z }) end)

-- Stacks on their way in a box of cells, gone without falling out: what a
-- network being built again does with what was in it (demo.luau's reset).
core.event.define("pipes:clear", { fields = { x0 = "number", z0 = "number", x1 = "number", z1 = "number" },
                                   cancellable = false })
core.event.on("pipes:clear", function(ev)
    for _, s in moving do
        local c = s.a
        if c[1] >= ev.x0 and c[1] <= ev.x1 and c[3] >= ev.z0 and c[3] <= ev.z1 then finish(s) end
    end
end)

-- Two working networks to look at, on the plaza.
if core.plaza() ~= nil then require("./demo") end

core.log("pipes: item, junction, interface, request and firewall pipes; seven chips")
