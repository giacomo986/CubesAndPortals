# Cambiamenti

*[Read in English](CHANGELOG.md)*

Una voce per ogni alpha consegnata, la più recente in cima. Come si numerano e come si fa
un'alpha: [docs/BUILDING.it.md](docs/BUILDING.it.md#versioni).

## Non ancora consegnato

- Un sandbox pacifico: nessun mostro esce dal buio, e quelli in giro spariscono. Si sceglie quando
  si crea il mondo ("Mostri" nella pagina del mondo nuovo), nella pagina Modifica di un mondo
  salvato prima di caricarlo, o in partita con `/peaceful on|off` (admin); resta dopo i riavvii.
  Quello che lasciano solo i mostri si trova ancora: lo spawner di un dungeon, ora quieto, lascia
  ogni tanto quello che lascerebbero le sue creature (ossa, carne, polvere da sparo, frecce);
  l'altare del profondo dà cuore e frammenti senza svegliare il guardiano; i villaggi non hanno
  assedi. Server: `--peaceful`. Per le mod: `core.peaceful()`, `core.set_peaceful(on)`,
  `core.mob_leave(kind, x, y, z)`.
- **Protocollo 114**: non gioca con server o client 113.
- Il cubo delle camere di prova ora è una scatola vera: cade, scivola, si ribalta sugli spigoli,
  rotola e rimbalza, e resta com'è atterrato; lanciato o colpito si gira mentre vola, e
  attraversando un portale continua a girare. I cubi si urtano e si impilano; ci si sale sopra, e
  uno lanciato contro di te ti spinge indietro. Camera di prova 14, i gradini: due pareti troppo
  alte da scalare, un cubo portato ai piedi di ognuna è la via per salire. Camera di prova 15, tre
  cubi: una parete mezzo blocco più alta, che si raggiunge solo da due cubi impilati, e quelli solo
  salendo su un terzo. Per le mod: `core.mob_tumble(id)`, un worker reso corpo rigido (Jolt), disegnato com'è girato su
  ogni client.
- Cadendo in un portale a pavimento e uscendo da uno a parete non vieni più portato un blocco
  troppo avanti: esci dentro l'apertura della parete, dove l'immagine ti mostrava, fin dove un
  corpo ci sta.
- Un blocco che una mod mette e gira subito, dove il mondo non è ancora caricato, mantiene la
  rotazione: le porte delle camere di prova a volte stavano di traverso, e i pezzi di pista
  potevano uscire non girati.
- Il vetro si vede di nuovo dove mancava: nelle camere di prova non veniva disegnato affatto,
  né direttamente né attraverso un portale, perché il disegno perdeva lo shader quando cambiava
  la miscelazione.
- Tra due portali a pavimento o a soffitto mantieni la direzione in cui andavi: segue il portale
  solo l'inclinazione (guardi in giù entrando, in su uscendo), e l'orizzonte inclinato torna in
  piano con un'animazione.
- Un portale nel soffitto ti prende quando ci arriva la testa, non i piedi: saltandoci o volandoci
  dentro, la vista non entra più prima nel soffitto. (Le pareti andavano già con la testa, i
  pavimenti con i piedi, che ci arrivano per primi.)
- `tools/portallab.sh`: una scena per controllare i portali a occhio -- vetro attraverso
  un'apertura in parete e una nel pavimento, un'apertura nel soffitto in cui saltare.
- Uscendo da un portale la vista non viene più girata: guardi dove il passaggio porta il tuo
  sguardo, e viene raddrizzato solo l'orizzonte inclinato, con un'animazione morbida tra un
  quarto e poco più di mezzo secondo. (La telecamera del kart si livella ancora.)
- Camere di prova: un cubo portato in mano attraverso il campo dissolvente non resta più in mano;
  si disintegra lì, elettrificato (consumato con i bordi che bruciano di blu, scintille), e gli
  altri lo vedono disintegrarsi dov'era. Il tubo del distributore di cubi non sfarfalla più visto
  da sotto.
- **Portali fra server costruiti nel mondo**: un anello d'oro di `portal:server_frame` (otto
  cornici di portale attorno a un rubino). Il clic destro di un admin apre il suo menù: il nome, i
  server amici per cui è disponibile e i portali che quelli offrono a questo server, un clic per
  collegarsi. L'altro server accetta il collegamento quando il suo portale è offerto a sua volta;
  prima entrambi controllano di sentirsi, di parlare un protocollo in comune e che i fori abbiano
  la stessa misura. I server amici lo condividono come `portals` (attivo per i nuovi amici; un
  amico vecchio va aggiunto di nuovo). Un collegamento fatto dura anche con uno dei due server, o
  entrambi, spenti, e cade solo quando un admin lo lascia da una delle due parti o cambia la
  chiave di firma di un server. Tre caselle nel menù fanno a meno di un controllo ciascuna (l'altro
  server online, un protocollo in comune, fori della stessa misura), ognuna con il suo avviso.
