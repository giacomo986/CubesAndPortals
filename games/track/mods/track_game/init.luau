-- track_game: a timed lap of track_core's loop. The start line and three
-- checkpoints -- painted by a stage here, so where they are is written once --
-- the server timing every player across them, and the HUD on the player's
-- machine.
--
-- The SERVER keeps the time. It reads each player's accepted position once a
-- step and puts the moment a line was crossed between two of them, so a lap is
-- timed to a fraction of a step and not to a whole one. The player's machine
-- counts up from "go" to show a running time, and shows the server's number
-- when a lap ends.
--
-- A player arrives ON FOOT and walks up to a kart: a right click on one
-- (track_core's block, drawn as the kart it is) puts them in it -- driving,
-- the size of a kart, drawn as one, and the camera behind it -- held for a
-- countdown, and let go at zero. F gets them out, and the
-- kart is parked beside them. The best laps are kept by name in the world
-- file: a player who leaves and comes back, or finds the server restarted,
-- finds their best again, and everybody sees the fastest few.
--
-- `side = both`, as yafps_game: the server runs the first half, a player's
-- machine the second, and a process that is its own server runs both.

local GROUND = 20

-- Across the road, in the order a lap meets them. A line stands at `at` on
-- `axis` and counts when crossed towards `dir`; `from`..`to` are the road's
-- cells along the other coordinate. The first is the start and the finish.
--
-- Tracks, each its own lines: the loop, the ring south of the paddock,
-- the grand circuit east of the loop and the loops track west of it (where
-- track_core puts them), and the Inferno track. A player
-- laps whichever track's start line they crossed last, and each track keeps its
-- own best laps and board -- the loop's under the keys it always had.
local TRACKS = {
    { key = "", gates = {
        { axis = "x", at = 40, dir = 1,  from = 17,  to = 23 },    -- south straight, running +x
        { axis = "z", at = 64, dir = 1,  from = 105, to = 111 },   -- east, +z
        { axis = "x", at = 64, dir = -1, from = 105, to = 111 },   -- north, -x
        { axis = "z", at = 64, dir = -1, from = 17,  to = 23 },    -- west, -z
    } },
    { key = "ring", gates = {
        { axis = "x", at = 113, dir = -1, from = -64, to = -57 },  -- north side, over its flat squares
        { axis = "z", at = -73, dir = -1, from = 96, to = 103 },   -- west, before the wall ride
        { axis = "x", at = 140, dir = 1,  from = -128, to = -121 }, -- south, after the jump
        { axis = "z", at = -92, dir = 1,  from = 160, to = 167 },  -- east, the banked side
    } },
    { key = "grand", gates = {
        { axis = "x", at = 160, dir = 1,  from = 24,  to = 39 },   -- the start straight
        { axis = "x", at = 280, dir = 1,  from = 80,  to = 95 },   -- after the first S
        { axis = "x", at = 240, dir = -1, from = 144, to = 159 },  -- after the jump
        { axis = "z", at = 104, dir = -1, from = 168, to = 183 },  -- before the second S
    } },
    -- Half in the Inferno, games/inferno's room (track_core, and inferno_track
    -- there): west from the paddock through frame A into it, two checkpoints
    -- there, and one back in the world. `paint`: its road has every line
    -- painted on it, the start and the marks. `inferno`: a gate in that room,
    -- which this mod times when it runs there.
    { key = "inferno", paint = true, gates = {
        { axis = "x", at = 55, dir = -1, from = -8, to = -1 },     -- west, towards frame A
        { axis = "z", at = -30, dir = -1, from = -217, to = -210, inferno = true }, -- south
        { axis = "z", at = -40, dir = 1,  from = -182, to = -175, inferno = true }, -- north to C
        { axis = "x", at = 20, dir = 1,  from = -32, to = -25 },   -- out of D, east
    } },
    -- West of the loop (track_core), reached from the Inferno track's road: a
    -- wide loop, down to one lane, a tall loop out to the left and one out to
    -- the right, then two low ones. Its checkpoints are the tops of the wide
    -- and the tall loops, crossed the way only a kart
    -- upside down over one goes: at the bottom of a loop its way back down is
    -- beside its way up, and a kart that went across there missed the loop.
    { key = "loops", gates = {
        { axis = "z", at = 40, dir = 1,  from = -8,   to = 7 },    -- the start straight, +z
        { axis = "z", at = 64, dir = -1, from = -24,  to = 7 },    -- over the wide loop
        { axis = "x", at = -76, dir = 1, from = 88,   to = 103 },  -- over the tall loop to the left
        { axis = "z", at = 76, dir = 1,  from = -104, to = -89 },  -- over the tall loop to the right
    } },
    -- A player's own: the start line and checkpoints put down with track_core's
    -- tools, wherever the pieces were built. Kept in the store; `key` gets
    -- the number of the start line it is, so a new one starts a new board.
    { key = "custom", custom = true, gates = {} },
}
local CUSTOM = #TRACKS

