# Commands

*[Leggi in italiano](COMMANDS.it.md) · [Leia em português](COMMANDS.pt.md)*

Every command a player can type in chat (T, then `/name`), what it is and what it does.
`/help` in the game lists the ones you may use where you are; the list changes with the
world, because most commands come from its mods.

The server's console takes the administration commands too, with or without the `/`, as
the owner. Durations are `45s`, `30m`, `2h`, `7d`, `1w`. NAME is a player online in any
room, the owner of that name on this world, or the bare name.

**Permission** is who may use it. "Anybody" needs nothing; the others name a permission,
and the role that has it by default (helper < moderator < admin < owner, each with
everything below it). `/perm` shows and changes which role a permission wants;
[`ADMIN.md`](ADMIN.md) explains roles.

## For everybody

### Chat and information

| Command | Permission | What it does |
|---|---|---|
| `/help` | anybody | The commands you may use in this world, one line each |
| `/list` | anybody | Who is in this world |
| `/me <action>` | anybody | Says `* name action` to everybody (`mods/whisper`) |
| `/msg <player> <text>` | anybody | Sends the text to that player only (`mods/whisper`) |
| `/seen NAME` | anybody | When that player was last here, and how long they have played in all |
| `/where NAME` | anybody | Whether that player is here and in which room, or on which friend server |
| `/fed TEXT` | anybody | Chat to the players of every friend server |
| `/report NAME what happened` | anybody | A report for the staff, told at once to those online |
| `/perms` | anybody | Your role here |
| `/rules` | anybody | The server's rules |
| `/accept` | anybody | Accepts the server's rules, which a server that has them asks of everybody |
| `/leave` | anybody | Leaves the server without accepting the rules |
| `/page [NAME]` | anybody | The in-game pages: with no name the list (`server`, `mods`, `here`, `rules` and the mods' own), with one that page |

### Playing

| Command | Permission | What it does |
|---|---|---|
| `/kill` | anybody | You die and come back at your bed or the world's spawn; armour wears a little, as from any blow. For a body stuck somewhere. Works in creative mode too (`mods/creative`) |
| `/giveup` | anybody, in a world with a trial | Gives up that world's trial: you die, then are walked out through the world's way in and come back on the sandbox, outside that world's gate. A body that cannot be hurt is only walked out. What the trial already counted (a key earned) stays (`mods/keys`) |
| `/journal` | anybody | Opens the engineer's pages of the keys you carry, one page a key and a last page when you have all seven (`mods/keys`) |
| `/town [town\|city]` | anybody | Takes you to the square of the nearest village; with any word after it, of the nearest town (a bigger village, searched twice as far); with `city`, of the nearest city (`mods/villages`) |
| `/creative TEXT` | creative (moderator) | Opens the creative catalogue narrowed to items whose name has the text (`mods/creative`) |
| `/zone [save\|set\|delete\|propose\|approve\|refuse\|try\|show] ...` | anybody; `approve`, `refuse`, `try` and `show` admin (`try` and `show` anybody in their own workshop) | Pieces of levels (`mods/zones`). Alone, lists your zones and the approved ones. `save WORLD NAME X1 Y1 Z1 X2 Y2 Z2 [start\|room\|end]` saves that box as the zone WORLD.NAME, yours: doorway blocks in its sides become its doorways, marker blocks its markers. `set ID d MIN MAX \| weight N \| mirror on\|off \| role ...` says what it is for, `delete ID` removes it. An admin's `approve ID` puts it in everybody's levels and `refuse ID` (or `private`) takes it back; `propose ID` asks the admins for it (`/page zones`); `try WORLD [COUNT [SEED [D]]]` lays a level of that world's zones beside you, `show ID` that zone alone |
| `/workshop [leave\|blocks]` | anybody with a key | Into your own workshop (`mods/workshop`), a flat room where zones are built with a wand, a brush and a panel, tried and proposed; `leave` goes out to the first room's spawn; `blocks` opens the brush's palette |
| `/myroom [whitelist\|blacklist\|add NAME\|remove NAME\|look yes\|no]` | anybody with a key | Who may come into your own room (`mods/home`): only the names on its list (whitelist) or everybody but them (blacklist, the default), and whether the others may look in through your frame. Alone, says the rule |
| `/myroom leave` | anybody, in a player's own room | Out of the room, to the spawn of the server's first room: the way out when its frame is gone |

### In one world only

| Command | World | Permission | What it does |
|---|---|---|---|
| `/garage` | track | anybody | Opens the garage: the colours and parts of your kart; a click changes it at once, on the road too, and it is kept |
| `/ghost [record\|mine\|NAME\|off]` | track | anybody | Which ghost kart drives your laps: the track's record (the default; the engineer's on the Inferno track), your own best lap, another player's best lap, or none. Kept |
| `/laps` | track | anybody | Everybody's best lap on the track you last drove: the first ten and your own place, and who has a ghost |
| `/pieces` | track | anybody | Puts in your bag a stack of every track piece, the track editor (B with it in hand picks the next piece from afar), and the start line, checkpoint, off-road, item box and portal tools |
| `/manta` | cloud world | anybody | How many times you have beaten the Cloud Manta |
| `/vehicle <jeep\|tank\|heli\|plane\|bomber\|nest\|mortar\|howitzer\|aa>` | front, assault | moderator | Puts that machine six nodes in front of you, for testing |
| `/arpg <town\|butcher\|grave\|emberwing\|crypt\|floor1\|floor2\|keeper\|level N\|gold [N]>` | arpg | admin | Test shortcuts: to the town, an area's boss, the crypt or one of its floors; your level up to N (a class picked first); N gold (1000) |
| `/backrooms <pink\|hotel\|party\|pool\|slide\|portal\|exit\|suburb\|shift\|kit>` | backrooms | admin | Test shortcuts: to the pink door, the nearest hotel or party district, the pool floor, a slide, a portal site, the exit, a street of the endless suburb (a world 96 high); `shift` moves the portal links on at once; `kit` gives both compasses, the torch and crafting drops |
| `/chamber <0..18>` | chambers | admin | To that test chamber, with the gun it needs (blue from 03, both from 04) |
| `/defense [kit\|turrets\|wave N]` | tower defense | anybody (records), admin (the rest) | With no word, the records: the field's best wave and who held it, and the best wave of each player here. `kit` the building materials, `turrets` two of each turret and barricades, `wave N` clears the field and starts wave N |
| `/inferno <hall\|brazier 1..4\|light>` | inferno | admin | To the heart's hall or a brazier; `light` lights all four and opens the gate |
| `/labphase <shield\|siege\|climb\|pit\|fire\|dark\|frost\|race>` | lab | admin | L'Ospite's fight from that phase, the room laid out for it (the climb's route, the lava, the lamps out, the kart for the race) and you in place; `pit` is the climb from the pit's parkour |
| `/parkour <1..18\|tower 1..7\|summit>` | parkour | admin | To that pad (of the level you are on, in survival), a tower station or the summit, counted as reached |
| `/plat <room\|1-1\|1-2\|1-3\|bonus\|boss\|big\|fire>` | platform game | admin | Out to the room, into a level, to 1-1's bonus pipe or 1-3's boss hall; `big` and `fire` give that power in a level |
| `/shmup <wave N\|power\|drops>` | shooter | admin | `wave N` starts that wave now with 30 lives (or at the next take-off); `power` fills your fighter's power-ups; `drops` makes every kill drop one, or not |
| `/skies <manta\|village\|updraft\|phase2\|phase3\|kit\|reset>` | cloud world | admin | To the Manta's island, the nearest village or updraft; the Manta's low phase or its fall started now; `kit` the gliders, crystals and weapons; `reset` the fight back to the start |
| `/track <start\|ring\|grand\|inferno\|loops\|power KIND\|kart>` | track | admin | To that track's start line (in its room); `power KIND` (boost, boost3, banana, shell, homing, ink, star, bolt) into your kart; `kart` a kart in your bag |

## For the staff

### World and time

| Command | Permission | What it does |
|---|---|---|
| `/kill NAME` | moderator | Kills another player, who comes back at their bed or the spawn (`mods/creative`) |
| `/creative on\|off` | admin | Creative mode for yourself: flying without the jetpack, no damage, digging ten times as fast; kept by name across restarts. Free for the owner of a world started from the title screen (`mods/creative`) |
| `/give NAME ITEM [COUNT]` | give (admin) | Puts items, by name (`chest:box`), into an online player's bag |
| `/weather [clear\|rain\|storm] [MINUTES]` | admin | The sandbox's sky: with no word, what it is and how long it lasts; with one, that sky now, for MINUTES or its usual time (`mods/weather`) |
| `/time [day\|noon\|dusk\|night\|midnight\|dawn\|HH:MM]` | time (admin) | The clock of the world you are in, in every world: with no word, the hour; with one, that hour at once for everybody there. In the sandbox, turned back past midnight it counts a day, as a night slept through |
| `/story NAME [keys all\|WORLD...\|ending\|clear]` | admin | A player's story progress, the player being in your world: with no word, how many keys and whether l'Ospite is beaten. `keys all` or `keys pista inferno ...` records those keys and puts them in the bag (worlds: pista inferno arena arpg chambers backrooms parkour); `ending` is all seven and l'Ospite beaten, which opens the front and the assault and gives the rewards; `clear` takes it all back (`mods/keys`) |
| `/season [spring\|summer\|autumn\|winter]` | admin | The sandbox's season (five days each): with no word, which and what day of it; with one, the first day of that season (`mods/weather`) |
| `/difficulty [peaceful\|easy\|normal\|hard]` | admin | How hard the world is, at once and across restarts (WORLD.difficulty): peaceful, no creature that fights comes out or stays -- a dungeon's spawner leaves what its creatures would, the deep's altar gives its heart and shards unfought, villages have no sieges; easy halves a creature's blow and how many come out, and hunger; hard adds half to both. With no word, which it is (`mods/rules`) |
| `/peaceful [on\|off]` | admin | `/difficulty peaceful`, or normal again (`mods/rules`) |
| `/rule [NAME on\|off]` | admin | The world's rules, across restarts (WORLD.rules): `keep_bag` (a survival world's bag kept on death, no grave), `day_cycle` (the sun moves), `weather_cycle` (the weather changes by itself), `hunger`. With no word, the list (`mods/rules`) |
| `/goals` | anybody | How many of the sandbox's goals you have reached; `/page goals` lists them with hints for the next (`mods/goals`) |
| `/trophies` | anybody | How many trophies you have won; `/page trophies` lists them, with hints and which way the hidden portals are (`mods/trophies`) |
| `/trophies sites` `/trophies go ID [place] [NAME]` `/trophies home [NAME]` | admin | Where the five hidden portals are and whether each is built; you or NAME in front of one (`well`, `summit`, `lake`, `hill`, `circle`) or, with `place`, into the place it leads to; NAME (or you) into their own room, to see the trophy shelf (`mods/trophies`) |
| `/village siege\|feast\|caravan\|incoming` | admin | Starts a village event now in the village you stand in: a siege warned of now and coming at nightfall, the harvest feast, a caravan to the nearest other village, or one coming from an unvisited village along the paths (`mods/villagers`) |
| `/go [ROOM]` | go (admin) | With no word, the rooms of this server and whether each is up or started on demand; with one, you to that room's spawn (a room on demand is started). Then `/tp` for a place in it |
| `/tp X Y Z` `/tp X Z` `/tp NAME` | tp (admin) | You to that place in the room you are in; with X Z only, onto the ground there (the highest floor with two free nodes above, or your height on a column not loaded); with a name, to that player, or to the spawn of the room they are in |
| `/cursedghost` | admin | A cursed ghost four nodes in front of you, to test its hold (`mods/ghost`) |

