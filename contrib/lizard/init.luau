--!strict
-- lizard: a species for the player, from outside the game.
--
-- A species is a body "<player model>_<name>" on the player model's rig, and
-- its arms seen in first person "<player model>_<name>_arms": registered under
-- those names, the client offers it on the look page after the game's own and
-- draws whoever picked it with it (src/client/main.cpp, addon_seen). Neither
-- carries clips: the client plays the player model's on them (borrow_clips).
-- Made by make_lizard.py, beside this file.
local function model(name: string)
    core.register_model("robot:" .. name, name .. ".gltf")
end

model("robot_lizard")
model("robot_lizard_arms")
-- Always worn: the tail, on wag_* links that swing it.
model("robot_lizard_worn")
-- Its styles: "_hat_" ones come off under a hat.
model("robot_lizard_hat_crest")
model("robot_lizard_style_frill")
model("robot_lizard_style_spikes")
-- The robot's extras, fitted to its head and neck.
for _, extra in ipairs({ "cap", "tophat", "headphones", "scarf", "bowtie", "medal" }) do
    model("robot_lizard_" .. extra)
end
