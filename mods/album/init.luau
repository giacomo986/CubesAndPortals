--!strict
-- album: a camera that takes photographs, frames to hang them, an album.
--
-- Hold the right button with the camera in the hand and you look through
-- it: the view narrows to the picture's (core.zoom) and the viewfinder
-- shows its square; a left click takes the photograph -- as many as you
-- like while you look -- and letting go puts it down. The picture is an INSTANT: the player's own machine
-- draws it at once from the eye (core.render.camera, still), reads its
-- pixels (core.render.camera_read, 64 by 64) and sends them to the server,
-- which keeps them; everybody else's machine asks for them and shows them
-- as they were (core.render.photo). A photograph the server has no pixels
-- for (taken before, or lost on the way) is drawn as before, from the place
-- it was taken.
--
-- It goes in your album (/album), forty at most, the oldest let go. A FRAME
-- put on a wall asks which of your photographs goes in it, and then
-- everybody near sees it there (core.render.screen, plain).
--
-- Taken somewhere rare -- on a peak, in the Hollows, by the relics, over the
-- clouds -- a photograph is worth something to a village's museum: the album
-- offers it to the museum of the village you stand in (the event
-- museum:photo, mods/villagers), which pays for the first of each.
--
-- Pictures: tools/make_album_assets.py; the camera in the hand
-- tools/make_held_models.py.
--
-- Over the mod's channel, both ways:
--   to the server   "shoot"                  the shutter pressed
--                   "P id k n data"          piece k of n of photograph id's pixels
--                   "G id"                   which pixels has it of id?
--   to a machine    "H frames"               the frames near and their photographs
--                   "T id"                   your last shot is photograph id: send it
--                   "Q id k n data"          piece k of n of id's pixels
--                   "N id"                   no pixels for id: draw it from its place

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local FOV = 60       -- the photograph's, degrees: the viewfinder narrows the engine's 70 to it
local PIECE = 480    -- base64 characters a message carries (512 bytes at most)
local KEEP = 4000    -- and a storage value (4096 at most)
local CAMERA = "album:camera"

-- The two ends, wherever they are: a process that serves and draws hands the
-- text straight over.
local server_heard: (number, string) -> () = function(_, _) end
local machine_heard: (string) -> () = function(_) end
local function to_server(p: number, text: string)
    if serving then
        server_heard(p, text)
    else
        core.net.send(0, text)
    end
end
local function to_machine(p: number, text: string)
    if drawing then
        machine_heard(text)
    else
        core.net.send(p, text)
    end
end

-- Pixels in pieces a message carries.
local function pieces(data: string, size: number): { string }
    local out = {}
    for i = 1, #data, size do
        table.insert(out, string.sub(data, i, i + size - 1))
    end
    return out
end

local FRONT = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }   -- an upright block's front: +Z, +X, -Z, -X
local SHOWN = 48
local RARE = { peak = "a peak", hollows = "the Hollows", relic = "a relic", clouds = "the clouds" }

-- What this machine draws: each frame near, and its photograph's place.
type Hung = { x: number, y: number, z: number, f: number, id: number,
              px: number, py: number, pz: number, yaw: number, pitch: number }
local hung: { Hung } = {}
local made: { [number]: boolean } = {}    -- photographs made into a camera here (drawn from their place)
local pics: { [number]: boolean } = {}    -- and those shown from their pixels
local asked: { [number]: boolean } = {}   -- asked the server for
local coming: { [number]: { string } } = {}   -- pixels arriving, by piece

-- Shots waiting for the server's number, and shots numbered waiting for
-- their picture to be drawn: a camera of their own each, from TEMP on.
local TEMP = 1000000000
local temp_next = TEMP
local unnumbered: { { number } } = {}   -- { temp camera, seconds waited }
type Shot = { temp: number, id: number, age: number }
local numbered: { Shot } = {}

