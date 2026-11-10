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
-- With cinnabar: a lever, button, plate, torch, repeater or comparator drives a
-- wire. Alloy wire gives power to the dust, lamps, repeaters and pistons beside
-- it, and a gate gives it out of its output. Gates and lamps read lit dust, but
-- dust does not drive a wire: dust beside a wire it was lit by would hold it on
-- for ever, so dust goes into a wire through a repeater or a buffer gate.
--
-- Nothing here is saved but the blocks and their meta: a wire's state is which
-- block it is, and every change is worked out again from the world.
local TICK = 0.1
local PULSE = 0.25
local MAX_NET = 4096  -- ponytail: a net past this is left part done; ask for a limit by world if one is hit

local COLOURS = {
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
local SHAPES = { { "none", 0 }, { "end", 1 }, { "line", 5 }, { "corner", 3 }, { "tee", 7 }, { "cross", 15 } }
local FIT: { [number]: { shape: number, turn: number } } = {}
for i, s in SHAPES do
    for turn = 0, 3 do
        local m = bit32.band(bit32.bor(bit32.lshift(s[2], turn), bit32.rshift(s[2], 4 - turn)), 15)
        if not FIT[m] then
            FIT[m] = { shape = i, turn = turn }
        end
    end
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
        local off = core.register_node(if i == 1 then prefix else `{prefix}_{s[1]}`, r, g, b, false, 0, "upright", 0, 0.1, "")
        local on = off
        if kind ~= "cable" then
            on = core.register_node(`{prefix}_on_{s[1]}`, brighter(r), brighter(g), brighter(b), false, 0, "upright", 0, 0.1, "")
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

local LAMP: { [number]: { off: number, on: number } } = {}
for _, c in COLOURS do
    local off = core.register_node(`circuits:lamp_{c[1]}`, math.floor(c[2] / 2), math.floor(c[3] / 2), math.floor(c[4] / 2), true, 0, "", 0, 0.3, "")
    local on = core.register_node(`circuits:lamp_on_{c[1]}`, c[2], c[3], c[4], true, 15, "", 0, 0.3, "")
    LAMP[off] = { off = off, on = on }
    LAMP[on] = { off = off, on = on }
    core.node_drop(on, off)
end

-- ---------------------------------------------------------------- what cinnabar has
local function cid(name: string): number
    return core.node_id(name)
end
local GIVES_ALL: { [number]: boolean } = {}
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
for id in LAMP do JOINS[id] = true end
JOINS[0] = nil

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

local function k(x: number, y: number, z: number): string
    return `{x},{y},{z}`
end

local FRONT = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }
local function front_of(x: number, y: number, z: number): (number, number)
    local f = FRONT[core.get_facing(x, y, z) % 4 + 1]
    return f[1], f[2]
end

-- Whether the block at n gives power into the cell t beside it.
local function gives_into(nx: number, ny: number, nz: number, tx: number, ty: number, tz: number): boolean
    local id = core.get_block(nx, ny, nz)
    if GIVES_ALL[id] then
        return true
    end
    if id == REPEATER_ON or id == COMPARATOR_ON or (GATE[id] and GATE[id].on) then
        local fx, fz = front_of(nx, ny, nz)
        return ny == ty and nx - fx == tx and nz - fz == tz
    end
    if id == OBSERVER_ON then
        local d = ALL6[core.get_facing(nx, ny, nz) % 6 + 1]
        return nx + d[1] == tx and ny + d[2] == ty and nz + d[3] == tz
    end
    return false
end

-- Whether a gate or lamp at t reads power from the block at n: all of the above,
-- and lit dust, and a lit wire.
local function reads(nx: number, ny: number, nz: number, tx: number, ty: number, tz: number): boolean
    local id = core.get_block(nx, ny, nz)
    local w = WIRE[id]
    return DUST_LIT[id] or (w ~= nil and w.on) or gives_into(nx, ny, nz, tx, ty, tz)
end

-- ---------------------------------------------------------------- shapes
local function links(a: Wire, b: Wire): boolean
    if a.kind == "cable" then return b.kind == "cable" or b.kind == "wire" end
    if b.kind == "cable" then return a.kind == "wire" end
    if a.kind == "wire" and b.kind == "wire" then return a.colour == b.colour end
    return true
end

