--!strict
-- cameras: a CAMERA and a SCREEN.
--
-- A camera is put down facing whoever puts it (as a chest is), and looks the
-- way it faces, tipped a little down. Right click it to choose it, then right
-- click a screen: from then the screen shows what that camera sees, live, to
-- everybody near (core.render.camera, core.render.screen -- drawn by each
-- player's machine). A screen with no camera, or whose camera went, shows snow.
--
-- A camera is a sensor too: while it sees a player or a creature within 16
-- in front of it, nothing solid between, it gives power like a lever that is
-- on (mods/cinnabar, told by the events cinnabar:source and cinnabar:changed)
-- -- a door that opens on whoever comes, a lamp, an alarm.
--
-- Pictures: tools/make_cameras_assets.py.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local FRONT = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }   -- an upright block's front: +Z, +X, -Z, -X
local PITCH = -0.2   -- a camera looks this far down
local SEES = 16      -- and this far
local CONE = math.cos(math.rad(35))
local SHOWN = 48     -- screens this near a player are drawn

local function key(x: number, y: number, z: number): string
    return `{x},{y},{z}`
end

-- Where a camera at (x, y, z) facing f looks from, and its turn.
local function eye_of(x: number, y: number, z: number, f: number): (number, number, number, number)
    local fr = FRONT[f % 4 + 1]
    return x + 0.5 + fr[1] * 0.55, y + 0.55, z + 0.5 + fr[2] * 0.55, math.atan2(fr[1], -fr[2])
end

-- What this machine draws: each screen near, with its camera's place.
type Shown = { x: number, y: number, z: number, f: number, cam: string?, cx: number, cy: number, cz: number, cf: number }
local shown: { Shown } = {}
local cam_ids: { [string]: number } = {}
local next_id = 1

local function heard(text: string)
    if string.sub(text, 1, 2) ~= "V " then
        return
    end
    local now: { Shown } = {}
    local wanted: { [string]: boolean } = {}
    for part in string.gmatch(string.sub(text, 3), "[^;]+") do
        local n = {}
        for v in string.gmatch(part, "[^,]+") do
            table.insert(n, tonumber(v) or 0)
        end
        if #n >= 4 then
            local s: Shown = { x = n[1], y = n[2], z = n[3], f = n[4], cam = nil, cx = 0, cy = 0, cz = 0, cf = 0 }
            if #n >= 8 then
                s.cam, s.cx, s.cy, s.cz, s.cf = key(n[5], n[6], n[7]), n[5], n[6], n[7], n[8]
                wanted[s.cam :: string] = true
            end
            table.insert(now, s)
        end
    end
    -- Cameras no screen here shows any more are forgotten; new ones placed.
    for k, id in cam_ids do
        if not wanted[k] then
            core.render.camera_drop(id)
            cam_ids[k] = nil
        end
    end
    for _, s in now do
        if s.cam and cam_ids[s.cam] == nil then
            local id = next_id
            next_id += 1
            cam_ids[s.cam :: string] = id
            local ex, ey, ez, yaw = eye_of(s.cx, s.cy, s.cz, s.cf)
            core.render.camera(id, ex, ey, ez, yaw, PITCH, 70)
        end
    end
    shown = now
end

