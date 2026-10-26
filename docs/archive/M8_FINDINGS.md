# M8 — ECS e API meccaniche: cosa è stato costruito e cosa si è imparato

Milestone M8 di `DESIGN.md` §8: *componenti/sistemi da Lua con iterazione
batch, fasi ordinate, trait sui nodi, render pass registrabili, canali di rete
per mod.*
**Test:** *riscrivere i blocchi classici dei puzzle a portali (pulsante, dispenser, pedestal, clock)
**solo** come mod, senza toccare l'engine — è la prova che l'estensibilità è
vera.*

**In corso.** Sotto c'è solo ciò che è stato costruito e verificato da
esecuzioni.

---

## 1. Il gate prima del codice

M7 ha chiuso con il suo test d'uscita scritto per ultimo, e la prima run è stata
rossa in tre punti — tra cui uno spawn che esisteva solo con `--portals`, quindi
ventidue minuti di giocatore che cadeva fuori dal mondo. La lezione era che
quella run avrebbe dovuto esistere prima.

Quindi M8 comincia dal suo test d'uscita, con l'engine che non è ancora capace
di farlo passare. Quattro mod in `tests/mods_portal1/` — `button`, `dispenser`,
`pedestal`, `clock` — un probe headless (`cnp_net_loopback_test --portal1`) che
ci cammina dentro, e `tests/portal1.sh` che misura le quattro meccaniche.

**Registrato in `ctest` e `DISABLED`.** Un test rosso lasciato nella suite
trasforma la suite in una cosa che si legge di sfuggita; `DISABLED` lo mostra in
ogni riepilogo come *Disabled* — presente, nominato, senza mentire su un verde
che non c'è. La riga viene tolta quando l'ultimo controllo passa, ed è quello
che vuol dire chiudere M8.

### 1.1 I quattro blocchi, ridotti a quello che M8 deve provare

Il fork li ha già, in `yaportal/init.lua`, e sono 2×2 con `nodebox`, `facedir`,
texture composte a runtime e mesecons. Niente di tutto ciò è M8: `nodebox` e i
modificatori texture sono di M11, le texture di M12. Quello che è di M8 sta
sotto:

| blocco | cosa deve saper fare, in termini di engine |
|---|---|
| pulsante | una cella deve sapere **chi ci sta sopra**, ogni tick, e dirlo al resto del mondo |
| dispenser | una cella deve reagire al **segnale di un altro mod**, e ricordare la propria configurazione |
| pedestal | una cella deve **tenere un oggetto**, prenderlo dalla borsa di chi lo offre e restituirlo a chi si avvicina |
| clock | una cella deve **tenere un numero** e riaverlo dopo che il mondo è stato chiuso e riaperto |

Il display del clock qui sono quattro blocchi accesi o spenti che leggono il
conteggio in binario. Volutamente rozzo: quello che il gate misura è lo *stato*,
e una fila di blocchi è un display che si legge dal file del mondo.

### 1.2 Otto chiamate che non esistono, dette dai mod stessi

I quattro mod non falliscono al caricamento: **sondano** quello che gli serve e
lo scrivono nel log. Un gate che si ferma alla prima mancanza dice una cosa
sola; questi dicono tutta la lista in una run.

```
button:    MISSING 2 call(s): players, player_pos
clock:     MISSING 3 call(s): meta_set, meta_get, find_nodes
dispenser: MISSING 2 call(s): meta_set, meta_get
pedestal:  MISSING 7 call(s): meta_set, meta_get, players, player_pos,
                              give_item, held_item, take_item
```

Otto nomi distinti, con dietro quattro cose:

1. **stato per cella** (`meta_set`/`meta_get`) — tre mod su quattro. Oggi il
   `container_*` è per cella ma tiene *item*, non campi; e una tabella Lua nel
   VM del mod è esattamente ciò che non sopravvive.
2. **dove sono i giocatori** (`players`/`player_pos`) — due mod su quattro. In
   `core.*` non c'è niente che dica che qualcuno è nel mondo.
3. **la borsa di un giocatore** (`held_item`/`take_item`/`give_item`) — il
   pedestal. E qui c'è una scoperta, sotto.
4. **ritrovare le proprie celle** (`find_nodes`) — dopo un riavvio nessuno
   ripiazza un blocco che è già nel file del mondo. È quello per cui l'engine di partenza ha
   gli LBM.

Nessuna di queste era nella lista di cinque scritta leggendo `DESIGN.md`. Le
prime tre sono uscite dallo scrivere i mod, che è il motivo per cui il gate
viene prima.

### 1.3 `core:used` porta il blocco cliccato, mai la mano

Trovato scrivendo il pedestal, non leggendo l'engine. `core:used` ha un campo
`id`, e in `session.cpp` è:

```cpp
out.use_had = m_world->get_block(req.x, req.y, req.z);
```

cioè **il blocco su cui si è cliccato**. Un pedestal non può quindi distinguere
un click con la portal gun da un click a mani sporche di terra, né togliere la
gun dalla borsa da cui viene. Da qui `held_item` e `take_item`.

### 1.4 `ev.cancelled` non è `ev.cancel`, e nessuno lo dice

La prima versione del pedestal annullava il click con `ev.cancelled = true`. Il
campo si chiama `cancel`. Scrivere una chiave che non esiste su una tabella Lua
non è un errore, quindi il mod ha girato, ha detto di aver annullato, e il
blocco è stato piazzato lo stesso. Aperto come `TODO.md` OPEN-18.

### 1.5 Un blocco che un mod cambia non arriva a nessuno

**Il difetto che il gate ha trovato alla prima run, ed è in produzione, non in
M8.** Il clock contava — tredici passi nel log del server — e il probe leggeva
il display *tutto spento*. `host_set_block` nel server è:

```cpp
void host_set_block(void *ud, int x, int y, int z, int id)
{
    ServerState *s = static_cast<ServerState *>(ud);
    s->world.set_block(x, y, z, (BlockId)id);
    s->block_writes++;
}
```

Scrive nel mondo e non lo dice a nessuno. Un `Edit` di un giocatore viene
rimbalzato a tutti (`block_change_message`); una scrittura di un mod no. Chi è
già connesso vede lo stato che c'era quando gli è arrivato il chunk, e nient'altro,
finché non si riconnette.

