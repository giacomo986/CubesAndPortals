--!strict
-- skies: the sky islands (games/skies), and what they are for: the Cloud
-- Manta, a boss for one player or several.
--
-- Six islands over a sea of cloud, bridges between them (layout.luau). The
-- Manta circles high over them, out of reach, and blows gusts at whoever is
-- on a bridge. On the four outer islands stand lightning rods: a right click
-- on one calls a bolt down on it, and four bring it down onto the centre
-- island, where it is fought (phase 2), swooping low and calling small storm
-- clouds (squalls). Hurt to under a third, it rises again and the bridges
-- fall (phase 3): everybody is handed a glider, and two rods struck again
-- -- reached by gliding from the centre -- finish it. Whoever is in the world
-- when it falls gets the storm glider (mods/glider), theirs to carry out: a
-- glider that the storm's winds keep lifting. A minute later the bridges and
-- the rods are back and the Manta circles again.
--
-- Together: its health grows by 60% for each player more, and so do the
-- squalls it calls. A fall into the clouds is not a death: the body is put
-- back where it last stood (or on the nearest island), less four health.
--
-- The sandbox's mods are linked here (robot, tools, archery, the glider ...),
-- so a player's weapons and bag come with them.

local L = require("@self/layout")

local id = core.node_id
local DIRT, STONE = id("core:dirt"), id("core:stone")
-- Grass of its own, drawn as the sandbox's: on core:grass the sandbox's
-- animals come out, and would walk off the edge.
local GRASS = core.register_node("skies:turf", 96, 150, 60, true, 0, "", 0, 999, "soil")
local BRICKS, SMOOTH = id("rocks:stone_bricks"), id("rocks:smooth_stone")
local BLOCK: { [string]: number } = { ["timber:planks"] = id("timber:planks"), ["timber:log"] = id("timber:log") }

local CLOUD = core.register_node("skies:cloud", 245, 248, 255, false, 0)
core.node_glass(CLOUD, 200)
local ROD = core.register_node("skies:rod", 250, 210, 90, true, 12, "", 0, 999, "metal")
local ROD_SPENT = core.register_node("skies:rod_spent", 70, 70, 80, true, 0, "", 0, 999, "metal")
local RACK = core.register_node("skies:rack", 150, 110, 70, true, 0, "", 0, 999, "wood")

core.mapgen_core(false)
core.mapgen_only("skies")
core.eye_height(1.5)

local HOME = L.ISLANDS[1]
local CENTRE = L.ISLANDS[3]

-- What is not an island's rock nor a bridge: the rods and their pillars, the
-- racks, the doorway home.
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
-- The doorway: a frame of smooth stone round a hole one wide and two high,
-- on the home island's south side, facing +z towards the others.
local DOOR_Z = HOME.z - 6
for v = 0, 3 do
    for u = -2, 2 do
        local hole = u == 0 and v < 2
        if not hole then put(HOME.x + u, HOME.y + 1 + v, DOOR_Z, SMOOTH) end
        put(HOME.x + u, HOME.y + 1 + v, DOOR_Z - 1, SMOOTH)
    end
end

