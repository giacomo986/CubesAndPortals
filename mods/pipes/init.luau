--!strict
-- pipes: item pipes and routed networks, after the transportation module of
-- the classic tech mod (projectredwiki.com/wiki/Transportation).
--
-- PIPES. Every pipe joins the pipes on its six sides and carries stacks along
-- them, drawn as a small glowing cube in the colour of their aura.
--   * ITEM PIPE: no logic. It never touches a container. A stack on its way
--     follows its way through it; one with no way, where three or more pipes
--     meet, goes on any way but back, at random.
--   * JUNCTION PIPE: the routed pipe. A stack that reaches it is sent on along
--     the shortest way to where it is going, or given somewhere to go. Put one
--     where item pipes cross, or a stack that has lost its way wanders there.
--   * INTERFACE PIPE: a junction joined to ONE container beside it (the first
--     found; a screwdriver or the pipe wrench turns it to the next). A right click opens its four
--     rows of chips: in each row the first square is the chip and the other
--     eight its filter. All four work at once.
--   * REQUEST PIPE: a right click opens a grid of what the broadcasters on its
--     network hold, and of what they hold the makings of (a small wrench in
--     the corner), with a search and the amount a click asks for. What is
--     asked for comes from the store, and what the store lacks is crafted out
--     of it; it comes to the container beside the pipe, or onto the pipe.
--   * LATENCY PIPE: an item pipe that slows what passes and counts as eight
--     cells to a way's length, so ways go round it when they can.
--   * VALVE PIPE: shut, nothing passes; open while something of mods/cinnabar
--     gives power beside it. Ways go round a shut one, and a stack whose way
--     runs through it waits there for it to open.
--   * CRAFTING PIPE: touching an automatic crafting table or furnace, its
--     nine recipes (chosen on its panel, no thing needed) are what that
--     machine can make; each EXTENSION PIPE joined to it adds nine more.
--   * The PIPE ROUTER, right clicked on a pipe, says what its network is.
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
--   * RECIPE: in an interface pipe touching an automatic crafting table or
--     furnace, the things in its filter are what that machine can make. A
--     machine with no recipe chip beside it makes nothing.
-- Among chips of the same kind the highest PRIORITY wins -- how many of that
-- chip the interface holds, a stack of three being priority 3 -- and of those
-- the nearest. A stack that finds no room
-- where it arrives looks again from there; one with nowhere at all to go
-- wanders (green), and after a minute falls out where it is.
--
-- THE GRAPH. A network is not searched cell by cell each time a stack needs a
-- way. It is boiled down once into a graph: its NODES are the cells where
-- something is decided (every routed pipe, and an item pipe where three or
-- more meet, or that ends), its EDGES the runs of item pipe between them, with
-- their cells and length. The shortest ways from a node are worked out once
-- (Dijkstra) and kept, one set per combination of firewalls an item is held
-- back by. The graph is thrown away when a cell of it or beside it changes
-- (core.watch_cell sees every write, a player's or a mod's), and built again
-- when next asked. What changes all the time -- chips, filters, what is in a
-- container -- is not in it: that is read when a stack chooses, and the
-- chips' and firewalls' squares are kept until their container's revision
-- changes.
--
-- Nothing in transit is saved: a restart drops it (a minute's worth at most).
-- ponytail: no crafting/extension pipes, valve, latency pipe, router utility or
-- chip priority yet; add them when a network needs them.

local MAX_NET = 65536      -- cells of one network in its graph; ponytail: bigger nets are cut there
local STALE_AFTER = 30     -- seconds a graph is trusted: what watch_cell cannot see (a chunk loaded back) is caught up then
local MAX_ROUTES = 4096    -- sets of ways a graph keeps before it starts again
local MAX_ASK = 1024       -- the most one click of the request panel asks for
local LATENCY_COST = 8     -- a latency pipe counts as this many cells to a way's length
local LATENCY_SLOW = 0.35  -- and a stack in one goes at this much of its speed
local RECIPE_SLOTS = 9     -- recipes a crafting or an extension pipe holds
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
    latency = { name = "latency_pipe", routed = false, inv = false, rgb = { 120, 90, 150 }, ids = {} },
    valve = { name = "valve_pipe", routed = false, inv = false, rgb = { 150, 40, 40 }, ids = {} },
    crafting = { name = "crafting_pipe", routed = true, inv = false, rgb = { 200, 120, 40 }, ids = {} },
    extension = { name = "extension_pipe", routed = false, inv = false, rgb = { 210, 180, 90 }, ids = {} },
}
local KIND_OF: { [number]: Kind } = {}
local NAMES: { [Kind]: { string } } = {}

for _, k in KINDS do
    NAMES[k] = {}
    for mask = 0, 63 do
        local name = if mask == 0 then `pipes:{k.name}` else `pipes:{k.name}_{mask}`
        -- Solid: a body is stopped by the tube's boxes (and walks round or jumps
        -- over it), and an item dropped on one rests there. Light still passes,
        -- as only a whole cube is opaque.
        local id = core.register_node(name, k.rgb[1], k.rgb[2], k.rgb[3], true, 0, "", 0, 0.3, "")
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
for _, c in { "blank", "responder", "dynamic_responder", "overflow", "terminator", "extractor", "broadcaster", "stock_keeper",
              "recipe" } do
    CHIP[c] = core.register_item(`pipes:{c}_chip`, 16, "", 0, 1.0, "")
end

-- The pipe wrench: turns an interface or a request pipe to its next container,
-- as the screwdriver does; and the mark on what a request pipe's network can craft.
core.register_item("pipes:wrench", 1, "", 0, 1.0, "")
-- The pipe router: what a network is, on a page (show_network).
core.register_item("pipes:router", 1, "", 0, 1.0, "")

-- The machines: level-two blocks, the network's crafting table and furnace.
-- Each is a block in two states (standing, at work) drawn by its Blender model
-- (tools/make_pipes_machines.py), with a full cell of box, a container of its
-- own for what is brought to it, and dug only with an iron pick or better.
local MACHINE_SLOTS = 27
local MACHINE: { [number]: string } = {}    -- block id -> "craft" or "smelt"
local MACHINE_IDS: { [string]: { off: number, on: number } } = {}
for _, m in { { "autocrafter", "craft", { 112, 112, 120 }, 4 }, { "autofurnace", "smelt", { 96, 92, 90 }, 13 } } do
    local name, kind, rgb, glow = m[1], m[2], m[3], m[4]
    local off = core.register_node(`pipes:{name}`, rgb[1], rgb[2], rgb[3], true, 0, "upright", 2, 5.0, "stone")
    local on = core.register_node(`pipes:{name}_on`, rgb[1], rgb[2], rgb[3], true, glow, "upright", 2, 5.0, "stone")
    core.node_model(off, core.register_model(`pipes:{name}`, `{name}.gltf`))
    core.node_model(on, core.register_model(`pipes:{name}_on`, `{name}_on.gltf`))
    core.register_model(`pipes:{name}_held`, `{name}_held.gltf`)
    for _, id in { off, on } do
        core.node_box(id, 0, 0, 0, 1, 1, 1)
        core.container_roles(id, string.rep("raw ", MACHINE_SLOTS))
        MACHINE[id] = kind
    end
    core.node_drop(on, off)
    MACHINE_IDS[kind] = { off = off, on = on }
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
local function machine_at(c: Cell): string? return MACHINE[core.get_block(c[1], c[2], c[3])] end
-- A container an interface or a request pipe can be joined to: not a pipe's
-- squares, and not a machine's, which are the machine's own business.
local function has_container(c: Cell): boolean
    return core.container_fill(c[1], c[2], c[3]) >= 0 and kind_at(c) == nil and machine_at(c) == nil
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
        local n = near(c, d)
        if kind_at(n) or machine_at(n) then mask = bit32.bor(mask, bit32.lshift(1, d - 1)) end
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
-- What an interface's squares say, and a firewall's, kept until the container
-- changes (core.container_revision): read on every choice of every stack.
-- `prio`: how many of that chip the interface holds -- a stack of three
-- responder chips is a responder of priority 3, chosen before a nearer one of 1.
type Row = { chip: number, filter: { [number]: boolean }, any: boolean, prio: number }
local rows_kept: { [string]: { rev: number, rows: { Row } } } = {}
local function rows_of(c: Cell): { Row }
    local k = key(c)
    local rev = core.container_revision(c[1], c[2], c[3])
    local kept = rows_kept[k]
    if kept and kept.rev == rev then return kept.rows end
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
            table.insert(out, { chip = chip, filter = filter, any = any,
                                prio = math.max(1, core.container_count(c[1], c[2], c[3], chip)) })
        end
    end
    rows_kept[k] = { rev = rev, rows = out }
    return out
end
local function has_chip(c: Cell, chip: number): boolean
    for _, r in rows_of(c) do
        if r.chip == chip then return true end
    end
    return false
end

local walls_kept: { [string]: { rev: number, items: { [number]: boolean } } } = {}
local function walled(c: Cell): { [number]: boolean }
    local k = key(c)
    local rev = core.container_revision(c[1], c[2], c[3])
    local kept = walls_kept[k]
    if kept and kept.rev == rev then return kept.items end
    local items = {}
    for i = 1, FIREWALL_SLOTS do
        local f = core.container_item(c[1], c[2], c[3], i)
        if f ~= 0 then items[f] = true end
    end
    walls_kept[k] = { rev = rev, items = items }
    return items