Non se n'era accorto nessuno perché **i mod girano solo sul client non
networked** — il commento in `client/main.cpp` lo dice e ha ragione a dirlo — e
lì `set_block` scrive nel mondo che quello stesso processo disegna. Su un server
`mods/beacons` accende un faro diverso ogni frame da M2 e nessun giocatore
collegato ne ha mai visto uno.

**E c'era una seconda metà, trovata guardando la prima.** L'edit del giocatore
non si limita a essere trasmesso: `apply_edit` fa `relight_around`. La
scrittura di un mod no — e il mondo viene illuminato **una volta sola**, prima
che i mod si carichino (`compute_lighting` alla riga 761, `load_dir` alla 859).
Quindi una lampada piazzata da un mod al caricamento era **spenta in ogni chunk
mai spedito** da quel server, non solo per chi era già connesso. Da M0: `spike`
registra `spike:lamp` con `light_source = 14` e la piazza in cima alla sua
torre.

### 1.5b Una scrittura sola, e chi viene avvisato resta la differenza

Chiuso facendo passare le due strade per una funzione (`TODO.md` CLOSED-17):

```cpp
void write_block(ServerState &s, int x, int y, int z, BlockId id)
{
    s.world.set_block(x, y, z, id);
    s.block_writes++;
    relight_around(s.world, s.registry, x, y, z);
}
```

Due modi di cambiare un blocco erano due insiemi di conseguenze che vanno
d'accordo finché non se ne modifica uno. **Chi viene avvisato**, invece, resta
diverso apposta: l'edit di un giocatore è risposto sul momento, perché il
giocatore sta aspettando; la scrittura di un mod finisce in una lista svuotata
una volta per tick — posizioni, deduplicate, con l'id letto dal mondo quando il
messaggio parte — perché un handler può scrivere cento celle e cento invii da
dentro un handler è un handler che può bloccare il loop. E prima che il socket
esista non si accoda niente: al caricamento ogni mod sta costruendo la sua
scena, e quelle celle sono già nel chunk che riceve chi entra.

Gate: [`tests/mod_write.sh`](../../tests/mod_write.sh), headless e **nel set di
default**, dall'unico lato da cui il difetto si vede — un client dentro il mondo
mentre un mod lo cambia, e un client che arriva **dopo** che un mod ha acceso
qualcosa.

| sabotaggio | esito |
|---|---|
| S1 la scrittura di un mod non viene detta a nessuno | rosso |
| S2 la scrittura di un mod non viene rilluminata | rosso |
| S3 il mod non fa mai il flip | rosso |
| S4 la coda smette di deduplicare | **verde, ed è giusto così** |

S4 è verde di proposito, e la riga del gate lo dice: togliere la deduplicazione
costa messaggi e non cambia nessuna risposta, quindi un gate che diventasse
rosso lì starebbe misurando la cosa sbagliata.

**Limite dichiarato, aperto come OPEN-19.** Un mod che riscrive migliaia di
celle in un tick manda migliaia di messaggi da nove byte. Oltre il centinaio e
mezzo di celle nello stesso chunk conviene dimenticare quel chunk in ogni
`PlayerInterest` e lasciarlo rimandare dallo streamer — cosa che richiede un
`forget()` per chunk che non esiste.

### 1.6 I sabotaggi, e il primo verde

Quattro controlli su quattordici sono verdi oggi. Tutti e quattro sabotati.

| sabotaggio | esito |
|---|---|
| S1 uno dei quattro mod non si carica | **verde** — il gate non se n'è accorto |
| S2 il clock non conta mai | rosso |
| S3 `src/` impara il nome di un blocco | rosso |
| S4 il probe non entra mai nel mondo | rosso |

**S1 verde è di nuovo la forma di M7_FINDINGS 31.7:** un log letto per una cosa
che non sta dicendo. Il gate contava le righe che cominciano per `mod '` — e un
mod che fallisce ne stampa una anche lui:

```
mod 'clock' not loaded: cannot open tests/mods_portal1/clock/nothing.luau
```

Quattro righe, quattro mod, tre caricati. Adesso il gate legge la frase che
l'engine scrive **dopo** aver deciso (`3 mods running`), ed è rosso.

Sabotato anche il controllo che regge l'intera milestone — «`src/` non nomina
nessuno dei quattro» — e la prima versione era rossa **per il motivo sbagliato**:
cercava la parola inglese *dispenser* e la trovava in due commenti, uno in
`api.luau` che spiega a cosa serve `container_add`. Un commento che dice «un
dispenser è fatto così» è l'opposto del difetto cercato: è l'engine che descrive
una chiamata generale. Ora cerca i **nomi registrati** (`dispenser:vent`,
`clock:lit`, `button:pressed`, …), che è quello che un caso speciale in C++
dovrebbe per forza scrivere.

E il display del clock veniva letto moltiplicando gli id grezzi dei quattro
blocchi: `12221`, maggiore di zero, da un display spento per tre quarti. Ora il
probe chiede l'id di `clock:lit` alla tabella dei blocchi che ha adottato dal
server e legge quattro bit.

### 1.7 Dov'è il gate, e com'è oggi

    tests/portal1.sh build/src/server/cnp_server build/tests/cnp_net_loopback_test

    ok    all four mods loaded
    ok    a body joined the world the mods built in
    FAIL  the pad did not notice a player standing on it (was 9, still 9)
    FAIL  the pad stayed down after the player left
    FAIL  nothing was dispensed: the button's signal reached no vent
    FAIL  the vent never reported dispensing anything
    FAIL  the stand took nothing: the bag went 1 -> 1
    FAIL  walking up to the stand got nothing back (1 -> 1)
    FAIL  the clock display never lit
    ok    and it counted while the world ran (13 steps)
    FAIL  the clock came back at 0: a cell cannot remember anything
    FAIL  the clock did not recognise the cell it had already built
    FAIL  the stand did not recognise the cell it had already built
    ok    nothing under src/ names a block, an item or an event of the four

    PORTAL BLOCKS FAILED (10)