local function reshape(x: number, y: number, z: number)
    local id = core.get_block(x, y, z)
    local w = WIRE[id]
    if not w or w.riser then
        return
    end
    local mask = 0
    for i, d in SIDES do
        local nid = core.get_block(x + d[1], y, z + d[2])
        local other = WIRE[nid]
        if (other and links(w, other)) or (not other and w.kind ~= "cable" and JOINS[nid]) then
            mask += bit32.lshift(1, i - 1)
        end
    end
    local fit = FIT[mask]
    local pair = w.pair[fit.shape]
    local want = if w.on then pair.on else pair.off
    if want ~= id then
        core.set_block(x, y, z, want)
    end
    if want ~= id or core.get_facing(x, y, z) ~= fit.turn then
        core.set_facing(x, y, z, fit.turn)
    end
end

local function reshape_around(x: number, y: number, z: number)
    reshape(x, y, z)
    for _, d in SIDES do
        reshape(x + d[1], y, z + d[2])
    end
end

-- ---------------------------------------------------------------- what is due
local clock = 0
local dirty: { [string]: { number } } = {}
local pending: { [string]: { x: number, y: number, z: number, on: boolean, at: number } } = {}
local pulses: { [string]: { x: number, y: number, z: number, at: number } } = {}
local elapsed: { [string]: number } = {}

local function touch(x: number, y: number, z: number)
    local function one(cx: number, cy: number, cz: number)
        local id = core.get_block(cx, cy, cz)
        if WIRE[id] or GATE[id] or LAMP[id] then
            dirty[k(cx, cy, cz)] = { cx, cy, cz }
        end
    end
    one(x, y, z)
    for _, d in ALL6 do
        one(x + d[1], y + d[2], z + d[3])
    end
end

local function tell_cinnabar(x: number, y: number, z: number)
    for _, d in ALL6 do
        local name = core.node_name(core.get_block(x + d[1], y + d[2], z + d[3]))
        if string.sub(name, 1, 9) == "cinnabar:" or string.sub(name, 1, 6) == "doors:" then
            core.event.emit("cinnabar:changed", { x = x, y = y, z = z })
            return
        end
    end
end

-- ---------------------------------------------------------------- a net
local function driven(x: number, y: number, z: number): boolean
    for _, d in ALL6 do
        if gives_into(x + d[1], y + d[2], z + d[3], x, y, z) then
            return true
        end
    end
    return false
end

-- Every wire joined to this one, and on if anything drives any of them. A cable
-- is walked once per channel; `ctx` is the colour in hand while inside it.
local function solve_net(sx: number, sy: number, sz: number, ctx: number, done: { [string]: boolean })
    local seen: { [string]: boolean } = {}
    local cells: { { number } } = {}
    local stack: { { number } } = { { sx, sy, sz, ctx } }
    local on, count = false, 0
    while #stack > 0 do
        local c = table.remove(stack) :: { number }
        local x, y, z, here = c[1], c[2], c[3], c[4]
        local w = WIRE[core.get_block(x, y, z)]
        local at = k(x, y, z) .. (if w and w.kind == "cable" then `:{here}` else "")
        if w and not seen[at] then
            seen[at] = true
            count += 1
            if count > MAX_NET then
                core.log(`circuits: a net wider than {MAX_NET} cells at {sx} {sy} {sz}, left part done`)
                return
            end
            if w.kind ~= "cable" then
                table.insert(cells, c)
                on = on or driven(x, y, z)
            end
            for _, d in SIDES do
                local nx, nz = x + d[1], z + d[2]
                local n = WIRE[core.get_block(nx, y, nz)]
                if n and links(w, n) then
                    if n.kind == "cable" then
                        table.insert(stack, { nx, y, nz, if w.kind == "cable" then here else w.colour })
                    elseif w.kind ~= "cable" or n.colour == here then
                        table.insert(stack, { nx, y, nz, 0 })
                    end
                end
            end
            if w.riser then
                for dy = -1, 1, 2 do
                    local up = WIRE[core.get_block(x, y + dy, z)]
                    if up and up.riser then
                        table.insert(stack, { x, y + dy, z, 0 })
                    end
                end
            end
        end
    end
    for _, c in cells do
        local x, y, z = c[1], c[2], c[3]
        done[k(x, y, z)] = true
        local id = core.get_block(x, y, z)
        local w = WIRE[id]
        if w.on ~= on then
            local pair = w.pair[w.shape]
            local facing = core.get_facing(x, y, z)
            core.set_block(x, y, z, if on then pair.on else pair.off)
            core.set_facing(x, y, z, facing)
            touch(x, y, z)
            if w.kind == "alloy" then
                tell_cinnabar(x, y, z)
            end
        end
    end
end

-- ---------------------------------------------------------------- a gate
local function bit(x: number, y: number, z: number, key: string): boolean
    return core.meta_get(x, y, z, key) == "1"
