--!strict
-- assault_test: never shipped. tools/track.sh --fresh puts it beside the
-- assault's mods, and it tells assault_game to start every game with 100000
-- credits, to try the shop with.
local said = false
core.event.on("core:tick", function(_)
    if said then return end
    said = true
    core.event.emit("assault:test_credits", { n = 100000 })
end)