**Un limite dichiarato.** Le coordinate delle quattro celle sono scritte due
volte: nei mod e nel probe. È una regola scritta due volte, che in questo repo è
la causa di quasi ogni difetto trovato — e qui non c'è una terza opzione, perché
non esiste una chiamata che chieda a un mod dove ha costruito e inventarne una
per un fixture non è ciò che M8 chiede. Detto qui invece di lasciarlo scoprire.

---

## 2. Quello che una cella ricorda

Il pezzo più grosso della lista di 1.2: `meta_set`/`meta_get`, che tre dei
quattro blocchi nominavano. Con questo il gate d'uscita passa da 4 verdi a **8**,
e il riavvio funziona: il clock riparte dal numero a cui si era fermato.

### 2.1 Stringhe, e solo stringhe

```lua
core.meta_set(x, y, z, chiave, valore)   -- valore vuoto = cancella
core.meta_get(x, y, z, chiave)           -- "" se non c'è niente
```

Non perché i numeri siano difficili, ma perché **un numero è spesso la cosa
sbagliata da salvare**. Un pedestal che salva l'id 41 sta salvando qualunque
cosa voglia dire 41 la prossima volta che il mondo si apre: gli id sono
assegnati in ordine di registrazione, quindi basta un mod in più e quel 41 è un
altro oggetto. Un **nome** sopravvive. Chi vuole un numero scrive `tostring` e
legge `tonumber`, che è una conversione che si vede invece di una che l'engine fa
dietro le spalle.

**Cancellare è scrivere niente**, e questa è una regola invece di due. Leggere
un campo che nessuno ha scritto dà una stringa vuota; leggerne uno cancellato
pure. Se cancellare fosse una chiamata a parte ci sarebbero due modi di dire la
stessa cosa e chi legge dovrebbe distinguerli.

Limiti — rifiutati, **non troncati**, con il motivo nel log: nome 1–64 byte,
valore ≤ 256 byte, 16 campi per cella, 65536 celle per mondo. Un valore tagliato
in silenzio è un valore che si rilegge come qualcos'altro.

### 2.2 Rompere un blocco si porta via lo stato, cambiarlo no

La regola sta in `write_block`, cioè in **un** posto per entrambe le strade:

```cpp
if (id == kAir) s.node_meta.clear(Cell{x, y, z});
```

Sulla cella che diventa **aria**, non su ogni cambio di id, e la differenza è
reale: un pulsante che passa da `button:up` a `button:down` è la stessa cella
con lo stesso stato, e cancellare a ogni cambio glielo toglierebbe. Quello che
rompere un blocco deve togliere è lo stato, così il blocco piazzato dopo non
eredita quello di prima.

Una `Cell` sola, e non due. `ContainerAt` era già una cella con il suo hash;
adesso è un `using` di `Cell` in `core/voxel/cell.h`, così nessuno dei suoi
novanta punti di chiamata ha dovuto muoversi per smettere di essere una seconda
copia — due hash su tre interi sono due hash che vanno d'accordo finché non se
ne tocca uno.

### 2.3 `core:world_ready`, ovvero: un mod si carica prima del mondo

**Trovato facendo passare il gate, non leggendo il codice.** Il clock leggeva il
proprio conteggio al caricamento e leggeva **zero ogni volta**, e vedendo che
non c'era niente decideva di non aver mai costruito e ricostruiva sopra ciò che
stava per essergli caricato addosso.

L'ordine è questo, e ha una ragione:

    790  mapgen_hash                 (il mondo generato)
    793  compute_lighting
    893  load_dir                    <- i mod girano qui
    920  db.open                     <- il file, controllato contro la tabella FINALE
    927  load_all_chunks
    ...  load_node_meta

Il file del mondo si apre **dopo** che i mod hanno registrato i loro blocchi,
perché l'hash di contenuto contro cui viene verificato è la tabella finale — un
server che aprisse prima scriverebbe un file che poi si rifiuta di riaprire.
Quindi un mod che guarda il mondo mentre si carica guarda quello del mapgen.

Da qui `core:world_ready`: senza campi, emesso **una volta sola**, dopo che il
mondo è quello che sarà, e **anche quando non c'è nessun file del mondo** — un
mod che lo aspetta non deve aspettarlo per sempre. Sul client in un solo
processo esiste lo stesso e nello stesso punto dell'ordine: un evento che scatta
su un server e non su un client sono due giochi.

È l'equivalente del problema per cui l'engine di partenza ha gli LBM, e infatti risolve solo
metà: un mod sa ritrovare le **proprie** celle se sa dove ha costruito. Trovare
le celle che un *giocatore* ha piazzato serve `find_nodes`, che non c'è.

### 2.4 Otto sabotaggi, uno verde, e una riga che non poteva fallire

| sabotaggio | esito |
|---|---|
| N1 una cella rotta tiene il suo stato | rosso |
| N2 quello che le celle ricordano non viene mai salvato | rosso |
| N3 quello che il file contiene non viene mai riletto | rosso |
| N4 `core:world_ready` non scatta mai | rosso |
| N5 un valore troppo lungo viene tagliato invece che rifiutato | rosso |
| N6 un valore vuoto viene **salvato** invece di cancellare | **verde** |
| N7 svuotare una cella toglie un campo solo | rosso |
| N8 un campo cancellato resta nel file | rosso |

**N6 verde, e la colpa era di una riga che non poteva fallire.** Il controllo
era `meta.get(a, "count").empty()` con la frase «ed è così che un campo si
cancella» — ma un campo *salvato* come stringa vuota si rilegge vuoto anche lui,
quindi quella riga era verde con la cancellazione tolta. Adesso è
`meta.fields() == 0`: quello che distingue i due casi è se il campo c'è ancora,
non cosa restituisce leggerlo.

