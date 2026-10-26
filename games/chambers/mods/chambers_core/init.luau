--!strict
-- chambers_core: the blocks of a world of test chambers in the manner of
-- classic portal puzzle games (DESIGN.md M13). White panels a portal takes and dark metal it does
-- not, tiled floors, lights, glass; the floor button and the door it opens,
-- the vent a cube drops out of, the launch plate, the clock and its
-- button, the pedestal a gun waits on. chambers_game builds the chambers out
-- of them and makes them work.

local function node(name: string, rgb: { number }, light: number?): number
    return core.register_node(`chambers_core:{name}`, rgb[1], rgb[2], rgb[3], true, light or 0, "", 0, 0, "metal")
end

node("panel", { 226, 228, 230 })     -- portals go on these, and only these
node("metal", { 112, 118, 126 })
node("floor", { 132, 134, 138 })
node("light", { 236, 244, 250 }, 15)
-- The automatic door: two cells wide, two tall, one deep, drawn whole from
-- its lower-left cell (door_shut.gltf, door_open.gltf, Blender:
-- tools/make_chambers_models.py) and turned with it (upright); its other
-- three cells draw nothing and only stop a body. Shut, two leaves meeting in
-- the middle of its depth; open, the frame alone.
local HIDDEN = core.register_model("chambers_core:hidden", "hidden.gltf")
local function door(name: string, model: number, open: boolean): number
    local id = core.register_node(`chambers_core:{name}`, 150, 154, 160, not open, 0, "upright", 0, 0, "metal")
    core.node_model(id, model)
    if not open then
        core.node_box(id, 0, 0, 0.42, 1, 1, 0.58)
    end
    return id
end
local SHUT, OPEN = core.register_model("chambers_core:door_shut", "door_shut.gltf"),
    core.register_model("chambers_core:door_open", "door_open.gltf")
core.node_door(door("door", SHUT, false), door("door_open", OPEN, true))
core.node_door(door("door_part", HIDDEN, false), door("door_open_part", HIDDEN, true))
-- And between, the leaves a fifth, two, three and four fifths into the wall:
-- a block's model does not move, so chambers_game steps a door's lower-left
-- cell through these. Shut to a body until it is open.
for k = 1, 4 do
    door(`door_{k}`, core.register_model(`chambers_core:door_{k}`, `door_{k}.gltf`), false)
end

-- The vent a cube drops out of: a ceiling cell wearing a tube that hangs into
-- the room (vent.gltf).
core.node_model(node("vent", { 200, 204, 208 }), core.register_model("chambers_core:vent", "vent.gltf"))
node("clock", { 20, 20, 22 }, 6)
node("exit", { 40, 160, 70 }, 8)
node("rim_blue", { 40, 130, 255 }, 10)
node("rim_orange", { 255, 140, 20 }, 10)
core.node_glass(node("glass", { 190, 220, 235 }), 60)

-- Flat things a body stands on or beside: the floor button (and pressed), the
-- launch plate, the push button on its stalk, the pedestal.
-- Their models are Blender's (tools/make_chambers_models.py).
local function model(name: string): number
    return core.register_model(`chambers_core:{name}`, `{name}.gltf`)
end
-- The floor button is two cells by two as the plate is, drawn whole from its
-- lower corner (up or pressed); its other three cells draw nothing. All four
-- are its plinth to a body, as high up as pressed: nothing on it moves as it
-- goes down.
local function plinth(name: string, rgb: { number }, drawn: number): number
    local id = node(name, rgb)
    core.node_box(id, 0.0, 0.0, 0.0, 1.0, 0.15, 1.0)
    core.node_model(id, drawn)
    return id
