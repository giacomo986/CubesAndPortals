# Cubes and Portals

Un sandbox voxel scritto da zero, con i **portali come primitiva dell'engine**: un portale
mostra un altro mondo, anche su un altro server, e lo si attraversa senza schermata di carico.
Dalla piazza dello spawn i portali portano a mondi che sono altri giochi: una pista di kart, un
FPS in arena, un action RPG isometrico, camere con i puzzle a portali, le backrooms, il
parkour, un assalto da giocare in compagnia. Si gioca da soli o in multiplayer, e ogni mondo è
fatto di mod in Luau che chiunque può leggere e cambiare.

È un **progetto collaborativo e aperto ai contributi**: codice, mondi, mod, texture pack,
traduzioni, segnalazioni di bug e partite di prova sono tutti benvenuti (vedi
[Contribuire](#contribuire)).

> **In English.** A voxel sandbox written from scratch, where portals are an engine primitive
> and lead to worlds that are whole other games. Open to contributions; most of the
> documentation is in Italian, while code, comments, `TODO.md` and commit messages are in
> English. Issues and pull requests in either language are fine.

## Stato

**Alpha giocabile.** C'è il sandbox (mondo senza bordo, biomi, grotte, villaggi con abitanti,
economia, animali e creature, circuiti, scacchi e carte), ci sono i sette mondi dietro i
portali della piazza, il laboratorio finale e il multiplayer. Molte cose sono incomplete o
grezze: cosa manca e cosa non va è in [`TODO.md`](TODO.md), le novità di ogni versione in
[`CHANGELOG.md`](CHANGELOG.md).

Finché il gioco è in alpha una versione nuova può non leggere i mondi salvati con la
precedente, né giocare con i suoi server: quando succede il CHANGELOG lo dice.

## Piattaforme

| Sistema | Pacchetti | Stato |
|---|---|---|
| **Linux** x86_64 | `.tar.gz`, `.AppImage`, `.flatpak` | La piattaforma principale: il gioco si sviluppa e si prova qui. Il tarball vuole glibc 2.38 o più recente (Ubuntu 24.04, Fedora 39 e successive) |
| **Windows** 10 o più recente, x86_64 | `.zip` | Funziona, ma è stato provato soprattutto con wine. Lacune note: niente stack dump in caso di crash, un client che va in crash chiude il suo server senza salvare |
| **macOS** 11 o più recente (Apple silicon) | `.dmg` | Il pacchetto si costruisce, ma **non è mai stato provato su un Mac** |

Il client vuole una scheda video con **OpenGL 3.3**; su Linux gira su X11 o su Wayland
attraverso XWayland. Il server (`cnp_server`) non ha finestra e gira ovunque giri il resto.
Segnalazioni da Windows e da macOS veri sono particolarmente utili.

## Scaricare il gioco

I pacchetti già compilati sono nella pagina **[Releases](../../releases)** del repository:
a ogni versione (un tag `v*`) GitHub Actions li costruisce per tutti e tre i sistemi e li
pubblica come pre-release. Se la pagina è ancora vuota, il gioco si compila dal sorgente
(sotto).

- **Linux**: il `.tar.gz` si scompatta dove si vuole e si avvia `./cnp_client`; l'`.AppImage`
  è un file solo da rendere eseguibile (`chmod +x`) e avviare; il `.flatpak` si installa con
  `flatpak install --user <file>.flatpak` e compare nel menù del desktop.
- **Windows**: si scompatta lo `.zip` e si fa doppio clic su `cnp_client.exe`.
- **macOS**: si apre il `.dmg` e si trascina l'app dove si vuole.

I pacchetti non sono firmati: al primo avvio Windows SmartScreen chiede conferma ("Ulteriori
informazioni", poi "Esegui comunque"), e su macOS va fatto clic destro e Apri.

Mondi salvati e opzioni stanno in `~/.local/share/cubes-and-portals` e
`~/.config/cubes-and-portals` su Linux, in `~/.var/app/io.github.giacomoperin.CubesAndPortals`
con il Flatpak, in `%APPDATA%\cubes-and-portals` su Windows.

## Compilare dal sorgente

Tutte le librerie (raylib, Luau, ENet, zstd, SQLite, Monocypher, flecs, Jolt) sono in
`third_party/`, come submodule, e si compilano insieme al gioco. Servono **CMake 3.25+** e un
compilatore C++20 (**GCC 13** o Clang). Su Linux (Ubuntu/Debian):

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
`<cartella-di-build>/dist/`. Il pacchetto Windows si compila **da Linux** con zig, quello macOS
**su un Mac**. Tutti i dettagli (opzioni di CMake, pacchetti, versioni) sono in
[`docs/BUILDING.md`](docs/BUILDING.md).

## Contribuire

Il progetto è aperto a tutti. Si può contribuire in molti modi:

- **giocare e segnalare** bug, crash e cose confuse, aprendo una issue: cosa è successo, cosa ci
  si aspettava, sistema operativo e versione (in basso a destra nel menù), e se possibile il log;
- **tradurre**: una lingua nuova è un file nuovo in `assets/lang/` (vedi `it.txt`);
- **fare mod, mondi e texture pack**: [`docs/MODDING.md`](docs/MODDING.md),
  [`docs/API.md`](docs/API.md) e [`docs/TEXTURE_PACKS.md`](docs/TEXTURE_PACKS.md); i sorgenti
  Blender dei modelli sono in [`assets/blender/`](assets/blender/README.md);
- **sistemare l'engine**: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) spiega come è fatto il
  codice e dove va una feature nuova; [`TODO.md`](TODO.md) elenca quello che manca.

Come si propone una modifica:

1. per una modifica grande (un mondo nuovo, un cambio al protocollo o al formato dei salvataggi)
   conviene aprire prima una issue e parlarne;
2. una pull request fa **una cosa sola**, con una descrizione che dice cosa cambia, perché, e come
   la si è provata (in gioco e/o con `ctest`);
3. il codice compila senza warning nuovi e i test passano;
4. il codice nuovo somiglia a quello attorno (nomi, commenti, stile), e la documentazione che
   descrive la parte toccata viene aggiornata insieme;
5. texture, modelli e suoni sono propri o con una licenza compatibile, dichiarata in un
   `CREDITS.md` nella cartella del mod (come [`games/track/CREDITS.md`](games/track/CREDITS.md)).
   Niente nomi, marchi o materiale di giochi commerciali.

**Ogni contributo è accettato, purché non sia malevolo o disordinato.**

- *Malevolo*: malware, backdoor, raccolta di dati di chi gioca, codice che nasconde quello che fa,
  materiale copiato senza averne il diritto, contenuti offensivi. Una PR così viene chiusa e chi
  l'ha aperta non ne apre altre.
- *Disordinato*: PR enormi che mescolano cose diverse, riformattazioni di massa, codice morto,
  modifiche senza descrizione, build rotto, codice generato e mai riletto. Una PR così non viene
  rifiutata per principio: si chiede di dividerla o sistemarla.

Chi contribuisce accetta che il suo contributo sia distribuito con la stessa licenza del
progetto (MIT).

## Uso dell'intelligenza artificiale

Questo progetto è sviluppato con un **uso esteso dell'AI**, e lo dice apertamente:

- **Codice, documentazione e messaggi di commit** sono scritti in gran parte da Claude
  (Anthropic), attraverso Claude Code, sotto la direzione di chi mantiene il progetto, che
  decide cosa fare, prova il gioco e approva ogni modifica. I commit scritti così lo dichiarano
  con una riga `Co-Authored-By: Claude`.
- **Modelli 3D, texture e quasi tutti i suoni** sono generati da script Python (e Blender) in
  `tools/`, scritti anch'essi con l'AI: nessun asset scaricato da altri giochi.
- **Le illustrazioni del mazzo di carte** e alcuni suoni sono generati con ElevenLabs.

I contributi fatti con l'aiuto dell'AI sono benvenuti alle stesse condizioni degli altri: chi
apre la pull request ne risponde, l'ha letta, capita e provata, e lo scrive nella descrizione.
L'output di un modello incollato senza verificarlo rientra nel "disordinato".

## Il gioco

### I mondi

Dalla piazza dello spawn sette portali (più l'ottavo, del laboratorio), ognuno in una struttura a tema (`mods/gates`), portano ad
altrettante stanze dello stesso server (`tools/track.sh`, o un mondo dal menu del titolo):

- **La pista** (`games/track`): kart a corpo rigido (Jolt) con sospensioni e nitro, il server che
  guida una copia del kart con i tasti del client; cinque piste a tempo con classifica, una che
  passa per l'Inferno vero e una con cinque giri della morte (due alti il doppio, 17 nodi); pezzi
  di pista come nei giochi di corse acrobatiche (giri della morte bassi, alti e larghi, e adattatori
  da larga a stretta compresi) e un editor per costruirne di nuove: con l'attrezzo editor un
  pannello propone i pezzi che si attaccano al capo aperto (un click li mette giù), annulla
  l'ultimo e chiude il circuito da solo (prima in piano, con gli adattatori se le larghezze
  cambiano). Con B (editor in mano) si sceglie da lontano: il corpo resta fermo, la telecamera
  guarda il capo aperto e il pezzo in prova è già giù; A/D lo cambiano, click sinistro lo tiene,
  S annulla, F chiude il circuito, B esce. In mano e per terra un pezzo è la sua miniatura. Lungo ogni pista file di
  scatole con i power-up dei giochi di kart: turbo, banane, gusci verdi e rossi, stella,
  fulmine e inchiostro (click sinistro per usarli). Il nitro sputa fiamme dagli scarichi; con CTRL
  il kart derapa (il retro perde aderenza e scivola in curva) e le gomme lasciano sgommate sulla
  pista, visibili a tutti; con `/garage` si sceglie il
  proprio kart: tre colori (carrozzeria, finiture, cerchi) e ruote, muso, pance, scarichi e alettone
  fra 14 pezzi, con il kart scelto che gira nell'anteprima a destra; sulle piste costruite a mano le scatole si
  piazzano con il loro attrezzo (`/pezzi`). C apre l'inventario creativo, e il jetpack (preso da lì e
  indossato sulla schiena) fa volare sopra le piste.
- **L'Inferno** (`games/inferno`): caverne di scoria, lava che scorre, fortezze, brute, wraith
  e cinder; ci si arriva anche da un portale di ossidiana costruito a mano.
- **La base** (`games/yafps`): FPS in stile arena shooter moderno, cinque armi, ondate di demoni.
- **La landa** (`games/arpg`): punta e clicca in stile action RPG isometrico: un paese con mercanti
  e forziere, tre zone di aree e corridoi con incontri che non rinascono, waypoint, portale cittadino
  (B), tre classi con quattro abilità, punti statistica e abilità a ogni livello, oggetti normali,
  magici, rari, unici e di set, che si vedono addosso (armi, scudi, elmi, armature, guanti,
  stivali, cinture, amuleti) con le loro animazioni; magie e colpi disegnati con shader di effetti;
  botti, casse e urne da rompere per quello che contengono. I apre l'inventario, C il personaggio, K le abilità.
- **Le camere di prova** (`games/chambers`): sei camere in stile puzzle a portali, portal gun progressiva,
  cubi, pulsanti, pedane di lancio. Un portale ancora senza l'altro capo è chiuso da una
  membrana animata: blu o arancione per la portal gun, verde per i portali fissi.
- **Le backrooms** (`games/backrooms`): tre livelli infiniti, scale, buchi, piscine, scivoli in cui
  si scende seduti e due creature; casse rare con la bussola dell'entrata e una torcia che stordisce,
  la bussola dell'uscita fatta con quello che lasciano le creature; guida in
  `games/backrooms/README.md`.
- **Il parkour** (`games/parkour`): corsa sui muri, rimbalzi, scivolate, capriole, rope gun;
  guida in `games/parkour/README.md`.

**Le sette chiavi** (`mods/keys`, la fine del gioco, in costruzione). L'ingegnere che ha
costruito i portali ha chiuso l'Ospite nel suo laboratorio e ha diviso la serratura in sette
chiavi, una in ogni mondo, dietro una prova. L'ottavo portale della piazza (62.5 21 76.5), una
cornice d'acciaio scuro con sette piedistalli, porta al **laboratorio** (`games/lab`) e respinge
chi non ha mai tenuto tutte e sette le chiavi; un clic su un piedistallo dice quali mancano,
`/diario` legge la pagina del diario di ogni chiave che si porta. Ogni mondo ha la sua prova (in
`TODO.md`). Nel laboratorio c'è l'Ospite, da battere con un po' di ogni mondo: portal gun, fucile,
salita alla galleria col parkour, valvole sotto la lava, torcia nel buio, gelo, e poi la fuga in
kart in un tunnel e i titoli di coda; dopo, il sandbox resta libero. Ogni mondo si lascia da un'uscita in fondo alla
sua prova, non da dove si entra, e si esce dall'arco di quel mondo, in fila sul lato nord della
piazza: ogni arco è un portale vero sull'uscita del suo mondo, e ci si può entrare anche dalla
piazza. Uscita e arco sono chiusi da una membrana rossa per chi non ha vinto quel mondo (l'uscita
finché la prova non è superata, l'arco finché il sandbox non ha visto la sua chiave nella borsa), e
si aprono solo per lui. La chiave di ogni prova non arriva da sola: aspetta su
un piedistallo dorato accanto all'uscita, e si prende avvicinandosi.

Il racconto completo di come M13 è arrivata qui è in
[`docs/archive/README-M13-2026-10-09.md`](docs/archive/README-M13-2026-10-09.md).

Il piano completo e le decisioni sono in [`docs/DESIGN.md`](docs/DESIGN.md). Quello che manca o
non va è in [`TODO.md`](TODO.md).

### Cosa c'è

**Motore voxel.** Chunk da 32³, greedy meshing su thread, vertice da 12 byte, luce del sole e dei
blocchi con flood fill, ambient occlusion, frustum e occlusion culling, profiler a schermo (F6).
Biomi, grotte collegate fra loro, minerali, alberi, laghi e torri diroccate generati dai mod,
ciclo giorno/notte.

**Luce.** Buio vero: una grotta senza torce è nera (2%), la notte è quasi nera con la luna.
Il sole e la luna sono una luce direzionale con ombre (shadow map 2048 attorno all'occhio,
anche le foglie fanno ombra; Opzioni → Shadows: high/low/off). Torce e blocchi luminosi
illuminano le facce rivolte verso di loro: la direzione si ricava dalla pendenza del flood fill,
senza dati in più sulla rete; luce morbida per vertice. Sole e luna visibili nel cielo.
La luce di prima resta come alternativa: Opzioni → Light: classic (o `--classic-light`).

**Terre e mari.** Biomi di clima larghi un paio di centinaia di blocchi (pianura, deserto, tundra,
montagne); un campo continentale di circa 600 blocchi fa oceani profondi (fondale a 6-9) e mari
sulla piattaforma lungo le coste, con spiagge di sabbia dove il suolo è a pelo d'acqua; fiumi larghi
qualche blocco scavati a tre sotto il livello del mare, con le rive che risalgono in una dozzina di
blocchi, che sfociano in mare e non passano sotto le montagne né sulla piazza; lo spawn è sempre su
terra. In collina e in montagna, ogni tanto, un lago in quota: un catino largo 14-30 blocchi scavato
nel pendio, con l'acqua un blocco sotto il punto più basso del bordo, così non trabocca (seed 1: 23
in 4096x4096, fra quota 23 e 34). In un terzo del mondo circa, gole: tagli larghi tre o quattro
blocchi fino a due sotto il livello del mare, con un torrente sul fondo e pareti di roccia che
scendono in quattro-cinque blocchi, profonde fino a una trentina in montagna; un sentiero che ne
incontra una la passa su un ponte all'altezza dei bordi. `core.biome` dice anche `core:ocean`,
`core:sea`, `core:river`, `core:lake`, `core:beach` e `core:gorge`; `core.water_level(x, z)` dice fin dove sta l'acqua sopra una colonna.
Sotto terra (`mods/depths`), una regione di 128 blocchi su tre ha una miniera abbandonata (una sala
di assi, gallerie puntellate fino a 40 blocchi con diramazioni, torce, ragnatele, casse) o una
cripta (sala di mattoni con lampada e spawner, quattro tombe con bara di lastre e casse), sei-dodici
blocchi sotto, e una scala che sale in superficie.

**Un mondo senza bordo.** Il server genera la regione attorno allo spawn all'avvio; ogni altra
colonna di chunk si rilegge dal file o si genera quando un giocatore arriva a quattro colonne, e
si lascia (salvata) dopo trenta secondi senza nessuno vicino. Il client lascia i chunk che il
server gli dice di lasciare, e il client che gioca da solo ha lo stesso mondo senza bordo. In Release una colonna costa 1.7 ms da generare e 3.4 ms da
illuminare; volando a 12 m/s per 900 blocchi il client ha tenuto al massimo 198 chunk.

**Paesi e città** (`mods/villages`). Una regione di 256 blocchi su tre ha un paese: una piazza col
pozzo, da due a quattro vie di ghiaia e lungo le vie case di assi con tetto a due falde, porta e
finestre, campi di grano recintati col canale, una fucina con forno, incudine e cassa, lampioni.
Ogni tanto è una città (vie di ciottoli con traverse, case di mattoni e pietra, una torre). Un
sentiero di terra battuta porta al paese più vicino aggirando le montagne, il mare e l'oceano, e
passa fiumi, laghi e gole su un ponte di assi ad arco, con parapetti e piloni di tronchi (ogni tratto
a quattro o più blocchi dal suolo è ponte). `/paese` porta alla
piazza del paese più vicino, `/paese città` alla città più vicina. Tetti di scale di legno o
arenaria secondo il bioma (le scale nuove di `mods/steps`), sentieri e vie senza gradini più alti di
un blocco, e la cassa della fucina si riempie una volta sola.

**Portali.** Immagine continua con la vista reale, clip plane laterali, ricorsione fino a 3
livelli, tutte e 24 le orientazioni (pavimento, parete, soffitto, girati), corpi tagliati mentre
attraversano, orizzonte che torna in bolla dopo un attraversamento inclinato. Due mondi nello
stesso processo (`--dual`) e portali verso **un altro server**, con ticket firmati e mondi che si
possono solo guardare. **Si costruiscono in gioco** (`mods/portal`): una cornice di `portal:frame`
attorno a un buco d'aria, in piedi (dentro da 1×2 a 8×8) o coricata nel pavimento o nel soffitto
(da 1×1 a 8×8), si apre quando va a posto l'ultimo blocco; il tasto destro sulla cornice apre il
menù (rinominarla, girare il davanti, ruotare di un quarto, chiudere, collegarla a un'altra
cornice per nome, come la lista di yaportal). Fino a 16 per mondo, le quattro più vicine in vista disegnate; attorno alla piazza dello spawn una struttura a tema per ogni mondo -- pista, inferno, arena, rovine, camere di prova, backrooms, parkour (`mods/gates`) -- di cui il client apre e disegna solo le due bocche più vicine; l'altro
capo arriva prima di attraversare anche a 400 blocchi; uscendo da un muro con lo sguardo in su o
in giù la visuale torna a guardare fuori. Solo col server.

**Portali chiusi.** Un'apertura senza l'altro capo è chiusa da una membrana animata (un vortice con
un bordo luminoso a ovale) sull'ingresso del buco: blu o arancione per i due capi della portal gun,
verde per tutti gli altri, comprese le bocche della piazza non disegnate (il loro mondo non è ancora
pronto, o non sono tra le due più vicine). Quando l'altro capo arriva -- o avvicinandosi una bocca
entra tra quelle disegnate -- la membrana si apre dal centro sull'immagine in 0.9 s. Il colore lo
dà il mod (`core.portal_open(..., colore)`, protocollo 89). Rossa è una bocca sigillata a un
giocatore solo (`core.portal_seal`, protocollo 90): la vede lui, non ci passa, e quando il mod la
apre si apre per lui. Un mondo ha fino a 16 bocche e 32 aperture. Il tunnel del buco non sporge mai oltre
la faccia del blocco in cui sta l'apertura (quelle della gun stanno 0.1 fuori centro).

**Velocità attraverso un portale.** Si esce alla velocità con cui si è entrati, girata come il
portale, qualunque sia il dislivello fra le due aperture: il server compensa solo lo scarto fra
dove la mappa del portale mette il corpo e dove lo mette l'uscita (un tuffo da pavimento a
pavimento non perde quota a ogni rimbalzo). Fino al 2026-10-10 contava il dislivello intero, e
salendo di 4 blocchi a passo d'uomo si usciva fermi (camera 04 delle camere di prova).