if serving then
    core.net.open()
    local function node(name: string, light: number): number
        return core.register_node(name, 70, 72, 78, true, light, "upright", 0, 1.0, "stone")
    end
    local CAMERA    = node("cameras:camera", 0)
    local CAMERA_ON = node("cameras:camera_on", 3)
    local SCREEN    = node("cameras:screen", 4)
    for _, id in { CAMERA, CAMERA_ON } do
        core.node_box(id, 0.3, 0.0, 0.3, 0.7, 0.3, 0.7)       -- the stand
        core.node_box(id, 0.25, 0.3, 0.15, 0.75, 0.7, 0.85)   -- the body
        core.node_box(id, 0.38, 0.4, 0.85, 0.62, 0.6, 1.0)    -- the lens, at the front (+Z)
    end
    core.node_drop(CAMERA_ON, CAMERA)
    -- The screen: a panel at the back of its cell, its glass towards the front.
    core.node_box(SCREEN, 0.0, 0.0, 0.0, 1.0, 1.0, 0.125)
    core.recipe_shaped("cameras:camera", 1,
        "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot",
        "furnace:iron_ingot core:glass         ore:cinnabar",
        "-                  furnace:iron_ingot -")
    core.recipe_shaped("cameras:screen", 1,
        "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot",
        "furnace:iron_ingot core:glass         furnace:iron_ingot",
        "furnace:iron_ingot ore:cinnabar       furnace:iron_ingot")
    core.event.emit("cinnabar:source", { name = "cameras:camera_on" })

    type Cam = { x: number, y: number, z: number }
    local cams: { [string]: Cam } = {}
    local screens: { [string]: Cam } = {}
    local chosen: { [number]: string } = {}

    local function is_cam(id: number): boolean
        return id == CAMERA or id == CAMERA_ON
    end

    core.register_lbm("cameras:found", { "cameras:camera", "cameras:camera_on", "cameras:screen" }, function(cell)
        local id = core.get_block(cell.x, cell.y, cell.z)
        local c = { x = cell.x, y = cell.y, z = cell.z }
        if is_cam(id) then cams[key(cell.x, cell.y, cell.z)] = c else screens[key(cell.x, cell.y, cell.z)] = c end
    end)
    core.event.on("core:placed", function(ev)
        local c = { x = ev.x, y = ev.y, z = ev.z }
        if is_cam(ev.id) then
            cams[key(ev.x, ev.y, ev.z)] = c
        elseif ev.id == SCREEN then
            screens[key(ev.x, ev.y, ev.z)] = c
            core.notice(ev.player, "Right click a camera, then this screen, to show what it sees")
        end
    end)
    core.event.on("core:broken", function(ev)
        local k = key(ev.x, ev.y, ev.z)
        if cams[k] then
            cams[k] = nil
            core.event.emit("cinnabar:changed", { x = ev.x, y = ev.y, z = ev.z })
        end
        if screens[k] then
            screens[k] = nil
            core.meta_set(ev.x, ev.y, ev.z, "camera", "")
        end
    end)

    core.event.on("core:used", function(ev)
        if is_cam(ev.id) then
            chosen[ev.player] = key(ev.x, ev.y, ev.z)
            core.notice(ev.player, "Camera chosen: now right click a screen")
            ev.cancel = true
        elseif ev.id == SCREEN then
            local c = chosen[ev.player]
            if c and cams[c] then
                core.meta_set(ev.x, ev.y, ev.z, "camera", c)
                core.notice(ev.player, "The screen shows that camera")
            else
                core.notice(ev.player, "Right click a camera first, then this screen")
            end
            ev.cancel = true
        end
    end)

    -- Whether the camera at c sees a body standing at (bx, by, bz).
    local function sees(c: Cam, f: number, bx: number, by: number, bz: number): boolean
        local ex, ey, ez, yaw = eye_of(c.x, c.y, c.z, f)
        local dx, dy, dz = bx - ex, by + 1.0 - ey, bz - ez
        local d = math.sqrt(dx * dx + dy * dy + dz * dz)
        if d > SEES or d < 0.3 then
            return false
        end
        local cp = math.cos(PITCH)
        local fx, fy, fz = math.sin(yaw) * cp, math.sin(PITCH), -math.cos(yaw) * cp
        if (dx * fx + dy * fy + dz * fz) / d < CONE then
            return false
        end
        local _, _, _, at = core.raycast(ex, ey, ez, dx, dy, dz, d)
        return at == nil or at >= d - 0.6
    end

    local function near_player(x: number, y: number, z: number, r: number): boolean
        for _, p in core.players() do
            local px, py, pz = core.player_pos(p)
            if px and py and pz and (px - x) ^ 2 + (py - y) ^ 2 + (pz - z) ^ 2 < r * r then
                return true
            end
        end
        return false
    end

    local told: { [number]: string } = {}
    local since = 0.0
    core.event.on("core:tick", function(ev)
        since += ev.dt
        if since < 0.5 then
            return
        end
        since = 0
        -- Each camera near somebody: does it see anyone?
        for k, c in cams do
            local id = core.get_block(c.x, c.y, c.z)
            if not is_cam(id) then
                cams[k] = nil
            elseif near_player(c.x, c.y, c.z, 64) then
                local f = core.get_facing(c.x, c.y, c.z)
                local seen = false
                for _, p in core.players() do
                    local px, py, pz = core.player_pos(p)
                    if px and py and pz and sees(c, f, px, py, pz) then
                        seen = true
                        break
                    end
                end
                if not seen then
                    for _, m in core.mob_ids(c.x + 0.5, c.y + 0.5, c.z + 0.5, SEES, true) do
                        local mx, my, mz = core.mob_pos(m)
                        if mx and my and mz and sees(c, f, mx, my, mz) then
                            seen = true
                            break
                        end
                    end
                end
                local want = if seen then CAMERA_ON else CAMERA
                if id ~= want then
                    core.set_block(c.x, c.y, c.z, want)
                    core.set_facing(c.x, c.y, c.z, f)
                    core.event.emit("cinnabar:changed", { x = c.x, y = c.y, z = c.z })
                end
            end
        end
        -- And to each player, the screens near them and their cameras.
        local now: { [number]: string } = {}
        for _, p in core.players() do
            local px, py, pz = core.player_pos(p)
            if px and py and pz then
                local parts = {}
                for k, s in screens do
                    if core.get_block(s.x, s.y, s.z) ~= SCREEN then
                        screens[k] = nil
                    elseif (s.x - px) ^ 2 + (s.y - py) ^ 2 + (s.z - pz) ^ 2 < (SHOWN + 8) ^ 2 then
                        local part = `{s.x},{s.y},{s.z},{core.get_facing(s.x, s.y, s.z)}`
                        local c = cams[core.meta_get(s.x, s.y, s.z, "camera")]
                        if c then
                            part ..= `,{c.x},{c.y},{c.z},{core.get_facing(c.x, c.y, c.z)}`
                        end
                        table.insert(parts, part)
                    end
                end
                table.sort(parts)
                local text = "V " .. table.concat(parts, ";")
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

    core.render.pass("cameras:screens", 50, function()
        local me = core.players()[1]
        local px, py, pz = if me then core.player_pos(me) else nil
        if px == nil or py == nil or pz == nil then
            return
        end
        for _, s in shown do
            if (s.x - px) ^ 2 + (s.y - py) ^ 2 + (s.z - pz) ^ 2 < SHOWN * SHOWN then
                local fr = FRONT[s.f % 4 + 1]
                -- On the glass: the panel's front, a hair out of it.
                local back = 0.125 + 0.003 - 0.5
                local cx = s.x + 0.5 + fr[1] * back
                local cz = s.z + 0.5 + fr[2] * back
                local yaw = math.atan2(-fr[1], fr[2])
                core.render.screen(if s.cam then cam_ids[s.cam :: string] or -1 else -1,
                                   cx, s.y + 0.5, cz, yaw, 0.86, 0.86)
            end
        end
    end)
end