- `/friend add` con un nome già presente sostituisce quell'amico: un server la cui chiave è
  cambiata si aggiunge di nuovo con lo stesso nome.
- Camere di prova 11-13: la lunga caduta (in un portale sul fondo di un pozzo, fuori da uno nel
  pavimento verso l'alto, su una sporgenza), tre torrette sentinella dal disprezzo facile (si
  abbattono con un cubo lanciato o prendendole da dietro) e un campo dissolvente che chiude i tuoi
  portali e dissolve quello che porti. Zone di trigger aprono porte e portali e svegliano le
  torrette.
- Parkour: il pad di rimbalzo mira il lancio sull'approdo, da dovunque lo tocchi.
- Action RPG: si arriva in una sala con la telecamera normale, e la vista dall'alto parte dalla sua
  porta; le vie di casa e di ritorno guardano la telecamera. Armature, elmi, guanti, stivali e
  cinture calzano all'umano, all'uomo-talpa e all'uomo-rana.
- I binari salgono e scendono un gradino da soli, dai binari accanto.
- Assalto: un carro armato costa 220 (era 350), e uno bloccato in una buca viene sollevato fuori.
- Il sindaco di un villaggio in un mondo con la piazza dice cosa pensa il villaggio degli archi.
- Il testo del launcher è più compatto: caratteri più piccoli, due colonne, più cose in una schermata.

## 0.11.0-alpha.1 — 2026-11-01

Protocollo 113: non gioca con server o client 0.10 (il launcher scarica la versione che un server
vuole). Legge i mondi della 0.10.

- **Il launcher** (`cnp_launcher`), ora quello che ogni pacchetto apre, con l'aspetto della
  schermata del titolo:
  - **Versioni**: quella del pacchetto e altre scaricate dalla pagina delle release, una accanto
    all'altra; una è la predefinita che Gioca avvia. Un download parte solo se il
    `release.manifest` firmato della release lo elenca e dimensione e hash corrispondono; una
    versione si toglie con un secondo clic che dice quanti mondi ha creato.
  - **Mondi**: si giocano dall'elenco; uno di un'altra serie chiede prima, e una versione più
    nuova copia il mondo in `backups/` prima di aprirlo.
  - **Mod**: ovunque o per mondo, quelle del gioco (sola lettura) e le tue; cosa serve a
    ciascuna, cosa chiede, accese e spente, tolte, e aggiunte trascinando una cartella o un
    `.cnpmod` sulla finestra.
  - **Seguire un server**: un server segnato "needs version X" (o che rifiuta questa versione)
    rimanda il gioco al launcher, che avvia X, scaricandola se serve, su quel server.
  - Il gioco da solo c'è ancora: `cnp_client`, oppure `AppRun game` / `flatpak run ... game`.
- Le tue mod: `~/.cubes-and-portals/mods` (Windows `%APPDATA%\cubes-and-portals\mods`) si carica
  nella sandbox di ogni mondo; la cartella `mods/` e il `mods.conf` (`off = a, b`) di un mondo
  aggiungono mod e le spengono lì. Server: `--user-mods DIR`, `--no-mod NOME`.
- I piedistalli mostrano cosa contengono, sospeso e rotante sopra di loro (`core.show_item`): la
  pistola delle camere, i frammenti delle rovine, la prossima chiave dell'altare.
- `/giveup`: abbandona la prova di un mondo; muori e torni nella sandbox fuori dal suo portale.
- I comandi della chat sono in inglese (`/journal`, `/giveup`, `/town`, `/pieces`, `/vehicle`), e
  ogni comando è in `docs/COMMANDS.it.md`.
- Lo stato di un server dice quali protocolli parla.
- Parkour, la corda: il server sa dove si aggancia e quando viene lasciata, e la discesa
  dell'oscillazione non è più una caduta. Prima ogni oscillazione toglieva salute, qualcuna
  uccideva, e la rinascita lasciava la corda agganciata: il corpo scattava di nuovo su di essa a
  metà oscillazione.
