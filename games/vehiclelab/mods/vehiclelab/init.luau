--!strict
-- Vehicles through portals (tools/vehiclelab.sh), on a platform high over the
-- sandbox so nothing of its own is in the way.
--
--   RAIL  a line of rails running north (-z) into an opening standing free
--         over the rail (A), and out of one a quarter turn round (B) onto
--         rails running east (+x). A cart ridden in must come out on B's
--         rails, still riding, the way they run, at the speed it went in.
--   BOAT  a channel of water running north into an opening over it (C), and
--         out of one facing south (D) over a channel running south. A boat
--         rowed in must come out afloat, still in the boat.
--   TRAIN a portal carried by a train: an opening (T) running at 4 m/s along
--         a rail line that bends a quarter turn, its front where it goes,
--         turning with the bend (core.portal_move with a front, every tick).
--         Stood on the line after the bend, a body is run into and taken
--         through, out of one standing still (E), moving away from it at the
--         train's speed. Then T starts again from the line's start.
--   CARRY /vlab carry: an opening (K) goes with you 2.5 ahead, its front the
--         way you go -- on a cart, in a boat, on foot -- onto one standing
--         still (KE) by the platform's west edge. You see its back and never
--         go in; whoever stands ahead of you is taken. /vlab carry again
--         leaves it where it is.
--
-- Everybody arrives at the rail line's start with a cart and a boat in the
-- bag; /vlab rail and /vlab boat go to each.

local X, Y, Z = 300, 150, 300   -- the platform's corner; bodies stand on Y + 1
local FLOOR = core.node_id("core:stone")
local RAIL = core.node_id("building:rail")
local WATER = core.node_id("lakes:water")

local function fill(x0: number, x1: number, y0: number, y1: number, z0: number, z1: number, b: number)
    for x = x0, x1 do
        for y = y0, y1 do
            for z = z0, z1 do core.set_block(x, y, z, b) end
        end
    end
end

-- facing 0 runs north (-z), 3 east (+x): mods/carts' RUN.
local function rails(x0: number, x1: number, z0: number, z1: number, facing: number)
    for x = x0, x1 do
        for z = z0, z1 do
            core.set_block(x, Y + 1, z, RAIL)
            core.set_facing(x, Y + 1, z, facing)
        end
    end
end

core.event.on("core:world_ready", function()
    fill(X, X + 40, Y, Y, Z, Z + 40, FLOOR)
    fill(X, X + 40, Y + 1, Y + 8, Z, Z + 40, 0)
    rails(X + 5, X + 5, Z + 9, Z + 34, 0)     -- north, through A at z + 9
    rails(X + 25, X + 34, Z + 20, Z + 20, 3)  -- east, out of B at x + 25
    fill(X + 37, X + 37, Y + 1, Y + 2, Z + 19, Z + 21, FLOOR)   -- the line's end
    -- BOAT: water sunk into the floor, x + 14..16 north from z + 34 to C at
    -- z + 9, and x + 29..31 south from D at z + 26 to z + 38.
    fill(X + 13, X + 17, Y - 1, Y - 1, Z + 8, Z + 36, FLOOR)
    fill(X + 14, X + 16, Y, Y, Z + 9, Z + 35, WATER)
    fill(X + 28, X + 32, Y - 1, Y - 1, Z + 25, Z + 40, FLOOR)
    fill(X + 29, X + 31, Y, Y, Z + 26, Z + 39, WATER)
    -- TRAIN: north of the rest, z - 20..-1: east along z - 15 from x + 5,
    -- a bend round (x + 23, z - 13), south along x + 25 to z - 3.
    fill(X, X + 40, Y, Y, Z - 20, Z - 1, FLOOR)
    fill(X, X + 40, Y + 1, Y + 8, Z - 20, Z - 1, 0)
    rails(X + 5, X + 23, Z - 15, Z - 15, 3)
    rails(X + 24, X + 24, Z - 15, Z - 14, 3)
    rails(X + 25, X + 25, Z - 14, Z - 3, 2)
end)

local opened = false
local train = -1   -- T's id
local carried = -1 -- K's id
local carrier: number? = nil   -- who K goes with (/vlab carry)
local heading = { 0.0, -1.0 }  -- which way they last went, flat
local run = 0.0    -- how far along the line T is, in nodes

-- Where along the line T is `d` nodes from its start, and which way it goes.
local L1, R, L2 = 18.0, 2.0, 10.0
local function along(d: number): (number, number, number, number)
    local x0, z0 = X + 5.5, Z - 14.5
    if d <= L1 then
        return x0 + d, z0, 1, 0
    end
    d -= L1
    local arc = R * math.pi / 2
    if d <= arc then
        local a = d / R   -- 0..pi/2 round the bend's centre
        return x0 + L1 + R * math.sin(a), z0 + R - R * math.cos(a), math.cos(a), math.sin(a)
    end
    d -= arc
    return x0 + L1 + R, z0 + R + d, 0, 1
