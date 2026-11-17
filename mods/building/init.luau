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
--   signs       one per wood, on a wall, standing or hanging from a ceiling --
--               the last two facing any of eight ways, an eighth of a turn
--               apart; whoever puts one down is asked what it says, and it is
--               written on the board, in the world
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
--               whoever put it down, and a right click sits on it (the robot's
--               "sit" clip, tools/make_cart_clips.py) until they move
--   carpets     two wool of a colour make three, a sixteenth high
--   lantern     iron round a torch, hanging or standing, light 14
local WOODS = { "oak", "birch", "spruce", "jungle", "acacia", "dark_oak" }
local function planks_of(w: string): string
    return if w == "oak" then "timber:planks" else `woods:{w}_planks`
end

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

-- The signs near this machine's player, as the server tells them (the signs'
-- section, below): written on their boards by the client's render pass.
type WorldSign = { x: number, y: number, z: number, kind: string, f: number, wood: string, text: string }
local world_signs: { [string]: WorldSign } = {}
local function heard_sign(msg: string)
    local tag = string.sub(msg, 1, 2)
    if tag == "D " then
        world_signs[string.sub(msg, 3)] = nil
        return
    end
    if tag ~= "S " then return end
    local head, text = string.match(msg, "^S ([^\31]*)\31(.*)$")
    if not head or not text then return end
    local x, y, z, kind, f, wood = string.match(head, "^(-?%d+),(-?%d+),(-?%d+),(%a+),(%d+),([%w_]+)$")
    if not x then return end
    world_signs[`{x},{y},{z}`] = { x = tonumber(x) :: number, y = tonumber(y) :: number, z = tonumber(z) :: number,
                                  kind = kind :: string, f = tonumber(f) :: number, wood = wood :: string,
                                  text = text :: string }
