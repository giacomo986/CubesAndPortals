--!strict
-- building: The classic block game's blocks for building with.
--
--   bookshelf   six planks and three books; gives three books back
--   TNT         lit by flint and steel (infernoportal:lighter) or by a powered
--               lever, button, torch, dust, repeater or cinnabar block beside
--               it; four seconds later it blows up everything within about
--               four blocks but bedrock, obsidian and liquids, hurts whoever
--               is near, sets off the TNT round it, and a third of what it
--               breaks is left lying. Gunpowder comes from zombies here.
--   fences      one per wood, joined to the fences, gates and solid blocks
--               beside them; gates open and shut with a right click
--   glass panes and iron bars, joined the same way
--   trapdoors   one per wood, in the bottom or the top half of the cell by
--               where it was clicked, hinged on the wall it was put against;
--               a right click or cinnabar opens it, and open over a ladder
--               it is climbed
--   signs       one per wood, standing or on a wall; whoever puts one down is
--               asked what it says, and anybody looking at it reads it
--   anvil       falls, hurts what it lands on, and mends: a right click with a
--               worn tool in the hand spends an iron ingot for a quarter of it
--   cauldron    holds a bucket of water, and gives it back
--   hay bale    nine wheat
--   rails       six iron and a stick make sixteen
--   stained glass  eight glass round a dye make eight of its colour, sixteen
--               colours, and six of it sixteen panes, joined as panes are
--   walls       six cobblestone, mossy cobblestone, stone bricks or bricks make
--               six, lower than a block and joined as fences are
--   tiles       four terracotta of a colour make four glazed tiles; four stone
--               bricks four stone tiles; white and black concrete a chequer
--   chairs and tables  one per wood, of its planks and sticks; a chair faces
--               whoever put it down
--   carpets     two wool of a colour make three, a sixteenth high
--   lantern     iron round a torch, hanging or standing, light 14
local WOODS = { "oak", "birch", "spruce", "jungle", "acacia", "dark_oak" }
local function planks_of(w: string): string
    return if w == "oak" then "timber:planks" else `woods:{w}_planks`
end

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local shown_sign = ""   -- the text of the sign this machine's player looks at

