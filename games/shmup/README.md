# The arcade shooter

*[Leggi in italiano](README.it.md)*

A shooter seen from overhead on a fighter, in the manner of the arcade cabinets of the eighties:
one screen with the sea going by under it, ten waves of planes, power-ups, a flying fortress at
the end, three lives, a score and a high score. Two mods: `mods/shmup_core` (server only: the sea,
the island with the hangar, the walls of the screen, the models, and the body -- the sandbox's
robot, on foot) and `mods/shmup` (`side = both`: the game on the server; the take-off, the camera,
the scenery going by, the shots and the screen's text on a player's machine).

## How to get there

In the game (the title menu, or `tools/track.sh`) its gate is on the plaza's south edge, at
56.5 21 26.5: a black arcade cabinet (`mods/gates`). The world is not one the server starts with:
it is in the rooms file, starts when somebody comes near the gate and stops a minute after the last
one left; the gate is a portal between rooms that borrows a mouth. `tools/shmup.sh` starts the
sandbox with only this world, behind its first opening (`tools/shmup.sh -- --room shmup` is born in
it). Either way you come out of a doorway in the back wall of a hangar, on foot, looking down it at
four parked fighters and the sea past its open front. The doorway leads back.

Walk up to a fighter and press **F**: it rolls down the apron, takes off and climbs, and the camera
swings from behind it to overhead. The first fighter up starts the game; whoever boards during a
game joins it.

## The game

| key | |
|---|---|
| F | beside a parked fighter: take off. Flying: back to the hangar (out of this game); E does that too |
| W, A, S, D | slide the fighter up, left, down, right: it always faces up the screen |
| space | held, fires |

Darts, olive, come straight down (100 points). Weavers, orange, swing from side to side and aim a
shot now and then (150). Aces, black with red bands, stop half way and fire three at a time (400).
Wave 5 is a grey bomber that takes forty shots (3000). The planes keep to the middle 48 nodes of a
screen 80 wide, so a fighter can reach every one of them.

Now and then, from wave 2, an islet goes by with a **tank** on it (300, and always a power-up): it
turns on the nearest fighter and fires; it is on the ground, so flying over it costs nothing.

A plane shot down -- theirs, or yours and its wingmen with your last life -- goes into the sea turning over and smoking,
and leaves a ring where it meets the water.

Wave 10, the last, is the **flying fortress** (20000): 40 across, six turrets (500 each) that turn
on the nearest fighter, and a shield -- the ring of lights round it. While the shield is up
everything fired into it is spent on it; shot down (the blue bar at the top of the screen), it
stays down ten seconds, in which the turrets and the hull (the red bar) can be hit. Half gone, the
hull throws rings of shots as well. Down it goes and the game is won: the fighters leave by the top
of the screen faster than anything on it, come down onto the apron from the sea and roll back to
their pads, and their pilots are on foot in the hangar again.

A shot or a plane that touches you costs a life, and for two and a half seconds after nothing does
(light blue blinks at your wing tips). With none left you are put back in the hangar; when
everybody is, the game is over and the high score is kept (`core.storage_set`).

## Several players

Everybody flies the same waves, each with their own lives, score and power-ups. To start together,
each boards a fighter: the game starts when the first is up, and its first plane comes a second and
a half after -- the others are up by then. Whoever boards later joins the game as it is. Somebody
shot down waits in the hangar for the game to end. With others flying, a light blue light at the
tail marks your own fighter, and `PILOTS n` is under the wave's number.

The game is made harder for each pilot flying past the first:

| | each pilot more | two | three | four |
|---|---|---|---|---|
| what a plane, a turret, the hull and the shield take (a dart goes on taking one shot) | +60% | x1.6 | x2.2 | x2.8 |
| planes in a wave, in the same time | +50% | x1.5 | x2 | x2.5 |
| how fast planes and turrets fire | +25% | x1.25 | x1.5 | x1.75 |

Less than one more pilot's worth each time: two fighters have twice the guns but the same sky to
dodge in. A wave is sized when it starts and a plane when it comes in, so somebody joining or shot
down changes the next ones; the rate of fire follows who is flying now. More planes also means more
power-ups, which are for whoever reaches them.

## Power-ups

Every sixth plane shot down, every ace and the bomber (three) leave one, which drifts down the
screen: fly over it. What they give lasts the game, except the three that blink, which run out.

| colour | | |
|---|---|---|
| green | speed | 5 m/s faster, three times |
| green, blinking | turbo | 10 m/s faster for ten seconds |
| red | power | two guns, then three, then three firing faster |
| red, blinking | rapid fire | everything fires twice as fast for ten seconds |
| purple | homing missile | one more missile a salvo (every 1.2 s), up to four; each takes three hits' worth off the plane nearest it |
| yellow | diagonal fire | two shots at a slant, then four |
| cyan, blinking | laser | ten seconds (up to thirty) of a beam in place of the guns, through every plane over the fighter |
| white | wingman | a small plane alongside that fires too, up to two |

One a fighter already has all of is worth 500 points.

## How it is made

The server keeps everything that decides: where planes and shots are, who was hit. A player's
machine is told each shot once, when fired, and flies it by itself until told it is gone; missiles,
beams and the fortress, which do not go straight, it is told ten times a second. Planes, turrets
and wingmen are the mod's entities (`core.ecs.visible`); their models are the front's plane and
bomber in liveries of their own and a fortress built for this game (`tools/make_shmup_models.py`,
Blender), the pictures and the sounds are `tools/make_shmup_assets.py`'s.

The field stands still over a bare sea of blocks; what goes by under it -- crests, islands, clouds
-- is drawn by the player's machine (`core.render.pass`), endlessly, each island made from its
number. The one island that is blocks is the base, south of the picture's bottom edge. A take-off
is the player's machine too: the fighter is a model drawn along a path and the camera is
`core.fixed_view` asked again every frame, ending exactly where `core.top_view_at` then holds it;
only then does the server make the body a fighter (`core.body_model`), give it its flying
(`core.body_movement`, which a speed power-up changes) and put it over the field. The picture's top
is +z, so its right is -x.

For trying things, `TEST_WAVE` and `TEST_DROPS` at the top of `mods/shmup`'s server half start a
game at the fortress with thirty lives and make every plane leave a power-up.

## Not there yet

See `TODO.md`, "Platform game and arcade shooter (drafts)".
