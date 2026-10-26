# M3 — Server e rete: cosa è stato costruito e cosa si è imparato

Milestone M3 di `DESIGN.md` §8: *core headless, ENet + canali, handshake
versionato con negoziazione feature, streaming chunk compresso con interest
management, replica entità, autorità del movimento, SQLite, mapgen deterministico
con stage registrabili.*

Tutto quello che segue è verificato da esecuzioni. `ctest` è passato da 6 a
**15 test**.

---

## 1. Il determinismo prima del socket

La prima cosa costruita non è stata la rete. È la proprietà su cui la rete
poggia: **due macchine generano lo stesso mondo**.

`core/worldgen` è una lista di stage con nome — `core:terrain`, `core:scatter`
di serie, il client aggiunge i suoi. Il generatore duplicato che il client aveva
è sparito: ne aveva uno, il server ne avrebbe voluto un altro, e due
implementazioni del terreno sono un desync con la data sopra.

```
world hash 691762582e7ee82d     ← 1000 tick, seed 1
world hash 691762582e7ee82d     ← altro processo, stesso seed
world hash c8de5b292bb9c604     ← seed 7
```

L'hash del seed 1 è **fissato nel test**: un cambio non voluto al generatore si
vede lì, non in una segnalazione di due giocatori che vedono terreni diversi.

---

## 2. Il protocollo si decide sui buffer, non su UDP

`cnp_core` contiene formato del filo, regole di versione e macchina a stati delle
sessioni. `cnp_net` contiene ENet. **La separazione è la parte che porta peso**:
se una decisione scivola nel wrapper del socket, il test del protocollo — che non
linka nessuna libreria di rete — smette di poterla raggiungere, ed è l'errore di
link a dirlo.

- **Handshake versionato**: la versione è la più alta dentro entrambi i range. Il
  rifiuto dice *quale lato* è indietro, perché "protocol mismatch" manda su un
  forum e "il tuo client è più vecchio di questo server" manda alla pagina di
  download.
- **Feature come nomi, non bit**: un bit lo assegna chi possiede il protocollo,
  un nome può coniarlo un mod (§6.5). L'insieme concordato è l'intersezione
  **nell'ordine del server**, perché entrambi i lati indicizzano quella tabella.
- **Canali mod da 4 in su**, assegnati nel welcome: nessun mod fissa un numero.

### Input ostile

Più della metà del test del protocollo non testa funzionalità. Ogni prefisso di
un messaggio valido, un byte incollato in fondo, 440 bit flip fatti passare per
tutta la porta (decode **e** negotiate), tre loop di fuzz. Pulito sotto **ASan +
UBSan**.

Tetto dichiarato, non sperato: un handshake non autenticato non può far tenere al
server più di **4128 byte** di stringhe.

---

## 3. La versione capita, non sperata

Due campi, due mestieri diversi:

| campo | cos'è | chi decide |
|---|---|---|
| `engine_version` | `"0.3.0-m3"`, stringa | **nessuno**. Va nel log che legge una persona |
| `content_hash` | FNV-1a su ogni nome di nodo in ordine di id | **questo**. Derivato, non dichiarato |

Una stringa di versione la scrive un umano e può essere sbagliata, vecchia, o
identica su due build che differiscono proprio dove conta: gli **id dei blocchi**,
che nascono dall'ordine di registrazione dentro un processo.

Build con stringa diversa e stessa tabella: entra. Stessa stringa, tabella
diversa: rifiutata, con entrambi gli hash nel messaggio.

### Il client adotta la tabella

Il welcome porta ogni nome di nodo in ordine di id — gli id sono le posizioni,
zero numeri sul filo. Il client ricostruisce il registry registrando nell'ordine
in cui arrivano, e gli id escono identici.

**Adottare batte tradurre**: un chunk sono 32768 id, una tabella di rimappatura
girerebbe su tutti, su ogni chunk, per sempre. Un nome mai sentito diventa un
placeholder magenta e solido — lo stesso patto che il registry fa già per un
blocco il cui mod è stato tolto.

