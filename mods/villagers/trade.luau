--!strict
-- villagers: the merchant, the village's goods and blueprints. Part of the mod (init.luau says the order).

local common = require("./common")
local types = require("./types")
type V = types.V
type Box = types.Box
type Order = types.Order
local AID = common.AID
local cost_of = common.cost_of
local id = common.id
local lack_said = common.lack_said
local may_buy = common.may_buy
local panel_of = common.panel_of
local pay = common.pay
local people = common.people
local queue = common.queue
local region_of = common.region_of
local to_treasury = common.to_treasury
local treasury = common.treasury

-- ------------------------------------------------------------ the merchant

-- The MERCHANT (right click) sells what the village has in its chests -- the
-- mine's and its houses', not a player's -- and buys what a player holds, for
-- rubies. A bundle's price is its base where the village has four bundles
-- of it, up to twice that when it has less, down to half when it has more;
-- it buys at half its price (two bundles for one ruby where that is one).
-- What it buys goes into the village's chests, paid out of its treasury.
local GOODS = {   -- item, how many a bundle, base price in rubies
    { "farm:bread", 4, 1 }, { "pets:cooked_fish", 4, 1 }, { "animals:steak", 4, 1 },
    { "farm:wheat", 12, 1 }, { "animals:wool", 8, 1 }, { "animals:leather", 4, 1 },
    { "trees:trunk", 8, 1 }, { "timber:planks", 32, 1 }, { "rocks:cobblestone", 32, 1 },
    { "core:stone", 24, 1 }, { "core:dirt", 32, 1 },
    { "ore:coal", 8, 1 }, { "ore:raw_copper", 8, 1 }, { "ore:raw_iron", 3, 1 },
    { "ore:lapis", 8, 1 }, { "ore:cinnabar", 8, 1 }, { "ore:raw_gold", 1, 1 }, { "ore:diamond", 1, 5 },
}

-- Every worker trades too (right click), as in the classic block game: it sells from the
-- village's chests what its trade brings in, and buys what its trade needs,
-- at the full price, out of the treasury.
local TRADES = {   -- job -> what it sells, what it buys (item, a bundle, base price)
    farmer = { { { "farm:wheat", 12, 1 }, { "farm:bread", 4, 1 } }, { { "animals:seeds", 16, 1 }, { "farm:hoe", 1, 2 },
                                                                       { "furnace:iron_hoe", 1, 5 }, { "furnace:diamond_hoe", 1, 15 } } },
    -- And buys every fish a player brings: the far waters' dearer, a goldfish
    -- dearest of all.
    fisher = { { { "pets:cooked_fish", 4, 1 }, { "pets:fish", 6, 1 } },
               { { "farm:bread", 4, 1 }, { "pets:carp", 6, 1 }, { "pets:trout", 4, 1 },
                 { "pets:cod", 4, 1 }, { "pets:eel", 3, 1 }, { "pets:tuna", 2, 1 }, { "pets:goldfish", 1, 4 } } },
    forager = { { { "plants:melon_slice", 8, 1 }, { "plants:brown_mushroom", 4, 1 }, { "plants:red_mushroom", 4, 1 },
                  { "animals:seeds", 16, 1 } }, { { "farm:bread", 4, 1 } } },
    woodcutter = { { { "trees:trunk", 8, 1 }, { "timber:planks", 32, 1 } }, { { "trees:sapling", 4, 1 }, { "tools:axe", 1, 3 },
                                                                            { "furnace:iron_axe", 1, 5 }, { "furnace:diamond_axe", 1, 15 } } },
    miner = { { { "rocks:cobblestone", 32, 1 }, { "ore:coal", 8, 1 }, { "ore:raw_copper", 8, 1 }, { "ore:raw_iron", 3, 1 },
                { "ore:lapis", 8, 1 }, { "ore:cinnabar", 8, 1 }, { "ore:raw_gold", 1, 1 }, { "ore:diamond", 1, 5 } },
              { { "torches:torch", 16, 1 }, { "tools:pick", 1, 3 },
                { "furnace:iron_pick", 1, 5 }, { "furnace:diamond_pick", 1, 15 } } },
    digger = { { { "core:dirt", 32, 1 } }, { { "tools:shovel", 1, 2 }, { "farm:bread", 4, 1 },
                                           { "furnace:iron_shovel", 1, 5 }, { "furnace:diamond_shovel", 1, 15 } } },
}

