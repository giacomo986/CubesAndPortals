# M2 — Modding di prima classe: cosa è stato costruito e cosa si è imparato

Milestone M2 di `DESIGN.md` §8: *una VM per mod con quota e interrupt, capability
dal manifest, generatore di binding, registry data-driven con hot reload, event
bus tipizzato, hook di tick.*

Tutto quello che segue è verificato da esecuzioni, non da lettura del codice.
`ctest` è passato da 2 a 5 test.

---

## 1. Il registry: il motore non sa più qual è l'id della pietra

I blocchi sono dati: nome namespaced, texture per faccia, se ferma il movimento,
se ferma la luce, quanta ne emette. Mesher, luce, collisione e raycast chiedono
al registry. L'atlante è **generato dal registry** e dimensionato su quanti nodi
ci sono: `mods/palette` ne dichiara venti e l'atlante passa da 16 a 64 tile senza
che una costante venga toccata nel motore.

La luce da blocco si propaga davvero: il canale era portato da storage, vertice e
shader fin da M1 senza niente da metterci dentro.

---

## 2. Una VM per mod

Ogni mod ha la sua `lua_State`, la sua quota di heap e il suo budget di
interruzione, presi dal suo `mod.conf`.

Le capability si applicano **per rimozione**, non per convenzione: le globali non
dichiarate vengono messe a nil *prima* che `luaL_sandbox` congeli la tabella. È
obbligatorio che sia in quell'ordine — un thread sandboxed legge attraverso la
tabella globale congelata, quindi ombreggiare per-mod non nasconde niente.

Luau parte già senza `io`, `package`, `loadstring`, `dofile`, `os.execute`,
`os.getenv`: non c'è niente da togliere perché non c'erano mai stati. Resta `os`
(dietro capability, perché il tick deve poter essere deterministico per seed +
input) e `debug`. `native`, `exec`, `fs:read`, `net:http` sono **rifiutate per
nome**: una capability ignorata in silenzio è un sistema di permessi che mente.

Lo stesso vale per un refuso: `capabilties = os` non si legge come "nessuna
capability", fa fallire il caricamento.

### Il gate

`tests/script_isolation_test.cpp`, senza GPU, 9 mod fixture:

```
mod 'runaway' disabled: script exceeded its interrupt budget
mod 'hog' disabled: not enough memory
  ok    the control mod is still running after both died
  ok    and it stepped again: the tick continued past the failures
```

**Il timeout di 60 s in ctest è parte del test.** Se l'interrupt smette di
scattare il test non fallisce: si impianta dentro il `while true do end`.
Verificato mettendo quel budget a zero — il processo è dovuto essere ucciso a
mano.

---

## 3. L'ordine di caricamento era sbagliato, e i dati l'hanno detto subito

Con due mod, `beacons` veniva prima di `spike` in ordine alfabetico e
`core.node_id("spike:copper")` restituiva `0`. Aggiunto `depends` con
ordinamento topologico. Una dipendenza assente è un rifiuto che la nomina; un
ciclo non fa sparire i mod in silenzio, li rifiuta uno per uno dicendo cosa
stavano aspettando.

---

## 4. Hot reload

Salvi un file, entro mezzo secondo il mod è ricaricato. `F5` forza.

L'ordine delle operazioni è tutto: **la VM nuova viene costruita e fatta girare
prima che la vecchia venga distrutta**. Un typo in un mod in editing costa la
modifica, non il mod. Il manifest viene riletto — altrimenti basterebbe
aggiungere `capabilities = native` a un file che il loader ha smesso di guardare.

Lo stato attraversa come stringa (`on_save` → `on_load`). Non una tabella: un
serializzatore generico dovrebbe avere un'opinione su funzioni, cicli e userdata,
e sarebbe sbagliata su tutte e tre.

