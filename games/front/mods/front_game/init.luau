--!strict
-- front_game: the battle on front_core's field, in the manner of large team
-- shooters. Two teams, red from the south base and blue from the north, each
-- with tickets: every death costs its team one, and the team that runs out
-- loses the round. Rifles and machine guns that fire while the trigger is
-- held, a pistol, and grenades that bounce and then blow the ground apart:
-- what an explosion leaves hanging over the hole comes down after it.
--
-- The SERVER decides every shot, as in yafps_game: the player's machine only
-- says when the trigger goes down and up. A shot is tested here against the
-- other team's bodies (core.shoot never hits a player, and would hit friends),
-- so friendly fire is off. Models: tools/make_front_models.py.
--
-- `side = both`: the server runs the first half, a player's machine the
-- second, and a process that is its own server runs both.

local GROUND = 20
local SIZE   = 256
local MID    = 128

-- front_core's places, kept in step with its own copy.
local BASES: { {number} } = { { MID, 24 }, { MID, SIZE - 24 } }   -- red, blue
local TEAM_NAME = { "RED", "BLUE" }

-- The guns, the grenades, the bots and the machines are the battle's
-- (battle.luau, which assault_game shares): this game says what differs.
local battle = require("@self/battle")
local GUNS = battle.GUNS
local TICKETS    = 150