end

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

    -- ------------------------------------------------------------ scaffold
    -- Poles and planks (tools/make_scaffold_model.py, in Blender): nothing
    -- stops a body in it and it is climbed as a ladder is, jump up and crouch
    -- to hold, so a stack of them is a tower to go up beside a wall; creatures
    -- climb it too, and a robot building a blueprint puts it up where its
    -- hand does not reach, and takes it down after (mods/helpers).
    local SCAFFOLD = node("building:scaffold", { 170, 120, 70 }, false, "", 0.4, "wood")
    core.node_climbable(SCAFFOLD)
    core.node_model(SCAFFOLD, core.register_model("building:scaffold", "scaffold.gltf"))
    core.register_model("building:scaffold_held", "scaffold_held.gltf")
    core.recipe_shaped("building:scaffold", 6,
        "timber:planks timber:planks timber:planks",
        "timber:stick  -             timber:stick",
        "timber:stick  -             timber:stick")

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
    core.register_model("building:gunpowder_held", "gunpowder_held.gltf")   -- tools/make_held_models.py
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
                            -- Nor a pocket hall's shell or a pocket door's frame (mods/pocket).
                            if not HARD[name] and not string.find(name, "^lakes:") and not string.find(name, "^pocket:") then
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
    -- Lit dust in every shape it is drawn in (mods/cinnabar).
    for a = 0, 2 do for b = 0, 2 do for c = 0, 2 do for d = 0, 2 do
        local id = core.node_id(`cinnabar:dust_on_{a}{b}{c}{d}`)
        if id ~= 0 then
            ON[id] = true
        end
    end end end end
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
        -- In the hand: their own boxes, not a cube (tools/make_held_models.py).
        core.register_model(`building:{c}_stained_glass_pane_held`, `{c}_stained_glass_pane_held.gltf`)
        core.register_model(`building:{c}_carpet_held`, `{c}_carpet_held.gltf`)
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
        core.register_model(`building:{w[1]}_wall_held`, `{w[1]}_wall_held.gltf`)
    end

    -- Chairs and tables, one of each per wood.
    local CHAIRS: { [number]: boolean } = {}
    for _, w in WOODS do
        local chair = node(`building:{w}_chair`, { 160, 130, 80 }, true, "upright", 1.0, "wood")
        boxes(chair, { { 0.15, 0.30, 0.15, 0.85, 0.40, 0.85 },                        -- seat
                       { 0.15, 0, 0.15, 0.25, 0.30, 0.25 }, { 0.75, 0, 0.15, 0.85, 0.30, 0.25 },   -- legs
                       { 0.15, 0, 0.75, 0.25, 0.30, 0.85 }, { 0.75, 0, 0.75, 0.85, 0.30, 0.85 },
                       { 0.15, 0.40, 0.75, 0.85, 1.0, 0.85 } })                         -- back
        CHAIRS[chair] = true
        core.register_model(`building:{w}_chair_held`, `{w}_chair_held.gltf`)
        core.register_model(`building:{w}_table_held`, `{w}_table_held.gltf`)
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

    -- Sat on: on its seat facing away from its back, until the body moves off it.
    local RUN = { { 0, -1 }, { -1, 0 }, { 0, 1 }, { 1, 0 } }
    local seated: { [number]: { number } } = {}
    core.event.on("core:used", function(ev)
        if not CHAIRS[ev.id] or seated[ev.player] then
            return
        end
        ev.cancel = true
        local run = RUN[core.get_facing(ev.x, ev.y, ev.z) % 4 + 1]
        core.player_place(ev.player, ev.x + 0.5, ev.y + 0.40, ev.z + 0.5, run[1], -0.1, run[2])
        core.player_clip(ev.player, "sit", run[1], run[2])   -- facing away from its back, whatever the view
        -- The fourth: a second before moving counts, while the body is put there.
        seated[ev.player] = { ev.x + 0.5, ev.y + 0.40, ev.z + 0.5, 1.0 }
    end)
    core.event.on("core:tick", function(ev)
        for p, at in seated do
            at[4] -= ev.dt
            local x, y, z = core.player_pos(p)
            if at[4] > 0 then
                continue
            end
            if x == nil or (x - at[1]) ^ 2 + (z - at[3]) ^ 2 > 0.3 * 0.3 or math.abs(y - at[2]) > 0.3 then
                seated[p] = nil
                if x ~= nil then
                    core.player_clip(p, "")
                end
            end
        end
    end)

    -- A lantern: iron round a torch, hung under a block or stood on one.
    local LANTERN = node("building:lantern", { 230, 190, 110 }, false, "", 0.6, "stone", 14)
    boxes(LANTERN, { { 0.33, 0, 0.33, 0.67, 0.5, 0.67 } })
    local HUNG = node("building:lantern_hung", { 230, 190, 110 }, false, "", 0.6, "stone", 14)
    boxes(HUNG, { { 0.33, 0.32, 0.33, 0.67, 0.82, 0.67 }, { 0.47, 0.82, 0.47, 0.53, 1, 0.53 } })
    core.node_drop(HUNG, LANTERN)
    -- Drawn as models: an iron frame round glowing glass, hung on a chain
    -- (tools/make_lantern_model.py, assets/blender/lantern.blend).
    core.node_model(LANTERN, core.register_model("building:lantern", "lantern.gltf"))
    core.node_model(HUNG, core.register_model("building:lantern_hung", "lantern_hung.gltf"))
    core.register_model("building:lantern_held", "lantern_held.gltf")
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
    -- Three blocks a wood, each drawn by its Blender model
    -- (tools/make_sign_models.py): on a wall, standing square to the cell, and
    -- standing an eighth of a turn round -- with an upright block's four
    -- quarter turns, a standing sign faces any of eight ways. What a sign says
    -- is its cell's "text"; each player's machine is told the signs near it
    -- (below) and writes them on their boards in the world (core.render.text).

    local SIGN_KIND: { [number]: string } = {}   -- block id -> "wall", "stand", "diag"
    local SIGN_WOOD: { [number]: string } = {}
    for _, w in WOODS do
        -- On a wall: a board against the face it was put on.
        local wall = node(`building:{w}_sign`, { 160, 130, 80 }, false, "wallmounted", 1.0, "wood")
        core.node_box(wall, 0.0625, 0.25, 0, 0.9375, 0.8125, 0.0625)
        core.node_model(wall, core.register_model(`building:sign_{w}_wall`, `sign_{w}_wall.gltf`))
        -- Standing: a board on a post, facing whoever put it down.
        local stand = node(`building:{w}_standing_sign`, { 160, 130, 80 }, false, "upright", 1.0, "wood")
        core.node_box(stand, 0.0625, 0.5, 0.46875, 0.9375, 1.0, 0.53125)
        core.node_box(stand, 0.46875, 0, 0.46875, 0.53125, 0.5, 0.53125)
        core.node_model(stand, core.register_model(`building:sign_{w}_stand`, `sign_{w}_stand.gltf`))
        -- And the same, an eighth of a turn round: its front towards +X+Z at
        -- rest. Its boxes are only what the crosshair finds, so they need not
        -- turn with the board: the post, and the square the board sweeps.
        local diag = node(`building:{w}_standing_sign_diag`, { 160, 130, 80 }, false, "upright", 1.0, "wood")
        core.node_box(diag, 0.1875, 0.5, 0.1875, 0.8125, 1.0, 0.8125)
        core.node_box(diag, 0.46875, 0, 0.46875, 0.53125, 0.5, 0.53125)
        core.node_model(diag, core.register_model(`building:sign_{w}_diag`, `sign_{w}_diag.gltf`))
        -- Hanging from a ceiling on two chains, square or an eighth round.
        local hang = node(`building:{w}_hanging_sign`, { 160, 130, 80 }, false, "upright", 1.0, "wood")
        core.node_box(hang, 0.0625, 0.375, 0.46875, 0.9375, 1.0, 0.53125)
        core.node_model(hang, core.register_model(`building:sign_{w}_hang`, `sign_{w}_hang.gltf`))
        local hang_diag = node(`building:{w}_hanging_sign_diag`, { 160, 130, 80 }, false, "upright", 1.0, "wood")
        core.node_box(hang_diag, 0.1875, 0.375, 0.1875, 0.8125, 1.0, 0.8125)
        core.node_model(hang_diag, core.register_model(`building:sign_{w}_hang_diag`, `sign_{w}_hang_diag.gltf`))
        core.node_drop(stand, wall)
        core.node_drop(diag, wall)
        core.node_drop(hang, wall)
        core.node_drop(hang_diag, wall)
        SIGN_KIND[wall], SIGN_KIND[stand], SIGN_KIND[diag] = "wall", "stand", "diag"
        SIGN_KIND[hang], SIGN_KIND[hang_diag] = "hang", "hangdiag"
        SIGN_WOOD[wall], SIGN_WOOD[stand], SIGN_WOOD[diag], SIGN_WOOD[hang], SIGN_WOOD[hang_diag] = w, w, w, w, w
        core.recipe_shaped(`building:{w}_sign`, 3,
            `{planks_of(w)} {planks_of(w)} {planks_of(w)}`,
            `{planks_of(w)} {planks_of(w)} {planks_of(w)}`,
            "-  timber:stick  -")
        core.event.on("core:placed", function(ev)
            if ev.id ~= wall then
                return
            end
            -- Put down on a floor: a standing one, its front to its placer, to
            -- the nearest eighth of a turn. Even eighths are the square sign at
            -- a quarter turn (0 +Z, 1 +X, 2 -Z, 3 -X), odd ones the turned one.
            -- Under a ceiling: a hanging one, the same way.
            local f = core.get_facing(ev.x, ev.y, ev.z)
            if f == 2 or f == 3 then
                local lx, _, lz = core.player_look(ev.player)
                local eighth = 0
                if lx ~= nil and lz ~= nil and (lx ~= 0 or lz ~= 0) then
                    eighth = math.floor(math.atan2(-lx, -lz) / (math.pi / 4) + 0.5) % 8
                end
                local square, turned = if f == 2 then stand else hang, if f == 2 then diag else hang_diag
                core.set_block(ev.x, ev.y, ev.z, if eighth % 2 == 0 then square else turned)
                core.set_facing(ev.x, ev.y, ev.z, eighth // 2)
            end
        end)
    end
    local SIGNS: { [number]: boolean } = {}
    for id in SIGN_KIND do SIGNS[id] = true end

    -- Where the signs are: found as their chunks load, and as they are put down.
    local signs: { [string]: { number } } = {}
    local function sign_key(x: number, y: number, z: number): string return `{x},{y},{z}` end
    core.register_lbm("building:signs", (function()
        local names = {}
        for _, w in WOODS do
            table.insert(names, `building:{w}_sign`)
            table.insert(names, `building:{w}_standing_sign`)
            table.insert(names, `building:{w}_standing_sign_diag`)
            table.insert(names, `building:{w}_hanging_sign`)
            table.insert(names, `building:{w}_hanging_sign_diag`)
        end
        return names
    end)(), function(cell)
        signs[sign_key(cell.x, cell.y, cell.z)] = { cell.x, cell.y, cell.z }
    end)

    -- A player's words, in lines a board holds: at most four of about
    -- sixteen letters, broken between words. Lines written with "\n" (a mod's
    -- signs) are kept as they are.
    local LINE, LINES = 16, 4
    local function wrap(text: string): string
        if string.find(text, "\n", 1, true) then return text end
        local lines, cur = {}, ""
        for word in string.gmatch(text, "%S+") do
            if cur == "" then
                cur = word
            elseif #cur + 1 + #word <= LINE then
                cur ..= " " .. word
            else
                table.insert(lines, cur)
                cur = word
            end
        end
        if cur ~= "" then table.insert(lines, cur) end
        while #lines > LINES do table.remove(lines) end
        return table.concat(lines, "\n")
    end

    local editing: { [number]: { number } } = {}
    core.ui_define("building:sign", 10, 3)
    core.ui_label("building:sign", 0.3, 0.2, "What does the sign say? (Enter)")
    core.ui_field("building:sign", "text", 0.3, 1.2, 9.4, 1.2, "")
    core.event.on("core:placed", function(ev)
        if SIGNS[ev.id] then
            signs[sign_key(ev.x, ev.y, ev.z)] = { ev.x, ev.y, ev.z }
            editing[ev.player] = { ev.x, ev.y, ev.z }
            core.ui_show("building:sign", true, ev.player)
        end
    end)
    core.event.on("core:ui_click", function(ev)
        if ev.panel == "building:sign" and ev.id == "text" then
            local at = editing[ev.player]
            if at ~= nil then
                core.meta_set(at[1], at[2], at[3], "text", wrap(string.sub(ev.text or "", 1, 90)))
                editing[ev.player] = nil
            end
            core.ui_show("building:sign", false, ev.player)
        end
    end)

    -- Each player is told the signs within NEAR of them, one message a sign
    -- ("S x,y,z,kind,facing,wood" and its text after a \31), when it is new to
    -- them or has changed, and "D x,y,z" when it is gone or they went away.
    -- A message holds 512 bytes and a sign's text 256: one a sign.
    local NEAR, A_TURN = 40, 32
    local told: { [number]: { [string]: string } } = {}
    local tell_clock, look_clock = 0, 4
    -- And now and then, the signs round each player found by looking: a mod
    -- that put one down with core.set_block (the pipes' demo) fired no
    -- core:placed, and its chunk may have loaded before.
    local SIGN_NAMES = {}
    for id, _ in SIGN_KIND do table.insert(SIGN_NAMES, core.node_name(id)) end
    local function look_round()
        for _, p in core.players() do
            local px, py, pz = core.player_pos(p)
            if px and py and pz then
                local x, y, z = math.floor(px), math.floor(py), math.floor(pz)
                for _, name in SIGN_NAMES do
                    local found = core.find_nodes(x - 32, y - 10, z - 32, x + 32, y + 10, z + 32, name)
                    for i = 1, #found, 3 do
                        signs[sign_key(found[i], found[i + 1], found[i + 2])] = { found[i], found[i + 1], found[i + 2] }
                    end
                end
            end
        end
    end
    core.event.on("core:tick", function(ev)
        tell_clock += ev.dt
        look_clock += ev.dt
        if tell_clock < 0.5 then
            return
        end
        tell_clock = 0
        if look_clock >= 4 then
            look_clock = 0
            look_round()
        end
        for k, s in signs do
            if not SIGNS[core.get_block(s[1], s[2], s[3])] then signs[k] = nil end
        end
        local here: { [number]: boolean } = {}
        for _, p in core.players() do
            here[p] = true
            local mine = told[p] or {}
            told[p] = mine
            local px, py, pz = core.player_pos(p)
            local sent = 0
            local function say(text: string)
                if drawing then heard_sign(text) else core.net.send(p, text) end
                sent += 1
            end
            if px and py and pz then
                for k, s in signs do
                    if sent >= A_TURN then break end
                    if (s[1] - px) ^ 2 + (s[2] - py) ^ 2 + (s[3] - pz) ^ 2 < NEAR * NEAR then
                        local id = core.get_block(s[1], s[2], s[3])
                        local what = `S {k},{SIGN_KIND[id]},{core.get_facing(s[1], s[2], s[3])},{SIGN_WOOD[id]}\31`
                            .. core.meta_get(s[1], s[2], s[3], "text")
                        if mine[k] ~= what then
                            mine[k] = what
                            say(what)
                        end
                    end
                end
            end
            for k in mine do
                if sent >= A_TURN then break end
                local s = signs[k]
                if not s or not px or not py or not pz
                    or (s[1] - px) ^ 2 + (s[2] - py) ^ 2 + (s[3] - pz) ^ 2 >= (NEAR + 4) ^ 2 then
                    mine[k] = nil
                    say(`D {k}`)
                end
            end
        end
        for p in told do
            if not here[p] then told[p] = nil end
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
    -- No recipe for the slopes: a rail lays itself as one (below), and they
    -- are no item to hold (the recipes named one nobody had registered).
    -- What a plain rail looks like is worked out from the rails round it, as
    -- the engine rides it: a slope where the rail ahead along its run is a
    -- block higher (up) or the one behind is (down) and none is on its own
    -- level there; else straight on where one lies behind along its run,
    -- else turned towards a side one -- one running into it first, then one
    -- lying along that side's axis either way. Every rail, whatever its
    -- kind, joins the line; only plain ones and slopes change shape. So a line
    -- laid up or down a step of the ground climbs it by itself; a slope broken
    -- gives a plain rail back.
    local RUN = { { 0, -1 }, { -1, 0 }, { 0, 1 }, { 1, 0 } }
    local PLAIN = { [RAIL] = true, [CORNER_R] = true, [CORNER_L] = true, [RAIL_UP] = true, [RAIL_DOWN] = true }
    core.node_drop(RAIL_UP, RAIL)
    core.node_drop(RAIL_DOWN, RAIL)
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
        local function step(sx: number, sz: number): boolean   -- a rail a block up that way, none level
            return run_at(sx, y + 1, sz) ~= nil and run_at(sx, y, sz) == nil
        end
        local up, down = step(x + o[1], z + o[2]), step(x - o[1], z - o[2])
        if up ~= down then
            core.set_block(x, y, z, if up then RAIL_UP else RAIL_DOWN)
            core.set_facing(x, y, z, f)
            return
        end
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
    -- A rail a mod laid (core.set_block fires no core:placed): shaped as a
    -- hand's would be when the mod says so, the cells round it too.
    --   core.event.emit("building:rail_laid", { x = x, y = y, z = z })
    core.event.define("building:rail_laid", { fields = { x = "number", y = "number", z = "number" },
                                              cancellable = false })
    -- Kept until its cell can be read (a world's cells read as air while it
    -- is being made, at core:world_ready), a few seconds at most.
    local laid: { { number } } = {}
    core.event.on("building:rail_laid", function(ev)
        table.insert(laid, { ev.x, ev.y, ev.z, 0 })
    end)
    core.event.on("core:tick", function(_)
        for i = #laid, 1, -1 do
            local r = laid[i]
            r[4] += 1
            if ANY[core.get_block(r[1], r[2], r[3])] then
                reshape(r[1], r[2], r[3])
                table.remove(laid, i)
            elseif r[4] > 200 then
                table.remove(laid, i)
            end
        end
    end)
end

-- ===================================================================== client

if drawing then
    if not serving then
        core.event.on("core:net_message", function(ev)
            heard_sign(ev.text)
        end)
    end
    -- Every sign near, its words on its board: in front of the board's face by
    -- a hair more than its rim stands out (tools/make_sign_models.py), inside
    -- the rim, dark ink on a light wood and light on a dark one.
    local FRONT = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }   -- an upright block's front: +Z, +X, -Z, -X
    local NORMAL = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }
    local OUT = 0.12 / 16 + 0.003            -- the rim's height and a hair: where the words are, off the face
    local DARK = { spruce = true, dark_oak = true }
    local SHOWN = 32
    core.render.pass("building:signs", 60, function()
        local me = core.players()[1]
        if me == nil then return end
        local px, py, pz = core.player_pos(me)
        if px == nil or py == nil or pz == nil then return end
        for _, s in world_signs do
            if s.text ~= "" and (s.x + 0.5 - px) ^ 2 + (s.y + 0.5 - py) ^ 2 + (s.z + 0.5 - pz) ^ 2 < SHOWN * SHOWN then
                local fx, fz, cx, cy, cz, mh = 0, 0, 0, 0, 0, 0
                if s.kind == "wall" then
                    -- Its front out of the face it was put on (a wallmounted
                    -- block's facing), its back against the wall behind.
                    local n = NORMAL[s.f + 1]
                    if n and n[2] == 0 then
                        fx, fz = n[1], n[3]
                        local face = 0.5 - 1 / 16 - OUT
                        cx, cy, cz = s.x + 0.5 - fx * face, s.y + 8.5 / 16, s.z + 0.5 - fz * face
                        mh = 0.42
                    end
                else
                    -- Standing or hanging, square to the cell or an eighth of a turn round.
                    local turned = s.kind == "diag" or s.kind == "hangdiag"
                    local a = (if turned then math.pi / 4 else 0) + s.f * math.pi / 2
                    fx, fz = math.sin(a), math.cos(a)
                    if not turned then
                        local fr = FRONT[s.f % 4 + 1]
                        fx, fz = fr[1], fr[2]
                    end
                    local face = 0.5 / 16 + OUT
                    local up = if s.kind == "hang" or s.kind == "hangdiag" then 10 / 16 else 0.75
                    cx, cy, cz = s.x + 0.5 + fx * face, s.y + up, s.z + 0.5 + fz * face
                    mh = 0.36
                end
                -- Only from in front: its back is a plain board.
                if (fx ~= 0 or fz ~= 0) and (px - cx) * fx + (pz - cz) * fz > 0 then
                    local ink = if DARK[s.wood] then { 240, 230, 206 } else { 22, 14, 6 }
                    core.render.text(s.text, cx, cy, cz, math.atan2(-fx, fz), 0.1, ink[1], ink[2], ink[3], 255,
                                     0.72, mh)
                end
            end
        end
    end)
end
