--!strict
-- inferno_heart: the trial for the Chiave del Fuoco (mods/keys). At the end of
-- the arrival chamber's east way a hall of brick, the heart of the fortress,
-- shut by a gate of obsidian. Four braziers stand at the ends of the other
-- ways (west, south, north, and a branch off the north way); a right click
-- lights one, and when all four burn the gate opens. In the hall waits the
-- guardian, a cinder half as big again (tools/make_inferno_creatures.py
-- --guardian); its death gives the key to everybody in the hall. When nobody
-- is left in the Inferno the braziers go out and the gate shuts again.
--
-- Come by a survival site (mods/keys' keys:level), a player gets a level of
-- their own in the site's slot instead (LEVELS below): a fortress of its own
-- out past x 8192, never the hall above, so two sites never meet.

local id = core.node_id
local BRICKS, OBSIDIAN, GLOWROCK = id("inferno:bricks"), id("inferno:obsidian"), id("inferno:glowrock")
local BRAZIER = core.register_node("inferno_heart:brazier", 60, 40, 36, true, 0, "", 0, 0, "stone")
local LIT     = core.register_node("inferno_heart:brazier_lit", 250, 140, 40, true, 14, "", 0, 0, "stone")
local GATE    = core.register_node("inferno_heart:gate", 30, 20, 40, true, 2, "", 9, 999, "stone")
-- A level's way back to its start, in its hall's floor: dark obsidian until
-- the guardian falls.
local RETURN  = core.register_node("inferno_heart:return", 255, 120, 60, true, 12, "", 0, 0, "stone")

local FIREBALL = core.register_model("inferno_heart:fireball", "fireball.gltf")
core.register_mob("inferno_heart:guardian", core.register_model("inferno_heart:guardian", "cinder_guardian.gltf"),
                  120, 2.0, "archer", 4, 20.0, 1.8, "inferno:cinder_rod", 3, FIREBALL, false, 2.5, true,
                  "inferno_heart:never")
core.mob_sight("inferno_heart:guardian", 32)

local FLOOR = 30          -- inferno's arrival floor; its ways are tunnels at this height
local PX, PZ = 32, 32     -- and the chamber's middle
-- Where the survival sites' slots begin (mods/keys: slot k's square from
-- 8192 + (k % 16) * 2048 along x), with a margin: the levels' side.
local LEVELS = 8000

-- The hall: x 92..114, z 21..43, floor 29, roof 42; its gate in the west
-- wall, three wide, three high, facing the east way (which ends at 88).
local HX0, HX1, HZ0, HZ1, ROOF = 92, 114, 21, 43, 42

local BRAZIERS = { { -26, 32 }, { 32, 90 }, { 32, -26 }, { 54, 60 } }

-- Boxes of cells, written by a mapgen stage column by column.
local BOXES: { { number } } = {}
local function box(x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: number)
    table.insert(BOXES, { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1),
                          math.max(x0, x1), math.max(y0, y1), math.max(z0, z1), b })
end
-- A tunnel three wide with a brick floor, from (x0, z0) to (x1, z1) along one axis.
local function tunnel(x0: number, z0: number, x1: number, z1: number)
    local wx, wz = if x0 == x1 then 1 else 0, if z0 == z1 then 1 else 0
    box(x0 - wx, FLOOR - 1, z0 - wz, x1 + wx, FLOOR - 1, z1 + wz, BRICKS)
    box(x0 - wx, FLOOR, z0 - wz, x1 + wx, FLOOR + 3, z1 + wz, 0)
end

-- The hall, hollowed, its walls and roof, glowrock in the corners and a
-- ring of it round the middle where the guardian rises.
box(HX0, FLOOR - 1, HZ0, HX1, ROOF, HZ1, BRICKS)
box(HX0 + 1, FLOOR, HZ0 + 1, HX1 - 1, ROOF - 1, HZ1 - 1, 0)
for _, c in { { HX0 + 1, HZ0 + 1 }, { HX1 - 1, HZ0 + 1 }, { HX0 + 1, HZ1 - 1 }, { HX1 - 1, HZ1 - 1 } } do
    box(c[1], FLOOR, c[2], c[1], ROOF - 1, c[2], GLOWROCK)
