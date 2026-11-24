--!strict
-- defense_game: the tower defense, alone or together (games/defense).
--
-- Monsters of the other worlds come through the rift at the field's north
-- end and walk to the crystal at its south end; every one that reaches it
-- takes from it (a boss ten) and the game is over when it has nothing left.
-- Players stop them three ways:
--   * turrets, built at the benches by the doorway out of what was mined
--     (the bag's ingots, gems, ice, the Inferno's cinder rods) and put down
--     anywhere on the field; a right click on one upgrades it, up to level 3;
--   * barricades, two high, which the monsters walk round: a field of them
--     makes a longer way past more turrets. One that would shut the way
--     altogether is taken back;
--   * their own weapons: a monster that has a player close fights them.
-- A right click on the crystal starts a game, or the next wave at once. Each
-- wave has more monsters, and tougher; every fifth a boss of another world
-- (the arena's Guardian, then the crypt's Butcher). Every wave won gives each
-- player there materials and barricades: what turrets are made of, kept when
-- they leave. More players, more monsters and more health. The best wave is
-- kept per player and for the field: /defense shows them.

local G = 20                       -- as defense_core: feet at G + 1
local X0, X1, Z0, Z1 = 0, 48, 0, 72
local CX = 24
local CRYSTAL_Z = 6
local GOAL = { CX, G + 1, CRYSTAL_Z + 3 }      -- where a monster is "at the crystal"
local GATE = { CX, G + 1, 67 }                  -- where they come out
local BUILD_Z0, BUILD_Z1 = 11, 64               -- where turrets and barricades may stand


-- ------------------------------------------------------------ sides
--
-- The game is the server's. On a player's machine (side = both) the turrets
-- are drawn alive: each turned to its last target and easing back, kicked as
-- it fires, an idle glow or flame of its own, and what it fires seen on its
-- way -- an arrow's streak, a burst of fire, frost on the ground, lightning
-- leaping from one monster to the next, a portal's swirl at both ends. The
-- block under a turret is invisible (core.node_glass): it is only what stops
-- bodies and takes the clicks.

local side = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"
local heard: (string) -> () = function(_) end
local function tell(p: number, text: string)
    if drawing then heard(text) else core.net.send(p, text) end
end