```
adopted 31 blocks, 26 of them unknown to this build   ← client senza i mod del server
```

Chi non sa adottare viene rifiutato, perché leggerebbe ogni chunk con i propri id
e disegnerebbe un mondo fatto dei blocchi sbagliati — in silenzio, che è la parte
che merita un rifiuto.

---

## 4. Chunk sul filo, con le misure

Tre livelli, in ordine di quanto risparmiano:

1. **Chunk uniforme = solo header, 20 byte.** Non è compressione: è non mandare
   ciò che non c'è mai stato.
2. **Palette** impacchettata alla larghezza che serve.
3. **zstd livello 3**.

Se la palette aiutasse *sopra* zstd era una domanda vera — il bit packing
distrugge l'allineamento a byte che un compressore sfrutta. Il test codifica ogni
chunk in **entrambi i modi** e stampa i due totali:

```
32 chunks: 16 uniform, 16 palette, 0 raw
in memory 3.00 MB -> on the wire 0.01 MB (0.4%)
same chunks without the palette:      0.02 MB (0.5%)
largest single chunk 1234 bytes, encode 158 us, decode 203 us
```

**Il numero bello non resta da solo.** Quel mondo ha una piazza piatta e quattro
tipi di blocco. Il limite su cui si dimensiona un budget di streaming è un chunk
incomprimibile, quindi il test ne costruisce uno:

```
worst case (random blocks, random light): 91491 bytes
```

Tipico 320 byte, peggiore 89 KB. Quella è la forbice vera.

---

## 5. Interest management

Raggio in chunk, raggio verticale separato (i mondi sono più larghi che alti),
budget per tick **in chunk e in byte**, e l'insieme di ciò che quel giocatore ha
già. Logica pura: un test cammina un giocatore per il mondo e controlla cosa gli
sarebbe arrivato.

Due difetti di ordinamento, trovati dal gate:

- **Il chunk sotto i piedi arrivava quarto.** La distanza era dal *centro* del
  chunk, e un giocatore sul confine — cioè ogni volta che ne attraversa uno — è
  equidistante da quattro. Ora è dal punto più vicino, e il chunk che contiene il
  giocatore è pinnato primo. È la differenza fra apparire sul terreno e apparire
  in un buco.
- Poi è saltato *"guardare dall'altra parte cambia l'ordine"*, e **il codice aveva
  ragione**: il bias di sguardo è un moltiplicatore sulla distanza, quindi a
  distanza zero non fa nulla. Su uno spigolo non importa davvero da che parte
  guardi. Il check ora misura da dentro un chunk e dice perché.

---

## 6. Autorità del movimento

`DESIGN.md` §3.3 chiede *anti-cheat leggero*. Il server verifica che una posizione
dichiarata fosse raggiungibile e che non sia dentro un blocco solido. Rifiuto =
la pretesa viene buttata e il giocatore riceve dove **è**.

Tre regole, ognuna un buco senza le altre:

| regola sola | cosa lascia passare |
|---|---|
| solo speed cap | attraversare un muro **lentamente** |
| solo controllo solidi | traversare la mappa in un tick |
| entrambe, senza clamp sull'intervallo | stare zitti un minuto e spendere tutto in un salto |

```
moved 396.000000 blocks sideways in 0.006056s, the limit is 0.385652
```

Quello che **non** è: una simulazione server-side con rewind e replay. Molto più
grossa, e non viene spacciata per fatta.

Le costanti di velocità stavano nel client. Ora in `core/physics/movement.h` —
terza volta che lo stesso argomento salta fuori (un mapgen, una tabella blocchi,
un set di velocità).

---

## 7. Il mondo su disco

Un file SQLite, chunk per posizione, giocatori per nome, poche righe di metadata.
2-8 giocatori per mondo era già deciso, e quella decisione **è** il progetto dello
storage: niente region file, niente sharding, niente compattazione.

