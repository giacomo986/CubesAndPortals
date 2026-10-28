--!strict
-- Carts: five iron ingots, carried in the bag. Right click a rail (mods/building)
-- with one in the hand and you sit in it on the rail, facing the way it runs,
-- and ride (core.body_cart): plain rails at 6 m/s, powered ones at 11, a
-- powered rail that is off stops you -- a station: forward pushes off it --
-- and a powered switch turns you right. Jump gets you off; off the rails for
-- half a second, the cart goes back in the bag. Everybody sees it, worn on the
-- feet (core.player_wear), and the body sat in it (the drive_idle clip).

local CART = "carts:cart"
core.register_item(CART, 1)
local MODEL = core.register_model(CART, "cart.gltf")
core.recipe_shaped(CART, 1,
    "furnace:iron_ingot  -                   furnace:iron_ingot",
    "furnace:iron_ingot  furnace:iron_ingot  furnace:iron_ingot")

local RAIL: { [number]: number } = {}   -- rail id -> its top over the cell's floor, mid-cell
for name, top in { ["building:rail"] = 1 / 16, ["building:powered_rail"] = 1 / 16,
                   ["building:powered_rail_on"] = 1 / 16, ["building:switch"] = 1 / 16,
                   ["building:rail_up"] = 0.5 + 1 / 16, ["building:rail_down"] = 0.5 + 1 / 16 } do
    RAIL[core.node_id(name)] = top
end
-- The way an upright cell runs, facing 0..3: away from whoever laid it.
local RUN = { { 0, -1 }, { -1, 0 }, { 0, 1 }, { 1, 0 } }
local SLOT = 9       -- of core.player_wear's ten; mods/boats has the tenth
local DERAILED = 10  -- ticks off the rails before the cart is put back in the bag

local riding: { [number]: number } = {}   -- player -> ticks off the rails

local function leave(p: number)
    riding[p] = nil
    core.body_cart(p, false)
    core.player_wear(p, SLOT, 0)
    core.player_clip(p, "")
    if core.give_item(p, CART, 1) < 1 then
        local x, y, z = core.player_pos(p)
        core.drop_item(math.floor(x), math.floor(y), math.floor(z), core.item_id(CART), 1)
    end
end

core.event.on("core:used", function(ev)
    if ev.held ~= CART or riding[ev.player] then
        return
    end
    ev.cancel = true
    local top = RAIL[ev.id]
    if top == nil then
        core.notice(ev.player, "A cart goes on a rail")
        return
    end
    if core.take_item(ev.player, CART, 1) < 1 then
        return
    end
    local p = ev.player
    riding[p] = 0
    core.body_cart(p, true)
    core.player_wear(p, SLOT, MODEL)
    core.player_clip(p, "drive_idle")
    local run = RUN[core.get_facing(ev.x, ev.y, ev.z) % 4 + 1]
    core.player_place(p, ev.x + 0.5, ev.y + top, ev.z + 0.5, run[1], 0, run[2])
end)

core.event.on("core:tick", function()
    if next(riding) == nil then
        return
    end
    local here = {}
    for _, p in core.players() do
        here[p] = true
    end
    for p, n in riding do
        if not here[p] then   -- gone: the cart with them
            riding[p] = nil
            continue
        end
        local x, y, z = core.player_pos(p)
        local fx, fy, fz = math.floor(x), math.floor(y), math.floor(z)
        if RAIL[core.get_block(fx, fy, fz)] or RAIL[core.get_block(fx, fy - 1, fz)] then
            riding[p] = 0
        elseif n + 1 >= DERAILED then
            leave(p)
        else
            riding[p] = n + 1
        end
    end
end)
