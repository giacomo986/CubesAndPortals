--!strict
-- backrooms_exit: the trial for the Chiave dell'Uscita (mods/keys). Somewhere
-- in Level 1's dark -- one room, the same for everybody, over a hundred
-- nodes from the origin -- is the way out the engineer found: a room lit
-- green, a doorway in each wall, and the key on mods/keys' altar by its way
-- out (and the torch, his diary says, that will matter in the laboratory). Nothing
-- points to it but a radio: whoever is on Level 1 hears it crackle from the
-- way the exit lies, more often the nearer they are. Coming down to Level 1
-- is a checkpoint: a death there comes back to where they came down. The
-- world's way home is moved here, in a corner of the exit room.

local id = core.node_id
local SLAB, CONCRETE = id("backrooms:slab"), id("backrooms:concrete")
local SIGN  = core.register_node("backrooms_exit:sign", 60, 220, 90, true, 13, "", 0, 0, "stone")

local F, H = 5, 6            -- Level 1: the first cell of air, and how many up to its ceiling
local X0, Z0 = 96, -72       -- the exit room: room (12, -9), eight across, its low walls at X0, Z0
local MX, MZ = X0 + 4.5, Z0 + 4.5

-- The room, written after the backrooms' own stage over its column: its four
-- walls with a doorway two wide in the middle of each, the floor, lights.
core.register_mapgen("backrooms_exit:room", function(ctx)
    local function put(x: number, y: number, z: number, b: number)
        if x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z then
            core.set_block(x, y, z, b)
        end
    end
    for x = X0, X0 + 8 do
        for z = Z0, Z0 + 8 do
            put(x, F - 1, z, SLAB)
            local edge = x == X0 or x == X0 + 8 or z == Z0 or z == Z0 + 8
            local door = (x == X0 or x == X0 + 8) and (z == Z0 + 4 or z == Z0 + 5)
                or (z == Z0 or z == Z0 + 8) and (x == X0 + 4 or x == X0 + 5)
            for y = F, F + H - 1 do
                put(x, y, z, if edge and not (door and y < F + 3) then CONCRETE else 0)
            end
            put(x, F + H, z, CONCRETE)
        end
    end
    -- A green sign over each doorway, inside; a lamp in the ceiling.
    for _, s in { { X0 + 1, Z0 + 4 }, { X0 + 7, Z0 + 4 }, { X0 + 4, Z0 + 1 }, { X0 + 4, Z0 + 7 } } do
        put(s[1], F + 3, s[2], SIGN)
    end
    put(X0 + 4, F + H, Z0 + 4, SIGN)
    -- And the way home, the world's second opening (facing +x), in a frame of
    -- concrete in the room's south-west corner: the way out is not the way in.
    for y = F, F + 3 do
        for z = Z0 + 1, Z0 + 3 do put(X0 + 1, y, z, CONCRETE) end
        put(X0 + 2, y, Z0 + 1, CONCRETE)
        put(X0 + 2, y, Z0 + 3, CONCRETE)
    end
    put(X0 + 2, F, Z0 + 2, 0)
    put(X0 + 2, F + 1, Z0 + 2, 0)
    put(X0 + 2, F + 2, Z0 + 2, SIGN)
end)
core.set_portal(1, X0 + 2.6, F + 1, Z0 + 2.5)
core.event.on("core:pre_break", function(ev)
    if ev.id == SIGN then ev.cancel = true end
end)

local clock = 0
type P = { next_beep: number, next_word: number, down: boolean, found: boolean }
local players: { [number]: P } = {}

local function word(d: number): string
    return if d < 30 then "forte" elseif d < 80 then "chiaro" else "debole"
end

core.event.on("core:tick", function(ev)
    clock += ev.dt
    local here: { [number]: boolean } = {}
    for _, p in core.players() do
        here[p] = true
        local s = players[p]
        if s == nil then
            s = { next_beep = 0, next_word = 0, down = false, found = false }
            players[p] = s
        end
        local x, y, z = core.player_pos(p)
        if x == nil then continue end
        if y >= 17 then continue end   -- Level 1 is under 17 (backrooms' level_at)
        if not s.down then
            s.down = true
            core.player_spawn(p, x, y, z)
            core.notice(p, "Una radio gracchia nel buio, lontano. Segui il segnale.")
        end
        local dx, dz = MX - x, MZ - z
        local d = math.sqrt(dx * dx + dz * dz)
        if x >= X0 + 1 and x <= X0 + 8 and z >= Z0 + 1 and z <= Z0 + 8 then
            if not s.found then
                s.found = true
                core.notice(p, "L'uscita. La torcia dell'ingegnere e' rimasta qui.")
                core.event.emit("keys:earned", { player = p, key = "backrooms" })
            end
            continue
        end
        if clock >= s.next_beep then
            -- From the way the exit lies, four nodes off: the engine's sound
            -- is heard from where it is played.
            local k = math.min(d, 4) / math.max(d, 0.001)
            core.play_sound("backrooms_exit:radio", x + dx * k, F + 1, z + dz * k)
            s.next_beep = clock + math.max(1.5, math.min(6, d / 25))
        end
        if clock >= s.next_word then
            s.next_word = clock + 25
            core.notice(p, `Il segnale della radio e' {word(d)}.`)
        end
    end
    for p in players do
        if not here[p] then players[p] = nil end
    end
end)
