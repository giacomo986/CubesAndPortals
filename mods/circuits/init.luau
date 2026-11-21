--!strict
-- circuits: wiring and logic on top of mods/cinnabar.
--
-- ALLOY WIRE carries power without fading, as far as it is laid. INSULATED WIRE
-- comes in sixteen colours and joins only its own colour and alloy, so lines
-- can run side by side. A CABLE carries sixteen signals at once: each colour of
-- insulated wire that touches it is its own channel. A RISER is alloy going
-- straight up. GATES are flat tiles: the output is the side away from whoever
-- put one down, and the inputs are the other three. A LAMP lights in its colour.
--
-- Wire, insulated wire and cable go on floors, walls and ceilings: a wire lies
-- on the face of the block it was put against, and runs on round an inside
-- corner (a wall wire above a floor wire against the same wall) and over an
-- outside one (a floor wire at the edge of a block and a wall wire on its side).
--
-- BUS GATES are gates for a cable: AND, OR, XOR and NOT of the sixteen channels
-- one by one, out into the cable at their output. A plain signal at an input
-- (a wire, a lever) counts as all sixteen channels, so an AND with a lever is a
-- switch for a whole bus.
--
-- The CHIP BENCH saves a circuit as a CHIP: a blank chip used on the bench and
-- then on the far corner of the circuit copies the box between them. The box's
-- INPUT PINS and OUTPUT PINS, one a side, are the chip's sides; put down, a chip
-- runs a copy of the circuit inside it.
--
-- With cinnabar: a lever, button, plate, torch, repeater or comparator drives a
-- wire. Alloy wire gives power to the dust, lamps, repeaters and pistons beside
-- it, and a gate gives it out of its output. Gates and lamps read lit dust, but
-- dust does not drive a wire: dust beside a wire it was lit by would hold it on
-- for ever, so dust goes into a wire through a repeater or a buffer.
--
-- Nothing here is saved but the blocks and their meta, and the chips' designs
-- in storage: a wire's state is which block it is, and every change is worked
-- out again from the world.
local TICK = 0.1
local PULSE = 0.25
local MAX_NET = 4096  -- ponytail: a net past this is left part done; ask for a limit by world if one is hit
local ALL16 = 0xFFFF
local CHIP_W, CHIP_H = 16, 8   -- the biggest box a chip holds, across and high

local COLOURS: { { any } } = {
    { "white", 234, 236, 236 }, { "orange", 240, 118, 20 }, { "magenta", 190, 68, 180 },
    { "light_blue", 58, 176, 218 }, { "yellow", 248, 198, 40 }, { "lime", 112, 186, 26 },
    { "pink", 238, 142, 172 }, { "gray", 62, 68, 72 }, { "light_gray", 142, 142, 136 },
    { "cyan", 22, 138, 146 }, { "purple", 122, 42, 172 }, { "blue", 52, 56, 158 },
    { "brown", 114, 72, 40 }, { "green", 84, 110, 28 }, { "red", 160, 38, 34 },
    { "black", 20, 20, 24 },
}

-- +Z, +X, -Z, -X: the order a cell's quarter turns go round, so a shape turned
-- by the facing byte is its bits rotated.
local SIDES = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }
local ALL6 = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }
local SHAPES: { { any } } = { { "none", 0 }, { "end", 1 }, { "line", 5 }, { "corner", 3 }, { "tee", 7 }, { "cross", 15 } }
local FIT: { [number]: { shape: number, turn: number } } = {}
for i, s in SHAPES do
    for turn = 0, 3 do
        local m = bit32.band(bit32.bor(bit32.lshift(s[2], turn), bit32.rshift(s[2], 4 - turn)), 15)
        if not FIT[m] then
            FIT[m] = { shape = i, turn = turn }
        end
    end
end

-- A wire's facing byte is "facedir": axis * 4 + turn, the axis saying where the
-- wire's own up points (engine's media/face_tiles.h): 0 +Y (a floor), 1 +Z,
-- 2 -Z, 3 +X, 4 -X (walls), 5 -Y (a ceiling). 0..3 are the four upright turns
-- a floor wire always had, so a world laid before walls keeps its wires.
local UP = { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, -1, 0 } }
local ROT = {
    { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } },
    { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } },
    { { 1, 0, 0 }, { 0, 0, 1 }, { 0, -1, 0 } },
    { { 0, 1, 0 }, { -1, 0, 0 }, { 0, 0, 1 } },
    { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
    { { 1, 0, 0 }, { 0, -1, 0 }, { 0, 0, -1 } },
}
-- The four arms of a wire on each axis, in the world: SIDES turned by the tilt.
local ARMS: { { { number } } } = {}
for a, m in ROT do
    ARMS[a] = {}
    for i, s in SIDES do
        local v = { s[1], 0, s[2] }
        ARMS[a][i] = {
            m[1][1] * v[1] + m[1][2] * v[2] + m[1][3] * v[3],
            m[2][1] * v[1] + m[2][2] * v[2] + m[2][3] * v[3],
            m[3][1] * v[1] + m[3][2] * v[2] + m[3][3] * v[3],
        }
    end
end
local function axis_of_up(x: number, y: number, z: number): number
    for a, u in UP do
        if u[1] == x and u[2] == y and u[3] == z then
            return a - 1
        end
    end
    return 0
end

type Wire = { kind: string, colour: number, on: boolean, shape: number, riser: boolean,
              pair: { { off: number, on: number } } }
local WIRE: { [number]: Wire } = {}

local function wire_boxes(id: number, mask: number, w: number, h: number)
    local a, b = 0.5 - w, 0.5 + w
    core.node_box(id, a, 0, a, b, h, b)
    if bit32.btest(mask, 1) then core.node_box(id, a, 0, b, b, h, 1) end
    if bit32.btest(mask, 2) then core.node_box(id, b, 0, a, 1, h, b) end
    if bit32.btest(mask, 4) then core.node_box(id, a, 0, 0, b, h, a) end
    if bit32.btest(mask, 8) then core.node_box(id, 0, 0, a, a, h, b) end
end

local function brighter(v: number): number
    return math.floor(math.min(255, v + (255 - v) * 0.45))
end

-- Six shapes, each off and on; a cable has no state of its own, only channels.
local function wire_kind(prefix: string, kind: string, colour: number, r: number, g: number, b: number,
                         w: number, h: number): number
    local pair: { { off: number, on: number } } = {}
    for i, s in SHAPES do
        local off = core.register_node(if i == 1 then prefix else `{prefix}_{s[1]}`, r, g, b, false, 0, "facedir", 0, 0.1, "")
        local on = off
        if kind ~= "cable" then
            on = core.register_node(`{prefix}_on_{s[1]}`, brighter(r), brighter(g), brighter(b), false, 0, "facedir", 0, 0.1, "")
        end
        pair[i] = { off = off, on = on }
        for _, id in { off, on } do
            wire_boxes(id, s[2], w, h)
            WIRE[id] = { kind = kind, colour = colour, on = id ~= off, shape = i, riser = false, pair = pair }
        end
    end
    for i = 1, #SHAPES do
        for _, id in { pair[i].off, pair[i].on } do
            if id ~= pair[1].off then
                core.node_drop(id, pair[1].off)
            end
        end
    end
    return pair[1].off
end

local ALLOY = wire_kind("circuits:alloy", "alloy", 0, 130, 40, 30, 1 / 16, 1 / 16)
local CABLE = wire_kind("circuits:cable", "cable", 0, 70, 70, 80, 3 / 16, 3 / 16)
for i, c in COLOURS do
    wire_kind(`circuits:wire_{c[1]}`, "wire", i, c[2], c[3], c[4], 2 / 16, 2 / 16)
end

-- A riser: alloy going straight up, stacked; it joins wire on its own level.
local RISER = core.register_node("circuits:alloy_riser", 130, 40, 30, false, 0, "", 0, 0.1, "")
local RISER_ON = core.register_node("circuits:alloy_on_riser", 255, 90, 60, false, 0, "", 0, 0.1, "")
do
    local pair = { { off = RISER, on = RISER_ON } }
    for _, id in { RISER, RISER_ON } do
        core.node_box(id, 0.4375, 0, 0.4375, 0.5625, 1, 0.5625)
        WIRE[id] = { kind = "alloy", colour = 0, on = id == RISER_ON, shape = 1, riser = true, pair = pair }
    end
    core.node_drop(RISER_ON, RISER)
end

