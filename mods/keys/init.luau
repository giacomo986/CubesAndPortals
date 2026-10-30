--!strict
-- keys: the seven keys of the ending (TODO.md "The game as a whole").
--
-- The engineer who built the plaza's portals shut l'Ospite in the laboratory
-- and split its lock into seven keys, one hidden in each themed world behind a
-- trial. This mod is linked into every one of those worlds and into the
-- sandbox. Everywhere it registers the seven keys (items, one each, carried
-- from room to room in the bag like anything else) and /diario, which reads
-- the engineer's page of every key held. A world's trial says it is passed
-- (event keys:earned) and its key waits on an altar by the way out.
--
-- In the sandbox -- the world with an opening onto the laboratory -- it also
-- keeps, per player, which keys they have ever held (so a key dropped or put
-- in a chest is not progress lost), answers a click on a pedestal of the
-- laboratory's gate (mods/gates builds them), and keeps the laboratory shut
-- to anybody who has not held all seven: walking up to its opening puts them
-- back four nodes out.

local WORLDS = { "pista", "inferno", "arena", "arpg", "chambers", "backrooms", "parkour" }

local TITLE: { [string]: string } = {
    pista = "Chiave della Corsa",
    inferno = "Chiave del Fuoco",
    arena = "Chiave dell'Assedio",
    arpg = "Chiave della Cripta",
    chambers = "Chiave della Prova",
    backrooms = "Chiave dell'Uscita",
    parkour = "Chiave della Vetta",
}

-- The engineer's diary, a page with every key. Serious, and each page says
-- what that key will be for in the laboratory.
local PAGE: { [string]: string } = {
    pista = "Ho registrato il mio giro migliore sulla pista che attraversa l'Inferno. Chi vuole "
        .. "questa chiave deve battermi tre volte di fila. Quando il laboratorio crollera', "
        .. "servira' un kart, e qualcuno che sappia guidarlo piu' in fretta di me.",
    inferno = "L'Ospite non teme le armi ne' il buio: teme il fuoco che non lo consuma. Ho chiuso "
        .. "questa chiave nel cuore della fortezza, dietro quattro bracieri spenti. Chi la porta "
        .. "cammina nella lava senza bruciare, e nel laboratorio e' nella lava che lui si ritira.",
    arena = "La base e' stato il mio primo tentativo di fermare cio' che esce dalla breccia. "
        .. "Sette ondate, e alla fine qualcosa di piu' grande. Le armi che si trovano li' sono "
        .. "le sole che abbiano mai scalfito il suo nucleo.",
    arpg = "Sotto le rovine c'e' una cripta che non ho costruito io. Il Custode che la abita e' "
        .. "nato dalla stessa breccia. Il gelo che lo rallenta rallenta anche l'Ospite: e' "
        .. "l'unico modo di stargli dietro quando prende l'ultima forma.",
    chambers = "Le camere di prova insegnano una cosa sola: cio' che entra da un portale esce "
        .. "dall'altro. L'Ospite si copre con sfere della sua stessa energia. Apri un varco "
        .. "davanti a loro e restituiscigliele.",
    backrooms = "Mi sono perso qui per giorni. L'Ospite viene da un posto come questo, dove la "
        .. "luce non arriva. Quando spegnera' le luci del laboratorio, segui il suono e non gli "
        .. "occhi. La torcia che mi ha riportato fuori e' rimasta all'uscita.",
    parkour = "Quando la breccia si e' allargata il pavimento del laboratorio e' caduto nel "
        .. "vuoto, e io con lui, quasi. Ho imparato a correre sui muri e ad appendermi alle "
        .. "travi. Tu dovrai fare lo stesso, senza il tempo di imparare.",
}

local ALL_SEVEN = "Le sette chiavi aprono il laboratorio. Io non tornero'. Chiudi la breccia."

for _, w in WORLDS do
    core.register_item(`keys:{w}`, 1, "", 0, 1.0, "")
    core.register_model(`keys:{w}_held`, `{w}_held.gltf`)   -- tools/make_held_models.py
end

local function item_of(w: string): string return `keys:{w}` end

