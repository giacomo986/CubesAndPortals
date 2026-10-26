--!strict
-- infernoportal: the way to the Inferno is a frame a player builds.
--
-- The world's second opening is the one that leads to the Inferno (a server
-- started with --portal-to @inferno/0 as its second). It stands in the plaza in a
-- frame of obsidian until somebody builds another: a frame of obsidian round a
-- hole from one node wide and two tall up to eight by eight -- corners optional --
-- standing up either way, lit by a right click on it with a lighter. The opening
-- moves there (core.portal_place), the plaza's frame is left empty, and anybody
-- coming back from the Inferno comes out of the new one. Breaking a block of the
-- lit frame puts the opening back in the plaza. Where it is survives a restart.

local OBSIDIAN = core.register_node("infernoportal:obsidian", 22, 16, 34, true, 0, "", 1, 12.0,
                                    "stone")

-- Eight stone round a coal make two: the heat that obsidian is made by.
core.recipe_shaped("infernoportal:obsidian", 2,
    "core:stone core:stone core:stone",
    "core:stone ore:coal   core:stone",
    "core:stone core:stone core:stone")

-- Iron struck with coal: the spark.
local LIGHTER = "infernoportal:lighter"
core.register_item(LIGHTER, 1, "", 0, 1.0, "")
core.recipe_shapeless(LIGHTER, 1, "furnace:iron_ingot ore:coal")

local INDEX = 1   -- which of the world's openings leads to the Inferno

local MAX = 8      -- the widest and tallest hole a frame may hold

type Gate = { x: number, y: number, z: number, axis: number, sign: number, w: number, h: number }

-- The hole's lowest cell on its low side, which plane it stands in, and its
-- size: axis 0 is a frame along x (the opening faces z), 1 along z (it faces x).
local function step(g: Gate): (number, number)
    return if g.axis == 0 then 1 else 0, if g.axis == 0 then 0 else 1
end

local function frame_cells(g: Gate): { { number } }
    local ax, az = step(g)
    local cells = {}
    for up = 0, g.h - 1 do
        table.insert(cells, { g.x - ax, g.y + up, g.z - az })
        table.insert(cells, { g.x + ax * g.w, g.y + up, g.z + az * g.w })
    end
    for i = 0, g.w - 1 do
        table.insert(cells, { g.x + ax * i, g.y - 1, g.z + az * i })
        table.insert(cells, { g.x + ax * i, g.y + g.h, g.z + az * i })
    end
    return cells
end

local function corners(g: Gate): { { number } }
    local ax, az = step(g)
    return {
        { g.x - ax, g.y - 1, g.z - az }, { g.x + ax * g.w, g.y - 1, g.z + az * g.w },
        { g.x - ax, g.y + g.h, g.z - az }, { g.x + ax * g.w, g.y + g.h, g.z + az * g.w },
    }
end

local function whole(g: Gate): boolean
    if g.w < 1 or g.w > MAX or g.h < 2 or g.h > MAX then return false end
    local ax, az = step(g)
    for i = 0, g.w - 1 do
        for up = 0, g.h - 1 do
            if core.get_block(g.x + ax * i, g.y + up, g.z + az * i) ~= 0 then return false end
        end
    end
    for _, c in frame_cells(g) do
        if core.get_block(c[1], c[2], c[3]) ~= OBSIDIAN then return false end
    end
    return true
end

-- The hole beside the clicked block, in either plane: from each empty cell next
-- to it (diagonally too, for a corner), down to the floor and along to the low side, then measured.
local function frame_at(x: number, y: number, z: number): Gate?
    for axis = 0, 1 do
        local ax, az = if axis == 0 then 1 else 0, if axis == 0 then 0 else 1
        for _, d in { { ax, 0, az }, { -ax, 0, -az }, { 0, 1, 0 }, { 0, -1, 0 },
                      { ax, 1, az }, { -ax, 1, -az }, { ax, -1, az }, { -ax, -1, -az } } do
            local cx, cy, cz = x + d[1], y + d[2], z + d[3]
            if core.get_block(cx, cy, cz) == 0 then
                for _ = 1, MAX do
                    if core.get_block(cx, cy - 1, cz) ~= 0 then break end
                    cy -= 1
                end
                for _ = 1, MAX do
                    if core.get_block(cx - ax, cy, cz - az) ~= 0 then break end
                    cx, cz = cx - ax, cz - az
                end
                local w, h = 0, 0
                while w <= MAX and core.get_block(cx + ax * w, cy, cz + az * w) == 0 do w += 1 end
                while h <= MAX and core.get_block(cx, cy + h, cz) == 0 do h += 1 end
                local g: Gate = { x = cx, y = cy, z = cz, axis = axis, sign = 1, w = w, h = h }
                if whole(g) then return g end
            end
        end
    end
    return nil
