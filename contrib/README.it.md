# Add-on

*[Read in English](README.md)*

Quello che c'è qui **non fa parte del gioco**: nessun mondo lo carica se un giocatore non lo installa.
Ogni cartella è una mod fatta come la manderebbe qualcuno da fuori del progetto, tenuta nel
repository per provare la strada dei contributi -- installata nella cartella delle mod del giocatore,
caricata accanto a quelle del gioco, e giocata -- prima che qualcosa passi in `mods/`.

| Add-on | Cos'è |
|---|---|
| [`lizard`](lizard) | L'uomo lucertola, una specie per il giocatore: fatto con Meshy, cubi sullo scheletro del robot, una coda che scodinzola, tre stili e gli accessori del robot fatti su misura |

## Provarne uno

Va messo nella cartella delle mod del giocatore, `~/.cubes-and-portals/mods` (Windows:
`%APPDATA%/cubes-and-portals/mods`), come farebbe un giocatore:

- col launcher: si trascina la cartella dell'add-on sulla sua finestra, poi "Aggiungi a" (viene copiata); oppure
- mentre ci si lavora, un collegamento, così una modifica si vede subito:

  ```sh
  ln -s "$PWD/contrib/lizard" ~/.cubes-and-portals/mods/lizard
  ```

Poi si avvia un mondo dal menù del titolo, o `tools/track.sh`: tutti e due danno le mod del giocatore
al sandbox e alle sale tascabili, e un add-on il cui `mod.conf` dice `rooms = all` a tutte le stanze
del server. Una specie compare nella pagina dell'aspetto (Personaggio) dopo quelle del gioco. Per
toglierla si cancella il collegamento o la copia ("Remove" nella pagina delle mod del launcher), o la
si spegne per un mondo dalla sua pagina delle mod.

## Cos'è un add-on

Una mod come le altre ([docs/MODDING.it.md](../docs/MODDING.it.md)): una cartella con `mod.conf` e
`init.luau`, i suoi file accanto, niente fuori dalla sua cartella. Una specie per il giocatore è un
corpo `robot:robot_<nome>` e le sue braccia in prima persona `robot:robot_<nome>_arms`, con
`rooms = all` perché sia disegnata in ogni mondo; i suoi stili, il pezzo che porta sempre e la sua
versione degli accessori seguono gli stessi nomi ([docs/MODDING.it.md](../docs/MODDING.it.md),
"Colori e accessori del giocatore").

Quello che l'ha fatta -- uno script di Blender, un sorgente Meshy -- può stare nella sua cartella. Un
sorgente grande resta fuori dal repository (`.gitignore`): `lizard/meshy/lizard.glb` pesa 16,6 MB.
