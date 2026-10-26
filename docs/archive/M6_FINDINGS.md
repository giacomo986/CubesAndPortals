# M6 — Cross-server: cosa è stato costruito e cosa si è imparato

Milestone M6 di `DESIGN.md` §8: *chiavi d'identità, ticket firmati, scope
OBSERVE/ENTER, heartbeat di stato, fallback a redirect, media
content-addressed con cache condivisa, id di contenuto per-sessione.*
**Test:** *due server su macchine diverse, con game diversi; attraversamento
senza schermata di carico; una passiva verso un server morto non spamma il log
(regola del backoff).*

**In corso.** Sotto c'è solo ciò che è stato costruito e verificato da
esecuzioni.

---

## 1. Il grace token: un teletrasporto che non sembra un cheat

Primo pezzo, ed è quello che `TODO.md` teneva aperto da M4 come OPEN-1.

### 1.1 Il problema, in numeri

Il validatore di movimento del server rifiuta chi si muove troppo. Un portale lo
rompe **due volte**:

- **il salto**: un attraversamento sposta il giocatore di venti nodi in un tick.
  L'indennità è `max_horizontal × tolerance × dt + 0,25`, cioè
  `14 × 1,6 × 0,05 + 0,25 = 1,37` nodi. Venti nodi sono quattrocento volte
  troppo.
- **quello che si porta dietro**: una caduta dentro un portale a pavimento e
  fuori da uno a muro trasforma velocità verticale in orizzontale. La velocità
  terminale è **60 nodi al secondo** e il tetto orizzontale è `14 × 1,6 = 22,4`.
  Misurato in `docs/M4_FINDINGS.md` 20.1: 11,67 m/s di caduta escono come 11,67
  m/s in avanti.

Alzare `max_horizontal` sarebbe la correzione sbagliata: è il numero che ferma
un cheat di velocità, e un portale non è una ragione per smettere di controllare.

### 1.2 Cosa dice il progetto, e chi rilascia il token

`DESIGN.md` §3.4: *la teletrasportazione è decisa dal server, il client la
disegna*, e il pacchetto di swap porta un **grace token** che sopprime il
controllo di velocità per N tick sul server di destinazione.

La parte che conta è **chi lo rilascia**. Un token che un client può chiedere è
un token che ogni cheat chiede. Qui non c'è nessun messaggio che ne produca uno:
la finestra si apre solo dentro `ServerSessions::teleport`, cioè come
conseguenza del fatto che è stato **il server** a decidere di spostare qualcuno.

E sopprime **solo** i due controlli di velocità. Una posizione dentro un blocco
è rifiutata a finestra spalancata: un teletrasporto che deve rompere la regola
del mondo non è un teletrasporto che questo server esegue.

### 1.3 Quanto dura, e perché

`MovementLimits::portal_grace = 1.0` secondi — secondi e non tick perché tutto
il resto del validatore è misurato su un orologio.

Un secondo perché la velocità portata **non viene smorzata da niente**: in aria
il client può solo *aggiungere* alla velocità orizzontale, e l'assegnazione che
la sostituisce avviene all'atterraggio. Quindi l'eccesso dura finché il
giocatore non tocca terra, che da un'uscita normale è una frazione di secondo.

**Limite dichiarato:** chi è ancora in aria un secondo dopo l'attraversamento
viene corretto come chiunque altro.

### 1.4 Misurato, con la finestra aperta e chiusa

Un `check_move` senza rete (`chunk_codec`, sezione movimento), ogni caso nelle
due posizioni della finestra:

| claim | senza grace | con grace |
|---|---|---|
| venti nodi in un tick | rifiutato | **accettato** |
| 60 nodi/s sull'asse orizzontale | rifiutato | **accettato** |
| lo stesso, un tick dopo la scadenza | rifiutato | — |
| posizione dentro un blocco | rifiutato | **rifiutato** |

E sul filo, con un server e un client veri (`net_loopback`): il server sposta il
giocatore di venti nodi con `teleport`, il client viene avvisato, e poi il
client rivendica mezzo secondo di velocità portata (30 nodi).

```
con la grace          avvisato dello spostamento sì, la velocità portata accettata
con la grace tolta    avvisato dello spostamento sì, la velocità portata RIFIUTATA
```

La seconda riga è il sabotaggio: una grace mai verificata da chiusa è un tetto
di velocità cancellato.

**Due trappole trovate scrivendo il test, tutte e due dette perché sarebbero
tornate:** i claim portano un numero di sequenza e il server scarta quelli non
più nuovi, quindi la seconda corsa doveva continuare a contare invece di
ripartire da 1 — altrimenti ogni claim veniva buttato come vecchio, e «niente è
stato rifiutato» si legge identico a «la grace non serviva». E su un socket vero
l'intervallo fra due claim è quanto ha impiegato il ciclo di pompaggio, mentre
l'indennità è al secondo: un tick di distanza viene accettato o rifiutato a
seconda di quanto ha dormito il loop. Mezzo secondo di velocità portata — 30
nodi — è oltre l'indennità a **qualunque** intervallo, perché `max_interval` la
tappa a 22,65.

### 1.5 Cosa NON è stato fatto, detto

Il meccanismo c'è e il chiamante no. **Il server non ha ancora portali**: nulla,
oggi, chiama `teleport` per conto di un attraversamento. Il client tiene i
portali spenti quando è connesso (`Scene::portals` è falso in rete), quindi in
gioco vero questo percorso non viene ancora esercitato — solo dai gate.

Il pezzo successivo è quello: le aperture di lato server, che decidono
l'attraversamento e chiamano questo.

---

## 2. Le aperture di lato server: l'attraversamento lo decide il server

Il pezzo che mancava in 1.5: il meccanismo c'era e il chiamante no. Adesso c'è.

### 2.1 Un messaggio nuovo, e perché non è una correzione

`Teleport` (tipo 8, canale evento, affidabile) porta **posizione, direzione
dello sguardo, velocità e roll in un solo messaggio**.

Non è una `Correct` con più campi, ed è una distinzione con conseguenze. Una
correzione dice *«eri nel posto sbagliato»* e lascia stare la mira del
giocatore; questo dice *«ti ho spostato io»* e la sostituisce. `PORTAL_SYSTEM`
4.1 è la ragione per cui viaggiano insieme: mandarli separati lascia un
fotogramma nel posto nuovo con l'orientamento vecchio, cioè un giro completo del
mondo disegnato una volta e poi corretto.

Il roll c'è perché attraversare fra aperture non parallele inclina l'orizzonte,
e il client non conosce l'apertura di destinazione: non può ricalcolarlo.

**Validato in ricezione, non creduto.** Oltre ai NaN e ai numeri che nessun mondo
è grande abbastanza da contenere, uno sguardo che non è una direzione viene
rifiutato — finisce dritto nella base della telecamera, e un vettore nullo la fa
collassare. `protocol` lo verifica in tutte e due le direzioni: andata e ritorno
con tutti i campi, e un `look` a zero rifiutato.

### 2.2 Dove vive lo stato

- `ServerSessions::set_portals(a, b)` — la coppia, opzionale e silenziosa se
  assente: un server che non la chiama non ha portali, e chi cammina dove ce ne
  sarebbe uno cammina nell'aria.
- `Session::portal_a_state` / `portal_b_state` — lo stato del grilletto **per
  giocatore**. Un grilletto è una cosa che succede a qualcuno: due giocatori che
  entrano nella stessa apertura sono due attraversamenti, e uno stato condiviso
  lascerebbe che l'arrivo del primo annulli l'avvicinamento del secondo.
- `Session::crossings` — quanti gliene sono stati dati, per il log e per un gate
  che li conti.

Il `Portal` è **lo stesso tipo che usa la fisica del client**: `core/portal` non
ha mai avuto bisogno di un renderer.

### 2.3 La velocità che il server non ha

`portal_step_trigger` vuole una velocità, e il messaggio `Move` porta una
posizione e uno sguardo e basta. Aggiungerne una vorrebbe dire **chiedere al
client un numero che poi il server si fida** — lo stesso errore del token di
grazia che un client può richiedere.

Quindi è **derivata da due claim consecutivi**, e il costo è dichiarato: è in
ritardo di un claim, e `portal_step_trigger` la usa solo per il caso di chi è
già oltre il piano di un'apertura **orizzontale** al primo fotogramma in cui è
nei limiti. Chi cade veloce può avere bisogno di un claim in più rispetto alla
fisica del client.

### 2.4 Misurato, con e senza

`net_loopback`, un server e un client veri su un socket: il client cammina verso
l'apertura a un quinto di nodo per claim.

```
un server senza aperture:  nessun teletrasporto
con le aperture:           crossed to 61.10 20.00 60.50, looking 1.00 0.00 0.00,
                           velocity 3.30 0.00 0.00, roll 0.00
```

Cioè: entrato a (40,5 · 44,5) guardando −Z, uscito davanti all'altra apertura
guardando **+X**, con la velocità di camminata trasformata insieme a lui. La
prima riga è l'altra metà della misura: la stessa camminata su un server senza
aperture non teletrasporta nessuno, quindi il teletrasporto è le aperture e non
il camminare.

Verificato anche che **la copia del server è d'accordo con quello che ha
mandato** — `authority.x` uguale al campo del messaggio — perché un server che
dice al client una cosa e ne crede un'altra è la forma peggiore di questo bug.

Trovato scrivendo il gate: `peek_type` è una **lista bianca**, e un tipo nuovo
non riconosciuto lì non arriva a nessun parser. Il server mandava, il client non
ascoltava, e il conteggio degli attraversamenti lato server era già a 1: il
sintomo era «il server dice di averlo fatto e il client non se n'è accorto».

### 2.5 Il client lo applica

Nel ciclo di rete, accanto alla correzione: posizione, velocità **assoluta**
(non un delta: il delta lo ha già sommato il server), yaw e pitch ricavati dallo
sguardo, e il roll che l'animazione riporta a livello.

Nessun warp della telecamera, e detto sul posto: il server manda dove il
giocatore **è**, e da questo lato non c'è ancora un'immagine con cui l'occhio
fosse continuo. Quando arriverà la sessione passiva, è lì che andrà il warp.

### 2.6 Cosa manca, detto

`cnp_server --portals` mette la coppia nel plaza e lo dice all'avvio, ma **non
c'è niente da vedere**: il server non ha renderer e il client tiene i propri
portali spenti quando è connesso. Un giocatore che ci cammina dentro si ritrova
dall'altra parte senza che niente glielo abbia mostrato prima.

Il pezzo successivo è quello: la sessione passiva verso il server di
destinazione, cioè l'immagine dentro l'apertura.

---

## 3. Le aperture le mettono i giocatori

§2 ha spostato la decisione dell'attraversamento sul server, ma le aperture
erano ancora quelle di un flag di avvio. Le due cose non sono separabili: la
grazia che rende legale un salto di venti nodi la concede il server **per le
aperture che conosce**, e un'apertura che il server non conosce è un giocatore
che si teletrasporta da solo. Quindi o le aperture le piazza il server, o non
si piazzano.

### 3.1 Il messaggio non contiene l'apertura

`PlacePortal` (tipo 9, canale evento, affidabile) porta **un blocco e una
faccia**. Non porta dove va l'apertura, quanto è grande o come è girata: quelle
tre cose le ricava il server dal blocco e dalla faccia.

Non è ordine: è l'unico motivo per cui un client modificato non può appendere un
portale a mezz'aria a cento blocchi di distanza. Non esiste un campo in cui
scrivere la bugia. L'unica cosa su cui può mentire è **quale blocco ha
cliccato**, e quella viene controllata contro il mondo del server e contro
quanto lontano arriva un braccio.

Il rifiuto è a livello di decoder per quello che è una forma impossibile: una
faccia che nessun cubo ha (somma delle componenti diversa da 1), una terza
apertura (`which > 1`, che sarebbe una scrittura oltre la fine di un array di
due), un indice di nodo così grande che `x + nx` sarebbe undefined behaviour
prima ancora del controllo dei limiti.

### 3.2 La forma segue il giocatore

Un muro riceve un buco **1 nodo largo e 2 alto**, appoggiato alla cella davanti
alla faccia, perché quella è la misura di un giocatore. Un pavimento o un
soffitto ricevono **1 × 1**: quel buco lo si guarda da sopra o da sotto, e un
buco profondo due nodi in un pavimento è un pozzo, non un'apertura.

Le celle davanti alla faccia devono essere libere — tutte, una o due che siano.
È la stessa domanda che il controllo del movimento fa su una posizione
dichiarata, fatta prima che l'apertura esista invece che dopo.

### 3.3 Cosa viene rifiutato, misurato in tutte e due le posizioni

`net_loopback`, su un socket vero, con due muri costruiti apposta:

```
un blocco a quaranta nodi:   nessuna apertura        (fuori portata)
una faccia sull'aria:        nessuna apertura        (non c'è niente su cui appoggiarla)
il muro a tre nodi:          39.50 21.00 52.50
l'altro muro:                45.50 21.00 52.50
```

`place_reach` è 7 nodi contro i 6 di portata del client. Il nodo in più è la
distanza fra la posizione che il server ha accettato per ultima e quella in cui
il giocatore era quando ha cliccato: un tick di corsa è quasi un nodo, e
rifiutare quello vorrebbe dire rifiutare piazzamenti onesti a scatto pieno.

**Il primo caso "fuori portata" passava con il controllo della distanza
cancellato.** Mirava a un blocco d'aria a quaranta nodi, quindi a essere
misurato era il controllo del solido. Il caso adesso costruisce un blocco pieno
con aria sopra, lontano: tutto di lui è piazzabile tranne la distanza.
Sabotaggi, uno per controllo: distanza tolta, il piazzamento lontano riesce;
solido tolto, il portale si appende all'aria; memorizzazione tolta, i tre
controlli sulla posizione e i due sull'attraversamento diventano rossi.

### 3.4 Attraversare un'apertura che nessun file di configurazione ha mai visto

```
crossed to 46.10 20.00 52.50, looking 1.00 0.00 0.00
```

Entrato nell'apertura piazzata a 39,5, uscito da quella piazzata a 45,5, sul
lato davanti. E poi la riga per cui esiste tutta la sezione: la **richiesta
ancora in volo**, quella partita prima che l'attraversamento fosse deciso e
arrivata dopo, dice che il giocatore è ancora *davanti* all'apertura. Contro la
nuova autorità è un salto di 6,7 nodi in 0,064 s, contro un'indennità di circa
1,7. Con `portal_grace = 0` prima della camminata: **una correzione**. Con la
grazia: nessuna.

Cioè: la grazia arriva anche a un portale piazzato da un giocatore, che è
esattamente la frase da cui è partito tutto.

Sabotaggio che è servito a qualcosa: la prima versione azzerava la grazia
*dopo* l'attraversamento e restava verde. La grazia viene scritta in
`authority.grace_until` nel momento in cui il server sposta il giocatore, quindi
per toglierla bisogna toglierla prima della camminata. Un gate sabotato nel
punto sbagliato dice solo che il sabotaggio non ha fatto niente.

### 3.5 Quello che il piazzamento ha fatto emergere

Un'apertura orizzontale piazzata sulla faccia superiore del blocco **su cui sei
in piedi** produce, in sessione vera: **97 correzioni in cinque secondi**, tutte
`that position is inside a solid block`, giocatore fermo a y 19,96 su un blocco
che finisce a y 20.

Due regole, ognuna giusta per conto suo. Il client alza il fondo della collision
box perché altrimenti un portale a pavimento non ci si cade dentro
(PORTAL_SYSTEM 2.3). Il server rifiuta una posizione dentro un blocco pieno, e
la rifiuta anche sotto grazia, di proposito. Il giocatore affonda apposta e
viene rifiutato apposta.

Non è una regressione del floor shift: è la prima volta che un giocatore può
mettersi un portale a pavimento sotto i piedi. Aperto come OPEN-5, con la
riparazione stretta scritta lì.

Il gate del client di rete evita la cella sotto i piedi e piazza su un blocco
vicino, **e lo dice nel commento**: non è per far passare il gate, è perché il
caso sotto i piedi ha un difetto suo che sta scritto altrove.

### 3.6 L'immagine, e chi la disegna

`Portals` (tipo 10, canale evento, affidabile) porta le aperture come stanno
adesso, con una **maschera di bit** e non un conteggio: chi ha piazzato un capo
e sta camminando verso l'altro muro ha esattamente un'apertura, e *quale* delle
due decide su che muro viene disegnata. Una seconda apertura da sola, mandata
come lista di uno, arriverebbe come prima.

Il client di rete adesso i portali li disegna. Fino a ieri li teneva spenti
perché non c'era niente da mostrare; adesso c'è, ed è il mondo che sta già
ricevendo in streaming — l'immagine dentro l'apertura è quella stessa sessione,
non una seconda connessione. I due render target si creano anche in rete e
restano vuoti finché il server non dice dove sono le aperture.

Mezza coppia si disegna come niente. Un'apertura senza l'altra non ha
un'immagine da mostrare, e riempirla con la vista da una destinazione che
nessuno ha piazzato sarebbe la fotografia di un posto vero: un giocatore la
leggerebbe come un portale funzionante e ci camminerebbe dentro contro un muro.

Un'apertura vive in **tre posti** in questo client — due volte nel renderer,
come sorgente di un link e destinazione dell'altro, e una volta come `Portal` che
un corpo attraversa. `apply_server_portals` le scrive tutte e tre, e il gate del
client le rilegge tutte e tre:

```
placed an opening on top of 66 19 64: the server put it at 66.50 20.50 64.50,
version 1 -> 2
```

Con le copie del renderer non scritte: rosso su «la stessa apertura disegnata e
attraversata». Questa metà non la può vedere il probe di loopback, che non ha un
renderer con cui non essere d'accordo.

### 3.7 Limite dichiarato

Il legame fra il click del mouse e `send_place_portal` non è misurato da niente:
il gate del client di rete gira senza input e il piazzamento lo fa da codice.
Tutto quello che c'è a valle del click sì, dal messaggio al pixel. Sono tre
righe, e sono tre righe non coperte.

---

## 4. Il portale a pavimento che ti seppelliva (OPEN-5, chiuso)

§3.5 lo aveva aperto con un numero: un'apertura orizzontale piazzata sul blocco
**su cui sei in piedi** dava 97 correzioni in cinque secondi, tutte `that
position is inside a solid block`, e nessun attraversamento. Erano tre difetti
uno dentro l'altro, e nessuno dei tre si vedeva finché non si toglieva quello
sopra.

### 4.1 L'apertura era nella cella sbagliata

La prima versione metteva ogni apertura **nella cella davanti alla faccia**.
Per un muro è giusto: niente deve reggerla, e metterla dentro il nodo del muro
sarebbe un buco dentro un blocco pieno, dove il giocatore non può stare.

Per un pavimento è **il contrario**, ed è PORTAL_SYSTEM 2.3 e non una svista:
qualcosa deve reggere un portale a pavimento, e quel qualcosa è terra solida
esattamente dove il giocatore deve passare. Messa nella cella sopra, la
superficie dell'apertura finisce un nodo **sopra la testa**: il giocatore è
mezzo nodo *dietro* il piano di un portale che si è messo ai piedi, il trigger
non lo vede mai arrivare dal davanti, e sprofonda senza attraversare.

Con l'apertura nel nodo cliccato: piedi a 20,0, centro a 19,5, `depth` 0,5
contro un `trigger_depth` di 0,4 — arriva dal davanti. Sprofonda di un decimo e
passa.

### 4.2 Il nodo che regge il portale non è più un muro, ma solo lui

`SolidExemption`: una **scatola**, non un flag, perché la regola che si rilassa
è geometrica. Dentro *quel* nodo, sotto *quella* apertura, il solido è
attraversabile. Altrove niente cambia, e un teletrasporto sotto grazia dentro un
muro resta rifiutato — è la differenza fra un'esenzione e una licenza.

Un nodo è esente solo se ci sta **tutto dentro**. Mezzo nodo esente non è mezzo
nodo di pietra attraversabile.

Misurato senza socket (`chunk_codec`, sezione movimento) e con socket
(`net_loopback`):

```
sprofondare nel terreno senza portale:        rifiutato
lo stesso nodo, con il portale che regge:     accettato
un nodo più sotto:                            rifiutato
mezza scatola sul nodo accanto al buco:       rifiutato
sank a twelfth of a node into the block holding it up: 0 corrections
```

Con l'esenzione tolta: 1 correzione, gate rosso, in tutte e due le sedi.

**Il caso "mezza scatola sul nodo accanto" viene dal gioco.** Lo spawn è su un
confine di nodi, quindi la scatola larga 0,6 sta a cavallo di due nodi e metà
del giocatore è su terreno pieno accanto a un buco largo un nodo. Non ci può
cadere dentro, e non deve: 106 correzioni, tutte per il **nodo accanto**
all'apertura. L'esenzione allargata a coprirlo aprirebbe un tunnel largo un nodo
di fianco a ogni portale a pavimento. Nel gate il giocatore si mette al centro
del nodo prima di piazzare, e il commento dice perché.

