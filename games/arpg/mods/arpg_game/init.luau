-- arpg_game: point and click in the manner of isometric action RPGs. The camera looks down
-- from one fixed side (core.iso_view); the left button walks to where it is
-- pressed, strikes a creature, talks to a merchant, picks up what lies on the
-- ground; the right button uses the skill 1-4 chose.
--
-- The world is a town and three zones, laid out below (LAYOUT): areas joined by
-- corridors cut through high ground, nothing beyond them. Each area has its
-- level, and its creatures -- packs, champions, a named one now and then, a
-- lord at the end of a zone -- stand where the layout put them, wake when
-- somebody comes near and stay dead once killed, until the world is empty
-- again. The first area of each zone, the town and the crypt's door have a
-- waypoint; a scroll opens a town portal and the way back.
--
-- A character is a class (warrior, sorcerer, hunter), chosen on arrival, with
-- four skills, five points of strength, dexterity, vitality and energy and one
-- skill point a level. What they carry are items of many bases and five
-- rarities -- normal, magic, rare, unique and set -- with affixes rolled by the
-- level they dropped at; the town's smith, jeweller and alchemist buy and sell.
--
-- The SERVER decides every blow, every drop and every price. The half on the
-- player's machine sends what was clicked as the ray under the cursor; which
-- creature or thing that ray meets, how far the player is and whether the
-- mana is there are the server's own. It walks its body itself (core.walk), as
-- a player at the keys would, and the server's movement checks hold it to that.
--
-- `side = both`: the server runs the first half, a player's machine the
-- second, and a process that is its own server runs both and calls across.

local GROUND = 20     -- the floor's top block is one below this
local CHEST  = 1.0    -- where on a body a blow or a spell is aimed from and at

local side    = core.side()
local serving = side ~= "client"
local drawing = side ~= "server"

local clock = 0

-- Filled in below by whichever halves this process runs.
local heard = function(_text: string) end
local asked = function(_player: number, _text: string) end

local function tell(player: number, text: string)
    if drawing then
        heard(text)
    else
        core.net.send(player, text)
    end
end

local function words(text: string): {string}
    return string.split(text, " ")
end

-- A number from 0 to 1 for a cell or a region, the same every time for the same
-- one: what is where is a matter of the place, not of when it was made.
local function hash(x: number, z: number, salt: number): number
    local h = bit32.bxor(bit32.band(x * 73856093, 0xffffffff), bit32.band(z * 19349663, 0xffffffff),
                         bit32.band(salt * 83492791, 0xffffffff))
    h = bit32.band(h * 2654435761, 0xffffffff)
    return bit32.rshift(h, 8) / 16777216
end

-- ====================================================================== data
-- Shared by both halves: the server rolls and keeps items, a player's machine
-- reads the same codes to draw and describe them.

local D = {}

D.REACH      = 2.4     -- a blow lands from this far, feet to feet, across
D.BOW_RANGE  = 13      -- a bow's arrow is loosed from this far
D.BELT       = 8       -- potions of each kind, and scrolls, a player carries
D.BAG        = 40      -- the bag's squares, 10 by 4, one item each
D.STASH      = 40
D.MAX_LEVEL  = 40
D.MAX_RANK   = 20

-- Equipped, by square: the paper doll's ten.
D.EQ = { "weapon", "off", "head", "body", "hands", "feet", "waist", "neck", "ring", "ring" }
D.EQ_NAME = { "WEAPON", "OFF HAND", "HEAD", "BODY", "HANDS", "FEET", "WAIST", "NECK", "RING", "RING" }

-- name, colour; by rarity 0..4
D.RARITY = {
    [0] = { "NORMAL", 225, 225, 225 },
    [1] = { "MAGIC", 120, 140, 255 },
    [2] = { "RARE", 255, 225, 80 },
    [3] = { "UNIQUE", 205, 160, 90 },
    [4] = { "SET", 70, 225, 70 },
}

-- What each kind of base is: where it is worn, which affixes it takes (W
-- weapons, A armour, J jewels and orbs), the letters on its square.
D.CAT = {
    sword  = { slot = "weapon", g = "W", icon = "SW" },
    axe    = { slot = "weapon", g = "W", icon = "AX" },
    mace   = { slot = "weapon", g = "W", icon = "MC" },
    dagger = { slot = "weapon", g = "W", icon = "DG" },
    bow    = { slot = "weapon", g = "W", icon = "BW", bow = true },
    staff  = { slot = "weapon", g = "W", icon = "ST" },
    wand   = { slot = "weapon", g = "W", icon = "WD" },
    shield = { slot = "off", g = "A", icon = "SH" },
    orb    = { slot = "off", g = "J", icon = "OB" },
    helm   = { slot = "head", g = "A", icon = "HM" },
    armor  = { slot = "body", g = "A", icon = "AR" },
    gloves = { slot = "hands", g = "A", icon = "GL" },
    boots  = { slot = "feet", g = "A", icon = "BT" },
    belt   = { slot = "waist", g = "A", icon = "BL" },
    ring   = { slot = "ring", g = "J", icon = "RG" },
    amulet = { slot = "neck", g = "J", icon = "AM" },
}

-- The bases: name, kind, level, damage (weapons) or defence, speed (seconds a
-- blow takes, against 1), the class it is for if only one, what it always has.
-- Appended only: an item is saved by its base's place in this list.
local function base(name, cat, lvl, a, b, spd, cls, imp)
    return { name = name, cat = cat, lvl = lvl, min = a, max = b, def = if b == nil then a else 0,
             spd = spd or 1, cls = cls, imp = imp }
end
D.BASES = {
    base("SHORT SWORD", "sword", 1, 2, 6),
    base("LONG SWORD", "sword", 6, 4, 11),
    base("BROAD SWORD", "sword", 12, 7, 17),
    base("WAR SWORD", "sword", 18, 11, 25),
    base("HAND AXE", "axe", 1, 3, 5, 1.1),
    base("BATTLE AXE", "axe", 8, 6, 15, 1.1),
    base("GREAT AXE", "axe", 16, 12, 28, 1.15),
    base("CLUB", "mace", 1, 2, 7, 1.1),
    base("WAR HAMMER", "mace", 10, 7, 18, 1.15),
    base("DAGGER", "dagger", 1, 1, 5, 0.8),
    base("KRIS", "dagger", 10, 4, 10, 0.8),
    base("SHORT BOW", "bow", 1, 2, 5),
    base("LONG BOW", "bow", 7, 3, 10),
    base("WAR BOW", "bow", 15, 6, 17),
    base("HUNTER'S BOW", "bow", 10, 5, 12, 0.9, 3, { ias = 10 }),
    base("STAFF", "staff", 1, 2, 6, 1.2, nil, { mana = 10 }),
    base("BATTLE STAFF", "staff", 12, 6, 14, 1.2, nil, { mana = 25 }),
    base("WAND", "wand", 3, 1, 4, 1, 2, { mana = 15 }),
    base("BONE WAND", "wand", 14, 3, 7, 1, 2, { sk = 1 }),
    base("BUCKLER", "shield", 1, 4),
    base("KITE SHIELD", "shield", 7, 10),
    base("TOWER SHIELD", "shield", 14, 20, nil, 1, 1, { dr = 3 }),
    base("ORB", "orb", 3, 0, nil, 1, 2, { mana = 20 }),
    base("EYE ORB", "orb", 14, 0, nil, 1, 2, { mana = 35, mreg = 20 }),
    base("CAP", "helm", 1, 3),
    base("HELM", "helm", 6, 7),
    base("GREAT HELM", "helm", 14, 14, nil, 1, 1, { life = 10 }),
    base("HOOD", "helm", 8, 5, nil, 1, 3, { dex = 5 }),
    base("QUILTED ARMOR", "armor", 1, 8),
    base("LEATHER ARMOR", "armor", 4, 14),
    base("CHAIN MAIL", "armor", 9, 24),
    base("PLATE MAIL", "armor", 16, 40),
    base("LEATHER GLOVES", "gloves", 1, 2),
    base("CHAIN GLOVES", "gloves", 8, 4),
    base("GAUNTLETS", "gloves", 15, 7),
    base("BOOTS", "boots", 1, 2),
    base("CHAIN BOOTS", "boots", 8, 4),
    base("GREAVES", "boots", 15, 7),
    base("SASH", "belt", 1, 2),
    base("BELT", "belt", 8, 4),
    base("HEAVY BELT", "belt", 15, 7),
    base("RING", "ring", 1, 0),
    base("AMULET", "amulet", 1, 0),
}
D.BASE_OF = {}
for i, b in D.BASES do
    D.BASE_OF[b.name] = i
end

-- What an equipped item is drawn as on the body, for everybody (core.player_wear,
-- slot k of the paper doll): arpg_core's "gear_<its base>", nothing for a ring;
-- its trim gold on a unique and green on a set (dyes 5 and 6).
D.GEAR_DYE = { [3] = 5, [4] = 6 }
function D.wear(p: number, k: number, it)
    local model = 0
    if it and D.BASES[it.b] then
        local file = (string.gsub(string.gsub(string.lower(D.BASES[it.b].name), "'", ""), " ", "_"))
        model = core.model_id("arpg_core:gear_" .. file)
    end
    core.player_wear(p, k, model, if model ~= 0 then D.GEAR_DYE[it.r] or 0 else 0)
end

