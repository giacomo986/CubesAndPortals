--!strict
-- cameras: a CAMERA and a SCREEN.
--
-- A camera is put down facing whoever puts it (as a chest is), and looks the
-- way it faces, tipped a little down. Right click it to choose it, then right
-- click a screen: from then the screen shows what that camera sees, live, to
-- everybody near (core.render.camera, core.render.screen -- drawn by each
-- player's machine). A screen with no camera, or whose camera went, shows snow.
--
-- Right click a screen that shows a camera (with no camera just chosen) and
-- you take the camera's controls: the view goes to the screen and fills
-- with it (core.fixed_view), the body stands, and W A S D turn the camera --
-- A and D round, up to a right angle either way from where it faces, W and
-- S up and down. Right click again to step back. Where it is turned is
-- kept in the camera (its cell's "yaw" and "pitch") and seen by everybody,
-- and it is where the sensor looks -- and where its head turns, for
-- everybody: the block is the camera's stand (camera_stand.gltf) and each
-- machine draws the head on it as turned (camera_head.gltf, lit red while it
-- sees somebody; tools/make_camera_models.py).
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

local TURN = math.rad(90)                         -- round, either way from its front
local LOW, HIGH = math.rad(-70), math.rad(45)      -- and down and up
local SPEED = math.rad(60)                         -- a second, a key held

-- Where a camera at (x, y, z) facing f looks from, and its turn; `off` turns
-- it round from its front.
local function eye_of(x: number, y: number, z: number, f: number, off: number?): (number, number, number, number)
    local fr = FRONT[f % 4 + 1]
    return x + 0.5 + fr[1] * 0.55, y + 0.55, z + 0.5 + fr[2] * 0.55, math.atan2(fr[1], -fr[2]) + (off or 0)
end

-- What this machine draws: each screen near, with its camera's place.
type Shown = { x: number, y: number, z: number, f: number, cam: string?, cx: number, cy: number, cz: number, cf: number,
               off: number, pitch: number }
local shown: { Shown } = {}
local server_heard: (number, string) -> () = function(_, _) end
local aim_of: (any) -> (number, number)
local cam_ids: { [string]: number } = {}
local cam_aim: { [string]: { number } } = {}   -- each camera's turn as drawn here: off, pitch
local next_id = 1

-- The camera this machine is turning, from which screen.
type Control = { cam: string, sx: number, sy: number, sz: number, sf: number,
                 cx: number, cy: number, cz: number, cf: number, off: number, pitch: number }
local control: Control? = nil
local begin_control: (Control) -> () = function(_) end

local function place_camera(k: string, cx: number, cy: number, cz: number, cf: number, off: number, pitch: number)
    local id = cam_ids[k]
    if id == nil then
        return
    end
    local ex, ey, ez, yaw = eye_of(cx, cy, cz, cf, off)
    core.render.camera(id, ex, ey, ez, yaw, pitch, 70)
    cam_aim[k] = { off, pitch }
end

-- The cameras near, as the server last said: where, facing, turned, lit.
type Head = { x: number, y: number, z: number, f: number, off: number, pitch: number, on: boolean }
local heads: { Head } = {}

local function heard(text: string)
    if string.sub(text, 1, 2) == "K " then
        local now: { Head } = {}
        for part in string.gmatch(string.sub(text, 3), "[^;]+") do
            local n = {}
            for v in string.gmatch(part, "[^,]+") do
                table.insert(n, tonumber(v) or 0)
            end
            if #n >= 7 then
                table.insert(now, { x = n[1], y = n[2], z = n[3], f = n[4], off = n[5], pitch = n[6], on = n[7] == 1 })
            end
        end
        heads = now
        return
    end
    local c = { string.match(text, "^C (%S+) (%S+) (%S+) (%S+) (%S+) (%S+) (%S+) (%S+) (%S+) (%S+)$") }
    if #c == 10 then
        local n = {}
        for i = 1, 10 do
            n[i] = tonumber(c[i]) or 0
        end
        begin_control({ sx = n[1], sy = n[2], sz = n[3], sf = n[4], cx = n[5], cy = n[6], cz = n[7], cf = n[8],
                        off = n[9], pitch = n[10], cam = key(n[5], n[6], n[7]) })
        return
    end
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
            local s: Shown = { x = n[1], y = n[2], z = n[3], f = n[4], cam = nil, cx = 0, cy = 0, cz = 0, cf = 0,
                               off = 0, pitch = PITCH }
            if #n >= 10 then
                s.cam, s.cx, s.cy, s.cz, s.cf = key(n[5], n[6], n[7]), n[5], n[6], n[7], n[8]
                s.off, s.pitch = n[9], n[10]
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
            cam_aim[k] = nil
        end
    end
    for _, s in now do
        local k = s.cam
        if k then
            if cam_ids[k] == nil then
                cam_ids[k] = next_id
                next_id += 1
            end
            -- Turned by somebody else (not the one turning it here).
            local aim = cam_aim[k]
            local mine = control and control.cam == k
            if not mine and (aim == nil or aim[1] ~= s.off or aim[2] ~= s.pitch) then
                place_camera(k, s.cx, s.cy, s.cz, s.cf, s.off, s.pitch)
            end
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
    -- Drawn as its stand (the block, which does not turn) and its head, drawn
    -- by each machine where the camera is turned (below); the boxes stay the
    -- block's shape. In the hand and the bag: tools/make_held_models.py.
    local STAND = core.register_model("cameras:camera_stand", "camera_stand.gltf")   -- tools/make_camera_models.py
    core.node_model(CAMERA, STAND)
    core.node_model(CAMERA_ON, STAND)
    core.register_model("cameras:camera_head", "camera_head.gltf")
    core.register_model("cameras:camera_head_on", "camera_head_on.gltf")
    core.register_model("cameras:camera_held", "camera_held.gltf")
    core.register_model("cameras:screen_held", "screen_held.gltf")
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

    -- Where a camera is turned: round from its front, and up or down.
    aim_of = function(c: Cam): (number, number)
        return tonumber(core.meta_get(c.x, c.y, c.z, "yaw")) or 0,
               tonumber(core.meta_get(c.x, c.y, c.z, "pitch")) or PITCH
    end
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
            local shows = cams[core.meta_get(ev.x, ev.y, ev.z, "camera")]
            if c and cams[c] then
                core.meta_set(ev.x, ev.y, ev.z, "camera", c)
                chosen[ev.player] = nil
                core.notice(ev.player, "The screen shows that camera: right click it again to turn the camera")
            elseif shows then
                -- The controls, to that player's machine.
                local text = string.format("C %d %d %d %d %d %d %d %d %.4f %.4f", ev.x, ev.y, ev.z,
                    core.get_facing(ev.x, ev.y, ev.z), shows.x, shows.y, shows.z,
                    core.get_facing(shows.x, shows.y, shows.z), aim_of(shows))
                if drawing then heard(text) else core.net.send(ev.player, text) end
            else
                core.notice(ev.player, "Right click a camera first, then this screen")
            end
            ev.cancel = true
        end
    end)

    -- A camera turned from a screen: "A x y z off pitch", from somebody near
    -- a screen showing it, kept in its cell.
    server_heard = function(p: number, text: string)
        local x, y, z, off, pitch = string.match(text, "^A (%-?%d+) (%-?%d+) (%-?%d+) (%S+) (%S+)$")
        local c = if x then cams[key(tonumber(x) :: number, tonumber(y) :: number, tonumber(z) :: number)] else nil
        local o, pi = tonumber(off), tonumber(pitch)
        if c == nil or o == nil or pi == nil or o ~= o or pi ~= pi then
            return
        end
        local px, py, pz = core.player_pos(p)
        if px == nil or py == nil or pz == nil then
            return
        end
        local k = key(c.x, c.y, c.z)
        local near = false
        for _, s in screens do
            if core.meta_get(s.x, s.y, s.z, "camera") == k and (s.x - px) ^ 2 + (s.y - py) ^ 2 + (s.z - pz) ^ 2 < 10 * 10 then
                near = true
            end
        end
        if near then
            core.meta_set(c.x, c.y, c.z, "yaw", string.format("%.4f", math.clamp(o, -TURN, TURN)))
            core.meta_set(c.x, c.y, c.z, "pitch", string.format("%.4f", math.clamp(pi, LOW, HIGH)))
        end
    end
    core.event.on("core:net_message", function(ev)
        server_heard(ev.player, ev.text)
    end)

    -- Whether the camera at c sees a body standing at (bx, by, bz).
    local function sees(c: Cam, f: number, bx: number, by: number, bz: number): boolean
        local off, pitch = aim_of(c)
        local ex, ey, ez, yaw = eye_of(c.x, c.y, c.z, f, off)
        local dx, dy, dz = bx - ex, by + 1.0 - ey, bz - ez
        local d = math.sqrt(dx * dx + dy * dy + dz * dz)
        if d > SEES or d < 0.3 then
            return false
        end
        local cp = math.cos(pitch)
        local fx, fy, fz = math.sin(yaw) * cp, math.sin(pitch), -math.cos(yaw) * cp
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
    local told_k: { [number]: string } = {}
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
        local now_k: { [number]: string } = {}
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
                            local off, pitch = aim_of(c)
                            part ..= string.format(",%d,%d,%d,%d,%.4f,%.4f", c.x, c.y, c.z,
                                                   core.get_facing(c.x, c.y, c.z), off, pitch)
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
                -- And the cameras near, where each is turned and whether it sees
                -- somebody: their heads are drawn there.
                local heads = {}
                for _, c in cams do
                    if (c.x - px) ^ 2 + (c.y - py) ^ 2 + (c.z - pz) ^ 2 < (SHOWN + 8) ^ 2 then
                        local off, pitch = aim_of(c)
                        table.insert(heads, string.format("%d,%d,%d,%d,%.4f,%.4f,%d", c.x, c.y, c.z,
                            core.get_facing(c.x, c.y, c.z), off, pitch,
                            if core.get_block(c.x, c.y, c.z) == CAMERA_ON then 1 else 0))
                    end
                end
                table.sort(heads)
                local ktext = "K " .. table.concat(heads, ";")
                if told_k[p] ~= ktext then
                    if drawing then heard(ktext) else core.net.send(p, ktext) end
                end
                now_k[p] = ktext
            end
        end
        told = now
        told_k = now_k
    end)