Il registry sa **chi** ha registrato ogni nodo. Al reload le voci del mod vengono
marcate e quelle che non si ripresentano diventano orfane: id conservato (il
mondo ne è pieno, rinumerare vorrebbe dire riscrivere ogni chunk), magenta
acceso, ancora solide. Lo stesso patto dei classici giochi a blocchi, per lo stesso
motivo.

Provato sul motore vero, non solo nel test:

```
[mod] beacons: pillar=7 beacon=8 ...
[mod] beacons RELOADED: pillar=7 beacon=8 ...   <- stessi id
mod 'beacons' reloaded
```

---

## 5. Event bus tipizzato

Schema dichiarato (nomi dei campi, tipi, se annullabile). Tre tipi soltanto —
`number`, `string`, `boolean` — perché un bus che trasporta qualsiasi cosa è un
bus il cui schema non significa niente.

- **Priorità**: dichiarata, non ereditata dall'ordine di caricamento.
- **Annullare** ferma gli handler successivi; quelli prima hanno già parlato.
- **Modificare** riscrive il payload per gli handler dopo *e per il motore*.
- **Cross-VM**: un mod emette, un altro ascolta, nessuno chiama l'altro.
- Un handler che va in errore disabilita il suo mod e basta.

Il core emette sullo stesso bus con gli stessi schemi: `core:pre_place`,
`core:placed`, `core:pre_break`, `core:broken`, `core:tick`. Nessun percorso
privilegiato per il motore — è l'argomento di §6.3, e `mods/guard` lo rende
concreto: rifiuta le costruzioni nella piazza dei portali, sostituisce il blocco
sopra y=30, e alza un evento suo che `beacons` gestisce dalla propria VM. Nessuna
riga di motore conosce quella regola.

Il bus è di proprietà del mod host perché al reload va tolta esattamente la lista
di handler della VM che se ne va — **per VM e non per nome**, dato che per un
istante durante il reload esistono due VM dello stesso mod.

---

## 6. Generatore di binding

Le firme stanno una volta sola in `src/script/api.luau`. `tools/api_gen` (in
Luau, sul submodule già presente: nessuna toolchain nuova) genera glue C++,
`api.d.luau` e `API.md`. `script_luau.cpp` ha perso 80 righe scritte a mano.

Misurato sul posto: aggiungere `core.node_name` è costato **sei righe** nella
tabella più una funzione host. Controllo argomenti, tipi e documentazione si sono
scritti da soli.

`core.event.*` resta a mano: prende tabelle invece di scalari, e un generatore
che la coprisse sarebbe più grande delle tre funzioni che sostituisce. Detto nel
codice e nella doc, invece di far finta di coprire tutto.

---

## 7. Errori fatti, e come sono emersi

1. **Payload sopra la funzione.** Costruivo la tabella dell'evento sopra
   l'handler nello stack: `lua_pcall` chiamava la tabella
   (`attempt to call a table value`). Preso dal test del bus, mai arrivato a un
   mod.
2. **Un'asserzione mia sbagliata.** Avevo scritto che dopo un annullamento
   "gli handler successivi non girano" e ci avevo messo dentro un handler a
   priorità 0, che gira *prima* del canceller a 10. Il codice era giusto, il test
   no: aggiunto un handler a 90 per testare la cosa vera.
3. **`replace` troppo largo.** Uno script di editing ha sostituito anche
   `local function build()` oltre a `build()`, inserendo un handler due volte —
   visibile solo perché il log stampava due volte la stessa riga.
4. **Screenshot letti stantii.** raylib scrive nella directory di lavoro: ho
   confrontato due esecuzioni leggendo un PNG di un'ora prima. Ora c'è una sola
   directory e il path assoluto viene stampato.
5. **`const const char *`.** Il primo glue generato non compilava. Il generatore
   ora sa che `const char *` ha già il suo const.

## 8. Sabotaggi (il test serve solo se fallisce quando deve)

