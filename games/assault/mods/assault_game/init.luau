-- assault_game: the assault on Basoalth's dimension (assault_core's valley),
-- played together against his legion. Phases, one after another:
--
--   1-4  the strongholds: the outpost, the bastion, the battery and the
--        fortress, each held by a garrison (more for more players) with its
--        guns and machines. A stronghold is taken by standing at its flag
--        with none of its garrison near; then the camp moves there (where the
--        dead come back, and the crates top up ammunition) and the next one
--        is manned. The fortress's gate has to be blown in.
--   5    the hall: four pylons feed a shield round the core; all four down,
--        the core is bare for a while, then the pylons come back. Its guard
--        comes out of the side walls and Basoalth throws his orbs from the
--        throne.
--   6    the rising: the core broken, Basoalth takes its energy and grows a
--        giant through the roof; the hall's floor falls in.
--   7    the escape: jeeps from the foot of the fallen floor down a tunnel
--        that turns, climbs and goes down, under the ridge and along the
--        chasm; one drives and the others shoot -- the giant walks after them
--        breaking the floor behind, smashes holes ahead, throws orbs; a hit
--        on his head makes him reel.
--        Everybody lost and the road is built again for another try; one
--        jeep at the rock at the end and the way home opens to all.
--
-- Every kill of the allies is credits in one purse, spent in the shop (B, at
-- the camp) on more soldiers, better guns and machines. Bots fill the allies
-- to as many as have been bought; alone, a bot drives a jeep or mans its gun.
--
-- The guns, grenades, bots and machines are the front's (games/front,
-- front_game), copied and turned to this: the allies are team 1, the legion
-- team 2. The SERVER decides every shot; friendly fire is off.
--
-- `side = both`: the server runs the first half, a player's machine the
-- second, and a process that is its own server runs both.

local EYE    = 1.5
local GROUND = 20
local SIZE   = 216     -- what a machine may leave before it is lost: the valley and the chasm
local MID    = 64

local TEAM_NAME = { "ALLEATI", "LEGIONE" }

-- The guns: `every` seconds between rounds while the trigger is held (the
-- pistol fires once a pull), `mag` rounds a magazine, `reserve` more carried.
-- Health is 20: a rifle kills in five, two of them to the head. `spread` is
-- how far off a round goes from the hip, `ads` aimed down the sights (the
-- right button, which also slows the body to AIM_WALK); `zoom` the view
-- then, the sniper's through its scope.
local GUNS = {
    pistol = { every = 0.2, damage = 5, spread = 0.035, ads = 0.006, zoom = 0.85, range = 90, mag = 12, reserve = 60, reload = 1.2, auto = false },
    rifle  = { every = 0.1, damage = 4, spread = 0.045, ads = 0.005, zoom = 0.75, range = 160, mag = 30, reserve = 180, reload = 2.0, auto = true },
    lmg    = { every = 0.075, damage = 4, spread = 0.07, ads = 0.015, zoom = 0.8, range = 160, mag = 100, reserve = 200, reload = 4.0, auto = true },
    sniper = { every = 1.2, damage = 18, spread = 0.09, ads = 0.0004, zoom = 0.2, range = 300, mag = 5, reserve = 25, reload = 3.0, auto = false, scope = true },
    -- A rocket, not a round (M.shoot): against what the bullets bounce off.
    launcher = { every = 0.5, damage = 0, spread = 0.03, ads = 0.004, zoom = 0.85, range = 0, mag = 1, reserve = 4, reload = 2.5, auto = false, rocket = true },
}
local AIM_WALK = 1.8   -- m/s aiming, walk and sprint alike
local HEAD = 1.45     -- a hit this far above the feet is a head: double
local NADES      = 3
local NADE_SPEED = 17
local NADE_FUSE  = 3.0
local NADE_R     = 2.6    -- the hole
local NADE_REACH = 6.5    -- the blast
local NADE_DMG   = 26

-- assault_core's places, kept in step with its own copy: the camp, the
-- strongholds' flags (the fortress's in its courtyard), the escape road.
local CAMP  = { MID, 22 }
local FORTS = { { MID, 250 }, { MID, 480 }, { MID, 710 }, { MID, 905 } }
local FORT_NAME = { "L'AVAMPOSTO", "IL BASTIONE", "LA BATTERIA", "LA FORTEZZA" }
local CAPTURE_R = 10

local BOT_SIGHT  = 48

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local clock = 0

local function short(item: string): string
    return (string.gsub(item, "^assault_game:", ""))
end

local heard: (string) -> () = function(_) end
local asked: (number, string) -> () = function(_, _) end

local function tell(player: number, text: string)
    if drawing then
        heard(text)
    else
        core.net.send(player, text)
    end
end

-- The first t along o + d*t inside the box, or nil: the slabs.
local function ray_box(o: {number}, d: {number}, lo: {number}, hi: {number}, far: number): number?
    local near = 0
    for a = 1, 3 do
        if math.abs(d[a]) < 1e-9 then
            if o[a] < lo[a] or o[a] > hi[a] then return nil end
        else
            local t1, t2 = (lo[a] - o[a]) / d[a], (hi[a] - o[a]) / d[a]
            if t1 > t2 then t1, t2 = t2, t1 end
            near = math.max(near, t1)
            far = math.min(far, t2)
            if near > far then return nil end
        end
    end
    return near
end

-- The same against a box turned by `yaw` about its base (a machine): x to
-- its right, up, and z to its front, `h` high.
local function ray_obb(o: {number}, d: {number}, b, far: number): number?
    local sy, cy = math.sin(b.yaw), math.cos(b.yaw)
    local rx, ry, rz = o[1] - b.x, o[2] - b.y, o[3] - b.z
    local lo = { rx * cy + rz * sy, ry, rx * sy - rz * cy }
    local ld = { d[1] * cy + d[3] * sy, d[2], d[1] * sy - d[3] * cy }
    return ray_box(lo, ld, { -b.hw, 0, -b.hl }, { b.hw, b.h, b.hl }, far)
end

-- An angle between -pi and pi.
local function wrap(a: number): number
    return (a + math.pi) % (2 * math.pi) - math.pi
end

