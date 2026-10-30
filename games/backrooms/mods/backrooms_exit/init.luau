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

local WALLPAPER, CARPET = id("backrooms:wallpaper"), id("backrooms:carpet")

-- A survival site's level (mods/keys: keys:level) in its slot, from (ox, oz)
-- in the endless rooms, which are the same there as anywhere: a room to
-- start in on Level 0 -- its way out hidden in its west wall, under a dark
-- lamp -- and an exit room like this world's own, 48 + 24 d away the way the
-- seed points, on Level 0 up to d 3 and in Level 1's dark from d 4. The
-- radio leads there; reaching it is the level done, and its signal takes
-- the player back to the start room, where the wall has opened.
type Plan = { sx: number, sz: number, ex: number, ez: number, eF: number }
local PLANS: { [number]: Plan } = {}
local level_slot: { [number]: number } = {}
local function plan_of(slot: number): Plan?
    local hit = PLANS[slot]
    if hit then return hit end
    local d, seed = string.match(core.storage_get(`slot:{slot}`), "(%d+) (%d+)")
    if d == nil then return nil end
    local dd = tonumber(d) or 1
    local ox, oz = 8192 + (slot % 16) * 2048 + 1024, (slot // 16) * 2048 + 1024
    local a = ((tonumber(seed) or 0) % 360) * math.pi / 180
    local r = 48 + 24 * dd
    local plan: Plan = { sx = ox, sz = oz, ex = math.floor(ox + math.cos(a) * r), ez = math.floor(oz + math.sin(a) * r),
                         eF = if dd >= 4 then F else 21 }
    PLANS[slot] = plan
    return plan
end

-- The room, written after the backrooms' own stage over its column: its four
-- walls with a doorway two wide in the middle of each, the floor, lights.
local function exit_room(ctx: any, X0: number, Z0: number, F: number, wall: number, floor: number, home: boolean)
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
                put(x, y, z, if edge and not (door and y < F + 3) then wall else 0)
            end
            put(x, F - 1, z, floor)
            put(x, F + H, z, wall)
        end
    end
    -- A green sign over each doorway, inside; a lamp in the ceiling.
    for _, s in { { X0 + 1, Z0 + 4 }, { X0 + 7, Z0 + 4 }, { X0 + 4, Z0 + 1 }, { X0 + 4, Z0 + 7 } } do
        put(s[1], F + 3, s[2], SIGN)
    end
    put(X0 + 4, F + H, Z0 + 4, SIGN)
    if not home then return end
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
end

core.register_mapgen("backrooms_exit:room", function(ctx)
    exit_room(ctx, X0, Z0, F, CONCRETE, SLAB, true)
    if ctx.min_x < 8192 then return end
    local plan = plan_of((ctx.min_x - 8192) // 2048 + 16 * (ctx.min_z // 2048))
    if plan == nil then return end
    -- The start room, and its niche: a block of wall west of it with a hole
    -- one wide and two high, facing +x, behind two cells of its west wall.
    exit_room(ctx, plan.sx, plan.sz, 21, WALLPAPER, CARPET, false)
    local function put(x: number, y: number, z: number, b: number)
        if x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z then
            core.set_block(x, y, z, b)
        end
    end
    for x = plan.sx - 3, plan.sx - 1 do
        for z = plan.sz + 1, plan.sz + 3 do
            for y = 21, 25 do put(x, y, z, WALLPAPER) end
        end
    end
    put(plan.sx - 1, 21, plan.sz + 2, 0)
    put(plan.sx - 1, 22, plan.sz + 2, 0)
    for y = 21, 22 do put(plan.sx, y, plan.sz + 2, WALLPAPER) end   -- the doorway there, walled
    exit_room(ctx, plan.ex, plan.ez, plan.eF, if plan.eF == F then CONCRETE else WALLPAPER,
              if plan.eF == F then SLAB else CARPET, false)
end)
core.set_portal(1, X0 + 2.6, F + 1, Z0 + 2.5)

core.event.on("keys:level", function(ev)
    core.storage_set(`slot:{ev.slot}`, `{ev.d} {ev.seed}`)
    local plan = plan_of(ev.slot)
    if plan == nil then return end
    ev.cancel = true
    level_slot[ev.player] = ev.slot
    core.player_place(ev.player, plan.sx + 7.5, 21, plan.sz + 6.5, -1, 0, 0)
    core.player_spawn(ev.player, plan.sx + 7.5, 21, plan.sz + 6.5, true)
    core.event.emit("keys:start", { player = ev.player, slot = ev.slot, ex = plan.sx - 1, ey = 21, ez = plan.sz + 2 })
    core.notice(ev.player, "Una radio gracchia, lontano. L'uscita e' dove il segnale e' forte.")
end)
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
        local slot = level_slot[p]
        local plan = if slot then plan_of(slot) else nil
        if plan and slot then
            -- A level: the radio from wherever its exit room is, on any level.
            local cx, cz = plan.ex + 4.5, plan.ez + 4.5
            local dx, dz = cx - x, cz - z
            local d = math.sqrt(dx * dx + dz * dz)
            if x >= plan.ex + 1 and x <= plan.ex + 8 and z >= plan.ez + 1 and z <= plan.ez + 8
                and math.abs(y - plan.eF) < 3 then
                core.event.emit("keys:cleared", { player = p, slot = slot })
                core.notice(p, "Il segnale ti riporta indietro. Nella stanza dove sei arrivato qualcosa e' cambiato.")
                core.player_place(p, plan.sx + 4.5, 21, plan.sz + 4.5, -1, 0, 0)
                continue
            end
            if clock >= s.next_beep then
                local k = math.min(d, 4) / math.max(d, 0.001)
                core.play_sound("backrooms_exit:radio", x + dx * k, y + 1, z + dz * k)
                s.next_beep = clock + math.max(1.5, math.min(6, d / 25))
            end
            if clock >= s.next_word then
                s.next_word = clock + 25
                core.notice(p, `Il segnale della radio e' {word(d)}` ..
                    (if plan.eF == F and y >= 17 then ", e viene da sotto." else "."))
            end
            continue
        end
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
        if not here[p] then
            players[p] = nil
            level_slot[p] = nil
        end
    end
end)