**Un chunk salvato sono gli stessi byte di un chunk spedito.** Un encoder, un
decoder, un solo insieme di controlli sull'input ostile — l'alternativa sono due
di ciascuno, e uno dei due è sempre il più debole.

Due rifiuti, per lo stesso motivo dell'handshake: un file fatto con un altro seed,
e un file fatto da una build con un'altra tabella blocchi.

```
run 1: 0 chunks stored, saved 9 in 2 ms
run 2: 9 chunks stored (9 loaded), stesso world hash
--seed 7 e --no-mods su quel file: rifiutati, exit nonzero
```

---

## 8. Errori fatti, e come sono emersi

1. **Desync a ogni join, che non esisteva.** Il welcome annunciava l'hash del
   mondo *vivo*, preso dopo i mod: `spike` piazza 14 blocchi, `beacons` 3
   pilastri, e nessun client onesto poteva riprodurlo. Ora porta il **mapgen
   hash**. Il test loopback non poteva trovarlo: il suo "server" è uno stub il cui
   hash veniva dalla stessa funzione che il client usava per verificarlo — si dà
   ragione da solo. Stessa trappola già pagata dal gate del portale.
2. **La luce buttata via.** `Chunk::is_uniform()` risponde a una domanda sui
   *blocchi*, e l'encoder la leggeva come se coprisse anche la luce. Un chunk
   d'aria ha blocchi uniformi e un array di luce pieno appena il sole viene
   calcolato: ogni chunk d'aria veniva salvato **e spedito** come "aria, luce 0".
   Un cielo che arriva nero. Trovato dal gate dello storage; il percorso di rete
   aveva lo stesso bug e nessun check che potesse vederlo, perché confrontava i
   blocchi e non la luce. Ora confronta entrambi.
3. **Una riga corrotta poteva portarsi via un chunk sano.** 64 byte di spazzatura
   si decodificano come un chunk uniforme perfettamente valido alla posizione che
   i primi 12 byte compitano — la posizione viene dal blob, non dalla riga.
   `install_chunk` l'avrebbe scritto sopra una parte vera del mondo.
4. **La feature che il probe non mandava.** La riga che la aggiungeva finiva
   *sopra* quella che assegna l'intera lista, che la buttava via. Ogni join
   rifiutato, con un messaggio perfettamente corretto — numeri giusti, motivo
   sbagliato.
5. **`close()` faceva sparire il client senza salutare.** Il server teneva la
   sessione fino al timeout ENet, decine di secondi.
6. **stdout in blocco.** Il server scrive quasi sempre su pipe, che C rende fully
   buffered: `listening on UDP 25301` restava in un buffer da 4 KB mentre un test
   andava in timeout contro un server acceso da sempre. Costo: 80 secondi.
7. **`WITHOUT ROWID`.** Il test corrompeva una riga con un `UPDATE ... WHERE
   rowid`, che su quella tabella non trova nulla e riporta successo.

---

## 9. Test che passavano per il motivo sbagliato

Tre, e sono la parte di M3 che vale di più.

1. **"L'altro giocatore non ha visto il teletrasporto"** passava **anche col
   rifiuto sabotato**: se il teletrasporto fosse stato accettato, il barante
   finiva fuori dal raggio entità, nessun aggiornamento ulteriore, e il check
   leggeva il messaggio *precedente*. Verde per assenza di dati. Ora chiede
   direttamente al server cosa crede.
2. **"Un client con tabella diversa è rifiutato"** restava verde col controllo del
   server disattivato, perché il client rifiutava da sé. Difesa in profondità che
   funziona, e un test che non testava quello che diceva. Il probe ora esce con
   **2** = rifiutato dal server e **3** = ho rifiutato io; lo script pretende 2.
3. **Il fuzz dei chunk non arrivava ai parser**: corrompere il payload
   *compresso* fa fallire zstd prima. Sabotando tre controlli interni la sezione
   restava verde. Ora ci sono input costruiti a mano: frame zstd validi, contenuto
   ostile.

