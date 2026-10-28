--!strict
-- Maps: a COMPASS (four iron round a cinnabar) and a MAP (eight paper round a
-- compass). Held, the compass shows a dial at the top of the screen whose
-- needle points home -- the bed a player last slept in (mods/bed's bed:home),
-- or the world's spawn -- with how far it is. Held, the map shows the land
-- round its holder, north up, 512 nodes across: every pixel eight nodes, and
-- only where that player has been (48 nodes round wherever they went); the
-- rest is blank parchment. It carries the compass's dial too.
--
-- The server keeps what each player has seen -- a TILE of 16 x 16 pixels, a
-- character a pixel: 0 unseen, else its colour on the palette below, worked out
-- from the world's own generator (core.biome, core.terrain_height,
-- core.water_level) as it is seen, so the map shows the land as it was made and
-- not what was built on it since. A core.storage key a tile, and pages of the
-- tiles' names per player; their machine is sent a tile a message.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

core.register_item("maps:compass", 1)
core.register_item("maps:map", 1)
core.register_model("maps:compass_held", "compass_held.gltf")   -- tools/make_held_models.py
core.register_model("maps:map_held", "map_held.gltf")
core.recipe_shaped("maps:compass", 1,
    "-                   furnace:iron_ingot  -",
    "furnace:iron_ingot  ore:cinnabar        furnace:iron_ingot",
    "-                   furnace:iron_ingot  -")
core.recipe_shaped("maps:map", 1,
    "cane:paper  cane:paper     cane:paper",
    "cane:paper  maps:compass   cane:paper",
    "cane:paper  cane:paper     cane:paper")

local PIXEL = 8      -- nodes a pixel of the map
local SEEN = 6       -- pixels round a player that they have seen
local TILE = 16      -- pixels a tile's side
local PAGE = 300     -- tile names a page of the index, under a value's 4096 bytes
local ALPHA = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

-- The palette: 0 unseen; 1..3 water, deep to shallow; then three heights of
-- each climate, low to high.
local CLIMATES = { "core:plains", "core:desert", "core:tundra", "core:peaks", "core:gorge", "core:beach" }
local PALETTE: { { number } } = { { 40, 70, 160 }, { 60, 100, 190 }, { 90, 140, 210 } }
for _, rgb in { { 98, 156, 66 }, { 214, 196, 130 }, { 220, 228, 230 }, { 150, 150, 156 }, { 150, 110, 80 },
                { 226, 210, 150 }, { 110, 150, 80 } } do
    for band = 0, 2 do
        local k = 0.82 + band * 0.14
        table.insert(PALETTE, { math.min(255, rgb[1] * k), math.min(255, rgb[2] * k), math.min(255, rgb[3] * k) })
    end
end

-- ------------------------------------------------------------ the machine

local seen: { [number]: number } = {}   -- this player's pixels, by pk: their colour
local home_x, home_z = 0, 0
local function pk(px: number, pz: number): number
    return (px + 32768) * 65536 + (pz + 32768)
end