Limite dichiarato: `portal_floor_shift` decide se ingaggiare guardando il
**punto** dei piedi, non la scatola. Quindi il client alza la collision box
anche quando metà del giocatore è su terreno pieno. È il motivo per cui la
misura sopra esiste; la riparazione sta nel client e non è in questo commit.

### 4.3 Il seme dopo un attraversamento non è un seme dopo un piazzamento

`portal_seed_arrival` esiste per non rimbalzare indietro subito dopo essere
usciti. Applicato a un **piazzamento** dice un'altra cosa: «questa apertura l'hai
già usata». Il giocatore che si mette un portale ai piedi è in un'apertura già
usata, il floor shift lo fa sprofondare, il trigger è zitto, e affonda finché la
verifica del movimento lo rifiuta. Non attraversa mai.

Adesso un'apertura che cambia **azzera** lo stato del trigger e basta: dalla
richiesta successiva decide la regola normale. Il costo è dichiarato:
un'apertura piazzata attorno a un giocatore che sta davanti al piano se lo
prende, senza che abbia camminato. È quello che fa un portale piazzato addosso a
qualcuno.

### 4.4 Fine a fine, dal client vero

```
placed one under the player's feet: 1 crossings, 0 corrections while sinking
into it, and 97 more afterwards; ended at 18.96
```

Prima: 0 attraversamenti, 97-106 correzioni. Adesso: attraversa, e mentre
sprofonda nel nodo che regge il portale **zero** correzioni.

Le 97 dopo sono un'altra cosa e vanno dette: con **tutte e due** le aperture
piatte e rivolte in su, l'uscita è *sotto* la seconda — la mappa mette l'arrivo
dietro il piano di destinazione, e dietro un pavimento è dentro la terra. Il
giocatore esce sotto, sprofonda fuori dal nodo esente e viene corretto. Un
pavimento accoppiato a un muro, che è la forma per cui questo serve, esce in
piedi. Il gate lo stampa invece di nasconderlo.

### 4.5 E una cosa che il gioco ha detto per sbaglio

Le aperture sono stato del mondo e **restano lì**. Un client che si è collegato
dopo il run precedente è nato sopra il portale a pavimento lasciato dal run
prima, sul confine di nodi, e ha preso correzioni dal primo istante. Non c'è
proprietà, non c'è scadenza, non c'è niente che tolga un portale piazzato sotto
lo spawn. Non è aperto come difetto perché è una decisione di gioco che non è
stata ancora presa, ma è bene che sia scritto prima di scoprirlo.

---

## 5. Chi è il giocatore (DESIGN.md 4.4, primo punto)

Fino a qui un giocatore era una **stringa**. Chiunque poteva scrivere il nome di
chiunque, e su un server dove i portali portano a casa d'altri quello non è un
dettaglio: è l'assenza di identità.

### 5.1 Una chiave, e il nome resta un'etichetta

Coppia di chiavi per firme, generata al primo avvio del client e tenuta in
`player.key` (seme in hex, 0600, avviso se il file è leggibile da altri — la
stessa disciplina della chiave del server). L'identità è la **chiave pubblica**;
il nome è quello che uno scrive oggi.

Non è la stessa coppia della cifratura, e non lo è di proposito: X25519 fa
scambio di chiavi, EdDSA fa firme, e usare una chiave sola per tutte e due è il
modo classico di trasformare due primitive sane in una insana.

Detto per correttezza: è EdDSA su Curve25519 **con BLAKE2b**, che è quello che
offre il core di monocypher — non ed25519 RFC 8032. Qui dentro non interopera
con nessun altro, quindi la scelta è fra due opzioni internamente coerenti;
scrivere «ed25519» sarebbe una promessa di interoperabilità falsa.

### 5.2 Cosa si firma: la sessione, non il nome

La firma è su **channel binding ‖ etichetta ‖ nome**. Il binding esce dallo
stesso master della cifratura, con un'etichetta sua, e lo conoscono solo i due
capi di *quella* sessione.

Firmare il nome e basta avrebbe prodotto una **password**: valida per sempre, su
ogni server, per chiunque l'abbia vista passare — incluso il server a cui è
stata mandata. Il binding la rende inutile ovunque tranne lì e allora.

Misurato senza socket (`protocol`) e con socket (`net_loopback`):

```
alice si presenta e prova la chiave:        entra, 'alice' is b678:63d4:fbdc:40a8
mallory indossa la chiave di alice:         rifiutato, BadIdentity
la prova VERA di alice, un'altra sessione:  rifiutato
un server che non richiede identità:        anonimo entra
lo stesso con --require-identity:           anonimo rifiutato, alice entra
```

La terza riga è quella che conta. È una firma autentica, fatta da alice, con la
sua chiave, su una sfida vera — e non vale niente, perché era per una sessione
finita. Sabotaggio: tolto il binding dalla sfida, quella riga diventa rossa.
Tolta la verifica, ne diventano rosse tre.

E senza socket: la stessa prova non risponde a un'altra sessione, non risponde a
un altro nome, e una chiave con cui nessuno ha firmato non verifica.

### 5.3 Tre stati, non due

Un hello **senza** identità è anonimo, e se sia ammesso lo decide il server
(`--require-identity`). Un hello **con** identità viene verificato sempre,
politica o no: dichiarare una chiave che non si possiede è una bugia anche su un
server a cui non importa chi sei, e accettarla vorrebbe dire lasciare che
chiunque indossi la chiave pubblica di chiunque.

Un hello con **mezza** identità — chiave senza prova, o prova senza chiave —
viene rifiutato dal decoder. Una chiave senza prova è un'affermazione gratis;
una prova senza chiave sono 64 byte di niente.

### 5.4 Nessun binding, nessuna prova

Senza cifratura non c'è binding, quindi non c'è niente di unico da firmare:
qualunque prova sarebbe ripetibile da chi l'ha sentita. Il server rifiuta quel
caso e il client non ci prova nemmeno — mandare la chiave con una prova nulla
sarebbe peggio che entrare anonimi, perché metterebbe la chiave pubblica del
giocatore su un filo dove chiunque la raccoglie.

`--require-identity` senza cifratura **non parte**, e lo dice all'avvio: un
requisito che silenziosamente non si applica è peggio di nessun requisito.

### 5.5 Trovato scrivendo il gate

Il tetto dei motivi di rifiuto era scritto a mano:

```c
out.reason = (code <= (uint8_t)RefuseReason::ContentMismatch) ? ... : None;
```

Un motivo nuovo arrivava al client come `None`. Il server rifiutava
correttamente, il client diceva «nessun motivo». Un elenco che va aggiornato a
mano ogni volta che si aggiunge un valore, e nessuno lo aveva aggiornato.

### 5.6 Dove è finito il codice, e perché

`crypto.cpp` è passato da `cnp_net` a `cnp_core`. Non possiede socket — lo
scambio di chiavi sono byte dentro e byte fuori — e la regola «`cnp_core` compila
senza GPU e senza stack di rete» resta vera. Quello che è passato con lui è il
motivo: la macchina a stati della sessione adesso deve **verificare una firma**,
e una decisione di handshake che vive sopra il livello per cui decide è una
decisione che nessun test senza socket può raggiungere.

### 5.7 Limiti dichiarati

`player.key` sta nella directory corrente, come `server.key`. Per un gioco vero
va in una directory di configurazione per utente; per adesso è dove uno lancia il
client, e `--player-key` lo sposta.

L'identità **non fa ancora niente**: il server sa chi sei e lo scrive nel log,
ma non ci sono permessi, non c'è una lista di chi può entrare, e due sessioni
con la stessa chiave sono due giocatori. I ticket firmati e gli scope
OBSERVE/ENTER di §4.4 sono il pezzo dopo, e sono il motivo per cui questo viene
prima.

---

## 6. Una sessione che guarda soltanto (scope OBSERVE)

DESIGN.md 4.4 dice due cose sulla sessione passiva dietro un portale: **non è un
giocatore**, e oggi nel fork entra come utente fantasma `<nome>_pv` con password
vuota, che «non è accettabile in un gioco vero». Questo è il tipo di sessione,
prima che ci sia qualcosa che la apre.

### 6.1 Due scope, e cosa cambia davvero

`Scope::Enter` è una persona che cammina. `Scope::Observe` è un portale che
guarda dentro: prende i chunk, vede la gente che c'è, e **non ha corpo**.
Tenerli separati non è educazione — un osservatore con un avatar è un giocatore
in piedi in un mondo in cui non è mai entrato, e un osservatore che può scavare
è un modo di modificare un mondo senza esserci.

Concretamente, per una sessione Observe:

- non compare nella lista delle entità di nessuno;
- non può piazzare niente;
- non ha autorità di movimento: la sua richiesta di posizione è **una richiesta
  di chunk** e basta, perché un cap di velocità su una sessione senza avatar è
  una regola su una cosa che non esiste;
- non occupa un posto giocatore.

### 6.2 Due capienze, non una

`max_players` e `max_observers` sono numeri diversi perché non sono la stessa
risorsa: un mondo pieno di giocatori deve restare **guardabile** attraverso un
portale, e un mondo pieno di spettatori deve restare **giocabile**.

E un mondo può rifiutare di farsi guardare (`--no-observers`), con un motivo
suo (`NotWatchable`) invece di un generico «pieno».

### 6.3 Misurato su socket, con tre client

```
il giocatore riceve 1 corpo,  la finestra 2
2 giocatori e 1 spettatore ammessi
```

Tre client di proposito. Il giocatore riceve **l'altro giocatore** — che è la
prova che il meccanismo funziona — e **non** la finestra. La finestra riceve
tutti e due, che è il motivo per cui vale la pena guardarci attraverso.

**La prima versione aveva due giocatori in meno e passava per il motivo
sbagliato**: la lista era vuota, e «lo spettatore non è nella lista» è vero
anche di una lista vuota perché non funziona niente. Con il sabotaggio (tolto lo
skip degli osservatori) adesso il giocatore ne riceve 2 e il gate diventa rosso.

### 6.4 Rifiutato *per* la regola giusta

Il piazzamento da una sessione Observe è rifiutato due volte: dalla regola di
scope, e dal fatto che un osservatore non ha una posizione autorevole. Un
controllo che guarda solo «le aperture non sono cambiate» non distingue le due,
e infatti con la regola di scope cancellata restava verde.

Adesso il gate chiede **quale** regola ha sparato, leggendo la riga di log, e
con il sabotaggio diventa rosso. Prima di quello aveva anche un altro difetto
dello stesso tipo: mirava a un blocco a quaranta nodi, quindi misurava il
controllo della portata.

### 6.5 Limite dichiarato, e grosso

DESIGN.md 4.4 vuole i chunk di un osservatore **confinati in una sfera attorno
al punto d'uscita del portale**. Quel punto qui non esiste ancora: nessun
portale nomina un altro server. Finché non lo fa, un osservatore può farsi
mandare tutto il mondo camminando con le richieste di posizione — ed è
esattamente il motivo per cui l'impostazione che rifiuta gli osservatori esiste.

L'altra metà di §4.4 — i **ticket firmati** da un server che l'altro considera
fidato — non c'è. Qui c'è solo la seconda porta: «questo mondo accetta
osservatori anonimi», che è un'impostazione per-mondo e non ha bisogno di
infrastruttura. Il ticket serve quando la porta è chiusa.

---

## 7. Quante connessioni servono, non due

Il client teneva **una** sessione di rete e la scena che disegna teneva **due**
mondi. Nessuno dei due numeri veniva da qualcosa: il due era la forma dello
spike — un mondo qui, uno dietro il portale — e non c'è niente in un portale che
si ferma a due. Un portale nel mondo dietro un portale è un terzo server; un
atrio con quattro porte è cinque.

### 7.1 `ClientSessionSet`

Le connessioni vivono in una lista, si chiamano per **id**, e l'id sopravvive
alla chiusura di quelle accanto. Chi tiene un id — un'apertura che mostra il
mondo 3 — non deve ritrovarselo puntato al mondo 4 perché il mondo 2 se n'è
andato. Per questo uno slot chiuso **resta**: risponde ancora, e quello che
risponde è «sono andato via», che è una cosa che chi guarda deve poter sapere.

Quello che possiede in più di un vettore è il **pump**. Aspettare `timeout_ms`
su ognuno di cinque socket a turno è cinque volte l'attesa:

```
pumping five sessions with a 40 ms timeout took 40 ms
con il timeout speso su ogni socket:            200 ms
```

Duecento millisecondi sono dodici fotogrammi a 60 Hz passati a non fare niente.
Il primo poll può bloccare, gli altri prendono quello che è già arrivato.

### 7.2 Misurato con cinque connessioni

`net_loopback`, cinque socket veri contro un server, un giocatore e quattro
finestre:

```
5 su 5 entrate
5 su 5 hanno un mondo proprio, 18590 byte in totale
il server ha 1 giocatori e 4 spettatori
chiusa una: 4 ancora in gioco, l'id 1 risponde "closed"
```

Cinque mondi in un processo, uno per connessione, nessuno condiviso — che è il
singleton da cui questo motore è nato per scappare.

**Trovato scrivendo il gate:** le quattro finestre ricevevano chunk e il *solo
giocatore* no. La richiesta di posizione era su un blocco che una sezione
precedente aveva costruito lì: la richiesta di un giocatore dentro un blocco
viene rifiutata, e una richiesta rifiutata non sposta il centro d'interesse. Un
osservatore non ha corpo, quindi passava. Il difetto era del test, ma il sintomo
è esattamente quello che vedrebbe un giocatore che spawna dentro un muro.

Sabotaggi: timeout speso per socket, 200 ms invece di 40; slot cancellato invece
che tenuto, due check rossi sugli id.

### 7.3 La scena: `linked()` invece di `other()`

`Session sessions[2]` è diventato una lista di `unique_ptr` — **unique_ptr**
perché queste tengono il proprio indirizzo: una `ClientSession` riceve puntatori
al mondo e alla tabella blocchi di una sessione, e una lista che sposta il
contenuto quando cresce consegnerebbe a una connessione viva un mondo penzolante.

`bool links_to_other` è diventato `int link_session`. Il bool era lo stesso
«due» dell'array: poteva solo voler dire «quella che non è questa». Un indice
dice **quale**, quindi un mondo può portare al terzo mentre il terzo porta al
quarto. `-1` vuol dire «torna in questo mondo», che è quello che fa ogni gate
esistente.

`Scene::other()` è sparito e al suo posto c'è `linked()`, che era già scritto nel
commento di `other()`: «quando ce ne sono di più diventa *quella a cui questo
portale porta*, e questa sparisce».

E lo swap adesso deve chiedersi dove va: `1 - active` nominava sempre una
sessione, `link_session` può valere -1. Rifiutato con una riga invece di
indicizzare fuori.

### 7.4 Sul percorso vero, non solo nei test

Il client di rete passa dal set anche quando la connessione è una sola: non c'è
uno slot privilegiato, perché il giorno che c'è, «la connessione» torna a essere
un singleton.

`--watch host:port`, ripetibile. Una connessione e un mondo per ognuno, in join
come **osservatori**. Misurato con tre server veri e un client:

```
looking into 127.0.0.1:25402 as world 1: playing
looking into 127.0.0.1:25403 as world 2: playing
world 1: playing, 32 chunks, 32 blocks in its table
world 2: playing, 32 chunks, 32 blocks in its table
```

Tre mondi con tre semi diversi, tre tabelle blocchi separate, in un processo.

### 7.5 Limiti dichiarati

Da quei mondi **non si disegna niente**, e lo dico invece di lasciarlo intuire da
un rettangolo nero: il renderer è il pezzo dopo.

Il punto di vista di un osservatore è, per ora, **la posizione del giocatore** —
un segnaposto. Quello che dovrebbe essere è dove l'apertura esce in quel mondo, e
nessuna apertura nomina ancora un altro mondo. Quando lo farà, quella riga è il
posto dove va il punto d'uscita, e con lui la sfera di §4.4.

---

## 8. Un'apertura che nomina un altro server

§7 ha dato al client quante connessioni servono. Questo dà loro un motivo: la
**bocca** — un'apertura che porta al mondo di un altro server, e dentro cui si
vede quel mondo.

### 8.1 La destinazione sta sul link, non sull'apertura

`Portals` porta `dest_host` e `dest_port` **a livello di messaggio**. È quello di
cui parla: la prima apertura è la bocca di qua, e quello è il mondo che c'è
dall'altra parte. Vuoto vuol dire coppia locale, che è ogni scena che questo
motore ha avuto finora.

**La geometria dell'altro capo non c'è, e non per dimenticanza.** Questo server
non sa dove sia l'apertura dell'altro, e una copia qui sarebbe una copia che
scade nel momento in cui qualcuno di là la sposta. Il client lo chiede a quel
mondo direttamente: una sessione passiva riceve il suo `Portals` come chiunque
altro. È il pezzo che rende `Scope::Observe` di §6 utile per qualcosa.

Mezza destinazione è rifiutata dal decoder: un host senza porta è un posto a cui
nessuno può collegarsi, una porta senza host è un numero.

### 8.2 Con una destinazione, questo server non fa attraversare nessuno

Muovere le coordinate del giocatore lo metterebbe **in questo mondo** davanti
all'apertura lontana, che non è dove porta il portale. L'attraversamento vero è
uno scambio di sessioni sul client più un join dall'altra parte, e nessuna delle
due cose si fa spostando coordinate.

Quindi finché quello non esiste, una bocca è **qualcosa da guardare, non da
attraversarci dentro**, e il server lo rifiuta esplicitamente:

```
al client arriva: la coppia porta a altrove.example:25302
camminandoci dentro:  0 teletrasporti
```

Con il rifiuto sabotato: 1 teletrasporto, gate rosso. È l'unico modo di
distinguere «non attraversa perché è una bocca» da «non attraversa e basta».

### 8.3 Il client apre la sessione da solo

Non c'è un flag. Il server dice che l'apertura porta a `host:port`, e il client
va a guardarci: aggiunge un mondo alla scena, apre una connessione come
**osservatore**, aspetta la tabella blocchi, costruisce il renderer per quel
mondo, e infine costruisce il link quando quel server dice dov'è la sua
apertura. Quattro cose in ordine, e nessuna si può fare all'avvio: la
destinazione arriva dal server, la tabella arriva dal join, l'apertura lontana
arriva dall'altro server.

Stessa identità e stessa cifratura del join del giocatore — **non** lo stesso
pin, perché quella chiave è del server in cui il giocatore sta, e il mondo
dentro l'apertura è un altro. Un portale che scendesse di nascosto a una
connessione più debole sarebbe un portale che insegna a non guardare il
lucchetto.

E il punto di vista dell'osservatore adesso è quello giusto: **il centro
dell'apertura di là**. Era la posizione del giocatore, dichiarata come
segnaposto in §7.5; adesso quel punto esiste. La sfera di §4.4 attorno a
quel punto è ancora lato client — il server non la impone.

### 8.4 Misurato con due server veri

```
and the first one leads to 127.0.0.1:25412 -- a mouth, not a pair
the opening leads to 127.0.0.1:25412 -- looking into it as world 1
world 1 shows its opening at 40.50 21.00 44.50
its far end is drawn at 40.50 21.00 44.50 and that server says 40.50 21.00 44.50
world 1 meshed 56688 vertices
```

Cinque check: la sessione verso quel mondo esiste, il capo lontano del link è
dove **quell'altro** server dice che sta la sua apertura, il mondo disegnato
dentro l'apertura è quello in cui quella connessione fa streaming, c'è una
apertura sola (la seconda sarebbe un buco che riporta fuori da un mondo che non
è qui), e quel mondo è stato **meshato**.

### 8.5 Due difetti che il gate ha preso

**Il mondo dentro l'apertura non veniva meshato.** Il ciclo di rete faceva
`sync` del solo mondo attivo; il mondo lontano aveva i chunk e nessuna
geometria, quindi la bocca era un buco pieno di cielo. Letto come `0 vertices`.
È lo stesso difetto, con le stesse parole, che il caso locale aveva già avuto —
«prima del primo passaggio vedo solo il cielo» — e la riparazione è la stessa
funzione, `sync_sessions`, chiamata anche qui.

**Il link non seguiva l'apertura vicina.** Costruito solo quando cambiava
l'apertura *lontana*: un giocatore che sposta la bocca lasciava il link
disegnato dov'era prima. E l'ordine contava — il link legge `portal_a`, quindi
costruirlo prima che arrivi il nuovo `portal_a` lo disegna dov'era e poi il test
di versione dice che non è cambiato niente.

**E il sabotaggio non mordeva, per il motivo di §4.5.** Le aperture sono stato
del mondo e restano lì: il server aveva già la bocca dove il run precedente
l'aveva messa, quindi il piazzamento non spostava niente e `src == got` era vero
per caso. Con un server pulito il gate diventa rosso. Aggiunta anche una
condizione di ordine: il piazzamento non parte prima che il link della bocca
esista, altrimenti corre contro il primo messaggio del mondo lontano.

### 8.6 Cosa manca

**Attraversare.** Oggi la bocca si guarda. Il passo che manca è lo swap di
sessioni — che M5 ha già, `active = i` più le invarianti di §4.3 — più il
promuovere la sessione passiva a giocante, cioè un secondo join con scope
`Enter` e il ticket firmato di §4.4 per farlo su un server che non accetta
chiunque.