-- The village's chests: every chest in its ground that is not on a player's.
-- (Or in `box`, for a village nobody lives in yet: a caravan's.)
local function village_chests(rk: string, box: Box?): { { number } }
    local b = box or common.villages[rk]
    local out = {}
    if b == nil then
        return out
    end
    local f = AID.find(b.x1, b.y - 20, b.z1, b.x2, b.y + 10, b.z2, "chest:box")
    for i = 1, #f, 3 do
        local owner = core.owned_at(f[i], f[i + 2])
        if not (owner >= 32768 and owner < 65536) then
            table.insert(out, { f[i], f[i + 1], f[i + 2] })
        end
    end
    return out
end

local function stock_of(chests: { { number } }, item: string): number
    local iid, n = core.item_id(item), 0
    if iid == 0 then
        return 0
    end
    for _, c in chests do
        n += core.container_count(c[1], c[2], c[3], iid)
    end
    return n
end

-- The season (mods/weather, through farm:season) doubles what it makes
-- scarce: food in winter, when the fields stop, and wood in autumn, before it.
local DEAR = {
    winter = { ["farm:bread"] = true, ["farm:wheat"] = true, ["pets:cooked_fish"] = true, ["pets:fish"] = true,
               ["animals:steak"] = true, ["plants:melon_slice"] = true },
    autumn = { ["trees:trunk"] = true, ["timber:planks"] = true },
}
common.season = "spring"
core.event.on("farm:season", function(ev)
    common.season = ev.name
end)

local function price_of(good: { any }, stock: number): number
    local f = math.clamp(good[2] * 4 / math.max(stock, 1), 0.5, 2)
    if DEAR[common.season] and DEAR[common.season][good[1]] then
        f *= 2
    end
    return math.max(1, math.floor(good[3] * f + 0.5))
end

local function said(item: string): string
    return (string.gsub(string.gsub(item, "^.*:", ""), "_", " "))
end
local function ems(n: number): string
    return `{n} {if n == 1 then "ruby" else "rubies"}`
end

-- A row: the good, how many, the price, and true when the player sells it.
local trading: { [number]: { rk: string, who: string, rows: { { any } } } } = {}

-- `who` is "merchant" or a job of TRADES.
local function show_trade(p: number, rk: string, who: string)
    local sells, buys = GOODS, {}
    if who ~= "merchant" then
        sells, buys = TRADES[who][1], TRADES[who][2]
    end
    local chests = village_chests(rk)
    local rows = {}
    for _, g in sells do
        local st = stock_of(chests, g[1])
        if st >= g[2] then
            table.insert(rows, { g, g[2], AID.err.discount(p, rk, AID.ev.price(rk, g[1], price_of(g, st))) })
        end
        if #rows >= 9 then
            break
        end
    end
    local selling_rows = #rows
    if who == "merchant" then
        local held = core.held_item(p)
        for _, g in GOODS do
            if g[1] == held then
                local price = price_of(g, stock_of(chests, g[1]))
                table.insert(rows, { g, if price >= 2 then g[2] else g[2] * 2, math.max(1, price // 2), true })
            end
        end
    else
        for _, g in buys do
            if core.item_id(g[1]) ~= 0 then
                table.insert(rows, { g, g[2], price_of(g, stock_of(chests, g[1])), true })
            end
        end
    end
    local name = panel_of(p, "merchant")
    local title = if who == "merchant" then "The merchant" else `The {who}`
    local errands = who == "merchant"
    core.ui_define(name, 14, 4.6 + #rows * 1.1 + (if errands then 1.2 else 0))
    core.ui_label(name, 0.4, 0.3, if selling_rows > 0 then `{title} sells what the village has:`
        else `{title} has nothing to sell today.`)
    for i, r in rows do
        local y = 0.9 + (i - 1) * 1.1
        if r[4] then
            core.ui_button(name, `sell {i}`, 0.4, y, 13.2, 1.0,
                `Sell {r[2]} {said(r[1][1])} for {ems(r[3])}`)
        else
            core.ui_button(name, `buy {i}`, 0.4, y, 13.2, 1.0, `{r[2]} {said(r[1][1])} for {ems(r[3])}`)
        end
    end
    local y = 1.3 + #rows * 1.1
    core.ui_label(name, 0.4, y, if who == "merchant" and #rows == selling_rows
        then "Hold what you want to sell to be offered a price."
        else `The village's treasury: {ems(treasury(rk))}.`)
    if errands then
        core.ui_button(name, "errands", 0.4, y + 0.9, 13.2, 1.1, "Errands...")
        y += 1.2
    end
    core.ui_button(name, "close", 0.4, y + 0.9, 13.2, 1.1, "Close")
    trading[p] = { rk = rk, who = who, rows = rows }
    core.ui_show(name, true, p)
end

local function buy(p: number, t: { rk: string, who: string, rows: { { any } } }, i: number)
    local r = t.rows[i]
    if r == nil or r[4] then
        return
    end
    local good, price = r[1], r[3]
    local chests = village_chests(t.rk)
    if stock_of(chests, good[1]) < good[2] then
        core.notice(p, "That is gone.")
        return
    end
    local paid = core.take_item(p, "ore:ruby", price)
    if paid < price then
        if paid > 0 then
            core.give_item(p, "ore:ruby", paid)
        end
        core.notice(p, `That is {ems(price)}.`)
        return
    end
    to_treasury(t.rk, price)
    local iid, want = core.item_id(good[1]), good[2]
    for _, c in chests do
        if want <= 0 then
            break
        end
        want -= core.container_take(c[1], c[2], c[3], iid, want)
    end
    local got = good[2] - want
    local fit = core.give_item(p, good[1], got)
    if fit < got then
        local x, y, z = core.player_pos(p)
        core.drop_item(math.floor(x or 0), math.floor(y or 0), math.floor(z or 0), iid, got - fit)
    end
    core.notice(p, `You bought {got} {said(good[1])}.`)
end

local function sell_to(p: number, t: { rk: string, who: string, rows: { { any } } }, i: number)
    local r = t.rows[i]
    if r == nil or not r[4] then
        return
    end
    local good, n, pay = r[1], r[2], r[3]
    if treasury(t.rk) < pay then
        core.notice(p, `The village has {ems(treasury(t.rk))} to pay with.`)
        return
    end
    local took = core.take_item(p, good[1], n)
    if took < n then
        if took > 0 then
            core.give_item(p, good[1], took)
        end
        core.notice(p, `It buys {n} at a time.`)
        return
    end
    local iid, left = core.item_id(good[1]), n
    for _, c in village_chests(t.rk) do
        if left <= 0 then
            break
        end
        left = core.container_add(c[1], c[2], c[3], iid, left)
    end
    if left > 0 then
        -- ponytail: no room in the village's chests, it is not bought; a store
        -- house for the village when that happens in play.
        core.give_item(p, good[1], left)
        if left == n then
            core.notice(p, "The village's chests are full.")
            return
        end
        pay = math.max(1, pay * (n - left) // n)
    end
    to_treasury(t.rk, -pay)
    core.give_item(p, "ore:ruby", pay)
    core.notice(p, `You sold {n - left} {said(good[1])} for {ems(pay)}.`)
end

core.event.on("core:ui_click", function(ev)
    local t = trading[ev.player]
    if t == nil or ev.panel ~= panel_of(ev.player, "merchant") then
        return
    end
    trading[ev.player] = nil
    core.ui_show(ev.panel, false, ev.player)
    local i = tonumber(string.match(ev.id, "^buy (%d+)$") or "")
    local j = tonumber(string.match(ev.id, "^sell (%d+)$") or "")
    if ev.id == "errands" then
        AID.err.show(ev.player, t.rk, "merchant")
    elseif i ~= nil then
        buy(ev.player, t, i)
        show_trade(ev.player, t.rk, t.who)
    elseif j ~= nil then
        sell_to(ev.player, t, j)
        show_trade(ev.player, t.rk, t.who)
    end
end)

-- Somebody to ask for what is missing: the buyer if here, else whoever is near.
local function ask(o: Order, v: V, lack: { [string]: number })
    local said = `The builder needs {lack_said(lack)} for {if o.buyer == "" then "the village" else o.buyer}'s {o.kind}: right click it to hand them `
        .. "over, or to tell it to find them itself."
    for _, p in core.players() do
        local x, _, z = core.player_pos(p)
        local mx, _, mz = core.mob_pos(v.id)
        if core.player_name(p) == o.buyer or (x and z and mx and mz and (x - mx) ^ 2 + (z - mz) ^ 2 < 16 * 16) then
            core.notice(p, said)
        end
    end
end

local function has_kind(rk: string, kind: string): boolean
    for _, v in people do
        if v.kind == kind and region_of(v.hx, v.hz) == rk then
            return true
        end
    end
    return false
end

-- ------------------------------------------- the village's goods, shared

-- Out of the village's chests and into them: the village's people take and
-- leave freely; only a stranger pays (the merchant, the workers' panels).
local function take_from(chests: { { number } }, item: string, n: number): number
    local iid, want = core.item_id(item), n
    if iid == 0 then
        return 0
    end
    for _, c in chests do
        if want <= 0 then
            break
        end
        want -= core.container_take(c[1], c[2], c[3], iid, want)
    end
    return n - want
end
local function put_into(chests: { { number } }, item: string, n: number): number
    local iid, left = core.item_id(item), n
    if iid == 0 then
        return n
    end
    for _, c in chests do
        if left <= 0 then
            break
        end
        left = core.container_add(c[1], c[2], c[3], iid, left)
    end
    return left
end

-- ------------------------------------------------------------ blueprints
--
-- A player's laid-out blueprint (mods/blueprints) built by the builder, as an
-- order of the kind "blueprint" -- its cells asked of mods/blueprints with
-- "blueprints:ask" whenever its plan is needed. Inside the village's ground
-- only. What the gatherers bring (wood, stone, earth: cost_of) the builder
-- needs as for a house; any other block (glass, wool, an ore block) the
-- player hands over, all of it, when the order is made; the labour is paid in
-- rubies, four and one more for each forty blocks.
AID.bp = { got = {} :: { [string]: { any } } }
core.event.on("blueprints:cells", function(ev)
    local cells = {}
    for x, y, z, name, f in string.gmatch(ev.cells, "(%-?%d+),(%-?%d+),(%-?%d+),([^,]*),(%d+)") do
        table.insert(cells, { tonumber(x), tonumber(y), tonumber(z), name, if name == "" then nil else tonumber(f), false })
    end
    AID.bp.got[`{ev.id} {ev.x} {ev.y} {ev.z} {ev.t}`] = { cells = cells, w = ev.w, d = ev.d, id = ev.id,
                                                           x = ev.x, y = ev.y, z = ev.z, t = ev.t, owner = ev.owner }
    AID.bp.last = AID.bp.got[`{ev.id} {ev.x} {ev.y} {ev.z} {ev.t}`]
end)
AID.bp.plan = function(o: Order): { { any } }
    local id = tonumber(string.match(o.style, "^bp:(%d+)$") or "") or 0
    local key = `{id} {o.x0} {o.floor} {o.z0} {o.t}`
    if AID.bp.got[key] == nil then
        core.event.emit("blueprints:ask", { id = id, x = o.x0, y = o.floor, z = o.z0, t = o.t, owner = o.buyer })
    end
    local g = AID.bp.got[key]
    return if g then g.cells else {}
end
AID.bp.order = function(p: number, rk: string)
    local who = core.player_name(p)
    if not may_buy(p, rk) then
        return
    end
    AID.bp.last = nil
    core.event.emit("blueprints:layout", { owner = who })
    local g = AID.bp.last
    if g == nil or g.owner ~= who then
        core.notice(p, "Lay a blueprint out first (use it on the ground where it is to go).")
        return
    end
    local b = common.villages[rk]
    local fx2, fz2 = g.x + g.w - 1, g.z + g.d - 1
    if b == nil or g.x < b.x1 or g.z < b.z1 or fx2 > b.x2 or fz2 > b.z2 then
        core.notice(p, "The builder builds only on the village's ground.")
        return
    end
    -- What the player brings: every block the gatherers do not.
    local bring: { [string]: number } = {}
    local solid = 0
    for _, c in g.cells do
        if c[4] ~= "" then
            solid += 1
            if cost_of(c[4]) == nil and core.get_block(c[1], c[2], c[3]) ~= id(c[4]) then
                bring[c[4]] = (bring[c[4]] or 0) + 1
            end
        end
    end
    local short = {}
    for name, n in bring do
        local have = core.bag_count(p, name)
        if have < n then
            table.insert(short, `{n - have} {said(name)}`)
        end
    end
    if #short > 0 then
        core.notice(p, `First bring what the gatherers cannot: {table.concat(short, ", ")}.`)
        return
    end
    local price = 4 + solid // 40
    if not pay(p, rk, price) then
        return
    end
    for name, n in bring do
        core.take_item(p, name, n)
    end
    local w, d = if g.t % 2 == 0 then g.w else g.d, if g.t % 2 == 0 then g.d else g.w
    local o: Order = { kind = "blueprint", buyer = who, x0 = g.x, z0 = g.z, t = g.t, floor = g.y,
                       stage = "gather", g = {}, road = {}, need = {}, have = { wood = 0, stone = 0, earth = 0 },
                       since = common.clock, asked = 0, w = w, d = d, style = `bp:{g.id}`, storeys = 1 }
    queue(p, rk, o, price, "your blueprint")
end

-- What the files after this one use of it.
common.ask = ask
common.DEAR = DEAR
common.ems = ems
common.GOODS = GOODS
common.has_kind = has_kind
common.price_of = price_of
common.put_into = put_into
common.said = said
common.show_trade = show_trade
common.stock_of = stock_of
common.take_from = take_from
common.TRADES = TRADES
common.village_chests = village_chests