end

if drawing then
    core.event.on("core:net_message", function(ev)
        heard(ev.text)
    end)

    -- At a screen's controls: the view on the screen, W A S D turning its
    -- camera here at once and told to the server now and then.
    local held: { [string]: boolean } = {}
    local told_at, told_aim = 0.0, ""
    local function tell(c: Control)
        local text = string.format("A %d %d %d %.4f %.4f", c.cx, c.cy, c.cz, c.off, c.pitch)
        if text ~= told_aim then
            told_aim = text
            if serving then server_heard(core.players()[1] or 0, text) else core.net.send(0, text) end
        end
    end
    local function end_control()
        local c = control
        if c then
            tell(c)
        end
        control = nil
        held = {}
        core.fixed_view(0, 0, 0, 0, 0, 50, 0, 0.6)
    end
    begin_control = function(c: Control)
        control = c
        held = {}
        local fr = FRONT[c.sf % 4 + 1]
        local back = 0.125 + 0.003 - 0.5
        -- In front of the glass, looking at it, near enough that it fills the view.
        core.fixed_view(c.sx + 0.5 + fr[1] * back, c.sy + 0.5, c.sz + 0.5 + fr[2] * back, 0.8,
                        math.deg(math.atan2(-fr[1], fr[2])), 10, 0, 0.6)
    end

    core.event.on("core:input", function(ev)
        local c = control
        if c == nil then
            return
        end
        local a = ev.action
        if a == "mouse_right" then
            ev.cancel = true
            end_control()
            return
        end
        local k = string.match(a, "^key_([wasd])$")
        local up = string.match(a, "^key_([wasd])_up$")
        if k then
            held[k] = true
        elseif up then
            held[up] = nil
        end
    end)

    core.event.on("core:tick", function(ev)
        local c = control
        if c == nil then
            return
        end
        -- Gone from beside it, or the screen gone: back to the eye.
        local me = core.players()[1]
        local px = if me then (core.player_pos(me)) else nil
        local found = false
        for _, s in shown do
            found = found or (s.x == c.sx and s.y == c.sy and s.z == c.sz and s.cam == c.cam)
        end
        if not found or px == nil then
            end_control()
            return
        end
        local turn = (if held.d then 1 else 0) - (if held.a then 1 else 0)
        local tilt = (if held.w then 1 else 0) - (if held.s then 1 else 0)
        if turn ~= 0 or tilt ~= 0 then
            c.off = math.clamp(c.off + turn * SPEED * ev.dt, -TURN, TURN)
            c.pitch = math.clamp(c.pitch + tilt * SPEED * ev.dt, LOW, HIGH)
            place_camera(c.cam, c.cx, c.cy, c.cz, c.cf, c.off, c.pitch)
        end
        told_at += ev.dt
        if told_at > 0.2 then
            told_at = 0
            tell(c)
        end
    end)

    core.hud.pass("cameras:controls", 60, function()
        if control == nil then
            return
        end
        core.hud.text(0.5, 0.05, "A D: turn   ·   W S: up and down   ·   right click: step back", 0.028, 1,
                      255, 255, 255, 230)
    end)

    core.render.pass("cameras:screens", 50, function()
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
        -- Each camera's head where it is turned: the one being turned here
        -- where it is now, the rest where the server says.
        local head, head_on = core.model_id("cameras:camera_head"), core.model_id("cameras:camera_head_on")
        for _, h in heads do
            if (h.x - px) ^ 2 + (h.y - py) ^ 2 + (h.z - pz) ^ 2 < SHOWN * SHOWN then
                local off, pitch = h.off, h.pitch
                local c = control
                if c and c.cx == h.x and c.cy == h.y and c.cz == h.z then
                    off, pitch = c.off, c.pitch
                end
                local _, _, _, yaw = eye_of(h.x, h.y, h.z, h.f, off)
                -- The head's lens is its model's +Z, which Ry(a) Rx(b) (render.model's
                -- turn) sends to (sin a cos b, -sin b, cos a cos b): a look along
                -- (sin yaw, ., -cos yaw) is a = pi - yaw, and up by pitch is b = -pitch.
                core.render.model(if h.on then head_on else head, h.x + 0.5, h.y + 0.5, h.z + 0.5,
                                  math.pi - yaw, 1, -pitch)
            end
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
