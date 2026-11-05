--!strict
-- bees: the smoker's smoke, drawn on each player's machine. Part of the mod
-- (init.luau requires it wherever there is drawing).
--
-- A puff told by the server (apiary.luau, common.smoke) -- "smoke x y z" over
-- the mod's channel, or straight to common.puff where one process serves and
-- draws -- is a dozen grey puffs rising out of the smoker's spout to the
-- hive and drifting up round it over four seconds (core.render.fx's
-- "smoke", mixed over what is behind).

local common = require("./common")

type Puff = { x: number, y: number, z: number, dx: number, dz: number, rise: number, size: number,
              age: number, wait: number, life: number, grey: number }
local puffs: { Puff } = {}

function common.puff(x: number, y: number, z: number)
    for i = 1, 12 do
        table.insert(puffs, {
            x = x + (math.random() - 0.5) * 0.6, y = y + (math.random() - 0.5) * 0.4,
            z = z + (math.random() - 0.5) * 0.6,
            dx = (math.random() - 0.5) * 0.5, dz = (math.random() - 0.5) * 0.5,
            rise = 0.35 + math.random() * 0.35, size = 0.35 + math.random() * 0.3,
            age = 0, wait = i * 0.08, life = 2.5 + math.random() * 1.5, grey = 150 + math.random(60),
        })
    end
    while #puffs > 240 do
        table.remove(puffs, 1)
    end
end

core.event.on("core:net_message", function(ev)
    local x, y, z = string.match(ev.text, "^smoke (%S+) (%S+) (%S+)$")
    if x then
        common.puff(tonumber(x) or 0, tonumber(y) or 0, tonumber(z) or 0)
    end
end)

core.event.on("core:tick", function(ev)
    for i = #puffs, 1, -1 do
        local p = puffs[i]
        if p.wait > 0 then
            p.wait -= ev.dt
        else
            p.age += ev.dt
            p.x += p.dx * ev.dt
            p.z += p.dz * ev.dt
            p.y += p.rise * ev.dt
            if p.age >= p.life then
                table.remove(puffs, i)
            end
        end
    end
end)

core.render.pass("bees:smoke", 40, function()
    for _, p in puffs do
        if p.wait <= 0 then
            local k = p.age / p.life
            core.render.fx("smoke", p.x, p.y, p.z, p.size * (1 + k * 1.5), p.grey, p.grey, p.grey + 8, 200, k)
        end
    end
end)