(La riga accanto, `cells() == 0`, era rossa comunque — quindi il *gate* il
sabotaggio l'aveva preso. A essere verde era la riga che sosteneva di misurare
proprio quella cosa, ed è la stessa forma di M7_FINDINGS 31.7 in scala ridotta:
una frase che dice di leggere una cosa e ne legge un'altra.)

### 2.5 E un gate che misurava il proprio SIGTERM

`portal1.sh` uccideva il server e poi chiedeva se il clock si era ricordato il
conteggio. Non se lo era mai ricordato — quello che scrive il file è il
passaggio allo shutdown, dopo il loop dei tick, e un server ucciso lì non ci
arriva mai. Adesso il server riceve un budget di tick che esaurisce da solo e il
gate lo **aspetta**.

### 2.6 Com'è il gate adesso

    ok    all four mods loaded
          what the four mods still cannot reach:
            button:   MISSING 2 call(s): players, player_pos
            pedestal: MISSING 5 call(s): players, player_pos, give_item,
                                         held_item, take_item
    ok    a body joined the world the mods built in
    FAIL  the pad did not notice a player standing on it
    FAIL  the pad stayed down after the player left
    FAIL  nothing was dispensed: the button's signal reached no vent
    FAIL  the vent never reported dispensing anything
    FAIL  the stand took nothing: the bag went 1 -> 1
    FAIL  walking up to the stand got nothing back (1 -> 1)
    ok    the clock is showing something (13)
    ok    and it counted while the world ran (13 steps)
    ok    the clock went on from the number it stopped at (9)
    ok    and it found its own cell rather than building a second one
    ok    so did the stand
    ok    nothing under src/ names a block, an item or an event of the four

    PORTAL BLOCKS FAILED (6)

Restano cinque chiamate, e sono tutte la stessa cosa: **dove sono i giocatori e
cosa hanno in mano**. Più `find_nodes`, che nessuno dei quattro mod nomina più
perché sanno dove hanno costruito — ma che serve al primo blocco che un
giocatore piazza.

---

## 3. Il costo di dirlo a qualcuno, misurato su un link cattivo

La chiusura di OPEN-17 — «un blocco che un mod cambia adesso arriva a chi c'è» —
ha fatto diventare rosso `bad_link`, il test d'uscita di M6. Non una volta:
**quattro run rosse su cinque**, contro un tasso storico di circa una su sei
(`TODO.md` OPEN-9).

Isolato, non ipotizzato: con la sola coda disattivata e tutto il resto in
piedi, **dieci run su dieci verdi**. La causa era mia.

### 3.1 Un canale tutto loro, che da solo non basta

Il primo indizio era nel log della run rossa:

    heartbeats: longest gap 4.09 s across 2 session(s), 13 lost

Tredici heartbeat persi. E il commento su `Channel::Media` in `protocol.h`
descrive esattamente questa forma, scritto da M6:

> *Its own channel because ENet orders reliable messages WITHIN a channel: a
> 4 KB texture that lost a fragment holds up everything queued behind it, and on
> the event channel that included the heartbeat.*

I mod scrivono di continuo, e dal momento in cui quelle scritture hanno
cominciato a partire, il canale Event portava un messaggio affidabile per ogni
cella che un mod nominava — con l'heartbeat in coda dietro di loro.

Quindi `Channel::Block`, protocollo 19 → 20. **Entrambi i tipi di cambio sullo
stesso canale**, e non è un caso: due cambi alla stessa cella devono arrivare
nell'ordine in cui sono avvenuti, ed ENet ordina dentro un canale e non fra
canali — la scrittura di un mod su un canale e l'edit di un giocatore su un
altro sono una cella che finisce col tenere il più vecchio dei due.

**E da solo non è bastato.** La prima versione del commento su quel canale
diceva «con questo canale e nient'altro, cinque verdi» — scritta *prima* delle
run che l'avrebbero verificata. Erano tre rosse su cinque. Il commento adesso
riporta la progressione misurata:

| stato | rosse |
|---|---|
| ogni scrittura, su `Channel::Event` | 4 su 5 |
| + canale suo | 3 su 5 |
| + solo i cambi veri | rosse |
| + salta le sessioni che non hanno il chunk | 3 su 8 |
| + quello che una sessione è in debito aspetta mentre le arriva il mondo | **0 su 8** |

### 3.2 «Scrivi X» non è «X è cambiato»

`mods/spike` chiama `set_block` sulla stessa cella **ogni tick** e la cambia due
volte al secondo: un mod dice quello che *vuole* che una cella sia, non tiene un
diff. Quindi è l'engine il lato che deve sapere se è un cambio.

```cpp
bool write_block(ServerState &s, int x, int y, int z, BlockId id)
{
    if (s.world.get_block(x, y, z) == id) return false;
    ...
}
```

Ottanta messaggi al secondo diventano dieci. Misurato: 101 cambi veri su 200
tick contro ~800 scritture.

### 3.3 Il mondo prima della chiacchiera

Le due regole che hanno chiuso il buco, e sono entrambe **su una sessione sola**:

1. **Una cella dentro un chunk che quella sessione non ha ancora ricevuto non le
   è dovuta**: il chunk è codificato dal mondo com'è adesso, quindi il cambio è
   già dentro. Dirglielo lo stesso è un messaggio che costa allo stream con cui
   sta competendo. Il commento che stava lì — *«il filtro costerebbe più di
   quello che risparmia»* — era scritto quando un cambio di blocco succedeva
   perché un giocatore cliccava.
2. **Una sessione a cui sono stati mandati chunk in questo tick è una sessione a
   cui il mondo non è ancora arrivato**, e il mondo viene prima. Quello che le è
   dovuto **aspetta, non viene buttato**: una coda per sessione, deduplicata,
   svuotata al primo tick in cui non le sta arrivando niente.

Le vittime nelle run rosse erano proprio quelle: la finestra-osservatore verso
l'altro server non finiva mai di ricevere il mondo di là (`0 vertices`,
«that server says 0.00 0.00 0.00»), e il giocatore veniva corretto contro una
posizione rivendicata mentre il mondo era ancora per strada.

### 3.4 Gate, e tre sabotaggi

`tests/mod_write.sh` legge **due numeri** che il server scrive uscendo — quanti
cambi ha detto e quanti ne ha trattenuti — perché sono le due metà della regola:
una run in cui non si è mai trattenuto niente non ha provato «il mondo prima», e
una in cui si è trattenuto senza mai dirlo lo ha perso.

