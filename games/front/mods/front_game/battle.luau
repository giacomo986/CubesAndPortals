--!strict
-- battle: the guns, the grenades, the bots and the machines of a battle on
-- the server, as games/front (front_game) and games/assault (assault_game)
-- both fight it. The assault began as a copy of the front, and every fix to
-- a bot, a machine or a gun had to be made twice or was missed once; this is
-- the one copy both load.
--
-- One file in one place: it lives in front_game, and assault_game's
-- battle.luau is a link to it. Each mod runs in a VM of its own, so this is
-- shared as source, not as a table: each game requires it (`@self/battle`)
-- and gets a copy of its own, and a player's machine gets it in the game's
-- world code like any other file of the mod (src/modpack/mod_package.cpp
-- packs a linked file under its own name).
--
-- What differs from game to game is not in here: the game tells it with a
-- few values when it starts the battle (battle.server) and with the hooks on
-- the table that gives back (Game, below) -- what a kill costs, where a bot
-- is going, where the ground is. Everything else -- the round, the points,
-- the strongholds, the shop, a bot's mind, the player's machine -- is the
-- game's.

local battle = {}

battle.EYE = 1.5

-- The guns: `every` seconds between rounds while the trigger is held (the
-- pistol fires once a pull), `mag` rounds a magazine, `reserve` more carried.
-- Health is 20: a rifle kills in five, two of them to the head. `spread` is
-- how far off a round goes from the hip, `ads` aimed down the sights (the
-- right button, which also slows the body to AIM_WALK); `zoom` the view
-- then, the sniper's through its scope.
export type Gun = { every: number, damage: number, spread: number, ads: number, zoom: number, range: number,
                    mag: number, reserve: number, reload: number, auto: boolean, scope: boolean?, rocket: boolean? }
local GUNS: { [string]: Gun } = {
    pistol = { every = 0.2, damage = 5, spread = 0.035, ads = 0.006, zoom = 0.85, range = 90, mag = 12, reserve = 60, reload = 1.2, auto = false },
    rifle  = { every = 0.1, damage = 4, spread = 0.045, ads = 0.005, zoom = 0.75, range = 160, mag = 30, reserve = 180, reload = 2.0, auto = true },
    lmg    = { every = 0.075, damage = 4, spread = 0.07, ads = 0.015, zoom = 0.8, range = 160, mag = 100, reserve = 200, reload = 4.0, auto = true },
    sniper = { every = 1.2, damage = 18, spread = 0.09, ads = 0.0004, zoom = 0.2, range = 300, mag = 5, reserve = 25, reload = 3.0, auto = false, scope = true },
    -- A rocket, not a round (M.shoot): against what the bullets bounce off.
    launcher = { every = 0.5, damage = 0, spread = 0.03, ads = 0.004, zoom = 0.85, range = 0, mag = 1, reserve = 4, reload = 2.5, auto = false, rocket = true },
}
battle.GUNS = GUNS
local EYE        = battle.EYE
local AIM_WALK   = 1.8    -- m/s aiming, walk and sprint alike
local HEAD       = 1.45   -- a hit this far above the feet is a head: double
local NADES      = 3      -- grenades carried
battle.NADES = NADES
local NADE_SPEED = 17
local NADE_FUSE  = 3.0
local NADE_R     = 2.6    -- the hole
local NADE_REACH = 6.5    -- the blast
local NADE_DMG   = 26
local BOT_SIGHT  = 48

local drawing = core.side() ~= "server"

-- What the server says to a player's machine (0: to every one). A process
-- that is its own server and client hands it straight to what the client
-- half put in battle.heard.
battle.heard = function(_: string) end
local function tell(player: number, text: string)
    if drawing then
        battle.heard(text)
    else
        core.net.send(player, text)
    end
end
battle.tell = tell

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
export type Obb = { x: number, y: number, z: number, yaw: number, hw: number, hl: number, h: number }
local function ray_obb(o: {number}, d: {number}, b: Obb, far: number): number?
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
battle.wrap = wrap

