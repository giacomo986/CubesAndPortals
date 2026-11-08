--!strict
-- passport: the friend servers a player has come from, and post between them.
--
-- Every time a portal brings a player in from a friend server (--federation,
-- /friend; core:arrived) their passport takes that server's stamp, kept by
-- their identity on this server. /passport shows it: each server and how many
-- times.
--
-- A LETTERBOX sends to a player on a friend server: right click it, then
-- either a parcel -- what is in the hand, up to a stack -- or a letter, to
-- "name@server". It goes by core.fed_send to this mod there, which keeps it
-- and gives it to them the next time they are in (a parcel into the bag, a
-- letter in the chat). A friend that cannot be reached sends the parcel back
-- into the hand at once; one that is down when it goes loses it -- the line
-- between servers is not kept.

local LETTERBOX = core.register_node("passport:letterbox", 190, 40, 40, true, 0, "upright", 0, 2.0, "wood")
core.node_box(LETTERBOX, 0.15, 0.0, 0.15, 0.85, 0.95, 0.85)
core.register_item("passport:passport", 1)
core.register_model("passport:passport_held", "passport_held.gltf")   -- tools/make_held_models.py
core.recipe_shaped("passport:passport", 1, "cane:paper animals:leather cane:paper")
core.recipe_shaped("passport:letterbox", 1,
    "furnace:iron_ingot furnace:iron_ingot furnace:iron_ingot",
    "furnace:iron_ingot timber:planks      furnace:iron_ingot",
    "-                  timber:planks      -")

-- A player's own key into the mod's storage: their identity, or their name.
local function who(p: number): string
    local k = core.player_key(p)
    return string.sub(if k ~= "" then k else "name:" .. string.lower(core.player_name(p)), 1, 48)
end

-- Stamps: "server=count" by ";".
local function stamps_of(p: number): { [string]: number }
    local out = {}
    for name, n in string.gmatch(core.storage_get("stamps:" .. who(p)), "([^=;]+)=(%d+)") do
        out[name] = tonumber(n) or 0
    end
    return out
end

local function save_stamps(p: number, s: { [string]: number })
    local parts = {}
    for name, n in s do
        table.insert(parts, `{name}={n}`)
    end
    table.sort(parts)
    core.storage_set("stamps:" .. who(p), table.concat(parts, ";"))
end

core.event.on("core:arrived", function(ev)
    local s = stamps_of(ev.player)
    local first = s[ev.from] == nil
    s[ev.from] = (s[ev.from] or 0) + 1
    save_stamps(ev.player, s)
    core.notice(ev.player, if first then `A new stamp in your passport: {ev.from}` else `Passport stamped: {ev.from}`)
end)

local function show_passport(p: number)
    local s = stamps_of(p)
    local names = {}
    for name in s do
        table.insert(names, name)
    end
    table.sort(names)
    local page = { "# Passport", `> {core.player_name(p)}`, "" }
    if #names == 0 then
        table.insert(page, "No stamps yet: a portal to a friend server brings the first.")
    else
        for _, name in names do
            table.insert(page, `- **{name}**, {s[name]} time{if s[name] == 1 then "" else "s"}`)
        end
        table.insert(page, "")
        table.insert(page, `{#names} server{if #names == 1 then "" else "s"} visited.`)
    end
    core.page_show(p, "passport", table.concat(page, "\n"))
end

core.page_register("passport", "Passport")
core.command("passport", "-- the friend servers your passport is stamped by")
core.event.on("core:command", function(ev)
    if ev.name == "passport" then
        show_passport(ev.player)
    end
end)

-- ------------------------------------------------------------------ the post

local function said(item: string): string
    return (string.gsub(string.match(item, ":(.+)$") or item, "_", " "))
end

local at_box: { [number]: { number } } = {}

local function show_box(p: number, told: string?)
    local held = core.held_item(p)
    local page = { "# Letterbox", "> To a player on a friend server: name@server", "" }
    if held ~= "" then
        local n = math.min(core.bag_count(p, held), core.item_stack(core.item_id(held)))
        table.insert(page, `?[parcel of {n} {said(held)} to name@server](act:parcel {"{text}"})`)
    else
        table.insert(page, "Hold something to send it as a parcel.")
    end
    table.insert(page, `?[letter: name@server and what it says](act:letter {"{text}"})`)
    if told then
        table.insert(page, "")
        table.insert(page, `> {told}`)
    end
    core.page_show(p, "letterbox", table.concat(page, "\n"))
