-- cards: a card table, one cell, and the Neapolitan deck of forty on it:
-- briscola for two, three or four, cinquiglio for five, scopa and tresette
-- for two or four.
--
-- Right click on the table and you sit: the body goes to the seat, facing the
-- table, and holds its cards up in both hands (the robot's clip "cards",
-- tools/make_card_hold.py, played for everybody by core.player_clip). The
-- camera is its eye with the cursor free (core.fixed_view at a point): your
-- hand is a fan before you, held by the first-person arms, and a click on a
-- card plays it. V turns the view to above the table and back. Everybody else
-- sees only the backs of the fan -- this machine is never told another's
-- cards -- and a seat the table plays has its cards on the felt.
-- The seats are round the table, evenly
-- spaced for the game being played, partners facing each other in a game of
-- four; right click again for the menu: which game, get up. A seat nobody sits
-- in is played by the table itself, so one player alone plays against the
-- rest; somebody who sits down during a game takes over that seat's hand, and
-- somebody who gets up leaves it to the table. A left click on a card of your
-- hand plays it. Turn goes round to the right (counterclockwise), as in Italy;
-- whoever sits after the dealer leads.
--
--   Briscola    three cards each, the last of the stock turned face up under
--               it is the trump suit (for three, a two is taken out of the
--               deck). No need to follow; the trick goes to the highest trump,
--               or the highest of the suit led. Ace 11, three 10, king 4,
--               horse 3, jack 2: over 60 of the 120 wins, for three the most.
--   Cinquiglio  briscola for five, eight cards each and no stock, and an
--               auction first: each in turn bids a card lower than the last
--               (ace, three, king ... four, two), then the two with more points
--               than 61, or passes for good. Whoever bids last names its suit:
--               that is the trump, and whoever holds that card is their
--               partner, unknown until it is played. Together they must make
--               the points bid; caller 2, partner 1, the others 1 each, won or
--               paid. Holding the card one's self is playing alone, for 4.
--   Scopa       three each and four on the table. A card takes the card of its
--               value, or if there is none any cards that add up to it (a
--               panel asks which, when there is more than one way); taking all
--               the table is a scopa. The last to take gets what is left. A
--               point each for most cards, most coins, the seven of coins, the
--               primiera and every scopa; first to 11.
--   Tresette    ten each (and, for two, the rest a stock drawn after every
--               trick). Follow suit if you can; the highest of the suit led
--               takes it, three over two over ace over king. Aces a point, the
--               threes, twos and figures a third, the last trick a point, and
--               the accusi in the hand as dealt: ace, two and three of a suit
--               (napoletana) 3, three of the aces, twos or threes 3 and all
--               four 4. First to 21.
--
-- Two decks, chosen in the menu: the classic one, its art modelled in Blender,
-- and an illustrated one ElevenLabs draws a few pictures a day
-- (tools/elevenlabs_deck.json; until a picture is drawn, its card is the
-- classic one's).
--
-- The SERVER deals, keeps every hand and decides every card, and keeps every
-- game in its storage across a restart; a player's machine is told the table
-- and only its own hand, draws the cards (core.render.model; the art from
-- tools/make_card_art.py in Blender, faces from tools/make_cards.py, models
-- from tools/make_card_models.py) and sends the card clicked.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local TOP = 0.775                  -- the felt's top above the cell's floor
local CW, CH = 0.1, 0.16           -- a card (tools/make_card_models.py)

-- ===================================================================== cards
-- A card is 1..40: (suit) * 10 + value, suits denari, coppe, spade, bastoni,
-- values 1..7, fante 8, cavallo 9, re 10.

local function suit(c: number): number
    return (c - 1) // 10
end

local function value(c: number): number
    return (c - 1) % 10 + 1
end

local SUIT_NAME = { [0] = "denari", "coppe", "spade", "bastoni" }
local VALUE_NAME = { "asso", "due", "tre", "quattro", "cinque", "sei", "sette", "fante", "cavallo", "re" }
local GAME_NAME = { briscola = "Briscola", scopa = "Scopa", tresette = "Tresette", cinquiglio = "Cinquiglio" }
local COUNT_NAME = { [2] = "due", [3] = "tre", [4] = "quattro", [5] = "cinque" }

local function name_of(c: number): string
    return `{VALUE_NAME[value(c)]} di {SUIT_NAME[suit(c)]}`
end

local BR_RANK = { 10, 1, 9, 2, 3, 4, 5, 6, 7, 8 }   -- ace, three, king, horse, jack, 7..2
local BR_PTS  = { 11, 0, 10, 0, 0, 0, 0, 2, 3, 4 }
local TR_RANK = { 8, 9, 10, 1, 2, 3, 4, 5, 6, 7 }   -- three, two, ace, king, horse, jack, 7..4
local TR_THIRDS = { 3, 1, 1, 0, 0, 0, 0, 1, 1, 1 }
local PRIMIERA = { 16, 12, 13, 14, 15, 18, 21, 10, 10, 10 }

-- Cinquiglio's bids, lowest first: a card's value, each lower than the last
-- (to make 61), then the two to make 62, 63 ... 120.
local BID_VALUES = { 1, 3, 10, 9, 8, 7, 6, 5, 4 }
local MAX_BID = 9 + 60

local function bid_value(level: number): number
    return BID_VALUES[level] or 2
end

local function bid_target(level: number): number
    return if level <= 9 then 61 else 61 + level - 10
end

local function bid_name(level: number): string
    if level <= 9 then
        return `il {VALUE_NAME[bid_value(level)]}`
    end
    return `il due a {bid_target(level)}`
end

-- The rules a trick is taken by.
local function rules(game: string): string
    return if game == "cinquiglio" then "briscola" else game
end

local function csv(list: { number }): string
    return if #list == 0 then "-" else table.concat(list, ",")
end

local function uncsv(s: string?): { number }
    local out = {}
    if s == nil or s == "-" then
        return out
    end
    for _, w in string.split(s, ",") do
        local n = tonumber(w)
        if n then
            table.insert(out, n)
        end
    end
    return out
end

-- Whether card b, played after a and a being the best of the trick so far,
-- takes it: higher of the same suit, or a trump on anything else.
local function beats(game: string, a: number, b: number, trump: number): boolean
    if suit(a) == suit(b) then
        local rank = if rules(game) == "briscola" then BR_RANK else TR_RANK
        return rank[value(b)] > rank[value(a)]
    end
    return rules(game) == "briscola" and suit(b) == trump
end

-- The seat whose card takes a trick of { seat, card } pairs, led first.
local function taker(game: string, trick: { { number } }, trump: number): number
    local best = trick[1]
    for i = 2, #trick do
        if beats(game, best[2], trick[i][2], trump) then
            best = trick[i]
        end
    end
    return best[1]
end

-- The cards of `hand` that may be played on `trick`: tresette must follow.
local function playable(game: string, hand: { number }, trick: { { number } }): { [number]: boolean }
    local ok = {}
    local led = if #trick > 0 then suit(trick[1][2]) else -1
    local follows = false
    if game == "tresette" and led >= 0 then
        for _, c in hand do
            follows = follows or suit(c) == led
        end
    end
    for _, c in hand do
        ok[c] = not follows or suit(c) == led
    end
    return ok
end

-- The ways a scopa card takes from the table: every card of its value, or if
-- there is none every set of two or more that adds up to it.
local function captures(tbl: { number }, c: number): { { number } }
    local v = value(c)
    local out = {}
    for _, t in tbl do
        if value(t) == v then
            table.insert(out, { t })
        end
    end
    if #out > 0 or #tbl > 16 then
        return out
    end
    for mask = 1, bit32.lshift(1, #tbl) - 1 do
        local sum, set = 0, {}
        for i = 1, #tbl do
            if bit32.btest(mask, bit32.lshift(1, i - 1)) then
                sum += value(tbl[i])
                table.insert(set, tbl[i])
            end
        end
        if sum == v and #set >= 2 then
            table.insert(out, set)
        end
    end
    return out
end

-- A team's primiera: its best card of every suit by PRIMIERA, 0 missing one.
local function primiera(cards: { number }): number
    local best = { [0] = 0, 0, 0, 0 }
    for _, c in cards do
        best[suit(c)] = math.max(best[suit(c)], PRIMIERA[value(c)])
    end
    if best[0] == 0 or best[1] == 0 or best[2] == 0 or best[3] == 0 then
        return 0
    end
    return best[0] + best[1] + best[2] + best[3]
end

-- Tresette's accusi in a hand as dealt, in points.
local function accusi(hand: { number }): number
    local pts = 0
    local has = {}
    for _, c in hand do
        has[c] = true
    end
    for s = 0, 3 do
        if has[s * 10 + 1] and has[s * 10 + 2] and has[s * 10 + 3] then
            pts += 3   -- napoletana
        end
    end
    for _, v in { 1, 2, 3 } do
        local k = 0
        for s = 0, 3 do
            k += if has[s * 10 + v] then 1 else 0
        end
        pts += if k == 4 then 4 elseif k == 3 then 3 else 0   -- buongioco
    end
    return pts
end

local function key_of(x: number, y: number, z: number): string
    return `{x},{y},{z}`
end

local function cell_of(key: string): (number, number, number)
    local w = string.split(key, ",")
    return tonumber(w[1]) or 0, tonumber(w[2]) or 0, tonumber(w[3]) or 0
end

-- Where a seated body stands: SEAT_R from the table's middle, facing it, and
-- the grip of the cards it holds in the clip "cards" (tools/make_card_hold.py):
-- GRIP_FWD ahead of it and GRIP_UP over its feet; seen from its own eye, the
-- grip is GRIP_EYE (glTF axes: x right, y up, -z ahead).
local SEAT_R = 0.8
local GRIP_FWD, GRIP_UP = 0.35, 1.07
local GRIP_EYE = { 0.0, -0.22, -0.36 }
local EYE_H = 1.5   -- mods/robot's core.eye_height

-- The yaw of seat q's frame: seats evenly round the table from the side it faces.
local function seat_yaw(facing: number, n: number, q: number): number
    return facing * math.pi / 2 + q * 2 * math.pi / n
end

-- Text on the wire is one word: spaces travel as underscores.
local function word(s: string): string
    local w = string.gsub(s, "[%s_]", "_")
    return if w == "" then "-" else w
end

local heard: (string) -> () = function(_) end
local from_client: (number, string) -> () = function(_, _) end

-- ==================================================================== server

if serving then
    core.net.open()

    local TABLE = core.register_node("cards:table", 30, 110, 60, true, 0, "upright", 0, 1.0, "wood")
    core.node_model(TABLE, core.register_model("cards:table", "table.gltf"))
    core.node_box(TABLE, 0, 0, 0, 1, TOP, 1)
    for _, deck in { "", "e" } do
        for n = 1, 40 do
            core.register_model(`cards:{deck}c{n}`, `{deck}card{n}.gltf`)
            core.register_model(`cards:{deck}d{n}`, `{deck}dim{n}.gltf`)   -- may not be played now
        end
        core.register_model(`cards:{deck}back`, `{deck}back.gltf`)
    end
    core.recipe_shaped("cards:table", 1,
        "timber:planks timber:planks timber:planks",
        "timber:stick  timber:planks timber:stick",
        "timber:stick  -             timber:stick")

    local BOT_WAIT, TRICK_WAIT, SHOW_WAIT = 0.9, 1.6, 1.2
    local TARGET = { scopa = 11, tresette = 21 }

    -- A table's whole state is plain numbers, strings and tables of them, so
    -- that it can be kept as it is (enc/dec) across a restart; only `humans`
    -- and `timer` are not kept.
    type Table = {
        facing: number,
        game: string,           -- "" before a game is chosen
        deck: string,           -- "" the classic deck, "e" the illustrated one
        n: number,              -- players: seats 0..n-1 round the table
        humans: { [number]: number },   -- seat -> player
        phase: string,          -- "idle", "bid" and "suit" (cinquiglio's auction), "play", "trick" (a
                                -- full trick shown), "show" (a scopa card shown with what it takes),
                                -- "hand" (a hand counted), "match"
        hands: { [number]: { number } },
        stock: { number },      -- drawn from the front; briscola's trump card is the last
        trump: number,          -- briscola's turned card, cinquiglio's called one, 0 otherwise
        tbl: { number },        -- scopa's table
        trick: { { number } },  -- { seat, card } in the order played
        turn: number,
        dealer: number,
        piles: { [number]: number },     -- cards taken, by seat (drawn by the seat)
        taken: { [number]: { number } }, -- cards taken, by team
        scope: { [number]: number },
        thirds: { [number]: number },    -- tresette's last trick, by team
        acc: { [number]: number },       -- tresette's accusi, by seat
        lastcap: number,        -- scopa: the team that took last
        score: { [number]: number },     -- the match, by team
        show: { number },       -- scopa: seat, card, then the cards it takes
        sum: { number },        -- the last hand's count, for the players' screens
        gone: { [number]: boolean },     -- every card seen played this hand, for the table's players
        void: { [number]: { [number]: boolean } },  -- seat -> suits it has shown it has not
        -- cinquiglio
        level: number,          -- the highest bid, 0 none
        bidder: number,         -- who made it, -1 nobody
        out: { [number]: boolean },      -- seats that passed
        caller: number,
        partner: number,        -- who holds the called card (the caller, alone)
        revealed: boolean,      -- the called card has been played
        timer: number,
    }
    local tables: { [string]: Table } = {}
    local menu_of: { [number]: string } = {}       -- the table a player's menu is about
    local choosing: { [number]: { any } } = {}     -- a scopa card waiting for which take

    -- Somebody told their seat stands there holding their cards, for
    -- everybody else; told they watch or are out, they let go.
    local function seat_body(p: number, text: string)
        local w = string.split(text, " ")
        local T = if w[1] == "sit" then tables[w[2]] else nil
        local q = tonumber(w[3] or "") or -1
        if T and q >= 0 then
            local x, y, z = cell_of(w[2])
            local a = seat_yaw(T.facing, T.n, q)
            local bx, bz = x + 0.5 + SEAT_R * math.sin(a), z + 0.5 + SEAT_R * math.cos(a)
            -- not into a wall: where the seat is not free the body stays where it stands
            local cx, cz = math.floor(bx), math.floor(bz)
            if core.get_block(cx, y, cz) == 0 and core.get_block(cx, y + 1, cz) == 0 then
                core.player_place(p, bx, y, bz, -math.sin(a), 0, -math.cos(a))
            end
            core.player_clip(p, "cards")
        elseif w[1] == "sit" or w[1] == "out" then
            core.player_clip(p, "")
        end
    end

    local function tell(player: number, text: string)
        seat_body(player, text)
        if drawing then
            heard(text)
        else
            core.net.send(player, text)
        end
    end

    local function nxt(T: Table, s: number): number
        return (s + 1) % T.n
    end

    -- Partners face each other at four; otherwise everybody is their own
    -- team (cinquiglio's sides are counted at the end).
    local function team(T: Table, s: number): number
        return if T.n == 4 then s % 2 else s
    end

    local function teams(T: Table): number
        return if T.n == 4 then 2 else T.n
    end

    local function seat_of(T: Table, p: number): number
        for s, q in T.humans do
            if q == p then
                return s
            end
        end
        return -1
    end

    local function per(n: number, v: any): { [number]: any }
        local t = {}
        for i = 0, n - 1 do
            t[i] = if type(v) == "table" then {} else v
        end
        return t
    end

    local function fresh(facing: number): Table
        return { facing = facing, game = "", deck = "", n = 4, humans = {}, phase = "idle", hands = per(5, {}), stock = {},
                 trump = 0, tbl = {}, trick = {}, turn = 0, dealer = 0, piles = per(5, 0), taken = per(5, {}),
                 scope = per(5, 0), thirds = per(5, 0), acc = per(5, 0), lastcap = -1, score = per(5, 0),
                 show = {}, sum = {}, gone = {}, void = per(5, {}), level = 0, bidder = -1, out = {},
                 caller = -1, partner = -1, revealed = false, timer = 0 }
    end

    -- ------------------------------------------------------------ keeping
    -- enc: a number, "'" and a string, T or F, or {key=value,...}; dec reads it back.

    local function enc(v: any): string
        local t = type(v)
        if t == "number" then
            return tostring(v)
        elseif t == "boolean" then
            return if v then "T" else "F"
        elseif t == "string" then
            return "'" .. v
        end
        local parts = {}
        for k, x in v do
            table.insert(parts, `{enc(k)}={enc(x)}`)
        end
        return "{" .. table.concat(parts, ",") .. "}"
    end

    local function dec(s: string): any
        local i = 1
        local function read(): any
            local ch = string.sub(s, i, i)
            if ch == "{" then
                i += 1
                local t = {}
                while string.sub(s, i, i) ~= "}" and i <= #s do
                    local k = read()
                    i += 1   -- "="
                    t[k] = read()
                    if string.sub(s, i, i) == "," then
                        i += 1
                    end
                end
                i += 1
                return t
            end
            local j = i
            while i <= #s and not string.find(",=}", string.sub(s, i, i), 1, true) do
                i += 1
            end
            local w = string.sub(s, j, i - 1)
            if w == "T" or w == "F" then
                return w == "T"
            elseif string.sub(w, 1, 1) == "'" then
                return string.sub(w, 2)
            end
            return tonumber(w) or 0
        end
        return read()
    end

    local function keep_one(key: string, T: Table)
        local saved = {}
        for k, v in T :: any do
            if k ~= "humans" and k ~= "timer" then
                saved[k] = v
            end
        end
        if core.storage_set("t" .. key, enc(saved)) == 0 then
            core.log(`cards: the table at {key} could not be kept`)
        end
    end

    local function keep_list()
        local keys = {}
        for key in tables do
            table.insert(keys, key)
        end
        core.storage_set("tables", table.concat(keys, ";"))
    end

    core.event.on("core:world_ready", function()
        for _, key in string.split(core.storage_get("tables") or "", ";") do
            local s = core.storage_get("t" .. key)
            if key ~= "" and s ~= "" then
                local ok, saved = pcall(dec, s)
                if ok and type(saved) == "table" then
                    local T = fresh(0)
                    for k, v in saved do
                        (T :: any)[k] = v
                    end
                    T.humans, T.timer = {}, BOT_WAIT
                    tables[key] = T
                end
            end
        end
    end)

    -- ------------------------------------------------------------ telling

    -- What player p is told about table T: everything on it, and of the
    -- hands only their own. `x` is the game's own: cinquiglio's auction and
    -- call, tresette's accusi.
    local function line_for(key: string, T: Table, p: number): string
        local me = seat_of(T, p)
        local counts, piles, names = {}, {}, {}
        for s = 0, T.n - 1 do
            table.insert(counts, #T.hands[s])
            table.insert(piles, T.piles[s])
            local h = T.humans[s]
            table.insert(names, if h then word(core.player_name(h)) else "-")
        end
        local trick = {}
        for _, t in T.trick do
            table.insert(trick, `{t[1]}:{t[2]}`)
        end
        local score, scope = {}, {}
        for t = 0, teams(T) - 1 do
            table.insert(score, T.score[t])
            table.insert(scope, T.scope[t])
        end
        local x = {}
        if T.game == "cinquiglio" then
            local outs = 0
            for s in T.out do
                outs += bit32.lshift(1, s)
            end
            local seen_partner = if T.revealed or (me >= 0 and me == T.partner) then T.partner else -1
            x = { T.level, T.bidder, outs, T.trump, T.caller, seen_partner }
        elseif T.game == "tresette" then
            for s = 0, T.n - 1 do
                table.insert(x, T.acc[s])
            end
        end
        return `s {key} {T.facing} {if T.game == "" then "-" else T.game} {T.n} {T.phase} {T.turn} {T.dealer} {me} `
            .. `{if me >= 0 then csv(T.hands[me]) else "-"} {csv(counts)} {if #trick == 0 then "-" else table.concat(trick, ",")} `
            .. `{csv(T.tbl)} {#T.stock} {T.trump} {csv(piles)} {csv(score)} {csv(scope)} `
            .. `{csv(T.show)} {csv(T.sum)} {table.concat(names, "|")} {csv(x)} {if T.deck == "" then "-" else T.deck}`
    end

    local show_bid: (string, Table) -> () = function(_, _) end

    local function everybody(key: string, T: Table)
        keep_one(key, T)
        if drawing then
            local me = core.players()[1]
            if me then
                heard(line_for(key, T, me))
            end
        else
            for _, p in core.players() do
                core.net.send(p, line_for(key, T, p))
            end
        end
        show_bid(key, T)
    end

    -- ------------------------------------------------------------ dealing

    local function shuffled(n: number): { number }
        local d = {}
        for i = 1, 40 do
            if not (n == 3 and i == 12) then   -- for three, the two of cups is out
                table.insert(d, i)
            end
        end
        for i = #d, 2, -1 do
            local j = math.random(i)
            d[i], d[j] = d[j], d[i]
        end
        return d
    end

    local function to_turn(T: Table, s: number)
        T.turn, T.timer = s, BOT_WAIT
    end

    local function deal(T: Table)
        local deck = shuffled(T.n)
        if T.game == "scopa" then
            -- three kings or more on the table is a misdeal
            local function kings(): number
                local k = 0
                for i = 1, 4 do
                    k += if value(deck[i]) == 10 then 1 else 0
                end
                return k
            end
            while kings() >= 3 do
                deck = shuffled(T.n)
            end
        end
        T.hands, T.piles, T.taken = per(T.n, {}), per(T.n, 0), per(teams(T), {})
        T.scope, T.thirds, T.acc, T.void = per(teams(T), 0), per(teams(T), 0), per(T.n, 0), per(T.n, {})
        T.tbl, T.trick, T.show, T.sum, T.trump, T.lastcap, T.gone = {}, {}, {}, {}, 0, -1, {}
        T.level, T.bidder, T.out, T.caller, T.partner, T.revealed = 0, -1, {}, -1, -1, false
        if T.game == "scopa" then
            for _ = 1, 4 do
                local c = table.remove(deck, 1) :: number
                table.insert(T.tbl, c)
                T.gone[c] = true
            end
        end
        local each = if T.game == "tresette" then 10 elseif T.game == "cinquiglio" then 8 else 3
        local s = nxt(T, T.dealer)
        for _ = 1, each * T.n do
            table.insert(T.hands[s], table.remove(deck, 1) :: number)
            s = nxt(T, s)
        end
        T.stock = deck
        if T.game == "briscola" then
            T.trump = deck[#deck]
        elseif T.game == "tresette" then
            if T.n == 4 then
                T.stock = {}
            end
            for q = 0, T.n - 1 do
                T.acc[q] = accusi(T.hands[q])
            end
        end
        T.phase = if T.game == "cinquiglio" then "bid" else "play"
        to_turn(T, nxt(T, T.dealer))
    end

    -- A new match of `game` for n: everybody sitting goes to the seat of the
    -- new game nearest the one they had, round the table; anybody left over
    -- watches. Everybody is told their seat again, since the seats moved.
    local function reseat(key: string, T: Table, n: number)
        local people = {}
        for s, p in T.humans do
            table.insert(people, { s / T.n, p })   -- where round the table, 0..1
        end
        T.n, T.humans = n, {}
        for _, sp in people do
            local want = math.floor(sp[1] * n + 0.5) % n
            local put = -1
            for k = 0, n - 1 do
                local q = (want + k) % n
                if T.humans[q] == nil then
                    put = q
                    break
                end
            end
            if put >= 0 then
                T.humans[put] = sp[2]
            end
            tell(sp[2], `sit {key} {put}`)
        end
    end

    local function start(key: string, T: Table, game: string, n: number)
        T.game = game
        reseat(key, T, n)
        T.score = per(teams(T), 0)
        T.dealer = math.random(n) - 1
        deal(T)
    end

    -- ------------------------------------------------------------- the count

    local function end_hand(T: Table)
        local nt = teams(T)
        local add = per(nt, 0)
        local function pts_of(t: number): number
            local p = 0
            for _, c in T.taken[t] do
                p += BR_PTS[value(c)]
            end
            return p
        end
        if T.game == "briscola" then
            local best, at, tie = -1, -1, false
            T.sum = {}
            for t = 0, nt - 1 do
                local p = pts_of(t)
                table.insert(T.sum, p)
                if p > best then
                    best, at, tie = p, t, false
                elseif p == best then
                    tie = true
                end
            end
            if not tie then
                add[at] = 1   -- a game won
            end
        elseif T.game == "cinquiglio" then
            local ours = pts_of(T.caller) + (if T.partner ~= T.caller then pts_of(T.partner) else 0)
            local made = ours >= bid_target(T.level)
            local alone = T.partner == T.caller
            for q = 0, T.n - 1 do
                local gain = if q == T.caller then (if alone then 4 else 2) elseif q == T.partner then 1 else -1
                add[q] = if made then gain else -gain
            end
            T.sum = { ours, 120 - ours, if made then 1 else 0, bid_target(T.level), T.caller, T.partner }
        elseif T.game == "tresette" then
            T.sum = {}
            for t = 0, nt - 1 do
                local th = T.thirds[t]
                for _, c in T.taken[t] do
                    th += TR_THIRDS[value(c)]
                end
                local acc = 0
                for q = 0, T.n - 1 do
                    if team(T, q) == t then
                        acc += T.acc[q]
                    end
                end
                add[t] = th // 3 + acc
                table.insert(T.sum, th // 3)
                table.insert(T.sum, acc)
            end
        else
            local a, b = T.taken[0], T.taken[1]
            local den = { [0] = 0, 0 }
            local seven = -1
            for t = 0, 1 do
                for _, c in T.taken[t] do
                    if suit(c) == 0 then
                        den[t] += 1
                    end
                    if c == 7 then
                        seven = t
                    end
                end
            end
            local pa, pb = primiera(a), primiera(b)
            local function most(x: number, y: number)
                if x ~= y then
                    add[if x > y then 0 else 1] += 1
                end
            end
            most(#a, #b)
            most(den[0], den[1])
            if seven >= 0 then
                add[seven] += 1
            end
            most(pa, pb)
            add[0] += T.scope[0]
            add[1] += T.scope[1]
            T.sum = { #a, #b, den[0], den[1], seven, pa, pb, T.scope[0], T.scope[1], add[0], add[1] }
        end
        local top, second = -math.huge, -math.huge
        for t = 0, nt - 1 do
            T.score[t] += add[t]
            if T.score[t] > top then
                top, second = T.score[t], top
            elseif T.score[t] > second then
                second = T.score[t]
            end
        end
        local goal = TARGET[T.game]
        T.phase = if goal ~= nil and top >= goal and top ~= second then "match" else "hand"
    end

    local function empty_hands(T: Table): boolean
        for s = 0, T.n - 1 do
            if #T.hands[s] > 0 then
                return false
            end
        end
        return true
    end

    local function trump_suit(T: Table): number
        return if T.trump > 0 then suit(T.trump) else -1
    end

    -- A full trick is shown a moment, then goes to who took it; with a stock
    -- every hand draws one, the taker first.
    local function collect(T: Table)
        local w = taker(T.game, T.trick, trump_suit(T))
        for _, t in T.trick do
            table.insert(T.taken[team(T, w)], t[2])
        end
        T.piles[w] += #T.trick
        T.trick = {}
        if #T.stock > 0 then
            local s = w
            for _ = 1, T.n do
                if #T.stock > 0 then
                    table.insert(T.hands[s], table.remove(T.stock, 1) :: number)
                end
                s = nxt(T, s)
            end
        end
        if empty_hands(T) then
            if T.game == "tresette" then
                T.thirds[team(T, w)] += 3   -- the last trick
            end
            end_hand(T)
        else
            T.phase = "play"
            to_turn(T, w)
        end
    end

    -- A scopa card shown with what it takes, then taken or laid down.
    local function settle(T: Table)
        local s, c = T.show[1], T.show[2]
        local took = { table.unpack(T.show, 3) }
        local t = team(T, s)
        T.show = {}
        if #took > 0 then
            for _, x in took do
                table.remove(T.tbl, table.find(T.tbl, x) :: number)
                table.insert(T.taken[t], x)
            end
            table.insert(T.taken[t], c)
            T.piles[s] += #took + 1
            T.lastcap = t
            if #T.tbl == 0 and not (empty_hands(T) and #T.stock == 0) then
                T.scope[t] += 1
            end
        else
            table.insert(T.tbl, c)
        end
        if empty_hands(T) then
            if #T.stock > 0 then
                local d = nxt(T, T.dealer)
                for _ = 1, 3 * T.n do
                    if #T.stock > 0 then
                        table.insert(T.hands[d], table.remove(T.stock, 1) :: number)
                    end
                    d = nxt(T, d)
                end
            else
                if T.lastcap >= 0 then   -- what is left goes to whoever took last
                    for _, x in T.tbl do
                        table.insert(T.taken[T.lastcap], x)
                    end
                    for q = 0, T.n - 1 do
                        if team(T, q) == T.lastcap then
                            T.piles[q] += #T.tbl
                            break
                        end
                    end
                    T.tbl = {}
                end
                end_hand(T)
                return
            end
        end
        T.phase = "play"
        to_turn(T, nxt(T, s))
    end

    -- Seat s plays card c; for scopa `take` is the set it takes (one of
    -- captures(), or nil when there is none).
    local function play(T: Table, s: number, c: number, take: { number }?)
        table.remove(T.hands[s], table.find(T.hands[s], c) :: number)
        T.gone[c] = true
        if T.game == "scopa" then
            T.show = { s, c }
            for _, x in take or {} do
                table.insert(T.show, x)
            end
            T.phase, T.timer = "show", if take then SHOW_WAIT else SHOW_WAIT * 0.6
            return
        end
        if #T.trick > 0 and suit(c) ~= suit(T.trick[1][2]) then
            T.void[s][suit(T.trick[1][2])] = true
        end
        if c == T.trump and T.game == "cinquiglio" then
            T.revealed = true
        end
        table.insert(T.trick, { s, c })
        if #T.trick == T.n then
            T.phase, T.timer = "trick", TRICK_WAIT
        else
            to_turn(T, nxt(T, s))
        end
    end

    -- ------------------------------------------------------ cinquiglio's call

    -- The auction moves on to the next seat that has not passed; it is over
    -- when the last bid stands alone, and dealt again when everybody passed.
    local function bid_on(T: Table)
        local left = 0
        for q = 0, T.n - 1 do
            left += if T.out[q] then 0 else 1
        end
        if left == 0 then
            T.dealer = nxt(T, T.dealer)
            deal(T)
            return
        end
        if (left == 1 and T.bidder >= 0 and not T.out[T.bidder]) or T.level >= MAX_BID then
            T.phase = "suit"
            to_turn(T, T.bidder)
            return
        end
        local s = nxt(T, T.turn)
        while T.out[s] do
            s = nxt(T, s)
        end
        to_turn(T, s)
    end

    local function bid(T: Table, s: number, level: number?)
        if level and level > T.level and level <= MAX_BID then
            T.level, T.bidder = level, s
        else
            T.out[s] = true
        end
        bid_on(T)
    end

    -- The caller names the suit: its card of the bid's value is called, the
    -- suit is trumps, and whoever holds that card is the partner.
    local function call(T: Table, suit_of: number)
        local c = suit_of * 10 + bid_value(T.level)
        T.caller, T.trump, T.revealed = T.bidder, c, false
        for q = 0, T.n - 1 do
            if table.find(T.hands[q], c) then
                T.partner = q
            end
        end
        T.phase = "play"
        to_turn(T, nxt(T, T.dealer))
    end

    -- ------------------------------------------------------- the table plays

    local function cheapest(cards: { number }, cost: (number) -> number): number
        local best, bc = cards[1], math.huge
        for _, c in cards do
            local k = cost(c)
            if k < bc then
                best, bc = c, k
            end
        end
        return best
    end

    -- Whom seat s knows to be on its side.
    local function ally(T: Table, s: number, w: number): boolean
        if w == s then
            return true
        elseif T.game == "cinquiglio" then
            if s == T.caller then
                return T.revealed and w == T.partner
            elseif s == T.partner then
                return w == T.caller
            end
            return w ~= T.caller and T.revealed and w ~= T.partner
        end
        return T.n == 4 and team(T, w) == team(T, s)
    end

    local function ally_holds(T: Table, s: number): boolean
        return #T.trick > 0 and ally(T, s, taker(T.game, T.trick, trump_suit(T)))
    end

    local function wins(T: Table, s: number, c: number): boolean
        if #T.trick == 0 then
            return true
        end
        local t = table.clone(T.trick)
        table.insert(t, { s, c })
        return taker(T.game, t, trump_suit(T)) == s
    end

    -- Whether nothing that could still be played beats c in its own suit:
    -- every higher card of it has been played or is in s's own hand.
    local function master(T: Table, s: number, c: number): boolean
        local rank = if rules(T.game) == "briscola" then BR_RANK else TR_RANK
        for v = 1, 10 do
            local d = suit(c) * 10 + v
            if rank[v] > rank[value(c)] and not T.gone[d] and not table.find(T.hands[s], d) then
                return false
            end
        end
        return true
    end

    local function bot_briscola(T: Table, s: number): number
        local hand, ts = T.hands[s], trump_suit(T)
        local function plain(c: number): number   -- what throwing c away costs
            return BR_PTS[value(c)] * 3 + BR_RANK[value(c)] + (if suit(c) == ts then 40 else 0)
        end
        if #T.trick == 0 then
            return cheapest(hand, plain)
        end
        local pts = 0
        for _, t in T.trick do
            pts += BR_PTS[value(t[2])]
        end
        if ally_holds(T, s) then
            -- load the ally's trick with points that are not trumps
            return cheapest(hand, function(c)
                return if suit(c) == ts then 100 + BR_RANK[value(c)] else -BR_PTS[value(c)]
            end)
        end
        local best, bv = nil, math.huge
        for _, c in hand do
            if wins(T, s, c) then
                local trump = suit(c) == ts
                if not trump or pts + BR_PTS[value(c)] >= 10 or #T.stock == 0 then
                    local k = if trump then 50 + BR_RANK[value(c)] else -BR_PTS[value(c)]
                    if k < bv then
                        best, bv = c, k
                    end
                end
            end
        end
        return best or cheapest(hand, plain)
    end

    local function bot_tresette(T: Table, s: number): number
        local ok = playable("tresette", T.hands[s], T.trick)
        local legal = {}
        for _, c in T.hands[s] do
            if ok[c] then
                table.insert(legal, c)
            end
        end
        if #T.trick == 0 then
            -- near the end, a card nothing can beat takes what is played on it, best where the
            -- others still have the suit (earlier it only throws away the strength: measured, 1000
            -- hands a game against the table without it, 684-291 at two and 572-383 at four)
            local tops = {}
            for _, c in legal do
                if master(T, s, c) then
                    table.insert(tops, c)
                end
            end
            if #tops > 0 and #T.stock == 0 and #T.hands[s] <= 6 then
                return cheapest(tops, function(c)
                    local voids = 0
                    for q = 0, T.n - 1 do
                        voids += if q ~= s and T.void[q][suit(c)] then 1 else 0
                    end
                    return voids * 10 - TR_THIRDS[value(c)]
                end)
            end
            -- else the smallest of the longest suit
            local count = { [0] = 0, 0, 0, 0 }
            for _, c in legal do
                count[suit(c)] += 1
            end
            return cheapest(legal, function(c)
                return TR_RANK[value(c)] + TR_THIRDS[value(c)] * 4 - count[suit(c)] * 3
            end)
        end
        if ally_holds(T, s) then
            return cheapest(legal, function(c)
                return if suit(c) == suit(T.trick[1][2]) then -TR_THIRDS[value(c)] * 10 + TR_RANK[value(c)]
                    else -TR_THIRDS[value(c)]
            end)
        end
        local taking = {}
        for _, c in legal do
            if wins(T, s, c) then
                table.insert(taking, c)
            end
        end
        if #taking > 0 then
            return cheapest(taking, function(c)
                return TR_RANK[value(c)]
            end)
        end
        return cheapest(legal, function(c)
            return TR_THIRDS[value(c)] * 10 + TR_RANK[value(c)]
        end)
    end

    local function bot_scopa(T: Table, s: number): (number, { number }?)
        local last = #T.stock == 0
        for q = 0, T.n - 1 do
            last = last and (q == s and #T.hands[q] == 1 or q ~= s and #T.hands[q] == 0)
        end
        -- how many cards of value v the others may still hold
        local function unseen(v: number): number
            local k = 0
            for su = 0, 3 do
                local d = su * 10 + v
                if not T.gone[d] and not table.find(T.hands[s], d) then
                    k += 1
                end
            end
            return k
        end
        local function worth(cards: { number }): number
            local w = 0
            for _, c in cards do
                w += 1 + (if suit(c) == 0 then 1.5 else 0) + (if value(c) == 7 then 2 else 0)
                    + (if c == 7 then 10 else 0)
            end
            return w
        end
        -- the danger of leaving a table that adds up to `sum` (a scopa for the next), by the
        -- cards of that value still out (measured: 469-392 in 1000 hands against a flat 4)
        local function risk(sum: number): number
            if last or sum == 0 or sum > 10 then
                return 0
            end
            return math.min(unseen(sum), 2) * 3
        end
        local best, take, bv = T.hands[s][1], nil, -math.huge
        for _, c in T.hands[s] do
            local ways = captures(T.tbl, c)
            for _, w in ways do
                local all = table.clone(w)
                table.insert(all, c)
                local left = 0
                for _, t in T.tbl do
                    left += if table.find(w, t) then 0 else value(t)
                end
                local v = worth(all) + (if #w == #T.tbl and not last then 15 else 0) - risk(left)
                if v > bv then
                    best, take, bv = c, w, v
                end
            end
            if #ways == 0 then
                -- laid down: what it gives away if the next can take it, and a small table to sweep
                local sum = value(c)
                for _, t in T.tbl do
                    sum += value(t)
                end
                local exposed = if unseen(value(c)) > 0 then 1 else 0.3
                local v = -worth({ c }) * exposed - risk(sum)
                if v > bv then
                    best, take, bv = c, nil, v
                end
            end
        end
        return best, take
    end

    -- Cinquiglio: how good a hand is with `x` for trumps.
    local function strength(hand: { number }, x: number): number
        local k = 0
        for _, c in hand do
            k += BR_PTS[value(c)] * 0.6
            if suit(c) == x then
                k += 4 + BR_RANK[value(c)] * 0.8
            end
        end
        return k
    end

    local function bot_bid(T: Table, s: number)
        local best = 0
        for x = 0, 3 do
            best = math.max(best, strength(T.hands[s], x))
        end
        local willing = math.clamp(math.floor((best - 22) / 2.2), 0, 20)
        bid(T, s, if T.level + 1 <= willing then T.level + 1 else nil)
    end

    local function bot_call(T: Table)
        local s = T.bidder
        local v = bid_value(T.level)
        local pick, pv = 0, -math.huge
        for x = 0, 3 do
            -- a suit whose called card is somebody else's, unless the hand is very strong
            local mine = table.find(T.hands[s], x * 10 + v) ~= nil
            local k = strength(T.hands[s], x) - (if mine then 25 else 0)
            if k > pv then
                pick, pv = x, k
            end
        end
        call(T, pick)
    end

    local function bot(T: Table, s: number)
        if T.phase == "bid" then
            bot_bid(T, s)
        elseif T.phase == "suit" then
            bot_call(T)
        elseif rules(T.game) == "briscola" then
            play(T, s, bot_briscola(T, s))
        elseif T.game == "tresette" then
            play(T, s, bot_tresette(T, s))
        else
            local c, take = bot_scopa(T, s)
            play(T, s, c, take)
        end
    end

    -- --------------------------------------------------------- the tables

    local ready, laid = false, true
    core.event.on("core:world_ready", function()
        -- Only where there is a square: mods/guard's, the sandbox's. The
        -- Inferno loads this mod for what it holds, not for a table at its door.
        ready, laid = true, core.storage_get("plaza") == "1" or core.node_id("guard:cloud") == 0
    end)

    -- A table on the spawn's square, once a world: three nodes off where
    -- players arrive, the other side of the chess table. Moved round if that
    -- cell is taken; left out if no cell near is free.
    core.event.on("core:tick", function(_)
        local sx, sy, sz = core.world_spawn()
        if not ready or laid or sx == nil then
            return
        end
        local x0, y, z0 = math.floor(sx) - 3, math.floor(sy), math.floor(sz) + 3
        if core.loaded(x0, y, z0) == 0 then
            return
        end
        laid = true
        core.storage_set("plaza", "1")
        for _, d in { { 0, 0 }, { -1, 0 }, { 0, 1 }, { 1, 0 }, { 0, -1 }, { -2, 0 }, { 0, 2 } } do
            local x, z = x0 + d[1], z0 + d[2]
            if core.get_block(x, y, z) == 0 and core.get_block(x, y + 1, z) == 0
                and core.get_block(x, y - 1, z) ~= 0 then
                core.set_block(x, y, z, TABLE)
                core.set_facing(x, y, z, 2)
                core.log(`cards: a table on the spawn's square at {x} {y} {z}`)
                return
            end
        end
        core.log("cards: no free cell by the spawn for a table")
    end)

    local function table_at(x: number, y: number, z: number): (string, Table)
        local key = key_of(x, y, z)
        local T = tables[key]
        if T == nil then
            T = fresh(core.get_facing(x, y, z))
            tables[key] = T
            keep_list()
        end
        return key, T
    end

    core.event.on("core:broken", function(ev)
        local key = key_of(ev.x, ev.y, ev.z)
        local T = tables[key]
        if T == nil then
            return
        end
        for _, p in T.humans do
            tell(p, "out")
        end
        tables[key] = nil
        keep_list()
        core.storage_set("t" .. key, "")
        if drawing then
            heard(`g {key}`)
        else
            for _, p in core.players() do
                core.net.send(p, `g {key}`)
            end
        end
    end)

    local function near(p: number, key: string, reach: number): boolean
        local x, y, z = cell_of(key)
        local px, py, pz = core.player_pos(p)
        if px == nil then
            return false
        end
        return math.abs(px - x - 0.5) < reach and math.abs(py - y) < reach and math.abs(pz - z - 0.5) < reach
    end

    -- The seat nearest where the player stands, round the table: seat 0 is
    -- the side the table faces, and the seats go round to its right.
    local function side_of(T: Table, key: string, p: number): number
        local x, _, z = cell_of(key)
        local px, _, pz = core.player_pos(p)
        local a = T.facing * math.pi / 2
        local wx, wz = (px or 0) - x - 0.5, (pz or 0) - z - 0.5
        local u = wx * math.cos(a) - wz * math.sin(a)
        local v = wx * math.sin(a) + wz * math.cos(a)
        local turn = math.atan2(u, v) / (2 * math.pi)
        return math.floor(turn * T.n + 0.5) % T.n
    end

    -- Right click: sit on your side if it is free, else at another free seat,
    -- else watch.
    core.event.on("core:used", function(ev)
        if ev.id ~= TABLE then
            return
        end
        ev.cancel = true
        local key, T = table_at(ev.x, ev.y, ev.z)
        local p = ev.player
        local s = seat_of(T, p)
        if s < 0 then
            local want = side_of(T, key, p)
            for k = 0, T.n - 1 do
                local q = (want + k) % T.n
                if T.humans[q] == nil then
                    s = q
                    break
                end
            end
            if s >= 0 then
                T.humans[s] = p
            end
        end
        everybody(key, T)
        tell(p, `sit {key} {s}`)
    end)

    local function stand(T: Table, p: number)
        local s = seat_of(T, p)
        if s >= 0 then
            T.humans[s] = nil
            T.timer = BOT_WAIT   -- the table takes the seat over, not at once
        end
    end

    -- A panel of rows: { "label", text }, { id, text } a button across, or
    -- { { id, text }, { id, text } } two buttons side by side.
    local function panel(name: string, rows: { any }, p: number)
        local h = 0.5
        for _, r in rows do
            h += if r[1] == "label" then 0.8 else 1.1
        end
        core.ui_define(name, 12, h + 0.2)
        local y = 0.4
        for _, r in rows do
            if r[1] == "label" then
                core.ui_label(name, 0.5, y, r[2])
                y += 0.8
            elseif type(r[1]) == "table" then
                for i, b in r do
                    core.ui_button(name, b[1], 0.5 + (i - 1) * 5.6, y, 5.4, 0.9, b[2])
                end
                y += 1.1
            else
                core.ui_button(name, r[1], 0.5, y, 11, 0.9, r[2])
                y += 1.1
            end
        end
        core.ui_show(name, true, p)
    end

    -- Cinquiglio's questions go up on the screen of the player whose turn it
    -- is: a bid, or (having won the auction) the suit.
    -- A question in one row, short and wide: a panel sits in the middle of the
    -- screen, and a tall one covers the hand it is about.
    local function row_panel(name: string, label: string, buttons: { { string } }, p: number)
        local bw = 4.8   -- cells: a hotbar slot each, 30 at most across
        core.ui_define(name, 0.6 + #buttons * (bw + 0.2), 2.3)
        core.ui_label(name, 0.5, 0.35, label)
        for i, b in buttons do
            core.ui_button(name, b[1], 0.5 + (i - 1) * (bw + 0.2), 1.1, bw, 0.9, b[2])
        end
        core.ui_show(name, true, p)
    end

    local asked: { [number]: string } = {}   -- player -> what their panel is about
    show_bid = function(key: string, T: Table)
        local p = T.humans[T.turn]
        local about = `{key} {T.phase} {T.level} {T.turn}`
        if p == nil or T.game ~= "cinquiglio" or (T.phase ~= "bid" and T.phase ~= "suit") or asked[p] == about then
            return
        end
        asked[p] = about
        menu_of[p] = key
        if T.phase == "bid" then
            local buttons = {}
            for l = T.level + 1, math.min(T.level + 3, MAX_BID) do
                table.insert(buttons, { `l{l}`, `Chiamo {bid_name(l)}` })
            end
            table.insert(buttons, { "pass", "Passo" })
            row_panel(`cards:b{p}`, if T.level == 0 then "Asta: nessuno ha chiamato" else `Asta: si chiama {bid_name(T.level)}`,
                      buttons, p)
        else
            local v = bid_value(T.level)
            local buttons = {}
            for x = 0, 3 do
                local mine = table.find(T.hands[T.turn], x * 10 + v) ~= nil
                table.insert(buttons, { `x{x}`, `{VALUE_NAME[v]} di {SUIT_NAME[x]}` .. (if mine then " (tuo)" else "") })
            end
            row_panel(`cards:b{p}`, `Hai vinto l'asta con {bid_name(T.level)}: quale carta chiami? (la tua: da solo)`, buttons, p)
        end
    end

    -- Two to a row in the menu.
    local CHOICES = { { "briscola", 2, "Briscola a due" }, { "briscola", 3, "Briscola a tre" },
                      { "briscola", 4, "Briscola a 4, a coppie" }, { "cinquiglio", 5, "Cinquiglio, in cinque" },
                      { "scopa", 2, "Scopa a due" }, { "scopa", 4, "Scopa a 4, a coppie" },
                      { "tresette", 2, "Tresette a due" }, { "tresette", 4, "Tresette a 4, a coppie" } }

    local function show_menu(p: number, key: string, T: Table, confirm: boolean?)
        menu_of[p] = key
        local rows = {}
        if seat_of(T, p) < 0 then
            panel(`cards:m{p}`, { { "label", "Guardi la partita" }, { "up", "Alzati" }, { "close", "Chiudi" } }, p)
            return
        end
        local busy = T.phase ~= "idle" and T.phase ~= "hand" and T.phase ~= "match"
        if busy and confirm then
            panel(`cards:m{p}`, { { "label", "Lasci la partita a metà?" }, { "quit_yes", "Sì, smetti" },
                                  { "close", "No, continua" } }, p)
            return
        end
        if busy or T.phase == "hand" then
            table.insert(rows, { "quit", `Smetti il gioco ({GAME_NAME[T.game]})` })
        end
        table.insert(rows, { "deck", if T.deck == "e" then "Mazzo illustrato: passa al classico" else "Mazzo classico: passa all'illustrato" })
        table.insert(rows, { "label", if T.phase == "idle" then "A che si gioca?" else "Un'altra partita:" })
        for i = 1, #CHOICES, 2 do
            table.insert(rows, { { `g{i}`, CHOICES[i][3] }, { `g{i + 1}`, CHOICES[i + 1][3] } })
        end
        table.insert(rows, { { "up", "Alzati" }, { "close", "Chiudi" } })
        panel(`cards:m{p}`, rows, p)
    end

    local function to_idle(key: string, T: Table)
        T.phase, T.game, T.trick, T.show, T.tbl, T.trump, T.stock = "idle", "", {}, {}, {}, 0, {}
        T.hands, T.piles = per(5, {}), per(5, 0)
        reseat(key, T, 4)   -- everybody back to the four sides
    end

    core.event.on("core:ui_click", function(ev)
        local p = ev.player
        if ev.panel == `cards:b{p}` then
            core.ui_show(ev.panel, false, p)
            asked[p] = nil
            local key = menu_of[p] or ""
            local T = tables[key]
            if T == nil or T.game ~= "cinquiglio" or T.humans[T.turn] ~= p then
                return
            end
            local l = tonumber(string.match(ev.id, "^l(%d+)$") or "")
            local x = tonumber(string.match(ev.id, "^x(%d)$") or "")
            if T.phase == "bid" and (l or ev.id == "pass") then
                bid(T, T.turn, l)
            elseif T.phase == "suit" and x and x >= 0 and x <= 3 then
                call(T, x)
            end
            everybody(key, T)
            return
        end
        if ev.panel == `cards:t{p}` then
            -- which cards a scopa card takes
            core.ui_show(ev.panel, false, p)
            local waiting = choosing[p]
            choosing[p] = nil
            local i = tonumber(string.match(ev.id, "^o(%d+)$") or "")
            if waiting == nil or i == nil then
                return
            end
            local key, c = waiting[1], waiting[2]
            local T = tables[key]
            if T and T.phase == "play" and T.turn == seat_of(T, p) and table.find(T.hands[T.turn], c) then
                local ways = captures(T.tbl, c)
                if ways[i] then
                    play(T, T.turn, c, ways[i])
                    everybody(key, T)
                end
            end
            return
        end
        if ev.panel ~= `cards:m{p}` then
            return
        end
        core.ui_show(ev.panel, false, p)
        local key = menu_of[p] or ""
        local T = tables[key]
        if T == nil then
            return
        end
        local id = ev.id
        local g = tonumber(string.match(id, "^g(%d)$") or "")
        if id == "up" then
            stand(T, p)
            tell(p, "out")
        elseif seat_of(T, p) < 0 then
            return
        elseif id == "quit" then
            if T.phase ~= "hand" then
                show_menu(p, key, T, true)
                return
            end
            to_idle(key, T)
        elseif id == "quit_yes" then
            to_idle(key, T)
        elseif id == "deck" then
            T.deck = if T.deck == "e" then "" else "e"
        elseif g and CHOICES[g] then
            start(key, T, CHOICES[g][1], CHOICES[g][2])
        else
            return
        end
        everybody(key, T)
    end)

    from_client = function(p: number, text: string)
        local w = string.split(text, " ")
        if w[1] == "hi" then
            for key, T in tables do
                tell(p, line_for(key, T, p))
            end
            return
        end
        local key = w[2] or ""
        local T = tables[key]
        if T == nil then
            return
        end
        local s = seat_of(T, p)
        if w[1] == "menu" then
            if T.game == "cinquiglio" and (T.phase == "bid" or T.phase == "suit") and T.turn == s then
                asked[p] = nil   -- the question again, if it was closed
                show_bid(key, T)
                return
            end
            show_menu(p, key, T)
        elseif w[1] == "new" and s >= 0 then
            if T.phase == "hand" then
                T.dealer = nxt(T, T.dealer)
                deal(T)
            elseif T.phase == "match" then
                start(key, T, T.game, T.n)
            end
            everybody(key, T)
        elseif w[1] == "play" and s >= 0 then
            local c = tonumber(w[3]) or 0
            if T.phase ~= "play" or T.turn ~= s or not table.find(T.hands[s], c)
                or not playable(T.game, T.hands[s], T.trick)[c] or not near(p, key, 8) then
                tell(p, line_for(key, T, p))   -- puts the card back in the hand
                return
            end
            if T.game == "scopa" then
                local ways = captures(T.tbl, c)
                if #ways > 1 then
                    choosing[p] = { key, c }
                    local rows = { { "label", `Con il {name_of(c)} prendi:` } }
                    for i, set in ways do
                        local names = {}
                        for _, x in set do
                            table.insert(names, name_of(x))
                        end
                        table.insert(rows, { `o{i}`, table.concat(names, " + ") })
                    end
                    panel(`cards:t{p}`, rows, p)
                    return
                end
                play(T, s, c, ways[1])
            else
                play(T, s, c)
            end
            everybody(key, T)
        end
    end

    if not drawing then
        core.event.on("core:net_message", function(ev)
            from_client(ev.player, ev.text)
        end)
    end

    -- The table's own seats, the pauses, and whoever left or walked away gets up.
    local idle = 0.0
    core.event.on("core:tick", function(ev)
        for key, T in tables do
            if next(T.humans) == nil then
                continue   -- nobody at it: it waits
            end
            T.timer -= ev.dt
            if T.timer > 0 then
                continue
            end
            if T.phase == "trick" then
                collect(T)
                everybody(key, T)
            elseif T.phase == "show" then
                settle(T)
                everybody(key, T)
            elseif (T.phase == "play" or T.phase == "bid" or T.phase == "suit") and T.humans[T.turn] == nil then
                bot(T, T.turn)
                everybody(key, T)
            end
        end
        idle += ev.dt
        if idle < 1.0 then
            return
        end
        idle = 0.0
        local here = {}
        for _, p in core.players() do
            here[p] = true
        end
        for key, T in tables do
            for _, p in T.humans do
                if not (here[p] and near(p, key, 8)) then
                    stand(T, p)
                    if here[p] then
                        tell(p, "out")
                    end
                    everybody(key, T)
                end
            end
        end
    end)
end

-- ==================================================================== client

if drawing then
    type Seen = {
        x: number, y: number, z: number, facing: number,
        game: string, n: number, phase: string, turn: number, dealer: number, me: number,
        hand: { number }, counts: { number }, trick: { { number } }, tbl: { number },
        stock: number, trump: number, piles: { number }, score: { number }, scope: { number },
        show: { number }, sum: { number }, names: { string }, x_: { number }, deck: string,
        fly: { [number]: Flight },   -- the cards on their way to the table, by card
        at: { [number]: { number } },  -- where each card played was last drawn: x, y, z, yaw
        gather: { Gathered },          -- the cards on their way to a pile
        incoming: { Incoming },        -- backs drawn from the stock, on their way to another's hand
    }
    -- A card somebody else drew, flying from the stock to seat q's hand.
    type Incoming = { q: number, t0: number, from: { number } }
    -- A card taken, flying from where it lay to the top of seat q's pile.
    type Gathered = { c: number, from: { number }, q: number, t0: number }
    local GATHER_TIME = 0.6
    -- A card flying from a hand to the table: where it left from (x, y, z, yaw,
    -- pitch, roll) and when, on `clock`.
    -- `flip`: drawn from the stock, it leaves on its back and turns face up.
    -- A t0 still to come is a card not yet drawn: it is not drawn anywhere.
    type Flight = { from: { number }, t0: number, flip: boolean? }
    local FLY_TIME = 0.35
    local clock = 0
    -- Where each card of this player's own hand was drawn last, as a Flight's
    -- `from`: a card played leaves from there.
    local last_at: { [number]: { number } } = {}
    local seen: { [string]: Seen } = {}
    local sitting: string? = nil   -- the table this player sits at or watches
    local seat = -1                -- where, -1 watching
    local above = false            -- seated, the view from above instead of the eye (V)

    local function ask(text: string)
        if serving then
            local me = core.players()[1]
            if me then
                from_client(me, text)
            end
        else
            core.net.send(0, text)
        end
    end

    local function view_seat(s: Seen): number
        return if seat >= 0 then seat else 0
    end

    -- The yaw of seat q's frame: seats evenly round the table from the side it faces.
    local function yaw_of(s: Seen, q: number): number
        return seat_yaw(s.facing, s.n, q)
    end

    -- A point (u, v) of seat q's own frame -- u to its right, v towards it from
    -- the table's middle -- to the world.
    local function world_of(s: Seen, q: number, u: number, v: number): (number, number, number)
        local a = yaw_of(s, q)
        return s.x + 0.5 + u * math.cos(a) + v * math.sin(a), s.y + TOP, s.z + 0.5 - u * math.sin(a) + v * math.cos(a)
    end

    local function get_up()
        sitting, seat = nil, -1
        core.fixed_view(0, 0, 0, 0, 0, 50, 0, 0.8)
        core.engine_hud(true)
        core.player_clip(core.players()[1] or 0, "")
    end

    -- Seated, the camera is the eye of the body at the seat, looking down at
    -- the table with the cursor free: the arms hold the cards (core.player_clip).
    local EYE_PITCH, EYE_FOV = 40, 70

    local function eye_view(s: Seen): boolean
        return seat >= 0 and not above
    end

    -- The eye, and its right, up and ahead, while eye_view.
    local function eye_frame(s: Seen): ({ number }, { number }, { number }, { number })
        local a = yaw_of(s, seat)
        local sa, ca = math.sin(a), math.cos(a)
        local sp, cp = math.sin(math.rad(EYE_PITCH)), math.cos(math.rad(EYE_PITCH))
        local f = { -cp * sa, -sp, -cp * ca }
        local x, z = s.x + 0.5 + SEAT_R * sa, s.z + 0.5 + SEAT_R * ca
        local eye = { x - f[1] * 0.05, s.y + EYE_H - f[2] * 0.05, z - f[3] * 0.05 }
        return eye, { ca, 0, -sa }, { -sa * sp, cp, -ca * sp }, f
    end

    local function place_camera(s: Seen)
        local yaw = -math.deg(yaw_of(s, view_seat(s)))
        if eye_view(s) then
            local a = yaw_of(s, seat)
            core.fixed_view(s.x + 0.5 + SEAT_R * math.sin(a), s.y + EYE_H, s.z + 0.5 + SEAT_R * math.cos(a),
                            0.05, yaw, EYE_PITCH, EYE_FOV, 1.2)
        else
            local ax, az = -math.sin(math.rad(yaw)) * 0.12, math.cos(math.rad(yaw)) * 0.12
            core.fixed_view(s.x + 0.5 + ax, s.y + TOP, s.z + 0.5 + az, 1.05, yaw, 64, 0, 1.2)
        end
        core.engine_hud(false)
        core.player_clip(core.players()[1] or 0, if seat >= 0 then "cards" else "")
    end

    -- Where a card seat q plays leaves from: this player's own from where it
    -- was drawn in the hand, somebody else's from before them -- the grip of a
    -- hand held up, or the row of one lying on the table.
    local function source(key: string, s: Seen, q: number, c: number): { number }
        if key == sitting and q == seat and last_at[c] then
            return last_at[c]
        end
        local a = yaw_of(s, q)
        if (s.names[q + 1] or "") ~= "" then
            local fx, fz = -math.sin(a), -math.cos(a)
            return { s.x + 0.5 - fx * (SEAT_R - GRIP_FWD), s.y + GRIP_UP, s.z + 0.5 - fz * (SEAT_R - GRIP_FWD), a, 0, 0 }
        end
        local x, y, z = world_of(s, q, 0, if s.n >= 5 then 0.41 else 0.40)
        return { x, y, z, a, 0, 0 }
    end

    -- The top of the stock, as a Flight's `from`, seen from seat vs: n cards
    -- in it; briscola's in the middle, scopa's in its corner, and with none
    -- (a game dealt out whole) the middle of the table.
    local DRAW_GAP = 0.08
    local function stock_pose(s: Seen, vs: number, n: number): { number }
        local a = yaw_of(s, vs)
        local x, y, z
        if s.game == "scopa" then
            x, y, z = world_of(s, vs, -0.25, -0.25)
            a += math.pi / 2
        else
            x, y, z = world_of(s, vs, if n > 0 then -0.03 else 0, 0)
        end
        return { x, y + 0.0004 + math.ceil(n / 5) * 0.009, z, a, 0, 0 }
    end

    -- The cards played since `was`: into the trick, or onto scopa's table
    -- (shown being played, then laid down among the others), each sent flying.
    local function launch(key: string, was: Seen, s: Seen)
        local function go(c: number, from: { number })
            s.fly[c] = { from = from, t0 = clock }
        end
        local before = {}
        for _, t in was.trick do
            before[t[2]] = true
        end
        for _, t in s.trick do
            if not before[t[2]] then
                go(t[2], source(key, s, t[1], t[2]))
            end
        end
        if #s.show >= 2 and s.show[2] ~= was.show[2] then
            go(s.show[2], source(key, s, s.show[1], s.show[2]))
        end
        if #was.show >= 2 and not table.find(was.tbl, was.show[2]) and table.find(s.tbl, was.show[2]) then
            local a = yaw_of(s, view_seat(s))
            local x, y, z = world_of(s, was.show[1], 0, 0.2)
            go(was.show[2], { x, y + 0.02, z, a, 0, 0 })
        end
        for c, f in s.fly do
            if clock - f.t0 > FLY_TIME then
                s.fly[c] = nil
            end
        end

        for i = #s.incoming, 1, -1 do
            if clock - s.incoming[i].t0 > FLY_TIME then
                table.remove(s.incoming, i)
            end
        end
        -- The cards drawn since `was` (a hand dealt too): from the stock,
        -- round the table a card at a time; this player's own turned face up
        -- on the way, the others' backs to their hands.
        local vs = if key == sitting then view_seat(s) else 0
        local st = stock_pose(s, vs, was.stock)
        local had = {}
        for _, c in was.hand do
            had[c] = true
        end
        local mine, theirs, rounds = {}, {}, 0
        for _, c in s.hand do
            if not had[c] then
                table.insert(mine, c)
            end
        end
        for q = 0, s.n - 1 do
            theirs[q] = if q == s.me then #mine else math.max((s.counts[q + 1] or 0) - (was.counts[q + 1] or 0), 0)
            rounds = math.max(rounds, theirs[q])
        end
        -- after the cards taken have gone to their pile, if any were
        local k = 0
        for q = 0, s.n - 1 do
            if (s.piles[q + 1] or 0) > (was.piles[q + 1] or 0) then
                k = math.ceil((GATHER_TIME + 0.2) / DRAW_GAP)
            end
        end
        -- scopa's table laid at a deal, after the hands
        local laid = {}
        for _, c in s.tbl do
            if not table.find(was.tbl, c) and c ~= was.show[2] then
                table.insert(laid, c)
            end
        end
        if rounds > 0 and rounds <= 10 then
            for r = 1, rounds do
                for d = 1, s.n do
                    local q = (s.dealer + d) % s.n
                    if r <= theirs[q] then
                        if q == s.me then
                            s.fly[mine[r]] = { from = st, t0 = clock + k * DRAW_GAP, flip = true }
                        else
                            table.insert(s.incoming, { q = q, t0 = clock + k * DRAW_GAP, from = st })
                        end
                        k += 1
                    end
                end
            end
        end
        for _, c in laid do
            s.fly[c] = { from = st, t0 = clock + k * DRAW_GAP, flip = true }
            k += 1
        end

        -- The cards that were on the table and are not now, gone to the pile
        -- that grew the most: one after the other, from where they lay.
        local taker, grew = -1, 0
        for q = 0, s.n - 1 do
            local d = (s.piles[q + 1] or 0) - (was.piles[q + 1] or 0)
            if d > grew then
                taker, grew = q, d
            end
        end
        local kept = {}
        for i = #s.gather, 1, -1 do
            if clock - s.gather[i].t0 > GATHER_TIME then
                table.remove(s.gather, i)
            end
        end
        if taker < 0 then
            return
        end
        for _, t in s.trick do
            kept[t[2]] = true
        end
        for _, c in s.tbl do
            kept[c] = true
        end
        if #s.show >= 2 then
            kept[s.show[2]] = true
        end
        local gone = {}
        for _, t in was.trick do
            table.insert(gone, t[2])
        end
        for _, c in was.tbl do
            table.insert(gone, c)
        end
        if #was.show >= 2 then
            table.insert(gone, was.show[2])
        end
        local j = 0
        for _, c in gone do
            if not kept[c] and s.at[c] then
                table.insert(s.gather, { c = c, from = s.at[c], q = taker, t0 = clock + j * 0.06 })
                s.at[c] = nil
                j += 1
            end
        end
    end

    heard = function(text: string)
        local w = string.split(text, " ")
        if w[1] == "s" and #w >= 22 then
            -- s key facing game n phase turn dealer me hand counts trick table stock trump piles
            -- score scope show sum names x
            local x, y, z = cell_of(w[2])
            local trick = {}
            if w[12] ~= "-" then
                for _, t in string.split(w[12], ",") do
                    local a, b = string.match(t, "^(%d+):(%d+)$")
                    if a then
                        table.insert(trick, { tonumber(a) :: number, tonumber(b) :: number })
                    end
                end
            end
            local names = string.split(w[21], "|")
            for i, n in names do
                names[i] = if n == "-" then "" else (string.gsub(n, "_", " "))
            end
            local hand = uncsv(w[10])
            table.sort(hand)   -- by suit, and in a suit by value
            local was = seen[w[2]]
            local s: Seen = {
                x = x, y = y, z = z, facing = tonumber(w[3]) or 0,
                game = if w[4] == "-" then "" else w[4], n = tonumber(w[5]) or 4, phase = w[6],
                turn = tonumber(w[7]) or 0, dealer = tonumber(w[8]) or 0, me = tonumber(w[9]) or -1,
                hand = hand, counts = uncsv(w[11]), trick = trick, tbl = uncsv(w[13]),
                stock = tonumber(w[14]) or 0, trump = tonumber(w[15]) or 0, piles = uncsv(w[16]),
                score = uncsv(w[17]), scope = uncsv(w[18]), show = uncsv(w[19]), sum = uncsv(w[20]),
                names = names, x_ = uncsv(w[22]), deck = if w[23] == nil or w[23] == "-" then "" else w[23],
                fly = if was then was.fly else {},
                at = if was then was.at else {}, gather = if was then was.gather else {},
                incoming = if was then was.incoming else {},
            }
            if was then
                launch(w[2], was, s)
            end
            seen[w[2]] = s
            if sitting == w[2] and was and was.n ~= s.n then
                place_camera(s)   -- the seats moved round the table
            end
        elseif w[1] == "g" then
            seen[w[2]] = nil
            if sitting == w[2] then
                get_up()
            end
        elseif w[1] == "sit" then
            local s = seen[w[2]]
            if s == nil then
                core.log(`cards: told to sit at {w[2]}, a table this machine has not been told of`)
                return
            end
            sitting, seat = w[2], tonumber(w[3]) or -1
            core.log(`cards: sitting at {w[2]}, seat {seat}`)
            place_camera(s)
        elseif w[1] == "out" then
            get_up()
        end
    end

    if not serving then
        core.event.on("core:net_message", function(ev)
            heard(ev.text)
        end)
    end

    -- The card played under the cursor (set by the drawing), and how far each
    -- has come up to be read, 0 to 1, eased by the tick.
    local over_table: number? = nil
    local raised: { [number]: number } = {}
    local RAISE_TIME = 0.15

    local said_hi = false
    core.event.on("core:tick", function(ev)
        clock += ev.dt
        for c, r in raised do
            r += (if c == over_table then 1 else -1) * ev.dt / RAISE_TIME
            raised[c] = if r <= 0 then nil else math.min(r, 1)
        end
        if over_table and raised[over_table] == nil then
            raised[over_table] = math.min(ev.dt / RAISE_TIME, 1)
        end
        if said_hi then
            return
        end
        if serving then
            said_hi = core.players()[1] ~= nil
        else
            said_hi = core.net.open() ~= nil
        end
        if said_hi then
            ask("hi")
        end
    end)

    -- Where the cards of this player's hand lie, in their own frame: a row
    -- along the near edge, overlapping when there are many.
    local HAND_V = 0.37
    local function hand_u(k: number, i: number, room: number): number
        local step = math.min(CW + 0.01, room / math.max(k - 1, 1))
        return (i - (k + 1) / 2) * step
    end

    local function my_room(s: Seen): number
        return if s.n >= 5 then 0.4 else 0.56
    end

    -- In the eye view, the card of the fan under the cursor (set further down).
    local fan_under: (Seen, { number }, { number }) -> number? = function(_, _, _) return nil end

    -- The card under the cursor last frame: it comes out of the hand and over
    -- the others to be read (suit and all), so it is looked for first, where it
    -- is drawn -- otherwise the one it now covers would take the cursor back.
    local hovered: number? = nil
    local HOVER_FAN, HOVER_TABLE = 0.035, 0.05   -- how far it comes out

    -- The card of the hand under the cursor, and its index.
    -- Whether a card is on its way anywhere at the table, or has only just
    -- landed: the cards move under the cursor, so none is picked or played.
    local SETTLE = 0.15
    local function moving(s: Seen): boolean
        for _, f in s.fly do
            if clock - f.t0 < FLY_TIME + SETTLE then
                return true
            end
        end
        for _, g in s.gather do
            if clock - g.t0 < GATHER_TIME + SETTLE then
                return true
            end
        end
        for _, g in s.incoming do
            if clock - g.t0 < FLY_TIME + SETTLE then
                return true
            end
        end
        return false
    end

    local function under_cursor(s: Seen): number?
        if moving(s) then
            return nil
        end
        local ox, oy, oz, dx, dy, dz = core.cursor_ray()
        if ox and dx and eye_view(s) then
            return fan_under(s, { ox, oy :: number, oz :: number }, { dx, dy :: number, dz :: number })
        end
        if ox == nil or dy == nil or math.abs(dy) < 1e-4 then
            return nil
        end
        local t = (s.y + TOP - oy) / dy
        if t <= 0 then
            return nil
        end
        local wx, wz = ox + dx * t - s.x - 0.5, oz + dz * t - s.z - 0.5
        local a = yaw_of(s, view_seat(s))
        local u = wx * math.cos(a) - wz * math.sin(a)
        local v = wx * math.sin(a) + wz * math.cos(a)
        -- The card whose middle is nearest along the row: each its own equal
        -- share of it, the ones the next covers too.
        local k = #s.hand
        if k == 0 then
            return nil
        end
        local step = math.min(CW + 0.01, my_room(s) / math.max(k - 1, 1))
        local i = math.clamp(math.floor(u / step + (k + 1) / 2 + 0.5), 1, k)
        if math.abs(u - hand_u(k, i, my_room(s))) < CW / 2 and v > HAND_V - CH / 2 - 0.04 and v < HAND_V + CH / 2 then
            return i
        end
        -- and the one come forward, where it stands out over the felt
        local hv = hovered
        if hv and hv <= k and math.abs(u - hand_u(k, hv, my_room(s))) < CW / 2
            and v > HAND_V - CH / 2 - HOVER_TABLE - 0.04 and v < HAND_V + CH / 2 then
            return hv
        end
        return nil
    end

    local function my_turn(s: Seen): boolean
        return s.phase == "play" and seat >= 0 and s.turn == seat
    end

    core.event.on("core:input", function(ev)
        if sitting == nil then
            return
        end
        local s = seen[sitting]
        if s == nil then
            return
        end
        local a = ev.action
        if a == "key_v" and seat >= 0 then
            above = not above
            place_camera(s)
        elseif a == "mouse_right" then
            ev.cancel = true
            ask(`menu {sitting}`)
        elseif a == "mouse_left" then
            ev.cancel = true
            if seat < 0 then
                return
            end
            if s.phase == "hand" or s.phase == "match" then
                ask(`new {sitting}`)
                return
            end
            local i = under_cursor(s)
            if i and my_turn(s) and playable(s.game, s.hand, s.trick)[s.hand[i]] then
                local c = s.hand[i]
                ask(`play {sitting} {c}`)
                if s.game ~= "scopa" then
                    -- put down at once; the server's answer follows
                    table.remove(s.hand, i)
                    table.insert(s.trick, { seat, c })
                    s.turn = -1
                    if last_at[c] then
                        s.fly[c] = { from = last_at[c], t0 = clock }
                    end
                end
            end
        end
    end)

    local MODELS: { [string]: number } = {}
    local function model(name: string): number
        local id = MODELS[name]
        if id == nil or id == 0 then
            id = core.model_id(name)
            MODELS[name] = id
        end
        return id
    end

    -- A card of the table's deck lying in seat q's frame; c 0 is a back, a
    -- negative card the same card darkened (it may not be played now).
    -- A card on its way there (s.fly) is drawn along an arc from where it left,
    -- turning to lie as it will.
    -- A card on its way (f) from f.from to the pose `to` (x, y, z, yaw,
    -- pitch, roll), along an arc, every angle turning the short way: false
    -- once it is there. Not yet drawn (t0 to come), it is drawn nowhere; drawn
    -- from the stock, its back shows for the first half.
    local function flying(s: Seen, name: string, f: Flight, to: { number }): boolean
        local t = (clock - f.t0) / FLY_TIME
        if t >= 1 then
            return false
        end
        if t < 0 then
            return true
        end
        local e, o = t * t * (3 - 2 * t), f.from
        local function ang(i: number): number
            return o[i] + ((to[i] - o[i] + math.pi) % (2 * math.pi) - math.pi) * e
        end
        local shown = if f.flip and e < 0.5 then `cards:{s.deck}back` else name
        core.render.model(model(shown), o[1] + (to[1] - o[1]) * e, o[2] + (to[2] - o[2]) * e + math.sin(math.pi * t) * 0.08,
                          o[3] + (to[3] - o[3]) * e, ang(4), 1, ang(5), ang(6))
        return true
    end

    -- A card played (`played`) is remembered where it lies, for the cursor and
    -- for its flight to a pile, and comes up to be read while the cursor is on
    -- it (raised): higher, bigger, turned to the reader and tipped towards them.
    local function card(s: Seen, c: number, q: number, u: number, v: number, layer: number, yaw: number?,
                        played: boolean?)
        local x, y, z = world_of(s, q, u, v)
        y += 0.0004 + layer * 0.003
        local name = if c > 0 then `cards:{s.deck}c{c}` elseif c < 0 then `cards:{s.deck}d{-c}` else `cards:{s.deck}back`
        local a = yaw or yaw_of(s, q)
        local f = if c ~= 0 then s.fly[math.abs(c)] else nil
        if f and flying(s, name, f, { x, y, z, a, 0, 0 }) then
            return
        end
        if played and c > 0 then
            s.at[c] = { x, y, z, a }
            local r = raised[c]
            if r and seen[sitting or ""] == s then
                local e = r * r * (3 - 2 * r)
                local to = yaw_of(s, view_seat(s))
                local turn = (to - a + math.pi) % (2 * math.pi) - math.pi
                local tip = math.rad(90 - (if eye_view(s) then EYE_PITCH else 64)) * 0.9
                core.render.model(model(name), x, y + e * 0.08, z, a + turn * e, 1 + 1.1 * e, tip * e, 0)
                return
            end
        end
        core.render.model(model(name), x, y, z, a)
    end

    -- The card played under the cursor: the one up already first, at its size.
    local function table_under(s: Seen): number?
        if moving(s) then
            return nil
        end
        local ox, oy, oz, dx, dy, dz = core.cursor_ray()
        if ox == nil or dy == nil or math.abs(dy) < 1e-4 then
            return nil
        end
        local t = (s.y + TOP - oy) / dy
        if t <= 0 then
            return nil
        end
        local wx, wz = ox + dx * t, (oz :: number) + (dz :: number) * t
        local function on(c: number, grow: number): boolean
            local p = s.at[c]
            if p == nil then
                return false
            end
            local ex, ez = wx - p[1], wz - p[3]
            local ca, sa = math.cos(p[4]), math.sin(p[4])
            return math.abs(ex * ca - ez * sa) < CW / 2 * grow and math.abs(ex * sa + ez * ca) < CH / 2 * grow
        end
        local ot = over_table
        if ot and on(ot, 2.1) then
            return ot
        end
        local best, layer = nil, -1
        for i, tr in s.trick do
            if on(tr[2], 1) and i > layer then
                best, layer = tr[2], i
            end
        end
        if best then
            return best
        end
        if #s.show >= 2 and on(s.show[2], 1) then
            return s.show[2]
        end
        for _, c in s.tbl do
            if on(c, 1) then
                return c
            end
        end
        if s.trump > 0 and s.game ~= "scopa" and s.stock > 0 and on(s.trump, 1) then
            return s.trump
        end
        return nil
    end

    -- A hand held up and fanned about its grip g: the faces towards n, the
    -- heads up along w, r = w x n to the right (unit, square to each other).
    -- Card i of k: its head, its right and its middle, `lift` along its head.
    local function fanned(k: number, i: number, g: { number }, r: { number }, w: { number }, n: { number },
                          lift: number): ({ number }, { number }, { number })
        local th = ((k + 1) / 2 - i) * math.min(0.19, 1.1 / math.max(k, 1))
        local c, sn = math.cos(th), math.sin(th)
        local h, cr, m = {}, {}, {}
        local d = CH * 0.5 - 0.025 + lift
        for j = 1, 3 do
            h[j] = w[j] * c - r[j] * sn
            cr[j] = r[j] * c + w[j] * sn
            m[j] = g[j] + h[j] * d + n[j] * 0.002 * i   -- each in front of the one before
        end
        return h, cr, m
    end

    local function cross(a: { number }, b: { number }): { number }
        return { a[2] * b[3] - a[3] * b[2], a[3] * b[1] - a[1] * b[3], a[1] * b[2] - a[2] * b[1] }
    end

    -- A card standing in the air: c as for card(); the model's x along cr,
    -- its top (the face) towards n, its head along h.
    local function held(s: Seen, c: number, h: { number }, cr: { number }, n: { number }, m: { number })
        local name = if c > 0 then `cards:{s.deck}c{c}` elseif c < 0 then `cards:{s.deck}d{-c}` else `cards:{s.deck}back`
        local to = { m[1], m[2], m[3], math.atan2(-h[1], -h[3]), math.asin(math.clamp(h[2], -1, 1)), math.atan2(cr[2], n[2]) }
        local f = if c ~= 0 then s.fly[math.abs(c)] else nil
        if f and flying(s, name, f, to) then
            return
        end
        core.render.model(model(name), to[1], to[2], to[3], to[4], 1, to[5], to[6])
    end

    -- Where this player's own hand is in the eye view: the grip, and r, w, n.
    local function own_fan(s: Seen): ({ number }, { number }, { number }, { number })
        local eye, right, up, f = eye_frame(s)
        local g = {}
        for j = 1, 3 do
            g[j] = eye[j] + right[j] * GRIP_EYE[1] + up[j] * GRIP_EYE[2] - f[j] * GRIP_EYE[3]
        end
        return g, right, up, { -f[1], -f[2], -f[3] }
    end

    -- Somebody else's hand at seat q, held up before their body: only the
    -- backs, all this machine is told of it.
    local function their_fan(s: Seen, q: number, k: number)
        local a = yaw_of(s, q)
        local fx, fz = -math.sin(a), -math.cos(a)
        local x, z = s.x + 0.5 - fx * SEAT_R, s.z + 0.5 - fz * SEAT_R
        local g = { x + fx * GRIP_FWD, s.y + GRIP_UP, z + fz * GRIP_FWD }
        local back = 0.42   -- leant back towards their face, radians
        local n = { -fx * math.cos(back), math.sin(back), -fz * math.cos(back) }
        local w = { fx * math.sin(back), math.cos(back), fz * math.sin(back) }
        local r = cross(w, n)
        for i = 1, k do
            local h, cr, m = fanned(k, i, g, r, w, n, 0)
            held(s, 0, h, cr, n, m)
        end
    end

    -- How far card i of this player's fan rises: on their turn the ones they
    -- may play a little, the one under the cursor (hov) well out, at any time.
    local function fan_lift(s: Seen, i: number, ok: { [number]: boolean }, hov: number?): number
        local able = not my_turn(s) or ok[s.hand[i]]
        return (if my_turn(s) and able then 0.012 else 0) + (if i == hov then HOVER_FAN else 0)
    end

    -- Card i of the fan as drawn; the one under the cursor in front of all.
    local function fan_card(s: Seen, i: number, g: { number }, r: { number }, w: { number }, n: { number },
                            lift: number, front: boolean): ({ number }, { number }, { number })
        local h, cr, m = fanned(#s.hand, i, g, r, w, n, lift)
        if front then
            for j = 1, 3 do
                m[j] += n[j] * 0.002 * (#s.hand + 1 - i)
            end
        end
        return h, cr, m
    end

    fan_under = function(s: Seen, o: { number }, d: { number }): number?
        local g, r, w, n = own_fan(s)
        local k = #s.hand
        local dn = d[1] * n[1] + d[2] * n[2] + d[3] * n[3]
        if k == 0 or math.abs(dn) < 1e-4 then
            return nil
        end
        -- whether the ray goes through the card with head h, right cr, middle m
        local function on(h: { number }, cr: { number }, m: { number }): boolean
            local t = ((m[1] - o[1]) * n[1] + (m[2] - o[2]) * n[2] + (m[3] - o[3]) * n[3]) / dn
            local du, dv = 0, 0
            for j = 1, 3 do
                local e = o[j] + d[j] * t - m[j]
                du += e * cr[j]
                dv += e * h[j]
            end
            return math.abs(du) < CW / 2 and math.abs(dv) < CH / 2 + 0.02
        end
        -- The card whose head points nearest where the cursor is round the
        -- grip: each its own equal slice of the fan, the ones the next covers
        -- too (fanned's angles).
        local t = ((g[1] - o[1]) * n[1] + (g[2] - o[2]) * n[2] + (g[3] - o[3]) * n[3]) / dn
        local du, dw = 0, 0
        for j = 1, 3 do
            local e = o[j] + d[j] * t - g[j]
            du += e * r[j]
            dw += e * w[j]
        end
        local step = math.min(0.19, 1.1 / math.max(k, 1))
        local i = math.clamp(math.floor((k + 1) / 2 - math.atan2(-du, dw) / step + 0.5), 1, k)
        if on(fanned(k, i, g, r, w, n, 0)) then
            return i
        end
        -- the one come out, where it stands out over the others
        local hv = hovered
        if hv and hv <= k and on(fan_card(s, hv, g, r, w, n, fan_lift(s, hv, playable(s.game, s.hand, s.trick), hv), true)) then
            return hv
        end
        -- and past the ends of the fan, the edge of the card on top
        for j = k, 1, -1 do
            if on(fanned(k, j, g, r, w, n, 0)) then
                return j
            end
        end
        return nil
    end

    -- A flat mark lying square to seat q's frame.
    local function mark(s: Seen, q: number, u: number, v: number, w: number, h: number, layer: number,
                        r: number, g: number, b: number, alpha: number)
        local x, y, z = world_of(s, q, u, v)
        local a = yaw_of(s, q) % math.pi
        local across = a > math.pi / 4 and a < 3 * math.pi / 4   -- the frame turned about a quarter
        core.render.box(x, y + 0.0004 + layer * 0.003, z, if across then h else w, 0.0006, if across then w else h,
                        r, g, b, alpha)
    end

    -- A pile: a few backs, higher the more it holds.
    local function pile(s: Seen, q: number, u: number, v: number, n: number, turned: number?)
        for k = 0, math.ceil(n / 5) - 1 do
            card(s, 0, q, u, v, k * 3, yaw_of(s, q) + (turned or 0))
        end
    end

    -- Where a seat's taken cards lie in its frame: the corner to its right at
    -- four or two, beside its hand otherwise.
    local function pile_at(s: Seen): (number, number)
        if s.n == 2 or s.n == 4 then
            return 0.37, 0.37
        end
        return if s.n == 5 then 0.3 else 0.33, 0.39
    end

    core.render.pass("cards:table", 100, function()
        local me = core.players()[1]
        if me == nil then
            return
        end
        local mx, my, mz = core.player_pos(me)
        if mx == nil then
            return
        end
        for key, s in seen do
            if s.game == "" or math.abs(s.x - mx) > 32 or math.abs(s.z - mz) > 32 or math.abs(s.y - my) > 32 then
                continue
            end
            local here = key == sitting and seat >= 0
            local vs = if key == sitting then view_seat(s) else 0
            -- the hands: this player's face up, the others' backs
            local hover = if here then under_cursor(s) else nil
            if here then
                hovered = hover
            end
            if key == sitting then
                over_table = if hover then nil else table_under(s)
            end
            local ok = playable(s.game, s.hand, s.trick)
            local pu, pv = pile_at(s)
            -- the cards drawn by the others, on their way; not in their hands yet
            local coming = {}
            for _, g in s.incoming do
                if clock - g.t0 < FLY_TIME then
                    coming[g.q] = (coming[g.q] or 0) + 1
                    flying(s, `cards:{s.deck}back`, { from = g.from, t0 = g.t0 }, source(key, s, g.q, 0))
                end
            end
            -- the cards taken, flying to the pile and turning over on the way
            for _, g in s.gather do
                local t = math.clamp((clock - g.t0) / GATHER_TIME, 0, 1)
                if t >= 1 then
                    continue
                end
                local e, o = t * t * (3 - 2 * t), g.from
                local x, y, z = world_of(s, g.q, pu, pv)
                y += 0.0004 + math.ceil((s.piles[g.q + 1] or 0) / 5) * 0.009
                local a = yaw_of(s, g.q) + math.pi / 2
                local turn = (a - o[4] + math.pi) % (2 * math.pi) - math.pi
                local roll = math.pi * e
                local face = roll < math.pi / 2
                core.render.model(model(if face then `cards:{s.deck}c{g.c}` else `cards:{s.deck}back`),
                                  o[1] + (x - o[1]) * e, o[2] + (y - o[2]) * e + math.sin(math.pi * t) * 0.1,
                                  o[3] + (z - o[3]) * e, o[4] + turn * e, 1, 0, if face then roll else roll - math.pi)
            end
            for q = 0, s.n - 1 do
                local k = math.max((s.counts[q + 1] or 0) - (coming[q] or 0), 0)
                if here and q == seat and eye_view(s) then
                    -- held up before the eye; on this player's turn the cards they may
                    -- play rise, the one under the cursor more, the others darkened
                    local g, r, w, n = own_fan(s)
                    for i, c in s.hand do
                        local able = not my_turn(s) or ok[c]
                        local h, cr, m = fan_card(s, i, g, r, w, n, fan_lift(s, i, ok, hover), i == hover)
                        held(s, if able then c else -c, h, cr, n, m)
                        last_at[c] = { m[1], m[2], m[3], math.atan2(-h[1], -h[3]), math.asin(math.clamp(h[2], -1, 1)),
                                       math.atan2(cr[2], n[2]) }
                    end
                elseif (s.names[q + 1] or "") ~= "" and not (here and q == seat) then
                    their_fan(s, q, k)   -- somebody stands there holding them
                elseif here and q == seat then
                    -- on this player's turn the cards they may play come forward, the others darkened
                    local bound = my_turn(s) and s.game == "tresette" and #s.trick > 0
                    for i, c in s.hand do
                        local u = hand_u(#s.hand, i, my_room(s))
                        local able = not my_turn(s) or ok[c]
                        local v = HAND_V - (if my_turn(s) and able and bound then 0.03 else 0)
                            - (if i == hover then HOVER_TABLE else 0)
                        card(s, if able then c else -c, q, u, v, if i == hover then #s.hand + 1 else i)
                        local x, y, z = world_of(s, q, u, v)
                        last_at[c] = { x, y + 0.01, z, yaw_of(s, q), 0, 0 }
                    end
                else
                    local r = if s.n >= 5 then 0.41 else 0.40
                    for i = 1, k do
                        card(s, 0, q, hand_u(k, i, if s.n >= 5 then 0.26 else 0.4), r, i)
                    end
                end
                local p = s.piles[q + 1] or 0
                if p > 0 then
                    pile(s, q, pu, pv, p, math.pi / 2)
                end
            end
            -- the trick, each card before whoever played it, read from here
            local tr = if s.n >= 5 then 0.19 else 0.17
            for i, t in s.trick do
                card(s, t[2], t[1], 0, tr, i, yaw_of(s, vs), true)
            end
            -- the stock in the middle, briscola's trump card across under it
            if s.game ~= "scopa" and s.stock > 0 then
                if s.trump > 0 then
                    card(s, s.trump, vs, 0.035, 0.0, 0, yaw_of(s, vs) + math.pi / 2, true)
                end
                if s.stock > 1 or s.trump == 0 then
                    pile(s, vs, -0.03, 0.0, s.stock - (if s.trump > 0 then 1 else 0))
                end
            end
            -- scopa's table, in two rows, and the card being played on it
            if s.game == "scopa" then
                local taking = {}
                for i = 3, #s.show do
                    taking[s.show[i]] = true
                end
                local per = math.max(4, math.ceil(#s.tbl / 2))
                for i, c in s.tbl do
                    local row = (i - 1) // per
                    local col = (i - 1) % per
                    local inrow = math.min(per, #s.tbl - row * per)
                    local u = (col - (inrow - 1) / 2) * (CW + 0.012)
                    local v = if #s.tbl <= per then 0 else (row - 0.5) * (CH + 0.015)
                    card(s, c, vs, u, v, 1, nil, true)
                    if taking[c] then
                        mark(s, vs, u, v, CW + 0.014, CH + 0.014, 0, 240, 210, 60, 255)
                    end
                end
                if #s.show >= 2 then
                    card(s, s.show[2], s.show[1], 0, 0.2, 6, yaw_of(s, vs), true)
                end
                if s.stock > 0 then
                    pile(s, vs, -0.25, -0.25, s.stock, math.pi / 2)
                end
            end
        end
    end)

    -- ----------------------------------------------------------- the screen

    local function team_of(s: Seen, q: number): number
        return if s.n == 4 then q % 2 else q
    end

    local function who(s: Seen, q: number): string
        if q == seat then
            return "Tu"
        end
        local n = s.names[q + 1] or ""
        return if n == "" then `Tavolo {q + 1}` else n
    end

    -- A team's name as this player sees it: "noi"/"loro" at four, a player's own otherwise.
    local function side_name(s: Seen, t: number): string
        if s.n ~= 4 then
            return who(s, t)
        end
        if seat >= 0 then
            return if t == team_of(s, seat) then "Noi" else "Loro"
        end
        return if t == 0 then "Squadra 1" else "Squadra 2"
    end

    local function teams_of(s: Seen): number
        return if s.n == 4 then 2 else s.n
    end

    -- The scores in a line: "A 3   B 1   ...".
    local function tally(s: Seen, list: { number }): string
        local out = {}
        for t = 0, teams_of(s) - 1 do
            table.insert(out, `{side_name(s, t)} {list[t + 1] or 0}`)
        end
        return table.concat(out, "   ")
    end

    -- Cinquiglio's auction and call, from x_: level, bidder, out mask, called card, caller, partner (-1 hidden).
    local function call_line(s: Seen): string?
        local x = s.x_
        if s.game ~= "cinquiglio" or #x < 6 then
            return nil
        end
        if s.phase == "bid" or s.phase == "suit" then
            return if x[1] == 0 then "Asta: nessuno ha ancora chiamato" else `Asta: {who(s, x[2])} chiama {bid_name(x[1])}`
        end
        if x[4] <= 0 then
            return nil
        end
        local line = `{who(s, x[5])} chiama il {name_of(x[4])} (fa {bid_target(x[1])})`
        if x[6] >= 0 then
            line ..= if x[6] == x[5] then " -- da solo" elseif x[6] == seat then " -- il compagno sei tu"
                else ` -- compagno: {who(s, x[6])}`
        end
        return line
    end

    -- What the last hand made, line by line.
    local function summary(s: Seen): { string }
        local m = s.sum
        local out = {}
        if s.game == "briscola" and #m >= 2 then
            table.insert(out, `Punti: {tally(s, m)}`)
            local best, at, tie = -1, -1, false
            for i, p in m do
                if p > best then
                    best, at, tie = p, i - 1, false
                elseif p == best then
                    tie = true
                end
            end
            table.insert(out, if tie then "Pari" else `Vince {side_name(s, at)}`)
        elseif s.game == "cinquiglio" and #m >= 6 then
            local caller, partner = who(s, m[5]), who(s, m[6])
            table.insert(out, if m[5] == m[6] then `{caller} da solo: {m[1]} punti, ne servivano {m[4]}`
                else `{caller} e {partner}: {m[1]} punti, ne servivano {m[4]}`)
            local callers = if m[5] == m[6] then caller else `{caller} e {partner}`
            table.insert(out, if m[3] == 1 then `Fatto: incassa{if m[5] == m[6] then "" else "no"} {callers}, pagano gli altri`
                else `Non fatto (gli altri {m[2]}): paga{if m[5] == m[6] then "" else "no"} {callers}`)
        elseif s.game == "tresette" and #m >= 4 then
            local a, b = side_name(s, 0), side_name(s, 1)
            table.insert(out, `Mano: {a} {m[1]}, {b} {m[3]}`)
            if m[2] + m[4] > 0 then
                table.insert(out, `Accusi: {a} {m[2]}, {b} {m[4]}`)
            end
        elseif s.game == "scopa" and #m >= 11 then
            local a, b = side_name(s, 0), side_name(s, 1)
            local function by(x: number, y: number): string
                return if x > y then a elseif y > x then b else "nessuno"
            end
            table.insert(out, `Carte {m[1]}-{m[2]}: {by(m[1], m[2])}   Denari {m[3]}-{m[4]}: {by(m[3], m[4])}`)
            table.insert(out, `Settebello: {if m[5] == 0 then a elseif m[5] == 1 then b else "nessuno"}   `
                .. `Primiera {m[6]}-{m[7]}: {by(m[6], m[7])}   Scope {m[8]}-{m[9]}`)
            table.insert(out, `Mano: {a} {m[10]}, {b} {m[11]}`)
        end
        return out
    end

    core.hud.pass("cards:status", 100, function()
        local s = if sitting then seen[sitting] else nil
        if s == nil then
            return
        end
        local vs = view_seat(s)
        local hint
        if s.game == "" then
            core.hud.rect(0.5, 0.875, 1.0, 0.065, 0, 0, 0, 150)
            core.hud.text(0.5, 0.875, if seat >= 0 then "Tasto destro: scegli il gioco" else "Il tavolo è libero",
                          0.038)
            core.hud.text(0.5, 0.94, "Briscola, cinquiglio, scopa o tresette   --   tasto destro: menu, alzati", 0.028, 1,
                          230, 230, 230, 255)
            return
        end
        -- who sits where, round the table as this player sees it
        local a = math.max(core.hud.aspect(), 0.1)
        for q = 0, s.n - 1 do
            local turn = (q - vs) * 2 * math.pi / s.n
            local x, y = 0.5 + math.sin(turn) * 0.62 / a, 0.5 + math.cos(turn) * 0.29
            if eye_view(s) and q == vs then
                x, y = 0.16, 0.9   -- beside the cards in the hand, not over them
            end
            local on = (s.phase == "play" or s.phase == "bid" or s.phase == "suit") and s.turn == q
            local out = s.game == "cinquiglio" and s.phase == "bid" and bit32.btest(s.x_[3] or 0, bit32.lshift(1, q))
            local label = who(s, q) .. (if q == s.dealer then "  (mazziere)" else "") .. (if out then "  passa" else "")
            core.hud.rect(x, y, 0.36, 0.045, 0, 0, 0, if on then 190 else 110)
            core.hud.text(x, y, label, 0.03, 1, 255, if on then 220 else 255, if on then 90 else 255, 255)
        end
        -- the score, top right (the engine's lines are top left)
        local title = `{GAME_NAME[s.game]} a {COUNT_NAME[s.n] or s.n}`
        local line2 = tally(s, s.score)
        if s.game == "briscola" then
            line2 = `Partite vinte: {line2}`
        elseif s.game == "scopa" then
            line2 = `{line2}   (a 11)   scope {s.scope[1] or 0}-{s.scope[2] or 0}`
        elseif s.game == "tresette" then
            line2 = `{line2}   (a 21)`
        end
        local line3 = if (s.game == "briscola" or s.game == "cinquiglio") and s.trump > 0
            then `Briscola: {SUIT_NAME[suit(s.trump)]}` else nil
        local wide = if s.n >= 3 and s.n ~= 4 then 1.0 else 0.74   -- a name a player
        local sx = 1 - (wide / 2 + 0.03) / a
        core.hud.rect(sx, 0.1, wide, if line3 then 0.15 else 0.1, 0, 0, 0, 140)
        core.hud.text(sx, 0.075, title, 0.032)
        core.hud.text(sx, 0.115, line2, if s.n == 5 then 0.022 else 0.026, 1, 230, 230, 230, 255)
        if line3 then
            core.hud.text(sx, 0.155, line3, 0.026, 1, 240, 210, 90, 255)
        end
        -- a second line over the status: the call, or the accusi while the first trick is played
        local over = call_line(s)
        if s.game == "tresette" and s.phase ~= "hand" and s.phase ~= "match" then
            local said = {}
            for q = 0, s.n - 1 do
                if (s.x_[q + 1] or 0) > 0 and (s.counts[q + 1] or 0) >= 9 then
                    table.insert(said, `{who(s, q)} {s.x_[q + 1]}`)
                end
            end
            if #said > 0 then
                over = `Accusi: {table.concat(said, ", ")}`
            end
        end
        if over and s.phase ~= "hand" and s.phase ~= "match" then
            local oy = if eye_view(s) then 0.355 else 0.815
            core.hud.rect(0.5, oy, 1.2, 0.045, 0, 0, 0, 150)
            core.hud.text(0.5, oy, over, 0.028, 1, 240, 210, 90, 255)
        end
        -- the line under the table
        local line
        if s.phase == "hand" or s.phase == "match" then
            local lines = summary(s)
            if s.phase == "match" then
                local w, best = 0, -math.huge
                for t = 0, teams_of(s) - 1 do
                    if (s.score[t + 1] or 0) > best then
                        w, best = t, s.score[t + 1] or 0
                    end
                end
                table.insert(lines, `Partita finita: vince {side_name(s, w)}`)
            end
            for i, l in lines do
                local y = 0.44 + (i - 1) * 0.055
                core.hud.rect(0.5, y, 1.2, 0.05, 0, 0, 0, 170)
                core.hud.text(0.5, y, l, 0.032)
            end
            line = if s.phase == "match" then "Click: un'altra partita" else "Click: la mano dopo"
            hint = "tasto destro: un altro gioco, alzati"
        elseif s.phase == "bid" or s.phase == "suit" then
            line = if s.turn == seat then (if s.phase == "bid" then "Tocca a te: chiama o passa" else "Scegli il seme")
                else `Asta: tocca a {who(s, s.turn)}`
            hint = if s.turn == seat then "tasto destro: la domanda di nuovo" else "tasto destro: menu"
        elseif s.phase == "show" and #s.show >= 2 then
            local took = {}
            for i = 3, #s.show do
                table.insert(took, name_of(s.show[i]))
            end
            local by, mine = who(s, s.show[1]), s.show[1] == seat
            line = if #took == 0 then (if mine then `Cali il {name_of(s.show[2])}` else `{by} cala il {name_of(s.show[2])}`)
                elseif mine then `Prendi con il {name_of(s.show[2])}: {table.concat(took, ", ")}`
                else `{by} prende con il {name_of(s.show[2])}: {table.concat(took, ", ")}`
            hint = "tasto destro: menu"
        elseif s.phase == "trick" then
            line = if #s.trick > 0 then `Prende: {who(s, taker(s.game, s.trick, if s.trump > 0 then suit(s.trump) else -1))}`
                else "..."
            hint = "tasto destro: menu"
        elseif my_turn(s) then
            line = if s.game == "tresette" and #s.trick > 0 then "Tocca a te: rispondi al seme se puoi"
                else "Tocca a te: clicca una carta"
            hint = "tasto destro: un altro gioco, alzati"
        else
            line = if s.turn >= 0 then `Tocca a: {who(s, s.turn)}` else "..."
            hint = if seat < 0 then "Guardi -- tasto destro: alzati" else "tasto destro: un altro gioco, alzati"
        end
        -- the card under the cursor, by name, in case its picture is not enough
        local hv = if seat >= 0 then under_cursor(s) else nil
        if hv and s.hand[hv] then
            -- from above, beside the seat's own name under the hand
            local nx, ny = if eye_view(s) then 0.5 else 0.77, if eye_view(s) then 0.885 else 0.805
            core.hud.rect(nx, ny, 0.36, 0.05, 0, 0, 0, 170)
            core.hud.text(nx, ny, name_of(s.hand[hv]), 0.034, 1, 255, 235, 150, 255)
        end
        -- and the card played under it, with who played it
        local ot = over_table
        if ot and not hv then
            local by = nil
            for _, t in s.trick do
                if t[2] == ot then
                    by = `giocato da {who(s, t[1])}`
                end
            end
            if #s.show >= 2 and s.show[2] == ot then
                by = `calato da {who(s, s.show[1])}`
            elseif ot == s.trump and s.game ~= "scopa" then
                by = "la briscola"
            elseif not by then
                by = "sul tavolo"
            end
            local nx, ny = if eye_view(s) then 0.5 else 0.77, if eye_view(s) then 0.885 else 0.805
            core.hud.rect(nx, ny, 0.6, 0.05, 0, 0, 0, 170)
            core.hud.text(nx, ny, `{name_of(ot)}  --  {by}`, 0.032, 1, 255, 235, 150, 255)
        end
        -- in the eye view the hand is where the line was: it goes over the table
        local ly = if eye_view(s) then 0.3 else 0.875
        core.hud.rect(0.5, ly, 1.2, 0.065, 0, 0, 0, 150)
        core.hud.text(0.5, ly, line, 0.036)
        if eye_view(s) then
            core.hud.text(0.74, 0.95, `{hint}   --   V: dall'alto`, 0.022, 1, 230, 230, 230, 255)
        else
            core.hud.text(0.5, 0.94, if seat >= 0 then `{hint}   --   V: in mano` else hint, 0.028, 1, 230, 230, 230, 255)
        end
    end)
end
