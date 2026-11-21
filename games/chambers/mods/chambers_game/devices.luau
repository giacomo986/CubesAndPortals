--!strict
-- chambers_game's devices: what a chamber has besides its buttons, doors,
-- vents and plates, and what works through the portals.
--
--   a light bridge     an emitter in a wall lays a floor of hard light
--                      straight out of it, a cell at a time, until something
--                      solid stops it. Met by a portal it goes on out of the
--                      one that is linked, the way that one faces: shoot one
--                      where it meets the wall and the other where it is
--                      wanted. Only level: out of a portal in a floor or a
--                      ceiling it ends, a floor stood on end being a wall.
--   a funnel           an emitter whose beam carries what is in it, a body
--                      or a cube, slowly along it and holds it in the middle,
--                      through portals the same way -- up, too: out of a
--                      portal in the floor it is a lift.
--   gel                a dispenser in a ceiling pours a stream of it, which
--                      falls, goes through portals as anything falling does
--                      (out of the other at the speed it went in) and paints
--                      the floor where it lands. Blue gel throws a body that
--                      comes onto it up (harder the faster it came down);
--                      orange gel lets one run more than twice as fast while
--                      on it, and what it has when it leaves it is kept in
--                      the air.
--   an angled panel    two cells by two in the floor, hinged on one side: a
--                      push button flips it up and back, throwing whatever is
--                      on it the way it faces.
--
-- The SERVER traces the beams and the streams; the blocks a bridge lays and
-- the gel a stream paints are blocks every client has as it has any other.
-- What is light only -- a funnel's beam, a stream's drops, a panel's swing --
-- each client draws from what it is told ("funnel", "stream", "flip").
--
-- Every portal a beam may meet is said here as it opens (opened), closes
-- (closed) and is linked (link, in place of core.portal_link): the cells of
-- the wall or floor it is in and the way it faces.

local M = {}

local GRAVITY = 25          -- chambers_core's core.movement
local WALK, SPRINT, JUMP = 4.8, 8, 8
local FAST_WALK, FAST_SPRINT = 12, 16   -- on orange gel
local BOUNCE = 15           -- at least: a body thrown 4.5 up off blue gel, past a wall four high
local FUNNEL_SPEED = 3.0
local REACH = 64            -- the longest a beam goes, through every portal it meets
local HOPS = 6              -- and how many portals it goes through at most

local function key(x: number, y: number, z: number): string
    return `{x},{y},{z}`
end

-- =================================================================== server

export type End = { cells: { { number } }, nx: number, ny: number, nz: number }
type Seg = { x: number, y: number, z: number, dx: number, dy: number, dz: number, n: number }
export type Emitter = { x: number, y: number, z: number, dx: number, dy: number, dz: number, room: number,
                        id: number, laid: { [string]: { number } }, sig: string, segs: { Seg } }
export type Dispenser = { x: number, y: number, z: number, kind: string, spread: number, room: number, id: number,
                          painted: { [string]: { number } }, sig: string, land: { number }? }
export type Flip = { x: number, y: number, z: number, vx: number, vy: number, vz: number,
                     push: { number }, room: number, up: number }

