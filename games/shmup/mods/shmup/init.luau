--!strict
-- shmup: a shooter seen from overhead, in the manner of the arcade cabinets
-- of the eighties. A first draft: one screen that does not scroll, waves of
-- planes, three lives, a score and a high score.
--
-- Whoever arrives is a fighter on the carrier's deck at the bottom of the
-- picture (shmup_core), already seen from overhead (core.top_view_at). W A S
-- D slide the fighter about the screen; the space bar starts a game and,
-- held, fires; E goes back to the deck.
--
--   darts come straight down; weavers swing from side to side and aim a
--   shot now and then; aces stop half way and fire three at a time; every
--   fifth wave is a bomber that takes forty
--   a shot or a plane that touches the fighter costs a life, and two and a
--   half seconds follow in which nothing does; none left, the fighter is put
--   back on the deck until everybody is, and the game is over
--   several players fly the same waves, each with their own lives and score
--
-- The server keeps everything that decides: where the planes and the shots
-- are, who was hit. A player's machine is told each shot once, when it is
-- fired, and flies it by itself -- shots go straight -- until it is told the
-- shot is gone. The planes are the mod's entities, seen by everybody
-- (core.ecs.visible).
--
-- The picture's top is +z, so its right is -x (shmup_core).

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

-- The field, as shmup_core builds it.
local CX, CZ = 64.5, 66.5
local FLY    = 30
local HALF_W, HALF_H = 40, 25
local TOP, BOTTOM = CZ + HALF_H + 8, CZ - HALF_H - 8   -- where a plane comes in and where it is gone

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

    -- kind -> hp, points, half width and half length for a hit, model
    local KINDS = {
        dart   = { hp = 1,  points = 100,  hw = 4.0, hl = 3.2, model = model("foe_dart") },
        weaver = { hp = 2,  points = 150,  hw = 4.0, hl = 3.2, model = model("foe_weaver") },
        ace    = { hp = 5,  points = 400,  hw = 4.0, hl = 3.2, model = model("foe_ace") },
        bomber = { hp = 40, points = 3000, hw = 8.0, hl = 5.0, model = model("bomber") },
    }

    type Foe = { kind: string, x: number, z: number, x0: number, t: number, hp: number, gun: number }
    type Shot = { x: number, z: number, vx: number, vz: number, owner: number }
    type Flier = { lives: number, score: number, fire: boolean, gun: number, safe: number, out: boolean }

    local mode = "attract"      -- "attract" | "play" | "over"
    local wave, wave_at = 0, 0.0
    local queue: { { any } } = {}    -- { when, kind, x }
    local foes: { [number]: Foe } = {}
    local shots: { [number]: Shot } = {}
    local next_shot = 1
    local fliers: { [number]: Flier } = {}
    local clock = 0.0
    local timed = false
    local hi = tonumber(core.storage_get("hi") or "") or 0

    local function status(p: number)
        tell(p, `st {mode} {wave}`)
    end
    local function count(p: number, f: Flier)
        tell(p, `me {f.lives} {f.score} {hi}`)
    end

    local function to_deck(p: number)
        core.player_place(p, CX, FLY, CZ - 29, 0, 0, 1)
    end

    local function clear()
        for id in foes do core.ecs.kill(id) end
        table.clear(foes)
        for id in shots do tell(0, `k {id}`) end
        table.clear(shots)
        table.clear(queue)
    end

    local function spawn(kind: string, x: number)
        local id = core.ecs.spawn(FOE, x, FLY, TOP)
        if id == nil then return end   -- no session table: entities need a listening server
        local k = KINDS[kind]
        core.ecs.set(id, "hp", k.hp)
        core.ecs.set(id, "yaw", 0)     -- nose down the screen, -z
        core.ecs.visible(id, true)
        core.ecs.model(id, k.model)
        foes[id] = { kind = kind, x = x, z = TOP, x0 = x, t = 0, hp = k.hp, gun = 1 + math.random() }
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
        if wave % 5 == 0 then
            table.insert(queue, { 1.5, "bomber", CX })
        else
            local n = 4 + 2 * wave
            local gap = math.max(0.35, 1.0 - 0.06 * wave)
            for i = 1, n do
                local kind = if i % 3 == 0 and wave >= 2 then "weaver" else "dart"
                table.insert(queue, { 1.5 + i * gap, kind, CX + (math.random() * 2 - 1) * (HALF_W - 6) })
            end
            for i = 1, wave // 2 do
                table.insert(queue, { 3 + i * 4, "ace", CX + (math.random() * 2 - 1) * (HALF_W - 10) })
            end
        end
        status(0)
        if wave > 1 then core.play_sound("shmup:wave", CX, FLY, CZ) end
    end

    local function start_game(by: number)
        clear()
        mode, wave = "play", 0
        local n = 0
        for _, p in core.players() do
            -- The press that started the game is still down: it fires.
            fliers[p] = { lives = 3, score = 0, fire = p == by, gun = 0, safe = clock + 2, out = false }
            core.player_place(p, CX + (n % 2 * 2 - 1) * 8 * ((n + 1) // 2), FLY, CZ - 14, 0, 0, 1)
            count(p, fliers[p])
            n += 1
        end
        start_wave()
        core.play_sound("shmup:start", CX, FLY, CZ)
    end

    local function game_over()
        mode, wave_at = "over", clock
        clear()
        local best = hi
        for _, f in fliers do best = math.max(best, f.score) end
        if best > hi then
            hi = best
            core.storage_set("hi", tostring(hi))
        end
        for p, f in fliers do count(p, f) end
        status(0)
        core.play_sound("shmup:over", CX, FLY, CZ)
    end

    -- A life lost; the last one puts the fighter back on the deck.
    local function hit(p: number, f: Flier, x: number, z: number)
        f.lives -= 1
        f.safe = clock + 2.5
        tell(0, string.format("boom %.1f %.1f 6", x, z))
        tell(p, "hit")
        core.play_sound("shmup:hit", x, FLY, z)
        if f.lives <= 0 then
            f.out, f.fire = true, false
            to_deck(p)
        end
        count(p, f)
    end

    local function killed(id: number, foe: Foe, by: number)
        local k = KINDS[foe.kind]
        tell(0, string.format("boom %.1f %.1f %d", foe.x, foe.z, if foe.kind == "bomber" then 14 else 7))
        core.play_sound(if foe.kind == "bomber" then "shmup:big_boom" else "shmup:boom", foe.x, FLY, foe.z)
        core.ecs.kill(id)
        foes[id] = nil
        local f = fliers[by]
        if f then
            f.score += k.points
            count(by, f)
        end
    end

    asked = function(p: number, text: string)
        local f = fliers[p]
        if text == "f1" then
            if mode == "attract" then
                start_game(p)
            elseif mode == "play" and f == nil then
                -- Arrived with a game going: in, with three lives.
                fliers[p] = { lives = 3, score = 0, fire = true, gun = 0, safe = clock + 2, out = false }
                core.player_place(p, CX, FLY, CZ - 14, 0, 0, 1)
                count(p, fliers[p])
            elseif f and not f.out then
                f.fire = true
            end
        elseif text == "f0" then
            if f then f.fire = false end
        elseif text == "quit" then
            if f and not f.out then
                f.out, f.fire = true, false
                count(p, f)
            end
            to_deck(p)
        elseif text == "hello" then
            status(p)
            tell(p, `me {if f then f.lives else 0} {if f then f.score else 0} {hi}`)
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
        if not timed then
            timed = true
            core.set_time(0.5)
            core.set_day_cycle(false)
        end

        -- Who is here, and where.
        local here: { [number]: { number } } = {}
        for _, p in core.players() do
            local x, y, z = core.player_pos(p)
            if x then here[p] = { x, y, z } end
        end
        for p in fliers do
            if not here[p] then fliers[p] = nil end
        end

        if mode == "over" then
            if clock - wave_at > 5 then
                mode = "attract"
                table.clear(fliers)
                status(0)
            end
            return
        end
        if mode ~= "play" then return end

        -- Everybody down: over.
        local flying = false
        for _, f in fliers do
            if not f.out then flying = true end
        end
        if not flying then
            game_over()
            return
        end

        -- The wave's planes as their moments come, and the next wave.
        local since = clock - wave_at
        for i = #queue, 1, -1 do
            if since >= queue[i][1] then
                spawn(queue[i][2], queue[i][3])
                table.remove(queue, i)
            end
        end
        if #queue == 0 and next(foes) == nil and since > 2 then start_wave() end

        -- The fighters' guns.
        for p, f in fliers do
            local at = here[p]
            if at and f.fire and not f.out and clock >= f.gun then
                f.gun = clock + 0.14
                shoot(at[1], at[3] + 3.5, 0, SHOT_SPEED, p)
            end
        end

        -- The planes.
        for id, foe in foes do
            foe.t += dt
            if foe.kind == "dart" then
                foe.z -= 16 * dt
            elseif foe.kind == "weaver" then
                foe.z -= 10 * dt
                foe.x = math.clamp(foe.x0 + 12 * math.sin(foe.t * 1.6), CX - HALF_W + 4, CX + HALF_W - 4)
                if foe.t >= foe.gun then
                    foe.gun = foe.t + 2.2
                    aimed(foe.x, foe.z, 22, 0, 1)
                end
            elseif foe.kind == "ace" then
                if foe.t < 9 then
                    foe.z = math.max(CZ + 12, foe.z - 18 * dt)
                    foe.x = math.clamp(foe.x0 + 16 * math.sin(foe.t * 0.9), CX - HALF_W + 4, CX + HALF_W - 4)
                else
                    foe.z -= 20 * dt
                end
                if foe.t >= foe.gun and foe.t < 9 then
                    foe.gun = foe.t + 1.6
                    aimed(foe.x, foe.z, 26, 0.18, 3)
                end
            else   -- the bomber
                foe.z = math.max(CZ + 17, foe.z - 8 * dt)
                foe.x = CX + 26 * math.sin(foe.t * 0.5)
                if foe.t >= foe.gun then
                    foe.gun = foe.t + 1.2
                    aimed(foe.x, foe.z, 24, 0.22, 5)
                end
            end
            if foe.z < BOTTOM then
                core.ecs.kill(id)
                foes[id] = nil
            else
                core.ecs.set(id, "x", foe.x)
                core.ecs.set(id, "z", foe.z)
                -- Flown into: a life.
                local k = KINDS[foe.kind]
                for p, f in fliers do
                    local at = here[p]
                    if at and not f.out and clock >= f.safe and math.abs(at[1] - foe.x) < k.hw and math.abs(at[3] - foe.z) < k.hl then
                        hit(p, f, at[1], at[3])
                    end
                end
            end
        end

        -- The shots.
        for id, s in shots do
            s.x += s.vx * dt
            s.z += s.vz * dt
            local gone = math.abs(s.x - CX) > HALF_W + 6 or s.z > TOP or s.z < BOTTOM
            if not gone and s.owner > 0 then
                for fid, foe in foes do
                    local k = KINDS[foe.kind]
                    -- A shot covers SHOT_SPEED * dt a tick: the plane's length and that.
                    if math.abs(s.x - foe.x) < k.hw and math.abs(s.z - foe.z) < k.hl + SHOT_SPEED * dt * 0.5 then
                        gone = true
                        foe.hp -= 1
                        if foe.hp <= 0 then killed(fid, foe, s.owner) else tell(0, string.format("boom %.1f %.1f 2", s.x, s.z)) end
                        break
                    end
                end
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
        end
    end)
end

-- ================================================================ the player

if drawing then
    local mode, wave = "attract", 0
    local lives, score, hi = 0, 0, 0
    local shots: { [number]: { number } } = {}     -- id -> { x, z, vx, vz, ours }
    local booms: { { number } } = {}               -- { x, z, size, age }
    local clock = 0.0
    local flash = 0.0
    local looked = false
    local banner_until = 0.0
    local safe_until = 0.0     -- nothing hits this fighter until then
    local gun_heard = 0.0      -- when a shot of the fighters' was last heard

    local function send(text: string)
        if serving then asked(1, text) else core.net.send(0, text) end
    end

    heard = function(text: string)
        local w = string.split(text, " ")
        local what = w[1]
        if what == "b" and clock - gun_heard > 0.09 then
            -- Heard here and not sent as a sound: seven a second a fighter.
            gun_heard = clock
            core.play_sound("shmup:shot", tonumber(w[3]) or CX, FLY, tonumber(w[4]) or CZ)
        end
        if what == "b" or what == "e" then
            shots[tonumber(w[2]) :: number] = { tonumber(w[3]) :: number, tonumber(w[4]) :: number,
                                                tonumber(w[5]) :: number, tonumber(w[6]) :: number,
                                                if what == "b" then 1 else 0 }
        elseif what == "k" then
            shots[tonumber(w[2]) :: number] = nil
        elseif what == "boom" then
            table.insert(booms, { tonumber(w[2]) :: number, tonumber(w[3]) :: number, tonumber(w[4]) :: number, 0 })
        elseif what == "hit" then
            flash = 0.5
            safe_until = clock + 2.5
        elseif what == "st" then
            if w[2] ~= mode or tonumber(w[3]) ~= wave then banner_until = clock + 2.5 end
            if w[2] == "play" and mode ~= "play" then safe_until = clock + 2 end
            mode, wave = w[2], tonumber(w[3]) or 0
            if mode ~= "play" then table.clear(shots) end
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
            -- One screen, held over the middle of the field: its top is +z.
            looked = true
            core.top_view_at(CX, FLY, CZ, 40, 180, 70, 1.0)
            core.engine_hud(false)
            send("hello")
        end
        flash = math.max(0, flash - dt)
        for id, s in shots do
            s[1] += s[3] * dt
            s[2] += s[4] * dt
            if math.abs(s[1] - CX) > HALF_W + 8 or s[2] > TOP + 2 or s[2] < BOTTOM - 2 then shots[id] = nil end
        end
        for i = #booms, 1, -1 do
            booms[i][4] += dt
            if booms[i][4] > 0.5 then table.remove(booms, i) end
        end
    end)

    core.event.on("core:input", function(ev)
        if ev.action == "key_space" then
            send("f1")
        elseif ev.action == "key_space_up" then
            send("f0")
        elseif ev.action == "key_e" then
            send("quit")
        elseif ev.action == "mouse_left" or ev.action == "mouse_right" then
            ev.cancel = true
        end
    end)

    -- The sea goes by under the field, which stands still: streaks of foam
    -- and a few clouds, drawn here and nowhere else. Then the shots and the
    -- bursts.
    core.render.pass("shmup:field", 100, function()
        local span = 2 * HALF_H + 24
        for i = 0, 23 do
            local x = CX + ((i * 37) % 97 - 48) * 0.95
            local z = CZ + HALF_H + 12 - (i * 11.3 + clock * 9) % span
            core.render.box(x, 21.2, z, 5 + i % 4 * 2, 0.1, 0.5, 170, 210, 240, 255)
        end
        for i = 0, 5 do
            local x = CX + ((i * 53) % 89 - 44) * 0.9
            local z = CZ + HALF_H + 12 - (i * 17.7 + clock * 15) % span
            core.render.box(x, 25, z, 9, 0.6, 5, 245, 248, 252, 255)
            core.render.box(x + 3, 25.4, z + 1, 6, 0.6, 3.5, 245, 248, 252, 255)
        end
        -- This machine's own fighter while nothing can hit it: its wing tips blink.
        local px, _, pz = core.player_pos(1)
        if px ~= nil and mode == "play" and lives > 0 and clock < safe_until and clock % 0.2 < 0.1 then
            core.render.box(px - 4.4, FLY + 1.6, pz, 0.9, 0.3, 1.4, 60, 220, 255, 255)
            core.render.box(px + 4.4, FLY + 1.6, pz, 0.9, 0.3, 1.4, 60, 220, 255, 255)
        end
        for _, s in shots do
            if s[5] == 1 then
                core.render.box(s[1], FLY + 1, s[2], 0.45, 0.45, 2.4, 255, 236, 90, 255)
            else
                core.render.box(s[1], FLY + 1, s[2], 1.1, 1.1, 1.1, 255, 90, 60, 255)
                core.render.box(s[1], FLY + 1, s[2], 0.6, 1.3, 0.6, 255, 230, 200, 255)
            end
        end
        for _, b in booms do
            local t = b[4] / 0.5
            core.render.fx("glow", b[1], FLY + 1, b[2], b[3] * (0.4 + t), 255, 170, 60, 255, t)
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
        if mode == "play" then
            core.hud.text(0.94, 0.035, `WAVE {wave}`, 0.035, 2, 120, 220, 255, 255)
            core.hud.text(0.06, 0.95, `LIVES {lives}`, 0.04, 0, 255, 220, 60, 255)
            if clock < banner_until then
                core.hud.text(0.5, 0.4, if wave % 5 == 0 then "WARNING: BOMBER" else `WAVE {wave}`, 0.07, 1, 255, 240, 120, 255)
            end
            if lives <= 0 then
                core.hud.text(0.5, 0.5, "SHOT DOWN", 0.06, 1, 255, 90, 60, 255)
            end
        elseif mode == "over" then
            core.hud.text(0.5, 0.42, "GAME OVER", 0.1, 1, 255, 60, 60, 255)
        else
            core.hud.text(0.5, 0.3, "ACE OF THE SKIES", 0.1, 1, 255, 220, 60, 255)
            if clock % 1 < 0.6 then
                core.hud.text(0.5, 0.5, "INSERT COIN", 0.06, 1, 255, 255, 255, 255)
            end
            core.hud.text(0.5, 0.6, "press SPACE to take off", 0.035, 1, 200, 230, 255, 255)
            core.hud.text(0.5, 0.95, "W A S D move   SPACE fire   E back to the deck", 0.024, 1, 255, 255, 255, 200)
        end
        if flash > 0 then
            core.hud.rect(0.5, 0.5, 4, 1, 255, 40, 20, math.floor(120 * flash / 0.5))
        end
    end)
end
