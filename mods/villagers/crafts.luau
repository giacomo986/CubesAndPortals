--!strict
-- villagers: supplies, the crafter and the builder at work. Part of the mod (init.luau says the order).

local common = require("./common")
local types = require("./types")
type V = types.V
type Order = types.Order
local act = common.act
local add = common.add
local AIR = common.AIR
local ask = common.ask
local at = common.at
local BRINGS = common.BRINGS
local cost_of = common.cost_of
local deliver = common.deliver
local failed = common.failed
local food_of = common.food_of
local FOODS = common.FOODS
local has_kind = common.has_kind
local house_of = common.house_of
local houses = common.houses
local HUNGRY_EVERY = common.HUNGRY_EVERY
local id = common.id
local is_in = common.is_in
local is_night = common.is_night
local ITEMS = common.ITEMS
local LEAF_NAMES = common.LEAF_NAMES
local LOG_NAMES = common.LOG_NAMES
local MATS = common.MATS
local NATURAL = common.NATURAL
local nearest = common.nearest
local orders = common.orders
local patrol = common.patrol
local plan_of = common.plan_of
local pockets_here = common.pockets_here
local put_into = common.put_into
local reach = common.reach
local rest = common.rest
local ROAD_BLOCKS = common.ROAD_BLOCKS
local save_houses = common.save_houses
local save_orders = common.save_orders
local sleeps_now = common.sleeps_now
local standable = common.standable
local stock_of = common.stock_of
local take_from = common.take_from
local TOOL = common.TOOL
local TOOLING = common.TOOLING
local tree_log = common.tree_log
local village_at = common.village_at
local village_chests = common.village_chests
local wait = common.wait
local walk = common.walk
local wanting = common.wanting

common.supply = function(v: V): boolean
    if common.clock - (v.supplied or -999) < 30 then
        return false
    end
    v.supplied = common.clock
    local want: { { any } } = {}
    -- Every tool of its trade it might find, best first; it takes one.
    local ladder = TOOLING.ladder[v.kind] or {}
    if TOOL[v.kind] ~= nil and TOOLING.of(v) == nil then
        for _, t in ladder do
            table.insert(want, { t, 1 })
        end
    end
    if v.kind == "miner" and (v.inv["torches:torch"] or 0) < 4 then
        table.insert(want, { "torches:torch", 12 })
    end
    local hungry = common.clock - v.fed_at > HUNGRY_EVERY and food_of(v) == nil
    if hungry then
        for _, f in FOODS do
            table.insert(want, { f, 3 })
        end
    end
    local rk = if #want > 0 then village_at(v.hx, v.hz) else nil
    if rk == nil then
        return false
    end
    -- The nearest chest holding any of it.
    local px, _, pz = core.mob_pos(v.id)
    local best, bc = math.huge, nil
    for _, c in village_chests(rk) do
        for _, w in want do
            local iid = core.item_id(w[1])
            if iid ~= 0 and core.container_count(c[1], c[2], c[3], iid) > 0 then
                local d = (c[1] - (px or v.hx)) ^ 2 + (c[3] - (pz or v.hz)) ^ 2
                if d < best then
                    best, bc = d, c
                end
            end
        end
    end
    if bc == nil or not reach(v, bc[1], bc[2], bc[3]) then
        return false
    end
    act(v, "gather", "", 1.6, bc[1], bc[3])
    local got, fed, tooled = {}, false, false
    for _, w in want do
        local food = table.find(FOODS, w[1]) ~= nil
        local tool = table.find(ladder, w[1]) ~= nil
        if not (food and fed) and not (tool and tooled) then
            local n = take_from({ bc }, w[1], w[2])
            if n > 0 then
                add(v, w[1], n)
                table.insert(got, `{n} {w[1]}`)
                fed = fed or food
                if tool then
                    tooled = true
                    v.uses = 0
                end
            end
        end
    end
    if #got > 0 then
        core.log(`villagers: the {v.kind} at {v.key} took {table.concat(got, ", ")} from the village's chests`)
    end
    return true
