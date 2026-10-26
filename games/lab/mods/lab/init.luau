--!strict
-- lab: the engineer's laboratory and the end of the game (TODO.md "The game
-- as a whole"). Behind the plaza's eighth gate, which opens only to somebody
-- who has held all seven keys (mods/keys). A hall of steel round the breach,
-- and in it l'Ospite, the thing from between the worlds, beaten with a little
-- of every world:
--
--   1 lo scudo     it floats in a cage of light fed by two generators shut
--                  behind glass in the north wall: the portal gun (the test
--                  chambers) is the way in to break them by hand, and out
--   2 l'assedio    bare, it is hurt by the base's guns: a pistol that never
--                  runs dry and reaches far, a shotgun for close, and the
--                  chaingun one of its creatures leaves
--   3 la vetta     the floor gives way: up onto the gallery round the hall,
--                  by the chimneys in its corners or the rope (parkour)
--   4 il fuoco     lava fills the hall to the gallery and it heals in it while
--                  any of three valves on the walls under the lava is open:
--                  whoever carries the Chiave del Fuoco swims there unburnt
--   5 il buio      the lamps die and it is only heard: the engineer's torch
--                  (the backrooms), in the hotbar, shows it for a moment
--                  and only then is it hurt
--   6 il gelo      its last form is fast: the crypt's frost (the waste), a
--                  crystal in the hotbar, slows it
--   7 la corsa     dead, the laboratory falls in: a kart (the track) out
--                  through the north wall and along a road of track pieces
--                  in the dark -- banked turns, a hill -- before it falls in
--                  too; at its end the credits and the way home
--
-- All along it calls up the base's imps and ghouls, a couple at a time,
-- which leave shells, bullets, a medkit or the chaingun where they fall; it
-- rises and sinks between the floor and the gallery; and now and then it
-- raises its tentacles, aims them at somebody -- a thin red line, a second
-- to get out of it or behind something -- and fires its lasers. Out of the
-- pit, once the floor is gone, a parkour route climbs back to the gallery.
--
-- Its numbers go to everybody here; it fights all of them at once. When
-- nobody is left in the laboratory it is built again as it was.
--
-- `side = both`: the server runs the fight, a player's machine the clicks,
-- the rope, the shotgun's arms, the bar over the screen and the credits.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"
local clock   = 0.0

local heard: (string) -> () = function(_) end
local function tell(player: number, text: string)
    if drawing then
        heard(text)
    else
        core.net.send(player, text)
    end
end

local EYE = 1.5
local OSPITE_HP = 1000

-- ===================================================================== server

