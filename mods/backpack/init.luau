--!strict
-- backpack: a leather pack worn in the back square of the bag -- so it is this
-- or the flight pack, carrying or flying -- that holds 27 slots more, opened
-- with B.
--
-- What is in it is the BACKPACK's: each one is numbered the first time it is
-- opened (core.worn_set_id), and the number goes wherever it goes -- a chest,
-- the ground, another player's back -- so its contents go with it. Two
-- backpacks are two sets of 27 slots.
--
-- Three colours tell them apart: a backpack and one to three dyes at a table
-- (core.recipe_dye) colour its body, its flap and its straps, in the order the
-- dyes lie in the grid, and keep its number. The bag shows them as three marks.
--
-- The contents are a CARRIED container: at a cell below core's kCarriedBelow,
-- never in the world and never out of reach, one column along per number.
-- Saved with the world like a chest.
--
-- backpack.gltf is made in ~/kart_assets/robot18.blend (collection Backpack)
-- on the robot's own rig, the way mods/armor's pieces are: bound to "chest",
-- posed with the wearer.
--
-- `side = both`, for the key: B reaches only the player's machine
-- (core:input), so that half asks the server over the mod's channel, as
-- mods/creative does with C. A process that is its own server opens it at once.

local side    = core.side()
local serving = side ~= "client"

local open_for: (number) -> () = function(_) end

if serving then
    core.net.open()

    core.register_item("backpack:backpack", 1, "", 0, 1.0, "", 0, "back")
    core.register_model("backpack:backpack", "backpack.gltf")
    core.register_model("backpack:backpack_held", "backpack_held.gltf")   -- tools/make_held_models.py

    core.recipe_dye("backpack:backpack")

    core.recipe_shaped("backpack:backpack", 1,
        "animals:leather animals:leather animals:leather",
        "animals:leather chest:box       animals:leather",
        "animals:leather animals:leather animals:leather")

    local SLOTS = 27
    local Y     = -1073741824   -- kCarriedBelow: a carried container's row

    -- The number of the backpack on a player's back, given it the first time.
    local function number(player: number): number
        local id = core.worn_id(player, "back")
        if id ~= 0 then
            return id
        end
        id = ((tonumber(core.shared_storage_get("next")) or 0) :: number) + 1
        if core.worn_set_id(player, "back", id) == 0 then
            return 0
        end
        core.shared_storage_set("next", tostring(id))
        return id
    end

    open_for = function(player: number)
        if core.worn_item(player, "back") ~= "backpack:backpack" then
            return
        end
        local id = number(player)
        if id ~= 0 then
            core.container_show(id, Y, 0, SLOTS, player)
        end
    end

    core.event.on("core:net_message", function(ev)
        if ev.text == "open" then
            open_for(ev.player)
        end
    end)
end

if side ~= "server" then
    core.event.on("core:input", function(ev)
        if ev.action ~= "key_b" then
            return
        end
        if serving then
            open_for(ev.player)
        else
            core.net.send(0, "open")
        end
    end)
end
