--!strict
-- trophies: a shelf of trophies in a player's own room, for going places --
-- every world behind a gate, and five portals hidden out on the land.
--
-- WHAT WINS ONE, kept once for the whole server (core.shared_storage "t:KEY",
-- KEY the player's own as mods/home makes their room's name of it, so the
-- room reads it back without the player being there):
--   - a WORLD: back through its gate (gates:back, mods/gates), one a world;
--   - a HIDDEN PORTAL: standing in the place it leads to;
--   - the seven KEYS, each held once (they need not all be held at once);
--   - EIGHT WORLDS, and EVERY HIDDEN PORTAL, when those are in.
-- Each is told once with a notice and a chime; /page trophies lists them, with
-- a hint for those not won, and /trophies counts them.
--
-- THE HIDDEN PORTALS stand in the first room (the sandbox) only, five of
-- them, each a function of the world's seed and ground -- found once round the
-- spawn and kept (storage "site:ID"), built the first time a player comes
-- near with the columns in memory, and opened again at every start:
--   well    an old well with a rope down it; at the bottom, a tunnel;
--   summit  an arch on the highest ground round about, open only in winter;
--   lake    a frame with two lamps on the floor of a deep lake;
--   hill    a door dug into the side of a hill;
--   circle  a ring of standing stones, its arch open only at night.
-- Each leads to a place of its own, straight over or under it: an island in
-- the sky (56, the world's top being 64) where the ground under it is low,
-- else a cave of crystals at the bottom of the world. Its portal leads back.
-- Whoever stands there first wins its trophy and a diamond and some rubies.
-- The page says which way from the spawn each one is, and how far.
--
-- THE SHELF, in a player's own room (home-KEY, mods/home): two rows of
-- pedestals four and six steps east of the spawn (or west, or further off,
-- wherever the room is clear), a cup on each won -- in the colour of its
-- world -- and a right click on either says which it is. Built only into air:
-- what the owner put there stays.
--
-- ponytail: one shelf, where the room puts it; one hidden portal of a kind
-- per world, near the spawn. Scattered over a survival world's cells later.

if core.side() == "client" then
    return
end

-- What was kept is read once the world is ready: nothing is read or written before.
local READY = false
core.event.on("core:world_ready", function()
    READY = true
end)

local ROOM = core.room()
local IS_HOME = string.sub(ROOM, 1, 5) == "home-"
local IS_LAND = ROOM == "sandbox"

-- ------------------------------------------------------------ the trophies

type Trophy = { id: string, title: string, hint: string, rgb: { number }, row: number }

-- Row 1 the worlds, row 2 the rest; the order is the shelf's, south to north.
local TROPHIES: { Trophy } = {
    { id = "w.pista", title = "The Race Track", hint = "Through its gate on the plaza, and back.", rgb = { 230, 60, 50 }, row = 1 },
    { id = "w.inferno", title = "The Inferno", hint = "Through its gate on the plaza, and back.", rgb = { 250, 120, 20 }, row = 1 },
    { id = "w.arena", title = "The Arena", hint = "Through its gate on the plaza, and back.", rgb = { 150, 150, 160 }, row = 1 },
    { id = "w.arpg", title = "The Crypt Lands", hint = "Through its gate on the plaza, and back.", rgb = { 120, 140, 90 }, row = 1 },
    { id = "w.chambers", title = "The Test Chambers", hint = "Through its gate on the plaza, and back.", rgb = { 235, 235, 240 }, row = 1 },
    { id = "w.backrooms", title = "The Backrooms", hint = "Through its gate on the plaza, and back.", rgb = { 220, 200, 90 }, row = 1 },
    { id = "w.parkour", title = "The Parkour", hint = "Through its gate on the plaza, and back.", rgb = { 240, 140, 30 }, row = 1 },
    { id = "w.laboratorio", title = "The Laboratory", hint = "It opens to the seven keys.", rgb = { 90, 200, 220 }, row = 1 },
    { id = "w.front", title = "The Front", hint = "Through its gate on the plaza, and back.", rgb = { 70, 110, 200 }, row = 1 },
    { id = "w.assalto", title = "The Assault", hint = "Through its gate on the plaza, and back.", rgb = { 140, 60, 200 }, row = 1 },
    { id = "w.platformer", title = "The Platform Game", hint = "Through its gate on the plaza, and back.", rgb = { 60, 190, 70 }, row = 1 },
    { id = "w.shmup", title = "The Arcade Shooter", hint = "Through its gate on the plaza, and back.", rgb = { 230, 80, 180 }, row = 1 },
    { id = "h.well", title = "The Old Well", hint = "An old well with a rope down it. What is at the bottom?", rgb = { 110, 170, 230 }, row = 2 },
    { id = "h.summit", title = "The Summit", hint = "An arch on the highest ground round about. It opens in winter.", rgb = { 250, 250, 255 }, row = 2 },
    { id = "h.lake", title = "The Drowned Door", hint = "Two lamps on the floor of a deep lake.", rgb = { 40, 200, 190 }, row = 2 },
    { id = "h.hill", title = "The Door in the Hill", hint = "A door dug into the side of a hill.", rgb = { 160, 110, 60 }, row = 2 },
    { id = "h.circle", title = "The Night Circle", hint = "A ring of standing stones. It wakes at night.", rgb = { 130, 90, 240 }, row = 2 },
    { id = "keys", title = "The Seven Keys", hint = "Hold every world's key, once each.", rgb = { 250, 210, 60 }, row = 2 },
    { id = "explorer", title = "Explorer", hint = "Eight worlds.", rgb = { 250, 180, 90 }, row = 2 },
    { id = "secrets", title = "Keeper of Secrets", hint = "Every hidden portal.", rgb = { 200, 120, 255 }, row = 2 },
}

local BY_ID: { [string]: Trophy } = {}
for _, t in TROPHIES do
    BY_ID[t.id] = t
end

local KEYS = { "pista", "inferno", "arena", "arpg", "chambers", "backrooms", "parkour" }
local HIDDEN = { "well", "summit", "lake", "hill", "circle" }
local EXPLORER_AT = 8

-- The cups and their pedestals: a cup is a small glowing block that nobody
-- digs, in its trophy's colour.
local PEDESTAL = core.register_node("trophies:pedestal", 205, 185, 140, true, 0, "", 99, 1000.0, "stone")
local EMPTY    = core.register_node("trophies:pedestal_empty", 90, 80, 70, true, 0, "", 99, 1000.0, "stone")
local CUP: { [string]: number } = {}
local CUP_OF: { [number]: string } = {}
for _, t in TROPHIES do
    local name = string.gsub(t.id, "%.", "_")
    local cup = core.register_node(`trophies:cup_{name}`, t.rgb[1], t.rgb[2], t.rgb[3], true, 7, "", 99, 1000.0, "glass")
    core.node_box(cup, 0.3, 0, 0.3, 0.7, 0.15, 0.7)        -- the foot
    core.node_box(cup, 0.43, 0.15, 0.43, 0.57, 0.4, 0.57)  -- the stem
    core.node_box(cup, 0.22, 0.4, 0.22, 0.78, 0.75, 0.78)  -- the bowl
    CUP[t.id] = cup
    CUP_OF[cup] = t.id
end
-- The well's rope: climbed, up with jump, down crouching off.
local ROPE = core.register_node("trophies:rope", 150, 120, 70, false, 0, "", 0, 0.5, "wood")
core.node_box(ROPE, 0.44, 0, 0.44, 0.56, 1, 0.56)
core.node_climbable(ROPE, true)

-- The key a player's things are kept by: the same as mods/home names the room.
local function key_of(player: number): string
    local key = string.sub(string.gsub(string.lower(core.player_key(player)), "[^a-z0-9]", ""), 1, 24)
    if key == "" then
        return "n:" .. string.lower(core.player_name(player))
    end
    return key
end

local function won_of(key: string): { [string]: boolean }
    local r: { [string]: boolean } = {}
    for id in string.gmatch(core.shared_storage_get("t:" .. key), "[^;]+") do
        r[id] = true
    end
    return r
end

-- Only those on the shelf, not a single key's mark.
local function count(r: { [string]: boolean }): number
    local n = 0
    for _, t in TROPHIES do
        if r[t.id] then
            n += 1
        end
    end
    return n
end

local function save(key: string, r: { [string]: boolean })
    local ids = {}
    for id in r do
        table.insert(ids, id)
    end
    table.sort(ids)
    core.shared_storage_set("t:" .. key, table.concat(ids, ";"))
end

-- The hidden portals this world has (a site not found is no trophy to ask for).
local function sites_here(): { string }
    local s = core.shared_storage_get("sites")
    if s == "" then
        return HIDDEN
    end
    return string.split(s, ";")
end

local function won_ones(r: { [string]: boolean }, prefix: string): number
    local n = 0
    for id in r do
        if string.sub(id, 1, #prefix) == prefix then
            n += 1
        end
    end
    return n
end

-- Wins a trophy, or a mark towards one (k.ROOM, a key held), and whatever
-- that brings in. Read again every time: another room may have written since.
local function win(p: number, id: string)
    local key = key_of(p)
    local r = won_of(key)
    if r[id] then
        return
    end
    r[id] = true
    local told: { string } = {}
    if BY_ID[id] then
        table.insert(told, id)
    end
    if not r.keys then
        local all = true
        for _, k in KEYS do
            if not r["k." .. k] then all = false end
        end
        if all then
            r.keys = true
            table.insert(told, "keys")
        end
    end
    if not r.explorer and won_ones(r, "w.") >= EXPLORER_AT then
        r.explorer = true
        table.insert(told, "explorer")
    end
    if not r.secrets then
        local all = true
        for _, h in sites_here() do
            if not r["h." .. h] then all = false end
        end
        if all then
            r.secrets = true
            table.insert(told, "secrets")
        end
    end
    save(key, r)
    local n = count(r)
    for _, t in told do
        core.notice(p, `Trophy: {BY_ID[t].title} ({n} of {#TROPHIES}). /page trophies`)
        core.log(`trophies: {core.player_name(p)} won {t}`)
    end
    if #told > 0 then
        local x, y, z = core.player_pos(p)
        if x then
            core.play_sound("goals:reached", x, y + 1.6, z)
        end
    end
end

-- ------------------------------------------------------------ what wins one

core.event.on("gates:back", function(ev)
    local room = ev.room :: string
    if BY_ID["w." .. room] then
        win(ev.player, "w." .. room)
    end
end)

-- The keys, as mods/goals looks at them: in the bag, every two seconds.
local looked = 0.0
core.event.on("core:tick", function(ev)
    looked += ev.dt
    if looked < 2.0 or not READY then
        return
    end
    looked = 0.0
    for _, p in core.players() do
        for _, k in KEYS do
            if core.bag_count(p, "keys:" .. k) > 0 then
                win(p, "k." .. k)
            end
        end
    end
end)

-- ------------------------------------------------------------ the page

-- Which way from the spawn a hidden portal is: 0 north (-z), clockwise.
local DIRECTION_TEXT: { (string) -> string } = {
    function(s) return `North of the spawn, about {s} steps.` end,
    function(s) return `North-east of the spawn, about {s} steps.` end,
    function(s) return `East of the spawn, about {s} steps.` end,
    function(s) return `South-east of the spawn, about {s} steps.` end,
    function(s) return `South of the spawn, about {s} steps.` end,
    function(s) return `South-west of the spawn, about {s} steps.` end,
    function(s) return `West of the spawn, about {s} steps.` end,
    function(s) return `North-west of the spawn, about {s} steps.` end,
}

core.page_register("trophies", "Trophies")

local function show_page(p: number)
    local r = won_of(key_of(p))
    local page = { "# Trophies", `{count(r)} of {#TROPHIES} won.`, "" }
    local sites: { [string]: boolean } = {}
    for _, h in sites_here() do
        sites[h] = true
    end
    local function section(title: string, row: number, prefix: string?)
        table.insert(page, `## {title}`)
        for _, t in TROPHIES do
            if t.row ~= row or (prefix and string.sub(t.id, 1, #prefix) ~= prefix) then
                continue
            end
            if r[t.id] then
                table.insert(page, `| {t.title} | won |`)
            elseif string.sub(t.id, 1, 2) == "h." and not sites[string.sub(t.id, 3)] then
                table.insert(page, `| {t.title} | not in this world |`)
            else
                table.insert(page, `| {t.title} | {t.hint} |`)
                local where = core.shared_storage_get("where:" .. string.sub(t.id, 3))
                if string.sub(t.id, 1, 2) == "h." and where ~= "" then
                    local dir, steps = string.match(where, "^(%d) (%d+)$")
                    if dir and steps then
                        table.insert(page, "> " .. DIRECTION_TEXT[(tonumber(dir) :: number) + 1](steps :: string))
                    end
                end
            end
        end
        table.insert(page, "")
    end
    section("Worlds", 1)
    section("Hidden portals", 2, "h.")
    table.insert(page, "## More")
    for _, id in { "keys", "explorer", "secrets" } do
        local t = BY_ID[id]
        table.insert(page, if r[id] then `| {t.title} | won |` else `| {t.title} | {t.hint} |`)
    end
    table.insert(page, "")
    table.insert(page, "Their cups stand in your own room.")
    table.insert(page, "")
    table.insert(page, "[Goals](page:goals)")
    core.page_show(p, "trophies", table.concat(page, "\n"))
end

core.event.on("core:page_open", function(ev)
    if ev.page == "trophies" then
        show_page(ev.player)
    end
end)

core.command("trophies", "[sites|go ID [place] [NAME]|home [NAME]] -- how many trophies you have won; /page trophies lists them (sites and go: admin)", "")
core.event.on("core:command", function(ev)
    if ev.name ~= "trophies" or string.match(ev.args, "^%s*sites") or string.match(ev.args, "^%s*go") then
        return
    end
    local r = won_of(key_of(ev.player))
    core.chat(ev.player, `Trophies: {count(r)} of {#TROPHIES} won. /page trophies lists them.`)
end)

-- ================================================================ the hidden portals

local function mul32(a: number, b: number): number
    local hi = math.floor(a / 65536)
    local lo = a % 65536
    return ((hi * b) % 65536 * 65536 + lo * b) % 4294967296
end
local function hash(x: number, z: number, salt: number): number
    local h = mul32(x % 4294967296 + 0x9E3779B9, 374761393)
    h = bit32.bxor(h, mul32(z % 4294967296 + 0x85EBCA6B, 668265263))
    h = bit32.bxor(h, mul32(salt % 4294967296, 2246822519))
    h = bit32.bxor(h, bit32.rshift(h, 13))
    return mul32(h, 1274126177)
end

if IS_LAND then
    local function id(name: string, fallback: number?): number
        local n = core.node_id(name)
        if n == 0 and fallback then
            return fallback
        end
        return n
    end
    local AIR = 0
    local STONE   = id("core:stone")
    local GRASS   = id("core:grass", STONE)
    local DIRT    = id("core:dirt", STONE)
    local COBBLE  = id("rocks:cobblestone", STONE)
    local MOSSY   = id("rocks:mossy_cobblestone", COBBLE)
    local BRICKS  = id("rocks:stone_bricks", STONE)
    local BRICK   = id("ruins:mossy_brick", MOSSY)
    local LAMP    = id("ruins:lamp", BRICKS)
    local PLANKS  = id("timber:planks", COBBLE)
    local TRUNK   = id("trees:trunk", PLANKS)
    local WATER   = id("lakes:water")
    local SLATE   = id("deep:slate", STONE)
    local CRYSTAL = id("deep:crystal", LAMP)
    local MOSS    = id("deep:glowmoss")
    local FLOWERS: { number } = {}
    for _, f in { "dandelion", "poppy", "cornflower", "oxeye_daisy", "allium", "red_tulip", "white_tulip" } do
        local n = core.node_id("plants:" .. f)
        if n ~= 0 then table.insert(FLOWERS, n) end
    end

    local SEED = core.seed()
    local TOP = 63                     -- the highest cell of a world made 64 tall
    local SKY = 56                     -- the island's walking height
    local th = core.terrain_height

    local PLAZA_LO: number, PLAZA_HI: number = math.huge, -math.huge
    do
        local lo, hi = core.plaza()
        if lo and hi then
            PLAZA_LO, PLAZA_HI = lo, hi
        end
    end

    local function put(x: number, y: number, z: number, b: number)
        if y >= 1 and y <= TOP then
            core.set_block(x, y, z, b)
        end
    end
    local function fill(x0: number, x1: number, y0: number, y1: number, z0: number, z1: number, b: number)
        for x = x0, x1 do
            for y = y0, y1 do
                for z = z0, z1 do put(x, y, z, b) end
            end
        end
    end
    -- The ring round a 1x2 hole whose lowest cell is (x, y, z), standing in the
    -- XY plane; `hole` what the hole holds (air, or water under a lake).
    local function frame(x: number, y: number, z: number, b: number, hole: number?)
        for dx = -1, 1 do
            for dy = -1, 2 do
                put(x + dx, y + dy, z, if dx == 0 and (dy == 0 or dy == 1) then (hole or AIR) else b)
            end
        end
    end
    local function dry(x: number, z: number): boolean
        return core.water_level(x, z) <= th(x, z)
    end
    -- Every corner and the middle of the square round (x, z) within tol of g.
    local function flat(x: number, z: number, r: number, tol: number, g: number): boolean
        for _, c in { { -r, -r }, { r, -r }, { -r, r }, { r, r }, { 0, r }, { r, 0 }, { 0, -r }, { -r, 0 } } do
            local t = th(x + c[1], z + c[2])
            if math.abs(t - g) > tol or not dry(x + c[1], z + c[2]) then
                return false
            end
        end
        return true
    end

    local pinned: { [string]: boolean } = {}   -- "name id": a hidden portal put on that player's minimap
    type Site = {
        id: string,
        ax: number, ag: number, az: number,   -- where it was found: ground's first air
        hx: number, hy: number, hz: number,   -- its hole's lowest cell
        sky: boolean,                         -- an island over it, or a cave under it
        vy: number,                           -- the place's walking height
        built: boolean, a: number, b: number, open: boolean,
    }
    local sites: { Site } = {}

    -- Which way the hole is, as the shelf's page says it.
    local function where(s: Site, sx: number, sz: number): string
        local dx, dz = s.ax - sx, s.az - sz
        local a = math.atan2(dx, -dz)                     -- 0 north (-z), clockwise
        local sector = math.floor((a + math.pi / 8) / (math.pi / 4)) % 8
        local steps = math.max(10, math.floor(math.sqrt(dx * dx + dz * dz) / 10 + 0.5) * 10)
        return `{sector} {steps}`
    end

    local function hole_of(kind: string, ax: number, ag: number, az: number): (number, number, number)
        if kind == "well" then
            return ax, ag - 9, az - 3
        elseif kind == "hill" then
            return ax, ag, az - 4
        end
        return ax, ag, az
    end

    local function make(kind: string, ax: number, ag: number, az: number): Site
        local high = -math.huge
        for dx = -7, 7, 2 do
            for dz = -7, 7, 2 do
                high = math.max(high, th(ax + dx, az + dz))
            end
        end
        local hx, hy, hz = hole_of(kind, ax, ag, az)
        local sky = high <= SKY - 12
        return { id = kind, ax = ax, ag = ag, az = az, hx = hx, hy = hy, hz = hz, sky = sky,
                 vy = if sky then SKY else 4, built = core.storage_get("built:" .. kind) == "1",
                 a = -1, b = -1, open = false }
    end

    -- ------------------------------------------------------------ finding them

    local function clear_of(x: number, z: number): boolean
        if x >= PLAZA_LO - 24 and x <= PLAZA_HI + 24 and z >= PLAZA_LO - 24 and z <= PLAZA_HI + 24 then
            return false
        end
        for _, s in sites do
            if (s.ax - x) ^ 2 + (s.az - z) ^ 2 < 48 * 48 then
                return false
            end
        end
        return true
    end

    -- Places in a ring round the spawn, as the seed has them, until one passes.
    local function ring(salt: number, lo: number, hi: number, sx: number, sz: number,
                        test: (number, number) -> number?): (number?, number, number)
        for i = 0, 799 do
            local h1, h2 = hash(i, salt, SEED), hash(salt, i, SEED + 77)
            local a = (h1 % 3600) / 3600 * 2 * math.pi
            local d = lo + h2 % (hi - lo)
            local x = math.floor(sx + math.cos(a) * d)
            local z = math.floor(sz + math.sin(a) * d)
            if clear_of(x, z) then
                local g = test(x, z)
                if g then
                    return x, g, z
                end
            end
        end
        return nil, 0, 0
    end

    local TESTS: { [string]: (number, number) -> number? } = {
        well = function(x, z)
            local g = th(x, z)
            if g < 19 or g > 50 or not flat(x, z, 3, 1, g) then return nil end
            return g
        end,
        lake = function(x, z)
            local g, w = th(x, z), core.water_level(x, z)
            -- Lakes are shallow (seed 1: none past six): four is the frame and its lamps.
            if w - g < 4 or w > TOP - 4 then return nil end
            for _, c in { { -2, -2 }, { 2, -2 }, { -2, 2 }, { 2, 2 }, { 0, 2 } } do
                if core.water_level(x + c[1], z + c[2]) - th(x + c[1], z + c[2]) < 2 then return nil end
            end
            return g
        end,
        hill = function(x, z)
            local g = th(x, z)
            -- The ground rising to the north: the tunnel's lining is built anyway,
            -- so what is asked is a hillside over its end and an open way in.
            if g < 19 or g > 55 or not dry(x, z) or not dry(x, z + 2) then return nil end
            if th(x, z + 1) > g + 1 or th(x, z + 2) > g + 1 then return nil end
            if th(x, z - 3) < g + 3 or th(x, z - 4) < g + 3 then return nil end
            return g
        end,
        circle = function(x, z)
            local g = th(x, z)
            if g < 19 or g > 50 or core.biome(x, z) ~= "core:plains" or not flat(x, z, 6, 1, g) then return nil end
            return g
        end,
    }
    local RINGS: { [string]: { number } } = {
        well = { 60, 200 }, lake = { 50, 400 }, hill = { 60, 320 }, circle = { 80, 320 },
    }

    -- The summit: the highest dry ground of 800 places in its ring, room for its
    -- landing round it.
    local function summit(sx: number, sz: number): (number?, number, number)
        local best: number?, bg, bz = nil, -1, 0
        for i = 0, 799 do
            local h1, h2 = hash(i, 0x5A11, SEED), hash(0x5A11, i, SEED + 77)
            local a = (h1 % 3600) / 3600 * 2 * math.pi
            local d = 64 + h2 % 256
            local x = math.floor(sx + math.cos(a) * d)
            local z = math.floor(sz + math.sin(a) * d)
            local g = th(x, z)
            if g > bg and g <= TOP - 6 and clear_of(x, z) and flat(x, z, 2, 3, g) then
                best, bg, bz = x, g, z
            end
        end
        return best, bg, bz
    end

    local found = false
    local function find_all(): boolean
        local sx, _, sz = core.world_spawn()
        if sx == nil then
            return false
        end
        local spawn_x, spawn_z = math.floor(sx), math.floor(sz)
        local have: { string } = {}
        for n, kind in HIDDEN do
            local ax: number?, ag, az = nil, 0, 0
            local kept = core.storage_get("site:" .. kind)
            local kx, kg, kz = string.match(kept, "^(-?%d+) (-?%d+) (-?%d+)$")
            if kx then
                ax, ag, az = tonumber(kx), tonumber(kg) :: number, tonumber(kz) :: number
            elseif kept ~= "none" then
                if kind == "summit" then
                    ax, ag, az = summit(spawn_x, spawn_z)
                else
                    local r = RINGS[kind]
                    ax, ag, az = ring(0x7000 + n, r[1], r[2], spawn_x, spawn_z, TESTS[kind])
                end
                core.storage_set("site:" .. kind, if ax then `{ax} {ag} {az}` else "none")
            end
            if ax then
                local s = make(kind, ax :: number, ag, az)
                table.insert(sites, s)
                table.insert(have, kind)
                core.shared_storage_set("where:" .. kind, where(s, spawn_x, spawn_z))
                core.log(`trophies: the {kind} at {s.hx} {s.hy} {s.hz}, its place {if s.sky then "in the sky" else "in a cave"}`)
            else
                core.log(`trophies: no place for the {kind} round the spawn`)
            end
        end
        core.shared_storage_set("sites", table.concat(have, ";"))
        core.log(`trophies: {#have} hidden portals`)
        return true
    end

    -- ------------------------------------------------------------ building them

    local BUILD: { [string]: (number, number, number) -> () } = {
        -- A ring of mossy stone over a shaft ten deep, a rope down one corner from
        -- a beam on two posts; at the bottom a tunnel north to the hole.
        well = function(ax, G, az)
            fill(ax - 2, ax + 3, G, G + 3, az - 2, az + 3, AIR)
            for x = ax - 2, ax + 3 do
                for z = az - 2, az + 3 do
                    local t = th(x, z)
                    if t < G - 1 then fill(x, x, t, G - 2, z, z, DIRT) end
                    put(x, G - 1, z, GRASS)
                end
            end
            for x = ax - 1, ax + 2 do
                for z = az - 1, az + 2 do
                    if x < ax or x > ax + 1 or z < az or z > az + 1 then
                        put(x, G, z, MOSSY)
                        fill(x, x, G - 10, G - 1, z, z, COBBLE)
                    end
                end
            end
            local B = G - 9
            fill(ax, ax + 1, B, G, az, az + 1, AIR)
            fill(ax, ax + 1, B - 1, B - 1, az, az + 1, COBBLE)
            put(ax, B - 1, az + 1, LAMP)
            for y = G + 1, G + 2 do
                put(ax - 1, y, az + 1, TRUNK)
                put(ax + 2, y, az + 1, TRUNK)
            end
            fill(ax - 1, ax + 2, G + 3, G + 3, az + 1, az + 1, PLANKS)
            fill(ax + 1, ax + 1, B, G + 2, az + 1, az + 1, ROPE)
            for z = az - 2, az - 1 do
                fill(ax - 1, ax - 1, B, B + 1, z, z, COBBLE)
                fill(ax + 1, ax + 1, B, B + 1, z, z, COBBLE)
                fill(ax, ax, B, B + 1, z, z, AIR)
                put(ax, B + 2, z, COBBLE)
                put(ax, B - 1, z, COBBLE)
            end
            frame(ax, B, az - 3, BRICK)
        end,
        -- A landing of stone bricks, an arch and a lamp over it.
        summit = function(ax, G, az)
            for x = ax - 2, ax + 2 do
                for z = az - 2, az + 2 do
                    local t = th(x, z)
                    if t < G - 1 then fill(x, x, t, G - 2, z, z, STONE) end
                end
            end
            fill(ax - 2, ax + 2, G - 1, G - 1, az - 2, az + 2, BRICKS)
            fill(ax - 2, ax + 2, G, math.min(G + 4, TOP), az - 2, az + 2, AIR)
            frame(ax, G, az, BRICK)
            put(ax, G + 3, az, LAMP)
        end,
        -- A floor of bricks on the lake's bed, the frame on it, a lamp either side.
        lake = function(ax, G, az)
            fill(ax - 2, ax + 2, G - 1, G - 1, az - 2, az + 2, BRICKS)
            frame(ax, G, az, BRICKS, if WATER ~= 0 then WATER else AIR)
            put(ax - 1, G + 3, az, LAMP)
            put(ax + 1, G + 3, az, LAMP)
        end,
        -- A doorway of mossy stone at the foot of the hill, a tunnel three long,
        -- a lamp in its roof and the hole at its end.
        hill = function(ax, G, az)
            -- Steps of earth up to the doorway where the ground in front is lower:
            -- level for two, then one down a cell, so nothing is more than a step.
            for k = 1, 5 do
                local z, top = az + k, G - 1 - math.max(0, k - 2)
                for x = ax - 1, ax + 1 do
                    local t = th(x, z)
                    if t - 1 < top then
                        fill(x, x, t, top - 1, z, z, DIRT)
                        put(x, top, z, GRASS)
                    end
                    fill(x, x, top + 1, top + 3, z, z, AIR)
                end
            end
            for z = az - 3, az do
                put(ax, G - 1, z, COBBLE)
                fill(ax, ax, G, G + 1, z, z, AIR)
                if z < az then
                    fill(ax - 1, ax - 1, G, G + 1, z, z, MOSSY)
                    fill(ax + 1, ax + 1, G, G + 1, z, z, MOSSY)
                    put(ax, G + 2, z, MOSSY)
                end
            end
            fill(ax - 1, ax - 1, G, G + 1, az, az, MOSSY)
            fill(ax + 1, ax + 1, G, G + 1, az, az, MOSSY)
            fill(ax - 1, ax + 1, G + 2, G + 2, az, az, MOSSY)
            put(ax, G + 2, az - 2, LAMP)
            frame(ax, G, az - 4, BRICK)
        end,
        -- Eight standing stones round a smooth arch on a lawn.
        circle = function(ax, G, az)
            fill(ax - 6, ax + 6, G, G + 4, az - 6, az + 6, AIR)
            fill(ax - 6, ax + 6, G - 1, G - 1, az - 6, az + 6, GRASS)
            for i = 0, 7 do
                local a = i * math.pi / 4
                local x = ax + math.floor(5 * math.cos(a) + 0.5)
                local z = az + math.floor(5 * math.sin(a) + 0.5)
                local h = 2 + i % 2
                fill(x, x, G, G + h - 2, z, z, MOSSY)
                put(x, G + h - 1, z, COBBLE)
            end
            frame(ax, G, az, BRICKS)
        end,
    }

    -- The place behind a hole: an island of grass in the sky, or a cave of
    -- slate and crystals at the bottom of the world; its hole at the north.
    local function build_place(s: Site)
        local ax, az = s.ax, s.az
        if s.sky then
            local y = SKY - 1
            for dx = -7, 7 do
                for dz = -7, 7 do
                    local d2, x, z = dx * dx + dz * dz, ax + dx, az + dz
                    if d2 <= 36 then
                        fill(x, x, y + 1, y + 5, z, z, AIR)
                        put(x, y, z, GRASS)
                        if d2 <= 25 then put(x, y - 1, z, DIRT) end
                        if d2 <= 12 then put(x, y - 2, z, DIRT) end
                        if d2 <= 5 then put(x, y - 3, z, STONE) end
                        if d2 <= 1 then put(x, y - 4, z, STONE) end
                        if d2 > 26 then
                            put(x, y + 1, z, MOSSY)            -- a low wall round the edge
                        elseif #FLOWERS > 0 and d2 > 4 and dz > -3 and hash(x, z, SEED + 0xF10) % 5 == 0 then
                            put(x, y + 1, z, FLOWERS[hash(z, x, SEED) % #FLOWERS + 1])
                        end
                    end
                end
            end
            frame(ax, SKY, az - 4, BRICK)
            put(ax, SKY, az + 1, BRICKS)
            put(ax, SKY + 1, az + 1, CRYSTAL)
        else
            fill(ax - 6, ax + 6, 2, 10, az - 6, az + 6, SLATE)
            fill(ax - 5, ax + 5, 4, 8, az - 4, az + 5, AIR)
            frame(ax, 4, az - 5, BRICK)
            for _, c in { { -5, -4 }, { 5, -4 }, { -5, 5 }, { 5, 5 }, { -5, 1 }, { 5, 1 } } do
                put(ax + c[1], 4, az + c[2], CRYSTAL)
            end
            for _, c in { { 0, 0 }, { -3, 3 }, { 3, 3 }, { -3, -2 }, { 3, -2 }, { 0, 4 }, { -3, 0 }, { 3, 0 } } do
                put(ax + c[1], 9, az + c[2], LAMP)
            end
            for _, c in { { -6, -1 }, { 6, -1 }, { -6, 3 }, { 6, 3 } } do   -- in the walls, at eye height
                put(ax + c[1], 6, az + c[2], LAMP)
            end
            if MOSS ~= 0 then
                for dx = -4, 4 do
                    for dz = -3, 4 do
                        if hash(ax + dx, az + dz, SEED + 0x3055) % 6 == 0 then put(ax + dx, 4, az + dz, MOSS) end
                    end
                end
            end
            put(ax, 4, az + 1, BRICKS)
            put(ax, 5, az + 1, CRYSTAL)
        end
    end

    local function place_hole(s: Site): (number, number, number)
        if s.sky then
            return s.ax, SKY, s.az - 4
        end
        return s.ax, 4, s.az - 5
    end

    local function ready_to_build(s: Site): boolean
        for _, c in { { 0, 0 }, { -7, -7 }, { 7, -7 }, { -7, 7 }, { 7, 7 } } do
            if not core.loaded(s.ax + c[1], s.ag, s.az + c[2]) or not core.loaded(s.ax + c[1], s.vy, s.az + c[2]) then
                return false
            end
        end
        return core.loaded(s.hx, s.hy, s.hz)
    end

    local function build(s: Site)
        BUILD[s.id](s.ax, s.ag, s.az)
        build_place(s)
        core.storage_set("built:" .. s.id, "1")
        s.built = true
        core.log(`trophies: built the {s.id}`)
    end

    -- ------------------------------------------------------------ opening them

    local function night(): boolean
        local t = core.time_of_day()
        return t < 0.21 or t > 0.79
    end
    -- The season, as mods/weather says it (farm:season); "" with no weather,
    -- and then the summit is always open.
    local season = ""
    pcall(core.event.on, "farm:season", function(ev: EventPayload)
        season = ev.name :: string
    end, nil)

    -- The place's end first, kept open; the land's end opened and linked to it,
    -- and for the circle closed again by day, for the summit out of winter.
    local function keep_open(s: Site)
        if s.b < 0 then
            local px, py, pz = place_hole(s)
            s.b = core.portal_open(px + 0.5, py + 1.0, pz + 0.5, 0, 1, 0, 0.5, 1.0)
            if s.b < 0 then return end
            core.portal_link(s.b, -1)   -- unlinked it would lead to its id xor 1: somebody else's
        end
        local want = if s.id == "circle" then night()
            elseif s.id == "summit" then season == "" or season == "winter"
            else true
        if want and not s.open then
            s.a = core.portal_open(s.hx + 0.5, s.hy + 1.0, s.hz + 0.5, 0, 1, 0, 0.5, 1.0)
            if s.a < 0 then return end
            core.portal_link(s.a, s.b)
            s.open = true
        elseif not want and s.open then
            core.portal_close(s.hx + 0.5, s.hy + 1.0, s.hz + 0.5)
            core.portal_link(s.b, -1)
            s.a, s.open = -1, false
        end
    end

    local function in_place(s: Site, x: number, y: number, z: number): boolean
        if (x - s.ax - 0.5) ^ 2 + (z - s.az - 0.5) ^ 2 > 7.5 * 7.5 then return false end
        return y >= s.vy - 1 and y <= s.vy + 5
    end

    local every = 0.0
    core.event.on("core:tick", function(ev)
        if not READY then return end
        if not found then
            found = find_all()
            if not found then return end
        end
        every += ev.dt
        if every < 0.5 then return end
        every = 0.0
        local players = core.players()
        for _, s in sites do
            -- On the minimap of a player near it who carries a seeking or deep
            -- one (mods/maps: a pin, kept by the map, so sent once a player).
            for _, p in players do
                local x, _, z = core.player_pos(p)
                local name = core.player_name(p)
                if x and not pinned[`{name} {s.id}`] and (x - s.ax) ^ 2 + (z - s.az) ^ 2 < 128 * 128
                    and core.bag_count(p, "maps:seeking_minimap") + core.bag_count(p, "maps:deep_minimap") > 0 then
                    pinned[`{name} {s.id}`] = true
                    pcall(core.event.emit, "maps:pin", { player = p, label = `hidden {s.id}`, x = s.ax, z = s.az, colour = 5 })
                end
            end
            if not s.built then
                for _, p in players do
                    local x, _, z = core.player_pos(p)
                    if x and (x - s.ax) ^ 2 + (z - s.az) ^ 2 < 80 * 80 and ready_to_build(s) then
                        build(s)
                        break
                    end
                end
            end
            if s.built then
                keep_open(s)
                for _, p in players do
                    local x, y, z = core.player_pos(p)
                    if x and in_place(s, x, y, z) then
                        local r = won_of(key_of(p))
                        if not r["h." .. s.id] then
                            win(p, "h." .. s.id)
                            core.give_item(p, "ore:diamond", 1)
                            core.give_item(p, "ore:ruby", 5)
                            core.chat(p, "A diamond and five rubies were left here for whoever found the way.")
                        end
                        -- The circle by day: its arch on the land is shut, so the
                        -- frame here sends whoever walks into it out in front of it.
                        if s.id == "circle" and not s.open then
                            local px, py, pz = place_hole(s)
                            if math.abs(x - px - 0.5) < 0.8 and math.abs(z - pz - 0.5) < 0.9 and y < py + 2 then
                                core.player_place(p, s.hx + 0.5, s.hy, s.hz + 1.5, 0, 0, 1)
                            end
                        end
                    end
                end
            end
        end
    end)

    -- /trophies sites (admin, a world's own check): where the five are.
    -- /trophies go ID [place] (admin): in front of one, or into its place.
    core.event.on("core:command", function(ev)
        local words = string.split(string.gsub(ev.args, "^%s+", ""), " ")
        local verb, what = words[1], words[2] or ""
        local where_to = if words[3] == "place" then "place" else ""
        -- The last word, when it is not "place", names somebody else to send.
        local who = ev.player
        local name = if verb == "home" then words[2] elseif words[3] == "place" then words[4] else words[3]
        if ev.name ~= "trophies" or (verb ~= "sites" and verb ~= "go" and verb ~= "home") then
            return
        end
        if not core.has_perm(ev.player, "admin") then
            core.chat(ev.player, "Only an admin may.")
            return
        end
        if verb == "sites" then
            for _, s in sites do
                core.chat(ev.player, `{s.id}: {s.hx} {s.hy} {s.hz} -> {if s.sky then "sky" else "cave"} {s.ax} {s.vy} {s.az}, built {s.built}, open {s.open}`)
            end
            return
        end
        if name and name ~= "" then
            local found_one = false
            for _, p in core.players() do
                if string.lower(core.player_name(p)) == string.lower(name) then
                    who, found_one = p, true
                end
            end
            if not found_one then
                core.chat(ev.player, `Nobody called {name} is here.`)
                return
            end
        end
        -- /trophies home [NAME]: into that player's own room, to see the shelf.
        if verb == "home" then
            local key = key_of(who)
            if string.sub(key, 1, 2) == "n:" or not core.room_send(who, "home-" .. key) then
                core.chat(ev.player, "No room of their own to send them to.")
            end
            return
        end
        for _, s in sites do
            if s.id == what then
                if where_to == "place" then
                    core.player_place(who, s.ax + 0.5, s.vy, s.az + 3.5, 0, 0, -1)   -- facing the way back
                else
                    -- In front of the hole, but on the land: the well's is down its shaft.
                    local x, y, z = s.hx + 0.5, s.hy, s.hz + 1.5
                    if s.id == "well" then x, y, z = s.ax - 1.5, s.ag, s.az + 0.5 end
                    if s.id == "hill" then x, y, z = s.ax + 0.5, s.ag, s.az + 1.5 end
                    -- Looking at the hole (the last three are a direction, not a place).
                    local dx, dy, dz = s.hx + 0.5 - x, s.hy + 1 - (y + 1.6), s.hz + 0.5 - z
                    local len = math.max(0.001, math.sqrt(dx * dx + dy * dy + dz * dz))
                    core.player_place(who, x, y, z, dx / len, dy / len, dz / len)
                end
                return
            end
        end
        core.chat(ev.player, "/trophies go well|summit|lake|hill|circle [place] [NAME]")
    end)
end

-- ================================================================ the shelf

if IS_HOME then
    local OWNER = string.sub(ROOM, 6)
    local FLOOR = core.node_id("home:floor")

    -- Where each trophy's pedestal stands (plan, below).
    local spots: { [string]: { number } } = {}
    local by_cell: { [string]: string } = {}
    local base_known = false

    local function free(x: number, y: number, z: number): boolean
        local b = core.get_block(x, y, z)
        return b == 0 or b == PEDESTAL or b == EMPTY or CUP_OF[b] ~= nil
    end

    -- Where the two rows go: four and six east of the spawn if nothing of the
    -- room's is there, else the first of these places that is clear -- west,
    -- further south, further out. Chosen once, when the cells are in memory,
    -- and kept ("board": row 1's x, row 2's x, the first z, the floor's y).
    local PLACES = { { 4, 6, -6 }, { -4, -6, -6 }, { 4, 6, 8 }, { -4, -6, 8 }, { 10, 12, -6 }, { -10, -12, -6 } }

    local function lay(x1: number, x2: number, z1: number, y0: number)
        local n = { 0, 0 }
        for _, t in TROPHIES do
            local x = if t.row == 1 then x1 else x2
            local z = z1 + n[t.row]
            n[t.row] += 1
            spots[t.id] = { x, y0, z }
            by_cell[`{x} {z}`] = t.id
        end
    end

    local function plan(): boolean
        local kx1, kx2, kz, ky = string.match(core.storage_get("board"), "^(-?%d+) (-?%d+) (-?%d+) (-?%d+)$")
        if kx1 then
            lay(tonumber(kx1) :: number, tonumber(kx2) :: number, tonumber(kz) :: number, tonumber(ky) :: number)
            return true
        end
        local sx, sy, sz = core.world_spawn()
        if sx == nil then
            return false
        end
        local x0, y0, z0 = math.floor(sx), math.floor(sy), math.floor(sz)
        local function clear(o: { number }): boolean
            for _, x in { x0 + o[1], x0 + o[2] } do
                for z = z0 + o[3], z0 + o[3] + 11 do
                    if not core.loaded(x, y0, z) then return false end
                    if not free(x, y0, z) or not free(x, y0 + 1, z) then return false end
                end
            end
            return true
        end
        for _, o in PLACES do
            if not core.loaded(x0 + o[1], y0, z0 + o[3]) or not core.loaded(x0 + o[2], y0, z0 + o[3] + 11) then
                return false   -- not in memory yet: ask again later rather than settle on a worse place
            end
        end
        local pick = PLACES[1]
        for _, o in PLACES do
            if clear(o) then
                pick = o
                break
            end
        end
        core.storage_set("board", `{x0 + pick[1]} {x0 + pick[2]} {z0 + pick[3]} {y0}`)
        core.log(`home: the trophy shelf of {ROOM} at x {x0 + pick[1]} and {x0 + pick[2]}, z {z0 + pick[3]}`)
        lay(x0 + pick[1], x0 + pick[2], z0 + pick[3], y0)
        return true
    end

    local shown = "?"
    local function show()
        local text = core.shared_storage_get("t:" .. OWNER)
        if text == shown then
            return
        end
        for _, c in spots do
            if not core.loaded(c[1], c[2], c[3]) then return end
        end
        shown = text
        local r = won_of(OWNER)
        for _, t in TROPHIES do
            local c = spots[t.id]
            local x, y, z = c[1], c[2], c[3]
            if free(x, y, z) and free(x, y + 1, z) then
                if core.get_block(x, y - 1, z) == 0 and FLOOR ~= 0 then
                    core.set_block(x, y - 1, z, FLOOR)
                end
                core.set_block(x, y, z, if r[t.id] then PEDESTAL else EMPTY)
                core.set_block(x, y + 1, z, if r[t.id] then CUP[t.id] else 0)
            end
        end
    end

    local since = 0.0
    core.event.on("core:tick", function(ev)
        since += ev.dt
        if since < 2.0 or not READY then return end
        since = 0.0
        if not base_known then
            base_known = plan()
            if not base_known then return end
        end
        if #core.players() > 0 then
            show()
        end
    end)

    core.event.on("core:used", function(ev)
        local b = ev.id :: number
        if b ~= PEDESTAL and b ~= EMPTY and CUP_OF[b] == nil then
            return
        end
        local which = by_cell[`{ev.x} {ev.z}`]
        if which == nil then
            return
        end
        local t = BY_ID[which]
        if won_of(OWNER)[which] then
            core.chat(ev.player, `Trophy: {t.title} -- won.`)
        else
            core.chat(ev.player, `Trophy: {t.title} -- not yet. {t.hint}`)
        end
    end)
end