end

local function centre(g: Gate): (number, number, number)
    local ax, az = step(g)
    return g.x + (if ax == 1 then g.w / 2 else 0.5), g.y + g.h / 2,
           g.z + (if az == 1 then g.w / 2 else 0.5)
end

local function open_at(g: Gate): number
    local x, y, z = centre(g)
    return core.portal_place(INDEX, x, y, z, g.axis, g.sign, g.w / 2, g.h / 2)
end

-- What it looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
core.register_model("infernoportal:lighter_held", "lighter_held.gltf")

-- The rest only where that opening leads to the Inferno: a world that loads
-- these mods for the obsidian and the lighter (the Inferno itself, whose second
-- opening is the track's) gets no frame round it and lights none.
local room = core.portal_room(INDEX)
if room ~= "" and room ~= "inferno" then return end

-- Where the opening stands when nobody has lit a frame: where the server put it.
local home: Gate = {
    x = math.floor(core.get_portal(INDEX, 0)), y = math.floor(core.get_portal(INDEX, 1)) - 1,
    z = math.floor(core.get_portal(INDEX, 2)), axis = 1, sign = 1, w = 1, h = 2,
}

-- Its frame, while the world is made: the plaza's gate is a Inferno portal too.
for _, c in frame_cells(home) do core.set_block(c[1], c[2], c[3], OBSIDIAN) end
for _, c in corners(home) do core.set_block(c[1], c[2], c[3], OBSIDIAN) end

local lit: Gate? = nil

local function place(g: Gate): boolean
    if open_at(g) ~= 1 then return false end
    lit = g
    core.storage_set("gate", `{g.x} {g.y} {g.z} {g.axis} {g.sign} {g.w} {g.h}`)
    core.log(`infernoportal: the way to the Inferno is the {g.w}x{g.h} frame at {g.x} {g.y} {g.z}`)
    return true
end

-- Put back where it was left, on the first tick: the openings exist from then.
local restored = false
core.event.on("core:tick", function(_)
    if restored then return end
    restored = true
    local saved = core.storage_get("gate")
    if saved == nil or saved == "" then return end
    local v = string.split(saved, " ")
    local g: Gate = { x = tonumber(v[1]) or 0, y = tonumber(v[2]) or 0, z = tonumber(v[3]) or 0,
                      axis = tonumber(v[4]) or 0, sign = tonumber(v[5]) or 1,
                      w = tonumber(v[6]) or 1, h = tonumber(v[7]) or 2 }
    if whole(g) then place(g) end
end)

core.event.on("core:used", function(ev)
    if ev.held ~= LIGHTER or ev.id ~= OBSIDIAN then return end
    local g = frame_at(ev.x, ev.y, ev.z)
    if g == nil then
        core.notice(ev.player, `that obsidian is not a whole frame round a hole 1 to {MAX} wide and 2 to {MAX} tall`)
        return
    end
    -- Its front faces whoever lit it.
    local px, _, pz = core.player_pos(ev.player)
    if px then
        local cx, _, cz = centre(g)
        g.sign = if g.axis == 0 then (if pz >= cz then 1 else -1) else (if px >= cx then 1 else -1)
    end
    ev.cancel = true
    if place(g) then
        core.notice(ev.player, "the portal opens onto the Inferno")
    else
        core.notice(ev.player, "this world has no way to the Inferno to open")
    end
end)

-- A block of the lit frame broken: the way goes back to the plaza.
core.event.on("core:broken", function(ev)
    local g = lit
    if g == nil then return end
    for _, c in frame_cells(g) do
        if c[1] == ev.x and c[2] == ev.y and c[3] == ev.z then
            lit = nil
            core.storage_set("gate", "")
            open_at(home)
            core.log("infernoportal: the lit frame was broken; the way is in the plaza again")
            core.notice(ev.player, "the portal goes out; the way to the Inferno is in the plaza again")
            return
        end
    end
end)
