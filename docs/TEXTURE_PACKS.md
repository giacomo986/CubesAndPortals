# Pacchetti di texture

Un pacchetto è **una cartella di immagini** e, se serve, un foglietto che dice
quale immagine porta ogni blocco.

```sh
./build/src/client/cnp_client --textures assets/packs/kenney
```

Più di uno alla volta, e **F7** passa al successivo mentre il gioco gira:

```sh
./build/src/client/cnp_client --textures assets/textures --textures assets/packs/kenney
```

`--no-textures` non ne seleziona nessuno: ogni blocco resta il segnaposto che
l'engine cuoce da `colour`/`checker`/`opacity`. È lo stato contro cui il gate
misura tutto il resto.

---

## Senza foglietto

Una cartella senza `pack.txt` è un pacchetto lo stesso: i suoi file si chiamano
come i blocchi, con i due punti sostituiti da un underscore.

    core:stone   ->  core_stone.png
    beacons:beacon -> beacons_beacon.png

È come è fatta `assets/textures`, che è quella che il repo spedisce, e non ha
mai avuto bisogno di un foglietto. **Una tabella di alias vuota deve comportarsi
come la vecchia regola, non come un errore.**

## Una faccia alla volta

Un blocco può portare **una** immagine, oppure una sopra / una sotto / una di
lato, oppure sei diverse. A deciderlo è **quali file esistono**, non una
dichiarazione:

| file | chi lo porta |
|---|---|
| `core_stone.png` | tutte e sei le facce |
| `core_grass_top.png` | la faccia superiore (+Y) |
| `core_grass_bottom.png` | quella inferiore (−Y) |
| `core_grass_side.png` | le quattro verticali |
| `core_forno_front.png` | +Z |
| `core_forno_back.png` | −Z |
| `core_forno_right.png` | +X |
| `core_forno_left.png` | −X |

Ogni faccia cerca **il proprio file, poi quello del suo gruppo, poi quello
semplice del blocco**. Quindi:

- un pacchetto che non aggiunge niente si comporta esattamente come prima;
- un pacchetto che aggiunge `core_grass_top.png` divide il blocco in due senza
  che nessuno abbia dichiarato che l'erba ha una cima;
- una faccia di cui manca sia il file proprio sia quello del gruppo finisce sul
  file semplice, non sul segnaposto.

La dichiarazione sarebbe un secondo posto dove vive la verità, e i due sarebbero
in disaccordo la prima volta che un pacchetto spedisce un file di cui la tabella
dei nodi non sa niente.

**Un tile per immagine distinta, non per blocco.** Due blocchi che chiedono lo
stesso file condividono un tile; un blocco con cima, base e lati ne costa tre.
Il tetto è **256**, perché l'indice del tile sta in otto bit nel vertice: le
immagini che non ci stanno sono contate e dette, mai avvolte in silenzio.

Nel foglietto la faccia è la terza parte della chiave:

```ini
core:grass:top    = grass_top.png
core:grass:side   = dirt_grass.png
core:grass:bottom = dirt.png
```

Le parole ammesse sono `top`, `bottom`, `side`, `front`, `back`, `left`,
`right` e `all` (che è il file semplice). Una chiave viene letta come faccia
**solo se davanti resta ancora un nome di blocco**: un mod che registra
`mod:top` non si vede il blocco interpretato come la cima di un blocco chiamato
`mod`.

## Come una texture finisce su una faccia

Una texture ha un alto e una sinistra; una fetta di chunk no. La regola, per
tutte e sei le facce:

> **la u dell'immagine va verso destra guardando la faccia da fuori, la v va in
> giù.**

In giù e non in su, perché `v = 0` è la prima riga in memoria, che è la riga in
alto del file.

| faccia | destra | giù |
|---|---|---|
| +X | −Z | −Y |
| −X | +Z | −Y |
| +Z | +X | −Y |
| −Z | −X | −Y |
| +Y | +X | +Z |
| −Y | +X | −Z |

Le prime quattro righe sono `cross(forward, up)` con `up` il mondo: derivate,
non scelte. Le ultime due non hanno un alto proprio, e la convenzione è che si
leggano entrambe con **+X a destra**.

Il gate `pack_draw` mette un blocco da solo in aria con una tile a quattro
angoli di colore diverso e lo fotografa da tutte e sei le facce: rosso in alto a
sinistra, verde in alto a destra, blu in basso a sinistra, giallo in basso a
destra. Ognuno dei tre modi di sbagliare sposta una coppia diversa —
capovolta scambia rosso e blu, specchiata scambia rosso e verde, ruotata di 90°
manda il rosso in un angolo qualsiasi.

---

## Blocchi direzionali

Sei facce diverse non bastano da sole: un blocco il cui davanti è sempre +Z dà
le spalle a tre giocatori su quattro. Un blocco dichiarato **`rotatable`** viene
girato quando lo si piazza, così il davanti guarda chi l'ha messo giù.

