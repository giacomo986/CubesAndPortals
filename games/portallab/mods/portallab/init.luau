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
--   SLIDE  an opening standing free (S) that slides along x, six nodes
--          either way at up to 1.5 m/s (core.portal_move), onto one that stands
--          still (T). It must be drawn going smoothly; walked into, or
--          waited for, it takes you through, out of T at T's speed: none.
--   CHASE  an opening (U) going back and forth along its own front, four
--          nodes either way at up to 1.5 m/s, onto one standing still (V):
--          stood in its way, a body is run into and taken through -- the
--          door that follows you -- and comes out of V moving away from it
--          at the speed U ran into it.
--   SPIN   three openings standing free, each turning on its own (core.portal_move
--          with a front): Y about the vertical, X head over heels, Z rolling
--          about its own front; each onto one standing still beyond (Y2, X2,
--          Z2, facing the glass). Walked into at any angle, the picture must
--          be what comes out.
--   TINY   a door-sized opening (D1) onto one an eighth of a node high (G1)
--          on the floor among a few blocks, both scaling (core.portal_scale):
--          walked into D1, a body comes out of G1 a sixteenth of its size,
--          the blocks round it as tall as towers; walked back into G1, out
--          of D1 as it was.
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
    fill(58, 82, Y, Y, 58, 122, FLOOR)
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
local slide = -1      -- S's id, once open
local slide_t = 0.0   -- how long it has been sliding
local chase = -1      -- U's id
local spin: { number } = {}   -- Y's, X's and Z's ids
local still = false   -- /lab still: the moving ones stop
local function open_all(): boolean
    local w = core.portal_open(66.5, Y + 2.0, 56.5, 0, 1, 0, 0.5, 1.0)
    local b = core.portal_open(62.5, Y + 2.0, 68.5, 1, 1, 0, 0.5, 1.0)
    local a = core.portal_open(66.5, Y + 0.5, 63.0, 2, 1, 0, 0.5, 1.0)
    local f = core.portal_open(62.5, Y + 7.0, 66.5, 1, 1, 0, 0.5, 1.0)
    local c = core.portal_open(77.5, Y + 3.5, 64.0, 2, -1, 0, 0.5, 1.0)
    local d = core.portal_open(66.5, Y + 2.0, 78.5, 0, -1, 0, 0.5, 1.0)
    local pp = core.portal_open(60.5, Y + 0.5, 73.0, 2, 1, 0, 0.5, 1.0)
    local q = core.portal_open(65.0, Y + 0.5, 72.5, 2, 1, 1, 0.5, 1.0)
    local sl = core.portal_open(70.5, Y + 2.0, 86.5, 0, 1, 0, 0.5, 1.0)
    local t = core.portal_open(70.5, Y + 2.0, 82.5, 0, -1, 0, 0.5, 1.0)
    local u = core.portal_open(77.5, Y + 2.0, 86.0, 0, 1, 0, 0.5, 1.0)
    local vv = core.portal_open(80.5, Y + 2.0, 84.5, 1, -1, 0, 0.5, 1.0)
    if w < 0 or b < 0 or a < 0 or f < 0 or c < 0 or d < 0 or pp < 0 or q < 0 or sl < 0 or t < 0
        or u < 0 or vv < 0 then
        return false
    end
    core.portal_link(sl, t)
    core.portal_link(u, vv)
    slide = sl
    chase = u
    -- TINY: the garden round G1 at (74.5, z 114.5).
    fill(72, 72, Y + 1, Y + 1, 116, 116, RED)
    fill(77, 77, Y + 1, Y + 2, 113, 113, WALL)
    fill(75, 75, Y + 1, Y + 1, 118, 118, GLASS)
    local d1 = core.portal_open(64.5, Y + 2.0, 110.5, 0, 1, 0, 0.5, 1.0)
    local g1 = core.portal_open(74.5, Y + 1.0 + 1 / 16, 114.5, 0, 1, 0, 0.5 / 16, 1 / 16)
    if d1 < 0 or g1 < 0 then return false end
    core.portal_link(d1, g1)
    core.portal_scale(d1, true)
    core.portal_scale(g1, true)
    for k, x in { 62.5, 70.5, 78.5 } do
        local sp = core.portal_open(x, Y + 2.0, 98.5, 0, 1, 0, 0.5, 1.0)
        local fx = core.portal_open(x, Y + 2.0, 94.5, 0, -1, 0, 0.5, 1.0)
        if sp < 0 or fx < 0 then return false end
        core.portal_link(sp, fx)
        spin[k] = sp
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
    slide = { 70.5, Y + 1, 89.5, 0, -0.05, -1, "SLIDE: S slides along before you -- walk into it, or wait for it" },
    chase = { 77.5, Y + 1, 89.5, 0, -0.05, -1, "CHASE: stand still -- U comes for you" },
    spin = { 70.5, Y + 1, 103.5, 0, -0.05, -1, "SPIN: Y turns about the vertical, X head over heels, Z rolls -- walk into each" },
    spiny = { 62.5, Y + 1, 103.5, 0, -0.05, -1, "SPIN: Y, turning about the vertical" },
    spinz = { 78.5, Y + 1, 103.5, 0, -0.05, -1, "SPIN: Z, rolling about its own front" },
    tiny = { 64.5, Y + 1, 113.5, 0, -0.05, -1, "TINY: walk into D1 -- out of G1 a sixteenth of your size" },
    g1 = { 74.5, Y + 1, 114.8, 0, -0.05, -1, "TINY: before G1, as small as you are -- walk into it" },
}
local function put(p: number, which: string)
    local s = STATIONS[which]
    core.player_place(p, s[1], s[2], s[3], s[4], s[5], s[6])
    core.player_spawn(p, s[1], s[2], s[3])
    core.notice(p, s[7])
