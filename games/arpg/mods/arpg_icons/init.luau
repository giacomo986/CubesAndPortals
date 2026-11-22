--!strict
-- arpg_icons: the inventory's pictures of games/arpg (tools/make_arpg_icons.py),
-- every item base once per rarity, the belt's things, gold and the materials.
-- They reach a player's machine as the files of one model, icons.gltf -- a
-- triangle that names them all -- under "model.arpg_icons_all.<file>", which
-- arpg_game draws with core.hud.image. Not as items: items share the blocks'
-- atlas, and these would take it over (see the tool).
if string.sub(core.room(), 1, 9) ~= "workshop-" then
    core.register_model("arpg_icons:all", "icons.gltf")
end
