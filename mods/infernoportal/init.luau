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
--
-- And the other way, as the Inferno's map is an eighth of this one's: a frame
-- lit in the Inferno moves the Inferno's way back (its first opening) there;
-- this world hears it (core:far_moved) and builds its own frame eight times as
-- far out, on the ground, and moves the way to the Inferno into it. Each side
-- leaves alone what only answers its own move (the far opening coming back to
-- where this one already maps).

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

-- Which of the world's openings leads to the other world: here the second,
-- to the Inferno; in the Inferno -- the world whose first opening leads to
-- the sandbox -- the first, back here.
local IN_INFERNO = core.portal_room(0) == "sandbox"
local INDEX = if IN_INFERNO then 0 else 1
local SCALE = 8   -- nodes here for one in the Inferno
-- The Inferno's arrival, its first opening's hole (games/inferno's PX, FLOOR,
-- PZ): known here because this mod loads before that one, which says it.
local ARRIVAL = { 32, 30, 32 }
-- The Inferno's own obsidian, dug there, makes a frame as well as this --
-- asked for when needed: that mod loads after this one.
local function is_obsidian(id: number): boolean
    return id == OBSIDIAN or (id ~= 0 and id == core.node_id("inferno:obsidian"))
end

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
        if not is_obsidian(core.get_block(c[1], c[2], c[3])) then return false end
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