if serving then
    core.net.open()

    local id = core.node_id
    local WALL, FLOOR, STEEL = id("lab:wall"), id("lab:floor"), id("lab:steel")
    local LAMP, LAMP_OFF, BREACH, HAZARD = id("lab:lamp"), id("lab:lamp_off"), id("lab:breach"), id("lab:hazard")
    local PANEL, GLASS, GEN = id("lab:panel"), id("lab:glass"), id("lab:generator")
    local VALVE, VALVE_SHUT, SEAL, LAVA = id("lab:valve_open"), id("lab:valve_shut"), id("lab:seal"), id("lab:lava")
    local SHELL_LOW, SHELL_HIGH, KART = id("lab:shell_low"), id("lab:shell_high"), id("lab:kart")

    core.mapgen_core(false)
    core.world_sky(4, 4, 8)

    -- ---------------------------------------------------------------- the hall
    --
    -- Inside x and z -16..16, its floor's top at 20 (a body stands at 21),
    -- the roof at 38. Under the floor the pit it falls into, down to 7. A
    -- gallery three wide round the walls at 27 (standing at 28), a chimney on
    -- each side up to it -- the parkour world's station 4: a gap two wide
    -- between a wall to 31 and one to 27 -- and a steel beam under the roof
    -- across the middle for the rope. Two cells behind glass in the north
    -- wall hold the generators; the tunnel out goes north from the gallery.
    local BOXES: { { number } } = {}
    local function box(x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: number)
        table.insert(BOXES, { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1),
                              math.max(x0, x1), math.max(y0, y1), math.max(z0, z1), b })
    end
    local FL, GY, ROOF = 20, 27, 38

    box(-17, 6, -17, 17, ROOF, 17, WALL)                       -- the whole, then hollowed
    box(-16, 7, -16, 16, ROOF - 1, 16, 0)
    box(-16, 6, -16, 16, 6, 16, STEEL)                          -- the pit's bottom
    box(-16, FL - 1, -16, 16, FL, 16, FLOOR)                     -- the floor that gives way
    box(-16, GY, -16, 16, GY, -14, STEEL)                        -- the gallery
    box(-16, GY, 14, 16, GY, 16, STEEL)
    box(-16, GY, -13, -14, GY, 13, STEEL)
    box(14, GY, -13, 16, GY, 13, STEEL)
    for _, sx in { -1, 1 } do                                   -- a chimney west and east
        box(sx * 13, FL + 1, -5, sx * 13, GY, 4, STEEL)          -- B, up to the gallery
        box(sx * 10, FL + 1, -5, sx * 10, FL + 11, 4, STEEL)     -- A, higher
        box(sx * 12, FL + 1, -6, sx * 11, FL + 11, -6, STEEL)    -- shut at its south end
    end
    box(-16, ROOF - 3, 0, 16, ROOF - 2, 1, STEEL)                -- the beam for the rope
    for x = -12, 12, 8 do                                       -- lamps in the roof, and under the gallery
        for z = -12, 12, 8 do box(x, ROOF - 1, z, x, ROOF - 1, z, LAMP) end
        box(x, GY - 1, -16, x, GY - 1, -16, LAMP)
        box(x, GY - 1, 16, x, GY - 1, 16, LAMP)
    end
    box(-5, FL, 4, 5, FL, 8, HAZARD)                            -- the ring of the breach, standing
    for a = 0, 31 do
        local t = a / 32 * 2 * math.pi
        local x, y = math.floor(math.cos(t) * 4 + 0.5), math.floor(math.sin(t) * 4 + 0.5) + FL + 5
        box(x, y, 6, x, y, 6, BREACH)
    end
    -- The way back in: a frame round portal 0 (facing +z) in the south wall.
    box(-1, FL + 1, -17, 1, FL + 3, -17, STEEL)
    box(0, FL + 1, -17, 0, FL + 2, -17, 0)
    box(4, FL + 1, -17, 8, FL + 4, -17, PANEL)                  -- white beside it, for a portal
    -- The generators' cells: x -12..-6 and 6..12, z 18..22, behind glass three
    -- high; over the glass the room is open, and on its back wall a white
    -- panel high up is seen from the hall, a low one from inside.
    for _, sx in { -1, 1 } do
        local xa, xb = sx * 6, sx * 12
        box(xa, FL, 18, xb, FL, 22, FLOOR)
        box(xa, FL + 1, 18, xb, GY, 22, 0)
        box(xa, FL + 1, 17, xb, FL + 3, 17, GLASS)
        box(xa, FL + 4, 17, xb, GY - 1, 17, 0)
        box(sx * 8, FL + 5, 23, sx * 10, FL + 7, 23, PANEL)
        box(sx * 8, FL + 1, 23, sx * 10, FL + 3, 23, PANEL)
        box(sx * 9, FL + 1, 20, sx * 9, FL + 1, 20, GEN)
    end
    box(-3, FL + 1, 17, 3, FL + 4, 17, PANEL)                   -- white on the north wall between
    -- The valves, in the walls at the old floor's height.
    local VALVES = { { -17, FL + 2, 8 }, { 17, FL + 2, -8 }, { 0, FL + 2, 17 } }
    for _, v in VALVES do box(v[1], v[2], v[3], v[1], v[2], v[3], VALVE) end
    -- The way back up out of the pit once the floor is gone: four stations
    -- of the parkour world, built as the floor gives way (it would stand
    -- through the floor before). Each is the same cells as the station it
    -- copies (games/parkour), so the same jumps reach:
    --   1  the south chimney: a gap two wide between the pit's south wall and
    --      a wall to 13, from the pit's floor (standing at 7) up onto a
    --      ledge at 13
    --   2  a wall run east over a gap eight wide, the wall along its south
    --   3  the east chimney: between the pit's east wall and a wall to 20,
    --      onto where the floor was
    --   4  round the west of the hall's east chimney and into it from the
    --      north: it climbs onto the east gallery
    local ROUTE: { { number } } = {}
    local function rbox(x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: number)
        table.insert(ROUTE, { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1),
                              math.max(x0, x1), math.max(y0, y1), math.max(z0, z1), b })
    end
    -- 1: gap x -8..1, z -16..-15 on the pit's floor; B at z -14; the ledge
    --    beyond it; shut at x -9.
    rbox(-8, 7, -14, 1, 13, -14, WALL)
    rbox(-8, 7, -13, 1, 13, -9, WALL)
    rbox(-9, 7, -16, -9, 16, -15, WALL)
    rbox(1, 13, -13, 1, 13, -9, HAZARD)
    -- 2: the gap x 2..9, the landing x 10..15, the wall z -14 from 14 to 18.
    rbox(10, 7, -13, 15, 13, -9, WALL)
    rbox(-1, 14, -14, 11, 18, -14, WALL)
    -- 3: gap x 15..16, z -8..1 on 13; B at x 14 to 20; beyond it the
    --    platform x 10..13 up to 20; shut at z 2.
    rbox(15, 7, -8, 16, 13, 1, WALL)
    rbox(14, 14, -8, 14, 20, 1, WALL)
    rbox(10, 7, -8, 13, 20, 1, WALL)
    rbox(15, 14, 2, 16, 23, 2, WALL)
    rbox(15, 13, -9, 16, 13, -9, HAZARD)
    -- 4: a lane at 20 west of the hall's chimney and across its north end,
    --    and the chimney's own floor back under it.
    rbox(7, 7, -8, 9, 20, 6, WALL)
    rbox(10, 20, 5, 12, 20, 6, WALL)
    rbox(11, 20, 2, 12, 20, 4, WALL)
    rbox(10, 20, 6, 12, 20, 6, HAZARD)
    -- Lamps in the pit's walls, seen once the floor is gone.
    for k = -12, 12, 8 do
        for _, y in { 10, 16 } do
            box(-17, y, k, -17, y, k, LAMP)
            box(17, y, k, 17, y, k, LAMP)
            box(k, y, -17, k, y, -17, LAMP)
            box(k, y, 17, k, y, 17, LAMP)
        end
    end
    -- The way out: north from the gallery through a gap in the wall (sealed
    -- until l'Ospite is dead), an apron, and a road of track pieces out into
    -- the dark (the escape, below), at the gallery's height: a body stands
    -- on it at GY + 1, as on the gallery.
    box(-7, GY + 1, 17, 6, GY + 4, 17, SEAL)
    box(-8, GY, 17, 7, GY, 23, STEEL)
    box(-8, GY + 1, 18, 7, GY + 6, 23, 0)

    local function put_all()
        for _, b in BOXES do
            for x = b[1], b[4] do
                for z = b[3], b[6] do
                    for y = b[2], b[5] do core.set_block(x, y, z, b[7]) end
                end
            end
        end
    end
    core.register_mapgen("lab:hall", function(ctx)
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

    core.set_portal(0, 0.5, FL + 2, -16.4)
    -- ---------------------------------------------------------- the escape
    --
    -- Laid by track_core's turtle (games/track: the grand circuit), each wide
    -- piece by the square of its right lane, the next one's square following
    -- from its shape: north, a banked turn to the right with the tilts into
    -- and out of it, a rise over a hill, a banked turn to the left, down the
    -- hill, and a platform with the way home. A lamp hangs six over every square.
    local FWD = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }   -- +z, +x, -z, -x
    local function right_of(t: number) return FWD[(t + 3) % 4 + 1] end
    local ESCAPE = {
        { "straight3" }, { "straight2" }, { "tilt_in_r" }, { "bank_r", "r" }, { "tilt_out_r" },
        { "straight2" }, { "rise", "up" }, { "straight2" }, { "rise", "down" }, { "tilt_in_l" },
        { "bank_l", "l" }, { "tilt_out_l" }, { "straight3" },
    }
    local SIZE = { straight2 = 2, straight3 = 3, rise = 2, tilt_in_r = 3, tilt_out_r = 3, bank_r = 3,
                   tilt_in_l = 3, tilt_out_l = 3, bank_l = 3 }
    local ROAD_Y = GY + 1
    local pieces_left: { { number } } = {}
    local road_squares: { { number } } = {}
    local gx, gz, t, ry = -1, 3, 0, ROAD_Y      -- right lane square x -8..-1, z 24..31; heading +z
    for _, step in ESCAPE do
        local name, how = step[1], step[2]
        local n = SIZE[name]
        local f, r = FWD[t + 1], right_of(t)
        local piece = id(`lab:piece_{name}`)
        if how == "r" or how == "l" then
            for a = 0, n - 1 do
                for b = 0, n - 1 do
                    local side = if how == "r" then a - (n - 2) else a
                    table.insert(road_squares, { gx + f[1] * b - r[1] * side, gz + f[2] * b - r[2] * side, ry })
                end
            end
            table.insert(pieces_left, { gx * 8, ry, gz * 8, piece, (t + 2) % 4 })
            if how == "r" then
                gx, gz = gx + f[1] * (n - 2) + r[1] * (n - 1), gz + f[2] * (n - 2) + r[2] * (n - 1)
                t = (t + 3) % 4
            else
                gx, gz = gx + f[1] * (n - 1) - r[1] * n, gz + f[2] * (n - 1) - r[2] * n
                t = (t + 1) % 4
            end
        else
            for k = 0, n - 1 do
                for j = 0, 1 do table.insert(road_squares, { gx + f[1] * k - r[1] * j, gz + f[2] * k - r[2] * j, ry }) end
            end
            local climb = if name == "rise" then 2 else 0
            if how == "down" then
                local ex, ez = gx + f[1] * (n - 1) - r[1], gz + f[2] * (n - 1) - r[2]
                table.insert(pieces_left, { ex * 8, ry - climb, ez * 8, piece, (t + 4) % 4 })
                ry -= climb
            else
                table.insert(pieces_left, { gx * 8, ry, gz * 8, piece, (t + 2) % 4 })
                if how == "up" then ry += climb end
            end
            gx, gz = gx + f[1] * n, gz + f[2] * n
        end
    end
    -- The end: a platform of the two squares after the last piece, and on its
    -- west edge the doorway home (portal 1, facing +x); heading +z again.
    local END = { x0 = math.min(gx, gx - right_of(t)[1]) * 8, z0 = gz * 8, y = ry }
    box(END.x0, ry - 1, END.z0, END.x0 + 15, ry - 1, END.z0 + 15, STEEL)
    box(END.x0, ry, END.z0, END.x0 + 15, ry + 5, END.z0 + 15, 0)
    box(END.x0 - 2, ry, END.z0 + 5, END.x0 - 1, ry + 3, END.z0 + 9, STEEL)
    box(END.x0 - 1, ry, END.z0 + 7, END.x0 - 1, ry + 1, END.z0 + 7, 0)
    box(END.x0 + 4, ry + 6, END.z0 + 4, END.x0 + 4, ry + 6, END.z0 + 4, LAMP)
    box(END.x0 + 11, ry + 6, END.z0 + 11, END.x0 + 11, ry + 6, END.z0 + 11, LAMP)
    for _, sq in road_squares do
        box(sq[1] * 8 + 4, sq[3] + 6, sq[2] * 8 + 4, sq[1] * 8 + 4, sq[3] + 6, sq[2] * 8 + 4, LAMP)
    end
    core.set_portal(1, END.x0 - 0.4, ry + 1, END.z0 + 7.5)

    -- Pieces wait for their columns (far from anybody until somebody
    -- drives there): asked again every two seconds until all are down.
    local piece_wait = 0
    local function place_pieces()
        local still = {}
        for _, q in pieces_left do
            if core.get_block(q[1], q[2], q[3]) ~= q[4] and core.place_piece(q[1], q[2], q[3], q[4], q[5]) ~= 1 then
                table.insert(still, q)
            end
        end
        pieces_left = still
    end

    local START = { 0.5, FL + 1, -12.5 }         -- where a player arrives
    local GALLERY = { 0.5, GY + 1, -14.5 }      -- and comes back once the floor is gone
    local KART_AT = { 0, GY + 1, 15 }            -- the kart, on the north gallery, facing the way out
    local RUN_TIME = 60

    -- ---------------------------------------------------------- the players

    type P = { name: string, blue: any?, orange: any?, firing: boolean, next_fire: number,
               driving: boolean, run_left: number, credits: number, torch_at: number, frost_at: number,
               lava_at: number, done: boolean, health: number, grace: number,
               shells: number, bullets: number, dry: boolean }
    local players: { [number]: P } = {}

    local TOOLS = { "lab:portal_gun", "lab:pistol", "lab:shotgun", "lab:rope_gun", "lab:torch", "lab:frost",
                    "lab:chaingun" }
    -- Into the hotbar in this order, after whatever was already there. The
    -- chaingun is found.
    local GIVEN = { "lab:portal_gun", "lab:pistol", "lab:shotgun", "lab:rope_gun", "lab:torch", "lab:frost" }

    -- The base's guns, as yafps_game has them; the pistol never runs dry.
    local GUNS = {
        pistol   = { every = 0.28, pellets = 1, damage = 6, spread = 0.008, range = 80 },
        shotgun  = { every = 0.85, pellets = 9, damage = 4, spread = 0.05, range = 40, ammo = "shells" },
        chaingun = { every = 1 / 12, pellets = 1, damage = 4, spread = 0.025, range = 80, ammo = "bullets" },
    }
    local MAX = { shells = 40, bullets = 300 }

    local function ammo_line(p: number, s: P)
        tell(p, `ammo {s.shells} {s.bullets}`)
    end

    local function valves_open(): number
        local n = 0
        for _, v in VALVES do
            if core.get_block(v[1], v[2], v[3]) == VALVE then n += 1 end
        end
        return n
    end

    -- ----------------------------------------------------------- the portals
    --
    -- The test chambers' gun, as little of it as the fight needs: two white
    -- panels one over the other on a wall, left blue, right orange, a pair
    -- per player. ponytail: a copy of chambers_game's wall shot; no floors,
    -- no cubes. One mod for both wants the gun split out of the chambers.
    local function solid(x: number, y: number, z: number): boolean
        return core.node_solid(core.get_block(x, y, z)) == 1
    end
    local function face_of(ox: number, oy: number, oz: number, dx: number, dy: number, dz: number, t: number,
                           cx: number, cy: number, cz: number): (number, number, number)
        local px, py, pz = ox + dx * t, oy + dy * t, oz + dz * t
        local best, nx, ny, nz = math.huge, 0, 0, 0
        for _, f in { { -1, 0, 0, px - cx }, { 1, 0, 0, px - cx - 1 }, { 0, -1, 0, py - cy }, { 0, 1, 0, py - cy - 1 },
                      { 0, 0, -1, pz - cz }, { 0, 0, 1, pz - cz - 1 } } do
            if f[1] * dx + f[2] * dy + f[3] * dz < 0 and math.abs(f[4]) < best then
                best, nx, ny, nz = math.abs(f[4]), f[1], f[2], f[3]
            end
        end
        return nx, ny, nz
    end
    local function face_byte(nx: number, nz: number): number
        return if nx > 0 then 0 elseif nx < 0 then 1 elseif nz > 0 then 4 else 5
    end
    local function aim(p: number): any?
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then return nil end
        local l = math.sqrt(dx * dx + dy * dy + dz * dz)
        dx, dy, dz = dx / l, dy / l, dz / l
        local ox, oy, oz = x, y + EYE, z
        local cx, cy, cz, t = core.raycast(ox, oy, oz, dx, dy, dz, 80)
        if cx == nil then return nil end
        local nx, ny, nz = face_of(ox, oy, oz, dx, dy, dz, t, cx, cy, cz)
        if ny ~= 0 or (nx == 0 and nz == 0) then return nil end
        local function panel(a: number, b: number, c: number): boolean
            local k = core.get_block(a, b, c)
            return k == PANEL or k == SHELL_LOW or k == SHELL_HIGH
        end
        for _, base in { cy - 1, cy } do
            if panel(cx, base, cz) and panel(cx, base + 1, cz) and not solid(cx + nx, base, cz + nz)
                and not solid(cx + nx, base + 1, cz + nz) then
                return { x = cx + 0.5 + nx * 0.1, y = base + 1.0, z = cz + 0.5 + nz * 0.1,
                         axis = if nz ~= 0 then 0 else 1, sign = nx + nz,
                         cells = { { cx, base, cz, SHELL_LOW }, { cx, base + 1, cz, SHELL_HIGH } },
                         face = face_byte(nx, nz) }
            end
        end
        return nil
    end
    -- Every player's portals are rimmed in their colour on every screen,
    -- as in the chambers: "rim <player> <colour> x y z axis sign", or "-".
    local function rim_told(p: number, colour: string, at: any?)
        if at == nil then
            tell(0, `rim {p} {colour} -`)
        else
            tell(0, string.format("rim %d %s %.2f %.2f %.2f %d %d", p, colour, at.x, at.y, at.z, at.axis, at.sign))
        end
    end
    local function close_shot(sh: any?)
        if sh == nil then return end
        core.portal_close(sh.x, sh.y, sh.z)
        for _, c in sh.cells do core.set_block(c[1], c[2], c[3], PANEL) end
    end
    local function shoot_portal(p: number, colour: string)
        local s = players[p]
        local at = aim(p)
        if s == nil or at == nil then
            tell(p, "fizzle")
            return
        end
        local mine = if colour == "blue" then s.blue else s.orange
        local theirs = if colour == "blue" then s.orange else s.blue
        if theirs and math.abs(theirs.x - at.x) + math.abs(theirs.y - at.y) + math.abs(theirs.z - at.z) < 1.5 then
            tell(p, "fizzle")
            return
        end
        close_shot(mine)
        local idx = core.portal_open(at.x, at.y, at.z, at.axis, at.sign, 0, 0.5, 1.0, if colour == "blue" then 1 else 2)
        if idx < 0 then
            tell(p, "fizzle")
            return
        end
        for _, c in at.cells do
            core.set_block(c[1], c[2], c[3], c[4])
            core.set_facing(c[1], c[2], c[3], at.face)
        end
        at.idx = idx
        if colour == "blue" then s.blue = at else s.orange = at end
        rim_told(p, colour, at)
        if s.blue and s.orange then
            core.portal_link(s.blue.idx, s.orange.idx)
        else
            core.portal_link(idx, -1)
        end
        tell(p, `shot {colour}`)
    end

    -- --------------------------------------------------------------- l'Ospite

    local OSPITE, SHIELD, ORB = "lab:ospite", "lab:ospite_shield", "lab:orb"
    core.ecs.component(OSPITE, { "hp" })
    core.ecs.component(SHIELD, { "hp" })
    core.ecs.component(ORB, { "vx", "vy", "vz", "t" })

    local phase = "wait"     -- wait, scudo, assedio, vetta, fuoco, buio, gelo, corsa
    local phase_at = 0
    local ospite, shield = 0, 0
    local ox, oy, oz = 0.0, FL + 4.0, 6.0      -- where it floats: its feet
    local hp = OSPITE_HP
    local orbs: { [number]: boolean } = {}
    local next_orb, breath_at, visible_until = 0, 0, 0
    local slow_until = 0
    local anybody_was, arrived_at = false, 0

    local function banner(text: string)
        tell(0, `banner {text}`)
    end
    local function bar()
        tell(0, `hp {math.max(0, math.floor(hp))} {OSPITE_HP}`)
    end

    local function spawn_ospite()
        ospite = core.ecs.spawn(OSPITE, ox, oy, oz) or 0
        if ospite ~= 0 then
            core.ecs.visible(ospite, true)
            core.ecs.model(ospite, core.model_id("lab_core:ospite"))
            core.ecs.health(ospite, OSPITE_HP)
            core.ecs.box(ospite, 1.1, 3.2)   -- the whole of it, not its tentacles' tips
        end
        shield = core.ecs.spawn(SHIELD, ox, oy, oz) or 0
        if shield ~= 0 then
            core.ecs.visible(shield, true)
            core.ecs.model(shield, core.model_id("lab_core:ospite_shield"))
        end
    end

    local function set_phase(name: string)
        phase, phase_at = name, clock
        core.log(`lab: phase {name}`)
        local say = {
            scudo = "L'Ospite si leva dalla breccia, chiuso in una gabbia di luce. Due generatori la alimentano, dietro il vetro.",
            assedio = "La gabbia si spegne. Ora le armi lo feriscono.",
            vetta = "Il pavimento cede! Sali sulla galleria: i camini ai lati, o la corda alla trave. Dalla fossa, un percorso: camino a sud, corsa sul muro, camino a est.",
            fuoco = "La lava sale. Nella lava l'Ospite guarisce finche' una valvola resta aperta.",
            buio = "Le luci muoiono e l'Ospite sparisce. Prendi la torcia nella hotbar: col destro, puntata dove senti il respiro, lo mostra per 3 secondi. Solo allora lo colpisci.",
            gelo = "L'ultima forma, veloce. Il cristallo di gelo, col destro quando e' vicino, lo rallenta.",
            corsa = "L'Ospite si dissolve nella breccia. Il laboratorio crolla: al kart sulla galleria nord, e via!",
        }
        if say[name] then
            core.notice(0, say[name])
            banner(string.upper(name))
        end
    end

    -- Its health: the engine's on the entity, which a shot or a blow takes
    -- points off; put back while it may not be hurt, and read otherwise.
    local function guard(hurtable: boolean)
        if ospite == 0 then return end
        if hurtable then
            local h = core.ecs.health(ospite)
            if h ~= nil then hp = h end
        end
        core.ecs.health(ospite, math.max(1, hp))
    end

    -- Somebody just come back from dying is left alone for a few seconds:
    -- it waited by the place they came back to, and killed them again.
    local function nearest_player(): (number?, number, number, number)
        local best, bd, bx, by, bz = nil, math.huge, 0, 0, 0
        for p, s in players do
            if clock < s.grace then continue end
            local x, y, z = core.player_pos(p)
            if x ~= nil then
                local d = (x - ox) ^ 2 + (y - oy) ^ 2 + (z - oz) ^ 2
                if d < bd then best, bd, bx, by, bz = p, d, x, y, z end
            end
        end
        return best, bx, by, bz
    end

    local function throw_orb(tx: number, ty: number, tz: number, speed: number)
        local cy = oy + 2
        local dx, dy, dz = tx - ox, ty + 1 - cy, tz - oz
        local l = math.sqrt(dx * dx + dy * dy + dz * dz)
        if l < 0.1 then return end
        local o = core.ecs.spawn(ORB, ox, cy, oz)
        if o == nil then return end
        core.ecs.set(o, "vx", dx / l * speed)
        core.ecs.set(o, "vy", dy / l * speed)
        core.ecs.set(o, "vz", dz / l * speed)
        core.ecs.set(o, "t", 0)
        core.ecs.visible(o, true)
        core.ecs.model(o, core.model_id("lab_core:ospite_orb"))
        orbs[o] = true
    end

    local function step_orbs(dt: number)
        for o in orbs do
            local x, y, z = core.ecs.get(o, "x"), core.ecs.get(o, "y"), core.ecs.get(o, "z")
            if x == nil then
                orbs[o] = nil
                continue
            end
            local vx, vy, vz = core.ecs.get(o, "vx"), core.ecs.get(o, "vy"), core.ecs.get(o, "vz")
            local t = core.ecs.get(o, "t") + dt
            core.ecs.set(o, "t", t)
            local hit = false
            for p, s in players do
                local px, py, pz = core.player_pos(p)
                if px ~= nil and clock >= s.grace and (px - x) ^ 2 + (py + 1 - y) ^ 2 + (pz - z) ^ 2 < 1.2 then
                    core.damage(p, 3)
                    hit = true
                end
            end
            local l = math.sqrt(vx * vx + vy * vy + vz * vz)
            local _, _, _, wall = core.raycast(x, y, z, vx, vy, vz, l * dt)
            if hit or wall ~= nil or t > 5 then
                orbs[o] = nil
                core.ecs.kill(o)
            else
                core.ecs.set(o, "x", x + vx * dt)
                core.ecs.set(o, "y", y + vy * dt)
                core.ecs.set(o, "z", z + vz * dt)
            end
        end
    end

    -- ------------------------------------------------- what it calls up
    --
    -- The base's imps and ghouls, two at a time out of a burst of its
    -- light, on the floor while there is one and on the gallery after. Each
    -- one leaves what a player needs where it falls: the chaingun the first
    -- time, then shells, bullets or a medkit, or nothing.
    local PICKUP = "lab:pickup"
    core.ecs.component(PICKUP, { "t" })
    local minions: { [number]: { number } } = {}   -- id -> where it last stood
    local pickups: { [number]: { what: string, x: number, y: number, z: number, at: number } } = {}
    local chaingun_out = false
    local next_summon, summon_at = 0, 0
    local PICKUP_MODEL = { shells = "lab_core:ammo_shells", bullets = "lab_core:ammo_bullets",
                           medkit = "lab_core:medkit", chaingun = "lab:chaingun_held" }

    local function drop(what: string, x: number, y: number, z: number)
        local id = core.ecs.spawn(PICKUP, x, y + 0.3, z)
        if id == nil then return end
        core.ecs.visible(id, true)
        core.ecs.model(id, core.model_id(PICKUP_MODEL[what]))
        pickups[id] = { what = what, x = x, y = y, z = z, at = clock }
    end

    local function minion_points(): { { number } }
        if core.get_block(0, FL, 0) == FLOOR then
            return { { -7, FL + 1, -6 }, { 7, FL + 1, -6 }, { -7, FL + 1, 10 }, { 7, FL + 1, 10 } }
        end
        return { { -15, GY + 1, -15 }, { 15, GY + 1, -15 }, { -15, GY + 1, 15 }, { 15, GY + 1, 15 } }
    end

    local function count_minions(): number
        local n = 0
        for _ in minions do n += 1 end
        return n
    end

    local function step_minions()
        for id, at in minions do
            local x, y, z = core.mob_pos(id)
            if x ~= nil then
                at[1], at[2], at[3] = x, y, z
            else
                minions[id] = nil
                local r = math.random()
                if not chaingun_out then
                    chaingun_out = true
                    drop("chaingun", at[1], at[2], at[3])
                elseif r < 0.3 then
                    drop("shells", at[1], at[2], at[3])
                elseif r < 0.55 then
                    drop("bullets", at[1], at[2], at[3])
                elseif r < 0.8 then
                    drop("medkit", at[1], at[2], at[3])
                end
            end
        end
        for id, k in pickups do
            if clock - k.at > 45 then
                core.ecs.kill(id)
                pickups[id] = nil
                continue
            end
            for p, s in players do
                local x, y, z = core.player_pos(p)
                if x ~= nil and (x - k.x) ^ 2 + (z - k.z) ^ 2 < 2.2 and math.abs(y - k.y) < 2 then
                    local took = true
                    if k.what == "medkit" then
                        took = core.health(p) < 20
                        if took then core.heal(p, 8) end
                    elseif k.what == "shells" then
                        took = s.shells < MAX.shells
                        s.shells = math.min(MAX.shells, s.shells + 8)
                    elseif k.what == "bullets" then
                        took = s.bullets < MAX.bullets
                        s.bullets = math.min(MAX.bullets, s.bullets + 60)
                    elseif k.what == "chaingun" then
                        if core.bag_count(p, "lab:chaingun") == 0 then core.give_item(p, "lab:chaingun", 1) end
                        s.bullets = math.min(MAX.bullets, s.bullets + 100)
                        core.notice(p, "La mitragliatrice!")
                    end
                    if took then
                        core.ecs.kill(id)
                        pickups[id] = nil
                        ammo_line(p, s)
                        tell(p, `got {k.what}`)
                        break
                    end
                end
            end
        end
    end

    -- --------------------------------------------------------------- lasers
    --
    -- It turns to somebody, raises its tentacles (its "laser" clip) and aims:
    -- a thin red line from them to where that player stood, held a second --
    -- time to step out of it or behind a wall -- then the beams, which burn
    -- whoever is on that line with nothing solid between.
    local laser = { on = false, t0 = 0, tx = 0, ty = 0, tz = 0, fired = false }
    local next_laser = 0
    local clip_until, clip_now = 0, ""

    local function play_clip(name: string, seconds: number)
        if ospite == 0 then return end
        core.ecs.clip(ospite, name)
        clip_now, clip_until = name, clock + seconds
    end

    local function yaw_to(tx: number, tz: number): number
        return math.atan2(tx - ox, -(tz - oz))
    end

    local function laser_origin(): (number, number, number)
        local yaw = yaw_to(laser.tx, laser.tz)
        return ox + math.sin(yaw) * 1.0, oy + 0.9, oz - math.cos(yaw) * 1.0
    end

    local function step_laser()
        if not laser.on then
            if clock < next_laser or phase == "scudo" or phase == "corsa" or phase == "morte"
                or clip_now == "summon" then return end
            local p, px, py, pz = nearest_player()
            if p == nil then return end
            laser.on, laser.t0, laser.fired = true, clock, false
            laser.tx, laser.ty, laser.tz = px, py + 1.0, pz
            play_clip("laser", 1.8)
            local x0, y0, z0 = laser_origin()
            tell(0, string.format("laser 1 %.2f %.2f %.2f %.2f %.2f %.2f %.2f", x0, y0, z0,
                                  laser.tx, laser.ty, laser.tz, yaw_to(laser.tx, laser.tz)))
            return
        end
        local t = clock - laser.t0
        if t >= 1.05 and not laser.fired then
            laser.fired = true
            local x0, y0, z0 = laser_origin()
            local dx, dy, dz = laser.tx - x0, laser.ty - y0, laser.tz - z0
            local l = math.sqrt(dx * dx + dy * dy + dz * dz)
            dx, dy, dz = dx / l, dy / l, dz / l
            -- On through the aim to the first wall.
            local _, _, _, wall = core.raycast(x0, y0, z0, dx, dy, dz, 60)
            local reach = wall or 60
            tell(0, string.format("laser 2 %.2f %.2f %.2f %.2f %.2f %.2f %.2f", x0, y0, z0,
                                  x0 + dx * reach, y0 + dy * reach, z0 + dz * reach, yaw_to(laser.tx, laser.tz)))
            for p, s in players do
                local px, py, pz = core.player_pos(p)
                if px ~= nil and clock >= s.grace then
                    local vx, vy, vz = px - x0, py + 1.0 - y0, pz - z0
                    local along = vx * dx + vy * dy + vz * dz
                    if along > 0 and along < reach then
                        local ex, ey, ez = vx - dx * along, vy - dy * along, vz - dz * along
                        if ex * ex + ey * ey + ez * ez < 0.9 * 0.9 then
                            core.damage(p, 6)
                            core.notice(p, "Il laser ti ha colpito.")
                        end
                    end
                end
            end
        end
        if t >= 1.5 then
            laser.on = false
            tell(0, "laser 0")
            next_laser = clock + (if phase == "gelo" then 6 else 9)
        end
    end

    -- Where it goes: a drift round the breach, rising to the gallery and
    -- sinking to the floor and back; into the lava while it heals there;
    -- after the nearest player in its last two forms. Still while it aims.
    local function step_ospite(dt: number)
        if ospite == 0 or phase == "morte" then return end
        local swing = 0.5 + 0.5 * math.sin(clock * 0.25)
        local tx, ty, tz = math.sin(clock * 0.3) * 6, FL + 3 + (GY + 4 - (FL + 3)) * swing, 4 + math.cos(clock * 0.3) * 6
        if phase == "fuoco" and valves_open() > 0 then ty = GY - 3 end
        if phase == "buio" or phase == "gelo" then
            local p, px, py, pz = nearest_player()
            if p then tx, ty, tz = px, math.max(py - 1, FL + 1), pz end
        end
        local speed = if phase == "gelo" then 7 else 2.5
        if clock < slow_until then speed *= 0.3 end
        if laser.on or clip_now == "summon" then speed = 0 end
        local dx, dy, dz = tx - ox, ty - oy, tz - oz
        local l = math.sqrt(dx * dx + dy * dy + dz * dz)
        if l > 0.05 and speed > 0 then
            local k = math.min(1, speed * dt / l)
            ox, oy, oz = ox + dx * k, oy + dy * k, oz + dz * k
        end
        core.ecs.set(ospite, "x", ox)
        core.ecs.set(ospite, "y", oy)
        core.ecs.set(ospite, "z", oz)
        -- It faces whoever it aims at, or the nearest player.
        if laser.on then
            core.ecs.set(ospite, "yaw", yaw_to(laser.tx, laser.tz))
        else
            local p, px, _, pz = nearest_player()
            if p then core.ecs.set(ospite, "yaw", yaw_to(px, pz)) end
        end
        if shield ~= 0 then
            core.ecs.set(shield, "x", ox)
            core.ecs.set(shield, "y", oy)
            core.ecs.set(shield, "z", oz)
        end
        -- It strikes whoever it touches, in its last two forms most of all.
        for p in players do
            local px, py, pz = core.player_pos(p)
            if px ~= nil and clock >= players[p].grace and (px - ox) ^ 2 + (pz - oz) ^ 2 < 4 and math.abs(py - oy) < 3 then
                if (players[p].lava_at or 0) < clock then
                    players[p].lava_at = clock + 0.8
                    core.damage(p, if phase == "gelo" then 4 else 2)
                end
            end
        end
        if clip_until > 0 and clock > clip_until then
            clip_until, clip_now = 0, ""
            core.ecs.clip(ospite, "")
        end
        -- Its creatures, every sixteen seconds while fewer than five stand.
        if phase ~= "scudo" and clock >= next_summon and not laser.on and count_minions() < 5 then
            next_summon = clock + 16
            summon_at = clock + 0.6
            play_clip("summon", 1.4)
        end
        if summon_at > 0 and clock >= summon_at then
            summon_at = 0
            local pts = minion_points()
            for _ = 1, 2 do
                local at = pts[math.random(1, #pts)]
                local kind = if math.random() < 0.5 then "lab_core:imp" else "lab_core:ghoul"
                local m = core.spawn_mob(kind, at[1] + 0.5, at[2], at[3] + 0.5)
                if m ~= 0 then
                    minions[m] = { at[1] + 0.5, at[2], at[3] + 0.5 }
                    tell(0, string.format("gate %.2f %.2f %.2f", at[1] + 0.5, at[2], at[3] + 0.5))
                end
            end
        end
        if clock >= next_orb and phase ~= "corsa" and not laser.on then
            local p, px, py, pz = nearest_player()
            if p then
                throw_orb(px, py, pz, if phase == "gelo" then 12 else 8)
                if clip_now == "" then play_clip("shoot", 0.9) end   -- the maw opens as it throws
            end
            next_orb = clock + (if phase == "scudo" then 2.5 elseif phase == "gelo" then 2.5 else 4.0)
        end
        step_laser()
    end

    local function lamps(on: boolean)
        for x = -12, 12, 8 do
            for z = -12, 12, 8 do core.set_block(x, ROOF - 1, z, if on then LAMP else LAMP_OFF) end
            core.set_block(x, GY - 1, -16, if on then LAMP else LAMP_OFF)
            core.set_block(x, GY - 1, 16, if on then LAMP else LAMP_OFF)
        end
    end

    local function fill(y0: number, y1: number, b: number, only: number?)
        for x = -16, 16 do
            for z = -16, 16 do
                for y = y0, y1 do
                    if only == nil or core.get_block(x, y, z) == only then core.set_block(x, y, z, b) end
                end
            end
        end
    end

    local function generators_left(): number
        local n = 0
        for _, sx in { -1, 1 } do
            if core.get_block(sx * 9, FL + 1, 20) == GEN then n += 1 end
        end
        return n
    end

    -- ------------------------------------------------------------- the clicks

    local function gun_of(p: number): (string?, any)
        local held = core.held_item(p)
        local name = string.match(held, "^lab:(%a+)$")
        if name and GUNS[name] then return name, GUNS[name] end
        return nil, nil
    end

    local function fire(p: number, s: P)
        local name, g = gun_of(p)
        if name == nil then
            s.firing = false
            return
        end
        s.next_fire = clock + g.every
        if g.ammo then
            if s[g.ammo] <= 0 then
                if not s.dry then
                    s.dry = true
                    tell(p, "dry")
                end
                return
            end
            s[g.ammo] -= 1
            ammo_line(p, s)
        end
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then return end
        for _ = 1, g.pellets do
            local jx = dx + (math.random() * 2 - 1) * g.spread
            local jy = dy + (math.random() * 2 - 1) * g.spread
            local jz = dz + (math.random() * 2 - 1) * g.spread
            core.shoot(x, y + EYE, z, jx, jy, jz, g.range, g.damage)
        end
        tell(p, `fired {name}`)
    end

    local function click(p: number, what: string)
        local s = players[p]
        if s == nil then return end
        local held = core.held_item(p)
        if what == "left" and held == "lab:portal_gun" then
            shoot_portal(p, "blue")
        elseif what == "right" and held == "lab:portal_gun" then
            shoot_portal(p, "orange")
        elseif what == "fire1" then
            s.firing = gun_of(p) ~= nil
            s.dry = false
        elseif what == "fire0" then
            s.firing = false
        elseif what == "torch" and held == "lab:torch" and clock >= s.torch_at then
            s.torch_at = clock + 2.5
            local tx, ty, tz = core.player_pos(p)
            local lx, ly, lz = core.player_look(p)
            if tx and lx then
                tell(0, string.format("torch %.2f %.2f %.2f %.3f %.3f %.3f", tx, ty + EYE, tz, lx, ly, lz))
            end
            -- It shows for a moment if it is where they look.
            local x, y, z = core.player_pos(p)
            local dx, dy, dz = core.player_look(p)
            if x and dx and ospite ~= 0 then
                local vx, vy, vz = ox - x, oy + 2 - (y + EYE), oz - z
                local l = math.sqrt(vx * vx + vy * vy + vz * vz)
                local dl = math.sqrt(dx * dx + dy * dy + dz * dz)
                if l < 30 and (vx * dx + vy * dy + vz * dz) / (l * dl) > 0.8 then
                    visible_until = clock + 3
                    core.ecs.visible(ospite, true)
                    core.notice(p, "La torcia lo coglie!")
                end
            end
        elseif what == "frost" and held == "lab:frost" and clock >= s.frost_at then
            s.frost_at = clock + 6
            local x, y, z = core.player_pos(p)
            if x and (x - ox) ^ 2 + (z - oz) ^ 2 < 14 * 14 then
                slow_until = clock + 4
                tell(0, string.format("frost %.2f %.2f %.2f", x, y, z))
                core.notice(p, "Il gelo lo rallenta.")
            else
                tell(p, string.format("frost %.2f %.2f %.2f", x or 0, y or 0, z or 0))
            end
        end
    end
    core.event.on("core:net_message", function(ev)
        click(ev.player, ev.text)
    end)
    heard_click = click

    -- Breaking: only the generators. Using: the valves and the kart.
    core.event.on("core:pre_break", function(ev)
        if ev.id ~= GEN then ev.cancel = true end
    end)
    core.event.on("core:pre_place", function(ev) ev.cancel = true end)
    core.event.on("core:broken", function(ev)
        if ev.id == GEN and phase == "scudo" then
            local n = generators_left()
            core.notice(0, if n > 0 then `Un generatore e' spento. Ne resta {n}.` else "L'ultimo generatore si spegne.")
        end
    end)
    core.event.on("core:used", function(ev)
        if ev.id == VALVE then
            ev.cancel = true
            core.set_block(ev.x, ev.y, ev.z, VALVE_SHUT)
            core.notice(0, `Una valvola chiusa. Ne restano {valves_open()}.`)
        elseif ev.id == KART and phase == "corsa" then
            ev.cancel = true
            local s = players[ev.player]
            if s and not s.driving then
                -- The kart is this body now (as track_game's mount): the block
                -- goes, or the body is put inside it and cannot move.
                core.set_block(ev.x, ev.y, ev.z, 0)
                s.driving, s.run_left = true, RUN_TIME
                core.body_drive(ev.player, 12, 30, 2.6, 0.6, 1.7, 2.4)
                core.body_movement(ev.player, 6, 24, 0, 8, -25, 10)
                core.body_model(ev.player, core.model_id("lab_core:kart"))
                core.player_place(ev.player, KART_AT[1] + 0.5, KART_AT[2], KART_AT[3] + 0.5, 0, 0, 1)
                tell(ev.player, "kart 1")
                core.notice(ev.player, `{RUN_TIME} secondi prima che crolli. Fuori, a nord!`)
            end
        elseif ev.id == SEAL or ev.id == GEN or ev.id == LAVA then
            ev.cancel = true
        end
    end)

    -- ----------------------------------------------------------- the phases

    local function reset()
        put_all()
        phase, ospite, shield, hp, orbs = "wait", 0, 0, OSPITE_HP, {}
        for id in pickups do core.ecs.kill(id) end
        pickups, minions, chaingun_out = {}, {}, false
        laser.on, next_laser, next_summon, clip_until, clip_now = false, 0, 0, 0, ""
        ox, oy, oz = 0, FL + 4, 6
        anybody_was = false
        core.log("lab: built again for the next")
    end

    local function off_foot(p: number)
        local s = players[p]
        if s and s.driving then
            s.driving = false
            core.body_drive(p, 0, 0, 0, 0, 0.6, 0)
            core.body_movement(p, 4.8, 8, 0, 8, -25)
            core.body_model(p, core.model_id("lab_core:robot"))
            tell(p, "kart 0")
        end
    end

    local function step_phase(dt: number)
        if phase == "wait" then
            if clock - arrived_at > 6 then
                spawn_ospite()
                next_laser, next_summon = clock + 12, clock + 20
                set_phase("scudo")
            end
        elseif phase == "scudo" then
            guard(false)
            if generators_left() == 0 then
                if shield ~= 0 then core.ecs.kill(shield) end
                shield = 0
                set_phase("assedio")
            end
        elseif phase == "assedio" then
            guard(true)
            if hp <= OSPITE_HP * 0.7 then
                set_phase("vetta")
            end
        elseif phase == "vetta" then
            guard(false)
            if clock - phase_at > 6 and core.get_block(0, FL, 0) == FLOOR then
                fill(FL - 1, FL, 0)            -- the floor gives way
                for _, b in ROUTE do              -- and the way back up out of the pit is there
                    for x = b[1], b[4] do
                        for z = b[3], b[6] do
                            for y = b[2], b[5] do core.set_block(x, y, z, b[7]) end
                        end
                    end
                end
                for p in players do core.player_spawn(p, GALLERY[1], GALLERY[2], GALLERY[3]) end
            end
            -- Everybody up on the gallery, or half a minute: the lava.
            local up = true
            for p in players do
                local _, y = core.player_pos(p)
                if y and y < GY then up = false end
            end
            if clock - phase_at > 8 and (up or clock - phase_at > 30) then
                fill(8, GY - 1, LAVA, 0)
                set_phase("fuoco")
            end
        elseif phase == "fuoco" then
            -- Healing while a valve is open; hurtable once all are shut.
            if valves_open() > 0 then
                hp = math.min(OSPITE_HP, hp + 15 * dt)
                guard(false)
            else
                if core.get_block(0, 8, 0) == LAVA then
                    fill(8, GY - 1, 0, LAVA)
                    core.notice(0, "Le valvole sono chiuse: la lava defluisce.")
                end
                guard(true)
                if hp <= OSPITE_HP * 0.45 then
                    lamps(false)
                    core.ecs.visible(ospite, false)
                    set_phase("buio")
                end
            end
        elseif phase == "buio" then
            guard(true)
            if visible_until > 0 and clock > visible_until then
                visible_until = 0
                core.ecs.visible(ospite, false)
            end
            if clock >= breath_at then
                breath_at = clock + 2
                core.play_sound("lab_core:ospite_breath", ox, oy + 2, oz)
            end
            if hp <= OSPITE_HP * 0.25 then
                lamps(true)
                core.ecs.visible(ospite, true)
                set_phase("gelo")
            end
        elseif phase == "gelo" then
            guard(true)
            if hp <= 1 then
                -- Its death plays out (core.ecs.clip): the tentacles curl up,
                -- the maw falls open, it sinks; then it is gone.
                core.ecs.clip(ospite, "death")
                tell(0, "hp -1 1")   -- its bar goes
                for o in orbs do core.ecs.kill(o) end
                orbs = {}
                if laser.on then
                    laser.on = false
                    tell(0, "laser 0")
                end
                for m, at in minions do core.blast(at[1], at[2] + 1, at[3], 2, 1000) end
                set_phase("morte")
            end
        elseif phase == "morte" then
            guard(false)
            if clock - phase_at > 1.7 then
                core.ecs.kill(ospite)
                ospite = 0
                orbs = {}
                core.set_block(KART_AT[1], KART_AT[2], KART_AT[3], KART)
                core.set_facing(KART_AT[1], KART_AT[2], KART_AT[3], 2)
                for x = -7, 6 do
                    for y = GY + 1, GY + 4 do core.set_block(x, y, 17, 0) end
                end
                for p in players do core.player_spawn(p, KART_AT[1] + 2.5, KART_AT[2], KART_AT[3] - 1.5) end
                set_phase("corsa")
            end
        end
        if phase ~= "wait" and phase ~= "corsa" and phase ~= "morte" then bar() end
    end

    -- The way out, each player on their own: thirty seconds from getting in
    -- to the end of the tunnel; the time out is the roof coming down -- back
    -- by the kart to try again. At the end the credits, then the way home.
    local function step_run(p: number, s: P, dt: number)
        local x, y, z = core.player_pos(p)
        if x == nil then return end
        if s.credits > 0 then
            s.credits -= dt
            if s.credits <= 0 then
                core.body_movement(p, 4.8, 8, 0, 8, -25)
                tell(p, "credits 0")
                core.notice(p, "Il portale di casa e' alla parete ovest. Il sandbox e' tuo.")
            end
            return
        end
        if not s.driving then return end
        s.run_left -= dt
        if x >= END.x0 and x <= END.x0 + 16 and z >= END.z0 and z <= END.z0 + 16 and y >= END.y - 1 then
            off_foot(p)
            s.done, s.credits = true, 45
            core.event.emit("keys:ending", { player = p })
            for _, t in TOOLS do core.take_item(p, t, 9) end
            core.body_movement(p, 0, 0, 0, 0, -25)
            tell(p, "credits 1")
            core.log(`lab: {s.name} is out`)
        elseif s.run_left <= 0 or y < GY - 8 then
            -- Out of time, or off the road into the dark: back to the kart.
            off_foot(p)
            core.set_block(KART_AT[1], KART_AT[2], KART_AT[3], KART)
            core.set_facing(KART_AT[1], KART_AT[2], KART_AT[3], 2)
            core.player_place(p, KART_AT[1] + 2.5, KART_AT[2], KART_AT[3] - 1.5, 0, 0, 1)
            core.notice(p, if s.run_left <= 0 then "Il crollo ti ha raggiunto. Torna al kart e riprova."
                else "Sei caduto nel buio. Torna al kart e riprova.")
        elseif math.floor(s.run_left) % 10 == 0 and math.floor(s.run_left + dt) ~= math.floor(s.run_left) then
            core.notice(p, `{math.floor(s.run_left)} secondi.`)
        end
    end

    local function step_players(dt: number)
        local here: { [number]: boolean } = {}
        for _, p in core.players() do
            here[p] = true
            local s = players[p]
            if s == nil then
                s = { name = core.player_name(p), firing = false, next_fire = 0, driving = false, run_left = 0,
                      credits = 0, torch_at = 0, frost_at = 0, lava_at = 0, done = false, health = 20, grace = 0,
                      shells = 16, bullets = 0, dry = false }
                players[p] = s
                for q, o in players do
                    if o.blue then rim_told(q, "blue", o.blue) end
                    if o.orange then rim_told(q, "orange", o.orange) end
                end
                for _, t in TOOLS do core.take_item(p, t, 9) end
                for _, t in GIVEN do core.give_item(p, t, 1) end
                ammo_line(p, s)
                if phase == "wait" then arrived_at = clock end
                core.player_spawn(p, START[1], START[2], START[3])
                core.notice(p, "Nella hotbar: portal gun, pistola, fucile, corda, torcia, gelo.")
            end
            -- Back from dying: the bar full again where it was low.
            local h = core.health(p)
            if h > s.health + 10 then s.grace = clock + 5 end
            s.health = h
            if s.firing and clock >= s.next_fire then
                fire(p, s)
            end
            local x, y, z = core.player_pos(p)
            if x ~= nil then
                -- Lava burns whoever does not carry the Chiave del Fuoco.
                if core.get_block(math.floor(x), math.floor(y + 0.5), math.floor(z)) == LAVA
                    and core.bag_count(p, "keys:inferno") == 0 and clock >= s.lava_at then
                    s.lava_at = clock + 0.5
                    core.damage(p, 3)
                end
                -- Down in the pit with no floor over it: back on the gallery.
                if y < FL - 2 and phase == "vetta" then
                    core.player_place(p, GALLERY[1], GALLERY[2], GALLERY[3], 0, 0, 1)
                    core.damage(p, 3)
                end
            end
            step_run(p, s, dt)
        end
        for p, s in players do
            if not here[p] then
                local b, o = s.blue, s.orange
                if b then core.portal_close(b.x, b.y, b.z) end
                if o then core.portal_close(o.x, o.y, o.z) end
                rim_told(p, "blue", nil)
                rim_told(p, "orange", nil)
                players[p] = nil
            end
        end
        if next(players) ~= nil then
            anybody_was = true
        elseif anybody_was then
            reset()
        end
    end

    core.event.on("core:world_ready", function() place_pieces() end)

    function server_step(dt: number)
        if #pieces_left > 0 then
            piece_wait -= dt
            if piece_wait <= 0 then
                piece_wait = 2
                place_pieces()
            end
        end
        step_players(dt)
        if next(players) == nil then return end
        step_phase(dt)
        step_ospite(dt)
        step_orbs(dt)
        step_minions()
    end
end

-- ===================================================== the player's machine

if drawing then
    local hp, hp_max = -1, OSPITE_HP
    local banner_text, banner_left = "", 0
    local credits_t = -1
    local frost: { { number } } = {}
    local driving = false
    local shells, bullets = -1, -1
    local flash, flash_left = "", 0
    local beam = { stage = 0, x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0, yaw = 0, left = 0 }
    local gates: { { number } } = {}
    local torches: { { number } } = {}   -- x y z, look, time left: a beam of light
    local rims: { [string]: { number } } = {}   -- x y z axis sign blue, by "player colour"

    heard = function(text: string)
        local w = string.split(text, " ")
        if w[1] == "rim" then
            if w[4] == "-" then
                rims[`{w[2]} {w[3]}`] = nil
            else
                local v = {}
                for i = 4, 8 do v[i - 3] = tonumber(w[i]) or 0 end
                v[6] = if w[3] == "blue" then 1 else 0
                rims[`{w[2]} {w[3]}`] = v
            end
        elseif w[1] == "hp" then
            hp, hp_max = tonumber(w[2]) or hp, tonumber(w[3]) or hp_max
        elseif w[1] == "banner" then
            banner_text, banner_left = table.concat(w, " ", 2), 3
        elseif w[1] == "fired" then
            core.arms_clip(`fire_{w[2]}`)
        elseif w[1] == "ammo" then
            shells, bullets = tonumber(w[2]) or 0, tonumber(w[3]) or 0
        elseif w[1] == "dry" then
            flash, flash_left = "VUOTO", 1
        elseif w[1] == "got" then
            local names = { shells = "CARTUCCE", bullets = "PROIETTILI", medkit = "KIT MEDICO",
                            chaingun = "MITRAGLIATRICE" }
            flash, flash_left = names[w[2]] or w[2], 1.5
        elseif w[1] == "laser" then
            beam.stage = tonumber(w[2]) or 0
            if beam.stage > 0 then
                beam.x0, beam.y0, beam.z0 = tonumber(w[3]) or 0, tonumber(w[4]) or 0, tonumber(w[5]) or 0
                beam.x1, beam.y1, beam.z1 = tonumber(w[6]) or 0, tonumber(w[7]) or 0, tonumber(w[8]) or 0
                beam.yaw = tonumber(w[9]) or 0
                beam.left = if beam.stage == 2 then 0.45 else 1.2
            end
        elseif w[1] == "torch" then
            local t = {}
            for i = 2, 7 do t[i - 1] = tonumber(w[i]) or 0 end
            t[7] = 0.5
            table.insert(torches, t)
        elseif w[1] == "gate" then
            table.insert(gates, { tonumber(w[2]) or 0, tonumber(w[3]) or 0, tonumber(w[4]) or 0, 0.7 })
        elseif w[1] == "credits" then
            credits_t = if w[2] == "1" then 0 else -1
        elseif w[1] == "frost" then
            table.insert(frost, { tonumber(w[2]) or 0, tonumber(w[3]) or 0, tonumber(w[4]) or 0, 0.6 })
        elseif w[1] == "kart" then
            driving = w[2] == "1"
            core.view(if driving then 5 else 0)
            core.body_model(0, if driving then core.model_id("lab_core:kart") else 0)
        end
    end
    if not serving then
        core.event.on("core:net_message", function(ev) heard(ev.text) end)
    end

    local function ask(what: string, player: number)
        if serving then
            (heard_click :: any)(player, what)
        else
            core.net.send(0, what)
        end
    end

    -- The buttons are the tool's in the hand: the gun's two colours, the
    -- shotgun's trigger held, the rope's hook and letting go, the keys' torch
    -- and frost. Anything else -- breaking a generator, shutting a valve,
    -- getting into the kart -- is the engine's.
    core.event.on("core:input", function(ev)
        local held = core.held_item(1)
        if held == "lab:portal_gun" and (ev.action == "mouse_left" or ev.action == "mouse_right") then
            ev.cancel = true
            ask(if ev.action == "mouse_left" then "left" else "right", ev.player)
        elseif held == "lab:shotgun" or held == "lab:pistol" or held == "lab:chaingun" then
            if ev.action == "mouse_left" then
                ev.cancel = true
                ask("fire1", ev.player)
            elseif ev.action == "mouse_left_up" then
                ask("fire0", ev.player)
            end
        elseif held == "lab:rope_gun" then
            if ev.action == "mouse_right" then
                ev.cancel = true
                core.rope(true)
            elseif ev.action == "mouse_left" then
                ev.cancel = true
                core.rope(false)
            end
        elseif held == "lab:torch" and ev.action == "mouse_right" then
            ev.cancel = true
            ask("torch", ev.player)
        elseif held == "lab:frost" and ev.action == "mouse_right" then
            ev.cancel = true
            ask("frost", ev.player)
        end
    end)

    -- Each portal's rim on the wall's face, 0.4 out from the portal: four
    -- bars round its 1 x 2 opening, as chambers_game draws them.
    core.render.pass("lab:rims", 100, function()
        for _, v in rims do
            local x, y, z, axis, sign = v[1], v[2], v[3], v[4], v[5]
            local r, g, b = if v[6] == 1 then 40 else 255, if v[6] == 1 then 130 else 140, if v[6] == 1 then 255 else 20
            local t, hw, hh = 0.08, 0.5, 1.0
            if axis == 0 then
                local pz = z + sign * 0.42
                core.render.box(x - hw, y, pz, t, 2 * hh + t, t, r, g, b, 255)
                core.render.box(x + hw, y, pz, t, 2 * hh + t, t, r, g, b, 255)
                core.render.box(x, y - hh, pz, 2 * hw + t, t, t, r, g, b, 255)
                core.render.box(x, y + hh, pz, 2 * hw + t, t, t, r, g, b, 255)
            else
                local px = x + sign * 0.42
                core.render.box(px, y, z - hw, t, 2 * hh + t, t, r, g, b, 255)
                core.render.box(px, y, z + hw, t, 2 * hh + t, t, r, g, b, 255)
                core.render.box(px, y - hh, z, t, t, 2 * hw + t, r, g, b, 255)
                core.render.box(px, y + hh, z, t, t, 2 * hw + t, r, g, b, 255)
            end
        end
    end)

    -- The lasers: three beams from the raised tentacles, converging on where
    -- it aims -- thin dashes while it aims, thick and bright as it fires.
    core.render.pass("lab:laser", 100, function()
        if beam.stage == 0 then return end
        local rx, rz = math.cos(beam.yaw), math.sin(beam.yaw)
        for _, side in { -0.7, 0, 0.7 } do
            local x0, y0, z0 = beam.x0 + rx * side, beam.y0 - math.abs(side) * 0.3, beam.z0 + rz * side
            local dx, dy, dz = beam.x1 - x0, beam.y1 - y0, beam.z1 - z0
            local l = math.sqrt(dx * dx + dy * dy + dz * dz)
            if l > 0 then
                local thick = beam.stage == 2
                local step = if thick then 0.25 else 0.6
                local size = if thick then 0.22 else 0.05
                local d = 0.0
                while d < l do
                    local f = d / l
                    core.render.box(x0 + dx * f, y0 + dy * f, z0 + dz * f, size, size, size,
                                    255, if thick then 80 else 30, if thick then 200 else 40, 255)
                    d += step
                end
            end
        end
    end)

    -- The torch: a cone of pale light along the look, widening, a moment.
    core.render.pass("lab:torch", 100, function()
        for _, t in torches do
            local a = math.floor(200 * t[7] / 0.5)
            -- From a few nodes out: closer, it filled the screen.
            for k = 3, 26 do
                local d = k * 1.2
                local r = d * 0.15
                local size = 0.04 + d * 0.003
                for j = 0, 5 do
                    local ang = j / 6 * 2 * math.pi + k
                    local ux, uz = math.cos(ang) * r, math.sin(ang) * r
                    core.render.box(t[1] + t[4] * d + ux, t[2] + t[5] * d + uz * 0.5, t[3] + t[6] * d + uz,
                                    size, size, size, 255, 245, 190, a)
                end
            end
        end
    end)

    -- Where it calls a creature up: a violet ring spreading on the floor.
    core.render.pass("lab:gates", 100, function()
        for _, g in gates do
            local r = (0.7 - g[4]) * 3 + 0.3
            for k = 0, 15 do
                local a = k / 16 * 2 * math.pi
                core.render.box(g[1] + math.cos(a) * r, g[2] + 0.2, g[3] + math.sin(a) * r, 0.18, 0.18, 0.18, 190, 70, 255, 255)
            end
        end
    end)

    -- The frost: a ring of pale blue round where it was cast.
    core.render.pass("lab:frost", 100, function()
        for _, f in frost do
            local r = (0.6 - f[4]) * 20
            for k = 0, 23 do
                local a = k / 24 * 2 * math.pi
                core.render.box(f[1] + math.cos(a) * r, f[2] + 0.5, f[3] + math.sin(a) * r, 0.3, 0.3, 0.3, 170, 220, 255, 255)
            end
        end
    end)

    local CREDITS = {
        "CUBES AND PORTALS",
        "",
        "La breccia e' chiusa.",
        "L'Ospite non tornera'.",
        "",
        "L'ingegnere non e' mai tornato.",
        "Il suo laboratorio resta, in rovina, dietro l'ottavo portale.",
        "",
        "Un gioco di Giacomo Perin",
        "",
        "Motore, mondi, creature e prove:",
        "Giacomo Perin, con Claude",
        "",
        "Modelli: Blender",
        "Suoni: ElevenLabs e sintesi",
        "Fisica dei kart: Jolt Physics",
        "Grafica: raylib    Scripting: Luau",
        "Rete: ENet, zstd, Monocypher    Salvataggi: SQLite    ECS: Flecs",
        "",
        "Grazie per aver giocato.",
        "",
        "Il sandbox e' tuo.",
    }

    core.hud.pass("lab:hud", 100, function()
        if credits_t >= 0 then
            core.hud.rect(0.5, 0.5, 4.0, 2.0, 0, 0, 0, 235)   -- over the whole screen, any shape
            for i, line in CREDITS do
                local y = 1.05 + i * 0.06 - credits_t * 0.045
                if y > -0.05 and y < 1.05 then
                    core.hud.text(0.5, y, line, if i == 1 then 0.06 else 0.035)
                end
            end
            return
        end
        core.hud.rect(0.5, 0.5, 0.003, 0.003, 240, 240, 240, 220)
        if hp >= 0 then
            local f = math.max(0, hp / hp_max)
            core.hud.rect(0.5, 0.06, 0.5, 0.025, 30, 20, 40, 220)
            core.hud.rect(0.25 + 0.25 * f, 0.06, 0.5 * f, 0.02, 170, 60, 230, 255)
            core.hud.text(0.5, 0.1, "L'OSPITE", 0.03)
        end
        if banner_left > 0 then
            core.hud.text(0.5, 0.3, banner_text, 0.06)
        end
        if shells >= 0 then
            core.hud.text(0.85, 0.88, `CARTUCCE {shells}   PROIETTILI {bullets}`, 0.025)
        end
        if flash_left > 0 then
            core.hud.text(0.5, 0.62, flash, 0.03)
        end
    end)

    function client_step(dt: number)
        if banner_left > 0 then banner_left -= dt end
        if credits_t >= 0 then credits_t += dt end
        if flash_left > 0 then flash_left -= dt end
        if beam.stage > 0 then
            beam.left -= dt
            if beam.left <= 0 then beam.stage = 0 end
        end
        for i = #torches, 1, -1 do
            torches[i][7] -= dt
            if torches[i][7] <= 0 then table.remove(torches, i) end
        end
        for i = #gates, 1, -1 do
            gates[i][4] -= dt
            if gates[i][4] <= 0 then table.remove(gates, i) end
        end
        for i = #frost, 1, -1 do
            frost[i][4] -= dt
            if frost[i][4] <= 0 then table.remove(frost, i) end
        end
    end
end

function on_step(dt: number)
    clock += dt
    if serving then server_step(dt) end
    if drawing then client_step(dt) end
end