-- A paragraph cut into lines for a panel: about 4.5 characters a cell.
local function wrap(text: string, width: number): { string }
    local lines, line = {}, ""
    for word in string.gmatch(text, "%S+") do
        if #line > 0 and #line + 1 + #word > width then
            table.insert(lines, line)
            line = word
        else
            line = if #line > 0 then line .. " " .. word else word
        end
    end
    if #line > 0 then table.insert(lines, line) end
    return lines
end

-- One panel per player, as mods/chess has them: a panel's contents are its mod's.
local function show_pages(player: number, heading: string, pages: { { string } })
    local name = `keys:p{player}`
    local rows = 1
    local blocks: { { string } } = {}
    for _, p in pages do
        local lines = wrap(p[2], 92)
        table.insert(blocks, lines)
        rows += 1 + #lines
    end
    core.ui_define(name, 24, 1.6 + rows * 0.62 + 1.6)
    core.ui_label(name, 0.5, 0.4, heading)
    local y = 1.3
    for i, p in pages do
        core.ui_label(name, 0.5, y, p[1])
        y += 0.7
        for _, l in blocks[i] do
            core.ui_label(name, 0.9, y, l)
            y += 0.62
        end
        y += 0.3
    end
    core.ui_button(name, "close", 9.5, y + 0.2, 5, 1.1, "Chiudi")
    core.ui_show(name, true, player)
end

core.event.on("core:ui_click", function(ev)
    if string.sub(ev.panel, 1, 6) == "keys:p" and ev.id == "close" then
        core.ui_show(ev.panel, false, ev.player)
    end
end)

-- The keys a player holds now.
local function held(player: number): { string }
    local got = {}
    for _, w in WORLDS do
        if core.bag_count(player, item_of(w)) > 0 then table.insert(got, w) end
    end
    return got
end

core.command("diario", "-- the engineer's pages of the keys you carry")
core.event.on("core:command", function(ev)
    if ev.name ~= "diario" then return end
    local pages = {}
    for _, w in held(ev.player) do table.insert(pages, { TITLE[w], PAGE[w] }) end
    if #pages == 0 then
        core.notice(ev.player, "Non porti nessuna chiave. Ce n'e' una in ogni mondo dietro i portali.")
        return
    end
    if #pages == 7 then table.insert(pages, { "L'ultima pagina", ALL_SEVEN }) end
    show_pages(ev.player, "Il diario dell'ingegnere", pages)
end)

-- Who has held which key, by identity (the name for somebody without one).
local function who(player: number): string
    local key = core.player_key(player)
    return if key ~= "" then key else "name:" .. core.player_name(player)
end

-- =================================================================== key altars

-- The level each player here came for by a survival site, and what is done
-- when it is done: both in "levels" below, the trial's handler asks first.
type Level = { site: string, d: number, room: string, slot: number, done: boolean? }
local level_of: { [number]: Level } = {}
local done: (number, Level) -> () = function(_, _) end

-- A trial does not hand its key over: it says it is passed with
--   core.event.emit("keys:earned", { player = p, key = "parkour" })
-- and the key waits on an altar beside the world's way out (every way out is
-- at the end of its trial): walking up to it puts it in the bag, once at a
-- time, for whoever has passed the trial here. Walking, not a click: in these
-- worlds the right button is a gun's or the rope's. Who passed is kept (storage
-- "won:<who>"), so leaving or dying before taking it loses nothing.
local ALTAR = core.register_node("keys:altar", 230, 190, 70, true, 10, "", 0, 0, "stone")
core.node_box(ALTAR, 0.15, 0, 0.15, 0.85, 0.9, 0.85)

local won: { [string]: { [string]: boolean } } = {}
local function passed(player: number): { [string]: boolean }
    local id = who(player)
    local got = won[id]
    if got == nil then
        got = {}
        for w in string.gmatch(core.storage_get("won:" .. id), "[^,]+") do got[w] = true end
        won[id] = got
    end
    return got
end

