--!strict
-- villagers: the mayor's and the builder's panels. Part of the mod (init.luau says the order).

local common = require("./common")
local types = require("./types")
type Order = types.Order
type Portal = types.Portal
local AID = common.AID
local at = common.at
local bed_mark = common.bed_mark
local bill_of = common.bill_of
local deliver = common.deliver
local find_lot = common.find_lot
local FLOOR_PRICE = common.FLOOR_PRICE
local GARDEN_PRICE = common.GARDEN_PRICE
local GUARD_DAY = common.GUARD_DAY
local house_of = common.house_of
local ITEMS = common.ITEMS
local KEEPING = common.KEEPING
local lack_said = common.lack_said
local LOT = common.LOT
local lots = common.lots
local MAX_ORDERS = common.MAX_ORDERS
local orders = common.orders
local pockets_here = common.pockets_here
local rect_of = common.rect_of
local road_of = common.road_of
local save_lots = common.save_lots
local save_orders = common.save_orders
local SIZES = common.SIZES
local STYLES = common.STYLES
local survey = common.survey
local to_treasury = common.to_treasury
local treasury = common.treasury
local trust_of = common.trust_of
local try_spot = common.try_spot
local wanting = common.wanting

-- ------------------------------------------------------------ the mayor

local function panel_of(p: number, who: string): string
    return `villagers:{who}{p}`
end
local selling: { [number]: string } = {}   -- player -> the village they talk to

-- In AID, as the rest: the file is at Luau's 200 locals.
AID.pt = {
    LOOK = {
        pista = "an arch chequered black and white, like a race's start",
        inferno = "a frame of obsidian on scorched ground",
        arena = "a steel door striped yellow and black, a red light over it",
        arpg = "a ruined brick arch among graves",
        chambers = "a wall of white panels with a door in it",
        backrooms = "a doorway in a wall of yellow wallpaper",
        parkour = "an orange and black scaffold with a bar over it",
        laboratorio = "a dead frame of dark steel with seven pedestals before it",
        front = "a bunker door between sandbags and two flags",
        assalto = "a gate of black basalt with violet fire on its towers",
    } :: { [string]: string },
    heard = "",
}
core.event.on("gates:sites", function(ev)
    if ev.tag == "villagers" then AID.pt.heard = ev.list end
end)
-- The portals within r of (x, z), nearest first, none closer than 24, none
-- player p has done.
function AID.pt.near(x: number, z: number, r: number, p: number?): { Portal }
    AID.pt.heard = ""
    core.event.emit("gates:ask", { x = x, z = z, r = r, tag = "villagers", player = p or 0 })
    local out: { Portal } = {}
    for room, d, sx, sz in string.gmatch(AID.pt.heard, "([^;%s]+) (%d+) (%-?%d+) (%-?%d+)") do
        local px, pz = tonumber(sx) or 0, tonumber(sz) or 0
        if (px - x) ^ 2 + (pz - z) ^ 2 >= 24 * 24 then
            table.insert(out, { room = room, d = tonumber(d) or 1, x = px, z = pz })
        end
    end
    table.sort(out, function(a, b) return (a.x - x) ^ 2 + (a.z - z) ^ 2 < (b.x - x) ^ 2 + (b.z - z) ^ 2 end)
    return out
end
function AID.pt.bearing(dx: number, dz: number): string
    local dirs = { "north", "north-east", "east", "south-east", "south", "south-west", "west", "north-west" }
    return dirs[math.floor((math.atan2(dx, -dz) / (2 * math.pi) * 8 + 0.5)) % 8 + 1]
end
-- Vague: what it looks like, which way from (x, z), near or far, and how it went.
function AID.pt.rumour(s: Portal, x: number, z: number, from: string): string
    local far = math.sqrt((s.x - x) ^ 2 + (s.z - z) ^ 2)
    local how = if far < 200 then "not far" elseif far < 500 then "a good walk" else "far"
    local fear = if s.d >= 7 then " Nobody who went in has come back the same."
        elseif s.d >= 4 then " Those who went in came back hurt, when they came back."
        else " Children dare each other to go near it."
    return `They say there is {AID.pt.LOOK[s.room] or "a strange door"} {how} to the {AID.pt.bearing(s.x - x, s.z - z)} of {from}.{fear}`
