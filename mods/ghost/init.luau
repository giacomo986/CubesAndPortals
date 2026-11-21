--!strict
-- ghost: the CURSED GHOST, a creature of the dark (on the surface by night,
-- in caves always), rare.
--
-- From afar it is hardly there: past 6 nodes a faint shimmer, nearer it
-- fades in, seen through. It flies at whoever it notices, slower than a
-- walking player, and strikes nobody: if it reaches you it TAKES HOLD. Then
-- it hangs at your back, your keys no longer walk you (core.walk on your
-- machine walks you instead), and it
--   * walks you towards the nearest drop within 12 nodes -- a column with
--     no ground for 5 under your feet' level -- or, with none near, towards
--     the nearest other monster within 24, or else about at random;
--   * every few seconds throws one thing out of your bag, picked at random,
--     onto the ground behind you.
-- Every blow you swing while it holds you hurts it (10 health, 2 a blow:
-- five blows), and so does every blow or arrow another monster lands on you
-- (core:hurt); when it dies you are free. It lets go too if you die or
-- leave, and does not take you again for 8 seconds.
--
-- The engine moves it as a creature (core.register_mob, hovering) wearing
-- ghost_body.gltf, which draws nothing and is only its size
-- (tools/make_ghost_body.py). What is seen is ghost.gltf (made from a Meshy
-- model by tools/meshy_prop.py), drawn here on every machine with
-- core.render.model, seen through by how far it is from that player, which
-- the server says, posed by its clips (idle, fly, hold, death; skeleton and
-- clips by tools/make_ghost_rig.py); when it dies it comes apart in the
-- model's dissolve while its death clip plays.

local side = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local KIND = "ghost:cursed"
local HEALTH, BLOW = 10, 2
local FAR = 6             -- past this, only a shimmer
local HOLD_REACH = 1.3    -- this near it takes hold
local AGAIN = 8           -- seconds before it takes the same player again
local DROP_EVERY = 3.5    -- seconds between two things thrown out of the bag
local STEER_EVERY = 0.5   -- seconds between two looks for where to walk them