-- The Inferno told of a frame lit there, or of it going out: its mod keeps
-- where its way back stands (games/inferno's inferno, "gate").
core.event.define("infernoportal:lit", { fields = { x = "number", y = "number", z = "number", w = "number",
                                                    h = "number", axis = "number", sign = "number" },
                                         cancellable = false })
core.event.define("infernoportal:out", { fields = {}, cancellable = false })

-- The rest only where that opening leads to the other world: a world that
-- loads these mods for the obsidian and the lighter gets no frame round it
-- and lights none.
local room = core.portal_room(INDEX)
if room ~= "" and room ~= (if IN_INFERNO then "sandbox" else "inferno") then return end

-- Where the opening stands when nobody has lit a frame: where the server put
-- it -- the plaza's, standing along z; the Inferno's arrival, along x.
local home: Gate = if IN_INFERNO
    then { x = ARRIVAL[1], y = ARRIVAL[2], z = ARRIVAL[3], axis = 0, sign = 1, w = 1, h = 2 }
    else {
        x = math.floor(core.get_portal(INDEX, 0)), y = math.floor(core.get_portal(INDEX, 1)) - 1,
        z = math.floor(core.get_portal(INDEX, 2)), axis = 1, sign = 1, w = 1, h = 2,
    }

-- Its frame, while the world is made: the plaza's gate is a Inferno portal too.
-- (The Inferno's arrival builds its own.)
if not IN_INFERNO then
    for _, c in frame_cells(home) do core.set_block(c[1], c[2], c[3], OBSIDIAN) end
    for _, c in corners(home) do core.set_block(c[1], c[2], c[3], OBSIDIAN) end
end

local lit: Gate? = nil

local function place(g: Gate): boolean
    if open_at(g) ~= 1 then return false end
    lit = g
    core.storage_set("gate", `{g.x} {g.y} {g.z} {g.axis} {g.sign} {g.w} {g.h}`)
    if IN_INFERNO then
        core.event.emit("infernoportal:lit", { x = g.x, y = g.y, z = g.z, w = g.w, h = g.h, axis = g.axis, sign = g.sign })
    end
    core.log(`infernoportal: the way {if IN_INFERNO then "back" else "to the Inferno"} is the {g.w}x{g.h} frame at {g.x} {g.y} {g.z}`)
    return true
end

-- Built here for a frame lit in the Inferno: `far` its centre there. Eight
-- times as far out, on the ground (the first solid cell from the top that is
-- not leaves), the hole cleared, the frame of obsidian, a floor of it in front
-- and behind where there is none.
type Far = { x: number, z: number, w: number, h: number, axis: number, sign: number }
local wanted: Far? = nil

local function ground(x: number, z: number): number?
    if core.loaded(x, 0, z) ~= 1 then return nil end
    for y = 250, 1, -1 do
        local id = core.get_block(x, y, z)
        if id ~= 0 and core.node_solid(id) == 1 and not string.find(core.node_name(id), "leaves", 1, true) then
            return y + 1
        end
    end
    return nil
end

local function build_far(f: Far): boolean
    local ax, az = if f.axis == 0 then 1 else 0, if f.axis == 0 then 0 else 1
    local g: Gate = { x = math.floor(f.x - ax * f.w / 2 + 0.5 * ax), y = 0,
                      z = math.floor(f.z - az * f.w / 2 + 0.5 * az), axis = f.axis, sign = f.sign, w = f.w, h = f.h }
    local y = ground(g.x, g.z)
    if y == nil then return false end
    g.y = y
    for i = 0, g.w - 1 do
        for up = 0, g.h - 1 do
            core.set_block(g.x + ax * i, g.y + up, g.z + az * i, 0)
        end
        for _, d in { -1, 1 } do
            local fx, fz = g.x + ax * i + az * d, g.z + az * i + ax * d
            if core.node_solid(core.get_block(fx, g.y - 1, fz)) == 0 then core.set_block(fx, g.y - 1, fz, OBSIDIAN) end
            for up = 0, g.h - 1 do core.set_block(fx, g.y + up, fz, 0) end
        end
    end
    for _, c in frame_cells(g) do core.set_block(c[1], c[2], c[3], OBSIDIAN) end
    for _, c in corners(g) do core.set_block(c[1], c[2], c[3], OBSIDIAN) end
    return place(g)
end

if not IN_INFERNO then
    core.event.on("core:far_moved", function(ev)
        if ev.index ~= INDEX or ev.axis > 1 then return end
        -- The Inferno's way back gone home to its arrival (its frame broken):
        -- this one home to the plaza.
        if math.abs(ev.x - (ARRIVAL[1] + 0.5)) < 1 and math.abs(ev.z - (ARRIVAL[3] + 0.5)) < 1 then
            if lit then
                lit = nil
                core.storage_set("gate", "")
                open_at(home)
                core.log("infernoportal: the Inferno's frame went out; the way is in the plaza again")
            end
            wanted = nil
            return
        end
        local x, z = ev.x * SCALE, ev.z * SCALE
        -- Only the Inferno following a frame lit here: already where it maps.
        local cx, _, cz = centre(lit or home)
        if math.abs(x - cx) <= 3 * SCALE and math.abs(z - cz) <= 3 * SCALE then return end
        local w, h = math.max(1, math.floor(ev.half_w * 2 + 0.5)), math.max(2, math.floor(ev.half_h * 2 + 0.5))
        wanted = { x = x, z = z, w = math.min(w, MAX), h = math.min(h, MAX), axis = ev.axis, sign = ev.sign }
        -- There for now, so the engine makes the ground round it (an
        -- opening's columns are always made); the frame follows.
        core.portal_place(INDEX, x, 120, z, ev.axis, ev.sign, w / 2, h / 2)
    end)
    core.event.on("core:tick", function(_)
        local f = wanted
        if f and build_far(f) then
            wanted = nil
            core.log(`infernoportal: a frame lit in the Inferno; its twin here at {math.floor(f.x)} {math.floor(f.z)}`)
        end
    end)
end

-- Put back where it was left, on the first tick: the openings exist from then.
-- In the Inferno its own mod puts the way back where it was.
local restored = IN_INFERNO
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
    if ev.held ~= LIGHTER or not is_obsidian(ev.id) then return end
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
        core.notice(ev.player, if IN_INFERNO then "the portal opens back onto the world above" else "the portal opens onto the Inferno")
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
            if IN_INFERNO then core.event.emit("infernoportal:out", {}) end
            local where = if IN_INFERNO then "the arrival" else "the plaza"
            core.log(`infernoportal: the lit frame was broken; the way is at {where} again`)
            core.notice(ev.player, if IN_INFERNO then "the portal goes out; the way back is at the arrival again"
                                   else "the portal goes out; the way to the Inferno is in the plaza again")
            return
        end
    end
end)
