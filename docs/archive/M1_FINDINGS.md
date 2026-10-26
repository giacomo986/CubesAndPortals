# M1 — core voxel client

> Aggiornato il 2026-08-15. M1 di `DESIGN.md` §8: chunk store 32³ paginato, greedy meshing,
> formato vertice compatto, luce sole+blocco, AO, frustum culling, raycast, piazzamento e
> rottura. Test di uscita: mondo statico da 512³ a 60 FPS, memoria stabile su 10 minuti di volo.

## Come riprodurre

```sh
cmake -S . -B build -G Ninja && cmake --build build -j
ctest --test-dir build --output-on-failure      # 24 controlli headless + il gate di continuità
./build/src/client/cnp_client --world 512 --bench    # misura FPS, vertici, culling
./build/src/client/cnp_client --world 512 --soak 600 # dieci minuti di volo con edit continui
```

Hardware: AMD Radeon RX 5700, Mesa 25.2.8, GL 4.6 core, Ubuntu 24.04, g++ 13.3.

## Risultati

| Metrica | 128 | 256 | 512 |
|---|---|---|---|
| Chunk in memoria | 32 | 128 | 512 |
| Memoria del mondo | 2.0 MB | 8.0 MB | 32.9 MB |
| Illuminazione | 0.05 s | 0.21 s | 0.82 s |
| Vertici totali | 59 862 | 336 258 | 1 463 760 |
| Vertex buffer | 0.46 MB | 2.57 MB | 11.17 MB |
| FPS (2 portali, 1080p) | 3183 | 3189 | 2677 |

Il criterio è **60 FPS**. Il margine è ~45×, e senza cave culling, senza LOD e senza meshing su
worker — cioè senza tre delle ottimizzazioni che `DESIGN.md` §2.6 dà per necessarie.

### Soak: 10 minuti di volo con edit continui, mondo 512

```
1 099 850 frame, 1 099 850 edit in 600 s
frame ms   p50 0.365   p95 0.749   p99 3.626   max 24.786
RSS        138.0 -> 159.7 MB  (+21.7 MB)
PASS frame time (p99 sotto 16.67 ms)
PASS memory (crescita sotto 64 MB)
```

Il picco singolo di 24.8 ms è un relight più un rimesh caduti nello stesso frame. p99 a 3.6 ms
significa che succede raramente; è comunque l'argomento per spostare il meshing su worker.

La memoria **non cresce affatto**. La crescita osservata inizialmente era dello strumento di
misura, non del motore: §6.

## Il gate di continuità è sopravvissuto alla riscrittura

Formato vertice, mesher e shader sono stati riscritti da capo in questa milestone. Il gate di
`--verify` ha continuato a riportare **0 pixel su 15 488** dentro l'apertura del portale, senza
essere toccato. È esattamente il motivo per cui è stato scritto prima di iniziare M1 e non dopo.

## Insidie trovate

### 1. Overflow con segno in `hash2`, presente fin da M0

```c
uint32_t h = (uint32_t)(x * 374761393) ^ ...   // straripa già per x = 6
```

La moltiplicazione avviene in `int`: straripa, e l'overflow con segno è **undefined behaviour**.
L'ottimizzatore a `-O2` può assumere che non accada e dedurre quel che ne segue. Il risultato era
terreno che scriveva blocchi a migliaia di nodi sopra e sotto il mondo: un mondo da 128 allocava
539 chunk invece di 32, e uno da 512 arrivava a 1.9 GB e veniva ucciso dall'OOM killer.

Era lì da M0 e non si era mai visto, perché con il terreno piatto `hash2` decideva solo qualche
pilastro sparso. Dargli autorità sull'altezza del terreno l'ha fatto emergere. Le moltiplicazioni
ora sono senza segno.

### 2. Scrivere a un blocco il suo stesso valore non deve sporcare nulla

`World::set_block` marcava il chunk sporco incondizionatamente. Un mod che imposta uno stato a ogni
tick — cosa normalissima — rimeshava un chunk 32³ ogni frame. Con il greedy meshing, che per
ricostruzione costa molto più del mesher ingenuo, questo è valso un calo di 5× (1567 → 313 FPS).
`Chunk::set` ora riporta se qualcosa è cambiato davvero. Tornati a 3018.

### 3. Il flood-fill accodava ogni cella illuminata

La fase iniziale della luce solare metteva in coda ogni cella d'aria sopra il terreno: su 512×512
sono undici milioni di nodi, centinaia di MB, per una frontiera che è solo la superficie del
terreno. Ora accoda solo le celle che confinano davvero con l'ombra.

### 4. Il churn di allocazioni si vede come crescita di memoria

Ogni `sync` allocava uno snapshot da 118 KB e un vettore di vertici nuovi per chunk, e ogni edit
tre griglie e una deque. Toglierlo è comunque giusto — meno lavoro per frame — e ha tagliato la
pendenza di RSS da 0.041 a 0.014 MB/s. Snapshot e vettore ora sono riusati; la maschera del
greedy meshing è `thread_local`, così ogni worker terrà la propria senza riallocare.

Misurato: la pendenza è passata da 0.041 a 0.014 MB/s. Quel che resta **non** è churn di
allocazioni — vedi §6, dove l'ipotesi viene smontata con i dati.

Un secondo pezzo dello stesso problema: le tabelle di handle in `gfx_raylib.cpp` non riusavano mai
gli slot, e ogni chunk che superava la capacità del proprio buffer ne bruciava uno nuovo. Ora c'è
una free list. **Non era la causa principale** — misurato: la pendenza non è cambiata — ma è
comunque un difetto.

