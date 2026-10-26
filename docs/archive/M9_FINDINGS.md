# M9 — Performance e robustezza: cosa è stato costruito e cosa si è imparato

Milestone M9 di `DESIGN.md` §8: *meshing multi-thread a budget, occlusion
culling, LOD lontano, reverse-Z, profiler in-game, crash handler, migrazione dei
salvataggi.*

**Chiusa dal suo test d'uscita.** Sotto c'è solo ciò che è stato costruito e
verificato da esecuzioni. I numeri vengono dalle run, non dalle stime.

---

## 1. La milestone senza numeri, e il gate scritto prima

M9 era l'unica voce del piano con una lista di feature e **nessun numero**. Le
altre milestone hanno un test d'uscita in DESIGN; questa aveva sette nomi di
tecniche e niente che dicesse quando fossero finite.

Quindi il primo lavoro di M9 non è stato codice: sono stati tre gate, scritti
prima, con i numeri del giocatore e non quelli che l'engine produce. **16.7 ms**
è un sessantesimo di secondo. **50 ms** è uno scatto che si vede.

| Gate | Cosa chiede |
|---|---|
| `frame_budget` (gpu) | cosa costa un frame mentre il mondo si muove, su 512 chunk, in fasi |
| `crash_evidence` | un processo che muore dice di quale segnale, lascia uno stack, e **muore lo stesso** |
| `migration` | un mondo scritto da una build precedente si apre e tiene quello che aveva; uno da una successiva è rifiutato per nome |

Il motivo è scritto in DESIGN accanto alla milestone: M7 ha chiuso con il suo
test d'uscita scritto per ultimo, e la prima run ha trovato ventidue minuti di
giocatore che cadeva fuori dal mondo.

---

## 2. Migrazione: la versione si legge prima di toccare il file

`kSchemaVersion` era un numero locale al file confrontato con sé stesso: un
controllo che non poteva fallire. Adesso è `kWorldSchemaVersion` in `world_db.h`
e la versione si legge **prima** che lo schema di questa build tocchi il file.

Il punto non è aprire i file vecchi — è che un'apertura **rifiutata non deve
cambiare il file che ha rifiutato**. Il gate lo misura contando le tabelle: dopo
un rifiuto il file ne ha esattamente una.

E il valore vero del bump 1 → 2 è la direzione opposta a quella che sembra: una
build pre-M8 adesso **rifiuta** un file che questa ha scritto, invece di aprirlo,
non conoscere `node_meta`, e lasciare in giro la memoria di una cella che il
blocco successivo eredita.

Quattordici controlli, cinque sabotaggi. Uno dei cinque è stato rosso per il
motivo sbagliato — spostare tutta la chiamata dello schema sopra la lettura
rompe anche i file **nuovi** — ed è stato ristretto al solo percorso di rifiuto.

---

## 3. Crash handler: nominare il segnale, scrivere lo stack, e morire lo stesso

Il server non aveva **niente**, nemmeno per SIGQUIT. Ora `install_crash_handler()`
nomina il segnale, scrive lo stack con `backtrace_symbols_fd`, rimette la
disposizione di default e **rifà `raise`**: un handler che inghiotte SIGSEGV è
peggio di nessun handler, perché toglie il codice di uscita a chi guarda da
fuori.

Due dettagli che non sono dettagli: `sigaltstack`, perché il modo più comune di
prendere un SIGSEGV è finire lo stack e un handler senza stack proprio non parte;
e `-rdynamic`, senza il quale lo stack è una colonna di indirizzi.

**E il gate ha trovato due difetti miei.** `--play-seconds` arrivava solo a
`run_network`: sul loop a processo singolo non faceva niente, e il client girava
finché non chiudevi la finestra. Il gate aspettava i processi **senza limite**,
quindi è rimasto appeso venti minuti. Un gate deve fallire, non appendersi: ora
le attese sono limitate a dieci secondi e sopravvivere è di per sé un
fallimento. La prima versione limitata leggeva `$?` dopo un test `[` invece che
dopo il `wait`, quindi tornava sempre 0 e il controllo passava per un processo
uscito pulito.

