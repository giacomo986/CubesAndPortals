--!strict
-- rewards: the worlds' tools in the sandbox, for whoever beat l'Ospite.
--
-- mods/keys says who has (keys:ended, as they come in); the first time, they
-- are given three things, and /rewards gives them again if lost:
--
--   the ROPE GUN: right click hooks a rope to what you look at, up to 40
--   away, left click lets go; jump lets go too, crouch lets it out, sprint
--   takes it in (core.rope, allowed by core.body_rope while it is in the
--   hand). Thirty hooks a day.
--   the PORTAL GUN: left click a wall for the blue portal, right click for
--   the orange, two cells tall with room before them; through one, out of
--   the other (core.portal_open, core.portal_link). Twelve shots a day; the
--   pair closes when you leave.
--   the KART: right click any block with it and you drive it, anywhere
--   (core.body_drive); right click anything and you are back on foot with it
--   in the bag.
--
-- The days are the world's (a dawn fills them again). Whoever has not beaten
-- l'Ospite and finds these finds them dead in the hand.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local ROPE, PORTAL, KART = "rewards:rope_gun", "rewards:portal_gun", "rewards:kart"
local ROPES, SHOTS = 30, 12
local EYE = 1.62

-- This machine: driving or not, for the view and the body drawn.
local function heard(text: string)
    if text == "kart 1" or text == "kart 0" then
        local on = text == "kart 1"
        core.view(if on then 5 else 0)
        core.body_model(0, if on then core.model_id("rewards:kart") else 0)
    end
end

local heard_click: (number, string) -> () = function(_, _) end

