# M0 — risultati dello spike

> Aggiornato il 2026-08-14. Lo spike M0 è il *kill switch* di `DESIGN.md` §8: esiste per
> falsificare tre ipotesi, non per produrre codice da tenere. Questo file registra cosa è
> passato, cosa si è rotto e cosa è stato imparato — comprese le insidie che il documento di
> progettazione dava per note ma descriveva in modo impreciso.

## Come riprodurre

```sh
cmake -S . -B build -G Ninja && cmake --build build -j
./build/tests/cnp_portal_math_test      # 24 controlli, headless
./build/src/client/cnp_client --bench   # 10 screenshot + misura FPS
```

Hardware di riferimento: AMD Radeon RX 5700 (navi10), Mesa 25.2.8, OpenGL 4.6 core,
Ubuntu 24.04, g++ 13.3. Il client richiede GL 3.3 core.

## Criteri di uscita

| # | Criterio | Esito |
|---|---|---|
| 1 | Attraversamento visivamente continuo | **Passato** |
| 2 | Portale corretto con la camera dentro la cornice | **Passato** |
| 3 | ≥ 200 FPS con 2 portali a 1080p, scripting < 1% del frame | **Passato** — 1567 FPS (0.638 ms/frame); Luau 0.0021 ms/frame = **0.33%** del frame |

### Criterio 1 — continuità

Verificato come confronto tra file, non a impressione. Il bench scatta due immagini dallo stesso
scenario:

- `08a_through_portal.png` — la vista normale, con il portale che mostra la sua immagine;
- `08b_reference_no_portals.png` — una camera **reale** messa dove sta la camera virtuale, con i
  portali non disegnati affatto.

Entrambe usano la stessa proiezione, e il quad campiona il render target per posizione sullo
schermo: dentro l'apertura le due devono coincidere pixel per pixel. Coincidono, salvo il bordo
dove la geometria del tunnel appartiene a portali diversi nelle due immagini.

> **Il verdetto era giusto, la misura no.** Vedi `M4_FINDINGS.md` §6, §7 e §9.
> Questo confronto girava su un rettangolo **specchiato rispetto alla metà dello
> schermo**, quindi per la maggior parte della vita del progetto confrontava due
> volte lo stesso muro: sarebbe rimasto verde comunque. Corretto lo
> specchiamento è diventato rosso all'11–29% e ha portato a galla due difetti
> veri; adesso riporta **0.00% a 6.5, 3.0 e 1.5 blocchi**, e va a rosso al 48%
> se l'immagine del portale viene slittata di sette pixel. Il criterio è
> superato; da settembre lo è anche la sua verifica.

Il confronto *ingenuo* non funziona: se nell'immagine di riferimento si disegnano i portali, dentro
l'apertura si vede il contenuto del portale opposto invece del mondo grezzo, e il diff è privo di
significato. Da qui il flag `draw_portals`.

### Criterio 2 — camera dentro la cornice

Con la camera nell'apertura, `portal_oblique_near` **rifiuta** (la vcam non è dal lato destinazione)
e i clip plane restano attivi. È il comportamento progettato: applicare Lengyel lì manderebbe il
near plane dietro la camera e il portale diventerebbe nero. L'HUD stampa `oblique off / clip ON` e
lo screenshot `04_inside_frame.png` mostra la destinazione, non uno schermo nero.

### Criterio 3 — prestazioni

900 frame in 0.574 s → **1567 FPS, 0.638 ms/frame**, framebuffer 1920×1080, due portali, vsync off.
Otto volte la soglia, e senza nessuna delle ottimizzazioni che `DESIGN.md` §2.5-2.6 dà per
necessarie: niente cave culling, niente frustum culling, niente vertice compatto, niente LOD, RT a
risoluzione piena per entrambi i portali.

Il ciclo misurato è un frame **intero** — tick dello script, rimeshing che lo script provoca, poi
il render graph — non la sola parte grafica. Misurando il solo disegno il numero sale a ~4400 FPS
(0.23 ms), ma cronometrare solo i pezzi che fanno bella figura è il modo in cui un benchmark
comincia a mentire. La differenza è quasi tutta il rimeshing: il mod fa lampeggiare un blocco, e
un chunk 32³ rimeshato costa parecchio finché il greedy meshing di M1 non arriva.