if drawing then
    type Seen = { kind: string, lvl: number, x: number, y: number, z: number, yaw: number, aim: number,
                  kick: number, last: number }
    type Shot = { kind: string, pts: { number }, t0: number, T: number }
    local seen_turrets: { [string]: Seen } = {}
    local shots: { Shot } = {}
    local clock = 0.0
    local models: { [string]: number } = {}
    -- How far up each one's business end is.
    local TOP: { [string]: number } = { arrow = 1.2, brazier = 1.1, frost = 1.3, lightning = 1.35, portal = 0.85 }
    -- The two that aim turn their top only, over a base that stands still
    -- (tools/make_turret_parts.py), and how much more their top is turned than
    -- the aim (both are drawn facing it; checked from above). The brazier,
    -- the frost and the lightning turret strike all round them and stand
    -- still, whole.
    local TURNS: { [string]: number } = { arrow = 0, portal = 0 }

    local function nums(text: string): { number }
        local out = {}
        for n in string.gmatch(text, "%S+") do table.insert(out, tonumber(n) or 0) end
        return out
    end

    heard = function(text: string)
        local k = string.sub(text, 1, 1)
        if k == "C" then
            seen_turrets = {}
        elseif k == "T" then
            local key, kind, lvl = string.match(text, "^T (%S+) (%a+) (%d)$")
            if key then
                local x, y, z = string.match(key :: string, "^(%-?%d+),(%-?%d+),(%-?%d+)$")
                local old = seen_turrets[key :: string]
                seen_turrets[key :: string] = { kind = kind :: string, lvl = tonumber(lvl) :: number,
                    x = tonumber(x) :: number, y = tonumber(y) :: number, z = tonumber(z) :: number,
                    yaw = if old then old.yaw else 0, aim = if old then old.aim else 0, kick = 0, last = -99 }
            end
        elseif k == "R" then
            seen_turrets[string.sub(text, 3)] = nil
        elseif k == "F" then
            -- F kind x y z, then the points it strikes (x y z each)
            local kind, rest = string.match(text, "^F (%a+) (.*)$")
            if kind == nil then return end
            local v = nums(rest :: string)
            local t = seen_turrets[`{v[1]},{v[2]},{v[3]}`]
            if t and #v >= 6 then
                t.aim = math.atan2(-(v[4] - v[1] - 0.5), -(v[6] - v[3] - 0.5))
                t.kick, t.last = 1, clock
            end
            local T = if kind == "arrow" then 0.22 elseif kind == "portal" then 0.8 else 0.45
            table.insert(shots, { kind = kind :: string, pts = v, t0 = clock, T = T })
        end
    end
    if not serving then
        core.event.on("core:net_message", function(ev) heard(ev.text :: string) end)
    end
    core.event.on("core:tick", function(ev)
        clock += ev.dt
        for _, t in seen_turrets do
            t.kick = math.max(0, t.kick - ev.dt * 5)
            -- Turned to the last target, easing; long without one, the crossbow
            -- looks slowly round.
            if clock - t.last > 4 and t.kind == "arrow" then
                t.aim = t.aim + ev.dt * 0.4
            end
            local d = (t.aim - t.yaw + math.pi) % (2 * math.pi) - math.pi
            t.yaw += d * math.min(1, ev.dt * 8)
        end
    end)

    local function model(name: string): number
        name = `defense_game:turret_{name}`
        local m = models[name]
        if m == nil or m == 0 then
            m = core.model_id(name)
            models[name] = m
        end
        return m :: number
    end

    core.render.pass("defense:turrets", 65, function()
        for _, t in seen_turrets do
            local x, y, z = t.x + 0.5, t.y, t.z + 0.5
            local top = y + (TOP[t.kind] or 1.2)
            local scale = 1 + 0.08 * (t.lvl - 1)
            local turn = TURNS[t.kind]
            if turn then
                core.render.model(model(`{t.kind}_base`), x, y, z, 0, scale)
                core.render.model(model(`{t.kind}_top`), x, y, z, t.yaw + turn, scale, -0.12 * t.kick)
            else
                core.render.model(model(t.kind), x, y, z, 0, scale)
            end
            local pulse = 0.5 + 0.5 * math.sin(clock * 3 + z)
            if t.kind == "brazier" then
                core.render.fx("flame", x, top, z, 0.35 + 0.1 * t.lvl + 0.3 * t.kick, 255, 140, 40, 230, (clock * 1.3 + x) % 1)
            elseif t.kind == "frost" then
                core.render.fx("glow", x, top, z, 0.5 + 0.2 * pulse, 150, 220, 255, 200, 0.5)
            elseif t.kind == "lightning" then
                core.render.fx("glow", x, top, z, 0.4 + 0.3 * pulse + 0.4 * t.kick, 255, 80, 90, 220, 0.5)
                if math.random() < 0.08 then
                    local a = math.random() * 6.28
                    core.render.fx("bolt", x, top, z, 0.06, 255, 220, 230, 230, 0.5, x + math.cos(a) * 0.5,
                                   top + 0.3, z + math.sin(a) * 0.5)
                end
            elseif t.kind == "portal" then
                core.render.fx("vortex", x, y + 0.85, z, 0.35, 90, 150, 255, 220, (clock * 0.7) % 1)
            end
        end
        for i = #shots, 1, -1 do
            local s = shots[i]
            local u = (clock - s.t0) / s.T
            if u > 1.5 then
                table.remove(shots, i)
                continue
            end
            local p = s.pts
            if #p < 6 then continue end
            local x, y, z = p[1] + 0.5, p[2] + (TOP[s.kind] or 1.2), p[3] + 0.5
            local tx, ty, tz = p[4], p[5] + 1, p[6]
            if s.kind == "arrow" then
                if u <= 1 then
                    local ax, ay, az = x + (tx - x) * u, y + (ty - y) * u + math.sin(u * math.pi) * 0.6, z + (tz - z) * u
                    local bu = math.max(0, u - 0.35)
                    core.render.fx("streak", x + (tx - x) * bu, y + (ty - y) * bu, z + (tz - z) * bu, 0.08,
                                   255, 230, 170, 255, 0.5, ax, ay, az)
                else
                    core.render.fx("sparks", tx, ty, tz, 0.4, 255, 220, 150, 230, (u - 1) * 2)
                end
            elseif s.kind == "brazier" then
                if u <= 1 then
                    for k = 7, #p, 3 do
                        core.render.fx("flame", p[k], p[k + 1] + 0.3, p[k + 2], 0.6, 255, 120, 30, 230, u)
                    end
                    core.render.fx("ring", x, p[2] + 0.05, z, 1 + u * 3.5, 255, 140, 40, 200, u)
                    core.render.fx("flame", tx, ty, tz, 0.7, 255, 110, 30, 240, u)
                end
            elseif s.kind == "frost" then
                if u <= 1 then
                    core.render.fx("frost", x, p[2] + 0.05, z, 1 + u * 5, 170, 225, 255, 220, u)
                    core.render.fx("ring", x, p[2] + 0.1, z, 1 + u * 6, 180, 230, 255, 180, u)
                end
            elseif s.kind == "lightning" then
                if u <= 1 then
                    local fx, fy, fz = x, y, z
                    for k = 4, #p - 2, 3 do
                        core.render.fx("bolt", fx, fy, fz, 0.12, 255, 210, 230, 255, u, p[k], p[k + 1] + 1, p[k + 2])
                        fx, fy, fz = p[k], p[k + 1] + 1, p[k + 2]
                    end
                end
            elseif s.kind == "portal" then
                if u <= 1 then
                    core.render.fx("vortex", tx, ty, tz, 0.6 + 0.6 * (1 - u), 255, 150, 60, 230, u)
                    if #p >= 9 then core.render.fx("vortex", p[7], p[8] + 1, p[9], 0.4 + 0.8 * u, 90, 150, 255, 230, u) end
                end
            end
        end
    end)
end

if not serving then return end
core.net.open()

local CRYSTAL = core.node_id("defense_core:crystal")
local BENCH = core.node_id("defense_core:bench")
local NEVER = "defense_game:never"

-- The Inferno's materials, known here so a bag carried in shows them.
core.register_item("inferno:cinder_rod", 64, "", 0, 1.0, "")

-- ------------------------------------------------------------ blocks