Più due asserzioni mie sbagliate che il codice ha smentito: *"cadere non viola il
limite di velocità"* (il test cadeva dentro il terreno) e *"guardare dall'altra
parte cambia l'ordine"* (su uno spigolo non lo cambia, e non deve).

**Un check vacuo è peggio di nessun check, perché viene contato.**

---

## 10. Sabotaggi

Ogni gate è stato rotto di proposito per vedere se diventa rosso.

| sabotaggio | esito |
|---|---|
| bounds check del reader tolto | **ASan: heap-buffer-overflow** |
| cap su lunghezza stringa tolto | verde → **buco vero**, aggiunto un input grande e valido |
| cap sul conteggio tolto | verde → stesso buco, stessa correzione |
| controllo byte in eccesso tolto | 2 rossi |
| range del canale mod tolto | 2 rossi |
| scelta di versione invertita | 3 rossi |
| tetto sul payload chunk tolto | **si impianta** → per quello c'è il TIMEOUT |
| dimensione decompressa non verificata | 2 rossi |
| bound sull'indice di palette tolto | 2 rossi |
| chunk non marcati come inviati | 1 rosso (chi sta fermo streamma per sempre) |
| chunk contenitore non pinnato | 1 rosso |
| margine di dimenticanza ignorato | 1 rosso |
| posizione vecchia applicata | 1 rosso |
| posizione NaN accettata | 1 rosso |
| speed cap tolto | 2 rossi |
| intervallo non clampato | 1 rosso |
| controllo solidi tolto | 1 rosso |
| entità che includono te stesso | 1 rosso |
| pretesa rifiutata che muove comunque | 1 rosso *(dopo aver riparato il check vacuo)* |
| seed non verificato all'apertura | 2 rossi |
| content hash non verificato | 2 rossi |
| posizione del blob non verificata | 1 rosso |
| encoding uniforme che ignora la luce | 2 rossi |
| server che non offre l'adozione | 1 rosso |
| tabella adottata al contrario | 2 rossi |
| nomi sconosciuti saltati | 1 rosso |

Due controlli **non** si sono lasciati rompere: le due verifiche di lunghezza del
corpo di un chunk. Sono early-out con un messaggio migliore, non la barriera —
`unpack_indices` limita già le proprie letture. Sta scritto nel codice, perché un
controllo che nessuno riesce a rompere non sta proteggendo niente da solo, e
fingere il contrario è come una difesa viene cancellata più avanti da chi verifica
se serve e scopre che non serviva.

---

## 11. L'exit test

`DESIGN.md` §8 chiede: *«4 client su LAN, 30 minuti, nessuna desync; server
headless in CI che gira 1000 tick e verifica un hash del mondo.»*

**La metà headless c'è ed è in ctest.** 1000 tick con i quattro mod veri, due
volte, stesso hash.

**I quattro client ci sono**, come `tests/four_clients.sh`: quattro processi veri
su un server vero. La differenza rispetto a quanto chiesto è dichiarata nello
script invece che nascosta — loopback e non LAN, 10 secondi in ctest e non 30
minuti (`CNP_SOAK_SECONDS` allunga).

Cosa viene fatto significare a "nessuna desync", ognuna verificata:

- ogni client riproduce il mondo del server dal seed
- ogni chunk decodifica e coincide col mondo generato tranne dove hanno scritto i mod
- un client che cammina a passo d'uomo **non viene mai corretto**
- il server è ancora in piedi, con tutti usciti puliti

Quella terza è la metà interessante: **una correzione di chi gioca onestamente è
peggio di un cheat che passa**, perché il cheat è una persona e la correzione
falsa la sentono tutti. Il soak fallisce su una sola.

```
four clients, 10s: 8 chunks each, 0 corrections, ~214 entity updates each
```

---

## 12. Numeri

