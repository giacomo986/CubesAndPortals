--!strict
-- shmup: a shooter seen from overhead, in the manner of the arcade cabinets
-- of the eighties: one screen, the sea going by under it, two levels of ten
-- waves of planes, three lives, a score and a high score.
--
-- Whoever arrives is on foot in a hangar (shmup_core). F beside one of the
-- four fighters parked there boards it: it rolls out, takes off and climbs
-- while the camera swings from behind it to overhead (core.fixed_view asked
-- again every frame, then core.top_view_at). W A S D slide the fighter about
-- the screen; the space bar, held, fires; F (or E) goes back to the hangar.
--
--   darts come straight down; weavers swing from side to side and aim a
--   shot now and then; aces stop half way and fire three at a time; wave 5
--   is a bomber that takes forty; wave 10, the last, is a flying fortress:
--   six turrets and a shield that has to be shot down before anything under
--   it can be hit, and comes back ten seconds later
--   divers come down a little and then go for the nearest fighter; sweepers
--   cross the screen from side to side dropping shots; gunships stop and
--   throw four shots round and round; plated planes come five in a file
--   along a path (below, PATHS), and the whole file shot down leaves a
--   power-up
--   the fortress down, a second level: waves 11 to 20, more of everything,
--   and at the end an aircraft carrier on the sea that goes on launching
--   planes -- down its deck, up and out by the top of the screen, and back in
--   as any other plane -- while its four turrets and its island fire
--   the planes keep to the middle 48 of the screen's 80: a fighter can reach
--   every one of them
--   a shot or a plane that touches the fighter costs a life, and two and a
--   half seconds follow in which nothing does; none left, the pilot is put
--   back in the hangar until everybody is, and the game is over
--   planes shot down drop power-ups (below, DROPS); they last the game
--   now and then an islet goes by with a tank on it, which aims too (300,
--   and a power-up)
--   a plane shot down, anybody's, goes into the sea turning and smoking;
--   the game won, the fighters leave by the top of the screen and land
--   several players fly the same waves, each with their own lives and score:
--   whoever boards while a game is going joins it, and the game is made
--   harder for each pilot flying (below, `crew`)
--
-- The server keeps everything that decides: where the planes and the shots
-- are, who was hit. A player's machine is told each shot once, when it is
-- fired, and flies it by itself -- shots go straight -- until it is told the
-- shot is gone; missiles, beams and the fortress, which do not go straight,
-- it is told ten times a second. The planes are the mod's entities, seen by
-- everybody (core.ecs.visible). The islands, crests and clouds going by are
-- drawn by the player's machine and decide nothing.
--
-- The picture's top is +z, so its right is -x (shmup_core).

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

-- The field and the hangar, as shmup_core builds them.
local CX, CZ = 64.5, 140.5
local FLY    = 30
local FLOOR  = 22                    -- where feet stand in the hangar
local HALF_W, HALF_H = 40, 30        -- where a fighter can go
local FOE_W  = 24                    -- and how far from the middle a plane does
local TOP, BOTTOM = CZ + HALF_H + 10, CZ - HALF_H - 10   -- where a plane comes in and where it is gone
local PADS   = { CX - 18, CX - 6, CX + 6, CX + 18 }      -- the parked fighters, by x
local PAD_Z  = 36.5
local START_Z = CZ - 22              -- where a take-off ends: the bottom of the screen
local LIFT_Z  = 76.5                 -- where the wheels leave the apron
local ROLL, CLIMB = 3.2, 2.6         -- a take-off's seconds on the ground and in the climb
local DIST   = 50                    -- the camera over the field
local SHIELD_R = 21                  -- the fortress's shield

local SHOT_SPEED = 70

local heard: (string) -> () = function(_) end
local asked: (number, string) -> () = function(_, _) end

-- What the server tells one player, or everybody (player 0).
local function tell(player: number, text: string)
    if drawing then heard(text) else core.net.send(player, text) end
end

-- ===================================================================== server

