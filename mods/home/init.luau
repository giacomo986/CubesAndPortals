--!strict
-- home: a player's own room -- its door, who may come in, and the way out.
--
-- A server whose rooms file has the template [room home-*] gives every player
-- a room of their own, a whole world: "home-" and the start of their key. It
-- is made the first time a frame of theirs is told to lead to it (mods/portal,
-- "my room" in the frame's menu) and somebody comes near that frame, and it
-- saves and stops a minute after nobody is in it or near a way into it.
--
-- In such a room this mod says one portal to the other rooms, "door", one
-- wide and two tall, three steps north of the spawn and facing it: what the
-- player's frame links to, and the way back out to wherever that frame stands
-- now. The frame round it is put up, on a floor of its own, the first time a
-- player is in the room; breaking it changes nothing but the look, the door is
-- where it was said.
--
-- WHO MAY COME IN is the owner's, from any room (/myroom): a blacklist (all
-- but the names on it) or a whitelist (only the names on it), and whether the
-- others may still LOOK in through the frame or see a red membrane. Kept in
-- the server's shared storage, so every room knows it, and given to the
-- server (core.room_access), which holds it at every frame that leads there
-- and at the room itself. The owner, and whoever sets up the server's
-- portals, always come in. Somebody inside who may no longer be is sent out.
--
-- THE WAY OUT when the frame that led in is gone: /myroom leave, in a player's
-- room, sends whoever says it to the spawn of the server's first room (the
-- sandbox), as through a portal (core.room_send).
--
--   /myroom                        what your room lets in
--   /myroom whitelist | blacklist  only the names on the list, or all but them
--   /myroom add NAME   /myroom remove NAME
--   /myroom look yes|no            the others see in through the frame, or not
--   /myroom leave                  in somebody's room: out, to the spawn
--
-- ponytail: one door, where the world put it. The player moves the OTHER end
-- (any frame of theirs, one at a time); moving this end wants a block to carry.

if core.side() == "client" then
    return
end

local ROOM = core.room()
local IS_HOME = string.sub(ROOM, 1, 5) == "home-"

-- ------------------------------------------------------- who may come in

type Rule = { white: boolean, look: boolean, names: { string } }

local function key_of(player: number): string
    return string.sub(string.gsub(string.lower(core.player_key(player)), "[^a-z0-9]", ""), 1, 24)
end

local function room_of(player: number): string
    local key = key_of(player)
    return if key == "" then "" else "home-" .. key
end

-- "black|1|anna,bruno": the list's kind, whether the others may look, the names.
local function read(room: string): Rule
    local text = core.shared_storage_get(`access {room}`)
    local kind, look, names = string.match(text, "^(%a+)|(%d)|(.*)$")
    local rule: Rule = { white = kind == "white", look = look ~= "0", names = {} }
    if names and names ~= "" then
        rule.names = string.split(names, ",")
    end
    return rule
end

-- The rule given to the server. No list and a blacklist is a room open to all:
-- no rule at all.
local function apply(room: string)
    local rule = read(room)
    core.room_access_clear(room)
    if not rule.white and #rule.names == 0 then
        return
    end
    core.room_access(room, "key:" .. string.sub(room, 6), true, true)
    core.room_access(room, "*", if rule.white then rule.look else true, not rule.white)
    for _, name in rule.names do
        if rule.white then
            core.room_access(room, "name:" .. name, true, true)
        else
            core.room_access(room, "name:" .. name, rule.look, false)
        end
    end
end

local function write(room: string, rule: Rule)
    core.shared_storage_set(`access {room}`,
        `{if rule.white then "white" else "black"}|{if rule.look then 1 else 0}|{table.concat(rule.names, ",")}`)
    local known = core.shared_storage_get("rooms")
    local list = if known == "" then {} else string.split(known, ",")
    if not table.find(list, room) then
        table.insert(list, room)
        core.shared_storage_set("rooms", table.concat(list, ","))
    end
    core.shared_storage_set("stamp", tostring((tonumber(core.shared_storage_get("stamp")) or 0) + 1))
    apply(room)
end

-- Every room's rule, given again: the server keeps them only while it runs.
do
    local known = core.shared_storage_get("rooms")
    if known ~= "" then
        for _, room in string.split(known, ",") do
            apply(room)
        end
    end
end

-- Whether this player may be in `room` now, as the server would say it.
local function may_enter(room: string, player: number): boolean
    local key = key_of(player)
    if key ~= "" and "home-" .. key == room then
        return true
    end
    if core.has_perm(player, "portals") then
        return true
    end
    local rule = read(room)
    local listed = table.find(rule.names, string.lower(core.player_name(player))) ~= nil
    return if rule.white then listed else not listed
end

local function say_rule(player: number, room: string)
    local rule = read(room)
    local names = if #rule.names == 0 then "nobody" else table.concat(rule.names, ", ")
    if rule.white then
        core.chat(player, `Your room lets in only the names on its list: {names}.`)
    else
        core.chat(player, `Your room lets in everybody but the names on its list: {names}.`)
    end
    core.chat(player, if rule.look then "The others may look in through your frame."
        else "The others see a red membrane in your frame.")
    core.chat(player, "/myroom whitelist | blacklist | add NAME | remove NAME | look yes|no | leave")
