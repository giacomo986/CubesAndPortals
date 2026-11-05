--!strict
-- villagers: errands the villagers give. Part of the mod (init.luau says the order).

local common = require("./common")
local types = require("./types")
type Box = types.Box
type Errand = types.Errand
local age = common.age
local AID = common.AID
local DAY = common.DAY
local ems = common.ems
local GOODS = common.GOODS
local is_night = common.is_night
local panel_of = common.panel_of
local put_into = common.put_into
local said = common.said
local stock_of = common.stock_of
local to_treasury = common.to_treasury
local treasury = common.treasury
local village_at = common.village_at
local village_chests = common.village_chests

-- ------------------------------------------------------------ errands
--
-- ERRANDS the mayor and the merchant post for strangers, at most three a
-- village, each paid in rubies out of the treasury -- none is posted that
-- the treasury cannot pay, and its pay is set aside when it is taken. They
-- are seen on a NOTICE BOARD (villagers:board, by the well or put up by a
-- player in the village) and taken and handed in at the panel of whoever
-- gives them. Six kinds:
--
--   merchant  bring      what the village's chests are short of
--   merchant  procure    a rarer material it wants (diamonds, crystal...)
--   mayor     craft      tools for its people: an iron pick, a bow, torches
--   mayor     hunt       N creatures of the night killed inside the village
--   mayor     carry      a parcel to another village: paid on arriving there
--   mayor     explore    stand at a far place, then come back and say so
--
-- One taken at a time. Each lasts a day (DAY): one not taken goes, one taken
-- and not done goes back on the board, its pay back in the treasury -- and
-- nothing else is lost. Every one done is remembered by the village; after
-- five the merchant sells to that player at a fifth off.
-- (All in AID.err: this file's main chunk is at Luau's 200 locals.)

AID.err = { list = {} :: { [string]: { Errand } }, known = {} :: { [string]: { number } }, next = 1,
            done = {} :: { [string]: { [string]: number } } }
do
local E = AID.err
E.PROCURE = { { "ore:diamond", 2, 9 }, { "ore:raw_gold", 4, 5 }, { "ore:lapis", 16, 4 }, { "deep:shard", 8, 6 },
              { "rocks:amethyst_shard", 8, 4 }, { "animals:leather", 8, 3 }, { "deep:glowcap", 6, 5 },
              { "pets:goldfish", 1, 5 }, { "ore:cinnabar", 12, 4 } }
E.CRAFT = { { "furnace:iron_pick", 1, 6 }, { "furnace:iron_sword", 1, 6 }, { "furnace:iron_axe", 1, 6 },
            { "archery:bow", 1, 4 }, { "archery:arrow", 32, 4 }, { "torches:torch", 32, 3 },
            { "furnace:iron_shovel", 1, 5 }, { "animals:shears", 1, 4 } }

E.said = function(e: Errand): string
    if e.kind == "bring" or e.kind == "procure" then
        return `bring {e.n} {said(e.item)}`
    elseif e.kind == "craft" then
        local what = said(e.item)
        return `make and bring {if e.n > 1 then e.n .. " " elseif string.find(what, "^[aeiou]") then "an " else "a "}{what}`
    elseif e.kind == "hunt" then
        return `kill {e.n} creatures of the night in the village ({e.got} so far)`
    elseif e.kind == "carry" then
        local k = E.known[e.to]
        return if k then `carry a parcel to the village at {k[1]} {k[2]}` else "carry a parcel"
    end
    return `stand at {e.x} {e.z}, then come back{if e.got > 0 then " (been there)" else ""}`
end

E.load = function(rk: string): { Errand }
    local l = E.list[rk]
    if l then
        return l
    end
    l = {}
    for part in string.gmatch(core.storage_get(`errands {rk}`), "[^;]+") do
        local f = string.split(part, "|")
        if #f >= 13 then
            if f[4] == "ore:emerald" then f[4] = "ore:ruby" end   -- saved before 2026-11-01
            table.insert(l, { id = tonumber(f[1]) :: number, kind = f[2], giver = f[3], item = f[4],
                              n = tonumber(f[5]) :: number, x = tonumber(f[6]) :: number, z = tonumber(f[7]) :: number,
                              to = f[8], pay = tonumber(f[9]) :: number, posted = tonumber(f[10]) :: number,
                              taker = f[11], taken = tonumber(f[12]) :: number, got = tonumber(f[13]) :: number })
        end
    end
    E.list[rk] = l
    return l
