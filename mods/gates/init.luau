--!strict
-- gates: a structure round every opening onto a themed world, built to look
-- like the world behind it -- a chequered arch for the track, obsidian and
-- glowrock for the Inferno, a techbase door for the arena, a ruined arch among
-- graves for the arpg lands, white panels for the test chambers, a doorway in
-- yellow wallpaper for the backrooms, an orange and black scaffold with steps
-- up its side and a bar over it for the parkour course, a dead frame of dark
-- steel with seven pedestals for the laboratory, a sandbagged bunker door
-- under a red and a blue flag for the front, a black basalt gate with
-- violet embers for the assault.
--
-- Which world an opening leads to is the room it names (core.portal_room), so
-- a sandbox started with the arena as its first opening gets the arena's gate
-- there. The first two openings stay where the server put them (the spawn
-- faces the first, mods/infernoportal frames the second); the rest are put
-- about the plaza here. ponytail: fixed spots on the plaza; scattered over the
-- whole map later, which wants a place found per seed.
--
-- Built every start, as mods/infernoportal builds its frame: only the gate's
-- own cells and a clear way through the hole are written.

local id = core.node_id
local AIR = 0
local WHITE, BLACK, GRAY = id("colors:white_concrete"), id("colors:black_concrete"), id("colors:gray_concrete")
local LIGHT_GRAY, RED, YELLOW = id("colors:light_gray_concrete"), id("colors:red_concrete"), id("colors:yellow_concrete")
local ORANGE, BLUE = id("colors:orange_concrete"), id("colors:light_blue_concrete")
local ORANGE_WOOL = id("colors:orange_wool")
local RED_TERRA, BLACK_TERRA = id("colors:red_terracotta"), id("colors:black_terracotta")
local OBSIDIAN, GLOW, LAMP = id("infernoportal:obsidian"), id("ruins:lamp"), id("spike:lamp")
local BRICKS, MOSSY, COBBLE = id("rocks:stone_bricks"), id("ruins:mossy_brick"), id("rocks:cobblestone")
local MOSSY_COBBLE, SMOOTH, DEEPSLATE = id("rocks:mossy_cobblestone"), id("rocks:smooth_stone"), id("rocks:deepslate")
local COARSE, BROWN = id("rocks:coarse_dirt"), id("colors:brown_concrete")
local GREEN, SANDBAG = id("colors:green_concrete"), id("colors:yellow_terracotta")

-- Put(u, v, w, block): u along the opening, v up from its lowest cell, w out
-- of its front. The hole is u 0, v 0 and 1, w 0.
type Put = (number, number, number, number) -> ()

-- The ring of blocks round the 1x2 hole, corners too.
local function frame(put: Put, block: (number, number) -> number)
    for v = -1, 2 do
        for u = -1, 1 do
            if u ~= 0 or v == -1 or v == 2 then put(u, v, 0, block(u, v)) end
        end
    end
end

local function chequer(u: number, v: number): number
    return if (u + v) % 2 == 0 then WHITE else BLACK
end