Costo dello scripting: **0.0021 ms/frame, 0.33% del frame**, sotto la soglia dell'1%. Va letto come
misura conservativa: il frame qui dura 0.638 ms, quindi l'1% è un budget minuscolo. A 60 FPS reali
la stessa chiamata sarebbe lo 0.013% del frame. Heap della VM: ~460 KB.

## Insidie trovate

### 1. `Matrix` di raylib: l'ordine dei nomi non è l'ordine in memoria

La struct dichiara `float m0, m4, m8, m12; float m1, m5, …`, quindi in memoria i campi stanno in
quell'ordine, mentre l'indice `mN` significa `riga + 4·colonna` (column-major) e `MatrixToFloat`
riordina prima di caricare. Un `float[16]` column-major standard corrisponde elemento per elemento
a `mN`, ma **convertirlo con `memcpy` compila e produce silenziosamente una trasposta**. La
conversione va fatta campo per campo (`gfx_raylib.cpp`, `to_raylib`).

### 2. Il fork è left-handed, raylib è right-handed

`portal.cpp` del fork deriva Lengyel per Irrlicht LH con `+z_eye` in avanti. raylib è RH con
`-z_eye` in avanti. Le componenti in eye space e il segno della guardia sull'angolo radente
(`c_e_z > 1e-4` diventa `c_e_z < -1e-4`) **non si trascrivono**. Il piano in eye space si ottiene
trasformando quello world-space con l'inversa trasposta della view: elimina ogni ambiguità di segno
invece di riderivare a mano.

### 3. `dst_surface` è il centro del nodo

`DESIGN.md` §2.3 dice "ancorato a `dst_surface + n·0.49·BS`, dentro la faccia esterna" senza dire
che `dst_surface` è il **centro del nodo**: la faccia esterna è a `+0.5`, quindi `0.49` è appena
dentro. Il quinto clip plane usa invece `+0.5` esatto.

### 4. Il frustum laterale è volutamente troppo largo

L'apex dei quattro piani laterali non è la camera virtuale: viene tirato a `0.05` dietro la faccia
di destinazione, conservando solo lo scostamento laterale. Con l'apex lasciato sulla camera, il
semiangolo diventa `atan(half_w / profondità)` e taglia via quasi tutta la destinazione. La
conseguenza è che i piani laterali **non** sono un culling fedele: servono solo a tagliare la
cornice vicino al piano del portale. Un punto oltre la faccia e molto spostato di lato resta
dentro, ed è corretto — se ne occupa il near plane obliquo. Il test lo mette nero su bianco
(`wide-by-design`), perché la prima stesura del test lo dava per errore.

### 5. Bordo del portale complanare al pavimento → caso degenere

**Trovato dal bench, non previsto dal documento.** Con il bordo inferiore dell'apertura all'altezza
del pavimento, la linea di vista che rade quel bordo esce dalla faccia **interna** della
destinazione e prosegue *esattamente sul piano del pavimento*. Nulla decide se colpisce il terreno o
lo sfiora, tranne l'arrotondamento: la parte bassa dell'apertura si riempie di colore di sfondo.

Non è un errore della proiezione — alzando la coppia di un nodo l'immagine torna a coincidere con
il riferimento. È il caso radente degenere, ed è imparentato con il motivo per cui il fork ha
`floor_portal_cb_shift` e un percorso separato per i portali orizzontali. **Da risolvere in M4**;
lo spike aggira il problema mettendo le aperture un nodo sopra il pavimento.

### 6. Lo stato grafico va restituito alla piattaforma

Il testo di debug di raylib passa dal suo batch, che usa lo shader e lo stato correnti. Lasciando
uno shader nostro attivo il font esce come spazzatura; lasciando il blending disattivato — che è il
default di `gfx_state_default()` — ogni glifo diventa un **rettangolo pieno**, perché i glifi sono
ritagli in alpha di una sola texture. Serve `gfx_shader_bind_default()` più uno stato UI esplicito.