Quell'appeso ha prodotto quello che OPEN-6 aspettava da M7 — lo stack di un
client che non usciva:

    main -> run_interactive -> ui::draw_hotbar -> gfx_2d_text -> DrawTextEx

In esecuzione, non bloccato. **Non è OPEN-6**: è il bug di `--play-seconds`, e
tutte le run registrate in quella voce usavano `--connect`. È scritto lì lo
stesso, perché il sintomo è identico e chi guarderà il prossimo caso deve
escluderlo per primo.

---

## 4. Il profiler: le parti devono fare il tutto

DESIGN chiede «profiler in-game» e non dice cosa sia uno. La proprietà è stata
scritta prima: **tutto ciò che ha un nome, più tutto ciò che non ce l'ha, è il
frame** — esatto, come aritmetica.

È la sola cosa che conta. Un profiler che mostra quattro sezioni che sommano al
60% di un frame da 20 ms punta il dito sulla più grande delle quattro mentre gli
8 ms che nessuno ha strumentato sono dove il frame è andato davvero. Quindi il
resto è **calcolato** qui — `unattributed_ms` — e non lasciato a chi legge; e un
frame senza strumentazione torna al 100% non attribuito invece che come una
lista vuota, che si legge come un certificato di buona salute.

Il tempo sommato è quello **proprio**, non quello inclusivo: una sezione è
addebitata del tempo in cui era aperta **meno** quello dei figli, quindi padre e
figlio non reclamano mai gli stessi millisecondi. Entrambi i numeri sono
riportati, perché «il meshing costa 9 ms» e «il meshing costa 1 ms e l'upload
dentro di lui ne costa 8» sono risposte diverse.

Ventinove controlli, sei sabotaggi. Un thread solo, dichiarato: quando il
meshing è passato al worker, quello che questa riga misura è l'**attesa** del
thread del frame, che è il numero di cui parla un budget.

**F6** lo disegna in gioco, e dice di quale frame: questo non è finito finché
l'overlay stesso non è stato disegnato.

### Il numero che ha deciso il resto della milestone

Con il profiler acceso, un chunk su un mondo da 512 nodi:

    snapshot   0.32 ms   legge il World, deve girare dove sta il World
    build      3.12 ms   legge uno snapshot e il registry e nient'altro
    upload     0.00 ms   è una chiamata GL, non lascia mai questo thread
    mesh                 3.45 ms in totale

**Il 90% del meshing è la parte che può lasciare il thread.** Prima del profiler
niente qui poteva dirlo, e la scelta fra thread e LOD sarebbe stata un'opinione.

---

## 5. Tre difetti trovati dal profiler nella prima ora

**Il budget si fermava a «già speso».** `sync()` ha preso un budget in
millisecondi, e la prima misura ha detto **7.64 ms contro 4.0 dichiarati**: una
chiamata a 3.9 ms ne cominciava un altro da 3 ms e finiva a 6.9. Un budget che
si sfora del novanta per cento non è un budget. La correzione non è una soglia
più larga ma la domanda giusta — *ci sta il prossimo?*, stimato sui chunk di
questa stessa chiamata. **4.09 ms.**

**Il controllo leggeva il tempo sbagliato.** Quando `mesh` ha preso dei figli, il
suo tempo **proprio** è diventato la contabilità intorno a loro: il controllo del
budget passava verde su **0.02 ms** mentre il meshing costava 3.45. L'ha preso la
metà della coppia che chiede al frame quieto di essere diverso dalla tempesta —
un numero sempre piccolo passa uno dei due controlli e mai entrambi.

**Il budget imposto al frame loop pareggiava i due mondi.** Con il build ancora
sul thread del frame, 4 ms compravano esattamente un chunk, che è anche il
minimo del mondo passivo. `dual_session` l'ha detto con le parole con cui era
stato scritto: *«one tick: the active world meshed 1 chunks, the passive 1»*. Il
frame loop è tornato al conteggio finché il build non è uscito dal thread.

---

## 6. Meshing su thread