local function heard(text: string)
    local word = string.match(text, "^(%a) ")
    if word == "T" then
        local id = tonumber(string.match(text, "^T (%d+)$"))
        local first = table.remove(unnumbered, 1)
        if first == nil then
            return
        end
        local temp = first[1]
        if id and id > 0 then
            table.insert(numbered, { temp = temp, id = id, age = 0 })
        else
            core.render.camera_drop(temp)   -- refused: the server said why
        end
        return
    elseif word == "Q" then
        local id, k, n, data = string.match(text, "^Q (%d+) (%d+) (%d+) (%S+)$")
        local i, kk, nn = tonumber(id), tonumber(k), tonumber(n)
        if i == nil or kk == nil or nn == nil or data == nil or nn > 64 then
            return
        end
        local got = coming[i] or {}
        coming[i] = got
        got[kk] = data
        for j = 1, nn do
            if got[j] == nil then
                return
            end
        end
        coming[i] = nil
        if core.render.photo(i, table.concat(got)) then
            if made[i] then
                made[i] = nil
            end
            pics[i] = true
        end
        return
    elseif word == "N" then
        local id = tonumber(string.match(text, "^N (%d+)$"))
        for _, h in hung do
            if id and h.id == id and not made[id] and not pics[id] then
                core.render.camera(h.id, h.px, h.py, h.pz, h.yaw, h.pitch, FOV, true)
                made[id] = true
            end
        end
        return
    elseif word ~= "H" then
        return
    end
    local now: { Hung } = {}
    for part in string.gmatch(string.sub(text, 3), "[^;]+") do
        local n = {}
        for v in string.gmatch(part, "[^,]+") do
            table.insert(n, tonumber(v) or 0)
        end
        if #n >= 10 then
            table.insert(now, { x = n[1], y = n[2], z = n[3], f = n[4], id = n[5],
                                px = n[6], py = n[7], pz = n[8], yaw = n[9], pitch = n[10] })
        end
    end
    local wanted: { [number]: boolean } = {}
    hung = now
    for _, h in now do
        wanted[h.id] = true
        if not made[h.id] and not pics[h.id] and not asked[h.id] then
            asked[h.id] = true
            to_server(core.players()[1] or 0, `G {h.id}`)
        end
    end
    for _, t in { made, pics, asked } do
        for id in t do
            if not wanted[id] then
                if made[id] or pics[id] then
                    core.render.camera_drop(id)
                end
                t[id] = nil
            end
        end
    end
end
machine_heard = heard