### 7. La sandbox di Luau blocca anche il mod, se il mod gira nello stato principale

`luaL_sandbox()` congela la tabella dei globali. Il primo mod è morto subito:

```
[".../mods/spike/init.luau"]:46: attempt to modify a readonly table
```

alla riga `function on_step(dt: number)` — cioè un mod non riesce nemmeno a dichiarare una funzione
globale. Il modello corretto di Luau è: stato principale sandboxato e **congelato**, e ogni mod su
un proprio `lua_newthread` + `luaL_sandboxthread()`, che gli dà una tabella di globali scrivibile
concatenata in lettura a quella congelata.

Non è un aggiramento della sandbox: è la sandbox usata come previsto, e coincide con l'isolamento
"una VM per mod" chiesto da `DESIGN.md` §5.6 — due mod non si vedono e non si sovrascrivono i
globali a vicenda. Il thread va ancorato con `lua_ref`, altrimenti il collector se lo riprende.

Conferma incidentale che la scelta di §5.2 regge: la sandbox ha fermato una scrittura ai globali al
primo tentativo, senza che nessuno l'avesse configurata per farlo.

### 8. `rlgl` disegna gli indici come `GL_UNSIGNED_SHORT` e basta

`rlDrawVertexArrayElements` è cablata su indici a 16 bit, il che limiterebbe un chunk a 65 536
vertici — soglia che un chunk 32³ nel caso peggiore supera. Lo spike usa geometria **non
indicizzata** (sei vertici per quad) per non introdurre un overflow silenzioso proprio nella
milestone che deve produrre misure affidabili. Da riprendere in M1 insieme al vertice compatto.

## Stato dello scripting

Luau 0.733 è integrato dietro `script_*`, con una VM sandboxata, tetto di heap (16 MB) e budget di
interruzione (2 M safepoint per chiamata). `mods/spike/init.luau` è un mod `--!strict` che piazza
14 blocchi al caricamento e, a ogni frame, sposta un portale e legge/scrive blocchi — esattamente
la richiesta di §8 per M0.

I binding sono **scritti a mano** (4 funzioni). Il generatore di §5.4, che da una tabella
descrittiva emette glue C++, `.d.luau` e documentazione, è lavoro di M2: con quattro chiamate non
avrebbe ancora ripagato sé stesso.

Una nota onesta sul mod: sposta il portale B ma **non** il muro che lo incornicia, perché in M0 un
portale non è ancora legato ai blocchi che lo contengono. L'ampiezza è tenuta piccola per questo.

## Cosa non è stato fatto

- **Nessuna ricorsione** (`MAXD`, pool di RT): `DESIGN.md` §2.5 la dà come capacità nuova, ma non è
  fra i criteri di uscita.
- **Nessun culling**: né frustum né cave culling. Il margine sul criterio 3 li rende non urgenti
  per M0, ma sono il grosso di M1.
- **Texture array assenti**: servono `glTexImage3D` via `glfwGetProcAddress`, rimandato a M1.
- Nessun server, nessuna rete, nessuna sessione multipla: M3 e M5.

## Raccomandazione

**Tutti e tre i criteri di uscita sono soddisfatti. Il kill switch non scatta: si procede a M1.**

I due che il documento considerava rischiosi — continuità e camera dentro la cornice — sono passati,
e la matematica che li regge è verificata da 24 controlli headless che girano senza GPU, quindi resterà
verificata anche dopo che il renderer sarà stato riscritto tre volte. Il terzo ha otto volte il
margine richiesto sul frame rate e un terzo del budget concesso allo scripting, in entrambi i casi
prima di qualunque ottimizzazione.

Va detto cosa questo *non* dimostra. Un mondo statico da 128³ con due portali non è una prova di
carico: mancano cave culling, streaming, entità, rete e il costo del meshing sotto un giocatore che
si muove. Il margine attuale è largo abbastanza da rendere improbabile che sia la grafica a fermare
il progetto — che era la domanda di M0 — ma il rischio numero uno di §9 resta quello che era, e non
è tecnico: i contenuti.
