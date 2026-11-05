--!strict
-- album: a camera that takes photographs, frames to hang them, an album.
--
-- Right click with the camera in the hand -- at a block or at nothing, a
-- view, the sky -- and it takes a picture of what you see: where you stand
-- and which way you look. It goes in your
-- album (/album), forty at most, the oldest let go. A FRAME put on a wall
-- asks which of your photographs goes in it, and then everybody near sees
-- it there: each player's machine draws it once, from the place it was
-- taken, and keeps it (core.render.camera, still; core.render.screen,
-- plain) -- a photograph shows the place as it is when it is first seen.
--
-- Taken somewhere rare -- on a peak, in the Hollows, by the relics, over the
-- clouds -- a photograph is worth something to a village's museum: the album
-- offers it to the museum of the village you stand in (the event
-- museum:photo, mods/villagers), which pays for the first of each.
--
-- Pictures: tools/make_album_assets.py.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local REACH = 6   -- how far a right click reaches a block (the engine's)
local shoot: (number) -> () = function(_) end

-- Whether a block is within a right click's reach where that player looks.
local function facing_block(p: number): boolean
    local x, y, z = core.player_pos(p)
    local dx, dy, dz = core.player_look(p)
    if x == nil or y == nil or z == nil or dx == nil or dy == nil or dz == nil then
        return true
    end
    return core.raycast(x, y + 1.62, z, dx, dy, dz, REACH) ~= nil
end

local FRONT = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }   -- an upright block's front: +Z, +X, -Z, -X
local SHOWN = 48
local RARE = { peak = "a peak", hollows = "the Hollows", relic = "a relic", clouds = "the clouds" }

-- What this machine draws: each frame near, and its photograph's place.
type Hung = { x: number, y: number, z: number, f: number, id: number,
              px: number, py: number, pz: number, yaw: number, pitch: number }
local hung: { Hung } = {}
local made: { [number]: boolean } = {}   -- photographs made into a camera here

local function heard(text: string)
    if string.sub(text, 1, 2) ~= "H " then
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
    for _, h in now do
        wanted[h.id] = true
        if not made[h.id] then
            core.render.camera(h.id, h.px, h.py, h.pz, h.yaw, h.pitch, 60, true)
            made[h.id] = true
        end
    end
    for id in made do
        if not wanted[id] then
            core.render.camera_drop(id)
            made[id] = nil
        end
    end
    hung = now
end

if serving then
    core.net.open()
    core.register_item("album:camera", 1)
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

    shoot = function(p: number)
        local x, y, z = core.player_pos(p)
        local lx, ly, lz = core.player_look(p)
        if x == nil or lx == nil then
            return
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
    end
    core.event.on("core:used", function(ev)
        if ev.held ~= "album:camera" then
            return
        end
        ev.cancel = true
        shoot(ev.player)
    end)
    -- Aimed at no block -- a view, the sky: the player's machine asks (below).
    core.event.on("core:net_message", function(ev)
        if ev.text == "shoot" and core.held_item(ev.player) == "album:camera" and not facing_block(ev.player) then
            shoot(ev.player)
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
            table.insert(page, "No photographs yet: right click with the camera in your hand.")
        end
        for i = #list, 1, -1 do
            local id = list[i]
            local ph = photo(id)
            if ph then
                local line = `- **#{id}** at {math.floor(ph.x)} {math.floor(ph.y)} {math.floor(ph.z)}`
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
        if ev.id == FRAME and ev.held ~= "album:camera" then
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
    -- A right click at no block within reach -- a view, the sky, the clouds
    -- -- takes a picture as well: core:used is only a click on a block.
    core.event.on("core:input", function(ev)
        if ev.action ~= "mouse_right" or core.held_item(ev.player) ~= "album:camera" or facing_block(ev.player) then
            return
        end
        if serving then
            shoot(ev.player)
        else
            core.net.send(0, "shoot")
        end
    end)
    core.render.pass("album:frames", 50, function()
        local me = core.players()[1]
        local px, py, pz = if me then core.player_pos(me) else nil
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
