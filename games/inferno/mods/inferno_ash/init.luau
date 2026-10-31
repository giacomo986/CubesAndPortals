--!strict
-- inferno_ash: the Inferno's air, on a player's machine and nowhere else:
-- flakes of grey ash drifting down and now and then an ember going up, in a
-- box round the eye that follows it. Each flake is a fixed point of a lattice
-- shifted by time and wrapped into the box, so nothing is kept from frame to
-- frame and every camera (a portal's too) sees the same air.

local N = 160      -- flakes
local R = 11       -- the box's half side
local t = 0.0
core.event.on("core:tick", function(ev)
    t += ev.dt
end)

local function frac(v: number): number
    return v - math.floor(v)
end

-- Into -R .. R round the eye's c.
local function wrap(p: number, c: number): number
    return ((p - c + R) % (2 * R)) - R + c
end

core.render.pass("inferno_ash:air", 50, function()
    local cx, cy, cz = core.cursor_ray()
    if cx == nil then return end
    for i = 1, N do
        local a, b, c = frac(i * 0.6180339), frac(i * 0.4142136), frac(i * 0.7320508)
        local ember = i % 9 == 0
        local rise = if ember then 0.9 else -0.35 - 0.2 * c
        local sway = math.sin(t * (0.4 + b) + i) * 0.8
        local x = wrap(a * 2 * R * 7 + sway + t * 0.25, cx)
        local y = wrap(b * 2 * R * 5 + t * rise, cy)
        local z = wrap(c * 2 * R * 3 + math.cos(t * 0.3 + i) * 0.8, cz)
        if ember then
            core.render.fx("glow", x, y, z, 0.07, 255, 130, 40, 220, 0.5)
        else
            core.render.box(x, y, z, 0.05, 0.05, 0.05, 92, 86, 82, 200)
        end
    end
end)