- Parkour: corpo e visuale girano attorno a un angolo interno nella corsa sul muro, il salto
  d'angolo può venire prima dell'angolo, e le strisce di pericolo continuano da un blocco all'altro.

## 0.10.0-alpha.2 — 2026-11-01

Protocollo 111, come la 0.10.0-alpha.1: gioca con i suoi server e client e ne legge i mondi
(i loro smeraldi si caricano come rubini).

- `/time [day|noon|dusk|night|midnight|dawn|HH:MM]` (admin), `/kill [nome]` (sé stessi per tutti,
  un altro per un moderatore) e `/creative on|off`: modalità creativa, volo senza jetpack,
  nessun danno, scavo veloce; libera in un mondo tuo, da admin su un server. Funziona anche nel
  mondo `--quick`, e lì alchemy torna a caricarsi.
- Lo smeraldo ora è il rubino, ovunque: minerale, gemma, blocco, la moneta dei villaggi, pietre
  di viaggio, cippi di confine, bottini. Cremisi, per non confonderlo col cinabro. I mondi salvati
  prima caricano i loro smeraldi come rubini.
- Il Flatpak si aggiorna: installato una volta dal repository
  (`flatpak install --user https://giacomo986.github.io/CubesAndPortals/cubes-and-portals.flatpakref`),
  `flatpak update` porta ogni versione nuova. Il repository è firmato e sta su GitHub Pages;
  il `.flatpak` delle release si installa ancora, ma da file non si aggiorna.
- Le release delle alpha e delle beta escono segnate come pre-release.
- La documentazione e questo changelog sono in inglese, ognuno con il suo gemello italiano
  accanto (`<nome>.it.md`).
- La lista dei mondi salvati nel menù del titolo segna quelli creati da una versione precedente:
  un bordo ambra e la versione che li ha creati, o "versione vecchia" per uno di prima che i mondi
  la registrassero (il nuovo file `version` del mondo, scritto una volta quando nasce).
- Parkour: una corsa sul muro fermata da un muro di traverso (un angolo interno) gira su quel muro
  con un salto entro 0,4 s (`core.parkour`). Il percorso del parkour ha due stazioni in più prima
  della torre: due travi per la corda in fila, e una corsa sul muro attorno a un angolo interno;
  poi due travi ad angolo, tre a S e tre a U.
- L'Inferno: i suoi bottini servono (lingotti d'oro, resistenza al fuoco all'alambicco, polvere di
  tizzone per un accendino e lanterne di brace, blocchi di quarzo e comparatori); campi di basalto,
  foreste di funghi e deserti di cenere; bauli lungo le fortezze; le palle di fuoco accendono fuochi
  brevi (nuovo evento `core:burst`); suoni e cenere nell'aria; e una cornice accesa nell'Inferno vi
  porta la via di ritorno, con la gemella costruita otto volte più lontano nel mondo di sopra.
- L'Inferno da un sito sopravvivenza è un livello del sito, separato dall'Inferno aperto: una
  fortezza di bracieri secondo d (gallerie, un ponte sulla lava, una galleria di spettri, colonne
  di basalto, un braciere sotto la lava, il buio), il guardiano nella sua sala, e una via di ritorno
  alla partenza. Il cuore della piazza è sempre d 1. In un livello una cornice di ossidiana non
  porta da nessuna parte e i bauli danno meno.