local BARRICADE = core.register_node("defense_game:barricade", 150, 110, 60, true, 0, "", 0, 0.6, "wood")
local BARRICADE_TOP = core.register_node("defense_game:barricade_top", 150, 110, 60, true, 0, "", 0, 0.6, "wood")
core.node_drop(BARRICADE_TOP, BARRICADE)
-- Over every turret, a cell nobody sees and nothing goes through: a turret is
-- one block, and a monster would step up on it and over a barricade.
local CAP = core.register_node("defense_game:cap", 255, 255, 255, true, 0, "", 0, 999, "")
core.node_glass(CAP, 0)

type Kind = { name: string, title: string, build: { { any } }, up: { { { any } } },
              range: { number }, every: { number } }
-- Turrets (tools/make_sky_creatures.py's neighbour: Meshy concept, image to
-- 3D, tools/meshy_prop.py at 1.4 m). `build` what one costs at a bench, `up`
-- what level 2 and level 3 cost, `range` and `every` by level.
local KINDS: { Kind } = {
    { name = "arrow", title = "Arrow turret",
      build = { { "timber:planks", 4 }, { "furnace:iron_ingot", 2 }, { "rocks:cobblestone", 4 } },
      up = { { { "furnace:iron_ingot", 2 } }, { { "furnace:iron_ingot", 4 } } },
      range = { 10, 11, 12 }, every = { 0.8, 0.65, 0.5 } },
    { name = "brazier", title = "Fire brazier",
      build = { { "rocks:cobblestone", 6 }, { "inferno:cinder_rod", 2 }, { "furnace:iron_ingot", 2 } },
      up = { { { "inferno:cinder_rod", 2 } }, { { "inferno:cinder_rod", 3 } } },
      range = { 3.5, 4, 4.5 }, every = { 1.0, 1.0, 1.0 } },
    { name = "frost", title = "Frost turret",
      build = { { "rocks:ice", 4 }, { "furnace:iron_ingot", 2 } },
      up = { { { "rocks:ice", 3 } }, { { "rocks:ice", 5 } } },
      range = { 6, 7, 8 }, every = { 1.0, 1.0, 1.0 } },
    { name = "lightning", title = "Lightning turret",
      build = { { "furnace:gold_ingot", 2 }, { "ore:ruby", 1 }, { "furnace:iron_ingot", 2 } },
      up = { { { "ore:ruby", 1 } }, { { "ore:ruby", 2 } } },
      range = { 9, 10, 11 }, every = { 1.6, 1.4, 1.2 } },
    { name = "portal", title = "Portal turret",
      build = { { "ore:diamond", 1 }, { "furnace:gold_ingot", 1 }, { "rocks:cobblestone", 4 } },
      up = { { { "ore:diamond", 1 } }, { { "ore:diamond", 2 } } },
      range = { 5, 6, 7 }, every = { 10, 8, 6 } },
}
local KIND_OF: { [number]: Kind } = {}   -- by block id
for _, k in KINDS do
    local b = core.register_node(`defense_game:turret_{k.name}`, 160, 160, 160, true, 0, "", 0, 1.0, "stone")
    -- The model is drawn by each player's machine, turning and firing (above):
    -- the block itself is seen through, only a body's stop and a click's aim.
    core.register_model(`defense_game:turret_{k.name}`, `turret_{k.name}.gltf`)
    core.node_glass(b, 0)
    if k.name == "arrow" or k.name == "portal" then
        -- Cut in two by tools/make_turret_parts.py: a base that stands still,
        -- the crossbow or the ring over it that turns.
        core.register_model(`defense_game:turret_{k.name}_base`, `turret_{k.name}_base.gltf`)
        core.register_model(`defense_game:turret_{k.name}_top`, `turret_{k.name}_top.gltf`)
    end
    core.node_box(b, 0.1, 0.0, 0.1, 0.9, 1.0, 0.9)
    KIND_OF[b] = k
end

-- ------------------------------------------------------------ monsters

-- Workers (fight "worker"): where they go is this mod's (core.mob_goto), and
-- they fight only whoever this mod sets them on (core.mob_hunt).
type Foe = { title: string, hp: number, speed: number, leak: number, boss: boolean }
local FOES: { [string]: Foe } = {
    zombie   = { title = "zombie", hp = 24, speed = 2.6, leak = 1, boss = false },
    demon    = { title = "demon", hp = 16, speed = 4.2, leak = 1, boss = false },
    cyborg   = { title = "cyborg", hp = 45, speed = 2.4, leak = 2, boss = false },
    guardian = { title = "the Guardian of the arena", hp = 400, speed = 2.0, leak = 10, boss = true },
    butcher  = { title = "the Butcher of the crypt", hp = 600, speed = 2.2, leak = 10, boss = true },
}
for name, f in FOES do
    --                 name                       model                                                       hp    speed    fight    dmg reach every drop n shot doors hover fire  spawn_on
    core.register_mob(`defense_game:{name}`, core.register_model(`defense_game:{name}`, `{name}.gltf`), f.hp, f.speed,
                      "worker", if f.boss then 5 else 2, if f.boss then 2.4 else 1.6, 1.0, "", 0, 0, false, 0, false, NEVER)
end

-- ------------------------------------------------------------ state

type Turret = { kind: Kind, lvl: number, x: number, y: number, z: number, wait: number, mode: string? }
type Monster = { kind: string, foe: Foe, hunting: number, stop: number, hunt_left: number, hp: number }

