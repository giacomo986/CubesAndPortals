--!strict
-- parkour: a course in the sky, a run of stations along -x from the plaza a
-- player arrives on, each one a move of the engine's parkour (core.parkour):
--
--   1  a pit eight wide with a brick wall along its right: jump, and forward
--      held runs along the wall, level, for up to five nodes, then over
--   2  a wall with a hole one high: crouch at a run is a slide under it
--   3  a step two high: a jump does not reach it, a backflip does (back and
--      jump, with your back to it)
--   4  two walls two apart: jump against one and jump again to kick off it
--      to the other, higher each time, up onto the lower wall's top
--   5  a pit eighteen wide under a steel beam: the rope gun (right button)
--      hooks the beam, the swing crosses it; jump or left lets go
--
-- An orange pad starts every station. Fallen into the void, a player is put
-- back on the last pad they stood on. A doorway on the plaza and another at
-- the end lead back to the sandbox. Nothing is dug and nothing is built.
--
-- `side = both`: the server builds the course and keeps the pads; a player's
-- machine fires the rope.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local GUN = "parkour:rope_gun"

-- ===================================================================== server

if serving then
    local AIR = 0
    local function node(name: string, rgb: { number }, light: number?): number
        return core.register_node(`parkour:{name}`, rgb[1], rgb[2], rgb[3], true, light or 0, "", 0, 0, "stone")
    end
    local CONCRETE = node("concrete", { 150, 152, 150 })
    local BRICK    = node("brick", { 156, 66, 52 })
    local STEEL    = node("steel", { 70, 78, 88 })
    local HAZARD   = node("hazard", { 236, 196, 30 })
    local PAD      = node("pad", { 240, 120, 24 }, 8)
    local FRAME    = node("frame", { 34, 34, 36 })
    local LAMP     = node("lamp", { 255, 240, 200 }, 14)

    core.register_item(GUN, 1, "", 0, 1.0, "")
    core.register_model("parkour:rope_gun_held", "rope_gun_held.gltf")

    -- The body, and parkour on: parkour_core.

    -- Nothing of the engine's terrain: the course hangs in the sky.
    core.mapgen_core(false)

    -- The course, as boxes of cells, ends included: { x0, y0, z0, x1, y1, z1, block }.
    -- The floor's top cell is y 20 on the plaza, so a body stands at 21.
    local BOXES: { { number } } = {}
    local function box(x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, b: number)
        table.insert(BOXES, { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1),
                              math.max(x0, x1), math.max(y0, y1), math.max(z0, z1), b })
    end
    -- A floor of concrete from x0 to x1 with hazard along both long edges.
    local function floor(x0: number, x1: number, y: number, z0: number, z1: number)
        box(x0, y, z0, x1, y, z1, CONCRETE)
        box(x0, y, z0, x1, y, z0, HAZARD)
        box(x0, y, z1, x1, y, z1, HAZARD)
    end
    -- A doorway one wide and two high in a black frame, its front +z (k 0)
    -- or +x (k 1) of the cell (hx, F, hz), F the first cell of air.
    local function doorway(hx: number, F: number, hz: number, plus_x: boolean)
        local function put(u: number, v: number, w: number, b: number)
            if plus_x then box(hx + w, F + v, hz + u, hx + w, F + v, hz + u, b)
            else box(hx + u, F + v, hz + w, hx + u, F + v, hz + w, b) end
        end
        for v = 0, 3 do
            for u = -2, 2 do
                put(u, v, -1, FRAME)
                local hole = u == 0 and v < 2
                put(u, v, 0, if hole then AIR elseif v == 3 and u == 0 then LAMP else FRAME)
            end
        end
    end

    -- The plaza, and the doorway home on its -z side, facing +z.
    floor(-6, 6, 20, -8, 8)
    doorway(0, 21, -7, false)

    -- 1. The wall run: a pit x -22 .. -15, eight wide -- a sprinting jump
    -- clears five -- and the brick wall along its right going -x (z -3),
    -- x -25 .. -13, five high.
    box(-25, 21, -3, -13, 25, -3, BRICK)
    floor(-14, -7, 20, -2, 2)
    floor(-40, -23, 20, -2, 2)

    -- 2. The slide: a wall across the corridor x -35 .. -32 with a hole one
    -- high at the floor, and walls either side.
    box(-35, 22, -2, -32, 25, 2, BRICK)
    box(-35, 21, -3, -32, 25, -3, BRICK)
    box(-35, 21, 3, -32, 25, 3, BRICK)

    -- 3. The backflip: a step two high, x -50 .. -41, its top 22; walled
    -- along both sides.
    box(-50, 21, -2, -41, 22, 2, CONCRETE)
    box(-50, 21, -3, -41, 24, -3, BRICK)
    box(-50, 21, 3, -41, 24, 3, BRICK)
    box(-50, 22, -2, -41, 22, -2, HAZARD)
    box(-50, 22, 2, -41, 22, 2, HAZARD)

    -- 4. The chimney: from the step's top (stand at 23), a gap two wide
    -- (z -1 .. 0) between wall A (z -2, up to 32) and wall B (z 1, up to 29),
    -- x -60 .. -51, shut at x -61. Beyond B, solid up to 29: the way on is
    -- its top, at 30.
    box(-60, 22, -1, -51, 22, 0, CONCRETE)
    box(-61, 23, -2, -51, 32, -2, BRICK)
    box(-61, 23, -1, -61, 32, 0, BRICK)
    box(-60, 23, 1, -51, 29, 1, BRICK)
    -- Eight wide, so a last kick off A that carries over B lands on it.
    box(-60, 23, 2, -51, 29, 8, CONCRETE)
    box(-70, 29, 1, -61, 29, 8, CONCRETE)
    box(-70, 29, 1, -61, 29, 1, HAZARD)
    box(-70, 29, 8, -51, 29, 8, HAZARD)

    -- 5. The rope: a pit x -88 .. -71 under a steel beam at 42, x -81 .. -78,
    -- on two posts out at the sides; the far side x -101 .. -89.
    box(-81, 42, -6, -78, 43, 9, STEEL)
    box(-80, 20, -6, -79, 41, -6, STEEL)
    box(-80, 20, 9, -79, 41, 9, STEEL)
    box(-101, 29, 0, -89, 29, 5, CONCRETE)
    box(-101, 29, 0, -89, 29, 0, HAZARD)
    box(-101, 29, 5, -89, 29, 5, HAZARD)

    -- The tower (the trial for the Chiave della Vetta, mods/keys): the five
    -- moves again, one after another and climbing, from the rope's landing to
    -- a summit at 53. Each station is one of the five above, moved: the same
    -- cells, so the same jumps reach. A corridor five wide centred on zc,
    -- going -x from xs with its floor at F; each returns where the next starts.
    local function chimney(xs: number, F: number, zc: number): (number, number)
        box(xs - 9, F, zc - 1, xs, F, zc, CONCRETE)
        box(xs - 10, F + 1, zc - 2, xs, F + 10, zc - 2, BRICK)
        box(xs - 10, F + 1, zc - 1, xs - 10, F + 10, zc, BRICK)
        box(xs - 9, F + 1, zc + 1, xs, F + 7, zc + 1, BRICK)
        box(xs - 9, F, zc + 2, xs, F + 7, zc + 8, CONCRETE)
        floor(xs - 19, xs - 11, F + 7, zc - 2, zc + 8)
        box(xs - 10, F + 7, zc + 1, xs - 10, F + 7, zc + 8, CONCRETE)
        return xs - 20, F + 7
    end
    local function wallrun(xs: number, F: number, zc: number): (number, number)
        floor(xs - 14, xs - 9, F, zc - 2, zc + 2)
        box(xs - 10, F + 1, zc - 3, xs + 2, F + 5, zc - 3, BRICK)
        return xs - 15, F
    end
    local function backflip(xs: number, F: number, zc: number): (number, number)
        floor(xs - 4, xs, F, zc - 2, zc + 2)
        box(xs - 14, F + 1, zc - 2, xs - 5, F + 2, zc + 2, CONCRETE)
        box(xs - 14, F + 1, zc - 3, xs - 5, F + 4, zc - 3, BRICK)
        box(xs - 14, F + 1, zc + 3, xs - 5, F + 4, zc + 3, BRICK)
        return xs - 15, F + 2
    end
    local function slide(xs: number, F: number, zc: number): (number, number)
        floor(xs - 9, xs, F, zc - 2, zc + 2)
        box(xs - 6, F + 2, zc - 2, xs - 3, F + 5, zc + 2, BRICK)
        box(xs - 6, F + 1, zc - 3, xs - 3, F + 5, zc - 3, BRICK)
        box(xs - 6, F + 1, zc + 3, xs - 3, F + 5, zc + 3, BRICK)
        return xs - 10, F
    end
    local function rope(xs: number, F: number, zc: number): (number, number)
        box(xs - 11, F + 13, zc - 6, xs - 8, F + 14, zc + 9, STEEL)
        box(xs - 10, F - 9, zc - 6, xs - 9, F + 12, zc - 6, STEEL)
        box(xs - 10, F - 9, zc + 9, xs - 9, F + 12, zc + 9, STEEL)
        floor(xs - 31, xs - 19, F, zc - 2, zc + 2)
        return xs - 32, F
    end

    -- From the landing south onto the tower's line (z -5), past the doorway.
    floor(-100, -89, 29, -8, -1)
    local TOWER: { { number } } = {}   -- each station's start: { x, floor, z }
    local tx, tf, tz = -101, 29, -5
    for _, station in { chimney, wallrun, backflip, chimney, slide, rope, chimney } do
        table.insert(TOWER, { tx + 1, tf, tz })
        floor(tx, tx, tf, tz - 2, tz + 2)
        tx, tf = station(tx, tf, tz)
    end
    -- The summit, its key pad, and the doorway home.
    floor(tx - 10, tx, tf, tz - 2, tz + 2)
    local SUMMIT = { tx - 4, tf, tz }
    doorway(tx - 10, tf + 1, tz, true)

    -- The pads, where a fall puts a player back: { x, y the floor, z }.
    local PADS: { { number } } = {
        { 0, 20, 2 }, { -27, 20, 0 }, { -40, 20, 0 }, { -53, 22, 0 }, { -67, 29, 2 }, { -92, 29, 2 },
    }
    for _, t in TOWER do table.insert(PADS, t) end
    table.insert(PADS, SUMMIT)
    for _, p in PADS do
        box(p[1], p[2], p[3], p[1], p[2], p[3], PAD)
    end
    -- Said to a player standing on each pad: what the next station wants.
    local HINTS = {
        "Corsa sul muro: rincorsa, salta accanto al muro di mattoni e tieni avanti (fino a 5 blocchi).",
        "Scivolata: corri e premi crouch per passare sotto il blocco.",
        "Capriola all'indietro: dai le spalle al gradino, tieni indietro e salta.",
        "Salto dal muro: salta contro un muro e risalta per rimbalzare sull'altro, sempre più su.",
        "Rope gun: tasto destro sulla trave d'acciaio per agganciarti, avanti per oscillare, salto per staccarti.",
        "La torre: sette prove in salita. In cima c'e' la Chiave della Vetta.",
        "Torre 1/7: il camino. Salta da un muro all'altro.",
        "Torre 2/7: corsa sul muro.",
        "Torre 3/7: capriola all'indietro sul gradino.",
        "Torre 4/7: di nuovo il camino, piu' in alto.",
        "Torre 5/7: scivolata.",
        "Torre 6/7: la corda.",
        "Torre 7/7: l'ultimo camino.",
        "La cima.",
    }

    core.register_mapgen("parkour:course", function(ctx)
        for _, b in BOXES do
            local x0, x1 = math.max(b[1], ctx.min_x), math.min(b[4], ctx.max_x)
            local z0, z1 = math.max(b[3], ctx.min_z), math.min(b[6], ctx.max_z)
            if x0 <= x1 and z0 <= z1 then
                for x = x0, x1 do
                    for z = z0, z1 do
                        for y = b[2], b[5] do
                            core.set_block(x, y, z, b[7])
                        end
                    end
                end
            end
        end
    end)

    -- Portal 0 in the plaza's doorway (facing +z, the spawn four in front of
    -- it), portal 1 in the last one (facing +x).
    core.set_portal(0, 0.5, 22, -6.4)
    -- At the top of the tower (the doorway built above, its front +x).
    core.set_portal(1, tx - 9.4, tf + 2, tz + 0.5)

    core.event.on("core:pre_break", function(ev)
        ev.cancel = true
    end)
    core.event.on("core:pre_place", function(ev)
        ev.cancel = true
    end)

    -- Every player's last pad, and the rope gun handed to whoever arrives.
    local reached: { [number]: number } = {}
    core.event.on("core:tick", function(_)
        local here: { [number]: boolean } = {}
        for _, p in core.players() do
            here[p] = true
            local x, y, z = core.player_pos(p)
            if x == nil then continue end
            if reached[p] == nil then
                reached[p] = 1
                -- Exactly one, however many they came with.
                core.take_item(p, GUN, 99)
                core.give_item(p, GUN, 1)
                core.notice(p, HINTS[1])
            end
            for i, pad in PADS do
                if i ~= reached[p] and math.abs(x - (pad[1] + 0.5)) < 1.2 and math.abs(z - (pad[3] + 0.5)) < 1.2
                    and math.abs(y - (pad[2] + 1)) < 0.6 then
                    reached[p] = i
                    core.notice(p, HINTS[i])
                    -- The summit: the trial passed, its key on the altar (mods/keys).
                    if i == #PADS then
                        core.event.emit("keys:earned", { player = p, key = "parkour" })
                    end
                end
            end
            if y < 12 then
                local pad = PADS[reached[p]]
                core.player_place(p, pad[1] + 0.5, pad[2] + 1, pad[3] + 0.5, -1, 0, 0)
            end
        end
        for p in reached do
            if not here[p] then reached[p] = nil end
        end
    end)
end

-- ================================================================ the player

if drawing then
    -- The rope gun in the hand: right shoots the rope, left lets go. Both
    -- buttons are the gun's while it is held: never a dig or a blow.
    core.event.on("core:input", function(ev)
        if core.held_item(1) ~= GUN then return end
        if ev.action == "mouse_right" then
            ev.cancel = true
            core.rope(true)
        elseif ev.action == "mouse_left" then
            ev.cancel = true
            core.rope(false)
        end
    end)
end
