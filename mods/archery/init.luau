--!strict
-- archery: The classic block game's bow and arrows.
--
-- The right button held down with a bow in the hand draws it, as long as there
-- is an arrow in the bag; let go, it looses. How far it was drawn is the classic block game's
-- power: t seconds of a one-second draw give (t^2 + 2t) / 3, at most 1, and
-- under a tenth nothing leaves the string. The arrow leaves at 60 m/s times the
-- power, falls 20 m/s^2 and slows 0.99 a tick (core.shoot_arrow), and takes
-- ceil(6 x power) off what it strikes; a full draw is a critical, up to half as
-- much again and 2 more. Drawing slows the archer to a fifth of a walk. A bow
-- lasts 384 arrows.
--
-- An arrow stays a minute where it lands and anybody who walks up to it picks
-- it up. Skeletons leave 0-2 of them. Made as the classic block game makes them: a bow of
-- three sticks and three string, four arrows of a flint (mods/gravel), a
-- stick and a feather; string is a wool pulled apart (four).
--
-- The SERVER decides every shot: the player's machine says when the button
-- goes down and up, and draws the bow bending and the view narrowing.
local BOW, ARROW = "archery:bow", "archery:arrow"
local FULL_SPEED = 60     -- m/s, the classic block game's 3 nodes a tick
local SLOW = 0.2          -- of a walk, while drawing

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local clock = 0

-- The classic block game's power for t seconds drawn.
local function power(t: number): number
    return math.min(1, (t * t + 2 * t) / 3)
end

-- Filled in by whichever halves this process runs.
local pull: (number, boolean) -> () = function(_, _) end
local heard: (string) -> ()         = function(_) end

-- ===================================================================== server

if serving then
    core.net.open()

    core.register_item(BOW, 1, "", 0, 1.0, "", 0, "", 0, 384)
    core.register_item(ARROW, 64, "", 0, 1.0, "")
    core.register_item("archery:string", 64, "", 0, 1.0, "")
    for _, n in { "bow", "bow_pulling_0", "bow_pulling_1", "bow_pulling_2", "arrow", "string" } do
        core.register_model(`archery:{n}_held`, `{n}_held.gltf`)
    end

    core.recipe_shaped(BOW, 1,
        "-             timber:stick  archery:string",
        "timber:stick  -             archery:string",
        "-             timber:stick  archery:string")
    core.recipe_shaped(ARROW, 4,
        "-  gravel:flint     -",
        "-  timber:stick     -",
        "-  animals:feather  -")
    core.recipe_shapeless("archery:string", 4, "animals:wool")
    core.mob_drop("mobs:skeleton", ARROW, 2)

    local FLIGHT = core.model_id("mobs:arrow")
    local since: { [number]: number } = {}   -- player -> when they began to draw

    local function tell(p: number, text: string)
        if drawing then
            heard(text)
        else
            core.net.send(p, text)
        end
    end

    local function stop(p: number)
        since[p] = nil
        core.body_movement(p)
    end

    pull = function(p: number, down: boolean)
        if down then
            if core.held_item(p) ~= BOW then
                return
            end
            -- An arrow in the bag, and put back where it was.
            if core.take_item(p, ARROW, 1) == 0 then
                tell(p, "empty")
                return
            end
            core.give_item(p, ARROW, 1)
            since[p] = clock
            core.body_movement(p, 4.8 * SLOW, 4.8 * SLOW)
            tell(p, "draw")
            return
        end
        local from = since[p]
        if from == nil then
            return
        end
        stop(p)
        tell(p, "loose")
        local f = power(clock - from)
        if f < 0.1 or core.held_item(p) ~= BOW or core.take_item(p, ARROW, 1) == 0 then
            return
        end
        local damage = math.ceil(6 * f)
        if f >= 1 then
            damage += math.random(0, damage // 2 + 1)   -- a critical
        end
        core.shoot_arrow(p, FULL_SPEED * f, damage, FLIGHT, ARROW)
    end

    core.event.on("core:net_message", function(ev)
        if ev.text == "draw" or ev.text == "loose" then
            pull(ev.player, ev.text == "draw")
        end
    end)

    -- A bow put away while drawn is not drawn.
    core.event.on("core:tick", function(ev)
        clock += ev.dt
        for p in since do
            if core.held_item(p) ~= BOW then
                stop(p)
                tell(p, "loose")
            end
        end
    end)
end

-- ===================================================================== client

if drawing then
    local drawn_at: number? = nil   -- this machine's clock when the server said "draw"
    local now = 0
    local zoom = 1

    heard = function(text: string)
        if text == "draw" then
            drawn_at = now
        elseif text == "loose" then
            drawn_at = nil
        elseif text == "empty" then
            core.notice(1, "No arrows")
        end
    end

    if not serving then
        core.event.on("core:net_message", function(ev)
            heard(ev.text)
        end)
    end

    local function ask(p: number, down: boolean)
        if serving then
            pull(p, down)
        else
            core.net.send(0, if down then "draw" else "loose")
        end
    end

    -- The right button is the bow's while one is held: nothing is placed or
    -- used with it.
    core.event.on("core:input", function(ev)
        if ev.action == "mouse_right" and core.held_item(1) == BOW then
            ev.cancel = true
            ask(ev.player, true)
        elseif ev.action == "mouse_right_up" and drawn_at ~= nil then
            ask(ev.player, false)
        end
    end)

    -- The bow bends as the classic block game's does -- its first frame, then the second at
    -- 0.65 of a draw and the third at 0.9 -- and the view narrows by up to 15%,
    -- eased in and out.
    function on_step(dt: number)
        now += dt
        if not serving then
            clock = now
        end
        local want = 1
        if drawn_at ~= nil and core.held_item(1) == BOW then
            local t = now - drawn_at
            local f = math.min(t, 1)
            want = 1 - 0.15 * f * f
            core.held_model(`archery:bow_pulling_{if t < 0.65 then 0 elseif t < 0.9 then 1 else 2}_held`)
        else
            drawn_at = nil
            core.held_model("")
        end
        if math.abs(zoom - want) > 1e-3 then
            zoom += (want - zoom) * math.min(1, dt * 12)
            core.zoom(zoom)
        elseif zoom ~= want then
            zoom = want
            core.zoom(zoom)
        end
    end
end
