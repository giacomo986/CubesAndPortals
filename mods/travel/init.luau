--!strict
-- Travel: a WAYSTONE stands by the well of every village laid out since, and
-- one is made of smooth stone round an emerald and a cinnabar. It only works
-- on owned ground -- a village's, or a player's territory (mods/territory):
-- put down anywhere else it comes back to the bag. Right click one: it is
-- remembered for that player (attuned), and they are shown the others they
-- remember, nearest first, each with its price -- an emerald every 200 nodes,
-- one at least. Pick one: paid, they stand beside it. Broken, it is forgotten
-- by everybody.
--
-- Kept in core.storage: "stones", a line per waystone (x,y,z|name), and per
-- player name "att:<name>", the keys of the stones they touched.

local STONE = core.register_node("travel:waystone", 90, 200, 190, true, 7, "", 0, 2.0, "")
core.node_box(STONE, 0.1, 0, 0.1, 0.9, 0.2, 0.9)
core.node_box(STONE, 0.25, 0.2, 0.25, 0.75, 0.85, 0.75)
core.node_box(STONE, 0.18, 0.85, 0.18, 0.82, 1.0, 0.82)
core.recipe_shaped("travel:waystone", 1,
    "rocks:smooth_stone  ore:emerald   rocks:smooth_stone",
    "rocks:smooth_stone  ore:cinnabar  rocks:smooth_stone",
    "rocks:smooth_stone  rocks:smooth_stone  rocks:smooth_stone")

local PER = 200     -- nodes an emerald
local SHOWN = 8     -- destinations on the panel at most
local VILLAGE = 65536          -- mods/territory: a village's ground from here,
local TERRITORY = 32768        -- a player's from here

type Stone = { x: number, y: number, z: number, name: string }
local stones: { [string]: Stone } = {}

local function key(x: number, y: number, z: number): string
    return `{x},{y},{z}`
end

local function save()
    local lines = {}
    for k, s in stones do
        table.insert(lines, `{k}|{s.name}`)
    end
    table.sort(lines)
    core.storage_set("stones", table.concat(lines, "\n"))
end

for line in string.gmatch(core.storage_get("stones") or "", "[^\n]+") do
    local x, y, z, name = string.match(line, "^(-?%d+),(-?%d+),(-?%d+)|(.*)$")
    if x then
        local s = { x = tonumber(x) :: number, y = tonumber(y) :: number, z = tonumber(z) :: number, name = name :: string }
        stones[key(s.x, s.y, s.z)] = s
    end
end

local function attuned(p: number): { [string]: boolean }
    local out = {}
    for k in string.gmatch(core.storage_get("att:" .. core.player_name(p)) or "", "[^;]+") do
        out[k] = true
    end
    return out
end

local function attune(p: number, k: string)
    local a = attuned(p)
    if a[k] then
        return false
    end
    a[k] = true
    local list = {}
    for s in a do
        if stones[s] then   -- the broken ones left out
            table.insert(list, s)
        end
    end
    core.storage_set("att:" .. core.player_name(p), table.concat(list, ";"))
    return true
end

-- Every village's ground as mods/villages tells it, named by its middle:
-- known before mods/villagers has made it a territory.
local villages: { { number } } = {}
core.event.on("villages:extent", function(ev)
    for _, v in villages do
        if v[1] == ev.x1 and v[2] == ev.z1 then
            v[3], v[4] = ev.x2, ev.z2
            return
        end
    end
    table.insert(villages, { ev.x1, ev.z1, ev.x2, ev.z2 })
end)

-- What owned ground a cell is on, as a waystone's name: nil for nobody's.
local function ground_name(x: number, z: number, who: string): string?
    for _, v in villages do
        if x >= v[1] and x <= v[3] and z >= v[2] and z <= v[4] then
            return `the village at {(v[1] + v[3]) // 2}, {(v[2] + v[4]) // 2}`
        end
    end
    local tag = core.owned_at(x, z)
    if tag >= VILLAGE then
        return `the village at {x}, {z}`
    elseif tag >= TERRITORY then
        return `{who}'s ground at {x}, {z}`
    end
    return nil
end

local function price(a: Stone, b: Stone): number
    return math.max(1, math.ceil(math.sqrt((a.x - b.x) ^ 2 + (a.z - b.z) ^ 2) / PER))
end