local phase = "idle"       -- idle, wave, rest, over
local wave = 0
local crystal = 20
local CRYSTAL_MAX = 20
local queue: { string } = {}
local spawn_wait = 0.0
local rest_left = 0.0
local over_left = 0.0
local empty_for = 0.0
local turrets: { [string]: Turret } = {}
local monsters: { [number]: Monster } = {}
local placed: { [string]: { number } } = {}   -- every barricade and turret cell, to clear
local hp_scale = 1.0

local function key(x: number, y: number, z: number): string return `{x},{y},{z}` end
local function f1(v: number): string return string.format("%.1f", v) end
local function tell_turret(p: number, t: { kind: { name: string }, lvl: number, x: number, y: number, z: number })
    tell(p, `T {key(t.x, t.y, t.z)} {t.kind.name} {t.lvl}`)
end

local function everyone(text: string)
    for _, p in core.players() do core.notice(p, text) end
end

local function sound(name: string, x: number, y: number, z: number)
    core.play_sound(`defense_game:{name}`, x, y, z)
end

-- The way from the rift to the crystal, as stops every few cells: a creature
-- sent the whole length at once (core.mob_goto) runs out of search on a field
-- of barricades and stops halfway, so each goes from stop to stop. Made again
-- whenever a barricade or a turret is put down or taken up.
local route: { { number } } = {}
local STOP = 6

local function make_route(): boolean
    local w = core.find_path(GATE[1], GATE[2], GATE[3], GOAL[1], GOAL[2], GOAL[3], 2, 1, 3, false, 12000)
    local n = #w
    if n < 3 or math.abs(w[n - 2] - GOAL[1]) > 1 or math.abs(w[n] - GOAL[3]) > 1 then return false end
    local r: { { number } } = {}
    for i = 1, n // 3, STOP do
        table.insert(r, { w[i * 3 - 2], w[i * 3 - 1], w[i * 3] })
    end
    table.insert(r, { GOAL[1], GOAL[2], GOAL[3] })
    route = r
    return true
end

