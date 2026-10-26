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

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local FL  = 20    -- the floor a body stands on in every chamber
local EYE = 1.5   -- chambers_core's eye height

local clock = 0.0

local heard: (string) -> () = function(_) end
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

    local function id(name: string): number
        return core.node_id(`chambers_core:{name}`)
    end
    local PANEL, METAL, FLOOR, LIGHT, GLASS = id("panel"), id("metal"), id("floor"), id("light"), id("glass")
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
        { x0 = 245, x1 = 252, h = 5, name = "Testing complete. The portal home is on the west wall." },
    }
    local Z1 = 11
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
    local making: any = nil
    local function put(x: number, y: number, z: number, b: number)
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
        local m = making
        if m ~= nil then
            x0, x1 = math.max(x0, m.min_x), math.min(x1, m.max_x)
            z0, z1 = math.max(z0, m.min_z), math.min(z1, m.max_z)
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
    type Door = { x: number, open: boolean, want: boolean }
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
                put_turned(d.x, FL, DZ0, DOOR_FRAMES[d.frame + 1], 3)
                local open = d.frame == #DOOR_FRAMES - 1
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
    }
    type Vent = { x: number, z: number, top: number, room: number, cube: number }
    local vents: { Vent } = {
        { x = 33, z = 3, top = FL + 7, room = 3, cube = 0 },
        { x = 74, z = 9, top = FL + 8, room = 5, cube = 0 },
        { x = 140, z = 1, top = FL + 8, room = 8, cube = 0 },   -- 07, behind the glass
        { x = 162, z = 2, top = FL + 7, room = 9, cube = 0 },   -- 08, two
        { x = 164, z = 9, top = FL + 7, room = 9, cube = 0 },
    }
    type Plate = { x: number, z: number, vx: number, vy: number, vz: number }
    local plates: { Plate } = {
        { x = 93, z = 5, vx = 8, vy = 12, vz = 0 },
        { x = 102, z = 5, vx = 8, vy = 12, vz = 0 },
        { x = 185, z = 5, vx = 8, vy = 12, vz = 0 },   -- 09, as 05
        { x = 194, z = 5, vx = 8, vy = 12, vz = 0 },
    }
    -- A pedestal's gun is level 1 (blue) or 2 (both); a player's level is
    -- kept by name, so a gun is given once.
    type Pedestal = { x: number, z: number, gives: string, takes: string?, level: number }
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

    -- A floor button, two by two from its lower corner (x, z), which wears it.
    local function button(x: number, y: number, z: number)
        fill(x, x + 1, y, y, z, z + 1, BUTTON_PART)
        put(x, y, z, BUTTON)
    end

    local function build()
        for _, r in ROOMS do
            room(r)
        end
        -- Doorways and corridors between rooms, and the exit doors.
        for k = 1, #ROOMS - 1 do
            local a, b = ROOMS[k], ROOMS[k + 1]
            local cx = a.x1 + 2
            fill(cx, cx, FL - 1, FL - 1, DZ0 - 1, DZ1 + 1, FLOOR)
            fill(cx, cx, FL + 2, FL + 2, DZ0 - 1, DZ1 + 1, METAL)
            put(cx, FL + 2, DZ0, LIGHT)
            fill(cx, cx, FL, FL + 1, DZ0 - 1, DZ0 - 1, METAL)
            fill(cx, cx, FL, FL + 1, DZ1 + 1, DZ1 + 1, METAL)
            fill(cx, cx, FL, FL + 1, DZ0, DZ1, 0)
            fill(b.x0 - 1, b.x0 - 1, FL, FL + 1, DZ0, DZ1, 0)   -- the next room's doorway
            door_cells(a.x1 + 1, false)
            -- A green sign over each exit.
            fill(a.x1 + 1, a.x1 + 1, FL + 2, FL + 2, DZ0, DZ1, EXIT)
        end

        -- 01: the closed room behind a metal wall, and the fixed pair's rims.
        fill(21, 21, FL, FL + 7, 0, Z1, METAL)
        rim(16, -1, FL, true, RIM_BLUE)
        rim(24, -1, FL, true, RIM_ORANGE)
        recess(16, -1, FL, 4)
        recess(24, -1, FL, 4)

        -- 02: the vent in the ceiling, the button on the floor.
        put(33, FL + 8, 3, VENT)
        button(40, FL, 8)

        -- 03: the pedestal, white panels on both long walls before the
        -- chasm, the chasm, and the orange portal's rim past it.
        put(51, FL, 2, PEDESTAL)
        fill(52, 55, FL, FL + 3, -1, -1, PANEL)
        fill(52, 55, FL, FL + 3, Z1 + 1, Z1 + 1, PANEL)
        pit(57, 61)
        rim(64, Z1 + 1, FL, true, RIM_ORANGE)
        recess(64, Z1 + 1, FL, 5)

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
            put(p.x, FL - 1, p.z, PLATE)   -- drawn whole from here
        end

        -- 06: the push button, the clock over the exit, a pit between, white
        -- panels on each side of it.
        put(PUSHES[1].x, PUSHES[1].y, PUSHES[1].z, PUSH)
        fill(ROOMS[7].x1 + 1, ROOMS[7].x1 + 1, FL + 3, FL + 3, DZ0, DZ1, CLOCK)
        pit(121, 129)
        fill(117, 120, FL, FL + 4, -1, -1, PANEL)
        fill(130, 133, FL, FL + 4, Z1 + 1, Z1 + 1, PANEL)
        fill(130, 133, FL, FL + 4, -1, -1, PANEL)

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

        -- 08: two vents on the near side, a button there and one past a pit,
        -- white on the south wall before it and the north wall after.
        put(162, FL + 8, 2, VENT)
        put(164, FL + 8, 9, VENT)
        button(167, FL, 9)
        button(177, FL, 3)
        pit(169, 173)
        fill(161, 166, FL, FL + 3, -1, -1, PANEL)
        fill(175, 179, FL, FL + 3, Z1 + 1, Z1 + 1, PANEL)

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
            put(pl.x, FL - 1, pl.z, PLATE)
        end
        pit(205, 210)
        fill(201, 204, FL, FL + 3, -1, -1, PANEL)
        fill(211, 214, FL, FL + 3, Z1 + 1, Z1 + 1, PANEL)

        -- 10: the push button, a pit to the far side, white by the button
        -- and by the door, the clock over the door.
        put(220, FL, 2, PUSH)
        pit(222, 237)
        fill(219, 221, FL, FL + 3, -1, -1, PANEL)
        fill(238, 241, FL, FL + 3, Z1 + 1, Z1 + 1, PANEL)
        fill(ROOMS[11].x1 + 1, ROOMS[11].x1 + 1, FL + 3, FL + 3, DZ0, DZ1, CLOCK)

        -- The last room: the way home's rim.
        rim(ROOMS[#ROOMS].x0 - 1, 9, FL, false, RIM_BLUE)
        recess(ROOMS[#ROOMS].x0 - 1, 9, FL, 0)
    end

    -- The chambers are made as their columns are, a mapgen stage: lit with
    -- the column, there however far a player is.
    core.register_mapgen("chambers_game:chambers", function(ctx)
        if ctx.max_x < -2 or ctx.min_x > ROOMS[#ROOMS].x1 + 2 or ctx.max_z < -2 or ctx.min_z > 13 then
            return
        end
        making = ctx
        build()
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
    local CHAMBERS = "8"
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
    end)

    -- Nobody digs or builds in a test chamber.
    core.event.on("core:pre_break", function(ev)
        ev.cancel = true
    end)
    core.event.on("core:pre_place", function(ev)
        ev.cancel = true
    end)

    -- ---------------------------------------------------------- players

    type Shot = { x: number, y: number, z: number, idx: number, at: any }
    type P = { blue: Shot?, orange: Shot?, carry: number, room: number, plate_at: number, level: number,
               name: string }
    local players: { [number]: P } = {}

    local function room_of(x: number): number
        for k, r in ROOMS do
            if x >= r.x0 - 1 and x <= r.x1 + 2.5 then
                return k
            end
        end
        return 0
    end

    local function start_of(k: number): (number, number, number)
        local r = ROOMS[k]
        return r.x0 + 1.5, FL, 6.0
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

    local function solid(x: number, y: number, z: number): boolean
        return core.node_solid(core.get_block(x, y, z)) == 1
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
    local function aim(p: number): ({ x: number, y: number, z: number, axis: number, sign: number, hw: number, hh: number }?, string?)
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
            core.portal_link(o, best)
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
            core.portal_link(s.blue.idx, s.orange.idx)
        elseif one ~= nil then
            core.portal_link(one.idx, -1)
        end
        -- Whatever 03's orange led to may have just been taken from it.
        if one ~= nil and (one.idx == c3_linked or (s.orange and s.orange.idx == c3_linked)) then
            c3_linked = -1
        end
        step_fixed_orange()
    end

    -- Every player's portals are rimmed in their colour on every screen:
    -- "rim <player> <colour> x y z axis sign hw hh", or "-" when it closes.
    local function rim_told(p: number, colour: string, at: any?)
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

    local cubes: { [number]: boolean } = {}   -- every cube there is

    local function dispense(v: Vent)
        if v.cube ~= 0 then
            core.mob_remove(v.cube)
            cubes[v.cube] = nil
            for _, s in players do
                if s.carry == v.cube then
                    s.carry = 0
                end
            end
        end
        -- In the tube, which hangs from the ceiling a metre and a bit.
        v.cube = core.spawn_mob("chambers_game:cube", v.x + 0.5, v.top, v.z + 0.5)
        if v.cube ~= 0 then
            cubes[v.cube] = true
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
        for c in cubes do
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
        tell(p, "hold 0")
        if throw then
            local dx, dy, dz = core.player_look(p)
            if dx ~= nil then
                core.mob_velocity(c, dx * 9, dy * 9 + 2, dz * 9)
            end
        else
            core.mob_velocity(c, 0, 0, 0)
        end
    end

    -- ------------------------------------------------------------- clicks

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
        for _, b in PUSHES do
            if b.x == cx and b.y == cy and b.z == cz then
                timers[b.door] = b.time
                set_door(b.door, true)
                tell(0, `clock {b.time}`)
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
                s.carry = c
                -- Drawn by their own client at their eye, not trailing it:
                -- left out of their world while it is in their hands.
                core.mob_unseen(c, p)
                tell(p, "hold 1")
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
                end
            end
        end
        if all then
            for _, f in fixed do
                if f.pair then
                    core.portal_link(f.idx, fixed[f.pair].idx)
                end
            end
            core.portal_link(fixed.c3_orange.idx, -1)
            fixed_open = true
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
    -- those of the rooms with nothing to hold theirs open (01, 03, 05) and
    -- the last.
    local NEAR_DOORS = { 1, 2, 4, 6, 10 }
    local function step_doors()
        for _, k in NEAR_DOORS do
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
    end

    -- Trigger zones: a player coming into a room is told its name and has its
    -- start for a respawn; a vent drops its cube as somebody comes into its
    -- room with none in it; falling into a pit is a fall back to the room's
    -- start, and a cube that falls in comes again out of its vent.
    local function step_zones()
        for p, s in players do
            local x, y, z = core.player_pos(p)
            if x == nil then
                continue
            end
            local k = room_of(x)
            if k ~= 0 and k ~= s.room and x >= ROOMS[k].x0 then
                s.room = k
                if ROOMS[k].name ~= "" then
                    core.notice(p, ROOMS[k].name)
                end
                if k == #ROOMS then
                    core.event.emit("keys:earned", { player = p, key = "chambers" })
                end
                local sx, sy, sz = start_of(k)
                core.player_spawn(p, sx, sy, sz)
                for _, v in vents do
                    if v.room == k and (v.cube == 0 or core.mob_pos(v.cube) == nil) then
                        dispense(v)
                    end
                end
            end
            if y < FL - 3 then
                local sx, sy, sz = start_of(if s.room ~= 0 then s.room else 1)
                core.player_place(p, sx, sy, sz, 1, 0, 0)
                drop(p, false)
            end
        end
        for _, v in vents do
            if v.cube ~= 0 then
                local _, cy = core.mob_pos(v.cube)
                if cy == nil or cy < FL - 4 then
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
                if x >= pl.x and x <= pl.x + 2 and z >= pl.z and z <= pl.z + 2 and math.abs(y - FL) < 0.3 then
                    core.player_launch(p, pl.vx, pl.vy, pl.vz, 2.0)
                    s.plate_at = clock + 1.0
                end
            end
        end
    end

    local function step_pedestals()
        for p, s in players do
            local x, y, z = core.player_pos(p)
            if x == nil then
                continue
            end
            for _, pd in pedestals do
                if (x - pd.x - 0.5) ^ 2 + (z - pd.z - 0.5) ^ 2 < 2.2 and math.abs(y - FL) < 1
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
                    tell(p, "hold 0")
                else
                    core.mob_place(s.carry, x, y, z)
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
                               level = tonumber(core.storage_get(`gun {name}`)) or 0 }
                crosshair(p)
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
        step_zones()
        step_plates()
        step_pedestals()
        step_timer(dt)
    end
end

-- ================================================================ the player

if drawing then
    local blue_up, orange_up = false, false
    local holding = false
    local clock_left = 0.0
    local flash = 0.0

    local rims: { [string]: { number } } = {}
    heard = function(text: string)
        local w = string.split(text, " ")
        if w[1] == "hold" then
            holding = w[2] == "1"
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
            heard(ev.text)
        end)
    end

    -- Both buttons are this world's: never a dig, a blow, a block put down.
    local function ask(what: string, player: number)
        if serving then
            (heard_click :: any)(player, what)
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

    -- Looked up when first drawn: the world's model table comes after the
    -- mod loads.
    local CUBE_DRAWN = 0
    -- The cube in this player's hands, where their eye is this frame: the
    -- server's cube is left out of their world while they carry it (it
    -- trailed their view, jumping with every position the server heard).
    -- 1.6 ahead.
    core.render.pass("chambers_game:held_cube", 100, function()
        -- This machine's player, as its own world code names it.
        local me = core.players()[1]
        if not holding or me == nil then
            return
        end
        local x, y, z = core.player_pos(me)
        local dx, dy, dz = core.player_look(me)
        if x == nil or dx == nil then
            return
        end
        local cx, cy, cz = x + dx * 1.6, math.max(y + 0.95, y + EYE + dy * 1.6), z + dz * 1.6
        -- The cube's own model (Blender), its feet half its height down,
        -- turned with the view.
        if CUBE_DRAWN == 0 then
            CUBE_DRAWN = core.model_id("chambers_game:cube")
        end
        core.render.model(CUBE_DRAWN, cx, cy - 0.35, cz, math.atan2(dx, dz))
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