core.register_mapgen("skies:islands", function(ctx)
    for x = ctx.min_x, ctx.max_x do
        for z = ctx.min_z, ctx.max_z do
            -- The sea of cloud: puffs, one or two high.
            local n = math.noise(x / 17, 0.37, z / 17)
            if n > 0.05 then core.set_block(x, L.CLOUD_Y, z, CLOUD) end
            if n > 0.3 then core.set_block(x, L.CLOUD_Y + 1, z, CLOUD) end
            local i, s = L.island_at(x, z)
            if s then
                local dx, dz = x - s.x, z - s.z
                local depth = 3 + (L.edge(i, s, dx, dz) - math.sqrt(dx * dx + dz * dz)) * 1.4
                for y = math.floor(s.y - depth), s.y - 3 do core.set_block(x, y, z, STONE) end
                core.set_block(x, s.y - 2, z, DIRT)
                core.set_block(x, s.y - 1, z, DIRT)
                core.set_block(x, s.y, z, GRASS)
            end
        end
    end
    for _, c in L.CELLS do
        if c.x >= ctx.min_x and c.x <= ctx.max_x and c.z >= ctx.min_z and c.z <= ctx.max_z then
            core.set_block(c.x, c.y, c.z, BLOCK[c.b])
        end
    end
    for _, e in EXTRA do
        if e.x >= ctx.min_x and e.x <= ctx.max_x and e.z >= ctx.min_z and e.z <= ctx.max_z then
            core.set_block(e.x, e.y, e.z, e.b)
        end
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

-- Nobody digs or builds the islands away.
core.event.on("core:pre_break", function(ev) ev.cancel = true end)
core.event.on("core:pre_place", function(ev) ev.cancel = true end)

-- ================================================================== the Manta

local NEVER = "skies:never"
local MANTA_KIND = "skies:manta"
--                name        model                                         hp   speed fight     dmg reach every drop n shot doors hover fire  spawn_on
core.register_mob(MANTA_KIND, core.register_model("skies:manta", "manta.gltf"), 400, 6, "worker", 0, 3, 1.0, "", 0, 0, false, 1, false, NEVER)
core.register_mob("skies:squall", core.register_model("skies:squall", "squall.gltf"), 10, 5, "melee", 2, 1.6, 1.2, "", 0, 0, false, 2.5, false, NEVER)
core.mob_sight("skies:squall", 40)

local BASE_HP = 400
local phase = "calm"        -- calm, rods, low, fall, won
local manta = 0
local hp, whole = 0.0, 1.0
local angle, clock = 0.0, 0.0
local mx, my, mz = 0.0, 58.0, 0.0
local spent = 0             -- rods struck in this phase
local gust_wait, gust_at, gust_who = 6.0, 0.0, 0
local squall_wait, touch_wait = 10.0, 0.0
local won_left, empty_for = 0.0, 0.0
local crumble: { number } = {}  -- indexes into L.DECK still to fall, shuffled
local crumble_wait = 0.0
local safe: { [number]: { number } } = {}
local squalls: { [number]: boolean } = {}
local seen: { [number]: boolean } = {}

local function everyone(text: string)
    for _, p in core.players() do core.notice(p, text) end
end

local function rods_back()
    for _, r in RODS do core.set_block(r[1], r[2], r[3], ROD) end
end

local function bridges_back()
    for _, c in L.CELLS do core.set_block(c.x, c.y, c.z, BLOCK[c.b]) end
end

local function spawn_manta()
    if manta ~= 0 then core.mob_remove(manta) end
    manta = core.spawn_mob(MANTA_KIND, 0, 58, 36)
    local n = math.max(1, #core.players())
    whole = BASE_HP * (1 + 0.6 * (n - 1))
    hp = whole
    if manta ~= 0 then core.mob_health(manta, math.floor(hp)) end
end

local function reset()
    for s in squalls do core.mob_remove(s) end
    squalls = {}
    bridges_back()
    rods_back()
    phase, spent, crumble = "calm", 0, {}
    spawn_manta()
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

-- A bolt down the rod and into the Manta.
local function strike(r: { number }, share: number)
    core.set_block(r[1], r[2], r[3], ROD_SPENT)
    core.play_sound("skies:thunder", r[1], r[2], r[3])
    core.play_sound("skies:thunder", mx, my, mz)
    local dmg = whole * share
    hp -= dmg
    if manta ~= 0 then core.mob_hurt(manta, math.ceil(dmg), r[1], r[3]) end
    spent += 1
end

local function hurt_by_rod(p: number, x: number, y: number, z: number)
    local here: { number }? = nil
    for _, r in RODS do
        if r[1] == x and r[2] == y and r[3] == z then here = r end
    end
    if here == nil then return end
    local r = here :: { number }
    local n = #core.players()
    if phase == "calm" then
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

local function to_fall()
    phase, spent = "fall", 0
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
    for _, p in core.players() do
        core.give_item(p, "glider:storm_glider", 1)
        local k = "won:" .. string.lower(core.player_name(p))
        core.storage_set(k, tostring((tonumber(core.storage_get(k)) or 0) + 1))
        core.notice(p, "The Cloud Manta is beaten. The storm glider is yours: hold it to fly on the storm's winds.")
    end
    manta = 0
end

core.event.on("core:used", function(ev)
    if ev.id == ROD or ev.id == ROD_SPENT then
        ev.cancel = true
        hurt_by_rod(ev.player, ev.x, ev.y, ev.z)
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

-- Struck by a player: only on the centre island (phase 2) does it tell.
core.event.on("core:hit", function(ev)
    if ev.target ~= manta or manta == 0 then return end
    if phase == "low" then
        hp -= ev.damage
        if hp <= whole * 0.3 then
            core.mob_health(manta, math.max(1, math.floor(hp)))
            to_fall()
        end
    else
        core.mob_health(manta, math.max(1, math.floor(hp)))
    end
end)

-- Whether a player stands on an island (not on a bridge).
local function on_island(x: number, z: number): boolean
    local i = L.island_at(math.floor(x), math.floor(z))
    return i ~= 0
end

core.event.on("core:tick", function(ev)
    local dt: number = ev.dt
    clock += dt
    local here = core.players()
    if #here == 0 then
        empty_for += dt
        if empty_for > 30 and phase ~= "calm" then reset() end
        return
    end
    empty_for = 0
    if manta == 0 and phase ~= "won" then spawn_manta() end
    for _, p in here do
        if not seen[p] then
            seen[p] = true
            core.player_spawn(p, HOME.x + 0.5, HOME.y + 1, HOME.z + 2.5)
        end
    end

    -- Bodies that fell into the clouds, back where they last stood.
    for _, p in here do
        local x, y, z = core.player_pos(p)
        if x == nil then continue end
        if y < L.CATCH_Y then
            local s = safe[p]
            if s == nil or core.get_block(math.floor(s[1]), math.floor(s[2]) - 1, math.floor(s[3])) == 0 then
                -- Its bridge is gone: the nearest island.
                local best, d2 = HOME, math.huge
                for _, isl in L.ISLANDS do
                    local d = (isl.x - x) ^ 2 + (isl.z - z) ^ 2
                    if d < d2 then best, d2 = isl, d end
                end
                s = { best.x + 0.5, best.y + 1, best.z + 2.5 }
            end
            local t = s :: { number }
            core.player_place(p, t[1], t[2], t[3])
            core.damage(p, 4)
            core.notice(p, "Caught by the wind and set down again.")
        elseif core.get_block(math.floor(x), math.floor(y) - 1, math.floor(z)) ~= 0
            and core.get_block(math.floor(x), math.floor(y) - 1, math.floor(z)) ~= CLOUD then
            safe[p] = { x, y, z }
        end
    end

    if phase == "won" then
        won_left -= dt
        if won_left <= 0 then
            reset()
            everyone("The bridges are back, and the Manta circles again.")
        end
        return
    end

    -- Where the Manta is: high round the islands, or low over the centre.
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
        core.mob_face(manta, mx - math.sin(angle) * 10, mz + math.cos(angle) * 10)
    end

    -- Gusts at whoever is on a bridge, said a moment before.
    if phase == "rods" or phase == "fall" or phase == "calm" then
        gust_wait -= dt
        if gust_who ~= 0 then
            gust_at -= dt
            if gust_at <= 0 then
                local x, _, z = core.player_pos(gust_who)
                if x then
                    local dx, dz = x - mx, z - mz
                    local d = math.max(0.1, math.sqrt(dx * dx + dz * dz))
                    core.player_launch(gust_who, dx / d * 12, 4, dz / d * 12, 0.35)
                    core.play_sound("skies:gust", x, CENTRE.y, z)
                end
                gust_who = 0
            end
        elseif gust_wait <= 0 then
            gust_wait = if phase == "calm" then 12 else 6
            local on_bridge = {}
            for _, p in here do
                local x, _, z = core.player_pos(p)
                if x and not on_island(x, z) then table.insert(on_bridge, p) end
            end
            if #on_bridge > 0 then
                gust_who = on_bridge[math.random(1, #on_bridge)]
                gust_at = 1.5
                core.notice(gust_who, "The Manta draws breath...")
            end
        end
    end

    -- On the centre island: it brushes whoever stands under it, and calls squalls.
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

core.command("manta", "-- how many times you have beaten the Cloud Manta")
core.event.on("core:command", function(ev)
    if ev.name ~= "manta" then return end
    local n = tonumber(core.storage_get("won:" .. string.lower(core.player_name(ev.player)))) or 0
    core.notice(ev.player, if n == 0 then "You have not beaten the Cloud Manta yet." else `Cloud Manta beaten: {n}`)
end)