-- The gates: name, recipe's top row. Logic below, by name.
local GATE_LIST = {
    { "buffer", "- ore:cinnabar -" }, { "not", "- cinnabar:torch -" },
    { "or", "ore:cinnabar ore:cinnabar ore:cinnabar" }, { "nor", "cinnabar:torch ore:cinnabar ore:cinnabar" },
    { "and", "ore:cinnabar cinnabar:torch ore:cinnabar" }, { "nand", "cinnabar:torch cinnabar:torch ore:cinnabar" },
    { "xor", "timber:stick ore:cinnabar timber:stick" }, { "xnor", "timber:stick cinnabar:torch timber:stick" },
    { "rs_latch", "ore:cinnabar cinnabar:repeater ore:cinnabar" }, { "toggle", "cinnabar:torch cinnabar:repeater cinnabar:torch" },
    { "d_latch", "timber:stick cinnabar:repeater timber:stick" }, { "pulse", "- cinnabar:repeater -" },
    { "timer", "furnace:iron_ingot cinnabar:repeater furnace:iron_ingot" },
    { "counter", "furnace:iron_ingot ore:cinnabar furnace:iron_ingot" },
    { "random", "furnace:iron_ingot cinnabar:torch furnace:iron_ingot" },
    { "light", "- core:glass -" },
}
type Gate = { kind: string, on: boolean, off_id: number, on_id: number }
local GATE: { [number]: Gate } = {}
for _, g in GATE_LIST do
    local off = core.register_node(`circuits:gate_{g[1]}`, 60, 90, 70, true, 0, "upright", 0, 0.2, "")
    local on = core.register_node(`circuits:gate_on_{g[1]}`, 90, 130, 80, true, 0, "upright", 0, 0.2, "")
    for _, id in { off, on } do
        core.node_box(id, 0, 0, 0, 1, 0.125, 1)
        GATE[id] = { kind = g[1], on = id == on, off_id = off, on_id = on }
    end
    core.node_drop(on, off)
end

-- Bus gates: the same flat tile, the same sides, sixteen channels at a time.
-- What is out is in the cell's meta "o", the channels as bits; the block is
-- the lit one while any of them is.
local BUS_LIST = { "and", "or", "xor", "not" }
local BUS: { [number]: Gate } = {}
for _, n in BUS_LIST do
    local off = core.register_node(`circuits:bus_gate_{n}`, 50, 70, 100, true, 0, "upright", 0, 0.2, "")
    local on = core.register_node(`circuits:bus_gate_on_{n}`, 80, 110, 150, true, 0, "upright", 0, 0.2, "")
    for _, id in { off, on } do
        core.node_box(id, 0, 0, 0, 1, 0.125, 1)
        BUS[id] = { kind = n, on = id == on, off_id = off, on_id = on }
    end
    core.node_drop(on, off)
end

local LAMP: { [number]: { off: number, on: number } } = {}
for _, c in COLOURS do
    local off = core.register_node(`circuits:lamp_{c[1]}`, math.floor(c[2] / 2), math.floor(c[3] / 2), math.floor(c[4] / 2), true, 0, "", 0, 0.3, "")
    local on = core.register_node(`circuits:lamp_on_{c[1]}`, c[2], c[3], c[4], true, 15, "", 0, 0.3, "")
    LAMP[off] = { off = off, on = on }
    LAMP[on] = { off = off, on = on }
    core.node_drop(on, off)
end

-- Pins: a chip's sides, inside the circuit it was made of. An input pin gives
-- power while the chip's side is given it (and, in the world, while a hand has
-- switched it on, to try the circuit); an output pin lights like a lamp, and
-- the chip gives out of its side what it shows.
local function flat(name: string, r: number, g: number, b: number, light: number): number
    local id = core.register_node(name, r, g, b, true, light, "", 0, 0.2, "")
    core.node_box(id, 0.125, 0, 0.125, 0.875, 0.1875, 0.875)
    return id
end
local PIN_IN = flat("circuits:pin_in", 60, 140, 70, 0)
local PIN_IN_ON = flat("circuits:pin_in_on", 120, 230, 120, 7)
local PIN_OUT = flat("circuits:pin_out", 140, 70, 50, 0)
local PIN_OUT_ON = flat("circuits:pin_out_on", 250, 140, 90, 7)
core.node_drop(PIN_IN_ON, PIN_IN)
core.node_drop(PIN_OUT_ON, PIN_OUT)
local PIN: { [number]: boolean } = { [PIN_IN] = true, [PIN_IN_ON] = true, [PIN_OUT] = true, [PIN_OUT_ON] = true }

-- A chip put down; the item is circuits:chip, numbered (core.held_id) by its
-- design. Broken it gives a blank chip, which the hand gets back numbered.
local CHIP_OFF = core.register_node("circuits:chip_block", 40, 44, 50, true, 0, "upright", 0, 0.3, "")
local CHIP_ON = core.register_node("circuits:chip_block_on", 60, 66, 80, true, 0, "upright", 0, 0.3, "")
local CHIP: { [number]: boolean } = { [CHIP_OFF] = true, [CHIP_ON] = true }
for id in CHIP do
    core.node_box(id, 0.0625, 0, 0.0625, 0.9375, 0.25, 0.9375)
    core.node_drop_item(id, "circuits:blank_chip")
end
local BENCH = core.register_node("circuits:chip_bench", 120, 100, 80, true, 0, "", 0, 1.0, "wood")

-- ---------------------------------------------------------------- what cinnabar has
local function cid(name: string): number
    return core.node_id(name)
end
local GIVES_ALL: { [number]: boolean } = { [PIN_IN_ON] = true }
for _, n in { "cinnabar:lever_on", "cinnabar:button_on", "cinnabar:torch", "cinnabar:plate_on", "compact:cinnabar" } do
    GIVES_ALL[cid(n)] = true
end
local REPEATER_ON, COMPARATOR_ON, OBSERVER_ON = cid("cinnabar:repeater_on"), cid("cinnabar:comparator_on"), cid("cinnabar:observer_on")
local DUST_LIT: { [number]: boolean } = { [cid("cinnabar:dust_on")] = true }
local JOINS: { [number]: boolean } = {}   -- what a gate input or a wire's arm reaches
for _, n in { "cinnabar:lever", "cinnabar:lever_on", "cinnabar:button", "cinnabar:button_on", "cinnabar:torch",
              "cinnabar:torch_off", "cinnabar:plate", "cinnabar:plate_on", "compact:cinnabar", "cinnabar:repeater",
              "cinnabar:repeater_on", "cinnabar:comparator", "cinnabar:comparator_on", "cinnabar:dust", "cinnabar:dust_on",
              "cinnabar:lamp", "cinnabar:lamp_on", "cinnabar:observer", "cinnabar:observer_on" } do
    JOINS[cid(n)] = true
end
for a = 0, 2 do for b = 0, 2 do for c = 0, 2 do for d = 0, 2 do
    local off, on = cid(`cinnabar:dust_{a}{b}{c}{d}`), cid(`cinnabar:dust_on_{a}{b}{c}{d}`)
    if on ~= 0 then
        DUST_LIT[on] = true
        JOINS[on] = true
    end
    if off ~= 0 then
        JOINS[off] = true
    end
end end end end
for id in GATE do JOINS[id] = true end
for id in BUS do JOINS[id] = true end
for id in LAMP do JOINS[id] = true end
for id in PIN do JOINS[id] = true end
for id in CHIP do JOINS[id] = true end
JOINS[0] = nil
-- What a cable's arm reaches besides wire: only what reads or writes a bus.
local CABLE_JOINS: { [number]: boolean } = {}
for id in BUS do CABLE_JOINS[id] = true end

-- Told once, so cinnabar draws dust into these and takes their power.
for _, s in SHAPES do
    core.event.emit("cinnabar:family", { name = `circuits:alloy_on_{s[1]}`, find = "circuits:alloy_on_*", way = "all" })
    core.event.emit("cinnabar:family", { name = if s[1] == "none" then "circuits:alloy" else `circuits:alloy_{s[1]}`, find = "", way = "join" })
end
core.event.emit("cinnabar:family", { name = "circuits:alloy_on_riser", find = "circuits:alloy_on_*", way = "all" })
core.event.emit("cinnabar:family", { name = "circuits:alloy_riser", find = "", way = "join" })
for _, g in GATE_LIST do
    core.event.emit("cinnabar:family", { name = `circuits:gate_on_{g[1]}`, find = "circuits:gate_on_*", way = "out" })
    core.event.emit("cinnabar:family", { name = `circuits:gate_{g[1]}`, find = "", way = "join" })
