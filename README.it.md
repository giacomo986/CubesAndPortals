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
[`TODO.md`](TODO.md), cosa cambia a ogni versione in [`CHANGELOG.md`](CHANGELOG.md).

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

- **Linux**: si scompatta il `.tar.gz` dove si vuole e si avvia `./cnp_client`; oppure si rende
  eseguibile l'`.AppImage` (`chmod +x`) e lo si avvia; oppure si installa il `.flatpak` con
  `flatpak install --user <file>.flatpak`, e il gioco compare nel menù del desktop.
- **Windows**: si scompatta lo `.zip` e si fa doppio clic su `cnp_client.exe`.
- **macOS**: si apre il `.dmg` e si trascina l'app dove si vuole.

I pacchetti non sono firmati. Al primo avvio Windows SmartScreen chiede conferma ("Ulteriori
informazioni", poi "Esegui comunque"); su macOS si fa clic destro sull'app e Apri.

**Aggiornamenti.** Il gioco controlla da solo se c'è una versione nuova: all'apertura del menù
chiede a GitHub l'ultima release di questo repository (manda solo quella richiesta, nulla di chi
gioca). Se ce n'è una più recente compare il bottone "Nuova versione …: scarica", che apre la
pagina Releases. Si spegne in Opzioni, Aggiornamenti, o per una sola partita con
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
| `build/src/client/cnp_client` | il gioco: menù, mondi salvati, multiplayer |
| `build/src/server/cnp_server` | un mondo per altri giocatori, senza finestra |
| `build/tools/cnp_modpack` | impacchetta un mod in un file `.cnpmod` |

Un build di sviluppo legge `mods/`, `games/` e `assets/` dal checkout, quindi va avviato da lì.
I test si lanciano con `ctest --test-dir build` (13 test, pochi secondi).

I pacchetti per chi gioca si costruiscono con gli script di `tools/dist/` e finiscono in
`<cartella di build>/dist/`. Il pacchetto Windows si compila **da Linux** con zig, quello macOS
**su un Mac**. Tutti i dettagli (opzioni di CMake, pacchetti, versioni) sono in
[`docs/BUILDING.md`](docs/BUILDING.md).

## Contribuire

Il progetto è aperto a tutti, e si può aiutare in molti modi:

- **giocare e segnalare** bug, crash e cose confuse, aprendo una issue: cosa è successo, cosa ci
  si aspettava, sistema operativo e versione del gioco (in basso a destra nel menù), e se
  possibile il log;
- **tradurre**: una lingua nuova è un file nuovo in `assets/lang/` (vedi `it.txt`);
- **fare mod, mondi e texture pack**: [`docs/MODDING.md`](docs/MODDING.md),
  [`docs/API.md`](docs/API.md) e [`docs/TEXTURE_PACKS.md`](docs/TEXTURE_PACKS.md); i sorgenti
  Blender dei modelli sono in [`assets/blender/`](assets/blender/README.md);
- **lavorare sull'engine**: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) spiega come è fatto il
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
  [`games/backrooms/README.md`](games/backrooms/README.md).
- **Il parkour** (`games/parkour`): corsa sui muri, rimbalzi, scivolate, capriole e rope gun.
  Guida in [`games/parkour/README.md`](games/parkour/README.md).

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
  squadre, due basi e tre punti da prendere, rossi contro blu, con bot e mezzi.
- **L'assalto** (`games/assault`): l'assalto finale alla dimensione di Basoalth, da giocare
  insieme: quattro fortezze, un negozio, alleati e costruttori, un boss che diventa un gigante e
  la fuga in jeep in un tunnel di lava.

### Il sandbox

**Il territorio.** Un mondo senza bordo: pianure, deserti, tundra e montagne larghi qualche
centinaio di blocchi, oceani e mari bassi con le spiagge, fiumi che arrivano al mare, laghi in
quota, gole con un torrente sul fondo, grotte collegate fra loro, minerali in vene dentro la
pietra, e il ciclo giorno/notte con nuvole sopra cui si vola. Sotto terra ci sono miniere
abbandonate e cripte con le casse, e una scala per risalire.

**Villaggi** (`mods/villages`, `mods/villagers`). Paesi e città con piazza, vie, case, campi,
fucina e lampioni, collegati da sentieri che passano i fiumi sui ponti. Li abitano tre specie:
umani contadini, rospi pescatori e talpe raccoglitrici, ognuno con una casa, un mestiere, i pasti
e un letto. Ogni villaggio ha:

- una ronda (guardie, arcieri e un ispettore che guarda nelle borse in cerca di refurtiva: il
  furto viene marcato, e un ladro è perdonato una volta, poi braccato);
