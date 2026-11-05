--!strict
-- arena: a scored fight between players, in a ring round a GONG.
--
-- A gong (gold round iron) stands anywhere; one is put up once by the
-- spawn's square, beside the chess table. Right click it: the standing --
-- rounds won and knock-downs, kept for good -- and, while no round is on,
-- "Strike the gong": a round of three minutes begins for everybody in the
-- ring (10 round the gong, 6 up or down) and whoever walks in after.
--
-- In a round a player knocked down by another inside the ring is the
-- other's point; the engine puts the fallen back on their feet with their
-- bag (it is never spilled) and this mod puts them back on the ring's edge.
-- Every half minute the score is said to those in the ring; at the end the
-- winner is named to everybody near, and the standing grows.
--
-- On the screen: the ring's edge drawn on the ground round every gong near,
-- faint, bright while a round is on; and for whoever is in the ring during a
-- round, the time left and the score over the view, every second.
--
-- The same panel strikes a PROP HUNT, in a wider ring (16 round the gong), for
-- two players or more in it: one in four seeks (at least one), the others hide
-- as a block (core.body_block). A hider is first the block under their feet and
-- right-clicks any solid block to become that one; standing still, they settle
-- square into the grid, and their camera stands back (core.view) so they see
-- what they look like. For 30 s the seekers wait by the gong, blind and still;
-- then three minutes in all to find them. A seeker's blow on a hider finds them
-- (nobody is hurt in a hunt) and the found seek too. A block dug at in the
-- ring is only a block: nothing is broken or built there during a hunt. All
-- found, the seekers win; at the end, whoever is still hidden. A hider who
-- walks out of the ring is put back on its edge. The standing keeps, apart
-- from the fights', how many each found and how many hunts each lasted.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"
local RING, UP = 10, 6
-- This machine's: gongs near ("x,y,z" -> {x, y, z, on, when}) and the line shown.
local near_gongs: { [string]: { number } } = {}
local shown = ""
local clock = 0.0
local blind = false        -- a seeker waiting, in a hunt
local viewed: number? = nil  -- the camera's distance before a hider's stood it back
local heard: (string) -> () = function(_) end

