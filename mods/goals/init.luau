--!strict
-- goals: a diary of what there is to do in the sandbox, for a player who does
-- not know where to start. Each goal is reached once per player, told with a
-- notice and a chime, and kept (core.storage "g:<name>": the ids reached).
-- /page goals lists them, reached or not, with a hint for the next ones; the
-- World page (mods/rules) links to it.
--
-- They are in the order of a first hour, so the list is a guide: a log, a
-- tool, through a gate onto another world and back, a key out of one of them
-- (and /journal to read it), a bed -- then the rest of the land, and the
-- other six keys last. The first not reached is shown small on the left
-- of the screen with its hint, the NEXT GOAL; /goals hide puts it away and
-- /goals show brings it back (kept per player, "gh:<name>"). Somebody new is
-- told once, as they arrive, what it is and how to hide it.
--
-- What reaches one: an item in the bag (looked at every two seconds), or an
-- event another mod already sends -- a bed slept in (bed:home), a pin on the
-- map (maps:pin: 3 a relic, 4 a village), back through a gate (gates:back).
local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local KEYS = { "keys:pista", "keys:inferno", "keys:arena", "keys:arpg", "keys:chambers", "keys:backrooms", "keys:parkour" }

type Goal = { id: string, title: string, hint: string, items: { string }?, pin: number?, bed: boolean?, gate: boolean? }

local GOALS: { Goal } = {
    { id = "wood", title = "Wood", hint = "Break a tree's log with your hands.",
      -- Every tree's: the plain one is trees:trunk, not timber:log.
      items = { "trees:trunk", "timber:log", "timber:mossy_log", "woods:birch_log", "woods:spruce_log",
                "woods:jungle_log", "woods:acacia_log", "woods:dark_oak_log" } },
    { id = "pick", title = "A wooden pick", hint = "Logs make planks, planks make sticks (E); four planks make a crafting table, and the pick is made on it.", items = { "tools:wood_pick" } },
    { id = "gate", title = "Through a gate", hint = "A gate, on the plaza or out on the land, opens onto a world: walk through one, and come back.",
      gate = true },
    { id = "key", title = "A key", hint = "Pass a world's trial: its key waits by the way out. /journal reads it.",
      items = KEYS },
    { id = "bed", title = "A night in bed", hint = "Three wool over three planks; sleep in it and it is your home.", bed = true },
    { id = "bread", title = "Bread", hint = "Grow wheat on tilled soil by water, then craft it.", items = { "farm:bread" } },
    { id = "iron", title = "Iron", hint = "Smelt iron ore in a furnace.", items = { "furnace:iron_ingot" } },
    { id = "fish", title = "A fish", hint = "A rod is three sticks corner to corner; fish in a lake, a river or the sea.",
      items = { "pets:fish", "pets:carp", "pets:trout", "pets:cod", "pets:eel", "pets:tuna", "pets:goldfish" } },
    { id = "map", title = "A map", hint = "Eight paper round a compass.", items = { "maps:map" } },
    { id = "village", title = "A village", hint = "Follow a path, or ask the way at a waystone.", pin = 4 },
    { id = "boat", title = "A boat", hint = "Five planks; right click water with it.", items = { "boats:boat" } },
    { id = "cart", title = "A cart", hint = "Five iron ingots in a U; it goes on a rail.", items = { "carts:cart" } },
    { id = "diamond", title = "A diamond", hint = "Deep down, near the bedrock.", items = { "ore:diamond" } },
    { id = "relic", title = "A relic", hint = "A temple in the desert, a wreck on a shore, a ruin in the hills.", pin = 3 },
    { id = "heart", title = "The guardian's heart", hint = "Deep down, in a hollow walled in bedrock: its altar.", items = { "deep:heart" } },
    { id = "key_pista", title = "Key of the Race", hint = "Win the kart track's trial.", items = { "keys:pista" } },
    { id = "key_inferno", title = "Key of Fire", hint = "Light the fortress's braziers in the inferno.", items = { "keys:inferno" } },
    { id = "key_arena", title = "Key of the Siege", hint = "Hold the arena.", items = { "keys:arena" } },
    { id = "key_arpg", title = "Key of the Crypt", hint = "Beat the lord of the crypt.", items = { "keys:arpg" } },
    { id = "key_chambers", title = "Key of the Trial", hint = "Pass the test chambers.", items = { "keys:chambers" } },
    { id = "key_backrooms", title = "Key of the Way Out", hint = "Find the backrooms' exit.", items = { "keys:backrooms" } },
    { id = "key_parkour", title = "Key of the Summit", hint = "Reach the top of the parkour.", items = { "keys:parkour" } },
}

