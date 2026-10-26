--!strict
-- Twenty blocks, declared in a loop, and a wall built out of them. Nothing in
-- the engine knows any of these exist: the ids come back from the registry, the
-- atlas grows to fit them, and the mesher and the lighting read what the
-- registry says.
--
-- Edit a colour and save: the file watcher reloads this mod inside half a
-- second and the wall changes without the world being regenerated (F5 forces
-- it). The ids stay the same, because the registry keeps a name's id when the
-- name is registered again.

local COUNT = 20

local BASE_X = 60
local BASE_Y = 20
local BASE_Z = 44

local ids: {number} = {}

-- A hue ramp, so twenty blocks are twenty visibly different blocks rather than
-- twenty greys. Written out by hand: no colour library, no toolchain.
local function ramp(i: number): (number, number, number)
    local t = (i / COUNT) * 6.0
    local c = math.floor(t)
    local f = t - c
    local q = math.floor(255 * (1.0 - f))
    local u = math.floor(255 * f)

    if c == 0 then return 255, u, 0
    elseif c == 1 then return q, 255, 0
    elseif c == 2 then return 0, 255, u
    elseif c == 3 then return 0, q, 255
    elseif c == 4 then return u, 0, 255
    else return 255, 0, q
    end
end

for i = 0, COUNT - 1 do
    local r, g, b = ramp(i)
    -- Every fifth block emits light, so the block-light channel has something
    -- to do and a reload has something visible to change.
    local emit = if (i % 5) == 0 then 12 else 0
    ids[i + 1] = core.register_node(`palette:hue{i}`, r, g, b, true, emit)
end

local function build()
    for i = 0, COUNT - 1 do
        local x = BASE_X + (i % 10)
        local y = BASE_Y + math.floor(i / 10)
        core.set_block(x, y, BASE_Z, ids[i + 1])
    end
end

build()

-- And one item that is not a block, which is the other half of M2's exit test
-- ("20 blocchi e un item"). The twenty blocks above each got an item form of
-- their own without a line being written about them; this is the kind that has
-- no block behind it, so it has to be declared.
local BRUSH = core.register_item("palette:brush", 1)

core.log(`palette: {COUNT} blocks registered, first id {ids[1]}, last id {ids[COUNT]}, `
      .. `brush item {BRUSH}`)

-- No on_step. The host still looks the global up once per frame and finds
-- nothing, which is what the 0.0003 ms/call in --bench is measuring: the cost
-- of a mod that does nothing is the cost of asking.

function on_save(): string
    -- Nothing to carry across a reload -- the wall is in the world, and the
    -- world outlives the VM. Returning the count anyway makes the reload path
    -- visible in the log.
    return tostring(COUNT)
end

function on_load(state: string?)
    core.log(`palette: reloaded, previous version had {state} blocks`)
end

-- What it looks like in a hand: its own model, drawn in the palm
-- (tools/make_held_models.py).
core.register_model("palette:brush_held", "brush_held.gltf")
