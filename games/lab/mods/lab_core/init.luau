--!strict
-- lab_core: what the engineer's laboratory is made of (games/lab, the end of
-- the game) -- its blocks, the tools a player is handed there, the models of
-- l'Ospite (tools/make_ospite.py) and of what the other worlds lend it, and
-- the body a player has: the sandbox's robot on foot, parkour on (the wall
-- runs and the rope the last fight asks for), its arms the base's, which hold
-- a shotgun. Apart from games/lab/mods/lab because that one is also a player's
-- machine's code, which may not link out of its mod; the models here are
-- links to the worlds they come from.

core.player_model(core.register_model("lab_core:robot", "robot.gltf"))
-- The extras a player may pick to wear (tools/make_robot_extras.py): the
-- client draws "<player model>_<extra>" on their skeleton in their colours.
for _, extra in {"cap", "tophat", "headphones", "scarf", "bowtie", "medal",
                  -- and who a player may be instead (tools/make_species.py): the
                  -- client draws "<player model>_<species>", its arms and styles.
                  "human", "human_arms", "human_short", "human_long", "human_beard",
                  "mole", "mole_arms", "mole_lamp", "mole_goggles", "mole_glasses",
                  "frog", "frog_arms", "frog_spots", "frog_crown", "frog_lily",
                  -- the extras refitted to each species, worn instead of the robot's
                  "human_cap", "human_tophat", "human_headphones", "human_scarf", "human_bowtie", "human_medal",
                  "mole_cap", "mole_tophat", "mole_headphones", "mole_scarf", "mole_bowtie", "mole_medal",
                  "frog_cap", "frog_tophat", "frog_headphones", "frog_scarf", "frog_bowtie", "frog_medal"} do
    core.register_model("lab_core:robot_" .. extra, "robot_" .. extra .. ".gltf")
end
core.register_model("lab_core:robot_arms", "robot_arms.gltf")   -- yafps_core's: the guns' bones and clips
core.eye_height(1.5)
core.movement(4.8, 8, 0, 8, -25)
core.parkour(true)

local function node(name: string, rgb: { number }, light: number?, solid: boolean?): number
    return core.register_node(`lab:{name}`, rgb[1], rgb[2], rgb[3], if solid == nil then true else solid,
                              light or 0, "", 0, 0, "stone")
end
node("wall", { 150, 155, 160 })
node("floor", { 60, 64, 70 })
node("steel", { 32, 34, 38 })
node("lamp", { 235, 240, 255 }, 14)
node("lamp_off", { 40, 42, 46 })
node("breach", { 60, 20, 90 }, 6)
node("hazard", { 220, 180, 20 })
node("panel", { 226, 228, 230 })             -- a portal goes on these, and only these
core.node_glass(node("glass", { 190, 220, 235 }), 60)
node("generator", { 120, 210, 255 }, 12)     -- the shield's: broken by hand
node("valve_open", { 230, 120, 40 }, 6)      -- the lava's: shut by a right click
node("valve_shut", { 70, 90, 70 })
node("seal", { 50, 30, 70 }, 4)              -- the way out, shut until the end
local LAVA = node("lava", { 230, 100, 20 }, 10, false)
core.node_liquid(LAVA, 250)

-- A panel a portal is in: chambers_core's shells (a sixteenth thick, open to
-- the room, drawn as nothing), so a body walks into a portal on a wall.
local HIDDEN = core.register_model("lab_core:hidden", "hidden.gltf")
local T = 1 / 16
local function shell(name: string, high: boolean)
    local id = core.register_node(`lab:{name}`, 226, 228, 230, true, 0, "wallmounted", 0, 0, "stone")
    core.node_model(id, HIDDEN)
    core.node_box(id, 0, 0, 0, 1, 1, T)
    core.node_box(id, 0, 0, 0, T, 1, 1)
    core.node_box(id, 1 - T, 0, 0, 1, 1, 1)
    if high then core.node_box(id, 0, 1 - T, 0, 1, 1, 1) end
end
shell("shell_low", false)
shell("shell_high", true)

-- The kart the way out is driven in: track_core's, standing as a block until
-- somebody gets in.
local KART = core.register_model("lab_core:kart", "kart.gltf")
local kart = core.register_node("lab:kart", 200, 40, 40, true, 0, "upright", 0, 0, "metal")
core.node_model(kart, KART)

