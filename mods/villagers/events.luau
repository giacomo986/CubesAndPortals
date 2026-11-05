--!strict
-- villagers: the village's events: raiders, sieges, feasts and caravans. Part of the mod (init.luau says the order).

local common = require("./common")
local types = require("./types")
type V = types.V
type Siege = types.Siege
type Feast = types.Feast
type Caravan = types.Caravan
local age = common.age
local AID = common.AID
local ARROW = common.ARROW
local DAY = common.DAY
local DEAR = common.DEAR
local ems = common.ems
local extents = common.extents
local GOODS = common.GOODS
local is_night = common.is_night
local panel_of = common.panel_of
local people = common.people
local price_of = common.price_of
local put_into = common.put_into
local said = common.said
local save_trust = common.save_trust
local somewhere_in = common.somewhere_in
local standable = common.standable
local stock_of = common.stock_of
local take_from = common.take_from
local to_treasury = common.to_treasury
local treasury = common.treasury
local trust_of = common.trust_of
local village_at = common.village_at
local village_chests = common.village_chests
local wait = common.wait
local walk = common.walk
local watch = common.watch

-- ------------------------------------------------------- the village's events
--
-- What happens in a village now and then (agreed 2026-10-28), each told on
-- its notice board, by the hall's bell (bell.ogg) and in a notice to the
-- players near:
--
--   SIEGE    rare -- a night in seven to ten a village, and only with a
--            player near: the mayor warns at dusk, and in the night three
--            waves of zombies and skeletons (villagers:raider, its archer)
--            come from the edge for its people and the players; the watch
--            fights them (monster_in) and keeps awake. No villager lost: the
--            players who stood there are paid 5 rubies from the treasury
--            each, it counts as two errands done (the merchant's fifth off),
--            and any strike against them is forgotten.
--   CARAVAN  about once a day a village with a player near sends a merchant
--            and a mule (mule.gltf: tools/make_mule_model.py) to another
--            village, with real goods out of its chests -- what it has
--            plenty of and the other lacks. It walks while it is seen and is
--            carried on along the line when no one is near. On the road it
--            sells its cargo (right click), and whoever escorts it is paid on
--            its arrival; at night monsters come for it, and killed, its
--            goods are left where it fell. Arrived, its goods go into the
--            other village's chests and its price into its own treasury.
--            Caravans COME too, now and then, from a village near that nobody
--            has been to yet, with what its land is known for (EV.TYPICAL:
--            sandstone and cactus from the desert, spruce and leather from the
--            cold...). Every caravan walks the paths between the villages
--            (mods/villages' network, villages:route_ask), carried along them
--            too; its merchant tells the way to its village, aloud and on the
--            player's map (mods/maps' maps:route).
--   FEAST    the last day of summer (farm:season's `left`): stalls and
--            lanterns in the square (the board's, by the well), the people
--            gathered there, food at half price, music (music.ogg) and in
--            the evening fireworks (mods/fireworks). Put away at night.
--
-- /village siege|feast|caravan (admin) starts one now where the admin stands.

AID.ev = { news = {} :: { [string]: { { any } } }, siege = {} :: { [string]: Siege },
           raiders = {} :: { [number]: { any } }, feast = {} :: { [string]: Feast }, caravans = {} :: { Caravan },
           asked = {} :: { [string]: number }, talk = {} :: { [number]: Caravan }, left = 99 }
do
local EV = AID.ev
local E = AID.err
core.register_mob("villagers:raider", core.model_id("mobs:zombie"), 20, 2.2, "worker", 3, 1.6, 1.0, "mobs:flesh", 1)
core.register_mob("villagers:raider_archer", core.model_id("mobs:skeleton"), 16, 2.4, "worker", 3, 15.0, 2.0,
    "mobs:bone", 2, ARROW)
core.register_mob("villagers:mule", core.register_model("villagers:mule", "mule.gltf"), 30, 2.6, "worker", 0,
    1.6, 1.0, "", 1)
core.mob_voice("villagers:raider", "mobs:zombie")
core.mob_voice("villagers:raider_archer", "mobs:skeleton")
core.event.on("farm:season", function(ev)
    EV.left = ev.left
end)

-- The players within r of a village's ground.
EV.near = function(rk: string, r: number): { number }
    local b, out = common.villages[rk], {}
    if b == nil then
        return out
    end
    for _, p in core.players() do
        local x, _, z = core.player_pos(p)
        if x and z and x > b.x1 - r and x < b.x2 + r and z > b.z1 - r and z < b.z2 + r then
            table.insert(out, p)
        end
    end
    return out
end

-- The node of that name in a village's ground nearest its middle.
EV.spot = function(rk: string, name: string): { number }?
    local b = common.villages[rk]
    if b == nil then
        return nil
    end
    local mx, mz = (b.x1 + b.x2) / 2, (b.z1 + b.z2) / 2
    local f = AID.find(b.x1, b.y - 12, b.z1, b.x2, b.y + 12, b.z2, name)
    local best, bd = nil, math.huge
    for i = 1, #f, 3 do
        local d = (f[i] - mx) ^ 2 + (f[i + 2] - mz) ^ 2
        if d < bd then
            best, bd = { f[i], f[i + 1], f[i + 2] }, d
        end
    end
    return best
end

-- A cell to stand in at x, z, near y; nil where nothing is loaded or none.
EV.ground = function(x: number, y: number, z: number): number?
    for dy = 0, 10 do
        for _, gy in { y + dy, y - dy } do
            if standable(x, gy, z) then
                return gy
            end
        end
    end
    return nil
end

EV.announce = function(rk: string, text: string)
    local l = EV.news[rk] or {}
    table.insert(l, 1, { text, age() + DAY })
    while #l > 3 do
        table.remove(l)
    end
    EV.news[rk] = l
    local bell = EV.spot(rk, "villages:bell")
    if bell then
        core.play_sound("villagers:bell", bell[1] + 0.5, bell[2] + 0.5, bell[3] + 0.5)
    end
    for _, p in EV.near(rk, 48) do
        core.notice(p, text)
    end
    core.log(`villagers: {rk}: {text}`)
end

-- What the board shows above the errands: the news of the last day.
EV.lines = function(rk: string): { string }
    local out = {}
    for _, n in EV.news[rk] or {} do
        if n[2] > age() then
            table.insert(out, n[1])
        end
    end
    return out
end

-- ------------------------------------------------------------ the raiders

EV.raider = function(name: string, x: number, y: number, z: number, rk: string, c: Caravan?): number
    if core.peaceful() then
        return 0   -- a peaceful world has no monsters, not even a village's
    end
    local id = core.spawn_mob(name, x + 0.5, y, z + 0.5)
    if id ~= 0 then
        EV.raiders[id] = { rk, -1, false, c }   -- its village, whom it is after, a player?, its caravan
    end
    return id
end

-- After the nearest of a village's people, its watch, a caravan's merchant
-- or a player, within 24; among its homes when nothing is near.
EV.steer = function(id: number, r: { any })
    local x, y, z = core.mob_pos(id)
    if x == nil or y == nil or z == nil then
        EV.raiders[id] = nil
        return
    end
    local best, bd, player = 0, 24 * 24, false
    for _, p in core.players() do
        local px, py, pz = core.player_pos(p)
        if px and py and pz then
            local d = (px - x) ^ 2 + (py - y) ^ 2 + (pz - z) ^ 2
            if d < bd then
                best, bd, player = p, d, true
            end
        end
    end
    local function consider(mid: number)
        local mx, my, mz = core.mob_pos(mid)
        if mx and my and mz then
            local d = (mx - x) ^ 2 + (my - y) ^ 2 + (mz - z) ^ 2
            if d < bd then
                best, bd, player = mid, d, false
            end
        end
    end
    local c = r[4] :: Caravan?
    if c then
        if c.m ~= 0 then consider(c.m) end
        if c.mule ~= 0 then consider(c.mule) end
    else
        for _, list in { people, watch } do
            for _, v in list do
                if v.id ~= 0 and not v.sleeping then
                    consider(v.id)
                end
            end
        end
    end
    if best ~= r[2] or player ~= r[3] then
        r[2], r[3] = best, player
        core.mob_hunt(id, if player then best else 0)
        core.mob_attack(id, if player then 0 else best)
    end
    if best == 0 and common.villages[r[1]] and core.mob_going(id) == 0 then
        local gx, gy, gz = somewhere_in(r[1])
        if gx and gy and gz then
            core.mob_goto(id, gx, gy, gz)
        end
    end
end

-- How many of a village's (or a caravan's) raiders are still up.
EV.up = function(rk: string, c: Caravan?): number
    local n = 0
    for id, r in EV.raiders do
        if core.mob_pos(id) == nil then
            EV.raiders[id] = nil
        elseif r[1] == rk and r[4] == c then
            n += 1
        end
    end
    return n
end

-- ------------------------------------------------------------ the siege

EV.begin_siege = function(rk: string)
    if core.peaceful() then
        return
    end
    EV.siege[rk] = { wave = 0, next = 0, lost = 0, started = false, fought = {} }
    core.storage_set(`siege {rk}`, tostring(math.floor(age())))
    EV.announce(rk, "The mayor rings the bell: monsters are gathering, and tonight they will come. "
        .. "Stand with the watch!")
end

-- One of a village's people fell (step): a siege counts it.
EV.fell = function(v: V)
    local rk = v.rk or village_at(v.hx, v.hz)
    local s = rk and EV.siege[rk]
    if s and s.started and v.kind ~= "guard" and v.kind ~= "archer" then
        s.lost += 1
        core.log(`villagers: {rk}: the {v.kind} at {v.hx} {v.hz} fell in the siege`)
    end
end

EV.wave = function(rk: string, s: Siege)
    local b = common.villages[rk]
    s.wave += 1
    if b == nil then
        return
    end
    s.next = common.clock + 50
    local n = 2 + s.wave + #EV.near(rk, 24) // 2   -- 3, 4, 5, and more for a crowd
    local sidex = math.random(0, 1) == 1
    local far = math.random(0, 1) == 1
    local put = 0
    for _ = 1, n * 4 do
        if put >= n then
            break
        end
        local x, z
        if sidex then
            x, z = if far then b.x2 + 6 else b.x1 - 6, math.random(b.z1, b.z2)
        else
            x, z = math.random(b.x1, b.x2), if far then b.z2 + 6 else b.z1 - 6
        end
        local y = EV.ground(x, b.y, z) or EV.ground(x, core.terrain_height(x, z) + 1, z)
        if y then
            EV.raider(if math.random(3) == 1 then "villagers:raider_archer" else "villagers:raider", x, y, z, rk)
            put += 1
        end
    end
    for _, p in EV.near(rk, 48) do
        core.notice(p, `The siege: wave {s.wave} of 3 comes from the {if sidex then (if far then "east" else "west")
            else (if far then "south" else "north")}.`)
    end
end

EV.end_siege = function(rk: string, s: Siege)
    for id, r in EV.raiders do
        if r[1] == rk and r[4] == nil then
            core.mob_remove(id)
            EV.raiders[id] = nil
        end
    end
    EV.siege[rk] = nil
    if not s.started then
        return
    end
    if s.lost > 0 then
        EV.announce(rk, `The siege is over. {s.lost} of the village fell.`)
        return
    end
    local names = {}
    for name in s.fought do
        table.insert(names, name)
        local pay = math.min(5, treasury(rk))
        to_treasury(rk, -pay)
        for _, q in core.players() do
            if core.player_name(q) == name then
                core.give_item(q, "ore:ruby", 5)
            end
        end
        E.credit(rk, name, 2)
        local t = trust_of(rk, name)
        if t.strikes > 0 then
            t.strikes, t.hostile = 0, false
            save_trust(rk)
        end
    end
    table.sort(names)
    EV.announce(rk, `The siege is over and no one was lost. The mayor thanks {if #names > 0
        then table.concat(names, ", ") else "the watch"}{if #names > 0 then ": 5 rubies each" else ""}.`)
end

EV.run_siege = function(rk: string, s: Siege)
    if not s.started then
        local t = core.time_of_day()
        if is_night() then
            s.started, s.next = true, common.clock
        elseif t > 0.3 and t < 0.65 then
            EV.end_siege(rk, s)   -- warned and never came: a day went by
        end
        return
    end
    for _, p in EV.near(rk, 24) do
        s.fought[core.player_name(p)] = true
    end
    local up = EV.up(rk, nil)
    if s.wave < 3 and common.clock >= s.next then
        EV.wave(rk, s)
    elseif (s.wave >= 3 and up == 0) or not is_night() then
        EV.end_siege(rk, s)
    end
end

-- ------------------------------------------------------------ the feast

EV.FOOD = DEAR.winter
EV.price = function(rk: string, item: string, price: number): number
    return if EV.feast[rk] and EV.FOOD[item] then math.max(1, price // 2) else price
end

EV.begin_feast = function(rk: string)
    local b = common.villages[rk]
    local sq = EV.spot(rk, "villagers:board") or EV.spot(rk, "villages:bell")
    local mayor = watch[`{rk} mayor 1`]
    if sq == nil and mayor then
        sq = { mayor.hx, mayor.hy, mayor.hz }
    end
    if b == nil or sq == nil then
        return
    end
    core.storage_set(`feast {rk}`, tostring(math.floor(age() / DAY)))
    local f: Feast = { x = sq[1], y = sq[2], z = sq[3], put = {}, fired = false, played = 0 }
    local function put(x: number, y: number, z: number, name: string)
        local nid = core.node_id(name)
        if nid ~= 0 and core.get_block(x, y, z) == 0 then
            core.set_block(x, y, z, nid)
            table.insert(f.put, { x, y, z, nid })
        end
    end
    -- Four stalls round the square, a table with a lantern on it under an
    -- awning of wool on two posts, and a lantern on a post at each corner.
    local AWNING = { "colors:red_wool", "colors:white_wool", "colors:yellow_wool", "colors:blue_wool" }
    -- Only on ground level with the square, the three cells across clear
    -- three high: not on a roof, a slope or against a wall; nearer or
    -- further out where it is not.
    local function fits(x: number, y: number, z: number, ax: number, az: number): boolean
        if math.abs(y - f.y) > 2 then
            return false
        end
        for s = -1, 1 do
            local cx, cz = x + ax * s, z + az * s
            if not standable(cx, y, cz) or core.get_block(cx, y + 2, cz) ~= 0 then
                return false
            end
        end
        return true
    end
    for k, d in { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
        local ax, az = if d[1] == 0 then 1 else 0, if d[1] == 0 then 0 else 1
        local x, y, z = 0, nil, 0
        for _, r in { 6, 5, 7, 4, 8 } do
            x, z = f.x + d[1] * r, f.z + d[2] * r
            y = EV.ground(x, f.y, z)
            if y and fits(x, y, z, ax, az) then
                break
            end
            y = nil
        end
        if y then
            put(x, y, z, "building:oak_table")
            put(x, y + 1, z, "building:lantern")
            for _, s in { -1, 1 } do
                put(x + ax * s, y, z + az * s, "building:oak_fence")
                put(x + ax * s, y + 1, z + az * s, "building:oak_fence")
            end
            for s = -1, 1 do
                put(x + ax * s, y + 2, z + az * s, AWNING[k])
            end
        end
    end
    for _, d in { { 4, 4 }, { -4, 4 }, { 4, -4 }, { -4, -4 } } do
        local x, z = f.x + d[1], f.z + d[2]
        local y = EV.ground(x, f.y, z)
        if y and math.abs(y - f.y) <= 2 then
            put(x, y, z, "building:oak_fence")
            put(x, y + 1, z, "building:lantern")
        end
    end
    EV.feast[rk] = f
    EV.announce(rk, "The harvest feast is today, in the square: food at half price, music, "
        .. "and fireworks tonight!")
end

EV.end_feast = function(rk: string, f: Feast)
    for i = #f.put, 1, -1 do
        local c = f.put[i]
        if core.get_block(c[1], c[2], c[3]) == c[4] then
            core.set_block(c[1], c[2], c[3], 0)
        end
    end
    EV.feast[rk] = nil
end

EV.run_feast = function(rk: string, f: Feast)
    local t = core.time_of_day()
    if t > 0.9 or t < 0.2 then
        EV.end_feast(rk, f)
        return
    end
    if common.clock >= f.played then
        f.played = common.clock + 12
        core.play_sound("villagers:music", f.x + 0.5, f.y + 1, f.z + 0.5)
    end
    if not f.fired and t >= 0.76 then
        f.fired = true
        core.event.emit("fireworks:show", { x = f.x, y = f.y + 2, z = f.z, seconds = 120 })
    end
end

-- A villager's day at the feast (life): to the square, a while there, and
-- back to work now and then.
EV.feasting = function(v: V): boolean
    local f = EV.feast[village_at(v.hx, v.hz) or ""]
    if f == nil or core.time_of_day() > 0.86 or math.random() < 0.25 then
        return false
    end
    local a = math.random() * 2 * math.pi
    local x, z = math.floor(f.x + 0.5 + math.cos(a) * (2 + math.random() * 3)),
        math.floor(f.z + 0.5 + math.sin(a) * (2 + math.random() * 3))
    local y = EV.ground(x, f.y, z)
    if y and walk(v, x, y, z, 30) then
        core.mob_face(v.id, f.x + 0.5, f.z + 0.5)
        wait(v, 10 + math.random() * 15)
    else
        wait(v, 3)
    end
    return true
end

-- ------------------------------------------------------------ the caravans

EV.save_caravans = function()
    local parts = {}
    for _, c in EV.caravans do
        local g = {}
        for item, n in c.goods do
            table.insert(g, `{item}={n}`)
        end
        table.insert(parts, table.concat({ c.from, c.to, math.floor(c.x), math.floor(c.y), math.floor(c.z),
                                            table.concat(g, ","), c.escort }, "|"))
    end
    core.storage_set("caravans", table.concat(parts, ";"))
end

core.event.on("core:world_ready", function()
    for part in string.gmatch(core.storage_get("caravans"), "[^;]+") do
        local f = string.split(part, "|")
        if #f >= 6 then
            local goods = {}
            for item, n in string.gmatch(f[6], "([^,=]+)=(%d+)") do
                goods[if item == "ore:emerald" then "ore:ruby" else item] = tonumber(n) :: number
            end
            table.insert(EV.caravans, { from = f[1], to = f[2], x = tonumber(f[3]) :: number, y = tonumber(f[4]) :: number,
                                        z = tonumber(f[5]) :: number, goods = goods, m = 0, mule = 0, escort = f[7] or "",
                                        best = math.huge, best_at = 0, att_at = 0, k = 1 })
        end
    end
end)

EV.middle = function(rk: string): (number?, number?)
    local b = common.villages[rk]
    if b then
        return (b.x1 + b.x2) // 2, (b.z1 + b.z2) // 2
    end
    local k = E.known[rk]
    if k then
        return k[1], k[2]
    end
    return nil, nil
end

-- Sets a caravan off from a village, if another lived in is far enough and
-- its chests have plenty of something; true when it went.
EV.send = function(rk: string): boolean
    for _, c in EV.caravans do
        if c.from == rk then
            return false
        end
    end
    local mx, mz = EV.middle(rk)
    local merchant = watch[`{rk} merchant 1`]
    if mx == nil or mz == nil or merchant == nil then
        return false
    end
    local to, td = nil, math.huge
    for other in E.known do
        local ox, oz = EV.middle(other)
        if other ~= rk and ox and oz then
            local d = math.sqrt((ox - mx) ^ 2 + (oz - mz) ^ 2)
            if d >= 100 and d < 2000 and d < td then
                to, td = other, d
            end
        end
    end
    if to == nil then
        return false
    end
    local src = village_chests(rk)
    local tx, tz = EV.middle(to)
    local dst = if tx and common.villages[to] and core.loaded(tx, common.villages[to].y, tz) == 1 then village_chests(to) else nil
    local can = {}
    for _, g in GOODS do
        if stock_of(src, g[1]) >= g[2] + (g[2] + 1) // 2 and (dst == nil or stock_of(dst, g[1]) < g[2]) then
            table.insert(can, g)
        end
    end
    local goods, any = {}, false
    for _ = 1, 3 do
        if #can == 0 then
            break
        end
        local g = table.remove(can, math.random(#can))
        -- What it has over a bundle, kept at home: a bundle at most.
        local n = take_from(src, g[1], math.min(g[2], stock_of(src, g[1]) - g[2]))
        if n > 0 then
            goods[g[1]], any = n, true
        end
    end
    if not any then
        return false
    end
    local mxp, myp, mzp = merchant.hx, merchant.hy, merchant.hz
    if merchant.id ~= 0 then
        local x, y, z = core.mob_pos(merchant.id)
        if x and y and z then
            mxp, myp, mzp = math.floor(x), math.floor(y), math.floor(z)
        end
    end
    table.insert(EV.caravans, { from = rk, to = to, x = mxp, y = myp, z = mzp, goods = goods, m = 0, mule = 0,
                                escort = "", best = math.huge, best_at = common.clock, att_at = 0, k = 1 })
    EV.save_caravans()
    local things = {}
    for item, n in goods do
        table.insert(things, `{n} {said(item)}`)
    end
    table.sort(things)
    EV.announce(rk, `A caravan leaves for the village at {tx} {tz}, with {table.concat(things, ", ")}. `
        .. "Escort it and be paid when it gets there.")
    return true
end

-- What a village's land is known for, by its middle's climate: what a
-- caravan from a village nobody has been to brings (item, a bundle, price).
EV.TYPICAL = {
    ["core:desert"] = { { "rocks:sandstone", 32, 1 }, { "plants:cactus", 8, 1 }, { "core:sand", 32, 1 },
                        { "ore:raw_gold", 1, 1 } },
    ["core:tundra"] = { { "woods:spruce_log", 8, 1 }, { "animals:leather", 4, 1 }, { "animals:wool", 8, 1 },
                        { "pets:cooked_fish", 4, 1 } },
    ["core:beach"] = { { "pets:cooked_fish", 4, 1 }, { "pets:fish", 6, 1 }, { "core:sand", 32, 1 },
                       { "rocks:cobblestone", 32, 1 } },
    high = { { "ore:raw_iron", 3, 1 }, { "ore:coal", 8, 1 }, { "rocks:cobblestone", 32, 1 }, { "core:stone", 24, 1 } },
    other = { { "farm:wheat", 12, 1 }, { "farm:bread", 4, 1 }, { "trees:trunk", 8, 1 }, { "animals:wool", 8, 1 },
              { "animals:steak", 4, 1 } },
}
-- A good by its item: the merchant's (GOODS) or a land's.
EV.good = function(item: string): { any }?
    for _, g in GOODS do
        if g[1] == item then
            return g
        end
    end
    for _, list in EV.TYPICAL do
        for _, g in list do
            if g[1] == item then
                return g
            end
        end
    end
    return nil
end

EV.value = function(c: Caravan): number
    local v = 0
    for item, n in c.goods do
        local g = EV.good(item)
        if g then
            v += math.ceil(n / g[2]) * g[3]
        end
    end
    return v
end

-- The way between two villages along the paths (mods/villages), a point
-- every 4 nodes; {} where the paths do not join them.
EV.got = ""
core.event.on("villages:route", function(ev)
    EV.got = ev.points
end)
EV.way = function(from: string, to: string): { { number } }
    local ax, az = string.match(from, "^(-?%d+),(-?%d+)$")
    local bx, bz = string.match(to, "^(-?%d+),(-?%d+)$")
    local out = {}
    if ax and bx then
        EV.got = ""
        core.event.emit("villages:route_ask", { rx1 = tonumber(ax) :: number, rz1 = tonumber(az) :: number,
                                               rx2 = tonumber(bx) :: number, rz2 = tonumber(bz) :: number })
        for x, z in string.gmatch(EV.got, "(-?%d+),(-?%d+)") do
            table.insert(out, { tonumber(x) :: number + 0.5, tonumber(z) :: number + 0.5 })
        end
    end
    return out
end
EV.route_of = function(c: Caravan): { { number } }
    if c.route == nil then
        local r = EV.way(c.from, c.to)
        -- On it from the point nearest where the caravan is.
        local best, bd = 1, math.huge
        for k, q in r do
            local d = (q[1] - c.x) ^ 2 + (q[2] - c.z) ^ 2
            if d < bd then
                best, bd = k, d
            end
        end
        c.route, c.k = r, best
    end
    return c.route :: { { number } }
end
-- Where a caravan heads: `ahead` points on along its route, or the
-- village's middle off the end of it (or with none).
EV.aim = function(c: Caravan, ahead: number): (number, number)
    local r = EV.route_of(c)
    while c.k <= #r and (r[c.k][1] - c.x) ^ 2 + (r[c.k][2] - c.z) ^ 2 < 36 do
        c.k += 1
    end
    if c.k > #r then
        local tx, tz = EV.middle(c.to)
        return tx or c.x, tz or c.z
    end
    local q = r[math.min(#r, c.k + ahead)]
    return q[1], q[2]
end
-- How far it has still to go, along its route.
EV.remaining = function(c: Caravan, d: number): number
    local r = c.route
    if r == nil or c.k > #r then
        return d
    end
    return (#r - c.k) * 4 + math.sqrt((r[c.k][1] - c.x) ^ 2 + (r[c.k][2] - c.z) ^ 2)
end

-- A caravan from a village near that nobody has been to, to this one (lived
-- in, a player near): true when one set off.
EV.incoming = function(rk: string): boolean
    for _, c in EV.caravans do
        if c.to == rk then
            return false
        end
    end
    local mx, mz = EV.middle(rk)
    if mx == nil or mz == nil then
        return false
    end
    local can = {}
    for other, k in E.known do
        local d = math.sqrt((k[1] - mx) ^ 2 + (k[2] - mz) ^ 2)
        if common.villages[other] == nil and d >= 100 and d < 900 then
            table.insert(can, other)
        end
    end
    while #can > 0 do
        local from = table.remove(can, math.random(#can))
        local r = EV.way(from, rk)
        if #r > 0 then
            local k = E.known[from]
            local h = core.terrain_height(k[1], k[2])
            local land = EV.TYPICAL[core.biome(k[1], k[2])] or (if h > 40 then EV.TYPICAL.high else EV.TYPICAL.other)
            local list = table.clone(land)
            local goods, things = {}, {}
            for _ = 1, 3 do
                if #list == 0 then
                    break
                end
                local g = table.remove(list, math.random(#list))
                if core.item_id(g[1]) ~= 0 then
                    goods[g[1]] = g[2]
                    table.insert(things, `{g[2]} {said(g[1])}`)
                end
            end
            if next(goods) == nil then
                return false
            end
            table.insert(EV.caravans, { from = from, to = rk, x = r[1][1], y = core.terrain_height(r[1][1], r[1][2]),
                                        z = r[1][2], goods = goods, m = 0, mule = 0, escort = "", best = math.huge,
                                        best_at = common.clock, att_at = 0, route = r, k = 1 })
            EV.save_caravans()
            table.sort(things)
            core.log(`villagers: a caravan sets off from the village at {k[1]} {k[2]} for {rk}, `
                .. `{#r * 4} nodes along the paths, with {table.concat(things, ", ")}`)
            return true
        end
    end
    return false
end

-- The way to a caravan's own village, told to a player: aloud, and on
-- their map.
EV.tell_way = function(p: number, c: Caravan)
    local fx, fz = EV.middle(c.from)
    local px, _, pz = core.player_pos(p)
    if fx == nil or fz == nil or px == nil or pz == nil then
        return
    end
    local dx, dz = fx - px, fz - pz
    local dirs = { "north", "north-east", "east", "south-east", "south", "south-west", "west", "north-west" }
    local dir = dirs[math.floor((math.atan2(dx, -dz) / (2 * math.pi) * 8 + 0.5)) % 8 + 1]
    local far = math.floor(math.sqrt(dx * dx + dz * dz))
    local back = EV.way(c.to, c.from)
    if #back > 0 then
        -- From where the player stands: the route's point nearest them on.
        local best, bd = 1, math.huge
        for k, q in back do
            local d = (q[1] - px) ^ 2 + (q[2] - pz) ^ 2
            if d < bd then
                best, bd = k, d
            end
        end
        local parts = { `{math.floor(px)},{math.floor(pz)}` }
        for k = best, #back do
            table.insert(parts, `{math.floor(back[k][1])},{math.floor(back[k][2])}`)
        end
        table.insert(parts, `{fx},{fz}`)
        core.event.emit("maps:route", { player = p, label = `village {c.from}`, points = table.concat(parts, ";") })
    end
    core.notice(p, `"Our village is {far} nodes to the {dir}, at {fx} {fz}. `
        .. (if #back > 0 then `Keep to the path: I have marked it on your map."` else `There is no path: go by the sun."`))
end

EV.bodies_off = function(c: Caravan)
    for id, r in EV.raiders do
        if r[4] == c then
            core.mob_remove(id)
            EV.raiders[id] = nil
        end
    end
    for _, id in { c.m, c.mule } do
        if id ~= 0 then
            core.mob_remove(id)
        end
    end
    c.m, c.mule = 0, 0
end

EV.arrive = function(i: number, c: Caravan, tx: number, tz: number): boolean
    local b, e = common.villages[c.to], extents[c.to]
    if b == nil and e ~= nil then
        b = { x1 = e[1], z1 = e[2], x2 = e[3], z2 = e[4], y = core.terrain_height(tx, tz), homes = {} }
    end
    if b == nil or core.loaded(tx, b.y, tz) ~= 1 then
        return false   -- waits there to be seen unloading
    end
    local chests = village_chests(c.to, b)
    local things = {}
    for item, n in c.goods do
        local left = put_into(chests, item, n)
        if left > 0 then
            core.drop_item(tx, b.y + 1, tz, core.item_id(item), left)
        end
        table.insert(things, `{n} {said(item)}`)
    end
    local value = EV.value(c)
    to_treasury(c.from, value)
    to_treasury(c.to, -value)
    local fx, fz = EV.middle(c.from)
    local pay = 2 + math.floor(math.sqrt(((fx or tx) - tx) ^ 2 + ((fz or tz) - tz) ^ 2) / 200)
    for _, p in core.players() do
        local px, _, pz = core.player_pos(p)
        if core.player_name(p) == c.escort and px and pz and (px - c.x) ^ 2 + (pz - c.z) ^ 2 < 40 * 40 then
            core.give_item(p, "ore:ruby", pay)
            to_treasury(c.from, -pay)
            core.notice(p, `The caravan is in: {ems(pay)} for the escort.`)
            E.credit(c.from, c.escort, 1)
        end
    end
    EV.bodies_off(c)
    table.remove(EV.caravans, i)
    EV.save_caravans()
    table.sort(things)
    EV.announce(c.to, `A caravan from the village at {fx} {fz} has come in, with {table.concat(things, ", ")}.`)
    return true
end

EV.lost = function(i: number, c: Caravan)
    for item, n in c.goods do
        core.drop_item(math.floor(c.x), math.floor(c.y) + 1, math.floor(c.z), core.item_id(item), n)
    end
    EV.bodies_off(c)
    table.remove(EV.caravans, i)
    EV.save_caravans()
    EV.announce(c.from, `The caravan was lost on the road, at {math.floor(c.x)} {math.floor(c.z)}: its goods lie there.`)
end

EV.seen = function(x: number, z: number, r: number): boolean
    for _, p in core.players() do
        local px, _, pz = core.player_pos(p)
        if px and pz and (px - x) ^ 2 + (pz - z) ^ 2 < r * r then
            return true
        end
    end
    return false
end

-- The next stretch of the road: a cell `far` ahead that a way on foot (no
-- swimming) reaches, straight on if it can, else turning off by up to 75
-- degrees either side -- round a lake, a cliff, a wood -- and the one
-- straight on, to be lifted over, when none does.
EV.onward = function(id: number, x: number, y: number, z: number, ux: number, uz: number, far: number)
    local fx, fy, fz = math.floor(x), math.floor(y + 0.01), math.floor(z)
    local first = nil
    for _, a in { 0, 0.45, -0.45, 0.9, -0.9, 1.3, -1.3 } do
        local cx, cz = ux * math.cos(a) - uz * math.sin(a), ux * math.sin(a) + uz * math.cos(a)
        local nx, nz = math.floor(x + cx * far), math.floor(z + cz * far)
        local ny = EV.ground(nx, fy, nz)
        if ny and math.abs(ny - fy) <= 5 then
            first = first or { nx, ny, nz }
            if #core.find_path(fx, fy, fz, nx, ny, nz, 2, 1, 3, false, 2000) > 0 then
                core.mob_goto(id, nx, ny, nz)
                return
            end
        end
    end
    if first then
        core.mob_goto(id, first[1], first[2], first[3])
    end
end

-- Every second: on along the road, walking when seen, carried on when not.
EV.roads = function(dt: number)
    for i = #EV.caravans, 1, -1 do
        local c = EV.caravans[i]
        local tx, tz = EV.middle(c.to)
        if tx == nil or tz == nil then
            table.remove(EV.caravans, i)
            continue
        end
        local d = math.sqrt((tx - c.x) ^ 2 + (tz - c.z) ^ 2)
        if d < 12 then
            EV.arrive(i, c, tx, tz)
            continue
        end
        -- Its heading: along its route, a little ahead while it walks.
        local hx, hz = EV.aim(c, if c.m ~= 0 then 2 else 0)
        local dx, dz = hx - c.x, hz - c.z
        local hd = math.max(0.01, math.sqrt(dx * dx + dz * dz))
        dx, dz = dx / hd, dz / hd
        local left = EV.remaining(c, d)
        if c.m ~= 0 then
            local x, y, z = core.mob_pos(c.m)
            if x == nil or y == nil or z == nil then
                if core.loaded(math.floor(c.x), math.floor(c.y), math.floor(c.z)) == 1 then
                    EV.lost(i, c)   -- killed
                else
                    EV.bodies_off(c)
                end
                continue
            end
            c.x, c.y, c.z = x, y, z
            if not EV.seen(x, z, 96) then
                EV.bodies_off(c)
                continue
            end
            if left < c.best - 1 then
                c.best, c.best_at = left, common.clock
            elseif common.clock - c.best_at > 12 then
                -- Stuck: over whatever is in the way.
                local nx, nz = math.floor(x + dx * 5), math.floor(z + dz * 5)
                local ny = EV.ground(nx, math.floor(y), nz)
                if ny then
                    core.mob_place(c.m, nx + 0.5, ny, nz + 0.5)
                    core.log(`villagers: a caravan to {c.to} lifted on at {nx} {ny} {nz}`)
                end
                c.best_at = common.clock
            end
            if core.mob_going(c.m) == 0 then
                -- On the path where it has one; else the way it can find.
                local gx, gz = math.floor(hx), math.floor(hz)
                local gy = if c.k <= #(c.route or {}) then EV.ground(gx, math.floor(y), gz) else nil
                if gy and math.abs(gy - y) <= 6 then
                    core.mob_goto(c.m, gx, gy, gz)
                else
                    EV.onward(c.m, x, y, z, dx, dz, math.min(hd, 10))
                end
            end
            if c.mule ~= 0 then
                local ux, uy, uz = core.mob_pos(c.mule)
                if ux == nil or uy == nil or uz == nil then
                    c.mule = 0
                elseif (ux - x) ^ 2 + (uz - z) ^ 2 > 3 * 3 and core.mob_going(c.mule) == 0 then
                    local bx, bz = math.floor(x - dx * 2), math.floor(z - dz * 2)
                    core.mob_goto(c.mule, bx, EV.ground(bx, math.floor(y), bz) or math.floor(y), bz)
                end
            end
            if is_night() and common.clock >= c.att_at and EV.up("caravan", c) == 0 then
                -- Two out of the dark every minute or two, once the last are down.
                c.att_at = common.clock + 60 + math.random() * 60
                local put = 0
                for _ = 1, 8 do
                    local a = math.random() * 2 * math.pi
                    local rx, rz = math.floor(x + math.cos(a) * 14), math.floor(z + math.sin(a) * 14)
                    local ry = EV.ground(rx, math.floor(y), rz)
                    if ry and put < 2 then
                        EV.raider("villagers:raider", rx, ry, rz, "caravan", c)
                        put += 1
                    end
                end
            end
        else
            local step = math.min(hd, 2.2 * dt)
            c.x += dx * step
            c.z += dz * step
            local x, z = math.floor(c.x), math.floor(c.z)
            if EV.seen(c.x, c.z, 64) and core.loaded(x, math.floor(c.y), z) == 1 then
                local y = EV.ground(x, math.floor(c.y), z)
                if y == nil then
                    local h = core.terrain_height(x, z)
                    y = EV.ground(x, h + 1, z)
                end
                if y then
                    c.y = y
                    c.m = core.spawn_mob("villagers:merchant", x + 0.5, y, z + 0.5)
                    local bx, bz = math.floor(c.x - dx * 2), math.floor(c.z - dz * 2)
                    c.mule = core.spawn_mob("villagers:mule", bx + 0.5, EV.ground(bx, y, bz) or y, bz + 0.5)
                    c.best, c.best_at = left, common.clock
                end
            end
        end
    end
end

-- The caravan's panel: its cargo to buy, at the market's price, and the escort.
EV.show = function(p: number, c: Caravan)
    local rows = {}
    for item, n in c.goods do
        local g = EV.good(item)
        if g and n >= g[2] then
            table.insert(rows, { g, price_of(g, n) })
        end
    end
    local name = panel_of(p, "caravan")
    local tx, tz = EV.middle(c.to)
    local fx, fz = EV.middle(c.from)
    core.ui_define(name, 15, 7.6 + #rows * 1.2)
    core.ui_label(name, 0.4, 0.3, `A caravan from the village at {fx} {fz}, on its way to the one at {tx} {tz}.`)
    for i, r in rows do
        core.ui_button(name, `b {i}`, 0.4, 0.2 + i * 1.2, 14.2, 1.1, `Buy {r[1][2]} {said(r[1][1])}: {ems(r[2])}`)
    end
    local who = core.player_name(p)
    local y = 1.6 + #rows * 1.2
    if c.escort == who then
        core.ui_label(name, 0.4, y + 0.2, "You are its escort: stay with it to the end of the road.")
    elseif c.escort ~= "" then
        core.ui_label(name, 0.4, y + 0.2, `{c.escort} escorts it.`)
    else
        core.ui_button(name, "escort", 0.4, y, 14.2, 1.1, "Escort it (paid when it gets there)")
    end
    core.ui_button(name, "way", 0.4, y + 1.4, 14.2, 1.1, "Ask the way to its village")
    core.ui_button(name, "news", 0.4, y + 2.8, 14.2, 1.1, "News from the road")
    core.ui_button(name, "close", 0.4, y + 4.2, 14.2, 1.1, "Close")
    EV.talk[p] = c
    EV.rows = EV.rows or {}
    EV.rows[p] = rows
    core.ui_show(name, true, p)
end

core.event.on("core:used_mob", function(ev)
    for _, c in EV.caravans do
        if ev.target ~= 0 and (ev.target == c.m or ev.target == c.mule) then
            EV.show(ev.player, c)
            return
        end
    end
end)

core.event.on("core:ui_click", function(ev)
    if ev.panel ~= panel_of(ev.player, "caravan") then
        return
    end
    core.ui_show(ev.panel, false, ev.player)
    local c = EV.talk[ev.player]
    if c == nil or table.find(EV.caravans, c) == nil then
        return
    end
    if ev.id == "way" then
        EV.tell_way(ev.player, c)
        return
    end
    if ev.id == "news" then
        local fx, fz = EV.middle(c.from)
        local s = if fx and fz then AID.pt.near(fx, fz, 800, ev.player)[1] else nil
        core.notice(ev.player, if s and fx and fz then `"{AID.pt.rumour(s, fx, fz, "our village")}"`
            else "\"Nothing new on the road. Wolves, rain.\"")
        return
    end
    if ev.id == "escort" and c.escort == "" then
        c.escort = core.player_name(ev.player)
        EV.save_caravans()
        core.notice(ev.player, "You escort the caravan: stay with it to the end of the road.")
        return
    end
    local i = tonumber(string.match(ev.id, "^b (%d+)$") or "")
    local r = i and EV.rows[ev.player] and EV.rows[ev.player][i]
    if r == nil or (c.goods[r[1][1]] or 0) < r[1][2] then
        return
    end
    local got = core.take_item(ev.player, "ore:ruby", r[2])
    if got < r[2] then
        if got > 0 then
            core.give_item(ev.player, "ore:ruby", got)
        end
        core.notice(ev.player, `That is {ems(r[2])}.`)
        return
    end
    core.give_item(ev.player, r[1][1], r[1][2])
    c.goods[r[1][1]] -= r[1][2]
    if c.goods[r[1][1]] <= 0 then
        c.goods[r[1][1]] = nil
    end
    to_treasury(c.from, r[2])
    EV.save_caravans()
    EV.show(ev.player, c)
end)

-- ------------------------------------------------------------ the clock

core.command("village", "siege|feast|caravan|incoming -- start one now in the village you stand in", "admin")
core.event.on("core:command", function(ev)
    if ev.name ~= "village" then
        return
    end
    local x, _, z = core.player_pos(ev.player)
    local rk = x and z and village_at(x, z)
    local what = string.match(ev.args, "^%s*(%a+)")
    if rk == nil then
        core.chat(ev.player, "Not in a village.")
    elseif what == "siege" and core.peaceful() then
        core.chat(ev.player, "This world is peaceful: no siege.")
    elseif what == "siege" then
        EV.begin_siege(rk)
    elseif what == "feast" then
        EV.begin_feast(rk)
    elseif what == "caravan" then
        if not EV.send(rk) then
            core.chat(ev.player, "No caravan: none to go to, one out already, or nothing to spare.")
        end
    elseif what == "incoming" then
        if not EV.incoming(rk) then
            core.chat(ev.player, "No caravan: no village near unvisited and joined by the paths, or one coming already.")
        end
    else
        core.chat(ev.player, "Usage: /village siege|feast|caravan|incoming")
    end
end)

local every, sent = 0.0, 0.0
core.event.on("core:tick", function(ev)
    every += ev.dt
    if every < 1 then
        return
    end
    local dt = every
    every = 0
    local t = core.time_of_day()
    local today = math.floor(age() / DAY)
    for rk in common.villages do
        local s = EV.siege[rk]
        if s and core.peaceful() then
            s.started = false   -- made peaceful while it lasts: it ends, unpaid and untold
            EV.end_siege(rk, s)
        elseif s then
            EV.run_siege(rk, s)
        elseif t > 0.70 and t < 0.78 and EV.asked[rk] ~= today then
            EV.asked[rk] = today
            local last = tonumber(core.storage_get(`siege {rk}`)) or 0
            if #EV.near(rk, 32) > 0 and age() - last >= 7 * DAY and math.random(3) == 1 then
                EV.begin_siege(rk)
            end
        end
        local f = EV.feast[rk]
        if f then
            EV.run_feast(rk, f)
        elseif common.season == "summer" and EV.left == 0 and t > 0.3 and t < 0.7
            and core.storage_get(`feast {rk}`) ~= tostring(today) and #EV.near(rk, 64) > 0 then
            EV.begin_feast(rk)
        end
    end
    for id, r in EV.raiders do
        if core.peaceful() then
            core.mob_remove(id)
            EV.raiders[id] = nil
        else
            EV.steer(id, r)
        end
    end
    EV.roads(dt)
    if common.clock >= sent then
        sent = common.clock + 60
        for rk in common.villages do
            if #EV.near(rk, 64) > 0 then
                if math.random(20) == 1 then
                    EV.send(rk)
                elseif math.random(20) == 1 then
                    EV.incoming(rk)
                end
            end
        end
    end
end)
end
