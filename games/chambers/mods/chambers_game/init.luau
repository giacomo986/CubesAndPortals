--!strict
-- chambers_game: the test chambers, built and working, in the manner of
-- classic portal puzzle games. A lobby a player arrives in from the sandbox, six chambers in a row
-- and a last room whose portal leads home:
--
--   01  two fixed portals, blue and orange, into a closed room with the exit
--   02  a cube out of a vent, onto the floor button that holds the door open
--   03  the gun that shoots blue, on a pedestal; a chasm, a fixed orange
--       portal on its far side
--   04  the whole gun, blue and orange; a ledge with the button on it, a cube
--       to carry up there through a portal in the floor and one in the wall
--   05  launch plates over a pit
--   06  a push button that opens the door for twelve seconds, a clock over it
--   07  the cube shut behind glass: portals in and out with it, then up onto
--       a ledge with the button, as in 04
--   08  two buttons and two cubes, one button across a pit
--   09  the launch plates again, then a pit only a portal crosses
--   10  a push button and a pit: portals shot first, ten seconds to get through
--   11  the long fall: a portal at the bottom of a deep pit and one in the
--       floor by a high ledge -- jump in, come out going up as fast as you
--       fell, land on the ledge; a target there opens the door
--   12  three sentry turrets: knocked over by a thrown cube, or picked up from
--       behind (a portal behind them helps); the door opens when all are down
--   13  a dissolving field: crossing it closes your portals and dissolves what
--       you carry, so the cube goes round it through a portal; then the long
--       fall again, cube in hand, up to the button on a ledge
--   14  the steps: two walls four high across the room, out of reach from
--       the floor -- a cube carried to the foot of each and stood on is the
--       way up (two vents at the foot of each wall, one to spare); a stair
--       down to the door
--   15  three cubes: a wall four and a half high, past reach from one cube
--       (4.10) and in reach from two stacked (4.82) -- and two stacked are
--       too high to jump onto (1.44 against 1.28), so the third is a step
--       up to them; three vents, a stair down to the door
--   16  a light bridge: its emitter lays one across the near floor into the
--       north wall; a portal there and one in the west wall lay it over the
--       pit instead
--   17  gels: blue poured where it lands by the door, unless a portal under
--       the stream and one in the ceiling over the foot of a wall four high
--       pour it there to be thrown over; then orange before a pit seven wide,
--       a run on it that jumps it
--
-- Trigger zones (TRIGGERS): a box that, when a body comes into it, does what
-- it lists -- opens a door, opens a linked pair of portals, wakes a room's
-- turrets, says a line. Fields (FIELDS) and turrets (TURRETS) likewise are
-- data; a survival site's copy of the row gets its own of each.
--
-- The last room gives the Chiave della Prova (mods/keys) to whoever reaches it.
--
-- The SERVER decides every portal and every cube: the half on the player's
-- machine says which button went down and draws the crosshair and the clock.
-- Portals go only on white panels (chambers_core:panel), with room in front:
-- a wall's is one wide and two high, a floor's or a ceiling's one by one
-- (core.portal_open, the engine's own shapes). Left is blue, right orange;
-- right with no orange to shoot -- or at a cube -- picks a cube up, and either
-- button drops one carried (left throws it). A cube is a worker creature
-- (core.spawn_mob) and goes through portals as a body does, carried or thrown.
--
-- `side = both`: the server runs the first half, a player's machine the
-- second, and a process that is its own server runs both.

local Devices = require("@self/devices")

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local FL  = 20    -- the floor a body stands on in every chamber
local EYE = 1.5   -- chambers_core's eye height

local clock = 0.0

local heard: (string) -> () = function(_) end
local heard_click: (number, string) -> () = function(_, _) end
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

    core.register_item("chambers_game:gun_blue", 1, "", 0, 1.0, "")
    core.register_item("chambers_game:gun", 1, "", 0, 1.0, "")
    core.register_model("chambers_game:gun_blue_held", "gun_blue_held.gltf")
    core.register_model("chambers_game:gun_held", "gun_held.gltf")
    local CUBE_MODEL = core.register_model("chambers_game:cube", "cube.gltf")
    -- A storage cube: a worker that never walks, only falls, is carried and is
    -- thrown. Nothing comes out of the dark.
    core.register_mob("chambers_game:cube", CUBE_MODEL, 1000, 0, "worker", 0, 0, 1, "", 0, 0, false, 0, false,
                      "chambers_game:never")
    -- The sentry turret, standing and knocked over (tools/make_chambers_turret.py):
    -- workers that never walk; what one sees and shoots is this mod's.
    core.register_mob("chambers_game:turret", core.register_model("chambers_game:turret", "turret.gltf"), 1000, 0,
                      "worker", 0, 0, 1, "", 0, 0, false, 0, false, "chambers_game:never")
    core.register_mob("chambers_game:turret_down", core.register_model("chambers_game:turret_down", "turret_down.gltf"),
                      1000, 0, "worker", 0, 0, 1, "", 0, 0, false, 0, false, "chambers_game:never")

    local function id(name: string): number
        return core.node_id(`chambers_core:{name}`)
    end
    local PANEL, METAL, FLOOR, LIGHT, GLASS = id("panel"), id("metal"), id("floor"), id("light"), id("glass")
    local SLAB = id("metal_slab")
    local DOOR, VENT, CLOCK, EXIT = id("door"), id("vent"), id("clock"), id("exit")
    local DOOR_OPEN, DOOR_PART, DOOR_OPEN_PART = id("door_open"), id("door_part"), id("door_open_part")
    local BUTTON, DOWN, PLATE, PUSH, PEDESTAL = id("button"), id("button_down"), id("plate"), id("pushbutton"),
        id("pedestal")
    local RIM_BLUE, RIM_ORANGE = id("rim_blue"), id("rim_orange")
    local PLATE_PART, BUTTON_PART = id("plate_part"), id("button_part")
    -- A door's lower-left cell from shut to open (chambers_core's door_1..4
    -- between): a block's model does not move, so a door is stepped through.
    local DOOR_FRAMES = { DOOR, id("door_1"), id("door_2"), id("door_3"), id("door_4"), DOOR_OPEN }
    local SHELL_LOW, SHELL_HIGH, SHELL_ONE = id("shell_low"), id("shell_high"), id("shell_one")
    local SHELL_FLAT = id("shell_flat")
    local EMITTER, TARGET = id("emitter"), id("target")
    local BRIDGE_EMITTER, DISPENSE_BOUNCE, DISPENSE_SPEED = id("bridge_emitter"), id("dispenser_bounce"),
        id("dispenser_speed")
    -- A face's index as a wallmounted cell's byte means it: +X -X +Y -Y +Z -Z.
    local function face_byte(nx: number, ny: number, nz: number): number
        return if nx > 0 then 0 elseif nx < 0 then 1 elseif ny > 0 then 2 elseif ny < 0 then 3
            elseif nz > 0 then 4 else 5
    end

    -- ---------------------------------------------------------- the layout

    -- Every room is z 0..11 inside, its floor at FL - 1, along x; a room's
    -- exit is a door two wide (z 5..6) in its east wall, a corridor of one
    -- cell, and the next room's doorway.
    type Room = { x0: number, x1: number, h: number, name: string }
    local ROOMS: { Room } = {
        { x0 = 0, x1 = 9, h = 5, name = "" },                                     -- the lobby
        { x0 = 13, x1 = 26, h = 8, name = "Test chamber 01: portals" },
        { x0 = 30, x1 = 45, h = 8, name = "Test chamber 02: the storage cube" },
        { x0 = 49, x1 = 66, h = 8, name = "Test chamber 03: the portal gun (blue)" },
        { x0 = 70, x1 = 87, h = 9, name = "Test chamber 04: the portal gun (blue and orange)" },
        { x0 = 91, x1 = 112, h = 10, name = "Test chamber 05: launch plates" },
        { x0 = 116, x1 = 133, h = 8, name = "Test chamber 06: against the clock" },
        { x0 = 137, x1 = 156, h = 9, name = "Test chamber 07: behind the glass" },
        { x0 = 160, x1 = 179, h = 8, name = "Test chamber 08: two buttons" },
        { x0 = 183, x1 = 214, h = 10, name = "Test chamber 09: over the pit" },
        { x0 = 218, x1 = 241, h = 8, name = "Test chamber 10: the shortcut" },
        { x0 = 245, x1 = 266, h = 14, name = "Test chamber 11: the long fall" },
        { x0 = 270, x1 = 293, h = 8, name = "Test chamber 12: the sentries" },
        { x0 = 297, x1 = 320, h = 14, name = "Test chamber 13: the dissolving field" },
        { x0 = 324, x1 = 355, h = 14, name = "Test chamber 14: the steps" },
        { x0 = 359, x1 = 380, h = 12, name = "Test chamber 15: three cubes" },
        { x0 = 384, x1 = 407, h = 8, name = "Test chamber 16: the light bridge" },
        { x0 = 411, x1 = 436, h = 12, name = "Test chamber 17: gels" },
        { x0 = 440, x1 = 447, h = 5,
          name = "Testing complete. The portal home is on the west wall; the button, more chambers." },
    }
    local DZ0, DZ1 = 5, 6   -- the doors' cells across

    -- The world's two openings (tools/chambers.sh starts it with --portals):
    -- opening 0 in the lobby's south wall, facing +Z -- a player who comes in
    -- from the sandbox stands four in front of it -- and opening 1 in the last
    -- room's west wall, facing +X, the way home.
    core.set_portal(0, 4.5, FL + 1, 0.5)
    -- Set into the wall as the fixed pairs are (recess, below): its plane
    -- nine tenths in, the wall's two cells hollowed round it.
    core.set_portal(1, ROOMS[#ROOMS].x0 - 0.4, FL + 1, 9.5)

    -- Inside a mapgen stage only the column being made is written (`making`),
    -- so the chambers are lit with it; outside one, a door, anywhere.
    local making: MapgenStage? = nil
    -- Along x, where build() writes: 0 for the chambers, a site's copy's
    -- start for its copy (CH0 + slot * CHW, below).
    local OFF = 0
    -- And whether it is mirrored across the room, z to Z1 - z (a site's
    -- chamber may be): the doors in the middle stay where they are, a wall's
    -- things go to the other long wall.
    local MIR = false
    local Z1 = 11
    local function put(x: number, y: number, z: number, b: number)
        x += OFF
        if MIR then z = Z1 - z end
        local m = making
        if m == nil or (x >= m.min_x and x <= m.max_x and z >= m.min_z and z <= m.max_z) then
            core.set_block(x, y, z, b)
        end
    end
    -- A block that turns, with its turn. In a stage, in the column being
    -- made, at once. Otherwise both are written and the cell is looked at
    -- again every tick until it reads as that block turned that way: a
    -- column not taking writes yet holds them back, and a turn applied before
    -- its block is refused -- a shell left facing the wrong way stood in
    -- front of its portal in a world built again at its start.
    local turned_later: { { number } } = {}
    local function put_turned(x: number, y: number, z: number, b: number, f: number)
        x += OFF
        if MIR then
            z = Z1 - z
            -- Facing +Z is facing -Z (wallmounted 4 and 5); a door is never mirrored.
            f = if f == 4 then 5 elseif f == 5 then 4 else f
        end
        local m = making
        if m ~= nil then
            if x >= m.min_x and x <= m.max_x and z >= m.min_z and z <= m.max_z then
                core.set_block(x, y, z, b)
                core.set_facing(x, y, z, f)
            end
            return
        end
        core.set_block(x, y, z, b)
        core.set_facing(x, y, z, f)
        table.insert(turned_later, { x, y, z, b, f })
    end
    function step_turned()
        local left = {}
        for _, w in turned_later do
            if core.get_block(w[1], w[2], w[3]) ~= w[4] then
                core.set_block(w[1], w[2], w[3], w[4])
                table.insert(left, w)
            elseif core.get_facing(w[1], w[2], w[3]) ~= w[5] then
                core.set_facing(w[1], w[2], w[3], w[5])
                table.insert(left, w)
            end
        end
        turned_later = left
    end

    local function fill(x0: number, x1: number, y0: number, y1: number, z0: number, z1: number, b: number)
        x0, x1 = x0 + OFF, x1 + OFF
        if MIR then z0, z1 = Z1 - z1, Z1 - z0 end
        local m = making
        if m ~= nil then
            x0 = math.max(x0, m.min_x)
            x1 = math.min(x1, m.max_x)
            z0 = math.max(z0, m.min_z)
            z1 = math.min(z1, m.max_z)
        end
        for x = x0, x1 do
            for y = y0, y1 do
                for z = z0, z1 do
                    core.set_block(x, y, z, b)
                end
            end
        end
    end

    -- A room: floor, ceiling with lights, metal walls. Its doors are cut after.
    local function room(r: Room)
        fill(r.x0 - 1, r.x1 + 1, FL - 1, FL - 1, -1, Z1 + 1, FLOOR)
        fill(r.x0 - 1, r.x1 + 1, FL + r.h, FL + r.h, -1, Z1 + 1, METAL)
        for x = r.x0 + 1, r.x1 - 1, 3 do
            fill(x, x, FL + r.h, FL + r.h, 1, Z1 - 1, LIGHT)
        end
        fill(r.x0 - 1, r.x0 - 1, FL, FL + r.h - 1, -1, Z1 + 1, METAL)
        fill(r.x1 + 1, r.x1 + 1, FL, FL + r.h - 1, -1, Z1 + 1, METAL)
        fill(r.x0, r.x1, FL, FL + r.h - 1, -1, -1, METAL)
        fill(r.x0, r.x1, FL, FL + r.h - 1, Z1 + 1, Z1 + 1, METAL)
        fill(r.x0, r.x1, FL, FL + r.h - 1, 0, Z1, 0)
        -- And a strip of light along the top of each long wall.
        for x = r.x0, r.x1, 2 do
            put(x, FL + r.h - 1, -1, LIGHT)
            put(x, FL + r.h - 1, Z1 + 1, LIGHT)
        end
    end

    -- A pit across a room from x0 to x1: eight deep, its bottom metal.
    local function pit(x0: number, x1: number, z0: number?, z1: number?)
        fill(x0, x1, FL - 9, FL - 1, z0 or 0, z1 or Z1, 0)
        fill(x0, x1, FL - 10, FL - 10, z0 or 0, z1 or Z1, METAL)
        fill(x0, x1, FL - 9, FL - 1, -1, -1, METAL)
        fill(x0, x1, FL - 9, FL - 1, Z1 + 1, Z1 + 1, METAL)
    end

    -- The doors: cells, open or shut. A shut door is its block, an open one air.
    -- A door of a level of zones (below) says its own cells, the one wearing
    -- the model first, the way they face, its middle, and whether it opens as
    -- somebody comes up to it; the chambers' doors stand at DZ0 on the floor.
    type Door = { x: number, open: boolean, want: boolean, frame: number, next: number,
                  cells: { { number } }?, f: number?, mid: { number }?, near: boolean? }
    local doors: { [number]: Door } = {}   -- by the room it lets out of
    -- A door's four cells, shut or open: the lower one at DZ0 wears the
    -- model, turned so its two cells run along +Z (upright 3).
    local function door_cells(x: number, open: boolean)
        for dy = 0, 1 do
            for dz = 0, 1 do
                local b = if dy == 0 and dz == 0 then (if open then DOOR_OPEN else DOOR)
                    else (if open then DOOR_OPEN_PART else DOOR_PART)
                put_turned(x, FL + dy, DZ0 + dz, b, 3)
            end
        end
    end
    -- Opened and shut a frame at a time, 0.06 s each: the leaves slide into
    -- the wall and out (step_door_frames). Shut to a body until open.
    local function set_door(k: number, open: boolean)
        local d = doors[k]
        if d ~= nil then
            d.open = open
        end
    end
    function step_door_frames()
        for _, d in doors do
            local want = if d.open then #DOOR_FRAMES - 1 else 0
            if d.frame ~= want and clock >= d.next then
                local was_open = d.frame == #DOOR_FRAMES - 1
                d.frame += if want > d.frame then 1 else -1
                d.next = clock + 0.06
                local open = d.frame == #DOOR_FRAMES - 1
                local cells, f = d.cells, d.f or 3
                if cells ~= nil then
                    local c = cells[1]
                    put_turned(c[1], c[2], c[3], DOOR_FRAMES[d.frame + 1], f)
                    if open ~= was_open then
                        for i = 2, #cells do
                            put_turned(cells[i][1], cells[i][2], cells[i][3], if open then DOOR_OPEN_PART else DOOR_PART, f)
                        end
                    end
                    continue
                end
                put_turned(d.x, FL, DZ0, DOOR_FRAMES[d.frame + 1], 3)
                if open ~= was_open then
                    for dy = 0, 1 do
                        for dz = 0, 1 do
                            if dy + dz > 0 then
                                put_turned(d.x, FL + dy, DZ0 + dz, if open then DOOR_OPEN_PART else DOOR_PART, 3)
                            end
                        end
                    end
                end
            end
        end
    end

    -- The fixed portals: where, and once open which opening they are.
    type Fixed = { x: number, y: number, z: number, axis: number, sign: number, idx: number, pair: string? }
    local fixed: { [string]: Fixed } = {
        -- 01: blue by the doorway in the south wall, orange in the closed room.
        c1_blue = { x = 16.5, y = FL + 1, z = -0.4, axis = 0, sign = 1, idx = -1, pair = "c1_orange" },
        c1_orange = { x = 24.5, y = FL + 1, z = -0.4, axis = 0, sign = 1, idx = -1, pair = "c1_blue" },
        -- 03: orange past the chasm, in the north wall; the blue is the player's.
        c3_orange = { x = 64.5, y = FL + 1, z = 12.4, axis = 0, sign = -1, idx = -1 },
    }

    -- A coloured frame round a fixed portal's two cells, in the wall at
    -- (x, z), the portal's bottom at y; along x or along z.
    local function rim(x: number, z: number, y: number, along_x: boolean, b: number)
        for dy = -1, 2 do
            for d = -1, 1, 2 do
                put(if along_x then x + d else x, y + dy, if along_x then z else z + d, b)
            end
        end
        put(x, y + 2, z, b)
        put(x, y - 1, z, b)
    end

    -- A fixed portal's two wall cells, hollowed as a gun's are; `face` the
    -- wallmounted byte of the way it faces (0 is +X, 4 is +Z, 5 is -Z).
    local function recess(x: number, z: number, y: number, face: number)
        for k, b in { SHELL_LOW, SHELL_HIGH } do
            put_turned(x, y + k - 1, z, b, face)
        end
    end

    -- The buttons and what they hold open; the vents and their cubes; the
    -- plates and how hard they throw; the pedestals; the push button.
    type Button = { x: number, y: number, z: number, door: number, pressed: boolean }
    local buttons: { Button } = {
        { x = 40, y = FL, z = 8, door = 3, pressed = false },       -- 02
        { x = 85, y = FL + 4, z = 9, door = 5, pressed = false },   -- 04, on the ledge
        { x = 154, y = FL + 4, z = 9, door = 8, pressed = false },  -- 07, on the ledge
        { x = 167, y = FL, z = 9, door = 9, pressed = false },      -- 08, both of them
        { x = 177, y = FL, z = 3, door = 9, pressed = false },
        { x = 317, y = FL + 6, z = 1, door = 14, pressed = false },   -- 13, on the ledge
    }
    -- `low`: under it a cube has fallen and comes again (FL - 4 in the chambers).
    type Vent = { x: number, z: number, top: number, room: number, cube: number, low: number? }
    local vents: { Vent } = {
        { x = 33, z = 3, top = FL + 7, room = 3, cube = 0 },
        { x = 74, z = 9, top = FL + 8, room = 5, cube = 0 },
        { x = 140, z = 1, top = FL + 8, room = 8, cube = 0 },   -- 07, behind the glass
        { x = 162, z = 2, top = FL + 7, room = 9, cube = 0 },   -- 08, two
        { x = 164, z = 9, top = FL + 7, room = 9, cube = 0 },
        { x = 271, z = 9, top = FL + 7, room = 13, cube = 0 },   -- 12
        { x = 298, z = 9, top = FL + 13, room = 14, cube = 0 },  -- 13, before the field
        { x = 327, z = 3, top = FL + 13, room = 15, cube = 0 },  -- 14, two at the foot of each wall
        { x = 327, z = 8, top = FL + 13, room = 15, cube = 0 },
        { x = 333, z = 3, top = FL + 13, room = 15, cube = 0 },
        { x = 333, z = 8, top = FL + 13, room = 15, cube = 0 },
        { x = 361, z = 2, top = FL + 11, room = 16, cube = 0 },  -- 15, three
        { x = 363, z = 9, top = FL + 11, room = 16, cube = 0 },
        { x = 366, z = 4, top = FL + 11, room = 16, cube = 0 },
    }
    -- `y`: the height a body stands at on it (FL in the chambers).
    type Plate = { x: number, z: number, vx: number, vy: number, vz: number, y: number? }
    local plates: { Plate } = {
        { x = 93, z = 5, vx = 8, vy = 12, vz = 0 },
        { x = 102, z = 5, vx = 8, vy = 12, vz = 0 },
        { x = 185, z = 5, vx = 8, vy = 12, vz = 0 },   -- 09, as 05
        { x = 194, z = 5, vx = 8, vy = 12, vz = 0 },
    }
    -- A pedestal's gun is level 1 (blue) or 2 (both); a player's level is
    -- kept by name, so a gun is given once.
    type Pedestal = { x: number, z: number, gives: string, takes: string?, level: number, y: number? }
    local pedestals: { Pedestal } = {
        { x = 51, z = 2, gives = "chambers_game:gun_blue", level = 1 },
        { x = 72, z = 2, gives = "chambers_game:gun", takes = "chambers_game:gun_blue", level = 2 },
    }
    -- The push buttons: the door each opens and for how long.
    local PUSHES = {
        { x = 118, y = FL, z = 2, door = 7, time = 12 },    -- 06
        { x = 220, y = FL, z = 2, door = 11, time = 10 },   -- 10
    }
    local timers: { [number]: number } = {}   -- seconds left, by door
    -- The last room's push button: the next level of the series past chamber
    -- 15 (levels of zones, below).
    local MORE_X, MORE_Z = 445, 2

    -- Trigger zones: a box (cells x0..x1, z0..z1 inclusive, y0..y1 in
    -- metres) in a room; the first body to come into it does its actions, once
    -- until the room starts again (somebody comes into it with nobody there):
    --   { "door", k }               open room k's door, and keep it open
    --   { "portal", ax, ay, az, bx, by, bz }
    --                               two portals in east walls (facing -x), each
    --                               ay/by its bottom, linked, their walls hollowed
    --   { "wake", k }               room k's turrets start looking
    --   { "say", text }             a line for everybody in the room
    type Trigger = { room: number, x0: number, x1: number, y0: number, y1: number, z0: number, z1: number,
                     actions: { { any } }, fired: boolean }
    local TRIGGERS: { Trigger } = {
        -- 11: the target on the ledge opens the door.
        { room = 12, x0 = 263, x1 = 264, y0 = FL + 5.5, y1 = FL + 9, z0 = 9, z1 = 10,
          actions = { { "door", 12 }, { "say", "The door is open. Mind the step down." } }, fired = false },
        -- 12: a step into the room wakes the turrets.
        { room = 13, x0 = 275, x1 = 276, y0 = FL - 1, y1 = FL + 4, z0 = 0, z1 = Z1,
          actions = { { "wake", 13 } }, fired = false },
        -- 13: on the ledge, a way down by portal to the door.
        { room = 14, x0 = 313, x1 = 320, y0 = FL + 5.5, y1 = FL + 9, z0 = 0, z1 = 3,
          actions = { { "portal", 321, FL + 6, 1, 321, FL, 9 } }, fired = false },
    }
    -- Dissolving fields: the plane x + 0.5 across a room, z0..z1 and y0..y1.
    type Field = { x: number, room: number, y0: number, y1: number, z0: number, z1: number }
    local FIELDS: { Field } = {
        { x = 301, room = 14, y0 = FL, y1 = FL + 14, z0 = 0, z1 = Z1 + 1 },
    }
    -- Sentry turrets: where, the way they face, their room; `id` the creature
    -- while there is one, `state` idle (asleep), awake, down (knocked over,
    -- silent for good) or gone (dissolved).
    -- `y`: where it stands (FL in the chambers); `near`: it wakes as somebody
    -- comes within 12 (a level of zones has no trigger for it).
    type Turret = { x: number, z: number, fx: number, fz: number, room: number, id: number, state: string,
                    seen: number, next: number, lost: number, aimed: number, y: number?, near: boolean? }
    local function turret(x: number, z: number, room: number): Turret
        return { x = x, z = z, fx = -1, fz = 0, room = room, id = 0, state = "idle", seen = 0, next = 0, lost = 0,
                 aimed = 0 }
    end
    local TURRETS: { Turret } = { turret(287.5, 2.5, 13), turret(290.5, 5.5, 13), turret(287.5, 9.5, 13) }
    local fields_told: () -> () = function() end   -- set with the fields' code, below
    -- The devices (devices.luau): light bridges' emitters, in a wall, and the
    -- way they lay; gel dispensers, in a ceiling, their gel and how far round
    -- where it lands they paint.
    type Device = { kind: string, x: number, y: number, z: number, dx: number?, dz: number?, room: number,
                    gel: string?, spread: number? }
    local DEVICES: { Device } = {
        { kind = "bridge", x = 387, y = FL, z = -1, dx = 0, dz = 1, room = 17 },        -- 16
        { kind = "dispenser", x = 413, y = FL + 12, z = 2, gel = "bounce", spread = 1, room = 18 },   -- 17
        { kind = "dispenser", x = 424, y = FL + 12, z = 5, gel = "speed", spread = 2, room = 18 },
    }
    local cubes: { [number]: boolean } = {}   -- every cube there is
    local D = Devices.server(tell, cubes)
    -- A device at its place: in the chambers as it is, in a site's copy moved
    -- along x by o, mirrored across the room or not, in the room keyed `room`.
    local function add_device(d: Device, o: number, mir: boolean, room: number)
        local z = if mir then Z1 - d.z else d.z
        local dz = if mir then -(d.dz or 0) else (d.dz or 0)
        if d.kind == "bridge" then
            D.bridge({ x = d.x + o, y = d.y, z = z, dx = d.dx or 0, dz = dz, room = room })
        else
            D.dispenser({ x = d.x + o, y = d.y, z = z, kind = d.gel, spread = d.spread, room = room })
        end
    end
    for _, d in DEVICES do
        add_device(d, 0, false, d.room)
    end
    -- Come by a survival site (mods/keys' keys:level), a push button holds its
    -- door 1.2 - 0.04 d of its time (d 1 116%, d 10 80%); the last to come sets it.
    local hold = 1.0
    core.event.on("keys:level", function(ev)
        hold = 1.2 - 0.04 * ev.d
    end)

    -- A floor button, two by two from its lower corner (x, z), which wears it.
    -- A thing two by two drawn whole from its lower corner (x, z): mirrored,
    -- that corner is the cell z + 1 was.
    local function corner(z: number): number
        return if MIR then z + 1 else z
    end
    local function button(x: number, y: number, z: number)
        fill(x, x + 1, y, y, z, z + 1, BUTTON_PART)
        put(x, y, corner(z), BUTTON)
    end

    -- A pit with a white bottom and its ends shut: the long fall (11, 13).
    local function deep_pit(x0: number, x1: number)
        pit(x0, x1)
        fill(x0, x1, FL - 10, FL - 10, 0, Z1, PANEL)
        fill(x0 - 1, x0 - 1, FL - 9, FL - 2, 0, Z1, METAL)
        fill(x1 + 1, x1 + 1, FL - 9, FL - 2, 0, Z1, METAL)
    end

    -- Each chamber's own things, built over its room (build_row, below) with
    -- OFF and MIR set for wherever it goes; by the room's index in ROOMS.
    local PIECES: { [number]: () -> () } = {}

    PIECES[2] = function()
        -- 01: the closed room behind a metal wall, and the fixed pair's rims.
        fill(21, 21, FL, FL + 7, 0, Z1, METAL)
        rim(16, -1, FL, true, RIM_BLUE)
        rim(24, -1, FL, true, RIM_ORANGE)
        recess(16, -1, FL, 4)
        recess(24, -1, FL, 4)
    end

    PIECES[3] = function()
        -- 02: the vent in the ceiling, the button on the floor.
        put(33, FL + 8, 3, VENT)
        button(40, FL, 8)
    end

    PIECES[4] = function()
        -- 03: the pedestal, white panels on both long walls before the
        -- chasm, the chasm, and the orange portal's rim past it.
        put(51, FL, 2, PEDESTAL)
        fill(52, 55, FL, FL + 3, -1, -1, PANEL)
        fill(52, 55, FL, FL + 3, Z1 + 1, Z1 + 1, PANEL)
        pit(57, 61)
        rim(64, Z1 + 1, FL, true, RIM_ORANGE)
        recess(64, Z1 + 1, FL, 5)
    end

    PIECES[5] = function()
        -- 04: the pedestal; a white floor before the ledge; the ledge in the
        -- north-east corner, four high, its button; white panels on the north
        -- wall over it and on the walls round the floor; the vent.
        put(72, FL, 2, PEDESTAL)
        fill(73, 78, FL - 1, FL - 1, 2, 9, PANEL)
        fill(80, 87, FL, FL + 3, 8, Z1, METAL)
        button(85, FL + 4, 9)
        fill(81, 86, FL + 4, FL + 8, Z1 + 1, Z1 + 1, PANEL)
        fill(72, 78, FL, FL + 5, -1, -1, PANEL)
        put(74, FL + 9, 9, VENT)
    end

    PIECES[6] = function()
        -- 05: the pit with an island in it, the plates.
        pit(96, 100)
        pit(104, 108)
        fill(101, 103, FL - 9, FL - 1, -1, -1, METAL)
        fill(101, 103, FL - 9, FL - 1, Z1 + 1, Z1 + 1, METAL)
        pit(101, 103, 0, 2)
        pit(101, 103, 9, Z1)
        for k = 1, 2 do
            local p = plates[k]
            fill(p.x, p.x + 1, FL - 1, FL - 1, p.z, p.z + 1, PLATE_PART)
            put(p.x, FL - 1, corner(p.z), PLATE)   -- drawn whole from here
        end
    end

    PIECES[7] = function()
        -- 06: the push button, the clock over the exit, a pit between, white
        -- panels on each side of it.
        put(PUSHES[1].x, PUSHES[1].y, PUSHES[1].z, PUSH)
        fill(ROOMS[7].x1 + 1, ROOMS[7].x1 + 1, FL + 3, FL + 3, DZ0, DZ1, CLOCK)
        pit(121, 129)
        fill(117, 120, FL, FL + 4, -1, -1, PANEL)
        fill(130, 133, FL, FL + 4, Z1 + 1, Z1 + 1, PANEL)
        fill(130, 133, FL, FL + 4, -1, -1, PANEL)
    end

    PIECES[8] = function()
        -- 07: the cube's room behind glass three high in the south-west
        -- corner, its vent; a white panel high on its south wall, seen over
        -- the glass, and one low on its west wall for the way out; white on
        -- the north wall; then 04 again -- a white floor, the ledge in the
        -- north-east corner with the button, white over it.
        fill(137, 143, FL, FL + 2, 4, 4, GLASS)
        fill(144, 144, FL, FL + 2, 0, 4, GLASS)
        put(140, FL + 9, 1, VENT)
        fill(139, 142, FL + 5, FL + 8, -1, -1, PANEL)
        fill(136, 136, FL, FL + 3, 0, 3, PANEL)
        fill(145, 149, FL, FL + 3, Z1 + 1, Z1 + 1, PANEL)
        fill(145, 149, FL - 1, FL - 1, 5, 10, PANEL)
        fill(150, 156, FL, FL + 3, 8, Z1, METAL)
        button(154, FL + 4, 9)
        fill(151, 155, FL + 4, FL + 8, Z1 + 1, Z1 + 1, PANEL)
    end

    PIECES[9] = function()
        -- 08: two vents on the near side, a button there and one past a pit,
        -- white on the south wall before it and the north wall after.
        put(162, FL + 8, 2, VENT)
        put(164, FL + 8, 9, VENT)
        button(167, FL, 9)
        button(177, FL, 3)
        pit(169, 173)
        fill(161, 166, FL, FL + 3, -1, -1, PANEL)
        fill(175, 179, FL, FL + 3, Z1 + 1, Z1 + 1, PANEL)
    end

    PIECES[10] = function()
        -- 09: 05's plates, pits and island, then a pit six wide, white on
        -- the south wall before it and the north wall past it.
        pit(188, 192)
        pit(196, 200)
        fill(193, 195, FL - 9, FL - 1, -1, -1, METAL)
        fill(193, 195, FL - 9, FL - 1, Z1 + 1, Z1 + 1, METAL)
        pit(193, 195, 0, 2)
        pit(193, 195, 9, Z1)
        for k = 3, 4 do
            local pl = plates[k]
            fill(pl.x, pl.x + 1, FL - 1, FL - 1, pl.z, pl.z + 1, PLATE_PART)
            put(pl.x, FL - 1, corner(pl.z), PLATE)
        end
        pit(205, 210)
        fill(201, 204, FL, FL + 3, -1, -1, PANEL)
        fill(211, 214, FL, FL + 3, Z1 + 1, Z1 + 1, PANEL)
    end

    PIECES[11] = function()
        -- 10: the push button, a pit to the far side, white by the button
        -- and by the door, the clock over the door.
        put(220, FL, 2, PUSH)
        pit(222, 237)
        fill(219, 221, FL, FL + 3, -1, -1, PANEL)
        fill(238, 241, FL, FL + 3, Z1 + 1, Z1 + 1, PANEL)
        fill(ROOMS[11].x1 + 1, ROOMS[11].x1 + 1, FL + 3, FL + 3, DZ0, DZ1, CLOCK)
    end

    PIECES[12] = function()
        -- 11: a pit six wide, its bottom white, its ends shut; on the far side
        -- a white patch of floor beside a ledge five high along the north
        -- wall, a target on it.
        deep_pit(251, 256)
        fill(259, 262, FL - 1, FL - 1, 6, 7, PANEL)
        fill(259, 266, FL, FL + 5, 8, Z1, METAL)
        fill(263, 264, FL + 5, FL + 5, 9, 10, TARGET)
    end

    PIECES[13] = function()
        -- 12: a screen three high past the way in and two low walls to hide
        -- behind, white on the south wall by the way in and on the north wall
        -- behind the turrets (seen over the low walls), the vent.
        fill(277, 277, FL, FL + 2, 4, 7, METAL)
        fill(278, 278, FL, FL + 1, 1, 4, METAL)
        fill(283, 283, FL, FL + 1, 7, 10, METAL)
        fill(271, 274, FL, FL + 3, -1, -1, PANEL)
        fill(289, 293, FL, FL + 3, Z1 + 1, Z1 + 1, PANEL)
        put(271, FL + 8, 9, VENT)
    end

    PIECES[14] = function()
        -- 13: the vent by the way in, white on the south wall either side of
        -- the field, the field's emitters in both long walls, a deep pit, a
        -- white patch beside a ledge along the south wall with the button on
        -- it, and the rims of the way down from the ledge.
        put(298, FL + 14, 9, VENT)
        fill(297, 299, FL, FL + 3, -1, -1, PANEL)
        fill(302, 303, FL, FL + 3, -1, -1, PANEL)
        fill(301, 301, FL, FL + 13, -1, -1, EMITTER)
        fill(301, 301, FL, FL + 13, Z1 + 1, Z1 + 1, EMITTER)
        deep_pit(305, 310)
        fill(314, 317, FL - 1, FL - 1, 4, 5, PANEL)
        fill(313, 320, FL, FL + 5, 0, 3, METAL)
        button(317, FL + 6, 1)
        rim(321, 1, FL + 6, false, RIM_BLUE)
        rim(321, 9, FL, false, RIM_ORANGE)
    end

    PIECES[15] = function()
        -- 14: the steps. Two metal walls four high across the whole room, the
        -- second on top of the first. A jump takes the hands to an edge 3.38
        -- over the feet (player.cpp: 1.28 of jump, the hands 2.1 over the
        -- feet), short of four from the floor; from a cube's top, 0.72 up,
        -- they take it (measured: from the floor no, from one cube yes, up on
        -- the ledge). Two vents at the foot of each wall, one to spare. Metal,
        -- so no portal goes round them. Then a stair down, a node a cell, to
        -- the door.
        fill(331, 336, FL, FL + 3, 0, Z1, METAL)
        fill(337, 341, FL, FL + 7, 0, Z1, METAL)
        for i = 0, 6 do
            fill(342 + i, 342 + i, FL, FL + 6 - i, 0, Z1, METAL)
        end
        for _, v in { { 327, 3 }, { 327, 8 }, { 333, 3 }, { 333, 8 } } do
            put(v[1], FL + 14, v[2], VENT)
        end
    end

    PIECES[16] = function()
        -- 15: three cubes. A metal wall four high and a slab on it across the
        -- room: its edge 4.5 up, past what the hands take from one cube's top
        -- (4.10) and within what they take from two stacked (4.82). Two
        -- stacked are 1.44 high, out of a jump's 1.28 from the floor, so a
        -- third is a step to them. Three vents, no more; metal, so no portal
        -- goes round. Then a stair down -- half a node, then a node a cell --
        -- to the door.
        fill(369, 375, FL, FL + 3, 0, Z1, METAL)
        fill(369, 375, FL + 4, FL + 4, 0, Z1, SLAB)
        fill(376, 376, FL, FL + 3, 0, Z1, METAL)
        for i = 0, 2 do
            fill(377 + i, 377 + i, FL, FL + 2 - i, 0, Z1, METAL)
        end
        for _, v in { { 361, 2 }, { 363, 9 }, { 366, 4 } } do
            put(v[1], FL + 12, v[2], VENT)
        end
    end

    PIECES[17] = function()
        -- 16: the light bridge's emitter low in the south wall, two white
        -- panels across from it in the north wall where its light meets it
        -- and two low in the west wall by the way in; a pit ten wide.
        put(387, FL, -1, BRIDGE_EMITTER)
        fill(387, 387, FL, FL + 1, Z1 + 1, Z1 + 1, PANEL)
        fill(383, 383, FL, FL + 1, 2, 3, PANEL)
        pit(392, 401)
    end

    PIECES[18] = function()
        -- 17: the blue dispenser over a white patch of floor by the way in; a
        -- wall four high across the room, two thick, and a white patch of
        -- ceiling over its foot; the orange dispenser over the floor before
        -- a pit seven wide.
        put(413, FL + 12, 2, DISPENSE_BOUNCE)
        fill(412, 414, FL - 1, FL - 1, 1, 3, PANEL)
        fill(420, 421, FL, FL + 3, 0, Z1, METAL)
        fill(417, 419, FL + 12, FL + 12, 3, 8, PANEL)
        put(424, FL + 12, 5, DISPENSE_SPEED)
        pit(427, 433)
    end

    PIECES[#ROOMS] = function()
        -- The last room: the way home's rim, and the button for more.
        rim(ROOMS[#ROOMS].x0 - 1, 9, FL, false, RIM_BLUE)
        recess(ROOMS[#ROOMS].x0 - 1, 9, FL, 0)
        put(MORE_X, FL, MORE_Z, PUSH)
    end

    -- The rooms of a row, each placed at its offset along x and mirrored
    -- across the room or not: all the rooms first, then the doorways and
    -- corridors between each and the next (a room's walls would shut them),
    -- then each one's own things.
    type Place = { k: number, o: number, mir: boolean }
    local function build_row(row: { Place })
        for _, pl in row do
            OFF = pl.o
            room(ROOMS[pl.k])
        end
        -- Doorways and corridors between rooms, and the exit doors.
        for i = 1, #row - 1 do
            local a, b = ROOMS[row[i].k], ROOMS[row[i + 1].k]
            OFF = row[i].o
            local cx = a.x1 + 2
            fill(cx, cx, FL - 1, FL - 1, DZ0 - 1, DZ1 + 1, FLOOR)
            fill(cx, cx, FL + 2, FL + 2, DZ0 - 1, DZ1 + 1, METAL)
            put(cx, FL + 2, DZ0, LIGHT)
            fill(cx, cx, FL, FL + 1, DZ0 - 1, DZ0 - 1, METAL)
            fill(cx, cx, FL, FL + 1, DZ1 + 1, DZ1 + 1, METAL)
            fill(cx, cx, FL, FL + 1, DZ0, DZ1, 0)
            door_cells(a.x1 + 1, false)
            -- A green sign over each exit.
            fill(a.x1 + 1, a.x1 + 1, FL + 2, FL + 2, DZ0, DZ1, EXIT)
            OFF = row[i + 1].o
            fill(b.x0 - 1, b.x0 - 1, FL, FL + 1, DZ0, DZ1, 0)   -- the next room's doorway
        end
        for _, pl in row do
            local piece = PIECES[pl.k]
            if piece ~= nil then
                OFF, MIR = pl.o, pl.mir
                piece()
            end
        end
        OFF, MIR = 0, false
    end
    local MAIN: { Place } = {}
    for k = 1, #ROOMS do
        table.insert(MAIN, { k = k, o = 0, mir = false })
    end
    local function build()
        build_row(MAIN)
    end

    -- The chambers are made as their columns are, a mapgen stage: lit with
    -- the column, there however far a player is.
    -- A survival site's chambers (see "a site's chambers"): a row of its own
    -- made of the chambers picked and mirrored for it, CHW apart from CH0 on,
    -- slot by slot, from x0 to x1.
    -- (Plan 9 made the row longer than 300: its copies moved from 20000 to
    -- 40000; plan 13 made a site's its own mix and moved them to 60000. The
    -- old ones are left where they were, out of the way.)
    local CH0, CHW = 60000, 400
    type Layout = { row: { Place }, x0: number, x1: number }
    local layouts: { [number]: Layout } = {}
    core.register_mapgen("chambers_game:chambers", function(ctx)
        if ctx.max_z < -2 or ctx.min_z > 13 then
            return
        end
        making = ctx
        if not (ctx.max_x < -2 or ctx.min_x > ROOMS[#ROOMS].x1 + 2) then
            build()
        end
        for _, lay in layouts do
            if not (ctx.max_x < lay.x0 - 6 or ctx.min_x > lay.x1 + 4) then
                build_row(lay.row)
            end
        end
        making = nil
    end)

    -- Each start, every door shut and every button up, as they were made --
    -- and as they were saved: a door is shut and a button up whenever nobody
    -- holds them, and nothing is written that is already so.
    for k = 1, #ROOMS - 1 do
        doors[k] = { x = ROOMS[k].x1 + 1, open = false, want = false, frame = 0, next = 0 }
    end

    -- A mapgen stage makes each column once, so a world made by an older
    -- plan of the chambers keeps it: the fixed portals in solid wall, the
    -- doors of four blocks each drawing a whole door. The plan's version is
    -- kept, and a world behind it has the chambers built again over it, all
    -- of them, at its start (writes to columns not made yet wait for them).
    local CHAMBERS = "13"
    core.event.on("core:world_ready", function()
        if core.storage_get("chambers") ~= CHAMBERS then
            build()
            core.storage_set("chambers", CHAMBERS)
            core.log("chambers_game: the chambers built again, plan " .. CHAMBERS)
        end
        -- And the panels a portal was in when the world last stopped:
        -- portals do not outlive a run, the shells do.
        for c in string.gmatch(core.storage_get("shells"), "[^;]+") do
            local x, y, z = string.match(c, "^(-?%d+),(-?%d+),(-?%d+)$")
            if x then
                core.set_block(tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number, PANEL)
            end
        end
        core.storage_set("shells", "")
        -- And what the devices laid, laid again from nothing as they work.
        D.forget_saved()
    end)

    -- Nobody digs or builds in a test chamber.
    core.event.on("core:pre_break", function(ev)
        ev.cancel = true
    end)
    core.event.on("core:pre_place", function(ev)
        ev.cancel = true
    end)

    -- ---------------------------------------------------------- players

    type Aim = { x: number, y: number, z: number, axis: number, sign: number, rot: number?, hw: number, hh: number,
                 cells: { { number } }, face: number }
    type Shot = { x: number, y: number, z: number, idx: number, at: Aim }
    -- `zl`: the tag of the level of zones they are in (below), or nil.
    type P = { blue: Shot?, orange: Shot?, carry: number, room: number, plate_at: number, level: number,
               name: string, low: number, px: number?, py: number?, pz: number?, said: number, zl: string? }
    local players: { [number]: P } = {}

    -- A room by its key: k in the chambers, k + 100 * slot in a site's copy.
    -- A room by its key: k in the chambers, i + 100 * slot for the i-th room
    -- of a site's row.
    local function room_of(x: number): number
        if x >= CH0 - 8 then
            local slot = (x - CH0 + 8) // CHW
            local lay = layouts[slot]
            if lay ~= nil then
                for i, pl in lay.row do
                    local r = ROOMS[pl.k]
                    if x - pl.o >= r.x0 - 1 and x - pl.o <= r.x1 + 2.5 then
                        return i + 100 * slot
                    end
                end
            end
            return 0
        end
        for k, r in ROOMS do
            if x >= r.x0 - 1 and x <= r.x1 + 2.5 then
                return k
            end
        end
        return 0
    end
    -- The chamber a room is (its index in ROOMS, 0 for none), how far along x
    -- from the chambers' own, and whether mirrored.
    local function base_of(key: number): (number, number, boolean)
        if key >= 100 then
            local lay = layouts[key // 100]
            local pl = if lay ~= nil then lay.row[key % 100] else nil
            if pl == nil then
                return 0, 0, false
            end
            return pl.k, pl.o, pl.mir
        end
        return key, 0, false
    end

    local function start_of(key: number): (number, number, number)
        local k, o = base_of(key)
        local r = ROOMS[math.max(k, 1)]
        return r.x0 + 1.5 + o, FL, 6.0
    end

    local function guns_of(p: number): (boolean, boolean)
        local held = core.held_item(p)
        return held == "chambers_game:gun_blue" or held == "chambers_game:gun", held == "chambers_game:gun"
    end

    local function crosshair(p: number)
        local s = players[p]
        if s ~= nil then
            tell(p, `portals {if s.blue then 1 else 0} {if s.orange then 1 else 0}`)
        end
    end

    -- --------------------------------------------------------- the portals

    -- Solid to a portal wanting room in front of it: not a light bridge's
    -- light, which goes where the beam does.
    local function solid(x: number, y: number, z: number): boolean
        local b = core.get_block(x, y, z)
        return b ~= D.BRIDGE and core.node_solid(b)
    end

    -- Which face of cell (cx, cy, cz) a ray from o along d (of length 1) met
    -- at distance t: the plane nearest the point it reached, of those the ray
    -- comes at from in front.
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

    -- Where a portal shot from player p's eye lands, or nil and why not.
    local function aim(p: number): (Aim?, string?)
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then
            return nil, nil
        end
        local len = math.sqrt(dx * dx + dy * dy + dz * dz)
        if len < 1e-6 then
            return nil, nil
        end
        dx, dy, dz = dx / len, dy / len, dz / len
        local ox, oy, oz = x, y + EYE, z
        local cx, cy, cz, t = core.raycast(ox, oy, oz, dx, dy, dz, 80)
        -- Gel lying on a floor is a skin the shot goes through to the panel
        -- under it (devices.luau).
        for _ = 1, 40 do
            if cx == nil or not D.gel(core.get_block(cx, cy, cz)) then break end
            ox, oy, oz = ox + dx * (t + 0.05), oy + dy * (t + 0.05), oz + dz * (t + 0.05)
            cx, cy, cz, t = core.raycast(ox, oy, oz, dx, dy, dz, 80)
        end
        if cx == nil then
            return nil, "nothing there"
        end
        local nx, ny, nz = face_of(ox, oy, oz, dx, dy, dz, t, cx, cy, cz)
        if nx == 0 and ny == 0 and nz == 0 then
            return nil, nil
        end
        -- A white panel, or one a portal is already in (it moves).
        local function panel(a: number, b: number, c: number): boolean
            local k = core.get_block(a, b, c)
            return k == PANEL or k == SHELL_LOW or k == SHELL_HIGH or k == SHELL_ONE or k == SHELL_FLAT
        end
        local face = face_byte(nx, ny, nz)
        if ny ~= 0 then
            -- A floor or a ceiling: two panels long, as a wall's is two high,
            -- the long way the way the shooter looks and its "up" that way
            -- (yaportal's gun4): the panel hit and the next one on, or if that
            -- will not do the one behind it, room on the side it faces over
            -- both. The pair's rot by the facing: a ceiling's basis is the
            -- floor's turned over, so its quarter turns run the other way.
            local along_z = math.abs(dz) >= math.abs(dx)
            local step = if along_z then (if dz >= 0 then 1 else -1) else (if dx >= 0 then 1 else -1)
            local rot = if along_z then (if step > 0 then 0 else 2)
                else (if (step > 0) == (ny > 0) then 3 else 1)
            for _, st in { step, -step } do
                local ox2, oz2 = if along_z then 0 else st, if along_z then st else 0
                if panel(cx, cy, cz) and panel(cx + ox2, cy, cz + oz2) and not solid(cx, cy + ny, cz)
                    and not solid(cx + ox2, cy + ny, cz + oz2) then
                    return { x = cx + 0.5 + ox2 * 0.5, y = cy + 0.5, z = cz + 0.5 + oz2 * 0.5, axis = 2,
                             sign = ny, rot = rot, hw = 0.5, hh = 1.0,
                             cells = { { cx, cy, cz, SHELL_FLAT }, { cx + ox2, cy, cz + oz2, SHELL_FLAT } },
                             face = face }, nil
                end
            end
            return nil, "a portal needs two white panels side by side"
        end
        -- A wall: two panels one over the other, air in front of both -- the
        -- one hit and the one under it, or over it: as low as it goes, so a
        -- portal shot at a wall's foot is walked into. Both become shells
        -- open to the room, and the portal stands in them, 0.4 into the
        -- wall: in it, and shallow enough that a body is past it before
        -- it meets the shells' back.
        local fx, fz = cx + nx, cz + nz
        for _, base in { cy - 1, cy } do
            if panel(cx, base, cz) and panel(cx, base + 1, cz) and not solid(fx, base, fz)
                and not solid(fx, base + 1, fz) then
                return { x = cx + 0.5 + nx * 0.1, y = base + 1.0, z = cz + 0.5 + nz * 0.1,
                         axis = if nz ~= 0 then 0 else 1, sign = nx + nz, hw = 0.5, hh = 1.0,
                         cells = { { cx, base, cz, SHELL_LOW }, { cx, base + 1, cz, SHELL_HIGH } },
                         face = face }, nil
            end
        end
        return nil, "a portal needs two white panels, one over the other"
    end

    -- The chamber's fixed orange portal a blue one leads to when its maker
    -- has no orange of their own (03).
    -- 03's fixed orange leads to one blue at a time: of those shot in 03 by
    -- players with no orange of their own, the one whose maker is nearest it
    -- -- whoever is about to walk into theirs. Asked again every tick.
    local c3_linked = -1
    local function step_fixed_orange()
        local o = fixed.c3_orange.idx
        if o < 0 then
            return
        end
        local best, near = -1, math.huge
        for p, s in players do
            local b = s.blue
            if b ~= nil and s.orange == nil and room_of(b.x) == 4 then
                local x, y, z = core.player_pos(p)
                if x ~= nil then
                    local d = (x - b.x) ^ 2 + (y + 1 - b.y) ^ 2 + (z - b.z) ^ 2
                    if d < near then
                        best, near = b.idx, d
                    end
                end
            end
        end
        if best ~= c3_linked then
            D.link(o, best)
            c3_linked = best
        end
    end

    local function link_pair(p: number)
        local s = players[p]
        if s == nil then
            return
        end
        local one = s.blue or s.orange
        if s.blue and s.orange then
            D.link(s.blue.idx, s.orange.idx)
        elseif one ~= nil then
            D.link(one.idx, -1)
        end
        -- Whatever 03's orange led to may have just been taken from it.
        if one ~= nil and (one.idx == c3_linked or (s.orange and s.orange.idx == c3_linked)) then
            c3_linked = -1
        end
        step_fixed_orange()
    end

    -- Every player's portals are rimmed in their colour on every screen:
    -- "rim <player> <colour> x y z axis sign hw hh", or "-" when it closes.
    local function rim_told(p: number, colour: string, at: Aim?)
        if at == nil then
            tell(0, `rim {p} {colour} -`)
        else
            -- Its half sizes across x and z for a floor's, as drawn.
            local hw, hh = at.hw, at.hh
            if at.axis == 2 and (at.rot or 0) % 2 == 1 then
                hw, hh = hh, hw
            end
            tell(0, string.format("rim %d %s %.2f %.2f %.2f %d %d %.2f %.2f", p, colour, at.x, at.y, at.z,
                                  at.axis, at.sign, hw, hh))
        end
    end

    -- The panels a portal is in hollowed into shells, and back.
    -- Every shell a gun's portal is in, kept, for the next start to put
    -- back as panels.
    local shells: { [string]: boolean } = {}
    local function keep_shells()
        local out = {}
        for k in shells do
            table.insert(out, k)
        end
        core.storage_set("shells", table.concat(out, ";"))
    end
    local function hollow(at: any)
        for _, c in at.cells do
            core.set_block(c[1], c[2], c[3], c[4])
            core.set_facing(c[1], c[2], c[3], at.face)
            shells[`{c[1]},{c[2]},{c[3]}`] = true
        end
        keep_shells()
    end
    local function close_shot(sh: Shot?)
        if sh ~= nil then
            core.portal_close(sh.x, sh.y, sh.z)
            D.closed(sh.idx)
            for _, c in sh.at.cells do
                core.set_block(c[1], c[2], c[3], PANEL)
                shells[`{c[1]},{c[2]},{c[3]}`] = nil
            end
            keep_shells()
        end
    end

    local function shoot(p: number, colour: string)
        local s = players[p]
        if s == nil then
            return
        end
        local at, why = aim(p)
        if at == nil then
            if why then
                tell(p, "fizzle")
            end
            return
        end
        local mine = if colour == "blue" then s.blue else s.orange
        local theirs = if colour == "blue" then s.orange else s.blue
        if theirs and math.abs(theirs.x - at.x) + math.abs(theirs.y - at.y) + math.abs(theirs.z - at.z) < 1.5 then
            tell(p, "fizzle")
            return
        end
        close_shot(mine)
        local idx = core.portal_open(at.x, at.y, at.z, at.axis, at.sign, at.rot or 0, at.hw, at.hh, if colour == "blue" then 1 else 2)
        if idx < 0 then
            tell(p, "fizzle")
            return
        end
        hollow(at)
        -- Where it is for a beam or a stream to meet it: its cells, the way it faces.
        D.opened(idx, at.cells, if at.axis == 1 then at.sign else 0, if at.axis == 2 then at.sign else 0,
                 if at.axis == 0 then at.sign else 0)
        local sh = { x = at.x, y = at.y, z = at.z, idx = idx, at = at }
        if colour == "blue" then
            s.blue = sh
        else
            s.orange = sh
        end
        rim_told(p, colour, at)
        link_pair(p)
        tell(p, `shot {colour}`)
        crosshair(p)
    end

    -- ------------------------------------------------------------ the cubes


    local function dispense(v: Vent)
        if v.cube ~= 0 then
            core.mob_remove(v.cube)
            cubes[v.cube] = nil
            -- Somebody carrying it lets go, and their client stops drawing it.
            for p, s in players do
                if s.carry == v.cube then
                    s.carry = 0
                    tell(p, "hold -")
                end
            end
        end
        -- In the tube, which hangs from the ceiling a metre and a bit.
        v.cube = core.spawn_mob("chambers_game:cube", v.x + 0.5, v.top, v.z + 0.5)
        if v.cube ~= 0 then
            cubes[v.cube] = true
            -- A box that tumbles: thrown, knocked or dropped it rolls and
            -- lies the way it lands (core.mob_tumble).
            core.mob_tumble(v.cube, true, 30)
        end
    end

    -- The cube player p is looking at within reach, or 0.
    local function aimed_cube(p: number): number
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then
            return 0
        end
        local len = math.sqrt(dx * dx + dy * dy + dz * dz)
        dx, dy, dz = dx / len, dy / len, dz / len
        local ox, oy, oz = x, y + EYE, z
        local best, which = 0.6, 0
        local things = table.clone(cubes)
        for _, t in TURRETS do
            if t.id ~= 0 then things[t.id] = true end
        end
        for c in things do
            local cx, cy, cz = core.mob_pos(c)
            if cx ~= nil then
                local vx, vy, vz = cx - ox, cy + 0.35 - oy, cz - oz
                local along = vx * dx + vy * dy + vz * dz
                if along > 0 and along < 2.8 then
                    local ex, ey, ez = vx - dx * along, vy - dy * along, vz - dz * along
                    local off = math.sqrt(ex * ex + ey * ey + ez * ez)
                    if off < best then
                        best, which = off, c
                    end
                end
            end
        end
        return which
    end

    local function carry_point(p: number): (number?, number?, number?)
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then
            return nil, nil, nil
        end
        local len = math.sqrt(dx * dx + dy * dy + dz * dz)
        dx, dy, dz = dx / len, dy / len, dz / len
        local ox, oy, oz = x, y + EYE, z
        local reach = 1.6
        local _, _, _, wall = core.raycast(ox, oy, oz, dx, dy, dz, reach + 0.5)
        if wall ~= nil then
            reach = math.max(0.4, wall - 0.5)
        end
        -- Held at least half a node up: a cube in the hands is not dropped
        -- into a portal in the floor by looking down at it.
        return ox + dx * reach, math.max(y + 0.6, oy + dy * reach - 0.35), oz + dz * reach
    end

    local function drop(p: number, throw: boolean)
        local s = players[p]
        if s == nil or s.carry == 0 then
            return
        end
        local c = s.carry
        s.carry = 0
        core.mob_unseen(c, 0)
        tell(p, "hold -")
        if throw then
            local dx, dy, dz = core.player_look(p)
            if dx ~= nil then
                core.mob_velocity(c, dx * 9, dy * 9 + 2, dz * 9)
            end
        else
            core.mob_velocity(c, 0, 0, 0)
        end
    end

    -- ------------------------------------------------------------ turrets
    --
    -- A turret awake looks the way it faces: whoever is within 18, inside 55
    -- degrees of that and not behind a block is seen; seen for 0.7 s, shot, 2
    -- points every 0.4 s (two turrets empty a full bar in about 3 s), a bolt
    -- drawn on every screen. It is put
    -- out of action for good by being knocked over -- a cube thrown into it,
    -- or picked up (only from behind or the side: from the front it refuses)
    -- -- and then it lies there as turret_down. It talks, with contempt.

    local LINES = {
        wake = { "Turret: Oh. A visitor. How tedious.", "Turret: Somebody's here. I'll pretend to be pleased." },
        spot = { "Turret: There you are. Not that I was looking.", "Turret: I see you. Unfortunately.",
                 "Turret: Hold still. It's the least you can do.", "Turret: Target acquired. Standards lowered." },
        lost = { "Turret: Hiding. Very brave.", "Turret: Gone? Typical.", "Turret: Come back. I was nearly interested." },
        kill = { "Turret: That was quicker than I expected. Not that I expected much.",
                 "Turret: Don't take it personally. I do." },
        front = { "Turret: From the front? I'm a turret, not an idiot.", "Turret: Nice try. Hands where I can shoot them." },
        lift = { "Turret: Put me down. This is undignified.", "Turret: Hands off. I have a reputation." },
        tip = { "Turret: Fine. I wasn't trying anyway.", "Turret: I'll remember this. I have excellent memory.",
                "Turret: Ow. Rude, but effective." },
        last = { "Turret: Congratulations on defeating the furniture." },
    }
    local said_n = 0
    local function line(kind: string): string
        said_n += 1
        local l = LINES[kind]
        return l[said_n % #l + 1]
    end
    -- A line for everybody in room key k, at most one a player every 2 s.
    local function say_in(k: number, text: string, force: boolean?)
        for p, s in players do
            if s.room == k and (force or clock >= s.said) then
                core.notice(p, text)
                s.said = clock + 2
            end
        end
    end

    local function turret_of(id: number): Turret?
        for _, t in TURRETS do
            if t.id == id and id ~= 0 then return t end
        end
        return nil
    end

    local function knock_over(t: Turret, why: string)
        if t.state == "down" or t.state == "gone" then return end
        t.state = "down"
        core.mob_act(t.id, "")
        core.mob_become(t.id, "chambers_game:turret_down")
        local x, y, z = core.mob_pos(t.id)
        if x ~= nil then core.play_sound("chambers_game:turret_tip", x, y + 0.6, z) end
        local left = 0
        for _, o in TURRETS do
            if o.room == t.room and (o.state == "idle" or o.state == "awake") then left += 1 end
        end
        say_in(t.room, if left == 0 then line("last") else line(why), true)
    end

    -- Picked up by player p: refused from the front of a turret still on its
    -- feet; otherwise knocked over by it. True when it may be carried.
    local function lift_turret(p: number, t: Turret): boolean
        if t.state == "idle" or t.state == "awake" then
            local x, _, z = core.player_pos(p)
            local tx, _, tz = core.mob_pos(t.id)
            if x ~= nil and tx ~= nil then
                local dx, dz = x - tx, z - tz
                local l = math.max(0.01, math.sqrt(dx * dx + dz * dz))
                if (dx * t.fx + dz * t.fz) / l > 0.3 then
                    core.notice(p, line("front"))
                    return false
                end
            end
            knock_over(t, "lift")
        end
        return true
    end

    -- A room's turrets as new, standing, asleep (on somebody coming into it
    -- with nobody there).
    local function reset_turrets(k: number)
        for _, t in TURRETS do
            if t.room == k then
                if t.id ~= 0 then
                    for _, s in players do
                        if s.carry == t.id then s.carry = 0 end
                    end
                    core.mob_remove(t.id)
                end
                t.id = core.spawn_mob("chambers_game:turret", t.x, t.y or FL, t.z)
                t.state, t.seen, t.next, t.lost = "idle", 0, 0, 0
                if t.id ~= 0 then core.mob_face(t.id, t.x + t.fx * 8, t.z + t.fz * 8) end
            end
        end
    end

    -- Who turret t sees: the nearest player in its cone with nothing between.
    local function sighted(t: Turret): (number, number, number, number)
        local tx, ty, tz = core.mob_pos(t.id)
        if tx == nil then return 0, 0, 0, 0 end
        local ey = ty + 0.85
        local best, who, wx, wy, wz = 18.0, 0, 0, 0, 0
        for p in players do
            local x, y, z = core.player_pos(p)
            if x ~= nil and core.health(p) > 0 then
                local dx, dy, dz = x - tx, y + 1.1 - ey, z - tz
                local flat = math.sqrt(dx * dx + dz * dz)
                local d = math.sqrt(flat * flat + dy * dy)
                if d < best and flat > 0.01 and (dx * t.fx + dz * t.fz) / flat > 0.57 then
                    local _, _, _, wall = core.raycast(tx, ey, tz, dx, dy, dz, d)
                    if wall == nil or wall > d - 0.3 then
                        best, who, wx, wy, wz = d, p, x, y + 1.1, z
                    end
                end
            end
        end
        return who, wx, wy, wz
    end

    local cube_was: { [number]: { number } } = {}
    local function step_turrets(dt: number)
        -- A cube flying into a turret on its feet knocks it over.
        for c in cubes do
            local x, y, z = core.mob_pos(c)
            local was = cube_was[c]
            if x ~= nil and was ~= nil then
                local v = math.sqrt((x - was[1]) ^ 2 + (y - was[2]) ^ 2 + (z - was[3]) ^ 2) / math.max(dt, 1e-3)
                if v > 3 then
                    for _, t in TURRETS do
                        if t.id ~= 0 and (t.state == "idle" or t.state == "awake") then
                            local tx, ty, tz = core.mob_pos(t.id)
                            if tx ~= nil and (x - tx) ^ 2 + (z - tz) ^ 2 < 0.8 * 0.8 and y < ty + 1.2 and y > ty - 0.8 then
                                knock_over(t, "tip")
                            end
                        end
                    end
                end
            end
            cube_was[c] = if x ~= nil then { x, y, z } else nil
        end
        for _, t in TURRETS do
            if t.state ~= "awake" or t.id == 0 then continue end
            local who, wx, wy, wz = sighted(t)
            if who == 0 then
                if t.seen > 0 then
                    t.seen = 0
                    t.lost = clock
                    core.mob_act(t.id, "")
                    say_in(t.room, line("lost"))
                end
                continue
            end
            local tx, ty, tz = core.mob_pos(t.id)
            if tx == nil then continue end
            core.mob_face(t.id, wx, wz)
            if t.seen == 0 then
                core.play_sound("chambers_game:turret_spot", tx, ty + 0.8, tz)
                if clock - t.lost > 4 then say_in(t.room, line("spot")) end
            end
            t.seen += dt
            if clock >= t.aimed then
                t.aimed = clock + 0.1
                tell(0, string.format("aim %d %.2f %.2f %.2f %.2f %.2f %.2f", t.id, tx, ty + 0.85, tz, wx, wy, wz))
            end
            if t.seen >= 0.7 and clock >= t.next then
                t.next = clock + 0.4
                core.mob_act(t.id, "shoot")
                core.play_sound("chambers_game:turret_shot", tx, ty + 0.8, tz)
                tell(0, string.format("shot %.2f %.2f %.2f %.2f %.2f %.2f", tx, ty + 0.75, tz, wx, wy - 0.2, wz))
                if core.damage(who, 2) <= 0 then
                    say_in(t.room, line("kill"), true)
                end
            end
        end
        -- A room whose turrets are all down or gone lets you out.
        local standing: { [number]: boolean } = {}
        for _, t in TURRETS do
            if standing[t.room] == nil then standing[t.room] = false end
            if t.id == 0 or t.state == "idle" or t.state == "awake" then standing[t.room] = true end
        end
        for k, up in standing do
            set_door(k, not up)
        end
    end

    -- ------------------------------------------------------ trigger zones

    local function do_action(a: { any }, room: number)
        local what = a[1]
        if what == "door" then
            set_door(a[2], true)
        elseif what == "say" then
            say_in(room, a[2], true)
        elseif what == "wake" then
            for _, t in TURRETS do
                if t.room == a[2] and t.state == "idle" then t.state = "awake" end
            end
            say_in(a[2], line("wake"), true)
        elseif what == "portal" then
            local ids = {}
            for k = 0, 1 do
                local x, y, z = a[2 + 3 * k], a[3 + 3 * k], a[4 + 3 * k]
                for dy = 0, 1 do
                    core.set_block(x, y + dy, z, if dy == 0 then SHELL_LOW else SHELL_HIGH)
                    core.set_facing(x, y + dy, z, 1)
                end
                ids[k + 1] = core.portal_open(x + 0.4, y + 1, z + 0.5, 1, -1, 0, 0.5, 1.0)
            end
            if ids[1] >= 0 and ids[2] >= 0 then core.portal_link(ids[1], ids[2]) end
        end
    end

    local function step_triggers()
        for _, t in TRIGGERS do
            if t.fired then continue end
            for p in players do
                local x, y, z = core.player_pos(p)
                if x ~= nil and x >= t.x0 and x < t.x1 + 1 and z >= t.z0 and z < t.z1 + 1 and y >= t.y0 and y <= t.y1 then
                    t.fired = true
                    for _, a in t.actions do do_action(a, t.room) end
                    break
                end
            end
        end
    end
    -- Somebody came into room key k with nobody else there: it starts again.
    local function room_starts(k: number, who: number)
        for p, s in players do
            if p ~= who and s.room == k then return end
        end
        for _, t in TRIGGERS do
            if t.room == k then t.fired = false end
        end
        reset_turrets(k)
        D.room_starts(k)
    end

    -- ------------------------------------------------------- dissolving fields

    -- What crossing a field takes: the body's portals, and whatever it
    -- carries (a cube goes back to its vent, a turret is gone).
    local function dissolve_thing(c: number)
        -- Everybody sees it come apart where it is (its carrier, in their
        -- hands: "dissolve" there), before it goes.
        local t0 = turret_of(c)
        local x, y, z = core.mob_pos(c)
        local model = if t0 == nil then "cube" elseif t0.state == "down" then "turret_down" else "turret"
        for p, s in players do
            if s.carry == c then
                s.carry = 0
                tell(p, "dissolve")
            elseif x ~= nil and y ~= nil and z ~= nil then
                tell(p, string.format("zap %s %.2f %.2f %.2f", model, x, y, z))
            end
        end
        for _, v in vents do
            if v.cube == c then
                dispense(v)
                return
            end
        end
        local t = turret_of(c)
        if t then
            core.mob_remove(c)
            t.id, t.state = 0, "gone"
        end
    end
    local function crossed(f: Field, ax: number, ay: number, az: number, bx: number, by: number, bz: number): boolean
        local px = f.x + 0.5
        if (ax - px) * (bx - px) > 0 or (ax - bx) ^ 2 + (az - bz) ^ 2 > 4 then return false end
        local z, y = (az + bz) / 2, (ay + by) / 2
        return z >= f.z0 and z <= f.z1 and y >= f.y0 - 1 and y <= f.y1
    end
    local thing_was: { [number]: { number } } = {}
    local function step_fields()
        for p, s in players do
            local x, y, z = core.player_pos(p)
            if x == nil then continue end
            if s.px ~= nil then
                for _, f in FIELDS do
                    if crossed(f, s.px :: number, s.py :: number, s.pz :: number, x, y, z) then
                        local had = s.blue ~= nil or s.orange ~= nil or s.carry ~= 0
                        close_shot(s.blue)
                        close_shot(s.orange)
                        s.blue, s.orange = nil, nil
                        rim_told(p, "blue", nil)
                        rim_told(p, "orange", nil)
                        link_pair(p)
                        crosshair(p)
                        if s.carry ~= 0 then
                            dissolve_thing(s.carry)
                        end
                        if had then
                            core.play_sound("chambers_game:field_fizzle", x, y + 1, z)
                            tell(p, "fizzle")
                        end
                    end
                end
            end
            s.px, s.py, s.pz = x, y, z
        end
        -- And a thing thrown or fallen through one.
        local things = table.clone(cubes)
        for _, t in TURRETS do
            if t.id ~= 0 then things[t.id] = true end
        end
        for c in things do
            local x, y, z = core.mob_pos(c)
            local was = thing_was[c]
            if x ~= nil and was ~= nil then
                for _, f in FIELDS do
                    if crossed(f, was[1], was[2], was[3], x, y, z) then
                        core.play_sound("chambers_game:field_fizzle", x, y + 0.4, z)
                        dissolve_thing(c)
                        x = nil
                        break
                    end
                end
            end
            thing_was[c] = if x ~= nil then { x, y, z } else nil
        end
    end
    -- Every client draws the fields it is told of: "field x y0 z0 y1 z1".
    fields_told = function()
        for _, f in FIELDS do
            tell(0, string.format("field %d %.1f %.1f %.1f %.1f", f.x, f.y0, f.z0, f.y1, f.z1))
        end
    end

    -- ------------------------------------------------------------- clicks

    local more_pressed: (p: number) -> () = function(_) end
    local function pressed_push(p: number): boolean
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then
            return false
        end
        local cx, cy, cz = core.raycast(x, y + EYE, z, dx, dy, dz, 3)
        if cx == nil or core.get_block(cx, cy, cz) ~= PUSH then
            return false
        end
        if cx == MORE_X and cy == FL and cz == MORE_Z then
            more_pressed(p)
            return true
        end
        if D.pushed(cx, cy, cz) then
            return true
        end
        for _, b in PUSHES do
            if b.x == cx and b.y == cy and b.z == cz then
                local t = math.floor(b.time * hold + 0.5)
                timers[b.door] = t
                set_door(b.door, true)
                tell(0, `clock {t}`)
            end
        end
        return true
    end

    local function click(p: number, which: string)
        local s = players[p]
        if s == nil then
            return
        end
        if pressed_push(p) then
            return
        end
        local has_blue, has_orange = guns_of(p)
        if s.carry ~= 0 then
            drop(p, which == "left")
            return
        end
        if which == "right" then
            local c = aimed_cube(p)
            if c ~= 0 then
                local t = turret_of(c)
                if t ~= nil and not lift_turret(p, t) then
                    return
                end
                s.carry = c
                -- Drawn by their own client at their eye, not trailing it:
                -- left out of their world while it is in their hands.
                core.mob_unseen(c, p)
                tell(p, if t then "hold turret_down" else "hold cube")
                return
            end
            if has_orange then
                shoot(p, "orange")
            end
        elseif has_blue then
            shoot(p, "blue")
        end
    end

    core.event.on("core:net_message", function(ev)
        if ev.text == "left" or ev.text == "right" then
            click(ev.player, ev.text)
        end
    end)
    -- A process that is its own server: the clicks come straight here.
    heard_click = click

    -- -------------------------------------------------------------- the tick

    local fixed_open = false
    local function open_fixed()
        local all = true
        for _, f in fixed do
            if f.idx < 0 then
                f.idx = core.portal_open(f.x, f.y, f.z, f.axis, f.sign, 0, 0.5, 1.0)
                if f.idx < 0 then
                    all = false
                else
                    -- Its two cells in the wall, under its middle and over.
                    local cx, cz = math.floor(f.x), math.floor(f.z)
                    D.opened(f.idx, { { cx, f.y - 1, cz }, { cx, f.y, cz } }, if f.axis == 1 then f.sign else 0, 0,
                             if f.axis == 0 then f.sign else 0)
                end
            end
        end
        if all then
            for _, f in fixed do
                if f.pair then
                    D.link(f.idx, fixed[f.pair].idx)
                end
            end
            for _, f in fixed do
                if not f.pair then D.link(f.idx, -1) end
            end
            fixed_open = true
        end
    end

    -- ------------------------------------------------- a site's chambers
    --
    -- Come by a survival site (mods/keys' keys:level): a row of chambers of
    -- its own, CHW apart from CH0 by slot -- 2 + d / 2 of them, picked for
    -- the site's d out of the chambers (POOL), in the order of how hard they
    -- are, each the way it is or mirrored across the room. Built by the same
    -- pieces moved along x, its buttons, vents, plates, pedestals, push
    -- buttons, fixed portals, triggers, fields, turrets, devices and doors
    -- copied the same way into the tables the tick walks. The whole gun is
    -- given at its start. Its way out is behind the west wall of the first,
    -- under a dark lamp; a plain room past the last is the level done, and a
    -- portal there leads back to the first. A site keeps the row it was given
    -- (storage "layout <slot>"), and so keeps its level after a restart.
    --
    -- The chambers a site's row is made of, each with how hard it is, 1 to
    -- 8: not 01 (nothing to do) nor 03 (its fixed orange is the chambers').
    local POOL: { { number } } = {
        { 3, 1 }, { 5, 2 }, { 6, 2 }, { 7, 3 }, { 8, 3 }, { 9, 4 }, { 10, 4 }, { 11, 5 },
        { 12, 6 }, { 13, 5 }, { 14, 7 }, { 15, 6 }, { 16, 8 }, { 17, 4 }, { 18, 6 },
    }
    local base = { buttons = table.clone(buttons), vents = table.clone(vents), plates = table.clone(plates),
                   pedestals = table.clone(pedestals), pushes = table.clone(PUSHES), fixed = table.clone(fixed),
                   triggers = table.clone(TRIGGERS), fields = table.clone(FIELDS), turrets = table.clone(TURRETS) }
    -- Which of the chambers' rooms a thing at x is in.
    local function chamber_at(x: number): number
        for k, r in ROOMS do
            if x >= r.x0 - 1 and x <= r.x1 + 2.5 then
                return k
            end
        end
        return 0
    end

    -- The row for a site at d, from a seed: the chambers nearest d's
    -- hardness, with some luck in it, easiest first; the first never
    -- mirrored (its west wall is the way out's), the others by a coin.
    local function pick_row(d: number, seed: number): { number }
        local state = seed % 2147483647
        if state <= 0 then state += 2147483646 end
        local function rand(): number
            state = (state * 48271) % 2147483647
            return state / 2147483647
        end
        local count = math.clamp(2 + d // 2, 2, 7)
        local want = 1 + (d - 1) * 7 / 9
        type Pick = { k: number, rank: number, score: number }
        local scored: { Pick } = {}
        for _, c in POOL do
            table.insert(scored, { k = c[1], rank = c[2], score = math.abs(c[2] - want) + rand() * 3 })
        end
        table.sort(scored, function(a: Pick, b: Pick): boolean return a.score < b.score end)
        local chosen: { Pick } = {}
        for i = 1, count do table.insert(chosen, scored[i]) end
        table.sort(chosen, function(a: Pick, b: Pick): boolean return a.rank < b.rank or (a.rank == b.rank and a.k < b.k) end)
        local out = {}
        for i, c in chosen do
            table.insert(out, c.k)
            table.insert(out, if i > 1 and rand() < 0.5 then 1 else 0)
        end
        return out
    end

    -- A site's row laid out: each chamber four past the last one's end (a
    -- wall, a corridor, a wall), then the lobby's plain room for the end.
    local function lay_out(slot: number, picked: { number }): Layout
        local row: { Place } = {}
        local x = CH0 + slot * CHW + 8
        local x0 = x
        for i = 1, #picked - 1, 2 do
            local k = picked[i]
            local o = x - ROOMS[k].x0
            table.insert(row, { k = k, o = o, mir = picked[i + 1] == 1 })
            x = ROOMS[k].x1 + o + 4
        end
        table.insert(row, { k = 1, o = x - ROOMS[1].x0, mir = false })
        return { row = row, x0 = x0, x1 = x + ROOMS[1].x1 - ROOMS[1].x0 }
    end

    local function make_copy(slot: number, picked: { number })
        if layouts[slot] then return end
        local lay = lay_out(slot, picked)
        layouts[slot] = lay
        for i, pl in lay.row do
            local k, o, mir = pl.k, pl.o, pl.mir
            local key = i + 100 * slot
            -- A cell's z, a point's, and a two-by-two thing's lower corner's.
            local function cz(z: number): number return if mir then Z1 - z else z end
            local function pz(z: number): number return if mir then Z1 + 1 - z else z end
            local function lz(z: number): number return if mir then Z1 - 1 - z else z end
            local function here(x: number): boolean return chamber_at(x) == k end
            if i < #lay.row then
                doors[key] = { x = ROOMS[k].x1 + 1 + o, open = false, want = false, frame = 0, next = 0 }
            end
            for _, b in base.buttons do
                if here(b.x) then
                    table.insert(buttons, { x = b.x + o, y = b.y, z = lz(b.z), door = key, pressed = false })
                end
            end
            for _, v in base.vents do
                if here(v.x) then
                    table.insert(vents, { x = v.x + o, z = cz(v.z), top = v.top, room = key, cube = 0 })
                end
            end
            for _, p in base.plates do
                if here(p.x) then
                    table.insert(plates, { x = p.x + o, z = lz(p.z), vx = p.vx, vy = p.vy,
                                           vz = if mir then -p.vz else p.vz })
                end
            end
            for _, pd in base.pedestals do
                if here(pd.x) then
                    table.insert(pedestals, { x = pd.x + o, z = cz(pd.z), gives = pd.gives, takes = pd.takes,
                                              level = pd.level })
                end
            end
            for _, b in base.pushes do
                if here(b.x) then
                    table.insert(PUSHES, { x = b.x + o, y = b.y, z = cz(b.z), door = key, time = b.time })
                end
            end
            for name, f in base.fixed do
                if here(f.x) then
                    fixed[`{name}_{slot}_{i}`] = { x = f.x + o, y = f.y, z = pz(f.z), axis = f.axis,
                                                   sign = if mir and f.axis == 0 then -f.sign else f.sign, idx = -1,
                                                   pair = if f.pair then `{f.pair}_{slot}_{i}` else nil }
                end
            end
            for _, t in base.triggers do
                if here(t.x0) then
                    local acts = {}
                    for _, a in t.actions do
                        local b = table.clone(a)
                        if b[1] == "door" or b[1] == "wake" then
                            b[2] = key
                        elseif b[1] == "portal" then
                            b[2] += o
                            b[4] = cz(b[4])
                            b[5] += o
                            b[7] = cz(b[7])
                        end
                        table.insert(acts, b)
                    end
                    table.insert(TRIGGERS, { room = key, x0 = t.x0 + o, x1 = t.x1 + o, y0 = t.y0, y1 = t.y1,
                                             z0 = cz(if mir then t.z1 else t.z0), z1 = cz(if mir then t.z0 else t.z1),
                                             actions = acts, fired = false })
                end
            end
            for _, f in base.fields do
                if here(f.x) then
                    table.insert(FIELDS, { x = f.x + o, room = key, y0 = f.y0, y1 = f.y1,
                                           z0 = pz(if mir then f.z1 else f.z0), z1 = pz(if mir then f.z0 else f.z1) })
                end
            end
            for _, t in base.turrets do
                if here(t.x) then
                    local c = turret(t.x + o, pz(t.z), key)
                    c.fx, c.fz = t.fx, if mir then -t.fz else t.fz
                    table.insert(TURRETS, c)
                end
            end
            for _, d in DEVICES do
                if here(d.x) then
                    add_device(d, o, mir, key)
                end
            end
        end
        fields_told()
        fixed_open = false
        -- Built now, the columns not made yet waiting for their writes; and by
        -- the stage for any made later.
        build_row(lay.row)
    end
    -- A site's row as kept: "k m;k m;..." -- each chamber, mirrored or not.
    local function kept_row(slot: number): { number }?
        local text = core.storage_get(`layout {slot}`)
        if text == "" then
            return nil
        end
        local out = {}
        for k, m in string.gmatch(text, "(%d+) (%d);") do
            table.insert(out, tonumber(k) :: number)
            table.insert(out, tonumber(m) :: number)
        end
        return out
    end

    type Level = { slot: number, first: number, done_at: number, done: boolean }
    local level_of: { [number]: Level } = {}
    local function level_done(p: number, lv: Level)
        if lv.done then return end
        lv.done = true
        core.event.emit("keys:cleared", { player = p, slot = lv.slot })
        local rk, ro = base_of(lv.done_at)
        local fk, fo = base_of(lv.first)
        local r, f = ROOMS[rk], ROOMS[fk]
        local a = core.portal_open(r.x0 + ro + 1.5, FL + 1, 9.5, 1, 1, 0, 0.5, 1.0)
        local b = core.portal_open(f.x0 + fo + 1.5, FL + 1, 9.5, 1, 1, 0, 0.5, 1.0)
        if a >= 0 and b >= 0 then core.portal_link(a, b) end
        core.notice(p, "Prova superata. Un portale riporta alla prima camera: li' qualcosa si e' aperto.")
    end
    -- A site's level as a row of the chambers of its own: when the chambers
    -- have no zones to lay one of (levels of zones, below).
    local function site_by_copy(ev: { player: number, slot: number, d: number })
        if layouts[ev.slot] == nil then
            local picked = kept_row(ev.slot)
            if picked == nil then
                picked = pick_row(ev.d, ev.slot * 7919 + ev.d * 104729 + 17)
                local parts = {}
                for i = 1, #picked - 1, 2 do table.insert(parts, `{picked[i]} {picked[i + 1]};`) end
                core.storage_set(`layout {ev.slot}`, table.concat(parts))
                local known = core.storage_get("layouts")
                core.storage_set("layouts", known .. `{ev.slot};`)
            end
            make_copy(ev.slot, picked :: { number })
        end
        local lay = layouts[ev.slot]
        local count = #lay.row - 1
        local first, done_at = 1 + 100 * ev.slot, #lay.row + 100 * ev.slot
        level_of[ev.player] = { slot = ev.slot, first = first, done_at = done_at, done = false }
        -- The whole gun: any chamber may want both portals.
        local s = players[ev.player]
        if core.bag_count(ev.player, "chambers_game:gun") == 0 then
            local blue = core.bag_count(ev.player, "chambers_game:gun_blue")
            if blue > 0 then core.take_item(ev.player, "chambers_game:gun_blue", blue) end
            core.give_item(ev.player, "chambers_game:gun", 1)
            if s then s.level = 2 end
        end
        -- The way out: a block of metal outside the west wall, its hole at
        -- (x0 - 2, FL, 9) facing +x, behind two cells of that wall.
        local pl = lay.row[1]
        local x0 = ROOMS[pl.k].x0 + pl.o
        for x = x0 - 4, x0 - 2 do
            for z = 8, 10 do
                for y = FL - 1, FL + 3 do core.set_block(x, y, z, METAL) end
            end
        end
        core.set_block(x0 - 2, FL, 9, 0)
        core.set_block(x0 - 2, FL + 1, 9, 0)
        local sx, sy, sz = start_of(first)
        core.player_place(ev.player, sx, sy, sz, 1, 0, 0)
        core.player_spawn(ev.player, sx, sy, sz, true)
        core.event.emit("keys:start", { player = ev.player, slot = ev.slot, ex = x0 - 2, ey = FL, ez = 9 })
        core.notice(ev.player, `Camere di prova: {count}, dalla {ROOMS[pl.k].name}.`)
        local names = {}
        for i = 1, count do
            local p2 = lay.row[i]
            table.insert(names, `{p2.k - 1}{if p2.mir then "m" else ""}`)
        end
        core.log(`chambers_game: slot {ev.slot}, d {ev.d}: chambers {table.concat(names, " ")}`)
    end
    core.event.on("core:world_ready", function()
        for slot in string.gmatch(core.storage_get("layouts"), "(%d+);") do
            local n = tonumber(slot) :: number
            local picked = kept_row(n)
            if picked ~= nil then make_copy(n, picked) end
        end
    end)

    -- ------------------------------------------------ levels of zones
    --
    -- A level laid out of the chambers' zones (mods/zones, built in a player's
    -- workshop): a survival site's when there are any (keys:level, else the
    -- copy above), the series past chamber 15 (the last room's push button),
    -- an admin's /zone try here. What is in it works by rule, out of the
    -- blocks as they were laid ("zones:block"):
    --   a door                opens as somebody comes up to it, unless one of
    --                         these holds it -- each holds the door nearest it:
    --     a floor button      all of a door's buttons down: it is open
    --     a push button       open for 12 s, the clock over it
    --     a target tile       a body on it opens it for good
    --     a turret ("mob")    all of a door's turrets down: it is open
    --   a vent                drops a cube, and again when it falls out or
    --                         lies on a pit's bottom
    --   a launch plate        throws a body onto the nearest target tile
    --                         within 24, or straight up without one
    --   a pedestal            the whole gun
    --   a pit's bottom        a second on it, or out under the level, and a
    --                         body is back at the level's start
    -- The markers: "start" where a player is put, "exit" the level done.
    local PIT = id("pit")
    local function on_pit_at(x: number, y: number, z: number): boolean
        return core.get_block(math.floor(x), math.floor(y - 0.05), math.floor(z)) == PIT
    end
    function on_pit(c: number): boolean
        local x, y, z = core.mob_pos(c)
        return x ~= nil and on_pit_at(x, y :: number, z :: number)
    end
    local WATCH = { "door", "door_part", "button", "button_down", "button_part", "plate", "plate_part",
                    "pushbutton", "vent", "pedestal", "target" }
    do
        local names = {}
        for _, n in WATCH do table.insert(names, `chambers_core:{n}`) end
        core.event.emit("zones:watch", { names = table.concat(names, " ") })
    end
    local GRAVITY = 25   -- chambers_core's core.movement

    type ZLevel = { tag: string, kind: string, player: number, slot: number, d: number,
                    blocks: { { any } }, marks: { { any } }, box: { number }?,
                    start: { number }?, exit: { number }?, doors: { number }, done: boolean,
                    way: { number }? }
    local zlevels: { [string]: ZLevel } = {}
    local next_key = 1000000   -- a level's doors' keys, past any room's

    local function zlevel(tag: string): ZLevel
        local lv = zlevels[tag]
        if lv ~= nil then
            return lv
        end
        local fresh: ZLevel = { tag = tag, kind = "try", player = 0, slot = 0, d = 1, blocks = {}, marks = {},
                                doors = {}, done = false }
        zlevels[tag] = fresh
        return fresh
    end
    core.event.on("zones:block", function(ev)
        table.insert(zlevel(ev.tag).blocks, { ev.name, ev.x, ev.y, ev.z, ev.facing })
    end)
    core.event.on("zones:mark", function(ev)
        table.insert(zlevel(ev.tag).marks, { ev.kind, ev.x, ev.y, ev.z, ev.facing })
    end)

    -- Along which way an upright door's second cell stands from the one that
    -- wears it: +X turned f quarters (each +X to -Z), so the chambers' 3 is +Z.
    local ALONG = { { 1, 0 }, { 0, -1 }, { -1, 0 }, { 0, 1 } }
    local function cell_key(x: number, y: number, z: number): string
        return `{x},{y},{z}`
    end

    local function nearest_door(lv: ZLevel, x: number, y: number, z: number): number
        local best, which = math.huge, -1
        for _, k in lv.doors do
            local m = (doors[k] :: Door).mid :: { number }
            local dd = (m[1] - x) ^ 2 + (m[2] - y) ^ 2 * 4 + (m[3] - z) ^ 2
            if dd < best then
                best, which = dd, k
            end
        end
        return which
    end

    -- The 2 x 2 a two-by-two thing's cell `at` is in, out of `cells`: its lowest
    -- corner, which wears it (drawn towards +x and +z), made so if the zone was
    -- turned the other way.
    local function square(cells: { [string]: boolean }, x: number, y: number, z: number, main: number,
                          part: number): (number, number)
        for _, o in { { 0, 0 }, { -1, 0 }, { 0, -1 }, { -1, -1 } } do
            local cx, cz = x + o[1], z + o[2]
            if cells[cell_key(cx, y, cz)] and cells[cell_key(cx + 1, y, cz)] and cells[cell_key(cx, y, cz + 1)]
                and cells[cell_key(cx + 1, y, cz + 1)] then
                if cx ~= x or cz ~= z then
                    core.set_block(x, y, z, part)
                    core.set_block(cx, y, cz, main)
                end
                return cx, cz
            end
        end
        return x, z
    end

    -- A plate's throw: from the middle of its two by two onto the target tile
    -- nearest (its top), the way up 12 m/s or what reaches it.
    local function throw_of(lv: ZLevel, x: number, y: number, z: number): (number, number, number)
        local fx, fz, best, tx, ty, tz = x + 1, z + 1, 24 * 24, 0.0, 0.0, 0.0
        local found = false
        for _, b in lv.blocks do
            if b[1] == "chambers_core:target" then
                local dd = (b[2] + 0.5 - fx) ^ 2 + (b[4] + 0.5 - fz) ^ 2
                if dd < best then
                    best, tx, ty, tz, found = dd, b[2] + 0.5, b[3] + 1, b[4] + 0.5, true
                end
            end
        end
        if not found then
            return 0, 12, 0
        end
        local dy = ty - y
        local vy = math.max(12, math.sqrt(2 * GRAVITY * math.max(dy + 1.5, 0.5)))
        local t = (vy + math.sqrt(math.max(vy * vy - 2 * GRAVITY * dy, 0))) / GRAVITY
        return (tx - fx) / t, vy, (tz - fz) / t
    end

    -- Everything of a level that stands, made to work.
    local function wire(lv: ZLevel)
        local box = lv.box :: { number }
        local cells: { [string]: boolean } = {}
        for _, b in lv.blocks do
            if b[1] ~= "chambers_core:door" then
                cells[cell_key(b[2], b[3], b[4])] = true
            end
        end
        -- The doors first: everything else holds the nearest.
        for _, b in lv.blocks do
            if b[1] == "chambers_core:door" then
                local x, y, z, f = b[2], b[3], b[4], b[5] % 4
                local a = ALONG[f + 1]
                local ox, oz = x + a[1], z + a[2]
                if not cells[cell_key(ox, y, oz)] and cells[cell_key(x - a[1], y, z - a[2])] then
                    -- Mirrored: the cell that wears it is the other one.
                    ox, oz = x, z
                    x, z = x - a[1], z - a[2]
                    put_turned(x, y, z, DOOR, f)
                    put_turned(ox, y, oz, DOOR_PART, f)
                end
                next_key += 1
                doors[next_key] = { x = x, open = false, want = false, frame = 0, next = 0,
                                    cells = { { x, y, z }, { x, y + 1, z }, { ox, y, oz }, { ox, y + 1, oz } }, f = f,
                                    mid = { (x + ox) / 2 + 0.5, y, (z + oz) / 2 + 0.5 }, near = true }
                table.insert(lv.doors, next_key)
            end
        end
        local function hold(k: number)
            local d = doors[k]
            if d ~= nil then d.near = false end
        end
        for _, b in lv.blocks do
            local name, x, y, z = b[1], b[2], b[3], b[4]
            if name == "chambers_core:button" or name == "chambers_core:button_down" then
                local cx, cz = square(cells, x, y, z, BUTTON, BUTTON_PART)
                local k = nearest_door(lv, cx + 1, y, cz + 1)
                hold(k)
                table.insert(buttons, { x = cx, y = y, z = cz, door = k, pressed = false })
            elseif name == "chambers_core:plate" then
                local cx, cz = square(cells, x, y, z, PLATE, PLATE_PART)
                local vx, vy, vz = throw_of(lv, cx, y + 1, cz)
                table.insert(plates, { x = cx, z = cz, y = y + 1, vx = vx, vy = vy, vz = vz })
            elseif name == "chambers_core:pushbutton" then
                local k = nearest_door(lv, x, y, z)
                hold(k)
                table.insert(PUSHES, { x = x, y = y, z = z, door = k, time = 12 })
            elseif name == "chambers_core:target" then
                local k = nearest_door(lv, x, y, z)
                hold(k)
                table.insert(TRIGGERS, { room = -1, x0 = x, x1 = x, y0 = y + 0.5, y1 = y + 3, z0 = z, z1 = z,
                                         actions = { { "door", k } }, fired = false })
            elseif name == "chambers_core:vent" then
                local v: Vent = { x = x, z = z, top = y - 1, room = -1, cube = 0, low = box[2] - 3 }
                table.insert(vents, v)
                dispense(v)
            elseif name == "chambers_core:pedestal" then
                table.insert(pedestals, { x = x, z = z, y = y, gives = "chambers_game:gun",
                                          takes = "chambers_game:gun_blue", level = 2 })
            end
        end
        for _, m in lv.marks do
            if m[1] == "start" and lv.start == nil then
                lv.start = { m[2] + 0.5, m[3], m[4] + 0.5 }
            elseif m[1] == "exit" and lv.exit == nil then
                lv.exit = { m[2] + 0.5, m[3], m[4] + 0.5 }
            end
        end
        local st = lv.start or { box[1] + 1.5, box[2] + 1, box[3] + 1.5 }
        lv.start = st
        for _, m in lv.marks do
            if m[1] == "mob" then
                local x, z = m[2] + 0.5, m[4] + 0.5
                local k = nearest_door(lv, x, m[3], z)
                hold(k)
                local t = turret(x, z, k)
                t.y, t.near = m[3], true
                local dx, dz = st[1] - x, st[3] - z
                local len = math.sqrt(dx * dx + dz * dz)
                if len > 0.1 then
                    t.fx, t.fz = dx / len, dz / len
                end
                table.insert(TURRETS, t)
                reset_turrets(k)
            end
        end
        fields_told()
        core.log(`chambers_game: level {lv.tag} works: {#lv.doors} door(s), {#lv.blocks} thing(s), {#lv.marks} marker(s)`)
    end

    -- Taking a level's cells away before one is laid there again, a slice a tick.
    type Clearing = { box: { number }, x: number, after: () -> () }
    local clearings: { Clearing } = {}
    local function clear_then(tag: string, after: () -> ())
        local x1, y1, z1, x2, y2, z2 = string.match(core.storage_get(`box {tag}`),
            "^(-?%d+) (-?%d+) (-?%d+) (-?%d+) (-?%d+) (-?%d+)$")
        if x1 == nil then
            after()
            return
        end
        local box = { tonumber(x1) :: number, tonumber(y1) :: number, tonumber(z1) :: number,
                      tonumber(x2) :: number, tonumber(y2) :: number, tonumber(z2) :: number }
        table.insert(clearings, { box = box, x = box[1], after = after })
    end
    local function step_clearings()
        local c = clearings[1]
        if c == nil then return end
        local b = c.box
        local left = 30000
        while c.x <= b[4] and left > 0 do
            for y = b[2], b[5] do
                for z = b[3], b[6] do
                    core.set_block(c.x, y, z, 0)
                end
            end
            left -= (b[5] - b[2] + 1) * (b[6] - b[3] + 1)
            c.x += 1
        end
        if c.x > b[4] then
            table.remove(clearings, 1)
            c.after()
        end
    end

    -- What made the last level under a tag work, let go: its doors, and
    -- everything else standing inside its box (cubes and turrets taken away).
    local function unwire(lv: ZLevel)
        for _, k in lv.doors do
            doors[k] = nil
            timers[k] = nil
        end
        lv.doors = {}
        local b = lv.box
        if b == nil then return end
        local function inside(x: number, y: number, z: number): boolean
            return x >= b[1] and x <= b[4] + 1 and y >= b[2] - 1 and y <= b[5] + 1 and z >= b[3] and z <= b[6] + 1
        end
        local function keep<T>(list: { T }, at: (T) -> boolean): { T }
            local out = {}
            for _, e in list do
                if not at(e) then table.insert(out, e) end
            end
            return out
        end
        buttons = keep(buttons, function(e: Button) return inside(e.x, e.y, e.z) end)
        plates = keep(plates, function(e: Plate) return inside(e.x, e.y or FL, e.z) end)
        PUSHES = keep(PUSHES, function(e) return inside(e.x, e.y, e.z) end)
        pedestals = keep(pedestals, function(e: Pedestal) return inside(e.x, e.y or FL, e.z) end)
        TRIGGERS = keep(TRIGGERS, function(e: Trigger) return inside(e.x0, e.y0, e.z0) end)
        vents = keep(vents, function(e: Vent)
            if not inside(e.x, e.top, e.z) then return false end
            if e.cube ~= 0 then
                core.mob_remove(e.cube)
                cubes[e.cube] = nil
            end
            return true
        end)
        TURRETS = keep(TURRETS, function(e: Turret)
            if not inside(e.x, e.y or FL, e.z) then return false end
            if e.id ~= 0 then core.mob_remove(e.id) end
            return true
        end)
    end

    -- A level asked for: the last one under that tag let go and its cells
    -- taken away, then laid.
    local function lay_level(lv: ZLevel, x: number, y: number, z: number, count: number, seed: number)
        unwire(lv)
        lv.blocks, lv.marks, lv.box, lv.start, lv.exit, lv.done, lv.way = {}, {}, nil, nil, nil, false, nil
        clear_then(lv.tag, function()
            core.event.emit("zones:lay", { tag = lv.tag, world = "chambers", owner = "", x = x, y = y, z = z,
                                           seed = seed, d = lv.d, count = count })
        end)
    end

    -- A site's way out, beside the level outside its box (keys:start): a hole
    -- at (ex, ey, ez) facing +x, the wall at ex + 1 between it and a little room
    -- a portal from the level's exit comes out in, once it is done.
    local function way_out(lv: ZLevel)
        local box, st = lv.box :: { number }, lv.start :: { number }
        local ex, ey, ez = box[1] - 8, math.floor(st[2]), math.floor(st[3])
        for x = ex - 1, ex + 6 do
            for y = ey - 1, ey + 3 do
                for z = ez - 2, ez + 2 do
                    local inside = x >= ex + 2 and x <= ex + 5 and y >= ey and y <= ey + 2 and z >= ez - 1 and z <= ez + 1
                    core.set_block(x, y, z, if inside then 0 elseif y == ey + 3 and x == ex + 3 and z == ez then LIGHT
                        else METAL)
                end
            end
        end
        core.set_block(ex, ey, ez, 0)
        core.set_block(ex, ey + 1, ez, 0)
        lv.way = { ex, ey, ez }
        core.event.emit("keys:start", { player = lv.player, slot = lv.slot, ex = ex, ey = ey, ez = ez })
    end

    core.event.on("zones:laid", function(ev)
        local lv = zlevel(ev.tag)
        if ev.pieces == 0 then
            zlevels[ev.tag] = nil
            if lv.kind == "site" then
                site_by_copy({ player = lv.player, slot = lv.slot, d = lv.d })
            elseif lv.kind == "more" then
                core.notice(lv.player, "There are no more chambers yet: none of the workshops' has been approved.")
            end
            return
        end
        lv.box = { ev.x1, ev.y1, ev.z1, ev.x2, ev.y2, ev.z2 }
        core.storage_set(`box {ev.tag}`, `{ev.x1} {ev.y1} {ev.z1} {ev.x2} {ev.y2} {ev.z2}`)
        wire(lv)
        local st = lv.start :: { number }
        if lv.kind ~= "try" and core.player_name(lv.player) ~= "" then
            core.player_place(lv.player, st[1], st[2], st[3], 1, 0, 0)
            core.player_spawn(lv.player, st[1], st[2], st[3], true)
            local s = players[lv.player]
            if s then s.zl = lv.tag end
        end
        if lv.kind == "site" then
            way_out(lv)
            core.notice(lv.player, `Camere di prova: {ev.pieces}, fatte nelle officine.`)
        elseif lv.kind == "more" then
            core.notice(lv.player, `More chambers, difficulty {lv.d}: {ev.pieces} rooms.`)
        end
    end)

    -- A site's level: of zones when there are any (else site_by_copy, above).
    core.event.on("keys:level", function(ev)
        ev.cancel = true
        local lv = zlevel(`site {ev.slot}`)
        lv.kind, lv.player, lv.slot, lv.d = "site", ev.player, ev.slot, ev.d
        lay_level(lv, ev.ox, FL - 1, ev.oz, math.clamp(2 + ev.d // 2, 2, 7), ev.seed)
    end)

    -- The series past chamber 15: a level a player at a time, harder each one
    -- done, each player's in a place of their own.
    more_pressed = function(p: number)
        local s = players[p]
        if s == nil then return end
        local tag0 = core.storage_get(`more slot {s.name}`)
        local slot = tonumber(tag0)
        if slot == nil then
            slot = tonumber(core.storage_get("more next")) or 0
            core.storage_set("more next", tostring((slot :: number) + 1))
            core.storage_set(`more slot {s.name}`, tostring(slot))
        end
        local lv = zlevel(`more {slot}`)
        if lv.kind == "more" and lv.player == p and lv.box == nil then
            return   -- being laid
        end
        lv.kind, lv.player, lv.slot = "more", p, slot :: number
        lv.d = math.clamp(tonumber(core.storage_get(`more d {s.name}`)) or 1, 1, 10)
        core.notice(p, "The next chamber is being made...")
        lay_level(lv, -8192, FL - 1, (slot :: number) * 1024, math.clamp(2 + lv.d // 2, 2, 7), math.random(1, 1000000))
    end

    local function level_through(p: number, s: P, lv: ZLevel)
        lv.done = true
        core.log(`chambers_game: {s.name} through level {lv.tag} (d {lv.d})`)
        if lv.kind == "site" then
            core.event.emit("keys:cleared", { player = p, slot = lv.slot })
            local e, w = lv.exit :: { number }, lv.way
            if w ~= nil then
                local a = core.portal_open(e[1], e[2] + 1, e[3], 1, 1, 0, 0.5, 1.0)
                local b = core.portal_open(w[1] + 4.5, w[2] + 1, w[3] + 0.5, 1, -1, 0, 0.5, 1.0)
                if a >= 0 and b >= 0 then core.portal_link(a, b) end
            end
            core.notice(p, "Prova superata. Un portale porta all'uscita.")
        elseif lv.kind == "more" then
            local d = math.min(lv.d + 1, 10)
            core.storage_set(`more d {s.name}`, tostring(d))
            s.zl = nil
            local sx, sy, sz = start_of(#ROOMS)
            core.player_place(p, sx, sy, sz, 1, 0, 0)
            core.player_spawn(p, sx, sy, sz, true)
            core.notice(p, `Chamber done. The button again for the next, difficulty {d}.`)
        else
            core.notice(p, "The level is done.")
        end
    end

    -- Who is in which level, its exit reached, a fall back to its start; a
    -- level's turrets waking as somebody comes near.
    function step_levels(dt: number)
        step_clearings()
        for p, s in players do
            local x, y, z = core.player_pos(p)
            if x == nil then continue end
            if s.zl == nil then
                for tag, lv in zlevels do
                    local b = lv.box
                    if b ~= nil and x >= b[1] and x <= b[4] + 1 and z >= b[3] and z <= b[6] + 1 and y >= b[2] - 2
                        and y <= b[5] + 1 then
                        s.zl = tag
                    end
                end
            end
            local lv = if s.zl then zlevels[s.zl] else nil
            if lv == nil or lv.box == nil then
                s.zl = nil
                continue
            end
            local b, st = lv.box :: { number }, lv.start :: { number }
            local e = lv.exit
            if e ~= nil and not lv.done and math.abs(x - e[1]) < 1.2 and math.abs(z - e[3]) < 1.2
                and math.abs(y - e[2]) < 1.5 then
                level_through(p, s, lv)
                continue
            end
            s.low = if y < b[2] - 2 or on_pit_at(x, y, z) then s.low + dt else 0
            if s.low > 1.0 then
                s.low = 0
                core.player_place(p, st[1], st[2], st[3], 1, 0, 0)
                drop(p, false)
            end
            if x < b[1] - 12 or x > b[4] + 12 or z < b[3] - 12 or z > b[6] + 12 then
                s.zl = nil
            end
        end
        for _, t in TURRETS do
            if t.near and t.state == "idle" and t.id ~= 0 then
                local tx, ty, tz = core.mob_pos(t.id)
                if tx ~= nil then
                    for p in players do
                        local x, y, z = core.player_pos(p)
                        if x ~= nil and (x - tx) ^ 2 + (z - tz :: number) ^ 2 < 144 and math.abs(y - ty :: number) < 4 then
                            t.state = "awake"
                            core.notice(p, line("wake"))
                            break
                        end
                    end
                end
            end
        end
    end

    -- On each player and each cube whether it is on a button.
    local function on_button(b: Button, x: number, y: number, z: number): boolean
        return x >= b.x - 0.1 and x <= b.x + 2.1 and z >= b.z - 0.1 and z <= b.z + 2.1 and y >= b.y - 0.1
            and y <= b.y + 0.9
    end

    local function step_buttons()
        local held: { [number]: boolean } = {}   -- a door opens when all its buttons are down
        for _, b in buttons do
            local down = false
            for p in players do
                local x, y, z = core.player_pos(p)
                if x ~= nil and on_button(b, x, y, z) then
                    down = true
                end
            end
            for c in cubes do
                local x, y, z = core.mob_pos(c)
                if x ~= nil and on_button(b, x, y, z) then
                    down = true
                end
            end
            if down ~= b.pressed then
                b.pressed = down
                core.set_block(b.x, b.y, b.z, if down then DOWN else BUTTON)
            end
            held[b.door] = (held[b.door] ~= false) and down
        end
        for door, open in held do
            set_door(door, open)
        end
    end

    -- The doors that open as somebody comes up to them: the lobby's, and
    -- those of the rooms with nothing to hold theirs open (01, 03, 05, 09,
    -- 14 to 17); in a site's row, those of the same chambers.
    local NEAR_DOORS = { 1, 2, 4, 6, 10, 15, 16, 17, 18 }
    local function step_doors()
        local keys_near = table.clone(NEAR_DOORS)
        for slot, lay in layouts do
            for i, pl in lay.row do
                if table.find(NEAR_DOORS, pl.k) then table.insert(keys_near, i + 100 * slot) end
            end
        end
        for _, k in keys_near do
            local d = doors[k]
            if d ~= nil then
                local near = false
                for p in players do
                    local x, y, z = core.player_pos(p)
                    if x ~= nil and math.abs(x - d.x) < 3 and z >= DZ0 - 2 and z <= DZ1 + 3 and y < FL + 3 then
                        near = true
                    end
                end
                set_door(k, near)
            end
        end
        -- And a level's doors nothing else holds (levels of zones, below).
        for _, d in doors do
            local m = d.mid
            if d.near and m ~= nil then
                local near = false
                for p in players do
                    local x, y, z = core.player_pos(p)
                    if x ~= nil and math.abs(x - m[1]) < 3 and math.abs(z - m[3]) < 3 and math.abs(y - m[2]) < 2.5 then
                        near = true
                    end
                end
                d.open = near
            end
        end
    end

    -- Trigger zones: a player coming into a room is told its name and has its
    -- start for a respawn; a vent drops its cube as somebody comes into its
    -- room with none in it; falling into a pit is a fall back to the room's
    -- start, and a cube that falls in comes again out of its vent.
    local function step_zones(dt: number)
        for p, s in players do
            local x, y, _ = core.player_pos(p)
            if x == nil then
                continue
            end
            local k = room_of(x)
            local bk, bo = base_of(k)
            if k ~= 0 and k ~= s.room and x - bo >= ROOMS[bk].x0 then
                s.room = k
                if ROOMS[bk].name ~= "" then
                    core.notice(p, ROOMS[bk].name)
                end
                if k == #ROOMS then
                    core.event.emit("keys:earned", { player = p, key = "chambers" })
                end
                local lv = level_of[p]
                if lv and k == lv.done_at then level_done(p, lv) end
                local sx, sy, sz = start_of(k)
                core.player_spawn(p, sx, sy, sz)
                room_starts(k, p)
                for _, v in vents do
                    if v.room == k and (v.cube == 0 or core.mob_pos(v.cube) == nil) then
                        dispense(v)
                    end
                end
            end
            -- A second below the floor, not at once: the long fall's pit is
            -- a way through when a portal is at its bottom. A level of zones
            -- keeps its own (step_levels).
            if s.zl ~= nil then
                continue
            end
            s.low = if y < FL - 3 then s.low + dt else 0
            if s.low > 1.0 then
                s.low = 0
                local sx, sy, sz = start_of(if s.room ~= 0 then s.room else 1)
                core.player_place(p, sx, sy, sz, 1, 0, 0)
                drop(p, false)
            end
        end
        for _, v in vents do
            if v.cube ~= 0 then
                local _, cy = core.mob_pos(v.cube)
                if cy == nil or cy < (v.low or FL - 4) or on_pit(v.cube) then
                    dispense(v)
                end
            end
        end
    end

    local function step_plates()
        for p, s in players do
            local x, y, z = core.player_pos(p)
            if x == nil or clock < s.plate_at then
                continue
            end
            for _, pl in plates do
                if x >= pl.x and x <= pl.x + 2 and z >= pl.z and z <= pl.z + 2 and math.abs(y - (pl.y or FL)) < 0.3 then
                    core.player_launch(p, pl.vx, pl.vy, pl.vz, 2.0)
                    s.plate_at = clock + 1.0
                end
            end
        end
    end

    local shown_on: { [number]: number } = {}   -- the gun shown over each pedestal
    local function step_pedestals()
        -- Over each pedestal, hanging, its gun while somebody here has yet to
        -- take it (core.show_item).
        for i, pd in pedestals do
            local want = 0
            for _, s in players do
                if s.level < pd.level then want = core.item_id(pd.gives) end
            end
            if shown_on[i] ~= want and core.show_item(pd.x, (pd.y or FL) + 1, pd.z, want) then
                shown_on[i] = want
            end
        end
        for p, s in players do
            local x, y, z = core.player_pos(p)
            if x == nil then
                continue
            end
            for _, pd in pedestals do
                if (x - pd.x - 0.5) ^ 2 + (z - pd.z - 0.5) ^ 2 < 2.2 and math.abs(y - (pd.y or FL)) < 1
                    and s.level < pd.level then
                    s.level = pd.level
                    core.storage_set(`gun {s.name}`, tostring(pd.level))
                    do
                        if pd.takes then
                            core.take_item(p, pd.takes, 1)
                        end
                        core.give_item(p, pd.gives, 1)
                        core.notice(p, if pd.takes then "The portal gun: left is blue, right is orange."
                            else "The portal gun: left shoots blue, on white panels.")
                    end
                end
            end
        end
    end

    local function step_timer(dt: number)
        for door, left in timers do
            left -= dt
            if left <= 0 then
                timers[door] = nil
                set_door(door, false)
                tell(0, "clock 0")
            else
                timers[door] = left
            end
        end
    end

    local function step_carry()
        for p, s in players do
            if s.carry ~= 0 then
                local x, y, z = carry_point(p)
                if x == nil or core.mob_pos(s.carry) == nil then
                    s.carry = 0
                    tell(p, "hold -")
                else
                    core.mob_place(s.carry, x, y :: number, z :: number)
                end
            end
        end
    end

    function server_step(dt: number)
        if not fixed_open then
            open_fixed()
        end
        -- Who is here, who has gone: a player who leaves takes their portals.
        local here = {}
        for _, p in core.players() do
            here[p] = true
            if players[p] == nil then
                local name = core.player_name(p)
                players[p] = { carry = 0, room = 0, plate_at = 0, name = name,
                               level = tonumber(core.storage_get(`gun {name}`)) or 0, low = 0, said = 0 }
                crosshair(p)
                fields_told()
                D.told()
                for q, o in players do
                    if o.blue then rim_told(q, "blue", o.blue.at) end
                    if o.orange then rim_told(q, "orange", o.orange.at) end
                end
            end
        end
        for p, s in players do
            if not here[p] then
                close_shot(s.blue)
                close_shot(s.orange)
                rim_told(p, "blue", nil)
                rim_told(p, "orange", nil)
                drop(p, false)
                players[p] = nil
            end
        end
        step_turned()
        step_door_frames()
        step_fixed_orange()
        step_carry()
        step_buttons()
        step_doors()
        step_zones(dt)
        step_triggers()
        step_fields()
        step_turrets(dt)
        step_plates()
        step_pedestals()
        step_timer(dt)
        step_levels(dt)
        D.step(dt)
    end

    core.command("chamber", "<0..18> -- jump straight to a test chamber", "admin")
    core.event.on("core:command", function(ev)
        if ev.name ~= "chamber" then return end
        local n = tonumber(string.match(ev.args, "^%s*(%d+)"))
        if n == nil or n < 0 or n > #ROOMS - 1 then
            core.chat(ev.player, `/chamber <0..{#ROOMS - 1}>`)
            return
        end
        local s = players[ev.player]
        if s then s.zl = nil end
        local want_lvl = if n >= 4 then 2 elseif n == 3 then 1 else 0
        if want_lvl == 2 and core.bag_count(ev.player, "chambers_game:gun") == 0 then
            local blue = core.bag_count(ev.player, "chambers_game:gun_blue")
            if blue > 0 then core.take_item(ev.player, "chambers_game:gun_blue", blue) end
            core.give_item(ev.player, "chambers_game:gun", 1)
            if s then s.level = 2 end
        elseif want_lvl == 1 and core.bag_count(ev.player, "chambers_game:gun_blue") == 0
            and core.bag_count(ev.player, "chambers_game:gun") == 0 then
            core.give_item(ev.player, "chambers_game:gun_blue", 1)
            if s then s.level = 1 end
        end
        if n == 0 then
            core.player_place(ev.player, 4.5, FL, 4.5, 1, 0, 0)
        else
            local r = ROOMS[n + 1]
            core.player_place(ev.player, r.x0 + 1.5, FL, 6.0, 1, 0, 0)
            core.player_spawn(ev.player, r.x0 + 1.5, FL, 6.0, true)
        end
    end)
end

-- ================================================================ the player

if drawing then
    local blue_up, orange_up = false, false
    local holding = ""   -- the model of what this player carries, or ""
    -- Turrets' aim lines (by creature, until when), their shots (fading),
    -- and the dissolving fields.
    local aims: { [string]: { number } } = {}
    local shots: { { number } } = {}
    local fields: { [string]: { number } } = {}
    local now = 0.0
    local clock_left = 0.0
    local flash = 0.0

    local rims: { [string]: { number } } = {}
    -- Things a field is taking apart, each for DYING seconds: in this
    -- player's hands (hand, following the view) or where it was (x, y, z).
    type Dying = { model: string, at: number, hand: boolean, x: number, y: number, z: number }
    local dying: { Dying } = {}
    local DYING = 0.9
    local devices = Devices.client()
    heard = function(text: string)
        local w = string.split(text, " ")
        if devices.heard(w) then
            return
        elseif w[1] == "dissolve" then
            if holding ~= "" then
                table.insert(dying, { model = holding, at = now, hand = true, x = 0, y = 0, z = 0 })
            end
            holding = ""
        elseif w[1] == "zap" then
            table.insert(dying, { model = "chambers_game:" .. w[2], at = now, hand = false,
                                  x = tonumber(w[3]) or 0, y = tonumber(w[4]) or 0, z = tonumber(w[5]) or 0 })
        elseif w[1] == "hold" then
            holding = if w[2] == "-" then "" else "chambers_game:" .. w[2]
        elseif w[1] == "aim" or w[1] == "shot" then
            local v = {}
            for i = 1, 6 do
                v[i] = tonumber(w[i + (if w[1] == "aim" then 2 else 1)]) or 0
            end
            v[7] = now + (if w[1] == "aim" then 0.2 else 0.12)
            if w[1] == "aim" then aims[w[2]] = v else table.insert(shots, v) end
        elseif w[1] == "field" then
            fields[`{w[2]} {w[4]}`] = { tonumber(w[2]) or 0, tonumber(w[3]) or 0, tonumber(w[4]) or 0,
                                        tonumber(w[5]) or 0, tonumber(w[6]) or 0 }
        elseif w[1] == "rim" then
            local key = `{w[2]} {w[3]}`
            if w[4] == "-" then
                rims[key] = nil
            else
                local v = {}
                for i = 4, 11 do
                    v[i - 3] = tonumber(w[i]) or 0
                end
                v[9] = if w[3] == "blue" then 1 else 0
                rims[key] = v
            end
        elseif w[1] == "portals" then
            blue_up, orange_up = w[2] == "1", w[3] == "1"
        elseif w[1] == "clock" then
            clock_left = tonumber(w[2]) or 0
        elseif w[1] == "fizzle" then
            flash = 0.25
        end
    end

    if not serving then
        core.event.on("core:net_message", function(ev)
            heard(ev.text :: string)
        end)
    end

    -- Both buttons are this world's: never a dig, a blow, a block put down.
    local function ask(what: string, player: number)
        if serving then
            heard_click(player, what)
        else
            core.net.send(0, what)
        end
    end
    core.event.on("core:input", function(ev)
        if ev.action == "mouse_left" then
            ev.cancel = true
            ask("left", ev.player)
        elseif ev.action == "mouse_right" then
            ev.cancel = true
            ask("right", ev.player)
        end
    end)

    -- Each portal's rim, on the surface it was shot at: four bars round
    -- the opening (which stands in the air cell before a wall, or is the
    -- floor's node).
    core.render.pass("chambers_game:rims", 100, function()
        for _, v in rims do
            local x, y, z, axis, sign, hw, hh = v[1], v[2], v[3], v[4], v[5], v[6], v[7]
            local r, g, b = if v[9] == 1 then 40 else 255, if v[9] == 1 then 130 else 140, if v[9] == 1 then 255 else 20
            local t = 0.08
            if axis == 2 then
                local py = y + sign * 0.52
                core.render.box(x - hw, py, z, t, t, 2 * hh + t, r, g, b, 255)
                core.render.box(x + hw, py, z, t, t, 2 * hh + t, r, g, b, 255)
                core.render.box(x, py, z - hh, 2 * hw + t, t, t, r, g, b, 255)
                core.render.box(x, py, z + hh, 2 * hw + t, t, t, r, g, b, 255)
            elseif axis == 0 then
                local pz = z + sign * 0.42   -- on the wall's face, 0.4 out from the portal
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

    -- Where the hands hold a thing now: its feet, and the yaw of the view.
    local function in_hands(): (number?, number, number, number)
        -- This machine's player, as its own world code names it.
        local me = core.players()[1]
        if me == nil then
            return nil, 0, 0, 0
        end
        local x, y, z = core.player_pos(me)
        local dx, dy, dz = core.player_look(me)
        if x == nil or y == nil or z == nil or dx == nil or dy == nil or dz == nil then
            return nil, 0, 0, 0
        end
        local cy = math.max(y + 0.95, y + EYE + dy * 1.6)
        return x + dx * 1.6, cy - 0.35, z + dz * 1.6, math.atan2(dx, dz)
    end
    -- Where a dying thing is this frame, and how far through (0..1).
    local function dying_at(d: Dying): (number?, number, number, number, number)
        local k = (now - d.at) / DYING
        if d.hand then
            local x, y, z, yaw = in_hands()
            return x, y, z, yaw, k
        end
        return d.x, d.y, d.z, 0, k
    end
    -- The turrets' red aim lines and their shots, and the fields: a curtain
    -- of thin blue lightning across the room, flickering.
    core.render.pass("chambers_game:light", 100, function()
        for k, v in aims do
            if now > v[7] then
                aims[k] = nil
            else
                core.render.fx("bolt", v[1], v[2], v[3], 0.015, 255, 30, 20, 160, 0.3, v[4], v[5], v[6])
            end
        end
        local left = {}
        for _, v in shots do
            if now < v[7] then
                core.render.fx("bolt", v[1], v[2], v[3], 0.05, 255, 200, 80, 255, 1 - (v[7] - now) / 0.12,
                               v[4], v[5], v[6])
                table.insert(left, v)
            end
        end
        shots = left
        -- Lightning crawling over whatever a field is taking apart, sparks
        -- off it, and a blue glow that goes out with it.
        for _, d in dying do
            local x, y, z, _, k = dying_at(d)
            if x ~= nil and k < 1 then
                local cy = y + 0.35
                core.render.fx("glow", x, cy, z, 0.9 * (1 - k * 0.5), 90, 170, 255, math.floor(200 * (1 - k)), k)
                core.render.fx("sparks", x, cy, z, 0.6, 140, 200, 255, 255, k)
                for _ = 1, 5 do
                    local ax, ay, az = (math.random() - 0.5) * 0.8, (math.random() - 0.5) * 0.8, (math.random() - 0.5) * 0.8
                    local bx, by, bz = (math.random() - 0.5) * 0.9, (math.random() - 0.5) * 0.9, (math.random() - 0.5) * 0.9
                    core.render.fx("bolt", x + ax, cy + ay, z + az, 0.02, 150, 210, 255, math.floor(230 * (1 - k)),
                                   math.random(), x + bx, cy + by, z + bz)
                end
            end
        end
        for _, f in fields do
            local x = f[1] + 0.5
            local z = f[3]
            while z <= f[5] do
                local t = (now * 0.7 + z * 0.37) % 1
                core.render.fx("bolt", x, f[2], z, 0.04, 90, 170, 255, 70, t, x, f[4], z + 0.2)
                z += 0.5
            end
            core.render.fx("bolt", x, f[2] + 0.05, f[3], 0.06, 120, 200, 255, 150, (now * 1.3) % 1, x, f[2] + 0.05, f[5])
            core.render.fx("bolt", x, f[4] - 0.05, f[3], 0.06, 120, 200, 255, 150, (now * 1.1) % 1, x, f[4] - 0.05, f[5])
        end
    end)

    -- Looked up when first drawn: the world's model table comes after the
    -- mod loads.
    local drawn_ids: { [string]: number } = {}
    -- The cube in this player's hands, where their eye is this frame: the
    -- server's cube is left out of their world while they carry it (it
    -- trailed their view, jumping with every position the server heard).
    -- 1.6 ahead.
    local function model_of(name: string): number
        if (drawn_ids[name] or 0) == 0 then
            drawn_ids[name] = core.model_id(name)
        end
        return drawn_ids[name]
    end
    core.render.pass("chambers_game:held_cube", 100, function()
        if holding ~= "" then
            local x, y, z, yaw = in_hands()
            -- The cube's own model (Blender), its feet half its height down,
            -- turned with the view.
            if x ~= nil then core.render.model(model_of(holding), x, y, z, yaw) end
        end
        -- Coming apart: eaten in patches by the model's dissolve shader,
        -- shaking as the current goes through it.
        local left = {}
        for _, d in dying do
            local x, y, z, yaw, k = dying_at(d)
            if k < 1 then
                table.insert(left, d)
                if x ~= nil then
                    local j = 0.025 * (1 - k)
                    core.render.model(model_of(d.model), x + (math.random() - 0.5) * j, y + (math.random() - 0.5) * j,
                                      z + (math.random() - 0.5) * j, yaw + (math.random() - 0.5) * 0.06, 1, 0, 0,
                                      math.min(1, k * 1.15))
                end
            end
        end
        dying = left
    end)

    -- The crosshair: a ring of two halves, blue on the left and orange on the
    -- right, filled when that portal is up; red for a moment when a shot
    -- found nothing to open on. The clock at the top while it runs.
    core.hud.pass("chambers_game:hud", 100, function()
        local a = core.hud.aspect()
        local dx = 0.012 / a
        local function half(x: number, up: boolean, r: number, g: number, b: number)
            local k = if up then 255 else 110
            core.hud.rect(x, 0.5, 0.006, 0.024, r, g, b, k)
        end
        if flash > 0 then
            core.hud.rect(0.5, 0.5, 0.01, 0.01, 230, 40, 30, 255)
        else
            core.hud.rect(0.5, 0.5, 0.003, 0.003, 240, 240, 240, 220)
        end
        half(0.5 - dx, blue_up, 40, 130, 255)
        half(0.5 + dx, orange_up, 255, 140, 20)
        if clock_left > 0 then
            core.hud.text(0.5, 0.1, string.format("%05.2f", clock_left), 0.06)
        end
    end)

    function client_step(dt: number)
        now += dt
        devices.step(dt)
        if clock_left > 0 then
            clock_left = math.max(0, clock_left - dt)
        end
        if flash > 0 then
            flash -= dt
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