`MeshPool`: i worker prendono uno snapshot e un registry, eseguono
`mesh_snapshot_into` senza tenere nessun lock, e restituiscono i vertici. I
buffer fanno il giro e vengono **scambiati** in entrambe le direzioni, perché
uno snapshot da 118 KB e due vettori di vertici per chunk per frame logorano
l'allocatore abbastanza da vedersi come una salita lenta della memoria residente:
glibc l'arena non la restituisce.

Il registry **non è sotto lock** e non lo sarà: un lock intorno alla ricerca di
un nodo è un lock preso un milione di volte per chunk. La regola è che niente sia
in volo mentre cambia, e `drain()` è come ci si arriva — un reload di mod è chi
ne ha bisogno.

| | prima | dopo |
|---|---|---|
| tempesta p50 | 6.79 ms | 1.83 ms |
| tempesta p99 | 8.15 ms | 2.38 ms |
| tempesta, peggior frame | 8.44 ms | 4.23 ms |
| un blocco per frame, p50 | 3.86 ms | 0.76 ms |
| il mondo recupera | **mai** in 240 frame | frame **130** |

### La guardia sullo stale, e il gate che non la esercitava

Un chunk modificato mentre viene meshato è sottomesso due volte, due worker
corrono, e se il risultato vecchio è caricato per secondo il giocatore rivede il
blocco che ha appena rotto. La guardia sta nel pool e non in chi lo chiama,
perché di chiamanti ce ne sarà più d'uno.

Il primo gate non la esercitava: sottomettendo due volte, i due tornavano
**nell'ordine in cui erano partiti** e alla guardia non veniva chiesto niente. La
corsa va **fatta accadere**, e con lavoro, non con sleep: un chunk a scacchiera
ha una faccia visibile su ogni lato di ogni blocco e batte il greedy meshing,
quindi ci mette molto più di un chunk d'aria. Lento poi veloce, 24 volte:
**48 sottomissioni, 42 tornate, 6 scartate come vecchie**. Anche il conteggio
degli scartati è controllato, perché se non se ne scarta mai nessuno allora «i
numeri vanno avanti» è vero anche di un pool senza guardia.

### Il sabotaggio che non è andato rosso, ed è servito di più degli altri

Con `submit()` costretto ad accettare lavoro su un pool fermo, il gate non è
fallito: si è **appeso**, ed è stato ucciso dal suo timeout senza niente da dire.
Lo `stop()` del distruttore aspettava un conteggio che nessun worker era rimasto
a far scendere. `drain()` adesso torna quando non ci sono thread: **aspettare
lavoro che nessuno può fare è un deadlock**, e un gate deve fallire, non
appendersi. Per la stessa ragione `forget()` aspetta da sé invece di dire al
chiamante di farlo: «chiama drain prima» è una regola che vale finché qualcuno
non lo fa.

### Due mondi hanno gli stessi ChunkPos

Con una coda sola per tutti, il renderer di una sessione prende quello che ha
finito per primo e lo carica a quella posizione **nel proprio** mondo; con una
tabella dello stale sola, il mesh fresco di un mondo viene buttato perché l'altro
ha sottomesso dopo. `(0,0,0)` è un chunk in tutti e due.

Il primo controllo confrontava **intervalli** di numeri e tornava verde sotto
entrambi i sabotaggi: quale coda pesca un pool rotto è questione di fortuna, e
gli intervalli uscivano ordinati lo stesso. Adesso si confronta l'**identità** —
cosa era stato promesso a ciascun proprietario al momento della sottomissione — e
si prelevano i risultati in **due ordini diversi**, perché nessuno dei due trova
entrambi i difetti: prelevare a turno smaschera la coda condivisa, prelevare
prima il proprietario più recente smaschera la tabella condivisa.

---

## 7. Occlusion culling: una camminata, non un test

La visibilità non è una proprietà che un chunk ha da solo. Un chunk è visibile se
c'è un modo di arrivarci dalla camera attraverso chunk che si vedono; quindi la
domanda si fa **andando**. Ogni chunk porta quali delle sue sei facce si
raggiungono fra loro — **quindici bit**, un flood fill, calcolato sul worker
dallo stesso snapshot da cui esce la mesh.

