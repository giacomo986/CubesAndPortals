# Open items

What is missing or wrong today, one short entry each. An entry leaves this file
when it is fixed or no longer matters. The history -- how each feature was
built, measured and fixed -- is in the archive, under the same OPEN-n /
CLOSED-n ids that comments in `src/` still cite:

- [`docs/archive/TODO-storia.md`](docs/archive/TODO-storia.md): up to 2026-09-13.
- [`docs/archive/TODO-diario-2026-10-09.md`](docs/archive/TODO-diario-2026-10-09.md):
  every feature entry from 2026-09-13 to 2026-10-09, with its numbers.

---

## The game as a whole

**Declared playable, 2026-10-09.** Giacomo lifted the gate: M10 (distribution)
is no longer suspended. What "playable" is checked by from now on, every one
by playing, not by a test:
1. A fresh world from the title menu: an hour of play with no blocking defect
   (a crash, a lost world, a stuck body, a gate that does not take you there).
2. Every gate on the plaza there and back on foot, bag and health carried.
3. Two clients on one server for half an hour: the sandbox, a kart race, a
   village, a game of chess and of cards.
4. A world saved, the server killed with -9, the world read back.

Run so far (2026-10-09): three clients (anna, bruno, carla) on a fresh
`tools/track.sh` in Xephyr, 0 corrections on any side; all three had been put
at the same spawn, inside one another -- fixed, each next join stands a node
along x, alternating sides (39.5, 40.5, 41.5 saved). Half an hour, a race,
chess and cards with two people: still to be played by hand. Check 4
passed (see Saving).

**A new world takes 25 s to start (2026-10-11).** It took about 80 s:
`core:world_ready` ran outside a LightRun, so every cell the track and the
the chambers wrote was relit on its own (pista 30 s, chambers 22 s, now
2 s each). What is left: the sandbox's 13 mapgen stages (3 s) and the
inferno's 4 (5.7 s), 9 rooms one after another. The title screen shows a bar
and the step under way ("inferno: the mods shaping the land"), read off the
server's log: six steps a room, equal on the bar though not in time.

**An ending, and a little story (agreed in part, 2026-10-09).** Agreed with
Giacomo: seven keys, one per themed world; each world reworked into a real
challenge that ends with its key; the final world is the laboratory, where a
last monster is beaten with a combination of what every world taught; after
the credits the sandbox stays free; the tone is serious. Still to agree
before building -- agreed the same evening: keys per player, any order, a
death in a trial goes back to its last checkpoint, l'Ospite and an unnamed
engineer, the story as text only, and the trials and the lab as proposed
(memory: game-ending-needs-design).

