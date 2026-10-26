--!strict
-- assault_escape: never shipped. tools/escape.sh puts it beside the assault's
-- mods; three seconds after the first player is in, the assault goes straight
-- to the hall and the core breaking (assault_game's "assault:to_escape"), and
-- again whenever everybody has left and somebody comes back.
local ticks = 0
local done = false
core.event.on("core:tick", function(_)
    if #core.players() == 0 then
        ticks, done = 0, false
        return
    end
    if done then return end
    ticks += 1
    if ticks >= 60 then
        done = true
        core.event.emit("assault:to_escape", {})
    end
end)
