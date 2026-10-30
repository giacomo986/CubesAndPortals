-- yafps_game: a base in the manner of modern arena shooters. Five guns -- a pistol,
-- a pump shotgun, the super shotgun, a chaingun and the plasma cannon -- and waves of
-- demons that come through the gates in the floors of yafps_core's rooms:
-- ghouls with rifles, imps that throw fire, pinkies that charge, gazers
-- that float and spit. Ammo and medkits wait about the rooms, a kill leaves a
-- health orb, and the bigger guns turn up as the waves go on. Every model is
-- from Blender (tools/make_fps_models.py): in first person the robot's own
-- arms hold the guns.
--
-- The SERVER decides every shot. The half on the player's machine says when the
-- trigger goes down and up and nothing else: where from and which way are the
-- server's own (core.player_pos, core.player_look), which gun is what the
-- server knows is in the hand (core.held_item), and what a shot meets is
-- core.shoot. A client that lies can only lie about when it pulled.
--
-- `side = both`: the server runs the first half, a player's machine the
-- second, and a process that is its own server runs both and calls across
-- instead of sending.

local EYE    = 1.5    -- the robot's visor, as yafps_core's core.eye_height says
local GROUND = 20     -- yafps_core's arena floor is one below this
local CENTRE = 64.5   -- the middle of the arena, in the middle of its cell

-- The guns. `every` seconds between shots while the trigger is held; each shot
-- is `pellets` bullets of `damage`, spread by up to `spread` either way, and
-- spends `cost` of `ammo` (none for the pistol, as in modern arena shooters).
local GUNS = {
    pistol   = { every = 0.28, pellets = 1, damage = 6, spread = 0.008, ammo = nil, cost = 0, range = 80 },
    shotgun  = { every = 0.85, pellets = 9, damage = 4, spread = 0.07, ammo = "shells", cost = 1, range = 40 },
    ssg      = { every = 1.4, pellets = 20, damage = 4, spread = 0.13, ammo = "shells", cost = 2, range = 30 },
    chaingun = { every = 1 / 12, pellets = 1, damage = 4, spread = 0.03, ammo = "bullets", cost = 1, range = 80 },
    plasma      = { every = 2.2, pellets = 0, damage = 0, spread = 0, ammo = "cells", cost = 40, range = 0 },
}
local MAX_AMMO = { bullets = 300, shells = 50, cells = 300 }