end
-- What opens a valve: something of mods/cinnabar giving power beside it -- a
-- lever or button that is on, a lit torch, lit dust, a repeater or comparator
-- that is on, an observer pulsing, a cinnabar block (as mods/building reads it).
local POWER: { [number]: boolean }? = nil
local function power_ids(): { [number]: boolean }
    if POWER then return POWER end
    local p: { [number]: boolean } = {}
    for _, n in { "cinnabar:lever_on", "cinnabar:button_on", "cinnabar:torch", "cinnabar:dust_on",
                  "cinnabar:repeater_on", "cinnabar:comparator_on", "cinnabar:observer_on", "compact:cinnabar" } do
        local id = core.node_id(n)
        if id ~= 0 then p[id] = true end
    end
    for a = 0, 2 do for b = 0, 2 do for c = 0, 2 do for d = 0, 2 do
        local id = core.node_id(`cinnabar:dust_on_{a}{b}{c}{d}`)
        if id ~= 0 then p[id] = true end
    end end end end
    POWER = p
    return p
end
local function valve_open(c: Cell): boolean
    local p = power_ids()
    for d = 1, 6 do
        local n = near(c, d)
        if p[core.get_block(n[1], n[2], n[3])] then return true end
    end
    return false
end

-- Whether that pipe keeps an item out: a firewall that names it, a valve that is shut.
local function blocks(c: Cell, item: number): boolean
    local k = kind_at(c)
    if k == KINDS.firewall then return walled(c)[item] == true end
    if k == KINDS.valve then return not valve_open(c) end
    return false
end

-- Which class of destination this interface is for that item: 1 a responder,
-- 2 a terminator, 3 an overflow, nil none or no room.
-- And its priority for it (how many of the chip that makes it so).
local function class_of(c: Cell, item: number): (number?, number)
    if kind_at(c) ~= KINDS.interface then return nil, 0 end
    local inv = inv_of(c)
    if not inv or core.container_fill(inv[1], inv[2], inv[3]) >= 1 then return nil, 0 end
    local best: number?, prio = nil, 0
    for _, r in rows_of(c) do
        local cl: number? = nil
        if r.chip == CHIP.responder and r.filter[item] then cl = 1
        elseif r.chip == CHIP.dynamic_responder and core.container_count(inv[1], inv[2], inv[3], item) > 0 then cl = 1
        elseif r.chip == CHIP.terminator and r.filter[item] then cl = 2
        elseif r.chip == CHIP.overflow then cl = 3 end
        if cl then
            if best == nil or cl < (best :: number) then best, prio = cl, r.prio
            elseif cl == best then prio = math.max(prio, r.prio) end
        end
    end
    return best, prio
end

-- ---------------------------------------------------------------- the graph
-- An edge leaves a node and runs to another (or back to itself): `cells` are
-- the ones after the node it leaves, the last being the node it reaches.
type Edge = { to: string, cells: { Cell }, len: number }
type Node = { cell: Cell, kind: Kind, edges: { Edge } }
-- The ways from one node for items held back by one set of firewalls: how far
-- each node is, the edge that reaches it and the node before; `ends` the
-- interfaces, nearest first.
type Ways = { dist: { [string]: number }, via: { [string]: Edge }, back: { [string]: string }, ends: { string } }
-- A machine beside the network: its cell, its kind, and the pipe it is fed through
-- (a node of the graph, so a way can end there).
type Spot = { cell: Cell, kind: string, pipe: Cell }
type Net = { kinds: { [string]: Kind }, nodes: { [string]: Node }, firewalls: { string }, ways: { [string]: Ways },
             nways: number, live: boolean, born: number, watched: { string },
             machines: { Spot }, machine_at: { [string]: string }, valves: { string } }

local clock = 0                                         -- seconds since the mod loaded
local net_at: { [string]: Net } = {}                    -- a pipe cell's network
local beside: { [string]: { [Net]: boolean } } = {}     -- the networks a cell that is not theirs touches
local watching: { [string]: boolean } = {}              -- cells already asked of core.watch_cell

local function watch(k: string, c: Cell)
    -- Never let go again: core.watch_cell is one set for every mod, and an
    -- observer (mods/cinnabar) may be watching the same cell.
    if not watching[k] then
        watching[k] = true
        core.watch_cell(c[1], c[2], c[3], true)
    end
end

local function drop_net(net: Net)
    if not net.live then return end
    net.live = false
    for _, k in net.watched do
        if net_at[k] == net then net_at[k] = nil end
        local b = beside[k]
        if b then
            b[net] = nil
            if next(b) == nil then beside[k] = nil end
        end
    end
end

-- Something at this cell may have changed what the networks round it are.
local function touched(c: Cell)
    local k = key(c)
    rows_kept[k], walls_kept[k] = nil, nil
    local n = net_at[k]
    if n then drop_net(n) end
    local b = beside[k]
    if b then
        local all = {}
        for net in b do table.insert(all, net) end
        for _, net in all do drop_net(net) end
    end
end

local function build(start: Cell): Net
    local net: Net = { kinds = {}, nodes = {}, firewalls = {}, ways = {}, nways = 0, live = true, born = clock,
                       watched = {}, machines = {}, machine_at = {}, valves = {} }
    local cells = { start }
    net.kinds[key(start)] = kind_at(start) :: Kind
    local head = 1
    while head <= #cells and #cells < MAX_NET do
        local c = cells[head]
        head += 1
        for d = 1, 6 do
            local n = near(c, d)
            local nk = key(n)
            if not net.kinds[nk] then
                local k = kind_at(n)
                if k then
                    net.kinds[nk] = k
                    table.insert(cells, n)
                end
            end
        end
    end
    local function ours(c: Cell): boolean return net.kinds[key(c)] ~= nil end

    -- The nodes: every routed pipe, every pipe a machine is fed through, and
    -- an item pipe that is not the middle of a run (three ways or more, or one,
    -- or none).
    for _, c in cells do
        local k = key(c)
        local kind = net.kinds[k]
        local ways, feeds = 0, false
        for d = 1, 6 do
            local n = near(c, d)
            if ours(n) then
                ways += 1
            else
                local mk = machine_at(n)
                if mk then
                    feeds = true
                    local nk = key(n)
                    if not net.machine_at[nk] then
                        net.machine_at[nk] = mk
                        table.insert(net.machines, { cell = n, kind = mk, pipe = c })
                    end
                end
            end
        end
        if kind.routed or ways ~= 2 or feeds or kind == KINDS.valve then
            net.nodes[k] = { cell = c, kind = kind, edges = {} }
            if kind == KINDS.firewall then table.insert(net.firewalls, k) end
            if kind == KINDS.valve then table.insert(net.valves, k) end
        end
    end
    -- The edges: from each node, along each way, to the node the run ends at.
    for _, node in net.nodes do
        for d = 1, 6 do
            local c = near(node.cell, d)
            if ours(c) then
                local run: { Cell } = {}
                local prev = node.cell
                local length = 0   -- in cells, a latency pipe counting LATENCY_COST
                for _ = 1, #cells do
                    table.insert(run, c)
                    local ck = key(c)
                    length += if net.kinds[ck] == KINDS.latency then LATENCY_COST else 1
                    if net.nodes[ck] then
                        table.insert(node.edges, { to = ck, cells = run, len = length })
                        break
                    end
                    local nxt: Cell? = nil
                    for d2 = 1, 6 do
                        local m = near(c, d2)
                        if key(m) ~= key(prev) and ours(m) then
                            nxt = m
                            break
                        end
                    end
                    if not nxt then break end
                    prev, c = c, nxt
                end
            end
        end
    end
    -- Heard of again when any of its cells, or one beside them, is written.
    for _, c in cells do
        local k = key(c)
        net_at[k] = net
        watch(k, c)
        table.insert(net.watched, k)
        for d = 1, 6 do
            local n = near(c, d)
            local nk = key(n)
            if not net.kinds[nk] then
                beside[nk] = beside[nk] or {}
                beside[nk][net] = true
                watch(nk, n)
                table.insert(net.watched, nk)
            end
        end
    end
    return net
end

local function net_of(c: Cell): Net?
    local n = net_at[key(c)]
    if n and n.live and clock - n.born < STALE_AFTER then return n end
    if n then drop_net(n) end
    if not kind_at(c) then return nil end
    return build(c)
end

-- A small heap of { distance, node key }, for Dijkstra.
local function push(h: { { any } }, d: number, k: string)
    table.insert(h, { d, k })
    local i = #h
    while i > 1 do
        local p = i // 2
        if h[p][1] <= h[i][1] then break end
        h[p], h[i] = h[i], h[p]
        i = p
    end
end
local function pop(h: { { any } }): { any }
    local top = h[1]
    local last = table.remove(h) :: { any }
    if #h > 0 then
        h[1] = last
        local i = 1
        while true do
            local l, r, m = 2 * i, 2 * i + 1, i
            if l <= #h and h[l][1] < h[m][1] then m = l end
            if r <= #h and h[r][1] < h[m][1] then m = r end
            if m == i then break end
            h[m], h[i] = h[i], h[m]
            i = m
        end
    end
    return top
end

-- The ways from a node for this item: worked out once for each set of
-- firewalls it is held back by, and kept with the graph.
local function ways_of(net: Net, from: Cell, item: number): Ways?
    local fk = key(from)
    if not net.nodes[fk] then return nil end
    local shut: { [string]: boolean } = {}
    local sig = {}
    for _, fw in net.firewalls do
        if item ~= 0 and walled(net.nodes[fw].cell)[item] then
            shut[fw] = true
            table.insert(sig, fw)
        end
    end
    -- And every valve that is shut, for every item.
    for _, v in net.valves do
        if not valve_open(net.nodes[v].cell) then
            shut[v] = true
            table.insert(sig, "v" .. v)
        end
    end
    local wk = fk .. "|" .. table.concat(sig, ";")
    local kept = net.ways[wk]
    if kept then return kept end
    if net.nways >= MAX_ROUTES then
        net.ways, net.nways = {}, 0
    end
    local w: Ways = { dist = { [fk] = 0 }, via = {}, back = {}, ends = {} }
    local done: { [string]: boolean } = {}
    local h = {}
    push(h, 0, fk)
    while #h > 0 do
        local top = pop(h)
        local d, k = top[1] :: number, top[2] :: string
        if not done[k] then
            done[k] = true
            local node = net.nodes[k]
            if node.kind == KINDS.interface then table.insert(w.ends, k) end
            for _, e in node.edges do
                if not shut[e.to] then
                    local nd = d + e.len
                    local had = w.dist[e.to]
                    if had == nil or nd < had then
                        w.dist[e.to], w.via[e.to], w.back[e.to] = nd, e, k
                        push(h, nd, e.to)
                    end
                end
            end
        end
    end
    net.ways[wk] = w
    net.nways += 1
    return w