-- The clip a body plays for a blow, a shot or a spell (core.player_clip, the
-- robot's), and how long it lasts.
D.CLIP = { sword_slash = 0.8, sword_overhead = 0.87, dagger_stab = 0.42, spin_slash = 0.54, war_cry = 0.83,
           jump = 0.6, bow_shoot = 0.71, staff_cast = 0.54, cast_nova = 0.67 }

-- How a stat reads on an item, and on the character sheet.
D.STAT = {
    ed = "+%d%% ENHANCED DAMAGE", maxd = "+%d TO MAXIMUM DAMAGE", edef = "+%d%% ENHANCED DEFENSE",
    def = "+%d DEFENSE", life = "+%d TO LIFE", mana = "+%d TO MANA", str = "+%d TO STRENGTH",
    dex = "+%d TO DEXTERITY", vit = "+%d TO VITALITY", ene = "+%d TO ENERGY",
    ias = "+%d%% ATTACK SPEED", frw = "+%d%% FASTER RUN", ls = "%d%% LIFE STOLEN PER HIT",
    ms = "%d%% MANA STOLEN PER HIT", lreg = "REPLENISH %d LIFE A SECOND", mreg = "+%d%% MANA REGENERATION",
    mf = "+%d%% BETTER CHANCE OF MAGIC ITEMS", gf = "+%d%% EXTRA GOLD", dr = "DAMAGE REDUCED BY %d%%",
    sk = "+%d TO ALL SKILLS", fire = "ADDS %d FIRE DAMAGE", crit = "%d%% CHANCE OF DOUBLE DAMAGE",
}
-- The order an item lists them in.
D.STAT_ORDER = { "sk", "ed", "maxd", "fire", "ias", "crit", "edef", "def", "life", "mana", "str", "dex", "vit",
                 "ene", "frw", "ls", "ms", "lreg", "mreg", "dr", "mf", "gf" }

-- The affixes: a prefix or not, its word, the stat, the level it first rolls
-- at, its range, the kinds it goes on (W A J), and a kind it is only for.
-- Appended only: an item is saved by an affix's place in this list.
local function affix(pre, name, stat, lvl, a, b, g, only)
    return { pre = pre, name = name, stat = stat, lvl = lvl, min = a, max = b, g = g, only = only }
end
D.AFFIX = {
    affix(true, "SHARP", "ed", 1, 10, 20, "W"),
    affix(true, "FINE", "ed", 5, 21, 35, "W"),
    affix(true, "CRUEL", "ed", 12, 36, 60, "W"),
    affix(true, "MERCILESS", "ed", 18, 61, 90, "W"),
    affix(true, "JAGGED", "maxd", 1, 2, 4, "W"),
    affix(true, "VICIOUS", "maxd", 10, 5, 9, "W"),
    affix(true, "STURDY", "edef", 1, 10, 20, "A"),
    affix(true, "STRONG", "edef", 6, 21, 35, "A"),
    affix(true, "GLORIOUS", "edef", 12, 36, 60, "A"),
    affix(true, "BLESSED", "edef", 18, 61, 90, "A"),
    affix(true, "LIZARD'S", "mana", 1, 5, 10, "AJ"),
    affix(true, "SNAKE'S", "mana", 6, 11, 20, "AJ"),
    affix(true, "SERPENT'S", "mana", 12, 21, 35, "J"),
    affix(true, "BURNING", "fire", 4, 2, 6, "WJ"),
    affix(true, "BLAZING", "fire", 12, 7, 14, "W"),
    affix(true, "ARCANE", "sk", 10, 1, 1, "J"),
    affix(true, "ARCHON", "sk", 18, 2, 2, "J", "amulet"),
    affix(true, "FORTUNATE", "mf", 3, 5, 15, "AJ"),
    affix(true, "LUCKY", "mf", 12, 16, 30, "J"),
    affix(true, "WARDED", "dr", 8, 3, 6, "A"),
    affix(true, "BULWARK", "dr", 16, 7, 12, "A"),
    affix(true, "BLOODY", "ls", 5, 3, 5, "WJ"),
    affix(true, "VAMPIRIC", "ls", 14, 6, 9, "W"),
    affix(false, "OF STRENGTH", "str", 1, 2, 5, "WAJ"),
    affix(false, "OF MIGHT", "str", 8, 6, 10, "WAJ"),
    affix(false, "OF THE TITAN", "str", 16, 11, 18, "AJ"),
    affix(false, "OF DEXTERITY", "dex", 1, 2, 5, "WAJ"),
    affix(false, "OF SKILL", "dex", 8, 6, 10, "WAJ"),
    affix(false, "OF PRECISION", "dex", 16, 11, 18, "AJ"),
    affix(false, "OF VIGOR", "vit", 1, 2, 5, "AJ"),
    affix(false, "OF THE BEAR", "vit", 8, 6, 10, "AJ"),
    affix(false, "OF THE MAMMOTH", "vit", 16, 11, 18, "AJ"),
    affix(false, "OF ENERGY", "ene", 1, 2, 5, "WJ"),
    affix(false, "OF THE MIND", "ene", 8, 6, 10, "WJ"),
    affix(false, "OF WIZARDRY", "ene", 16, 11, 18, "J"),
    affix(false, "OF THE JACKAL", "life", 1, 5, 10, "AJ"),
    affix(false, "OF THE FOX", "life", 6, 11, 20, "AJ"),
    affix(false, "OF THE WOLF", "life", 12, 21, 35, "AJ"),
    affix(false, "OF THE TIGER", "life", 18, 36, 55, "A"),
    affix(false, "OF HASTE", "ias", 3, 10, 15, "WJ"),
    affix(false, "OF SWIFTNESS", "ias", 12, 16, 25, "W"),
    affix(false, "OF PACING", "frw", 1, 5, 10, "A", "boots"),
    affix(false, "OF RUNNING", "frw", 8, 11, 20, "A", "boots"),
    affix(false, "OF THE WIND", "frw", 15, 21, 30, "A", "boots"),
    affix(false, "OF REGENERATION", "lreg", 4, 1, 2, "AJ"),
    affix(false, "OF RENEWAL", "lreg", 14, 3, 5, "AJ"),
    affix(false, "OF THE SPRING", "mreg", 1, 10, 25, "J"),
    affix(false, "OF FOCUS", "mreg", 10, 26, 50, "J"),
    affix(false, "OF THE LEECH", "ms", 6, 2, 4, "WJ"),
    affix(false, "OF GREED", "gf", 1, 15, 40, "AJ"),
    affix(false, "OF RUIN", "crit", 8, 3, 6, "W"),
    affix(false, "OF SLAUGHTER", "crit", 16, 7, 12, "W"),
    affix(false, "OF WARDING", "def", 1, 3, 8, "A"),
}

-- Two words a rare item is called by: x = first * 16 + second.
D.RARE_A = { "GRIM", "DOOM", "BLOOD", "BONE", "STORM", "SHADOW", "GHOUL", "RUNE", "WRAITH", "HATE", "PAIN",
             "DREAD", "GALE", "VIPER", "RAVEN", "ASH" }
D.RARE_B = { "BITE", "EDGE", "SONG", "MARK", "SHELL", "WARD", "GRASP", "STRIDE", "COIL", "EYE", "LOOP",
             "SPIRE", "BRAND", "FANG", "VEIL", "KNOT" }

-- The uniques: a name, its base, the level it drops from, what it has.
D.UNIQUE = {
    { name = "THE GRAVECUTTER", base = "LONG SWORD", lvl = 7, s = { ed = 120, ls = 6, str = 10 } },
    { name = "EMBERWAND", base = "WAND", lvl = 5, s = { sk = 2, fire = 6, mana = 30 } },
    { name = "WHISPERWIND", base = "LONG BOW", lvl = 8, s = { ed = 100, ias = 20, dex = 15 } },
    { name = "BONEHELM", base = "HELM", lvl = 6, s = { edef = 80, life = 30, dr = 8 } },
    { name = "ASHEN SHROUD", base = "LEATHER ARMOR", lvl = 5, s = { edef = 120, frw = 15, mana = 20 } },
    { name = "GRIP OF THE DEAD", base = "CHAIN GLOVES", lvl = 9, s = { ls = 5, ias = 15, edef = 60 } },
    { name = "WANDERER'S STRIDE", base = "BOOTS", lvl = 3, s = { frw = 30, dex = 8, edef = 50 } },
    { name = "CIRCLE OF EMBERS", base = "RING", lvl = 6, s = { fire = 8, mf = 20, mana = 15 } },
    { name = "EYE OF THE CRYPT", base = "AMULET", lvl = 10, s = { sk = 1, life = 20, mana = 20 } },
    { name = "WALL OF AGES", base = "TOWER SHIELD", lvl = 14, s = { edef = 100, dr = 15, vit = 10 } },
    { name = "THE BUTCHER'S BILL", base = "BATTLE AXE", lvl = 10, s = { ed = 150, crit = 10, life = 20 } },
    { name = "STARFALL", base = "BATTLE STAFF", lvl = 12, s = { sk = 2, ene = 20, mreg = 50 } },
}

-- The sets, one for each class: three pieces, and what two and three of them
-- give besides their own. x = set * 10 + piece.
D.SET = {
    { name = "IRONWARD", items = {
        { name = "IRONWARD VISAGE", base = "GREAT HELM", s = { edef = 60, life = 20 } },
        { name = "IRONWARD HAUBERK", base = "PLATE MAIL", s = { edef = 70, str = 10 } },
        { name = "IRONWARD BASTION", base = "TOWER SHIELD", s = { edef = 50, dr = 5 } } },
      two = { life = 40 }, three = { dr = 10, sk = 1 } },
    { name = "EMBER CIRCLE", items = {
        { name = "EMBER CIRCLE ROD", base = "BONE WAND", s = { fire = 6, mana = 25 } },
        { name = "EMBER CIRCLE ORB", base = "EYE ORB", s = { ene = 15, mreg = 30 } },
        { name = "EMBER CIRCLE TORC", base = "AMULET", s = { life = 25, mana = 25 } } },
      two = { mana = 50 }, three = { sk = 2, mreg = 50 } },
    { name = "STALKER'S GARB", items = {
        { name = "STALKER'S RECURVE", base = "LONG BOW", s = { ed = 70, dex = 10 } },
        { name = "STALKER'S HIDE", base = "LEATHER ARMOR", s = { edef = 80, life = 20 } },
        { name = "STALKER'S TREADS", base = "BOOTS", s = { frw = 20, dex = 8 } } },
      two = { frw = 15 }, three = { ed = 60, ias = 20 } },
}

-- The classes: what they start with, how life and mana grow, their skills.
D.CLASS = {
    { key = "warrior", name = "WARRIOR", str = 25, dex = 20, vit = 25, ene = 10,
      life = 30, lpl = 2, lpv = 1.4, mana = 8, mpl = 1, mpe = 1,
      gear = { "SHORT SWORD", "BUCKLER" },
      text = "STEEL AND STRENGTH. QUICK BLOWS THAT CUT THROUGH A CROWD.",
      skills = {
          { name = "BASH", lvl = 1, cost = 3, text = "A BLOW FOR 150% OF WEAPON DAMAGE, +15% A RANK" },
          { name = "CLEAVE", lvl = 3, cost = 6, text = "HITS ALL AROUND FOR 80% WEAPON DAMAGE, +12% A RANK" },
          { name = "WAR CRY", lvl = 6, cost = 9, text = "HURTS AND SLOWS ALL WITHIN 7, 60% +10% A RANK" },
          { name = "LEAP", lvl = 10, cost = 10, text = "LEAPS UP TO 12 AND STRIKES WHERE IT LANDS" },
      } },
    { key = "sorcerer", name = "SORCERER", str = 10, dex = 20, vit = 15, ene = 35,
      life = 24, lpl = 1, lpv = 1.2, mana = 20, mpl = 2, mpe = 1.6,
      gear = { "STAFF" },
      text = "FIRE, FROST AND LIGHTNING. FRAIL, BUT DEADLY FROM AFAR.",
      skills = {
          { name = "FIREBALL", lvl = 1, cost = 7, text = "BURSTS FOR 4-8 FIRE AND HALF THE STAFF, +20% A RANK" },
          { name = "FROST NOVA", lvl = 3, cost = 9, text = "A RING OF ICE: 6-10 COLD, SLOWS, +30% A RANK" },
          { name = "CHAIN LIGHTNING", lvl = 6, cost = 8, text = "4-24 LIGHTNING, LEAPS TO FIVE MORE" },
          { name = "TELEPORT", lvl = 10, cost = 12, text = "TO WHERE THE CURSOR IS, UP TO 25" },
      } },
    { key = "hunter", name = "HUNTER", str = 15, dex = 30, vit = 20, ene = 15,
      life = 28, lpl = 1.5, lpv = 1.3, mana = 12, mpl = 1.5, mpe = 1.2,
      gear = { "SHORT BOW" },
      text = "BOW AND SPEED. KILLS BEFORE IT IS REACHED.",
      skills = {
          { name = "POWER SHOT", lvl = 1, cost = 3, text = "AN ARROW THAT PIERCES: 150% +20% A RANK" },
          { name = "MULTISHOT", lvl = 3, cost = 6, text = "A FAN OF ARROWS FOR 75% WEAPON DAMAGE" },
          { name = "EXPLOSIVE ARROW", lvl = 6, cost = 7, text = "BURSTS WHERE IT HITS: 100% +20% A RANK" },
          { name = "EVADE", lvl = 10, cost = 6, text = "A DASH, UNTOUCHABLE FOR A MOMENT" },
      } },
}

-- Life and mana a player's level and points make.
function D.next_xp(level: number): number
    return math.floor(80 * level ^ 1.6)
end

-- What creatures are: their life at a zone of the first level, the experience
-- they give, and their names as the zones deepen.
D.MOB = {
    ["arpg_core:zombie"]     = { hp = 15, xp = 10, names = { "ZOMBIE", "GHOUL", "PLAGUE WALKER", "GRAVE HORROR" } },
    ["arpg_core:skeleton"]   = { hp = 11, xp = 9, names = { "SKELETON", "BONE ARCHER", "BONE WARDEN", "DEATH ARCHER" } },
    ["arpg_core:imp"]        = { hp = 13, xp = 12, names = { "IMP", "FIRE IMP", "FLAME FIEND", "HELL IMP" } },
    ["arpg_core:grave_lord"] = { hp = 90, xp = 50, boss = "THE GRAVE LORD" },
    ["arpg_core:ember_wing"] = { hp = 96, xp = 60, boss = "EMBERWING" },
    ["arpg_crypt:custode"]   = { hp = 200, xp = 80, boss = "IL CUSTODE DELLE TOMBE" },
    -- What is broken for what is inside: no experience, a creature's drops.
    ["arpg_core:barrel"]     = { hp = 5, xp = 0, names = { "BARREL", "BARREL", "BARREL", "BARREL" }, prop = true },
    ["arpg_core:crate"]      = { hp = 6, xp = 0, names = { "CRATE", "CRATE", "CRATE", "CRATE" }, prop = true },
    ["arpg_core:urn"]        = { hp = 3, xp = 0, names = { "URN", "URN", "URN", "URN" }, prop = true },
}
D.UNITS = 1000   -- what every one of them has in the engine (arpg_core)
-- normal, champion, a named one, a lord
D.RANK = { [0] = { hp = 1, xp = 1 }, [1] = { hp = 3, xp = 3 }, [2] = { hp = 5, xp = 6 }, [3] = { hp = 1, xp = 1 } }
function D.hp_mult(level: number): number
    return 1 + (level - 1) * 0.45 + (level - 1) ^ 2 * 0.02
end
function D.dmg_mult(level: number): number
    return 0.6 + (level - 1) * 0.28
end
-- How much longer than their kind's a creature waits between blows: twice as
-- long in the first area, as their kind from the sixth on.
function D.blow_every(level: number): number
    return math.max(1, 2.2 - 0.2 * level)
end
-- The warrior's blows come this much sooner than anybody else's.
D.WARRIOR_PACE = 0.7
-- and land on whatever stands within this of the one struck, for this share.
D.CLEAVE_R, D.CLEAVE_SHARE = 1.8, 0.5
function D.tier(level: number): number
    return math.clamp(1 + (level - 1) // 3, 1, 4)
end
D.NAME_A = { "BONE", "ROT", "GRAVE", "ASH", "BLIGHT", "GORE", "DOOM", "GRIM", "HUSK", "MIRE" }
D.NAME_B = { "GRINDER", "MAW", "FANG", "SPINE", "HOWL", "REND", "SHROUD", "WAIL", "GNAW", "CLAW" }

-- ===================================================================== items
-- An item is { b = base, r = rarity, l = level, x = unique, set or rare name,
-- a = { {affix, value}, ... } }, and on the wire and in storage a code:
-- "b.r.l.x.i=v;i=v".

local IT = {}

function IT.encode(it): string
    local parts = {}
    for _, av in it.a do
        table.insert(parts, `{av[1]}={av[2]}`)
    end
    return `{it.b}.{it.r}.{it.l}.{it.x}.{table.concat(parts, ";")}`
end

function IT.decode(code: string)
    local p = string.split(code, ".")
    if #p < 4 then
        return nil
    end
    local it = { b = tonumber(p[1]) or 0, r = tonumber(p[2]) or 0, l = tonumber(p[3]) or 1, x = tonumber(p[4]) or 0,
                 a = {} }
    if D.BASES[it.b] == nil then
        return nil
    end
    if p[5] ~= nil and p[5] ~= "" then
        for _, kv in string.split(p[5], ";") do
            local q = string.split(kv, "=")
            local i, v = tonumber(q[1]), tonumber(q[2])
            if i ~= nil and v ~= nil and D.AFFIX[i] ~= nil then
                table.insert(it.a, { i, v })
            end
        end
    end
    return it
end

-- The unique or set piece an item is, or nil.
function IT.special(it)
    if it.r == 3 then
        return D.UNIQUE[it.x]
    elseif it.r == 4 then
        local set = D.SET[it.x // 10]
        return set and set.items[it.x % 10]
    end
    return nil
end

-- What it adds, by stat, besides its damage and defence.
function IT.stats(it): {[string]: number}
    local out = {}
    local b = D.BASES[it.b]
    for k, v in (b.imp or {}) do
        out[k] = (out[k] or 0) + v
    end
    local sp = IT.special(it)
    if sp then
        for k, v in sp.s do
            out[k] = (out[k] or 0) + v
        end
    end
    for _, av in it.a do
        local st = D.AFFIX[av[1]].stat
        out[st] = (out[st] or 0) + av[2]
    end
    return out
end

-- Its damage or defence with what it has.
function IT.base_numbers(it): (number, number, number)
    local b = D.BASES[it.b]
    local s = IT.stats(it)
    local ed = (s.ed or 0) / 100
    local dmin = math.floor(b.min * (1 + ed))
    local dmax = math.floor((b.max or 0) * (1 + ed)) + (s.maxd or 0)
    local def = math.floor(b.def * (1 + (s.edef or 0) / 100)) + (s.def or 0)
    return dmin, math.max(dmin, dmax), def
end

function IT.name(it): string
    local b = D.BASES[it.b]
    local sp = IT.special(it)
    if sp then
        return sp.name
    elseif it.r == 2 then
        return `{D.RARE_A[it.x // 16 + 1] or "GRIM"} {D.RARE_B[it.x % 16 + 1] or "BITE"}`
    elseif it.r == 1 then
        local pre, suf = "", ""
        for _, av in it.a do
            local a = D.AFFIX[av[1]]
            if a.pre then
                pre = a.name .. " "
            else
                suf = " " .. a.name
            end
        end
        return pre .. b.name .. suf
    end
    return b.name
end

function IT.req(it): number
    local b = D.BASES[it.b]
    local r = b.lvl
    local sp = IT.special(it)
    if it.r == 3 and sp then
        r = math.max(r, sp.lvl)
    end
    for _, av in it.a do
        r = math.max(r, D.AFFIX[av[1]].lvl - 2)
    end
    return math.max(1, r)
end

-- What a merchant asks for it; a quarter of it is what they pay.
function IT.value(it): number
    local b = D.BASES[it.b]
    local v = (10 + b.lvl * 6) * ({ 1, 3, 6, 15, 12 })[it.r + 1]
    for _, av in it.a do
        v += D.AFFIX[av[1]].lvl * 8
    end
    return math.floor(v)
end

-- Its lines as a tooltip shows them: text and colour.
function IT.lines(it): { { any } }
    local b = D.BASES[it.b]
    local rc = D.RARITY[it.r]
    local out = { { IT.name(it), rc[2], rc[3], rc[4] } }
    if it.r >= 2 then
        table.insert(out, { b.name, rc[2], rc[3], rc[4] })
    end
    local dmin, dmax, def = IT.base_numbers(it)
    if D.CAT[b.cat].slot == "weapon" then
        table.insert(out, { `DAMAGE: {dmin} TO {dmax}`, 255, 255, 255 })
        table.insert(out, { string.format("SPEED: %.2f", b.spd), 200, 200, 200 })
    elseif def > 0 then
        table.insert(out, { `DEFENSE: {def}`, 255, 255, 255 })
    end
    table.insert(out, { `REQUIRED LEVEL: {IT.req(it)}`, 200, 200, 200 })
    if b.cls then
        table.insert(out, { `{D.CLASS[b.cls].name} ONLY`, 200, 200, 200 })
    end
    local s = IT.stats(it)
    for _, k in D.STAT_ORDER do
        local v = s[k]
        if v ~= nil and v ~= 0 then
            table.insert(out, { string.format(D.STAT[k], v), 120, 140, 255 })
        end
    end
    if it.r == 4 then
        local set = D.SET[it.x // 10]
        table.insert(out, { set.name, 70, 225, 70 })
        for k, v in set.two do
            table.insert(out, { "2 PIECES: " .. string.format(D.STAT[k], v), 60, 180, 60 })
        end
        for k, v in set.three do
            table.insert(out, { "3 PIECES: " .. string.format(D.STAT[k], v), 60, 180, 60 })
        end
    end
    return out
end

-- What lies on the ground and is not an item: gold, the belt's potions and
-- scrolls, the globes taken by walking over them.
function IT.loose(code: string): (string?, number)
    local c = string.sub(code, 1, 1)
    if c == "$" then
        return "gold", tonumber(string.sub(code, 2)) or 0
    elseif c == "!" then
        return ({ l = "lpot", m = "mpot", t = "tp" })[string.sub(code, 2, 2)], 1
    elseif c == "@" then
        return if string.sub(code, 2, 2) == "l" then "life" else "mana", 1
    end
    return nil, 0
end
D.LOOSE_NAME = { lpot = "LIFE POTION", mpot = "MANA POTION", tp = "TOWN PORTAL SCROLL" }

-- ==================================================================== layout
-- Cells of a grid G wide round the town at OX, OZ. Each area sits in one
-- cell, a main room and a bulge or two, and a corridor five wide runs from its
-- middle to the middle of the area it is reached from. Everything else is
-- high ground two nodes up, which nobody walks on.

local L = {}
L.OX, L.OZ, L.G = 1024, 1024, 48
-- Where arpg_crypt draws the crypt (its DX, DZ): the ground over it is open.
L.CRYPT_DX, L.CRYPT_DZ = 1317, 1013
L.ZONE = { [0] = "ASHBURY", "THE GREY WASTE", "THE BARROWS", "THE CRYPT" }

L.AREAS = {
    { key = "town", i = 0, j = 0, zone = 0, level = 1, kind = "town" },
    { key = "a1", i = 1, j = 0, zone = 1, level = 1, from = "town", waypoint = true },
    { key = "a2", i = 2, j = 0, zone = 1, level = 2, from = "a1" },
    { key = "b1", i = 2, j = 1, zone = 1, level = 3, from = "a2", kind = "branch" },
    { key = "a3", i = 2, j = -1, zone = 1, level = 3, from = "a2" },
    { key = "a4", i = 3, j = -1, zone = 1, level = 4, from = "a3" },
    { key = "b2", i = 3, j = -2, zone = 1, level = 5, from = "a4", kind = "branch" },
    { key = "a5", i = 4, j = -1, zone = 1, level = 5, from = "a4", kind = "lair", boss = "arpg_core:grave_lord",
      minion = "arpg_core:zombie" },
    { key = "a6", i = 5, j = -1, zone = 2, level = 6, from = "a5", waypoint = true },
    { key = "a7", i = 5, j = 0, zone = 2, level = 6, from = "a6" },
    { key = "a8", i = 6, j = 0, zone = 2, level = 7, from = "a7" },
    { key = "b3", i = 6, j = -1, zone = 2, level = 8, from = "a8", kind = "branch" },
    { key = "a9", i = 7, j = 0, zone = 2, level = 8, from = "a8" },
    { key = "b4", i = 8, j = 0, zone = 2, level = 9, from = "a9", kind = "branch" },
    { key = "a10", i = 7, j = 1, zone = 2, level = 9, from = "a9", kind = "lair", boss = "arpg_core:ember_wing",
      minion = "arpg_core:imp" },
    { key = "crypt", i = 8, j = 1, zone = 3, level = 10, from = "a10", kind = "crypt", waypoint = true },
}
L.BY_KEY = {}
L.RECTS = {}      -- every open rectangle: { x0, z0, x1, z1, area }
L.BUCKET = {}     -- cell -> the rectangles that touch it

local function cell_key(i: number, j: number): number
    return (i + 100) * 1000 + (j + 100)
end

local function add_rect(x0: number, z0: number, x1: number, z1: number, area: number)
    local r = { math.min(x0, x1), math.min(z0, z1), math.max(x0, x1), math.max(z0, z1), area }
    table.insert(L.RECTS, r)
    local g = L.G
    for i = (r[1] - L.OX + g // 2) // g, (r[3] - L.OX + g // 2) // g do
        for j = (r[2] - L.OZ + g // 2) // g, (r[4] - L.OZ + g // 2) // g do
            local k = cell_key(i, j)
            L.BUCKET[k] = L.BUCKET[k] or {}
            table.insert(L.BUCKET[k], r)
        end
    end
end

for n, a in L.AREAS do
    L.BY_KEY[a.key] = n
    a.cx, a.cz = L.OX + a.i * L.G, L.OZ + a.j * L.G
    local h1, h2 = hash(a.i, a.j, 31), hash(a.i, a.j, 32)
    local hw, hd
    if a.kind == "town" then
        hw, hd = 20, 20
    elseif a.kind == "branch" then
        hw, hd = 9 + math.floor(h1 * 4), 9 + math.floor(h2 * 4)
    elseif a.kind == "lair" then
        hw, hd = 17 + math.floor(h1 * 3), 17 + math.floor(h2 * 3)
    else
        hw, hd = 13 + math.floor(h1 * 6), 13 + math.floor(h2 * 6)
    end
    a.hw, a.hd = hw, hd
    if a.kind == "crypt" then
        -- The ground over arpg_crypt's floors, from just before its
        -- mausoleum to past the third floor's east wall.
        local x0, z0 = L.CRYPT_DX + 71, L.CRYPT_DZ + 43
        add_rect(x0, z0, L.CRYPT_DX + 167, L.CRYPT_DZ + 76, n)
        a.cx, a.cz = L.CRYPT_DX + 74, L.CRYPT_DZ + 59
        a.hw, a.hd = 3, 15
    else
        add_rect(a.cx - hw, a.cz - hd, a.cx + hw, a.cz + hd, n)
        if a.kind ~= "town" then
            -- A bulge or two, so a room is not a box.
            for k = 1, 1 + math.floor(hash(a.i, a.j, 33) * 2) do
                local along = hash(a.i, a.j, 34 + k) < 0.5
                local s = if hash(a.i, a.j, 40 + k) < 0.5 then -1 else 1
                local bw, bd = 5 + math.floor(hash(a.i, a.j, 45 + k) * 4), 5 + math.floor(hash(a.i, a.j, 50 + k) * 4)
                local bx = if along then a.cx + s * (hw - 1) else a.cx + math.floor((hash(a.i, a.j, 55 + k) - 0.5) * hw)
                local bz = if along then a.cz + math.floor((hash(a.i, a.j, 60 + k) - 0.5) * hd) else a.cz + s * (hd - 1)
                add_rect(math.max(bx - bw, a.cx - 21), math.max(bz - bd, a.cz - 21), math.min(bx + bw, a.cx + 21),
                         math.min(bz + bd, a.cz + 21), n)
            end
        end
    end
end
-- The cell of the town's arch (opening 0), in the high ground of its north edge.
add_rect(L.OX, L.OZ - 21, L.OX, L.OZ - 21, 1)
-- The corridors: from each area's middle to the middle of the one before it.
for n, a in L.AREAS do
    if a.from then
        local f = L.AREAS[L.BY_KEY[a.from]]
        if f.j == a.j then
            add_rect(math.min(f.cx, a.cx), a.cz - 2, math.max(f.cx, a.cx), a.cz + 2, n)
        else
            add_rect(a.cx - 2, math.min(f.cz, a.cz), a.cx + 2, math.max(f.cz, a.cz), n)
        end
    end
end

-- The area a column is in, or 0 for the high ground.
function L.at(x: number, z: number): number
    x, z = math.floor(x), math.floor(z)
    local list = L.BUCKET[cell_key((x - L.OX + L.G // 2) // L.G, (z - L.OZ + L.G // 2) // L.G)]
    if list then
        for _, r in list do
            if x >= r[1] and x <= r[3] and z >= r[2] and z <= r[4] then
                return r[5]
            end
        end
    end
    return 0
end

-- A place's level: its area's, and down in the crypt the floor's.
function L.level_at(x: number, y: number, z: number): number
    local n = L.at(x, z)
    local a = L.AREAS[n]
    if a == nil then
        return 1
    end
    if a.kind == "crypt" and y < GROUND - 1 then
        return if x >= L.CRYPT_DX + 138 then 12 elseif x >= L.CRYPT_DX + 110 then 11 else 10
    end
    return a.level
end

-- The town's places, around its middle.
L.TX, L.TZ = L.OX, L.OZ
L.SPAWN = { L.TX + 0.5, GROUND, L.TZ + 4.5 }
L.NPCS = {
    { kind = "smith", name = "THE SMITH", x = L.TX - 12, z = L.TZ - 9 },
    { kind = "alchemist", name = "THE ALCHEMIST", x = L.TX + 12, z = L.TZ - 9 },
    { kind = "jeweler", name = "THE JEWELLER", x = L.TX - 12, z = L.TZ + 9 },
    { kind = "guard", name = "THE GATE WARDEN", x = L.TX + 17, z = L.TZ + 4 },
}
L.STASH = { L.TX + 5, L.TZ + 7 }
L.TP_SPOTS = {}
for k = 0, 7 do
    table.insert(L.TP_SPOTS, { L.TX - 7 + 2 * k + 0.5, GROUND, L.TZ + 12.5 })
end

-- The waypoints: the town's by its gate, and the first area of each zone's.
L.WPS = { { name = "ASHBURY", x = L.TX + 13, z = L.TZ + 6 } }
for _, a in L.AREAS do
    if a.waypoint then
        local f = L.AREAS[L.BY_KEY[a.from]]
        -- Between the middle and the way in, so it is the first thing met.
        local dx, dz = math.sign(f.cx - a.cx), math.sign(f.cz - a.cz)
        local x, z = a.cx + dx * 6 + (if dx == 0 then 4 else 0), a.cz + dz * 6 + (if dz == 0 then 4 else 0)
        if a.kind == "crypt" then
            x, z = L.CRYPT_DX + 73, L.CRYPT_DZ + 50
        end
        table.insert(L.WPS, { name = L.ZONE[a.zone], x = x, z = z, area = a.key })
    end
end

-- The encounters: packs where each area's hash put them, and in a lair its
-- lord. Each is { area, x, z, members = { {kind, rank} }, name }.
L.ENC = {}
L.CHESTS = {}
L.PROPS = {}      -- what is broken for what is inside: { x, z, area, kind }
local KINDS = { [1] = { "zombie", "zombie", "skeleton" }, [2] = { "zombie", "skeleton", "imp", "imp" } }
for n, a in L.AREAS do
    if a.kind == "town" or a.kind == "crypt" then
        continue
    end
    local spots = {}
    local function free_spot(salt: number): (number, number)
        for t = 0, 30 do
            local x = a.cx - a.hw + 4 + math.floor(hash(n, t, salt) * (2 * a.hw - 7))
            local z = a.cz - a.hd + 4 + math.floor(hash(t, n, salt + 1) * (2 * a.hd - 7))
            local ok = true
            for _, s in spots do
                if (s[1] - x) ^ 2 + (s[2] - z) ^ 2 < 64 then
                    ok = false
                end
            end
            for _, w in L.WPS do
                if (w.x - x) ^ 2 + (w.z - z) ^ 2 < 100 then
                    ok = false
                end
            end
            if ok then
                table.insert(spots, { x, z })
                return x, z
            end
        end
        return a.cx, a.cz
    end
    -- Fewer and smaller packs where a character is new.
    local groups = if a.kind == "branch" or a.kind == "lair" then 2 else
                   math.min(4, 1 + (a.level + 1) // 2 + math.floor(hash(n, 1, 70) * 2))
    for g = 1, groups do
        local kinds = KINDS[a.zone]
        local kind = "arpg_core:" .. kinds[1 + math.floor(hash(n, g, 71) * #kinds)]
        local roll = hash(n, g, 72)
        local e = { area = n, members = {} }
        e.x, e.z = free_spot(80 + g * 3)
        if (a.kind == "branch" and g == 1) or (roll < 0.15 and a.kind ~= "lair") then
            for _ = 1, 2 + a.level // 4 do
                table.insert(e.members, { kind, 1 })
            end
        elseif roll < 0.25 then
            e.name = D.NAME_A[1 + math.floor(hash(n, g, 73) * #D.NAME_A)] ..
                     string.lower(D.NAME_B[1 + math.floor(hash(n, g, 74) * #D.NAME_B)])
            table.insert(e.members, { kind, 2 })
            for _ = 1, 3 do
                table.insert(e.members, { kind, 0 })
            end
        else
            for _ = 1, 2 + math.floor(hash(n, g, 75) * 2) + a.level // 3 do
                table.insert(e.members, { kind, 0 })
            end
        end
        table.insert(L.ENC, e)
    end
    if a.kind == "lair" then
        local e = { area = n, x = a.cx, z = a.cz, members = { { a.boss, 3 } }, name = D.MOB[a.boss].boss }
        for _ = 1, 4 do
            table.insert(e.members, { a.minion, 0 })
        end
        table.insert(L.ENC, e)
    end
    if a.kind ~= "field" and a.kind ~= nil then
        local x, z = free_spot(90)
        table.insert(L.CHESTS, { x = x, z = z, level = a.level, area = n })
    end
    -- Things to break, a few an area: barrels and crates, and in the Barrows urns.
    local kinds = if a.zone == 1 then { "barrel", "barrel", "crate" } else { "barrel", "crate", "urn", "urn" }
    for j = 1, 3 + n % 3 do
        local x, z = free_spot(110 + j)
        table.insert(L.PROPS, { x = x, z = z, area = n,
                                kind = "arpg_core:" .. kinds[1 + math.floor(hash(n, j, 120) * #kinds)] })
    end
end

-- ==================================================================== server

if serving then
    core.net.open()

    -- Opening 0, where a player from the sandbox comes in: an arch in the
    -- town's north edge, facing into it (+Z); the server puts them four in front.
    core.set_portal(0, L.TX + 0.5, GROUND + 1, L.TZ - 21 + 0.5)

    local SV = {}
    SV.players = {}   -- player id -> state
    SV.mobs = {}      -- creature id -> what this file knows of it
    SV.killed = {}    -- creature id -> true, struck down by a player
    SV.npc = {}       -- creature id -> the town's person it is
    SV.ground = {}    -- id -> { code, x, y, z }
    SV.proj = {}      -- in flight
    SV.portals = {}   -- player id -> their town portal
    SV.enc = {}       -- encounter -> its state
    SV.chest_open = {}
    SV.props = {}     -- a breakable thing standing, by id -> its place in L.PROPS
    SV.prop_st = {}   -- by place: { id = while it stands, broken = once it is }
    SV.next_id = 1
    SV.was_anybody = false

    local B = {}      -- block ids, read once the world runs
    local function blocks()
        if B.grass then
            return
        end
        for _, n in { "grass", "moss", "dirt", "rock", "cobble", "mossy", "grave", "brazier", "waystone", "planks",
                      "awning", "cliff", "highland", "flags", "roof", "stash", "coffer", "coffer_open" } do
            B[n] = core.node_id("arpg_core:" .. n)
        end
    end

    -- ------------------------------------------------------------- the ground

    -- Boxes of cells the mapgen writes over the floor, by grid cell.
    local BOX = {}
    local function box(x0, y0, z0, x1, y1, z1, b: string)
        local r = { math.min(x0, x1), math.min(y0, y1), math.min(z0, z1), math.max(x0, x1), math.max(y0, y1),
                    math.max(z0, z1), b }
        local g = L.G
        for i = (r[1] - L.OX + g // 2) // g, (r[4] - L.OX + g // 2) // g do
            for j = (r[3] - L.OZ + g // 2) // g, (r[6] - L.OZ + g // 2) // g do
                local k = cell_key(i, j)
                BOX[k] = BOX[k] or {}
                table.insert(BOX[k], r)
            end
        end
    end
    local G1 = GROUND - 1

    -- The town: a square of flags, the arch, the stalls, houses, the stash, the
    -- waypoint and the gate.
    do
        local tx, tz = L.TX, L.TZ
        box(tx - 7, G1, tz - 7, tx + 7, G1, tz + 7, "flags")
        box(tx - 1, GROUND, tz - 1, tx + 1, GROUND, tz + 1, "cobble")
        box(tx, GROUND + 1, tz, tx, GROUND + 1, tz, "brazier")
        -- The arch (opening 0): a frame in the high ground, a waystone on top.
        box(tx - 1, GROUND, tz - 21, tx - 1, GROUND + 2, tz - 21, "cobble")
        box(tx + 1, GROUND, tz - 21, tx + 1, GROUND + 2, tz - 21, "cobble")
        box(tx, GROUND, tz - 21, tx, GROUND + 1, tz - 21, "air")
        box(tx, GROUND + 2, tz - 21, tx, GROUND + 2, tz - 21, "waystone")
        box(tx - 1, G1, tz - 20, tx + 1, G1, tz - 8, "flags")
        -- Stalls: a back of planks, two posts, an awning, a lamp.
        for _, n in L.NPCS do
            if n.kind ~= "guard" then
                -- Behind the merchant on the side away from the camera (it
                -- looks from -x -z), so nothing of it hides them.
                local s = 1
                box(n.x - 2, GROUND, n.z + 3 * s, n.x + 2, GROUND + 1, n.z + 3 * s, "planks")
                box(n.x - 2, GROUND, n.z + s, n.x - 2, GROUND + 2, n.z + s, "planks")
                box(n.x + 2, GROUND, n.z + s, n.x + 2, GROUND + 2, n.z + s, "planks")
                box(n.x - 2, GROUND + 3, n.z + s, n.x + 2, GROUND + 3, n.z + 3 * s, "awning")
                box(n.x, GROUND + 2, n.z + 3 * s, n.x, GROUND + 2, n.z + 3 * s, "brazier")
                box(n.x - 3, G1, n.z - 3, n.x + 3, G1, n.z + 3, "flags")
            end
        end
        -- Houses in the corners, roofed, shut.
        for _, c in { { -17, 12 }, { 11, 12 }, { -17, -17 } } do
            local x0, z0 = tx + c[1], tz + c[2]
            box(x0, GROUND, z0, x0 + 5, GROUND + 2, z0 + 4, "planks")
            box(x0 - 1, GROUND + 3, z0 - 1, x0 + 6, GROUND + 3, z0 + 5, "roof")
            box(x0 + 2, GROUND + 2, z0 - 1, x0 + 3, GROUND + 2, z0 - 1, "brazier")
        end
        box(L.STASH[1], GROUND, L.STASH[2], L.STASH[1], GROUND, L.STASH[2], "stash")
        -- The gate east: pillars with fire either side of the way out.
        for _, s in { -3, 3 } do
            box(tx + 20, GROUND, tz + s, tx + 20, GROUND + 1, tz + s, "cobble")
            box(tx + 20, GROUND + 2, tz + s, tx + 20, GROUND + 2, tz + s, "brazier")
        end
        box(tx + 8, G1, tz - 2, tx + 20, G1, tz + 2, "flags")
    end

    -- The waypoints: three by three of waystone in the floor.
    for _, w in L.WPS do
        box(w.x - 1, G1, w.z - 1, w.x + 1, G1, w.z + 1, "waystone")
    end
    for _, c in L.CHESTS do
        box(c.x, GROUND, c.z, c.x, GROUND, c.z, "coffer")
    end

    -- What stands about in an area: the waste's broken walls, graves,
    -- braziers and the corners of ruined houses, more fire in the barrows.
    for n, a in L.AREAS do
        if a.kind == "town" or a.kind == "crypt" then
            continue
        end
        local count = math.floor(a.hw * a.hd / 45)
        for k = 1, count do
            local cx = a.cx - a.hw + 4 + math.floor(hash(n, k, 101) * (2 * a.hw - 8))
            local cz = a.cz - a.hd + 4 + math.floor(hash(k, n, 102) * (2 * a.hd - 8))
            -- Not in the ways through the middle, not on a waypoint or a chest.
            if math.abs(cx - a.cx) <= 3 or math.abs(cz - a.cz) <= 3 then
                continue
            end
            local clear = true
            for _, w in L.WPS do
                if math.abs(w.x - cx) < 6 and math.abs(w.z - cz) < 6 then
                    clear = false
                end
            end
            for _, c in L.CHESTS do
                if math.abs(c.x - cx) < 4 and math.abs(c.z - cz) < 4 then
                    clear = false
                end
            end
            if not clear then
                continue
            end
            local what = hash(cx, cz, 5)
            local fire_share = if a.zone == 2 then 0.32 else 0.22
            if what < 0.3 then
                local along = hash(cx, cz, 6) < 0.5
                local len = 3 + math.floor(hash(cx, cz, 7) * 4)
                for i = 0, len - 1 do
                    if hash(cx + i, cz, 8) < 0.2 then
                        continue
                    end
                    local h = 1 + math.floor(hash(cx, cz + i, 9) * (if i > 0 and i < len - 1 then 3 else 2))
                    local x, z = if along then cx + i else cx, if along then cz else cz + i
                    box(x, GROUND, z, x, GROUND + h - 1, z, if hash(x, z, 10) < 0.35 then "mossy" else "cobble")
                end
            elseif what < 0.5 then
                for i = 0, 2 + math.floor(hash(cx, cz, 11) * 2) do
                    box(cx + i * 2, GROUND, cz, cx + i * 2, GROUND, cz, "grave")
                    box(cx + i * 2, G1, cz + 1, cx + i * 2, G1, cz + 1, "dirt")
                end
            elseif what < 0.5 + fire_share then
                box(cx, GROUND, cz, cx, GROUND, cz, "cobble")
                box(cx, GROUND + 1, cz, cx, GROUND + 1, cz, "brazier")
            elseif what < 0.85 then
                for i = 0, 3 do
                    for y = 0, 2 - (i // 2) do
                        box(cx + i, GROUND + y, cz, cx + i, GROUND + y, cz, "cobble")
                        box(cx, GROUND + y, cz + i, cx, GROUND + y, cz + i, "mossy")
                    end
                end
            end
        end
    end

    core.register_mapgen("arpg_game:land", function(ctx)
        blocks()
        for x = ctx.min_x, ctx.max_x do
            for z = ctx.min_z, ctx.max_z do
                core.set_block(x, GROUND - 4, z, B.rock)
                core.set_block(x, GROUND - 3, z, B.rock)
                core.set_block(x, GROUND - 2, z, B.dirt)
                local n = L.at(x, z)
                local a = L.AREAS[n]
                if a == nil then
                    -- The high ground: a cliff's face two high, more than a
                    -- jump and little enough that a body by its foot is not
                    -- hidden from the camera (three hid the nearest two nodes).
                    core.set_block(x, G1, z, B.rock)
                    core.set_block(x, GROUND, z, if hash(x, z, 21) < 0.3 then B.rock else B.cliff)
                    core.set_block(x, GROUND + 1, z, if hash(x // 3, z // 3, 22) < 0.3 then B.moss else B.highland)
                else
                    local top = B.grass
                    local p = hash(x // 4, z // 4, 1)
                    if a.zone == 0 then
                        top = if p < 0.4 then B.dirt else B.grass
                    elseif a.zone == 1 then
                        top = if p < 0.18 then B.dirt elseif p > 0.8 or hash(x, z, 2) < 0.08 then B.moss else B.grass
                    elseif a.zone == 2 then
                        top = if p < 0.35 then B.dirt elseif p > 0.7 then B.moss elseif hash(x, z, 2) < 0.1 then
                              B.rock else B.grass
                    else
                        top = if p < 0.5 then B.cobble else B.mossy
                    end
                    core.set_block(x, G1, z, top)
                end
            end
        end
        local seen = {}
        for _, corner in { { ctx.min_x, ctx.min_z }, { ctx.max_x, ctx.min_z }, { ctx.min_x, ctx.max_z },
                           { ctx.max_x, ctx.max_z } } do
            local list = BOX[cell_key((corner[1] - L.OX + L.G // 2) // L.G, (corner[2] - L.OZ + L.G // 2) // L.G)]
            for _, b in (list or {}) do
                if seen[b] then
                    continue
                end
                seen[b] = true
                local x0, x1 = math.max(b[1], ctx.min_x), math.min(b[4], ctx.max_x)
                local z0, z1 = math.max(b[3], ctx.min_z), math.min(b[6], ctx.max_z)
                local id = if b[7] == "air" then 0 else B[b[7]]
                for x = x0, x1 do
                    for z = z0, z1 do
                        for y = b[2], b[5] do
                            core.set_block(x, y, z, id)
                        end
                    end
                end
            end
        end
    end)

    -- ------------------------------------------------------------ characters

    local function new_state(p: number)
        return { name = core.player_name(p), cls = 0, lvl = 1, xp = 0, gold = 0, lpot = 4, mpot = 2, tp = 2,
                 str = 0, dex = 0, vit = 0, ene = 0, sp = 0, kp = 0, ranks = { 0, 0, 0, 0 }, inv = {}, eq = {},
                 stash = {}, wps = 1, mana = 0, next_blow = 0, way_at = 0, way_to = { 0, 0, 0 }, st = {},
                 lreg_acc = 0, ls_acc = 0, ms_acc = 0, evade_until = 0, tp_cool = 0, restock = true, stock = {},
                 zone = -1, dirty = true, factors = "" }
    end

    local function pack(list: {any}, n: number): string
        local parts = {}
        for k = 1, n do
            if list[k] then
                table.insert(parts, `{k}/{IT.encode(list[k])}`)
            end
        end
        return table.concat(parts, ",")
    end

    local function unpack_into(list: {any}, text: string)
        if text == "" then
            return
        end
        for _, part in string.split(text, ",") do
            local q = string.split(part, "/")
            local k = tonumber(q[1])
            local it = q[2] and IT.decode(q[2])
            if k and it then
                list[k] = it
            end
        end
    end

    function SV.save(s)
        local r = s.ranks
        core.storage_set(`c:{s.name}`, string.format("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", s.cls,
            s.lvl, s.xp, s.gold, s.lpot, s.mpot, s.tp, s.str, s.dex, s.vit, s.ene, s.sp, s.kp, r[1], r[2], r[3], r[4],
            s.wps))
        core.storage_set(`i:{s.name}`, pack(s.inv, D.BAG))
        core.storage_set(`e:{s.name}`, pack(s.eq, 10))
        core.storage_set(`s:{s.name}`, pack(s.stash, D.STASH))
        s.dirty = false
    end

    function SV.load(p: number)
        local s = new_state(p)
        local n = {}
        for _, v in string.split(core.storage_get(`c:{s.name}`), " ") do
            table.insert(n, tonumber(v) or 0)
        end
        if #n >= 18 then
            s.cls, s.lvl, s.xp, s.gold, s.lpot, s.mpot, s.tp = n[1], n[2], n[3], n[4], n[5], n[6], n[7]
            s.str, s.dex, s.vit, s.ene, s.sp, s.kp = n[8], n[9], n[10], n[11], n[12], n[13]
            s.ranks = { n[14], n[15], n[16], n[17] }
            s.wps = bit32.bor(n[18], 1)
        end
        unpack_into(s.inv, core.storage_get(`i:{s.name}`))
        unpack_into(s.eq, core.storage_get(`e:{s.name}`))
        unpack_into(s.stash, core.storage_get(`s:{s.name}`))
        return s
    end

    -- Everything a character is with what they wear: life, mana, damage,
    -- defence and the rest, told to the engine (life, how they walk and what
    -- a blow takes off) and to their machine.
    function SV.recompute(p: number, s)
        local c = D.CLASS[s.cls]
        local t = { str = s.str, dex = s.dex, vit = s.vit, ene = s.ene }
        local def = 0
        local w = s.eq[1]
        local sets = {}
        for k = 1, 10 do
            local it = s.eq[k]
            if it then
                for st, v in IT.stats(it) do
                    if st ~= "ed" and st ~= "edef" and st ~= "maxd" and st ~= "def" then
                        t[st] = (t[st] or 0) + v
                    end
                end
                local _, _, d = IT.base_numbers(it)
                def += d
                if it.r == 4 then
                    sets[it.x // 10] = (sets[it.x // 10] or 0) + 1
                end
            end
        end
        for set, count in sets do
            for k, v in (if count >= 2 then D.SET[set].two else {}) do
                t[k] = (t[k] or 0) + v
            end
            for k, v in (if count >= 3 then D.SET[set].three else {}) do
                t[k] = (t[k] or 0) + v
            end
        end
        if c then
            t.str += c.str
            t.dex += c.dex
            t.vit += c.vit
            t.ene += c.ene
        end
        local st = t
        st.def = def + st.dex // 4
        st.bow = w ~= nil and D.BASES[w.b].cat == "bow"
        local dmin, dmax = 1, 3
        st.spd = 1
        if w then
            dmin, dmax = IT.base_numbers(w)
            st.spd = D.BASES[w.b].spd
        end
        local m = 1 + (if st.bow then st.dex else st.str) / 100
        st.dmin = math.max(1, math.floor(dmin * m))
        st.dmax = math.max(st.dmin, math.floor(dmax * m))
        if c then
            st.life = math.floor(c.life + s.lvl * c.lpl + st.vit * c.lpv + (st.life or 0))
            st.mana = math.floor(c.mana + s.lvl * c.mpl + st.ene * c.mpe + (st.mana or 0))
        else
            st.life, st.mana = 40, 10
        end
        st.life = math.clamp(st.life, 1, 1000)
        for _, k in { "ias", "frw", "ls", "ms", "lreg", "mreg", "mf", "gf", "dr", "sk", "fire", "crit" } do
            st[k] = st[k] or 0
        end
        st.dr = math.min(st.dr, 50)
        s.st = st
        core.player_max_health(p, st.life)
        for k = 1, 10 do
            D.wear(p, k, s.eq[k])
        end
        s.mana = math.min(s.mana, st.mana)
        s.factors = ""   -- told again on the next tick
        tell(p, string.format("sh %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", st.str, st.dex, st.vit,
             st.ene, st.dmin, st.dmax, st.def, st.ias, st.frw, st.ls, st.mf, st.gf, st.dr, st.sk, st.crit, st.fire,
             st.life, st.mana))
        tell(p, `sk {s.ranks[1]} {s.ranks[2]} {s.ranks[3]} {s.ranks[4]} {st.sk}`)
    end

    function SV.stat_line(p: number, s): string
        return string.format("st %d %d %d %d %d %d %d %d %d %d %d %d %d %d", core.health(p), s.st.life or 20,
                             math.floor(s.mana), s.st.mana or 10, s.xp, D.next_xp(s.lvl), s.lvl, s.gold, s.lpot,
                             s.mpot, s.tp, s.cls, s.sp, s.kp)
    end

    function SV.send_list(p: number, what: string, list, n: number)
        for k = 1, n do
            tell(p, if list[k] then `{what} {k} {IT.encode(list[k])}` else `{what} {k}`)
        end
    end

    function SV.msg(p: number, text: string)
        tell(p, "msg " .. text)
    end

    -- ------------------------------------------------------------- rolling

    -- An item that dropped at `level`: its base, its rarity (better with magic
    -- find), its affixes. `only` keeps to bases of one kind group ("J" jewels).
    function SV.roll(level: number, mf: number, only: string?, floor_rarity: number?)
        local bases = {}
        for i, b in D.BASES do
            if b.lvl <= level + 1 and (only == nil or string.find(only, D.CAT[b.cat].g, 1, true)) then
                table.insert(bases, i)
            end
        end
        local bi = bases[math.random(1, #bases)]
        local b = D.BASES[bi]
        local m = 1 + mf / 100
        local u = math.random()
        local r = 0
        if u < 0.012 * m then
            r = 3
        elseif u < 0.03 * m then
            r = 4
        elseif u < 0.1 * m then
            r = 2
        elseif u < 0.38 * m then
            r = 1
        end
        r = math.max(r, floor_rarity or 0)
        local it = { b = bi, r = r, l = level, x = 0, a = {} }
        if r == 3 then
            local pool = {}
            for k, uq in D.UNIQUE do
                if uq.lvl <= level + 2 then
                    table.insert(pool, k)
                end
            end
            if #pool == 0 then
                it.r, r = 2, 2
            else
                it.x = pool[math.random(1, #pool)]
                it.b = D.BASE_OF[D.UNIQUE[it.x].base]
                return it
            end
        end
        if r == 4 then
            local set = math.random(1, #D.SET)
            local piece = math.random(1, 3)
            it.x = set * 10 + piece
            it.b = D.BASE_OF[D.SET[set].items[piece].base]
            return it
        end
        local cat = b.cat
        if r == 0 and (cat == "ring" or cat == "amulet") then
            it.r, r = 1, 1
        end
        if r == 0 then
            return it
        end
        local want_pre, want_suf
        if r == 1 then
            local k = math.random()
            want_pre, want_suf = (if k < 0.66 then 1 else 0), (if k > 0.33 then 1 else 0)
        else
            local n = math.random(3, 5)
            want_pre = math.random(math.max(1, n - 3), math.min(3, n - 1))
            want_suf = n - want_pre
            it.x = math.random(0, 255)
        end
        local g = D.CAT[cat].g
        local used = {}
        for _, pre in { true, false } do
            local want = if pre then want_pre else want_suf
            for _ = 1, want do
                local pool = {}
                for i, a in D.AFFIX do
                    if a.pre == pre and a.lvl <= level and string.find(a.g, g, 1, true) and not used[a.stat] and
                       (a.only == nil or a.only == cat) then
                        table.insert(pool, i)
                    end
                end
                if #pool > 0 then
                    local i = pool[math.random(1, #pool)]
                    used[D.AFFIX[i].stat] = true
                    table.insert(it.a, { i, math.random(D.AFFIX[i].min, D.AFFIX[i].max) })
                end
            end
        end
        if #it.a == 0 then
            it.r = 0
        end
        return it
    end

    -- ------------------------------------------------------------- the ground

    function SV.drop(code: string, x: number, y: number, z: number)
        local id = SV.next_id
        SV.next_id += 1
        local a = math.random() * 2 * math.pi
        local d = 0.4 + math.random() * 1.1
        x, z = x + math.cos(a) * d, z + math.sin(a) * d
        y = math.floor(y + 0.5)
        SV.ground[id] = { code = code, x = x, y = y, z = z }
        tell(0, string.format("gi %d %.2f %d %.2f %s", id, x, y, z, code))
    end

    function SV.drop_item(it, x, y, z)
        SV.drop(IT.encode(it), x, y, z)
    end

    -- What a death leaves: experience to all near, then gold, potions,
    -- globes and items by how great it was.
    function SV.reward(p: number, info, x: number, y: number, z: number)
        local mob = D.MOB[info.kind] or D.MOB["arpg_core:zombie"]
        local xp = math.floor(mob.xp * info.lvl * D.RANK[info.rank].xp * (if info.rank == 3 then 8 else 1))
        local mf, gf = 0, 0
        for q, s in SV.players do
            local qx, _, qz = core.player_pos(q)
            if qx and s.cls > 0 and (qx - x) ^ 2 + (qz - z) ^ 2 < 1600 then
                SV.give_xp(q, s, xp)
                if q == p then
                    mf, gf = s.st.mf or 0, s.st.gf or 0
                end
            end
        end
        local items = ({ [0] = 0, 1, 1, 3 })[info.rank]
        local chance = ({ [0] = 0.12, 0.6, 1, 1 })[info.rank]
        if math.random() < chance then
            for k = 1, items do
                SV.drop_item(SV.roll(info.lvl, mf, nil, if info.rank >= 2 and k == 1 then 1 else 0), x, y, z)
            end
        end
        if info.rank == 3 then
            SV.drop_item(SV.roll(info.lvl + 1, mf + 200, nil, 2), x, y, z)
        end
        if math.random() < 0.45 + 0.2 * info.rank then
            SV.drop(`${math.floor(math.random(2, 6) * info.lvl * (1 + info.rank) * (1 + gf / 100))}`, x, y, z)
        end
        local r = math.random()
        if r < 0.05 then
            SV.drop("!l", x, y, z)
        elseif r < 0.09 then
            SV.drop("!m", x, y, z)
        elseif r < 0.11 then
            SV.drop("!t", x, y, z)
        elseif r < 0.26 then
            SV.drop("@l", x, y, z)
        elseif r < 0.4 then
            SV.drop("@m", x, y, z)
        end
    end

    function SV.give_xp(p: number, s, n: number)
        if s.lvl >= D.MAX_LEVEL then
            return
        end
        s.xp += n
        while s.xp >= D.next_xp(s.lvl) and s.lvl < D.MAX_LEVEL do
            s.xp -= D.next_xp(s.lvl)
            s.lvl += 1
            s.sp += 5
            s.kp += 1
            SV.recompute(p, s)
            core.heal(p, 1000)
            s.mana = s.st.mana
            tell(p, `level {s.lvl}`)
        end
        s.dirty = true
    end

    -- ------------------------------------------------------------ creatures

    -- What this file knows of a creature: its kind, level, rank, name and real
    -- life. One it did not spawn (arpg_crypt's, which says so) or never heard
    -- of is taken for a zombie of the place it stands in.
    function SV.info(id: number)
        local info = SV.mobs[id]
        if info then
            return info
        end
        local x, y, z = core.mob_pos(id)
        if x == nil then
            return nil
        end
        info = SV.new_info("arpg_core:zombie", L.level_at(x, y, z), 0)
        SV.mobs[id] = info
        return info
    end

    function SV.new_info(kind: string, lvl: number, rank: number, name: string?)
        local mob = D.MOB[kind] or D.MOB["arpg_core:zombie"]
        local real = math.max(1, math.floor(mob.hp * D.hp_mult(lvl) * D.RANK[rank].hp))
        local shown = name
        if shown == nil then
            shown = mob.boss or mob.names[D.tier(lvl)]
            if rank == 1 then
                shown = "CHAMPION " .. shown
            end
        end
        return { kind = kind, lvl = lvl, rank = if mob.boss then 3 else rank, real = real, taken = 0, name = shown }
    end

    -- arpg_crypt says what it spawns, so its dead are known by kind.
    core.event.define("arpg:spawned", {
        fields = { id = "number", kind = "string" },
        cancellable = false,
    })
    core.event.on("arpg:spawned", function(ev)
        local x, y, z = core.mob_pos(ev.id)
        if x ~= nil then
            local lvl = L.level_at(x, y, z)
            SV.mobs[ev.id] = SV.new_info(ev.kind, lvl, 0)
            core.mob_blow_every(ev.id, D.blow_every(lvl))
        end
    end)

    -- A blow of `dmg` real points from player p: the share of the creature's
    -- thousand it is, life and mana stolen, and the reward if it died.
    function SV.deal(p: number, s, id: number, dmg: number, fx: number, fz: number, steal: boolean): boolean
        if SV.npc[id] then
            return false
        end
        local info = SV.info(id)
        if info == nil then
            return false
        end
        local x, y, z = core.mob_pos(id)
        if x == nil then
            return false
        end
        if s.st.crit > 0 and math.random(1, 100) <= s.st.crit then
            dmg *= 2
        end
        dmg = math.max(1, math.floor(dmg + 0.5))
        local units = math.max(1, math.floor(dmg * D.UNITS / info.real + 0.5))
        -- Champions, named ones and lords stand their ground: struck from
        -- where they stand, they are not pushed. Knocked back by every blow, a
        -- lord never came within its own reach of a warrior.
        if info.rank > 0 or SV.props[id] then
            fx, fz = x, z
        end
        local r = core.mob_hurt(id, units, fx, fz)
        if r == 0 then
            return false
        end
        info.taken += dmg
        s.last_hit = id
        if steal then
            s.ls_acc += dmg * s.st.ls / 100
            s.ms_acc += dmg * s.st.ms / 100
            if s.ls_acc >= 1 then
                core.heal(p, math.floor(s.ls_acc))
                s.ls_acc -= math.floor(s.ls_acc)
            end
            if s.ms_acc >= 1 then
                s.mana = math.min(s.st.mana, s.mana + math.floor(s.ms_acc))
                s.ms_acc -= math.floor(s.ms_acc)
            end
        end
        if r == 2 then
            SV.killed[id] = true
            SV.reward(p, info, x, y, z)
        end
        return true
    end

    -- Every creature within r of a point, but the town's people.
    function SV.around(x: number, y: number, z: number, r: number): {number}
        local out = {}
        for _, id in core.mob_ids(x, y, z, r) do
            if not SV.npc[id] then
                table.insert(out, id)
            end
        end
        for id in SV.props do
            local px, py, pz = core.mob_pos(id)
            if px and (px - x) ^ 2 + (py - y) ^ 2 + (pz - z) ^ 2 <= r * r then
                table.insert(out, id)
            end
        end
        return out
    end

    function SV.weapon_hit(s): number
        return math.random(s.st.dmin, s.st.dmax) + s.st.fire
    end

    -- The creature a ray under the cursor meets first, by id, and where its
    -- feet are; or nil: the nearest along the ray of those whose middle passes
    -- within 0.9 of it. `npc` takes the town's people as well (workers, which
    -- core.mob_ids does not list).
    function SV.mob_on_ray(o: {number}, d: {number}, around: {number}, npc: boolean): (number?, {number}?)
        local best, best_at, best_t = nil, nil, math.huge
        local ids = core.mob_ids(around[1], around[2], around[3], 40)
        for id in SV.props do
            table.insert(ids, id)
        end
        if npc then
            for id in SV.npc do
                table.insert(ids, id)
            end
        end
        for _, id in ids do
            local x, y, z = core.mob_pos(id)
            if x ~= nil then
                local cx, cy, cz = x - o[1], y + 0.9 - o[2], z - o[3]
                local t = cx * d[1] + cy * d[2] + cz * d[3]
                if t > 0 and cx * cx + cy * cy + cz * cz - t * t < 0.81 and t < best_t then
                    best, best_at, best_t = id, { x, y, z }, t
                end
            end
        end
        return best, best_at
    end

    function SV.ground_on_ray(o: {number}, d: {number}): {number}?
        local _, _, _, dist = core.raycast(o[1], o[2], o[3], d[1], d[2], d[3], 120)
        if dist == nil then
            return nil
        end
        return { o[1] + d[1] * dist, o[2] + d[2] * dist, o[3] + d[3] * dist }
    end

    -- A ray as the client sends it: six numbers after the first word, d of
    -- length 1. Nil for anything else.
    function SV.read_ray(w: {string}, from: number): ({number}?, {number}?)
        local n = {}
        for i = from, from + 5 do
            n[#n + 1] = tonumber(w[i])
            if n[#n] == nil or n[#n] ~= n[#n] then
                return nil, nil
            end
        end
        local l = math.sqrt(n[4] * n[4] + n[5] * n[5] + n[6] * n[6])
        if l < 0.5 then
            return nil, nil
        end
        return { n[1], n[2], n[3] }, { n[4] / l, n[5] / l, n[6] / l }
    end

    -- Somewhere a body can stand: open ground, two clear above.
    function SV.standable(x: number, y: number, z: number): boolean
        x, y, z = math.floor(x), math.floor(y + 0.5), math.floor(z)
        return L.at(x, z) ~= 0 and core.get_block(x, y, z) == 0 and core.get_block(x, y + 1, z) == 0 and
               core.get_block(x, y - 1, z) ~= 0
    end

    -- ------------------------------------------------------------ projectiles

    -- An arrow or a fireball: from the chest, flat, `speed` along x z, until
    -- it meets a wall, a creature (a piercing one goes on) or its time.
    function SV.shoot(p: number, kind: string, x: number, y: number, z: number, dx: number, dz: number,
                      speed: number, life: number, dmg: number, opts)
        local l = math.sqrt(dx * dx + dz * dz)
        if l < 0.01 then
            return
        end
        local id = SV.next_id
        SV.next_id += 1
        local b = { id = id, kind = kind, owner = p, x = x + dx / l * 0.6, y = y, z = z + dz / l * 0.6,
                    vx = dx / l * speed, vz = dz / l * speed, t = 0, life = life, dmg = dmg, hit = {},
                    pierce = opts and opts.pierce, burst = opts and opts.burst }
        table.insert(SV.proj, b)
        tell(0, string.format("pj %d %s %.2f %.2f %.2f %.2f %.2f", id, kind, b.x, b.y, b.z, b.vx, b.vz))
    end

    function SV.burst(p: number, s, x: number, y: number, z: number, r: number, dmg: number)
        for _, id in SV.around(x, y - CHEST, z, r) do
            SV.deal(p, s, id, dmg, x, z, false)
        end
    end

    function SV.step_proj(dt: number)
        for i = #SV.proj, 1, -1 do
            local b = SV.proj[i]
            local s = SV.players[b.owner]
            b.t += dt
            local speed = math.sqrt(b.vx * b.vx + b.vz * b.vz)
            local _, _, _, wall = core.raycast(b.x, b.y, b.z, b.vx / speed, 0, b.vz / speed, speed * dt)
            local done = wall ~= nil or b.t > b.life or s == nil
            if wall ~= nil then
                b.x += b.vx / speed * wall
                b.z += b.vz / speed * wall
            end
            if not done then
                for _, id in SV.around(b.x, b.y - CHEST, b.z, 1.2) do
                    if not b.hit[id] then
                        b.hit[id] = true
                        if b.burst == nil then
                            SV.deal(b.owner, s, id, b.dmg, b.x, b.z, b.kind == "arrow")
                        end
                        if not b.pierce then
                            done = true
                            break
                        end
                    end
                end
            end
            if done then
                table.remove(SV.proj, i)
                if b.burst and s then
                    SV.burst(b.owner, s, b.x, b.y, b.z, b.burst, b.dmg)
                end
                tell(0, string.format("pp %d %.2f %.2f %.2f %s", b.id, b.x, b.y, b.z,
                                      if b.burst then "burst" else b.kind))
            else
                b.x += b.vx * dt
                b.z += b.vz * dt
            end
        end
    end

    -- ---------------------------------------------------------------- skills

    local function rank_of(s, n: number): number
        return if s.ranks[n] > 0 then s.ranks[n] + s.st.sk else 0
    end

    local function spell_mult(s, n: number, per: number): number
        return (1 + per * (rank_of(s, n) - 1)) * (1 + 0.03 * s.lvl)
    end

    -- A clip for everybody to see the body play, and for its own machine, until
    -- it has played once (D.CLIP); then the walk and the idle again.
    function SV.anim(p: number, s, clip: string)
        core.player_clip(p, clip)
        s.clip_until = clock + D.CLIP[clip]
        tell(p, `anim {clip} {D.CLIP[clip]}`)
    end

    -- A melee skill or the plain blow, on the creature in reach.
    function SV.strike(p: number, s, id: number, n: number?)
        local px, _, pz = core.player_pos(p)
        local x, y, z = core.mob_pos(id)
        if px == nil or x == nil then
            return
        end
        local hit = SV.weapon_hit(s)
        local w = s.eq[1] and D.BASES[s.eq[1].b]
        SV.anim(p, s, if n == 1 and s.cls == 1 then "sword_overhead"
                      elseif w and w.cat == "dagger" then "dagger_stab" else "sword_slash")
        if n == 1 and s.cls == 1 then
            hit *= 1.5 + 0.15 * (rank_of(s, 1) - 1)
            tell(0, string.format("fx bash %.2f %.2f %.2f", x, y + CHEST, z))
        else
            tell(0, string.format("fx slash %.2f %.2f %.2f", x, y + CHEST, z))
        end
        -- A warrior's swing carries on into whatever stands by the one struck.
        if s.cls == 1 then
            for _, other in SV.around(x, y, z, D.CLEAVE_R) do
                if other ~= id then
                    SV.deal(p, s, other, hit * D.CLEAVE_SHARE, px, pz, true)
                end
            end
        end
        SV.deal(p, s, id, hit, px, pz, true)
    end

    -- An arrow at a point, as the plain shot or a hunter's skill.
    function SV.loose(p: number, s, at: {number}, n: number?)
        local px, py, pz = core.player_pos(p)
        if px == nil then
            return false
        end
        local dx, dz = at[1] - px, at[3] - pz
        local base = SV.weapon_hit(s)
        SV.anim(p, s, "bow_shoot")
        if n == nil then
            SV.shoot(p, "arrow", px, py + CHEST + 0.3, pz, dx, dz, 24, 0.75, base)
        elseif n == 1 then
            SV.shoot(p, "arrow", px, py + CHEST + 0.3, pz, dx, dz, 28, 0.8, base * (1.5 + 0.2 * (rank_of(s, 1) - 1)),
                     { pierce = true })
        elseif n == 2 then
            local count = math.min(9, 3 + rank_of(s, 2) // 2)
            local a0 = math.atan2(dz, dx)
            for k = 0, count - 1 do
                local a = a0 + (k - (count - 1) / 2) * 0.14
                SV.shoot(p, "arrow", px, py + CHEST + 0.3, pz, math.cos(a), math.sin(a), 24, 0.7, base * 0.75)
            end
        elseif n == 3 then
            SV.shoot(p, "fire", px, py + CHEST + 0.3, pz, dx, dz, 22, 0.8,
                     base * (1 + 0.2 * (rank_of(s, 3) - 1)) + 4, { burst = 2.5 })
        end
        return true
    end

    -- Where a leap, a teleport or an evade lands: the point, or nil.
    local function landing(s, at: {number}, px: number, py: number, pz: number, far: number): {number}?
        local x, z = math.floor(at[1]), math.floor(at[3])
        local y = math.floor(at[2] + 0.5)
        if (x + 0.5 - px) ^ 2 + (z + 0.5 - pz) ^ 2 > far * far or not SV.standable(x, y, z) then
            return nil
        end
        return { x + 0.5, y, z + 0.5 }
    end

    function SV.cast(p: number, s, n: number, o: {number}, d: {number})
        local c = D.CLASS[s.cls]
        local px, py, pz = core.player_pos(p)
        if c == nil or px == nil or c.skills[n] == nil then
            return
        end
        local sk = c.skills[n]
        if s.zone == 0 then
            SV.msg(p, "NOT IN TOWN")
            return
        end
        if s.ranks[n] == 0 then
            SV.msg(p, if s.lvl < sk.lvl then `{sk.name} NEEDS LEVEL {sk.lvl}` else `{sk.name}: SPEND A SKILL POINT (K)`)
            return
        end
        if s.mana < sk.cost then
            tell(p, "nomana")
            return
        end
        local id, mob = SV.mob_on_ray(o, d, { px, py, pz }, false)
        local at = if mob then { mob[1], mob[2] + CHEST, mob[3] } else SV.ground_on_ray(o, d)
        local ok = false
        local key = c.key
        if key == "warrior" then
            if n == 1 then
                -- Walked up to, and struck in reach (step_target).
                if id == nil then
                    return
                end
                s.target, s.pending = id, 1
                s.way_at = 0
                return
            elseif n == 2 then
                local hit = SV.weapon_hit(s) * (0.8 + 0.12 * (rank_of(s, 2) - 1))
                for _, m in SV.around(px, py, pz, 3.2) do
                    SV.deal(p, s, m, hit, px, pz, true)
                end
                tell(0, string.format("fx cleave %.2f %.2f %.2f", px, py, pz))
                SV.anim(p, s, "spin_slash")
                ok = true
            elseif n == 3 then
                local hit = SV.weapon_hit(s) * (0.6 + 0.1 * (rank_of(s, 3) - 1))
                for _, m in SV.around(px, py, pz, 7) do
                    SV.deal(p, s, m, hit, px, pz, false)
                end
                core.slow_mobs(px, py, pz, 7, 0.5, 4)
                tell(0, string.format("fx cry %.2f %.2f %.2f", px, py, pz))
                SV.anim(p, s, "war_cry")
                ok = true
            elseif n == 4 and at then
                local to = landing(s, at, px, py, pz, 12)
                if to then
                    core.player_place(p, to[1], to[2], to[3])
                    local hit = SV.weapon_hit(s) * (1 + 0.15 * (rank_of(s, 4) - 1))
                    for _, m in SV.around(to[1], to[2], to[3], 3) do
                        SV.deal(p, s, m, hit, to[1], to[3], true)
                    end
                    tell(0, string.format("fx leap %.2f %.2f %.2f %.2f %.2f %.2f", px, py, pz, to[1], to[2], to[3]))
                    SV.anim(p, s, "jump")
                    s.target = nil
                    ok = true
                end
            end
        elseif key == "sorcerer" then
            if n == 1 and at then
                SV.shoot(p, "fire", px, py + CHEST + 0.3, pz, at[1] - px, at[3] - pz, 16, 1.6,
                         (math.random(4, 8) + SV.weapon_hit(s) * 0.5) * spell_mult(s, 1, 0.2), { burst = 2.5 })
                SV.anim(p, s, "staff_cast")
                ok = true
            elseif n == 2 then
                SV.burst(p, s, px, py + CHEST, pz, 6, math.random(6, 10) * spell_mult(s, 2, 0.3))
                core.slow_mobs(px, py, pz, 6, 0.4, 4)
                tell(0, string.format("fx nova %.2f %.2f %.2f", px, py, pz))
                SV.anim(p, s, "cast_nova")
                ok = true
            elseif n == 3 and at then
                local first = SV.around(at[1], at[2], at[3], 3)
                local fx, fy, fz = px, py + CHEST + 0.3, pz
                SV.anim(p, s, "staff_cast")
                if #first == 0 then
                    tell(0, string.format("zap %.2f %.2f %.2f %.2f %.2f %.2f", fx, fy, fz, at[1], at[2] + 0.2, at[3]))
                else
                    local struck = {}
                    local cur: number? = first[1]
                    for _ = 1, 5 + rank_of(s, 3) // 4 do
                        local mx, my, mz = core.mob_pos(cur :: number)
                        if mx == nil then
                            break
                        end
                        struck[cur :: number] = true
                        SV.deal(p, s, cur :: number, math.random(4, 24) * spell_mult(s, 3, 0.3), fx, fz, false)
                        tell(0, string.format("zap %.2f %.2f %.2f %.2f %.2f %.2f", fx, fy, fz, mx, my + CHEST, mz))
                        fx, fy, fz = mx, my + CHEST, mz
                        cur = nil
                        for _, other in SV.around(mx, my, mz, 9) do
                            if not struck[other] then
                                cur = other
                                break
                            end
                        end
                        if cur == nil then
                            break
                        end
                    end
                end
                ok = true
            elseif n == 4 and at then
                local to = landing(s, at, px, py, pz, 25)
                if to then
                    core.player_place(p, to[1], to[2], to[3])
                    SV.anim(p, s, "cast_nova")
                    tell(0, string.format("fx blink %.2f %.2f %.2f %.2f %.2f %.2f", px, py, pz, to[1], to[2], to[3]))
                    s.target = nil
                    ok = true
                end
            end
        else
            if n <= 3 then
                if not s.st.bow then
                    SV.msg(p, "NEEDS A BOW")
                    return
                end
                if at then
                    ok = SV.loose(p, s, at, n)
                end
            elseif n == 4 and at then
                -- Along the way to the cursor, as far as 8 and the ground goes.
                local dx, dz = at[1] - px, at[3] - pz
                local l = math.sqrt(dx * dx + dz * dz)
                if l > 0.5 then
                    local far = math.min(8, l)
                    local to = nil
                    for k = 1, math.floor(far * 2) do
                        local x, z = px + dx / l * k * 0.5, pz + dz / l * k * 0.5
                        if not SV.standable(x, py, z) then
                            break
                        end
                        to = { x, py, z }
                    end
                    if to then
                        core.player_place(p, to[1], to[2], to[3])
                        s.evade_until = clock + 1 + 0.15 * rank_of(s, 4)
                        s.factors = ""
                        tell(0, string.format("fx blink %.2f %.2f %.2f %.2f %.2f %.2f", px, py, pz, to[1], to[2], to[3]))
                        s.target = nil
                        ok = true
                    end
                end
            end
        end
        if ok then
            s.mana -= sk.cost
            tell(p, SV.stat_line(p, s))
        end
    end

    -- ------------------------------------------------------------ the bag

    local function free_slot(list, n: number): number?
        for k = 1, n do
            if list[k] == nil then
                return k
            end
        end
        return nil
    end

    -- Whether a character can wear it, and the reason when not.
    function SV.can_wear(s, it): string?
        local b = D.BASES[it.b]
        if IT.req(it) > s.lvl then
            return `NEEDS LEVEL {IT.req(it)}`
        end
        if b.cls and b.cls ~= s.cls then
            return `{D.CLASS[b.cls].name} ONLY`
        end
        return nil
    end

    function SV.equip(p: number, s, k: number)
        local it = s.inv[k]
        if it == nil then
            return
        end
        local why = SV.can_wear(s, it)
        if why then
            SV.msg(p, why)
            return
        end
        local slot = D.CAT[D.BASES[it.b].cat].slot
        local to = nil
        for e = 1, 10 do
            if D.EQ[e] == slot and (to == nil or (s.eq[to] ~= nil and s.eq[e] == nil)) then
                to = e
            end
        end
        if to == nil then
            return
        end
        s.inv[k], s.eq[to] = s.eq[to], it
        tell(p, if s.inv[k] then `iv {k} {IT.encode(s.inv[k])}` else `iv {k}`)
        tell(p, `eq {to} {IT.encode(it)}`)
        SV.recompute(p, s)
        s.dirty = true
    end

    function SV.unequip(p: number, s, e: number)
        if s.eq[e] == nil then
            return
        end
        local k = free_slot(s.inv, D.BAG)
        if k == nil then
            SV.msg(p, "THE BAG IS FULL")
            return
        end
        s.inv[k], s.eq[e] = s.eq[e], nil
        tell(p, `iv {k} {IT.encode(s.inv[k])}`)
        tell(p, `eq {e}`)
        SV.recompute(p, s)
        s.dirty = true
    end

    -- Into the bag, or the belt for what goes there; false when full.
    function SV.take(p: number, s, code: string): boolean
        local what, n = IT.loose(code)
        if what == "gold" then
            s.gold += n
            tell(p, `got gold {n}`)
            return true
        elseif what == "lpot" or what == "mpot" or what == "tp" then
            if s[what] >= D.BELT then
                return false
            end
            s[what] += 1
            tell(p, `got {what}`)
            return true
        elseif what == "life" then
            core.heal(p, math.max(1, math.floor((s.st.life or 20) * 0.15)))
            return true
        elseif what == "mana" then
            s.mana = math.min(s.st.mana or 10, s.mana + (s.st.mana or 10) * 0.2)
            return true
        end
        local it = IT.decode(code)
        local k = free_slot(s.inv, D.BAG)
        if it == nil or k == nil then
            if k == nil then
                SV.msg(p, "THE BAG IS FULL")
            end
            return false
        end
        s.inv[k] = it
        tell(p, `iv {k} {code}`)
        tell(p, `got item {code}`)
        return true
    end

    function SV.pick(p: number, s, id: number)
        local g = SV.ground[id]
        if g and SV.take(p, s, g.code) then
            SV.ground[id] = nil
            tell(0, `gg {id}`)
            tell(p, SV.stat_line(p, s))
            s.dirty = true
        end
    end

    -- ------------------------------------------------------------ merchants

    -- What each one has for this player, made again each time they come back
    -- to town: the smith's arms and armour, the jeweller's rings, amulets and
    -- orbs, the alchemist's potions, scrolls and staves.
    function SV.restock(s)
        local lvl = s.lvl + 2
        s.stock = { smith = {}, jeweler = {}, alchemist = { { code = "!l", price = 25 }, { code = "!m", price = 25 },
                                                            { code = "!t", price = 40 } } }
        for _ = 1, 12 do
            local it = SV.roll(lvl, 0, "WA")
            if it.r > 1 then
                it = SV.roll(lvl, -100, "WA")
            end
            table.insert(s.stock.smith, { code = IT.encode(it), price = IT.value(it) })
        end
        for _ = 1, 8 do
            local it = SV.roll(lvl, 0, "J", 1)
            if it.r > 2 then
                it.r, it.x = 1, 0
            end
            if #it.a == 0 then
                it = SV.roll(lvl, 0, "J", 1)
            end
            if it.r <= 2 then
                table.insert(s.stock.jeweler, { code = IT.encode(it), price = IT.value(it) })
            end
        end
        for _ = 1, 3 do
            local it = SV.roll(lvl, 0, "W", 1)
            local cat = D.BASES[it.b].cat
            if (cat == "staff" or cat == "wand") and it.r <= 2 then
                table.insert(s.stock.alchemist, { code = IT.encode(it), price = IT.value(it) })
            end
        end
        s.restock = false
    end

    function SV.open_shop(p: number, s, kind: string)
        if kind == "guard" then
            SV.msg(p, "THE WAY EAST IS NOT SAFE. MIND THE DEAD.")
            return
        end
        if s.restock then
            SV.restock(s)
        end
        s.shop = kind
        if kind == "alchemist" then
            core.heal(p, 1000)
            s.mana = s.st.mana
            SV.msg(p, "THE ALCHEMIST TENDS YOUR WOUNDS")
        end
        tell(p, `shop {kind}`)
        for k, w in s.stock[kind] do
            tell(p, `sp {k} {w.price} {w.code}`)
        end
    end

    function SV.buy(p: number, s, k: number)
        local w = s.shop and s.stock[s.shop][k]
        if w == nil then
            return
        end
        if s.gold < w.price then
            SV.msg(p, "NOT ENOUGH GOLD")
            return
        end
        local what = IT.loose(w.code)
        if what then
            if s[what] >= D.BELT then
                SV.msg(p, "THE BELT IS FULL")
                return
            end
            s[what] += 1
        else
            local slot = free_slot(s.inv, D.BAG)
            if slot == nil then
                SV.msg(p, "THE BAG IS FULL")
                return
            end
            s.inv[slot] = IT.decode(w.code)
            tell(p, `iv {slot} {w.code}`)
            table.remove(s.stock[s.shop], k)
            tell(p, `shop {s.shop}`)
            for j, v in s.stock[s.shop] do
                tell(p, `sp {j} {v.price} {v.code}`)
            end
        end
        s.gold -= w.price
        s.dirty = true
        tell(p, SV.stat_line(p, s))
    end

    function SV.sell(p: number, s, k: number)
        local it = s.inv[k]
        if it == nil or s.shop == nil then
            return
        end
        local price = math.max(1, IT.value(it) // 4)
        s.gold += price
        s.inv[k] = nil
        tell(p, `iv {k}`)
        SV.msg(p, `SOLD FOR {price} GOLD`)
        s.dirty = true
        tell(p, SV.stat_line(p, s))
    end

    -- ------------------------------------------------------------ the town's people

    function SV.spawn_npcs()
        if next(SV.npc) ~= nil then
            return
        end
        for _, n in L.NPCS do
            local id = core.spawn_mob(`arpg_core:{n.kind}`, n.x + 0.5, GROUND, n.z + 0.5)
            if id ~= 0 then
                SV.npc[id] = n
                core.mob_face(id, L.TX, L.TZ)
            end
        end
    end

    function SV.tell_npcs(p: number)
        for id, n in SV.npc do
            tell(p, string.format("npc %d %s %.1f %d %.1f", id, n.kind, n.x + 0.5, GROUND, n.z + 0.5))
        end
    end

    -- ------------------------------------------------------------ portals

    function SV.open_portal(p: number, s)
        if s.zone == 0 then
            SV.msg(p, "YOU ARE IN TOWN")
            return
        end
        if s.tp <= 0 then
            SV.msg(p, "NO TOWN PORTAL SCROLLS")
            return
        end
        local px, py, pz = core.player_pos(p)
        if px == nil then
            return
        end
        local fx, fz = px + 1.2, pz - 1.2
        if not SV.standable(fx, py, fz) then
            fx, fz = px, pz
        end
        local spot = L.TP_SPOTS[(p % #L.TP_SPOTS) + 1]
        s.tp -= 1
        SV.portals[p] = { fx = fx, fy = py, fz = fz, tx = spot[1], ty = spot[2], tz = spot[3] }
        s.tp_cool = clock + 1
        tell(0, string.format("tp %d %.2f %.2f %.2f %.2f %.2f %.2f", p, fx, py, fz, spot[1], spot[2], spot[3]))
        tell(p, SV.stat_line(p, s))
        s.dirty = true
    end

    function SV.close_portal(p: number)
        if SV.portals[p] then
            SV.portals[p] = nil
            tell(0, `tpx {p}`)
        end
    end

    -- Through a portal: who walks into either end comes out of the other; its
    -- owner coming back from town closes it.
    function SV.step_portals()
        for q, s in SV.players do
            if clock < s.tp_cool then
                continue
            end
            local x, y, z = core.player_pos(q)
            if x == nil then
                continue
            end
            for owner, t in SV.portals do
                if (x - t.fx) ^ 2 + (z - t.fz) ^ 2 < 0.8 and math.abs(y - t.fy) < 2 then
                    core.player_place(q, t.tx, t.ty, t.tz + 1.6)
                    s.tp_cool = clock + 1.5
                    break
                elseif (x - t.tx) ^ 2 + (z - t.tz) ^ 2 < 0.8 and math.abs(y - t.ty) < 2 then
                    core.player_place(q, t.fx + 1.4, t.fy, t.fz + 1.4)
                    s.tp_cool = clock + 1.5
                    if q == owner then
                        SV.close_portal(owner)
                    end
                    break
                end
            end
        end
    end

    -- ------------------------------------------------------------ messages

    -- The corners of a way round what stands between a player and `to`, told
    -- to their machine, which walks it.
    function SV.send_way(p: number, px: number, py: number, pz: number, to: {number})
        local cells = core.find_path(math.floor(px), math.floor(py + 0.01), math.floor(pz), math.floor(to[1]),
                                     math.floor(to[2] + 0.01), math.floor(to[3]), 2, 1, 3, false, 3000, 1.5, true)
        if #cells <= 3 then
            return
        end
        local parts = { "way" }
        for i = 1, #cells, 3 do
            table.insert(parts, string.format("%.1f %.1f", cells[i] + 0.5, cells[i + 2] + 0.5))
        end
        tell(p, table.concat(parts, " "))
    end

    -- Somewhere to walk to and do a thing at: a merchant, the stash, a
    -- waypoint, a chest, an item on the ground.
    function SV.go(p: number, s, goal, x: number, y: number, z: number)
        s.goal, s.target = goal, nil
        local px, py, pz = core.player_pos(p)
        if px then
            SV.send_way(p, px, py, pz, { x, y, z })
        end
        tell(p, string.format("goto %.2f %.2f %.2f", x, y, z))
    end

    function SV.thing_near(at: {number}): (any, number, number, number)
        local best, bd = nil, 2.2
        local bx, by, bz = 0, 0, 0
        local function consider(goal, x, y, z)
            local d = math.sqrt((at[1] - x) ^ 2 + (at[3] - z) ^ 2)
            if d < bd and math.abs(at[2] - y) < 3 then
                best, bd, bx, by, bz = goal, d, x, y, z
            end
        end
        for id, g in SV.ground do
            if IT.loose(g.code) == nil or string.sub(g.code, 1, 1) == "!" then
                consider({ kind = "item", id = id }, g.x, g.y, g.z)
            end
        end
        consider({ kind = "stash" }, L.STASH[1] + 0.5, GROUND, L.STASH[2] + 0.5)
        for k, w in L.WPS do
            consider({ kind = "wp", k = k }, w.x + 0.5, GROUND, w.z + 0.5)
        end
        for k, c in L.CHESTS do
            if not SV.chest_open[k] then
                consider({ kind = "chest", k = k }, c.x + 0.5, GROUND, c.z + 0.5)
            end
        end
        return best, bx, by, bz
    end

    asked = function(p: number, text: string)
        local s = SV.players[p]
        if s == nil then
            return
        end
        local w = words(text)
        local cmd = w[1]
        local px, py, pz = core.player_pos(p)
        if px == nil then
            return
        end
        if cmd == "class" then
            local k = tonumber(w[2]) or 0
            if s.cls ~= 0 or D.CLASS[k] == nil then
                return
            end
            s.cls = k
            s.ranks = { 1, 0, 0, 0 }
            for e, name in D.CLASS[k].gear do
                local it = { b = D.BASE_OF[name], r = 0, l = 1, x = 0, a = {} }
                s.eq[e] = it
            end
            SV.recompute(p, s)
            core.heal(p, 1000)
            s.mana = s.st.mana
            SV.send_list(p, "eq", s.eq, 10)
            tell(p, "picked")
            SV.save(s)
            return
        end
        if s.cls == 0 then
            return
        end
        if cmd == "move" then
            local o, d = SV.read_ray(w, 2)
            if o == nil then
                return
            end
            s.pending = nil
            local id, mob = SV.mob_on_ray(o :: {number}, d :: {number}, { px, py, pz }, true)
            if id and SV.npc[id] then
                SV.go(p, s, { kind = "npc", id = id }, mob[1], mob[2], mob[3])
                return
            end
            s.goal = nil
            s.target = id
            local at = mob or SV.ground_on_ray(o :: {number}, d :: {number})
            if id == nil and at then
                local thing, tx, ty, tz = SV.thing_near(at)
                if thing then
                    SV.go(p, s, thing, tx, ty, tz)
                    return
                end
            end
            if at ~= nil then
                SV.send_way(p, px, py, pz, at)
            end
            s.way_at = clock
        elseif cmd == "pick" then
            local id = tonumber(w[2]) or 0
            local g = SV.ground[id]
            if g then
                SV.go(p, s, { kind = "item", id = id }, g.x, g.y, g.z)
            end
        elseif cmd == "cast" then
            local o, d = SV.read_ray(w, 3)
            if o ~= nil then
                SV.cast(p, s, tonumber(w[2]) or 0, o :: {number}, d :: {number})
            end
        elseif cmd == "drink" then
            if w[2] == "life" and s.lpot > 0 and core.health(p) < s.st.life then
                s.lpot -= 1
                core.heal(p, math.max(1, math.floor(s.st.life * 0.4)))
                tell(p, "drank life")
            elseif w[2] == "mana" and s.mpot > 0 and s.mana < s.st.mana then
                s.mpot -= 1
                s.mana = math.min(s.st.mana, s.mana + s.st.mana * 0.5)
                tell(p, "drank mana")
            end
            s.dirty = true
            tell(p, SV.stat_line(p, s))
        elseif cmd == "town" then
            SV.open_portal(p, s)
        elseif cmd == "stat" then
            local k = ({ "str", "dex", "vit", "ene" })[tonumber(w[2]) or 0]
            if k and s.sp > 0 then
                s.sp -= 1
                s[k] += 1
                SV.recompute(p, s)
                s.dirty = true
            end
        elseif cmd == "skill" then
            local k = tonumber(w[2]) or 0
            local sk = D.CLASS[s.cls].skills[k]
            if sk and s.kp > 0 and s.lvl >= sk.lvl and s.ranks[k] < D.MAX_RANK then
                s.kp -= 1
                s.ranks[k] += 1
                SV.recompute(p, s)
                s.dirty = true
            end
        elseif cmd == "equip" then
            SV.equip(p, s, tonumber(w[2]) or 0)
        elseif cmd == "unequip" then
            SV.unequip(p, s, tonumber(w[2]) or 0)
        elseif cmd == "drop" then
            local k = tonumber(w[2]) or 0
            local it = s.inv[k]
            if it then
                s.inv[k] = nil
                tell(p, `iv {k}`)
                SV.drop_item(it, px, py, pz)
                s.dirty = true
            end
        elseif cmd == "buy" then
            SV.buy(p, s, tonumber(w[2]) or 0)
        elseif cmd == "sell" then
            SV.sell(p, s, tonumber(w[2]) or 0)
        elseif cmd == "stash" or cmd == "unstash" then
            if not s.stash_open then
                return
            end
            local k = tonumber(w[2]) or 0
            local from, to, fw, tw, n = s.inv, s.stash, "iv", "sx", D.STASH
            if cmd == "unstash" then
                from, to, fw, tw, n = s.stash, s.inv, "sx", "iv", D.BAG
            end
            local it = from[k]
            local j = free_slot(to, n)
            if it and j then
                from[k], to[j] = nil, it
                tell(p, `{fw} {k}`)
                tell(p, `{tw} {j} {IT.encode(it)}`)
                s.dirty = true
            end
        elseif cmd == "wp" then
            local k = tonumber(w[2]) or 0
            local wp = L.WPS[k]
            if wp and s.at_wp and bit32.btest(s.wps, bit32.lshift(1, k - 1)) then
                core.player_place(p, wp.x + 0.5, GROUND, wp.z + 3.5)
                s.target, s.goal = nil, nil
            end
        elseif cmd == "close" then
            s.shop, s.stash_open = nil, false
        end
    end

    core.event.on("core:net_message", function(ev)
        asked(ev.player, ev.text)
    end)

    -- ---------------------------------------------------------------- the tick

    -- The creature being gone for: struck while in reach, or shot at from a
    -- bow's range. A pending melee skill is the blow when it lands.
    function SV.step_target(p: number, s, px: number, py: number, pz: number)
        if s.target == nil then
            return
        end
        local x, y, z = core.mob_pos(s.target)
        if x == nil then
            s.target, s.pending = nil, nil
            tell(p, "unchase")
            return
        end
        local ranged = s.st.bow and s.pending == nil
        local reach = if ranged then D.BOW_RANGE else D.REACH
        tell(p, string.format("chase %.2f %.2f %.2f %.1f", x, y, z, reach - 0.6))
        local moved = (x - s.way_to[1]) ^ 2 + (z - s.way_to[3]) ^ 2 > 1
        if clock - s.way_at > 0.5 or (moved and clock - s.way_at > 0.15) then
            s.way_at, s.way_to = clock, { x, y, z }
            SV.send_way(p, px, py, pz, { x, y, z })
        end
        if (x - px) ^ 2 + (z - pz) ^ 2 <= reach * reach and math.abs(y - py) < 3 and clock >= s.next_blow then
            s.next_blow = clock + 0.55 * s.st.spd * 100 / (100 + s.st.ias) *
                          (if s.cls == 1 then D.WARRIOR_PACE else 1)
            if ranged then
                SV.loose(p, s, { x, y + CHEST, z }, nil)
            elseif s.pending then
                local cost = D.CLASS[s.cls].skills[s.pending].cost
                if s.mana >= cost then
                    s.mana -= cost
                    SV.strike(p, s, s.target, s.pending)
                else
                    tell(p, "nomana")
                    SV.strike(p, s, s.target, nil)
                end
                s.pending = nil
            else
                SV.strike(p, s, s.target, nil)
            end
        end
    end

    -- A goal reached: what it was for.
    function SV.step_goal(p: number, s, px: number, py: number, pz: number)
        local g = s.goal
        if g == nil then
            return
        end
        local x, _y, z
        local reach = 1.8
        if g.kind == "npc" then
            x, _y, z = core.mob_pos(g.id)
            reach = 3.2
        elseif g.kind == "item" then
            local it = SV.ground[g.id]
            if it then
                x, _y, z = it.x, it.y, it.z
            end
        elseif g.kind == "stash" then
            x, _y, z = L.STASH[1] + 0.5, GROUND, L.STASH[2] + 0.5
            reach = 2.6
        elseif g.kind == "wp" then
            local w = L.WPS[g.k]
            x, _y, z = w.x + 0.5, GROUND, w.z + 0.5
            reach = 1.6
        elseif g.kind == "chest" then
            local c = L.CHESTS[g.k]
            x, _y, z = c.x + 0.5, GROUND, c.z + 0.5
            reach = 2.4
        end
        if x == nil then
            s.goal = nil
            return
        end
        if (x - px) ^ 2 + (z - pz) ^ 2 > reach * reach then
            return
        end
        s.goal = nil
        if g.kind == "npc" then
            SV.open_shop(p, s, SV.npc[g.id].kind)
        elseif g.kind == "item" then
            SV.pick(p, s, g.id)
        elseif g.kind == "stash" then
            s.stash_open = true
            tell(p, "stash 1")
            SV.send_list(p, "sx", s.stash, D.STASH)
        elseif g.kind == "chest" then
            SV.open_chest(g.k, px, py, pz, p)
        end
    end

    function SV.open_chest(k: number, px: number, py: number, pz: number, p: number)
        if SV.chest_open[k] then
            return
        end
        local c = L.CHESTS[k]
        SV.chest_open[k] = true
        blocks()
        core.set_block(c.x, GROUND, c.z, B.coffer_open)
        local s = SV.players[p]
        local mf = s and s.st.mf or 0
        for _ = 1, math.random(2, 3) do
            SV.drop_item(SV.roll(c.level, mf + 50), c.x + 0.5, GROUND, c.z + 0.5)
        end
        SV.drop(`${math.random(10, 30) * c.level}`, c.x + 0.5, GROUND, c.z + 0.5)
    end

    -- Gold, potions and globes are taken by walking over them.
    function SV.step_ground()
        for id, g in SV.ground do
            local what = IT.loose(g.code)
            if what == nil then
                continue
            end
            for p, s in SV.players do
                local x, y, z = core.player_pos(p)
                if x ~= nil and s.cls > 0 and (x - g.x) ^ 2 + (z - g.z) ^ 2 < 1.6 and math.abs(y - g.y) < 2 then
                    SV.pick(p, s, id)
                    break
                end
            end
        end
    end

    -- The encounters: woken by somebody near, followed while they stand, done
    -- once every one of them is down. One the engine took away for want of
    -- anybody near (a creature goes with nobody within 72) comes back next time.
    function SV.step_encounters()
        local where = {}
        for p in SV.players do
            local x, y, z = core.player_pos(p)
            if x then
                table.insert(where, { x, y, z })
            end
        end
        for k, e in L.ENC do
            local st = SV.enc[k]
            if st == nil then
                st = { woke = false, ids = {}, left = table.clone(e.members), done = false }
                SV.enc[k] = st
            end
            if st.done then
                continue
            end
            if not st.woke then
                for _, w in where do
                    if (w[1] - e.x) ^ 2 + (w[3] - e.z) ^ 2 < 20 * 20 and L.at(w[1], w[3]) ~= 1 then
                        st.woke = true
                        break
                    end
                end
                if st.woke then
                    local lvl = L.AREAS[e.area].level
                    for i, m in st.left do
                        local a = i * 2.4
                        local r = if m[2] >= 2 then 0 else 1.5 + (i % 3)
                        local x, z = e.x + 0.5 + math.cos(a) * r, e.z + 0.5 + math.sin(a) * r
                        if not SV.standable(x, GROUND, z) then
                            x, z = e.x + 0.5, e.z + 0.5
                        end
                        local id = core.spawn_mob(m[1], x, GROUND, z)
                        if id ~= 0 then
                            local info = SV.new_info(m[1], lvl, m[2], if m[2] >= 2 then e.name else nil)
                            SV.mobs[id] = info
                            core.mob_blow_every(id, D.blow_every(lvl))
                            st.ids[id] = { m = m, x = x, z = z }
                        end
                    end
                    st.left = {}
                end
                continue
            end
            for id, held in st.ids do
                local x, y, z = core.mob_pos(id)
                if x ~= nil then
                    held.x, held.z = x, z
                    -- A lord keeps to its lair: one led out after somebody (an
                    -- archer backs off from a body near it, 70 nodes in a
                    -- fight) is put back where it waited.
                    if held.m[2] == 3 and L.at(x, z) ~= e.area then
                        local cx, cy, cz = math.floor(x), math.floor(y + 0.01), math.floor(z)
                        core.push_mobs(cx, cy, cz, e.x - cx, GROUND - cy, e.z - cz)
                    end
                    continue
                end
                st.ids[id] = nil
                SV.mobs[id] = nil
                local dead = SV.killed[id]
                SV.killed[id] = nil
                if not dead then
                    local near = false
                    for _, w in where do
                        if (w[1] - held.x) ^ 2 + (w[3] - held.z) ^ 2 < 55 * 55 then
                            near = true
                        end
                    end
                    if not near then
                        table.insert(st.left, held.m)   -- went for want of anybody: it comes back
                    end
                end
            end
            if next(st.ids) == nil then
                if #st.left == 0 then
                    st.done = true
                else
                    st.woke = false
                end
            end
        end
        -- The things to break: stood up when somebody comes within 24, and
        -- once broken, broken until the world is empty.
        for k, pr in L.PROPS do
            local st = SV.prop_st[k]
            if st == nil then
                st = {}
                SV.prop_st[k] = st
            end
            if st.broken then
                continue
            end
            if st.id then
                if core.mob_pos(st.id) == nil then
                    st.broken = SV.killed[st.id] == true
                    SV.props[st.id], SV.mobs[st.id], SV.killed[st.id] = nil, nil, nil
                    st.id = nil
                end
                continue
            end
            for _, w in where do
                if (w[1] - pr.x - 0.5) ^ 2 + (w[3] - pr.z - 0.5) ^ 2 < 24 * 24 then
                    local id = core.spawn_mob(pr.kind, pr.x + 0.5, GROUND, pr.z + 0.5)
                    if id ~= 0 then
                        st.id = id
                        SV.props[id] = k
                        SV.mobs[id] = SV.new_info(pr.kind, L.AREAS[pr.area].level, 0)
                        core.mob_face(id, pr.x + 0.5 + math.cos(k), pr.z + 0.5 + math.sin(k))
                    end
                    break
                end
            end
        end
    end

    -- The town keeps nothing out of the zones: a creature that follows somebody
    -- in is put back out past the gate (core.mob_ids lists no worker, so the
    -- town's people stay).
    function SV.keep_town_clear()
        for _, id in core.mob_ids(L.TX, GROUND, L.TZ, 32) do
            local x, y, z = core.mob_pos(id)
            if x ~= nil and L.at(x, z) == 1 then
                local cx, cy, cz = math.floor(x), math.floor(y + 0.01), math.floor(z)
                core.push_mobs(cx, cy, cz, L.TX + 25 - cx, GROUND - cy, L.TZ - cz)
            end
        end
    end

    -- What the zone a player stands in does: its level scales what a blow
    -- takes off them, their defence and damage reduction take some back.
    function SV.step_player(p: number, s, dt: number, px: number, py: number, pz: number)
        if s.clip_until and clock >= s.clip_until then
            s.clip_until = nil
            core.player_clip(p, "")
        end
        local n = L.at(px, pz)
        local a = L.AREAS[n]
        local zone = if a then a.zone else s.zone
        if zone ~= s.zone then
            if zone == 0 and s.zone > 0 then
                s.restock = true
            end
            s.zone = zone
        end
        if n == 0 and core.health(p) > 0 then
            -- On the high ground, or nowhere this world has: back to town.
            core.player_place(p, L.SPAWN[1], L.SPAWN[2], L.SPAWN[3])
        end
        local lvl = L.level_at(px, py, pz)
        local red = s.st.def / (s.st.def + 20 + 8 * lvl)
        local hurt = D.dmg_mult(lvl) * (1 - math.min(0.75, red)) * (1 - s.st.dr / 100)
        if clock < s.evade_until then
            hurt = 0
        end
        local walk = 1 + s.st.frw / 100
        local regen = (s.st.life or 20) / 20 * 0.5
        local key = string.format("%.2f %.2f %.2f", walk, hurt, regen)
        if key ~= s.factors then
            s.factors = key
            core.player_factors(p, walk, 1, math.clamp(hurt, 0, 10), math.clamp(regen, 0, 10))
        end
        s.mana = math.min(s.st.mana, s.mana + (1.2 + 0.03 * s.st.ene) * (1 + s.st.mreg / 100) * dt)
        if s.st.lreg > 0 then
            s.lreg_acc += s.st.lreg * dt
            if s.lreg_acc >= 1 then
                core.heal(p, math.floor(s.lreg_acc))
                s.lreg_acc -= math.floor(s.lreg_acc)
            end
        end
        -- At a waypoint: the ones known, and this one known from now on.
        local at_wp = nil
        for k, w in L.WPS do
            if (px - w.x - 0.5) ^ 2 + (pz - w.z - 0.5) ^ 2 < 2.6 then
                at_wp = k
            end
        end
        if at_wp ~= s.at_wp then
            s.at_wp = at_wp
            if at_wp then
                local bitk = bit32.lshift(1, at_wp - 1)
                if not bit32.btest(s.wps, bitk) then
                    s.wps = bit32.bor(s.wps, bitk)
                    SV.msg(p, "WAYPOINT ACTIVATED")
                    s.dirty = true
                end
                tell(p, `wpo 1 {s.wps}`)
            else
                tell(p, "wpo 0")
            end
        end
        -- A shop or the stash shuts when walked away from.
        if s.shop or s.stash_open then
            local far = true
            if s.stash_open and (px - L.STASH[1]) ^ 2 + (pz - L.STASH[2]) ^ 2 < 25 then
                far = false
            end
            if s.shop then
                for _, npc in L.NPCS do
                    if npc.kind == s.shop and (px - npc.x) ^ 2 + (pz - npc.z) ^ 2 < 36 then
                        far = false
                    end
                end
            end
            if far then
                s.shop, s.stash_open = nil, false
                tell(p, "shop -")
                tell(p, "stash 0")
            end
        end
        if s.cls > 0 then
            SV.step_target(p, s, px, py, pz)
            SV.step_goal(p, s, px, py, pz)
        end
    end

    -- Every quarter second: the champions, named ones and lords near each
    -- player, for their machine to ring, and the creature they last went for.
    function SV.tell_elites(p: number, s, px: number, py: number, pz: number)
        local parts = { "els" }
        for _, id in core.mob_ids(px, py, pz, 40) do
            local info = SV.mobs[id]
            if info and info.rank > 0 and #parts < 16 then
                local x, y, z = core.mob_pos(id)
                if x then
                    table.insert(parts, string.format("%.1f,%.1f,%.1f,%d", x, y, z, info.rank))
                end
            end
        end
        tell(p, table.concat(parts, " "))
        local id = s.target or s.last_hit
        local info = id and SV.mobs[id]
        if id and core.mob_pos(id) ~= nil then
            info = info or SV.info(id)
            if info then
                tell(p, string.format("tgt %.3f %d %s", math.clamp(1 - info.taken / info.real, 0, 1), info.rank,
                                      info.name))
            end
        else
            s.last_hit = nil
            tell(p, "tgt")
        end
    end

    -- Nobody left: the world is new again for whoever comes next.
    function SV.reset()
        for id in SV.props do   -- a worker stays where it was put: take the unbroken away
            core.mob_remove(id)
        end
        SV.enc, SV.mobs, SV.killed, SV.proj = {}, {}, {}, {}
        SV.props, SV.prop_st = {}, {}
        SV.ground, SV.portals = {}, {}
        blocks()
        for k in SV.chest_open do
            local c = L.CHESTS[k]
            core.set_block(c.x, GROUND, c.z, B.coffer)
        end
        SV.chest_open = {}
        core.log("arpg_game: nobody here; the zones are new again")
    end

    local stat_at, time_at, save_at, enc_at, npc_at = 0, 0, 0, 0, 0

    function server_step(dt: number)
        -- The sun held low, early in the morning: light enough to see the
        -- zones by, the braziers bright against it. What the sun burns off a
        -- creature (a point a second) is nothing to their thousand.
        if clock >= time_at then
            time_at = clock + 2
            core.set_time(0.33)
        end
        local list = core.players()
        if #list == 0 then
            if SV.was_anybody then
                SV.was_anybody = false
                SV.reset()
            end
            return
        end
        SV.was_anybody = true
        if clock >= npc_at then
            npc_at = clock + 5
            local had = next(SV.npc) ~= nil
            SV.spawn_npcs()
            if not had then
                for p in SV.players do
                    SV.tell_npcs(p)
                end
            end
        end
        local here = {}
        local keep = clock >= save_at
        if keep then
            save_at = clock + 5
        end
        local say = clock >= stat_at
        if say then
            stat_at = clock + 0.25
        end
        for _, p in list do
            here[p] = true
            local s = SV.players[p]
            if s == nil then
                s = SV.load(p)
                SV.players[p] = s
                SV.recompute(p, s)
                core.heal(p, 1000)
                s.mana = s.st.mana
                core.player_spawn(p, L.SPAWN[1], L.SPAWN[2], L.SPAWN[3])
                SV.send_list(p, "iv", s.inv, D.BAG)
                SV.send_list(p, "eq", s.eq, 10)
                for id, g in SV.ground do
                    tell(p, string.format("gi %d %.2f %d %.2f %s", id, g.x, g.y, g.z, g.code))
                end
                for q, t in SV.portals do
                    tell(p, string.format("tp %d %.2f %.2f %.2f %.2f %.2f %.2f", q, t.fx, t.fy, t.fz, t.tx, t.ty, t.tz))
                end
                SV.tell_npcs(p)
                if s.cls == 0 then
                    tell(p, "pick")
                end
            end
            local px, py, pz = core.player_pos(p)
            if px ~= nil then
                SV.step_player(p, s, dt, px, py, pz)
                if say then
                    SV.tell_elites(p, s, px, py, pz)
                end
            end
            if keep and s.dirty then
                SV.save(s)
            end
            if say then
                tell(p, SV.stat_line(p, s))
            end
        end
        for p, s in SV.players do
            if not here[p] then
                SV.save(s)
                SV.close_portal(p)
                SV.players[p] = nil
            end
        end
        if clock >= enc_at then
            enc_at = clock + 0.5
            SV.step_encounters()
        end
        SV.keep_town_clear()
        SV.step_portals()
        SV.step_proj(dt)
        SV.step_ground()
    end
end

-- ===================================================== the player's machine

if drawing then
    core.engine_hud(false)   -- the globes and the belt instead of the hotbar and hearts

    local C = {}
    C.me = 1
    C.skill = 1
    C.held = false
    C.again = 0
    C.dest = nil
    C.chase = nil
    C.chase_left = 0
    C.chase_stop = D.REACH - 0.6
    C.path = {}
    C.marks = {}
    C.fx = {}
    C.proj = {}
    C.ground = {}       -- id -> { code, x, y, z, it }
    C.npcs = {}         -- id -> { kind, x, y, z }
    C.portals = {}      -- owner -> { fx, fy, fz, tx, ty, tz }
    C.elites = {}
    C.inv, C.eq, C.stash, C.stock = {}, {}, {}, {}
    C.stat = { hp = 20, max_hp = 20, mana = 10, max_mana = 10, xp = 0, next = 80, level = 1, gold = 0, lpot = 0,
               mpot = 0, tp = 0, cls = 0, sp = 0, kp = 0 }
    C.sheet = {}
    C.ranks = { 0, 0, 0, 0, 0 }
    C.panel = nil       -- "char", "skills" on the left, or nil
    C.bag_open = false
    C.shop = nil
    C.stash_open = false
    C.wp_open = false
    C.wps = 1
    C.picking = false
    C.target = nil      -- { frac, rank, name }
    C.banner, C.banner_left = "", 0
    C.notice, C.notice_left = "", 0
    C.nomana_left = 0
    C.zone = -1
    C.ctrl = false
    C.regions = {}      -- what the last frame drew that a click can hit
    C.hover = nil       -- the region under the mouse

    local function decoded(code: string?)
        return code and IT.decode(code)
    end

    heard = function(text: string)
        local w = words(text)
        local what = w[1]
        local n = {}
        for i = 2, #w do
            n[i - 1] = tonumber(w[i]) or 0
        end
        if what == "st" then
            local s = C.stat
            s.hp, s.max_hp, s.mana, s.max_mana, s.xp, s.next, s.level = n[1], n[2], n[3], n[4], n[5], n[6], n[7]
            s.gold, s.lpot, s.mpot, s.tp, s.cls, s.sp, s.kp = n[8], n[9], n[10], n[11], n[12], n[13], n[14]
        elseif what == "sh" then
            local keys = { "str", "dex", "vit", "ene", "dmin", "dmax", "def", "ias", "frw", "ls", "mf", "gf", "dr", "sk",
                           "crit", "fire", "life", "mana" }
            for i, k in keys do
                C.sheet[k] = n[i]
            end
        elseif what == "sk" then
            C.ranks = { n[1], n[2], n[3], n[4], n[5] }
        elseif what == "iv" then
            C.inv[n[1]] = decoded(w[3])
        elseif what == "anim" then
            core.player_clip(C.me, w[2])
            C.anim_left = n[2] or 0.5
        elseif what == "eq" then
            C.eq[n[1]] = decoded(w[3])
            D.wear(C.me, n[1], C.eq[n[1]])   -- this machine's own drawing of the body
        elseif what == "sx" then
            C.stash[n[1]] = decoded(w[3])
        elseif what == "shop" then
            C.shop = if w[2] == "-" then nil else w[2]
            C.stock = {}
            if C.shop then
                C.bag_open = true
            end
        elseif what == "sp" then
            C.stock[n[1]] = { price = n[2], code = w[4], it = decoded(w[4]) }
        elseif what == "stash" then
            C.stash_open = n[1] == 1
            if C.stash_open then
                C.bag_open = true
            end
        elseif what == "wpo" then
            C.wp_open = n[1] == 1
            if n[2] then
                C.wps = n[2]
            end
        elseif what == "pick" then
            C.picking = true
        elseif what == "picked" then
            C.picking = false
            C.banner, C.banner_left = D.CLASS[C.stat.cls] and D.CLASS[C.stat.cls].name or "", 2.5
        elseif what == "msg" then
            C.notice, C.notice_left = string.sub(text, 5), 2
        elseif what == "got" then
            if w[2] == "gold" then
                C.notice, C.notice_left = `{w[3]} GOLD`, 1.2
            elseif w[2] == "item" then
                local it = decoded(w[3])
                if it then
                    C.notice, C.notice_left = IT.name(it), 1.5
                end
            else
                C.notice, C.notice_left = D.LOOSE_NAME[w[2]] or w[2], 1.2
            end
        elseif what == "gi" then
            C.ground[n[1]] = { code = w[6], x = n[2], y = n[3], z = n[4], it = decoded(w[6]) }
        elseif what == "gg" then
            C.ground[n[1]] = nil
        elseif what == "npc" then
            C.npcs[n[1]] = { kind = w[3], x = n[3], y = n[4], z = n[5] }
        elseif what == "tp" then
            C.portals[n[1]] = { fx = n[2], fy = n[3], fz = n[4], tx = n[5], ty = n[6], tz = n[7] }
        elseif what == "tpx" then
            C.portals[n[1]] = nil
        elseif what == "els" then
            C.elites = {}
            for i = 2, #w do
                local q = string.split(w[i], ",")
                table.insert(C.elites, { tonumber(q[1]) or 0, tonumber(q[2]) or 0, tonumber(q[3]) or 0,
                                         tonumber(q[4]) or 1 })
            end
        elseif what == "tgt" then
            C.target = if #w >= 4 then { n[1], n[2], table.concat(w, " ", 4) } else nil
        elseif what == "chase" then
            C.chase, C.chase_left = { n[1], n[2], n[3] }, 0.4
            C.chase_stop = n[4] or (D.REACH - 0.6)
            C.dest = nil
        elseif what == "unchase" then
            C.chase = nil
        elseif what == "goto" then
            C.dest, C.chase = { n[1], n[2], n[3] }, nil
        elseif what == "way" then
            C.path = {}
            for i = 2, #w do
                table.insert(C.path, tonumber(w[i]) or 0)
            end
        elseif what == "pj" then
            C.proj[n[1]] = { kind = w[3], x = n[3], y = n[4], z = n[5], vx = n[6], vz = n[7] }
        elseif what == "pp" then
            C.proj[n[1]] = nil
            local kind = w[6]
            if kind == "burst" or kind == "fire" then
                table.insert(C.fx, { kind = "pop", x = n[2], y = n[3], z = n[4], left = 0.45, life = 0.45 })
            end
        elseif what == "fx" then
            local kind = w[2]
            local v = {}
            for i = 3, #w do
                v[i - 2] = tonumber(w[i]) or 0
            end
            local life = ({ slash = 0.3, bash = 0.35, cleave = 0.4, cry = 0.6, nova = 0.5, blink = 0.5, leap = 0.5 })[kind]
                         or 0.3
            if kind == "blink" or kind == "leap" then
                table.insert(C.fx, { kind = "blink", x = v[1], y = v[2], z = v[3], left = 0.5, life = 0.5 })
                table.insert(C.fx, { kind = if kind == "leap" then "cleave" else "blink", x = v[4], y = v[5],
                                     z = v[6], left = 0.5, life = 0.5 })
                C.dest, C.chase, C.path = nil, nil, {}
            else
                table.insert(C.fx, { kind = kind, x = v[1], y = v[2], z = v[3], left = life, life = life })
            end
        elseif what == "zap" then
            table.insert(C.fx, { kind = "zap", a = { n[1], n[2], n[3] }, b = { n[4], n[5], n[6] },
                                 left = 0.3, life = 0.3, seed = math.random(1, 1000) })
        elseif what == "level" then
            C.banner, C.banner_left = `LEVEL {n[1]}`, 2.5
            table.insert(C.fx, { kind = "levelup", on_me = true, left = 1.4, life = 1.4 })
        elseif what == "nomana" then
            C.nomana_left = 1
        elseif what == "drank" then
            C.notice, C.notice_left = if w[2] == "life" then "+ LIFE" else "+ MANA", 0.8
            table.insert(C.fx, { kind = "drank_" .. w[2], on_me = true, left = 0.7, life = 0.7 })
        end
    end

    if not serving then
        core.event.on("core:net_message", function(ev)
            heard(ev.text)
        end)
    end

    local function ask(text: string)
        if serving then
            asked(C.me, text)
        else
            core.net.send(0, text)
        end
    end

    local function ray_text(): string?
        local ox, oy, oz, dx, dy, dz = core.cursor_ray()
        if ox == nil then
            return nil
        end
        return string.format("%.3f %.3f %.3f %.4f %.4f %.4f", ox, oy, oz, dx, dy, dz)
    end

    -- ------------------------------------------------------------ the screen

    -- Where a point of the world is on the screen, as fractions like
    -- core.hud.text's, or nil behind the camera: the camera's eye is where the
    -- cursor's ray starts, and it looks down the way core.iso_view set.
    local YAW, PITCH, FOV = math.rad(135), math.rad(55), math.rad(40)
    local FWD = { math.cos(PITCH) * math.sin(YAW), -math.sin(PITCH), -math.cos(PITCH) * math.cos(YAW) }
    local RIGHT, UP
    do
        -- right = forward x world up, up = right x forward
        local rx, rz = -FWD[3], FWD[1]
        local l = math.sqrt(rx * rx + rz * rz)
        RIGHT = { rx / l, 0, rz / l }
        UP = { RIGHT[2] * FWD[3] - RIGHT[3] * FWD[2], RIGHT[3] * FWD[1] - RIGHT[1] * FWD[3],
               RIGHT[1] * FWD[2] - RIGHT[2] * FWD[1] }
    end
    local eye = nil
    local aspect = 1.78

    local function project(x: number, y: number, z: number): (number?, number?)
        if eye == nil then
            return nil, nil
        end
        local vx, vy, vz = x - eye[1], y - eye[2], z - eye[3]
        local d = vx * FWD[1] + vy * FWD[2] + vz * FWD[3]
        if d < 0.5 then
            return nil, nil
        end
        local t = math.tan(FOV / 2)
        local sx = (vx * RIGHT[1] + vy * RIGHT[2] + vz * RIGHT[3]) / (d * t * aspect)
        local sy = (vx * UP[1] + vy * UP[2] + vz * UP[3]) / (d * t)
        return 0.5 + 0.5 * sx, 0.5 - 0.5 * sy
    end

    -- A box on the screen, middle and size in the height's fractions (as
    -- core.hud.rect takes them), x in the width's; what a click on it does.
    local function region(cx: number, cy: number, w: number, h: number, act, tip)
        table.insert(C.regions, { x0 = cx - w / 2 / aspect, x1 = cx + w / 2 / aspect, y0 = cy - h / 2,
                                  y1 = cy + h / 2, act = act, tip = tip })
    end

    local function region_at(mx: number, my: number)
        for i = #C.regions, 1, -1 do
            local r = C.regions[i]
            if mx >= r.x0 and mx <= r.x1 and my >= r.y0 and my <= r.y1 then
                return r
            end
        end
        return nil
    end

    local function rarity_colour(it): (number, number, number)
        local rc = D.RARITY[it.r]
        return rc[2], rc[3], rc[4]
    end

    -- An item's square: its rarity's border, its kind's letters.
    local function item_square(it, cx: number, cy: number, size: number, dim: boolean)
        local r, g, b = rarity_colour(it)
        core.hud.rect(cx, cy, size, size, r // 3, g // 3, b // 3, 255)
        core.hud.rect(cx, cy, size * 0.86, size * 0.86, 30, 26, 22, 255)
        local cat = D.CAT[D.BASES[it.b].cat]
        local k = if dim then 0.45 else 1
        core.hud.text(cx, cy, cat.icon, size * 0.36, 1, math.floor(r * k), math.floor(g * k), math.floor(b * k), 255)
    end

    local function panel_box(x0: number, y0: number, w: number, h: number, title: string)
        core.hud.rect(x0 + w / 2 / aspect, y0 + h / 2, w, h, 22, 18, 15, 235)
        core.hud.rect(x0 + w / 2 / aspect, y0 + 0.025, w, 0.05, 50, 38, 26, 255)
        core.hud.text(x0 + w / 2 / aspect, y0 + 0.025, title, 0.024, 1, 235, 200, 120, 255)
        -- A panel's whole box takes clicks, so nothing behind it walks.
        region(x0 + w / 2 / aspect, y0 + h / 2, w, h, function() end, nil)
    end

    -- A grid of squares of `list`, `cols` wide; a click does `act(k)`.
    local function item_grid(list, n: number, cols: number, x0: number, y0: number, cell: number, act, price)
        for k = 1, n do
            local col, row = (k - 1) % cols, (k - 1) // cols
            local cx = x0 + (col + 0.5) * cell / aspect
            local cy = y0 + (row + 0.5) * cell
            core.hud.rect(cx, cy, cell * 0.94, cell * 0.94, 40, 34, 28, 255)
            local it = list[k]
            if it then
                local item = if price then it.it else it
                if item then
                    item_square(item, cx, cy, cell * 0.9, false)
                elseif it.code then
                    -- The alchemist's potions and scrolls.
                    local what = IT.loose(it.code)
                    local col3 = if what == "lpot" then { 220, 40, 40 } elseif what == "mpot" then { 50, 80, 240 }
                                 else { 200, 180, 120 }
                    core.hud.rect(cx, cy, cell * 0.5, cell * 0.6, col3[1], col3[2], col3[3], 255)
                end
                region(cx, cy, cell, cell, function() act(k) end, { item = item, code = it.code, price = it.price })
            end
        end
    end

    local function lines_of(tip): { { any } }
        local out = {}
        if tip.item then
            out = IT.lines(tip.item)
        elseif tip.code then
            local what = IT.loose(tip.code)
            out = { { D.LOOSE_NAME[what] or "?", 230, 230, 230 } }
        elseif tip.text then
            for _, t in tip.text do
                table.insert(out, { t, 230, 230, 230 })
            end
        end
        if tip.price then
            table.insert(out, { `PRICE: {tip.price} GOLD`, 240, 200, 60 })
        elseif tip.item and C.shop and tip.sell then
            table.insert(out, { `SELLS FOR {math.max(1, IT.value(tip.item) // 4)} GOLD`, 240, 200, 60 })
        end
        return out
    end

    local function tooltip(tip, mx: number, my: number)
        local lines = lines_of(tip)
        if #lines == 0 then
            return
        end
        local size, gap = 0.019, 0.026
        local longest = 0
        for _, l in lines do
            longest = math.max(longest, #l[1])
        end
        local w = longest * size * 0.62 + 0.03
        local h = #lines * gap + 0.02
        local cx = math.clamp(mx + w / 2 / aspect + 0.01, w / 2 / aspect, 1 - w / 2 / aspect)
        local top = math.clamp(my - h - 0.01, 0.01, 1 - h)
        core.hud.rect(cx, top + h / 2, w, h, 12, 10, 8, 240)
        for i, l in lines do
            core.hud.text(cx, top + 0.01 + (i - 0.5) * gap, l[1], size, 1, l[2], l[3], l[4], 255)
        end
    end

    -- ------------------------------------------------------------ panels

    local BAG_W = 0.62
    local function draw_bag()
        local x0 = 0.98 - BAG_W / aspect
        local y0 = 0.1
        panel_box(x0, y0, BAG_W, 0.66, "INVENTORY")
        -- The paper doll.
        local slots = { { 0.1, 0.2, 0.1, 0.16 }, { 0.52, 0.2, 0.1, 0.16 }, { 0.31, 0.1, 0.08, 0.08 },
                        { 0.31, 0.22, 0.1, 0.13 }, { 0.1, 0.36, 0.08, 0.08 }, { 0.52, 0.36, 0.08, 0.08 },
                        { 0.31, 0.33, 0.1, 0.05 }, { 0.42, 0.1, 0.06, 0.06 }, { 0.2, 0.36, 0.06, 0.06 },
                        { 0.42, 0.36, 0.06, 0.06 } }
        for e, sl in slots do
            local cx, cy = x0 + sl[1] / aspect, y0 + sl[2]
            core.hud.rect(cx, cy, sl[3], sl[4], 44, 36, 30, 255)
            local it = C.eq[e]
            if it then
                item_square(it, cx, cy, math.min(sl[3], sl[4]) * 0.95, false)
                region(cx, cy, sl[3], sl[4], function() ask(`unequip {e}`) end, { item = it })
            else
                core.hud.text(cx, cy, D.EQ_NAME[e], 0.011, 1, 120, 110, 100, 255)
            end
        end
        local cell = 0.058
        item_grid(C.inv, D.BAG, 10, x0 + 0.02 / aspect, y0 + 0.42, cell, function(k)
            if C.ctrl then
                ask(`drop {k}`)
            elseif C.shop then
                ask(`sell {k}`)
            elseif C.stash_open then
                ask(`stash {k}`)
            else
                ask(`equip {k}`)
            end
        end)
        for _, r in C.regions do
            if r.tip and r.tip.item and r.y0 >= y0 + 0.41 then
                r.tip.sell = true
            end
        end
        core.hud.text(x0 + 0.03 / aspect, y0 + 0.64, `GOLD  {C.stat.gold}`, 0.02, 0, 240, 200, 60, 255)
        core.hud.text(x0 + (BAG_W - 0.02) / aspect, y0 + 0.64, "CLICK: WEAR / SELL   CTRL+CLICK: DROP", 0.012, 2,
                      170, 160, 150, 255)
    end

    local LEFT_W = 0.62
    local function left_x0(): number
        return 0.02
    end

    local SHOP_NAME = { smith = "THE SMITH", jeweler = "THE JEWELLER", alchemist = "THE ALCHEMIST" }
    local function draw_shop()
        local x0, y0 = left_x0(), 0.1
        panel_box(x0, y0, LEFT_W, 0.5, SHOP_NAME[C.shop] or "")
        item_grid(C.stock, 24, 8, x0 + 0.04 / aspect, y0 + 0.08, 0.068, function(k) ask(`buy {k}`) end, true)
        core.hud.text(x0 + LEFT_W / 2 / aspect, y0 + 0.47, "CLICK TO BUY - CLICK YOUR ITEMS TO SELL", 0.014, 1, 190,
                      180, 160, 255)
    end

    local function draw_stash()
        local x0, y0 = left_x0(), 0.1
        panel_box(x0, y0, LEFT_W, 0.34, "STASH")
        item_grid(C.stash, D.STASH, 10, x0 + 0.02 / aspect, y0 + 0.07, 0.058, function(k) ask(`unstash {k}`) end)
    end

    local function draw_waypoints()
        local x0, y0 = left_x0(), 0.1
        panel_box(x0, y0, 0.5, 0.08 + #L.WPS * 0.06, "WAYPOINTS")
        for k, w in L.WPS do
            local known = bit32.btest(C.wps, bit32.lshift(1, k - 1))
            local cx, cy = x0 + 0.25 / aspect, y0 + 0.06 + k * 0.06
            core.hud.rect(cx, cy, 0.44, 0.05, if known then 50 else 30, if known then 44 else 28, 30, 255)
            core.hud.text(cx, cy, if known then w.name else "- - -", 0.02, 1, if known then 230 else 110,
                          if known then 220 else 110, if known then 200 else 110, 255)
            if known then
                region(cx, cy, 0.44, 0.05, function() ask(`wp {k}`) end, nil)
            end
        end
    end

    local function draw_char()
        local x0, y0 = left_x0(), 0.1
        local sh = C.sheet
        local c = D.CLASS[C.stat.cls]
        panel_box(x0, y0, LEFT_W, 0.62, "CHARACTER")
        local tx = x0 + 0.04 / aspect
        core.hud.text(tx, y0 + 0.08, `{c and c.name or ""}   LEVEL {C.stat.level}`, 0.022, 0, 235, 200, 120, 255)
        core.hud.text(tx, y0 + 0.115, `EXPERIENCE {C.stat.xp} / {C.stat.next}`, 0.016, 0, 200, 200, 200, 255)
        local names = { "STRENGTH", "DEXTERITY", "VITALITY", "ENERGY" }
        local keys = { "str", "dex", "vit", "ene" }
        for i, nm in names do
            local cy = y0 + 0.13 + i * 0.05
            core.hud.text(tx, cy, nm, 0.019, 0, 220, 210, 190, 255)
            core.hud.text(x0 + 0.34 / aspect, cy, tostring(sh[keys[i]] or 0), 0.019, 1, 255, 255, 255, 255)
            if C.stat.sp > 0 then
                local bx = x0 + 0.42 / aspect
                core.hud.rect(bx, cy, 0.035, 0.035, 80, 120, 60, 255)
                core.hud.text(bx, cy, "+", 0.024, 1, 255, 255, 255, 255)
                region(bx, cy, 0.035, 0.035, function() ask(`stat {i}`) end, nil)
            end
        end
        if C.stat.sp > 0 then
            core.hud.text(tx, y0 + 0.37, `{C.stat.sp} STAT POINTS TO SPEND`, 0.017, 0, 255, 220, 90, 255)
        end
        local rows = {
            `DAMAGE {sh.dmin or 0} - {sh.dmax or 0}`, `DEFENSE {sh.def or 0}`,
            `LIFE {C.stat.max_hp}   MANA {C.stat.max_mana}`,
            `ATTACK SPEED +{sh.ias or 0}%   FASTER RUN +{sh.frw or 0}%`,
            `LIFE STOLEN {sh.ls or 0}%   DAMAGE REDUCED {sh.dr or 0}%`,
            `MAGIC FIND +{sh.mf or 0}%   GOLD FIND +{sh.gf or 0}%`,
            `CRITICAL {sh.crit or 0}%   FIRE +{sh.fire or 0}   SKILLS +{sh.sk or 0}`,
        }
        for i, r in rows do
            core.hud.text(tx, y0 + 0.39 + i * 0.03, r, 0.016, 0, 200, 200, 210, 255)
        end
    end

    local function draw_skills()
        local x0, y0 = left_x0(), 0.1
        local c = D.CLASS[C.stat.cls]
        if c == nil then
            return
        end
        panel_box(x0, y0, LEFT_W, 0.56, "SKILLS")
        local tx = x0 + 0.04 / aspect
        for i, sk in c.skills do
            local cy = y0 + 0.02 + i * 0.1
            local rank = C.ranks[i] or 0
            local open = C.stat.level >= sk.lvl
            local col = if rank > 0 then 255 elseif open then 200 else 120
            core.hud.text(tx, cy, `{i}  {sk.name}`, 0.021, 0, col, col, math.floor(col * 0.85), 255)
            core.hud.text(tx, cy + 0.03, sk.text, 0.013, 0, 170, 170, 180, 255)
            core.hud.text(tx, cy + 0.055, if rank > 0 then `RANK {rank} (+{C.ranks[5] or 0})   {sk.cost} MANA`
                          elseif open then "NOT LEARNED YET" else `NEEDS LEVEL {sk.lvl}`, 0.014, 0, 150, 170, 255, 255)
            if C.stat.kp > 0 and open and rank < D.MAX_RANK then
                local bx = x0 + (LEFT_W - 0.06) / aspect
                core.hud.rect(bx, cy + 0.02, 0.035, 0.035, 80, 120, 60, 255)
                core.hud.text(bx, cy + 0.02, "+", 0.024, 1, 255, 255, 255, 255)
                region(bx, cy + 0.02, 0.035, 0.035, function() ask(`skill {i}`) end, nil)
            end
        end
        if C.stat.kp > 0 then
            core.hud.text(tx, y0 + 0.53, `{C.stat.kp} SKILL POINTS TO SPEND`, 0.017, 0, 255, 220, 90, 255)
        end
    end

    local function draw_picker()
        core.hud.rect(0.5, 0.5, 4, 2, 0, 0, 0, 170)
        core.hud.text(0.5, 0.2, "CHOOSE YOUR CLASS", 0.05, 1, 235, 200, 120, 255)
        for i, c in D.CLASS do
            local cx = 0.5 + (i - 2) * 0.36 / aspect * 1.1
            local cy = 0.5
            local hover = C.hover and C.hover.cls == i
            core.hud.rect(cx, cy, 0.34, 0.4, if hover then 70 else 40, if hover then 55 else 32, 30, 245)
            core.hud.text(cx, cy - 0.15, c.name, 0.032, 1, 235, 200, 120, 255)
            core.hud.text(cx, cy - 0.09, `STR {c.str}  DEX {c.dex}`, 0.016, 1, 220, 210, 190, 255)
            core.hud.text(cx, cy - 0.06, `VIT {c.vit}  ENE {c.ene}`, 0.016, 1, 220, 210, 190, 255)
            for k, sk in c.skills do
                core.hud.text(cx, cy - 0.02 + k * 0.03, sk.name, 0.015, 1, 160, 180, 255, 255)
            end
            core.hud.text(cx, cy + 0.16, c.text, 0.0105, 1, 200, 200, 200, 255)
            region(cx, cy, 0.34, 0.4, function() ask(`class {i}`) end, nil)
            C.regions[#C.regions].cls = i
        end
    end

    -- ------------------------------------------------------------ the world

    local function ring(x: number, y: number, z: number, r: number, n: number, size: number,
                        cr: number, cg: number, cb: number)
        for i = 0, n - 1 do
            local a = i * 2 * math.pi / n
            core.render.box(x + math.cos(a) * r, y, z + math.sin(a) * r, size, size, size, cr, cg, cb, 255)
        end
    end

    local function draw_portal(x: number, y: number, z: number)
        local t = clock * 3
        for i = 0, 5 do
            local a = t + i
            core.render.box(x + math.cos(a) * 0.5, y + 0.3 + (i * 0.35 + t * 0.3) % 2, z + math.sin(a) * 0.15,
                            0.1, 0.1, 0.1, 200, 230, 255, 255)
        end
    end

    core.render.pass("arpg_game:world", 100, function()
        for _, m in C.marks do
            ring(m.x, m.y + 0.05, m.z, 0.2 + (0.5 - m.left), 10, 0.08, 120, 255, 120)
        end
        if C.chase then
            ring(C.chase[1], C.chase[2] + 0.05, C.chase[3], 0.7, 14, 0.08, 255, 50, 30)
        end
        for _, e in C.elites do
            local r, g, b = 90, 120, 255
            if e[4] == 2 then
                r, g, b = 255, 210, 60
            elseif e[4] == 3 then
                r, g, b = 255, 120, 30
            end
            ring(e[1], e[2] + 0.06, e[3], 0.8, 16, 0.07, r, g, b)
        end
        for _, b in C.proj do
            if b.kind == "arrow" then
                local l = math.sqrt(b.vx * b.vx + b.vz * b.vz)
                for k = 0, 3 do
                    local f = k * 0.18 / l
                    core.render.box(b.x - b.vx * f, b.y, b.z - b.vz * f, 0.08, 0.08, 0.08, 220, 200, 160, 255)
                end
            end
        end
        for _, g in C.ground do
            local bob = 0.3 + math.sin(clock * 3 + g.x) * 0.08
            local what = IT.loose(g.code)
            if what == "life" then
                core.render.box(g.x, g.y + bob, g.z, 0.35, 0.35, 0.35, 230, 30, 30, 255)
            elseif what == "mana" then
                core.render.box(g.x, g.y + bob, g.z, 0.35, 0.35, 0.35, 40, 80, 255, 255)
            elseif what == "gold" then
                core.render.box(g.x, g.y + 0.06, g.z, 0.3, 0.12, 0.3, 240, 200, 50, 255)
                core.render.box(g.x + 0.1, g.y + 0.16, g.z - 0.05, 0.15, 0.1, 0.15, 255, 225, 90, 255)
            elseif what then
                local r, gg, b = 200, 180, 120
                if what == "lpot" then
                    r, gg, b = 220, 30, 40
                elseif what == "mpot" then
                    r, gg, b = 50, 70, 240
                end
                core.render.box(g.x, g.y + 0.15, g.z, 0.22, 0.26, 0.22, r, gg, b, 255)
                core.render.box(g.x, g.y + 0.34, g.z, 0.08, 0.12, 0.08, 200, 190, 170, 255)
            elseif g.it then
                local r, gg, b = rarity_colour(g.it)
                core.render.box(g.x, g.y + 0.1, g.z, 0.45, 0.2, 0.3, r, gg, b, 255)
                if g.it.r >= 2 then
                    core.render.box(g.x, g.y + 1.2, g.z, 0.06, 2.2, 0.06, r, gg, b, 140)
                end
            end
        end
        for _, t in C.portals do
            draw_portal(t.fx, t.fy, t.fz)
            draw_portal(t.tx, t.ty, t.tz)
        end
    end)

    -- The light of spells and blows (core.render.fx, the engine's effect
    -- shaders), after everything solid: added onto it, hidden by walls.
    local FX = {}
    FX.fire = { 255, 140, 40 }
    FX.ice = { 120, 200, 255 }
    FX.storm = { 150, 180, 255 }
    FX.gold = { 255, 200, 90 }
    FX.blade = { 255, 225, 180 }
    FX.arcane = { 180, 100, 255 }
    function FX.fx(kind: string, x: number, y: number, z: number, size: number, c, t: number, a: number?)
        core.render.fx(kind, x, y, z, size, c[1], c[2], c[3], a or 255, t)
    end
    core.render.pass("arpg_game:fx", 110, function()
        for _, b in C.proj do
            if b.kind ~= "arrow" then
                local l = math.sqrt(b.vx * b.vx + b.vz * b.vz)
                for k = 3, 1, -1 do   -- the trail first, the head over it
                    local f = k * 0.3 / l
                    FX.fx("glow", b.x - b.vx * f, b.y, b.z - b.vz * f, 0.5 - k * 0.08, FX.fire, k * 0.22)
                end
                FX.fx("flame", b.x, b.y, b.z, 0.5, FX.fire, 0)
                FX.fx("glow", b.x, b.y, b.z, 1.1, FX.fire, 0.3, 150)
            end
        end
        for _, t in C.portals do
            for _, p in { { t.fx, t.fy, t.fz }, { t.tx, t.ty, t.tz } } do
                FX.fx("vortex", p[1], p[2] + 1.1, p[3], 1.0, { 90, 140, 255 }, 0.5)
                FX.fx("glow", p[1], p[2] + 1.1, p[3], 1.6, { 60, 110, 255 }, 0.4, 140)
                FX.fx("ring", p[1], p[2], p[3], 1.2, { 90, 140, 255 }, 0.7 + 0.1 * math.sin(clock * 3))
            end
        end
        local mx, my, mz = core.player_pos(C.me)
        for _, e in C.fx do
            local k = 1 - e.left / e.life
            if e.on_me and mx then
                e.x, e.y, e.z = mx, my, mz
            end
            if e.x == nil and e.a == nil then
                continue
            end
            if e.kind == "pop" then
                FX.fx("flame", e.x, e.y + 0.3, e.z, 0.6 + k * 2.2, FX.fire, k)
                FX.fx("ring", e.x, (e.y or 0) - 0.9, e.z, 2.6, FX.fire, k)
                FX.fx("sparks", e.x, e.y, e.z, 2.2, { 255, 200, 80 }, k)
            elseif e.kind == "nova" then
                FX.fx("frost", e.x, e.y, e.z, 6, FX.ice, k)
                FX.fx("ring", e.x, e.y, e.z, 6, { 200, 240, 255 }, k)
                FX.fx("glow", e.x, e.y + 1, e.z, 1.6, FX.ice, k, 160)
            elseif e.kind == "cry" then
                FX.fx("ring", e.x, e.y, e.z, 7, { 255, 160, 80 }, k)
                FX.fx("ring", e.x, e.y, e.z, 5, { 255, 220, 140 }, math.min(1, k * 1.3))
                FX.fx("glow", e.x, e.y + 1.4, e.z, 1.4, { 255, 170, 90 }, k)
            elseif e.kind == "cleave" then
                FX.fx("slash", e.x, e.y + 1, e.z, 2.4, FX.blade, k)
                FX.fx("ring", e.x, e.y, e.z, 3.2, { 255, 240, 200 }, k)
            elseif e.kind == "zap" then
                core.render.fx("bolt", e.a[1], e.a[2], e.a[3], 0.35, FX.storm[1], FX.storm[2], FX.storm[3], 255, k,
                               e.b[1], e.b[2], e.b[3])
                FX.fx("glow", e.b[1], e.b[2], e.b[3], 0.9, FX.storm, k)
                FX.fx("sparks", e.b[1], e.b[2], e.b[3], 0.8, { 200, 220, 255 }, k)
            elseif e.kind == "slash" or e.kind == "bash" then
                local big = if e.kind == "bash" then 1.5 else 1
                FX.fx("slash", e.x, e.y, e.z, 0.9 * big, FX.blade, k)
                FX.fx("sparks", e.x, e.y, e.z, 0.7 * big, if big > 1 then FX.gold else { 255, 240, 220 }, k)
            elseif e.kind == "blink" then
                FX.fx("vortex", e.x, e.y + 1, e.z, 1.2, FX.arcane, k)
                FX.fx("glow", e.x, e.y + 1, e.z, 1.4, FX.arcane, k, 180)
            elseif e.kind == "levelup" then
                FX.fx("ring", e.x, e.y, e.z, 2.5, FX.gold, k)
                FX.fx("vortex", e.x, e.y + 1, e.z, 1.3, FX.gold, k, 200)
                FX.fx("sparks", e.x, e.y + 1, e.z, 1.8, { 255, 230, 140 }, k)
            elseif e.kind == "drank_life" or e.kind == "drank_mana" then
                local c = if e.kind == "drank_life" then { 255, 70, 60 } else { 70, 110, 255 }
                FX.fx("glow", e.x, e.y + 1, e.z, 1.3, c, k, 200)
                FX.fx("ring", e.x, e.y, e.z, 1.2, c, k)
            end
        end
    end)

    -- ------------------------------------------------------------ the HUD

    -- A globe of life or mana: rows of a circle, the full part from the
    -- bottom up, the rest dark.
    local function globe(cx: number, cy: number, r: number, full: number, cr: number, cg: number, cb: number)
        local rows = 28
        local h = 2 * r / rows
        core.hud.rect(cx, cy, 2 * r + 0.012, 2 * r + 0.012, 30, 24, 20, 230)
        for i = 0, rows - 1 do
            local y = cy + r - (i + 0.5) * h
            local half = math.sqrt(math.max(0, r * r - (y - cy) ^ 2))
            local lit = (i + 0.5) / rows <= full
            if lit then
                core.hud.rect(cx, y, 2 * half, h * 1.05, cr, cg, cb, 255)
            else
                core.hud.rect(cx, y, 2 * half, h * 1.05, cr // 5, cg // 5, cb // 5, 255)
            end
        end
    end

    -- A name over something in the world; one that would cover another goes
    -- below it, so a heap of drops reads as a list.
    local placed = {}
    local function label(x: number, y: number, text: string, size: number, r: number, g: number, b: number, act, tip)
        local w = #text * size * 0.62 + 0.012
        local h = size * 1.5
        local moved = true
        while moved do
            moved = false
            for _, q in placed do
                if math.abs(q[1] - x) * aspect < (q[3] + w) / 2 and math.abs(q[2] - y) < h then
                    y = q[2] + h + 0.002
                    moved = true
                end
            end
        end
        table.insert(placed, { x, y, w })
        core.hud.rect(x, y, w, size * 1.5, 10, 8, 6, 200)
        core.hud.text(x, y, text, size, 1, r, g, b, 255)
        if act then
            region(x, y, w, size * 1.5, act, tip)
        end
    end

    core.hud.pass("arpg_game:hud", 100, function()
        aspect = core.hud.aspect()
        if aspect <= 0 then
            aspect = 1.78
        end
        local ox, oy, oz = core.cursor_ray()
        eye = if ox then { ox, oy, oz } else nil
        C.regions = {}
        placed = {}
        local s = C.stat
        local mx, my = core.hud.mouse()

        -- The world's labels first, so a panel over them takes the click.
        local px, py, pz = core.player_pos(C.me)
        if px then
            for id, g in C.ground do
                if (g.x - px) ^ 2 + (g.z - pz) ^ 2 < 400 then
                    local sx, sy = project(g.x, g.y + 0.45, g.z)
                    if sx and sx > 0 and sx < 1 and sy > 0 and sy < 1 then
                        local what = IT.loose(g.code)
                        if g.it then
                            local r, gg, b = rarity_colour(g.it)
                            label(sx, sy, IT.name(g.it), 0.016, r, gg, b, function() ask(`pick {id}`)
                                C.dest = { g.x, g.y, g.z } end, { item = g.it })
                        elseif what == "lpot" or what == "mpot" or what == "tp" then
                            label(sx, sy, D.LOOSE_NAME[what], 0.014, 230, 230, 230, function() ask(`pick {id}`)
                                C.dest = { g.x, g.y, g.z } end, nil)
                        elseif what == "gold" then
                            label(sx, sy, `{select(2, IT.loose(g.code))} GOLD`, 0.014, 240, 200, 60, nil, nil)
                        end
                    end
                end
            end
            for _, n in C.npcs do
                local sx, sy = project(n.x, n.y + 2.3, n.z)
                if sx and (n.x - px) ^ 2 + (n.z - pz) ^ 2 < 900 then
                    local name = ({ smith = "THE SMITH", jeweler = "THE JEWELLER", alchemist = "THE ALCHEMIST",
                                    guard = "THE GATE WARDEN" })[n.kind] or ""
                    label(sx, sy, name, 0.016, 220, 220, 160, nil, nil)
                end
            end
            for _, w in L.WPS do
                if (w.x - px) ^ 2 + (w.z - pz) ^ 2 < 100 then
                    local sx, sy = project(w.x + 0.5, GROUND + 0.8, w.z + 0.5)
                    if sx then
                        label(sx, sy, "WAYPOINT", 0.014, 140, 180, 255, nil, nil)
                    end
                end
            end
            if (L.STASH[1] - px) ^ 2 + (L.STASH[2] - pz) ^ 2 < 100 then
                local sx, sy = project(L.STASH[1] + 0.5, GROUND + 1.4, L.STASH[2] + 0.5)
                if sx then
                    label(sx, sy, "STASH", 0.014, 230, 200, 120, nil, nil)
                end
            end
            for owner, t in C.portals do
                for _, e in { { t.fx, t.fy, t.fz }, { t.tx, t.ty, t.tz } } do
                    if (e[1] - px) ^ 2 + (e[3] - pz) ^ 2 < 400 then
                        local sx, sy = project(e[1], e[2] + 2.6, e[3])
                        if sx then
                            label(sx, sy, if owner == C.me then "TOWN PORTAL" else "A TOWN PORTAL", 0.014, 140, 180,
                                  255, nil, nil)
                        end
                    end
                end
            end
            -- The zone, said on the way in.
            local a = L.AREAS[L.at(px, pz)]
            if a and a.zone ~= C.zone then
                C.zone = a.zone
                C.banner, C.banner_left = L.ZONE[a.zone], 2.5
            end
        end

        local r = 0.085
        local lx, rx, gy = 0.03 + r / aspect, 0.97 - r / aspect, 0.98 - r
        globe(lx, gy, r, math.clamp(s.hp / math.max(1, s.max_hp), 0, 1), 200, 20, 20)
        globe(rx, gy, r, math.clamp(s.mana / math.max(1, s.max_mana), 0, 1), 40, 70, 230)
        core.hud.text(lx, gy - r - 0.03, `{s.hp} / {s.max_hp}`, 0.024, 1, 255, 200, 190, 255)
        core.hud.text(rx, gy - r - 0.03, `{s.mana} / {s.max_mana}`, 0.024, 1, 190, 210, 255, 255)
        core.hud.text(lx, gy - r - 0.062, `Q  {s.lpot} LIFE`, 0.016, 1, 255, 150, 140, 255)
        core.hud.text(rx, gy - r - 0.062, `R  {s.mpot} MANA`, 0.016, 1, 150, 170, 255, 255)
        core.hud.text(rx, gy - r - 0.088, `B  {s.tp} TOWN PORTAL`, 0.016, 1, 200, 220, 255, 255)

        -- The skills, the chosen one lit.
        local c = D.CLASS[s.cls]
        if c then
            local w = 0.22
            for i, sk in c.skills do
                local x = 0.5 + (i - 2.5) * (w + 0.01) / aspect
                local chosen = i == C.skill
                local rank = C.ranks[i] or 0
                local ok = rank > 0 and s.mana >= sk.cost
                core.hud.rect(x, 0.935, w, 0.06, if chosen then 120 else 40, if chosen then 90 else 34,
                              if chosen then 40 else 30, 220)
                core.hud.text(x, 0.925, `{i}  {sk.name}`, 0.015, 1, if ok then 255 else 130, if ok then 235 else 130,
                              if ok then 200 else 130, 255)
                core.hud.text(x, 0.95, if rank > 0 then `RANK {rank}  {sk.cost} MANA` else `LEVEL {sk.lvl}`, 0.012, 1,
                              150, 170, 255, 255)
            end
        end
        local bx0, bx1 = lx + (r + 0.02) / aspect, rx - (r + 0.02) / aspect
        local bw = (bx1 - bx0) * aspect
        core.hud.rect((bx0 + bx1) / 2, 0.985, bw, 0.008, 40, 36, 30, 230)
        local f = math.clamp(s.xp / math.max(1, s.next), 0, 1)
        if f > 0 then
            core.hud.rect(bx0 + (bx1 - bx0) * f / 2, 0.985, bw * f, 0.008, 230, 190, 60, 255)
        end
        core.hud.text(0.5, 0.875, `LEVEL {s.level}`, 0.026, 1, 230, 190, 60, 255)
        core.hud.text(0.5, 0.845, `{s.gold} GOLD`, 0.02, 1, 240, 200, 60, 255)
        if s.sp > 0 or s.kp > 0 then
            core.hud.text(0.5, 0.815, `POINTS TO SPEND: {s.sp} STAT (C)  {s.kp} SKILL (K)`, 0.016, 1, 255, 220, 90, 255)
        end
        core.hud.text(0.98, 0.03, "LMB WALK / ATTACK   RMB SKILL   1-4 SKILL   Q R POTIONS   B PORTAL   I BAG   C STATS   K SKILLS",
                      0.016, 2, 200, 200, 200, 200)
        if C.zone >= 0 and px then
            local lvl = L.level_at(px, py, pz)
            core.hud.text(0.02, 0.03, if C.zone == 0 then L.ZONE[0] else `{L.ZONE[C.zone]}  -  LEVEL {lvl}`, 0.02,
                          0, 220, 210, 180, 255)
        end

        -- What is being fought.
        if C.target then
            local t = C.target
            local tr, tg, tb = 230, 230, 230
            if t[2] == 1 then
                tr, tg, tb = 120, 150, 255
            elseif t[2] == 2 then
                tr, tg, tb = 255, 215, 70
            elseif t[2] == 3 then
                tr, tg, tb = 255, 140, 40
            end
            core.hud.rect(0.5, 0.05, 0.5, 0.035, 60, 10, 10, 220)
            if t[1] > 0 then
                core.hud.rect(0.5 - 0.25 / aspect * (1 - t[1]), 0.05, 0.5 * t[1], 0.035, 170, 20, 20, 255)
            end
            core.hud.text(0.5, 0.05, t[3], 0.018, 1, tr, tg, tb, 255)
        end

        if C.notice_left > 0 then
            core.hud.text(0.5, 0.7, C.notice, 0.024, 1, 255, 230, 140, math.floor(255 * math.min(1, C.notice_left * 2)))
        end
        if C.banner_left > 0 then
            core.hud.text(0.5, 0.25, C.banner, 0.06, 1, 255, 210, 80, math.floor(255 * math.min(1, C.banner_left * 2)))
        end
        if C.nomana_left > 0 then
            core.hud.text(0.5, 0.74, "NOT ENOUGH MANA", 0.026, 1, 120, 150, 255,
                          math.floor(255 * math.min(1, C.nomana_left * 2)))
        end

        -- The panels.
        if C.shop then
            draw_shop()
        elseif C.stash_open then
            draw_stash()
        elseif C.wp_open then
            draw_waypoints()
        elseif C.panel == "char" then
            draw_char()
        elseif C.panel == "skills" then
            draw_skills()
        end
        if C.bag_open then
            draw_bag()
        end
        if C.picking then
            draw_picker()
        end
        C.hover = if mx then region_at(mx, my) else nil
        if C.hover and C.hover.tip and mx then
            tooltip(C.hover.tip, mx, my)
        end
    end)

    -- ------------------------------------------------------------ input

    -- The left button: walk to the ground under the cursor at once, and ask
    -- the server whether there is a creature or a thing there instead.
    local function click(fresh: boolean)
        local ray = ray_text()
        if ray == nil then
            return
        end
        local ox, oy, oz, dx, dy, dz = core.cursor_ray()
        local _, _, _, dist = core.raycast(ox, oy, oz, dx, dy, dz, 120)
        if dist ~= nil then
            C.dest = { ox + dx * dist, oy + dy * dist, oz + dz * dist }
            C.chase, C.path = nil, {}
            if fresh then
                table.insert(C.marks, { x = C.dest[1], y = C.dest[2], z = C.dest[3], left = 0.5 })
            end
        end
        ask(`move {ray}`)
    end

    local function over_ui(): any
        local mx, my = core.hud.mouse()
        return mx and region_at(mx, my)
    end

    core.event.on("core:input", function(ev)
        C.me = ev.player
        local a = ev.action
        if a == "mouse_left" then
            ev.cancel = true
            local r = over_ui()
            if r then
                r.act()
                return
            end
            if C.picking then
                return
            end
            C.held, C.again = true, 0.15
            click(true)
        elseif a == "mouse_left_up" then
            C.held = false
        elseif a == "mouse_right" then
            ev.cancel = true
            local r = over_ui()
            if r then
                if r.tip and r.tip.item then
                    r.act()
                end
                return
            end
            local ray = ray_text()
            if ray ~= nil and not C.picking then
                ask(`cast {C.skill} {ray}`)
            end
        elseif a == "key_ctrl" then
            C.ctrl = true
        elseif a == "key_ctrl_up" then
            C.ctrl = false
        elseif a == "key_q" then
            ask("drink life")
        elseif a == "key_r" then
            ask("drink mana")
        elseif a == "key_b" then
            ask("town")
        elseif a == "key_i" then
            C.bag_open = not C.bag_open
            if not C.bag_open and (C.shop or C.stash_open) then
                C.shop, C.stash_open = nil, false
                ask("close")
            end
        elseif a == "key_c" then
            C.panel = if C.panel == "char" then nil else "char"
        elseif a == "key_k" then
            C.panel = if C.panel == "skills" then nil else "skills"
        elseif a == "key_w" or a == "key_a" or a == "key_s" or a == "key_d" then
            C.dest, C.chase, C.path = nil, nil, {}
        elseif a == "key_1" or a == "key_2" or a == "key_3" or a == "key_4" then
            C.skill = tonumber(string.sub(a, 5)) :: number
        end
    end)

    -- ---------------------------------------------------------------- walking

    local camera_set = false

    function client_step(dt: number)
        if not camera_set then
            camera_set = true
            core.iso_view(30, 135, 55, 40, 0)
        end
        if C.held then
            C.again -= dt
            if C.again <= 0 then
                C.again = 0.15
                if not over_ui() then
                    click(false)
                end
            end
        end
        C.chase_left -= dt
        if C.chase_left <= 0 then
            C.chase = nil
        end

        -- Towards the creature (to striking distance) or the point clicked,
        -- round what is in the way by the corners the server found ("way").
        local x, _, z = core.player_pos(C.me)
        local goal = C.chase or C.dest
        if x == nil or goal == nil then
            core.walk(0, 0)
            C.path = {}
        else
            while #C.path >= 2 and (C.path[1] - x) ^ 2 + (C.path[2] - z) ^ 2 < 0.16 do
                table.remove(C.path, 1)
                table.remove(C.path, 1)
            end
            local dx, dz = goal[1] - x, goal[3] - z
            local d = math.sqrt(dx * dx + dz * dz)
            local stop = if C.chase then C.chase_stop else 0.2
            if d <= stop then
                core.walk(0, 0)
                C.dest, C.path = nil, {}
            elseif #C.path >= 4 then
                core.walk(C.path[1] - x, C.path[2] - z)
            else
                core.walk(dx, dz, math.clamp(d - stop + 0.3, 0.3, 1))
            end
        end

        for _, b in C.proj do
            b.x += b.vx * dt
            b.z += b.vz * dt
        end
        for i = #C.marks, 1, -1 do
            C.marks[i].left -= dt
            if C.marks[i].left <= 0 then
                table.remove(C.marks, i)
            end
        end
        for i = #C.fx, 1, -1 do
            C.fx[i].left -= dt
            if C.fx[i].left <= 0 then
                table.remove(C.fx, i)
            end
        end
        C.banner_left -= dt
        C.notice_left -= dt
        if C.anim_left and C.anim_left > 0 then
            C.anim_left -= dt
            if C.anim_left <= 0 then
                core.player_clip(C.me, "")
            end
        end
        C.nomana_left -= dt
    end
end

function on_step(dt: number)
    clock += dt
    if serving then
        server_step(dt)
    end
    if drawing then
        client_step(dt)
    end
end