end

E.save = function(rk: string)
    local parts = {}
    for _, e in E.list[rk] or {} do
        table.insert(parts, table.concat({ e.id, e.kind, e.giver, e.item, e.n, e.x, e.z, e.to, e.pay,
                                           math.floor(e.posted), e.taker, math.floor(e.taken), e.got }, "|"))
    end
    core.storage_set(`errands {rk}`, table.concat(parts, ";"))
    core.storage_set("errand next", tostring(E.next))
end

E.done_by = function(rk: string, name: string): number
    local d = E.done[rk]
    if d == nil then
        d = {}
        for part in string.gmatch(core.storage_get(`errands done {rk}`), "[^;]+") do
            local n, who = string.match(part, "^(%d+),(.+)$")
            if n and who then
                d[who] = tonumber(n) :: number
            end
        end
        E.done[rk] = d
    end
    return d[name] or 0
end

-- A fifth off what the merchant sells, for whoever has done five errands here.
E.discount = function(p: number, rk: string, price: number): number
    return if E.done_by(rk, core.player_name(p)) >= 5 then math.max(1, math.floor(price * 0.8 + 0.5)) else price
end

-- The errand this player has taken, anywhere, and its village.
E.mine = function(name: string): (Errand?, string?)
    for rk in E.known do
        for _, e in E.load(rk) do
            if e.taker == name then
                return e, rk
            end
        end
    end
    return nil, nil
end

