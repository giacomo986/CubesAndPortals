--!strict
-- enchant: The classic block game's experience and enchanting table, cut down.
--
-- Experience comes from ore dug (coal 1, cinnabar 2, lapis 3, diamond and
-- emerald 5), from creatures killed (3) and from smelting, and fills levels as the classic block game's
-- does: 7 points for the first, two more for each after. A green bar with the
-- level over it sits above the hunger row.
--
-- The table -- a book over two diamonds and an obsidian, on three obsidian --
-- right clicked offers I, II or III, each with a clue of what it gives. Level
-- L costs L lapis and L levels of experience and draws, for what is in the hand (core.enchant_held), one kind
-- that fits it at level L (or one less, three times in ten) and, one time in
-- five a level, a second one: Efficiency (a tool digs 1 + 0.5 a level faster)
-- on picks, shovels and axes, Sharpness (a point harder a level) on swords,
-- Protection (a point of armour a level, worn) on armour, Unbreaking (wears
-- one use in level + 1) on anything that is one thing. A thing keeps every
-- kind it has; a draw never lowers one. Smelting's experience comes from
-- mods/furnace, as the event furnace:xp.
--
-- The server keeps the points, by name across a restart (the mod's storage),
-- and tells the player's machine, which draws the bar.
local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local function needed(level: number): number
    if level < 16 then
        return 2 * level + 7
    elseif level < 31 then
        return 5 * level - 38
    end
    return 9 * level - 158
end

local function level_of(points: number): (number, number)
    local level = 0
    while points >= needed(level) do
        points -= needed(level)
        level += 1
    end
    return level, points / needed(level)
end

local shown = 0   -- the points the bar on this machine shows

if serving then
    core.net.open()

    local TABLE = core.register_node("enchant:table", 60, 30, 50, true, 7, "", 1, 5.0, "stone")
    core.recipe_shaped("enchant:table", 1,
        "-                      cane:book              -",
        "ore:diamond            infernoportal:obsidian  ore:diamond",
        "infernoportal:obsidian  infernoportal:obsidian  infernoportal:obsidian")

    -- Enchanted tools were items of their own until a stack could carry an
    -- enchantment; a bag saved then gets the plain tool back.
    for _, t in { "pick", "shovel", "axe", "sword", "iron_pick" } do
        local plain = if t == "iron_pick" then "furnace:iron_pick" else `tools:{t}`
        core.item_alias(`enchant:{t}`, plain)
    end

    local points: { [number]: number } = {}

    local function key(p: number): string
        return "xp:" .. core.player_name(p)
    end
    local function tell(p: number)
        core.shared_storage_set(key(p), tostring(points[p]))
        if drawing then
            shown = points[p]
        else
            core.net.send(p, tostring(points[p]))
        end
    end
    local function xp(p: number): number
        if points[p] == nil then
            points[p] = tonumber(core.shared_storage_get(key(p))) or 0
            tell(p)
        end
        return points[p]
    end
    local function gain(p: number, n: number)
        points[p] = xp(p) + n
        tell(p)
    end

    -- Read again on every arrival: the points are kept once for every room
    -- (core.shared_storage_*), and what this room remembers of somebody who
    -- went to the Inferno and back is what they had when they left.
    local seen: { [number]: boolean } = {}
    core.event.on("core:tick", function(_)
        local here: { [number]: boolean } = {}
        for _, p in core.players() do
            here[p] = true
            if not seen[p] then
                seen[p] = true
                xp(p)
            end
        end
        for p in seen do
            if not here[p] then
                seen[p], points[p] = nil, nil
            end
        end
    end)

    local ORE_XP: { [string]: number } = {
        ["ore:coal_ore"] = 1, ["ore:cinnabar_ore"] = 2, ["ore:lapis_ore"] = 3,
        ["ore:diamond_ore"] = 5, ["ore:emerald_ore"] = 5,
    }
    core.event.on("core:broken", function(ev)
        local n = ORE_XP[core.node_name(ev.id)]
        if n and ev.player > 0 then
            gain(ev.player, n)
        end
    end)
    core.event.on("core:hit", function(ev)
        if ev.killed then
            gain(ev.player, 3)
        end
    end)

    -- The table: three prices, and what they buy drawn by lot -- a lot drawn
    -- ahead, from a seed of the player's own, so the panel can show a clue of
    -- it (the main kind and level, as the classic block game's does) and the clue is what
    -- they get. The seed moves on only when something is enchanted.
    local KINDS = { "Efficiency", "Sharpness", "Unbreaking", "Protection" }
    local ROMAN = { "I", "II", "III" }

    -- Which kinds go on a thing, by its name: Efficiency on what digs,
    -- Sharpness on a sword, Protection on armour, Unbreaking on anything.
    local function kinds_for(item: string): { number }
        local out = { 3 }
        if string.find(item, "pick") or string.find(item, "shovel") or string.find(item, "axe") then
            table.insert(out, 1)
        end
        if string.find(item, "sword") then
            table.insert(out, 2)
        end
        if string.sub(item, 1, 6) == "armor:" then
            table.insert(out, 4)
        end
        return out
    end

    local function level_in(byte: number, kind: number): number
        return (byte // (4 ^ (kind - 1))) % 4
    end

    -- A small generator of our own (xorshift), so a draw is the same draw on
    -- the panel and on the click.
    local function rng(seed: number): () -> number
        local x = seed % 4294967296
        if x == 0 then
            x = 2463534242
        end
        local function step(): number
            x = bit32.bxor(x, bit32.lshift(x, 13))
            x = bit32.bxor(x, bit32.rshift(x, 17))
            x = bit32.bxor(x, bit32.lshift(x, 5))
            return x / 4294967296
        end
        -- Seeds a level apart are close together, and xorshift's first few
        -- numbers from close seeds are close too: every clue came out the same.
        for _ = 1, 8 do
            step()
        end
        return step
    end

    local function seed_of(p: number): number
        local kept = tonumber(core.shared_storage_get("seed:" .. core.player_name(p)))
        if kept then
            return kept
        end
        local fresh = math.random(1, 2147483646)
        core.shared_storage_set("seed:" .. core.player_name(p), tostring(fresh))
        return fresh
    end

    -- What level L gives this thing: one kind that fits at L (or one less, 3
    -- times in 10) and, 2 in 10 a level, a second at a random level.
    local function draw(seed: number, level: number, item: string): ({ [number]: number }, number)
        local r = rng(seed * 7 + level * 1013 + #item * 31)
        local fits = kinds_for(item)
        local picks: { [number]: number } = {}
        local main = fits[math.floor(r() * #fits) + 1]
        picks[main] = if r() < 0.7 then level else math.max(1, level - 1)
        if #fits > 1 and r() < 0.2 * level then
            local other = fits[math.floor(r() * #fits) + 1]
            if other ~= main then
                picks[other] = math.floor(r() * level) + 1
            end
        end
        return picks, main
    end

    local function panel_of(p: number): string
        return `enchant:choose{p}`
    end

    -- Built for the one who opened it, with what their hand would get.
    local function show(p: number)
        local name = panel_of(p)
        local item = core.held_item(p)
        local seed = seed_of(p)
        core.ui_define(name, 9.0, 5.8)
        core.ui_label(name, 0.4, 0.3, "Level L costs L lapis and L levels")
        core.ui_label(name, 0.4, 0.9, if item == "" then "Hold what you want enchanted"
                                      else "A clue of what each gives:")
        for level = 1, 3 do
            local text = `Enchant {ROMAN[level]}`
            if item ~= "" then
                local picks, main = draw(seed, level, item)
                text = `{ROMAN[level]}: {KINDS[main]} {ROMAN[picks[main]]}?`
            end
            core.ui_button(name, tostring(level), 0.4, 1.6 + (level - 1) * 1.1, 8.2, 0.9, text)
        end
        core.ui_button(name, "close", 0.4, 4.7, 8.2, 0.9, "close")
        core.ui_show(name, true, p)
    end

    core.event.on("core:used", function(ev)
        if ev.id ~= TABLE then
            return
        end
        ev.cancel = true
        show(ev.player)
    end)

    core.event.on("core:ui_click", function(ev)
        local p = ev.player
        if ev.panel ~= panel_of(p) then
            return
        end
        if ev.id == "close" then
            core.ui_show(ev.panel, false, p)
            return
        end
        local l = tonumber(ev.id) :: number
        local item = core.held_item(p)
        local now = core.held_enchant(p)
        if item == "" or now < 0 then
            core.notice(p, "Hold what you want enchanted")
            return
        end
        local have = level_of(xp(p))
        if have < l then
            core.notice(p, `It takes {l} level{if l > 1 then "s" else ""} of experience`)
            return
        end

        -- Nothing goes down: a kind already higher stays as it is.
        local picks = draw(seed_of(p), l, item)
        local gained = {}
        for kind, lv in picks do
            if lv > level_in(now, kind) then
                table.insert(gained, { kind, lv })
            end
        end
        if #gained == 0 then
            core.notice(p, "That gives nothing it does not have; nothing spent")
            return
        end

        local lapis = core.take_item(p, "ore:lapis", l)
        if lapis < l then
            core.give_item(p, "ore:lapis", lapis)
            core.notice(p, `It takes {l} lapis`)
            return
        end
        local said = {}
        for _, g in gained do
            if core.enchant_held(p, g[2], g[1]) < 0 then
                core.give_item(p, "ore:lapis", lapis)
                core.notice(p, "Only one thing at a time can be enchanted")
                return
            end
            table.insert(said, `{KINDS[g[1]]} {ROMAN[g[2]]}`)
        end
        -- Spend the levels from the top: what is left is that many fewer.
        local left, total = have - l, 0
        for n = 0, left - 1 do
            total += needed(n)
        end
        points[p] = total
        tell(p)
        -- And the next draws are new ones.
        core.shared_storage_set("seed:" .. core.player_name(p), tostring(math.floor(rng(seed_of(p))() * 2147483646) + 1))
        core.ui_show(ev.panel, false, p)
        core.notice(p, table.concat(said, ", "))
    end)

    -- Smelting's experience, which mods/furnace keeps in the furnace until
    -- somebody opens it (The classic block game gives it on taking the result out).
    core.event.on("furnace:xp", function(ev)
        gain(ev.player, ev.points)
    end)
end

if drawing then
    core.event.on("core:net_message", function(ev)
        shown = tonumber(ev.text) or shown
    end)

    -- The engine's measures (client/ui/hud.cpp), as mods/hunger reads them:
    -- one bar above the hunger row, the width of the hotbar.
    local SLOT, PAD = 0.075, 0.0075
    local WIDE = 9 * SLOT + 8 * PAD
    local TALL = SLOT * 0.22
    local HEALTH_TOP = 1 - SLOT - 0.02 - PAD * 2 - TALL
    local Y = HEALTH_TOP - 3 * (TALL * 2) + TALL * 0.5
    core.hud.pass("enchant:xp", 50, function()
        local aspect = core.hud.aspect()
        -- Beside the engine's bars, and hidden with them (a chess game).
        if aspect <= 0 or not core.engine_hud_on() then
            return
        end
        local level, part = level_of(shown)
        local w = WIDE / aspect   -- in widths
        core.hud.rect(0.5, Y, WIDE + PAD * 2, TALL * 0.6 + PAD, 20, 20, 24, 150)
        core.hud.rect(0.5, Y, WIDE, TALL * 0.6, 30, 50, 30, 255)
        if part > 0 then
            local fill = WIDE * part
            core.hud.rect(0.5 - w / 2 + (fill / aspect) / 2, Y, fill, TALL * 0.6, 110, 220, 70, 255)
        end
        if level > 0 then
            core.hud.text(0.5, Y - TALL * 0.9, tostring(level), 0.028, 1, 140, 255, 90, 255)
        end
    end)
end