```lua
core.register_node("mymod:forno", 150, 110, 60, true, 0, true)
--                                                        ^ rotatable
```

Nel pacchetto:

```ini
mymod:forno        = pietra.png
mymod:forno:front  = bocca.png
```

**Un blocco con un davanti è un blocco solo.** Il giro è un byte sulla
**cella** — `Chunk` tiene un piano di facce accanto a quello dei blocchi e a
quello della luce, viaggia nello stesso frame compresso e finisce nello stesso
file di mondo — e il mesher lo legge quando decide quale immagine va su quale
faccia. Il pacchetto non ne sa niente: dipinge le facce per nome e basta.

Erano **quattro id** fino a poco fa, uno per quarto di giro, e i tre in più
portavano le immagini del primo permutate. Costavano zero tile e zero byte sul
filo, e costavano invece una regola su ogni chiamante: chi chiedeva «che blocco
è questo» doveva chiedere la base. Adesso l'id nel mondo è quello che
`register_node` ha restituito, sempre, e `core.find_nodes` cerca un nome e
trova le celle qualunque verso guardino.

Il giro si decide dalla **posizione** di chi piazza, non da dove sta guardando:
il server ha la prima e non la seconda, e una regola raccontata in due modi sono
due giochi. Il davanti non può guardare in alto o in basso: quattro giri attorno
alla verticale, non ventiquattro — ma questo adesso è un limite di quanto scrive
il piazzamento, non della tabella dei blocchi, ed è per questo che valeva la
pena cambiarlo.

---

## Con foglietto: `pack.txt`

Serve nel momento in cui l'arte è di qualcun altro. Nessun pacchetto scaricato
chiama i suoi file `core_stone.png`, e rinominare ottantacinque file prima di
poterli provare è il motivo per cui non si prova niente.

```ini
# commento; anche ; va bene
name = Kenney Voxel Pack

core:stone = stone.png
core:grass = grass_top.png
core:dirt  = dirt.png
```

- La chiave **con i due punti** è un blocco — o un **oggetto**: vale lo stesso
  nome namespacizzato, e il foglietto non impara che gli oggetti non sono
  blocchi. Senza i due punti è una proprietà del pacchetto: oggi si legge solo
  `name`, le altre si ignorano.
- Un blocco che il foglietto non nomina tiene il proprio nome di file. Le due
  regole convivono: si può nominare metà pacchetto e lasciare che l'altra metà
  si chiami da sé.
- Due blocchi possono portare la stessa immagine.
- Una riga rifiutata **non costa le righe dopo di sé**.

### Il bersaglio è un nome di file, e basta