end

-- What the CRAFTER makes for the village, and of what: the tools its
-- workers wear out and torches for its miners, two of each tool (16
-- torches) kept in its chests for whoever comes for them, only for the
-- kinds the village has. Sticks it makes of planks, planks of logs.
local MAKES = {
    -- Iron first, when the chests hold the ingots; stone otherwise. Two of a
    -- kind is two of any material (stocked, below).
    { "furnace:iron_pick", 1, { ["furnace:iron_ingot"] = 3, ["timber:stick"] = 2 }, "miner" },
    { "furnace:iron_axe", 1, { ["furnace:iron_ingot"] = 3, ["timber:stick"] = 2 }, "woodcutter" },
    { "furnace:iron_shovel", 1, { ["furnace:iron_ingot"] = 1, ["timber:stick"] = 2 }, "digger" },
    { "furnace:iron_hoe", 1, { ["furnace:iron_ingot"] = 2, ["timber:stick"] = 2 }, "human" },
    { "tools:pick", 1, { ["rocks:cobblestone"] = 3, ["timber:stick"] = 2 }, "miner" },
    { "tools:axe", 1, { ["rocks:cobblestone"] = 3, ["timber:stick"] = 2 }, "woodcutter" },
    { "tools:shovel", 1, { ["rocks:cobblestone"] = 1, ["timber:stick"] = 2 }, "digger" },
    { "farm:hoe", 1, { ["timber:planks"] = 2, ["timber:stick"] = 2 }, "human" },
    { "torches:torch", 4, { ["ore:coal"] = 1, ["timber:stick"] = 1 }, "miner" },
}
local KEEP_MADE = { ["torches:torch"] = 16 }

-- n of planks or sticks in the chests, made of what is there when short.
local function enough(chests: { { number } }, item: string, n: number): boolean
    while stock_of(chests, item) < n do
        if item == "timber:stick" then
            if not enough(chests, "timber:planks", 2) then
                return false
            end
            take_from(chests, "timber:planks", 2)
            put_into(chests, "timber:stick", 4)
        elseif item == "timber:planks" then
            local log = nil
            for _, l in LOG_NAMES do
                if stock_of(chests, l) > 0 then
                    log = l
                    break
                end
            end
            if log == nil then
                return false
            end
            take_from(chests, log, 1)
            put_into(chests, "timber:planks", 4)
        else
            return false
        end
    end
    return true
end

common.crafter_life = function(v: V)
    local rk = v.rk :: string
    while true do
        if sleeps_now(v) and rest(v) then
            continue
        end
        local chests = village_chests(rk)
        local made = false
        for _, m in MAKES do
            local ladder = TOOLING.ladder[m[4]]
            local stocked = 0
            if ladder ~= nil and table.find(ladder, m[1]) then
                for _, t in ladder do
                    stocked += stock_of(chests, t)
                end
            else
                stocked = stock_of(chests, m[1])
            end
            if #chests > 0 and has_kind(rk, m[4]) and stocked < (KEEP_MADE[m[1]] or 2) then
                local ok = true
                for item, n in m[3] do
                    ok = ok and enough(chests, item, n)
                end
                if ok then
                    -- At the anvil or a crafting table near its post.
                    local wx, wy, wz = nearest({ "building:anvil", "craft:table" }, v.hx, v.hy, v.hz, 24, 6)
                    if wx ~= nil and wy ~= nil and wz ~= nil then
                        reach(v, wx, wy, wz)
                    end
                    act(v, "gather", m[1], 3.0, wx, wz)
                    for item, n in m[3] do
                        take_from(chests, item, n)
                    end
                    local left = put_into(chests, m[1], m[2])
                    if left > 0 then
                        local px, py, pz = core.mob_pos(v.id)
                        core.drop_item(math.floor(px or v.hx), math.floor(py or v.hy), math.floor(pz or v.hz),
                            core.item_id(m[1]), left)
                    end
                    core.log(`villagers: {rk}: the crafter made {m[2]} {m[1]}`)
                    made = true
                    break
                end
            end
        end
        if not made then
            patrol(v)
            wait(v, 15)
        end
        wait(v, 0.5)
    end
