--!strict
-- A scene for checking portals by eye (tools/portallab.sh), on a platform in
-- the sky so the sun lights everything. Two stations:
--
--   GLASS  a wall of glass nine high: plain glass on the left, the test
--          chambers' faint glass on the right, red behind both. Two ways to
--          see it through a portal: a wall opening (W) leads to one (B) that
--          faces it; a hole in the floor (A) leads to one high in the same
--          wall (F) that looks down at it. Either must show the same glass
--          as walking round to look at it (/lab view).
--   HEAD   a ceiling two high with an opening in it (C) to a wall opening
--          (D). A jump under C takes the head through it: the crossing goes
--          when the head reaches the plane, not the feet.
--   BOUNCE two holes in the floor leading to each other, a quarter turn
--          apart as a portal gun's often are: dropped above one, a body
--          falls through, flies up out of the other and falls back in, for
--          ever. Where it looks must stay where it looked, the horizon
--          turning back level.
--
-- Everybody arrives before W, looking into it. /lab wall, glass and head put
-- you at a station again; /lab view stands where B looks out from, to see
-- the glass without a portal.

local id = core.node_id
local FLOOR = core.register_node("portallab:floor", 200, 200, 205, true, 0, "", 0, 1.0, "stone")
local WALL = core.register_node("portallab:wall", 120, 125, 135, true, 0, "", 0, 1.0, "stone")
local RED = core.register_node("portallab:red", 200, 40, 40, true, 0, "", 0, 1.0, "stone")
local FAINT = core.register_node("portallab:faint_glass", 190, 220, 235, true, 0, "", 0, 0.3, "glass")
core.node_glass(FAINT, 60)   -- as games/chambers' glass
local GLASS = id("core:glass")

local Y = 40   -- the platform's blocks; bodies stand on y 41

local function fill(x0: number, x1: number, y0: number, y1: number, z0: number, z1: number, b: number)
    for x = x0, x1 do
        for y = y0, y1 do
            for z = z0, z1 do core.set_block(x, y, z, b) end
        end
    end
end

local function build()
    fill(58, 82, Y, Y, 58, 80, FLOOR)
    -- GLASS: the glass wall at x 70, red behind it at x 74; a wall at x 62
    -- facing it with B low at z 68 and F high at z 66; W in a wall at z 56
    -- facing +z; A in the floor at x 66, z 62..63.
    fill(70, 70, Y + 1, Y + 9, 62, 67, GLASS)
    fill(70, 70, Y + 1, Y + 9, 68, 74, FAINT)
    fill(74, 74, Y + 1, Y + 10, 60, 76, RED)
    fill(62, 62, Y + 1, Y + 9, 64, 70, WALL)
    fill(62, 62, Y + 1, Y + 2, 68, 68, 0)
    fill(62, 62, Y + 6, Y + 7, 66, 66, 0)
    fill(64, 68, Y + 1, Y + 4, 56, 56, WALL)
    fill(66, 66, Y + 1, Y + 2, 56, 56, 0)
    fill(66, 66, Y, Y, 62, 63, 0)
    -- HEAD: a roof at y 43 over x 76..79, z 62..66 with C at x 77, z 63..64;
    -- D in a wall at z 78 facing -z.
    fill(76, 79, Y + 3, Y + 3, 62, 66, WALL)
    fill(77, 77, Y + 3, Y + 3, 63, 64, 0)
    fill(64, 68, Y + 1, Y + 4, 78, 78, WALL)
    fill(66, 66, Y + 1, Y + 2, 78, 78, 0)
    -- BOUNCE: P along z at x 60, z 72..73; Q along x at x 64..65, z 72.
    fill(60, 60, Y, Y, 72, 73, 0)
    fill(64, 65, Y, Y, 72, 72, 0)
end

core.event.on("core:world_ready", build)

-- The openings: a server's exist a few ticks after the first, so asked
-- again until they open.
local opened = false
local function open_all(): boolean
    local w = core.portal_open(66.5, Y + 2.0, 56.5, 0, 1, 0, 0.5, 1.0)
    local b = core.portal_open(62.5, Y + 2.0, 68.5, 1, 1, 0, 0.5, 1.0)
    local a = core.portal_open(66.5, Y + 0.5, 63.0, 2, 1, 0, 0.5, 1.0)
    local f = core.portal_open(62.5, Y + 7.0, 66.5, 1, 1, 0, 0.5, 1.0)
    local c = core.portal_open(77.5, Y + 3.5, 64.0, 2, -1, 0, 0.5, 1.0)
    local d = core.portal_open(66.5, Y + 2.0, 78.5, 0, -1, 0, 0.5, 1.0)
    local pp = core.portal_open(60.5, Y + 0.5, 73.0, 2, 1, 0, 0.5, 1.0)
    local q = core.portal_open(65.0, Y + 0.5, 72.5, 2, 1, 1, 0.5, 1.0)
    if w < 0 or b < 0 or a < 0 or f < 0 or c < 0 or d < 0 or pp < 0 or q < 0 then
        return false
    end
    core.portal_link(w, b)
    core.portal_link(a, f)
    core.portal_link(c, d)
    core.portal_link(pp, q)
    core.log(`portallab: openings W {w} B {b} A {a} F {f} C {c} D {d} P {pp} Q {q}`)
    return true
end

local STATIONS = {
    wall = { 66.5, Y + 1, 59.5, 0, -0.05, -1, "WALL: the glass wall must be there, through the opening" },
    glass = { 66.5, Y + 1, 64.8, 0, -0.8, -0.6, "GLASS: look down into the hole -- the glass wall must be there" },
    head = { 77.5, Y + 1, 65.8, 0, 0.2, -1, "HEAD: step under the opening in the roof and jump" },
    bounce = { 60.5, Y + 7, 73.0, 0, -0.6, -0.8, "BOUNCE: falling into P, out of Q and back -- the look must stay yours" },
    view = { 63.6, Y + 1, 68.5, 1, -0.05, 0, "VIEW: what B sees, without a portal -- the glass wall ahead" },
}
local function put(p: number, which: string)
    local s = STATIONS[which]
    core.player_place(p, s[1], s[2], s[3], s[4], s[5], s[6])
    core.player_spawn(p, s[1], s[2], s[3])
    core.notice(p, s[7])
end

core.command("lab", "wall|glass|head|bounce|view -- go to a station of the portal lab")
core.event.on("core:command", function(ev)
    if ev.name == "lab" then
        put(ev.player, if STATIONS[ev.args] then ev.args else "wall")
    end
end)

-- Put at the first station, again for the first three seconds: a body
-- placed before its client has the world falls off where it was.
local seen: { [number]: number } = {}
core.event.on("core:tick", function(ev)
    if not opened then
        opened = open_all()
    end
    for _, p in core.players() do
        local t = (seen[p] or 0) + (ev.dt or 0.05)
        if t < 3 then
            local s = STATIONS.wall
            core.player_place(p, s[1], s[2], s[3], s[4], s[5], s[6])
        end
        if seen[p] == nil then
            put(p, "wall")
        end
        seen[p] = t
    end
end)
