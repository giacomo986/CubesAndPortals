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
    core.node_model(b, core.register_model(`defense_game:turret_{k.name}`, `turret_{k.name}.gltf`))
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

type Turret = { kind: Kind, lvl: number, x: number, y: number, z: number, wait: number }
type Monster = { kind: string, foe: Foe, hunting: number, stop: number, hunt_left: number }

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

-- The monsters within r of a cell, the nearest the crystal first.
local function foes_near(x: number, y: number, z: number, r: number): { number }
    local out: { number } = {}
    for id, m in monsters do
        local mx, my, mz = core.mob_pos(id)
        if mx and core.mob_kind(id) == m.kind and (mx - x) ^ 2 + (mz - z) ^ 2 <= r * r and math.abs(my - y) < 4 then
            table.insert(out, id)
        end
    end
    table.sort(out, function(a, b)
        local _, _, az = core.mob_pos(a)
        local _, _, bz = core.mob_pos(b)
        return (az :: number) < (bz :: number)
    end)
    return out
end

local DAMAGE = { arrow = { 5, 7, 10 }, brazier = { 3, 5, 7 }, lightning = { 9, 12, 16 } }
local SLOW = { 0.55, 0.45, 0.35 }

local function fire(t: Turret)
    local x, y, z = t.x + 0.5, t.y, t.z + 0.5
    local r = t.kind.range[t.lvl]
    local near = foes_near(x, y, z, r)
    if #near == 0 then return false end
    local name = t.kind.name
    if name == "arrow" then
        core.mob_hurt(near[1], DAMAGE.arrow[t.lvl], x, z)
        sound("arrow", x, y + 1, z)
    elseif name == "brazier" then
        for _, id in near do core.mob_hurt(id, DAMAGE.brazier[t.lvl], x, z) end
        sound("fire", x, y + 1, z)
    elseif name == "frost" then
        core.slow_mobs(x, y, z, r, SLOW[t.lvl], 1.5)
        sound("frost", x, y + 1, z)
    elseif name == "lightning" then
        -- The first struck, then the next nearest it within 4, each three
        -- quarters of the one before.
        local dmg, hit, at = DAMAGE.lightning[t.lvl], { [near[1]] = true }, near[1]
        core.mob_hurt(at, dmg, x, z)
        for _ = 1, 2 + t.lvl do
            local ax, ay, az = core.mob_pos(at)
            if ax == nil then break end
            local nxt = 0
            for _, id in foes_near(ax, ay, az, 4) do
                if not hit[id] then nxt = id break end
            end
            if nxt == 0 then break end
            dmg *= 0.75
            hit[nxt] = true
            core.mob_hurt(nxt, dmg, ax, az)
            at = nxt
        end
        sound("zap", x, y + 1, z)
    elseif name == "portal" then
        -- The one nearest the crystal that is no boss goes back to the rift.
        for _, id in near do
            local m = monsters[id]
            if m and not m.foe.boss then
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
    core.ui_define(name, 13, 4.2)
    core.ui_label(name, 0.4, 0.3, `{t.kind.title}, level {t.lvl}`)
    if t.lvl < 3 then
        core.ui_button(name, "up", 0.4, 1.3, 12.2, 1.2, `Upgrade to level {t.lvl + 1}: {cost_text(t.kind.up[t.lvl])}`)
    else
        core.ui_label(name, 0.4, 1.5, "At its best.")
    end
    core.ui_button(name, "close", 0.4, 2.8, 12.2, 1.1, "Close")
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
    if ev.id == "up" then
        local t = turrets[upgrading[ev.player] or ""]
        if t and t.lvl < 3 then
            local cost = t.kind.up[t.lvl]
            if has_all(ev.player, cost) then
                pay(ev.player, cost)
                t.lvl += 1
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
    if k then turrets[key(x, y, z)] = { kind = k, lvl = 1, x = x, y = y, z = z, wait = 0 } end
    send_all()
end)

core.event.on("core:pre_break", function(ev)
    if ev.id ~= BARRICADE and ev.id ~= BARRICADE_TOP and KIND_OF[ev.id] == nil then ev.cancel = true end
end)

core.event.on("core:broken", function(ev)
    local x, y, z = ev.x, ev.y, ev.z
    placed[key(x, y, z)] = nil
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
            local m = { kind = `defense_game:{kind}`, foe = foe, hunting = 0, stop = 1, hunt_left = -6 }
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