end

-- Near enough a cell to work on it: within 5, or one walk to the standable
-- cell nearest it; where there is no way there (up a wall it is raising,
-- shut in by it), up its scaffolding: put there.
local function near(v: V, x: number, y: number, z: number): boolean
    local px, py, pz = core.mob_pos(v.id)
    if px == nil or py == nil or pz == nil or ((px - x - 0.5) ^ 2 + (pz - z - 0.5) ^ 2 < 36 and math.abs(py - y) < 6) then
        return true
    end
    local best, bx, by, bz = math.huge, nil, nil, nil
    for dx = -3, 3 do
        for dz = -3, 3 do
            for dy = 3, -4, -1 do
                local sx, sy, sz = x + dx, y + dy, z + dz
                local d = dx * dx + dz * dz + dy * dy * 0.5
                if d < best and standable(sx, sy, sz) and not failed(v, sx, sy, sz) then
                    best, bx, by, bz = d, sx, sy, sz
                end
            end
        end
    end
    if bx == nil or by == nil or bz == nil then
        return false
    end
    if not walk(v, bx, by, bz, 6) then
        core.mob_place(v.id, bx + 0.5, by + 0.02, bz + 0.5)
    end
    return true
end

local function build(v: V, rk: string, o: Order)
    -- A cell's last block in the plan only: a wall cleared and then put
    -- again was dug out and rebuilt on every build taken up again (a
    -- restart), and paid for twice.
    local plan = plan_of(o)
    local final: { [string]: number } = {}
    for i, p in plan do
        final[`{p[1]},{p[2]},{p[3]}`] = i
    end
    for i, p in plan do
        local x, y, z, name, facing = p[1], p[2], p[3], p[4], p[5]
        if final[`{x},{y},{z}`] ~= i then
            continue
        end
        local want = if name == "" then AIR else id(name)
        local now = core.get_block(x, y, z)
        -- A way does not go through what somebody built: it steps round.
        local built = p[6] and now ~= AIR and core.node_solid(now) == 1 and not is_in("natural", NATURAL, now)
            and not tree_log(x, y, z) and not is_in("leaves", LEAF_NAMES, now)
            and not is_in("road", ROAD_BLOCKS, now)
        if now ~= want and not built then
            near(v, x, y, z)
            local c = cost_of(name)
            if c ~= nil then
                o.have[c[1]] = math.max(0, (o.have[c[1]] or 0) - c[2])
            end
            act(v, if name == "" then "dig" else "chop", if name == "" then "" else name, 0.25, x, z)
            core.set_block(x, y, z, want)
            if facing ~= nil then
                core.set_facing(x, y, z, facing)
            end
            if name == "chest:box" then
                core.container_make(x, y, z, 27)
            end
        end
    end
end

-- How a kind of thing is fetched, by whoever fetches it (set with the gatherers).
local GATHER: { [string]: (V) -> () } = {}