```
server     1000 tick, mondo 128:  world hash 691762582e7ee82d
handshake  238 byte out, 161 in (lato server)
tick       50.041 ms/tick su 600 tick con due client dentro
chunk      tipico 320 byte, peggiore 91491 byte
storage    9 chunk in 2 ms, 28 KB su disco
client     2351 FPS, scripting 0.567% del frame
ctest      13 test
```

---

## 13. Cosa manca a M3, dichiarato

- **Il soak da 30 minuti**: eseguito, §16. Su loopback, non su due macchine, e
  quella differenza resta aperta. Ha trovato un difetto: il client grafico fermo
  viene disconnesso poco prima della fine, causa non ancora identificata.
- ~~**Nessuna crittografia.**~~ **Chiusa: §15.** La cifratura di sessione c'è, con
  forward secrecy e chiave del server appuntabile. Le chiavi d'identità del
  **giocatore** e i ticket firmati di §4.4 restano M6.
- **Nessun messaggio di modifica blocchi.** Il client non può ancora rompere o
  piazzare **attraverso la rete**: lo fa solo in locale. Quindi la "distanza di
  interazione" dell'anti-cheat leggero non esiste ancora — non c'è interazione da
  limitare.
- **Nessun inventario server-side**, per lo stesso motivo per cui M2 non aveva
  item: non esiste un sistema di item. Arriva con M7.
- **Salvataggio solo allo spegnimento.** Un server che cade perde ciò che è
  successo dal suo avvio. Il salvataggio incrementale ha bisogno che l'insieme
  "sporco" significhi *cambiato da quando è stato salvato* e non *da rimagliare*.
- **I canali dei mod sono assegnati ma vuoti**: `core.net.channel` di §6.5 non
  esiste ancora, quindi la tabella parte vuota invece che indovinata.

Tre voci di questa lista sono state chiuse dopo: il client di rete in §14, la
crittografia in §15 e il soak in §16 — che a sua volta ne ha aperta una nuova.

## 14. Il client di gioco entra in rete

Il buco peggiore di M3 non era in questa lista, perché non me n'ero accorto
mentre lo scavavo: **tutta la rete era stata costruita contro una sonda di
test**. Il client che si gioca generava il mondo in locale e non aveva mai
aperto un socket.

Il rischio non è che il client non funzionasse. È che esistessero *due* client —
quello testato, dentro `tests/net_loopback_test.cpp`, e quello che si spedisce —
e che il soak misurasse il primo. Un soak da trenta minuti sul client sbagliato
è trenta minuti buttati.

La correzione è una sola classe: `src/core/net/client_session.{h,cpp}`. Fa
quello che faceva la sonda — connessione, handshake, adozione della tabella,
installazione dei chunk, rivendicazione della posizione, correzioni — e adesso
**la sonda e il gioco la condividono**. Sta in `cnp_net` e non in `cnp_core`
perché possiede un socket; ogni decisione che prende (`accept_welcome`,
`adopt_node_table`) resta in `cnp_core` e resta testabile senza socket.

Cosa cambia nel client quando è connesso, e ciascuna è una decisione:

| Cosa | Da soli | Connessi |
|---|---|---|
| Mondo | generato qui | arriva a chunk, e all'inizio non c'è |
| Tabella blocchi | registrata | **adottata** dal server |
| Luce | calcolata qui | dentro il chunk, già fatta dal server |
| Mod | caricati | **nessuno** |
| Portali | due, con le pareti | spenti |
| Rompi/piazza | sì | no |
| Movimento | locale | locale, con il server che corregge |

I mod non girano sul client di rete, e non è una scorciatoia. Questi mod
scrivono blocchi e piazzano portali: girando qui modificherebbero una copia del
mondo del server di cui il server non sa niente, e ogni modifica sarebbe una
desync che il client si è fabbricato da solo. Un'API mod lato client — che
disegna e ascolta ma non scrive — è un'altra cosa, e non è questa.

