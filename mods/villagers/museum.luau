--!strict
-- villagers: the village's museum and its curator. Part of the mod (init.luau says the order).
--
-- A village without a museum orders one (order_post, under the sign
-- villagers:curio): seven by seven, a bed for its CURATOR and seven plinths
-- along its walls. The curator takes the first of each thing a player brings
-- -- fish, ores, gems, the deep's and the relics' finds -- puts it in the
-- collection, kept per village, and pays for it out of the village's
-- treasury: more for what is rarer. The collection stands on the plinths, the
-- first seven things given, each turning over its own (core.show_item).
-- Right click the curator for what is in it and what is still wanted.

local common = require("./common")
local said = common.said
local to_treasury = common.to_treasury
local treasury = common.treasury
local village_at = common.village_at
local watch = common.watch

-- The sign that the house is the museum, and a plinth to show a thing on.
local CURIO = core.register_node("villagers:curio", 200, 190, 160, true, 0, "", 0, 1.0, "wood")
core.node_box(CURIO, 0.3, 0.0, 0.3, 0.7, 0.55, 0.7)
local PLINTH = core.register_node("villagers:plinth", 225, 220, 210, true, 0, "", 0, 2.0, "stone")
core.node_box(PLINTH, 0.15, 0.0, 0.15, 0.85, 0.75, 0.85)

-- What the curator collects, and what each is worth to it in rubies.
local WANTED: { { string | number } } = {}
for _, w in {
    { "pets:fish", 1 }, { "pets:carp", 1 }, { "pets:trout", 2 }, { "pets:cod", 2 }, { "pets:eel", 3 },
    { "pets:tuna", 3 }, { "pets:goldfish", 6 },
    { "ore:coal", 1 }, { "ore:raw_copper", 1 }, { "ore:raw_iron", 1 }, { "ore:lapis", 2 }, { "ore:raw_gold", 2 },
    { "ore:cinnabar", 2 }, { "ore:ruby", 4 }, { "ore:diamond", 6 },
    { "rocks:amethyst_shard", 3 }, { "deep:glowcap", 3 }, { "deep:shard", 5 }, { "relics:shard", 6 },
    { "bees:honeycomb", 2 }, { "mobs:bone", 1 },
} do
    if core.item_id(w[1] :: string) ~= 0 then
        table.insert(WANTED, w)
    end
end
local PRICE: { [string]: number } = {}
for _, w in WANTED do
    PRICE[w[1] :: string] = w[2] :: number
end

-- A village's collection: what, and who gave it, in the order given.
type Piece = { item: string, giver: string }
local function collection(rk: string): { Piece }
    local out = {}
    for item, giver in string.gmatch(core.storage_get(`museum {rk}`), "([^,;]+),([^;]*)") do
        table.insert(out, { item = item, giver = giver })
    end
    return out
end

local function has(rk: string, item: string): boolean
    for _, p in collection(rk) do
        if p.item == item then
            return true
        end
    end
    return false
end

local function add_piece(rk: string, item: string, giver: string)
    local was = core.storage_get(`museum {rk}`)
    core.storage_set(`museum {rk}`, (if was == "" then "" else was .. ";") .. `{item},{giver}`)
end

-- Photographs of rare places (mods/album), by the tag the album gives them,
-- and what the first of each is worth.
local PHOTOS: { [string]: { string | number } } = {
    peak = { "a peak", 4 }, relic = { "a relic", 5 }, clouds = { "the clouds", 5 }, hollows = { "the Hollows", 6 },
}

local function title(s: string): string
    local tag = string.match(s, "^photo:(%a+)$")
    if tag and PHOTOS[tag] then
        return `A photograph of {PHOTOS[tag][1]}`
    end
    local w = said(s)
    return string.upper(string.sub(w, 1, 1)) .. string.sub(w, 2)
end

local looking: { [number]: string } = {}

local function show_curator(p: number, rk: string, told: string?)
    looking[p] = rk
    local pieces = collection(rk)
    local page = { "# The village's museum", `> {#pieces} things in the collection`, "",
                   "Photographs of rare places are offered from your album (/album).", "" }
    local held = core.held_item(p)
    if PRICE[held] ~= nil and not has(rk, held) then
        table.insert(page, `[Give your {said(held)} to the museum: {PRICE[held]} rubies](act:give)`)
        table.insert(page, "")
    elseif PRICE[held] ~= nil then
        table.insert(page, `> The museum has a {said(held)} already.`)
        table.insert(page, "")
    end
    if told then
        table.insert(page, `> {told}`)
        table.insert(page, "")
    end
    if #pieces > 0 then
        table.insert(page, "## In the collection")
        for _, piece in pieces do
            table.insert(page, `- **{title(piece.item)}**, given by {piece.giver}`)
        end
        table.insert(page, "")
    end
    local still = {}
    for _, w in WANTED do
        if not has(rk, w[1] :: string) then
            table.insert(still, `{said(w[1] :: string)} ({w[2]})`)
        end
    end
    for tag, ph in PHOTOS do
        if not has(rk, `photo:{tag}`) then
            table.insert(still, `a photograph of {ph[1]} ({ph[2]})`)
        end
    end
    if #still > 0 then
        table.insert(page, "## Still wanted, and what each pays")
        table.insert(page, table.concat(still, ", ") .. ".")
    else
        table.insert(page, "Every thing the curator looks for is here. The village thanks you all.")
    end
    core.page_show(p, "museum", table.concat(page, "\n"))