-- How a body in a seat that is not the wheel is drawn, where passengers
-- fight (Game.PASSENGERS_ARMED): at a machine's gun its hands on it --
-- standing, the arms forward as if aimed; sat, as at a wheel -- and anywhere
-- else with its own gun's clip of the arms (`arms`), over the sitting legs
-- when it sits ("base+arms", the client's). The server's and the client's.
function battle.seat_clip(stand: boolean, mounted: boolean, arms: string): string
    if mounted then return if stand then "aim_lmg" else "drive_idle" end
    if stand then return arms end
    return if arms == "" then "drive_idle" else "drive_idle+" .. arms
end

-- ===================================================================== server

-- A player fighting (Game.players); the fields after `supply_at` come and go.
export type Player = {
    team: number, kills: number, held: boolean, next_fire: number, dry: boolean, keys: {number},
    throw_at: number, reload_at: number, reloading: string, supply_at: number,
    mag: { [string]: number }, res: { [string]: number }, nades: number,
    home: {number}?, throwing_until: number?, firing_until: number?, aiming: boolean?, clip: string?,
    ray: {number}?, aim: {number}?, held2: boolean?,
    lock_t: number?, lock_on: number?, lock_said: number?, locking: boolean?, died: boolean?, at: {number}?,
}
-- Every body a shot or a blast may find (enemies); `thing` is something else
-- a game puts there (Game.more_targets), with its own `hit`.
export type Target = {
    player: number?, bot: number?, machine: number?, thing: any, team: number, feet: number,
    lo: {number}, hi: {number}, obb: Obb?, empty: boolean?, big: boolean?, self: boolean?,
}
-- What hurt takes: a target, or only who it is.
export type Hit = { read player: number?, read bot: number?, read machine: number?, read thing: any, read team: number? }
export type Spotted = { t: Target?, x: number, y: number, z: number, d: number, key: number }
export type Bot = {
    team: number, n: number, think_at: number, shot_at: number, nade_at: number, enemy: Spotted?,
    goal: number, throwing: number, role: string, ammo: { [string]: { mag: number, res: number } },
    gun: string, burst: number, yaw: number,
    dead: boolean?, alert: {number}?, clip: string?, item: string?, react_at: number?,
    mem: { key: number, x: number, y: number, z: number, t: number }?,
    ride: { id: number, seat: number }?, phase: string?, phase_until: number?, hold_fire: boolean?,
    cover: {number}?, peek: {number}?, last: {number}?, aside_until: number?, reload_at: number?,
    hold_until: number?, look_at: number?, off_until: number?, busy: number?, lob: {number}?,
    err_yaw: number?, err_pitch: number?, carried: number?, crew: any,
}
-- A seat: x, y, z and what it has; a part: its model, x, y, z and how it moves.
export type Seat = { [number]: number, gun: string?, part: number?, stand: boolean?, on: number? }
export type Part = { [number]: any, by: number, on: number?, turret: boolean?, barrel: boolean?, lob: number?, high: boolean? }
export type Kind = {
    craft: string?, model: string, mass: number?, hw: number, hh: number?, hl: number, h: number,
    accel: number?, top: number?, turn: number?, hp: number, armor: number, view: number,
    clip: string?, wreck: string?, wheel: boolean?, breaks: boolean?, spin: boolean?,
    seats: {Seat}, parts: {Part},
}
export type Stick = { left: number, at: number, by: number, team: number }
export type Machine = {
    kind: string, k: Kind, parts: { [number]: number }, riders: { [number]: number },
    bots: { [number]: number }, hp: number, team: number, aim: { [number]: { yaw: number, pitch: number } },
    next: { [number]: number }, home: {number}, salvo: number, max: number,
    dead: boolean?, again: boolean?, pose: CraftPose?, last_by: number?, side: number?, stick: Stick?,
    lock_t: number?, lock_on: number?, lock_said: number?, aa_left: number?, ran: { [number]: number }?,
    moving: boolean?, sound_at: number?, bot_fwd: number?, wait_until: number?, goal_x: number?,
    goal_z: number?, alone: number?, stuck: number?, back_until: number?, claim: {number}?, bomb_at: number?,
    want: number?,
}
export type ShotSpec = { model: string, g: number, r: number, reach: number, dmg: number, direct: number, air: boolean? }
export type Shot = { kind: string, v: {number}, by: number, team: number, from: number?, t: number, homing: number? }
export type Seen = { x: number, y: number, z: number, t: number }
-- Put down once the ground under it is in memory (M.step): `again` comes
-- back 30 s after it is lost, `then_` is told its id.
export type Order = { kind: string, home: {number}, at: number, again: boolean?, then_: ((number) -> ())? }
-- The machines' table: its data typed, its functions (M.where, ...) under the indexer.
export type Machines = {
    list: { [number]: Machine }, seat_of: { [number]: { id: number, seat: number } },
    wrecks: { { id: number, x: number, y: number, z: number, vy: number, ground: number, gone: number } },
    respawn: { Order }, shots: { [number]: Shot },
    spawned: boolean, seen: { { [number]: Seen } }, hud_at: number?,
    ROLES: {string}, PREFER: { [string]: {string} }, REACT: { [string]: number },
    BOTGUN: { [string]: { every: number, burst: {number}, pause: {number}, spread: number?, sight: number? } },
    BOT_HURTS: number, CONE: number, MODEL: { [string]: number }, THING: string, KINDS: { [string]: Kind },
    SHOTS: { [string]: ShotSpec }, AA_LOCK: number, AA_CONE: number, AA_RANGE: number,
    LOB: { [string]: { cool: number, range: number } }, SOUND: { [string]: {any} }, PARK: { any },
    BOT_DRIVES: { [string]: boolean }, BOT_GUNS: { [string]: boolean },
    [string]: any,
}

-- What a game says of its battle when it starts it (battle.server).
export type Setup = {
    NAME: string,              -- the mod's: its items, models, sounds and log lines
    TEAM_NAME: {string},       -- the kill feed's
    GROUND: number,            -- the field's height, where the air starts for what flies
    SIZE: number,              -- a plane circles inside SIZE on x and z
    LOST_Y: number,            -- a machine under this is lost (crash)
    FAR_Z: number,             -- and one past this on z
    SHOT_FLOOR: number,        -- a shell under this is done with
    TOUGH: { [number]: boolean },   -- the blocks a blast does not take
    NADE: string,              -- the grenades' component, and their model
    NADE_MODEL: number,
}

-- The battle: the Setup, the state, the hooks the game sets, and what this
-- gives back to it.
export type Game = Setup & {
    clock: number,             -- the game's clock: it sets it every tick (server_step)
    players: { [number]: Player },   -- player id -> state
    bots: { [number]: Bot },         -- mob id -> state
    M: Machines,
    -- Passengers fight with their own guns from a seat with none of the
    -- machine's (the assault's); else a seat is the machine's.
    PASSENGERS_ARMED: boolean,
    -- A team whose bots stay at a gun that stays put however long there is
    -- nothing to shoot (the assault's legion, at its post); 0 none.
    HOLDS_GUNS: number,

    -- The hooks.
    on_kill: (team: number, by: number) -> (),   -- one of `team` killed, by `by`
    player_down: (p: number, s: Player) -> (),   -- player p killed (still dead)
    feet_from: () -> number,                     -- the lowest a body's feet are looked for
    -- Where a bot going somewhere goes: x, z; `keep` when its mind is made up.
    goal: (id: number, b: Bot, keep: boolean?) -> {number},
    -- The rest only where a game has them.
    dealt: ((by: number, target: Hit, dmg: number) -> number)?,   -- what a hit takes off, if not dmg
    more_targets: ((team: number, out: {Target}) -> ())?,       -- things besides bodies and machines
    on_carve: ((x: number, y: number, z: number, r: number) -> ())?,
    noise: ((team: number, x: number, z: number, what: string) -> ())?,   -- a shot (its gun) or "blast"
    blast_taken: (() -> number)?,   -- how much of a blast a player takes
    trap_ripe: ((trap: any) -> boolean)?,   -- a thing that is a trap: whether to shoot it now
    things_first: (() -> boolean)?,         -- whether bots go for things before bodies
    crew_goal: ((m: Machine) -> ())?,       -- a crew's driver: where (b.crew)
    hull: ((team: number, hp: number) -> number)?,   -- a new machine's hp
    in_lava: ((x: number, y: number, z: number) -> boolean)?,
    asked_more: ((p: number, s: Player, what: string) -> ())?,   -- what a player's machine asks besides

    -- What battle.server gives back.
    asked: (p: number, what: string) -> (),
    ammo_line: (s: Player) -> string,
    refill: (s: Player) -> (),
    who: (by: number) -> (string, number),
    killed: (by: number, victim: string, team: number, weapon: string) -> (),
    hurt: (by: number, target: Hit, dmg: number, weapon: string, ox: number, oz: number) -> boolean,
    enemies: (team: number) -> {Target},
    explode: (x: number, y: number, z: number, r: number, reach: number, dmg: number, by: number, team: number,
              weapon: string) -> (),
    spawn_nade: (x: number, y: number, z: number, vx: number, vy: number, vz: number, by: number, team: number) -> (),
    throw: (p: number, s: Player) -> (),
    step_nades: (dt: number) -> (),
    jitter: (d: {number}, spread: number) -> {number},
    bullet: (p: number, s: { team: number }, o: {number}, d: {number}, g: { range: number, damage: number },
             gun: string, ignore: number?) -> (),
    fire: (p: number, s: Player) -> (),
    feet_at: (x: number, z: number) -> number,
    spotted: (id: number, b: Bot) -> Spotted?,
    bot_throw: (id: number, b: Bot) -> (),
}

-- The battle's server half, started once a game has registered its items,
-- models and soldiers (the models here come after them, as they always did).
function battle.server(setup: Setup): Game
    local W: Game = {
        NAME = setup.NAME, TEAM_NAME = setup.TEAM_NAME, GROUND = setup.GROUND, SIZE = setup.SIZE,
        LOST_Y = setup.LOST_Y, FAR_Z = setup.FAR_Z, SHOT_FLOOR = setup.SHOT_FLOOR, TOUGH = setup.TOUGH,
        NADE = setup.NADE, NADE_MODEL = setup.NADE_MODEL,
        clock = 0, players = {}, bots = {},
        -- The machines (below, "machines"): one table, so its helpers are
        -- fields and not more of a chunk's 200 locals.
        M = { list = {}, seat_of = {}, wrecks = {}, respawn = {}, shots = {}, spawned = false,
              seen = { {}, {} } } :: any,   -- each team's: what its bots see, by who it is
        PASSENGERS_ARMED = false, HOLDS_GUNS = 0,
        -- Until the game sets its own: a kill costs nothing, a player killed
        -- is left as the engine leaves them, and a bot stays where it is.
        on_kill = function(_, _) end,
        player_down = function(_, _) end,
        feet_from = function() return setup.GROUND - 6 end,
        goal = function(id, _, _)
            local x, _, z = core.mob_pos(id)
            return { x or 0, z or 0 }
        end,
    } :: any
    local NAME, TEAM_NAME, GROUND, SIZE, TOUGH = W.NAME, W.TEAM_NAME, W.GROUND, W.SIZE, W.TOUGH
    local players, bots, M = W.players, W.bots, W.M
    local NADE, NADE_MODEL = W.NADE, W.NADE_MODEL
    local nades: { [number]: boolean } = {}   -- entity id -> true

    local function short(item: string): string
        return (string.gsub(item, `^{NAME}:`, ""))
    end

    local function ammo_line(s: Player): string
        return `ammo {s.mag.pistol} {s.res.pistol} {s.mag.rifle} {s.res.rifle} {s.mag.lmg} {s.res.lmg} {s.nades} {s.kills} {s.mag.launcher} {s.res.launcher} {s.mag.sniper} {s.res.sniper}`
    end

    local function refill(s: Player)
        s.mag, s.res = {}, {}
        for name, g in GUNS do
            s.mag[name], s.res[name] = g.mag, g.reserve
        end
        s.nades = NADES
        s.reload_at = 0
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
        W.on_kill(team, by)
        local name, kt = who(by)
        tell(0, `kill {kt} {name} {team} {victim} {weapon}`)
        core.log(`{NAME}: {name} killed {victim} ({weapon})`)
    end

    -- Takes `dmg` off whatever `target` is; true when that killed it.
    local function hurt(by: number, target: Hit, dmg: number, weapon: string, ox: number, oz: number): boolean
        if target.thing then
            return target.thing.hit(dmg, by, weapon)
        end
        if W.dealt then dmg = W.dealt(by, target, dmg) end
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
                if v then W.player_down(target.player, v) end
                killed(by, core.player_name(target.player), target.team :: number, weapon)
                return true
            end
        elseif target.bot then
            local b = bots[target.bot]
            if b then b.alert = { ox, oz, W.clock } end   -- it turns to where that came from
            if b and core.mob_hurt(target.bot, dmg, ox, oz) == 2 then
                local name = who(-target.bot)
                killed(by, name, target.team :: number, weapon)
                b.dead = true
                return true
            end
        end
        return false
    end

    -- Every body of the other team a shot or a blast may find: { player or
    -- bot, team, box }. Both are 0.7 wide and 1.85 high from the feet.
    local function enemies(team: number): {Target}
        local out: {Target} = {}
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
        if W.more_targets then W.more_targets(team, out) end
        return out
    end

    -- ---------------------------------------------------------------- explosions

    -- The hole: every cell within r but the TOUGH ones is gone, and then what
    -- was standing on what went -- a house's upper wall, a roof -- comes down.
    local function carve(cx: number, cy: number, cz: number, r: number)
        if W.on_carve then W.on_carve(cx, cy, cz, r) end
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
        if W.noise then W.noise(team, x, z, "blast") end
        tell(0, string.format("boom %.2f %.2f %.2f %.1f", x, y, z, r))
        core.play_sound(if r >= 3 then `{NAME}:boom` else `{NAME}:boom_small`, x, y, z)
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
                if t.player and W.blast_taken then hit *= W.blast_taken() end
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

    local function throw(p: number, s: Player)
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
            local x, y, z = core.ecs.get(id, "x"), core.ecs.get(id, "y") :: number, core.ecs.get(id, "z") :: number
            if x == nil then
                nades[id] = nil
                continue
            end
            local t = (core.ecs.get(id, "t") :: number) + dt
            core.ecs.set(id, "t", t)
            if t >= NADE_FUSE then
                nades[id] = nil
                local owner, team = core.ecs.get(id, "owner") or 0, core.ecs.get(id, "team") or 0
                core.ecs.kill(id)
                explode(x, y + 0.2, z, NADE_R, NADE_REACH, NADE_DMG, owner, team, "grenade")
                continue
            end
            local vx, vy, vz = core.ecs.get(id, "vx") :: number, (core.ecs.get(id, "vy") :: number) - 22 * dt,
                               core.ecs.get(id, "vz") :: number
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
    local function bullet(p: number, s: { team: number }, o: {number}, d: {number}, g: { range: number, damage: number },
                          gun: string, ignore: number?)
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
        if W.noise then W.noise(s.team, o[1], o[3], gun) end
        tell(0, string.format("shot %.2f %.2f %.2f %.2f %.2f %.2f %d", o[1], o[2], o[3],
                              e[1], e[2], e[3], if best then 1 else 0))
    end

    local function fire(p: number, s: Player)
        -- In a seat, the machine's gun -- or, where passengers fight, the gun
        -- in the hand from any seat but the wheel and a machine's gun.
        local cur = M.seat_of[p]
        local mm = cur and M.list[cur.id]
        if cur and (not W.PASSENGERS_ARMED or cur.seat == 1 or (mm and mm.k.seats[cur.seat].gun)) then
            M.fire(p, s, false)
            return
        end
        local item = short(core.held_item(p))
        if item == "grenade" then
            if s.nades > 0 and s.throw_at == 0 then
                s.nades -= 1
                s.next_fire = W.clock + 0.9
                s.throw_at = W.clock + 0.42
                s.throwing_until = W.clock + 0.9
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
                s.reload_at, s.reloading = W.clock + g.reload, item
                tell(p, `reload {item}`)
            elseif not s.dry then
                s.dry = true
                tell(p, "dry")
            end
            return
        end
        s.mag[item] -= 1
        s.next_fire = W.clock + g.every
        s.firing_until = W.clock + g.every + 0.08
        if not g.auto then s.held = false end
        tell(p, `fired {item}`)
        tell(p, ammo_line(s))
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then return end
        core.play_sound(`{NAME}:{if g.rocket then "rocket" else item}`, x, y + EYE, z)
        dx, dy, dz = M.toward(p, { x, y + EYE, z }, dx, dy, dz)
        local d = jitter({ dx, dy, dz }, if s.aiming then g.ads else g.spread)
        if g.rocket then
            M.shoot("rocket", { x + d[1] * 0.8, y + EYE - 0.1 + d[2] * 0.8, z + d[3] * 0.8 },
                    { d[1] * 45, d[2] * 45, d[3] * 45 }, p, s.team, nil,
                    if (s.lock_t or 0) >= M.AA_LOCK then s.lock_on else nil)
            return
        end
        bullet(p, s, { x, y + EYE, z }, d, g, item)
    end

    -- What everybody else sees this player's arms do with what they hold
    -- (tools/make_front_poses.py: clips of the arms alone, over the walk):
    -- the throw, a reload, a burst, the sights, or the gun held ready.
    function M.pose(p: number, s: Player)
        local cur = M.seat_of[p]
        local m = cur and M.list[cur.id]
        -- Where passengers fight, a seat but the wheel draws the arms too.
        local seat = if W.PASSENGERS_ARMED and m then m.k.seats[cur.seat] else nil
        if cur and (seat == nil or (cur.seat == 1 and m.k.craft)) then
            s.clip = nil   -- the seat's (M.enter, the keys): looked at again once off it
            return
        end
        local item = short(core.held_item(p))
        local clip = ""
        if (s.throwing_until or 0) > W.clock then
            clip = "fire_grenade"
        elseif GUNS[item] then
            clip = if s.reload_at > 0 and s.reloading == item then `reload_{item}`
                elseif (s.firing_until or 0) > W.clock then `fire_{item}`
                elseif s.aiming then `aim_{item}`
                else `hold_{item}`
        end
        if seat then clip = battle.seat_clip(seat.stand == true, seat.gun ~= nil, clip) end
        if clip ~= s.clip then
            s.clip = clip
            core.player_clip(p, clip)
        end
    end

    -- Aiming down the sights: precise, and slow.
    function M.sights(p: number, s: Player, on: boolean)
        if s.aiming == on then return end
        s.aiming = on
        if on then
            core.body_movement(p, AIM_WALK, AIM_WALK, 0, 6, -25)
        else
            core.body_movement(p, 4.8, 8.5, 0, 8, -25)
        end
    end

    local function asked(p: number, what: string)
        local s = players[p]
        if s == nil then return end
        if what == "fire1" then
            s.held, s.dry = true, false
        elseif what == "fire0" then
            s.held = false
        elseif string.sub(what, 1, 4) == "aim " then
            -- The camera's ray through the crosshair: from, and which way.
            local w = string.split(what, " ")
            -- Six numbers or nothing: a client's text is not trusted, and an
            -- error here would switch the whole game off for everybody.
            local r: {number} = {}
            for i = 2, 7 do
                local n = tonumber(w[i])
                if n == nil then return end
                r[i - 1] = n
            end
            local l = math.sqrt(r[4] ^ 2 + r[5] ^ 2 + r[6] ^ 2)
            if l > 1e-6 then
                s.ray = { r[1], r[2], r[3], r[4] / l, r[5] / l, r[6] / l, W.clock }
                s.aim = nil
            end
        elseif what == "use" then
            M.use(p)
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
                s.reload_at, s.reloading = W.clock + g.reload, item
                tell(p, `reload {item}`)
            end
        elseif W.asked_more then
            W.asked_more(p, s, what)
        end
    end

    core.event.on("core:net_message", function(ev)
        asked(ev.player :: number, ev.text :: string)
    end)

    -- For trying things out: /vehicle jeep puts one six nodes in front, of the
    -- player's team, and none comes back when it is lost (it is not `again`).
    core.command("vehicle", "<jeep|tank|heli|plane|bomber|nest|mortar|howitzer|aa> -- a machine in front of you (test)", "moderator")
    core.event.on("core:command", function(ev)
        if ev.name ~= "vehicle" then return end
        local kind = string.match(ev.args or "", "%S+") or "jeep"
        local x, _, z = core.player_pos(ev.player)
        local dx, _, dz = core.player_look(ev.player)
        local s = players[ev.player]
        if M.KINDS[kind] == nil or x == nil or dx == nil or s == nil then
            core.chat(ev.player, "/vehicle jeep|tank|heli|plane|bomber|nest|mortar|howitzer|aa")
            return
        end
        local l = math.max(0.01, math.sqrt(dx * dx + dz * dz))
        M.spawn(kind, x + dx / l * 6, z + dz / l * 6, math.atan2(dx, -dz), s.team)
    end)

    -- ---------------------------------------------------------------- bots

    -- The cell a body's feet go in at x, z: the lowest one with ground under
    -- it and room for a body over it -- a house's floor, not its roof --
    -- from as low as the game says (Game.feet_from).
    local function feet_at(x: number, z: number): number
        for y = W.feet_from(), GROUND + 12 do
            if core.get_block(x, y - 1, z) ~= 0 and core.get_block(x, y, z) == 0
               and core.get_block(x, y + 1, z) == 0 then
                return y
            end
        end
        return GROUND
    end

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

    -- Turned to look at x, z, and that remembered: what it sees is in front.
    function M.face(id: number, b: Bot, x: number, z: number)
        local bx, _, bz = core.mob_pos(id)
        if bx == nil then return end
        core.mob_face(id, x, z)
        if (x - bx) ^ 2 + (z - bz) ^ 2 > 0.01 then b.yaw = math.atan2(x - bx, -(z - bz)) end
    end

    -- What it is seen doing, and the gun in its hands (its model has none of
    -- its own: tools/make_front_models.py); `bare` for hands on a wheel or a
    -- machine's gun.
    function M.bot_clip(id: number, b: Bot, clip: string, bare: boolean?)
        local item = if bare then "" else `{NAME}:{b.gun}`
        if b.clip ~= clip or b.item ~= item then
            b.clip, b.item = clip, item
            core.mob_act(id, clip, item)
        end
    end

    -- The nearest enemy it can see from its eyes, within sight: in the cone
    -- in front of it, or close enough to hear, or the one it is already
    -- watching. From a machine's seat, all round.
    local function spotted(id: number, b: Bot): Spotted?
        local x, y, z = core.mob_pos(id)
        if x == nil then return nil end
        local best: Spotted?, at = nil, (M.BOTGUN[b.gun] and M.BOTGUN[b.gun].sight) or BOT_SIGHT
        if b.crew then at = 150 end   -- a jeep's gunner in the assault's escape: the giant's head, his orbs
        local sight = at
        local ripe, things_first = W.trap_ripe, W.things_first
        local fx, fz = math.sin(b.yaw or 0), -math.cos(b.yaw or 0)
        for _, t in enemies(b.team) do
            if t.empty then continue end
            local trap = t.thing and t.thing.trap
            if trap and ripe and not ripe(trap) then continue end
            local tx, ty, tz = (t.lo[1] + t.hi[1]) / 2, t.feet + 1.3, (t.lo[3] + t.hi[3]) / 2
            local dx, dy, dz = tx - x, ty - (y + EYE), tz - z
            local d = math.sqrt(dx * dx + dy * dy + dz * dz)
            -- A trap whose moment has come, and a thing when the game says
            -- things first (the assault's pylons and core), before a body.
            local rank = if trap then d * 0.3 elseif t.thing and things_first and things_first() then d * 0.4 else d
            if rank < at and d < sight then
                local key = t.player or (if t.bot then -t.bot elseif t.thing then 200000 + t.thing.n else 100000 + (t.machine or 0))
                local flat = math.sqrt(dx * dx + dz * dz)
                local ahead = flat < 0.01 or (dx * fx + dz * fz) / flat > M.CONE
                local known = b.mem and b.mem.key == key and W.clock - b.mem.t < 2
                if b.ride or d < 4 or ahead or known then
                    local _, _, _, wall = core.raycast(x, y + EYE, z, dx, dy, dz, d)
                    -- A trap's own blocks (the wheel, the rocks, the tank) lie
                    -- inside its box: they are not in the way of it.
                    if wall == nil or (trap and wall > d - (t.hi[1] - t.lo[1])) then
                        best, at = { t = t, x = tx, y = ty, z = tz, d = d, key = key } :: Spotted, rank
                    end
                end
            end
        end
        if best then
            -- Somebody new: a moment before the first round.
            if b.mem == nil or b.mem.key ~= best.key or W.clock - b.mem.t > 3 then
                b.react_at = W.clock + M.REACT[b.role] + math.random() * 0.3
            end
            b.mem = { key = best.key, x = best.x, y = best.y, z = best.z, t = W.clock }
        end
        return best
    end

    -- Somewhere near out of the line from `e` to the eyes, not nearer to it:
    -- the nearest of a ring of cells 3, 5 and 7 nodes off.
    function M.find_cover(x: number, y: number, z: number, e: { x: number, y: number, z: number }): (number?, number?)
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
    function M.cover(id: number, b: Bot, x: number, y: number, z: number): boolean
        local e = b.enemy or (if b.mem and W.clock - b.mem.t < 6 then b.mem else nil)
        if e == nil then
            b.phase, b.hold_fire = nil, false
            return false
        end
        local function go(at: {number})
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
            b.phase, b.phase_until = "hide", W.clock + 1.2 + math.random() * 0.8
            go(b.cover :: {number})
        elseif W.clock >= (b.phase_until :: number) then
            if b.phase == "hide" then
                b.phase, b.phase_until = "peek", W.clock + 1.8 + math.random()
                go(b.peek :: {number})
            else
                b.phase, b.phase_until = "hide", W.clock + 1.5 + math.random()
                go(b.cover :: {number})
            end
        end
        b.hold_fire = b.phase == "hide"
        return true
    end

    local function bot_shoot(id: number, b: Bot)
        local e = b.enemy
        local x, y, z = core.mob_pos(id)
        if x == nil or e == nil then return end
        local g, bg = GUNS[b.gun], M.BOTGUN[b.gun]
        local o = { x, y + EYE, z }
        local dx, dy, dz = e.x - o[1], e.y - o[2], e.z - o[3]
        local l = math.sqrt(dx * dx + dy * dy + dz * dz)
        -- Twice as far off at first, settling over a second and a half.
        local settle = 1 + math.max(0, 1 - (W.clock - (b.react_at or 0)) / 1.5)
        local spread = (bg.spread or g.spread) * (1 + l / 40) * settle
        if g.rocket then
            -- Raised for the rocket's drop on the way (45 m/s, g 2).
            local t = l / 45
            local d = jitter({ dx / l, (dy + t * t) / l, dz / l }, spread)
            M.shoot("rocket", { o[1] + d[1] * 0.8, o[2] - 0.1 + d[2] * 0.8, o[3] + d[3] * 0.8 },
                    { d[1] * 45, d[2] * 45, d[3] * 45 }, -id, b.team)
            core.play_sound(`{NAME}:rocket`, o[1], o[2], o[3])
            return
        end
        local d = jitter({ dx / l, dy / l, dz / l }, spread)
        bullet(-id, b, o, d, { range = g.range, damage = g.damage * M.BOT_HURTS }, b.gun)
        core.play_sound(`{NAME}:{b.gun}`, o[1], o[2], o[3])
    end

    -- The trigger: a burst, a pause; a reload when the magazine is out, the
    -- next gun it likes when all of this one's rounds are.
    function M.bot_trigger(id: number, b: Bot)
        if W.clock < (b.react_at or 0) or b.hold_fire then return end
        local a = b.ammo[b.gun]
        if b.reload_at then
            if W.clock < b.reload_at then return end
            local n = math.min(GUNS[b.gun].mag - a.mag, a.res :: number)
            a.mag += n
            a.res -= n
            b.reload_at = nil
        end
        if W.clock < b.shot_at then return end
        if a.mag <= 0 then
            if a.res > 0 then
                b.reload_at = W.clock + GUNS[b.gun].reload
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
            b.shot_at = W.clock + bg.pause[1] + math.random() * (bg.pause[2] - bg.pause[1])
        else
            b.shot_at = W.clock + bg.every
        end
        bot_shoot(id, b)
    end

    local function bot_throw(id: number, b: Bot)
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
    --
    -- Two are the game's, put on M by it: M.start, its fleet once somebody
    -- is there to see it (M.PARK and how it is laid out are its own), and
    -- M.bot_drive, a bot at the wheel -- across the front's open field, or
    -- along the assault's valley feeling for the way round what is in it.

    M.MODEL = {}
    for _, n in { "jeep", "tank_hull", "tank_turret", "tank_gun", "heli", "plane", "mg", "mg_tripod",
                  "mortar_base", "mortar_tube", "howitzer", "howitzer_barrel", "shell", "rocket", "bomb",
                  "jeep_wreck", "tank_wreck", "heli_wreck", "plane_wreck",
                  "bomber", "bomber_wreck", "aa_base", "aa_launcher", "missile" } do
        M.MODEL[n] = core.register_model(`{NAME}:{n}`, `{n}.gltf`)
    end
    M.THING = `{NAME}:machine`
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
    M.SOUND = { wheeled = { `{NAME}:engine_jeep`, 0.55 }, tracked = { `{NAME}:engine_tank`, 0.55 },
                rotor = { `{NAME}:rotor`, 0.55 }, wing = { `{NAME}:engine_plane`, 0.38 } }

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

    function M.spawn(kind: string, x: number, z: number, yaw: number, team: number): number?
        local k = M.KINDS[kind]
        local y = feet_at(math.floor(x), math.floor(z))
        local id = core.ecs.spawn(M.THING, x, y, z)
        if id == nil then return nil end
        core.ecs.visible(id, true)
        core.ecs.model(id, M.MODEL[k.model])
        core.ecs.set(id, "yaw", yaw)
        if k.craft then
            core.craft.make(id, k.craft, k.mass :: number, k.hw, k.hh :: number, k.hl, k.accel :: number, k.top :: number,
                            k.turn :: number)
        end
        -- As strong as the game says (the assault's allies: what has been bought).
        local hp = if W.hull then W.hull(team, k.hp) else k.hp
        local m: Machine = { kind = kind, k = k, parts = {}, riders = {}, bots = {}, hp = hp, max = hp, team = team, aim = {},
                             next = {}, home = { x, z, yaw, team }, salvo = 8 }
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
            then string.format(" %.0f %s %g %g %g %g %g %g %g", id, k.craft, k.mass :: number, k.hw, k.hh :: number, k.hl,
                               k.accel :: number, k.top :: number, k.turn :: number)
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
        local order: { {number} } = {}
        for i in m.k.seats do
            local sx, _, sz = M.seat_pos(best, m, i)
            table.insert(order, { i, (sx - px) ^ 2 + (sz - pz) ^ 2 })
        end
        table.sort(order, function(a: {number}, b: {number}): boolean return a[2] < b[2] end)
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
                if v then W.player_down(p, v) end
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
        if m.again then table.insert(M.respawn, { kind = m.kind, home = m.home, at = W.clock + 30, again = true }) end
        core.log(`{NAME}: {m.kind} destroyed by {name} ({weapon})`)
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
    function M.flying(m: Machine?): boolean
        return m ~= nil and not m.dead and m.pose ~= nil and (m.k.craft == "rotor" or m.k.craft == "wing")
            and m.pose.y - GROUND > 3
    end

    function M.step_shots(dt: number)
        for id, sh in M.shots do
            local x, y, z = core.ecs.get(id, "x"), core.ecs.get(id, "y") :: number, core.ecs.get(id, "z") :: number
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
            if tm and not tm.dead then
                local tx, ty, tz = M.where(sh.homing)
                local dx, dy, dz = tx - x, ty + 1 - y, tz - z
                local l = math.sqrt(dx * dx + dy * dy + dz * dz)
                if l < 3 then
                    M.shots[id] = nil
                    core.ecs.kill(id)
                    hurt(sh.by, { machine = sh.homing } :: Target, spec.direct, sh.kind, x, z)
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
            if wall ~= nil or target ~= nil or sh.t > (if spec.air then 7 else 12) or y < W.SHOT_FLOOR then
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
        if s == nil or s.ray == nil or W.clock - s.ray[7] >= 1 then return nil end
        if s.aim and s.aim[4] == W.clock then return s.aim end
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
        s.aim = { o[1] + d[1] * at, o[2] + d[2] * at, o[3] + d[3] * at, W.clock }
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

    function M.fire(p: number, s: Player, second: boolean)
        local cur = M.seat_of[p]
        local m = cur and M.list[cur.id]
        if m == nil then return end
        local seat = m.k.seats[cur.seat]
        local gun = seat.gun
        if gun == nil or W.clock < (m.next[cur.seat] or 0) then return end
        local dx, dy, dz = core.player_look(p)
        if dx == nil then return end

        if gun == "mg" then
            m.next[cur.seat] = W.clock + 0.08
            local pid = m.parts[seat.part :: number]
            local px, py, pz = core.ecs.get(pid, "x"), core.ecs.get(pid, "y") :: number, core.ecs.get(pid, "z") :: number
            if px == nil then return end
            local yaw, pitch = M.want(p, { px, py, pz })
            local cp = math.cos(pitch)
            dx, dy, dz = math.sin(yaw) * cp, math.sin(pitch), -math.cos(yaw) * cp
            bullet(p, s, { px + dx * 1.2, py + 0.05 + dy * 1.2, pz + dz * 1.2 }, jitter({ dx, dy, dz }, 0.02),
                   { range = 170, damage = 5 }, "mg", cur.id)
            core.play_sound(`{NAME}:mg`, px, py, pz)
        elseif gun == "cannon" then
            m.next[cur.seat] = W.clock + 3
            M.cannon(cur.id, m, p, s.team)
            tell(p, "fired cannon")
        elseif gun == "rockets" then
            m.salvo -= 1
            m.next[cur.seat] = W.clock + (if m.salvo <= 0 then 5 else 0.3)
            if m.salvo <= 0 then m.salvo = 8 end
            local x, y, z = M.point(cur.id, if m.salvo % 2 == 0 then 1.15 else -1.15, 0.75, 1.8)
            local yaw, pitch = M.want(p, { x, y, z })
            local cp = math.cos(pitch)
            dx, dy, dz = math.sin(yaw) * cp, math.sin(pitch), -math.cos(yaw) * cp
            M.shoot("heli", { x, y, z }, { dx * 55, dy * 55, dz * 55 }, p, s.team, cur.id)
            core.play_sound(`{NAME}:rocket`, x, y, z)
        elseif gun == "plane" then
            local pose = m.pose
            if pose == nil then return end
            if second then
                m.next[cur.seat] = W.clock + 3
                local x, y, z = M.point(cur.id, 0, -0.3, 0)
                M.shoot("bomb", { x, y, z }, { pose.vx, pose.vy, pose.vz }, p, s.team, cur.id)
                return
            end
            m.next[cur.seat] = W.clock + 0.07
            local cp = math.cos(pose.pitch)
            local d = { math.sin(pose.yaw) * cp, math.sin(pose.pitch), -math.cos(pose.yaw) * cp }
            m.side = -(m.side or 1)
            local x, y, z = M.point(cur.id, 2.2 * (m.side :: number), 0.5, 3.5)
            bullet(p, s, { x + d[1] * (pose.speed * 0.1), y + d[2] * (pose.speed * 0.1), z + d[3] * (pose.speed * 0.1) },
                   jitter(d, 0.012), { range = 180, damage = 4 }, "plane", cur.id)
            core.play_sound(`{NAME}:mg`, x, y, z)
        elseif gun == "bomber" then
            if second or m.pose == nil then return end
            m.next[cur.seat] = W.clock + 9
            m.stick = { left = 6, at = W.clock, by = p, team = s.team }
            tell(p, "fired bomber")
        elseif gun == "aa" then
            if (m.lock_t or 0) < M.AA_LOCK then return end
            M.aa_fire(cur.id, m, p, s.team)
            tell(p, "fired aa")
        elseif gun == "mortar" or gun == "howitzer" then
            m.next[cur.seat] = W.clock + M.LOB[gun].cool
            local tx, ty, tz = M.target(p, M.LOB[gun].range)
            if tx ~= nil and M.lob(cur.id, m, cur.seat, tx, ty, tz, p, s.team) then tell(p, `fired {gun}`) end
        end
    end

    -- Where a bomb let go now would come down (22 down, the machine's speed on).
    function M.impact(m: Machine): (number, number, number)
        local pose = m.pose :: CraftPose
        local alt = math.max(0, pose.y - GROUND)
        local t = (pose.vy + math.sqrt(math.max(0, pose.vy * pose.vy + 44 * alt))) / 22
        return pose.x + pose.vx * t, GROUND, pose.z + pose.vz * t
    end

    -- The bomb bay: one of the stick every 0.22 s, out of the middle.
    function M.drop_stick(id: number, m: Machine)
        local st = m.stick
        while st and st.left > 0 and W.clock >= st.at and m.pose do
            st.left -= 1
            st.at += 0.22
            local x, y, z = M.point(id, 0, 0.4, 0.4)
            M.shoot("bomb", { x, y, z }, { m.pose.vx, m.pose.vy, m.pose.vz }, st.by, st.team, id)
        end
        if st and st.left <= 0 then m.stick = nil end
    end

    -- Where the anti-air's tubes point.
    function M.aa_dir(id: number, m: Machine): ({number}?, {number}?)
        local pid = m.parts[1]
        local px, py, pz = core.ecs.get(pid, "x"), core.ecs.get(pid, "y") :: number, core.ecs.get(pid, "z") :: number
        if px == nil then return nil, nil end
        local a = m.aim[1]
        local yaw = (core.ecs.get(id, "yaw") or 0) + a.yaw
        local cp = math.cos(a.pitch)
        return { px, py, pz }, { math.sin(yaw) * cp, math.sin(a.pitch), -math.cos(yaw) * cp }
    end

    -- Holding a lock: the enemy machine in the air nearest the tubes' line,
    -- within AA_CONE, in sight; the time it has been the same one.
    function M.aa_lock(id: number, m: Machine, dt: number)
        local o, f = M.aa_dir(id, m)
        if o == nil then return end
        local best, score = nil, M.AA_CONE
        for _, t in enemies(m.team) do
            local tm = t.machine and M.list[t.machine]
            if not M.flying(tm) then continue end
            local tpose = (tm :: Machine).pose :: CraftPose
            local dx, dy, dz = tpose.x - o[1], tpose.y + 1 - o[2], tpose.z - o[3]
            local l = math.sqrt(dx * dx + dy * dy + dz * dz)
            if l > M.AA_RANGE then continue end
            local ang = math.acos(math.clamp((dx * f[1] + dy * f[2] + dz * f[3]) / l, -1, 1))
            if ang < score then
                local _, _, _, wall = core.raycast(o[1], o[2], o[3], dx, dy, dz, l - 2)
                if wall == nil then best, score = t.machine, ang end
            end
        end
        if best ~= nil and best == m.lock_on then
            m.lock_t = (m.lock_t :: number) + dt
        else
            m.lock_on, m.lock_t = best, 0
        end
        local p = m.riders[1]
        if p and W.clock >= (m.lock_said or 0) then
            m.lock_said = W.clock + 0.1
            local tm = best and M.list[best]
            if tm then
                tell(p, string.format("lock %d %.2f %.1f %.1f %.1f", if (m.lock_t :: number) >= M.AA_LOCK then 1 else 0,
                                      math.min(1, (m.lock_t :: number) / M.AA_LOCK), (tm.pose :: CraftPose).x,
                                      (tm.pose :: CraftPose).y + 1, (tm.pose :: CraftPose).z))
            else
                tell(p, "lock none")
            end
        end
    end

    -- The bazooka's lock, aimed down its sight: the enemy vehicle nearest the
    -- crosshair, within AA_CONE, in sight, held for AA_LOCK. Locked, the next
    -- rocket goes after it (fire, M.shoot).
    function M.launcher_lock(p: number, s: Player, dt: number)
        local loaded = s.aiming and not M.seat_of[p] and short(core.held_item(p)) == "launcher"
            and s.reload_at == 0 and s.mag.launcher > 0
        local x, y, z = core.player_pos(p)
        local lx, ly, lz = core.player_look(p)
        local best, score = nil, M.AA_CONE
        if loaded and x ~= nil and lx ~= nil then
            local o = { x, y + EYE, z }
            local fx, fy, fz = M.toward(p, o, lx, ly, lz)
            for _, t in enemies(s.team) do
                local tm = t.machine and M.list[t.machine]
                if tm == nil or tm.k.craft == nil then continue end   -- a vehicle, not a gun on legs
                local tx, ty, tz = M.where(t.machine)
                local dx, dy, dz = tx - o[1], ty + 1 - o[2], tz - o[3]
                local l = math.sqrt(dx * dx + dy * dy + dz * dz)
                if l > M.AA_RANGE or l < 3 then continue end
                local ang = math.acos(math.clamp((dx * fx + dy * fy + dz * fz) / l, -1, 1))
                if ang < score then
                    local _, _, _, wall = core.raycast(o[1], o[2], o[3], dx / l, dy / l, dz / l, l - 2)
                    if wall == nil then best, score = t.machine, ang end
                end
            end
        end
        if best ~= nil and best == s.lock_on then
            s.lock_t = (s.lock_t :: number) + dt
        else
            s.lock_on, s.lock_t = best, 0
        end
        if W.clock < (s.lock_said or 0) or (best == nil and not s.locking) then return end
        s.lock_said, s.locking = W.clock + 0.1, best ~= nil
        if best == nil then
            tell(p, "lock none")
        else
            local tx, ty, tz = M.where(best)
            tell(p, string.format("lock %d %.2f %.1f %.1f %.1f", if (s.lock_t :: number) >= M.AA_LOCK then 1 else 0,
                                  math.min(1, (s.lock_t :: number) / M.AA_LOCK), tx, ty + 1, tz))
        end
    end

    -- A missile out of the next tube, after what is locked: four, 1.2 s
    -- apart, then 8 s to load again.
    function M.aa_fire(id: number, m: Machine, by: number, team: number)
        if W.clock < (m.next[1] or 0) or m.lock_on == nil then return end
        local o, f = M.aa_dir(id, m)
        if o == nil then return end
        m.aa_left = (m.aa_left or 4) - 1
        m.next[1] = W.clock + (if (m.aa_left :: number) <= 0 then 8 else 1.2)
        if (m.aa_left :: number) <= 0 then m.aa_left = 4 end
        M.shoot("missile", { o[1] + f[1] * 1.4, o[2] + f[2] * 1.4 + 0.3, o[3] + f[3] * 1.4 },
                { f[1] * 30, f[2] * 30, f[3] * 30 }, by, team, id, m.lock_on)
        core.play_sound(`{NAME}:rocket`, o[1], o[2], o[3])
    end

    -- A shell from seat i's lobbing gun onto tx, ty, tz.
    function M.lob(id: number, m: Machine, i: number, tx: number, ty: number, tz: number, by: number, team: number): boolean
        local seat = m.k.seats[i]
        local pid = m.parts[seat.part :: number]
        local ox, oy, oz = core.ecs.get(pid, "x"), core.ecs.get(pid, "y") :: number, core.ecs.get(pid, "z") :: number
        if ox == nil then return false end
        oy += 1.0
        -- At the gun's angle the speed that comes down on the target:
        -- v^2 = g D^2 / (2 cos^2 a (D tan a - H)), a little off each time.
        local a = m.k.parts[seat.part :: number].lob :: number
        local hx, hz = tx - ox, tz - oz
        local D = math.sqrt(hx * hx + hz * hz) * (1 + (math.random() - 0.5) * 0.06)
        local H = ty - oy
        local under = 2 * math.cos(a) ^ 2 * (D * math.tan(a) - H)
        if D < 3 or under <= 0 then return false end
        local v = math.sqrt(22 * D * D / under)
        local l = math.sqrt(hx * hx + hz * hz)
        M.shoot(seat.gun, { ox, oy, oz }, { hx / l * v * math.cos(a), v * math.sin(a), hz / l * v * math.cos(a) },
                by, team, id)
        core.play_sound(`{NAME}:mortar`, ox, oy, oz)
        return true
    end

    -- The tank's gun, along where its turret and barrel point.
    function M.cannon(id: number, m: Machine, by: number, team: number)
        local a, b = m.aim[1], m.aim[2]
        local yaw = (m.pose and m.pose.yaw or 0) + a.yaw
        local pitch = b.pitch + (m.pose and m.pose.pitch or 0)
        local d = { math.sin(yaw) * math.cos(pitch), math.sin(pitch), -math.cos(yaw) * math.cos(pitch) }
        local pid = m.parts[2]
        local px, py, pz = core.ecs.get(pid, "x"), core.ecs.get(pid, "y") :: number, core.ecs.get(pid, "z") :: number
        if px == nil then return end
        M.shoot("cannon", { px + d[1] * 3.8, py + d[2] * 3.8, pz + d[3] * 3.8 }, { d[1] * 70, d[2] * 70, d[3] * 70 },
                by, team, id)
        core.play_sound(`{NAME}:cannon`, px, py, pz)
    end

    -- The driver's keys to the machine: on the ground as they are; in the air
    -- the mouse steers -- a helicopter turns to the look, a plane banks and
    -- pitches towards it, W and S its throttle.
    function M.drive(id: number, m: Machine, p: number)
        local s = players[p]
        local k = s and s.keys or { 0, 0, 0 }
        local craft, pose = m.k.craft, m.pose
        if craft == "wheeled" or craft == "tracked" or pose == nil then
            m.want = k[1]
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
    function M.aim(id: number, m: Machine, dt: number)
        local hull_yaw = if m.pose then m.pose.yaw else (core.ecs.get(id, "yaw") or 0)
        for i, part in m.k.parts do
            local p = m.riders[part.by]
            local pid = m.parts[i]
            -- A bot at it aims at what it sees, and holds still when it sees nothing.
            local b = if p == nil and m.bots[part.by] then bots[m.bots[part.by]] else nil
            local tp = if b == nil then nil elseif part.lob then b.lob
                elseif b.enemy then { b.enemy.x, b.enemy.y, b.enemy.z } else nil
            if (p == nil and tp == nil) or pid == nil then continue end
            local px, py, pz = core.ecs.get(pid, "x"), core.ecs.get(pid, "y") :: number, core.ecs.get(pid, "z") :: number
            if px == nil then continue end
            local want_yaw, want_pitch, dist
            if p then
                want_yaw, want_pitch, dist = M.want(p, { px, py, pz })
            else
                -- A bot's hand is not steady: off by a little, new after each shell.
                want_yaw, want_pitch, dist = M.want_at(tp, { px, py, pz })
                want_yaw += (b :: Bot).err_yaw or 0
                want_pitch += (b :: Bot).err_pitch or 0
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
                    tx, _, tz = M.target(p, (M.LOB[m.k.seats[part.by].gun :: string] or { range = 140 }).range)
                else
                    tx, tz = (tp :: {number})[1], (tp :: {number})[3]
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
    function M.breach(m: Machine, id: number)
        local pose = m.pose :: CraftPose
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
    function M.run_over(id: number, m: Machine)
        local pose = m.pose :: CraftPose
        local speed = math.abs(pose.speed)
        if speed < 4 then return end
        m.ran = m.ran or {}
        local s, c = math.sin(pose.yaw), math.cos(pose.yaw)
        for _, t in enemies(m.team) do
            if t.machine then continue end
            local key = t.player or -(t.bot or 0)
            if ((m.ran :: { [number]: number })[key] or 0) > W.clock then continue end
            local cx, cz = (t.lo[1] + t.hi[1]) / 2 - pose.x, (t.lo[3] + t.hi[3]) / 2 - pose.z
            local lx, lz = cx * c + cz * s, cx * s - cz * c
            if math.abs(lx) < m.k.hw + 0.4 and math.abs(lz) < m.k.hl + 0.4 and math.abs(t.feet - pose.y) < m.k.h then
                (m.ran :: { [number]: number })[key] = W.clock + 1
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
            if W.clock >= r.at then
                local x, z = math.floor(r.home[1]), math.floor(r.home[2])
                if core.loaded(x, GROUND, z) then
                    table.remove(M.respawn, i)
                    local id = M.spawn(r.kind, r.home[1], r.home[2], r.home[3], r.home[4])
                    if id then
                        M.list[id].again = r.again
                        if r.then_ then r.then_(id) end
                    end
                else
                    core.keep_loaded(x, z, 8)
                    r.at = W.clock + 1
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
                    w.gone = W.clock + 5.2
                    core.ecs.clip(w.id, "death")
                    tell(0, string.format("burn %.1f %.1f %.1f", w.x, w.y, w.z))
                end
            elseif W.clock >= w.gone then
                core.ecs.kill(w.id)
                table.remove(M.wrecks, i)
            end
        end
        local hud = W.clock >= (M.hud_at or 0)
        if hud then M.hud_at = W.clock + 0.25 end
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
                if W.in_lava and W.in_lava(pose.x, pose.y, pose.z) then
                    M.destroy(id, m.last_by or 0, "lava")
                    continue
                end
                -- Off the field, or under it (LOST_Y: under the deepest a
                -- machine may go, short of where the engine has done with a body).
                if pose.y < W.LOST_Y or pose.x < -20 or pose.z < -20 or pose.x > SIZE + 20 or pose.z > W.FAR_Z then
                    M.destroy(id, m.last_by or 0, "crash")
                    continue
                end
                -- A tank driven and not moving -- its belly on a hole's lip, a
                -- track in a crater -- is lifted and nudged the way it is
                -- driven after a second and a bit: the tracks alone could not.
                if m.k.craft == "tracked" then
                    if math.abs(m.want or 0) > 0.3 and pose.speed < 0.8 then
                        m.stuck = (m.stuck or 0) + dt
                        if (m.stuck :: number) > 1.2 then
                            m.stuck = 0
                            local d = if (m.want or 0) > 0 then 1 else -1
                            core.craft.push(id, math.sin(pose.yaw) * 3 * d, 5, -math.cos(pose.yaw) * 3 * d)
                        end
                    else
                        m.stuck = 0
                    end
                    m.want = 0
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
                if (driver or m.bots[1]) and W.clock >= (m.sound_at or 0) then
                    local snd = M.SOUND[m.k.craft]
                    m.sound_at = W.clock + snd[2]
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
                    local wait = math.max(0, (m.next[i] or 0) - W.clock)
                    tell(p, string.format("mach %d %d %.1f %.1f %.2f %.1f", math.max(0, math.floor(m.hp)), math.floor(m.max),
                                          if pose then math.abs(pose.speed) else 0, if pose then pose.y - GROUND else 0,
                                          if pose then pose.throttle else 0, wait))
                    local gun = m.k.seats[i].gun
                    if gun == "bomber" and m.pose then
                        local ix, iy, iz = M.impact(m)
                        tell(p, string.format("mark %.1f %.1f %.1f", ix, iy, iz))
                    end
                    if gun and M.LOB[gun] then
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
    -- A bot in a machine sits where this mod says, carried by the engine
    -- (core.mob_ride, M.carry) on the hull or the part its seat is on; the
    -- engine's seats are players'. Bots drive the jeep and
    -- the tank to their point, wait a moment for the others near to get in,
    -- and the jeep's let off there; they take the guns and the free seats of
    -- a machine one of their team drives and has stopped near them -- a
    -- player's too -- and get off when nobody drives it.

    -- Where seat i is now: on the hull, or on its part (a turret) as it turns.
    function M.seat_pos(id: number, m: Machine, i: number): (number, number, number)
        local seat = m.k.seats[i]
        local pid = seat.on and m.parts[seat.on]
        local px = pid and core.ecs.get(pid, "x")
        if px then
            local yaw = (if m.pose then m.pose.yaw else (core.ecs.get(id, "yaw") or 0)) + m.aim[seat.on :: number].yaw
            local sn, c = math.sin(yaw), math.cos(yaw)
            return px + c * seat[1] + sn * seat[3], (core.ecs.get(pid :: number, "y") :: number) + seat[2],
                   (core.ecs.get(pid :: number, "z") :: number) + sn * seat[1] - c * seat[3]
        end
        return M.point(id, seat[1], seat[2], seat[3])
    end

    -- The seats a bot takes: the driver's of a jeep or a tank, and any other
    -- whose gun it can fire (or that has none).
    M.BOT_DRIVES = { jeep = true, tank = true, heli = true, plane = true, bomber = true }
    M.BOT_GUNS = { mg = true, cannon = true, mortar = true, howitzer = true, rockets = true, plane = true,
                   bomber = true, aa = true }
    function M.bot_seat_ok(m: Machine, i: number): boolean
        local seat = m.k.seats[i]
        if seat == nil or m.riders[i] or m.bots[i] then return false end
        if i == 1 and m.k.craft then return M.BOT_DRIVES[m.kind] == true end
        return seat.gun == nil or M.BOT_GUNS[seat.gun] == true
    end

    -- A player of `team` within r of the machine: it is left to them.
    function M.player_near(m: Machine, team: number, r: number): boolean
        for p, s in players do
            local x, _, z = core.player_pos(p)
            if s.team == team and x and (x - (m.pose :: CraftPose).x) ^ 2 + (z - (m.pose :: CraftPose).z) ^ 2 < r * r then return true end
        end
        return false
    end

    function M.bot_board(bid: number, b: Bot, id: number, i: number): boolean
        local m = M.list[id]
        if m == nil or m.dead or not M.bot_seat_ok(m, i) then return false end
        m.bots[i] = bid
        b.ride = { id = id, seat = i }
        b.busy = W.clock
        b.phase, b.hold_fire, b.enemy = nil, false, nil
        m.team = b.team
        if i == 1 then m.wait_until = W.clock + 4 end
        return true
    end

    function M.bot_leave(bid: number, b: Bot)
        local cur = b.ride
        if cur == nil then return end
        b.ride = nil
        b.last = nil
        b.carried = nil
        core.mob_ride(bid, 0)
        b.off_until = W.clock + 10   -- not straight back in
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
    function M.bot_hop(bid: number, b: Bot, x: number, z: number): boolean
        if W.clock < (b.off_until or 0) then return false end
        local p = W.goal(bid, b)
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
                and (m.claim == nil or m.claim[1] == bid or W.clock > m.claim[2]) then
                best, at, seat = id, d, 1
            end
        end
        if best == nil then return false end
        local m = M.list[best]
        if seat == 1 and m.k.craft then m.claim = { bid, W.clock + 15 } end
        local reach = math.max(m.k.hw or 0.6, m.k.hl or 0.6) + 2
        if at < reach * reach then return M.bot_board(bid, b, best, seat) end
        local mx, _, mz = M.where(best)
        local tx, tz = math.floor(mx), math.floor(mz)
        core.mob_goto(bid, tx, feet_at(tx, tz), tz)
        return true
    end

    function M.bot_ride_think(bid: number, b: Bot)
        local cur = b.ride :: { id: number, seat: number }
        local m = M.list[cur.id]
        if m == nil or m.dead or (m.k.craft and m.pose == nil) then
            b.ride, b.carried = nil, nil
            core.mob_ride(bid, 0)
            return
        end
        local gun = m.k.seats[cur.seat].gun
        b.enemy = if gun == "aa" then M.aa_pick(bid, b) else spotted(bid, b)
        if gun and M.LOB[gun] then b.lob = M.lob_target(bid, b, M.LOB[gun].range) end
        if b.enemy or b.lob then b.busy = W.clock end
        if not m.k.craft then
            -- At a gun that stays put: off it after 40 s with nothing to shoot
            -- (but the team that holds its guns: the assault's legion at its post).
            if b.team ~= W.HOLDS_GUNS and W.clock - (b.busy or W.clock) > 40 then M.bot_leave(bid, b) end
            return
        end
        if cur.seat == 1 and b.crew and W.crew_goal then
            W.crew_goal(m)   -- the assault's escape: down the road
        elseif cur.seat == 1 then
            local p = W.goal(bid, b, true)
            m.goal_x, m.goal_z = p[1] + (b.n % 3 - 1) * 4, p[2]
            -- The jeep lets everybody off at the point; the tank stays in it.
            if m.kind == "jeep" and (p[1] - m.pose.x) ^ 2 + (p[2] - m.pose.z) ^ 2 < 20 * 20 then
                for _, other in m.bots do
                    if bots[other] then M.bot_leave(other, bots[other]) end
                end
            end
        elseif m.riders[1] == nil and m.bots[1] == nil then
            m.alone = m.alone or W.clock
            if W.clock - (m.alone :: number) > 3 then M.bot_leave(bid, b) end
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
                and ((m.pose :: CraftPose).x - x) ^ 2 + ((m.pose :: CraftPose).z - z) ^ 2 < M.AA_RANGE * M.AA_RANGE then
                return true
            end
        end
        return false
    end

    -- At the anti-air: the nearest enemy machine in the air it can see.
    function M.aa_pick(bid: number, b: Bot): Spotted?
        local x, y, z = core.mob_pos(bid)
        if x == nil then return nil end
        local best: Spotted?, at = nil, M.AA_RANGE
        for id, m in M.list do
            if m.team == b.team or m.team == 0 or not M.flying(m) then continue end
            local pose = m.pose :: CraftPose
            local dx, dy, dz = pose.x - x, pose.y + 1 - (y + EYE), pose.z - z
            local d = math.sqrt(dx * dx + dy * dy + dz * dz)
            if d < at then
                local _, _, _, wall = core.raycast(x, y + EYE, z, dx, dy, dz, d - 2)
                if wall == nil then
                    best, at = { x = pose.x, y = pose.y + 1, z = pose.z, d = d, key = 100000 + id } :: Spotted, d
                end
            end
        end
        return best
    end

    -- What the team has seen in the last 3 s that a lobbing gun at the bot
    -- reaches (20 nodes on), a few nodes off; nil when nothing.
    function M.lob_target(bid: number, b: Bot, range: number): {number}?
        local x, _, z = core.mob_pos(bid)
        if x == nil then return nil end
        local best, at = nil, range * range
        for _, e in M.seen[b.team] do
            local d = (e.x - x) ^ 2 + (e.z - z) ^ 2
            if W.clock - e.t < 3 and d > 400 and d < at then best, at = e, d end
        end
        if best == nil then return nil end
        local off = 3 + math.sqrt(at) / 40
        return { best.x + (math.random() * 2 - 1) * off, best.y - 1.3, best.z + (math.random() * 2 - 1) * off }
    end

    -- A bot at the stick. The helicopter climbs to 18 over the ground, goes
    -- to its point and stays over it, turned to what it sees. The plane
    -- runs up to speed down the way it was parked, lifts off, climbs to 45
    -- and circles over the point, 75 out; pulled up when it is low and nose
    -- down. Both drop or fire on
    -- their own; the plane's bombs go when where they would land is within
    -- 8 of something the team has seen.
    function M.bot_fly(id: number, m: Machine)
        local pose, b = m.pose :: CraftPose, bots[m.bots[1]]
        if b == nil or m.goal_x == nil or W.clock < (m.wait_until or 0) then
            core.craft.keys(id, 0, 0, if m.k.craft == "wing" then -1 else 0, 0)
            return
        end
        local ground = feet_at(math.floor(pose.x), math.floor(pose.z))
        local alt = pose.y - ground
        -- Stuck on the ground against something: everybody out.
        if alt < 2 and math.abs(pose.speed) < 1 then m.stuck = m.stuck or W.clock else m.stuck = nil end
        if m.stuck and W.clock - m.stuck > 8 then
            m.stuck = nil
            for _, other in m.bots do
                if bots[other] then M.bot_leave(other, bots[other]) end
            end
            return
        end
        if m.k.craft == "rotor" then
            local gx, gz = m.goal_x :: number, m.goal_z :: number
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
        local cx = math.clamp(m.goal_x :: number, R + 35, SIZE - R - 35)
        local cz = math.clamp(m.goal_z :: number, R + 35, SIZE - R - 35)
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
        if alt > 10 and W.clock >= (m.next[1] or 0) and W.clock >= (m.bomb_at or 0) then
            local t = (pose.vy + math.sqrt(math.max(0, pose.vy * pose.vy + 44 * alt))) / 22
            local ix, iz = pose.x + pose.vx * t, pose.z + pose.vz * t
            for _, e in M.seen[b.team] do
                if W.clock - e.t < 4 and (e.x - ix) ^ 2 + (e.z - iz) ^ 2 < (if big then 144 else 64) then
                    if big then
                        -- The stick starts short of them and walks across.
                        m.bomb_at = W.clock + 9
                        m.stick = { left = 6, at = W.clock, by = -m.bots[1], team = b.team }
                    else
                        m.bomb_at = W.clock + 3
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
    function M.carry(id: number, m: Machine)
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
                    core.log(string.format("%s: a %s off its seat on a %s (%.1f away), carried again", NAME,
                                           b.role, m.kind, math.sqrt((mx - x) ^ 2 + (my - y) ^ 2 + (mz - z) ^ 2)))
                    b.carried = nil
                end
            end
            if b.carried ~= id then
                b.carried = id
                local carrier = if seat.on then m.parts[seat.on] else id
                if not core.mob_ride(bid, carrier, seat[1], seat[2], seat[3] - (if seat.stand then 0 else 0.27)) then
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
            -- A passenger shoots its own gun from its seat, where passengers fight.
            if W.PASSENGERS_ARMED and seat.gun == nil and i ~= 1 and e then M.bot_trigger(bid, b) end
            if seat.gun == nil or W.clock < (m.next[i] or 0) then continue end
            if M.LOB[seat.gun] then
                if b.lob then
                    m.next[i] = W.clock + M.LOB[seat.gun].cool + 2 + math.random() * 2
                    M.lob(id, m, i, b.lob[1], b.lob[2], b.lob[3], -bid, b.team)
                end
                continue
            end
            if seat.gun == "aa" then
                if e and (m.lock_t or 0) >= M.AA_LOCK then M.aa_fire(id, m, -bid, b.team) end
                continue
            end
            if e == nil or W.clock < (b.react_at or 0) then continue end
            if seat.gun == "mg" then
                local gun_pid = m.parts[seat.part :: number]
                local px, py, pz = core.ecs.get(gun_pid, "x"), core.ecs.get(gun_pid, "y") :: number, core.ecs.get(gun_pid, "z") :: number
                if px == nil then continue end
                b.burst -= 1
                if b.burst <= 0 then
                    b.burst = math.random(5, 9)
                    m.next[i] = W.clock + 0.8 + math.random() * 0.4
                else
                    m.next[i] = W.clock + 0.12
                end
                local dx, dy, dz = e.x - px, e.y - py, e.z - pz
                local l = math.sqrt(dx * dx + dy * dy + dz * dz)
                local d = jitter({ dx / l, dy / l, dz / l }, 0.04 * (1 + l / 40))
                bullet(-bid, b, { px + d[1] * 1.2, py + 0.05 + d[2] * 1.2, pz + d[3] * 1.2 }, d,
                       { range = 170, damage = 4 }, "mg", id)
                core.play_sound(`{NAME}:mg`, px, py, pz)
            elseif seat.gun == "rockets" and m.pose then
                -- At what is in front of the nose, a pod at a time.
                local fx, fz = math.sin(m.pose.yaw), -math.cos(m.pose.yaw)
                local dx, dy, dz = e.x - m.pose.x, e.y - m.pose.y, e.z - m.pose.z
                local l = math.sqrt(dx * dx + dy * dy + dz * dz)
                if l < 120 and (dx * fx + dz * fz) / math.max(0.01, math.sqrt(dx * dx + dz * dz)) > 0.85 then
                    m.salvo -= 1
                    m.next[i] = W.clock + (if m.salvo <= 0 then 6 else 0.35)
                    if m.salvo <= 0 then m.salvo = 8 end
                    local rx, ry, rz = M.point(id, if m.salvo % 2 == 0 then 1.15 else -1.15, 0.75, 1.8)
                    local d = jitter({ dx / l, dy / l, dz / l }, 0.03)
                    M.shoot("heli", { rx, ry, rz }, { d[1] * 55, d[2] * 55, d[3] * 55 }, -bid, b.team, id)
                    core.play_sound(`{NAME}:rocket`, rx, ry, rz)
                end
            elseif seat.gun == "plane" and m.pose then
                -- The wing guns, along the nose, when it points at them.
                local pose = m.pose
                local cp = math.cos(pose.pitch)
                local d = { math.sin(pose.yaw) * cp, math.sin(pose.pitch), -math.cos(pose.yaw) * cp }
                local dx, dy, dz = e.x - pose.x, e.y - pose.y, e.z - pose.z
                local l = math.sqrt(dx * dx + dy * dy + dz * dz)
                if l < 130 and (dx * d[1] + dy * d[2] + dz * d[3]) / l > 0.985 then
                    m.next[i] = W.clock + 0.09
                    m.side = -(m.side or 1)
                    local wx, wy, wz = M.point(id, 2.2 * (m.side :: number), 0.5, 3.5)
                    bullet(-bid, b, { wx + d[1] * pose.speed * 0.1, wy + d[2] * pose.speed * 0.1, wz + d[3] * pose.speed * 0.1 },
                           jitter(d, 0.015), { range = 180, damage = 3 }, "plane", id)
                    core.play_sound(`{NAME}:mg`, wx, wy, wz)
                end
            elseif seat.gun == "cannon" and m.pose then
                local turret_pid = m.parts[2]
                local px, pz = core.ecs.get(turret_pid, "x"), core.ecs.get(turret_pid, "z") :: number
                if px == nil then continue end
                local off = wrap(math.atan2(e.x - px, -(e.z - pz)) - (m.pose :: CraftPose).yaw - m.aim[1].yaw)
                if math.abs(off) < 0.08 then
                    m.next[i] = W.clock + 5.5 + math.random() * 1.5
                    M.cannon(id, m, -bid, b.team)
                    b.err_yaw, b.err_pitch = (math.random() - 0.5) * 0.07, (math.random() - 0.5) * 0.03
                end
            end
        end
    end

    -- What the game's own code calls.
    W.asked, W.ammo_line, W.refill, W.who, W.killed = asked, ammo_line, refill, who, killed
    W.hurt, W.enemies, W.explode, W.spawn_nade, W.throw = hurt, enemies, explode, spawn_nade, throw
    W.step_nades, W.jitter, W.bullet, W.fire, W.feet_at = step_nades, jitter, bullet, fire, feet_at
    W.spotted, W.bot_throw = spotted, bot_throw
    return W
end

return battle
