--!strict
-- goals: a diary of what there is to do in the sandbox, for a player who does
-- not know where to start. Each goal is reached once per player, told with a
-- notice and a chime, and kept (core.storage "g:<name>": the ids reached).
-- /page goals lists them, reached or not, with a hint for the next ones; the
-- World page (mods/rules) links to it.
--
-- What reaches one: an item in the bag (looked at every two seconds), or an
-- event another mod already sends -- a bed slept in (bed:home), a pin on the
-- map (maps:pin: 1 a portal, 3 a relic, 4 a village).
if core.side() == "client" then
    return
end

type Goal = { id: string, title: string, hint: string, items: { string }?, pin: number?, bed: boolean? }

local GOALS: { Goal } = {
    { id = "wood", title = "Wood", hint = "Break a tree's log with your hands.", items = { "timber:log" } },
    { id = "pick", title = "A wooden pick", hint = "Craft one from planks and sticks (E).", items = { "tools:wood_pick" } },
    { id = "bread", title = "Bread", hint = "Grow wheat on tilled soil by water, then craft it.", items = { "farm:bread" } },
    { id = "bed", title = "A night in bed", hint = "Craft a bed and sleep in it: it is your home then.", bed = true },
    { id = "iron", title = "Iron", hint = "Smelt iron ore in a furnace.", items = { "furnace:iron_ingot" } },
    { id = "fish", title = "A fish", hint = "Fish in a lake, a river or the sea.",
      items = { "pets:fish", "pets:carp", "pets:trout", "pets:cod", "pets:eel", "pets:tuna", "pets:goldfish" } },
    { id = "map", title = "A map", hint = "Eight paper round a compass.", items = { "maps:map" } },
    { id = "village", title = "A village", hint = "Follow a path, or ask the way at a waystone.", pin = 4 },
    { id = "boat", title = "A boat", hint = "Five planks; right click water with it.", items = { "boats:boat" } },
    { id = "cart", title = "A cart", hint = "Iron round a chest; it goes on a rail.", items = { "carts:cart" } },
    { id = "diamond", title = "A diamond", hint = "Deep down, near the bedrock.", items = { "ore:diamond" } },
    { id = "relic", title = "A relic", hint = "A temple in the desert, a wreck on a shore, a ruin in the hills.", pin = 3 },
    { id = "portal", title = "A portal", hint = "One stands on the plaza for every world; out on the land too.", pin = 1 },
    { id = "heart", title = "The guardian's heart", hint = "Under the bedrock, in a hollow: its altar.", items = { "deep:heart" } },
    { id = "key_pista", title = "Key of the Race", hint = "Win the kart track's trial.", items = { "keys:pista" } },
    { id = "key_inferno", title = "Key of Fire", hint = "Light the fortress's braziers in the inferno.", items = { "keys:inferno" } },
    { id = "key_arena", title = "Key of the Siege", hint = "Hold the arena.", items = { "keys:arena" } },
    { id = "key_arpg", title = "Key of the Crypt", hint = "Beat the lord of the crypt.", items = { "keys:arpg" } },
    { id = "key_chambers", title = "Key of the Trial", hint = "Pass the test chambers.", items = { "keys:chambers" } },
    { id = "key_backrooms", title = "Key of the Way Out", hint = "Find the backrooms' exit.", items = { "keys:backrooms" } },
    { id = "key_parkour", title = "Key of the Summit", hint = "Reach the top of the parkour.", items = { "keys:parkour" } },
}

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
end

local since = 0
core.event.on("core:tick", function(ev)
    since += ev.dt
    if since < 2 then
        return
    end
    since = 0
    for _, p in core.players() do
        local r = of(core.player_name(p))
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

core.page_register("goals", "Goals")

core.event.on("core:page_open", function(ev)
    if ev.page ~= "goals" then
        return
    end
    local r = of(core.player_name(ev.player))
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
    table.insert(page, "[World](page:world)")
    core.page_show(ev.player, "goals", table.concat(page, "\n"))
end)

core.command("goals", "-- the diary of goals", "")
core.event.on("core:command", function(ev)
    if ev.name == "goals" then
        local r = of(core.player_name(ev.player))
        core.chat(ev.player, `Goals: {count(r)} of {#GOALS} reached. /page goals lists them.`)
    end
end)