E l'immagine è misurata per **geometria**, non per pixel: che il mondo giusto
sia meshato e che il link punti dove dice l'altro server. Un confronto di pixel
contro una vista di riferimento di quel mondo è la misura che manca, e la
macchina per farla esiste già nei gate locali (`continuity_between`).

---

## 9. Attraversare, fra due server

§8 ha fatto la bocca: si guarda dentro. Questo la attraversa, e il test di M6 in
DESIGN.md §8 è **senza schermata di carico**.

### 9.1 Una finestra diventa una porta

Il pezzo che mancava non era il salto: era che la sessione passiva dietro
l'apertura ha **già** ricevuto quel mondo. Riconnettersi come giocatore
significherebbe arrivare con un interest set vuoto e farsi rimandare ogni chunk
— cioè una schermata di carico, con un altro nome.

Quindi `ChangeScope` (tipo 11): la stessa connessione cambia di scopo. Chi
guardava diventa giocante; chi giocava diventa una finestra sul mondo che ha
lasciato. Nessun secondo handshake, nessun secondo mondo.

Misurato, ed è la riga per cui esiste tutta la sezione:

```
32 chunk prima dell'attraversamento, 32 dopo
```

Con l'interest azzerato all'arrivo — cioè arrivando come una connessione nuova —
diventano 16 su 8: il mondo arriva due volte, gate rosso.

### 9.2 L'arrivo lo calcola il server che riceve

Il client manda **l'apertura in cui è entrato** e la sua posa dentro di essa. Quel
server non conosce quell'apertura, è di qualcun altro, quindi viene spedita — e
poi **controllata**: la posa deve stare dentro i bordi dell'apertura che dichiara,
e l'apertura deve avere una dimensione plausibile. Poi l'arrivo esce da
`portal_traverse` contro l'apertura **di questo mondo**.

È quello che limita la bugia. Comunque sia modificato il client, l'arrivo esce
relativo al buco che c'è davvero: al massimo può scegliere *in che punto*
dell'apertura arriva, esattamente come può scegliere dove mettersi quando entra
in un portale locale. Quello che non può fare è arrivare da un'altra parte del
mondo.

Non è una rivendicazione di inattaccabilità: rinunciarci del tutto — lasciare che
il client dica dov'è arrivato — non sarebbe peggio di un join nuovo, che la prima
richiesta la prende per buona. Non sarebbe nemmeno meglio, e questo costa venti
byte.

```
una posa fuori dall'apertura dichiarata:  porta chiusa, resta una finestra
la posa giusta:                           came out at 40.50 20.80 45.10
```

**Il primo caso «fuori» era a quaranta nodi e passava con il controllo
cancellato**: l'arrivo finiva dentro il terreno e veniva rifiutato come
bloccato, cioè misurava un'altra regola. Adesso è a tre nodi, sopra terreno
libero: rifiutato solo dal controllo dei bordi.

### 9.3 Il giro completo, con due server veri

Due server che si puntano a vicenda (`--portal-to` incrociato), un client che
cammina dentro l'apertura:

```
walked into the opening; asking 127.0.0.1:25422 to take us
came out at 39.93 20.00 45.10 in the other world;
    now playing on 127.0.0.1:25422, watching 127.0.0.1:25421
world 0 meshed 63540 vertices
```

E nei due log dei server, la stessa cosa dalle due parti:

```
A:  'pell5' joined as player  ...  'pell5' stepped back out of this world and is watching it
B:  'pell5' joined as observer  ...  'pell5' came out of this world's opening at 39 20 45
```

Il mondo appena lasciato è adesso l'immagine dentro l'apertura — 63540 vertici,
già meshati, perché erano già lì.

### 9.4 Cosa si scambia, oltre alla sessione

`arrive_from_portal` fa tutte le invarianti di DESIGN.md 4.3 — avatar proprio,
view range, cielo che è già del mondo giusto perché sta nella struttura — più
una che esiste solo qui: **il render target si sposta**. Ce n'è uno, appartiene
al mondo in cui si sta, e il mondo appena lasciato non ne ha bisogno. Con lo
spostamento sabotato il gate diventa rosso su «una apertura, disegnata».

E nessun warp di telecamera: l'occhio *era* continuo attraverso l'apertura —
l'immagine dentro serviva a quello — quindi non c'è niente da assorbire.

### 9.5 Tre difetti che il gate ha preso

**La camminata scritta a mano scriveva posizioni.** Una linea retta ad altezza
fissa attraversa le colline, e il server rifiuta ogni richiesta dentro una.
Adesso la camminata guida **l'input** — gira verso l'apertura e preme avanti —
e si prende collisione, gradino e gravità gratis, che è quello che fa un
giocatore.

**Due controlli saltavano la connessione sbagliata.** Dopo un attraversamento il
giocatore non è più sulla connessione con cui è entrato, e il codice che salta
«la propria» saltava quella originale. Uno faceva contare il mondo del giocatore
fra quelli guardati (gate rosso, trovato subito); l'altro **mandava una richiesta
di posizione da osservatore sulla connessione del giocatore**, trascinandogli il
centro d'interesse sull'apertura lontana a ogni tick. Quel secondo era invisibile
nel test perché le due aperture stanno per caso alle stesse coordinate nei due
mondi — un difetto che il gate non vedeva e che la lettura del codice ha preso.

### 9.6 Limiti dichiarati

Nessun **ticket**: chi entra viene ammesso perché quel mondo ammette chiunque
(`allow_observers`, e un posto giocatore libero). La seconda porta di §4.4 — un
ticket firmato dal server di partenza — non c'è, e serve per i mondi che non
accettano chiunque.

*Tutti e tre i limiti di questa sezione sono chiusi: i primi due in §10, il
ticket in §11. Restano scritti qui perché il seguito si legge meglio sapendo da
dove partiva.*

---

## 10. Quale apertura, e cosa dice un rifiuto

Due dei tre limiti scritti in §9.6. Non erano ipotesi: erano due frasi in un file,
e questa sezione le cancella con dei numeri.

### 10.1 L'apertura la decide il collegamento, non chi ci passa

Prima si usciva **sempre dall'apertura 0** del mondo di destinazione. Non era una
regola: era quello che succede quando nessuno manda un numero.

Adesso il numero sta **sul collegamento** — `Portals::dest_portal`, deciso da chi
ha configurato quel lato — viaggia al client dentro il messaggio delle aperture,
e torna indietro dentro il `ChangeScope` che il client manda di là. Il server che
riceve lo usa per scegliere *la propria* apertura, e rifiuta un numero che non ha.

Il giocatore non lo sceglie. La bugia più larga che un client modificato può dire
resta quella di §9.2: al massimo scegli *in che punto* dell'apertura arrivi, e
adesso anche *quale* delle due aperture che quel server ha davvero installato.
Non è "da qualsiasi parte del mondo".

Sul filo: `--portal-to host:port/1`. Due server veri, la bocca di A puntata alla
**seconda** apertura di B:

```
walked into the opening; asking 127.0.0.1:25432 to take us out of its opening 1
came out at 61.10 20.00 61.08 in the other world
the link named opening 1, and that world's openings are at
    40.50 21.00 44.50 and 60.50 21.00 60.50
```

Le due aperture di B stanno a **26 nodi** l'una dall'altra, apposta: «vicino a
un'apertura» e «vicino a quella giusta» non possono essere la stessa
affermazione. Con `opening(req.portal_id)` riscritto a `opening(0)` l'arrivo
finisce a 0.6 nodi dalla prima e 25.2 dalla seconda, e il gate diventa rosso su
entrambe le metà.

Un difetto che questo ha tirato fuori: il **seed dell'arrivo**. Dopo un
attraversamento si semina lo stato del trigger, o il giocatore riesce
dall'apertura da cui è appena uscito. Si seminava sempre `portal_a_state`; con
l'arrivo dall'apertura 1 quello è lo stato sbagliato, e il giocatore arriva
dentro un trigger intatto. Adesso segue l'apertura usata.

E il gate del client aveva la stessa assunzione dentro: controllava l'arrivo
contro `portal_a` del mondo d'arrivo. Con la bocca puntata all'apertura 1 è
diventato rosso — un gate che misurava «vicino all'apertura 0» invece che «vicino
all'apertura giusta». Adesso legge il numero che il collegamento ha nominato.

### 10.2 Un rifiuto è un messaggio, non un silenzio

Prima: se il mondo di là non prendeva il giocatore **non arrivava niente**, e il
client lo scopriva aspettando due secondi. Due secondi di un giocatore fermo
dentro un'apertura che non lo prende, senza niente sullo schermo che dica perché.
È lo stesso difetto di una connessione che si pianta invece di essere rifiutata.

Adesso c'è `ScopeDenied`: cosa era stato chiesto, il codice del motivo, e la
frase. **Solo per un rifiuto** — una richiesta accolta risponde con quello che ha
prodotto (un `Teleport` per chi arriva, niente per chi torna a guardare). Non
chiude la sessione: la richiesta è fallita, la sessione sta bene, e resta quello
che era.

Su socket vero, con B senza posti giocatore (`--max-players 0`):

```
the world through the opening refused us after 0.3 ms:
    this world is full of players (server is full)
1 crossing(s) refused, answered in 0.3 ms; 0 waited out the timeout instead
```

0.3 ms contro 2000. Con la risposta del server cancellata: `answered in never`,
`1 waited out the timeout instead`, due gate rossi.

**Il gate qui ha avuto un difetto suo, ed è quello che vale la pena leggere.**
La prima versione girava solo `if (crossings_denied > 0)` — cioè solo se un
rifiuto era arrivato. Con la risposta sabotata non diventava rosso: **spariva**.
Il caso che il messaggio doveva eliminare era esattamente il caso in cui il
controllo non veniva eseguito. Adesso la condizione è «un attraversamento è stato
tentato e non è arrivato nessuno da nessuna parte», e il timeout fa fallire il
controllo invece di saltarlo.

Il codice del motivo è portato oltre la frase apposta: `ArrivalBlocked`,
`ServerFull`, `NoSuchOpening` sono cose su cui un client può agire senza fare il
parsing dell'inglese. E un motivo che arriva da qualcosa di più nuovo di questo
parser diventa «nessun motivo», non un valore che nessuno sa nominare — è la
stessa regola di `RefuseReason::Last` che §5 aveva già dovuto imparare una volta.

### 10.3 Un difetto trovato di lato: i posti per le finestre

Il server apriva la porta con `max_players` peer e basta. Un osservatore è un
peer come gli altri, contato però contro un numero diverso apposta — quindi un
mondo pieno di giocatori **non poteva essere guardato affatto**: il nono peer non
prendeva un socket, e quindi non arrivava mai all'handshake che l'avrebbe
ammesso come finestra. Adesso la capacità è `max_players + max_observers`.

Non è stato preso da un gate. È stato preso mettendo `--max-players 0` per
misurare un rifiuto, e scoprendo che con zero posti non si connetteva nemmeno la
finestra.

### 10.4 Limiti dichiarati

Resta il **ticket** di §4.4, che è il limite grosso di §9.6: si entra perché quel
mondo accetta chiunque, non perché qualcuno ha detto che puoi.

`dest_portal` e `portal_id` sono **un byte utile ciascuno**: il protocollo ha due
aperture per mondo e il decoder rifiuta qualsiasi numero sopra 1. Un mondo con
molte aperture è un cambio di formato, non di logica.

E il numero è **statico**: sta nella configurazione di chi ha aperto la bocca, e
non c'è modo per il server di destinazione di dire «quella apertura adesso è
altrove, usa l'altra». Il rifiuto `NoSuchOpening` esiste, quindi il caso non
finisce in un arrivo sbagliato — finisce in un attraversamento che non avviene.

---

## 11. Il ticket: la seconda porta di §4.4

Il limite grosso di §9.6 e §10.4. Fino a qui si entrava in un mondo perché quel
mondo **accetta chiunque** (`allow_observers` e un posto libero). Adesso c'è
l'altra: il server A firma una dichiarazione su un giocatore, e B lo ammette
perché si fida di A.

### 11.1 Cosa c'è dentro la firma, e perché ogni campo

```
"cnp1 portal ticket"
issuer(32) subject(32) audience(32) portal(4) scopes(1) issued(8) expires(8)
firma(64)                                                    -> 181 byte sul filo
```

- **issuer** — chi ha firmato. La chiave che B cerca nella propria lista.
- **subject** — *quale giocatore*. Senza, il ticket è al portatore: chiunque lo
  copi dal filo lo può spendere. Con, spenderlo richiede anche la metà segreta
  di quella chiave, che viene provata a parte contro il channel binding della
  sessione (§5). Sono due meccanismi diversi che si tengono a vicenda.
- **audience** — *quale server*. Senza, un ticket che A emette per B vale anche
  a C, e ogni server di cui A si fida diventa per sbaglio un server che si fida
  dei giocatori di A. A impara la chiave di B dalla configurazione; B confronta
  con la **propria**, quindi un NAT o un secondo nome di host non rompono il
  controllo — cosa che un audience `host:port` farebbe.
- **portal** — quale apertura. L'attraversamento ne nomina una (§10.1) e il
  ticket deve essere d'accordo, o un ticket vale per tutti i buchi del mondo.
- **scopes** — guardare e attraversare sono **permessi diversi**. Un mondo può
  essere contento di farsi vedere e schizzinoso su chi esce dalle sue aperture.
- **issued/expires** — una finestra. Non una lista di revoche: questo è un
  portale, non una banca.

Le due porte di §4.4 restano separate e nominabili: `--no-observers --trust <hex>`
è «solo i portali di chi conosco», `allow_observers` da solo è «chiunque», e
nessuna delle due è «tutti tranne quelli che ho dimenticato di elencare».

### 11.2 Ogni campo è misurato, uno alla volta

Il punto non è «una firma verifica». È che **ogni parte** della dichiarazione
venga controllata. Sette casi su socket vero, sei rifiuti e un ingresso:

```
no ticket at all                       -> this world only lets in portals it has been vouched for
a ticket for somebody else             -> the ticket was issued for a different player
a ticket for another server            -> the ticket was issued for a different server
a ticket from a server this world does not trust
                                       -> this world does not trust the server that signed it
a ticket that ran out an hour ago      -> the ticket has expired
a ticket for walking in, presented to look in
                                       -> the ticket does not cover that
a ticket that says all of it           -> dentro, e il server registra di chi era la parola
```

Cinque sabotaggi, uno per controllo — soggetto, audience, fiducia, scadenza,
firma — e **ciascuno fa diventare rosso esattamente il proprio caso e nessun
altro**. Un controllo che ne coprisse due sarebbe un controllo che, tolto, ne
lascia passare uno in silenzio.

Sulla firma, a parte: sei modifiche di un campo alla volta, sei prese. Una firma
su quattro campi su sette verifica altrettanto volentieri, e i tre lasciati
fuori sono i tre che vale la pena cambiare.

**Un gate verde per il motivo sbagliato, preso.** Il caso «ticket modificato
dopo la firma» metteva `portal_id = 1` nel ticket e mandava la richiesta con
`portal_id = 0`: veniva rifiutato dal confronto fra i due, *prima* che la firma
venisse calcolata. Verde, e misurava un'altra regola. Adesso anche la richiesta
dice 1, così l'unica cosa rimasta a prenderlo è la firma — e il gate legge la
frase che lo dice.

### 11.3 E le due estremità del giro

Chi emette rifiuta di firmare quello che non sa: senza la chiave del vicino
(`--portal-peer-key`) **non esce nessun ticket**, invece di uscirne uno con
audience vuoto — che sarebbe un ticket buono presso ogni server che si fida di
questo. E un ticket nomina un giocatore, quindi una sessione anonima non ne
riceve.

Il giro completo, due server veri, B con `--no-observers --require-ticket
--trust <chiave di A>`:

```
this world vouched for us at 127.0.0.1:25492, opening 1, for 300 seconds
walked into the opening; asking 127.0.0.1:25492 to take us out of its opening 1
came out at 61.10 20.00 61.07 in the other world
B:  'ticketed' joined as observer ... identity 58db:1559:2626:dc59,
        on a ticket from 51c3:f53d:8f4b:4655
```

Tolto il `--trust` a B, lo stesso comando:

```
server: --require-ticket with no --trust: no portal can come in, because there
        is nobody whose ticket this world would take
B:  refused 'ticketed' with a ticket: this world does not trust the server that signed it
```

Il client chiede **una volta sola** e per **entrambi gli scope**: guardare e
passare sono un viaggio solo, e chiedere due volte metterebbe un round trip in
mezzo a un attraversamento. Se il mondo non emette ticket, dopo un secondo si
va senza — che è il caso ordinario, e non deve costare un blocco.

### 11.4 Un portale che mentiva

Trovato mentre si scriveva questo, ed è il difetto più interessante della
sessione perché **tutti i gate erano verdi mentre succedeva**.

`link_to_remote` prendeva sempre `far.opening[0]` — l'immagine dentro la porta
era della prima apertura del mondo di là. §10.1 aveva insegnato
all'*attraversamento* a nominare un'apertura, e a questo no. Con
`--portal-to .../1`: la porta mostrava 40.50 21.00 44.50 e il giocatore usciva a
61.10 20.00 61.07. **Un portale che mostra un posto e ne porta un altro**, a 26
nodi di distanza.

Il gate non lo vedeva perché aveva la stessa assunzione dentro: confrontava
l'immagine con `far.opening[0]`, cioè con la stessa cosa sbagliata. Due copie
della stessa supposizione si confermano a vicenda.

Il controllo che l'avrebbe preso non è «l'immagine è dell'apertura giusta» ma:
**dove guardavi è dove esci**. L'immagine e l'arrivo li calcolano due pezzi di
codice diversi da due messaggi diversi, e adesso il gate stampa i due numeri
accanto:

```
the doorway was showing 60.50 21.00 60.50, and the player came out at 61.10 20.00 61.07
```

### 11.5 Limiti dichiarati

**Nessuna revoca.** Un ticket vale finché non scade, 300 secondi. Un giocatore
espulso da B lo resta finché il suo ticket non scade — o finché B non smette di
fidarsi di A, che è l'unica leva che c'è ed è grossa.

**Un orologio condiviso.** `expires` è tempo di parete su due macchine.
Tolleranza di 60 secondi in avanti (`kTicketSkew`), dichiarata nel protocollo e
non scelta in un punto qualsiasi; due macchine che divergono di più non possono
fare portale l'una verso l'altra. È l'unica cosa in questo protocollo che
dipende da qualcosa fuori dal protocollo.

**Le chiavi si configurano a mano.** `--portal-peer-key` e `--trust` sono esadecimale
sulla riga di comando. Non c'è scoperta, non c'è directory di server, non c'è
un modo per B di dire ad A «ecco chi sono». Per due server che si conoscono va
bene; per una rete di portali serve qualcos'altro, e quel qualcos'altro è
progetto, non codice mancante.

**Il ticket non protegge un join diretto.** Chi scrive l'indirizzo di B nel
proprio client non passa da un portale, e `--require-ticket` non lo riguarda:
quella è la manopola `--require-identity`. Detto perché «questo mondo non
ammette nessuno» sarebbe una lettura sbagliata della bandiera.

---

## 12. Una finestra su un server che non c'è

Terza clausola del test d'uscita di M6: *«una passiva verso un server morto non
spamma il log (regola del backoff)»*.

### 12.1 Due modi di soddisfare quella frase, e uno è sbagliato

Prima di questo lavoro il client faceva **un tentativo e basta**: dopo dieci
secondi la sessione andava in `timed out` e restava lì per sempre.

```
world 1: timed out, 0 chunks, 0 blocks in its table
```

Il log non veniva spammato. Ed è la lettura sbagliata della frase: era silenzioso
perché aveva **rinunciato**, e un portale che rinuncia non torna quando il server
torna. Quello che serve è continuare a provare, a un ritmo che **cala**.

L'attesa parte da `retry_first`, raddoppia, e smette di crescere a `retry_max`.
Un'ora di vicino morto costa una sessantina di handshake invece di centomila.

### 12.2 I numeri

Sei secondi contro una porta dove non c'è nessuno, timeout di handshake a 0,3 s:

```
6 s against a dead port: 6 attempts, 4 lines a log would get, 1185 pumps
```

**1185 pump, 6 tentativi.** Con il raddoppio tolto: **12 tentativi** — e il gate
diventa rosso su «l'attesa cresce» e su «l'attesa ha raggiunto il tetto».

La regola del log è una frase: dire qualcosa mentre l'attesa cresce, una volta
quando smette di crescere, e niente dopo. Sul client vero, quaranta secondi:

```
nothing behind the opening at 127.0.0.1:25999 (timed out); trying again in 2 s
nothing behind the opening at 127.0.0.1:25999 (timed out); trying again in 4 s
nothing behind the opening at 127.0.0.1:25999 (timed out); trying again in 8 s
```

Tre righe. (Nota: con il raddoppio sabotato le righe diventano **una** — perché
l'attesa non cambia mai. Il conteggio delle righe da solo non prende quel
difetto; è il conteggio dei tentativi che lo prende. Due controlli perché
misurano due cose.)