-- The PLASMA: the ball leaves 0.6 s after the trigger (the gun's charge clip),
-- flies at BALL_SPEED, bursts on the first wall or demon it meets, and then,
-- as the original's forty tracers, every demon in front of whoever fired it
-- is struck again.
local PLASMA_DELAY  = 0.6
local BALL_SPEED = 18
local BALL_LIFE  = 4
local BLAST_R    = 7
local BLAST_DMG  = 140
local RAYS       = 32
local RAY_DMG    = 16

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local clock = 0

local function short(item: string): string
    return (string.gsub(item, "^yafps_game:", ""))
end

-- Filled in below by whichever halves this process runs.
local trigger: (number, boolean) -> () = function(_, _) end
local heard: (string) -> ()            = function(_) end

-- What the server tells one player, or everybody (player 0). Sent when there
-- is a network between them, handed straight to the drawing half when this
-- process is both.
local function tell(player: number, text: string)
    if drawing then
        heard(text)
    else
        core.net.send(player, text)
    end
end

-- ===================================================================== server

if serving then
    core.net.open()

    -- The trial for the Chiave dell'Assedio (mods/keys): seven waves, then the
    -- Guardian of the reactor (tools/make_fps_boss.py); its death gives the key.
    local LAST_WAVE = 7
    -- Come in by a survival site (mods/keys' keys:level), the base is that
    -- site's level: 3 + d / 2 waves and 10% more demons in each a step of d,
    -- the same base and the same Guardian. The first to come into an empty
    -- base sets it; empty again, it is the trial's seven.
    local last_wave, more = LAST_WAVE, 1.0
    -- And 15% more health a step of d (core.mob_health), from register_mob's.
    local sturdier = 1.0
    local HP = { ghoul = 14, imp = 26, pinky = 70, gazer = 130, guardian = 700 }
    for name in GUNS do
        core.register_item(`yafps_game:{name}`, 1, "", 0, 1.0, "")
        core.register_model(`yafps_game:{name}_held`, `{name}_held.gltf`)
    end

    local model = {}
    for _, n in { "imp", "pinky", "gazer", "ghoul", "fireball", "caco_ball", "bullet",
                  "plasma_ball", "ammo_bullets", "ammo_shells", "ammo_cells", "medkit", "health_orb" } do
        model[n] = core.register_model(`yafps_game:{n}`, `{n}.gltf`)
    end

    -- The demons. What they do is the engine's (core/ecs/mob.h); this says
    -- what each kind is. `spawn_on` names a block no world has, so none of them
    -- ever comes out of the dark by itself: only through a gate, in a wave.
    local NEVER = "yafps_game:never"
    -- `fire` makes a shot fly straight and slower, not drop like an arrow: the
    -- ghouls's too, bolts rather than bullets, as in modern arena shooters.
    --                  name                     model              hp   speed fight    dmg reach every drop n shot                 doors hover fire  spawn_on
    core.register_mob("yafps_game:ghoul", model.ghoul, 14,  2.6, "archer", 1, 18,  1.8, "", 1, model.bullet,    false, 0,    true,  NEVER)
    core.register_mob("yafps_game:imp",       model.imp,       26,  3.4, "archer", 2, 16,  2.2, "", 1, model.fireball,  false, 0,    true,  NEVER)
    core.register_mob("yafps_game:pinky",     model.pinky,     70,  6.5, "melee",  3, 2.0, 0.9, "", 1, 0,               false, 0,    false, NEVER)
    core.register_mob("yafps_game:gazer", model.gazer, 130, 2.2, "archer", 3, 22,  3.0, "", 1, model.caco_ball, false, 3.5,  true,  NEVER)
    model.guardian = core.register_model("yafps_game:guardian", "guardian.gltf")
    model.guardian_ball = core.register_model("yafps_game:guardian_ball", "guardian_ball.gltf")
    core.register_mob("yafps_game:guardian",  model.guardian,  700, 2.6, "archer", 5, 24,  1.6, "", 1, model.guardian_ball, false, 0, true, NEVER)
    for _, n in { "ghoul", "imp", "pinky", "gazer", "guardian" } do
        core.mob_sight(`yafps_game:{n}`, 64)   -- the whole arena, and then some
    end

    -- Where the demons come through (yafps_core's gates) and where things lie,
    -- relative to its middle and floor (y up from the floor; the ledges and
    -- catwalks are 5 up, the reactor's stair 10 at the top).
    local GATES = { {10, 3}, {-6, 3}, {0, 32}, {34, 7}, {22, -7}, {-24, 6}, {-31, -6} }
    local PADS = {
        { what = "ammo_bullets", x = 10, z = -10 },        { what = "ammo_shells", x = -8, z = -10 },
        { what = "medkit", x = 0, y = 5, z = 13 },          { what = "ammo_cells", x = -13, y = 5, z = 0 },
        { what = "ammo_bullets", x = -22, z = -8 },        { what = "ammo_shells", x = -22, z = 8 },
        { what = "medkit", x = -33, z = 8 },
        { what = "ammo_shells", x = -10, y = 5, z = 34 },  { what = "ammo_bullets", x = 10, y = 5, z = 22 },
        { what = "medkit", x = 6, z = 34 },
        { what = "ammo_cells", x = 34, z = -7 },           { what = "medkit", x = 22, z = 7 },
    }
    -- The bigger guns, where and from which wave: the super shotgun on the
    -- foundry's bridge, the chaingun on the hub's platform, the plasma cannon at the top of
    -- the reactor's stair.
    local GUN_AT = {
        { gun = "ssg", wave = 1, x = 0, y = 5, z = 28 },
        { gun = "chaingun", wave = 2, x = 0, y = 2, z = 0 },
        { gun = "plasma", wave = 4, x = 25, y = 10, z = 3 },
    }
    local GIVES = {
        ammo_bullets = { ammo = "bullets", n = 60 }, ammo_shells = { ammo = "shells", n = 12 },
        ammo_cells = { ammo = "cells", n = 80 }, medkit = { heal = 8 }, health_orb = { heal = 2 },
    }

    -- Things on the floor and balls in flight are the mod's own entities, seen
    -- by everybody (core.ecs.visible) wearing their model.
    local PICKUP = "yafps_game:pickup"
    local BALL   = "yafps_game:ball"
    core.ecs.component(PICKUP, { "pad" })
    core.ecs.component(BALL, { "vx", "vy", "vz", "owner", "t" })

    local pickups = {}   -- id -> { what = name, pad = index or nil, gun = name or nil }
    local balls   = {}   -- id -> true
    local players = {}   -- player id -> state, below
    local armed   = {}   -- player name -> true once given the guns

    local function spawn_pickup(what: string, x: number, y: number, z: number, pad: number?, gun: string?): boolean
        local id = core.ecs.spawn(PICKUP, x, y, z)
        if id == nil then
            return false   -- no session table yet: entities need a listening server
        end
        core.ecs.visible(id, true)
        core.ecs.model(id, if gun then core.model_id(`yafps_game:{gun}_held`) else model[what])
        pickups[id] = { what = what, pad = pad, gun = gun }
        return true
    end

    local function ammo_line(s): string
        return `ammo {s.ammo.bullets} {s.ammo.shells} {s.ammo.cells} {s.kills}`
    end

    local function add_ammo(s, kind: string, n: number)
        s.ammo[kind] = math.min(MAX_AMMO[kind], s.ammo[kind] + n)
    end

    -- ---------------------------------------------------------------- waves

    local wave      = 0
    local state     = "rest"     -- rest: counting down to a wave; fight: a wave is on
    local next_at   = 0          -- when the wave starts, or the next of it comes through
    local to_spawn  = {}         -- kinds still to come through this wave
    local left_said = -1
    local gun_out   = { ssg = false, chaingun = false, plasma = false }
    local TITLE     = { ssg = "THE SUPER SHOTGUN", chaingun = "THE CHAINGUN", plasma = "THE plasma cannon" }

    local function alive(): number
        return core.monsters_near(CENTRE, GROUND, CENTRE, 70)
    end

    local function start_wave()
        wave += 1
        to_spawn = {}
        local function add(kind: string, n: number)
            for _ = 1, n do
                table.insert(to_spawn, kind)
            end
        end
        add("ghoul", math.ceil(math.min(2 + wave, 8) * more))
        add("imp", math.ceil(math.min(wave, 6) * more))
        add("pinky", math.ceil(math.min(wave // 2, 4) * more))
        add("gazer", math.ceil(math.min((wave - 1) // 2, 3) * more))
        -- Shuffled, so a wave does not come through a kind at a time.
        for i = #to_spawn, 2, -1 do
            local j = math.random(1, i)
            to_spawn[i], to_spawn[j] = to_spawn[j], to_spawn[i]
        end
        state, next_at = "fight", clock
        tell(0, `wave {wave} {#to_spawn}`)
        core.log(`yafps_game: wave {wave}, {#to_spawn} demons`)

        -- The bigger guns turn up, each from its wave on, one at a time.
        for _, g in GUN_AT do
            if wave >= g.wave and not gun_out[g.gun] and
               spawn_pickup(g.gun, CENTRE + g.x, GROUND + g.y + 0.3, CENTRE + g.z, nil, g.gun) then
                gun_out[g.gun] = true
                tell(0, `msg {TITLE[g.gun]} HAS APPEARED`)
            end
        end
        -- And every pad not holding its thing gets it back.
        local held = {}
        for _, p in pickups do
            if p.pad then
                held[p.pad] = true
            end
        end
        for i, pad in PADS do
            if not held[i] then
                spawn_pickup(pad.what, CENTRE + pad.x, GROUND + (pad.y or 0), CENTRE + pad.z, i)
            end
        end
    end

    -- The Guardian: out of the reactor's east gate once the last wave is
    -- cleared. Dead, every player here has passed the trial (mods/keys' altar).
    local guardian = 0
    local function step_guardian()
        if core.mob_pos(guardian) ~= nil then
            return
        end
        guardian = 0
        state = "done"
        tell(0, "msg THE GUARDIAN HAS FALLEN")
        for _, p in core.players() do
            core.event.emit("keys:earned", { player = p, key = "arena" })
        end
        core.log("yafps_game: the guardian is dead")
    end

    local function step_waves(anybody: boolean)
        if not anybody then
            -- Nobody here: the engine lets the demons go, and the next player
            -- starts from the first wave.
            wave, state, next_at, to_spawn = 0, "rest", clock + 5, {}
            guardian = 0   -- the engine lets it go with the rest
            last_wave, more, sturdier = LAST_WAVE, 1.0, 1.0
            return
        end
        if state == "done" then
            return
        end
        if state == "guardian" then
            step_guardian()
            return
        end
        if state == "rest" then
            if clock >= next_at and wave >= last_wave then
                guardian = core.spawn_mob("yafps_game:guardian", CENTRE + 34, GROUND, CENTRE + 7)
                if guardian ~= 0 and sturdier > 1 then core.mob_health(guardian, math.floor(HP.guardian * sturdier)) end
                if guardian ~= 0 then
                    state = "guardian"
                    tell(0, `gate {CENTRE + 34} {GROUND} {CENTRE + 7}`)
                    tell(0, "msg THE GUARDIAN OF THE REACTOR")
                    core.log("yafps_game: the guardian is out")
                end
            elseif clock >= next_at then
                start_wave()
            end
            return
        end
        if #to_spawn > 0 then
            if clock >= next_at then
                local kind = table.remove(to_spawn)
                local g = GATES[math.random(1, #GATES)]
                local m = core.spawn_mob(`yafps_game:{kind}`, CENTRE + g[1], GROUND, CENTRE + g[2])
                if m ~= 0 and sturdier > 1 then core.mob_health(m, math.floor(HP[kind] * sturdier)) end
                tell(0, `gate {CENTRE + g[1]} {GROUND} {CENTRE + g[2]}`)
                next_at = clock + 0.7
            end
            return
        end
        local n = alive()
        if n ~= left_said then
            left_said = n
            tell(0, `left {n}`)
        end
        if n == 0 then
            state, next_at = "rest", clock + 8
            tell(0, `clear {wave}`)
        end
    end

    core.event.on("keys:level", function(ev)
        if wave == 0 then
            last_wave, more = 3 + ev.d // 2, 1 + 0.1 * (ev.d - 1)
            sturdier = 1 + 0.15 * (ev.d - 1)
            core.log(`yafps_game: a site's level, d {ev.d}: {last_wave} waves, x{more} demons`)
        end
    end)

    -- ---------------------------------------------------------------- shooting

    local function jitter(d: {number}, spread: number): {number}
        if spread <= 0 then
            return d
        end
        local x = d[1] + (math.random() * 2 - 1) * spread
        local y = d[2] + (math.random() * 2 - 1) * spread
        local z = d[3] + (math.random() * 2 - 1) * spread
        local l = math.sqrt(x * x + y * y + z * z)
        return { x / l, y / l, z / l }
    end

    local function killed(p: number, at: {number})
        local s = players[p]
        if s then
            s.kills += 1
            tell(p, ammo_line(s))
        end
        -- Modern arena shooters' reward for a kill: health, where it fell.
        spawn_pickup("health_orb", at[1], math.max(GROUND, at[2] - 0.8), at[3])
    end

    -- One bullet from o along d: what it struck, and the tracer everybody sees.
    local function bullet(p: number, o: {number}, d: {number}, range: number, damage: number, kind: string)
        local far = range
        local _, _, _, wall = core.raycast(o[1], o[2], o[3], d[1], d[2], d[3], range)
        if wall ~= nil then
            far = wall
        end
        local dist, dead = core.shoot(o[1], o[2], o[3], d[1], d[2], d[3], far, damage)
        if dist ~= nil then
            far = dist
        end
        local e = { o[1] + d[1] * far, o[2] + d[2] * far, o[3] + d[3] * far }
        if dead then
            killed(p, e)
        end
        tell(0, string.format("%s %.2f %.2f %.2f %.2f %.2f %.2f %d", kind, o[1], o[2], o[3],
                              e[1], e[2], e[3], if dist ~= nil then 1 else 0))
    end

    local function launch_ball(p: number)
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then
            return
        end
        local id = core.ecs.spawn(BALL, x + dx * 0.8, y + EYE - 0.4 + dy * 0.8, z + dz * 0.8)
        if id == nil then
            return
        end
        core.ecs.set(id, "vx", dx * BALL_SPEED)
        core.ecs.set(id, "vy", dy * BALL_SPEED)
        core.ecs.set(id, "vz", dz * BALL_SPEED)
        core.ecs.set(id, "owner", p)
        core.ecs.set(id, "t", 0)
        core.ecs.visible(id, true)
        core.ecs.model(id, model.plasma_ball)
        balls[id] = true
    end

    -- The ball bursts: the blast, then the tracers from whoever fired it, fanned
    -- across the way they are looking.
    local function burst(id: number, x: number, y: number, z: number)
        balls[id] = nil
        local owner = core.ecs.get(id, "owner") or 0
        core.ecs.kill(id)
        local dead = core.blast(x, y, z, BLAST_R, BLAST_DMG)
        tell(0, string.format("boom %.2f %.2f %.2f", x, y, z))
        local s = players[owner]
        if s and dead > 0 then
            s.kills += dead
            tell(owner, ammo_line(s))
        end
        local px, py, pz = core.player_pos(owner)
        local dx, dy, dz = core.player_look(owner)
        if px == nil or dx == nil then
            return
        end
        local yaw = math.atan2(dx, dz)
        local o = { px, py + EYE, pz }
        for i = 0, RAYS - 1 do
            local a = yaw + (i / (RAYS - 1) - 0.5) * math.rad(90)
            local d = { math.sin(a), dy * 0.5 + (math.random() - 0.5) * 0.1, math.cos(a) }
            local l = math.sqrt(d[1] * d[1] + d[2] * d[2] + d[3] * d[3])
            d = { d[1] / l, d[2] / l, d[3] / l }
            local dist, dead_one = core.shoot(o[1], o[2], o[3], d[1], d[2], d[3], 40, RAY_DMG)
            if dist ~= nil then
                local e = { o[1] + d[1] * dist, o[2] + d[2] * dist, o[3] + d[3] * dist }
                if dead_one then
                    killed(owner, e)
                end
                tell(0, string.format("ray %.2f %.2f %.2f %.2f %.2f %.2f 1", o[1], o[2], o[3],
                                      e[1], e[2], e[3]))
            end
        end
    end

    local function step_balls(dt: number)
        for id in balls do
            local x, y, z = core.ecs.get(id, "x"), core.ecs.get(id, "y"), core.ecs.get(id, "z")
            if x == nil then
                balls[id] = nil
                continue
            end
            local vx, vy, vz = core.ecs.get(id, "vx"), core.ecs.get(id, "vy"), core.ecs.get(id, "vz")
            local t = core.ecs.get(id, "t") + dt
            core.ecs.set(id, "t", t)
            local cy = y + 0.4   -- the middle of the ball; its feet are at y
            local step = BALL_SPEED * dt
            local _, _, _, wall = core.raycast(x, cy, z, vx, vy, vz, step)
            if wall ~= nil then
                local f = wall / BALL_SPEED
                burst(id, x + vx * f, cy + vy * f, z + vz * f)
            elseif core.monsters_near(x, cy - 0.9, z, 1.4) > 0 or t > BALL_LIFE then
                burst(id, x, cy, z)
            else
                core.ecs.set(id, "x", x + vx * dt)
                core.ecs.set(id, "y", y + vy * dt)
                core.ecs.set(id, "z", z + vz * dt)
            end
        end
    end

    local function fire(p: number, s)
        local gun = short(core.held_item(p))
        local g = GUNS[gun]
        if g == nil then
            return
        end
        if g.ammo and s.ammo[g.ammo] < g.cost then
            if not s.dry then
                s.dry = true
                tell(p, "dry")
            end
            return
        end
        s.next_fire = clock + g.every
        if g.ammo then
            s.ammo[g.ammo] -= g.cost
            tell(p, ammo_line(s))
        end
        tell(p, `fired {gun}`)
        if gun == "plasma" then
            s.plasma_at = clock + PLASMA_DELAY
            return
        end

        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then
            return
        end
        local o = { x, y + EYE, z }
        for _ = 1, g.pellets do
            bullet(p, o, jitter({ dx, dy, dz }, g.spread), g.range, g.damage,
                   if gun == "shotgun" or gun == "ssg" then "pellet" else "shot")
        end
    end

    trigger = function(p: number, down: boolean)
        local s = players[p]
        if s ~= nil then
            s.held = down
            s.dry = false
        end
    end

    core.event.on("core:net_message", function(ev)
        if ev.text == "fire1" then
            trigger(ev.player, true)
        elseif ev.text == "fire0" then
            trigger(ev.player, false)
        end
    end)

    -- ---------------------------------------------------------------- the tick

    local function step_pickups()
        for id, p in pickups do
            local tx, ty, tz = core.ecs.get(id, "x"), core.ecs.get(id, "y"), core.ecs.get(id, "z")
            if tx == nil then
                pickups[id] = nil
                continue
            end
            for pl, s in players do
                local x, y, z = core.player_pos(pl)
                if x ~= nil and (x - tx) ^ 2 + (z - tz) ^ 2 < 1.6 and math.abs(y - ty) < 1.8 then
                    local took = false
                    if p.gun then
                        core.give_item(pl, `yafps_game:{p.gun}`, 1)
                        add_ammo(s, GUNS[p.gun].ammo, if p.gun == "plasma" then 80 elseif p.gun == "ssg" then 12 else 100)
                        gun_out[p.gun] = false
                        took = true
                    else
                        local give = GIVES[p.what]
                        if give.heal then
                            -- Left lying there by somebody who does not need it.
                            if core.health(pl) < 20 then
                                core.heal(pl, give.heal)
                                took = true
                            end
                        elseif s.ammo[give.ammo] < MAX_AMMO[give.ammo] then
                            add_ammo(s, give.ammo, give.n)
                            took = true
                        end
                    end
                    if took then
                        pickups[id] = nil
                        core.ecs.kill(id)
                        tell(pl, ammo_line(s))
                        tell(pl, `got {p.gun or p.what}`)
                        break
                    end
                end
            end
        end
    end

    function server_step(dt: number)
        -- Who is here: a new player gets the pistol and the shotgun, some ammo,
        -- and is told so; one who has left stops being kept.
        local here = {}
        local anybody = false
        for _, p in core.players() do
            here[p] = true
            anybody = true
            local s = players[p]
            if s == nil then
                s = { ammo = { bullets = 50, shells = 16, cells = 0 }, kills = 0, held = false,
                      next_fire = 0, plasma_at = 0, dry = false }
                players[p] = s
                local name = core.player_name(p)
                if not armed[name] then
                    armed[name] = true
                    core.give_item(p, "yafps_game:pistol", 1)
                    core.give_item(p, "yafps_game:shotgun", 1)
                end
                tell(p, ammo_line(s))
                if wave > 0 then
                    tell(p, `wave {wave} 0`)
                end
            end
            if s.held and clock >= s.next_fire then
                fire(p, s)
            end
            if s.plasma_at > 0 and clock >= s.plasma_at then
                s.plasma_at = 0
                launch_ball(p)
            end
        end
        for p in players do
            if not here[p] then
                players[p] = nil
            end
        end

        step_waves(anybody)
        step_balls(dt)
        step_pickups()
    end
end

-- ===================================================== the player's machine

if drawing then
    local tracers = {}   -- { o, e, left, r, g, b, size }
    local bursts  = {}   -- { x, y, z, left }
    local ammo    = { bullets = 0, shells = 0, cells = 0 }
    local kills   = 0
    local wave    = 0
    local left    = -1
    local banner, banner_left = "", 0
    local notice, notice_left = "", 0
    local dry_left = 0
    local hand = ""

    local function say_banner(text: string, seconds: number)
        banner, banner_left = text, seconds
    end

    heard = function(text: string)
        local w = string.split(text, " ")
        local what = w[1]
        if (what == "shot" or what == "pellet" or what == "ray") and #w == 8 then
            local green = what == "ray"
            table.insert(tracers, {
                o = { tonumber(w[2]), tonumber(w[3]), tonumber(w[4]) },
                e = { tonumber(w[5]), tonumber(w[6]), tonumber(w[7]) },
                left = if green then 0.35 else 0.06,
                r = if green then 90 else 255, g = if green then 255 else 225, b = if green then 80 else 140,
                size = if green then 0.12 else 0.05,
                hit = w[8] == "1",
            })
        elseif what == "boom" then
            table.insert(bursts, { x = tonumber(w[2]), y = tonumber(w[3]), z = tonumber(w[4]), left = 0.6 })
        elseif what == "gate" then
            table.insert(bursts, { x = tonumber(w[2]), y = tonumber(w[3]), z = tonumber(w[4]), left = 0.5, red = true })
        elseif what == "fired" then
            core.arms_clip(`fire_{w[2]}`)
        elseif what == "ammo" then
            ammo.bullets, ammo.shells, ammo.cells = tonumber(w[2]) or 0, tonumber(w[3]) or 0, tonumber(w[4]) or 0
            kills = tonumber(w[5]) or kills
        elseif what == "wave" then
            wave = tonumber(w[2]) or wave
            if (tonumber(w[3]) or 0) > 0 then
                say_banner(`WAVE {wave}`, 2.5)
            end
            left = -1
        elseif what == "left" then
            left = tonumber(w[2]) or left
        elseif what == "clear" then
            say_banner(`WAVE {w[2]} CLEARED`, 3)
            left = 0
        elseif what == "msg" then
            say_banner(table.concat(w, " ", 2), 3)
        elseif what == "got" then
            local names = { ammo_bullets = "BULLETS", ammo_shells = "SHELLS", ammo_cells = "CELLS",
                            medkit = "MEDKIT", health_orb = "HEALTH", chaingun = "THE CHAINGUN!",
                            ssg = "THE SUPER SHOTGUN!",
                            plasma = "THE plasma cannon!" }
            notice, notice_left = `PICKED UP {names[w[2]] or w[2]}`, 1.5
        elseif what == "dry" then
            dry_left = 1.0
        end
    end

    if not serving then
        core.event.on("core:net_message", function(ev)
            heard(ev.text)
        end)
    end

    -- The left button is the trigger in this world, held for as long as it is
    -- down; the right one does nothing. Both claimed, so the engine neither
    -- digs, hits, builds nor uses with them.
    local function ask(what: string, player: number)
        if serving then
            trigger(player, what == "fire1")
        else
            core.net.send(0, what)
        end
    end

    core.event.on("core:input", function(ev)
        if ev.action == "mouse_left" then
            ev.cancel = true
            ask("fire1", ev.player)
        elseif ev.action == "mouse_left_up" then
            ask("fire0", ev.player)
        elseif ev.action == "mouse_right" then
            ev.cancel = true
        end
    end)

    -- Tracers as rows of small boxes, a PLASMA burst as a green ball of boxes, a
    -- gate as a red one as a demon comes through.
    core.render.pass("yafps_game:tracers", 100, function()
        for _, t in tracers do
            local dx, dy, dz = t.e[1] - t.o[1], t.e[2] - t.o[2], t.e[3] - t.o[3]
            local len = math.sqrt(dx * dx + dy * dy + dz * dz)
            if len > 0 then
                local d = 1.2
                while d < len do
                    local f = d / len
                    core.render.box(t.o[1] + dx * f, t.o[2] + dy * f, t.o[3] + dz * f,
                                    t.size, t.size, t.size, t.r, t.g, t.b, 255)
                    d += 0.9
                end
                if t.hit then
                    core.render.box(t.e[1], t.e[2], t.e[3], 0.18, 0.18, 0.18, 200, 20, 10, 255)
                end
            end
        end
        for _, b in bursts do
            local k = b.left / (if b.red then 0.5 else 0.6)
            local r = (1 - k) * (if b.red then 1.6 else 6) + 0.3
            for i = 0, 11 do
                local a = i * math.pi / 6
                for _, h in { -0.6, 0, 0.6 } do
                    local s = math.sqrt(1 - h * h)
                    core.render.box(b.x + math.cos(a) * r * s, b.y + (if b.red then 0.8 + h * r else h * r),
                                    b.z + math.sin(a) * r * s, 0.25, 0.25, 0.25,
                                    if b.red then 255 else 120, if b.red then 60 else 255,
                                    if b.red then 20 else 90, 255)
                end
            end
        end
    end)

    local function weapon_line(): (string, boolean)
        local gun = short(hand)
        if gun == "pistol" then
            return "PISTOL", true
        elseif gun == "shotgun" then
            return `SHOTGUN   {ammo.shells}`, ammo.shells > 0
        elseif gun == "ssg" then
            return `SUPER SHOTGUN   {ammo.shells}`, ammo.shells >= 2
        elseif gun == "chaingun" then
            return `CHAINGUN   {ammo.bullets}`, ammo.bullets > 0
        elseif gun == "plasma" then
            return `plasma cannon   {ammo.cells}`, ammo.cells >= 40
        end
        return "", true
    end

    core.hud.pass("yafps_game:hud", 100, function()
        -- The crosshair: four ticks round a dot, green like modern arena shooters'.
        local c = { 120, 255, 120, 230 }
        core.hud.rect(0.5, 0.5, 0.004, 0.004, c[1], c[2], c[3], c[4])
        core.hud.rect(0.5 - 0.016 / core.hud.aspect(), 0.5, 0.012, 0.003, c[1], c[2], c[3], c[4])
        core.hud.rect(0.5 + 0.016 / core.hud.aspect(), 0.5, 0.012, 0.003, c[1], c[2], c[3], c[4])
        core.hud.rect(0.5, 0.484, 0.003, 0.012, c[1], c[2], c[3], c[4])
        core.hud.rect(0.5, 0.516, 0.003, 0.012, c[1], c[2], c[3], c[4])

        local line, ok = weapon_line()
        if line ~= "" then
            local r, g, b = 223, 244, 255
            if not ok or dry_left > 0 then
                r, g, b = 255, 70, 50
            end
            core.hud.text(0.97, 0.9, line, 0.045, 2, r, g, b, 255)
        end
        core.hud.text(0.97, 0.95, `B {ammo.bullets}   S {ammo.shells}   C {ammo.cells}`, 0.028, 2,
                      180, 190, 200, 220)

        if wave > 0 then
            local what = if left > 0 then `WAVE {wave}   DEMONS {left}` elseif left == 0 then `WAVE {wave}   CLEAR` else `WAVE {wave}`
            core.hud.text(0.5, 0.05, what, 0.035, 1, 255, 120, 60, 255)
        end
        core.hud.text(0.97, 0.05, `KILLS {kills}`, 0.035, 2, 255, 120, 60, 255)

        if banner_left > 0 then
            core.hud.text(0.5, 0.3, banner, 0.07, 1, 255, 70, 30, math.floor(255 * math.min(1, banner_left * 2)))
        end
        if notice_left > 0 then
            core.hud.text(0.5, 0.62, notice, 0.03, 1, 255, 230, 120, math.floor(255 * math.min(1, notice_left * 2)))
        end
    end)

    function client_step(dt: number)
        for i = #tracers, 1, -1 do
            tracers[i].left -= dt
            if tracers[i].left <= 0 then
                table.remove(tracers, i)
            end
        end
        for i = #bursts, 1, -1 do
            bursts[i].left -= dt
            if bursts[i].left <= 0 then
                table.remove(bursts, i)
            end
        end
        banner_left -= dt
        notice_left -= dt
        dry_left -= dt

        -- A gun brought up when it comes into the hand.
        local now = core.held_item(1)   -- this machine's own player
        if now ~= hand then
            hand = now
            if string.find(hand, "^yafps_game:") then
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