local heard: (string) -> () = function(_) end
-- In a world that is its own server: a blow swung by the one player here.
local swung_here: () -> () = function() end

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

    local BODY = core.register_model("ghost:body", "ghost_body.gltf")
    core.register_model("ghost:ghost", "ghost.gltf")
    -- Health, speed (a player walks 4.8), melee with no blow and a reach it
    -- never strikes from (it takes hold instead, below), no drop; hovers.
    core.register_mob(KIND, BODY, HEALTH, 2.6, "melee", 0, 0.3, 1.0, "", 0, 0, false, 0.6)
    core.mob_sight(KIND, 20)
    core.mob_share(KIND, 0.06)

    type Ghost = { x: number, y: number, z: number, yaw: number, host: number? }
    type Held = { ghost: number, steer: number, drop: number, dx: number, dz: number, wander: number,
                  swung: number, hits: number }
    local ghosts: { [number]: Ghost } = {}
    local held: { [number]: Held } = {}       -- player -> what holds them
    local free_at: { [number]: number } = {}  -- player -> when it may be taken again
    local shown: { [number]: boolean } = {}   -- player -> was told of ghosts last time
    local clock = 0.0

    local function is_ghost(id: number): boolean
        return core.mob_kind(id) == KIND
    end

    -- No ground for 5 nodes under the feet' level at that column, and nothing
    -- in the way at the feet: a drop to walk somebody into.
    local function drop_at(x: number, y: number, z: number): boolean
        local bx, by, bz = math.floor(x), math.floor(y), math.floor(z)
        if core.node_solid(core.get_block(bx, by, bz)) or core.node_solid(core.get_block(bx, by + 1, bz)) then
            return false
        end
        for dy = 1, 5 do
            if core.node_solid(core.get_block(bx, by - dy, bz)) then
                return false
            end
        end
        return true
    end

    -- Where to walk a held player: towards a drop, else another monster,
    -- else wherever it fancies. A unit direction on the ground.
    local function steer(p: number, h: Held): (number, number)
        local x, y, z = core.player_pos(p)
        if x == nil then
            return 0, 0
        end
        -- ponytail: 16 directions out to 12 nodes, nearest drop wins; a drop
        -- behind a wall is walked at into the wall.
        for d = 2, 12 do
            for k = 0, 15 do
                local a = k * math.pi / 8
                local cx, cz = math.sin(a), math.cos(a)
                if drop_at(x + cx * d, y :: number, z + cz * d) then
                    return cx, cz
                end
            end
        end
        for _, id in core.mob_ids(x, y :: number, z :: number, 24) do
            if not is_ghost(id) then
                local mx, _, mz = core.mob_pos(id)
                if mx ~= nil then
                    local dx, dz = mx - x, mz - z
                    local n = math.sqrt(dx * dx + dz * dz)
                    if n > 0.5 then
                        return dx / n, dz / n
                    end
                end
            end
        end
        if clock >= h.wander then
            local a = math.random() * 2 * math.pi
            h.wander = clock + 2
            return math.sin(a), math.cos(a)
        end
        return h.dx, h.dz
    end

    -- One thing out of the bag, at random, onto the ground behind them.
    local function throw_out(p: number, h: Held)
        local have: { { any } } = {}
        for id = 1, core.item_count() do
            local name = core.item_name(id)
            if name ~= "" and core.bag_count(p, name) > 0 then
                table.insert(have, { id, name })
            end
        end
        if #have == 0 then
            return
        end
        local pick = have[math.random(1, #have)]
        if core.take_item(p, pick[2], 1) < 1 then
            return
        end
        local x, y, z = core.player_pos(p)
        if x == nil then
            return
        end
        core.drop_item(x - h.dx * 1.5, (y :: number) + 0.5, (z :: number) - h.dz * 1.5, pick[1], 1)
        core.play_sound("ghost:throw", x, (y :: number) + 1, z :: number)
    end

    local function let_go(p: number, why: string?)
        local h = held[p]
        if h == nil then
            return
        end
        held[p] = nil
        local g = ghosts[h.ghost]
        if g then
            g.host = nil
        end
        free_at[p] = clock + AGAIN
        tell(p, "own 0")
        core.log(`ghost: {core.player_name(p)} let go after {h.hits} blows`)
        if why then
            core.notice(p, why)
        end
    end

    local function take_hold(p: number, id: number)
        held[p] = { ghost = id, steer = 0, drop = clock + 1.5, dx = 0, dz = 0, wander = 0, swung = 0, hits = 0 }
        local g = ghosts[id]
        if g then
            g.host = p
        end
        tell(p, `own 1 {HEALTH}`)
        core.notice(p, "A cursed ghost has taken hold of you! Strike to break free.")
        local x, y, z = core.player_pos(p)
        if x then
            core.play_sound("ghost:hold", x, (y :: number) + 1.5, z :: number)
        end
        core.log(`ghost: {core.player_name(p)} taken hold of`)
    end

    -- A blow swung by somebody held: the ghost holding them is hurt.
    -- The ghost holding p loses a blow's worth.
    local function strike(p: number, h: Held)
        h.hits += 1
        local x, _, z = core.player_pos(p)
        local hit = core.mob_hurt(h.ghost, BLOW, x or 0, z or 0)
        if hit == 2 or hit == 0 then
            let_go(p, "The cursed ghost is gone. You are free.")
        else
            tell(p, `own 1 {math.max(0, HEALTH - BLOW * h.hits)}`)
        end
    end
    local function swing(p: number)
        local h = held[p]
        if h == nil or clock - h.swung < 0.35 then
            return
        end
        h.swung = clock
        strike(p, h)
    end
    core.event.on("core:net_message", function(ev)
        if ev.text == "swing" then
            swing(ev.player :: number)
        end
    end)
    swung_here = function()
        for p in held do
            swing(p)
        end
    end

    -- Another monster's blow or arrow on whoever it holds strikes it too.
    core.event.on("core:hurt", function(ev)
        local p = ev.player :: number
        local h = held[p]
        if h ~= nil then
            strike(p, h)
        end
    end)

    core.event.on("core:died", function(ev)
        let_go(ev.player :: number)
    end)

    local since = 0.0
    core.event.on("core:tick", function(ev)
        clock += ev.dt
        local players = core.players()
        local here: { [number]: boolean } = {}
        for _, p in players do
            here[p] = true
        end
        for p in held do
            if not here[p] then
                held[p] = nil
            end
        end

        -- The ghosts near anybody, and where.
        local seen: { [number]: boolean } = {}
        for _, p in players do
            local x, y, z = core.player_pos(p)
            if x ~= nil then
                for _, id in core.mob_ids(x, y :: number, z :: number, 48) do
                    if not seen[id] and is_ghost(id) then
                        seen[id] = true
                        local gx, gy, gz = core.mob_pos(id)
                        if gx ~= nil then
                            local g: Ghost = ghosts[id] or { x = gx, y = gy :: number, z = gz :: number, yaw = 0, host = nil }
                            g.x, g.y, g.z = gx, gy :: number, gz :: number
                            ghosts[id] = g
                        end
                    end
                end
            end
        end
        -- One that was there and is not: dead, or gone. It comes apart where it was.
        for id, g in ghosts do
            if not seen[id] then
                ghosts[id] = nil
                for _, p in players do
                    local x, y, z = core.player_pos(p)
                    if x ~= nil and (x - g.x) ^ 2 + (y - g.y) ^ 2 + (z - g.z) ^ 2 < 48 * 48 then
                        tell(p, string.format("x %.2f %.2f %.2f %.2f", g.x, g.y, g.z, g.yaw))
                    end
                end
                if g.host and held[g.host] and held[g.host].ghost == id then
                    let_go(g.host, "The cursed ghost is gone. You are free.")
                end
            end
        end

        -- Taking hold.
        for id, g in ghosts do
            if g.host == nil then
                for _, p in players do
                    local x, y, z = core.player_pos(p)
                    if x ~= nil and held[p] == nil and clock >= (free_at[p] or 0)
                            and (x - g.x) ^ 2 + (z - g.z) ^ 2 < HOLD_REACH * HOLD_REACH
                            and math.abs((y :: number) + 0.6 - g.y) < 2 then
                        take_hold(p, id)
                        break
                    end
                end
            end
        end

        -- Those held: the ghost at their back, where they walk, what they lose.
        for p, h in held do
            local g = ghosts[h.ghost]
            local x, y, z = core.player_pos(p)
            if g == nil or x == nil then
                let_go(p)
                continue
            end
            if clock >= h.steer then
                h.steer = clock + STEER_EVERY
                local dx, dz = steer(p, h)
                if math.abs(dx - h.dx) > 0.01 or math.abs(dz - h.dz) > 0.01 then
                    h.dx, h.dz = dx, dz
                    tell(p, string.format("w %.2f %.2f", dx, dz))
                end
            end
            if clock >= h.drop then
                h.drop = clock + DROP_EVERY
                throw_out(p, h)
            end
            local lx, _, lz = core.player_look(p)
            local fx, fz = lx or h.dx, lz or h.dz
            local n = math.max(math.sqrt(fx * fx + fz * fz), 0.01)
            fx, fz = fx / n, fz / n
            g.x, g.y, g.z = x - fx * 0.6, (y :: number) + 0.5, (z :: number) - fz * 0.6
            g.yaw = math.atan2(fx, fz)
            core.mob_place(h.ghost, g.x, g.y, g.z)
        end

        -- Ten times a second, each player told of the ghosts near them: where,
        -- facing, how far from them, and whether it holds somebody.
        since += ev.dt
        if since < 0.1 then
            return
        end
        since = 0
        for _, p in players do
            local x, y, z = core.player_pos(p)
            if x == nil then
                continue
            end
            local parts = {}
            for id, g in ghosts do
                local d = math.sqrt((x - g.x) ^ 2 + ((y :: number) - g.y) ^ 2 + ((z :: number) - g.z) ^ 2)
                if d < 48 and #parts < 8 then
                    if g.host == nil then
                        g.yaw = math.atan2(x - g.x, (z :: number) - g.z)
                    end
                    table.insert(parts, string.format("%d %.2f %.2f %.2f %.2f %.1f %d", id, g.x, g.y, g.z, g.yaw, d,
                        if g.host == p then 2 elseif g.host then 1 else 0))
                end
            end
            if #parts > 0 or shown[p] then
                tell(p, "p " .. table.concat(parts, ";"))
                shown[p] = #parts > 0
            end
        end
    end)
end

-- ===================================================================== client

if drawing then
    local MODEL = core.model_id("ghost:ghost")
    type Seen = { x: number, y: number, z: number, yaw: number, d: number, held: number,
                  fx: number, fy: number, fz: number, at: number }
    local seen: { [number]: Seen } = {}
    local dying: { { number } } = {}   -- x, y, z, yaw, since
    local owned = false
    local hp = 0
    local clock = 0.0

    heard = function(text: string)
        local w = string.split(text, " ")
        if w[1] == "p" then
            local now: { [number]: boolean } = {}
            for part in string.gmatch(string.sub(text, 3), "[^;]+") do
                local f = string.split(part, " ")
                local id = tonumber(f[1]) :: number
                local s = seen[id]
                local x, y, z = tonumber(f[2]) :: number, tonumber(f[3]) :: number, tonumber(f[4]) :: number
                if s == nil then
                    s = { x = x, y = y, z = z, yaw = 0, d = 99, held = 0, fx = x, fy = y, fz = z, at = clock }
                    seen[id] = s
                end
                -- Drawn from where it was drawn towards where it is, over a tenth.
                local t = math.clamp((clock - s.at) / 0.1, 0, 1)
                s.fx, s.fy, s.fz = s.fx + (s.x - s.fx) * t, s.fy + (s.y - s.fy) * t, s.fz + (s.z - s.fz) * t
                s.x, s.y, s.z, s.at = x, y, z, clock
                s.yaw, s.d, s.held = tonumber(f[5]) :: number, tonumber(f[6]) :: number, tonumber(f[7]) :: number
                now[id] = true
            end
            for id in seen do
                if not now[id] then
                    seen[id] = nil
                end
            end
        elseif w[1] == "x" then
            table.insert(dying, { tonumber(w[2]) :: number, tonumber(w[3]) :: number, tonumber(w[4]) :: number,
                                  tonumber(w[5]) :: number, clock })
        elseif w[1] == "own" then
            owned = w[2] == "1"
            hp = tonumber(w[3]) or 0
            if not owned then
                core.walk(0, 0)
            end
        elseif w[1] == "w" and owned then
            core.walk(tonumber(w[2]) or 0, tonumber(w[3]) or 0)
        end
    end
    if not serving then
        core.event.on("core:net_message", function(ev)
            heard(ev.text :: string)
        end)
    end

    core.event.on("core:tick", function(ev)
        clock += ev.dt
    end)

    -- Every blow swung while held is the ghost's (the server counts them).
    core.event.on("core:input", function(ev)
        if owned and ev.action == "mouse_left" then
            if serving then
                swung_here()
            else
                core.net.send(0, "swing")
            end
        end
    end)

    core.render.pass("ghost:draw", 60, function()
        for id, s in seen do
            local t = math.clamp((clock - s.at) / 0.1, 0, 1)
            local x, y, z = s.fx + (s.x - s.fx) * t, s.fy + (s.y - s.fy) * t, s.fz + (s.z - s.fz) * t
            local bob = math.sin(clock * 2.1 + id) * 0.12
            -- Its clip (tools/make_ghost_rig.py): clutching whoever it
            -- holds, reaching as it comes for somebody, else floating.
            local clip = if s.held > 0 then "hold" elseif s.d < 14 then "fly" else "idle"
            local a
            if s.held == 2 then
                a = 0.25   -- at your own back: hardly in the way of the view
            elseif s.d > FAR then
                -- A shimmer: the air flickering where it is.
                a = 0.03 + 0.05 * (0.5 + 0.5 * math.sin(clock * 9 + id * 3))
            else
                a = 0.15 + 0.5 * math.clamp((FAR - s.d) / (FAR - 2), 0, 1)
            end
            core.render.model(MODEL, x, y + bob, z, s.yaw, 1, 0, 0, 0, a, 0, clip, clock + id * 0.37)
        end
        for i = #dying, 1, -1 do
            local d = dying[i]
            local t = (clock - d[5]) / 1.2
            if t >= 1 then
                table.remove(dying, i)
            else
                core.render.model(MODEL, d[1], d[2] + t * 0.5, d[3], d[4], 1, 0, 0, t, 0.6 * (1 - t * 0.5), 0,
                                  "death", math.min(t * 1.2, 1.19))
            end
        end
    end)

    core.hud.pass("ghost:held", 60, function()
        if not owned then
            return
        end
        local pulse = 0.5 + 0.5 * math.sin(clock * 4)
        core.hud.rect(0.5, 0.5, 4, 1, 60, 200, 140, math.floor(40 + 30 * pulse))
        core.hud.text(0.5, 0.3, "A CURSED GHOST HOLDS YOU", 0.045, 1, 190, 255, 220, 255)
        core.hud.text(0.5, 0.36, "STRIKE TO BREAK FREE", 0.03, 1, 220, 240, 230, 255)
        local blows = math.ceil(hp / BLOW)
        core.hud.text(0.5, 0.41, `{blows} BLOWS LEFT`, 0.025, 1, 200, 230, 210, 255)
    end)
end