local function heard(text: string)
    local kind, rest = string.match(text, "^(%a) ?(.*)$")
    if kind == "t" then   -- a tile: "tx,tz:" and a character a pixel
        local tx, tz, code = string.match(rest or "", "^(-?%d+),(-?%d+):(%S+)$")
        if tx then
            local x0, z0 = (tonumber(tx) :: number) * TILE, (tonumber(tz) :: number) * TILE
            for i = 0, math.min(#code, TILE * TILE) - 1 do
                local c = (string.find(ALPHA, string.sub(code, i + 1, i + 1), 1, true) or 1) - 1
                if c > 0 then
                    seen[pk(x0 + i % TILE, z0 + i // TILE)] = c
                end
            end
        end
    elseif kind == "h" then
        local x, z = string.match(rest or "", "^(-?[%d.]+) (-?[%d.]+)$")
        home_x, home_z = tonumber(x) or 0, tonumber(z) or 0
    end
end

-- ------------------------------------------------------------ the server

if serving then
    core.net.open()

    local function send(p: number, text: string)
        if drawing then
            heard(text)
        else
            core.net.send(p, text)
        end
    end

    type Seen = { tiles: { [string]: { number } }, names: { string }, dirty: { [string]: boolean },
                  name: string }

    -- A pixel's colour on the palette, from the generator.
    local function paint(px: number, pz: number): number
        local x, z = px * PIXEL + PIXEL // 2, pz * PIXEL + PIXEL // 2
        local h, w = core.terrain_height(x, z), core.water_level(x, z)
        if w > h then
            return if w - h >= 8 then 1 elseif w - h >= 3 then 2 else 3
        end
        local b = table.find(CLIMATES, core.biome(x, z)) or #CLIMATES + 1
        local band = if h < 24 then 0 elseif h < 40 then 1 else 2
        return 4 + (b - 1) * 3 + band
    end
    local function encode(codes: { number }): string
        local out = table.create(TILE * TILE)
        for i = 1, TILE * TILE do
            local c = codes[i] or 0
            out[i] = string.sub(ALPHA, c + 1, c + 1)
        end
        return table.concat(out)
    end
    local function decode(text: string): { number }
        local codes = table.create(TILE * TILE, 0)
        for i = 1, math.min(#text, TILE * TILE) do
            codes[i] = (string.find(ALPHA, string.sub(text, i, i), 1, true) or 1) - 1
        end
        return codes
    end
    local players: { [number]: Seen } = {}
    local homes: { [number]: { number } } = {}

    local function home_of(p: number): string
        local h = homes[p]
        local sx, _, sz = core.world_spawn()
        return `h {if h then h[1] else sx} {if h then h[2] else sz}`
    end

    core.event.on("bed:home", function(ev)
        homes[ev.player] = if ev.on then { ev.x + 0.5, ev.z + 0.5 } else nil
        send(ev.player, home_of(ev.player))
    end)

    local function save(s: Seen)
        local index = false
        for t in s.dirty do
            core.storage_set(`t:{s.name}:{t}`, encode(s.tiles[t]))
            index = true
        end
        s.dirty = {}
        if index then
            for page = 1, math.max(1, math.ceil(#s.names / PAGE)) do
                core.storage_set(`tiles:{s.name}:{page}`,
                    table.concat(s.names, ";", (page - 1) * PAGE + 1, math.min(#s.names, page * PAGE)))
            end
        end
    end

    local function tell(p: number, s: Seen, names: { string })
        for _, t in names do
            send(p, `t {t}:{encode(s.tiles[t])}`)
        end
    end

    local function join(p: number): Seen
        local s: Seen = { tiles = {}, names = {}, dirty = {}, name = core.player_name(p) }
        local page = 1
        while true do
            local list = core.storage_get(`tiles:{s.name}:{page}`) or ""
            if list == "" then
                break
            end
            for t in string.gmatch(list, "[^;]+") do
                if not s.tiles[t] then
                    s.tiles[t] = decode(core.storage_get(`t:{s.name}:{t}`) or "")
                    table.insert(s.names, t)
                end
            end
            page += 1
        end
        tell(p, s, s.names)
        send(p, home_of(p))
        return s
    end

    local since, saved = 0, 0
    core.event.on("core:tick", function(ev)
        since += ev.dt
        saved += ev.dt
        if since < 1 then
            return
        end
        since = 0
        local here: { [number]: boolean } = {}
        for _, p in core.players() do
            here[p] = true
            local s = players[p] or join(p)
            players[p] = s
            local x, _, z = core.player_pos(p)
            if x then
                local cx, cz = math.floor(x / PIXEL), math.floor(z / PIXEL)
                local changed: { [string]: boolean } = {}
                for dz = -SEEN, SEEN do
                    for dx = -SEEN, SEEN do
                        if dx * dx + dz * dz <= SEEN * SEEN then
                            local px, pz = cx + dx, cz + dz
                            local tx, tz = px // TILE, pz // TILE
                            local t = `{tx},{tz}`
                            local codes = s.tiles[t]
                            if codes == nil then
                                codes = table.create(TILE * TILE, 0)
                                s.tiles[t] = codes
                                table.insert(s.names, t)
                            end
                            local i = (pz - tz * TILE) * TILE + (px - tx * TILE) + 1
                            if codes[i] == 0 then
                                codes[i] = paint(px, pz)
                                changed[t] = true
                                s.dirty[t] = true
                            end
                        end
                    end
                end
                local names = {}
                for t in changed do
                    table.insert(names, t)
                end
                tell(p, s, names)
            end
        end
        for p, s in players do
            if not here[p] then   -- left: kept, and let go
                save(s)
                players[p] = nil
                homes[p] = nil
            end
        end
        if saved >= 10 then
            saved = 0
            for _, s in players do
                save(s)
            end
        end
    end)
end

-- ------------------------------------------------------------ the machine draws

if drawing then
    core.event.on("core:net_message", function(ev)
        heard(ev.text)
    end)

    local SIDE = 64      -- pixels across the map
    local SIZE = 0.62    -- of the screen's height
    local PAPER = { 222, 205, 160 }

    -- Home's way on a dial at (cx, cy), radius rad (in screen heights): a ring,
    -- N at the top of the ring turning with the view, the needle home, and how far.
    local function dial(cx: number, cy: number, rad: number, aspect: number, x: number, z: number)
        local lx, _, lz = core.player_look(1)
        local look = math.atan2(lx or 0, -(lz or -1))
        local function at(angle: number, r: number): (number, number)
            return cx + math.sin(angle) * r / aspect, cy - math.cos(angle) * r
        end
        core.hud.rect(cx, cy, rad * 2.3, rad * 2.3, 30, 26, 20, 170)
        for i = 0, 23 do
            local ax, ay = at(i / 24 * math.pi * 2, rad)
            core.hud.rect(ax, ay, rad * 0.12, rad * 0.12, 200, 190, 160, 230)
        end
        local nx, ny = at(-look, rad * 0.78)
        core.hud.text(nx, ny, "N", rad * 0.5, 1, 240, 230, 200, 255)
        local dx, dz = home_x - x, home_z - z
        local away = math.sqrt(dx * dx + dz * dz)
        local rel = math.atan2(dx, -dz) - look
        for i = 1, 6 do
            local sx, sy = at(rel, rad * 0.11 * i)
            core.hud.rect(sx, sy, rad * 0.14, rad * 0.14, 220, 40, 30, 255)
        end
        for i = 1, 3 do
            local sx, sy = at(rel + math.pi, rad * 0.11 * i)
            core.hud.rect(sx, sy, rad * 0.12, rad * 0.12, 230, 230, 235, 255)
        end
        core.hud.text(cx, cy + rad * 1.45, if away < 3 then "home" else `home {math.floor(away)}`,
            rad * 0.45, 1, 240, 230, 200, 255)
    end

    core.hud.pass("maps:map", 90, function()
        local held = core.held_item(1)
        if held ~= "maps:map" and held ~= "maps:compass" then
            return
        end
        local x, _, z = core.player_pos(1)
        if x == nil then
            return
        end
        local aspect = core.hud.aspect()
        if aspect <= 0 then
            return
        end
        if held == "maps:compass" then
            dial(0.5, 0.12, 0.06, aspect, x, z)
            return
        end

        -- The parchment, then the pixels seen, a row at a time and a run of
        -- one colour at a time; the holder in the middle, home where it is.
        local cell = SIZE / SIDE
        local left, top = 0.5 - SIZE / 2 / aspect, 0.5 - SIZE / 2
        core.hud.rect(0.5, 0.5, SIZE + 0.03, SIZE + 0.03, 120, 90, 50, 235)
        core.hud.rect(0.5, 0.5, SIZE, SIZE, PAPER[1], PAPER[2], PAPER[3], 240)
        local ox, oz = math.floor(x / PIXEL) - SIDE // 2, math.floor(z / PIXEL) - SIDE // 2
        for row = 0, SIDE - 1 do
            local pz = oz + row
            local run, from = -1, 0
            local function flush(to: number)
                if run > 0 then
                    local rgb = PALETTE[run] or PAPER
                    local r, g, b = rgb[1], rgb[2], rgb[3]
                    local n = to - from
                    core.hud.rect(left + (from + n / 2) * cell / aspect, top + (row + 0.5) * cell,
                        n * cell + 0.0005, cell + 0.0005, r, g, b, 255)
                end
            end
            for col = 0, SIDE - 1 do
                local px = ox + col
                local c = seen[pk(px, pz)] or -1
                if c ~= run then
                    flush(col)
                    run, from = c, col
                end
            end
            flush(SIDE)
        end
        local function mark(wx: number, wz: number, r: number, g: number, b: number, s: number)
            local cx = (wx / PIXEL - ox) * cell
            local cz = (wz / PIXEL - oz) * cell
            if cx >= 0 and cx <= SIZE and cz >= 0 and cz <= SIZE then
                core.hud.rect(left + cx / aspect, top + cz, s, s, r, g, b, 255)
            end
        end
        mark(home_x, home_z, 200, 30, 30, cell * 2.4)
        mark(x, z, 20, 20, 20, cell * 2.2)
        mark(x, z, 255, 255, 255, cell * 1.2)
        local lx, _, lz = core.player_look(1)
        local len = math.sqrt((lx or 0) ^ 2 + (lz or 0) ^ 2)
        if len > 0.01 then
            mark(x + (lx :: number) / len * PIXEL * 2, z + (lz :: number) / len * PIXEL * 2, 255, 255, 255, cell * 0.9)
        end
        core.hud.text(0.5, top - 0.03, `{math.floor(x)}, {math.floor(z)}`, 0.03, 1, 240, 230, 200, 255)
        dial(0.5 + (SIZE / 2 + 0.09) / aspect, 0.5 - SIZE / 2 + 0.09, 0.06, aspect, x, z)
    end)
end
