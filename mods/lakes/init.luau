--!strict
-- Water, where the ground lies low -- and what it does once something lets it
-- out.
--
-- Every column whose ground is under the sea level is filled with water from the
-- ground up to it: lakes in the hollows and along the valleys, none on the plaza,
-- which is at 20. Measured over 29 216 columns of seed 1: one in eleven lies low
-- enough, and none deeper than six.
--
-- A LAKE is made of sources. What comes out of one is FLOWING water, and the
-- difference is a level: a source is level 7 and never changes, a flow is 1 to 6
-- and holds only while something wetter is above it or beside it. That one rule
-- is what makes water move rather than appear:
--
--   * it falls into air under it and arrives at level 6, so a stream runs down
--     a hillside and has its whole reach again at the bottom of every drop;
--   * it spreads sideways one level weaker, and only onto something that holds
--     it up -- flowing water on water stays put -- so a flow dies six cells
--     from whatever fed it; where it meets a weaker flow it raises it, and the
--     new level runs on down that stream;
--   * and it DRAINS: a flow with nothing wetter next to it is gone on the next
--     sweep, so taking the source away empties the channel a cell at a time.
--
-- A source never sweeps -- a lake of a thousand cells costs nothing to leave
-- alone. Only flows do, and only something that opens a way (a dig, a bucket
-- poured) turns a source into a flow in the first place.

local WATER = core.register_node("lakes:water", 52, 104, 178, false, 0, "", 0, 0.0, "")

-- A liquid: a body swims in it, light goes through it, and a little more than
-- half of what is behind it is hidden.
core.node_liquid(WATER, 150)

-- Six levels of flowing water, weakest first, each one a block of its own.
--
-- A block and not a number on the cell: cell memory is a saved transaction per
-- cell and this is a thing that changes every half second, so the level lives
-- where the engine already puts a cell's state -- in which block is in it.
-- They look like water and behave like water, and each is drawn at its own
-- height (core.node_liquid's fill) with a surface sloping to its neighbours.
local SOURCE_LEVEL = 7
local FLOW: { number } = {}
local LEVEL: { [number]: number } = { [WATER] = SOURCE_LEVEL }
local FLOW_NAMES: { string } = {}

for level = 1, SOURCE_LEVEL - 1 do
    local name = `lakes:flow{level}`
    local id   = core.register_node(name, 52, 104, 178, false, 0, "", 0, 0.0, "")

    -- And how full the cell is, in eighths: level 6 is seven eighths of a cell
    -- and level 1 a quarter, so a channel gets shallower the further it runs
    -- and the client slopes its surface down to the thin end.
    core.node_liquid(id, 150, level + 1)
    FLOW[level] = id
    LEVEL[id]   = level
    table.insert(FLOW_NAMES, name)
end

-- Water stands in every cell below this. mods/trees and mods/ruins keep to the
-- ground above it.
local SEA = 18

local SIDES = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }

-- The level of whatever is in a cell, or 0 for anything that is not water.
local function level_at(x: number, y: number, z: number): number
    return LEVEL[core.get_block(x, y, z)] or 0
end

-- Whether the water in a cell goes sideways at all: not the thinnest, not over
-- air (it falls), and not a flow over water (it joins what is under it). The
-- one test step_water spreads by and fed() counts a feeder by, so a flow is
-- kept by exactly what would make it.
local function spreads(x: number, y: number, z: number, level: number): boolean
    if level <= 1 then
        return false
    end
    local below = if y > 0 then core.get_block(x, y - 1, z) else -1
    return below ~= 0 and (level == SOURCE_LEVEL or LEVEL[below] == nil)
end