### 12.3 E torna da solo

La metà che «rinunciare in silenzio» non fa. Nessuno riavvia niente e nessun
chiamante se ne accorge: la stessa sessione, ancora in tentativo, trova un server
che quando è partita non c'era.

```
the world through the opening is back
world 1 shows its opening at 40.50 21.00 44.50
```

### 12.4 `Closed` vuol dire due cose opposte

Il difetto trovato scrivendo questo. Un socket che non va da nessuna parte
finisce in `TimedOut`; un server che **sparisce dopo un join riuscito** finisce
in `Closed` — che è lo **stesso stato** che produce un chiamante che chiude la
sessione.

Una è il chiamante che dice basta, l'altra è esattamente l'interruzione per cui
il retry esiste. Senza distinguerle, o la finestra su un server che muore non
torna mai, o una sessione chiusa apposta si riapre da sola — un socket che
nessuno ha chiesto. Entrambe misurate, e con il flag rimosso il gate «la ritrova
da sola» diventa rosso.

### 12.5 Un rifiuto non è un'interruzione

L'altra metà del non spammare, e riguarda il log **dell'altro server** quanto
questo. «Questo mondo non ammette osservatori» è una **decisione**: un client che
si riconnette ogni minuto per sentirsi dire la stessa cosa costa a quel server un
handshake al minuto per sempre, e mette la stessa riga in due log.

```
refused as 'this world does not allow observers': 1 attempt(s) in 3 s, given up: yes
refused as 'server is full':                      4 attempt(s) in 2 s, given up: no
```

`refusal_is_permanent()` divide le due cose, e **server pieno è l'unico rifiuto
transitorio**: è l'unico che parla di un momento invece che dei due capi. Un
portale che smettesse di funzionare la prima volta che qualcun altro è di mezzo
sarebbe rotto. Con la regola sabotata, il mondo che non ammette osservatori viene
richiesto **5 volte in 3 secondi** e il gate diventa rosso.

Un motivo che arriva da qualcosa di più nuovo di questo build è trattato come
permanente: riprovare qualcosa che non si sa nominare è la peggiore delle due
supposizioni.

### 12.6 Limiti dichiarati

**Nessun jitter.** Cento client che perdono lo stesso server riprovano tutti
insieme, e continuano a farlo in sincrono perché il ritardo è deterministico. Con
due server è irrilevante; con cento client su un server che riparte è un
tempesta di riconnessioni auto-inflitta. La correzione è nota (jitter sul
ritardo) e non c'è.

**Il mondo vecchio resta nel client.** Alla riconnessione i chunk già arrivati
non vengono buttati: se dall'altra parte è cambiato qualcosa mentre era giù, si
guarda il vecchio finché non arriva il nuovo. Per un mondo che è lo stesso mondo
va bene; per un server che nel frattempo ha caricato un altro mondo no, e non c'è
niente nel protocollo che dica quale dei due sia.

**Non c'è un heartbeat.** Un server che smette di rispondere senza chiudere il
socket non è ancora un caso che questa sessione noti: viene notato solo quando
ENet lascia cadere il peer. È la voce successiva di M6.

---

## 13. L'heartbeat di stato

Voce successiva di M6, e chiude il limite dichiarato in §12.6.

### 13.1 Due metà, e nessuna delle due la risponde qualcos'altro

**«Sono ancora qui.»** Non è quello che fa ENet. ENet pinga il peer e lo lascia
cadere quando il **socket** smette di funzionare — e un server con il loop
piantato risponde a quei ping perfettamente mentre non manda niente. Per il
client è indistinguibile da un mondo tranquillo, che è esattamente lo stato in
cui un portale sul server di qualcun altro non deve restare per sempre.

**«E questo è quello che sono.»** È la metà che lo rende diverso da un ping. Una
bocca mostra un mondo in cui il giocatore magari vuole entrare, e **se in questo
momento si può** — un posto libero, un mondo ancora disposto a farsi guardare —
non è una cosa che si deduce dai chunk. Senza, la prima notizia di un mondo
pieno arriva come un attraversamento rifiutato, con il giocatore già dentro
l'apertura.

**14 byte**, uno al secondo: 840 byte al minuto per giocatore.

### 13.2 Il caso che ENet non vede

Il server smette di rispondere **senza chiudere il socket**: il processo c'è, il
transport viene ancora pompato, i ping ricevono risposta, il peer non cade mai.
Solo l'applicazione è muta.

```
the server stopped answering: noticed after 2.00 s (silence set to 2.0), 5 more attempts
```

E poi è un'interruzione come le altre: riparte il backoff di §12.

**Prima versione del gate: verde per il motivo sbagliato.** Smetteva di pompare
il server *del tutto* — che è un'altra cosa. ENet se ne accorgeva da solo dopo
**5,13 s**, quindi con il controllo del silenzio cancellato la sessione finiva
comunque fuori da `playing` e metà del gate restava verde. Adesso il transport
del server continua a girare e solo `beat()` è spento: cancellando il controllo,
tre controlli diventano rossi.

### 13.3 La regola di compatibilità, che è anche un gate

Il silenzio si misura **solo dopo il primo battito**. Un server che non ne manda
non è un server morto: è un server che questa cosa non la fa.

```
6.0 s with no beat ever: playing, 1 attempt(s)
```

Con la regola tolta, la stessa sessione fa **8 tentativi in 6 secondi** — e non
perché il server sia caduto, ma perché al momento del join `m_beats` è zero e
`m_last_beat` pure, quindi la sessione si dichiara morta appena entrata.
Aggiungere l'heartbeat senza quella riga sarebbe stata un'aggiunta di protocollo
che rompe ogni peer che la ignora.

### 13.4 Un difetto trovato da un gate che falliva sempre

Il controllo «un mondo che ha smesso di far guardare dentro lo dice» falliva in
modo riproducibile. Non era l'heartbeat: era `connect()` che **non azzerava lo
stato del battito**. Dopo una riconnessione la sessione teneva l'ultimo battito
del server precedente e un `m_last_beat` da prima dell'interruzione — cioè un
mondo già scaduto, e uno stato di mondo che quella sessione non aveva mai
ricevuto.

(Il primo sospetto era una flakiness di temporizzazione. Non lo era, e il modo
di scoprirlo è stato stampare i numeri invece di cambiare le attese: il server
diceva `allow 0, trusted 0` e il battito arrivava con `flags 3`.)

### 13.5 Cosa ne fa il client

Una riga quando cambia, che è la differenza fra un giocatore che scopre che di
là è pieno **prima** e uno che lo scopre stando dentro l'apertura:

```
the world through the opening: 0/8 players, 1/8 windows, open to portals, room to walk in
```

### 13.6 Limiti dichiarati

**Il client non agisce ancora su quei numeri.** Li stampa. Non disegna un'apertura
diversa per un mondo pieno, e non impedisce di camminarci dentro — l'attraversamento
viene comunque tentato e comunque rifiutato con `ScopeDenied`. Il dato c'è, la
decisione no.

**Solo dal server verso il client.** Un client piantato che tiene aperta una
sessione passiva non viene notato dal server in alcun modo: resta un peer che
occupa un posto osservatore finché ENet non lo lascia cadere. La direzione
opposta è la stessa idea e non c'è.

**Un secondo per sessione, non una trasmissione unica.** Con molte sessioni è un
messaggio per sessione per secondo. A otto giocatori e otto finestre sono 16
messaggi da 14 byte al secondo, che è niente; a mille sarebbe da rivedere, e il
modo di rivederlo (una sola trasmissione, o un intervallo che cresce con il
numero di peer) non è stato costruito.

---

## 14. Il fallback a redirect

Ultima voce di M6 prima dei contenuti, e la parola che conta è **fallback**.

### 14.1 Perché serve un ripiego

L'attraversamento senza schermata di carico (§9) ha un prerequisito: una sessione
che **guarda già** il mondo di là. Ci sono motivi perfettamente ordinari perché
quella sessione non esista — è stata rifiutata, sta fra due tentativi dopo
un'interruzione (§12), o **quel client le sessioni passive non le apre affatto**.
Senza un ripiego, in tutti quei casi la bocca è un buco in cui il giocatore entra
e non succede niente.

Il ripiego è: il server dice **dove**, e il client ci si riconnette.

### 14.2 Lo decide il server, come ogni teletrasporto qui

DESIGN.md 3.4. Il server possiede l'apertura ed è lui che vede qualcuno entrarci,
quindi è lui a mandare `Redirect{host, port, portal_id, ticket, motivo}`.
Cosa ne fa il client dipende da cosa ha già: con una finestra aperta su quel
mondo l'attraversamento senza salto è disponibile e il redirect si butta.

**Porta un ticket**, perché un redirect verso un mondo che ne richiede uno è
altrimenti un redirect verso un rifiuto — e il client non può chiederlo dopo,
perché a quel punto sta parlando con l'altro server. Misurato togliendolo:

```
redirected to 127.0.0.1:25492 opening 1: you walked into an opening onto ...
the redirect went nowhere: no ticket this world will honour
```

Il ticket copre **entrambi** gli scope in una volta, perché guardare ed entrare
sono un viaggio solo: il client presenta lo stesso ticket nell'hello e poi nel
`ChangeScope`. Anche questo misurato per assenza — passando `nullptr` al secondo,
il mondo risponde *«this world only takes players it has been vouched for»* con
il ticket inutilizzato nell'hello.

### 14.3 Niente arrivo nuovo: gli stessi messaggi, un mondo in meno

Il ripiego non ha una via d'arrivo propria. Apre una connessione come
**osservatore** con il ticket, e appena è dentro manda lo **stesso** `ChangeScope`
che manda il percorso senza salto, con la stessa apertura e la stessa posa.
L'unica differenza è che il mondo non era già arrivato.

Che è esattamente cosa costa, in numeri:

```
seamless:  that world had 32 chunks before the crossing and 32 after
fallback:  0 chunk(s) in hand when the player arrived, 32 by the end; 2 ms
```

**Zero contro trentadue** è la schermata di carico da cui prende il nome. E il
gate legge tutte e due le metà: che all'arrivo il mondo non c'era, e che dopo è
arrivato — una sola delle due sarebbe compatibile con un ripiego che non ripiega.

### 14.4 Il difetto sotto: il trigger campionato al ritmo delle richieste

Il pezzo che ha reso il resto inutile finché non è stato trovato.

Una richiesta di posizione arriva 20 volte al secondo e un giocatore in corsa fa
circa 0,4 m fra due — **più larga della fascia che il trigger guarda**. Provare
solo *dove il giocatore è* invece di *dove è passato* scavalca il portale.

Il client non se ne accorgeva perché campiona a ogni frame con la propria copia
della stessa regola. È diventato visibile nel momento in cui il server era
l'unica cosa che poteva accorgersene: un client senza finestra dietro la bocca
ci cammina dentro, finisce un nodo e mezzo oltre, e non succede niente.

```
DBGSRV feet 40.61 20.00 43.09   (piano a 44.50, otto giri di trigger, zero attraversamenti)
```

La correzione è `trigger_along`: la **stessa** `portal_step_trigger` che usa il
client, camminata lungo il passo a sotto-campioni più piccoli della fascia, con
un tetto sul numero. Non un test swept: due regole diverse per lo stesso evento
è la cosa da evitare qui. Applicata anche all'attraversamento locale, che aveva
lo stesso buco.

Misurato direttamente: due richieste, una **2,0 nodi davanti** e la successiva
**2,0 nodi oltre** — nessuna delle due dentro l'apertura, e distanti abbastanza
nel tempo perché il controllo del movimento le accetti. Con i sotto-campioni, un
redirect. Senza, zero, e sette controlli rossi.

### 14.5 Due gate verdi per il motivo sbagliato, e un difetto della camminata

**Il blocco del ripiego spariva invece di diventare rosso.** Girava solo se un
ripiego era avvenuto; cancellando il `Redirect` del server, il run **passava**.
Adesso gira ogni volta che questo client non apre finestre e ha visto una bocca:
in quella configurazione un attraversamento *deve* avvenire.

**La camminata scritta aveva due versioni sbagliate.** Puntare al **centro**
dell'apertura si ferma con i piedi a un decimo di nodo dal piano: sembra un
arrivo, e nessuno ha attraversato niente (40,62 / 44,58 contro un piano a 44,50).
Puntare a un punto **oltre** l'apertura è peggio: l'avvicinamento è diagonale,
quindi la linea taglia il piano prima di arrivare all'apertura — piano attraversato
a x 42,3 con l'apertura fra 40,0 e 41,0, cioè attraverso il muro accanto.
Adesso: al centro, e da vicino dritto attraverso. Che è quello che fa una persona.

E una terza: la camminata **spingeva contro le colline**. Un run su tre finiva
cinque nodi corto dopo diciotto secondi. Adesso salta quando in mezzo secondo non
si è mosso di un quarto di nodo. Il gate stampa dove il giocatore è finito e
dov'era la bocca, perché un rosso senza quei due numeri è un mistero.

### 14.6 Limiti dichiarati

**Il redirect si manda sempre.** Il server non sa se quel client ha già una
finestra, quindi lo manda a ogni attraversamento della bocca e il client lo
ignora quando non gli serve. Sono 72 byte per attraversamento; l'alternativa
sarebbe che il client dichiari cosa tiene aperto, che è stato più protocollo di
quanto valga.

**Il mondo lasciato si chiude.** Chi attraversa per redirect non ottiene una
finestra su dove era: quella connessione viene chiusa. Il percorso senza salto la
tiene (§9), questo no, e tornare indietro costa un altro giro completo.

**Un ordine di arrivo, non una garanzia.** Se il client prende il ripiego e
l'altro mondo lo rifiuta, il giocatore resta dov'era e lo legge — ma la
connessione che aveva è già stata chiusa nel caso riuscito, non in questo. È
gestito, non è elegante.

**Il client non chiede mai un redirect.** Non c'è un messaggio «mandami di là»:
l'unico modo di ottenerne uno è camminare dentro l'apertura. Per un mod che
voglia spostare un giocatore (il `redirect_player` del fork) manca il lato che lo
chiede, non quello che lo esegue.

---

## 15. Due server con *game* diversi

Ultima clausola del test d'uscita di M6 rimasta non misurata: *«due server su
macchine diverse, con game diversi»*.

### 15.1 Funzionava già. Nessun gate lo dimostrava

A con le quattro mod, B con `--no-mods`, un client, un attraversamento:

```
A: block table 3e88c6445856edd9 (32 nodes)
B: block table fbf689b78a30fb55 (6 nodes)
crossed once ... 32 chunks before, 32 after
```

E il controllo che c'era — *«each into a world and a block table of its own»* —
confrontava **puntatori**. Due mondi con tabelle identiche byte per byte lo
passavano altrettanto volentieri. Non è quella la proprietà.

### 15.2 La proprietà, detta bene

Gli id si leggono attraverso la tabella **della sessione da cui sono arrivati**,
e attraverso nessun'altra. Adesso è un ciclo su tutta la tabella, non un
controllo a campione: una tabella condivisa sarebbe d'accordo su tutti i blocchi
core e in disaccordo esattamente su quelli aggiunti da una mod — cioè il caso
a campione lo prende solo se il campione cade nel posto giusto.

### 15.3 Il caso difficile, costruito apposta

Due modi in cui due game differiscono: **lo stesso id nomina blocchi diversi**, e
**una tabella ha blocchi che l'altra non ha**. Con mod vere di solito capita il
secondo, perché i nodi core si registrano per primi e nello stesso ordine dalle
due parti.

Il primo si costruisce: in `net_loopback` un secondo mondo registra
`test:only-here` **prima** dei nodi core, così si prende un id dentro l'intervallo
che l'altro server usa per i propri blocchi.

```
6 blocks here, 7 there; id 1 is 'core:stone' and 'test:only-here'
in this client: id 1 is 'core:stone' on one and 'test:only-here' on the other
the ground of the far world is id 3: 'core:grass' in its own table,
                                     'core:bedrock' in the other one
```

**Id 3 è due blocchi diversi nello stesso istante nello stesso processo**, e il
terreno del mondo lontano si legge giusto perché lo si legge con la tabella
giusta. Letto con l'altra sarebbe bedrock.

Sabotaggio: una `static NodeRegistry` condivisa da tutte le sessioni — che è
esattamente il difetto per cui il controllo esiste. **Sei controlli rossi**, e la
riga stampata lo dice da sola: *«id 1 is 'core:stone' on one and 'core:stone' on
the other»*.

### 15.4 Sul filo, fra due server veri

`--different-games` sul client dice che quel run *è* la clausola del test
d'uscita, così il gate la **afferma** invece di riportarla:

```
1 of 1 world(s) are a different game from this one (block table fbf689b78a30fb55 here)
of the 6 ids both tables have, 0 name different blocks;
    0 block(s) here are unknown there and 26 there are unknown here
```

**Contato nelle due direzioni, e la prima versione non lo faceva.** Un
attraversamento scambia quale connessione è «qui»: partendo dal mondo con le mod
e arrivando in quello senza, ogni blocco di «qui» esisteva anche di là e il
conteggio diceva zero differenze. Verde, e misurava niente.

### 15.5 Cosa resta di M6, e perché non è codice mancante

**I media non esistono.** Le texture dei blocchi le genera `make_atlas` dal
registry — `colour`, `checker`, `opacity`. Nessun file, niente da indirizzare per
contenuto e niente da mettere in cache. «Media content-addressed con cache
condivisa» è un trasporto per un carico che non c'è: i file arrivano con il
contenuto di gioco, che è M7.

Detto qui e non lasciato come voce spuntata a metà: la parte di M6 che si può
costruire senza inventare il carico è costruita e misurata; quella che non si
può aspetta il carico.

**Gli id di contenuto per-sessione ci sono**, ed è quello che questa sezione
misura: un registry e un atlante per mondo, riempiti dal welcome di quel mondo.

---

## 16. I media: prima bisogna che esistano

§15.5 diceva che «media content-addressed con cache condivisa» è un trasporto
per un carico che non c'è. Questo è il carico.

### 16.1 Generati, non scaricati, e il motivo

Quello che serve alla pipeline di DESIGN.md 4.5 — indirizzamento per hash, cache
condivisa, handshake a delta — sono **file con un hash**, non arte. Qualunque
byte esercita tutte e tre le cose. Generati costano zero licenze, zero download,
zero dipendenze; l'arte li sostituisce file per file quando l'aspetto inizia a
contare, e il trasporto sotto non cambia di una riga.

`tools/bake_textures` carica le mod — i blocchi che vale la pena texturare sono
quelli che una mod ha registrato — e scrive un PNG per nodo:

```
bake: 4 mods loaded
bake: 31 blocks; 31 written, 0 already there, 127.0 KB into assets/textures
```

**Non sovrascrive quello che trova.** Il senso dei file è che uno vero possa
rimpiazzare uno generato; un baker che clobbera cancellerebbe l'arte alla prima
esecuzione. `--force` per rifarli.

### 16.2 Un encoder PNG in cnp_core, e perché lì

Il **server** è la cosa che possiede un content set: i media viaggiano da lui,
quindi ciò che produce un segnaposto deve essere raggiungibile da una build
headless. `cnp_core` non include mai raylib, e non lo fa nemmeno adesso.

È il più piccolo encoder che produce un PNG vero: un IDAT di blocchi deflate
**STORED** — niente Huffman, niente finestra LZ77, nessuna compressione. È uno
zlib valido e ogni decoder lo legge.

```
32 x 32, 4196 bytes in 3 chunks -- 4.1 bytes a pixel, because nothing here compresses
```

Il costo è dichiarato: **un segnaposto generato è grosso per quello che mostra**.
L'arte vera esce da un encoder vero ed è più piccola. Niente sopra quel file se
ne accorge — i media viaggiano come byte opachi con un hash sopra.

Il test decodifica quello che l'encoder scrive: chunk per chunk, CRC per CRC, e i
blocchi stored a mano. Sabotato facendo coprire al CRC anche il campo lunghezza —
l'errore che ogni encoder PNG scritto a mano fa una volta — il controllo diventa
rosso.

### 16.3 Il segnaposto: una funzione, due chiamanti

`bake_tile` la usa il baker per scrivere il file **e** il renderer come ripiego
per un blocco il cui file manca. Se fossero due copie, lo stesso blocco disegnato
da disco e disegnato dal ripiego sarebbero diversi, e la differenza sarebbe
invisibile finché qualcuno non cancella un file.

Deterministico dal **nome**, non dall'id: gli id si assegnano in ordine di
registrazione e si spostano quando si aggiunge una mod, e una texture che cambia
perché hai installato qualcos'altro è una texture che nessuno può tenere.

### 16.4 Due gate verdi per il motivo sbagliato, sulla stessa riga

Il controllo che la trama **si ripete senza cucitura** — una texture che non
wrappa disegna una griglia sul mondo, una linea per blocco.

**Prima versione:** confrontava la media di due bordi opposti con una tolleranza
assoluta. Cancellando il wrap restava **verde**: 153 contro 149. Due punti non
correlati confrontati con una soglia larga.