-- The stop after the one nearest a creature.
local function next_stop(x: number, z: number): number
    local best, d2 = 1, math.huge
    for i, s in route do
        local d = (s[1] + 0.5 - x) ^ 2 + (s[3] + 0.5 - z) ^ 2
        if d < d2 then best, d2 = i, d end
    end
    return math.min(best + 1, #route)
end

local function go(id: number, m: Monster)
    local s = route[m.stop]
    if s then core.mob_goto(id, s[1], s[2], s[3]) end
end

local function send_all()
    for id, m in monsters do
        local x, _, z = core.mob_pos(id)
        if x and m.hunting == 0 then
            m.stop = next_stop(x, z)
            go(id, m)
        end
    end
end

-- ------------------------------------------------------------ the record

local function best_of(name: string): number
    return tonumber(core.storage_get("best:" .. string.lower(name))) or 0
end

local function keep_record()
    if wave <= 0 then return end
    local names = {}
    for _, p in core.players() do
        local n = core.player_name(p)
        table.insert(names, n)
        if wave > best_of(n) then
            core.storage_set("best:" .. string.lower(n), tostring(wave))
            core.notice(p, `Your best: wave {wave}`)
        end
    end
    local field = tonumber(string.match(core.storage_get("best"), "^(%d+)") or "") or 0
    if wave > field then
        core.storage_set("best", `{wave};{table.concat(names, ", ")}`)
        everyone(`A record for the field: wave {wave}`)
    end
end

-- ------------------------------------------------------------ the game

local function clear_field()
    for id in monsters do core.mob_remove(id) end
    monsters = {}
    for _, c in placed do core.set_block(c[1], c[2], c[3], 0) end
    placed, turrets = {}, {}
    tell(0, "C")
    queue, route = {}, {}
end

local function reset()
    clear_field()
    phase, wave, crystal = "idle", 0, CRYSTAL_MAX
end

local function give(p: number, item: string, n: number)
    if n > 0 then core.give_item(p, item, n) end
end

-- What each player has to begin with: barricades every game, and the first
-- time ever enough for an arrow turret (kept by name: a game started again
-- is no well of iron). After that, wave 1's reward buys an arrow turret.
local function kit()
    for _, p in core.players() do
        local k = "kit:" .. string.lower(core.player_name(p))
        if core.storage_get(k) == "" then
            core.storage_set(k, "1")
            give(p, "timber:planks", 8)
            give(p, "furnace:iron_ingot", 4)
            give(p, "rocks:cobblestone", 8)
        end
        give(p, "defense_game:barricade", 24)
        core.notice(p, "The crystal holds. Build at the benches, then click the crystal for the first wave.")
    end
end

local function reward()
    for _, p in core.players() do
        give(p, "furnace:iron_ingot", 2 + wave // 2)
        give(p, "timber:planks", 4)
        give(p, "rocks:cobblestone", 4)
        give(p, "defense_game:barricade", 8)
        if wave >= 2 then give(p, "rocks:ice", 2) end
        if wave >= 3 then give(p, "inferno:cinder_rod", 1 + wave // 4) end
        if wave >= 4 then give(p, "furnace:gold_ingot", 1 + wave // 6) end
        if wave >= 4 and wave % 2 == 0 then give(p, "ore:ruby", 1) end
        if wave % 5 == 0 then give(p, "ore:diamond", 1 + wave // 10) end
        core.notice(p, `Wave {wave} held. Materials in your bag; the next in 25 s.`)
    end
end

local function start_wave()
    if #route == 0 then make_route() end
    wave += 1
    local n = #core.players()
    local more = 1 + 0.5 * math.max(0, n - 1)
    hp_scale = (1.12 ^ (wave - 1)) * (1 + 0.35 * math.max(0, n - 1))
    queue = {}
    local count = math.floor((5 + 2 * wave) * more)
    for i = 1, count do
        local r = (i * 7 + wave * 3) % 10
        local kind = if wave >= 4 and r < 3 then "cyborg" elseif r < 5 then "demon" else "zombie"
        table.insert(queue, kind)
    end
    local boss = ""
    if wave % 5 == 0 then
        boss = if (wave // 5) % 2 == 1 then "guardian" else "butcher"
        table.insert(queue, boss)
    end
    phase, spawn_wait = "wave", 0
    sound("wave", GATE[1], GATE[2], GATE[3])
    everyone(if boss ~= "" then `Wave {wave}: {count} monsters and {FOES[boss].title}`
             else `Wave {wave}: {count} monsters`)
end

local function game_over()
    keep_record()
    sound("over", GOAL[1], GOAL[2], GOAL[3])
    everyone(`The crystal is broken at wave {wave}. Click it to play again.`)
    clear_field()
    phase, over_left = "over", 8
end

local function leak(id: number, m: Monster)
    core.mob_remove(id)
    monsters[id] = nil
    crystal -= m.foe.leak
    sound("hit", GOAL[1], GOAL[2], GOAL[3])
    if crystal <= 0 then
        game_over()
    else
        everyone(`The crystal: {crystal} / {CRYSTAL_MAX}`)
    end
end

-- ------------------------------------------------------------ turrets

-- The monsters within r of a point, the nearest it first: what a turret aims at.
local function foes_near(x: number, y: number, z: number, r: number): { number }
    local out: { number } = {}
    for id, m in monsters do
        local mx, my, mz = core.mob_pos(id)
        if mx and core.mob_kind(id) == m.kind and (mx - x) ^ 2 + (mz - z) ^ 2 <= r * r and math.abs(my - y) < 4 then
            table.insert(out, id)
        end
    end
    local d2: { [number]: number } = {}
    for _, id in out do
        local mx, _, mz = core.mob_pos(id)
        d2[id] = ((mx :: number) - x) ^ 2 + ((mz :: number) - z) ^ 2
    end
    table.sort(out, function(a, b) return d2[a] < d2[b] end)
    return out
end

-- Whom a turret that picks one aims at, set in its panel: the nearest (the
-- default), the furthest, the furthest along the way to the crystal, the
-- furthest back, the one with most health left, the one with least. Health
-- is counted here (core.mob_hurt and players' blows, core:hit): the engine
-- has no call that reads it.
local MODES = { { "near", "Nearest" }, { "far", "Furthest" }, { "first", "Furthest along" },
                { "last", "Furthest back" }, { "strong", "Strongest" }, { "weak", "Weakest" } }
local CHOOSES: { [string]: boolean } = { arrow = true, lightning = true, portal = true }

-- How far along the way a monster is: its stop, less what is left to it.
local function progress(id: number): number
    local m = monsters[id]
    local x, _, z = core.mob_pos(id)
    if m == nil or x == nil then return 0 end
    local st = route[m.stop]
    local d = if st then math.sqrt((st[1] + 0.5 - x) ^ 2 + (st[3] + 0.5 - z) ^ 2) else 0
    return m.stop * 1000 - d
end

-- The list (nearest first, as foes_near gives it) ordered as the mode wants.
local function by_mode(list: { number }, mode: string?): { number }
    if mode == nil or mode == "near" then return list end
    local out = table.clone(list)
    if mode == "far" then
        for i = 1, #out // 2 do out[i], out[#out + 1 - i] = out[#out + 1 - i], out[i] end
        return out
    end
    local val: { [number]: number } = {}
    for _, id in out do
        local m = monsters[id]
        val[id] = if mode == "first" or mode == "last" then progress(id)
                  elseif m then m.hp else 0
    end
    local high = mode == "first" or mode == "strong"
    local rank: { [number]: number } = {}   -- equal ones: the nearer first
    for i, id in out do rank[id] = i end
    table.sort(out, function(a, b)
        if val[a] == val[b] then return rank[a] < rank[b] end
        if high then return val[a] > val[b] end
        return val[a] < val[b]
    end)
    return out
end

-- A turret's blow: the engine's, and the count of health kept here.
local function hurt(id: number, dmg: number, fx: number, fz: number)
    core.mob_hurt(id, dmg, fx, fz)
    local m = monsters[id]
    if m then m.hp -= dmg end
end

core.event.on("core:hit", function(ev)
    local m = monsters[ev.target]
    if m then m.hp -= ev.damage end
end)

local DAMAGE = { arrow = { 5, 7, 10 }, brazier = { 3, 5, 7 }, lightning = { 9, 12, 16 } }
local SLOW = { 0.55, 0.45, 0.35 }

local function fire(t: Turret)
    local x, y, z = t.x + 0.5, t.y, t.z + 0.5
    local r = t.kind.range[t.lvl]
    local near = foes_near(x, y, z, r)
    if #near == 0 then return false end
    local name = t.kind.name
    if CHOOSES[name] then near = by_mode(near, t.mode) end
    local head = `F {name} {t.x} {t.y} {t.z}`
    local function at(id: number): string
        local mx, my, mz = core.mob_pos(id)
        return if mx then ` {f1(mx)} {f1(my)} {f1(mz)}` else ""
    end
    if name == "arrow" then
        tell(0, head .. at(near[1]))
        hurt(near[1], DAMAGE.arrow[t.lvl], x, z)
        sound("arrow", x, y + 1, z)
    elseif name == "brazier" then
        local msg = head .. at(near[1])
        for i, id in near do
            if i <= 6 then msg ..= at(id) end
        end
        tell(0, msg)
        for _, id in near do hurt(id, DAMAGE.brazier[t.lvl], x, z) end
        sound("fire", x, y + 1, z)
    elseif name == "frost" then
        tell(0, head .. at(near[1]))
        core.slow_mobs(x, y, z, r, SLOW[t.lvl], 1.5)
        sound("frost", x, y + 1, z)
    elseif name == "lightning" then
        -- The first struck, then the next nearest it within 4, each three
        -- quarters of the one before.
        local dmg, hit, cur = DAMAGE.lightning[t.lvl], { [near[1]] = true }, near[1]
        local msg = head .. at(cur)
        hurt(cur, dmg, x, z)
        for _ = 1, 2 + t.lvl do
            local ax, ay, az = core.mob_pos(cur)
            if ax == nil then break end
            local nxt = 0
            for _, id in foes_near(ax, ay, az, 4) do
                if not hit[id] then nxt = id break end
            end
            if nxt == 0 then break end
            dmg *= 0.75
            hit[nxt] = true
            msg ..= at(nxt)
            hurt(nxt, dmg, ax, az)
            cur = nxt
        end
        tell(0, msg)
        sound("zap", x, y + 1, z)
    elseif name == "portal" then
        -- The first its mode picks that is no boss goes back to the rift.
        for _, id in near do
            local m = monsters[id]
            if m and not m.foe.boss then
                tell(0, head .. at(id) .. ` {GATE[1] + 0.5} {GATE[2]} {GATE[3] + 0.5}`)
                core.mob_place(id, GATE[1] + 0.5, GATE[2], GATE[3] + 0.5)
                m.hunting, m.stop = 0, 1
                go(id, m)
                sound("portal", x, y + 1, z)
                return true
            end
        end
        return false
    end
    return true
end

-- ------------------------------------------------------------ panels

local function has_all(p: number, cost: { { any } }): boolean
    for _, c in cost do
        if core.bag_count(p, c[1]) < c[2] then return false end
    end
    return true
end

local function pay(p: number, cost: { { any } })
    for _, c in cost do core.take_item(p, c[1], c[2]) end
end

local function cost_text(cost: { { any } }): string
    local parts = {}
    for _, c in cost do table.insert(parts, `{c[2]} {core.item_name(core.item_id(c[1]))}`) end
    return table.concat(parts, ", ")
end

local function show_bench(p: number)
    local name = `defense_game:bench{p}`
    core.ui_define(name, 15, 1.6 + #KINDS * 1.5 + 1.4)
    core.ui_label(name, 0.4, 0.3, "Build a turret, then put it down on the field.")
    for i, k in KINDS do
        local y = 0.3 + i * 1.5
        core.ui_item(name, `see:{k.name}`, 0.4, y - 0.1, 1.2, 1.2, `defense_game:turret_{k.name}`)
        core.ui_button(name, `build:{k.name}`, 1.8, y, 12.8, 1.2, `{k.title}: {cost_text(k.build)}`)
    end
    core.ui_button(name, "close", 0.4, 0.6 + #KINDS * 1.5 + 0.6, 14.2, 1.1, "Close")
    core.ui_show(name, true, p)
end

local upgrading: { [number]: string } = {}

local function show_turret(p: number, t: Turret)
    local name = `defense_game:turret{p}`
    local choose = CHOOSES[t.kind.name] == true
    core.ui_define(name, 13, if choose then 9.0 else 4.2)
    core.ui_label(name, 0.4, 0.3, `{t.kind.title}, level {t.lvl}`)
    if t.lvl < 3 then
        core.ui_button(name, "up", 0.4, 1.3, 12.2, 1.2, `Upgrade to level {t.lvl + 1}: {cost_text(t.kind.up[t.lvl])}`)
    else
        core.ui_label(name, 0.4, 1.5, "At its best.")
    end
    local y = 2.8
    if choose then
        -- Whom it aims at: six buttons, the one set marked.
        -- (The titles alone on each, so each player's machine translates them.)
        core.ui_label(name, 0.4, 2.8, "Aims at:")
        for i, m in MODES do
            if (t.mode or "near") == m[1] then core.ui_label(name, 4.4, 2.8, m[2]) end
            core.ui_button(name, `mode:{m[1]}`, 0.4 + ((i - 1) % 2) * 6.2, 3.6 + ((i - 1) // 2) * 1.4, 6.0, 1.2, m[2])
        end
        y = 7.6
    end
    core.ui_button(name, "close", 0.4, y, 12.2, 1.1, "Close")
    core.ui_show(name, true, p)
end

local function show_records(p: number)
    local name = `defense_game:records{p}`
    local rows = {}
    for _, q in core.players() do
        table.insert(rows, { core.player_name(q), best_of(core.player_name(q)) })
    end
    table.sort(rows, function(a, b) return a[2] > b[2] end)
    core.ui_define(name, 13, 3.6 + #rows * 0.8)
    local field, who = string.match(core.storage_get("best"), "^(%d+);(.*)$")
    core.ui_label(name, 0.4, 0.3, if field then `The field's record: wave {field} ({who})` else "No record yet.")
    for i, r in rows do
        core.ui_label(name, 0.6, 0.5 + i * 0.8, `{r[1]}: best wave {r[2]}`)
    end
    core.ui_button(name, "close", 0.4, 1.6 + #rows * 0.8, 12.2, 1.1, "Close")
    core.ui_show(name, true, p)
end

core.event.on("core:ui_click", function(ev)
    local panel: string = ev.panel
    if string.sub(panel, 1, 13) ~= "defense_game:" then return end
    if ev.id == "close" then
        core.ui_show(panel, false, ev.player)
        return
    end
    local what = string.match(ev.id, "^build:(.+)$")
    if what then
        for _, k in KINDS do
            if k.name == what then
                if has_all(ev.player, k.build) then
                    pay(ev.player, k.build)
                    core.give_item(ev.player, `defense_game:turret_{k.name}`, 1)
                    core.notice(ev.player, `{k.title} built: put it down on the field.`)
                else
                    core.notice(ev.player, `Not enough for that: {cost_text(k.build)}`)
                end
            end
        end
        return
    end
    local mode = string.match(ev.id, "^mode:(%a+)$")
    if mode then
        local t = turrets[upgrading[ev.player] or ""]
        if t then
            t.mode = mode
            show_turret(ev.player, t)
        end
        return
    end
    if ev.id == "up" then
        local t = turrets[upgrading[ev.player] or ""]
        if t and t.lvl < 3 then
            local cost = t.kind.up[t.lvl]
            if has_all(ev.player, cost) then
                pay(ev.player, cost)
                t.lvl += 1
                tell_turret(0, t)
                core.notice(ev.player, `{t.kind.title}: level {t.lvl}`)
                show_turret(ev.player, t)
            else
                core.notice(ev.player, `Not enough for that: {cost_text(cost)}`)
            end
        end
    end
end)

-- ------------------------------------------------------------ the world's rules

local function on_field(x: number, y: number, z: number): boolean
    return x >= X0 and x <= X1 and z >= BUILD_Z0 and z <= BUILD_Z1 and y == G + 1
end

core.event.on("core:pre_place", function(ev)
    local b = ev.id
    if b ~= BARRICADE and KIND_OF[b] == nil then
        ev.cancel = true
        return
    end
    if not on_field(ev.x, ev.y, ev.z) or core.get_block(ev.x, ev.y + 1, ev.z) ~= 0 then
        ev.cancel = true
        core.notice(ev.player, "Only on the field's grass, between the crystal and the rift.")
    end
end)

core.event.on("core:placed", function(ev)
    local x, y, z = ev.x, ev.y, ev.z
    core.set_block(x, y + 1, z, if ev.id == BARRICADE then BARRICADE_TOP else CAP)
    if not make_route() then
        core.set_block(x, y, z, 0)
        core.set_block(x, y + 1, z, 0)
        make_route()
        core.give_item(ev.player, core.node_name(ev.id), 1)
        core.notice(ev.player, "That would shut the way: the monsters must have one.")
        return
    end
    placed[key(x, y, z)] = { x, y, z }
    placed[key(x, y + 1, z)] = { x, y + 1, z }
    local k = KIND_OF[ev.id]
    if k then
        local t = { kind = k, lvl = 1, x = x, y = y, z = z, wait = 0 }
        turrets[key(x, y, z)] = t
        tell_turret(0, t)
    end
    send_all()
end)

core.event.on("core:pre_break", function(ev)
    if ev.id ~= BARRICADE and ev.id ~= BARRICADE_TOP and KIND_OF[ev.id] == nil then ev.cancel = true end
end)

core.event.on("core:broken", function(ev)
    local x, y, z = ev.x, ev.y, ev.z
    placed[key(x, y, z)] = nil
    if turrets[key(x, y, z)] then tell(0, `R {key(x, y, z)}`) end
    turrets[key(x, y, z)] = nil
    if KIND_OF[ev.id] and core.get_block(x, y + 1, z) == CAP then
        core.set_block(x, y + 1, z, 0)
        placed[key(x, y + 1, z)] = nil
    elseif ev.id == BARRICADE and core.get_block(x, y + 1, z) == BARRICADE_TOP then
        core.set_block(x, y + 1, z, 0)
        placed[key(x, y + 1, z)] = nil
    elseif ev.id == BARRICADE_TOP and core.get_block(x, y - 1, z) == BARRICADE then
        core.set_block(x, y - 1, z, 0)
        placed[key(x, y - 1, z)] = nil
    end
    make_route()
    send_all()
end)

core.event.on("core:used", function(ev)
    if ev.id == CRYSTAL then
        ev.cancel = true
        if phase == "idle" then
            phase, crystal = "rest", CRYSTAL_MAX
            rest_left = 30
            kit()
        elseif phase == "rest" then
            rest_left = 0
        elseif phase == "over" and over_left <= 0 then
            reset()
            phase, rest_left = "rest", 30
            kit()
        else
            show_records(ev.player)
        end
    elseif ev.id == BENCH then
        ev.cancel = true
        show_bench(ev.player)
    else
        -- The turret, or the cap over it: its model reaches into that cell.
        local y = if ev.id == CAP then ev.y - 1 else ev.y
        local t = turrets[key(ev.x, y, ev.z)]
        if t then
            ev.cancel = true
            upgrading[ev.player] = key(ev.x, y, ev.z)
            show_turret(ev.player, t)
        end
    end
end)

core.command("defense", "-- the tower defense's records: the field's and those of the players here")
core.event.on("core:command", function(ev)
    if ev.name == "defense" then show_records(ev.player) end
end)

-- ------------------------------------------------------------ the tick

local every = 0.0
local seen: { [number]: boolean } = {}
core.event.on("core:tick", function(ev)
    local dt: number = ev.dt
    every += dt
    for _, t in turrets do t.wait -= dt end

    local here = core.players()
    if #here == 0 then
        empty_for += dt
        if empty_for > 20 and phase ~= "idle" then reset() end
        return
    end
    empty_for = 0
    for _, p in here do
        if not seen[p] then
            seen[p] = true
            core.player_spawn(p, CX + 0.5, G + 1, 2.5)
            for _, t in turrets do tell_turret(p, t) end
            core.notice(p, if phase == "idle" then "Click the crystal to begin." else `Wave {wave} is on.`)
        end
    end

    if phase == "over" then
        over_left -= dt
        return
    end
    if phase == "rest" then
        rest_left -= dt
        if rest_left <= 0 then start_wave() end
        return
    end
    if phase ~= "wave" then return end

    -- Out of the rift, one at a time.
    spawn_wait -= dt
    if #queue > 0 and spawn_wait <= 0 then
        local kind = table.remove(queue, 1) :: string
        local foe = FOES[kind]
        local off = ((#queue * 5) % 5) - 2
        local id = core.spawn_mob(`defense_game:{kind}`, GATE[1] + off + 0.5, GATE[2], GATE[3] + 0.5)
        if id ~= 0 then
            core.mob_health(id, math.floor(foe.hp * hp_scale))
            local m = { kind = `defense_game:{kind}`, foe = foe, hunting = 0, stop = 1, hunt_left = -6,
                        hp = math.floor(foe.hp * hp_scale) }
            monsters[id] = m
            go(id, m)
        end
        spawn_wait = if foe.boss then 2.0 else 0.8
    end

    for _, t in turrets do
        if t.wait <= 0 and fire(t) then t.wait = t.kind.every[t.lvl] end
    end

    if every < 0.5 then return end
    every = 0
    for id, m in monsters do
        local x, y, z = core.mob_pos(id)
        -- Gone, or dead and its id given to some other creature since.
        if x == nil or core.mob_kind(id) ~= m.kind then
            monsters[id] = nil
            continue
        end
        if (x - GOAL[1] - 0.5) ^ 2 + (z - GOAL[3] - 0.5) ^ 2 < 2.5 * 2.5
            or (x - CX - 0.5) ^ 2 + (z - CRYSTAL_Z - 0.5) ^ 2 < 3 * 3 then
            leak(id, m)
            if phase ~= "wave" then return end
            continue
        end
        -- A player close by is fought for up to 10 s; far again, or after that,
        -- back on the way (and no fight for 6 s): a monster kept busy by a
        -- player by the crystal still gets to it.
        m.hunt_left -= 0.5
        -- Near the crystal (within 4 of its way in) nobody is fought: it goes
        -- for the crystal, and whoever stands there has to stop it.
        local home = z < GOAL[3] + 4 and math.abs(x - GOAL[1]) < 14
        local near, d2 = 0, if home then 0 else 3.5 * 3.5
        for _, p in here do
            local px, py, pz = core.player_pos(p)
            if px and math.abs(py - y) < 3 then
                local d = (px - x) ^ 2 + (pz - z) ^ 2
                if d < d2 then near, d2 = p, d end
            end
        end
        if m.hunting ~= 0 then
            local px, _, pz = core.player_pos(m.hunting)
            if px == nil or (px - x) ^ 2 + (pz - z) ^ 2 > 8 * 8 or m.hunt_left <= 0 or home then
                m.hunting, m.hunt_left = 0, 0
                m.stop = next_stop(x, z)
                go(id, m)
            end
        elseif near ~= 0 and m.hunt_left <= -6 then
            m.hunting, m.hunt_left = near, 10
            core.mob_hunt(id, near)
        elseif not core.mob_going(id) then
            -- At its stop (or as near as it gets): the next one.
            local st = route[m.stop]
            if st and (st[1] + 0.5 - x) ^ 2 + (st[3] + 0.5 - z) ^ 2 < 2.5 * 2.5 then
                m.stop = math.min(m.stop + 1, #route)
            else
                m.stop = next_stop(x, z)
            end
            go(id, m)
        end
    end
    if #queue == 0 and next(monsters) == nil then
        reward()
        keep_record()
        phase, rest_left = "rest", 25
    end
end)
