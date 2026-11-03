--!strict
-- trade: a market stall, for players to sell to one another while they are
-- away. Its owner -- whoever put it down -- right clicks it to see its 27
-- squares in three rows:
--
--   goods  the first row: what it sells, one kind (the first square's);
--   price  the second row: what one lot costs, as a sample -- three rubies
--          there is a price of three rubies;
--   till   the third row: what buyers paid, for the owner to take out.
--
-- How many make a lot is set on its page (a field: 1 to 64). Anybody else's
-- right click shows the offer -- "16 bread for 3 rubies, 5 lots left" -- and
-- a Buy button: the price leaves their bag for the till, a lot of the goods
-- goes into it. Only the owner (or an admin) opens it or breaks it; broken,
-- all three rows fall out.
if core.side() == "client" then
    return
end

local STALL = core.register_node("trade:stall", 170, 130, 80, true, 0, "upright", 0, 2.0, "wood")
core.container_roles(STALL, "raw raw raw raw raw raw raw raw raw fuel fuel fuel fuel fuel fuel fuel fuel fuel "
    .. "product product product product product product product product product")
core.recipe_shaped("trade:stall", 1,
    "timber:planks  timber:planks  timber:planks",
    "timber:planks  chest:box      timber:planks")
local SLOTS = 27

local function owner_of(x: number, y: number, z: number): string
    return core.meta_get(x, y, z, "owner")
end
local function may(p: number, x: number, y: number, z: number): boolean
    return owner_of(x, y, z) == core.player_name(p) or core.has_perm(p, "admin") == 1
end

core.event.on("core:placed", function(ev)
    if ev.id == STALL and ev.player > 0 then
        core.container_make(ev.x, ev.y, ev.z, SLOTS)
        core.meta_set(ev.x, ev.y, ev.z, "owner", core.player_name(ev.player))
        core.meta_set(ev.x, ev.y, ev.z, "lot", "1")
    end
end)

core.event.on("core:pre_break", function(ev)
    if ev.id ~= STALL then
        return
    end
    if ev.player > 0 and not may(ev.player, ev.x, ev.y, ev.z) then
        ev.cancel = true
        core.notice(ev.player, `This stall is {owner_of(ev.x, ev.y, ev.z)}'s`)
        return
    end
    core.container_spill(ev.x, ev.y, ev.z)
    core.container_clear(ev.x, ev.y, ev.z)
end)

-- What it offers, worked out from its rows.
type Offer = { goods: number, lot: number, stock: number, price: number, cost: number }
local function offer(x: number, y: number, z: number): Offer?
    local goods, price = 0, 0
    for i = 1, 9 do
        if goods == 0 then
            goods = core.container_item(x, y, z, i)
        end
        if price == 0 then
            price = core.container_item(x, y, z, i + 9)
        end
    end
    if goods == 0 or price == 0 then
        return nil
    end
    local lot = math.clamp(tonumber(core.meta_get(x, y, z, "lot")) or 1, 1, 64)
    return { goods = goods, lot = lot, stock = core.container_count(x, y, z, goods, "raw"), price = price,
             cost = core.container_count(x, y, z, price, "fuel") }
end

local looking: { [number]: { number } } = {}   -- who is at which stall's page

-- An item as a page says it: "farm:bread" is "bread", "ore:iron_lump" "iron lump".
local function said(item: number): string
    return (string.gsub(string.match(core.item_name(item), ":(.+)$") or core.item_name(item), "_", " "))
end

local function show(p: number, x: number, y: number, z: number, told: string?)
    looking[p] = { x, y, z }
    local page = { "# Market stall", `> {owner_of(x, y, z)}'s`, "" }
    local o = offer(x, y, z)
    if o == nil then
        table.insert(page, "Nothing for sale.")
    else
        local lots = o.stock // o.lot
        table.insert(page, `{o.lot} {said(o.goods)} for {o.cost} {said(o.price)}`)
        table.insert(page, `{lots} lots left`)
        if lots > 0 then
            table.insert(page, "[Buy](act:buy)")
        end
    end
    if may(p, x, y, z) then
        table.insert(page, "")
        table.insert(page, `?[lot size: {core.meta_get(x, y, z, "lot")}](act:lot {"{text}"})`)
        table.insert(page, "[Open the stall](act:open)")
    end
    if told then
        table.insert(page, "")
        table.insert(page, `> {told}`)
    end
    core.page_show(p, "stall", table.concat(page, "\n"))
end

core.event.on("core:used", function(ev)
    if ev.id ~= STALL then
        return
    end
    ev.cancel = true
    core.container_make(ev.x, ev.y, ev.z, SLOTS)
    if may(ev.player, ev.x, ev.y, ev.z) and owner_of(ev.x, ev.y, ev.z) == core.player_name(ev.player)
        and offer(ev.x, ev.y, ev.z) == nil then
        core.container_show(ev.x, ev.y, ev.z, SLOTS, ev.player)   -- an empty stall: straight to stocking it
    else
        show(ev.player, ev.x, ev.y, ev.z)
    end
end)

local function buy(p: number, x: number, y: number, z: number): string
    local o = offer(x, y, z)
    if o == nil or o.stock < o.lot then
        return "Sold out."
    end
    local price = core.item_name(o.price)
    if core.bag_count(p, price) < o.cost then
        return `You need {o.cost} {said(o.price)}.`
    end
    local paid = core.take_item(p, price, o.cost)
    local over = core.container_add(x, y, z, o.price, paid, "product")   -- what did not fit
    if over > 0 then
        core.container_take(x, y, z, o.price, paid - over, "product")
        core.give_item(p, price, paid)
        return "The till is full: its owner has to empty it."
    end
    local got = core.container_take(x, y, z, o.goods, o.lot, "raw")
    local goods = core.item_name(o.goods)
    local left = got - core.give_item(p, goods, got)
    if left > 0 then
        local px, py, pz = core.player_pos(p)
        core.drop_item(math.floor(px), math.floor(py), math.floor(pz), o.goods, left)
    end
    core.play_sound("trade:coins", x + 0.5, y + 1, z + 0.5)
    return `Bought {got} {said(o.goods)}.`
end

core.page_register("stall", "Market stall")
core.event.on("core:page_open", function(ev)
    local at = looking[ev.player]
    if ev.page == "stall" and at then
        show(ev.player, at[1], at[2], at[3])
    end
end)
core.event.on("core:page_action", function(ev)
    local at = looking[ev.player]
    if ev.page ~= "stall" or at == nil then
        return
    end
    local x, y, z = at[1], at[2], at[3]
    if core.get_block(x, y, z) ~= STALL then
        return
    end
    local px, py, pz = core.player_pos(ev.player)
    if px == nil or (px - x) ^ 2 + (py - y) ^ 2 + (pz - z) ^ 2 > 8 * 8 then
        show(ev.player, x, y, z, "Too far from the stall.")
        return
    end
    if ev.id == "buy" then
        show(ev.player, x, y, z, buy(ev.player, x, y, z))
    elseif ev.id == "open" and may(ev.player, x, y, z) then
        core.container_show(x, y, z, SLOTS, ev.player)
    elseif string.sub(ev.id, 1, 3) == "lot" and may(ev.player, x, y, z) then
        local n = tonumber(ev.text) or tonumber(string.sub(ev.id, 5))
        if n then
            core.meta_set(x, y, z, "lot", tostring(math.clamp(math.floor(n), 1, 64)))
        end
        show(ev.player, x, y, z)
    end
end)