core.event.define("keys:earned", {
    fields = { player = "number", key = "string" },
    cancellable = false,
})
core.event.on("keys:earned", function(ev)
    -- By a survival site: the world's own trial stood in for its level.
    local lv = level_of[ev.player]
    if lv then
        done(ev.player, lv)
        if lv.d < 4 then return end
    end
    local got = passed(ev.player)
    if got[ev.key] then return end
    got[ev.key] = true
    local list = {}
    for w in got do table.insert(list, w) end
    core.storage_set("won:" .. who(ev.player), table.concat(list, ","))
    core.notice(ev.player, `Prova superata. La {TITLE[ev.key]} ti aspetta sul piedistallo accanto all'uscita.`)
end)

-- The laboratory says l'Ospite is beaten with
--   core.event.emit("keys:ending", { player = p })
-- and it is kept for every room (shared storage "ending:<who>"): the front
-- opens to that player in the sandbox (below).
core.event.define("keys:ending", {
    fields = { player = "number" },
    cancellable = false,
})
core.event.on("keys:ending", function(ev)
    core.shared_storage_set("ending:" .. who(ev.player), "1")
end)

-- Somebody come up to the altar: the key if the trial is passed, else said
-- what is missing.
local function at_altar(player: number)
    local any = false
    for w in passed(player) do
        any = true
        if core.bag_count(player, item_of(w)) == 0 then
            core.give_item(player, item_of(w), 1)
            core.notice(player, `Hai preso la {TITLE[w]}. /diario per leggere la pagina dell'ingegnere.`)
        end
    end
    if not any then
        core.notice(player, "Un incavo a forma di chiave, vuoto. La prova non e' ancora superata.")
    end
end
core.event.on("core:pre_break", function(ev)
    if ev.id == ALTAR then ev.cancel = true end
end)

