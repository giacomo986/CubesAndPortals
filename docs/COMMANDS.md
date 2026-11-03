# Commands

*[Leggi in italiano](COMMANDS.it.md)*

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
| `/town [town]` | anybody | Takes you to the square of the nearest village; with any word after it, of the nearest town (a bigger village, searched twice as far) (`mods/villages`) |
| `/creative TEXT` | creative (moderator) | Opens the creative catalogue narrowed to items whose name has the text (`mods/creative`) |

### In one world only

| Command | World | Permission | What it does |
|---|---|---|---|
| `/garage` | track | anybody | Opens the garage: the colours and parts of your kart; a click changes it at once, on the road too, and it is kept |
| `/pieces` | track | anybody | Puts in your bag a stack of every track piece, the track editor (B with it in hand picks the next piece from afar), and the start line, checkpoint, off-road and item box tools |
| `/vehicle <jeep\|tank\|heli\|plane\|bomber\|nest\|mortar\|howitzer\|aa>` | front, assault | moderator | Puts that machine six nodes in front of you, for testing |

## For the staff

### World and time

| Command | Permission | What it does |
|---|---|---|
| `/kill NAME` | moderator | Kills another player, who comes back at their bed or the spawn (`mods/creative`) |
| `/creative on\|off` | admin | Creative mode for yourself: flying without the jetpack, no damage, digging ten times as fast; kept by name across restarts. Free for the owner of a world started from the title screen (`mods/creative`) |
| `/give NAME ITEM [COUNT]` | give (admin) | Puts items, by name (`chest:box`), into an online player's bag |
| `/weather [clear\|rain\|storm] [MINUTES]` | admin | The sandbox's sky: with no word, what it is and how long it lasts; with one, that sky now, for MINUTES or its usual time (`mods/weather`) |
| `/time [day\|noon\|dusk\|night\|midnight\|dawn\|HH:MM]` | admin | The clock: with no word, the hour; with one, that hour at once for everybody. Turned back past midnight it counts a day, as a night slept through (`mods/weather`) |
| `/season [spring\|summer\|autumn\|winter]` | admin | The sandbox's season (five days each): with no word, which and what day of it; with one, the first day of that season (`mods/weather`) |
| `/difficulty [peaceful\|easy\|normal\|hard]` | admin | How hard the world is, at once and across restarts (WORLD.difficulty): peaceful, no creature that fights comes out or stays -- a dungeon's spawner leaves what its creatures would, the deep's altar gives its heart and shards unfought, villages have no sieges; easy halves a creature's blow and how many come out, and hunger; hard adds half to both. With no word, which it is (`mods/rules`) |
| `/peaceful [on\|off]` | admin | `/difficulty peaceful`, or normal again (`mods/rules`) |
| `/rule [NAME on\|off]` | admin | The world's rules, across restarts (WORLD.rules): `keep_bag` (a survival world's bag kept on death, no grave), `day_cycle` (the sun moves), `weather_cycle` (the weather changes by itself), `hunger`. With no word, the list (`mods/rules`) |
| `/goals` | anybody | How many of the sandbox's goals you have reached; `/page goals` lists them with hints for the next (`mods/goals`) |
| `/village siege\|feast\|caravan\|incoming` | admin | Starts a village event now in the village you stand in: a siege warned of now and coming at nightfall, the harvest feast, a caravan to the nearest other village, or one coming from an unvisited village along the paths (`mods/villagers`) |

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
| `/rollback NAME DURATION [ROOM]` | rollback (moderator) | Puts back every block that player changed in that time |

### Portals and friend servers

| Command | Permission | What it does |
|---|---|---|
| `/portals` | portals (admin) | Where each room's openings lead, and which servers send players in |
| `/friend add NAME HOST:PORT KEY [shares]` | federation (owner) | A friend server; `shares` is any of `bans,presence,chat,bags,portals`, all by default; a NAME already there is replaced (a friend whose key changed) |
| `/friend remove NAME` | federation (owner) | No longer friends; its bans are lifted here |
| `/friend list` | federation (owner) | The friends, and when each was last heard |

## For mod makers

A mod adds a command with `core.command(name, help, perm)` and answers it in
`core:command`; the name is English, the help a short line after `--`. See
[`API.md`](API.md).
