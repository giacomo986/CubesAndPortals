# Cubes and Portals

Sandbox voxel scritto da zero, con i **portali come primitiva dell'engine** invece che come
innesto: near plane obliquo (Lengyel), clip plane laterali, ricorsione, e un client capace di
tenere **più mondi vivi contemporaneamente** — così un portale può mostrare, e far attraversare,
un altro server senza schermata di carico.

Nasce dall'esperienza di un fork precedente di un altro engine voxel, dove
tutto questo funziona già, ma combattendo un'architettura che non era pensata per due mondi
nello stesso processo. Qui l'ordine è invertito: l'architettura parte dai portali.

## Stato

**M0 — spike di validazione superato.** Il fork precedente è congelato (tag
`frozen-2026-08-14`).

Dei tre criteri di uscita di [`docs/DESIGN.md`](../DESIGN.md) §8:

| Criterio | Stato |
|---|---|
| 1. Attraversamento visivamente continuo | **Passato** — l'immagine nel portale coincide con la vista di riferimento dalla camera virtuale |
| 2. Portale corretto con la camera dentro la cornice | **Passato** — la guardia disattiva il near plane obliquo e tiene i clip plane |
| 3. ≥ 200 FPS con 2 portali a 1080p, scripting sotto l'1% del frame | **Passato** — 1567 FPS (0.638 ms/frame) su RX 5700, con Luau a 0.0021 ms/frame (0.33% del frame) |

**M1 — core voxel client, superato.** Chunk store sparso, greedy meshing, vertice da 8 byte, luce
flood-fill, AO, frustum culling, raycast e piazzamento/rottura. Mondo 512×64×512 a **2677 FPS**
contro un criterio di 60; dieci minuti di volo con un milione di edit, memoria piatta al decimo di MB.

**M2 — modding di prima classe, superato.** Una VM Luau per mod con
quota di memoria e budget di interruzione, capability applicate *per rimozione* prima che la
tabella globale venga congelata, hot reload che costruisce la VM nuova prima di distruggere la
vecchia, event bus tipizzato su cui il motore emette senza percorsi privilegiati, e i binding
generati da una sola tabella descrittiva. Il mod in loop infinito viene interrotto senza fermare
gli altri. **L'ultima casella, l'item dell'exit test, è chiusa in M7**: `core.register_item` esiste, `mods/palette`
registra i suoi venti blocchi **e** un `palette:brush`, e `mod_reload_test` lo fa alla lettera con un
mod fixture — venti blocchi e un item, ricaricato a caldo.

**M3 — server e rete, superato.** Mapgen deterministico con stage registrabili, server headless
che linka solo core e scripting, protocollo su ENet con handshake versionato e negoziazione
feature, tabella dei blocchi che il client **adotta** dal server, streaming chunk compresso con
zstd e interest management, autorità del movimento, replica entità e persistenza SQLite.

| Numero | Valore |
|---|---|
| Mondo su disco → sul filo | 3.00 MB → 0.01 MB tipico, **91 KB** un chunk incomprimibile |
| Handshake | 238 byte in uscita, 161 in entrata |
| Tick del server | 50.041 ms su 600 tick, con due client connessi |
| Quattro client che camminano | 0 correzioni, ~214 aggiornamenti entità ciascuno |
| `ctest` | **15 test**, di cui due richiedono una GPU |

**Il client di gioco entra in rete.** `cnp_client --connect <host>` prende il mondo dal server
invece di generarlo: niente mapgen locale, niente mod, tabella dei blocchi **adottata**, chunk che
arrivano mentre si gioca, altri giocatori disegnati, correzioni applicate. Sonda di test e gioco
condividono una sola `ClientSession`, che è il punto: un soak contro un secondo client misurerebbe
il secondo client. Dettagli e sabotaggi in [`docs/M3_FINDINGS.md`](M3_FINDINGS.md) §14.

**Le sessioni sono cifrate.** X25519 con forward secrecy, XChaCha20-Poly1305 su ogni pacchetto,
finestra anti-replay per canale, chiave del server appuntabile con `--server-key`. Sta *sotto* il
protocollo: `Transport` non riporta un peer come connesso finché non c'è una chiave, quindi non
esiste un percorso che salti il sigillo — e il test del protocollo continua a girare su buffer
senza socket. Costa **25 byte a pacchetto**, misurati. Senza pin si è protetti da chi ascolta ma
non da chi si mette in mezzo, e il client lo dice a voce. Dettagli in
[`docs/M3_FINDINGS.md`](M3_FINDINGS.md) §15.

Non c'è ancora modifica dei blocchi *attraverso la rete*, il salvataggio avviene allo spegnimento
e non incrementalmente, e le chiavi d'identità del **giocatore** (§4.4) sono M6. Tutto elencato in
[`docs/M3_FINDINGS.md`](M3_FINDINGS.md) §13 invece che lasciato scoprire.

**I portali li piazzano i giocatori, e li possiede il server.** Click sinistro e destro mettono i
due capi sulla faccia del blocco che si sta guardando; il messaggio porta *il blocco e la faccia*,
non l'apertura, quindi non c'è un campo in cui un client modificato possa appendere un portale a
mezz'aria. Il server controlla portata (7 nodi contro i 6 del braccio), che ci sia qualcosa su cui
appoggiarlo e che ci sia spazio davanti, poi ricava forma e verso e li manda a tutti. Da lì
l'attraversamento lo decide lui e concede la grazia che rende legale il salto — anche per
un'apertura che nessun file di configurazione ha mai visto. `cnp_server --portals` ne mette una
coppia nel plaza per partire con qualcosa. Misure e sabotaggi in
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §3.

Un'apertura **a pavimento è il blocco che hai cliccato**, non la cella sopra: qualcosa deve
reggerla, ed è terra solida esattamente dove devi passare. Il server accetta di vederti sprofondare
in *quel* nodo e in nessun altro — un nodo, tutto dentro, altrove la regola non cambia. Piazzarne
una sotto i propri piedi adesso ci fa cadere dentro: 1 attraversamento e **0 correzioni** mentre si
sprofonda, contro 0 e 97 di prima. §4.

**Un giocatore è una chiave, non un nome.** Coppia di firma generata al primo avvio in
`player.key` (0600); il nome resta un'etichetta. Quello che si firma è il **channel binding** della
sessione, non il nome: una firma sul nome sarebbe una password valida per sempre e su ogni server,
mentre questa non vale niente fuori da quella sessione — misurato riproponendo una prova autentica
di un'altra sessione, che viene rifiutata. Un'identità presentata viene sempre verificata;
`--require-identity` decide solo se l'anonimo è ammesso, e senza cifratura il server non parte.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §5.

**Una sessione può guardare senza entrare.** `Scope::Observe` è quello che un portale apre nel
mondo di qualcun altro: riceve i chunk e vede la gente che c'è, non ha avatar in nessuna lista, non
può piazzare niente e non occupa un posto giocatore — capienze separate, perché un mondo pieno di
giocatori deve restare guardabile e uno pieno di spettatori giocabile. `--no-observers` chiude la
porta con un motivo suo. Manca ancora la sfera attorno all'uscita del portale, e sta scritto perché:
quel punto non esiste finché un portale non nomina un altro server.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §6.

**Quante connessioni servono, non due.** Il client teneva una sessione di rete e la scena due
mondi; nessuno dei due numeri veniva da qualcosa. Adesso le connessioni stanno in un
`ClientSessionSet`, si chiamano per id, e l'id sopravvive alla chiusura di quelle accanto — chi
tiene un id non deve ritrovarselo puntato altrove. Il pump spende il timeout **una volta**: cinque
socket a 40 ms l'uno sono 200 ms, cioè dodici fotogrammi a non fare niente. La scena tiene una lista
di mondi e ogni mondo dice *quale* altro apre, non «l'altro». Misurato con tre server veri e un
client: `--watch host:port`, ripetibile. Da quei mondi non si disegna ancora niente, ed è detto.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §7.

**Un'apertura può nominare un altro server.** `--portal-to host:port` fa della coppia una **bocca**:
una apertura di qua, e dietro il mondo di qualcun altro. La destinazione sta sul link, non
sull'apertura, e la geometria dell'altro capo non viaggia — sarebbe una copia che scade appena
qualcuno di là la sposta: il client la chiede a quel mondo, con una sessione passiva. Il client apre
la connessione da solo, con la stessa identità e la stessa cifratura del proprio join. Con una
destinazione il server **non fa attraversare nessuno**: spostare le coordinate metterebbe il
giocatore in questo mondo davanti all'apertura sbagliata, quindi per ora una bocca si guarda.
Misurato con due server veri. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §8.

**E lo si attraversa.** La sessione che guardava dietro l'apertura **diventa** quella giocante, con
un messaggio: riconnettersi vorrebbe dire farsi rimandare ogni chunk, cioè una schermata di carico
con un altro nome. Misurato: **32 chunk prima dell'attraversamento, 32 dopo**; arrivando come
connessione nuova sono 16 su 8. L'arrivo lo calcola il server che riceve, contro la *propria*
apertura, dopo aver controllato che la posa stia dentro l'apertura dichiarata — quello che un client
modificato può scegliere è dove nell'apertura esce, non in che punto del mondo. Due server veri che
si puntano a vicenda: `walked into the opening ... came out at 39.93 20.00 45.10 ... now playing on
:25422, watching :25421`. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §9.

**E si esce dall'apertura che il collegamento nomina.** Prima si usciva sempre dalla prima: non era
una regola, era quello che succede quando nessuno manda un numero. Adesso il numero sta sul link
(`--portal-to host:port/1`), viaggia al client dentro le aperture e torna dentro l'attraversamento;
il server che riceve sceglie *la propria* apertura e rifiuta un numero che non ha. Le due aperture
di prova stanno a 26 nodi l'una dall'altra apposta — «vicino a un'apertura» e «vicino a quella
giusta» non sono la stessa cosa. **E un rifiuto è un messaggio**: prima non arrivava niente e il
client aspettava due secondi per capirlo; adesso `refused us after 0.3 ms: this world is full of
players`. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §10.

**E un mondo può non ammettere nessuno.** La seconda porta di DESIGN.md 4.4: il server A firma
una dichiarazione su un giocatore — chi, per quale server, quale apertura, per guardare o per
passare, fino a quando — e B lo ammette perché si fida di A (`--trust <chiave>`, `--require-ticket`,
`--no-observers`). Ogni campo sta sotto la firma e ognuno è misurato da solo: ticket di un altro
giocatore, per un altro server, da un firmatario sconosciuto, scaduto, per il permesso sbagliato,
modificato dopo la firma — sei rifiuti, ognuno con la propria frase, e cinque sabotaggi che fanno
diventare rosso esattamente il proprio caso. Niente revoca e un orologio condiviso: detto, non
nascosto. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §11.

**E una finestra su un server che non c'è continua a provare, sempre più piano.** Prima faceva un
tentativo e restava in `timed out` per sempre: silenzioso perché aveva rinunciato, e un portale che
rinuncia non torna quando il server torna. Adesso l'attesa raddoppia fino a un tetto — **6 tentativi
in 6 secondi contro una porta morta, con 1185 pump**, e tre righe di log in quaranta secondi — e la
finestra si riapre da sola quando dall'altra parte qualcuno riaccende. Un *rifiuto* invece non si
riprova: «questo mondo non ammette osservatori» è una decisione, non un'interruzione, e solo
«server pieno» è transitorio. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §12.

**E un server che si pianta senza chiudere il socket viene notato.** ENet pinga il *socket*, e un
server con il loop bloccato risponde a quei ping mentre non manda niente: per il client è un mondo
tranquillo. L'heartbeat è 14 byte al secondo che dicono «sono qui» e **cosa è questo mondo adesso** —
`0/8 players, 1/8 windows, open to portals, room to walk in` — così un mondo pieno si scopre prima
di entrare nell'apertura e non con un rifiuto in faccia. Silenzio per N secondi e diventa
un'interruzione come le altre, con il backoff di sopra. Un server che non manda battiti non è un
server morto: il silenzio si misura solo dopo il primo.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §13.

**E se dietro l'apertura non c'è una finestra, si passa lo stesso — riconnettendosi.** L'attraversamento
senza schermata di carico richiede una sessione che *guardi già* di là, e ci sono motivi ordinari
perché non ci sia. Allora il server manda l'indirizzo (`Redirect`, con dentro un ticket, perché un
redirect verso un mondo che ne vuole uno è un redirect verso un rifiuto) e il client ci si riconnette:
stessi messaggi, stessa apertura, stessa posa — solo il mondo non era già arrivato. **32 chunk già in
mano contro 0**, che è la schermata di carico da cui il ripiego prende il nome. Sotto ci stava un
difetto: il trigger del server era campionato al ritmo delle richieste di posizione, più larghe della
fascia che guarda, quindi un giocatore in corsa **scavalcava il portale**.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §14.

**E i due mondi possono essere due *game* diversi.** Un client, due server, tabelle di blocchi
diverse: gli id si leggono attraverso la tabella della sessione da cui sono arrivati e nessun'altra.
Il controllo che c'era confrontava puntatori — due mondi con tabelle identiche lo passavano uguale.
Adesso è un ciclo su tutta la tabella, più il caso difficile costruito apposta: **id 3 è `core:grass`
in un mondo e `core:bedrock` nell'altro, nello stesso istante nello stesso processo**, e il terreno si
legge giusto perché lo si legge con la tabella giusta. Con un registry condiviso fra le sessioni:
sei controlli rossi. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §15.

**E i blocchi adesso hanno dei file.** Prima non c'era nessun media: il renderer generava una tile per
nodo da `colour`/`checker`/`opacity` a ogni atlante, ed è il motivo per cui la pipeline di DESIGN.md
4.5 non aveva niente da trasportare. `tools/bake_textures` carica le mod e scrive **31 PNG**; l'encoder
sta in `cnp_core` — che non include mai raylib — perché il *server* è la cosa che possiede un content
set. Generati e non scaricati apposta: alla pipeline servono file con un hash, non arte, e l'arte li
sostituisce uno per uno senza cambiare il trasporto. Il client dice da dove ha preso i pixel:
`atlas: 31 block(s) from a texture file, 0 from a generated placeholder`.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §16.

**E adesso viaggiano, una volta sola.** I media sono indirizzati per contenuto — BLAKE2b-256, non
BLAKE3 come dice DESIGN.md, perché monocypher è già linkato e la differenza costerebbe una dipendenza
per una proprietà che qui nessuno usa: sta scritto invece di essere taciuto. Il server offre `nome,
hash, dimensione` dietro il welcome; il client chiede solo quello che non ha; la cache su disco è
**una per client, non una per sessione**. Due server, 62 offerte, **31 file distinti, 31 scaricati**;
con `--different-games` la deduplicazione attraversa due *game* diversi. Il segnaposto si cuoce sul
**server**, e per questo un portale su un altro *game* mostra i suoi blocchi invece del magenta:
`the world through the opening: 26 block(s) this build has never heard of; its atlas is 31 from its
own media, 0 generated`. Ogni lettura dalla cache è ri-hashata prima di uscire, e un bit ribaltato sul
filo fa rifiutare il file: `62 refused`, zero byte corrotti su disco. Il sabotaggio che ha trovato un
difetto vero: con la cache che risponde sempre «non ce l'ho», il client scaricava **90 024 file e 368
MB in 25 secondi**; adesso ogni file si chiede una volta e il rosso costa 62 file.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §17.

**E la cache ha una dimensione.** 256 MB di default (`--media-cache-size <MB>`), sfratto per
**ultimo uso** e non per data di scrittura — una texture che ogni mondo manda è proprio il file che
questa cache esiste per tenere, ed è anche quello arrivato per primo. I file che la sessione in corso
sta usando non si sfrattano mai: un mondo più grande dell'intera cache altrimenti riscaricherebbe sé
stesso per sempre, quindi la cache va sopra il budget e lo **dice**. Contare la dimensione costa
**181 ms su 60 000 file / 236 MB**, una volta all'avvio; oggi con 31 file sono 0.05 ms. Il budget non
morde ancora — una run finisce con `31 file(s), 0.1 MB of 256 MB` — e c'è perché una cache senza
limite è un difetto che si manifesta quando non lo sta guardando nessuno.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §18.

