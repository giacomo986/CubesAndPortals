--!strict
-- /me and /msg, the chat commands every player has in the classic block game.
--
-- The engine answers /help and /list itself; a mod's command is declared with
-- core.command and arrives as core:command to this mod only.

core.command("me", "<action> -- says what you do, as * name action")
core.command("msg", "<player> <text> -- to one player only")

core.event.on("core:command", function(ev)
    local me = core.player_name(ev.player)

    -- A held chat (/mute) is held here too, or /msg is the way round it.
    if core.is_muted(ev.player) == 1 then
        core.chat(ev.player, "You are muted.")
        return
    end

    if ev.name == "me" then
        if ev.args == "" then
            core.chat(ev.player, "/me <action>")
            return
        end
        core.chat(0, `* {me} {ev.args}`)
        return
    end

    if ev.name == "msg" then
        local to, text = string.match(ev.args, "^(%S+)%s+(.+)$")
        if not to then
            core.chat(ev.player, "/msg <player> <text>")
            return
        end
        for _, p in core.players() do
            if core.player_name(p) == to then
                core.chat(p, `{me} whispers to you: {text}`)
                core.chat(ev.player, `you whisper to {to}: {text}`)
                return
            end
        end
        core.chat(ev.player, `nobody called {to} in this world`)
    end
end)