-- ===================================================================== levels
--
-- Behind a survival site (mods/gates) a world builds a level of its own for
-- that site, at a difficulty d of 1 to 10. Who walked in carries the site
-- (core.player_carried "site=<id> d=<d> s=<seed> t=<room>"); here it gets a
-- SLOT, a square of the world 2048 across kept for that site (shared storage
-- "inst:<site>"), slot 0 being the world's own trial where it always was. Then
--   keys:level { player, slot, d, seed, ox, oz }   -- cancellable
-- asks the world to build the slot's level from (ox, oz) and take the player
-- there: a world that does cancels it, and answers
--   keys:start { player, slot, ex, ey, ez }
-- with its way out: a hole one wide and two high at (ex, ey, ez), facing +x,
-- 8 to 20 from where the level starts and hidden -- the two cells at ex + 1
-- are the level's own wall, and the cell over them a dark lamp (keys:hint),
-- the one sign of it. A world that does not answer keeps the player in its
-- own trial, which counts the same.
--   keys:cleared { player, slot }
-- says the level is done: the wall opens, the lamp lights, the way out (this
-- world's first mouth back to the sandbox, moved there) unseals for that
-- player, a reward waits for them at their site (shared "reward:<who>",
-- mods/gates hands it over), and the world's key is theirs if d >= 4 -- or
-- whatever d in the world's own trial reached from the plaza.

local HINT = core.register_node("keys:hint", 70, 74, 66, true, 0, "", 0, 0, "stone")
local HINT_LIT = core.register_node("keys:hint_lit", 120, 230, 140, true, 12, "", 0, 0, "stone")
core.event.define("keys:level", {
    fields = { player = "number", slot = "number", d = "number", seed = "number", ox = "number", oz = "number" },
    cancellable = true,
})
core.event.define("keys:start", {
    fields = { player = "number", slot = "number", ex = "number", ey = "number", ez = "number" },
    cancellable = false,
})
core.event.define("keys:cleared", { fields = { player = "number", slot = "number" }, cancellable = false })

local exit_of: { [number]: { number } } = {}   -- each slot's way out, once its world said

local function cleared(site: string, player: number): boolean
    return core.shared_storage_get(`cleared:{site}:{who(player)}`) ~= ""
end

-- A slot's way out, said by its world (and kept, for after a restart).
local function exit_at(slot: number): { number }?
    local e = exit_of[slot]
    if e == nil then
        local x, y, z = string.match(core.storage_get(`exit:{slot}`), "(-?%d+),(-?%d+),(-?%d+)")
        if x then
            e = { tonumber(x) or 0, tonumber(y) or 0, tonumber(z) or 0 }
            exit_of[slot] = e
        end
    end
    return e
end

-- Somebody arrived: a slot for the site they came by, and its world asked.
local function arrived(player: number)
    local line = core.player_carried(player)
    local site = string.match(line, "site=([^ ]+)")
    local d = tonumber(string.match(line, "d=(%d+)") or "")
    local seed = tonumber(string.match(line, "s=(%d+)") or "") or 0
    local room = string.match(line, "t=([^ ]+)") or ""
    if site == nil or d == nil then
        level_of[player] = nil
        return
    end
    local slot = tonumber(core.shared_storage_get("inst:" .. site))
    if slot == nil then
        slot = (tonumber(core.shared_storage_get("inst:next:" .. room)) or 1)
        core.shared_storage_set("inst:next:" .. room, tostring(slot + 1))
        core.shared_storage_set("inst:" .. site, tostring(slot))
    end
    level_of[player] = { site = site, d = d, room = room, slot = slot }
    local ox, oz = 8192 + (slot % 16) * 2048, (slot // 16) * 2048
    local taken = core.event.emit("keys:level",
        { player = player, slot = slot, d = d, seed = seed, ox = ox, oz = oz })
    core.log(`keys: {core.player_name(player)} by {site} (d {d}): slot {slot}, `
        .. (if taken then "its level" else "the world's own trial"))
end

core.event.on("keys:start", function(ev)
    exit_of[ev.slot] = { ev.ex, ev.ey, ev.ez }
    core.storage_set(`exit:{ev.slot}`, `{ev.ex},{ev.ey},{ev.ez}`)
    -- Hidden: the level's wall stands in front of it, the dark lamp over --
    -- lit, and the wall gone, once anybody has opened it.
    local open = core.storage_get(`opened:{ev.slot}`) == "1"
    core.set_block(ev.ex + 1, ev.ey + 2, ev.ez, if open then HINT_LIT else HINT)
    if open then
        core.set_block(ev.ex + 1, ev.ey, ev.ez, 0)
        core.set_block(ev.ex + 1, ev.ey + 1, ev.ez, 0)
    end
end)

done = function(player: number, lv: Level)
    if lv.done then return end   -- a trial may say it twice
    lv.done = true
    local first = not cleared(lv.site, player)
    core.log(`keys: {core.player_name(player)} has done {lv.site} (d {lv.d}, slot {lv.slot})`)
    core.shared_storage_set(`cleared:{lv.site}:{who(player)}`, "1")
    local pending = core.shared_storage_get("reward:" .. who(player))
    core.shared_storage_set("reward:" .. who(player),
        pending .. `{lv.room} {lv.d} {if first then 1 else 0} {lv.site};`)
    local e = exit_at(lv.slot)
    if e then
        core.set_block(e[1] + 1, e[2], e[3], 0)
        core.set_block(e[1] + 1, e[2] + 1, e[3], 0)
        core.set_block(e[1] + 1, e[2] + 2, e[3], HINT_LIT)
        core.storage_set(`opened:{lv.slot}`, "1")
        core.play_sound("keys:rumble", e[1] + 0.5, e[2] + 1, e[3] + 0.5)
    end
    local own = if TITLE[lv.room] then lv.room else ""
    if lv.d >= 4 and own ~= "" and core.bag_count(player, item_of(own)) == 0 then
        core.give_item(player, item_of(own), 1)
        core.notice(player, `Livello superato. Hai la {TITLE[own]}; la ricompensa ti aspetta al portale.`)
    elseif lv.d < 4 then
        core.notice(player, "Livello superato. La ricompensa ti aspetta al portale; le chiavi sono "
            .. "nei livelli di difficolta' 4 e oltre.")
    else
        core.notice(player, "Livello superato. La ricompensa ti aspetta al portale.")
    end
    core.notice(player, "Qualcosa si e' aperto vicino all'ingresso.")
end

core.event.on("keys:cleared", function(ev)
    local lv = level_of[ev.player]
    if lv and lv.slot == ev.slot then done(ev.player, lv) end
end)

-- =================================================================== ways out
--
-- Every world's way home at the end of it is not its way in (asked for,
-- 2026-10-10): in a themed world, an opening past the first that leads to the
-- sandbox is its way out, and it comes out at that world's arch, a row of them
-- on the plaza's north edge -- a mouth of its own (--portal-to @pista/3 ...),
-- so the arch looks into the world's way out and leads there too.
--
-- Both ends are sealed in red to whoever has not won that world (asked for,
-- 2026-10-10; core.portal_seal, per player): the way out until its trial is
-- passed here, the arch until the sandbox has seen its key in their bag. The
-- membrane parts for them alone.

local HUB = false     -- the sandbox: leads to other rooms, never to "sandbox"
local EXITS: { number } = {}
for k = 0, 15 do
    local r = core.portal_room(k)
    if r == "sandbox" then
        if k > 0 then table.insert(EXITS, k) end
    elseif r ~= "" then
        HUB = true
    end
end
if #EXITS > 0 then HUB = false end

if not HUB and #EXITS > 0 then
    local by_altar: { [number]: boolean } = {}
    local here: { [number]: boolean } = {}
    local home: { number }? = nil   -- where the way out stands for the world's own trial
    local out_at = ""               -- where it stands now
    core.event.on("core:tick", function(_)
        -- Who came in since the last tick, and by which site.
        local now: { [number]: boolean } = {}
        for _, p in core.players() do
            now[p] = true
            if not here[p] then arrived(p) end
        end
        for p in here do
            if not now[p] then level_of[p] = nil end
        end
        here = now
        -- Not in the laboratory, whose way out ends the game, not a trial;
        -- nor in the assault, whose way out is its own (assault_game).
        if core.node_id("lab:seal") ~= 0 or core.node_id("assault_core:seal") ~= 0 then return end
        -- The way out goes to the hidden niche of the level of whoever is
        -- nearest one, or back where the world's own trial has it.
        local k = EXITS[1]
        if home == nil then
            local h = { core.get_portal(k, 0), core.get_portal(k, 1), core.get_portal(k, 2) }
            home, out_at = h, `{h[1]},{h[2]},{h[3]}`
        end
        local want, best = home :: { number }, math.huge
        for _, p in core.players() do
            local px, _, pz = core.player_pos(p)
            local lv = level_of[p]
            local e = if lv and lv.slot > 0 then exit_at(lv.slot) else nil
            if px and e then
                local dd = (px - e[1]) ^ 2 + (pz - e[3]) ^ 2
                if dd < best then want, best = { e[1] + 0.5, e[2] + 1, e[3] + 0.5 }, dd end
            end
        end
        local key = `{want[1]},{want[2]},{want[3]}`
        if key ~= out_at and core.portal_place(k, want[1], want[2], want[3], 1, 1, 0.5, 1.0) == 1 then
            out_at = key
        end
        for _, p in core.players() do
            local x, y, z = core.player_pos(p)
            if x == nil then continue end
            local lv = level_of[p]
            local won = if lv then cleared(lv.site, p) else next(passed(p)) ~= nil
            for _, k in EXITS do core.portal_seal(p, k, not won) end
            -- The altar, one forward and two to the side of each way out (all
            -- face +x), put back while anybody is near: a world's own stages
            -- write over what is set before its columns are made.
            local by = false
            for i, k in EXITS do
                -- Where the way out stands for the world's own trial, even
                -- while the first one is at a level's niche.
                local hx = if i == 1 and home then home[1] else core.get_portal(k, 0)
                local hy = if i == 1 and home then home[2] else core.get_portal(k, 1)
                local hz = if i == 1 and home then home[3] else core.get_portal(k, 2)
                local ax = math.floor(hx) + 1
                local ay = math.floor(hy) - 1
                local az = math.floor(hz) + 2
                if (x - ax) ^ 2 + (z - az) ^ 2 < 40 * 40 and core.get_block(ax, ay, az) ~= ALTAR then
                    core.set_block(ax, ay, az, ALTAR)
                end
                if (x - ax - 0.5) ^ 2 + (z - az - 0.5) ^ 2 < 1.8 * 1.8 and math.abs(y - ay) < 1.5 then by = true end
            end
            if by and not by_altar[p] then at_altar(p) end
            by_altar[p] = by
        end
    end)
end

-- The sandbox's arches: mouth k past the gates (8..15) and the room it leads
-- to, placed facing the plaza once the world runs.
local ARCHES: { { k: number, room: string } } = {}

if HUB then
    -- One spot per world the gates lead to, in a row along the plaza's north
    -- edge (z 96; the plaza is 20..100 both ways), in the gates' order: the
    -- same spots as before the arches were mouths, so a world built then keeps
    -- its frames. Built every start, as mods/gates is.
    local EXIT_FRAME = core.register_node("keys:exit_frame", 200, 204, 212, true, 0, "", 0, 0, "stone")
    local EXIT_LAMP = core.register_node("keys:exit_lamp", 120, 230, 140, true, 12, "", 0, 0, "stone")
    local SPOT: { [string]: number } = {}
    local n = 0
    for k = 0, 7 do
        local r = core.portal_room(k)
        if r ~= "" and SPOT[r] == nil then
            SPOT[r] = 24 + n * 8
            n += 1
        end
    end
    for k = 8, 15 do
        local r = core.portal_room(k)
        local x = SPOT[r]
        if x == nil then continue end
        table.insert(ARCHES, { k = k, room = r })
        -- No plaza (survival): mods/gates puts each arch beside its world's site.
        if core.plaza() == nil then continue end
        local y, z = 21, 96
        -- The frame round a hole one wide and two high, the opening's size at
        -- the far end; a floor out in front of it.
        for dy = 0, 3 do
            for dx = -2, 2 do
                local hole = dx == 0 and dy <= 1
                core.set_block(x + dx, y + dy, z, if hole then 0 else EXIT_FRAME)
            end
        end
        for dx = -2, 2 do core.set_block(x + dx, y + 4, z, EXIT_FRAME) end
        core.set_block(x, y + 4, z, EXIT_LAMP)
        for dx = -1, 1 do
            for dz = 0, 3 do core.set_block(x + dx, y - 1, z - dz, EXIT_FRAME) end
            for dy = 0, 3 do
                for dz = 1, 3 do core.set_block(x + dx, y + dy, z - dz, 0) end
            end
        end
        core.set_portal(k, x + 0.5, y + 1, z + 0.5)
    end

    -- Turned to face the plaza (-z) once the openings exist: set_portal only
    -- moves a centre. Asked again until it takes.
    local placed = if core.plaza() == nil then #ARCHES else 0
    core.event.on("core:tick", function(_)
        if placed >= #ARCHES then return end
        placed = 0
        for _, a in ARCHES do
            local x, y, z = core.get_portal(a.k, 0), core.get_portal(a.k, 1), core.get_portal(a.k, 2)
            placed += core.portal_place(a.k, x, y, z, 0, -1, 0.5, 1.0)
        end
    end)
end

-- What was won behind the sites, handed to mods/gates (which knows where the
-- sites are) as a player comes back into the sandbox:
--   keys:reward { player, list = "room d first site;..." }
core.event.define("keys:reward", { fields = { player = "number", list = "string" }, cancellable = false })
if HUB then
    local seen: { [number]: boolean } = {}
    core.event.on("core:tick", function(_)
        local now: { [number]: boolean } = {}
        for _, p in core.players() do
            now[p] = true
            if not seen[p] then
                local pending = core.shared_storage_get("reward:" .. who(p))
                if pending ~= "" then
                    core.shared_storage_set("reward:" .. who(p), "")
                    core.event.emit("keys:reward", { player = p, list = pending })
                end
            end
        end
        seen = now
    end)
end

-- ==================================================================== sandbox

local LAB = -1
for k = 0, 7 do
    if core.portal_room(k) == "laboratorio" then LAB = k end
end
if LAB < 0 then return end

local PEDESTAL = core.register_node("keys:pedestal", 70, 72, 78, true, 3, "", 0, 0, "stone")
core.register_node("keys:frame", 38, 40, 46, true, 0, "", 0, 0, "stone")
core.node_box(PEDESTAL, 0.15, 0, 0.15, 0.85, 0.9, 0.85)

local function record(player: number): { [string]: boolean }
    -- Read each time and not kept: the keys seen are kept once for every
    -- room (core.shared_storage_*), and a copy here would write back over
    -- the key taken in another world.
    local got = {}
    for w in string.gmatch(core.shared_storage_get("got:" .. who(player)), "[^,]+") do got[w] = true end
    return got
end

local function count(got: { [string]: boolean }): number
    local n = 0
    for _, w in WORLDS do if got[w] then n += 1 end end
    return n
end

-- A key seen in a bag is kept for good; all seven the first time is said.
local function update(player: number)
    local got = record(player)
    local before = count(got)
    local new = false
    for _, w in held(player) do
        if not got[w] then
            got[w] = true
            new = true
        end
    end
    if not new then return end
    local list = {}
    for _, w in WORLDS do if got[w] then table.insert(list, w) end end
    core.shared_storage_set("got:" .. who(player), table.concat(list, ","))
    local n = count(got)
    if n == 7 and before < 7 then
        core.notice(player, "Le sette chiavi vibrano insieme. Il portale del laboratorio si e' acceso.")
    elseif n > before then
        core.notice(player, `Un incavo del piedistallo si illumina: {n} chiavi su 7.`)
    end
end

core.event.on("core:used", function(ev)
    if ev.id ~= PEDESTAL then return end
    ev.cancel = true
    update(ev.player)
    local got = record(ev.player)
    local pages = {}
    for _, w in WORLDS do
        table.insert(pages, { TITLE[w] .. (if got[w] then "" else "  -- manca"),
                              if got[w] then PAGE[w] else "Un incavo vuoto nella pietra." })
    end
    if count(got) == 7 then table.insert(pages, { "L'ultima pagina", ALL_SEVEN }) end
    show_pages(ev.player, `Il portale del laboratorio: {count(got)} chiavi su 7`, pages)
end)

-- The front (games/front) and the assault (games/assault) open once l'Ospite
-- is beaten. FRONT_OPEN keeps both open to everybody while they are tested;
-- false is the game as it ships.
local FRONT_OPEN = true
local FRONT, ASSAULT = -1, -1
for k = 0, 19 do
    if core.portal_room(k) == "front" then FRONT = k end
    if core.portal_room(k) == "assalto" then ASSAULT = k end
end

-- The lock. A body within reach of the opening without all seven is put back
-- out in front of it, facing it; told at most every three seconds.
local told: { [number]: number } = {}
local clock = 0
local since = 0
core.event.on("core:tick", function(ev)
    clock += ev.dt
    since += ev.dt
    local scan = since >= 1
    if scan then since = 0 end
    local ox, oy, oz = core.get_portal(LAB, 0), core.get_portal(LAB, 1), core.get_portal(LAB, 2)
    for _, p in core.players() do
        if scan then update(p) end
        for _, a in ARCHES do core.portal_seal(p, a.k, not record(p)[a.room]) end
        for _, k in { FRONT, ASSAULT } do
            if k >= 0 then
                core.portal_seal(p, k, not FRONT_OPEN and core.shared_storage_get("ending:" .. who(p)) ~= "1")
            end
        end
        local x, y, z = core.player_pos(p)
        if x == nil then continue end
        if math.abs(x - ox) < 2.6 and math.abs(z - oz) < 1.6 and math.abs(y - oy) < 3
            and count(record(p)) < 7 then
            -- The gate faces +x (mods/gates turns every opening past the first that way).
            core.player_place(p, ox + 4, y, z, -1, 0, 0)
            if (told[p] or -10) + 3 < clock then
                told[p] = clock
                core.notice(p, `Il portale e' spento. Servono le sette chiavi: ne hai portate {count(record(p))}.`)
            end
        end
    end
end)
