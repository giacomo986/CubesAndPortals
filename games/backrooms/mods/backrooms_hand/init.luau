--!strict
-- backrooms_hand: a right click with the torch in hand, wherever the player
-- looks. core:used comes only from a block within reach, and the torch is for
-- a creature down a corridor: so the player's machine claims the click
-- (core:input) and tells the server, which lights it: a beam along the look
-- that stuns, for STUN seconds, the creatures in it (core.slow_mobs at a
-- twentieth of their pace: they barely walk and do not strike), then a
-- RECHARGE before it lights again. The server tells every machine where a
-- torch was lit, and each draws its beam a moment. The item and where it is
-- found are mods/backrooms'. The compasses are read clicking a block,
-- so that a click with one in hand still opens a chest.

local HANDLED = { ["backrooms:torch"] = true }
local side = core.side()

local EYE = 1.5        -- mods/backrooms' core.eye_height
local STUN = 4.0       -- seconds a creature in the beam is stunned
local RECHARGE = 6.0   -- seconds before the same torch lights again
local REACH = 14       -- nodes the beam reaches

if side ~= "client" then
    core.net.open()
    local clock = 0.0
    local ready: { [number]: number } = {}   -- by player: when their torch lights again
    function on_step(dt: number)
        clock += dt
    end
    core.event.on("core:net_message", function(ev)
        local p = ev.player
        if ev.text ~= "aim" or not HANDLED[core.held_item(p)] then
            return
        end
        if clock < (ready[p] or 0) then
            core.notice(p, "La torcia si sta ricaricando.")
            return
        end
        local x, y, z = core.player_pos(p)
        local lx, ly, lz = core.player_look(p)
        if x == nil or y == nil or z == nil or lx == nil or ly == nil or lz == nil then
            return
        end
        ready[p] = clock + RECHARGE
        -- The beam's cone as balls along it, wider further out; a creature
        -- in two of them is stunned once.
        local n = 0
        for d = 1, REACH, 2.5 do
            n += core.slow_mobs(x + lx * d, y + EYE + ly * d - 0.8, z + lz * d, 1.2 + d * 0.15, 0.05, STUN)
        end
        core.net.send(0, string.format("torch %.2f %.2f %.2f %.3f %.3f %.3f", x, y + EYE, z, lx, ly, lz))
        if n > 0 then
            core.notice(p, "La luce le stordisce.")
        end
    end)
end

if side ~= "server" then
    -- Where a torch was lit: eye, look, and how long its beam has left.
    local beams: { { number } } = {}
    core.event.on("core:net_message", function(ev)
        local w = string.split(ev.text, " ")
        if w[1] == "torch" then
            local b = {}
            for i = 2, 7 do b[i - 1] = tonumber(w[i]) or 0 end
            b[7] = 0.6
            table.insert(beams, b)
        end
    end)
    core.event.on("core:input", function(ev)
        if ev.action == "mouse_right" and HANDLED[core.held_item(1)] then
            ev.cancel = true
            core.net.send(0, "aim")
        end
    end)
    -- The engine's step: in a process that is its own server, after the
    -- server half's.
    local server_step = on_step
    function on_step(dt: number)
        if server_step then server_step(dt) end
        for i = #beams, 1, -1 do
            beams[i][7] -= dt
            if beams[i][7] <= 0 then table.remove(beams, i) end
        end
    end
    -- A cone of white light along the look, widening and fading: games/lab's
    -- torch, from a couple of nodes out (closer, it filled the screen).
    core.render.pass("backrooms_hand:torch", 100, function()
        for _, b in beams do
            local a = math.floor(220 * b[7] / 0.6)
            for k = 2, 18 do
                local d = k * 0.8
                local r = d * 0.2
                local size = 0.05 + d * 0.004
                for j = 0, 5 do
                    local ang = j / 6 * 2 * math.pi + k
                    local ux, uz = math.cos(ang) * r, math.sin(ang) * r
                    core.render.box(b[1] + b[4] * d + ux, b[2] + b[5] * d + uz * 0.5, b[3] + b[6] * d + uz,
                                    size, size, size, 255, 250, 215, a)
                end
            end
        end
    end)
end