end
plinth("button", { 210, 40, 30 }, model("button_up"))
plinth("button_down", { 120, 30, 24 }, model("button_down"))
plinth("button_part", { 226, 228, 230 }, HIDDEN)
-- The plate is two cells by two, drawn whole from its lower corner; the other
-- three are floor that draws nothing.
core.node_model(node("plate", { 214, 180, 40 }), model("plate"))
core.node_model(node("plate_part", { 214, 180, 40 }), HIDDEN)
local PUSH = node("pushbutton", { 224, 226, 228 })
core.node_model(PUSH, model("pushbutton"))
core.node_box(PUSH, 0.35, 0.0, 0.35, 0.65, 0.85, 0.65)
core.node_box(PUSH, 0.25, 0.85, 0.25, 0.75, 1.0, 0.75)
local PEDESTAL = node("pedestal", { 200, 204, 208 })
core.node_box(PEDESTAL, 0.2, 0.0, 0.2, 0.8, 0.9, 0.8)

-- A panel a portal is in: a shell a sixteenth thick, open on its front (+Z
-- before it is turned, wallmounted: the front points out of the face the
-- portal was shot at), the portal inside it. A wall's portal is two of them,
-- the high one with a roof; the low one has no floor, the cell under it is the
-- hole's floor, flush with the room's (a lip a sixteenth high there stopped a
-- body walking in). A floor's or a ceiling's is one, its four sides closed.
-- Not drawn: only a body is stopped by them. The opening's own doorway sides
-- show the far end there, as the engine draws any doorway; drawn, they stood
-- in front of that picture inside the opening.
local T = 1 / 16
local function shell(name: string, low: boolean, high: boolean)
    local id = core.register_node(`chambers_core:{name}`, 226, 228, 230, true, 0, "wallmounted", 0, 0, "metal")
    core.node_model(id, HIDDEN)
    core.node_box(id, 0, 0, 0, 1, 1, T)          -- the back
    core.node_box(id, 0, 0, 0, T, 1, 1)          -- the sides
    core.node_box(id, 1 - T, 0, 0, 1, 1, 1)
    if low then
        core.node_box(id, 0, 0, 0, 1, T, 1)      -- the floor of the hole
    end
    if high then
        core.node_box(id, 0, 1 - T, 0, 1, 1, 1)  -- its roof
    end
end
shell("shell_low", false, false)
shell("shell_high", false, true)
shell("shell_one", true, true)
-- A floor's or a ceiling's portal is two cells long, and two of those would
-- stand a wall between them: its cells are the back alone (the floor round
-- the hole is its sides).
local FLAT = core.register_node("chambers_core:shell_flat", 226, 228, 230, true, 0, "wallmounted", 0, 0, "metal")
core.node_box(FLAT, 0, 0, 0, 1, 1, T)
core.node_model(FLAT, HIDDEN)

-- Nothing of the engine's terrain: the chambers hang in nothing.
core.mapgen_core(false)
core.world_sky(40, 44, 50)

-- A player is the sandbox's robot (mods/robot, linked here as games/arpg's),
-- seen from the eye, the gun in its hands.
core.player_model(core.register_model("chambers_core:robot", "robot.gltf"))
-- The extras a player may pick to wear (tools/make_robot_extras.py): the
-- client draws "<player model>_<extra>" on their skeleton in their colours.
for _, extra in {"cap", "tophat", "headphones", "scarf", "bowtie", "medal",
                  -- and who a player may be instead (tools/make_species.py): the
                  -- client draws "<player model>_<species>", its arms and styles.
                  "human", "human_arms", "human_short", "human_long", "human_beard",
                  "mole", "mole_arms", "mole_lamp", "mole_goggles", "mole_glasses",
                  "frog", "frog_arms", "frog_spots", "frog_crown", "frog_lily",
                  -- the extras refitted to each species, worn instead of the robot's
                  "human_cap", "human_tophat", "human_headphones", "human_scarf", "human_bowtie", "human_medal",
                  "mole_cap", "mole_tophat", "mole_headphones", "mole_scarf", "mole_bowtie", "mole_medal",
                  "frog_cap", "frog_tophat", "frog_headphones", "frog_scarf", "frog_bowtie", "frog_medal"} do
    core.register_model("chambers_core:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
core.register_model("chambers_core:robot_arms", "robot_arms.gltf")
core.eye_height(1.5)

-- The sandbox's walk and run and jump; no flying.
core.movement(4.8, 8, 0, 8, -25)