-- How a body in a seat that is not the wheel is drawn: at a machine's gun
-- its hands on it -- standing, the arms forward as if aimed; sat, as at a
-- wheel -- and anywhere else with its own gun's clip of the arms (`arms`),
-- over the sitting legs when it sits ("base+arms", the client's).
function M_seat_clip(stand: boolean, mounted: boolean, arms: string): string
    if mounted then return if stand then "aim_lmg" else "drive_idle" end
    if stand then return arms end
    return if arms == "" then "drive_idle" else "drive_idle+" .. arms
end

-- ===================================================================== server

if serving then
    core.net.open()

    for name in GUNS do
        core.register_item(`assault_game:{name}`, 1, "", 0, 1.0, "")
        core.register_model(`assault_game:{name}_held`, `{name}_held.gltf`)
    end
    -- What a builder holds.
    core.register_item("assault_game:wrench", 1, "", 0, 1.0, "")
    core.register_model("assault_game:wrench_held", "wrench_held.gltf")
    core.register_item("assault_game:grenade", 1, "", 0, 1.0, "")
    local NADE_MODEL = core.register_model("assault_game:grenade_held", "grenade_held.gltf")

    local TOUGH = {}
    for _, n in { "bedrock", "ridge", "keep", "seal", "hall", "magma", "frame" } do
        TOUGH[core.node_id(`assault_core:{n}`)] = true
    end

    local NADE = "assault_game:nade"
    core.ecs.component(NADE, { "vx", "vy", "vz", "t", "owner", "team" })

    -- The allies in blue (the front's); the legion three kinds, each by the
    -- way it fights (tools/make_assault_models.py): zombies take cover and
    -- take more killing, demons rush and are quick, cyborgs keep watch with
    -- the sniper and the launcher and are plated.
    -- Workers: what they do is this mod's, not the engine's. `spawn_on` names a
    -- block no world has, so none comes out of the dark by itself.
    local KIND: { [any]: string } = { "assault_game:ally", cover = "assault_game:zombie", rush = "assault_game:demon",
                                       watch = "assault_game:cyborg", at = "assault_game:cyborg" }
    --         name             model             hp  speed
    for _, k in { { "ally", "soldier_blue", 20, 3.6 }, { "zombie", "zombie", 28, 3.0 },
                  { "demon", "demon", 18, 4.4 }, { "cyborg", "cyborg", 26, 3.4 },
                  { "builder", "builder", 50, 3.4 } } do   -- the allies' builders: tough, unarmed
        --                 name                  model                                              hp    speed fight  dmg reach every drop n shot doors hover fire  spawn_on
        core.register_mob(`assault_game:{k[1]}`, core.register_model(`assault_game:{k[2]}`, `{k[2]}.gltf`), k[3], k[4],
                          "worker", 0, 1.6, 1.0, "", 1, 0, true, 0, false, "assault_game:never")
    end

    local players = {}   -- player id -> state, below
    local bots    = {}   -- mob id -> { team, n, ... }
    -- The machines (below, "machines"): one table, so its helpers are fields
    -- and not more of this chunk's 200 locals.
    local M = { list = {}, seat_of = {}, wrecks = {}, respawn = {}, shots = {}, spawned = false,
                seen = { {}, {} } }   -- each team's: what its bots see, by who it is
    local nades   = {}   -- entity id -> true
    -- The assault itself (below, "the assault"): the phase, the purse, what
    -- has been bought, the hall's and the escape's things. One table, as M.
    --   stage   "fort" (fort = 1..4, the one being fought for), "hall",
    --           "rise", "escape", "won"
    --   base    where the allies come back: the camp, then each taken stronghold
    --   things  what a shot may hit besides bodies and machines: the pylons,
    --           the core, the orbs, the giant's head; each { x, y, z, hw, h,
    --           hit = fn(dmg, by, weapon) -> killed }
    local A = { stage = "fort", fort = 1, base = { CAMP[1], CAMP[2] }, credits = 0, allies = 4,
                bought = {}, things = {}, progress = 0, garrison = {}, empty_at = nil, punch = 1 }

    -- Where each player comes back after dying (core.player_spawn), kept to
    -- tell a death the engine dealt -- a fall, the magma -- by where it puts them.
    A.spawn_of = {}
    function A.spawn_at(p: number, x: number, y: number, z: number)
        A.spawn_of[p] = { x, y, z }
        core.player_spawn(p, x, y, z)
    end

    -- Where the allies come back and the bots come out: round the base, on
    -- the open ground north of the last stronghold taken (A.BASE).
    local function base_spot(_t: number): (number, number, number)
        local b = A.base
        local x, z = b[1] + 0.5 + math.random(-8, 8), b[2] + 0.5 + math.random(-4, 4)
        return x, A.feet_at(math.floor(x), math.floor(z)), z
    end

    local function ammo_line(s): string
        return `ammo {s.mag.pistol} {s.res.pistol} {s.mag.rifle} {s.res.rifle} {s.mag.lmg} {s.res.lmg} {s.nades} {s.kills} {s.mag.launcher} {s.res.launcher} {s.mag.sniper} {s.res.sniper}`
    end

    local function refill(s)
        s.mag, s.res = {}, {}
        for name, g in GUNS do
            s.mag[name], s.res[name] = g.mag, g.reserve
        end
        s.nades = NADES
        s.reload_at = 0
    end

    -- Sent home, and sent again for the next few seconds whenever they are
    -- not there: a body that has only just come in has no place yet, and the
    -- engine then puts it at the world's spawn after this has placed it
    -- (step_players asks again while s.home is set).
    local function to_base(p: number, s)
        local x, y, z = base_spot(s.team)
        A.spawn_at(p, x, y, z)
        core.player_place(p, x, y, z, 0, 0, 1)
        s.home = { x, y, z, clock + 4 }
    end

    local function still_going_home(p: number, s)
        local px, _, pz = core.player_pos(p)
        local h = s.home
        if px ~= nil and (px - h[1]) ^ 2 + (pz - h[3]) ^ 2 < 4 then
            if clock > h[4] then s.home = nil end
        else
            core.player_place(p, h[1], h[2], h[3], 0, 0, 1)
        end
    end

    -- ---------------------------------------------------------------- deaths

    -- Into the purse: a player's kill counts double a bot's.
    function A.earn(n: number)
        A.credits += n
        tell(0, `credits {A.credits}`)
    end

    -- Who did it, for the feed: a player's id, or a bot's negated.
    local function who(by: number): (string, number)
        if by > 0 then
            local s = players[by]
            return core.player_name(by), if s then s.team else 0
        end
        local b = bots[-by]
        if b then return `{TEAM_NAME[b.team]}-{b.n}`, b.team end
        return "?", 0
    end

    local function killed(by: number, victim: string, team: number, weapon: string)
        local s = players[by]
        if s then
            s.kills += 1
            tell(by, ammo_line(s))
        end
        if team == 2 then A.earn(if by > 0 then 10 else 5) end
        local name, kt = who(by)
        tell(0, `kill {kt} {name} {team} {victim} {weapon}`)
        core.log(`assault_game: {name} killed {victim} ({weapon})`)
    end

    -- Takes `dmg` off whatever `target` is; true when that killed it.
    local function hurt(by: number, target, dmg: number, weapon: string, ox: number, oz: number): boolean
        if target.thing then
            return target.thing.hit(dmg, by, weapon)
        end
        -- The allies' rounds go further once the purse has paid for it.
        if target.team == 2 and by ~= 0 then dmg *= A.punch end
        if target.machine then
            return M.damage(target.machine, dmg, weapon, by)
        end
        if target.player then
            -- Off the seat first: a body killed while carried would be carried
            -- on, wherever it came back.
            if M.seat_of[target.player] and core.health(target.player) <= dmg then
                M.leave(target.player, true)
            end
            local left = core.damage(target.player, dmg)
            if left == 0 then
                local v = players[target.player]
                if v then v.died = true end
                killed(by, core.player_name(target.player), target.team, weapon)
                return true
            end
        elseif target.bot then
            local b = bots[target.bot]
            if b then b.alert = { ox, oz, clock } end   -- it turns to where that came from
            if b and core.mob_hurt(target.bot, dmg, ox, oz) == 2 then
                local name = who(-target.bot)
                killed(by, name, target.team, weapon)
                b.dead = true
                return true
            end
        end
        return false
    end

    -- Every body of the other team a shot or a blast may find: { player or
    -- bot, team, box }. Both are 0.7 wide and 1.85 high from the feet.
    local function enemies(team: number)
        local out = {}
        for p, s in players do
            if s.team ~= team and core.health(p) > 0 then
                local x, y, z = core.player_pos(p)
                if x ~= nil then
                    table.insert(out, { player = p, team = s.team, feet = y,
                                        lo = { x - 0.35, y, z - 0.35 }, hi = { x + 0.35, y + 1.85, z + 0.35 } })
                end
            end
        end
        for id, b in bots do
            if b.team ~= team and not b.dead then
                local x, y, z = core.mob_pos(id)
                if x ~= nil then
                    table.insert(out, { bot = id, team = b.team, feet = y,
                                        lo = { x - 0.35, y, z - 0.35 }, hi = { x + 0.35, y + 1.85, z + 0.35 } })
                end
            end
        end
        -- Machines: of the other team, or of nobody's with somebody in them.
        for id, m in M.list do
            local manned = next(m.riders) ~= nil or next(m.bots) ~= nil
            if not m.dead and m.team ~= team and (m.team ~= 0 or manned) then
                local x, y, z, yaw = M.where(id)
                local r, h = math.max(m.k.hw or 0.6, m.k.hl or 0.6), m.k.h or 1.6
                table.insert(out, { machine = id, team = m.team, feet = y, empty = not manned,
                                    lo = { x - r, y, z - r }, hi = { x + r, y + h, z + r },
                                    obb = { x = x, y = y, z = z, yaw = yaw, hw = m.k.hw or 0.6,
                                            hl = m.k.hl or 0.6, h = h } })
            end
        end
        -- The legion's things: to the allies only.
        if team ~= 2 then
            for _, t in A.things do
                table.insert(out, { thing = t, team = 2, feet = t.y, big = t.big,
                                    lo = { t.x - t.hw, t.y, t.z - t.hw }, hi = { t.x + t.hw, t.y + t.h, t.z + t.hw } })
            end
        end
        return out
    end

    -- ---------------------------------------------------------------- explosions

    -- The hole: every cell within r but the TOUGH ones is gone, and then what
    -- was standing on what went -- a house's upper wall, a roof -- comes down.
    local function carve(cx: number, cy: number, cz: number, r: number)
        A.hurt_ground(cx, cy, cz, r)
        local R = math.ceil(r)
        local bx, by, bz = math.floor(cx), math.floor(cy), math.floor(cz)
        local tops = {}   -- column -> the highest cell cleared in it
        for x = -R, R do
            for z = -R, R do
                for y = -R, R do
                    if x * x + y * y + z * z <= r * r then
                        local id = core.get_block(bx + x, by + y, bz + z)
                        if id ~= 0 and not TOUGH[id] then
                            core.set_block(bx + x, by + y, bz + z, 0)
                            local key = (bx + x) * 4096 + (bz + z)
                            tops[key] = math.max(tops[key] or -1e9, by + y)
                        end
                    end
                end
            end
        end
        local fell = 0
        for key, top in tops do
            local x = key // 4096
            local z = key - x * 4096
            for y = top + 1, top + 8 do
                local id = core.get_block(x, y, z)
                if id == 0 or TOUGH[id] or fell >= 48 then break end
                core.fall(x, y, z)
                fell += 1
            end
        end
    end

    -- A blast at x, y, z thrown by `by` of `team`: the hole, then everybody of
    -- the other team within reach -- less at the edge, less behind a wall --
    -- and its thrower, by half.
    local function explode(x: number, y: number, z: number, r: number, reach: number, dmg: number,
                           by: number, team: number, weapon: string)
        carve(x, y, z, r)
        tell(0, string.format("boom %.2f %.2f %.2f %.1f", x, y, z, r))
        core.play_sound(if r >= 3 then "assault_game:boom" else "assault_game:boom_small", x, y, z)
        local targets = enemies(team)
        if players[by] and core.health(by) > 0 then
            local px, py, pz = core.player_pos(by)
            if px ~= nil then
                table.insert(targets, { player = by, team = team, self = true, feet = py,
                                        lo = { px - 0.35, py, pz - 0.35 }, hi = { px + 0.35, py + 1.85, pz + 0.35 } })
            end
        end
        for _, t in targets do
            local mx, my, mz = (t.lo[1] + t.hi[1]) / 2, (t.lo[2] + t.hi[2]) / 2, (t.lo[3] + t.hi[3]) / 2
            local dx, dy, dz = mx - x, my - y, mz - z
            local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
            if dist <= reach then
                local hit = dmg * (1 - 0.7 * dist / reach)
                if dist > 0.5 then
                    local _, _, _, wall = core.raycast(x, y, z, dx, dy, dz, dist)
                    if wall ~= nil and wall < dist - 0.5 then hit *= 0.35 end
                end
                if t.self then hit *= 0.5 end
                if t.player then hit *= A.blast end   -- the anti-blast armour bought
                if hit >= 1 then hurt(by, t, math.floor(hit), weapon, x, z) end
                if t.machine and dist > 0.1 then
                    local k = hit * 0.15 / dist
                    core.craft.push(t.machine, dx * k, dy * k + hit * 0.05, dz * k)
                end
            end
        end
    end

    -- ---------------------------------------------------------------- grenades

    local function spawn_nade(x: number, y: number, z: number, vx: number, vy: number, vz: number,
                              by: number, team: number)
        local id = core.ecs.spawn(NADE, x, y, z)
        if id == nil then return end
        core.ecs.set(id, "vx", vx)
        core.ecs.set(id, "vy", vy)
        core.ecs.set(id, "vz", vz)
        core.ecs.set(id, "t", 0)
        core.ecs.set(id, "owner", by)
        core.ecs.set(id, "team", team)
        core.ecs.visible(id, true)
        core.ecs.model(id, NADE_MODEL)
        nades[id] = true
    end

    local function throw(p: number, s)
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then return end
        dx, dy, dz = M.toward(p, { x, y + EYE, z }, dx, dy, dz)
        spawn_nade(x + dx * 0.6, y + EYE - 0.2 + dy * 0.6, z + dz * 0.6,
                   dx * NADE_SPEED, dy * NADE_SPEED + 3, dz * NADE_SPEED, p, s.team)
    end

    -- Falls, and bounces off what it meets: off a floor it loses most of its
    -- fall and some of its run, off a wall the run across it.
    local function step_nades(dt: number)
        for id in nades do
            local x, y, z = core.ecs.get(id, "x"), core.ecs.get(id, "y"), core.ecs.get(id, "z")
            if x == nil then
                nades[id] = nil
                continue
            end
            local t = core.ecs.get(id, "t") + dt
            core.ecs.set(id, "t", t)
            if t >= NADE_FUSE then
                nades[id] = nil
                local owner, team = core.ecs.get(id, "owner") or 0, core.ecs.get(id, "team") or 0
                core.ecs.kill(id)
                explode(x, y + 0.2, z, NADE_R, NADE_REACH, NADE_DMG, owner, team, "grenade")
                continue
            end
            local vx, vy, vz = core.ecs.get(id, "vx"), core.ecs.get(id, "vy") - 22 * dt, core.ecs.get(id, "vz")
            local speed = math.sqrt(vx * vx + vy * vy + vz * vz)
            if speed > 0.05 then
                local step = speed * dt
                local cx, cy, cz, wall = core.raycast(x, y + 0.08, z, vx, vy, vz, step + 0.08)
                if wall ~= nil then
                    if cy < math.floor(y + 0.08) then
                        vy, vx, vz = -vy * 0.3, vx * 0.6, vz * 0.6
                        if math.abs(vy) < 1.2 then vy = 0 end
                    elseif cy > math.floor(y + 0.08) then
                        vy = -vy * 0.3
                    else
                        if cx ~= math.floor(x) then vx = -vx * 0.35 end
                        if cz ~= math.floor(z) then vz = -vz * 0.35 end
                    end
                    if math.abs(vx) + math.abs(vz) < 0.3 and vy == 0 then vx, vz = 0, 0 end
                else
                    core.ecs.set(id, "x", x + vx * dt)
                    core.ecs.set(id, "y", y + vy * dt)
                    core.ecs.set(id, "z", z + vz * dt)
                end
            end
            core.ecs.set(id, "vx", vx)
            core.ecs.set(id, "vy", vy)
            core.ecs.set(id, "vz", vz)
        end
    end

    -- ---------------------------------------------------------------- shooting

    local function jitter(d: {number}, spread: number): {number}
        local x = d[1] + (math.random() * 2 - 1) * spread
        local y = d[2] + (math.random() * 2 - 1) * spread
        local z = d[3] + (math.random() * 2 - 1) * spread
        local l = math.sqrt(x * x + y * y + z * z)
        return { x / l, y / l, z / l }
    end

    -- One round from o along d: the wall it stops at, the nearest enemy
    -- before it, and the tracer everybody sees.
    local function bullet(p: number, s, o: {number}, d: {number}, g, gun: string, ignore: number?)
        local far = g.range
        local _, _, _, wall = core.raycast(o[1], o[2], o[3], d[1], d[2], d[3], far)
        if wall ~= nil then far = wall end
        local best, at = nil, far
        for _, t in enemies(s.team) do
            local hit = if ignore ~= nil and t.machine == ignore then nil
                elseif t.obb then ray_obb(o, d, t.obb, at) else ray_box(o, d, t.lo, t.hi, at)
            if hit ~= nil and hit < at then
                best, at = t, hit
            end
        end
        local e = { o[1] + d[1] * at, o[2] + d[2] * at, o[3] + d[3] * at }
        if best then
            local dmg = if e[2] - best.feet >= HEAD and not best.machine and not best.thing then g.damage * 2 else g.damage
            local dead = hurt(p, best, dmg, gun, o[1], o[3])
            if p > 0 then
                tell(p, if dead then "hit 2" elseif dmg > g.damage then "hit 1" else "hit 0")
            end
        end
        tell(0, string.format("shot %.2f %.2f %.2f %.2f %.2f %.2f %d", o[1], o[2], o[3],
                              e[1], e[2], e[3], if best then 1 else 0))
    end

    local function fire(p: number, s)
        -- At the wheel or at a machine's gun, that; on any other seat, the
        -- gun in the hand, fired from the seat.
        local cur = M.seat_of[p]
        local mm = cur and M.list[cur.id]
        if cur and (cur.seat == 1 or (mm and mm.k.seats[cur.seat].gun)) then
            M.fire(p, s, false)
            return
        end
        local item = short(core.held_item(p))
        if item == "grenade" then
            if s.nades > 0 and s.throw_at == 0 then
                s.nades -= 1
                s.next_fire = clock + 0.9
                s.throw_at = clock + 0.42
                s.throwing_until = clock + 0.9
                s.held = false
                tell(p, "fired grenade")
                tell(p, ammo_line(s))
            end
            return
        end
        local g = GUNS[item]
        if g == nil or s.reload_at > 0 then return end
        if s.mag[item] <= 0 then
            if s.res[item] > 0 then
                s.reload_at, s.reloading = clock + g.reload, item
                tell(p, `reload {item}`)
            elseif not s.dry then
                s.dry = true
                tell(p, "dry")
            end
            return
        end
        s.mag[item] -= 1
        s.next_fire = clock + g.every
        s.firing_until = clock + g.every + 0.08
        if not g.auto then s.held = false end
        tell(p, `fired {item}`)
        tell(p, ammo_line(s))
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then return end
        core.play_sound(`assault_game:{if g.rocket then "rocket" else item}`, x, y + EYE, z)
        dx, dy, dz = M.toward(p, { x, y + EYE, z }, dx, dy, dz)
        local d = jitter({ dx, dy, dz }, if s.aiming then g.ads else g.spread)
        if g.rocket then
            M.shoot("rocket", { x + d[1] * 0.8, y + EYE - 0.1 + d[2] * 0.8, z + d[3] * 0.8 },
                    { d[1] * 45, d[2] * 45, d[3] * 45 }, p, s.team)
            return
        end
        bullet(p, s, { x, y + EYE, z }, d, g, item)
    end

    -- What everybody else sees this player's arms do with what they hold
    -- (tools/make_front_poses.py: clips of the arms alone, over the walk):
    -- the throw, a reload, a burst, the sights, or the gun held ready.
    function M.pose(p: number, s)
        local cur = M.seat_of[p]
        local m = cur and M.list[cur.id]
        local seat = m and m.k.seats[cur.seat]
        if cur and (seat == nil or (cur.seat == 1 and m.k.craft)) then
            s.clip = nil   -- the wheel's (M.enter, the keys): looked at again once off it
            return
        end
        local item = short(core.held_item(p))
        local clip = ""
        if (s.throwing_until or 0) > clock then
            clip = "fire_grenade"
        elseif GUNS[item] then
            clip = if s.reload_at > 0 and s.reloading == item then `reload_{item}`
                elseif (s.firing_until or 0) > clock then `fire_{item}`
                elseif s.aiming then `aim_{item}`
                else `hold_{item}`
        end
        if seat then clip = M_seat_clip(seat.stand == true, seat.gun ~= nil, clip) end
        if clip ~= s.clip then
            s.clip = clip
            core.player_clip(p, clip)
        end
    end

    -- Aiming down the sights: precise, and slow.
    function M.sights(p: number, s, on: boolean)
        if s.aiming == on then return end
        s.aiming = on
        if on then
            core.body_movement(p, AIM_WALK, AIM_WALK, 0, 6, -25)
        else
            core.body_movement(p, 4.8, 8.5, 0, 8, -25)
        end
    end

    asked = function(p: number, what: string)
        local s = players[p]
        if s == nil then return end
        if what == "fire1" then
            s.held, s.dry = true, false
        elseif what == "fire0" then
            s.held = false
        elseif string.sub(what, 1, 4) == "aim " then
            -- The camera's ray through the crosshair: from, and which way.
            local w = string.split(what, " ")
            local r = {}
            for i = 2, 7 do r[i - 1] = tonumber(w[i]) end
            local l = if r[6] then math.sqrt(r[4] ^ 2 + r[5] ^ 2 + r[6] ^ 2) else 0
            if l > 1e-6 then
                s.ray = { r[1], r[2], r[3], r[4] / l, r[5] / l, r[6] / l, clock }
                s.aim = nil
            end
        elseif what == "use" then
            M.use(p)
        elseif what == "shop" then
            A.shop(p)
        elseif what == "aim1" or what == "aim0" then
            M.sights(p, s, what == "aim1")
        elseif what == "fire2" then
            s.held2 = true
        elseif what == "fire2up" then
            s.held2 = false
        elseif string.sub(what, 1, 2) == "k " then
            local w = string.split(what, " ")
            s.keys = { math.clamp(tonumber(w[2]) or 0, -1, 1), math.clamp(tonumber(w[3]) or 0, -1, 1),
                       math.clamp(tonumber(w[4]) or 0, -1, 1) }
            -- At a wheel the arms turn it the way it is turned.
            local cur = M.seat_of[p]
            local m = cur and M.list[cur.id]
            if m and m.k.wheel and cur.seat == 1 then
                core.player_clip(p, M.steer_clip(s.keys[2]))
            end
        elseif string.sub(what, 1, 5) == "seat " then
            M.switch(p, tonumber(string.sub(what, 6)) or 1)
        elseif what == "reload" then
            local item = short(core.held_item(p))
            local g = GUNS[item]
            if g and s.reload_at == 0 and s.mag[item] < g.mag and s.res[item] > 0 then
                s.reload_at, s.reloading = clock + g.reload, item
                tell(p, `reload {item}`)
            end
        end
    end

    core.event.on("core:net_message", function(ev)
        asked(ev.player, ev.text)
    end)

    -- For trying things out: /mezzo jeep puts one six nodes in front, of the
    -- player's team, and none comes back when it is lost.
    core.command("mezzo", "<jeep|tank|heli|plane|bomber|nest|mortar|howitzer|aa> -- a machine in front of you (test)", "moderator")
    core.event.on("core:command", function(ev)
        if ev.name ~= "mezzo" then return end
        local kind = string.match(ev.args or "", "%S+") or "jeep"
        local x, _, z = core.player_pos(ev.player)
        local dx, _, dz = core.player_look(ev.player)
        local s = players[ev.player]
        if M.KINDS[kind] == nil or x == nil or dx == nil or s == nil then
            core.chat(ev.player, "/mezzo jeep|tank|heli|plane|bomber|nest|mortar|howitzer|aa")
            return
        end
        local l = math.max(0.01, math.sqrt(dx * dx + dz * dz))
        local id = M.spawn(kind, x + dx / l * 6, z + dz / l * 6, math.atan2(dx, -dz), s.team)
        if id then M.list[id].debug = true end
    end)

    -- ---------------------------------------------------------------- bots

    -- The cell a body's feet go in at x, z: the lowest one with ground under
    -- it and room for a body over it -- a house's floor, not its roof.
    -- Once the hall's floor has fallen in (A.deep) from the tunnel's floor
    -- down there, deeper than the valley's.
    local function feet_at(x: number, z: number): number
        for y = if A.deep then -12 else GROUND - 6, GROUND + 12 do
            if core.get_block(x, y - 1, z) ~= 0 and core.get_block(x, y, z) == 0
               and core.get_block(x, y + 1, z) == 0 then
                return y
            end
        end
        return GROUND
    end
    A.feet_at = feet_at

    local bot_n = { 0, 0 }

    -- Three ways to fight, one each in turn: the watch stops now and then to
    -- look round and would rather snipe; the rush runs at what it sees with
    -- the machine gun; the cover goes behind something first, looks out to
    -- shoot a burst and goes back. Each keeps to its first gun while that has
    -- rounds, then the next. A rifle round of theirs does a little less than
    -- a player's, in bursts, off at first and closer the longer they look.
    M.ROLES = { "watch", "rush", "cover", "at" }
    M.PREFER = { watch = { "sniper", "rifle", "pistol" }, rush = { "lmg", "rifle", "pistol" },
                 cover = { "rifle", "pistol" }, at = { "launcher", "rifle", "pistol" } }
    M.REACT = { watch = 0.8, rush = 0.45, cover = 0.65, at = 0.9 }   -- seconds from seeing to shooting
    M.BOTGUN = {
        rifle  = { every = 0.16, burst = { 3, 5 }, pause = { 0.5, 0.9 } },
        lmg    = { every = 0.12, burst = { 6, 10 }, pause = { 0.7, 1.1 } },
        pistol = { every = 0.45, burst = { 1, 2 }, pause = { 0.4, 0.8 } },
        sniper = { every = 0, burst = { 1, 1 }, pause = { 2.2, 3.0 }, spread = 0.008, sight = 90 },
        launcher = { every = 0, burst = { 1, 1 }, pause = { 2.6, 3.4 } },
    }
    M.BOT_HURTS = 0.75    -- of a gun's damage
    M.CONE = 0.57         -- cos 55 degrees: what is outside it is not seen

    -- An ally out at the base, or one of the legion at x, z with its post
    -- there (it keeps to it: A.goal). `role` for one that must be a kind.
    function A.spawn_bot(team: number, x: number?, z: number?, role: string?): number?
        local bx, _, bz = base_spot(team)
        if team == 1 then bx, _, bz = A.barracks_spot() end
        x, z = x or bx, z or bz
        role = role or M.ROLES[(bot_n[team] + 1) % #M.ROLES + 1]
        local id = core.spawn_mob(if team == 1 then KIND[1] else KIND[role], x, feet_at(math.floor(x), math.floor(z)), z)
        if id == 0 then return nil end
        bot_n[team] += 1
        local ammo = {}
        for _, g in M.PREFER[role] do ammo[g] = { mag = GUNS[g].mag, res = GUNS[g].reserve } end
        bots[id] = { team = team, n = bot_n[team], think_at = 0, shot_at = 0, nade_at = clock + 10,
                     enemy = nil, throwing = 0, role = role, ammo = ammo,
                     post = if team == 2 then { x, z } else nil, fort = A.fort,
                     gun = M.PREFER[role][1], burst = 0, yaw = if team == 1 then math.pi else 0 }
        M.bot_clip(id, bots[id], "")
        return id
    end

    -- Turned to look at x, z, and that remembered: what it sees is in front.
    function M.face(id: number, b, x: number, z: number)
        local bx, _, bz = core.mob_pos(id)
        if bx == nil then return end
        core.mob_face(id, x, z)
        if (x - bx) ^ 2 + (z - bz) ^ 2 > 0.01 then b.yaw = math.atan2(x - bx, -(z - bz)) end
    end

    -- What it is seen doing, and the gun in its hands (its model has none of
    -- its own: tools/make_front_models.py); `bare` for hands on a wheel or a
    -- machine's gun.
    function M.bot_clip(id: number, b, clip: string, bare: boolean?)
        local item = if bare then "" else `assault_game:{b.gun}`
        if b.clip ~= clip or b.item ~= item then
            b.clip, b.item = clip, item
            core.mob_act(id, clip, item)
        end
    end

    -- The nearest enemy it can see from its eyes, within sight: in the cone
    -- in front of it, or close enough to hear, or the one it is already
    -- watching. From a machine's seat, all round.
    local function spotted(id: number, b)
        local x, y, z = core.mob_pos(id)
        if x == nil then return nil end
        local best, at = nil, (M.BOTGUN[b.gun] and M.BOTGUN[b.gun].sight) or BOT_SIGHT
        if b.crew then at = 150 end   -- a jeep's gunner in the escape: the giant's head, his orbs
        local sight = at
        local fx, fz = math.sin(b.yaw or 0), -math.cos(b.yaw or 0)
        for _, t in enemies(b.team) do
            if t.empty then continue end
            local trap = t.thing and t.thing.trap
            if trap and not A.trap_ripe(trap) then continue end
            local tx, ty, tz = (t.lo[1] + t.hi[1]) / 2, t.feet + 1.3, (t.lo[3] + t.hi[3]) / 2
            local dx, dy, dz = tx - x, ty - (y + EYE), tz - z
            local d = math.sqrt(dx * dx + dy * dy + dz * dz)
            -- In the hall the allies go for the pylons and the core before the
            -- guard; in the tunnel for a trap whose moment has come.
            local rank = if trap then d * 0.3 elseif t.thing and A.stage == "hall" then d * 0.4 else d
            if rank < at and d < sight then
                local key = t.player or (if t.bot then -t.bot elseif t.thing then 200000 + t.thing.n else 100000 + (t.machine or 0))
                local flat = math.sqrt(dx * dx + dz * dz)
                local ahead = flat < 0.01 or (dx * fx + dz * fz) / flat > M.CONE
                local known = b.mem and b.mem.key == key and clock - b.mem.t < 2
                if b.ride or d < 4 or ahead or known then
                    local _, _, _, wall = core.raycast(x, y + EYE, z, dx, dy, dz, d)
                    -- A trap's own blocks (the wheel, the rocks, the tank) lie
                    -- inside its box: they are not in the way of it.
                    if wall == nil or (trap and wall > d - (t.hi[1] - t.lo[1])) then
                        best, at = { t = t, x = tx, y = ty, z = tz, d = d, key = key }, rank
                    end
                end
            end
        end
        if best then
            -- Somebody new: a moment before the first round.
            if b.mem == nil or b.mem.key ~= best.key or clock - b.mem.t > 3 then
                b.react_at = clock + M.REACT[b.role] + math.random() * 0.3
            end
            b.mem = { key = best.key, x = best.x, y = best.y, z = best.z, t = clock }
        end
        return best
    end

    -- Where it is going: one of the legion to its post, an ally to the
    -- stronghold being fought for, then the hall, then the garage.
    local function goal_of(_id: number, b): { number }
        if b.post then return b.post end
        -- One ally in three goes for a bonus ground not yet held, if any.
        if A.stage == "fort" and b.team == 1 and b.n % 3 == 0 then
            local z = A.zone_for(b)
            if z then return { z.x, z.z } end
        end
        if A.stage == "fort" then return FORTS[A.fort] end
        if A.stage == "hall" then return { MID, 976 } end
        return { MID, 1022 }
    end

    -- Somewhere near out of the line from `e` to the eyes, not nearer to it:
    -- the nearest of a ring of cells 3, 5 and 7 nodes off.
    function M.find_cover(x: number, y: number, z: number, e): (number?, number?)
        local now = (e.x - x) ^ 2 + (e.z - z) ^ 2
        for _, r in { 3, 5, 7 } do
            for k = 0, 7 do
                local a = k * math.pi / 4
                local cx, cz = math.floor(x + math.cos(a) * r), math.floor(z + math.sin(a) * r)
                local fy = feet_at(cx, cz)
                if math.abs(fy - y) > 2 or (e.x - cx) ^ 2 + (e.z - cz) ^ 2 < now then continue end
                local dx, dy, dz = cx + 0.5 - e.x, fy + 1.3 - e.y, cz + 0.5 - e.z
                local d = math.sqrt(dx * dx + dy * dy + dz * dz)
                local _, _, _, wall = core.raycast(e.x, e.y, e.z, dx, dy, dz, d)
                if wall ~= nil and wall < d - 0.6 then return cx, cz end
            end
        end
        return nil, nil
    end

    -- The cover's round: behind cover, out to where it saw them for a burst,
    -- back. While it remembers somebody (6 s); false when it does not.
    function M.cover(id: number, b, x: number, y: number, z: number): boolean
        local e = b.enemy or (if b.mem and clock - b.mem.t < 6 then b.mem else nil)
        if e == nil then
            b.phase, b.hold_fire = nil, false
            return false
        end
        local function go(at)
            core.mob_goto(id, at[1], feet_at(at[1], at[2]), at[2])
        end
        if b.phase == nil then
            local cx, cz = M.find_cover(x, y, z, e)
            if cx == nil then
                core.mob_goto(id, math.floor(x), math.floor(y), math.floor(z))   -- nothing to hide behind
                b.hold_fire = false
                return true
            end
            b.cover, b.peek = { cx, cz }, { math.floor(x), math.floor(z) }
            b.phase, b.phase_until = "hide", clock + 1.2 + math.random() * 0.8
            go(b.cover)
        elseif clock >= b.phase_until then
            if b.phase == "hide" then
                b.phase, b.phase_until = "peek", clock + 1.8 + math.random()
                go(b.peek)
            else
                b.phase, b.phase_until = "hide", clock + 1.5 + math.random()
                go(b.cover)
            end
        end
        b.hold_fire = b.phase == "hide"
        return true
    end

    local function think(id: number, b)
        local x, y, z = core.mob_pos(id)
        if x == nil then return end
        if b.builder then
            A.build_think(id, b, x, y, z)
            return
        end
        if b.ride then
            M.bot_ride_think(id, b)
            return
        end
        -- Not a node further in three seconds of walking: stuck on something
        -- the way round which is longer than the search. A step aside.
        -- (One of the legion standing at its post is not stuck.)
        local at_post = b.post ~= nil and (x - b.post[1]) ^ 2 + (z - b.post[2]) ^ 2 < 9
        if b.last and (x - b.last[1]) ^ 2 + (z - b.last[2]) ^ 2 < 1 and not b.enemy and not at_post then
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
        if A.blow_gate(id, b, x, y, z) then return end

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
                -- Head down at them, shooting on the way; the legion not
                -- further than 30 from its post.
                if e.d > 3 and (b.post == nil or (e.x - b.post[1]) ^ 2 + (e.z - b.post[2]) ^ 2 < 900) then
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
        local p = goal_of(id, b)
        -- A waypoint a couple of dozen nodes on, with a little spread so a
        -- squad does not walk in single file; at the point, somewhere in it.
        local gx, gz = p[1] + (b.n % 5 - 2) * 3, p[2] + (b.n % 3 - 1) * 4
        if b.post then gx, gz = p[1], p[2] end
        local dx, dz = gx - x, gz - z
        local d = math.sqrt(dx * dx + dz * dz)
        if d > 24 then
            gx, gz = x + dx / d * 24, z + dz / d * 24
        end
        if core.mob_going(id) == 0 or d > 24 then
            gx, gz = math.floor(gx), math.floor(gz)
            core.mob_goto(id, gx, feet_at(gx, gz), gz)
        end
    end

    local function bot_shoot(id: number, b)
        local e = b.enemy
        local x, y, z = core.mob_pos(id)
        if x == nil or e == nil then return end
        local g, bg = GUNS[b.gun], M.BOTGUN[b.gun]
        local o = { x, y + EYE, z }
        local dx, dy, dz = e.x - o[1], e.y - o[2], e.z - o[3]
        local l = math.sqrt(dx * dx + dy * dy + dz * dz)
        -- Twice as far off at first, settling over a second and a half.
        local settle = 1 + math.max(0, 1 - (clock - (b.react_at or 0)) / 1.5)
        local spread = (bg.spread or g.spread) * (1 + l / 40) * settle
        if g.rocket then
            -- Raised for the rocket's drop on the way (45 m/s, g 2).
            local t = l / 45
            local d = jitter({ dx / l, (dy + t * t) / l, dz / l }, spread)
            M.shoot("rocket", { o[1] + d[1] * 0.8, o[2] - 0.1 + d[2] * 0.8, o[3] + d[3] * 0.8 },
                    { d[1] * 45, d[2] * 45, d[3] * 45 }, -id, b.team)
            core.play_sound("assault_game:rocket", o[1], o[2], o[3])
            return
        end
        local d = jitter({ dx / l, dy / l, dz / l }, spread)
        bullet(-id, b, o, d, { range = g.range, damage = g.damage * M.BOT_HURTS }, b.gun)
        core.play_sound(`assault_game:{b.gun}`, o[1], o[2], o[3])
    end

    -- The trigger: a burst, a pause; a reload when the magazine is out, the
    -- next gun it likes when all of this one's rounds are.
    function M.bot_trigger(id: number, b)
        if clock < (b.react_at or 0) or b.hold_fire then return end
        local a = b.ammo[b.gun]
        if b.reload_at then
            if clock < b.reload_at then return end
            local n = math.min(GUNS[b.gun].mag - a.mag, a.res)
            a.mag += n
            a.res -= n
            b.reload_at = nil
        end
        if clock < b.shot_at then return end
        if a.mag <= 0 then
            if a.res > 0 then
                b.reload_at = clock + GUNS[b.gun].reload
                M.bot_clip(id, b, "")
                return
            end
            for _, g in M.PREFER[b.role] do
                if b.ammo[g].mag + b.ammo[g].res > 0 then
                    b.gun = g
                    return
                end
            end
            -- Nothing left: a pistol's worth picked up off the field.
            b.gun = "pistol"
            b.ammo.pistol = b.ammo.pistol or { mag = 0, res = 0 }
            b.ammo.pistol.res = 24
            return
        end
        a.mag -= 1
        local bg = M.BOTGUN[b.gun]
        b.burst -= 1
        if b.burst <= 0 then
            b.burst = math.random(bg.burst[1], bg.burst[2])
            b.shot_at = clock + bg.pause[1] + math.random() * (bg.pause[2] - bg.pause[1])
        else
            b.shot_at = clock + bg.every
        end
        bot_shoot(id, b)
    end

    local function bot_throw(id: number, b)
        local e = b.enemy
        local x, y, z = core.mob_pos(id)
        if x == nil or e == nil then return end
        local dx, dz = e.x - x, e.z - z
        local d = math.sqrt(dx * dx + dz * dz)
        -- At 45 degrees a throw of speed v lands v^2/g away.
        local v = math.sqrt(d * 22)
        local h = v / math.sqrt(2)
        spawn_nade(x + dx / d * 0.5, y + EYE, z + dz / d * 0.5, dx / d * h, h, dz / d * h, -id, b.team)
    end

    local function step_bots()
        -- Who is dead: gone from the engine once its death clip has played.
        for id, b in bots do
            if core.mob_pos(id) == nil then bots[id] = nil end
        end
        -- The allies on foot: as many as have been bought, while there is
        -- somebody here and a stronghold or the hall to fight for; the
        -- legion is A's (its garrisons and the hall's guard). A jeep's crew
        -- in the escape is not counted (A.crew).
        local count, builders = 0, 0
        for _, b in bots do
            if not b.dead and b.team == 1 and not b.crew then
                if b.builder then builders += 1 else count += 1 end
            end
        end
        local fighting = #core.players() > 0 and (A.stage == "fort" or A.stage == "hall")
        -- The builders: as many as bought, one back every REINFORCE seconds.
        if fighting and builders < A.builders and clock >= (A.builder_at or 0) then
            A.builder_at = clock + (if builders == 0 then 1 else A.REINFORCE)
            A.spawn_builder()
        end
        local want = if fighting then A.allies else 0
        -- The fallen come back in waves: every REINFORCE seconds all that are
        -- missing come out at the base, one a tick.
        if count < want and not A.reinforcing and clock >= (A.reinforce_at or 0) then
            A.reinforcing = true
            if count > 0 then tell(0, "msg RINFORZI ALLA BASE") end
        end
        if A.reinforcing and count >= want then
            A.reinforcing, A.reinforce_at = false, clock + A.REINFORCE
        end
        if count < want and A.reinforcing then
            A.spawn_bot(1)
        elseif count > want then
            for id, b in bots do
                if b.team == 1 and not b.dead and not b.crew and not b.builder then
                    if b.ride then M.bot_leave(id, b) end
                    core.mob_remove(id)
                    bots[id] = nil
                    break
                end
            end
        end
        for id, b in bots do
            if b.dead then continue end
            if clock >= b.think_at then
                b.think_at = clock + 0.5 + math.random() * 0.2
                local x, y, z = core.mob_pos(id)
                if x ~= nil and not b.ride and A.in_lava(x, y, z) then
                    hurt(0, { bot = id, team = b.team }, 999, "lava", x, z)
                    continue
                end
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
    -- Jeeps, tanks, helicopters and planes are the engine's machines
    -- (core.craft): Jolt moves them, this says who sits where and what each
    -- seat fires. The guns that stay put -- a machine gun nest, a mortar, a
    -- howitzer -- are the same without a craft: a base and a part to aim.
    -- A part rides the hull (or the part `on`), and follows the look of
    -- whoever sits in seat `by`. Seats are x right, y up, z front of the base;
    -- the first is the driver's, who also has its gun. A seated robot
    -- (drive_idle) has its hips 0.3 over its feet and 0.27 behind them, so a
    -- seat is a cushion's top less 0.3; one inside a hull keeps the head under
    -- its roof (the robot is 1.37 tall seated). Models:
    -- tools/make_front_models.py.

    M.MODEL = {}
    for _, n in { "jeep", "tank_hull", "tank_turret", "tank_gun", "heli", "plane", "mg", "mg_tripod",
                  "mortar_base", "mortar_tube", "howitzer", "howitzer_barrel", "shell", "rocket", "bomb",
                  "jeep_wreck", "tank_wreck", "heli_wreck", "plane_wreck",
                  "bomber", "bomber_wreck", "aa_base", "aa_launcher", "missile" } do
        M.MODEL[n] = core.register_model(`assault_game:{n}`, `{n}.gltf`)
    end
    M.THING = "assault_game:machine"
    core.ecs.component(M.THING, { "kind" })

    M.KINDS = {
        jeep = { craft = "wheeled", model = "jeep", mass = 900, hw = 1.0, hh = 0.45, hl = 2.0, h = 2.0,
                 accel = 7, top = 18, turn = 1.2, hp = 70, armor = 0.8, view = 7, clip = "drive", wreck = "jeep_wreck",
                 wheel = true,   -- a steering wheel at the driver's hands (tools/make_front_models.py)
                 seats = { { -0.45, 1.1, 0.42 }, { 0.45, 1.1, 0.42 }, { 0, 1.18, -1.3, gun = "mg", part = 1, stand = true },
                           { -0.45, 1.1, -0.95 } },
                 parts = { { "mg", 0, 2.0, -0.9, by = 3 } } },
        tank = { craft = "tracked", model = "tank_hull", mass = 4000, hw = 1.6, hh = 0.6, hl = 3.2, h = 2.6,
                 accel = 4, top = 9, turn = 0.9, hp = 260, armor = 0.1, view = 10, wreck = "tank_wreck", breaks = true,
                 -- The commander in the turret's hatch, turning with it, head and
                 -- shoulders out; the gunner standing at the MG on its roof.
                 seats = { { -0.45, -0.12, -0.13, gun = "cannon", part = 2, on = 1 },
                           { 0.6, 0.7, -0.75, gun = "mg", part = 3, on = 1, stand = true } },
                 parts = { { "tank_turret", 0, 1.85, -0.3, by = 1, turret = true },
                           { "tank_gun", 0, 0.35, 1.1, on = 1, by = 1, barrel = true },
                           { "mg", 0.6, 0.78, -0.3, on = 1, by = 2 } } },
        heli = { craft = "rotor", model = "heli", mass = 1500, hw = 1.0, hh = 0.8, hl = 3.0, h = 2.2,
                 accel = 6, top = 22, turn = 1.4, hp = 120, armor = 0.5, view = 12, spin = true, wreck = "heli_wreck",
                 seats = { { 0, 0.25, 1.3, gun = "rockets" }, { -0.55, 0.25, 0.1, gun = "mg", part = 1 }, { 0.55, 0.25, 0.1 } },
                 parts = { { "mg", -1.05, 1.0, 0.1, by = 2 } } },
        plane = { craft = "wing", model = "plane", mass = 800, hw = 0.8, hh = 0.6, hl = 3.0, h = 1.6,
                  accel = 14, top = 42, turn = 1.3, hp = 80, armor = 0.5, view = 12, spin = true, wreck = "plane_wreck",
                  seats = { { 0, 0.1, 0.35, gun = "plane" } }, parts = {} },
        -- Slower and heavier than the plane: its pilot drops a stick of six
        -- bombs (where they would land marked), a gunner stands in its back
        -- turret.
        bomber = { craft = "wing", model = "bomber", mass = 800, hw = 1.2, hh = 0.9, hl = 4.8, h = 2.6,
                   accel = 13, top = 36, turn = 1.0, hp = 220, armor = 0.4, view = 16, spin = true, wreck = "bomber_wreck",
                   seats = { { 0, 1.0, 2.6, gun = "bomber" }, { 0, 1.2, -1.5, gun = "mg", part = 1, stand = true } },
                   parts = { { "mg", 0, 2.2, -1.5, by = 2 } } },
        -- Missiles against what flies, and only that: held on one (AA_LOCK
        -- seconds with it inside AA_CONE of the tubes), then four, one a press.
        aa = { model = "aa_base", hp = 50, armor = 1, view = 4, hw = 0.9, hl = 0.9, h = 1.6,
               seats = { { 0, 0, -1.1, gun = "aa", part = 1, stand = true } },
               parts = { { "aa_launcher", 0, 1.1, 0, by = 1, high = true } } },
        nest = { model = "mg_tripod", hp = 40, armor = 1, view = 0, hw = 0.6, hl = 0.6, h = 1.3,
                 seats = { { 0, 0, -0.9, gun = "mg", part = 1, stand = true } }, parts = { { "mg", 0, 1.0, 0, by = 1 } } },
        mortar = { model = "mortar_base", hp = 30, armor = 1, view = 4, hw = 0.5, hl = 0.7, h = 1.0,
                   seats = { { 0, 0, -1.0, gun = "mortar", part = 1, stand = true } },
                   parts = { { "mortar_tube", 0, 0.65, 0.45, by = 1, lob = 1.05 } } },
        howitzer = { model = "howitzer", hp = 90, armor = 0.3, view = 5, hw = 1.1, hl = 1.6, h = 1.7,
                     seats = { { 0.7, 0, -1.4, gun = "howitzer", part = 1, stand = true } },
                     parts = { { "howitzer_barrel", 0, 0.9, 0.0, by = 1, lob = 0.62 } } },
    }

    -- What each weapon throws (M.shoot): speed, gravity, the hole, the blast,
    -- and what a direct hit takes off besides.
    M.SHOTS = {
        cannon   = { model = "shell", g = 4, r = 2.2, reach = 5, dmg = 45, direct = 110 },
        rocket   = { model = "rocket", g = 2, r = 1.6, reach = 4, dmg = 26, direct = 85 },
        heli     = { model = "rocket", g = 0, r = 1.8, reach = 4.5, dmg = 30, direct = 60 },
        bomb     = { model = "bomb", g = 22, r = 3.5, reach = 8, dmg = 60, direct = 90 },
        mortar   = { model = "shell", g = 22, r = 2.5, reach = 6, dmg = 40, direct = 40 },
        howitzer = { model = "shell", g = 22, r = 3.5, reach = 8, dmg = 70, direct = 70 },
        missile  = { model = "missile", g = 0, r = 0.8, reach = 4, dmg = 20, direct = 80, air = true },
    }
    M.AA_LOCK, M.AA_CONE, M.AA_RANGE = 1.2, 0.1, 260
    M.LOB = { mortar = { cool = 3, range = 140 }, howitzer = { cool = 6, range = 250 } }
    -- Each craft's engine (tools/make_front_sounds.py), and how long a stretch of it lasts.
    M.SOUND = { wheeled = { "assault_game:engine_jeep", 0.55 }, tracked = { "assault_game:engine_tank", 0.55 },
                rotor = { "assault_game:rotor", 0.55 }, wing = { "assault_game:engine_plane", 0.38 } }

    -- The allies' own: two jeeps in the base's park, facing north, back there
    -- 30 s after one is lost (and at the new base once it moves).
    M.PARK = { "jeep", "jeep" }
    -- Each stronghold's guns and machines, the legion's, facing south (yaw 0)
    -- the way the allies come; manned when it is (A.man), never back once lost.
    M.FORT = {
        { { "nest", 60.5, 236.5 }, { "nest", 68.5, 236.5 } },
        { { "nest", 58.5, 468.5 }, { "nest", 70.5, 468.5 }, { "mortar", 48.5, 490.5 }, { "mortar", 80.5, 490.5 },
          { "tank", 78.5, 472.5 } },
        { { "nest", 44.5, 698.5 }, { "nest", 84.5, 698.5 }, { "howitzer", 48.5, 726.5 }, { "howitzer", 80.5, 726.5 },
          { "aa", 64.5, 730.5 }, { "heli", 64.5, 740.5 } },
        { { "tank", 40.5, 900.5 }, { "tank", 88.5, 900.5 }, { "nest", 56.5, 880.5 }, { "nest", 72.5, 880.5 },
          { "mortar", 44.5, 925.5 }, { "mortar", 84.5, 925.5 }, { "aa", 34.5, 932.5 }, { "aa", 94.5, 932.5 },
          { "heli", 64.5, 930.5 } },
    }

    -- Where a machine is: its pose when it moves, else where it was put.
    function M.where(id: number): (number, number, number, number)
        local m = M.list[id]
        if m and m.pose then
            return m.pose.x, m.pose.y, m.pose.z, m.pose.yaw
        end
        return core.ecs.get(id, "x") or 0, core.ecs.get(id, "y") or 0, core.ecs.get(id, "z") or 0,
               core.ecs.get(id, "yaw") or 0
    end

    -- A point in a machine's frame (x right, y up, z front) in the world's, its
    -- tip left out: where a gun's muzzle or a pod is, near enough.
    function M.point(id: number, x: number, y: number, z: number): (number, number, number)
        local bx, by_, bz, yaw = M.where(id)
        local s, c = math.sin(yaw), math.cos(yaw)
        return bx + c * x + s * z, by_ + y, bz + s * x - c * z
    end

    function M.spawn(kind: string, x: number, z: number, yaw: number, team: number)
        local k = M.KINDS[kind]
        local y = feet_at(math.floor(x), math.floor(z))
        local id = core.ecs.spawn(M.THING, x, y, z)
        if id == nil then return end
        core.ecs.visible(id, true)
        core.ecs.model(id, M.MODEL[k.model])
        core.ecs.set(id, "yaw", yaw)
        if k.craft then
            core.craft.make(id, k.craft, k.mass, k.hw, k.hh, k.hl, k.accel, k.top, k.turn)
        end
        -- The allies' are as strong as what has been bought (A.hull).
        local hp = if team == 1 then k.hp * A.hull else k.hp
        local m = { kind = kind, k = k, parts = {}, riders = {}, bots = {}, hp = hp, max = hp, team = team, aim = {}, next = {},
                    home = { x, z, yaw, team }, salvo = 8 }
        for i, part in k.parts do
            local pid = core.ecs.spawn(M.THING, x, y, z)
            if pid == nil then continue end
            core.ecs.visible(pid, true)
            core.ecs.model(pid, M.MODEL[part[1]])
            core.craft.mount(pid, if part.on then m.parts[part.on] else id, part[2], part[3], part[4])
            m.parts[i] = pid
            m.aim[i] = { yaw = 0, pitch = part.lob or 0 }
            core.craft.aim(pid, 0, part.lob or 0)
        end
        M.list[id] = m
        return id
    end

    -- A machine to put down, once the ground under it is in memory (a machine
    -- made over a column nobody had loaded yet fell through it and was lost):
    -- `again` comes back 30 s after it is lost, `then_` is told its id.
    function M.order(kind: string, x: number, z: number, yaw: number, team: number, again: boolean?,
                     then_: ((number) -> ())?)
        table.insert(M.respawn, { kind = kind, home = { x, z, yaw, team }, at = 0, again = again, then_ = then_ })
    end

    -- The allies' fleet, once there is somebody to see it: entities need a
    -- server that listens.
    function M.start()
        if M.spawned then return end
        M.spawned = true
        for i, kind in M.PARK do
            local h = A.park(i)
            M.order(kind, h[1], h[2], h[3], 1, true)
        end
    end

    function M.steer_clip(right: number): string
        return if right > 0 then "drive_right" elseif right < 0 then "drive_left" else "drive_idle"
    end

    function M.enter(p: number, id: number, i: number): boolean
        local m = M.list[id]
        local seat = m and m.k.seats[i]
        if seat == nil or m.riders[i] or m.bots[i] or m.dead then return false end
        local cur = M.seat_of[p]
        if cur then
            local old = M.list[cur.id]
            if old then old.riders[cur.seat] = nil end
        end
        -- On the hull, or on the part it rides (a turret): in that one's frame.
        local carrier = if seat.on then m.parts[seat.on] else id
        if carrier == nil or not core.craft.seat(p, carrier, seat[1], seat[2], seat[3]) then return false end
        m.riders[i] = p
        M.seat_of[p] = { id = id, seat = i }
        M.sights(p, players[p], false)
        core.player_clip(p, if seat.stand then "" else "drive_idle")
        m.team = players[p].team
        if m.k.spin and i == 1 then core.ecs.clip(id, "spin") end
        -- The driver's client runs the machine too (core.craft.make there):
        -- which one, and what it is made of.
        local k = m.k
        local spec = if i == 1 and k.craft
            then string.format(" %.0f %s %g %g %g %g %g %g %g", id, k.craft, k.mass, k.hw, k.hh, k.hl, k.accel, k.top, k.turn)
            else ""
        tell(p, `seat {m.kind} {i} {if i == 1 and k.craft then "drive" else "ride"} {seat.gun or "-"} {k.view} {#k.seats} {if seat.stand then "stand" elseif k.wheel and i == 1 then "wheel" else "sit"}{spec}`)
        return true
    end

    -- Off the seat, beside the machine unless `quiet` (a death: the engine
    -- puts the body back itself).
    function M.leave(p: number, quiet: boolean?)
        local cur = M.seat_of[p]
        if cur == nil then return end
        M.seat_of[p] = nil
        core.ride(p, 0)
        core.player_clip(p, "")
        tell(p, "seat none")
        local m = M.list[cur.id]
        if m == nil then return end
        m.riders[cur.seat] = nil
        if next(m.riders) == nil and next(m.bots) == nil then
            m.team = m.home[4]
            if m.k.spin then core.ecs.clip(cur.id, "") end
        end
        if not quiet then
            local x, _, z = M.point(cur.id, (m.k.hw or 0.6) + 1.2, 0, 0)
            core.player_place(p, x, feet_at(math.floor(x), math.floor(z)), z)
        end
    end

    function M.gone(p: number)
        local cur = M.seat_of[p]
        if cur == nil then return end
        M.seat_of[p] = nil
        local m = M.list[cur.id]
        if m then m.riders[cur.seat] = nil end
    end

    -- F: out of a seat, or into the free seat nearest the body of the
    -- nearest machine -- walk round to a jeep's back for its back seats.
    function M.use(p: number)
        if M.seat_of[p] then
            M.leave(p)
            return
        end
        local px, py, pz = core.player_pos(p)
        if px == nil then return end
        local best, at = nil, 1e9
        for id, m in M.list do
            if m.dead then continue end
            local x, y, z = M.where(id)
            local reach = math.max(m.k.hw or 0.6, m.k.hl or 0.6) + 2.5
            local d = (x - px) ^ 2 + (z - pz) ^ 2
            if d < reach * reach and d < at and math.abs(y - py) < 4 then best, at = id, d end
        end
        if best == nil then return end
        local m = M.list[best]
        local order = {}
        for i in m.k.seats do
            local sx, _, sz = M.seat_pos(best, m, i)
            table.insert(order, { i, (sx - px) ^ 2 + (sz - pz) ^ 2 })
        end
        table.sort(order, function(a, b) return a[2] < b[2] end)
        for _, o in order do
            if M.enter(p, best, o[1]) then return end
        end
    end

    function M.switch(p: number, i: number)
        local cur = M.seat_of[p]
        if cur and cur.seat ~= i then M.enter(p, cur.id, i) end
    end

    function M.damage(id: number, dmg: number, weapon: string, by: number): boolean
        local m = M.list[id]
        if m == nil or m.dead then return false end
        local bang = M.SHOTS[weapon] ~= nil or weapon == "grenade" or weapon == "crash"
        m.hp -= if bang then dmg else dmg * m.k.armor
        m.last_by = by
        if m.hp <= 0 then
            M.destroy(id, by, weapon)
            return true
        end
        return false
    end

    -- Lost: whoever is in it dies, it blows up, a wreck stays a while, and a
    -- new one stands at home half a minute on.
    function M.destroy(id: number, by: number, weapon: string)
        local m = M.list[id]
        if m == nil or m.dead then return end
        m.dead = true
        local x, y, z, yaw = M.where(id)
        for _, p in m.riders do
            M.leave(p, true)
            if core.health(p) > 0 and core.damage(p, 1000) == 0 then
                local v = players[p]
                if v then v.died = true end
                killed(by, core.player_name(p), m.team, weapon)
            end
        end
        for _, bid in m.bots do
            local b = bots[bid]
            if b and not b.dead then
                b.ride = nil
                if core.mob_hurt(bid, 1000, x, z) == 2 then
                    b.dead = true
                    killed(by, (who(-bid)), b.team, weapon)
                end
            end
        end
        for _, pid in m.parts do core.ecs.kill(pid) end
        core.ecs.kill(id)
        M.list[id] = nil
        local name, kt = who(by)
        tell(0, `kill {kt} {name} {m.team} {string.upper(m.kind)} {weapon}`)
        explode(x, y + 1, z, 1.5, 7, 30, by, kt, weapon)
        -- The wreck: down to the ground if it was in the air, then its death
        -- (tools/make_front_models.py: thrown about, parts off, sunk into
        -- the ground in 5 s), and gone.
        if m.k.wreck then
            local w = core.ecs.spawn(M.THING, x, y, z)
            if w then
                core.ecs.visible(w, true)
                core.ecs.model(w, M.MODEL[m.k.wreck])
                core.craft.aim(w, yaw, 0)
                table.insert(M.wrecks, { id = w, x = x, y = y, z = z, vy = if m.pose then m.pose.vy else 0,
                                         ground = feet_at(math.floor(x), math.floor(z)), gone = 0 })
            end
        end
        if m.again then table.insert(M.respawn, { kind = m.kind, home = m.home, at = clock + 30, again = true }) end
        core.log(`assault_game: {m.kind} destroyed by {name} ({weapon})`)
    end

    -- A shell, a rocket or a bomb from o at velocity v, thrown by `by` of
    -- `team`, from inside machine `from` (which it does not hit).
    function M.shoot(kind: string, o: {number}, v: {number}, by: number, team: number, from: number?, homing: number?)
        local id = core.ecs.spawn(M.THING, o[1], o[2], o[3])
        if id == nil then return end
        core.ecs.visible(id, true)
        core.ecs.model(id, M.MODEL[M.SHOTS[kind].model])
        M.shots[id] = { kind = kind, v = v, by = by, team = team, from = from, t = 0, homing = homing }
    end

    -- In the air: a helicopter or a plane more than 3 over the ground.
    function M.flying(m): boolean
        return m ~= nil and not m.dead and m.pose ~= nil and (m.k.craft == "rotor" or m.k.craft == "wing")
            and m.pose.y - GROUND > 3
    end

    function M.step_shots(dt: number)
        for id, sh in M.shots do
            local x, y, z = core.ecs.get(id, "x"), core.ecs.get(id, "y"), core.ecs.get(id, "z")
            if x == nil then
                M.shots[id] = nil
                continue
            end
            local spec = M.SHOTS[sh.kind]
            sh.t += dt
            local v = sh.v
            v[2] -= spec.g * dt
            -- A missile turns after what it was locked on, faster and faster,
            -- and bursts within 3 of it.
            local tm = sh.homing and M.list[sh.homing]
            if tm and tm.pose and not tm.dead then
                local dx, dy, dz = tm.pose.x - x, tm.pose.y + 1 - y, tm.pose.z - z
                local l = math.sqrt(dx * dx + dy * dy + dz * dz)
                if l < 3 then
                    M.shots[id] = nil
                    core.ecs.kill(id)
                    hurt(sh.by, { machine = sh.homing }, spec.direct, sh.kind, x, z)
                    explode(x, y, z, spec.r, spec.reach, spec.dmg, sh.by, sh.team, sh.kind)
                    continue
                end
                local sp = math.sqrt(v[1] * v[1] + v[2] * v[2] + v[3] * v[3])
                local k = math.min(1, 3.5 * dt)
                local cx, cy, cz = v[1] / sp + (dx / l - v[1] / sp) * k, v[2] / sp + (dy / l - v[2] / sp) * k,
                                   v[3] / sp + (dz / l - v[3] / sp) * k
                local cl = math.sqrt(cx * cx + cy * cy + cz * cz)
                sp = math.min(75, sp + 40 * dt)
                v[1], v[2], v[3] = cx / cl * sp, cy / cl * sp, cz / cl * sp
            end
            local speed = math.sqrt(v[1] * v[1] + v[2] * v[2] + v[3] * v[3])
            local d = { v[1] / speed, v[2] / speed, v[3] / speed }
            local step = speed * dt
            local o = { x, y, z }
            local _, _, _, wall = core.raycast(x, y, z, d[1], d[2], d[3], step)
            local at, target = wall or step, nil
            for _, t in enemies(sh.team) do
                if t.machine == sh.from and sh.from ~= nil then continue end
                if spec.air and not (t.machine and M.flying(M.list[t.machine])) then continue end
                local hit = if t.obb then ray_obb(o, d, t.obb, at) else ray_box(o, d, t.lo, t.hi, at)
                if hit ~= nil and hit < at then at, target = hit, t end
            end
            if wall ~= nil or target ~= nil or sh.t > (if spec.air then 7 else 12) or y < -16 then
                M.shots[id] = nil
                core.ecs.kill(id)
                local ex, ey, ez = x + d[1] * at, y + d[2] * at, z + d[3] * at
                if target then hurt(sh.by, target, spec.direct, sh.kind, x, z) end
                explode(ex - d[1] * 0.3, ey - d[2] * 0.3, ez - d[3] * 0.3, spec.r, spec.reach, spec.dmg,
                        sh.by, sh.team, sh.kind)
            else
                core.ecs.set(id, "x", x + v[1] * dt)
                core.ecs.set(id, "y", y + v[2] * dt)
                core.ecs.set(id, "z", z + v[3] * dt)
                core.craft.aim(id, math.atan2(d[1], -d[3]), math.asin(math.clamp(d[2], -1, 1)))
            end
        end
    end

    -- What the crosshair is on, as the player's machine last said (its camera
    -- is behind the head, so the look from the eye misses it), fresh for a
    -- second; else nil.
    -- What the crosshair is on: the camera's ray (as the player's machine
    -- last said it, fresh for a second) to the first block or body it meets
    -- -- a player, a bot, a machine, of either team, not this player nor what
    -- they ride -- or 300 nodes out. Worked out once a tick.
    function M.crosshair(p: number): {number}?
        local s = players[p]
        if s == nil or s.ray == nil or clock - s.ray[7] >= 1 then return nil end
        if s.aim and s.aim[4] == clock then return s.aim end
        local r = s.ray
        local o, d = { r[1], r[2], r[3] }, { r[4], r[5], r[6] }
        local _, _, _, wall = core.raycast(o[1], o[2], o[3], d[1], d[2], d[3], 256)
        local at = wall or 300
        local mine = M.seat_of[p] and M.seat_of[p].id
        -- enemies of no team: everybody, friends too.
        for _, t in enemies(-1) do
            if t.player == p or (mine ~= nil and t.machine == mine) then continue end
            local hit = if t.obb then ray_obb(o, d, t.obb, at) else ray_box(o, d, t.lo, t.hi, at)
            if hit ~= nil and hit < at then at = hit end
        end
        s.aim = { o[1] + d[1] * at, o[2] + d[2] * at, o[3] + d[3] * at, clock }
        return s.aim
    end

    -- From the eye to what the crosshair is on: in third person the camera
    -- sits behind and over the head, and a shot along the eye's own look
    -- landed well off the crosshair. The look when there is no crosshair,
    -- or it is on something nearer than a node or behind the eye (the
    -- camera's ray met a wall behind the body).
    function M.toward(p: number, o: {number}, dx: number, dy: number, dz: number): (number, number, number)
        local c = M.crosshair(p)
        if c == nil then return dx, dy, dz end
        local tx, ty, tz = c[1] - o[1], c[2] - o[2], c[3] - o[3]
        local l = math.sqrt(tx * tx + ty * ty + tz * tz)
        if l < 1 or (tx * dx + ty * dy + tz * dz) / l < 0.5 then return dx, dy, dz end
        return tx / l, ty / l, tz / l
    end

    -- Which way something at `from` has to point to put a shot on the
    -- crosshair: yaw and pitch, and how far; the look when there is none.
    function M.want_at(c: {number}, from: {number}): (number, number, number)
        local dx, dy, dz = c[1] - from[1], c[2] - from[2], c[3] - from[3]
        local flat = math.sqrt(dx * dx + dz * dz)
        return math.atan2(dx, -dz), math.atan2(dy, flat), math.sqrt(flat * flat + dy * dy)
    end

    function M.want(p: number, from: {number}): (number, number, number)
        local c = M.crosshair(p)
        if c then return M.want_at(c, from) end
        local dx, dy, dz = core.player_look(p)
        if dx == nil then return 0, 0, 100 end
        return math.atan2(dx, -dz), math.asin(math.clamp(dy, -1, 1)), 100
    end

    -- Where a lobbing gun's shell would land: what the crosshair is on, as far
    -- as the gun reaches.
    function M.target(p: number, range: number): (number?, number?, number?)
        local c = M.crosshair(p)
        if c then return c[1], c[2], c[3] end
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then return nil, nil, nil end
        local _, _, _, wall = core.raycast(x, y + EYE, z, dx, dy, dz, math.min(range, 256))
        local t = wall or math.min(range, 256)
        return x + dx * t, y + EYE + dy * t, z + dz * t
    end

    function M.fire(p: number, s, second: boolean)
        local cur = M.seat_of[p]
        local m = cur and M.list[cur.id]
        if m == nil then return end
        local seat = m.k.seats[cur.seat]
        local gun = seat.gun
        if gun == nil or clock < (m.next[cur.seat] or 0) then return end
        local dx, dy, dz = core.player_look(p)
        if dx == nil then return end

        if gun == "mg" then
            m.next[cur.seat] = clock + 0.08
            local pid = m.parts[seat.part]
            local px, py, pz = core.ecs.get(pid, "x"), core.ecs.get(pid, "y"), core.ecs.get(pid, "z")
            if px == nil then return end
            local yaw, pitch = M.want(p, { px, py, pz })
            local cp = math.cos(pitch)
            dx, dy, dz = math.sin(yaw) * cp, math.sin(pitch), -math.cos(yaw) * cp
            bullet(p, s, { px + dx * 1.2, py + 0.05 + dy * 1.2, pz + dz * 1.2 }, jitter({ dx, dy, dz }, 0.02),
                   { range = 170, damage = 5 }, "mg", cur.id)
            core.play_sound("assault_game:mg", px, py, pz)
        elseif gun == "cannon" then
            m.next[cur.seat] = clock + 3
            M.cannon(cur.id, m, p, s.team)
            tell(p, "fired cannon")
        elseif gun == "rockets" then
            m.salvo -= 1
            m.next[cur.seat] = clock + (if m.salvo <= 0 then 5 else 0.3)
            if m.salvo <= 0 then m.salvo = 8 end
            local x, y, z = M.point(cur.id, if m.salvo % 2 == 0 then 1.15 else -1.15, 0.75, 1.8)
            local yaw, pitch = M.want(p, { x, y, z })
            local cp = math.cos(pitch)
            dx, dy, dz = math.sin(yaw) * cp, math.sin(pitch), -math.cos(yaw) * cp
            M.shoot("heli", { x, y, z }, { dx * 55, dy * 55, dz * 55 }, p, s.team, cur.id)
            core.play_sound("assault_game:rocket", x, y, z)
        elseif gun == "plane" then
            local pose = m.pose
            if pose == nil then return end
            if second then
                m.next[cur.seat] = clock + 3
                local x, y, z = M.point(cur.id, 0, -0.3, 0)
                M.shoot("bomb", { x, y, z }, { pose.vx, pose.vy, pose.vz }, p, s.team, cur.id)
                return
            end
            m.next[cur.seat] = clock + 0.07
            local cp = math.cos(pose.pitch)
            local d = { math.sin(pose.yaw) * cp, math.sin(pose.pitch), -math.cos(pose.yaw) * cp }
            m.side = -(m.side or 1)
            local x, y, z = M.point(cur.id, 2.2 * m.side, 0.5, 3.5)
            bullet(p, s, { x + d[1] * (pose.speed * 0.1), y + d[2] * (pose.speed * 0.1), z + d[3] * (pose.speed * 0.1) },
                   jitter(d, 0.012), { range = 180, damage = 4 }, "plane", cur.id)
            core.play_sound("assault_game:mg", x, y, z)
        elseif gun == "bomber" then
            if second or m.pose == nil then return end
            m.next[cur.seat] = clock + 9
            m.stick = { left = 6, at = clock, by = p, team = s.team }
            tell(p, "fired bomber")
        elseif gun == "aa" then
            if (m.lock_t or 0) < M.AA_LOCK then return end
            M.aa_fire(cur.id, m, p, s.team)
            tell(p, "fired aa")
        elseif gun == "mortar" or gun == "howitzer" then
            m.next[cur.seat] = clock + M.LOB[gun].cool
            local tx, ty, tz = M.target(p, M.LOB[gun].range)
            if tx ~= nil and M.lob(cur.id, m, cur.seat, tx, ty, tz, p, s.team) then tell(p, `fired {gun}`) end
        end
    end

    -- Where a bomb let go now would come down (22 down, the machine's speed on).
    function M.impact(m): (number, number, number)
        local pose = m.pose
        local alt = math.max(0, pose.y - GROUND)
        local t = (pose.vy + math.sqrt(math.max(0, pose.vy * pose.vy + 44 * alt))) / 22
        return pose.x + pose.vx * t, GROUND, pose.z + pose.vz * t
    end

    -- The bomb bay: one of the stick every 0.22 s, out of the middle.
    function M.drop_stick(id: number, m)
        local st = m.stick
        while st and st.left > 0 and clock >= st.at and m.pose do
            st.left -= 1
            st.at += 0.22
            local x, y, z = M.point(id, 0, 0.4, 0.4)
            M.shoot("bomb", { x, y, z }, { m.pose.vx, m.pose.vy, m.pose.vz }, st.by, st.team, id)
        end
        if st and st.left <= 0 then m.stick = nil end
    end

    -- Where the anti-air's tubes point.
    function M.aa_dir(id: number, m): ({number}?, {number}?)
        local pid = m.parts[1]
        local px, py, pz = core.ecs.get(pid, "x"), core.ecs.get(pid, "y"), core.ecs.get(pid, "z")
        if px == nil then return nil, nil end
        local a = m.aim[1]
        local yaw = (core.ecs.get(id, "yaw") or 0) + a.yaw
        local cp = math.cos(a.pitch)
        return { px, py, pz }, { math.sin(yaw) * cp, math.sin(a.pitch), -math.cos(yaw) * cp }
    end

    -- Holding a lock: the enemy machine in the air nearest the tubes' line,
    -- within AA_CONE, in sight; the time it has been the same one.
    function M.aa_lock(id: number, m, dt: number)
        local o, f = M.aa_dir(id, m)
        if o == nil then return end
        local best, score = nil, M.AA_CONE
        for _, t in enemies(m.team) do
            local tm = t.machine and M.list[t.machine]
            if not M.flying(tm) then continue end
            local dx, dy, dz = tm.pose.x - o[1], tm.pose.y + 1 - o[2], tm.pose.z - o[3]
            local l = math.sqrt(dx * dx + dy * dy + dz * dz)
            if l > M.AA_RANGE then continue end
            local ang = math.acos(math.clamp((dx * f[1] + dy * f[2] + dz * f[3]) / l, -1, 1))
            if ang < score then
                local _, _, _, wall = core.raycast(o[1], o[2], o[3], dx, dy, dz, l - 2)
                if wall == nil then best, score = t.machine, ang end
            end
        end
        if best ~= nil and best == m.lock_on then
            m.lock_t += dt
        else
            m.lock_on, m.lock_t = best, 0
        end
        local p = m.riders[1]
        if p and clock >= (m.lock_said or 0) then
            m.lock_said = clock + 0.1
            local tm = best and M.list[best]
            if tm then
                tell(p, string.format("lock %d %.2f %.1f %.1f %.1f", if m.lock_t >= M.AA_LOCK then 1 else 0,
                                      math.min(1, m.lock_t / M.AA_LOCK), tm.pose.x, tm.pose.y + 1, tm.pose.z))
            else
                tell(p, "lock none")
            end
        end
    end

    -- A missile out of the next tube, after what is locked: four, 1.2 s
    -- apart, then 8 s to load again.
    function M.aa_fire(id: number, m, by: number, team: number)
        if clock < (m.next[1] or 0) or m.lock_on == nil then return end
        local o, f = M.aa_dir(id, m)
        if o == nil then return end
        m.aa_left = (m.aa_left or 4) - 1
        m.next[1] = clock + (if m.aa_left <= 0 then 8 else 1.2)
        if m.aa_left <= 0 then m.aa_left = 4 end
        M.shoot("missile", { o[1] + f[1] * 1.4, o[2] + f[2] * 1.4 + 0.3, o[3] + f[3] * 1.4 },
                { f[1] * 30, f[2] * 30, f[3] * 30 }, by, team, id, m.lock_on)
        core.play_sound("assault_game:rocket", o[1], o[2], o[3])
    end

    -- A shell from seat i's lobbing gun onto tx, ty, tz.
    function M.lob(id: number, m, i: number, tx: number, ty: number, tz: number, by: number, team: number): boolean
        local seat = m.k.seats[i]
        local pid = m.parts[seat.part]
        local ox, oy, oz = core.ecs.get(pid, "x"), core.ecs.get(pid, "y"), core.ecs.get(pid, "z")
        if ox == nil then return false end
        oy += 1.0
        -- At the gun's angle the speed that comes down on the target:
        -- v^2 = g D^2 / (2 cos^2 a (D tan a - H)), a little off each time.
        local a = m.k.parts[seat.part].lob
        local hx, hz = tx - ox, tz - oz
        local D = math.sqrt(hx * hx + hz * hz) * (1 + (math.random() - 0.5) * 0.06)
        local H = ty - oy
        local under = 2 * math.cos(a) ^ 2 * (D * math.tan(a) - H)
        if D < 3 or under <= 0 then return false end
        local v = math.sqrt(22 * D * D / under)
        local l = math.sqrt(hx * hx + hz * hz)
        M.shoot(seat.gun, { ox, oy, oz }, { hx / l * v * math.cos(a), v * math.sin(a), hz / l * v * math.cos(a) },
                by, team, id)
        core.play_sound("assault_game:mortar", ox, oy, oz)
        return true
    end

    -- The tank's gun, along where its turret and barrel point.
    function M.cannon(id: number, m, by: number, team: number)
        local a, b = m.aim[1], m.aim[2]
        local yaw = (m.pose and m.pose.yaw or 0) + a.yaw
        local pitch = b.pitch + (m.pose and m.pose.pitch or 0)
        local d = { math.sin(yaw) * math.cos(pitch), math.sin(pitch), -math.cos(yaw) * math.cos(pitch) }
        local pid = m.parts[2]
        local px, py, pz = core.ecs.get(pid, "x"), core.ecs.get(pid, "y"), core.ecs.get(pid, "z")
        if px == nil then return end
        M.shoot("cannon", { px + d[1] * 3.8, py + d[2] * 3.8, pz + d[3] * 3.8 }, { d[1] * 70, d[2] * 70, d[3] * 70 },
                by, team, id)
        core.play_sound("assault_game:cannon", px, py, pz)
    end

    -- The driver's keys to the machine: on the ground as they are; in the air
    -- the mouse steers -- a helicopter turns to the look, a plane banks and
    -- pitches towards it, W and S its throttle.
    function M.drive(id: number, m, p: number)
        local s = players[p]
        local k = s and s.keys or { 0, 0, 0 }
        local craft, pose = m.k.craft, m.pose
        if craft == "wheeled" or craft == "tracked" or pose == nil then
            core.craft.keys(id, k[1], k[2], 0, 0)
            return
        end
        local dx, dy, dz = core.player_look(p)
        if dx == nil then return end
        local turn = wrap(math.atan2(dx, -dz) - pose.yaw)
        if craft == "rotor" then
            core.craft.keys(id, k[1], k[2], k[3], math.clamp(turn * 1.5, -1, 1))
        else
            local bank = math.clamp(turn * 1.2, -1, 1) * 0.9 + k[2] * 0.5
            local pitch = math.asin(math.clamp(dy, -1, 1))
            core.craft.keys(id, math.clamp(-(pitch - pose.pitch) * 2.5, -1, 1),
                            math.clamp((bank - pose.roll) * 2, -1, 1), k[1], math.clamp(turn, -1, 1) * 0.5)
        end
    end

    -- Each aiming part follows its gunner's look, within what it can do: a
    -- turret turns at its own pace, a barrel only rises and dips, a lobbing
    -- gun keeps its angle and turns to the target.
    function M.aim(id: number, m, dt: number)
        local hull_yaw = if m.pose then m.pose.yaw else (core.ecs.get(id, "yaw") or 0)
        for i, part in m.k.parts do
            local p = m.riders[part.by]
            local pid = m.parts[i]
            -- A bot at it aims at what it sees, and holds still when it sees nothing.
            local b = if p == nil and m.bots[part.by] then bots[m.bots[part.by]] else nil
            local tp = if b == nil then nil elseif part.lob then b.lob
                elseif b.enemy then { b.enemy.x, b.enemy.y, b.enemy.z } else nil
            if (p == nil and tp == nil) or pid == nil then continue end
            local px, py, pz = core.ecs.get(pid, "x"), core.ecs.get(pid, "y"), core.ecs.get(pid, "z")
            if px == nil then continue end
            local want_yaw, want_pitch, dist
            if p then
                want_yaw, want_pitch, dist = M.want(p, { px, py, pz })
            else
                -- A bot's hand is not steady: off by a little, new after each shell.
                want_yaw, want_pitch, dist = M.want_at(tp, { px, py, pz })
                want_yaw += b.err_yaw or 0
                want_pitch += b.err_pitch or 0
            end
            local carrier = hull_yaw + (if part.on then m.aim[part.on].yaw else 0)
            local a = m.aim[i]
            if part.turret then
                local d = wrap(want_yaw - carrier - a.yaw)
                a.yaw = wrap(a.yaw + math.clamp(d, -1.2 * dt, 1.2 * dt))
            elseif part.barrel then
                -- Raised for the shell's drop over that distance as well.
                local shot = M.SHOTS.cannon
                local lift = 0.5 * math.asin(math.min(1, shot.g * dist / (70 * 70)))
                local want = want_pitch + lift - (if m.pose then m.pose.pitch else 0)
                a.pitch += math.clamp(want - a.pitch, -0.8 * dt, 0.8 * dt)
                a.pitch = math.clamp(a.pitch, -0.15, 0.4)
            elseif part.lob then
                local tx, tz
                if p then
                    tx, _, tz = M.target(p, (M.LOB[m.k.seats[part.by].gun] or { range = 140 }).range)
                else
                    tx, tz = tp[1], tp[3]
                end
                if tx ~= nil then a.yaw = wrap(math.atan2(tx - px, -(tz - pz)) - carrier) end
            else
                a.yaw, a.pitch = wrap(want_yaw - carrier), if part.high then math.clamp(want_pitch, -0.1, 1.45)
                    else math.clamp(want_pitch, -0.5, 0.8)
            end
            core.craft.aim(pid, a.yaw, a.pitch)
        end
    end

    -- A tank pushing at what has stopped it, column by column across its
    -- front: one block high it climbs (the hop, core/physics/craft.cpp); two
    -- high it knocks the top one off and climbs the other; three or more it
    -- goes through, ground to roof, and what stood on that comes down.
    function M.breach(m, id: number)
        local pose = m.pose
        local hits = 0
        local across = math.ceil(m.k.hw + 0.4)
        for a = -across, across do
            local x, _, z = M.point(id, a, 0, m.k.hl + 0.6)
            local bx, bz = math.floor(x), math.floor(z)
            local by_ = math.floor(pose.y + 0.3)
            local high = 0
            while high < 4 do
                local block = core.get_block(bx, by_ + high, bz)
                if block == 0 or TOUGH[block] then break end
                high += 1
            end
            local from, to = if high == 2 then 1 else 0, if high == 2 then 1 else 2
            if high >= 2 then
                for level = from, to do
                    core.set_block(bx, by_ + level, bz, 0)
                    hits += 1
                end
                for up = to + 1, to + 3 do
                    local over = core.get_block(bx, by_ + up, bz)
                    if over == 0 or TOUGH[over] then break end
                    core.fall(bx, by_ + up, bz)
                end
            end
        end
        if hits > 0 then
            tell(0, string.format("dust %.1f %.1f %.1f", pose.x, pose.y + 1, pose.z))
        end
    end

    -- Moving fast through the other team: struck as by its weight.
    function M.run_over(id: number, m)
        local pose = m.pose
        local speed = math.abs(pose.speed)
        if speed < 4 then return end
        m.ran = m.ran or {}
        local s, c = math.sin(pose.yaw), math.cos(pose.yaw)
        for _, t in enemies(m.team) do
            if t.machine then continue end
            local key = t.player or -(t.bot or 0)
            if (m.ran[key] or 0) > clock then continue end
            local cx, cz = (t.lo[1] + t.hi[1]) / 2 - pose.x, (t.lo[3] + t.hi[3]) / 2 - pose.z
            local lx, lz = cx * c + cz * s, cx * s - cz * c
            if math.abs(lx) < m.k.hw + 0.4 and math.abs(lz) < m.k.hl + 0.4 and math.abs(t.feet - pose.y) < m.k.h then
                m.ran[key] = clock + 1
                hurt(m.riders[1] or (if m.bots[1] then -m.bots[1] else 0), t, math.floor(speed * 2), "roadkill", pose.x, pose.z)
                if t.player then
                    core.player_launch(t.player, pose.vx * 0.6 + lx * 2, 5, pose.vz * 0.6, 0.5)
                end
            end
        end
    end

    function M.step(dt: number)
        if not M.spawned and #core.players() > 0 then M.start() end
        for i = #M.respawn, 1, -1 do
            local r = M.respawn[i]
            if clock >= r.at then
                local x, z = math.floor(r.home[1]), math.floor(r.home[2])
                if core.loaded(x, GROUND, z) == 1 then
                    table.remove(M.respawn, i)
                    local id = M.spawn(r.kind, r.home[1], r.home[2], r.home[3], r.home[4])
                    if id then
                        M.list[id].again = r.again
                        if r.then_ then r.then_(id) end
                    end
                else
                    core.keep_loaded(x, z, 8)
                    r.at = clock + 1
                end
            end
        end
        for i = #M.wrecks, 1, -1 do
            local w = M.wrecks[i]
            if w.gone == 0 then
                w.vy -= 25 * dt
                w.y = math.max(w.ground, w.y + w.vy * dt)
                core.ecs.set(w.id, "y", w.y)
                if w.y <= w.ground then
                    w.gone = clock + 5.2
                    core.ecs.clip(w.id, "death")
                    tell(0, string.format("burn %.1f %.1f %.1f", w.x, w.y, w.z))
                end
            elseif clock >= w.gone then
                core.ecs.kill(w.id)
                table.remove(M.wrecks, i)
            end
        end
        local hud = clock >= (M.hud_at or 0)
        if hud then M.hud_at = clock + 0.25 end
        for id, m in M.list do
            if m.dead then continue end
            if m.k.craft then
                m.pose = core.craft.pose(id)
                if m.pose == nil then
                    M.list[id] = nil
                    continue
                end
                local pose = m.pose
                -- A crash: a knock past what a landing takes.
                local hard = if m.k.craft == "wheeled" or m.k.craft == "tracked" then 16 else 12
                if pose.impact > hard then M.damage(id, (pose.impact - hard) * 5, "crash", m.last_by or 0) end
                if m.dead then continue end
                if A.in_lava(pose.x, pose.y, pose.z) then
                    M.destroy(id, m.last_by or 0, "lava")
                    continue
                end
                -- Off the field, or under it (the escape's tunnel goes down to -6;
                -- under -16 the engine has done with a body).
                if pose.y < -14 or pose.x < -20 or pose.z < -20 or pose.x > SIZE + 20 or pose.z > 1100 then
                    M.destroy(id, m.last_by or 0, "crash")
                    continue
                end
                local driver = m.riders[1]
                if driver then
                    M.drive(id, m, driver)
                elseif m.bots[1] then
                    if m.k.craft == "rotor" or m.k.craft == "wing" then M.bot_fly(id, m) else M.bot_drive(id, m) end
                else
                    -- Nobody at the stick: what flies comes down.
                    core.craft.keys(id, 0, 0, if m.k.craft == "rotor" or m.k.craft == "wing" then -1 else 0, 0)
                end
                if m.k.clip then
                    local moving = math.abs(pose.speed) > 0.5
                    if moving ~= m.moving then
                        m.moving = moving
                        core.ecs.clip(id, if moving then m.k.clip else "")
                    end
                end
                -- Its engine, a stretch at a time while somebody drives.
                if (driver or m.bots[1]) and clock >= (m.sound_at or 0) then
                    local snd = M.SOUND[m.k.craft]
                    m.sound_at = clock + snd[2]
                    core.play_sound(snd[1], pose.x, pose.y + 1, pose.z)
                end
                local pushing = (driver and players[driver] and players[driver].keys[1] > 0)
                    or (driver == nil and m.bots[1] ~= nil and (m.bot_fwd or 0) > 0)
                if m.k.breaks and pushing and math.abs(pose.speed) < 1.5 then
                    M.breach(m, id)
                end
                M.run_over(id, m)
            end
            M.aim(id, m, dt)
            M.carry(id, m)
            if m.stick then M.drop_stick(id, m) end
            if m.kind == "aa" then
                if m.riders[1] or m.bots[1] then M.aa_lock(id, m, dt) else m.lock_on = nil end
            end
            if hud then
                for i, p in m.riders do
                    local pose = m.pose
                    local wait = math.max(0, (m.next[i] or 0) - clock)
                    tell(p, string.format("mach %d %d %.1f %.1f %.2f %.1f", math.max(0, math.floor(m.hp)), math.floor(m.max or m.k.hp),
                                          if pose then math.abs(pose.speed) else 0, if pose then pose.y - GROUND else 0,
                                          if pose then pose.throttle else 0, wait))
                    local gun = m.k.seats[i].gun
                    if gun == "bomber" and m.pose then
                        local ix, iy, iz = M.impact(m)
                        tell(p, string.format("mark %.1f %.1f %.1f", ix, iy, iz))
                    end
                    if M.LOB[gun] then
                        local tx, ty, tz = M.target(p, M.LOB[gun].range)
                        if tx then tell(p, string.format("mark %.1f %.1f %.1f", tx, ty, tz)) end
                    end
                end
            end
        end
        M.step_shots(dt)
    end

    -- ---------------------------------------------------------------- bots at the wheel
    --
    -- A bot in a machine is carried by this mod and not by the engine (whose
    -- seats are players'): put at its seat every tick. Bots drive the jeep and
    -- the tank to their point, wait a moment for the others near to get in,
    -- and the jeep's let off there; they take the guns and the free seats of
    -- a machine one of their team drives and has stopped near them -- a
    -- player's too -- and get off when nobody drives it.

    -- Where seat i is now: on the hull, or on its part (a turret) as it turns.
    function M.seat_pos(id: number, m, i: number): (number, number, number)
        local seat = m.k.seats[i]
        local pid = seat.on and m.parts[seat.on]
        local px = pid and core.ecs.get(pid, "x")
        if px then
            local yaw = (if m.pose then m.pose.yaw else (core.ecs.get(id, "yaw") or 0)) + m.aim[seat.on].yaw
            local sn, c = math.sin(yaw), math.cos(yaw)
            return px + c * seat[1] + sn * seat[3], core.ecs.get(pid, "y") + seat[2], core.ecs.get(pid, "z") + sn * seat[1] - c * seat[3]
        end
        return M.point(id, seat[1], seat[2], seat[3])
    end

    -- The seats a bot takes: the driver's of a jeep or a tank, and any other
    -- whose gun it can fire (or that has none).
    M.BOT_DRIVES = { jeep = true, tank = true, heli = true, plane = true, bomber = true }
    M.BOT_GUNS = { mg = true, cannon = true, mortar = true, howitzer = true, rockets = true, plane = true,
                   bomber = true, aa = true }
    function M.bot_seat_ok(m, i: number): boolean
        local seat = m.k.seats[i]
        if seat == nil or m.riders[i] or m.bots[i] then return false end
        if i == 1 and m.k.craft then return M.BOT_DRIVES[m.kind] == true end
        return seat.gun == nil or M.BOT_GUNS[seat.gun] == true
    end

    -- A player of `team` within r of the machine: it is left to them.
    function M.player_near(m, team: number, r: number): boolean
        for p, s in players do
            local x, _, z = core.player_pos(p)
            if s.team == team and x and (x - m.pose.x) ^ 2 + (z - m.pose.z) ^ 2 < r * r then return true end
        end
        return false
    end

    function M.bot_board(bid: number, b, id: number, i: number): boolean
        local m = M.list[id]
        if m == nil or m.dead or not M.bot_seat_ok(m, i) then return false end
        m.bots[i] = bid
        b.ride = { id = id, seat = i }
        b.busy = clock
        b.phase, b.hold_fire, b.enemy = nil, false, nil
        m.team = b.team
        if i == 1 then m.wait_until = clock + 4 end
        return true
    end

    function M.bot_leave(bid: number, b)
        local cur = b.ride
        if cur == nil then return end
        b.ride = nil
        b.last = nil
        b.carried = nil
        core.mob_ride(bid, 0)
        b.off_until = clock + 10   -- not straight back in
        local m = M.list[cur.id]
        if m == nil then return end
        m.bots[cur.seat] = nil
        if next(m.riders) == nil and next(m.bots) == nil then m.team = m.home[4] end
        local side = if cur.seat % 2 == 0 then 1 else -1
        local x, _, z = M.point(cur.id, side * ((m.k.hw or 0.6) + 1.2), 0, 0)
        core.mob_place(bid, x, feet_at(math.floor(x), math.floor(z)), z)
        M.bot_clip(bid, b, "")
    end

    -- On foot and nothing in sight: a seat in a machine of its team that
    -- somebody drives and that has stopped near, or -- its point far off -- a
    -- jeep or a tank of its team with nobody at the wheel. Walks to it, and in.
    function M.bot_hop(bid: number, b, x: number, z: number): boolean
        if clock < (b.off_until or 0) then return false end
        local p = goal_of(bid, b)
        local far = (p[1] - x) ^ 2 + (p[2] - z) ^ 2 > 60 * 60
        local best, at, seat = nil, 1e9, 0
        for id, m in M.list do
            if m.dead then continue end
            if not m.k.craft then
                -- The watch takes a gun that stays put: a nest, a mortar, a howitzer.
                local sx, _, sz = M.where(id)
                local d = (sx - x) ^ 2 + (sz - z) ^ 2
                local want = if m.kind == "aa" then d < 20 * 20 and M.air_near(b.team, sx, sz)
                    else b.role == "watch" and d < 15 * 15
                if want and d < at and M.bot_seat_ok(m, 1) and (m.home[4] == 0 or m.home[4] == b.team) then
                    best, at, seat = id, d, 1
                end
                continue
            end
            if m.pose == nil then continue end
            local d = (m.pose.x - x) ^ 2 + (m.pose.z - z) ^ 2
            if d >= at then continue end
            local driven = m.riders[1] ~= nil or m.bots[1] ~= nil
            if driven and m.team == b.team and d < 14 * 14 and math.abs(m.pose.speed) < 2 then
                for i = 2, #m.k.seats do
                    if M.bot_seat_ok(m, i) then
                        best, at, seat = id, d, i
                        break
                    end
                end
            elseif not driven and far and d < 30 * 30 and M.bot_seat_ok(m, 1) and not M.player_near(m, b.team, 25)
                and (m.team == b.team or (next(m.riders) == nil and next(m.bots) == nil and m.home[4] == b.team))
                and (m.claim == nil or m.claim[1] == bid or clock > m.claim[2]) then
                best, at, seat = id, d, 1
            end
        end
        if best == nil then return false end
        local m = M.list[best]
        if seat == 1 and m.k.craft then m.claim = { bid, clock + 15 } end
        local reach = math.max(m.k.hw or 0.6, m.k.hl or 0.6) + 2
        if at < reach * reach then return M.bot_board(bid, b, best, seat) end
        local mx, _, mz = M.where(best)
        local tx, tz = math.floor(mx), math.floor(mz)
        core.mob_goto(bid, tx, feet_at(tx, tz), tz)
        return true
    end

    function M.bot_ride_think(bid: number, b)
        local cur = b.ride
        local m = M.list[cur.id]
        if m == nil or m.dead or (m.k.craft and m.pose == nil) then
            b.ride, b.carried = nil, nil
            core.mob_ride(bid, 0)
            return
        end
        local gun = m.k.seats[cur.seat].gun
        b.enemy = if gun == "aa" then M.aa_pick(bid, b) else spotted(bid, b)
        if M.LOB[gun] then b.lob = M.lob_target(bid, b, M.LOB[gun].range) end
        if b.enemy or b.lob then b.busy = clock end
        if not m.k.craft then
            -- At a gun that stays put: off it after 40 s with nothing to shoot
            -- (the legion stays at its post).
            if b.team == 1 and clock - (b.busy or clock) > 40 then M.bot_leave(bid, b) end
            return
        end
        if cur.seat == 1 and b.crew then
            A.crew_goal(m)   -- down the escape road
        elseif cur.seat == 1 then
            local p = goal_of(bid, b)
            m.goal_x, m.goal_z = p[1] + (b.n % 3 - 1) * 4, p[2]
            -- The jeep lets everybody off at the point; the tank stays in it.
            if m.kind == "jeep" and (p[1] - m.pose.x) ^ 2 + (p[2] - m.pose.z) ^ 2 < 20 * 20 then
                for _, other in m.bots do
                    if bots[other] then M.bot_leave(other, bots[other]) end
                end
            end
        elseif m.riders[1] == nil and m.bots[1] == nil then
            m.alone = m.alone or clock
            if clock - m.alone > 3 then M.bot_leave(bid, b) end
        else
            m.alone = nil
            -- A passenger without a gun gets off where the machine stops a while near the fight.
            if m.k.seats[cur.seat].gun == nil and b.enemy and math.abs(m.pose.speed) < 1 then M.bot_leave(bid, b) end
        end
    end

    -- An enemy machine in the air within the anti-air's reach of x, z.
    function M.air_near(team: number, x: number, z: number): boolean
        for _, m in M.list do
            if m.team ~= team and m.team ~= 0 and M.flying(m)
                and (m.pose.x - x) ^ 2 + (m.pose.z - z) ^ 2 < M.AA_RANGE * M.AA_RANGE then
                return true
            end
        end
        return false
    end

    -- At the anti-air: the nearest enemy machine in the air it can see.
    function M.aa_pick(bid: number, b)
        local x, y, z = core.mob_pos(bid)
        if x == nil then return nil end
        local best, at = nil, M.AA_RANGE
        for id, m in M.list do
            if m.team == b.team or m.team == 0 or not M.flying(m) then continue end
            local dx, dy, dz = m.pose.x - x, m.pose.y + 1 - (y + EYE), m.pose.z - z
            local d = math.sqrt(dx * dx + dy * dy + dz * dz)
            if d < at then
                local _, _, _, wall = core.raycast(x, y + EYE, z, dx, dy, dz, d - 2)
                if wall == nil then
                    best, at = { x = m.pose.x, y = m.pose.y + 1, z = m.pose.z, d = d, key = 100000 + id }, d
                end
            end
        end
        return best
    end

    -- What the team has seen in the last 3 s that a lobbing gun at the bot
    -- reaches (20 nodes on), a few nodes off; nil when nothing.
    function M.lob_target(bid: number, b, range: number): {number}?
        local x, _, z = core.mob_pos(bid)
        if x == nil then return nil end
        local best, at = nil, range * range
        for _, e in M.seen[b.team] do
            local d = (e.x - x) ^ 2 + (e.z - z) ^ 2
            if clock - e.t < 3 and d > 400 and d < at then best, at = e, d end
        end
        if best == nil then return nil end
        local off = 3 + math.sqrt(at) / 40
        return { best.x + (math.random() * 2 - 1) * off, best.y - 1.3, best.z + (math.random() * 2 - 1) * off }
    end

    -- A bot at the wheel: towards its point, turning as it goes; backing off a
    -- moment when it has not moved for a while; the tank stops to fight what
    -- it sees within 70.
    function M.bot_drive(id: number, m)
        local pose, b = m.pose, bots[m.bots[1]]
        m.bot_fwd = 0
        if b == nil or m.goal_x == nil or clock < (m.wait_until or 0) then
            core.craft.keys(id, 0, 0, 0, 0)
            return
        end
        local dx, dz = m.goal_x - pose.x, m.goal_z - pose.z
        if dx * dx + dz * dz < 64 or (m.kind == "tank" and b.enemy and b.enemy.d < 70) then
            core.craft.keys(id, 0, 0, 0, 0)
            return
        end
        if math.abs(pose.speed) < 0.8 then m.stuck = m.stuck or clock else m.stuck = nil end
        if m.stuck and clock - m.stuck > 2.5 then
            m.back_until, m.stuck = clock + 1.5, nil
            m.steer_at = clock + 1.5   -- a new way looked for once backed off
        end
        -- Far from its goal and off the road, it heads for the road first:
        -- the road is kept clear of trees and crystals all the way.
        local want = math.atan2(dx, -dz)
        local d = math.sqrt(dx * dx + dz * dz)
        if not b.crew and d > 40 and math.abs(pose.x - MID) > 8 and A.stage == "fort" then
            local ahead = pose.z + (if dz > 0 then 20 else -20)
            want = math.atan2(MID + 0.5 - pose.x, -(ahead - pose.z))
        end
        if clock >= (m.steer_at or 0) then
            m.steer_at = clock + 0.25
            m.steer = M.feel(m, pose, want, not b.crew)
        end
        local fwd, right
        if m.steer == nil then
            -- Nothing free ahead: back off a little, turning the other way.
            m.back_until = math.max(m.back_until or 0, clock + 1.2)
            m.steer = wrap(pose.yaw + math.pi * 0.5)
        end
        local turn = wrap(m.steer - pose.yaw)
        fwd, right = if math.abs(turn) > 1.2 then 0.4 elseif math.abs(turn) > 0.5 then 0.7 else 1, math.clamp(turn * 2, -1, 1)
        if clock < (m.back_until or 0) then fwd, right = -1, -right end
        m.bot_fwd = fwd
        core.craft.keys(id, fwd, right, 0, 0)
    end

    -- How far a machine at `pose` can go along `yaw`: each of the lines its
    -- width covers is walked a node at a time over the ground as it is -- a
    -- step one high is climbed, two high (a trunk, a wall) stops it, as does
    -- anything over the ground where its body passes (a low branch) and, with
    -- `holes`, ground that falls away three or more. Out to `far`.
    function M.reach(m, pose, yaw: number, far: number, holes: boolean): number
        local fx, fz = math.sin(yaw), -math.cos(yaw)
        local y0 = math.floor(pose.y + 0.3)
        local w = math.ceil(m.k.hw or 1)
        local tall = if (m.k.h or 2) > 2.2 then 2 else 1   -- cells over the feet its body fills
        local reach = far
        for side = -w, w do
            local lvl = y0
            for dist = 1, reach do
                local cx = math.floor(pose.x + fx * dist - fz * side)
                local cz = math.floor(pose.z + fz * dist + fx * side)
                if core.get_block(cx, lvl, cz) ~= 0 then
                    lvl += 1   -- a step up, if there is room over it
                end
                local clear = true
                for k = 0, tall do
                    if core.get_block(cx, lvl + k, cz) ~= 0 then clear = false end
                end
                if not clear then
                    reach = dist
                    break
                end
                local drop = 0
                while drop < 3 and core.get_block(cx, lvl - 1, cz) == 0 do
                    lvl -= 1
                    drop += 1
                end
                if (holes and drop >= 3) or core.get_block(cx, lvl - 1, cz) == A.LAVA then
                    reach = dist
                    break
                end
            end
        end
        return reach
    end

    -- The way to steer for `want`: itself if free for a while, else the
    -- nearest way to it either side that is; nil when nothing is free near.
    M.FEEL = { 0, 0.3, -0.3, 0.6, -0.6, 0.9, -0.9, 1.25, -1.25, 1.6, -1.6, 2.1, -2.1 }
    function M.feel(m, pose, want: number, holes: boolean): number?
        local far = if m.kind == "tank" then 12 else 10
        local best, at = nil, 3
        for _, off in M.FEEL do
            local r = M.reach(m, pose, want + off, far, holes)
            if r >= far then return wrap(want + off) end
            if r > at then best, at = wrap(want + off), r end
        end
        return best
    end

    -- A bot at the stick. The helicopter climbs to 18 over the ground, goes
    -- to its point and stays over it, turned to what it sees. The plane
    -- runs up to speed down the way it was parked, lifts off, climbs to 45
    -- and circles over the point, 75 out; pulled up when it is low and nose
    -- down. Both drop or fire on
    -- their own; the plane's bombs go when where they would land is within
    -- 8 of something the team has seen.
    function M.bot_fly(id: number, m)
        local pose, b = m.pose, bots[m.bots[1]]
        if b == nil or m.goal_x == nil or clock < (m.wait_until or 0) then
            core.craft.keys(id, 0, 0, if m.k.craft == "wing" then -1 else 0, 0)
            return
        end
        local ground = feet_at(math.floor(pose.x), math.floor(pose.z))
        local alt = pose.y - ground
        -- Stuck on the ground against something: everybody out.
        if alt < 2 and math.abs(pose.speed) < 1 then m.stuck = m.stuck or clock else m.stuck = nil end
        if m.stuck and clock - m.stuck > 8 then
            m.stuck = nil
            for _, other in m.bots do
                if bots[other] then M.bot_leave(other, bots[other]) end
            end
            return
        end
        if m.k.craft == "rotor" then
            local gx, gz = m.goal_x, m.goal_z
            local dx, dz = gx - pose.x, gz - pose.z
            local d = math.sqrt(dx * dx + dz * dz)
            local face = math.atan2(dx, -dz)
            if b.enemy then face = math.atan2(b.enemy.x - pose.x, -(b.enemy.z - pose.z)) end
            local turn = wrap(face - pose.yaw)
            local go = wrap(math.atan2(dx, -dz) - pose.yaw)
            -- Forward and sideways along where the point is, from the way it faces.
            local fwd = if alt < 6 then 0 else math.clamp(d / 40, 0, 1) * math.cos(go)
            local right = if alt < 6 then 0 else math.clamp(d / 40, 0, 1) * math.sin(go)
            core.craft.keys(id, fwd, right, math.clamp((GROUND + 18 - pose.y) * 0.25 + (ground - GROUND) * 0.1, -1, 1),
                            math.clamp(turn * 1.5, -1, 1))
            return
        end
        -- The plane: down the way it was parked until it flies, then round
        -- the point, 75 out, the circle kept over the field.
        local big = m.kind == "bomber"
        local R = if big then 85 else 75
        local cx = math.clamp(m.goal_x, R + 35, SIZE - R - 35)
        local cz = math.clamp(m.goal_z, R + 35, SIZE - R - 35)
        local dx, dz = pose.x - cx, pose.z - cz
        local r = math.max(1, math.sqrt(dx * dx + dz * dz))
        local heading
        if alt < 3 and pose.speed < (if big then 17 else 20) then
            heading = m.home[3]
        elseif r > R * 1.3 then
            heading = math.atan2(-dx, dz)   -- back towards it
        else
            -- Along the circle, turned in or out by how far off it is.
            heading = math.atan2(-dx, dz) + math.pi / 2 - math.clamp((r - R) / R, -1, 1) * 0.9
        end
        local turn = wrap(heading - pose.yaw)
        local want_pitch = 0
        local lift = if big then 17 else 20
        if pose.speed > lift or alt > 3 then
            want_pitch = math.clamp((GROUND + (if big then 55 else 45) - pose.y) / 30, -0.3, 0.35)
        end
        if alt < 14 and pose.pitch < 0.05 and pose.speed > lift then want_pitch = 0.35 end
        local bank = if alt > 6 then math.clamp(turn * 1.2, -1, 1) * 0.8 else 0
        core.craft.keys(id, math.clamp(-(want_pitch - pose.pitch) * 2.5, -1, 1), math.clamp((bank - pose.roll) * 2, -1, 1),
                        1, math.clamp(turn * 2, -1, 1))
        if alt > 10 and clock >= (m.next[1] or 0) and clock >= (m.bomb_at or 0) then
            local t = (pose.vy + math.sqrt(math.max(0, pose.vy * pose.vy + 44 * alt))) / 22
            local ix, iz = pose.x + pose.vx * t, pose.z + pose.vz * t
            for _, e in M.seen[b.team] do
                if clock - e.t < 4 and (e.x - ix) ^ 2 + (e.z - iz) ^ 2 < (if big then 144 else 64) then
                    if big then
                        -- The stick starts short of them and walks across.
                        m.bomb_at = clock + 9
                        m.stick = { left = 6, at = clock, by = -m.bots[1], team = b.team }
                    else
                        m.bomb_at = clock + 3
                        local x, y, z = M.point(id, 0, -0.3, 0)
                        M.shoot("bomb", { x, y, z }, { pose.vx, pose.vy, pose.vz }, -m.bots[1], b.team, id)
                    end
                    break
                end
            end
        end
    end

    -- Every tick: the bots at their seats, and the guns they hold fired at
    -- what they see -- the machine gun in bursts, the cannon once the turret
    -- is round to it.
    function M.carry(id: number, m)
        for i, bid in m.bots do
            local b = bots[bid]
            if b == nil or b.dead or b.ride == nil or b.ride.id ~= id then
                m.bots[i] = nil
                continue
            end
            local x, y, z = M.seat_pos(id, m, i)
            local seat = m.k.seats[i]
            -- Carried by the engine (core.mob_ride), the same tick the machine
            -- moves. A seat's place is a robot's, whose seated hips are 0.27
            -- behind its feet; a soldier's are over them.
            -- Should it ever be left behind (it has been seen sitting in the
            -- air with the jeep gone), it is put back on and carried again.
            if b.carried == id then
                local mx, my, mz = core.mob_pos(bid)
                if mx ~= nil and (mx - x) ^ 2 + (my - y) ^ 2 + (mz - z) ^ 2 > 9 then
                    core.log(string.format("assault_game: a %s off its seat on a %s (%.1f away), carried again",
                                           b.role, m.kind, math.sqrt((mx - x) ^ 2 + (my - y) ^ 2 + (mz - z) ^ 2)))
                    b.carried = nil
                end
            end
            if b.carried ~= id then
                b.carried = id
                local carrier = if seat.on then m.parts[seat.on] else id
                if core.mob_ride(bid, carrier, seat[1], seat[2], seat[3] - (if seat.stand then 0 else 0.27)) == 0 then
                    core.mob_place(bid, x, y, z)   -- not carried: placed each tick, a tick behind
                    b.carried = nil
                end
            end
            -- Turned the way its gun points (a launcher, a gun on a pintle),
            -- or the way the machine does: placing it does not turn it.
            local pid = if seat.part then m.parts[seat.part] else nil
            local yaw = (pid and core.ecs.get(pid, "yaw")) or select(4, M.where(id))
            M.face(bid, b, x + math.sin(yaw) * 4, z - math.cos(yaw) * 4)
            local e = b.enemy
            M.bot_clip(bid, b, if not seat.stand then "sit" elseif e and seat.gun then "fire" else "",
                       seat.gun ~= nil or i == 1)
            -- A passenger shoots its own gun from its seat.
            if seat.gun == nil and i ~= 1 and e then M.bot_trigger(bid, b) end
            if seat.gun == nil or clock < (m.next[i] or 0) then continue end
            if M.LOB[seat.gun] then
                if b.lob then
                    m.next[i] = clock + M.LOB[seat.gun].cool + 2 + math.random() * 2
                    M.lob(id, m, i, b.lob[1], b.lob[2], b.lob[3], -bid, b.team)
                end
                continue
            end
            if seat.gun == "aa" then
                if e and (m.lock_t or 0) >= M.AA_LOCK then M.aa_fire(id, m, -bid, b.team) end
                continue
            end
            if e == nil or clock < (b.react_at or 0) then continue end
            if seat.gun == "mg" then
                local pid = m.parts[seat.part]
                local px, py, pz = core.ecs.get(pid, "x"), core.ecs.get(pid, "y"), core.ecs.get(pid, "z")
                if px == nil then continue end
                b.burst -= 1
                if b.burst <= 0 then
                    b.burst = math.random(5, 9)
                    m.next[i] = clock + 0.8 + math.random() * 0.4
                else
                    m.next[i] = clock + 0.12
                end
                local dx, dy, dz = e.x - px, e.y - py, e.z - pz
                local l = math.sqrt(dx * dx + dy * dy + dz * dz)
                local d = jitter({ dx / l, dy / l, dz / l }, 0.04 * (1 + l / 40))
                bullet(-bid, b, { px + d[1] * 1.2, py + 0.05 + d[2] * 1.2, pz + d[3] * 1.2 }, d,
                       { range = 170, damage = 4 }, "mg", id)
                core.play_sound("assault_game:mg", px, py, pz)
            elseif seat.gun == "rockets" and m.pose then
                -- At what is in front of the nose, a pod at a time.
                local fx, fz = math.sin(m.pose.yaw), -math.cos(m.pose.yaw)
                local dx, dy, dz = e.x - m.pose.x, e.y - m.pose.y, e.z - m.pose.z
                local l = math.sqrt(dx * dx + dy * dy + dz * dz)
                if l < 120 and (dx * fx + dz * fz) / math.max(0.01, math.sqrt(dx * dx + dz * dz)) > 0.85 then
                    m.salvo -= 1
                    m.next[i] = clock + (if m.salvo <= 0 then 6 else 0.35)
                    if m.salvo <= 0 then m.salvo = 8 end
                    local x, y, z = M.point(id, if m.salvo % 2 == 0 then 1.15 else -1.15, 0.75, 1.8)
                    local d = jitter({ dx / l, dy / l, dz / l }, 0.03)
                    M.shoot("heli", { x, y, z }, { d[1] * 55, d[2] * 55, d[3] * 55 }, -bid, b.team, id)
                    core.play_sound("assault_game:rocket", x, y, z)
                end
            elseif seat.gun == "plane" and m.pose then
                -- The wing guns, along the nose, when it points at them.
                local pose = m.pose
                local cp = math.cos(pose.pitch)
                local d = { math.sin(pose.yaw) * cp, math.sin(pose.pitch), -math.cos(pose.yaw) * cp }
                local dx, dy, dz = e.x - pose.x, e.y - pose.y, e.z - pose.z
                local l = math.sqrt(dx * dx + dy * dy + dz * dz)
                if l < 130 and (dx * d[1] + dy * d[2] + dz * d[3]) / l > 0.985 then
                    m.next[i] = clock + 0.09
                    m.side = -(m.side or 1)
                    local x, y, z = M.point(id, 2.2 * m.side, 0.5, 3.5)
                    bullet(-bid, b, { x + d[1] * pose.speed * 0.1, y + d[2] * pose.speed * 0.1, z + d[3] * pose.speed * 0.1 },
                           jitter(d, 0.015), { range = 180, damage = 3 }, "plane", id)
                    core.play_sound("assault_game:mg", x, y, z)
                end
            elseif seat.gun == "cannon" and m.pose then
                local pid = m.parts[2]
                local px, pz = core.ecs.get(pid, "x"), core.ecs.get(pid, "z")
                if px == nil then continue end
                local off = wrap(math.atan2(e.x - px, -(e.z - pz)) - m.pose.yaw - m.aim[1].yaw)
                if math.abs(off) < 0.08 then
                    m.next[i] = clock + 5.5 + math.random() * 1.5
                    M.cannon(id, m, -bid, b.team)
                    b.err_yaw, b.err_pitch = (math.random() - 0.5) * 0.07, (math.random() - 0.5) * 0.03
                end
            end
        end
    end

    -- ---------------------------------------------------------------- the assault

    A.FOE_CLOTH, A.ALLY_CLOTH = core.node_id("assault_core:foe"), core.node_id("assault_core:ally")
    -- Each stronghold's garrison at one player, two more for each other; where
    -- its posts are drawn from (x0, x1, z0, z1); where its flag's cloth hangs
    -- (assault_core's poles: x, z, the cloth's lowest cell); and where the
    -- allies come back once it is theirs.
    A.GARRISON = { 8, 11, 14, 18 }
    A.AREA = { { 50, 78, 236, 264 }, { 42, 86, 466, 496 }, { 30, 98, 686, 720 }, { 26, 102, 876, 936 } }
    A.POLE = { { MID, 250, GROUND + 4 }, { MID, 483, GROUND + 11 }, { MID, 710, GROUND + 8 }, { MID, 905, GROUND + 5 } }
    -- Out on the open ground north of each, on the road on to the next (the
    -- fortress's in its courtyard, clear of its cover).
    A.BASE = { { MID, 276 }, { MID, 506 }, { MID, 742 }, { MID, 915 } }
    A.REINFORCE = 15        -- seconds between waves of the allies' fallen
    A.INCOME = { 5, 5 }     -- credits, every so many seconds, while there is a fight
    A.STUCK = 40            -- seconds a machine of the allies may sit still away from the park

    -- The n-th place in the base's park: side by side either side of the
    -- road, six apart, a little north of the base, facing north.
    function A.park(n: number): { number }
        local side = if n % 2 == 1 then -1 else 1
        local x = A.base[1] + 0.5 + side * (8 + 6 * ((n - 1) // 2))
        return { math.clamp(x, 8.5, 119.5), A.base[2] + 6.5, math.pi, 1 }
    end

    -- A machine of the allies nobody drives that has sat still, away from the
    -- park, for A.STUCK seconds -- caught on a wall, in a crater, on a hill
    -- it cannot climb -- is taken away and stands in the park again.
    function A.unstick()
        if clock < (A.stuck_at or 0) then return end
        A.stuck_at = clock + 5
        local n = 0
        for id, m in M.list do
            if m.home[4] ~= 1 or m.escape or m.dead or not m.k.craft then continue end
            n += 1
            local x, _, z = M.where(id)
            local st = m.still
            if next(m.riders) ~= nil or st == nil or (x - st[1]) ^ 2 + (z - st[2]) ^ 2 > 9 then
                m.still = { x, z, clock }
            elseif clock - st[3] > A.STUCK and (x - m.home[1]) ^ 2 + (z - m.home[2]) ^ 2 > 15 * 15 then
                A.repark(id, m, n)
                core.log(`assault_game: a stuck {m.kind} back to the park`)
            end
        end
    end

    -- Machine id taken away and put down again at the park's n-th place.
    function A.repark(id: number, m, n: number)
        for _, bid in m.bots do
            if bots[bid] then M.bot_leave(bid, bots[bid]) end
        end
        for _, pid in m.parts do core.ecs.kill(pid) end
        core.ecs.kill(id)
        M.list[id] = nil
        local h = A.park(n)
        M.order(m.kind, h[1], h[2], h[3], 1, m.again)
    end

    -- The slow supply: a few credits every few seconds while there is a fight.
    function A.supply()
        if not (A.stage == "fort" or A.stage == "hall") then return end
        if clock < (A.income_at or 0) then return end
        if A.income_at then A.earn(A.INCOME[1] + A.ZONE_INCOME * A.zones_held()) end   -- not the very first call
        A.income_at = clock + A.INCOME[2]
    end

    -- ------------------------------------------------ the bonus grounds
    -- A clearing either side of each stronghold (assault_core) with three of
    -- the legion on it. Taken by standing by its beacon with none of the
    -- legion near, as a flag; held, it adds ZONE_INCOME to every supply.
    A.ZONE_INCOME = 3
    A.ZONES = {}
    for k, f in FORTS do
        for _, zx in { 10, 118 } do
            table.insert(A.ZONES, { x = zx, z = f[2], fort = k, owner = 2, progress = 0 })
        end
    end

    function A.zones_held(): number
        local n = 0
        for _, zn in A.ZONES do
            if zn.owner == 1 then n += 1 end
        end
        return n
    end

    -- The ground an ally sent for one goes to: the nearest not held, of the
    -- strongholds reached so far.
    function A.zone_for(b): any
        local best, at = nil, 1e9
        for _, zn in A.ZONES do
            if zn.owner ~= 1 and zn.fort <= A.fort then
                local d = math.abs(zn.z - A.base[2]) + math.abs(zn.x - MID)
                if d < at then best, at = zn, d end
            end
        end
        return best
    end

    function A.zone_cloth(zn)
        local cloth = if zn.owner == 1 then A.ALLY_CLOTH else A.FOE_CLOTH
        for x = zn.x - 1, zn.x + 1 do
            for y = GROUND + 4, GROUND + 5 do core.set_block(x, y, zn.z, cloth) end
        end
    end

    function A.step_zones()
        if clock < (A.zones_at or 0) then return end
        A.zones_at = clock + 1
        for _, zn in A.ZONES do
            if zn.owner == 1 or zn.fort > A.fort then continue end
            local allies, foes = 0, 0
            for p in players do
                local x, _, z = core.player_pos(p)
                if x and core.health(p) > 0 and (x - zn.x) ^ 2 + (z - zn.z) ^ 2 < 49 then allies += 1 end
            end
            for id, b in bots do
                if b.dead then continue end
                local x, _, z = core.mob_pos(id)
                if x == nil then continue end
                local d = (x - zn.x) ^ 2 + (z - zn.z) ^ 2
                if b.team == 1 and not b.builder and d < 49 then allies += 1 end
                if b.team == 2 and d < 196 then foes += 1 end
            end
            if foes == 0 and allies > 0 then
                zn.progress = math.min(100, zn.progress + 12 * math.min(allies, 3))
            elseif allies == 0 then
                zn.progress = math.max(0, zn.progress - 6)
            end
            if zn.progress >= 100 then
                zn.owner = 1
                A.zone_cloth(zn)
                A.earn(50)
                A.say(`ZONA BONUS PRESA: +{A.ZONE_INCOME} AL RIFORNIMENTO`)
                core.log(`assault_game: bonus ground at {zn.x} {zn.z} taken`)
            end
        end
    end

    -- ------------------------------------------------ counterattacks
    -- Every COUNTER[1] to COUNTER[1] + COUNTER[2] seconds while a stronghold
    -- is fought for, one jeep of the legion (two now and then, or with more
    -- players) comes out of it full of fresh soldiers, drives at the allies'
    -- base, lets them off there and they fight.
    A.COUNTER = { 100, 40 }

    function A.step_counter()
        if A.counter_at == nil or clock < A.counter_at then return end
        A.counter_at = clock + A.COUNTER[1] + math.random() * A.COUNTER[2]
        local alive = 0
        for _, m in M.list do
            if m.counter and not m.dead then alive += 1 end
        end
        local n = if #core.players() > 1 or math.random() < 0.4 then 2 else 1
        n = math.min(n, 2 - alive)
        if n <= 0 then return end
        local f = FORTS[A.fort]
        local fort = A.fort
        for i = 1, n do
            local x = f[1] + 0.5 + (if i == 1 then -5 else 5)
            M.order("jeep", x, f[2] - 30.5, 0, 2, false, function(id)
                local m = M.list[id]
                if m == nil then return end
                m.counter = true
                local roles = { "rush", "cover", "rush", "watch" }
                for seat = 1, #m.k.seats do
                    local bid = A.spawn_bot(2, x + 2, f[2] - 30.5, roles[seat])
                    if bid then
                        local b = bots[bid]
                        b.fort, b.post = fort, { A.base[1] + math.random(-6, 6), A.base[2] + math.random(-4, 4) }
                        M.bot_board(bid, b, id, seat)
                    end
                end
                m.wait_until = clock + 1
            end)
        end
        A.say(if n > 1 then "CONTRATTACCO: DUE JEEP NEMICHE VERSO LA BASE!" else "CONTRATTACCO: UNA JEEP NEMICA VERSO LA BASE!")
        core.log(`assault_game: counterattack, {n} jeep(s)`)
    end

    -- ------------------------------------------------ the twisters
    -- Now and then, while a stronghold is fought for, a twister comes down
    -- out of the storm on the open ground between the base and it and
    -- wanders slowly across the valley for a minute or so. Whatever is within
    -- TW_PULL is drawn in and round; what reaches its middle is thrown high
    -- and hurt -- players, soldiers of both sides, machines. Its clients
    -- draw it from "torn" (x y z of each, every fifth of a second).
    A.TWISTERS = {}
    A.TW_EVERY = { 45, 40 }   -- seconds to the next: the first, and up to the second more
    A.TW_PULL, A.TW_CORE, A.TW_HURT = 14, 3.5, 7

    function A.step_twisters(dt: number)
        if A.stage ~= "fort" then
            if #A.TWISTERS > 0 then
                A.TWISTERS = {}
                tell(0, "torn")
            end
            A.tw_at = nil
            return
        end
        A.tw_at = A.tw_at or clock + 30
        if clock >= A.tw_at then
            A.tw_at = clock + A.TW_EVERY[1] + math.random() * A.TW_EVERY[2]
            local z0, z1 = A.base[2] + 30, FORTS[A.fort][2] - 20
            if #A.TWISTERS < 2 and z1 > z0 then
                table.insert(A.TWISTERS, { x = math.random(16, 112) + 0.5, z = math.random(z0, z1) + 0.5,
                                           a = math.random() * 2 * math.pi, speed = 1.5 + math.random(),
                                           gone = clock + 45 + math.random() * 30, hit = {} })
                A.say("UN TORNADO!")
            end
        end
        local tick = clock >= (A.tw_tick or 0)
        if tick then A.tw_tick = clock + 0.2 end
        local parts = {}
        for i = #A.TWISTERS, 1, -1 do
            local t = A.TWISTERS[i]
            if clock > t.gone then
                table.remove(A.TWISTERS, i)
                continue
            end
            -- Wandering: its heading drifts, and turns back off the ridge and
            -- off the strongholds' ends of the open ground.
            t.a += (math.random() - 0.5) * 1.2 * dt
            t.x += math.sin(t.a) * t.speed * dt
            t.z -= math.cos(t.a) * t.speed * dt
            if t.x < 10 or t.x > 118 then t.a = -t.a; t.x = math.clamp(t.x, 10, 118) end
            local z0, z1 = A.base[2] + 15, FORTS[A.fort][2] - 10
            if t.z < z0 or t.z > z1 then t.a = math.pi - t.a; t.z = math.clamp(t.z, z0, math.max(z0, z1)) end
            t.y = feet_at(math.floor(t.x), math.floor(t.z))
            table.insert(parts, string.format("%.1f %.1f %.1f", t.x, t.y, t.z))
            if not tick then continue end
            for _, b in enemies(0) do
                if b.thing then continue end
                local cx, cz = (b.lo[1] + b.hi[1]) / 2, (b.lo[3] + b.hi[3]) / 2
                local dx, dz = t.x - cx, t.z - cz
                local d = math.sqrt(dx * dx + dz * dz)
                if d >= A.TW_PULL then continue end
                local nx, nz = dx / math.max(d, 0.1), dz / math.max(d, 0.1)
                local vx, vy, vz
                -- Thrown: left to fly for a moment, or the pull would hold it down.
                local key = b.player or (if b.bot then -b.bot else 100000 + b.machine)
                if (t.hit[key] or 0) - clock > 0.5 then continue end
                if d < A.TW_CORE then
                    -- Its middle: up and out, once every couple of seconds.
                    if (t.hit[key] or 0) > clock then continue end
                    t.hit[key] = clock + 2
                    vx, vy, vz = -nz * 9 - nx * 5, 16 + math.random() * 6, nx * 9 - nz * 5
                    hurt(0, b, A.TW_HURT, "tornado", t.x, t.z)
                else
                    -- Drawn in and round, the harder the nearer.
                    local k = 1 - d / A.TW_PULL
                    vx, vy, vz = nx * (2 + 7 * k) - nz * (3 + 6 * k), 3 * k, nz * (2 + 7 * k) + nx * (3 + 6 * k)
                end
                if b.machine then
                    local m = M.list[b.machine]
                    local w = if m and (m.k.mass or 0) > 2000 then 0.25 else 0.6   -- a tank, hardly
                    core.craft.push(b.machine, vx * w * 0.3, vy * w * 0.3, vz * w * 0.3)
                elseif b.player then
                    if not M.seat_of[b.player] and core.health(b.player) > 0 then
                        -- No more than 16 m/s an axis (net::Push::kMax): it is refused.
                        core.player_launch(b.player, math.clamp(vx, -15.5, 15.5), math.min(vy, 15.5),
                                           math.clamp(vz, -15.5, 15.5), 2)
                    end
                elseif b.bot then
                    local bb = bots[b.bot]
                    if bb and not bb.ride and not bb.dead then core.mob_velocity(b.bot, vx, vy, vz) end
                end
            end
        end
        if tick and (#parts > 0 or A.tw_said) then
            tell(0, "torn " .. table.concat(parts, " "))
            A.tw_said = #parts > 0
        end
    end

    -- ------------------------------------------------ the builders
    -- Allies who do not fight: tougher, unarmed, a spanner in the hand. They
    -- go to the holes the fight made behind the front -- in the bases taken,
    -- on the road -- and put them back as the dimension made them
    -- (assault_core's "assault:repair"), a few seconds each.
    A.damage = {}   -- { x, y, z, r }: what explosions broke, merged when near

    function A.hurt_ground(x: number, y: number, z: number, r: number)
        if not (A.stage == "fort" or A.stage == "hall") then return end
        for _, d in A.damage do
            if (d.x - x) ^ 2 + (d.y - y) ^ 2 + (d.z - z) ^ 2 < 9 then
                d.r = math.min(5, math.max(d.r, r + 1))
                return
            end
        end
        if #A.damage < 300 then table.insert(A.damage, { x = x, y = y, z = z, r = r + 0.5 }) end
    end

    function A.spawn_builder()
        local x, y, z = A.barracks_spot()
        local id = core.spawn_mob("assault_game:builder", x, y, z)
        if id == 0 then return end
        bot_n[1] += 1
        bots[id] = { team = 1, n = bot_n[1], think_at = 0, shot_at = 0, nade_at = 1e9, throwing = 0, role = "build",
                     ammo = {}, gun = "rifle", burst = 0, builder = true, yaw = math.pi }
        core.mob_act(id, "", "assault_game:wrench")
    end

    -- Where builders may work: behind the stronghold fought for.
    function A.may_build(d): boolean
        if A.stage == "hall" then return d.z < 870 or (d.z > 875 and d.z < 940) end
        return d.z < FORTS[A.fort][2] - 45
    end

    -- Inside the barracks by the base (assault_core's, west of it), where the
    -- allies come out.
    function A.barracks_spot(): (number, number, number)
        local b = A.base
        return b[1] - 21 + math.random(-1, 1) + 0.5, GROUND, b[2] - 1 + math.random(-1, 1) + 0.5
    end

    -- What the builders put up for stronghold k, worked out the first time
    -- one asks (its walls in memory by then, A.keep): a ladder up the south
    -- face of what stands either side of the middle -- a wall, a tower, a
    -- bunker -- 3 to 16 high with room on top; and four nests of sandbags out
    -- in front, in the open short of it.
    A.LADDER, A.SANDBAG = core.node_id("assault_core:ladder"), core.node_id("assault_core:sandbag")
    A.plans = {}
    function A.plan(k: number)
        if A.plans[k] then return A.plans[k] end
        local f, ar = FORTS[k], A.AREA[k]
        if core.loaded(f[1], GROUND, ar[3] - 12) ~= 1 or core.loaded(f[1], GROUND, f[2]) ~= 1 then return {} end
        local function solid(x: number, y: number, z: number): boolean return core.get_block(x, y, z) ~= 0 end
        local list = {}
        for _, dx in { -12, 12, -22, 22 } do
            local x = f[1] + dx
            for z = ar[3] - 12, f[2] do
                if solid(x, GROUND, z) and solid(x, GROUND + 1, z) then
                    local top = GROUND + 1
                    while top < GROUND + 16 and solid(x, top + 1, z) do top += 1 end
                    if top >= GROUND + 2 and top < GROUND + 16 and not solid(x, top + 2, z) then
                        table.insert(list, { kind = "ladder", fort = k, x = x, z = z - 1, top = top, sx = x, sz = z - 2 })
                    end
                    break
                end
            end
        end
        for _, dx in { -14, 14, -28, 28 } do
            local z = ar[3] - 25
            table.insert(list, { kind = "nest", fort = k, x = f[1] + dx, z = z, sx = f[1] + dx, sz = z - 2 })
        end
        A.plans[k] = list
        return list
    end

    -- A job nobody alive is on, not given up on for now, not done.
    function A.free(job, id: number): boolean
        local t = job.taken and bots[job.taken]
        return not job.done and clock >= (job.skip or 0)
               and (job.taken == id or t == nil or t.dead or t.job ~= job)
    end

    function A.put_up(job)
        if job.kind == "ladder" then
            for y = A.feet_at(job.x, job.z), job.top do
                if core.get_block(job.x, y, job.z) == 0 then
                    core.set_block(job.x, y, job.z, A.LADDER)
                    core.set_facing(job.x, y, job.z, 0)
                end
            end
            A.say("UNA SCALA SULLE MURA")
        else
            -- A low wall across, its ends bent back: the open side ours.
            for _, c in { { -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 }, { -2, -1 }, { 2, -1 } } do
                local cx, cz = job.x + c[1], job.z + c[2]
                local fy = A.feet_at(cx, cz)
                for y = fy, fy + 1 do
                    if core.get_block(cx, y, cz) == 0 then core.set_block(cx, y, cz, A.SANDBAG) end
                end
            end
        end
        core.log(`assault_game: a builder put up a {job.kind} at {job.x} {job.z}`)
    end

    function A.build_think(id: number, b, x: number, y: number, z: number)
        -- Not a node further in three seconds on the way: a step aside, as
        -- a soldier takes (think).
        if core.mob_going(id) == 1 and b.last and (x - b.last[1]) ^ 2 + (z - b.last[2]) ^ 2 < 1 then
            if clock - b.last[3] > 3 then
                local a = math.random() * 2 * math.pi
                local sx, sz = math.floor(x + math.cos(a) * 6), math.floor(z + math.sin(a) * 6)
                core.mob_goto(id, sx, A.feet_at(sx, sz), sz)
                b.aside_until, b.last = clock + 3, { x, z, clock }
                return
            end
        else
            b.last = { x, z, clock }
        end
        if b.aside_until and clock < b.aside_until then return end

        local job = b.job
        if job and (job.done or (if job.kind then A.stage ~= "fort" or A.fort ~= job.fort else not A.may_build(job))) then
            job, b.job = nil, nil
        end
        if job == nil then
            -- Half of them see to what is broken first, half to what is to be put up.
            local plan = if A.stage == "fort" then A.plan(A.fort) else {}
            local best, at = nil, 1e9
            for pass = 1, 2 do
                local builds = (pass == 1) == (b.n % 2 == 0)
                for _, d in (if builds then plan else A.damage) do
                    if A.free(d, id) and (builds or A.may_build(d)) then
                        local dd = (d.x - x) ^ 2 + (d.z - z) ^ 2
                        if dd < at then best, at = d, dd end
                    end
                end
                if best then break end
            end
            if best == nil then
                -- Nothing to do: by the base.
                local bx, bz = A.base[1] + 0.5 + (b.n % 5 - 2) * 2, A.base[2] - 3.5
                if (x - bx) ^ 2 + (z - bz) ^ 2 > 16 and core.mob_going(id) == 0 then
                    core.mob_goto(id, math.floor(bx), A.feet_at(math.floor(bx), math.floor(bz)), math.floor(bz))
                end
                core.mob_act(id, "", "assault_game:wrench")
                return
            end
            best.taken = id
            b.job, job = best, best
            b.work_until, b.trying = nil, nil
        end
        -- Where it works from: in front of what it puts up; the edge of a
        -- hole, not its bottom.
        local far
        local tx, tz = job.sx, job.sz
        if job.kind then
            far = (tx + 0.5 - x) ^ 2 + (tz + 0.5 - z) ^ 2 > 2.25
        else
            local dx, dz = x - job.x, z - job.z
            local l = math.max(0.1, math.sqrt(dx * dx + dz * dz))
            tx, tz = math.floor(job.x + dx / l * (job.r + 1.5)), math.floor(job.z + dz / l * (job.r + 1.5))
            far = l > job.r + 2.5
        end
        if far then
            if core.mob_going(id) == 0 then core.mob_goto(id, tx, A.feet_at(tx, tz), tz) end
            core.mob_act(id, "", "assault_game:wrench")
            -- Not getting there: left for a while, to it or another.
            b.trying = b.trying or clock
            if clock - b.trying > 25 then
                job.skip, job.taken, b.job, b.trying = clock + 45, nil, nil, nil
            end
            return
        end
        b.trying = nil
        core.mob_goto(id, math.floor(x), math.floor(y), math.floor(z))
        core.mob_face(id, job.x, job.z)
        core.mob_act(id, "build", "assault_game:wrench")
        b.work_until = b.work_until or clock + (if job.kind == "ladder" then 3 + (job.top - GROUND) * 0.4
                                                elseif job.kind then 5 else 2 + job.r)
        if clock >= b.work_until then
            job.done = true
            if job.kind then
                A.put_up(job)
            else
                core.event.emit("assault:repair", { x = job.x, y = job.y, z = job.z, r = job.r + 0.5 })
                core.log(string.format("assault_game: a builder mended %.0f %.0f %.0f", job.x, job.y, job.z))
                for i, d in A.damage do
                    if d == job then table.remove(A.damage, i); break end
                end
            end
            b.job, b.work_until = nil, nil
            core.mob_act(id, "", "assault_game:wrench")
        end
    end

    -- The fortress's gate (assault_core's GATE, three thick across the
    -- middle of its south wall, z 870..872): a cell of it still standing
    -- in its middle two high, the nearest layer first, or nothing once it
    -- is open -- its z, x, y.
    A.GATE = core.node_id("assault_core:gate")
    function A.gate_layer(): (number?, number, number)
        for z = 870, 872 do
            for y = GROUND, GROUND + 1 do
                for xx = MID - 1, MID + 1 do
                    if core.get_block(xx, y, z) == A.GATE then return z, xx, y end
                end
            end
        end
        return nil, 0, 0
    end

    -- The allies on foot before the shut gate, within 30 nodes: one of them
    -- at a time goes up to it and, from 7 nodes or less, a grenade goes off
    -- in it every few seconds. One that has not managed in 15 s (a crater, a wall in
    -- the way) leaves it to another for a while.
    function A.blow_gate(id: number, b, x: number, y: number, z: number): boolean
        if b.team ~= 1 or A.stage ~= "fort" or A.fort ~= 4 or b.throwing > 0 then return false end
        if clock < (b.sap_skip or 0) then return false end
        local s = A.sapper and bots[A.sapper]
        if s and not s.dead and A.sapper ~= id then return false end
        if z > 869.5 or (MID + 0.5 - x) ^ 2 + (869 - z) ^ 2 > 30 * 30 then
            if A.sapper == id then A.sapper = nil end
            return false
        end
        local lz, cx, cy = A.gate_layer()
        if lz == nil then
            A.sapper = nil
            return false
        end
        if A.sapper ~= id then A.sapper, A.sapper_since = id, clock end
        if clock - A.sapper_since > 15 then
            A.sapper, b.sap_skip = nil, clock + 20
            return false
        end
        if clock < (A.gate_at or 0) then return false end
        local gx, gz = cx + 0.5, lz + 0.5
        local dx, dz = gx - x, gz - z
        local d = math.sqrt(dx * dx + dz * dz)
        if d > 7 then
            local tx, tz = MID, lz - 4
            if core.mob_going(id) == 0 then core.mob_goto(id, tx, feet_at(tx, tz), tz) end
            M.bot_clip(id, b, "")
            return true
        end
        -- Put in what is left of it rather than thrown: ponytail, a toss
        -- came off the gate, one laid at its foot fell into the crater the
        -- last one made, and both burst where they did nothing.
        A.gate_at, A.sapper_since = clock + 3.5, clock
        spawn_nade(gx, cy + 0.5, gz, 0, 0, 0, -id, 1)
        core.mob_goto(id, math.floor(x), math.floor(y), math.floor(z))
        M.face(id, b, gx, gz)
        M.bot_clip(id, b, "throw")
        return true
    end

    A.PYLONS = { { 38, 956 }, { 90, 956 }, { 38, 997 }, { 90, 997 } }
    A.CORE = { MID + 0.5, GROUND + 5, 976.5 }
    A.THRONE = { MID + 0.5, GROUND + 8, 1006.5 }
    A.SHIELD_OPEN = 18      -- seconds the core is bare once every pylon is down
    -- The escape tunnel (assault_core's TUBE, kept in step): each corner x, z
    -- and the height a body stands at there; how far along it the rock is.
    A.PATH = { { 64, 976, 2 }, { 64, 1044, 2 }, { 104, 1044, 4 }, { 104, 1068, 4 }, { 172, 1068, 0 },
               { 172, 960, 8 }, { 156, 960, 8 }, { 156, 850, -2 }, { 188, 850, -2 }, { 188, 720, 10 },
               { 156, 720, 10 }, { 156, 600, -2 }, { 188, 600, -2 }, { 188, 470, 10 }, { 160, 470, 10 },
               { 160, 340, -2 }, { 184, 340, -2 }, { 184, 200, 10 }, { 172, 200, 10 }, { 172, 20, 20 } }
    A.S_END = -4
    for i = 1, #A.PATH - 1 do
        A.S_END += math.abs(A.PATH[i + 1][1] - A.PATH[i][1]) + math.abs(A.PATH[i + 1][2] - A.PATH[i][2])
    end
    A.START = { MID + 0.5, 2, 1017.5 }   -- where they come back in the escape: the tunnel's foot
    A.GRACE = 14            -- seconds from the floor falling in to the giant's first step
    A.MODEL = {}
    for _, n in { "basoalth", "basoalth_giant", "core", "pylon", "pylon_dead", "shield", "orb" } do
        A.MODEL[n] = core.register_model(`assault_game:{n}`, `{n}.gltf`)
    end
    A.THING_N = 0

    -- What the shop sells (B at the base): once for all, or as often as the
    -- purse allows. A gun bought is in every ally's hands.
    -- An upgrade (`grow`) can be bought up to `max` times, each dearer than
    -- the last by that factor (A.cost).
    A.SHOP = {
        { id = "troops", name = "5 soldati", cost = 150 },
        { id = "builder", name = "1 costruttore", cost = 120 },
        { id = "lmg", name = "Mitragliatrice, a tutti", cost = 120, once = true, gun = "lmg" },
        { id = "sniper", name = "Precisione, a tutti", cost = 120, once = true, gun = "sniper" },
        { id = "launcher", name = "Lanciarazzi, a tutti", cost = 200, once = true, gun = "launcher" },
        { id = "arms", name = "Armi +15% danno", cost = 150, grow = 1.6, max = 8 },
        { id = "hulls", name = "Mezzi +25% resistenza", cost = 180, grow = 1.6, max = 6 },
        { id = "blast", name = "Antiesplosione -15%", cost = 140, grow = 1.7, max = 4 },
        { id = "jeep", name = "Jeep", cost = 100, machine = "jeep" },
        { id = "tank", name = "Carro armato", cost = 350, machine = "tank" },
        { id = "heli", name = "Elicottero", cost = 500, machine = "heli" },
    }
    A.MAX_ALLIES = 100
    A.MAX_BUILDERS = 6
    A.level = {}            -- each upgrade's level
    A.hull, A.blast, A.builders = 1, 1, 2

    function A.cost(item): number
        return math.floor(item.cost * (item.grow or 1) ^ (A.level[item.id] or 0) + 0.5)
    end

    function A.say(text: string)
        tell(0, `msg {text}`)
    end

    -- A thing a shot may hit (A.things): an entity of `model` at x, y, z.
    function A.thing(model: string, x: number, y: number, z: number, hw: number, h: number, hit): any
        local id = core.ecs.spawn(M.THING, x, y, z)
        if id == nil then return nil end
        core.ecs.visible(id, true)
        core.ecs.model(id, A.MODEL[model])
        A.THING_N += 1
        local t = { id = id, x = x, y = y, z = z, hw = hw, h = h, n = A.THING_N, hit = hit }
        return t
    end

    function A.drop_thing(t)
        if t == nil then return end
        for i, o in A.things do
            if o == t then
                table.remove(A.things, i)
                break
            end
        end
        if t.id then core.ecs.kill(t.id) end
        t.id = nil
    end

    function A.flag(k: number, cloth: number)
        local p = A.POLE[k]
        for y = p[3], p[3] + 2 do
            for x = p[1] + 1, p[1] + 2 do core.set_block(x, y, p[2], cloth) end
        end
    end

    -- Every ally on the field the bought guns, those they do not have yet.
    function A.kit(p: number)
        local guns = { "rifle", "pistol", "grenade" }
        for _, item in A.SHOP do
            if item.gun and A.bought[item.id] then table.insert(guns, item.gun) end
        end
        for _, g in guns do
            if core.bag_count(p, `assault_game:{g}`) == 0 then core.give_item(p, `assault_game:{g}`, 1) end
        end
    end

    -- ------------------------------------------------ the strongholds

    -- A garrison to put out, as the ground under it is in memory.
    A.pending = {}
    function A.step_pending()
        for _ = 1, 2 do
            local q = A.pending[1]
            if q == nil then return end
            if core.loaded(math.floor(q[1]), GROUND, math.floor(q[2])) == 1 then
                table.remove(A.pending, 1)
                local id = A.spawn_bot(2, q[1], q[2], q[3])
                if id then bots[id].fort = q[4] end
            else
                core.keep_loaded(math.floor(q[1]), math.floor(q[2]), 16)
                return
            end
        end
    end

    -- Somebody of the legion at every seat of a machine that can be manned.
    function A.crew(id: number, fort: number)
        local m = M.list[id]
        if m == nil then return end
        local x, _, z = M.where(id)
        for i, seat in m.k.seats do
            if seat.gun or i == 1 then
                local bid = A.spawn_bot(2, x + 1.5, z + 1.5, "watch")
                if bid then
                    bots[bid].fort = fort
                    M.bot_board(bid, bots[bid], id, i)
                end
            end
        end
    end

    function A.man(k: number)
        A.fort, A.progress = k, 0
        local n = A.GARRISON[k] + 2 * math.max(0, #core.players() - 1)
        local a = A.AREA[k]
        local roles = { "watch", "cover", "rush", "cover", "at", "watch" }
        for i = 1, n do
            table.insert(A.pending, { math.random(a[1], a[2]) + 0.5, math.random(a[3], a[4]) + 0.5, roles[i % #roles + 1], k })
        end
        for _, g in M.FORT[k] do
            M.order(g[1], g[2], g[3], 0, 2, false, function(id) A.crew(id, k) end)
        end
        -- Three on each of its bonus grounds.
        for _, zn in A.ZONES do
            if zn.fort == k then
                for i = 1, 3 do
                    table.insert(A.pending, { zn.x + math.random(-4, 4) + 0.5, zn.z + math.random(-4, 4) + 0.5,
                                              if i == 1 then "watch" else "cover", k })
                end
            end
        end
        A.counter_at = clock + A.COUNTER[1] + math.random() * A.COUNTER[2]
        A.say(`OBIETTIVO: {FORT_NAME[k]}`)
        if k == 4 then A.say("IL CANCELLO DELLA FORTEZZA VA FATTO SALTARE: GRANATE, RAZZI O UN CARRO") end
        core.log(`assault_game: stronghold {k} manned, {n} on foot`)
    end

    -- The legion left of stronghold k (or all of it), on foot or at a gun.
    function A.garrison_left(k: number?): number
        local n = 0
        for _, b in bots do
            if b.team == 2 and not b.dead and (k == nil or b.fort == k) then n += 1 end
        end
        return n
    end

    function A.rout(k: number?)
        for id, b in bots do
            if b.team == 2 and (k == nil or b.fort == k) then
                if b.ride then M.bot_leave(id, b) end
                core.mob_remove(id)
                bots[id] = nil
            end
        end
        A.pending = {}
    end

    -- The allies on foot left far behind (more than 100 back from the base)
    -- come up to it: they would walk half the valley to catch up.
    function A.regroup()
        for id, b in bots do
            if b.team ~= 1 or b.dead or b.ride or b.crew then continue end
            local x, _, z = core.mob_pos(id)
            if x ~= nil and A.base[2] - z > 100 then
                local bx, by, bz = base_spot(1)
                core.mob_place(id, bx, by, bz)
                b.last, b.phase = nil, nil
            end
        end
    end

    -- Taken: the flag in blue, what is left of its garrison runs, the base
    -- comes here, and the next one is manned.
    function A.take(k: number)
        A.flag(k, A.ALLY_CLOTH)
        A.rout(k)
        A.earn(150)
        A.base = A.BASE[k]
        core.event.emit("assault:barracks", { k = k + 1 })
        for p in players do A.spawn_at(p, base_spot(1)) end
        -- The allies' machines go to the new base's park, each to a place of
        -- its own, straight away unless a player is in one (that one comes
        -- back there when it is lost or left stuck).
        local n = 0
        for id, m in M.list do
            if m.home[4] == 1 and not m.escape then
                n += 1
                m.home = A.park(n)
                if next(m.riders) == nil then A.repark(id, m, n) end
            end
        end
        for _, r in M.respawn do
            if r.home[4] == 1 then
                n += 1
                r.home = A.park(n)
            end
        end
        A.reinforce_at = 0   -- and the fallen straight away
        A.regroup()
        A.say(`{FORT_NAME[k]} E' NOSTRO`)
        core.log(`assault_game: stronghold {k} taken`)
        if k < 4 then A.man(k + 1) else A.open_hall() end
    end

    -- At the flag: allies near and none of the legion, and it goes on, faster
    -- with more of them (up to three); the legion near, and it goes back.
    function A.step_fort()
        if clock < (A.cap_at or 0) then return end
        A.cap_at = clock + 1
        local f = FORTS[A.fort]
        local allies, foes = 0, 0
        local function near(x: number, z: number, r: number): boolean
            return (x - f[1]) ^ 2 + (z - f[2]) ^ 2 <= r * r
        end
        for p in players do
            local x, _, z = core.player_pos(p)
            if x ~= nil and core.health(p) > 0 and near(x, z, CAPTURE_R) then allies += 1 end
        end
        for id, b in bots do
            if b.dead then continue end
            local x, _, z = core.mob_pos(id)
            if x == nil then continue end
            if b.team == 1 and near(x, z, CAPTURE_R) then allies += 1 end
            if b.team == 2 and near(x, z, CAPTURE_R * 2) then foes += 1 end
        end
        if foes == 0 and allies > 0 then
            A.progress = math.min(100, A.progress + 10 * math.min(allies, 3))
        elseif foes > 0 then
            A.progress = math.max(0, A.progress - 10)
        end
        if A.progress >= 100 then A.take(A.fort) end
    end

    -- ------------------------------------------------ the hall

    function A.pylon_hp(): number
        return 260 * (1 + 0.5 * math.max(0, #core.players() - 1))
    end

    function A.light_pylons()
        A.pylons = {}
        for i, c in A.PYLONS do
            local pl
            pl = A.thing("pylon", c[1] + 0.5, GROUND + 1, c[2] + 0.5, 1.2, 5, function(dmg: number, _by: number, _w: string): boolean
                if pl.hp <= 0 then return false end
                pl.hp -= dmg
                if pl.hp > 0 then return false end
                core.ecs.model(pl.id, A.MODEL.pylon_dead)
                for j, o in A.things do
                    if o == pl then table.remove(A.things, j); break end
                end
                tell(0, string.format("boom %.2f %.2f %.2f 2.5", pl.x, pl.y + 3, pl.z))
                core.play_sound("assault_game:boom", pl.x, pl.y + 3, pl.z)
                A.earn(100)
                return true
            end)
            if pl then
                pl.hp, pl.max = A.pylon_hp() * (1 + 0.2 * (A.cycle - 1)), A.pylon_hp() * (1 + 0.2 * (A.cycle - 1))
                A.pylons[i] = pl
                table.insert(A.things, pl)
            end
        end
        if A.shield and A.shield.id then core.ecs.visible(A.shield.id, true) end
    end

    function A.pylons_up(): number
        local n = 0
        for _, pl in A.pylons or {} do
            if pl.hp > 0 then n += 1 end
        end
        return n
    end

    -- The courtyard taken: the hall's door opens, its pylons, its core and
    -- Basoalth on his throne.
    function A.open_hall()
        A.stage, A.cycle = "hall", 1
        for x = MID - 4, MID + 4 do
            for y = GROUND, GROUND + 7 do
                for z = 941, 942 do core.set_block(x, y, z, 0) end
            end
        end
        A.light_pylons()
        A.core_hp = 1200 * (1 + 0.5 * math.max(0, #core.players() - 1))
        A.core_max = A.core_hp
        A.core = A.thing("core", A.CORE[1], A.CORE[2], A.CORE[3], 2.2, 4.4, function(dmg: number, _by: number, _w: string): boolean
            if A.stage ~= "hall" or not A.bare then return false end
            A.core_hp -= dmg
            if A.core_hp <= 0 then
                A.rise()
                return true
            end
            return false
        end)
        A.shield = A.thing("shield", A.CORE[1], A.CORE[2] - 1, A.CORE[3], 0, 0, nil)
        A.lord = A.thing("basoalth", A.THRONE[1], A.THRONE[2], A.THRONE[3], 0, 0, nil)
        if A.lord then core.ecs.set(A.lord.id, "yaw", 0) end
        for _, g in { { "nest", 44.5, 1004.5 }, { "nest", 84.5, 1004.5 } } do
            M.order(g[1], g[2], g[3], 0, 2, false, function(id) A.crew(id, 5) end)
        end
        A.wave_at, A.orb_at = clock + 4, clock + 6
        A.say("LA SALA DEL NUCLEO: ABBATTETE I QUATTRO PILONI")
        core.log("assault_game: the hall")
    end

    -- Its guard, out of the doors in the side walls, a few at a time.
    function A.wave()
        local alive = A.garrison_left(5)
        local n = math.min(4 + #core.players(), 14 - alive)
        for i = 1, n do
            local west = i % 2 == 0
            local x = if west then 22.5 else 105.5
            local z = 976.5 + math.random(-12, 12)
            local id = A.spawn_bot(2, x, z)
            if id then
                bots[id].fort = 5
                bots[id].post = { math.random(32, 96) + 0.5, math.random(950, 1000) + 0.5 }
            end
        end
    end

    function A.step_hall()
        if clock >= A.wave_at then
            A.wave_at = clock + 18
            A.wave()
        end
        -- Basoalth throws an orb at somebody in the hall, more often the
        -- fewer pylons stand.
        if clock >= A.orb_at then
            A.orb_at = clock + 2.5 + A.pylons_up() * 0.6
            local them = {}
            for p in players do
                local x, _, z = core.player_pos(p)
                if x ~= nil and core.health(p) > 0 and z > 943 and z < 1009 then table.insert(them, p) end
            end
            if #them > 0 then
                A.orb(A.THRONE[1], A.THRONE[2] + 2.2, A.THRONE[3] - 1, them[math.random(1, #them)], nil)
            end
        end
        if not A.bare and A.pylons_up() == 0 then
            A.bare, A.bare_until = true, clock + A.SHIELD_OPEN
            if A.shield and A.shield.id then core.ecs.visible(A.shield.id, false) end
            table.insert(A.things, A.core)
            A.say("IL NUCLEO E' SCOPERTO: SPARATE!")
        elseif A.bare and clock >= A.bare_until then
            A.bare = false
            for j, o in A.things do
                if o == A.core then table.remove(A.things, j); break end
            end
            for _, pl in A.pylons do A.drop_thing(pl) end
            A.cycle += 1
            A.light_pylons()
            A.say("I PILONI SI RIACCENDONO")
        end
    end

    -- ------------------------------------------------ the orbs

    -- One of Basoalth's orbs from x, y, z after a player (or a machine): it
    -- turns after them, bursts on what it meets, and one round puts it out.
    A.orbs = {}
    function A.orb(x: number, y: number, z: number, player: number?, machine: number?)
        local o
        o = A.thing("orb", x, y, z, 0.6, 1.2, function(_dmg: number, _by: number, _w: string): boolean
            if o.gone then return false end
            o.gone = true
            tell(0, string.format("boom %.2f %.2f %.2f 0.8", o.x, o.y + 0.6, o.z))
            A.drop_thing(o)
            return true
        end)
        if o == nil then return end
        o.player, o.machine, o.t, o.v = player, machine, 0, { 0, 6, 0 }
        table.insert(A.things, o)
        A.orbs[o] = true
        core.play_sound("assault_game:rocket", x, y, z)
    end

    function A.step_orbs(dt: number)
        for o in A.orbs do
            if o.gone then
                A.orbs[o] = nil
                continue
            end
            o.t += dt
            local tx, ty, tz
            if o.machine and M.list[o.machine] and not M.list[o.machine].dead then
                tx, ty, tz = M.where(o.machine)
                ty += 1
            elseif o.player then
                tx, ty, tz = core.player_pos(o.player)
                if tx then ty += 1 end
            end
            local v = o.v
            if tx ~= nil then
                local dx, dy, dz = tx - o.x, ty - o.y, tz - o.z
                local l = math.max(0.01, math.sqrt(dx * dx + dy * dy + dz * dz))
                if l < 1.8 then o.t = 99 end
                local speed = if o.machine then 26 else 15
                local k = math.min(1, 1.6 * dt)
                v[1] += (dx / l * speed - v[1]) * k
                v[2] += (dy / l * speed - v[2]) * k
                v[3] += (dz / l * speed - v[3]) * k
            end
            local sp = math.sqrt(v[1] ^ 2 + v[2] ^ 2 + v[3] ^ 2)
            local _, _, _, wall = core.raycast(o.x, o.y, o.z, v[1], v[2], v[3], sp * dt + 0.3)
            if wall ~= nil or o.t > 7 then
                o.gone = true
                A.orbs[o] = nil
                A.drop_thing(o)
                explode(o.x, o.y, o.z, 1.4, 3.5, 12, 0, 2, "orb")
                continue
            end
            o.x += v[1] * dt
            o.y += v[2] * dt
            o.z += v[3] * dt
            core.ecs.set(o.id, "x", o.x)
            core.ecs.set(o.id, "y", o.y)
            core.ecs.set(o.id, "z", o.z)
        end
    end

    -- ------------------------------------------------ the rising

    -- Every machine put away at once, nobody's left in one.
    function A.clear_machines()
        for id, m in M.list do
            for _, p in m.riders do M.leave(p) end
            for _, bid in m.bots do
                if bots[bid] then M.bot_leave(bid, bots[bid]) end
            end
            for _, pid in m.parts do core.ecs.kill(pid) end
            core.ecs.kill(id)
            M.list[id] = nil
        end
        M.respawn = {}
    end

    function A.rise()
        A.stage, A.rise_t = "rise", 0
        A.bare = false
        A.rout(nil)
        -- What the allies drove here stays behind: the escape is the tunnel's jeeps.
        A.clear_machines()
        for _, pl in A.pylons do A.drop_thing(pl) end
        A.drop_thing(A.shield)
        local c = A.CORE
        A.drop_thing(A.core)
        tell(0, string.format("boom %.2f %.2f %.2f 4", c[1], c[2] + 2, c[3]))
        core.play_sound("assault_game:boom", c[1], c[2], c[3])
        for o in A.orbs do o.gone = true; A.drop_thing(o) end
        A.say("IL NUCLEO SI SPEZZA...")
        core.log("assault_game: the core is broken")
    end

    function A.step_rise(dt: number)
        local t0 = A.rise_t
        A.rise_t += dt
        local t = A.rise_t
        local function past(s: number): boolean return t0 < s and t >= s end
        local lord = A.lord
        if t < 4 and lord and lord.id then
            -- Down from the throne to where the core was.
            local k = math.min(1, t / 3.5)
            local x = A.THRONE[1] + (A.CORE[1] - A.THRONE[1]) * k
            local y = A.THRONE[2] + (A.CORE[2] - 1 - A.THRONE[2]) * k
            local z = A.THRONE[3] + (A.CORE[3] - A.THRONE[3]) * k
            core.ecs.set(lord.id, "x", x)
            core.ecs.set(lord.id, "y", y)
            core.ecs.set(lord.id, "z", z)
        end
        if past(0.2) and lord and lord.id then core.ecs.clip(lord.id, "absorb") end
        if past(3) then A.say("BASOALTH ASSORBE L'ENERGIA DEL NUCLEO") end
        if past(5) then
            A.drop_thing(lord)
            A.lord = nil
            -- Out through the roof.
            for x = MID - 18, MID + 18 do
                for z = 960, 992 do
                    for y = GROUND + 24, GROUND + 25 do core.set_block(x, y, z, 0) end
                end
            end
            A.giant = A.thing("basoalth_giant", A.CORE[1], GROUND, A.CORE[3], 0, 0, nil)
            if A.giant then
                core.ecs.set(A.giant.id, "yaw", 0)
                core.ecs.clip(A.giant.id, "rise")
            end
            for _, d in { { 0, 30, 0 }, { -12, 26, 6 }, { 12, 26, -6 } } do
                tell(0, string.format("boom %.2f %.2f %.2f 4", A.CORE[1] + d[1], GROUND + d[2], A.CORE[3] + d[3]))
            end
            A.say("BASOALTH SI ERGE!")
        end
        if past(7.5) then
            -- The floor falls in under all of them, onto the heaps at the
            -- tunnel's foot: whole before and after, so the fall costs nothing.
            for p in players do core.heal(p, 20) end
            A.deep = true
            core.event.emit("assault:collapse", { on = true })
            for _, d in { { -20, -20 }, { 20, -20 }, { -20, 20 }, { 20, 20 }, { 0, 34 } } do
                tell(0, string.format("boom %.2f %.2f %.2f 4", A.CORE[1] + d[1], GROUND, A.CORE[3] + d[2]))
            end
            core.play_sound("assault_game:boom", A.CORE[1], GROUND, A.CORE[3])
            if A.giant and A.giant.id then core.ecs.set(A.giant.id, "y", A.PATH[1][3]) end
            A.jeeps()
            A.say("IL PAVIMENTO CROLLA! ALLE JEEP, NEL TUNNEL A NORD!")
        end
        if past(9.5) then
            for p in players do core.heal(p, 20) end
        end
        if t >= 10 then A.start_escape() end
    end

    -- ------------------------------------------------ the tunnel's traps
    -- Along the tunnel, things to shoot that hold Basoalth back where they
    -- are, one every 110 or so, built again for every try:
    --   valve  a valve of the magma's pipes on a wall: shot open, it throws
    --          lava across the tunnel for 25 s; he walks through it burnt,
    --          at under half his pace for 5 s
    --   rocks  a cluster hanging from the roof: shot down, it lies across
    --          the road; he trips on it and reels 4 s
    --   tank   a tank of the dimension's gas in a channel: shot, it blows at
    --          once -- worth it with him within 40, a heavy blow and a reel
    A.TRAP_KINDS = { "valve", "rocks", "tank" }
    A.TRAP_HP = { valve = 40, rocks = 60, tank = 50 }
    A.traps = {}
    A.TB = {}
    for _, n in { "wall", "hazard", "basalt", "concrete", "ember", "foe" } do A.TB[n] = core.node_id(`assault_core:{n}`) end

    function A.clear_traps()
        for _, t in A.traps do
            A.drop_thing(t.thing)
            for _, c in t.cells do core.set_block(c[1], c[2], c[3], 0) end
        end
        A.traps = {}
    end

    function A.build_traps()
        A.clear_traps()
        local n = 0
        for s = 150, A.S_END - 80, 110 do
            n += 1
            local kind = A.TRAP_KINDS[(n - 1) % 3 + 1]
            local side = if n % 2 == 0 then 1 else -1
            local x, z, dx, dz, fy = A.at(s)
            local lx, lz = -dz * side, dx * side     -- across, to that side
            local t = { s = s, kind = kind, cells = {}, used = false, open_at = nil }
            local function cell(off: number, up: number, along: number, block: number)
                local cx = math.floor(x + lx * off + dx * along)
                local cz = math.floor(z + lz * off + dz * along)
                core.set_block(cx, fy + up, cz, block)
                table.insert(t.cells, { cx, fy + up, cz })
            end
            local hx, hy, hz, hw, hh
            if kind == "valve" then
                -- A pipe up the wall out of the magma, the wheel on it in the
                -- hazard's colours.
                for up = -2, 12 do cell(12, up, 0, A.TB.wall) end
                for _, a in { -1, 1 } do cell(11, 6, a, A.TB.hazard); cell(11, 6 + a, 0, A.TB.hazard) end
                cell(11, 6, 0, A.TB.foe)
                hx, hy, hz, hw, hh = x + lx * 11.5, fy + 5, z + lz * 11.5, 1.2, 3
            elseif kind == "rocks" then
                for a = -2, 2 do
                    for o = -2, 2 do
                        local deep = 3 - math.max(math.abs(a), math.abs(o))
                        for up = 43 - deep, 43 do cell(o, up, a, A.TB.basalt) end
                    end
                end
                cell(0, 39, 0, A.TB.ember)
                hx, hy, hz, hw, hh = x, fy + 38, z, 2.6, 6
            else
                for a = -1, 1 do
                    for o = 8, 10 do
                        cell(o, -2, a, A.TB.concrete)
                        for up = -1, 2 do cell(o, up, a, if up % 2 == 0 then A.TB.hazard else A.TB.foe) end
                    end
                end
                hx, hy, hz, hw, hh = x + lx * 9, fy - 1, z + lz * 9, 1.8, 4
            end
            local hp = A.TRAP_HP[kind]
            t.thing = A.thing("orb", hx, hy, hz, hw, hh, function(dmg: number, _by: number, _w: string): boolean
                if t.open_at then return false end
                hp -= dmg
                if hp > 0 then return false end
                A.spring(t)
                return true
            end)
            if t.thing then
                core.ecs.visible(t.thing.id, false)
                t.thing.big = true
                t.thing.trap = t
                table.insert(A.things, t.thing)
            end
            t.at = { x, fy, z, lx, lz, dx, dz }
            table.insert(A.traps, t)
        end
    end

    -- Whether the bots should shoot it now: the rocks any time (they lie
    -- there till he comes), the valve once he is close enough to reach its
    -- 25 s of lava even slowed (about 7 m/s), the tank with him near enough
    -- that its 50 hp, at about 4 a hit, give out inside its 40.
    function A.trap_ripe(t): boolean
        if A.stage ~= "escape" or clock < (A.go_at or math.huge) then return false end
        local behind = t.s - (A.f - 10)
        if t.kind == "valve" then return behind < 150 end
        if t.kind == "tank" then return behind < 55 end
        return true
    end

    -- Shot: what each does at once.
    function A.spring(t)
        t.open_at = clock
        A.drop_thing(t.thing)
        core.log(string.format("assault_game: %s at %d shot, Basoalth %.0f from it", t.kind, t.s, t.s - (A.f - 10)))
        local x, fy, z, lx, lz = t.at[1], t.at[2], t.at[3], t.at[4], t.at[5]
        if t.kind == "valve" then
            for _, c in t.cells do
                if c[2] == fy + 6 or c[2] == fy + 5 or c[2] == fy + 7 then
                    if math.abs(c[1] - math.floor(x + lx * 12)) + math.abs(c[3] - math.floor(z + lz * 12)) > 0 then
                        core.set_block(c[1], c[2], c[3], 0)
                    end
                end
            end
            tell(0, string.format("jet %.1f %.1f %.1f %.2f %.2f 25", x + lx * 11, fy + 6, z + lz * 11, -lx, -lz))
            A.say("VALVOLA APERTA: LAVA SUL SUO CAMMINO!")
        elseif t.kind == "rocks" then
            for _, c in t.cells do core.set_block(c[1], c[2], c[3], 0) end
            -- Down into the channels either side, the road left clear for the
            -- jeeps; what is left of it in the way trips him all the same.
            for _, sd in { -1, 1 } do
                for a = -2, 2 do
                    for o = 8, 11 do
                        local cx, cz = math.floor(x + lx * o * sd + t.at[6] * a), math.floor(z + lz * o * sd + t.at[7] * a)
                        core.set_block(cx, fy - 2, cz, A.TB.basalt)
                        table.insert(t.cells, { cx, fy - 2, cz })
                    end
                end
            end
            tell(0, string.format("boom %.2f %.2f %.2f 4", x, fy + 38, z))
            tell(0, string.format("boom %.2f %.2f %.2f 4", x, fy + 1, z))
            core.play_sound("assault_game:boom", x, fy + 20, z)
            A.say("MASSI GIU': BASOALTH CI INCIAMPERA'!")
        else
            for _, c in t.cells do core.set_block(c[1], c[2], c[3], 0) end
            local hx, hz = x + lx * 9, z + lz * 9
            tell(0, string.format("boom %.2f %.2f %.2f 5", hx, fy + 1, hz))
            core.play_sound("assault_game:boom", hx, fy, hz)
            local behind = t.s - (A.f - 10)
            if behind < 40 then
                A.heat = (A.heat or 0) + 300
                A.reel_until = math.max(A.reel_until or 0, clock + 2.5)
                A.act = { "blast", clock + 2.5 }
                A.say("IL SERBATOIO ESPLODE IN FACCIA A BASOALTH!")
            else
                A.say("SERBATOIO ESPLOSO: TROPPO PRESTO, ERA LONTANO")
            end
            t.used = true
        end
    end

    -- What he walks into.
    function A.step_traps()
        local at = A.f - 10
        for _, t in A.traps do
            if not t.open_at or t.used or at < t.s - 3 then continue end
            if t.kind == "valve" then
                if clock - t.open_at <= 25 then
                    t.used = true
                    A.burn_until = clock + 5
                    A.act = { "burn", clock + 5 }
                    A.say("BASOALTH SI BRUCIA NELLA LAVA!")
                end
            elseif t.kind == "rocks" then
                t.used = true
                A.reel_until = math.max(A.reel_until or 0, clock + 4)
                A.act = { "trip", clock + 4 }
                A.say("BASOALTH INCIAMPA NEI MASSI!")
            end
        end
    end

    -- ------------------------------------------------ the escape

    function A.jeeps()
        local n = 3   -- those the players leave are crewed by bots (A.step_crews) and fight too
        -- Two side by side in the tunnel's foot, the third ahead of them.
        for i, at in { { MID - 3.5, 1022.5 }, { MID + 4.5, 1022.5 }, { MID + 0.5, 1032.5 } } do
            if i <= n then M.order("jeep", at[1], at[2], math.pi, 1, false, function(id) M.list[id].escape = true end) end
        end
    end

    -- The point s along the road, which way it goes there, and the height a
    -- body stands at there.
    function A.at(s: number): (number, number, number, number, number)
        local P = A.PATH
        for i = 1, #P - 1 do
            local a, b = P[i], P[i + 1]
            local dx, dz = b[1] - a[1], b[2] - a[2]
            local l = math.sqrt(dx * dx + dz * dz)
            if s <= l or i == #P - 1 then
                local k = math.max(s, -60) / l
                -- Level for 12 either side of a corner (assault_core's tunnel).
                local f0, f1 = if i > 1 then 12 else 0, if i < #P - 1 then 12 else 0
                local u = math.clamp((math.max(s, -60) - f0) / math.max(1, l - f0 - f1), 0, 1)
                local y = a[3] + math.floor((b[3] - a[3]) * u + 0.5)
                return a[1] + dx * k + 0.5, a[2] + dz * k + 0.5, dx / l, dz / l, y
            end
            s -= l
        end
        return P[1][1], P[1][2], 0, 1, P[1][3]
    end

    -- How far along the road x, z is, and how far off it.
    function A.along(x: number, z: number): (number, number)
        local P = A.PATH
        local best, off, base = 0, 1e9, 0
        for i = 1, #P - 1 do
            local a, b = P[i], P[i + 1]
            local dx, dz = b[1] - a[1], b[2] - a[2]
            local l = math.sqrt(dx * dx + dz * dz)
            local k = math.clamp(((x - 0.5 - a[1]) * dx + (z - 0.5 - a[2]) * dz) / (l * l), 0, 1)
            local px, pz = a[1] + dx * k, a[2] + dz * k
            local d = math.sqrt((x - 0.5 - px) ^ 2 + (z - 0.5 - pz) ^ 2)
            if d < off then best, off = base + k * l, d end
            base += l
        end
        return best, off
    end

    function A.start_escape()
        A.stage = "escape"
        A.f, A.carved = -14, 18
        A.go_at = clock + A.GRACE
        A.smash_at, A.orb_at = A.go_at + 3, A.go_at + 2
        A.reel_until, A.head_hp = 0, 0
        A.out, A.done = {}, {}
        A.smashes, A.craters = {}, {}
        -- No crates on the road: everybody's guns full for every try.
        for p, s in players do
            A.spawn_at(p, A.START[1], A.START[2], A.START[3])
            s.died = true
        end
        if A.giant == nil then
            A.giant = A.thing("basoalth_giant", A.CORE[1], GROUND, A.CORE[3], 0, 0, nil)
        end
        -- Every hit on him holds him back (A.heat, A.slowed); his head counts
        -- double, and hit hard enough he reels.
        A.heat = 0
        A.body = A.thing("orb", 0, 0, 0, 3.2, 28, function(dmg: number, _by: number, _w: string): boolean
            A.heat += dmg
            return false
        end)
        if A.body then
            core.ecs.visible(A.body.id, false)
            A.body.big = true
            table.insert(A.things, A.body)
        end
        A.head = A.thing("orb", 0, 0, 0, 2.6, 4.5, function(dmg: number, _by: number, _w: string): boolean
            A.heat += dmg * 2
            A.head_hp += dmg
            if A.head_hp >= 60 then
                A.head_hp = 0
                A.reel_until = clock + 3.5
                if A.giant and A.giant.id then core.ecs.clip(A.giant.id, "reel") end
                A.say("BASOALTH BARCOLLA!")
            end
            return false
        end)
        if A.head then
            core.ecs.visible(A.head.id, false)
            A.head.big = true
            table.insert(A.things, A.head)
        end
        A.build_traps()
        A.burn_until, A.act = 0, nil
        A.say("SPARATE A VALVOLE, MASSI E SERBATOI NEL TUNNEL PER RALLENTARLO")
        core.log("assault_game: the escape")
    end

    -- The furthest along any jeep with somebody in it is.
    function A.lead(): number
        local lead = -1e9
        for id, m in M.list do
            if m.escape and not m.dead and next(m.riders) ~= nil then
                local x, _, z = M.where(id)
                lead = math.max(lead, (A.along(x, z)))
            end
        end
        return lead
    end

    -- The road at s, across and down, gone.
    -- In the river of lava across the valley (assault_core): lost.
    A.LAVA = core.node_id("assault_core:lava")
    function A.in_lava(x: number, y: number, z: number): boolean
        local bx, by, bz = math.floor(x), math.floor(y), math.floor(z)
        for d = 0, 2 do
            if core.get_block(bx, by - d, bz) == A.LAVA then return true end
        end
        return false
    end

    -- Over the tunnel's channels of magma, either side of its road.
    A.MAGMA = core.node_id("assault_core:magma")
    function A.in_magma(x: number, y: number, z: number): boolean
        local bx, by, bz = math.floor(x), math.floor(y), math.floor(z)
        for d = 1, 3 do
            if core.get_block(bx, by - d, bz) == A.MAGMA then return true end
        end
        return false
    end

    -- The tunnel's floor gone across, three deep, under his feet (assault_core
    -- lays it again before the next try: its REFLOOR).
    function A.break_road(s: number)
        local x, z, dx, dz, fy = A.at(s)
        for w = -12, 12 do
            local cx, cz = math.floor(x - dz * w), math.floor(z + dx * w)
            for y = fy - 3, fy - 1 do core.set_block(cx, y, cz, 0) end
        end
    end

    -- Where the giant stands for front f: on the road's middle line, right
    -- behind the jeeps, breaking it as he comes -- on the ground in the
    -- valley, down in the chasm where the road was (his head over it).
    function A.giant_pos(f: number): (number, number, number, number)
        local x, z, dx, dz, fy = A.at(f - 10)
        return x, fy, z, math.atan2(dx, -dz)
    end

    -- How much the hits on him hold him back: 1 is his full pace, down to
    -- 0.4 under steady fire. What has hit him fades with a time of 2 s, so a
    -- gunner on him holds him at about 0.73, two at about 0.63.
    function A.slowed(dt: number): number
        A.heat = (A.heat or 0) * math.exp(-dt / 2)
        local slow = A.heat / (A.heat + 150)
        return 1 - 0.6 * slow
    end

    function A.step_escape(dt: number)
        A.step_traps()
        if A.jeeps_at and clock >= A.jeeps_at then
            A.jeeps_at = nil
            A.jeeps()
        end
        local lead = A.lead()
        local going = clock >= A.go_at
        if going then
            -- Less than on the old road: the tunnel's corners slow the jeeps.
            local v = 9 + math.min(5, (clock - A.go_at) * 0.08)
            if lead - A.f > 50 then v += 6 end
            A.pace = A.slowed(dt)
            v *= A.pace
            if clock < A.reel_until then v *= 0.35 end
            if clock < (A.burn_until or 0) then v *= 0.45 end
            A.f += v * dt
            while A.carved < A.f do
                A.carved += 1
                if A.carved > 18 then A.break_road(A.carved) end
            end
        end
        -- The giant, after the front.
        if A.giant and A.giant.id then
            local gx, gy, gz, yaw = A.giant_pos(A.f)
            core.ecs.set(A.giant.id, "x", gx)
            core.ecs.set(A.giant.id, "y", gy)
            core.ecs.set(A.giant.id, "z", gz)
            core.ecs.set(A.giant.id, "yaw", yaw)
            if A.head then
                A.head.x, A.head.y, A.head.z = gx, gy + 31, gz
            end
            if A.body then
                A.body.x, A.body.y, A.body.z = gx, gy, gz
            end
            -- What a trap did to him plays for as long as it holds him back
            -- (tools/make_assault_models.py: burn, trip, blast).
            local clip = if A.act and clock < A.act[2] then A.act[1]
                elseif clock < A.reel_until then "reel" elseif going then "walk" else "idle"
            if (A.smash_wind or 0) > clock and not (A.act and clock < A.act[2]) then clip = "smash" end
            if clip ~= A.giant.clip then
                A.giant.clip = clip
                core.ecs.clip(A.giant.id, clip)
            end
        end
        -- A fist down ahead of the first jeep, on one side of the road: marked,
        -- then the hole.
        if going and clock >= A.smash_at and lead > 0 then
            A.smash_at = clock + math.max(3.5, 7 - (clock - A.go_at) * 0.03)
            local s = lead + 32 + math.random() * 16
            if s < A.S_END - 30 then
                local off = if math.random() < 0.5 then -4 else 4
                local x, z, dx, dz, fy = A.at(s)
                local hx, hz = x - dz * off, z + dx * off
                table.insert(A.smashes, { x = hx, y = fy, z = hz, at = clock + 1.6, s = s, off = off })
                A.smash_wind = clock + 1.6
                tell(0, string.format("warn %.1f %.1f %.1f 3 1.6", hx, fy, hz))
            end
        end
        for i = #A.smashes, 1, -1 do
            local sm = A.smashes[i]
            if clock >= sm.at then
                table.remove(A.smashes, i)
                explode(sm.x, sm.y - 1, sm.z, 2.6, 5, 30, 0, 2, "basoalth")
                table.insert(A.craters, { s = sm.s, off = sm.off })
            end
        end
        -- Orbs at the jeeps.
        if going and clock >= A.orb_at and A.head then
            A.orb_at = clock + 2.6
            local list = {}
            for id, m in M.list do
                if m.escape and not m.dead and next(m.riders) ~= nil then table.insert(list, id) end
            end
            if #list > 0 then A.orb(A.head.x, A.head.y, A.head.z, nil, list[math.random(1, #list)]) end
        end
        -- What the front has caught up with: jeeps under his feet, bodies on
        -- the broken road; and what has gone off the road into the magma.
        for id, m in M.list do
            if m.escape and not m.dead then
                local x, y, z = M.where(id)
                local s = A.along(x, z)
                if s < A.f + 2 and going then
                    M.destroy(id, 0, "basoalth")
                elseif A.in_magma(x, y, z) then
                    M.destroy(id, 0, "magma")
                end
            end
        end
        local left = 0
        for p in players do
            if A.out[p] or A.done[p] then continue end
            local x, y, z = core.player_pos(p)
            if x == nil then continue end
            local s, off = A.along(x, z)
            local _, _, _, _, fy = A.at(s)
            local inside = M.seat_of[p] ~= nil
            if core.health(p) <= 0 or y < fy - 8 or (not inside and A.in_magma(x, y, z)) or (going and not inside and off < 14 and s < A.f)
               or (going and A.f > 30 and off > 20) then
                A.out[p] = true
                players[p].died = true
                if core.health(p) > 0 then
                    M.leave(p, true)
                    core.damage(p, 1000)
                end
                tell(p, "msg BASOALTH TI HA PRESO: ASPETTA I COMPAGNI")
                continue
            end
            if s >= A.S_END then
                A.done[p] = true
                M.leave(p)
                core.player_place(p, 170.5, GROUND, 14.5, -1, 0, 0)
                tell(p, "msg FUORI!")
                continue
            end
            left += 1
        end
        if left == 0 and next(players) ~= nil then
            if next(A.done) ~= nil then A.win() else A.retry() end
        end
    end

    -- Nobody out: the road again, the jeeps again, everybody to the tunnel's foot.
    function A.retry()
        for id, m in M.list do
            if m.escape then M.destroy(id, 0, "basoalth") end
        end
        for id, b in bots do
            if b.crew then
                core.mob_remove(id)
                bots[id] = nil
            end
        end
        for o in A.orbs do o.gone = true; A.drop_thing(o) end
        A.drop_thing(A.head)
        A.drop_thing(A.body)
        A.body = nil
        A.head = nil
        core.event.emit("assault:track", {})
        A.jeeps_at = clock + 2   -- once the floor is laid again (assault:track)
        for p in players do
            A.spawn_at(p, A.START[1], A.START[2], A.START[3])
            core.player_place(p, A.START[1] + math.random(-3, 3), A.START[2], A.START[3], 0, 0, 1)
            core.heal(p, 20)
        end
        A.say("BASOALTH VI HA PRESI. ANCORA, ALLE JEEP!")
        A.start_escape()
    end

    function A.win()
        A.stage = "won"
        A.won_at = clock
        A.clear_traps()
        for o in A.orbs do o.gone = true; A.drop_thing(o) end
        A.drop_thing(A.head)
        A.drop_thing(A.body)
        A.body = nil
        A.head = nil
        if A.giant and A.giant.id then core.ecs.clip(A.giant.id, "death") end
        for p in players do
            core.shared_storage_set("assalto:" .. core.player_name(p), "1")
            if not A.done[p] then core.player_place(p, 170.5 + math.random(-3, 3), GROUND, 16.5, -1, 0, 0) end
            core.heal(p, 20)
        end
        tell(0, "roll 1")
        A.say("BASOALTH CADE NEL MAGMA. LA SUA DIMENSIONE CROLLA: AL PORTALE!")
        core.log("assault_game: won")
    end

    function A.step_won(dt: number)
        -- The giant sinks; the dimension comes apart round whoever is still in it.
        if A.giant and A.giant.id then
            local y = (core.ecs.get(A.giant.id, "y") or GROUND) - dt * 1.5
            core.ecs.set(A.giant.id, "y", y)
            if y < GROUND - 60 then A.drop_thing(A.giant) end
        end
        if clock >= (A.doom_at or 0) then
            A.doom_at = clock + 0.7
            for p in players do
                local x, y, z = core.player_pos(p)
                if x ~= nil then
                    tell(p, string.format("boom %.2f %.2f %.2f 3", x + math.random(-40, 40), y + math.random(0, 20),
                                          z + math.random(-40, 40)))
                end
            end
        end
    end

    -- A jeep's crew in the escape: somebody in it and nobody at the wheel, a
    -- bot drives; a player driving and nobody at its gun, a bot shoots.
    function A.step_crews()
        for id, m in M.list do
            -- A jeep with a player in it after a moment; one nobody took a few
            -- seconds after the garage opens: bots to drive it and man its
            -- gun, out down the road ahead to shoot at his head and his orbs.
            local manned = next(m.riders) ~= nil
            if not m.escape or m.dead or (not manned and clock < A.go_at - A.GRACE + 4) then
                m.crew_at = nil
                continue
            end
            m.crew_at = m.crew_at or clock + (if manned then 1.5 else 0)
            if clock < m.crew_at then continue end
            for _, i in { 1, 3 } do
                if m.riders[i] == nil and m.bots[i] == nil then
                    local x, _, z = M.where(id)
                    local bid = A.spawn_bot(1, x + 2, z)
                    if bid then
                        bots[bid].crew = true
                        bots[bid].role = "watch"
                        if not M.bot_board(bid, bots[bid], id, i) then
                            core.mob_remove(bid)
                            bots[bid] = nil
                        elseif i == 1 then
                            m.wait_until = clock + 0.5   -- no waiting for others: the giant is coming
                        end
                    end
                end
            end
        end
    end

    -- Where a bot at a jeep's wheel heads: 16 on along the road, on the side
    -- away from the next block in the way (z from, z to, nodes east of the
    -- middle) or a hole ahead.
    function A.crew_goal(m)
        local s = A.along(m.pose.x, m.pose.z)
        local ahead = s + 16
        local off = 0
        -- A hole on one side, from before it is ahead until it is passed.
        for _, c in A.craters do
            if c.s > s - 3 and c.s < ahead + 6 then off = -c.off * 0.7 end
        end
        local x, z, dx, dz = A.at(ahead)
        m.goal_x, m.goal_z = x - dz * off, z + dx * off
    end

    -- ------------------------------------------------ the shop

    function A.shop(p: number)
        if not (A.stage == "fort" or A.stage == "hall") then
            tell(p, "msg IL NEGOZIO E' CHIUSO")
            return
        end
        local x, _, z = core.player_pos(p)
        if x == nil or (x - A.base[1]) ^ 2 + (z - A.base[2]) ^ 2 > 30 * 30 then
            tell(p, "msg IL NEGOZIO E' ALLA BASE")
            return
        end
        -- Two columns, so it stays on the screen however long the list grows.
        local name = `assault_game:shop{p}`
        local rows = math.ceil(#A.SHOP / 2)
        core.ui_define(name, 22, 1.6 + rows * 1.15 + 1.6)   -- about 24 cells are a screen's width
        core.ui_label(name, 0.4, 0.4, `Crediti {A.credits}   soldati {A.allies}/{A.MAX_ALLIES}   costruttori {A.builders}/{A.MAX_BUILDERS}`)
        for i, item in A.SHOP do
            local col, row = (i - 1) // rows, (i - 1) % rows
            local x, y = 0.4 + col * 10.9, 1.4 + row * 1.15
            local lvl = A.level[item.id] or 0
            local sold = (item.once and A.bought[item.id]) or (item.max and lvl >= item.max)
            core.ui_label(name, x, y + 0.22, if item.max then `{item.name}  liv. {lvl}/{item.max}` else item.name)
            core.ui_button(name, item.id, x + 7.4, y, 3.0, 0.95, if sold then (if item.max then "Max" else "Preso") else `{A.cost(item)}`)
        end
        core.ui_button(name, "close", 8.5, 1.6 + rows * 1.15 + 0.2, 5, 1.1, "Chiudi")
        core.ui_show(name, true, p)
    end

    core.event.on("core:ui_click", function(ev)
        if string.sub(ev.panel, 1, 17) ~= "assault_game:shop" then return end
        if ev.id == "close" then
            core.ui_show(ev.panel, false, ev.player)
            return
        end
        for _, item in A.SHOP do
            if item.id ~= ev.id then continue end
            if item.once and A.bought[item.id] then return end
            if item.max and (A.level[item.id] or 0) >= item.max then return end
            local cost = A.cost(item)
            if A.credits < cost then
                tell(ev.player, "msg CREDITI INSUFFICIENTI")
                return
            end
            if item.id == "troops" and A.allies >= A.MAX_ALLIES then
                tell(ev.player, "msg NON CI SONO ALTRI SOLDATI")
                return
            end
            if item.id == "builder" and A.builders >= A.MAX_BUILDERS then
                tell(ev.player, "msg NON CI SONO ALTRI COSTRUTTORI")
                return
            end
            A.credits -= cost
            tell(0, `credits {A.credits}`)
            A.bought[item.id] = true
            if item.grow then A.level[item.id] = (A.level[item.id] or 0) + 1 end
            local lvl = A.level[item.id] or 0
            if item.id == "troops" then
                A.allies = math.min(A.MAX_ALLIES, A.allies + 5)
            elseif item.id == "builder" then
                A.builders += 1
            elseif item.id == "arms" then
                A.punch = 1 + 0.15 * lvl
            elseif item.id == "hulls" then
                -- Every machine of the allies now and to come, the same share more.
                local was = A.hull
                A.hull = 1 + 0.25 * lvl
                for _, m in M.list do
                    if m.home[4] == 1 then
                        m.hp *= A.hull / was
                        m.max = m.k.hp * A.hull
                    end
                end
            elseif item.id == "blast" then
                A.blast = 1 - 0.15 * lvl
            elseif item.gun then
                for q in players do A.kit(q) end
            elseif item.machine then
                local n = 1
                for _, m in M.list do
                    if m.home[4] == 1 then n += 1 end
                end
                local h = A.park(n + #M.respawn)
                M.order(item.machine, h[1], h[2], h[3], 1, false)
            end
            A.say(`{string.upper(core.player_name(ev.player))}: {string.upper(item.name)}{if item.grow then ` (LIVELLO {lvl})` else ""}`)
            A.shop(ev.player)
        end
    end)

    -- ------------------------------------------------ the whole

    -- Credits for testing: tools/track.sh --fresh adds its assault_test mod,
    -- which says how many. In the purse now, and again after every reset.
    A.test_credits = 0
    core.event.define("assault:test_credits", { fields = { n = "number" }, cancellable = false })
    core.event.on("assault:test_credits", function(ev)
        A.credits += ev.n - A.test_credits
        A.test_credits = ev.n
        tell(0, `credits {A.credits}`)
        core.log(`assault_game: {ev.n} credits for testing`)
    end)

    -- What the bots need in memory: the base, the stronghold being fought for
    -- and the way between; the fortress once the hall is open.
    function A.keep()
        if clock < (A.keep_at or 0) then return end
        A.keep_at = clock + 15
        local to = if A.stage == "fort" then FORTS[A.fort] else { MID, 976 }
        local from = A.base
        local d = math.abs(to[2] - from[2])
        for k = 0, math.ceil(d / 64) do
            local z = from[2] + (to[2] - from[2]) * math.min(1, k * 64 / math.max(1, d))
            core.keep_loaded(MID, math.floor(z), 48)
        end
    end

    -- Everybody gone: everything put away, ready for the next who come. What
    -- the fight broke comes back as the columns are let go and made again
    -- (no world file); what was changed on purpose is put back here, in case
    -- somebody comes before that. ponytail: a blown gate or a crater near the
    -- camp stays until its columns have been idle thirty seconds.
    function A.reset()
        A.rout(nil)
        for id, b in bots do
            if b.ride then M.bot_leave(id, b) end
            core.mob_remove(id)
            bots[id] = nil
        end
        A.clear_machines()
        M.spawned = false
        for o in A.orbs do A.drop_thing(o) end
        for _, t in { A.head, A.body, A.giant, A.lord, A.core, A.shield } do A.drop_thing(t) end
        for _, pl in A.pylons or {} do A.drop_thing(pl) end
        A.things, A.orbs, A.pylons = {}, {}, nil
        A.head, A.body, A.giant, A.lord, A.core, A.shield = nil, nil, nil, nil, nil, nil
        A.stage, A.fort, A.base, A.credits, A.allies, A.bought, A.progress, A.punch = "fort", 1, { CAMP[1], CAMP[2] }, A.test_credits, 4, {}, 0, 1
        A.started, A.dirty, A.bare = false, false, false
        for _, zn in A.ZONES do
            zn.owner, zn.progress = 2, 0
            if core.loaded(zn.x, GROUND, zn.z) == 1 then A.zone_cloth(zn) end
        end
        A.damage, A.level, A.hull, A.blast, A.builders = {}, {}, 1, 1, 2
        A.counter_at, A.builder_at, A.sapper, A.gate_at = nil, 0, nil, nil
        A.TWISTERS, A.tw_at = {}, nil
        A.income_at, A.reinforce_at, A.reinforcing = nil, 0, false
        local SEAL, KEEP = core.node_id("assault_core:seal"), core.node_id("assault_core:keep")
        if core.loaded(MID, GROUND, 941) == 1 then
            for x = MID - 4, MID + 4 do
                for y = GROUND, GROUND + 7 do
                    for z = 941, 942 do core.set_block(x, y, z, SEAL) end
                end
            end
        end
        if core.loaded(MID, GROUND, 1011) == 1 then
            for x = MID - 6, MID + 6 do
                for y = GROUND, GROUND + 6 do
                    for z = 1010, 1012 do core.set_block(x, y, z, KEEP) end
                end
            end
        end
        for k = 1, 4 do
            if core.loaded(A.POLE[k][1], GROUND, A.POLE[k][2]) == 1 then A.flag(k, A.FOE_CLOTH) end
        end
        core.event.emit("assault:track", {})
        core.event.emit("assault:barracks", { k = 0 })
        A.clear_traps()
        core.event.emit("assault:collapse", { on = false })
        A.deep = false
        A.plans = {}
        core.log("assault_game: reset, nobody here")
    end

    function A.hud()
        if clock < (A.hud_at or 0) then return end
        A.hud_at = clock + 0.5
        local obj, bar, bar2 = "", "", ""
        if A.stage == "fort" then
            obj = `OBIETTIVO {A.fort}/4: PRENDERE {FORT_NAME[A.fort]}   (NEMICI {A.garrison_left(A.fort) + #A.pending})   ZONE BONUS {A.zones_held()}/8`
            bar = string.format("%.2f PRESA DELLA BANDIERA", A.progress / 100)
        elseif A.stage == "hall" then
            obj = if A.bare then string.format("IL NUCLEO E' SCOPERTO: SPARATE! %d", math.ceil(A.bare_until - clock))
                else "ABBATTETE I PILONI CHE NUTRONO LO SCUDO"
            bar = string.format("%.2f PILONI %d/4", A.pylons_up() / 4, A.pylons_up())
            bar2 = string.format("%.2f NUCLEO", math.max(0, A.core_hp / A.core_max))
        elseif A.stage == "rise" then
            obj = "..."
        elseif A.stage == "escape" then
            local lead = math.max(0, A.lead())
            obj = if clock < A.go_at then string.format("ALLE JEEP! BASOALTH SI MUOVE TRA %d", math.ceil(A.go_at - clock))
                else "FUGGITE! SPARATE A BASOALTH PER RALLENTARLO, ALLA TESTA PER FARLO BARCOLLARE"
            bar = string.format("%.2f FUGA", math.clamp(lead / A.S_END, 0, 1))
            bar2 = string.format("%.2f BASOALTH A %d M%s", math.clamp((lead - A.f) / 100, 0, 1), math.max(0, math.floor(lead - A.f)),
                                 if (A.pace or 1) < 0.9 then string.format("   RALLENTATO %d%%", math.floor((1 - A.pace) * 100)) else "")
        elseif A.stage == "won" then
            obj = "LA DIMENSIONE CROLLA: AL PORTALE SULLA ROCCIA!"
        end
        tell(0, `obj {obj}`)
        -- The bonus grounds, for their beacons' light on every screen.
        local zs = {}
        for _, zn in A.ZONES do
            if zn.fort <= A.fort and (A.stage == "fort" or A.stage == "hall") then
                table.insert(zs, `{zn.x},{zn.z},{zn.owner}`)
            end
        end
        tell(0, "zones " .. table.concat(zs, " "))
        tell(0, `bar {bar}`)
        tell(0, `bar2 {bar2}`)
    end

    -- For trying the escape alone (tools/escape.sh's assault_escape mod): the
    -- strongholds and the hall skipped -- everybody to the hall, and the core
    -- broken once its columns are there.
    core.event.define("assault:to_escape", { fields = {}, cancellable = false })
    core.event.on("assault:to_escape", function(_)
        if A.stage ~= "fort" and A.stage ~= "hall" then return end
        A.started = true
        A.rout(nil)
        A.pylons = A.pylons or {}
        for p in players do core.player_place(p, MID + 0.5, GROUND, 960.5, 0, 0, 1) end
        A.rise_at = clock + 4
        core.log("assault_game: straight to the escape")
    end)

    function A.step(dt: number)
        if A.rise_at and clock >= A.rise_at then
            A.rise_at = nil
            A.rise()
        end
        if #core.players() == 0 then
            A.empty_at = A.empty_at or clock
            if A.dirty and clock - A.empty_at > 20 then A.reset() end
            return
        end
        A.empty_at = nil
        A.dirty = true
        if not A.started then
            A.started = true
            A.man(1)
        end
        A.step_pending()
        A.keep()
        A.supply()
        A.unstick()
        if A.stage == "fort" then
            A.step_fort()
            A.step_zones()
            A.step_counter()
        elseif A.stage == "hall" then A.step_hall()
        elseif A.stage == "rise" then A.step_rise(dt)
        elseif A.stage == "escape" then A.step_crews(); A.step_escape(dt)
        elseif A.stage == "won" then A.step_won(dt) end
        A.step_orbs(dt)
        A.step_twisters(dt)
        A.hud()
    end

    -- ---------------------------------------------------------------- the tick

    local tags_at = 0

    local function step_players()
        local here = {}
        for _, p in core.players() do
            here[p] = true
            local s = players[p]
            -- The way home at the end of the road opens only once it is over.
            core.portal_seal(p, 1, A.stage ~= "won")
            if s == nil then
                if core.player_pos(p) == nil then continue end   -- not placed yet
                s = { team = 1, kills = 0, held = false, next_fire = 0, dry = false, keys = { 0, 0, 0 },
                      throw_at = 0, reload_at = 0, reloading = "", supply_at = 0 }
                refill(s)
                players[p] = s
                A.kit(p)
                -- Come in later: out where the others are.
                if A.stage == "fort" and A.fort > 1 then
                    to_base(p, s)
                elseif A.stage == "escape" then
                    A.spawn_at(p, A.START[1], A.START[2], A.START[3])
                    core.player_place(p, A.START[1], A.START[2], A.START[3], 0, 0, 1)
                    A.out[p] = true
                else
                    A.spawn_at(p, base_spot(1))
                end
                tell(p, "team 1")
                tell(p, ammo_line(s))
                tell(p, `credits {A.credits}`)
                tell(p, "msg LIBERATE LA VALLE: B ALLA BASE PER IL NEGOZIO")
                core.log(`assault_game: {core.player_name(p)} joins`)
            end
            if s.home then
                still_going_home(p, s)
            end
            -- Back from dying -- shot, blown up, a fall, the magma: the engine
            -- puts a body at its spawn whole and with its bag. Every gun full
            -- again, and whatever it was in the middle of forgotten.
            local px, py, pz = core.player_pos(p)
            local sp = A.spawn_of[p]
            if px ~= nil and s.at and sp and (px - s.at[1]) ^ 2 + (pz - s.at[2]) ^ 2 > 36
               and (px - sp[1]) ^ 2 + (pz - sp[3]) ^ 2 < 4 then
                s.died = true
            end
            if px ~= nil then s.at = { px, pz } end
            if s.died then
                s.died = false
                refill(s)
                s.held, s.held2, s.throw_at, s.reloading = false, false, 0, ""
                M.sights(p, s, false)
                A.kit(p)
                tell(p, ammo_line(s))
            end
            if px ~= nil and not M.seat_of[p] and core.health(p) > 0 and A.in_lava(px, py, pz) then
                core.damage(p, 1000)
                s.died = true
                tell(p, "msg NELLA LAVA")
            end
            M.pose(p, s)
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
            -- At the base, its crates top everything up.
            if clock >= s.supply_at then
                s.supply_at = clock + 2
                local x, _, z = core.player_pos(p)
                local b = A.base
                if x ~= nil and math.abs(x - b[1]) < 22 and math.abs(z - b[2]) < 18 then
                    local changed = false
                    for name, g in GUNS do
                        if s.res[name] < g.reserve then
                            s.res[name] = math.min(g.reserve, s.res[name] + g.mag)
                            changed = true
                        end
                    end
                    if s.nades < NADES then
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

        -- Where everybody else is, for the marks over their heads.
        if clock >= tags_at then
            tags_at = clock + 0.25
            local parts = {}
            for p, s in players do
                local x, y, z = core.player_pos(p)
                if x ~= nil and core.health(p) > 0 then
                    parts[p] = string.format("%.1f,%.1f,%.1f,%d", x, y, z, s.team)
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

    function server_step(dt: number)
        step_players()
        M.step(dt)
        step_bots()
        A.step(dt)
        step_nades(dt)
    end
end

-- ===================================================== the player's machine

if drawing then
    local tracers = {}   -- { o, e, left, hit }
    local booms   = {}   -- { x, y, z, r, left }
    local feed    = {}   -- { text, team, left }
    local tags    = {}   -- { x, y, z, team }
    local ammo    = { pistol = { 0, 0 }, rifle = { 0, 0 }, lmg = { 0, 0 }, launcher = { 0, 0 }, sniper = { 0, 0 } }
    local nades, kills = 0, 0
    local team    = 0
    -- What the assault says: the objective, a bar or two, the purse, the
    -- giant's fists to come, and the credits at the end.
    local G = { obj = "", bar = nil, bar2 = nil, credits = 0, warns = {}, roll = -1 }
    local banner, banner_left = "", 0
    local marker, marker_left = 0, 0
    local reload_left, dry_left = 0, 0
    local hand = ""
    local COLOUR = { { 60, 120, 240 }, { 170, 60, 230 } }   -- the allies, the legion
    -- In a machine: which, which seat, what it fires; the keys held; what the
    -- server last said of it; where a lobbing gun would land.
    local V = { seat = nil, keys = { 0, 0, 0 }, held = {}, mach = nil, mark = nil, mark_left = 0, burns = {},
                torn = {}, torn_spin = 0, jets = {} }

    heard = function(text: string)
        local w = string.split(text, " ")
        local what = w[1]
        if what == "shot" and #w == 8 then
            table.insert(tracers, {
                o = { tonumber(w[2]), tonumber(w[3]), tonumber(w[4]) },
                e = { tonumber(w[5]), tonumber(w[6]), tonumber(w[7]) },
                left = 0.06, hit = w[8] == "1",
            })
        elseif what == "torn" then
            -- The twisters: each where the server says, drawn gliding there.
            local n = (#w - 1) // 3
            for i = 1, n do
                local x, y, z = tonumber(w[3 * i - 1]), tonumber(w[3 * i]), tonumber(w[3 * i + 1])
                local t = V.torn[i]
                if t == nil then
                    V.torn[i] = { x = x, y = y, z = z, tx = x, ty = y, tz = z }
                else
                    t.tx, t.ty, t.tz = x, y, z
                end
            end
            for i = #V.torn, n + 1, -1 do table.remove(V.torn, i) end
        elseif what == "jet" and #w >= 7 then
            -- A valve shot open: lava thrown across the tunnel for a while.
            table.insert(V.jets, { x = tonumber(w[2]) or 0, y = tonumber(w[3]) or 0, z = tonumber(w[4]) or 0,
                                   dx = tonumber(w[5]) or 0, dz = tonumber(w[6]) or 0, left = tonumber(w[7]) or 20 })
        elseif what == "boom" then
            table.insert(booms, { x = tonumber(w[2]), y = tonumber(w[3]), z = tonumber(w[4]),
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
        elseif what == "obj" then
            G.obj = table.concat(w, " ", 2)
        elseif what == "bar" or what == "bar2" then
            local b = if #w >= 3 then { k = tonumber(w[2]) or 0, text = table.concat(w, " ", 3) } else nil
            if what == "bar" then G.bar = b else G.bar2 = b end
        elseif what == "credits" then
            G.credits = tonumber(w[2]) or 0
        elseif what == "zones" then
            G.zones = {}
            for i = 2, #w do
                local f = string.split(w[i], ",")
                table.insert(G.zones, { tonumber(f[1]) or 0, tonumber(f[2]) or 0, tonumber(f[3]) or 2 })
            end
        elseif what == "warn" then
            table.insert(G.warns, { x = tonumber(w[2]) or 0, y = tonumber(w[3]) or 0, z = tonumber(w[4]) or 0,
                                    r = tonumber(w[5]) or 3, left = tonumber(w[6]) or 1.5 })
        elseif what == "roll" then
            G.roll = if w[2] == "1" then 0 else -1
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
                V.seat.pose = w[8]
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
            table.insert(booms, { x = tonumber(w[2]), y = tonumber(w[3]), z = tonumber(w[4]), r = 1.2, left = 1.6 })
        end
    end

    if not serving then
        core.event.on("core:net_message", function(ev)
            heard(ev.text)
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
    local DRIVE_KEYS = { key_w = { 1, 1 }, key_s = { 1, -1 }, key_d = { 2, 1 }, key_a = { 2, -1 },
                         key_space = { 3, 1 }, key_shift = { 3, -1 } }

    local GUN_NAME = { mg = "MACHINE GUN", cannon = "CANNON", rockets = "ROCKETS", plane = "GUNS / BOMBS",
                       mortar = "MORTAR", howitzer = "HOWITZER", bomber = "BOMB BAY", aa = "AA MISSILES" }

    -- In a seat with no gun of the machine's: the gun in the hands.
    function V.own_gun(): boolean
        return V.seat ~= nil and V.seat.role ~= "drive" and GUN_NAME[V.seat.gun] == nil
    end

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
            local own = V.seat == nil or (V.seat.role ~= "drive" and GUN_NAME[V.seat.gun] == nil)   -- a passenger aims too
            V.aiming = own
            ask(if own then "aim1" else "fire2", ev.player)
        elseif a == "mouse_right_up" then
            local own = V.seat == nil or (V.seat.role ~= "drive" and GUN_NAME[V.seat.gun] == nil)
            V.aiming = false
            ask(if own then "aim0" else "fire2up", ev.player)
        elseif a == "key_r" then
            ev.cancel = true
            ask("reload", ev.player)
        elseif a == "key_f" then
            ev.cancel = true
            ask("use", ev.player)
        elseif a == "key_b" then
            ev.cancel = true
            ask("shop", ev.player)
        elseif V.seat and string.match(a, "^key_%d$") and not V.own_gun() then
            -- At the wheel or a machine's gun the number keys change seat; a
            -- passenger shoots its own gun and they pick it, as on foot.
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
    core.render.pass("assault_game:fx", 100, function()
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
        -- A twister: the funnel is the cloud pass's (core.storm_funnel, from
        -- client_step); here the bits whirling round its foot.
        for _, t in V.torn do
            local spin = V.torn_spin
            for i = 0, 17 do
                local a = spin * 3 + i * 0.35
                local r = 1.5 + (i % 6) * 0.9
                core.render.box(t.x + math.cos(a) * r, t.y + 0.3 + (i % 5) * 0.6 + math.sin(spin * 2 + i) * 0.3,
                                t.z + math.sin(a) * r, 0.25, 0.25, 0.25, 110, 90, 70, 255)
            end
        end

        -- A valve's lava: blobs thrown in an arc across the tunnel, down onto
        -- the road and into the far channel, flowing on while it lasts.
        for _, j in V.jets do
            local k = math.min(1, j.left / 2)
            for i = 0, 23 do
                local u = ((i / 24) + V.torn_spin * 0.5) % 1
                local out = u * 22
                local up = 4 * u * (1 - u) * 6 - u * 7
                local wob = math.sin(i * 1.7 + V.torn_spin * 4) * 0.4
                local sz = (0.45 + 0.25 * math.sin(i * 2.3 + V.torn_spin * 6)) * k
                core.render.box(j.x + j.dx * out - j.dz * wob, j.y + up, j.z + j.dz * out + j.dx * wob,
                                sz, sz, sz, 255, 110 + (i * 23) % 90, 20, 255)
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
        -- The bonus grounds: a beam of light over each beacon, the legion's
        -- violet while it holds it, blue once it is the allies'.
        for _, zn in G.zones or {} do
            local c = if zn[3] == 1 then { 70, 140, 255 } else { 190, 60, 240 }
            for k = 0, 14 do
                core.render.box(zn[1] + 0.5, GROUND + 7 + k * 2, zn[2] + 0.5, 0.35, 0.8, 0.35, c[1], c[2], c[3], 150)
            end
        end
        -- Where the giant's fist comes down: a red ring closing in, a column.
        for _, wn in G.warns do
            local r = wn.r * (0.4 + wn.left / 1.6)
            for i = 0, 15 do
                local a = i / 16 * 2 * math.pi
                core.render.box(wn.x + math.cos(a) * r, wn.y + 0.15, wn.z + math.sin(a) * r, 0.3, 0.12, 0.3, 255, 40, 30, 255)
            end
            for k = 0, 8 do core.render.box(wn.x, wn.y + k * 1.5, wn.z, 0.2, 0.6, 0.2, 255, 60, 40, 200) end
        end
        -- Over every other player, their team's colour: a diamond of two boxes.
        for _, t in tags do
            local c = COLOUR[t.team] or { 200, 200, 200 }
            core.render.box(t.x, t.y + 2.15, t.z, 0.12, 0.12, 0.12, c[1], c[2], c[3], 255)
        end
    end)


    local function weapon_line(): (string, boolean)
        -- At the wheel or at a machine's gun, that; a passenger has their own.
        local name = V.seat and GUN_NAME[V.seat.gun]
        if V.seat and (name ~= nil or V.seat.role == "drive") then
            if name == nil then return "DRIVER", true end
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

    core.hud.pass("assault_game:hud", 100, function()
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
            local r, g, b = 230, 240, 230
            if not ok or dry_left > 0 then r, g, b = 255, 80, 60 end
            core.hud.text(0.97, 0.9, line, 0.042, 2, r, g, b, 255)
        end
        core.hud.text(0.97, 0.95, `GRENADES {nades}   KILLS {kills}`, 0.026, 2, 190, 200, 190, 220)

        -- In a machine: what it is, how much of it is left, how it goes.
        if V.seat and V.seat.gun == "aa" then
            local L = V.lock
            local text = if L == nil or L.left <= 0 then "NO TARGET" elseif L.on then "LOCKED -- FIRE"
                else string.format("LOCKING %d%%", math.floor(L.k * 100))
            local c = if L and L.left > 0 and L.on then { 255, 60, 40 } else { 255, 220, 60 }
            core.hud.text(0.5, 0.58, text, 0.028, 1, c[1], c[2], c[3], 255)
        end
        if V.seat then
            local m = V.mach
            local x0 = 0.03
            core.hud.text(x0, 0.82, `{string.upper(V.seat.kind)}   SEAT {V.seat.n}/{V.seat.seats}   F: OUT   `
                          .. (if V.own_gun() then "1-9: ARMI" else `1-{V.seat.seats}: SEAT`),
                          0.024, 0, 220, 230, 220, 255)
            if m then
                local k = math.clamp(m.hp / math.max(1, m.max), 0, 1)
                core.hud.rect(x0 + 0.1, 0.87, 0.2, 0.018, 40, 40, 40, 200)
                core.hud.rect(x0 + 0.1 * k, 0.87, 0.2 * k, 0.018, math.floor(255 * (1 - k)), math.floor(200 * k + 40), 40, 255)
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
        -- The objective, its bars, the purse.
        core.hud.text(0.5, 0.035, G.obj, 0.028, 1, 235, 225, 245, 255)
        for i, b in { G.bar or false, G.bar2 or false } do
            if b then
                local y = 0.075 + (i - 1) * 0.045
                local k = math.clamp(b.k, 0, 1)
                core.hud.rect(0.5, y, 0.36, 0.016, 30, 20, 40, 210)
                core.hud.rect(0.32 + 0.18 * k, y, 0.36 * k, 0.012, if i == 1 then 90 else 190,
                              if i == 1 then 150 else 60, if i == 1 then 240 else 230, 255)
                core.hud.text(0.5, y + 0.022, b.text, 0.02, 1, 220, 215, 230, 230)
            end
        end
        core.hud.text(0.03, 0.04, `CREDITI {G.credits}   B: NEGOZIO`, 0.026, 0, 250, 220, 120, 255)
        if G.roll >= 0 then
            core.hud.rect(0.5, 0.5, 4.0, 2.0, 0, 0, 0, math.floor(math.min(200, G.roll * 40)))
            local ROLL = { "BASOALTH E' CADUTO", "", "La sua dimensione si spegne alle vostre spalle.",
                           "La legione non esiste piu'.", "", "Il portale sulla roccia vi riporta a casa.",
                           "", "CUBES AND PORTALS", "Un gioco di Giacomo Perin" }
            for i, line in ROLL do
                local y = 1.05 + i * 0.06 - G.roll * 0.05
                if y > -0.05 and y < 1.05 then core.hud.text(0.5, y, line, if i == 1 then 0.06 else 0.034, 1) end
            end
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
        for i = #G.warns, 1, -1 do
            G.warns[i].left -= dt
            if G.warns[i].left <= 0 then table.remove(G.warns, i) end
        end
        if G.roll >= 0 then G.roll += dt end
        if G.roll > 30 then G.roll = -1 end
        V.mark_left -= dt
        if V.lock then V.lock.left -= dt end
        V.torn_spin += dt * 2.5
        for i = #V.jets, 1, -1 do
            V.jets[i].left -= dt
            if V.jets[i].left <= 0 then table.remove(V.jets, i) end
        end
        for i = 1, 4 do
            local t = V.torn[i]
            if t then
                local k = math.min(1, dt * 5)
                t.x += (t.tx - t.x) * k
                t.y += (t.ty - t.y) * k
                t.z += (t.tz - t.z) * k
                core.storm_funnel(i - 1, t.x, t.y, t.z)
            else
                core.storm_funnel(i - 1, 0, 0, 0, false)
            end
        end

        -- Down the sights while the right button is held and the hand has a
        -- gun: its zoom, and through the sniper's scope from the eye whatever
        -- the camera was (put back after). And what this body's arms do,
        -- one clip for the arms seen in first person and the body seen from
        -- behind (tools/make_front_models.py, make_front_poses.py): a
        -- reload, the sights, or the gun held ready.
        local gun = short(hand)
        local g = GUNS[gun]
        local aimed = if V.aiming and g ~= nil and g.zoom then gun else nil
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
        if V.seat == nil or V.seat.role ~= "drive" then
            local clip = if g == nil then ""
                elseif reload_left > 0 and V.reloading == gun then `reload_{gun}`
                elseif aimed then `aim_{gun}`
                else `hold_{gun}`
            -- In a seat: at a machine's gun, the hands on it; anywhere
            -- else this body's own gun, sat (M.seat_clip, the server's copy).
            if V.seat then clip = M_seat_clip(V.seat.pose == "stand", GUN_NAME[V.seat.gun] ~= nil, clip) end
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
            if string.find(hand, "^assault_game:") then
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