E per distinguere *trattenuto* da *buttato* serve una cella che cambia **una
volta sola**: una che continua a cambiare si rimette a posto da sé e nasconde il
cambio perduto. Il mod trasforma il primo click destro di chiunque in quel
cambio, e il probe clicca mezzo secondo dopo essere entrato — dentro la finestra
in cui ha già il chunk ma il mondo non è finito di arrivare.

| sabotaggio | esito |
|---|---|
| W1 il mondo non viene prima | rosso |
| W2 quello che è dovuto viene buttato invece che trattenuto | rosso |
| W3 nessun filtro sul chunk | **verde, ed è giusto così** |

W3 è verde di proposito: costa messaggi e non cambia nessuna risposta, come la
deduplicazione. Ci sono voluti tre tentativi per rendere W2 rosso, e i due
fallimenti erano informativi: la prima volta il gate leggeva la riga sbagliata,
la seconda il click arrivava *prima* che la sessione avesse il chunk — e allora
il chunk portava il cambio da sé e buttare la coda era davvero invisibile.

### 3.5 E due gate che misuravano il proprio segnale di stop

`portal1.sh` uccideva il server e poi chiedeva se il clock si era ricordato il
conteggio (§2.5). `mod_write.sh` ha fatto la stessa cosa un'ora dopo, leggendo i
due contatori dal log di un processo a cui aveva appena mandato un segnale:
zero e zero. Quello che scrive quei numeri è il passaggio all'uscita, e un
server ucciso lì non ci arriva mai. Entrambi adesso **aspettano**.

---

## 4. Il generatore impara due forme, e il pulsante si preme

Dei sei controlli rossi rimasti, **quattro cadevano su due chiamate sole**:
`players` e `player_pos`. E nessuna delle due si poteva scrivere.

### 4.1 «Scalari dentro, uno scalare fuori» non basta più

`tools/api_gen` parlava solo scalari: argomenti `int`/`float`/`string`/`bool` e
**un** valore di ritorno. `players()` è una lista. `player_pos()` sono tre
numeri.

L'alternativa era scriverle come scalari — `player_count()`, `player_at(i)`,
`player_x/y/z(id)`: **cinque chiamate dove ne bastano due**, e ogni futura
chiamata che risponde con più di un numero avrebbe ripagato lo stesso prezzo.
Quindi il generatore ha imparato due forme:

```lua
fn.ret  = "int[]"                        -- una lista, come array Lua
fn.rets = { "float", "float", "float" }  -- più valori
```

Entrambe sono **out-parameter** dal lato C, perché un puntatore a funzione C non
può restituire un vettore: una lista riempie il buffer di chi chiama e
restituisce quanti ne ha scritti, più valori si scrivono attraverso puntatori con
un 1 o uno 0 che dice se c'era una risposta.

### 4.2 Niente, e non lo zero

La regola per cui la forma «più valori» esiste in quel modo:

```lua
local x, y, z = core.player_pos(id)
if x == nil then continue end   -- se n'è andato fra le due chiamate
```

Un giocatore che non c'è **non ha una posizione**. Se ne restituisse una,
sarebbe `0, 0, 0` — e l'origine è un posto vero: un mod che ci misurasse una
distanza troverebbe tutti in piedi sul pulsante nell'angolo del mondo. Il tipo
generato lo dice: `(number?, number?, number?)`.

Stessa regola sul server: `host_player_pos` non risponde per una sessione che è
entrata e non si è mai mossa (`MovementState::valid`), e restituisce la
posizione che l'autorità del movimento ha **accettato** — non l'ultima
rivendicata da un client, o si potrebbe stare su un pulsante dall'altra parte del
mondo dicendo di esserci.

Gli osservatori non compaiono in `players()`: un portale che guarda dentro non ha
avatar né inventario (DESIGN §4.4), quindi non c'è nessuno da mettere su un
pulsante.

### 4.3 Cinque sabotaggi, uno verde e dichiarato

| sabotaggio | esito |
|---|---|
| A1 il conteggio dichiarato dall'host viene creduto | rosso |
| A2 nessuna risposta diventa tre zeri | rosso |
| A3 una lista senza risposta è `nil` invece che vuota | rosso |
| A4 il server dice che il mondo è vuoto | rosso |
| A5 una sessione che non si è mai mossa ha una posizione | **verde** |

**A1 è stato verde al primo tentativo**, e la colpa era del sabotaggio: l'host
dichiarava dodici elementi su un buffer da trentadue, e l'asserzione «non ne sono
tornati più di trentadue» era vera anche senza il clamp. Con la bugia portata a
sessantatré è rossa. Si controlla la **lunghezza** e non il contenuto: il glue
non può sapere che l'host ne ha scritti meno di quanti ne dichiara — il buffer è
azzerato, quindi gli elementi in più sono il giocatore 0, che non è nessuno. Il
lavoro del clamp è solo che non si legga oltre la fine del buffer.

**E due sabotaggi sono stati verdi perché non compilavano.** Lo script mandava
gli errori di build su `/dev/null` e riportava verde su un binario vecchio. Ora
dice «build failed» invece di mentire — la stessa forma di §1.6 e §2.4: un
rapporto che dice di leggere una cosa e ne legge un'altra.

**A5 verde, e dichiarato invece che nascosto.** Togliendo il controllo su
`MovementState::valid`, una sessione entrata e mai mossa risponde `0 0 0`.
Nessun gate lo vede: il probe di `portal1.sh` si muove appena entrato, quindi
quella finestra non esiste mai nel test. Servirebbe un probe che entra e sta
fermo, ed è la cosa che **non** ho scritto — detto qui.

### 4.4 Com'è il gate adesso

    ok    the pad went down while somebody stood on it (9 -> 10)
    ok    and came back up when they stepped off
    ok    the vent dropped a cube on another mod's signal (1 on the ground)
    ok    and the vent is what says so, not the engine
    FAIL  the stand took nothing: the bag went 1 -> 1
    FAIL  walking up to the stand got nothing back (1 -> 1)

**12 verdi, 2 rossi.** Il pulsante si preme, il segnale attraversa da un mod a un
altro, il vent lascia cadere un cubo. Restano tre chiamate, tutte sulla borsa di
chi sta davanti al pedestal: `held_item`, `take_item`, `give_item`.

