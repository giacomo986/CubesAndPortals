--!strict
-- alchemy: a STILL, flasks and potions.
--
-- A flask (three of glass) is filled by right clicking water with it. A still
-- (copper over bricks) brews: right click it holding an ingredient, with a
-- flask of water in the bag, and both go in; it bubbles and glows for ten
-- seconds and puts the potion on top of itself. A potion is drunk as food is
-- eaten -- right click with it -- and gives the empty flask back.
--
--   ingredient          potion        three minutes of
--   deep:glowcap        night_sight   seeing in the dark (core.night_sight)
--   cane:sugar_cane     swiftness     walking 1.4 times as fast
--   deep:shard          stoneskin     half of every blow
--   plants:melon_slice  mending       health back three times as fast
--   rocks:amethyst_shard haste        digging 1.6 times as fast
--   pets:fish           water_breathing  no air lost under water (mods/breath)
--
-- Each potion is its own source of core.player_factors, so two drunk add up
-- and neither undoes a territory's. The effects are in memory: they end with
-- the server, or with the player leaving.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local LAST = 180       -- seconds a potion lasts
local BREW = 10        -- seconds a still brews

-- What each potion does: the four factors, or night sight.
type Potion = { from: string, factors: { number }?, sight: boolean?, gills: boolean?, said: string }
local POTIONS: { [string]: Potion } = {
    night_sight = { from = "deep:glowcap", sight = true, said = "Night sight" },
    swiftness   = { from = "cane:sugar_cane", factors = { 1.4, 1, 1, 1 }, said = "Swiftness" },
    stoneskin   = { from = "deep:shard", factors = { 1, 1, 0.5, 1 }, said = "Stoneskin" },
    mending     = { from = "plants:melon_slice", factors = { 1, 1, 1, 3 }, said = "Mending" },
    haste       = { from = "rocks:amethyst_shard", factors = { 1, 1.6, 1, 1 }, said = "Haste" },
    water_breathing = { from = "pets:fish", gills = true, said = "Water breathing" },
}

