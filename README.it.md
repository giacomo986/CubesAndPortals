# Cubes and Portals

*[Read in English](README.md)*

[![Un video del gioco](https://img.youtube.com/vi/WdtsBnpiZpA/hqdefault.jpg)](https://www.youtube.com/watch?v=WdtsBnpiZpA)

**[Un video del gioco](https://www.youtube.com/watch?v=WdtsBnpiZpA)**

**Indice**

- [Stato](#stato)
- [Piattaforme](#piattaforme)
- [Scaricare il gioco](#scaricare-il-gioco)
- [Compilare dal sorgente](#compilare-dal-sorgente)
- [Contribuire](#contribuire)
- [Uso dell'intelligenza artificiale](#uso-dellintelligenza-artificiale)
- [Il gioco](#il-gioco)
  - [La piazza e i suoi mondi](#la-piazza-e-i-suoi-mondi)
  - [Il sandbox](#il-sandbox)
  - [L'engine](#lengine)
- [Giocare e testare: riferimento](#giocare-e-testare-riferimento)
  - [Comandi](#comandi)
  - [Il menù del titolo](#il-menù-del-titolo)
  - [Client (cnp_client)](#client-cnp_client)
  - [Server (cnp_server)](#server-cnp_server)
  - [Amministrazione del server](#amministrazione-del-server)
  - [Strumenti](#strumenti)
  - [Test](#test)
- [Tecnologia](#tecnologia)
- [Documentazione](#documentazione)
- [Licenza](#licenza)

Un sandbox voxel scritto da zero, in cui **i portali fanno parte dell'engine**: un portale
mostra un altro mondo, anche uno che gira su un altro server, e lo si attraversa senza schermata
di caricamento. Dalla piazza dello spawn i portali portano a mondi che sono altri giochi: una
pista di kart, un arena shooter, un action RPG isometrico, camere con i puzzle a portali, le
backrooms, un percorso di parkour e un assalto da giocare insieme. Si gioca da soli o in
multiplayer, e ogni mondo è fatto di mod in Luau che chiunque può leggere e cambiare.

È un **progetto collaborativo e aperto ai contributi**: codice, mondi, mod, texture pack,
traduzioni, segnalazioni di bug e partite di prova sono tutti benvenuti (vedi
[Contribuire](#contribuire)).

La documentazione di approfondimento (`docs/`, le guide dei mondi, il changelog) è in italiano;
codice, commenti, `TODO.md` e messaggi di commit sono in inglese. Issue e pull request vanno bene
in tutte e due le lingue.

## Stato

**Alpha giocabile.** C'è il sandbox (un mondo senza bordo con biomi, grotte, villaggi con i loro
abitanti e la loro economia, animali e mostri, circuiti, scacchi e carte), ci sono i sette mondi
dietro i portali della piazza, il laboratorio finale, altri due mondi dopo di esso, e il
multiplayer. Molte cose sono ancora incomplete o grezze: cosa manca e cosa non va è in
[`TODO.md`](TODO.md), cosa cambia a ogni versione in [`CHANGELOG.md`](CHANGELOG.it.md).

Finché il gioco è in alpha una versione nuova può non leggere i mondi salvati con la
precedente, né giocare con i suoi server: quando succede il changelog lo dice.

## Piattaforme

| Sistema | Pacchetti | Stato |
|---|---|---|
| **Linux** x86_64 | `.tar.gz`, `.AppImage`, `.flatpak` | La piattaforma principale: il gioco si sviluppa e si prova qui. Il tarball vuole glibc 2.38 o più recente (Ubuntu 24.04, Fedora 39 e successive) |
| **Windows** 10 o più recente, x86_64 | `.zip` | Funziona, ma è stato provato soprattutto con wine. Lacune note: niente stack dump in caso di crash, e un client che va in crash chiude il suo server locale senza salvare |
| **macOS** 11 o più recente (Apple silicon) | `.dmg` | Il pacchetto si costruisce, ma **non è mai stato provato su un Mac** |

Il client vuole una scheda video con **OpenGL 3.3**; su Linux gira su X11, o su Wayland
attraverso XWayland. Il server (`cnp_server`) non ha finestra e gira ovunque giri il resto.
Segnalazioni da Windows e da macOS veri sono particolarmente utili.

## Scaricare il gioco

I pacchetti già compilati sono nella pagina **[Releases](../../releases)**. A ogni versione (un
tag `v*`) GitHub Actions li costruisce per tutti e tre i sistemi e li pubblica come pre-release.

- **Linux**: si scompatta il `.tar.gz` dove si vuole e si avvia `./cnp_launcher`; oppure si rende
  eseguibile l'`.AppImage` (`chmod +x`) e lo si avvia; oppure si installa il Flatpak dal suo
  repository, che lo tiene aggiornato (`flatpak update` porta ogni versione nuova):

  ```sh
  flatpak install --user https://giacomo986.github.io/CubesAndPortals/cubes-and-portals.flatpakref
  ```

  Anche il `.flatpak` di ogni release si installa (`flatpak install --user <file>.flatpak`),
  ma un Flatpak installato da file non si aggiorna mai: meglio dal repository.
- **Windows**: si scompatta lo `.zip` e si fa doppio clic su `cnp_launcher.exe`.
- **macOS**: si apre il `.dmg` e si trascina l'app dove si vuole.

Ogni pacchetto apre il **launcher**: Gioca avvia il gioco; Versioni tiene più versioni una
accanto all'altra (scaricate dalla pagina Releases, ognuna controllata con il manifest firmato
della release prima di girare); Mondi gioca un mondo salvato, copiandolo prima quando lo apre una
versione più nuova; Mod mostra, aggiunge (trascinando una cartella o un `.cnpmod` sulla finestra),
spegne e toglie le mod, ovunque o per mondo. Un server che vuole un'altra versione rimanda il
gioco al launcher, che avvia quella. Il gioco da solo è `cnp_client` (`AppRun game`,
`flatpak run … game`).

I pacchetti non sono firmati. Al primo avvio Windows SmartScreen chiede conferma ("Ulteriori
informazioni", poi "Esegui comunque"); su macOS si fa clic destro sull'app e Apri.

**Aggiornamenti.** Il gioco controlla da solo se c'è una versione nuova: all'apertura del menù
chiede a GitHub l'ultima release di questo repository (manda solo quella richiesta, nulla di chi
gioca). Se ce n'è una più recente compare il bottone "Nuova versione …: scarica", che apre la
pagina Releases; un Flatpak installato dal repository (sopra) lo aggiorna invece
`flatpak update`. Il controllo si spegne in Opzioni, Aggiornamenti, o per una sola partita con
`--no-update-check`.

Mondi salvati e opzioni stanno in `~/.local/share/cubes-and-portals` e
`~/.config/cubes-and-portals` su Linux, in `~/.var/app/io.github.giacomoperin.CubesAndPortals`
con il Flatpak, in `%APPDATA%\cubes-and-portals` su Windows.

## Compilare dal sorgente

Tutte le librerie che il gioco usa (raylib, Luau, ENet, zstd, SQLite, Monocypher, flecs, Jolt)
sono in `third_party/`, come submodule, e si compilano insieme al gioco. Servono **CMake 3.25+**
e un compilatore C++20 (**GCC 13** o Clang). Su Linux (Ubuntu/Debian):

```sh
sudo apt install build-essential cmake ninja-build git \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libasound2-dev

git clone --recursive https://github.com/giacomo986/CubesAndPortals.git
cd CubesAndPortals
cmake -S . -B build -G Ninja
cmake --build build -j
./build/src/client/cnp_client
```

Un clone fatto senza `--recursive` si completa con `git submodule update --init --recursive`.

Il build produce tre programmi:

| | |
|---|---|
| `build/src/launcher/cnp_launcher` | il launcher: versioni, mondi, mod; quello che aprono i pacchetti |
| `build/src/client/cnp_client` | il gioco: menù, mondi salvati, multiplayer |
| `build/src/server/cnp_server` | un mondo per altri giocatori, senza finestra |
| `build/tools/cnp_modpack` | impacchetta un mod in un file `.cnpmod` |

Un build di sviluppo legge `mods/`, `games/` e `assets/` dal checkout, quindi va avviato da lì.
I test si lanciano con `ctest --test-dir build` (13 test, pochi secondi).

I pacchetti per chi gioca si costruiscono con gli script di `tools/dist/` e finiscono in
`<cartella di build>/dist/`. Il pacchetto Windows si compila **da Linux** con zig, quello macOS
**su un Mac**. Tutti i dettagli (opzioni di CMake, pacchetti, versioni) sono in
[`docs/BUILDING.md`](docs/BUILDING.it.md).

## Contribuire

Il progetto è aperto a tutti, e si può aiutare in molti modi:

- **giocare e segnalare** bug, crash e cose confuse, aprendo una issue: cosa è successo, cosa ci
  si aspettava, sistema operativo e versione del gioco (in basso a destra nel menù), e se
  possibile il log;
- **tradurre**: una lingua nuova è un file nuovo in `assets/lang/` (vedi `it.txt`);
- **fare mod, mondi e texture pack**: [`docs/MODDING.md`](docs/MODDING.it.md),
  [`docs/API.md`](docs/API.md) e [`docs/TEXTURE_PACKS.md`](docs/TEXTURE_PACKS.it.md); i sorgenti
  Blender dei modelli sono in [`assets/blender/`](assets/blender/README.md);
- **lavorare sull'engine**: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.it.md) spiega come è fatto il
  codice e dove va una feature nuova; [`TODO.md`](TODO.md) elenca quello che manca.

Come si propone una modifica:

1. per una modifica grande (un mondo nuovo, un cambio al protocollo di rete o al formato dei
   salvataggi) conviene aprire prima una issue e parlarne;
2. una pull request fa **una cosa sola**, e la sua descrizione dice cosa cambia, perché, e come
   la si è provata (in gioco e/o con `ctest`);
3. il codice compila senza warning nuovi e i test passano;
4. il codice nuovo somiglia a quello attorno (nomi, commenti, stile), e la documentazione della
   parte toccata viene aggiornata insieme;
5. texture, modelli e suoni sono propri o con una licenza compatibile, dichiarata in un
   `CREDITS.md` nella cartella del mod (come [`games/track/CREDITS.md`](games/track/CREDITS.md)).
   Niente nomi, marchi o materiale di giochi commerciali.

**Ogni contributo è accettato, purché non sia malevolo o disordinato.**

- *Malevolo*: malware, backdoor, raccolta di dati di chi gioca, codice che nasconde quello che fa,
  materiale copiato senza averne il diritto, contenuti offensivi. Una pull request così viene
  chiusa, e chi l'ha aperta non ne apre altre.
- *Disordinato*: pull request enormi che mescolano cose diverse, riformattazioni di massa, codice
  morto, modifiche senza descrizione, build rotto, codice generato e mai riletto. Una pull request
  così non viene rifiutata per principio: si chiede di dividerla o sistemarla.

Chi contribuisce accetta che il suo contributo sia distribuito con la licenza del progetto (MIT).

## Uso dell'intelligenza artificiale

Questo progetto è sviluppato con un **uso esteso dell'AI**, e lo dice apertamente:

- **Codice, documentazione e messaggi di commit** sono scritti in gran parte da Claude
  (Anthropic), attraverso Claude Code, sotto la direzione di chi mantiene il progetto, che decide
  cosa fare, prova il gioco e approva ogni modifica. I commit scritti così lo dichiarano con una
  riga `Co-Authored-By: Claude`.
- **Modelli 3D, texture e quasi tutti i suoni** sono generati da script Python (e Blender) in
  `tools/`, scritti anch'essi con l'AI: nessun asset è preso da altri giochi.
- **Le illustrazioni del mazzo di carte** e alcuni suoni sono generati con ElevenLabs.

I contributi fatti con l'aiuto dell'AI sono benvenuti alle stesse condizioni degli altri: chi
apre la pull request ne risponde, l'ha letta, capita e provata, e lo scrive nella descrizione.
L'output di un modello incollato senza verificarlo rientra nel "disordinato".

## Il gioco

### La piazza e i suoi mondi

Si parte da una piazza. Attorno, sette portali, ognuno dentro una struttura che somiglia al
mondo che c'è dietro (`mods/gates`), portano a sette mondi, tutti stanze dello stesso server
(`tools/track.sh`, o qualunque mondo avviato dal menù del titolo):

- **La pista** (`games/track`): kart a corpo rigido (Jolt) con sospensioni e nitro, guidati dal
  server con i tasti del giocatore. Cinque piste a tempo con classifica, una che passa per
  l'Inferno vero e una con cinque giri della morte. Pezzi di pista come nei giochi di corse
  acrobatiche (giri della morte bassi, alti e larghi, adattatori da larga a stretta) e un editor
  per costruire piste nuove: propone i pezzi che si attaccano al capo aperto, annulla e chiude il
  circuito da solo. Scatole di power-up lungo la strada (turbo, banane, gusci verdi e rossi,
  stella, fulmine, inchiostro), derapata con le sgommate, fiamme dagli scarichi e un garage
  (`/garage`) per scegliere colori e 14 pezzi del proprio kart.
- **L'Inferno** (`games/inferno`): caverne di scoria, lava che scorre, fortezze, brute, wraith e
  cinder. Ci si arriva anche da un portale di ossidiana costruito a mano.
- **La base** (`games/yafps`): un arena shooter moderno con cinque armi e ondate di demoni.
- **La landa** (`games/arpg`): un action RPG isometrico punta e clicca. Un paese con mercanti e
  forziere, tre zone di aree e corridoi, waypoint e portale cittadino, tre classi con quattro
  abilità ciascuna, livelli, e oggetti normali, magici, rari, unici e di set che si vedono
  addosso, con le loro animazioni.
- **Le camere di prova** (`games/chambers`): sei camere con i puzzle a portali, una portal gun che
  cresce con chi gioca, cubi, pulsanti e pedane di lancio.
- **Le backrooms** (`games/backrooms`): tre livelli infiniti con scale, buchi, piscine, scivoli e
  due creature; casse rare contengono la via del ritorno. Guida in
  [`games/backrooms/README.md`](games/backrooms/README.it.md).
- **Il parkour** (`games/parkour`): corsa sui muri, rimbalzi, scivolate, capriole e rope gun.
  Guida in [`games/parkour/README.md`](games/parkour/README.it.md).

**Una casa più grande dentro** (`mods/pocket`). Una porta tascabile (una porta e un rubino in un
anello di cornici di portale) messa in un muro diventa un vano su una sala tutta tua, quindici per
quindici e alta sei: costruiscici intorno una capanna tre per tre ed è più grande dentro che
fuori. Le sale stanno in una stanza a parte, `pockets`, con tutti i mod del sandbox, anche i tuoi;
rompi la cornice e la porta si ripiega, e rimessa giù si riapre sulla stessa sala. Il sindaco di un
villaggio vende una capanna tascabile già fatta; quattro porte tascabili possono essere in uso insieme.

**Le sette chiavi** (`mods/keys`, la fine del gioco). L'ingegnere che ha costruito i portali ha
chiuso l'Ospite nel laboratorio e ha diviso la serratura in sette chiavi, una in ogni mondo,
dietro una prova. L'ottavo portale della piazza, una cornice d'acciaio scuro con sette
piedistalli, porta al **laboratorio** (`games/lab`) e respinge chi non ha mai tenuto tutte e sette
le chiavi. Ogni mondo si lascia da un'uscita in fondo alla sua prova, e ogni chiave aspetta su un
piedistallo dorato accanto. Nel laboratorio l'Ospite si batte con un po' di ogni mondo: portal
gun, fucile, una salita di parkour, valvole sotto la lava, una torcia nel buio, il gelo, e infine
la fuga in kart in un tunnel e i titoli di coda. Dopo, il sandbox resta libero e si aprono altri
due mondi:

- **Il fronte** (`games/front`): un campo di battaglia nello stile dei grandi sparatutto a
  squadre, due basi e tre punti da prendere, rossi contro blu, con bot e mezzi; ogni base ha una
  pista da cui decollano i suoi aerei.
  La tribuna dove si sceglie la squadra ha il campo intero in miniatura sotto il pavimento di vetro,
  e il lanciarazzi aggancia i veicoli nemici attraverso il suo vetrino.
- **L'assalto** (`games/assault`): l'assalto finale alla dimensione di Basoalth, da giocare
  insieme: quattro fortezze, un negozio, alleati e costruttori, un boss che diventa un gigante e
  la fuga in jeep in un tunnel di lava.

`ALL_OPEN = true` in `mods/keys/init.luau` apre tutti i mondi a tutti, per provarli senza vincere
niente: le uscite, gli archi, il cancello del laboratorio, il fronte e l'assalto. `false` è il
gioco come viene distribuito.

Sulla piazza, verso 31 20 43, una porticina in una siepe e una teca di vetro nel terreno
(`mods/garden`): dal vetro si vede dall'alto, in miniatura, un giardino recintato a est della
piazza; dalla porticina ci si esce dentro grandi un sedicesimo, i blocchi d'erba grandi come stanze
e la siepe una scogliera. Si torna dalla stessa apertura minuscola, nel suo stipite di legno tra due
funghi, e si esce dalla porticina grandi come prima.
Da piccoli si cammina veloci quanto dice la taglia: il giardino si attraversa in un minuto o poco più.
Le formiche fanno la spola tra il nido e i fiori, le coccinelle girano, le farfalle si posano sui
fiori: a grandezza vera, lì una formica è grande come un cane. Una creatura che passa dalla
porticina cambia taglia come te (un sedicesimo entrando, fino a quattro volte uscendo).
Una sella su una coccinella la addomestica e ti porta come un pony: cavalcata fuori dalla porticina esce grande quanto te, più di un metro, e aspetta sellata dove scendi (Ctrl o clic destro a terra; clic destro su di lei per risalire).

### Difficoltà, regole e obiettivi

"Mostri" nella pagina del mondo nuovo, o nella pagina Modifica di un mondo salvato, sceglie quanto è
difficile il sandbox: nessuno (pacifico), pochi e deboli (facile), come sempre, tanti e forti
(difficile); `/difficulty` in partita (`cnp_server --difficulty`). Pacifico non fa uscire nessuna
creatura che combatte e manda via quelle in giro, e quello che lasciano solo i mostri si trova
ancora: lo spawner quieto di un dungeon lascia ogni tanto ossa, carne, polvere da sparo e frecce,
l'altare del profondo dà cuore e frammenti senza lotta, i villaggi non hanno assedi. Facile dimezza
il colpo di una creatura, quante ne escono e la fame; difficile aggiunge metà a tutti e tre. Le
regole (`/rule`): tenere la borsa alla morte in un mondo survival, il sole che si muove, il meteo
che cambia, la fame. Il pulsante **Mondo** del menu di pausa mostra tutto, con meteo, ora e
stagione, come pulsanti per un admin, e porta agli **obiettivi**: 21 cose da fare, dal primo legno
alla chiave di ogni mondo, annunciate quando le raggiungi (`/page goals`).

### Survival: i mondi sparsi sulla terra

"Mode: Survival" nella pagina del nuovo mondo del menù del titolo (`cnp_server --survival` sulla
stanza sandbox, `tools/track.sh --survival`) fa lo stesso gioco senza piazza:

- **Niente piazza, uno spawn a caso.** Il terreno è il rumore ovunque; si parte su terra asciutta
  e piana scelta dal seme fino a 2500 dal centro, sotto il cielo aperto se ce n'è entro 64. Niente
  catalogo (C), neanche per il proprietario (`/perm creative owner` lo ridà), e una morte lascia la
  borsa in una tomba dove è successo, segnata sulla mappa (`mods/grave`).
- **Portali sparsi sulla terra** (`mods/gates`): cinque celle di 256 su sei ne hanno uno, una porta
  nell'aspetto del suo mondo con accanto un arco per tornare e una fila di dieci lampade sopra:
  tante accese quanta è la sua difficoltà, da 1 a 10, che sale di uno ogni 300 nodi dallo spawn.
  Un sito di ogni mondo sta entro 950 dallo spawn, facile; il laboratorio è a 1500-2200, chiuso
  dalle sette chiavi come sulla piazza; il fronte e l'assalto, oltre 1500, si aprono dopo il finale.
- **Dietro ogni portale un livello suo** (`mods/keys` dà a ogni sito uno slot del suo mondo): il
  parkour costruisce un percorso dalle sue stazioni e da pezzi nuovi (salti, gradini, pilastri, pad
  di rimbalzo, lastre che cedono), 3 + d, con varchi misurati sul salto del corpo; le backrooms una
  stanza d'uscita a 48 + 24 d, nel buio del Livello 1 da d 4, con le luci che si spengono da d 5;
  la pista un circuito chiuso suo (chicane, una salita, un trampolino secondo d) con tre giri da
  battere; le terre dell'arpg un percorso di 3 + d aree fino a un covo e al suo signore; le camere
  di prova una copia della loro fila, un tratto di 2 + d / 2 camere più avanti quanto più è dura;
  l'arena, il cuore dell'Inferno, il fronte e l'assalto scalano con d ondate (con una variante
  ciascuna da d 4), guardiani, salute, bot e guarnigioni. L'uscita di ogni livello è chiusa, vicina
  a dove si comincia e nascosta dietro una sua parete sotto una lampada spenta; a livello fatto la
  parete cade, la lampada si accende, e una porta o il circuito stesso riporta lì. Due siti dello
  stesso mondo si possono attraversare insieme.
- **Ricompense e chiavi.** Al ritorno al portale una cassa tiene ciò che quel mondo dà (rubini e
  binari dalla pista, oro e ossidiana dall'Inferno, frecce e armatura dall'arena, ...), il 40% in
  più per ogni gradino di d, un quarto per un livello già fatto, ogni tanto qualcosa di raro. La
  chiave di un mondo viene solo da un livello di d 4 o più.
- **Indicazioni** (`mods/villagers`): un abitante ogni tanto parla del portale più vicino, in modo
  vago (com'è, da che parte, vicino o lontano, come sono tornati quelli che ci sono andati); una
  carovana, di uno vicino al villaggio da cui viene; il sindaco, per 2 + d rubini, esattamente,
  con la strada sulla mappa. I portali visti vanno sulla mappa, verdi una volta fatti.

### Il sandbox

**Il territorio.** Un mondo senza bordo: pianure, deserti, tundra e montagne larghi qualche
centinaio di blocchi, oceani e mari bassi con le spiagge, fiumi che arrivano al mare, laghi in
quota, gole con un torrente sul fondo, grotte collegate fra loro, minerali in vene dentro la
pietra, e il ciclo giorno/notte con nuvole sopra cui si vola. Sotto terra ci sono miniere
abbandonate e cripte con le casse, e una scala per risalire.

**Creature che si arrampicano** (`mods/mobs`). Gli zombie salgono un muro per raggiungerti quando stai più in alto, e un tipo più rapido, il corridore, scatta, si arrampica e si tuffa su di te da sette blocchi.

**Villaggi** (`mods/villages`, `mods/villagers`). Un mondo nuovo ha città (una per riquadro di 2048 dove il terreno lo permette, larghe fino a 500, palazzi al centro) unite da strade maestre lastricate che passano i monti e fanno ponti sull'acqua, cittadine fitte attorno alle città e villaggi radi nelle zone selvagge; `/town city` porta alla più vicina. Paesi e cittadine con piazza, vie, case, campi,
fucina e lampioni, collegati da una rete di sentieri tortuosi -- ghiaia, arenaria, ciottoli o terra
battuta secondo il terreno, con cartelli, pietre miliari, lanterne e ponti. Li abitano tre specie:
umani contadini, rospi pescatori e talpe raccoglitrici, ognuno con una casa, un mestiere, i pasti
e un letto. Ogni villaggio ha:

**Museo, album e giornale** (`mods/villagers`, `mods/album`). Ogni villaggio costruisce un museo: il suo
curatore espone il primo di ogni pesce, minerale, gemma e reperto che un giocatore porta e lo paga in
rubini. Una macchina fotografica scatta foto vere -- tieni premuto il tasto destro per guardare nel mirino,
clic sinistro per scattare -- istantanee che tutti vedono com'erano al momento dello scatto, tenute in un album (`/album`) e appese in cornici; una
foto di una vetta, delle Cavità, di una reliquia o delle nuvole vale qualcosa per un museo. E la bacheca
di un villaggio tiene il suo giornale: chi l'ha difeso da un assedio, ha portato una carovana, ha donato
qualcosa al museo, o è stato scoperto a rubare.

- una ronda (guardie, arcieri e un ispettore che guarda nelle borse in cerca di refurtiva: il
  furto viene marcato, e un ladro è perdonato una volta, poi braccato);
- un sindaco che vende lotti e case, e un costruttore che le alza blocco per blocco con i
  materiali portati dai taglialegna, dai minatori e dagli scavatori del villaggio;
- un mercante e dei lavoratori che commerciano a rubini, con prezzi che seguono l'offerta, e
  una cassa del villaggio;
- una miniera, un artigiano che rifà gli attrezzi, e una casa per ogni ruolo.

**Territori** (`mods/territory`): una pietra di confine fonda un territorio che cresce mentre i
proprietari ci stanno dentro. Lì dentro i proprietari sono più veloci e resistenti, gli estranei
più deboli; quello che gli estranei prendono lì è marcato rubato.

**Giochi in piazza** (`mods/chess`, `mods/arena`): il tavolo degli scacchi gioca anche a dama italiana e
al mulino; un gong avvia una sfida PvP di tre minuti nel cerchio attorno, con punti e classifica -- o un
prop hunt, in cui chi si nasconde diventa un blocco del mondo e chi cerca ha tre minuti per trovarlo.

**Decorazioni** (`mods/building`): vetro colorato e lastre in sedici colori, muretti di pietra e
mattoni, piastrelle smaltate e a scacchi, sedie e tavoli di ogni legno, tappeti, lanterne.

**Ferrovie** (`mods/building`, `mods/carts`): binari, curve, salite, binari alimentati e scambi come
veri modelli; un carrello da miniera in cui si sale e si viaggia seduti.

**Progetti** (`mods/blueprints`): si copia una costruzione tra due angoli (fino a 32x24x32) su un
progetto, lo si porta con sé, lo si traccia come un fantasma trasparente e lo si costruisce a mano --
o lo fa il costruttore del villaggio, o un robot. Il robot monta una torre di impalcature per ciò
che la sua mano non raggiunge e poi la smonta; l'impalcatura (sei da tre assi e quattro bastoni) si
sale come una scala, chiunque può farlo.

**Commissioni**: ogni villaggio ne espone fino a tre su una bacheca accanto al pozzo -- portare ciò che
manca, procurare un materiale, fabbricare utensili, cacciare di notte, portare un pacco a un altro
villaggio, esplorare un luogo lontano -- prese e pagate (dalla tesoreria) dal sindaco e dal mercante;
dopo cinque, il mercante fa lo sconto.

**Eventi del villaggio**, annunciati dalla campana del municipio e sulla bacheca: carovane di un mercante
e un mulo che portano merci vere a un altro villaggio (si compra da loro per strada, o si scortano a
pagamento), rari assedi notturni annunciati al tramonto (tre ondate contro la guardia: nessun abitante
perso, rubini a chi difende) carovane da villaggi non ancora visitati (il mercante indica la strada, anche sulla mappa) e la festa
del raccolto l'ultimo giorno d'estate -- bancarelle e lanterne
in piazza, cibo a metà prezzo, musica, fuochi d'artificio la sera (`mods/fireworks`).

**Allevamento** (`mods/animals`): si tinge una pecora per la lana colorata, si munge una mucca con un
secchio (formaggio, torta), una mangiatoia di frumento o semi fa da stalla (tiene gli animali e li fa riprodurre), e un
robot può badare agli animali.

**Pesca**: cosa abbocca dipende dall'acqua -- persici e carpe nei laghi, trote nei fiumi, merluzzi e
anguille in mare, tonni nell'oceano, un raro pesce rosso ovunque -- e i pescatori-rana li comprano.

**Alchimia** (`mods/alchemy`). Un alambicco distilla una fiasca d'acqua e un ingrediente in una
pozione: vista notturna, rapidità, pelle di pietra, guarigione, fretta, respirare sott'acqua, vigore -- tre minuti
ciascuna, che restano se esci o se il server riparte. Sott'acqua l'aria dura quindici secondi (`mods/breath`).

**Api** (`mods/bees`). Nidi selvatici tra i fiori dei prati, e arnie di assi e favi: si riempiono di
miele tanto più in fretta quanti più fiori hanno attorno, d'inverno dormono, e aiutano a crescere i
campi vicini. Con una fiasca si prende una bottiglia di miele (cibo), a mani nude due favi (una
pozione, un'arnia); una torcia sotto, o un affumicatore, tiene le api dal pungere. Di giorno le api
ci volano attorno. Api allevate: un retino cattura la regina di un nido selvatico, e un'arnia
artificiale la accoglie, con telaini per riempirsi prima, un'imbottitura di lana per l'inverno e un
infusore che fa miele speciale da ciò che ha attorno (funghi luminosi, lava, ametista); una pressa
lo lavora in lampade di miele, cera ardente per la fornace e ametista, ogni tanto un diamante.

**Cavità** (`mods/deep`). Sotto la roccia madre di alcune regioni c'è un paese di caverne sigillato,
a cui si scende da una voragine con i pioli: funghi e muschio luminosi, cristalli, laghi di lava, e un
altare il cui guardiano si sveglia quando ci si avvicina. Nelle caverne si aggirano lurker pallidi.

**Reliquie** (`mods/relics`). Rari templi nel deserto, relitti e rovine sommerse, ognuno con un
piccolo enigma davanti al forziere: glifi da girare come mostra la parete, i passi del diario di bordo
da contare e scavare, frammenti da rimettere sui piedistalli.

**Mappa e bussola** (`mods/maps`). La bussola in mano punta a casa, al letto o allo spawn; la mappa
in mano mostra il terreno attorno dove si è già stati, tenuto dal server fra una sessione e l'altra.

**Pietre del viaggio** (`mods/travel`). Ogni villaggio ne ha una accanto al pozzo, e una si
fabbrica per il proprio territorio: toccarla la fa ricordare, e da qualunque pietra si paga un
rubino ogni 200 nodi per aprire un portale verso una di quelle ricordate, attraversabile da tutti
per cinque minuti. Un mondo tiene quanti portali si vuole; sono attivi solo quelli vicini ai
giocatori.

**Carrelli** (`mods/carts`). Cinque lingotti di ferro fanno un carrello: tasto destro su un binario
per salire, salto per scendere. I binari vanno nei due sensi, curvano e salgono; un binario alimentato spinge a 11 m/s
quando il cinabro lo accende ed è una stazione quando è spento; uno scambio alimentato gira a destra.

**Barche** (`mods/boats`). Cinque assi fanno una barca: tasto destro sull'acqua per sedersi,
remare al doppio del nuoto e sterzare con A e D (scansa le creature e rimbalza sulle altre barche), tasto destro su una riva (o arenarsi) per scendere con la barca nella borsa.

**In volo** (`mods/glider`, `mods/balloon`). Un deltaplano di lana e bastoni, tenuto in mano, si apre
quando scendi da un'altura e ti porta dove guardi -- guarda in basso per picchiare, in alto per
rallentare, accovacciati per chiuderlo e cadere. Una mongolfiera parte da un terreno libero: salta per
salire, accovacciati per scendere, i tasti e il vento la portano piano, fin sopra le nuvole; atterrata,
tasto destro per ripiegarla. Il pilota del deltaplano sta appeso prono alla barra; quello della
mongolfiera sta nel cesto e tira la valvola del bruciatore mentre sale, con la fiamma che ruggisce nel
pallone.

**Dopo il finale** (`mods/rewards`). Chi ha battuto l'Ospite trova nel sandbox gli strumenti dei mondi,
in forma ridotta: la pistola a fune (trenta agganci al giorno), una pistola dei portali (dodici colpi al
giorno, su qualsiasi muro) e il kart, da guidare ovunque (F per scendere). `/rewards` li ridà.

**Meteo** (`mods/weather`). Cielo sereno, poi pioggia o temporale per qualche minuto: il cielo
e le nuvole si fanno grigi, la pioggia cade attorno a te e si ferma sui tetti, i campi a cielo
aperto vengono innaffiati. Nella tundra e sulle vette nevica e la neve si posa; nel deserto
non cade niente. Un temporale colpisce il suolo vicino ai giocatori, con il tuono. Stagioni di
cinque giorni: d'autunno le foglie si fanno arancioni e d'inverno spente (l'abete resta verde),
l'estate ha più temporali e la primavera più pioggia, i campi rallentano d'autunno e d'inverno
crescono solo sotto le lampade, e d'inverno nevica ovunque tranne nel deserto. I mercanti fanno pagare il doppio il cibo d'inverno e la legna
d'autunno.

**Il cielo** (`mods/sky`). Una notte serena su cinque porta una pioggia di meteore, e ogni tanto una
meteorite che cade vicino, scava un cratere e lascia una pietra con un diamante e ferro grezzo. D'inverno
l'aurora si stende sul nord, vista dalla tundra e dalle vette. Un giorno su dodici un'eclissi oscura la
terra per tre minuti, e le creature della notte escono. `/sky` ne fa partire una.

**Creature.** Zombie e scheletri arcieri escono al buio e bruciano al sole; maiali, mucche, pecore
e galline pascolano, si riproducono e crescono; un cane e un gatto si addomesticano e seguono chi
li ha presi (`mods/mobs`, `mods/animals`, `mods/pets`).

**Sopravvivenza e costruzione.** Borsa e hotbar, craft, casse (due affiancate fanno una doppia),
una fornace con le righe per materiale, combustibile e prodotto, attrezzi in cinque materiali che
si consumano, armature in quattro, fame, esperienza e tavolo da incantamento, letti che fanno
passare la notte, campi (zappa, semi, grano, pane, angurie e zucche, acqua e luce), pesca, canna da zucchero, carta
e libri, porte, scale a pioli e botole.

**Macchine.** Circuiti di cinnabar (`mods/cinnabar`): polvere, leve, pulsanti, torce, ripetitori,
comparatori, lampade, pistoni, observer e hopper, così orologi e logica funzionano. Nastri
trasportatori (`mods/conveyor`) che si fondono, curvano e salgono, con selezionatori ed
estrattori. Robot aiutanti (`mods/helpers`) da programmare con fino a sei ordini: abbattere
alberi, piantare, scavare, cercare minerali, ordinare le casse, cucinare, cavare, illuminare il
terreno, seguire chi li ha messi.

**Telecamere** (`mods/cameras`). Una telecamera -- a terra, su una parete o al soffitto, dove la si
appoggia -- e uno schermo: tasto destro sulla telecamera, poi
sullo schermo, e lo schermo mostra dal vivo cosa vede la telecamera -- la base, la gara, un villaggio.
Tasto destro di nuovo sullo schermo per prendere i comandi: la visuale va sullo schermo e con W A S D
si gira la telecamera.
Una telecamera che vede qualcuno dà energia al cinabro accanto: una porta che si apre, una lampada, un
allarme.

**Passaporto** (`mods/passport`). Ogni server amico da cui un portale ti porta timbra il tuo passaporto
(`/passport`). Una cassetta delle lettere spedisce un pacco -- quello che hai in mano -- o una lettera a
`nome@server` su un server amico, consegnati la prossima volta che entra.

**Giochi sulla piazza.** Scacchi (`mods/chess`) con tutte le regole classiche, l'orologio e nove
varianti (960, re della collina, tre scacchi, corsa dei re, vinciperdi, atomica, crazyhouse,
orda). Carte (`mods/cards`) col mazzo napoletano da quaranta: briscola, cinquiglio, scopa e
tresette, da due a cinque giocatori, con il tavolo che gioca i posti vuoti.

**Personaggi ed equipaggiamento.** Si sceglie robot, umano, uomo talpa o uomo rana, uno stile,
tre colori e due accessori, visti da tutti. Sulla schiena: un jetpack per volare, uno zaino di
cuoio con 27 slot suoi, o uno zaino di metallo con due braccia snodate che tengono uno strumento
ciascuna. Il catalogo creativo (C) ha una casella di ricerca. `/creative on` è la modalità creativa: volo
senza jetpack, nessun danno, scavo veloce (libera in un mondo tuo, da admin su un server). `/time`
cambia l'ora, `/kill` riporta al letto o allo spawn quando si resta bloccati. In un mondo con una
prova, `/giveup` la abbandona: si muore e si torna nel sandbox, fuori dal portale del mondo. Ciò che un piedistallo o
un altare contiene (una chiave, la pistola dei portali, un frammento) galleggia sopra, girando.

### L'engine

- **Voxel.** Chunk da 32³, greedy meshing su thread, vertici da 12 byte, luce del sole e dei
  blocchi con flood fill, ambient occlusion, frustum e occlusion culling, profiler a schermo
  (F6).
- **Luce.** Buio vero: una grotta senza torce è nera, e la notte quasi. Sole e luna fanno ombra
  (anche le foglie); le torce illuminano le facce rivolte verso di loro. La luce di prima c'è
  ancora: Opzioni, Luce: classica. Le lampade fanno luce del loro colore: un blocco dà luce del colore con cui è registrato, più tenue, e due luci che si incontrano si mescolano.
- **Portali.** Un'immagine continua con la vista reale, ricorsione fino a tre livelli, tutte e 24
  le orientazioni (pavimento, parete, soffitto, girati), corpi tagliati mentre attraversano, e
  l'orizzonte che torna in bolla dopo un attraversamento inclinato. Attraverso un portale si
  mantiene la velocità, girata come il portale. I portali **si costruiscono in gioco**
  (`mods/portal`): una cornice attorno a un buco si apre quando va a posto l'ultimo blocco, fino a
  16 per mondo. Un'apertura senza l'altro capo è chiusa da una membrana animata.
- **Mondi e server.** Due mondi in un solo processo, portali verso **un altro server** con ticket
  firmati, e più mondi (stanze) sullo stesso server e sulla stessa porta. Il mondo non ha bordo:
  le colonne si generano o si caricano quando un giocatore si avvicina, e si lasciano quando non
  c'è nessuno.
- **Rete.** Server headless a 20 tick al secondo, ENet + zstd, cifratura X25519 +
  XChaCha20-Poly1305, identità del giocatore a chiave, media content-addressed con una cache su
  disco condivisa fra i mondi. Il client **adotta** la tabella dei blocchi del server; il server
  valida il movimento e lancia lui il raggio di ogni colpo.
- **Modding.** Luau, una VM per mod con quota di memoria, budget di interruzione e capability. Hot
  reload (F5), event bus tipizzato, ECS scriptabile, ABM/LBM, stage del mapgen, blocchi a scatole
  o disegnati come modello, movimento per mondo e per corpo, pacchetti `.cnpmod` con versioni e
  dipendenze. Un mondo può mandare **codice Luau al client**, in una sandbox senza file né rete, e
  il giocatore può rifiutarlo. Guida in [`docs/MODDING.md`](docs/MODDING.it.md).
- **Salvataggio.** SQLite: chunk, casse, borse, posizione dei giocatori e memoria dei mod. Si
  scrive solo quello che è cambiato, in una transazione sola, e il server salva quando lo si
  ferma con Ctrl+C. I blocchi sono salvati per nome, così aggiungere un mod non costringe mai a
  ricominciare il mondo.

Il piano completo e le sue decisioni sono in [`docs/DESIGN.md`](docs/DESIGN.it.md).

## Giocare e testare: riferimento

Dopo aver compilato ([sopra](#compilare-dal-sorgente)), dalla cartella del checkout:

```sh
ctest --test-dir build                       # 13 test, ~3 s; -LE gpu salta quello che apre una finestra

./build/src/client/cnp_client                # il menù del titolo
./build/src/client/cnp_client --quick        # avvio rapido: un mondo locale di prova, niente salvato

./build/src/server/cnp_server --listen 25301 --ticks 0 --world 128 --world-file mondo.db
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name giocatore

# L'arena shooter: un sandbox e l'arena su un server solo
tools/fps.sh --fresh

# L'arena da sola: il client non ha i mod, glieli manda il server
./build/src/server/cnp_server --listen 25302 --mods games/yafps/mods --textures games/yafps/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25302 --name giocatore

# L'action RPG: un sandbox e il mondo arpg su un server solo, dietro un arco di pietra
tools/arpg.sh --fresh

# La pista a tempo: con --portals si nasce sulla strada, davanti alla partenza
./build/src/server/cnp_server --listen 25303 --portals --mods games/track/mods --textures games/track/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25303 --name giocatore

# Un sandbox con due portali, verso l'arena e verso la pista, su tre server.
# Ognuno riporta all'apertura da cui si è entrati (/0, /1). Senza --ticks 0 un server si ferma dopo 50 s
./build/src/server/cnp_server --listen 25301 --ticks 0 --portals --portal-to 127.0.0.1:25302/0 --portal-to 127.0.0.1:25303/0
./build/src/server/cnp_server --listen 25302 --ticks 0 --portals --portal-to 127.0.0.1:25301/0 --mods games/yafps/mods --textures games/yafps/textures
./build/src/server/cnp_server --listen 25303 --ticks 0 --portals --portal-to 127.0.0.1:25301/1 --mods games/track/mods --textures games/track/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name giocatore

# Lo stesso su un server solo, a stanze: sandbox, pista e inferno (tools/track.sh lo fa, con tutti i mondi)
./build/src/server/cnp_server --listen 25301 --ticks 0 --portals \
    --room sandbox --portal-to @pista/0 --portal-to @inferno/0 \
    --room pista --mods games/track/mods --textures games/track/textures --portal-to @sandbox/0 --portal-to @inferno/2 --portal-to @inferno/1 \
    --room inferno --mods games/inferno/mods --textures games/inferno/textures --fallback-textures assets/textures --portal-to @sandbox/1 --portal-to @pista/2 --portal-to @pista/1
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name giocatore   # --room inferno per entrarci subito
```

### Comandi

| | |
|---|---|
| WASD, Spazio, F | muoversi, saltare, volare (col jetpack) |
| Tasto sinistro / destro | rompere / piazzare (a mani vuote si usa il blocco) |
| Rotellina o 1-9, E, C, Q | slot della hotbar, borsa, catalogo creativo, buttare |
| T, / | chat, chat con un comando (`/help`, `/list`) |
| V, F11, ESC, F1 | terza persona, schermo intero, menù di pausa, nascondere mani e hotbar |
| F3, F4, F8 | informazioni, confini dei territori, celle dove possono nascere i mostri |
| F5, F6, F7, F2 | ricaricare i mod, profiler, texture pack successivo, occlusion culling spento |

Le opzioni restano fra una partita e l'altra. `CNP_HITCH_MS=30 cnp_client ...` stampa ogni frame
più lungo di 30 ms con le sezioni del profiler.

### Il menù del titolo

**Gioca** avvia un mondo proprio su un server locale, con la chiave di
`~/.local/share/cubes-and-portals/player.key`; l'elenco mostra i mondi salvati, il bottone verde
in cima ne crea uno nuovo (nome e seme), e Modifica ne rinomina o cancella uno. Un mondo creato da
una versione precedente ha un bordo ambra e quella versione accanto al nome (il suo file
`version`, scritto una volta quando nasce; "versione vecchia" per uno di prima del file).
**Multigiocatore** tiene la lista dei server, ognuno con un tondo colorato (grigio non provato,
giallo in prova, verde con il ping, rosso se non ha risposto). L'account è del server: si sceglie
il server, poi un nome e una password per quello, da cui l'identità viene derivata (Argon2id)
invece di stare in un file; la chiave del server viene fissata al primo join. **Ospita un
server** apre i propri mondi agli altri su una porta a scelta, e il menù di pausa mostra
l'indirizzo da dare. **Personaggio** sceglie specie, stile, colori e accessori. **Opzioni** ha
mouse, volume, lingua e controllo degli aggiornamenti; la pagina **Video** ha finestra, senza bordi o schermo intero, risoluzione del gioco (indipendente dalla grandezza della finestra, in ogni modalità; l'immagine viene scalata per riempirla), VSync, limite FPS, distanza visiva, campo visivo, luce, ombre, dettaglio dei portali, cielo in tempesta semplice (senza vortici, per i PC deboli) e luminosità per il buio.

**Le traduzioni** sono `assets/lang/<codice>.txt`, una riga `testo come scritto = traduzione` con
`{}` per le parti variabili. Valgono per tutto ciò che il client disegna: menù, pannelli e avvisi
dei mod, chat e testi dell'HUD, e i nomi degli oggetti. Una riga assente lascia il testo com'è.
`python3 tools/lang_check.py` elenca ciò che mod e mondi mostrano e che `it.txt` (per i testi in
inglese) o `en.txt` (per quelli in italiano) non hanno; va lanciato dopo aver cambiato i testi di un mod.

### Client (`cnp_client`)

| Flag | Cosa fa |
|---|---|
| (nessuno) | Il menù del titolo (sopra) |
| `--quick` | Avvio rapido, senza menù: un mondo locale in un solo processo, che non salva |
| `--connect HOST` `--port N` `--name NOME` | Entra in un server invece di generare un mondo locale |
| `--livery N` | L'aspetto del giocatore, per un join con `--connect` |
| `--server-key HEX` | Accetta solo il server con questa chiave (la stampa il server all'avvio) |
| `--player-key FILE` | Dove sta l'identità del giocatore (default `player.key`, creata al primo avvio) |
| `--watch HOST:PORT` | Guarda dentro un altro mondo come osservatore (ripetibile) |
| `--world-code refuse` | Non esegue il codice che i mondi mandano al client |
| `--no-update-check` | Non chiede a GitHub se c'è una release più nuova (come Opzioni, Aggiornamenti: no, ma solo per questa partita) |
| `--media-cache DIR` `--no-media-cache` `--media-cache-size MB` | Cache dei media scaricati |
| `--textures DIR` (ripetibile) `--no-textures` | Texture pack; con più di uno F7 li alterna |
| `--models DIR` `--no-models` `--model FILE.gltf` | Dove cercare i modelli; un personaggio davanti allo spawn |
| `--mods DIR` `--seed N` `--world N` | Mondo locale: mod, seme, lato in blocchi |
| `--user-mods DIR` | Mondo locale: altri mod, caricati con `--mods` come un solo insieme |
| `--play-world NOME` | Dritto in quel mondo salvato, oltre la schermata del titolo (quello che avvia il launcher) |
| `--join-listed HOST:PORTA` | Quella riga dell'elenco server, raggiunta come con un clic (il launcher, seguendo un server) |
| `--day-length S` `--time T` | Durata del giorno in secondi, ora di partenza (0..1) |
| `--dual` `--facing` `--oriented` | Scene di prova: due mondi, due portali affacciati, un portale coricato |
| `--recursion N` `--portal-scale F` `--portal-config FILE` | Profondità della ricorsione (0-3), risoluzione dei target, forma del portale ([`config/portal.cfg`](config/portal.cfg)) |
| `--log-view` | Traccia della vista attorno a ogni attraversamento |
| `--bench` `--dual-bench` `--soak S` | Misure: FPS e costo dello scripting, costo del secondo mondo, volo automatico |
| `--verify` | Il test di continuità del portale (lo lancia `ctest`) |
| `--play-seconds S` `--watchdog S` | Esce dopo S secondi; scrive lo stack se un frame non arriva entro S secondi |
| `--shot S` | Uno screenshot in `bench/play.png` dopo S secondi di una corsa a tempo |
| `--fly-down N` `--walked-down` | In una corsa a tempo: sale N blocchi in volo e riscende, dicendo cosa costa l'atterraggio |

L'elenco completo è in `main()` di [`src/client/main.cpp`](src/client/main.cpp).

### Server (`cnp_server`)

| Flag | Cosa fa |
|---|---|
| `--listen PORT` | Accetta giocatori. Senza, genera il mondo, esegue `--ticks` tick e ne stampa l'hash |
| `--bind HOST` | L'indirizzo su cui ascoltare (default tutti); il menù del client usa 127.0.0.1 |
| `--world N` `--seed N` `--world-file FILE` | Lato della regione generata all'avvio (default 128; il mondo non ha bordo), seme (default 1), file SQLite dove salvarlo. Un solo server per file: il secondo trova il lucchetto ed esce |
| `--world-depth N` `--world-top N` | Solo alla creazione del mondo: fin dove arriva la roccia scavabile sotto y 0 (default 0, la vecchia roccia madre su y 0; fino a 1024, a passi di 32, sopra un fondo di roccia madre) e quanto è alto il mondo (default 64). Conservati accanto al file del mondo come `<file>.shape`; un mondo già fatto resta 0 e 64. Il nuovo mondo della schermata iniziale parte da 128 e 256 |
| `--save-every S` | Ogni quanto scrivere quello che è cambiato (default 10 s; 0 solo alla chiusura). Ctrl+C o SIGTERM salvano e chiudono |
| `--mods DIR` `--no-mods` | I mod del mondo |
| `--user-mods DIR` (ripetibile) `--no-mod NOME` (ripetibile) | Altri mod, quelli del giocatore, caricati con `--mods` come un solo insieme: un solo ordine per `depends`, e un nome già caricato rifiutato. Un mondo avviato dalla schermata del titolo passa `~/.cubes-and-portals/mods` (Windows: `%APPDATA%/cubes-and-portals/mods`) alla sua stanza sandbox, poi la cartella `mods/` del mondo, e `--no-mod` per ogni nome che il suo `mods.conf` spegne (`off = a, b`) |
| `--textures DIR` `--no-textures` | I media che il mondo serve |
| `--day-length S` `--time T` | Ciclo giorno/notte |
| `--key-file FILE` `--no-encryption` | L'identità del server; cifratura spenta (solo per prove) |
| `--max-players N` `--require-identity` `--no-observers` | Posti, giocatori obbligati ad avere una chiave, niente osservatori da altri mondi. Un nome è di chi lo ha usato per primo, e resta nel file del mondo |
| `--names` `--free-name NOME` | Con `--world-file` e nessun server sul file: chi possiede ogni nome, o libera un nome. Chi entra dopo con quel nome eredita borsa e posizione del vecchio proprietario |
| `--owner NOME` | Chi gioca come NOME con un'identità è il proprietario del server. Il client lo passa ai mondi che avvia |
| `--config FILE` | I flag da un file, un `chiave valore` (o `chiave = valore`) per riga, `[room NOME]` per una stanza, `#` per i commenti; `chiave = true` è il flag da solo. Letto dove sta, quindi i flag dopo vincono |
| `--rcon PORTA` `--rcon-password PW` | Comandi admin via TCP solo su 127.0.0.1: `tools/admin.sh -p PORTA ban bob 2h`. La password va nel file di config, non sulla riga di comando |
| `--motd TESTO` | Il messaggio del giorno, detto in chat a ogni giocatore che entra (anche `/motd`) |
| `--restart-at HH:MM` | Un riavvio ogni giorno, con dieci minuti di avvisi; sotto `tools/serve.sh`, che lo fa ripartire |
| `--backup-every 6h` `--backup-keep N` | Copie di ogni file del mondo accanto a lui (`NOME.backup-DATA`), fatte a server acceso; restano le N più recenti (5). `/backup` ne fa una subito |
| `--max-per-address N` | Al massimo N giocatori dallo stesso indirizzo |
| `--watchdog S` | Una stanza che non finisce un tick in S secondi fa uscire il server con lo stack stampato, perché `tools/serve.sh` lo riavvii |
| `--federation PORTA` | Server amici (`/friend`): righe firmate e cifrate su questa porta TCP, con la chiave dei biglietti (`--ticket-key`, o MONDO.sign) |
| `--rules-file FILE` `--no-update-check` | Regole che ogni giocatore accetta prima di giocare; niente domanda a GitHub su una release più nuova ([`docs/ADMIN.md`](docs/ADMIN.it.md)) |
| `--webhook URL` | Azioni admin, avvio, arresto e segnalazioni mandati lì in POST come JSON ([`docs/ADMIN.md`](docs/ADMIN.it.md#webhook)) |
| `--report FILE` `--since 7d` `--json` | Senza server: cosa ricorda un file del mondo -- giocatori, tempo di gioco, massimo insieme, ingressi per ora, stanze, chi è stato respinto, azioni dello staff, riavvii e crash, attraversamenti da altri server |
| `--portals` `--portal-to HOST:PORT[/N]` | Una coppia di aperture gestita dal server; `--portal-to` dice dove porta la prima (e da quale apertura `N` si esce); ripetuto, la seconda, la terza e la quarta |
| `--ticket-key FILE` `--portal-peer-key HEX` `--trust HEX` `--require-ticket` | Ticket firmati per attraversare fra server |
| `--mobs N` `--ticks N` `--quiet` | Mob di prova, tick prima di uscire, meno log |
| `--room NOME` | Una stanza: un mondo in più sullo stesso server e sulla stessa porta. Quello che segue vale solo per quella stanza (`--mods`, `--world-file`, `--portal-to`...), quello prima del primo `--room` per tutte. `--portal-to @NOME[/N]` porta in un'altra stanza; borsa e salute sono le stesse in tutte. Il client sceglie la stanza con `--room NOME` |

### Amministrazione del server

Si scrive in chat da un giocatore il cui ruolo lo permette, sullo stdin del server senza la
barra, via RCON (`tools/admin.sh`), o con i pulsanti della pagina `/admin`. Ruoli (guest,
player, helper, moderator, admin, owner, anche per stanza), ban, mute, whitelist,
avvertimenti, segnalazioni e note, regole da accettare, account doppi, riavvii con avvisi e manutenzione, messaggio del giorno,
visite e `--report`, `/inspect` e `/rollback` di quello che un giocatore ha cambiato, backup,
webhook, `/bag` e `/give`, e server amici che condividono ban, presenza, una chat, un solo
zaino per ogni giocatore e i portali costruiti fra loro (una cornice d'oro che un admin imposta dal suo menù).

**Ogni comando, permesso, flag e chiave di `server.conf` è in [`docs/ADMIN.md`](docs/ADMIN.it.md);
ogni comando della chat, anche quelli dei giocatori, con cosa fa, in [`docs/COMMANDS.it.md`](docs/COMMANDS.it.md).**
`/help` in gioco elenca quelli che puoi usare.

`tools/serve.sh -l logs build-release/src/server/cnp_server --config server.conf` tiene un
server acceso tra riavvii e crash, con un log al giorno.

### Strumenti

| | |
|---|---|
| `tools/play.sh` (`local`, `--fresh`) | Compila in Release, avvia server e client con tutti i mod e un mondo salvato in `worlds/play/` |
| `tools/track.sh` (`--fresh`) | Un server con il sandbox e tutti i mondi come stanze, come lo avvia il menù del titolo ma con tutti i mondi sbloccati (una copia di `mods/keys` con `ALL_OPEN`; mondi e log in `worlds/track/`); `--fresh` ti fa anche suo proprietario, così il catalogo creativo (C) è aperto |
| `tools/arpg.sh`, `tools/fps.sh`, `tools/chambers.sh` (`--fresh`) | Il sandbox e un mondo solo, per lavorare su quel mondo |
| `tools/portallab.sh` | Una scena per controllare i portali a occhio (games/portallab): vetro visto attraverso le aperture, una a soffitto in cui saltare, due pavimenti tra cui rimbalzare, aperture che scorrono, inseguono e girano su ogni asse; `/lab wall`, `glass`, `head`, `bounce`, `view`, `slide`, `chase`, `spin`, `spiny`, `spinz`, `still`, `at <gradi>`; un'apertura grande come una porta su una alta un ottavo che ti rimpicciolisce a 1/16, `/lab tiny`, `g1`, `scale <s>` |
| `tools/windowlab.sh` | I corpi visti da una finestra su un altro mondo (games/windowlab): due stanze, sei creature che sfilano davanti alla bocca lontana. |
| `tools/vehiclelab.sh` | I veicoli attraverso i portali (games/vehiclelab, sopra le mod del sandbox): una linea di binari in un'apertura e fuori da un'altra girata di un quarto, un canale per la barca uguale; un portale portato da un treno in curva; `NAME=boat` o `train` parte lì, `/vlab rail`, `boat`, `train`, `watch`; un client a tempo vuole `--keep-portals` |
| `build/tools/cnp_modpack pack mods/ore` | Un mod in un file `.cnpmod`, che si carica da `mods/` come una directory |
| `tools/dist/package-*.sh` | I pacchetti per Linux, Windows e macOS ([`docs/BUILDING.md`](docs/BUILDING.it.md)) |
| `build/tools/cnp_netshape` | Relay UDP con ritardo e perdita, per provare link lenti |
| `build/tools/libcnp_clock_skew.so` | `LD_PRELOAD` che sposta l'orologio di un processo, per i ticket |
| `tools/admin.sh` (`-p PORTA`) | Un comando admin a un server avviato, via RCON; la password in `CNP_RCON_PASSWORD` |
| `tools/serve.sh` (`-l DIR` `-k GIORNI` `-w URL`) | Tiene acceso un server tra riavvii e crash, un log al giorno; i crash mandati in POST all'URL |
| `tools/two_machines.sh` | Due server in container, con MTU e NAT veri |
| `build/tools/cnp_bake_textures` | Rigenera le texture segnaposto dei blocchi |

### Test

Tredici, scelti perché proteggono cose che si rompono in silenzio e costano care: l'input dalla
rete (`protocol`, `chunk_codec`, `crypto`), i mondi salvati (`storage`, `migration`,
`persistence`), la matematica dei portali e le collisioni (`portal_math`, `portal_traverse`,
`physics_box`, `portal_continuity` in GPU), la sandbox dei mod (`script_isolation`), e client e
server d'accordo sul mondo (`worldgen`, `two_process_join`). Il resto si verifica giocando. Il
perché di ciascuno è in [`tests/CMakeLists.txt`](tests/CMakeLists.txt).

## Tecnologia

| | |
|---|---|
| Grafica | raylib 6.0 dietro un'interfaccia `gfx_*` sottile, OpenGL 3.3 core |
| Linguaggi | C++20 per l'engine, Luau 0.733 per lo scripting |
| Rete | ENet 1.3.18, zstd 1.5.7, Monocypher 4.0.3 |
| Fisica | Jolt (mezzi) e quella dell'engine (corpi, blocchi, portali) |
| Salvataggio | SQLite 3.50.4 |
| ECS | Flecs 4.1.6 |

## Documentazione

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.it.md): come è fatto il codice oggi e dove si mette una
  feature nuova.
- [`docs/DESIGN.md`](docs/DESIGN.it.md): analisi di fattibilità, decisioni, piano a milestone,
  rischi.
- [`docs/MODDING.md`](docs/MODDING.it.md): scrivere un mod; [`docs/API.md`](docs/API.md): l'API
  Luau, generata da `src/script/api.luau`.
- [`docs/TEXTURE_PACKS.md`](docs/TEXTURE_PACKS.it.md): fare un texture pack.
- [`docs/BUILDING.md`](docs/BUILDING.it.md): compilare, pacchetti, versioni e il repository Flatpak.
- [`TODO.md`](TODO.md) (in inglese): cosa manca e cosa non va.
- [`docs/archive/`](docs/archive/README.md): lo storico, milestone per milestone, con le misure.

## Licenza

Il codice è sotto licenza MIT ([`LICENSE`](LICENSE)). Le librerie in `third_party/` hanno
ciascuna la propria licenza, nella loro cartella.