### Players

| Command | Permission | What it does |
|---|---|---|
| `/role [NAME [ROLE [ROOM]]]` | roles (admin) | Lists the roles given; shows one player's; gives one (in one room with ROOM). `player` takes it away |
| `/kick NAME [reason]` | kick (moderator) | Puts the player out of every room, portal windows too; their client goes back to the title saying why |
| `/ban NAME [time] [reason]` | ban (moderator) | Out and kept out, for a time or for good. A name with an owner bans the owner's key; a name without one bans the address it last came from too |
| `/banip NAME\|ADDRESS [time] [reason]` | ban.ip (admin) | Keeps an address out |
| `/unban NAME\|ADDRESS` | ban (moderator) | Lifts a ban; lifting a friend server's ban here keeps it lifted |
| `/bans` | ban (moderator) | Who is banned, by whom, until when |
| `/mute NAME [time] [reason]` | mute (helper) | Holds the player's chat, and their words wherever a mod passes them on. A name nobody owns yet is muted by name |
| `/unmute NAME` | mute (helper) | Lets them speak again |
| `/warn NAME reason` | warn (helper) | A warning; three in 30 days are a day's ban |
| `/note NAME text` | notes (helper) | A note on the player, shown in `/whois` |
| `/whois NAME` | whois (moderator) | Key, names used, addresses, warnings, notes, bans |
| `/alts NAME` | alts (moderator) | Other players who came in from an address this one used, and which are banned |
| `/players [7d]` | players (helper) | Who came in lately, visits and time played |
| `/bag NAME` | bag (moderator) | What the player carries, square by square, and their backpacks; offline, the bag kept here |
| `/reports` | reports (helper) | The latest reports |
| `/sc TEXT` | staffchat (helper) | Chat between the staff online |

