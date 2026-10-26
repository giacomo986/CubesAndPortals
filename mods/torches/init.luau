--!strict
-- torches: The classic block game's torch. Coal over a stick is four; one gives light 14,
-- is walked through and aimed at by its stick. It stands on the floor of its
-- cell whatever it was put against (no torch on a wall here), and goes with
-- the block under it.
local TORCH = core.register_node("torches:torch", 250, 200, 90, false, 14, "", 0, 0.0, "")
core.node_box(TORCH, 0.4375, 0, 0.4375, 0.5625, 0.625, 0.5625)

core.recipe_shaped("torches:torch", 4,
    "ore:coal",
    "timber:stick")

core.event.on("core:broken", function(ev)
    if core.get_block(ev.x, ev.y + 1, ev.z) == TORCH then
        core.set_block(ev.x, ev.y + 1, ev.z, 0)
        core.drop_item(ev.x, ev.y + 1, ev.z, core.item_id("torches:torch"), 1)
    end
end)

-- F8: where the dark's creatures can come out round this player, a tile on
-- each floor cell they could stand on (ground under it, room for a body over
-- it). Red: dark enough now; yellow: lit only by the sky, so dark at night.
-- A light block keeps a cell clear when it lights it above 7 (core/ecs/mob.h):
-- a torch's 14 clears everything up to six nodes away.
if core.side() ~= "server" then
    local R, DOWN, UP = 12, 6, 4
    local shown, frame = false, 0
    local tiles: { { number } } = {}

    local function scan()
        tiles = {}
        local me = core.players()[1]
        if me == nil then
            return
        end
        local px, py, pz = core.player_pos(me)
        if px == nil then
            return
        end
        local bx, by, bz = math.floor(px), math.floor(py), math.floor(pz)
        for x = bx - R, bx + R do
            for z = bz - R, bz + R do
                for y = by - DOWN, by + UP do
                    if core.node_solid(core.get_block(x, y - 1, z)) == 1
                        and core.node_solid(core.get_block(x, y, z)) ~= 1
                        and core.node_solid(core.get_block(x, y + 1, z)) ~= 1
                        and core.block_light(x, y, z) <= 7 then
                        table.insert(tiles, { x + 0.5, y + 0.02, z + 0.5, if core.light(x, y, z) <= 7 then 1 else 0 })
                    end
                end
            end
        end
    end

    core.event.on("core:input", function(ev)
        if ev.action == "key_f8" then
            shown = not shown
            frame = 0
        end
    end)

    core.render.pass("torches:dark", 100, function()
        if not shown then
            return
        end
        -- ponytail: a full rescan every 15 frames; follow core:placed/broken if it shows in a profile.
        if frame % 15 == 0 then
            scan()
        end
        frame += 1
        for _, t in tiles do
            core.render.box(t[1], t[2], t[3], 0.5, 0.02, 0.5, 255, if t[4] == 1 then 50 else 210, 40, 150)
        end
    end)
end