if serving then
    core.net.open()

    local function node(name: string, rgb: { number }, solid: boolean, turn: string, secs: number,
                        material: string, light: number?): number
        return core.register_node(name, rgb[1], rgb[2], rgb[3], solid, light or 0, turn, 0, secs, material)
    end
    local function boxes(id: number, list: { { number } })
        for _, b in list do
            core.node_box(id, b[1], b[2], b[3], b[4], b[5], b[6])
        end
    end

    -- ------------------------------------------------------------ bookshelf, hay, TNT

    local SHELF = node("building:bookshelf", { 160, 130, 80 }, true, "", 1.5, "wood")
    -- Its item stays for the recipe; breaking it gives the three books back.
    core.event.on("core:broken", function(ev)
        if ev.id == SHELF and core.take_item(ev.player, "building:bookshelf", 1) == 1 then
            core.give_item(ev.player, "cane:book", 3)
        end
    end)
    core.recipe_shaped("building:bookshelf", 1,
        "timber:planks timber:planks timber:planks",
        "cane:book     cane:book     cane:book",
        "timber:planks timber:planks timber:planks")

    node("building:hay_bale", { 200, 170, 50 }, true, "", 0.5, "")
    core.recipe_shaped("building:hay_bale", 1, "farm:wheat farm:wheat farm:wheat",
        "farm:wheat farm:wheat farm:wheat", "farm:wheat farm:wheat farm:wheat")
    core.recipe_shapeless("farm:wheat", 9, "building:hay_bale")

    local TNT = node("building:tnt", { 200, 50, 40 }, true, "", 0.0, "")
    local LIT = node("building:tnt_lit", { 250, 250, 240 }, true, "", 0.0, "", 6)
    core.node_drop(LIT, TNT)
    core.register_item("building:gunpowder", 64, "", 0, 1.0, "")
    core.mob_drop("mobs:zombie", "building:gunpowder", 2)
    core.recipe_shaped("building:tnt", 1,
        "building:gunpowder core:sand          building:gunpowder",
        "core:sand          building:gunpowder core:sand",
        "building:gunpowder core:sand          building:gunpowder")

    local fuses: { [string]: { x: number, y: number, z: number, t: number } } = {}
    local function light(x: number, y: number, z: number, t: number)
        local key = `{x},{y},{z}`
        if fuses[key] == nil then
            core.set_block(x, y, z, LIT)
            fuses[key] = { x = x, y = y, z = z, t = t }
        end
    end

    local HARD: { [string]: boolean } = { ["core:bedrock"] = true, ["infernoportal:obsidian"] = true,
                                          ["rocks:lava"] = true, ["building:anvil"] = true }
    local function explode(cx: number, cy: number, cz: number)
        core.set_block(cx, cy, cz, 0)
        local R = 4
        for dy = -R, R do
            for dz = -R, R do
                for dx = -R, R do
                    local d = math.sqrt(dx * dx + dy * dy + dz * dz)
                    if d <= R - 0.5 + math.random() then
                        local x, y, z = cx + dx, cy + dy, cz + dz
                        local id = core.get_block(x, y, z)
                        if id == TNT then
                            light(x, y, z, 0.5 + math.random())
                        elseif id ~= 0 and id ~= LIT then
                            local name = core.node_name(id)
                            if not HARD[name] and not string.find(name, "^lakes:") then
                                core.set_block(x, y, z, 0)
                                if math.random() < 0.3 then
                                    local item = core.item_id(name)
                                    if item ~= 0 then
                                        core.drop_item(x, y, z, item, 1)
                                    end
                                end
                            end
                        end
                    end
                end
            end
        end
        core.blast(cx + 0.5, cy + 0.5, cz + 0.5, 7, 30)
        for _, p in core.players() do
            local x, y, z = core.player_pos(p)
            if x ~= nil then
                local d = math.sqrt((x - cx - 0.5) ^ 2 + (y + 0.9 - cy - 0.5) ^ 2 + (z - cz - 0.5) ^ 2)
                if d < 7 then
                    core.damage(p, math.ceil(22 * (1 - d / 7)))
                end
            end
        end
        core.log(`building: TNT went off at {cx} {cy} {cz}`)
    end

    core.event.on("core:used", function(ev)
        if ev.id == TNT and ev.held == "infernoportal:lighter" then
            light(ev.x, ev.y, ev.z, 4)
            ev.cancel = true
        end
    end)

    -- Power beside it: what cinnabar is ON.
    local ON: { [number]: boolean } = {}
    for _, n in { "cinnabar:lever_on", "cinnabar:button_on", "cinnabar:torch", "cinnabar:dust_on",
                  "cinnabar:repeater_on", "cinnabar:comparator_on", "cinnabar:observer_on",
                  "compact:cinnabar" } do
        local id = core.node_id(n)
        if id ~= 0 then
            ON[id] = true
        end
    end
    local function powered(x: number, y: number, z: number): boolean
        for _, d in { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } } do
            if ON[core.get_block(x + d[1], y + d[2], z + d[3])] then
                return true
            end
        end
        return false
    end
    core.register_abm("building:tnt_powered", { "building:tnt" }, 0.5, 1, function(cell)
        if powered(cell.x, cell.y, cell.z) then
            light(cell.x, cell.y, cell.z, 4)
        end
    end)

    core.event.on("core:tick", function(ev)
        for key, f in fuses do
            f.t -= ev.dt
            if f.t <= 0 then
                fuses[key] = nil
                if core.get_block(f.x, f.y, f.z) == LIT then
                    explode(f.x, f.y, f.z)
                end
            end
        end
    end)

    -- ------------------------------------------------------------ joined things

    -- A post and what reaches out of it towards each joined side: bit 1 +X,
    -- 2 -X, 4 +Z, 8 -Z. Sixteen blocks for each kind, dropping as the first.
    local SIDES = { { 1, 0, 1 }, { -1, 0, 2 }, { 0, 1, 4 }, { 0, -1, 8 } }
    local JOINED: { [number]: { base: number, variants: { number } } } = {}
    local JOINS: { [number]: boolean } = {}   -- what a fence joins to besides solid blocks

    local function joined(prefix: string, rgb: { number }, secs: number, material: string,
                          post: { number }, arm: (number) -> { { number } }, see: number?)
        local variants: { number } = {}
        for mask = 0, 15 do
            local name = if mask == 0 then prefix else `{prefix}_{mask}`
            local id = node(name, rgb, true, "", secs, material)
            core.node_box(id, post[1], post[2], post[3], post[4], post[5], post[6])
            for _, s in SIDES do
                if bit32.band(mask, s[3]) ~= 0 then
                    boxes(id, arm(s[3]))
                end
            end
            if see then
                core.node_glass(id, see)
            end
            variants[mask + 1] = id
        end
        for mask = 1, 15 do
            core.node_drop(variants[mask + 1], variants[1])
        end
        for _, id in variants do
            JOINED[id] = { base = variants[1], variants = variants }
            JOINS[id] = true
        end
    end

    -- Rails of a fence, and the panel of a pane, out towards one side.
    local function reach(lo: number, hi: number, heights: { { number } }): (number) -> { { number } }
        return function(bitv: number): { { number } }
            local out = {}
            for _, h in heights do
                if bitv == 1 then
                    table.insert(out, { hi, h[1], lo, 1, h[2], hi })
                elseif bitv == 2 then
                    table.insert(out, { 0, h[1], lo, lo, h[2], hi })
                elseif bitv == 4 then
                    table.insert(out, { lo, h[1], hi, hi, h[2], 1 })
                else
                    table.insert(out, { lo, h[1], 0, hi, h[2], lo })
                end
            end
            return out
        end
    end

    for _, w in WOODS do
        joined(`building:{w}_fence`, { 160, 130, 80 }, 2.0, "wood", { 0.375, 0, 0.375, 0.625, 1, 0.625 },
               reach(0.4375, 0.5625, { { 0.375, 0.5625 }, { 0.75, 0.9375 } }))
        core.recipe_shaped(`building:{w}_fence`, 3,
            `{planks_of(w)} timber:stick {planks_of(w)}`,
            `{planks_of(w)} timber:stick {planks_of(w)}`)
    end
    joined("building:glass_pane", { 205, 235, 245 }, 0.3, "glass", { 0.4375, 0, 0.4375, 0.5625, 1, 0.5625 },
           reach(0.4375, 0.5625, { { 0, 1 } }), 110)
    core.recipe_shaped("building:glass_pane", 16, "core:glass core:glass core:glass",
        "core:glass core:glass core:glass")
    joined("building:iron_bars", { 170, 170, 176 }, 5.0, "stone", { 0.4375, 0, 0.4375, 0.5625, 1, 0.5625 },
           reach(0.46875, 0.53125, { { 0, 1 } }))
    core.recipe_shaped("building:iron_bars", 16,
        "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot",
        "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot")

    -- ------------------------------------------------------------ decoration

    -- The sixteen colours of mods/colors, in its order, and their dyes.
    local COLOURS = {
        { "white", { 234, 236, 236 } }, { "orange", { 240, 118, 20 } }, { "magenta", { 190, 68, 180 } },
        { "light_blue", { 58, 176, 218 } }, { "yellow", { 248, 198, 40 } }, { "lime", { 112, 186, 26 } },
        { "pink", { 238, 142, 172 } }, { "gray", { 62, 68, 72 } }, { "light_gray", { 142, 142, 136 } },
        { "cyan", { 22, 138, 146 } }, { "purple", { 122, 42, 172 } }, { "blue", { 52, 56, 158 } },
        { "brown", { 114, 72, 40 } }, { "green", { 84, 110, 28 } }, { "red", { 160, 38, 34 } },
        { "black", { 20, 20, 24 } },
    }
    for _, entry in COLOURS do
        local c, rgb = entry[1], entry[2]
        local dye = `colors:{c}_dye`
        local glass = node(`building:{c}_stained_glass`, rgb, true, "", 0.3, "glass")
        core.node_glass(glass, 150)
        core.recipe_shaped(`building:{c}_stained_glass`, 8,
            "core:glass core:glass core:glass",
            `core:glass {dye} core:glass`,
            "core:glass core:glass core:glass")
        joined(`building:{c}_stained_glass_pane`, rgb, 0.3, "glass", { 0.4375, 0, 0.4375, 0.5625, 1, 0.5625 },
               reach(0.4375, 0.5625, { { 0, 1 } }), 150)
        core.recipe_shaped(`building:{c}_stained_glass_pane`, 16,
            `building:{c}_stained_glass building:{c}_stained_glass building:{c}_stained_glass`,
            `building:{c}_stained_glass building:{c}_stained_glass building:{c}_stained_glass`)
        node(`building:{c}_tiles`, rgb, true, "", 1.4, "stone")
        core.recipe_shaped(`building:{c}_tiles`, 4,
            `colors:{c}_terracotta colors:{c}_terracotta`,
            `colors:{c}_terracotta colors:{c}_terracotta`)
        local carpet = node(`building:{c}_carpet`, rgb, false, "", 0.1, "")
        core.node_box(carpet, 0, 0, 0, 1, 1 / 16, 1)
        core.recipe_shaped(`building:{c}_carpet`, 3, `colors:{c}_wool colors:{c}_wool`)
    end
    node("building:stone_tiles", { 150, 150, 152 }, true, "", 2.0, "stone")
    core.recipe_shaped("building:stone_tiles", 4, "rocks:stone_bricks rocks:stone_bricks",
        "rocks:stone_bricks rocks:stone_bricks")
    node("building:chequer_tiles", { 130, 130, 130 }, true, "", 1.8, "stone")
    core.recipe_shaped("building:chequer_tiles", 4, "colors:white_concrete colors:black_concrete",
        "colors:black_concrete colors:white_concrete")

    -- Walls: a post, and a wall lower than it out to each joined side.
    for _, w in { { "cobblestone", "rocks:cobblestone", { 120, 120, 120 } },
                  { "mossy_cobblestone", "rocks:mossy_cobblestone", { 100, 120, 90 } },
                  { "stone_brick", "rocks:stone_bricks", { 122, 122, 124 } },
                  { "brick", "craft:brick", { 190, 80, 60 } } } do
        joined(`building:{w[1]}_wall`, w[3], 2.0, "stone", { 0.25, 0, 0.25, 0.75, 1, 0.75 },
               reach(0.3125, 0.6875, { { 0, 0.8125 } }))
        core.recipe_shaped(`building:{w[1]}_wall`, 6, `{w[2]} {w[2]} {w[2]}`, `{w[2]} {w[2]} {w[2]}`)
    end

    -- Chairs and tables, one of each per wood.
    for _, w in WOODS do
        local chair = node(`building:{w}_chair`, { 160, 130, 80 }, true, "upright", 1.0, "wood")
        boxes(chair, { { 0.15, 0.45, 0.15, 0.85, 0.55, 0.85 },                        -- seat
                       { 0.15, 0, 0.15, 0.25, 0.45, 0.25 }, { 0.75, 0, 0.15, 0.85, 0.45, 0.25 },   -- legs
                       { 0.15, 0, 0.75, 0.25, 0.45, 0.85 }, { 0.75, 0, 0.75, 0.85, 0.45, 0.85 },
                       { 0.15, 0.55, 0.75, 0.85, 1.1, 0.85 } })                         -- back
        core.recipe_shaped(`building:{w}_chair`, 2,
            "timber:stick  -",
            `{planks_of(w)} {planks_of(w)}`,
            "timber:stick  timber:stick")
        local tab = node(`building:{w}_table`, { 160, 130, 80 }, true, "", 1.2, "wood")
        boxes(tab, { { 0, 0.85, 0, 1, 1, 1 },
                     { 0.06, 0, 0.06, 0.18, 0.85, 0.18 }, { 0.82, 0, 0.06, 0.94, 0.85, 0.18 },
                     { 0.06, 0, 0.82, 0.18, 0.85, 0.94 }, { 0.82, 0, 0.82, 0.94, 0.85, 0.94 } })
        core.recipe_shaped(`building:{w}_table`, 1,
            `{planks_of(w)} {planks_of(w)} {planks_of(w)}`,
            "timber:stick  -            timber:stick")
    end

    -- A lantern: iron round a torch, hung under a block or stood on one.
    local LANTERN = node("building:lantern", { 230, 190, 110 }, false, "", 0.6, "stone", 14)
    boxes(LANTERN, { { 0.3, 0, 0.3, 0.7, 0.45, 0.7 }, { 0.36, 0.45, 0.36, 0.64, 0.52, 0.64 },
                     { 0.47, 0.52, 0.47, 0.53, 0.62, 0.53 } })
    local HUNG = node("building:lantern_hung", { 230, 190, 110 }, false, "", 0.6, "stone", 14)
    boxes(HUNG, { { 0.3, 0.38, 0.3, 0.7, 0.83, 0.7 }, { 0.36, 0.83, 0.36, 0.64, 0.9, 0.64 },
                  { 0.47, 0.9, 0.47, 0.53, 1, 0.53 } })
    core.node_drop(HUNG, LANTERN)
    core.recipe_shaped("building:lantern", 1,
        "-                  furnace:iron_ingot -",
        "furnace:iron_ingot torches:torch      furnace:iron_ingot",
        "-                  furnace:iron_ingot -")
    -- Put against the underside of a block, it hangs from it.
    core.event.on("core:placed", function(ev)
        if ev.id == LANTERN and core.get_block(ev.x, ev.y - 1, ev.z) == 0 and core.get_block(ev.x, ev.y + 1, ev.z) ~= 0 then
            core.set_block(ev.x, ev.y, ev.z, HUNG)
        end
    end)

    -- Gates, shut across and open as two posts; trapdoors, down and up.
    local TOGGLE: { [number]: number } = {}
    local TRAPDOOR: { [number]: boolean } = {}   -- the shut ones
    local RUNG: { [number]: number } = {}        -- open -> open over a ladder
    local PLAIN: { [number]: number } = {}       -- and back
    for _, w in WOODS do
        local shut = node(`building:{w}_fence_gate`, { 160, 130, 80 }, true, "upright", 2.0, "wood")
        local open = node(`building:{w}_fence_gate_open`, { 160, 130, 80 }, true, "upright", 2.0, "wood")
        boxes(shut, { { 0, 0.3125, 0.4375, 0.125, 1, 0.5625 }, { 0.875, 0.3125, 0.4375, 1, 1, 0.5625 },
                      { 0.125, 0.375, 0.4375, 0.875, 0.5625, 0.5625 }, { 0.125, 0.75, 0.4375, 0.875, 0.9375, 0.5625 } })
        boxes(open, { { 0, 0.3125, 0.4375, 0.125, 1, 0.5625 }, { 0.875, 0.3125, 0.4375, 1, 1, 0.5625 },
                      { 0, 0.375, 0.5625, 0.125, 0.9375, 1 }, { 0.875, 0.375, 0.5625, 1, 0.9375, 1 } })
        core.node_door(shut, open)
        core.node_drop(open, shut)
        TOGGLE[shut], TOGGLE[open] = open, shut
        JOINS[shut], JOINS[open] = true, true
        core.recipe_shaped(`building:{w}_fence_gate`, 1,
            `timber:stick {planks_of(w)} timber:stick`,
            `timber:stick {planks_of(w)} timber:stick`)

        -- A trapdoor, as in the classic block game. Shut, a board in the bottom of its cell,
        -- or in the top: "facedir", so the top one is the same block upside
        -- down (TRAP_TOP). Open, stood up against the hinge side, which is the
        -- back (-Z unturned) -- the far side from whoever put it on a floor or
        -- a ceiling, the wall it was put against. Open over a ladder, it is
        -- climbed as the ladder's last rung, so a hatch over a shaft lets a
        -- body up through it.
        local down = node(`building:{w}_trapdoor`, { 160, 130, 80 }, true, "facedir", 3.0, "wood")
        local up = node(`building:{w}_trapdoor_open`, { 160, 130, 80 }, true, "facedir", 3.0, "wood")
        local rung = node(`building:{w}_trapdoor_rung`, { 160, 130, 80 }, true, "facedir", 3.0, "wood")
        -- As thin as a ladder, so a body climbing one is not stopped by the
        -- board stood up over it.
        core.node_box(down, 0, 0, 0, 1, 0.125, 1)
        core.node_box(up, 0, 0, 0, 1, 1, 0.125)
        core.node_box(rung, 0, 0, 0, 1, 1, 0.125)
        core.node_climbable(rung)
        -- A door to the engine, so one shut on a body lets it out.
        core.node_door(down, up)
        core.node_drop(up, down)
        core.node_drop(rung, down)
        TOGGLE[down], TOGGLE[up], TOGGLE[rung] = up, down, down
        TRAPDOOR[down], RUNG[up], PLAIN[rung] = true, rung, up
        core.recipe_shaped(`building:{w}_trapdoor`, 2,
            `{planks_of(w)} {planks_of(w)} {planks_of(w)}`,
            `{planks_of(w)} {planks_of(w)} {planks_of(w)}`)
    end

    -- What a trapdoor opened over is climbed with.
    local function ladder_under(x: number, y: number, z: number): boolean
        local n = core.node_name(core.get_block(x, y - 1, z))
        return n == "timber:rungs" or n == "plants:vine"
    end

    local function toggle(x: number, y: number, z: number, id: number)
        local other = TOGGLE[id]
        if RUNG[other] ~= nil and ladder_under(x, y, z) then
            other = RUNG[other]
        end
        local facing = core.get_facing(x, y, z)
        core.set_block(x, y, z, other)
        core.set_facing(x, y, z, facing)
        if TRAPDOOR[id] or TRAPDOOR[other] then
            core.play_sound(if TRAPDOOR[other] then "building:trapdoor_shut" else "building:trapdoor_open",
                            x + 0.5, y + 0.5, z + 0.5)
        end
    end

    core.event.on("core:used", function(ev)
        if TOGGLE[ev.id] ~= nil then
            toggle(ev.x, ev.y, ev.z, ev.id)
            ev.cancel = true
        end
    end)

    -- The upside-down orientation of each upright turn, keeping where the
    -- front points: 20 + t is turned t, then over about X (core/media/
    -- face_tiles.h), which sends a front along Z the other way.
    local TRAP_TOP = { [0] = 22, 21, 20, 23 }
    -- The upright turn whose front (+Z unturned) points along x, z.
    local function turn_to(x: number, z: number): number
        if math.abs(x) > math.abs(z) then
            return if x > 0 then 1 else 3
        end
        return if z > 0 then 0 else 2
    end

    -- Put down: which half and which hinge, from where the placer's look
    -- leaves the cell -- through the face of the block they clicked. A floor
    -- is the bottom half and a ceiling the top, each opening away from them;
    -- a wall's side is the half they clicked, hinged on that wall.
    core.event.on("core:placed", function(ev)
        if not TRAPDOOR[ev.id] then
            return
        end
        local px, py, pz = core.player_pos(ev.player)
        local lx, ly, lz = core.player_look(ev.player)
        if px == nil or py == nil or pz == nil or lx == nil or ly == nil or lz == nil then
            return
        end
        local e = { px, py + 1.62, pz }
        local l = { lx, ly, lz }
        local c = { ev.x, ev.y, ev.z }
        local t, axis = math.huge, 0
        for a = 1, 3 do
            if l[a] ~= 0 then
                local far = (c[a] + (if l[a] > 0 then 1 else 0) - e[a]) / l[a]
                if far < t then
                    t, axis = far, a
                end
            end
        end
        local turn = core.get_facing(ev.x, ev.y, ev.z)
        local top = false
        if axis == 2 then
            top = l[2] > 0
        elseif axis ~= 0 then
            -- The front away from the wall, towards the placer.
            turn = if axis == 1 then turn_to(-l[1], 0) else turn_to(0, -l[3])
            top = e[2] + l[2] * t - ev.y > 0.5
        end
        core.set_facing(ev.x, ev.y, ev.z, if top then TRAP_TOP[turn] else turn)
    end)

    -- Cinnabar opens one and shuts it again when the power goes; a hand still
    -- opens or shuts it while the power stays as it is. The same sweep keeps
    -- an open one climbable just while it has a ladder under it.
    local POWERED: { [string]: boolean } = {}
    local trapdoors: { string } = {}
    for _, w in WOODS do
        for _, k in { "", "_open", "_rung" } do
            table.insert(trapdoors, `building:{w}_trapdoor{k}`)
        end
    end
    core.register_abm("building:trapdoor_powered", trapdoors, 0.25, 1, function(cell)
        -- Open, climbed while a ladder is under it and only then: one put
        -- under it or taken away since it opened changes which it is.
        local swap = if ladder_under(cell.x, cell.y, cell.z) then RUNG[cell.id] else PLAIN[cell.id]
        if swap ~= nil then
            local facing = core.get_facing(cell.x, cell.y, cell.z)
            core.set_block(cell.x, cell.y, cell.z, swap)
            core.set_facing(cell.x, cell.y, cell.z, facing)
            cell.id = swap
        end
        local key = `{cell.x},{cell.y},{cell.z}`
        local on = powered(cell.x, cell.y, cell.z)
        if on == (POWERED[key] == true) then
            return
        end
        POWERED[key] = if on then true else nil
        if (TRAPDOOR[cell.id] == true) == on then
            toggle(cell.x, cell.y, cell.z, cell.id)
        end
    end)

    local function joins_to(id: number): boolean
        return id ~= 0 and (JOINS[id] == true or (core.node_solid(id) == 1 and JOINED[id] == nil))
    end

    -- A fence, pane or bars cell takes the variant its neighbours call for.
    local function rejoin(x: number, y: number, z: number)
        local me = JOINED[core.get_block(x, y, z)]
        if me == nil then
            return
        end
        local mask = 0
        for _, s in SIDES do
            local n = core.get_block(x + s[1], y, z + s[2])
            local them = JOINED[n]
            if (them ~= nil and them.base == me.base) or (them == nil and joins_to(n)) then
                mask += s[3]
            end
        end
        local want = me.variants[mask + 1]
        if core.get_block(x, y, z) ~= want then
            core.set_block(x, y, z, want)
        end
    end

    local function around(ev)
        rejoin(ev.x, ev.y, ev.z)
        for _, s in SIDES do
            rejoin(ev.x + s[1], ev.y, ev.z + s[2])
        end
    end
    core.event.on("core:placed", around)
    core.event.on("core:broken", around)

    -- ------------------------------------------------------------ signs

    local SIGNS: { [number]: boolean } = {}
    for _, w in WOODS do
        -- On a wall: a board against the face it was put on.
        local wall = node(`building:{w}_sign`, { 160, 130, 80 }, false, "wallmounted", 1.0, "wood")
        core.node_box(wall, 0.0625, 0.25, 0, 0.9375, 0.8125, 0.0625)
        -- Standing: a board on a post, facing whoever put it down.
        local stand = node(`building:{w}_standing_sign`, { 160, 130, 80 }, false, "upright", 1.0, "wood")
        core.node_box(stand, 0.0625, 0.5, 0.46875, 0.9375, 1.0, 0.53125)
        core.node_box(stand, 0.46875, 0, 0.46875, 0.53125, 0.5, 0.53125)
        core.node_drop(stand, wall)
        SIGNS[wall], SIGNS[stand] = true, true
        core.recipe_shaped(`building:{w}_sign`, 3,
            `{planks_of(w)} {planks_of(w)} {planks_of(w)}`,
            `{planks_of(w)} {planks_of(w)} {planks_of(w)}`,
            "-  timber:stick  -")
        core.event.on("core:placed", function(ev)
            if ev.id ~= wall then
                return
            end
            -- Put down on a floor: the standing one, turned to its placer.
            if core.get_facing(ev.x, ev.y, ev.z) == 2 then
                local lx, _, lz = core.player_look(ev.player)
                core.set_block(ev.x, ev.y, ev.z, stand)
                if lx ~= nil then
                    local turn = if math.abs(lx) > math.abs(lz) then (if lx > 0 then 3 else 1)
                                 else (if lz > 0 then 2 else 0)
                    core.set_facing(ev.x, ev.y, ev.z, turn)
                end
            end
        end)
    end

    local editing: { [number]: { number } } = {}
    core.ui_define("building:sign", 10, 3)
    core.ui_label("building:sign", 0.3, 0.2, "What does the sign say? (Enter)")
    core.ui_field("building:sign", "text", 0.3, 1.2, 9.4, 1.2, "")
    core.event.on("core:placed", function(ev)
        if SIGNS[ev.id] then
            editing[ev.player] = { ev.x, ev.y, ev.z }
            core.ui_show("building:sign", true, ev.player)
        end
    end)
    core.event.on("core:ui_click", function(ev)
        if ev.panel == "building:sign" and ev.id == "text" then
            local at = editing[ev.player]
            if at ~= nil then
                core.meta_set(at[1], at[2], at[3], "text", string.sub(ev.text or "", 1, 90))
                editing[ev.player] = nil
            end
            core.ui_show("building:sign", false, ev.player)
        end
    end)

    -- What each player is looking at, read out to them when it is a sign.
    local reading: { [number]: string } = {}
    local look_clock = 0
    core.event.on("core:tick", function(ev)
        look_clock += ev.dt
        if look_clock < 0.25 then
            return
        end
        look_clock = 0
        for _, p in core.players() do
            local x, y, z = core.player_pos(p)
            local lx, ly, lz = core.player_look(p)
            local text = ""
            if x ~= nil and lx ~= nil then
                local hx, hy, hz = core.raycast(x, y + 1.5, z, lx, ly, lz, 6)
                if hx ~= nil and SIGNS[core.get_block(hx, hy, hz)] then
                    text = core.meta_get(hx, hy, hz, "text") or ""
                    if text == "" then
                        text = " "
                    end
                end
            end
            if reading[p] ~= text then
                reading[p] = text
                if drawing then
                    shown_sign = text
                else
                    core.net.send(p, "sign " .. text)
                end
            end
        end
    end)

    -- ------------------------------------------------------------ anvil

    local ANVIL = node("building:anvil", { 68, 68, 72 }, true, "upright", 5.0, "stone")
    boxes(ANVIL, { { 0.125, 0, 0.125, 0.875, 0.25, 0.875 }, { 0.25, 0.25, 0.3125, 0.75, 0.625, 0.6875 },
                   { 0, 0.625, 0.1875, 1, 1, 0.8125 } })
    core.node_falls(ANVIL)
    core.recipe_shaped("building:anvil", 1,
        "compact:iron       compact:iron       compact:iron",
        "-                  furnace:iron_ingot -",
        "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot")
    core.event.on("core:landed", function(ev)
        if ev.id ~= ANVIL or ev.fell < 2 then
            return
        end
        local hurt = math.min(40, 2 * (ev.fell - 1))
        core.blast(ev.x + 0.5, ev.y + 0.5, ev.z + 0.5, 0.9, hurt)
        for _, p in core.players() do
            local x, y, z = core.player_pos(p)
            if x ~= nil and math.floor(x) == ev.x and math.floor(z) == ev.z and y >= ev.y - 2 and y <= ev.y + 1 then
                core.damage(p, hurt)
            end
        end
    end)
    core.event.on("core:used", function(ev)
        if ev.id ~= ANVIL or ev.held == "" then
            return
        end
        ev.cancel = true
        if core.take_item(ev.player, "furnace:iron_ingot", 1) == 0 then
            core.notice(ev.player, "An iron ingot mends it")
            return
        end
        -- A quarter of what the tool can take: nobody knows its durability
        -- here, so a quarter of 400, and anything it had not spent is kept.
        local was = core.repair_held(ev.player, 100)
        if was <= 0 then
            core.give_item(ev.player, "furnace:iron_ingot", 1)
            core.notice(ev.player, "Nothing to mend")
        else
            core.notice(ev.player, "Mended")
        end
    end)

    -- ------------------------------------------------------------ cauldron

    local CAULDRON: { number } = {}
    for level = 0, 3 do
        local id = node(if level == 0 then "building:cauldron" else `building:cauldron_{level}`,
                        { 60, 60, 64 }, true, "", 2.0, "stone")
        boxes(id, { { 0, 0.1875, 0, 1, 0.3125, 1 }, { 0, 0.3125, 0, 0.125, 1, 1 }, { 0.875, 0.3125, 0, 1, 1, 1 },
                    { 0.125, 0.3125, 0, 0.875, 1, 0.125 }, { 0.125, 0.3125, 0.875, 0.875, 1, 1 },
                    { 0, 0, 0, 0.25, 0.1875, 0.25 }, { 0.75, 0, 0, 1, 0.1875, 0.25 },
                    { 0, 0, 0.75, 0.25, 0.1875, 1 }, { 0.75, 0, 0.75, 1, 0.1875, 1 } })
        if level > 0 then
            core.node_box(id, 0.125, 0.3125, 0.125, 0.875, 0.3125 + 0.1875 * level, 0.875)
            core.node_drop(id, CAULDRON[1])
        end
        CAULDRON[level + 1] = id
    end
    core.recipe_shaped("building:cauldron", 1,
        "furnace:iron_ingot -                  furnace:iron_ingot",
        "furnace:iron_ingot -                  furnace:iron_ingot",
        "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot")
    core.event.on("core:used", function(ev)
        if ev.id == CAULDRON[1] and ev.held == "lakes:bucket_water" then
            if core.take_item(ev.player, "lakes:bucket_water", 1) == 1 then
                core.give_item(ev.player, "lakes:bucket", 1)
                core.set_block(ev.x, ev.y, ev.z, CAULDRON[4])
            end
            ev.cancel = true
        elseif ev.id == CAULDRON[4] and ev.held == "lakes:bucket" then
            if core.take_item(ev.player, "lakes:bucket", 1) == 1 then
                core.give_item(ev.player, "lakes:bucket_water", 1)
                core.set_block(ev.x, ev.y, ev.z, CAULDRON[1])
            end
            ev.cancel = true
        end
    end)

    -- ------------------------------------------------------------ rails

    local RAIL = node("building:rail", { 150, 130, 110 }, false, "upright", 0.7, "stone")
    core.node_model(RAIL, core.register_model("building:rail", "rail.gltf"))
    core.node_box(RAIL, 0, 0, 0, 1, 0.125, 1)
    core.recipe_shaped("building:rail", 16,
        "furnace:iron_ingot -            furnace:iron_ingot",
        "furnace:iron_ingot timber:stick furnace:iron_ingot",
        "furnace:iron_ingot -            furnace:iron_ingot")

    -- Ridden by a body in a cart (mods/carts, core.node_conveyor's `rail`): one
    -- way, away from whoever laid them, turning where a rail on its own level
    -- runs into a side. A rail brings a cart to 6 m/s. A powered rail is a
    -- brake while off -- a station: a cart stops on it and forward held pushes
    -- it off -- and 11 m/s on. A switch turns a cart right while powered,
    -- straight on else. Slopes up and down a block, slowed going up and sped
    -- down. mods/cinnabar powers them, as a lamp. The new ones are flat boxes
    -- along +Z turned with the cell; a slope is four steps.
    local PRAIL     = node("building:powered_rail", { 150, 120, 60 }, false, "upright", 0.7, "stone")
    local PRAIL_ON  = node("building:powered_rail_on", { 230, 170, 60 }, false, "upright", 0.7, "stone", 4)
    local SWITCH    = node("building:switch", { 120, 110, 140 }, false, "upright", 0.7, "stone")
    local SWITCH_ON = node("building:switch_on", { 150, 110, 170 }, false, "upright", 0.7, "stone")
    -- A rail where the line turns, right or left: two quarter arcs about the
    -- corner of the side it comes in by and the one it leaves by (-Z), as the
    -- engine rides it (chute_at). Laid as a rail; swapped in by the line round it.
    local CORNER_R  = node("building:rail_right", { 175, 175, 180 }, false, "upright", 0.7, "stone")
    local CORNER_L  = node("building:rail_left", { 175, 175, 180 }, false, "upright", 0.7, "stone")
    local function arcs(id: number, mirror: boolean, w: number)
        for _, r in { 0.17, 0.83 } do
            local n = if r < 0.5 then 4 else 8
            for i = 0, n do
                local th = i / n * math.pi / 2
                local x, z = 1 - r * math.sin(th), r * math.cos(th)
                if mirror then
                    x = 1 - x
                end
                core.node_box(id, x - w, 0, z - w, x + w, 1 / 16, z + w)
            end
        end
    end
    arcs(CORNER_R, false, 0.06)
    arcs(CORNER_L, true, 0.06)
    arcs(SWITCH_ON, false, 0.06)
    for _, id in { CORNER_R, CORNER_L } do
        core.node_drop(id, RAIL)
    end
    core.node_drop(SWITCH_ON, SWITCH)
    local RAIL_UP   = node("building:rail_up", { 150, 130, 110 }, false, "upright", 0.7, "stone")
    local RAIL_DOWN = node("building:rail_down", { 150, 130, 110 }, false, "upright", 0.7, "stone")
    for _, id in { PRAIL, PRAIL_ON, SWITCH } do
        core.node_box(id, 0.12, 0, 0, 0.22, 1 / 16, 1)
        core.node_box(id, 0.78, 0, 0, 0.88, 1 / 16, 1)
        for _, z in { 0.1, 0.45, 0.8 } do
            core.node_box(id, 0.05, 0, z, 0.95, 1 / 32, z + 0.1)
        end
    end
    for i = 0, 3 do   -- up away from whoever laid it (-Z), or down
        core.node_box(RAIL_UP, 0.05, 0, 0.75 - i * 0.25, 0.95, 1 / 16 + 0.25 * i + 0.125, 1 - i * 0.25)
        core.node_box(RAIL_DOWN, 0.05, 0, 0.75 - i * 0.25, 0.95, 1 - 0.25 * i - 0.125, 1 - i * 0.25)
    end
    core.node_conveyor(RAIL, 6, 1 / 16, 0, true)
    core.node_conveyor(PRAIL, 0, 1 / 16, 0, true)
    core.node_conveyor(PRAIL_ON, 11, 1 / 16, 0, true)
    core.node_conveyor(SWITCH, 6, 1 / 16, 0, true)
    core.node_conveyor(SWITCH_ON, 6, 1 / 16, 0, true)
    core.node_conveyor(CORNER_R, 6, 1 / 16, 0, true)
    core.node_conveyor(CORNER_L, 6, 1 / 16, 0, true)
    core.node_conveyor(RAIL_UP, 6, 1 / 16, 1, true)
    core.node_conveyor(RAIL_DOWN, 6, 1 + 1 / 16, -1, true)
    core.node_drop(PRAIL_ON, PRAIL)
    -- Drawn as models (tools/make_rail_models.py, assets/blender/rails.blend):
    -- sleepers, steel rails, the curves and the embankments; the boxes above
    -- stay what is aimed at and stood on.
    for id, file in { [PRAIL] = "powered_rail", [PRAIL_ON] = "powered_rail_on", [SWITCH] = "switch",
                      [SWITCH_ON] = "switch_on", [CORNER_R] = "rail_right", [CORNER_L] = "rail_left",
                      [RAIL_UP] = "rail_up", [RAIL_DOWN] = "rail_down" } do
        core.node_model(id, core.register_model(`building:{file}`, `{file}.gltf`))
    end
    core.recipe_shaped("building:powered_rail", 6,
        "furnace:gold_ingot -            furnace:gold_ingot",
        "furnace:gold_ingot timber:stick furnace:gold_ingot",
        "furnace:gold_ingot ore:cinnabar furnace:gold_ingot")
    core.recipe_shaped("building:switch", 1, "cinnabar:lever", "building:rail")
    core.recipe_shaped("building:rail_up", 1, "building:rail", "rocks:cobblestone")
    core.recipe_shaped("building:rail_down", 1, "rocks:cobblestone", "building:rail")
    -- What a plain rail looks like is worked out from the rails round it, as
    -- the engine rides it: straight on where one lies behind along its run,
    -- else turned towards a side one -- one running into it first, then one
    -- lying along that side's axis either way. Every rail, whatever its
    -- kind, joins the line; only plain ones change shape.
    local RUN = { { 0, -1 }, { -1, 0 }, { 0, 1 }, { 1, 0 } }
    local PLAIN = { [RAIL] = true, [CORNER_R] = true, [CORNER_L] = true }
    local ANY = { [RAIL] = true, [CORNER_R] = true, [CORNER_L] = true, [PRAIL] = true, [PRAIL_ON] = true,
                  [SWITCH] = true, [SWITCH_ON] = true, [RAIL_UP] = true, [RAIL_DOWN] = true }
    local function run_at(x: number, y: number, z: number): { number }?
        if not ANY[core.get_block(x, y, z)] then
            return nil
        end
        return RUN[core.get_facing(x, y, z) % 4 + 1]
    end
    local function shape(x: number, y: number, z: number)
        if not PLAIN[core.get_block(x, y, z)] then
            return
        end
        local f = core.get_facing(x, y, z)
        local o = RUN[f % 4 + 1]
        for dy = -1, 1 do
            local r = run_at(x - o[1], y + dy, z - o[2])
            if r and r[1] * o[1] + r[2] * o[2] ~= 0 then
                core.set_block(x, y, z, RAIL)
                core.set_facing(x, y, z, f)
                return
            end
        end
        for pass = 1, 2 do
            for _, s in { 1, -1 } do
                local px, pz = s * o[2], -s * o[1]
                for dy = 0, 1 do
                    local r = run_at(x + px, y + dy, z + pz)
                    if r and (if pass == 1 then r[1] == -px and r[2] == -pz else r[1] * px + r[2] * pz ~= 0) then
                        -- In heading -p: a right turn if its right, (p.z, -p.x), is the way out.
                        local right = pz == o[1] and -px == o[2]
                        core.set_block(x, y, z, if right then CORNER_R else CORNER_L)
                        core.set_facing(x, y, z, f)
                        return
                    end
                end
            end
        end
        core.set_block(x, y, z, RAIL)
        core.set_facing(x, y, z, f)
    end
    local function reshape(x: number, y: number, z: number)
        shape(x, y, z)
        for _, d in RUN do
            for dy = -1, 1 do
                shape(x + d[1], y + dy, z + d[2])
            end
        end
    end
    -- A switch remembers the way it was laid; mods/cinnabar turns it from there.
    core.event.on("core:placed", function(ev)
        if ev.id == SWITCH then
            core.meta_set(ev.x, ev.y, ev.z, "base", tostring(core.get_facing(ev.x, ev.y, ev.z)))
        end
        if ANY[ev.id] then
            reshape(ev.x, ev.y, ev.z)
        end
    end)
    core.event.on("core:broken", function(ev)
        if ANY[ev.id] then
            reshape(ev.x, ev.y, ev.z)
        end
    end)
end

-- ===================================================================== client

if drawing then
    if not serving then
        core.event.on("core:net_message", function(ev)
            if string.sub(ev.text, 1, 5) == "sign " then
                shown_sign = string.sub(ev.text, 6)
            end
        end)
    end
    core.hud.pass("building:sign", 50, function()
        if shown_sign ~= "" and shown_sign ~= " " then
            core.hud.rect(0.5, 0.40, 0.42, 0.06, 90, 64, 36, 220)
            core.hud.text(0.5, 0.40, shown_sign, 0.03, 1, 255, 245, 220, 255)
        end
    end)
end