end
-- Exact, and on the map.
function AID.pt.tell(p: number)
    local px, _, pz = core.player_pos(p)
    if px == nil or pz == nil then return end
    local s = AID.pt.near(px, pz, 1200, p)[1]
    if s == nil then
        core.notice(p, "\"No portal I know of, this side of the mountains.\"")
        return
    end
    core.event.emit("maps:route", { player = p, label = "portal",
                                    points = `{math.floor(px)},{math.floor(pz)};{s.x},{s.z}` })
    core.notice(p, `"{AID.pt.LOOK[s.room] or "A strange door"}, {math.floor(math.sqrt((s.x - px) ^ 2 + (s.z - pz) ^ 2))} `
        .. `nodes to the {AID.pt.bearing(s.x - px, s.z - pz)}, at {s.x} {s.z}. I have marked it on your map."`)
end
-- What the nearest portal's exact place costs from here: 2 + its d, or nil.
function AID.pt.price(p: number): number?
    local px, _, pz = core.player_pos(p)
    if px == nil or pz == nil then return nil end
    local s = AID.pt.near(px, pz, 1200, p)[1]
    return if s then 2 + s.d else nil
end

local function show_mayor(p: number, rk: string)
    local name = panel_of(p, "mayor")
    local price = AID.pt.price(p)
    local pocket = pockets_here()
    core.ui_define(name, 13, if pocket then 13.2 else 11.8)
    local waiting = #(orders[rk] or {})
    core.ui_label(name, 0.4, 0.3, "The mayor: land in this village, for rubies.")
    core.ui_label(name, 0.4, 1.0, (if waiting > 0 then `The builder has {waiting} order(s) before yours.`
        else "The builder is free.") .. ` The treasury holds {treasury(rk)} rubies.`)
    core.ui_button(name, "lot", 0.4, 1.9, 12.2, 1.2, `A lot of 7 by 7, levelled, a way to it: {LOT} rubies`)
    core.ui_button(name, "house", 0.4, 3.3, 12.2, 1.2, "A house on a lot of its own...")
    core.ui_button(name, "extend", 0.4, 4.7, 12.2, 1.2, "More for the house you have here...")
    core.ui_button(name, "guard", 0.4, 6.1, 12.2, 1.2, `A guard for your territory, a day: {GUARD_DAY} rubies`)
    core.ui_button(name, "errands", 0.4, 7.5, 12.2, 1.2, "Errands for the village...")
    local y = 8.9
    if pocket then
        core.ui_button(name, "pocket", 0.4, y, 12.2, 1.2, `A pocket hut, 3 by 3 and a hall inside: {AID.pocket_price} rubies`)
        y += 1.4
    end
    if price then
        core.ui_button(name, "portal", 0.4, y, 12.2, 1.2, `Where exactly is the nearest portal? {price} rubies`)
        y += 1.4
    else
        -- A world with a plaza: the portals stand there, and the mayor says
        -- what the village makes of them.
        core.ui_label(name, 0.4, y, "\"The arches on the plaza were there before the first house.")
        core.ui_label(name, 0.4, y + 0.6, "Some who go in come back with a key. We keep away.\"")
        y += 1.4
    end
    core.ui_button(name, "close", 0.4, y, 12.2, 1.1, "Not now")
    selling[p] = rk
    core.ui_show(name, true, p)
end

-- A house's size (rows) and style (columns).
local function show_houses(p: number, rk: string)
    local name = panel_of(p, "mayor")
    core.ui_define(name, 15, 6.6)
    core.ui_label(name, 0.4, 0.3, "Which house? Built of wood, stone bricks or bricks.")
    local y = 1.0
    for _, size in { "small", "medium", "large" } do
        local sz = SIZES[size]
        core.ui_label(name, 0.4, y + 0.35, `{size} {sz[1]}x{sz[2]}, {sz[3]}`)
        for i, style in { "wood", "stone", "brick" } do
            core.ui_button(name, `h {size} {style}`, 5.4 + (i - 1) * 3.1, y, 3.0, 1.1, style)
        end
        y += 1.3
    end
    core.ui_button(name, "close", 0.4, y + 0.3, 14.2, 1.1, "Not now")
    selling[p] = rk
    core.ui_show(name, true, p)
end