La conseguenza visibile: contro un server con i mod attivi il client adotta 31
blocchi di cui **26 sconosciuti a questa build**, che diventano segnaposto
magenta. È esattamente il comportamento voluto — si entra su un server di cui non
si hanno i mod — e adesso si vede a schermo.

### Il gate, e un controllo che passava per il motivo sbagliato

`tests/net_client.sh` (etichettato `gpu`) avvia il server vero e il client vero,
gioca otto secondi e controlla. Quattro sabotaggi:

| Sabotaggio | Esito |
|---|---|
| La sessione installa i chunk in un mondo suo invece che in quello disegnato | **rosso** |
| Il client non rivendica mai una posizione | **rosso** |
| Il client tiene i propri id invece di adottare quelli del server | **rosso** |
| Niente freno mentre il mondo non è ancora arrivato | **verde** — non lo prende |

L'ultimo è dichiarato invece che nascosto: su loopback i primi chunk arrivano in
circa cinquanta millisecondi, troppo poco perché una caduta conti. Morde su un
collegamento dove il mondo tarda più di una caduta, cioè su tutti quelli veri.

Il terzo sabotaggio ha trovato un difetto **nel gate stesso**. Il primo controllo
"il giocatore è atterrato" leggeva `player.on_ground`, e passava per un motivo
che non regge: senza niente da disegnare il client gira a seimila frame al
secondo, quindi `dt` è 160 microsecondi e un frame di gravità sposta il giocatore
di un milionesimo di blocco — meno di quanto un `float` sappia rappresentare a
y = 20. Il passo verso il basso arrotonda a zero, nessuna collisione viene mai
risolta, e `on_ground` resta al valore che aveva. **Quel flag stava misurando il
frame rate, non il terreno.**

Sostituito da tre domande che non dipendono dal tempo: sotto il giocatore c'è un
blocco solido, il giocatore non ci è dentro (`inside_solid`, lo stesso predicato
con cui il server rifiuta), e non è caduto sotto il mondo. E l'adozione si
controlla direttamente — stessa dimensione della tabella del server, e ogni nome
all'id che il server gli ha dato — perché il terreno è fatto solo di blocchi core,
che hanno lo stesso id da entrambe le parti: un client che non adotta niente
cammina sull'erba e passerebbe tutto il resto.

### Una duplicazione trovata di passaggio (§14)

`movement.cpp` teneva la propria `kHalfWidth`/`kHeight` accanto a
`kPlayerHalfWidth`/`kPlayerHeight` di `aabb.h`, con un commento che diceva "se le
due non concordano il server rifiuterà posizioni che il client considera legali".
Due costanti che *si dà il caso* coincidano sono due costanti che possono
smettere di coincidere in una modifica sola. Adesso ce n'è una.

## 15. La crittografia di sessione

Seconda voce chiusa dalla lista §13. ENet non cifra, e `DESIGN.md` §3.2 dice in
lettere che non è opzionale se i portali portano su server di terzi.

### Sta *sotto* il protocollo, non dentro

La decisione che conta è dove metterla. Sta nel trasporto: `Transport` non
riporta al chiamante l'evento *Connected* di un peer finché lo scambio di chiavi
per quel peer non è finito. Da quel momento ogni byte in ogni direzione è
sigillato — handshake, tabella dei blocchi, chunk, rivendicazioni di posizione.

Conseguenza misurabile: `protocol.h`, `session.h` e `client_session.h` **non sono
cambiati**. Il test del protocollo continua a girare su buffer senza socket, e
`net_loopback` è passato al primo colpo con la cifratura accesa senza toccare una
riga di test. Se la crittografia fosse finita dentro il protocollo, quella
separazione sarebbe morta.

L'altra conseguenza: non esiste un percorso da un chiamante a `enet_peer_send`
che salti il sigillo. Non è disciplina, è che la funzione che sigilla è l'ultima
cosa prima dell'invio e non c'è modo di aggirarla.

### Cosa promette, e cosa no