**Seconda versione:** salto attraverso la cucitura contro i salti dentro la tile,
un *rapporto* invece di una tolleranza. Diventava rosso — **su una tile che wrappa
benissimo**: 11.97 contro 2.71. La cucitura cade su un bordo della scacchiera, e
una scacchiera ha bordi veri suoi; stavo confrontando un bordo con una coppia
qualunque.

**Terza:** senza scacchiera e senza bordo, così sotto resta solo il rumore.

```
onesto:    attraverso la cucitura 0.12, fra due vicini dentro la tile 1.47
sabotato:  attraverso la cucitura 6.88, fra due vicini dentro la tile 1.61
```

E il **bordo scuro è un parametro** proprio per questo: cade esattamente sulla
cucitura e la nasconde. Non è un aggancio per il test — l'arte vera ha i suoi
bordi e non ne vuole uno sintetico.

### 16.5 Il difetto che i file tolgono

Prima il client disegnava blocchi di cui non aveva mai ricevuto una texture,
perché se le rigenerava. Adesso c'è un numero:

```
atlas: 31 block(s) from a texture file, 0 from a generated placeholder
```

Con `--no-textures`: **0 da file, 31 generati**, e il gate diventa rosso. Il
ripiego resta — un blocco senza arte si disegna lo stesso — ma è *contato* invece
che invisibile.

### 16.6 Limiti dichiarati

**Una texture per nodo, non una per faccia.** `NodeDef::tex[6]` esiste e non è
usato: l'erba ha lo stesso disegno sopra, sotto e di lato. Serve arte vera prima
che valga la pena, e allora sarà un file per faccia con lo stesso trasporto.

**I file stanno in una cartella accanto al binario.** ~~`--textures <dir>`. È la
sorgente sbagliata per un portale~~ — risolto in §17.9: per un mondo altrui il
client legge solo i media di quella sessione, e la cartella locale serve soltanto
il mondo che genera lui. La **callback** del renderer è ciò che ha reso il cambio
una riga.

**Nessun hash ancora.** ~~I file si leggono per nome.~~ Risolto in §17: BLAKE2b-256
sul contenuto, cache condivisa su disco, delta all'handshake.

**Sono segnaposto e si vede.** Rumore su un colore piatto con un bordo. Restano
segnaposto finché non arriva arte vera; il posto dove prenderla, quando servirà,
è un pacchetto **CC0** (Kenney e simili) — non le texture di default dell'engine di partenza,
che sono quasi tutte CC BY-SA 3.0 e si portano dietro attribuzione e share-alike.

---

## 17. I media come contenuto: hash, cache condivisa, delta

§16.6 diceva «nessun hash ancora, i file si leggono per nome». Questo è l'hash,
e i tre pezzi che DESIGN.md 4.5 chiede: indirizzamento per contenuto, cache
condivisa su disco fra tutte le sessioni, e un handshake che manda solo il delta.

### 17.1 BLAKE2b e non BLAKE3, detto invece che nascosto

DESIGN.md dice BLAKE3. Questo non è BLAKE3.

Il motivo è che monocypher è **già linkato** — sigilla la sessione e firma i
ticket — e porta BLAKE2b; BLAKE3 sarebbe una dipendenza nuova comprata per una
proprietà che qui nessuno usa (la velocità su file grandi: qui i file sono da 4
KB). Sono entrambi a 256 bit e resistenti alle collisioni, e il formato sul filo
porta 32 byte in un caso e nell'altro: cambiarlo dopo costa una versione di
protocollo e zero progettazione.

Sta scritto in `content_hash.h` e qui perché **una deviazione silenziosa dal
documento di progetto è peggio di una dichiarata**.

L'hash è sui **byte del file come viaggiano**, non sui pixel che decodifica: due
encoder che producono la stessa immagine da byte diversi sono due file diversi, e
una cache che fingesse il contrario dovrebbe decodificare tutto ciò che tiene per
scoprirlo.

### 17.2 L'ordine è l'inverso di come si legge la frase

DESIGN.md dice: «il client manda la lista di hash che ha già; il server manda
solo il delta.» Preso alla lettera vuol dire un client che annuncia **tutta la
sua cache** — ogni texture di ogni mondo in cui abbia mai guardato — a un server
che di tutte quelle non sa che farsene.

Quindi: prima l'**offerta**, poi il client risponde *dentro* l'offerta.

```
MediaOffer  server -> client   nome, hash, dimensione, per ogni file
MediaNeed   client -> server   gli hash che non ha, al massimo 64 per volta
MediaData   server -> client   un file, byte compresi
```

Il delta è lo stesso delta. Quello che cambia è che la sua dimensione è limitata
dal mondo a cui ci si collega e non dalla storia del giocatore.

I media si offrono per **nome di file**, non per id di blocco: gli id sono per
sessione, e una texture sopravvive alla sessione che l'ha portata — il nome è
quello che il blocco del prossimo mondo chiederà, quando si scoprirà che è la
stessa pietra.

### 17.3 L'offerta viaggia dietro il welcome, sullo stesso canale

Prima versione: offerta sul canale eventi subito dopo il welcome sul canale
handshake. **Il client rifiutava di collegarsi**, una volta su una.

ENet ordina i messaggi affidabili *dentro* un canale e non fra canali. L'offerta
sorpassava il welcome, arrivava a un client che non aveva ancora aderito, e
`ClientSession` la trattava per quello che era: un messaggio che il server non
aveva motivo di mandare.

`SessionAction` ha ora un campo `more`, e va sul **canale della risposta**. Con
il welcome l'offerta è ordinata dietro di lui; con la richiesta i file vanno sul
canale eventi. Ed è dietro il welcome e non dopo un round trip perché il client
ha appena adottato la tabella dei blocchi di quel mondo: i nomi dell'offerta sono
nomi che sa già risolvere.

### 17.4 Il segnaposto si cuoce sul server, e questo cambia il cross-game

Il ripiego di §16.3 stava sul client. Non poteva funzionare per un portale: il
filo porta **nomi**, non colori, e un client non può cuocere il segnaposto di un
blocco di cui non sa niente. `adopt_node_table` inventa magenta, ed era tutto
quello che si poteva disegnare per i blocchi di un altro *game*.

Adesso il mondo che possiede i blocchi cuoce le loro immagini e le manda:

```
the world through the opening: 26 block(s) this build has never heard of;
its atlas is 31 from its own media, 0 generated
```

Ventisei blocchi che questa build non ha mai sentito nominare, disegnati con le
immagini di chi li possiede. È il pezzo che mancava a §15.

### 17.5 Una cache, non una per sessione

```
media: world 0 offered 31 file(s), 0 already here, 31 arrived (127.0 KB)
media: world 1 offered 31 file(s), 31 already here,  0 arrived (0.0 KB)
media: 62 offer(s) across 2 world(s) name 31 distinct file(s);
       31 were fetched, 31 were already on disk, 0 refused
```

Due server, 62 offerte, **31 file distinti, 31 scaricati**. È la frase di
DESIGN.md 4.5 come numero.

Il gate è scritto come **totale** e non come «il mondo 1 non ha scaricato
niente», e la differenza non è stilistica: con una cache calda non scarica niente
nessuno, e un gate che leggesse solo il mondo 1 chiamerebbe quello un successo
dimostrando zero. Quello che vale in entrambi i casi è che l'unione delle offerte
limita quanto è stato preso.

Con `--different-games` la deduplicazione attraversa due *game* diversi:

```
media: world 0 offered 31 file(s), 0 already here, 31 arrived (127.0 KB)
media: world 1 offered  5 file(s), 5 already here,  0 arrived (0.0 KB)
media: 36 offer(s) across 2 world(s) name 31 distinct file(s)
```

I cinque file del mondo senza mod erano già lì perché il mondo con le mod aveva
mandato le stesse texture dei nodi core.

### 17.6 Ogni lettura dalla cache è verificata

Il file viene ri-hashato e confrontato con la chiave **prima** che i suoi byte
escano. Una cache indirizzata per contenuto che si fida dei propri nomi di file è
una cartella dove qualunque cosa qualcuno riesca a scrivere diventa qualcosa che
il renderer disegna — e questa cartella è condivisa fra mondi, quindi chi scrive
non è necessariamente chi legge.

Costa una passata BLAKE2b su qualche kilobyte, cioè microsecondi. Un file che non
passa viene **cancellato** e contato come miss, così la sessione dopo lo riscarica
invece di morirci sopra per sempre.

Sabotato togliendo il confronto, il test serve i byte piantati e diventa rosso su
quattro righe.

C'è anche il livello sotto: `MediaData::decode` rifiuta un messaggio i cui byte
non hashano all'hash che porta. Sabotato ribaltando **un bit** sul filo:

```
media: 62 offer(s) across 2 world(s) name 31 distinct file(s);
       0 were fetched, 0 were already on disk, 62 refused
atlas: 0 block(s) from media this world sent, 31 from a generated placeholder
```

Zero byte corrotti sono finiti sul disco, e il gate è rosso su due righe.

### 17.7 Il sabotaggio che ha trovato un difetto vero

Sabotando `MediaCache::has()` perché rispondesse sempre «non ce l'ho», il gate è
diventato rosso come previsto — e ha stampato questo:

```
media: world 0 offered 31 file(s), 0 already here, 90024 arrived (368887.4 KB)
```

**90 024 file, 368 MB in venticinque secondi.** Il client chiedeva il prossimo
lotto quando quello in volo finiva, e decideva cosa manca chiedendolo alla cache:
una cache che non riesce a memorizzare — disco pieno, cartella in sola lettura —
lo faceva girare per sempre.

Adesso ogni file offerto ha due flag, *ce l'ho* e *l'ho chiesto*, e non si chiede
due volte. Stesso sabotaggio, stesso rosso, **62 file e 254 KB**.

L'altro capo dello stesso problema: una richiesta senza risposta bloccava il
renderer per sempre. Ora c'è `kMediaRequestTimeout` (15 s), i file rimasti si
contano in `media_dropped()` e si va avanti.

### 17.8 Cosa costa

```
media: 31 file(s), 31 distinct, 127.0 KB (31 drawn, 0 generated);
       the offer a join carries is 1679 bytes
```

**1679 byte per join**, sempre, cache calda o fredda: è il prezzo fisso di sapere
cosa c'è di là. I 127 KB si pagano una volta per macchina, non una per mondo e
non una per sessione.

Sul filo un file da 4128 byte costa 4165 byte: 37 byte di intestazione, cioè
l'hash più la lunghezza più il tipo. Un'offerta di tre file è 162 byte, 54 per
file.

### 17.9 Il client non legge più la propria cartella per un mondo altrui

`world_media()` per una sessione legge **solo** i media di quella sessione. Niente
ripiego su `assets/textures`, di proposito: un client che non trovasse la pietra
del mondo di là e disegnasse la propria mostrerebbe l'immagine del *game*
sbagliato, e sembrerebbe perfettamente a posto. Il segnaposto generato dal
renderer almeno dice che non è arrivato niente.

Misurato invece che argomentato — con la cartella delle texture del client
**svuotata** (`--no-textures`), contro due server con mod diverse:

```
media: 36 offer(s) across 2 world(s) name 31 distinct file(s); 31 were fetched
the world through the opening: 26 block(s) this build has never heard of;
its atlas is 31 from its own media, 0 generated
atlas: 5 block(s) from media this world sent, 0 from a generated placeholder
```

Nessun file locale, entrambi i mondi disegnati per intero.

### 17.10 Limiti dichiarati

**Niente cancellazione dalla cache.** ~~Cresce e basta.~~ Risolto in §18: budget
di 256 MB, sfratto LRU, e i numeri che dicono cosa costa.

**Il set di contenuti si costruisce una volta all'avvio.** Un reload di mod che
cambiasse i blocchi dovrebbe costruirne uno nuovo e scambiarlo fra due tick. Le
sessioni tengono un puntatore proprio perché oggi non cambia.

**Nessuna compressione sul trasporto.** I PNG generati sono deflate STORED
(§16.2) e viaggiano così. Un file vero esce compresso dal suo encoder; questi no,
e i 127 KB sono in gran parte quello.

**Il gate sul rebuild dell'atlante è debole.** `media_atlas_rebuilds > 0` è quasi
sempre vero: il rebuild scatta una volta per connessione, ci fosse o no qualcosa
che mancava. Sta lì per il suo sabotaggio — tolto il rebuild, un mondo i cui
media arrivano dopo l'atlante è un mondo disegnato tutto a segnaposto, e nient'
altro in quella run lo direbbe.

**Un hash per file, non per faccia né per variante.** Come §16.6: quando arriverà
l'arte per faccia saranno più file con lo stesso trasporto.

**L'orologio non c'entra e la revoca non esiste.** Un file nella cache ci resta
per sempre e non ha scadenza. È corretto per contenuto indirizzato — l'hash *è*
la versione — ma vuol dire che una texture sostituita sul server è un file nuovo,
e quello vecchio resta sul disco del giocatore.

---

## 18. La cache ha una dimensione, e ci resta sotto

§17.10 diceva: «cresce e basta». Una cache condivisa fra tutti i mondi in cui un
giocatore abbia mai guardato è una cartella che sale e non scende, e il giorno in
cui diventa un problema è fra mesi sul disco di qualcun altro.

### 18.1 Il numero, misurato prima di sceglierlo

Un mondo di questo engine è 31 file e 127 KB di segnaposto generati. Il default
di **256 MB** è circa quattromila mondi così. L'arte vera esce da un encoder vero
ed è più piccola per file, ma ce n'è di più: un *game* pesantemente moddato con
una texture per faccia sta su qualche megabyte, e lo stesso numero diventa circa
centocinquanta mondi. È la storia di un giocatore, non di una sessione, ed è la
scala per cui serve.

`--media-cache-size <MB>` sul client.

**Oggi non morde**, e questo va detto: una run reale finisce con `31 file(s), 0.1
MB of 256 MB`. Il budget non c'è perché serve adesso, c'è perché una cache senza
limite è un difetto che si manifesta quando non lo sta guardando nessuno.

### 18.2 «Vecchio» vuol dire non usato, non scritto tanto tempo fa

Lo sfratto va per **ultimo uso**, e la differenza non è accademica: una texture
che ogni mondo manda è esattamente il file che questa cache esiste per tenere, ed
è anche il file arrivato per primo. Un FIFO butterebbe via proprio quelli.

L'uso deve arrivare al disco, e il modo ovvio — toccare il file a ogni lettura —
trasforma ogni lettura della cache in una scrittura. `kMediaTouchInterval` è
un'ora: molto più fine di quanto allo sfratto interessi, e al massimo una
scrittura per file all'ora.

Sabotaggi, ognuno rosso solo sulle sue righe:

| sabotaggio | cosa diventa rosso |
|---|---|
| ordina dal più recente | «il file più vecchio, perché qualcuno l'ha letto, è ancora lì» + altre 2 |
| non aggiorna mai il timestamp all'uso | «smette di essere il più vecchio» + 1 |
| nessuno sfratto | «scende al suo budget entrando» + 2 |
| sfratta anche ciò che questa sessione usa | «va sopra il budget invece di sfrattare ciò che serve adesso» |

### 18.3 Un mondo più grande dell'intera cache

Il caso che rende una cache un ciclo di download: se un mondo ha più media del
budget, sfrattare i propri file mentre arrivano vuol dire riscaricarli per sempre.

I file che **questa sessione** ha scritto o letto non si sfrattano mai. Quando
questo lascia la cache sopra il budget, la cache lo **dice** (`over_budget()`)
invece di fingere:

```
a world of 6 file(s) into a 32 KB cache: 6 held, 10 evicted, over budget yes
```

Misurato anche sul client vero, con la cache riempita di spazzatura e un budget da
1 MB: 931 file e 3.7 MB all'apertura, scesi a 254 file e 1.0 MB, poi i 31 file del
mondo arrivano lo stesso e l'atlante è completo.

```
media: cache at ... -- 254 file(s), 1.0 MB of 1 MB, counted in 4.17 ms
       (over budget on arrival, oldest thrown away)
atlas: 31 block(s) from media this world sent, 0 from a generated placeholder
```

### 18.4 Cosa costa contare

La dimensione si conta **una volta** all'apertura e poi si mantiene: uno store non
cammina la cartella, e un mondo che arriva non la cammina una volta per file. Il
fan-out è di due cifre esadecimali, quindi sono 256 sottocartelle fisse comunque
vada.

Misurato su una cache piena al budget di default:

```
60000 file(s), 236.2 MB
scan: 181.3 ms
```

**181 ms all'avvio**, una volta per esecuzione del client, sulla cache più grande
che il default permetta. Oggi con 31 file sono 0.05 ms.

E quando c'è da sfrattare davvero — budget sceso da 236 MB a 128 MB, 27 487 file
da buttare:

```
open with a budget of 128 MB: 32513 file(s), 128.0 MB left, 27487 evicted
(108.2 MB), scan 177.2 ms, whole open 956.0 ms
```

Quasi un secondo, una volta, quando qualcuno stringe il budget. All'apertura dopo:
97 ms e zero sfratti.

L'insieme dei file protetti è **ordinato e cercato per bisezione**, non scorso:
con trentuno file non si nota, ma lo sfratto cammina ogni file della cache e
chiede di ognuno, e una lista lineare trasformerebbe «cache grande incontra mondo
grande» in decine di milioni di confronti per niente.

### 18.5 Limiti dichiarati

**Lo sfratto scatta sullo store, non su una soglia di tempo.** Una sessione che
legge soltanto non fa mai scendere una cache già sopra il budget finché non
riapre. Va bene perché aprire è ciò che fa un client all'avvio, ma non è una
garanzia continua.

**Il timestamp è quello del filesystem.** Su un filesystem montato senza scritture
di mtime, o con l'orologio spostato all'indietro, l'ordine LRU si degrada in
qualcosa di arbitrario — resta corretto (il budget si rispetta comunque), diventa
solo stupido su quale file butta.

**Nessun blocco fra processi.** Due client dello stesso giocatore che sfrattano la
stessa cartella insieme possono contarsi addosso: le dimensioni in memoria
divergono da quelle su disco fino alla riapertura. Nessun file viene corrotto — le
scritture passano da un rename atomico (§17.6) — ma il budget può essere superato
finché uno dei due non riapre.

**I 256 MB non sono stati validati su una cache vera di arte vera**, perché non
esiste ancora arte vera. Il numero deriva da una stima dichiarata in §18.1, non da
una misura di uso reale.

---

## 19. La bocca sa cosa c'è di là, e lo dice prima

§13 aggiungeva all'heartbeat la metà che non parla di liveness: quanti posti ha
quel mondo, e se ci si può ancora entrare. Il client la **stampava** e non ci
faceva niente. Il commento accanto diceva già qual era il difetto:

> «è la differenza fra un giocatore che scopre adesso che il mondo di là è pieno
> e uno che lo scopre stando nell'apertura con un rifiuto in faccia.»

### 19.1 Il difetto, misurato

Server di là avviato con `--max-players 0`: una finestra ci guarda dentro,
nessuno ci può camminare dentro. Client con il controllo **tolto**:

```
the world through the opening refused us after 0.4 ms: this world is full of
players (server is full)          x4
walked into a mouth 4 time(s): 4 crossing(s) asked for, 0 held back
```

Quattro attraversamenti chiesti, quattro rifiutati. Lo 0.3–0.4 ms è loopback: su
un collegamento vero è un **round trip**, con il giocatore già nell'apertura.

Con il controllo:

```
the world through the opening: 0/0 players, 1/8 windows, open to portals,
                               no room to walk in
did not walk into the opening: 127.0.0.1:25492 says 0/0 players, no room  x5
walked into a mouth 5 time(s): 0 crossing(s) asked for, 5 held back before
                               anything went out
```

Il gate non dice «il rifiuto è stato gestito bene». Dice che **non c'è stato
niente da rifiutare, perché non è partito niente**.

### 19.2 Nessun heartbeat è un sì

La regola sta su `ClientSession::world_has_room()`, dove un test senza finestra
la può raggiungere, e non nel client grafico:

```cpp
bool world_has_room() const { return m_beats == 0 || m_beat.enterable(); }
```

Un mondo che non manda heartbeat non è un mondo pieno: è un mondo che **questa
cosa non la fa**. È la stessa regola di compatibilità che impedisce al timer di
silenzio di dichiarare morto ogni server più vecchio. Al contrario, renderebbe
impassabile ogni bocca su qualunque cosa costruita prima degli heartbeat — e
niente in quel comportamento somiglierebbe a un bug.

Sabotata (`m_beats > 0 && ...`), diventano rosse due righe: quella prima del
primo battito e quella della riconnessione.

### 19.3 Guardare e entrare sono due porte

`kWorldWatchable` e `kWorldEnterable` sono flag distinti e restano distinti: il
mondo del test ha zero posti **e** una finestra aperta, e il gate controlla
entrambe le risposte perché una funzione che dicesse sempre di no le passerebbe
tutte e due se fossero una sola.

```
it says 0/0 players, 1/4 windows, open to portals, no room to walk in
```

`world_takes_windows()` esiste e **non viene ancora usato per decidere niente** —
una finestra già aperta resta aperta. È lì perché una bocca su un mondo che si è
chiuso ai portali è una cosa che una persona deve poter vedere invece di dedurla
da una riconnessione rifiutata.

