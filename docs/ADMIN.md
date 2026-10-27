# Running a server

Everything an owner or a member of staff can do to a `cnp_server`: the commands, who may use
them, the flags and the `server.conf` keys, the scripts, and what each one keeps on disk. The
code is [`src/server/admin.h`](../src/server/admin.h) and `admin.cpp`; every command goes
through one function there, whichever way it was typed.

## Contents

- [Where commands come from](#where-commands-come-from)
- [Roles and permissions](#roles-and-permissions)
- [Commands](#commands)
- [In-game pages](#in-game-pages)
- [Flags and server.conf](#flags-and-serverconf)
- [Keeping it running](#keeping-it-running)
- [Restarts, stops and maintenance](#restarts-stops-and-maintenance)
- [What mute holds](#what-mute-holds)
- [Rules to accept](#rules-to-accept)
- [Alt accounts](#alt-accounts)
- [Update check](#update-check)
- [Visits, logs and reports](#visits-logs-and-reports)
- [Blocks: inspect and rollback](#blocks-inspect-and-rollback)
- [Backups](#backups)
- [Webhooks](#webhooks)
- [Portals between servers](#portals-between-servers)
- [Friend servers](#friend-servers)
- [What is kept, and where](#what-is-kept-and-where)
- [Limits](#limits)

## Where commands come from

| Way in | How | Role |
|---|---|---|
| Chat | `/ban bob 2h griefing` typed in the game | the player's own |
| Console | the server's stdin, without the slash: `ban bob 2h griefing` | above every role |
| RCON | `--rcon PORT --rcon-password PW`, TCP on 127.0.0.1 only; `CNP_RCON_PASSWORD=PW tools/admin.sh -p PORT ban bob 2h` | above every role |
| Pages | `/admin` in the game, buttons that run the same commands | the player's own |

The RCON protocol is plain lines: the password first (answered `ok`), then one command a
line, each answered by its lines and a line holding a lone `.`. Put the password in
`server.conf`, not on the command line, where `ps` shows it.

`/help` lists the commands the caller may use. A mod may not declare a command with an
admin command's name, nor `page` or `admin`.

## Roles and permissions

`guest < player < helper < moderator < admin < owner`.

- **guest**: a player with no identity (no key). Nothing can be given to a guest, because a
  name without a key is a label anybody can type.
- **player**: everybody with a key, unless given more.
- **owner**: `--owner NAME` (the client passes it for the worlds it starts itself), or
  `/role NAME owner` from the console.

Roles are given by key: a player keeps theirs under another name. Nobody gives a role equal
to or above their own, and nobody acts on a player of the same role or above (the console
excepted).

`/role NAME ROLE ROOM` gives a role in one room only: in that room the player has the higher
of the two. A moderator of the arena is a player in the sandbox.

Each command asks for a permission, and each permission wants a least role. The defaults:

| Role | Permissions |
|---|---|
| helper | `mute`, `pages`, `staffchat`, `warn`, `reports`, `notes`, `players`, `inspect` |
| moderator | `kick`, `ban`, `creative`, `announce`, `whois`, `rollback`, `bag`, `alts` |
| admin | `ban.ip`, `access`, `roles`, `restart`, `maintenance`, `motd`, `rules`, `logs`, `backup`, `portals`, `give` |
| owner | `perms`, `federation` |

`/perm PERMISSION ROLE` (owner) moves a permission to another role; `/perm` lists them.
A role's own name is a permission too (`moderator`), for a mod that has no better word.

Mods: `core.command(name, help, perm)` gates a mod's command; `core.has_perm(player, perm)`
and `core.role(player)` ask (see [`API.md`](API.md)).

## Commands

Durations are `45s`, `30m`, `2h`, `7d`, `1w`. NAME is a player online in any room, or the
owner of that name on this world, or the bare name.

### Who may come in

| Command | Permission | What it does |
|---|---|---|
| `/perms` | anybody | Your role here |
| `/role [NAME [ROLE [ROOM]]]` | roles | List the roles given; see one player's; give one (in one room with ROOM). `player` takes it away |
| `/kick NAME [reason]` | kick | Out of every room, the portal windows too; the client goes back to the title saying why |
| `/ban NAME [time] [reason]` | ban | Out and kept out, for a time or for good. A name with an owner bans the owner's key; a name without one bans the address it last came from as well |
| `/banip NAME\|ADDRESS [time] [reason]` | ban.ip | Keep an address out |
| `/unban NAME\|ADDRESS` | ban | Lift a ban. Lifting a friend server's ban here keeps it lifted |
| `/bans` | ban | Who is banned, by whom, until when |
| `/mute NAME [time] [reason]` `/unmute NAME` | mute | Hold a player's chat, and their words wherever a mod passes them on (below). A name nobody owns yet is muted by name, and the mute holds when it comes with a key |
| `/whitelist on\|off\|list\|add NAME\|remove NAME` | access | Only the listed players, and staff, come in |
| `/perm [PERMISSION ROLE]` | perms | Which role each permission wants |
| `/warn NAME reason` | warn | A warning; three in 30 days are a day's ban |
| `/rules [set TEXT\|off]` | anybody (rules to change) | The rules everybody accepts; setting new ones asks everybody again |
| `/accept` `/leave` | anybody | Accept the rules, or leave the server not accepting them |

### The server's life

| Command | Permission | What it does |
|---|---|---|
| `/restart [5m\|now\|cancel] [reason]` | restart | Restart with warnings on every screen (10m, 5m, 1m, 30s, 10s and the last seconds). The server leaves with exit status 75 for `tools/serve.sh`; clients put out as "restarting" join again by themselves |
| `/stop [5m\|now\|cancel] [reason]` | restart | The same, and the server stays stopped |
| `/maintenance on [text]\|off` | maintenance | Only staff come in; the others are put out with the text. The server list in the client shows "maintenance" |
| `/motd [TEXT\|off]` | motd (to change) | The message of the day, said in chat to each player coming in |
| `/announce TEXT` | announce | A notice on every screen |
| `/backup` | backup | Copy every world file now |
| `/metrics` | logs | Uptime, players and windows per room, the longest gap between ticks |

### Players

| Command | Permission | What it does |
|---|---|---|
| `/seen NAME` | anybody | When a player was last here, and for how long in all |
| `/players [7d]` | players | Who came in lately, visits and time played |
| `/whois NAME` | whois | Key, names used, addresses, warnings, notes, bans |
| `/alts NAME` | alts | Other players who came in from an address this one came from, and which are banned |
| `/bag NAME` | bag | What a player carries, square by square, and what their backpacks hold; offline, the bag kept here; and whether it went to a friend server |
| `/give NAME ITEM [COUNT]` | give | Items by name (`chest:box`) into an online player's bag |
| `/report NAME what happened` | anybody | A report for the staff, told at once to those online |
| `/reports` | reports | The latest reports |
| `/note NAME text` | notes | A note on a player, shown in `/whois` |
| `/sc TEXT` | staffchat | Chat between the staff online |
| `/logs [N] [ACTION]` | logs | The latest admin actions, all or one kind (`ban`, `rollback`...) |

### Blocks

| Command | Permission | What it does |
|---|---|---|
| `/inspect X Y Z [ROOM]` | inspect | The last ten players who changed that block, from what to what, when |
| `/rollback NAME DURATION [ROOM]` | rollback | Put back every block that player changed in that time |

### Portals and friend servers

| Command | Permission | What it does |
|---|---|---|
| `/portals` | portals | Where each room's openings lead, and which servers send players in, how many, from where |
| `/friend add NAME HOST:PORT KEY [shares]` | federation | A friend server; `shares` is any of `bans,presence,chat,bags`, all by default |
| `/friend remove NAME` `/friend list` | federation | Stop being friends (its bans are lifted here); the friends, and when each was last heard |
| `/fed TEXT` | anybody | Chat to the players of every friend server |
| `/where NAME` | anybody | Here (and in which room), or on which friend server |

### Pages

| Command | Permission | What it does |
|---|---|---|
| `/page [NAME]` | anybody | The in-game pages, or one of them |
| `/admin` | pages | The administration page |

## In-game pages

`/admin` opens a page with who is online and buttons to kick, ban and mute; from there
`bans`, `whitelist`, `roles`, `perms`, `running` (restarts, maintenance, message of the day,
backups), `players`, `reports` and `log`. Everybody has `server` (the rooms and who is in
them), `mods`, `here` (where you stand and whose ground it is), `rules` and the pages mods add.
A page shows only what its viewer may do, and each button runs the command typing it would.

Pages are laid out by the server in English. A client that translates a label or a button
draws the translation smaller when it is wider than the room it was given, down to two thirds
of the size, and past that the English words, which fit by construction.

## Flags and server.conf

`--config FILE` reads flags from a file where it stands on the command line, so flags after it
win. One `key value` or `key = value` a line, meaning `--key value`; `key = true` is a bare
flag; `#` starts a comment; `[room NAME]` starts the flags of one room.

```
# server.conf
listen = 25300
world-file = worlds/main.world
owner = alice
rcon = 25399
rcon-password = change-me
motd = Welcome! /help lists the commands.
rules-file = rules.txt
restart-at = 04:00
backup-every = 6h
backup-keep = 8
max-per-address = 3
watchdog = 60
federation = 25480
webhook = https://example.org/hooks/server
```

| Flag | What it does |
|---|---|
| `--owner NAME` | Whoever plays under NAME with a key is the owner |
| `--rcon PORT` `--rcon-password PW` | Admin commands over TCP on 127.0.0.1 |
| `--motd TEXT` | The message of the day |
| `--rules-file FILE` | The rules to accept, from a file (several lines); see below |
| `--no-update-check` | Do not ask GitHub whether a newer release is out |
| `--restart-at HH:MM` | A restart every day at that local time, with ten minutes of warnings |
| `--backup-every 6h` `--backup-keep N` | Backups while running; the newest N stay (5) |
| `--max-per-address N` | At most N players from one address |
| `--watchdog S` | A room stuck for S seconds aborts the server with its stack printed |
| `--federation PORT` | Friend servers' TCP port, under the ticket key (`--ticket-key`, or WORLD.sign) |
| `--webhook URL` | Admin actions, start, stop and reports POSTed there |
| `--max-players N` `--require-identity` `--no-observers` | Seats; players must have a key; no portal windows from other worlds |
| `--report FILE` `--since 7d` `--json` | No server: what a world file remembers, printed; `--json` as one JSON object |
| `--names` `--free-name NAME` | No server: who owns each name; let one go |

The rest of the flags (world, mods, portals, rooms) are in the [README](../README.md#server-cnp_server).

## Keeping it running

```
tools/serve.sh -l logs -k 14 -w https://example.org/hooks/server \
    build-release/src/server/cnp_server --config server.conf
```

- Starts the server again when it restarts (exit status 75: `/restart`, `--restart-at`) or
  crashes (a `--watchdog` abort included), five seconds after a crash.
- Leaves it stopped after `/stop`, Ctrl+C or SIGTERM (exit status 0), and gives up after five
  crashes in ten minutes.
- Writes everything the server prints to `LOGDIR/server-YYYY-MM-DD.log`, a file a day, the time
  on every line; files older than `-k` days (14) go.
- `-w URL` POSTs each crash and the giving up, with the same fields as `--webhook`.
- Its stdin is the server's, so the console still works under it.

## Restarts, stops and maintenance

A restart or stop counts down with a notice on every screen, then puts everybody out with the
reason "restarting" or "stopping" and saves. A client put out by a restart goes back to the
title and joins the same server again by itself once it answers (for three minutes). `/restart
cancel` stops the countdown.

Maintenance keeps everybody but staff out; whoever is in and not staff is put out with its
text. It survives a restart, until `/maintenance off`.

## What mute holds

A muted player's own chat is held, and so is whatever a mod says to others while running their
command, page action, panel click or message. And the words they handed a mod are noted: a
mod's chat line, notice or page line that carries three of those words in a row (twelve
letters or more, or all they said if that is less) reaches only them, then or later -- a letter
delivered on a later tick, a note pinned on a board. A held page line reads `(held: NAME is
muted)`. Words are noted while the player is muted and kept while they stay muted and for an
hour after; what they said in under six letters is not. The log says each time a line is held.

## Rules to accept

With `--rules-file FILE` (or `/rules set TEXT`), each player coming in sees the page `rules`
with the text and two buttons, Accept and Leave. Until they accept they cannot chat, run
commands other than `/rules`, `/accept`, `/leave` and `/help`, or break or place blocks; the
page comes back each time they try. Staff are taken to have accepted. Setting different rules
asks everybody again; the same text read again at a restart does not. `/rules off` removes
them.

## Alt accounts

`/alts NAME` lists the other players (keys) who came in from any address that player came
from, with which of them are banned. When a player comes in from an address a banned player
used, the staff online are told at once (`[alts] bob came in from where eve did, who is
banned`). An address is not a person: a household, a school or a phone network shares one.

## Update check

A server that listens asks GitHub once, at start, whether a release newer than its own is out
(the same question the client's title screen asks). When one is, the log says so, and every
admin online and each admin coming in is told in chat. `--no-update-check` turns it off.

## Visits, logs and reports

Each visit is a row: who, from which address, in which room, from when to when, and why it
ended. A crash leaves visits open; the next start closes them and logs it as an unclean stop.
Every admin action goes to the admin log with who did it, to whom, and the details.

`cnp_server --report worlds/main.world --since 7d` prints, without stopping the server:
players and their time, most at once, joins by hour, rooms, who was turned away and why,
staff actions, restarts and crashes, and arrivals through portals from other servers.
With `--json` the same numbers come as one JSON object (`players`, `visits`,
`seconds_played`, `most_at_once`, `joins_by_hour`, `most_played`, `rooms`, `refused`,
`staff_actions`, `server`, `crossings_in`), for a program to read.

## Blocks: inspect and rollback

Every block a player breaks or places is written down: the room, the cell, what was there,
what it became, who and when. They go to the file together every five seconds; a month is
kept.

`/rollback NAME 2h` puts each cell that player changed in the last two hours back to what it
was before their first change in that time, unless somebody changed it after them: that
change stays. Those rows are then gone, so a second rollback does nothing. What the player
got from the blocks stays in their bag; `/bag` and the item commands are for that.

## Backups

`--backup-every 6h` and `/backup` copy every world file next to itself as
`NAME.backup-YYYYMMDD-HHMMSS`, with SQLite's online backup on a thread of its own, while the
server runs. The newest `--backup-keep` stay.

To go back to one: stop the server, keep the current file aside, copy the backup over the world
file (the `.key`, `.sign` and `.lock` files beside it stay as they are), and start it again.

## Webhooks

With `--webhook URL`, each admin action, each `/report`, and the server's start (`unclean
start` after a crash) and stop are POSTed as JSON:

```json
{"event":"ban","by":"alice","target":"bob","detail":"until 2026-10-28 18:00, griefing",
 "at":1793120997,"content":"[ban] alice bob: until 2026-10-28 18:00, griefing",
 "text":"[ban] alice bob: until 2026-10-28 18:00, griefing"}
```

`content` and `text` are the same sentence, which chat services' incoming webhooks show as
they are. The post goes through `curl`, five seconds at most, on a thread of its own: a
webhook that is down costs nothing. The URL must be `http://` or `https://` with plain
characters.

## Portals between servers

`--portal-to HOST:PORT[/N]` says where a server-managed opening leads; `--trust HEX` takes the
tickets another server signs, `--require-ticket` takes nobody else. A client crossing checks
that the far server's key is the one the link names, through a portal window and through the
slow redirect alike. `/portals` shows both directions.

## Friend servers

Two servers that add each other share, over their `--federation` port:

- **bans**: permanent bans by key. A friend's ban holds here until `/unban` lifts it (and then
  stays lifted) or the friend is removed.
- **presence**: who is online, for `/where`.
- **chat**: `/fed`.
- **bags**: one bag for a player on all of them (below).

Setting up two friends:

1. Start each with `--federation PORT`; the log says `this server's key HEX`, and `/friend list`
   repeats it.
2. On the first: `/friend add second second.example.org:25480 HEX_OF_SECOND`.
3. On the second: `/friend add first first.example.org:25480 HEX_OF_FIRST`.

Each line goes on a TCP connection of its own. The receiver speaks first, with a key made for
that connection alone; the sender answers with one of its own and the line, sealed for the
receiver (XChaCha20-Poly1305, with a key from the two servers' long-term keys and the two
throwaway ones) and signed by the sender's ticket key. The throwaway keys are forgotten at
once, so a line recorded today cannot be opened tomorrow even with both servers' keys: forward
secrecy. A line older than the sender's last, five minutes away from this clock, or from a key
not in the list is dropped.

### One bag

A player's bag lives on one friend server at a time and goes where they go:

- **Through a portal** from a friend: the server they arrive at asks the one they come from,
  which lets go of it (the player there is a window now) and sends it.
- **Walking in** from the menu: the server asks every friend whose copy is newest. The newest,
  if newer than its own, sends it and lets go of it. Newest is the copy that moved between
  friends most often, which needs no clock (only the holder is ever given a higher count); two
  copies that never moved, from before the servers were friends, are told apart by when each
  was kept.
- The one that sent it remembers where it went: a later visit there asks that server first,
  and `/bag` there says so.
- While it comes (a few milliseconds, at most five seconds) the player holds an empty bag, and
  nothing saves over the one on disk. A friend that does not answer in time leaves the player
  with the bag kept here; if the friend's bag comes later after all, it takes the place of
  that old copy, and what the player picked up in the meantime goes into it.
- **Backpacks** (`mods/backpack`) go with their contents. A backpack's contents are kept under
  its number, which each server's mod counts from 1, so the server a backpack comes to gives
  it a number of its own from 100000 up (kept when it comes back to a server where that number
  is free).
- **Things a world does not know** (an item of a mod it does not have) are hidden: kept aside,
  out of the bag, and sent along with it, a hidden backpack with its contents; they come back
  on a server that knows them, into the square they had (a backpack onto the back) or a free
  one. `/bag` says when a player has some.

## What is kept, and where

In the first room's world file (one store for every room), next to the world:

| Table | What |
|---|---|
| `admin` | One row a thing: `role`, `ban`, `mute`, `allow` (whitelist), `setting` (whitelist on, maintenance, message of the day, rules), `accepted` (which rules a player accepted), `perm`, `addr` (last address), `friend`, `fedskip` (a friend's ban lifted here), `bagaway` (whose bag went to which friend), `baggen` (how often a bag moved), `hidden` and `packs` (what a player carries that this world does not know, and what its backpacks hold) |
| `admin_log` | Every admin action, start, stop, unclean stop, report, note, warning |
| `sessions` | One row a visit |
| `edits` | Every block a player changed, a month |

Bags themselves are in each room's world file, as they always were.

## Limits

- `/mute` recognises a muted player's words by three in a row: a mod that rewords them, or a
  line under six letters, gets through; another player repeating their phrase within the hour
  is held too. A mod that relays words asks `core.is_muted` itself, as `/msg` and `/me` do.
- A friend that stays unreachable while a player plays on with the copy kept here keeps its own
  copy too; whichever moved more often wins the next time the two are compared.
- The client's check that the far server's key is the one a portal names stands behind the
  ticket: a server that is not the one named refuses the ticket before the client ever reads
  its key, so the check only matters for a server that would lie about tickets too.
