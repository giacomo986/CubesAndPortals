--!strict
-- skies: the cloud world (games/skies). Islands without end over a sea of
-- cloud (layout.luau), the owl men's villages on some of them, updrafts that
-- wander between them, and at the end of the sky road north of home the Cloud
-- Manta's five islands: a boss for one player or several.
--
-- The Manta circles high over its islands, out of reach, and blows gusts --
-- a ball of wind seen flying, that pushes whoever is still where it lands --
-- at whoever is on a bridge. On its four outer islands stand lightning rods:
-- a right click on one calls a bolt down on it, and four bring it down onto
-- the centre island, where it is fought (phase 2), swooping low and calling
-- squalls. Hurt to under a third, it rises again and its bridges fall (phase
-- 3): everybody is handed a glider, and two rods struck again finish it.
-- Whoever is there when it falls gets the storm glider (mods/glider). Three
-- owl men fly in to help while it is fought. Its health is shown to everybody
-- near its islands; 60% more health and more squalls for each player more.
--
-- The owl men live in villages, walk between their huts and now and then
-- glide to another island. A right click on one: what it knows of the Manta
-- and the way there, and what it trades for sky crystals (picked off the
-- islands' grass).
--
-- An updraft (drawn rising, wandering slowly) throws whoever is in it up to
-- 58: a way back from a fall, and with a glider a way up. A fall into the
-- clouds is not a death: the body is put back where it last stood (or on the
-- nearest island), less four health.
--
-- The sandbox's mods are linked here (robot, tools, archery, the glider ...),
-- so a player's weapons and bag come with them. This mod runs on both sides:
-- the server's world and game, and on a player's machine the sea of cloud,
-- the updrafts, the shots and the Manta's health.

local L = require("@self/layout")
type Island = L.Island

local side = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

-- What the server tells one player, or everybody (0). Handed straight to the
-- drawing half when one process is both.
local heard: (string) -> () = function(_) end
local function tell(p: number, text: string)
    if drawing then heard(text) else core.net.send(p, text) end
end
local function tell_all(text: string)
    if drawing then heard(text) else core.net.send(0, text) end
end

local HOME = L.HOME
local CENTRE = L.CENTRE

-- ================================================================== server

if serving then
    core.net.open()
    L.set_seed(core.seed())

    local id = core.node_id
    local DIRT, STONE = id("core:dirt"), id("core:stone")
    local SMOOTH = id("rocks:smooth_stone")
    local BRICKS = id("rocks:stone_bricks")
    -- Grass of its own, drawn as the sandbox's: on core:grass the sandbox's
    -- animals come out, and would walk off the edge.
    local GRASS = core.register_node("skies:turf", 96, 150, 60, true, 0, "", 0, 999, "soil")
    local ROD = core.register_node("skies:rod", 250, 210, 90, true, 12, "", 0, 999, "metal")
    local ROD_SPENT = core.register_node("skies:rod_spent", 70, 70, 80, true, 0, "", 0, 999, "metal")
    local RACK = core.register_node("skies:rack", 150, 110, 70, true, 0, "", 0, 999, "wood")
    -- Sky crystals: a pale blue shard standing on the grass, that a hand picks.
    core.register_item("skies:sky_crystal", 64, "", 0, 1.0, "")
    local CRYSTAL = core.register_node("skies:crystal", 150, 220, 255, false, 9, "", 0, 0.3, "glass")
    core.node_box(CRYSTAL, 0.3, 0.0, 0.3, 0.7, 0.7, 0.7)
    core.node_drop_item(CRYSTAL, "skies:sky_crystal")

    local BLOCK: { [string]: number } = {}
    for _, n in { "timber:planks", "timber:log", "colors:brown_wool", "colors:white_wool", "trees:trunk",
                  "trees:leaves" } do
        BLOCK[n] = id(n)
    end
    BLOCK["skies:crystal"] = CRYSTAL

    core.mapgen_core(false)
    core.mapgen_only("skies")
    core.eye_height(1.5)

    -- What is not rock nor bridge: the rods and their pillars, the racks, the doorway.
    type Put = { x: number, y: number, z: number, b: number }
    local EXTRA: { Put } = {}
    local function put(x: number, y: number, z: number, b: number) table.insert(EXTRA, { x = x, y = y, z = z, b = b }) end
    local RODS: { { number } } = {}
    for _, s in L.ISLANDS do
        if s.rod then
            put(s.x, s.y + 1, s.z, BRICKS)
            put(s.x, s.y + 2, s.z, BRICKS)
            put(s.x, s.y + 3, s.z, ROD)
            table.insert(RODS, { s.x, s.y + 3, s.z })
        end
    end
    put(HOME.x + 3, HOME.y + 1, HOME.z, RACK)
    put(CENTRE.x + 4, CENTRE.y + 1, CENTRE.z + 4, RACK)
    -- The doorway home: on the home island's south side, facing +z, up the road.
    local DOOR_Z = HOME.z - 6
    for v = 0, 3 do
        for u = -2, 2 do
            if not (u == 0 and v < 2) then put(HOME.x + u, HOME.y + 1 + v, DOOR_Z, SMOOTH) end
            put(HOME.x + u, HOME.y + 1 + v, DOOR_Z - 1, SMOOTH)
        end
    end

    local function rock(s: Island, x: number, z: number)
        local dx, dz = x - s.x, z - s.z
        local e = L.edge(s, dx, dz)
        local d2 = dx * dx + dz * dz
        if d2 > e * e then return end
        local depth = 3 + (e - math.sqrt(d2)) * 1.4
        for y = math.floor(s.y - depth), s.y - 3 do core.set_block(x, y, z, STONE) end
        core.set_block(x, s.y - 2, z, DIRT)
        core.set_block(x, s.y - 1, z, DIRT)
        core.set_block(x, s.y, z, GRASS)
    end

    local function within(c: { x: number, z: number }, ctx: any): boolean
        return c.x >= ctx.min_x and c.x <= ctx.max_x and c.z >= ctx.min_z and c.z <= ctx.max_z
    end

    core.register_mapgen("skies:islands", function(ctx)
        local near: { Island } = {}
        local cells = L.cells_over(ctx.min_x - 16, ctx.min_z - 16, ctx.max_x + 16, ctx.max_z + 16)
        for _, c in cells do
            if c.island then table.insert(near, c.island :: Island) end
        end
        for _, s in L.ISLANDS do table.insert(near, s) end
        for x = ctx.min_x, ctx.max_x do
            for z = ctx.min_z, ctx.max_z do
                for _, s in near do rock(s, x, z) end
            end
        end
        for _, c in cells do
            for _, b in c.cells do
                if within(b, ctx) then core.set_block(b.x, b.y, b.z, BLOCK[b.b]) end
            end
        end
        for _, b in L.ARENA_CELLS do
            if within(b, ctx) then core.set_block(b.x, b.y, b.z, BLOCK[b.b]) end
        end
        for _, e in EXTRA do
            if within(e, ctx) then core.set_block(e.x, e.y, e.z, e.b) end
        end
    end)

    -- Opening 0 in the doorway, facing +z. Started from the rooms file, as the
    -- game does it, the doorway is said to the other rooms as "door"
    -- (core.room_portal_set), which the gate on the sandbox's plaza links to.
    core.set_portal(0, HOME.x + 0.5, HOME.y + 2, DOOR_Z + 0.6)
    local door_said = false
    core.event.on("core:tick", function(_)
        if door_said then return end
        door_said = true
        core.set_day_cycle(false)
        core.set_time(0.45)
        for k = 0, 1 do
            if core.portal_room(k) == "" then core.portal_park(k) end
        end
        if core.portal_room(0) == "" then
            door_said = core.room_portal_set("door", HOME.x + 0.5, HOME.y + 2, DOOR_Z + 0.5, 0, 1, 0.5, 1.0)
        end
    end)

    -- Nobody digs or builds the islands away; a sky crystal is picked.
    core.event.on("core:pre_break", function(ev)
        if ev.id ~= CRYSTAL then ev.cancel = true end
    end)
    core.event.on("core:pre_place", function(ev) ev.cancel = true end)

    -- ------------------------------------------------------------ creatures

    local NEVER = "skies:never"
    local MANTA_KIND = "skies:manta"
    local OWL, OWL_FLYING = "skies:owlman", "skies:owlman_flying"
    local OWL_MODEL = core.register_model("skies:owlman", "owlman.gltf")
    --                name        model                                                 hp   speed fight     dmg reach every drop n shot doors hover fire  spawn_on
    core.register_mob(MANTA_KIND, core.register_model("skies:manta", "manta.gltf"), 400, 6, "worker", 0, 3, 1.0, "", 0, 0, false, 1, false, NEVER)
    core.register_mob("skies:squall", core.register_model("skies:squall", "squall.gltf"), 10, 5, "melee", 2, 1.6, 1.2, "", 0, 0, false, 2.5, false, NEVER)
    core.register_mob(OWL, OWL_MODEL, 30, 2.2, "worker", 2, 1.6, 1.0, "", 0, 0, true, 0, false, NEVER)
    core.register_mob(OWL_FLYING, OWL_MODEL, 30, 6, "worker", 2, 1.6, 1.0, "", 0, 0, false, 1, false, NEVER)
    core.mob_voice(OWL_FLYING, OWL)
    core.mob_sight("skies:squall", 40)

    local function everyone(text: string)
        for _, p in core.players() do core.notice(p, text) end
    end
    local function f1(v: number): string return string.format("%.1f", v) end

    -- Whoever is near the Manta's islands.
    local function near_arena(x: number, z: number, r: number): boolean
        return (x - CENTRE.x) ^ 2 + (z - CENTRE.z) ^ 2 < r * r
    end

    -- ------------------------------------------------------------ the Manta

    local BASE_HP = 400
    local phase = "calm"        -- calm, rods, low, fall, won
    local manta = 0
    local hp, whole = 0.0, 1.0
    local angle, clock = 0.0, 0.0
    local mx, my, mz = CENTRE.x + 0.0, 58.0, CENTRE.z + 36.0
    local spent = 0
    local gust_wait, gust_at, gust_who = 6.0, 0.0, 0
    type Gust = { who: number, x: number, y: number, z: number, at: number }
    local gusts: { Gust } = {}
    local squall_wait, touch_wait = 10.0, 0.0
    local won_left, away_for = 0.0, 0.0
    local crumble: { number } = {}
    local crumble_wait = 0.0
    local squalls: { [number]: boolean } = {}
    local allies: { [number]: number } = {}   -- an owl man flying in to help: its place in the ring

    local function rods_back()
        for _, r in RODS do core.set_block(r[1], r[2], r[3], ROD) end
    end
    local function bridges_back()
        for _, c in L.ARENA_CELLS do core.set_block(c.x, c.y, c.z, BLOCK[c.b]) end
    end

    local function spawn_manta()
        if manta ~= 0 then core.mob_remove(manta) end
        manta = core.spawn_mob(MANTA_KIND, mx, my, mz)
        local n = 0
        for _, p in core.players() do
            local x, _, z = core.player_pos(p)
            if x and near_arena(x, z, 200) then n += 1 end
        end
        whole = BASE_HP * (1 + 0.6 * (math.max(1, n) - 1))
        hp = whole
        if manta ~= 0 then core.mob_health(manta, math.floor(hp)) end
    end

    local function drop_allies()
        for a in allies do core.mob_remove(a) end
        allies = {}
    end

    local function reset()
        for s in squalls do core.mob_remove(s) end
        squalls = {}
        drop_allies()
        bridges_back()
        rods_back()
        phase, spent, crumble, gusts = "calm", 0, {}, {}
        if manta ~= 0 then core.mob_remove(manta) end
        manta = 0
        tell_all("h 0 0")
    end

    local function call_squalls(x: number, y: number, z: number, n: number)
        local alive = 0
        for s in squalls do
            if core.mob_pos(s) == nil then squalls[s] = nil else alive += 1 end
        end
        for i = 1, math.min(n, 10 - alive) do
            local a = i * 2.4
            local s = core.spawn_mob("skies:squall", x + math.cos(a) * 4, y + 3, z + math.sin(a) * 4)
            if s ~= 0 then squalls[s] = true end
        end
    end

    -- A blow at the Manta from anything but a player's hand (a bolt, an owl
    -- man): its health goes down, and in phase 2 under a third it rises.
    local to_fall: () -> () = function() end
    local function hurt_manta(dmg: number, fx: number, fz: number)
        hp -= dmg
        if manta ~= 0 then core.mob_hurt(manta, math.ceil(dmg), fx, fz) end
        if phase == "low" and hp <= whole * 0.3 then to_fall() end
    end

    -- A bolt from the sky onto the rod, and from the rod into the Manta.
    local function strike(r: { number }, share: number)
        core.set_block(r[1], r[2], r[3], ROD_SPENT)
        core.play_sound("skies:thunder", r[1], r[2], r[3])
        core.play_sound("skies:thunder", mx, my, mz)
        tell_all(`b {r[1] + 0.5} {r[2] + 30} {r[3] + 0.5} {r[1] + 0.5} {r[2] + 0.5} {r[3] + 0.5}`)
        tell_all(`b {r[1] + 0.5} {r[2] + 0.5} {r[3] + 0.5} {f1(mx)} {f1(my + 0.7)} {f1(mz)}`)
        hurt_manta(whole * share, r[1], r[3])
        spent += 1
    end

    local function on_rod(p: number, x: number, y: number, z: number)
        local here: { number }? = nil
        for _, r in RODS do
            if r[1] == x and r[2] == y and r[3] == z then here = r end
        end
        if here == nil then return end
        local r = here :: { number }
        local n = #core.players()
        if phase == "calm" then
            if manta == 0 then spawn_manta() end
            phase, spent = "rods", 0
            everyone("The Manta turns on you: strike all four rods to bring it down.")
        end
        if phase == "rods" then
            strike(r, 0.12)
            call_squalls(r[1], r[2], r[3], 1 + n)
            if spent >= 4 then
                phase = "low"
                squall_wait = 8
                everyone("The Manta falls onto the centre island: fight it there!")
            else
                everyone(`A bolt strikes the Manta: {spent} of 4 rods.`)
            end
        elseif phase == "fall" then
            strike(r, 0.17)
            if spent >= 2 or hp <= 0 then
                hp = 0
                if manta ~= 0 then core.mob_hurt(manta, 99999, r[1], r[3]) end
            else
                everyone("A bolt strikes the Manta: one rod more.")
            end
        else
            core.notice(p, "The rod is spent.")
        end
    end

    to_fall = function()
        phase, spent = "fall", 0
        if manta ~= 0 then core.mob_health(manta, math.max(1, math.floor(hp))) end
        rods_back()
        crumble = {}
        for i = 1, #L.DECK do table.insert(crumble, i) end
        for i = #crumble, 2, -1 do
            local j = math.random(1, i)
            crumble[i], crumble[j] = crumble[j], crumble[i]
        end
        for _, p in core.players() do
            if core.bag_count(p, "glider:glider") + core.bag_count(p, "glider:storm_glider") == 0 then
                core.give_item(p, "glider:glider", 1)
            end
        end
        everyone("The Manta rises and the bridges fall! Hold a glider and strike two rods again.")
    end

    local function victory()
        phase, won_left = "won", 60
        for s in squalls do core.mob_remove(s) end
        squalls = {}
        drop_allies()
        tell_all("h 0 0")
        for _, p in core.players() do
            local x, _, z = core.player_pos(p)
            if x and near_arena(x, z, 200) then
                core.give_item(p, "glider:storm_glider", 1)
                local k = "won:" .. string.lower(core.player_name(p))
                core.storage_set(k, tostring((tonumber(core.storage_get(k)) or 0) + 1))
                core.notice(p, "The Cloud Manta is beaten. The storm glider is yours: hold it to fly on the storm's winds.")
            end
        end
        manta = 0
    end

    -- Struck by a player: only in phase 2 does it tell.
    core.event.on("core:hit", function(ev)
        if ev.target ~= manta or manta == 0 then return end
        if phase == "low" then
            hp -= ev.damage
            if hp <= whole * 0.3 then to_fall() end
        else
            core.mob_health(manta, math.max(1, math.floor(hp)))
        end
    end)

    -- ------------------------------------------------------------ owl men

    type Owl = { home: Island, flying: boolean, wait: number,
                 fx: number, fy: number, fz: number, tx: number, ty: number, tz: number,
                 t: number, T: number, to: Island? }
    local owls: { [number]: Owl } = {}
    local villages: { [string]: { number } } = {}   -- by island name: its owl men

    local function kind_ok(o: number): boolean
        local k = core.mob_kind(o)
        return k == OWL or k == OWL_FLYING
    end

    local function spawn_village(s: Island)
        local ids = {}
        for i, off in { { -4, -5 }, { 3, -2 }, { 0, 2 } } do
            local o = core.spawn_mob(OWL, s.x + off[1] + 0.5, s.y + 1, s.z + off[2] + 0.5)
            if o ~= 0 then
                owls[o] = { home = s, flying = false, wait = 2 + i * 2, fx = 0, fy = 0, fz = 0, tx = 0, ty = 0,
                            tz = 0, t = 0, T = 1, to = nil }
                table.insert(ids, o)
            end
        end
        villages[s.name] = ids
    end

    local function land(o: number, w: Owl)
        core.mob_become(o, OWL)
        core.mob_act(o, "", "")
        w.flying = false
        w.home = w.to or w.home
        w.wait = 4
    end

    local function take_off(o: number, w: Owl, to: Island)
        local x, y, z = core.mob_pos(o)
        if x == nil then return end
        core.mob_become(o, OWL_FLYING)
        core.mob_act(o, "glide", "")
        w.flying, w.to = true, to
        w.fx, w.fy, w.fz = x, y, z
        local a = math.random() * 6.28
        w.tx, w.ty, w.tz = to.x + 0.5 + math.cos(a) * 2, to.y + 1, to.z + 0.5 + math.sin(a) * 2
        w.t = 0
        w.T = math.max(3, math.sqrt((w.tx - x) ^ 2 + (w.tz - z) ^ 2) / 7)
    end

    -- What an owl man trades, for sky crystals mostly.
    type Trade = { give: { { any } }, get: { any } }
    local TRADES: { Trade } = {
        { give = { { "skies:sky_crystal", 2 } }, get = { "glider:glider", 1 } },
        { give = { { "furnace:iron_ingot", 6 } }, get = { "skies:sky_crystal", 3 } },
        { give = { { "skies:sky_crystal", 5 } }, get = { "ore:diamond", 1 } },
        { give = { { "skies:sky_crystal", 3 } }, get = { "timber:planks", 16 } },
    }

    local function trade_text(t: Trade): string
        local parts = {}
        for _, c in t.give do table.insert(parts, `{c[2]} {core.item_name(core.item_id(c[1]))}`) end
        return `{table.concat(parts, ", ")} -> {t.get[2]} {core.item_name(core.item_id(t.get[1]))}`
    end

    local function show_owl(p: number, o: number)
        local x, _, z = core.mob_pos(o)
        if x == nil then return end
        local d = math.floor(math.sqrt((CENTRE.x - x) ^ 2 + (CENTRE.z - z) ^ 2))
        local name = `skies:owl{p}`
        core.ui_define(name, 15, 6.2 + #TRADES * 1.4)
        if d < 90 then
            core.ui_label(name, 0.4, 0.3, "Hoo. The Manta is here. Strike the four rods and it comes down.")
        else
            core.ui_label(name, 0.4, 0.3, "Hoo. The Cloud Manta circles at the end of the sky road, north of home.")
            core.ui_label(name, 0.4, 0.9, `Its islands are {d} m from here.`)
        end
        core.ui_label(name, 0.4, 1.7, "Fall, and an updraft carries you back up. In one, a glider climbs.")
        core.ui_label(name, 0.4, 2.5, "Bring me sky crystals, from the islands' grass:")
        for i, t in TRADES do
            core.ui_button(name, `trade:{i}`, 0.4, 2.0 + i * 1.4, 14.2, 1.2, trade_text(t))
        end
        core.ui_button(name, "close", 0.4, 3.6 + #TRADES * 1.4, 14.2, 1.1, "Close")
        core.ui_show(name, true, p)
    end

    core.event.on("core:used_mob", function(ev)
        local w = owls[ev.target]
        if w == nil then return end
        local px, _, pz = core.player_pos(ev.player)
        if not w.flying and px then
            core.mob_face(ev.target, px, pz)
            core.mob_act(ev.target, "wave", "")
            w.wait = 3
        end
        show_owl(ev.player, ev.target)
    end)

    core.event.on("core:ui_click", function(ev)
        local panel: string = ev.panel
        if string.sub(panel, 1, 9) ~= "skies:owl" then return end
        if ev.id == "close" then
            core.ui_show(panel, false, ev.player)
            return
        end
        local i = tonumber(string.match(ev.id, "^trade:(%d+)$") or "")
        local t = if i then TRADES[i] else nil
        if t == nil then return end
        for _, c in t.give do
            if core.bag_count(ev.player, c[1]) < c[2] then
                core.notice(ev.player, `Not enough for that: {trade_text(t)}`)
                return
            end
        end
        for _, c in t.give do core.take_item(ev.player, c[1], c[2]) end
        core.give_item(ev.player, t.get[1], t.get[2])
        core.notice(ev.player, "Hoo. A fair trade.")
    end)

    -- The owl men's half second: villages near somebody filled, far ones
    -- emptied, and each one's next errand.
    local function owls_step(here: { number })
        local spots: { { number } } = {}
        for _, p in here do
            local x, _, z = core.player_pos(p)
            if x then table.insert(spots, { x, z }) end
        end
        local function near_any(x: number, z: number, r: number): boolean
            for _, s in spots do
                if (s[1] - x) ^ 2 + (s[2] - z) ^ 2 < r * r then return true end
            end
            return false
        end
        for _, s in spots do
            for _, isl in L.islands_near(s[1], s[2], 80) do
                if isl.village and villages[isl.name] == nil then spawn_village(isl) end
            end
        end
        for name, ids in villages do
            local any = false
            for _, o in ids do
                local x, _, z = core.mob_pos(o)
                if x and near_any(x, z, 130) then any = true end
            end
            if not any then
                for _, o in ids do
                    if owls[o] then core.mob_remove(o) end
                    owls[o] = nil
                end
                villages[name] = nil
            end
        end
        for o, w in owls do
            if allies[o] then continue end
            if core.mob_pos(o) == nil or not kind_ok(o) then
                owls[o] = nil
                continue
            end
            if w.flying then continue end
            w.wait -= 0.5
            if w.wait > 0 then continue end
            w.wait = 6 + math.random() * 8
            if math.random() < 0.12 then
                local others = {}
                for _, isl in L.islands_near(w.home.x, w.home.z, 75) do
                    if isl ~= w.home and not isl.rod and isl ~= CENTRE then table.insert(others, isl) end
                end
                if #others > 0 then
                    take_off(o, w, others[math.random(1, #others)])
                    continue
                end
            end
            local a, d = math.random() * 6.28, math.random() * (w.home.r - 3)
            core.mob_goto(o, math.floor(w.home.x + math.cos(a) * d), w.home.y + 1, math.floor(w.home.z + math.sin(a) * d))
        end
    end

    -- Every tick: the owl men in the air, along an arc to where they land.
    local function owls_fly(dt: number)
        for o, w in owls do
            if not w.flying or allies[o] then continue end
            w.t += dt
            local u = math.min(1, w.t / w.T)
            local x = w.fx + (w.tx - w.fx) * u
            local z = w.fz + (w.tz - w.fz) * u
            local y = w.fy + (w.ty - w.fy) * u + math.sin(u * math.pi) * 8
            core.mob_place(o, x, y, z)
            core.mob_face(o, w.tx, w.tz)
            if u >= 1 then land(o, w) end
        end
    end

    -- The Manta's fight: three owl men circling its islands, each in turn
    -- striking the nearest squall or, low, the Manta itself.
    local ally_wait = 0.0
    local function allies_step(dt: number)
        local fighting = phase == "rods" or phase == "low" or phase == "fall"
        if not fighting then
            if next(allies) then drop_allies() end
            return
        end
        local n = 0
        for a, slot in allies do
            if core.mob_pos(a) == nil or not kind_ok(a) then
                allies[a] = nil
            else
                n += 1
                local ang = clock * 0.35 + slot * 2.09
                core.mob_place(a, CENTRE.x + math.cos(ang) * 16, CENTRE.y + 9 + math.sin(clock + slot) * 1.5,
                               CENTRE.z + math.sin(ang) * 16)
                core.mob_face(a, CENTRE.x + math.cos(ang + 0.3) * 16, CENTRE.z + math.sin(ang + 0.3) * 16)
            end
        end
        if n < 3 then
            local a = core.spawn_mob(OWL_FLYING, CENTRE.x, CENTRE.y + 20, CENTRE.z - 30)
            if a ~= 0 then
                local used: { [number]: boolean } = {}
                for _, s in allies do used[s] = true end
                local slot = 0
                while used[slot] do slot += 1 end
                allies[a] = slot
                core.mob_act(a, "glide", "")
                if n == 0 then everyone("Owl men fly in to help!") end
            end
        end
        ally_wait -= dt
        if ally_wait > 0 then return end
        ally_wait = 0.55
        for a in allies do
            if math.random() > 0.35 then continue end
            local ax, ay, az = core.mob_pos(a)
            if ax == nil then continue end
            local best, d2 = 0, 30 * 30
            for s in squalls do
                local sx, _, sz = core.mob_pos(s)
                if sx then
                    local d = (sx - ax) ^ 2 + (sz - az) ^ 2
                    if d < d2 then best, d2 = s, d end
                end
            end
            if best ~= 0 then
                local sx, sy, sz = core.mob_pos(best)
                core.mob_hurt(best, 6, ax, az)
                tell_all(`o {f1(ax)} {f1(ay + 1)} {f1(az)} {f1(sx :: number)} {f1(sy + 0.3)} {f1(sz)}`)
            elseif phase == "low" and manta ~= 0 and math.random() < 0.5 then
                -- A peck, no more: alone they would take a minute and more
                -- over what a player does in seconds.
                tell_all(`o {f1(ax)} {f1(ay + 1)} {f1(az)} {f1(mx)} {f1(my + 0.7)} {f1(mz)}`)
                hurt_manta(1, ax, az)
            end
        end
    end

    -- ------------------------------------------------------------ the tick

    local safe: { [number]: { number } } = {}
    local seen: { [number]: boolean } = {}
    local lift_wait, half = 0.0, 0.0
    local updrafts: { [number]: { number } } = {}   -- each player's, last sent

    core.event.on("core:tick", function(ev)
        local dt: number = ev.dt
        clock += dt
        local here = core.players()
        owls_fly(dt)
        if #here == 0 then return end

        for _, p in here do
            if not seen[p] then
                seen[p] = true
                core.player_spawn(p, HOME.x + 0.5, HOME.y + 1, HOME.z + 2.5)
            end
        end

        -- Updrafts: sent twice a second to whoever is near one, lifting every
        -- fifth of a second whoever stands in one.
        half += dt
        if half >= 0.5 then
            half = 0
            for _, p in here do
                local x, _, z = core.player_pos(p)
                if x == nil then continue end
                local u = L.updrafts_near(x, z, 96, clock)
                updrafts[p] = u
                local parts = {}
                for i = 1, #u do table.insert(parts, f1(u[i])) end
                tell(p, "u " .. table.concat(parts, " "))
                if near_arena(x, z, 160) and phase ~= "calm" and phase ~= "won" then
                    tell(p, `h {math.max(0, math.floor(hp))} {math.floor(whole)}`)
                end
            end
            owls_step(here)
        end
        lift_wait -= dt
        if lift_wait <= 0 then
            lift_wait = 0.2
            for _, p in here do
                local x, y, z = core.player_pos(p)
                local u = updrafts[p]
                if x == nil or u == nil or y > L.UP_TOP then continue end
                for i = 1, #u, 2 do
                    if (u[i] - x) ^ 2 + (u[i + 1] - z) ^ 2 < L.UP_R * L.UP_R then
                        core.player_launch(p, 0, 15.5, 0, 0.25)   -- 16 m/s an axis at most
                        break
                    end
                end
            end
        end

        -- Bodies that fell into the clouds, back where they last stood.
        for _, p in here do
            local x, y, z = core.player_pos(p)
            if x == nil then continue end
            if y < L.CATCH_Y then
                local s = safe[p]
                if s == nil or core.get_block(math.floor(s[1]), math.floor(s[2]) - 1, math.floor(s[3])) == 0 then
                    local best, d2 = HOME, math.huge
                    for _, isl in L.islands_near(x, z, 120) do
                        local d = (isl.x - x) ^ 2 + (isl.z - z) ^ 2
                        if d < d2 then best, d2 = isl, d end
                    end
                    s = { best.x + 0.5, best.y + 1, best.z + 2.5 }
                end
                local t = s :: { number }
                core.player_place(p, t[1], t[2], t[3])
                core.damage(p, 4)
                core.notice(p, "Caught by the wind and set down again.")
            elseif core.get_block(math.floor(x), math.floor(y) - 1, math.floor(z)) ~= 0 then
                safe[p] = { x, y, z }
            end
        end

        -- The Manta: there while somebody is near its islands.
        local close = false
        for _, p in here do
            local x, _, z = core.player_pos(p)
            if x and near_arena(x, z, 220) then close = true end
        end
        if not close then
            away_for += dt
            if away_for > 60 and (phase ~= "calm" or manta ~= 0) then reset() end
            return
        end
        away_for = 0
        if phase == "won" then
            won_left -= dt
            if won_left <= 0 then
                reset()
                everyone("The bridges are back, and the Manta circles again.")
            end
            return
        end
        if manta == 0 then spawn_manta() end
        allies_step(dt)

        local radius, speed, alt
        if phase == "low" then
            radius, speed = 8, 5
            alt = CENTRE.y + 3 + 2.5 * (1 + math.sin(clock * 0.8))
        else
            radius, speed = 36, 8
            alt = 58 - (if phase == "rods" then spent * 3 else 0)
        end
        angle += speed / radius * dt
        local tx, tz = CENTRE.x + math.cos(angle) * radius, CENTRE.z + math.sin(angle) * radius
        mx += (tx - mx) * math.min(1, dt * 2)
        mz += (tz - mz) * math.min(1, dt * 2)
        my += (alt - my) * math.min(1, dt * 0.8)
        if manta ~= 0 then
            if core.mob_pos(manta) == nil then
                if phase == "low" or phase == "fall" then victory() return end
                manta = 0
                return
            end
            core.mob_place(manta, mx, my, mz)
            -- Where it flies: along the circle, angle growing.
            core.mob_face(manta, mx - math.sin(angle) * 10, mz + math.cos(angle) * 10)
        end

        -- Gusts: a ball of wind from the Manta to where somebody on a bridge
        -- stands, seen flying; whoever is still there when it lands is thrown.
        if phase == "rods" or phase == "fall" or phase == "calm" then
            gust_wait -= dt
            if gust_who ~= 0 then
                gust_at -= dt
                if gust_at <= 0 then
                    local x, y, z = core.player_pos(gust_who)
                    if x then
                        local T = 0.9
                        tell_all(`g {f1(mx)} {f1(my + 0.6)} {f1(mz)} {f1(x)} {f1(y + 1)} {f1(z)} {T}`)
                        table.insert(gusts, { who = gust_who, x = x, y = y, z = z, at = T })
                    end
                    gust_who = 0
                end
            elseif gust_wait <= 0 then
                gust_wait = if phase == "calm" then 12 else 6
                local on_bridge = {}
                for _, p in here do
                    local x, _, z = core.player_pos(p)
                    if x and near_arena(x, z, 80) and L.island_at(math.floor(x), math.floor(z)) == nil then
                        table.insert(on_bridge, p)
                    end
                end
                if #on_bridge > 0 then
                    gust_who = on_bridge[math.random(1, #on_bridge)]
                    gust_at = 1.2
                    core.notice(gust_who, "The Manta draws breath...")
                end
            end
        end
        for i = #gusts, 1, -1 do
            local g = gusts[i]
            g.at -= dt
            if g.at <= 0 then
                table.remove(gusts, i)
                local x, y, z = core.player_pos(g.who)
                core.play_sound("skies:gust", g.x, g.y, g.z)
                if x and (x - g.x) ^ 2 + (y - g.y) ^ 2 + (z - g.z) ^ 2 < 3.5 * 3.5 then
                    local dx, dz = x - mx, z - mz
                    local d = math.max(0.1, math.sqrt(dx * dx + dz * dz))
                    core.player_launch(g.who, dx / d * 12, 4, dz / d * 12, 0.35)
                end
            end
        end

        if phase == "low" then
            squall_wait -= dt
            if squall_wait <= 0 then
                squall_wait = 12
                call_squalls(CENTRE.x, CENTRE.y + 1, CENTRE.z, 1 + #here // 2)
            end
            touch_wait -= dt
            if touch_wait <= 0 then
                touch_wait = 1
                for _, p in here do
                    local x, y, z = core.player_pos(p)
                    if x and (x - mx) ^ 2 + (z - mz) ^ 2 < 16 and math.abs(y - my) < 3 then
                        core.damage(p, 3)
                        tell_all(`r {f1(x)} {f1(y)} {f1(z)}`)
                        local dx, dz = x - mx, z - mz
                        local d = math.max(0.1, math.sqrt(dx * dx + dz * dz))
                        core.player_launch(p, dx / d * 8, 3, dz / d * 8, 0.3)
                    end
                end
            end
        end

        -- Phase 3: the bridges go, a few planks every tenth of a second.
        -- ponytail: taken away, not core.fall: a plank falling into the void
        -- has nothing to stop it.
        if phase == "fall" and #crumble > 0 then
            crumble_wait -= dt
            if crumble_wait <= 0 then
                crumble_wait = 0.1
                for _ = 1, 4 do
                    local i = table.remove(crumble)
                    if i == nil then break end
                    local c = L.DECK[i]
                    core.set_block(c.x, c.y, c.z, 0)
                end
            end
        end
    end)

    core.event.on("core:used", function(ev)
        if ev.id == ROD or ev.id == ROD_SPENT then
            ev.cancel = true
            on_rod(ev.player, ev.x, ev.y, ev.z)
        elseif ev.id == RACK then
            ev.cancel = true
            if core.bag_count(ev.player, "glider:glider") + core.bag_count(ev.player, "glider:storm_glider") == 0 then
                core.give_item(ev.player, "glider:glider", 1)
                core.notice(ev.player, "A glider: hold it and step off. Crouch to fold it.")
            else
                core.notice(ev.player, "You have a glider already.")
            end
        end
    end)

    core.command("manta", "-- how many times you have beaten the Cloud Manta")
    core.command("skies", "<manta|village|updraft|phase2|phase3|kit|reset> -- quick test jump", "admin")
    core.event.on("core:command", function(ev)
        if ev.name == "manta" then
            local n = tonumber(core.storage_get("won:" .. string.lower(core.player_name(ev.player)))) or 0
            core.notice(ev.player, if n == 0 then "You have not beaten the Cloud Manta yet." else `Cloud Manta beaten: {n}`)
            return
        end
        if ev.name ~= "skies" then return end
        local verb = string.lower(string.match(ev.args, "^%s*(%S*)") or "")
        if verb == "manta" then
            core.player_place(ev.player, CENTRE.x + 0.5, CENTRE.y + 1, CENTRE.z - 22, 0, 0, 1)
        elseif verb == "village" then
            local px, _, pz = core.player_pos(ev.player)
            local cx0, cz0 = math.floor((px or 0) / L.CELL), math.floor((pz or 0) / L.CELL)
            local best: Island? = nil
            local bd = math.huge
            for r = 0, 12 do
                for dz = -r, r do
                    for dx = -r, r do
                        for _, isl in L.islands_near((cx0 + dx) * L.CELL, (cz0 + dz) * L.CELL, L.CELL) do
                            if isl.village then
                                local d = (isl.x - (px or 0)) ^ 2 + (isl.z - (pz or 0)) ^ 2
                                if d < bd then best, bd = isl, d end
                            end
                        end
                    end
                end
                if best then break end
            end
            if best then
                core.player_place(ev.player, best.x + 0.5, best.y + 1, best.z + 4.5, 0, 0, -1)
            else
                core.chat(ev.player, "No village found near here.")
            end
        elseif verb == "updraft" then
            local px, _, pz = core.player_pos(ev.player)
            local u = L.updrafts_near(px or 0, pz or 0, 200, clock)
            if #u >= 2 then
                core.player_place(ev.player, u[1], 36, u[2], 0, 0, 1)
            else
                core.chat(ev.player, "No updraft within 200 m.")
            end
        elseif verb == "phase2" then
            reset()
            spawn_manta()
            for _, r in RODS do core.set_block(r[1], r[2], r[3], ROD_SPENT) end
            spent, phase, squall_wait = 4, "low", 4
            core.player_place(ev.player, CENTRE.x + 0.5, CENTRE.y + 1, CENTRE.z - 6, 0, 0, 1)
            everyone("The Manta falls onto the centre island: fight it there!")
        elseif verb == "phase3" then
            if manta == 0 then spawn_manta() end
            hp = whole * 0.28
            core.player_place(ev.player, CENTRE.x + 0.5, CENTRE.y + 1, CENTRE.z - 6, 0, 0, 1)
            to_fall()
        elseif verb == "kit" then
            core.give_item(ev.player, "glider:storm_glider", 1)
            core.give_item(ev.player, "glider:glider", 1)
            core.give_item(ev.player, "skies:sky_crystal", 20)
            core.give_item(ev.player, "archery:bow", 1)
            core.give_item(ev.player, "archery:arrow", 64)
            core.give_item(ev.player, "tools:sword", 1)
            core.notice(ev.player, "Storm glider, sky crystals and weapons given.")
        elseif verb == "reset" then
            reset()
            core.notice(ev.player, "Manta reset.")
        else
            core.chat(ev.player, "/skies <manta|village|updraft|phase2|phase3|kit|reset>")
        end
    end)
end

-- ================================================================== a player's machine

if drawing then
    local clock = 0.0
    local ups: { number } = {}                 -- x, z of each updraft near
    local boss_hp, boss_whole = 0, 0
    local boss_seen = -99.0
    type Shot = { kind: string, a: { number }, b: { number }, t0: number, T: number }
    local shots: { Shot } = {}

    local function nums(text: string): { number }
        local out = {}
        for n in string.gmatch(text, "%S+") do table.insert(out, tonumber(n) or 0) end
        return out
    end

    heard = function(text: string)
        local k = string.sub(text, 1, 1)
        local v = nums(string.sub(text, 3))
        if k == "u" then
            ups = v
        elseif k == "h" then
            boss_hp, boss_whole, boss_seen = v[1] or 0, v[2] or 0, clock
        elseif k == "g" and #v >= 7 then
            table.insert(shots, { kind = "gust", a = { v[1], v[2], v[3] }, b = { v[4], v[5], v[6] }, t0 = clock, T = v[7] })
        elseif k == "b" and #v >= 6 then
            table.insert(shots, { kind = "bolt", a = { v[1], v[2], v[3] }, b = { v[4], v[5], v[6] }, t0 = clock, T = 0.45 })
        elseif k == "o" and #v >= 6 then
            table.insert(shots, { kind = "owl", a = { v[1], v[2], v[3] }, b = { v[4], v[5], v[6] }, t0 = clock, T = 0.35 })
        elseif k == "r" and #v >= 3 then
            table.insert(shots, { kind = "ring", a = { v[1], v[2], v[3] }, b = { v[1], v[2], v[3] }, t0 = clock, T = 0.5 })
        end
    end
    if not serving then
        core.event.on("core:net_message", function(ev) heard(ev.text :: string) end)
    end

    local flash = 0.0
    core.event.on("core:tick", function(ev)
        clock += ev.dt
        -- Every tick, not once: the first ones come while the join is still
        -- under way, and a value set then is the loading screen's.
        core.cloud_sea(L.CLOUD_BASE, L.CLOUD_TOP)
        if flash > 0 then
            core.sky_flash(flash)
            flash = math.max(0, flash - ev.dt * 2.5)
        end
    end)

    core.render.pass("skies:air", 70, function()
        -- Updrafts: pale blue puffs rising up the column, thinning as they go,
        -- and streaks of light running up through them.
        local span = L.UP_TOP - 12
        for i = 1, #ups, 2 do
            local x, z = ups[i], ups[i + 1]
            -- Puffs in a spiral up the column, as wide as its reach.
            for k = 0, 23 do
                local h = (clock * 7 + k * span / 24) % span
                local y = 12 + h
                local u = h / span
                local a = k * 2.4 + clock * 0.8
                core.render.fx("smoke", x + math.cos(a) * 1.8, y, z + math.sin(a) * 1.8, 2.2 + u * 1.5,
                               150, 205, 255, 200, 0.15 + u * 0.7)
            end
            for k = 0, 7 do
                local h = (clock * 16 + k * span / 8) % span
                local ox, oz = math.cos(k * 0.79 + clock * 0.5) * 2.4, math.sin(k * 0.79 + clock * 0.5) * 2.4
                core.render.fx("streak", x + ox, 12 + h, z + oz, 0.22, 120, 220, 255, 255, 0.5,
                               x + ox, 12 + h + 5, z + oz)
            end
            core.render.fx("glow", x, L.UP_TOP - 2, z, 3, 140, 210, 255, 150, 0.5)
            core.render.fx("ring", x, L.CLOUD_TOP + 1, z, L.UP_R + 1 + 0.5 * math.sin(clock * 2), 120, 210, 255, 200, 0.4)
        end
        -- The Manta's gusts, the bolts, the owl men's strikes.
        for i = #shots, 1, -1 do
            local s = shots[i]
            local u = (clock - s.t0) / s.T
            if u > 1.6 then
                table.remove(shots, i)
                continue
            end
            local a, b = s.a, s.b
            if s.kind == "gust" then
                if u <= 1 then
                    local x, y, z = a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u, a[3] + (b[3] - a[3]) * u
                    local tu = math.max(0, u - 0.18)
                    core.render.fx("glow", x, y, z, 1.3, 190, 225, 255, 220, 0.5)
                    core.render.fx("smoke", x, y, z, 1.1, 230, 240, 255, 180, 0.3)
                    core.render.fx("streak", a[1] + (b[1] - a[1]) * tu, a[2] + (b[2] - a[2]) * tu,
                                   a[3] + (b[3] - a[3]) * tu, 0.35, 170, 215, 255, 220, 0.5, x, y, z)
                else
                    local t = u - 1
                    core.render.fx("smoke", b[1], b[2], b[3], 1.5 + t * 3, 230, 240, 255, 200, t / 0.6)
                    core.render.fx("ring", b[1], b[2] - 1, b[3], 1 + t * 5, 190, 225, 255, 200, t / 0.6)
                end
            elseif s.kind == "bolt" then
                if u <= 1 then
                    core.render.fx("bolt", a[1], a[2], a[3], 0.5, 255, 245, 170, 255, u, b[1], b[2], b[3])
                    if u < 0.1 then flash = math.max(flash, 0.7) end
                end
            elseif s.kind == "owl" then
                if u <= 1 then
                    core.render.fx("streak", a[1], a[2], a[3], 0.2, 255, 210, 120, 230, u,
                                   a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u, a[3] + (b[3] - a[3]) * u)
                end
            elseif s.kind == "ring" then
                if u <= 1 then core.render.fx("ring", a[1], a[2], a[3], 1 + u * 4, 200, 230, 255, 220, u) end
            end
        end
    end)

    -- The Manta's health, at the top of the screen while it is fought near.
    core.hud.pass("skies:boss", 60, function()
        if boss_whole <= 0 or clock - boss_seen > 2 then return end
        local f = math.clamp(boss_hp / boss_whole, 0, 1)
        core.hud.text(0.5, 0.045, "Cloud Manta", 0.03, 1, 220, 235, 255, 255)
        core.hud.rect(0.5, 0.085, 0.42, 0.024, 0, 0, 0, 170)
        core.hud.rect(0.5 - 0.205 + 0.205 * f, 0.085, 0.41 * f, 0.016, 120, 190, 255, 240)
    end)
end