- Liquidi: le superfici oscillano (l'onda ombreggiata dalla sua pendenza, la lava più lenta), e
  ogni loro faccia ondeggia un poco di lato, anche le colonne piene; un angolo contro un blocco
  resta fermo, così il liquido aderisce al blocco senza fessure né sovrapposizioni. La
  lava è un poco trasparente: ciò che sta sotto sfuma nel suo colore con la profondità, quasi
  sparito a otto nodi, e lontano dall'occhio è opaca. L'opacità di un liquido ora limita l'alpha
  della sua texture (le immagini delle lave erano opache qualunque cosa dicesse la mod); le lave
  sono a 130.
- `tools/track.sh --fresh` fa del giocatore il proprietario: il catalogo creativo (C) è aperto.
- Angurie e zucche si coltivano: una fetta è un seme d'anguria, una zucca quattro semi di zucca;
  seminato sulla terra arata, un gambo cresce in quattro stadi come il grano e, cresciuto, mette
  un frutto in una cella libera accanto, uno alla volta.

## 0.10.0-alpha.1 — 2026-10-31

Protocollo 111 (110 i binari, 111 fino a 24 bocche per stanza): non gioca con i server della
0.9 né i loro client con i suoi. Legge i mondi della 0.9.0-alpha.4; un mondo nuovo può essere
sandbox o survival.

- Modalità **survival** (menu nuovo mondo, `--survival`): niente piazza, spawn casuale su terra,
  i mondi a tema dietro portali sparsi con difficoltà 1-10 che cresce con la distanza, un livello
  per ogni portale (parkour e backrooms generati; arena, inferno, fronte e assalto scalati), uscita
  nascosta che si apre a livello finito, cassa di ricompensa al ritorno, chiavi solo da difficoltà
  4, laboratorio in un punto casuale. Dicerie e indicazioni (a pagamento dal sindaco) sui portali
  vicini. Niente catalogo creativo e borsa in una tomba alla morte. Nuove API `core.seed`,
  `core.plaza`, `core.bag_to_container`, evento `core:died`; `/perm PERMESSO nobody`.
- Il catalogo (C) chiede il permesso `creative` (moderatore in su; nel mondo proprio il proprietario).
- I carrelli sui binari: curve, salite e discese, binari alimentati dal cinabro (stazioni da
  spenti), scambi. I binari si percorrono nei due sensi e le curve si vedono curve; le ruote
  fanno rumore sui giunti.
- Le pietre del viaggio: una per villaggio accanto al pozzo, fabbricabili per i territori;
  pagando uno smeraldo ogni 200 nodi si apre un portale vero verso una pietra toccata, per
  cinque minuti.
- Il pannello creativo mostra di nuovo zaino, jetpack e armpack: un contenitore portato rifiutava
  tutto ciò che si porta sulla schiena, anche nelle caselle del catalogo.
- Sentieri fra i villaggi rifatti: una rete (ogni villaggio ai vicini), curve morbide, fondo per
  terreno (arenaria, ciottoli, ghiaia, terra battuta) con bordi irregolari, cartelli alle uscite,
  pietre miliari con lanterna ogni 100 blocchi. I mondi già esplorati tengono i sentieri vecchi.
- Carovane da villaggi non ancora visitati, con i prodotti della loro terra, lungo i sentieri; il
  mercante indica la strada a voce e la segna sulla mappa. `/village incoming`.
- Eventi dei villaggi: carovane mercante e mulo fra i villaggi (si compra o si scorta), assedi
  notturni rari con preavviso e ricompensa, festa del raccolto a fine estate con bancarelle, musica
  e fuochi d'artificio (nuovo `mods/fireworks`). Annunci con la campana e sulla bacheca;
  `/village` per gli admin. `farm:season` porta anche i giorni rimasti.
- Il tavolo degli scacchi gioca anche a dama italiana e a mulino; il gong dell'arena avvia sfide PvP
  di tre minuti con punteggio e classifica. Nuova API `core.body_player`.
- Modelli in mano e icone per tutti gli oggetti e i blocchi nuovi che non li avevano (le sedie
  nell'inventario non sono più quadrati di assi).
- Decorazioni: vetro colorato e lastre nei sedici colori, muretti, piastrelle smaltate, a scacchi
  e di pietra, sedie (ci si siede, e il corpo non gira con la telecamera) e tavoli per ogni legno,
  tappeti, lanterne in ferro e vetro. Dal carrello si scende anche con F.
  `core.player_clip` accetta una direzione in cui il corpo resta girato.
- Le ferrovie rifatte in Blender: traversine e rotaie vere, curve ad arco, salite su terrapieno,
  binari alimentati che si accendono, scambi con leva; un vero carrello da miniera; il giocatore ci
  sale scavalcando il bordo e viaggia seduto; in prima persona si vedono il carrello e le mani sui
  bordi, e il carrello resta nella direzione dei binari quando si gira la visuale. Un giocatore ora vede anche su di sé la posa e ciò che
  indossa per una mod del server (messaggio OwnPose).
- I progetti: copiare una costruzione, tracciarla come fantasma, farla costruire a mano, dal
  costruttore del villaggio o da un robot. Nuove API: `core.held_id`, `core.held_set_id`, `id` in
  `core.give_item`, `core.ghost_cell`, `core.ghost_clear`, `core.node_colour`.
- Le commissioni dei villaggi: sei tipi, una bacheca accanto al pozzo, pagate dalla tesoreria,
  uno sconto dal mercante dopo cinque.
- L'allevamento: pecore tinte (lana colorata), latte (formaggio e torta; il secchio resta), mangiatoie che fanno da stalla, l'ordine
  "Bada agli animali" per il robot. Nuove API: `core.mob_kind`, `mob_love`, `mob_keep`,
  `mob_shear_now`, `mob_become`, `mob_voice`; `core:used_mob` anche sugli animali.
- La pesca per acqua: persici, carpe, trote, merluzzi, anguille, tonni e un raro pesce rosso;
  tutti si cuociono e il pescatore-rana li compra.
- L'alchimia: un alambicco, le fiasche e cinque pozioni (vista notturna, rapidità, pelle di
  pietra, guarigione, fretta, respirare sott'acqua). Si annega: quindici secondi d'aria
  sott'acqua, poi si perde salute. Nuove API: `core.night_sight`, e una fonte per
  `core.player_factors`, così gli effetti di mod diverse si sommano.
- Le Cavità sotto la roccia madre: caverne luminose, lava, cristalli, lurker e un guardiano.
  Il mondo ora tiene blocchi fino a y -64.
- Un chunk sotto il giocatore arriva solo dopo quello sopra: niente più caverne viste dall'alto
  attraverso il terreno non ancora arrivato.
- Strutture rare con un enigma: templi nel deserto, relitti, rovine sommerse.
- La bussola che punta a casa e la mappa che si riempie esplorando.
- I portali non hanno più un massimo per mondo: il server tiene attivi i 32 più vicini ai
  giocatori. Le cornici di portale non finiscono più a 16.
- La barca dondola sull'onda. Barca e carrello hanno un'icona.
- I mattoni di pietra si fanno con la pietra liscia: la ricetta vecchia era quella dei mattoni.

- Il sandbox ha il meteo: pioggia, neve nella tundra e sulle vette, temporali con fulmini e
  tuono. La pioggia innaffia i campi.
- Le stagioni: foglie che cambiano colore, campi fermi d'inverno senza lampade, neve
  ovunque d'inverno. I mercanti raddoppiano il cibo d'inverno e la legna d'autunno.
- Le barche: cinque assi, tasto destro sull'acqua per salire, su una riva per scendere. Vanno
  al doppio del nuoto, sterzano con A e D, scansano le creature, rimbalzano fra loro e si
  sentono i remi.

## 0.9.0-alpha.4 — 2026-10-27

Protocollo 109, come la 0.9.0-alpha.2: gioca con i suoi server e legge i suoi mondi. La
0.9.0-alpha.3 non è uscita: il pacchetto macOS non si costruiva.

- Chi tiene un server ha un'amministrazione completa: ruoli, ban, kick, mute, lista d'accesso,
  console, RCON, riavvii programmati, manutenzione, messaggio del giorno, regole da accettare,
  rollback dei blocchi, backup e webhook. Il manuale è [docs/ADMIN.md](docs/ADMIN.it.md).
- I server amici condividono ban, presenza, chat e lo zaino: chi passa da un portale verso un
  amico porta con sé lo stesso zaino.
- La lista Multiplayer mostra giocatori, versione e messaggio del giorno di ogni server.
- Il titolo avvisa quando su GitHub c'è un'alpha più nuova.
- Il README è anche in inglese.

## 0.9.0-alpha.2 — 2026-10-26

Protocollo 109, come la 0.9.0-alpha.1: gioca con i suoi server e legge i suoi mondi.

- Il gioco parla italiano e inglese: Options, Language sceglie la lingua (all'inizio quella del
  sistema). Una lingua nuova è un file in `assets/lang/`.
- In Multiplayer l'account è del server: si sceglie il server, poi nome e password per quello.
- Nella fuga dell'assalto i bot a bordo sparano alle trappole al momento giusto.
- La piazza dello spawn è silenziosa: niente più lampada che lampeggia né fari che si
  accendono a turno.
- I pacchetti non contengono più modelli di terzi, e si costruiscono da un clone del
  repository; il codice è pubblico, con licenza MIT, e i sorgenti Blender dei modelli sono in
  `assets/blender/`.

## 0.9.0-alpha.1 — 2026-10-25

Protocollo 109: non gioca con i server della 0.8.0-alpha.1. Legge i suoi mondi.

- In rete, nei mondi con tanti soldati (l'assalto, il fronte), soldati e giocatori non saltano più
  da un punto all'altro: i corpi viaggiano in pacchetti piccoli, così un pacchetto perso ne ferma
  pochi e per un attimo; chi esce dalla vista sparisce subito invece di restare fermo; se ne vedono
  fino a 96.
- Il server scrive nel log quando non sta al passo, e perché.

## 0.8.0-alpha.1 — 2026-10-25

Protocollo 108: non gioca con i server della 0.7.0-alpha.5. Legge i suoi mondi.

- Nuovo mondo, **l'assalto**, da giocare insieme contro Basoalth (dalla piazza del sandbox, a
  90.5 21 76.5): quattro fortezze da prendere una dopo l'altra, crediti e negozio, alleati che
  escono dalle caserme, costruttori che riparano, mettono scale sulle mura e nidi di sacchi, bot
  che fanno saltare il cancello della fortezza; la sala del nucleo con i piloni; Basoalth che
  diventa un gigante, il pavimento che crolla e la fuga in jeep in un tunnel di lava con curve,
  salite e discese, valvole, massi e serbatoi da colpire per rallentarlo.
- La valle dell'assalto: cielo rosso con nuvole a vortici che girano, tornado che risucchiano e
  scagliano via, una catena di vulcani con colate di lava, un fiume di lava con un ponte e isolotti.
- Chi muore torna con tutte le armi e le munizioni piene.
- Sui mezzi: i gradini hanno rampe più lunghe e si salgono senza fermarsi (in tutti i mondi);
  da passeggero si spara con la propria arma e 1-9 la sceglie; seduti si sta seduti con l'arma,
  alla torretta con le mani sull'arma, girati dove si guarda.
- Mezzi e obiettivi non spariscono più quando ci sono molti soldati in giro.
- Dietro il vetro si vede quello che c'è (prima i corpi ci passavano davanti); le nuvole non sono
  più a puntini.
- Kart: derapata con Ctrl, segni delle gomme sulla strada, fiamme del nitro, e un garage per farsi
  il proprio kart.
- Rovine: una città, tre zone, classi e oggetti con proprietà; barili, casse e urne da rompere;
  incantesimi e colpi con effetti nuovi.
- Parkour: la corda va dove sta il mirino, anche in terza persona.
- Borsa e creativo con caselle più piccole e la barra sotto; un baule catalogo.
- Il mazzo illustrato delle carte ha nuove carte di denari, coppe e spade.
- I passi si sentono meno.

## 0.7.0-alpha.5 — 2026-10-18

Protocollo 100: non gioca con i server della 0.7.0-alpha.4. Legge i suoi mondi.

- Si entra nel fronte da una tribuna sul crinale, a metà fra le basi e in alto sul campo, con una
  vetrata da cui si guarda la battaglia. Due portali, uno rosso e uno blu, portano nel bunker
  della base: da quello che si attraversa si sceglie la squadra. Dal bunker si torna in tribuna.
- La battaglia dei bot parte anche se c'è solo chi guarda dalla tribuna.
- I mezzi sono solidi: due mezzi si urtano invece di passarsi attraverso, chi è a piedi non entra
  nello scafo, ci sale sopra e viene portato.
- Chi guida vede il mezzo muoversi subito, senza aspettare il server: il client lo simula e lo
  allinea a quello del server.
- I soldati alle armi fisse (contraerea, mitragliatrice, mortaio) guardano dove punta l'arma.
- Corretto un crash del server quando due mezzi nascevano nello stesso istante.
- Il mazzo illustrato delle carte ha anche il 2, il 3 e il 4 di denari.

## 0.7.0-alpha.4 — 2026-10-18

Protocollo 99, come la 0.7.0-alpha.3: gioca con i suoi server e legge i suoi mondi.

- Al fronte si mira: col destro l'arma sale al centro della vista, i colpi vanno dove si guarda
  e si cammina piano. Nuovo fucile da cecchino con cannocchiale. Dal fianco i colpi si allargano.
  Le armi si vedono impugnate, puntate, sparate e ricaricate anche da fuori.
- In terza persona i colpi vanno dove sta il mirino, anche se c'è un soldato o un mezzo davanti;
  passando dalla prima alla terza persona e viceversa il mirino resta sullo stesso punto.
- I soldati bot sono più alla portata: vedono solo davanti a sé, aspettano un attimo prima di
  sparare, sparano a raffiche meno precise e fanno meno danno. Combattono in quattro modi
  (sentinella col cecchino, assaltatore con la mitragliatrice, prudente che si ripara, anticarro
  col lanciarazzi), tengono la loro arma finché ha colpi e la si vede nelle loro mani.
- I bot usano i mezzi: guidano jeep e carro, pilotano elicottero, aereo e bombardiere, prendono
  nidi, mortai, obici e contraerea, e salgono sui mezzi guidati dai compagni, anche dai giocatori.
- Nuovi mezzi: il bombardiere (una fila di sei bombe, con il punto d'impatto segnato, e una
  torretta sul dorso) e la contraerea, che aggancia elicotteri e aerei e lancia missili che li
  inseguono.
- Sui mezzi: F fa salire sul posto libero più vicino (da dietro la jeep si sale dietro) e 1-9
  cambiano posto senza cambiare l'arma in mano. I vetri dei mezzi sono trasparenti.
- I pannelli di vetro sono trasparenti.
- Il fronte sarà il premio di chi batte l'Ospite nel laboratorio; per ora resta aperto a tutti.

## 0.7.0-alpha.3 — 2026-10-18

Protocollo 99: non gioca con i server della 0.7.0-alpha.2.

- Nuovo mondo, il fronte: un campo di battaglia fra una base rossa e una blu, con tre punti da
  conquistare (fattoria, villaggio, bunker sulla collina) e 150 ticket per squadra. Si entra
  dalla porta-bunker fra i sacchi di sabbia sulla piazza. Bot soldato riempiono le squadre fino
  a 8.
- Al fronte: fucile e mitragliatrice automatici, pistola, granate e lanciarazzi. Le esplosioni
  scavano il terreno e fanno crollare quello che resta sospeso.
- Mezzi: jeep a quattro posti con mitragliatrice, carri armati che salgono i gradini e sfondano
  i muri, elicotteri con razzi, aerei con mitragliatrici e bombe, nidi di mitragliatrice,
  mortai e obici che tirano dove punta il mirino. Si sale e si scende con F, 1-9 cambiano posto,
  il mouse mira e governa quello che vola. Un mezzo distrutto brucia, perde i pezzi e sprofonda.
- Seduti su un mezzo il corpo resta solidale con lui; alla guida della jeep le mani stanno sul
  volante, e in prima persona si vedono solo le braccia.
- Carte: le carte giocate, prese, pescate e distribuite volano sul tavolo una alla volta;
  il cursore su una carta giocata la solleva e dice chi l'ha giocata.
- Corretto: dopo essere scesi da un mezzo o morti dentro, il server poteva smettere di
  accettare i movimenti (granate che non partivano, colpi che non arrivavano).

## 0.7.0-alpha.2 — 2026-10-17

Protocollo 97: non gioca con i server della 0.7.0-alpha.1.

- Multiplayer: gli altri giocatori e le creature si muovono fluidi, non più a scatti, scivolando
  da una posizione ricevuta alla successiva.
- Multiplayer: chi vola è visto volare (non più camminare quando vola rasoterra).
- Volo: il corpo si inclina nella direzione in cui vola e plana quando si lascia andare.
- Pista: icone per il jetpack e gli attrezzi del cronometro.
- Carte: seme di bastoni, dorso e asso di denari illustrati.

## 0.7.0-alpha.1 — 2026-10-17

Protocollo 96: non gioca con i server della 0.6.0-alpha.2.

- Personaggio (Character): oltre al robot, umano, uomo talpa e uomo rana, ognuno con tre stili
  (capelli corti, lunghi, barba; lampada da minatore, occhialoni, occhialini; macchie, corona,
  ninfea), i tre colori e gli accessori di prima. Stesso scheletro e clip del robot, braccia
  proprie in prima persona. Gli accessori sono rifatti su misura per ogni specie (la rana porta il
  cappellino all'indietro); un accessorio in testa prende il posto di lampada, corona o ninfea.
- Kart: zaino, armature e oggetto in mano stanno sul pilota, non più sul kart (il jetpack finiva
  sulla testa e in prima persona copriva la vista).
- Kart: si scende con F, non più col tasto destro, che faceva scendere per sbaglio.
- Personaggi scolpiti in Blender: umano, talpa e rana sono superfici continue e levigate (non più
  primitive incollate) che si piegano a gomiti e ginocchia; i capelli coprono la nuca. Medaglia e
  capi della sciarpa oscillano solo in avanti, senza entrare nel petto.
- Attrezzi: piccone, ascia, pala, spada e zappa in legno, pietra, ferro, oro e diamante, ognuno
  con la sua forma; si consumano con l'uso.
- Zaino a braccia: uno zaino di metallo con due braccia snodate, un attrezzo in ogni pinza; scava
  col migliore fra mano e braccia e somma le spade.
- Villaggi: ogni lavoratore prende dai bauli l'attrezzo migliore del suo mestiere, l'artigiano fa
  attrezzi di ferro, e si comprano in ferro e diamante.
- Editor delle piste: B (o il pulsante "pick from afar") sceglie da lontano; al posto della hotbar
  una griglia con tutti i pezzi, clic per provarne uno, W per tenerlo. A/D non si bloccano più
  togliendo curve sopraelevate e muri, che prima restavano piazzati.
- Pezzi di pista: si piazzano anche nel terreno, che viene tolto (e 3 blocchi sopra la strada);
  li ferma solo un altro pezzo.
- Jetpack: il suono è molto più basso e sale appena mentre ci si muove.
- ARPG: la stanza d'arrivo è sicura, le creature restano fuori; W A S D interrompono la camminata
  col clic.

## 0.6.0-alpha.2 — 2026-10-14

Gioca con i server della 0.6.0-alpha.1 (protocollo 93); i mondi salvati si leggono.

- Pista: casse dei potenziamenti dopo ogni traguardo, come nei giochi di kart: turbo, triplo
  turbo, banana, guscio, guscio a ricerca, inchiostro, stella, fulmine. Se ne tengono due.
- Editor delle piste (in /pezzi): clic destro su un pezzo propone quelli che si attaccano alla
  fine aperta, Undo toglie l'ultimo, Close circuit chiude il circuito da solo, prima in piano.
  I pezzi in mano e per terra si vedono in miniatura.
- Pista: lo strumento delle casse, il pannello creativo e il jetpack per volarci sopra.
- Pista: giri della morte alti il doppio; il kart non inciampa più uscendo dai giri.
- Luce contro il buio: F8 mostra dove escono le creature, i robot le illuminano, un'ancora li
  tiene al lavoro.
- Aiutanti: una cava con la sua scala per uscire, e Close sul pannello del programma.
- Portali: tra stanze dello stesso server niente biglietto, e un biglietto scaduto si rinnova
  invece di essere rifiutato.

## 0.6.0-alpha.1 — 2026-10-12

Non gioca con i server della 0.5 (protocollo 93); i mondi salvati si leggono ancora.

- Il robot si personalizza: Robot, nel menù iniziale o in pausa, sceglie colore primario,
  secondario e accento (16 colori) e un accessorio in testa (cappellino, cilindro, cuffie) e uno
  al collo (sciarpa, papillon, medaglia), con il robot che gira accanto. Gli altri giocatori lo
  vedono, anche quando si cambia a partita in corso.
- Luce nuova: buio vero, luce morbida, lampade che illuminano le facce rivolte verso di loro,
  sole e luna con le ombre. La luce di prima resta nelle opzioni (Light: classic), e le ombre
  si possono abbassare o spegnere.
- Schermo intero: F11 o nelle opzioni.
- Carte: al tavolo il robot si siede e tiene le carte in mano.

## 0.5.0-alpha.1 — 2026-10-11

Non gioca con i server della 0.4 (protocollo 91); i mondi salvati si leggono ancora, e le zone
già generate tengono il terreno di prima.

- Mondo: oceani, mari, spiagge, fiumi, laghi in quota e gole, con ponti dove i sentieri li
  attraversano; sotto terra miniere abbandonate e cripte.
- Le uscite dei mondi sono sigillate in rosso finché non si vince quel mondo; gli archi sulla
  piazza portano all'uscita di ogni mondo.
- L'Inferno carica i mod di gioco del sandbox: quello che si porta in borsa vale anche lì; fame,
  esperienza e chiavi sono le stesse in tutte le stanze.
- Pista: una nuova pista con tre giri della morte, la telecamera che segue il kart nei giri, e
  niente lampo verde attraversando l'Inferno.
- Pista dell'Inferno: tolta una cornice di ossidiana in più nel portale di uscita; scendendo e
  risalendo sul kart nell'Inferno si torna sulla pista disegnati giusti, e la vista scelta resta.
- Scacchi: otto varianti. Parkour: sempre di corsa, e ci si aggrappa ai bordi.
- Un mondo nuovo parte in circa 25 secondi (erano 80), con una barra sul titolo.

## 0.4.0-alpha.1 — 2026-10-10

La prima alpha consegnata.

- Pacchetti: Linux (tarball, AppImage, Flatpak), Windows (zip), macOS (`.dmg`, mai provato su un Mac).
- La versione è scritta nell'angolo in basso a destra del menù: va indicata quando si segnala un bug.
- Su Windows: niente stack dump in caso di crash, e tornando al menù va rimessa la password.