| | |
|---|---|
| Contro chi ascolta | protetto sempre — restano solo dimensioni e tempi dei pacchetti |
| Contro chi si mette in mezzo | protetto **solo** se il client ha appuntato la chiave (`--server-key`) |
| Forward secrecy | sì: la coppia effimera muore con la sessione |
| Identità del **giocatore** | **no** — è M6, con i ticket firmati di §4.4 |

Senza pin è lo stesso patto di SSH su una macchina mai vista. Il client lo dice
a voce ogni volta invece di lasciarlo intuire:

```
encrypted, server key 75cdd674…2277
NOT pinned: safe from listening, not from interception.
```

Il server rifiuta di aprire la porta se non riesce a produrre una chiave. Partire
comunque vorrebbe dire partire in chiaro, e l'unica cosa peggiore di nessuna
crittografia è una crittografia che la gente crede di avere.

### Le due cose che si comprano separatamente

Lo scambio è **due** X25519, non uno, e ciascuno paga una cosa diversa:

- effimera × effimera → forward secrecy. Nessuna chiave a lungo termine la
  recupera dopo, perché entrambe le metà muoiono con la sessione.
- effimera × statica → l'identità del server. Solo chi ha il segreto statico può
  calcolarla.

Solo la prima darebbe un canale impersonabile da chiunque; solo la seconda un
canale che una chiave rubata decifra retroattivamente. Nel hash entra anche la
trascrizione delle tre chiavi pubbliche.

### Il nonce porta il canale, e le due direzioni hanno chiavi diverse

Non è dettaglio: sono le due righe che rendono impossibili due attacchi che
altrimenti funzionerebbero perfettamente contro pacchetti autentici.

- **Chiavi diverse per direzione**: un frame rispedito a chi l'ha mandato non si
  apre. Con una chiave sola si aprirebbe e autenticherebbe.
- **Canale nel nonce**: una rivendicazione di posizione non si può rigiocare come
  chunk.

Il replay ha una finestra scorrevole di 64 **per canale**, non una condivisa. Il
canale 2 è inaffidabile e riordina liberamente: una finestra sola butterebbe
aggiornamenti entità ogni volta che parte una raffica di chunk sul canale 1. E la
finestra si muove **solo dopo** che il tag è stato verificato — altrimenti
chiunque sappia mandare spazzatura potrebbe bruciare contatori e chiudere fuori
il peer vero.

Un frame che non autentica viene buttato e contato, non uccide la sessione: UDP
consegna pacchetti corrotti, e chiudere alla prima significherebbe regalare un
denial of service a chi lo manda.

### I numeri

| | |
|---|---|
| Costo per pacchetto | **25 byte** — 1 di tag, 8 di contatore, 16 di MAC |
| 20 s di cammino, in uscita | 16 614 → 26 578 byte (400 rivendicazioni × 25 = 10 000, misurato 9 964) |
| 20 s di cammino, in entrata | 15 829 → 16 729 byte (36 pacchetti × 25 = 900, misurato 900) |
| In proporzione | +0.03% su un chunk da 91 KB, **+86%** su una rivendicazione da 29 byte |
| In assoluto, per giocatore | circa 500 byte al secondo |

Il +86% sul messaggio piccolo è il numero onesto da guardare: 25 byte sono tanti
rispetto a 29. In assoluto sono mezzo kilobyte al secondo per giocatore, che non
è un problema — ma è la ragione per cui non si aggiungono altri messaggi da 20
byte a 20 Hz senza pensarci.

### Il gate

`tests/crypto_test.cpp`, 46 controlli, nessun socket: manomissione di un bit nel
testo cifrato e nel MAC, troncamento, frame vuoto, replay immediato, riordino
lecito, frame vecchio di 200 pacchetti, riflessione al mittente, spostamento di
canale, due sessioni che non si leggono a vicenda, pin giusto, pin sbagliato,
chiave pubblica tutta zeri, primo pacchetto che non è un'offerta, sigillo prima
che ci sia una chiave, e i file di chiave.