### Who may come in

| Command | Permission | What it does |
|---|---|---|
| `/whitelist on\|off\|list\|add NAME\|remove NAME` | access (admin) | With it on, only the listed players and the staff come in |
| `/rules set TEXT\|off` | rules (admin) | New rules, which everybody has to `/accept` again; `off` takes them away |
| `/perm [PERMISSION ROLE]` | perms (owner) | Which role each permission wants; with two words, changes one |

### The server

| Command | Permission | What it does |
|---|---|---|
| `/restart [5m\|now\|cancel] [reason]` | restart (admin) | Restarts with warnings on every screen (10m, 5m, 1m, 30s, 10s, the last seconds); clients join again by themselves |
| `/stop [5m\|now\|cancel] [reason]` | restart (admin) | The same, and the server stays stopped |
| `/maintenance on [text]\|off` | maintenance (admin) | Only the staff come in; the others are put out with the text |
| `/motd [TEXT\|off]` | motd (admin, to change) | The message of the day, said to each player coming in; with no word, shows it |
| `/announce TEXT` | announce (moderator) | A notice on every screen |
| `/backup` | backup (admin) | Copies every world file now |
| `/metrics` | logs (admin) | Uptime, players and windows per room, the longest gap between ticks |
| `/logs [N] [ACTION]` | logs (admin) | The latest admin actions, all or one kind (`ban`, `rollback`...) |
| `/admin` | pages (helper) | The administration page: who is online, with buttons to kick, ban and mute, and the pages behind it |

