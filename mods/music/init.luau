--!strict
-- music: a short tune going round each player (core.sound_loop), chosen by the
-- land they stand on (core.biome) and moved with them every tick, as the
-- weather's rain is. Four tunes, made by tools/make_loop_sounds.py: the
-- plains, the desert, the cold (tundra and peaks) and the water's edge.
-- Nothing underground, where the caves have their own sounds (lava), and
-- nothing in a world with no generator to say what land it is.
--
-- Loops of one name are one voice as loud as the nearest, so two players on
-- the plains together hear one tune, not two out of step.

local TUNE: { [string]: string } = {
    ["core:plains"] = "plains", ["core:gorge"] = "plains",
    ["core:desert"] = "desert",
    ["core:tundra"] = "cold", ["core:peaks"] = "cold",
    ["core:ocean"] = "sea", ["core:sea"] = "sea", ["core:beach"] = "sea",
    ["core:lake"] = "sea", ["core:river"] = "sea",
}
local RANGE, GAIN = 14, 0.3
local UNDER = 10   -- this far below the ground it is a cave, and quiet
local LIFT = 2     -- over the head, so turning round does not swing it
local EVERY = 2    -- seconds between looking at the land again

type Playing = { id: number, tune: string }
local playing: { [number]: Playing } = {}
local since = EVERY

local function stop(p: number)
    local m = playing[p]
    if m then
        core.sound_loop_stop(m.id)
        playing[p] = nil
    end
end

core.event.on("core:tick", function(ev)
    for p, m in playing do
        local x, y, z = core.player_pos(p)
        if x and y and z then
            core.sound_loop_move(m.id, x, y + LIFT, z)
        end
    end
    since += ev.dt
    if since < EVERY then
        return
    end
    since = 0
    local here: { [number]: boolean } = {}
    for _, p in core.players() do
        here[p] = true
        local x, y, z = core.player_pos(p)
        local tune: string? = nil
        if x and y and z then
            local fx, fz = math.floor(x), math.floor(z)
            if y > core.terrain_height(fx, fz) - UNDER then
                tune = TUNE[core.biome(fx, fz)]
            end
        end
        local m = playing[p]
        if m and m.tune ~= tune then
            stop(p)
            m = nil
        end
        if tune and m == nil and x and y and z then
            local id = core.sound_loop("music:" .. tune, x, y + LIFT, z, RANGE, GAIN)
            if id ~= 0 then
                playing[p] = { id = id, tune = tune }
            end
        end
    end
    for p in playing do
        if not here[p] then
            stop(p)
        end
    end
end)
