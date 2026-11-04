--!strict
-- pocket: a door bigger inside. Put a pocket door down in the sandbox and it
-- becomes a frame round a doorway, one wide and two tall, facing whoever put
-- it down; through it is a hall of that player's own, fifteen by fifteen and
-- six tall, lit, its shell unbreakable. Build a hut of three by three round
-- the door and the hut is bigger inside than out.
--
-- The halls are not in the sandbox: they are in a room of the same server,
-- "pockets", which loads every mod the sandbox loads (so a chest, a furnace,
-- a bed work in a hall as at home) and makes no ground at all -- this mod's
-- stage is the whole world there: hall n stands in its own column of a grid,
-- a function of n alone. Every hall has one door out, in its north wall.
--
-- Four mouths join the rooms (the sandbox's openings onto "pockets", and
-- the pockets room's four back), shared as mods/gates shares its worlds': every
-- half second each sandbox mouth goes to the door nearest a player, and the
-- pockets room moves its own into that door's hall, told through the shared
-- storage, so the doorway shows the hall it leads to. Whoever walks in carries
-- the hall with them ("pocket=n", core.player_carry), is put in it if the
-- mouth was elsewhere by then, and coming back is put in front of their own
-- door whatever the mouth did meanwhile. A hall somebody is in keeps a mouth,
-- so there is a way out; a doorway with no mouth free -- a fifth door
-- somebody is near, a fifth hall somebody is in -- is shut by a panel until
-- one is. With nobody near a door its doorway is empty. A village's mayor
-- sells a hut with a pocket door (mods/villagers, through pocket:put).
--
-- Breaking a block of the frame folds the door: the frame goes, a pocket door
-- comes out, and put down again by the same player it opens on the same hall.
-- Kept in the sandbox's core.storage: "doors" (a line a door) and "folded"
-- (a line a hall put away), "count" the last hall given.
--
-- Shared storage (one store for every room): "pocket:mouth<i>", the hall the
-- i-th mouth is at; "pocket:inside", the halls somebody is in.

local ROOM = core.room()
local serving = core.side() ~= "client"

local DOOR  = core.register_node("pocket:door", 120, 70, 150, true, 0, "", 0, 1.0, "wood")
local FRAME = core.register_node("pocket:frame", 92, 54, 120, true, 3, "", 0, 1.5, "wood")
local WALL  = core.register_node("pocket:wall", 226, 214, 188, true, 0, "", 0, 2.0, "stone")
local LAMP  = core.register_node("pocket:lamp", 255, 236, 190, true, 15, "", 0, 1.0, "glass")
local FLOOR = core.register_node("pocket:floor", 176, 136, 84, true, 0, "", 0, 2.0, "wood")
-- A doorway with no way through now (every mouth in use): shut, so nobody
-- walks into a frame that leads nowhere, and opened again when one is free.
local SHUT  = core.register_node("pocket:shut", 64, 38, 84, true, 0, "", 0, 9.0, "wood")
core.node_drop(FRAME, DOOR)   -- any block of the frame gives the door back

-- For other mods: a pocket door put down without a player placing it.
core.event.define("pocket:put", {
    fields = { x = "number", y = "number", z = "number", axis = "number", sign = "number", owner = "string" },
    cancellable = false,
})

-- A door of wood round a ruby, in a ring of portal frames.
core.recipe_shaped("pocket:door", 1,
    "portal:frame portal:frame portal:frame",
    "portal:frame doors:door   portal:frame",
    "portal:frame ore:ruby     portal:frame")

-- The halls' grid, in the pockets room: hall n's corner at (X0, F - 1, Z0),
-- each well inside its own 32 x 32 column, far from the openings' defaults.
local GRID, ORIGIN, ACROSS = 64, 1024, 16
local F = 20                -- the floor's top: a body stands at y F
local SIZE, TALL = 15, 6    -- inside

local function hall_corner(n: number): (number, number)
    return ORIGIN + (n % ACROSS) * GRID + 7, ORIGIN + (n // ACROSS) * GRID + 7
end

-- Which hall a point is in (inside or in its shell), or nil.
local function hall_at(x: number, y: number, z: number): number?
    local i, j = math.floor((x - ORIGIN) / GRID), math.floor((z - ORIGIN) / GRID)
    if i < 0 or i >= ACROSS or j < 0 then return nil end
    local n = i + j * ACROSS
    local x0, z0 = hall_corner(n)
    if x < x0 or x > x0 + SIZE + 1 or z < z0 or z > z0 + SIZE + 1 or y < F - 1 or y > F + TALL then
        return nil
    end
    return n
end

-- The shell: floor, ceiling and four walls. What nobody may break.
local function in_shell(x: number, y: number, z: number): boolean
    local n = hall_at(x, y, z)
    if n == nil then return false end
    local x0, z0 = hall_corner(n)
    return y == F - 1 or y == F + TALL or x == x0 or x == x0 + SIZE + 1 or z == z0 or z == z0 + SIZE + 1
end

-- The way out of hall n: the middle of the doorway in its north wall, and
-- where a body arriving stands, two in front of it.
local function hall_door(n: number): (number, number, number)
    local x0, z0 = hall_corner(n)
    return x0 + 8.5, F + 1, z0 + 0.5
end
local function hall_arrival(n: number): (number, number, number)
    local x0, z0 = hall_corner(n)
    return x0 + 8.5, F, z0 + 2.5
end

local function hall_of(line: string): number?
    return tonumber(string.match(line, "pocket=(%d+)") or "")
end

local function halls_in(line: string): { [number]: boolean }
    local set: { [number]: boolean } = {}
    for v in string.gmatch(line, "%d+") do set[tonumber(v) :: number] = true end
    return set
end

local function who(p: number): string
    local key = core.player_key(p)
    return if key ~= "" then key else "name:" .. core.player_name(p)
end

-- How many mouths there may be: core.portal_room answers for 0..27.
local MOUTH_MAX = 27

-- A doorway's two cells shut, or open again; never shut on a body in it.
local function set_shut(x: number, y: number, z: number, shut: boolean, bodies: { { number } })
    if shut then
        for _, b in bodies do
            if math.abs(b[1] - (x + 0.5)) < 1.3 and math.abs(b[3] - (z + 0.5)) < 1.3 and b[2] > y - 2 and b[2] < y + 2 then
                return
            end
        end
    end
    for cy = y, y + 1 do
        local id = core.get_block(x, cy, z)
        if shut and id == 0 then
            core.set_block(x, cy, z, SHUT)
        elseif not shut and id == SHUT then
            core.set_block(x, cy, z, 0)
        end
    end
end

---------------------------------------------------------------------------
-- The pockets room: the halls, and its mouths moved into them.
---------------------------------------------------------------------------
if ROOM == "pockets" then
    core.mapgen_core(false)
    core.mapgen_only("pocket")

    core.register_mapgen("pocket:halls", function(ctx)
        local i = (ctx.min_x - ORIGIN) // GRID
        local j = (ctx.min_z - ORIGIN) // GRID
        if i < 0 or i >= ACROSS or j < 0 then return end
        local n = i + j * ACROSS
        local x0, z0 = hall_corner(n)
        if x0 < ctx.min_x or x0 + SIZE + 1 > ctx.max_x or z0 < ctx.min_z or z0 + SIZE + 1 > ctx.max_z then
            return
        end
        for x = x0, x0 + SIZE + 1 do
            for z = z0, z0 + SIZE + 1 do
                local edge = x == x0 or x == x0 + SIZE + 1 or z == z0 or z == z0 + SIZE + 1
                core.set_block(x, F - 1, z, if edge then WALL else FLOOR)
                local lit = not edge and (x - x0) % 4 == 2 and (z - z0) % 4 == 2
                core.set_block(x, F + TALL, z, if lit then LAMP else WALL)
                for y = F, F + TALL - 1 do
                    core.set_block(x, y, z, if edge then WALL else 0)
                end
            end
        end
        -- The doorway out, framed as the door it leads to.
        local dx = x0 + 8
        for y = F, F + 1 do core.set_block(dx, y, z0, 0) end
        for _, c in { { dx - 1, F }, { dx - 1, F + 1 }, { dx + 1, F }, { dx + 1, F + 1 },
                      { dx - 1, F + 2 }, { dx, F + 2 }, { dx + 1, F + 2 } } do
            core.set_block(c[1], c[2], z0, FRAME)
        end
    end)

    if serving then
        -- The shell stays; a pocket door does not go down in a pocket.
        core.event.on("core:pre_break", function(ev)
            if in_shell(ev.x, ev.y, ev.z) then ev.cancel = true end
        end)
        core.event.on("core:pre_place", function(ev)
            if ev.id == DOOR then
                ev.cancel = true
                core.notice(ev.player, "A pocket door does not open inside a pocket")
            end
        end)

        -- This room's mouths: the ones leading back to the sandbox.
        local MOUTHS: { number } = {}
        for k = 0, MOUTH_MAX do
            if core.portal_room(k) == "sandbox" then table.insert(MOUTHS, k) end
        end
        local at: { [number]: number } = {}       -- mouth k: the hall it is in, -1 parked
        local seen: { [number]: boolean } = {}    -- players here at the last look
        local told: { [number]: number } = {}     -- whom the shut way out was said to, by hall
        local since = 0.0

        core.event.on("core:tick", function(ev)
            since += ev.dt
            if since < 0.5 then return end
            since = 0.0

            -- Each mouth into the hall the sandbox says, or out of the way.
            for i, k in MOUTHS do
                local n = tonumber(core.shared_storage_get(`pocket:mouth{i}`)) or -1
                if at[k] ~= n then
                    local ok
                    if n >= 0 then
                        local x, y, z = hall_door(n)
                        ok = core.portal_place(k, x, y, z, 0, 1, 0.5, 1.0)
                    else
                        ok = core.portal_place(k, k * 4 + 0.5, 40, 0.5, 0, 1, 0.5, 1.0)
                    end
                    if ok == 1 then
                        at[k] = n
                        if n >= 0 then core.log(`pocket: mouth {k} into hall {n}`) end
                    end
                end
            end

            local open: { [number]: boolean } = {}
            for _, n in at do
                if n >= 0 then open[n] = true end
            end
            local bodies: { { number } } = {}
            for _, p in core.players() do
                local x, y, z = core.player_pos(p)
                if x and y and z then table.insert(bodies, { x, y, z }) end
            end

            local inside: { string } = {}
            local now: { [number]: boolean } = {}
            for _, p in core.players() do
                now[p] = true
                local x, y, z = core.player_pos(p)
                if x == nil or y == nil or z == nil then continue end
                local here = hall_at(x, y, z)
                local want = hall_of(core.player_carried(p))
                -- Just come, or somewhere no hall is: into the hall carried.
                if (not seen[p] and want and here ~= want) or here == nil then
                    local n = want or 0
                    local ax, ay, az = hall_arrival(n)
                    core.player_place(p, ax, ay, az, 0, 0, 1)
                    here = n
                end
                if here then
                    table.insert(inside, tostring(here))
                    if want ~= here then core.player_carry(p, `pocket={here}`) end
                    -- Its way out: open with a mouth in it, shut without.
                    local x0, z0 = hall_corner(here)
                    set_shut(x0 + 8, F, z0, not open[here], bodies)
                    if not open[here] and told[p] ~= here then
                        told[p] = here
                        core.notice(p, "Every way out is in use: this door opens as soon as one is free")
                    elseif open[here] then
                        told[p] = nil
                    end
                end
            end
            seen = now
            core.shared_storage_set("pocket:inside", table.concat(inside, " "))
        end)
    end
    return
end

---------------------------------------------------------------------------
-- The sandbox: the doors, and its mouths moved to them.
---------------------------------------------------------------------------
local MOUTHS: { number } = {}
for k = 0, MOUTH_MAX do
    if core.portal_room(k) == "pockets" then table.insert(MOUTHS, k) end
end

if not serving then return end

-- A door: its hole's lowest cell, its plane (0 XY, 1 YZ, as portal_place
-- takes it), which way its front faces, whose, and the frame cells it wrote.
type Door = { n: number, x: number, y: number, z: number, axis: number, sign: number, owner: string,
              cells: { { number } } }
local doors: { [number]: Door } = {}
local folded: { [string]: { number } } = {}   -- halls put away, by owner

local function save()
    local lines = {}
    for _, d in doors do
        local cells = {}
        for _, c in d.cells do table.insert(cells, `{c[1]},{c[2]},{c[3]}`) end
        table.insert(lines, `{d.n}|{d.x},{d.y},{d.z}|{d.axis}|{d.sign}|{d.owner}|{table.concat(cells, ";")}`)
    end
    core.storage_set("doors", table.concat(lines, "\n"))
    local f = {}
    for owner, ns in folded do
        for _, n in ns do table.insert(f, `{n}|{owner}`) end
    end
    core.storage_set("folded", table.concat(f, "\n"))
end

for line in string.gmatch(core.storage_get("doors") or "", "[^\n]+") do
    local n, x, y, z, axis, sign, owner, cells =
        string.match(line, "^(%d+)|(-?%d+),(-?%d+),(-?%d+)|(%d)|(-?%d)|([^|]*)|(.*)$")
    if n and x and y and z and axis and sign and owner and cells then
        local d: Door = { n = tonumber(n) :: number, x = tonumber(x) :: number, y = tonumber(y) :: number,
                          z = tonumber(z) :: number, axis = tonumber(axis) :: number,
                          sign = tonumber(sign) :: number, owner = owner, cells = {} }
        for cx, cy, cz in string.gmatch(cells, "(-?%d+),(-?%d+),(-?%d+)") do
            table.insert(d.cells, { tonumber(cx) :: number, tonumber(cy) :: number, tonumber(cz) :: number })
        end
        doors[d.n] = d
    end
end
for line in string.gmatch(core.storage_get("folded") or "", "[^\n]+") do
    local n, owner = string.match(line, "^(%d+)|(.*)$")
    if n and owner then
        folded[owner] = folded[owner] or {}
        table.insert(folded[owner], tonumber(n) :: number)
    end
end

-- The middle of a door's doorway, and the spot in front of it.
local function door_centre(d: Door): (number, number, number)
    return d.x + 0.5, d.y + 1, d.z + 0.5
end
local function door_front(d: Door): (number, number, number)
    local cx, _, cz = door_centre(d)
    if d.axis == 0 then return cx, d.y, cz + d.sign * 1.5 end
    return cx + d.sign * 1.5, d.y, cz
end

local function door_at(x: number, y: number, z: number): Door?
    for _, d in doors do
        if x == d.x and z == d.z and (y == d.y or y == d.y + 1) then return d end
        for _, c in d.cells do
            if c[1] == x and c[2] == y and c[3] == z then return d end
        end
    end
    return nil
end

core.event.on("core:pre_place", function(ev)
    if ev.id == DOOR then
        if #MOUTHS == 0 then
            ev.cancel = true
            core.notice(ev.player, "A pocket door needs a world with its pocket room (start it from the title menu)")
        elseif core.get_block(ev.x, ev.y + 1, ev.z) ~= 0 then
            ev.cancel = true   -- a doorway is two tall
        end
        return
    end
    -- Nothing goes into a doorway.
    for _, d in doors do
        if ev.x == d.x and ev.z == d.z and (ev.y == d.y or ev.y == d.y + 1) then
            ev.cancel = true
            return
        end
    end
end)

-- A door with its doorway at (x, y, z), its front along +-z (axis 0) or +-x
-- (axis 1), for owner: their folded hall if they have one, a new one else.
-- The hall's number, and whether it was theirs already.
-- A hall put away is found under the owner's key or, a door the mayor sold
-- (bought by name), under "name:<name>": also. A door another mod puts
-- (force) has its whole frame, in place of whatever stood there: a mayor's
-- hut is wall all round its doorway, and a frame of wall cannot fold.
local function put_door(x: number, y: number, z: number, axis: number, sign: number, owner: string,
                        also: string?, force: boolean?): (number, boolean)
    local n: number
    local mine = folded[owner]
    if (mine == nil or #mine == 0) and also then
        mine = folded[also]
        if mine and #mine > 0 then owner = also end
    end
    local again = mine ~= nil and #mine > 0
    if mine and #mine > 0 then
        n = table.remove(mine, 1) :: number
    else
        n = (tonumber(core.storage_get("count")) or 0) + 1
        core.storage_set("count", tostring(n))
    end
    local d: Door = { n = n, x = x, y = y, z = z, axis = axis, sign = sign, owner = owner, cells = {} }
    core.set_block(x, y, z, 0)
    core.set_block(x, y + 1, z, 0)
    -- The frame where there is room for it: in a wall it is the wall.
    for _, uv in { { -1, 0 }, { -1, 1 }, { 1, 0 }, { 1, 1 }, { -1, 2 }, { 0, 2 }, { 1, 2 } } do
        local cx = if axis == 0 then x + uv[1] else x
        local cz = if axis == 1 then z + uv[1] else z
        local cy = y + uv[2]
        if force or core.get_block(cx, cy, cz) == 0 then
            core.set_block(cx, cy, cz, FRAME)
            table.insert(d.cells, { cx, cy, cz })
        end
    end
    doors[n] = d
    save()
    core.log(`pocket: door {n} at {x} {y} {z} (axis {axis}, facing {sign}) for {owner}`)
    return n, again
end

core.event.on("core:placed", function(ev)
    if ev.id ~= DOOR then return end
    local px, _, pz = core.player_pos(ev.player)
    local dx, dz = (px or ev.x + 0.5) - (ev.x + 0.5), (pz or ev.z + 1.5) - (ev.z + 0.5)
    local axis = if math.abs(dz) >= math.abs(dx) then 0 else 1
    local ahead = if axis == 0 then dz else dx
    local _, again = put_door(ev.x, ev.y, ev.z, axis, if ahead < 0 then -1 else 1, who(ev.player),
                              "name:" .. core.player_name(ev.player))
    core.notice(ev.player, if again then "The door opens on your hall again" else "A hall of your own is behind this door")
end)

-- Another mod's door (a plot the mayor sells): owner as core.player_key gives
-- it, or "name:<name>".
core.event.on("pocket:put", function(ev)
    if #MOUTHS == 0 or (ev.axis ~= 0 and ev.axis ~= 1) then return end
    put_door(math.floor(ev.x), math.floor(ev.y), math.floor(ev.z), ev.axis, if ev.sign < 0 then -1 else 1, ev.owner,
             nil, true)
end)

-- A block of the frame broken: the door folds up, the hall kept for its owner.
core.event.on("core:broken", function(ev)
    if ev.id ~= FRAME then return end
    local d = door_at(ev.x, ev.y, ev.z)
    if d == nil then return end
    for _, c in d.cells do
        if core.get_block(c[1], c[2], c[3]) == FRAME then core.set_block(c[1], c[2], c[3], 0) end
    end
    set_shut(d.x, d.y, d.z, false, {})
    doors[d.n] = nil
    folded[d.owner] = folded[d.owner] or {}
    table.insert(folded[d.owner], d.n)
    save()
    core.notice(ev.player, "The pocket door folds up; put down again by its owner, it opens on the same hall")
end)

-- The mouths, every half second.
local SX, SY, SZ = core.world_spawn()
local spawn_x, spawn_y, spawn_z = SX or 0, SY or 21, SZ or 0
local NEAR = 24
local at: { [number]: number } = {}         -- mouth k: the hall it is at, -1 parked
local seen: { [number]: boolean } = {}
local near_door: { [number]: number } = {}  -- each player: the door they stand at
local told: { [number]: number } = {}       -- whom a shut door was explained to, by door
local since = 0.0

-- Mouth k to hall n: at its door, or -- a hall with nobody's door, somebody
-- inside -- beside the spawn, so they have a way out.
local function place(k: number, n: number): boolean
    local d = doors[n]
    local ok
    if n < 0 then
        ok = core.portal_place(k, spawn_x + k * 3 + 0.5, 4, spawn_z + 0.5, 1, 1, 0.5, 1.0)
    elseif d then
        local x, y, z = door_centre(d)
        ok = core.portal_place(k, x, y, z, d.axis, d.sign, 0.5, 1.0)
    else
        ok = core.portal_place(k, spawn_x + 4.5 + k, spawn_y + 1, spawn_z + 0.5, 1, 1, 0.5, 1.0)
    end
    return ok == 1
end

core.event.on("core:tick", function(ev)
    if #MOUTHS == 0 then return end
    since += ev.dt
    if since < 0.5 then return end
    since = 0.0

    -- Wanted: the halls somebody is in first, then the doors nearest a player.
    local want: { { n: number, dist: number } } = {}
    local function wanted(n: number, dist: number)
        for _, w in want do
            if w.n == n then
                w.dist = math.min(w.dist, dist)
                return
            end
        end
        table.insert(want, { n = n, dist = dist })
    end
    for n in halls_in(core.shared_storage_get("pocket:inside")) do wanted(n, -1) end

    local now: { [number]: boolean } = {}
    for _, p in core.players() do
        now[p] = true
        local x, y, z = core.player_pos(p)
        if x == nil or y == nil or z == nil then continue end
        -- Back from a hall: in front of its door, wherever the mouth is now.
        if not seen[p] then
            local n = hall_of(core.player_carried(p))
            local d = if n then doors[n] else nil
            if d then
                local fx, fy, fz = door_front(d)
                if (x - fx) ^ 2 + (z - fz) ^ 2 > 6 * 6 then
                    local cx, _, cz = door_centre(d)
                    core.player_place(p, fx, fy, fz, fx - cx, 0, fz - cz)
                end
            end
            if n then core.player_carry(p, "") end
        end
        near_door[p] = nil
        for _, d in doors do
            local cx, cy, cz = door_centre(d)
            local dist = (cx - x) ^ 2 + (cy - y) ^ 2 + (cz - z) ^ 2
            if dist < NEAR * NEAR then wanted(d.n, dist) end
            if dist < 4 * 4 then near_door[p] = d.n end
        end
        -- Close enough to walk in: the hall goes with them.
        local n = near_door[p]
        if n and hall_of(core.player_carried(p)) ~= n then core.player_carry(p, `pocket={n}`) end
    end
    seen = now

    table.sort(want, function(a, b) return a.dist < b.dist end)
    local chosen: { [number]: boolean } = {}
    for i = 1, math.min(#want, #MOUTHS) do chosen[want[i].n] = true end
    local free: { number } = {}
    for _, k in MOUTHS do
        local cur = at[k]
        if cur and cur >= 0 and chosen[cur] then chosen[cur] = nil else table.insert(free, k) end
    end
    for i = 1, math.min(#want, #MOUTHS) do
        local n = want[i].n
        if chosen[n] and #free > 0 then
            local k = table.remove(free, 1) :: number
            if place(k, n) then
                at[k] = n
                core.log(`pocket: mouth {k} to hall {n}`)
            end
            chosen[n] = nil
        end
    end
    for _, k in free do
        if at[k] ~= -1 and place(k, -1) then at[k] = -1 end
    end
    for i, k in MOUTHS do
        core.shared_storage_set(`pocket:mouth{i}`, if (at[k] or -1) >= 0 then tostring(at[k]) else "")
    end

    -- The doors somebody is near: open with a mouth in them, shut without
    -- (every mouth at a door nearer somebody, or at a hall somebody is in).
    local open: { [number]: boolean } = {}
    for _, n in at do
        if n >= 0 then open[n] = true end
    end
    local bodies: { { number } } = {}
    for _, p in core.players() do
        local x, y, z = core.player_pos(p)
        if x and y and z then table.insert(bodies, { x, y, z }) end
    end
    for _, w in want do
        local d = doors[w.n]
        if d then set_shut(d.x, d.y, d.z, not open[d.n], bodies) end
    end
    for _, p in core.players() do
        local n = near_door[p]
        if n and not open[n] and told[p] ~= n then
            told[p] = n
            core.notice(p, "Every pocket way is in use: this door opens as soon as one is free")
        elseif n == nil or open[n] then
            told[p] = nil
        end
    end
end)

-- What shuts a doorway is not for breaking.
core.event.on("core:pre_break", function(ev)
    if ev.id == SHUT then ev.cancel = true end
end)

local count = 0
for _ in doors do count += 1 end
core.log(`pocket: {#MOUTHS} mouth(s) onto the pockets room, {count} door(s)`)