if serving then
    core.net.open()
    core.register_item(ROPE, 1)
    core.register_item(PORTAL, 1)
    core.register_item(KART, 1)
    core.register_model("rewards:rope_gun_held", "rope_gun_held.gltf")
    core.register_model("rewards:portal_gun_held", "portal_gun_held.gltf")
    local KART_MODEL = core.register_model("rewards:kart", "kart.gltf")

    local function who(p: number): string
        local k = core.player_key(p)
        return string.sub(if k ~= "" then k else "name:" .. string.lower(core.player_name(p)), 1, 48)
    end
    local function tell(p: number, text: string)
        if drawing then heard(text) else core.net.send(p, text) end
    end

    local ended: { [number]: boolean } = {}
    local function give_all(p: number)
        for _, item in { ROPE, PORTAL, KART } do
            if core.bag_count(p, item) == 0 then
                core.give_item(p, item, 1)
            end
        end
    end
    core.event.on("keys:ended", function(ev)
        ended[ev.player] = true
        if core.storage_get("given " .. who(ev.player)) == "" then
            core.storage_set("given " .. who(ev.player), "1")
            give_all(ev.player)
            core.notice(ev.player, "L'Ospite is beaten: the worlds' tools are yours here -- a rope gun, a portal gun, a kart")
        end
    end)
    core.command("rewards", "-- the worlds' tools again, for whoever beat l'Ospite")
    core.event.on("core:command", function(ev)
        if ev.name ~= "rewards" then
            return
        end
        if ended[ev.player] then
            give_all(ev.player)
            core.chat(ev.player, "The rope gun, the portal gun and the kart are in your bag.")
        else
            core.chat(ev.player, "Beat l'Ospite first, in the laboratory.")
        end
    end)

    -- Charges, by the world's day: "day,ropes,shots".
    local day = 0
    local last_t = core.time_of_day()
    local function charges(p: number): (number, number)
        local d, r, s = string.match(core.storage_get("charges " .. who(p)), "^(%d+),(%d+),(%d+)$")
        if d == nil or tonumber(d) ~= day then
            return ROPES, SHOTS
        end
        return tonumber(r) :: number, tonumber(s) :: number
    end
    local function spend(p: number, rope: number, shot: number)
        local r, s = charges(p)
        core.storage_set("charges " .. who(p), `{day},{math.max(0, r - rope)},{math.max(0, s - shot)}`)
    end

    -- ------------------------------------------------------------- portals
    local function solid(x: number, y: number, z: number): boolean
        return core.node_solid(core.get_block(x, y, z)) == 1
    end
    local function face_of(ox: number, oy: number, oz: number, dx: number, dy: number, dz: number, t: number,
                           cx: number, cy: number, cz: number): (number, number, number)
        local px, py, pz = ox + dx * t, oy + dy * t, oz + dz * t
        local best, nx, ny, nz = math.huge, 0, 0, 0
        for _, f in { { -1, 0, 0, px - cx }, { 1, 0, 0, px - cx - 1 }, { 0, -1, 0, py - cy }, { 0, 1, 0, py - cy - 1 },
                      { 0, 0, -1, pz - cz }, { 0, 0, 1, pz - cz - 1 } } do
            if f[1] * dx + f[2] * dy + f[3] * dz < 0 and math.abs(f[4]) < best then
                best, nx, ny, nz = math.abs(f[4]), f[1], f[2], f[3]
            end
        end
        return nx, ny, nz
    end
    type Shot = { x: number, y: number, z: number, axis: number, sign: number, idx: number }
    -- A wall two cells tall where it was aimed, with room before it.
    local function aim(p: number): Shot?
        local x, y, z = core.player_pos(p)
        local dx, dy, dz = core.player_look(p)
        if x == nil or dx == nil then return nil end
        local ox, oy, oz = x, y + EYE, z
        local cx, cy, cz, t = core.raycast(ox, oy, oz, dx, dy, dz, 64)
        if cx == nil then return nil end
        local nx, ny, nz = face_of(ox, oy, oz, dx, dy, dz, t, cx, cy, cz)
        if ny ~= 0 or (nx == 0 and nz == 0) then return nil end
        for _, base in { cy - 1, cy } do
            if solid(cx, base, cz) and solid(cx, base + 1, cz) and not solid(cx + nx, base, cz + nz)
                and not solid(cx + nx, base + 1, cz + nz) then
                return { x = cx + 0.5 + nx * 0.51, y = base + 1.0, z = cz + 0.5 + nz * 0.51,
                         axis = if nz ~= 0 then 0 else 1, sign = nx + nz, idx = -1 }
            end
        end
        return nil
    end
    local pairs_of: { [number]: { blue: Shot?, orange: Shot? } } = {}
    local function close(sh: Shot?)
        if sh then core.portal_close(sh.x, sh.y, sh.z) end
    end
    local function shoot(p: number, colour: string)
        local _, shots = charges(p)
        if shots <= 0 then
            core.notice(p, "The portal gun is spent until tomorrow")
            return
        end
        local at = aim(p)
        if at == nil then
            core.notice(p, "A portal wants a wall two blocks tall with room before it")
            return
        end
        local pr = pairs_of[p] or {}
        pairs_of[p] = pr
        local mine = if colour == "blue" then pr.blue else pr.orange
        close(mine)
        local idx = core.portal_open(at.x, at.y, at.z, at.axis, at.sign, 0, 0.5, 1.0, if colour == "blue" then 1 else 2)
        if idx < 0 then
            core.notice(p, "The portal will not open there")
            return
        end
        at.idx = idx
        if colour == "blue" then pr.blue = at else pr.orange = at end
        if pr.blue and pr.orange then
            core.portal_link((pr.blue :: Shot).idx, (pr.orange :: Shot).idx)
        else
            core.portal_link(idx, -1)
        end
        spend(p, 0, 1)
        local _, left = charges(p)
        core.notice(p, `{colour} portal; {left} shots left today`)
    end

    -- ----------------------------------------------------------------- kart
    local driving: { [number]: boolean } = {}
    local function on_foot(p: number)
        if driving[p] == nil then return end
        driving[p] = nil
        core.body_drive(p, 0, 0, 0, 0, 0.6, 0)
        core.body_movement(p)
        core.body_model(p, 0)
        tell(p, "kart 0")
        if core.give_item(p, KART, 1) < 1 then
            local x, y, z = core.player_pos(p)
            if x and y and z then core.drop_item(math.floor(x), math.floor(y), math.floor(z), core.item_id(KART), 1) end
        end
    end

    core.event.on("core:used", function(ev)
        local p = ev.player
        if driving[p] then
            ev.cancel = true
            on_foot(p)
            return
        end
        if ev.held == KART then
            ev.cancel = true
            if not ended[p] then
                core.notice(p, "The kart will not start for you")
            elseif core.take_item(p, KART, 1) == 1 then
                driving[p] = true
                core.body_drive(p, 12, 30, 2.6, 0.6, 1.7, 2.4)
                core.body_movement(p, 6, 14, 0, 8, -25, 10)
                core.body_model(p, KART_MODEL)
                core.player_place(p, ev.x + 0.5, ev.y + 1, ev.z + 0.5)
                tell(p, "kart 1")
                core.notice(p, "Right click anything to get out of the kart")
            end
        elseif ev.held == ROPE or ev.held == PORTAL then
            ev.cancel = true   -- a click on a block is the gun's (below), not a placement
        end
    end)

    heard_click = function(p: number, what: string)
        local held = core.held_item(p)
        if not ended[p] then
            return
        end
        if held == PORTAL and (what == "left" or what == "right") then
            shoot(p, if what == "left" then "blue" else "orange")
        elseif held == ROPE and what == "hook" then
            spend(p, 1, 0)
            local ropes = charges(p)
            if ropes <= 0 then
                core.notice(p, "The rope gun is spent until tomorrow")
            end
        end
    end
    core.event.on("core:net_message", function(ev)
        heard_click(ev.player, ev.text)
    end)

    local roped: { [number]: boolean } = {}
    core.event.on("core:tick", function(ev)
        local t = core.time_of_day()
        if last_t < 0.25 and t >= 0.25 then
            day += 1
        end
        last_t = t
        local here: { [number]: boolean } = {}
        for _, p in core.players() do
            here[p] = true
            -- The rope allowed while the gun is in the hand and has hooks left.
            local ropes = charges(p)
            local want = ended[p] == true and core.held_item(p) == ROPE and ropes > 0
            if want ~= (roped[p] == true) then
                core.body_rope(p, want)
                roped[p] = want or nil
            end
        end
        for p in ended do
            if not here[p] then
                ended[p], roped[p], driving[p] = nil, nil, nil
                local pr = pairs_of[p]
                if pr then close(pr.blue); close(pr.orange); pairs_of[p] = nil end
            end
        end
    end)
end

if drawing then
    if not serving then
        core.event.on("core:net_message", function(ev) heard(ev.text) end)
    end
    local function ask(what: string, player: number)
        if serving then heard_click(player, what) else core.net.send(0, what) end
    end
    core.event.on("core:input", function(ev)
        local held = core.held_item(1)
        if held == PORTAL and (ev.action == "mouse_left" or ev.action == "mouse_right") then
            ev.cancel = true
            ask(if ev.action == "mouse_left" then "left" else "right", ev.player)
        elseif held == ROPE then
            if ev.action == "mouse_right" then
                ev.cancel = true
                if core.rope(true) == 1 then ask("hook", ev.player) end
            elseif ev.action == "mouse_left" then
                ev.cancel = true
                core.rope(false)
            end
        end
    end)
end