end
local function set_bit(x: number, y: number, z: number, key: string, v: boolean)
    core.meta_set(x, y, z, key, if v then "1" else "")
end
local function number_of(x: number, y: number, z: number, key: string, default: number): number
    return tonumber(core.meta_get(x, y, z, key)) or default
end

local function want_out(x: number, y: number, z: number, g: Gate, on: boolean)
    local at = k(x, y, z)
    if on == g.on then
        pending[at] = nil
    elseif not pending[at] or pending[at].on ~= on then
        pending[at] = { x = x, y = y, z = z, on = on, at = clock + TICK }
    end
end

local function start_pulse(x: number, y: number, z: number)
    pulses[k(x, y, z)] = { x = x, y = y, z = z, at = clock + PULSE }
end

local function eval_gate(x: number, y: number, z: number)
    local g = GATE[core.get_block(x, y, z)]
    if not g then
        return
    end
    local fx, fz = front_of(x, y, z)
    local function input(dx: number, dz: number): (boolean, boolean)
        local nx, nz = x + dx, z + dz
        local nid = core.get_block(nx, y, nz)
        local w = WIRE[nid]
        return reads(nx, y, nz, x, y, z), (w ~= nil and w.kind ~= "cable") or JOINS[nid] == true
    end
    local a, pa = input(fx, fz)
    local l, pl = input(-fz, fx)
    local r, pr = input(fz, -fx)
    -- What each input was last time, for the gates that act on a rising edge.
    local rise_a = a and not bit(x, y, z, "a")
    local rise_l = l and not bit(x, y, z, "l")
    local rise_r = r and not bit(x, y, z, "r")
    set_bit(x, y, z, "a", a)
    set_bit(x, y, z, "l", l)
    set_bit(x, y, z, "r", r)

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
        out = (l or bit(x, y, z, "q")) and not r
        set_bit(x, y, z, "q", out)
    elseif kind == "toggle" then
        out = bit(x, y, z, "q") ~= rise_a
        set_bit(x, y, z, "q", out)
    elseif kind == "d_latch" then
        out = if l or r then a else bit(x, y, z, "q")
        set_bit(x, y, z, "q", out)
    elseif kind == "pulse" then
        if rise_a then start_pulse(x, y, z) end
        out = pulses[k(x, y, z)] ~= nil
    elseif kind == "timer" then
        out = pulses[k(x, y, z)] ~= nil
    elseif kind == "counter" then
        local most = number_of(x, y, z, "m", 8)
        local n = math.clamp(number_of(x, y, z, "n", 0) + (if rise_l then 1 else 0) - (if rise_r then 1 else 0), 0, most)
        core.meta_set(x, y, z, "n", tostring(n))
        out = n >= most
    elseif kind == "random" then
        out = a and g.on
    elseif kind == "light" then
        out = core.block_light(x, y + 1, z) >= 8
    end
    want_out(x, y, z, g, out)
end

local function eval_lamp(x: number, y: number, z: number)
    local id = core.get_block(x, y, z)
    local lamp = LAMP[id]
    if not lamp then
        return
    end
    local lit = false
    for _, d in ALL6 do
        if reads(x + d[1], y + d[2], z + d[3], x, y, z) then
            lit = true
            break
        end
    end
    local want = if lit then lamp.on else lamp.off
    if want ~= id then
        core.set_block(x, y, z, want)
    end
end

local function set_gate(x: number, y: number, z: number, on: boolean)
    local g = GATE[core.get_block(x, y, z)]
    if not g or g.on == on then
        return
    end
    local facing = core.get_facing(x, y, z)
    core.set_block(x, y, z, if on then g.on_id else g.off_id)
    core.set_facing(x, y, z, facing)
    touch(x, y, z)
    tell_cinnabar(x, y, z)
end

-- ---------------------------------------------------------------- the tick
core.event.on("core:tick", function(ev)
    clock += ev.dt
    for at, p in pending do
        if clock >= p.at then
            pending[at] = nil
            set_gate(p.x, p.y, p.z, p.on)
        end
    end
    for at, p in pulses do
        if clock >= p.at then
            pulses[at] = nil
            touch(p.x, p.y, p.z)
        end
    end
    if next(dirty) == nil then
        return
    end
    local batch = dirty
    dirty = {}
    local done: { [string]: boolean } = {}
    for at, c in batch do
        local x, y, z = c[1], c[2], c[3]
        local id = core.get_block(x, y, z)
        local w = WIRE[id]
        if w then
            if w.kind == "cable" then
                for channel = 1, #COLOURS do
                    solve_net(x, y, z, channel, done)
                end
            elseif not done[at] then
                solve_net(x, y, z, 0, done)
            end
        elseif GATE[id] then
            eval_gate(x, y, z)
        elseif LAMP[id] then
            eval_lamp(x, y, z)
        end
    end
end)