end
core.event.emit("cinnabar:family", { name = "circuits:pin_in_on", find = "circuits:pin_in_on*", way = "all" })
for _, n in { "circuits:pin_in", "circuits:pin_out", "circuits:pin_out_on", "circuits:chip_block", "circuits:chip_block_on" } do
    core.event.emit("cinnabar:family", { name = n, find = "", way = "join" })
end

local function k(x: number, y: number, z: number): string
    return `{x},{y},{z}`
end

-- ---------------------------------------------------------------- a world
-- Everything below works on a Sim: the world itself, or the copy of a circuit
-- that runs inside a chip, which is a few tables and no blocks anybody sees.
type Due = { x: number, y: number, z: number, on: boolean, at: number }
type BusDue = { x: number, y: number, z: number, mask: number, at: number }
type Sim = {
    real: boolean,
    get: (number, number, number) -> number,
    set: (number, number, number, number) -> (),
    facing: (number, number, number) -> number,
    set_facing: (number, number, number, number) -> (),
    mget: (number, number, number, string) -> string,
    mset: (number, number, number, string, string) -> (),
    light: (number, number, number) -> number,
    dirty: { [string]: { number } },
    pending: { [string]: Due },
    bus_pending: { [string]: BusDue },
    pulses: { [string]: { x: number, y: number, z: number, at: number } },
    elapsed: { [string]: number },
    -- A cable cell's sixteen channels as last solved. Kept in memory and not in
    -- meta (a long cable would spend the world's meta cells): a cell not here is
    -- solved when a bus gate first reads it.
    bus: { [string]: number },
}

local function new_sim(real: boolean): Sim
    return {
        real = real,
        get = function(_x: number, _y: number, _z: number): number return 0 end,
        set = function(_x: number, _y: number, _z: number, _id: number) end,
        facing = function(_x: number, _y: number, _z: number): number return 0 end,
        set_facing = function(_x: number, _y: number, _z: number, _f: number) end,
        mget = function(_x: number, _y: number, _z: number, _key: string): string return "" end,
        mset = function(_x: number, _y: number, _z: number, _key: string, _v: string) end,
        light = function(_x: number, _y: number, _z: number): number return 0 end,
        dirty = {}, pending = {}, bus_pending = {}, pulses = {}, elapsed = {}, bus = {},
    }
end

local REAL = new_sim(true)
REAL.get = function(x: number, y: number, z: number): number return core.get_block(x, y, z) end
REAL.set = function(x: number, y: number, z: number, id: number) core.set_block(x, y, z, id) end
REAL.facing = function(x: number, y: number, z: number): number return core.get_facing(x, y, z) end
REAL.set_facing = function(x: number, y: number, z: number, f: number) core.set_facing(x, y, z, f) end
REAL.mget = function(x: number, y: number, z: number, key: string): string return core.meta_get(x, y, z, key) end
REAL.mset = function(x: number, y: number, z: number, key: string, v: string) core.meta_set(x, y, z, key, v) end
REAL.light = function(x: number, y: number, z: number): number return core.block_light(x, y, z) end

-- A box w by h by d of cells, 0-based, with air all round it.
local function virtual_sim(w: number, h: number, d: number, light: () -> number): Sim
    local s = new_sim(false)
    local blocks: { [string]: number } = {}
    local facings: { [string]: number } = {}
    local metas: { [string]: { [string]: string } } = {}
    local function inside(x: number, y: number, z: number): boolean
        return x >= 0 and y >= 0 and z >= 0 and x < w and y < h and z < d
    end
    s.get = function(x: number, y: number, z: number): number return blocks[k(x, y, z)] or 0 end
    s.set = function(x: number, y: number, z: number, id: number)
        if inside(x, y, z) then
            local at = k(x, y, z)
            blocks[at] = if id == 0 then nil else id
            facings[at] = nil   -- as the world's: a block set is facing 0
            if id == 0 then metas[at] = nil end
        end
    end
    s.facing = function(x: number, y: number, z: number): number return facings[k(x, y, z)] or 0 end
    s.set_facing = function(x: number, y: number, z: number, f: number)
        if inside(x, y, z) then facings[k(x, y, z)] = f end
    end
    s.mget = function(x: number, y: number, z: number, key: string): string
        local m = metas[k(x, y, z)]
        return if m then m[key] or "" else ""
    end
    s.mset = function(x: number, y: number, z: number, key: string, v: string)
        if not inside(x, y, z) then return end
        local at = k(x, y, z)
        local m = metas[at] or {}
        m[key] = if v == "" then nil else v
        metas[at] = m
    end
    s.light = function(_x: number, _y: number, _z: number): number return light() end
    return s
end

local FRONT = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }
local function front_of(s: Sim, x: number, y: number, z: number): (number, number)
    local f = FRONT[s.facing(x, y, z) % 4 + 1]
    return f[1], f[2]
end

-- Which of SIDES t is from n (1..4), or nil when not beside it on its level.
local function side_to(nx: number, ny: number, nz: number, tx: number, ty: number, tz: number): number?
    if ny ~= ty then return nil end
    for i, d in SIDES do
        if nx + d[1] == tx and nz + d[2] == tz then return i end
    end
    return nil
end

local function out_mask(s: Sim, x: number, y: number, z: number): number
    return tonumber(s.mget(x, y, z, "o")) or 0
end

-- Whether the block at n gives power into the cell t beside it.
local function gives_into(s: Sim, nx: number, ny: number, nz: number, tx: number, ty: number, tz: number): boolean
    local id = s.get(nx, ny, nz)
    if GIVES_ALL[id] then
        return true
    end
    if id == REPEATER_ON or id == COMPARATOR_ON or (GATE[id] and GATE[id].on) or (BUS[id] and BUS[id].on) then
        local fx, fz = front_of(s, nx, ny, nz)
        return ny == ty and nx - fx == tx and nz - fz == tz
    end
    if id == OBSERVER_ON then
        local d = ALL6[s.facing(nx, ny, nz) % 6 + 1]
        return nx + d[1] == tx and ny + d[2] == ty and nz + d[3] == tz
    end
    if id == CHIP_ON then
        -- Its meta "o" is the sides it gives out of, already turned into the world's.
        local i = side_to(nx, ny, nz, tx, ty, tz)
        return i ~= nil and bit32.btest(out_mask(s, nx, ny, nz), bit32.lshift(1, i - 1))
    end
    return false
end

-- Whether a gate or lamp at t reads power from the block at n: all of the above,
-- and lit dust, and a lit wire.
local function reads(s: Sim, nx: number, ny: number, nz: number, tx: number, ty: number, tz: number): boolean
    local id = s.get(nx, ny, nz)
    local w = WIRE[id]
    return DUST_LIT[id] or (w ~= nil and w.on) or gives_into(s, nx, ny, nz, tx, ty, tz)
end

-- ---------------------------------------------------------------- shapes
local function links(a: Wire, b: Wire): boolean
    if a.kind == "cable" then return b.kind == "cable" or b.kind == "wire" end
    if b.kind == "cable" then return a.kind == "wire" end
    if a.kind == "wire" and b.kind == "wire" then return a.colour == b.colour end
    return true
end

local function axis_at(s: Sim, x: number, y: number, z: number): number
    return s.facing(x, y, z) // 4
end

local function same(v: { number }, x: number, y: number, z: number): boolean
    return v[1] == x and v[2] == y and v[3] == z
end

