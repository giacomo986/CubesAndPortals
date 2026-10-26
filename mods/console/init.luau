--!strict
-- A panel, declared from Lua.
--
-- What this mod proves is narrow and worth stating: every rectangle, every
-- caption and every button below is described HERE, and the engine draws them
-- without knowing what they are for. The clicks come back on the same event bus
-- every other event uses -- there is no privileged path for a UI.
--
-- How to see it: break the switch block near the spawn. It cannot be broken --
-- the mod puts it straight back -- and breaking it is what opens the panel. It
-- works in a single-process client and over a socket: the declaration crosses
-- the wire as net::PanelShow and the click comes back as net::UiClick.
--
-- The panel goes to the person who broke the switch and to nobody else: the
-- events carry a player id and ui_show takes one.

local SWITCH = core.register_node("console:switch", 250, 200, 60, true, 6)
local MARK   = core.register_node("console:mark", 90, 200, 240, true, 10)

-- Where the switch sits. Beside the spawn, on the ground, so it is the first
-- thing in front of a player who has just arrived.
local SX, SY, SZ = 38, 20, 47

local PANEL = "console:menu"

-- What the buttons have done so far. The panel is redeclared to show it, which
-- is the honest v1 answer to "how does a label change": there is no way to set
-- one, so the mod says the whole panel again. It is cheap -- a handful of
-- strings -- and it is written down as a limitation rather than hidden.
local built = 0

-- Who has it up. A number the events hand this mod and it hands straight back
-- to ui_show; the mod never invents a player id and has no table of them.
-- Zero means nobody, which is also what ui_show reads as "everybody" -- so the
-- redeclaration below is careful not to reopen it for the world.
local viewer = 0

local function say()
    -- Six cells wide, five tall. A cell is a hotbar slot, so this is about a
    -- third of the screen either way, whatever the screen is.
    if core.ui_define(PANEL, 8.0, 6.5) == 0 then
        return
    end

    core.ui_box(PANEL, 0.2, 0.2, 7.6, 0.9, 40, 44, 56)
    core.ui_label(PANEL, 0.5, 0.45, "mods/console -- an example panel")
    core.ui_label(PANEL, 0.5, 1.4, "you broke the yellow switch block.")
    core.ui_label(PANEL, 0.5, 2.0, "every word here is written in Lua.")
    core.ui_label(PANEL, 0.5, 2.9, `blocks placed from here: {built}`)

    core.ui_button(PANEL, "build", 0.5, 3.7, 3.5, 0.9, "place a blue block")
    core.ui_button(PANEL, "clear", 4.2, 3.7, 3.3, 0.9, "take them away")
    core.ui_button(PANEL, "close", 0.5, 5.0, 7.0, 0.9, "close this panel")

    -- A redeclaration does not take the panel off anybody's screen, so this is
    -- only here for the case where it was never on one.
    if viewer ~= 0 then
        core.ui_show(PANEL, true, viewer)
    end
end

-- The switch, put where a player will walk into it. Placed at load, and put
-- back every time somebody breaks it: a switch that disappears when you use it
-- is a switch you can use once.
core.set_block(SX, SY, SZ, SWITCH)
say()

core.event.on("core:broken", function(ev)
    if ev.x ~= SX or ev.y ~= SY or ev.z ~= SZ then
        return
    end

    core.set_block(SX, SY, SZ, SWITCH)

    -- To the person who broke it, and to nobody else. `ev.player` is the id the
    -- engine put on the event; on a server it is a session, in the
    -- single-process client it is the one player there is.
    viewer = ev.player
    core.ui_show(PANEL, true, viewer)
end)

-- And the clicks. The same bus core:broken arrives on: a UI event is not a
-- special kind of event.
core.event.on("core:ui_click", function(ev)
    if ev.panel ~= PANEL then
        return
    end

    if ev.id == "close" then
        core.ui_show(PANEL, false, ev.player)
        if viewer == ev.player then
            viewer = 0
        end
        return
    end

    if ev.id == "build" then
        -- A column beside the switch, one block per click.
        core.set_block(SX + 2, SY + built, SZ, MARK)
        built += 1
        viewer = ev.player
        say()
        return
    end

    if ev.id == "clear" then
        for i = 0, built - 1 do
            core.set_block(SX + 2, SY + i, SZ, 0)
        end
        built = 0
        viewer = ev.player
        say()
        return
    end
end)

core.log(`console: panel '{PANEL}' declared, switch at {SX} {SY} {SZ}`)