-- The points to take (front_core's), and how: CAPTURE_R nodes round each,
-- a second per ten points of progress for every soldier more than the other
-- team has there, a hundred to take it. A team that holds more of them than
-- the other bleeds the other's tickets.
local POINTS: { {number} } = { { 48, MID }, { MID, MID }, { 208, MID } }
local POINT_NAME = { "A", "B", "C" }
local CAPTURE_R = 10

-- The bots: soldiers that fill each team up to BOTS_PER_TEAM with the players
-- on it. They walk point to point a waypoint at a time (the engine's path
-- search looks at 600 cells), and shoot what they see.
local BOTS_PER_TEAM = 8
-- Come in by a survival site (mods/keys' keys:level): 4 + d / 2 to a team and
-- 100 + 10 d tickets, set by the first to come into an empty field; a round
-- won by a player's team is that site's level done (keys:earned).
local level_d = 0
if core.side() ~= "client" then
    core.event.on("keys:level", function(ev)
        if level_d == 0 then level_d = ev.d end
    end)
end

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local clock = 0

local function short(item: string): string
    return (string.gsub(item, "^front_game:", ""))
end

local asked: (number, string) -> () = function(_, _) end
local tell = battle.tell
local wrap = battle.wrap

-- ===================================================================== server

type Player = battle.Player
type Bot = battle.Bot
type Machine = battle.Machine

if serving then
    core.net.open()

    for name in GUNS do
        core.register_item(`front_game:{name}`, 1, "", 0, 1.0, "")
        core.register_model(`front_game:{name}_held`, `{name}_held.gltf`)
    end
    core.register_item("front_game:grenade", 1, "", 0, 1.0, "")
    local NADE_MODEL = core.register_model("front_game:grenade_held", "grenade_held.gltf")

    local TOUGH: { [number]: boolean } = { [core.node_id("front_core:bedrock")] = true, [core.node_id("front_core:ridge")] = true,
                    [core.node_id("front_core:gallery")] = true, [core.node_id("front_core:pane")] = true,
                    [core.node_id("front_core:map_glass")] = true,
                    [core.node_id("front_core:frame_red")] = true, [core.node_id("front_core:frame_blue")] = true }

    local NADE = "front_game:nade"
    core.ecs.component(NADE, { "vx", "vy", "vz", "t", "owner", "team" })

    local SOLDIER = {
        core.register_model("front_game:soldier_red", "soldier_red.gltf"),
        core.register_model("front_game:soldier_blue", "soldier_blue.gltf"),
    }
    -- Workers: what they do is this mod's, not the engine's. `spawn_on` names a
    -- block no world has, so none comes out of the dark by itself.
    for t, kind in { "soldier_red", "soldier_blue" } do
        --                 name                   model       hp  speed fight     dmg reach every drop n shot doors hover fire  spawn_on
        core.register_mob(`front_game:{kind}`, SOLDIER[t], 20, 3.6, "worker", 0, 1.6, 1.0, "", 1, 0, true, 0, false, "front_game:never")
    end

    -- The battle (battle.luau): its players, bots and machines, and the
    -- hooks below that make it this one.
    local W = battle.server({
        NAME = "front_game", TEAM_NAME = TEAM_NAME, GROUND = GROUND, SIZE = SIZE,
        LOST_Y = GROUND - 15, FAR_Z = SIZE + 20, SHOT_FLOOR = GROUND - 20,
        TOUGH = TOUGH, NADE = NADE, NADE_MODEL = NADE_MODEL,
    })
    local players, bots, M = W.players, W.bots, W.M
    local refill, ammo_line, fire, throw, feet_at, spotted = W.refill, W.ammo_line, W.fire, W.throw, W.feet_at, W.spotted
    local step_nades, bot_throw = W.step_nades, W.bot_throw
    asked = W.asked
    local armed: { [string]: boolean } = {}   -- player name -> true once given the kit
    local tickets: {number} = { TICKETS, TICKETS }
    local over_at = 0    -- while a round is over: when the next starts
    local owner: {number} = { 0, 0, 0 }      -- each point's team, 0 nobody's
    local progress: {number} = { 0, 0, 0 }     -- -100 blue's .. 100 red's

    local function team_count(t: number): number
        local n = 0
        for _, s in players do
            if s.team == t then n += 1 end
        end
        return n
    end

    local function base_spot(t: number): (number, number, number)
        local b = BASES[t]
        local f = if t == 1 then 1 else -1
        return b[1] + 0.5 + math.random(-10, 10), GROUND, b[2] + 0.5 + f * math.random(-4, 8)
    end

    -- Sent home, and sent again for the next few seconds whenever they are
    -- not there: a body that has only just come in has no place yet, and the
    -- engine then puts it at the world's spawn after this has placed it
    -- (step_players asks again while s.home is set).
    local function to_base(p: number, s: Player)
        local x, y, z = base_spot(s.team)
        core.player_spawn(p, x, y, z)
        core.player_place(p, x, y, z, 0, 0, if s.team == 1 then 1 else -1)
        s.home = { x, y, z, clock + 4 }
    end

    local function still_going_home(p: number, s: Player)
        local px, _, pz = core.player_pos(p)
        local h = s.home :: {number}
        if px ~= nil and (px - h[1]) ^ 2 + (pz - h[3]) ^ 2 < 4 then
            if clock > h[4] then s.home = nil end
        else
            core.player_place(p, h[1], h[2], h[3], 0, 0, if s.team == 1 then 1 else -1)
        end
    end

    -- ---------------------------------------------------------------- deaths

    local function lose_ticket(t: number)
        if over_at > 0 then return end
        tickets[t] = math.max(0, tickets[t] - 1)
        tell(0, `tickets {tickets[1]} {tickets[2]}`)
        if tickets[t] == 0 then
            over_at = clock + 10
            for p, s in players do
                if s.team == 3 - t then core.event.emit("keys:earned", { player = p, key = "front" }) end
            end
            tell(0, `msg {TEAM_NAME[3 - t]} WINS THE ROUND`)
            core.log(`front_game: {TEAM_NAME[3 - t]} wins`)
        end
    end

    -- A death costs its team a ticket; a player killed comes back with every
    -- gun full, and not aiming.
    W.on_kill = function(team: number, _by: number) lose_ticket(team) end
    W.player_down = function(p: number, s: Player)
        refill(s)
        M.sights(p, s, false)
    end

    -- ---------------------------------------------------------------- bots

    -- A body's feet are looked for from six under the field (a cellar, a crater).
    W.feet_from = function(): number return GROUND - 6 end

    local bot_n = { 0, 0 }

    local function spawn_bot(team: number)
        local x, _, z = base_spot(team)
        local id = core.spawn_mob(`front_game:{if team == 1 then "soldier_red" else "soldier_blue"}`, x, GROUND, z)
        if id == 0 then return end
        bot_n[team] += 1
        local role = M.ROLES[bot_n[team] % #M.ROLES + 1]
        local ammo = {}
        for _, g in M.PREFER[role] do ammo[g] = { mag = GUNS[g].mag, res = GUNS[g].reserve } end
        bots[id] = { team = team, n = bot_n[team], think_at = 0, shot_at = 0, nade_at = clock + 10,
                     enemy = nil, goal = 1 + bot_n[team] % 3, throwing = 0, role = role, ammo = ammo,
                     gun = M.PREFER[role][1], burst = 0, yaw = if team == 1 then math.pi else 0 }
        M.bot_clip(id, bots[id], "")
    end

    -- The point it is going for: its own unless that is already its team's,
    -- else the nearest that is not; all of them held, it stays on its own.
    local function goal_of(id: number, b: Bot): number
        if owner[b.goal] ~= b.team then return b.goal end
        local x, _, z = core.mob_pos(id)
        local best, at = b.goal, 1e9
        for i, p in POINTS do
            if owner[i] ~= b.team then
                local d = (p[1] - (x or 0)) ^ 2 + (p[2] - (z or 0)) ^ 2
                if d < at then best, at = i, d end
            end
        end
        return best
    end

    -- Where a bot going somewhere goes: its point (b.goal, kept once it is
    -- at the wheel and its mind made up, as think keeps it).
    W.goal = function(id: number, b: Bot, keep: boolean?): {number}
        local g = goal_of(id, b)
        if keep then b.goal = g end
        return POINTS[g]
    end

    local function think(id: number, b: Bot)
        local x, y, z = core.mob_pos(id)
        if x == nil then return end
        if b.ride then
            M.bot_ride_think(id, b)
            return
        end
        -- Not a node further in three seconds of walking: stuck on something
        -- the way round which is longer than the search. A step aside.
        if b.last and (x - b.last[1]) ^ 2 + (z - b.last[2]) ^ 2 < 1 and not b.enemy then
            if clock - b.last[3] > 3 then
                local a = math.random() * 2 * math.pi
                local sx, sz = math.floor(x + math.cos(a) * 10), math.floor(z + math.sin(a) * 10)
                core.mob_goto(id, sx, feet_at(sx, sz), sz)
                b.aside_until = clock + 4
                b.last = { x, z, clock }
                return
            end
        else
            -- It looks where it walks.
            if b.last and not b.enemy then b.yaw = math.atan2(x - b.last[1], -(z - b.last[2])) end
            b.last = { x, z, clock }
        end
        if b.aside_until and clock < b.aside_until then return end

        b.enemy = spotted(id, b)
        if b.enemy then
            local e = b.enemy
            M.face(id, b, e.x, e.z)
            M.bot_clip(id, b, if b.throwing > 0 then "throw" elseif b.hold_fire or b.reload_at then "" else "fire")
            -- Now and then, at a middling distance, a grenade instead: thrown
            -- high enough to come down on them at 45 degrees.
            if clock >= b.nade_at and e.d > 9 and e.d < 26 and math.random() < (if b.role == "rush" then 0.5 else 0.25) then
                b.nade_at = clock + 14
                b.throwing = clock + 0.45
                M.bot_clip(id, b, "throw")
            end
            if b.role == "cover" then
                M.cover(id, b, x, y, z)
                return
            elseif b.role == "rush" then
                -- Head down at them, shooting on the way.
                if e.d > 3 then
                    core.mob_goto(id, math.floor(e.x), feet_at(math.floor(e.x), math.floor(e.z)), math.floor(e.z))
                end
                return
            end
            -- The watch stands and fights; further off than 30 it keeps
            -- going and shoots on the way, but not with the scope.
            if e.d < 30 or b.gun == "sniper" then
                core.mob_goto(id, math.floor(x), math.floor(y), math.floor(z))
                return
            end
        else
            M.bot_clip(id, b, "")
            if b.role == "cover" and M.cover(id, b, x, y, z) then return end
            -- Shot at from where it was not looking: it turns that way.
            if b.alert and clock - b.alert[3] < 2 then
                M.face(id, b, b.alert[1], b.alert[2])
                b.alert = nil
                core.mob_goto(id, math.floor(x), math.floor(y), math.floor(z))
                return
            end
            -- The watch stops every few seconds and looks somewhere else.
            if b.role == "watch" then
                if b.hold_until and clock < b.hold_until then return end
                if clock >= (b.look_at or 0) then
                    b.look_at = clock + 4 + math.random() * 3
                    b.hold_until = clock + 1.6
                    local a = math.random() * 2 * math.pi
                    core.mob_goto(id, math.floor(x), math.floor(y), math.floor(z))
                    M.face(id, b, x + math.sin(a) * 5, z - math.cos(a) * 5)
                    return
                end
            end
            if M.bot_hop(id, b, x, z) then return end
        end
        b.goal = goal_of(id, b)
        local p = POINTS[b.goal]
        -- A waypoint a couple of dozen nodes on, with a little spread so a
        -- squad does not walk in single file; at the point, somewhere in it.
        local gx: number, gz: number = p[1] + (b.n % 5 - 2) * 3, p[2] + (b.n % 3 - 1) * 4
        local dx, dz = gx - x, gz - z
        local d = math.sqrt(dx * dx + dz * dz)
        if d > 24 then
            gx, gz = x + dx / d * 24, z + dz / d * 24
        end
        if not core.mob_going(id) or d > 24 then
            gx, gz = math.floor(gx), math.floor(gz)
            core.mob_goto(id, gx, feet_at(gx, gz), gz)
        end
    end

    local function step_bots()
        -- Who is dead: gone from the engine once its death clip has played.
        for id, b in bots do
            if core.mob_pos(id) == nil then bots[id] = nil end
        end
        local count = { 0, 0 }
        for _, b in bots do
            if not b.dead then count[b.team] += 1 end
        end
        -- Somebody in the world, fighting or watching from the gallery.
        local anybody = #core.players() > 0
        for t = 1, 2 do
            local per = if level_d > 0 then 4 + level_d // 2 else BOTS_PER_TEAM
            local want = if anybody and over_at == 0 then math.max(0, per - team_count(t)) else 0
            if count[t] < want then
                spawn_bot(t)   -- one a tick, so they come out one after another
            elseif count[t] > want then
                for id, b in bots do
                    if b.team == t and not b.dead then
                        core.mob_remove(id)
                        bots[id] = nil
                        break
                    end
                end
            end
        end
        for id, b in bots do
            if b.dead then continue end
            if clock >= b.think_at then
                b.think_at = clock + 0.5 + math.random() * 0.2
                think(id, b)
                local e = b.enemy
                if e then M.seen[b.team][e.key] = { x = e.x, y = e.y, z = e.z, t = clock } end
            end
            if b.throwing > 0 then
                if clock >= b.throwing then
                    b.throwing = 0
                    bot_throw(id, b)
                end
            elseif b.enemy and not b.ride then
                M.bot_trigger(id, b)
            end
        end
    end

    -- ---------------------------------------------------------------- machines
    --
    -- What they are, how they move and fight and how bots drive them is the
    -- battle's (battle.luau); where this field's stand, and how a bot drives
    -- one to its point, are here.

    -- Where each stands at the start and comes back 30 s after it is lost:
    -- red's, and blue's the same mirrored across the middle.
    M.PARK = {
        { "jeep", 112.5, 32.5, 1 }, { "jeep", 116.5, 20.5, 1 }, { "tank", 142.5, 22.5, 1 },
        -- The planes at the west end of their side's airstrip (front_core's
        -- STRIP: x 156..240, z 19..30), each in a lane, nose east along it.
        { "heli", 98.5, 24.5, 1 }, { "plane", 160.5, 27.5, 1, math.pi / 2 },
        { "mortar", 112.5, 13.5, 1 }, { "mortar", 144.5, 13.5, 1 }, { "howitzer", 151.5, 30.5, 1 },
        { "nest", 122.5, 60.5, 0 }, { "nest", 134.5, 60.5, 0 }, { "nest", 80.5, 122.5, 0 },
        { "bomber", 162.5, 22.5, 1, math.pi / 2 }, { "aa", 118.5, 34.5, 1 }, { "aa", 138.5, 34.5, 1 },
    }

    -- The fleet, once there is somebody to see it: entities need a server
    -- that listens.
    -- Each is put down once the ground under it is in memory: a machine made
    -- over a column nobody had loaded yet fell through it and was lost.
    function M.start()
        if M.spawned then return end
        M.spawned = true
        for _, p in M.PARK do
            -- Facing the other side (red +z, blue -z), or along its airstrip
            -- (p[5], the same way for both).
            -- Each comes back half a minute after it is lost (`again`).
            table.insert(M.respawn, { kind = p[1], home = { p[2], p[3], p[5] or math.pi, p[4] }, at = 0, again = true })
            table.insert(M.respawn, { kind = p[1], home = { p[2], SIZE - p[3], p[5] or 0, if p[4] == 0 then 0 else 2 }, at = 0,
                                      again = true })
        end
    end

    -- A bot at the wheel: towards its point, turning as it goes; backing off a
    -- moment when it has not moved for a while; the tank stops to fight what
    -- it sees within 70.
    function M.bot_drive(id: number, m: Machine)
        local pose, b = m.pose :: CraftPose, bots[m.bots[1]]
        m.bot_fwd = 0
        if b == nil or m.goal_x == nil or clock < (m.wait_until or 0) then
            core.craft.keys(id, 0, 0, 0, 0)
            return
        end
        local dx, dz = (m.goal_x :: number) - pose.x, (m.goal_z :: number) - pose.z
        if dx * dx + dz * dz < 64 or (m.kind == "tank" and b.enemy and b.enemy.d < 70) then
            core.craft.keys(id, 0, 0, 0, 0)
            return
        end
        if math.abs(pose.speed) < 0.8 then m.stuck = m.stuck or clock else m.stuck = nil end
        if m.stuck and clock - m.stuck > 2.5 then
            m.back_until, m.stuck = clock + 1.5, nil
        end
        local turn = wrap(math.atan2(dx, -dz) - pose.yaw)
        local fwd, right = if math.abs(turn) > 1.2 then 0.5 else 1, math.clamp(turn * 2, -1, 1)
        if clock < (m.back_until or 0) then fwd, right = -1, -right end
        m.bot_fwd = fwd
        m.want = fwd   -- for M.step's lift out of a crater, as a driver's keys
        core.craft.keys(id, fwd, right, 0, 0)
    end

    -- ---------------------------------------------------------------- points

    local pole_built = false
    local CLOTH = { core.node_id("front_core:red"), core.node_id("front_core:blue") }
    local WHITE = core.node_id("front_core:concrete")
    local POLE = core.node_id("front_core:wall")

    -- The flag on each point's pole, in its owner's colour.
    local function flag(i: number)
        local p = POINTS[i]
        local x, z = p[1], p[2]
        local y = feet_at(x, z)
        if not pole_built then
            for k = 0, 6 do core.set_block(x, y + k, z, POLE) end
        end
        local cloth = if owner[i] == 0 then WHITE else CLOTH[owner[i]]
        for k = 4, 6 do
            core.set_block(x + 1, y + k, z, cloth)
            core.set_block(x + 2, y + k, z, cloth)
        end
    end

    local points_at, bleed_at, keep_at = 0, 0, 0

    local function points_line(): string
        return `points {owner[1]} {owner[2]} {owner[3]} {math.floor(progress[1])} {math.floor(progress[2])} {math.floor(progress[3])}`
    end

    local function step_points()
        if clock < points_at then return end
        points_at = clock + 1
        -- The whole field stays in memory while anybody fights on it: a bot
        -- at the far base walks on ground nobody is near.
        if clock >= keep_at and next(bots) ~= nil then
            keep_at = clock + 20
            for _, x in { 48, 144, 240 } do
                for _, z in { 48, 144, 240 } do core.keep_loaded(x, z, 48) end
            end
        end
        if not pole_built then
            for i = 1, 3 do
                if core.loaded(POINTS[i][1], GROUND, POINTS[i][2]) then flag(i) end
            end
            pole_built = core.loaded(POINTS[1][1], GROUND, POINTS[1][2]) and core.loaded(POINTS[3][1], GROUND, POINTS[3][2])
        end
        local near = {}
        for i = 1, 3 do near[i] = { 0, 0 } end
        local function count(x: number, z: number, team: number)
            for i, p in POINTS do
                if (x - p[1]) ^ 2 + (z - p[2]) ^ 2 <= CAPTURE_R * CAPTURE_R then
                    near[i][team] += 1
                end
            end
        end
        for p, s in players do
            local x, _, z = core.player_pos(p)
            if x ~= nil and core.health(p) > 0 then count(x, z, s.team) end
        end
        for id, b in bots do
            local x, _, z = core.mob_pos(id)
            if x ~= nil and not b.dead then count(x, z, b.team) end
        end
        local changed = false
        for i = 1, 3 do
            local diff = near[i][1] - near[i][2]
            if diff ~= 0 then
                local before = progress[i]
                progress[i] = math.clamp(progress[i] + 10 * math.clamp(diff, -3, 3), -100, 100)
                local was = owner[i]
                if progress[i] >= 100 then owner[i] = 1
                elseif progress[i] <= -100 then owner[i] = 2
                elseif (was == 1 and progress[i] <= 0) or (was == 2 and progress[i] >= 0) then owner[i] = 0 end
                if owner[i] ~= was then
                    flag(i)
                    if owner[i] > 0 then
                        tell(0, `msg {TEAM_NAME[owner[i]]} TAKES {POINT_NAME[i]}`)
                    end
                    core.log(`front_game: point {POINT_NAME[i]} now {owner[i]}`)
                end
                changed = changed or progress[i] ~= before
            end
        end
        if changed then tell(0, points_line()) end
        -- More points than the other team: their tickets go.
        local held = { 0, 0 }
        for i = 1, 3 do
            if owner[i] > 0 then held[owner[i]] += 1 end
        end
        if held[1] ~= held[2] and clock >= bleed_at then
            bleed_at = clock + (if math.max(held[1], held[2]) == 3 then 2 else 4)
            lose_ticket(if held[1] > held[2] then 2 else 1)
        end
    end

    -- ---------------------------------------------------------------- the tick

    local tags_at = 0

    -- The gallery (front_core): a room on the west ridge, 16 over the field,
    -- where the way in from the sandbox comes out. Whoever is in it watches
    -- and is nobody's; its red and blue portals lead to the bases, and whoever
    -- comes out of one fights for that base. Back through a base's portal is
    -- back to watching.
    local GALLERY_SPOT = { -3.5, GROUND + 16, MID }
    local function in_gallery(p: number): boolean
        local x, y, z = core.player_pos(p)
        return x ~= nil and x < 3 and y > GROUND + 10 and math.abs(z - MID) < 12
    end
    local watching: { [number]: boolean } = {}   -- player -> true once seen in the gallery

    local function step_players(dt: number)
        local here = {}
        for _, p in core.players() do
            here[p] = true
            local s = players[p]
            if s and in_gallery(p) then
                M.leave(p, true)
                M.gone(p)
                players[p] = nil
                s = nil
                core.player_spawn(p, 0, 0, 0, false)
                tell(p, "team 0")
                tell(p, "msg YOU WATCH: RED PORTAL OR BLUE PORTAL TO FIGHT")
            end
            if s == nil then
                if core.player_pos(p) == nil then continue end   -- not placed yet
                if in_gallery(p) then
                    if not watching[p] then
                        watching[p] = true
                        tell(p, "team 0")
                        tell(p, `tickets {tickets[1]} {tickets[2]}`)
                        tell(p, points_line())
                        tell(p, "msg YOU WATCH: RED PORTAL OR BLUE PORTAL TO FIGHT")
                    end
                    continue
                end
                if not watching[p] then
                    -- Come in somewhere else (the world's spawn): up to the
                    -- gallery first, until the body is there.
                    core.player_place(p, GALLERY_SPOT[1], GALLERY_SPOT[2], GALLERY_SPOT[3], 1, 0, 0)
                    continue
                end
                -- Out of a portal: the base it came out at is its side.
                local _, _, pz = core.player_pos(p)
                local team = if pz < MID then 1 else 2
                s = { team = team, kills = 0, held = false, next_fire = 0, dry = false, keys = { 0, 0, 0 },
                      throw_at = 0, reload_at = 0, reloading = "", supply_at = 0 } :: Player
                refill(s)
                players[p] = s
                local name = core.player_name(p)
                if not armed[name] then
                    armed[name] = true
                    for _, item in { "rifle", "lmg", "pistol", "grenade", "launcher", "sniper" } do
                        core.give_item(p, `front_game:{item}`, 1)
                    end
                end
                core.player_spawn(p, base_spot(team))
                tell(p, `team {team}`)
                tell(p, ammo_line(s))
                tell(p, `tickets {tickets[1]} {tickets[2]}`)
                tell(p, points_line())
                tell(p, `msg YOU FIGHT FOR {TEAM_NAME[team]}`)
                core.log(`front_game: {name} joins {TEAM_NAME[team]}`)
            end
            if s == nil then continue end
            if s.home then
                still_going_home(p, s)
            end
            M.pose(p, s)
            M.launcher_lock(p, s, dt)
            if s.held and clock >= s.next_fire then
                fire(p, s)
            end
            if s.held2 and M.seat_of[p] then
                M.fire(p, s, true)
            end
            if s.throw_at > 0 and clock >= s.throw_at then
                s.throw_at = 0
                throw(p, s)
            end
            if s.reload_at > 0 and clock >= s.reload_at then
                local item, g = s.reloading, GUNS[s.reloading]
                local n = math.min(g.mag - s.mag[item], s.res[item])
                s.mag[item] += n
                s.res[item] -= n
                s.reload_at = 0
                tell(p, ammo_line(s))
            end
            -- At home, the base's crates top everything up.
            if clock >= s.supply_at then
                s.supply_at = clock + 2
                local x, _, z = core.player_pos(p)
                local b = BASES[s.team]
                if x ~= nil and math.abs(x - b[1]) < 20 and math.abs(z - b[2]) < 14 then
                    local changed = false
                    for name, g in GUNS do
                        if s.res[name] < g.reserve then
                            s.res[name] = math.min(g.reserve, s.res[name] + g.mag)
                            changed = true
                        end
                    end
                    if s.nades < battle.NADES then
                        s.nades += 1
                        changed = true
                    end
                    if changed then tell(p, ammo_line(s)) end
                end
            end
        end
        for p in players do
            if not here[p] then
                players[p] = nil
                M.gone(p)
            end
        end
        for p in watching do
            if not here[p] then watching[p] = nil end
        end

        -- Where everybody else is and for whom, for the marks over their heads.
        if clock >= tags_at then
            tags_at = clock + 0.25
            local parts = {}
            for p, s in players do
                local x, y, z = core.player_pos(p)
                if x ~= nil and core.health(p) > 0 then
                    parts[p] = string.format("%.1f,%.1f,%.1f,%d", x, y, z, s.team :: number)
                end
            end
            for p in players do
                local mine = {}
                for q, part in parts do
                    if q ~= p then table.insert(mine, part) end
                end
                tell(p, "tags " .. table.concat(mine, " "))
            end
        end
    end

    local function step_round()
        if over_at > 0 and clock >= over_at then
            over_at = 0
            if next(players) == nil then level_d = 0 end
            local t = if level_d > 0 then 100 + 10 * level_d else TICKETS
            tickets = { t, t }
            owner, progress = { 0, 0, 0 }, { 0, 0, 0 }
            for i = 1, 3 do flag(i) end
            tell(0, `tickets {tickets[1]} {tickets[2]}`)
            tell(0, points_line())
            tell(0, "msg A NEW ROUND")
            for p, s in players do
                refill(s)
                to_base(p, s)
                core.heal(p, 20)
                tell(p, ammo_line(s))
            end
        end
    end

    function server_step(dt: number)
        W.clock = clock
        step_players(dt)
        M.step(dt)
        step_bots()
        step_points()
        step_nades(dt)
        step_round()
    end
end

-- ===================================================== the player's machine

-- What a player's machine knows of the seat it is in, and the rest it keeps (V, below).
type View = {
    seat: { kind: string, n: number, role: string, gun: string, seats: number }?, keys: {number},
    held: { [string]: boolean }, mark: {number}?, mark_left: number,
    mach: { hp: number, max: number, speed: number, alt: number, throttle: number, wait: number }?,
    burns: { { x: number, y: number, z: number, left: number } },
    pred: { id: number, craft: string, n: {number}, on: boolean }?,
    lock: { on: boolean, k: number, x: number, y: number, z: number, left: number }?,
    aimed: string?, reloading: string?, wheel: boolean?, me: number?, aiming: boolean?, view_before: number?,
    clip: string?, aim_at: number?, aim: {number}?, aim_sent: number?,
}

if drawing then
    local tracers: { { o: {number}, e: {number}, left: number, hit: boolean } } = {}   -- { o, e, left, hit }
    local booms: { { x: number, y: number, z: number, r: number, left: number } } = {}   -- { x, y, z, r, left }
    local feed: { { killer: string, kt: number, victim: string, vt: number, weapon: string, left: number } } = {}   -- { text, team, left }
    local tags: { { x: number, y: number, z: number, team: number } } = {}   -- { x, y, z, team }
    local ammo: { [string]: {number} } = { pistol = { 0, 0 }, rifle = { 0, 0 }, lmg = { 0, 0 }, launcher = { 0, 0 }, sniper = { 0, 0 } }
    local nades, kills = 0, 0
    local team    = 0
    local tickets: {number} = { 0, 0 }
    local banner, banner_left = "", 0
    local marker, marker_left = 0, 0
    local reload_left, dry_left = 0, 0
    local hand = ""
    local COLOUR: { {number} } = { { 230, 60, 50 }, { 60, 120, 240 } }
    -- In a machine: which, which seat, what it fires; the keys held; what the
    -- server last said of it; where a lobbing gun would land.
    local V: View = { seat = nil, keys = { 0, 0, 0 }, held = {}, mach = nil, mark = nil, mark_left = 0, burns = {} }

    battle.heard = function(text: string)
        local w = string.split(text, " ")
        local what = w[1]
        if what == "shot" and #w == 8 then
            table.insert(tracers, {
                o = { tonumber(w[2]) :: number, tonumber(w[3]) :: number, tonumber(w[4]) :: number },
                e = { tonumber(w[5]) :: number, tonumber(w[6]) :: number, tonumber(w[7]) :: number },
                left = 0.06, hit = w[8] == "1",
            })
        elseif what == "boom" then
            table.insert(booms, { x = tonumber(w[2]) :: number, y = tonumber(w[3]) :: number, z = tonumber(w[4]) :: number,
                                  r = tonumber(w[5]) or 2.5, left = 1.6 })
        elseif what == "fired" then
            -- Aimed, the kick from the sight line and not from the hip.
            core.arms_clip(if V.aimed == w[2] then `fire_{w[2]}_aim` else `fire_{w[2]}`)
        elseif what == "reload" then
            reload_left = (GUNS[w[2]] and GUNS[w[2]].reload) or 1.5
            V.reloading = w[2]
        elseif what == "ammo" then
            ammo.launcher = { tonumber(w[10]) or 0, tonumber(w[11]) or 0 }
            ammo.sniper = { tonumber(w[12]) or 0, tonumber(w[13]) or 0 }
            ammo.pistol = { tonumber(w[2]) or 0, tonumber(w[3]) or 0 }
            ammo.rifle = { tonumber(w[4]) or 0, tonumber(w[5]) or 0 }
            ammo.lmg = { tonumber(w[6]) or 0, tonumber(w[7]) or 0 }
            nades = tonumber(w[8]) or 0
            kills = tonumber(w[9]) or kills
            reload_left = 0
        elseif what == "team" then
            team = tonumber(w[2]) or 0
        elseif what == "tickets" then
            tickets = { tonumber(w[2]) or 0, tonumber(w[3]) or 0 }
        elseif what == "hit" then
            marker, marker_left = tonumber(w[2]) or 0, 0.25
        elseif what == "kill" then
            table.insert(feed, 1, { killer = w[3], kt = tonumber(w[2]) or 0, victim = w[5],
                                    vt = tonumber(w[4]) or 0, weapon = string.upper(w[6] or ""), left = 6 })
            if #feed > 5 then table.remove(feed) end
        elseif what == "tags" then
            tags = {}
            for i = 2, #w do
                local f = string.split(w[i], ",")
                if #f == 4 then
                    table.insert(tags, { x = tonumber(f[1]) or 0, y = tonumber(f[2]) or 0,
                                         z = tonumber(f[3]) or 0, team = tonumber(f[4]) or 0 })
                end
            end
        elseif what == "msg" then
            banner, banner_left = table.concat(w, " ", 2), 3
        elseif what == "dry" then
            dry_left = 1.0
        elseif what == "seat" then
            -- This machine's own body seated too, as the server shows it to
            -- everybody else.
            V.pred = nil
            if w[2] == "none" then
                V.seat, V.mach, V.mark, V.lock = nil, nil, nil, nil
                V.held = {}
                core.view(0)
                core.player_clip(1, "")
            else
                V.seat = { kind = w[2], n = tonumber(w[3]) or 1, role = w[4], gun = w[5], seats = tonumber(w[7]) or 1 }
                core.view(tonumber(w[6]) or 0)
                V.wheel = w[8] == "wheel"
                if w[4] == "drive" and w[9] then
                    local n = {}
                    for j = 11, 17 do n[j - 10] = tonumber(w[j]) or 1 end
                    V.pred = { id = tonumber(w[9]) or 0, craft = w[10], n = n, on = false }
                end
                core.player_clip(1, if w[8] == "stand" then "" else "drive_idle")
            end
        elseif what == "mach" then
            V.mach = { hp = tonumber(w[2]) or 0, max = tonumber(w[3]) or 1, speed = tonumber(w[4]) or 0,
                       alt = tonumber(w[5]) or 0, throttle = tonumber(w[6]) or 0, wait = tonumber(w[7]) or 0 }
        elseif what == "mark" then
            V.mark = { tonumber(w[2]) or 0, tonumber(w[3]) or 0, tonumber(w[4]) or 0 }
            V.mark_left = 0.6
        elseif what == "lock" then
            V.lock = if w[2] == "none" then nil else { on = w[2] == "1", k = tonumber(w[3]) or 0,
                x = tonumber(w[4]) or 0, y = tonumber(w[5]) or 0, z = tonumber(w[6]) or 0, left = 0.4 }
        elseif what == "burn" then
            table.insert(V.burns, { x = tonumber(w[2]) or 0, y = tonumber(w[3]) or 0, z = tonumber(w[4]) or 0, left = 4.5 })
        elseif what == "dust" then
            table.insert(booms, { x = tonumber(w[2]) :: number, y = tonumber(w[3]) :: number, z = tonumber(w[4]) :: number,
                                  r = 1.2, left = 1.6 })
        end
    end

    if not serving then
        core.event.on("core:net_message", function(ev)
            battle.heard(ev.text :: string)
        end)
    end

    local function ask(what: string, player: number)
        if serving then
            asked(player, what)
        else
            core.net.send(0, what)
        end
    end

    -- In a machine WASD, space and shift are its driver's (forward, right,
    -- up), said to the server when they change; 1 to 9 change seat.
    local DRIVE_KEYS: { [string]: {number} } = { key_w = { 1, 1 }, key_s = { 1, -1 }, key_d = { 2, 1 }, key_a = { 2, -1 },
                         key_space = { 3, 1 }, key_shift = { 3, -1 } }

    core.event.on("core:input", function(ev)
        V.me = ev.player
        local a = ev.action
        if a == "mouse_left" then
            ev.cancel = true
            ask("fire1", ev.player)
        elseif a == "mouse_left_up" then
            ask("fire0", ev.player)
        elseif a == "mouse_right" then
            -- On foot the right button aims down the sights; in a machine it
            -- is the second weapon (a plane's bombs).
            ev.cancel = true
            V.aiming = V.seat == nil
            ask(if V.seat then "fire2" else "aim1", ev.player)
        elseif a == "mouse_right_up" then
            V.aiming = false
            ask(if V.seat then "fire2up" else "aim0", ev.player)
        elseif a == "key_r" then
            ev.cancel = true
            ask("reload", ev.player)
        elseif a == "key_f" then
            ev.cancel = true
            ask("use", ev.player)
        elseif V.seat and string.match(a, "^key_%d$") then
            ev.cancel = true
            ask(`seat {string.sub(a, 5)}`, ev.player)
        elseif V.seat then
            local up = string.sub(a, -3) == "_up"
            local key = if up then string.sub(a, 1, -4) else a
            local k = DRIVE_KEYS[key]
            if k then
                V.held[key] = not up
                local keys = { 0, 0, 0 }
                for name, held in V.held do
                    local d = DRIVE_KEYS[name]
                    if held then keys[d[1]] += d[2] end
                end
                if keys[1] ~= V.keys[1] or keys[2] ~= V.keys[2] or keys[3] ~= V.keys[3] then
                    if V.wheel and keys[2] ~= V.keys[2] then
                        core.player_clip(1, if keys[2] > 0 then "drive_right" elseif keys[2] < 0 then "drive_left" else "drive_idle")
                    end
                    V.keys = keys
                    ask(`k {keys[1]} {keys[2]} {keys[3]}`, ev.player)
                end
            end
        end
    end)

    -- Tracers as rows of small boxes; a blast as a flash, a ball of fire
    -- that grows and goes, and smoke that rises slower.
    core.render.pass("front_game:fx", 100, function()
        for _, t in tracers do
            local dx, dy, dz = t.e[1] - t.o[1], t.e[2] - t.o[2], t.e[3] - t.o[3]
            local len = math.sqrt(dx * dx + dy * dy + dz * dz)
            local d = 1.5
            while d < len do
                local f = d / len
                core.render.box(t.o[1] + dx * f, t.o[2] + dy * f, t.o[3] + dz * f, 0.04, 0.04, 0.04, 255, 220, 130, 255)
                d += 1.1
            end
            if t.hit then
                core.render.box(t.e[1], t.e[2], t.e[3], 0.15, 0.15, 0.15, 200, 20, 10, 255)
            end
        end
        for _, b in booms do
            local age = 1.6 - b.left
            if age < 0.35 then
                local k = age / 0.35
                local r = b.r * (0.4 + 1.2 * k)
                for i = 0, 13 do
                    local a = i * math.pi / 7
                    for _, h in { -0.7, -0.2, 0.3, 0.8 } do
                        local s = math.sqrt(1 - h * h)
                        local hot = math.floor(255 * (1 - k))
                        core.render.box(b.x + math.cos(a) * r * s, b.y + h * r, b.z + math.sin(a) * r * s,
                                        0.35, 0.35, 0.35, 255, 120 + hot // 2, 30, 255)
                    end
                end
            end
            -- Smoke: grey cubes rising and spreading, a few of them.
            for i = 0, 9 do
                local a = i * 2.4
                local up = age * (1.2 + (i % 3) * 0.5)
                local out = b.r * 0.4 + age * 0.8
                local g = math.floor(70 + 60 * math.min(1, age))
                local size = 0.3 + age * 0.35
                core.render.box(b.x + math.cos(a) * out, b.y + up, b.z + math.sin(a) * out,
                                size, size, size, g, g, g, 255)
            end
        end
        -- A wreck burning: flames flickering at its foot, dark smoke rising
        -- and spreading, both dying down as it sinks.
        for _, b in V.burns do
            local k = math.min(1, b.left / 3)
            for i = 0, 7 do
                local a = i * 0.8 + b.left * 3
                local r = 1.2 + math.sin(b.left * 7 + i) * 0.4
                local s = (0.25 + 0.15 * math.sin(b.left * 11 + i * 2)) * k
                core.render.box(b.x + math.cos(a) * r, b.y + 0.4 + (i % 3) * 0.3, b.z + math.sin(a) * r,
                                s, s * 1.4, s, 255, 120 + (i * 17) % 100, 20, 255)
            end
            for i = 0, 11 do
                local rise = ((4.5 - b.left) * 1.5 + i * 0.7) % 8
                local spread = 0.6 + rise * 0.25
                local a = i * 2.39
                local g = math.floor(40 + rise * 8)
                local s = (0.35 + rise * 0.08) * k
                core.render.box(b.x + math.cos(a) * spread, b.y + 1 + rise, b.z + math.sin(a) * spread,
                                s, s, s, g, g, g, 255)
            end
        end
        -- Where a mortar's or a howitzer's shell would come down: a red post.
        if V.mark and V.mark_left > 0 then
            for k = 0, 6 do
                core.render.box(V.mark[1], V.mark[2] + k * 0.8, V.mark[3], 0.15, 0.3, 0.15, 255, 40, 30, 255)
            end
        end
        -- The anti-air's lock: corners round what it holds, yellow closing
        -- in while it locks, red once it has.
        if V.lock and V.lock.left > 0 then
            local L = V.lock
            local r = 4 - 1.5 * L.k
            local c = if L.on then { 255, 40, 30 } else { 255, 220, 40 }
            for _, sx in { -1, 1 } do
                for _, sy in { -1, 1 } do
                    core.render.box(L.x + sx * r, L.y + sy * r, L.z, 0.5, 0.08, 0.08, c[1], c[2], c[3], 255)
                    core.render.box(L.x + sx * r, L.y + sy * r, L.z, 0.08, 0.5, 0.08, c[1], c[2], c[3], 255)
                end
            end
        end
        -- Over every other player, their team's colour: a diamond of two boxes.
        for _, t in tags do
            local c = COLOUR[t.team] or { 200, 200, 200 }
            core.render.box(t.x, t.y + 2.15, t.z, 0.12, 0.12, 0.12, c[1], c[2], c[3], 255)
        end
    end)

    local GUN_NAME: { [string]: string } = { mg = "MACHINE GUN", cannon = "CANNON", rockets = "ROCKETS", plane = "GUNS / BOMBS",
                       mortar = "MORTAR", howitzer = "HOWITZER", bomber = "BOMB BAY", aa = "AA MISSILES" }

    local function weapon_line(): (string, boolean)
        if V.seat then
            local name = GUN_NAME[V.seat.gun]
            if name == nil then return if V.seat.role == "drive" then "DRIVER" else "PASSENGER", true end
            local wait = V.mach and V.mach.wait or 0
            if wait > 0.4 then return `{name}   {string.format("%.1f", wait)}`, false end
            return name, true
        end
        local gun = short(hand)
        if gun == "grenade" then
            return `GRENADES   {nades}`, nades > 0
        end
        local a = ammo[gun]
        if a == nil then return "", true end
        if reload_left > 0 then return `{string.upper(gun)}   RELOADING`, false end
        return `{string.upper(gun)}   {a[1]} / {a[2]}`, a[1] > 0
    end

    core.hud.pass("front_game:hud", 100, function()
        local asp = core.hud.aspect()
        local c = { 230, 240, 230, 230 }
        local g = GUNS[short(hand)]
        if V.aimed == "sniper" then
            -- Through the scope: black round a circle (a strip per row), the
            -- crosshair's thin lines across it, a red dot in the middle.
            local r, rows = 0.42, 90
            for i = 0, rows - 1 do
                local y = (i + 0.5) / rows
                local dy = (y - 0.5) / r
                local half = if math.abs(dy) < 1 then math.sqrt(1 - dy * dy) * r / asp else 0
                local left = 0.5 - half   -- of the width; a rect's sizes are of the height
                core.hud.rect(left / 2, y, left * asp, 1 / rows + 0.001, 0, 0, 0, 255)
                core.hud.rect(1 - left / 2, y, left * asp, 1 / rows + 0.001, 0, 0, 0, 255)
            end
            core.hud.rect(0.5, 0.5, 2 * r, 0.0015, 0, 0, 0, 230)
            core.hud.rect(0.5, 0.5, 0.0015, 2 * r, 0, 0, 0, 230)
            core.hud.rect(0.5, 0.5, 0.005, 0.005, 230, 30, 20, 255)
        elseif V.aimed ~= nil then
            core.hud.rect(0.5, 0.5, 0.003, 0.003, c[1], c[2], c[3], c[4])   -- aimed: a dot
        else
            -- From the hip the ticks stand as far out as the rounds go.
            local gap = 0.012 + (if g then g.spread else 0.03) * 0.5
            core.hud.rect(0.5, 0.5, 0.003, 0.003, c[1], c[2], c[3], c[4])
            core.hud.rect(0.5 - gap / asp, 0.5, 0.012, 0.0025, c[1], c[2], c[3], c[4])
            core.hud.rect(0.5 + gap / asp, 0.5, 0.012, 0.0025, c[1], c[2], c[3], c[4])
            core.hud.rect(0.5, 0.5 - gap, 0.0025, 0.012, c[1], c[2], c[3], c[4])
            core.hud.rect(0.5, 0.5 + gap, 0.0025, 0.012, c[1], c[2], c[3], c[4])
        end
        if marker_left > 0 then
            -- A hit: white; a head: yellow; a kill: red.
            local m = if marker == 2 then { 255, 50, 40 } elseif marker == 1 then { 255, 220, 60 } else { 255, 255, 255 }
            for _, o in { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } } do
                core.hud.rect(0.5 + o[1] * 0.012 / asp, 0.5 + o[2] * 0.012, 0.008, 0.008, m[1], m[2], m[3], 255)
            end
        end

        local line, ok = weapon_line()
        if line ~= "" then
            local tr, tg, tb = 230, 240, 230
            if not ok or dry_left > 0 then tr, tg, tb = 255, 80, 60 end
            core.hud.text(0.97, 0.9, line, 0.042, 2, tr, tg, tb, 255)
        end
        core.hud.text(0.97, 0.95, `GRENADES {nades}   KILLS {kills}`, 0.026, 2, 190, 200, 190, 220)

        -- In a machine: what it is, how much of it is left, how it goes.
        if (V.seat and V.seat.gun == "aa") or (not V.seat and V.lock and V.lock.left > 0) then
            local L = V.lock
            local text = if L == nil or L.left <= 0 then "NO TARGET" elseif L.on then "LOCKED -- FIRE"
                else string.format("LOCKING %d%%", math.floor(L.k * 100))
            local lc = if L and L.left > 0 and L.on then { 255, 60, 40 } else { 255, 220, 60 }
            core.hud.text(0.5, 0.58, text, 0.028, 1, lc[1], lc[2], lc[3], 255)
        end
        if V.seat then
            local m = V.mach
            local x0 = 0.03
            core.hud.text(x0, 0.82, `{string.upper(V.seat.kind)}   SEAT {V.seat.n}/{V.seat.seats}   F: OUT   1-{V.seat.seats}: SEAT`,
                          0.024, 0, 220, 230, 220, 255)
            if m then
                local k = math.clamp(m.hp / math.max(1, m.max), 0, 1)
                core.hud.rect(x0 + 0.1 / core.hud.aspect(), 0.87, 0.2, 0.018, 40, 40, 40, 200)
                core.hud.rect(x0 + 0.1 * k / core.hud.aspect(), 0.87, 0.2 * k, 0.018, math.floor(255 * (1 - k)), math.floor(200 * k + 40), 40, 255)
                local info = string.format("%d KM/H", math.floor(m.speed * 3.6))
                if V.seat.kind == "heli" or V.seat.kind == "plane" or V.seat.kind == "bomber" then
                    info ..= string.format("   ALT %d", math.floor(m.alt))
                end
                if V.seat.kind == "plane" or V.seat.kind == "bomber" then
                    info ..= string.format("   THROTTLE %d%%", math.floor(m.throttle * 100))
                end
                core.hud.text(x0, 0.91, info, 0.026, 0, 220, 230, 220, 255)
            end
        end

        -- Tickets: red left, blue right, ours first in bright.
        core.hud.text(0.44, 0.04, `RED {tickets[1]}`, 0.04, 2, 240, 70, 60, 255)
        core.hud.text(0.56, 0.04, `{tickets[2]} BLUE`, 0.04, 0, 80, 130, 250, 255)
        if team > 0 then
            local tc = COLOUR[team]
            core.hud.text(0.5, 0.085, TEAM_NAME[team], 0.026, 1, tc[1], tc[2], tc[3], 255)
        end

        for i, f in feed do
            local a = math.floor(255 * math.min(1, f.left))
            local kc, vc = COLOUR[f.kt] or { 220, 220, 220 }, COLOUR[f.vt] or { 220, 220, 220 }
            local y = 0.05 + (i - 1) * 0.032
            core.hud.text(0.75, y, f.killer, 0.024, 2, kc[1], kc[2], kc[3], a)
            core.hud.text(0.76, y, f.weapon, 0.02, 0, 200, 200, 200, a)
            core.hud.text(0.86, y, f.victim, 0.024, 0, vc[1], vc[2], vc[3], a)
        end

        if banner_left > 0 then
            core.hud.text(0.5, 0.3, banner, 0.06, 1, 255, 230, 160, math.floor(255 * math.min(1, banner_left * 2)))
        end
    end)

    -- The machine this player drives, run here as well (core.craft.make on a
    -- client): started once the engine has the seat, then given the keys
    -- M.drive gives it on the server, so it moves before the server says so.
    local function drive_here()
        local d = V.pred
        if d == nil then return end
        if not d.on then
            d.on = core.craft.make(d.id, d.craft, d.n[1], d.n[2], d.n[3], d.n[4], d.n[5], d.n[6], d.n[7])
            if not d.on then return end
        end
        local k = V.keys
        if d.craft == "wheeled" or d.craft == "tracked" then
            core.craft.keys(d.id, k[1], k[2], 0, 0)
            return
        end
        local pose = core.craft.pose(d.id)
        local dx, dy, dz = core.player_look(1)
        if pose == nil or dx == nil then return end
        local turn = wrap(math.atan2(dx, -dz) - pose.yaw)
        if d.craft == "rotor" then
            core.craft.keys(d.id, k[1], k[2], k[3], math.clamp(turn * 1.5, -1, 1))
        else
            local bank = math.clamp(turn * 1.2, -1, 1) * 0.9 + k[2] * 0.5
            local pitch = math.asin(math.clamp(dy, -1, 1))
            core.craft.keys(d.id, math.clamp(-(pitch - pose.pitch) * 2.5, -1, 1),
                            math.clamp((bank - pose.roll) * 2, -1, 1), k[1], math.clamp(turn, -1, 1) * 0.5)
        end
    end

    function client_step(dt: number)
        drive_here()
        for _, list in { tracers, booms } do
            for i = #list, 1, -1 do
                list[i].left -= dt
                if list[i].left <= 0 then table.remove(list, i) end
            end
        end
        for i = #feed, 1, -1 do
            feed[i].left -= dt
            if feed[i].left <= 0 then table.remove(feed, i) end
        end
        banner_left -= dt
        marker_left -= dt
        V.mark_left -= dt
        if V.lock then V.lock.left -= dt end

        -- Down the sights while the right button is held and the hand has a
        -- gun: its zoom, and through the sniper's scope from the eye whatever
        -- the camera was (put back after). And what this body's arms do,
        -- one clip for the arms seen in first person and the body seen from
        -- behind (tools/make_front_models.py, make_front_poses.py): a
        -- reload, the sights, or the gun held ready.
        local gun = short(hand)
        local g = GUNS[gun]
        local aimed = if V.aiming and V.seat == nil and g ~= nil and g.zoom then gun else nil
        if aimed ~= V.aimed then
            V.aimed = aimed
            core.zoom(if aimed then g.zoom else 1)
            if aimed and g.scope then
                V.view_before = core.view(0)
            elseif V.view_before then
                core.view(V.view_before)
                V.view_before = nil
            end
        end
        if V.seat == nil then
            local clip = if g == nil then ""
                elseif reload_left > 0 and V.reloading == gun then `reload_{gun}`
                elseif aimed then `aim_{gun}`
                else `hold_{gun}`
            if clip ~= V.clip then
                V.clip = clip
                core.player_clip(1, clip)
            end
        else
            V.clip = nil
        end

        -- The crosshair: the camera's ray through the middle of the screen.
        -- The server finds what it is on, and puts shots there from the eye
        -- and turrets, barrels and guns on it.
        if clock >= (V.aim_at or 0) then
            -- The ray itself and not where it ends: the server meets it with
            -- every body as well as the blocks, which this side does not know.
            local ox, oy, oz, dx, dy, dz = core.cursor_ray()
            if ox ~= nil then
                local was = V.aim or { 0, 0, 0, 0, 0, 0 }
                local moved = (ox - was[1]) ^ 2 + (oy - was[2]) ^ 2 + (oz - was[3]) ^ 2 > 0.0025
                    or (dx - was[4]) ^ 2 + (dy - was[5]) ^ 2 + (dz - was[6]) ^ 2 > 1e-6
                if moved or clock >= (V.aim_sent or 0) + 0.5 then
                    V.aim, V.aim_sent = { ox, oy, oz, dx, dy, dz }, clock
                    ask(string.format("aim %.2f %.2f %.2f %.4f %.4f %.4f", ox, oy, oz, dx, dy, dz), V.me or 1)
                end
            end
            V.aim_at = clock + 0.05
        end
        dry_left -= dt
        reload_left -= dt

        local now = core.held_item(1)
        if now ~= hand then
            hand = now
            if string.find(hand, "^front_game:") then
                core.arms_clip(`raise_{short(hand)}`)
            end
        end
    end
end

function on_step(dt: number)
    clock += dt
    if serving then
        server_step(dt)
    end
    if drawing then
        client_step(dt)
    end
end