-- ------------------------------------------------------------ the machine draws

-- The next goal of this machine's player, as the server last said: empty
-- when hidden or all reached.
local next_title, next_hint = "", ""

local function heard(text: string)
    local title, hint = string.match(text, "^n ([^|]*)|(.*)$")
    if title then
        next_title, next_hint = title, hint :: string
    end
end

if drawing then
    core.event.on("core:net_message", function(ev)
        heard(ev.text)
    end)

    -- Small, on the left, out of the way of the hotbar, the compass's dial
    -- (top middle) and the notices -- and below the debug lines (F3), which a
    -- new player has on; a shadow so it reads on snow and on sky.
    core.hud.pass("goals:next", 80, function()
        if next_title == "" or not core.engine_hud_on() then
            return
        end
        local x, y = 0.006, 0.2
        local function say(at: number, text: string, size: number, r: number, g: number, b: number)
            core.hud.text(x + 0.0012, at + 0.0018, text, size, 0, 0, 0, 0, 150)
            core.hud.text(x, at, text, size, 0, r, g, b, 235)
        end
        say(y, "Next goal:", 0.019, 200, 190, 150)
        say(y + 0.026, next_title, 0.023, 245, 235, 200)
        say(y + 0.054, next_hint, 0.018, 220, 220, 220)
        -- ponytail: the starting key; a player who moved it in Controls sees J still.
        say(y + 0.078, "J: all the goals", 0.015, 170, 170, 160)
    end)
end

-- Set further down where the server half is: one machine playing alone has both.
local show_page_here: (number) -> () = function(_) end

if drawing then
    core.register_key("goals:page", "J", "All the goals")
    core.event.on("core:input", function(ev)
        if ev.action ~= "key_goals:page" then
            return
        end
        if serving then
            show_page_here(ev.player :: number)
        else
            core.net.send(0, "page")
        end
    end)
end

if not serving then
    return
end

-- ------------------------------------------------------------ the server

core.net.open()

local function send(p: number, text: string)
    if drawing then
        heard(text)
    else
        core.net.send(p, text)
    end
end

local reached: { [string]: { [string]: boolean } } = {}   -- by player name

local function of(name: string): { [string]: boolean }
    local r = reached[name]
    if r == nil then
        r = {}
        for id in string.gmatch(core.storage_get("g:" .. name), "[^;]+") do
            r[id] = true
        end
        reached[name] = r
    end
    return r
end

local function count(r: { [string]: boolean }): number
    local n = 0
    for _, g in GOALS do
        if r[g.id] then
            n += 1
        end
    end
    return n
end

-- The first goal in the list not reached yet, or nil.
local function first_left(r: { [string]: boolean }): Goal?
    for _, g in GOALS do
        if not r[g.id] then
            return g
        end
    end
    return nil
end

-- The next goal to this player's screen, or nothing when they hid it.
local function tell_next(p: number)
    local name = core.player_name(p)
    local g = first_left(of(name))
    if g == nil or core.storage_get("gh:" .. name) == "1" then
        send(p, "n |")
    else
        send(p, `n {g.title}|{g.hint}`)
    end
end

