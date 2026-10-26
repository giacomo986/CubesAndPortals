--!strict
-- hunger: The classic block game's hunger bar, with its saturation and exhaustion.
--
-- Every player has twenty points of food, ten drumsticks over the health bar,
-- and a hidden saturation, at most the food. Doing things is exhaustion:
-- walking 0.01 a metre, sprinting (faster than a walk) 0.1, a jump 0.05, a
-- block broken 0.005, a blow landed 0.1, a point of health coming back 6. Four
-- of exhaustion take a point of saturation, or of food when there is none left.
-- Eating puts both back (bread 5 food and 6 saturation, cooked meat 8 and 12.8,
-- raw meat 3 and 1.8, a fried egg 4 and 4.8, an egg 2 and 1.2, a fish 2 and
-- 0.4) and is refused on a full bar. With 18 or more food, health comes back
-- on its own; under that it does not (core.player_regen); at 0 it goes, a
-- point every four seconds, down to one.
--
-- The server keeps food and saturation, by name across a restart (the mod's
-- storage), and tells the player's machine the food, which draws the bar.
local MAX = 20
local STARVE = 4
local FOOD: { [string]: { number } } = {
    ["farm:bread"] = { 5, 6 },
    ["animals:cooked_pork"] = { 8, 12.8 }, ["animals:steak"] = { 8, 12.8 },
    ["animals:pork"] = { 3, 1.8 }, ["animals:beef"] = { 3, 1.8 },
    ["animals:fried_egg"] = { 4, 4.8 }, ["animals:egg"] = { 2, 1.2 }, ["pets:fish"] = { 2, 0.4 },
    ["plants:melon_slice"] = { 2, 1.2 },
}
local WALK, SPRINT, JUMP, DIG, HIT, HEAL = 0.01, 0.1, 0.05, 0.005, 0.1, 6
local SPRINT_SPEED = 5.5   -- m/s: over the engine's walk of 4.8

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local shown = MAX   -- what the bar on this machine shows