Niente `/`, niente `\`, niente `..`, niente punto iniziale, e non più di 128
caratteri. Una riga che infrange la regola viene **contata e scartata**, e il
blocco resta con il proprio nome — non con mezzo percorso.

Non è una comodità. Un pacchetto è contenuto, il contenuto qui arriva anche
**dal filo**, e un foglietto che potesse nominare `../../../etc/passwd` è un
foglietto che legge tutto quello che legge il giocatore. Il gate
`texture_pack` prova otto modi diversi di uscire dalla cartella, uno per
controllo, perché un parser che si ferma a `/` ne prende quattro e lascia
passare gli altri.

---

## Vestire un OGGETTO

Un attrezzo non piazza niente, quindi non ha un blocco da cui prendere in
prestito l'immagine: senza una riga nel foglietto resta un quadratino grigio
nella borsa. La riga è come tutte le altre.

```ini
tools:pick   = pick_iron.png
tools:shovel = shovel_iron.png
```

L'oggetto chiede **un file solo**, sotto il proprio nome (`tools_pick.png` per
`tools:pick`): non ha facce, non ha cima né lato, e non ha una catena in cui
ripiegare. Se nessuno l'ha disegnato non prende niente — e allora la borsa
ripiega sul **blocco che l'oggetto piazza**, che è quello che fa sì che una
pila di pietra somigli alla pietra senza che nessuno disegni un'icona apposta.

Gli oggetti entrano nell'atlante **dopo** tutti i blocchi, e questo non è un
dettaglio: volere un'immagine per un oggetto non deve spostare il tile di un
blocco, perché i numeri dei tile stanno dentro i vertici già costruiti.

E condividono lo stesso budget di 256 e la stessa deduplicazione: un oggetto
dipinto con un file che un blocco porta già **non costa un secondo tile**.

## Un tile per IMMAGINE, non per nome

Se il foglietto manda sei blocchi sullo stesso `flat.png`, l'atlante chiede sei
nomi diversi (`core_stone.png`, `core_grass.png`, …) e riceve sei volte la
stessa immagine: costano **un tile solo**. La deduplicazione è sul contenuto
decodificato, non sul nome del file, quindi prende anche due file diversi che
contengono la stessa immagine.

Conta perché l'atlante tiene **256 tile** — un indice di tile è otto bit nel
vertice — e trentanove blocchi per sei facce sono già 234. Misurato: il
pacchetto del gate passava da sei tile a uno.

---

## La dimensione della tile è quella del pacchetto

L'atlante ha **una** dimensione di tile, e a deciderla è il pacchetto: le
immagini vengono decodificate **prima** che l'atlante esista.

- Vince la dimensione su cui la maggioranza dei file è d'accordo; a parità, la
  più grande. Non «la prima»: quale file viene per primo è l'ordine di
  registrazione, quindi con quella regola aggiungere un mod qualunque
  cambierebbe la risoluzione di tutto il mondo.
- Un file a una dimensione che il resto del pacchetto non usa viene **saltato,
  non riscalato**, e il suo blocco torna segnaposto. Lo si dice sulla riga di
  errore.
- Le tile devono essere quadrate, fra 4 e 512 px, e l'atlante intero non supera
  4096 px: oltre, si torna a 32 px e lo si dice.
- Anche i segnaposti si cuociono alla dimensione scelta, così un pacchetto a
  128 px non ha metà mondo a 32.

Kenney a 128 px su 39 blocchi:

```
textures: 'Kenney Voxel Pack' (1 of 1) from assets/packs/kenney -- 19 name(s) mapped, 0 refused
atlas: 8 x 8 tiles at 128px (1024px square), 19 from the pack, 20 generated
```

I venti generati sono i `palette:hue*`, campioni di colore piatto che il
foglietto lascia apposta com'erano.

---

## Dal filo

Un server può indossare un pacchetto (`cnp_server --textures <cartella>`) e il
foglietto si risolve **dalla parte che possiede la cartella**. Il nome che il
client chiede resta sempre quello del blocco, `core_stone.png`: il protocollo
non impara che i pacchetti esistono.

Per lo stesso motivo **F7 non tocca un mondo remoto**. Un mondo che qualcun
altro sta servendo si disegna con quello che ha mandato *quel* server; vestirlo
con un pacchetto di questa macchina sarebbe l'immagine del gioco sbagliato, e
sembrerebbe giusta.

### Il pacchetto viaggia anche come pacchetto

Oltre alle immagini sotto i nomi dell'engine, un server che indossa un
pacchetto serve **il pacchetto stesso**: il foglietto sotto `pack.manifest`, e
ogni file che il foglietto nomina sotto `pack.file.<il suo nome>`. Il client
ricostruisce la cartella nella cache media, la carica e la mette nella rotazione
F7 — quindi puoi **tenere** l'arte di un mondo, indossarla altrove, o toglierla.
Il client lo dice quando succede:

    pack: this world's 'a pack over the wire' kept -- 1 file(s), 2 name(s), F7 to wear it here or anywhere

La cartella si chiama come il **contenuto del foglietto**: lo stesso pacchetto
da due mondi è una cartella sola, e un pacchetto cambiato è una cartella nuova
invece di una mezza vecchia. Non costa nessun blob — i media sono indirizzati
per contenuto, quindi un secondo nome per byte già presenti è una riga in una
tabella — e non costa nessuna versione di protocollo: un mondo senza pacchetto
non offre quel nome, e la trattativa finisce lì.

Un file che il foglietto nomina e che il server non riesce a leggere viene
contato e non è un errore: anche l'atlante del server ricade sul nome
successivo, quindi un client senza quel file si comporta allo stesso modo. È
questa equivalenza la promessa.

---

## Cosa non fa ancora

- **Un solo blocco spedito è direzionale.** `chest:box` è `rotatable`, e il
  foglietto di Kenney gli dipinge il davanti (`chest:box:front = oven.png`).
  Ogni altro blocco del gioco di serie ha sei facce uguali o al più cima, fondo
  e lato.
- **Solo quattro giri, attorno alla verticale.** Un blocco non può guardare in
  su o in giù. Il byte sulla cella ne terrebbe ventiquattro: a scriverne solo
  quattro è il piazzamento.
- **Niente mipmap e niente filtro lineare.** L'atlante è `GFX_FILTER_NEAREST`, e
  a 128 px questo si vede in lontananza più di quanto si vedesse a 32.
- **Niente animazioni di texture, niente varianti casuali per blocco.**
- **Il pacchetto di serie non veste gli attrezzi.** `assets/textures` è
  l'arte generata di questo motore, e non contiene `tools_pick.png` né
  `tools_shovel.png`: con il pacchetto di default i due attrezzi sono
  quadratini. Il pacchetto Kenney li veste, perché la cartella `PNG/Items` del
  suo archivio è disegnata a 128 px come le tile — le stesse dimensioni, niente
  da ridimensionare.