| sabotaggio | esito |
|---|---|
| strip delle globali disattivato | 4 asserzioni rosse |
| budget di interrupt a 0 | il test si impianta → esiste il timeout |
| VM vecchia distrutta prima della nuova | "the version that does compile is still running" rossa |
| sweep degli orfani saltato | asserzioni sugli orfani rosse |
| `drop_vm` al reload rimosso | **segfault** al primo emit successivo |
| sort per priorità disattivato | 3 asserzioni rosse |

---

## 9. Il p95 che si è mosso, e perché non era una regressione

Soak da 120 s su mondo 512, con i quattro mod: memoria **piatta** (152.4 MB
identici a ogni report), `p99 3.955 ms`, gate passati. Ma il `p95` era passato da
**0.749 ms (M1) a 3.445 ms**. Un percentile che si muove di 4× va spiegato, non
archiviato.

### A/B, non teorie

| configurazione | p50 | p95 | p99 | frame che rimagliano |
|---|---|---|---|---|
| solo `spike` (come M1) | 0.365 | 0.805 | 3.945 | — |
| `spike` + `palette` | 0.355 | 0.685 | 3.825 | **3.3 %** |
| tutti e quattro | 0.375 | 3.455 | 3.945 | **5.0 %** |

Due ipotesi sbagliate prima di misurare: la tabella payload allocata a ogni
`core:tick` (tolto l'handler: p95 invariato a 3.445) e la geometria in più
piazzata dai mod (il volo del soak passa a 140 blocchi dal centro, la piazza è
altrove).

Poi ho strumentato invece di indovinare — conteggio dei frame che ricostruiscono
un chunk, e tempi per fase:

```
per frame  script 0.0032  sync 0.1666  render 0.0313  finish 0.3944 ms (mean)
remesh     10066 frames of 201300 (5.0%) rebuilt a chunk
```

### La spiegazione, completa

1. **Rimagliare un chunk costa ~3.5 ms** (snapshot + greedy meshing + upload di
   32³). È l'unica operazione in quella fascia, ed è la stessa in tutte le
   configurazioni: per questo il **p99 non si muove**.
2. Il `p95` non ha cambiato costo, ha cambiato **lato di una soglia**: con il
   3.3 % di frame che rimagliano il 95° percentile cade appena sotto la fascia,
   con il 5.0 % ci cade dentro. Un dirupo di percentile, non un rallentamento.
3. Da dove viene l'1.7 % in più: il soak passa agli script un `dt` **fisso** di
   1/60 mentre gira a ~1700 fps, quindi il tempo simulato scorre ~28× più veloce
   del reale. `beacons` sposta il faro acceso ogni 0.67 s **simulati** — cioè
   circa 5000 volte in 120 s reali — e ogni spostamento sporca un chunk.

Nessuna regressione del motore: un artefatto della misura più un costo che era
già lì.

**Il costo che resta vero e va aggredito:** un blocco cambiato = un chunk 32³
ricostruito per intero, ~3.5 ms. Il meshing su thread worker era stato rimandato
in M1 con giustificazione misurata (p99 3.6 ms, 45× di margine); questo è il
primo dato che dice quando andrà ripreso.

---

## 10. Numeri

Mondo 128³, quattro mod caricati:

```
criterion 3: 900 frames in 0.392 s -> 2298 FPS (0.435 ms/frame)
             scripting PASS (luau, 4 mods, 0.0028 ms/frame = 0.636% of frame, threshold 1%)
             script heap 1886440 bytes across every VM
             events 1024 emitted, 1023 handler calls, 6 schemas, 4 handlers
```

---

## 11. Cosa manca a M2, dichiarato

L'exit test di §8 chiede *«un mod `--!strict` che aggiunge 20 blocchi **e un
item**, ricaricato a caldo senza riavvio»*. I venti blocchi ci sono
(`mods/palette`), il reload a caldo c'è, il mod in loop infinito viene interrotto
senza fermare gli altri. **L'item no: non esiste ancora un sistema di item né un
inventario**, e arriva con M7 (inventario, craft, UI). Non è stato aggirato con un finto item
per far tornare la casella.

Manca anche il registry per **entità**: quello poggia sull'ECS di M8.