---

## 5. La mano, la borsa, e il test d'uscita che passa

Gli ultimi due rossi erano il pedestal: prendere quello che gli si offre e
restituirlo a chi si avvicina.

### 5.1 `held_item` non è stata scritta, e il motivo è che avrebbe mentito

La lista di §1.2 chiedeva `held_item(player)`. Non esiste, ed è la decisione
giusta: **il motore non sa cosa uno ha in mano se non nel momento del click.**

`Inventory` tiene uno slot selezionato, ma sul server `select()` viene chiamato
in un posto solo — `edit_done`, cioè quando un piazzamento è **riuscito**.
Quindi fra due piazzamenti lo slot selezionato è quello dell'ultimo, non la
mano. Una `core.held_item(player)` avrebbe risposto con qualcosa che era vero un
po' di tempo fa, che è peggio che non rispondere.

Quindi la mano viaggia **sull'evento**, dove il motore la conosce davvero:
`core:used` ha un campo in più, `held`, il nome di quello che chi ha cliccato
aveva in mano — vuoto a mani vuote. Due chiamate invece di tre, e la terza
domanda risposta dove il motore ha la risposta.

`EditPlan::use_holding` viene riempito **prima di ogni rifiuto**, così un mod che
reclama un click che sarebbe stato rifiutato comunque sa lo stesso cosa gli
veniva offerto.

### 5.2 `take_item` e `give_item`, e quanti se ne sono mossi davvero

Per **nome**, come `meta_*`, e per la stessa ragione. Tutt'e due restituiscono
quanti se ne sono mossi: una borsa può non avere quello che si chiede, e può
essere piena.

Quello che non entra con `give_item` **non viene lasciato cadere**. Non gli
succede niente, e decide il mod — `core.drop_item` lo mette per terra, non fare
niente lascia al giocatore il compito di fare spazio. Un motore che lo buttasse
deciderebbe per tutti i mod insieme. Il pedestal usa proprio questo: la cella
lascia andare quello che tiene **solo** se la borsa l'ha preso, così una borsa
piena lascia la gun sul piedistallo invece di farla sparire.

E chi ha la borsa viene avvisato: `bags_touched`, svuotata nel tick, manda
l'inventario a chi un mod ha toccato — altrimenti un giocatore continua a essere
disegnato con in mano quello che gli è stato tolto.

### 5.3 Quattro sabotaggi, uno verde e dichiarato

| sabotaggio | esito |
|---|---|
| B1 `core:used` si dimentica la mano | rosso |
| B2 `take_item` dichiara una presa che non ha fatto | rosso |
| B3 la borsa che un mod ha cambiato non viene mandata | rosso |
| B4 `give_item` conta anche quello che non è entrato | **verde** |

B2 è stato verde al primo giro e la colpa era del rapporto, non del gate: il
grep leggeva la riga sbagliata. Terza volta in questa milestone (§1.6, §2.4).

**B4 verde, e dichiarato.** Contare quello che non è entrato cambia risposta solo
con una **borsa piena**, e nel gate la borsa del probe ha un oggetto e nove
caselle. Servirebbe un probe che si riempie la borsa prima di andare al
piedistallo; non l'ho scritto.

### 5.4 E due volte il gate misurava sé stesso

Il pedestal funzionava da due run prima che il gate lo dicesse.

- Il probe cliccava dal centro della piazza, che è **esattamente** due nodi dal
  piedistallo — cioè dentro il raggio con cui il piedistallo consegna. Prendeva e
  ridava al tick dopo: la borsa faceva 1 → 1 → 1 e il gate leggeva «non ha preso
  niente», mentre il log diceva che aveva fatto tutte e due le metà. Un giocatore
  può *cliccare* a sette nodi e il piedistallo consegna a due: il probe deve
  offrire **a distanza di braccio**.
- Spostato a quattro nodi dietro il piedistallo… che è la cella del clock. Una
  mossa dentro un blocco solido viene rifiutata, quindi il probe è rimasto dov'era
  ed è successo di nuovo. Ora si sposta **di lato**.

### 5.5 M8 passa

    PORTAL BLOCKS OK

Quattordici controlli, headless, e la riga `DISABLED` in `tests/CMakeLists.txt`
è stata tolta. L'ultimo controllo è un `grep`: **nessun file sotto `src/` nomina
un blocco, un item o un evento dei quattro**.