end

core.page_register("letterbox", "Letterbox")
core.event.on("core:used", function(ev)
    if ev.id == LETTERBOX then
        at_box[ev.player] = { ev.x, ev.y, ev.z }
        show_box(ev.player)
        ev.cancel = true
    end
end)

local function near_box(p: number): boolean
    local b = at_box[p]
    local px, py, pz = core.player_pos(p)
    return b ~= nil and px ~= nil and core.get_block(b[1], b[2], b[3]) == LETTERBOX
        and (px - b[1]) ^ 2 + (py - b[2]) ^ 2 + (pz - b[3]) ^ 2 < 8 * 8
end

core.event.on("core:page_action", function(ev)
    if ev.page ~= "letterbox" then
        return
    end
    local p = ev.player
    if not near_box(p) then
        show_box(p, "Too far from the letterbox.")
        return
    end
    local kind, rest = string.match(ev.id, "^(%a+) (.*)$")
    local to, server, words = string.match(rest or "", "^%s*([^@%s]+)@(%S+)%s*(.*)$")
    if kind == nil or to == nil or server == nil then
        show_box(p, "Write it as name@server.")
        return
    end
    local from = core.player_name(p)
    if kind == "parcel" then
        local held = core.held_item(p)
        if held == "" then
            show_box(p, "Nothing in the hand.")
            return
        end
        local n = core.take_item(p, held, math.min(core.bag_count(p, held), core.item_stack(core.item_id(held))))
        if n <= 0 then
            return
        end
        if core.fed_send(server, `parcel\t{to}\t{from}\t{held}\t{n}`) == 1 then
            show_box(p, `Sent {n} {said(held)} to {to} on {server}.`)
        else
            core.give_item(p, held, n)
            show_box(p, `No friend server called {server} takes post from here.`)
        end
    elseif kind == "letter" then
        if words == "" then
            show_box(p, "A letter needs something in it: name@server and then the words.")
        elseif core.fed_send(server, `letter\t{to}\t{from}\t{string.sub(words, 1, 600)}`) == 1 then
            show_box(p, `Your letter to {to} on {server} is on its way.`)
        else
            show_box(p, `No friend server called {server} takes post from here.`)
        end
    end
end)

-- Post that came for somebody here: kept by their name until they are in.
core.event.on("core:fed_message", function(ev)
    local kind, to = string.match(ev.text, "^(%a+)\t([^\t]+)\t")
    if kind ~= "parcel" and kind ~= "letter" or to == nil then
        return
    end
    local key = "post:" .. string.sub(string.lower(to), 1, 40)
    local kept = core.storage_get(key)
    local line = `{ev.server}\t{ev.text}`
    if #kept + #line + 1 <= 4000 then
        core.storage_set(key, if kept == "" then line else kept .. "\n" .. line)
    end
end)

local since = 0.0
core.event.on("core:tick", function(ev)
    since += ev.dt
    if since < 2 then
        return
    end
    since = 0
    for _, p in core.players() do
        local key = "post:" .. string.sub(string.lower(core.player_name(p)), 1, 40)
        local kept = core.storage_get(key)
        if kept ~= "" then
            core.storage_set(key, "")
            for line in string.gmatch(kept, "[^\n]+") do
                local server, kind, _, from, a, b = string.match(line, "^([^\t]+)\t(%a+)\t([^\t]+)\t([^\t]+)\t([^\t]+)\t?(.*)$")
                if kind == "parcel" and a then
                    local n = tonumber(b) or 0
                    if core.item_id(a) == 0 then
                        core.chat(p, `A parcel from {from} on {server}: {n} {said(a)}, which this world does not know.`)
                    else
                        local left = n - core.give_item(p, a, n)
                        if left > 0 then
                            local px, py, pz = core.player_pos(p)
                            core.drop_item(math.floor(px), math.floor(py), math.floor(pz), core.item_id(a), left)
                        end
                        core.chat(p, `A parcel from {from} on {server}: {n} {said(a)}.`)
                    end
                elseif kind == "letter" and a then
                    local words = if b ~= "" then a .. "\t" .. b else a
                    core.chat(p, `A letter from {from} on {server}: "{words}"`)
                end
            end
        end
    end
end)