end

-- The cells from `from` (the node the ways start at) to the node `to`.
local function way(w: Ways, from: Cell, to: string): { Cell }
    local edges = {}
    local k = to
    while w.back[k] do
        table.insert(edges, w.via[k])
        k = w.back[k]
    end
    local path = { from }
    for i = #edges, 1, -1 do
        for _, c in edges[i].cells do table.insert(path, c) end
    end
    return path
end

-- Where an item should go from here: the nearest of the best class.
local function route(from: Cell, item: number, skip: { [string]: boolean }): ({ Cell }?, number?)
    local net = net_of(from)
    local w = if net then ways_of(net, from, item) else nil
    if not net or not w then return nil, nil end
    -- In each class the highest priority, and of those the nearest (the ends
    -- come nearest first).
    local best: { [number]: string } = {}
    local prio: { [number]: number } = {}
    for _, k in w.ends do
        if not skip[k] then
            local cl, pr = class_of(net.nodes[k].cell, item)
            if cl and (not best[cl] or pr > prio[cl]) then best[cl], prio[cl] = k, pr end
        end
    end
    for cl = 1, 3 do
        if best[cl] then return way(w, from, best[cl]), cl end
    end
    return nil, nil
end

-- ---------------------------------------------------------------- stacks in transit
type Stack = { e: number?, item: number, n: number, a: Cell, b: Cell, t: number, speed: number,
               path: { Cell }?, i: number, dest: string?, requested: boolean, owed: number, age: number,
               skip: { [string]: boolean }, into: Cell? }