**Non** coperto, e dichiarato in DESIGN §8: «componenti/sistemi da Lua con
iterazione batch» resta la parte non fatta — i quattro blocchi hanno avuto
bisogno di stato per cella, non di entità che un mod possiede. Mancano anche le
**fasi ordinate** (oggi c'è una priorità per evento), i **render pass
registrabili**, i **canali di rete per mod** (il campo esiste nell'handshake da
M3 e nessun mod lo usa) e `find_nodes`.

---

## 6. L'altra metà di M8, quella che il test d'uscita non toccava

M8 si è chiusa sul suo test d'uscita con la milestone **dichiarata aperta a
metà**: i quattro blocchi classici dei puzzle a portali volevano stato per **cella**, non entità
proprie, quindi «componenti/sistemi da Lua con iterazione batch, fasi ordinate,
render pass registrabili, canali di rete per mod» e `find_nodes` non erano stati
scritti. Questa sezione è quella metà.

Stessa forma di §1: **il gate prima del codice**. Due gate, scritti insieme e
`DISABLED` in `tests/CMakeLists.txt` — nominati in ogni summary, senza mentire su
un verde che non avevano.

### 6.1 Il mod che nomina i buchi invece di morire sul primo

`tests/mods_sentry/sentry` è la torretta classica dei puzzle a portali ridotta a quello che una
macchina headless può misurare. Il primo giro:

    sentry: MISSING core.find_nodes      sentry: MISSING core.ecs.kill
    sentry: MISSING core.ecs.component   sentry: MISSING core.ecs.count
    sentry: MISSING core.ecs.phase       sentry: MISSING core.net.open
    sentry: MISSING core.ecs.system      sentry: MISSING core.net.send
    sentry: MISSING core.ecs.spawn
    sentry: MISSING core.ecs.set

**Dieci righe e non una.** Un mod che muore sulla prima chiamata assente nomina
un buco e ne nasconde nove, e il gate poi diventa verde un buco alla volta senza
che nessuno sappia quanta strada resta. Quindi il mod cerca ogni chiamata che gli
serve *prima* di usarla, e ogni assenza è una riga nel log con il nome dentro.

Un dettaglio che è costato subito: `core.event.on("core:net_message", …)` sta
**dietro** il controllo su `net.open`, non accanto. Ascoltare un evento che il
motore non ha dichiarato è un errore che **disabilita il mod** — e con il mod
disabilitato le altre nove righe non vengono mai stampate. Il primo giro del gate
diceva `0 mods running`.

4 verdi / 9 rossi al primo giro. 16 verdi alla fine.

### 6.2 `find_nodes`, e il rifiuto che nessuno guardava

Le celle che un mod **non ha piazzato**: l'equivalente di un LBM dell'engine di partenza. Un
mod che ha messo giù i propri blocchi poteva anche solo ricordarsi dove; questo
serve per quelli che ha messo giù un *giocatore*, o il mapgen.

Scritto **una volta sola** in `src/core/voxel/find.cpp` e chiamato dai due main.
Server e client fanno girare tutt'e due i mod, e una regola scritta due volte è
due regole — che in questo repo è la causa, non una nota di stile.

Triple piatte e non una tabella per cella: una tabella per cella è
un'allocazione per cella, sull'unica chiamata che un mod fa su tutta una regione.
`#found // 3` è quante sono.

**Due rifiuti**, tutt'e due dichiarati e tutt'e due nel gate:

- un nome che nessuna registry ha. Rispondere «non ce ne sono» a un refuso è un
  mod che non fa niente e una segnalazione che dice che la ricerca è rotta.
- una scatola di più di 1 048 576 celle. La ricerca gira **dentro il tick**, e il
  budget di interrupt che ferma un mod impazzito conta i safepoint nella VM, non
  il tempo che il motore spende per conto suo.

Tutt'e due restituiscono una **lista vuota** — in una lista non c'è posto per una
frase — quindi l'unica cosa che li distingue da «lì non c'è niente» è la riga che
scrive il motore.

**E questo l'ha trovato un sabotaggio verde.** F3 toglieva il rifiuto sul nome
sconosciuto e il gate restava verde: cercare `sentry:turret` risolve a un id
vero, il ramo `kAir` o c'è o non c'è e nessun controllo lo guardava. I due
controlli sui rifiuti esistono per questo.

| sabotaggio | esito |
|---|---|
| F1 `find_nodes` non trova mai niente | rosso |
| F2 la scatola è ignorata, si cerca in tutto il mondo | rosso |
| F3 un nome sconosciuto vale aria invece di essere rifiutato | **verde**, poi rosso |
| F4 il rifiuto sulla dimensione della scatola sparisce | rosso |

### 6.3 Entità che un mod possiede, e una chiamata sola per tutte

```lua
core.ecs.component("sentry:pellet", { "vx", "vy", "vz", "age" })
core.ecs.phase("sentry:motion", 100)
core.ecs.system("sentry:fly", "sentry:motion", "sentry:pellet", function(b) … end)
```

Tre cose lo rendono diverso da «un mod tiene una tabella Lua e la scorre in
`on_step`»:

**Sono le entità del motore.** Un pellet è un'entità flecs con una `Position`,
nel mondo dove stanno i mob e le pile per terra. Un secondo magazzino di entità
sarebbe **due spazi di id per una sola idea**, che è la forma in fondo a quasi
tutti i difetti di questo repo.

**Un sistema è chiamato una volta per molte entità**, con colonne di numeri che
indicizza e in cui riscrive — structure of arrays, perché dall'altra parte Lua
riceve array. 512 entità, una chiamata. Una chiamata per entità spende più tempo
ad attraversare il confine che dentro.

**I sistemi girano in fasi ordinate da un numero**, così due mod che non si sono
mai visti girano in un ordine dichiarato invece che nell'ordine in cui si sono
caricati. Il mod del gate registra i suoi due sistemi **al contrario** apposta.

Scritte a mano accanto a `core.event` invece che generate, e per due motivi: una
di loro prende una **funzione**, e gli id sono **double** — un id di entità porta
una generazione sopra l'indice, quindi `luaL_checkinteger` lo troncherebbe in
silenzio la prima volta che qualcosa viene riciclato.

Dichiarati e non scoperti dopo: un sistema nomina **un** componente e riceve chi
ce l'ha, non c'è join fra due; i campi sono numeri; un batch è al massimo 4096
entità, perché un mod con centomila costruirebbe centomila numeri Lua per colonna
dentro una quota da 32 MB; e le entità di un mod vivono nel mondo che possiede la
**tabella delle sessioni**, quindi un run offline (`--ticks` senza `--listen`) non
ne ha — è un run di hash del mapgen, non una partita.

`ModHost` ha guadagnato tre porte perché nessuno dei due strati dovesse imparare
l'altro: `enter_vm`/`leave_vm` (chi sta girando), `disable_vm` (cosa costa un
sistema che va in errore) e `set_vm_gone` (lasciare andare). `forget_vm` ha
sostituito cinque copie di «togli gli handler, poi distruggi».

### 6.4 Due sabotaggi verdi, e tutt'e due erano colpa del gate

| sabotaggio | esito |
|---|---|
| E1 le fasi sono ignorate, si va in ordine di registrazione | rosso |
| E2 una entità per chiamata | **verde**, poi rosso |
| E3 le colonne non vengono mai riscritte | rosso |
| E4 `kill` restituisce vero e non fa niente | **verde**, poi rosso |

**E2.** Il controllo confrontava il numero di chiamate della popolazione più
grande con quello della più piccola, presi da **righe diverse**. Con una entità
per chiamata i due estremi cadevano sullo stesso tick e concordavano fra loro.
Adesso i due numeri escono dalla **stessa riga** — quante entità ha fatto un tick
e quante chiamate gli sono costate — e le due dimensioni devono essere diverse.