Cinque sabotaggi, ognuno rosso sul controllo giusto:

| Sabotaggio | Controllo che diventa rosso |
|---|---|
| Una chiave sola nelle due direzioni | il frame riflesso al mittente si apre |
| Canale fuori dal nonce | il frame spostato di canale si apre |
| Finestra di replay disattivata | tre controlli di replay |
| Pin non verificato | tre controlli sull'impostore |
| Chiave degenere accettata | la chiave tutta zeri passa |

Un controllo scritto e poi rifatto: *«e il frame genuino si apre lo stesso dopo»*.
Serve a dire che nessuno dei rifiuti precedenti ha mosso la finestra — cioè che
mandare spazzatura non chiude fuori nessuno.

Sopra il socket, `two_process_join.sh` fa le stesse domande ai binari veri: il
server stampa una chiave, un client che la appunta entra, un client che ne
appunta un'altra rifiuta, e un client con la cifratura spenta non entra affatto.

## 16. Il soak da 30 minuti, eseguito

L'exit test di M3 (`DESIGN.md` §8: *4 client su LAN, 30 minuti, nessuna desync*)
era la casella dichiarata aperta in §13. È stato eseguito.

**Differenza dichiarata, non glissata**: quattro processi su loopback e non
quattro macchine su LAN. Quello che il loopback non esercita è perdita di
pacchetti, riordino su un collegamento vero, MTU e frammentazione, e i pochi
millisecondi che separano una predizione da una correzione. Tutto il resto —
protocollo, crittografia, streaming, autorità del movimento, e se qualcosa
perde colpi in mezz'ora — lo esercita esattamente.

Il quinto giocatore è **il gioco stesso, con la finestra**, così che la misura
includa il client che si esegue davvero e non solo la sonda. Due delle quattro
sonde appuntano la chiave del server, due no.

### I numeri

| | |
|---|---|
| Durata | 1800 s, cinque giocatori |
| Correzioni | **0**, su tutte e quattro le sonde |
| Errori di chunk | **0** |
| Blocchi diversi dal mondo generato | 48 su 1 048 576 (il lavoro dei mod) |
| Aggiornamenti entità per sonda | 36 020 |
| Traffico per sonda | 4.86 MB in / 2.28 MB out |
| Client grafico | 11 555 936 frame in 1798 s (**6426 FPS** con il socket nel loop) |
| RSS del server | 11 356 kB all'avvio → **12 612 kB** alla fine |
| Frame sigillati rifiutati | 0 |

Un megabyte e mezzo di crescita di RSS in mezz'ora con cinque giocatori. Non è
piatto, ed è il genere di numero che va guardato di nuovo a M9; non è nemmeno una
perdita evidente, perché il grosso è l'arena dell'allocatore che si assesta.

### E ha trovato qualcosa che cinque minuti non trovavano

**Il client grafico è stato disconnesso a 1798.2 s**, cioè 1.8 s prima della
fine, con `disconnected: closed`. Il server ha registrato `'giocatore' left`
prima di tutti gli altri; le quattro sonde hanno completato i 1800 s senza una
correzione. Le sonde hanno smesso di vederlo fra 1680 s e 1740 s.

La differenza fra il client grafico e le sonde in quella modalità è che **il
client grafico sta fermo**: manda 36 000 volte la stessa posizione, mentre le
sonde camminano. La causa non è ancora identificata, e viene scritta così invece
che spiegata con una teoria non verificata.

Questo è esattamente ciò per cui esiste un exit test da mezz'ora: il soak da 5
minuti eseguito prima di chiudere M3 non lo aveva mostrato, e nessuno dei 18
test della suite lo mostra. **È la prima cosa che M3 deve ancora.**

Lo script è `tests/soak.sh`, non è in `ctest` — mezz'ora non si mette in una
suite che la gente esegue tutto il giorno — e prende i secondi come argomento.