end

core.command("lab", "wall|glass|head|bounce|view|slide|chase|spin|spiny|spinz|tiny|g1|still|at <degrees>|scale <s> -- go to a station of the portal lab")
core.event.on("core:command", function(ev)
    local k = if ev.name == "lab" then tonumber(string.match(ev.args, "^scale ([%d%.]+)$")) else nil
    if k then
        -- /lab scale <s>: this body that big (core.body_scale).
        core.notice(ev.player, if core.body_scale(ev.player, k) == 1 then `SCALE {k}` else "SCALE: refused")
        return
    end
    local deg = if ev.name == "lab" then tonumber(string.match(ev.args, "^at (%-?%d+)$")) else nil
    if deg then
        -- /lab at <degrees>: every turning one stopped at that angle.
        still = true
        slide_t = math.rad(deg) / 0.5
        core.notice(ev.player, `STILL at {deg} degrees`)
    elseif ev.name == "lab" and ev.args == "still" then
        still = not still
        core.notice(ev.player, if still then "STILL: the moving openings stop where they are" else "STILL: they move again")
    elseif ev.name == "lab" then
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
    if slide >= 0 then
        if not still then slide_t += ev.dt or 0.05 end
        core.portal_move(slide, 70.5 + 6 * math.sin(slide_t * 0.25), Y + 2.0, 86.5)
        core.portal_move(chase, 77.5, Y + 2.0, 86.0 + 4 * math.sin(slide_t * 0.375))
        local a = slide_t * 0.5
        local c, sn = math.cos(a), math.sin(a)
        if spin[3] then
            core.portal_move(spin[1], 62.5, Y + 2.0, 98.5, -1, 0, -1, sn, 0, c, 0, 1, 0)
            core.portal_move(spin[2], 70.5, Y + 2.0, 98.5, -1, 0, -1, 0, sn, c, 0, c, -sn)
            core.portal_move(spin[3], 78.5, Y + 2.0, 98.5, -1, 0, -1, 0, 0, 1, sn, c, 0)
        end
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