end
local MX, MZ = (HX0 + HX1) // 2, (HZ0 + HZ1) // 2
for a = 0, 23 do
    local t = a / 24 * 2 * math.pi
    box(MX + math.floor(math.cos(t) * 5 + 0.5), FLOOR - 1, MZ + math.floor(math.sin(t) * 5 + 0.5),
        MX + math.floor(math.cos(t) * 5 + 0.5), FLOOR - 1, MZ + math.floor(math.sin(t) * 5 + 0.5), GLOWROCK)
end
tunnel(89, PZ, HX0 - 1, PZ)                       -- from the east way to the gate
box(HX0, FLOOR, PZ - 1, HX0, FLOOR + 2, PZ + 1, GATE)

-- The way out (the world's fourth opening, onto the sandbox, facing +x): a
-- frame of obsidian in the hall's east end, out past the guardian.
for y = FLOOR, FLOOR + 3 do
    box(109, y, PZ - 1, 109, y, PZ + 1, OBSIDIAN)
    box(110, y, PZ - 1, 110, y, PZ - 1, OBSIDIAN)
    box(110, y, PZ + 1, 110, y, PZ + 1, OBSIDIAN)
end
box(110, FLOOR + 2, PZ, 110, FLOOR + 3, PZ, GLOWROCK)
core.set_portal(3, 110.6, FLOOR + 1, PZ + 0.5)

-- The braziers: an alcove past the end of each way, the brazier in it.
tunnel(PX - 57, PZ, PX - 59, PZ)
tunnel(PX, PZ + 57, PX, PZ + 59)
tunnel(PX, PZ - 57, PX, PZ - 59)
tunnel(PX + 2, 60, 55, 60)                        -- a branch east off the north way
for _, b in BRAZIERS do box(b[1], FLOOR, b[2], b[1], FLOOR, b[2], BRAZIER) end

core.register_mapgen("inferno_heart:hall", function(ctx)
    for _, b in BOXES do
        local x0, x1 = math.max(b[1], ctx.min_x), math.min(b[4], ctx.max_x)
        local z0, z1 = math.max(b[3], ctx.min_z), math.min(b[6], ctx.max_z)
        if x0 <= x1 and z0 <= z1 then
            for x = x0, x1 do
                for z = z0, z1 do
                    for y = b[2], b[5] do core.set_block(x, y, z, b[7]) end
                end
            end
        end
    end
end)

local function lit_count(): number
    local n = 0
    for _, b in BRAZIERS do
        if core.get_block(b[1], FLOOR, b[2]) == LIT then n += 1 end
    end
    return n
end

local function set_gate(open: boolean)
    for z = PZ - 1, PZ + 1 do
        for y = FLOOR, FLOOR + 2 do core.set_block(HX0, y, z, if open then 0 else GATE) end
    end
end

local level_used: (ev: any) -> ()   -- the levels' braziers and gates, below
core.event.on("core:used", function(ev)
    if ev.x >= LEVELS and (ev.id == BRAZIER or ev.id == LIT or ev.id == GATE) then
        ev.cancel = true
        level_used(ev)
        return
    end
    if ev.id == BRAZIER then
        ev.cancel = true
        core.set_block(ev.x, ev.y, ev.z, LIT)
        local n = lit_count()
        if n == 4 then
            set_gate(true)
            core.notice(0, "Il quarto braciere arde. Il cancello del cuore della fortezza si apre, a est.")
        else
            core.notice(ev.player, `Un braciere acceso: {n} di 4.`)
        end
    elseif ev.id == LIT or ev.id == GATE then
        ev.cancel = true
        if ev.id == GATE then
            core.notice(ev.player, `Il cancello resta chiuso: {lit_count()} bracieri accesi su 4.`)
        end
    end
end)
core.event.on("core:pre_break", function(ev)
    if ev.id == BRAZIER or ev.id == LIT or ev.id == GATE or ev.id == RETURN then ev.cancel = true end
end)

local function in_hall(x: number, z: number): boolean
    return x > HX0 and x < HX1 and z > HZ0 and z < HZ1
end

-- The guardian rises when somebody walks into the open hall; dead, the key
-- to everybody who came into the hall while it lived, dead since or not. Who
-- has come into the open hall comes back after dying at its gate, not at the
-- arrival (a trial's checkpoint).
local guardian, fought, anybody_was = 0, false, false
local fighters: { [number]: boolean } = {}
core.event.on("core:tick", function(_)
    local players = core.players()
    -- The levels' players have their own fortress (below).
    local here = {}
    for _, p in players do
        local x = core.player_pos(p)
        if x ~= nil and x < LEVELS then table.insert(here, p) end
    end
    players = here
    if #players == 0 then
        if anybody_was then
            -- Nobody left: the fire goes out and the gate shuts.
            for _, b in BRAZIERS do core.set_block(b[1], FLOOR, b[2], BRAZIER) end
            set_gate(false)
            guardian, fought, anybody_was = 0, false, false
            fighters = {}
        end
        return
    end
    anybody_was = true
    for _, p in players do
        local x, _, z = core.player_pos(p)
        if x ~= nil and lit_count() == 4 and in_hall(x, z) and not fighters[p] then
            fighters[p] = true
            core.player_spawn(p, 88.5, FLOOR, PZ + 0.5)
        end
    end
    if guardian ~= 0 then
        if core.mob_pos(guardian) == nil then
            guardian = 0
            for _, p in players do
                if fighters[p] then
                    core.notice(p, "Il guardiano e' caduto.")
                    core.event.emit("keys:earned", { player = p, key = "inferno" })
                end
            end
            core.log("inferno_heart: the guardian is dead")
        end
        return
    end
    if fought or lit_count() < 4 then return end
    for _, p in players do
        local x, _, z = core.player_pos(p)
        if x ~= nil and in_hall(x, z) then
            guardian = core.spawn_mob("inferno_heart:guardian", MX + 0.5, FLOOR + 1, MZ + 0.5)
            if guardian ~= 0 then
                fought = true
                core.notice(0, "Dal cerchio di fuoco si leva il guardiano del cuore.")
                core.log("inferno_heart: the guardian rises")
            end
            return
        end
    end
end)

-- ===================================================================== levels
--
-- A survival site's level (mods/keys: keys:level), in its slot's square from
-- (ox, oz): a fortress of brick on the floor of the arrival, built over the
-- caverns. A start room with the way out hidden in its west wall (a dark lamp
-- over two cells of brick), then a piece for each brazier, eastwards, then the
-- gate and a hall where the guardian rises when the braziers burn. One new
-- idea at a time, the newest first:
--   d 1-2  a plain tunnel                         2 braziers
--   d 3-4  a bridge over a lake of lava, brutes   2
--   d 5-6  a gallery of wraiths on ledges, or     3
--          columns of basalt up and down over lava
--   d 7-8  a brazier at the bottom of a pool of lava: fire resistance wanted
--   d 9-10 any of them, and no glowrock: only the braziers light it   4
-- The guardian is 15% sturdier a step of d and has d - 1 of the fortress's
-- own beside it. Its death is the level done (keys:cleared) for whoever came
-- into the hall, and a cell of the hall's floor lights: stood on, it takes
-- them back to the start, where the way out has opened.
local BASALT, LAVA = id("inferno:basalt"), id("inferno:lava")

type Plan = { boxes: { { number } }, start: { number }, exit: { number }, braziers: { { number } },
              gate: { number }, hall: { number }, back: { number }, spawns: { { any } }, d: number }
local PLANS: { [number]: Plan } = {}

local function lcg(seed: number): () -> number
    local st = seed % 2147483647
    if st <= 0 then st += 2147483646 end
    return function()
        st = (st * 48271) % 2147483647
        return st / 2147483647
    end
end

local function slot_at(x: number, z: number): number
    return (x - 8192) // 2048 + 16 * (z // 2048)
end

local function plan_of(slot: number): Plan?
    local hit = PLANS[slot]
    if hit then return hit end
    local ds, ss = string.match(core.storage_get(`slot:{slot}`), "(%d+) (%d+)")
    if ds == nil then return nil end
    local d, rnd = tonumber(ds) or 1, lcg((tonumber(ss) or 1) + slot * 7919)
    local ox, oz = 8192 + (slot % 16) * 2048 + 1024, (slot // 16) * 2048 + 1024
    local F, dark = FLOOR, d >= 9
    -- Brick first, then the air in it, then what stands in the air.
    local solid, air, put = {}, {}, {}
    local function add(list: { { number } }, x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: number)
        table.insert(list, { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1),
                             math.max(x0, x1), math.max(y0, y1), math.max(z0, z1), b })
    end
    local function glow(x: number, y: number, z: number)
        if not dark then add(put, x, y, z, x, y, z, GLOWROCK) end
    end
    local plan: Plan = { boxes = {}, start = { ox + 4, F, oz }, exit = { ox - 5, F, oz }, braziers = {},
                         gate = {}, hall = {}, back = {}, spawns = {}, d = d }

    -- The start room, and the block in its west wall the way out is in: its
    -- hole at exit, facing +x, behind one cell of brick (mods/keys opens it).
    add(solid, ox - 7, F - 1, oz - 7, ox + 7, F + 6, oz + 7, BRICKS)
    add(air, ox - 6, F, oz - 6, ox + 6, F + 5, oz + 6, 0)
    add(put, ox - 6, F, oz - 1, ox - 4, F + 3, oz + 1, BRICKS)
    add(put, ox - 5, F, oz, ox - 5, F + 1, oz, 0)
    for _, c in { { -6, -6 }, { 6, -6 }, { -6, 6 }, { 6, 6 } } do glow(ox + c[1], F + 5, oz + c[2]) end

    -- A tunnel three wide and four high from x0 to x1 (both its end walls
    -- opened), glowrock in its roof every six.
    local function tunnel(x0: number, x1: number)
        add(solid, x0, F - 1, oz - 2, x1, F + 4, oz + 2, BRICKS)
        add(air, x0, F, oz - 1, x1, F + 3, oz + 1, 0)
        for x = x0 + 3, x1 - 1, 6 do glow(x, F + 4, oz) end
    end
    -- A tunnel of eight with a brazier in an alcove off its south side.
    local function tail(x: number): number
        tunnel(x, x + 8)
        add(solid, x + 3, F - 1, oz + 2, x + 5, F + 3, oz + 4, BRICKS)
        add(air, x + 4, F, oz + 2, x + 4, F + 2, oz + 3, 0)
        add(put, x + 4, F, oz + 3, x + 4, F, oz + 3, BRAZIER)
        table.insert(plan.braziers, { x + 4, F, oz + 3 })
        return x + 8
    end
    -- A cavern of brick from x to x + L, its floor a lake of lava four under
    -- the doorways, which open at both ends.
    local function cavern(x: number, L: number, half: number)
        add(solid, x, F - 8, oz - half - 1, x + L, F + 8, oz + half + 1, BRICKS)
        add(air, x + 1, F - 4, oz - half, x + L - 1, F + 7, oz + half, 0)
        add(put, x + 1, F - 7, oz - half, x + L - 1, F - 5, oz + half, LAVA)
        add(air, x, F, oz - 1, x, F + 3, oz + 1, 0)
        add(air, x + L, F, oz - 1, x + L, F + 3, oz + 1, 0)
        for _, c in { { 1, -half }, { L - 1, -half }, { 1, half }, { L - 1, half } } do glow(x + c[1], F + 7, oz + c[2]) end
    end

    local function plain(x: number): number
        tunnel(x, x + 6)
        return tail(x + 6)
    end
    -- A bridge over the lava, three wide, one from d 6; gaps of two from d 4.
    local function bridge(x: number): number
        local L = 18 + 2 * math.min(d, 6)
        cavern(x, L, 6)
        local w = if d >= 6 then 0 else 1
        for bx = x + 1, x + L - 1 do
            local gap = d >= 4 and bx > x + 3 and bx < x + L - 3 and (bx - x) % 7 >= 5
            if not gap then add(put, bx, F - 1, oz - w, bx, F - 1, oz + w, BRICKS) end
        end
        for i = 1, d // 2 do
            local bx = x + math.floor(L * i / (d // 2 + 1))
            if (bx - x) % 7 >= 5 then bx -= 2 end   -- on the bricks, not over a gap
            table.insert(plan.spawns, { bx, F, oz, "inferno:brute", x })
        end
        return tail(x + L)
    end
    -- A tall gallery with a ledge along each wall, wraiths on them.
    local function gallery(x: number): number
        local L = 24
        add(solid, x, F - 1, oz - 5, x + L, F + 9, oz + 5, BRICKS)
        add(air, x, F, oz - 1, x + L, F + 3, oz + 1, 0)
        add(air, x + 1, F, oz - 4, x + L - 1, F + 8, oz + 4, 0)
        add(put, x + 1, F + 4, oz - 4, x + L - 1, F + 4, oz - 3, BRICKS)
        add(put, x + 1, F + 4, oz + 3, x + L - 1, F + 4, oz + 4, BRICKS)
        for gx = x + 4, x + L - 2, 8 do glow(gx, F + 8, oz) end
        for i = 1, 1 + d // 2 do
            table.insert(plan.spawns, { x + 3 + (i * 5) % (L - 4), F + 5, if i % 2 == 0 then oz - 4 else oz + 4,
                                        "inferno:wraith", x })
        end
        return tail(x + L)
    end
    -- Columns of basalt over the lava, two apart, a node up, up, level, down,
    -- down; one across from d 7, else three.
    local function columns(x: number): number
        local L = 22
        cavern(x, L, 4)
        add(put, x + 1, F - 7, oz - 1, x + 1, F - 1, oz + 1, BRICKS)
        add(put, x + L - 1, F - 7, oz - 1, x + L - 1, F - 1, oz + 1, BRICKS)
        local w = if d >= 7 then 0 else 1
        for i, up in { 0, 1, 2, 2, 1, 0 } do
            local cx = x + 1 + 3 * i
            add(put, cx, F - 7, oz - w, cx, F - 1 + up, oz + w, BASALT)
        end
        return tail(x + L)
    end
    -- A room with a pool of lava six deep, the brazier at its bottom.
    local function sunk(x: number): number
        local L = 12
        add(solid, x, F - 8, oz - 4, x + L, F + 5, oz + 4, BRICKS)
        add(air, x, F, oz - 3, x + L, F + 4, oz + 3, 0)
        add(put, x, F, oz - 3, x, F + 4, oz - 2, BRICKS)
        add(put, x, F, oz + 2, x, F + 4, oz + 3, BRICKS)
        add(put, x + L, F, oz - 3, x + L, F + 4, oz - 2, BRICKS)
        add(put, x + L, F, oz + 2, x + L, F + 4, oz + 3, BRICKS)
        add(put, x + 3, F - 6, oz - 2, x + 9, F - 1, oz + 2, LAVA)
        add(put, x + 6, F - 6, oz, x + 6, F - 6, oz, BRAZIER)
        table.insert(plan.braziers, { x + 6, F - 6, oz })
        for _, c in { { 1, -3 }, { L - 1, -3 }, { 1, 3 }, { L - 1, 3 } } do glow(x + c[1], F + 4, oz + c[2]) end
        return x + L
    end

    -- Which pieces: the newest idea for d first, then any of those up to d.
    local POOL = { plain, bridge, gallery, columns, sunk }
    local upto = if d <= 2 then 1 elseif d <= 4 then 2 elseif d <= 6 then 4 else 5
    local newest = if d <= 2 then 1 elseif d <= 4 then 2 elseif d <= 6 then 3 + math.floor(rnd() * 2)
                   elseif d <= 8 then 5 else 1 + math.floor(rnd() * 5)
    local count = if d <= 4 then 2 elseif d <= 8 then 3 else 4
    local order = { newest }
    while #order < count do table.insert(order, 1 + math.floor(rnd() * upto)) end

    local x = ox + 7
    for _, i in order do x = POOL[i](x) end
    tunnel(x, x + 4)
    x += 4
    -- The hall, its gate in the west wall, a ring of glowrock where the
    -- guardian rises (lit even in the dark), the way back in its east end.
    add(solid, x, F - 1, oz - 8, x + 16, F + 10, oz + 8, BRICKS)
    add(air, x + 1, F, oz - 7, x + 15, F + 9, oz + 7, 0)
    add(put, x, F, oz - 1, x, F + 2, oz + 1, GATE)
    for a = 0, 23 do
        local t = a / 24 * 2 * math.pi
        local rx, rz = x + 8 + math.floor(math.cos(t) * 5 + 0.5), oz + math.floor(math.sin(t) * 5 + 0.5)
        add(put, rx, F - 1, rz, rx, F - 1, rz, GLOWROCK)
    end
    for _, c in { { 1, -7 }, { 15, -7 }, { 1, 7 }, { 15, 7 } } do glow(x + c[1], F + 9, oz + c[2]) end
    add(put, x + 14, F - 1, oz, x + 14, F - 1, oz, OBSIDIAN)
    plan.gate, plan.hall, plan.back = { x, F, oz }, { x + 8, F, oz }, { x + 14, F - 1, oz }

    for _, l in { solid, air, put } do
        for _, b in l do table.insert(plan.boxes, b) end
    end
    core.log(`inferno_heart: slot {slot}, d {d}: {#plan.braziers} braziers, the hall at {x} {F} {oz}`)
    PLANS[slot] = plan
    return plan
end

core.register_mapgen("inferno_heart:levels", function(ctx)
    if ctx.min_x < 8192 then return end
    local plan = plan_of(slot_at(ctx.min_x, ctx.min_z))
    if plan == nil then return end
    for _, b in plan.boxes do
        local x0, x1 = math.max(b[1], ctx.min_x), math.min(b[4], ctx.max_x)
        local z0, z1 = math.max(b[3], ctx.min_z), math.min(b[6], ctx.max_z)
        if x0 <= x1 and z0 <= z1 then
            for bx = x0, x1 do
                for bz = z0, z1 do
                    for y = b[2], b[5] do core.set_block(bx, y, bz, b[7]) end
                end
            end
        end
    end
end)

-- A level under way: its guardian, who came into the hall, which spawns are
-- out, and cells to put back as they were once their ground is there.
type Run = { guardian: number, fought: boolean, cleared: boolean, fighters: { [number]: boolean },
             spawned: { [number]: boolean }, pending: { { number } } }
local RUNS: { [number]: Run } = {}

local function in_slot(slot: number): { number }
    local out = {}
    for _, p in core.players() do
        local x, _, z = core.player_pos(p)
        if x ~= nil and x >= LEVELS and slot_at(math.floor(x), math.floor(z)) == slot then table.insert(out, p) end
    end
    return out
end

local function gate_cells(plan: Plan): { { number } }
    local g, out = plan.gate, {}
    for z = g[3] - 1, g[3] + 1 do
        for y = g[2], g[2] + 2 do table.insert(out, { g[1], y, z }) end
    end
    return out
end

-- Fresh: braziers out, the gate shut, the way back dark.
local function reset(slot: number, plan: Plan)
    local run: Run = { guardian = 0, fought = false, cleared = false, fighters = {}, spawned = {}, pending = {} }
    for _, b in plan.braziers do table.insert(run.pending, { b[1], b[2], b[3], BRAZIER }) end
    for _, c in gate_cells(plan) do table.insert(run.pending, { c[1], c[2], c[3], GATE }) end
    table.insert(run.pending, { plan.back[1], plan.back[2], plan.back[3], OBSIDIAN })
    RUNS[slot] = run
end

local function run_of(slot: number): (Run?, Plan?)
    local plan = plan_of(slot)
    if plan == nil then return nil, nil end
    if RUNS[slot] == nil then
        RUNS[slot] = { guardian = 0, fought = false, cleared = false, fighters = {}, spawned = {}, pending = {} }
    end
    return RUNS[slot], plan
end

local function level_lit(plan: Plan): number
    local n = 0
    for _, b in plan.braziers do
        if core.get_block(b[1], b[2], b[3]) == LIT then n += 1 end
    end
    return n
end

core.event.on("keys:level", function(ev)
    core.storage_set(`slot:{ev.slot}`, `{ev.d} {ev.seed}`)
    local plan = plan_of(ev.slot)
    if plan == nil then return end
    ev.cancel = true
    if #in_slot(ev.slot) == 0 then reset(ev.slot, plan) end
    local s = plan.start
    core.player_place(ev.player, s[1] + 0.5, s[2], s[3] + 0.5, 1, 0, 0)
    core.player_spawn(ev.player, s[1] + 0.5, s[2], s[3] + 0.5, true)
    local e = plan.exit
    core.event.emit("keys:start", { player = ev.player, slot = ev.slot, ex = e[1], ey = e[2], ez = e[3] })
    core.notice(ev.player, `Una fortezza: {#plan.braziers} bracieri da accendere, a est, poi il cuore.`)
    if plan.d >= 7 and plan.d <= 8 then
        core.notice(ev.player, "Un braciere e' in fondo alla lava: serve la resistenza al fuoco.")
    end
end)

level_used = function(ev: any)
    local slot = slot_at(ev.x, ev.z)
    local run, plan = run_of(slot)
    if run == nil or plan == nil then return end
    local n = level_lit(plan)
    if ev.id == BRAZIER then
        core.set_block(ev.x, ev.y, ev.z, LIT)
        n += 1
        if n == #plan.braziers then
            for _, c in gate_cells(plan) do core.set_block(c[1], c[2], c[3], 0) end
            for _, p in in_slot(slot) do
                core.notice(p, "L'ultimo braciere arde. Il cancello del cuore si apre, a est.")
            end
        else
            core.notice(ev.player, `Un braciere acceso: {n} di {#plan.braziers}.`)
        end
    elseif ev.id == GATE then
        core.notice(ev.player, `Il cancello resta chiuso: {n} bracieri accesi su {#plan.braziers}.`)
    end
end

core.event.on("core:tick", function(_)
    local slots: { [number]: { number } } = {}
    for _, p in core.players() do
        local x, _, z = core.player_pos(p)
        if x ~= nil and x >= LEVELS then
            local slot = slot_at(math.floor(x), math.floor(z))
            slots[slot] = slots[slot] or {}
            table.insert(slots[slot], p)
        end
    end
    for slot, players in slots do
        local run, plan = run_of(slot)
        if run == nil or plan == nil then continue end
        local left = {}
        for _, c in run.pending do
            if core.loaded(c[1], c[2], c[3]) == 1 then core.set_block(c[1], c[2], c[3], c[4])
            else table.insert(left, c) end
        end
        run.pending = left
        local open = level_lit(plan) == #plan.braziers
        local h = plan.hall
        for _, p in players do
            local x, y, z = core.player_pos(p)
            if x == nil then continue end
            for i, s in plan.spawns do
                if not run.spawned[i] and x >= s[5] - 6 and math.abs(z - s[3]) < 16 then
                    run.spawned[i] = true
                    core.spawn_mob(s[4], s[1] + 0.5, s[2], s[3] + 0.5)
                end
            end
            local inside = x > h[1] - 7 and x < h[1] + 8 and math.abs(z - h[3]) < 8
            if open and inside and not run.fighters[p] then
                run.fighters[p] = true
                core.player_spawn(p, plan.gate[1] - 1.5, h[2], h[3] + 0.5)
            end
            if run.cleared and core.get_block(math.floor(x), math.floor(y - 0.1), math.floor(z)) == RETURN then
                local s = plan.start
                core.player_place(p, s[1] + 0.5, s[2], s[3] + 0.5, -1, 0, 0)
            end
            if open and inside and not run.fought then
                run.fought = true
                run.guardian = core.spawn_mob("inferno_heart:guardian", h[1] + 0.5, h[2] + 1, h[3] + 0.5)
                if run.guardian ~= 0 and plan.d > 1 then
                    -- 15% sturdier a step of d (register_mob made it 120).
                    core.mob_health(run.guardian, math.floor(120 * (1 + 0.15 * (plan.d - 1))))
                end
                for i = 1, plan.d - 1 do
                    local a = i * 2.4
                    core.spawn_mob(if i % 2 == 0 then "inferno:wraith" else "inferno:brute",
                                   h[1] + 0.5 + math.cos(a) * 4, h[2] + 1, h[3] + 0.5 + math.sin(a) * 4)
                end
                for _, q in players do core.notice(q, "Dal cerchio di fuoco si leva il guardiano del cuore.") end
            end
        end
        if run.fought and not run.cleared and run.guardian ~= 0 and core.mob_pos(run.guardian) == nil then
            run.cleared = true
            core.set_block(plan.back[1], plan.back[2], plan.back[3], RETURN)
            for p in run.fighters do
                core.notice(p, "Il guardiano e' caduto. Il fuoco nel pavimento, a est, riporta all'ingresso.")
                core.event.emit("keys:cleared", { player = p, slot = slot })
            end
            core.log(`inferno_heart: slot {slot}: the guardian is dead`)
        end
    end
end)