Built (2026-10-09): `mods/keys`, linked into every themed world: the seven
keys (items, icons from tools/make_key_textures.py), `/diario` (the engineer's
page of each key carried). In the sandbox the eighth gate (mods/gates, a dark
steel frame and seven pedestals at 62.5 21 76.5) leads to the new room
`laboratorio` (games/lab: a steel hall round the breach, parkour on), and
keeps out anybody who has not held all seven: scripted, without keys put back
4 nodes out with "Il portale e' spento", given the seven by a scratch mod it
said they lit the portal and the walk went through into the lab. Keys held
are kept per identity in the sandbox's storage. `core.bag_count` is new.
Open:
- Trials built: parkour (2026-10-09), a tower of seven stations after the
  rope, each a copy of a proven station, from 29 up to a summit at 52 that
  gives keys:parkour; checked by putting a player on the summit (key given,
  the tower seen from it). Not climbed by hand: the chimneys' exits and the
  rope's reach from a beam 13 over the floor are the stations' own, untried
  in the tower.
  Chambers (2026-10-09): four more chambers before the last room, each made
  of mechanisms the first six proved -- 07 the cube behind glass three high
  (in and out by portals over it, then 04's ledge), 08 two buttons for one
  door and two cubes, one button past a pit, 09 05's plates then a pit six
  wide only a portal crosses, 10 a push button (10 s) and a pit sixteen wide
  -- and the last room gives keys:chambers. A door may have several buttons
  (all down), push buttons each their own door and time. Seen by a scripted
  tour (each chamber in a screenshot, the key given in the last room); not
  solved by hand.
  2026-10-11: a cube picked up no longer shrinks away on the floor (the
  carrier's client took it for dead; protocol 91 sends it held), and the
  floor button is two by two (Blender, a plinth 0.15 high up or pressed).
  Seen in Xephyr: the pick-up clean, the button drawn and pressed by a body.
  Arena (2026-10-09): after wave 7 the waves stop and the Guardian of the
  reactor comes out of its east gate (tools/make_fps_boss.py, Blender: a
  horned goat-legged demon 1.5 times an imp, idle/walk/run/shoot/attack/death,
  a green fireball; 700 hp, bolts of 5 from 24); its death gives keys:arena to
  everybody in the base. Checked with a scratch mod (the Guardian at once, a
  blast to kill it): out, dead, key given. Seven waves not played through.
  Track (2026-10-09): three laps of the Inferno track in a row each under
  the engineer's 32.0 s give keys:pista; the count is kept by name (a
  player's state in the pista room is made again on every return from the
  Inferno) and told after each lap. Scripted, unloaded machine: laps 29.9,
  31.5, 31.7 s, the key given on the third (with the time raised for the
  check). No ghost kart: the engineer is a time. 32 s is the scripted
  driver's pace, so possibly too hard by hand -- to tune from play.
  Inferno (2026-10-09): games/inferno/mods/inferno_heart -- a hall of brick at
  the end of the arrival's east way (x 92..114, z 21..43) behind a gate of
  obsidian, four braziers at the ends of the other ways (one on a branch off
  the north way), lit by a right click; four lit open the gate. In the hall a
  cinder guardian 1.7 times a cinder (tools/make_inferno_creatures.py
  --guardian, 120 hp) rises from a ring of glowrock; whoever came into the
  open hall comes back at its gate after dying, and is given keys:inferno
  when it dies. Nobody left in the Inferno: braziers out, gate shut. Checked:
  the west brazier lit by a real right click (--use-at), the rest by a
  scratch mod, the guardian risen, the player killed once and back at the
  gate, a blast on the guardian, the key given. A world made before keeps
  its old columns: --fresh.
  Inferno pictures (2026-10-11): the room serves games/inferno/textures and,
  for what that folder has not got, assets/textures (new server flag
  --fallback-textures). Before, the sandbox mods it links had no item
  squares (creative showed words) and their blocks baked placeholders: now
  283 of 349 items drawn. The bag's tooltip says "Iron pick" as creative
  does, not "tools:iron_pick". Open: no per-item label a mod can choose.
  Arpg effects and breakables (2026-10-20): core.render.fx (new, client
  only) draws an effect of light with the engine's shaders
  (src/client/render/fx_render.cpp, one quad and one GLSL program: glow,
  flame, ring and frost on the ground, bolt between two points, slash,
  sparks, vortex, added onto the frame, hidden by walls); the arpg's
  fireballs, bursts, nova, war cry, cleave, blows, chain lightning,
  teleport, town portal, level and potions use it instead of boxes. Seen
  in Xephyr one by one. Barrels, crates and urns
  (tools/make_arpg_props.py: pieces on a rig of their own, clips idle, hurt
  and death, the pieces thrown out and gone in a second; sounds
  synthesised by tools/make_prop_sounds.py): 3 to 5 an area, workers stood
  up within 24 of a player, struck and broken like a creature, with a
  creature's drops and no experience, broken until the world is empty. A
  warrior broke an area's three in 21 s for 28 gold and a potion. Open:
  sfx-gen (Stable Audio) is refused by Hugging Face, the sounds are made
  by hand and not heard here; none in the crypt.
  Arpg gear (2026-10-20): what a character has equipped is seen on them.
  tools/make_arpg_gear.py builds one model for every base but the ring (42,
  games/arpg/mods/arpg_core/gear_*.gltf) on the robot's rig: weapons bound
  to hand_R pointing as the robot's sword, bows and orbs to hand_L, shields
  to forearm_L, the rest to head, chest, hands, feet, hips; the trim is
  tint_1, gold on a unique and green on a set. core.player_wear (new,
  protocol 102: ten slots of a mod's models on a Player) dresses the body
  for everybody, and the client's own drawing of it. tools/make_arpg_clips.py
  adds six clips to the robot and the three species (bow_shoot, staff_cast,
  cast_nova, war_cry, dagger_stab, spin_slash), turns only, made from each
  body's own file; arpg_game plays them, with sword_slash and sword_overhead,
  for every blow, shot and spell (core.player_clip until the clip has
  played). Seen in Xephyr on all three classes. Open: fitted to the robot
  only, a species wears the robot's pieces; a weapon held at rest points
  ahead, as the robot's tools do; nothing shows a ring.
  Arpg rework (2026-10-18): the endless waste is gone. arpg_game lays out a
  town (Ashbury: smith, jeweller, alchemist, stash, waypoint) and three zones
  of areas and corridors cut through high ground two nodes up (three hid a
  body by its foot from the camera): the Grey Waste (levels 1-5, lord the
  Grave Lord), the Barrows (6-9, Emberwing), the crypt (10-12, moved to
  x+1317 z+1013). 40 encounters (packs, champions, named ones) wake within 20
  and stay dead until the world is empty; 6 chests; waypoints at the town and
  each zone's first area; a town portal per scroll (B). Classes warrior,
  sorcerer, hunter, 4 skills each, 5 stat and 1 skill point a level; items of
  43 bases, 54 affixes, 12 uniques, 3 sets of 3, rolled by level and magic
  find (3000 rolls at magic find 0-200: 27% normal, 53% magic, 14% rare, 2.5% unique, 3.4% set,
  every code read back the same). Creatures have 1000 engine points each and
  their real life is the mod's; core.player_max_health (new) holds a
  character's life. Checked in Xephyr: class pick, shops, buy and sell,
  equip and the level rule, stash, waypoints, the portal there and back,
  fighting, levelling, death back to town. Creatures of the first areas
  strike less often (core.mob_blow_every, new: twice their kind's wait at
  level 1, their own from level 6); the warrior strikes 30% sooner and his
  blow takes half on all within 1.8 of the one struck: a fresh warrior
  cleared the first area's packs to level 2 without dying. Nobody hangs from
  a ledge there (core.ledges(false), new, protocol 101): jumping at the high
  ground left the body at its foot. Balance model (2026-10-19, the mod's own
  tables, a full clear, 3/5 of the points in the main stat, every skill point
  in the first skill, no potions): one arrives at the Grave Lord at level 5,
  at Emberwing at 11 (zone 9), at the crypt at 12. Seconds to kill the lord
  against seconds to die, starting gear: Grave Lord warrior 27/10, hunter
  35/8, sorcerer 6/6; Emberwing 38/9, 42/8, 6/6; the custode 78/16, 88/14,
  10/10. With the best base and +50% damage everywhere the melee classes kill
  in 7-22 s. The fireball ignored gear: the sorcerer was the same either way.
  Changed: the two lords have 0.6 of their life and strike less often (Grave
  Lord every 1.4, Emberwing 2.0), the custode 200 life (was 220) and 6 a blow
  (was 4); the fireball is 4-8 and half the staff's hit, +20% a rank, 7 mana
  (was 8-14, +30%, 5). After, starting gear: Grave Lord 15/14, 18/12, 9/9;
  Emberwing 21/11, 20/10, 6/7; with the gear above the custode 11/15, 12/12,
  17/9 (starting gear there 57-79 s). A sorcerer's first creatures still take
  two fireballs. Then fought for real (a server-side pilot in a scratch copy:
  a client in Xephyr, the lord's encounter alone, skill 1 whenever there is
  mana, a potion under 40%): every blow knocked a creature back, so the
  Grave Lord never came within its own reach of a warrior (20 s of melee, 4
  life lost). Champions, named ones and lords now stand their ground. After,
  starting gear, 8 potions: Grave Lord (level 5) warrior 20 s and 2 potions,
  hunter 13 s and 1, sorcerer 8 s and none; Emberwing (11) hunter 18 s and 6,
  sorcerer 8 s and 2, the warrior used all 8 and died at 31 s chasing it 70
  nodes out of its lair (an archer backs off from a body near it). A lord
  outside its lair's area is now put back where it waited (checked every
  0.5 s): Emberwing went from 1378 back to 1362 and never left area 15. Then,
  starting gear: the warrior brought it to 43 of 564 and died at 26 s, the
  hunter to 12 and died at 16 s, each with a potion left (drunk under 40%).
  Open: Emberwing at level 11 is lost about as often as won without the gear
  of the Barrows; the warrior there walked by the pilot through walls. The
  custode (level 12, placed on the crypt's third floor, its skeletons as they
  come): with starting gear the warrior and hunter died leaving it 1015 and
  811 of 1674, the sorcerer killed it at 18 s with 6 potions; with the best
  plain base of every square at level 12 (the smith's) warrior 20 s and 3
  potions, hunter 20 s and 4, sorcerer 14 s and 3. Nothing changed. With
  decent gear (the best base of every square, magic: the best damage prefix
  on the weapon, the best life suffix on the rest, mid range; life 128 at
  level 5, 205 at 11, 287 at 12 for the warrior) no lord costs a potion:
  warrior Grave Lord 14 s (lowest 61 of 128), Emberwing 13 s (95 of 205),
  custode 14 s (159 of 287); hunter 15 s (44 of 106), 5 s (123 of 171),
  12 s (149 of 240). Six life suffixes nearly double a character's life:
  what turns a lord from a near death into no danger. Then by hand
  (2026-10-19): the real client in Xephyr driven by xdotool, the mouse on
  the lord's ring found in a screen grab, the skill clicked while there is
  mana and the left button held otherwise, Q under 45%. Warrior, starting
  gear: Grave Lord won in 16-21 s for 2-3 potions; Emberwing lost at 34 s
  leaving it 69 of 564. Warrior against the custode with the smith's bases:
  lost at 41 s leaving it 500 of 1674, out of sight 70% of the time -- the
  floor's walls are eight high and hide a strip of 5-6 nodes along the two
  on the camera's side, and the custode backed off into it and shot from
  there. It is now kept within 5 of its throne (arpg_crypt): three fights
  won in 27-28 s for 4-6 potions, out of sight 0-1%. Hunter there: won in 17
  s for 3 potions, lost leaving it 74, lost at 11 s with 8 potions unused
  (a fireball and the skeletons' arrows within one look at the life globe).
  Open: holding the left button strikes again every 0.15 s and drops a
  skill waiting for the creature to come within reach, and the right button
  does not repeat when held: a bash wants a click each time. The "audio: no file for
  sound" lines of those runs were the pilot's: a --play-seconds client opens
  no audio device; the files arrive whole; a world from before keeps its old waste around x 64 and starts
  the new ground at x 1000 (the arrival is moved there).
  Arpg (2026-10-09): games/arpg/mods/arpg_crypt -- east of the spawn room a
  mausoleum's steps go down into three floors of a crypt open to the sky
  (the camera looks down; floors at 15, 13, 11, 20 by 20 and 24 by 24),
  each woken by somebody coming down (zombies, skeletons, imps), its seal
  to the next floor broken when its dead are down. On the third the
  Custode delle tombe (tools/make_crypt_boss.py, Blender: robed, a skull, an
  iron crown, a staff; 300 hp, fireballs) calls up two skeletons every 8 s;
  its death gives keys:arpg to everybody who came down to it. A death puts a
  player back on the floor they reached. arpg_game: drops lie at the height
  of what was killed, and no packs come round a player under the ground.
  Checked with a scratch mod (blasts for the fighting): both seals broken,
  the Custode risen, killed, the key given; floor 1 and the middle of floor
  3 seen from the camera. Open: within about 8 of a floor's south and west
  walls a body is hidden behind them from the camera.
  Backrooms (2026-10-09): games/backrooms/mods/backrooms_exit -- a lit exit
  room in Level 1 at x 96..104, z -72..-64, a doorway in each wall, the key
  on a stand; a radio (tools/make_radio_sound.py) crackles from the exit's
  way, every 1.5 to 6 s by distance, and a notice every 25 s says weak,
  clear or strong; coming down to Level 1 sets the respawn there. Checked
  with a scratch mod (put on Level 1, then by the exit, then in it): the
  notices, the room in a screenshot, the key given. The radio not heard: a
  timed client has no sound (see memory gui-runs-in-xephyr).
  All seven trials are built.
  Backrooms compasses and slides (2026-10-11, from LAN play: "no items help
  to find the way; slides jerky, ride inside them"): two compasses given on
  coming in (a right click on a block: metres and side to the way in or the
  way out, or the nearest stairs/hole to the right level); a fast downhill
  belt is now a chute the engine rides (chute_at, player_update): on the bed
  line, round turns, 2 to 5 m/s, client-predicted, no server pushes; slides
  not solid. Checked in Xephyr with a scratch mod: straight and turning rides
  on the line, the robot sat inside the chute, the compasses' sentences on
  three levels. Open: the ride's feel at a real frame rate not tried.
  Then (same day, from the user): compasses no longer given -- the way in's
  in 2 of 5 rare chests (backrooms:chest, 25 in 1000 empty rooms, filled on
  first opening), the way out's crafted from 2 Bacteria filaments + 2
  Smiler teeth (core.mob_drop, 0..1 each). Checked: a chest at room 4,-4
  opened by a right click showing compass and filament, 163 of 400 chests
  with a compass, the recipe registered. Open: chests only in newly
  generated parts of a saved world; drops and the craft not played.
  And a torch (backrooms:torch, 1 chest in 8; mods/backrooms_hand, side
  both): a right click anywhere with it in hand lights a beam 14 long that
  stuns Bacteria and Smilers 4 s (core.slow_mobs 0.05), 6 s to recharge,
  the beam drawn on every client. Checked by hand in Xephyr: two Smilers
  at the player, both stunned (0.1 m/s for 4 s, no blow), then on again;
  the notice and the torch in hand on screen. Open: no sound for it.
  Facing (same day, from LAN play: "the other player did not always turn the
  right way"): the server kept a player's Look as the look vector's x and y,
  and clients drew x as the yaw in radians -- right only facing near -z. Now
  set_look stores yaw and pitch (session.cpp). Round a slide's turn the view
  turns with it, and a rider is sent facing down the chute. Checked: two
  clients, the other seen face on and in profile; the look on a turning
  slide going from -z to -x.
  Ways out (2026-10-10, from play: "an exit different from the entrance, in
  every world"): each world's way home is at the end of its trial, not where
  one comes in -- backrooms in the exit room, arpg on floor 3 by the Custode,
  yafps behind the reactor, inferno by the heart, pista past the finish (new
  openings: pista @sandbox/0, inferno @sandbox/1), chambers, parkour and the
  lab as before. core.player_carry now goes with a body on foot too; mods/keys
  marks "exit" near a world's way home and puts the body in the sandbox at
  that world's arch, a row along the plaza's north edge (z 96, x 24 + 8n),
  not at its gate. Checked: parkour summit to the plaza (57, 21.5, 88.75).
  Open: the other worlds' ways out not walked through one by one.
  Sealed ways out (2026-10-10, from play: "a red membrane on the exits that
  opens, from the sandbox side too, once the world is won"): the arches are
  mouths now (sandbox openings 8-14 -> each world's way out; protocol 90: 16
  mouths, 32 openings, message Seal), and core.portal_seal(player, mouth, on)
  shuts one to one player: a red membrane, no crossing, no Redirect. In a
  world the way out is sealed until its trial is passed there; in the sandbox
  the arch until the key has been seen in the bag. The window behind two
  mouths of one world streams round the nearer one's far end. Checked in
  Xephyr: arches red but chambers' (key given), walked through it into
  the chambers' way out; arena's way out red, keys:earned emitted, opened onto
  the plaza. Open: the seal is the client's to keep for the seamless
  crossing (the far room does not check it); 15 mouths' render targets are
  made as each links (VRAM not measured).
  Key altars (2026-10-10, from play: "keys to pick up from pedestals at the
  end of the trials"): no trial gives its key any more; it emits keys:earned
  and mods/keys keeps who passed (storage "won:<who>") and sets a gold altar
  one forward and two aside of every way out (not in the lab). Walking up to
  it gives the key -- not a click: the rope gun and the guns own the right
  button. Checked: parkour summit, earned, walked to the altar, key in bag.
  Open: the other six altars not walked to one by one.
- The laboratory (2026-10-09): games/lab/mods/lab (side both) and lab_core.
  l'Ospite (tools/make_ospite.py, Blender: a dark mass with a ring of eyes,
  a maw, tentacles; its cage of light and its orb) is the lab's own entity,
  rigged since 2026-10-10 (body, jaw, three bones down each of seven
  tentacles; idle, walk and run sway the tentacles in a wave, wider as it
  goes faster -- the engine picks the clip by its speed, as for any body;
  seen in Blender frames and in game; since 2026-10-10 it opens its maw as it
  throws and plays its death before it goes, by core.ecs.clip),
  1000 points, moved by the mod. Phases: scudo (two generators behind glass
  in the north wall, reached by a portal gun of the lab's own -- a copy of
  chambers_game's wall shot, its portals rimmed blue and orange on every
  screen as in the chambers since 2026-10-10), assedio (the base's shotgun, the arms of
  yafps_core), vetta at 70% (the floor gives way; up to the gallery by the
  chimneys, station 4's, or the rope gun), fuoco (lava to the gallery, it
  heals while any of three valves is open; lava burns only those without the
  Chiave del Fuoco), buio at 45% (lamps dead, it is invisible and heard,
  tools/make_radio_sound.py's breath; the Chiave dell'Uscita shows it for 3 s,
  and only then can it be hit), gelo at 25% (fast; the Chiave della Cripta's
  frost slows it 4 s), corsa (track_core's kart on the gallery, 30 s up the
  tunnel north, then the credits and the way home). A player back from dying
  is left alone 5 s. Nobody left: the lab built again.
  Then (2026-10-10, from Giacomo's notes): it was never hit -- a mod thing's
  box was 0.6 at its feet, the tentacles' tips; core.ecs.box gives it 1.1 by
  3.2 now (10 of 10 shots at its body hit). Guns as the base's: a pistol that
  never runs dry (80 reach), the shotgun on shells, the chaingun on bullets,
  ammo on the HUD. Every 16 s (its "summon" clip) two imps or ghouls of
  the base come out of a violet ring, on the floor while there is one and on
  the gallery after; dead, they leave the chaingun the first time, then
  shells, bullets or a medkit, picked up by walking over. It rises and sinks
  between the floor and the gallery (20 s a swing) and faces whoever it goes
  for (core.ecs.set yaw). Every 9 s (6 in its last form) its "laser" clip:
  tentacles up and aimed, a dashed red line for a second, then three beams
  for 6 to whoever is on the line with nothing between. The fall back to the
  gallery happens only while the floor gives way; after, out of the pit by
  a parkour route built as the floor goes (2026-10-10, in place of steel
  rungs): the south chimney (between the pit's wall and one to 13, onto a
  ledge at 13), a wall run east over a gap of 8, the east chimney (between
  the pit's wall and one to 20, onto where the floor was), a lane round the
  hall's east chimney into it from the north, and up it onto the east
  gallery; light panels, yellow marks at each start, lamps in the pit's
  walls. Each station is the parkour world's cells; not climbed by hand.
  The torch and the frost were the keys held in the hand, which nothing
  showed (asked from play, 2026-10-10): they are items now, lab:torch (a
  flashlight) and lab:frost (an ice crystal), Blender models and icons from
  tools/make_held_models.py, given on arrival after the guns; the torch
  throws a cone of light that is seen, and the dark's notice says to take it
  and aim it at the breath. Checked scripted: the Ospite shown by the torch.
  The escape (2026-10-10, from play: "the kart does not move"): getting in
  left the kart block where the body was put, inside it -- the block goes
  now, as in track_game, and comes back for another try. The tunnel is a
  road of track pieces two squares wide out in the dark north of the hall,
  laid by track_core's turtle: north, a banked right turn with its tilts, a
  hill, a banked left turn (lab_core's own left tilts), a platform with the
  way home; a lamp six over every square, the pieces light grey. 60 s from
  getting in; off the road into the dark is back to the kart. Driven by the
  scripted driver (--walk-path through the turns' arcs): the whole road,
  banked in the turns, 30.3 over the hill, at the platform with about 25 s
  left, 'anna is out'. Checked with the scratch script:
  the shots, the laser seen aiming and hitting, four creatures blasted and
  their drops, every phase to the credits.
  Checked with a scratch mod doing the fighting (generators taken, blasts,
  valves shut, the kart mounted and put at the tunnel's end): every phase in
  order, a screenshot of each (the cage, the lava, the dark, the last form,
  the kart in the tunnel, the credits), 'anna is out'. Not played by hand:
  the gun's shots, breaking a generator by hand, the climb to the gallery,
  swimming to the valves, the torch and the frost by real clicks, driving
  the tunnel; the numbers (1000 points, 36 a shotgun shot, orbs of 3) are
  guesses.
- The pedestal's panel was not seen on screen (the scripted click did not
  reach it). The keys have held models since 2026-10-09
  (tools/make_held_models.py), seen in the hand.
- The mayor's line about the portals is not written (mods/villagers is at 194
  of 200 locals).

**Network client's own checks in Xephyr (2026-10-11).** Its frame check
asks for more than 30 FPS and Xephyr draws with llvmpipe: a kart lap through
the Inferno averages 29.5-29.7 (2953-2973 frames in 100 s), the plain
sandbox 24-25, the chambers atrium 670-2400. That one red is the display,
not the client; on the real GPU it does not apply. The other reds of a kart
lap were the checks': fixed the same day.

## The sandbox: what comes next (planned 2026-10-28)

Asked: put every suggestion on the plan and start. In order; 3 and 13 touch
the villagers and start with questions (memory: villagers-need-design-first).
1. Weather -- built 2026-10-28 (`mods/weather`, `core.overcast`). One sky
   per world: clear 10-20 min, then rain (one time in four a storm) 3-8 min;
   snow in tundra/peaks, nothing in the desert. Drawn round the body (450
   drops, 600 in a storm, 360 flakes, stopped by what is overhead), sky and
   clouds greyed by core.overcast. Rain waters soil under the open sky
   (farm:rain); snow settles a layer near players (87 layers in 12 s, forced
   in a test); a storm strikes every 6-18 s near a player, 8 points within 3
   nodes. /weather for admins. Seen in Xephyr (rain, storm, snow). Open: the
   bolt not caught in a screenshot; no rain sound (no looping sound for a
   mod); core.light is not dimmed, so creatures do not come out in a storm;
   thunder is synthesised (tools/make_weather_sounds.py), the sfx model is
   gated.
2. Seasons -- built 2026-10-28 in `mods/weather` (`core.node_foliage`,
   `core.foliage`: a vertex byte and a tint in the world shader). Five days
   a season, the day counted in the mod's storage. Leaves (not spruce) fade
   to orange in autumn, drab in winter; clear spells and storm odds by
   season; autumn crops half as often, winter only under lamp light 9+
   (farm:season); winter snows on every biome but the desert, and its snow
   melts in spring. /season for admins. Seen in Xephyr (autumn and winter
   leaves); seasons and winter snow checked headless. The traders' prices
   follow too: food doubles in winter, wood in autumn (checked headless).
3. Village events: caravans on the roads, night sieges, a harvest feast.
4. Boats -- built 2026-10-28 in `mods/boats` (`core.body_boat`: bit 2 of
   BodyMovement's moves byte, which a 109 client ignores and swims). Five
   planks; right click water with one to sit in it at the top, a shore to step
   out, or run aground for half a second: back in the bag. The body floats 0.35
   under the top and goes at its sprint, twice a swim, drifting (grip 1.2). The
   hull is worn on the root bone, the body sits in drive_idle (a mod clip now
   wins over the swim clip). Steered: A/D turn the heading (and the view) at
   1.8 rad/s, W/S row along it, the keel takes off sideways drift (4/s). Under
   way it shoves creatures in the cell off the bow aside (core.push_mobs), two
   hulls within 1.5 bounce apart at 4 m/s (core.player_launch), and the oars
   sound every 0.9 s (tools/make_boat_sounds.py). Seen in Xephyr (steering,
   third person); a cow 3.5 ahead pushed a node aside; two clients put 0.9
   apart drifted 6 apart. On a swell since: heave 0.05 (server positions, so
   everybody sees it; measured 19.61..19.70 about 19.65), roll and pitch 0.06
   rad and a lean out of a turn -- drawn on the rider's own body only, as
   body_roll is not sent for a body without a chassis. A creature sunk to the
   bottom is passed over: it is under the hull, as it would be.
5. Minecarts -- built 2026-10-28: `mods/carts` and rails in `mods/building`
   (its old decorative rail is now ridable), powered by `mods/cinnabar`.
   Engine: `core.node_conveyor`'s `rail` (NodeDef::rail, protocol 110: bit 32
   of a block's body flags, which a 109 client refuses) and `core.body_cart`
   (bit 3 of BodyMovement's moves byte). A rail is a slide only a body in a
   cart rides -- level, up or down, turning where a rail on its own level runs
   into a side -- brought up to its speed at 4 m/s^2 and down at 20, a slope
   giving 8 m/s^2 a node of rise, top 12. Rail 6 m/s; powered rail 11 on and
   a brake off (a station: forward pushes off at 2); a switch turns right
   while powered; slopes up and down. The server counts a cart on a rail as
   standing (no fall, no rise refusal). Checked in the client: straight,
   corner, slope up, the brake stopping at 6 m/s within a node, a cinnabar
   block beside it sending the cart off, a powered switch turning it right and
   down a slope; seen in Xephyr sat in the cart. Since: corner cells draw two
   quarter arcs (building:rail_right/_left, swapped in by mods/building from
   the rails round a rail laid or broken, as the engine rides them) and a
   powered switch is building:switch_on, a right arc; rails are ridden both
   ways (chute_at takes the way the body goes, or looks standing, and turns
   the tangent and the rise round; a corner also joins a side rail lying
   either way) -- checked: the L track ridden backwards round its corner;
   wheels clack every two nodes (tools/make_cart_sounds.py); boat and cart
   have icons (tools/make_item_textures.py). Open: a rail laid by a mod
   (set_block) keeps its shape until a hand lays or breaks one beside it. Found on the way: the recipe
   clash message now names both results, which showed rocks:stone_bricks never
   craftable (craft:brick's shape) -- now four smooth stone.
6. Travel -- built 2026-10-28 as waystones (`mods/travel`), not engine
   portals: the sandbox's mouths are nearly all taken (kMaxMouths 20). Rule
   and cost chosen without Giacomo (he asked to go on): a waystone works on a
   village's ground (villages:extent, known before mods/villagers makes it a
   territory) or a territory, else it goes back to the bag; touched, it is
   remembered per player name; the panel lists the remembered ones nearest
   first (8 at most), an emerald every 200 nodes, one at least. Every village
   laid out since has one by its well (where a lamp would stand); one is made
   of smooth stone round an emerald and a cinnabar. Whoosh synthesised
   (tools/make_travel_sounds.py). Checked on a server with a client, seed 1:
   the stone by the well of the village at -161 116; one laid off any ground
   given back; one on a territory 300 east attuned; the trip there cost 2
   emeralds and put the player beside it. Open: villages made before today
   have no stone (craft one); nothing stops a trip into a stranger's
   territory -- its own rules apply there.
   Since (Giacomo: the limit is portals loaded at once, not portals in a
   world): core.portal_open is VIRTUAL -- ids unbounded, lowest free, i xor 1
   until linked, as before -- and every half second the server
   (schedule_portals) gives its 32 openings to the portals within 160 nodes of
   a player and to what those lead to, nearest first, and takes them back
   from the rest; mouths onto other rooms are not handed out. Checked: 102
   opened, 32 live; the player moved 2000 east, 30 given and 30 taken back.
   mods/portal frames are no longer capped. A trip by waystone is now a real
   portal: paid, one opens beside this stone onto one beside that stone, both
   ways, for anybody, five minutes (a new trip from or to either replaces
   it). A stone's portal place (gate) is found while its ground is loaded and
   kept in storage, so a far stone can be reached. Checked on a server with a
   client: the portal shows the far stone's beach; walked into with W, the
   player came out 300 east beside it; with the time cut to 5 s both ends shut
   on time. Open: a body thrown in by core.player_launch went through the
   plane without crossing (walking crosses); at more than 32 wanted at once,
   the farthest pair may have one end live and the other not.
7. Map and compass -- built 2026-10-28 in `mods/maps` (side both). A compass
   (four iron round a cinnabar) held shows a dial at the top: N turning with
   the view, a needle home -- the bed last slept in (new event bed:home from
   mods/bed) or the world's spawn -- and the distance. A map (eight paper
   round a compass) held shows 512 x 512 nodes round its holder, north up, a
   pixel 8 nodes, only where they have been (48 nodes round each place),
   with the dial beside it. The server keeps it: a tile of 16 x 16 pixels, a
   character each (0 unseen, else a palette colour: three water depths, three
   heights of each climate from core.biome / terrain_height / water_level),
   a storage key a tile and pages of tile names per player, a tile a message
   (a message carries 512 bytes, a stored value 4096). Coloured on the server:
   a connected client has no generator (its core.terrain_height is 0). Seen
   on a server with a client: a route walked shows peaks, tundra, plains and
   water, and the same map after a restart. Open: the land as generated, not
   as built (no trees, houses or roads); the compass alone and the bed's home
   not looked at in a screenshot.
8. Rare structures -- built 2026-10-28 in `mods/relics`: one square of 384
   in two may hold one, chosen by the ground at a point of it (seed 1: 17
   temples, 36 wrecks, 19 ruins in 441 squares). A desert temple (stepped
   sandstone, a hall lit by two torches): turn four floor glyphs to the
   colours of the four wall tiles and the seal over the vault gives way. A
   wreck on a shallow sea floor: its logbook gives the paces from the mast to
   a chest buried two deep. A sunken ruin: break three urns for shards, set
   one on each pedestal and the chamber's seal breaks. Seals, tiles, glyphs
   and pedestals dig with nothing (dig_level 99). Loot: emeralds, gold, iron,
   lapis, now and then diamonds. The seed is kept from the first stage (no
   API gives it). Synthesised grind (tools/make_relic_sounds.py), a shard icon.
   Checked on a server with a client, seed 1: the temple at 60 -2865 (seal
   opened, chest with 8 emeralds), the wreck at 999 -3314 (logbook read,
   chest where it says), the ruin at 1238 -3186 (3 urns, seal broken); seen
   in Xephyr inside the temple hall, the wreck from above, the ruin's
   pillars and pedestals. Open: the nearest relic is some 3 km from the
   spawn on seed 1; the map does not show relics.
9. The Hollows -- built 2026-10-28 in `mods/deep`, UNDER the bedrock: the
   engine now lets a world hold blocks down to y -64 (kWorldBottom; a
   column's light box goes down there only where it has chunks below 0, so a
   world without them pays nothing) and a body dies under -80, not -16 (the
   old line killed anybody below -16). Not under every column: one square of
   512 in two has a hollow, 80 round its middle, y -61..-1, walled and floored
   with bedrock -- a deep layer everywhere would double every column's chunks
   on disk and on the wire. A sinkhole at its middle: a five-wide shaft from
   the surface through the bedrock, rungs down its north wall, a pool at -28
   on a floor three thick. Caverns from value noise on two lattices (Luau, no
   noise API), lava under -51, glowcaps (light 14) and glow moss (8) on
   floors, crystals (12) on ceilings giving shards. A tunnel to an arena 40
   off with an altar: a player within 18 wakes the guardian (the zombie's
   rig 1.5 times as big in slate and crystal, tools/make_guardian_model.py,
   its sounds pitched down; 160 health, 5 a blow); beaten, it drops its heart
   and the altar stays quiet (storage). Checked on a server with a client,
   seed 1, hollow at 338 -92: 126 glowcaps and 86 crystals by its middle,
   stood in the lit chamber at -28 (health full), the guardian woke and
   killed an unarmed player in four blows, a blast beat it and the heart was
   dropped. Closed after: a hollow's column took ~61 ms more than a plain
   one, now ~21 (the plain one costs ~29): the noise is blended in the plane
   once per lattice layer and down the column after, and what grows is read
   from the column's own array, not back through get_block -- of the 21, ~8
   are the set_block calls. A chunk below the player is now sent only after
   the one over it (src/core/net/interest.cpp), so a hollow is no longer seen
   from above through ground not arrived yet. The guardian came out of the
   dark like any creature (no spawn_on); now "deep:never". LURKERS live
   there: the zombie's rig at 0.85 in pale skin and teal eyes (14 health,
   3 m/s, 2 a blow, drop a glowcap), put by the mod on cave-floor slate 12..30
   from a player in a hollow, at most 4 within 32 -- the engine's own spawning
   wants light 7 or less, which the glowcaps leave almost nowhere. Checked:
   four lurkers round the chamber within 15 s, the guardian still kills in
   four blows. Open, not the hollows': for ~4 s after a teleport, cave walls
   of chunks whose nearer neighbours have not arrived show black in the sky
   (seen the same without mods/deep).
10. Alchemy -- built 2026-10-28 in `mods/alchemy` (side both). Flasks (3 of
    glass), filled by right clicking water or a full cauldron; a still (3
    copper, 3 bricks, a glass) brews a flask of water and an ingredient in
    10 s, glowing and bubbling, and puts the potion on top. Drunk like food,
    3 minutes, the flask back: night sight (glowcap), swiftness (sugar cane,
    walk 1.4), stoneskin (crystal shard, blows 0.5), mending (melon, regen 3),
    haste (amethyst, dig 1.6); a HUD list top right with what is left.
    Drowning added after (`mods/breath`, side both): with the eye in water
    15 s of air (ten bubbles over the hunger bar, shown while not full), back
    4 a second out of it, then 2 points a second -- an unarmed player dies in
    ~24 s. Water breathing (raw fish) stops the air going, through the
    "breath:water" event. Checked in a pool: drowned at the expected ticks;
    with the event, 20 s under water with no damage. Engine: core.night_sight
    (client; blocks and models drawn no darker than 0.55 x amount), and
    core.player_factors takes a `source` -- each source keeps its own four
    and the body gets their product (mods/territory passes "territory"), so a
    potion on owned ground does not undo the ground. Checked with a client at
    midnight: brewed by right click, drunk, the night lit up, both timers on
    the HUD. Open: effects are in memory (gone on a restart or a leave); a
    still brewing when the server stops finishes when next right clicked; the
    0.55 floor was not looked at again after lowering it from 0.8.
11. Fishing widened -- built 2026-10-28 in `mods/pets`: what bites is by
    the core.biome of the float's column -- lake 70 perch / 30 carp, river
    and gorge 70 trout / 30 perch, sea and beach 70 cod / 30 eel, ocean 50
    tuna / 50 cod -- and a goldfish 3% anywhere. Each its own icon and held
    model (tools/make_fish_models.py, the fish recoloured); all cook to a
    cooked fish (furnace, villagers' cooks), all are food (tuna 3). The toad
    fisher buys them: carp 6, trout and cod 4, eel 3, tuna 2 for an emerald,
    a goldfish 4 emeralds (the season's and stock's price rules apply).
    Checked headless: 1000 catches at a column of each water came out at the
    weights (sea 663 cod, 306 eel, 31 goldfish...). Then fished with a client
    (the bite forced, its window widened, in a scratch copy): 10 casts in a
    river at 160 120 gave 9 trout and a perch, 14 in the sea at 355 115 gave
    10 cod and 4 eels. The toad fishers catch by the same weights now (a copy
    of the table inside fish(): villagers is at Luau's 200 locals); a toad
    made by hand by a pond on the plains brought up perch and carp and
    cooked them.
12. Herding widened -- built 2026-10-28 in `mods/animals` and
    `mods/helpers`. Engine: core:used_mob now also for any creature the
    engine did nothing with; core.mob_kind, mob_love (in love as if fed),
    mob_keep (a wild animal kept: never sent away, saved with the world as
    a tamed pet is, line owner "*"; its young kept too), mob_shear_now (with
    `drop` false the caller takes the fleece), mob_become (another kind in
    place, keeping health, age, shearing and stable), mob_voice (a kind
    using another's sounds); core.mob_ids takes `all` for the animals too.
    Dyed sheep: a dye on a sheep turns it that colour (15 kinds
    animals:sheep_<colour>, tools/make_dyed_sheep.py recolours the models),
    shorn they give that wool, their lambs are theirs, white dye undoes it.
    Milk: a bucket on a cow; drunk, 4 health and the bucket back. A trough
    (5 planks and a wheat) is a stable: filled with wheat or seeds (16), every
    10 s it keeps the animals within 8 and puts two of a kind that eats it in
    love, until 8 of that kind. Robot order "Tend animals": shears the sheep
    round its base into its bag, and feeds two near each other from its bag
    or a chest. Checked with a client: a sheep dyed red, then shorn (red
    wool on the ground); a bucket of milk from a cow; two cows at a trough
    made a calf, 8 wheat down to 4, and all four were still there after the
    player spent 20 s 300 nodes away; a robot sheared a white and a blue
    sheep (2 + 2 wool in its bag) and fed two cows into a calf. Seen on the
    way, not fixed: for ~10 s after joining, creatures are drawn as boxes
    while the client loads the 456 models. Open: no cake or cheese from the
    milk; a kept animal can only be let go by a mod (core.mob_keep false).
    Closed after: core.item_returns (engine, RecipeSet::consume) leaves a
    spent item's container in its square -- a bucket of milk makes 2 cheese
    or, with wheat, sugar cane and an egg, a cake (food 4 and 10), and the
    bucket stays in the grid (checked against the engine's craft_take: the
    square held lakes:bucket x1 after). A stick on a kept animal lets it go
    (no trough keeps it for 10 minutes): two kept cows, one let go, the
    player 300 away for 30 s, one cow left when back.
13. Errands -- built 2026-10-28 in `mods/villagers` (design agreed the same
    day). At most three a village, posted only when the treasury can pay,
    their pay set aside when taken: the merchant's "bring" (what the
    village's chests are short of, two bundles) and "procure" (diamonds,
    crystal shards, lapis...), the mayor's "craft" (an iron pick, a bow, 32
    torches...), "hunt" (3-6 creatures of the night killed in the village at
    night, counted on core:hit), "carry" (a parcel to another village the
    world has made, paid on arriving within 32 of its middle) and "explore"
    (stand within 12 of a place 300-600 away, then tell the mayor). Seen on a
    notice board (villagers:board: by every new village's well, or crafted
    from planks, paper and a stick), taken and handed in at the mayor's and
    merchant's panels ("Errands..."). One at a time; a day each; not taken it
    goes, taken and not done it goes back on the board with its pay back in
    the treasury, nothing else lost. Each done is counted per village and
    player; from five, the merchant sells at a fifth off. Checked on a server
    with a client, village at 411 -356: errands posted (bring 64 cobblestone,
    explore, hunt, craft, procure); a bring taken and handed in (treasury 32
    -> 29 set aside, 3 emeralds paid, the cobblestone into its chests); an
    explore done by standing there; a carry to the village at 625 -417 paid
    on arrival; a hunt of one done by killing a zombie at night with a
    sword; the board and the mayor's errands panel on screen; the board by
    the well (405 22 -356). Open: the carry's goal and the explore's place
    are said as coordinates only (no mark on the map); the board's panel
    only shows, it does not take.
14. Blueprints -- built 2026-10-28 in `mods/blueprints` (design agreed the
    same day), with mods/villagers and mods/helpers. An empty blueprint (2
    paper, a lapis: two) used on two corners copies the box between them,
    at most 32 x 24 high x 32, block and turn, into a blueprint: an item
    numbered by the engine's new core.give_item `id` / core.held_id /
    held_set_id, its blocks in the mod's storage (palette + runs, in parts
    of 3800 bytes). Used, its panel lays it out with its corner on the block
    clicked, turned to the player's side (villagers' own frame, so the
    builder agrees), copies it onto an empty one, or takes the layout away.
    Laid out it is a GHOST on the player's screen (engine: client
    core.ghost_cell / ghost_clear, drawn with the track pieces' see-through
    triangles; core.node_colour, as a networked client does not know the
    colour of a block only the server registers); it shrinks as blocks go
    in and "The blueprint is built" ends it. Built by hand; by the village's
    builder ("Build the blueprint I laid out...": inside the village's
    ground, 4 emeralds + 1 a 40 blocks, wood/stone/earth from the gatherers
    as for a house, every other block handed over by the player at once --
    an order of kind "blueprint" whose plan is asked of mods/blueprints by
    event); or by a robot's new order "Build my blueprint" (blocks from its
    bag or the chests by its base, from outside the layout, says what it
    lacks once a minute). Checked with a client: a hut copied with two real
    clicks (2 x 3 x 3), its panel opened and "Lay it out" clicked, the ghost
    in the blocks' colours, finished by hand ("built"); the builder built a
    3 x 3 x 3 planks-and-glass hut at 411 22 -360 (4 glass taken from the
    player, 6 logs asked, 25/25); a robot built 23 of 25 from a chest that
    held only 2 of the 4 glass. Open: a robot reaches some four up from the
    layout's bottom (no scaffolding); the ghost is sent whole again on each
    change (a big blueprint is many messages).
15. More decorative blocks: stained glass, fences, walls, tiles, furniture.
16. Plaza games on the chess frame (draughts, mill), a scored PvP arena.

To fix after item 14 (asked 2026-10-28): the railways done properly in
Blender -- straight track, slopes up and down and curves as real models
(sleepers, rails), not node boxes; a proper cart model; the player's clips
for getting in and riding, seated in the cart. Use the MCP servers
(Blender) where they help.

## Portals and movement

**OPEN-2 -- a crossing may wedge the player in the exit block.** Reported from
play, never reproduced. Untried: a destination more than three nodes higher
than the source, an arrival inside another body, a crossing taken while the
camera warp from the previous one is still settling.

**OPEN-45 -- above ~120 m/s a body outruns the streamed ground.** At 256 m/s
only 64 nodes of ground stay ahead. Options: an interest radius that grows with
speed, a centre led ahead of the body, a bigger budget.

**OPEN-46 -- a claim whose path turns back on an axis is refused.** Only bodies
on foot are walked; a kart is judged by the server's copy (protocol 46).

**OPEN-47 -- vehicles.** On foot is still check_move's (budget, walk, rise).
The kart's copy is bit for bit on one machine and one build (libm's trig);
a chunk a client says it lacks is air to it for 5 s; three lost claims in a
row put the chassis down afresh; no modified client was run against it. Other
karts do not collide with yours. Nothing in `games/track` sits a passenger
(`core.ride` exists).

**Portals built from blocks.**
- The picture through a floor or ceiling portal is upright from one side only;
  floor to floor is a somersault.
- A fifth opening in sight is not drawn.
- Ctrl+click moves an opening out of its frame and the menu does not follow;
  a frame taken apart by a mod's set_block stays open.
- A model block, and a robot, are not cut at a portal (OPEN-29).

**Closed portals (2026-10-10, protocol 89).** An opening that leads nowhere
is plugged by an animated membrane: blue or orange for a portal gun's end
(`core.portal_open`'s colour, passed by chambers_game and lab), green for every
other, across the doorway's mouth. A doorway never starts out past the face
of the node it stands in (a gun's opening is 0.1 off centre and stood 0.1
through its rim). Linked, the membrane opens from the middle onto the image
in 0.9 s, as does an opening that arrives already linked (not in a world's
first Portals). A mouth of the plaza not drawn (world not up, or not among
the two nearest) is plugged green too, and opens when walking up swaps it in.
Seen in Xephyr; not played with the gun by hand.

**Mouths and windows.**
- A window once open stays open and streams: a Scene cannot drop a world.
- A window shows no bodies.
- Gates are at fixed spots on the plaza, holes 1x2: a kart does not fit.
- A window's stream is centred on the world's `portal_a`.
- A ticket for one room is good at every room of the server.

**Kart and tracks.**
- Nitro flames, skid marks and the garage (2026-10-20, protocol 103): the
  chassis says each step whether the nitro burns and how fast it slides
  sideways (DriveLook, kSlideSeen 2 m/s); the server sends both bits of its
  own copy, so everybody sees them. Flames out of the flame_* joints (a jet
  and puffs facing the eye: a ribbon seen end on from the chase camera was
  invisible), marks a ring of 4096 strips fading in 30 s, per world. /garage:
  3 dye colours and 14 parts (tools/make_kart_parts.py), worn as
  core.player_wear pieces the client puts on the vehicle's bones. Xephyr,
  scripted: flames, a mark out of a corner, the parts and paint seen; lap
  18.07 s, 0 corrections. The scripted driver slides 1.5 m/s at most, so
  marks at 2 m/s were only seen with the threshold lowered; the panel's
  clicks were not driven (it was seen, values read from the store). Open:
  parked karts are drawn as made, not as their last driver's.
  2026-10-20, from play: marks gone on piece tracks -- a ray hit the piece's
  fill box (20.219), under its smooth road (20.250), and the strip lay 1 cm
  under the road; now at piece_surface. The garage shows the kart turning on
  the right (core.ui_model, protocol 104), 13.4 cells wide to fit 16:10.
  Ctrl drifts (protocol 105, DriveKeys::drift): over 5 m/s the rear tyres
  hold sideways 0.4 of their grip, the wheel turns 1.6 times as far, and the
  yaw is held to 1.4 rad/s (without it a kart with no countersteer spun,
  3.6 rad in 2 s). physics_box_test, half the wheel for 2 s at top speed:
  1.19 rad and no slide without ctrl, 2.93 rad sliding 107 of 120 steps
  with it; the server's copy replays it to the bit. Xephyr: 0 corrections.
  Never drifted by hand.
- Inferno laps 49-57 s scripted (the cavern was 31-34): not measured unloaded.
- A lap across rooms skips a tick or two of time.
- Track pieces: `core.set_block` on a fill cell leaves the rest of
  the piece; a wall ride was never driven by hand.
- Loops and adapters (2026-10-10): plans 6/7 (loop out a lane to the left /
  right, narrow 8.5 up, wide 11.1, kPieceMaxUnits 4 for the wide one's four
  squares across) and 8/9 (wide down to its right / left lane); the loops
  track west of the loop, from the Inferno road, its checkpoints the loops'
  tops crossed backwards (at a loop's bottom its way down is beside its way
  up: going across there skipped it). Kart rise 10 -> 12. Headless: a kart at
  24 m/s goes over upside down (179 degrees) onto the lane beside, the
  server's copy to the last bit; at 7 m/s it gets to 47 degrees and falls
  back. In Xephyr: the track builds and closes, 0 corrections up to the wide
  loop, its HUD counts 3 checkpoints; never driven round a loop by hand or by
  the scripted driver (it brakes for each point and steers by the heading,
  which flips over the top). A loop unrolled is a chicane: the lane's shift
  is steered (8 nodes over 19, 16 over 25 on two lanes).
  Kart seen through the road in a loop (2026-10-11): the chase camera stood
  on the world's up, so over a loop's top the head was outside it and the
  camera went on through the road (only blocks pull a camera in) -- the near
  plane cut the road away. Now on a road steeper than 37 degrees it eases
  into the kart's own frame (behind and over it along its up, half as far
  from 72 degrees on). Xephyr: near the top, the camera inside, road whole;
  a tumbling kart tilts the camera with it.
  2026-10-11: the way in went north beside frame A, and a kart turning
  there drove into the Inferno; now north along x 8..15 east of the track and
  west up a ramp (steps of 0.1 to the curb's 0.4) onto the start straight
  before its line. Scripted in Xephyr: up the ramp onto the road, 0
  corrections. Saved worlds get the new way too (store "loops" 2); the old
  one stays as paddock earth.
  Tall loops (2026-10-13): one lane three squares long is a loop with twice
  the line (64) and twice the height, 16.9 up; kPieceMaxHeight 12 -> 18 (the
  most the vehicle's chunk reach allows), pieces tall_loop_left/right. The
  loops track has them in place of the low left loop and before the low
  pair on the west side; its checkpoints on their tops (x -76, z 76). Saved
  worlds get the track laid again (store "loops" 3). Headless, the physics
  test's ride on a tall loop: over upside down onto the lane beside with the
  throttle capped at 24, 20, 18 and 16 m/s. Never driven round by hand.
  Stumbling out of them, from play (2026-10-13), found with a per-frame drive
  log: (1) coming down a loop the step hop's flat rays reached the hill
  beside the track, took its top for a step and set the fall of 23 m/s to
  4.5 up -- now only within kMaxTilt of level (headless, a hill 12 over the
  road beside a tall loop: 24.3 -> 5.3 m/s before, no drop after); (2) a
  wheel cast against UP skipped the road's facet past 50 degrees for one
  behind it, a one-frame stretch of the spring -- road against the
  chassis's up only, blocks against UP, the nearer; (3) the chase camera's
  yaw lag filled to 0.5 rad over a loop's top (the heading sweeps half a
  circle there) and swung back on the way out -- no lag counted past 37
  degrees. Open: a kart that comes down off the exit lane of the tall left
  loop rolls under it and hits the hill at x -81, where the loop ends.
- Power-ups (2026-10-13): a row of item boxes (2-5 across) ten nodes past
  every line of every track, the player's own included; a box gives one of
  eight at random (boost, triple boost, banana, shell, homing shell, ink,
  star, lightning), used with the left click, S held throws a shell behind.
  A kart holds two (a box taken with two held gives nothing): the first is
  used, the second moves up; the HUD shows it smaller above the first.
  Item box tool (track_core:box_tool, in /pezzi): a right click on a road
  puts a box over that cell, another on the same cell takes it away; kept
  in the store ("item_boxes"), laid with the rows at world_ready. Headless,
  by emitted core:used: two down (60 -> 62), one taken (61), a click on
  grass refused, 61 again after a restart. Never clicked by hand.
  Creative panel and jetpack in the track world (2026-10-13): mods/creative
  and mods/jetpack linked into games/track/mods; the world's fly speed 0 ->
  14 (also on getting out of a kart), mods/jetpack still deciding who flies.
  The jetpack has no `depends = furnace` any more: its recipe goes in at
  world_ready where the ingots exist (in the sandbox furnace loads AFTER
  jetpack, so a load-time check missed it; checked: recipe registered).
  The icons are 32 px now: the atlas takes one tile size, and 64 px ones
  were red squares in the bag and the creative panel. Xephyr by hand: C,
  jetpack from the panel, worn with shift-click, F and space -- flying over
  the loop, 0 corrections. The creative panel lists the power-up icons too.
  Missing icons in the track world (2026-10-17): its textures dir had no
  jetpack_jetpack.png (now a link to assets/textures) and start_line,
  checkpoint, offroad had none at all (drawn by tools/make_track_icons.py).
  Client log: all four "drawn"; not looked at in the bag by hand.
  Flight (2026-10-17): fly_hover with fly blended in by forward speed (full
  at 3 m/s), the body tipped back / onto its side up to 26 deg at 14 m/s,
  about its middle (fly_lean, own body only). Inertia: kFlyGrip 3 -> 2
  while pushing, kFlyCoast 1 on an axis with no key: a stop from 14 m/s
  drifts ~14 nodes. Xephyr by hand: every direction leans, drift visible.
  Track pieces held in the hand (2026-10-13): drawn as the piece in
  miniature (held_piece_model in src/client/main.cpp: piece_triangles
  unturned, longest side 0.24 of the forearm-sized unit, the middle of each
  role's tile, road/sides/dark curbs shaded 1/0.75/0.5) in place of a cube.
  Xephyr by hand: a turn in first and third person; dropped, on the ground too.
  Track editor (2026-10-14): track_core:editor (in /pezzi). A right click on
  a piece follows its track back to the start and on to the open end
  (core.piece_entry, new: the entry square, height, block and facing of any
  piece cell; piece_entry() in piece.cpp, the inverse of piece_origin) and
  opens a panel of the pieces whose entry (width, grade, bank) fits the open
  end, reversed ones too ("rev"); a click puts it down, Undo takes the last
  back (core.take_piece, new), Close circuit runs A* over (square, heading,
  height) with straights, turns, chicanes and rises of the road's width,
  avoiding any square with something at road height (all 64 columns, cached
  per search), 300 turtles a tick, at most 30 pieces / 20 000 turtles.
  Carried, every piece put down by hand opens the panel. lay() now goes
  through the same step(); the grand circuit and the loops track still close.
  Headless: the ring followed all the way round (wall ride, jump, banks) and
  called a circuit; a straight + turn2_left, Undo, and Close circuit found a
  5-piece way back. Xephyr by hand in the paddock: 32 pieces offered after a
  straight, a click put a turn down, Close circuit put down 22 pieces, and the
  editor then calls the track a circuit. The first try stopped at piece 8 on
  a parked kart (5 columns sampled), hence all 64.
  Limits taken on (2026-10-14): the closing searches flat first and lets the
  rises in only when that fails (rises still cost 4 squares extra); the width
  is part of the search state, with the adapters both ways, so a wide start
  closes to a narrow end (headless in a cleared field: wide_straight +
  wide_to_right, closed flat in 6 pieces with wide_to_left the other way
  round); core.take_piece in the local single-process world too; a dropped
  piece lies on the ground in miniature (kBlock * 1.5). Driven: a circuit
  the editor closed in the paddock (straight + straight2, closed with turn1,
  turn1, straight2, straight, turn1, turn1), four scripted laps in a kart in
  Xephyr, every point reached, on the road (y 20.25), 0 corrections.
  Picking from afar (2026-10-15): B with the editor in hand freezes the body
  (core.fixed_view from the eye onto the start of the piece on trial, at most
  40 nodes off), and the piece on trial is put down for real; A/D swap it for
  the next way that fits and goes down, left click keeps it, S takes the last
  back, F closes the circuit, B leaves. Keys on track_game's client half,
  events track_core:pick / track_core:pick_view (pcall'd: the Inferno runs
  track_game without track_core). Headless: trial, next, prev, put, undo,
  leave, then close in 5 pieces. Xephyr by hand: straight, 4 / 32 turn2 on
  trial, nothing fits against a wall said, S, F, B back to the eye.
  2026-10-16: B on an open panel was never heard (a free cursor sends the
  mods no key): the panel has a "pick from afar" button. A piece now takes
  the place of ground and blocks in its cells and empties 3 cells over its
  road; only another piece stops it (headless: turn2 into stone 1, over it
  0). Open: trying pieces from afar digs the ground for each, and undo does
  not put it back.
  2026-10-17: a left click kept whatever was on trial, so pieces went down
  nobody wanted. Now a grid of all 61 pieces replaces the hotbar while
  picking (new core.hud.item, core.hud.mouse): a click puts that piece on
  trial, W keeps it, a click off the grid does nothing. Xephyr: grid drawn,
  click on turn2 -> on trial, click off grid -> nothing, W -> kept, B -> hotbar back.
  Then A/D stuck on "nothing fits": take_back took a piece by the middle of
  its entry square at its height, no cell of 17 of 88 ways (banks, tilts,
  wall rides), which stayed down and walled off the open end. Now it looks
  for the cell entered there (core.piece_entry). Headless, put/take/put
  all 88 ways: 17 left behind before, 0 after.
  Server-side in track_game; boxes, bananas and shells are visible mod
  entities (models from tools/make_track_items.py, HUD icons from
  tools/make_track_icons.py, served as items of track_core). Boost and star
  raise sprint/accel (34/60, 30/40: new numbers rebuild the chassis with its
  speed); a hit holds the kart at 0 and turns it once with 16 player_place in
  0.9 s; lightning slows the others to 14 for 4 s. Xephyr, scripted with a
  test copy that uses each item 1.3 s after its box: all eight fire, own
  banana and own bounced shell spin the kart, 0 corrections; with a second
  client sitting in a kart the lightning spins it and the ink covers its
  screen. Never driven by hand (the left click). Open: the same odds for
  everybody (no race order to weight by); no sounds; the Inferno room has
  no boxes (no models there) and an item held is lost crossing into it;
  shells follow the ground three nodes down at most and do not climb loops;
  a box row whose cells are not road (over a loop's top) is skipped.
- Green on crossing into the Inferno and back (2026-10-11): the world arrived
  in kept which mouths it drew when last stood in (none, the first time), so
  the mouth just come out of played its 0.9 s green opening round the
  player. Now a world just arrived in takes what it draws as open. Xephyr:
  before, a screen of membrane 0.9 s after the crossing; after, no green in
  6 shots 0.2-1.1 s after it, and none 0.3-0.7 s after the way back.
- A 3x4 obsidian frame floating in C's hole (2026-10-11): the Inferno now
  loads mods/infernoportal (obsidian, lighter), which framed the world's
  second opening -- the plaza's way to the Inferno in the sandbox, C in the
  Inferno. Now it frames and lights only where that opening leads to the
  Inferno; inferno_track takes the stray blocks out of a saved world once C's
  cells load (10 in Giacomo's). Xephyr: gone in a new world and in a copy
  of worlds/track.
- Kart and foot between the rooms (2026-10-11): walking back into the track
  drew the body as a kart (the client's copy of track_game still had "kart
  1"); the track now says "kart 0" to whoever walks in. A kart got into in
  the Inferno by somebody who walked there has no lap ("kart 0 ..."), and
  came into the track as on foot -- driven as a kart, drawn as a robot; any
  carried kart drives in now. The client changes the view only when in/out
  changes (third person kept walking into the Inferno). Scripted lap fine;
  Giacomo saw the first fixed, the other two not tried by hand yet.
- A parked kart is one node solid (its nose and tail are drawn through).
- The chase camera (V) is not portal-aware.
- A crouch does not stop a body at an edge, and the box stays 1.8 tall.

**Parkour.** Nobody else sees a rope, a backflip or a kick; the validator
allows any height and the 0.8 box to anybody in a parkour world.
Since 2026-10-11 a parkour world (core.parkour) always runs at the sprint
(8.0 m in 1 s held, was 4.8 walking) and hands grab ledges as in the sandbox:
checked on a wall two high in the plaza, hung at 23 - 1.85, jump pulled up
onto it, 0 corrections. Applies to games/lab too. The tower's stations not
re-climbed by hand with the grab on.

**Chambers.** A crossing kept no speed into an opening higher than its other
end: exit_velocity evened out the raw height between the two (chamber 04, 4
nodes: 5.89 m/s in, 0 out); now the height the map sends the body to (fixed
2026-10-10, 5.89 out). Not played by hand with the mouse (aiming, picking up a cube,
difficulty). After `core.player_place` the client may keep only the yaw.
The single-process client's own crossings keep no speed.

## World, blocks and content

**The assault (2026-10-23).** A co-op final world against Basoalth
(games/assault, room `assalto`, sandbox mouth 16, gate at 90.5 21 76.5; opens
after the lab like the front, FRONT_OPEN keeps both open). Asked and chosen:
after the lab, allied bots fill the team (alone, a bot drives a jeep or mans
its gun), one shared purse spent at a shop at each taken stronghold, the core
behind four pylons with guard waves (memory: assault-world). A valley of four
strongholds (outpost, bastion, battery, basalt fortress whose gate is blown
in), each manned with a garrison and the front's guns and machines; taken at
the flag with none of the legion near, then the base moves there. Then the
hall: pylons, a core bare 18 s, Basoalth's orbs; the rising (a 34 m giant
through the roof); the escape down a 790 m road along a magma chasm, broken
behind by the giant, holes punched ahead, orbs at the jeeps, his head makes
him reel; one jeep at the rock and the way home opens to all. assault_game
is a copy of front_game turned PvE (allies team 1, legion team 2). The world
has no --world-file: it comes back whole once nobody is near. Engine:
kMaxMouths 16 -> 20, protocol 107. Models: tools/make_assault_models.py.
Measured with a scripted pilot (one player, garrisons killed by the script,
seated as gunner so a bot drives): all four strongholds, hall, rising and
escape in one run; the bot driver gets through every obstacle (lanes round
the barricades, the post, the pillars and the holes), about 60 s from the
garage to the rock with the giant some 50 m behind (10 to 16 m/s, +6 past 50
m); some tries end at a smash hole and the road is built again. Open: not
played by hand; numbers (garrisons, pylon and core hp, giant speed) await
tuning; no sounds of its own (the front's).

**The assault: a hundred allies, a shop that fits (2026-10-24).** Allies up
to 100, bought five at a time (150); with 102 bots on the allies' side
against the outpost the server took about 7% of a core. The shop is two
columns, 22 cells wide (a screen is about 24), seen whole in Xephyr at
1920x1080.

**The assault, from a played game (2026-10-24).** Giacomo played it through
and asked for more. Deaths were never logged: the engine set a killed body
whole again before core.damage answered, so it answered the full bar and not
0 (as documented); ServerSessions::hurt now answers 0 for a killing blow (the
front had the same blind spot). New core.mob_ride(mob, thing, x, y, z): a
worker carried on a mod's thing, put on its place by the engine right after
the machines move -- the bots in a jeep went a tick behind it (the mods run
before step_machines) and dropped under gravity between placements; seen in
Xephyr now seated, in place, frame after frame. Passengers (players and bots)
shoot their own guns and aim. Bonus grounds either side of every stronghold
(+3 a supply each, +50 when taken; an ally in three goes for them); legion
counterattacks of one or two jeeps at the base every 100-140 s; allied
builders (50 hp, unarmed, Blender model with a hard hat and a spanner, a
"build" clip) put the craters behind the front back as the dimension made
them (assault_core's column generator, gate and hall door left alone);
upgrades bought again and again at a rising price (arms, hulls, anti-blast
armour). Measured with the bots alone: four bonus grounds taken, nine craters
mended, a counterattack of two jeeps, three strongholds in 400 s.

**The assault: shots hold the giant back (2026-10-24).** Asked: shooting the
boss slows him; the boss centred in the chase. Every hit on him (a body box
28 high as well as the head, which counts double) adds to a heat that fades
with a time of 2 s; his pace is 1 - 0.6 h / (h + 150): one machine gun on
him (about 60 a second) holds him near 0.73, two near 0.63, never below 0.4;
the HUD says "RALLENTATO n%". He walks the road's middle line 10 behind the
front, on the ground in the valley and 12 down where the causeway was in the
chasm (before: 22 off to the east in the magma). Measured: heat 56, pace 0.96
from the bot gunners early on; seen in Xephyr right behind a jeep.

**The client asks GitHub for a newer release (2026-10-26).** Asked: the builds
point at this repository to look for updates. client/update_check: on the
title screen, once, a thread runs curl on the releases API (kReleasesApi in
core/version.h, newest first, pre-releases included) and compares the first
tag with kEngineVersion; a newer one is a "New version vX: download" button
that opens kReleasesPage. Options, Updates turns it off (update_check in the
options file), --no-update-check for one run. Seen in Xephyr pointed at
another repository's releases: the button under Quit, Options with Back moved
beside the title so the panel keeps its height. Against the real repository
0.9.0-alpha.2 finds nothing newer. Open: on Windows curl through _popen may
flash a console (WinHTTP if it does); no curl, no check, silently.

**The Blender files are in the repository (2026-10-26).** Asked: import every
Blender model and publish it. The 36 .blend of ~/kart_assets are in
assets/blender/ (23 MB), and the make_*.py scripts save there. Cleaned on the
way in: render paths (old names and scratch folders left in the fixed buffer),
and the datablocks and action slots of inferno.blend and held_items.blend still
named after the old creatures and items. Same objects, meshes and actions as
before, file by file. The unsplit kart glTF make_kart_parts.py starts from is
in assets/blender/kart_unsplit/ too. Released as 0.9.0-alpha.2.

**The plaza is quiet (2026-10-26).** Asked: hide the cubes that come and go
on the spawn plaza, too much noise. They were the spike's lamp blinking on its
tower (44 28 44, twice a second) and the beacon walking along beacons' pillars
(50..54 24 42); both gone, the cells cleared at load for saved worlds, the
beacons still light on a guard alarm. Checked headless: both mods load.

**Translations, and the account is per server (2026-10-26).** Asked: a way to
translate, Italian and English for now; the multiplayer account per server, not
before choosing it. client/ui/lang: assets/lang/<code>.txt, "text = traduzione"
with {} holes (translated too), looked up where text is drawn (panels, notices,
chat, hud text, tooltips), so a mod's strings translate with no API. it.txt has
the engine's menus and ~250 mod strings; en.txt the ~80 the worlds wrote in
Italian. Options, Language cycles them; LANG picks the first. Multiplayer opens
the list straight away; a server's row asks name and password for that server
(identity_for), a refused join asks again. Seen in Xephyr: menus in both,
accents drawn, a join with --require-identity. Open: strings a mod builds from
tables (lore pages, the diary, item names) and the engine's technical refusals
are not in the files yet.

**The assault: the bots shoot the traps in time (2026-10-26).** Asked: the
bots in the escape shoot the traps at the right moment. Their sight stopped
on the trap's own blocks (the valve's wheel, the rocks, the tank), so they
never shot one; now those do not count, and a trap is a target only when it
pays (A.trap_ripe): rocks any time, a valve with him within 150 (its 25 s of
lava), a tank within 55 (50 hp at about 4 a hit: it blows inside his 40),
ranked before him. Measured, player seated idle, bots alone: every valve and
rocks shot (valves with him 33-125 away), one tank of four blown with him 2
away, none wasted; the jeep reached the rock (before: caught at about 1100 m).
Open: the tanks are behind the jeep when ripe and mostly out of sight; maybe
too easy now with bots alone.

**The assault, bots at the wheel and in the last fight (2026-10-24).** Asked:
bots in machines ran into trees and stopped; bots to help in the final fight.
A bot driver now feels ahead (M.reach/M.feel): each line across its width is
walked over the ground node by node -- a step of one climbed, two high or a
low branch a stop, a fall of three a hole (not for the escape's crews, whose
road has a gap to jump) -- and it steers the nearest free way to its goal,
off the road heading for the road first, backing off when nothing is free.
Measured: three bot jeeps put in the marsh and the burnt forest (up to 178 m
off) reached the outpost in 20-25 s; before, stuck on the first trunk. On a
capture the allies on foot left over 100 behind come up to the new base (they
lagged hundreds of metres and never reached the hall); in the hall they go
for the pylons first (two damaged, one to 52 of 260, in 45 s with no player
shooting). In the escape all three jeeps go: one nobody took gets a bot
driver and gunner 4 s after the garage opens. The gaps after the ramps are
two now (a crew at 8 m/s fell in the three).

**The assault, third pass (2026-10-24).** Asked: a slow steady supply of
credits, allies that come back regularly, allied machines back when stuck,
the spawns moved properly after each capture, room to get through. 5
credits every 5 s while there is a fight; the allies' fallen come back in
waves every 15 s (at once after a capture); the base is the open ground north
of the last stronghold taken, with a park of numbered places (A.park), its
machines moved there straight away on a capture (a player's own stays with
them); one nobody drives that sits still 40 s away from the park goes back.
The outpost and the bastion have a gate north too, the battery's trenches a
gap on the road. Measured with a pilot that only kills each garrison and
stands at the base: the four allied bots walked to and took all four
strongholds on their own (about 60 s each), the jeeps in the new park at
once, the credits rising 5 each 5 s.

**The assault, second pass (2026-10-24).** Asked: a red sky with clouds in
whirls, the strongholds further apart in lands of their own, the legion as
zombies, demons and cyborgs. New core.world_storm(r, g, b) (protocol 108, a
flag and three bytes after the sky's in the Welcome): the sky of the hour
turned that colour, the sun and the clock kept, clouds of the same colour
(dark bellies, ember tops) turned about whirls in some cells of a 220-node
grid, swinging slowly with the wind. The valley is 1080 long: strongholds at
z 250, 480, 710, 905, each in its land (dead marsh, burnt forest, crystal
waste, basalt with magma cracks and ribs of bone), the escape road 1190 m
with a second set of obstacles in the chasm's north. The legion is three
mobs on the front soldier's bones (make_front_models.soldier takes a body):
zombies (cover, 28 hp, slow), demons (rush, 18 hp, quick), cyborgs (watch
and launcher, 26 hp); their voices the soldiers' pitched by ffmpeg. Seen in
Xephyr from a tour of viewpoints.

**The front's gallery (2026-10-18).** The way in from the sandbox comes out
in a gallery on the west ridge, halfway between the bases and 16 over the
field, glazed along it (front_core: tough nodes, so no shell opens it). Who is
in it watches and is nobody's. Its red and blue portals (core.portal_open,
linked) lead to the back of each base's bunker, and coming out of one is
joining that side; back through the bunker's portal is watching again.
Scripted with a client: into the red portal, out in the red bunker, "joins
RED"; back into the gallery; into the blue, "joins BLUE"; 1 server
correction, the test client's own floor portal. Seen in Xephyr: the field
through the panes, both bases through the portals. The battle runs with
only watchers in the world (bots and machines started on players in a team,
so a gallery with nobody fighting watched an empty field). Bots at a gun
face the way it points (AA launcher, nest, mortar; seated ones the way the
machine does) -- they were left facing wherever they last walked. Open: no
limit on how uneven the sides get now that players pick; a gunner's seat
does not turn round with the launcher.

**The front (2026-10-17), phases 1-2 of 6.** A war world (games/front, room
`front`, sandbox mouth 15, gate at 90.5 21 58.5): a 256x256 field in a ridge,
red base south and blue north, points A (farm), B (village) and C (bunker on
a hill), two stone walls across. Two teams with 150 tickets each: a death
costs one, holding more points bleeds the other side (one every 4 s, 2 s
with all three). Rifle and LMG fire while held, pistol once a pull, R
reloads, the base yard tops up ammo and grenades. Hits are tested in Lua
against the other team (friendly fire off), a head doubles; new
core.mob_hurt(id, damage, from_x, from_z) hurts one creature by id.
Grenades bounce for 3 s and carve a sphere of 2.6 (bedrock and ridge stay),
then up to 48 blocks left hanging over the hole fall (core.fall). Bots:
worker soldiers fill each team to 8, go point to point by waypoints of 24
nodes (the path search stops at 600 cells), shoot what they see within 48
and now and then throw a grenade. Headless, 200 s with bots only: 48 kills
(25 blue, 23 red), all three points changed hands. Seen in Xephyr: arriving
at the base, a burst, a crater in the yard's concrete. Models from
tools/make_front_models.py (arms with pistol, rifle, LMG, grenade; soldiers
with idle, walk, run, fire, throw, death); make_fps_models.fp_arms now takes
its guns and clips as arguments. Open: two players fighting each other not
yet played;
vehicles, turrets, mortars, artillery and aircraft are phases 3-6.

**The front's machines (2026-10-17), phases 3-5.** New core.craft (MODDING.md
"Mezzi"): a visible mod thing made a machine -- wheeled (four wheels driven),
tracked (two tracks, turns on the spot), rotor, wing -- in Jolt
(core/physics/craft.cpp, a Jolt world per machine like the kart's, server
only, 60 Hz steps), parts mounted on it (core.craft.mount/aim: a turret, a
barrel, a gun) and players seated on it (core.craft.seat; riders keep their
own look now, which core.player_look answers). Protocol 98: a thing's roll.
The front has jeeps (4 seats, rear MG), tanks (cannon on the driver, MG on
the turret, through walls), helicopters (rockets, door MG), planes (guns,
bombs), MG nests, mortars and howitzers that lob onto the crosshair, and an
infantry launcher. F gets in and out, 1-9 change seat, WASD space shift
drive, the mouse steers what flies. Measured headless: a heli climbs at 6
m/s and holds height, cruises at 20; a plane leaves the ground at 19 m/s; a
tank stopped by a 2-high wall carves a 5-wide gap and drives on over the
hills to the far base. Seen in Xephyr: a jeep driven out of the yard, a
tank's cannon cratering a hill, the seat change to its MG, a mortar's
red post on the crosshair, a helicopter turned to the look, flown at 59
km/h 13 up and its rockets bursting on the ridge. Sounds synthesised by
tools/make_front_sounds.py (guns, blasts, engines, rotor, soldiers' voices).
Fixed on the way: a mod that let somebody off a seat and placed them in
the same tick (core.ride 0, then core.player_place) had the seat's
correction sent after the placement, with the older epoch -- every claim
after was "from before it was put somewhere" and the body stood still on
the server; seats now go out before placements. Machines are solid
(2026-10-18): two that overlap are parted by mass at 1.5 m/s (two jeeps head
on at full throttle stop nose to nose, 4.0 apart, no way through), and a body
on foot in a hull is pushed out its nearest side with a launch's grace, as
fast as the machine comes at it plus 3 (put inside a parked jeep: out in 0.5
s; a jeep at 14 m/s shoves a standing player ahead; 2 corrections in 30 s,
both the test's placements). Fixed on the way: two machines made in one tick
aborted the server (step_machines added Roll while walking them). Run over by an
enemy machine at 4 m/s or more is the mod's (front_game M.run_over,
"roadkill", the driver credited). A body stands on a hull (protocol 100: a machine's hull
goes with its roll; the client keeps its body out of the hulls it draws and
carries it on the one it stands on, World::movers; the server grants a
roof's grace instead of a walk's limits): dropped on a parked jeep it stayed
on its roof at 21.61, rode it to 7 m/s and round a turn, and fell off over
a bump -- 1 correction in 27 s, the drop's. The driver's client runs its
machine too (core.craft.make/keys/pose on a client, front_game drive_here
repeating M.drive): drawn and sat on where it will be, what rides it moved
with it, held against the server's pose of a round trip ago -- a fifth of
the difference eased each pose, put back past 3 nodes or 0.6 rad. A scripted
drive (W, a turn, W, stop) through a relay at 137 ms round trip: the jeep
moves the frame W is pressed, 358 poses compared, 0.12 off on average, 0 put
back, both ends within 5 cm (1 ms link: 0.10). Open: the roof is flat (no
pitch or roll); the wing's throttle is the client's own guess; the plane not flown by hand.
Reported from play the same day and fixed: the mouse did nothing in a
machine (a seated client skipped player_update, which is where the mouse
turns the head: the view stayed put, a tank's turret and a plane did not
follow); a robot's legs showed through the jeep (seats were 0.3 low for
drive_idle, whose hips are 0.3 over its feet, and a player's own body did
not play core.player_clip at all -- it does now); a tank at a wall works
column by column: one high it climbs, two high it knocks the top off and
climbs, three or more it goes through (headless: a 2-high wall kept its
ground row and the tank went over it).
Then: a gun aims at what the crosshair is on, not along the eye's look
(the camera is behind the head): the client sends the point its camera's
ray through the crosshair meets (core.cursor_ray, now the screen's middle
while the game holds the mouse; it was the hidden cursor, which drifts)
and turrets, barrels, MGs, rockets and lobbing guns point from their own
pivot at it, the cannon raised for its shell's drop -- in Xephyr the shell
burst on the crosshair. The tank's commander stands in the turret's open
hatch and turns with it (core.craft.seat on the turret). Steps: the
machines' ground has a 45 degree ramp in front of every step one block
high with room over it, and the pull of the weight down a slope is taken
away while driven up it, so a tank climbs nose first and its tail follows
(headless: pitch 0.1 to 0.32 on the hills' steps, 0.18 over a wall's
remaining row) instead of hopping.
Then, reported from play: out of a destroyed tank (dead, back at the
base) grenades did not leave, shots hit nobody and no enemy aimed at the
player. The machine's end lets its riders off and kills them in one tick;
the death's placement went out before the getting off, and the client
took the older epoch, so the server refused every claim after and kept
the body where it was put back. The client now drops a placement older
than one it has heard (ClientSession::stale_placement), whatever order a
server sends them in; checked in Xephyr: one claim refused after the
death, then the body moves again. And machines are put down only once
the ground under them is in memory: at the far base they were made over
columns not loaded yet, fell through and were lost (22 of 22 now stand).
Wrecks die and go: each is the machine burnt and rigged by its pieces
with a 5 s "death" clip (tools/make_front_models.py) -- the tank's turret
thrown off to land beside the hull, the jeep over on its back and its
wheels away, the helicopter spun onto its side and its rotor off, the
plane nose in and over -- then sunk into the ground; a wreck made in the
air falls to the ground first, burns (flames and smoke on the client)
while it plays, and is taken away when it is over. Seen in Xephyr.
The jeep's wheels were bones named wheel_*, which makes a model a kart to
the client (has_wheels): it turned them by its own rule, on the wrong
axis, and sat the world's player model in the jeep, empty or not. They
are tire_* now, turned by the jeep's drive clip about their axles, with
lugs on the tread to see it; no machine model has a wheel_* bone. A
mod's thing with wheel_* bones is drawn as a kart with a rider: worth a
line in MODDING.md if another world trips on it.
A rider sits fast on the machine: Ride carries the seat in the host's
frame (protocol 99) and the client puts the body on the host as it draws
it, turned with it, so the two move as one; others see a rider facing
the way its seat does. In first person a seated body is seen from
its head with its arms alone drawn -- the rest's skinning gathered into a
point after the pose, nothing worn (Scene::seat_view, Body::arms_only) --
the look still the mouse's: at the jeep's wheel, the two hands on it.
Aiming at the front (2026-10-18): from the hip a round goes up to `spread`
off (rifle 0.045, LMG 0.07, sniper 0.09; the crosshair's ticks stand that
far out), and with the right button held `ads` off (rifle 0.005, sniper
0.0004), the view narrowed (core.zoom: rifle 0.75, sniper 0.2 through a
scope drawn on the HUD) and the body slowed to 1.8 m/s (core.body_movement;
measured 1.86 in Xephyr, 4.8 from the hip). The arms hold aim_<gun> and
shoot fire_<gun>_aim, captured with the right shoulder brought in so the
hand reaches the sight line. New bolt-action sniper rifle: 18 a round, 5 to
a magazine. Seen in Xephyr: the hip ticks, the rifle and LMG aimed, the
scope on the village.
Then from outside and reloading (2026-10-18): the robot and its species
have hold_, aim_, fire_ and reload_<gun> and fire_grenade, clips of the
arms alone (tools/make_front_poses.py: wrists put by IK, the right hand
turned as the gun is, baked to the arm bones only, merged with
gltf_add_clips.py). The engine lays a clip that keys nothing below the
arms over the body's own (arms_only_clip; pose() blends each layer past
the first only on the joints its clip moves), so legs walk under a gun
aimed. The server plays the state for everybody else (M.pose), the
client the same for its own body and first-person arms (one
core.player_clip; FP reload_<gun> from make_front_models.py). The sniper
aimed in third person goes to the eye for the scope and back: core.view
now returns the distance it was. Seen in Xephyr: the arms from behind at
the ready, aimed and reloading; the scope from third person and back; the
first-person reload.
On foot too a shot goes where the crosshair is (2026-10-18): in third
person the camera sits behind and over the head and a round along the
eye's look landed far off it. The client sends the point its camera's ray
through the crosshair meets, seated or not, and the server sends rounds,
rockets and grenades from the eye towards it (M.toward; the look when the
point is nearer than a node or behind the eye). Seen in Xephyr: a rocket
from third person bursting on the crosshair.
The crosshair's ray stops on bodies too (2026-10-18): the client sends the
camera's ray, not the point, and the server meets it with the blocks and
every player, bot and machine but the shooter and what they ride (once a
tick). Seen in Xephyr: aimed at a parked jeep, the ray stopped at 13.3
nodes on it, not on the wall at 14.5 behind.
Switching between first and third person keeps the crosshair on the same
thing (2026-10-18): the client meets the old camera's middle with the
blocks and the bodies, moves the camera, and turns the look until the new
camera's middle meets that point (set_view_distance; V, the menu's button
and core.view, so the scope too; not in a kart). Seen in Xephyr: third,
first and third again with the crosshair on the same edge of a far tank.
The machines' windows are see-through (2026-10-18): a model's material
whose base colour's alpha is under 1 is drawn after the rest of it,
blended, without writing depth (the texture's alpha still only cuts). The
jeep's windscreen, the helicopter's nose and the plane's canopy are a
"window" material at 0.3, and the jeep's screen frame is a frame now, not a
plate behind the glass. Seen in Xephyr: the jeep's seats through its
windscreen. Not sorted against another model's glass.
Glass panes are see-through (2026-10-18): a translucent block drawn as
boxes (building:glass_pane) went in the opaque boxes' draw and showed as a
grey sheet. Its boxes now go at the end of the same buffer and are drawn
in the translucent pass, after the chunks' glass and water (not sorted
against them). Seen in Xephyr: the blocks behind a wall of panes.
The front opens after the ending (2026-10-18): the laboratory emits
keys:ending when the kart is out, mods/keys keeps it in shared storage
(ending:<who>) and seals the sandbox's mouth onto the front to whoever has
not. FRONT_OPEN = true in mods/keys keeps it open to all while it is
tested -- set it false for a release. Checked headless with it false:
sealed, then open after keys:ending.
The front's bots are fairer and fight three ways (2026-10-18): a cone of
110 degrees (and 4 nodes all round), a reaction of 0.45-1.1 s, bursts that
start twice as wide and settle in 1.5 s, three quarters of a gun's damage;
hit, they turn to the shot. watch (stops to look round, sniper > rifle >
pistol), rush (runs at them, lmg > rifle > pistol), cover (cover first,
peeks for a burst, back; rifle > pistol); a gun kept till its rounds are
out. And they start to use the machines: drive the jeep and the tank to
their point (the jeep lets them off there, the tank stops to fire at what
it sees within 70), take the guns and seats of a machine their team
drives, a player's too. A bot in a seat is put there by the mod every tick
(the engine's seats are players'), standing. In a 3-minute match with the
client in base: 19 boardings, kills by cannon, lmg, sniper, grenade and
roadkill. Not yet: bots flying, at mortars, howitzers, nests or the
launcher; machines to sort out who drives where together.
Seats for players (2026-10-18): F takes the free seat nearest the body
(a jeep's back seats from behind it), and 1-9 change seat without also
changing the hand -- a key_1..9 press a world's code cancels no longer
selects the hotbar (Scene::digit_claimed, both loops). Bots leave a jeep or
a tank to a player of their team within 25 nodes. /mezzo <kind> puts a
machine in front of you for testing (none comes back when it is lost).
Seen in Xephyr: F from behind a jeep to its gun, then 1, 2, 4, hand kept.
Bots use everything (2026-10-18): soldiers hold the gun they use (hand_R
carries its _held model) and sit in seats; an anti-tank role starts with
the launcher; the watch takes nests, mortars and howitzers, which lob onto
what the team has seen in 3 s; bots fly the helicopter (18 over its point)
and the plane (circles the point at 45, 75 out). A bomber (two seats: a
stick of six bombs with the landing marked, a back turret) and two anti-air
launchers a base (lock 1.2 s within 0.1 rad, four homing missiles that hit
only what flies; bots take them when an enemy flies near). In 3-minute bot
matches: kills by mortar, howitzer, rocket, missile, cannon, MG; bombers
take off from z 36 and circle at 50-60; AA bots shot down planes, a heli
and both bombers. Open: a craft of mass 2500 barely moves on the ground
(800 is fine), not looked into -- the bomber is 800.
The jeep has a steering wheel under drive_idle's hands, turned by the
client as the jeep turns (a steering_wheel bone along its column), front
wheels that steer (steer_*), and its driver plays drive_left/right with
A and D. Seen in Xephyr from behind and from the seat.

**OPEN-58 -- the world anchor (2026-10-13).** New core.keep_loaded(x, z, r):
the columns over that square stay in memory 30 s with nobody near, made or
read back if they are not (server only; ServerState::kept feeds the anchors
the column keeper already had for stacks and creatures). New item
helpers:anchor (4 iron, 4 cinnabar, a diamond), given to a robot like a tool:
every 10 s it keeps 24 round itself and 16 round its base, and Light up waits
for ground out of memory instead of passing it over. Checked headless with no
player: Light up's ground within 30 of the base stayed 3719 of 3719 cells for
6 min (without the anchor it fell to 406 in 2 min and its torches "went").
Open: no cap on how many columns all the robots keep together.

**OPEN-57 -- light against the dark (2026-10-13).** F8 (mods/torches, client)
shows the floor cells a creature could come out on within 12: red dark now,
yellow dark at night; new core.block_light (the light blocks' light, no sky).
Torches already kept creatures off (spawn needs light 7 or less; a torch's 14
clears six steps). New helper order, Light up: torches from the base's chests
on dark ground up to 100 from the base, nearest first, on the (6,7)/(7,-6)
diamond lattice (85 cells a torch on flat ground), else where one lights the
most dark. Checked with a client near the plaza (seed 1): 58 torches in 10 min,
dark ground within 30 of the base 3077 -> 1236 of 3719, about 39 cells a torch,
half of them off the lattice (the plaza's steps and walls). Open: water beds
show in F8; only the surface of each column is lit, not caves; ground beyond
the loaded chunks is skipped.

**OPEN-56 -- the helper robot's quarry (2026-10-13).** A new order, Quarry
(mods/helpers, with a pick): a 5x5 pit east of the base, a layer at a time,
and round it a spiral stair, one step a layer, dug before its layer so the pit
is never deeper than the way up. The program panel got a Close button (before
it, only Pick up took it off the screen). Checked headless: from the surface
about a layer and an unload a minute; from a pit pre-dug to 20, layers 21-24
with every load of 24 carried up the stair to the chest (112 in it), then
stopped by the chest. Open: one turn of the stair, 24 deep (a second turn needs
its steps checked against the first one's headroom); the pit is always east;
ground over the base's level is left standing. A chest in another chunk unloads
with no player near and the robot waits full -- seen only in the test.

**OPEN-55 -- the robot's livery (2026-10-12).** Primary, secondary, accent (the
16 dyes or the model's own) and an extra on the head and at the neck, picked on
the title screen or the pause menu (Robot), with the robot turning in its
colours beside the swatches; sent in the hello and, changed mid-game, in a
Dress (protocol 93; 4 bytes on a Player that picked any). The scarf's ends and
the medal swing on sway_* bones the client poses (draw_worn). Checked in
Xephyr: own body, first-person arms, the preview on both pages, a change in
the pause menu seen by a second client. Open: 16 dyes, not free colours (5 bits
a slot on the wire).

**OPEN-54 -- lighting rework (branch `lighting-rework`, 2026-10-11).** True dark
(2% floor), smooth per-corner light, lamps lighting the faces that look at them
(from the flood fill's slope), sun and moon with a 2048 shadow map, Options →
Shadows; the old light kept as Options → Lighting: classic (`--classic-light`,
the packed light byte still rides in the vertex). Software GL (Xephyr), `--quick --soak 30`, main vs branch: frame p50
89-93 → 143-144 ms, render 56 → 97 ms, sync (meshing) 6.5-6.9 → 11.9-12.5 ms;
mesh memory 31 → 36 MB. Open:
- Not measured on a real GPU yet; the render cost is three shadow passes a
  frame here (eye + two openings).
- Meshing about twice as slow (four cells a corner, light slope per cell).
- Bodies, models and boxes neither cast nor receive the sun's shadow; the model
  shader still lights from a fixed direction.
- One map, no cascades: shadow edges near the eye are 5.5 cm texels, soft.
- No coloured light: every lamp is the same warm white.
- `relight_around` may give false sunlight in caves taller than 16 (light.cpp
  sun seed at the box top); not checked.

**OPEN-35 -- the world made as players go.** A stage changed between runs
shows a seam; `core.get_block` on a column not in memory reads air (use
`core.loaded`); a listening server spends 20 ms a tick on columns; the
welcome's hash covers the region only. A furnace smelts only while its column
is in memory.

**OPEN-38 -- letting go of the world.** Without a world file changed columns
stay in memory for the life of the process.

**OPEN-43 -- block shapes.** No separate selection box; the client does not
refuse a dig it could predict. One material per block and per tool (CLOSED-31).

**Villagers** (design in memory; new behaviour starts with questions).
- `mods/villagers` is at 194 of Luau's 200 locals in its main chunk: new state
  goes in a table, or the mod does not load.
- Villages planned before the plan keys were kept get today's plan.

**Creatures.** Every body is clamped to 0.3 half-width and 1.8 tall to the
hit test (a wraith); creatures do not hang; one chasing burns rather than
hides; players and creatures do not push each other; a creature in a column
not in memory waits.

**Smaller gaps.**
- Furnace: an old furnace keeps what is in a wrong row; no heat gauge; two
  crafting recipes share a shape (warning at startup, not looked into).
- Conveyor: ribs are made for 1 m/s; a belt a mod writes reshapes late.
- Sounds: attack and shoot of the creatures not heard; the poolrooms are
  quiet; the yafps base has no sounds.
- Signs are read on the HUD, not on the board; no minecart; a plant cell is a
  draw call; the Kenney pack lacks the new blocks.
- Bow: no aiming pose in third person.
- Armour: no pauldron in first person; the wear strip not seen on screen.
- World (2026-10-10): climates ~200 blocks across (periods 256/208 + a quarter
  octave), continental field (fbm 640) for ocean (floor 6-15) and sea
  shelf, rivers (fbm 360, bed at 15-17, banks over ~12 blocks, not under
  hills or near the plaza), beaches at 18-19; seed 1 over 4096: ocean 21%,
  sea 12%, plains 23%, peaks 14%, tundra 14%, desert 7%, river 5%, beach
  3%, lake 1.4%. Bridges (arched planks, fences, log piers) where village
  paths cross rivers and lakes; paths no longer cross sea or ocean.
  mods/depths: 49 mines and crypts in 13x13 regions of seed 1. Village
  ground covers the whole plan (villages:extent). Seen in Xephyr: sea,
  river and pond with beaches, an arched bridge, a crypt and a mine; not
  walked through by hand. Saved worlds keep their old terrain; new columns
  next to old ones meet them at a seam.
  Lakes up high: a site in one 176-cell in two, kept where the ground on two
  rings round it is 6+ over the sea and no sea, river or plaza is near;
  water one under the rings' lowest, a bowl 2-5 deep, a 14-block slope cut
  round it (worst step 2-5). Seed 1: 23 in 4096x4096. core.water_level
  for the mods; lakes, villages (bridges), rocks (ice), plants (lilies),
  depths follow it. Seen in Xephyr: one at 924 33 -228 in a stone bowl.
  Gorges: a field (fbm 420) crossing its middle, where a slow one (720) is
  high; cut to 16 over 3-4 blocks, walls over 4-5; 1.1% of seed 1, the
  deepest 34 (1197 -1464). Paths cross at the rims' height; any path cell 4+
  over the ground is a bridge. Seen in Xephyr: seed 3's crossing at 768
  -474 (deck at 23 over water at 17, fences both sides) and seed 1's deep one.
- Villagers through shut doors (2026-10-10): walk()'s unsticking put a
  body up to four cells on along its way, doors included; now no further
  than the cell before the first door, which it opens if it is the next
  cell (the engine shuts it behind). Checked headless (both halves opened,
  turn kept); not watched in play.
- Woodcutter felled a mine's headframe log for a sapling, builder stuck in
  the mine (2026-10-10): tree_log now refuses timber:log (only ever built),
  wants the world's ground under the trunk, nothing built against it, no
  roof within 4; the sapling goes only where the trunk came down, on earth;
  only miners use the galleries, a builder fetching stone quarries outside.
  Checked headless (tree yes; mine post, log by a door, roofed log no); not
  watched in play.
- A builder jumping in a corner of the mine's hut (2026-10-10): sent at the
  mine chest's own cell. walk() now goes beside a block it is sent into,
  the builder reaches chests; and the engine stops a worker whose goal has
  no way to it (PathResult.reached false, under 48 off) at once --
  core.mob_going 0 -- instead of walking as near as it can and on straight
  at it. Headless: a sealed cell given up in one tick, an open one 10 off
  reached in 4.9 s.
- Miners never in the mine (2026-10-10): find_mine's one core.find_nodes
  over the village's box +30 was over the million-cell cap in any village
  of size -- refused, mine never found, miners quarried outside. Searches
  now go in 64x64 tiles (AID.find; the keepers' beds and the village's
  chests had the same risk). Seed 1's village at 85 -170: mine found at
  once; with its humans made miners in a scratch copy, both went down to
  y 6, dug galleries and brought stone up, four minutes running. That
  village has no gatherer of its own: a home is one only one time in three
  or four, so a small village may have none (kept random, 2026-10-10).
- Square patches gone (2026-10-10): rocks' red sand (16-squares) and
  coarse dirt / mycelium on the plains (6-squares) by a smooth value noise
  with a finer octave; woods' kinds by 48-squares bent by up to 20 either
  way. Seen in Xephyr: the desert at 28 280, red sand in curved bands.
- Chess: a game of two left half way waits for ever; no table in the
  single-process client. Variants (960, king of the hill, three-check, racing
  kings, losing chess, atomic, crazyhouse, horde): rules checked headless
  (300 shuffles, castling both ways in 960, each variant's win, atomic's
  blast and touching kings, crazyhouse's hands and promoted pieces going
  back as pawns, old saved boards load as classical); in Xephyr a queen put
  down from the crazyhouse hand onto e4, and an atomic Nxd7 seen going off
  (fire, smoke, the pieces thrown off the table; blast.ogg from ElevenLabs,
  blast/smoke.gltf from tools/make_chess_models.py). Not yet played through
  by hand; the sound not heard (Xephyr has no audio checked).
- Cards: the illustrated deck has 30 of 45 pictures to draw
  (`tools/elevenlabs_deck.json`).
- Cards held in hand (2026-10-12): seated, the body goes to its seat and plays
  the robot's `cards` clip for everybody (core.player_clip, protocol 92); the
  view is the eye with the cursor free (core.fixed_view under 0.1 draws the
  arms), the hand a fan before it (core.render.model's new pitch and roll),
  V to the old view from above. Seen in Xephyr with two clients at a tresette:
  each sees the other's fan as backs, a card clicked in the fan is played.
  The corner values are smaller and tight in the corner (tools/make_cards.py,
  24 instead of 34) and the fan wider (0.11 rad a card at ten, 0.19 at
  three): all ten values of a tresette hand read in the fan.
  Open: a seat behind a wall leaves the body where it stood, the
  fan drawn at the seat; the fists hide the fan's bottom edge; the table's
  own seats keep their cards on the felt.
- Cards played (2026-10-17): a card fades nowhere -- it flies from the hand to
  the table, and a trick or a scopa capture flies one card after another
  (0.6 s each) to the taker's pile, turning onto its back. The cursor on a
  card played (trick, scopa's table, the trump) raises it, bigger and tipped
  to the eye, its name and who played it on the screen. Cards drawn (and
  dealt, scopa's table too) fly from the stock round the table a card at a
  time after the taken ones reach their pile, one's own turning face up.
  The card under the cursor is the one nearest round the fan's grip (or
  along the row from above), so each card has an equal share and covered
  ones are reached: all ten of a tresette hand, both views. Nothing is
  picked or played while a card is moving, or for 0.15 s after.
  Seen in Xephyr at a
  briscola and a scopa for two against the table.
- Fireballs do not burn or break; inferno gold, tears and rods have no use;
  zombie flesh and bones have no use.
- Water: only the top is lit smoothly; the selection outline under water is
  not faded; the walk's cost on a weak GPU was not measured.
- Clouds: the wind is not saved.

**OPEN-37 -- single-pixel pinholes** at one exact pose. Only if reported.

**OPEN-51 -- closed 2026-10-09.** Other bodies' held blocks are cached one
cube per block and model (`Scene::held_bodies`). Built and three clients run;
two players holding different blocks were not set up in view of a third.

## Saving

**CLOSED-10 leftovers.**
- A crash loses up to one save interval: 10 s by default since 2026-10-09
  (was 30; a save on a played world is 2-13 ms). Killed with -9 after 75 s
  of play: 162 chunks, the player and 185 containers read back.
- Stacks on the ground are not saved.

**A saved world from before block names were kept** is refused; a placeholder's
id can move between starts.

**Identity and menu.** First key seen is trusted (ssh-style) and nothing shows
it; a password is the key, recovered only by a server's owner
(`--free-name`); the join resolves a name on the frame thread; the hosted
address is the first non-loopback IPv4; a deleted world has no bin; fields
have no cursor; the menu is in English.

## Server administration

**Done 2026-10-27** (`src/server/admin.h`, `src/core/ui/page.h`):
- Phase 1: roles, /kick /ban /banip /mute /whitelist, an audit table,
  `--owner`, the stdin console, `core.command(..., perm)`, `core.has_perm`,
  `core.role`.
- Phase 2: `--config FILE` and RCON (`--rcon`, `tools/admin.sh`).
- In-game pages: `/page`, `/admin`, built-in pages for administration, the
  server, the mods and the ground underfoot; `core.page_register` and
  `core.page_show` for mods (`mods/board`).
- `/perm` changes which role each permission wants.
- An anonymous player's ban takes their address too.
- The single-process client's player is owner.
- `/msg` and `/me` respect /mute (`core.is_muted`).
- /mute holds any mod's chat: while a muted player's command runs, its mod's
  `core.chat` to anybody else is held.
- The address each player last came from is kept (`addr` rows), so an offline
  player without a key is banned with it, and `/banip NAME` works offline.
- Page text is measured with the glyph widths of the client's font (raylib's
  default, copied into `src/core/ui/page.cpp`): a line is cut at the page's
  edge to the pixel.
- The single-process client has `/page`: the mods' pages and `mods`, no
  administration.

Checked:
- `two_process_join` bans a name from the console, refuses it, lets another
  name in and the first back after /unban, and RCON answers only to its
  password.
- In Xephyr: /admin, the permission buttons (the page reloads with the change),
  the board's field and actions, back/next over 4 screens of mods.
- `two_process_join`: an offline name without a key is banned with its address.
- In Xephyr, `--quick`: /page, the board (post, take down), mods 1/4 to 2/4,
  ESC; a wide note cut exactly at the edge. Joined and muted from the console,
  /me through a whisper mod without its own check is held.

Found on the way: a mapgen stage from a mod that failed halfway through loading
stayed listed with its dead VM's address, and ran in the next VM malloc put
there. `worldgen` failed 1/40 on HEAD and 15/40 with this work under 4-way
load; stages now go dead with their VM, and it passes 60/60.

**Done 2026-10-27, phases 3-6 and extras:**
- Restarts: `/restart`, `/stop` with warnings, `--restart-at`, exit code 75
  under `tools/serve.sh` (restarts, crash count, a log a day with times),
  `--watchdog`. A player put out as Restarting is joined again by the client's
  title once the server answers (not in the first 5 s, given up after 3 min).
- `/maintenance`, `/motd` (`--motd`), `/announce`; the refusals Restarting,
  Stopping, Maintenance (Restarting retried, the others permanent).
- Visits (`sessions` table): `/seen`, `/players`, `/whois`, `/logs`,
  `/metrics`; `cnp_server --report FILE [--since 7d]`; refusals at the door
  and server start/stop/unclean in `admin_log`.
- Pages `running`, `players`, `reports`, `log`.
- Status query (`StatusQuery`/`Status`, before any hello): the client's server
  list shows players or "maintenance". A server from before answers as up.
- `/portals`: links out, and crossings in by issuer (`inbound` rows). A portal
  window checks the far server's signing key against the link's
  (`ClientConfig::expect_signer`).
- Federation (`--federation`, `/friend`, `/fed`, `/where`): signed TCP lines,
  bans by key, presence, chat. DESIGN.md 4.3 updated.
- Extras: `/warn` (3 in 30 days = 1 day ban), `/report` `/reports` `/note`,
  `/sc`, chat flood limit (5 lines in 5 s), `--max-per-address`, `/backup`
  and `--backup-every` (SQLite online backup on a thread).
- Mute also holds a mod's chat during a muted player's page action or click.
- A translation wider than a page's button is drawn untranslated.

Checked: headless script (maintenance refuses, 3 warns ban, report, note,
whois, logs, metrics, backup file written, restart under serve.sh and stop,
daily log, --report); two servers federated (ban on A refused on B, /unban on
B sticks, `/fed` chat, `/where` across); in Xephyr the server list shows
"0 playing", MOTD in chat on join, /admin and its new pages, restart from the
page with countdown, client back in 5 s after the restart. ctest 13/13.

Found on the way: the federation's condition variable destroyed at exit hung
the server on its way out (glibc waits for waiters): kept alive for good.
`peek_type` did not know the new message types until listed (its switch has
no default on purpose, and the warning went unseen).

**Done 2026-10-27, the rest:**
- One bag across friend servers (share `bags`, on by default): the server a
  player arrives at asks the friend whose ticket brought them, or the one it
  gave the bag to last (`bagaway` rows), or, for a player who walked in,
  every friend whose copy is newest; that one lets go of it. Items a world
  does not know are kept aside (`hidden` rows) and travel with the bag.
- Federation lines sealed for the receiver (`net::seal_for`: X25519 from the
  two signing keys, XChaCha20-Poly1305), `CNPFED2`.
- A redirect crossing checks the far key the link names, like a window.
- `/inspect X Y Z`, `/rollback NAME 2h` (an `edits` table, a month kept,
  written every 5 s); `cnp_join_probe --dig N`.
- `--webhook URL` (admin log, reports, start, stop, unclean start) and
  `tools/serve.sh -w URL` (crashes).
- `/role NAME ROLE ROOM`: a role in one room.
- `/bag NAME`, `/give NAME ITEM [N]`.
- docs/ADMIN.md: every command, permission, flag, conf key and script.

Checked: two servers with different mods, a player walking between them
four times: `t1:shell` (unknown on B) hidden on B and back on A, gems given
on B arrive on A; a player online on A joining B takes the bag from A's
session; webhook posts received by a local listener (start, friend, give,
ban, stop); per-room role in a 2-room server; a probe digs 3 cells, /inspect
names it, /rollback puts them back. ctest 13/13.

**Done 2026-10-27, the open limits:**
- A bag later than the 5 s wait is no longer dropped: it replaces the copy
  the player was given, plus what they picked up meanwhile.
- "Newest" between friends is the bag that moved most (`baggen` rows), not
  a clock; a clock only between two copies that never moved.
- Backpack contents travel with the bag, renumbered from 100000 on the
  server they reach; a hidden backpack keeps its contents aside with it;
  hidden things come back into the square they had.
- Federation lines: the receiver sends a throwaway key first (`CNPFED3`),
  mixed into the sealing key: forward secrecy.
- A translated page label or button wider than its room is drawn smaller
  (down to 2/3), past that in English.
- Rules to accept (`--rules-file`, `/rules`, `/accept`, `/leave`, page
  `rules`): no chat, commands or edits before.
- `/alts NAME`; the staff told when a player comes in from a banned
  player's address.
- The server asks GitHub for a newer release (`update_check` moved to
  core), says so in the log and to admins; `--no-update-check`.
- `--report FILE --json`.
- Fixed on the way: after a first redirect, the client never sent the
  second crossing's enter (`fallback_asked` was never reset), so a
  `--no-passive` run could not come back; the gate now counts one redirect
  per crossing.

Checked: a scripted client in Xephyr crossing A->B->A through real
portals, with windows and with `--no-passive` (redirect): the bag goes
with it both ways, B hides `t1:shell` and A gets it back. A friend's bag
held back 8 s (`CNP_TEST_FED_DELAY_MS`): the late bag replaced the old copy
and kept 2 shells given meanwhile. A backpack #7 from A arrives on B as
#100000 beside B's own #7, and back on A; with B not knowing backpacks it
is hidden there and returns onto the back with its 6 gems. Rules: edits and
chat refused before /accept, the page in Xephyr in Italian ("Accetta"),
accepted by a click. /alts and the join warning. ctest 13/13.

**Done 2026-10-27, mute and mod chat:** the words a muted player hands a
mod (command, page field, click, mod message) are noted; a mod's chat,
notice or page line carrying three of them in a row (12+ letters) reaches
only them, then or later (`g_muted_words`, an hour after the words or while
muted). Fixed: a mute given to a name nobody owned yet was never applied
once that name came with a key (`muted` looked at the key only).
Checked with a letter-box scratch mod: bob muted, his letter delivered a
tick later held in chat, notice and on the board for eve and carl; carl's
own letters, one sharing two of bob's words, pass. ctest 13/13.

Open:
- /mute knows words, not meaning: a mod that rewords them gets through.
- A friend unreachable while the player plays on keeps its own copy too.
- The client's far-key check is behind the ticket check, which refuses
  first; a server lying about tickets is the only case it catches.

## Mods and scripting

**OPEN-42 -- modifiers (ABM/LBM).** One Lua call per cell that fires, no
batching.

**OPEN-20 -- visible mod entities** collide with nothing; the hit test assumes
a box.

**OPEN-49 -- code a world sends to clients.** A client-only mod cannot talk to
the server; world code is loaded once per session (new media need a rejoin); a
world without a signing key can only be refused by the global flag.

**Chat.** Per world, not per server; no permissions (`/tp`, `/give`); no Tab
completion or cursor.

**OPEN-53 -- the Inferno shares the bag and the player's storage.** Since
2026-10-11 the Inferno loads the sandbox's gameplay mods (50 running, 348 items,
11 sandbox stages dropped by `core.mapgen_only`); not gates, guard, villages,
villagers, depths, spike, beacons, console, palette -- their items cross as
orphans. Hunger, experience, keys seen and the backpacks' counter are kept
once for every room (`core.shared_storage_*`, in the first room's file); a
backpack's contents move to the room its wearer enters. Not run with a player
yet: a pack crossing with something in it. A crash between the two rooms'
saves may leave a pack's contents in both files, and the copy in the room
entered next wins. Beds, chests and tables stay per room, as places.

**Characters kept by name** (arpg, track boards): a name is a label where the
server does not require identity.

**Backpack and dyes.** A mod reload does not undo `core.recipe_dye`; the games
in `games/` do not load mods/jetpack.

## Distribution (M10, open since 2026-10-09)

**OPEN-40 -- macOS never run; Windows only under wine.** Packages, 2026-10-10:
tarball and AppImage (23 MB; until now they lacked `assets/sounds` and
`assets/models` and read them from the checkout), Flatpak bundle (17 MB, the
portable build on the 25.08 runtime), Windows zip (25 MB, zig cross-compile;
`core/os.cpp` holds the lock, child server and stop that were POSIX). Seen:
Flatpak title -> new world -> 9-room server -> plaza drawn -> close saved;
Windows under wine: title drawn, `--connect` joined, world hash equal to
Linux (056395260f78d090), child server stopped by its event and saved. Open:
- macOS: `package-macos.sh` + CI job written, never run (no Mac here). Server
  and modpack link for arm64 with zig; the client needs Apple's SDK.
- Windows: no stack dump or watchdog; a crashed client kills its server
  unsaved (job object); back-to-title asks the password again; clicks under
  wine+Xephyr never arrived, so the menu path is untried on real Windows.
- Unsigned everywhere (SmartScreen, Gatekeeper). App id
  `io.github.giacomoperin.CubesAndPortals` is a placeholder.

**OPEN-39 -- a mod package has no source.** `.cnpmod` is checked against its own
hash: no index, no signatures, no trust decision.

**OPEN-52 -- no Android or browser build (lowest priority).** Desktop GL 3.3
only; portals clip with `gl_ClipDistance`. First step either way: DESIGN.md 2.4
"strada B" (clip by `discard`, `#version 300 es`), tryable on the desktop.