### 19.4 Una riconnessione riparte da sì

Stessa regola del reset dell'heartbeat in `connect()`, controllata dove **decide**
qualcosa e non dove viene stampata: un client che si riconnette a un mondo diverso
non deve portarsi dietro il «pieno» del mondo precedente.

### 19.5 Limiti dichiarati

**Senza finestra non si sa niente.** Con `--no-passive` non c'è sessione passiva,
quindi non c'è heartbeat, quindi la bocca è passabile per definizione: il client
segue il redirect, si riconnette e viene rifiutato. È il percorso di §14 e resta
com'era. L'unico che potrebbe saperlo prima è il server di qua, che dovrebbe
tenere una sessione verso il vicino solo per questo — un costo per un caso che il
fallback già gestisce, male ma correttamente.

**Non si vede.** La bocca non cambia aspetto: si continua a guardare dentro un
mondo in cui non si può entrare e l'unico segnale è una riga stampata. Servirebbe
una tinta sull'apertura, cioè un uniform nuovo sullo shader del portale, e un
gate su uno screenshot; è lavoro di rendering con un controllo debole, e sta qui
come limite dichiarato invece che fatto a metà.

**Nessuna coda.** Un mondo pieno resta pieno finché qualcuno non esce; il client
riprova a ogni approccio del giocatore e basta. Un posto prenotato è
`ChangeScope` con una risposta «aspetta», e non esiste.

**Il trigger scatta comunque.** `portal_step_trigger` avanza la sua macchina a
stati anche quando l'attraversamento viene trattenuto, quindi il giocatore deve
uscire e rientrare nell'apertura perché riscatti. Nella run è successo cinque
volte in venti secondi, che è il comportamento voluto — ma è una conseguenza,
non una scelta scritta.

---

## 20. Un link che non è loopback

L'exit test di M6 dice **«due server su macchine diverse»**. Ogni numero di
questo milestone è stato misurato su `127.0.0.1`, dove il round trip è qualche
decimo di millisecondo e non si perde mai niente — e le costanti scelte contro
quei numeri (timeout di attraversamento 2 s, silenzio heartbeat 5 s, 64 file
media per richiesta) non hanno mai incontrato un collegamento che si comporti da
collegamento. `tests/soak.sh` lo dice in un commento da mesi.

`tools/netshape` è un relay UDP che sta in mezzo e si comporta male apposta.

### 20.1 Sotto ENet, non dentro

La scorciatoia ovvia è scartare messaggi in `Transport::send`, ed è **sbagliata**:
il canale affidabile di ENet ritrasmette ciò che la rete perde, quindi un
messaggio scartato prima che ENet lo veda è un messaggio che non torna più.
Scartare **datagrammi sul filo** è l'unico posto dove «perso» vuol dire quello
che vuol dire sul campo: ritrasmissione, stima dell'RTT e finestra di congestione
si esercitano tutti, perché nessuno dei tre distingue questo da un link vero.

### 20.2 Un processo, non un'interfaccia

`tc qdisc ... netem` è lo strumento vero e riconfigura un'interfaccia per **ogni
processo della macchina**. Questo è un processo: parte, deforma, stampa cosa ha
fatto, esce. Nessun root, niente da rimettere a posto.

Cosa **non** emula, detto qui invece di scoprirlo: MTU e frammentazione (i
datagrammi passano interi), cambi di percorso, limiti di banda e la coda che ne
deriva, e qualunque asimmetria fra le due direzioni oltre a quella che gli stessi
parametri producono.

### 20.3 Riporta quello che ha applicato, non quello che gli è stato chiesto

E per questo ha trovato il **proprio** difetto alla prima esecuzione. Chiesto 60
ms ± 20 e 2% di perdita, ha misurato:

```
delay 40.9/50.4/60.3 ms min/mean/max;  10 dropped (3.9%)
```

Media 50 invece di 60, massimo 60 invece di 80, perdita quasi doppia. Il
generatore prendeva `s >> 33` — **31 bit**, non 32 — diviso 2³², quindi `unit()`
stava in [0, 0.5): il jitter era sempre negativo e la probabilità di perdita
raddoppiata. Con `s >> 32`:

```
delay 41.0/60.5/80.6 ms min/mean/max;  3 dropped (1.3%)
```

Un deformatore che riportasse le proprie *intenzioni* non avrebbe mai mostrato
niente di tutto questo.

### 20.4 Il difetto che ha trovato al primo colpo

Client con i 31 file **già in cache**, contro un link a 120 ms:

```
media: world 0 offered 31 file(s), 31 already here, 0 arrived (0.0 KB)
atlas: 0 block(s) from media this world sent, 31 from a generated placeholder
```

Tutto in cache, niente da scaricare, e l'atlante **vuoto di media**.

`media_complete()` era `nessuna richiesta in volo && niente da chiedere`. Prima
che l'offerta arrivi, **l'offerta è vuota**: niente in volo, niente da chiedere,
quindi «completo» dice sì. Il rebuild one-shot scattava contro zero media, e i
file che atterravano 60 ms dopo non raggiungevano mai l'atlante.

Su loopback l'offerta batteva sempre il renderer e il difetto non esisteva.

**Il gate era verde per il motivo sbagliato**, e vale la pena guardarlo:

```cpp
check(media_atlas_rebuilds > 0 || had == offered,   // ← la scappatoia
      "and the atlas was rebuilt when they landed, unless they were all already here");
```

`had == offered` era vero — erano davvero tutti già lì — quindi il controllo
passava mentre l'atlante non ne conteneva nessuno. La clausola diceva «se erano
già qui non serve ricostruire», che è falso: *essere in cache* e *essere
nell'atlante* sono due cose diverse.

**Correzione:** `ClientSession::media_version()`, incrementata quando arriva
l'offerta e per ogni file che atterra. Il client ricostruisce quando è completo
**e** la versione è cambiata rispetto a quella con cui l'atlante è stato
costruito. E il gate perde la scappatoia: `media_atlas_rebuilds > 0`, punto.

Stesso link, dopo:

```
media: world 0 offered 31 file(s), 0 already here, 31 arrived (127.0 KB)
atlas: 31 block(s) from media this world sent, 0 from a generated placeholder
network client passed
```

### 20.5 E una traccia su OPEN-6

Il client si pianta ogni tanto e non si sapeva da cosa dipendesse. Attraverso il
deformatore a 60 ms: **2 blocchi su 5 esecuzioni**, contro circa 1 su 50 su
loopback. Il deformatore registrava ~2 datagrammi al secondo da un client che ne
manda venti.

Sotto `gdb` non succede: sei esecuzioni della stessa scena, sei finite. Attaccarsi
dopo è vietato da `ptrace_scope`, esserci dall'inizio lo fa sparire. Registrato in
`TODO.md` OPEN-6 con quello che è stato **escluso** — non è contesa fra i test
grafici, non è la cache dei media — perché la prima diagnosi era sbagliata e
l'unica cosa peggiore di nessuna diagnosi è una sbagliata scritta con sicurezza.

### 20.6 Limiti dichiarati

**Non sono due macchine.** È lo stesso kernel, la stessa scheda di loopback e lo
stesso orologio. Manca tutto ciò che dipende dal percorso: MTU, frammentazione,
NAT, banda finita.

**Le due direzioni hanno gli stessi parametri.** Un ADSL reale non è simmetrico.

**La perdita è indipendente per datagramma.** Le perdite vere arrivano a raffica.
Un modello a due stati (Gilbert-Elliott) sarebbe più onesto e non c'è.

**Nessun gate lo usa ancora.** Il deformatore è uno strumento eseguito a mano;
niente in `ctest` gira attraverso di lui. È il passo successivo, e ha senso farlo
solo dopo OPEN-6 — un gate che si pianta due volte su cinque non è un gate.

---

## 21. Le costanti erano scelte su loopback

§20 ha costruito il deformatore e ha trovato un difetto con la prima esecuzione.
Questo è cosa ha trovato quando gli si è chiesto di reggere l'exit test intero di
M6 a **400 ms di round trip, 60 ms di jitter, 10% di perdita** — e tutte e
quattro le cose sono la stessa cosa: un numero scelto dove il round trip è tre
decimi di millisecondo e non si perde mai niente.

### 21.1 Correzioni di velocità su un giocatore fermo

```
FAIL  standing still was never corrected
left after 40.0s, ..., 10 corrections applied

corrected to 57.48 20.00 57.09: moved 0.416524 blocks sideways in 0.007002s,
                                the limit is 0.406839
```

Zero quattro blocchi in **sette millisecondi**. Il controllo divideva la distanza
di una richiesta per il tempo dall'**arrivo della precedente** — un numero che
decide la rete, non il giocatore. Con il jitter due richieste spedite a 50 ms di
distanza arrivano a 7 ms quando la prima è stata trattenuta più della seconda:
la distanza è quella vera, il tempo è quello che la rete ha compresso.

**Correzione: un budget.** Si riempie alla velocità consentita sul tempo reale,
ha un tetto, e ogni richiesta ci attinge. Quello che resta limitato è **distanza
su tempo** — la proprietà per cui il controllo esiste — invece di distanza su un
intervallo che il client non controlla. Il jitter adesso spende una riserva che
la richiesta arrivata in ritardo aveva costruito.

Il tetto è **identico a quello di prima** (`max_burst = max_interval = 1 s`), di
proposito: questa modifica cambia *una* cosa sola, su cosa si misura l'indennità.
Stringere l'anti-cheat è una decisione separata che vorrebbe le sue prove — e
mezzo secondo è stato provato per primo: rifiuta la sonda di soak, che si muove a
passi di dodici blocchi a una velocità media perfettamente legale.

Sabotaggio (torna al rateo per richiesta, stessa scena, cache fredda): le
correzioni tornano, identiche — `0.801411 blocks sideways in 0.004070s`.

### 21.2 I media dietro l'heartbeat

```
cache calda, stesso link:  40 s, 0 correzioni, verde
cache fredda, 127 KB:      morto dopo 5.1 s -- "the server stopped answering"
```

Il mondo rispondeva. Le sue risposte erano dietro una texture: ENet ordina i
messaggi affidabili **dentro un canale**, e i media stavano sul canale eventi
insieme all'heartbeat.

Il primo tentativo di dimostrarlo è **fallito**: rimesso i media sul canale
eventi, quattro esecuzioni su quattro sono sopravvissute. Quella morte non si è
più riprodotta. Quindi invece di discuterne si è misurato il **gap massimo fra
due heartbeat**, tre esecuzioni per parte:

| canale dei media | gap massimo |
|---|---|
| proprio (`Channel::Media`) | 1.76 / 2.45 / 1.70 s |
| eventi | 2.77 / 2.64 / 2.71 s |

Circa **0.75 s in più**, sistematici. Contro una finestra da 5 s è la differenza
fra 1.8× e 2.5× di margine. Il canale separato resta, e resta per questo numero e
non per il ragionamento che lo aveva proposto.

Costa una versione di protocollo: `kProtocol` 4, `ModBase` da 4 a 5.

### 21.3 Cinque secondi non sono cinque intervalli

Anche col canale separato, a **20-30% di perdita** il client dichiarava morto un
mondo vivo. `kHeartbeatSilence = 5.0` era «cinque intervalli», scelto dove un
intervallo costa un decimo di millisecondo.

Un heartbeat è **affidabile**: quando si perde viene ritrasmesso dopo un timeout
che parte dal round trip e **raddoppia**. Quattro ritrasmissioni sono circa
quindici round trip.

```
finestra = max(5 s, 25 x round trip)
```

Su loopback vince il pavimento e nessun numero cambia (`round trip 1 ms, so the
silence window is 5.0 s`). A 400 ms diventa **10.2 s**.

Misurato al variare della perdita, 40 s per esecuzione:

| perdita | gap massimo | finestra | esito |
|---|---|---|---|
| 10% | 2.82 / 1.71 s | 10.5 / 10.2 s | passa |
| 20% | 5.72 / 8.43 s | 10.0 / 10.2 s | dentro la finestra |
| 30% | oltre | 8-9 s | dichiara morto |

I 5 s originali avevano **1.8× di margine** sul caso peggiore misurato al 10%.
Non è margine, è fortuna.

### 21.4 Due finestre che chiedono lo stesso file insieme

```
media: 62 offer(s) across 2 world(s) name 31 distinct file(s);
       51 were fetched, 11 were already on disk
FAIL  and nothing was downloaded twice, however many worlds offered it
```

«Due server che spediscono la stessa texture la scaricano una volta sola» valeva
solo finché i due trasferimenti **non si sovrapponevano**. Su loopback i file
della prima finestra atterravano prima che arrivasse l'offerta della seconda; a
400 ms si sovrappongono, e la seconda sessione chiede venti file che la prima sta
già scaricando.

La cache adesso sa anche cosa è **in volo**: `claim()` risponde di sì a una sola
sessione per hash, le altre aspettano e lo trovano su disco. Una prenotazione
scade dopo 20 s, così una sessione che chiede e muore non rende un file
irrecuperabile.

E la sessione che ha aspettato deve accorgersi: niente glielo dice, quindi
ricontrolla la cache quattro volte al secondo finché ha qualcosa che non ha né
ottenuto né chiesto.

Stessa scena, dopo: **31 scaricati su 31 distinti**.

### 21.5 Cosa regge adesso, e cosa no

A **400 ms di round trip, 60 ms di jitter, 10% di perdita**, cache fredda:

```
left after 40.0s, 185565 frames, 32 chunks, 0 corrections applied
came out at 61.10 20.00 61.08 in the other world
media: 62 offer(s) ... 31 distinct ... 31 fetched, 31 already on disk, 0 refused
atlas: 31 block(s) from media this world sent, 0 from a generated placeholder
network client passed
```

**Al 20% non regge**, e i due fallimenti sono la stessa classe di difetto di
tutto questo capitolo:

- `a refused crossing is answered, not waited out` — il gate aspetta **2 s** la
  risposta a un `ChangeScope`. A 400 ms con il 20% di perdita quel round trip
  può superarli. È il numero che avevo previsto sarebbe caduto per primo.
- `a pair that leads away opens a session to the world it leads to` — la sessione
  passiva verso il mondo di là non è salita entro il tempo che il gate concede.

Sono soglie di **gate**, non comportamento dell'engine, ed è per questo che non
sono state toccate in questo giro: cambiare un gate mentre si sistema il codice
che misura è il modo di ritrovarsi con entrambi sbagliati. Vanno derivate dal
round trip misurato, come §21.3, ed è il pezzo successivo.

### 21.6 Limiti dichiarati

**`kHeartbeatSilenceRtts = 25` non è misurato, è derivato.** Viene dalla somma
della sequenza di ritrasmissione (quattro tentativi ≈ quindici round trip) più
margine. Al 10% funziona; al 30% no, e al 30% non funziona niente.

**Il round trip viene da ENet.** È la sua stima, non una misura indipendente, e
su un link che sta collassando la stima peggiora insieme al link — il che è la
direzione giusta ma non è una garanzia.

**Il tetto del budget resta un secondo di viaggio.** Un cheat può ancora
accumulare un secondo di silenzio e spenderlo in un salto. Era vero prima e resta
vero: qui non si è stretto niente, si è solo smesso di punire chi è onesto.

## 22. Il segnale di vita non deve stare in coda

§21.5 lasciava due soglie da derivare dal round trip misurato. Derivarle ha
richiesto prima di capire *perché* cadevano, e la risposta non era la soglia.

### 22.1 Un battito affidabile è un battito in ritardo

Il gate stampava, run dopo run al 20% di perdita:

```
heartbeats: longest gap 9.17 s across 2 session(s), 0 lost
```

Nove secondi fra due messaggi spediti a un secondo di distanza, **e zero persi**.
I due numeri insieme non descrivono una rete che perde pacchetti: descrivono una
coda. L'heartbeat era `reliable` su `Channel::Event`; ENet ritrasmette un
messaggio affidabile con un timeout che parte dal round trip e **raddoppia**, e
finché quello non arriva tutti i battiti dietro di lui aspettano in ordine.

E `heartbeats_lost` non poteva muoversi: un canale affidabile non lascia buchi
nella sequenza. Era un contatore che valeva zero per costruzione — non una
misura, una tautologia.

Un battito dice cosa è vero **adesso**. Uno ritrasmesso dice cosa era vero due
secondi fa, e nel frattempo ha tenuto fermo quello che dice il presente. Adesso
è non sequenziato: un battito perso costa un secondo, e il client lo vede
mancare.

Stessa scena, stesso shaper, quattro run per configurazione a 400 ms:

| battito | gap massimo | persi |
|---|---|---|
| affidabile | 3.52 / 9.17 / 7.07 / 4.30 s | 0 / 0 / 0 / 0 |
| non sequenziato | 3.07 / 4.16 / 3.14 / 3.10 s | 19 / 21 / 26 / 20 |

Il 9.17 s è quello che contava: la finestra di silenzio a 400 ms vale 10.1 s, e
in una run su quattro il client ha dichiarato morto un mondo vivo.

Il client scarta un battito con `seq` non più recente di quello che ha già: senza
ordine sul filo, uno vecchio può arrivare dopo uno nuovo, e applicarlo
sposterebbe l'orologio all'indietro.

**E il numero era anche contato male.** Con `lost` sempre zero non si vedeva:
il report partiva da `conn->heartbeats_lost()` e poi sommava *tutte* le sessioni,
`conn` compresa. Al 10% dava 16 persi su ~76 battiti — il 21% su un link che ne
perde il 10. Corretto: 4 / 7 / 9, che è il 10% dello shaper.

### 22.2 L'attesa di un attraversamento non è un numero

Il gate `a refused crossing is answered, not waited out` cadeva perché il client
aspettava **2 s** una risposta a `ChangeScope`. Primo tentativo: derivarla dal
round trip, `max(2 s, 8 × rtt)` — 3.2 s a 400 ms. **Due run su sei cadevano
ancora.** Otto round trip erano un altro numero scelto a mano, solo più grande.

Un `ChangeScope` è affidabile e la sua risposta pure. L'unica cosa che può
impedirle di arrivare è che la sessione finisca — e quale sia il momento in cui
una sessione è finita lo decide già la finestra di silenzio. Quindi l'attesa
*è* la finestra di silenzio, e non c'è una costante nuova da scegliere:

```cpp
double ClientSession::crossing_window() const
{
    const double w = heartbeat_window();
    return w > kCrossingAnswer ? w : kCrossingAnswer;
}
```

Rinunciare prima è peggio che aspettare: il server di là può aver detto sì e
aver già spostato il giocatore, lasciando questo client in un mondo in cui quel
server non lo tiene più. Zero fallimenti in sedici run da quando l'attesa è
quella della sessione.

E se scade lo stesso, la risposta in ritardo viene **buttata**
(`discard_teleport`), non tenuta per il tentativo successivo: arrivare usando la
posizione decisa per un'altra camminata nella bocca è un difetto peggiore di
quello che risolve.

**Sabotaggio.** Server che non risponde mai a un `ChangeScope`:

```
the world through the opening did not answer in 10.4 s (round trip 414 ms); staying here
FAIL  a refused crossing is answered, not waited out
```

Il gate lo prende ancora.

### 22.3 Anche entrare era un numero di loopback

Con le altre due sistemate ne è uscita una terza: una run su sei al 20% non
entrava affatto.

```
could not join: timed out -- no answer in 10 seconds
```

Un join è quattro scambi affidabili in fila — la connessione di ENet, l'offerta
e l'accettazione della chiave, l'hello e il welcome — e ognuno può richiedere una
ritrasmissione. Dieci secondi erano dodici round trip a 400 ms.

`handshake_window() = max(10 s, 40 × rtt)`, e il client stampa quanto è costato
davvero:

```
took 2.4 s of the 16.4 s allowed (round trip 410 ms)
```

Otto run al 20%: da 1.2 a 3.9 s. Nessuna caduta da allora. Il round trip non si
conosce finché la connessione ENet non è su, quindi all'inizio vale il pavimento
e basta — che è il verso giusto: la parte senza misura è la parte che un numero
fisso protegge ancora.

### 22.4 «Non è stato scaricato due volte» era una percentuale

Il gate diceva: dopo l'attraversamento non devono arrivare più di un quarto dei
chunk che c'erano prima. Al 20% cadeva su questo:

```
that world had 23 chunks before the crossing and 32 after
```

Ventitré su trentadue non è un secondo streaming: è il primo che stava ancora
finendo, perché su un link che perde pacchetti il giocatore arriva alla bocca
prima che la finestra abbia finito di riempirsi. La percentuale non sa
distinguere le due cose.

Cosa vuol dire davvero «streamato una seconda volta»: **un chunk che quel mondo
aveva già arriva di nuovo**. Che è una cosa che la sessione può contare esatta.

```
that world had 32 chunks before the crossing and 32 after, 0 of which it already had
```

**Sabotaggio.** Server che dimentica cosa ha già mandato quando la sessione passa
a `Enter`:

```
32 chunks before the crossing and 64 after, 32 of which it already had
FAIL  and the world arrived in was not streamed a second time
```

### 22.5 Dove siamo