-- The trial for the Chiave della Corsa (mods/keys): three laps of the Inferno
-- track in a row, each faster than the engineer's. A lap over it starts the
-- count again. ponytail: a time, not a ghost kart to race; a ghost wants the
-- engineer's lap recorded and replayed in both rooms.
local ENGINEER = 32.0   -- scripted laps of it run 30-32 s (2026-10-09)
-- By name: a player's state here is made again each time a lap brings them
-- back from the Inferno.
local under: { [string]: number } = {}
-- Come by a survival site (mods/keys' keys:level), the time to beat is that
-- site's: 1.3 - 0.035 d of the engineer's (d 1 41 s, d 4 37 s, d 10 30 s).
local beat: { [string]: number } = {}
if core.side() ~= "client" then
    core.event.on("keys:level", function(ev)
        beat[core.player_name(ev.player)] = ENGINEER * (1.3 - 0.035 * ev.d)
    end)
end
function engineer_lap(p: number, s: any, lap: number)
    local target = beat[s.name] or ENGINEER
    if core.bag_count(p, "keys:pista") > 0 and beat[s.name] == nil then return end
    local n = if lap < target then (under[s.name] or 0) + 1 else 0
    under[s.name] = n
    if n >= 3 then
        under[s.name] = 0
        core.notice(p, "Tre giri sotto il tempo dell'ingegnere.")
        core.event.emit("keys:earned", { player = p, key = "pista" })
    else
        core.notice(p, string.format("Il tempo dell'ingegnere: %.1f s. Giri di fila sotto: %d di 3.", target, n))
    end
end

-- Which room this is: the track's, or the Inferno's, where this mod runs too
-- for the Inferno track's side of a lap. A gate is timed, and painted, in its
-- own room only.
local INFERNO = core.node_id("inferno:scoria") ~= 0
local function here(g): boolean
    return (g.inferno == true) == INFERNO
end

local GATES = TRACKS[1].gates
local CHECKPOINTS = #GATES - 1

-- How long a player is held after getting in, before they may drive, in
-- seconds.
local COUNTDOWN = 3

-- A kart: 24 m/s flat out after two seconds of throttle, 6 in reverse, from top
-- speed to a stop in under a second, a wheel that turns 2.6 rad/s, and a speed
-- that fades by half in a little over a second off the throttle. As wide and as
-- long as the mesh, so its sides meet a wall where the wall is. `sprint` is the
-- top speed and `walk` the top speed in reverse, which is how a body that
-- drives reads them.
--
-- Here and not in track_core because here is what puts a player in one: the
-- world is on foot (core.movement) and a body gets these one at a time
-- (core.body_drive). On foot again is FOOT, which is what track_core declared.
--
-- `rise` is how high over the road a kart may go: 12, a ceiling it bounces off
-- -- and over the top of a wide loop (track_core), 11.1 up with nothing under
-- it the server counts as standing.
-- The walls are not what keeps it on the track -- a jump held at speed puts a
-- kart on top of one -- leaving the road is: half a second off it and the kart
-- is put back on the nearest road (OFF_ROAD). Ten and not a jump's height, so
-- that something that throws a kart up can come later.
local KART = { walk = 6, sprint = 24, jump = 8, gravity = -25, rise = 12,
               accel = 12, brake = 30, turn = 2.6, drag = 0.6,
               width = 1.7, length = 2.4 }
local FOOT = { walk = 4.8, sprint = 8, fly = 14, jump = 8, gravity = -25, width = 0.6 }

-- How many best laps everybody is shown.
local BOARD = 5

-- How long a kart may be off the road, in seconds, before it is put back on it.
local OFF_ROAD = 0.5

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local clock = 0

local heard: (string) -> () = function(_) end

-- Asked for by the half on the player's machine, answered by the server's.
local get_out: (number) -> () = function(_) end
local respawn: (number) -> () = function(_) end
local use_item: (number, boolean) -> () = function(_, _) end

local function tell(player: number, text: string)
    if drawing then
        heard(text)
    else
        core.net.send(player, text)
    end
end

-- ===================================================================== server

if serving then
    core.net.open()

    core.register_mapgen("track_game:lines", function(_ctx)
        if INFERNO then
            return
        end
        local START = core.node_id("track_core:start")
        local MARK  = core.node_id("track_core:mark")
        local WALL  = core.node_id("track_core:wall")
        local LIGHT = core.node_id("track_core:light")
        if START == 0 or MARK == 0 then
            core.log("track_game: track_core is not loaded, so there is no road to paint")
            return
        end
        for i, g in GATES do
            for w = g.from - 1, g.to + 1 do
                local x = if g.axis == "x" then g.at else w
                local z = if g.axis == "x" then w else g.at
                if w < g.from or w > g.to then
                    for y = GROUND, GROUND + 3 do   -- a post in each wall
                        core.set_block(x, y, z, WALL)
                    end
                else
                    core.set_block(x, GROUND - 1, z, if i == 1 then START else MARK)
                end
                core.set_block(x, GROUND + 4, z, LIGHT)   -- and a lit beam across
            end
        end
    end)

    -- Where along a step from (x0, z0) to (x1, z1) a gate was crossed the way
    -- it counts, as a fraction of the step, or nil.
    local function crossed(g, x0: number, z0: number, x1: number, z1: number): number?
        local a0 = if g.axis == "x" then x0 else z0
        local a1 = if g.axis == "x" then x1 else z1
        local line = g.at + 0.5
        if (a0 - line) * g.dir >= 0 or (a1 - line) * g.dir < 0 then
            return nil
        end
        local f = (line - a0) / (a1 - a0)
        local w = if g.axis == "x" then z0 + (z1 - z0) * f else x0 + (x1 - x0) * f
        if w < g.from or w > g.to + 1 then
            return nil
        end
        return f
    end

    local KART_NODE  = core.node_id("track_core:kart")
    local KART_MODEL = core.model_id("track_core:kart")
    local ROBOT_MODEL = core.model_id("track_core:robot")

    -- Held and let go by the KART's numbers: a hold says how fast a body may
    -- go and not what it is, so a body that drives is still driving at zero.
    --
    -- The jump is NOT zeroed, and it is what a held body needs: getting in
    -- puts the body on four sprung wheels and the chassis settles 1.07 higher
    -- than the feet that walked up to it, which a server that allows no rise at
    -- all refuses every claim (8 corrections in the first second, reported by
    -- the log). It is also what the ceiling a kart bounces off is measured by:
    -- a kart jumps on the spot while it is held, as it does on the road.
    local function hold(p: number)
        core.body_movement(p, 0, 0, 0, KART.jump, KART.gravity, KART.rise)
    end
    local function release(p: number)
        core.body_movement(p, KART.walk, KART.sprint, 0, KART.jump, KART.gravity, KART.rise)
    end

    -- What a kart counts as the track under it: the road and its painted
    -- lines, a parked kart, which is standing on the road, and the track
    -- pieces with the cells under them (track_core's core.node_piece).
    local ROAD = {}
    local PIECE = {}   -- a track piece's blocks, road too
    -- The paddock and its ways to the tracks are ground and not road, and
    -- allowed on a lap all the same: a track may run through them.
    for _, name in { "track_core:road", "track_core:start", "track_core:mark", "track_core:kart",
                     "track_core:inferno_road", "track_core:paddock" } do
        local id = core.node_id(name)
        if id ~= 0 then
            ROAD[id] = true
        end
    end
    -- And every other block of track_core's but its base, walls and lights:
    -- the track pieces and the cells under them, whatever pieces it grows.
    local NOT_PIECE = { base = true, wall = true, light = true, road = true, start = true,
                        mark = true, kart = true, inferno_road = true, paddock = true, scoria = true,
                        lava = true, glowrock = true, obsidian = true }
    for id = 1, 4095 do
        local name = core.node_name(id)
        if name == "" then
            break
        end
        local short = string.match(name, "^track_core:(.+)$")
        if short and not NOT_PIECE[short] then
            ROAD[id] = true
            PIECE[id] = true
        end
    end

    -- Whether the first solid thing under the feet is road. Over a wall it is
    -- the wall, inside the loop the grass, and past the ground that is loaded,
    -- nothing -- all of them off the road. A jump over the road is still over it.
    -- Off-road areas (track_core's tool, below): a rectangle of the map where
    -- a kart on a lap may drive whatever it drives on -- grass, sand, a hill
    -- -- so a track may leave the road and come back to it. { x0, z0, x1, z1, y },
    -- y the height its corner posts stand at.
    local AREAS = {}

    local function over_road(x: number, y: number, z: number): boolean
        local bx, bz = math.floor(x), math.floor(z)
        for _, a in AREAS do
            if bx >= a.x0 and bx <= a.x1 and bz >= a.z0 and bz <= a.z1 then
                return true
            end
        end
        for by = math.floor(y - 0.01), math.floor(y) - KART.rise - 2, -1 do
            local id = core.get_block(bx, by, bz)
            if id ~= 0 then
                if ROAD[id] == true then
                    return true
                end
                break
            end
        end
        -- On a wall ride the road is beside the kart and not under it: the
        -- track counts as under it when a track PIECE is a node or two away
        -- at the kart's own height or under it -- a piece, so that
        -- the top of the loop's walls, beside its road, is still off it, and
        -- not over it, so that a kart fallen under a piece's road is. Down as
        -- far as a kart may be over the road: up a loop's far side the road
        -- under it is a few nodes back.
        for dx = -2, 2 do
            for dz = -2, 2 do
                for by = math.floor(y), math.floor(y) - KART.rise - 2, -1 do
                    if PIECE[core.get_block(bx + dx, by, bz + dz)] then
                        return true
                    end
                end
            end
        end
        return false
    end

    -- The nearest place on the road a kart fits, ring by ring out from (x, z):
    -- a road cell with road round it and air over all nine, so a kart put there
    -- has no wall in it -- the loop's road, or a track piece's flat road, whose
    -- cells are about as full as a road's base (a quarter, just under) with
    -- air over them. How
    -- high to put it, or nil. The middle of that cell, or nil past 48 nodes.
    local LOOP_ROAD = {}
    for _, name in { "track_core:road", "track_core:start", "track_core:mark",
                     "track_core:inferno_road" } do
        LOOP_ROAD[core.node_id(name)] = true
    end
    local FILL = core.node_id("track_core:piece_fill")
    -- The loop's road at GROUND, or flat piece road whose cells are at `gy`.
    local function fits(cx: number, cz: number, gy: number): number?
        local loop, flat = gy == GROUND, true
        for dx = -1, 1 do
            for dz = -1, 1 do
                local x, z = cx + dx, cz + dz
                local at = core.get_block(x, gy, z)
                loop = loop and LOOP_ROAD[core.get_block(x, gy - 1, z)] == true and at == 0
                local full = if at == FILL then core.get_facing(x, gy, z) else 0
                flat = flat and full >= 1 and full <= 9 and core.get_block(x, gy + 1, z) == 0
                if not loop and not flat then
                    return nil
                end
            end
        end
        return if loop then gy else gy + 0.3
    end
    -- Ring by ring out from the kart: the loop's road and pieces at GROUND,
    -- and within 24 nodes pieces at the kart's own height or up to eight
    -- under it -- a track built out in the country, on a hill, is put back on
    -- rather than left for the loop's road over there.
    local function road_near(x: number, y: number, z: number): (number?, number?, number?)
        local cx, cz, cy = math.floor(x), math.floor(z), math.floor(y)
        for r = 0, 48 do
            local bx, bz, by, bd = nil, nil, nil, math.huge
            for dx = -r, r do
                local step = if math.abs(dx) == r then 1 else 2 * r
                for dz = -r, r, math.max(step, 1) do
                    local tx, tz = cx + dx, cz + dz
                    local function try(gy: number)
                        local fy = fits(tx, tz, gy)
                        if fy ~= nil then
                            local d = (tx + 0.5 - x) ^ 2 + (tz + 0.5 - z) ^ 2 + (fy - y) ^ 2
                            if d < bd then
                                bx, bz, by, bd = tx + 0.5, tz + 0.5, fy, d
                            end
                        end
                    end
                    try(GROUND)
                    if r <= 24 then
                        for gy = cy + 2, cy - 8, -1 do
                            -- only a piece's cell is worth the nine reads
                            if gy ~= GROUND and core.get_block(tx, gy, tz) == FILL then
                                try(gy)
                            end
                        end
                    end
                end
            end
            if bx ~= nil then
                return bx, bz, by
            end
        end
        return nil, nil, nil
    end

    -- The kart a player made for themself (/garage): three colours -- the
    -- bodywork, the trim, the rims, each 0 for the kart's own or 1..16 a dye
    -- -- and a part of each kind, 0 for the kart's own. Kept by name in the
    -- world file ("kart:<name>"). Worn on the kart while they drive
    -- (core.player_wear): each part not the kart's own in slots 1..5, which
    -- the client draws in place of the kart's (track_core's
    -- kart_<part>_<variant>), and the kart itself in slot 6, which paints it.
    local PARTS = {
        { key = "wheel", label = "Wheels",    variants = { "slick", "offroad", "retro" } },
        { key = "nose",  label = "Nose",      variants = { "wedge", "ram", "shark" } },
        { key = "pods",  label = "Side pods", variants = { "rocket", "slab" } },
        { key = "pipes", label = "Exhaust",   variants = { "twin", "stack", "jet" } },
        { key = "wing",  label = "Wing",      variants = { "spoiler", "twin", "fin" } },
    }
    local PAINTS = { "Bodywork", "Trim", "Rims" }
    -- The sixteen dyes, in the engine's order (kDyeColours).
    local DYES = {
        { "white", 234, 236, 236 }, { "orange", 240, 118, 20 }, { "magenta", 190, 68, 180 },
        { "light blue", 58, 176, 218 }, { "yellow", 248, 198, 40 }, { "lime", 112, 186, 26 },
        { "pink", 238, 142, 172 }, { "gray", 62, 68, 72 }, { "light gray", 142, 142, 136 },
        { "cyan", 22, 138, 146 }, { "purple", 122, 42, 172 }, { "blue", 52, 56, 158 },
        { "brown", 114, 72, 40 }, { "green", 84, 110, 28 }, { "red", 160, 38, 34 }, { "black", 20, 20, 24 },
    }
    local garage = {}   -- name -> { paint = { 3 }, part = { 5 } }

    local function kart_of(name: string)
        local g = garage[name]
        if g == nil then
            local w = string.split(core.storage_get(`kart:{name}`), " ")
            g = { paint = {}, part = {} }
            for i = 1, 3 do
                g.paint[i] = math.clamp(tonumber(w[i]) or 0, 0, #DYES)
            end
            for i, part in PARTS do
                g.part[i] = math.clamp(tonumber(w[3 + i]) or 0, 0, #part.variants)
            end
            garage[name] = g
        end
        return g
    end

    -- A garage's kart as worn: its tint, and its parts' models (0 for the
    -- kart's own).
    local function kart_look(g): (number, { number })
        local parts = {}
        for i, part in PARTS do
            local v = g.part[i]
            parts[i] = if v > 0 then core.model_id(`track_core:kart_{part.key}_{part.variants[v]}`) else 0
        end
        return g.paint[1] + g.paint[2] * 32 + g.paint[3] * 1024, parts
    end

    -- Puts a driver's kart on them, or takes it off a body on foot: for
    -- everybody else here, and for their own machine ("dress", the half below).
    local function dress_kart(p: number, s)
        local tint, parts = kart_look(kart_of(s.name))
        local worn = {}
        for i, m in parts do
            worn[i] = if s.driving then m else 0
        end
        worn[6] = if s.driving and tint ~= 0 then KART_MODEL else 0
        for i, m in worn do
            core.player_wear(p, i, m, if m ~= 0 then tint else 0)
        end
        tell(p, `dress {tint} {table.concat(worn, " ")}`)
    end

    local function garage_panel(p: number): string
        return `track_game:garage{p}`
    end

    local function show_garage(p: number, s)
        local g = kart_of(s.name)
        local panel = garage_panel(p)
        if core.ui_define(panel, 13.4, 6.9) == 0 then
            return
        end
        core.ui_box(panel, 0.2, 0.2, 13.0, 0.7, 40, 44, 56)
        core.ui_label(panel, 0.4, 0.33, "garage -- your kart")
        core.ui_button(panel, "close", 10.8, 0.25, 2.3, 0.6, "close")
        -- A row: what it is, < its value >.
        local function row(y: number, id: string, label: string, value: string)
            core.ui_label(panel, 0.4, y + 0.1, label)
            core.ui_button(panel, `{id}-`, 3.0, y, 0.62, 0.62, "<")
            core.ui_label(panel, 4.5, y + 0.1, value)
            core.ui_button(panel, `{id}+`, 7.4, y, 0.62, 0.62, ">")
        end
        for i, label in PAINTS do
            local d = DYES[g.paint[i]]
            local y = 1.1 + (i - 1) * 0.72
            row(y, `c{i}`, label, if d ~= nil then d[1] else "as made")
            if d ~= nil then
                core.ui_box(panel, 3.75, y + 0.1, 0.5, 0.45, d[2], d[3], d[4])
            end
        end
        for i, part in PARTS do
            local v = g.part[i]
            row(3.46 + (i - 1) * 0.72, `p{i}`, part.label, if v > 0 then part.variants[v] else "standard")
        end
        -- And on the right, the kart as it is now, turning.
        local tint, parts = kart_look(g)
        local pieces = {}
        for _, m in parts do
            if m ~= 0 then
                table.insert(pieces, m)
            end
        end
        core.ui_model(panel, 8.2, 1.1, 5.0, 5.6, KART_MODEL, tint, table.concat(pieces, " "))
        core.ui_show(panel, true, p)
    end

    -- A parked kart, by the facing of its cell (0..3): which way its nose
    -- points, and the corner of the cell its middle is drawn at -- a cell draws
    -- a model turned about the cell's own middle, and the kart is authored
    -- about its own, so it stands on a corner. Facing 2 points at +z, the road.
    local NOSE   = { [0] = { 0, -1 }, { -1, 0 }, { 0, 1 }, { 1, 0 } }
    local CORNER = { [0] = { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 0 } }

    -- Getting in: the kart parked here IS this body from now on -- it drives,
    -- it is the size of a kart and everybody sees one -- and the block is gone,
    -- because there is one kart and a player is in it. The body goes where
    -- the kart was drawn and faces where it pointed: a player who walked up
    -- from its nose drove off into the bay's back wall before, and the kart
    -- jumped two nodes to where they had stood.
    local function mount(p: number, s, x: number, y: number, z: number)
        local f = core.get_facing(x, y, z) % 4
        core.set_block(x, y, z, 0)
        core.body_drive(p, KART.accel, KART.brake, KART.turn, KART.drag, KART.width, KART.length)
        core.body_model(p, KART_MODEL)
        core.player_place(p, x + CORNER[f][1], y, z + CORNER[f][2], NOSE[f][1], 0, NOSE[f][2])
        hold(p)
        s.driving, s.held = true, COUNTDOWN
        s.next, s.lapping, s.off, s.skip = 1, false, 0, 0
        s.speeds = `{KART.walk} {KART.sprint} {KART.accel}`   -- what release puts back at zero
        tell(p, "kart 1")
        dress_kart(p, s)
        tell(p, `count {COUNTDOWN}`)
    end

    -- Parks a kart two nodes to one side of a body: the side it is looking
    -- along last, because the first clear one wins and a player gets out where
    -- there is room. Pointing the way the body was heading, to a quarter turn,
    -- so getting back in does not turn anybody round. False when nothing
    -- around it is clear, which is a kart that does not come back -- said out
    -- loud in the log rather than left.
    local function park(x: number, y: number, z: number, lx: number, lz: number): boolean
        local n = math.sqrt(lx * lx + lz * lz)
        if n < 0.01 then
            lx, lz = 0, 1
        else
            lx, lz = lx / n, lz / n
        end
        local facing = if math.abs(lx) > math.abs(lz)
            then (if lx > 0 then 3 else 1)
            else (if lz > 0 then 2 else 0)
        local sides = { { -lz, lx }, { lz, -lx }, { -lx, -lz }, { lx, lz } }
        for _, d in sides do
            local cx = math.floor(x + d[1] * 2.5)
            local cz = math.floor(z + d[2] * 2.5)

            -- Down to what holds it up, three nodes at most. A body that
            -- drives stands on four springs, up to 1.07 above the road it is
            -- driving on, and a kart parked from there hangs in the air --
            -- seen in the bay, one node up, after a driver left the world.
            local cy = math.floor(y + 0.1)
            for _ = 1, 3 do
                if core.get_block(cx, cy - 1, cz) ~= 0 then
                    break
                end
                cy -= 1
            end

            if core.get_block(cx, cy, cz) == 0 and core.get_block(cx, cy + 1, cz) == 0 then
                core.set_block(cx, cy, cz, KART_NODE)
                core.set_facing(cx, cy, cz, facing)
                return true
            end
        end
        return false
    end

    -- And getting out: a body on foot again, the size of one and drawn as one,
    -- with its kart standing beside it for whoever clicks it next.
    local function dismount(p: number, s)
        local x, y, z = core.player_pos(p)
        local lx, _, lz = core.player_look(p)
        core.body_drive(p, 0, 0, 0, 0, FOOT.width, 0)
        core.body_movement(p, FOOT.walk, FOOT.sprint, FOOT.fly, FOOT.jump, FOOT.gravity)
        core.body_model(p, ROBOT_MODEL)
        s.driving, s.lapping, s.held = false, false, 0
        s.items, s.boost, s.star, s.spin, s.slow = {}, 0, 0, 0, 0
        tell(p, "items")
        tell(p, "fx clear")
        if x ~= nil and not park(x, y, z, lx or 0, lz or 0) then
            core.log(`track_game: nowhere to park {core.player_name(p)}'s kart`)
        end
        tell(p, "kart 0")
        dress_kart(p, s)
    end

    local players = {}   -- player id -> { x, y, z, track, next, lapping, started, name, held, driving, off, skip,
                         --   and the power-ups below: items, boost, star, spin, slow, speeds }
    local rows_dirty = true   -- the item boxes to be put down again: a track's lines changed
    local bests   = {}   -- track -> name -> best lap in seconds, of the board and of everyone seen
    for i = 1, #TRACKS do
        bests[i] = {}
    end

    -- ======================================================== a site's circuit
    --
    -- Come by a survival site (mods/keys' keys:level), in the track's room: a
    -- circuit of that site's own, laid by track_core in its slot's square
    -- (track_core:level, answered by track_core:laid with its lines), a kart
    -- on a pad beside its start, and behind the pad the way out, hidden.
    -- Three laps in a row each under the site's time -- its length over
    -- 9 + d metres a second -- are the level done.
    local LEVEL_AT: { [number]: number } = {}     -- slot -> its index in TRACKS
    local level_of: { [number]: { slot: number, d: number, under: number } } = {}
    local function arrive(p: number, slot: number)
        local tr = TRACKS[LEVEL_AT[slot]]
        local sx, sz = tr.sx, tr.sz
        local WALL_ID, BASE_ID = core.node_id("track_core:wall"), core.node_id("track_core:base")
        -- The pad: level, cleared, west of the start line.
        for x = sx - 16, sx - 1 do
            for z = sz - 2, sz + 9 do
                core.set_block(x, 19, z, BASE_ID)
                for y = 20, 25 do core.set_block(x, y, z, 0) end
            end
        end
        core.set_block(sx - 3, 20, sz + 3, core.node_id("track_core:kart"))
        -- The way out: a block of wall with a hole facing +x, behind two
        -- cells of it under a dark lamp (mods/keys).
        for x = sx - 14, sx - 11 do
            for z = sz + 1, sz + 5 do
                for y = 20, 23 do core.set_block(x, y, z, WALL_ID) end
            end
        end
        core.set_block(sx - 12, 20, sz + 3, 0)
        core.set_block(sx - 12, 21, sz + 3, 0)
        core.player_place(p, sx - 6.5, 20, sz + 3.5, 1, 0, 0)
        core.player_spawn(p, sx - 6.5, 20, sz + 3.5, true)
        core.event.emit("keys:start", { player = p, slot = slot, ex = sx - 12, ey = 20, ez = sz + 3 })
        local lv = level_of[p]
        core.notice(p, string.format("Il circuito di questo portale: tre giri di fila sotto %.1f s. Il kart e' qui accanto.",
                                     tr.length / (9 + (if lv then lv.d else 1))))
    end
    local waiting: { [number]: { number } } = {}   -- slot -> players waiting for its circuit
    core.event.on("keys:level", function(ev)
        if INFERNO then return end
        ev.cancel = true
        level_of[ev.player] = { slot = ev.slot, d = ev.d, under = 0 }
        if LEVEL_AT[ev.slot] then
            arrive(ev.player, ev.slot)
        else
            waiting[ev.slot] = waiting[ev.slot] or {}
            table.insert(waiting[ev.slot], ev.player)
            core.event.emit("track_core:level", { slot = ev.slot, d = ev.d, seed = ev.seed })
        end
    end)
    -- Only in the track's own room: the Inferno's has no track_core to answer.
    if not INFERNO then core.event.on("track_core:laid", function(ev)
        local gates = {}
        for axis, at, dir, from, to in string.gmatch(ev.gates, "(%a) (-?%d+) (-?%d+) (-?%d+) (-?%d+)") do
            table.insert(gates, { axis = axis, at = tonumber(at), dir = tonumber(dir), from = tonumber(from),
                                  to = tonumber(to) })
        end
        table.insert(TRACKS, { key = `level{ev.slot}`, gates = gates, level = ev.slot, length = ev.length,
                               sx = ev.x, sz = ev.z })
        LEVEL_AT[ev.slot] = #TRACKS
        bests[#TRACKS] = {}
        core.log(`track_game: slot {ev.slot}'s circuit, {ev.length} m, {#gates} lines`)
        for _, p in waiting[ev.slot] or {} do arrive(p, ev.slot) end
        waiting[ev.slot] = nil
    end) end
    local function level_lap(p: number, ti: number, lap: number)
        local tr, lv = TRACKS[ti], level_of[p]
        if lv == nil or lv.slot ~= tr.level then return end
        local target = tr.length / (9 + lv.d)
        lv.under = if lap < target then lv.under + 1 else 0
        if lv.under >= 3 then
            lv.under = 0
            core.notice(p, "Tre giri sotto il tempo. Dietro la piazzola qualcosa si e' aperto.")
            core.event.emit("keys:cleared", { player = p, slot = tr.level })
        else
            core.notice(p, string.format("Il tempo: %.1f s. Giri di fila sotto: %d di 3.", target, lv.under))
        end
    end

    -- Where a track keeps a thing in the store: the loop under the keys it
    -- always had ("board", "best:<name>"), the others under their own.
    local function stored(i: number, what: string): string
        local k = TRACKS[i].key
        return if k == "" then what else `{what}:{k}`
    end

    -- The fastest few of a track, as "name time name time ...". A name has no
    -- spaces in it (the handshake refuses one that does).
    local function fastest(i: number): string
        local rows = {}
        for name, t in bests[i] do
            table.insert(rows, { name = name, t = t })
        end
        table.sort(rows, function(a, b) return a.t < b.t end)
        local out = {}
        for i = 1, math.min(BOARD, #rows) do
            table.insert(out, string.format("%s %.3f", rows[i].name, rows[i].t))
        end
        return table.concat(out, " ")
    end

    local function board_text(i: number): string
        local f = fastest(i)
        return if f == "" then "board" else `board {f}`
    end

    -- Kept across a restart (core.storage_set): each player's best under
    -- "best:<name>", read when they arrive, and the board under "board", read
    -- once the world file has been. A key a name, so how many players are
    -- remembered is the store's 4 MiB, not what fits in one value.
    local function keep_best(i: number, name: string)
        core.storage_set(`{stored(i, "best")}:{name}`, string.format("%.3f", bests[i][name]))
        core.storage_set(stored(i, "board"), fastest(i))
    end
    core.event.on("core:world_ready", function()
        -- A player's own track, as it was left: which start line it is (its
        -- board's key) and its lines, "axis at dir from to y" each.
        TRACKS[CUSTOM].key = `custom{core.storage_get("custom_n")}`
        for _, line in string.split(core.storage_get("areas"), ";") do
            local w = string.split(line, " ")
            if #w == 5 then
                table.insert(AREAS, { x0 = tonumber(w[1]), z0 = tonumber(w[2]), x1 = tonumber(w[3]),
                                      z1 = tonumber(w[4]), y = tonumber(w[5]) })
            end
        end
        for _, line in string.split(core.storage_get("custom_gates"), ";") do
            local w = string.split(line, " ")
            if #w == 6 then
                table.insert(TRACKS[CUSTOM].gates, { axis = w[1], at = tonumber(w[2]), dir = tonumber(w[3]),
                                                     from = tonumber(w[4]), to = tonumber(w[5]),
                                                     y = tonumber(w[6]) })
            end
        end

        for i = 1, #TRACKS do
            local w = string.split(core.storage_get(stored(i, "board")), " ")
            for k = 1, #w - 1, 2 do
                local t = tonumber(w[k + 1])
                if t ~= nil then
                    bests[i][w[k]] = t
                end
            end
        end

        -- A start line on a track of pieces has nothing to paint: a post
        -- either side of the road and a lit beam over it. A painted track
        -- gets them at every line, and the line on its road.
        local WALL  = core.node_id("track_core:wall")
        local LIGHT = core.node_id("track_core:light")
        local START = core.node_id("track_core:start")
        local MARK  = core.node_id("track_core:mark")
        for i = 2, #TRACKS do
            if TRACKS[i].custom then
                continue
            end
            for k, g in TRACKS[i].gates do
                if k > 1 and not TRACKS[i].paint then
                    break
                end
                if not here(g) then
                    continue
                end
                for w = g.from - 1, g.to + 1 do
                    local x = if g.axis == "x" then g.at else w
                    local z = if g.axis == "x" then w else g.at
                    if w < g.from or w > g.to then
                        for y = GROUND, GROUND + 3 do
                            core.set_block(x, y, z, WALL)
                        end
                    end
                    if TRACKS[i].paint and w >= g.from and w <= g.to then
                        core.set_block(x, GROUND - 1, z, if k == 1 then START else MARK)
                    end
                    core.set_block(x, GROUND + 5, z, LIGHT)
                end
            end
        end
    end)

    -- What a player on track i is told of it: how many checkpoints, their best
    -- on it (-1 for none yet) and its board.
    local function tell_track(p: number, i: number, name: string)
        tell(p, `ready {#TRACKS[i].gates - 1}`)
        local b = bests[i][name]
        tell(p, if b ~= nil then string.format("best %.3f", b) else "best -1")
        tell(p, board_text(i))
    end

    -- Everybody lapping track i (or on none yet, for the loop's).
    local function tell_track_riders(i: number, text: string)
        for p, s in players do
            if (s.track or 1) == i then
                tell(p, text)
            end
        end
    end

    -- ------------------------------------------------ a player's own track
    --
    -- A right click on a road with track_core's start line tool puts a start
    -- line across it there, and with the checkpoint tool the next checkpoint:
    -- across the road as far as it goes each way (twelve at most), counted
    -- when crossed AWAY from whoever clicked, as a piece runs away from whoever
    -- put it down. A white beam over the start, a yellow one over each
    -- checkpoint. The same tool on its own line takes it away -- the start
    -- line, the whole track. One track of these on a server, kept in the store.
    local BEAM_START = core.node_id("track_core:start")
    local BEAM_MARK  = core.node_id("track_core:mark")

    -- Road in the column at (x, z), about as high as y: a bank's edges stand
    -- up to four over its middle.
    -- `strict`: the paddock's earth, which a lap may cross, is not road.
    local PADDOCK = core.node_id("track_core:paddock")
    local function road_at(x: number, y: number, z: number, strict: boolean?): boolean
        for by = y + 4, y - 4, -1 do
            local b = core.get_block(x, by, z)
            if ROAD[b] == true and not (strict and b == PADDOCK) then
                return true
            end
        end
        return false
    end

    local function beam(g, id: number, on: boolean)
        for w = g.from, g.to do
            local x = if g.axis == "x" then g.at else w
            local z = if g.axis == "x" then w else g.at
            local now = core.get_block(x, g.y + 5, z)
            if on and now == 0 then
                core.set_block(x, g.y + 5, z, id)
            elseif not on and now == id then
                core.set_block(x, g.y + 5, z, 0)
            end
        end
    end

    local function gate_from(p: number, x: number, y: number, z: number)
        local px, _, pz = core.player_pos(p)
        local dx, dz = x + 0.5 - (px or x), z + 0.5 - (pz or z)
        local g = if math.abs(dx) >= math.abs(dz)
            then { axis = "x", at = x, dir = if dx >= 0 then 1 else -1, from = z, to = z, y = y + 1 }
            else { axis = "z", at = z, dir = if dz >= 0 then 1 else -1, from = x, to = x, y = y + 1 }
        local function on(w: number): boolean
            return if g.axis == "x" then road_at(x, y, w, true) else road_at(w, y, z, true)
        end
        local w0 = g.from
        -- Across the road it is on, not the paddock's: on the paddock, which
        -- is all open ground, as wide as the loop's road.
        if not on(w0) then
            g.from, g.to = w0 - 3, w0 + 3
            return g
        end
        while g.from > w0 - 12 and on(g.from - 1) do
            g.from -= 1
        end
        while g.to < w0 + 12 and on(g.to + 1) do
            g.to += 1
        end
        return g
    end

    local function custom_changed()
        local tr  = TRACKS[CUSTOM]
        local out = {}
        for _, g in tr.gates do
            table.insert(out, `{g.axis} {g.at} {g.dir} {g.from} {g.to} {g.y}`)
        end
        core.storage_set("custom_gates", table.concat(out, ";"))
        rows_dirty = true
        -- Whoever was lapping it starts again from its start line.
        for q, s in players do
            if s.track == CUSTOM then
                s.next, s.lapping = 1, false
                if #tr.gates == 0 then
                    s.track = nil
                end
                tell_track(q, s.track or 1, s.name)
            end
        end
    end

    core.event.on("core:used", function(ev)
        local start = ev.held == "track_core:start_line"
        -- A kart is got into whatever is in hand.
        if (not start and ev.held ~= "track_core:checkpoint") or ev.id == KART_NODE then
            return
        end
        ev.cancel = true
        local tr = TRACKS[CUSTOM]

        for k, g in tr.gates do
            local a = if g.axis == "x" then ev.x else ev.z
            local w = if g.axis == "x" then ev.z else ev.x
            if (k == 1) == start and math.abs(a - g.at) <= 1 and w >= g.from and w <= g.to then
                if start then
                    for j, h in tr.gates do
                        beam(h, if j == 1 then BEAM_START else BEAM_MARK, false)
                    end
                    tr.gates = {}
                    core.chat(ev.player, "Start line taken away, and the track with it")
                else
                    beam(g, BEAM_MARK, false)
                    table.remove(tr.gates, k)
                    core.chat(ev.player, `Checkpoint {k - 1} taken away`)
                end
                custom_changed()
                return
            end
        end

        if not road_at(ev.x, ev.y, ev.z) then
            core.chat(ev.player, "A line goes across a road: click the road")
            return
        end
        if not start and #tr.gates == 0 then
            core.chat(ev.player, "Put the start line down first")
            return
        end
        local g = gate_from(ev.player, ev.x, ev.y, ev.z)
        if start then
            for j, h in tr.gates do
                beam(h, if j == 1 then BEAM_START else BEAM_MARK, false)
            end
            -- A new start line is a new track: a board of its own.
            local n = (tonumber(core.storage_get("custom_n")) or 0) + 1
            core.storage_set("custom_n", `{n}`)
            tr.key, tr.gates, bests[CUSTOM] = `custom{n}`, { g }, {}
            beam(g, BEAM_START, true)
            core.chat(ev.player, `Start line down, {g.to - g.from + 1} wide`)
        else
            table.insert(tr.gates, g)
            beam(g, BEAM_MARK, true)
            core.chat(ev.player, `Checkpoint {#tr.gates - 1} down, {g.to - g.from + 1} wide`)
        end
        custom_changed()
    end)

    -- The off-road tool: a right click on the ground is one corner of an area,
    -- the next the opposite one (at most 256 a side). A lit post, two high,
    -- stands at each of its four corners. A click inside an area, with no
    -- corner waiting, takes that area away.
    local AREA_POST = core.node_id("track_core:light")
    local MAX_AREA  = 256
    local corner    = {}   -- player -> { x, z } of the first corner

    local function posts(a, on: boolean)
        for _, c in { { a.x0, a.z0 }, { a.x1, a.z0 }, { a.x0, a.z1 }, { a.x1, a.z1 } } do
            for y = a.y, a.y + 1 do
                local now = core.get_block(c[1], y, c[2])
                if on and now == 0 then
                    core.set_block(c[1], y, c[2], AREA_POST)
                elseif not on and now == AREA_POST then
                    core.set_block(c[1], y, c[2], 0)
                end
            end
        end
    end

    local function save_areas()
        local out = {}
        for _, a in AREAS do
            table.insert(out, `{a.x0} {a.z0} {a.x1} {a.z1} {a.y}`)
        end
        core.storage_set("areas", table.concat(out, ";"))
    end

    core.event.on("core:used", function(ev)
        if ev.held ~= "track_core:offroad" or ev.id == KART_NODE then
            return
        end
        ev.cancel = true
        local c = corner[ev.player]
        if c == nil then
            for i, a in AREAS do
                if ev.x >= a.x0 and ev.x <= a.x1 and ev.z >= a.z0 and ev.z <= a.z1 then
                    posts(a, false)
                    table.remove(AREAS, i)
                    save_areas()
                    core.chat(ev.player, "Off-road area taken away")
                    return
                end
            end
            corner[ev.player] = { x = ev.x, z = ev.z }
            core.chat(ev.player, "First corner of an off-road area: now click the opposite one")
            return
        end
        corner[ev.player] = nil
        local a = { x0 = math.min(c.x, ev.x), z0 = math.min(c.z, ev.z),
                    x1 = math.max(c.x, ev.x), z1 = math.max(c.z, ev.z), y = ev.y + 1 }
        if a.x1 - a.x0 >= MAX_AREA or a.z1 - a.z0 >= MAX_AREA then
            core.chat(ev.player, `An off-road area is at most {MAX_AREA} a side: start again`)
            return
        end
        table.insert(AREAS, a)
        posts(a, true)
        save_areas()
        core.chat(ev.player, `Off-road area {a.x1 - a.x0 + 1} by {a.z1 - a.z0 + 1}: a kart on a lap may drive there`)
    end)

    -- A right click on a parked kart, which is how you get in. Claimed, or the
    -- same click would also build whatever is in hand on top of it.
    core.event.on("core:used", function(ev)
        local s = players[ev.player]
        if ev.id ~= KART_NODE or KART_NODE == 0 or s == nil or s.driving then
            return
        end
        ev.cancel = true
        mount(ev.player, s, ev.x, ev.y, ev.z)
    end)

    -- R: back to the last line crossed, the way stunt racers' respawn is. Two
    -- nodes past it, in the middle of the road, standing and facing the way
    -- the lap runs, with the clock still going; before the first lap has
    -- started, three nodes short of the start line, so it is still crossed.
    local function respawn_at(s): (number, number, number, number)
        local gates = TRACKS[s.track or 1].gates
        local g = gates[1]
        local past = -3
        if s.lapping then
            g = gates[(s.next - 2) % #gates + 1]
            past = 2
        end
        local mid  = (g.from + g.to + 1) / 2
        local line = g.at + 0.5 + g.dir * past
        if g.axis == "x" then
            return line, mid, g.dir, 0, g.y or GROUND
        end
        return mid, line, 0, g.dir, g.y or GROUND
    end

    respawn = function(p: number)
        local s = players[p]
        if s == nil or not s.driving or s.held > 0 then
            return
        end
        local x, z, lx, lz, y = respawn_at(s)
        local gates = TRACKS[s.track or 1].gates
        if s.lapping and not here(gates[(s.next - 2) % #gates + 1]) then
            -- The last line crossed is in the other room: the nearest road
            -- here, the way the kart is looking.
            local px, py, pz = core.player_pos(p)
            local rx, rz, ry = road_near(px or 0, py or GROUND, pz or 0)
            if rx == nil or rz == nil then
                return
            end
            local vx, _, vz = core.player_look(p)
            x, z, y, lx, lz = rx, rz, ry or GROUND, vx or 0, vz or 1
        end
        core.player_place(p, x, y, z, lx, 0, lz)
        -- Not a run across anything: the step it is judged by goes from where
        -- the kart was to where it is put, and the claims in flight still come
        -- from the old place for a moment.
        s.skip, s.off = 3, 0
        core.log(string.format("track_game: %s back to %.1f %.1f", s.name, x, z))
    end

    get_out = function(p: number)
        local s = players[p]
        if s ~= nil and s.driving and s.held <= 0 then
            dismount(p, s)
        end
    end

    -- ------------------------------------------------------------ power-ups
    --
    -- As kart racers have them: rows of item boxes across the road, one row
    -- a little past every line of every track, and a kart that drives through
    -- one gets a power-up at random, used with the left click. A box comes
    -- back a few seconds after it is taken. What is dropped and thrown is the
    -- mod's own entities, seen by everybody (core.ecs.visible) in the models
    -- of track_core. The server decides everything; the player's machine is
    -- told what they hold and what happened to them, and draws it.
    --
    -- Not in the Inferno's room: its world has none of the models, and its
    -- side of the Inferno track gets no boxes.
    local BOX_MODEL = core.model_id("track_core:item_box")
    local MODELS = { banana = core.model_id("track_core:banana"), shell = core.model_id("track_core:shell_green"),
                     homing = core.model_id("track_core:shell_red") }
    local ITEMS_HERE = BOX_MODEL ~= 0 and not INFERNO
    local BOX, THING = "track_game:box", "track_game:thing"
    core.ecs.component(BOX, { "row" })
    core.ecs.component(THING, { "t" })

    -- What a box gives, and how often: the more a power-up does, the rarer.
    -- ponytail: the same odds for everybody; kart racers give the last ones
    -- the strong ones, which wants a race order this game has no laps for.
    local ROLL = { { "boost", 20 }, { "boost3", 9 }, { "banana", 20 }, { "shell", 18 },
                   { "homing", 12 }, { "ink", 10 }, { "star", 6 }, { "bolt", 5 } }
    local USES = { boost3 = 3 }
    local HOLD = 2   -- power-ups a kart carries: the first is used, the second waits

    -- What a player holds, told whole: "items" and then "<what>:<uses>:<roulette
    -- seconds left>" for each, the one used next first.
    local function tell_items(p: number, s)
        local out = { "items" }
        for _, it in s.items do
            table.insert(out, string.format("%s:%d:%.2f", it.what, it.uses, math.max(0, it.ready - clock)))
        end
        tell(p, table.concat(out, " "))
    end

    local BOX_BACK   = 4       -- seconds before a taken box is there again
    local BOX_REACH  = 1.6     -- how near a kart's middle takes one
    local BOOST      = 1.2     -- seconds of a boost
    local STAR       = 7       -- seconds of a star
    local SPIN       = 0.9     -- seconds a hit kart spins on the spot
    local SLOW       = 4       -- seconds the lightning leaves the others slow
    local INK        = 4       -- seconds of ink on the others' screens
    local SHELL      = 30      -- m/s: faster than a kart flat out
    local SHELL_LIFE = 6
    local HIT        = 1.3     -- how near a kart's middle a banana or shell hits
    local BANANAS    = 24      -- the most on the road at once; the oldest goes

    local boxes  = {}   -- { x, z, gy, y, id, back } a box; y nil until its road is found, false for none
    local things = {}   -- entity id -> { kind, vx, vz, t, owner, target, bounces }
    local peels  = {}   -- the bananas' ids, oldest first

    -- How high the road is at (x, z), within three of `gy`: its top, or nil
    -- where there is something else there, or nothing yet.
    local FILL_ID = core.node_id("track_core:piece_fill")
    local function road_top(x: number, z: number, gy: number): number?
        for by = gy + 3, gy - 3, -1 do
            local b = core.get_block(x, by, z)
            if b ~= 0 then
                if ROAD[b] ~= true then
                    return nil
                end
                return if b == FILL_ID then by + core.get_facing(x, by, z) / 10
                       elseif PIECE[b] then by + 0.3 else by + 1
            end
        end
        return nil
    end
    -- What a thing on the road stands on: the first thing under it, three down
    -- at most, or nil in the air.
    local function ground(x: number, y: number, z: number): number?
        local bx, bz = math.floor(x), math.floor(z)
        for by = math.floor(y + 0.5), math.floor(y) - 3, -1 do
            local b = core.get_block(bx, by, bz)
            if b ~= 0 then
                return if b == FILL_ID then by + core.get_facing(bx, by, bz) / 10
                       elseif PIECE[b] then by + 0.3 else by + 1
            end
        end
        return nil
    end

    -- A row ten nodes past each line, as many boxes as fit three apart, two
    -- at least and five at most.
    local function lay_rows()
        rows_dirty = false
        for _, b in boxes do
            if b.id then core.ecs.kill(b.id) end
        end
        boxes = {}
        for _, tr in TRACKS do
            for _, g in tr.gates do
                if not here(g) then
                    continue
                end
                local wide = g.to - g.from + 1
                local n = math.clamp(wide // 3, 2, 5)
                local a = g.at + 0.5 + g.dir * 10
                for k = 0, n - 1 do
                    local w = g.from + wide * (k + 0.5) / n
                    local x, z = if g.axis == "x" then a else w, if g.axis == "x" then w else a
                    table.insert(boxes, { x = x, z = z, gy = (g.y or GROUND) - 1, back = 0 })
                end
            end
        end
        -- And the ones players put down with the item box tool (below).
        for _, line in string.split(core.storage_get("item_boxes"), ";") do
            local w = string.split(line, " ")
            if #w == 3 then
                table.insert(boxes, { x = tonumber(w[1]), y = tonumber(w[2]), z = tonumber(w[3]),
                                      back = 0, placed = true })
            end
        end
    end
    -- Laid again once the store has been read, with the boxes kept there.
    core.event.on("core:world_ready", function()
        rows_dirty = true
    end)

    -- The item box tool (track_core): a right click on a road puts an item
    -- box over the cell clicked, for a track of pieces somebody built; the
    -- same click on the road under one of these takes it away. The rows past
    -- the lines are there anyway. Kept in the store, "x y z" each.
    local function save_boxes()
        local out = {}
        for _, b in boxes do
            if b.placed then
                table.insert(out, string.format("%.1f %.2f %.1f", b.x, b.y, b.z))
            end
        end
        core.storage_set("item_boxes", table.concat(out, ";"))
    end
    core.event.on("core:used", function(ev)
        if ev.held ~= "track_core:box_tool" or ev.id == KART_NODE then
            return
        end
        ev.cancel = true
        if not ITEMS_HERE then
            core.chat(ev.player, "No item boxes in this world")
            return
        end
        local x, z = ev.x + 0.5, ev.z + 0.5
        for i, b in boxes do
            if b.placed and math.abs(b.x - x) < 1 and math.abs(b.z - z) < 1 and math.abs(b.y - ev.y) < 4 then
                if b.id then core.ecs.kill(b.id) end
                table.remove(boxes, i)
                save_boxes()
                core.chat(ev.player, "Item box taken away")
                return
            end
        end
        local y = road_top(ev.x, ev.z, ev.y)
        if y == nil then
            core.chat(ev.player, "An item box goes on a road: click the road")
            return
        end
        table.insert(boxes, { x = x, y = y, z = z, back = 0, placed = true })
        save_boxes()
        core.chat(ev.player, "Item box down")
    end)

    -- The boxes not down yet, once the ground under them is there: put down,
    -- or given up on where it is not road.
    local boxes_said = 0
    local function find_rows()
        local down = 0
        for _, b in boxes do
            if b.id then down += 1 end
        end
        if down ~= boxes_said then
            boxes_said = down
            core.log(`track_game: {down} of {#boxes} item boxes down`)
        end
        for _, b in boxes do
            if b.y == nil and core.loaded(math.floor(b.x), b.gy, math.floor(b.z)) == 1 then
                local top = road_top(math.floor(b.x), math.floor(b.z), b.gy)
                b.y = top or false
            end
            if b.y and b.id == nil then
                b.id = core.ecs.spawn(BOX, b.x, b.y, b.z)
                if b.id then
                    core.ecs.visible(b.id, true)
                    core.ecs.model(b.id, BOX_MODEL)
                end
            end
        end
    end

    -- How fast a kart may go now, from what is on it: put on the body only
    -- when it changes, since new numbers build the chassis again.
    local function speeds(p: number, s)
        if not s.driving or s.held > 0 then
            return
        end
        local walk, sprint, accel = KART.walk, KART.sprint, KART.accel
        if s.spin > 0 then
            walk, sprint = 0, 0
        else
            if s.star > 0 then sprint, accel = 30, 40 end
            if s.boost > 0 then sprint, accel = 34, 60 end
            if s.slow > 0 and s.star <= 0 then sprint = math.min(sprint, 14) end
        end
        local key = `{walk} {sprint} {accel}`
        if key == s.speeds then
            return
        end
        local was_accel = tonumber(string.split(s.speeds or "", " ")[3])
        s.speeds = key
        core.body_movement(p, walk, sprint, 0, KART.jump, KART.gravity, KART.rise)
        if was_accel ~= accel then
            core.body_drive(p, accel, KART.brake, KART.turn, KART.drag, KART.width, KART.length)
        end
    end

    -- Hit by a banana, a shell, a star or the lightning: the kart stops and
    -- spins round once where it is. A star is hit by nothing.
    local function hit(p: number, s, why: string)
        if s.star > 0 or s.spin > 0 or not s.driving or s.held > 0 then
            return
        end
        local lx, _, lz = core.player_look(p)
        s.spin, s.boost, s.yaw = SPIN, 0, math.atan2(lx or 0, lz or 1)
        tell(p, `fx spin {SPIN}`)
        core.log(`track_game: {s.name} hit by {why}`)
    end

    local function drivers(): { [number]: any }
        local out = {}
        for p, s in players do
            if s.driving and s.held <= 0 then
                out[p] = s
            end
        end
        return out
    end

    local function remove(id: number)
        things[id] = nil
        core.ecs.kill(id)
        for i, b in peels do
            if b == id then
                table.remove(peels, i)
                break
            end
        end
    end

    local function spawn_thing(kind: string, x: number, y: number, z: number, owner: number): number?
        local id = core.ecs.spawn(THING, x, y, z)
        if id == nil then
            return nil
        end
        core.ecs.visible(id, true)
        core.ecs.model(id, MODELS[kind])
        things[id] = { kind = kind, vx = 0, vz = 0, t = 0, owner = owner, bounces = 0 }
        return id
    end

    use_item = function(p: number, back: boolean)
        local s = players[p]
        local first = s and s.items[1]
        if not ITEMS_HERE or s == nil or first == nil or not s.driving or s.held > 0 or s.spin > 0
                or clock < first.ready then
            return
        end
        local item = first.what
        first.uses -= 1
        if first.uses <= 0 then
            table.remove(s.items, 1)
        end
        tell_items(p, s)

        local x, y, z = core.player_pos(p)
        local lx, _, lz = core.player_look(p)
        if x == nil or lx == nil then
            return
        end
        local n = math.max(math.sqrt(lx * lx + lz * lz), 0.01)
        lx, lz = lx / n, lz / n

        if item == "boost" or item == "boost3" then
            s.boost = BOOST
            tell(p, `fx boost {BOOST}`)
        elseif item == "star" then
            s.star = STAR
            tell(p, `fx star {STAR}`)
        elseif item == "bolt" or item == "ink" then
            for q, o in players do
                if q ~= p and o.driving then
                    if item == "bolt" then
                        if o.star <= 0 then
                            hit(q, o, `{s.name}'s lightning`)
                            o.slow = SLOW
                            tell(q, `fx slow {SLOW}`)
                        end
                    else
                        tell(q, `fx ink {INK}`)
                    end
                end
            end
            tell(0, `fx {item}go {s.name}`)
        elseif item == "banana" then
            local bx, bz = x - lx * 2.2, z - lz * 2.2
            local id = spawn_thing("banana", bx, ground(bx, y + 0.5, bz) or y, bz, p)
            if id then
                table.insert(peels, id)
                if #peels > BANANAS then
                    remove(peels[1])
                end
            end
        else
            -- A shell, ahead or (S held) behind. A red one picks the nearest
            -- kart ahead of the one that throws it, and steers for it.
            local d = if back then -1 else 1
            local sx, sz = x + lx * 2.0 * d, z + lz * 2.0 * d
            local id = spawn_thing(item, sx, ground(sx, y + 0.5, sz) or y, sz, p)
            if id then
                local t = things[id]
                t.vx, t.vz = lx * SHELL * d, lz * SHELL * d
                if item == "homing" and not back then
                    local best = 100 * 100
                    for q in drivers() do
                        local qx, _, qz = core.player_pos(q)
                        if q ~= p and qx ~= nil then
                            local dx, dz = qx - x, qz - z
                            local d2 = dx * dx + dz * dz
                            if d2 < best and dx * lx + dz * lz > 0 then
                                best, t.target = d2, q
                            end
                        end
                    end
                end
            end
        end
        core.log(`track_game: {s.name} used {item}`)
    end

    local function roll(): string
        local sum = 0
        for _, r in ROLL do sum += r[2] end
        local k = math.random() * sum
        for _, r in ROLL do
            k -= r[2]
            if k <= 0 then return r[1] end
        end
        return ROLL[1][1]
    end

    local find_every = 0
    local function items_step(dt: number)
        if not ITEMS_HERE then
            return
        end
        if rows_dirty then
            lay_rows()
        end
        find_every -= dt
        if find_every <= 0 then
            find_every = 1
            find_rows()
        end

        -- The boxes turn, and come back after they are taken.
        local yaw = clock * 1.5
        for _, b in boxes do
            if b.id then
                if b.back > 0 then
                    b.back -= dt
                    if b.back <= 0 then
                        core.ecs.set(b.id, "y", b.y)
                    end
                end
                core.ecs.set(b.id, "yaw", yaw)
            end
        end

        local on = drivers()
        for p, s in on do
            local x, y, z = core.player_pos(p)
            if x == nil then
                continue
            end
            -- What is on the kart runs out.
            s.boost = math.max(0, s.boost - dt)
            s.star  = math.max(0, s.star - dt)
            s.slow  = math.max(0, s.slow - dt)
            if s.spin > 0 then
                -- Round once on the spot, a quarter of a turn more each
                -- fraction of SPIN, and facing the way it was at the end.
                s.spin = math.max(0, s.spin - dt)
                local a = s.yaw + 2 * math.pi * (1 - s.spin / SPIN)
                core.player_place(p, x, y, z, math.sin(a), 0, math.cos(a))
                s.skip = math.max(s.skip, 2)
            end
            speeds(p, s)

            -- Through a box: a power-up, if the kart has none.
            for _, b in boxes do
                if b.id and b.back <= 0 and (b.x - x) ^ 2 + (b.z - z) ^ 2 < BOX_REACH ^ 2
                        and math.abs(b.y - y) < 2.5 then
                    b.back = BOX_BACK
                    core.ecs.set(b.id, "y", b.y - 40)   -- out of sight at once; hidden would take seconds
                    if #s.items < HOLD then
                        local what = roll()
                        -- Ready once the roulette has turned on the player's screen.
                        table.insert(s.items, { what = what, uses = USES[what] or 1, ready = clock + 1 })
                        tell_items(p, s)
                    end
                end
            end

            -- A star knocks whoever it touches.
            if s.star > 0 then
                for q, o in on do
                    local qx, qy, qz = core.player_pos(q)
                    if q ~= p and qx ~= nil and (qx - x) ^ 2 + (qz - z) ^ 2 < 2.2 ^ 2 and math.abs(qy - y) < 2 then
                        hit(q, o, `{s.name}'s star`)
                    end
                end
            end
        end

        -- Shells fly, and bananas wait. Either hits the first kart it meets,
        -- and a shell takes a banana with it.
        for id, t in things do
            local x, y, z = core.ecs.get(id, "x"), core.ecs.get(id, "y"), core.ecs.get(id, "z")
            if x == nil then
                things[id] = nil
                continue
            end
            t.t += dt
            if t.kind ~= "banana" then
                if t.t > SHELL_LIFE then
                    remove(id)
                    continue
                end
                local target = t.target and on[t.target] and t.target
                if target then
                    -- Turned towards it, five radians a second at most.
                    local qx, _, qz = core.player_pos(target)
                    if qx ~= nil then
                        local want = math.atan2(qx - x, qz - z)
                        local have = math.atan2(t.vx, t.vz)
                        local turn = (want - have + math.pi) % (2 * math.pi) - math.pi
                        have += math.clamp(turn, -5 * dt, 5 * dt)
                        t.vx, t.vz = math.sin(have) * SHELL, math.cos(have) * SHELL
                    end
                end
                local step = SHELL * dt
                local bx, _, bz = core.raycast(x, y + 0.35, z, t.vx / SHELL, 0, t.vz / SHELL, step + 0.3)
                if bx ~= nil then
                    -- Off the wall it met, along the side it met it on.
                    t.bounces += 1
                    if target or t.bounces > 5 then
                        remove(id)
                        continue
                    end
                    if bx ~= math.floor(x) then t.vx = -t.vx end
                    if bz ~= math.floor(z) then t.vz = -t.vz end
                else
                    x, z = x + t.vx * dt, z + t.vz * dt
                    local gy = ground(x, y, z)
                    y = if gy ~= nil then gy else y - 10 * dt
                    if y < GROUND - 16 then
                        remove(id)
                        continue
                    end
                    core.ecs.set(id, "x", x)
                    core.ecs.set(id, "y", y)
                    core.ecs.set(id, "z", z)
                    core.ecs.set(id, "yaw", t.t * 12)
                end
            end
            local gone = false
            for q, o in on do
                local qx, qy, qz = core.player_pos(q)
                if qx ~= nil and (qx - x) ^ 2 + (qz - z) ^ 2 < HIT * HIT and math.abs(qy - y) < 2
                        and not (q == t.owner and t.t < 0.6) then
                    hit(q, o, if t.kind == "banana" then "a banana" else "a shell")
                    gone = true
                    break
                end
            end
            if not gone and t.kind ~= "banana" then
                for other, u in things do
                    if other ~= id and u.kind == "banana" then
                        local ox, oz = core.ecs.get(other, "x"), core.ecs.get(other, "z")
                        if ox ~= nil and (ox - x) ^ 2 + (oz - z) ^ 2 < 1 then
                            remove(other)
                            gone = true
                            break
                        end
                    end
                end
            end
            if gone then
                remove(id)
            end
        end
    end

    core.event.on("core:net_message", function(ev)
        if ev.text == "off" then
            get_out(ev.player)
        elseif ev.text == "respawn" then
            respawn(ev.player)
        elseif ev.text == "use" or ev.text == "use back" then
            use_item(ev.player, ev.text == "use back")
        elseif string.sub(ev.text, 1, 5) == "pick " then
            pcall(core.event.emit, "track_core:pick", { player = ev.player, what = string.sub(ev.text, 6) })
        end
    end)

    -- The garage: the panel, and a click on it changes the kart at once --
    -- on the road too, for everybody to see -- and keeps it.
    core.command("garage", "-- the colours and the parts of your kart")
    core.event.on("core:command", function(ev)
        local s = players[ev.player]
        if ev.name == "garage" and s ~= nil then
            show_garage(ev.player, s)
        end
    end)
    core.event.on("core:ui_click", function(ev)
        local p = ev.player
        local s = players[p]
        if ev.panel ~= garage_panel(p) or s == nil then
            return
        end
        if ev.id == "close" or ev.id == "core:esc" then
            core.ui_show(ev.panel, false, p)
            return
        end
        local g = kart_of(s.name)
        local what, i, way = string.match(ev.id, "^([cp])(%d)([%+%-])$")
        local list, n = if what == "c" then g.paint else g.part, tonumber(i)
        if what == nil or list[n] == nil then
            return
        end
        local count = if what == "c" then #DYES else #PARTS[n].variants
        list[n] = (list[n] + (if way == "+" then 1 else count)) % (count + 1)
        core.storage_set(`kart:{s.name}`, `{table.concat(g.paint, " ")} {table.concat(g.part, " ")}`)
        dress_kart(p, s)
        show_garage(p, s)
    end)

    -- The track editor picking from afar (track_core): where the camera is
    -- held and what the HUD says, to the player's machine. The Inferno runs
    -- this mod without track_core, and there the event was never declared.
    pcall(core.event.on, "track_core:pick_view", function(ev)
        tell(ev.player, `pick {ev.text}`)
    end)

    function server_step(dt: number)
        local present = {}
        for _, p in core.players() do
            present[p] = true
            local x, y, z = core.player_pos(p)
            local s = players[p]
            if x == nil then
                -- not placed yet
            elseif s == nil then
                local name = core.player_name(p)
                for i = 1, #TRACKS do
                    local kept = tonumber(core.storage_get(`{stored(i, "best")}:{name}`))
                    if kept ~= nil and (bests[i][name] == nil or kept < bests[i][name]) then
                        bests[i][name] = kept
                    end
                end
                players[p] = { x = x, y = y, z = z, track = nil, next = 1, lapping = false,
                               started = 0, name = name, held = 0, driving = false, off = 0, skip = 0,
                               items = {}, boost = 0, star = 0, spin = 0, slow = 0,
                               speeds = `{KART.walk} {KART.sprint} {KART.accel}` }

                -- Driven in from the other room, still in the kart and on a
                -- lap (core.player_carried): the lap goes on here. Or in a kart
                -- and on no lap ("kart 0 ..."): got into in the Inferno by
                -- somebody who walked in there, and a kart all the same.
                local c = string.split(core.player_carried(p), " ")
                local ti = tonumber(c[2]) or 0
                if c[1] == "kart" then
                    local s = players[p]
                    s.driving, s.skip = true, 3
                    tell(p, "kart 1")
                    dress_kart(p, s)
                    if TRACKS[ti] ~= nil then
                        local gates = TRACKS[ti].gates
                        s.track = ti
                        s.next = math.clamp(tonumber(c[3]) or 1, 1, #gates)
                        s.lapping = c[4] == "1"
                        s.started = clock - (tonumber(c[5]) or 0)
                        tell_track(p, ti, name)
                        if s.lapping then
                            local passed = if s.next == 1 then #gates - 1 else s.next - 2
                            tell(p, string.format("run %.3f %d", clock - s.started, passed))
                        end
                        core.log(`track_game: {name} drives in, lapping {TRACKS[ti].key}`)
                    else
                        if not INFERNO then tell_track(p, 1, name) end
                        core.log(`track_game: {name} drives in, on no lap`)
                    end
                else
                    -- On foot, and this machine's copy of the mod may still
                    -- think it drives: it last heard "kart 1" here before the
                    -- kart left for the other room, and was drawing its own
                    -- body as a kart after walking back.
                    tell(p, "kart 0")
                    if not INFERNO then
                        -- In the Inferno only a driver has a track: somebody
                        -- who walked in from the sandbox is shown none.
                        tell_track(p, 1, name)
                    end
                end
                if bests[1][name] ~= nil then
                    core.log(string.format("track_game: %s is back, best %.3f s", name, bests[1][name]))
                end
            elseif s.held > 0 then
                -- The countdown: a line for each whole second, and the body let
                -- go at zero. Positions still move with it, so a body nudged by
                -- its arrival is not counted as having crossed anything.
                local was = math.ceil(s.held)
                s.held -= dt
                if s.held <= 0 then
                    release(p)
                    tell(p, "count 0")
                elseif math.ceil(s.held) < was then
                    tell(p, `count {math.ceil(s.held)}`)
                end
                s.x, s.y, s.z = x, y, z
            elseif not s.driving then
                -- A lap is driven. Somebody who got out and walked over the
                -- line has not crossed it, and their kart is where they left
                -- it.
                s.x, s.y, s.z = x, y, z
            elseif s.skip > 0 then
                -- Just put somewhere by the server (a respawn): nothing is
                -- crossed or left on the way there.
                s.skip -= 1
                s.x, s.y, s.z = x, y, z
            else
                -- A step longer than anyone runs in one is a portal or a
                -- correction, not a run across a line.
                local moved = math.abs(x - s.x) + math.abs(z - s.z)

                -- Over another track's start line: that track is the one being
                -- lapped now, from this line. On a lap only once its first
                -- checkpoint follows (below): a lap whose route crosses that
                -- line meets its own next gate first and goes on (`alt`).
                if moved < 20 then
                    for i, tr in TRACKS do
                        local f = if i ~= (s.track or 0) and #tr.gates > 0 and here(tr.gates[1])
                            then crossed(tr.gates[1], s.x, s.z, x, z) else nil
                        if f ~= nil and not s.lapping then
                            s.track, s.next, s.alt = i, 1, nil
                            tell_track(p, i, s.name)
                        elseif f ~= nil then
                            s.alt = { track = i, started = clock - dt + dt * f }
                        end
                    end
                end

                local ti    = s.track or 1
                local gates = TRACKS[ti].gates
                local f = if moved < 20 and s.track ~= nil and gates[s.next] ~= nil
                        and here(gates[s.next])
                    then crossed(gates[s.next], s.x, s.z, x, z) else nil
                if f ~= nil then
                    local t = clock - dt + dt * f
                    if s.next == 1 then
                        if s.lapping then
                            local lap  = t - s.started
                            local best = bests[ti][s.name]
                            if best == nil or lap < best then
                                bests[ti][s.name] = lap
                                best = lap
                                keep_best(ti, s.name)
                                tell_track_riders(ti, board_text(ti))
                            end
                            tell(p, string.format("lap %.3f %.3f", lap, best))
                            if TRACKS[ti].key == "inferno" then engineer_lap(p, s, lap) end
                            if TRACKS[ti].level then level_lap(p, ti, lap) end
                            core.log(string.format("track_game: %s lap %.3f s, best %.3f s%s",
                                                   s.name, lap, best,
                                                   if TRACKS[ti].key == "" then "" else ` ({TRACKS[ti].key})`))
                        else
                            tell(p, "go")
                        end
                        s.lapping = true
                        s.started = t
                    else
                        tell(p, string.format("cp %d %d %.3f", s.next - 1, #gates - 1,
                                              t - s.started))
                    end
                    s.next = s.next % #gates + 1
                    s.alt = nil   -- the lap went on past the other line
                end

                -- Or the other track's first checkpoint, before this one's
                -- next gate: the other track it is, timed from its line.
                local a = s.alt
                local g2 = a and TRACKS[a.track].gates[2]
                if moved < 20 and g2 ~= nil and here(g2) and crossed(g2, s.x, s.z, x, z) ~= nil then
                    local ag = TRACKS[a.track].gates
                    s.track, s.started, s.alt = a.track, a.started, nil
                    s.next = if #ag >= 3 then 3 else 1
                    tell_track(p, a.track, s.name)
                    tell(p, string.format("run %.3f %d", clock - s.started, 1))
                    core.log(`track_game: {s.name} took the line onto {TRACKS[a.track].key}`)
                end

                -- Off the road -- on a wall, in the grass inside the loop, out
                -- past it -- for OFF_ROAD, and the kart is put back on the
                -- nearest road, standing, facing the way it was going. Only
                -- on a lap: before its start line is crossed a kart goes
                -- where it likes, which is how it reaches pieces somebody
                -- built out in the country, and leaves them again.
                if not s.lapping or over_road(x, y, z) then
                    s.off = 0
                else
                    s.off += dt
                    if s.off >= OFF_ROAD then
                        s.off = 0
                        local rx, rz, ry = road_near(x, y, z)
                        if rx ~= nil and rz ~= nil then
                            local lx, _, lz = core.player_look(p)
                            lx, lz = lx or 0, lz or 0
                            if math.abs(lx) + math.abs(lz) < 0.01 then
                                lx, lz = 0, 1
                            end
                            core.player_place(p, rx, ry or GROUND, rz, lx, 0, lz)
                            core.log(string.format("track_game: %s off the road at %.1f %.1f %.1f, "
                                                   .. "put back at %.1f %.1f", s.name, x, y, z, rx, rz))
                            s.skip = 3   -- and no line is crossed by being put back
                        end
                    end
                end
                s.x, s.y, s.z = x, y, z
            end

            -- What goes with a kart driven into the other room: its lap.
            local d = players[p]
            if d ~= nil and d.driving then
                core.player_carry(p, string.format("kart %d %d %d %.3f", d.track or 0, d.next,
                                                   if d.lapping then 1 else 0, clock - d.started))
            elseif d ~= nil and core.player_carried(p) ~= "" and string.sub(core.player_carried(p), 1, 5) == "kart " then
                -- On foot the line goes along too now: not a lap any more.
                core.player_carry(p, "")
            end
        end

        -- And whoever has gone. A player who leaves in a kart leaves it
        -- behind: without this the track loses one every time somebody drives
        -- into the portal home. Not when the kart went too, driven into
        -- another room that takes it (core.body_taken).
        for p, s in players do
            if not present[p] then
                if s.driving and core.body_taken(p) == 0 then
                    park(s.x, s.y, s.z, 0, 1)
                    core.log(string.format("track_game: %s gone, their kart parked at %.1f %.1f",
                                           s.name, s.x, s.z))
                elseif s.driving then
                    core.log(`track_game: {s.name} drove into another room, and the kart went too`)
                end
                players[p] = nil
            end
        end
        items_step(dt)
    end
end

-- ===================================================== the player's machine

if drawing then
    local total   = -1     -- checkpoints in a lap, once the server has said
    local running = false
    local time    = 0
    local passed  = 0
    local last    = -1
    local best    = -1
    local flash   = ""
    local flash_left = 0
    local count   = -1     -- the countdown's number, -1 when there is none
    local board   = {}     -- { name, t } rows, fastest first
    local driving = false  -- in a kart, told by the server
    local picking = ""     -- the piece on trial while the track editor picks from afar
    -- And the grid of every piece drawn instead of the hotbar meanwhile: the
    -- pieces by their place in track_core's list, the one on trial, those
    -- that fit the open end.
    local pieces: { string } = {}
    local pick_sel = 0
    local pick_fits: { [number]: boolean } = {}
    local GRID_COLS, GRID_SIDE, GRID_GAP = 16, 0.05, 0.006
    -- The middle of square n, x a fraction of the width and y of the height.
    local function grid_at(n: number): (number, number)
        local rows = (#pieces + GRID_COLS - 1) // GRID_COLS
        local col, row = (n - 1) % GRID_COLS, (n - 1) // GRID_COLS
        local step = GRID_SIDE + GRID_GAP
        local aspect = math.max(core.hud.aspect(), 0.1)
        return 0.5 + (col - (GRID_COLS - 1) / 2) * step / aspect,
               1 - 0.02 - GRID_SIDE / 2 - (rows - 1 - row) * step
    end
    -- The square under the cursor, or 0.
    local function grid_under(): number
        local mx, my = core.hud.mouse()
        if mx == nil then
            return 0
        end
        local aspect = math.max(core.hud.aspect(), 0.1)
        for n = 1, #pieces do
            local x, y = grid_at(n)
            if math.abs(mx - x) * aspect <= GRID_SIDE / 2 and math.abs(my - y) <= GRID_SIDE / 2 then
                return n
            end
        end
        return 0
    end

    -- The power-up held (server's "item"), and what is on this kart or on
    -- this screen ("fx"), counted down here.
    local slots = {}       -- { what, uses, rolling }, the one used next first
    local fx = { boost = 0, star = 0, spin = 0, slow = 0, ink = 0, bolt = 0 }
    local blots = {}       -- the ink: { x, y, s } on the screen
    local back_held = false
    local NAMES = { boost = "BOOST", boost3 = "TRIPLE BOOST", banana = "BANANA", shell = "SHELL",
                    homing = "HOMING SHELL", ink = "INK", star = "STAR", bolt = "LIGHTNING" }
    local COLOURS = { boost = { 255, 120, 60 }, boost3 = { 255, 120, 60 }, banana = { 250, 215, 50 },
                      shell = { 60, 200, 80 }, homing = { 230, 50, 40 }, ink = { 120, 110, 200 },
                      star = { 255, 230, 80 }, bolt = { 120, 200, 255 } }
    local ORDER = { "boost", "banana", "shell", "homing", "ink", "star", "bolt", "boost3" }

    local function clock_text(t: number): string
        return string.format("%d:%05.2f", t // 60, t % 60)
    end

    heard = function(text: string)
        local w = string.split(text, " ")
        if w[1] == "pick" and w[2] == "list" then
            -- "pick list <first> name,name,..."
            local first = tonumber(w[3]) or 1
            for k, name in string.split(w[4] or "", ",") do
                pieces[first + k - 1] = name
            end
        elseif w[1] == "pick" then
            -- "pick off", or "pick x y z distance yaw pitch|label|on trial|fits"
            local parts = string.split(string.sub(text, 6), "|")
            if #parts < 2 then
                picking = ""
                core.fixed_view(0, 0, 0, 0, 0, 50, 0, 0.6)
                core.engine_hud(true)
            else
                picking = parts[2]
                pick_sel = tonumber(parts[3]) or 0
                pick_fits = {}
                for _, n in string.split(parts[4] or "", ",") do
                    if tonumber(n) then pick_fits[tonumber(n) :: number] = true end
                end
                core.engine_hud(false)
                local n = string.split(parts[1], " ")
                core.fixed_view(tonumber(n[1]) or 0, tonumber(n[2]) or 0, tonumber(n[3]) or 0,
                    tonumber(n[4]) or 1, tonumber(n[5]) or 0, tonumber(n[6]) or 50, 0, 0.6)
            end
        elseif w[1] == "ready" then
            total, running, passed = tonumber(w[2]) or -1, false, 0
        elseif w[1] == "count" then
            count = tonumber(w[2]) or -1
            if count == 0 then
                flash, flash_left, count = "GO", 1.0, -1
            end
        elseif w[1] == "best" then
            best = tonumber(w[2]) or best
        elseif w[1] == "board" then
            board = {}
            for i = 2, #w - 1, 2 do
                table.insert(board, { name = w[i], t = tonumber(w[i + 1]) or 0 })
            end
        elseif w[1] == "go" then
            running, time, passed = true, 0, 0
        elseif w[1] == "run" then
            -- A lap driven in from the other room, so far.
            running, time, passed = true, tonumber(w[2]) or 0, tonumber(w[3]) or 0
        elseif w[1] == "cp" then
            passed = tonumber(w[2]) or passed
            flash = `CHECKPOINT {w[2]}   {clock_text(tonumber(w[4]) or 0)}`
            flash_left = 1.5
        elseif w[1] == "kart" then
            -- In a kart or out of one: a kart is driven from behind it, and a
            -- body on foot is looked out of.
            -- Only when it changes: "kart 0" to a body already on foot
            -- (walked back in) leaves the view the player chose.
            local now = w[2] == "1"
            if now == driving then return end
            driving = now
            core.view(if driving then 5 else 0)
            -- What this machine draws its own body as: the server dresses
            -- everybody else, and its welcome carries one model, the robot.
            core.body_model(0, if driving then core.model_id("track_core:kart") else 0)
            if not driving then
                running, count = false, -1
                slots = {}
            end
        elseif w[1] == "dress" then
            -- The kart this machine draws its own body in (the server's
            -- dress_kart): the parts in slots 1..5, the kart itself, painted, in 6.
            local tint = tonumber(w[2]) or 0
            for i = 1, 6 do
                local m = tonumber(w[2 + i]) or 0
                core.player_wear(1, i, m, if m ~= 0 then tint else 0)   -- this machine's own, as core.players() has it
            end
        elseif w[1] == "items" then
            slots = {}
            for i = 2, #w do
                local f = string.split(w[i], ":")
                table.insert(slots, { what = f[1], uses = tonumber(f[2]) or 1, rolling = tonumber(f[3]) or 0 })
            end
        elseif w[1] == "fx" then
            local t = tonumber(w[3]) or 0
            if w[2] == "clear" then
                for k in fx do fx[k] = 0 end
            elseif w[2] == "ink" then
                fx.ink = t
                blots = {}
                for _ = 1, 9 do
                    table.insert(blots, { x = 0.15 + math.random() * 0.7, y = 0.2 + math.random() * 0.6,
                                          s = 0.25 + math.random() * 0.3 })
                end
            elseif w[2] == "boltgo" or w[2] == "inkgo" then
                fx.bolt = if w[2] == "boltgo" then 0.3 else 0
                flash, flash_left = `{if w[2] == "boltgo" then "LIGHTNING" else "INK"} FROM {w[3] or ""}`, 1.5
            elseif fx[w[2]] ~= nil then
                fx[w[2]] = t
                if w[2] == "spin" then
                    flash, flash_left = "SPIN!", 1.0
                elseif w[2] == "boost" then
                    flash, flash_left = "BOOST", 0.6
                elseif w[2] == "star" then
                    flash, flash_left = "STAR", 1.0
                end
            end
        elseif w[1] == "lap" then
            last, best = tonumber(w[2]) or 0, tonumber(w[3]) or 0
            running, time, passed = true, 0, 0
            flash = `LAP {clock_text(last)}`
            flash_left = 2.5
        end
    end

    if not serving then
        core.event.on("core:net_message", function(ev)
            heard(ev.text)
        end)
    end

    -- Getting out is F -- a key a driver presses meaning to, where a right
    -- click got them out by mistake; the engine's own F (fly) does nothing to a
    -- body that drives. A right click while driving is claimed and does
    -- nothing, so it does not build or open something either. Getting IN is an
    -- ordinary right click on the parked kart, which the engine carries to the
    -- server as core:used.
    -- The track editor picking from afar: B in and out, A and D the piece on
    -- trial, a left click on the grid puts that one on trial instead (and
    -- nothing anywhere else: a click that kept whatever was on trial put
    -- pieces down nobody wanted), W keeps it, S takes the last back, F closes
    -- the circuit.
    local PICK_KEYS = { key_a = "prev", key_d = "next", key_w = "put", key_s = "undo", key_f = "close" }
    local function pick(player: number, what: string)
        if serving then
            pcall(core.event.emit, "track_core:pick", { player = player, what = what })
        else
            core.net.send(0, `pick {what}`)
        end
    end

    core.event.on("core:input", function(ev)
        if ev.action == "key_b" and (picking ~= "" or core.held_item(1) == "track_core:editor") then
            pick(ev.player, "toggle")
        elseif picking ~= "" and (PICK_KEYS[ev.action] ~= nil or ev.action == "mouse_right"
                                  or ev.action == "mouse_left") then
            ev.cancel = true
            if PICK_KEYS[ev.action] then
                pick(ev.player, PICK_KEYS[ev.action])
            elseif ev.action == "mouse_left" then
                local n = grid_under()
                if n > 0 then
                    pick(ev.player, `sel {n}`)
                end
            end
        elseif ev.action == "mouse_right" and driving then
            ev.cancel = true
        elseif ev.action == "key_f" and driving then
            ev.cancel = true
            if serving then
                get_out(ev.player)
            else
                core.net.send(0, "off")
            end
        elseif ev.action == "mouse_left" and driving then
            -- The power-up, once the roulette has stopped; S held throws it
            -- behind. Claimed whatever: a kart does not dig.
            ev.cancel = true
            if slots[1] ~= nil and slots[1].rolling <= 0 then
                if serving then
                    use_item(ev.player, back_held)
                else
                    core.net.send(0, if back_held then "use back" else "use")
                end
            end
        elseif ev.action == "key_s" or ev.action == "key_s_up" then
            back_held = ev.action == "key_s"
        elseif ev.action == "key_r" and driving then
            ev.cancel = true
            if serving then
                respawn(ev.player)
            else
                core.net.send(0, "respawn")
            end
        end
    end)

    -- Below the engine's debug lines, which own the top left.
    core.hud.pass("track_game:hud", 100, function()
        if picking ~= "" then
            core.hud.text(0.5, 0.12, picking, 0.035, 1, 255, 255, 255, 255)
            core.hud.text(0.5, 0.17,
                "CLICK A PIECE  TRY IT    W  KEEP IT    A / D  ANOTHER ONE THAT FITS    S  UNDO    F  CLOSE CIRCUIT    B  BACK",
                0.018, 1, 200, 200, 200, 255)
            -- The grid: what fits the open end lit, the rest dimmed, the one
            -- on trial framed in yellow, the one under the cursor named.
            local under = grid_under()
            local aspect = math.max(core.hud.aspect(), 0.1)
            local rows = (#pieces + GRID_COLS - 1) // GRID_COLS
            local step = GRID_SIDE + GRID_GAP
            local _, top = grid_at(1)
            core.hud.rect(0.5, top - GRID_SIDE / 2 + rows * step / 2,
                GRID_COLS * step + GRID_GAP, rows * step + GRID_GAP, 15, 15, 18, 190)
            for n, name in pieces do
                local x, y = grid_at(n)
                core.hud.item(`track_core:{name}`, x, y, GRID_SIDE)
                if not pick_fits[n] then
                    core.hud.rect(x, y, GRID_SIDE, GRID_SIDE, 0, 0, 0, 170)
                end
                if n == pick_sel or n == under then
                    local c = if n == pick_sel then { 255, 220, 80 } else { 255, 255, 255 }
                    local t, half = 0.004, GRID_SIDE / 2
                    core.hud.rect(x, y - half, GRID_SIDE, t, c[1], c[2], c[3], 255)
                    core.hud.rect(x, y + half, GRID_SIDE, t, c[1], c[2], c[3], 255)
                    core.hud.rect(x - half / aspect, y, t, GRID_SIDE, c[1], c[2], c[3], 255)
                    core.hud.rect(x + half / aspect, y, t, GRID_SIDE, c[1], c[2], c[3], 255)
                end
            end
            if under > 0 then
                local _, y = grid_at(1)
                core.hud.text(0.5, y - GRID_SIDE, `{pieces[under]}{if pick_fits[under] then "" else "  (does not fit here)"}`,
                    0.025, 1, 255, 255, 255, 255)
            end
        end
        if total < 0 then
            return
        end
        if count > 0 then
            core.hud.text(0.5, 0.3, `{count}`, 0.12, 1, 255, 210, 80, 255)
        elseif running then
            core.hud.text(0.5, 0.22, clock_text(time), 0.06, 1, 255, 255, 255, 255)
            core.hud.text(0.5, 0.28, `CHECKPOINT {passed} / {total}`, 0.03, 1, 220, 220, 220, 255)
            core.hud.text(0.5, 0.32, "CTRL  DRIFT    R  BACK TO THE LAST CHECKPOINT    F  GET OUT", 0.018, 1, 180, 180, 180, 255)
        elseif not driving then
            if picking == "" then
                core.hud.text(0.5, 0.22, "RIGHT CLICK A KART TO GET IN", 0.04, 1, 255, 255, 255, 255)
            end
        else
            core.hud.text(0.5, 0.22, "CROSS THE LINE TO START", 0.04, 1, 255, 255, 255, 255)
            core.hud.text(0.5, 0.27, "CTRL  DRIFT    F  GET OUT", 0.018, 1, 180, 180, 180, 255)
        end
        if last >= 0 then
            core.hud.text(0.97, 0.22, `LAST {clock_text(last)}`, 0.03, 2, 220, 220, 220, 255)
        end
        if best >= 0 then
            core.hud.text(0.97, 0.27, `BEST {clock_text(best)}`, 0.03, 2, 255, 210, 80, 255)
        end
        if #board > 0 then
            core.hud.text(0.97, 0.36, "FASTEST", 0.025, 2, 200, 200, 200, 255)
            for i, row in board do
                core.hud.text(0.97, 0.36 + 0.04 * i, `{row.name}  {clock_text(row.t)}`, 0.025, 2,
                              235, 235, 235, 255)
            end
        end
        if flash_left > 0 then
            core.hud.text(0.5, 0.4, flash, 0.045, 1, 255, 210, 80,
                          math.floor(255 * math.min(1, flash_left * 2)))
        end

        -- The power-ups: two slots on the left, the one used next below and
        -- bigger, the roulette turning in a slot for a second after a box.
        if driving then
            local function slot(y: number, size: number, it)
                core.hud.rect(0.08, y, size, size, 20, 20, 30, 170)
                core.hud.rect(0.08, y, size * 0.88, size * 0.88, 50, 50, 70, 200)
                if it == nil then
                    return nil
                end
                local shown = it.what
                if it.rolling > 0 then
                    shown = ORDER[math.floor(it.rolling * 14) % #ORDER + 1]
                end
                local icon = if shown == "boost3" then "boost" else shown
                core.hud.image(`track_core_item_{icon}.png`, 0.08, y, size * 0.77, size * 0.77, 255)
                return shown
            end
            slot(0.385, 0.08, slots[2])
            local first = slots[1]
            local shown = slot(0.5, 0.13, first)
            if shown ~= nil then
                local c = COLOURS[shown]
                local label = NAMES[shown]
                if first.rolling <= 0 and first.uses > 1 then
                    label = `{label}  x{first.uses}`
                end
                core.hud.text(0.08, 0.59, label, 0.022, 1, c[1], c[2], c[3], 255)
                if first.rolling <= 0 then
                    core.hud.text(0.08, 0.62, "LEFT CLICK  (S: BEHIND)", 0.015, 1, 190, 190, 190, 255)
                end
            else
                core.hud.text(0.08, 0.59, "NO ITEM", 0.018, 1, 150, 150, 150, 255)
            end
        end

        -- A star: the edges of the screen in turning colours.
        if fx.star > 0 then
            local h = (clock * 4) % 3
            local r = math.floor(255 * math.clamp(math.abs(h - 1.5) - 0.5, 0, 1))
            local g = math.floor(255 * math.clamp(1 - math.abs(h - 1), 0, 1))
            local b = math.floor(255 * math.clamp(1 - math.abs(h - 2), 0, 1))
            core.hud.rect(0.5, 0.01, 4, 0.02, r, g, b, 200)
            core.hud.rect(0.5, 0.99, 4, 0.02, r, g, b, 200)
            core.hud.rect(0.0, 0.5, 0.04, 1, r, g, b, 200)
            core.hud.rect(1.0, 0.5, 0.04, 1, r, g, b, 200)
        end
        if fx.boost > 0 then
            core.hud.rect(0.5, 0.5, 4, 1, 255, 140, 40, math.floor(50 * fx.boost))
        end
        if fx.slow > 0 then
            core.hud.text(0.5, 0.46, "SLOWED", 0.03, 1, 120, 200, 255, 220)
        end
        if fx.bolt > 0 then
            core.hud.rect(0.5, 0.5, 4, 1, 255, 255, 255, math.floor(600 * fx.bolt))
        end
        -- Ink: blots over the middle, gone in the last second.
        if fx.ink > 0 then
            local a = math.floor(235 * math.min(1, fx.ink))
            for _, b in blots do
                core.hud.image("track_core_ink_blot.png", b.x, b.y, b.s, b.s, a)
            end
        end
    end)

    function client_step(dt: number)
        if running then
            time += dt
        end
        if flash_left > 0 then
            flash_left -= dt
        end
        for _, it in slots do
            it.rolling = math.max(0, it.rolling - dt)
        end
        for k, t in fx do
            fx[k] = math.max(0, t - dt)
        end
    end
end

function on_step(dt: number)
    clock += dt
    if serving then
        server_step(dt)
    end
    if drawing then
        client_step(dt)
    end
end