-- One step of the water in a cell: down if it can, sideways if it cannot.
--
-- Water that falls does not also spread, which is the rule that keeps a
-- waterfall a column instead of a cone, and it arrives at level 6 whatever it
-- was: a stream runs down a hillside as far as the hillside goes, and at the
-- bottom of every drop it has its whole reach again. That is the rule the game
-- everybody knows keeps, and the one taken back out after a pass that let a fall
-- keep its level -- a stream then died three steps down a staircase, which was
-- reported from play as water stopping where it should have run.
--
-- Sideways only onto something that holds it up. Flowing water over air falls
-- and flowing water over water stays where it is, so a waterfall landing in a
-- pool joins it instead of spreading across its surface -- asked for in so many
-- words: "water spreads only when there is a block under it that stops it".
-- That is what keeps a lake dug into from pouring a sheet across the valley: a
-- flow on top of other water does not go anywhere. A SOURCE spreads over water
-- too, and that is the one exception, on purpose: the top of a lake is sources
-- standing on sources, and a hole dug in its side at the waterline would
-- otherwise stay dry for ever.
local function step_water(x: number, y: number, z: number)
    local level = level_at(x, y, z)
    if level == 0 then
        return
    end

    local below = if y > 0 then core.get_block(x, y - 1, z) else -1
    if below == 0 then
        core.set_block(x, y - 1, z, FLOW[SOURCE_LEVEL - 1])
        return
    end

    -- Falling onto a weaker flow is falling all the same: it arrives at level 6
    -- and the flow under it spreads from there, instead of the fall stopping on
    -- a thin stream and the stream staying thin.
    if (LEVEL[below] or SOURCE_LEVEL) < SOURCE_LEVEL - 1 then
        core.set_block(x, y - 1, z, FLOW[SOURCE_LEVEL - 1])
    end

    if not spreads(x, y, z, level) then
        return
    end

    -- Into AIR, or into a flow weaker than what it would get from here: two
    -- streams that meet join, and the stronger one raises the weaker one, which
    -- raises the cells after it on its own sweep -- the new level runs down the
    -- channel a cell a sweep, and past the old end if it has reach left. Asked
    -- for from play: a stream used to stop dead at the first flowing water.
    --
    -- And onto water as well, a lake's or a stream's: what is made there does
    -- not go further -- it joins what is under it -- so a stream that reaches a
    -- lake or a step over another stream pours into it instead of stopping at
    -- the edge, and a lake is not covered with a sheet.
    for _, side in SIDES do
        local nx, nz = x + side[1], z + side[2]
        local there = core.get_block(nx, y, nz)
        if there == 0 or (LEVEL[there] or SOURCE_LEVEL) < level - 1 then
            core.set_block(nx, y, nz, FLOW[level - 1])
        end
    end
end

-- Whether a flow still has something feeding it: any water falling into it
-- from above, or wetter water beside it that SPREADS -- not water of its own
-- level, which is two flows holding each other up and a puddle that never
-- dries, and not water that goes nowhere sideways: a row of flows lying on a
-- lake would hold itself up the same way, one of the sheets a dug lake used to
-- pour across a valley, and it dries a cell a sweep from the far end. The top
-- of a cascade stands on what it pours and is kept by the edge beside it, which
-- stands on ground; drying it on the water alone switched 3 893 cells on and
-- off at the foot of a staircase, and a flow on a lake was kept off the lake
-- for that until 2026-09-18.
local function fed(x: number, y: number, z: number, level: number): boolean
    if level_at(x, y + 1, z) > 0 then
        return true
    end
    for _, side in SIDES do
        local nx, nz = x + side[1], z + side[2]
        local there = level_at(nx, y, nz)
        if there > level and spreads(nx, y, nz, there) then
            return true
        end
    end
    return false
end

-- Flowing water, twice a second: it drains if nothing feeds it any more, and
-- goes one cell further if something does. Only flows -- a lake's sources are
-- not in this modifier's list and are never looked at.
--
-- A settled pool writes nothing: every cell it would write into already holds
-- water, so the sweep is reads only and the light is never touched again.
core.register_abm("lakes:flow", FLOW_NAMES, 0.5, 1, function(cell)
    local level = LEVEL[cell.id] or 0
    if level == 0 then
        return
    end

    if not fed(cell.x, cell.y, cell.z, level) then
        core.set_block(cell.x, cell.y, cell.z, 0)
        return
    end

    step_water(cell.x, cell.y, cell.z)
end)

-- A dig is one of the two things that open a way for water. The cell it opened
-- is dry and the water is beside it or above it, so what steps is the WATER and
-- not the hole: every wet neighbour takes one step, and whichever of them
-- reaches the hole fills it. The rest is the modifier's.
--
-- Water does not climb, so a hole dug in the bed of a lake is filled from the
-- water over it and a hole dug in a hillside is not filled at all.
local function open_way(x: number, y: number, z: number)
    if level_at(x, y + 1, z) > 0 then
        step_water(x, y + 1, z)
    end
    for _, side in SIDES do
        local nx, nz = x + side[1], z + side[2]
        if level_at(nx, y, nz) > 0 then
            step_water(nx, y, nz)
        end
    end
end

core.event.on("core:broken", function(ev)
    if core.get_block(ev.x, ev.y, ev.z) ~= 0 then
        return   -- a mod put something else there; not a hole any more
    end
    open_way(ev.x, ev.y, ev.z)
end)

-- The bucket: the other thing that opens a way for water, and the only thing
-- that MAKES a source.
--
-- Two items, because a bucket with water in it is a different thing to hold
-- than an empty one -- the same way a game everybody has played does it. It is
-- made of three iron ingots, and that recipe is in mods/furnace with the rest
-- of the iron: this mod cannot depend on that one without moving where its
-- mapgen stage runs in the order.
local EMPTY = "lakes:bucket"
local FULL  = "lakes:bucket_water"

core.register_item(EMPTY, 1, "", 0, 1.0, "")
core.register_item(FULL, 1, "", 0, 1.0, "")

-- How far a hand reaches, and how finely the look is walked.
--
-- Walked here rather than asked of core.raycast, which finds the first SOLID
-- block: water is not solid, so a ray fired at a lake comes back with the bed
-- under it or with nothing at all. What a bucket needs is the first cell of
-- WATER and the last cell of AIR before whatever stops the look, and those are
-- two questions a ray through solids cannot answer.
--
-- Six, the client's own reach: at five, water the player had aimed at from
-- above was past the end of the walk and the bucket stayed empty.
local REACH = 6.0
local STEP  = 0.2

