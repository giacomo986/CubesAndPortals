--!strict
-- arpg_crypt: the trial for the Chiave della Cripta (mods/keys). At the end
-- of arpg_game's second zone a mausoleum's steps go down into three floors
-- of a crypt, each deeper than the last and open to the sky -- the camera
-- looks down from above, as everywhere in this world. A floor wakes when
-- somebody comes down into it: its dead rise, and the seal on the way down
-- to the next floor stays until they are all down again. On the third floor
-- waits the Custode delle tombe (tools/make_crypt_boss.py), who calls up
-- skeletons while it stands; its death gives the key to everybody who came
-- down to it. With nobody left in the world the crypt sleeps again.

local id = core.node_id
local COBBLE, MOSSY, ROCK = id("arpg_core:cobble"), id("arpg_core:mossy"), id("arpg_core:rock")
local GLOW, FIRE, GRAVE = id("arpg_core:waystone"), id("arpg_core:brazier"), id("arpg_core:grave")
local SEAL = core.register_node("arpg_crypt:seal", 70, 110, 200, true, 9, "", 9, 999, "stone")

local FIREBALL = core.register_model("arpg_crypt:fireball", "fireball.gltf")
--                  name                    model                                                hp  speed fight    dmg reach every drop n shot      doors hover fire  spawn_on
-- A thousand points as arpg_core's creatures have: arpg_game keeps its real life.
core.register_mob("arpg_crypt:custode", core.register_model("arpg_crypt:custode", "custode.gltf"), 1000, 2.2, "archer", 6, 14, 1.8, "", 1, FIREBALL, false, 0, true, "arpg_crypt:never")
core.mob_sight("arpg_crypt:custode", 30)

local GROUND = 20   -- arpg_core's ground: the top block is at 19

-- Where arpg_game's layout puts the crypt: everything below is drawn at these
-- offsets, and arpg_game opens the ground over it (CRYPT_DX, CRYPT_DZ there).
local DX, DZ = 1317, 1013

-- The floors: inside x0..x1, z0..z1, standing at fy + 1. The way down from
-- each is a stair along +x, three wide (z 58..60), its seal at sx.
type Floor = { x0: number, x1: number, z0: number, z1: number, fy: number, sx: number?,
               dead: { { any } } }
local FLOORS: { Floor } = {
    { x0 = DX + 84, x1 = DX + 103, z0 = DZ + 50, z1 = DZ + 69, fy = 15, sx = DX + 104,
      dead = { { "zombie", 6 }, { "skeleton", 2 } } },
    { x0 = DX + 112, x1 = DX + 131, z0 = DZ + 50, z1 = DZ + 69, fy = 13, sx = DX + 132,
      dead = { { "zombie", 5 }, { "skeleton", 4 }, { "imp", 2 } } },
    { x0 = DX + 140, x1 = DX + 163, z0 = DZ + 48, z1 = DZ + 71, fy = 11, dead = {} },
}
local SZ0, SZ1 = DZ + 58, DZ + 60

-- Boxes of cells, written by a mapgen stage column by column.
local BOXES: { { number } } = {}
local function box(x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: number)
    table.insert(BOXES, { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1),
                          math.max(x0, x1), math.max(y0, y1), math.max(z0, z1), b })
end

-- A floor: walls from its floor to the ground, the floor, open to the sky.
for k, f in FLOORS do
    box(f.x0 - 1, f.fy, f.z0 - 1, f.x1 + 1, GROUND - 1, f.z1 + 1, if k % 2 == 0 then MOSSY else COBBLE)
    box(f.x0, f.fy + 1, f.z0, f.x1, GROUND - 1, f.z1, 0)
    box(f.x0, f.fy, f.z0, f.x1, f.fy, f.z1, ROCK)
    -- Fire in the corners on a ledge, graves in rows.
    for _, c in { { f.x0, f.z0 }, { f.x1, f.z0 }, { f.x0, f.z1 }, { f.x1, f.z1 } } do
        box(c[1], f.fy + 3, c[2], c[1], f.fy + 3, c[2], FIRE)
    end
    for gx = f.x0 + 3, f.x1 - 3, 5 do
        for _, gz in { f.z0 + 3, f.z1 - 3 } do box(gx, f.fy + 1, gz, gx, f.fy + 1, gz, GRAVE) end
    end