if serving then
    core.net.open()

    type Belly = {
        points: number, saturation: number, exhaustion: number, starve: number, name: string,
        x: number?, y: number?, z: number?, rising: boolean, since: number, metres: number, health: number,
    }
    local bellies: { [number]: Belly } = {}

    local function tell(p: number, b: Belly)
        core.player_regen(p, b.points >= 18)
        core.shared_storage_set("h:" .. b.name, `{b.points} {b.saturation}`)
        if drawing then
            shown = b.points
        else
            core.net.send(p, tostring(b.points))
        end
    end

    local function belly(p: number): Belly
        local b = bellies[p]
        if b == nil then
            local name = core.player_name(p)
            local pts, sat = string.match(core.shared_storage_get("h:" .. name), "^(%S+) (%S+)$")
            b = {
                points = tonumber(pts) or MAX, saturation = tonumber(sat) or 5, exhaustion = 0,
                starve = 0, name = name, rising = false, since = 0, metres = 0, health = core.health(p),
            }
            bellies[p] = b
            tell(p, b)
        end
        return b
    end

    local function tire(p: number, amount: number)
        local b = belly(p)
        b.exhaustion += amount
        local changed = false
        while b.exhaustion >= 4 do
            b.exhaustion -= 4
            if b.saturation > 0 then
                b.saturation = math.max(0, b.saturation - 1)
            elseif b.points > 0 then
                b.points -= 1
                changed = true
            end
        end
        if changed then
            tell(p, b)
        end
    end

    core.event.on("core:tick", function(ev)
        local here: { [number]: boolean } = {}
        for _, p in core.players() do
            here[p] = true
            local b = belly(p)
            -- Where the server last heard the player was. It hears in bursts,
            -- two claims in one tick and none in the next, so walking and
            -- sprinting are told apart by the speed over a whole second.
            local x, y, z = core.player_pos(p)
            b.since += ev.dt
            if x and b.x and (x ~= b.x or y ~= b.y or z ~= b.z) then
                local dx, dz = x - (b.x :: number), z - (b.z :: number)
                local metres = math.sqrt(dx * dx + dz * dz)
                -- A step of more than a few metres is being put somewhere, not walking.
                if metres < 3 then
                    b.metres += metres
                end
                -- A jump: the feet going up after they were not.
                local dy = y - (b.y :: number)
                if dy > 0.05 and not b.rising and dy < 1 then
                    tire(p, JUMP)
                end
                b.rising = dy > 0
            end
            if x then
                b.x, b.y, b.z = x, y, z
            end
            if b.since >= 1 then
                tire(p, b.metres * (if b.metres / b.since > SPRINT_SPEED then SPRINT else WALK))
                b.metres, b.since = 0, 0
            end

            -- Health that came back on its own is paid for: a point at a time.
            -- A jump from nothing to full is joining or being put back after
            -- dying, and costs nothing (it once emptied a new player's bar).
            local health = core.health(p)
            if health > b.health and b.health > 0 and health - b.health <= 2 then
                tire(p, HEAL * (health - b.health))
            end
            b.health = health

            if b.points == 0 then
                b.starve += ev.dt
                if b.starve >= STARVE then
                    b.starve = 0
                    if core.health(p) > 1 then
                        core.damage(p, 1)
                        b.health = core.health(p)
                    end
                end
            end
        end
        for p in bellies do
            if not here[p] then
                bellies[p] = nil   -- left: an id is not reused for them
            end
        end
    end)

    core.event.on("core:broken", function(ev)
        if ev.player > 0 then
            tire(ev.player, DIG)
        end
    end)

    core.event.on("core:hit", function(ev)
        tire(ev.player, HIT)
    end)

    -- Before mods/animals and mods/farm, which heal with food when this mod
    -- is not loaded: here food fills the bar instead.
    core.event.on("core:used", -5, function(ev)
        local gives = FOOD[ev.held]
        if gives == nil then
            return
        end
        ev.cancel = true
        local b = belly(ev.player)
        if b.points >= MAX then
            core.notice(ev.player, "Not hungry")
        elseif core.take_item(ev.player, ev.held, 1) > 0 then
            b.points = math.min(MAX, b.points + gives[1])
            b.saturation = math.min(b.points, b.saturation + gives[2])
            tell(ev.player, b)
        end
    end)
end

if drawing then
    core.event.on("core:net_message", function(ev)
        shown = tonumber(ev.text) or shown
    end)

    -- Ten drumsticks over the health bar and its armour row, the engine's own
    -- measures (client/ui/hud.cpp): a slot is 0.075 of the height, the bar
    -- nine slots and eight gaps of a tenth of one wide, 0.0165 tall.
    local SLOT, PAD = 0.075, 0.0075
    local WIDE = 9 * SLOT + 8 * PAD
    local TALL = SLOT * 0.22
    local HEALTH_TOP = 1 - SLOT - 0.02 - PAD * 2 - TALL
    local Y = HEALTH_TOP - 2 * (TALL * 2) + TALL * 0.5   -- two bars up, centred
    core.hud.pass("hunger:bar", 50, function()
        local aspect = core.hud.aspect()
        -- Beside the engine's bars, and hidden with them (a chess game).
        if aspect <= 0 or not core.engine_hud_on() then
            return
        end
        local gap = TALL * 0.25
        local step = (WIDE + gap) / 10
        local w = step - gap
        core.hud.rect(0.5, Y, (WIDE + PAD * 2) / 1, TALL + PAD * 2, 20, 20, 24, 150)
        for i = 0, 9 do
            -- x is a fraction of the width, sizes of the height.
            local cx = 0.5 + (-WIDE / 2 + i * step + w / 2) / aspect
            local left = shown - i * 2
            core.hud.rect(cx, Y, w, TALL, 60, 40, 24, 255)
            if left >= 2 then
                core.hud.rect(cx, Y, w, TALL, 196, 124, 52, 255)
            elseif left == 1 then
                core.hud.rect(cx - w / 4 / aspect, Y, w / 2, TALL, 196, 124, 52, 255)
            end
        end
    end)
end