-- Every change cinnabar makes (mods/cinnabar's put), remembered for the next tick:
-- this can arrive in the middle of its solving.
core.event.on("cinnabar:put", function(ev)
    touch(ev.x, ev.y, ev.z)
end)

core.register_abm("circuits:timer", { "circuits:gate_timer", "circuits:gate_on_timer" }, 0.25, 1, function(cell)
    local at = k(cell.x, cell.y, cell.z)
    local e = (elapsed[at] or 0) + 0.25
    local fx, fz = front_of(cell.x, cell.y, cell.z)
    local paused = reads(cell.x + fx, cell.y, cell.z + fz, cell.x, cell.y, cell.z)
    if not paused and e >= number_of(cell.x, cell.y, cell.z, "p", 1) then
        e = 0
        start_pulse(cell.x, cell.y, cell.z)
        touch(cell.x, cell.y, cell.z)
    end
    elapsed[at] = e
end)
core.register_abm("circuits:random", { "circuits:gate_random", "circuits:gate_on_random" }, 0.25, 1, function(cell)
    local g = GATE[core.get_block(cell.x, cell.y, cell.z)]
    local fx, fz = front_of(cell.x, cell.y, cell.z)
    if reads(cell.x + fx, cell.y, cell.z + fz, cell.x, cell.y, cell.z) then
        want_out(cell.x, cell.y, cell.z, g, math.random() < 0.5)
    else
        want_out(cell.x, cell.y, cell.z, g, false)
    end
end)
core.register_abm("circuits:light", { "circuits:gate_light", "circuits:gate_on_light" }, 1, 1, function(cell)
    eval_gate(cell.x, cell.y, cell.z)
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

core.event.on("core:used", function(ev)
    local g = GATE[ev.id]
    if ev.held == "screwdriver:screwdriver" then
        touch(ev.x, ev.y, ev.z)
    elseif g and g.kind == "toggle" then
        set_bit(ev.x, ev.y, ev.z, "q", not bit(ev.x, ev.y, ev.z, "q"))
        touch(ev.x, ev.y, ev.z)
        ev.cancel = true
    elseif g and g.kind == "timer" then
        local p = next_of({ 1, 2, 4, 8 }, number_of(ev.x, ev.y, ev.z, "p", 1))
        core.meta_set(ev.x, ev.y, ev.z, "p", tostring(p))
        core.notice(ev.player, `Every {p} s`)
        ev.cancel = true
    elseif g and g.kind == "counter" then
        local m = next_of({ 2, 4, 8, 16 }, number_of(ev.x, ev.y, ev.z, "m", 8))
        core.meta_set(ev.x, ev.y, ev.z, "m", tostring(m))
        core.meta_set(ev.x, ev.y, ev.z, "n", "")
        core.notice(ev.player, `Counts to {m}`)
        touch(ev.x, ev.y, ev.z)
        ev.cancel = true
    end
end)

core.event.on("core:placed", function(ev)
    local w = WIRE[ev.id]
    if w and not w.riser and core.node_solid(core.get_block(ev.x, ev.y - 1, ev.z)) == 0 then
        core.set_block(ev.x, ev.y, ev.z, 0)
        core.drop_item(ev.x, ev.y, ev.z, core.item_id(core.node_name(w.pair[1].off)), 1)
        return
    end
    reshape_around(ev.x, ev.y, ev.z)
    touch(ev.x, ev.y, ev.z)
end)

core.event.on("core:broken", function(ev)
    for _, key in { "a", "l", "r", "q", "n", "m", "p" } do
        core.meta_set(ev.x, ev.y, ev.z, key, "")
    end
    elapsed[k(ev.x, ev.y, ev.z)] = nil
    local above = WIRE[core.get_block(ev.x, ev.y + 1, ev.z)]
    if above and not above.riser then
        core.set_block(ev.x, ev.y + 1, ev.z, 0)
        core.drop_item(ev.x, ev.y + 1, ev.z, core.item_id(core.node_name(above.pair[1].off)), 1)
    end
    reshape_around(ev.x, ev.y, ev.z)
    touch(ev.x, ev.y, ev.z)
end)

-- ---------------------------------------------------------------- recipes
core.register_item("circuits:alloy_ingot", 64, "", 0, 1.0, "")
core.register_item("circuits:plate", 64, "", 0, 1.0, "")
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
local _ = ALLOY + CABLE