end

core.command("myroom", "[whitelist|blacklist|add NAME|remove NAME|look yes|no|leave] -- who may come into your own room, and the way out of one")
core.event.on("core:command", function(ev)
    if ev.name ~= "myroom" then
        return
    end
    local a = string.split(string.gsub(ev.args, "^%s+", ""), " ")
    local verb, player = string.lower(a[1] or ""), ev.player

    if verb == "leave" then
        if not IS_HOME then
            core.chat(player, "/myroom leave is the way out of a player's own room: you are not in one.")
        elseif not core.room_send(player, "") then
            core.chat(player, "There is no room to send you to.")
        end
        return
    end

    local room = room_of(player)
    if room == "" then
        core.chat(player, "A room of your own is kept by your key: join with one.")
        return
    end
    local rule = read(room)
    if verb == "" then
        say_rule(player, room)
        return
    elseif verb == "whitelist" or verb == "blacklist" then
        rule.white = verb == "whitelist"
    elseif verb == "look" and (a[2] == "yes" or a[2] == "no") then
        rule.look = a[2] == "yes"
    elseif (verb == "add" or verb == "remove") and a[2] and a[2] ~= "" then
        local name = string.lower(a[2])
        if not string.match(name, "^[%w_%-]+$") or #name > 32 then
            core.chat(player, "A name is letters, digits, _ and -.")
            return
        end
        local at = table.find(rule.names, name)
        if verb == "add" and not at then
            if #rule.names >= 64 then
                core.chat(player, "The list holds 64 names.")
                return
            end
            table.insert(rule.names, name)
        elseif verb == "remove" and at then
            table.remove(rule.names, at)
        end
    else
        core.chat(player, "/myroom whitelist | blacklist | add NAME | remove NAME | look yes|no | leave")
        return
    end
    write(room, rule)
    core.log(`home: {core.player_name(player)} set the rule of {room}`)
    say_rule(player, room)
end)

if not IS_HOME then
    return
end

-- ------------------------------------------------------- the room itself

local FRAME = core.register_node("home:frame", 150, 112, 64, true, 3, "", 0, 1.5, "wood")
local FLOOR = core.register_node("home:floor", 176, 136, 84, true, 0, "", 0, 2.0, "wood")

-- The hole's lowest cell, once the world says where it starts.
local hx: number?, hy: number?, hz: number? = nil, nil, nil
local said, built = false, core.storage_get("built") == "1"

local function say(): boolean
    if hx == nil then
        local at = core.storage_get("door")
        local x, y, z = string.match(at, "^(-?%d+) (-?%d+) (-?%d+)$")
        if x then
            hx, hy, hz = tonumber(x), tonumber(y), tonumber(z)
        else
            local sx, sy, sz = core.world_spawn()
            if sx == nil or sy == nil or sz == nil then
                return false
            end
            hx, hy, hz = math.floor(sx), math.floor(sy), math.floor(sz) - 3
            core.storage_set("door", `{hx} {hy} {hz}`)
        end
    end
    local x, y, z = hx :: number, hy :: number, hz :: number
    -- Standing in the XY plane, its front towards the spawn (+z).
    if not core.room_portal_set("door", x + 0.5, y + 1.0, z + 0.5, 0, 1, 0.5, 1.0) then
        return false
    end
    core.log(`home: the door of {ROOM} is at {x} {y} {z}`)
    return true
end

-- The frame round the hole, a floor under it and in front, and the air a body
-- coming out needs.
local function build()
    local x, y, z = hx :: number, hy :: number, hz :: number
    for dx = -1, 1 do
        for dz = 0, 3 do
            core.set_block(x + dx, y - 1, z + dz, FLOOR)
            for dy = 0, 2 do
                if dz > 0 then
                    core.set_block(x + dx, y + dy, z + dz, 0)
                end
            end
        end
        for dy = 0, 2 do
            core.set_block(x + dx, y + dy, z, if dx == 0 and dy < 2 then 0 else FRAME)
        end
    end
    core.storage_set("built", "1")
    built = true
end

-- Every two seconds: whoever is in and may no longer be goes out, and whoever
-- is in a room whose way in is gone hears how to leave (once, and again when
-- it is gone again).
local polled, told = 0.0, {} :: { [number]: boolean }
core.event.on("core:tick", function(ev)
    if not said then
        said = say()
    end
    if said and not built and #core.players() > 0 then
        build()
    end
    polled += ev.dt
    if polled < 2.0 then
        return
    end
    polled = 0.0
    local open = string.sub(core.room_portal_state("door"), 1, 6) == "linked"
    for _, p in core.players() do
        if not may_enter(ROOM, p) then
            core.notice(p, "the owner of this room no longer lets you in")
            core.room_send(p, "")
        elseif not open and not told[p] then
            told[p] = true
            core.notice(p, "the way into this room is closed: /myroom leave takes you to the spawn")
        elseif open then
            told[p] = nil
        end
    end
end)
