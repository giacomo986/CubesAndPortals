# Il percorso di parkour

*[Read in English](README.md)*

Un percorso sospeso nel cielo, dieci stazioni in fila lungo -x, ognuna una
mossa del parkour del motore (`core.parkour`, `core.rope`). Due mod:
`mods/parkour_core` (solo server: il robot e come si muove) e `mods/parkour`
(`side = both`: il percorso sul server, la rope gun sul client).

## Come ci si arriva

`tools/track.sh` (o il menù del titolo) avvia il sandbox con otto stanze; il
settimo cancello della piazza, un'impalcatura a strisce arancioni e nere con
una scala di blocchi grigi e una corda appesa, sta a 16.5 21 52.5
(`mods/gates`). Si esce sulla piazza del percorso, davanti alla porta di
ritorno; un'altra porta di ritorno è in cima alla torre. All'arrivo si riceve la rope
gun (una sola, quante se ne avessero).

## Le stazioni

Ogni stazione comincia con un pad arancione: chi ci sta sopra legge cosa fare,
e chi cade nel vuoto (sotto y 12) torna sull'ultimo pad toccato.

| | dove (x) | cosa | come |
|---|---|---|---|
| 1 | -22 .. -15 | fossa larga 8, muro di mattoni a destra | rincorsa, salto accanto al muro, avanti tenuto: 5 nodi sul muro (un salto di corsa ne fa 5) |
| 2 | -35 .. -32 | muro con un buco alto 1 | corsa e crouch: scivolata |
| 3 | -50 .. -41 | gradino alto 2 | spalle al gradino, indietro + salto: capriola |
| 4 | -60 .. -51 | due muri a 2, alti 10 e 7 | salto contro un muro e di nuovo salto: rimbalzi fino in cima al muro basso, a 30 |
| 5 | -88 .. -71 | fossa larga 18 sotto una trave d'acciaio a 42 | rope gun (destro) sulla trave, avanti per oscillare, salto per staccarsi sull'altra sponda |
| 6 | -140 .. -103 | fossa larga 37 sotto due travi a 42, a 19 l'una dall'altra | aggancia la prima, staccati in alto nell'oscillazione, aggancia la seconda prima di cadere, staccati sopra l'approdo |
| 7 | -163 .. -153 | muro di mattoni a destra e uno di traverso in fondo, sopra una fossa | corsa sul muro di destra; dove incontra quello di traverso, salta: la corsa gira su quello (un angolo interno) e atterra dopo l'angolo, la strada prosegue 5 più a +z |
| 8 | -217 .. -175 | due travi ad angolo: la prima di traverso, la seconda lungo la strada, a sinistra | dalla prima guarda la seconda; la sua oscillazione gira la linea a +z sull'approdo, la strada prosegue 26 più a +z |
| 9 | -282 .. -218 | tre travi a S: di traverso, lungo la strada a sinistra, di nuovo di traverso | guarda ogni volta la trave dopo; approdo 21 più a +z |
| 10 | -310 .. -275 | tre travi a U: di traverso, lungo la strada a sinistra, di traverso a destra | guarda ogni volta la trave dopo; l'ultima ti riporta verso +x sull'approdo, e la strada riparte a ovest da lì, sotto le travi, 25 più a +z |

Stazione 6 provata con la fisica del motore, cercando fra i tempi (corda dopo il salto, tenuta,
stacco, seconda corda, tenuta): 6011 combinazioni su 22218 arrivano, tutte usando entrambe le
travi. Stazione 7: saltando dal bordo o due nodi prima, la corsa incontra l'angolo entro i suoi
cinque nodi, gira al salto e atterra a z +6; senza salto si cade. Stazioni 8 e 9 provate allo
stesso modo, con il corpo che guarda la trave dopo (i tasti spingono l'oscillazione dove si
guarda): arrivano 1392 combinazioni su 8464 sulla 8, 337 su 4096 sulla 9 e 358 su 4096 sulla 10,
nessuna saltando una trave.

## La torre e la Chiave della Vetta

Dalla strada dopo la U (pad 11) comincia
la torre: le cinque mosse di nuovo, una dopo l'altra e in salita, con le stesse
celle delle stazioni sopra (quindi gli stessi salti). Ogni stazione ha il suo
pad, e chi cade torna sull'ultimo toccato.

| | stazione | pavimento |
|---|---|---|
| 1 | camino | 29 → 36 |
| 2 | corsa sul muro | 36 |
| 3 | capriola sul gradino | 36 → 38 |
| 4 | camino | 38 → 45 |
| 5 | scivolata | 45 |
| 6 | corda, trave a 58 | 45 |
| 7 | camino | 45 → 52 |

In cima (x -452 .. -442, z 79) il pad della cima dà la **Chiave della Vetta**
(`keys:parkour`, mods/keys) a chi non la porta già, e la porta di ritorno al
sandbox è lì (il secondo portale del mondo, spostato dall'approdo della corda).

## Il percorso di un sito survival

Dietro un sito survival (`keys:level` di mods/keys) il percorso è del sito, nel suo slot: le
stazioni qui sopra come pezzi, più un salto in piano, un salto di un nodo in su, tre pilastri, un
pad di rimbalzo e lastre che cedono, mescolati dal seme del sito, 3 + d, con varchi misurati sul
salto del corpo (il 60% a d 1, l'85% a d 10). Il pad della cima dice che il livello è fatto, e una
porta oltre riporta all'inizio, dove l'uscita nascosta si è aperta. Le due travi, l'angolo e
le travi ad angolo, a S e a U entrano da d 7, con tutti i pezzi.

## La rope gun

Tasto destro: spara la corda dove si guarda (fino a 40 nodi) e ci si appende.
Avanti e indietro spingono l'oscillazione, crouch dà corda, sprint la
riprende, salto o tasto sinistro si stacca. La corda è disegnata come una fila
di cubetti arancioni dalle mani al punto d'aggancio.

## Come è fatto

- Il percorso è una lista di scatole di celle (`box`), scritta da uno stage di
  mapgen (`parkour:course`) colonna per colonna, tagliata ai bordi di ognuna;
  niente terreno del motore (`core.mapgen_core(false)`).
- Texture: `tools/make_parkour_textures.py`. La rope gun in mano e la sua icona:
  `tools/make_rope_gun.py` (Blender, assets/blender/rope_gun.blend).
- Le clip del robot (corsa sul muro a destra e a sinistra, salto dal muro,
  scivolata, capriola, oscillazione sulla corda) sono in
  assets/blender/robot18.blend e aggiunte a `mods/robot/robot.gltf` con
  `tools/gltf_add_clips.py`.
