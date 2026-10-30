--!strict
-- inferno_heart: the trial for the Chiave del Fuoco (mods/keys). At the end of
-- the arrival chamber's east way a hall of brick, the heart of the fortress,
-- shut by a gate of obsidian. Four braziers stand at the ends of the other
-- ways (west, south, north, and a branch off the north way); a right click
-- lights one, and when all four burn the gate opens. In the hall waits the
-- guardian, a cinder half as big again (tools/make_inferno_creatures.py
-- --guardian); its death gives the key to everybody in the hall. When nobody
-- is left in the Inferno the braziers go out and the gate shuts again.

local id = core.node_id
local BRICKS, OBSIDIAN, GLOWROCK = id("inferno:bricks"), id("inferno:obsidian"), id("inferno:glowrock")
local BRAZIER = core.register_node("inferno_heart:brazier", 60, 40, 36, true, 0, "", 0, 0, "stone")
local LIT     = core.register_node("inferno_heart:brazier_lit", 250, 140, 40, true, 14, "", 0, 0, "stone")
local GATE    = core.register_node("inferno_heart:gate", 30, 20, 40, true, 2, "", 9, 999, "stone")

local FIREBALL = core.register_model("inferno_heart:fireball", "fireball.gltf")
core.register_mob("inferno_heart:guardian", core.register_model("inferno_heart:guardian", "cinder_guardian.gltf"),
                  120, 2.0, "archer", 4, 20.0, 1.8, "inferno:cinder_rod", 3, FIREBALL, false, 2.5, true,
                  "inferno_heart:never")
core.mob_sight("inferno_heart:guardian", 32)

local FLOOR = 30          -- inferno's arrival floor; its ways are tunnels at this height
local PX, PZ = 32, 32     -- and the chamber's middle

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

core.event.on("core:used", function(ev)
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
    if ev.id == BRAZIER or ev.id == LIT or ev.id == GATE then ev.cancel = true end
end)

local function in_hall(x: number, z: number): boolean
    return x > HX0 and x < HX1 and z > HZ0 and z < HZ1
end

-- The guardian rises when somebody walks into the open hall; dead, the key
-- to everybody who came into the hall while it lived, dead since or not. Who
-- has come into the open hall comes back after dying at its gate, not at the
-- arrival (a trial's checkpoint).
local guardian, fought, anybody_was = 0, false, false
-- Come in by a survival site (mods/keys' keys:level), the heart is that site's
-- level: the guardian rises with d - 1 of the fortress's own beside it, brutes
-- and wraiths in turn. Set by the first to come before the fight; gone when
-- the fire goes out.
local level_d = 1
core.event.on("keys:level", function(ev)
    if not fought then level_d = math.max(level_d, ev.d) end
end)
local fighters: { [number]: boolean } = {}
core.event.on("core:tick", function(_)
    local players = core.players()
    if #players == 0 then
        if anybody_was then
            -- Nobody left: the fire goes out and the gate shuts.
            for _, b in BRAZIERS do core.set_block(b[1], FLOOR, b[2], BRAZIER) end
            set_gate(false)
            guardian, fought, anybody_was = 0, false, false
            fighters = {}
            level_d = 1
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
                -- And 15% sturdier a step of d (register_mob made it 120).
                if level_d > 1 then core.mob_health(guardian, math.floor(120 * (1 + 0.15 * (level_d - 1)))) end
                for i = 1, level_d - 1 do
                    local a = i * 2.4
                    core.spawn_mob(if i % 2 == 0 then "inferno:wraith" else "inferno:brute",
                                   MX + 0.5 + math.cos(a) * 4, FLOOR + 1, MZ + 0.5 + math.sin(a) * 4)
                end
                core.notice(0, "Dal cerchio di fuoco si leva il guardiano del cuore.")
                core.log("inferno_heart: the guardian rises")
            end
            return
        end
    end
end)