**E la bocca sa cosa c'è di là.** L'heartbeat porta «quanti posti ha quel mondo e se ci si può ancora
entrare» da quando esiste, e il client lo stampava soltanto: la prima notizia che un mondo era pieno
era un attraversamento **rifiutato**, un round trip dopo che il giocatore era già entrato
nell'apertura. Adesso non parte niente: `walked into a mouth 5 time(s): 0 crossing(s) asked for, 5
held back before anything went out`. Senza il controllo: `4 crossing(s) asked for`, quattro
`this world is full of players`. **Nessun heartbeat è un sì** — un mondo che non li manda non è pieno,
è un mondo che questa cosa non la fa — e sabotare quella regola rende rosse due righe.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §19.

**E finalmente un link che non è loopback.** L'exit test di M6 dice «due server su macchine diverse»
e ogni numero era stato misurato a 127.0.0.1. `tools/netshape` è un relay UDP che ritarda, perde e
duplica datagrammi — **sotto** ENet, non dentro: scartare in `Transport::send` sarebbe sbagliato,
perché il canale affidabile ritrasmette ciò che la rete perde e un messaggio che ENet non ha mai visto
non torna più. Riporta quello che ha **applicato**, non quello che gli è stato chiesto, e così ha
trovato il proprio difetto alla prima esecuzione (`s >> 33` = 31 bit: jitter sempre negativo, perdita
raddoppiata). Alla prima esecuzione vera ha trovato anche il difetto per cui esiste: con i 31 file già
in cache e 120 ms di round trip, `atlas: 0 block(s) from media this world sent, 31 generated` — perché
«completo» era vero *prima che l'offerta arrivasse*, e il gate aveva una scappatoia (`|| had ==
offered`) che lo teneva verde. Su loopback l'offerta batteva sempre il renderer e il difetto non
esisteva. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §20.

**E le costanti erano scelte su loopback.** Con l'exit test di M6 intero a 400 ms di round trip e 10%
di perdita sono caduti quattro numeri. Il controllo di velocità divideva la distanza per il tempo
dall'**arrivo** della richiesta precedente — la rete comprime quell'intervallo e un giocatore fermo
veniva corretto per `0.416524 blocks sideways in 0.007002s`; adesso è un **budget** che si riempie sul
tempo reale, con lo stesso tetto di prima. I media stavano sul canale eventi insieme all'heartbeat, e
ENet ordina dentro un canale: gap massimo fra due battiti **2.7 s contro 1.9 s** con un canale loro
(il primo tentativo di dimostrarlo è fallito — quattro run su quattro sopravvissute — quindi si è
misurato invece di discutere). `kHeartbeatSilence = 5 s` era «cinque intervalli» su un link dove un
intervallo costa 0.1 ms: adesso è `max(5 s, 25 × round trip)`, e i 5 s originali avevano **1.8× di
margine** sul caso peggiore misurato. E «una texture, un download» valeva solo se i due trasferimenti
non si sovrapponevano: **51 scaricati su 31 distinti**, finché la cache non ha imparato cosa è in
volo. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §21.

**Il segnale di vita non deve stare in coda.** Al 20% di perdita il gate stampava «gap massimo fra
battiti **9.17 s**, **zero persi**»: due numeri che insieme non descrivono una rete che perde
pacchetti ma una coda. L'heartbeat era affidabile, ENet ritrasmette con un timeout che raddoppia, e
tutti i battiti dietro aspettano in ordine — mentre `heartbeats_lost` non poteva muoversi, perché un
canale affidabile non lascia buchi nella sequenza. Adesso è non sequenziato: **9.17 s → 4.16 s** nel
caso peggiore, e il contatore dei persi è diventato una misura. Poi l'attesa di un attraversamento:
2 s erano loopback, `8 × round trip` era solo un numero più grande e cadeva ancora due volte su sei,
e la risposta giusta era che non serve una costante — un `ChangeScope` è affidabile, quindi finché
la sessione è viva la risposta sta arrivando, e l'attesa **è** la finestra di silenzio. Stessa cosa
per il join: 10 s erano dodici round trip a 400 ms, ora `max(10 s, 40 × round trip)`, misurato
1.2–3.9 s. E «non streamato una seconda volta» era una percentuale (un quarto dei chunk) che al 20%
non distingue un secondo streaming dal primo che sta ancora finendo: adesso si contano i chunk che
quel mondo **aveva già**, che sono zero. Al 10%: sette run su sette. Al 20%: sette su otto, e le
quattro cose che restano rotte sono elencate con i loro numeri in
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §22.

**La stessa richiesta voleva dire due cose.** Dopo un attraversamento su un link lento il server
rimetteva il giocatore al centro dell'apertura — `corrected to 60.50 21.00 60.50: that position is
inside a solid block`, quindici volte, e il giocatore finiva a **y = -1607**. Quel punto è ciò che il
client dichiara come **punto di vista** della finestra che guarda di là: una `Move` da un corpo vuol
dire «sono qui» e viene controllata contro il mondo, da una finestra vuol dire «tienimi i chunk qui
attorno» e non c'è niente da controllare. Un attraversamento trasforma l'una nell'altra, e a 400 ms
una richiesta spedita da finestra atterra dopo che il server l'ha fatta corpo. Il numero di sequenza
non poteva prenderla: quella richiesta è la **più nuova**. Adesso la richiesta dice cosa credeva di
essere e il server la applica solo se la sessione lo è ancora (protocollo **5**), e il client smette
di mandare punti di vista a una sessione che sta diventando un corpo. Al 20% di perdita: **otto run
su otto, zero correzioni**, dove prima erano sette su otto e due su una trentina finivano con il
giocatore sotto il mondo. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §23.

**E la bocca aspetta il mondo che c'è dietro.** Una run su otto al 20% di perdita attraversava con
`0 chunks before the crossing and 32 after`: il mondo di là arrivava **dopo** il giocatore, che è una
schermata di carico per quanto breve. Non si aspetta che quel mondo sia completo — su un link lento
non lo è mai al momento giusto — ma solo **il blocco in cui si uscirà**, che è anche il primo a
partire, perché il punto di vista dichiarato per quella finestra è proprio il punto d'arrivo. Con un
link lontano a 400 ms e 30% di perdita: **due attraversamenti su sei senza il blocco d'arrivo prima,
zero su dodici dopo**, e l'attesa scatta in quattro run su dodici — un frame o due sulla soglia, poi
si passa. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §24.

**Un timeout non è una risposta.** `36 were fetched` per **31 file distinti**, tutti sulla stessa
sessione: il client rinunciava a metà lotto e richiedeva tutto, e poi arrivavano entrambe le copie.
Far ripartire l'orologio a ogni file che atterra non bastava, perché il timeout era ancora **l'unico
modo che il client aveva di sapere qualcosa** — un server che non possiede un hash chiesto non diceva
niente, quindi «non arriva» e «arriva piano» erano indistinguibili. Stesso difetto di un
attraversamento senza `ScopeDenied` dietro, stessa cura: `MediaMissing`, protocollo **6**, una
risposta per ogni hash. E non bastava neanche quello, finché un numero aggiunto apposta non ha detto
perché — `18 taken over from a session that looked dead`: la prenotazione di un file scadeva sotto una
sessione viva, perché il suo timer misurava il trasferimento invece della sessione. A 400 ms e **25%
di perdita**, cache fredda: da **62 scaricati su 31 distinti** a **31 su 31, otto run su otto**.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §25.

**E l'ultima attesa era di qualcun altro.** Tre run su otto perdevano una connessione senza che
nessuno lo avesse deciso: `disconnected: closed`, e basta. Il dato che mancava, aggiunto invece di
indovinato — `last heard from 1255 ms ago` — dice tutto: i dati passavano, era **un singolo comando
affidabile** rimasto in sospeso mentre il suo backoff raddoppiava, e ENet che lo dichiara morto con i
suoi default, mai scelti da nessuno qui. Adesso ENet è il limite **esterno**: non decide prima del
codice che sa dire perché. E poiché finora era proprio quel timeout a liberare un posto a sedere, il
server prende la sua metà della domanda, che non aveva — un client che gioca manda venti posizioni al
secondo, quindi dodici secondi di silenzio sono un client assente. Un client ucciso senza saluto
libera il posto in **12.2 s invece di 31.5**. A 400 ms e 25% di perdita: da tre cadute su otto a
**zero**, e lo scenario dell'exit test al 20% fa **otto run su otto**.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §26.

**E il link cattivo entra nella suite.** Sei commit di fila hanno trovato difetti che esistono solo
sopra il loopback, e tutti e sei **a mano**: in `ctest` non c'era niente che ci passasse vicino.
`tests/bad_link.sh` è quello scenario dentro la suite — due server con game diversi dietro due relay,
cache fredda, un attraversamento, 400 ms e 20% di perdita. Asserisce **invarianti e non tempi** (la
perdita è casuale, un gate sui secondi lampeggia), che lo scenario sia successo davvero (i check
sull'attraversamento stanno dentro `if (crossed_once > 0)`, quindi una run in cui nessuno passa non
riporta nulla), e che il link fosse davvero cattivo — con `CNP_LINK_LOSS=0` il gate diventa rosso da
solo. Rimettendo i difetti uno alla volta, quattro run ciascuno: l'heartbeat affidabile lo fa cadere
**2 volte su 4**; l'attesa dell'attraversamento a due secondi fissi e il timeout di ENet ai default
**no**, e questo è detto invece che nascosto — la prima adesso basterebbe davvero, il secondo cadeva
al 25% dove il gate non arriva. E `tests/dead_world.sh` copre la terza clausola dell'exit test, che
non aveva un gate: il mondo di là viene ucciso con `kill -9` sotto una finestra aperta, e quello che
si controlla è la forma del backoff — `2 line(s) about it in 26s, waits: 2 4` — e che il mondo in cui
il giocatore sta non se ne accorga. Tolta la regola, lo stesso scenario scrive **28108 righe**.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §27.

**Due server, due orologi.** L'exit test dice «due server su **macchine diverse**», e uno shaper
simula bene il round trip e la perdita ma non che le due wall clock non coincidano — che è la cosa su
cui un ticket si regge. Il commento accanto a `kTicketSkew` dichiarava la regola giusta, il codice ne
applicava due: sessanta secondi di tolleranza su un estremo della finestra e **zero** sull'altro. Un
server con l'orologio avanti rifiutava un ticket firmato un secondo prima dicendo «the ticket has
expired», e chi legge quel log va a guardare le durate invece degli orologi. Adesso la tolleranza è la
stessa ai due estremi e il messaggio porta i numeri. Gated senza una seconda macchina, perché non
serve spostare un orologio: un ticket porta i suoi timestamp e il codice sotto esame è il confronto.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §28.

**E l'ECS arriva prima della milestone che lo chiede.** DESIGN §8 mette l'ECS in M8 e chiede «mob base
con ECS» in M7 — otto settimane di inventario, craft e UI con un ECS dato per scontato sotto. Flecs
**v4.1.6** entra dalla sua distribuzione a file singolo (17.3 s di compilazione, il server da 32.8 a
38.8 MB), e quello che cambia è una frase: prima si mandava a un giocatore «le **altre sessioni**,
saltando quelle non Ready, saltando gli osservatori, saltando quelle che non hanno detto dove sono» —
quattro fatti sulle connessioni, e un mob non ne soddisfa nessuno. Adesso «le **entità** vicine con un
`Body`». La prova che non è una tabella di sessioni con una libreria attaccata è `--mobs N`: tre corpi
che nessuno guida, mandati dal codice che non è stato toccato per farli passare, e un client vero che
ne vede cinque. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §29.

**Un indirizzo è una strada, non un nome.** Due container, due reti, MTU 1280, e i due server che si
raggiungono **attraverso le porte pubblicate dell'host** — NAT vero in entrambi i versi. Alla prima
run: dopo l'attraversamento il client apriva una **terza** sessione verso una macchina con cui stava
già parlando, perché il mondo di là annunciava quel server come `172.31.8.1:25891` mentre il client lo
conosceva come `127.0.0.1:25891`, e il confronto era fra stringhe. Su loopback le due grafie
coincidono sempre e il difetto non esiste; dietro NAT un server ha sempre almeno due nomi. Adesso un
server ha un nome — la chiave con cui firma i ticket, annunciata nel `Welcome` e portata da `Portals`
per il mondo di là (protocollo **7**) — e l'indirizzo resta il ripiego. È servita anche una correzione
di configurazione: un collegamento che porta a un server ora lo **nomina** in tutti e due i versi.
Gated senza docker, facendo dire al mondo di là `127.0.0.2` di un server che il client ha chiamato
`127.0.0.1`. [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §30.

**E finalmente due orologi che non vanno d'accordo davvero.** §28 aveva gated la *politica* firmando
ticket con date scelte a mano; quello che mancava erano due **processi** che leggono orologi diversi.
Non serve né un container né una VM: i *time namespace* di Linux non coprono `CLOCK_REALTIME`, e
`tools/clock_skew` sono trenta righe di `LD_PRELOAD` che spostano la wall clock di un processo e
lasciano intatto ogni orologio monotono — tick, round trip e backoff continuano a misurare sé stessi.
Ha trovato che il server **spiegava** e il client **buttava**: `refused 'player' with a ticket: the
ticket is dated 120s ahead of this server's clock, which allows 60s` diventava, per chi gioca, «no
ticket this world will honour», e chi legge quel log va a guardare fiducia e firme. `Refuse` porta
`reason` **e** `detail`, e due punti del loop stampavano il solo `reason`. Il gate stesso è passato
per la ragione sbagliata alla prima scrittura — `grep "clock"` trovava `os.clock at load` stampato da
una mod del client — e adesso cerca la frase e **il numero dentro la frase**. Quattro casi, i due
estremi della finestra dentro e fuori; tre sabotaggi, tutti rossi.
[`docs/M6_FINDINGS.md`](M6_FINDINGS.md) §31.

**Un portale funziona in un verso solo.** Dal davanti mostra la destinazione e ci si passa
attraverso; da dietro non c'è: niente immagine, niente fianchi del vano, e nemmeno il passaggio di
rendering: resta il buco nel muro e quello che c'è oltre. Non è una regola nuova, è l'immagine che
raggiunge il corpo — attraversare da dietro non è mai stato possibile. `one_way = 0` in
`config/portal.cfg` per il vecchio comportamento a due facce. Misure in
[`docs/M5_FINDINGS.md`](M5_FINDINGS.md) §13.

**La forma di un portale sta in un file.** `config/portal.cfg` tiene i sei numeri che la
descrivono — dove sta l'immagine dentro il blocco, dove comincia il vano, dove scatta
l'attraversamento, dove viene posato chi arriva, quanto oltre la faccia d'uscita si guarda e dove
va il piano di taglio obliquo — ognuno con la sua derivazione e il suo costo scritti accanto.
`--portal-config PATH` o `$CNP_PORTAL_CONFIG` per puntarne un altro. Tabella e misure in
[`docs/M5_FINDINGS.md`](M5_FINDINGS.md) §9.

### M4, M5, M6 — chiusi, clausola per clausola

Non «sembra funzionare»: ogni clausola dell'exit test di [`docs/DESIGN.md`](../DESIGN.md) §8 con
accanto il gate che la misura, e girano tutte a ogni `ctest`.

**M4 — portali completi.**

| Clausola dell'exit test | Gate |
|---|---|
| orizzontale ↔ verticale | `portal_orientation` |
| mezzo-offset | `portal_cases` — «the offset opening is measurable on screen» |
| giocatore dentro la cornice | `portal_cases` — «the oblique near plane declines with the eye inside the opening» |
| portale dietro il vetro | `portal_cases` — «the portal image survives the glass in front of it» |
| teletrasporto intra-frame ≥ 4 nodi | `portal_cases` — «the jump is the four nodes the case asks for» |
| due portali che si guardano a profondità 2 | `portal_recursion` — «an image appears inside the third at depth 2» |

L'ultima riga è quella che il fork non sapeva fare.

**M5 — multi-sessione locale.** L'exit test chiede dieci giri e i quattro bug storici:

| Clausola | Gate (`dual_session`) |
|---|---|
| entra ed esce dieci volte di fila | «ten crossings in a row, each one swapping the active world» |
| cielo | «the sky is the arrived world's on every lap» |
| avatar proprio | «exactly one session draws the player's own avatar» |
| view range | «the promoted world gets the full view range and the demoted one the budget» |
| tasti premuti | «and a held key still moves the player through the swap» |

**M6 — cross-server.**

| Clausola | Gate |
|---|---|
| due server, *game* diversi | `bad_link` — «which is a different game, drawn from the pictures that world sent» |
| macchine diverse | `bad_link` (due indirizzi, due relay), `two_clocks` (due orologi), `tools/two_machines.sh` (due stack di rete, NAT, MTU 1280) |
| attraversamento senza schermata di carico | `bad_link` — il mondo di là ha già il blocco d'arrivo prima che si entri |
| una passiva verso un server morto non spamma | `dead_world` — 2 righe in 26 s contro le 28108 senza la regola |

**Cosa «macchine diverse» ancora non è.** Un kernel separato e due processi che non si contendono la
stessa CPU: quelli restano a una VM, e non sono coperti. Tutto il resto — MTU vera e frammentazione
(`ReasmReqds=786 ReasmOKs=393`), NAT in entrata e in uscita, un server con due nomi, due wall clock
che divergono — è misurato. `tools/two_machines.sh` **non** è un `ctest` di proposito: un test che
fallisce su una macchina senza container insegna a ignorare i fallimenti. Serve a trovare; quello che
trova finisce in `tests/bad_link.sh`.

E i limiti dichiarati invece che nascosti: `bad_link` **non** riprende due dei sei difetti che il
link cattivo ha trovato (§27.3), ENet negozia 1392 byte senza path-MTU discovery, `kSessionSilence`
a 12 s chiude una sessione viva ma muta a cui il server non deve più niente (§31), e
`TODO.md` OPEN-1…OPEN-5 sono aperti. OPEN-6, il client che si piantava circa una run su venti, è
chiuso su prove e non su una diagnosi: sessanta run dello scenario che ne produceva 2 su 5 senza
una piantata, e `--watchdog` armato in ogni gate grafico, che trasforma una ricomparsa in uno stack
simbolizzato invece che in un timeout di ctest senza niente dentro.

### M7 — gioco minimo, con il test d'uscita passato

**Gli item esistono, e c'è una borsa in cui tenerli.** Due registry e non un campo su `NodeDef`,
perché i due insiemi si sovrappongono solo in parte: ogni blocco riceve una **forma-item** derivata
(31 blocchi tenibili sui mod veri), e un piccone è un item senza blocco. L'aria è esclusa — non si
tiene in mano niente in particolare. Le quattro regole che una prima stesura sbaglia sono gate:
le pile parziali si riempiono prima di prendere uno slot nuovo (senza, otto slot da un sasso
diventano otto sassi), `add()` restituisce quello che non ci stava invece di mangiarlo, una spesa è
tutto o niente, e uno slot vuoto è `kNoItem` con conto 0 e mai un conto 0 con ancora un nome dentro.
Si paga **dopo** che il blocco è sceso, perché un mod può annullare il piazzamento; e rompere con la
borsa piena rompe lo stesso, perché un blocco che a volte rifiuta di rompersi senza spiegazione è
peggio di un drop perso. Un reload non svuota la borsa di nessuno: gli id sopravvivono e l'orfano
smette di piazzare. Quattro sabotaggi, tutti rossi. **Non passa ancora dalla rete** — un client
connesso non può rompere né piazzare, non è mai stato possibile, ed è il pezzo successivo.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §1.