-- What the player is pointing at, as two cells: the first source of water along
-- their look, and the last air before the look hits anything. Either may be
-- nothing.
local function pointing_at(player: number)
    local px, py, pz = core.player_pos(player)
    local lx, ly, lz = core.player_look(player)
    if px == nil or py == nil or pz == nil or lx == nil or ly == nil or lz == nil then
        return nil, nil
    end

    -- The eye, which is where a look starts. 1.62 is the engine's own number
    -- (core.player_look says so), not one this mod picked.
    local ex, ey, ez = px, py + 1.62, pz

    local source: { number }? = nil
    local air: { number }? = nil
    local last_x, last_y, last_z = -1, -1, -1

    local travelled = 0.0
    while travelled <= REACH do
        local cx = math.floor(ex + lx * travelled)
        local cy = math.floor(ey + ly * travelled)
        local cz = math.floor(ez + lz * travelled)
        travelled += STEP

        if cx ~= last_x or cy ~= last_y or cz ~= last_z then
            last_x, last_y, last_z = cx, cy, cz
            local id = core.get_block(cx, cy, cz)

            if id == 0 then
                air = { cx, cy, cz }       -- the last air is the one wanted
            elseif id == WATER then
                source = { cx, cy, cz }
                break                      -- a source stops the look
            elseif LEVEL[id] == nil then
                break                      -- anything solid stops it
            end
        end
    end

    return source, air
end

core.event.on("core:used", function(ev)
    if ev.held ~= EMPTY and ev.held ~= FULL then
        return
    end
    -- A cauldron fills and empties a bucket itself (mods/building).
    if string.find(core.node_name(ev.id), "^building:cauldron") then
        return
    end

    -- Claimed either way, like the chest and the bench: a right click is a use
    -- before it is a placement.
    ev.cancel = true

    local source, air = pointing_at(ev.player)

    if ev.held == EMPTY then
        if source == nil then
            return   -- pointing at no water: a bucket dipped in air stays empty
        end
        if core.take_item(ev.player, EMPTY, 1) == 0 then
            return
        end
        -- The cell goes first, so a full bag cannot mint water: what does not
        -- fit is handed back as an empty bucket and the water stays where it is.
        core.set_block(source[1], source[2], source[3], 0)
        if core.give_item(ev.player, FULL, 1) == 0 then
            core.set_block(source[1], source[2], source[3], WATER)
            core.give_item(ev.player, EMPTY, 1)
            return
        end
        -- And the water round it closes over the hole, as it would over a
        -- dig: a lake with a square hole in its surface where a bucket went in
        -- was a hole that stayed (seen in play, 2026-09-19).
        open_way(source[1], source[2], source[3])
        core.log(`lakes: player {ev.player} filled a bucket at {source[1]} {source[2]} {source[3]}`)
        return
    end

    if air == nil then
        return   -- nowhere to put it
    end
    if core.take_item(ev.player, FULL, 1) == 0 then
        return
    end

    core.set_block(air[1], air[2], air[3], WATER)
    core.give_item(ev.player, EMPTY, 1)

    -- And it starts moving now, not when somebody digs beside it: a bucket
    -- poured is exactly the case a source has to answer for.
    step_water(air[1], air[2], air[3])
    core.log(`lakes: player {ev.player} poured a bucket at {air[1]} {air[2]} {air[3]}`)
end)

-- Up to where the generator says the water stands (core.water_level): the
-- sea's level, or a lake's own up in the hills. The sea's where it does not say.
core.register_mapgen("lakes:fill", function(ctx)
    for z = ctx.min_z, ctx.max_z do
        for x = ctx.min_x, ctx.max_x do
            local ground = core.terrain_height(x, z)
            local top = core.water_level(x, z)
            if top == 0 then
                top = SEA
            end
            top = math.max(top, SEA)
            if ground < top then
                -- Only air: a stage before this one may have built on the
                -- ground, and a pillar in a lake is a pillar standing in water.
                for y = ground, top - 1 do
                    if core.get_block(x, y, z) == 0 then
                        core.set_block(x, y, z, WATER)
                    end
                end
            end
        end
    end
end)

-- What this does not do, and why:
--   * Nothing floods a hole that was already there when the world was made,
--     and nothing needs to: mods/cave keeps a roof of rock under the ground
--     everywhere, so no tunnel opens under a lake (seeds 1 and 7, 1024 across:
--     no source over air). A player who digs up into a lake bed opens the way
--     (core:broken above).
--   * Water is not conserved, as in the game this copies: a source is not spent
--     by feeding a flow, so a lake pouring into a channel does not fall.

-- What each item looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
for _, n in { "bucket", "bucket_water" } do
    core.register_model(`lakes:{n}_held`, `{n}_held.gltf`)
end