if serving then
    core.net.open()

    core.register_item("alchemy:flask", 16, "", 0, 1.0, "")
    core.register_item("alchemy:flask_water", 16, "", 0, 1.0, "")
    local BY_INGREDIENT: { [string]: string } = {}
    for name, p in POTIONS do
        core.register_item("alchemy:" .. name, 16, "", 0, 1.0, "")
        BY_INGREDIENT[p.from] = name
    end

    -- Two blocks, as the furnace: a brewing still glows. Bricks under a copper
    -- pot, a lid and the pipe off it.
    local STILL = core.register_node("alchemy:still", 184, 115, 75, true, 0, "upright", 0, 2.5, "stone")
    local BUSY  = core.register_node("alchemy:still_on", 210, 140, 80, true, 10, "upright", 0, 2.5, "stone")
    for _, id in { STILL, BUSY } do
        core.node_box(id, 0.05, 0, 0.05, 0.95, 0.3, 0.95)
        core.node_box(id, 0.15, 0.3, 0.15, 0.85, 0.8, 0.85)
        core.node_box(id, 0.3, 0.8, 0.3, 0.7, 0.95, 0.7)
        core.node_box(id, 0.45, 0.85, 0.7, 0.55, 0.95, 1.0)
    end
    core.node_drop(BUSY, STILL)

    core.recipe_shaped("alchemy:flask", 3,
        "core:glass -          core:glass",
        "-          core:glass -")
    core.recipe_shaped("alchemy:still", 1,
        "-                    core:glass           -",
        "furnace:copper_ingot furnace:copper_ingot furnace:copper_ingot",
        "rocks:brick          rocks:brick          rocks:brick")

    -- Whether a flask held by this player points at water within reach, walked
    -- along the look as mods/lakes walks it (a ray finds only solids).
    local function at_water(player: number): boolean
        local px, py, pz = core.player_pos(player)
        local lx, ly, lz = core.player_look(player)
        if px == nil or lx == nil then
            return false
        end
        for t = 0, 6, 0.2 do
            local id = core.get_block(math.floor(px + lx * t), math.floor(py + 1.62 + ly * t), math.floor(pz + lz * t))
            local name = core.node_name(id)
            if string.find(name, "^lakes:water") or string.find(name, "^building:cauldron") and name ~= "building:cauldron" then
                return true
            end
            if id ~= 0 and core.node_solid(id) == 1 then
                return false
            end
        end
        return false
    end

    -- ------------------------------------------------------------ effects

    type Effect = { [string]: number }   -- potion -> seconds left
    local effects: { [number]: Effect } = {}

    local function show(p: number)
        local list = {}
        for name, left in effects[p] or {} do
            table.insert(list, `{name}={math.ceil(left)}`)
        end
        local text = table.concat(list, " ")
        if drawing then
            core.event.emit("alchemy:shown", { text = text })
        else
            core.net.send(p, text)
        end
    end

    local function apply(p: number, name: string, on: boolean)
        local f = POTIONS[name].factors
        if f then
            if on then
                core.player_factors(p, f[1], f[2], f[3], f[4], "alchemy:" .. name)
            else
                core.player_factors(p, 1, 1, 1, 1, "alchemy:" .. name)
            end
        end
    end

    -- ------------------------------------------------------------- brewing

    type Brew = { x: number, y: number, z: number, left: number, potion: string }
    local brewing: { Brew } = {}

    local function finish(x: number, y: number, z: number, potion: string)
        core.set_block(x, y, z, STILL)
        core.meta_set(x, y, z, "brew", "")
        core.drop_item(x, y + 1, z, core.item_id("alchemy:" .. potion), 1)
    end

    core.event.on("core:used", -5, function(ev)
        local held = ev.held
        -- Drunk.
        local name = string.match(held, "^alchemy:(.+)$")
        if name and POTIONS[name] and ev.id ~= STILL then
            ev.cancel = true
            if core.take_item(ev.player, held, 1) == 1 then
                core.give_item(ev.player, "alchemy:flask", 1)
                local e = effects[ev.player] or {}
                effects[ev.player] = e
                e[name] = LAST
                apply(ev.player, name, true)
                if POTIONS[name].gills then
                    core.event.emit("breath:water", { player = ev.player, seconds = LAST })
                end
                core.play_sound("alchemy:drink", ev.x, ev.y, ev.z)
                core.notice(ev.player, POTIONS[name].said .. " for three minutes")
                show(ev.player)
            end
            return
        end
        -- Filled.
        if held == "alchemy:flask" and ev.id ~= STILL then
            ev.cancel = true
            if at_water(ev.player) and core.take_item(ev.player, held, 1) == 1 then
                core.give_item(ev.player, "alchemy:flask_water", 1)
            end
            return
        end
        if ev.id == BUSY then
            ev.cancel = true
            -- One left brewing when the server stopped: done.
            local potion = core.meta_get(ev.x, ev.y, ev.z, "brew")
            local going = false
            for _, b in brewing do
                going = going or (b.x == ev.x and b.y == ev.y and b.z == ev.z)
            end
            if not going and POTIONS[potion] then
                finish(ev.x, ev.y, ev.z, potion)
            end
            return
        end
        if ev.id ~= STILL then
            return
        end
        ev.cancel = true
        local potion = BY_INGREDIENT[held]
        if potion == nil then
            core.notice(ev.player, "A still brews a flask of water with something in it")
            return
        end
        if core.take_item(ev.player, "alchemy:flask_water", 1) == 0 then
            core.notice(ev.player, "It needs a flask of water")
            return
        end
        if core.take_item(ev.player, held, 1) == 0 then
            core.give_item(ev.player, "alchemy:flask_water", 1)
        else
            core.set_block(ev.x, ev.y, ev.z, BUSY)
            core.meta_set(ev.x, ev.y, ev.z, "brew", potion)
            table.insert(brewing, { x = ev.x, y = ev.y, z = ev.z, left = BREW, potion = potion })
            core.play_sound("alchemy:bubble", ev.x + 0.5, ev.y + 0.5, ev.z + 0.5)
        end
    end)

    local since = 0
    core.event.on("core:tick", function(ev)
        since += ev.dt
        if since < 1 then
            return
        end
        local dt = since
        since = 0
        for i = #brewing, 1, -1 do
            local b = brewing[i]
            b.left -= dt
            if core.get_block(b.x, b.y, b.z) ~= BUSY then
                table.remove(brewing, i)   -- dug while it brewed: lost
            elseif b.left <= 0 then
                finish(b.x, b.y, b.z, b.potion)
                table.remove(brewing, i)
            elseif math.floor(b.left) % 2 == 0 then
                core.play_sound("alchemy:bubble", b.x + 0.5, b.y + 0.5, b.z + 0.5)
            end
        end
        local here: { [number]: boolean } = {}
        for _, p in core.players() do
            here[p] = true
        end
        for p, e in effects do
            if not here[p] then
                effects[p] = nil
            else
                for name, left in e do
                    e[name] = left - dt
                    if e[name] <= 0 then
                        e[name] = nil
                        apply(p, name, false)
                        core.notice(p, POTIONS[name].said .. " wears off")
                    end
                end
                show(p)
                if next(e) == nil then
                    effects[p] = nil
                end
            end
        end
    end)
end

if drawing then
    -- What this player has drunk, name=seconds left, from the server.
    local shown: { { any } } = {}
    local function took(text: string)
        shown = {}
        for name, left in string.gmatch(text, "([%w_]+)=(%d+)") do
            table.insert(shown, { name, tonumber(left) })
        end
        table.sort(shown, function(a, b) return a[1] < b[1] end)
    end
    if serving then
        core.event.on("alchemy:shown", function(ev) took(ev.text) end)
    else
        core.event.on("core:net_message", function(ev) took(ev.text) end)
    end

    -- Night sight fades in over a second and out over the last ten.
    local sight = 0.0
    core.event.on("core:tick", function(ev)
        local want = 0.0
        for _, s in shown do
            if s[1] == "night_sight" then
                want = math.clamp(s[2] / 10, 0, 1)
            end
        end
        sight += math.clamp(want - sight, -ev.dt, ev.dt)
        core.night_sight(sight)
    end)

    core.hud.pass("alchemy:effects", 40, function()
        if not core.engine_hud_on() then
            return
        end
        for i, s in shown do
            local p = POTIONS[s[1]]
            if p then
                local left = s[2] :: number
                core.hud.text(0.985, 0.04 + (i - 1) * 0.035, `{p.said} {left // 60}:{string.format("%02d", left % 60)}`,
                              0.022, 2, 235, 230, 210, 230)
            end
        end
    end)
end
