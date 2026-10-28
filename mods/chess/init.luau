-- chess: a chess table, one cell, and a game on it by the classical rules.
--
-- Right click on the table and the camera flies from the eye to above the
-- board, on your side of it (core.fixed_view); the cursor is free and the body
-- stands. You sit on the side of the board you stand on -- white's is the
-- side of the table that faced whoever put it down -- or on the other if
-- yours is taken; anybody else watches; somebody alone plays both. Press on a piece of the side to move and drag it to its
-- square, or click it and click the square. White moves first,
-- and castling, en passant, promotion (a panel asks to what), check, mate,
-- stalemate, fifty moves, threefold repetition and too little to mate are the
-- rules, all drawn without being claimed. Right click while sitting opens a
-- menu: offer a draw (it stands until the other side answers or moves; one
-- offer per move, and the question goes up on their screen), accept or
-- decline one, resign (asked twice), get up, and before the first move or
-- after the last the game (VARIANTS: classical, 960 and seven more) and a
-- clock -- none, 3+2, 5 or 10 minutes -- which starts after white's first move, stops while the side to move has got up from a game of
-- two, and loses on time unless the other side could never mate. ESC is the
-- game's pause menu, as anywhere. When a game is over a click on the board
-- sets up the next with the same game and clock.
--
-- The SERVER keeps every board (its storage, with the positions counted for
-- repetition, so a game outlives a restart) and decides every move; a player's
-- machine draws the pieces (core.render.model, tools/make_chess_models.py) and
-- sends what was dragged where. Both halves run the same rules: the client's
-- only to show the moves a piece has, and to put it down without waiting for
-- the answer.

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local SQ  = 0.1     -- a square, in nodes (tools/make_chess_models.py)
local TOP = 0.775   -- the squares' top above the cell's floor

-- ===================================================================== rules
-- Besides chess the table plays two other games on the same squares (VARIANTS,
-- the menu's "Variante"):
--   dama    Italian draughts on the dark squares: men step forward one
--           diagonal and take forward only, never a king; kings step one
--           diagonal any way; a taking is a must, the line that takes most,
--           then the king's, then the one taking most kings; a man that
--           gets to the far rank is crowned and stops. Who cannot move loses;
--           forty moves without a taking or a man moving are a draw.
--   mulino  the mill (nine men's morris), on 24 points over the board: nine
--           stones each put down one a turn from a tray beside the table, then
--           moved along the lines to a point next to them -- anywhere, with
--           three left; three in a line is a mill, and its maker takes one of
--           the other's stones (not out of a mill while there is one that is
--           not). Down to two, or with no move, loses.
-- Their pieces are letters chess does not use: M and D (man, king), O (stone).
-- A board is 64 letters, a1 first, rank by rank: PNBRQK white, pnbrqk black,
-- "." empty. A square is its index, 1..64.

local START = "RNBQKBNRPPPPPPPP" .. string.rep(".", 32) .. "pppppppprnbqkbnr"

-- The games a table plays, in the order the menu goes round them:
--   std    the classical rules
--   960    Fischer's: the back rank shuffled, bishops on both colours and the
--          king between the rooks; castling takes the king to g or c and the
--          rook to f or d whatever their files, and is played king onto rook
--   koth   king of the hill: a king on d4, e4, d5 or e5 wins
--   3check the third check given wins
--   race   racing kings: no pawns, nobody may give check, and a king on the
--          eighth rank wins; if white gets there first black has a move to
--          get there too, and then it is drawn
--   anti   losing chess: taking is a must, the king is a piece like the
--          others (no check, no castling, a pawn may become one) and who has
--          no move left, nothing left or stalemated, wins
--   atomic a taking blows up the taker, the taken and every piece but pawns
--          on the squares round them; kings do not take, kings side by side
--          are not in check, and blowing up the other king wins
--   zh     crazyhouse: what you take is yours, to put down (a move) on any
--          empty square, pawns not on the first or last rank; a promoted
--          piece taken is a pawn again
--   horde  white has 36 pawns and no king (those on the first rank may go
--          two too), black the usual set: black wins taking them all
local VARIANTS = { "std", "960", "koth", "3check", "race", "anti", "atomic", "zh", "horde", "dama", "mulino" }
local VARIANT_NAME = { std = "Classica", ["960"] = "Scacchi 960", koth = "Re della collina",
                       ["3check"] = "Tre scacchi", race = "Corsa dei re", anti = "Vinciperdi",
                       atomic = "Atomica", zh = "Crazyhouse", horde = "Orda", dama = "Dama italiana",
                       mulino = "Mulino" }

-- Crazyhouse puts pieces down from the hand: a move from 100 + k for white
-- and 110 + k for black, k the place in DROPS of the piece put down.
local DROPS = "PNBRQ"

local function at(f: number, r: number): number
    return r * 8 + f + 1
end

local function fr(i: number): (number, number)
    return (i - 1) % 8, (i - 1) // 8
end

local function colour(p: string): string?
    if p == "." then
        return nil
    end
    return if p == string.upper(p) then "w" else "b"
end

local function other(c: string): string
    return if c == "w" then "b" else "w"
end

-- `kind` ("N") as `c` plays it.
local function as(kind: string, c: string): string
    return if c == "w" then kind else string.lower(kind)
end

local KNIGHT = { { 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 }, { -1, -2 }, { -2, -1 }, { -2, 1 }, { -1, 2 } }
local KING   = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 } }
local ROOK   = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } }
local BISHOP = { { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 } }

local function on(f: number, r: number): boolean
    return f >= 0 and f < 8 and r >= 0 and r < 8
end

-- Whether `by` attacks square i.
local function attacked(b: { string }, i: number, by: string): boolean
    local f, r = fr(i)
    local back = if by == "w" then -1 else 1   -- where its pawns attack from
    for _, df in { -1, 1 } do
        if on(f + df, r + back) and b[at(f + df, r + back)] == as("P", by) then
            return true
        end
    end
    for _, d in KNIGHT do
        if on(f + d[1], r + d[2]) and b[at(f + d[1], r + d[2])] == as("N", by) then
            return true
        end
    end
    for _, d in KING do
        if on(f + d[1], r + d[2]) and b[at(f + d[1], r + d[2])] == as("K", by) then
            return true
        end
    end
    for _, set in { { ROOK, "R" }, { BISHOP, "B" } } do
        for _, d in set[1] do
            local x, y = f + d[1], r + d[2]
            while on(x, y) do
                local p = b[at(x, y)]
                if p ~= "." then
                    if p == as(set[2], by) or p == as("Q", by) then
                        return true
                    end
                    break
                end
                x, y = x + d[1], y + d[2]
            end
        end
    end
    return false
end

local function king_of(b: { string }, c: string): number
    return table.find(b, as("K", c)) or 0
end

type State = {
    b: { string },
    turn: string,
    castle: string,   -- "KQkq" or less
    ep: number,       -- the square a pawn passed over last move, 0 if none
    half: number,     -- moves since a capture or a pawn moved
    from: number,     -- the last move, 0 before the first
    to: number,
    result: string,   -- "" playing, "w" or "b" won, "=" drawn
    why: string,      -- "mate", "stalemate", "fifty", "material", "repetition", "agreed",
                      -- "resign", "time"; "" playing
    tc: number,       -- the clock: seconds each, 0 for none, and seconds added a move
    inc: number,
    wt: number,       -- seconds left to white and to black
    bt: number,
    offer: string,    -- "w" or "b" has offered a draw, "" nobody
    var: string,      -- one of VARIANTS
    rk: string,       -- the files the rooks castle from, queen's side then king's: "07"
    checks: string,   -- checks given by white and by black, for 3check: "00"
    pocket: string,   -- zh: the pieces in hand, white's upper case, sorted
    pr: string,       -- zh: 64 letters, "+" on a square whose piece was a pawn, "" in other games
}

-- The piece on square i, or the one a hand slot holds ("." none left).
local function piece_at(st: State, i: number): string
    if i <= 64 then
        return st.b[i]
    end
    if st.var == "mulino" then
        local stone = if i < 110 then "O" else "o"
        return if string.find(st.pocket, stone, 1, true) then stone else "."
    end
    local p = string.sub(DROPS, i % 10, i % 10)
    p = if i < 110 then p else string.lower(p)
    return if string.find(st.pocket, p, 1, true) then p else "."
end