-- The way out's road: track_core's pieces, two squares wide, on a grid of
-- 8 (core.node_piece): straights, a rise, banked turns both ways and the
-- tilts into and out of them -- the left ones are not track_core's, which
-- has only its right turns banked wide. The same numbers as its wide set.
core.register_node("lab:piece_fill", 60, 62, 70, true, 0, "", 0, 0.2)
local PFILL = core.node_id("lab:piece_fill")
--  name           plan size climb g0 g1 bank0 bank1
for _, p in { { "straight2",  1, 2, 0, 0, 0, 0, 0 }, { "straight3",  1, 3, 0, 0, 0, 0, 0 },
              { "rise",       1, 2, 2, 0, 0, 0, 0 },
              { "tilt_in_r",  1, 3, 0, 0, 0, 0, 8 }, { "tilt_out_r", 1, 3, 0, 0, 0, 8, 0 },
              { "bank_r",     2, 3, 0, 0, 0, 8, 8 },
              { "tilt_in_l",  1, 3, 0, 0, 0, 0, -8 }, { "tilt_out_l", 1, 3, 0, 0, 0, -8, 0 },
              { "bank_l",     5, 3, 0, 0, 0, -8, -8 } } do
    local id = core.register_node(`lab:piece_{p[1]}`, 168, 172, 182, true, 0, "upright", 0, 0.2)   -- light: it is seen in the dark
    if core.node_piece(id, PFILL, p[2], p[3], p[4], p[5], p[6], p[7], p[8], true, false, 2) ~= 1 then
        core.log(`lab_core: piece {p[1]} refused`)
    end
end

-- What a player is handed there, each lent by the world that taught it.
core.register_item("lab:portal_gun", 1, "", 0, 1.0, "")
core.register_model("lab:portal_gun_held", "portal_gun_held.gltf")
core.register_item("lab:shotgun", 1, "", 0, 1.0, "")
core.register_model("lab:shotgun_held", "shotgun_held.gltf")
core.register_item("lab:rope_gun", 1, "", 0, 1.0, "")
core.register_model("lab:rope_gun_held", "rope_gun_held.gltf")

-- The engineer's torch (from the backrooms' exit) and the crypt's frost:
-- what the dark and the last form ask for (tools/make_held_models.py).
core.register_item("lab:torch", 1, "", 0, 1.0, "")
core.register_model("lab:torch_held", "torch_held.gltf")
core.register_item("lab:frost", 1, "", 0, 1.0, "")
core.register_model("lab:frost_held", "frost_held.gltf")
core.register_item("lab:pistol", 1, "", 0, 1.0, "")
core.register_model("lab:pistol_held", "pistol_held.gltf")
core.register_item("lab:chaingun", 1, "", 0, 1.0, "")
core.register_model("lab:chaingun_held", "chaingun_held.gltf")

-- What lies on the floor for a player to walk over: shells, bullets, a
-- medkit (the base's models), and the chaingun as it is held.
core.register_model("lab_core:ammo_shells", "ammo_shells.gltf")
core.register_model("lab_core:ammo_bullets", "ammo_bullets.gltf")
core.register_model("lab_core:medkit", "medkit.gltf")

-- What l'Ospite calls up: the base's imps and ghouls, never out of the dark
-- by themselves. Their voices are arpg_core's imp's (imp_*.ogg, linked).
local NEVER = "lab_core:never"
--                  name                  model                                                      hp speed fight    dmg reach every drop n shot                                                        doors hover fire spawn_on
core.register_mob("lab_core:imp",       core.register_model("lab_core:imp", "imp.gltf"),             26, 3.4, "archer", 2, 16, 2.2, "", 1, core.register_model("lab_core:fireball", "fireball.gltf"), false, 0, true, NEVER)
core.register_mob("lab_core:ghoul", core.register_model("lab_core:ghoul", "ghoul.gltf"), 16, 2.6, "archer", 1, 18, 1.8, "", 1, core.register_model("lab_core:bullet", "bullet.gltf"),     false, 0, true, NEVER)
core.mob_sight("lab_core:imp", 48)
core.mob_sight("lab_core:ghoul", 48)

-- l'Ospite, its shield and its spheres: the lab's own entities, moved by it.
core.register_model("lab_core:ospite", "ospite.gltf")
core.register_model("lab_core:ospite_shield", "ospite_shield.gltf")
core.register_model("lab_core:ospite_orb", "ospite_orb.gltf")