-- Somewhere beside a stone to stand: a side with two cells of room over ground.
local function beside(s: Stone): (number, number, number)
    for _, d in { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } } do
        local x, z = s.x + d[1], s.z + d[2]
        for dy = 1, -1, -1 do
            local y = s.y + dy
            if core.get_block(x, y, z) == 0 and core.get_block(x, y + 1, z) == 0
                and core.node_solid(core.get_block(x, y - 1, z)) == 1 then
                return x + 0.5, y, z + 0.5
            end
        end
    end
    return s.x + 0.5, s.y + 1, s.z + 0.5   -- on top of it, then
end

-- Per player, the panel's rows: the stone each button goes to.
local offered: { [number]: { from: Stone, to: { Stone } } } = {}

local function panel_of(p: number): string
    return `travel:to{p}`
end

local function show(p: number, from: Stone)
    local a = attuned(p)
    local list = {}
    for k in a do
        local s = stones[k]
        if s and s ~= from then
            table.insert(list, s)
        end
    end
    table.sort(list, function(u, v)
        return price(from, u) < price(from, v) or (price(from, u) == price(from, v) and u.name < v.name)
    end)
    while #list > SHOWN do
        table.remove(list)
    end
    offered[p] = { from = from, to = list }
    local name = panel_of(p)
    core.ui_define(name, 11.0, 2.9 + #list * 1.1)
    core.ui_label(name, 0.4, 0.3, from.name)
    core.ui_label(name, 0.4, 0.9, if #list == 0 then "Touch another waystone to travel there from here."
                                  else "Travel to:")
    for i, s in list do
        local n = price(from, s)
        core.ui_button(name, tostring(i), 0.4, 1.6 + (i - 1) * 1.1, 10.2, 0.9,
            `{s.name} -- {n} emerald{if n == 1 then "" else "s"}`)
    end
    core.ui_button(name, "close", 0.4, 1.7 + #list * 1.1, 10.2, 0.9, "close")
    core.ui_show(name, true, p)
end

-- Laid on owned ground, or given back.
local function lay(x: number, y: number, z: number, who: string): boolean
    local name = ground_name(x, z, who)
    if name == nil then
        return false
    end
    stones[key(x, y, z)] = { x = x, y = y, z = z, name = name }
    save()
    return true
end

core.event.on("core:placed", function(ev)
    if ev.id ~= STONE then
        return
    end
    if not lay(ev.x, ev.y, ev.z, core.player_name(ev.player)) then
        core.set_block(ev.x, ev.y, ev.z, 0)
        core.give_item(ev.player, "travel:waystone", 1)
        core.notice(ev.player, "A waystone works on a village's ground or a territory")
        return
    end
    attune(ev.player, key(ev.x, ev.y, ev.z))
    core.notice(ev.player, "Waystone laid")
end)

core.event.on("core:broken", function(ev)
    local k = key(ev.x, ev.y, ev.z)
    if stones[k] then
        stones[k] = nil
        save()
    end
end)

core.event.on("core:used", function(ev)
    if ev.id ~= STONE then
        return
    end
    ev.cancel = true
    local k = key(ev.x, ev.y, ev.z)
    local s = stones[k]
    if s == nil then   -- one a village laid out: known from its first touch
        if not lay(ev.x, ev.y, ev.z, core.player_name(ev.player)) then
            core.notice(ev.player, "This waystone is on nobody's ground")
            return
        end
        s = stones[k]
    end
    if attune(ev.player, k) then
        core.notice(ev.player, `Waystone remembered: {s.name}`)
    end
    show(ev.player, s)
end)

core.event.on("core:ui_click", function(ev)
    local p = ev.player
    if ev.panel ~= panel_of(p) then
        return
    end
    local o = offered[p]
    if ev.id == "close" or o == nil then
        core.ui_show(ev.panel, false, p)
        return
    end
    local to = o.to[tonumber(ev.id) or 0]
    if to == nil or stones[key(to.x, to.y, to.z)] == nil then
        core.ui_show(ev.panel, false, p)
        return
    end
    local n = price(o.from, to)
    local paid = core.take_item(p, "ore:emerald", n)
    if paid < n then
        if paid > 0 then
            core.give_item(p, "ore:emerald", paid)
        end
        core.notice(p, `That is {n} emerald{if n == 1 then "" else "s"}`)
        return
    end
    core.ui_show(ev.panel, false, p)
    local fx, fy, fz = core.player_pos(p)
    core.play_sound("travel:whoosh", fx, fy, fz)
    local x, y, z = beside(to)
    core.player_place(p, x, y, z)
    core.play_sound("travel:whoosh", x, y, z)
    core.notice(p, to.name)
end)
