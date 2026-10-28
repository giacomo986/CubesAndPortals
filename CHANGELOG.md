# Cambiamenti

Una voce per ogni alpha consegnata, la più recente in cima. Come si numerano e come si fa
un'alpha: [docs/BUILDING.md](docs/BUILDING.md#versioni).

## Non ancora uscito

Protocollo 110: un client 0.9.0-alpha.4 non entra in un server con i binari.

- I carrelli sui binari: curve, salite e discese, binari alimentati dal cinabro (stazioni da
  spenti), scambi. I binari si percorrono nei due sensi e le curve si vedono curve; le ruote
  fanno rumore sui giunti.
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
  rollback dei blocchi, backup e webhook. Il manuale è [docs/ADMIN.md](docs/ADMIN.md).
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