-- A new errand for a village, or nil when nothing fits or the treasury
-- cannot pay it.
E.make = function(rk: string): Errand?
    local b = common.villages[rk]
    local k = E.known[rk]
    if b == nil or k == nil then
        return nil
    end
    local open = E.load(rk)
    local has: { [string]: boolean } = {}
    for _, e in open do
        has[e.kind] = true
    end
    local kinds = { "bring", "procure", "craft", "hunt", "carry", "explore" }
    local kind = kinds[math.random(#kinds)]
    if has[kind] then
        return nil
    end
    local e: Errand = { id = E.next, kind = kind, giver = "mayor", item = "", n = 0, x = 0, z = 0, to = "",
                        pay = 0, posted = age(), taker = "", taken = 0, got = 0 }
    if kind == "bring" then
        local chests = village_chests(rk)
        local short = {}
        for _, g in GOODS do
            if core.item_id(g[1]) ~= 0 and stock_of(chests, g[1]) < g[2] then
                table.insert(short, g)
            end
        end
        if #short == 0 then
            return nil
        end
        local g = short[math.random(#short)]
        e.giver, e.item, e.n, e.pay = "merchant", g[1], g[2] * 2, g[3] * 2 + 1
    elseif kind == "procure" or kind == "craft" then
        local list = if kind == "procure" then E.PROCURE else E.CRAFT
        local g = list[math.random(#list)]
        if core.item_id(g[1]) == 0 then
            return nil
        end
        e.giver = if kind == "procure" then "merchant" else "mayor"
        e.item, e.n, e.pay = g[1], g[2], g[3]
    elseif kind == "hunt" then
        e.n = math.random(3, 6)
        e.pay = e.n
    elseif kind == "carry" then
        local best, bd = nil, math.huge
        for other, o in E.known do
            local d = math.sqrt((o[1] - k[1]) ^ 2 + (o[2] - k[2]) ^ 2)
            if other ~= rk and d >= 150 and d < bd then
                best, bd = other, d
            end
        end
        if best == nil then
            return nil
        end
        e.to, e.pay = best, math.max(3, math.floor(bd / 100) + 2)
    else
        local a, d = math.random() * 2 * math.pi, 300 + math.random() * 300
        e.x, e.z = math.floor(k[1] + math.cos(a) * d), math.floor(k[2] + math.sin(a) * d)
        e.pay = math.floor(d / 100) + 2
    end
    if treasury(rk) < e.pay then
        return nil
    end
    E.next += 1
    return e
end

-- Every few seconds: villages near players get errands up to three, and what has
-- lasted a day goes (posted) or goes back on the board (taken).
E.tick = function()
    local now = age()
    for rk in common.villages do
        local l = E.load(rk)
        local changed = false
        for i = #l, 1, -1 do
            local e = l[i]
            if e.taker == "" and now - e.posted > DAY then
                table.remove(l, i)
                changed = true
            elseif e.taker ~= "" and now - e.taken > DAY then
                to_treasury(rk, e.pay)
                for _, q in core.players() do
                    if core.player_name(q) == e.taker then
                        core.notice(q, `Not done in a day, and back on the board: {E.said(e)}`)
                        if e.kind == "carry" then
                            core.take_item(q, "villagers:parcel", 1)
                        end
                    end
                end
                e.taker, e.taken, e.got, e.posted = "", 0, 0, now
                changed = true
            end
        end
        if #l < 3 then
            local e = E.make(rk)
            if e then
                table.insert(l, e)
                changed = true
            end
        end
        if changed then
            E.save(rk)
        end
    end
    -- An explorer at the place; a carrier at the other village.
    for _, q in core.players() do
        local e, erk = E.mine(core.player_name(q))
        local to = e and E.known[e.to]
        local qx, _, qz = core.player_pos(q)
        if e and e.kind == "carry" and to and qx and (qx - to[1]) ^ 2 + (qz - to[2]) ^ 2 < 32 * 32
            and core.take_item(q, "villagers:parcel", 1) == 1 then
            E.finish(q, e, erk :: string)
        elseif e and e.kind == "explore" and e.got == 0 then
            local x, _, z = core.player_pos(q)
            if x and (x - e.x) ^ 2 + (z - e.z) ^ 2 < 12 * 12 then
                e.got = 1
                core.notice(q, "You are there: go back and tell the mayor.")
                local _, rk = E.mine(core.player_name(q))
                E.save(rk :: string)
            end
        end
    end
end

-- The board: every errand of the village, who gives it and who has it.
E.board = function(p: number, rk: string)
    local l = E.load(rk)
    local news = AID.ev.lines(rk)
    local top = #news * 1.1
    local paper = AID.ev.paper(rk)
    local low = 1.9 + top + math.max(1, #l) * 1.1   -- under the errands: the village paper
    local name = panel_of(p, "board")
    core.ui_define(name, 15, 3.4 + top + math.max(1, #l) * 1.1 + (if #paper > 0 then 1.1 + #paper * 1.1 else 0))
    if #paper > 0 then
        core.ui_label(name, 0.4, low, "The village paper:")
        for i, line in paper do
            core.ui_label(name, 0.4, low + i * 1.1, line)
        end
        low += 1.1 + #paper * 1.1
    end
    for i, n in news do
        core.ui_label(name, 0.4, 0.3 + (i - 1) * 1.1, n)
    end
    core.ui_label(name, 0.4, 0.3 + top, `The village's errands. The treasury holds {ems(treasury(rk))}.`)
    for i, e in l do
        local who = if e.taker == "" then `ask the {e.giver}` else `taken by {e.taker}`
        core.ui_label(name, 0.4, 0.2 + top + i * 1.1, `{E.said(e)}: {ems(e.pay)} ({who})`)
    end
    if #l == 0 then
        core.ui_label(name, 0.4, 1.3 + top, "Nothing today.")
    end
    core.ui_button(name, "close", 0.4, low, 14.2, 1.1, "Close")
    core.ui_show(name, true, p)
end

-- The giver's panel: its errands to take, and the player's to hand in.
E.show = function(p: number, rk: string, giver: string)
    local who = core.player_name(p)
    local mine, mine_rk = E.mine(who)
    local rows = {}
    for _, e in E.load(rk) do
        if e.giver == giver and (e.taker == "" or e.taker == who) then
            table.insert(rows, e)
        end
    end
    local name = panel_of(p, "errands")
    core.ui_define(name, 15, 3.6 + math.max(1, #rows) * 1.2)
    local done = E.done_by(rk, who)
    core.ui_label(name, 0.4, 0.3, `Errands of the {giver}. Done here: {done}{if done >= 5 then " (a fifth off at the merchant)" else ""}.`)
    for i, e in rows do
        local label = if e.taker == who then `Hand in: {E.said(e)}` else `Take: {E.said(e)} -- {ems(e.pay)}`
        core.ui_button(name, `e {e.id}`, 0.4, 0.1 + i * 1.2, 14.2, 1.1, label)
    end
    if #rows == 0 then
        core.ui_label(name, 0.4, 1.3, "Nothing for you today.")
    end
    core.ui_button(name, "close", 0.4, 1.7 + math.max(1, #rows) * 1.2, 14.2, 1.1, "Close")
    E.talking = E.talking or {}
    E.talking[p] = { rk = rk, mine_rk = mine_rk }
    core.ui_show(name, true, p)
end

-- Errands done by a player, counted up (a siege held or a caravan escorted
-- counts too, mods/villagers' events).
E.credit = function(rk: string, who: string, n: number)
    E.done_by(rk, who)
    E.done[rk][who] = (E.done[rk][who] or 0) + n
    local parts = {}
    for n2, c in E.done[rk] do
        table.insert(parts, `{c},{n2}`)
    end
    core.storage_set(`errands done {rk}`, table.concat(parts, ";"))
end

-- Paid out of what was set aside, and remembered.
E.finish = function(p: number, e: Errand, rk: string)
    local who = core.player_name(p)
    core.give_item(p, "ore:ruby", e.pay)
    local l = E.load(rk)
    for i, x in l do
        if x == e then
            table.remove(l, i)
        end
    end
    E.save(rk)
    E.credit(rk, who, 1)
    core.notice(p, `Done: {ems(e.pay)}. Thank you.`)
    core.log(`villagers: {rk}: {who} did an errand ({e.kind}) for {e.pay}`)
    -- Somebody who went far for the village is told, as well, where the
    -- nearest portal they have not done stands (none on a plaza).
    if e.kind == "explore" then
        local px, _, pz = core.player_pos(p)
        if px and pz and AID.pt.near(px, pz, 1200, p)[1] then
            core.notice(p, "\"And since you like the road -- one more thing, for nothing.\"")
            AID.pt.tell(p)
        end
    end
end

E.click = function(p: number, id: number)
    local t = E.talking and E.talking[p]
    if t == nil then
        return
    end
    local who = core.player_name(p)
    local mine, mine_rk = E.mine(who)
    if mine and mine.id == id then
        -- Handing in.
        local e, rk = mine, mine_rk :: string
        if e.kind == "bring" or e.kind == "procure" or e.kind == "craft" then
            local got = core.take_item(p, e.item, e.n)
            if got < e.n then
                if got > 0 then
                    core.give_item(p, e.item, got)
                end
                core.notice(p, `That is {e.n} {said(e.item)}.`)
                return
            end
            local left = put_into(village_chests(rk), e.item, e.n)
            if left > 0 then
                local x, y, z = core.player_pos(p)
                core.drop_item(math.floor(x or 0), math.floor(y or 0), math.floor(z or 0), core.item_id(e.item), left)
            end
        elseif e.kind == "hunt" and e.got < e.n then
            core.notice(p, `{e.got} of {e.n} so far.`)
            return
        elseif e.kind == "explore" and e.got == 0 then
            core.notice(p, `You have not been to {e.x} {e.z} yet.`)
            return
        elseif e.kind == "carry" then
            core.notice(p, "The parcel is for the other village: it is paid there.")
            return
        end
        E.finish(p, e, rk)
        return
    end
    if mine then
        core.notice(p, `You have an errand already: {E.said(mine)}.`)
        return
    end
    for _, e in E.load(t.rk) do
        if e.id == id and e.taker == "" then
            if treasury(t.rk) < e.pay then
                core.notice(p, "The treasury cannot pay for that now.")
                return
            end
            to_treasury(t.rk, -e.pay)   -- set aside
            e.taker, e.taken, e.got = who, age(), 0
            if e.kind == "carry" then
                core.give_item(p, "villagers:parcel", 1)
            end
            E.save(t.rk)
            core.notice(p, `Taken: {E.said(e)}, within a day.`)
            return
        end
    end
end

core.register_item("villagers:parcel", 1, "", 0, 1.0, "")
core.register_model("villagers:parcel_held", "parcel_held.gltf")   -- tools/make_held_models.py
E.BOARD = core.register_node("villagers:board", 150, 112, 70, true, 0, "upright", 0, 1.5, "wood")
core.node_box(E.BOARD, 0.45, 0, 0.45, 0.55, 1.0, 0.55)
core.node_box(E.BOARD, 0.05, 0.55, 0.42, 0.95, 1.25, 0.58)
core.register_model("villagers:board_held", "board_held.gltf")   -- tools/make_held_models.py
core.recipe_shaped("villagers:board", 1,
    "timber:planks cane:paper   timber:planks",
    "-             timber:stick -")

core.event.on("core:world_ready", function()
    E.next = tonumber(core.storage_get("errand next")) or 1
    for part in string.gmatch(core.storage_get("known villages"), "[^;]+") do
        local rk, x, z = string.match(part, "^(.+)=(-?%d+),(-?%d+)$")
        if rk and x and z then
            E.known[rk] = { tonumber(x) :: number, tonumber(z) :: number }
        end
    end
end)

-- Villages seen, by their middle: where a parcel may go. Every one the
-- world has made (villages:extent), lived in or not yet.
core.event.on("villages:extent", function(ev)
    E.saw(`{ev.rx},{ev.rz}`, { x1 = ev.x1, z1 = ev.z1, x2 = ev.x2, z2 = ev.z2, y = 0, homes = {} })
end)
E.saw = function(rk: string, b: Box)
    if E.known[rk] == nil then
        E.known[rk] = { (b.x1 + b.x2) // 2, (b.z1 + b.z2) // 2 }
        local parts = {}
        for k, c in E.known do
            table.insert(parts, `{k}={c[1]},{c[2]}`)
        end
        core.storage_set("known villages", table.concat(parts, ";"))
    end
end

local since = 0.0
core.event.on("core:tick", function(ev)
    since += ev.dt
    if since >= 5 then
        since = 0
        E.tick()
    end
end)

-- A creature of the night killed in the village, at night, by a hunter.
core.event.on("core:hit", function(ev)
    if not ev.killed then
        return
    end
    local e, rk = E.mine(core.player_name(ev.player))
    if e == nil or e.kind ~= "hunt" or e.got >= e.n or not is_night() then
        return
    end
    local kind = core.mob_kind(ev.target)
    if kind == "" or string.find(kind, "^animals:") or string.find(kind, "^villagers:")
        or string.find(kind, "^helpers:") or string.find(kind, "^pets:") then
        return
    end
    local x, _, z = core.player_pos(ev.player)
    if x and village_at(x, z) == rk then
        e.got += 1
        core.notice(ev.player, if e.got >= e.n then "That is enough: go and tell the mayor."
            else `{e.got} of {e.n}.`)
        E.save(rk :: string)
    end
end)

core.event.on("core:used", function(ev)
    if ev.id ~= E.BOARD then
        return
    end
    ev.cancel = true
    local rk = village_at(ev.x, ev.z)
    if rk then
        E.board(ev.player, rk)
    else
        core.notice(ev.player, "A board, but no village here to post errands on it.")
    end
end)

core.event.on("core:ui_click", function(ev)
    if ev.panel == panel_of(ev.player, "board") then
        core.ui_show(ev.panel, false, ev.player)
    elseif ev.panel == panel_of(ev.player, "errands") then
        core.ui_show(ev.panel, false, ev.player)
        local id = tonumber(string.match(ev.id, "^e (%d+)$") or "")
        if id then
            E.click(ev.player, id)
        end
    end
end)
end