**Chat.** T apre la chat, / la apre col comando: si parla a tutto il mondo, Su e Giù
richiamano le righe già mandate, le righe spariscono dopo 10 secondi. `/help` e `/list` sono
del motore; un mod aggiunge i suoi con `core.command` (`mods/whisper`: `/me`, `/msg`).

**Rete.** Server headless a 20 tick al secondo, ENet + zstd, cifratura X25519 +
XChaCha20-Poly1305, identità del giocatore a chiave, 2-8 giocatori per mondo. Il client **adotta**
la tabella dei blocchi del server, con forma, rotazione e modello di ogni blocco. Media
content-addressed con cache su disco condivisa fra i mondi. Il server valida il movimento (anche
il percorso di ogni claim) e lancia lui il raggio di un colpo.

**Movimento.** Ci si appende ai cigli e ci si tira su, si sale sulle scale a pioli camminandoci contro (o saltando), anche attraverso una botola aperta sopra; le botole stanno in alto o in basso nella cella secondo dove si clicca e si aprono a mano o con la pietrarossa, come le porte, e fanno rumore; una scala senza muro dietro cade. I villager salgono e scendono le scale a pioli (il minatore nel pozzo della miniera, che non scava il muro delle scale e ripara il pozzo se lo trova rotto; davanti a un ostacolo scava una piccola deviazione attorno) e girano attorno ai pali delle staccionate invece di tagliarci in mezzo. Il corpo a piedi ha peso: 0.6 s per arrivare alla corsa e 1.6 nodi di scivolata per fermarsi. In aria si muove come nei classici puzzle a portali (l'accelerazione in aria degli FPS classici): lo slancio resta, si curva strafando, e chi cade verso un portale nel pavimento viene portato al centro.

**Gioco.** Borsa e hotbar, oggetti a terra, casse con coperchio animato (due affiancate diventano una doppia da 54), porte e scale a pioli, craft, una fornace che
fonde il ferro col fuoco animato, acqua in cui si nuota, attrezzi con livello e durezza dei blocchi, salute e danni da caduta, audio con attenuazione (suoni in `assets/sounds`: blocchi, passi, acqua, portali, casse, jetpack), HUD, texture
pack intercambiabili (F7), personaggi glTF con scheletro, zombie e scheletri arcieri che escono al buio, inseguono il giocatore, colpiscono e tirano frecce,
rinculano ai colpi (e fanno rinculare), bruciano al sole, lasciano carne e ossa, anche nel mondo locale.
Scacchi (`mods/chess`, modelli Blender da `tools/make_chess_models.py`): un tavolino con la
scacchiera (le coordinate scritte in piccolo sulla cornice) e i pezzi Staunton; ogni mondo ne ha uno sulla piazza dello spawn, tre blocchi più in là
verso +X e +Z (a 67 20 67 nel sandbox di `tools/track.sh`), e altri si fabbricano. Tasto destro e la visuale vola sopra la scacchiera dal proprio lato;
col mouse si trascina un pezzo, o si clicca il pezzo e poi la casa, con le regole classiche (prima
il bianco, arrocco, en passant, promozione a scelta, scacco, matto, stallo, cinquanta mosse, triplice
ripetizione, materiale insufficiente). Ci si siede dal lato della scacchiera
dove si sta, bianco (il lato che guardava chi l'ha messo) o nero, o dall'altro se il proprio è preso;
gli altri guardano; da soli si muovono tutti e due. Col tasto destro
un menù: proporre la patta (la domanda compare sullo schermo dell'altro, che accetta o rifiuta;
giocare una mossa la rifiuta, e una sola proposta per mossa), abbandonare (con conferma), alzarsi e,
prima della prima mossa o a partita finita, la variante e l'orologio. Varianti: classica, scacchi 960
(traversa mescolata; si arrocca portando il re sulla torre), re della collina (re in d4/e4/d5/e5
vince), tre scacchi, corsa dei re (niente pedoni né scacchi, vince il re che arriva all'ottava;
se il bianco ci arriva prima, il nero ha una mossa per pareggiare) e vinciperdi (prendere è
obbligatorio, il re è un pezzo qualsiasi, vince chi resta senza mosse), atomica (ogni presa fa
saltare chi prende e i pezzi intorno, pedoni esclusi, con palla di fuoco, fumo, botto e i pezzi che
volano via; vince chi fa saltare il re avversario),
crazyhouse (i pezzi presi vanno in mano, su un vassoio accanto al tavolo, e si rimettono sulla
scacchiera al posto di una mossa) e orda (36 pedoni bianchi senza re contro i pezzi neri). Orologio: senza, 3+2, 5 o 10 minuti; chi resta senza
tempo perde, a meno che l'altro non abbia più di che dare matto, e l'orologio si ferma se chi deve
muovere si è alzato. ESC è il menù di pausa, come altrove. Il server decide ogni
mossa e tiene le partite fra un riavvio e l'altro.
Carte (`mods/cards`): un tavolo verde col mazzo napoletano da quaranta: briscola a due, tre o
quattro (a coppie), cinquiglio a cinque, scopa e tresette a due o a quattro; ogni mondo ne ha uno sulla piazza dello spawn, dall'altra parte degli
scacchi (a 61 20 67 nel sandbox). I semi e le figure sono modellati e resi in Blender
(`tools/make_card_art.py`), le facce composte da `tools/make_cards.py`, i modelli da
`tools/make_card_models.py`. Tasto destro e ci si siede: il corpo va al posto, girato verso il tavolo, e
tiene le carte in mano con tutte e due le mani (la clip `cards` del robot, `tools/make_card_hold.py`); la
visuale è il suo occhio col cursore libero, la mano un ventaglio davanti, tenuto dalle braccia in prima
persona (V: la vista dall'alto e ritorno). Gli altri vedono solo i dorsi del ventaglio: a nessuna macchina
arriva la mano di un altro. I posti sono in giro per il tavolo, quanti ne vuole il gioco, i compagni uno di
fronte all'altro; tasto destro di nuovo per scegliere il gioco o alzarsi. Un click su una carta della mano la gioca. I posti vuoti li gioca il
tavolo, così da soli si gioca contro tre; chi si siede a metà prende la mano di quel posto. Briscola
con la carta girata sotto il mazzo e i 120 punti; scopa con la presa obbligata della carta uguale, le
somme (un pannello chiede quale, se ce n'è più d'una), le scope, carte, denari, settebello e
primiera, fino a 11; tresette con l'obbligo di rispondere al seme (quando tocca a te le carte che puoi
giocare vengono avanti, le altre scure), i terzi, l'ultima presa e gli accusi (napoletana, buongioco),
fino a 21. A tre la briscola è senza un due; il cinquiglio ha l'asta (una carta sempre più bassa, poi
il due a più di 61), il seme lo sceglie chi la vince e il compagno è chi ha la carta chiamata, segreto
finché non la gioca; da soli se la carta è propria. Il tavolo gioca contando le carte uscite dove si è
misurato che serve (tresette a fine mano, il rischio di scopa). Il server dà le carte, manda a ognuno
solo la sua mano e tiene le partite fra un riavvio e l'altro. Due mazzi, a scelta dal menù: il classico e
uno illustrato da ElevenLabs, che si completa qualche immagine al giorno (`tools/elevenlabs_deck.json`;
finché un'immagine manca, quella carta è la classica).
Nuvole vere nel cielo: cumuli a y 96–112 che il vento sposta piano, sopra cui si può volare.
Animali domestici (`mods/pets`, modelli Blender da `tools/make_pet_models.py`): un cane e un gatto che escono di giorno
sull'erba; tasto destro con un osso (cane) o con un pesce (gatto) e diventano tuoi, col collare: ti seguono, ti
raggiungono se ti allontani oltre 14 nodi, e un altro tasto destro li fa sedere o rialzare. Restano tuoi anche
dopo un riavvio del server. Il pesce per il gatto si prende con la canna (tre bastoni in diagonale): si lancia
il galleggiante sull'acqua, e quando affonda ("A bite!") si ha poco più di un secondo per tirare su.
Animali selvatici (`mods/animals`, modelli Blender da `tools/make_animal_models.py`, a blocchi in pixel come in
Il classico gioco a blocchi): maiale, mucca, pecora e gallina pascolano sull'erba di giorno e lasciano carne, lana o una piuma. Colpiti
scappano in preda al panico. La pecora si tosa con le forbici (due lingotti di ferro in diagonale): due lane,
e ricresce in cinque minuti; le forbici durano 238 tosature. La gallina fa un uovo ogni due-cinque minuti, e ne
lascia uno quando muore. Col grano in mano (mucca, pecora) o i semi (maiale, gallina; uno ogni otto blocchi d'erba
scavati) due animali della stessa specie si innamorano (cuori sopra la testa) e fanno un cucciolo dalla testa
grande, che segue un genitore e cresce in venti minuti. Carne e
uova si mangiano (tasto destro), e cotte al forno ridanno di più.
Abitanti (`mods/villagers`, modelli Blender da `tools/make_villager_models.py` sullo scheletro del robot): ogni
letto con un banco da lavoro entro cinque blocchi è una casa, nei villaggi o costruita da un giocatore, e ha il suo
abitante. Umani contadini (mietono il grano maturo, riseminano, arano e fanno il pane), rospi pescatori (lanciano la
canna dalla riva, il galleggiante sull'acqua) e talpe raccoglitrici (meloni, funghi, semi dall'erba). Cucinano al
forno del villaggio verso mezzogiorno, mangiano quando hanno fame, mettono il superfluo nella cassa della loro casa e la
sera vanno a dormire sdraiati nel loro letto. Ogni casa dei villaggi ha la sua cassa accanto al letto e al tavolo. Strade e
sentieri seguono il terreno levigato su cinque blocchi, con un terrapieno dove il terreno scende, e salgono ogni
dislivello con una scala (o una lastra) del loro materiale: ghiaia, ciottoli, pietra liscia, terra grossolana, assi. Tasto destro: chi è e cosa porta. Uccisi, tornano dopo cinque minuti.
Ogni villaggio ha la sua ronda, alla casa più vicina al centro: guardie in ferro con la spada, arcieri col cappuccio
verde e un ispettore (cappello alto e registro) che si avvicina a chi passa e gli guarda nella borsa; due di ciascuno
nei villaggi con almeno otto case, tre guardie. Quello che un giocatore prende dalle casse e dai forni del villaggio,
raccoglie nei suoi campi o stacca dalle sue case (le case più 12 blocchi) è marcato rubato: non si impila con la stessa
cosa avuta onestamente. Beccato la prima volta, può restituire tutto (finisce nella cassa più vicina) e il villaggio
non si fida più di lui (gli abitanti non gli parlano, l'ispettore lo ricontrolla spesso); se rifiuta, non risponde o
viene beccato di nuovo, guardie e arcieri lo attaccano finché è nel villaggio, fino alla sua morte. Tre giorni senza
furti scoperti e il villaggio dimentica. Ogni villaggio marca con un numero suo e solo il suo ispettore lo riconosce;
quello che un giocatore ha costruito nel villaggio resta suo. Le guardie sono forti: 8 a colpo, di cui il 60% passa
l'armatura (tre colpi senza armatura, quattro in diamante); gli arcieri 6, metà oltre l'armatura. Di notte la ronda
combatte i mostri che arrivano entro 8 blocchi dal villaggio.
Territori (`mods/territory`): una pietra di confine (otto ciottoli attorno a uno smeraldo) fonda il territorio di chi la
posa, 5x5 attorno alla pietra; cresce di una cella ogni 5 secondi mentre uno dei proprietari ci sta sopra, la cella libera
del bordo più vicina a lui, fino a 64 blocchi dalla pietra (128x128). Tasto destro sulla pietra: il fondatore vede quante
celle ha, aggiunge i giocatori vicini come proprietari (fino a 6) o li toglie; solo lui la rompe, e il territorio sparisce.
Dentro, i proprietari camminano il 10% più veloci, scavano il 20% più veloci, prendono il 15% in meno dai colpi e
guariscono una volta e mezza più in fretta; gli altri il contrario, e non guariscono (anche nei villaggi). Quello che un
non proprietario prende, rompe o apre lì è marcato rubato. Il bordo si vede come pali luminosi: verdi i propri, rossi
quelli degli altri, gialli quelli di un villaggio. Il territorio di un villaggio copre tutta la sua pianta (vie,
case, campi, torri, con 4 blocchi di margine), le case abitate con 12 di margine, la miniera e i lotti venduti.
Ogni villaggio ha un sindaco (cappotto blu e fascia rossa) e un costruttore (grembiule e berretto giallo) alla casa più
vicina al centro. Tasto destro sul sindaco: un lotto 7x7 spianato con una strada per 8 smeraldi, o una casa per 20; il
lotto diventa territorio di chi lo compra e il villaggio cresce per comprenderlo. Il costruttore spiana, alza la casa
blocco per blocco (assi, tronchi agli angoli, vetri, porta, tetto, letto, tavolo, cassa e torcia) e posa una strada di
ciottoli con gli scalini fino alla via più vicina. I materiali (tronchi, ciottoli, terra) li portano i taglialegna, i
minatori (una scala scavata nel terreno) e gli scavatori del villaggio; dove mancano, o se per due minuti non arriva
niente, il costruttore li chiede al compratore: tasto destro sul costruttore per consegnarli. Sei ordini per villaggio;
terre e territori solo a chi gioca con una chiave, e nessun territorio entro 24 blocchi dallo spawn. Ogni villaggio
nuovo ha la sua miniera: un capanno con un pozzo di pioli che scende 14 blocchi fino a una galleria, dove i minatori
scavano gallerie in quattro direzioni con rami e torce; è terreno del villaggio, e quello che un giocatore ne porta via
è rubato. Chi ruba in un territorio altrui lo sa, e i proprietari presenti vengono avvisati.
Un mercante per villaggio (veste viola) vende quello che c'è nelle casse del villaggio (miniera e case, mai quelle dei
giocatori) e compra quello che tieni in mano, a smeraldi: il prezzo raddoppia se la merce scarseggia e si dimezza se
abbonda, e compra a metà prezzo. Anche ogni lavoratore commercia (tasto destro): vende dalle casse quello che porta il
suo mestiere e compra a prezzo pieno quello che gli serve (torce e picconi il minatore, semi il contadino, alberelli e
asce il taglialegna...). Ogni smeraldo pagato al villaggio (merce, lotti, case, guardie) e ogni smeraldo che i suoi
minatori scavano va nella cassa del villaggio, che il sindaco mostra; i villaggi pagano solo con quella, e a cassa
vuota non comprano; ogni villaggio parte con 32 smeraldi. Gli smeraldi entrano nel mondo solo come minerale. Tra
abitanti non si paga: prendono dalle casse del villaggio quello che serve (attrezzi, torce, cibo). Ogni lavoratore ha
il suo attrezzo, il migliore che trova nelle casse (diamante, ferro, oro, pietra, legno): lavora a 0.5, 0.7, 0.6, 1 e
1.5 volte il tempo e dura 715, 115, 15, 60 e 27 lavori (senza va a metà velocità). Un artigiano per villaggio rifà
attrezzi e torce con i materiali delle casse, di ferro se ci sono i lingotti; i lavoratori comprano dai giocatori
anche l'attrezzo del loro mestiere di ferro (5 smeraldi) e di diamante (15). Ogni ruolo dorme dove lavora: il sindaco nel municipio vicino alla
piazza (campana), guardie, arcieri e ispettori in una caserma in fondo a una strada (stendardo), l'artigiano in una
stanza sopra la fucina (incudine), mercante e costruttore in una casetta ciascuno vicino alla piazza (bilancia,
progetti); nei villaggi vecchi il costruttore costruisce quello che manca, e nel frattempo dormono dove trovano posto.
Guardie e arcieri dormono a turni. I custodi non scambiano l'angolo di tronchi di una casa per un albero. Ci sono anche case per famiglie (tre letti) e case a due piani. I minatori mettono i minerali nella cassa della
miniera e scavano anche senza ordini.
Dal sindaco: case piccole 5x5 (14), medie 7x7 (20) o grandi 9x7 a due stanze (30), di legno, mattoni di pietra o
mattoni; sulla propria casa un secondo piano con scala a pioli (12) e un orto recintato (6); e una guardia del
villaggio per il proprio territorio, 6 smeraldi al giorno, che insegue chi porta via la roba rubata. Chi ha rubato e
rientra nel territorio mentre c'è un proprietario gli restituisce tutto. In alto sullo schermo: dove sei e che effetto
ti fa.
Le porte: i villager le aprono anche arrivando di sbieco e le richiudono solo dopo esserci passati e quando
nella soglia non c'è nessuno (giocatore o creatura), e chiudono così anche le porte lasciate aperte da altri che
trovano sulla strada; una porta aperta non blocca più chi passa, e una porta chiusa
addosso a qualcuno lo lascia uscire. Il tasto destro su una porta con qualcuno nella soglia apre o chiude la porta.
Al costruttore si può dire di arrangiarsi (tasto destro, "Find it yourself"): i raccoglitori del villaggio portano
anche i materiali che non sono loro, e lui stesso va a raccogliere, prima quello che nessuno porta.
Una creatura che per 3 secondi non si avvicina alla prossima cella del suo percorso evita per 20 secondi la cella
che ha davanti e ricalcola la strada; bloccata fuori dal centro della cella, prima si ricentra (agli angoli di case e
corridoi si incastrava).
Robot aiutanti (`mods/helpers`): sei lingotti di ferro, una pietra rossa e una cassa al banco; si posano col tasto
destro. Tasto destro con un'ascia, un piccone o una pala gliela dà; senza, apre il programma: fino a sei ordini
eseguiti a giro attorno alla base (raggio 12) -- abbatti alberi (tronco e chioma, e qualche germoglio), pianta
germogli, scava terra o roccia sopra il livello della base, cerca minerali (galleria a gradini e ritorno; oro,
diamante, pietra rossa e smeraldo col piccone di ferro), ordina le casse, cucina (cibo crudo e combustibile dalle
casse al forno, il cotto in cassa), cava (col piccone: una fossa 5x5 subito a est della base, uno strato alla volta
fino a 24 di profondità, con attorno una scala a chiocciola, un gradino per strato, per risalire), illumina (torce
dalle casse della base sul terreno buio fino a 100 blocchi, le più vicine prima, su un reticolo a rombi che illumina
ogni cella una volta sola: i mostri non nascono più lì), seguimi. Scarica
in cassa a 24 oggetti. L'ancora del mondo (ferro agli angoli, pietra rossa ai lati, un diamante al centro), data
al robot come un attrezzo, tiene caricato il terreno attorno a lui e alla sua base anche senza giocatori vicini: lavora
lontano da tutti e riparte dopo un riavvio. "Close" chiude il pannello, "Pick up" lo restituisce con tutto.
In galleria puntella ghiaia e sabbia con ciottoli e risale scavando sopra di sé; se non raggiunge più la sua cassa,
o si trova lontano o sotto la base, torna a casa scavando.
Il banco degli attrezzi (`tools:bench`) dopo il primo piccone è un banco 3x3 per farne altri; i germogli
(`trees:sapling`) crescono in alberi in circa quattro minuti; il pesce si cuoce al forno.
Piccone, ascia, pala, spada e zappa in cinque materiali, con le forme della pietra: legno (assi,
`tools:wood_*` e `farm:hoe`, livello 0, 2x la mano, spada 3, 59 usi), pietra (`tools:pick` ecc. e
`tools:stone_hoe`, livello 1, 5x/4x, spada 4, 131), ferro (`furnace:iron_*`, livello 2, 9x/7.2x, spada 5,
250), oro (`furnace:gold_*`, livello 0 ma 15x/12x, spada 3, 32) e diamante (`furnace:diamond_*`, livello 3,
12x/9.6x, spada 6, 1561). Ogni strumento si consuma: un uso per blocco con durezza scavato, per colpo dato, per
zolla arata (`core.wear_held`); finiti gli usi sparisce. Ogni materiale ha la sua forma in mano
(`tools/make_held_models.py`: legno squadrato, pietra scheggiata e legata, ferro forgiato col collare, oro
sottile con anelli e pomolo, diamante a punte di cristallo con impugnatura fasciata); le icone sono quelle di
pietra ricolorate (`tools/make_item_textures.py`).
Campi (`mods/farm`): la zappa (due assi e due bastoni) ara erba e terra, i semi si piantano sulla terra arata
e il grano cresce da solo in quattro stadi, circa un minuto e mezzo l'uno; maturo dà una spiga e 1-3 semi.
Tre spighe in fila al banco fanno il pane, che si mangia. Il grano si attraversa e si mira all'altezza della
pianta; con l'acqua entro quattro blocchi cresce il doppio più in fretta, e senza luce (9 o più: sole
o torcia) non cresce. La terra arata nuda e senz'acqua torna terra, e saltarci sopra la calpesta.
Modelli e texture: `tools/make_farm_assets.py`.
Letto (`mods/bed`, tre lane sopra tre assi, lungo due blocchi): cliccato fissa dove si rinasce; di notte ci si sdraia
(disteso sulla schiena con la testa sul cuscino, visto così da tutti, protocollo 75), e quando
tutti nel mondo sono a letto arriva l'alba. Con un mostro entro 8 blocchi non si dorme. Cacciavite
(`mods/screwdriver`, lingotto di ferro su bastone): gira un blocco già piazzato. Blocchi compatti (`mods/compact`): nove lingotti, gemme o polveri al banco fanno un blocco, e il
blocco torna nove. Cinnabar (`mods/cinnabar`): la polvere si stende col tasto destro; leva (bastone su pietra),
pulsante (una pietra, acceso un secondo), torcia di cinnabar (cinnabar su bastone: spenta se il blocco sotto
è alimentato, per le porte NOT), ripetitore (torcia, cinnabar, torcia su tre pietre: riporta la corrente a
15, in una direzione, con ritardo da 1 a 4 col tasto destro), comparatore (legge la corrente o quanto è pieno
un baule), lampada (quattro cinnabar attorno al vetro), pistone e pistone appiccicoso (spingono fino a 12
blocchi in tutte e sei le direzioni, giocatori e creature compresi), observer (manda un impulso quando il blocco
che guarda cambia), hopper (passa gli oggetti da un contenitore all'altro e raccoglie quelli a terra);
anche il blocco di cinnabar alimenta; la corrente fa 15 blocchi e passa nei blocchi solidi; torce e
ripetitori cambiano con un attimo di ritardo, quindi gli orologi funzionano.
Nastro trasportatore (`mods/conveyor`, tre cuoio su lingotto, cinnabar, lingotto: quattro): porta a 1 m/s
oggetti a terra, giocatori e creature nel verso in cui guardava chi l'ha piazzato, e il nastro scorre (costole e
frecce animate). Nastri vicini si fondono in uno (i rulli restano solo alle estremità), un nastro messo di
traverso alla fine di una linea la fa curvare di 90°, e un nastro un blocco più in alto davanti (o dietro) fa
una rampa in salita (o in discesa) con gradini sotto. Un blocco davanti ferma gli oggetti, oltre la fine cadono
(anche in un hopper); girato col cacciavite si ridisegna entro un secondo. Protocollo 78. Selezionatore
(tre nastri e una cinnabar): entra da dietro ed esce a sinistra, avanti o a destra; i suoi tre quadretti
(tasto destro) sono filtri in quest'ordine, e ciò che nessun filtro nomina esce a turno dalle uscite senza
filtro che hanno un nastro; se l'uscita è occupata aspetta. Estrattore (hopper su nastro): prende un oggetto
al secondo da un contenitore accanto, dietro, sopra o sotto e lo mette sul nastro, così com'è (usura e
incantesimi compresi) e solo dagli slot magazzino o prodotto finito; si ferma se il nastro è pieno. Sui nastri e sul
selezionatore gli oggetti non scadono. Modelli: `tools/make_conveyor.py`.
Slot con un ruolo (`core.container_roles`, protocollo 79): materia prima, materiale di consumo, prodotto finito,
magazzino (quello di ogni cassa). Le macchine (estrattore, hopper) prendono solo da prodotto e magazzino e non
mettono mai nel prodotto; a mano nel prodotto non si mette niente. Nel pannello ogni slot ha una barra del colore
del suo ruolo (blu materia prima, arancio consumo, verde prodotto) e i colori sono spiegati sopra. La fornace ha
una riga per ruolo: minerale, carbone, lingotti, e ogni riga accetta solo i suoi oggetti (`core.container_accepts`):
nella prima ciò che si cuoce, nella seconda ciò che brucia, nella terza niente; il legno va in entrambe, e ciò che
sta nella prima non viene mai bruciato. Una cottura dura 2 s e una barra sotto gli slot ne mostra l'avanzamento
(`core.container_progress`, protocollo 80). Cuoce 21 cose (metalli grezzi, sabbia, ciottoli, pietra, argilla, cactus,
carne, uova, e ogni tronco in carbone di legna) e brucia come il classico gioco a blocchi: carbone e carbone di legna 8 cotture,
blocco di carbone 80, tronchi e assi 1.5, bastone 0.5; il calore avanzato resta nella fornace. Un blocco di ferro
(nove lingotti) non si cuoce.
Fame (`mods/hunger`): dieci cosce sopra la salute, un punto ogni 40 s; il cibo le riempie, e la salute torna
da sola solo con la barra quasi piena; a zero si perde salute fino a 1. Torce (`mods/torches`, carbone su
bastone, luce 14). Canna da zucchero sulle rive dei laghi, che cresce fino a tre: tre canne fanno carta, tre
carte e una pelle (le mucche la lasciano) un libro. Esperienza da minerali e creature (barra verde col livello). Tavolo da incantamento (libro, due diamanti,
quattro ossidiana): il livello I, II o III costa altrettanti lapis e livelli e dà a sorte uno o due
incantesimi adatti all'oggetto (Efficienza, Affilatezza, Indistruttibilità, Protezione), che restano
sull'oggetto (borsa, baule, a terra, riavvio). Anche fondere al forno dà esperienza, a chi lo apre.
In prima persona sul kart si vedono il kart e le proprie braccia sul volante, animate come in terza persona; in volo
le braccia non ondeggiano più come camminando.
Armature (`mods/armor`): elmo, corazza, gambali e stivali in legno, oro, ferro e diamante, fatti al banco con le
forme classiche, indossati nei quattro slot a sinistra della borsa (anche con lo shift-click) e visibili sul
robot, modelli Blender sul suo rig; ogni punto toglie il 4% di un colpo, fino all'80% col diamante (le cadute
no), con una barra sopra la salute, e ogni colpo consuma i pezzi fino a romperli. Oro e diamante stanno in
fondo al mondo e si scavano col piccone di ferro.
Il personaggio si sceglie dal menù iniziale o dalla pausa (Character): robot, umano, uomo talpa o uomo rana
(`tools/make_species.py`, sullo scheletro del robot con tutte le sue clip), uno stile per specie (capelli corti,
lunghi o barba; lampada da minatore, occhialoni o occhialini; macchie, corona o ninfea), tre colori e due accessori
(`tools/make_robot_extras.py`; le code della sciarpa e la medaglia dondolano), visti da tutti.
Lo zaino per il volo (`mods/jetpack`) non è più parte del robot: si fa al banco (sei lingotti di ferro e
uno d'oro) e si indossa nel quinto slot, la schiena (protocollo 76); senza non si vola (F non fa niente, e
toglierlo in aria vuol dire cadere) e le sue fiamme si accendono solo in volo. Al suo posto si può portare
uno zaino di cuoio (`mods/backpack`, otto cuoi e un baule): B apre i suoi 27 slot, che seguono quello zaino
ovunque vada (per terra, in un baule, a un altro giocatore). Al banco, uno zaino e da uno a tre coloranti ne
colorano corpo, patta e spallacci, nell'ordine in cui stanno nella griglia; nella borsa i colori sono tre
segni sull'icona. Tutti e due hanno un modello in mano e uno addosso, fatti in Blender. Il creative (C) è
una doppia chest sopra la borsa: trascinarne fuori un oggetto ne dà una copia, la rotella scorre le righe
(una barra a destra dice dove si è), accanto c'è un cestino (e quel che si rimette nel catalogo sparisce); la casella di ricerca a sinistra (o `/creative testo`) filtra per nome mentre si scrive.
Lo zaino a braccia (`mods/armpack`, uno zaino di cuoio, sei lingotti di ferro, due di rame e uno d'oro) è
di metallo, con due braccia snodate che salgono sopra le spalle e una pinza ciascuna per uno strumento in
più (`tools/make_armpack.py`: ossa `pack_*` sul rig del robot, clip `armpack_idle`, `armpack_swing_L/R`,
`armpack_fold/unfold`, prese `pack_tool_L/R`). Indossato apre i due slot delle braccia della borsa
(`register_item(..., arms)`, protocollo 95), che prendono un oggetto non indossabile ciascuno; non si toglie
finché le braccia tengono qualcosa. Gli strumenti si vedono nelle pinze, in terza persona e per gli altri
(protocollo 96) e in prima persona, dove le braccia (`armpack_arms.gltf`) entrano dagli angoli alti della vista;
scavando conta lo strumento migliore fra mano e braccia, e colpendo una creatura o un giocatore il danno della
mano si somma a quello delle armi nelle braccia (spada in mano e due nelle braccia: 12); le braccia che tengono
qualcosa colpiscono (insieme, a turno). I blocchi messi nelle braccia non si vedono.
La borsa (E) ha a sinistra della griglia una figura stilizzata: testa, petto, gambe e piedi sulle sue parti,
la schiena accanto, e gli slot delle braccia sopra le spalle quando lo zaino ne ha.

Minerali (`mods/ore`) come nel classico gioco a blocchi, in vene dentro la pietra e mai in superficie: carbone, rame,
ferro, lapislazzuli, oro, cinnabar, diamante e smeraldo (solo sotto le vette, a blocchi singoli). Carbone a
mano; rame, ferro e lapis col piccone di pietra; il resto con quello di ferro. Ognuno dà il suo item: il
carbone dà carbone (il combustibile del forno), ferro/oro/rame danno il grezzo da fondere in lingotto, cinnabar
4-5 polveri, lapis 4-9, diamante e smeraldo la gemma. Diamante e cinnabar sotto y 10, carbone e ferro fin
dentro le montagne. Texture: `tools/make_ore_textures.py`, la pietra del gioco con i grumi del minerale.
Nei mondi salvati prima, il vecchio blocco `ore:coal` si carica come minerale di carbone (`core.node_alias`),
e i minerali di ferro e oro rimasti in borse e bauli come il grezzo (`core.item_alias`).
Un inferno (`games/inferno`): caverne di scoria fra pavimento e soffitto di bedrock, mari di lava che
bruciano, glowrock appesa, ash sand, quarzo, fortezze di mattoni a ponti e corridoi, cielo rosso senza
sole, lava che scorre, brute, wraith e cinder con le palle di fuoco; ci si arriva dalla seconda apertura del
sandbox, in una camera con la cornice di ossidiana, o da un portale costruito: una cornice di ossidiana
accesa con l'acciarino diventa la porta dell'inferno. L'inferno è un'estensione del sandbox: carica i suoi
mod di gioco (attrezzi, blocchi, cibo, armature...) così quello che si porta in borsa lì vale ancora, ma
non i loro laghi, alberi e minerali (`core.mapgen_only`) né le costruzioni della piazza. Fame, esperienza,
chiavi viste e zaini restano gli stessi fra le stanze (`core.shared_storage_*`, nel file del sandbox).

**Modding.** Luau, una VM per mod con quota di memoria, budget di interruzione e capability.
Hot reload (F5), event bus tipizzato, binding generati da una tabella sola, ECS scriptabile,
ABM/LBM, stage del mapgen, blocchi con forma a scatole o disegnati come modello, rotazioni,
movimento per mondo e per corpo, un giocatore portato sul corpo di un altro (`core.ride`, protocollo 63),
entità visibili a tutti, pacchetti `.cnpmod` con versioni e
dipendenze, una memoria per mod nel world file (`core.storage_set`) e una frase sullo schermo di un
giocatore (`core.notice`). Un mondo può mandare **codice Luau al client**, in una sandbox senza file né rete, e il
giocatore può rifiutarlo. Guida in [`docs/MODDING.md`](docs/MODDING.md).

**Salvataggio.** SQLite: chunk, casse, borse, posizione e sguardo dei giocatori (per chiave), la memoria dei mod. Ogni 30 s
il server scrive solo i chunk cambiati, in una transazione sola con casse e memoria delle celle (che riscrive
solo se sono cambiate), e salva anche quando lo si ferma con Ctrl+C. Il file si
legge una colonna alla volta, quando serve; senza file, delle colonne lasciate restano in memoria
(compresse) solo quelle che qualcosa ha cambiato. Il file tiene i nomi dei blocchi: un mod che ne
aggiunge uno non costringe a ripartire, i chunk salvati vengono riscritti per nome all'apertura; i
blocchi di un mod tolto restano, magenta, finché il mod non torna.

## Giocare e testare: riferimento

Dopo aver compilato ([sopra](#compilare-dal-sorgente)), dalla cartella del checkout:

```sh
ctest --test-dir build                       # 13 test, ~3 s; -LE gpu salta quello che apre una finestra

./build/src/client/cnp_client                # menù: Gioca (mondo nuovo o salvato), Opzioni, Esci
./build/src/client/cnp_client --quick        # avvio rapido: il mondo locale di prova, niente salvato

./build/src/server/cnp_server --listen 25301 --world 128 --world-file mondo.db
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name giacomo

# L'arena yafps in stile arena shooter moderno: un sandbox e l'arena su un server solo
tools/fps.sh --fresh

# L'arena da sola: il client non ha i mod, gli arrivano dal server
./build/src/server/cnp_server --listen 25302 --mods games/yafps/mods --textures games/yafps/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25302 --name giacomo

# Punta e clicca in stile action RPG isometrico: un sandbox e il mondo arpg su un server solo, dietro un arco di pietra
tools/arpg.sh --fresh

# La pista a tempo: con --portals si nasce sulla strada, davanti alla piazzola
./build/src/server/cnp_server --listen 25303 --portals --mods games/track/mods --textures games/track/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25303 --name giacomo

# Un sandbox con due portali: il primo verso l'arena, il secondo verso la pista.
# Ognuno torna all'apertura da cui si è entrati (/0, /1). --ticks 0: senza, un server si ferma dopo 50 s
./build/src/server/cnp_server --listen 25301 --ticks 0 --portals --portal-to 127.0.0.1:25302/0 --portal-to 127.0.0.1:25303/0
./build/src/server/cnp_server --listen 25302 --ticks 0 --portals --portal-to 127.0.0.1:25301/0 --mods games/yafps/mods --textures games/yafps/textures
./build/src/server/cnp_server --listen 25303 --ticks 0 --portals --portal-to 127.0.0.1:25301/1 --mods games/track/mods --textures games/track/textures
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name giacomo

# Lo stesso su un server solo, a stanze: sandbox, pista e inferno (tools/track.sh fa questo)
./build/src/server/cnp_server --listen 25301 --ticks 0 --portals \
    --room sandbox --portal-to @pista/0 --portal-to @inferno/0 \
    --room pista --mods games/track/mods --textures games/track/textures --portal-to @sandbox/0 --portal-to @inferno/2 --portal-to @inferno/1 \
    --room inferno --mods games/inferno/mods --textures games/inferno/textures --fallback-textures assets/textures --portal-to @sandbox/1 --portal-to @pista/2 --portal-to @pista/1
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name giacomo   # --room inferno per entrarci subito
```

**Comandi.** WASD muove, F vola, tasto sinistro rompe, tasto destro piazza (a mani vuote usa il
blocco). Rotellina o 1-9 per lo slot, E borsa, C lista creativa, Q butta. F3 informazioni, F4 mostra o nasconde i confini dei territori, F8 le celle dove i mostri possono nascere (rosso: buio ora; giallo: buio di notte; una torcia libera sei blocchi attorno), F5 ricarica i mod, F6
profiler, F7 cambia texture pack, F2 spegne l'occlusion culling (per capire se qualcosa che sparisce
lo nasconde il culling), F11 schermo intero, V vista da dietro, ESC pausa (riprendi, opzioni: sensibilità del mouse, volume, vista, luce nuova o classica, ombre, schermo intero, informazioni; torna al titolo, se la partita è partita da lì; esci). Le opzioni restano tra una partita e l'altra, in `~/.config/cubes-and-portals/options`. `CNP_HITCH_MS=30 cnp_client ...` stampa ogni frame più lungo di 30 ms con le sezioni del profiler.

### Client (`cnp_client`)

| Flag | Cosa fa |
|---|---|
| (nessuno) | Il menù iniziale: Play, Multiplayer, Host a server, Options, Character, Quit. Character sceglie la specie (robot, umano, talpa, rana) e il suo stile, i colori (primario, secondario, accento: il suo o uno dei 16 delle tinture) e un accessorio in testa (cappellino, cilindro, cuffie) e uno al collo (sciarpa, papillon, medaglia), con il robot che gira accanto; restano nelle opzioni e gli altri giocatori li vedono (protocollo 94). Si cambia anche a partita in corso: ESC, Character. `--livery N` fa lo stesso per un join con `--connect`. Play entra senza chiedere nulla, con la chiave di `~/.local/share/cubes-and-portals/player.key` (creata al primo uso) e il nome salvato ("Player" se non c'è); se il mondo conosceva il nome sotto un'altra chiave, il client lo libera prima di avviarlo e la nuova eredita borsa e posizione. L'account è del server: in Multiplayer si sceglie prima il server e poi il menù chiede nome e password (almeno 6 caratteri) per QUEL server, precompilando il nome con cui ci si è già giocati; Host a server li chiede per il server ospitato. Un altro server è un altro account, e un join rifiutato li richiede. L'identità del giocatore è derivata da quei due (Argon2id, 64 MiB, 3 passate, ~130 ms) invece di stare in un file, e i server che il client avvia girano con `--require-identity`. Il nome resta in `~/.local/share/cubes-and-portals/player`, la password no. Di ogni server la lista ricorda anche il nome con cui ci si è giocati (`indirizzo<TAB>nome<TAB>chiave` in `servers`, mostrato sulla riga come "as nome"), e la chiave che il server ha mostrato al primo join: da lì in poi è fissata, e un server che ne presenta un'altra viene rifiutato finché la riga non viene tolta e rimessa: tornandoci il menù chiede la password di QUEL nome. Play elenca i mondi salvati in `~/.local/share/cubes-and-portals/worlds/` (uno per cartella: file del mondo, chiave, log e seme) o, col grande bottone verde in cima "+ Create a new world", ne crea uno con nome e seme, e con Edit lo rinomina o lo cancella (due clic; non se un server lo sta usando); il mondo scelto gira su un `cnp_server` avviato dal client su 127.0.0.1, che si ferma salvando quando il client esce (anche se muore). Multiplayer tiene la lista dei server (`host` o `host:porta`, porta 25301 se non detta) in `~/.local/share/cubes-and-portals/servers`: aprendo la pagina ognuno viene bussato a turno e mostra un tondo — grigio non provato, giallo in prova, verde con il ping, rosso se non risponde — e la `x` lo toglie. Un join che fallisce riporta alla lista con il motivo. Host a server apre lo stesso elenco di mondi più una porta (default 25301): il server sta su tutti gli indirizzi della macchina invece che solo su 127.0.0.1, e il menù di pausa mostra "Hosting at INDIRIZZO:PORTA" da leggere a chi deve entrare. Options, Language sceglie la lingua (all'inizio quella di `LANG`, salvata nelle opzioni come `lang`): le traduzioni sono `assets/lang/<codice>.txt`, una riga `testo come scritto = traduzione` con `{}` per le parti variabili, e valgono per tutto ciò che il client scrive a schermo, menù, pannelli e avvisi delle mod, chat e `core.hud.text`; una stringa assente resta com'è. Ci sono `it.txt` e `en.txt` (quest'ultimo per i testi che le mod scrivono in italiano); una lingua nuova è un file nuovo |
| `--quick` | Avvio rapido, senza menù: il mondo locale di un solo processo, che non salva. Lo saltano anche `--connect`, le scene di prova e le misure |
| `--connect HOST` `--port N` `--name NOME` | Entra in un server invece di generare un mondo locale |
| `--server-key HEX` | Accetta solo il server con questa chiave (la stampa il server all'avvio) |
| `--player-key FILE` | Dove sta l'identità del giocatore (default `player.key`, creata al primo avvio) |
| `--watch HOST:PORT` | Guarda dentro un altro mondo come osservatore (ripetibile) |
| `--world-code refuse` | Non esegue il codice che i mondi mandano al client |
| `--media-cache DIR` `--no-media-cache` `--media-cache-size MB` | Cache dei media scaricati |
| `--textures DIR` (ripetibile) `--no-textures` | Texture pack; più di uno e F7 li alterna |
| `--models DIR` `--no-models` `--model FILE.gltf` | Dove cercare i modelli; un personaggio davanti allo spawn |
| `--mods DIR` `--seed N` `--world N` | Mondo locale: mod, seme, lato in blocchi |
| `--day-length S` `--time T` | Durata del giorno in secondi, ora di partenza (0..1) |
| `--dual` `--facing` `--oriented` | Scene di prova: due mondi, due portali affacciati, un portale coricato |
| `--recursion N` `--portal-scale F` `--portal-config FILE` | Profondità della ricorsione (0-3), risoluzione dei target, forma del portale ([`config/portal.cfg`](config/portal.cfg)) |
| `--log-view` | Traccia della vista attorno a ogni attraversamento |
| `--bench` `--dual-bench` `--soak S` | Misure: FPS e costo dello scripting, costo del secondo mondo, volo automatico |
| `--verify` | Il test di continuità del portale (lo lancia `ctest`) |
| `--play-seconds S` `--watchdog S` | Esce dopo S secondi; scrive lo stack se un frame non arriva entro S secondi |
| `--shot S` | Uno screenshot in `bench/play.png` (relativo alla directory di lavoro) dopo S secondi di una corsa a tempo, in locale o connesso |
| `--fly-down N` `--walked-down` | In una corsa a tempo: sale N blocchi in volo e riscende da solo, dicendo cosa costa l'atterraggio; con `--walked-down` lascia il volo in cima, e quella è una caduta |

L'elenco completo è in `main()` di [`src/client/main.cpp`](src/client/main.cpp).

### Server (`cnp_server`)

| Flag | Cosa fa |
|---|---|
| `--listen PORT` | Accetta giocatori. Senza, genera il mondo, esegue `--ticks` tick e stampa l'hash |
| `--bind HOST` | L'indirizzo su cui ascoltare (default tutti); il menù del client usa 127.0.0.1 |
| `--world N` `--seed N` `--world-file FILE` | Lato della regione generata all'avvio e coperta dall'hash (default 128; il mondo non ha bordo), seme (default 1), file SQLite dove salvarlo. Un solo server per file: il secondo trova il lucchetto (`FILE.lock`, `flock`) ed esce dicendolo |
| `--save-every S` | Ogni quanti secondi scrive quello che è cambiato (default 10; 0 solo alla chiusura). Ctrl+C o SIGTERM chiudono salvando (un secondo Ctrl+C chiude subito; un SIGTERM ripetuto no) |
| `--mods DIR` `--no-mods` | Mod del mondo |
| `--textures DIR` `--no-textures` | Media che il mondo serve |
| `--day-length S` `--time T` | Ciclo giorno/notte |
| `--key-file FILE` `--no-encryption` | Identità del server; cifratura spenta (solo per prove) |
| `--max-players N` `--require-identity` `--no-observers` | Posti, giocatori obbligati ad avere una chiave, niente osservatori da altri mondi. Un nome è di chi lo ha usato per primo: con un file del mondo la coppia nome-identità resta nella tabella `names` e chi torna con un'altra chiave viene rifiutato, anche dopo un riavvio; senza file vale finché il server gira |
| `--names` `--free-name NOME` | Con `--world-file`, e con nessun server sul file: chi possiede ogni nome, o libera un nome. Chi entra dopo con quel nome eredita borsa e posizione del vecchio proprietario: così si recupera una password dimenticata, o si cede un nome |
| `--portals` `--portal-to HOST:PORT[/N]` | Una coppia di aperture gestita dal server. `--portal-to` dice dove porta la prima (e da quale apertura `N` di là si esce, 0-15); ripetuto, dove portano la seconda, la terza e la quarta (fino a 4 bocche per mondo, protocollo 72) |
| `--ticket-key FILE` `--portal-peer-key HEX` `--trust HEX` `--require-ticket` | Ticket firmati per attraversare fra server |
| `--mobs N` `--ticks N` `--quiet` | Mob di prova, numero di tick prima di uscire, meno log |
| `--room NOME` | Una stanza: un mondo in più sullo stesso server e sulla stessa porta (protocollo 56). Quello che segue vale solo per quella stanza (`--mods`, `--world-file`, `--portal-to`...), quello prima del primo `--room` per tutte. `--portal-to @NOME[/N]` porta in un'altra stanza del server; chiavi e ticket fra le stanze li fa il server. Borsa e vita del giocatore sono una sola in tutte le stanze. Il client sceglie la stanza con `--room NOME` (senza, la prima) |

### Strumenti

| | |
|---|---|
| `tools/play.sh` (`local`, `--fresh`) | Compila in Release, avvia server e client con tutti i mod e un mondo salvato in `worlds/play/`, e stampa dove trovare lago, torre, grotte e fornace (seme 1) |
| `tools/track.sh` (`--fresh`) | Un server con due stanze: il mondo di partenza (i mod di `mods/`) e la pista kart, la prima apertura del sandbox porta sulla piazzola e il portale della piazzola riporta indietro (mondi e log in `worlds/track/`) |
| `tools/arpg.sh` (`--fresh`) | Un server con due stanze: il sandbox e il mondo `arpg` (`games/arpg`), punta e clicca in stile action RPG isometrico; la prima apertura del sandbox porta davanti a un arco di pietra che riporta indietro (mondi e log in `worlds/arpg/`) |
| `build/tools/cnp_modpack pack mods/ore` | Un mod in un file `.cnpmod`, che si carica da `mods/` come una directory |
| `tools/dist/package-linux.sh build-dist` | Tarball Linux portabile (dopo `cmake -DCNP_PORTABLE=ON -B build-dist`) |
| `tools/dist/package-appimage.sh build-dist` | Lo stesso come AppImage, un file solo (23 MB): avviato è il client, `… server` e `… modpack` gli altri due. Serve `appimagetool` nel PATH o in `$APPIMAGETOOL` |
| `tools/dist/package-flatpak.sh build-dist` | Lo stesso come bundle Flatpak (17 MB, runtime freedesktop 25.08), senza flatpak-builder; si installa con `flatpak install --user <file>.flatpak` |
| `tools/dist/package-windows.sh` | Zip Windows x86_64 (25 MB), cross-compilato da Linux con zig 0.16 in `build-win/` |
| `tools/dist/package-macos.sh` | `.app` in un `.dmg`, da lanciare su un Mac (serve l'SDK Apple); lo fa `.github/workflows/release.yml` su un runner macOS, insieme agli altri pacchetti |
| `build/tools/cnp_netshape` | Relay UDP con ritardo e perdita, per provare link lenti |
| `build/tools/libcnp_clock_skew.so` | `LD_PRELOAD` che sposta l'orologio di un processo, per i ticket |
| `tools/two_machines.sh` | Due server in container, con MTU e NAT veri |
| `build/tools/cnp_bake_textures` | Rigenera le texture segnaposto dei blocchi |

### Test

Tredici, scelti perché proteggono cose che si rompono in silenzio e costano care: input dalla
rete (`protocol`, `chunk_codec`, `crypto`), il mondo salvato (`storage`, `migration`,
`persistence`), la matematica dei portali e le collisioni (`portal_math`, `portal_traverse`,
`physics_box`, `portal_continuity` in GPU), la sandbox dei mod (`script_isolation`), client e
server d'accordo sul mondo (`worldgen`, `two_process_join`). Il resto si verifica giocando. Il
perché di ciascuno è in [`tests/CMakeLists.txt`](tests/CMakeLists.txt).

## Scelte tecniche

| | |
|---|---|
| Grafica | raylib 6.0 dietro un'interfaccia `gfx_*` sottile (migrabile a `sokol_gfx`), OpenGL 3.3 core |
| Linguaggi | C++20 per l'engine, Luau 0.733 per lo scripting |
| Rete | ENet 1.3.18, zstd 1.5.7, Monocypher 4.0.3 |
| Storage | SQLite 3.50.4 |
| ECS | Flecs 4.1.6 |

## Documentazione

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — come è fatto il codice oggi e dove si mette
  una feature nuova.
- [`docs/DESIGN.md`](docs/DESIGN.md) — analisi di fattibilità, decisioni, piano a milestone
  M-1..M13, rischi.
- [`docs/MODDING.md`](docs/MODDING.md) — scrivere un mod. [`docs/API.md`](docs/API.md) — l'API
  Lua, generata da `src/script/api.luau`.
- [`docs/TEXTURE_PACKS.md`](docs/TEXTURE_PACKS.md) — fare un texture pack.
- [`TODO.md`](TODO.md) — cosa manca e cosa non va (solo le voci aperte).
- [`docs/archive/`](docs/archive/README.md) — lo storico: il vecchio README milestone per
  milestone, il vecchio TODO per intero e i `M*_FINDINGS.md` con le misure.

## Licenza

Il codice è sotto licenza MIT ([`LICENSE`](LICENSE)). Le librerie in `third_party/` hanno
ciascuna la propria licenza, nella loro cartella.