local function reach(p: number, g: Goal)
    local name = core.player_name(p)
    local r = of(name)
    if r[g.id] then
        return
    end
    r[g.id] = true
    local ids = {}
    for id in r do
        table.insert(ids, id)
    end
    core.storage_set("g:" .. name, table.concat(ids, ";"))
    core.notice(p, `Goal reached: {g.title} ({count(r)} of {#GOALS}). /page goals`)
    local x, y, z = core.player_pos(p)
    if x then
        core.play_sound("goals:reached", x, y + 1.6, z)
    end
    tell_next(p)
end

-- Who is here, so somebody just come is told their next goal (and, the first
-- time ever, what it is).
local here: { [number]: boolean } = {}

local since = 0
core.event.on("core:tick", function(ev)
    since += ev.dt
    if since < 2 then
        return
    end
    since = 0
    local now: { [number]: boolean } = {}
    for _, p in core.players() do
        now[p] = true
        local name = core.player_name(p)
        local r = of(name)
        if not here[p] then
            tell_next(p)
            if core.storage_get("gw:" .. name) ~= "1" then
                core.storage_set("gw:" .. name, "1")
                core.notice(p, "Your next goal is on the left. /page goals lists them all; /goals hide puts it away.")
            end
        end
        for _, g in GOALS do
            if not r[g.id] and g.items then
                for _, item in g.items :: { string } do
                    if core.bag_count(p, item) > 0 then
                        reach(p, g)
                        break
                    end
                end
            end
        end
    end
    here = now
end)

core.event.on("bed:home", function(ev)
    if ev.on then
        for _, g in GOALS do
            if g.bed then
                reach(ev.player, g)
            end
        end
    end
end)

core.event.on("maps:pin", function(ev)
    for _, g in GOALS do
        if g.pin == ev.colour then
            reach(ev.player, g)
        end
    end
end)

core.event.on("gates:back", function(ev)
    for _, g in GOALS do
        if g.gate then
            reach(ev.player, g)
        end
    end
end)

core.page_register("goals", "Goals")

local function show_page(p: number)
    local r = of(core.player_name(p))
    local page = { "# Goals", `{count(r)} of {#GOALS} reached.`, "" }
    local hinted = 0
    for _, g in GOALS do
        if r[g.id] then
            table.insert(page, `| {g.title} | reached |`)
        elseif hinted < 3 then
            -- The next few say how; the rest wait.
            hinted += 1
            table.insert(page, `| {g.title} | {g.hint} |`)
        else
            table.insert(page, `| {g.title} | ... |`)
        end
    end
    table.insert(page, "")
    table.insert(page, "/goals hide or /goals show: the next goal on the left of the screen.")
    table.insert(page, "")
    table.insert(page, "[World](page:world)")
    core.page_show(p, "goals", table.concat(page, "\n"))
end

show_page_here = show_page

core.event.on("core:page_open", function(ev)
    if ev.page == "goals" then
        show_page(ev.player)
    end
end)

-- J (the key below the next goal) asks for the page from the player's machine.
core.event.on("core:net_message", function(ev)
    if ev.text == "page" then
        show_page(ev.player :: number)
    end
end)

core.command("goals", "[hide|show] -- the diary of goals; the next one on the screen or not", "")
core.event.on("core:command", function(ev)
    if ev.name ~= "goals" then
        return
    end
    local name = core.player_name(ev.player)
    local word = string.lower(string.match(ev.args, "^%s*(%S*)") or "")
    if word == "hide" or word == "show" then
        core.storage_set("gh:" .. name, if word == "hide" then "1" else "")
        tell_next(ev.player)
        core.chat(ev.player, if word == "hide" then "The next goal is hidden. /goals show brings it back."
            else "The next goal is on the left. /goals hide puts it away.")
        return
    end
    local r = of(name)
    core.chat(ev.player, `Goals: {count(r)} of {#GOALS} reached. /page goals lists them.`)
end)