if serving then
    core.net.open()

    local GONG = core.register_node("arena:gong", 200, 160, 60, true, 4, "upright", 0, 3.0, "stone")
    core.node_box(GONG, 0.1, 0, 0.4, 0.2, 1, 0.6)
    core.node_box(GONG, 0.8, 0, 0.4, 0.9, 1, 0.6)
    core.node_box(GONG, 0.1, 0.9, 0.4, 0.9, 1, 0.6)
    core.node_box(GONG, 0.22, 0.2, 0.46, 0.78, 0.76, 0.54)
    core.register_model("arena:gong_held", "gong_held.gltf")   -- tools/make_held_models.py
    core.recipe_shaped("arena:gong", 1,
        "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot",
        "furnace:iron_ingot furnace:gold_ingot furnace:iron_ingot",
        "furnace:iron_ingot -                  furnace:iron_ingot")

    local ROUND = 180   -- seconds

    type Round = { x: number, y: number, z: number, left: number, said: number, score: { [string]: number } }
    local rounds: { [string]: Round } = {}
    local back: { { any } } = {}   -- the fallen, to be put on the ring's edge: player, round, ticks

    -- The prop hunt's.
    local HUNT, HIDE, FIELD = 180, 30, 16
    local STONE = core.node_id("core:stone")
    type Hunt = { x: number, y: number, z: number, left: number, hiding: number,
                  seekers: { [number]: boolean }, hiders: { [number]: number },
                  found: { [string]: number }, everyone: { [number]: boolean },
                  still: { [number]: { number } } }
    local hunts: { [string]: Hunt } = {}

    -- The standing: "name=wins,kills;..."
    local function standing(): { [string]: { number } }
        local out = {}
        for part in string.gmatch(core.storage_get("standing"), "[^;]+") do
            local n, w, k = string.match(part, "^(.+)=(%d+),(%d+)$")
            if n then
                out[n] = { tonumber(w) :: number, tonumber(k) :: number }
            end
        end
        return out
    end
    local function keep(t: { [string]: { number } })
        local parts = {}
        for n, v in t do
            table.insert(parts, `{n}={v[1]},{v[2]}`)
        end
        core.storage_set("standing", table.concat(parts, ";"))
    end

    local function inside(p: number, r: Round): boolean
        local x, y, z = core.player_pos(p)
        return x ~= nil and (x - r.x - 0.5) ^ 2 + (z - r.z - 0.5) ^ 2 <= RING * RING and math.abs(y - r.y) <= UP
    end

    local function in_field(p: number, h: Hunt): boolean
        local x, y, z = core.player_pos(p)
        return x ~= nil and (x - h.x - 0.5) ^ 2 + (z - h.z - 0.5) ^ 2 <= FIELD * FIELD and math.abs(y - h.y) <= UP
    end

    -- The hunt a player is in, if any.
    local function hunt_of(p: number): Hunt?
        for _, h in hunts do
            if h.everyone[p] then
                return h
            end
        end
        return nil
    end

    -- The hunt's own standing: "name=found,lasted;..."
    local function hunt_standing(): { [string]: { number } }
        local out = {}
        for part in string.gmatch(core.storage_get("hunt"), "[^;]+") do
            local n, f, l = string.match(part, "^(.+)=(%d+),(%d+)$")
            if n then
                out[n] = { tonumber(f) :: number, tonumber(l) :: number }
            end
        end
        return out
    end
    local function keep_hunt(t: { [string]: { number } })
        local parts = {}
        for n, v in t do
            table.insert(parts, `{n}={v[1]},{v[2]}`)
        end
        core.storage_set("hunt", table.concat(parts, ";"))
    end

    local function in_ring(r: Round): { number }
        local out = {}
        for _, p in core.players() do
            if inside(p, r) then
                table.insert(out, p)
            end
        end
        return out
    end

    local function said(r: Round): string
        local list = {}
        for n, k in r.score do
            table.insert(list, { n, k })
        end
        table.sort(list, function(a, b) return a[2] > b[2] end)
        local parts = {}
        for i, e in list do
            if i > 5 then
                break
            end
            table.insert(parts, `{e[1]} {e[2]}`)
        end
        return if #parts > 0 then table.concat(parts, ", ") else "nobody yet"
    end

    local function show(p: number, key: string)
        local name = `arena:panel{p}`
        local t = standing()
        local list = {}
        for n, v in t do
            table.insert(list, { n, v[1], v[2] })
        end
        table.sort(list, function(a, b) return a[2] > b[2] or (a[2] == b[2] and a[3] > b[3]) end)
        local rows = math.min(#list, 8)
        -- And the hunt's best: who found most, who lasted most.
        local ht, finders, lasters = hunt_standing(), {}, {}
        for n, v in ht do
            if v[1] > 0 then
                table.insert(finders, { n, v[1] })
            end
            if v[2] > 0 then
                table.insert(lasters, { n, v[2] })
            end
        end
        local function best(l: { { any } }): string
            table.sort(l, function(a, b) return a[2] > b[2] end)
            local parts = {}
            for i = 1, math.min(#l, 3) do
                table.insert(parts, `{l[i][1]} {l[i][2]}`)
            end
            return if #parts > 0 then table.concat(parts, ", ") else "nobody yet"
        end
        core.ui_define(name, 12, 7.3 + math.max(rows, 1) * 0.8)
        core.ui_label(name, 0.4, 0.3, "The arena's standing: rounds won, knock-downs.")
        for i = 1, rows do
            core.ui_label(name, 0.6, 0.4 + i * 0.8, `{i}. {list[i][1]}: {list[i][2]} won, {list[i][3]} down`)
        end
        if rows == 0 then
            core.ui_label(name, 0.6, 1.2, "Nobody has fought here yet.")
        end
        local y = 1.4 + math.max(rows, 1) * 0.8
        core.ui_label(name, 0.4, y, `Prop hunt, found most: {best(finders)}`)
        core.ui_label(name, 0.4, y + 0.7, `Never found most: {best(lasters)}`)
        y += 1.5
        local r, h = rounds[key], hunts[key]
        if r then
            core.ui_label(name, 0.4, y, `A round is on: {math.ceil(r.left)} s left -- {said(r)}`)
        elseif h then
            local hidden = 0
            for _ in h.hiders do
                hidden += 1
            end
            core.ui_label(name, 0.4, y, `A prop hunt is on: {math.ceil(h.left)} s left, {hidden} still hidden`)
        else
            core.ui_button(name, `start {key}`, 0.4, y, 11.2, 1.1, "Strike the gong: a round of three minutes")
            core.ui_button(name, `hunt {key}`, 0.4, y + 1.3, 11.2, 1.1, "Prop hunt: hide as a block, or find who did")
        end
        y += 1.3
        core.ui_button(name, "close", 0.4, y + 1.3, 11.2, 1.0, "Close")
        core.ui_show(name, true, p)
    end

    -- Where the gongs are, for drawing their rings: "x,y,z;..."
    local gongs: { [string]: boolean } = {}
    for k in string.gmatch(core.storage_get("gongs"), "[^;]+") do
        gongs[k] = true
    end
    local function keep_gongs()
        local list = {}
        for k in gongs do
            table.insert(list, k)
        end
        core.storage_set("gongs", table.concat(list, ";"))
    end
    core.event.on("core:placed", function(ev)
        if ev.id == GONG then
            gongs[`{ev.x},{ev.y},{ev.z}`] = true
            keep_gongs()
        end
    end)
    core.event.on("core:broken", function(ev)
        if ev.id == GONG then
            gongs[`{ev.x},{ev.y},{ev.z}`] = nil
            keep_gongs()
        end
    end)

    local function send(p: number, text: string)
        if drawing then
            heard(text)
        else
            core.net.send(p, text)
        end
    end

    -- A hider takes the look of a block; a seeker is told it is only a block.
    local disguise: (h: Hunt, p: number, id: number) -> ()
    local start_hunt: (key: string, by: number) -> ()
    -- Who is in a hunt, as what. A hider is drawn as a block, held square
    -- (a clip of nothing, faced south), their camera stood back on their own
    -- machine; all of them take no blows.
    disguise = function(h: Hunt, p: number, id: number)
        h.hiders[p] = id
        core.body_block(p, id)
        core.player_clip(p, "arena_prop", 0, 1)
        send(p, `d {id}`)
    end
    local function undisguise(p: number)
        core.body_block(p, 0)
        core.player_clip(p, "")
        send(p, "d 0")
    end
    local function make_seeker(h: Hunt, p: number)
        if h.hiders[p] then
            undisguise(p)
            h.hiders[p] = nil
        end
        h.still[p] = nil
        h.seekers[p] = true
        if h.hiding > 0 then
            core.body_movement(p, 0, 0, 0, 0)
            send(p, "b 1")
        end
    end
    local function release(h: Hunt, p: number)
        if h.hiders[p] then
            undisguise(p)
        end
        if h.seekers[p] and h.hiding > 0 then
            core.body_movement(p)
            send(p, "b 0")
        end
        core.player_factors(p, 1, 1, 1, 1, "arena:hunt")
        h.everyone[p] = nil
        h.hiders[p] = nil
        h.seekers[p] = nil
        h.still[p] = nil
    end
    local function near(x: number, z: number, line: string)
        for _, p in core.players() do
            local px, _, pz = core.player_pos(p)
            if px and (px - x) ^ 2 + (pz - z) ^ 2 < 48 * 48 then
                core.notice(p, line)
            end
        end
    end

    start_hunt = function(key: string, by: number)
        if rounds[key] or hunts[key] then
            return
        end
        local x, y, z = string.match(key, "^(%-?%d+),(%-?%d+),(%-?%d+)$")
        local h: Hunt = { x = tonumber(x) :: number, y = tonumber(y) :: number, z = tonumber(z) :: number,
                          left = HUNT, hiding = HIDE, seekers = {}, hiders = {}, found = {}, everyone = {},
                          still = {} }
        local list = {}
        for _, p in core.players() do
            if in_field(p, h) and hunt_of(p) == nil then
                table.insert(list, p)
            end
        end
        if #list < 2 then
            core.notice(by, `A prop hunt needs two players at least within {FIELD} of the gong.`)
            return
        end
        for i = #list, 2, -1 do
            local j = math.random(1, i)
            list[i], list[j] = list[j], list[i]
        end
        hunts[key] = h
        local seekers = math.max(1, math.floor(#list / 4))
        for i, p in list do
            h.everyone[p] = true
            core.player_factors(p, 1, 1, 0, 1, "arena:hunt")
            if i <= seekers then
                make_seeker(h, p)
                core.notice(p, `You seek. Wait {HIDE} s by the gong, then find the blocks that are not blocks: a blow finds them.`)
            else
                local px, py, pz = core.player_pos(p)
                local under = core.get_block(math.floor(px :: number), math.floor((py :: number) - 0.5), math.floor(pz :: number))
                disguise(h, p, if core.node_solid(under) == 1 and under ~= GONG then under else STONE)
                core.notice(p, `You hide: {HIDE} s before they look. Right-click a block to become one like it; stand still.`)
            end
        end
        core.play_sound("arena:gong", h.x + 0.5, h.y + 0.5, h.z + 0.5)
        near(h.x, h.z, `The gong: a prop hunt round it, {#list - seekers} hiding, {seekers} seeking!`)
        core.log(`arena: a prop hunt at {key}, {#list} players, struck by {core.player_name(by)}`)
    end

    local function end_hunt(key: string, h: Hunt)
        hunts[key] = nil
        local t = hunt_standing()
        local lasted = {}
        for p in h.hiders do
            local n = core.player_name(p)
            if n ~= "" then
                table.insert(lasted, n)
                t[n] = t[n] or { 0, 0 }
                t[n][2] += 1
            end
        end
        for n, k in h.found do
            t[n] = t[n] or { 0, 0 }
            t[n][1] += k
        end
        keep_hunt(t)
        local finders = {}
        for n, k in h.found do
            table.insert(finders, `{n} {k}`)
        end
        local line = if #lasted == 0 and #finders == 0
            then "The hunt is over: nobody is left hiding."
            elseif #lasted == 0
            then `The hunt is over: every block was found -- {table.concat(finders, ", ")}`
            else `The hunt is over: never found, {table.concat(lasted, " and ")}`
                 .. (if #finders > 0 then ` -- found: {table.concat(finders, ", ")}` else "")
        local all = {}
        for p in h.everyone do
            table.insert(all, p)
        end
        for _, p in all do
            release(h, p)
            send(p, "h ")
        end
        core.play_sound("arena:gong", h.x + 0.5, h.y + 0.5, h.z + 0.5)
        near(h.x, h.z, line)
        core.log(`arena: {line}`)
    end

    core.event.on("core:used", function(ev)
        local h = hunt_of(ev.player)
        if h and h.hiders[ev.player] and ev.id ~= GONG then
            ev.cancel = true
            if core.node_solid(ev.id) ~= 1 then
                core.notice(ev.player, "Only a solid block: that one is not a shape a body can take.")
                return
            end
            if h.hiders[ev.player] ~= ev.id then
                disguise(h, ev.player, ev.id)
                core.notice(ev.player, `You are {core.node_name(ev.id)} now. Stand still and you settle into the grid.`)
            end
            return
        end
        if ev.id ~= GONG then
            return
        end
        ev.cancel = true
        show(ev.player, `{ev.x},{ev.y},{ev.z}`)
    end)

    -- Nothing broken or built in a hunt's ring by those hunting: a block dug at
    -- there is only a block.
    local function in_hunt_field(p: number, x: number, z: number): boolean
        local h = hunt_of(p)
        return h ~= nil and (x + 0.5 - h.x - 0.5) ^ 2 + (z + 0.5 - h.z - 0.5) ^ 2 <= (FIELD + 2) ^ 2
    end
    core.event.on("core:pre_break", function(ev)
        if in_hunt_field(ev.player, ev.x, ev.z) then
            ev.cancel = true
            core.notice(ev.player, "Only a block. Nothing is broken in a hunt's ring.")
        end
    end)
    core.event.on("core:pre_place", function(ev)
        if in_hunt_field(ev.player, ev.x, ev.z) then
            ev.cancel = true
            core.notice(ev.player, "Nothing is built in a hunt's ring while it is on.")
        end
    end)

    core.event.on("core:ui_click", function(ev)
        if ev.panel ~= `arena:panel{ev.player}` then
            return
        end
        core.ui_show(ev.panel, false, ev.player)
        local hkey = string.match(ev.id, "^hunt (.+)$")
        if hkey then
            start_hunt(hkey, ev.player)
            return
        end
        local key = string.match(ev.id, "^start (.+)$")
        if key == nil or rounds[key] or hunts[key] then
            return
        end
        local x, y, z = string.match(key, "^(%-?%d+),(%-?%d+),(%-?%d+)$")
        local r: Round = { x = tonumber(x) :: number, y = tonumber(y) :: number, z = tonumber(z) :: number,
                           left = ROUND, said = ROUND, score = {} }
        rounds[key] = r
        core.play_sound("arena:gong", r.x + 0.5, r.y + 0.5, r.z + 0.5)
        for _, p in core.players() do
            local px, _, pz = core.player_pos(p)
            if px and (px - r.x) ^ 2 + (pz - r.z) ^ 2 < 48 * 48 then
                core.notice(p, "The gong: a round of three minutes in the ring round it!")
            end
        end
        core.log(`arena: a round at {key}, struck by {core.player_name(ev.player)}`)
    end)

    -- A knock-down in a ring during a round: the striker's point.
    core.event.on("core:hit", function(ev)
        -- In a hunt: a seeker's blow on a hider finds them, and nothing else counts.
        local h = hunt_of(ev.player)
        if h then
            local victim = core.body_player(ev.target)
            if h.seekers[ev.player] and h.hiding <= 0 and victim ~= 0 and h.hiders[victim] then
                local who = core.player_name(ev.player)
                h.found[who] = (h.found[who] or 0) + 1
                make_seeker(h, victim)
                core.notice(ev.player, `Found {core.player_name(victim)}! {h.found[who]} this hunt`)
                core.notice(victim, `Found by {who}. You seek now.`)
                core.play_sound("arena:gong", h.x + 0.5, h.y + 0.5, h.z + 0.5)
            end
            return
        end
        if not ev.killed then
            return
        end
        local victim = core.body_player(ev.target)
        if victim == 0 or victim == ev.player or hunt_of(victim) then
            return
        end
        for _, r in rounds do
            if inside(ev.player, r) then
                local who = core.player_name(ev.player)
                r.score[who] = (r.score[who] or 0) + 1
                core.notice(ev.player, `Down! {r.score[who]} this round`)
                core.notice(victim, `Knocked down by {who}`)
                table.insert(back, { victim, r, 2 })
                return
            end
        end
    end)

    local since = 0
    core.event.on("core:tick", function(ev)
        -- The fallen back on the ring's edge, once the engine has stood them up.
        for i = #back, 1, -1 do
            local b = back[i]
            b[3] -= 1
            if b[3] <= 0 then
                local r = b[2] :: Round
                local a = math.random() * 2 * math.pi
                local x, z = r.x + 0.5 + math.cos(a) * (RING - 1), r.z + 0.5 + math.sin(a) * (RING - 1)
                core.player_place(b[1], x, r.y, z, r.x + 0.5 - x, 0, r.z + 0.5 - z)
                table.remove(back, i)
            end
        end
        -- A hider standing still settles square into the grid.
        for _, h in hunts do
            for p in h.hiders do
                local x, y, z = core.player_pos(p)
                if x == nil then
                    continue
                end
                local st = h.still[p]
                if st == nil or math.abs(st[1] - x) > 0.02 or math.abs(st[2] - y) > 0.02 or math.abs(st[3] - z) > 0.02 then
                    h.still[p] = { x, y, z, 0 }
                elseif st[4] >= 0 then
                    st[4] += ev.dt
                    if st[4] > 0.6 then
                        local cx, cz = math.floor(x) + 0.5, math.floor(z) + 0.5
                        if math.abs(cx - x) > 0.01 or math.abs(cz - z) > 0.01 then
                            local lx, ly, lz = core.player_look(p)
                            core.player_place(p, cx, y, cz, lx or 0, ly or 0, lz or 1)
                        end
                        h.still[p] = { cx, y, cz, -1 }   -- settled: not again until they move
                    end
                end
            end
        end
        since += ev.dt
        if since < 1 then
            return
        end
        local dt = since
        since = 0
        -- The hunts: the hiding over, who left, who wandered off, the line, the end.
        local hunt_line: { [number]: string } = {}
        for key, h in hunts do
            local was = h.hiding
            h.left -= dt
            h.hiding -= dt
            if was > 0 and h.hiding <= 0 then
                for p in h.seekers do
                    core.body_movement(p)
                    send(p, "b 0")
                end
                core.play_sound("arena:gong", h.x + 0.5, h.y + 0.5, h.z + 0.5)
                for p in h.everyone do
                    core.notice(p, if h.seekers[p] then "Seek! A blow finds a hider." else "They are looking. Keep still.")
                end
            end
            for p in h.everyone do
                if core.player_name(p) == "" then
                    h.everyone[p], h.hiders[p], h.seekers[p], h.still[p] = nil, nil, nil, nil
                elseif not in_field(p, h) then
                    local x, y, z = core.player_pos(p)
                    if x and h.hiders[p] then
                        -- Back on the ring's edge, the nearest point of it.
                        local dx, dz = x - h.x - 0.5, z - h.z - 0.5
                        local d = math.max(0.01, math.sqrt(dx * dx + dz * dz))
                        local bx, bz = h.x + 0.5 + dx / d * (FIELD - 1.5), h.z + 0.5 + dz / d * (FIELD - 1.5)
                        local by = h.y
                        for dy = UP, -UP, -1 do
                            local cx, cz = math.floor(bx), math.floor(bz)
                            if core.get_block(cx, h.y + dy, cz) == 0 and core.get_block(cx, h.y + dy + 1, cz) == 0
                                and core.node_solid(core.get_block(cx, h.y + dy - 1, cz)) == 1 then
                                by = h.y + dy
                                break
                            end
                        end
                        core.player_place(p, bx, by, bz, h.x + 0.5 - bx, 0, h.z + 0.5 - bz)
                        core.notice(p, "The hunt is in the ring: back you go.")
                    elseif x and h.seekers[p] and h.hiding <= 0 then
                        -- A seeker who walks away has left the hunt.
                        release(h, p)
                        core.notice(p, "You walked out of the hunt.")
                        send(p, "h ")
                    end
                end
            end
            local hidden, seeking = 0, 0
            for _ in h.hiders do
                hidden += 1
            end
            for _ in h.seekers do
                seeking += 1
            end
            if hidden == 0 or seeking == 0 or h.left <= 0 then
                end_hunt(key, h)
                continue
            end
            for p in h.everyone do
                local role = if h.hiders[p] then `you are {core.node_name(h.hiders[p])}` else "you seek"
                hunt_line[p] = if h.hiding > 0
                    then `Prop hunt: hiding, {math.ceil(h.hiding)} s -- {hidden} hiding -- {role}`
                    else `Prop hunt: {math.ceil(h.left)} s -- {hidden} still hidden -- {role}`
            end
        end
        -- Each player told of the gongs near and, in a ring in a round, the score.
        for _, p in core.players() do
            local px, _, pz = core.player_pos(p)
            local line = ""
            for key in gongs do
                local gx, gy, gz = string.match(key, "^(%-?%d+),(%-?%d+),(%-?%d+)$")
                local x, z = tonumber(gx) :: number, tonumber(gz) :: number
                if px and (px - x) ^ 2 + (pz - z) ^ 2 < 48 * 48 then
                    local r = rounds[key]
                    send(p, `g {key} {if r or hunts[key] then 1 else 0} {if hunts[key] then FIELD else RING}`)
                    if r and inside(p, r) then
                        line = `Arena: {math.max(0, math.ceil(r.left - dt))} s -- {said(r)}`
                    end
                end
            end
            send(p, `h {hunt_line[p] or line}`)
        end
        for key, r in rounds do
            r.left -= dt
            if r.said - r.left >= 30 and r.left > 1 then
                r.said = r.left
                for _, p in in_ring(r) do
                    core.notice(p, `{math.ceil(r.left)} s left -- {said(r)}`)
                end
            end
            if r.left <= 0 then
                rounds[key] = nil
                local t = standing()
                local best, top = {}, 0
                for n, k in r.score do
                    t[n] = t[n] or { 0, 0 }
                    t[n][2] += k
                    if k > top then
                        best, top = { n }, k
                    elseif k == top and k > 0 then
                        table.insert(best, n)
                    end
                end
                local line
                if top == 0 then
                    line = "The round is over: nobody was knocked down."
                else
                    for _, n in best do
                        t[n][1] += 1
                    end
                    line = `The round is over: {table.concat(best, " and ")} won with {top} -- {said(r)}`
                end
                keep(t)
                core.play_sound("arena:gong", r.x + 0.5, r.y + 0.5, r.z + 0.5)
                for _, p in core.players() do
                    local px, _, pz = core.player_pos(p)
                    if px and (px - r.x) ^ 2 + (pz - r.z) ^ 2 < 48 * 48 then
                        core.notice(p, line)
                    end
                end
                core.log(`arena: {line}`)
            end
        end
    end)

    -- One by the spawn's square, once a world, across from the chess table.
    local laid = core.storage_get("plaza") == "1" or core.plaza() == nil
    core.event.on("core:tick", function()
        if laid then
            return
        end
        local sx, sy, sz = core.world_spawn()
        if sx == nil then
            return
        end
        local x0, y, z0 = math.floor(sx) - 4, math.floor(sy), math.floor(sz) - 4
        if core.loaded(x0, y, z0) == 0 then
            return
        end
        laid = true
        core.storage_set("plaza", "1")
        for _, d in { { 0, 0 }, { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } } do
            local x, z = x0 + d[1], z0 + d[2]
            if core.get_block(x, y, z) == 0 and core.get_block(x, y + 1, z) == 0 and core.get_block(x, y - 1, z) ~= 0 then
                core.set_block(x, y, z, GONG)
                gongs[`{x},{y},{z}`] = true
                keep_gongs()
                core.log(`arena: a gong by the spawn's square at {x} {y} {z}`)
                return
            end
        end
    end)
end

if drawing then
    local tops: { [string]: { number } } = {}
    heard = function(text: string)
        if string.sub(text, 1, 2) == "h " then
            shown = string.sub(text, 3)
            return
        end
        if text == "b 1" or text == "b 0" then
            blind = text == "b 1"
            return
        end
        -- Drawn as a block, or not: this machine's own drawing of the body, and
        -- the camera behind it to see what they look like.
        local as = string.match(text, "^d (%d+)$")
        if as then
            local id = tonumber(as) :: number
            core.body_block(0, id)
            if id ~= 0 and viewed == nil then
                viewed = core.view(3.5)
            elseif id == 0 and viewed ~= nil then
                core.view(viewed)
                viewed = nil
            end
            return
        end
        local key, on, radius = string.match(text, "^g (%S+) (%d) (%d+)$")
        if key then
            local x, y, z = string.match(key, "^(%-?%d+),(%-?%d+),(%-?%d+)$")
            local had = near_gongs[key]
            local r = tonumber(radius) :: number
            if had and had[6] ~= r then
                tops[key] = nil
            end
            near_gongs[key] = { tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number,
                                tonumber(on) :: number, clock, r }
        end
    end
    if not serving then
        core.event.on("core:net_message", function(ev)
            heard(ev.text)
        end)
    end
    core.event.on("core:tick", function(ev)
        clock += ev.dt
    end)

    -- The ring's edge, on the ground: the top of whatever is solid at each
    -- point, within a few nodes of the gong's level.
    local function edge(g: { number }, key: string): { number }
        local t = tops[key]
        if t and #t > 0 then
            return t
        end
        t = {}
        local radius = g[6] or RING
        local marks = math.floor(radius * 6.4)
        for k = 0, marks - 1 do
            local a = k / marks * 2 * math.pi
            local x, z = g[1] + 0.5 + math.cos(a) * radius, g[3] + 0.5 + math.sin(a) * radius
            local y = g[2]
            for dy = 3, -3, -1 do
                local bx, bz = math.floor(x), math.floor(z)
                if core.get_block(bx, g[2] + dy, bz) == 0 and core.node_solid(core.get_block(bx, g[2] + dy - 1, bz)) == 1 then
                    y = g[2] + dy
                    break
                end
            end
            table.insert(t, x)
            table.insert(t, y)
            table.insert(t, z)
        end
        tops[key] = t
        return t
    end
    core.render.pass("arena:ring", 60, function()
        for key, g in near_gongs do
            if clock - g[5] > 3 then
                near_gongs[key] = nil
                tops[key] = nil
                continue
            end
            local t = edge(g, key)
            local on = g[4] == 1
            for i = 1, #t, 3 do
                core.render.box(t[i], t[i + 1] + 0.03, t[i + 2], 0.5, 0.05, 0.5,
                                if on then 255 else 200, if on then 120 else 170, if on then 40 else 90,
                                if on then 255 else 150)
            end
        end
    end)

    core.hud.pass("arena:score", 60, function()
        if blind then
            core.hud.rect(0.5, 0.5, 1, 1, 6, 6, 10, 245)
            core.hud.text(0.5, 0.45, "Eyes shut: the hiders are hiding.", 0.04, 1, 230, 220, 200, 255)
        end
        if shown == "" then
            return
        end
        core.hud.rect(0.5, 0.11, 0.9, 0.055, 0, 0, 0, 150)
        core.hud.text(0.5, 0.11, shown, 0.032, 1, 255, 220, 140, 255)
    end)
end
