--!strict
-- rules: the world's settings in one place.
--
--   /difficulty peaceful|easy|normal|hard   core.set_difficulty (the engine's)
--   /peaceful [on|off]                      the same, peaceful or normal
--   /rule [NAME on|off]                     core.set_rule, for the rules below
--   /page world                             all of it, and the weather, the
--                                           hour and the season, as buttons
--
-- The engine gives a rule no meaning: each mod reads its own with core.rule,
-- and DECLARES it here so /rule and the World page list it, in whatever world
-- that mod is loaded and nowhere else:
--
--   core.event.on("core:world_ready", function()
--       pcall(core.event.emit, "rules:declare", { name = "hunger", default = "on",
--           what = "Hunger", survival = false })
--   end)
--
-- (pcall: in a world without this mod there is no such event.) `default` is
-- what an empty value means, "on" or "off"; `survival` true hides the rule in
-- a world with a plaza. A rule declared twice is the later one.
if core.side() == "client" then
    return
end

type Rule = { name: string, default: string, what: string, survival: boolean? }
local RULES: { Rule } = {
    { name = "day_cycle", default = "on", what = "The sun moves (day and night)" },
}

core.event.define("rules:declare", {
    fields = { name = "string", default = "string", what = "string", survival = "boolean" },
    cancellable = false,
})
core.event.on("rules:declare", function(ev)
    if not string.match(ev.name, "^[a-z][a-z0-9_]*$") or (ev.default ~= "on" and ev.default ~= "off") then
        core.log(`rules: '{ev.name}' not declared: a name is a-z, 0-9 and _, a default on or off`)
        return
    end
    local r: Rule = { name = ev.name, default = ev.default, what = ev.what, survival = ev.survival }
    for i, old in RULES do
        if old.name == r.name then
            RULES[i] = r
            return
        end
    end
    table.insert(RULES, r)
    table.sort(RULES, function(a: Rule, b: Rule): boolean
        return a.name < b.name
    end)
end)

local function value(r): string
    local v = core.rule(r.name)
    return if v == "" then r.default else v
end

local function shown(r): boolean
    return not r.survival or core.plaza() == nil
end

local DIFFICULTIES = {
    { "peaceful", "Peaceful: no monsters" },
    { "easy", "Easy: few, weak monsters" },
    { "normal", "Normal" },
    { "hard", "Hard: many, strong monsters" },
}

local function admin(p: number): boolean
    return core.has_perm(p, "admin") == 1
end

-- What a rule changing does at once, besides being kept.
local function apply(name: string)
    if name == "day_cycle" then
        core.set_day_cycle(core.rule("day_cycle") ~= "off")
    end
end

-- At the first tick: before it there is no clock to stop (core:world_ready
-- comes before the server's sessions).
local started = false
core.event.on("core:tick", function()
    if not started then
        started = true
        apply("day_cycle")
    end
end)

local function set_difficulty(p: number, name: string)
    local gone = core.set_difficulty(name)
    if gone < 0 then
        core.chat(p, "Usage: /difficulty peaceful|easy|normal|hard")
    elseif name == "peaceful" then
        core.chat(p, `Difficulty: peaceful, no monsters ({gone} went).`)
    else
        core.chat(p, `Difficulty: {name}.`)
    end
end

core.command("difficulty", "[peaceful|easy|normal|hard] -- how hard this world is", "admin")
core.command("peaceful", "[on|off] -- no monsters in this world, or monsters again", "admin")
core.command("rule", "[NAME on|off] -- the world's rules", "admin")
core.event.on("core:command", function(ev)
    local word = string.lower(string.match(ev.args, "^%s*(%S*)") or "")
    if ev.name == "difficulty" then
        if word == "" then
            core.chat(ev.player, `Difficulty: {core.difficulty()}. /difficulty peaceful|easy|normal|hard`)
        else
            set_difficulty(ev.player, word)
        end
    elseif ev.name == "peaceful" then
        if word == "on" or word == "off" then
            set_difficulty(ev.player, if word == "on" then "peaceful" else "normal")
        elseif word == "" then
            core.chat(ev.player, if core.peaceful() then "Peaceful. /peaceful off brings monsters back."
                else "Not peaceful. /peaceful on sends the monsters away.")
        else
            core.chat(ev.player, "Usage: /peaceful [on|off]")
        end
    elseif ev.name == "rule" then
        local name, v = string.match(ev.args, "^%s*(%S+)%s+(%S+)%s*$")
        if name == nil then
            for _, r in RULES do
                if shown(r) then
                    core.chat(ev.player, `{r.name} = {value(r)} -- {r.what}`)
                end
            end
            return
        end
        v = string.lower(v)
        for _, r in RULES do
            if r.name == name and (v == "on" or v == "off") then
                core.set_rule(name, if v == r.default then "" else v)
                apply(name)
                core.chat(ev.player, `{name} = {v}.`)
                return
            end
        end
        core.chat(ev.player, "Usage: /rule NAME on|off (/rule lists them)")
    end
end)

-- The World page: everybody reads it, an admin's links change it. Opened from
-- the pause menu's World button (/page world).
core.page_register("world", "World")

local function show(p: number)
    local can = admin(p)
    local function choice(label: string, cmd: string, on: boolean): string
        if on then
            return `{label} (now)`
        end
        return if can then `[{label}](cmd:{cmd})` else label
    end
    -- A world with the sandbox's mods has mods/weather, which declares its rule.
    local sandbox = false
    for _, r in RULES do
        sandbox = sandbox or r.name == "weather_cycle"
    end
    local page = { "# This world", "", "## Difficulty" }
    for _, d in DIFFICULTIES do
        table.insert(page, choice(d[2], `/difficulty {d[1]}`, core.difficulty() == d[1]))
    end
    table.insert(page, "> Peaceful: a dungeon's spawner leaves its creatures' drops, the deep's altar its heart.")
    table.insert(page, "")
    table.insert(page, "## Rules")
    for _, r in RULES do
        if shown(r) then
            local v = value(r)
            table.insert(page, `| {r.what} | {choice("on", `/rule {r.name} on`, v == "on")} | {choice("off", `/rule {r.name} off`, v == "off")} |`)
        end
    end
    if can then
        -- The weather and the season are mods/weather's, which declares the
        -- rule weather_cycle where it is loaded: a themed world has neither.
        local weather = sandbox
        table.insert(page, "")
        table.insert(page, "## Now")
        if weather then
            table.insert(page, `Weather: [clear](cmd:/weather clear) [rain](cmd:/weather rain) [storm](cmd:/weather storm)`)
        end
        table.insert(page, `Time: [dawn](cmd:/time dawn) [noon](cmd:/time noon) [dusk](cmd:/time dusk) [midnight](cmd:/time midnight)`)
        if weather then
            table.insert(page, `Season: [spring](cmd:/season spring) [summer](cmd:/season summer) [autumn](cmd:/season autumn) [winter](cmd:/season winter)`)
        end
    else
        table.insert(page, "")
        table.insert(page, "> Only an admin changes these.")
    end
    if sandbox then   -- mods/goals is the sandbox's
        table.insert(page, "")
        table.insert(page, "[Goals](page:goals)")
    end
    core.page_show(p, "world", table.concat(page, "\n"))
end

core.event.on("core:page_open", function(ev)
    if ev.page == "world" then
        show(ev.player)
    end
end)