function M.server(tell: (number, string) -> (), cubes: { [number]: boolean })
    local D = {}
    local function id(name: string): number
        return core.node_id(`chambers_core:{name}`)
    end
    local BRIDGE, GEL_BOUNCE, GEL_SPEED = id("bridge"), id("gel_bounce"), id("gel_speed")
    local FLIP, FLIP_PART = id("flip"), id("flip_part")
    D.BRIDGE, D.FLIP, D.FLIP_PART = BRIDGE, FLIP, FLIP_PART

    -- What a beam, a stream or a shot goes through as if it were air: the
    -- light of a bridge, gel lying on a floor, anything not solid.
    local function clear(b: number): boolean
        return b == 0 or b == BRIDGE or not core.node_solid(b)
    end
    D.clear = clear
    function D.gel(b: number): boolean
        return b == GEL_BOUNCE or b == GEL_SPEED
    end
    -- And a cell known to be so: one not in memory reads as air and is not.
    local function open(x: number, y: number, z: number): boolean
        return core.loaded(x, y, z) and clear(core.get_block(x, y, z))
    end

    -- ------------------------------------------------------ the portals

    local ends: { [number]: End } = {}
    local links: { [number]: number } = {}
    local at_cell: { [string]: number } = {}   -- a portal's cell: which portal
    local version = 0                           -- anything about them changed

    function D.opened(idx: number, cells: { { number } }, nx: number, ny: number, nz: number)
        D.closed(idx)
        ends[idx] = { cells = cells, nx = nx, ny = ny, nz = nz }
        for _, c in cells do
            at_cell[key(c[1], c[2], c[3])] = idx
            -- Gel lying where the opening now is, gone into it.
            local b = core.get_block(c[1] + nx, c[2] + ny, c[3] + nz)
            if b == GEL_BOUNCE or b == GEL_SPEED then
                core.set_block(c[1] + nx, c[2] + ny, c[3] + nz, 0)
            end
        end
        version += 1
    end
    function D.closed(idx: number)
        local e = ends[idx]
        if e ~= nil then
            for _, c in e.cells do
                local k = key(c[1], c[2], c[3])
                if at_cell[k] == idx then at_cell[k] = nil end
            end
            ends[idx] = nil
            version += 1
        end
    end
    -- core.portal_link, kept: a and b each lead to the other, -1 to nothing.
    function D.link(a: number, b: number): boolean
        local was = links[a]
        if was ~= nil and links[was] == a then links[was] = nil end
        if b >= 0 then
            local wb = links[b]
            if wb ~= nil and links[wb] == b then links[wb] = nil end
            links[b] = a
        end
        links[a] = if b >= 0 then b else nil
        version += 1
        return core.portal_link(a, b)
    end

    -- Through the portal whose cell (x, y, z) is, met going along (dx, dy, dz):
    -- the cell of the other end it comes out of and the way it then goes, or
    -- nil when that cell is no portal, is met from behind or leads nowhere.
    local function through(x: number, y: number, z: number, dx: number, dy: number,
                           dz: number): (number?, number, number, number, number, number)
        local idx = at_cell[key(x, y, z)]
        if idx == nil then return nil, 0, 0, 0, 0, 0 end
        local e = ends[idx] :: End
        if e.nx * dx + e.ny * dy + e.nz * dz >= 0 then return nil, 0, 0, 0, 0, 0 end
        local other = links[idx]
        local f = if other ~= nil then ends[other] else nil
        if f == nil then return nil, 0, 0, 0, 0, 0 end
        local i = 1
        for k, c in e.cells do
            if c[1] == x and c[2] == y and c[3] == z then i = k end
        end
        local c = f.cells[math.min(i, #f.cells)]
        return c[1] + f.nx, c[2] + f.ny, c[3] + f.nz, f.nx, f.ny, f.nz
    end

    -- A beam from the cell (x, y, z) along an axis: its straight pieces, each
    -- n cells from its first; through the portals it meets.
    local function trace(x: number, y: number, z: number, dx: number, dy: number, dz: number,
                         level: boolean): { Seg }
        local segs: { Seg } = {}
        if not open(x, y, z) then return segs end
        local seg: Seg = { x = x, y = y, z = z, dx = dx, dy = dy, dz = dz, n = 1 }
        table.insert(segs, seg)
        local hops, left = 0, REACH
        while left > 0 do
            left -= 1
            local nx, ny, nz = x + dx, y + dy, z + dz
            -- A portal's cell first: the panel it is in may not be solid.
            local ox, oy, oz, odx, ody, odz = through(nx, ny, nz, dx, dy, dz)
            if ox == nil then
                if open(nx, ny, nz) and at_cell[key(nx, ny, nz)] == nil then
                    x, y, z = nx, ny, nz
                    seg.n += 1
                    continue
                end
                break
            end
            if hops >= HOPS or (level and ody ~= 0) then break end
            if not open(ox, oy, oz) then break end
            hops += 1
            x, y, z, dx, dy, dz = ox, oy, oz, odx, ody, odz
            seg = { x = x, y = y, z = z, dx = dx, dy = dy, dz = dz, n = 1 }
            table.insert(segs, seg)
        end
        return segs
    end
    local function sig_of(segs: { Seg }): string
        local out = {}
        for _, s in segs do
            table.insert(out, `{s.x},{s.y},{s.z},{s.dx},{s.dy},{s.dz},{s.n}`)
        end
        return table.concat(out, ";")
    end

    -- Somebody, or a cube, in the cell: a bridge is not laid through a body.
    local function body_in(x: number, y: number, z: number): boolean
        for _, p in core.players() do
            local px, py, pz = core.player_pos(p)
            if px ~= nil and math.abs(px - x - 0.5) < 0.85 and math.abs(pz - z - 0.5) < 0.85
                and py < y + 0.1 and py > y - 1.75 then
                return true
            end
        end
        for c in cubes do
            local cx, cy, cz = core.mob_pos(c)
            if cx ~= nil and math.abs(cx - x - 0.5) < 0.9 and math.abs(cz - z - 0.5) < 0.9
                and cy < y + 0.1 and cy > y - 0.75 then
                return true
            end
        end
        return false
    end

    -- ------------------------------------------------------ what they laid

    -- Every cell a bridge or a stream has laid, kept in the world's storage:
    -- the portals that carried them do not outlive a run, and what they laid
    -- would, out over a pit, made by nothing. Taken away at the next start.
    local kept: { [string]: boolean } = {}
    local dirty = false
    local function keep(k: string, on: boolean)
        if (kept[k] == true) ~= on then
            kept[k] = if on then true else nil
            dirty = true
        end
    end
    local function save_kept()
        if dirty then
            dirty = false
            local out = {}
            for k in kept do table.insert(out, k) end
            core.storage_set("devices laid", table.concat(out, ";"))
        end
    end
    function D.forget_saved()
        for c in string.gmatch(core.storage_get("devices laid"), "[^;]+") do
            local x, y, z = string.match(c, "^(-?%d+),(-?%d+),(-?%d+)$")
            if x then
                local cx, cy, cz = tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number
                local b = core.get_block(cx, cy, cz)
                if b == BRIDGE or b == GEL_BOUNCE or b == GEL_SPEED then core.set_block(cx, cy, cz, 0) end
            end
        end
        core.storage_set("devices laid", "")
    end

    -- ------------------------------------------------------ the devices

    local bridges: { Emitter } = {}
    local funnels: { Emitter } = {}
    local dispensers: { Dispenser } = {}
    local flips: { Flip } = {}
    local next_id = 0

    local function emitter(e: { [string]: any }): Emitter
        next_id += 1
        return { x = e.x, y = e.y, z = e.z, dx = e.dx, dy = e.dy or 0, dz = e.dz, room = e.room, id = next_id,
                 laid = {}, sig = "", segs = {} }
    end
    function D.bridge(e: { [string]: any })
        table.insert(bridges, emitter(e))
    end
    function D.funnel(e: { [string]: any })
        table.insert(funnels, emitter(e))
    end
    function D.dispenser(e: { [string]: any })
        next_id += 1
        table.insert(dispensers, { x = e.x, y = e.y, z = e.z, kind = e.kind, spread = e.spread or 1, room = e.room,
                                   id = next_id, painted = {}, sig = "" })
    end
    function D.flip(e: { [string]: any })
        table.insert(flips, { x = e.x, y = e.y, z = e.z, vx = e.vx, vy = e.vy, vz = e.vz, push = e.push,
                              room = e.room, up = 0 })
    end

    -- Somebody near enough for a device to be worth working: a device far from
    -- everybody keeps what it last laid.
    local function near(x: number, y: number, z: number): boolean
        for _, p in core.players() do
            local px, py, pz = core.player_pos(p)
            if px ~= nil and (px - x) ^ 2 + (pz - z) ^ 2 < 48 * 48 and math.abs(py - y) < 40 then
                return true
            end
        end
        return false
    end

    -- A bridge's light laid again where its beam goes now, and taken away where
    -- it went and no longer does.
    local function step_bridge(b: Emitter)
        local segs = trace(b.x + b.dx, b.y + b.dy, b.z + b.dz, b.dx, b.dy, b.dz, true)
        local want: { [string]: { number } } = {}
        for _, s in segs do
            if s.dy ~= 0 then continue end
            for i = 0, s.n - 1 do
                local x, y, z = s.x + s.dx * i, s.y, s.z + s.dz * i
                want[key(x, y, z)] = { x, y, z }
            end
        end
        for k, c in b.laid do
            if want[k] == nil then
                if core.get_block(c[1], c[2], c[3]) == BRIDGE then core.set_block(c[1], c[2], c[3], 0) end
                b.laid[k] = nil
                keep(k, false)
            end
        end
        for k, c in want do
            if b.laid[k] == nil then
                local was = core.get_block(c[1], c[2], c[3])
                if was == BRIDGE then
                    b.laid[k] = c   -- another bridge's too: whichever goes last takes it away
                elseif was == 0 and not body_in(c[1], c[2], c[3]) then
                    core.set_block(c[1], c[2], c[3], BRIDGE)
                    b.laid[k] = c
                    keep(k, true)
                end
            end
        end
        local sig = sig_of(segs)
        if sig ~= b.sig then
            b.sig = sig
            b.segs = segs
            local out = {}
            for _, s in segs do
                if s.dy == 0 then table.insert(out, `{s.x} {s.y} {s.z} {s.dx} {s.dy} {s.dz} {s.n}`) end
            end
            tell(0, `bridge {b.id} {table.concat(out, " ")}`)
        end
    end

    local function step_funnel(f: Emitter)
        local segs = trace(f.x + f.dx, f.y + f.dy, f.z + f.dz, f.dx, f.dy, f.dz, false)
        local sig = sig_of(segs)
        if sig ~= f.sig then
            f.sig = sig
            f.segs = segs
            local out = {}
            for _, s in segs do
                table.insert(out, `{s.x} {s.y} {s.z} {s.dx} {s.dy} {s.dz} {s.n}`)
            end
            tell(0, `funnel {f.id} {table.concat(out, " ")}`)
        end
    end

    -- Where a body at (x, y, z) -- its middle -- is in a funnel: the speed it
    -- is given, along the beam and in towards its middle; nil outside.
    local function in_funnel(x: number, y: number, z: number): (number?, number, number)
        for _, f in funnels do
            for _, s in f.segs do
                -- Along the piece from its first cell's middle, and off its line.
                local ax, ay, az = s.x + 0.5, s.y + 0.5, s.z + 0.5
                local rx, ry, rz = x - ax, y - ay, z - az
                local along = rx * s.dx + ry * s.dy + rz * s.dz
                if along < -0.6 or along > s.n - 0.4 then continue end
                local ox, oy, oz = rx - s.dx * along, ry - s.dy * along, rz - s.dz * along
                if ox * ox + oy * oy + oz * oz > 0.95 * 0.95 then continue end
                return s.dx * FUNNEL_SPEED - ox * 3, s.dy * FUNNEL_SPEED - oy * 3, s.dz * FUNNEL_SPEED - oz * 3
            end
        end
        return nil, 0, 0
    end

    -- ------------------------------------------------------------- gel

    -- A stream: where a drop out of the dispenser goes, falling and through
    -- portals, until it lands. Its path as the arcs it is made of, each where
    -- it starts, its speed there and how long it lasts (seven numbers an arc:
    -- what a client draws the drops along), and the cell it lands in.
    local function pour(d: Dispenser): ({ number }, { number }?)
        local x, y, z = d.x + 0.5, d.y - 0.05, d.z + 0.5
        local vx, vy, vz = 0.0, -1.0, 0.0
        local arcs: { number } = {}
        local t, t0 = 0.0, 0.0
        local sx, sy, sz, svx, svy, svz = x, y, z, vx, vy, vz
        local function arc_ends()
            for _, v in { sx, sy, sz, svx, svy, svz, t - t0 } do table.insert(arcs, v) end
            sx, sy, sz, svx, svy, svz, t0 = x, y, z, vx, vy, vz, t
        end
        local dt = 0.02
        local hops = 0
        for _ = 1, 600 do
            local nvy = vy - GRAVITY * dt
            local nx, ny, nz = x + vx * dt, y + (vy + nvy) * 0.5 * dt, z + vz * dt
            local cx, cy, cz = math.floor(nx), math.floor(ny), math.floor(nz)
            local moved = cx ~= math.floor(x) or cy ~= math.floor(y) or cz ~= math.floor(z)
            if moved and not core.loaded(cx, cy, cz) then
                break   -- not known yet: drawn as far as it is
            end
            if moved and (at_cell[key(cx, cy, cz)] ~= nil or not clear(core.get_block(cx, cy, cz))) then
                local ox, oy, oz, odx, ody, odz = nil, 0, 0, 0, 0, 0
                -- The way it went into that cell, along the axis it crossed.
                local fx, fy, fz = math.floor(x), math.floor(y), math.floor(z)
                local ax, ay, az = cx - fx, cy - fy, cz - fz
                if ay ~= 0 then ax, az = 0, 0 elseif ax ~= 0 then az = 0 end
                if ax ~= 0 or ay ~= 0 or az ~= 0 then
                    ox, oy, oz, odx, ody, odz = through(cx, cy, cz, ax, ay, az)
                end
                if ox ~= nil and hops < HOPS then
                    -- Out of the other at the speed it went in, the way that one faces.
                    hops += 1
                    local speed = math.sqrt(vx * vx + vy * vy + vz * vz)
                    arc_ends()
                    x, y, z = ox + 0.5, oy + 0.5, oz + 0.5
                    vx, vy, vz = odx * speed, ody * speed, odz * speed
                    sx, sy, sz, svx, svy, svz = x, y, z, vx, vy, vz
                    continue
                end
                if ay == 0 or ay > 0 then
                    -- Against a wall it runs down it; against a ceiling it
                    -- stops going up and falls.
                    arc_ends()
                    if ay > 0 then vy = 0 else vx, vz = 0, 0 end
                    svx, svy, svz = vx, vy, vz
                    continue
                end
                -- On a floor: it lands in the cell it was in.
                arc_ends()
                return arcs, { fx, fy, fz }
            end
            x, y, z, vy = nx, ny, nz, nvy
            t += dt
        end
        arc_ends()
        return arcs, nil
    end

    -- The gel a stream lays round where it lands: the floor's cells within
    -- `spread`, each an empty cell over something solid, at the same height.
    local function paint(d: Dispenser, at: { number })
        local gel = if d.kind == "speed" then GEL_SPEED else GEL_BOUNCE
        local r = d.spread
        for ox = -r, r do
            for oz = -r, r do
                if ox * ox + oz * oz > r * r + r then continue end
                local x, y, z = at[1] + ox, at[2], at[3] + oz
                local b = core.get_block(x, y, z)
                local under = core.get_block(x, y - 1, z)
                if (b == 0 or b == GEL_BOUNCE or b == GEL_SPEED) and b ~= gel and core.node_solid(under)
                    and at_cell[key(x, y - 1, z)] == nil then
                    core.set_block(x, y, z, gel)
                    d.painted[key(x, y, z)] = { x, y, z }
                    keep(key(x, y, z), true)
                end
            end
        end
    end

    local function step_dispenser(d: Dispenser)
        local arcs, land = pour(d)
        local parts = {}
        for _, v in arcs do
            table.insert(parts, string.format("%.2f", v))
        end
        local sig = table.concat(parts, " ")
        if sig ~= d.sig then
            d.sig = sig
            tell(0, `stream {d.id} {d.kind} {sig}`)
        end
        d.land = land
        if land ~= nil then paint(d, land) end
    end

    -- A room starting again: the gel its dispensers laid, gone.
    function D.room_starts(room: number)
        for _, d in dispensers do
            if d.room == room then
                for k, c in d.painted do
                    local b = core.get_block(c[1], c[2], c[3])
                    if b == GEL_BOUNCE or b == GEL_SPEED then core.set_block(c[1], c[2], c[3], 0) end
                    d.painted[k] = nil
                    keep(k, false)
                end
            end
        end
    end

    -- What gel a body stands on: its feet's cell and the ones its box reaches.
    local function gel_under(x: number, y: number, z: number): number
        local cy = math.floor(y + 0.05)
        for _, o in { { 0, 0 }, { 0.3, 0.3 }, { -0.3, 0.3 }, { 0.3, -0.3 }, { -0.3, -0.3 } } do
            local b = core.get_block(math.floor(x + o[1]), cy, math.floor(z + o[2]))
            if b == GEL_BOUNCE or b == GEL_SPEED then return b end
        end
        return 0
    end
    local function on_ground(x: number, y: number, z: number): boolean
        return y - math.floor(y) < 0.1 and not clear(core.get_block(math.floor(x), math.floor(y) - 1, math.floor(z)))
    end

    -- Each player's last few positions: how fast they go, and how fast they
    -- came down before they stopped.
    type Body = { x: number, y: number, z: number, vx: number, vy: number, vz: number, fall: number,
                  fall_at: number, bounced: number, fast: boolean, funnel: boolean }
    local bodies: { [number]: Body } = {}
    local clock = 0.0
    local cube_bounced: { [number]: number } = {}

    local function step_bodies(dt: number)
        local here = {}
        for _, p in core.players() do
            here[p] = true
            local x, y, z = core.player_pos(p)
            if x == nil then continue end
            local b = bodies[p]
            if b == nil then
                b = { x = x, y = y, z = z, vx = 0, vy = 0, vz = 0, fall = 0, fall_at = 0, bounced = 0, fast = false,
                      funnel = false }
                bodies[p] = b
            end
            local k = 1 / math.max(dt, 1e-3)
            -- A crossing moves a body many metres in a tick: not a speed.
            if (x - b.x) ^ 2 + (y - b.y) ^ 2 + (z - b.z) ^ 2 < 4 then
                b.vx, b.vy, b.vz = (x - b.x) * k, (y - b.y) * k, (z - b.z) * k
            end
            b.x, b.y, b.z = x, y, z
            if b.vy < b.fall or clock - b.fall_at > 0.3 then
                b.fall, b.fall_at = math.min(b.vy, 0), clock
            end
            local gel = gel_under(x, y, z)
            -- Blue: thrown up, at least BOUNCE, as fast as it came down when that
            -- is more; the way it was going it goes on.
            if gel == GEL_BOUNCE and clock - b.bounced > 0.35 and b.vy <= 0.5 then
                local up = math.max(BOUNCE, -b.fall * 0.95)
                core.player_launch(p, b.vx, math.min(up, 30), b.vz, 1.5)
                b.bounced = clock
                b.fall = 0
            end
            -- Orange: fast while on it; back to the world's own pace once on
            -- something else (the speed it left with is kept in the air).
            if gel == GEL_SPEED and not b.fast then
                b.fast = true
                core.body_movement(p, FAST_WALK, FAST_SPRINT, 0, JUMP, -GRAVITY)
            elseif gel ~= GEL_SPEED and b.fast and on_ground(x, y, z) then
                b.fast = false
                core.body_movement(p, WALK, SPRINT, 0, JUMP, -GRAVITY)
            end
            -- In a funnel: carried along it, the fall it would have between two
            -- ticks given back.
            local fx, fy, fz = in_funnel(x, y + 0.9, z)
            if fx ~= nil then
                core.player_launch(p, fx, fy + GRAVITY * dt * 0.5, fz, 0.6)
                b.funnel = true
            elseif b.funnel then
                b.funnel = false
            end
        end
        for p in bodies do
            if not here[p] then bodies[p] = nil end
        end
        for c in cubes do
            local x, y, z = core.mob_pos(c)
            if x == nil then continue end
            local fx, fy, fz = in_funnel(x, y + 0.35, z)
            if fx ~= nil then
                core.mob_velocity(c, fx, fy + GRAVITY * dt * 0.5, fz)
            elseif gel_under(x, y, z) == GEL_BOUNCE and clock - (cube_bounced[c] or -9) > 0.5 then
                cube_bounced[c] = clock
                core.mob_velocity(c, 0, 11, 0)
            end
        end
    end

    -- ------------------------------------------------------------ panels

    -- A push button pressed at (x, y, z): the panels it flips, flipped.
    -- True when it was one of theirs.
    function D.pushed(x: number, y: number, z: number): boolean
        local any = false
        for _, f in flips do
            if f.push[1] == x and f.push[2] == y and f.push[3] == z then
                any = true
                if clock - f.up < 0.8 then continue end
                f.up = clock
                local function on(px: number, py: number, pz: number): boolean
                    return px >= f.x - 0.2 and px <= f.x + 2.2 and pz >= f.z - 0.2 and pz <= f.z + 2.2
                        and py >= f.y - 0.2 and py <= f.y + 0.8
                end
                for _, p in core.players() do
                    local px, py, pz = core.player_pos(p)
                    if px ~= nil and on(px, py :: number, pz :: number) then
                        core.player_launch(p, f.vx, f.vy, f.vz, 2.0)
                    end
                end
                for c in cubes do
                    local cx, cy, cz = core.mob_pos(c)
                    if cx ~= nil and on(cx, cy :: number, cz :: number) then
                        core.mob_velocity(c, f.vx, f.vy, f.vz)
                    end
                end
                tell(0, string.format("flip %d %d %d %.2f %.2f", f.x, f.y, f.z, f.vx, f.vz))
            end
        end
        return any
    end

    -- -------------------------------------------------------------- tick

    local since, seen = 0.0, -1
    function D.step(dt: number)
        clock += dt
        since += dt
        -- Traced again a few times a second, and at once when a portal changed.
        if since >= 0.25 or version ~= seen then
            since, seen = 0, version
            for _, b in bridges do
                if near(b.x, b.y, b.z) then step_bridge(b) end
            end
            for _, f in funnels do
                if near(f.x, f.y, f.z) then step_funnel(f) end
            end
            for _, d in dispensers do
                if near(d.x, d.y, d.z) then step_dispenser(d) end
            end
            save_kept()
        end
        step_bodies(dt)
    end

    -- Everything drawn from what was said, said again: a player come in.
    function D.told()
        for _, b in bridges do b.sig = "" end
        for _, f in funnels do f.sig = "" end
        for _, d in dispensers do d.sig = "" end
        since = 1
    end

    return D
end

-- =================================================================== client

function M.client()
    local C = {}
    local now = 0.0
    -- By id: each beam's pieces, x y z dx dy dz n.
    local funnels: { [string]: { number } } = {}
    local bridges: { [string]: { number } } = {}
    -- By id: a stream's arcs (x y z vx vy vz seconds each), its kind and
    -- how long a drop takes down the whole of it.
    type Stream = { arcs: { number }, kind: string, len: number }
    local streams: { [string]: Stream } = {}
    type Swing = { x: number, y: number, z: number, vx: number, vz: number, at: number }
    local swings: { Swing } = {}

    function C.heard(w: { string }): boolean
        local what = w[1]
        if what == "funnel" or what == "bridge" then
            local v = {}
            for i = 3, #w do v[i - 2] = tonumber(w[i]) or 0 end
            if what == "funnel" then funnels[w[2]] = v else bridges[w[2]] = v end
            return true
        elseif what == "stream" then
            local arcs = {}
            local len = 0.0
            for i = 4, #w do
                local v = tonumber(w[i]) or 0
                table.insert(arcs, v)
                if (i - 3) % 7 == 0 then len += v end
            end
            streams[w[2]] = { arcs = arcs, kind = w[3], len = len }
            return true
        elseif what == "flip" then
            table.insert(swings, { x = tonumber(w[2]) or 0, y = tonumber(w[3]) or 0, z = tonumber(w[4]) or 0,
                                   vx = tonumber(w[5]) or 0, vz = tonumber(w[6]) or 0, at = now })
            return true
        end
        return false
    end

    -- Where a drop is `t` seconds after it left the dispenser.
    local function along(st: Stream, t: number): (number?, number, number)
        local a = st.arcs
        for i = 1, #a - 6, 7 do
            if t <= a[i + 6] then
                return a[i] + a[i + 3] * t, a[i + 1] + a[i + 4] * t - GRAVITY * 0.5 * t * t, a[i + 2] + a[i + 5] * t
            end
            t -= a[i + 6]
        end
        return nil, 0, 0
    end

    core.render.pass("chambers_game:devices", 100, function()
        -- A funnel: a blue tube of light rings drifting along it, and its edge.
        for _, v in funnels do
            for i = 1, #v - 6, 7 do
                local x, y, z, dx, dy, dz, n = v[i] + 0.5, v[i + 1] + 0.5, v[i + 2] + 0.5, v[i + 3], v[i + 4],
                    v[i + 5], v[i + 6]
                local ex, ey, ez = x + dx * (n - 1), y + dy * (n - 1), z + dz * (n - 1)
                -- Two axes across it, for the tube's sides.
                local ux, uy, uz = if dy ~= 0 then 1 else 0, if dy ~= 0 then 0 else 1, 0
                local wx, wy, wz = dy * uz - dz * uy, dz * ux - dx * uz, dx * uy - dy * ux
                for k = 0, 3 do
                    local a = k * math.pi / 2 + now * 0.8
                    local ox = (ux * math.cos(a) + wx * math.sin(a)) * 0.7
                    local oy = (uy * math.cos(a) + wy * math.sin(a)) * 0.7
                    local oz = (uz * math.cos(a) + wz * math.sin(a)) * 0.7
                    core.render.fx("bolt", x - dx * 0.5 + ox, y - dy * 0.5 + oy, z - dz * 0.5 + oz, 0.03, 90, 170, 255,
                                   90, (now * 0.5 + k * 0.25) % 1, ex + dx * 0.5 + ox, ey + dy * 0.5 + oy,
                                   ez + dz * 0.5 + oz)
                end
                local step = 0.9
                local off = (now * FUNNEL_SPEED) % step
                local d = off - 0.5
                while d < n - 0.5 do
                    core.render.fx("glow", x + dx * d, y + dy * d, z + dz * d, 0.35, 90, 170, 255, 60, 0.4)
                    d += step
                end
            end
        end
        -- A bridge: its blocks are the floor; a glow along its two edges.
        for _, v in bridges do
            for i = 1, #v - 6, 7 do
                local x, y, z, dx, dz, n = v[i], v[i + 1], v[i + 2], v[i + 3], v[i + 5], v[i + 6]
                local cx, cz = x + 0.5 - dx * 0.5, z + 0.5 - dz * 0.5
                local ex, ez = cx + dx * n, cz + dz * n
                local sx, sz = dz * 0.48, dx * 0.48
                for s = -1, 1, 2 do
                    core.render.fx("bolt", cx + sx * s, y + 0.07, cz + sz * s, 0.03, 150, 220, 255, 120,
                                   (now * 0.7) % 1, ex + sx * s, y + 0.07, ez + sz * s)
                end
            end
        end
        -- Gel: a drop every 0.04 s of the way, each where it has got to.
        for _, st in streams do
            local blue = st.kind ~= "speed"
            local r, g, b = if blue then 40 else 255, if blue then 120 else 130, if blue then 255 else 20
            local t = now % 0.04
            local n = 0
            while t < st.len and n < 120 do
                local x, y, z = along(st, t)
                if x ~= nil then
                    core.render.box(x, y, z, 0.13, 0.17, 0.13, r, g, b, 255)
                end
                t += 0.04
                n += 1
            end
        end
        -- A panel swinging up and back on its hinge, the side away from where
        -- it throws: up in 0.15 s, down in 0.5.
        local left = {}
        for _, sw in swings do
            local t = now - sw.at
            if t > 0.65 then continue end
            table.insert(left, sw)
            local a = if t < 0.15 then t / 0.15 else 1 - (t - 0.15) / 0.5
            a *= math.rad(55)
            local l = math.sqrt(sw.vx * sw.vx + sw.vz * sw.vz)
            local fx, fz = if l > 0 then sw.vx / l else 1, if l > 0 then sw.vz / l else 0
            -- The hinge: the panel's edge behind it, as it throws.
            local mx, mz = sw.x + 1, sw.z + 1
            local hx, hz = mx - fx, mz - fz
            for k = 0, 9 do
                local d = (k + 0.5) / 10 * 2
                local px, pz = hx + fx * d * math.cos(a), hz + fz * d * math.cos(a)
                local py = sw.y + d * math.sin(a)
                core.render.box(px, py, pz, if fx ~= 0 then 0.22 else 2, 0.12, if fz ~= 0 then 0.22 else 2,
                                226, 228, 230, 255)
            end
        end
        swings = left
    end)

    function C.step(dt: number)
        now += dt
    end
    return C
end

return M