common.builder_life = function(v: V)
    local rk = v.rk :: string
    -- The village's own order: out of its chests first, what they hold of
    -- what it lacks, every minute or so as they fill. Its people's goods
    -- are the village's (no gatherer brought a block to the hall's keepers'
    -- houses in fifteen minutes: there was none in the village).
    local chests_at = 0.0
    local function from_chests(o: Order, lack: { [string]: number }): boolean
        if o.buyer ~= "" or common.clock < chests_at then
            return false
        end
        chests_at = common.clock + 60
        local chests = village_chests(rk)
        local got = 0
        for m, n in lack do
            for _, item in ITEMS[m] or {} do
                local want = n - got
                if want <= 0 then
                    break
                end
                if stock_of(chests, item) > 0 then
                    local c = chests[1]
                    reach(v, c[1], c[2], c[3])
                    local took = take_from(chests, item, math.min(want, 64))
                    if took > 0 then
                        got += deliver(o, m, took)
                        core.log(`villagers: {rk}: the builder took {took} {item} from the village's chests`)
                    end
                end
            end
            got = 0
        end
        save_orders(rk)
        return true
    end
    while true do
        local o = (orders[rk] or {})[1]
        if o == nil or is_night() then
            if is_night() then
                if not rest(v) then
                    walk(v, v.hx, v.hy, v.hz, 40)
                    wait(v, 10)
                end
            else
                patrol(v)
            end
        elseif o.stage == "gather" then
            local _, lack = wanting(rk)
            if next(lack) == nil then
                core.log(`villagers: {rk}: building {if o.buyer == "" then "the village" else o.buyer}'s {o.kind}`)
                o.stage = "build"
                save_orders(rk)
            elseif from_chests(o, lack) then
                -- (taken what there was; looked at again next time round)
            elseif o.self or o.buyer == "" then
                -- Told to find it itself: it goes for what no gatherer of
                -- the village fetches first, then helps with the rest.
                local m = next(lack) :: string
                for _, k in MATS do
                    if lack[k] ~= nil and not has_kind(rk, BRINGS[k]) then
                        m = k
                        break
                    end
                end
                v.task = m
                GATHER[m](v)
            else
                local nobody = false
                for m in lack do
                    if not has_kind(rk, BRINGS[m]) then
                        nobody = true
                    end
                end
                if (nobody or common.clock - o.since > 120) and common.clock - o.asked > 60 then
                    o.asked = common.clock
                    ask(o, v, lack)
                end
                wait(v, 3)
            end
        else
            build(v, rk, o)
            core.log(`villagers: {rk}: {if o.buyer == "" then "the village" else o.buyer}'s {o.kind} built`)
            if o.kind == "house" and o.style == "pocket" and pockets_here() then
                -- Its doorway, at the front (local z 0), facing out of it.
                local dx, dz = at(o, o.w // 2, 0)
                local axis, sign = o.t % 2, if o.t <= 1 then -1 else 1
                core.event.emit("pocket:put", { x = dx, y = o.floor, z = dz, axis = axis, sign = sign,
                                                owner = "name:" .. o.buyer })
            end
            if o.kind == "house" then
                houses[rk] = houses[rk] or {}
                table.insert(houses[rk], { x0 = o.x0, z0 = o.z0, t = o.t, floor = o.floor, w = o.w, d = o.d,
                    style = o.style, storeys = 1, garden = false, buyer = o.buyer })
                save_houses(rk)
            elseif o.kind == "floor" or o.kind == "garden" then
                local h = house_of(rk, o.buyer)
                if h ~= nil then
                    if o.kind == "floor" then
                        h.storeys = 2
                    else
                        h.garden = true
                    end
                    save_houses(rk)
                end
            end
            table.remove(orders[rk], 1)
            save_orders(rk)
            for _, p in core.players() do
                if core.player_name(p) == o.buyer then
                    local cx, cz = at(o, o.w // 2, o.d // 2)
                    core.notice(p, ({ house = if o.style == "pocket" then `Your pocket hut at {cx} {cz} is ready: walk in.`
                            else `Your house at {cx} {cz} is ready.`,
                        floor = "Your house's second storey is ready.", garden = "Your garden is sown.",
                        lot = `Your lot at {cx} {cz} is ready to build on.` })[o.kind] or "Done.")
                end
            end
        end
        wait(v, 0.5)
    end
end

-- What the files after this one use of it.
common.GATHER = GATHER