local function show_extend(p: number, rk: string)
    local name = panel_of(p, "mayor")
    core.ui_define(name, 13, 5.2)
    core.ui_label(name, 0.4, 0.3, "For the house you have in this village:")
    core.ui_button(name, "x floor", 0.4, 1.0, 12.2, 1.2, `A second storey, a ladder up: {FLOOR_PRICE} rubies`)
    core.ui_button(name, "x garden", 0.4, 2.4, 12.2, 1.2, `A fenced garden of wheat beside it: {GARDEN_PRICE} rubies`)
    core.ui_button(name, "close", 0.4, 3.8, 12.2, 1.1, "Not now")
    selling[p] = rk
    core.ui_show(name, true, p)
end

-- Everything sold goes the same way: paid for, an order queued, its ground
-- the buyer's and the village's.
local function queue(p: number, rk: string, o: Order, price: number, what: string)
    o.road = if o.kind ~= "floor" and o.kind ~= "garden" and o.kind ~= "blueprint" then road_of(o) else {}
    o.need = bill_of(o)
    orders[rk] = orders[rk] or {}
    table.insert(orders[rk], o)
    if o.kind ~= "floor" then
        local x1, z1, x2, z2 = rect_of(o)
        lots[rk] = lots[rk] or {}
        table.insert(lots[rk], { x1, z1, x2, z2 })
        if o.buyer ~= "" then
            core.event.emit("territory:lot", { player = o.buyer, x1 = x1, z1 = z1, x2 = x2, z2 = z2 })
        end
        save_lots(rk)
        survey()
    end
    save_orders(rk)
    local cx, cz = at(o, o.w // 2, o.d // 2)
    if p ~= 0 then
        core.notice(p, `Yours: {what} at {cx} {cz}. The builder needs {lack_said(o.need)}.`)
    end
    core.log(`villagers: {rk}: {if o.buyer == "" then "the village" else o.buyer} ordered {what} at {cx} {o.floor} {cz}, a way of {#o.road} cells; `
        .. `it needs {lack_said(o.need)} ({price} rubies)`)
end

local function pay(p: number, rk: string, price: number): boolean
    local paid = core.take_item(p, "ore:ruby", price)
    if paid < price then
        if paid > 0 then
            core.give_item(p, "ore:ruby", paid)
        end
        core.notice(p, `That is {price} rubies.`)
        return false
    end
    to_treasury(rk, price)
    return true
end

local function may_buy(p: number, rk: string): boolean
    if trust_of(rk, core.player_name(p)).strikes > 0 then
        core.notice(p, "The mayor will not sell to a thief.")
        return false
    end
    if core.player_key(p) == "" then
        core.notice(p, "The mayor sells land only to a player with a key of their own.")
        return false
    end
    if #(orders[rk] or {}) >= MAX_ORDERS then
        core.notice(p, "The builder has too much to do: come back later.")
        return false
    end
    return true
end

-- A lot (7 by 7), or a house of a size and a style.
local function sell(p: number, rk: string, kind: string, size: string?, style: string?)
    if not may_buy(p, rk) then
        return
    end
    local sz = SIZES[size or "medium"]
    local w, d = if kind == "house" then sz[1] else 7, if kind == "house" then sz[2] else 7
    local price = if style == "pocket" then AID.pocket_price elseif kind == "house" then sz[3] else LOT
    local x0, z0, t, floor, g = find_lot(rk, w, d)
    if x0 == nil or z0 == nil or t == nil or floor == nil or g == nil then
        core.notice(p, "There is no room left round this village.")
        return
    end
    if not pay(p, rk, price) then
        return
    end
    local o: Order = { kind = kind, buyer = core.player_name(p), x0 = x0, z0 = z0, t = t, floor = floor,
                       stage = "gather", g = g, road = {}, need = {}, have = { wood = 0, stone = 0, earth = 0 },
                       since = common.clock, asked = 0, w = w, d = d, style = style or "wood", storeys = 1 }
    queue(p, rk, o, price, if style == "pocket" then "a pocket hut" elseif kind == "house" then `a {size} {style} house` else "a lot")
end

-- A keeper with no bed in a building of its own sign -- a village made
-- before villages had them -- has its builder put that building up, the
-- village's order, after those it has: the hall (the mayor's), barracks
-- (the watch's), or a small house with the merchant's scales, the builder's
-- plans or the crafter's anvil. Once each: built or found, it is not asked
-- for again.
local function order_post(rk: string, mark: string)
    local post = KEEPING.posts[mark]
    local key = if mark == "villages:bell" then `hall {rk}` else `post {rk} {mark}`
    local b = common.villages[rk]
    if post == nil or b == nil or core.storage_get(key) ~= "" then
        return
    end
    -- All its ground loaded, every 16 across and up: a building not loaded
    -- yet is not one missing.
    local R = KEEPING.reach
    local x1, z1, x2, z2 = b.x1 - R, b.z1 - R, b.x2 + R, b.z2 + R
    for x = x1, x2 + 15, 16 do
        for z = z1, z2 + 15, 16 do
            for y = b.y - 12, b.y + 16 + 15, 16 do
                if core.loaded(math.min(x, x2), math.min(y, b.y + 16), math.min(z, z2)) ~= 1 then
                    return
                end
            end
        end
    end
    -- Found: a bed under that sign (an old smithy has its anvil and no bed).
    local f = AID.find(x1, b.y - 12, z1, x2, b.y + 16, z2, "bed:bed")
    for i = 1, #f, 3 do
        if bed_mark(f[i], f[i + 1], f[i + 2]) == mark then
            core.storage_set(key, "1")
            return
        end
    end
    local x0, z0, t, floor, g = find_lot(rk, post.w, post.d)
    if x0 == nil or z0 == nil or t == nil or floor == nil or g == nil then
        return
    end
    core.storage_set(key, "1")
    local o: Order = { kind = post.kind, buyer = "", x0 = x0, z0 = z0, t = t, floor = floor,
                       stage = "gather", g = g, road = {}, need = {}, have = { wood = 0, stone = 0, earth = 0 },
                       since = common.clock, asked = 0, w = post.w, d = post.d, style = "wood",
                       storeys = if post.w == 9 then 2 else 1 }
    queue(0, rk, o, 0, post.what)
end

-- A storey on the house they have here, or a garden beside it.
local function extend(p: number, rk: string, what: string)
    if not may_buy(p, rk) then
        return
    end
    local h = house_of(rk, core.player_name(p))
    if h == nil then
        core.notice(p, "You have no house in this village.")
        return
    end
    local o: Order = { kind = what, buyer = h.buyer, x0 = h.x0, z0 = h.z0, t = h.t, floor = h.floor,
                       stage = "gather", g = {}, road = {}, need = {}, have = { wood = 0, stone = 0, earth = 0 },
                       since = common.clock, asked = 0, w = h.w, d = h.d, style = h.style, storeys = h.storeys }
    if what == "floor" then
        if h.storeys >= 2 then
            core.notice(p, "Your house has its second storey already.")
            return
        end
        if not pay(p, rk, FLOOR_PRICE) then
            return
        end
        queue(p, rk, o, FLOOR_PRICE, "a second storey")
        return
    end
    if h.garden then
        core.notice(p, "Your house has its garden already.")
        return
    end
    -- Beside the house, a cell between: its back first, then its sides.
    local x1, z1, x2, z2 = rect_of(o)
    local hx, hz = (x1 + x2) / 2, (z1 + z2) / 2
    local tries = {}
    for k = -4, 4, 2 do
        table.insert(tries, { x1 + k, z1 - 6 })
        table.insert(tries, { x1 + k, z2 + 2 })
        table.insert(tries, { x1 - 6, z1 + k })
        table.insert(tries, { x2 + 2, z1 + k })
    end
    for _, sp in tries do
        local t, floor, g = try_spot(h.floor, sp[1], sp[2], 5, 5, hx, hz)
        if t ~= nil and floor ~= nil and g ~= nil then
            if not pay(p, rk, GARDEN_PRICE) then
                return
            end
            o.x0, o.z0, o.t, o.floor, o.g, o.w, o.d = sp[1], sp[2], t, floor, g, 5, 5
            queue(p, rk, o, GARDEN_PRICE, "a garden")
            return
        end
    end
    core.notice(p, "There is no room for a garden by your house.")
end

-- ------------------------------------------------------------ the builder

local function show_builder(p: number, rk: string)
    local o, lack = wanting(rk)
    local name = panel_of(p, "builder")
    core.ui_define(name, 13, if o == nil then 6.0 else 7.4)
    if o == nil then
        local busy = (orders[rk] or {})[1]
        core.ui_label(name, 0.4, 0.3, if busy then `Building {busy.buyer}'s {busy.kind}.` else "Nothing to build.")
        core.ui_button(name, "blueprint", 0.4, 1.4, 12.2, 1.2, "Build the blueprint I laid out...")
        core.ui_button(name, "close", 0.4, 2.8, 12.2, 1.2, "Close")
    else
        core.ui_label(name, 0.4, 0.3, `For {o.buyer}'s {o.kind} the builder still needs:`)
        core.ui_label(name, 0.4, 1.0, lack_said(lack))
        core.ui_button(name, "give", 0.4, 1.8, 12.2, 1.2, "Hand over what I have of it")
        if o.self then
            core.ui_label(name, 0.4, 3.35, "The village's workers are fetching it.")
        else
            core.ui_button(name, "self", 0.4, 3.2, 12.2, 1.2, "Find it yourself: the workers fetch it")
        end
        core.ui_button(name, "blueprint", 0.4, 4.6, 12.2, 1.2, "Build the blueprint I laid out...")
        core.ui_button(name, "close", 0.4, 6.0, 12.2, 1.1, "Close")
    end
    selling[p] = rk
    core.ui_show(name, true, p)
end

local function hand_over(p: number, rk: string)
    local o, lack = wanting(rk)
    if o == nil then
        return
    end
    local given = 0
    for m, n in lack do
        for _, item in ITEMS[m] do
            if n <= 0 then
                break
            end
            local took = core.take_item(p, item, n)
            n -= took
            given += deliver(o, m, took)
        end
    end
    save_orders(rk)
    core.notice(p, if given > 0 then `You handed over {given}.` else "You have none of it.")
end

core.event.on("core:ui_click", function(ev)
    local rk = selling[ev.player]
    if rk == nil or (ev.panel ~= panel_of(ev.player, "mayor") and ev.panel ~= panel_of(ev.player, "builder")) then
        return
    end
    core.ui_show(ev.panel, false, ev.player)
    selling[ev.player] = nil
    if ev.id == "guard" then
        local paid = core.take_item(ev.player, "ore:ruby", GUARD_DAY)
        if paid < GUARD_DAY then
            if paid > 0 then
                core.give_item(ev.player, "ore:ruby", paid)
            end
            core.notice(ev.player, `That is {GUARD_DAY} rubies.`)
            return
        end
        if core.event.emit("territory:hire", { player = ev.player, days = 1 }) then
            core.give_item(ev.player, "ore:ruby", GUARD_DAY)
        else
            to_treasury(rk, GUARD_DAY)
        end
    elseif ev.id == "errands" then
        AID.err.show(ev.player, rk, "mayor")
    elseif ev.id == "portal" then
        local price = AID.pt.price(ev.player)
        if price and pay(ev.player, rk, price) then AID.pt.tell(ev.player) end
    elseif ev.id == "lot" then
        sell(ev.player, rk, "lot")
    elseif ev.id == "house" then
        show_houses(ev.player, rk)
    elseif ev.id == "pocket" then
        if pockets_here() then sell(ev.player, rk, "house", "small", "pocket") end
    elseif ev.id == "extend" then
        show_extend(ev.player, rk)
    elseif ev.id:sub(1, 2) == "h " then
        local size, style = string.match(ev.id, "^h (%a+) (%a+)$")
        if size and style and SIZES[size] and STYLES[style] then
            sell(ev.player, rk, "house", size, style)
        end
    elseif ev.id:sub(1, 2) == "x " then
        extend(ev.player, rk, ev.id:sub(3))
    elseif ev.id == "give" then
        hand_over(ev.player, rk)
    elseif ev.id == "blueprint" then
        AID.bp.order(ev.player, rk)
    elseif ev.id == "self" then
        local o, lack = wanting(rk)
        if o ~= nil then
            o.self = true
            o.since = common.clock
            save_orders(rk)
            core.notice(ev.player, `The village's workers will fetch {lack_said(lack)} for {if o.buyer == "" then "the village" else o.buyer}'s {o.kind}.`)
            core.log(`villagers: {rk}: the builder finds {lack_said(lack)} itself`)
        end
    end
end)

-- What the files after this one use of it.
common.may_buy = may_buy
common.order_post = order_post
common.panel_of = panel_of
common.pay = pay
common.queue = queue
common.show_builder = show_builder
common.show_mayor = show_mayor