-- A back rank for 960: a bishop on each colour, the queen and the knights
-- anywhere left, and rook, king, rook in the three squares left over.
local function shuffled(): (string, string)
    local row = { ".", ".", ".", ".", ".", ".", ".", "." }
    row[2 * math.random(0, 3) + 1] = "B"   -- files a, c, e, g: dark
    row[2 * math.random(0, 3) + 2] = "B"   -- b, d, f, h: light
    local function put(piece: string)
        local free = {}
        for f = 1, 8 do
            if row[f] == "." then
                table.insert(free, f)
            end
        end
        row[free[math.random(1, #free)]] = piece
    end
    put("Q")
    put("N")
    put("N")
    local rooks, left = {}, 0
    for f = 1, 8 do
        if row[f] == "." then
            left += 1
            row[f] = if left == 2 then "K" else "R"
            if left ~= 2 then
                table.insert(rooks, f - 1)
            end
        end
    end
    return table.concat(row), `{rooks[1]}{rooks[2]}`
end

local function new_game(tc: number?, inc: number?, var: string?): State
    local t = tc or 0
    local v = var or "std"
    local start, rk, castle = START, "07", "KQkq"
    if v == "960" then
        local row
        row, rk = shuffled()
        start = row .. string.rep("P", 8) .. string.rep(".", 32) .. string.rep("p", 8) .. string.lower(row)
    elseif v == "race" then
        start, castle = "qrbnNBRQkrbnNBRK" .. string.rep(".", 48), ""
    elseif v == "anti" then
        castle = ""
    elseif v == "horde" then
        start, castle = string.rep("P", 32) .. ".PP..PP." .. string.rep(".", 8) .. "pppppppprnbqkbnr", "kq"
    elseif v == "dama" then
        -- Men on the dark squares (a1's colour) of each side's three ranks.
        local b = {}
        for i = 1, 64 do
            local f, r = (i - 1) % 8, (i - 1) // 8
            b[i] = if (f + r) % 2 ~= 0 then "." elseif r <= 2 then "M" elseif r >= 5 then "m" else "."
        end
        start, castle = table.concat(b), ""
    elseif v == "mulino" then
        start, castle = string.rep(".", 64), ""
    end
    return { b = string.split(start, ""), turn = "w", castle = castle, ep = 0, half = 0, from = 0, to = 0,
             result = "", why = "", tc = t, inc = inc or 0, wt = t, bt = t, offer = "", var = v, rk = rk,
             checks = "00", pocket = if v == "mulino" then "OOOOOOOOOooooooooo" else "",
             pr = if v == "zh" then string.rep(".", 64) else "" }
end

-- The rook files a game castles from.
local function rook_files(st: State): (number, number)
    return tonumber(string.sub(st.rk, 1, 1)) or 0, tonumber(string.sub(st.rk, 2, 2)) or 7
end

-- Where the piece on i can go, its own king left aside.
local function pseudo(st: State, i: number, out: { number })
    local b = st.b
    local p = b[i]
    local c = colour(p)
    if c == nil then
        return
    end
    local f, r = fr(i)
    local kind = string.upper(p)
    local function free_or_enemy(x: number, y: number): boolean
        if not on(x, y) then
            return false
        end
        local q = colour(b[at(x, y)])
        return q ~= c
    end
    if kind == "P" then
        local dir = if c == "w" then 1 else -1
        local home = if c == "w" then 1 else 6
        if on(f, r + dir) and b[at(f, r + dir)] == "." then
            table.insert(out, at(f, r + dir))
            if (r == home or (st.var == "horde" and c == "w" and r == 0)) and b[at(f, r + 2 * dir)] == "." then
                table.insert(out, at(f, r + 2 * dir))
            end
        end
        for _, df in { -1, 1 } do
            if on(f + df, r + dir) then
                local t = at(f + df, r + dir)
                if colour(b[t]) == other(c) or t == st.ep then
                    table.insert(out, t)
                end
            end
        end
    elseif kind == "N" or kind == "K" then
        local takes = not (kind == "K" and st.var == "atomic")
        for _, d in (if kind == "N" then KNIGHT else KING) do
            if free_or_enemy(f + d[1], r + d[2]) and (takes or b[at(f + d[1], r + d[2])] == ".") then
                table.insert(out, at(f + d[1], r + d[2]))
            end
        end
        -- Castling, the classical way a case of 960's: the king to g or c and
        -- the rook to f or d, everything between where they are and where
        -- they go empty but for the two of them, and none of the squares
        -- the king crosses attacked. The right is lost once the king moves,
        -- so while it lasts the king is on its first square.
        local row = if c == "w" then 0 else 7
        local them = other(c)
        if kind == "K" and r == row and st.castle ~= "" and not attacked(b, i, them) then
            local qr, kr = rook_files(st)
            for _, side in { { "K", kr, 6, 5 }, { "Q", qr, 2, 3 } } do
                local rf, kt, rt = side[2], side[3], side[4]
                if not string.find(st.castle, as(side[1], c), 1, true) or b[at(rf, row)] ~= as("R", c) then
                    continue
                end
                local ok = true
                for x = math.min(f, kt, rf, rt), math.max(f, kt, rf, rt) do
                    if x ~= f and x ~= rf and b[at(x, row)] ~= "." then
                        ok = false
                    end
                end
                for x = math.min(f, kt), math.max(f, kt) do
                    if ok and attacked(b, at(x, row), them) then
                        ok = false
                    end
                end
                if ok then
                    table.insert(out, if st.var == "960" then at(rf, row) else at(kt, row))
                end
            end
        end
    else
        local dirs = if kind == "R" then ROOK elseif kind == "B" then BISHOP else table.move(BISHOP, 1, 4, 5, table.clone(ROOK))
        for _, d in dirs do
            local x, y = f + d[1], r + d[2]
            while on(x, y) do
                local q = colour(b[at(x, y)])
                if q == c then
                    break
                end
                table.insert(out, at(x, y))
                if q ~= nil then
                    break
                end
                x, y = x + d[1], y + d[2]
            end
        end
    end
end

-- Whether the move puts a pawn on its last rank.
-- --------------------------------------------------------------- draughts

-- The takings a piece on i can make, as lines: where it ends, how many it
-- takes, how many kings, the first king's place in the line, and the squares
-- taken. A man takes forward only and never a king; crowned, it stops.
type Line = { to: number, n: number, kings: number, first: number, taken: { number } }
local function dama_lines(b: { string }, i: number): { Line }
    local p = b[i]
    local c = colour(p) :: string
    local king = string.upper(p) == "D"
    local ahead = if c == "w" then 1 else -1
    local out: { Line } = {}
    local function walk(at_i: number, taken: { number }, kings: number, first: number)
        local f, r = fr(at_i)
        local went = false
        for _, d in BISHOP do
            if not king and d[2] ~= ahead then
                continue
            end
            local mf, mr, lf, lr = f + d[1], r + d[2], f + 2 * d[1], r + 2 * d[2]
            if not (on(mf, mr) and on(lf, lr)) then
                continue
            end
            local mid, land = at(mf, mr), at(lf, lr)
            local q = b[mid]
            if q == "." or colour(q) == c or table.find(taken, mid) then
                continue
            end
            if not king and string.upper(q) == "D" then
                continue   -- a man never takes a king
            end
            if b[land] ~= "." and land ~= i then
                continue
            end
            local t2 = table.clone(taken)
            table.insert(t2, mid)
            local k2 = kings + (if string.upper(q) == "D" then 1 else 0)
            local f2 = if first == 0 and string.upper(q) == "D" then #t2 else first
            went = true
            if not king and lr == (if c == "w" then 7 else 0) then
                table.insert(out, { to = land, n = #t2, kings = k2, first = f2, taken = t2 })   -- crowned: stops
            else
                walk(land, t2, k2, f2)
            end
        end
        if not went and #taken > 0 then
            table.insert(out, { to = at_i, n = #taken, kings = kings, first = first, taken = taken })
        end
    end
    walk(i, {}, 0, 0)
    return out
end

-- Whether line a is to be preferred over b by the Italian rules.
local function dama_better(b: { string }, ai: number, a: Line, bi: number, bl: Line): boolean
    if a.n ~= bl.n then
        return a.n > bl.n
    end
    local ak, bk = string.upper(b[ai]) == "D", string.upper(b[bi]) == "D"
    if ak ~= bk then
        return ak
    end
    if a.kings ~= bl.kings then
        return a.kings > bl.kings
    end
    return a.first ~= 0 and (bl.first == 0 or a.first < bl.first)
end

-- The side's moves: the best takings if there is one, else the steps.
local function dama_legal(st: State, only: number?): { { number } }
    local b = st.b
    local best: { { any } } = {}
    for i = 1, 64 do
        if colour(b[i]) == st.turn then
            for _, l in dama_lines(b, i) do
                if #best == 0 or dama_better(b, i, l, best[1][1], best[1][2]) then
                    best = { { i, l } }
                elseif not dama_better(b, best[1][1], best[1][2], i, l) then
                    table.insert(best, { i, l })
                end
            end
        end
    end
    local moves = {}
    if #best > 0 then
        for _, e in best do
            if only == nil or only == e[1] then
                table.insert(moves, { e[1], e[2].to })
            end
        end
        return moves
    end
    for i = 1, 64 do
        if colour(b[i]) == st.turn and (only == nil or only == i) then
            local f, r = fr(i)
            local king = string.upper(b[i]) == "D"
            for _, d in BISHOP do
                if (king or d[2] == (if st.turn == "w" then 1 else -1)) and on(f + d[1], r + d[2])
                    and b[at(f + d[1], r + d[2])] == "." then
                    table.insert(moves, { i, at(f + d[1], r + d[2]) })
                end
            end
        end
    end
    return moves
end

local function dama_apply(st: State, from: number, to: number): State
    local b = table.clone(st.b)
    local p = b[from]
    local c = colour(p) :: string
    local n = table.clone(st)
    n.b, n.turn, n.from, n.to, n.offer, n.result, n.why = b, other(c), from, to, st.offer, "", ""
    n.half = st.half + 1
    local best: Line? = nil
    for _, l in dama_lines(st.b, from) do
        if l.to == to and (best == nil or dama_better(st.b, from, l, from, best :: Line)) then
            best = l
        end
    end
    if best then
        for _, sq in best.taken do
            b[sq] = "."
        end
        n.half = 0
    end
    b[from], b[to] = ".", p
    if string.upper(p) == "M" then
        n.half = 0
        local _, r = fr(to)
        if r == (if c == "w" then 7 else 0) then
            b[to] = as("D", c)
        end
    end
    return n
end

-- ------------------------------------------------------------------- mill

-- The 24 points, (file, rank) on a 7 by 7 grid, and the 16 lines of three.
local MILL_POINTS = { { 0, 0 }, { 3, 0 }, { 6, 0 }, { 1, 1 }, { 3, 1 }, { 5, 1 }, { 2, 2 }, { 3, 2 }, { 4, 2 },
                      { 0, 3 }, { 1, 3 }, { 2, 3 }, { 4, 3 }, { 5, 3 }, { 6, 3 }, { 2, 4 }, { 3, 4 }, { 4, 4 },
                      { 1, 5 }, { 3, 5 }, { 5, 5 }, { 0, 6 }, { 3, 6 }, { 6, 6 } }
local MILL_LINES = {}
local MILL_NEXT: { [number]: { number } } = {}
local IS_POINT: { [number]: boolean } = {}
do
    local idx: { [string]: number } = {}
    for _, q in MILL_POINTS do
        IS_POINT[at(q[1], q[2])] = true
        idx[`{q[1]},{q[2]}`] = at(q[1], q[2])
    end
    -- Rows and columns: the points on each, in order, three at a time.
    for axis = 0, 1 do
        for k = 0, 6 do
            local row = {}
            for j = 0, 6 do
                local key = if axis == 0 then `{j},{k}` else `{k},{j}`
                if idx[key] then
                    table.insert(row, idx[key])
                end
            end
            -- The middle row and column hold two lines of three, either side.
            for s0 = 1, #row, 3 do
                local line = { row[s0], row[s0 + 1], row[s0 + 2] }
                table.insert(MILL_LINES, line)
                for a = 1, 2 do
                    MILL_NEXT[line[a]] = MILL_NEXT[line[a]] or {}
                    MILL_NEXT[line[a + 1]] = MILL_NEXT[line[a + 1]] or {}
                    table.insert(MILL_NEXT[line[a]], line[a + 1])
                    table.insert(MILL_NEXT[line[a + 1]], line[a])
                end
            end
        end
    end
end

local function in_mill(b: { string }, i: number): boolean
    local p = b[i]
    for _, l in MILL_LINES do
        if (l[1] == i or l[2] == i or l[3] == i) and b[l[1]] == p and b[l[2]] == p and b[l[3]] == p then
            return true
        end
    end
    return false
end

local function stones(st: State, c: string): (number, number)
    local s = as("O", c)
    local on_board = 0
    for i = 1, 64 do
        on_board += if st.b[i] == s then 1 else 0
    end
    return on_board, select(2, string.gsub(st.pocket, s, ""))
end

-- st.ep is 1 while the side to move has made a mill and takes a stone: then
-- its moves are { i, i } on the stones it may take.
local function mill_legal(st: State, only: number?): { { number } }
    local moves = {}
    local me, them = st.turn, other(st.turn)
    if st.ep == 1 then
        local free = {}
        local all = {}
        for i = 1, 64 do
            if st.b[i] == as("O", them) then
                table.insert(all, i)
                if not in_mill(st.b, i) then
                    table.insert(free, i)
                end
            end
        end
        for _, i in (if #free > 0 then free else all) do
            if only == nil or only == i then
                table.insert(moves, { i, i })
            end
        end
        return moves
    end
    local on_board, in_hand = stones(st, me)
    local hand = if me == "w" then 101 else 111
    if in_hand > 0 then
        if only == nil or only == hand then
            for i in IS_POINT do
                if st.b[i] == "." then
                    table.insert(moves, { hand, i })
                end
            end
        end
        return moves
    end
    for i in IS_POINT do
        if st.b[i] == as("O", me) and (only == nil or only == i) then
            local to = if on_board <= 3 then nil else MILL_NEXT[i]
            for j in (if to then {} else IS_POINT) do
                if st.b[j] == "." then
                    table.insert(moves, { i, j })
                end
            end
            for _, j in to or {} do
                if st.b[j] == "." then
                    table.insert(moves, { i, j })
                end
            end
        end
    end
    return moves
end

local function mill_apply(st: State, from: number, to: number): State
    local b = table.clone(st.b)
    local n = table.clone(st)
    n.b, n.from, n.to, n.result, n.why = b, from, to, "", ""
    n.half = st.half + 1
    if st.ep == 1 then
        b[to] = "."   -- taken
        n.ep, n.turn, n.half = 0, other(st.turn), 0
        return n
    end
    local p = piece_at(st, from)
    if from > 64 then
        n.pocket = (string.gsub(st.pocket, p, "", 1))
    else
        b[from] = "."
    end
    b[to] = p
    if in_mill(b, to) then
        n.ep = 1   -- and takes a stone before the other moves
    else
        n.turn = other(st.turn)
    end
    return n
end

local function promotes(st: State, from: number, to: number): boolean
    local _, tr = fr(to)
    return from <= 64 and string.upper(st.b[from]) == "P" and (tr == 0 or tr == 7)
end

-- The state after a move, which must be one pseudo() gave; a pawn that gets to
-- the last rank becomes `promo` ("Q", "R", "B", "N"), a queen if not said.
local function apply(st: State, from: number, to: number, promo: string?): State
    if st.var == "dama" then
        return dama_apply(st, from, to)
    elseif st.var == "mulino" then
        return mill_apply(st, from, to)
    end
    local b = table.clone(st.b)
    local p, taken = piece_at(st, from), b[to]
    local c = colour(p) :: string
    local n: State = { b = b, turn = other(c), castle = st.castle, ep = 0, half = st.half + 1, from = from,
                       to = to, result = "", why = "", tc = st.tc, inc = st.inc, wt = st.wt, bt = st.bt,
                       offer = st.offer, var = st.var, rk = st.rk, checks = st.checks, pocket = st.pocket,
                       pr = st.pr }
    if from > 64 then
        b[to] = p   -- put down from the hand
        n.pocket = (string.gsub(st.pocket, p, "", 1))
        return n
    end
    local ff, fr0 = fr(from)
    local tf, tr = fr(to)
    b[to], b[from] = p, "."
    local kind = string.upper(p)
    local qr, kr = rook_files(st)
    local castling = kind == "K" and (if st.var == "960" then taken == as("R", c) else math.abs(tf - ff) == 2)
    -- The piece taken, en passant too; nil if none.
    local victim = if taken ~= "." and colour(taken) ~= c then taken
        elseif kind == "P" and to == st.ep and taken == "." then as("P", other(c)) else nil
    local pr = if st.var == "zh" then string.split(st.pr, "") else nil
    if pr then
        if victim then
            local back = if pr[to] == "+" then "P" else string.upper(victim)
            local hand = string.split(st.pocket .. as(back, c), "")
            table.sort(hand)
            n.pocket = table.concat(hand)
        end
        pr[to], pr[from] = pr[from], "."
    end
    if kind == "P" then
        n.half = 0
        if to == st.ep and taken == "." then
            b[at(tf, fr0)] = "."   -- en passant
        end
        if math.abs(tr - fr0) == 2 then
            n.ep = at(ff, (tr + fr0) // 2)
        end
        if tr == 0 or tr == 7 then
            local pieces = if st.var == "anti" then "QRBNK" else "QRBN"
            b[to] = as(if promo and #promo == 1 and string.find(pieces, promo, 1, true) then promo else "Q", c)
            if pr then
                pr[to] = "+"
            end
        end
    elseif castling then
        local king_side = if st.var == "960" then tf == kr else tf == 6
        local rf, kt, rt = if king_side then kr else qr, if king_side then 6 else 2, if king_side then 5 else 3
        b[at(ff, tr)], b[at(rf, tr)] = ".", "."
        b[at(kt, tr)], b[at(rt, tr)] = as("K", c), as("R", c)
        if pr then
            pr[at(ff, tr)], pr[at(rf, tr)], pr[at(kt, tr)], pr[at(rt, tr)] = ".", ".", ".", "."
        end
    end
    if pr then
        n.pr = table.concat(pr)
    end
    if victim then
        n.half = 0
        if st.var == "atomic" then
            -- The blast: the taker and all but the pawns round it.
            b[to] = "."
            for _, d in KING do
                if on(tf + d[1], tr + d[2]) and string.upper(b[at(tf + d[1], tr + d[2])]) ~= "P" then
                    b[at(tf + d[1], tr + d[2])] = "."
                end
            end
            for _, right in { { "K", kr, 0 }, { "Q", qr, 0 }, { "k", kr, 7 }, { "q", qr, 7 } } do
                local king = if right[3] == 0 then "K" else "k"
                if b[at(right[2], right[3])] ~= as("R", colour(right[1]) :: string) or not table.find(b, king) then
                    n.castle = string.gsub(n.castle, right[1], "")
                end
            end
        end
    end
    local lose = { [at(qr, 0)] = "Q", [at(kr, 0)] = "K", [at(qr, 7)] = "q", [at(kr, 7)] = "k" }
    for _, sq in { from, to } do
        if lose[sq] then
            n.castle = string.gsub(n.castle, lose[sq], "")
        end
    end
    if p == "K" then
        n.castle = string.gsub(n.castle, "[KQ]", "")
    elseif p == "k" then
        n.castle = string.gsub(n.castle, "[kq]", "")
    end
    return n
end

-- Every move of the side to move that does not leave its king attacked,
-- as { from, to } pairs; or only those of the piece on `only`.
-- Whether the side that moved into n left its king to be taken: never for
-- horde's white, which has none; in atomic a king blown up is, the other's
-- blown up is not, and kings side by side are safe.
local function unsafe(st: State, n: State): boolean
    local me, them = st.turn, other(st.turn)
    if st.var == "horde" and me == "w" then
        return false
    end
    local k = king_of(n.b, me)
    if k == 0 then
        return true
    end
    if st.var == "atomic" then
        local e = king_of(n.b, them)
        local kf, kr = fr(k)
        local ef, er = fr(e)
        if e == 0 or (math.abs(kf - ef) <= 1 and math.abs(kr - er) <= 1) then
            return false
        end
    end
    return attacked(n.b, k, them)
end

-- In losing chess any king may be taken and a taking is a must: there all
-- the side's moves are looked at, and only the takings kept if there is one.
local function legal(st: State, only: number?): { { number } }
    if st.var == "dama" then
        return dama_legal(st, only)
    elseif st.var == "mulino" then
        return mill_legal(st, only)
    end
    local moves = {}
    local anti = st.var == "anti"
    local takes = false
    for i = 1, 64 do
        if (anti or only == nil or only == i) and colour(st.b[i]) == st.turn then
            local to = {}
            pseudo(st, i, to)
            for _, t in to do
                if anti then
                    local take = st.b[t] ~= "." or (t == st.ep and string.upper(st.b[i]) == "P")
                    if take and not takes then
                        takes, moves = true, {}
                    end
                    if take or not takes then
                        table.insert(moves, { i, t, if take then 1 else 0 })
                    end
                    continue
                end
                local n = apply(st, i, t)
                if not unsafe(st, n) and not (st.var == "race" and attacked(n.b, king_of(n.b, n.turn), st.turn)) then
                    table.insert(moves, { i, t })
                end
            end
        end
    end
    if st.var == "zh" then
        for k = 1, #DROPS do
            local id = (if st.turn == "w" then 100 else 110) + k
            if (only == nil or only == id) and piece_at(st, id) ~= "." then
                for t = 1, 64 do
                    local _, r = fr(t)
                    if st.b[t] == "." and not (k == 1 and (r == 0 or r == 7))
                        and not unsafe(st, apply(st, id, t)) then
                        table.insert(moves, { id, t })
                    end
                end
            end
        end
    end
    if anti and only then
        local mine = {}
        for _, m in moves do
            if m[1] == only then
                table.insert(mine, m)
            end
        end
        return mine
    end
    return moves
end

local function in_check(st: State): boolean
    local k = king_of(st.b, st.turn)
    if st.var == "anti" or k == 0 then
        return false
    end
    if st.var == "atomic" then
        local e = king_of(st.b, other(st.turn))
        local kf, kr = fr(k)
        local ef, er = fr(e)
        if e == 0 or (math.abs(kf - ef) <= 1 and math.abs(kr - er) <= 1) then
            return false
        end
    end
    return attacked(st.b, k, other(st.turn))
end

-- Whether `c` has enough to mate with at all: a pawn, a rook, a queen or two
-- minor pieces. Out of time against a side that has not, it is a draw.
local function can_mate(st: State, c: string): boolean
    if st.var ~= "std" and st.var ~= "960" then
        return true   -- a king alone can win the hill or the race, and so on
    end
    local b = st.b
    local minor = 0
    for _, p in b do
        if colour(p) == c then
            local k = string.upper(p)
            if k == "P" or k == "R" or k == "Q" then
                return true
            elseif k == "N" or k == "B" then
                minor += 1
            end
        end
    end
    return minor >= 2
end

-- Whether the game is over after the last move, and why.
local function judge(st: State): State
    local mover = other(st.turn)
    local v = st.var
    if v == "dama" then
        if #legal(st) == 0 then
            st.result, st.why = mover, "blocked"
        elseif st.half >= 80 then
            st.result, st.why = "=", "forty"
        end
        return st
    elseif v == "mulino" then
        if st.ep == 1 then
            return st   -- a stone to take first
        end
        local on_board, in_hand = stones(st, st.turn)
        if on_board + in_hand < 3 then
            st.result, st.why = mover, "few"
        elseif #legal(st) == 0 then
            st.result, st.why = mover, "blocked"
        elseif st.half >= 100 then
            st.result, st.why = "=", "fifty"
        end
        return st
    end
    if v == "atomic" and king_of(st.b, st.turn) == 0 then
        st.result, st.why = mover, "atomic"
        return st
    elseif v == "horde" and not string.find(table.concat(st.b), "[PNBRQ]") then
        st.result, st.why = "b", "horde"
        return st
    elseif v == "koth" then
        local f, r = fr(king_of(st.b, mover))
        if (f == 3 or f == 4) and (r == 3 or r == 4) then
            st.result, st.why = mover, "hill"
            return st
        end
    elseif v == "3check" and in_check(st) then
        local w, b = tonumber(string.sub(st.checks, 1, 1)) or 0, tonumber(string.sub(st.checks, 2, 2)) or 0
        if mover == "w" then
            w += 1
        else
            b += 1
        end
        st.checks = `{w}{b}`
        if math.max(w, b) >= 3 then
            st.result, st.why = mover, "checks"
            return st
        end
    elseif v == "race" then
        local _, wr = fr(king_of(st.b, "w"))
        local _, br = fr(king_of(st.b, "b"))
        if wr == 7 and br == 7 then
            st.result, st.why = "=", "race"
            return st
        elseif br == 7 then
            st.result, st.why = "b", "race"
            return st
        elseif wr == 7 then
            -- Black's move to catch up, if it has one.
            local catch = false
            for _, m in (if st.turn == "b" then legal(st) else {}) do
                local _, r = fr(m[2])
                catch = catch or (st.b[m[1]] == "k" and r == 7)
            end
            if not catch then
                st.result, st.why = "w", "race"
                return st
            end
        end
    end
    if #legal(st) == 0 then
        if v == "anti" then
            st.result, st.why = st.turn, "anti"
        elseif in_check(st) then
            st.result, st.why = other(st.turn), "mate"
        else
            st.result, st.why = "=", "stalemate"
        end
    elseif st.half >= 100 then
        st.result, st.why = "=", "fifty"
    elseif v == "std" or v == "960" or v == "3check" then
        local minor = {}
        for _, p in st.b do
            if p ~= "." and string.upper(p) ~= "K" then
                table.insert(minor, string.upper(p))
            end
        end
        if #minor == 0 or (v ~= "3check" and #minor == 1 and (minor[1] == "N" or minor[1] == "B")) then
            st.result, st.why = "=", "material"
        end
    end
    return st
end

local function save(st: State): string
    return `{table.concat(st.b)} {st.turn} {if st.castle == "" then "-" else st.castle} {st.ep} {st.half} `
        .. `{st.from} {st.to} {if st.result == "" then "-" else st.result} {if st.why == "" then "-" else st.why} `
        .. `{st.tc} {st.inc} {string.format("%.1f", st.wt)} {string.format("%.1f", st.bt)} `
        .. `{if st.offer == "" then "-" else st.offer} {st.var} {st.rk} {st.checks} `
        .. `{if st.pocket == "" then "-" else st.pocket} {if st.pr == "" then "-" else st.pr}`
end

local function load(w: { string }, k: number): State?
    if #(w[k] or "") ~= 64 then
        return nil
    end
    local function word(s: string?): string
        return if s == nil or s == "-" then "" else s
    end
    return { b = string.split(w[k], ""), turn = w[k + 1] or "w", castle = word(w[k + 2]),
             ep = tonumber(w[k + 3]) or 0, half = tonumber(w[k + 4]) or 0, from = tonumber(w[k + 5]) or 0,
             to = tonumber(w[k + 6]) or 0, result = word(w[k + 7]), why = word(w[k + 8]),
             tc = tonumber(w[k + 9]) or 0, inc = tonumber(w[k + 10]) or 0, wt = tonumber(w[k + 11]) or 0,
             bt = tonumber(w[k + 12]) or 0, offer = word(w[k + 13]),
             -- Boards kept before the variants have none of the three: classical.
             var = if VARIANT_NAME[w[k + 14] or ""] then w[k + 14] :: string else "std",
             rk = if string.match(w[k + 15] or "", "^%d%d$") then w[k + 15] :: string else "07",
             checks = if string.match(w[k + 16] or "", "^%d%d$") then w[k + 16] :: string else "00",
             pocket = if string.match(w[k + 17] or "", "^[PNBRQOpnbrqo]+$") then w[k + 17] :: string else "",
             pr = if string.match(w[k + 18] or "", "^[.+]+$") and #(w[k + 18] :: string) == 64
                 then w[k + 18] :: string else "" }
end

-- A board's cell as a key, and back.
local function key_of(x: number, y: number, z: number): string
    return `{x},{y},{z}`
end

local function cell_of(key: string): (number, number, number)
    local w = string.split(key, ",")
    return tonumber(w[1]) or 0, tonumber(w[2]) or 0, tonumber(w[3]) or 0
end

local heard: (string) -> () = function(_) end
local blow: (any, State) -> () = function(_, _) end
local from_client: (number, string) -> () = function(_, _) end


local function who(c: string): string
    return if c == "w" then "bianco" else "nero"
end

-- ==================================================================== server

if serving then
    core.net.open()

    local BOARD = core.register_node("chess:board", 120, 80, 50, true, 0, "upright", 0, 1.0, "wood")
    core.node_model(BOARD, core.register_model("chess:board", "board.gltf"))
    core.node_box(BOARD, 0, 0, 0, 1, TOP, 1)
    for _, c in { "w", "b" } do
        for _, k in { "p", "n", "b", "r", "q", "k", "m", "d", "o" } do
            core.register_model(`chess:{c}_{k}`, `{c}_{k}.gltf`)
        end
    end
    core.register_model("chess:blast", "blast.gltf")   -- atomic's explosion
    core.register_model("chess:smoke", "smoke.gltf")
    core.recipe_shaped("chess:board", 1,
        "ore:coal      timber:planks ore:coal",
        "timber:planks ore:coal      timber:planks",
        "timber:stick  timber:planks timber:stick")

    -- The clocks a game can be played with: seconds each and seconds a move.
    local CLOCKS = { { 0, 0, "Senza orologio" }, { 180, 2, "3 minuti + 2 secondi" }, { 300, 0, "5 minuti" },
                     { 600, 0, "10 minuti" } }

    type Board = {
        facing: number,
        st: State,
        white: number?,
        black: number?,
        since: number,              -- `now` when the side to move's clock started
        seen: { [string]: number }, -- positions since the last capture or pawn move
        plies: number,              -- moves made this game
        offered: number,            -- the move at which a draw was last offered
        duo: boolean,               -- two players have sat at this game
    }
    local boards: { [string]: Board } = {}
    local now = 0.0                                 -- seconds the server has run
    local menu_of: { [number]: string } = {}        -- the board a player's menu is about
    local promoting: { [number]: { any } } = {}     -- a pawn move waiting for its piece

    local function tell(player: number, text: string)
        if drawing then
            heard(text)
        else
            core.net.send(player, text)
        end
    end

    local function seated(bd: Board): { number }
        local out = {}
        if bd.white then
            table.insert(out, bd.white)
        end
        if bd.black and bd.black ~= bd.white then
            table.insert(out, bd.black)
        end
        return out
    end

    -- Which side a player plays here: "w", "b", "both" alone at a game nobody
    -- else has sat at, nil watching. Once two have played, the one who stays
    -- does not play for the one who got up.
    local function side_of(p: number, bd: Board): string?
        if bd.white == p then
            return if bd.black == nil and not bd.duo then "both" else "w"
        elseif bd.black == p then
            return if bd.white == nil and not bd.duo then "both" else "b"
        end
        return nil
    end

    local function running(st: State): boolean
        return st.tc > 0 and st.result == "" and st.from ~= 0
    end

    -- A clock stops while the side to move has nobody at the board for it:
    -- its player got up from a game of two, or everybody did.
    local function paused(bd: Board): boolean
        local seat = if bd.st.turn == "w" then bd.white else bd.black
        local across = if bd.st.turn == "w" then bd.black else bd.white
        return seat == nil and (across == nil or bd.duo)
    end

    local function ticking(bd: Board): boolean
        return running(bd.st) and not paused(bd)
    end

    local function seats(bd: Board): string
        local s = (if bd.white then "w" else "") .. (if bd.black then "b" else "")
            .. (if running(bd.st) and paused(bd) then "!" else "")   -- the clock stopped
            .. (if bd.duo then "2" else "")                         -- a game of two
        return if s == "" then "-" else s
    end

    -- The side to move's time spent since its turn began, taken off its clock.
    local function charge(bd: Board)
        local st = bd.st
        if ticking(bd) then
            local spent = now - bd.since
            if st.turn == "w" then
                st.wt = math.max(0, st.wt - spent)
            else
                st.bt = math.max(0, st.bt - spent)
            end
        end
        bd.since = now
    end

    local function state_line(key: string, bd: Board): string
        local st = bd.st
        if ticking(bd) then
            st = table.clone(st)
            local spent = now - bd.since
            if st.turn == "w" then
                st.wt = math.max(0, st.wt - spent)
            else
                st.bt = math.max(0, st.bt - spent)
            end
        end
        return `s {key} {bd.facing} {seats(bd)} {save(st)}`
    end

    local function everybody(text: string)
        if drawing then
            heard(text)   -- one player, and this is their machine
            return
        end
        for _, p in core.players() do
            core.net.send(p, text)
        end
    end

    -- A board a line: its cell, its facing, its game, the positions seen
    -- since the last capture or pawn move ("<position>=<times>;...") and
    -- whether two have played it.
    local function keep()
        local lines = {}
        for key, bd in boards do
            local seen = {}
            for pos, n in bd.seen do
                table.insert(seen, `{pos}={n}`)
            end
            table.insert(lines, `{key} {bd.facing} {save(bd.st)} {table.concat(seen, ";")} {if bd.duo then 1 else 0}`)
        end
        core.storage_set("boards", table.concat(lines, "\n"))
    end

    local function told(key: string, bd: Board)
        keep()
        everybody(state_line(key, bd))
    end

    -- A position as the repetition rule sees it: the pieces, who moves, the
    -- castling left, and the en passant square only if a pawn can take there.
    local function position(st: State): string
        local ep = 0
        if st.ep ~= 0 then
            for _, m in legal(st) do
                if m[2] == st.ep and m[1] <= 64 and string.upper(st.b[m[1]]) == "P" then
                    ep = st.ep
                    break
                end
            end
        end
        return `{table.concat(st.b)}{st.turn}{st.castle}{ep}{st.checks}{st.pocket}`
    end

    local function make(facing: number, st: State): Board
        local bd: Board = { facing = facing, st = st, since = now, seen = {}, plies = 0, offered = -1,
                            duo = false }
        bd.seen[position(st)] = 1
        return bd
    end

    local function restart(bd: Board, tc: number, inc: number, var: string)
        local fresh = make(bd.facing, new_game(tc, inc, var))
        bd.st, bd.since, bd.seen, bd.plies, bd.offered = fresh.st, fresh.since, fresh.seen, 0, -1
        bd.duo = bd.white ~= nil and bd.black ~= nil
    end

    local function end_game(bd: Board, result: string, why: string)
        charge(bd)
        bd.st.result, bd.st.why, bd.st.offer = result, why, ""
    end

    core.event.on("core:world_ready", function()
        for _, line in string.split(core.storage_get("boards") or "", "\n") do
            local w = string.split(line, " ")
            local st = load(w, 3)
            if st then
                local bd = make(tonumber(w[2]) or 0, st)
                local tail = #w - 1   -- the seen positions: the line's last but one, however long its game
                for _, entry in string.split(w[tail] or "", ";") do
                    local pos, n = string.match(entry, "^(.+)=(%d+)$")
                    if pos then
                        bd.seen[pos] = tonumber(n) :: number
                    end
                end
                bd.duo = w[tail + 1] == "1"
                boards[w[1]] = bd
            end
        end
    end)

    -- A table on the spawn's square, once a world: three nodes on from where
    -- players arrive each way (+X, +Z), white's side towards them. Moved round
    -- the spawn if that cell is taken; left out if no cell near is free.
    local ready, laid = false, true
    core.event.on("core:world_ready", function()
        -- Only where there is a square: mods/guard's, the sandbox's. The
        -- Inferno loads this mod for what it holds, not for a table at its door.
        ready, laid = true, core.storage_get("plaza") == "1" or core.node_id("guard:cloud") == 0
    end)
    core.event.on("core:tick", function(_)
        local sx, sy, sz = core.world_spawn()
        if not ready or laid or sx == nil then
            return
        end
        local x0, y, z0 = math.floor(sx) + 3, math.floor(sy), math.floor(sz) + 3
        if core.loaded(x0, y, z0) == 0 then
            return   -- the spawn's columns are not there yet
        end
        laid = true
        core.storage_set("plaza", "1")
        for _, d in { { 0, 0 }, { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 }, { 2, 0 }, { 0, 2 } } do
            local x, z = x0 + d[1], z0 + d[2]
            if core.get_block(x, y, z) == 0 and core.get_block(x, y + 1, z) == 0
                and core.get_block(x, y - 1, z) ~= 0 then
                core.set_block(x, y, z, BOARD)
                core.set_facing(x, y, z, 2)
                core.log(`chess: a table on the spawn's square at {x} {y} {z}`)
                return
            end
        end
        core.log("chess: no free cell by the spawn for a table")
    end)

    local function board_at(x: number, y: number, z: number): (string, Board)
        local key = key_of(x, y, z)
        local bd = boards[key]
        if bd == nil then
            bd = make(core.get_facing(x, y, z), new_game())
            boards[key] = bd
            keep()
        end
        return key, bd
    end

    core.event.on("core:placed", function(ev)
        if ev.id == BOARD then
            local key = key_of(ev.x, ev.y, ev.z)
            boards[key] = nil   -- a fresh game on a fresh table
            local _, bd = board_at(ev.x, ev.y, ev.z)
            everybody(state_line(key, bd))
        end
    end)

    core.event.on("core:broken", function(ev)
        local key = key_of(ev.x, ev.y, ev.z)
        local bd = boards[key]
        if bd == nil then
            return
        end
        for _, p in seated(bd) do
            tell(p, "out")
        end
        boards[key] = nil
        keep()
        everybody(`g {key}`)
    end)

    local function near(p: number, key: string, reach: number): boolean
        local x, y, z = cell_of(key)
        local px, py, pz = core.player_pos(p)
        if px == nil then
            return false
        end
        return math.abs(px - x - 0.5) < reach and math.abs(py - y) < reach and math.abs(pz - z - 0.5) < reach
    end

    -- The colour whose side of the board the player stands on: white's half
    -- is the one rank 1 is on, as the client turns the board by its facing.
    local function half_of(p: number, key: string, bd: Board): string
        local x, _, z = cell_of(key)
        local px, _, pz = core.player_pos(p)
        local a = bd.facing * math.pi / 2
        local v = ((px or 0) - x - 0.5) * math.sin(a) + ((pz or 0) - z - 0.5) * math.cos(a)
        return if v < 0 then "b" else "w"
    end

    -- Right click: sit on your side of the board if it is free, else on the
    -- other, watching if both are taken.
    core.event.on("core:used", function(ev)
        if ev.id ~= BOARD then
            return
        end
        ev.cancel = true
        local key, bd = board_at(ev.x, ev.y, ev.z)
        local p = ev.player
        local c = if bd.white == p then "w" elseif bd.black == p then "b" else nil
        charge(bd)   -- the clock may start again with somebody sitting
        if c == nil then
            local want = half_of(p, key, bd)
            if bd.white == nil and (want == "w" or bd.black ~= nil) then
                bd.white, c = p, "w"
            elseif bd.black == nil then
                bd.black, c = p, "b"
            else
                c = "v"
            end
        end
        if bd.white and bd.black and bd.st.result == "" then
            bd.duo = true
        end
        everybody(state_line(key, bd))
        tell(p, `sit {key} {c}`)
    end)

    local function stand(p: number, bd: Board)
        charge(bd)   -- and may stop with nobody to move
        if bd.white == p then
            bd.white = nil
        end
        if bd.black == p then
            bd.black = nil
        end
        keep()   -- with the clock as it stopped
    end

    -- Whether p may play from-to on this board now.
    local function may(p: number, key: string, bd: Board, from: number, to: number): boolean
        local st = bd.st
        local me = side_of(p, bd)
        if st.result ~= "" or me == nil or (me ~= "both" and me ~= st.turn) or not near(p, key, 8) then
            return false
        end
        for _, m in legal(st, from) do
            if m[2] == to then
                return true
            end
        end
        return false
    end

    local function play(key: string, bd: Board, from: number, to: number, promo: string?)
        local st = bd.st
        local mover = st.turn
        local n = judge(apply(st, from, to, promo))
        if running(st) then
            local spent = if ticking(bd) then now - bd.since else 0
            local left = (if mover == "w" then st.wt else st.bt) - spent + st.inc
            if mover == "w" then
                n.wt = left
            else
                n.bt = left
            end
        end
        -- Playing on answers a draw offered to the one who plays.
        if n.offer == other(mover) or n.result ~= "" then
            n.offer = ""
        end
        if n.half == 0 then
            bd.seen = {}
        end
        local pos = position(n)
        bd.seen[pos] = (bd.seen[pos] or 0) + 1
        if n.result == "" and bd.seen[pos] >= 3 then
            n.result, n.why = "=", "repetition"
        end
        bd.st, bd.since, bd.plies = n, now, bd.plies + 1
        if n.var == "atomic" and n.b[to] == "." then
            -- The taker blew up with the rest: everybody near hears it.
            local x, y, z = cell_of(key)
            core.play_sound("chess:blast", x + 0.5, y + TOP, z + 0.5)
        end
        told(key, bd)
    end

    -- The panels are a player's own (chess:m<id>, chess:q<id>): a panel's
    -- contents are its mod's, and two players with a menu up at once would
    -- otherwise see each other's.
    local function panel(name: string, rows: { { string } }, p: number)
        local h = 0.5
        for _, r in rows do
            h += if r[1] == "label" then 0.8 else 1.1
        end
        core.ui_define(name, 9, h + 0.2)
        local y = 0.4
        for _, r in rows do
            if r[1] == "label" then
                core.ui_label(name, 0.5, y, r[2])
                y += 0.8
            else
                core.ui_button(name, r[1], 0.5, y, 8, 0.9, r[2])
                y += 1.1
            end
        end
        core.ui_show(name, true, p)
    end

    -- Right click while sitting: offer or take a draw, resign, pick a clock.
    local function show_menu(p: number, key: string, bd: Board, confirm: boolean?)
        local me = side_of(p, bd)
        local st = bd.st
        local rows = {}
        menu_of[p] = key
        if me == nil then
            if p == bd.white or p == bd.black then
                return
            end
            panel(`chess:m{p}`, { { "label", "Guardi la partita" }, { "up", "Alzati" }, { "close", "Chiudi" } }, p)
            return
        end
        if st.result == "" and confirm then
            table.insert(rows, { "label", "Abbandoni la partita?" })
            table.insert(rows, { "resign_yes", "Sì, abbandono" })
            table.insert(rows, { "close", "No, continuo" })
            panel(`chess:m{p}`, rows, p)
            return
        end
        if st.result == "" then
            if me ~= "both" and st.offer == other(me) then
                table.insert(rows, { "label", `Il {who(st.offer)} propone la patta` })
                table.insert(rows, { "accept", "Accetta la patta" })
                table.insert(rows, { "decline", "Rifiuta" })
            elseif me ~= "both" and st.offer == me then
                table.insert(rows, { "label", "Patta proposta: aspetta la risposta" })
            elseif me ~= "both" and bd.offered == bd.plies then
                table.insert(rows, { "label", "Patta già proposta in questa mossa" })
            else
                table.insert(rows, { "draw", if me == "both" then "Patta" else "Proponi la patta" })
            end
            table.insert(rows, { "resign", if me == "both" then `Abbandona il {who(st.turn)}` else "Abbandona" })
        end
        if st.result ~= "" or st.from == 0 then
            table.insert(rows, { "var", `Variante: {VARIANT_NAME[st.var]} (cambia)` })
            table.insert(rows, { "label", if st.result ~= "" then "Nuova partita:" else "Orologio:" })
            for i, c in CLOCKS do
                table.insert(rows, { `tc{i}`, c[3] })
            end
        end
        table.insert(rows, { "up", "Alzati" })
        table.insert(rows, { "close", "Chiudi" })
        panel(`chess:m{p}`, rows, p)
    end

    core.event.on("core:ui_click", function(ev)
        local p = ev.player
        if ev.panel == `chess:q{p}` then
            core.ui_show(ev.panel, false, p)
            local waiting = promoting[p]
            promoting[p] = nil
            if waiting == nil or not string.find("QRBNK", ev.id, 1, true) or #ev.id ~= 1 then
                return
            end
            local key, from, to = waiting[1], waiting[2], waiting[3]
            local bd = boards[key]
            if bd and may(p, key, bd, from, to) then
                play(key, bd, from, to, ev.id)
            end
            return
        end
        if ev.panel ~= `chess:m{p}` then
            return
        end
        core.ui_show(ev.panel, false, p)
        local key = menu_of[p] or ""
        local bd = boards[key]
        if bd and ev.id == "up" then
            stand(p, bd)
            tell(p, "out")
            everybody(state_line(key, bd))
            return
        end
        local me = if bd then side_of(p, bd) else nil
        if bd == nil or me == nil then
            return
        end
        local st = bd.st
        local id = ev.id
        local tc = tonumber(string.match(id, "^tc(%d)$") or "")
        if st.result == "" and id == "resign" then
            show_menu(p, key, bd, true)
        elseif st.result == "" and id == "resign_yes" then
            end_game(bd, other(if me == "both" then st.turn else me), "resign")
            told(key, bd)
        elseif st.result == "" and (id == "accept" or id == "draw") and (me == "both" or st.offer == other(me)) then
            end_game(bd, "=", "agreed")
            told(key, bd)
        elseif st.result == "" and id == "draw" and st.offer == "" and bd.offered ~= bd.plies then
            st.offer, bd.offered = me, bd.plies
            told(key, bd)
            local them = if me == "w" then bd.black else bd.white
            if them then
                show_menu(them, key, bd)   -- the question on their screen
            end
        elseif st.result == "" and id == "decline" and st.offer == other(me) then
            st.offer = ""
            told(key, bd)
        elseif tc and CLOCKS[tc] and (st.result ~= "" or st.from == 0) then
            restart(bd, CLOCKS[tc][1], CLOCKS[tc][2], st.var)
            told(key, bd)
        elseif id == "var" and (st.result ~= "" or st.from == 0) then
            -- The next game round the list, set up now with the same clock.
            local next = VARIANTS[(table.find(VARIANTS, st.var) or 0) % #VARIANTS + 1]
            restart(bd, st.tc, st.inc, next)
            told(key, bd)
            show_menu(p, key, bd)
        end
    end)

    from_client = function(p: number, text: string)
        local w = string.split(text, " ")
        if w[1] == "hi" then
            for key, bd in boards do
                tell(p, state_line(key, bd))
            end
            return
        end
        local key = w[2] or ""
        local bd = boards[key]
        if bd == nil then
            return
        end
        if w[1] == "menu" then
            show_menu(p, key, bd)
        elseif w[1] == "new" then
            if bd.st.result ~= "" and side_of(p, bd) then
                restart(bd, bd.st.tc, bd.st.inc, bd.st.var)   -- the same clock and game as the last
                told(key, bd)
            end
        elseif w[1] == "mv" then
            local from, to = tonumber(w[3]) or 0, tonumber(w[4]) or 0
            if not may(p, key, bd, from, to) then
                tell(p, state_line(key, bd))   -- puts back the piece it put down
            elseif promotes(bd.st, from, to) and w[5] == nil then
                promoting[p] = { key, from, to }
                local rows = { { "label", "Il pedone diventa:" }, { "Q", "Donna" }, { "R", "Torre" },
                               { "B", "Alfiere" }, { "N", "Cavallo" } }
                if bd.st.var == "anti" then
                    table.insert(rows, { "K", "Re" })
                end
                panel(`chess:q{p}`, rows, p)
            else
                play(key, bd, from, to, w[5])
            end
        end
    end

    if not drawing then
        core.event.on("core:net_message", function(ev)
            from_client(ev.player, ev.text)
        end)
    end

    -- The clocks, and whoever left the world or walked away from a table
    -- gets up.
    local idle = 0.0
    core.event.on("core:tick", function(ev)
        now += ev.dt
        for key, bd in boards do
            local st = bd.st
            if ticking(bd) and (if st.turn == "w" then st.wt else st.bt) - (now - bd.since) <= 0 then
                -- Out of time loses, unless the other side could never mate.
                local them = other(st.turn)
                end_game(bd, if can_mate(st, them) then them else "=", "time")
                told(key, bd)
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
        for key, bd in boards do
            for _, p in seated(bd) do
                if not (here[p] and near(p, key, 8)) then
                    stand(p, bd)
                    if here[p] then
                        tell(p, "out")
                    end
                    everybody(state_line(key, bd))
                end
            end
        end
    end)
end

-- ==================================================================== client

if drawing then
    type Seen = { x: number, y: number, z: number, facing: number, seats: string, st: State, at: number }
    local seen: { [string]: Seen } = {}

    local sitting: string? = nil   -- the board this player sits at
    local playing = "v"            -- "w", "b", or "v" watching
    local picked: number? = nil    -- the square of the piece in hand
    local dragging = false
    local targets: { [number]: boolean } = {}
    local clock = 0.0              -- this machine's seconds, for the clocks shown

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

    -- A board's square (file, rank) to the world, its centre on the squares'
    -- top; the board turned by its cell's facing like its model.
    -- The mill's points are a 7 by 7 grid over the board, MILL_STEP apart.
    local MILL_STEP = SQ * 8 / 7
    local function world_of(s: Seen, f: number, r: number): (number, number, number)
        local a = s.facing * math.pi / 2
        local u, v = (f - 3.5) * SQ, (3.5 - r) * SQ
        if s.st.var == "mulino" and f >= 0 and f <= 6 and r >= 0 and r <= 6 then
            u, v = (f - 3) * MILL_STEP, (3 - r) * MILL_STEP
        end
        return s.x + 0.5 + u * math.cos(a) + v * math.sin(a), s.y + TOP, s.z + 0.5 - u * math.sin(a) + v * math.cos(a)
    end

    -- The square under the cursor, and where on the squares' top it points.
    local function under_cursor(s: Seen): (number?, number, number, number)
        local ox, oy, oz, dx, dy, dz = core.cursor_ray()
        if ox == nil or dy == nil or math.abs(dy) < 1e-4 then
            return nil, 0, 0, 0
        end
        local t = (s.y + TOP - oy) / dy
        if t <= 0 then
            return nil, 0, 0, 0
        end
        local px, pz = ox + dx * t, oz + dz * t
        local a = s.facing * math.pi / 2
        local wx, wz = px - s.x - 0.5, pz - s.z - 0.5
        local u = wx * math.cos(a) - wz * math.sin(a)
        local v = wx * math.sin(a) + wz * math.cos(a)
        local f, r = math.floor(u / SQ + 4), math.floor(4 - v / SQ)
        local hand = if s.st.var == "mulino" then 1 elseif s.st.var == "zh" then #DROPS else 0
        if s.st.var == "mulino" then
            -- The nearest point, if the cursor is near enough to it.
            local gf, gr = u / MILL_STEP + 3, 3 - v / MILL_STEP
            local nf, nr = math.floor(gf + 0.5), math.floor(gr + 0.5)
            if math.abs(gf - nf) < 0.4 and math.abs(gr - nr) < 0.4 and nf >= 0 and nf <= 6 and nr >= 0 and nr <= 6
                and IS_POINT[at(nf, nr)] then
                return at(nf, nr), px, s.y + TOP, pz
            end
        elseif on(f, r) then
            return at(f, r), px, s.y + TOP, pz
        end
        if f == 9 and r >= 1 and r <= hand then
            return 100 + r, px, s.y + TOP, pz
        elseif f == -2 and 7 - r >= 1 and 7 - r <= hand then
            return 110 + 7 - r, px, s.y + TOP, pz
        end
        return nil, px, s.y + TOP, pz
    end

    local function may_move(s: Seen, c: string): boolean
        return playing == c
            or (playing ~= "v" and not string.find(s.seats, c, 1, true) and not string.find(s.seats, "2", 1, true))
    end

    local function drop()
        picked, dragging, targets = nil, false, {}
    end

    local function get_up()
        sitting = nil
        drop()
        core.fixed_view(0, 0, 0, 0, 0, 50, 0, 0.8)
        core.engine_hud(true)
    end

    -- Atomic chess's explosions as they go off: a ball of fire, smoke going
    -- up, and every piece blown up thrown off the board, turning, till it
    -- falls out of sight.
    type Blast = { x: number, y: number, z: number, t0: number, spin: number }
    type Flung = { p: string, x: number, y: number, z: number, vx: number, vy: number, vz: number,
                   spin: number, t0: number }
    local blasts: { Blast } = {}
    local flung: { Flung } = {}

    -- A new move on s that left its taker's square empty went off: the
    -- pieces gone since `old` (the mover too) fly from where they stood.
    blow = function(s: Seen, old: State)
        local st = s.st
        if st.var ~= "atomic" or st.from == 0 or st.from > 64 or (st.from == old.from and st.to == old.to)
            or st.b[st.to] ~= "." then
            return
        end
        local tf, tr = fr(st.to)
        local cx, cy, cz = world_of(s, tf, tr)
        table.insert(blasts, { x = cx, y = cy + 0.05, z = cz, t0 = clock, spin = math.random() * 6.3 })
        for i = 1, 64 do
            local p = old.b[i]
            if p == "." or st.b[i] ~= "." then
                continue
            end
            local f, r = fr(i)
            if i == st.from then
                f, r = tf, tr   -- the taker went off on the square it took
            end
            local x, y, z = world_of(s, f, r)
            local dx, dz = x - cx, z - cz
            local d = math.sqrt(dx * dx + dz * dz)
            if d < 0.01 then
                local a = math.random() * 6.3
                dx, dz, d = math.cos(a), math.sin(a), 1
            end
            local speed = 1.2 + math.random() * 0.8   -- off the table before they come down
            table.insert(flung, { p = p, x = x, y = y, z = z, vx = dx / d * speed, vy = 1.4 + math.random() * 0.8,
                                  vz = dz / d * speed, spin = (math.random() - 0.5) * 24, t0 = clock })
        end
    end

    heard = function(text: string)
        local w = string.split(text, " ")
        if w[1] == "s" then
            local st = load(w, 5)
            if st then
                local x, y, z = cell_of(w[2])
                local before = seen[w[2]]
                seen[w[2]] = { x = x, y = y, z = z, facing = tonumber(w[3]) or 0, seats = w[4], st = st, at = clock }
                if before then
                    blow(seen[w[2]], before.st)
                end
                if w[2] == sitting and picked and colour(piece_at(st, picked)) ~= st.turn then
                    drop()
                end
            end
        elseif w[1] == "g" then
            seen[w[2]] = nil
            if sitting == w[2] then
                get_up()
            end
        elseif w[1] == "sit" then
            local s = seen[w[2]]
            if s == nil then
                core.log(`chess: told to sit at {w[2]}, a table this machine has not been told of`)
                return
            end
            sitting, playing = w[2], w[3]
            core.log(`chess: sitting at {w[2]} as {playing}`)
            drop()
            -- Above the middle of the board, from the side this player plays,
            -- aimed a little short of the middle so the near rank clears the
            -- message line at the bottom.
            local yaw = -s.facing * 90 + (if playing == "b" then 180 else 0)
            local ax, az = -math.sin(math.rad(yaw)) * 0.1, math.cos(math.rad(yaw)) * 0.1
            core.fixed_view(s.x + 0.5 + ax, s.y + TOP, s.z + 0.5 + az, 1.0, yaw, 62, 0, 1.2)
            core.engine_hud(false)   -- and the bars of hunger and experience with it
        elseif w[1] == "out" then
            get_up()
        end
    end

    if not serving then
        core.event.on("core:net_message", function(ev)
            heard(ev.text)
        end)
    end

    -- Every board there is, asked for once there is a server to ask.
    local said_hi = false
    core.event.on("core:tick", function(ev)
        clock += ev.dt
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

    local function move(s: Seen, from: number, to: number)
        ask(`mv {sitting} {from} {to}`)
        if not promotes(s.st, from, to) then
            local before = s.st
            s.st = judge(apply(s.st, from, to))   -- put down now; the server's answer follows
            blow(s, before)
        end
        drop()   -- a pawn on the last rank waits for the piece it becomes
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
        if a == "mouse_right" then
            ev.cancel = true
            drop()
            ask(`menu {sitting}`)
        elseif a == "mouse_left" then
            ev.cancel = true
            if s.st.result ~= "" then
                if playing ~= "v" then
                    ask(`new {sitting}`)
                end
                return
            end
            local i = under_cursor(s)
            if i ~= nil and s.st.var == "mulino" and s.st.ep == 1 and may_move(s, s.st.turn) then
                -- A mill made: the click takes the stone it is on, if it may.
                for _, m in legal(s.st, i) do
                    if m[1] == i and m[2] == i then
                        move(s, i, i)
                        return
                    end
                end
                drop()
            elseif i == nil then
                drop()
            elseif picked and targets[i] then
                move(s, picked, i)
            elseif colour(piece_at(s.st, i)) == s.st.turn and may_move(s, s.st.turn) then
                picked, dragging, targets = i, true, {}
                for _, m in legal(s.st, i) do
                    targets[m[2]] = true
                end
            else
                drop()
            end
        elseif a == "mouse_left_up" and dragging then
            dragging = false
            local i = under_cursor(s)
            if picked and i and i ~= picked and targets[i] then
                move(s, picked, i)
            end
        end
    end)

    local MODELS: { [string]: number } = {}
    local function model(p: string): number
        local name = `chess:{colour(p)}_{string.lower(p)}`
        local id = MODELS[name]
        if id == nil or id == 0 then
            id = core.model_id(name)
            MODELS[name] = id
        end
        return id
    end

    -- Where a square is, or a slot of a crazyhouse hand: white's a file off
    -- the board on white's right, from the second rank (the first is under
    -- the bottom of the view) pawn first, black's the same on black's.
    local function place(i: number): (number, number)
        if i <= 64 then
            return fr(i)
        end
        local k = i % 10
        return if i < 110 then 9 else -2, if i < 110 then k else 7 - k
    end

    -- A flat mark on a square, just over the wood.
    local function mark(s: Seen, i: number, size: number, r: number, g: number, b: number, alpha: number)
        local f, rr = place(i)
        local x, y, z = world_of(s, f, rr)
        core.render.box(x, y + 0.002, z, SQ * size, 0.003, SQ * size, r, g, b, alpha)
    end

    core.render.pass("chess:pieces", 100, function()
        local me = core.players()[1]
        if me == nil then
            return
        end
        local mx, my, mz = core.player_pos(me)
        if mx == nil then
            return
        end
        for key, s in seen do
            if math.abs(s.x - mx) > 48 or math.abs(s.z - mz) > 48 or math.abs(s.y - my) > 48 then
                continue
            end
            local st = s.st
            local here = key == sitting
            if st.from > 0 then
                mark(s, st.from, 0.98, 205, 200, 90, 255)
                mark(s, st.to, 0.98, 215, 210, 80, 255)
            end
            if st.result == "" and in_check(st) then
                mark(s, king_of(st.b, st.turn), 0.98, 220, 50, 40, 255)
            end
            local cx, cy, cz
            local hover
            if here then
                hover, cx, cy, cz = under_cursor(s)
                if hover and not dragging then
                    mark(s, hover, 0.98, 235, 235, 225, 255)
                end
                if picked then
                    mark(s, picked, 0.98, 110, 190, 90, 255)
                    for t in targets do
                        mark(s, t, if st.b[t] ~= "." then 0.9 else 0.3, 60, 120, 60, 255)
                    end
                end
            end
            if st.var == "mulino" then
                -- A light board over the squares, its lines and points.
                local cx0, cy0, cz0 = world_of(s, 3, 3)
                core.render.box(cx0, cy0 + 0.001, cz0, 8 * SQ, 0.003, 8 * SQ, 214, 186, 140, 255)
                for _, l in MILL_LINES do
                    local f1, r1 = fr(l[1])
                    local f3, r3 = fr(l[3])
                    local x1, y1, z1 = world_of(s, f1, r1)
                    local x3, _, z3 = world_of(s, f3, r3)
                    core.render.box((x1 + x3) / 2, y1 + 0.004, (z1 + z3) / 2, math.abs(x3 - x1) + 0.012, 0.003,
                                    math.abs(z3 - z1) + 0.012, 60, 40, 25, 255)
                end
                for i in IS_POINT do
                    local f, r = fr(i)
                    local x, y, z = world_of(s, f, r)
                    core.render.box(x, y + 0.005, z, 0.03, 0.004, 0.03, 60, 40, 25, 255)
                end
            end
            local turn = s.facing * math.pi / 2
            for i, p in st.b do
                if p == "." then
                    continue
                end
                -- Knights look across the board at the other side.
                local yaw = turn + (if colour(p) == "b" then math.pi else 0)
                if here and dragging and i == picked and hover ~= nil then
                    core.render.model(model(p), cx, cy + 0.03, cz, yaw)
                else
                    local f, r = fr(i)
                    local x, y, z = world_of(s, f, r)
                    core.render.model(model(p), x, y, z, yaw)
                end
            end
            -- The crazyhouse hands: a tray beside the table each, a piece a
            -- kind held, and as many pips as there are of it past one.
            if st.var == "zh" or st.var == "mulino" then
                local kinds = if st.var == "mulino" then "O" else DROPS
                for _, c in { "w", "b" } do
                    local base = if c == "w" then 100 else 110
                    local tf, tr = place(base + 3)
                    local x, y, z = world_of(s, tf, tr)
                    local along = s.facing % 2 == 0
                    core.render.box(x, y - 0.01, z, if along then SQ else 5 * SQ, 0.02, if along then 5 * SQ else SQ,
                                    90, 55, 30, 255)
                    for k = 1, #kinds do
                        local p = as(string.sub(kinds, k, k), c)
                        local n = select(2, string.gsub(st.pocket, p, ""))
                        if n == 0 then
                            continue
                        end
                        local f, r = place(base + k)
                        local yaw = turn + (if c == "b" then math.pi else 0)
                        if here and dragging and picked == base + k and hover ~= nil then
                            core.render.model(model(p), cx, cy + 0.03, cz, yaw)
                        end
                        if not (here and dragging and picked == base + k and hover ~= nil) or n > 1 then
                            local px, py, pz = world_of(s, f, r)
                            core.render.model(model(p), px, py, pz, yaw)
                        end
                        for j = 2, math.min(n, 9) do
                            local qx, qy, qz = world_of(s, f + (if c == "w" then -0.42 else 0.42), r - 0.4 + (j - 1) * 0.16)
                            core.render.box(qx, qy + 0.004, qz, 0.012, 0.008, 0.012, 230, 220, 190, 255)
                        end
                    end
                end
            end
        end
        -- The explosions: the fire swells in a tenth of a second and dies in
        -- four; the smoke rises and swells for a second and a half.
        local fire, smoke = core.model_id("chess:blast"), core.model_id("chess:smoke")
        for k = #blasts, 1, -1 do
            local b = blasts[k]
            local t = clock - b.t0
            if t > 1.6 then
                table.remove(blasts, k)
                continue
            end
            if t < 0.5 then
                local size = if t < 0.1 then 0.1 + 5 * t else 0.6 * (1 - (t - 0.1) / 0.4)
                core.render.model(fire, b.x, b.y - size * 0.3, b.z, b.spin + t * 3, size)
            end
            if t > 0.12 then
                local u = (t - 0.12) / 1.48
                core.render.model(smoke, b.x, b.y + 0.1 + 0.5 * u, b.z, b.spin - t, 0.5 * math.sin(math.pi * u) + 0.02)
            end
        end
        for k = #flung, 1, -1 do
            local f = flung[k]
            local t = clock - f.t0
            if t > 2.2 then
                table.remove(flung, k)
                continue
            end
            -- Half gravity: the pieces float a little, like a cartoon's.
            core.render.model(model(f.p), f.x + f.vx * t, f.y + f.vy * t - 2.45 * t * t, f.z + f.vz * t, f.spin * t)
        end
    end)

    -- Seconds left to `c` as the board was last told, less what has passed
    -- since if it is c's turn.
    local function left(s: Seen, c: string): number
        local st = s.st
        local t = if c == "w" then st.wt else st.bt
        if st.result == "" and st.from ~= 0 and st.turn == c and not string.find(s.seats, "!", 1, true) then
            t -= clock - s.at
        end
        return math.max(0, t)
    end

    local function mmss(t: number): string
        if t < 10 then
            return string.format("%.1f", t)
        end
        local n = math.ceil(t)
        return string.format("%d:%02d", n // 60, n % 60)
    end

    local DRAWN = { stalemate = "stallo", fifty = "cinquanta mosse", material = "materiale insufficiente",
                    repetition = "triplice ripetizione", agreed = "d'accordo", time = "tempo scaduto",
                    race = "tutti e due i re all'ottava", forty = "quaranta mosse senza prese" }
    local WON = { hill = "Re sulla collina", checks = "Terzo scacco", race = "Re all'ottava traversa",
                  anti = "Senza più mosse", atomic = "Re saltato in aria", horde = "Orda distrutta",
                  blocked = "Avversario senza mosse", few = "Avversario ridotto a due pedine" }
    core.hud.pass("chess:status", 100, function()
        local s = if sitting then seen[sitting] else nil
        if s == nil then
            return
        end
        local st = s.st
        local line
        if st.result == "w" or st.result == "b" then
            local loser = who(other(st.result))
            line = if st.why == "resign" then `Il {loser} abbandona: vince il {who(st.result)}`
                elseif st.why == "time" then `Tempo scaduto al {loser}: vince il {who(st.result)}`
                elseif WON[st.why] then `{WON[st.why]}: vince il {who(st.result)}`
                else `Scacco matto: vince il {who(st.result)}`
        elseif st.result == "=" then
            line = `Patta: {DRAWN[st.why] or st.why}`
        else
            line = (if st.var == "std" then "" else `{VARIANT_NAME[st.var]} -- `)
                .. `Muove il {who(st.turn)}` .. (if in_check(st) then " -- scacco!" else "")
                .. (if st.var == "mulino" and st.ep == 1 then " -- mulino! togli una pedina" else "")
                .. (if st.var == "3check"
                    then ` -- scacchi dati {string.sub(st.checks, 1, 1)}-{string.sub(st.checks, 2, 2)}` else "")
                .. (if st.offer ~= "" then ` -- il {who(st.offer)} propone la patta` else "")
                .. (if string.find(s.seats, "!", 1, true) then ` -- orologio fermo, il {who(st.turn)} si è alzato`
                    else "")
        end
        -- Under the board: the top of the screen is the engine's (F3).
        core.hud.rect(0.5, 0.875, 1.2, 0.065, 0, 0, 0, 150)
        core.hud.text(0.5, 0.875, line, 0.038)
        if st.tc > 0 then
            -- Each side's clock beside the board, the one running brighter;
            -- the player's own on their left.
            local a = math.max(core.hud.aspect(), 0.1)
            for _, c in { "w", "b" } do
                local mine = c == (if playing == "b" then "b" else "w")
                local x = 0.5 + (if mine then -0.66 else 0.66) / a
                local on_move = st.result == "" and st.turn == c
                local t = left(s, c)
                local ink = if c == "w" then 20 else 235
                core.hud.rect(x, 0.5, 0.3, 0.075, if c == "w" then 230 else 30, if c == "w" then 225 else 28,
                              if c == "w" then 210 else 26, if on_move then 235 else 110)
                core.hud.text(x, 0.5, mmss(t), 0.05, 1, ink, ink, ink, 255)
                if on_move and t < 10 then
                    core.hud.rect(x, 0.543, 0.3, 0.008, 220, 50, 40, 255)
                end
            end
        end
        local you = if playing == "v" then "Guardi la partita"
            elseif may_move(s, other(playing)) then "Giochi con tutti e due"
            else `Giochi con il {who(playing)}`
        local hint = if playing == "v" then "tasto destro: alzati"
            elseif st.result ~= "" then "click: un'altra   tasto destro: variante, orologio, alzati"
            elseif st.from == 0 then "tasto destro: variante, orologio, patta, abbandono, alzati"
            else "tasto destro: patta, abbandono, alzati"
        core.hud.text(0.5, 0.94, `{you}   --   {hint}`, 0.028, 1, 230, 230, 230, 255)
    end)
end