**E4.** Il mod contava le uccisioni che **chiedeva**: con `kill` che non fa niente
le richiedeva ogni tick e riportava 165 952 pellet «raccolti». Quello che
distingue un kill che ha funzionato da uno che restituisce vero è quanti pellet
sono **nati**: una popolazione tenuta a 512 ha bisogno di più di 512 spawn solo
se i morti se ne sono davvero andati.

`tests/lua_ecs_test.cpp` è la parte che un mod non può raggiungere: la forma di
un componente che appartiene a chi l'ha dichiarato, una fase a due numeri che
nomina tutt'e due i mod, un sistema su una fase che nessuno ha dichiarato.

### 6.5 Un canale per mod, e il campo che c'era da M3

L'handshake porta la tabella dei canali dei mod **da M3** e nessun mod ne ha mai
usato uno: `server_mod_channels` restituiva `{}` con un commento che diceva che
sarebbe stata riempita quando `core.net.channel` fosse esistita.

    core.net.open()             il numero, chiesto mentre il mod si carica
    core.net.send(player, s)    0 vuol dire tutti in questo mondo
    core:net_message            { player, mod, text }, sullo stesso bus

Il messaggio **non porta il nome del mod**: il nome è il **canale**. È quello che
l'handshake ha distribuito, e un nome nel payload sarebbe una seconda risposta
alla domanda «di chi è questo» che può discordare dalla prima.

Chiesto **al caricamento** e rifiutato dopo, con una frase, perché i numeri
escono nel welcome: un mod che chiedesse dopo il primo client collegato avrebbe
un numero che a quel client non è mai stato detto. La lista che conta
`host_net_open` è la **stessa** da cui si costruisce il welcome, quindi il numero
dato a un mod e quello detto a un client non possono differire.

Protocollo **20 → 21**: `MsgType::ModData`, una stringa con un tetto di 512 byte.
Il tetto è del messaggio e non di un mod — un peer che può far tenere all'altra
parte quello che gli pare ha trovato il modo di spendere memoria gratis.

**Non trattenuto dietro il mondo** come lo è un cambio di blocco. Il messaggio di
un mod parla dello stato del mod, non di una cella che il client potrebbe non
avere ancora, e trattenerlo sarebbe il motore che ha un'opinione su una
conversazione che non legge.

Dal lato client il numero è **del server**, quindi `core.net.open` non risponde
niente finché non c'è una connessione e un mod client non ne ha bisogno: manda
con `core.net.send` e sente con l'evento, e tutt'e due cercano il canale quando
vengono usati. `player` è ignorato da quella parte, e non è una dimenticanza: un
client parla al **server**, e lasciargli indirizzare un altro giocatore sarebbe
un client che decide chi riceve messaggi.

Quattro sabotaggi, tutti rossi: dare a un mod uno dei canali del motore, buttare
quello che arriva su un canale di mod, non svuotare mai quello che un mod ha
detto, mandare la risposta sul canale eventi del motore.

**Non coperto e dichiarato:** il rifiuto per un mod che chiede un canale *troppo
tardi*. Servirebbe un mod che chiama `net.open` da dentro un tick, e il mod di
questo gate la chiama dove va chiamata.

### 6.6 Un mod che disegna

```lua
core.render.pass("hologram:glow", 100, function()
    core.render.box(x, y, z, w, h, d, r, g, b, a)
end)
```

Quello che lo rende un **pass** e non l'evento `core:tick` che esiste già: un
handler di tick gira **fra** i frame, senza camera montata e senza niente in cui
possa disegnare — e gira **una volta per frame**, mentre un portale disegna
questo mondo di nuovo da un'altra camera. Un ologramma disegnato da un tick
sarebbe assente attraverso ogni apertura. Un pass gira dentro il render, **una
volta per camera**, e disegnare è l'unica cosa che può fare.

`core.render.box` fuori da un pass viene **rifiutata e lo dice**. Non «non
succede niente»: fuori da un pass non c'è camera montata né target in cui si stia
disegnando, quindi la scatola finirebbe dove il frame precedente ha lasciato le
cose — un mod che disegna dentro il render di qualcun altro senza mai scoprire
perché.

Il numero ordina i pass **fra di loro**, non contro il disegno del motore. Detto
esplicitamente perché la cosa ovvia da assumere è il contrario: c'è **un** punto
nel frame dove i mod disegnano, dopo il mondo opaco e i corpi e prima delle
aperture.

Su un server i due puntatori sono nulli e il binding dice «questo processo non
disegna» invece di fallire con una frase su una chiamata che «non ha funzionato».
Chi scrive un mod su un server va detto su che tipo di processo si trova, non
mandato a cercare un bug.

**Il controllo viene prima**, ed è quello che dà senso al numero: due frame di una
scena che non si muove, senza nessun mod che disegna, devono essere lo stesso
frame. Differiscono di **0 pixel**; il pass ne cambia **108 666 su 2 073 600**.
Sono controllati tutti e due i lati — un pass che lasciasse le matrici della
camera in uno stato contro cui il resto del frame poi disegna cambierebbe **ogni**
pixel, che è anch'esso «qualcosa che non c'era».

Quattro sabotaggi, tutti rossi: pass mai chiamati (0 pixel), numero d'ordine
ignorato, guardia «solo dentro un pass» tolta, e un pass disegnato contro una
camera che non è quella in corso (0 pixel — la scatola finisce fuori vista).

**E `portal1` è andato rosso su questo commit prima che fosse scritto bene:** un
*commento* nominava la directory del fixture dei blocchi a portali, e quel gate cerca
esattamente quella stringa sotto `src/`. La regola è giusta; il commento è stato
riscritto.

### 6.7 Dove sta M8 adesso

    SENTRY OK       16 controlli, headless
    MOD PASS OK      6 controlli, gpu
    PORTAL BLOCKS OK  14 controlli, headless

Suite **51/51**, niente disabilitato. Le cinque cose che DESIGN §8 dichiarava
aperte — componenti e sistemi da Lua con iterazione batch, fasi ordinate, render
pass registrabili, canali di rete per mod, `find_nodes` — ci sono tutte.

Quello che resta aperto è scritto in `TODO.md` e non qui.