Sbagliare nei due versi costa in modo diverso, e l'asimmetria è il criterio di
tutto il file: dire che due facce sono unite quando non lo sono costa draw call;
dire che **non** lo sono quando lo sono è un buco nel mondo — una stanza che il
giocatore vede e che non viene mai disegnata. Dodici casi in
`tests/visibility_test.cpp`, quasi tutti del secondo tipo.

| | prima | dopo |
|---|---|---|
| camera sigillata nella roccia | 129 chunk | **4**, 125 nascosti |
| camera davanti a un muro | 154 chunk | **2**, e non un pixel diverso |

### Tre camere, perché nessuna sola basta

**Sigillata nella pietra** dice che il walk taglia. **Fuori da un muro che
guarda il muro** dice che l'immagine non è cambiata — l'unico controllo che può
prendere il fallimento che conta, un buco nel mondo, di cui nessun tempo di frame
parlerà mai. E **un cunicolo che va solo verso +x**, che è il solo punto dove il
chunk della camera non ha una faccia da cui essere entrata: un sabotaggio che ci
entrava «da -x» come in ogni altro passava tutto il resto.

### Le due cose che ha trovato la misura

**Il walk attraversava i chunk che non conosceva**, e vagava nel vuoto oltre il
bordo del mondo a ogni frame: un frame quieto è passato da **0.42 ms a 2.93**.
Tutti i controlli restavano verdi, perché 2.93 ms stanno in un sessantesimo di
secondo — una soglia che nascondeva un peggioramento di sette volte. Adesso il
walk ha una riga sua nel profiler e un tetto suo: **0.023 ms**.

**E il gate riempiva di pietra con `ensure_uniform_chunk`**, che è la via della
generazione in blocco e lo dice: `if (!m_blocks) m_uniform = id` non fa niente a
un chunk che il mapgen ha già materializzato. Misurava 129 chunk disegnati e
niente occluso, che si legge esattamente come un walk rotto ed era un gate che
non costruiva nessun muro.

### Una premessa del piano che era falsa

«Sottoterra, dentro il terreno che ha fatto il mapgen» **non è** una camera
dietro la roccia a scala di chunk. Questo mondo è alto 64 blocchi e un chunk ne è
32: sono **due strati**, e ogni chunk di quello inferiore arriva al cielo aperto
sopra il terreno. La camera era sepolta e il suo **chunk** no. Le colline da
dieci blocchi del mapgen non sono occlusori a scala di chunk, e il gate adesso
sette chunk interi di pietra se li costruisce.

### Il sabotaggio che ha misurato niente, e l'ha detto

Far unire tutte e sei le facce a ogni regione d'**aria** lascia un chunk di
pietra piena senza nessuna regione, quindi con la tabella a zero: la camera
sigillata non se ne accorgeva. Il sabotaggio giusto è far rispondere
`compute_visibility` sempre «si vede attraverso», e allora il gate va rosso su
tre controlli.

---

## 8. Cosa resta aperto, dichiarato

**LOD lontano e reverse-Z non sono stati scritti.** Erano mezzi, non fini, e il
budget non li ha chiesti: a mondo che si ricostruisce interamente il frame peggio
è **4.23 ms** su 16.7. Il momento di scriverli è quando un numero li chiede — un
mondo più grande di 512 nodi per il LOD, uno z-fighting misurato per il
reverse-Z. Scriverli adesso sarebbe ottimizzare contro un'opinione, che è
esattamente ciò contro cui il profiler è stato scritto.

**Il walk è a grana di chunk.** In un mondo alto due chunk non ha quasi niente da
nascondere: dalla camera in superficie scarta **zero** chunk (131 contro 131,
misurato). Quello che compra si vede dove ci sono occlusori grandi come un chunk
— sottoterra, e nelle grotte che questo engine ancora non ha.

**`bad_link` continua a lampeggiare** a circa una run su cinque, con una quarta e
una quinta faccia registrate in `TODO.md` sotto OPEN-9. Non è di M9 e non è del
meshing: la prova è che è rosso anche con il lavoro di M9 messo da parte.

---

## 9. Lo stato della suite

**57 test, tutti verdi, e per la prima volta in `ctest` non c'è più niente
`DISABLED`.**
