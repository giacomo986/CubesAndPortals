--!strict
-- villagers: the shapes its files share.

export type V = {
    key: string, hx: number, hy: number, hz: number, kind: string,
    id: number, back_at: number, inv: { [string]: number }, fed_at: number,
    co: thread?, wake: number, walking: boolean,
    tx: number, ty: number, tz: number,   -- where it was sent
    tried: { [string]: number },          -- cells it could not reach, until then
    rk: string?, hunting: number?,        -- the watch's: its village, who it is after,
    prey: number?,                        -- and what creature
    task: string?,                        -- what a gatherer is fetching (wood, stone, earth)
    climbing: boolean?,                   -- on its way up or down a mine's shaft
    uses: number?,                        -- works done with the tool in hand
    supplied: number?,                    -- when it last went to the chests for what it lacks
    bed: { number }?,                     -- a keeper's: its bed, and the sign of the building
    bed_mark: string?,                    -- it is in ("" none; see bed_mark)
    sleeping: boolean?,
}

-- Each village's box, the corners last given to core.owned_area.
export type Box = { x1: number, z1: number, x2: number, z2: number, y: number, homes: { V } }

-- What a village thinks of a player, by name: how many times caught, when
-- last, and whether its watch is after them. Kept a key per village.
export type Trust = { strikes: number, last: number, hostile: boolean }

-- A lot's frame, as mods/villages turns a building: its door on side t of
-- (0..3: -Z, -X, +Z, +X), local x along the front (w), local z inwards (d).
-- Kinds: "lot" (levelled), "house" (style, w by d, storeys), "floor" (a
-- storey on a house already built) and "garden" (a fenced plot of wheat).
export type Order = {
    kind: string, buyer: string, x0: number, z0: number, t: number, floor: number,
    stage: string, g: { number }, road: { { number } }, need: { [string]: number },
    have: { [string]: number }, since: number, asked: number,
    w: number, d: number, style: string, storeys: number,
    self: boolean?,   -- the builder told to find what it lacks itself
}

-- The houses a village built for players, for what they add to them.
export type House = { x0: number, z0: number, t: number, floor: number, w: number, d: number, style: string,
               storeys: number, garden: boolean, buyer: string }

-- ================================================================= portals
--
-- What villagers know of the portals scattered over a world without a plaza
-- (mods/gates' sites, asked with gates:ask): a worker now and then tells of
-- the nearest, vaguely -- what it looks like, which way, near or far, and
-- whether those who went came back; a caravan, of one near the village it
-- comes from; the mayor, for 2 + d rubies, exactly, and draws the way on
-- the player's map (maps:route).
export type Portal = { room: string, d: number, x: number, z: number }

export type Errand = { id: number, kind: string, giver: string, item: string, n: number, x: number, z: number,
                to: string, pay: number, posted: number, taker: string, taken: number, got: number }

export type Siege = { wave: number, next: number, lost: number, started: boolean, fought: { [string]: boolean } }

export type Feast = { x: number, y: number, z: number, put: { { number } }, fired: boolean, played: number }

export type Caravan = { from: string, to: string, x: number, y: number, z: number, goods: { [string]: number },
                 m: number, mule: number, escort: string, best: number, best_at: number, att_at: number,
                 route: { { number } }?, k: number }

return nil
