# Add-ons

*[Leggi in italiano](README.it.md)*

What is here is **not part of the game**: no world loads it unless a player installs it. Each folder
is a mod made the way somebody outside the project would send one, kept in the repository to try
the way contributions come in -- installed into a player's mods folder, loaded beside the game's
own, and played -- before anything is taken into `mods/`.

| Add-on | What it is |
|---|---|
| [`lizard`](lizard) | The lizard man, a species for the player: made with Meshy, cubes on the robot's skeleton, a tail that wags, three styles and the robot's accessories fitted to it |
| [`fox`](fox) | The fox man, a species for the player: made with Meshy, flat-coloured facets on the robot's skeleton, a bushy tail that wags, two styles (eye patch, quiver) and the robot's extras made to measure |

## Trying one

Put it in the player's mods folder, `~/.cubes-and-portals/mods` (Windows:
`%APPDATA%/cubes-and-portals/mods`), as a player would:

- with the launcher: drop the add-on's folder on its window, then "Add to" (it is copied); or
- while working on it, a link, so a change is seen at once:

  ```sh
  ln -s "$PWD/contrib/lizard" ~/.cubes-and-portals/mods/lizard
  ```

Then start a world from the title screen, or `tools/track.sh`: both give the player's mods to the
sandbox and the pocket halls, and an add-on whose `mod.conf` says `rooms = all` to every room of the
server. A species shows on the look page (Character) after the game's own. To take it away, remove
the link or the copy ("Remove" on the launcher's mods page), or turn it off for one world from its
mods page.

## What an add-on is

A mod like any other ([docs/MODDING.md](../docs/MODDING.md)): a folder with `mod.conf` and
`init.luau`, its files beside them, nothing outside its folder. A species for the player is a body
`robot:robot_<name>` and its first-person arms `robot:robot_<name>_arms`, with `rooms = all` so it is
drawn in every world; its styles, the piece it always wears and its own fit of the accessories
follow the same names ([docs/MODDING.md](../docs/MODDING.md), "The player's colours and accessories").

What made it -- a Blender script, a Meshy source -- may live in its folder too. A large source is
kept out of the repository (`.gitignore`): `lizard/meshy/lizard.glb` is 16.6 MB. The fox's is small
(`fox/meshy/`: the picture it was made from, 1 MB, and Meshy's model, 2.7 MB) and stays.