end
-- The stairs: from the ground into the first floor, and from each floor to
-- the next, a cell down at a time, walled, the seal at the top.
local function stair(xa: number, ya: number, xb: number, yb: number)
    -- From x xa at floor ya to x xb at floor yb, ya > yb, one down a cell.
    local y = ya
    for x = xa, xb do
        if y > yb then y -= 1 end
        box(x, y, SZ0, x, y, SZ1, COBBLE)
        box(x, y + 1, SZ0, x, GROUND - 1, SZ1, 0)
        box(x, y, SZ0 - 1, x, GROUND - 1, SZ0 - 1, MOSSY)
        box(x, y, SZ1 + 1, x, GROUND - 1, SZ1 + 1, MOSSY)
    end
end
stair(DX + 78, GROUND - 1, FLOORS[1].x0 - 1, FLOORS[1].fy)
stair(FLOORS[1].x1 + 1, FLOORS[1].fy, FLOORS[2].x0 - 1, FLOORS[2].fy)
stair(FLOORS[2].x1 + 1, FLOORS[2].fy, FLOORS[3].x0 - 1, FLOORS[3].fy)
-- The mausoleum over the way in: two pillars with a waystone each.
for _, z in { SZ0 - 2, SZ1 + 2 } do
    box(DX + 77, GROUND, z, DX + 77, GROUND + 2, z, COBBLE)
    box(DX + 77, GROUND + 3, z, DX + 77, GROUND + 3, z, GLOW)
end
box(DX + 77, GROUND + 3, SZ0 - 1, DX + 77, GROUND + 3, SZ1 + 1, MOSSY)

-- The world's way home (its second opening, facing +x) moved down here, in
-- a frame of cobble near the third floor's east wall: out after the Custode,
-- not back up the steps.
local OUT = { DX + 160, FLOORS[3].fy + 1, DZ + 60 }
for y = OUT[2], OUT[2] + 3 do
    box(OUT[1] - 1, y, OUT[3] - 1, OUT[1] - 1, y, OUT[3] + 1, COBBLE)
    box(OUT[1], y, OUT[3] - 1, OUT[1], y, OUT[3] - 1, MOSSY)
    box(OUT[1], y, OUT[3] + 1, OUT[1], y, OUT[3] + 1, MOSSY)
end
box(OUT[1], OUT[2] + 2, OUT[3], OUT[1], OUT[2] + 2, OUT[3], GLOW)
box(OUT[1], OUT[2] + 3, OUT[3], OUT[1], OUT[2] + 3, OUT[3], COBBLE)
core.set_portal(1, OUT[1] + 0.6, OUT[2] + 1, OUT[3] + 0.5)