local THEMES: { [string]: (Put) -> () } = {
    -- The track: a start arch, red and white posts, a chequered banner and the
    -- grey of a road through it.
    pista = function(put)
        frame(put, chequer)
        for v = 0, 3 do
            put(-3, v, 0, if v % 2 == 0 then RED else WHITE)
            put(3, v, 0, if v % 2 == 0 then WHITE else RED)
        end
        for v = 4, 5 do
            for u = -3, 3 do put(u, v, 0, chequer(u, v)) end
        end
        put(-3, 6, 0, LAMP)
        put(3, 6, 0, LAMP)
        for w = -4, 4 do
            for u = -2, 2 do
                if w ~= 0 or math.abs(u) == 2 then
                    put(u, -1, w, if math.abs(u) == 2 then WHITE else GRAY)
                end
            end
        end
    end,

    -- The Inferno: the obsidian frame mods/infernoportal lights, on scorched
    -- ground, between two broken pillars crowned with glowrock.
    inferno = function(put)
        frame(put, function() return OBSIDIAN end)
        for w = -3, 3 do
            for u = -3, 3 do
                if u * u + w * w <= 10 and not (w == 0 and math.abs(u) <= 1) then
                    put(u, -1, w, if (u * 7 + w * 3) % 5 == 0 then ORANGE
                        elseif (u + w) % 3 == 0 then BLACK_TERRA else RED_TERRA)
                end
            end
        end
        for v = 0, 2 do put(-3, v, 0, OBSIDIAN) end
        put(-3, 3, 0, GLOW)
        for v = 0, 4 do put(3, v, 0, OBSIDIAN) end
        put(3, 5, 0, GLOW)
    end,

    -- The arena: a techbase door, deepslate and black with hazard stripes, a
    -- red light over it.
    arena = function(put)
        frame(put, function() return BLACK end)
        for v = -1, 4 do
            for _, u in { -2, 2 } do put(u, v, 0, if v % 2 == 0 then YELLOW else BLACK) end
            for _, u in { -3, 3 } do put(u, v, 0, DEEPSLATE) end
        end
        for u = -3, 3 do
            put(u, 3, 0, DEEPSLATE)
            put(u, 4, 0, if u == 0 then RED else DEEPSLATE)
            put(u, 5, 0, if math.abs(u) == 2 then LAMP else GRAY)
        end
        for w = -3, 3 do
            for u = -3, 3 do
                if not (w == 0 and math.abs(u) <= 3) then
                    put(u, -1, w, if math.abs(w) == 2 then (if u % 2 == 0 then YELLOW else BLACK) else GRAY)
                end
            end
        end
    end,

    -- The arpg lands: a pointed arch of mossy brick, half fallen, among graves.
    arpg = function(put)
        frame(put, function(u, v) return if (u + v) % 3 == 0 then MOSSY else BRICKS end)
        for v = -1, 3 do put(-2, v, 0, if v % 2 == 0 then BRICKS else MOSSY) end
        for v = -1, 1 do put(2, v, 0, MOSSY) end
        put(-1, 3, 0, BRICKS)
        put(0, 3, 0, COBBLE)
        put(0, 4, 0, MOSSY)
        put(-2, 4, 0, GLOW)
        put(2, 2, 0, GLOW)
        for w = -3, 3 do
            for u = -3, 3 do
                if u * u + w * w <= 12 and not (w == 0 and math.abs(u) <= 2) then
                    put(u, -1, w, if (u + 2 * w) % 3 == 0 then MOSSY_COBBLE
                        elseif (u * w) % 2 == 0 then COARSE else COBBLE)
                end
            end
        end
        for _, g in { { -3, 3 }, { 3, 2 }, { -3, -3 }, { 3, -2 } } do
            put(g[1], 0, g[2], SMOOTH)
            put(g[1], 1, g[2], SMOOTH)
        end
    end,

    -- The test chambers: a white panelled wall, the hole edged in grey, a blue
    -- stripe on one side and an orange one on the other, a light strip above.
    chambers = function(put)
        frame(put, function() return LIGHT_GRAY end)
        for v = -1, 4 do
            for _, u in { -3, -2, 2, 3 } do put(u, v, 0, WHITE) end
        end
        for v = 0, 2 do
            put(-2, v, 0, BLUE)
            put(2, v, 0, ORANGE)
        end
        for u = -1, 1 do
            put(u, 3, 0, WHITE)
            put(u, 4, 0, LAMP)
        end
        for w = -2, 2 do
            for u = -2, 2 do
                if w ~= 0 then put(u, -1, w, if (u + w) % 2 == 0 then WHITE else LIGHT_GRAY) end
            end
        end
    end,

    -- The backrooms: a doorway in a stretch of yellow wall, a tube light over
    -- it, damp brown carpet in front.
    backrooms = function(put)
        frame(put, function() return LIGHT_GRAY end)
        for v = -1, 3 do
            for _, u in { -3, -2, 2, 3 } do put(u, v, 0, YELLOW) end
        end
        for u = -1, 1 do put(u, 3, 0, YELLOW) end
        for u = -3, 3 do put(u, 4, 0, if math.abs(u) <= 1 then LAMP else YELLOW) end
        for w = -2, 2 do
            for u = -2, 2 do
                if w ~= 0 then put(u, -1, w, BROWN) end
            end
        end
    end,

    -- The parkour course: an orange and black striped scaffold, a stair of
    -- grey blocks climbing its left side, a bar over the top with an orange
    -- rope hanging from it, and grey ground in front.
    parkour = function(put)
        frame(put, function(u, v) return if (u + v) % 2 == 0 then ORANGE else BLACK end)
        for v = -1, 4 do
            put(-3, v, 0, if v % 2 == 0 then ORANGE else BLACK)
            put(3, v, 0, if v % 2 == 0 then BLACK else ORANGE)
        end
        for u = -3, 3 do put(u, 5, 0, GRAY) end
        for k = 0, 3 do
            for v = -1, k - 1 do put(-4 - (3 - k), v, 1, LIGHT_GRAY) end
        end
        for v = 2, 4 do put(2, v, 1, ORANGE_WOOL) end
        put(2, 5, 1, GRAY)
        for w = -2, 2 do
            for u = -2, 2 do
                if w ~= 0 then put(u, -1, w, GRAY) end
            end
        end
    end,

    -- The laboratory: a dead frame of dark steel, and the seven pedestals of
    -- mods/keys in front of it, four on the left of the way in, three on the
    -- right. Only in a world that has the laboratory (and so mods/keys).
    laboratorio = function(put)
        local FRAME, PEDESTAL = id("keys:frame"), id("keys:pedestal")
        frame(put, function() return FRAME end)
        for v = -1, 4 do
            put(-2, v, 0, FRAME)
            put(2, v, 0, FRAME)
        end
        for v = 3, 4 do
            for u = -2, 2 do put(u, v, 0, FRAME) end
        end
        for w = -1, 5 do
            for u = -4, 4 do
                if w ~= 0 or math.abs(u) >= 2 then put(u, -1, w, if math.abs(u) <= 1 then FRAME else DEEPSLATE) end
            end
        end
        for w = 1, 4 do put(-3, 0, w, PEDESTAL) end
        for w = 1, 3 do put(3, 0, w, PEDESTAL) end
    end,

    -- The front: a concrete bunker door in a wall of sandbags, a red flag on
    -- one side and a blue one on the other, trodden ground before it.
    front = function(put)
        frame(put, function() return LIGHT_GRAY end)
        for v = -1, 3 do
            put(-2, v, 0, GRAY)
            put(2, v, 0, GRAY)
        end
        for u = -2, 2 do put(u, 3, 0, GRAY) end
        for u = -5, 5 do
            if math.abs(u) >= 3 then
                for v = 0, if math.abs(u) == 5 then 0 else 1 do put(u, v, 0, SANDBAG) end
            end
        end
        for _, f in { { -4, RED }, { 4, id("colors:blue_concrete") } } do
            for v = 2, 6 do put(f[1], v, 0, GRAY) end
            put(f[1] + (if f[1] < 0 then -1 else 1), 6, 0, f[2])
            put(f[1] + (if f[1] < 0 then -1 else 1), 5, 0, f[2])
        end
        for w = -4, 4 do
            for u = -3, 3 do
                if w ~= 0 or math.abs(u) >= 2 then put(u, -1, w, if (u * 3 + w * 5) % 4 == 0 then GREEN else COARSE) end
            end
        end
    end,

    -- The assault: a gate of black basalt between two towers with Basoalth's
    -- violet light on them, an iron portcullis raised over the hole, scorched
    -- ground before it.
    assalto = function(put)
        local BASALT, EMBER = id("colors:black_concrete"), id("colors:purple_concrete")
        frame(put, function() return GRAY end)
        for v = -1, 5 do
            for _, u in { -3, -2, 2, 3 } do put(u, v, 0, BASALT) end
        end
        for u = -1, 1 do
            put(u, 3, 0, BASALT)
            put(u, 2, 0, GRAY)
        end
        for u = -3, 3, 2 do put(u, 6, 0, BASALT) end
        put(-3, 7, 0, EMBER)
        put(3, 7, 0, EMBER)
        for w = -4, 4 do
            for u = -3, 3 do
                if w ~= 0 or math.abs(u) >= 2 then put(u, -1, w, if (u * 5 + w * 3) % 4 == 0 then RED_TERRA else BLACK_TERRA) end
            end
        end
    end,
}