local moving: { Stack } = {}
-- Something brought to a machine (the machines' section sets this up).
local fed: (Cell) -> () = function(_) end
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

-- `into`: the machine the stack is for, beside the last cell of its way.
local function send(path: { Cell }, item: number, n: number, style: { aura: string, speed: number },
                    requested: boolean, into: Cell?)
    local a = path[1]
    local s: Stack = { e = nil, item = item, n = n, a = a, b = path[2] or a, t = 0, speed = style.speed,
                       path = path, i = 1, dest = key(path[#path]), requested = requested, owed = n, age = 0, skip = {},
                       into = into }
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
        local net = net_of(c)
        local w = if net then ways_of(net, c, s.item) else nil
        if w and w.dist[dest] then return follow(s, c, way(w, c, dest)) end
    end
    settle(s)
    s.into = nil
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
    local into = s.into
    local inv = if into and core.container_fill(into[1], into[2], into[3]) >= 0 then into else inv_of(c)
    if inv and inv == into then fed(inv) end
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
        if nxt and kind_at(nxt) == KINDS.valve and not valve_open(nxt) then
            -- A shut valve ahead: it waits here for it to open.
            s.a, s.b = c, c
            return
        end
        s.i += 1
        if nxt and kind_at(nxt) and not blocks(nxt, s.item) then
            -- On its way, through item pipes too: the way was worked out
            -- whole (the graph's), and a crossing of item pipes sends a stack
            -- at random only when it has none.
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
    clock += ev.dt
    if #moving == 0 then return end
    local keep = {}
    for _, s in moving do
        if core.loaded(s.b[1], s.b[2], s.b[3]) == 0 then
            -- The cell ahead is not in memory (a chunk let go, or not read
            -- back yet after a restart): it waits at the edge rather than
            -- taking that air for the end of the pipe and falling out.
            table.insert(keep, s)
            continue
        end
        s.age += ev.dt
        local slow = if kind_at(s.b) == KINDS.latency or kind_at(s.a) == KINDS.latency then LATENCY_SLOW else 1
        s.t += s.speed * ev.dt * slow
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

-- ---------------------------------------------------------------- across a restart
-- What is in the pipes is kept in the mod's storage every few seconds and put
-- back in them when the world is read again: each stack's thing and count,
-- the cells it is between, where it was going and into what, and whether it
-- was asked for. It sets off again from where it was and finds its way at
-- the next routed pipe. A stock keeper's count of what is on its way to it
-- is made again from the stacks. A storage value holds 4 KB, so the stacks
-- go in as many values as they need ("transit_1", ...; "transit_n" says how
-- many). What machines had been brought stays in their containers, and goes
-- back to the store when they find themselves idle.
local SAVE_EVERY = 3
local KEEP_BYTES = 3800
local saved_parts = tonumber(core.storage_get("transit_n")) or 0

local function cell_text(c: Cell?): string
    return if c then `{c[1]},{c[2]},{c[3]}` else "-"
end
local function text_cell(t: string): Cell?
    local x, y, z = string.match(t, "^(-?%d+),(-?%d+),(-?%d+)$")
    return if x then { tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number } else nil
end

local function save_transit()
    local parts, cur = {}, {}
    local size = 0
    for _, st in moving do
        if st.n > 0 then
            local rec = `{core.item_name(st.item)}|{st.n}|{cell_text(st.a)}|{cell_text(st.b)}|{st.dest or "-"}|`
                .. `{cell_text(st.into)}|{if st.requested then 1 else 0}`
            if size + #rec + 1 > KEEP_BYTES and #cur > 0 then
                table.insert(parts, table.concat(cur, ";"))
                cur, size = {}, 0
            end
            table.insert(cur, rec)
            size += #rec + 1
        end
    end
    if #cur > 0 then table.insert(parts, table.concat(cur, ";")) end
    for i, text in parts do core.storage_set(`transit_{i}`, text) end
    for i = #parts + 1, saved_parts do core.storage_set(`transit_{i}`, "") end
    saved_parts = #parts
    core.storage_set("transit_n", tostring(#parts))
end

local function restore_transit()
    local n = tonumber(core.storage_get("transit_n")) or 0
    local back = 0
    for i = 1, n do
        for rec in string.gmatch(core.storage_get(`transit_{i}`), "[^;]+") do
            local name, count, a, b, dest, into, req =
                string.match(rec, "^([^|]+)|(%d+)|([^|]+)|([^|]+)|([^|]+)|([^|]+)|([01])$")
            local item = if name then core.item_id(name :: string) else 0
            local ca, cb = text_cell(a or ""), text_cell(b or "")
            if item ~= 0 and ca and cb then
                local st: Stack = { e = nil, item = item, n = tonumber(count) :: number, a = ca, b = cb, t = 0,
                                    speed = REQUESTED.speed, path = nil, i = 1,
                                    dest = if dest ~= "-" then dest else nil, requested = req == "1",
                                    owed = tonumber(count) :: number, age = 0, skip = {}, into = text_cell(into or "") }
                local e = core.ecs.spawn("pipes:stack", ca[1] + 0.5, ca[2] + 0.33, ca[3] + 0.5)
                if e then
                    st.e = e
                    core.ecs.visible(e, true)
                end
                look(st, if st.requested then REQUESTED elseif st.dest then CLASS[1] else LOST)
                table.insert(moving, st)
                if st.requested and st.dest then
                    local p = pending[st.dest :: string] or {}
                    pending[st.dest :: string] = p
                    p[item] = (p[item] or 0) + st.n
                end
                back += 1
            end
        end
    end
    if back > 0 then core.log(`pipes: {back} stacks back in the pipes, as they were`) end
end

core.event.on("core:world_ready", function(_) restore_transit() end)
local save_clock = 0
core.event.on("core:tick", function(ev)
    save_clock += ev.dt
    if save_clock >= SAVE_EVERY then
        save_clock = 0
        save_transit()
    end
end)

-- ---------------------------------------------------------------- what chips do
local function take(inv: Cell, item: number, n: number): number
    local got = core.container_take(inv[1], inv[2], inv[3], item, n, "store")
    if got < n then got += core.container_take(inv[1], inv[2], inv[3], item, n - got, "product") end
    return got
end

-- What a container offers the network, item by item: its "store" and
-- "product" squares (all of a chest, a furnace's finished things). Kept until
-- the container changes, as a store of many chests is read on every request.
local offers_kept: { [string]: { rev: number, have: { [number]: number } } } = {}
local function offers(inv: Cell): { [number]: number }
    local k = key(inv)
    local rev = core.container_revision(inv[1], inv[2], inv[3])
    local kept = offers_kept[k]
    if kept and kept.rev == rev then return kept.have end
    local have: { [number]: number } = {}
    local seen: { [number]: boolean } = {}
    for slot = 1, INV_SLOTS do
        local item = core.container_item(inv[1], inv[2], inv[3], slot)
        if item ~= 0 and not seen[item] then
            seen[item] = true
            local n = core.container_count(inv[1], inv[2], inv[3], item, "store")
                + core.container_count(inv[1], inv[2], inv[3], item, "product")
            if n > 0 then have[item] = n end
        end
    end
    offers_kept[k] = { rev = rev, have = have }
    return have
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

-- The broadcasters on the network of `to`, nearest first: each one's container
-- and the way from it to `to`, worked out only when asked for. One container
-- with two broadcasters beside it counts once.
type Offer = { inv: Cell, at: Cell, prio: number, order: number, path: () -> { Cell } }
local function broadcasters(to: Cell, item: number): { Offer }
    local net = net_of(to)
    local w = if net then ways_of(net, to, item) else nil
    if not net or not w then return {} end
    local out: { Offer } = {}
    local seen: { [string]: boolean } = {}
    local me = key(to)
    for _, k in w.ends do
        local c = net.nodes[k].cell
        if k ~= me and has_chip(c, CHIP.broadcaster) then
            local inv = inv_of(c)
            if inv and not seen[key(inv)] then
                seen[key(inv)] = true
                local ww: Ways, at: string = w, k
                table.insert(out, { inv = inv, at = c, prio = (function()
                    for _, r in rows_of(c) do
                        if r.chip == CHIP.broadcaster then return r.prio end
                    end
                    return 1
                end)(), order = #out, path = function()
                    local there = way(ww, to, at)
                    local back = {}
                    for i = #there, 1, -1 do table.insert(back, there[i]) end
                    return back
                end })
            end
        end
    end
    -- The higher priority first (a stack of broadcaster chips), nearest first among equals.
    table.sort(out, function(a, b)
        if a.prio ~= b.prio then return a.prio > b.prio end
        return a.order < b.order
    end)
    return out
end

-- Everything the broadcasters on the network of `c` offer and can send to it,
-- and from how many containers. A thing a firewall keeps from `c` is counted
-- only from the broadcasters it can still come from: what is listed is what
-- a request gets.
local function stock(c: Cell): ({ [number]: number }, number)
    local total: { [number]: number } = {}
    local list = broadcasters(c, 0)
    local net = net_of(c)
    local walls = net ~= nil and #(net :: Net).firewalls > 0
    for _, o in list do
        for item, n in offers(o.inv) do
            local ok = true
            if walls then
                local w = ways_of(net :: Net, c, item)
                ok = w ~= nil and w.dist[key(o.at)] ~= nil
            end
            if ok then total[item] = (total[item] or 0) + n end
        end
    end
    return total, #list
end

-- Sends up to n of an item from the broadcasters to `to`; how many set off.
local function request(to: Cell, item: number, n: number): number
    local sent = 0
    for _, o in broadcasters(to, item) do
        if (offers(o.inv)[item] or 0) > 0 then
            local path: { Cell }? = nil
            while sent < n do
                local got = take(o.inv, item, math.min(n - sent, core.item_stack(item)))
                if got == 0 then break end
                path = path or o.path()
                send(path :: { Cell }, item, got, REQUESTED, true)
                sent += got
            end
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

-- ---------------------------------------------------------------- crafting with machines
-- A network makes things only with its machines: an AUTOMATIC CRAFTING TABLE
-- for the crafting recipes, an AUTOMATIC FURNACE for what a furnace smelts (it
-- burns fuel from the store as a furnace does) -- and each machine only what
-- the recipe chips beside it name (abilities, below). They work in chains: what is
-- asked for is planned down to what the store holds, through up to MAX_DEPTH
-- recipes -- logs to planks to sticks to a pick -- and each step is done by a
-- machine: its ingredients brought to it through the pipes, CRAFT_SECONDS a
-- craft or SMELT_SECONDS a smelt, what it makes sent on to the machine of the
-- next step or, at the last, to whoever asked.
local MAX_DEPTH = 6          -- recipes in one chain
local CRAFT_SECONDS = 1.0    -- a craft, at an automatic crafting table
local SMELT_SECONDS = 2.0    -- a smelt, at an automatic furnace (mods/furnace's COOK)
local WAIT_FOR = 90          -- seconds a step waits for its ingredients before its job is given up
local IDLE_FLUSH = 5         -- seconds a machine with nothing to do keeps what is left in it
local PLAN_BUDGET = 4000     -- recipes tried in one plan, so a store of everything stays quick

-- One way of making a thing: a recipe of a table or a smelt of a furnace.
-- `leaves`: what its ingredients leave behind when spent (core.item_leaves: a
-- bucket of milk leaves the bucket), given back by the machine that makes it.
type Way = { kind: string, makes: number, yield: number, needs: { [number]: number }, leaves: { [number]: number } }
local ways_by: { [number]: { Way } }? = nil
local fuels: { { id: number, smelts: number } } = {}
local smelt_names: { { string } } = {}
local fuel_names: { { any } } = {}
core.event.on("furnace:smelt", function(ev) table.insert(smelt_names, { ev.from, ev.to }) end)
core.event.on("furnace:fuel", function(ev) table.insert(fuel_names, { ev.item, ev.smelts }) end)

-- Every way of making each thing, by the thing's id: the engine's recipes
-- (core.recipe_*) and mods/furnace's smelts (asked for with furnace:ask).
-- Read the first time it is needed, when every mod has had its say.
local function ways_to_make(): { [number]: { Way } }
    if ways_by then return ways_by end
    local out: { [number]: { Way } } = {}
    local function add(w: Way)
        out[w.makes] = out[w.makes] or {}
        table.insert(out[w.makes], w)
    end
    for i = 1, core.recipe_count() do
        local makes, yield = core.item_id(core.recipe_result(i)), core.recipe_yield(i)
        local needs: { [number]: number } = {}
        local ok = makes ~= 0 and yield > 0
        for name in string.gmatch(core.recipe_items(i), "%S+") do
            local id = core.item_id(name)
            if id == 0 then
                ok = false
                break
            end
            needs[id] = (needs[id] or 0) + 1
        end
        if ok and next(needs) ~= nil then
            local leaves: { [number]: number } = {}
            for id, k in needs do
                local back = core.item_id(core.item_leaves(core.item_name(id)))
                if back ~= 0 then leaves[back] = (leaves[back] or 0) + k end
            end
            add({ kind = "craft", makes = makes, yield = yield, needs = needs, leaves = leaves })
        end
    end
    smelt_names, fuel_names = {}, {}
    core.event.emit("furnace:ask", { who = "pipes" })
    for _, p in smelt_names do
        local from, to = core.item_id(p[1]), core.item_id(p[2])
        if from ~= 0 and to ~= 0 then add({ kind = "smelt", makes = to, yield = 1, needs = { [from] = 1 }, leaves = {} }) end
    end
    for _, f in fuel_names do
        local id = core.item_id(f[1])
        if id ~= 0 then table.insert(fuels, { id = id, smelts = f[2] }) end
    end
    ways_by = out
    return out
end

-- A step of a plan: a way, done `times` times, by a machine of its kind. `uses`
-- is what it takes from the store (fetched to its machine when the job
-- starts), `needs` all it has to have in its machine before it starts (the
-- store's and what earlier steps send it). What it makes goes to `parent`, the
-- step that needs it, or to whoever asked when it has none.
type Step = { way: Way, times: number, uses: { [number]: number }, needs: { [number]: number },
              parent: Step?, job: Job?, machine: Machine?, since: number, children: number }
type Job = { to: Cell, item: number, steps: { Step }, player: number, dead: boolean }
type Abil = { [string]: { [number]: { Spot } } }
type Machine = { cell: Cell, pipe: Cell, kind: string, queue: { Step }, current: Step?, done_at: number,
                 idle_since: number, fed: number }

-- Plans how to make q of an item out of `stock` (taken from it as the plan
-- goes) with the machines there are; the steps in the order they can be done,
-- or nil. Tried recipe by recipe, each undone if a part of it cannot be had.
-- `dry`: the store is given back as it was even when it works (a question,
-- not a job).
local function plan(stock: { [number]: number }, item: number, q: number, can: Abil,
                    dry: boolean?): { Step }?
    local log: { { number } } = {}
    local steps: { Step } = {}
    local busy: { [number]: boolean } = {}
    local budget = PLAN_BUDGET
    local function undo(to: number)
        for i = #log, to + 1, -1 do
            local e = log[i]
            stock[e[1]] = (stock[e[1]] or 0) + e[2]
            log[i] = nil
        end
    end
    local make: (number, number, Step?, number) -> boolean
    -- q of an item for step `st`: from the store as far as it goes, the rest made.
    local function need(id: number, q: number, st: Step, depth: number): boolean
        st.needs[id] = (st.needs[id] or 0) + q
        local u = math.min(math.max(stock[id] or 0, 0), q)
        if u > 0 then
            stock[id] -= u
            table.insert(log, { id, u })
            st.uses[id] = (st.uses[id] or 0) + u
        end
        return u == q or make(id, q - u, st, depth)
    end
    make = function(id: number, q: number, parent: Step?, depth: number): boolean
        local options = ways_to_make()[id]
        if not options or depth > MAX_DEPTH or busy[id] then return false end
        busy[id] = true
        for _, w in options do
            local a = can[w.kind]
            if a and a[w.makes] then
                budget -= 1
                if budget < 0 then break end
                local mark, made = #log, #steps
                local t = math.ceil(q / w.yield)
                local st: Step = { way = w, times = t, uses = {}, needs = {}, parent = parent, since = 0, children = 0 }
                local ok = true
                for need_id, k in w.needs do
                    if not need(need_id, k * t, st, depth + 1) then
                        ok = false
                        break
                    end
                end
                if ok and w.kind == "smelt" then
                    -- And what it burns: the first fuel there is enough of.
                    ok = false
                    for _, f in fuels do
                        local fmark, fneeds, fuses = #log, table.clone(st.needs), table.clone(st.uses)
                        if need(f.id, math.ceil(t / f.smelts), st, depth + 1) then
                            ok = true
                            break
                        end
                        undo(fmark)
                        st.needs, st.uses = fneeds, fuses
                    end
                end
                if ok then
                    table.insert(steps, st)
                    busy[id] = nil
                    return true
                end
                undo(mark)
                for i = #steps, made + 1, -1 do steps[i] = nil end
            end
        end
        busy[id] = nil
        return false
    end
    local ok = make(item, q, nil, 0)
    if dry or not ok then undo(0) end
    if not ok then return nil end
    for _, st in steps do
        if st.parent then (st.parent :: Step).children += 1 end
    end
    return steps
end

-- What the machines on a network can make: a machine makes only what the
-- RECIPE CHIPS beside it name. A recipe chip goes in a row of an interface pipe
-- that touches the machine, and the things in that row's filter are its
-- recipes -- up to eight a row, four rows an interface, and as many
-- interfaces as the machine has sides free. For a furnace they are what it
-- smelts things into. [kind][item] = the machines (their spots) that make it.
-- A crafting pipe's recipes and those of the extension pipes joined to it
-- (touching it, or touching one that does): the names in their cells' "r1" ..
-- "r9", chosen on their panel -- no thing needed to name one.
local MAX_EXTENSIONS = 16
local function recipes_of_pipe(c: Cell): { number }
    local out: { number } = {}
    local seen: { [string]: boolean } = { [key(c)] = true }
    local queue, head = { c }, 1
    while head <= #queue and head <= MAX_EXTENSIONS + 1 do
        local p = queue[head]
        head += 1
        for i = 1, RECIPE_SLOTS do
            local name = core.meta_get(p[1], p[2], p[3], `r{i}`)
            local id = if name ~= "" then core.item_id(name) else 0
            if id ~= 0 then table.insert(out, id) end
        end
        for d = 1, 6 do
            local n = near(p, d)
            if not seen[key(n)] and kind_at(n) == KINDS.extension then
                seen[key(n)] = true
                table.insert(queue, n)
            end
        end
    end
    return out
end

-- Or by a crafting pipe touching the machine, its recipes chosen on its panel.
local function abilities(net: Net?): Abil
    local can: Abil = { craft = {}, smelt = {} }
    if not net then return can end
    for _, spot in net.machines do
        local function knows(item: number)
            local by = can[spot.kind]
            by[item] = by[item] or {}
            if not table.find(by[item], spot) then table.insert(by[item], spot) end
        end
        for d = 1, 6 do
            local n = near(spot.cell, d)
            local k = kind_at(n)
            if k == KINDS.interface then
                for _, r in rows_of(n) do
                    if r.chip == CHIP.recipe then
                        for item in r.filter do knows(item) end
                    end
                end
            elseif k == KINDS.crafting then
                for _, item in recipes_of_pipe(n) do knows(item) end
            end
        end
    end
    return can
end

-- Of the things the network could make, which: [item] = true. A plan of one of
-- each, every one undone.
local function makeable(stock: { [number]: number }, can: Abil): { [number]: boolean }
    local out: { [number]: boolean } = {}
    if next(can.craft) == nil and next(can.smelt) == nil then return out end
    for item, ws in ways_to_make() do
        for _, w in ws do
            local a = can[w.kind]
            if a and a[item] then
                if plan(stock, item, 1, can, true) then out[item] = true end
                break
            end
        end
    end
    return out
end

-- ---------------------------------------------------------------- the machines at work
local machines: { [string]: Machine } = {}   -- by the machine's cell
local jobs: { Job } = {}
fed = function(c: Cell)
    local m = machines[key(c)]
    if m then m.fed = clock end
end

local function item_said(item: number): string
    local n = core.item_name(item)
    n = string.gsub(string.match(n, ":(.*)") or n, "_", " ")
    return n
end

local function machine(spot: Spot): Machine
    local k = key(spot.cell)
    local m = machines[k]
    if not m then
        m = { cell = spot.cell, pipe = spot.pipe, kind = spot.kind, queue = {}, current = nil, done_at = 0,
              idle_since = clock, fed = clock }
        machines[k] = m
    end
    m.pipe = spot.pipe
    return m
end

-- Standing or at work: the block (and its lamp, and the furnace's fire) says which.
local function show_working(m: Machine, on: boolean)
    local c = m.cell
    local ids = MACHINE_IDS[m.kind]
    local want = if on then ids.on else ids.off
    local now = core.get_block(c[1], c[2], c[3])
    if now ~= want and MACHINE[now] then
        local facing = core.get_facing(c[1], c[2], c[3])
        core.set_block(c[1], c[2], c[3], want)
        core.set_facing(c[1], c[2], c[3], facing)
    end
end

-- Sends n of an item from a machine's pipe to `to` along the network, into the
-- machine `into` there if it is one; dropped at the machine when there is no way.
local function ship(from: Machine, to: Cell, item: number, n: number, into: Cell?, requested: boolean)
    local net = net_of(from.pipe)
    local w = if net then ways_of(net, from.pipe, item) else nil
    local tk = key(to)
    if not w or not w.dist[tk] then
        core.drop_item(from.cell[1], from.cell[2] + 1, from.cell[3], item, n)
        return
    end
    local path = way(w, from.pipe, tk)
    while n > 0 do
        local k = math.min(n, core.item_stack(item))
        send(path, item, k, REQUESTED, requested, into)
        n -= k
    end
end

-- Brings n of an item from the broadcasters to a machine, the nearest first.
local function fetch(m: Machine, item: number, n: number): number
    local got = 0
    for _, o in broadcasters(m.pipe, item) do
        if got >= n then break end
        if (offers(o.inv)[item] or 0) > 0 then
            local path: { Cell }? = nil
            while got < n do
                local k = take(o.inv, item, math.min(n - got, core.item_stack(item)))
                if k == 0 then break end
                path = path or o.path()
                send(path :: { Cell }, item, k, REQUESTED, false, m.cell)
                got += k
            end
        end
    end
    return got
end

local function give_up(job: Job, why: string)
    if job.dead then return end
    job.dead = true
    for _, m in machines do
        local keep = {}
        for _, st in m.queue do
            if st.job ~= job then table.insert(keep, st) end
        end
        m.queue = keep
    end
    if job.player ~= 0 then core.notice(job.player, why) end
end

-- Starts a job: q of an item for `to`, planned out of the store of its network.
-- How many will come, 0 when it cannot be made.
local function craft(to: Cell, item: number, q: number, player: number): number
    local net = net_of(to)
    if not net or q <= 0 then return 0 end
    local can = abilities(net)
    if next(can.craft) == nil and next(can.smelt) == nil then return 0 end
    local base = stock(to)
    local steps = plan(table.clone(base), item, q, can)
    if not steps then
        -- Not all of it: the most that can be, found by halves.
        local lo, hi = 0, q - 1
        local best: { Step }? = nil
        while lo < hi do
            local mid = (lo + hi + 1) // 2
            local s = plan(table.clone(base), item, mid, can)
            if s then lo, best = mid, s else hi = mid - 1 end
        end
        steps, q = best, lo
    end
    if not steps or q == 0 then return 0 end
    -- Each step to a machine that has its recipe chip: the nearest, the least
    -- busy of the near ones.
    local w = ways_of(net, to, 0)
    local job: Job = { to = to, item = item, steps = steps, player = player, dead = false }
    for _, st in steps do
        local best, score = nil, math.huge
        for _, spot in can[st.way.kind][st.way.makes] do
            local m = machine(spot)
            local sc = ((if w then w.dist[key(spot.pipe)] else nil) or 1e6) + 16 * #m.queue
            if sc < score then best, score = m, sc end
        end
        st.job, st.machine, st.since = job, best, clock
    end
    for _, st in steps do
        local m = st.machine :: Machine
        for id, n in st.uses do fetch(m, id, n) end
        table.insert(m.queue, st)
    end
    table.insert(jobs, job)
    local last = steps[#steps]
    return last.times * last.way.yield
end

local function has_all(m: Machine, st: Step): boolean
    local c = m.cell
    for id, n in st.needs do
        if core.container_count(c[1], c[2], c[3], id) < n then return false end
    end
    return true
end

-- A step done: what it made goes on.
local function step_done(m: Machine, st: Step)
    local made = st.times * st.way.yield
    local item = st.way.makes
    local job = st.job :: Job
    if job.dead then
        core.container_add(m.cell[1], m.cell[2], m.cell[3], item, made)
        return
    end
    local p = st.parent
    if p then
        p.children -= 1
        p.since = clock
    end
    if not p then
        ship(m, job.to, item, made, nil, true)
        job.dead = true      -- done
    elseif p.machine == m then
        local left = core.container_add(m.cell[1], m.cell[2], m.cell[3], item, made)
        if left > 0 then core.drop_item(m.cell[1], m.cell[2] + 1, m.cell[3], item, left) end
    else
        local pm = p.machine :: Machine
        ship(m, pm.pipe, item, made, pm.cell, false)
    end
end

-- What is left in a machine with nothing to do goes back: a recipe's yield over
-- what was needed, what a job given up had brought. To whatever takes it on the
-- network (a responder, a terminator, an overflow), else into the store of
-- the nearest broadcaster.
local function flush(m: Machine)
    local c = m.cell
    for slot = 1, MACHINE_SLOTS do
        local item = core.container_item(c[1], c[2], c[3], slot)
        if item ~= 0 then
            local path, cl = route(m.pipe, item, {})
            if path and cl then
                local n = core.container_take(c[1], c[2], c[3], item, core.item_stack(item))
                if n > 0 then send(path, item, n, CLASS[cl], false) end
                return   -- one stack a turn
            end
            local home = broadcasters(m.pipe, item)[1]
            if home then
                local n = core.container_take(c[1], c[2], c[3], item, core.item_stack(item))
                if n > 0 then ship(m, home.at, item, n, home.inv, false) end
                return
            end
        end
    end
end

local work_machines: () -> ()
core.event.on("core:tick", function(_)
    if next(machines) ~= nil then work_machines() end
end)
work_machines = function()
    for k, m in machines do
        if not machine_at(m.cell) then
            -- Broken: its jobs with it.
            for _, st in m.queue do give_up(st.job :: Job, "A machine of the network was taken away") end
            machines[k] = nil
            continue
        end
        if m.current then
            if clock >= m.done_at then
                local st = m.current :: Step
                m.current = nil
                step_done(m, st)
                if #m.queue == 0 then
                    show_working(m, false)
                    m.idle_since = clock
                end
            end
        else
            -- The first step in the queue that has all it needs: a step after
            -- one still waiting may already have its own.
            for i, st in m.queue do
                if has_all(m, st) then
                    local c = m.cell
                    for id, n in st.needs do core.container_take(c[1], c[2], c[3], id, n) end
                    -- What the spent things leave (a bucket): back into the
                    -- machine, which gives it back to the store when it is idle.
                    for id, n in st.way.leaves do
                        local left = core.container_add(c[1], c[2], c[3], id, n * st.times)
                        if left > 0 then core.drop_item(c[1], c[2] + 1, c[3], id, left) end
                    end
                    table.remove(m.queue, i)
                    m.current = st
                    m.done_at = clock + st.times * (if m.kind == "smelt" then SMELT_SECONDS else CRAFT_SECONDS)
                    m.fed = clock
                    show_working(m, true)
                    break
                elseif st.children == 0 and clock - math.max(st.since, m.fed) > WAIT_FOR then
                    -- Nothing more coming for it, and nothing come to the machine for a while.
                    give_up(st.job :: Job, `Gave up making {item_said((st.job :: Job).item)}: what it needs did not arrive`)
                    break
                end
            end
            if not m.current and #m.queue == 0 and clock - m.idle_since > IDLE_FLUSH then flush(m) end
        end
    end
    local alive = {}
    for _, j in jobs do
        if not j.dead then table.insert(alive, j) end
    end
    jobs = alive
end

-- ---------------------------------------------------------------- the request panel
-- A grid of the things the network holds or can make, as the bag shows them:
-- the picture, its name under the pointer, how many in the corner, and a
-- small wrench at the bottom right of what the network can craft. Under it
-- the pages, the order, what to show, a search by name or id, and the amount
-- a click asks for. A panel answers each click; a store of thousands of things
-- is GRID at a time.
local COLS, LINES = 9, 4
local GRID = COLS * LINES
local SIDE, STEP = 1.0, 1.08
local MARK = 0.42                       -- the wrench's side
local PW = 0.8 + (COLS - 1) * STEP + SIDE
local PANELS = 6                        -- panels in turn: the engine has 64 for every mod there is
local BH, ROW_STEP = 0.8, 0.92          -- a control's height, and from one row of them to the next

type View = { cell: Cell, find: string, order: string, show: string, page: number, amount: number,
              shown: { number }, slot: number, used: number }
local views: { [number]: View } = {}            -- the player's open request panel
local liked: { [number]: { order: string, amount: number } } = {}   -- kept from one pipe to the next
local holder: { [number]: number } = {}         -- the player each panel is up for

local function panel_of(slot: number): string return `pipes:request_{slot}` end

local function close_request(player: number)
    local v = views[player]
    if not v then return end
    core.ui_show(panel_of(v.slot), false, player)
    if holder[v.slot] == player then holder[v.slot] = nil end
    views[player] = nil
end

local function free_slot(player: number): number
    for s = 1, PANELS do
        if not holder[s] then
            holder[s] = player
            return s
        end
    end
    -- All up: the one looked at longest ago is taken off its player.
    local old, when = 1, math.huge
    for s = 1, PANELS do
        local v = views[holder[s]]
        if not v then
            old = s
            break
        end
        if v.used < when then old, when = s, v.used end
    end
    if holder[old] then close_request(holder[old]) end
    holder[old] = player
    return old
end

local function shown_name(item: number): string
    local n = core.item_name(item)
    n = string.gsub(string.match(n, ":(.*)") or n, "_", " ")
    return string.upper(string.sub(n, 1, 1)) .. string.sub(n, 2)
end
local function short(n: number): string
    if n < 1000 then return tostring(n) end
    if n < 10000 then return string.format("%.1fk", n / 1000) end
    if n < 1000000 then return `{n // 1000}k` end
    return string.format("%.1fM", n / 1000000)
end
-- What a player wrote: up to 40 bytes, no line breaks.
local function clean(t: string?): string
    return string.sub((string.gsub(t or "", "[%c]", "")), 1, 40)
end

type Entry = { item: number, n: number, can: number, name: string, id: string }
local function draw_request(player: number)
    local v = views[player]
    if not v then return end
    local stock, sources = stock(v.cell)
    local can = makeable(stock, abilities(net_of(v.cell)))
    local find = string.lower(v.find)
    local list: { Entry } = {}
    local kinds, things = 0, 0
    local function add(item: number)
        local n, c = stock[item] or 0, if can[item] then 1 else 0
        local id = core.item_name(item)
        local name = shown_name(item)
        if (v.show == "all" or (v.show == "store" and n > 0) or (v.show == "craft" and c > 0))
            and (find == "" or string.find(string.lower(name), find, 1, true) ~= nil
                 or string.find(string.lower(id), find, 1, true) ~= nil) then
            table.insert(list, { item = item, n = n, can = c, name = name, id = id })
        end
    end
    for item, n in stock do
        kinds += 1
        things += n
        add(item)
    end
    for item in can do
        if not stock[item] then add(item) end
    end
    if v.order == "count" then
        table.sort(list, function(a, b)
            if a.n ~= b.n then return a.n > b.n end
            return a.name < b.name
        end)
    else
        table.sort(list, function(a, b)
            if a.name ~= b.name then return a.name < b.name end
            return a.item < b.item
        end)
    end
    local pages = math.max(1, math.ceil(#list / GRID))
    v.page = math.clamp(v.page, 1, pages)

    local P = panel_of(v.slot)
    local gy = 0.8                                  -- the grid's top
    local cy = gy + (LINES - 1) * STEP + SIDE + 0.3 -- the controls' top
    if core.ui_define(P, PW, cy + 2 * ROW_STEP + BH + 0.15) == 0 then return end
    core.ui_label(P, 0.4, 0.2, if sources == 0 then "No broadcaster on this network offers anything."
                               else `Request pipe: {kinds} kinds, {things} in all`)
    core.ui_box(P, 0.3, gy - 0.1, PW - 0.6, (LINES - 1) * STEP + SIDE + 0.2, 28, 30, 36)
    v.shown = {}
    for k = 1, GRID do
        local e = list[(v.page - 1) * GRID + k]
        if not e then break end
        local x = 0.4 + ((k - 1) % COLS) * STEP
        local y = gy + ((k - 1) // COLS) * STEP
        v.shown[k] = e.item
        core.ui_item(P, `i{k}`, x, y, SIDE, SIDE, e.id)
        if e.n > 0 then core.ui_label(P, x + 0.06, y + 0.03, short(e.n)) end
        -- A picture only (no id): the pointer stays on the thing under it.
        if e.can > 0 then core.ui_item(P, "", x + SIDE - MARK, y + SIDE - MARK, MARK, MARK, "pipes:wrench") end
    end
    if #list == 0 and sources > 0 then core.ui_label(P, 0.6, gy + 0.3, "Nothing here matches.") end

    -- Pages, order, what to show.
    core.ui_button(P, "prev", 0.4, cy, 0.9, BH, "<")
    core.ui_label(P, 1.45, cy + 0.23, `{v.page} / {pages}`)
    core.ui_button(P, "next", 2.9, cy, 0.9, BH, ">")
    core.ui_button(P, "order", 4.0, cy, 3.0, BH, if v.order == "count" then "By quantity" else "By name")
    core.ui_button(P, "show", 7.2, cy, PW - 7.6, BH,
                   if v.show == "store" then "In store" elseif v.show == "craft" then "Craftable" else "All")
    -- The search.
    local sy = cy + ROW_STEP
    core.ui_label(P, 0.4, sy + 0.23, "Search:")
    core.ui_field(P, "find", 2.0, sy, if v.find ~= "" then PW - 4.8 else PW - 2.4, BH, v.find)
    if v.find ~= "" then core.ui_button(P, "clear", PW - 2.6, sy, 2.2, BH, "Clear") end
    -- The amount a click asks for.
    local ay = sy + ROW_STEP
    core.ui_label(P, 0.4, ay + 0.23, if v.amount < 0 then "Ask for: a stack" else `Ask for: {v.amount}`)
    core.ui_button(P, "a1", 4.0, ay, 0.9, BH, "1")
    core.ui_button(P, "a8", 5.0, ay, 0.9, BH, "8")
    core.ui_button(P, "astack", 6.0, ay, 1.9, BH, "Stack")
    core.ui_field(P, "amount", 8.0, ay, PW - 8.4, BH, if v.amount > 1 and v.amount ~= 8 then tostring(v.amount) else "")
    core.ui_show(P, true, player)
end

local function open_request(player: number, c: Cell)
    local v = views[player]
    if v and key(v.cell) == key(c) then
        v.used = clock
        draw_request(player)
        return
    end
    if v then close_request(player) end
    local like = liked[player] or { order = "name", amount = 1 }
    views[player] = { cell = c, find = "", order = like.order, show = "all", page = 1, amount = like.amount,
                      shown = {}, slot = free_slot(player), used = clock }
    draw_request(player)
end

local function ask(player: number, v: View, item: number)
    local want = if v.amount < 0 then core.item_stack(item) else v.amount
    local got = request(v.cell, item, want)
    local made = 0
    if got < want then made = craft(v.cell, item, want - got, player) end
    if made > 0 then
        core.notice(player, `{got + made} on the way, {made} being made`)
    elseif got > 0 then
        core.notice(player, `{got} on the way`)
    else
        core.notice(player, "Nothing to send")
    end
end

core.event.on("core:ui_click", function(ev)
    local v = views[ev.player]
    if not v or ev.panel ~= panel_of(v.slot) then return end
    local c = v.cell
    -- The panel is a client's word: only for a request pipe still there, from beside it.
    local px, py, pz = core.player_pos(ev.player)
    if ev.id == "core:esc" or kind_at(c) ~= KINDS.request or not px
        or math.abs(px - c[1]) > 8 or math.abs(py - c[2]) > 8 or math.abs(pz - c[3]) > 8 then
        close_request(ev.player)
        return
    end
    v.used = clock
    local like = liked[ev.player] or { order = "name", amount = 1 }
    liked[ev.player] = like
    local k = tonumber(string.match(ev.id, "^[iw](%d+)$") or "")
    if k then
        local item = v.shown[k]
        if item then ask(ev.player, v, item) end
    elseif ev.id == "prev" then v.page -= 1
    elseif ev.id == "next" then v.page += 1
    elseif ev.id == "order" then
        v.order = if v.order == "count" then "name" else "count"
        v.page, like.order = 1, v.order
    elseif ev.id == "show" then
        v.show = if v.show == "all" then "store" elseif v.show == "store" then "craft" else "all"
        v.page = 1
    elseif ev.id == "find" then v.find, v.page = clean(ev.text), 1
    elseif ev.id == "clear" then v.find, v.page = "", 1
    elseif ev.id == "a1" then v.amount = 1
    elseif ev.id == "a8" then v.amount = 8
    elseif ev.id == "astack" then v.amount = -1
    elseif ev.id == "amount" then
        local n = tonumber(string.match(ev.text or "", "%d+") or "")
        if n then v.amount = math.clamp(math.floor(n), 1, MAX_ASK) end
    else
        return
    end
    like.amount = v.amount
    draw_request(ev.player)
end)

-- ---------------------------------------------------------------- the recipe panel
-- A crafting or an extension pipe's recipes, chosen without holding the thing:
-- at the top its nine, a click takes one off; under them a search and a grid
-- of what the machine it serves can make (a table's recipes or a furnace's
-- smelts; everything when it serves none yet), a click puts one in the first
-- free square. Panels of their own, in turn as the request pipe's.
local RPANELS = 3
local RLINES = 3
type RView = { cell: Cell, find: string, page: number, slot: number, used: number, shown: { number } }
local rviews: { [number]: RView } = {}
local rholder: { [number]: number } = {}
local function rpanel_of(slot: number): string return `pipes:recipes_{slot}` end

local function close_recipes(player: number)
    local v = rviews[player]
    if not v then return end
    core.ui_show(rpanel_of(v.slot), false, player)
    if rholder[v.slot] == player then rholder[v.slot] = nil end
    rviews[player] = nil
end
local function free_rslot(player: number): number
    for s = 1, RPANELS do
        if not rholder[s] then
            rholder[s] = player
            return s
        end
    end
    local old, when = 1, math.huge
    for s = 1, RPANELS do
        local v = rviews[rholder[s]]
        if v and v.used < when then old, when = s, v.used end
    end
    if rholder[old] then close_recipes(rholder[old]) end
    rholder[old] = player
    return old
end

-- The machine a crafting pipe serves -- for an extension, its crafting
-- pipe's: what kind of recipes to offer.
local function served_kind(c: Cell): string?
    local seen: { [string]: boolean } = { [key(c)] = true }
    local queue, head = { c }, 1
    while head <= #queue and head <= MAX_EXTENSIONS + 1 do
        local p = queue[head]
        head += 1
        if kind_at(p) == KINDS.crafting then
            for d = 1, 6 do
                local m = machine_at(near(p, d))
                if m then return m end
            end
        end
        for d = 1, 6 do
            local n = near(p, d)
            local k = kind_at(n)
            if not seen[key(n)] and (k == KINDS.extension or k == KINDS.crafting) then
                seen[key(n)] = true
                table.insert(queue, n)
            end
        end
    end
    return nil
end

local function draw_recipes(player: number)
    local v = rviews[player]
    if not v then return end
    local c = v.cell
    local kind = served_kind(c)
    local find = string.lower(v.find)
    local list = {}
    for item, ws in ways_to_make() do
        local fits = kind == nil
        for _, w in ws do
            if w.kind == kind then fits = true end
        end
        local name = core.item_name(item)
        if fits and name ~= "" and (find == "" or string.find(string.lower(shown_name(item)), find, 1, true) ~= nil
                                    or string.find(string.lower(name), find, 1, true) ~= nil) then
            table.insert(list, { item = item, name = shown_name(item) })
        end
    end
    table.sort(list, function(a, b) return a.name < b.name end)
    local per = COLS * RLINES
    local pages = math.max(1, math.ceil(#list / per))
    v.page = math.clamp(v.page, 1, pages)

    local P = rpanel_of(v.slot)
    local cy = 2.1 + (RLINES - 1) * STEP + SIDE + 1.15   -- the grid's foot, the controls under it
    if core.ui_define(P, PW, cy + 1.0) == 0 then return end
    core.ui_label(P, 0.4, 0.2, if kind_at(c) == KINDS.crafting then "Recipes of this crafting pipe"
                               else "Recipes of this extension pipe")
    for i = 1, RECIPE_SLOTS do
        local x = 0.4 + (i - 1) * STEP
        local name = core.meta_get(c[1], c[2], c[3], `r{i}`)
        if name ~= "" and core.item_id(name) ~= 0 then
            core.ui_item(P, `c{i}`, x, 0.75, SIDE, SIDE, name)
        else
            core.ui_box(P, x, 0.75, SIDE, SIDE, 28, 30, 36)
        end
    end
    core.ui_label(P, 0.4, 1.9, if kind == nil then "No machine at its end yet: every recipe is offered"
                               elseif kind == "smelt" then "What the furnace may smelt to (click one to add it)"
                               else "What the crafting table may make (click one to add it)")
    local gy = 2.45
    v.shown = {}
    for k = 1, per do
        local e = list[(v.page - 1) * per + k]
        if not e then break end
        v.shown[k] = e.item
        core.ui_item(P, `a{k}`, 0.4 + ((k - 1) % COLS) * STEP, gy + ((k - 1) // COLS) * STEP, SIDE, SIDE,
                     core.item_name(e.item))
    end
    local ny = gy + RLINES * STEP + 0.1
    core.ui_button(P, "prev", 0.4, ny, 0.9, 0.8, "<")
    core.ui_label(P, 1.45, ny + 0.23, `{v.page} / {pages}`)
    core.ui_button(P, "next", 2.9, ny, 0.9, 0.8, ">")
    core.ui_label(P, 4.1, ny + 0.23, "Search:")
    core.ui_field(P, "find", 5.6, ny, PW - 6.0, 0.8, v.find)
    core.ui_show(P, true, player)
end

local function open_recipes(player: number, c: Cell)
    local v = rviews[player]
    if v and key(v.cell) == key(c) then
        v.used = clock
        draw_recipes(player)
        return
    end
    if v then close_recipes(player) end
    rviews[player] = { cell = c, find = "", page = 1, slot = free_rslot(player), used = clock, shown = {} }
    draw_recipes(player)
end

core.event.on("core:ui_click", function(ev)
    local v = rviews[ev.player]
    if not v or ev.panel ~= rpanel_of(v.slot) then return end
    local c = v.cell
    local k = kind_at(c)
    local px, py, pz = core.player_pos(ev.player)
    if ev.id == "core:esc" or (k ~= KINDS.crafting and k ~= KINDS.extension) or not px
        or math.abs(px - c[1]) > 8 or math.abs(py - c[2]) > 8 or math.abs(pz - c[3]) > 8 then
        close_recipes(ev.player)
        return
    end
    v.used = clock
    local taken = tonumber(string.match(ev.id, "^c(%d+)$") or "")
    local added = tonumber(string.match(ev.id, "^a(%d+)$") or "")
    if taken then
        core.meta_set(c[1], c[2], c[3], `r{taken}`, "")
    elseif added then
        local item = v.shown[added]
        if item then
            local name = core.item_name(item)
            local free: number? = nil
            for i = 1, RECIPE_SLOTS do
                local was = core.meta_get(c[1], c[2], c[3], `r{i}`)
                if was == name then
                    free = nil
                    break
                end
                if was == "" and not free then free = i end
            end
            if free then
                core.meta_set(c[1], c[2], c[3], `r{free}`, name)
            elseif not table.find(recipes_of_pipe(c), item) then
                core.notice(ev.player, "No room for another recipe: put an extension pipe beside this one")
            end
        end
    elseif ev.id == "prev" then v.page -= 1
    elseif ev.id == "next" then v.page += 1
    elseif ev.id == "find" then v.find, v.page = clean(ev.text), 1
    else
        return
    end
    draw_recipes(ev.player)
end)

-- ---------------------------------------------------------------- the router
-- The pipe router: right click any pipe or machine with it and a page says
-- what that network is -- its pipes and how they are joined, its interfaces
-- and their chips, its broadcasters, valves, firewalls and machines with how
-- many recipes each, and what is on its way in it.
core.page_register("pipesnet", "Pipe network")
local function show_network(player: number, c: Cell)
    local net = net_of(c)
    if not net then
        for d = 1, 6 do
            local n = near(c, d)
            if kind_at(n) then
                net = net_of(n)
                break
            end
        end
    end
    if not net then
        core.page_show(player, "pipesnet", "# Pipe network\nNo pipe here.")
        return
    end
    local n = net :: Net
    local cells, nodes, edges = 0, 0, 0
    local by_kind: { [string]: number } = {}
    for _, k in n.kinds do
        cells += 1
        by_kind[k.name] = (by_kind[k.name] or 0) + 1
    end
    for _, node in n.nodes do
        nodes += 1
        edges += #node.edges
    end
    local chips: { [string]: number } = {}
    for k, node in n.nodes do
        if node.kind == KINDS.interface then
            for _, r in rows_of(node.cell) do
                local name = shown_name(r.chip)
                chips[name] = (chips[name] or 0) + 1
            end
        end
    end
    local moving_here, things = 0, 0
    for _, st in moving do
        if n.kinds[key(st.b)] or n.kinds[key(st.a)] then
            moving_here += 1
            things += st.n
        end
    end
    local open, shut = 0, 0
    for _, v in n.valves do
        if valve_open(n.nodes[v].cell) then open += 1 else shut += 1 end
    end
    local can = abilities(n)
    local page = { "# Pipe network", `> {cells} pipes, {nodes} nodes, {edges} ways between them` }
    local kinds = {}
    for name, count in by_kind do table.insert(kinds, `{shown_name(core.item_id("pipes:" .. name))}: {count}`) end
    table.sort(kinds)
    table.insert(page, table.concat(kinds, ", "))
    local cl = {}
    for name, count in chips do table.insert(cl, `{name}: {count}`) end
    table.sort(cl)
    table.insert(page, if #cl > 0 then "Chips: " .. table.concat(cl, ", ") else "No chips")
    table.insert(page, `Valves: {open} open, {shut} shut. Firewalls: {#n.firewalls}`)
    for _, spot in n.machines do
        local count = 0
        for _, list in can[spot.kind] do
            if table.find(list, spot) then count += 1 end
        end
        local m = machines[key(spot.cell)]
        table.insert(page, `{if spot.kind == "smelt" then "Automatic furnace" else "Automatic crafting table"} at `
            .. `{spot.cell[1]} {spot.cell[2]} {spot.cell[3]}: {count} recipes, `
            .. `{if m then #m.queue + (if m.current then 1 else 0) else 0} steps to do`)
    end
    table.insert(page, `On its way: {things} things in {moving_here} stacks`)
    core.page_show(player, "pipesnet", table.concat(page, "\n"))
end
core.event.on("core:page_open", function(ev)
    if ev.page == "pipesnet" then
        core.page_show(ev.player, "pipesnet", "# Pipe network\nRight click a pipe with the pipe router.")
    end
end)

-- ---------------------------------------------------------------- work far from everybody
-- Where something is on its way or a machine has work, the world stays in
-- memory (core.keep_loaded) until it is done, with nobody near: a request
-- made before walking off arrives, a chain of recipes is finished.
local KEEP_EVERY = 20
local keep_clock = KEEP_EVERY
core.event.on("core:tick", function(ev)
    keep_clock += ev.dt
    if keep_clock < KEEP_EVERY then return end
    keep_clock = 0
    local done: { [string]: boolean } = {}
    local function keep(c: Cell)
        local k = `{c[1] // 16},{c[3] // 16}`
        if done[k] then return end
        done[k] = true
        core.keep_loaded(c[1], c[3], 8)
    end
    for _, st in moving do keep(st.b) end
    for _, m in machines do
        if m.current or #m.queue > 0 then keep(m.cell) end
    end
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
    if MACHINE[ev.id] then core.container_make(ev.x, ev.y, ev.z, MACHINE_SLOTS) end
    -- A pipe, or a chest put beside an interface: the shapes round it change,
    -- and the networks it touches are worked out again when next asked.
    touched(c)
    around(c)
end)
core.event.on("core:broken", function(ev)
    touched({ ev.x, ev.y, ev.z })
    around({ ev.x, ev.y, ev.z })
end)
core.event.on("core:pre_break", function(ev)
    local k = KIND_OF[ev.id]
    if k == KINDS.interface or k == KINDS.firewall or MACHINE[ev.id] then
        core.container_spill(ev.x, ev.y, ev.z)
        core.container_clear(ev.x, ev.y, ev.z)
    end
end)

core.event.on("core:used", function(ev)
    if ev.held == "pipes:router" and (KIND_OF[ev.id] or MACHINE[ev.id]) then
        show_network(ev.player, { ev.x, ev.y, ev.z })
        ev.cancel = true
        return
    end
    if MACHINE[ev.id] then
        -- A machine shows what has been brought to it and not yet used.
        core.container_show(ev.x, ev.y, ev.z, MACHINE_SLOTS, ev.player)
        ev.cancel = true
        return
    end
    local k = KIND_OF[ev.id]
    if not k or is_pipe_item(ev.held) then return end
    local c = { ev.x, ev.y, ev.z }
    if ev.held == "screwdriver:screwdriver" or ev.held == "pipes:wrench" then
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
        open_request(ev.player, c)
        ev.cancel = true
    elseif k == KINDS.crafting or k == KINDS.extension then
        open_recipes(ev.player, c)
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

-- The machines: level two, and priced like it.
core.recipe_shaped("pipes:autocrafter", 1,
    "ore:diamond           pipes:stock_keeper_chip  ore:diamond",
    "pipes:interface_pipe  craft:table              pipes:interface_pipe",
    "furnace:iron_ingot    furnace:gold_ingot       furnace:iron_ingot")
core.recipe_shaped("pipes:autofurnace", 1,
    "ore:diamond           pipes:extractor_chip     ore:diamond",
    "pipes:interface_pipe  furnace:furnace          pipes:interface_pipe",
    "furnace:iron_ingot    ore:cinnabar             furnace:iron_ingot")
core.recipe_shaped("pipes:latency_pipe", 8,
    "pipes:item_pipe  pipes:item_pipe  pipes:item_pipe",
    "pipes:item_pipe  core:sand        pipes:item_pipe",
    "pipes:item_pipe  pipes:item_pipe  pipes:item_pipe")
core.recipe_shaped("pipes:valve_pipe", 1,
    "-                   ore:cinnabar     -",
    "furnace:iron_ingot  pipes:item_pipe  furnace:iron_ingot")
core.recipe_shaped("pipes:crafting_pipe", 1,
    "ore:cinnabar        craft:table          ore:cinnabar",
    "furnace:gold_ingot  pipes:junction_pipe  furnace:gold_ingot",
    "ore:cinnabar        furnace:gold_ingot   ore:cinnabar")
core.recipe_shaped("pipes:extension_pipe", 1,
    "cane:paper  pipes:junction_pipe  cane:paper")
core.recipe_shaped("pipes:router", 1,
    "ore:cinnabar        core:glass        ore:cinnabar",
    "furnace:gold_ingot  pipes:blank_chip  furnace:gold_ingot",
    "-                   furnace:iron_ingot  -")
core.recipe_shaped("pipes:wrench", 1,
    "furnace:iron_ingot  -                   furnace:iron_ingot",
    "-                   furnace:iron_ingot  -",
    "-                   furnace:iron_ingot  -")
core.recipe_shaped("pipes:blank_chip", 1,
    "furnace:gold_ingot  cane:paper  ore:cinnabar")
for _, c in { { "responder", "orange", "furnace:iron_ingot" }, { "dynamic_responder", "cyan", "colors:orange_dye" },
              { "overflow", "green", "furnace:iron_ingot" }, { "terminator", "purple", "colors:gray_dye" },
              { "extractor", "cyan", "furnace:iron_ingot" }, { "broadcaster", "magenta", "furnace:gold_ingot" },
              { "recipe", "yellow", "craft:table" },
              { "stock_keeper", "blue", "ore:diamond" } } do
    core.recipe_shaped(`pipes:{c[1]}_chip`, 1,
        `colors:{c[2]}_dye  ore:cinnabar     colors:{c[2]}_dye`,
        `{c[3]}             pipes:blank_chip  {c[3]}`,
        `colors:{c[2]}_dye  ore:cinnabar     colors:{c[2]}_dye`)
end

-- Another file's way to have a cell's shape worked out again (demo.luau, or a
-- mod that wrote pipes with core.set_block).
core.event.define("pipes:refresh", { fields = { x = "number", y = "number", z = "number" }, cancellable = false })
core.event.on("pipes:refresh", function(ev)
    touched({ ev.x, ev.y, ev.z })
    refresh({ ev.x, ev.y, ev.z })
end)

-- Any write to a cell of a network, or beside one: a mod's set_block, a
-- piston, a door, a player. A pipe that only changed shape is the same pipe;
-- a pipe gone, come or turned into another kind is a new graph.
core.event.on("core:watched", function(ev)
    local c = { ev.x, ev.y, ev.z }
    local k = key(c)
    local now = kind_at(c)
    local n = net_at[k]
    if n and n.kinds[k] ~= now then drop_net(n) end
    local b = beside[k]
    if b then
        -- A pipe come beside a network, or a machine come or gone (one only
        -- going to work or stopping is the same machine).
        local mk = machine_at(c)
        local all = {}
        for net in b do
            if now or net.machine_at[k] ~= mk then table.insert(all, net) end
        end
        for _, net in all do drop_net(net) end
    end
end)

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

-- A chunk read back: the networks through it are worked out again when next
-- asked, at once rather than when their graph is STALE_AFTER old. An LBM
-- names at most 32 blocks: the pipes' 320 shapes in ten.
do
    local all: { string } = {}
    for _, k in KINDS do
        for _, n in NAMES[k] do table.insert(all, n) end
    end
    table.sort(all)
    for i = 1, #all, 32 do
        core.register_lbm(`pipes:loaded_{(i - 1) // 32 + 1}`, table.move(all, i, math.min(i + 31, #all), 1, {}),
            function(cell) touched({ cell.x, cell.y, cell.z }) end)
    end
end
-- And the machines, found again after a restart: one that was at work stands
-- (its job is gone), and what it had been brought goes back to the store.
core.register_lbm("pipes:machines", { "pipes:autocrafter", "pipes:autocrafter_on", "pipes:autofurnace",
                                      "pipes:autofurnace_on" }, function(cell)
    local c = { cell.x, cell.y, cell.z }
    local kind = machine_at(c)
    if not kind or machines[key(c)] then return end
    for d = 1, 6 do
        local n = near(c, d)
        if kind_at(n) then
            local m = machine({ cell = c, kind = kind :: string, pipe = n })
            show_working(m, false)
            return
        end
    end
end)

-- Two working networks to look at, on the plaza.
if core.plaza() ~= nil then require("./demo") end

core.log("pipes: item, junction, interface, request, firewall, latency, valve, crafting and extension pipes; "
         .. "eight chips; an automatic crafting table and furnace; the wrench and the router")