core.register_mapgen("arpg_crypt:crypt", function(ctx)
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

local function set_seal(f: Floor, shut: boolean)
    if f.sx == nil then return end
    for z = SZ0, SZ1 do
        -- The stair's first cell is one below the floor: a body there stands at fy.
        for y = f.fy, f.fy + 2 do core.set_block(f.sx :: number, y, z, if shut then SEAL else 0) end
    end
end

core.event.on("core:pre_break", function(ev)
    if ev.id == SEAL then ev.cancel = true end
end)

local function floor_of(x: number, y: number, z: number): number
    for k, f in FLOORS do
        if x >= f.x0 and x <= f.x1 + 1 and z >= f.z0 and z <= f.z1 and y < GROUND - 1 and y >= f.fy then
            return k
        end
    end
    return 0
end

-- Per floor: woken, the ids of its dead still standing, cleared.
local woke: { [number]: boolean } = {}
local risen: { [number]: { number } } = {}
local cleared: { [number]: boolean } = {}
local custode, summon_at, clock = 0, 0, 0
local throne = { 0, 0, 0 }   -- where the Custode rises, and is kept near
local below: { [number]: boolean } = {}   -- who came down to the Custode
local anybody_was = false

local function rise(k: number)
    local f = FLOORS[k]
    woke[k] = true
    risen[k] = {}
    for _, d in f.dead do
        for _ = 1, d[2] do
            local x = f.x0 + 2 + math.random() * (f.x1 - f.x0 - 4)
            local z = f.z0 + 2 + math.random() * (f.z1 - f.z0 - 4)
            local m = core.spawn_mob(`arpg_core:{d[1]}`, x, f.fy + 1, z)
            if m ~= 0 then
                table.insert(risen[k], m)
                core.event.emit("arpg:spawned", { id = m, kind = `arpg_core:{d[1]}` })
            end
        end
    end
    if k == #FLOORS then
        throne = { (f.x0 + f.x1) / 2 + 4, f.fy + 1, (f.z0 + f.z1) / 2 + 0.5 }
        custode = core.spawn_mob("arpg_crypt:custode", throne[1], throne[2], throne[3])
        core.event.emit("arpg:spawned", { id = custode, kind = "arpg_crypt:custode" })
        summon_at = clock + 6
        core.notice(0, "Il Custode delle tombe si alza dal suo trono.")
    else
        core.notice(0, `Il piano {k} della cripta si sveglia.`)
    end
end

local function standing(ids: { number }): number
    local n = 0
    for _, m in ids do
        if core.mob_pos(m) ~= nil then n += 1 end
    end
    return n
end

core.event.on("core:tick", function(ev)
    clock += ev.dt
    local players = core.players()
    if #players == 0 then
        if anybody_was then
            -- Nobody left: the crypt sleeps, the seals close.
            for _, f in FLOORS do set_seal(f, true) end
            woke, risen, cleared, below = {}, {}, {}, {}
            custode, anybody_was = 0, false
        end
        return
    end
    if not anybody_was then
        anybody_was = true
        for _, f in FLOORS do set_seal(f, true) end
    end
    for _, p in players do
        local x, y, z = core.player_pos(p)
        if x == nil then continue end
        local k = floor_of(x, y, z)
        if k ~= 0 then
            if not woke[k] then rise(k) end
            if k == #FLOORS then below[p] = true end
        end
    end
    for k, f in FLOORS do
        if woke[k] and not cleared[k] and k < #FLOORS and standing(risen[k]) == 0 then
            cleared[k] = true
            set_seal(f, false)
            core.notice(0, `Il sigillo verso il piano {k + 1} si spezza.`)
        end
    end
    -- The Custode: skeletons at its side every few seconds, up to four.
    if custode ~= 0 then
        local cx, cy, cz = core.mob_pos(custode)
        if cx == nil then
            custode = 0
            for _, p in players do
                if below[p] then
                    core.notice(p, "Il Custode e' caduto.")
                    core.event.emit("keys:earned", { player = p, key = "arpg" })
                end
            end
            core.log("arpg_crypt: the custode is dead")
        elseif (cx - throne[1]) ^ 2 + (cz - throne[3]) ^ 2 > 25 then
            -- Kept by its throne: backing off from a body near it, it reached
            -- the strip the floor's walls (eight high) hide from the camera,
            -- and shot from there unseen, out of a warrior's reach.
            local fx, fy, fz = math.floor(cx), math.floor(cy + 0.01), math.floor(cz)
            core.push_mobs(fx, fy, fz, math.floor(throne[1]) - fx, throne[2] - fy, math.floor(throne[3]) - fz)
        elseif clock >= summon_at then
            summon_at = clock + 8
            if standing(risen[#FLOORS]) < 4 then
                for _ = 1, 2 do
                    local m = core.spawn_mob("arpg_core:skeleton", cx + (math.random() - 0.5) * 4, cy,
                                             cz + (math.random() - 0.5) * 4)
                    if m ~= 0 then
                        table.insert(risen[#FLOORS], m)
                        core.event.emit("arpg:spawned", { id = m, kind = "arpg_core:skeleton" })
                    end
                end
            end
        end
    end
end)