-- Where the gates past the first two stand, by room: round the spawn, which is
-- in front of the first opening (40.5 21 44.5, facing +Z).
local SPOTS: { [string]: { number } } = {
    pista = { 52.5, 32.5 }, inferno = { 60.5, 60.5 }, arena = { 28.5, 64.5 },
    arpg = { 78.5, 34.5 }, chambers = { 84.5, 84.5 }, backrooms = { 16.5, 28.5 },
    parkour = { 16.5, 52.5 }, laboratorio = { 62.5, 76.5 }, front = { 90.5, 58.5 },
    assalto = { 90.5, 76.5 },
}

-- Mouths 8 to 14 are mods/keys' arches; 15 is the front's and 16 the
-- assault's, which hold no key and so have a gate and no arch.
for _, k in { 0, 1, 2, 3, 4, 5, 6, 7, 15, 16 } do
    local room = core.portal_room(k)
    local theme = THEMES[room]
    if theme == nil then continue end
    if k >= 2 then
        local spot = SPOTS[room]
        -- Past the eighth the server has no place of its own for a mouth: the
        -- plaza's height is the first one's.
        core.set_portal(k, spot[1], core.get_portal(if k >= 8 then 0 else k, 1), spot[2])
    end
    local hx = math.floor(core.get_portal(k, 0))
    local y0 = math.floor(core.get_portal(k, 1)) - 1
    local hz = math.floor(core.get_portal(k, 2))
    -- The first opening faces +Z, every other +X (the server's defaults).
    local put: Put = if k == 0
        then function(u, v, w, b) core.set_block(hx + u, y0 + v, hz + w, b) end
        else function(u, v, w, b) core.set_block(hx + w, y0 + v, hz + u, b) end
    -- A clear way through, then the gate.
    for w = -2, 2 do
        for v = 0, 2 do
            for u = -1, 1 do
                if w ~= 0 or u == 0 then put(u, v, w, AIR) end
            end
        end
    end
    theme(put)
    core.log(`gates: opening {k} to '{room}' at {hx} {y0} {hz}`)
end