if serving then
    core.net.open()

    local FOE = "shmup:foe"
    core.ecs.component(FOE, { "hp" })
    local function model(name: string): number
        return core.model_id(`shmup_core:{name}`)
    end
    local ROBOT, FIGHTER, WINGMAN = model("robot"), model("fighter"), model("wingman")

    -- kind -> hp, points, half width and half length for a hit, model
    local KINDS = {
        dart   = { hp = 1,   points = 100,   hw = 4.0,  hl = 3.2, model = model("foe_dart") },
        weaver = { hp = 2,   points = 150,   hw = 4.0,  hl = 3.2, model = model("foe_weaver") },
        ace    = { hp = 5,   points = 400,   hw = 4.0,  hl = 3.2, model = model("foe_ace") },
        bomber = { hp = 40,  points = 3000,  hw = 8.0,  hl = 5.0, model = model("bomber") },
        mega   = { hp = 350, points = 20000, hw = 18.0, hl = 8.0, model = model("mega") },
        turret = { hp = 22,  points = 500,   hw = 1.8,  hl = 1.8, model = model("turret") },
        tank   = { hp = 6,   points = 300,   hw = 2.6,  hl = 3.4, model = model("tank") },
        diver   = { hp = 2,   points = 200,   hw = 4.0, hl = 3.2,  model = model("foe_diver") },
        sweeper = { hp = 3,   points = 250,   hw = 4.0, hl = 3.2,  model = model("foe_sweeper") },
        plated  = { hp = 6,   points = 300,   hw = 4.0, hl = 3.2,  model = model("foe_plated") },
        gunship = { hp = 14,  points = 800,   hw = 8.0, hl = 5.0,  model = model("foe_gunship") },
        carrier = { hp = 500, points = 30000, hw = 6.5, hl = 23.0, model = model("carrier") },
    }
    local HULLS  = { mega = true, carrier = true }      -- what turrets stand on: hit after them
    local GROUND = { tank = true, carrier = true }      -- on the sea: flying over it costs nothing
    local TURNS  = { diver = true, sweeper = true, plated = true }   -- their noses follow where they go
    -- What a player's machine draws falling into the sea when one is shot down.
    local FALLS = { dart = "foe_dart", weaver = "foe_weaver", ace = "foe_ace", bomber = "bomber", mega = "mega",
                    diver = "foe_diver", sweeper = "foe_sweeper", plated = "foe_plated", gunship = "foe_gunship",
                    carrier = "carrier" }
    local ISLET_Y = 21.95     -- the top of an islet's green: where a tank stands
    local SCROLL = 12         -- what the sea goes by at, and a tank with it
    -- The fortress's turrets: across, along (its nose is down the screen),
    -- how high they stand on it, and whether they fire three at a time.
    -- tools/make_shmup_models.py has a ring on the model for each.
    local TURRETS = { { -15, -1.5, 3.0, false }, { -8, -1.5, 3.0, false }, { 8, -1.5, 3.0, false },
                      { 15, -1.5, 3.0, false }, { 0, -7, 4.4, true }, { 0, 6, 4.4, true } }
    local SHIELD, SHIELD_DOWN = 70, 10
    local LAST_WAVE = 20      -- the carrier; the fortress is wave 10, the first level's last
    -- The carrier: its bow is up the screen. Its deck's height, its turrets
    -- (across, along: tools/make_shmup_models.py has a sponson for each),
    -- and what it launches.
    local DECK_Y = 24
    local CARRIER_GUNS = { { 6.5, 10 }, { -6.5, 10 }, { 6.5, -12 }, { -6.5, -14 } }
    local LAUNCHED = { "dart", "dart", "weaver", "weaver", "diver", "sweeper", "ace" }
    -- What a file of plated planes flies, each half a second behind the one
    -- before: t seconds in and which side it came from (1 or -1) -> x, z,
    -- and whether it is over. One that is not ends under the screen.
    local PW = FOE_W - 2
    local PATHS: { (number, number) -> (number, number, boolean) } = {
        function(t, s) return CX + s * PW * math.cos(0.9 * t), TOP - 10 * t, false end,                    -- swinging down
        function(t, s)                                                                                      -- down one side, up the other
            local a = math.pi * t / 7
            return CX + s * PW * math.cos(a), TOP - 54 * math.sin(a), t >= 7
        end,
        function(t, s) return CX + s * (10 + 9 * math.cos(1.8 * t)), TOP - 11 * t + 9 * math.sin(1.8 * t), false end,   -- loops
        function(t, s) return CX + s * PW * (1 - t / 2.75), TOP - 80 * t / 5.5, false end,                -- corner to corner
    }
    -- For trying things: a game starts after this wave (9 is the fortress at
    -- once, 19 the carrier, and thirty lives), and every plane leaves a
    -- power-up. 0 and false for release.
    local TEST_WAVE, TEST_DROPS = 0, false

    -- What a plane shot down may leave, and how often against the others.
    --   sp  speed, for the game (3 of them)     st  speed, for ten seconds
    --   fp  guns: two, three, three faster      ft  guns twice as fast, for ten seconds
    --   hm  a homing missile more a salvo (4)   dg  two shots at a slant, then four
    --   lz  a beam in place of the guns, ten seconds a pick-up
    --   wm  a small plane alongside that fires too (2)
    local DROPS = { { "sp", 2 }, { "st", 2 }, { "fp", 3 }, { "ft", 2 }, { "hm", 2 }, { "dg", 2 }, { "lz", 2 }, { "wm", 1 } }

    type Foe = { kind: string, x: number, z: number, x0: number, t: number, hp: number, max: number, gun: number,
                 ox: number, oz: number, spread: boolean,
                 vx: number, vz: number,          -- a diver's and a sweeper's way; a plane on the deck's speed
                 path: number, side: number, file: number,    -- a plated plane's
                 low: boolean,                    -- on the carrier: a turret, or a plane not yet up
                 launch: number }                 -- the carrier's next
    type Boss = { kind: string, x: number, z: number, shield: number, max: number, up_at: number }
    type Shot = { x: number, z: number, vx: number, vz: number, owner: number }
    type Wing = { id: number, x: number, z: number }
    type Flier = { lives: number, score: number, fire: boolean, gun: number, safe: number, out: boolean,
                   spd: number, spd_t: number, fast: boolean, pow: number, pow_t: number,
                   hom: number, hom_at: number, diag: number, dgun: number, laser_t: number, hum: number,
                   wings: { Wing }, wgun: number }

    local mode = "attract"      -- "attract" | "play" | "over" | "won"
    local wave, wave_at = 0, 0.0
    local queue: { { any } } = {}    -- { when, kind, x }
    local foes: { [number]: Foe } = {}
    local shots: { [number]: Shot } = {}
    local missiles: { [number]: { x: number, z: number, vx: number, vz: number, owner: number, born: number } } = {}
    local pickups: { [number]: { x: number, z: number, kind: string } } = {}
    local boss: Boss? = nil
    local files: { [number]: number } = {}    -- a file of plated planes -> how many of it are still to shoot down
    local next_file = 0
    -- How many fly, 1 at least, and what a game is made harder by for each
    -- pilot past the first: planes take 60% more (a dart goes on taking one),
    -- half as many more come in the same time, and they fire a quarter faster.
    -- A wave is sized when it starts, a plane when it comes in; the rate of
    -- fire follows who is flying now.
    local crew = 1
    local function more(by: number): number
        return 1 + by * (crew - 1)
    end
    local next_shot = 1
    local fliers: { [number]: Flier } = {}
    local boarding: { [number]: number } = {}    -- player -> the pad they are taking off from
    local homed: { [number]: number } = {}       -- player -> when they were last put in the hangar
    local kills = 0
    local clock = 0.0
    local ticks = 0
    local timed = false
    local hi = tonumber(core.storage_get("hi") or "") or 0

    local function status(p: number)
        tell(p, `st {mode} {wave} {crew}`)
    end
    local function count(p: number, f: Flier)
        tell(p, `me {f.lives} {f.score} {hi}`)
    end
    -- What a fighter carries, and how long what does not last has left.
    local function loadout(p: number, f: Flier)
        tell(p, string.format("ld %d %d %d %d %d %.1f %.1f %.1f", f.spd, f.pow, f.hom, f.diag, #f.wings,
            math.max(0, f.laser_t - clock), math.max(0, f.pow_t - clock), math.max(0, f.spd_t - clock)))
    end
    local function set_speed(p: number, f: Flier)
        f.fast = clock < f.spd_t
        core.body_movement(p, 6, 9, 24 + 5 * f.spd + (if f.fast then 10 else 0), 8, -25)
    end

    -- Out of the fighter and on foot at the hangar's back.
    local function to_hangar(p: number)
        local f = fliers[p]
        if f then
            f.out, f.fire = true, false
            for _, w in f.wings do core.ecs.kill(w.id) end
            table.clear(f.wings)
        end
        boarding[p] = nil
        homed[p] = clock
        core.body_model(p, ROBOT)
        core.body_movement(p, 6, 9, 0, 8, -25)
        core.player_place(p, CX, FLOOR, 27, 0, 0, 1)
        tell(p, "down")
    end

    local function clear()
        for id in foes do core.ecs.kill(id) end
        table.clear(foes)
        for id in shots do tell(0, `k {id}`) end
        table.clear(shots)
        for id in missiles do tell(0, `k {id}`) end
        table.clear(missiles)
        for id in pickups do tell(0, `k {id}`) end
        table.clear(pickups)
        table.clear(queue)
        table.clear(files)
        boss = nil
    end

    local function spawn(kind: string, x: number, z: number, y: number): number?
        local id = core.ecs.spawn(FOE, x, y, z)
        if id == nil then return nil end   -- no session table: entities need a listening server
        local k = KINDS[kind]
        local hp = if kind == "dart" then k.hp else math.ceil(k.hp * more(0.6))
        core.ecs.set(id, "hp", hp)
        core.ecs.set(id, "yaw", 0)     -- nose down the screen, -z
        core.ecs.visible(id, true)
        core.ecs.model(id, k.model)
        foes[id] = { kind = kind, x = x, z = z, x0 = x, t = 0, hp = hp, max = hp, gun = 1 + math.random(),
                     ox = 0, oz = 0, spread = false, vx = 0, vz = 0, path = 0, side = 1, file = 0,
                     low = false, launch = 0 }
        return id
    end

    local function spawn_boss()
        local z = TOP + 14
        if spawn("mega", CX, z, FLY) == nil then return end
        local shield = math.ceil(SHIELD * more(0.6))
        boss = { kind = "mega", x = CX, z = z, shield = shield, max = shield, up_at = 0 }
        for i, t in TURRETS do
            local id = spawn("turret", CX + t[1], z + t[2], FLY + t[3])
            if id then
                local foe = foes[id]
                foe.ox, foe.oz, foe.spread, foe.gun = t[1], t[2], t[4], 2 + i * 0.4
            end
        end
    end

    local function spawn_carrier()
        local z = TOP + 26
        local id = spawn("carrier", CX, z, 21)
        if id == nil then return end
        core.ecs.set(id, "yaw", math.pi)     -- bow up the screen
        foes[id].launch, foes[id].gun = 4, 5
        boss = { kind = "carrier", x = CX, z = z, shield = 0, max = 0, up_at = 0 }
        for i, t in CARRIER_GUNS do
            local tid = spawn("turret", CX + t[1], z + t[2], DECK_Y)
            if tid then
                local foe = foes[tid]
                foe.ox, foe.oz, foe.low, foe.gun = t[1], t[2], true, 3 + i * 0.5
            end
        end
    end

    -- A file of n plated planes from `at` seconds into the wave.
    local function file(at: number, n: number)
        next_file += 1
        files[next_file] = n
        local path, s = math.random(#PATHS), if math.random() < 0.5 then -1 else 1
        for i = 1, n do
            table.insert(queue, { at + i * 0.5, "plated", CX, path, s, next_file })
        end
    end

    local function shoot(x: number, z: number, vx: number, vz: number, owner: number)
        local id = next_shot
        next_shot += 1
        shots[id] = { x = x, z = z, vx = vx, vz = vz, owner = owner }
        tell(0, string.format("%s %d %.1f %.1f %.1f %.1f", if owner > 0 then "b" else "e", id, x, z, vx, vz))
        if owner == 0 then core.play_sound("shmup:foe_shot", x, FLY, z) end
    end

    -- A wave: what comes, and when from its start.
    local function start_wave()
        wave += 1
        wave_at = clock
        table.clear(queue)
        -- The wave in its level, 1 to 10, and the level.
        local lw, level = (wave - 1) % 10 + 1, (wave - 1) // 10 + 1
        local hard = level > 1
        local function across(margin: number): number
            return CX + (math.random() * 2 - 1) * (FOE_W - margin)
        end
        if wave == LAST_WAVE then
            spawn_carrier()
        elseif lw == 10 then
            spawn_boss()
        elseif lw == 5 then
            table.insert(queue, { 1.5, "bomber", CX })
            if hard then
                file(4, 6)
                file(12, 6)
            end
        else
            local n = math.floor((4 + 2 * lw + (if hard then 4 else 0)) * more(0.5))
            local gap = math.max(0.35, 1.0 - 0.06 * lw) / more(0.5)
            for i = 1, n do
                local kind = if i % 3 == 0 and (hard or lw >= 2) then "weaver"
                    elseif i % 4 == 0 and (hard or lw >= 6) then "diver"
                    elseif i % 7 == 0 and (hard or lw >= 4) then "sweeper"
                    else "dart"
                table.insert(queue, { 1.5 + i * gap, kind, across(3) })
            end
            -- Now and then an islet goes by with a tank on it.
            if (hard or lw >= 2) and math.random() < 0.6 then
                table.insert(queue, { 3 + math.random() * 6, "tank", across(4) })
            end
            for i = 1, math.ceil(lw // 2 * more(0.5)) do
                table.insert(queue, { 3 + i * 4 / more(0.5), "ace", across(12) })
            end
            for i = 1, if hard then lw // 3 + 1 elseif lw >= 7 then 1 else 0 do
                table.insert(queue, { 5 + i * 6, "gunship", across(12) })
            end
            for i = 1, (if hard then 2 elseif lw >= 3 then 1 else 0) + lw // 7 do
                file(i * 5 - 2, 4 + crew)
            end
        end
        status(0)
        if wave > 1 then core.play_sound("shmup:wave", CX, FLY, CZ) end
    end

    local function new_flier(): Flier
        return { lives = if TEST_WAVE > 0 then 30 else 3, score = 0, fire = false, gun = 0, safe = clock + 2, out = false,
                 spd = 0, spd_t = 0, fast = false, pow = 1, pow_t = 0, hom = 0, hom_at = 0, diag = 0, dgun = 0,
                 laser_t = 0, hum = 0, wings = {}, wgun = 0 }
    end

    local function ended(to: string)
        mode, wave_at = to, clock
        clear()
        local best = hi
        for _, f in fliers do best = math.max(best, f.score) end
        if best > hi then
            hi = best
            core.storage_set("hi", tostring(hi))
        end
        for p, f in fliers do count(p, f) end
        status(0)
        core.play_sound(if to == "won" then "shmup:start" else "shmup:over", CX, FLY, CZ)
    end

    -- A life lost; the last one puts the pilot back in the hangar.
    local function hit(p: number, f: Flier, x: number, z: number)
        f.lives -= 1
        f.safe = clock + 2.5
        tell(0, string.format("boom %.1f %.1f 6", x, z))
        tell(p, "hit")
        core.play_sound("shmup:hit", x, FLY, z)
        if f.lives <= 0 then
            -- Only the last life is a fighter lost: before it, the same one flies on.
            tell(0, string.format("fall fighter %.1f %.1f", x, z))
            for _, w in f.wings do tell(0, string.format("fall wingman %.1f %.1f", w.x, w.z)) end
            to_hangar(p)
        end
        count(p, f)
    end

    local function full(f: Flier, kind: string): boolean
        return (kind == "sp" and f.spd >= 3) or (kind == "fp" and f.pow >= 4) or (kind == "hm" and f.hom >= 4)
            or (kind == "dg" and f.diag >= 2) or (kind == "wm" and #f.wings >= 2)
    end

    -- A power-up left where a plane went down: one its killer can still use.
    local function drop(x: number, z: number, f: Flier?)
        local total, can = 0, {}
        for _, d in DROPS do
            if f == nil or not full(f, d[1]) then
                table.insert(can, d)
                total += d[2]
            end
        end
        local r, kind = math.random() * total, can[1][1]
        for _, d in can do
            r -= d[2]
            if r <= 0 then
                kind = d[1]
                break
            end
        end
        local id = next_shot
        next_shot += 1
        x = math.clamp(x, CX - FOE_W, CX + FOE_W)
        pickups[id] = { x = x, z = z, kind = kind }
        tell(0, string.format("u %d %s %.1f %.1f", id, kind, x, z))
    end

    local function take(p: number, f: Flier, kind: string, x: number, z: number)
        if full(f, kind) then
            f.score += 500
        elseif kind == "sp" then
            f.spd += 1
            set_speed(p, f)
        elseif kind == "st" then
            f.spd_t = clock + 10
            set_speed(p, f)
        elseif kind == "fp" then
            f.pow += 1
        elseif kind == "ft" then
            f.pow_t = clock + 10
        elseif kind == "hm" then
            f.hom += 1
        elseif kind == "dg" then
            f.diag += 1
        elseif kind == "lz" then
            f.laser_t = math.min(clock + 30, math.max(clock, f.laser_t) + 10)
        elseif kind == "wm" then
            local id = core.ecs.spawn(FOE, x, FLY, z - 3)
            if id then
                core.ecs.set(id, "hp", 1)
                core.ecs.set(id, "yaw", math.pi)     -- nose up the screen
                core.ecs.visible(id, true)
                core.ecs.model(id, WINGMAN)
                table.insert(f.wings, { id = id, x = x, z = z - 3 })
            end
        end
        tell(p, `got {kind}`)
        loadout(p, f)
        count(p, f)
        core.play_sound("shmup:pickup", x, FLY, z)
    end

    local function killed(id: number, foe: Foe, by: number)
        local k = KINDS[foe.kind]
        local big = foe.kind == "bomber" or foe.kind == "mega"
        tell(0, string.format("boom %.1f %.1f %d", foe.x, foe.z, if foe.kind == "mega" then 30 elseif big then 14 else 7))
        core.play_sound(if big then "shmup:big_boom" else "shmup:boom", foe.x, FLY, foe.z)
        if FALLS[foe.kind] then tell(0, string.format("fall %s %.1f %.1f", FALLS[foe.kind], foe.x, foe.z)) end
        core.ecs.kill(id)
        foes[id] = nil
        local f = fliers[by]
        if f then
            f.score += k.points
            count(by, f)
        end
        if HULLS[foe.kind] then
            for tid, t in foes do
                if t.kind == "turret" then
                    tell(0, string.format("boom %.1f %.1f 7", t.x, t.z))
                    core.ecs.kill(tid)
                    foes[tid] = nil
                end
            end
            for _ = 1, 8 do
                tell(0, string.format("boom %.1f %.1f %d", foe.x + (math.random() * 2 - 1) * k.hw,
                    foe.z + (math.random() * 2 - 1) * k.hl, 10 + math.random(8)))
            end
            boss = nil
            if wave == LAST_WAVE then
                ended("won")
            else
                wave_at = clock + 3     -- the fortress down: a breath, and the second level
            end
        elseif foe.kind ~= "turret" then
            -- Power-ups are few: a whole file of plated planes, a gunship,
            -- every other tank, two from a bomber, and one plane in fifteen.
            kills += 1
            local whole = false
            if foe.file > 0 and files[foe.file] then
                files[foe.file] -= 1
                whole = files[foe.file] == 0
            end
            if foe.kind == "bomber" then
                for i = -1, 1, 2 do drop(foe.x + i * 5, foe.z, f) end
            elseif whole or foe.kind == "gunship" or (foe.kind == "tank" and math.random() < 0.5)
                or kills % 15 == 0 or TEST_DROPS then
                drop(foe.x, foe.z, f)
            end
        end
    end

    local function hurt(id: number, foe: Foe, dmg: number, by: number, x: number, z: number)
        foe.hp -= dmg
        if foe.hp <= 0 then
            killed(id, foe, by)
        elseif dmg >= 1 then
            tell(0, string.format("boom %.1f %.1f 2", x, z))
        end
    end

    local function shield_hit(b: Boss, dmg: number, x: number, z: number)
        b.shield -= dmg
        if dmg >= 1 then tell(0, string.format("boom %.1f %.1f 2", x, z)) end
        if b.shield <= 0 then
            b.shield, b.up_at = 0, clock + SHIELD_DOWN
            tell(0, string.format("boom %.1f %.1f 24", b.x, b.z))
            core.play_sound("shmup:shield", b.x, FLY, b.z)
        end
    end

    -- A fighter's shot or missile at x, z: true if something took it. The
    -- shield takes everything inside it; a turret before the hull under it.
    local function strike(x: number, z: number, reach: number, dmg: number, by: number): boolean
        local b = boss
        if b and b.shield > 0 and (x - b.x) ^ 2 + (z - b.z) ^ 2 < SHIELD_R ^ 2 then
            shield_hit(b, dmg, x, z)
            return true
        end
        local hull_id: number?, hull: Foe? = nil, nil
        for fid, foe in foes do
            local k = KINDS[foe.kind]
            if math.abs(x - foe.x) < k.hw and math.abs(z - foe.z) < k.hl + reach then
                if HULLS[foe.kind] then
                    hull_id, hull = fid, foe
                else
                    hurt(fid, foe, dmg, by, x, z)
                    return true
                end
            end
        end
        if hull_id and hull then
            hurt(hull_id, hull, dmg, by, x, z)
            return true
        end
        return false
    end

    asked = function(p: number, text: string)
        local f = fliers[p]
        if text == "board" then
            local x, _, z = core.player_pos(p)
            if x == nil or boarding[p] or (f and not f.out) then return end
            if mode == "over" or mode == "won" or (mode == "play" and f) then
                tell(p, "wait")     -- shot down: until this game is over
                return
            end
            for i, px in PADS do
                if (x - px) ^ 2 + (z - PAD_Z) ^ 2 < 7 ^ 2 then
                    boarding[p] = i
                    tell(p, `go {i}`)
                    core.play_sound("shmup:engine", px, FLOOR, PAD_Z)
                    return
                end
            end
        elseif text == "up" then
            local pad = boarding[p]
            if pad == nil then return end
            boarding[p] = nil
            if mode == "over" or mode == "won" then
                to_hangar(p)
                return
            end
            if mode == "attract" then
                clear()
                mode, wave, kills, crew = "play", TEST_WAVE, 0, 1
                table.clear(fliers)
                start_wave()
                core.play_sound("shmup:start", CX, FLY, CZ)
            end
            local n = new_flier()
            fliers[p] = n
            core.body_model(p, FIGHTER)
            set_speed(p, n)
            core.player_place(p, PADS[pad], FLY, START_Z, 0, 0, 1)
            tell(p, "air")
            status(p)
            count(p, n)
            loadout(p, n)
        elseif text == "f1" then
            if f and not f.out and mode == "play" then f.fire = true end
        elseif text == "f0" then
            if f then f.fire = false end
        elseif text == "quit" then
            if f and not f.out then
                to_hangar(p)
                count(p, f)
            end
        elseif text == "hello" then
            status(p)
            tell(p, `me {if f then f.lives else 0} {if f then f.score else 0} {hi}`)
            if f and not f.out then
                tell(p, "air")
                loadout(p, f)
            end
        end
    end
    core.event.on("core:net_message", function(ev) asked(ev.player, ev.text) end)

    -- The nearest fighter still flying to a point, for a plane that aims.
    local function nearest(x: number, z: number): (number?, number?)
        local bx, bz, bd = nil, nil, math.huge
        for p, f in fliers do
            if not f.out then
                local px, _, pz = core.player_pos(p)
                if px then
                    local d = (px - x) ^ 2 + (pz - z) ^ 2
                    if d < bd then bx, bz, bd = px, pz, d end
                end
            end
        end
        return bx, bz
    end

    local function aimed(x: number, z: number, speed: number, spread: number, n: number)
        local tx, tz = nearest(x, z)
        local a = if tx then math.atan2(tx - x, (tz :: number) - z) else math.pi
        for i = 1, n do
            local b = a + (i - (n + 1) / 2) * spread
            shoot(x, z - 3, math.sin(b) * speed, math.cos(b) * speed, 0)
        end
    end

    core.event.on("core:tick", function(ev)
        local dt = ev.dt
        clock += dt
        ticks += 1
        if not timed then
            timed = true
            core.set_time(0.5)
            core.set_day_cycle(false)
        end

        -- Who is here, and where. Whoever is not flying belongs in the
        -- hangar: somebody who came back to a body left out over the sea, or
        -- got out of the hangar somehow, is put there.
        local here: { [number]: { number } } = {}
        for _, p in core.players() do
            local x, y, z = core.player_pos(p)
            if x then
                here[p] = { x, y, z }
                local f = fliers[p]
                if (f == nil or f.out) and (z > 80 or y < FLOOR - 0.6) and clock - (homed[p] or -9) > 1 then
                    to_hangar(p)
                end
            end
        end
        for p, f in fliers do
            if not here[p] then
                for _, w in f.wings do core.ecs.kill(w.id) end
                fliers[p] = nil
            end
        end
        for p in boarding do
            if not here[p] then boarding[p] = nil end
        end

        if mode == "over" or mode == "won" then
            -- Won: the bursts, the fighters off the top of the screen and their
            -- landing (the player's machine, 10.7 s of it) before anybody is on foot.
            if mode == "won" and clock - wave_at > 2.5 then
                -- The fighters leave the screen: the planes alongside go with them.
                for _, f in fliers do
                    for _, w in f.wings do core.ecs.kill(w.id) end
                    table.clear(f.wings)
                end
            end
            if clock - wave_at > (if mode == "won" then 11.5 else 5) then
                for p, f in fliers do
                    if not f.out then to_hangar(p) end
                end
                mode = "attract"
                table.clear(fliers)
                status(0)
            end
            return
        end
        if mode ~= "play" then return end

        -- Everybody down: over.
        local flying = 0
        for _, f in fliers do
            if not f.out then flying += 1 end
        end
        if flying ~= crew and flying > 0 then
            crew = flying
            status(0)
        end
        local haste = more(0.25)
        if flying == 0 and next(boarding) == nil then
            ended("over")
            return
        end

        -- The wave's planes as their moments come, and the next wave.
        local since = clock - wave_at
        for i = #queue, 1, -1 do
            if since >= queue[i][1] then
                if queue[i][2] == "tank" then
                    -- The islet under it is the player's machine's: told once, it goes by at the tank's pace.
                    if spawn("tank", queue[i][3], TOP + 8, ISLET_Y) then
                        tell(0, string.format("u %d isle %.1f %.1f", next_shot, queue[i][3], TOP + 8))
                        next_shot += 1
                    end
                else
                    local q = queue[i]
                    local id = spawn(q[2], q[3], TOP, FLY)
                    if id and q[4] then foes[id].path, foes[id].side, foes[id].file = q[4], q[5], q[6] end
                end
                table.remove(queue, i)
            end
        end
        if #queue == 0 and next(foes) == nil and since > 2 and mode == "play" then start_wave() end

        -- The fighters: what runs out, their guns, and the planes alongside.
        local beams: { { number } } = {}     -- { owner, x, z, where it ends }
        for p, f in fliers do
            local at = here[p]
            if at == nil or f.out then continue end
            local x, z = at[1], at[3]
            if f.fast and clock >= f.spd_t then
                set_speed(p, f)
                loadout(p, f)
            end
            local quick = if clock < f.pow_t then 0.5 else 1
            if f.fire and clock < f.laser_t then
                -- The beam: everything over the fighter, up to the shield.
                local dps = (10 + 4 * f.pow) / quick
                local b = boss
                local ze = TOP
                if b and b.shield > 0 and math.abs(x - b.x) < SHIELD_R and b.z > z then
                    ze = b.z - math.sqrt(SHIELD_R ^ 2 - (x - b.x) ^ 2)
                    shield_hit(b, dps * dt, x, ze)
                end
                for fid, foe in foes do
                    local k = KINDS[foe.kind]
                    if math.abs(foe.x - x) < k.hw + 0.6 and foe.z + k.hl > z and foe.z - k.hl < ze then
                        hurt(fid, foe, dps * dt, p, x, foe.z)
                    end
                end
                table.insert(beams, { p, x, z + 3, ze })
                if clock >= f.hum then
                    f.hum = clock + 0.2
                    core.play_sound("shmup:laser", x, FLY, z)
                end
            elseif f.fire and clock >= f.gun then
                f.gun = clock + (if f.pow >= 4 then 0.11 else 0.14) * quick
                if f.pow ~= 2 then shoot(x, z + 3.5, 0, SHOT_SPEED, p) end
                if f.pow >= 2 then
                    local off = if f.pow == 2 then 1.2 else 2.0
                    shoot(x - off, z + 3, 0, SHOT_SPEED, p)
                    shoot(x + off, z + 3, 0, SHOT_SPEED, p)
                end
            end
            if f.fire and f.diag > 0 and clock >= f.dgun then
                f.dgun = clock + 0.22 * quick
                for i = 1, f.diag do
                    local vx, vz = math.sin(0.35 * i) * SHOT_SPEED, math.cos(0.35 * i) * SHOT_SPEED
                    shoot(x - 2, z + 2, -vx, vz, p)
                    shoot(x + 2, z + 2, vx, vz, p)
                end
            end
            if f.fire and f.hom > 0 and clock >= f.hom_at then
                f.hom_at = clock + 1.2
                for i = 1, f.hom do
                    local id = next_shot
                    next_shot += 1
                    missiles[id] = { x = x, z = z + 1, vx = (i % 2 * 2 - 1) * (8 + 4 * i), vz = 12, owner = p, born = clock }
                end
                core.play_sound("shmup:missile", x, FLY, z)
            end
            local fire_wings = f.fire and clock >= f.wgun
            if fire_wings then f.wgun = clock + 0.3 end
            for i, w in f.wings do
                local k = math.min(1, 9 * dt)
                w.x += (x + (i * 2 - 3) * 6.5 - w.x) * k
                w.z += (z - 2.5 - w.z) * k
                core.ecs.set(w.id, "x", w.x)
                core.ecs.set(w.id, "z", w.z)
                if fire_wings then shoot(w.x, w.z + 2, 0, SHOT_SPEED, p) end
            end
        end

        -- The planes.
        local live, launch_x, launch_z = 0, nil, nil
        for id, foe in foes do
            foe.t += dt
            live += 1
            local gone = false
            local was_x, was_z = foe.x, foe.z
            if foe.low and foe.kind ~= "turret" then
                -- Launched: down the deck with the carrier, off its bow and up,
                -- out by the top of the screen -- and in again as any plane.
                local b = boss
                if b and foe.t < 1 then foe.x = b.x end
                foe.vz += 30 * dt
                foe.z += foe.vz * dt
                core.ecs.set(id, "y", DECK_Y + (FLY - DECK_Y) * math.clamp((foe.t - 1) / 0.8, 0, 1))
                if foe.z > TOP + 4 then
                    foe.low, foe.t, foe.vz, foe.gun = false, 0, 0, 1 + math.random()
                    foe.x, foe.z = CX + (math.random() * 2 - 1) * (FOE_W - 3), TOP
                    foe.x0, was_x, was_z = foe.x, foe.x, foe.z
                    core.ecs.set(id, "y", FLY)
                    core.ecs.set(id, "yaw", 0)
                end
            elseif foe.kind == "diver" then
                if foe.vz == 0 then
                    foe.z -= 9 * dt
                    if foe.t > 1.8 then
                        -- Now for the nearest fighter, and never back up the screen.
                        local tx, tz = nearest(foe.x, foe.z)
                        local a = if tx and tz then math.atan2(tx - foe.x, tz - foe.z) else math.pi
                        foe.vx, foe.vz = math.sin(a) * 34, math.min(-12, math.cos(a) * 34)
                    end
                else
                    foe.x += foe.vx * dt
                    foe.z += foe.vz * dt
                    gone = math.abs(foe.x - CX) > HALF_W + 6
                end
            elseif foe.kind == "sweeper" then
                if foe.vx == 0 then foe.vx = if foe.x > CX then -15 else 15 end
                foe.x += foe.vx * dt
                foe.z -= 8 * dt
                if math.abs(foe.x - CX) > FOE_W then
                    foe.x = math.clamp(foe.x, CX - FOE_W, CX + FOE_W)
                    foe.vx = -foe.vx
                end
                if foe.t >= foe.gun then
                    foe.gun = foe.t + 1.1 / haste
                    shoot(foe.x, foe.z - 3, 0, -24, 0)
                end
            elseif foe.kind == "plated" then
                foe.x, foe.z, gone = PATHS[foe.path](foe.t, foe.side)
                if foe.t >= foe.gun and foe.z < CZ + HALF_H then
                    foe.gun = foe.t + 3.2 / haste
                    aimed(foe.x, foe.z, 22, 0, 1)
                end
            elseif foe.kind == "gunship" then
                foe.z = if foe.t < 12 then math.max(CZ + 16, foe.z - 9 * dt) else foe.z - 14 * dt
                foe.x = math.clamp(foe.x0 + 6 * math.sin(foe.t * 0.6), CX - FOE_W, CX + FOE_W)
                if foe.t >= foe.gun and foe.t < 12 then
                    foe.gun = foe.t + 0.8 / haste
                    for i = 0, 3 do
                        local a = foe.t * 0.7 + i * math.pi / 2
                        shoot(foe.x, foe.z, math.sin(a) * 16, math.cos(a) * 16, 0)
                    end
                end
            elseif foe.kind == "carrier" then
                local b = boss
                foe.z = math.max(CZ + 6, foe.z - 7 * dt)
                foe.x = CX + 8 * math.sin(foe.t * 0.25)
                if b then b.x, b.z = foe.x, foe.z end
                -- Half gone, it launches and fires faster.
                local rage = foe.hp < foe.max / 2
                if foe.t >= foe.launch and foe.z < CZ + 30 then
                    foe.launch = foe.t + (if rage then 1.0 else 1.7) / more(0.5)
                    launch_x, launch_z = foe.x, foe.z - 16     -- after this loop: nothing is added to `foes` inside it
                end
                if foe.t >= foe.gun and foe.z < CZ + 30 then
                    foe.gun = foe.t + (if rage then 2.2 else 3.2) / haste
                    aimed(foe.x - 5.5, foe.z + 1, 22, 0.2, if rage then 7 else 5)     -- from its island
                end
            elseif foe.kind == "dart" then
                foe.z -= 16 * dt
            elseif foe.kind == "weaver" then
                foe.z -= 10 * dt
                foe.x = math.clamp(foe.x0 + 10 * math.sin(foe.t * 1.6), CX - FOE_W, CX + FOE_W)
                if foe.t >= foe.gun then
                    foe.gun = foe.t + 2.2 / haste
                    aimed(foe.x, foe.z, 22, 0, 1)
                end
            elseif foe.kind == "ace" then
                if foe.t < 9 then
                    foe.z = math.max(CZ + 14, foe.z - 18 * dt)
                    foe.x = math.clamp(foe.x0 + 12 * math.sin(foe.t * 0.9), CX - FOE_W, CX + FOE_W)
                else
                    foe.z -= 20 * dt
                end
                if foe.t >= foe.gun and foe.t < 9 then
                    foe.gun = foe.t + 1.6 / haste
                    aimed(foe.x, foe.z, 26, 0.18, 3)
                end
            elseif foe.kind == "bomber" then
                foe.z = math.max(CZ + 19, foe.z - 8 * dt)
                foe.x = CX + (FOE_W - 9) * math.sin(foe.t * 0.5)
                if foe.t >= foe.gun then
                    foe.gun = foe.t + 1.2 / haste
                    aimed(foe.x, foe.z, 24, 0.22, 5)
                end
            elseif foe.kind == "mega" then
                local b = boss
                foe.z = math.max(CZ + 9, foe.z - 6 * dt)
                foe.x = CX + 5 * math.sin(foe.t * 0.35)
                if b then
                    b.x, b.z = foe.x, foe.z
                    if b.shield <= 0 and clock >= b.up_at then b.shield = b.max end
                end
                -- Half gone, it throws rings as well.
                if foe.hp < foe.max / 2 and foe.t >= foe.gun then
                    foe.gun = foe.t + 3.5 / haste
                    for i = 1, 14 do
                        local a = i / 14 * 2 * math.pi + foe.t
                        shoot(foe.x + math.sin(a) * 3, foe.z + math.cos(a) * 3, math.sin(a) * 15, math.cos(a) * 15, 0)
                    end
                end
            elseif foe.kind == "tank" then
                foe.z -= SCROLL * dt
                local tx, tz = nearest(foe.x, foe.z)
                if tx and tz then core.ecs.set(id, "yaw", math.atan2(tx - foe.x, foe.z - tz)) end
                if foe.t >= foe.gun and foe.z < CZ + HALF_H then
                    foe.gun = foe.t + 2.5 / haste
                    aimed(foe.x, foe.z + 3, 20, 0, 1)
                end
            else   -- one of its turrets: where the hull is, turned on the nearest fighter
                local b = boss
                if b then foe.x, foe.z = b.x + foe.ox, b.z + foe.oz end
                local tx, tz = nearest(foe.x, foe.z)
                if tx and tz then core.ecs.set(id, "yaw", math.atan2(tx - foe.x, foe.z - tz)) end
                if foe.t >= foe.gun and foe.z < CZ + HALF_H then
                    foe.gun = foe.t + (if foe.spread then 2.6 else 2.0) / haste
                    aimed(foe.x, foe.z + 3, if foe.spread then 22 else 26, 0.2, if foe.spread then 3 else 1)
                end
            end
            if gone or foe.z < BOTTOM - (if foe.kind == "tank" then 12 else 0) then
                core.ecs.kill(id)
                foes[id] = nil
            else
                core.ecs.set(id, "x", foe.x)
                core.ecs.set(id, "z", foe.z)
                if TURNS[foe.kind] and not foe.low and (foe.x ~= was_x or foe.z ~= was_z) then
                    core.ecs.set(id, "yaw", math.atan2(foe.x - was_x, was_z - foe.z))
                end
                -- Flown into: a life.
                local k = KINDS[foe.kind]
                for p, f in fliers do
                    local at = here[p]
                    if at and not f.out and not foe.low and not GROUND[foe.kind] and clock >= f.safe and math.abs(at[1] - foe.x) < k.hw and math.abs(at[3] - foe.z) < k.hl then
                        hit(p, f, at[1], at[3])
                    end
                end
            end
        end
        if mode ~= "play" then return end     -- the carrier went down in this tick
        -- The carrier's next plane, while not too many are up.
        if launch_x and launch_z and live < 5 + 12 * more(0.5) then
            local lid = spawn(LAUNCHED[math.random(#LAUNCHED)], launch_x, launch_z, DECK_Y)
            if lid then
                foes[lid].low = true
                core.ecs.set(lid, "yaw", math.pi)     -- nose up the screen
            end
        end

        -- The shots.
        for id, s in shots do
            s.x += s.vx * dt
            s.z += s.vz * dt
            local gone = math.abs(s.x - CX) > HALF_W + 6 or s.z > TOP or s.z < BOTTOM
            if not gone and s.owner > 0 then
                -- A shot covers SHOT_SPEED * dt a tick: the plane's length and that.
                gone = strike(s.x, s.z, SHOT_SPEED * dt * 0.5, 1, s.owner)
            elseif not gone then
                for p, f in fliers do
                    local at = here[p]
                    if at and not f.out and clock >= f.safe and (at[1] - s.x) ^ 2 + (at[3] - s.z) ^ 2 < 1.8 ^ 2 then
                        gone = true
                        hit(p, f, at[1], at[3])
                        break
                    end
                end
            end
            if gone then
                shots[id] = nil
                tell(0, `k {id}`)
            end
            if mode ~= "play" then return end
        end

        -- The missiles: each turns on the plane nearest it.
        for id, m in missiles do
            local best: Foe?, bd = nil, math.huge
            for _, foe in foes do
                local d = (foe.x - m.x) ^ 2 + (foe.z - m.z) ^ 2
                if d < bd and foe.z < TOP then best, bd = foe, d end
            end
            local a, speed = math.atan2(m.vx, m.vz), math.min(38, math.sqrt(m.vx ^ 2 + m.vz ^ 2) + 40 * dt)
            if best then
                local want = math.atan2(best.x - m.x, best.z - m.z)
                local d = (want - a + math.pi) % (2 * math.pi) - math.pi
                a += math.clamp(d, -5 * dt, 5 * dt)
            end
            m.vx, m.vz = math.sin(a) * speed, math.cos(a) * speed
            m.x += m.vx * dt
            m.z += m.vz * dt
            if clock - m.born > 4 or math.abs(m.x - CX) > HALF_W + 6 or m.z > TOP or m.z < BOTTOM
                or strike(m.x, m.z, 0.8, 3, m.owner) then
                missiles[id] = nil
                tell(0, `k {id}`)
            end
            if mode ~= "play" then return end
        end

        -- The power-ups, drifting down the screen.
        for id, u in pickups do
            u.z -= 9 * dt
            local gone = u.z < BOTTOM
            for p, f in fliers do
                local at = here[p]
                if not gone and at and not f.out and (at[1] - u.x) ^ 2 + (at[3] - u.z) ^ 2 < 3.6 ^ 2 then
                    gone = true
                    take(p, f, u.kind, u.x, u.z)
                end
            end
            if gone then
                pickups[id] = nil
                tell(0, `k {id}`)
            end
        end

        -- What does not go straight, ten times a second: the missiles, the
        -- beams (each player told which is theirs) and the fortress.
        if ticks % 2 == 0 then
            local parts: { string } = {}
            for id, m in missiles do
                table.insert(parts, string.format("m %d %.1f %.1f %.1f %.1f", id, m.x, m.z, m.vx, m.vz))
            end
            local b = boss
            if b then
                local hull = 0
                for _, foe in foes do
                    if HULLS[foe.kind] then hull = foe.hp / foe.max end
                end
                table.insert(parts, string.format("B %.1f %.1f %d %d", b.x, b.z, if b.max > 0 then math.ceil(b.shield / b.max * 100) else 0, math.ceil(hull * 100)))
            end
            if #parts > 0 or #beams > 0 then
                local common = table.concat(parts, "|")
                for p in here do
                    local text = "t|" .. common
                    for _, l in beams do
                        text ..= string.format("|l %d %.1f %.1f %.1f", if l[1] == p then 1 else 0, l[2], l[3], l[4])
                    end
                    tell(p, text)
                end
            end
        end
    end)
end

-- ================================================================ the player

if drawing then
    local mode, wave, pilots = "attract", 0, 1
    local lives, score, hi = 0, 0, 0
    local phase = "foot"       -- "foot" | "takeoff" | "air"
    local pad, went, asked_up = 1, 0.0, false     -- the take-off: from where, since when, and whether the server was told it ended
    local shots: { [number]: { number } } = {}     -- id -> { x, z, vx, vz, ours }
    local ups: { [number]: { any } } = {}          -- id -> { x, z, kind }
    local missiles: { [number]: { number } } = {}  -- id -> { x, z, vx, vz, when last told }
    local beams: { { number } } = {}               -- { ours, x, z, where it ends }
    local beams_at = -1.0
    local fort: { number }? = nil                  -- the fortress: { x, z, shield, hull, when last told }
    local booms: { { number } } = {}               -- { x, z, size, age }
    local falls: { { any } } = {}                  -- planes going down: { model, x, z, age, how long, which way it spins, drift }
    local splashes: { { number } } = {}            -- where they met the sea: { x, z, size, age }
    local won_at, left_x, left_z = 0.0, CX, CZ     -- the game won: when, and where this fighter was when it left the screen
    local gear = { spd = 0, pow = 1, hom = 0, diag = 0, wing = 0, laser = 0.0, rapid = 0.0, turbo = 0.0 }
    local clock = 0.0
    local flash = 0.0
    local looked = false
    local banner_until = 0.0
    local note, note_until = "", 0.0     -- the last power-up taken, or why no fighter can be boarded
    local safe_until = 0.0     -- nothing hits this fighter until then
    local gun_heard = 0.0      -- when a shot of the fighters' was last heard
    local ids: { [string]: number } = {}

    local NAMES = { sp = "SPEED UP", st = "TURBO", fp = "POWER UP", ft = "RAPID FIRE", hm = "HOMING MISSILE",
                    dg = "DIAGONAL FIRE", lz = "LASER", wm = "WINGMAN" }
    local COLOURS: { [string]: { number } } = { sp = { 60, 220, 90 }, st = { 60, 220, 90 }, fp = { 255, 90, 50 }, ft = { 255, 90, 50 },
                    hm = { 200, 90, 255 }, dg = { 255, 220, 60 }, lz = { 80, 240, 255 }, wm = { 235, 240, 250 } }
    local BRIEF = { st = true, ft = true, lz = true }     -- the ones that run out: they blink

    local function send(text: string)
        if serving then asked(1, text) else core.net.send(0, text) end
    end
    local function model(name: string): number
        local id = ids[name]
        if id == nil then
            id = core.model_id(`shmup_core:{name}`) or 0
            ids[name] = id
        end
        return id
    end

    -- Where a fighter taking off from a pad is, t seconds on, and its nose's tilt.
    local function path(t: number): (number, number, number, number)
        local x = PADS[pad]
        if t < ROLL then
            local u = t / ROLL
            return x, FLOOR, PAD_Z + (LIFT_Z - PAD_Z) * u * u, 0
        end
        local u = math.min(1, (t - ROLL) / CLIMB)
        return x, FLOOR + (FLY - FLOOR) * u * u * (3 - 2 * u), LIFT_Z + (START_Z - LIFT_Z) * (1.5 * u - 0.5 * u * u),
            math.sin(u * math.pi) * 0.35
    end
    -- The game won, t seconds after this fighter left the field: off the top
    -- of the screen faster than anything on it (LEAVE seconds), then down
    -- onto the apron from the sea and along its lane to its pad.
    local LEAVE, GLIDE, RUN = 1.8, 3.0, 3.2
    local function way_home(t: number): (number, number, number, number, number)     -- x, y, z, yaw, tilt
        if t < LEAVE then return left_x, FLY, left_z + 40 * t * t, math.pi, 0 end
        local x = PADS[pad]
        local u = math.min(1, (t - LEAVE) / GLIDE)
        if u < 1 then
            return x, FLOOR + 11 * (1 - u) ^ 2, LIFT_Z + 75 * (1 - u), 0, 0.12 * (1 - u)
        end
        local v = math.min(1, (t - LEAVE - GLIDE) / RUN)
        return x, FLOOR, LIFT_Z - (LIFT_Z - PAD_Z) * (2 * v - v * v), 0, 0
    end

    -- How far the camera has swung from behind the fighter to over the field, 0 to 1.
    local function swing(t: number): number
        local u = math.clamp((t - ROLL) / CLIMB, 0, 1)
        return u * u * (3 - 2 * u)
    end

    local function on_foot()
        if phase ~= "foot" then
            core.top_view_at(CX, FLY, CZ, 0, 180, 70, 1.2)     -- back to the eye
            core.body_model(0, 0)
        end
        phase = "foot"
        table.clear(shots)
        table.clear(missiles)
        table.clear(beams)
    end

    heard = function(text: string)
        local w = string.split(text, " ")
        local what = w[1]
        if what == "t|" or string.sub(text, 1, 2) == "t|" then
            table.clear(beams)
            beams_at = clock
            for _, part in string.split(text, "|") do
                local v = string.split(part, " ")
                if v[1] == "m" then
                    missiles[tonumber(v[2]) :: number] = { tonumber(v[3]) :: number, tonumber(v[4]) :: number,
                                                           tonumber(v[5]) :: number, tonumber(v[6]) :: number, clock }
                elseif v[1] == "l" then
                    table.insert(beams, { tonumber(v[2]) :: number, tonumber(v[3]) :: number, tonumber(v[4]) :: number, tonumber(v[5]) :: number })
                elseif v[1] == "B" then
                    fort = { tonumber(v[2]) :: number, tonumber(v[3]) :: number, tonumber(v[4]) :: number, tonumber(v[5]) :: number, clock }
                end
            end
            return
        end
        if what == "b" and clock - gun_heard > 0.09 then
            -- Heard here and not sent as a sound: seven a second a fighter.
            gun_heard = clock
            core.play_sound("shmup:shot", tonumber(w[3]) or CX, FLY, tonumber(w[4]) or CZ)
        end
        if what == "b" or what == "e" then
            shots[tonumber(w[2]) :: number] = { tonumber(w[3]) :: number, tonumber(w[4]) :: number,
                                                tonumber(w[5]) :: number, tonumber(w[6]) :: number,
                                                if what == "b" then 1 else 0 }
        elseif what == "fall" then
            local big = w[2] == "mega" or w[2] == "bomber"
            table.insert(falls, { w[2], tonumber(w[3]) :: number, tonumber(w[4]) :: number, 0,
                                  if w[2] == "carrier" then 4.0 elseif w[2] == "mega" then 3.0 elseif big then 1.8 else 1.2,
                                  if math.random() < 0.5 then -1 else 1,
                                  if w[2] == "carrier" then 0 else (math.random() * 2 - 1) * 6 })
        elseif what == "u" then
            ups[tonumber(w[2]) :: number] = { tonumber(w[4]) :: number, tonumber(w[5]) :: number, w[3] }
        elseif what == "k" then
            local id = tonumber(w[2]) :: number
            shots[id], ups[id], missiles[id] = nil, nil, nil
        elseif what == "boom" then
            table.insert(booms, { tonumber(w[2]) :: number, tonumber(w[3]) :: number, tonumber(w[4]) :: number, 0 })
        elseif what == "hit" then
            flash = 0.5
            safe_until = clock + 2.5
        elseif what == "go" then
            phase, pad, went, asked_up = "takeoff", tonumber(w[2]) or 1, clock, false
        elseif what == "air" then
            if phase == "foot" then went = clock - ROLL - CLIMB end     -- arrived with a fighter already up
            phase = "air"
            safe_until = clock + 2
            core.top_view_at(CX, FLY, CZ, DIST, 180, 70, 0)
            core.body_model(0, model("fighter"))
        elseif what == "down" then
            on_foot()
        elseif what == "wait" then
            note, note_until = "The game is still going: wait for it to end", clock + 3
        elseif what == "got" then
            note, note_until = NAMES[w[2]] or "", clock + 2
        elseif what == "ld" then
            gear.spd, gear.pow, gear.hom, gear.diag, gear.wing = tonumber(w[2]) or 0, tonumber(w[3]) or 1,
                tonumber(w[4]) or 0, tonumber(w[5]) or 0, tonumber(w[6]) or 0
            gear.laser, gear.rapid, gear.turbo = clock + (tonumber(w[7]) or 0), clock + (tonumber(w[8]) or 0),
                clock + (tonumber(w[9]) or 0)
        elseif what == "st" then
            if w[2] ~= mode or tonumber(w[3]) ~= wave then banner_until = clock + 2.5 end
            if w[2] == "won" and mode ~= "won" then won_at = clock end
            mode, wave, pilots = w[2], tonumber(w[3]) or 0, tonumber(w[4]) or 1
            if mode ~= "play" then
                table.clear(shots)
                table.clear(ups)
                table.clear(missiles)
                fort = nil
            end
        elseif what == "me" then
            lives, score, hi = tonumber(w[2]) or 0, tonumber(w[3]) or 0, tonumber(w[4]) or 0
        end
    end
    if not serving then
        core.event.on("core:net_message", function(ev) heard(ev.text) end)
    end

    core.event.on("core:tick", function(ev)
        local dt = ev.dt
        clock += dt
        if not looked then
            looked = true
            core.engine_hud(false)
            send("hello")
        end
        if phase == "takeoff" then
            -- The camera, asked again every frame: behind the fighter down
            -- the apron, then up and over as it climbs, to end exactly where
            -- core.top_view_at holds it.
            local t = clock - went
            local x, y, z = path(t)
            local k = swing(t)
            core.fixed_view(x + (CX - x) * k, y + 1.5 + (FLY - y - 1.5) * k, z + 4 + (CZ - z - 4) * k,
                13 + (DIST - 13) * k, 180, 16 + (89 - 16) * k, 70, 0.8)
            if t >= ROLL + CLIMB and not asked_up then
                asked_up = true
                send("up")
            end
        end
        if phase == "air" and mode == "won" and clock - won_at >= 2.5 then
            -- From here the fighter on the screen is drawn, not flown.
            local x, _, z = core.player_pos(1)
            phase, left_x, left_z = "land", x or CX, z or CZ
            core.play_sound("shmup:engine", PADS[pad], FLOOR, LIFT_Z)
        end
        if phase == "land" then
            local t = clock - won_at - 2.5
            if t < LEAVE then
                core.fixed_view(CX, FLY, CZ, DIST, 180, 89, 70, 0)     -- as core.top_view_at had it
            else
                local x, y, z = way_home(t)
                core.fixed_view(x, y + 1.5, z - 4, 13, 0, 12, 70, 0)   -- behind it, looking at the hangar
            end
        end
        flash = math.max(0, flash - dt)
        for i = #falls, 1, -1 do
            local f = falls[i]
            f[4] += dt
            f[2] += f[7] * dt
            if f[4] >= f[5] then
                table.insert(splashes, { f[2], f[3], if f[5] > 1.5 then 12 else 5, 0 })
                table.remove(falls, i)
            end
        end
        for i = #splashes, 1, -1 do
            splashes[i][4] += dt
            if splashes[i][4] > 0.7 then table.remove(splashes, i) end
        end
        for id, s in shots do
            s[1] += s[3] * dt
            s[2] += s[4] * dt
            if math.abs(s[1] - CX) > HALF_W + 8 or s[2] > TOP + 2 or s[2] < BOTTOM - 2 then shots[id] = nil end
        end
        for id, u in ups do
            -- An islet goes by with the sea, and is gone under the picture's bottom edge.
            u[2] -= (if u[3] == "isle" then 12 else 9) * dt
            if u[2] < (if u[3] == "isle" then CZ - 52 else BOTTOM - 2) then ups[id] = nil end
        end
        for id, m in missiles do
            m[1] += m[3] * dt
            m[2] += m[4] * dt
            if clock - m[5] > 0.4 then missiles[id] = nil end
        end
        if fort and clock - fort[5] > 0.5 then fort = nil end
        for i = #booms, 1, -1 do
            booms[i][4] += dt
            if booms[i][4] > 0.5 then table.remove(booms, i) end
        end
    end)

    -- Beside a parked fighter: its pad, or nothing.
    local function pad_near(): number?
        local x, _, z = core.player_pos(1)
        if x == nil then return nil end
        for i, px in PADS do
            if (x - px) ^ 2 + (z - PAD_Z) ^ 2 < 7 ^ 2 then return i end
        end
        return nil
    end

    core.event.on("core:input", function(ev)
        if ev.action == "key_space" then
            if phase == "air" then send("f1") end
        elseif ev.action == "key_space_up" then
            send("f0")
        elseif ev.action == "key_f" or (ev.action == "key_e" and phase == "air") then
            -- E is the bag's key on foot, and the engine opens it whatever a mod says.
            if phase == "air" then
                send("quit")
            elseif phase == "foot" and pad_near() then
                send("board")
            end
        elseif ev.action == "mouse_left" or ev.action == "mouse_right" then
            ev.cancel = true
        end
    end)

    local function hash(n: number): number
        local s = math.sin(n * 127.1 + 311.7) * 43758.5453
        return s - math.floor(s)
    end

    -- An island going by: foam round it, sand, green and a few trees.
    local function island(k: number, x: number, z: number, r: number, bare: boolean?)
        core.render.box(x, 21.06, z, 2 * r + 2, 0.1, 2 * r, 150, 200, 235, 255)
        core.render.box(x, 21.06, z, 2 * r, 0.1, 2 * r + 2, 150, 200, 235, 255)
        core.render.box(x, 21.3, z, 2 * r, 0.5, 2 * r - 2, 222, 200, 130, 255)
        core.render.box(x, 21.3, z, 2 * r - 2, 0.5, 2 * r, 222, 200, 130, 255)
        if r > 4 then
            core.render.box(x, 21.7, z, 2 * r - 3, 0.5, 2 * r - 5, 60, 150, 70, 255)
            core.render.box(x, 21.7, z, 2 * r - 5, 0.5, 2 * r - 3, 60, 150, 70, 255)
            for j = 1, if bare then 0 else r - 3 do
                core.render.box(x + (hash(k * 7 + j) - 0.5) * (2 * r - 7), 22.6, z + (hash(k * 13 + j) - 0.5) * (2 * r - 7),
                    1.6, 1.4, 1.6, 40, 118, 52, 255)
            end
        end
    end

    -- The hangar's fighters; and, for whoever flies, the sea going by under
    -- the field, which stands still -- crests, islands and clouds, the same
    -- 12 a second -- then the power-ups, the shots and the bursts.
    core.render.pass("shmup:field", 100, function()
        local fighter = model("fighter")
        for i, px in PADS do
            if phase == "foot" or i ~= pad then core.render.model(fighter, px, FLOOR, PAD_Z, math.pi) end
        end
        if phase == "foot" then return end
        local t = clock - went
        if phase == "takeoff" then
            local x, y, z, tilt = path(t)
            core.render.model(fighter, x, y, z, math.pi, 1, tilt)
        elseif phase == "land" then
            local x, y, z, yaw, tilt = way_home(clock - won_at - 2.5)
            core.render.model(fighter, x, y, z, yaw, 1, tilt)
        end

        -- Nothing is over the sea when the wheels leave the ground: the
        -- crests come up out of it, the islands and clouds in from the top.
        -- Nor on the way down to the apron: what goes by is under the field.
        if phase == "land" and clock - won_at - 2.5 >= LEAVE then return end
        local scroll = math.max(0, t - ROLL) * 12
        local shown = math.floor(255 * math.clamp((t - ROLL) / 1.5, 0, 1))
        local span = 2 * HALF_H + 40
        for i = 0, 39 do
            local x = CX + ((i * 37) % 97 - 48) * 1.5
            local z = CZ + HALF_H + 20 - (i * 11.3 + scroll) % span
            core.render.box(x, 21.2, z, 5 + i % 4 * 2, 0.1, 0.5, 170, 210, 240, shown)
        end
        for k = math.max(1, math.floor((scroll - 130) / 46)), math.floor((scroll + 30) / 46) + 1 do
            local r = 4 + math.floor(hash(k + 0.25) * 5)
            local z = CZ + 60 + k * 46 + hash(k) * 16 - scroll
            -- Gone once all of it is under the picture's bottom edge, where the base is.
            if z + r + 1 > CZ - 43 and z - r - 1 < CZ + 78 then
                -- The carrier's water is kept clear: its wave's islands go by either side of it.
                local x = CX + (hash(k + 0.5) - 0.5) * 120
                if wave == 20 and math.abs(x - CX) < 26 then x = CX + (if x < CX then -26 else 26) end
                island(k, x, z, r)
            end
        end
        for k = math.max(1, math.floor((scroll * 1.4 - 130) / 31)), math.floor((scroll * 1.4 + 30) / 31) + 1 do
            local x, z = CX + (hash(k + 0.7) - 0.5) * 130, CZ + 60 + k * 31 - scroll * 1.4
            if z > CZ - 50 and z < CZ + 70 then
                core.render.box(x, 26, z, 9, 0.6, 5, 245, 248, 252, 255)
                core.render.box(x + 3, 26.4, z + 1, 6, 0.6, 3.5, 245, 248, 252, 255)
            end
        end

        -- This machine's own fighter while nothing can hit it: its wing tips blink.
        local px, _, pz = core.player_pos(1)
        if px ~= nil and phase == "air" and mode == "play" and clock < safe_until and clock % 0.2 < 0.1 then
            core.render.box(px - 4.4, FLY + 1.6, pz, 0.9, 0.3, 1.4, 60, 220, 255, 255)
            core.render.box(px + 4.4, FLY + 1.6, pz, 0.9, 0.3, 1.4, 60, 220, 255, 255)
        end
        -- With others flying, which fighter is this machine's: a light at its tail.
        if px ~= nil and phase == "air" and pilots > 1 then
            core.render.box(px, FLY + 1.6, pz - 4.4, 1.2, 0.3, 1.2, 60, 220, 255, 255)
        end
        -- A plane shot down: over on its side, round and nose down, smaller
        -- and smoking, into the sea -- where it leaves a ring.
        for _, f in falls do
            local u = f[4] / f[5]
            if f[1] == "carrier" then
                -- A ship goes down where it is, by the stern, burning.
                core.render.model(model("carrier"), f[2], 21 - 7 * u * u, f[3], math.pi, 1, -0.25 * u, f[6] * 0.2 * u)
                for i = -1, 1 do
                    core.render.fx("smoke", f[2], 26, f[3] + i * 14, 7, 50, 50, 55, 220, (f[4] * 2 + i * 0.3) % 1)
                    core.render.fx("flame", f[2], 25, f[3] + i * 14, 5 * (1 - u), 255, 150, 50, 255, (f[4] * 3 + i * 0.3) % 1)
                end
                continue
            end
            local y = FLY - (FLY - 21.5) * u * u
            local yaw = (if f[1] == "fighter" or f[1] == "wingman" then math.pi else 0) + f[6] * u * 1.5
            core.render.model(model(f[1]), f[2], y, f[3], yaw, 1 - 0.4 * u, -0.6 * u, f[6] * u * 5)
            core.render.fx("smoke", f[2], y + 2, f[3], (if f[5] > 1.5 then 5 else 2.5) * (0.6 + u), 50, 50, 55, 220, (f[4] * 2.5) % 1)
            core.render.fx("flame", f[2], y + 1, f[3], if f[5] > 1.5 then 4 else 1.8, 255, 150, 50, 255, (f[4] * 3) % 1)
        end
        for _, sp in splashes do
            local u = sp[4] / 0.7
            core.render.fx("ring", sp[1], 21.3, sp[2], sp[3] * (0.5 + u), 235, 245, 255, 255, u)
            core.render.box(sp[1], 21.3, sp[2], sp[3] * 0.5 * (1 - u), 0.2, sp[3] * 0.5 * (1 - u), 235, 245, 255, math.floor(255 * (1 - u)))
        end
        for _, u in ups do
            if u[3] == "isle" then
                island(0, u[1], u[2], 6, true)
                continue
            end
            local c = COLOURS[u[3]] or COLOURS.wm
            local lit = BRIEF[u[3]] and clock % 0.3 < 0.15
            core.render.box(u[1], FLY + 1, u[2], 3.0, 0.5, 3.0, 20, 22, 34, 255)
            core.render.box(u[1], FLY + 1.3, u[2], 2.2, 0.5, 2.2, if lit then 255 else c[1], if lit then 255 else c[2], if lit then 255 else c[3], 255)
            core.render.fx("glow", u[1], FLY + 1.6, u[2], 3.5, c[1], c[2], c[3], 160, 0.3)
        end
        for _, s in shots do
            if s[5] == 1 then
                core.render.box(s[1], FLY + 1, s[2], 0.45, 0.45, 2.4, 255, 236, 90, 255)
            else
                core.render.box(s[1], FLY + 1, s[2], 1.1, 1.1, 1.1, 255, 90, 60, 255)
                core.render.box(s[1], FLY + 1, s[2], 0.6, 1.3, 0.6, 255, 230, 200, 255)
            end
        end
        for _, m in missiles do
            core.render.box(m[1], FLY + 1, m[2], 0.8, 0.6, 0.8, 250, 250, 250, 255)
            core.render.fx("jet", m[1], FLY + 1, m[2], 0.7, 255, 170, 70, 255, 0, m[1] - m[3] * 0.07, FLY + 1, m[2] - m[4] * 0.07)
        end
        if clock - beams_at < 0.25 then
            for _, l in beams do
                local x, z = l[2], l[3]
                if l[1] == 1 and px ~= nil then x, z = px, pz + 3 end     -- ours: from where this machine has the fighter
                local len = math.max(0.5, l[4] - z)
                core.render.box(x, FLY + 1, z + len / 2, 1.8, 0.2, len, 80, 240, 255, 110)
                core.render.box(x, FLY + 1.1, z + len / 2, 0.7, 0.3, len, 220, 255, 255, 255)
                core.render.fx("glow", x, FLY + 1, z + len, 2.5, 120, 240, 255, 255, 0.4)
            end
        end
        local f = fort
        if f and f[3] > 0 then
            -- The shield: a ring of lights going round it, fainter as it wears down.
            local a = math.floor(90 + 165 * f[3] / 100)
            for i = 1, 32 do
                local b = i / 32 * 2 * math.pi + clock * 0.6
                core.render.box(f[1] + math.sin(b) * SHIELD_R, FLY + 3, f[2] + math.cos(b) * SHIELD_R, 1.3, 0.4, 1.3, 90, 200, 255, a)
            end
            core.render.fx("glow", f[1], FLY + 4, f[2], SHIELD_R, 60, 150, 255, math.floor(a / 3), 0.5)
        end
        for _, b in booms do
            local u = b[4] / 0.5
            core.render.fx("glow", b[1], FLY + 1, b[2], b[3] * (0.4 + u), 255, 170, 60, 255, u)
        end
    end)

    local function digits(n: number): string
        return string.format("%06d", n)
    end

    core.hud.pass("shmup:hud", 100, function()
        core.hud.text(0.06, 0.035, "1UP", 0.035, 0, 255, 60, 60, 255)
        core.hud.text(0.06, 0.08, digits(score), 0.04, 0, 255, 255, 255, 255)
        core.hud.text(0.5, 0.035, "HI-SCORE", 0.035, 1, 255, 60, 60, 255)
        core.hud.text(0.5, 0.08, digits(hi), 0.04, 1, 255, 255, 255, 255)
        if clock < note_until then
            core.hud.text(0.5, 0.72, note, 0.045, 1, 120, 255, 160, 255)
        end
        if mode == "over" then
            core.hud.text(0.5, 0.42, "GAME OVER", 0.1, 1, 255, 60, 60, 255)
        elseif mode == "won" then
            core.hud.text(0.5, 0.4, "MISSION COMPLETE", 0.09, 1, 255, 220, 60, 255)
        end
        if phase == "foot" then
            if mode == "attract" then
                core.hud.text(0.5, 0.2, "ACE OF THE SKIES", 0.08, 1, 255, 220, 60, 255)
            elseif mode == "play" then
                core.hud.text(0.5, 0.2, if lives <= 0 and score > 0 then "SHOT DOWN" else `WAVE {wave}`, 0.06, 1, 255, 240, 120, 255)
            end
            if pad_near() then
                if clock % 1 < 0.7 then core.hud.text(0.5, 0.6, "press F to take off", 0.045, 1, 255, 255, 255, 255) end
            else
                core.hud.text(0.5, 0.6, "walk up to a fighter", 0.035, 1, 200, 230, 255, 255)
            end
            core.hud.text(0.5, 0.95, "W A S D move   SPACE fire   F back to the hangar", 0.024, 1, 255, 255, 255, 200)
        elseif mode == "play" then
            core.hud.text(0.94, 0.035, `WAVE {wave}`, 0.035, 2, 120, 220, 255, 255)
            core.hud.text(0.94, 0.075, `LEVEL {(wave - 1) // 10 + 1}`, 0.028, 2, 120, 220, 255, 255)
            if pilots > 1 then core.hud.text(0.94, 0.11, `PILOTS {pilots}`, 0.028, 2, 255, 220, 60, 255) end
            core.hud.text(0.06, 0.95, `LIVES {lives}`, 0.04, 0, 255, 220, 60, 255)
            -- What the fighter carries.
            local y = 0.95
            local function line(text: string, r: number, g: number, b: number)
                core.hud.text(0.94, y, text, 0.028, 2, r, g, b, 255)
                y -= 0.035
            end
            if gear.laser > clock then line(`LASER {math.ceil(gear.laser - clock)}`, 80, 240, 255) end
            if gear.rapid > clock then line(`RAPID {math.ceil(gear.rapid - clock)}`, 255, 150, 90) end
            if gear.turbo > clock then line(`TURBO {math.ceil(gear.turbo - clock)}`, 120, 255, 150) end
            if gear.wing > 0 then line(`WINGMEN {gear.wing}`, 235, 240, 250) end
            if gear.diag > 0 then line(`DIAGONAL {gear.diag}`, 255, 220, 60) end
            if gear.hom > 0 then line(`MISSILES {gear.hom}`, 200, 90, 255) end
            line(`POWER {gear.pow}`, 255, 90, 50)
            line(`SPEED {gear.spd + 1}`, 60, 220, 90)
            if clock < banner_until and phase == "air" then
                core.hud.text(0.5, 0.4, if wave == 20 then "WARNING: AIRCRAFT CARRIER" elseif wave == 10 then "WARNING: FLYING FORTRESS"
                    elseif wave % 5 == 0 then "WARNING: BOMBER" elseif wave == 11 then "LEVEL 2" else `WAVE {wave}`,
                    0.07, 1, 255, 240, 120, 255)
            end
            local f = fort
            if f then
                core.hud.rect(0.5, 0.135, 0.42, 0.02, 20, 22, 34, 200)
                core.hud.rect(0.5, 0.13, 0.4 * f[4] / 100, 0.008, 255, 70, 50, 255)
                core.hud.rect(0.5, 0.142, 0.4 * f[3] / 100, 0.006, 90, 200, 255, 255)
            end
        end
        if flash > 0 then
            core.hud.rect(0.5, 0.5, 4, 1, 255, 40, 20, math.floor(120 * flash / 0.5))
        end
    end)
end