### 5. Un benchmark su terreno piatto non misura niente

Un piano perfetto è l'unico caso in cui il greedy meshing collassa a un quad per chunk. Il terreno
è ora una heightmap a due ottave di value noise, con una piazza piatta attorno ai portali perché il
gate di continuità dipende da quella scena. E la camera di misura stava in un angolo del mondo a
guardare fuori dal bordo, dove il frustum culling faceva il lavoro al posto del renderer: ora
misura dal centro guardando attraverso.

### 6. La crescita di memoria era lo strumento di misura

Questa sezione è stata riscritta due volte e vale la pena lasciare traccia di entrambe le volte,
perché l'errore è istruttivo.

**Primo tentativo:** il soak riportava +50 MB su dieci minuti, e io ho incolpato le nostre
allocazioni — snapshot e vettori di vertici allocati a ogni `sync`. Le ho riusate. La pendenza è
scesa da 0.041 a 0.014 MB/s, il che sembrava confermare la diagnosi. Non la confermava: avevo
cambiato due cose e attribuito il miglioramento a quella che avevo in mente.

**Secondo tentativo:** ho strumentato il soak con `mallinfo2` per separare i byte vivi dai byte che
glibc tiene. Risultato: `heap-used` piatto al byte per due minuti mentre RSS saliva. Ho concluso
frammentazione dell'arena, limitata dal massimo storico, e ho scritto che si sarebbe fermata da
sola. Tre ipotesi collaterali testate e scartate lungo la strada, tutte corrette come test:
`malloc_trim` recupera 1.3 MB una volta e poi zero; `MALLOC_MMAP_THRESHOLD_` non cambia la
pendenza; suddividere in tile il passaggio di luce non aiuta (e introduceva un bug — la cornice del
tile sforava i bordi del mondo, 648 chunk invece di 512 — quindi è stato rimosso).

**Poi un soak da 40 minuti ha smentito la conclusione.** A 35 minuti la RSS era a 196 MB e ancora in
salita, con un gradino di +19.5 MB attorno ai 1220 s in cui anche `heap-used` saltava da 80.2 a
95.5 MB. Ma le colonne `world` e `gpu` erano ferme a 32.9 e 11.2 MB per tutta la durata.

Il colpevole era `std::vector<double> frame_ms`, il buffer in cui il soak accumulava un campione
per frame. `reserve(2000000)` prende 16 MB in anticipo — per questo `uordblks` sembrava fermo, i
byte erano già contati — e le sue pagine diventano residenti man mano che i `push_back`
le toccano. Da qui la salita lenta e costante. A 2 milioni di frame il vettore raddoppia: da qui i
gradini.

Sostituito con un istogramma a bin fissi (20 000 bin da 0.01 ms, più un bin di overflow), che dà
gli stessi percentili a memoria costante. Il risultato:

```
t= 20s  RSS 151.6 (+13.4)   heap-used 65.0   heap-total 69.4   world 32.9  gpu 11.2
t= 60s  RSS 151.6 (+13.4)   heap-used 65.0   heap-total 69.4   world 32.9  gpu 11.2
t=100s  RSS 151.6 (+13.4)   heap-used 65.0   heap-total 69.4   world 32.9  gpu 11.2
t=140s  RSS 151.6 (+13.4)   heap-used 65.0   heap-total 69.4   world 32.9  gpu 11.2
```

Identica alla cifra decimale. **La memoria del motore è piatta, e non lo era mai stata altrimenti.**

La lezione, che è il motivo per cui questa sezione resta lunga: uno strumento di misura che alloca
proporzionalmente a quanto a lungo misura non può misurare la memoria. E due diagnosi
plausibili di fila, entrambe supportate da dati veri, erano entrambe sbagliate sulla causa.

### 7. Misurare il frame rate significa non misurare il compositore

Il soak ha registrato una singola frame da **89 secondi** con il processo al 4% di CPU. Non è
codice nostro: è `SwapBuffers` che si blocca quando la finestra è occlusa o il compositore
smette di presentarla. Un solo campione così rovina qualunque percentile in cui cade.

`gfx_set_present(false)` separa il lavoro del frame dalla consegna al compositore, e `gfx_finish()`
ferma il cronometro quando la GPU ha finito davvero, non quando il driver ha accettato il lavoro.
Il soak stampa comunque una riga `STALL` per ogni frame sopra i 200 ms, così un'anomalia
ambientale resta visibile invece di sparire in una media.

## Cosa non è stato fatto, e perché

- **Meshing su worker thread.** Nel piano di M1. Il singolo picco misurato (~22-30 ms) è un
  relight più rimesh che cadono nello stesso frame; è l'unico argomento rimasto a favore, e non
  è urgente con p99 a 3.8 ms. Lo snapshot è però già progettato per questo: non tiene alcun
  riferimento al mondo.
- **Cave culling.** Con 45× di margine sul criterio, non serviva per passare. Servirà quando ci
  saranno caverne vere e una view distance maggiore.
- **Texture array.** Restano l'atlante 4×4 e il wrap in `fract()`. Serve `glTexImage3D` via
  `glfwGetProcAddress`; il vertice compatto invece **non** ha avuto bisogno di alcuno shim, perché
  gli attributi passano come `UNSIGNED_BYTE` non normalizzati e i bit si estraggono nello shader.
- **Palette per chunk.** I chunk densi costano 96 KB (64 blocchi + 32 luce). A 512 sono 33 MB, che
  non è un problema; a view distance da gioco vero lo diventerà.