end
common.show_curator = show_curator

core.page_register("museum", "Museum")
core.event.on("core:page_open", function(ev)
    if ev.page == "museum" and looking[ev.player] then
        show_curator(ev.player, looking[ev.player])
    end
end)

core.event.on("core:page_action", function(ev)
    local rk = looking[ev.player]
    if ev.page ~= "museum" or ev.id ~= "give" or rk == nil then
        return
    end
    local p = ev.player
    local x, _, z = core.player_pos(p)
    if x == nil or village_at(x, z) ~= rk then
        show_curator(p, rk, "Bring it to the museum itself.")
        return
    end
    local held = core.held_item(p)
    local price = PRICE[held]
    if price == nil or has(rk, held) then
        show_curator(p, rk, "The curator does not want that.")
        return
    end
    if core.take_item(p, held, 1) < 1 then
        return
    end
    add_piece(rk, held, core.player_name(p))
    local pay = math.min(price, treasury(rk))
    if pay > 0 then
        to_treasury(rk, -pay)
        core.give_item(p, "ore:ruby", pay)
    end
    core.log(`villagers: {rk}: the museum took a {held} from {core.player_name(p)}, {pay} rubies`)
    common.AID.ev.deed(rk, `{core.player_name(p)} gave the museum its first {said(held)}.`)
    show_curator(p, rk, if pay > 0 then `The curator thanks you and pays {pay} rubies.`
        else "The curator thanks you: the treasury is empty, so it is a gift.")
end)

-- A photograph offered from an album (mods/album): taken of a rare place,
-- the first of its kind is bought by the museum of the village the player
-- stands in, near its curator.
core.event.define("museum:photo", { fields = { player = "number", tag = "string", id = "number" }, cancellable = false })
core.event.on("museum:photo", function(ev)
    local p = ev.player
    local x, y, z = core.player_pos(p)
    local rk = if x ~= nil and z ~= nil then village_at(x, z) else nil
    local cur = if rk then watch[`{rk} curator 1`] else nil
    local bed = if cur then cur.bed else nil
    if rk == nil or bed == nil or (x - bed[1]) ^ 2 + (y - bed[2]) ^ 2 + (z - bed[3]) ^ 2 > 12 * 12 then
        core.notice(p, "Offer it in a village's museum, by its curator")
        return
    end
    local ph = PHOTOS[ev.tag]
    if ph == nil then
        return
    end
    if has(rk :: string, `photo:{ev.tag}`) then
        core.notice(p, `This museum has a photograph of {ph[1]} already`)
        return
    end
    add_piece(rk :: string, `photo:{ev.tag}`, core.player_name(p))
    common.AID.ev.deed(rk :: string, `{core.player_name(p)} gave the museum a photograph of {ph[1]}.`)
    local pay = math.min(ph[2] :: number, treasury(rk :: string))
    if pay > 0 then
        to_treasury(rk :: string, -pay)
        core.give_item(p, "ore:ruby", pay)
    end
    core.notice(p, `The museum hangs your photograph of {ph[1]}{if pay > 0 then `: {pay} rubies` else ""}`)
end)

-- The collection on the plinths round the curator's bed: the first seven
-- things given, shown again as they come into sight (core.show_item is not
-- kept). By village and plinth, what is shown.
local shown: { [string]: string } = {}
local since = 0.0
core.event.on("core:tick", function(ev)
    since += ev.dt
    if since < 3 then
        return
    end
    since = 0
    for _, v in watch do
        local bed = v.bed
        if v.kind == "curator" and bed ~= nil and v.rk and common.near_player(v, 48) then
            local pieces = collection(v.rk :: string)
            local f = core.find_nodes(bed[1] - 6, bed[2] - 1, bed[3] - 6, bed[1] + 6, bed[2] + 1, bed[3] + 6,
                                      "villagers:plinth")
            local plinths = {}
            for i = 1, #f, 3 do
                table.insert(plinths, { f[i], f[i + 1], f[i + 2] })
            end
            table.sort(plinths, function(a, b)
                return a[1] * 100000 + a[3] < b[1] * 100000 + b[3]
            end)
            local things = {}
            for _, piece in pieces do
                if core.item_id(piece.item) ~= 0 then   -- a photograph is not a thing to stand up
                    table.insert(things, piece)
                end
            end
            for i, c in plinths do
                local piece = things[i]
                local k = `{c[1]},{c[2]},{c[3]}`
                if piece and shown[k] ~= piece.item
                    and core.show_item(c[1], c[2] + 1, c[3], core.item_id(piece.item)) == 1 then
                    shown[k] = piece.item
                end
            end
        end
    end
end)
