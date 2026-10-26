--!strict
-- Every item there is, in a chest of its own over the bag, and a drag is a
-- stack of it.
--
-- The list is not written down anywhere in this mod: it is walked off the item
-- table with core.item_count and core.item_name, so a mod added tomorrow is in
-- it tomorrow and an item whose mod went away is not in it at all (item_name
-- answers empty for an orphan). That is the whole reason those two calls exist.
--
-- How to see it: press C, or right click the green box by the spawn; E or ESC
-- shuts it. It is a double chest, six rows of nine, CARRIED (core.container_show
-- below kCarriedBelow, one per player, x the player and z the box's id so it
-- takes the box's roles): its squares are "source" ones, so a drag out takes a
-- copy and the square keeps it, and what is put back is gone; one square more,
-- "trash", is a bin beside the grid for anything dropped on it. The wheel turns
-- the rows, and a scrollbar says where they are; the search box left of the
-- grid (or /creative <text>) keeps only the items whose names hold the text.
--
-- `side = both`, for the keys: a key reaches only the player's machine
-- (core:input), so that half asks the server over the mod's channel. A process
-- that is its own server does it at once.

local side    = core.side()
local serving = side ~= "client"

-- Filled in by the server half.
local open_for: (number) -> () = function(_) end
local turn: (number, number) -> () = function(_, _) end

if serving then
    core.net.open()

    local BOX = core.register_node("creative:box", 90, 190, 110, true, 4)

    local BX, BY, BZ = 42, 20, 47   -- beside the chest, on the plaza
    local Y    = -1073741824         -- kCarriedBelow: a carried container's row
    local COLS = 9
    local SHOW = COLS * 6            -- six rows, and one square more: the bin
    do
        local roles = {}
        for _ = 1, SHOW do
            table.insert(roles, "source")
        end
        table.insert(roles, "trash")
        core.container_roles(BOX, table.concat(roles, " "))
    end

    -- Every item a player could be handed, by id, in the order the engine minted
    -- them: the engine's own blocks first, then each mod's, which is the order
    -- somebody reading the list would guess.
    local items: { number } = {}

    -- Each player's own: what they searched for, how far down they turned, and
    -- what their squares hold now (to be taken out before the next rows go in).
    local query: { [number]: string } = {}
    local row: { [number]: number } = {}
    local held: { [number]: { number } } = {}

    local function shown(player: number): { number }
        local q = string.lower(query[player] or "")
        local out = {}
        for _, id in items do
            if q == "" or string.find(string.lower(core.item_name(id)), q, 1, true) then
                table.insert(out, id)
            end
        end
        return out
    end

    -- The rows from `row` on, into the player's squares.
    local function fill(player: number)
        local list = shown(player)
        local last = math.max(0, (#list - SHOW + COLS - 1) // COLS)
        local r = math.clamp(row[player] or 0, 0, last)
        row[player] = r

        for _, id in held[player] or {} do
            core.container_take(player, Y, BOX, id, 9999, "source")
        end
        local now = {}
        -- Until the squares are full rather than SHOW items on: one that no
        -- container takes (a backpack) would leave a hole.
        for i = r * COLS + 1, #list do
            if #now >= SHOW then
                break
            end
            local id = list[i]
            if core.container_add(player, Y, BOX, id, core.item_stack(id), "source") == 0 then
                table.insert(now, id)
            end
        end
        held[player] = now
        core.container_search(player, Y, BOX, query[player] or "")
        core.container_scroll(player, Y, BOX, if #list > 0 then r * COLS / #list else 0,
                              if #list > SHOW then SHOW / #list else 0)
    end

    open_for = function(player: number)
        -- Emptied first: a carried container is saved with the world, and what
        -- a catalogue held before a restart is not in `held` to be taken out.
        core.container_clear(player, Y, BOX)
        held[player] = {}
        core.container_show(player, Y, BOX, SHOW + 1, player)
        fill(player)
    end

    -- After the mods have loaded, because the item form of a block is minted then:
    -- a list built at load time is a list of the engine's own blocks and nothing
    -- else. core:world_ready is the event that says "everybody has registered".
    core.event.on("core:world_ready", function()
        items = {}
        for id = 1, core.item_count() - 1 do
            if core.item_name(id) ~= "" then
                table.insert(items, id)
            end
        end

        -- The box is the sandbox's set piece: the Inferno loads this mod for
        -- the catalogue (C), and mods/guard's square is what makes a world the sandbox.
        if core.node_id("guard:cloud") ~= 0 then
            core.set_block(BX, BY, BZ, BOX)
        end
        core.log(`creative: {#items} item(s), box at {BX} {BY} {BZ}`)
    end)

    core.event.on("core:used", function(ev)
        if ev.id ~= BOX then
            return
        end

        -- Claimed, like the chest and the table: a right click is a use before it
        -- is a placement, and without this the same click would open the catalogue
        -- and build whatever is in hand on top of the box.
        ev.cancel = true
        open_for(ev.player)
    end)

    -- The box beside the grid: narrowed on every key.
    core.event.on("core:ui_click", function(ev)
        if ev.panel ~= "core:chest" or ev.id ~= "search" or held[ev.player] == nil then
            return
        end
        query[ev.player] = string.sub(ev.text, 1, 64)
        row[ev.player] = 0
        fill(ev.player)
    end)

    core.command("creative", "[text] -- the catalogue, only the items whose names hold the text")

    core.event.on("core:command", function(ev)
        if ev.name ~= "creative" then
            return
        end
        query[ev.player] = string.sub(ev.args, 1, 64)
        row[ev.player] = 0
        open_for(ev.player)
    end)

    turn = function(player: number, by: number)
        if held[player] == nil then
            return   -- never opened: nothing to turn
        end
        row[player] = (row[player] or 0) + by
        fill(player)
    end

    core.event.on("core:net_message", function(ev)
        if ev.text == "open" then
            open_for(ev.player)
        elseif ev.text == "up" or ev.text == "down" then
            turn(ev.player, if ev.text == "up" then -1 else 1)
        end
    end)
end

if side ~= "server" then
    core.event.on("core:input", function(ev)
        local say = if ev.action == "key_c" then "open"
            elseif ev.action == "wheel_up" then "up"
            elseif ev.action == "wheel_down" then "down"
            else nil
        if say == nil then
            return
        end
        if not serving then
            core.net.send(0, say)
        elseif say == "open" then
            open_for(ev.player)
        else
            turn(ev.player, if say == "up" then -1 else 1)
        end
    end)
end
