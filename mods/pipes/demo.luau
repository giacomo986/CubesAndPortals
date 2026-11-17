--!strict
-- Two working pipe networks on the plaza, east of the spawn and clear of the
-- gates (mods/gates' SPOTS), that run by themselves for whoever walks by:
--
--   SORTING (z 45..52). A supply chest and an extractor send cobblestone, dirt
--   and planks down a pipe. Cobblestone goes to a responder (blue) -- the
--   short way is shut by a firewall that names it, so it goes round the loop;
--   dirt to a terminator (purple); planks, which nobody asked for, to an
--   overflow (orange).
--   REQUESTS (z 54..58). A storage chest with a broadcaster. Right click the
--   request pipe for what it holds: it comes to the chest beside the pipe. A
--   stock keeper on the same network keeps a stack of sand in its chest.
--
-- Every few seconds the supplies are topped up and full chests at the far ends
-- emptied, so it never stops. Cheap blocks only, as the chests are open to
-- anybody. Built every start, only its own cells. Signs (mods/building's,
-- read by looking at them) say what each part does, and a pressure plate
-- (mods/cinnabar's) builds the whole thing again: pipes, chips and chests as
-- they were, nothing on its way.

local X, Z = 66, 46           -- the corner the two stand from: clear of the palette wall (z 44) and the Inferno gate (x 57..63)
local REFILL = 15             -- seconds between top-ups

local function n(name: string): number
    local id = core.node_id(name)
    assert(id ~= 0, `pipes demo: no block {name}`)
    return id
end
local function item(name: string): number
    local id = core.item_id(name)
    assert(id ~= 0, `pipes demo: no item {name}`)
    return id
end

local y = 0
local supply, sinks, storage, output, keeper = {}, {}, {}, {}, {}

local function put(x: number, z: number, name: string)
    core.set_block(x, y, z, n(name))
end
local function chest(x: number, z: number): { number }
    put(x, z, "chest:box")
    core.container_clear(x, y, z)
    core.container_make(x, y, z, 27)
    return { x, y, z }
end
-- An interface pipe holding one chip and up to eight filter things.
local function interface(x: number, z: number, chip: string, filter: { string })
    put(x, z, "pipes:interface_pipe")
    core.container_clear(x, y, z)
    core.container_make(x, y, z, 36)
    core.container_add(x, y, z, item(`pipes:{chip}_chip`), 1)
    for _, f in filter do core.container_add(x, y, z, item(f), 1) end
end
local function line(x0: number, z0: number, x1: number, z1: number, name: string)
    for x = math.min(x0, x1), math.max(x0, x1) do
        for z = math.min(z0, z1), math.max(z0, z1) do put(x, z, name) end
    end
end

-- A standing sign facing the spawn (west), its lines each a text of
-- assets/lang/it.txt: the hud translates a sign line by line.
local SIGN = "building:oak_standing_sign"
local function sign(x: number, z: number, lines: { string })
    if core.node_id(SIGN) == 0 then return end   -- a world without mods/building
    put(x, z, SIGN)
    core.set_facing(x, y, z, 3)
    core.meta_set(x, y, z, "text", table.concat(lines, "\n"))
end

local PLATE = { X - 1, Z + 7 }
local function build()
    y = core.terrain_height(X, Z)
    sinks = {}
    core.event.emit("pipes:clear", { x0 = X - 1, z0 = Z - 1, x1 = X + 11, z1 = Z + 12 })
    -- Its own ground clear, three high.
    for x = X - 1, X + 11 do
        for z = Z - 1, Z + 12 do
            if x ~= PLATE[1] or z ~= PLATE[2] then
                for dy = 0, 2 do core.set_block(x, y + dy, z, 0) end
            end
        end
    end

    -- SORTING. Row Z+2 is the main line; Z+4 the way round the firewall.
    local m = Z + 2
    supply = chest(X, m)
    interface(X + 1, m, "extractor", {})
    line(X + 2, m, X + 3, m, "pipes:item_pipe")
    put(X + 4, m, "pipes:junction_pipe")
    put(X + 5, m, "pipes:item_pipe")
    put(X + 6, m, "pipes:firewall_pipe")
    core.container_clear(X + 6, y, m)
    core.container_make(X + 6, y, m, 9)
    core.container_add(X + 6, y, m, item("rocks:cobblestone"), 1)
    put(X + 7, m, "pipes:junction_pipe")
    interface(X + 8, m, "responder", { "rocks:cobblestone" })
    table.insert(sinks, chest(X + 9, m))
    -- Round the firewall.
    put(X + 4, m + 1, "pipes:item_pipe")
    put(X + 4, m + 2, "pipes:junction_pipe")
    line(X + 5, m + 2, X + 6, m + 2, "pipes:item_pipe")
    put(X + 7, m + 2, "pipes:junction_pipe")
    put(X + 7, m + 1, "pipes:item_pipe")
    -- Dirt up from the first junction, planks down from the last.
    interface(X + 4, m - 1, "terminator", { "core:dirt" })
    table.insert(sinks, chest(X + 4, m - 2))
    interface(X + 7, m + 3, "overflow", {})
    table.insert(sinks, chest(X + 7, m + 4))

    -- REQUESTS, along Z+9.
    local r = Z + 9
    storage = chest(X, r)
    interface(X + 1, r, "broadcaster", {})
    line(X + 2, r, X + 4, r, "pipes:item_pipe")
    put(X + 5, r, "pipes:junction_pipe")
    put(X + 6, r, "pipes:item_pipe")
    put(X + 7, r, "pipes:request_pipe")
    output = chest(X + 8, r)
    interface(X + 5, r + 1, "stock_keeper", { "core:sand" })
    keeper = chest(X + 5, r + 2)

    -- What it all is, from the spawn's side.
    sign(X - 1, Z + 1, { "Sorting with pipes", "Extractor: empties this chest", "Cobblestone: responder, blue",
                         "Dirt: terminator, purple", "Planks: overflow, orange" })
    sign(X + 6, Z + 1, { "Firewall: no cobblestone", "It goes round the loop" })
    sign(X - 1, Z + 10, { "Requests with pipes", "Broadcaster: offers this chest", "Right click the request pipe",
                          "Stock keeper: a stack of sand" })
    sign(X - 1, Z + 6, { "Step on the plate", "to reset the demo" })
    -- The plate is left as it is once there: put up again under whoever is on it,
    -- it would go down again and reset again.
    local at = core.node_name(core.get_block(PLATE[1], y, PLATE[2]))
    if core.node_id("cinnabar:plate") ~= 0 and at ~= "cinnabar:plate" and at ~= "cinnabar:plate_on" then
        put(PLATE[1], PLATE[2], "cinnabar:plate")
    end

    -- The shapes, now that every neighbour is there.
    for x = X, X + 9 do
        for z = Z, Z + 11 do
            local id = core.get_block(x, y, z)
            if id ~= 0 then core.event.emit("pipes:refresh", { x = x, y = y, z = z }) end
        end
    end
end

local function top_up(c: { number }, things: { string }, want: number)
    for _, t in things do
        local id = item(t)
        local have = core.container_count(c[1], c[2], c[3], id)
        if have < want then core.container_add(c[1], c[2], c[3], id, want - have) end
    end
end
local function empty_if_full(c: { number }, past: number)
    if core.container_fill(c[1], c[2], c[3]) > past then
        core.container_clear(c[1], c[2], c[3])
        core.container_make(c[1], c[2], c[3], 27)
    end
end

local function tend()
    top_up(supply, { "rocks:cobblestone", "core:dirt", "timber:planks" }, 16)
    top_up(storage, { "rocks:cobblestone", "core:dirt", "timber:planks", "core:glass" }, 64)
    top_up(storage, { "core:sand" }, 128)   -- the stock keeper's stack, and some to ask for
    for _, c in sinks do empty_if_full(c, 0.02) end   -- about 35 things in a chest of 27
    empty_if_full(output, 0.5)
    empty_if_full(keeper, 0.02)   -- a full stack of sand: gone, and asked for again
end

local clock, tended, looked = 0, -REFILL, 0
local pressed = false   -- a player is on the plate, or was less than a second ago: one reset per step
local left_at = 0       -- when the plate was last seen with nobody on it
local COOLDOWN = 5      -- seconds after a reset before the plate counts again
local reset_at = -COOLDOWN

-- The player standing on the plate, if any. Not the plate's own state: it goes
-- down under the plaza's animals too, and a cow grazing on its edge reset the
-- demo every few seconds.
local function on_plate(): number?
    for _, p in core.players() do
        local px, py, pz = core.player_pos(p)
        if px and py and pz and math.floor(px) == PLATE[1] and math.floor(pz) == PLATE[2]
            and py >= y - 0.1 and py <= y + 0.5 then
            return p
        end
    end
    return nil
end
local built = false
core.event.on("core:tick", function(ev)
    if not built then
        built = true
        -- Only the sandbox's plaza: not the pocket halls, whose mods these are too.
        if core.room() ~= "sandbox" and core.room() ~= "" then return end
        build()
    end
    if y == 0 then return end
    clock += ev.dt
    if clock - tended >= REFILL then
        tended = clock
        tend()
    end
    if clock - looked < 0.25 then return end
    looked = clock
    local who = on_plate()
    if who == nil and pressed and clock - left_at >= 1 then pressed = false end
    if who == nil then return end
    left_at = clock
    if not pressed and clock - reset_at >= COOLDOWN then
        reset_at = clock
        build()
        tended = clock
        tend()
        core.log(`pipes demo: reset by {core.player_name(who)}`)
        for _, p in core.players() do
            local px, _, pz = core.player_pos(p)
            if px and pz and math.abs(px - X) < 16 and math.abs(pz - Z - 6) < 16 then core.notice(p, "Pipes demo reset") end
        end
    end
    pressed = true
end)
