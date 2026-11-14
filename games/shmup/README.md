# The arcade shooter

*[Leggi in italiano](README.it.md)*

A shooter seen from overhead on a fighter, in the manner of the arcade cabinets of the eighties.
**A first draft**: one screen that does not scroll, waves of planes, three lives, a score and a
high score. Two mods: `mods/shmup_core` (server only: the sea, the carrier's deck, the walls of the
screen, the planes, and the body -- a fighter, which flies) and `mods/shmup` (`side = both`: the game
on the server; the camera, the shots and the screen's text on a player's machine).

## How to get there

In the game (the title menu, or `tools/track.sh`) its gate is on the plaza's south edge, at
56.5 21 26.5: a black arcade cabinet (`mods/gates`). The world is not one the server starts with:
it is in the rooms file, starts when somebody comes near the gate and stops a minute after the last
one left; the gate is a portal between rooms that borrows a mouth. `tools/shmup.sh` starts the
sandbox with only this world, behind its first opening (`tools/shmup.sh -- --room shmup` is born in
it). Either way you come out on a carrier's deck at the bottom of
the picture, already a fighter and already seen from overhead (`core.top_view_at`, new with this
game: `docs/MODDING.md`). The doorway at the stern, down the screen, leads back.

## The game

| key | |
|---|---|
| space | starts a game; held, fires |
| W, A, S, D | slide the fighter up, left, down, right: it always faces up the screen |
| E | back to the deck (out of this game) |

Darts, olive, come straight down (100 points). Weavers, orange, swing from side to side and aim a shot now and then
(150). Aces, black with red bands, stop half way and fire three at a time (400). Every fifth wave is a grey bomber that takes
forty shots (3000). A shot or a plane that touches you costs a life, and for two and a half seconds
after nothing does (light blue blinks at your wing tips). Your fighter is the silver one with red wing tips. With none left you are put back on the deck;
when everybody is, the game is over and the high score is kept (`core.storage_set`). Several
players fly the same waves, each with their own lives and score; whoever arrives during a game
joins it with the space bar.

## How it is made

The server keeps everything that decides: where planes and shots are, who was hit. A player's
machine is told each shot once, when fired, and flies it by itself until told it is gone. Planes
are the mod's entities (`core.ecs.visible`); their models are the front's plane and bomber in
liveries of their own (`tools/make_shmup_models.py`, Blender), the pictures and the sounds are
`tools/make_shmup_assets.py`'s. The sea stands still: the streaks of foam and the
clouds going by are drawn by the player's machine (`core.render.pass`). The picture's top is +z, so
its right is -x.

## Not there yet

See `TODO.md`, "Platform game and arcade shooter (drafts)".
