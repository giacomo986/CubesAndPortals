--!strict
-- torches: The classic block game's torch. Coal over a stick is four; one gives light 14,
-- is walked through and aimed at by its stick. Put against the top of a block it
-- stands on the floor; against the side of one it leans out of that wall; against
-- the underside of one it hangs from the ceiling. Each goes with the block that
-- holds it up, and each gives back the one torch. Held in either hand it lights
-- round the player (the client does that for any block that gives light).
-- Models from tools/make_torch_models.py.
-- side = both: the blocks and their rules on the server; the key and the
-- dark spots on the player's machine.
local serving = core.side() ~= "client"

if serving then
    local TORCH = core.register_node("torches:torch", 250, 200, 90, false, 14, "", 0, 0.0, "")
    core.node_box(TORCH, 0.4375, 0, 0.4375, 0.5625, 0.625, 0.5625)
    -- On a wall: made leaning out of the cell's -Z face, turned to the face it was put on.
    local WALL = core.register_node("torches:torch_wall", 250, 200, 90, false, 14, "wallmounted", 0, 0.0, "")
    core.node_box(WALL, 0.4375, 0.18, 0, 0.5625, 0.62, 0.34)
    core.node_drop(WALL, TORCH)
    -- Under a ceiling: on an iron rod down from it.
    local CEILING = core.register_node("torches:torch_ceiling", 250, 200, 90, false, 14, "", 0, 0.0, "")
    core.node_box(CEILING, 0.4375, 0.26, 0.4375, 0.5625, 1, 0.5625)
    core.node_drop(CEILING, TORCH)

    core.node_model(TORCH, core.register_model("torches:torch", "torch.gltf"))
    core.node_model(WALL, core.register_model("torches:torch_wall", "torch_wall.gltf"))
    core.node_model(CEILING, core.register_model("torches:torch_ceiling", "torch_ceiling.gltf"))
    core.register_model("torches:torch_held", "torch_held.gltf")

    core.recipe_shaped("torches:torch", 4,
        "ore:coal",
        "timber:stick")

    -- A wallmounted cell's byte is the face its front points at (+X -X +Y -Y +Z
    -- -Z); the wall it hangs on is the opposite way.
    local AWAY = { [0] = { -1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 } }
    local FACE_OUT = { ["1,0,0"] = 1, ["-1,0,0"] = 0, ["0,0,1"] = 5, ["0,0,-1"] = 4 }

    local function solid(x: number, y: number, z: number): boolean
        return core.node_solid(core.get_block(x, y, z)) == 1
    end

    -- Which block the player put it against: the first solid one along their look,
    -- from their eye. The torch itself is walked through, so the ray passes it.
    local function against(player: number, x: number, y: number, z: number): (number?, number?, number?)
        local px, py, pz = core.player_pos(player)
        local lx, ly, lz = core.player_look(player)
        if px ~= nil and lx ~= nil then
            local hx, hy, hz = core.raycast(px, py + 1.62, pz, lx, ly, lz, 8)
            if hx ~= nil then
                local dx, dy, dz = hx - x, hy - y, hz - z
                if math.abs(dx) + math.abs(dy) + math.abs(dz) == 1 then
                    return dx, dy, dz
                end
            end
        end
        -- No look to go by (a mod put it, a dispenser): the floor, a wall, the ceiling.
        if solid(x, y - 1, z) then return 0, -1, 0 end
        for _, d in { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } } do
            if solid(x + d[1], y, z + d[3]) then return d[1], 0, d[3] end
        end
        if solid(x, y + 1, z) then return 0, 1, 0 end
        return nil, nil, nil
    end

    core.event.on("core:placed", function(ev)
        if ev.id ~= TORCH then
            return
        end
        local dx, dy, dz = against(ev.player, ev.x, ev.y, ev.z)
        if dx == nil or dy == -1 then
            return   -- on the floor, as placed
        end
        if dy == 1 then
            core.set_block(ev.x, ev.y, ev.z, CEILING)
            return
        end
        local face = FACE_OUT[`{dx},{dy},{dz}`]
        if face ~= nil then
            core.set_block(ev.x, ev.y, ev.z, WALL)
            core.set_facing(ev.x, ev.y, ev.z, face)
        end
    end)

    -- A torch held up by the block broken goes with it.
    local function fall(x: number, y: number, z: number)
        core.set_block(x, y, z, 0)
        core.drop_item(x, y, z, core.item_id("torches:torch"), 1)
    end

    core.event.on("core:broken", function(ev)
        if core.get_block(ev.x, ev.y + 1, ev.z) == TORCH then
            fall(ev.x, ev.y + 1, ev.z)
        end
        if core.get_block(ev.x, ev.y - 1, ev.z) == CEILING then
            fall(ev.x, ev.y - 1, ev.z)
        end
        for face = 0, 5 do
            local d = AWAY[face]
            -- The cell whose torch hangs on this block: one step out of it, the
            -- way that torch's front points.
            local tx, ty, tz = ev.x - d[1], ev.y - d[2], ev.z - d[3]
            if core.get_block(tx, ty, tz) == WALL and core.get_facing(tx, ty, tz) == face then
                fall(tx, ty, tz)
            end
        end
    end)
end

-- F8 (its own key, core.register_key: movable in Options > Controls): where the dark's creatures can come out round this player, a tile on
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

    core.register_key("torches:dark", "F8", "Dark spots")
    core.event.on("core:input", function(ev)
        if ev.action == "key_torches:dark" then
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