if serving then
    core.net.open()
    core.register_item(CAMERA, 1)
    core.register_model("album:camera_held", "camera_held.gltf")   -- tools/make_held_models.py
    core.recipe_shaped("album:camera", 1,
        "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot",
        "furnace:iron_ingot core:glass         ore:lapis",
        "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot")
    local FRAME = core.register_node("album:frame", 150, 110, 60, false, 0, "upright", 0, 0.5, "wood")
    core.node_box(FRAME, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0625)
    core.recipe_shaped("album:frame", 1,
        "timber:stick timber:stick timber:stick",
        "timber:stick cane:paper   timber:stick",
        "timber:stick timber:stick timber:stick")

    local function who(p: number): string
        local k = core.player_key(p)
        return string.sub(if k ~= "" then k else "name:" .. string.lower(core.player_name(p)), 1, 48)
    end

    type Photo = { x: number, y: number, z: number, yaw: number, pitch: number, tag: string, by: string }
    local function photo(id: number): Photo?
        local x, y, z, yaw, pitch, tag, by =
            string.match(core.storage_get(`photo {id}`), "^(%S+),(%S+),(%S+),(%S+),(%S+),(%a*),(.*)$")
        if x == nil then
            return nil
        end
        return { x = tonumber(x) :: number, y = tonumber(y) :: number, z = tonumber(z) :: number,
                 yaw = tonumber(yaw) :: number, pitch = tonumber(pitch) :: number, tag = tag :: string,
                 by = by :: string }
    end

    local function album_of(p: number): { number }
        local out = {}
        for id in string.gmatch(core.storage_get("album " .. who(p)), "%d+") do
            table.insert(out, tonumber(id) :: number)
        end
        return out
    end

    -- Where it was taken, if it is somewhere a museum wants.
    local function rare_at(x: number, y: number, z: number): string
        if y < 0 then
            return "hollows"
        elseif y > 115 then
            return "clouds"
        elseif #core.find_nodes(math.floor(x) - 10, math.floor(y) - 6, math.floor(z) - 10,
                                math.floor(x) + 10, math.floor(y) + 6, math.floor(z) + 10, "relics:*") > 0 then
            return "relic"
        elseif core.biome(math.floor(x), math.floor(z)) == "core:peaks" and y > 60 then
            return "peak"
        end
        return ""
    end

    local function shoot(p: number): number?
        local x, y, z = core.player_pos(p)
        local lx, ly, lz = core.player_look(p)
        if x == nil or lx == nil then
            return nil
        end
        local id = (tonumber(core.storage_get("photo next")) or 0) + 1
        core.storage_set("photo next", tostring(id))
        local ey = y + 1.62
        local yaw, pitch = math.atan2(lx, -lz), math.asin(math.clamp(ly, -1, 1))
        local tag = rare_at(x, ey, z)
        core.storage_set(`photo {id}`, string.format("%.2f,%.2f,%.2f,%.4f,%.4f,%s,%s", x, ey, z, yaw, pitch, tag,
                                                      core.player_name(p)))
        local list = album_of(p)
        table.insert(list, id)
        while #list > 40 do
            table.remove(list, 1)
        end
        local ids = {}
        for _, i in list do
            table.insert(ids, tostring(i))
        end
        core.storage_set("album " .. who(p), table.concat(ids, ","))
        core.play_sound("album:shutter", x, ey, z)
        core.notice(p, if tag ~= "" then `Photograph #{id}: {RARE[tag]} -- a museum would want it` else `Photograph #{id} taken`)
        return id
    end

    -- A photograph's pixels: kept in pieces a storage value holds.
    local function pixels_of(id: number): string?
        local n = tonumber(core.storage_get(`pic {id}`))
        if n == nil then
            return nil
        end
        local out = {}
        for k = 1, n do
            table.insert(out, core.storage_get(`pic {id} {k}`))
        end
        return table.concat(out)
    end
    local function keep_pixels(id: number, data: string)
        local parts = pieces(data, KEEP)
        for k, part in parts do
            core.storage_set(`pic {id} {k}`, part)
        end
        core.storage_set(`pic {id}`, tostring(#parts))
    end

    -- Uploads expected: the photograph's number, from whom, what has come.
    type Upload = { p: number, got: { [number]: string }, age: number }
    local uploads: { [number]: Upload } = {}

    server_heard = function(p: number, text: string)
        if text == "shoot" then
            if core.held_item(p) ~= CAMERA then
                core.log(`album: a shot from {core.player_name(p)} refused: holding '{core.held_item(p)}'`)
                core.notice(p, "Hold the camera in your hand to take a photograph")
                to_machine(p, "T 0")
                return
            end
            local id = shoot(p)
            core.log(`album: {core.player_name(p)} took photograph #{id or 0}`)
            if id == nil then
                to_machine(p, "T 0")
            end
            if id then
                uploads[id] = { p = p, got = {}, age = 0 }
                to_machine(p, `T {id}`)
            end
            return
        end
        local id, k, n, data = string.match(text, "^P (%d+) (%d+) (%d+) (%S+)$")
        if id then
            local i, kk, nn = tonumber(id) :: number, tonumber(k) :: number, tonumber(n) :: number
            local u = uploads[i]
            if u == nil or u.p ~= p or nn > 32 or kk < 1 or kk > nn or #data > PIECE then
                return
            end
            u.got[kk] = data
            for j = 1, nn do
                if u.got[j] == nil then
                    return
                end
            end
            uploads[i] = nil
            keep_pixels(i, table.concat(u.got))
            return
        end
        local g = tonumber(string.match(text, "^G (%d+)$"))
        if g then
            local data = pixels_of(g)
            if data == nil then
                to_machine(p, `N {g}`)
                return
            end
            local parts = pieces(data, PIECE)
            for kk, part in parts do
                to_machine(p, `Q {g} {kk} {#parts} {part}`)
            end
        end
    end
    core.event.on("core:net_message", function(ev)
        server_heard(ev.player, ev.text)
    end)
    -- An upload never finished is forgotten after a minute.
    core.event.on("core:tick", function(ev)
        for id, u in uploads do
            u.age += ev.dt
            if u.age > 60 then
                uploads[id] = nil
            end
        end
    end)

    -- The album, and for a frame just put down which photograph goes in it.
    local framing: { [number]: { number } } = {}
    local function show_album(p: number, told: string?)
        local page = { "# Album", "" }
        if framing[p] then
            table.insert(page, "> Choose the photograph for the frame.")
            table.insert(page, "")
        end
        if told then
            table.insert(page, `> {told}`)
            table.insert(page, "")
        end
        local list = album_of(p)
        if #list == 0 then
            table.insert(page, "No photographs yet: hold the right button with the camera in your hand to aim, left click to take one.")
        end
        for i = #list, 1, -1 do
            local id = list[i]
            local ph = photo(id)
            if ph then
                local line = `- Photograph #{id}, at {math.floor(ph.x)} {math.floor(ph.y)} {math.floor(ph.z)}`
                if ph.tag ~= "" then
                    line ..= `, {RARE[ph.tag] or ph.tag} [Offer to the museum](act:offer {id})`
                end
                if framing[p] then
                    line ..= ` [Hang it](act:hang {id})`
                end
                table.insert(page, line)
            end
        end
        core.page_show(p, "album", table.concat(page, "\n"))
    end

    core.page_register("album", "Album")
    core.command("album", "-- your photographs")
    core.event.on("core:command", function(ev)
        if ev.name == "album" then
            framing[ev.player] = nil
            show_album(ev.player)
        end
    end)
    core.event.on("core:page_open", function(ev)
        if ev.page == "album" then
            show_album(ev.player)
        end
    end)
    core.event.on("core:placed", function(ev)
        if ev.id == FRAME then
            framing[ev.player] = { ev.x, ev.y, ev.z }
            show_album(ev.player)
        end
    end)
    core.event.on("core:used", function(ev)
        if ev.id == FRAME and ev.held ~= CAMERA then
            framing[ev.player] = { ev.x, ev.y, ev.z }
            show_album(ev.player)
            ev.cancel = true
        end
    end)
    core.event.on("core:page_action", function(ev)
        if ev.page ~= "album" then
            return
        end
        local p = ev.player
        local what, n = string.match(ev.id, "^(%a+) (%d+)$")
        local id = tonumber(n)
        local mine = false
        for _, i in album_of(p) do
            mine = mine or i == id
        end
        if id == nil or not mine then
            return
        end
        if what == "hang" and framing[p] then
            local f = framing[p]
            if core.get_block(f[1], f[2], f[3]) == FRAME then
                core.meta_set(f[1], f[2], f[3], "photo", tostring(id))
                framing[p] = nil
                show_album(p, `Photograph #{id} hangs in the frame.`)
            end
        elseif what == "offer" then
            local ph = photo(id :: number)
            if ph and ph.tag ~= "" then
                local ok = pcall(core.event.emit, "museum:photo", { player = p, tag = ph.tag, id = id :: number })
                if not ok then
                    show_album(p, "There is no museum in this world.")
                end
            end
        end
    end)

    -- Frames near each player, and their photographs, to that player's machine.
    local frames: { [string]: { number } } = {}
    core.register_lbm("album:frames", { "album:frame" }, function(cell)
        frames[`{cell.x},{cell.y},{cell.z}`] = { cell.x, cell.y, cell.z }
    end)
    core.event.on("core:placed", function(ev)
        if ev.id == FRAME then
            frames[`{ev.x},{ev.y},{ev.z}`] = { ev.x, ev.y, ev.z }
        end
    end)
    core.event.on("core:broken", function(ev)
        if ev.id == FRAME then
            frames[`{ev.x},{ev.y},{ev.z}`] = nil
            core.meta_set(ev.x, ev.y, ev.z, "photo", "")
        end
    end)
    local told: { [number]: string } = {}
    local since = 0.0
    core.event.on("core:tick", function(ev)
        since += ev.dt
        if since < 1 then
            return
        end
        since = 0
        local now: { [number]: string } = {}
        for _, p in core.players() do
            local px, py, pz = core.player_pos(p)
            if px and py and pz then
                local parts = {}
                for k, f in frames do
                    if core.get_block(f[1], f[2], f[3]) ~= FRAME then
                        frames[k] = nil
                    elseif (f[1] - px) ^ 2 + (f[2] - py) ^ 2 + (f[3] - pz) ^ 2 < (SHOWN + 8) ^ 2 then
                        local id = tonumber(core.meta_get(f[1], f[2], f[3], "photo"))
                        local ph = if id then photo(id) else nil
                        if ph then
                            table.insert(parts, string.format("%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.4f,%.4f", f[1], f[2],
                                f[3], core.get_facing(f[1], f[2], f[3]), id, ph.x, ph.y, ph.z, ph.yaw, ph.pitch))
                        end
                    end
                end
                table.sort(parts)
                local text = "H " .. table.concat(parts, ";")
                if told[p] ~= text then
                    if drawing then heard(text) else core.net.send(p, text) end
                end
                now[p] = text
            end
        end
        told = now
    end)
end

if drawing then
    core.event.on("core:net_message", function(ev)
        heard(ev.text)
    end)

    -- Through the viewfinder: the right button held with the camera in the
    -- hand, the view eased to the picture's; the left button, the shot.
    local AIMED = FOV / 70
    local aiming = false
    local zoom = 1.0

    local function take(p: number)
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or y == nil or z == nil or dx == nil or dy == nil or dz == nil then
            return
        end
        temp_next += 1
        print(`album: shot {temp_next - TEMP} sent to the server`)
        core.render.camera(temp_next, x, y + 1.62, z, math.atan2(dx, -dz), math.asin(math.clamp(dy, -1, 1)), FOV, true)
        table.insert(unnumbered, { temp_next, 0 })
        to_server(p, "shoot")
    end

    core.event.on("core:input", function(ev)
        local holding = core.held_item(ev.player) == CAMERA
        if ev.action == "mouse_right" and holding then
            ev.cancel = true   -- nothing placed, nothing used: it is the camera's
            aiming = true
        elseif ev.action == "mouse_right_up" then
            aiming = false     -- the camera down, nothing taken
        elseif ev.action == "mouse_left" and aiming then
            ev.cancel = true   -- nothing dug, nothing struck: the shutter
            if holding then
                take(ev.player)
            end
        end
    end)

    core.event.on("core:tick", function(ev)
        local me = core.players()[1]
        if aiming and (me == nil or core.held_item(me) ~= CAMERA) then
            aiming = false
        end
        local want = if aiming then AIMED else 1.0
        if math.abs(zoom - want) > 0.001 then
            zoom += (want - zoom) * math.min(1, ev.dt * 12)
            if math.abs(zoom - want) < 0.002 then
                zoom = want
            end
            core.zoom(zoom)
        end
        -- A shot the server never answered: said, so it is not a mystery.
        local first = unnumbered[1]
        if first then
            first[2] += ev.dt
            if first[2] > 4 then
                table.remove(unnumbered, 1)
                core.render.camera_drop(first[1])
                core.notice(me or 0, "The photograph did not reach the server: try again")
                print("album: a shot got no answer from the server")
            end
        end
        -- Each shot numbered, once its camera has drawn it: shown here at
        -- once, and its pixels to the server.
        for i = #numbered, 1, -1 do
            local shot = numbered[i]
            shot.age += ev.dt
            local data = core.render.camera_read(shot.temp)
            if data ~= "" or shot.age > 10 then
                core.render.camera_drop(shot.temp)
                table.remove(numbered, i)
                if data ~= "" then
                    local parts = pieces(data, PIECE)
                    for k, part in parts do
                        to_server(me or 0, `P {shot.id} {k} {#parts} {part}`)
                    end
                    if core.render.photo(shot.id, data) then
                        pics[shot.id] = true
                    end
                end
            end
        end
    end)

    -- The viewfinder: the square the photograph takes, the rest darkened,
    -- its corners marked, and what the buttons do.
    core.hud.pass("album:viewfinder", 60, function()
        local k = math.clamp((1 - zoom) / (1 - AIMED), 0, 1)
        if k < 0.02 then
            return
        end
        local aspect = core.hud.aspect()
        if aspect <= 0 then
            return
        end
        local half = 0.5 / aspect                 -- half the square, in fractions of the width
        local side = 0.5 - half                   -- each band beside it
        if side > 0 then
            core.hud.rect(side / 2, 0.5, side * aspect, 1, 0, 0, 0, math.floor(150 * k))
            core.hud.rect(1 - side / 2, 0.5, side * aspect, 1, 0, 0, 0, math.floor(150 * k))
        end
        local a = math.floor(230 * k)
        local L, T = 0.07, 0.006                  -- a corner's arm and its thickness, of the height
        local x0, x1 = 0.5 - half + 0.03 / aspect, 0.5 + half - 0.03 / aspect
        for _, c in { { x0, 0.03, 1, 1 }, { x1, 0.03, -1, 1 }, { x0, 0.97, 1, -1 }, { x1, 0.97, -1, -1 } } do
            core.hud.rect(c[1] + c[3] * L / 2 / aspect, c[2], L, T, 255, 255, 255, a)
            core.hud.rect(c[1], c[2] + c[4] * L / 2, T, L, 255, 255, 255, a)
        end
        core.hud.rect(0.5, 0.5, 0.004, 0.03, 255, 255, 255, a)
        core.hud.rect(0.5, 0.5, 0.03, 0.004, 255, 255, 255, a)
        core.hud.text(0.5, 0.075, "Left click: take the photograph   ·   let go: put the camera down", 0.026, 1,
                      255, 255, 255, a)
    end)

    core.render.pass("album:frames", 50, function()
        local me = core.players()[1]
        if me == nil then
            return
        end
        -- Not `if me then core.player_pos(me) else nil`: an if-expression is
        -- ONE value, and y and z were nil -- nothing was ever drawn.
        local px, py, pz = core.player_pos(me)
        if px == nil or py == nil or pz == nil then
            return
        end
        for _, h in hung do
            if (h.x - px) ^ 2 + (h.y - py) ^ 2 + (h.z - pz) ^ 2 < SHOWN * SHOWN then
                local fr = FRONT[h.f % 4 + 1]
                local out = 0.0625 + 0.003 - 0.5
                core.render.screen(h.id, h.x + 0.5 + fr[1] * out, h.y + 0.5, h.z + 0.5 + fr[2] * out,
                                   math.atan2(-fr[1], fr[2]), 0.84, 0.84, true)
            end
        end
    end)
end