**E il blocco attraversa il filo.** Fino a qui un client connesso **non poteva rompere né piazzare**
— non una regressione, non è mai stato possibile, e il README lo dichiarava da M3. Protocollo **8**:
`Edit` porta il blocco colpito e la sua faccia, **mai la cella** che deve cambiare, perché una cella
sul filo è un campo in cui un client modificato appende un blocco a mezz'aria; e lo `slot` viaggia
dentro il click, perché una selezione che arriva dopo il click che la intendeva è come si paga un
blocco che non si è posato. `core/net` decide **se** (scope, portata, cosa c'è lì, cosa c'è in mano,
chi c'è in mezzo) e chi possiede il mondo **fa** — così un piazzamento che un mod annulla non costa
nulla, perché niente si spende prima di `edit_done`. Ogni edit ha una risposta, rifiutato o no, e la
risposta porta **la frase**. Nessun regalo: l'unico modo di avere un blocco è averne rotto uno.

**E due scenari in una run sono uno scenario che nessuno ha scritto.** La sequenza di edit e
l'attraversamento scriptato vivono nella stessa funzione, e su un link a 400 ms col 20% di perdita non
convivono. Far aspettare la camminata a sequenza finita — la cosa logica — ha **peggiorato** le cose:
l'attraversamento slitta di otto secondi, cioè cambia il timing della cosa che `bad_link` misura, e tre
run su otto tornavano rosse **non sulle mani** ma sulla finestra sul mondo di là. Un gate che va
ritarato per far entrare un secondo scenario sta misurando il secondo scenario. Adesso la sequenza non
parte dove le aperture portano altrove, `bad_link` è tornato ai suoi 40 secondi (sei run su sei verdi),
e la rinuncia è **detta**: il client stampa perché ha saltato, e `net_client.sh` controlla che lì sia
partita davvero.

**E tre gate sono passati verdi per la ragione sbagliata, in un commit solo.** «Piazzare a mano vuota
è rifiutato» passava su *there is already something there*; «non si costruisce addosso a chi c'è»
passava su *your hand is empty*; «una finestra non tocca niente» passava su *the server does not know
where you are yet* — e quel terzo restava **verde con la regola sullo scope tolta del tutto**. Un gate
su un rifiuto che asserisce solo il rifiuto non è un gate: adesso asseriscono **cosa dice**. Quattro
sabotaggi, tutti rossi. [`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §2.

**E c'è una barra in fondo allo schermo.** `plat_debug_text` diceva «debug overlay only, a real UI
arrives in M7» dal giorno di M0. Quello che entra **non** è `microui` né `Clay`: è il **backend** che
tutti e due chiedono — `gfx_2d_*`, cioè rettangoli, immagini, clip e testo, che è esattamente la lista
di comandi che quelle librerie emettono. Costruire prima i widget e poi la superficie è come una UI
finisce per non girare su nient'altro che il toolkit su cui è nata. La barra prende **slot e non un
`Inventory`**: la borsa di un giocatore in rete è quello che il server ha mandato, e farla dipendere da
un `Inventory` significherebbe tenerne una seconda copia in un tipo il cui scopo è *essere* l'autorità.
L'icona esce dallo stesso atlas del mondo, così non può contraddire il blocco che nomina.

**E qui i gate verdi per la ragione sbagliata sono stati quattro.** «Uno slot vuoto non mostra un
blocco» contava i pixel non-di-sfondo — ma uno slot vuoto ha il suo riempimento: **1681 contro 1640**,
verde con l'icona disegnata ovunque. «Due blocchi diversi non si disegnano uguali» e «il conteggio è
disegnato» confrontavano lo slot *selezionato* con uno non selezionato: era il bordo giallo, e forzando
tutte le icone alla tile 0 restavano **verdi**. E due slot cadono su offset **sub-pixel** diversi,
quindi due disegni identici differiscono lo stesso di ~2% e sotto quella soglia non c'era segnale.
Quattro correzioni, nessuna delle quali è una soglia allargata: confrontare i **centri** dove non cade
nessun bordo, confrontare slot che differiscono per **una cosa sola**, misurare il conteggio su **uno
slot in due frame** (dove l'offset non esiste: uno slot non toccato legge **0.0%**), e per la selezione
guardare solo il **bordo**. Cinque sabotaggi, tutti rossi.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §3.

**E la borsa si apre — senza `microui`.** DESIGN §2.7 propone `microui` o `Clay` per il buco «nessuna
UI dichiarativa», e la proposta resta valida: solo che a una borsa servono un rettangolo, una griglia
di quadrati e un hit test, e nessuna delle tre è un widget. Quello che `microui` comprerebbe davvero
sono i **form** — campi, slider, checkbox, liste che scorrono — cioè il lavoro formspec di §7.5.
Portarselo in casa prima del primo form è portarselo in casa per la ragione sbagliata, ed è scritto
così che non si debba rifare il ragionamento. **E non c'è nessuna pila nel cursore**: una pila a metà
strada fuori dalla borsa è stato che il client possiede e il server no, e il primo pacchetto perso è
un giocatore con in mano una cosa di cui il server non ha mai sentito parlare. Un gesto manda
`MoveItem` — protocollo **9**, l'unico messaggio che cambia un inventario, e nemmeno lui è il client
che ne cambia uno: è una richiesta, e la risposta è **tutta** la borsa. Il **tasto destro** vuol dire due cose, e quale
dipende da se un trascinamento è già in corso: **premuto a metà trascinamento** posa **un** item nella
casella sotto il mouse e lascia correre il gesto, **da solo** ne comincia uno suo che porta **metà**
pila. È quello che questo modello ha al posto della mezza pila nel cursore, e il conto viaggia in un
campo che `MoveItem` aveva già — zero vuol dire tutta la pila in tutti e tre gli strati. Metà si
arrotonda **per eccesso**, da una funzione sola: la stessa risposta serve al movimento, al fantasma
sotto il cursore e al conto che resta nel quadrato, e tre copie di `(n + 1) / 2` sono tre occasioni
perché il disegno e il movimento non siano d'accordo. **E shift più un click** manda la pila dove
ovviamente va — fuori da un baule in borsa, fuori dalla borsa nel baule aperto, e sul risultato
«fallo finché qualcosa non finisce». Un messaggio suo, `QuickMove`, **quattro byte**: un `MoveItem`
nomina due quadrati e uno shift-click ne nomina uno, e un `to` inventato sarebbe un campo che può
mentire.

**E adesso un giocatore può morire.** DESIGN §8 chiede la salute, ed è arrivata con la cosa che
la toglie: le cadute. Il punto interessante è che questo server **non simula la fisica** — valida le
pretese — quindi non c'è nessuna velocità da leggere e nessun momento in cui a un corpo viene detto
che è atterrato. Una caduta è una **forma in un flusso di posizioni**: scendere per un po', e poi
smettere. Misurata solo sulle pretese **accettate**, perché una rifiutata è una pretesa su un posto in
cui quel corpo non è mai stato. Tre nodi liberi, un punto per nodo oltre, ventitré nodi è una barra
intera. Sotto il mondo si muore, perché un corpo là sotto è in una caduta senza fondo. **Morire costa
il posto, non la borsa**: qui non c'è nessun modo di tornare dove sei morto — niente coordinate,
niente segnalino, niente tomba — e una pila per terra dura cinque minuti. E il corpo torna intero
**subito**, non all'arrivo: uno che girasse a zero fra il morire e l'essere rimesso a posto è uno che
qualsiasi cosa può ammazzare una seconda volta. La rigenerazione esiste perché non c'è cibo: un numero
che scende e basta non è una risorsa, è un conto alla rovescia.

**E il mondo ha quattro posti invece di uno.** Un bioma qui è un **clima**, non un posto: due assi
di rumore, temperatura e umidità, e il bioma più vicino in quello spazio possiede la colonna. Il più
vicino invece di una tabella di rettangoli perché una tabella di rettangoli ha **buchi** — qualche
clima non cade in nessuna casella, e quella colonna resta senza blocco di superficie. Il pezzo
interessante è la distinzione fra **cosa si sceglie e cosa si mescola**: quale bioma possiede una
colonna è un gradino, ed è giusto — l'erba finisce, la sabbia comincia, il bordo si vede. Un gradino
nell'**altezza** invece è un dirupo inventato da una tabella, quindi i parametri di forma si
mescolano nello spazio del clima, dove tutto è continuo. Misurato sullo stesso mondo: **6** blocchi
di salto massimo fra colonne vicine senza biomi, **7** con la forma mescolata, **20** con la forma
scelta. Il pareggio fra due biomi sullo stesso clima si rompe sul **nome** e non sull'ordine di
registrazione — l'opposto esatto degli stage del mapgen, dove l'ordine *è* il contratto — se no lo
stesso seme darebbe due mondi diversi su due build che caricano i mod in ordine diverso. E il rumore
va **stirato**, o il deserto resta una curiosità: su sei mondi messi insieme il bioma più raro passa
dal 7.9% al 15.2%. La piazza dei portali resta piatta ed erba qualunque cosa dica il clima lì. Nove
sabotaggi, tutti rossi. [`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §27.

**E il sole si muove.** Lo shader era pronto da M1 — `uDaylight` scala **solo** il canale del sole,
lascia stare la luce dei blocchi e prende il `max` dei due così che una torcia non bruci una
superficie già illuminata — e nessuno gli aveva mai detto che ora fosse: `set_daylight` non era
chiamata da nessuna parte. Un'ora qui è un punto su un **cerchio**, e ogni difetto in quell'aritmetica
è un posto dove qualcuno l'ha trattata come un punto su una retta: 0.99 e 0.01 distano due centesimi
e una sottrazione dice novantotto, quindi la cucitura di mezzanotte sta **dentro** ogni misura invece
che al bordo. Il pezzo che conta è quello che **non** c'è: il client non ha un orologio, non
interpola e non avanza fra un messaggio e l'altro. Misurato: fra un annuncio e il successivo il sole
cambia di **0.0061** della sua corsa, contro **0.0667** che è il passo fra due dei sedici livelli di
luce di un blocco — un decimo di un livello. La prima stesura aveva la correzione con lo snap e la
regola «mai all'indietro», sei controlli e un gate: **cancellata**, perché una regola che nessuno
esegue non è una regola. E con lei se n'è andata la lunghezza del giorno dal filo, che è quello che
lascerebbe il client in disaccordo su quanto va veloce il sole. Ogni quanto lo si dice è contato in
**giorni** e non in secondi, così `--day-length 20` funziona senza una seconda regola. L'ora va anche
a chi **guarda soltanto**, che la barra della salute non fa: chi guarda in questo mondo da un portale
sta guardando il suo cielo. E i due mondi di `--dual` adesso stanno a due **ore** diverse invece che
a due letterali con scritto «un arancione» — il gate che li distingue conta gli stessi pixel di prima
e adesso misura una cosa che anche il giocatore legge. Protocollo **18**, `Time` tre byte.
Dieci sabotaggi. [`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §28.

**E un difetto trovato dai log invece che dalla fortuna.** Ogni gate shell adesso tiene il run che
fallisce e **non tiene niente quando passa** — entrambe le metà sono gate, perché una directory
lasciata a ogni giro è quella che nessuno apre. Al secondo run ha preso questo: il server accetta il
join, lo scrive nel log, manda il welcome, e **dodici secondi dopo butta fuori il giocatore perché
non dice niente** — mentre quel giocatore sta ancora aspettando il welcome e non ha niente che gli
sia permesso dire. Due pazienze scelte in due file: 12 secondi fissi da una parte, `round_trip ×
kHandshakeRtts` dall'altra, quindi **oltre i 300 ms di round trip il server si arrende sempre per
primo** e la finestra del client non può più significare niente. Non è una soglia da allargare: la
misura era sbagliata, perché «un client che gioca manda una posizione venti volte al secondo» è vero
di uno che **gioca** e a questo non era ancora stato detto che poteva. Il silenzio si conta dalla
prima parola **dopo** il welcome, e fino ad allora la pazienza deriva dalla costante del client —
il server deve sopravvivergli, non pareggiare, perché il client è il lato che può riprovare. Il
numero da cui deriva è quanto è durata **quella** stretta di mano, misurata su quella connessione:
**377 ms** su un link da 400. Sette sabotaggi, tutti rossi.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §29.

**E adesso si sente.** L'attenuazione è aritmetica e sta in `core` senza nessuna libreria audio
sotto: il backend `snd_*` riceve un guadagno e un pan, stessa divisione di `gfx_*`, così quello che
vale la pena controllare gira su una macchina senza scheda audio. La curva è detta come una frase —
**a metà della portata, un quarto del volume** — e `kRolloff` vale 2 perché è il numero che produce
esattamente quello. Arriva a **zero alla portata** e non a un decimo, perché una curva che alla
propria portata si sente ancora o taglia di netto o lascia ogni suono mai partito a tenere una voce
per sempre. Il pezzo che questo motore ha e un mondo piatto no è sotto: un suono nel mondo di là
arriva **attraverso** l'apertura, quindi la distanza è il **giro** — orecchio, porta, e poi dalla
porta di là alla sorgente — e la direzione è quella della **porta**. Una sorgente che nel mondo di là
sta alle stesse coordinate dell'ascoltatore, calcolata come se fosse qui, è assordante: sono cento
nodi oltre la soglia. Stesso argomento della camera virtuale, stesso modo di sbagliarlo. Non c'è
nessun `.wav` nel repository: un suono è una frequenza, una lunghezza e se è intonato o no, e i
campioni si generano — un file è una cosa su cui due client possono non essere d'accordo. Dieci
sabotaggi rossi, dopo tre verdi tutti colpa del gate, e uno ha fatto **cancellare una riga** invece
di aggiungerne una al test. [`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §30.

**E il server che risponde, e poi butta fuori chi stava aspettando la risposta.** Quattordici run di
`bad_link` di fila, uno rosso, e stavolta il gate ha tenuto il run: il server di là aveva **concesso**
la traversata — «came out of this world's opening 1 at 61 20 61» — il client non l'ha mai ricevuta, e
dodici secondi dopo il posto è stato liberato. La risposta parte affidabile, quindi ENet la stava
ancora ritrasmettendo: **il drop è quello che ha messo fine alle ritrasmissioni**. E il client taceva
**apposta**, per una riga scritta di proposito: chi ha chiesto una traversata smette di mandare
pretese di punto di vista, o la pretesa descriverebbe il centro di un portale a un server che la
leggerebbe come una posizione. Adesso il silenzio si conta **dall'ultimo fra l'ultima parola sentita e
l'ultimo momento in cui questo capo doveva ancora dei byte affidabili** a quel peer, e questa regola e
il timeout di ENet si dividono le due situazioni senza buchi e senza sovrapposizioni: niente in
sospeso decide questa, qualcosa in sospeso decide ENet. Byte, e non
`enet_peer_has_outgoing_commands`, che conta anche un ping: ENet pinga due volte al secondo e la
regola non avrebbe lasciato andare più nessuno. Il gate nuovo è andato rosso subito per colpa del
**codice**: `Time` partiva affidabile una volta al secondo, quindi il server era perennemente in
debito con tutti — una regola verde e morta. Reso inaffidabile, la suite ha presentato il conto entro
otto run: `kTellEvery` è una frazione di **giornata**, e su una giornata da un milione di secondi un
annuncio perso restava perso per quattordici minuti. Ora l'attesa è il minore fra una frazione di
giornata e **un secondo**, in un posto solo, e `step_time` ha di nuovo un accumulatore soltanto.
Tredici sabotaggi, tutti rossi. [`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §31.

**E finalmente qualcosa che si può colpire.** Il mob che c'era lo diceva di sé — «il meno mob che
possa essere»: un corpo che deriva su un cerchio, lì per provare che lo streaming è **un percorso** e
non un caso speciale sulle connessioni. M7 chiede salute, danno e drop, e adesso ci sono: sei punti,
uno per colpo, un colpo ogni 0.4 s — due secondi e mezzo dal vivo al morto — e quello che portava
resta per terra. Il messaggio è **cinque byte**, un tipo e un id: un client che potesse dire quanto
vale un colpo potrebbe dire venti, e non dice nemmeno dove stava chi ha colpito, perché il server ha
già una posizione che ha accettato. Il raffreddamento sta **sul server** — venti colpi nello stesso
istante tolgono quanto uno — e la portata è il braccio che posa un blocco, non un secondo numero.
Morire è **una** regola: `reap_dead` guarda solo le cose con `Mob`, perché l'avatar di un giocatore
ha salute anche lui e sopra ci sta una sessione viva. Il filo ha dovuto imparare **cosa è** ogni
corpo: prima si leggeva da `item`, quindi un mob era «un giocatore più piccolo» che il client non
poteva vedere affatto, e disegnarli come persone vuol dire giocatori che si menano per sbaglio. Il
primo `kMobHalfWidth` era **più largo** di un giocatore, e l'unica cosa che l'ha preso è stato il
controllo sulla *frase*: mirare, colpire e morire funzionavano tutti benissimo con un mob largo.
Diciassette sabotaggi, tre verdi tutti e tre colpa del gate — due chiedevano «ha tolto qualcosa?»
dove a rifiutare ci sono sette regole in fila, e il terzo ha fatto **cancellare** un ramo che
ripeteva quello che l'aritmetica diceva già. [`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §32.

**E il test d'uscita: trenta minuti, senza crash e senza toccare la console.** `1800 s, 7.9 milioni
di frame, **zero correzioni**, RSS da 122.6 a 123.8 MB — piatta dal primo minuto — 224 blocchi rotti
e 224 rimessi, otto creature uccise, un giorno e mezzo di cielo.` Trenta minuti di un client fermo
sarebbero un test di un socket e di una perdita di memoria, quindi c'è `--roam`: un giocatore che
cammina, scava e ricostruisce, e va incontro a quello che vede — attraverso **gli stessi messaggi che
manda un click**. **La prima run è stata rossa e aveva ragione tre volte.** Il giocatore è uscito dal
bordo di un mondo finito all'ottavo minuto e ha passato gli altri ventidue **a cadere**: `stood at
-3.06 -130822.35 97.95`. La regola che doveva prenderlo c'era ed era giusta — sotto il mondo si muore
— ma `kill_body` guarisce e torna indietro se non c'è uno spawn, e lo spawn stava dentro
`if (portals)`: un mondo senza aperture non ne aveva **nessuno**. E quella regola **è gated**, in
`net_loopback` — solo che quel gate lo spawn se lo mette da solo, quindi non ha mai chiesto una volta
cosa fa un server che non ce l'ha. Regola giusta, irraggiungibile. Le altre due rosse erano una causa
sola detta due volte: una run lunga è un **secondo scenario**, e tre controlli che leggevano lo stato
*alla fine della run* stavano applicando la loro affermazione allo stato di qualcun altro — la borsa
che il roam aveva riempito, il blocco sotto i piedi che il roam aveva scavato. Adesso campionano
mentre la sequenza scriptata gira. [`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §33.

**Il difetto è arrivato dal messaggio nuovo.** Aggiunto `Vitals` al protocollo — **17** — cinque
sezioni del loopback sono diventate rosse in punti che non c'entravano niente. `ClientSession` chiude
la sessione su un messaggio che non conosce, che è la scelta giusta: aggiungere un messaggio
server→client senza insegnarlo al client non è un lavoro a metà, è un lavoro rotto.

**E due gate erano di nuovo verdi per la ragione sbagliata.** «Ogni quadrato risponde all'hit test col
suo numero» provava **solo i centri**: un hit test che si rifà l'aritmetica e dimentica lo spazio fra i
quadrati azzecca tutti i centri — **54 sonde su 180** fuori posto, e la versione a soli centri era
verde. E con la griglia **trasposta** `slot_at` e `slot_rect` restano d'accordo fra loro mentre mettono
nove righe da quattro sullo schermo. Cinque sonde per quadrato, e un controllo sulla **forma** che non
passa da `slot_rect` per decidere se ha ragione. Sei sabotaggi, tutti rossi.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §4.

**E poi un giro di gioco vero ha trovato quello che nessun gate cercava.** Il client stampa una riga
per mod a ogni ricarica, e quella riga diceva `mod 'palette': 20 item(s) can no longer be obtained,
0 new one(s) can` — sedici volte di fila. Un solo F5 e **26 blocchi su 31** smettevano di essere
posabili e di cadere quando li si rompeva, per sempre. La causa: un item «forma di blocco» è
**derivato**, non dichiarato — nessun mod scrive una riga sugli item dei propri blocchi, è il motore
che li conia — quindi chiedergli «il mod l'ha registrato di nuovo?» è la domanda sbagliata: il mod
non l'ha registrato **mai**. La domanda va fatta ai blocchi, e per farla serviva che `NodeRegistry`
tenesse conto di chi un reload ha lasciato indietro invece di limitarsi a dipingerlo di magenta. E il
gate era verde perché i suoi tre casi **circondavano** il difetto senza toccarlo: un item registrato a
mano, un item tolto, un blocco nuovo — mai un blocco che entrambe le versioni registrano, cioè tutta
la popolazione reale. [`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §5.

**Il drag & drop è l'unico modo in cui una pila si sposta.** Il click-poi-click che c'era prima è
stato **tolto**, non lasciato lì inutilizzato: due modi di fare una cosa sola sono due stati che chi
gioca deve tenere in testa, e uno dei due — «c'è qualcosa di scelto in questo momento?» — è invisibile
appena si guarda da un'altra parte. Il drag il suo stato lo tiene nel tasto del mouse, dove una mano
già sa dov'è; e chi si ricorda a metà il vecchio modo deve ottenere **niente**, non qualcosa che non
intendeva. Una funzione sola, pura, chiamata da entrambi i loop, con due regole che valgono da sole:
**il drag porta quello su cui è partito** e non ripiega su nessun'altra casella, e **un rilascio fuori
dal pannello non sposta nulla**, perché non esiste ancora un terreno su cui far cadere una pila.
Mentre il drag è in volo la casella di partenza è disegnata **vuota** — la pila è sotto il cursore ed
è una pila — ma il **quadrato resta**, perché un buco nella griglia si legge come «la borsa ha perso
uno slot». E si ripiena **da sé** se il drop non vale: non c'è un ramo che annulla, la casella è vuota
solo finché il tasto è giù, e un annullamento che deve ricordarsi di annullare prima o poi dimentica.
Undici sabotaggi, tutti rossi; tre meritano una riga, perché un `Move` **trasposto** è uno spostamento
legale verso il posto sbagliato e un gate che contasse «è avvenuto uno spostamento» resterebbe verde,
perché «il fantasma è sparito da dov'era» è vero anche di un fantasma inchiodato in un angolo, e
perché svuotare un quadrato e cancellarlo si assomigliano finché non si guarda cosa c'è sotto — il
gate confronta con una casella vuota **vera**, che ha un riempimento, e un buco no.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §6.

**E una pila può stare per terra.** Era la stessa mancanza scritta due volte in due sezioni diverse:
rompere con la borsa piena **perdeva** il blocco («e questa è la decisione, non una svista»), e
rilasciare un drag fuori dal pannello non faceva nulla «perché non esiste un terreno su cui far cadere
una pila». Un drop è un'**entità** con `Position`, `Look`, `Body` e `Dropped` — nessuna lista sua,
nessun messaggio suo — e questo è esattamente ciò che [`mob.h`](../../src/core/ecs/mob.h) era stato scritto
per rendere verificabile: che lo streaming è «le entità vicine con un `Body`» e non «le altre
sessioni, meno gli osservatori, meno quelle che non hanno detto dove sono». La seconda prova della
stessa affermazione, fatta da qualcosa che non è nemmeno una creatura: arriva a ogni client
attraverso codice che nessuno ha modificato per farcelo arrivare. Due byte per corpo — protocollo
**10**. Tre errori che una prima stesura fa sono gate: riposare **dentro** il blocco, riposare dove
capita (e affondare di un pelo a ogni tick), e chiedere `World::is_solid` invece del registry — che
risponde «non è aria», e un mod può registrare qualcosa che si attraversa. La raccolta chiede «le
entità con una posizione e un inventario», non «i giocatori», e **restituisce chi**: mandare un
inventario è affare della tabella delle sessioni, e chi dimentica di mandarlo è un giocatore la cui
borsa si riempie in silenzio. Quel che ci sta entra e il resto resta per terra — tutto-o-niente qui
vuol dire una pila su cui si passa sopra per sempre. Undici sabotaggi, tutti rossi.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §7.

**E adesso una pila la si può buttare via — ma prima la borsa non si apriva.** `E` non faceva niente
appena entrati: la barra si disegna dall'**ultima cosa che il server ha detto**, e il server non
diceva niente finché non arrivava una rottura. Una borsa vuota **è un fatto** e va mandata come
qualsiasi altro. Il gesto è `DropItem{slot, count}` — protocollo **11** — che porta **uno slot e un
conteggio e nient'altro**: dove la pila atterra è aritmetica del server da dove il giocatore già è,
perché un punto sul filo è un campo in cui un client modificato butta una pila dall'altra parte della
mappa. `Q` ne butta una, `CTRL+Q` tutta la pila, trascinarla fuori dal pannello la butta — ed è la
riga che il §6 diceva sarebbe cambiata «quando ci sarà un oggetto per terra». Il client
single-process ha adesso un ECS, così la borsa sta su un'entità e non in una variabile: la stessa e
unica borsa, non una copia tenuta in riga a mano.

**Il difetto più istruttivo l'ha trovato il gate: `peek_type` aveva un `default:`.** Messaggio
scritto, codec scritto, handler scritto, e il server non rispondeva — perché lo `switch` che
riconosce i tipi finiva con `default: return Invalid`, e un tipo nuovo non elencato è
**silenziosamente invalido** invece che un errore di compilazione. Il sintomo è una richiesta senza
risposta, che assomiglia esattamente a un handler che nessuno ha scritto. Tolto il `default:`: ora
ogni enumeratore va elencato o non si compila, e **il sabotaggio corrispondente non compila** — più
forte di un rosso. `protocol_test.cpp` aveva già lo stesso presidio e infatti ha funzionato: due
switch sullo stesso enum, uno protetto e uno no. E un gate era di nuovo verde per la ragione
sbagliata — «una finestra non può buttare via niente» restava verde con la regola di scope tolta,
perché l'inventario di un osservatore è sempre vuoto e non c'era niente da buttare.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §8.

**La rotellina cammina sulla barra, e la finestra ci sta sullo schermo.** `hotbar_scroll` è pura come
`bag_click` e per la stessa ragione: due loop la chiamano. Quattro errori che una prima stesura fa
sono gate — verso invertito (allontanare la rotella va a **sinistra**, l'opposto di una barra di
scorrimento, e un gate che controllasse solo «si è mossa» non saprebbe quale dei due), `-1 % 9` che in
C fa `-1` e mette la mano sullo slot −1, clamp invece di wrap, e troncare invece di arrotondare, che
su un trackpad rende un trascinamento lento buono a nulla. E la finestra chiedeva 1920x1080 su uno
schermo 1920x1080: una finestra grande come lo schermo **non ci sta** sullo schermo, e il fondo del
gioco finiva dietro la barra delle applicazioni. Ora è ridimensionabile e si apre ai nove decimi del
monitor, centrata — **ma solo quando non sta misurando**: i gate sul frame rate misurano contro lo
schermo, quindi una finestra più piccola li renderebbe più economici, e tutte e otto le prove GPU
stampano `window: 1920x1080, fixed for measurement`. Con il ridimensionamento arriva quel che si
porta dietro: `gfx_screen_*` erano catturate all'avvio, e i **render target dei portali** sono della
forma della finestra e campionati in spazio schermo — uno rimasto della forma vecchia mette un mondo
stirato dentro l'apertura, e non lo segnala niente. Il loop se ne accorge confrontando quel che ha
costruito con quel che sta disegnando, non leggendo un flag che qualcuno deve azzerare — e **aspetta
che il trascinamento si fermi**: la prima versione ricostruiva a ogni pixel, misurato in **195
ricostruzioni** in un solo trascinamento contro **una per ridimensionamento** dopo — un difetto che
nessun gate prende perché il risultato è giusto e solo il costo è sbagliato.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §9.

**E la UI la dichiara un mod.** DESIGN §8 chiede «UI (microui/Clay guidata da Lua)» e ogni sezione di
M7 finora finiva con «niente da Lua» — quattro volte. `mods/console` è la prova: dichiara un
pannello, il motore lo disegna, il click torna come evento, e **nessuna riga di codice del motore sa
che quel mod esiste**. È **ritenuta e non immediate mode**, e non è una preferenza: `microui` e Clay
descrivono tutta la UI a ogni frame, ma un mod gira sul **server** e il pannello si disegna su un
**client** — un pannello immediate vorrebbe dire mandare la descrizione sessanta volte al secondo
dentro una socket. Una dichiarazione mandata una volta e un click che torna è l'unica forma che
sopravvive al filo, ed è la forma che **formspec ha già**. Le coordinate sono in **celle** e non in
pixel; un pannello appartiene al mod che l'ha dichiarato e un altro non può né ridichiararlo né
aprirlo; un'**etichetta non si clicca**, perché una didascalia che rispondesse manderebbe al mod
eventi per una cosa che non ha mai reso cliccabile; e il click arriva su `core:ui_click`, lo **stesso
bus** di `core:broken` — non c'è un percorso privilegiato per la UI. Undici sabotaggi, tutti rossi;
uno era verde la prima volta, perché spostare il pannello di 40 px muove il disegno **e** il hit test
insieme e i bottoni restano cliccabili — mancava l'affermazione che `place_panel` **centra**.

**E la suite ha trovato un use-after-free.** `net_client` rosso: «il server ha smesso di rispondere per
5 secondi». `console` falliva su un evento che il **server** non dichiarava — ma due righe prima aveva
già registrato un handler, e `ModHost` distruggeva la VM **senza** toglierlo dal bus. Il primo
giocatore che rompeva un blocco faceva chiamare dentro memoria liberata. Il sintomo non era dove
stava la causa, e da fuori si vedeva solo silenzio. Il sabotaggio corrispondente non è rosso: è un
**segmentation fault**. E il gate che lo copre è stato verde per la ragione sbagliata **due volte**
mentre lo scrivevo — due modi diversi di far fallire il mod di prova *prima* del punto che conta.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §10.

**Il pannello attraversa il filo, e un mod aveva smesso di essere sé stesso.** Protocollo **12**:
`PanelShow` porta la dichiarazione intera quando **cambia** — che è un'azione di un mod e non un
flusso, ed è tutta la differenza fra ritenuto e immediate mode — e `UiClick` riporta **il nome del
pannello e l'id del bottone**, mai un punto e mai un indice: dove fosse il bottone sullo schermo di
quel giocatore è aritmetica del client, e un indice dentro un pannello che il server ha già
ridichiarato è un click sul bottone sbagliato, in silenzio. `open = false` è un messaggio e non
un'assenza, perché un client non distingue «chiuso» da «questo tick non è arrivato niente». Ma la cosa
che vale la pena raccontare non è il messaggio. Nel log di gioco c'era `[mod] ui: 'console:menu'
belongs to mod 'console'`: sembra una conferma, è un **rifiuto** — detto a `console` a proposito del
proprio pannello. `EventBus::emit` chiamava gli handler **senza dire quale mod stesse girando**,
quindi dentro un handler l'owner era la stringa vuota e ogni callback host era attribuita a nessuno.
Niente crash, niente errore in Lua: solo un pannello che non si apriva. Nessuno dei tre gate dei
pannelli lo vedeva, perché tutti e tre chiamavano `ui_*` al **caricamento** del mod, dove il mod si
sa. Ora un handler chiude il proprio pannello e il test lo verifica — più uno che non mi aspettavo di
dover scrivere: **quando l'handler ritorna, nessun mod sta girando**, senza il quale l'ultimo mod che
ha risposto a un evento firma tutto quello che il motore fa dopo. Contare i cambiamenti e non le
chiamate è costato una correzione misurata: quattro messaggi dove ne erano dovuti tre, perché ogni
`add` su un pannello **chiuso** contava. E tre check erano verdi leggendo memoria liberata
(`ByteReader br(x.encode())`), quindi il costruttore che prende un temporaneo è ora `= delete` e il
sabotaggio che lo rimette **non compila**. Diciassette sabotaggi: quindici rossi, due che non
compilano. [`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §11.

**E un pannello sta su uno schermo, non su quello del mondo.** Era una sola assenza con tre cappelli:
`ui_show` non aveva un argomento per **a chi**, `core:ui_click` non diceva **chi** aveva premuto, e
`Panel` aveva un solo `open` — quindi due persone nello stesso mondo non potevano guardare cose
diverse e chi premeva «close» chiudeva anche all'altro. Il modello nuovo sta in una frase: **la
dichiarazione è condivisa, la visibilità no**. Due giocatori che aprono lo stesso baule guardano una
descrizione e due risposte indipendenti a «è aperto». `ui::PlayerId` è lo stesso `uint32_t` di
`net::PeerId` — un server nomina uno spettatore con la sessione che ha già in mano, senza una seconda
tabella da tenere allineata — e `kAllPlayers` **è** `kNoPeer`, che non può mai essere una sessione
vera, quindi «a nessuno in particolare» significa «del mondo» e le due cose non si scontrano. La
revisione è per spettatore: un avviso che cambia arriva a tutti, il baule di un altro a nessun altro,
e riscrivere un'etichetta è un cambiamento e tanti messaggi quante persone ce l'hanno su. Gli eventi
dei blocchi e del click portano un campo `player` che un mod **ripassa** a `ui_show` senza inventarlo
mai; nel client in un solo processo vale `kLocalPlayer`, così un mod scritto per un server gira lì
senza sapere dove sta. Misurato con due client veri su un server: `uno` ha il pannello, `due` non ha
niente. Dodici sabotaggi rossi — uno era verde alla prima prova perché toglieva il pannello dallo
schermo del *mondo* mentre la sezione che lo copriva usava uno spettatore vero.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §12.

**E un inventario che appartiene a un posto.** Una borsa appartiene a una sessione e se ne va con
lei; un baule appartiene a una **cella**, resta dov'era e due persone possono averci le mani dentro
insieme. Un `Inventory` per cella non è una struttura furba — è una mappa — ma è l'unica forma in cui
«ci stanno guardando in due» è un fatto e non una gara: c'è **un** oggetto, e una copia per
spettatore sarebbero due risposte a «quanti ce n'è nello slot 3». Serviva anche il gesto che mancava:
un click destro a mani vuote era rifiutato con «your hand is empty» — una frase su un piazzamento che
nessuno stava tentando — ed è ora `core:used`, che nomina il blocco colpito e non la faccia accanto.
Quel cambio ha rotto `bad_link`, e **è stata una mia regressione**: il passo a mani vuote misura
un'assenza, quindi non finisce quando arriva la risposta, e aspettare sei secondi ha spostato
l'attraversamento — cosa che il commento sulla camminata scriptata già avvertiva. Corretto mettendolo
per ultimo con una finestra sua. Sulla rete `MoveItem` cresce un **lato** e non un posto:
`Side::Container` è «quello che ho aperto», che il server già sa, quindi non c'è nessun posto nel
messaggio in cui mentire. La revisione è per cella, e il flag accanto serve per il caso che il numero
da solo non copre — due bauli toccati lo stesso numero di volte sono alla stessa revisione, e chi
chiude uno e apre l'altro continuerebbe a vedere il primo; il sabotaggio è restato **verde** finché
il gate non ha portato due bauli alla stessa revisione di proposito, e scrivendo quel gate
l'affermazione che avevo messo ha preso la mia aritmetica sbagliata. Sullo schermo ci sono due
griglie, quindi un numero di slot smette di nominare un quadrato: `BagPoint` porta lo slot **e** la
griglia. Misurato con due client veri su un server: entrambi hanno lo stesso baule aperto, alla stessa
cella. Ventuno sabotaggi rossi, tre verdi alla prima prova.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §13.

**Due difetti trovati giocando.** Il click destro sulla chest apriva il pannello e **non liberava il
cursore**: si vedeva il baule e il mouse continuava a muovere la camera. Il loop di rete liberava il
cursore e quello in un solo processo no — la stessa regola scritta due volte, che è come due copie
divergono. Inseguendolo è venuto fuori il secondo: **non c'era modo di chiudere un baule**. Un mod
poteva, un giocatore no — premere E nascondeva il pannello mentre il server lo teneva aperto e il
messaggio dopo lo rimetteva. Protocollo **14**, `CloseContainer`, che non porta niente, nemmeno quale
baule: il server sa già quale, e una cella nel messaggio sarebbe un controllo che può solo essere in
disaccordo con sé stesso. E aggiungere quel gate ha reso `net_loopback` rosso in modo deterministico
su un check di un'altra sezione: la sezione dei bauli pompa solo i due giocatori di cui parla, e il
peer ENet dell'osservatore andava in **timeout da sotto il test** — un gate che affama una sessione a
cui non sta guardando. [`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §14.

**Un click destro è prima un uso, poi un piazzamento.** Legare `core:used` alla mano vuota voleva
dire posare quello che si porta per aprire un baule. Ora `core:used` è **annullabile** ed è emesso
**prima**: un mod che annulla ha **reclamato** il click, e senza quella riga lo stesso click aprirebbe
il baule e costruirebbe sopra. Un `EditPlan` porta due celle — il blocco toccato e quella in cui si
costruirebbe — e l'uso **sopravvive a un piazzamento rifiutato**, perché un baule che non si apre
quando la cella sopra è occupata è un baule che funziona solo a mani vuote. Da lì una cosa non ovvia:
un rifiuto dietro un click reclamato **non va mandato**, quindi il livello delle sessioni lo **tiene**
invece di spedirlo. E il pannello non restituiva il mouse: lo prendeva in ognuno dei due loop e
nessuno dei due lo ridava — la stessa regola scritta due volte, la stessa causa del difetto del
cursore di §14.1. Ora è una funzione, `ui::panel_cursor`, e il gate esegue quella che eseguono i loop:
era verde finché non è esistita una funzione da sabotare.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §15.

**Un baule sopravvive a un riavvio, e il nome viaggia al posto del numero.** Il file del mondo aveva
tre tabelle e nessuna sapeva niente di un item: un mondo riletto da disco tornava con i blocchi e
senza niente dentro, che delle due perdite è la peggiore — il terreno che rivedi dice che è lo stesso
mondo. Una pila su disco è **`(nome, quantità)`**: un `ItemId` è denso e assegnato nell'ordine di
registrazione, e `registry_content_hash` copre solo la tabella dei **blocchi**. E un nome che il
registry non conosce più non si butta, si **conia come orfano** — che è esattamente la risposta che
`end_reload` dà già a un item il cui mod ha smesso di registrarlo: il disco pone la stessa domanda
del reload a caso mai chiuso, una settimana dopo invece di un secondo dopo, e quando il mod torna
`adopt_blocks` rianima il nome sull'id che le pile stanno già tenendo. Scrivendolo è saltato fuori un
difetto vecchio: le due strade di rianimazione rimettevano a posto i campi **uno alla volta**, e
`core:stone` usciva da un file del mondo impilandosi a 65535. Fare un baule e guardarci dentro sono
ora **due chiamate** (`container_make`, `container_add`), perché una cassa del tesoro si riempie
prima che arrivi qualcuno. Misurato dal binario vero, due run sullo stesso file, con un mod che
rifornisce **solo se trova la cassa vuota** — venti sabotaggi, tutti rossi.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §16.

**E la borsa appartiene alla chiave, non al nome.** DESIGN §4.4 lo diceva da prima che costasse
qualcosa — *la chiave è l'identità, il nome è un'etichetta* — e la persistenza è il primo posto dove
si paga: una borsa indicizzata per nome è una borsa che chiunque si prende scrivendo il tuo. La
tabella ha per chiave i 32 byte della chiave pubblica, e il gate entra con **lo stesso nome e una
chiave diversa** per chiedere una borsa vuota. Chi non ha chiave non ha borsa salvata, e senza quel
controllo tutti gli anonimi ne condividerebbero una sotto trentadue byte di zero. Scritta in tre
momenti — chi esce, chi viene buttato giù, e il **server che si ferma con la gente dentro**, che è
l'unico senza altra copertura: il gate ha una sonda che **non saluta**. Nove sabotaggi rossi, e uno
verde alla prima prova che non era un difetto ma **un gate rotto** — la sonda «osservatore» entrava
come giocatore, perché `ClientSession` scrive lo scope della config sopra quello dell'hello.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §17.

**Un baule si chiude allontanandosi, e rotto versa tutto per terra.** Rompere una cassa chiamava
`container_clear`, che prendeva il baule **e il contenuto** — e la persistenza l'ha peggiorato,
perché da quando un baule sopravvive a un riavvio quelle erano le uniche pile del mondo che non si
potevano più riavere. Ora sono due atti in ordine, `container_spill` e `container_clear`, sopra un
primitivo vero (`drop_item`) di cui è fatto anche un dispenser. Una pila per terra sparisce dopo
**cinque minuti**, contati da quando è atterrata: un timer che un passante rinfresca non scatta mai,
e un mondo dove niente scade si riempie di entità che vengono streammate a chi non c'entra.
Allontanarsi chiude il baule **nello stato in cui è** — chiudere è chiudere la vista — e la distanza
non è un numero nuovo ma la **portata**: *un baule che non arrivi più a toccare è un baule in cui non
hai più le mani*. Sistemandolo è saltato fuori un quarto difetto: cancellare un contenitore lasciava
la sessione che ci guardava puntata su quella cella, e il **prossimo** baule costruito lì si apriva
da solo sullo schermo di uno che non l'aveva toccato. Tredici sabotaggi rossi, uno che non compila, e
**due verdi alla prima prova** di cui uno era un gate che misurava niente — il baule condannato stava
fuori portata, quindi la regola appena scritta lo chiudeva prima della rottura.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §18.

**Il craft comincia dalla tavola, non dalla griglia.** Niente sullo schermo e niente sul filo: la
regola si decide con dell'aritmetica e qualche stringa, ed è la metà che un gate riesce a tenere
ferma — lo stesso ordine di [`panel.h`](../../src/core/ui/panel.h) un pezzo prima. Una ricetta tiene
**nomi**, e stavolta non per il disco: un mod registra le sue ricette mentre carica, e la forma item
di un blocco viene coniata **dopo** che tutti i mod hanno parlato, quindi una tabella costruita con
gli id sarebbe una tabella di ricette per niente. «La stessa forma **ovunque** nella griglia» non si
fa scorrendo il disegno a nove offset: si ritagliano **entrambi** al riquadro minimo e o sono la
stessa cosa o no — e il ritaglio si misura da fuori, perché la stessa pietra scritta in mezzo a un
tre-per-tre viene rifiutata come duplicato di quella scritta da sola. Un buco è una cella che deve
essere **vuota**, uno specchio è un'altra forma, e quando due ricette combaciano decide l'ordine di
registrazione — dichiarato e misurato nei due sensi, invece di una regola che qualcuno dovrebbe
imparare. Quindici sabotaggi rossi; due verdi alla prima prova, uno perché il gate guardava solo i
risultati e non gli ingredienti, e uno perché la riga sabotata era una scorciatoia e non una regola.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §19.

**Il server rispondeva e nessuno leggeva la risposta.** Segnalato giocando: «vicino al portale posso
rompere i blocchi ma non piazzarli». Non era il piazzamento — è [`mods/guard`](../../mods/guard/init.luau),
che protegge la piazza del portale da M2 e lo dichiara nella sua prima riga. Il difetto era un altro:
il server manda **una frase** con ogni edit rifiutato da quando i blocchi vanno sul filo, e nessuno
la disegnava — andava a `printf`, nel terminale, e solo le prime quattro. Un rifiuto che arriva a un
client e non viene disegnato è la stessa cosa di nessun rifiuto. Ora c'è `ui::Notice`, sopra la barra
e non sopra i suoi slot, con una frase sola (`refused_by_mod`) condivisa dai due loop — perché un
giocatore a cui vengono dette due cose diverse sulla stessa regola sta imparando due giochi. Sei
sabotaggi rossi, e uno verde che ha fatto **togliere** una riga: due guardie sulla frase vuota si
coprivano a vicenda, e sabotandole una alla volta erano entrambe verdi.
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §20.

**E la griglia, con la casella che non è una casella.** Quattro quadrati nell'angolo della borsa, un
[`craft:table`](../../mods/craft/init.luau) che li fa nove. Sul filo **non c'è nessun flag «tavolo aperto»**:
la misura è lo stato, e un client a cui arriva 3 lo sa — due campi che dicono la stessa cosa sono due
campi che possono non essere d'accordo. Cambiare misura **svuota** la griglia, e non è ordine: la
casella 3 è (1,1) in un 2×2 e (0,1) in un 3×3, quindi una griglia che sopravvivesse si riordinerebbe
sotto le mani del giocatore. Prendere il risultato è tutto-o-niente su una copia della borsa: chi
viene addebitato per un craft che non ha ricevuto sta peggio di chi si sente dire di fare spazio.
`kCraftResultSlot` è fuori dalla portata di **ogni** inventario, il che fa fare a una riga sola due
lavori. Diciassette sabotaggi rossi, e due verdi alla prima prova: uno era un gate che non
raggiungeva la riga sabotata (mutava un byte, e il conto dei byte rifiutava prima), uno una riga di
troppo, tolta. [`docs/M7_FINDINGS.md`](M7_FINDINGS.md) §21.

### M8 — ECS e API meccaniche, chiusa per intero

**Il gate è esistito prima del codice che doveva passarlo, ed è stato rosso per tutta la
milestone.** M7 ha chiuso con il suo test
d'uscita scritto per ultimo, e la prima run ha trovato ventidue minuti di giocatore che cadeva fuori
dal mondo. M8 comincia dall'altra parte: quattro mod in [`tests/mods_portal1/`](../../tests/mods_portal1/)
— pulsante, dispenser, pedestal, clock — un probe headless che ci cammina dentro
(`cnp_net_loopback_test --portal1`) e [`tests/portal1.sh`](../../tests/portal1.sh) che misura le quattro
meccaniche. In `ctest` è stato registrato e **`DISABLED`**: un test rosso lasciato nella suite la
trasforma in una cosa che si legge di sfuggita, e *Disabled* in ogni riepilogo è presente e
nominato senza mentire su un verde che non c'è. Quella riga è uscita quando l'ultimo dei
**quattordici** controlli è passato — headless, e l'ultimo è un `grep`: nessun file sotto `src/`
nomina un blocco, un item o un evento dei quattro. È partito da **4 verdi e 10 rossi**.

**Otto chiamate che non esistono, dette dai mod stessi.** I quattro mod non falliscono al
caricamento: sondano quello che gli serve e lo scrivono nel log, tutto in una run —
`meta_set`/`meta_get` (stato per cella, tre mod su quattro — **fatto**, sotto),
`players`/`player_pos` (dove sono i giocatori, due su quattro), `held_item`/`take_item`/`give_item`
(la borsa di chi offre qualcosa al pedestal), `find_nodes` (ritrovare le celle che un *giocatore* ha
piazzato, che è ciò per cui l'engine di partenza ha gli LBM). **Nessuna delle otto era nella lista scritta
leggendo `DESIGN.md`**: sono uscite dallo scrivere i mod, che è il motivo per cui il gate viene
prima.

**E il gate ha trovato un difetto in produzione alla prima run: un blocco che un mod cambia non
arrivava a nessuno.** Il clock contava — tredici passi nel log del server — e il probe leggeva il
display tutto spento. `host_set_block` scriveva nel mondo e non lo diceva a nessuno; l'edit di un
giocatore viene rimbalzato a tutti, la scrittura di un mod no. Non se n'era accorto nessuno perché i
mod girano solo sul client non networked, dove `set_block` scrive nel mondo che quello stesso
processo disegna: su un server [`mods/beacons`](../../mods/beacons/init.luau) accendeva un faro diverso
ogni frame da M2 e nessun giocatore collegato ne ha mai visto uno. **E c'era una seconda metà**:
l'edit del giocatore viene anche *rilluminato*, la scrittura di un mod no, e il mondo viene
illuminato una volta sola prima che i mod si carichino — quindi la lampada che `spike` piazza in
cima alla sua torre da M0 era spenta in **ogni chunk mai spedito**. Chiuso facendo passare le due
strade per una funzione sola; chi viene avvisato resta diverso apposta, perché il giocatore sta
aspettando la sua risposta e un handler che scrive cento celle non deve mandare cento pacchetti da
dentro sé stesso. Gate headless nel set di default ([`tests/mod_write.sh`](../../tests/mod_write.sh)),
quattro sabotaggi: tre rossi e **uno verde di proposito** — togliere la deduplicazione della coda
costa messaggi e non cambia nessuna risposta, quindi un gate rosso lì misurerebbe la cosa sbagliata.
`TODO.md` CLOSED-17, con il costo residuo aperto come OPEN-19.

**E quello che una cella ricorda esiste.** `core.meta_set`/`core.meta_get`, il pezzo più grosso
della lista: tre dei quattro blocchi lo nominavano, e con questo il gate passa da 4 verdi a **8** —
il clock riparte dal numero a cui si era fermato. **Stringhe, e solo stringhe**, non perché i numeri
siano difficili ma perché un pedestal che salva l'id 41 sta salvando qualunque cosa voglia dire 41
la prossima volta che il mondo si apre; un nome sopravvive all'aggiunta di un mod. **Cancellare è
scrivere niente**, che è una regola invece di due — leggere un campo mai scritto e leggerne uno
cancellato devono dare la stessa cosa. Rompere un blocco si porta via lo stato della cella,
cambiarne l'*id* no: un pulsante che passa da su a giù è la stessa cella con lo stesso stato. Una
`Cell` sola e non due — `ContainerAt` è ora un `using` di essa, così nessuno dei suoi novanta punti
di chiamata si è mosso per smettere di essere una seconda copia.

**E un evento nuovo, trovato facendo passare il gate: `core:world_ready`.** Un mod si carica
**prima** che il file del mondo venga aperto — il file è controllato contro la tabella dei blocchi
*finale*, quindi non si può aprire finché ogni mod non ha registrato i suoi — perciò un mod che
guarda il mondo mentre si carica guarda quello del mapgen. Il clock leggeva zero ogni volta e
ricostruiva sopra ciò che stava per essergli caricato addosso. È l'equivalente del problema per cui
L'engine di partenza ha gli LBM, e ne risolve metà: ritrovare le celle che un *giocatore* ha piazzato vuole
`find_nodes`, che è arrivata con la seconda metà della milestone (sotto).

**Il generatore ha imparato due forme di ritorno, e il pulsante si preme.** Dei sei rossi rimasti,
quattro cadevano su due chiamate sole — `players` e `player_pos` — e nessuna delle due si poteva
scrivere: `tools/api_gen` parlava solo «scalari dentro, uno scalare fuori». L'alternativa era
`player_count` + `player_at` + `player_x/y/z`, **cinque chiamate dove ne bastano due**, con ogni
futura chiamata a più valori a ripagare lo stesso prezzo. Ora conosce una **lista** (`ret = "int[]"`)
e **più valori** (`rets = {...}`), entrambe come out-parameter perché un puntatore a funzione C non
restituisce vettori. Un giocatore che non c'è **non ha una posizione**: `core.player_pos` non
restituisce niente, non `0, 0, 0` — l'origine è un posto vero, e un mod che ci misurasse una distanza
troverebbe tutti in piedi sul pulsante nell'angolo del mondo. Il gate M8 passa a **12 verdi / 2
rossi**: il pad si abbassa, il segnale attraversa da un mod a un altro, il vent lascia cadere un
cubo. **Cinque sabotaggi, uno verde e dichiarato** — e due erano verdi solo perché non compilavano,
con gli errori mandati su `/dev/null`: lo script adesso dice «build failed» invece di mentire.
[`docs/M8_FINDINGS.md`](M8_FINDINGS.md) §4.

**E dirlo a qualcuno costa: la chiusura di OPEN-17 ha fatto diventare rosso `bad_link`, il test
d'uscita di M6.** Quattro run su cinque, contro un tasso storico di circa una su sei. Isolato invece
che ipotizzato: con la sola coda disattivata e tutto il resto in piedi, **dieci su dieci verdi** —
la causa era mia. Ci sono volute **quattro** cose per tornare a otto su otto, e le prime tre da sole
lasciavano il rosso: un **canale tutto loro** (`Channel::Block`, protocollo 19 → 20) perché il
canale Event porta l'heartbeat ed ENet ordina *dentro* un canale — tredici heartbeat persi e 4.09 s
di buco nella run rossa, che è **esattamente la forma descritta dal commento su `Channel::Media`
scritto in M6**; mandare **solo i cambi veri**, perché un mod dice quello che *vuole* che una cella
sia e `mods/spike` scrive la stessa cella venti volte al secondo cambiandola due (ottanta messaggi
al secondo diventano dieci); **non dire a una sessione di una cella dentro un chunk che non ha
ancora ricevuto**, perché il chunk è codificato dal mondo com'è adesso e se lo porta dietro; e
soprattutto **il mondo prima della chiacchiera** — quello che una sessione è in debito *aspetta*
mentre le stanno arrivando chunk, e non viene buttato. Le vittime nelle run rosse erano proprio
quelle: la finestra verso l'altro server che non finiva mai di ricevere il mondo di là, e un
giocatore corretto contro una posizione rivendicata mentre il suo mondo era ancora per strada.
Entrambi i tipi di cambio sullo **stesso** canale, e non è un caso: due cambi alla stessa cella
devono arrivare in ordine, ed ENet ordina dentro un canale e non fra canali.

**Tre sabotaggi sulla nuova regola, uno verde di proposito** — e W2 («quello che è dovuto viene
buttato invece che trattenuto») ha richiesto tre tentativi per diventare rosso: una cella che
continua a cambiare si rimette a posto da sé e nasconde il cambio perduto, quindi il gate ha avuto
bisogno di una cella che cambia **una volta sola**, dentro la finestra in cui la sessione ha già il
chunk ma il mondo non è finito di arrivare. E `mod_write.sh` ha ripetuto un'ora dopo l'errore che
`portal1.sh` aveva appena fatto: leggere due contatori dal log di un processo a cui aveva appena
mandato un segnale, e ottenere zero.

**E l'ultimo pezzo: la mano, e la borsa.** `core.held_item(player)` **non è stata scritta**, ed è la
decisione giusta: il motore non sa cosa uno ha in mano se non nel momento del click — `Inventory`
tiene uno slot selezionato, ma sul server viene scritto solo quando un piazzamento *riesce*, quindi
fra due piazzamenti è lo slot dell'ultimo e non la mano. Una chiamata del genere avrebbe risposto
con qualcosa di vero un po' di tempo fa, che è peggio che non rispondere. Così la mano viaggia
**sull'evento**: `core:used` ha un campo `held` col nome di quello che chi ha cliccato aveva in
mano. Due chiamate invece di tre. `take_item`/`give_item` restituiscono **quanti se ne sono mossi
davvero** — una borsa può non avere quello che si chiede e può essere piena — e quello che non entra
**non viene lasciato cadere**: decide il mod, e il pedestal ci si appoggia (la cella lascia andare la
gun solo se la borsa l'ha presa, così una borsa piena la lascia sul piedistallo invece di farla
sparire).

**E due volte il gate ha misurato sé stesso, con il pedestal già funzionante.** Il probe cliccava dal
centro della piazza, che è *esattamente* i due nodi entro cui il piedistallo consegna: prendeva e
ridava al tick dopo, la borsa faceva 1 → 1 → 1 e il gate leggeva «non ha preso niente» mentre il log
diceva che aveva fatto tutte e due le metà. Spostato quattro nodi dietro… che è la cella del clock,
e una mossa dentro un blocco solido viene rifiutata, quindi il probe è rimasto dov'era ed è successo
di nuovo. Adesso offre **a distanza di braccio, di lato**.

**Otto sabotaggi, uno verde, e la colpa era di una riga che non poteva fallire.** «Un valore vuoto
viene salvato invece di cancellare» è rimasto verde perché il controllo era «rileggerlo dà una
stringa vuota» — e un campo *salvato* vuoto si rilegge vuoto anche lui. Ora è `fields() == 0`:
quello che distingue i due casi è se il campo c'è ancora, non cosa restituisce leggerlo. (La riga
accanto era rossa lo stesso, quindi il gate il sabotaggio l'aveva preso; a essere verde era la riga
che sosteneva di misurare proprio quella cosa.) E `portal1.sh` uccideva il server e poi chiedeva se
il clock si era ricordato qualcosa: quello che scrive il file è il passaggio allo shutdown, e un
server ucciso lì non ci arriva mai — il gate stava misurando il proprio SIGTERM.

**Quattro sabotaggi, il primo verde, ed è di nuovo la forma di §31.7.** «Uno dei quattro mod non si
carica» è rimasto verde: il gate contava le righe che cominciano per `mod '`, e un mod che fallisce
ne stampa una anche lui (`mod 'clock' not loaded: ...`). Quattro righe, tre mod caricati. Adesso
legge la frase che l'engine scrive dopo aver deciso (`3 mods running`). Sabotato anche il controllo
su cui poggia l'intera milestone — «`src/` non nomina nessuno dei quattro» — che era rosso **per il
motivo sbagliato**: cercava la parola *dispenser* e la trovava in due commenti, uno dei quali spiega
a cosa serve `container_add`. Ora cerca i nomi registrati (`dispenser:vent`, `clock:lit`,
`button:pressed`), che è quello che un caso speciale in C++ dovrebbe per forza scrivere.
[`docs/M8_FINDINGS.md`](M8_FINDINGS.md) §1.

**E poi l'altra metà, quella che il test d'uscita non toccava.** M8 si era chiusa sul suo gate con
la milestone dichiarata aperta a metà: i quattro blocchi classici dei puzzle a portali volevano stato per *cella*, non
entità proprie, quindi componenti e sistemi da Lua, fasi ordinate, render pass registrabili, canali
di rete per mod e `find_nodes` non erano stati scritti. Stessa forma: **due gate prima del codice**,
`DISABLED` e nominati in ogni riepilogo — [`tests/sentry.sh`](../../tests/sentry.sh) headless e
`cnp_client --pass-check` con i pixel. Il mod di prova
([`tests/mods_sentry/`](../../tests/mods_sentry/), la torretta classica dei puzzle a portali) **non muore sulla prima
chiamata assente**: le cerca tutte e ne scrive dieci nel log in una run sola, perché un mod che muore
subito nomina un buco e ne nasconde nove. Da **4 verdi / 9 rossi** a **16 verdi**.

**Un sistema è chiamato una volta per 512 entità, non 512 volte.** Un pellet è un'entità *del
motore* — flecs, con una `Position`, nello stesso mondo dei mob e delle pile per terra: un secondo
magazzino di entità sarebbe due spazi di id per una cosa sola, che è la forma in fondo a quasi tutti
i difetti di questo repo. Il batch è structure-of-arrays, perché dall'altra parte Lua riceve array e
ci riscrive dentro. E i sistemi girano in **fasi ordinate da un numero**, non nell'ordine in cui i
mod si sono caricati: il mod del gate registra i suoi due sistemi al contrario apposta. Dichiarati e
non scoperti dopo: un sistema nomina *un* componente (non c'è join fra due), i campi sono numeri, un
batch è al massimo 4096 entità, e le entità di un mod vivono nel mondo che possiede la tabella delle
sessioni — quindi un run offline non ne ha.

**Il canale che l'handshake portava da M3 e nessuno usava.** `core.net.open` / `core.net.send` /
`core:net_message`, protocollo 20 → 21. Il messaggio non porta il nome del mod: **il nome è il
canale**, che è quello che l'handshake ha distribuito, e un nome nel payload sarebbe una seconda
risposta a «di chi è questo». Chiesto al caricamento e rifiutato dopo, perché i numeri escono nel
welcome. E un mod che disegna: `core.render.pass` gira **dentro** il render e **una volta per
camera** — un ologramma disegnato da un handler di tick sarebbe assente attraverso ogni apertura —
mentre `core.render.box` fuori da un pass è rifiutata, perché lì non c'è camera montata e la scatola
finirebbe dove il frame precedente ha lasciato le cose.

**Sedici sabotaggi, tre verdi, e tutti e tre erano colpa del gate.** «Un nome che nessuno ha
registrato vale aria» restava verde perché *nessun controllo cercava quel rifiuto*. «Una entità per
chiamata» restava verde perché il gate confrontava il conteggio di chiamate della popolazione più
grande con quello della più piccola presi da **righe diverse**, e con una chiamata per entità i due
estremi cadevano sullo stesso tick. E «`kill` restituisce vero e non fa niente» restava verde perché
il mod contava le uccisioni che *chiedeva*: con `kill` inerte le richiedeva ogni tick e riportava
165 952 pellet raccolti — quello che distingue un kill vero è quanti pellet sono **nati**. Il gate
dei pixel ha il controllo **prima** del numero: due frame di una scena ferma senza mod che disegnano
differiscono di 0 pixel, il pass ne cambia 108 666 su 2 073 600, e sono controllati entrambi i lati —
un pass che rompesse le matrici della camera cambierebbe *ogni* pixel, che è anch'esso «qualcosa che
non c'era». [`docs/M8_FINDINGS.md`](M8_FINDINGS.md) §6.

### M9 — performance e robustezza, chiusa dai suoi tre gate

M9 era l'unica milestone del piano con una lista di feature e **nessun numero**. I tre gate sono
stati scritti prima del codice, con i numeri del giocatore: 16.7 ms è un sessantesimo di secondo,
50 ms è uno scatto che si vede.

| Gate | Numeri |
|---|---|
| `migration` (14) | la versione si legge **prima** che lo schema tocchi il file; dopo un rifiuto il file ha esattamente una tabella |
| `crash_evidence` (5) | il segnale è nominato, lo stack è scritto, e il processo **muore lo stesso** |
| `frame_budget` (21) | 512 chunk che si ricostruiscono tutti insieme: **4.23 ms** nel frame peggiore, recupero al frame 130 |

**Il profiler ha deciso il resto della milestone.** La sua proprietà è aritmetica — tutto ciò che
ha un nome più tutto ciò che non ce l'ha **è** il frame, e la differenza la calcola lui invece di
lasciarla a chi legge. Il primo numero che ha prodotto dice dove va un chunk: `snapshot` 0.32 ms,
`build` 3.12, `upload` 0.00. Il 90% è la parte che legge solo uno snapshot e il registry, cioè
l'unica che può lasciare il thread. Senza quella riga la scelta fra thread e LOD sarebbe stata
un'opinione. **F6** lo disegna in gioco, e dice di quale frame: questo non è finito finché
l'overlay stesso non è stato disegnato.

**Meshing su thread**, e il registry resta senza lock — un lock intorno alla ricerca di un nodo è un
lock preso un milione di volte per chunk; la regola è che niente sia in volo mentre cambia. La
tempesta passa da 8.44 a 4.23 ms nel frame peggiore, un blocco per frame da 3.86 a 0.76 ms, e il
mondo recupera al frame 130 invece che mai.

**Occlusion culling è una camminata, non un test.** Quindici bit per chunk dicono quali facce si
raggiungono fra loro, e il renderer entra in un vicino solo se poteva attraversare quello da cui
viene. Camera sigillata nella roccia: **4 chunk invece di 129**. Davanti a un muro: **2 invece di
154, e non un pixel diverso** — l'unico controllo capace di prendere il fallimento che conta, un
buco nel mondo, di cui nessun tempo di frame parlerà mai.

**Tre difetti trovati dalle misure nella prima ora di profiler.** Il budget di meshing si fermava a
«già speso» e sforava del novanta per cento (7.64 ms contro 4.0): la correzione non è una soglia più
larga ma la domanda giusta, *ci sta il prossimo?* Il controllo del budget leggeva il tempo
**proprio** della riga `mesh` dopo che aveva preso dei figli, e passava verde su 0.02 ms mentre il
meshing costava 3.45. E il walk attraversava i chunk che non conosceva, vagando fuori dal mondo a
ogni frame: **frame quieto da 0.42 a 2.93 ms**, sotto la soglia e quindi invisibile a tutti i
controlli — ora ha una riga sua e un tetto suo, e costa 0.023 ms.

**Un sabotaggio non è andato rosso: si è appeso**, ed è servito più degli altri. Con `submit()`
costretto ad accettare lavoro su un pool fermo, `drain()` aspettava un conteggio che nessun worker
era rimasto a far scendere. Aspettare lavoro che nessuno può fare è un deadlock, e un gate deve
fallire, non appendersi.

**E una premessa del piano era falsa.** «Sottoterra, dentro il terreno che ha fatto il mapgen» non è
una camera dietro la roccia a scala di chunk: questo mondo è alto 64 e un chunk 32, quindi sono due
strati e ogni chunk di quello inferiore arriva al cielo aperto. La camera era sepolta e il suo
**chunk** no. Il gate adesso la roccia se la costruisce.

**Resta aperto e dichiarato**, in [`docs/M9_FINDINGS.md`](M9_FINDINGS.md): **LOD lontano e
reverse-Z non sono stati scritti**, perché erano mezzi e il budget non li ha chiesti — il momento di
farli è quando un numero li chiede.

**E il walk sa in che pezzo d'aria si trova.** Era a grana di chunk, e i quindici bit che una
faccia vede l'altra sono l'**unione** di quello che ogni pezzo d'aria del chunk sa fare: in un mondo
alto 64 con chunk da 32 ogni chunk dello strato inferiore contiene anche il cielo aperto sopra il
terreno, quindi quell'unione dice che da ogni faccia si vede ogni altra per quanta roccia ci sia in
mezzo. Adesso le regioni si tengono invece di sommarle, e il walk porta con sé quella in cui sta:
una caverna esce dalle facce della caverna, non da quelle del cielo. Da dentro un corridoio a
sei blocchi di profondità si disegnano **16 chunk contro i 201 che il frustum raggiunge**, con la
stessa immagine al pixel. Dalla superficie continua a non scartare niente, ed è l'altra metà della
stessa frase: un occlusore deve essere grande come un chunk, e le colline del mapgen sono dieci
blocchi.

Il prezzo è dichiarato e misurato. Ogni regione porta le facce che tocca, una maschera di celle per
faccia — due chunk si accordano sul piano che li separa cella per cella, ed è questo che distingue
la caverna dal cielo dietro la stessa faccia — e una mappa grossolana a 4×4×4 celle di dove si
trova, che serve a sapere in quale pezzo sta la camera senza tenere un id per cella (32 KB a chunk
per rispondere a una domanda al fotogramma). Sessantaquattro regioni per chunk, una per bit della
maschera: oltre, il chunk tiene i suoi quindici bit e non dice altro. Il walk passa da 0.023 ms a
**0.095 ms** a fotogramma tranquillo, 0.142 ms al p99. Cinque sabotaggi, tutti rossi; il sesto è
rimasto verde ed è scritto perché è rimasto verde. [`TODO.md`](TODO-storia.md) CLOSED-23.

E ha scoperto tre pixel di cielo attraverso la roccia: ci sono anche con il walk spento, e finché
si disegnava tutto il frustum c'era sempre un chunk dietro a tapparli. Non è un buco nella
geometria e non sono le cuciture fra i chunk, che è quello che si era sospettato per primo: la
stessa vista disegnata a sette risoluzioni della stessa forma ne mostra dieci a 1920x1080 e **zero
alle altre sei**, e spostando l'occhio di un decimillesimo di blocco -- un cinquantesimo di pixel --
non se ne vede più nessuno. Un varco ha una LARGHEZZA e su una griglia più fine si vede di più; qui
è un centro di pixel che cade esattamente su uno spigolo proiettato, cosa che questa camera
combina perché sta esattamente al centro di un voxel e guarda esattamente lungo un asse. Misurato
con un sabotaggio che la cucitura vera la fa: ogni mesh spostata di un cinquecentesimo di blocco dà
3 pixel a 960x540, 5 a 1920x1080 e 13 a 2880x1620 ([`TODO.md`](TODO-storia.md) OPEN-37).

**E sotto la pietra adesso c'è qualcosa.** `mods/cave` scava gallerie da uno stage di mapgen
scritto in Lua — una caverna per quadrato di 32 colonne, scelta *dentro* il quadrato invece che
«una colonna su N», così il numero è una proprietà della dimensione del mondo e non della fortuna
del seme: 256 gallerie e 95044 celle su un mondo da 512, lo 0.57%. Il passaggio è una croce alta
due celle, il tetto resta sei blocchi sotto il terreno **della colonna in cui la galleria si trova
adesso**, e si scava solo quello che è `core:stone`.

Si carica prima di `mods/ore` per ordine di nome, ed è la parte che decide il gioco: prima le
gallerie, poi il minerale riempie la pietra rimasta, quindi una vena finisce nella *parete* di una
caverna invece che dentro un blocco che nessuno guarderà mai. Costa 0.05 s in più a generare un
mondo da 512 e, sul link disturbato di `bad_link`, niente che si distingua dalla dispersione del
gate stesso (50.5/50.8/52.3 KB con le caverne, 53.8/52.9/56.5 senza). Tre sabotaggi rossi e tre
guardie che questo terreno non sa far scattare, elencate una per una in [`TODO.md`](TODO-storia.md)
OPEN-35.

**57 test in `ctest`, tutti verdi, e per la prima volta niente `DISABLED`.**

---

### Arte di qualcun altro: pacchetti di texture e un personaggio

**Un pacchetto è una cartella, e adesso non deve chiamare i suoi file come li
chiama questo engine.** `pack.txt` dice `core:stone = stone.png`, e le due
regole convivono con quella vecchia: un blocco che il foglietto non nomina tiene
il nome derivato dal proprio, e `assets/textures` — che non ha foglietto —
continua a funzionare com'era. `--textures <cartella>`, ripetibile, e **F7**
passa al successivo mentre il gioco gira.

**L'atlante prende la dimensione di tile del pacchetto, non la propria.** Le
immagini si decodificano *prima* che l'atlante esista, perché allocarlo a 32 px
e scoprire poi che l'arte è a 128 lascia due scelte e sono sbagliate entrambe:
buttare l'arte, oppure ridurla a un quarto del dettaglio con cui è stata
disegnata senza dirlo. Vince la dimensione su cui la maggioranza dei file è
d'accordo — non «la prima», che sarebbe l'ordine di registrazione, e allora
installare un mod qualunque cambierebbe la risoluzione di tutto il mondo. Un
file a una dimensione che il resto del pacchetto non usa viene **saltato, non
riscalato**. Il pacchetto CC0 di Kenney, 128 px, senza ritoccare un file:
`atlas: 8 x 8 tiles at 128px (1024px square), 19 from the pack, 20 generated`.

**E un blocco non porta per forza la stessa immagine su tutte e sei le facce.**
Una sopra, una sotto, una di lato — oppure sei diverse — e a deciderlo è *quali
file esistono*, non una dichiarazione: ogni faccia cerca il proprio file
(`core_grass_top.png`), poi quello del suo gruppo (`core_grass_side.png`), poi
quello semplice del blocco. Un pacchetto che non aggiunge niente si comporta
esattamente come prima; uno che aggiunge una cima divide il blocco senza che
nessuno abbia dichiarato che l'erba ha una cima. La dichiarazione sarebbe un
secondo posto dove vive la verità, e i due sarebbero in disaccordo la prima
volta che un pacchetto spedisce un file di cui la tabella dei nodi non sa
niente. **Un tile per immagine distinta, non per blocco**, con il tetto a 256
perché l'indice del tile sta in otto bit nel vertice — e le immagini che non ci
stanno vengono contate e dette, mai avvolte in silenzio.

I contatori non distinguono una cima da un fondo: tre immagini su un blocco sono
tre tile in entrambi i casi. Quindi il gate costruisce **tre strutture di pietra
lontane fra loro** e le fotografa una faccia alla volta — guardando in giù, di
lato, in su — e ogni fotogramma deve essere **al 100%** del colore che quella
faccia ha ricevuto. Un sabotaggio che scambia cima e fondo passa ogni conteggio
e muore lì.

**E poi un colore piatto non distingue una figura da sé stessa capovolta.** Con
l'arte di Kenney addosso, il lato di ogni blocco d'erba aveva la terra sopra e
il verde sotto — visto da una persona che guardava lo schermo, che è esattamente
il fallimento che un gate dovrebbe rendere impossibile. Il mesher percorre una
fetta con due assi ricavati ruotando l'indice dell'asse, e per tre facce su sei
quella base non è quella che vuole una **figura**: +Z veniva capovolta, −Z
girata di mezzo giro, le due facce X trasposte. La regola adesso è scritta una
volta sola: *la u dell'immagine va verso destra guardando la faccia da fuori, la
v va in giù* — `cross(forward, up)`, derivata e non scelta.

Il gate che l'avrebbe presa esiste adesso: **un blocco da solo in aria con una
tile a quattro angoli di colore diverso**, fotografato da tutte e sei le facce.
Rosso in alto a sinistra, verde in alto a destra, blu in basso a sinistra,
giallo in basso a destra — e i tre modi di sbagliare spostano coppie diverse:
capovolta scambia rosso e blu, specchiata scambia rosso e verde, ruotata manda
il rosso in un angolo qualunque. Tre sabotaggi, tutti rossi, ciascuno esattamente
sulle facce che rompe.

**Il bersaglio di una riga è un nome di file, e basta**: niente `/`, niente
`..`, niente punto iniziale. Non è una comodità — un pacchetto è contenuto, il
contenuto qui arriva anche dal filo, e un foglietto che potesse nominare
`../../../etc/passwd` leggerebbe tutto quello che legge il giocatore. Il gate
prova otto modi diversi di uscire dalla cartella, **uno per controllo**, perché
un parser che si ferma a `/` ne prende quattro e lascia passare gli altri; e il
gate a schermo ne mette uno con il file davvero lì fuori, così il rifiuto è
misurato contro qualcosa che si sarebbe trovato.

Tre gate, diciannove sabotaggi in tutto, **tutti rossi**: l'atlante che ignora
la dimensione del pacchetto, la prima dimensione vista che decide per tutte, il
file scalato invece che saltato, la regola sui percorsi tolta, il parser che si
arrende alla prima riga rifiutata, il foglietto letto e poi non usato, la
texture vecchia tenuta dopo il rebuild; e per le facce: la catena che salta il
gruppo, la faccia che non chiede mai un file suo, cima e fondo scambiati, un
tile per faccia invece che per immagine, due segnaposti diversi che ne
condividono uno, i numeri dei tile mai passati al mesher, il tetto che non è un
tetto. `pack_draw` scrive da sé i tre
pacchetti che sceglie — colori piatti, così «il pacchetto è sul mondo» è un
conteggio di pixel e non di file — e verifica anche che tornare indietro
riporti il frame **identico al pixel**.

**E un personaggio si carica.** Il mannequin UE (CC-BY-4.0) entra con
`--model`: 4106 vertici, 2088 triangoli, **55 giunti**, 1.38 × 1.82 × 0.36
blocchi — l'altezza di un giocatore, senza niente da scalare. Il secondo
modello provato è stato **cancellato**: CC-BY-ND vieta di distribuire versioni
modificate, e un personaggio skinnato non è altro che versioni modificate.

**E un blocco con un davanti si gira.** Dichiararlo `rotatable` lo registra
**quattro volte**, uno per quarto di giro, e piazzarlo mette giù quello il cui
davanti guarda chi l'ha messo. I tre id in più portano le immagini del primo
**permutate**: non costano un tile, non costano niente sul filo e niente nel
file di mondo — perché una cella di questo mondo è un id e nient'altro, e dare
un orientamento alla cella vorrebbe dire cambiare `Chunk`, il codec, lo schema
del database e la versione del protocollo. Il prezzo sta da un'altra parte ed è
dichiarato: chi chiede «che blocco è questo» deve chiedere la base
(`core.node_base` in Lua, `base_of` nel motore). `find_nodes` lo fa già da sé.

Il giro si decide dalla **posizione** di chi piazza e non da dove sta guardando,
perché il server ha la prima e non la seconda — e una regola raccontata in due
modi sono due giochi. Quindi è misurata in tutti e due: il gate a schermo piazza
il blocco dai quattro lati con la stessa chiamata di un click destro e fotografa
da davanti e da dietro (**0.353 magenta verso chi l'ha piazzato, 0.000
dall'altra parte**, quattro volte su quattro), e il gate `sentry` lo piazza
**attraverso un socket** e ritrova `sentry:turret^3` in tutte e due le celle.
Otto sabotaggi in più, tutti rossi, incluso l'anello che gira dal verso
sbagliato e la rottura che chiede l'oggetto dell'id girato invece che della
base.

**E la cassa ha un davanti.** `chest:box` è il primo blocco spedito che si gira:
i suoi due handler chiedono `core.node_base(ev.id)` invece di confrontare l'id
che `register_node` gli ha dato, così una cassa si apre e versa quello che ha
dentro **da qualsiasi parte guardi** — e il pacchetto Kenney le dipinge il
davanti. Il prezzo è misurato, non supposto: tre id in più spostano gli id di
ogni mod caricato dopo, e un file di mondo scritto da prima **viene rifiutato
per nome** («world file was made by a build with a different block table»), che
è `content_hash` — la stessa regola che l'handshake applica sulla rete.

**E un personaggio viaggia.** Un mod dichiara un modello
(`core.register_model("mymod:persona", "persona.gltf")`) e dice chi lo indossa;
il server ne pubblica i **nomi** nel saluto e ne serve i **file** — il `.gltf` e
tutto quello che nomina accanto a sé — per la stessa strada delle texture; un
corpo sul filo porta il suo id (`EntityState::model`, protocollo **22**); e un
client che quel file non l'ha mai avuto lo ricostruisce da quello che è
arrivato, in una cartella intitolata all'hash del `.gltf`. Misurato:
`models: 1 named by this world, 1 from it, 0 from this machine`.

Un modello si serve **intero o niente**, da tutte e due le parti: mezzo modello
è un personaggio con un buco, e chi lo guarda non può sapere quale file manca.
E i nomi dei media sono **piatti**, con i punti e mai con le barre — il primo
schema usava `model/<nome>/<file>`, e il parser rifiuta un nome di media con un
separatore dentro: un modello in un mondo costava al client **tutta** l'offerta,
texture comprese, in silenzio. Il gate dei media adesso passa i nomi per il
filo, e quel controllo sarebbe stato rosso al primo giro.

**E adesso i giri sono ventiquattro.** Un blocco dichiara **cosa significa** il
byte della cella: niente, `upright` (0..3, i quattro giri attorno alla
verticale), `facedir` (0..23, una qualsiasi orientazione di un cubo) o
`wallmounted` (0..5, la **faccia** verso cui punta il davanti). Un'orientazione
è `asse * 4 + giro` con l'asse che dice dove finisce il +Y del blocco, e
**0..3 sono esattamente i quattro giri che c'erano**: ogni byte in ogni file di
mondo mantiene il significato che aveva. L'ordine degli assi è quello di
`param2` facedir dell'engine di partenza, che è l'engine da cui questo progetto viene: una
convenzione già scritta e già provata, presa perché è buona e non perché qualcosa
debba restare compatibile.

La tabella 24×6 è **derivata** da matrici di rotazione intere a tempo di
compilazione, e l'inversa che chiede il mesher è la tabella diretta girata, così
le due non possono dissentire. Il gate non controlla una riga ma le **proprietà**
di un insieme di rotazioni del cubo: ventiquattro permutazioni distinte, facce
opposte che restano opposte, l'inversa che è l'inversa, e ogni elemento una
rotazione e non una riflessione — che è l'unico controllo capace di prendere uno
specchio. Anche le **scatole** di un `node_box` girano con quella stessa matrice:
forma e immagini non possono dissentire su come è messo il blocco.

Il piazzamento scrive ancora quattro giri dalla posizione di chi lo mette, e per
un `wallmounted` la **faccia cliccata** — che è già sul filo, perché la cella
bersaglio si deriva da lei. Protocollo 25 → 26. Un byte che il blocco non ammette
viene rifiutato (`core.set_facing` lo dice in log col nome del mod) e, al
disegno, si disegna non girato invece di leggere fuori da una tabella.

**E qualcosa che si spedisce si gira.** `mods/timber`: un tronco `facedir`
coricato su tre assi e un pannello di pioli `wallmounted` montato su tutte e sei
le facce, misurato sul **mondo** e non sul racconto che il mod fa di sé. La
grafica segnaposto ha imparato una faccia per l'occasione — anelli sul taglio del
tronco, un bersaglio sul davanti dei pioli — perché un segnaposto è identico su
tutte e sei le facce e un'orientazione che non si vede è un'orientazione che
nessuno controlla.

Nove sabotaggi rossi, due dei quali **verdi al primo giro**: comporre giro e
inclinazione al contrario sopravvive a ogni proprietà (è un'altra etichettatura
dello stesso gruppo) e l'ha preso solo il controllo che l'asse dice dove va il
**sopra** del blocco; e nessun gate disegnava un blocco a scatole `facedir`,
quindi `parts_draw` adesso ne ha uno, in piedi e coricato.

**Resta aperto e dichiarato:** niente gira una cella già piazzata (un cacciavite
è un item e non esiste), i quattro giri attorno al davanti di un `wallmounted`
non si distinguono (il byte è una faccia, quindi un cartello non può essere
capovolto), e i quattro giri del piazzamento vengono dalla **posizione** di chi
piazza, che è l'unica cosa che il server ha. I tile
si deduplicano per
nome di file e non per contenuto, quindi sei blocchi mandati sullo stesso file
costano sei tile identici (OPEN-26); un pacchetto
non viaggia come pacchetto, il server ne serve i file uno per uno sotto il nome
del blocco; nessuna mipmap, e a 128 px il `NEAREST` in lontananza si vede più di
quanto si vedesse a 32; entrambi i rig hanno **zero animazioni**, quindi lo
skinning gira e non c'è niente da suonarci.
[`docs/TEXTURE_PACKS.md`](../TEXTURE_PACKS.md).

---

### M10 — distribuzione, iniziata: un mod dice per quale motore è scritto

**Una versione adesso si confronta.** `version` in `mod.conf` era una stringa
libera, stampata nella riga di log e letta da nessuno: `depends` poteva chiedere
solo un **nome**. Ora è tre numeri — `1`, `1.2` e `1.2.3`, e quello che non è
scritto è zero — e `depends = spike >= 0.2, guard` chiede una versione. Il
motivo per cui non è più una stringa sta tutto in una riga: come stringa `1.10`
viene prima di `1.9`, che è il contrario di quello che intende chi scrive
`>= 1.9`.

**Due fallimenti, due frasi.** «Depends on 'spike', which is not loaded» manda a
installare un mod; «depends on spike >= 2.0.0, and the one loaded is 0.2.0»
manda ad aggiornare quello che c'è. È la stessa regola del rifiuto sul livello
dell'attrezzo, che nomina tutti e due i numeri invece di dire «serve un attrezzo
migliore».

**E `api` è il numero su cui si rifiuta.** Un intero, non una versione: dice per
quale API mod il mod è scritto. Questa build ne serve una sola e qualunque altra
viene rifiutata con tutti e due i numeri nella frase — che è quello che serve il
giorno in cui la API cambia forma invece di crescere soltanto: un mod scritto per
quella vecchia deve essere **rifiutato al caricamento**, non fallire alla riga
40 con mezzo mod già dentro il mondo. Un manifest che non lo dichiara viene
preso per la **più vecchia** servita, mai per la corrente, e il log dice
`(api 1, assumed)` così le due cose si distinguono.

La stessa regola vale per un reload, scritta una volta sola: un `mod.conf`
modificato in qualcosa che questa build non serve non entra, e continua a girare
la versione che c'era. Tutti e dieci i mod spediti dichiarano `api = 1` e
`mods/beacons` chiede `spike >= 0.2`, perché un meccanismo che non gira su
niente di spedito è un meccanismo che non ha girato. Gate nuovo `mod_package`,
otto sabotaggi tutti rossi su controlli distinti, [`TODO.md`](TODO-storia.md) OPEN-39.

**E un mod viaggia come un pezzo solo.** `.cnpmod` è un mod in un file, e
`cnp_modpack pack mods/ore` lo scrive: `ore-0.1.0.cnpmod`, 9047 byte di file in
3379 byte di pacchetto. Messo nella cartella dei mod si carica come una
directory — il motore lo verifica **intero**, lo scompatta in una cartella di
appoggio, e da lì in poi è il percorso di sempre: stesso manifest, stesso
ordine, stessi rifiuti.

**Il nome del mod non è scritto sul pacchetto.** Sta nel `mod.conf` dentro, che
è l'unico posto da cui si legge un manifest: un nome in due posti sono due nomi,
e quello fuori è quello che può mentire. Il nome del *file* lo decide il tool
leggendo quel manifest, ed è per questo che il tool linka il parser e il formato
no. Il formato invece è **canonico** — file ordinati per percorso, byte
contigui, niente spazi vuoti — quindi impacchettare due volte lo stesso mod dà
lo stesso file byte per byte, altrimenti l'hash di un pacchetto direbbe «chi
l'ha fatto» invece di «cosa c'è dentro».

**È la prima cosa in questo motore che arriva da fuori**, quindi il prodotto
sono i rifiuti: non è un pacchetto, header troncato, payload troncato, un byte
incollato in fondo, un formato di una build più nuova, un indice non ordinato o
che non rende conto di ogni byte, un contenuto che non torna con l'hash
dichiarato, un percorso che esce dal mod (controllato tre volte: quando si
scrive, quando si legge, quando si scompatta), un pacchetto senza `mod.conf`
dentro. Mezzo mod è peggio di nessun mod, che è la stessa regola dei modelli di
M9.

Gate nuovi: `mod_package` per il formato e `mod_pack` per i binari veri — quello
impacchetta `mods/ore` con il tool, dà al server una cartella con dentro solo
quel file, e confronta gli **hash del mondo**: `e9fbdf860bd4129d` dalla
directory e dal pacchetto, che è come si dimostra che il mod ha *girato* e non
solo caricato. Dieci sabotaggi rossi, e due rimasti verdi che hanno riscritto il
gate — un byte cambiato nel payload ferma zstd prima che l'hash venga
guardato, quindi adesso ne cambia anche uno dell'hash.

**E un binario copiato adesso legge i propri file, non quelli di chi l'ha
compilato.** Ogni percorso era inchiodato a `CNP_SOURCE_ROOT`, cioè alla
directory di build: su un'altra macchina quella directory non c'è e i mod
semplicemente non ci sono; sulla stessa macchina c'è, ed è peggio, perché una
copia del gioco legge un checkout che qualcuno sta modificando e non lo dice.
`core/paths.h` guarda prima **accanto al programma** (`/proc/self/exe`, non
`argv[0]`: argv[0] è quello che il chiamante ha voluto scrivere), poi qualche
livello sopra, e solo alla fine l'albero da cui è stato compilato — e **dice
quale ha scelto**: `mods from <dir> (next to the program)`.

**Il runtime C++ entra nel binario.** `-DCNP_PORTABLE=ON`: libstdc++ è l'unica
libreria che una distribuzione non può dare per scontata, e un programma
compilato contro una più nuova muore all'avvio su una macchina più vecchia con
un messaggio su una versione di GLIBCXX che non nomina né il programma né la
soluzione. Resta dinamico quello che è della macchina — libc, e GL e X11 che
sono del driver. `tools/dist/package-linux.sh` mette i tre programmi, `mods/` e
`assets/textures` in una directory sotto i nomi che i binari cercano accanto a
sé, e ne fa un tar: **8,4 MB**. E **rifiuta** una build che linka libstdc++
dinamicamente, nominando il flag.

Il gate `dist` è il primo che non esegue un binario dal proprio albero di build:
impacchetta, scompatta altrove, lancia il server dalla **radice del
filesystem** — né l'uno né l'altro — e controlla che il log dica di aver letto i
mod accanto a sé, che il mondo che ne esce sia quello dell'albero di build, e
che niente di quello che i programmi linkano punti dentro la directory di build.
Quattro sabotaggi rossi, e un controllo che da solo non può fallire, scritto
proprio per quello.

**E la documentazione dell'API è stata interrogata, non dichiarata.** Quello che
passa dal generatore è documentato per costruzione; cinque cose non ci passano —
`core.event.*`, `core.ecs.*`, `core.net.*`, `core.render.*` e
`core.register_mapgen`, perché prendono tabelle e funzioni — e stanno nella prosa
di MODDING.md, che non segue niente. Il gate `api_docs` costruisce una VM come fa
il motore, cammina la tabella `core` **che un mod vede davvero** (dopo la
rimozione delle capability e il congelamento dei globali) e controlla nei due
versi: tutti i **60** nomi sono scritti in uno dei tre file che un autore di mod
legge, e ogni titolo della guida generata esiste ancora. Il secondo verso è
quello che marcisce in silenzio — una chiamata rimossa lascia un paragrafo che
dice di usarla. Tre sabotaggi rossi. Quello che **non** controlla: se quello che
la documentazione dice è vero, e non solo che il nome c'è.

**Resta aperto e dichiarato:** un pacchetto **non si ricarica a caldo** (si
edita una directory, si spedisce un pacchetto); non esiste un posto da cui i
pacchetti *vengano* — niente indice, niente firma, niente decisione di fiducia,
e un hash dice che è arrivato intero, non chi l'ha fatto. Windows non si compila
qui; AppImage e Flatpak non si fanno, perché `appimagetool` e `flatpak-builder`
non sono su questa macchina e quello che impacchetterebbero è la directory che
lo script già produce.

---

---

### Cose che accadono alle celle da sole

**ABM e LBM.** `core.register_abm(nome, blocchi, interval, chance, azione)` gira
ogni `interval` secondi su ogni cella caricata che contiene uno di quei blocchi,
con probabilità uno su `chance`; `core.register_lbm(nome, blocchi, azione)` gira
una volta per cella **quando il chunk viene caricato**, e mai più. È il
meccanismo di cui è fatta la maggior parte dei contenuti di qualsiasi gioco a
celle — l'erba che si espande, una fornace che cuoce, un mondo sistemato mentre
si apre. Era nella lista che M11 chiedeva all'engine; quella milestone non si fa
più e questo resta, perché un mondo in cui non succede niente senza che qualcuno
clicchi non è un mondo.

Tre proprietà decidono se un mondo può essere lasciato girare con uno di questi,
e stanno tutte e tre nello scheduler:

| | |
|---|---|
| Non legge tutto | un bit per id di blocco **modulo 64** per chunk: una cella su 64 chunk si trova leggendo **32768 celle su 2097152** |
| Non fa tardare il tick | budget di **2 ms** per step, si ferma dov'è e riprende; l'intervallo riparte solo da una spazzata **finita** |
| È deterministico | la probabilità è un **hash** di seme, numero di spazzata e coordinate: budget intero o quasi nullo, le stesse celle nello stesso ordine |

Il filtro è conservativo in due direzioni, dichiarate: gli id collidono modulo
64 e un bit non viene mai spento, quindi un bit acceso vuol dire *forse* e il
prezzo di sbagliare è una scansione che non trova niente. Otto byte per chunk e
un OR per scrittura — una lista degli id distinti sarebbe una scansione di
quella lista a ogni scrittura, e il mapgen ne fa sedici milioni.

**E qualcosa che si spedisce li usa entrambi.** `mods/timber` fa crescere il
muschio su un tronco ogni due secondi (uno su quattro) e lo raschia via quando
il mondo viene riaperto — sui propri blocchi e su nient'altro, cioè cinque celle
messe dal mod stesso. Il gate `abm` fa crescere il muschio per 300 tick, scrive
il file di mondo, lo riapre, e verifica che l'LBM abbia girato **esattamente**
sulle celle che l'ABM aveva cambiato.

**E la luce di un run di scritture si fa una volta.** `host_set_block` rifaceva
la luce attorno a ogni cella scritta da un mod: 1,9 ms per cella, 1,36 s per 640
celle, 69 s per un chunk — cioè un mod che costruisce qualcosa era inutilizzabile
(TODO.md OPEN-29). Due vie d'uscita, indipendenti. La prima è **esatta**: il
passaggio di luce legge due cose di un blocco, se è opaco e cosa emette, quindi
una scrittura in cui vecchio e nuovo sono d'accordo su entrambe non può cambiare
niente. La seconda è il **run**: le celle si accumulano in scatole e la luce si
fa una volta, alla fine, con al massimo sedici scatole perché il lavoro resti
limitato qualunque cosa faccia un mod.

Misurato su 776 scritture — un muro da 640 celle, otto lampade all'aperto, un
tetto, e 64 scritture che la luce non distingue: **1,808 s una passata ciascuna
contro 0,015 s in batch**, su 220 528 celle in quattro scatole.

**E il batch non è solo più veloce, è più giusto** — che non era la tesi di
partenza. Il per-scrittura non è un riferimento: una passata semina gli
emettitori **dentro** la propria scatola, quindi una passata successiva su una
regione sovrapposta cancella la luce che veniva da una lampada fuori dalla
propria. Contro una passata completa sul mondo, il batch differisce in **0 celle
e il per-scrittura in 626**.

Cinque sabotaggi rossi, **tre dei quali verdi al primo giro**, e ognuno dei tre
ha comprato un controllo: la fixture non riusciva a vedere il margine (il sole
cade in verticale, quindi un muro spesso una cella non fa ombra di lato e una
lampada nella roccia non illumina niente), il rifiuto di aprire una scatola al
tetto rendeva la fusione **codice morto**, e niente controllava che un flush
svuotasse il batch.

**Resta aperto e dichiarato:** niente batch (una chiamata in Lua per cella, come
non fanno i sistemi ECS di M8, che prendono colonne proprio per questo); nessuna
nozione di «i chunk vicino a un giocatore prima», quindi una spazzata è equa e
non utile; e un chunk che arriva a metà spazzata non viene visitato da quella
spazzata.

**E la forma di un blocco non è più un disegno della forma.** `core.node_box`
esisteva dalla cassa e arrivava soltanto al mesher: una lastra era il *disegno*
di una lastra, la si calpestava mezzo nodo più in alto, la metà vuota della sua
cella era un muro, e mirare al cielo sopra di lei la selezionava. Adesso le
scatole sono il blocco — `core/registry/node_shape.h` è l'unico posto che dice
dove sono, ruotate come la cella, con la stessa matrice del mesher — e le leggono
la collisione, il validatore del server e il raycast, che prima erano tre copie
di «questa cella è solida». Il contorno si disegna attorno alle scatole: **5729
pixel, righe 460..685, attorno a un cubo contro 2814, righe 498..653, attorno
alle sue scatole**. Misurato dove conta: 177 957 sonde contro lastra, mensola e
cubo e 81 000 su tutte e ventiquattro le orientazioni di una forma a due scatole,
**0 disaccordi** con quello che verrebbe disegnato. Otto sabotaggi sul motore,
due sul contorno, quattro sul gioco.

**E il gioco ce le ha dentro:** `mods/steps` mette una lastra e una scala nella
piazza, di pietra e fabbricabili, perché un gate che si registra la propria
lastra misura la regola e non il gioco — è la stessa lezione di `mods/tools` per
la durezza. `tests/shape.sh` lo chiede a un server vero: stare **sopra** la
lastra, mezzo nodo dentro la sua cella, è accettato; mezzo nodo più in basso è
rifiutato; e i due gradini della scala sono la stessa cella alla stessa altezza a
un nodo di distanza in z, uno accettato e uno no.

**E ci si cammina sopra:** un corpo sale da solo fino a **0.55 di nodo** -- la
lastra e il gradino della scala -- e un nodo intero no, perché un cubo deve
restare un muro. Il passo è del risolutore e non delle forme, si prende solo da
fermi a terra, mai contro un soffitto, e posa il corpo sulla superficie invece di
lasciarlo sospeso: un passo solo lascia i piedi a **4.501** su una lastra alta
4.5. Cinque sabotaggi, ognuno rosso sul proprio controllo.

**E la forma viaggia** (protocollo 27), che è quello che ai due passi sopra
mancava: la tabella dei blocchi sul filo era di soli NOMI, quindi un client senza
quel mod vedeva un cubo magenta dove il server ha una lastra, e la sua collisione
-- che è quella che risolve il movimento prima di mandare qualsiasi cosa --
rifiutava di salirci. Il server l'avrebbe accettato. Adesso il welcome porta la
forma accanto al nome, per i blocchi che ne hanno una e per quelli che ruotano:
nel gioco che spedisce sono **163 byte per 6 blocchi su un welcome di 1835**. La
forma del server vince anche su un nome che il client conosce -- due copie dello
stesso mod che non vanno d'accordo su una scatola sono un giocatore fermo dove il
mondo dice che può stare. Cinque sabotaggi, ognuno rosso sul proprio controllo.

**E un blocco può essere disegnato come MODELLO**, l'altra metà di quello che il
piano chiamava `drawtype nodebox+mesh`: `core.node_model` gli attacca un glTF -- la
stessa pipeline che vestono i corpi -- e il mesher fa una domanda sola, «questo
blocco è un cubo?», quindi la cella esce dalla mesh e viene disegnata come
modello, ruotata attorno al centro della cella dalla stessa rotazione intera
delle scatole. **Il modello è la figura e le scatole sono il blocco**: nessuno
chiede a una mesh dove può stare un corpo. `mods/steps` spedisce entrambe le cose
su `steps:marker`, con un modello scritto da `tools/make_marker_gltf.py` invece
che modellato: sei triangoli, buffer incorporato, **un file solo**.

Due difetti veri trovati qui, tutti e due su un modello **senza scheletro** --
cioè ogni modello di un blocco. Il loader lega un vertice non skinnato al joint 0
con peso 1 e nessuno caricava il joint 0: il vertex shader leggeva tre righe di
zeri e il modello collassava nell'origine, quindi **ogni glTF statico che questo
motore avesse mai ricevuto disegnava niente** — e finora gli erano stati dati
solo personaggi riggati. L'altro era nel modello stesso: facce avvolte al
contrario, quindi nascoste dal culling, con lo stesso sintomo del primo.

**Resta aperto e dichiarato:** un blocco-modello non è illuminato dalla luce
della sua cella (ha la luce direzionale dei corpi), non si anima e non viene
tagliato da un portale; viaggiano forma, rotazione e modello e non il resto (un
blocco non solido di un mod è ancora un muro per chi non ha quel mod); un oggetto
caduto si posa sulla cella e non sulla scatola; e `selection_box` non è separato
da `node_box`. TODO.md OPEN-43.

**Un mondo dice come si muove un corpo** (2026-09-13, il primo pezzo di M13):
`core.movement(walk, sprint, fly, jump, gravity)`, nel welcome dal protocollo 28,
e il validatore del server ricava i suoi limiti da lì. Il gate su un server vero:
a 60 m/s **0 claim rifiutati su 98** nel mondo che li ha dichiarati e **14 su 99**
in uno senza mod; a 150 m/s **21 su 98** anche nel primo. Due difetti trovati per
strada. **Volare in sprint era un cheat**: 28 m/s contro un tetto di 22.4, e un
client non modificato che teneva premuti due tasti si vedeva rifiutare **17 claim
su 160** in otto secondi, il primo dopo 3.80 s. E **un passo più lungo di 1.6
nodi attraversava i muri**, perché il risolutore guarda dove finisce un passo e
non cosa attraversa: adesso un corpo si muove a passi di mezzo nodo, e uno veloce
si ferma fino a mezzo nodo prima di un muro. Una caduta si ferma a 60 m/s anche
sul client, qualunque sia la gravità. **Resta aperto:** velocità per corpo (un
veicolo), un server che verifica la fine di un claim e non il percorso, e lo
streaming dei chunk a quelle velocità, che nessuno ha misurato. TODO.md OPEN-44.

**Lo streaming a velocità di corsa, misurato prima di cambiarlo** (TODO.md
OPEN-45): un passaggio su un mondo largo 512, con un mod che dichiara 256 m/s.
Fino a **120 m/s regge** — il terreno davanti non scende sotto i 121 nodi, e il
limite che si sente è il raggio, 128 nodi, cioè un secondo di strada. A **256 m/s
no**: 4 chunk per tick sono 80 al secondo contro 144 dovuti, e davanti restano 64
nodi al minimo, un quarto di secondo. Il chunk sotto il corpo non è mai mancato.
Non è stato cambiato niente: raggio che cresce con la velocità, centro spostato in
avanti o budget più grande sono tre scelte che aspettano una pista.

**Il server guarda anche cosa attraversa un claim** (TODO.md OPEN-46), non solo
dove finisce: a 256 m/s un claim è lungo 12.8 nodi, cioè quasi ogni muro. Non in
linea retta, che rifiuterebbe chi gira attorno a uno spigolo o sale uno scalino:
il server ripercorre il claim con lo stesso risolutore del client, in cinque modi
(tutto insieme, prima x, prima z, prima di lato e poi giù, prima su e poi di
lato), e lo accetta se uno arriva entro **mezzo nodo** — che è quanto un corpo si
ferma prima di un muro, e meno dei sei decimi che separano i due lati di
qualunque muro, anche sottilissimo. Un giocatore a piedi quasi mai viene
ripercorso: i suoi claim sono più corti di mezzo nodo. Il primo claim dopo che il
server ha piazzato un corpo (spawn, teleport, respawn) si giudica ancora solo da
dove finisce. La suite ha trovato tre test che teletrasportavano un corpo, o lo
facevano camminare attraverso una colonna di terra: sono stati corretti i test, non
la regola. Resta rifiutato, e detto, un percorso che torna indietro su un asse
fra due claim.

**Un corpo può muoversi diversamente dagli altri** (TODO.md OPEN-47):
`core.body_movement(player, ...)` dà a un solo corpo numeri suoi, in qualunque
momento, con un messaggio a quel corpo (protocollo 29). Il gate su server vero, in
un mondo con sprint 60: il corpo a cui il mod dà sprint 150 va a 200 m/s con **0
claim rifiutati su 59**; rimesso ai numeri del motore, nel secondo di margine
**0 su 12**, e un secondo dopo **43 su 59**. È il primo pezzo di un veicolo, non un
veicolo: niente sterzo né inerzia, e i numeri di un corpo non passano un portale.

**Un colpo verificato dal server, e le cose di un mod che vedono tutti** (TODO.md
OPEN-15, OPEN-20). Il server lancia lui il raggio dall'occhio al corpo che un colpo
nomina, e un blocco solido in mezzo è un rifiuto, detto con la distanza: nel gate,
un corpo dietro un muro, a portata, non perde niente, e tolto il muro sì.
`core.ecs.visible` fa mandare a ogni giocatore vicino un'entità di un mod, con un
tipo suo (`Thing`, protocollo 30): nel gate tre cartelli arrivano come cose a un
client senza mod, e nascosto uno ne restano due. **E un difetto vero trovato per
strada** (OPEN-48): gli id dei corpi sul filo erano a 32 bit, mentre in flecs la
generazione sta nei 32 alti. Appena un indice veniva riusato, un corpo non si
poteva più colpire: misurato uno stack con id 492 sul filo e id vivo 4294967788.
Adesso gli id sono interi (protocollo 31), e un colpo pesa 9 byte invece di 5.

**Un mondo può mandare codice alla macchina del giocatore, e il giocatore può
rifiutarlo** (TODO.md OPEN-49). Un mod dice dove gira con `side = server |
client | both`. Il server fa girare i suoi e impacchetta quelli per i client
come media (`worldcode.<mod>.cnpmod`); il client li carica nel mondo in cui è,
con una tabella di funzioni che non può scrivere il mondo né registrare niente,
senza nessuna capability e con i budget di default. `--world-code refuse`, o la
chiave del mondo in `world_code_refused.txt`, e non gira niente: il mondo si gioca
lo stesso. Nel gate, un client senza mod fa girare `hud` e rifiuta `greedy`, che
chiede `os`; il blocco che `hud` prova a scrivere non viene scritto, e il suo
messaggio arriva alla metà sul server e la risposta torna. Per strada: un nome di
media con una barra fa rifiutare al client tutta l'offerta di un mondo, texture
comprese, e il primo nome ce l'aveva.

**M10 è sospesa.** Tre quarti sono fatti — pacchetto mod con versioni, build che
viaggia, documentazione dell'API sotto gate — e restano Windows e i due formati
bundle. Aspettano: non si impacchetta niente per altre macchine finché il gioco
non è dichiarato finito e giocabile.

**E la compatibilità con l'engine di partenza non si fa** (2026-09-12). Era M11, dieci
settimane per far girare i giochi e i mod dell'engine di partenza qui sopra, ed era la
mitigazione scritta contro il rischio n.1 del progetto: i contenuti. L'engine di partenza è
stato l'ispirazione e il fork precedente è da dove viene l'esperienza, non
l'obiettivo. Quello che M11 chiedeva all'**engine** era già stato fatto e vale da
solo — `param2` come trait, scheduler ABM/LBM, drawtype nodebox+mesh — e cade
tutto il resto: parser formspec, `[combine`/`[colorize`, VoxelManip, alias di
mapgen e il secondo backend LuaJIT. **Quello che resta scoperto va detto:** i
contenuti adesso si scrivono qui.

**Lo scope è fissato** (2026-09-12): un clone del classico gioco a blocchi da cui i portali
portano ad altri mondi con mod propri, fino a mondi che sono un altro gioco — una
corsa acrobatica, un FPS in stile arena shooter. Non è più piccolo dello scope che §7.5
chiamava fatale senza i contenuti di qualcun altro; lo tiene in piedi il fatto che
ogni modo è un mondo piccolo dietro un portale, e non un pezzo del sandbox. Quello
che manca all'engine per reggerlo è in M13, letto dal codice: il comportamento di
un mondo non arriva al client, il validatore rifiuta più di 22.4 m/s in
orizzontale, un colpo è controllato solo per distanza, e le entità di un mod non
le vede nessuno. **Deciso anche il codice** (2026-09-13): un mondo può mandare al
client codice Luau, in una sandbox senza file né rete, e il client può rifiutarlo —
il server resta l'autorità, quindi rifiutarlo non dà vantaggi e toglie solo quello
che il mondo voleva mostrare. `docs/DESIGN.md` §0, §5.6, §7.5, M11-M13 e la
tabella dei rischi.

---

**I test sono stati ridotti a quelli fondamentali** (2026-09-13). Lo sviluppo era diventato più
test che feature: 81 test in `ctest`, ~33.5k righe in `tests/` e ~9.6k righe di modalità
`--*-check` dentro il client. Ne restano 13, in ~3 s: parser del protocollo e dei chunk contro
input ostile, cifratura, mondo salvato e migrazione, aritmetica del portale e collisione, isolamento
dei mod, determinismo del mapgen, join fra due processi reali, persistenza del server, e la
continuità del portale in GPU. Il resto si verifica giocando. L'elenco con il perché di ciascuno è
in [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt). Le menzioni di gate rimossi più sopra e in
`TODO.md` sono storia.

Risultati e insidie trovate: [`docs/M0_FINDINGS.md`](M0_FINDINGS.md) ·
[`docs/M1_FINDINGS.md`](M1_FINDINGS.md) · [`docs/M2_FINDINGS.md`](M2_FINDINGS.md) ·
[`docs/M3_FINDINGS.md`](M3_FINDINGS.md) · [`docs/M4_FINDINGS.md`](M4_FINDINGS.md) ·
[`docs/M5_FINDINGS.md`](M5_FINDINGS.md) · [`docs/M6_FINDINGS.md`](M6_FINDINGS.md) ·
[`docs/M7_FINDINGS.md`](M7_FINDINGS.md) · [`docs/M8_FINDINGS.md`](M8_FINDINGS.md) ·
[`docs/M9_FINDINGS.md`](M9_FINDINGS.md) · [`docs/MODDING.md`](../MODDING.md) ·
[`docs/TEXTURE_PACKS.md`](../TEXTURE_PACKS.md).

```sh
cmake -S . -B build -G Ninja && cmake --build build -j
ctest --test-dir build                  # 13 test fondamentali, ~3 s; -LE gpu per la sola metà headless
./build/src/client/cnp_client           # WASD, F vola, LMB rompe, RMB piazza (a mani vuote: usa)
                                        # F3 hud, F6 profiler (dove è andato il frame), F7 pacchetto dopo
                                        # rotellina o 1-9 slot, E borsa, Q butta, ESC libera il mouse
./build/src/client/cnp_client --dual    # due mondi e un portale fra loro, a due ore diverse
./build/src/client/cnp_client --day-length 20 --time 0.2   # tutto il ciclo giorno/notte in 20 s
./build/src/client/cnp_client --dual-bench --world 512   # quanto costa il secondo mondo
./build/src/client/cnp_client --portal-scale 0.5         # target del portale a metà risoluzione
./build/src/client/cnp_client --oriented --log-view      # traccia della vista attorno a ogni attraversamento
./build/src/client/cnp_client --bench   # screenshot dei criteri + misura FPS e scripting
./build/src/client/cnp_client --textures assets/textures --textures assets/packs/kenney
                                        # due pacchetti, F7 passa da uno all'altro (docs/TEXTURE_PACKS.md)
./build/src/client/cnp_client --model personaggio.gltf   # un personaggio in piedi davanti allo spawn
./build/src/client/cnp_client --models assets/models      # dove cercare i modelli che un mondo nomina
                                        # un nome «mymod:crab» diventa mymod_crab.gltf lì dentro e da nessun'altra parte
./build/tools/cnp_modpack pack mods/ore    # un mod in un file: ore-0.1.0.cnpmod
                                        # messo in mods/ si carica come una directory
cmake -S . -B build-dist -DCNP_PORTABLE=ON && cmake --build build-dist -j
./tools/dist/package-linux.sh build-dist  # un tar che gira dove non è stato compilato
./build/src/server/cnp_server --listen 25000 --world-file mondo.db

# e in due terminali: il mondo arriva dal server, non viene generato
./build/src/server/cnp_server --listen 25301 --world 128
./build/src/client/cnp_client --connect 127.0.0.1 --port 25301 --name giacomo
```

## Scelte tecniche

| | |
|---|---|
| Piattaforma grafica | raylib 6.0, dietro un'interfaccia `gfx_*` di ~35 funzioni (migrabile a `sokol_gfx`) |
| Linguaggi | C++20 per l'engine, **Luau 0.733** per lo scripting, **un runtime solo** (il secondo backend LuaJIT è caduto con la compatibilità dell'engine di partenza) |
| Rete | ENet + zstd + **Monocypher** (X25519, XChaCha20-Poly1305), 2-8 giocatori per mondo |
| Storage | SQLite |
| Obiettivo | Sandbox in stile gioco a blocchi classico da cui i portali cross-server portano ad altri mondi con mod propri, fino a modi di gioco diversi (corsa acrobatica, FPS in stile arena shooter) |

## Documentazione

- [`docs/DESIGN.md`](../DESIGN.md) — analisi di fattibilità completa, pregi e difetti, piano a
  milestone da M-1 a M13, invarianti da non rompere.