- un sindaco che vende lotti e case, e un costruttore che le alza blocco per blocco con i
  materiali portati dai taglialegna, dai minatori e dagli scavatori del villaggio;
- un mercante e dei lavoratori che commerciano a smeraldi, con prezzi che seguono l'offerta, e
  una cassa del villaggio;
- una miniera, un artigiano che rifà gli attrezzi, e una casa per ogni ruolo.

**Territori** (`mods/territory`): una pietra di confine fonda un territorio che cresce mentre i
proprietari ci stanno dentro. Lì dentro i proprietari sono più veloci e resistenti, gli estranei
più deboli; quello che gli estranei prendono lì è marcato rubato.

**Meteo** (`mods/weather`). Cielo sereno, poi pioggia o temporale per qualche minuto: il cielo
e le nuvole si fanno grigi, la pioggia cade attorno a te e si ferma sui tetti, i campi a cielo
aperto vengono innaffiati. Nella tundra e sulle vette nevica e la neve si posa; nel deserto
non cade niente. Un temporale colpisce il suolo vicino ai giocatori, con il tuono. Stagioni di
cinque giorni: d'autunno le foglie si fanno arancioni e d'inverno spente (l'abete resta verde),
l'estate ha più temporali e la primavera più pioggia, i campi rallentano d'autunno e d'inverno
crescono solo sotto le lampade, e d'inverno nevica ovunque tranne nel deserto. I mercanti fanno pagare il doppio il cibo d'inverno e la legna
d'autunno.

**Creature.** Zombie e scheletri arcieri escono al buio e bruciano al sole; maiali, mucche, pecore
e galline pascolano, si riproducono e crescono; un cane e un gatto si addomesticano e seguono chi
li ha presi (`mods/mobs`, `mods/animals`, `mods/pets`).

**Sopravvivenza e costruzione.** Borsa e hotbar, craft, casse (due affiancate fanno una doppia),
una fornace con le righe per materiale, combustibile e prodotto, attrezzi in cinque materiali che
si consumano, armature in quattro, fame, esperienza e tavolo da incantamento, letti che fanno
passare la notte, campi (zappa, semi, grano, pane, acqua e luce), pesca, canna da zucchero, carta
e libri, porte, scale a pioli e botole.

**Macchine.** Circuiti di cinnabar (`mods/cinnabar`): polvere, leve, pulsanti, torce, ripetitori,
comparatori, lampade, pistoni, observer e hopper, così orologi e logica funzionano. Nastri
trasportatori (`mods/conveyor`) che si fondono, curvano e salgono, con selezionatori ed
estrattori. Robot aiutanti (`mods/helpers`) da programmare con fino a sei ordini: abbattere
alberi, piantare, scavare, cercare minerali, ordinare le casse, cucinare, cavare, illuminare il
terreno, seguire chi li ha messi.

**Giochi sulla piazza.** Scacchi (`mods/chess`) con tutte le regole classiche, l'orologio e nove
varianti (960, re della collina, tre scacchi, corsa dei re, vinciperdi, atomica, crazyhouse,
orda). Carte (`mods/cards`) col mazzo napoletano da quaranta: briscola, cinquiglio, scopa e
tresette, da due a cinque giocatori, con il tavolo che gioca i posti vuoti.

**Personaggi ed equipaggiamento.** Si sceglie robot, umano, uomo talpa o uomo rana, uno stile,
tre colori e due accessori, visti da tutti. Sulla schiena: un jetpack per volare, uno zaino di
cuoio con 27 slot suoi, o uno zaino di metallo con due braccia snodate che tengono uno strumento
ciascuna. Il catalogo creativo (C) ha una casella di ricerca.

### L'engine

- **Voxel.** Chunk da 32³, greedy meshing su thread, vertici da 12 byte, luce del sole e dei
  blocchi con flood fill, ambient occlusion, frustum e occlusion culling, profiler a schermo
  (F6).
- **Luce.** Buio vero: una grotta senza torce è nera, e la notte quasi. Sole e luna fanno ombra
  (anche le foglie); le torce illuminano le facce rivolte verso di loro. La luce di prima c'è
  ancora: Opzioni, Luce: classica.
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
  il giocatore può rifiutarlo. Guida in [`docs/MODDING.md`](docs/MODDING.md).
- **Salvataggio.** SQLite: chunk, casse, borse, posizione dei giocatori e memoria dei mod. Si
  scrive solo quello che è cambiato, in una transazione sola, e il server salva quando lo si
  ferma con Ctrl+C. I blocchi sono salvati per nome, così aggiungere un mod non costringe mai a
  ricominciare il mondo.

Il piano completo e le sue decisioni sono in [`docs/DESIGN.md`](docs/DESIGN.md).

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
| V, F11, ESC | terza persona, schermo intero, menù di pausa |
| F3, F4, F8 | informazioni, confini dei territori, celle dove possono nascere i mostri |
| F5, F6, F7, F2 | ricaricare i mod, profiler, texture pack successivo, occlusion culling spento |

Le opzioni restano fra una partita e l'altra. `CNP_HITCH_MS=30 cnp_client ...` stampa ogni frame
più lungo di 30 ms con le sezioni del profiler.

### Il menù del titolo

**Gioca** avvia un mondo proprio su un server locale, con la chiave di
`~/.local/share/cubes-and-portals/player.key`; l'elenco mostra i mondi salvati, il bottone verde
in cima ne crea uno nuovo (nome e seme), e Modifica ne rinomina o cancella uno.
**Multigiocatore** tiene la lista dei server, ognuno con un tondo colorato (grigio non provato,
giallo in prova, verde con il ping, rosso se non ha risposto). L'account è del server: si sceglie
il server, poi un nome e una password per quello, da cui l'identità viene derivata (Argon2id)
invece di stare in un file; la chiave del server viene fissata al primo join. **Ospita un
server** apre i propri mondi agli altri su una porta a scelta, e il menù di pausa mostra
l'indirizzo da dare. **Personaggio** sceglie specie, stile, colori e accessori. **Opzioni** ha
mouse, volume, luce, ombre, schermo intero, lingua e controllo degli aggiornamenti.

**Le traduzioni** sono `assets/lang/<codice>.txt`, una riga `testo come scritto = traduzione` con
`{}` per le parti variabili. Valgono per tutto ciò che il client disegna: menù, pannelli e avvisi
dei mod, chat e testi dell'HUD. Una riga assente lascia il testo com'è.

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
| `--save-every S` | Ogni quanto scrivere quello che è cambiato (default 10 s; 0 solo alla chiusura). Ctrl+C o SIGTERM salvano e chiudono |
| `--mods DIR` `--no-mods` | I mod del mondo |
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
| `--rules-file FILE` `--no-update-check` | Regole che ogni giocatore accetta prima di giocare; niente domanda a GitHub su una release più nuova ([`docs/ADMIN.md`](docs/ADMIN.md)) |
| `--webhook URL` | Azioni admin, avvio, arresto e segnalazioni mandati lì in POST come JSON ([`docs/ADMIN.md`](docs/ADMIN.md#webhooks)) |
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
webhook, `/bag` e `/give`, e server amici che condividono ban, presenza, una chat e un solo
zaino per ogni giocatore.

**Ogni comando, permesso, flag e chiave di `server.conf` è in [`docs/ADMIN.md`](docs/ADMIN.md)** (in inglese).
`/help` in gioco elenca quelli che puoi usare.

`tools/serve.sh -l logs build-release/src/server/cnp_server --config server.conf` tiene un
server acceso tra riavvii e crash, con un log al giorno.

### Strumenti

| | |
|---|---|
| `tools/play.sh` (`local`, `--fresh`) | Compila in Release, avvia server e client con tutti i mod e un mondo salvato in `worlds/play/` |
| `tools/track.sh` (`--fresh`) | Un server con il sandbox e tutti i mondi come stanze, come lo avvia il menù del titolo (mondi e log in `worlds/track/`) |
| `tools/arpg.sh`, `tools/fps.sh`, `tools/chambers.sh` (`--fresh`) | Il sandbox e un mondo solo, per lavorare su quel mondo |
| `build/tools/cnp_modpack pack mods/ore` | Un mod in un file `.cnpmod`, che si carica da `mods/` come una directory |
| `tools/dist/package-*.sh` | I pacchetti per Linux, Windows e macOS ([`docs/BUILDING.md`](docs/BUILDING.md)) |
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

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): come è fatto il codice oggi e dove si mette una
  feature nuova.
- [`docs/DESIGN.md`](docs/DESIGN.md): analisi di fattibilità, decisioni, piano a milestone,
  rischi.
- [`docs/MODDING.md`](docs/MODDING.md): scrivere un mod; [`docs/API.md`](docs/API.md): l'API
  Luau, generata da `src/script/api.luau`.
- [`docs/TEXTURE_PACKS.md`](docs/TEXTURE_PACKS.md): fare un texture pack.
- [`docs/BUILDING.md`](docs/BUILDING.md): compilare, pacchetti e versioni.
- [`TODO.md`](TODO.md) (in inglese): cosa manca e cosa non va.
- [`docs/archive/`](docs/archive/README.md): lo storico, milestone per milestone, con le misure.

## Licenza

Il codice è sotto licenza MIT ([`LICENSE`](LICENSE)). Le librerie in `third_party/` hanno
ciascuna la propria licenza, nella loro cartella.