-- Every wire cell this one touches, by which of its arms (1..4; 0 for a
-- riser's up and down). On one face, straight on; round an INSIDE corner, the
-- wire against the wall above a floor wire (or the wall wire sat at the foot of
-- its wall, whose arm comes down to the floor wire beside it); over an OUTSIDE
-- corner, the wire on the side of the block whose top this one lies on.
type Link = { x: number, y: number, z: number, w: Wire, arm: number }
local function linked(s: Sim, x: number, y: number, z: number, w: Wire): { Link }
    local out: { Link } = {}
    if w.riser then
        for i, d in SIDES do
            local nx, nz = x + d[1], z + d[2]
            local n = WIRE[s.get(nx, y, nz)]
            if n and n.riser then
                table.insert(out, { x = nx, y = y, z = nz, w = n, arm = i })
            elseif n then
                for _, arm in ARMS[axis_at(s, nx, y, nz) + 1] do
                    if same(arm, -d[1], 0, -d[2]) then
                        table.insert(out, { x = nx, y = y, z = nz, w = n, arm = i })
                        break
                    end
                end
            end
        end
        for dy = -1, 1, 2 do
            local n = WIRE[s.get(x, y + dy, z)]
            if n and n.riser then
                table.insert(out, { x = x, y = y + dy, z = z, w = n, arm = 0 })
            end
        end
        return out
    end
    local a = axis_at(s, x, y, z) + 1
    local u = UP[a]
    for i, d in ARMS[a] do
        local bx, by, bz = x + d[1], y + d[2], z + d[3]
        local n = WIRE[s.get(bx, by, bz)]
        if n and n.riser then
            if d[2] == 0 then
                table.insert(out, { x = bx, y = by, z = bz, w = n, arm = i })
            end
        elseif n then
            local ub = UP[axis_at(s, bx, by, bz) + 1]
            if same(ub, u[1], u[2], u[3]) or same(ub, -d[1], -d[2], -d[3]) then
                table.insert(out, { x = bx, y = by, z = bz, w = n, arm = i })
            end
        end
        local ox, oy, oz = bx - u[1], by - u[2], bz - u[3]
        n = WIRE[s.get(ox, oy, oz)]
        if n and not n.riser and same(UP[axis_at(s, ox, oy, oz) + 1], d[1], d[2], d[3]) then
            table.insert(out, { x = ox, y = oy, z = oz, w = n, arm = i })
        end
        local ix, iy, iz = x + u[1], y + u[2], z + u[3]
        n = WIRE[s.get(ix, iy, iz)]
        if n and not n.riser and same(UP[axis_at(s, ix, iy, iz) + 1], -d[1], -d[2], -d[3]) then
            table.insert(out, { x = ix, y = iy, z = iz, w = n, arm = i })
        end
    end
    return out
end

local function reshape(s: Sim, x: number, y: number, z: number)
    local id = s.get(x, y, z)
    local w = WIRE[id]
    if not w or w.riser then
        return
    end
    local axis = axis_at(s, x, y, z)
    local mask = 0
    for _, l in linked(s, x, y, z, w) do
        if links(w, l.w) then
            mask = bit32.bor(mask, bit32.lshift(1, l.arm - 1))
        end
    end
    for i, d in ARMS[axis + 1] do
        local nid = s.get(x + d[1], y + d[2], z + d[3])
        if not WIRE[nid] and (if w.kind == "cable" then CABLE_JOINS[nid] else JOINS[nid]) then
            mask = bit32.bor(mask, bit32.lshift(1, i - 1))
        end
    end
    local fit = FIT[mask]
    local pair = w.pair[fit.shape]
    local want = if w.on then pair.on else pair.off
    local facing = axis * 4 + fit.turn
    if want ~= id then
        s.set(x, y, z, want)
    end
    if want ~= id or s.facing(x, y, z) ~= facing then
        s.set_facing(x, y, z, facing)
    end
end

local function reshape_around(s: Sim, x: number, y: number, z: number)
    for dy = -1, 1 do for dz = -1, 1 do for dx = -1, 1 do
        reshape(s, x + dx, y + dy, z + dz)
    end end end
end

-- ---------------------------------------------------------------- what is due
local clock = 0

local function watched(id: number): boolean
    return WIRE[id] ~= nil or GATE[id] ~= nil or BUS[id] ~= nil or LAMP[id] ~= nil or PIN[id] ~= nil or CHIP[id] ~= nil
end

local function touch(s: Sim, x: number, y: number, z: number)
    local function one(cx: number, cy: number, cz: number)
        if watched(s.get(cx, cy, cz)) then
            s.dirty[k(cx, cy, cz)] = { cx, cy, cz }
        end
    end
    one(x, y, z)
    for _, d in ALL6 do
        one(x + d[1], y + d[2], z + d[3])
    end
end

-- A cell put or taken: a wire round an outside corner is a diagonal away.
local function touch_around(s: Sim, x: number, y: number, z: number)
    for dy = -1, 1 do for dz = -1, 1 do for dx = -1, 1 do
        local cx, cy, cz = x + dx, y + dy, z + dz
        if watched(s.get(cx, cy, cz)) then
            s.dirty[k(cx, cy, cz)] = { cx, cy, cz }
        end
    end end end
end

local function tell_cinnabar(s: Sim, x: number, y: number, z: number)
    if not s.real then
        return
    end
    for _, d in ALL6 do
        local name = core.node_name(core.get_block(x + d[1], y + d[2], z + d[3]))
        if string.sub(name, 1, 9) == "cinnabar:" or string.sub(name, 1, 6) == "doors:" then
            core.event.emit("cinnabar:changed", { x = x, y = y, z = z })
            return
        end
    end
end

-- ---------------------------------------------------------------- a net
local function driven(s: Sim, x: number, y: number, z: number): boolean
    for _, d in ALL6 do
        if gives_into(s, x + d[1], y + d[2], z + d[3], x, y, z) then
            return true
        end
    end
    return false
end

-- Whether a bus gate beside this cable cell puts `channel` into it.
local function bus_into(s: Sim, x: number, y: number, z: number, channel: number): boolean
    for _, d in SIDES do
        local nx, nz = x + d[1], z + d[2]
        local g = BUS[s.get(nx, y, nz)]
        if g and g.on then
            local fx, fz = front_of(s, nx, y, nz)
            if nx - fx == x and nz - fz == z and bit32.btest(out_mask(s, nx, y, nz), bit32.lshift(1, channel - 1)) then
                return true
            end
        end
    end
    return false
end

-- Every wire joined to this one, and on if anything drives any of them. A cable
-- is walked once per channel; `ctx` is the colour in hand while inside it.
local function solve_net(s: Sim, sx: number, sy: number, sz: number, ctx: number, done: { [string]: boolean })
    local seen: { [string]: boolean } = {}
    local cells: { { number } } = {}
    local cables: { { number } } = {}
    local stack: { { number } } = { { sx, sy, sz, ctx } }
    local on, count = false, 0
    while #stack > 0 do
        local c = table.remove(stack) :: { number }
        local x, y, z, here = c[1], c[2], c[3], c[4]
        local w = WIRE[s.get(x, y, z)]
        local at = k(x, y, z) .. (if w and w.kind == "cable" then `:{here}` else "")
        if w and not seen[at] then
            seen[at] = true
            count += 1
            if count > MAX_NET then
                if s.real then
                    core.log(`circuits: a net wider than {MAX_NET} cells at {sx} {sy} {sz}, left part done`)
                end
                return
            end
            if w.kind ~= "cable" then
                table.insert(cells, c)
                on = on or driven(s, x, y, z)
            elseif here > 0 then
                table.insert(cables, c)
                on = on or bus_into(s, x, y, z, here)
            end
            for _, l in linked(s, x, y, z, w) do
                local n = l.w
                if links(w, n) then
                    if n.kind == "cable" then
                        table.insert(stack, { l.x, l.y, l.z, if w.kind == "cable" then here else w.colour })
                    elseif w.kind ~= "cable" or n.colour == here then
                        table.insert(stack, { l.x, l.y, l.z, 0 })
                    end
                end
            end
        end
    end
    for _, c in cells do
        local x, y, z = c[1], c[2], c[3]
        done[k(x, y, z)] = true
        local id = s.get(x, y, z)
        local w = WIRE[id]
        if w.on ~= on then
            local pair = w.pair[w.shape]
            local facing = s.facing(x, y, z)
            s.set(x, y, z, if on then pair.on else pair.off)
            s.set_facing(x, y, z, facing)
            touch(s, x, y, z)
            if w.kind == "alloy" then
                tell_cinnabar(s, x, y, z)
            end
        end
    end
    for _, c in cables do
        local x, y, z, channel = c[1], c[2], c[3], c[4]
        local at = k(x, y, z)
        done[`{at}:{channel}`] = true
        local was = s.bus[at] or 0
        local now = if on then bit32.bor(was, bit32.lshift(1, channel - 1)) else bit32.band(was, bit32.bnot(bit32.lshift(1, channel - 1)))
        s.bus[at] = now
        if now ~= was then
            for _, d in SIDES do
                local nx, nz = x + d[1], z + d[2]
                if BUS[s.get(nx, y, nz)] then
                    s.dirty[k(nx, y, nz)] = { nx, y, nz }
                end
            end
        end
    end
end

-- A cable cell's channels; solved now if nobody has yet.
local function bus_of(s: Sim, x: number, y: number, z: number): number
    local at = k(x, y, z)
    if s.bus[at] == nil then
        local done: { [string]: boolean } = {}
        for channel = 1, #COLOURS do
            solve_net(s, x, y, z, channel, done)
        end
    end
    return s.bus[at] or 0
end

-- ---------------------------------------------------------------- a gate
local function bit(s: Sim, x: number, y: number, z: number, key: string): boolean
    return s.mget(x, y, z, key) == "1"
end
local function set_bit(s: Sim, x: number, y: number, z: number, key: string, v: boolean)
    s.mset(x, y, z, key, if v then "1" else "")
end
local function number_of(s: Sim, x: number, y: number, z: number, key: string, default: number): number
    return tonumber(s.mget(x, y, z, key)) or default
end

local function want_out(s: Sim, x: number, y: number, z: number, g: Gate, on: boolean)
    local at = k(x, y, z)
    if on == g.on then
        s.pending[at] = nil
    elseif not s.pending[at] or s.pending[at].on ~= on then
        s.pending[at] = { x = x, y = y, z = z, on = on, at = clock + TICK }
    end
end

local function start_pulse(s: Sim, x: number, y: number, z: number)
    s.pulses[k(x, y, z)] = { x = x, y = y, z = z, at = clock + PULSE }
end

local function eval_gate(s: Sim, x: number, y: number, z: number)
    local g = GATE[s.get(x, y, z)]
    if not g then
        return
    end
    local fx, fz = front_of(s, x, y, z)
    local function input(dx: number, dz: number): (boolean, boolean)
        local nx, nz = x + dx, z + dz
        local nid = s.get(nx, y, nz)
        local w = WIRE[nid]
        return reads(s, nx, y, nz, x, y, z), (w ~= nil and w.kind ~= "cable") or JOINS[nid] == true
    end
    local a, pa = input(fx, fz)
    local l, pl = input(-fz, fx)
    local r, pr = input(fz, -fx)
    -- What each input was last time, for the gates that act on a rising edge.
    local rise_a = a and not bit(s, x, y, z, "a")
    local rise_l = l and not bit(s, x, y, z, "l")
    local rise_r = r and not bit(s, x, y, z, "r")
    set_bit(s, x, y, z, "a", a)
    set_bit(s, x, y, z, "l", l)
    set_bit(s, x, y, z, "r", r)

    local any = a or l or r
    -- An input with nothing at it does not count against an AND.
    local all = (pa or pl or pr) and (a or not pa) and (l or not pl) and (r or not pr)
    local odd = ((if a then 1 else 0) + (if l then 1 else 0) + (if r then 1 else 0)) % 2 == 1
    local kind, out = g.kind, false
    if kind == "buffer" then out = a
    elseif kind == "not" then out = not a
    elseif kind == "or" then out = any
    elseif kind == "nor" then out = not any
    elseif kind == "and" then out = all
    elseif kind == "nand" then out = not all
    elseif kind == "xor" then out = odd
    elseif kind == "xnor" then out = not odd
    elseif kind == "rs_latch" then
        out = (l or bit(s, x, y, z, "q")) and not r
        set_bit(s, x, y, z, "q", out)
    elseif kind == "toggle" then
        out = bit(s, x, y, z, "q") ~= rise_a
        set_bit(s, x, y, z, "q", out)
    elseif kind == "d_latch" then
        out = if l or r then a else bit(s, x, y, z, "q")
        set_bit(s, x, y, z, "q", out)
    elseif kind == "pulse" then
        if rise_a then start_pulse(s, x, y, z) end
        out = s.pulses[k(x, y, z)] ~= nil
    elseif kind == "timer" then
        out = s.pulses[k(x, y, z)] ~= nil
    elseif kind == "counter" then
        local most = number_of(s, x, y, z, "m", 8)
        local n = math.clamp(number_of(s, x, y, z, "n", 0) + (if rise_l then 1 else 0) - (if rise_r then 1 else 0), 0, most)
        s.mset(x, y, z, "n", tostring(n))
        out = n >= most
    elseif kind == "random" then
        out = a and g.on
    elseif kind == "light" then
        out = s.light(x, y + 1, z) >= 8
    end
    want_out(s, x, y, z, g, out)
end

-- A bus gate: each input is a cable's sixteen channels, another bus gate's
-- output, or a plain signal standing for all sixteen.
local function eval_bus(s: Sim, x: number, y: number, z: number)
    local g = BUS[s.get(x, y, z)]
    if not g then
        return
    end
    local fx, fz = front_of(s, x, y, z)
    local function input(dx: number, dz: number): (number, boolean)
        local nx, nz = x + dx, z + dz
        local nid = s.get(nx, y, nz)
        local w = WIRE[nid]
        if w and w.kind == "cable" then
            return bus_of(s, nx, y, nz), true
        end
        local other = BUS[nid]
        if other then
            local ofx, ofz = front_of(s, nx, y, nz)
            if nx - ofx == x and nz - ofz == z then
                return out_mask(s, nx, y, nz), true
            end
        end
        if reads(s, nx, y, nz, x, y, z) then
            return ALL16, true
        end
        return 0, w ~= nil or JOINS[nid] == true
    end
    local a, pa = input(fx, fz)
    local l, pl = input(-fz, fx)
    local r, pr = input(fz, -fx)
    local out = 0
    if g.kind == "not" then
        out = bit32.band(bit32.bnot(a), ALL16)
    else
        local first = true
        for _, v in { { a, pa }, { l, pl }, { r, pr } } do
            if v[2] then
                local m = v[1] :: number
                if first then
                    out, first = m, false
                elseif g.kind == "and" then
                    out = bit32.band(out, m)
                elseif g.kind == "or" then
                    out = bit32.bor(out, m)
                else
                    out = bit32.bxor(out, m)
                end
            end
        end
    end
    local at = k(x, y, z)
    if out == out_mask(s, x, y, z) then
        s.bus_pending[at] = nil
    elseif not s.bus_pending[at] or s.bus_pending[at].mask ~= out then
        s.bus_pending[at] = { x = x, y = y, z = z, mask = out, at = clock + TICK }
    end
end

local function set_bus(s: Sim, x: number, y: number, z: number, mask: number)
    local id = s.get(x, y, z)
    local g = BUS[id]
    if not g then
        return
    end
    s.mset(x, y, z, "o", if mask == 0 then "" else tostring(mask))
    local want = if mask ~= 0 then g.on_id else g.off_id
    if want ~= id then
        local facing = s.facing(x, y, z)
        s.set(x, y, z, want)
        s.set_facing(x, y, z, facing)
        s.mset(x, y, z, "o", if mask == 0 then "" else tostring(mask))
    end
    touch(s, x, y, z)
    tell_cinnabar(s, x, y, z)
end

-- A lamp, and an output pin, which is one: lit by anything beside it.
local function eval_lamp(s: Sim, x: number, y: number, z: number)
    local id = s.get(x, y, z)
    local off, on
    if LAMP[id] then
        off, on = LAMP[id].off, LAMP[id].on
    elseif id == PIN_OUT or id == PIN_OUT_ON then
        off, on = PIN_OUT, PIN_OUT_ON
    else
        return
    end
    local lit = false
    for _, d in ALL6 do
        if reads(s, x + d[1], y + d[2], z + d[3], x, y, z) then
            lit = true
            break
        end
    end
    local want = if lit then on else off
    if want ~= id then
        s.set(x, y, z, want)
    end
end

local function set_gate(s: Sim, x: number, y: number, z: number, on: boolean)
    local g = GATE[s.get(x, y, z)]
    if not g or g.on == on then
        return
    end
    local facing = s.facing(x, y, z)
    s.set(x, y, z, if on then g.on_id else g.off_id)
    s.set_facing(x, y, z, facing)
    touch(s, x, y, z)
    tell_cinnabar(s, x, y, z)
end

-- The timer's and the random gate's beat, every 0.25 s: the world's from an
-- ABM, a chip's from its own tick.
local function timer_beat(s: Sim, x: number, y: number, z: number)
    local at = k(x, y, z)
    local e = (s.elapsed[at] or 0) + 0.25
    local fx, fz = front_of(s, x, y, z)
    local paused = reads(s, x + fx, y, z + fz, x, y, z)
    if not paused and e >= number_of(s, x, y, z, "p", 1) then
        e = 0
        start_pulse(s, x, y, z)
        touch(s, x, y, z)
    end
    s.elapsed[at] = e
end
local function random_beat(s: Sim, x: number, y: number, z: number)
    local g = GATE[s.get(x, y, z)]
    if not g then
        return
    end
    local fx, fz = front_of(s, x, y, z)
    want_out(s, x, y, z, g, reads(s, x + fx, y, z + fz, x, y, z) and math.random() < 0.5)
end

-- ---------------------------------------------------------------- chips
-- A design: the box, the side the maker stood at (0..3, SIDES), every cell of
-- the circuit "x y z name facing m p", and its pins.
type Pin = { x: number, y: number, z: number, side: number, out: boolean }
type Design = { w: number, h: number, d: number, front: number, cells: { { any } }, pins: { Pin } }
type Chip = { x: number, y: number, z: number, id: number, rot: number, design: Design, sim: Sim,
              beat: number, out: number }
local designs: { [number]: Design } = {}
local chips: { [string]: Chip } = {}
local PART = 3800   -- a storage value is at most 4096 bytes

-- Which side of a w by d box a cell is on, 0..3 (SIDES), or nil for none or two.
local function pin_side(lx: number, lz: number, w: number, d: number): number?
    local found: number? = nil
    local n = 0
    if lz == d - 1 then found, n = 0, n + 1 end
    if lx == w - 1 then found, n = 1, n + 1 end
    if lz == 0 then found, n = 2, n + 1 end
    if lx == 0 then found, n = 3, n + 1 end
    return if n == 1 then found else nil
end

local function load_design(id: number): Design?
    if designs[id] then
        return designs[id]
    end
    local w, h, d, front, parts = string.match(core.storage_get(`chip {id}`), "^(%d+) (%d+) (%d+) (%d) (%d+)$")
    if w == nil then
        return nil
    end
    local text = {}
    for p = 1, tonumber(parts) :: number do
        table.insert(text, core.storage_get(`chip {id} {p}`))
    end
    local des: Design = { w = tonumber(w) :: number, h = tonumber(h) :: number, d = tonumber(d) :: number,
                          front = tonumber(front) :: number, cells = {}, pins = {} }
    for cell in string.gmatch(table.concat(text), "[^;]+") do
        local x, y, z, name, f, m, p = string.match(cell, "^(%d+) (%d+) (%d+) (%S+) (%d+) (%S*) ?(%S*)$")
        if x then
            local lx, ly, lz = tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number
            table.insert(des.cells, { lx, ly, lz, name, tonumber(f), m or "", p or "" })
            local nid = core.node_id(name :: string)
            if PIN[nid] then
                local side = pin_side(lx, lz, des.w, des.d)
                if side then
                    table.insert(des.pins, { x = lx, y = ly, z = lz, side = side, out = nid == PIN_OUT or nid == PIN_OUT_ON })
                end
            end
        end
    end
    designs[id] = des
    return des
end

-- What of a box goes into a chip: this mod's blocks and cinnabar's.
local function kept(name: string): boolean
    return string.sub(name, 1, 9) == "circuits:" or string.sub(name, 1, 9) == "cinnabar:" or name == "compact:cinnabar"
end

-- Saves the box as a design: its number, or nil and why not.
local function save_design(x1: number, y1: number, z1: number, x2: number, y2: number, z2: number,
                           front: number): (number?, string)
    local w, h, d = x2 - x1 + 1, y2 - y1 + 1, z2 - z1 + 1
    if w > CHIP_W or d > CHIP_W or h > CHIP_H then
        return nil, `At most {CHIP_W} by {CHIP_H} high by {CHIP_W}.`
    end
    local cells: { string } = {}
    local taken: { [number]: boolean } = {}
    local pins, left_out = 0, 0
    for y = y1, y2 do
        for z = z1, z2 do
            for x = x1, x2 do
                local id = core.get_block(x, y, z)
                local name = core.node_name(id)
                if CHIP[id] or id == BENCH then
                    left_out += 1
                elseif id ~= 0 and kept(name) then
                    if PIN[id] then
                        local side = pin_side(x - x1, z - z1, w, d)
                        if side == nil then
                            return nil, "A pin goes on one side of the box: not inside it, nor on a corner."
                        end
                        if taken[side] then
                            return nil, "One pin a side."
                        end
                        taken[side] = true
                        pins += 1
                        -- An input pin goes in off: the chip's side says when it is on.
                        if id == PIN_IN_ON then name = "circuits:pin_in" end
                    end
                    local m, p = core.meta_get(x, y, z, "m"), core.meta_get(x, y, z, "p")
                    table.insert(cells, `{x - x1} {y - y1} {z - z1} {name} {core.get_facing(x, y, z)} {m} {p}`)
                end
            end
        end
    end
    if pins == 0 then
        return nil, "No pins: put an input or an output pin on a side of the box."
    end
    local id = tonumber(core.storage_get("next chip")) or 1
    if id > 131071 then
        return nil, "No more chips can be made in this world."
    end
    core.storage_set("next chip", tostring(id + 1))
    local text = table.concat(cells, ";")
    local parts = math.max(1, math.ceil(#text / PART))
    for p = 1, parts do
        core.storage_set(`chip {id} {p}`, string.sub(text, (p - 1) * PART + 1, p * PART))
    end
    core.storage_set(`chip {id}`, `{w} {h} {d} {front} {parts}`)
    designs[id] = nil
    local why = `A chip of {#cells} blocks, {pins} pins.`
    if left_out > 0 then
        why ..= " Chips and benches inside are left out."
    end
    return id, why
end

-- A chip in the world, running: made when it is first touched, and again after
-- a restart (its inside starts from the design; what it gives out was kept).
local function chip_at(x: number, y: number, z: number): Chip?
    local at = k(x, y, z)
    local c = chips[at]
    if c then
        return c
    end
    local id = tonumber(core.meta_get(x, y, z, "chip")) or 0
    local des = load_design(id)
    if des == nil then
        return nil
    end
    local sim = virtual_sim(des.w, des.h, des.d, function(): number return core.block_light(x, y + 1, z) end)
    for _, cell in des.cells do
        local cx, cy, cz = cell[1] :: number, cell[2] :: number, cell[3] :: number
        sim.set(cx, cy, cz, core.node_id(cell[4] :: string))
        sim.set_facing(cx, cy, cz, cell[5] :: number)
        if cell[6] ~= "" then sim.mset(cx, cy, cz, "m", cell[6] :: string) end
        if cell[7] ~= "" then sim.mset(cx, cy, cz, "p", cell[7] :: string) end
    end
    for _, cell in des.cells do
        touch(sim, cell[1] :: number, cell[2] :: number, cell[3] :: number)
    end
    c = { x = x, y = y, z = z, id = id, rot = (core.get_facing(x, y, z) - des.front) % 4, design = des, sim = sim,
          beat = 0, out = tonumber(core.meta_get(x, y, z, "o")) or 0 }
    chips[at] = c
    return c
end

-- The chip's sides in: each input pin on while its side is given power.
local function eval_chip(x: number, y: number, z: number)
    local c = chip_at(x, y, z)
    if c == nil then
        return
    end
    for _, p in c.design.pins do
        if not p.out then
            local d = SIDES[(p.side + c.rot) % 4 + 1]
            local want = if reads(REAL, x + d[1], y, z + d[2], x, y, z) then PIN_IN_ON else PIN_IN
            if c.sim.get(p.x, p.y, p.z) ~= want then
                c.sim.set(p.x, p.y, p.z, want)
                touch(c.sim, p.x, p.y, p.z)
            end
        end
    end
end

-- And out: what the output pins show, as the world's sides.
local function chip_outputs(c: Chip)
    local mask = 0
    for _, p in c.design.pins do
        if p.out and c.sim.get(p.x, p.y, p.z) == PIN_OUT_ON then
            mask = bit32.bor(mask, bit32.lshift(1, (p.side + c.rot) % 4))
        end
    end
    if mask == c.out then
        return
    end
    c.out = mask
    core.meta_set(c.x, c.y, c.z, "o", if mask == 0 then "" else tostring(mask))
    local want = if mask ~= 0 then CHIP_ON else CHIP_OFF
    if core.get_block(c.x, c.y, c.z) ~= want then
        local facing = core.get_facing(c.x, c.y, c.z)
        core.set_block(c.x, c.y, c.z, want)
        core.set_facing(c.x, c.y, c.z, facing)
        core.meta_set(c.x, c.y, c.z, "chip", tostring(c.id))
        core.meta_set(c.x, c.y, c.z, "o", if mask == 0 then "" else tostring(mask))
    end
    touch(REAL, c.x, c.y, c.z)
    tell_cinnabar(REAL, c.x, c.y, c.z)
end

-- ---------------------------------------------------------------- the tick
local function step(s: Sim)
    for at, p in s.pending do
        if clock >= p.at then
            s.pending[at] = nil
            set_gate(s, p.x, p.y, p.z, p.on)
        end
    end
    for at, p in s.bus_pending do
        if clock >= p.at then
            s.bus_pending[at] = nil
            set_bus(s, p.x, p.y, p.z, p.mask)
        end
    end
    for at, p in s.pulses do
        if clock >= p.at then
            s.pulses[at] = nil
            touch(s, p.x, p.y, p.z)
        end
    end
    if next(s.dirty) == nil then
        return
    end
    local batch = s.dirty
    s.dirty = {}
    local done: { [string]: boolean } = {}
    for at, c in batch do
        local x, y, z = c[1], c[2], c[3]
        local id = s.get(x, y, z)
        local w = WIRE[id]
        if w then
            if w.kind == "cable" then
                for channel = 1, #COLOURS do
                    if not done[`{at}:{channel}`] then
                        solve_net(s, x, y, z, channel, done)
                    end
                end
            elseif not done[at] then
                solve_net(s, x, y, z, 0, done)
            end
        elseif GATE[id] then
            eval_gate(s, x, y, z)
        elseif BUS[id] then
            eval_bus(s, x, y, z)
        elseif LAMP[id] or id == PIN_OUT or id == PIN_OUT_ON then
            eval_lamp(s, x, y, z)
        elseif CHIP[id] and s.real then
            eval_chip(x, y, z)
        end
    end
end

core.event.on("core:tick", function(ev)
    clock += ev.dt
    step(REAL)
    for at, c in chips do
        if not CHIP[core.get_block(c.x, c.y, c.z)] then
            chips[at] = nil
            continue
        end
        c.beat += ev.dt
        if c.beat >= 0.25 then
            c.beat -= 0.25
            for _, cell in c.design.cells do
                local id = c.sim.get(cell[1] :: number, cell[2] :: number, cell[3] :: number)
                local g = GATE[id]
                if g and g.kind == "timer" then
                    timer_beat(c.sim, cell[1] :: number, cell[2] :: number, cell[3] :: number)
                elseif g and g.kind == "random" then
                    random_beat(c.sim, cell[1] :: number, cell[2] :: number, cell[3] :: number)
                elseif g and g.kind == "light" then
                    touch(c.sim, cell[1] :: number, cell[2] :: number, cell[3] :: number)
                end
            end
        end
        step(c.sim)
        chip_outputs(c)
    end
end)

-- Every change cinnabar makes (mods/cinnabar's put), remembered for the next tick:
-- this can arrive in the middle of its solving.
core.event.on("cinnabar:put", function(ev)
    touch(REAL, ev.x, ev.y, ev.z)
end)

-- A mod that wrote wires, gates or chips with core.set_block (a cell's facing
-- byte saying the face a wire lies on: axis * 4) says where, and they are
-- shaped and worked out as if a hand had put them.
core.event.define("circuits:changed", { fields = { x = "number", y = "number", z = "number" }, cancellable = false })
core.event.on("circuits:changed", function(ev)
    reshape_around(REAL, ev.x, ev.y, ev.z)
    touch_around(REAL, ev.x, ev.y, ev.z)
end)

-- And a mod may make a chip's design from a box: "circuits:chip_saved" answers
-- with its number (0 and why not), for core.give_item's id or a chip block's
-- meta "chip". `front` is the side (0 +Z, 1 +X, 2 -Z, 3 -X) a chip put down
-- facing 0 has where the box had it.
core.event.define("circuits:save_chip", { fields = { x1 = "number", y1 = "number", z1 = "number", x2 = "number",
                                                     y2 = "number", z2 = "number", front = "number" }, cancellable = false })
core.event.define("circuits:chip_saved", { fields = { id = "number", why = "string" }, cancellable = false })
core.event.on("circuits:save_chip", function(ev)
    local id, why = save_design(math.min(ev.x1, ev.x2), math.min(ev.y1, ev.y2), math.min(ev.z1, ev.z2),
                                math.max(ev.x1, ev.x2), math.max(ev.y1, ev.y2), math.max(ev.z1, ev.z2), ev.front % 4)
    core.log(`circuits: chip design {id or 0}: {why}`)
    core.event.emit("circuits:chip_saved", { id = id or 0, why = why })
end)

core.register_abm("circuits:timer", { "circuits:gate_timer", "circuits:gate_on_timer" }, 0.25, 1, function(cell)
    timer_beat(REAL, cell.x, cell.y, cell.z)
end)
core.register_abm("circuits:random", { "circuits:gate_random", "circuits:gate_on_random" }, 0.25, 1, function(cell)
    random_beat(REAL, cell.x, cell.y, cell.z)
end)
core.register_abm("circuits:light", { "circuits:gate_light", "circuits:gate_on_light" }, 1, 1, function(cell)
    eval_gate(REAL, cell.x, cell.y, cell.z)
end)
-- A chip after a restart: running again as soon as its column is about.
core.register_abm("circuits:chip", { "circuits:chip_block", "circuits:chip_block_on" }, 2, 1, function(cell)
    if chips[k(cell.x, cell.y, cell.z)] == nil then
        touch(REAL, cell.x, cell.y, cell.z)
    end
end)

-- ---------------------------------------------------------------- hands
local function next_of(list: { number }, now: number): number
    for i, v in list do
        if v == now then
            return list[i % #list + 1]
        end
    end
    return list[1]
end

local function is_wire_item(name: string): boolean
    return name == "circuits:alloy" or name == "circuits:cable" or string.sub(name, 1, 14) == "circuits:wire_"
end

-- The block a wire in hand was last used on: the face it was clicked on is the
-- one it goes against (core:placed does not say which face that was).
local aimed: { [number]: { number } } = {}
local bench_of: { [number]: { number } } = {}
local placing_chip: { [number]: number } = {}
local breaking_chip: { [number]: number } = {}
local renumber: { { number } } = {}

core.event.on("core:tick", function(_ev)
    for _, r in renumber do
        local player, id = r[1], r[2]
        if core.take_item(player, "circuits:blank_chip", 1) == 1 and core.give_item(player, "circuits:chip", 1, id) == 0 then
            core.give_item(player, "circuits:blank_chip", 1)
        end
    end
    table.clear(renumber)
end)

-- Which side of the cell at x z the player stands on, 0..3 (SIDES).
local function side_of_player(player: number, x: number, z: number): number
    local px, _, pz = core.player_pos(player)
    local dx, dz = (px or x) - x, (pz or z) - z
    if math.abs(dx) > math.abs(dz) then
        return if dx > 0 then 1 else 3
    end
    return if dz > 0 then 0 else 2
end

core.event.on("core:used", function(ev)
    local g = GATE[ev.id]
    if is_wire_item(ev.held) then
        aimed[ev.player] = { ev.x, ev.y, ev.z }
    end
    if ev.held == "circuits:blank_chip" then
        ev.cancel = true
        if ev.id == BENCH then
            bench_of[ev.player] = { ev.x, ev.y, ev.z }
            core.notice(ev.player, "The bench is one corner: now use the chip on the far corner of the circuit.")
            return
        end
        local b = bench_of[ev.player]
        if b == nil then
            core.notice(ev.player, "Use the blank chip on a chip bench first.")
            return
        end
        bench_of[ev.player] = nil
        local x1, x2 = math.min(b[1], ev.x), math.max(b[1], ev.x)
        local z1, z2 = math.min(b[3], ev.z), math.max(b[3], ev.z)
        local y1 = b[2] + 1
        local y2 = math.max(y1, ev.y)
        local id, why = save_design(x1, y1, z1, x2, y2, z2,
                                    side_of_player(ev.player, (x1 + x2 + 1) / 2, (z1 + z2 + 1) / 2))
        if id and core.take_item(ev.player, "circuits:blank_chip", 1) == 1 then
            core.give_item(ev.player, "circuits:chip", 1, id)
            core.log(`circuits: {core.player_name(ev.player)} made chip #{id} from {x1} {y1} {z1} to {x2} {y2} {z2}`)
        end
        core.notice(ev.player, why)
        return
    end
    if ev.held == "screwdriver:screwdriver" then
        touch(REAL, ev.x, ev.y, ev.z)
    elseif ev.id == PIN_IN or ev.id == PIN_IN_ON then
        -- A hand switches an input pin, to try the circuit before it is a chip.
        core.set_block(ev.x, ev.y, ev.z, if ev.id == PIN_IN then PIN_IN_ON else PIN_IN)
        touch(REAL, ev.x, ev.y, ev.z)
        tell_cinnabar(REAL, ev.x, ev.y, ev.z)
        ev.cancel = true
    elseif g and g.kind == "toggle" then
        set_bit(REAL, ev.x, ev.y, ev.z, "q", not bit(REAL, ev.x, ev.y, ev.z, "q"))
        touch(REAL, ev.x, ev.y, ev.z)
        ev.cancel = true
    elseif g and g.kind == "timer" then
        local p = next_of({ 1, 2, 4, 8 }, number_of(REAL, ev.x, ev.y, ev.z, "p", 1))
        core.meta_set(ev.x, ev.y, ev.z, "p", tostring(p))
        core.notice(ev.player, `Every {p} s`)
        ev.cancel = true
    elseif g and g.kind == "counter" then
        local m = next_of({ 2, 4, 8, 16 }, number_of(REAL, ev.x, ev.y, ev.z, "m", 8))
        core.meta_set(ev.x, ev.y, ev.z, "m", tostring(m))
        core.meta_set(ev.x, ev.y, ev.z, "n", "")
        core.notice(ev.player, `Counts to {m}`)
        touch(REAL, ev.x, ev.y, ev.z)
        ev.cancel = true
    end
end)

core.event.on("core:pre_place", function(ev)
    if CHIP[ev.id] then
        local id = core.held_id(ev.player)
        if load_design(id) == nil then
            core.notice(ev.player, "This chip is blank.")
            ev.cancel = true
            return
        end
        placing_chip[ev.player] = id
    end
end)

core.event.on("core:pre_break", function(ev)
    if CHIP[ev.id] then
        breaking_chip[ev.player] = tonumber(core.meta_get(ev.x, ev.y, ev.z, "chip")) or 0
    end
end)

-- The face a wire put at x y z goes against: the one clicked, or failing that
-- (a placement with Shift, which is not a use) the floor, the ceiling, a wall.
local function support_of(player: number, x: number, y: number, z: number): { number }?
    local a = aimed[player]
    aimed[player] = nil
    if a then
        local dx, dy, dz = a[1] - x, a[2] - y, a[3] - z
        if math.abs(dx) + math.abs(dy) + math.abs(dz) == 1 and core.node_solid(core.get_block(a[1], a[2], a[3])) then
            return { dx, dy, dz }
        end
    end
    for _, d in { { 0, -1, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } } do
        if core.node_solid(core.get_block(x + d[1], y + d[2], z + d[3])) then
            return d
        end
    end
    return nil
end

core.event.on("core:placed", function(ev)
    local w = WIRE[ev.id]
    if w and not w.riser then
        local s = support_of(ev.player, ev.x, ev.y, ev.z)
        if s == nil then
            core.set_block(ev.x, ev.y, ev.z, 0)
            core.drop_item(ev.x, ev.y, ev.z, core.item_id(core.node_name(w.pair[1].off)), 1)
            return
        end
        core.set_facing(ev.x, ev.y, ev.z, axis_of_up(-s[1], -s[2], -s[3]) * 4)
    elseif CHIP[ev.id] then
        core.meta_set(ev.x, ev.y, ev.z, "chip", tostring(placing_chip[ev.player] or 0))
        placing_chip[ev.player] = nil
    end
    reshape_around(REAL, ev.x, ev.y, ev.z)
    touch_around(REAL, ev.x, ev.y, ev.z)
end)

core.event.on("core:broken", function(ev)
    local at = k(ev.x, ev.y, ev.z)
    REAL.elapsed[at] = nil
    REAL.bus[at] = nil
    if CHIP[ev.id] then
        chips[at] = nil
        -- It goes to the bag as a blank chip, after this event: on the next
        -- tick that one is swapped for the chip numbered with its design.
        local id = breaking_chip[ev.player] or 0
        breaking_chip[ev.player] = nil
        if id ~= 0 and ev.player > 0 then
            table.insert(renumber, { ev.player, id })
        end
    end
    -- A wire that lay on this block falls off it.
    for _, d in ALL6 do
        local x, y, z = ev.x + d[1], ev.y + d[2], ev.z + d[3]
        local w = WIRE[core.get_block(x, y, z)]
        if w and not w.riser then
            local u = UP[axis_at(REAL, x, y, z) + 1]
            if x - u[1] == ev.x and y - u[2] == ev.y and z - u[3] == ev.z then
                core.set_block(x, y, z, 0)
                core.drop_item(x, y, z, core.item_id(core.node_name(w.pair[1].off)), 1)
                reshape_around(REAL, x, y, z)
                touch_around(REAL, x, y, z)
            end
        end
    end
    reshape_around(REAL, ev.x, ev.y, ev.z)
    touch_around(REAL, ev.x, ev.y, ev.z)
end)

-- ---------------------------------------------------------------- help
local HELP = {
    "Circuits: wire, insulated wire and cable go on floors, walls and ceilings, on the face you click.",
    "A wire on a wall joins the floor wire under it against the same wall, and runs over the edge of a block.",
    "A cable carries sixteen signals: one for each colour of insulated wire that touches it.",
    "Bus gates (AND, OR, XOR, NOT) work on a cable's sixteen channels one by one; the output is the side away from you.",
    "A plain signal at a bus gate counts as all sixteen channels: an AND with a lever switches a whole cable.",
    "Chips: put input and output pins on the sides of a circuit, one a side; a hand switches an input pin to try it.",
    "Use a blank chip on a chip bench under a corner of the circuit, then on the far corner: the box becomes a chip.",
    "A chip put down runs the circuit inside it: its sides are the pins, turned the way you put it down.",
}
core.command("circuits", "-- how wires, bus gates and chips work")
core.event.on("core:command", function(ev)
    if ev.name == "circuits" then
        for _, line in HELP do
            core.chat(ev.player, line)
        end
    end
end)

-- ---------------------------------------------------------------- recipes
core.register_item("circuits:alloy_ingot", 64, "", 0, 1.0, "")
core.register_item("circuits:plate", 64, "", 0, 1.0, "")
core.register_item("circuits:blank_chip", 16, "", 0, 1.0, "")
core.register_item("circuits:chip", 16, "circuits:chip_block", 0, 1.0, "")
core.recipe_shapeless("circuits:alloy_ingot", 1, "furnace:iron_ingot ore:cinnabar ore:cinnabar")
core.recipe_shaped("circuits:plate", 6, "core:stone ore:cinnabar core:stone")
core.recipe_shapeless("circuits:alloy", 8, "circuits:alloy_ingot")
core.recipe_shaped("circuits:alloy_riser", 3, "circuits:alloy", "timber:stick", "circuits:alloy")
core.recipe_shaped("circuits:cable", 3,
    "circuits:alloy     circuits:alloy     circuits:alloy",
    "colors:white_wool  colors:white_wool  colors:white_wool",
    "circuits:alloy     circuits:alloy     circuits:alloy")
for _, c in COLOURS do
    core.recipe_shapeless(`circuits:wire_{c[1]}`, 4, `circuits:alloy circuits:alloy circuits:alloy circuits:alloy colors:{c[1]}_wool`)
    core.recipe_shapeless(`circuits:lamp_{c[1]}`, 1, `core:glass ore:cinnabar colors:{c[1]}_dye`)
end
for _, g in GATE_LIST do
    core.recipe_shaped(`circuits:gate_{g[1]}`, 1, g[2], "circuits:plate circuits:plate circuits:plate")
end
for _, n in BUS_LIST do
    core.recipe_shaped(`circuits:bus_gate_{n}`, 1, `circuits:cable circuits:gate_{n} circuits:cable`,
                       "circuits:plate circuits:plate circuits:plate")
end
core.recipe_shapeless("circuits:pin_in", 2, "circuits:plate circuits:alloy cinnabar:lever")
core.recipe_shapeless("circuits:pin_out", 2, "circuits:plate circuits:alloy cinnabar:lamp")
core.recipe_shapeless("circuits:blank_chip", 1, "circuits:plate circuits:alloy_ingot circuits:alloy_ingot")
core.recipe_shaped("circuits:chip_bench", 1,
    "circuits:plate     circuits:plate      circuits:plate",
    "timber:planks      circuits:gate_and   timber:planks",
    "timber:planks      timber:planks       timber:planks")
local _ = ALLOY + CABLE