### Blocks

| Command | Permission | What it does |
|---|---|---|
| `/inspect X Y Z [ROOM]` | inspect (helper) | The last ten players who changed that block, from what to what, when |
| `/rollback NAME DURATION [ROOM]` | rollback (moderator) | Puts back every block that player changed in that time (needs `--block-history`) |
| `/oldbags NAME [N]` | oldbags (admin) | The copies of a bag that lost out between friend servers; with N, gives that one back |

### Portals and friend servers

| Command | Permission | What it does |
|---|---|---|
| `/portals` | portals (admin) | Where each room's openings lead, and which servers send players in |
| `/room [list]` | rooms (owner) | The server's rooms: up or down, how many in each, which are started when wanted |
| `/room add NAME [FLAG [VALUE]...]` | rooms (owner) | A new room in the rooms file (`--rooms-file`), with its own flags; it starts when something leads to it |
| `/room start NAME` `/room stop NAME` | rooms (owner) | Start a room of the rooms file now; save and stop an empty one |
| `/room remove NAME` | rooms (owner) | Take a stopped room out of the rooms file; its world file stays |
| `/friend add NAME HOST:PORT KEY [shares]` | federation (owner) | A friend server; `shares` is any of `bans,presence,chat,bags,portals,mods`, all by default; a NAME already there is replaced (a friend whose key changed) |
| `/friend remove NAME` | federation (owner) | No longer friends; its bans are lifted here |
| `/friend list` | federation (owner) | The friends, and when each was last heard |

## For mod makers

A mod adds a command with `core.command(name, help, perm)` and answers it in
`core:command`; the name is English, the help a short line after `--`. See
[`API.md`](API.md).