A **400 ms, 60 ms di jitter, 10% di perdita**, cache fredda, sette run: **7 su 7
verdi**. Join 1.2–3.0 s, gap massimo fra battiti 2.1–3.1 s contro una finestra di
10 s, 31 file su 31 distinti, 0 chunk ripetuti.

Al **20%**, otto run: **7 su 8**. Su loopback niente è cambiato: join 0.0 s su
10 concessi, gap 1.05 s su 5, 0 battiti persi, 0 chunk ripetuti, 25 test su 25.

E la regola del backoff regge: ucciso il server di là prima
dell'attraversamento, in quaranta secondi il client scrive **due righe**, a 2 s e
a 4 s, e prende il redirect.

### 22.6 Cosa resta rotto, con i numeri

Quattro cose viste al 20%, nessuna toccata qui.

**Il claim di un osservatore diventa il claim di un corpo.** Due run su circa
trenta, dopo un attraversamento:

```
came out at 61.10 20.00 61.07 in the other world
corrected to 60.50 21.00 60.50: that position is inside a solid block   (× molte)
```

`60.50 21.00 60.50` è il centro dell'apertura di quel mondo — esattamente quello
che il client manda come punto di vista di un osservatore. Una `Move` spedita
mentre quella sessione era una finestra arriva dopo che il server l'ha fatta
diventare un corpo, e il server la controlla come una posizione. Sul filo le due
cose sono lo stesso messaggio e il `seq` non aiuta: quel claim è più *nuovo* di
tutti gli altri, non più vecchio. Servirà distinguerle nel protocollo, ed è un
bump.

**Si attraversa una bocca dietro cui non c'è ancora niente.** Una run su otto:
`0 chunks before the crossing and 32 after`. Il mondo dietro l'apertura non aveva
fatto in tempo a streamare, e il giocatore ci è entrato lo stesso: quella è una
schermata di carico, per quanto breve. Il client ha già `world_has_room()` per
trattenerlo quando di là è pieno; «di là non c'è ancora niente» è lo stesso tipo
di risposta e nessuno la legge.

**ENet chiude da solo.** Due run su circa quattordici: `disconnected: closed`
senza che nessuno abbia deciso niente. È il timeout del peer di ENet con i suoi
valori di default (5 s minimi, limite 32) su un link dove un tentativo su tre
fallisce in andata o in ritorno. Non è configurato da nessuna parte in questo
codice, e dovrebbe esserlo.

**Quaranta secondi non bastano per i media al 20%.** Una run su otto: 27 file su
31, atlante a 24 su 31 e sette segnaposto. Nessuno stallo — il resto stava
arrivando quando la run è finita. È il tempo del gate, non un difetto del
trasferimento, ma va detto perché il gate lo riporta come una mancanza.

## 23. La stessa richiesta voleva dire due cose

§22.6 lasciava quattro difetti misurati al 20% di perdita. Il primo era il
peggiore, e non era una soglia né un'attesa: era un messaggio che significava due
cose diverse a seconda di chi lo aveva mandato.

### 23.1 Il sintomo

Dopo un attraversamento, in due run su una trentina:

```
came out at 61.10 20.00 61.07 in the other world
corrected to 60.50 21.00 60.50: that position is inside a solid block
corrected to 60.50 21.00 60.50: that position is inside a solid block
...
corrected to 60.50 -1607.34 60.50: moved 49.401733 blocks vertically in 0.167381s
left after 40.0s, ..., 15 corrections applied
```

Quindici correzioni, e il giocatore a **y = -1607**. Il server ha rimesso il
giocatore al centro dell'apertura, il client lì sotto non ha niente, cade, e ogni
caduta produce un'altra correzione.

### 23.2 La causa

`60.50 21.00 60.50` è il centro dell'apertura di quel mondo — esattamente il
punto che il client manda come **punto di vista** della finestra che guarda di
là. DESIGN §4.4: i chunk di un osservatore stanno in una sfera attorno al punto
in cui l'apertura sbuca, e il client lo dichiara con una `Move`.

Ma una `Move` da un corpo vuol dire «sono qui» e il server la controlla contro il
mondo; da una finestra vuol dire «tienimi i chunk qui attorno» e non c'è niente
da controllare. Un attraversamento trasforma la prima cosa nell'altra, e con 400
ms di round trip **una richiesta spedita da finestra atterra dopo che il server
l'ha fatta corpo**. Il centro di un portale, letto come posizione, è dentro un
blocco solido.

Il `seq` non poteva prenderla: quella richiesta è la **più nuova**, non la più
vecchia. Il controllo di staleness che esisteva guarda nella direzione sbagliata.

E c'era anche la metà che il client faceva da solo: fra `send_enter` e l'arrivo —
un round trip — il client continuava a mandare il punto di vista a quella
sessione, che nel frattempo si considerava già un corpo. Non erano solo le
richieste in volo: ne produceva di nuove.

### 23.3 La correzione

La richiesta dice cosa credeva di essere quando è stata fatta, e il server la
applica solo se la sessione lo è ancora. Un byte in `Move`, protocollo **5**:

```cpp
if (move.scope != (uint8_t)s.scope) {
    ++s.stale_claims;
    return wrong_kind;      // contata, non controllata
}
```

Simmetrica: prende anche il verso opposto, la richiesta da corpo che arriva dopo
che la sessione è tornata finestra — il mondo lasciato dietro.

E il client smette di mandare punti di vista a una sessione che non è più
`Observe`. Il filo prende quelle già in volo, questo smette di produrne.

Contate e non loggate ogni volta: attorno a un attraversamento su un link lento
ne arriva una raffica, ed è normale. Prima e ogni centesima, come le correzioni.

### 23.4 I numeri

A 400 ms, 60 ms di jitter, **20% di perdita**, otto run: **8 su 8 verdi, zero
correzioni**. Prima erano 7 su 8, con due run su una trentina che finivano con il
giocatore sotto il mondo. Al 10%, sei run: 6 su 6.

E il meccanismo si vede scattare davvero — nei log dei server, in entrambi i
versi:

```
'player' sent a claim of the wrong kind: observer on a session that is player (1 so far); ignored
'player' sent a claim of the wrong kind: player on a session that is observer (1 so far); ignored
```

**Il gate.** Loopback non può produrre la corsa, quindi non è la corsa a essere
testata: è che il server si rifiuti di leggere un tipo di richiesta come
l'altro, che è la cosa che rende la corsa innocua. Un corpo che manda una
richiesta marcata «finestra» venti blocchi più in là — che come corpo sarebbe
troppo veloce e verrebbe corretta — non deve produrre né una correzione né uno
spostamento dell'autorità, e il passo successivo di mezzo blocco dalla posizione
**vera** deve ancora passare.

**Sabotaggio.** Tolto il confronto fra i due scope:

```
FAIL  a claim that says it came from a window is not checked as a body
FAIL  it is counted, so a burst of them around a crossing is visible rather than silent
FAIL  and it did not move the server's idea of where the player is
        0 claim(s) of the wrong kind ignored, 1 correction(s)
```

### 23.5 Il costo

`kMinProtocol` sale a 5 insieme a `kProtocol`: il campo è obbligatorio, quindi
una `Move` di un client versione 4 viene rifiutata come troncata. Non c'è una
finestra di compatibilità qui, ed è una scelta: un default silenzioso avrebbe
voluto dire che un client vecchio manda punti di vista che il server legge come
posizioni, cioè esattamente il difetto, solo senza modo di accorgersene.

## 24. La bocca aspetta il mondo che c'è dietro

Il secondo dei quattro difetti di §22.6. Una run su otto al 20% di perdita
attraversava così:

```
that world had 0 chunks before the crossing and 32 after
```

Zero. Il mondo dietro l'apertura non aveva streamato niente, il giocatore ci è
entrato lo stesso, e il mondo è arrivato dopo. È una schermata di carico, per
quanto breve — cioè esattamente la cosa che l'exit test di M6 dice di non avere.

### 24.1 Cosa aspettare, e cosa no

Non «il mondo di là è completo»: su un link lento non lo è mai al momento giusto,
e aspettarlo sarebbe una schermata di carico messa da un'altra parte.

Quello che serve è **il blocco in cui il giocatore uscirà**. Ed è la domanda
giusta anche per un'altra ragione: il punto di vista che il client dichiara per
quella finestra è proprio il punto d'arrivo — DESIGN §4.4 vuole i chunk di un
osservatore in una sfera attorno a lì — quindi il chunk dell'arrivo è **il primo
a partire**. Aspettarlo costa quasi niente quando la finestra sta streamando, e
costa tutto solo quando non sta streamando affatto, che è il caso da fermare.

Contato a parte dai rifiuti: un mondo pieno è una risposta, questa è un'attesa.

### 24.2 I numeri

Link vicino 200 ms / 5% di perdita, link **lontano** 400 ms / 30% — cioè una
finestra che fatica a riempirsi mentre il mondo in cui si cammina va bene.

| | run | attraversamenti senza il blocco d'arrivo |
|---|---|---|
| senza l'attesa | 6 | **2** (0 chunk e 2 chunk) |
| con l'attesa | 12 | **0** |

E l'attesa scatta davvero: in 4 run su 12 il client si è fermato sulla soglia, e
in 3 di quelle ha attraversato subito dopo con il blocco in mano.

```
waited at the opening: 127.0.0.1:25892 has 2 chunk(s) so far and none of them is the one to come out in
walked into a mouth 2 time(s): 1 crossing(s) asked for, 0 held back, 1 waited for the world of arrival
crossed once: ... that world had 22 chunks before the crossing and 32 after
waited 1 frame(s) at the opening for that block, from 10.4 s into the run
```

Nella dodicesima il mondo di là non è mai arrivato e il giocatore non è passato:
il gate lo dice — `and that world was meshed, so the opening is not a hole full
of sky` — che è la risposta giusta e non un fallimento nascosto.

Al 20% con lo scenario normale l'attesa non scatta mai: il client già aspettava
le aperture del mondo di là prima di camminare, e quando quelle arrivano di solito
sono arrivati anche i chunk. Il difetto vive nella finestra fra le due cose.

### 24.3 Cosa resta

Al 20%, sei run: **4 su 6**. I due fallimenti sono i due difetti ancora aperti di
§22.6, entrambi con la stessa forma di tutto questo capitolo:

- `36 were fetched` per **31 file distinti** — tutti sulla stessa sessione. È
  `kMediaRequestTimeout = 15 s`: al 20% un trasferimento può superarli, la
  sessione rinuncia e richiede, e poi arrivano tutti e due. Un'attesa fissa su
  una cosa che decide il link.
- `could not join: timed out -- no answer in 16 seconds` — `kHandshakeRtts = 40`
  non basta sempre. Due join su circa trenta al 20%.

Più OPEN-8, il timeout del peer di ENet, che non è di questo codice ma è l'unica
attesa che nessuno ha ancora scelto.

## 25. Un timeout non è una risposta

Il terzo dei quattro di §22.6: `36 were fetched` per **31 file distinti**, tutti
sulla stessa sessione. Sistemarlo ha richiesto tre passaggi, e i primi due non
bastavano — vale la pena scriverli tutti, perché la sequenza è la lezione.

### 25.1 Primo passaggio: l'orologio riparte

`kMediaRequestTimeout = 15 s` misurava **quanto può metterci un lotto intero**.
Ma quanto ci mette lo decide il link, non il client: al 20% di perdita un lotto
supera i quindici secondi, la sessione rinunciava a metà e richiedeva tutto,
e poi arrivavano tutte e due le copie.

Adesso l'orologio riparte a ogni file che atterra: quindici secondi di **silenzio
completo** sono uno stallo, quindici secondi di trasferimento no.

Al 20%, dieci run: **31 su 31 ogni volta**. Ma a 400 ms e 25% non bastava —
55 file scaricati per 31 distinti.

### 25.2 Secondo passaggio: il server risponde anche di no

Perché il timeout scattava ancora? Perché era ancora **l'unico modo che il client
aveva di sapere qualcosa**. Un server che non possiede un hash chiesto non
diceva niente — `++s.media_unknown; continue;` — quindi «non arriva» e «arriva
piano» erano indistinguibili, e l'unico strumento era un cronometro.

È lo stesso difetto di un attraversamento senza `ScopeDenied` dietro, e la stessa
cura: `MediaMissing`, protocollo **6**. Ogni hash chiesto ha una risposta.

Il timeout torna a essere quello che doveva essere: una rete di sicurezza per un
link che si è fermato. E soprattutto **non richiede più**: la prima copia è in
coda su un canale affidabile, e chiedere due volte è come uno stallo diventava
24 file duplicati.

A 400 ms e 25%, sei run: **due ancora rotte** (52 e 39 su 31).

### 25.3 Terzo passaggio: la prenotazione segue la sessione, non il trasferimento

Il numero che lo diceva è stato aggiunto apposta invece di indovinare:

```
0 claim(s) still held, 18 taken over from a session that looked dead
```

`kMediaClaimTimeout = 20 s` esisteva per una sessione che chiede e **muore**: un
processo sparito a metà download non deve rendere un file irrecuperabile. Non
esisteva per decidere che un trasferimento è troppo lento — e a 400 ms con il 25%
di perdita un lotto può passare venti secondi senza che atterri niente mentre
ENet lavora su una ritrasmissione. La prenotazione scadeva sotto una sessione che
stava benissimo, e l'altra finestra riscaricava gli stessi file.

Quindi la prenotazione segue la **sessione**: finché quella pompa ed è viva, le
sue prenotazioni sono vive. Una volta al secondo, perché dall'altra parte c'è un
timer da venti.

### 25.4 I numeri, passaggio per passaggio

Tutti a **400 ms, 60 ms di jitter, 25% di perdita**, cache fredda, 31 file
distinti offerti da due mondi:

| | run rotte | peggior caso |
|---|---|---|
| prima di tutto | 2 su 4 | 62 scaricati su 31 |
| orologio che riparte | 2 su 4 | 55 su 31 |
| \+ `MediaMissing` | 2 su 6 | 52 su 31 |
| \+ prenotazione rinnovata all'arrivo | 1 su 6 | 50 su 31 |
| \+ prenotazione legata alla sessione | **0 su 8** | 31 su 31 |

Al 20%, sei run dopo: 6 su 6. Su loopback: 25 test su 25, invariati.

**Sabotaggio.** Tolta la `MediaMissing` dal server, nel gate di loopback:

```
FAIL  and is told that this world does not hold it, rather than left to time out
```

### 25.5 Cosa resta

Al 25% restano le cadute di connessione — tre run su otto — e sono **OPEN-8**: il
timeout del peer di ENet, che questo codice non ha mai configurato, con i suoi
default (5 s minimi, limite 32, 30 s massimi). È l'unica attesa che nessuno ha
scelto, ed è la prossima.

## 26. L'ultima attesa era di qualcun altro

Il quarto e ultimo di §22.6: sessioni che cadevano senza che nessuno lo avesse
deciso.

```
disconnected: closed
left after 33.0s, 156821 frames, 32 chunks, 0 corrections applied
```

Tre run su otto a 400 ms e 25% di perdita. Nessun gap fra i battiti, nessuna
finestra di silenzio scaduta, nessuna decisione presa da questo codice.

### 26.1 Il numero che mancava

`closed` da solo è un rapporto senza il dato utile. Aggiunto:

```
disconnected: closed -- ENet gave up on the peer; last heard from 1255 ms ago, round trip 820 ms
```

**Un secondo e due decimi.** I dati passavano. Non è un link morto: è **un
singolo comando affidabile** rimasto in sospeso mentre il suo backoff
raddoppiava, e ENet che a un certo punto dichiara morto il peer — cinque secondi
minimi con il limite superato, o trenta secondi comunque. Sono i suoi default, e
nessuno in questo codice li aveva mai scelti.

### 26.2 La regola

ENet deve essere il limite **esterno**. Non può decidere prima del codice che sa
dire *perché*: la finestra di silenzio spiega, `closed` no.

`Transport::hold_peer(peer, secondi)` tiene aperta la pazienza di ENet oltre la
propria, e il client la chiama una volta al secondo con il doppio della sua
finestra — che cresce col round trip, quindi cresce anche questa. Mai sotto i
default: serve a impedirgli di mollare presto, non a stringere.

### 26.3 E il posto a sedere

Tenere aperto il timeout di ENet senza altro sarebbe stato scambiare una
sessione chiusa troppo presto con un posto occupato troppo a lungo: finora
l'unica cosa che liberava un posto era proprio quel timeout.

Quindi il server prende la sua metà della domanda, che non aveva: `last_heard_at`
su ogni sessione e `ServerSessions::silent()`. Un client che gioca manda una
posizione venti volte al secondo, quindi dodici secondi di silenzio sono duecento
quaranta claim mancati — non un client tranquillo, un client assente.

Un client ucciso con `kill -9`, senza saluto:

| | il posto torna dopo |
|---|---|
| senza la regola del server | **31.5 s** (decide ENet) |
| con la regola | **12.2 s** |

```
dropped 'player': nothing heard for 12 s
```

### 26.4 I numeri

A **400 ms, 60 ms di jitter, 25% di perdita**, otto run:

| | connessioni cadute senza decisione |
|---|---|
| prima | 3 su 8 |
| dopo | **0 su 8** |

L'unica sessione persa adesso lo dice da sé, ed è vero:
`the server stopped answering for 19 seconds (round trip 786 ms)`.

E allo scenario dell'exit test — 400 ms, 20% di perdita, cache fredda, due server
con game diversi — **otto run su otto verdi**. Su loopback: 25 test su 25.

### 26.5 Limiti dichiarati

**Dodici secondi di silenzio uccidono una sessione viva ma muta.** Il client di
questo engine manda sempre una posizione, quindi non capita; un client scritto
diversamente, che sta fermo e non parla, verrebbe buttato fuori. È una regola
nuova e va detta.

**Al 25% i media non finiscono sempre in sessanta secondi.** Una run su otto:
l'atlante del mondo di là era incompleto alla fine della run. Il trasferimento
stava andando, la run è finita prima.

**`kSessionSilence = 12 s` è scelto, non misurato.** Viene da «venti claim al
secondo, duecento e passa mancati», non da una distribuzione osservata di
silenzi. È il numero giusto per il caso a cui serve — un processo sparito — e non
è stato messo alla prova contro un client che ha solo una brutta giornata.

## 27. Il link cattivo entra nella suite

Sei commit di fila hanno trovato difetti che esistono solo sopra il loopback, e
**tutti e sei a mano**, con `tools/netshape` lanciato da una shell. In `ctest`
non c'era niente che ci passasse vicino: §20.6 lo diceva e restava vero.

`tests/bad_link.sh` è quello scenario, dentro la suite. Due server con game
diversi dietro due relay, cache dei media fredda, un attraversamento, 400 ms di
round trip, 60 ms di jitter, 20% di perdita.

### 27.1 Cosa asserisce, e cosa no

**Invarianti, non tempi.** La perdita è casuale: un gate che dice
«l'attraversamento è durato meno di due secondi» è un gate che lampeggia. Quello
che deve valere a qualunque perdita è che si esce dall'apertura giusta, che
niente viene scaricato due volte, che il blocco d'arrivo era già lì, che nessuno
viene corretto stando fermo. Sono i check che il client fa già da sé; questo li
fa girare sul link cattivo.

**E che lo scenario sia successo davvero.** Tutti i check del client
sull'attraversamento stanno dentro `if (crossed_once > 0)`, quindi una run in cui
il giocatore non passa non riporta **nessun** fallimento. Un gate che passa per
non essere successo è peggio di nessun gate, quindi il join, l'attraversamento e
il fatto che di là ci sia un game diverso sono asseriti a parte.

**E che il link fosse davvero cattivo.** Il relay stampa cosa ha *applicato*, e
la run controlla che qualcosa sia stato buttato via. Con `CNP_LINK_LOSS=0`:

```
FAIL  the relays dropped nothing on the way to the client: loopback with extra steps
```

### 27.2 Perché il 20% e non di più

Non è una taratura al ribasso: è il livello più alto a cui la build corretta è
**affidabilmente** verde. Diciotto run su diciotto.

Al 25% la build corretta fa 6 su 8, e i due fallimenti sono onesti — una finestra
di silenzio che scatta su un link che si è fermato davvero per 19 secondi, e i
media che non finiscono in sessanta. Un gate che fallisce su una verità è
comunque un gate che fallisce.

`CNP_LINK_LOSS`, `CNP_LINK_DELAY`, `CNP_LINK_JITTER` e `CNP_LINK_SECONDS` lo
spostano a mano.

### 27.3 Cosa prende davvero — e cosa no

Provato rimettendo i difetti uno alla volta, quattro run ciascuno:

| difetto rimesso | run rosse |
|---|---|
| heartbeat di nuovo affidabile (§22.1) | **2 su 4** |
| attesa dell'attraversamento a 2 s fissi (§22.2) | 0 su 4 |
| timeout del peer di ENet ai default (§26) | 0 su 4 |

Le ultime due righe sono il risultato più utile di tutto l'esercizio, e vanno
dette invece che nascoste.