end
local LINE = L1 + R * math.pi / 2 + L2
local function open_all(): boolean
    local a = core.portal_open(X + 5.5, Y + 2.0, Z + 9.5, 0, 1, 0, 0.5, 1.0)
    local b = core.portal_open(X + 25.5, Y + 2.0, Z + 20.5, 1, 1, 0, 0.5, 1.0)
    local c = core.portal_open(X + 15.5, Y + 1.5, Z + 9.5, 0, 1, 0, 1.5, 1.0)
    local d = core.portal_open(X + 30.5, Y + 1.5, Z + 25.5, 0, 1, 0, 1.5, 1.0)
    local t = core.portal_open(X + 5.5, Y + 2.0, Z - 14.5, 1, 1, 0, 0.5, 1.0)
    local e = core.portal_open(X + 10.5, Y + 2.0, Z - 6.5, 1, 1, 0, 0.5, 1.0)
    if a < 0 or b < 0 or c < 0 or d < 0 or t < 0 or e < 0 then return false end
    core.portal_link(t, e)
    train = t
    local k = core.portal_open(X + 1.5, Y + 2.0, Z + 2.5, 1, -1, 0, 0.5, 1.0)
    local ke = core.portal_open(X + 1.5, Y + 2.0, Z + 6.5, 1, 1, 0, 0.5, 1.0)
    if k < 0 or ke < 0 then return false end
    core.portal_link(k, ke)
    carried = k
    core.portal_link(a, b)
    core.portal_link(c, d)
    core.log(`vehiclelab: openings A {a} B {b} C {c} D {d}`)
    return true
end

local STATIONS = {
    rail = { X + 5.5, Y + 1.5, Z + 36.5, 0, -0.1, -1 },
    boat = { X + 15.5, Y + 1.5, Z + 37.5, 0, -0.3, -1 },
    train = { X + 25.5, Y + 1.5, Z - 4.5, 0, -0.1, -1 },
    watch = { X + 33.5, Y + 1.5, Z - 6.5, -1, -0.15, -0.5 },
}
local start = "rail"
local function put(p: number, which: string?)
    local s = STATIONS[which or start]
    core.player_place(p, s[1], s[2], s[3], s[4], s[5], s[6])
    core.player_spawn(p, s[1], s[2], s[3])
end

core.command("vlab", "rail|boat|train|watch|carry -- go to a station of the vehicle lab")
core.event.on("core:command", function(ev)
    if ev.name == "vlab" and ev.args == "carry" then
        carrier = if carrier == ev.player then nil else ev.player
        core.notice(ev.player, if carrier then "CARRY: K goes with you" else "CARRY: K stays where it is")
    elseif ev.name == "vlab" then
        put(ev.player, if STATIONS[ev.args] then ev.args else "rail")
    end
end)
-- A timed run types no commands: a player named after a station starts
-- there (NAME=boat tools/vehiclelab.sh).
local function first(p: number): string
    local n = core.player_name(p)
    if n == "carry" then carrier = p end   -- the rail line, with K
    return if STATIONS[n] then n else "rail"
end

local seen: { [number]: number } = {}
local was: { [number]: { number } } = {}
core.event.on("core:tick", function(ev)
    if not opened then opened = open_all() end
    if train >= 0 then
        run += 4 * (ev.dt or 0.05)
        if run > LINE then run = 0 end
        local x, z, hx, hz = along(run)
        core.portal_move(train, x, Y + 2.0, z, -1, 0, -1, hx, 0, hz, 0, 1, 0)
    end
    if carrier and carried >= 0 then
        local x, y, z = core.player_pos(carrier)
        local w = was[carrier]
        if x and w then
            local dx, dz = x - w[1], z - w[3]
            local d = math.sqrt(dx * dx + dz * dz)
            -- The way they go, once they go: standing, it stays as it was.
            if d > 0.02 then heading = { dx / d, dz / d } end
            core.portal_move(carried, x + heading[1] * 2.5, y + 1.0, z + heading[2] * 2.5,
                             -1, 0, -1, heading[1], 0, heading[2], 0, 1, 0)
        end
    end
    for _, p in core.players() do
        local t = (seen[p] or 0) + (ev.dt or 0.05)
        if seen[p] == nil then
            start = first(p)
            put(p)
            core.give_item(p, if start == "boat" then "boats:boat" else "carts:cart", 1)
            core.give_item(p, if start == "boat" then "carts:cart" else "boats:boat", 1)
            core.notice(p, if start == "boat" then "BOAT: right click the water with the boat, and row north into C"
                else "RAIL: right click the rail ahead with the cart, and ride north into A")
        elseif t < 8 then
            put(p)
        end
        seen[p] = t
        -- Where each body is, once a second: the log is the check.
        local x, y, z = core.player_pos(p)
        if x == nil then continue end
        local w = was[p]
        if w == nil or math.floor(t) ~= math.floor(t - (ev.dt or 0.05)) then
            core.log(string.format("vehiclelab: at %.2f %.2f %.2f", x, y, z))
        end
        was[p] = { x, y, z }
    end
end)
