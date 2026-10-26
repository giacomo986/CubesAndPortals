--!strict
-- dungeons: The classic block game's dungeons.
--
-- One column in about 1500 has a room under it, seven across and five tall
-- inside its walls, of cobblestone with a floor mostly of mossy cobblestone.
-- In its middle a SPAWNER keeps bringing zombies or skeletons, one or two
-- every five seconds while somebody is within sixteen blocks and fewer than
-- six creatures are about it; against its walls one or two chests hold what
-- The classic block game's do -- bones, string, gunpowder, iron, bread, wheat, coal,
-- cinnabar, a bucket, now and then an iron pick -- and cobwebs hang in its
-- corners. A spawner is broken like stone and gives nothing.
local SPAWNER = core.register_node("dungeons:spawner", 30, 30, 36, true, 0, "", 1, 5.0, "stone")
core.node_glass(SPAWNER, 200)
core.node_drop_item(SPAWNER, "rocks:cobblestone")

local KINDS = { "mobs:zombie", "mobs:skeleton", "mobs:zombie" }

local function mul32(a: number, b: number): number
    local hi = math.floor(a / 65536)
    local lo = a % 65536
    return ((hi * b) % 65536 * 65536 + lo * b) % 4294967296
end

local function hash(x: number, z: number, salt: number): number
    local h = mul32(x + 0x9E3779B9, 374761393)
    h = bit32.bxor(h, mul32(z + 0x85EBCA6B, 668265263))
    h = bit32.bxor(h, mul32(salt % 4294967296, 2246822519))
    h = bit32.bxor(h, bit32.rshift(h, 13))
    return mul32(h, 1274126177)
end

core.register_abm("dungeons:spawn", { "dungeons:spawner" }, 5.0, 1, function(cell)
    local near = false
    for _, p in core.players() do
        local x, y, z = core.player_pos(p)
        if x ~= nil and (x - cell.x) ^ 2 + (y - cell.y) ^ 2 + (z - cell.z) ^ 2 < 256 then
            near = true
        end
    end
    if not near or core.monsters_near(cell.x + 0.5, cell.y, cell.z + 0.5, 8) >= 6 then
        return
    end
    local kind = KINDS[1 + hash(cell.x, cell.z, cell.y) % #KINDS]
    for _ = 1, math.random(1, 2) do
        local x, z = cell.x + math.random(-3, 3), cell.z + math.random(-3, 3)
        if core.get_block(x, cell.y, z) == 0 and core.get_block(x, cell.y + 1, z) == 0 then
            core.spawn_mob(kind, x + 0.5, cell.y, z + 0.5)
        end
    end
end)

local LOOT = {
    { "mobs:bone", 8 }, { "archery:string", 6 }, { "building:gunpowder", 6 }, { "furnace:iron_ingot", 4 },
    { "farm:bread", 3 }, { "farm:wheat", 6 }, { "ore:coal", 6 }, { "ore:cinnabar", 6 }, { "lakes:bucket", 1 },
    { "furnace:iron_pick", 1 }, { "archery:arrow", 8 },
}

local function stock(x: number, y: number, z: number, h: number)
    local chest = core.node_id("chest:box")
    if chest == 0 then
        return
    end
    core.set_block(x, y, z, chest)
    core.container_make(x, y, z, 27)
    for i = 0, 3 + h % 4 do
        local pick = LOOT[1 + bit32.rshift(h, i * 3) % #LOOT]
        local item = core.item_id(pick[1])
        if item ~= 0 and core.container_count(x, y, z, item) == 0 then
            core.container_add(x, y, z, item, 1 + bit32.rshift(h, i * 2 + 1) % pick[2])
        end
    end
end

local R = 4   -- the room's half width, walls included

core.register_mapgen("dungeons:rooms", function(ctx)
    local COBBLE = core.node_id("rocks:cobblestone")
    local MOSSY = core.node_id("rocks:mossy_cobblestone")
    local WEB = core.node_id("plants:cobweb")
    for cz = ctx.min_z - R, ctx.max_z + R do
        for cx = ctx.min_x - R, ctx.max_x + R do
            local h = hash(cx, cz, ctx.seed + 0xD0D6)
            if h % 1500 == 0 then
                local top = core.terrain_height(cx, cz)
                if top > 16 then
                    local floor = 4 + math.floor(h / 1500) % (top - 14)
                    for dz = -R, R do
                        for dx = -R, R do
                            local x, z = cx + dx, cz + dz
                            if x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z then
                                local wall = math.abs(dx) == R or math.abs(dz) == R
                                for y = floor - 1, floor + 5 do
                                    local shell = wall or y == floor - 1 or y == floor + 5
                                    local id = 0
                                    if shell then
                                        local mossy = hash(x * 7 + y, z, ctx.seed + 0xD0D7) % (if y == floor - 1 then 3 else 5) ~= 0
                                        id = if mossy and y == floor - 1 then MOSSY elseif mossy then COBBLE else MOSSY
                                    end
                                    core.set_block(x, y, z, id)
                                end
                                -- Cobwebs up in the corners.
                                if not wall and math.abs(dx) == R - 1 and math.abs(dz) == R - 1 and h % 2 == 0 and WEB ~= 0 then
                                    core.set_block(x, floor + 4, z, WEB)
                                end
                            end
                        end
                    end
                    if cx >= ctx.min_x and cx <= ctx.max_x and cz >= ctx.min_z and cz <= ctx.max_z then
                        core.set_block(cx, floor, cz, SPAWNER)
                    end
                    for i, spot in { { R - 1, 0 }, { 0, -(R - 1) } } do
                        local x, z = cx + spot[1], cz + spot[2]
                        if (i == 1 or h % 3 == 0) and x >= ctx.min_x and x <= ctx.max_x and z >= ctx.min_z and z <= ctx.max_z then
                            stock(x, floor, z, hash(x, z, ctx.seed + i))
                        end
                    end
                end
            end
        end
    end
end)