**L'attesa di 2 s adesso basterebbe.** Cadeva quando l'heartbeat stava in coda e
ENet chiudeva sessioni vive; sistemate quelle, la risposta a un `ChangeScope`
arriva comodamente dentro i due secondi a 400 ms e 20%. Quindi `crossing_window`
in questo momento **non porta peso**: è un'assicurazione, non una correzione
attiva. Resta perché il ragionamento regge — una risposta affidabile arriva
finché la sessione vive, e nessun numero scelto a mano lo sa — ma nessuno creda
che sia questo gate a tenerla onesta.

**Il timeout di ENet cadeva al 25%, non al 20%**, ed è esattamente dove il gate
non arriva. Quel difetto resta coperto solo da una run a mano.

Un gate che dichiara cosa non prende è più utile di uno che lascia credere di
prendere tutto.

### 27.4 Costo

45 secondi di orologio, e non possono essere di meno: quello che misura è cosa
succede **nel tempo** su un link cattivo. Etichettato `gpu;slow` — serve una
finestra, e `ctest -LE slow` lo salta. La suite passa da 25 test a 26 e da 50 a
52 secondi in parallelo.

### 27.5 E la terza clausola dell'exit test

DESIGN §8 chiede tre cose a M6. Due avevano un gate; la terza — «una passiva
verso un server morto non spamma il log (regola del backoff)» — no: si
controllava ammazzando un server e leggendo cosa usciva.

`tests/dead_world.sh`. Due mondi, quello di là **senza posti a sedere** —
`--max-players 0`, così una finestra può guardarci dentro e nessuno può entrarci.
Serve a questo: su loopback il giocatore attraversa in due secondi, e uccidere il
mondo in cui sta è un'altra clausola. Poi quel mondo viene ucciso con `kill -9`,
non spento con garbo: un server che saluta è un caso diverso e più facile.

Cosa vale come «non spamma», in numeri: il client dice qualcosa finché l'attesa
**cresce**, una volta quando smette di crescere, e poi più niente.

```
2 line(s) about it in 26s, waits: 2 4
```

E la seconda metà della clausola, quella che un client potrebbe passare
arrendendosi del tutto: il mondo in cui il giocatore sta non se ne accorge.

**Sabotaggio.** Tolta la regola — il client dice la stessa cosa a ogni frame:

```
28108 line(s) about it in 26s
```

Ventottomila righe contro due. È il gate meno ambiguo di tutto il capitolo, ed è
anche l'unico che si poteva scrivere senza un link cattivo: la regola del backoff
non è una cosa che la perdita di pacchetti rivela, è una cosa che l'assenza di un
server rivela.

Suite: da 25 test a **27**, 52 secondi in parallelo.

## 28. Due server, due orologi

L'exit test di M6 dice «due server su **macchine diverse**». Tutto quello che
c'è qui gira su una macchina con `tools/netshape` in mezzo: il round trip è
simulato bene, la perdita anche, e **l'orologio è uno solo**. Quello che una
seconda macchina porta e uno shaper non porta è che le due wall clock non
coincidono, ed è la cosa su cui un ticket si regge.

### 28.1 La regola che il codice non seguiva

Il commento accanto a `kTicketSkew` diceva già la regola giusta — «due macchine
che disagree di più della tolleranza non possono attraversare» — ma il codice ne
applicava due diverse:

```cpp
if (t.expires <= unix_now)              // niente tolleranza
if (t.issued > unix_now + kTicketSkew)  // sessanta secondi
```

Un server di là con l'orologio avanti rifiutava un ticket **firmato un secondo
prima** dicendo *«the ticket has expired»* — e chi legge quel log va a guardare
le durate, non gli orologi. La stessa tolleranza ai due estremi, e i numeri
dentro il messaggio:

```
the ticket ran out 90s ago by this server's clock (allowing 60s of disagreement);
if that is not true where it was signed, the two clocks are apart
the ticket is dated 90s ahead of this server's clock, which allows 60s
```

### 28.2 I gate, senza una seconda macchina

Non serve spostare un orologio: un ticket porta i suoi due timestamp, e il codice
sotto esame è il **confronto**. Quindi i ticket vengono firmati con le date che
servono. Quattro casi nuovi, appena dentro e appena fuori i due estremi:

```
ok    gets in: a ticket that ran out, from a clock a little behind this one
ok    refused: a ticket that ran out, from a clock further behind than the allowance
ok    gets in: a ticket from a clock a little ahead of this one
ok    refused: a ticket from a clock further ahead than the allowance
```

**Sabotaggio.** Tolta la tolleranza dall'estremo della scadenza:

```
FAIL  gets in: a ticket that ran out, from a clock a little behind this one
```

### 28.3 Quanto si può disagree, davvero

Con `d = (orologio che controlla − orologio che firma)`, un ticket presentato
appena firmato è buono per **−60s ≤ d < 360s**.

I 360 sono generosi solo perché è soprattutto la **durata** a lavorare, e
calano man mano che il ticket invecchia. Il numero con cui ragionare è
`kTicketSkew`, non il 360 — scritto lì accanto alla costante, perché era proprio
quel genere di conto a mente a rendere il codice asimmetrico.

### 28.4 Cosa una seconda macchina prenderebbe ancora

Questo copre la **politica** sugli orologi. Non copre il resto di «macchine
diverse»: MTU vera e frammentazione, NAT, un percorso che riordina in modi che
uno shaper con un solo hop non produce, e due processi che non si contendono la
stessa CPU. Restano non provati, e questo non è un sostituto — è la metà che si
poteva provare qui.

> **Chiuso in §31.** MTU e NAT sono arrivati con i container (§30). E i due
> orologi che divergono *davvero* non hanno avuto bisogno né di container né di
> VM: trenta righe di `LD_PRELOAD`, e hanno trovato subito un difetto che questo
> gate non poteva vedere — il client buttava via proprio la frase costruita qui
> sopra. Restano fuori solo il kernel separato e la contesa di CPU.

## 29. L'ECS, prima della milestone che lo chiede

DESIGN §8 mette l'ECS in **M8** e chiede «mob base con ECS» in **M7**. M7 sono
otto settimane di inventario, craft, UI e mapgen con un ECS dato per scontato
sotto. Costruire le fondamenta mentre ci si sta sopra è il modo di costruirle
male, quindi la parte C++ arriva adesso e l'API Lua resta a M8.

### 29.1 Flecs, e come è entrato

§6.1 nomina Flecs. Preso alla **v4.1.6**, dalla sua distribuzione a file singolo
— `distr/flecs.c` e `flecs.h` — come sqlite e monocypher, e non con
`add_subdirectory`: flecs costruisce di default una libreria dinamica, installa
target e si porta dietro test ed esempi. Ereditare un build system per
spegnerlo tutto è il modo in cui una dipendenza comincia a decidere cose.

Costo, misurato invece che stimato: **17.3 s** per compilare l'amalgamazione,
**10.9 MB** di libreria statica, e il server passa da **32.8 MB a 38.8 MB** —
non spogliato, con i simboli di debug. Gli addon sono lasciati tutti accesi
apposta: quelli che serviranno (query, gerarchie, l'explorer REST che §6.1 chiede)
sono lì dentro, e tagliare prima di sapere quali è un risparmio misurato contro
un'ipotesi.

### 29.2 La frase che è cambiata

Prima, cosa veniva mandato a un giocatore:

> le **altre sessioni**, saltando quelle non Ready, saltando gli osservatori,
> saltando quelle che non hanno ancora detto dove sono

Quattro fatti sulle **connessioni**, e un mob non ne soddisfa nessuno. Adesso:

> le **entità** vicine con un `Body`

I quattro fatti sono diventati un tag, messo quando c'è qualcosa da disegnare e
tolto quando non c'è. `Session::authority` non è più un campo: è una `flecs::entity`
e due accessori che leggono i componenti. Il resto della sessione — chiavi,
scope, ticket, handshake — resta dov'era, perché appartiene a una connessione e
non a un corpo: un giocatore che attraversa un portale tiene la sessione e prende
un corpo nuovo di là, e un mob ha un corpo e nessuna sessione.

L'id sul filo adesso è quello dell'**entità**, non del peer. Uno spazio di id
solo, così un mob non ne vuole un secondo e chi legge non deve sapere di che
tipo è la cosa che guarda.

### 29.3 La prova che non è una tabella di sessioni con una libreria attaccata

Un ECS adottato e non usato è una dipendenza con una storia sopra. Quindi:
`--mobs N`, tre corpi che nessuno guida, mandati a un client dal codice che non
è stato toccato per farli passare.

```
ok    and is sent three bodies, by the code that streams players
ok    and no connection is behind any of them: they are not players
ok    and there is exactly one session in that world, which is this one
      3 of 3 moved; furthest any of them strayed from its circle: 0.0000 nodes
ok    and all three of them moved, because a system moved them
ok    each still exactly on the circle it was given, which is what the system computes
```

L'ultimo check è il motivo per cui il movimento è in **forma chiusa** e non
integrato: «si è mosso» lo passa anche un corpo che va alla deriva per il motivo
sbagliato. La posizione viene calcolata dall'orologio del mondo, quindi un server
che perde un tick non lascia i suoi mob altrove, e il gate può chiedere **dove**
invece che *se*.

E il client vero, su un socket vero, contro un server vero con `--mobs 5`:

```
left after 10.0s, 49074 frames, 32 chunks, 0 corrections applied
        5 other bodies in view at the end
network client passed
```

**Sabotaggi.** Tolto il `Body` ai mob: quattro check rossi, zero corpi mandati.
Non progredito il mondo: `0 of 3 moved`.

### 29.4 Cosa non è stato fatto

**L'API Lua resta a M8**, ed è la metà che conta per l'estensibilità: componenti
e sistemi dichiarati da un mod, fasi ordinate, iterazione a batch. Qui non c'è
niente che un mod possa raggiungere.

**Niente relazioni ancora.** §6.1 le nomina come la ragione principale per Flecs
nel caso multi-mondo — `(InWorld, SessionId)` — e il server ha ancora un mondo
solo per processo, quindi non c'è niente da mettere in relazione. Quando ce ne
saranno due, quello è il posto.

**`Wander` non è un mob**, è un corpo che gira in tondo. Niente salute, niente
collisioni, niente comportamento: quelli sono M7, e questo è ciò su cui M7 si
appoggerà.

## 30. Un indirizzo è una strada, non un nome

§28.4 diceva cosa una seconda macchina prenderebbe ancora e uno shaper no: MTU
vera, NAT, un percorso che riordina, due processi che non si contendono la CPU.
Con docker se ne prendono tre su quattro — l'orologio no, perché i *time
namespace* di Linux non coprono `CLOCK_REALTIME` e nessun container può spostare
l'orologio che un ticket legge.

`tools/two_machines.sh`: due container `ubuntu:24.04`, un server ciascuno, **due
reti separate** con MTU **1280**, e i due server che si raggiungono **attraverso
le porte pubblicate dell'host** — DNAT in entrata, masquerade in uscita. Mai per
indirizzo di container: un indirizzo di portale che sopravvive a questo è un
indirizzo che sopravvive a un router di casa.

L'albero sorgente è montato allo stesso path dentro il container. Senza, il
server gira con **0 mod e texture generate** invece di 31 file e 4 mod, e ogni
gate su tabella dei blocchi e media misurerebbe un game diverso senza dirlo.

### 30.1 Cosa ha trovato, alla prima run

```
came out at 61.10 20.00 61.07 in the other world; now playing on 172.31.7.1:25892, watching 127.0.0.1:25891
the opening leads to 172.31.8.1:25891 -- looking into it as world 2
the world through the opening will not have us: no ticket this world will honour
```

Dopo l'attraversamento il client apre una **terza** sessione verso una macchina
con cui sta già parlando. Il mondo di là annuncia A come `172.31.8.1:25891`; il
client conosce A come `127.0.0.1:25891`. Il confronto era una stringa:

```cpp
conn_spec[(size_t)remote_conn] == spec
```

Su loopback le due grafie coincidono sempre e il difetto **non esiste**. Dietro
NAT ogni server ha almeno due nomi — quello che il peer usa e quello che il
client usa — e non c'è ragione perché coincidano. Cinque check rossi a valle, e
il peggiore è l'ultimo: quella seconda sessione è stata **rifiutata**, perché il
ticket che il client aveva era per l'altro mondo.

### 30.2 Un server ha un nome, ed è la sua chiave

Il `Welcome` porta la chiave con cui quel mondo firma i ticket, e `Portals`
porta quella del mondo di là — che il server vicino aveva già, perché è
l'*audience* dei ticket che emette. Il client confronta le chiavi e ricade
sull'indirizzo solo quando una delle due manca. Protocollo **7**.

Un server senza `--ticket-key` non ha un nome e lo dice lasciando il campo
vuoto: onesto, e gli costa la possibilità di essere riconosciuto attraverso un
secondo indirizzo.

**Ed è servita anche una correzione di configurazione**, che è una scoperta a sé:
il collegamento di B verso A non nominava A. `--portal-peer-key` c'era solo da un
lato perché solo un lato emetteva ticket; adesso un collegamento che porta a un
server lo **nomina**, in tutti e due i versi. Fino a quel momento il difetto
restava, per metà.

### 30.3 Il gate, senza docker

`tests/bad_link.sh` fa dire al mondo di là che il server del client è
`127.0.0.2`, mentre il client l'ha chiamato `127.0.0.1`. Stessa macchina, due
grafie: la stessa domanda dei container, riducibile a un host solo.

```
ok    every world it is talking to said which server it is, not just where
      1 other world(s) had a window opened onto them
ok    and no extra session was opened to a server it already had, under its other name
```

Contati i **mondi distinti**, non le righe: una finestra che cade e torna dice la
stessa cosa due volte, e contare le righe chiamerebbe quello un secondo server.

**Sabotaggio.** Confronto per solo indirizzo:

```
      2 other world(s) had a window opened onto them
FAIL  2 windows: a second session was opened to a server already connected
FAIL  the far world was not a different game, so nothing was proved about media
FAIL  the client's own checks failed over a link with loss in it
```

### 30.4 E l'MTU

MTU 1280 frammenta davvero. Sui contatori IP dell'host, durante una run:

```
ReasmReqds=786  ReasmOKs=393
```

393 datagrammi riassemblati da 786 frammenti. ENet negozia 1392 e non fa path-MTU
discovery, quindi ogni messaggio pieno esce più grande del path. In una run
precedente si sono visti anche **4 `FragFails`** — pacchetti con DF troppo grandi,
buttati dal kernel. Non ha rotto niente (la ritrasmissione copre), ma è traffico
che loopback non produce **mai**, e la run intera passa:

```
2 of 2 session(s) told this client what server they are, not just where
media: 62 offer(s) across 2 world(s) name 31 distinct file(s); 31 were fetched
network client passed
```

### 30.5 Cosa resta a una VM

L'orologio e il kernel. Il ticket legge `std::time(nullptr)` e nessun container
può spostarlo; §28 copre la **politica** sugli orologi con ticket datati a mano,
non due orologi che divergono davvero. Restano anche fuori: contesa vera di CPU
fra i due server, e un percorso con più di un hop che riordina.

`tools/two_machines.sh` non è un `ctest` e non deve diventarlo: un test che
fallisce su una macchina senza container è un test che insegna a ignorare i
fallimenti. Serve a trovare; quello che trova va in `tests/bad_link.sh`.

## 31. Due orologi che non vanno d'accordo davvero

§28.4 lasciava scritto cosa restava fuori: quello lì è un gate sulla **politica**
— i ticket firmati con date scelte a mano e il codice sotto esame che è il
confronto. Quello che non c'era erano **due processi che leggono orologi
diversi**, sulla chiamata che il server fa davvero.

E non serve né un container né una VM. I *time namespace* di Linux non coprono
`CLOCK_REALTIME` — un container quell'orologio non lo sposta affatto; una VM sì,
al prezzo di un kernel che nessuno ha chiesto. Quello che serve è più piccolo:
**un processo che legge una wall clock sbagliata di una quantità nota**.

`tools/clock_skew`: trenta righe di `LD_PRELOAD` su tre simboli — `time`,
`clock_gettime` e `gettimeofday` — con `CNP_CLOCK_SKEW` in secondi con segno.

### 31.1 Cosa NON sposta, ed è metà del progetto

Solo `CLOCK_REALTIME` (e `CLOCK_REALTIME_COARSE`). Ogni orologio monotono passa
intatto, di proposito: i tick, la stima del round trip, le finestre
dell'heartbeat e il backoff girano su `CLOCK_MONOTONIC`, e spostare quello non
sarebbe una wall clock sbagliata — sarebbe una macchina che vive il tempo in
modo diverso, che è un'altra affermazione e non quella che si fa sulla deriva
NTP.

Verificato prima di concludere qualsiasi cosa: `time`, `CLOCK_REALTIME` e
`gettimeofday` si spostano tutti e tre di 180 s esatti, e `CLOCK_MONOTONIC`
legge **5501 in tutte e tre le run**.

### 31.2 `LD_PRELOAD` non sa nominare questo albero

`LD_PRELOAD` è una lista separata da **spazi**, e questo sorgente sta in
`/home/giacomo/dev/Cubes and portals`:

```
ERROR: ld.so: object '/home/giacomo/dev/Cubes' from LD_PRELOAD cannot be preloaded
```

Non c'è quoting che tenga: non è la shell, è il loader. La libreria viene copiata
nella directory di lavoro del test, e il test rifiuta di partire se anche quella
ha uno spazio. Non è pulizia, è l'unico modo di nominarla.

### 31.3 Il difetto: il server lo spiegava, il client lo buttava

Con l'orologio di là indietro di 120 s, il server di là scrive:

```
refused 'player' with a ticket: the ticket is dated 120s ahead of this server's
clock, which allows 60s
```

Che è esattamente la frase costruita in §28. Il client scrive:

```
the world through the opening will not have us: no ticket this world will honour
```

`Refuse` porta `reason` **e** `detail`; `ClientSession::error()` è già
«reason: detail». Due punti del loop stampavano il solo `reason` — la finestra
passiva e il ripiego a redirect — e il dettaglio moriva lì. Chi legge quel log
va a guardare fiducia e firme, che stanno benissimo.

Un `why_not()`, e i due punti lo usano. Il lavoro di §28 era già tutto fatto:
mancava di arrivare a chi legge.

### 31.4 Il gate che passava per la ragione sbagliata

Prima versione del controllo: `grep -e "clock"` sul log del client. **Verde
subito**, con il client che non diceva una parola su nessun orologio — perché una
delle mod del client stesso stampa `os.clock at load 5702.860s`.

Un gate scritto su una **parola** invece che su una **frase**, e verde alla prima
run: la forma esatta di quello che questo progetto chiama un gate che non misura
niente. Adesso cerca `this server's clock` e poi **il numero dentro la frase**, e
quel numero è l'unica cosa che dimostra che lo skew è arrivato fino al controllo
del ticket invece che i due server aver semplicemente smesso di parlarsi.

### 31.5 I numeri, e i quattro casi

Con `d = (orologio che controlla − orologio che firma)`, la regola di §28.3 è
`−60s ≤ d < 360s`. L'orologio sbagliato è quello del server **di là**, che è
quello che controlla:

```
ok    inside_behind: -30s apart is inside what a ticket allows, and the player goes through
ok    inside_ahead: 300s apart is inside what a ticket allows, and the player goes through
ok    far_behind: -120s apart is outside what a ticket allows, and the world says no
ok    far_behind: and it names the gap as 120s, which is the skew this test applied
ok    far_ahead: 420s apart is outside what a ticket allows, and the world says no
ok    far_ahead: and it names the gap as 120s, which is the skew this test applied
```

I due estremi cadono sullo stesso divario di 120 s per costruzione: 120 indietro
è un ticket «datato 120 s avanti», e 420 avanti è un ticket con 300 di vita
«scaduto da 120». E in ogni caso il mondo in cui il giocatore sta non perde la
sessione per l'orologio del vicino.

### 31.6 Sabotaggi

Tre, tutti rossi:

| Sabotaggio | Esito |
|---|---|
| Il client ristampa il solo `reason` | 4 rossi: nessun orologio nominato, divario `none` |
| `LD_PRELOAD` tolto (skew non applicato) | 6 rossi: i due casi fuori finestra **attraversano** |
| `kTicketSkew` 60 → 600 | 6 rossi: gli stessi due attraversano |

Il secondo e il terzo dicono cose diverse e servono entrambi: il secondo che il
gate misura lo skew, il terzo che misura la **costante** e non solo il fatto che
i due processi non riescano a parlarsi.

### 31.7 Limiti dichiarati

È la risposta della **libc**, non del kernel. Chi legge l'ora per un'altra via —
una syscall a mano, il vDSO diretto, un binario statico — vede l'ora vera e lo
shim non lo sa nemmeno. Il nostro server chiama `std::time(nullptr)` attraverso
la PLT, che è esattamente ciò che viene intercettato, e il test lo **verifica**
invece di darlo per scontato (§31.1).

E lo scarto è **costante**: un orologio che deriva durante una sessione, o che
salta mentre un ticket è in volo, non è coperto. Restano fuori anche le altre
cose di §28.4 che nemmeno questo prende: contesa vera di CPU fra i due server e
un percorso con più di un hop.
