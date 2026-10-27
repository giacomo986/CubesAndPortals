# Cubes and Portals — sandbox voxel ex-novo su raylib con portali nativi

> Documento di progettazione. Analisi di fattibilità (grafica, server, client multi-istanza,
> scripting, estensibilità) e piano a milestone. Non è ancora un impegno a scrivere codice:
> §8 contiene un *kill switch* esplicito dopo 3 settimane di spike.
>
> Scritto il 2026-08-14, partendo dall'esperienza di un fork precedente
> (fork dell'engine di partenza con portali + dual-client cross-server, funzionante).

---

## Context

Il fork precedente oggi è: ~9 700 righe di Lua (`yaportal/init.lua` 6911, `yaportal_link/init.lua`
2221, `yafps/` 552) sopra un **fork dell'engine** di partenza (77 file, +6 369/−309 righe su una base
di ~214k righe C++). Sette capacità del fork sono *invisibili* a un mod stock e quindi
non-upstreamabili in blocco: RTT da seconda camera, near plane obliquo, `gl_ClipDistance`
per-vertice, teletrasporto atomico in un pacchetto, roll della camera, shift della collision
box, **seconda connessione live** a un altro server renderizzata dentro il portale.

Il costo di questo assetto non è scrivere le feature: è **inseguire upstream** e combattere
architetture che non erano pensate per due mondi vivi nello stesso processo (singleton, sky
globale, `ViewArea` CPU-side che si sporca se tocchi la projection, identità dei client
per indirizzo invece che per mondo). Dato che la probabilità di merge upstream è ~0, la
domanda è: **conviene ripartire da un engine proprio, con raylib come layer di piattaforma,
progettato dal giorno 0 attorno a portali e multi-sessione?**

Questo documento risponde su quattro assi (grafica, server, client multi-istanza, scripting),
con pregi/difetti misurati e un piano a milestone.

---

## 0. TL;DR — verdetto

| Domanda | Risposta breve |
|---|---|
| Portali con oblique near clipping su raylib sono fattibili? | **Sì, e più puliti di adesso.** `rlSetMatrixProjection` accetta la matrice di Lengyel senza toccare il culling CPU: l'hack `setRenderProjectionMatrix` del fork sparisce per costruzione. |
| I clip plane laterali sono fattibili? | **Sì.** Shader nostri → `gl_ClipDistance[0..4]`; `glEnable(GL_CLIP_DISTANCE0+i)` è GL core, basta dichiararlo (raylib non lo espone ma non lo blocca). Fallback `discard` per GLES/Web. |
| Server fattibile in C/C++ puro? | **Sì.** ENet + zstd + SQLite + FastNoiseLite. Nessuna toolchain nuova, tutte librerie C. |
| Client multi-istanza per portali cross-server? | **Sì, ed è l'argomento più forte a favore del rewrite.** Oggi funziona ma è stato strappato a forza dai singleton; da zero è "una `struct Session`, niente globali". Misura già in mano: **il render del mondo B costa solo il 3-7% del frame** — il multi-sessione non è il collo di bottiglia. |
| Linguaggio di scripting? | **Luau** (un Lua moderno: tipi graduali, native codegen, sandbox *progettata* per codice non fidato) dietro un'interfaccia `script_*`. Era previsto **LuaJIT** come secondo backend per i mod legacy l'engine di partenza e per il codice trusted con FFI: caduta la compatibilità (2026-09-12), resta un runtime solo e la FFI non è in piano. Valutati anche QuickJS-ng, WASM, Wren, Umka, C#: §5. |
| Estendere le *meccaniche*, non solo i blocchi? | ECS server-side con componenti/sistemi definiti da Lua + trait sui nodi + render-graph con pass registrabili + canali di protocollo per mod. §6. |
| **Costo reale** | Spike di validazione ~3 settimane. Client+server giocabile con portali cross-server: **~7 mesi**. Engine + gioco minimo: **~11 mesi**. Parità di contenuti in stile gioco a blocchi classico: **anni**, a meno di §7.5. |
| **Raccomandazione** | Spike M0 come cancello. Sui contenuti la raccomandazione era il **layer di compatibilità con l'API dell'engine di partenza** (§7.5), **annullato il 2026-09-12**: L'engine di partenza era l'ispirazione, non l'obiettivo. Resta vera la metà scomoda della frase — un progetto come questo muore sui contenuti e non sulla grafica. **Scope fissato lo stesso giorno:** un clone del classico gioco a blocchi da cui i portali portano ad altri mondi, ciascuno un server con i suoi mod, fino a mondi che sono un altro gioco — una corsa acrobatica, un FPS in stile arena shooter. Non è più piccolo dello scope che §7.5 chiamava fatale; lo tiene in piedi il fatto che ogni modo è un mondo piccolo dietro un portale e non un pezzo del sandbox (M12, M13). |

### Decisioni prese (fissano lo scope di questo piano)

| Decisione | Scelta | Conseguenza nel piano |
|---|---|---|
| Obiettivo | **Clone del classico gioco a blocchi, con portali verso altri mondi con mod propri — fino a modi di gioco diversi (corsa acrobatica, FPS in stile arena shooter)** (2026-09-12) | §7.5 è annullato, quindi i contenuti del sandbox si scrivono qui (M12) e la parità resta anni. Ogni modo è un server con i suoi mod, e chiede all'engine cose che un sandbox non chiede: comportamento di un mondo che arriva al client, movimento veloce, colpi a distanza (M13) |
| Fork dell'engine di partenza | **Congelato** | Nessun prodotto vivo durante il rewrite → M-1 obbligatoria (release taggata + bug documentati prima del freeze) |
| Layer grafico | **raylib dietro `gfx_*`** (~30 funzioni) | Migrazione a `sokol_gfx` possibile in giorni se serve in M9 |
| Multiplayer | **2-8 giocatori per mondo** | SQLite basta, anti-cheat leggero, interest management semplice (−1 sett. su M3) |
| Scripting | **Luau**, unico runtime | Sandbox e tipi dal runtime; costo binding pagato con un generatore (§5.4). Il secondo backend LuaJIT è caduto con la compatibilità dell'engine di partenza (§5.3) |
| Collocazione | **`/home/giacomo/dev/Cubes and portals/`** | Repo nuovo e indipendente; il fork precedente non viene toccato se non per il freeze |

---

## 1. Punto di partenza — cosa esiste e quanto pesa

| Componente | Dimensione | Riusabile in un engine nuovo? |
|---|---|---|
| `yaportal/init.lua` | 6911 righe Lua | **Sì, ~70%.** Geometria portale, trasformata, trigger, macchina a stati, blocchi classici dei puzzle a portali. Cambiano solo le chiamate `core.*`. |
| `yaportal_link/init.lua` | 2221 righe Lua | **Sì, ~50%.** Registry a file JSON e logica di pairing sopravvivono; l'handshake si riprogetta (vedi §4.4). |
| `yafps/` (gioco intero) | 552 righe + 160 KB media | **Sì.** E dimostra che un *gioco* qui costa 5 mod e mezzo giorno — dato importante per §7. |
| Renderer portali del fork | `portal.{cpp,h}` + `portal_manager.{cpp,h}` ≈ **1660 righe**, + ~250 di hook in `ClientMap`, ~240 di `Sky`, 12 di shader | **Da riscrivere**, ma è l'unico pezzo grafico davvero specifico: 2 000 righe, non 200 000. |
| Resto del fork (dual-client, protocollo, mapnode, API Lua) | ~4 500 righe | **No come codice, sì come conoscenza.** Le invarianti di `AGENTS.MD` §461 valgono più del codice: sono bug già pagati. |
| `docs/PORTAL_SYSTEM.md` | 1366 righe + 9 figure SVG | **Sì**: è la specifica dell'engine nuovo, già scritta, fino alle formule. |
| Engine di partenza | ~214k righe C++ (+ Irrlicht) | Sostituito. Un engine mirato ne serve ~40-60k per fare quello che ci serve. |
| Giochi esistenti (contenuti) | enorme | **Perso.** È il costo nascosto del rewrite. |

**Due limiti dell'implementazione attuale che un engine nuovo toglierebbe di mezzo per
costruzione** (misurati, non supposti):

- **Nessuna ricorsione.** Il portale dentro il portale oggi non esiste: l'hook che disegna i quad
  viene saltato durante qualunque pass RTT (`clientmap.cpp:1281`). Profondità massima = 1.
- **Ogni portale costa una scena intera a risoluzione piena.** Ogni slot attivo fa un
  `drawAll()` completo con frustum culling disabilitato, alla dimensione di rendering dello
  schermo, senza LOD, senza risoluzione ridotta, senza grafo di visibilità che decida quali
  portali contano davvero.

**Numero da tenere a mente:** `docs/PORTAL_SYSTEM.md` esiste già ed è dettagliata a livello di
formule (matrice di Lengyel invertita in forma chiusa, apex standoff `0.05·BS`, plane 4 sulla
faccia *esterna* del nodo, clamp `n·bn < 0`). Un rewrite non riparte dalla teoria: riparte
dall'implementazione, con la teoria in mano. Questo taglia forse il 40% del rischio grafico.

---

## 2. Fattibilità grafica su raylib

### 2.1 Cosa dà raylib, cosa non dà

Verificato su **raylib 5.5** (header dell'installazione locale in `~/dev/raylib`, binding CPython).

**Dà (e ti risparmia mesi):** finestra/input/gamepad via GLFW o SDL2, `rlgl` come astrazione GL
3.3 core / GLES 2-3, caricamento immagini e font, audio (miniaudio), matematica (`raymath`),
mesh + `UploadMesh`/`DrawMesh`/`DrawMeshInstanced`, `RenderTexture2D`, shader custom completi,
e — cruciale — **accesso di basso livello**: `rlLoadFramebuffer`, `rlFramebufferAttach`,
`rlLoadTextureDepth`, `rlLoadVertexArray`, `rlUpdateVertexBuffer`, `rlDrawVertexArrayElements`,
`rlEnableShader`, `rlSetUniform`, `rlSetUniformMatrix`, `rlSetMatrixProjection`/`Modelview`,
`rlTextureParameters`, `rlSetCullFace`, `rlColorMask`, `rlDisableDepthMask`.

Tradotto: **raylib non ti sta in mezzo**. Quando serve fare GL "vero", `rlgl` ti passa gli id
degli oggetti GL e te li lascia manipolare.

**Non dà (e devi scriverlo tu):**

| Mancanza | Impatto | Rimedio |
|---|---|---|
| Nessun scene graph / frustum culling / occlusion | Alto — è *il* lavoro di un engine voxel | Lo scrivi tu, ed è meglio così (§2.6) |
| Nessuna texture array (`GL_TEXTURE_2D_ARRAY`) | Medio | ~60 righe di shim GL, oppure atlas con padding + mip manuali |
| GL single-thread | Medio | Meshing su worker → buffer CPU → upload sul thread GL (come fa l'engine di partenza) |
| Niente shadow map / ombre | Medio | Le scrivi tu, o parti senza ombre (Il classico gioco a blocchi non le ha). Scritte il 2026-10-11: vedi ARCHITECTURE.md, "Come si disegna la luce" |
| Audio non spaziale 3D (solo pan/volume) | Basso | Attenuazione+pan a mano, o miniaudio diretto (già linkato) |
| Nessun `BeginTextureMode` annidato | Basso | Non serve: la ricorsione portali si fa **dal più profondo al più superficiale** su un pool di RT (§2.5) |
| Nessuna UI dichiarativa | Medio | `microui` o `Clay` (C, MIT) + backend raylib; formspec-like guidato da Lua |
| Niente rete, niente fisica, niente storage | Alto ma atteso | ENet / AABB proprio / SQLite (§3) |

**Conclusione onesta:** raylib copre *il guscio*, non l'engine. Ti regala forse il 15% del
lavoro client — ma è il 15% più noioso e più bug-prone (piattaforme, input, decodifica
immagini, audio), ed è il 15% che il fork dell'engine di partenza oggi paga con Irrlicht, che è tenuto in vita
dal progetto stesso.

### 2.2 Pipeline portali proposta

Un frame, mondo attivo `A`, N portali visibili:

```
1. cull:      per la camera del giocatore  → drawlist A
2. portal:    per ogni portale visibile e linkato, ordinati per profondità:
                 vcam = transform(src → dst, cam)
                 cull per vcam                        → drawlist P[i]   (bypass del frustum del giocatore)
                 BeginTextureMode(RT[i])
                    rlSetMatrixProjection(oblique(P_base, piano_dst))   ← Lengyel
                    upload uniform portalClipPlanes[5]; glEnable(CLIP_DISTANCE0..4)
                    render opachi(P[i]) ; render trasparenti(P[i], ordinati per vcam)
                    glDisable(CLIP_DISTANCE0..4)
                 EndTextureMode()
3. main:      opachi(A)
              quad portali: 4 pareti inset + fondo con RT[i] (UV screen-space)    ← DENTRO il pass opaco
              trasparenti(A) ordinati per la camera vera
4. post:      fog/tonemap/UI
```

Due decisioni sono già state pagate col sangue nel fork e vanno riportate identiche:

- **I quad dei portali si disegnano dentro il pass opaco**, non dopo. La geometria trasparente
  scrive depth: un quad disegnato dopo viene z-cullato da qualunque lastra di vetro davanti al
  portale.
- **Il pass RTT prima del pass principale**, così il driver torna a FBO 0 e il main pass ribinda
  il suo target pulito.

Su raylib entrambe sono banali perché **il render graph è nostro**: non c'è un `Draw3D` altrui
in cui infilarsi con un hook. Questo è il primo punto in cui il rewrite è più semplice del fork.

### 2.3 Oblique near clipping su rlgl — fattibilità confermata

raylib usa proiezione **RH, depth [-1,1]** (`MatrixPerspective` → `m11 = -1`). Lengyel standard,
indici in layout column-major GL (identici ai campi `Matrix.m2/m6/m10/m14` di raylib):

```c
// C = piano di clip in eye space (camera space RH, -Z avanti), normalizzato,
//     ancorato appena dentro la faccia esterna del portale di destinazione.
static Matrix ObliqueNear(Matrix P, Vector4 C)
{
    Vector4 Q = {
        (sgnf(C.x) + P.m8)  / P.m0,
        (sgnf(C.y) + P.m9)  / P.m5,
        -1.0f,
        (1.0f + P.m10) / P.m14
    };
    float s = 2.0f / (C.x*Q.x + C.y*Q.y + C.z*Q.z + C.w*Q.w);
    P.m2  = C.x * s;
    P.m6  = C.y * s;
    P.m10 = C.z * s + 1.0f;
    P.m14 = C.w * s;
    return P;
}
// uso:
BeginTextureMode(rt); BeginMode3D(vcam);
    rlDrawRenderBatchActive();                      // flush di quel che c'era
    rlSetMatrixProjection(ObliqueNear(rlGetMatrixProjection(), plane_eye));
    draw_world(drawlist_i);                          // DrawMesh usa matProjection corrente
EndMode3D(); EndTextureMode();
```

`rlgl` ricalcola `mvp = matProjection * matModelView` a ogni `rlDrawMesh`/flush del batch, quindi
la matrice obliqua raggiunge sia le mesh sia il batch immediato.

> **Verificato in M0 su raylib 6.0** (non 5.5: il progetto è partito da 6.0). `rlGetMatrixProjection`,
> `rlSetMatrixProjection` e `rlDrawRenderBatchActive` esistono, e la convenzione di proiezione è
> **invariata** rispetto a 5.5 — `MatrixPerspective` resta RH con depth `[-1,1]` e `m11 = -1`,
> quindi la formula sopra vale identica. Due correzioni alla prima stesura di questo paragrafo:
> **`rlGetRenderBatch()` non esiste** (e non è mai esistita); per svuotare il batch interno si usa
> `rlDrawRenderBatchActive()`. E `rlLoadShaderCode` si chiama **`rlLoadShader`** (sorgente singolo)
> o **`rlLoadShaderProgram`** (vertex+fragment da stringhe).
>
> La formula è stata verificata numericamente, non solo per ispezione: `tests/portal_math_test.cpp`
> controlla che un punto giacente sul piano di clip finisca esattamente a `ndc.z == -1`, in asse e
> fuori asse, e che la geometria tra camera e portale finisca oltre il near plane.
>
> Attenzione a un dettaglio di raylib che non è nel codice sopra: la struct `Matrix` **dichiara** i
> campi nell'ordine `m0, m4, m8, m12, m1, …`, quindi l'ordine in memoria non è l'ordine dei nomi.
> L'indice `mN` significa `riga + 4·colonna` (column-major) e `MatrixToFloat` riordina prima di
> caricare. Un `float[16]` column-major standard corrisponde elemento per elemento a `mN`, ma
> **convertire con `memcpy` produce una matrice trasposta**, silenziosamente.

**Il bug che sparisce da solo:** nel fork, impostare la projection vera faceva cullare le entità
da parte del SceneManager contro un frustum obliquo (macchie nere), e per questo esiste
`setRenderProjectionMatrix` in `ICameraSceneNode`. In un engine nostro il culling CPU usa il
frustum **non modificato** perché è codice nostro e sappiamo perché: nessun hack, nessuna patch
a una classe di terze parti.

**Guardie da riportare** (tutte e tre già nel fork, `portal.cpp:153-206`):
1. Obliqua **solo** se la vcam è dal lato destinazione (`side < -0.001·BS`). Con il giocatore
   *dentro* il portale `c_e_w` cambia segno, il near plane finisce dietro la camera e il portale
   diventa nero → projection normale.
2. Salto se `c_e_z ≤ 1e-4` (angolo radente) o `|dot(c_c,q)| ≤ 1e-6` — degeneri.
3. Il piano di clip è ancorato a `dst_surface + n·0.49·BS`, *dentro* la faccia esterna, per non
   fare z-fighting con i blocchi adiacenti.

> **Precisazione da M0.** `dst_surface` qui è il **centro del nodo**, non la superficie visibile:
> la faccia esterna sta a `+0.5·BS`, quindi `0.49·BS` è "appena dentro" quella faccia. È la stessa
> convenzione del quinto clip plane di §2.4 (ancorato invece a `pos + n·0.5·BS` esatto). Confondere
> centro e superficie è ciò che, nel fork, lasciava la metà anteriore del nodo di uscita a coprire
> l'immagine agli angoli radenti.
>
> Il segno della guardia 2 va riletto passando a raylib: il fork è **Irrlicht LH** con `+z_eye`
> in avanti e usa `c_e_z > 1e-4`; raylib è **RH** con `-z_eye` in avanti, quindi la condizione
> equivalente è `c_e_z < -1e-4`. Il piano in eye space conviene ottenerlo trasformando quello in
> world space con l'inversa trasposta della view, invece di riderivare a mano le componenti:
> toglie ogni ambiguità di segno.

**Precisione:** la view matrix della vcam va costruita a mano dal `vdir` già normalizzato, non
via `lookAt(pos, target)`. La sottrazione `target − pos` in float32 perde precisione con
posizioni di portale lontane (il caso cross-world) — nel fork è servita una funzione apposta
(`setRenderViewMatrix`); in un engine nostro è semplicemente come si scrive la funzione.

### 2.4 Clip plane laterali — due strade, entrambe aperte

Il near plane obliquo da solo lascia entrare la cornice del portale di destinazione ai bordi,
ad angoli radenti. Servono i 5 piani (4 laterali + faccia) del fork.

**Strada A — `gl_ClipDistance` (desktop GL 3.3, preferita).** Gli shader sono nostri:

```glsl
// VS
uniform vec4 uClipPlanes[5];
uniform int  uClipCount;              // 0 = disattivato
out float gl_ClipDistance[5];
...
for (int k = 0; k < uClipCount; ++k)
    gl_ClipDistance[k] = dot(vec4(worldPos, 1.0), uClipPlanes[k]);
```

raylib non espone `glEnable`, ma **non serve un loader**: `glEnable` è GL core, esportato da
`libGL.so`. Bastano

```c
extern void glEnable(unsigned int);  extern void glDisable(unsigned int);
#define GL_CLIP_DISTANCE0 0x3000
```

e cinque righe attorno al draw. (Alternativa pulita: `glfwGetProcAddress`, GLFW è già linkato.)

**Strada B — `discard` nel fragment shader (GLES 2/3, Web, driver ostili).** Stessi 5 piani,
test su `worldPos` interpolato, `discard` se fuori. Costa early-z e un po' di fill, ma è
portabile ovunque e a portata di `#ifdef`. Su GLES3 esiste `EXT_clip_cull_distance`, non
garantito.

**Da riportare identici dal fork** (sono raffinamenti costati bug, non abbellimenti):
apex **fuori** dalla faccia (`vpos -= n·(apex_side + 0.05·BS)`), altrimenti il frustum ha
semiangolo `atan(hw/depth)` e taglia via mezza destinazione; piano 4 ancorato alla faccia
**esterna** del nodo (al centro copre tutta l'immagine); bypass completo quando il giocatore è
dentro l'apertura (`|lx|<hw && |ly|<hh`); clamp del tilt all'indietro (`n·bn < 0` → si toglie
la componente `bn` e si ancora al centro del bordo).

### 2.5 Ricorsione (portale dentro portale) — capacità nuova

Oggi non esiste (profondità 1, vedi §1). raylib non annida `BeginTextureMode`, ma non è un
limite: la ricorsione si fa **iterativa**, che è anche l'unico modo sano di limitarne il costo.

```
stack = [(cam_giocatore, depth 0)]
per depth = MAXD..1:  renderizza i livelli più profondi per primi in RT del pool
```

Pool suggerito: `MAX_PORTALS=16` slot × `MAXD=2` livelli, RT a **risoluzione ridotta con la
profondità** (100% / 50% / 25%) e riuso aggressivo degli slot per portali fuori schermo. Al
livello massimo, il portale disegna una tinta piatta o l'ultimo frame (ghosting accettabile).

### 2.6 Il vero lavoro: il renderer voxel

Questo è il 70% dello sforzo grafico, e raylib **non** te lo dà:

- **Meshing**: greedy meshing per faccia + occlusione tra chunk vicini; formato vertice
  compatto (posizione quantizzata 8-bit in chunk-space, normale in 3 bit, AO in 2 bit, indice
  texture in 12 bit → 8-12 byte/vertice). Su worker thread; upload sul thread GL a budget
  (es. max 4 chunk/frame) per non fare stutter.
- **Texture**: `GL_TEXTURE_2D_ARRAY` (uno strato per texture di blocco) elimina il bleeding tra
  tile dell'atlas e rende gratis il mipmap. È la mancanza più fastidiosa di raylib: ~60 righe di
  shim (`glTexImage3D`/`glTexSubImage3D` via `glfwGetProcAddress`).
  *Costruito nel frattempo:* l'atlante resta un atlante, ma la dimensione della tile è quella del
  **pacchetto** e non una costante — 32 px per l'arte di questo repo, 128 px per un pacchetto CC0
  scaricato, senza ritoccare un file. Il passaggio a texture array eredita la stessa regola: una
  dimensione per pacchetto, decisa leggendo i file prima di allocare
  ([`TEXTURE_PACKS.md`](TEXTURE_PACKS.md)).
- **Luce**: propagazione flood-fill sole+blocco per chunk (modello classico dei giochi a blocchi, 4 bit
  ciascuna), impacchettata nel vertice; ricalcolo incrementale sul piazzamento blocco.
- **Culling**: frustum per chunk + **cave culling** (visibilità di attraversamento per chunk,
  precalcolata al meshing: quale faccia vede quale faccia). È quello che rende un engine voxel
  veloce, ed è il motivo per cui oggi *il collo di bottiglia misurato è il render del mondo A*.
- **Trasparenze**: bucket separato ordinato back-to-front **per camera** — quindi **per
  sessione e per vcam**, come già fa `setPortalCamera(true, …)` nel fork.
- **Draw list per vcam**: la vcam guarda terreno che la camera vera non ha mai visto. Serve una
  drawlist propria con culling proprio. **Invariante da non rompere** (è già nella memoria del
  progetto): quel percorso non deve *mai* accodare un mesh update — i chunk d'aria non hanno
  mesh in permanenza e riaccodarli ogni frame allaga la coda e inchioda il gioco.

### 2.7 Rischi grafici

| Rischio | Gravità | Mitigazione |
|---|---|---|
| Ricorsione + N sessioni = N×M pass | Media | Budget per frame, RT scalati, MAXD=2, portali fuori schermo saltati |
| `gl_ClipDistance` su driver vecchi/GLES | Bassa | Strada B (`discard`) già progettata |
| Precisione depth in RT lontani | Media | Reverse-Z + depth float32 (siamo padroni della projection: qui è più facile che su Irrlicht) |
| Nessuna ombra → look "piatto" | Bassa | AO al vertice + fog direzionale; ombre dopo |
| Meshing stutter | Media | Worker + budget upload + amortized rebuild |

---

## 3. Fattibilità server

### 3.1 Forma del processo

Un solo binario, tre modi: `--server` (headless, niente raylib linkato nel core sim), client con
server in-process (singleplayer), client puro. La regola già imparata sul fork vale ancora:
**il server in-process muore col client che l'ha lanciato** — la sopravvivenza al redirect non
si ripropone; per un mondo che deve restare in piedi si lancia un processo dedicato.

Separazione netta e non negoziabile: `core/` (voxel, ECS, mapgen, protocollo) non include mai
`raylib.h`. Solo `client/` lo fa. È ciò che rende il server compilabile senza GPU e testabile
headless in CI.

### 3.2 Rete

- **Trasporto: ENet** (C, MIT, ~10k righe, zero dipendenze). UDP con canali affidabili/inaffidabili,
  frammentazione, congestion control, sequenziamento per canale. Rispetta il vincolo "solo
  linguaggi nativi" e non aggiunge toolchain.
- **Canali**: 0 handshake/auth · 1 chunk (affidabile, ordinato, compresso) · 2 entità (inaffidabile,
  sequenziato) · 3 eventi di gioco (affidabile) · 4+ **canali dei mod** (§6.5).
- **Compressione: zstd** livello 3 sui chunk (o LZ4 se serve latenza minima).
- **Crypto**: ENet non cifra. `libsodium` per handshake X25519 + chiave di sessione, e per le
  **chiavi d'identità del giocatore** che servono in §4.4. Non è opzionale se i portali portano
  su server di terzi.
  > **Scostamento, applicato dopo M3.** La libreria è **Monocypher 4.0.3**, non libsodium.
  > Il motivo è il vincolo dietro questa riga più che la riga stessa: libsodium si compila con
  > autotools, e metterlo come sottomodulo aggiungerebbe autoconf e libtool all'elenco di ciò che
  > serve per compilare il gioco — esattamente la crescita di toolchain che §3.1 esclude.
  > Monocypher dà le stesse primitive (X25519, XChaCha20-Poly1305, BLAKE2b, ed25519 per i ticket
  > di §4.4) in due file C99. Costo: molti meno occhi sopra. Il ritorno a libsodium è una modifica
  > di build e non di protocollo, perché niente sopra `core/net/crypto.h` sa cosa c'è sotto.
  > Cosa esiste oggi: cifratura di sessione con forward secrecy e chiave del server appuntabile
  > (`--server-key`). Cosa non esiste: le chiavi d'identità del **giocatore**, che restano M6.
- **Protocollo**: header versionato + tabella di feature negoziate all'handshake, così un client
  nuovo parla con un server vecchio. Il fork oggi mette un magic `0x59415054` in testa ai suoi
  pacchetti proprio per non collidere: da zero, si progetta bene subito.

### 3.3 Simulazione, storage, mapgen

- **Tick** autoritativo a 20 Hz, disaccoppiato dal frame rate del client. Interpolazione client,
  predizione del solo giocatore locale, riconciliazione a ritroso.
- **Interest management**: per giocatore, raggio in chunk + priorità per direzione dello sguardo.
  Un portale aggiunge **un secondo centro di interesse** (la destinazione) — regola nativa qui,
  bolt-on nel fork.
- **Storage**: SQLite con WAL (Write-Ahead Logging) + compressione dei chunk con **zstd** (livello 3) — transazionale, compatto su disco e resistente a corruzioni in caso di crash/power loss improvviso. Se emerge come collo di bottiglia a carichi estremi, region file custom stile Anvil.
- **Mapgen**: FastNoiseLite (single-header C) + pipeline a stage registrabili da Lua (§6).
  Deterministica per seed: obbligatoria se vuoi testare headless.
- **Thread**: job system con pool (N-1 core): mapgen, meshing (client), lighting, I/O. Il tick
  di gioco resta single-thread (determinismo e sanità mentale); i mod girano lì.

### 3.4 Autorità e cheat

Movimento validato lato server (speed cap, `noclip` off), piazzamento/rottura validati per
distanza e permessi, inventari solo server-side. I portali introducono un caso proprio: la
teletrasportazione è *decisa dal server* (come oggi in Lua), il client la disegna. Il "moved too
fast" post-swap che oggi è aperto come BUG-10 nel fork si previene alla radice: il pacchetto di
swap porta con sé un **grace token** che sopprime il controllo di velocità per N tick sul server
di destinazione.

---

## 4. Client multi-istanza — il portale tra server diversi

### 4.1 Requisito numero uno: zero stato globale

Tutta la difficoltà del dual-client nel fork viene da lì: `Sky`, `Clouds`, la view range, il
"chi disegna il proprio avatar", l'input, l'identità del client — tutte cose che esistevano una
volta sola. Le invarianti in `AGENTS.MD` sono, tutte, cicatrici da singleton.

Regola dell'engine nuovo, applicata a compile time:

```c
typedef struct Session {           // TUTTO lo stato di un mondo vive qui dentro
    NetPeer     *peer;             // connessione a quel server
    ChunkStore   map;              // voxel + luce + mesh
    EntityStore  entities;
    ContentReg   content;          // registry blocchi/item DI QUEL server (namespaced)
    Sky          sky;  Clouds clouds;  float view_range;
    Camera3D     cam;   bool has_camera;   // false = passiva
    LuaState    *csm;              // eventuali script client-side, isolati
} Session;

Session *sessions[MAX_SESSIONS];   // 0 = attiva; le altre passive
int      active_session;
```

Niente funzione di `core/` o `client/world/` prende dati impliciti: la sessione è **sempre** un
parametro. Lo swap diventa `active_session = i` più un pugno di transizioni esplicite, invece
delle promozioni/demozioni chirurgiche di oggi.

### 4.2 Costi — c'è già una misura

Dal lavoro dual-client sul fork: **renderizzare il mondo B costa il 3-7% del frame**; il collo di
bottiglia è il render del mondo A. Due conseguenze dirette:

1. Il multi-sessione **non è il problema di performance**. 3-4 mondi vivi contemporaneamente sono
   plausibili con budget e view range ridotti per le passive.
2. Il guadagno più grosso di un engine nuovo non è nei portali: è nel **renderer del mondo
   principale** (cave culling, vertici compatti, meno draw call), dove l'engine di partenza lascia molto sul
   tavolo.

Budget per sessione passiva: view range ~1/3, niente audio, niente particelle, niente predizione,
tick di mesh a priorità bassa, RT alla risoluzione del portale (non dello schermo).

### 4.3 Protocollo di link e swap

Il modello attuale è giusto, va solo ripulito. Il **client** fa tutto il lavoro; i due server non
si parlano per un attraversamento (fondamentale: niente fiducia reciproca da stabilire per passare
da un mondo all'altro).

**Eccezione dal 2026-10-27: i server amici** (`/friend`, `src/server/admin.h`). Due server che si
mettono a vicenda nella propria lista si scambiano, su una porta TCP loro (`--federation`), righe
firmate con la chiave di firma di ciascuno: i ban permanenti per chiave, chi è online (`/where`) e
una chat (`/fed`). La fiducia c'è perché è stata data a mano, da entrambe le parti, ed è
revocabile (`/friend remove` toglie anche i suoi ban). Non passa di lì niente che serva per
attraversare: un portale verso un amico funziona come verso chiunque. Lo zaino non viaggia tra
server (vedi TODO.md, "Server administration").

```
1. Server A → client:  PORTAL_LINK { portal_id, host, port, world, dst_pos, dst_yaw, ticket }
2. Client:             apre sessione passiva su B (handshake normale + ticket)
3. Client:             quando la passiva è Ready → dichiara lo stato a A, con HEARTBEAT
4. Attraversamento:    A e B validano; swap in place; A parcheggia il fantasma, B promuove
5. Fallback:           nessuna passiva pronta → redirect classico con schermata di carico
```

Invarianti già pagate, da riportare nella spec del protocollo (non "da ricordare"):

- Lo stato della passiva si **ripete a timer** (2 s), mai fire-and-forget: il server principale
  scarta i pacchetti in-game finché il client non è attivo lì, e il mondo A può caricare per
  decine di secondi dopo che la passiva è già pronta.
- Una sessione passiva si dichiara **nell'handshake**, non via chat: il server deve saperlo
  *prima* di far comparire l'avatar fantasma a chiunque.
- Una sessione si identifica per **nome del mondo (+porta)**, mai per stringa di indirizzo:
  `127.0.0.1` e l'IP LAN sono lo stesso mondo, e confrontare indirizzi trasforma ogni ritorno a
  casa in un redirect.
- Lo swap **non azzera i tasti premuti**: solo gli edge (`pressed/released/wheel`). Entrare in una
  porta con W premuto deve continuare a camminare.
- Lo swap **ridecide chi disegna il proprio avatar**: la sessione promossa sì, la demossa no.
- Il cielo della sessione promossa si prende dal `Sky` **vivo** della passiva, mai dal replay
  degli eventi (un server manda `set_sky` una volta sola: al secondo giro il replay è vuoto).
- `init_secondary()` è **idempotente**: due connessioni con lo stesso nome vengono rifiutate dal
  server e uccidono la passiva che funzionava.

### 4.4 Identità e autenticazione (il vero salto di qualità)

Oggi la sessione passiva entra come utente fantasma `<nome>_pv` con password vuota. Non è
accettabile in un gioco vero. Progetto:

- Ogni giocatore ha una **coppia di chiavi** (ed25519, `libsodium`) generata al primo avvio.
  L'identità è la chiave pubblica; il nome è solo un'etichetta.
- Il server A emette un **ticket** firmato `{pubkey, portal_id, exp, scope=OBSERVE|ENTER}`.
  Il client lo presenta a B. B lo accetta se: la firma è valida, A è nella sua lista di peer
  fidati **oppure** B accetta osservatori anonimi (impostazione per-mondo).
- Scope `OBSERVE` = niente avatar, niente inventario, niente interazione: solo chunk streaming
  in una sfera attorno al punto d'uscita. Riduce anche la superficie di abuso (nessun "entro
  passivo in 50 server per minare dati").
- Rate limit: N sessioni passive per IP, timeout se il giocatore non attraversa mai.

### 4.5 Contenuti e collisioni di asset

Due server = due registry di blocchi e due set di media, e oggi questo produce già warning di
collisione media nel caso cross-game. Progetto:

- Media **content-addressed** (BLAKE3 del file), cache condivisa su disco tra tutte le sessioni.
  Due server che spediscono la stessa texture la scaricano una volta sola.
- ID di contenuto **per-sessione**: `session->content` mappa `id numerico → nome stringa`.
  Nessun id globale, mai. Le texture finiscono in strati di array texture allocati per sessione,
  con un budget.
- Handshake: il client manda la lista di hash che ha già; il server manda solo il delta.

---

## 5. Scripting

### 5.1 I criteri, prima dei nomi

Un linguaggio di scripting per questo gioco deve reggere cinque cose insieme. Sono in ordine di
peso, e sono il motivo per cui la risposta non è automaticamente "Lua come l'engine di partenza":

1. **Sandbox per codice non fidato.** I portali portano su server di altri; prima o poi qualcuno
   vorrà mandare script al client. Un runtime dove "sandbox" significa "cancella `io` e spera"
   non basta.
2. **Costo dei binding.** L'engine è C/C++: ogni API nuova non deve costare 50 righe di glue.
3. **Prestazioni prevedibili.** Niente pause GC visibili a 60 fps con migliaia di entità.
4. **Vincolo del progetto**: solo Lua e C/C++, nessuna toolchain nuova da installare — né per
   te, né per chi scriverà i mod.
5. **Continuità con l'engine di partenza**, perché §7.5 (compat layer) e ~9 700 righe di Lua già scritte
   pesano.

### 5.2 Il campo, valutato sul serio

| Runtime | Anno / stato | Sandbox | Perf | Binding | Toolchain per l'autore di mod | Verdetto |
|---|---|---|---|---|---|---|
| **Luau** | attivo, C++, MIT | **Progettata per codice ostile**: globals congelati, no `getfenv/setfenv/loadstring`, no `os/io`, interruzioni a budget, memory cap per VM | Interprete ~2-3× PUC Lua; **native codegen** x86-64/arm64 per il codice caldo | API stile `lua_State` (no FFI) | Nessuna (opzionale: type checker + LSP) | **Scelto (primario per gameplay/nodi/UI)** |
| **WASM** (WAMR / Wasmtime) | attivo, C/Rust, Apache/MIT | **Isolamento totale (ISA/memoria virtuale)**: zero accessi non concessi dall'host | **Quasi nativa (JIT/AOT)** | Marshalling / C-ABI / Component Model | **Sì: richiede compilatore (Rust/Zig/C++/C#)** | **Scelto come estensione per plugin pesanti (§5.8, §6.6)** |
| **TypeScript / JS** (Hermes / QuickJS-ng) | attivo, C++, MIT | Ottima (realm isolati; Hermes con bytecode AOT compatto) | Buona; GC deterministico su Hermes | Verboso su QuickJS, strutturato su Hermes | TypeScript opzionale | Alternativa per modder JS/TS; supportabile tramite transpiler Luau (`roblox-ts`) o runtime Hermes |
| **LuaJIT 2.1** | rolling, C, MIT | Debole se c'è la FFI (= accesso pieno alla macchina) | Il più veloce del gruppo | **FFI: zero glue** | Nessuna | ~~Secondo backend~~ — la ragione (compat legacy) è caduta, §5.3 |
| Lua 5.4 (PUC) | ufficiale | Media | Lenta | manuale | Nessuna | Fallback di portabilità |
| Wren | quasi fermo | Media | Media | Semplice | Nessuna | No: ecosistema fermo |
| Umka | attivo, C, piccolo | Media | Buona (tipizzato) | Semplice | Nessuna | No: comunità minuscola, nessuna continuità |
| Janet / Fennel | attivi | Media | Media | Media | Fennel: nessuna | No: sintassi Lisp = barriera per i modder |
| C# (.NET NativeAOT/Mono) | attivo | Debole senza AppDomain | Ottima | Buona | **SDK .NET obbligatorio** | No: runtime pesante (30-80 MB) |
| Python (CPython) | attivo | Pessima da sandboxare | GIL, startup lento | Media | Nessuna ma pesante | No |

### 5.3 Scelta: Luau primario, LuaJIT come secondo backend

**Perché Luau e non LuaJIT come prima scelta**, avendo un gioco che apre porte su macchine altrui:

- È **l'unico Lua moderno progettato per eseguire codice di sconosciuti**: una grande piattaforma esegue milioni
  di script utente sopra questa VM. Globals congelati per default, `getfenv`/`setfenv`/`loadstring`
  rimossi, `io`/`os`/`debug` assenti, budget di interruzione e limite di memoria per VM sono
  *primitive del runtime*, non convenzioni tue.
- **Tipi graduali**: `--!strict`, annotazioni, type checker e LSP ufficiali. Su un'API di gioco con
  centinaia di funzioni, la differenza in usabilità per chi scrive mod è enorme rispetto a Lua 5.1
  nudo — ed è ciò che l'utente chiedeva con "più moderno".
- **Sintassi moderna** senza rompere Lua: `continue`, interpolazione di stringhe, `+=`, iterazione
  generalizzata, `type`/`export type`.
- **Native codegen** (x86-64 e arm64) sul codice caldo: non è il JIT di LuaJIT, ma copre il caso
  "sistema ECS in Lua chiamato 20 volte al secondo su 5 000 entità".
- C++ con licenza MIT, buildabile con CMake: rientra nel vincolo "solo Lua e C++".

**Perché LuaJIT restava come backend opzionale** — e perché adesso non è più lavoro in piano:

1. ~~**I mod dell'engine di partenza legacy non girano su Luau senza modifiche.**~~ Era la prima ragione, e cade
   con il layer di compatibilità (M11 annullata, §7.5): non c'è più un mod Lua 5.1 da far girare.
2. **FFI per il codice trusted**: i sistemi ECS pesanti e il codice dell'engine scritto in Lua
   otterrebbero struct C native senza glue. Resta l'unica ragione, e nessun numero l'ha ancora
   chiesta — il profiler di M9 dice dove va un frame, e non è nei binding.

```
script_* (interfaccia, ~25 funzioni)
   ├── backend "luau"    → tutti i mod: nuovi, scaricati, qualunque cosa non fidata  [l'unico]
   └── backend "luajit"  → mai scritto; resterebbe per FFI e hot path trusted        [non in piano]
```

Stessa forma già scelta per `gfx_*`: un'interfaccia sottile, così la decisione resta reversibile.
**Oggi il runtime è uno solo**, ed è il risultato che il criterio di revisione qui scritto
cercava — per un'altra strada: non perché il compat girasse su Luau con poco shim, ma perché il
compat non si fa (§7.5).

### 5.4 Costo dei binding senza FFI (il vero prezzo di Luau)

È l'unico svantaggio reale rispetto a LuaJIT, e va pagato con del tooling, non a mano:

- Le API si dichiarano **una volta** in una tabella descrittiva (nome, argomenti, tipi, flags);
  uno **script Lua a build time** genera sia il glue C++ (`lua_pushnumber`/`luaL_check*`) sia il
  file `.d.luau` di definizioni per il type checker sia la documentazione Markdown. Generatore in
  Lua = nessuna toolchain nuova (vincolo 4 rispettato).
- Le strutture calde (posizioni, blocchi, componenti ECS) non passano per tabelle: si usano
  **userdata con metatabelle** e iteratori batch (§6.1), così il costo per entità resta in C++.

### 5.5 Authoring moderno, opzionale

Chi vuole può scrivere i mod in **Luau tipizzato** (`--!strict`), oppure in TypeScript
transpilato a Luau (con un transpiler TS→Luau) — ma **il gioco non lo richiede mai**: un mod resta un
file `.luau` che si apre in un editor di testo e funziona. Questa è la linea da non superare, ed è
il motivo per cui C#, WASM e simili sono fuori: appena l'autore di un mod deve installare un SDK,
la mod library muore.

### 5.6 Sandbox e sicurezza

Il fork oggi ha `secure.trusted_mods` e mod che leggono/scrivono una directory condivisa. Nel
nuovo engine "sandbox" significa qualcosa di preciso, e Luau ne regala metà:

- **Una VM per mod** (non un env condiviso): isolamento di crash e di globals.
- **Manifest con capability** dichiarate: `fs:read <dir>`, `net:http <host>`, `native`, `exec`.
  Niente manifest → niente `io`, `os`, `package`, punto. Il caricatore le rimuove *prima* di
  eseguire una riga del mod. Su Luau molte non esistono nemmeno da rimuovere.
- **Il backend LuaJIT (e quindi la FFI) è una capability di livello massimo**, equivalente a
  "questo mod è codice nativo": avviso esplicito all'utente all'installazione, mai concessa a un
  mod arrivato dalla rete.
- **Quota memoria** per VM e **budget di interruzione** per tick (`lua_callbacks().interrupt` su
  Luau, hook a conteggio istruzioni su LuaJIT): un mod in loop infinito viene interrotto, non
  congela il server.
- **Mod lato client** — ~~di default nessuno scaricato dal server viene eseguito; se mai, solo
  su Luau, firmati e in whitelist~~. **Deciso il 2026-09-13 (M13): un mondo può mandare al
  client codice Luau, e il client può rifiutarlo.** La frase "è la differenza tra un gioco e una
  botnet" resta vera, ed è il motivo delle condizioni:
  - solo **Luau**, mai codice nativo; una **VM per mod e per sessione**, che muore con la
    sessione;
  - **capability solo da client**: disegnare, HUD, input, il canale del proprio mod, leggere il
    mondo della propria sessione. **Niente file, niente rete, niente processi**, e nessuna
    capability del manifest che un mod installato a mano potrebbe chiedere;
  - **quota di memoria e budget di interruzione**, come sul server: un mod che va in loop si
    ferma, il client no;
  - **l'autorità resta al server**: il codice che arriva prevede e disegna, non decide. Un client
    che lo rifiuta non guadagna niente — il server controlla lo stesso — e perde quello che il
    mondo voleva mostrargli;
  - **il rifiuto è del giocatore**: per tutti i mondi o per un mondo, e un mondo rifiutato si
    gioca lo stesso, senza il suo codice.

  **Fatto il 2026-09-13** (TODO.md OPEN-49). Un mod dice dove gira con `side = server | client |
  both`; il server impacchetta quelli per i client e li offre come media (`worldcode.<mod>.cnpmod`,
  al massimo 1 MB come ogni media); il client li carica nel `ModHost` del mondo in cui è, con una
  tabella di funzioni da cui è tolto tutto ciò che scrive il mondo o registra contenuti, nessuna
  capability e quota e interruzione non oltre i default. Il rifiuto è `--world-code refuse`, o la
  chiave di firma del mondo in `world_code_refused.txt` nella cache dei media.

### 5.7 Hot reload e determinismo

- Ogni mod dichiara il proprio stato serializzabile; il reload ricrea lo stato dal serializzato e
  ri-registra gli handler. Registry con generazione: al reload si scartano solo le voci del mod.
- Il tick di gioco è deterministico per seed+input: fondamentale per test headless (già oggi
  esiste una pratica di test headless con mod usa-e-getta e grep sul log — quella va promossa a
  framework di prima classe, non workaround).

### 5.8 Strategia Ibrida di Modding e Scripting (Raccomandazione Architetturale)

Per combinare massima accessibilità per i modder casuali e potenza nativa per i sistemi complessi,
l'architettura adotta una **strategia su 3 livelli**:

1. **Livello 1 — Gameplay e Blocchi (Luau)**:
   - Default per il 90% dei mod (blocchi, item, ricette, UI, logica di interazione).
   - Zero toolchain per chi scrive il mod: file `.luau` modificabili a caldo.
   - Tipi statici opzionali (`--!strict`), LSP integrabile in VS Code, sandbox nativa a prova di crash.
2. **Livello 2 — Sistemi ad Alte Prestazioni e Modding Polyglot (WASM via WAMR)**:
   - Per generatori di mondo procedurali complessi, simulatori di fluidi, o fisica personalizzata.
   - I modder avanzati possono compilare in `.wasm` da **Rust, Zig, C++, C# o AssemblyScript**.
   - Sandbox ad isolamento totale della memoria (ISA-level): nessun accesso a syscall o memoria non autorizzata.
3. ~~**Livello 3 — Compatibilità Legacy & Hot Path Trusted (LuaJIT 2.1)**~~:
   - Era per il layer di compatibilità dell'engine di partenza. Caduto quello (§7.5, 2026-09-12), resta un livello
     senza un utente: nessun mod da far girare, e la FFI non l'ha ancora chiesta nessun numero.

---

## 6. Estendere le *meccaniche*, non solo i blocchi

Registrare un blocco è facile e lo fanno tutti. Il problema vero è: **come fa un mod ad
aggiungere un sistema che l'engine non prevedeva** (un tipo di fluido, un circuito, un portale
non-euclideo, un veicolo)? Sei leve, in ordine di potenza crescente.

### 6.1 ECS moderno: adozione di Flecs (C/C++)

Invece di scrivere un motore ECS da zero a mano con inevitabile debito tecnico, la raccomandazione
è integrare **Flecs** (C99/C++20, licenza MIT, zero dipendenze):
- **Archetype storage nativo**: query vettorizzate e cache-friendly ad altissime prestazioni (milioni di entità/sec).
- **Gerarchie native e relazioni**: supporto di prima classe per coppie/relazioni (es. `(PortalChildOf, ParentPortal)`, `(InWorld, SessionId)`), fondamentale per il multi-mondo.
- **Binding pronti**: Flecs espone già binding C/C++, Lua/Luau e integrazione WASM.
- **REST / Web Dashboard di debug**: Flecs include un modulo explorer per ispezionare visivamente lo stato di tutte le entità in tempo reale nel browser durante lo sviluppo.

Entità = id + set di componenti; il mod dichiara componenti (schema tipizzato → layout compatto
lato C) e registra sistemi in fasi ordinate:

```luau
--!strict
core.ecs.component("mymod:magnet", { radius = "f32", strength = "f32" })
core.ecs.system("mymod:attract", {
    phase   = "pre_physics",             -- input | pre_physics | physics | post_physics | net
    after   = { "core:gravity" },
    query   = { "mymod:magnet", "core:transform" },
    run = function(dt: number, it: core.Iter)   -- iterazione batch, non per-entità
        for i = 1, it.n do
            continue                      -- sintassi Luau; il tipo di `it` è controllato
        end
    end,
})
```

La query è un *archetype iterator* eseguito da Flecs in C: il costo per entità resta nativo, Lua vede
un batch compatto. Questo è ciò che permette a un mod di aggiungere *fisica* nuova, non solo comportamento.

### 6.2 Trait sui nodi, invece di comportamenti hardcoded

Nell'engine nuovo, "solido", "liquido", "scalabile", "superficie-di-portale", "conduce segnale"
non sono flag di un enum chiuso ma **trait** che un mod può definire e che l'engine consulta
tramite tabelle di dispatch compilate al momento della registrazione:

```lua
core.trait.define("mymod:conductive", { on_signal = "function", delay = "u8" })
core.node.register("mymod:wire", { traits = { ["mymod:conductive"] = { delay = 1 } } })
```

Le facce solide direzionali (che nel fork sono bit in `param2` e patch a `mapnode.cpp`) diventano
un trait standard: `solidity = { [FACE_NX] = true, ... }`. **Il portal_block di tipo 2 smette di
essere una patch all'engine.**

### 6.3 Bus di eventi tipizzato

Eventi con schema dichiarato, handler con priorità e possibilità di cancellare/modificare; il
core stesso emette solo eventi (nessun path "speciale" riservato all'engine). Se
`portal:pre_teleport` è cancellabile, un mod può fare "i portali non funzionano nell'Inferno"
senza toccare `yaportal`.

### 6.4 Render graph estensibile

La leva che manca completamente all'engine di partenza oggi. Il client espone il grafo dei pass come dati:

```lua
core.render.add_pass("mymod:heat_haze", {
    after   = "core:transparent",
    shader  = "mymod:haze",
    inputs  = { color = "core:scene", depth = "core:depth" },
    enabled = function() return player_in_desert end,
})
```

Con questo, "aggiungere un nuovo tipo di portale con effetto visivo proprio", oggi impossibile
senza forkare, diventa un mod. È anche l'assicurazione contro il ripetersi di questa stessa
analisi tra due anni.

### 6.5 Canali di protocollo per mod

Un mod dichiara messaggi con schema e ottiene un canale ENet dedicato:

```lua
local ch = core.net.channel("yaportal:link", { reliable = true })
ch:define("state", { portal = "u16", ready = "bool", world = "string" })
ch:send_to(player, "state", { portal = 3, ready = true, world = "backportal" })
```

Nessun opcode nuovo nell'engine per ogni feature — che è esattamente ciò che il fork oggi deve
fare (`0x65`-`0x68`, `0x54`).

### 6.6 Plugin nativi (C ABI)

Per ciò che lo scripting non può reggere (mapgen custom pesante, fisica di veicoli, compressione):
mod che spediscono una `.so`/`.dll` con una tabella di puntatori a funzione versionata. Non
sandboxati, opt-in esplicito dell'utente, versione ABI controllata al caricamento.

**Alternativa moderna, da preferire quando la performance lo consente: WASM** (WAMR, C, Apache-2)
al posto di `dlopen`. Stesso caso d'uso — codice compilato da un altro linguaggio — ma
**isolato per costruzione**: un plugin WASM che va in crash o tenta una syscall non porta giù il
server, e può arrivare dalla rete senza che l'utente firmi cambiali. Il prezzo è il marshalling
(niente valori dinamici attraverso il confine) e il fatto che l'autore deve compilare. Ordine
consigliato: `dlopen` per i plugin locali dell'utente, WASM per tutto ciò che è distribuito.

---

### 6.7 Superficie API minima per riportare `yaportal` (criterio di accettazione concreto)

Inventario reale delle chiamate usate oggi dai tre mod. È la lista della spesa dell'engine nuovo:
se questa è coperta, il mod si porta; se manca un pezzo, si sa esattamente quale.

**Stock dell'engine di partenza (il grosso, ~120 chiamate distinte):**

| Gruppo | Cosa serve davvero |
|---|---|
| Registrazione | `register_node` (~150 nodi generati in loop), `register_tool`, `register_craftitem`, `register_entity`, `register_craft`, `register_alias`, `register_lbm` (8: ri-registrano bottoni/vent/porte/pedestal/display al caricamento del mapblock), `register_abm`, `register_biome`, `override_item` |
| Callback | `register_globalstep` (13 stepper indipendenti), join/leave/die/respawn, `on_dignode`, `on_player_hpchange`, `on_mods_loaded`, `on_shutdown`, `on_player_receive_fields`, `after` |
| Definizioni nodo | drawtype `nodebox` (×14) e `mesh`, `node_box`/`selection_box`, `paramtype2` facedir+wallmounted, `special_tiles`, `groups`, `drop`, `light_source`, `glow`, `pointable`, `walkable`, `use_texture_alpha`, `sunlight_propagates` |
| Modificatori texture | `[combine` (il display 7-segmenti del clock è composto a runtime!), `[colorize` |
| Mappa | `get/set_node`, `swap_node`, `get_meta` (23 siti) con tipi misti, `hash_node_position`, `emerge_area`, `raycast`, `get_voxel_manip`, `is_protected`, `item_place` |
| Player/Object | pos/vel/accel, look dir/yaw/pitch, `get_player_control`, `get/set_properties`, `set_physics_override`, `get_inventory`, `get_player_information`, `get_sky` |
| UI | `show_formspec` (9 siti), `formspec_version[4]`, `textlist`, `dropdown`, `checkbox`, `field_close_on_enter`, `explode_textlist_event` |
| Storage | `get_mod_storage`, `serialize`/`deserialize`, `write_json`/`parse_json`, `get_worldpath`, `get_modpath` (32 siti — rilevamento del gioco ospite) |
| Particelle/suono | `add_particle(spawner)`, `sound_play` |

**Fork-only (~20 chiamate) — nel nuovo engine devono essere API di prima classe, non patch:**
`set_portals`, `set/clear_portal_cam_hint`, `register/update_portal_sky_type`,
`set/clear_portal_sky`, `set/clear_portal_xworld_dest`, `set/clear_xworld_target`,
`redirect_player`, `xworld_swap_player`, `player_dual_ready`, `xworld_exchange(_posts)`,
`get_bind_port/address`, `player:portal_teleport(...)` (atomico: pos+yaw+pitch+roll+velocità+cielo),
`player:set/get_look_roll`, `set_look_pitch_animate`, `set_pos_look`, più la chiave
`physics_override.floor_portal_cb_shift` e i **tre gruppi interpretati dal C++**
(`portal_block`, `portal_frame`, `portal_slab` + codifica carve nei bit 3-6 di `param2`).
Nel modello §6.2 questi tre gruppi diventano **trait dichiarati da Lua** e smettono di essere
codice di engine.

**Due dipendenze "brutte" da sostituire con feature vere** (oggi sono scappatoie):
- `request_insecure_environment()` → `io`/`os` reali e `io.popen` per **lanciare altri server**.
  Nel nuovo engine: un **world supervisor** nel core (API `core.world.spawn{game, port}`), con
  permessi espliciti — non un buco nella sandbox.
- `request_http_api()` + servizio `PortalExchange` su `porta+5000` → **servizio di registry**
  nativo, con firma dei record (§4.4), non HTTP in chiaro con chiave condivisa.
- Monkey-patch di `register_abm` e `rawset(_G, …)` per aggirare il limite dei 200 local per
  chunk: sintomi di un'API senza namespace propri. Nel nuovo: moduli per mod, niente `_G`.

**Nota che vale oro:** oggi il dual-client condivide la cache texture *per nome* tra i due giochi,
e l'unica difesa è il prefisso `yafps_` su ogni file. È esattamente il bug che §4.5 (media
content-addressed + id per-sessione) elimina per costruzione.

---

## 7. Pregi e difetti — verdetto onesto

### Pro del rewrite su raylib

1. **L'architettura nasce con i portali dentro.** Multi-sessione, render graph, drawlist per
   camera virtuale, culling separato: tutte prime classi invece di innesti.
2. **Il debito di Irrlicht sparisce.** Irrlicht è mantenuto dall'engine di partenza stesso; oggi paghi il suo
   fixed-function stack, il suo scene manager, i suoi shader.
3. **Niente più inseguire upstream.** Un fork mai mergiabile è una tassa a vita: ogni rebase è un
   rischio di regressione sulle invarianti di `AGENTS.MD`.
4. **Performance potenzialmente molto superiori.** Sul mondo principale (che *è* il collo di
   bottiglia misurato): cave culling, vertici compatti, meno draw call. Sui portali: oggi ogni
   portale attivo costa una scena intera a risoluzione piena senza culling; con RT scalati e un
   grafo di visibilità dei portali il costo cala di un fattore, non di una percentuale.
5. **Scriptabilità migliore fin dall'inizio**: FFI, ECS, render graph, canali di rete per mod.
6. **Base di codice comprensibile per una persona sola**: 40-60k righe mirate contro 214k+Irrlicht.
7. **Distribuzione più semplice**: raylib è statico e minuscolo; niente AppImage da 85 MB.
8. **La specifica è già scritta**: `PORTAL_SYSTEM.md` + le invarianti sono metà del lavoro di design.

### Contro (e sono pesanti)

1. **I contenuti.** Un gioco completo per l'engine di partenza è anni-uomo di lavoro. Senza un gioco, l'engine è una demo. È **il**
   rischio del progetto, non la grafica. Questo contro era scritto quando la risposta prevista era
   far girare quei contenuti (§7.5); caduta la compatibilità, resta in piedi tale e quale e la sola
   risposta è scegliere un gioco che una persona sola finisce.
2. **Tutto ciò che è noioso e invisibile**: inventari, craft, GUI, i18n, salvataggi, migrazioni,
   mob e AI, meteo, suoni, accessibilità, controller, multipiattaforma.
3. **Nessuna community il giorno 1.** L'engine di partenza porta mod, server, giocatori, wiki.
4. **Il tempo attuale è speso su feature che oggi funzionano già.** Sei a build 8, con crafting,
   cross-game e cross-server funzionanti: un rewrite li rimette tutti a zero.
5. **raylib è pensato per giochi piccoli.** Nessuno ha spedito un voxel-MMO con raylib; ogni
   sistema serio (culling, texture array, threading) lo aggiungi tu — raylib non ti aiuta, si
   limita a non ostacolarti.
6. **Rischio bus-factor 1.** Un engine proprio senza community è un engine che muore con l'autore.

### Alternative al rewrite completo

| Opzione | Costo | Pro | Contro |
|---|---|---|---|
| **Restare sul fork dell'engine di partenza** | 0 | Funziona oggi, contenuti gratis | Tassa di rebase a vita, tetto di performance, architettura ostile al multi-sessione |
| **Rewrite raylib (questo doc)** | 6-10 mesi al giocabile | Controllo totale, portali nativi | Zero contenuti, zero community |
| **Engine generalista + estensioni C++** | 3-5 mesi | Editor, UI, asset pipeline, community | Portali richiedono comunque viewport+matrici custom; multi-sessione = due `SceneTree` (difficile); GDScript ≠ vincolo Lua; engine grosso da capire |
| **bgfx / sokol_gfx invece di raylib** | +1 mese vs raylib | Multi-backend (Vulkan/D3D/Metal), threading di render vero | Più cerimonia, meno "batterie incluse"; sokol è C (ok), bgfx è C++ |
| **SDL3 + GL puro** | +1-2 mesi vs raylib | Zero astrazioni tra te e la GPU | Riscrivi ciò che raylib regala (immagini, font, audio, math) |

**Nota su bgfx/sokol:** se il vero obiettivo è *un engine serio*, `sokol_gfx` (C, single header,
GL/Metal/D3D11/WebGPU) è tecnicamente la scelta più solida di raylib per il renderer, e si può
tenere raylib solo per finestra/input/audio... oppure `sokol_app`. **Scelta adottata: raylib
dietro `gfx_*`** — velocità di prototipo ora (verificato che espone tutto il necessario), con
un'interfaccia sottile di ~30 funzioni che rende la migrazione a sokol un lavoro di giorni.

L'interfaccia `gfx_*` da rispettare (tutto il resto del client passa da qui, mai da `raylib.h`):

```c
gfx_init/shutdown/begin_frame/end_frame
gfx_buffer_create/update/destroy          gfx_vertexlayout_create
gfx_texture_create/update/destroy         gfx_texture_array_create   // shim GL con raylib
gfx_shader_create/destroy                 gfx_uniform_set{f,v4,mat4,int}
gfx_rt_create/destroy/begin/end           gfx_rt_texture
gfx_draw(buffer, count, offset)           gfx_draw_instanced
gfx_state_set(depth, cull, blend, colormask)
gfx_clip_planes_set(vec4 *planes, int n)  // gl_ClipDistance o discard, dietro le quinte
gfx_projection_set(mat4)  gfx_view_set(mat4)   // punto d'ingresso di Lengyel
```

### 7.5 Strategia contenuti — ~~il layer di compatibilità con l'API dell'engine di partenza~~ (annullata 2026-09-12)

> **Questa sezione è storia, non piano.** Il layer `compat_layer/` non si fa: l'engine di partenza è stato
> l'ispirazione, non l'obiettivo, e questo engine non deve far girare i mod di qualcun altro
> (M11 in §8). Resta scritta perché l'analisi qui sotto è il motivo per cui l'API nativa ha la
> forma che ha — trait invece di enum, UI dichiarativa, chunk 32³ — e perché il costo che
> nominava è reale e adesso lo si paga: **i contenuti si scrivono qui**.
>
> Quello che di concreto l'engine aveva da fare per il layer è stato fatto e vale da solo:
> `param2` come trait, drawtype nodebox+mesh, scheduler ABM/LBM. Il resto — parser formspec,
> `[combine`/`[colorize`, VoxelManip, alias di mapgen, backend LuaJIT — non si fa.

Deciso l'obiettivo "clone del classico gioco a blocchi completo", la parità di contenuti è **il** problema. Due
strade, e solo una regge per una persona sola:

| Strada | Costo | Esito |
|---|---|---|
| Riscrivere i contenuti da zero | anni-uomo | Il progetto muore prima |
| ~~Far girare i mod dell'engine di partenza sul nuovo engine~~ | 8-12 settimane | I giochi esistenti, tutta la mod library, **e i 9 700 righe di Lua già scritte** girano con modifiche minime |

**Come**, senza sporcare l'architettura nuova: il compat layer è **un mod Lua**, non codice di
engine. L'engine espone la sua API nativa pulita (§6); `compat_layer/` è un pacchetto Lua che
costruisce la tabella legacy/`core` sopra di essa. Gira sul **backend LuaJIT** (§5.3): i mod
esistenti dell'engine di partenza sono Lua 5.1 e usano `setfenv`/`getfenv`/`loadstring`, che su Luau non esistono.

```lua
-- compat_layer/init.lua (schema)
local M = {}
function M.register_node(name, def)
    core.node.register(name, translate_def(def))   -- drawtype/nodebox/param2 → trait
end
function M.register_abm(def)  core.sched.abm(def)  end
function M.show_formspec(pl, fid, spec)  core.ui.show(pl, formspec_parse(spec))  end
rawset(_G, "legacy", M); rawset(_G, "core", M)
```

Quello che l'**engine** deve fornire perché il layer sia sottile (e non una reimplementazione di
L'engine di partenza travestita):

- `param2` a 8 bit per nodo con semantica `facedir`/`wallmounted` come **trait standard**;
- drawtype `nodebox` e `mesh` con `node_box`/`selection_box` (14 nodebox nel solo `yaportal`);
- scheduler ABM/LBM (tick per area con probabilità/intervallo, e hook "al caricamento del chunk");
- metadata per nodo tipizzati e per-item;
- VoxelManip equivalente (accesso a blocco grezzo per il mapgen dei mod);
- **un parser formspec** — la parte più sgradevole: linguaggio a stringhe, 20+ elementi. Si copre
  il sottoinsieme reale (button/field/label/box/dropdown/textlist/checkbox/list/item_image +
  `formspec_version[4]`), non la grammatica intera;
- media per nome con i modificatori `[combine` e `[colorize` (il display del clock li usa a
  runtime).

**Costo onesto:** ~60-70% dell'API dell'engine di partenza stock. Non è gratis, e va detto chiaro: **adottare
questa strada significa ereditare alcuni vincoli di design dell'engine di partenza** (semantica `param2`,
formspec, mapblock 16³). La mitigazione è tenerli confinati nel layer: il core nuovo resta libero
(chunk 32³ internamente, UI dichiarativa nativa, trait invece di enum), e il compat traduce.

**Ordine consigliato:** il layer NON si fa presto. Si fa in M11, dopo che l'API nativa si è
assestata su un gioco vero (M7): un compat scritto sopra un'API instabile si riscrive due volte.

> Questo ordine ha retto fino in fondo: l'API si è assestata su M7, M8 e M9, e la scelta di non
> fare il layer è arrivata prima di scrivere una riga di compat. Quello che si è perso è zero
> lavoro; quello che si è guadagnato sono tre feature d'engine che il layer avrebbe chiesto e che
> il gioco usa comunque.

---

## 8. Piano di implementazione

Effort: singolo sviluppatore a tempo pieno con assistenza AI. "sett." = settimane.

### M-1 — Chiudere bene il fork prima di congelarlo (1 sett.) · **obbligatoria**

Il fork viene congelato: se non si chiude bene, non esiste più nulla di funzionante a cui
tornare. Prima del freeze:

- Release taggata e riproducibile (AppImage + Flatpak, `BUILD_NUMBER` bumpato) dell'ultimo stato
  buono, con le build già in repo.
- Ogni bug aperto scritto in `TODO.md` con sintomo, riproduzione e ipotesi — non nella memoria
  di chi ci lavora.

  > **Corretto in M0:** questo punto citava `BUG-10` ("moved too fast" post-swap) come aperto,
  > ma il `TODO.md` del fork lo dà **risolto dal 2026-07-28**. Al momento del freeze risultavano
  > aperti `BUG-1..BUG-6` e `BUG-9`; risolti `BUG-7`, `BUG-8`, `BUG-10`, `BUG-11`. La mitigazione
  > del *grace token* descritta in §3.4 resta valida come progetto per il nuovo engine.
- Commit e **push su entrambi i remoti** di tutto il lavoro non pushato (engine `portal-fork` e
  outer), submodule allineato. Oggi ci sono commit locali non pushati: congelare con quelli
  fuori sync equivale a perderli.
- `README.md` con una riga: stato = congelato, dove prosegue il lavoro.

**Uscita:** `git status` pulito su entrambi i repo, tag remoto, artefatti scaricabili.

### M0 — Spike di validazione (3 sett.) · **KILL SWITCH**

L'unico obiettivo è falsificare le tre ipotesi rischiose, con codice buttabile.

- Finestra raylib **già dietro `gfx_*`** (§7 — l'interfaccia nasce qui, non dopo), un chunk 32³
  meshato a mano, movimento FPS con collisione AABB.
- Un portale: RT, camera virtuale con la trasformata di `PORTAL_SYSTEM.md` §1.3, matrice di
  Lengyel via `rlSetMatrixProjection`, 5 clip plane via `gl_ClipDistance` + `glEnable` dichiarato
  a mano, quad tunnel con UV screen-space.
- **Luau** integrato con una manciata di binding generati, un mod che piazza blocchi e sposta un
  portale a runtime.

**Criteri di uscita (tutti e tre, o si ferma qui):**
1. Attraversamento visivamente continuo — nessun frame che mostra la geometria tra vcam e portale.
2. Il portale resta corretto con la camera *dentro* la cornice (il caso che rompe Lengyel).
3. ≥ 200 FPS con 2 portali a 1080p su hardware di sviluppo, con il costo del binding Luau
   misurato e sotto l'1% del frame.

### M1 — Core voxel client (6 sett.)
Chunk store 32³ paginato, greedy meshing su worker, formato vertice compatto, texture array
(shim GL), luce sole+blocco flood-fill, AO al vertice, frustum + cave culling, collisione AABB,
raycast per selezione, piazzamento/rottura.
**Test:** mondo statico da 512³ a 60 FPS, memoria stabile su 10 minuti di volo.

### M2 — Scripting e registry (5 sett.)
Interfaccia `script_*` + **backend Luau** (una VM per mod, budget di interruzione, quota memoria,
capability dal manifest), generatore di binding da tabella descrittiva (glue C++ + `.d.luau` +
doc), registry data-driven per blocchi/item/entità con hot reload, event bus tipizzato, hook di
tick. Il backend LuaJIT era previsto per M11: quella milestone è annullata (§8) e il runtime
resta uno.
**Test:** un mod `--!strict` che aggiunge 20 blocchi e un item, ricaricato a caldo senza riavvio;
un mod che va in loop infinito viene interrotto senza fermare il server.

### M3 — Server e rete (5 sett.)
Core headless, ENet + canali, handshake versionato con negoziazione feature, streaming chunk
compresso con interest management, replica entità, autorità del movimento, SQLite, mapgen
deterministico con stage registrabili.
Dimensionato per **2-8 giocatori per mondo** (deciso): niente sharding, niente region file,
anti-cheat leggero (speed cap + distanza di interazione + inventari server-side).
**Test:** 4 client su LAN, 30 minuti, nessuna desync; server headless in CI che gira 1000 tick e
verifica un hash del mondo.

### M4 — Portali completi (5 sett.)
Porting della logica di `yaportal` (geometria, trasformata, trigger, macchina a stati per
giocatore, passthrough del pavimento, roll, teletrasporto atomico), RT pool con ricorsione,
ordinamento trasparenti per vcam, cam hint, cielo/nebbia attraverso il portale.
**Test:** la suite di casi di `PORTAL_SYSTEM.md` — orizzontale↔verticale, mezzo-offset, giocatore
dentro la cornice, portale dietro il vetro, teletrasporto intra-frame (camera che salta ≥ 4 nodi
tra il pass RTT e il pass dei quad), e **due portali che si guardano a profondità 2** — che oggi
il fork non sa fare.

> **Chiuso.** Ogni clausola ha un gate in `ctest`: `portal_cases` per mezzo-offset, camera dentro
> la cornice, vetro e salto intra-frame; `portal_orientation` per orizzontale↔verticale;
> `portal_recursion` per la profondità 2 — la riga che il fork non sapeva fare. Tabella clausola per
> clausola nel `README.md`, misure in `docs/archive/M4_FINDINGS.md`.

### M5 — Multi-sessione locale (3 sett.)
Due `Session` nello stesso processo (due mondi locali), una attiva e una passiva renderizzata nel
portale, swap in place con tutte le invarianti di §4.3.
**Test:** entra e esce dieci volte di fila; cielo, tasti premuti, avatar proprio e view range
corretti a ogni giro (i quattro bug storici).

> **Chiuso.** `dual_session` fa i dieci giri e controlla i quattro bug a ogni lap. Misure in
> `docs/archive/M5_FINDINGS.md`.

### M6 — Cross-server (4 sett.)
Chiavi d'identità, ticket firmati, scope OBSERVE/ENTER, heartbeat di stato, fallback a redirect,
media content-addressed con cache condivisa, id di contenuto per-sessione.
**Test:** due server su macchine diverse, con *game* diversi; attraversamento senza schermata di
carico; una passiva verso un server morto non spamma il log (regola del backoff).

> **Chiuso, con un limite dichiarato.** `bad_link` fa lo scenario intero a 400 ms e 20% di perdita
> fra due server con *game* diversi, e insiste che l'attraversamento sia successo davvero;
> `dead_world` misura la regola del backoff (2 righe in 26 s contro 28108 senza); `two_clocks` fa
> divergere le due wall clock. «Macchine diverse» è coperto per MTU, frammentazione, NAT, un server
> con due nomi e due orologi — `tools/two_machines.sh` in container per i primi, un `LD_PRELOAD` per
> l'ultimo. **Non** coperti: un kernel separato e la contesa reale di CPU, che restano a una VM.
> Misure in `docs/archive/M6_FINDINGS.md`.

### M7 — Gioco minimo (8 sett.)
Inventario, craft, UI (microui/Clay guidata da Lua), HUD, salute, drop, contenitori, mapgen con
biomi, ciclo giorno/notte, audio con attenuazione, mob base con ECS.
**Test:** una partita di 30 minuti dall'inizio, senza crash e senza toccare la console.

> **Chiuso, con quattro limiti dichiarati.** `tests/long_game.sh` fa i trenta minuti — 7.9 milioni
> di frame, **zero correzioni**, RSS piatta a +1.1 MB, 224 blocchi rotti e 224 rimessi, otto
> creature uccise, un giorno e mezzo di cielo. La prima run è stata rossa e ha trovato uno spawn
> che esisteva solo con `--portals`, quindi un giocatore uscito dal mondo cadeva per sempre.
> **Non** coperti: è uno *script* che cammina e non una persona (nessuno tocca la console, e nessuno
> gioca davvero); le creature finiscono dopo un minuto e mezzo perché in M7 non c'è spawn di mob;
> il mondo non ha aperture, quindi l'attraversamento resta `bad_link`; e serve una finestra, quindi
> su una macchina headless questo test non gira. Misure in `docs/archive/M7_FINDINGS.md` §33.

### M8 — ECS e API meccaniche (4 sett.)
Componenti/sistemi da Lua con iterazione batch, fasi ordinate, trait sui nodi, render pass
registrabili, canali di rete per mod.
**Test:** riscrivere i blocchi classici dei puzzle a portali (pulsante, dispenser, pedestal, clock) **solo** come mod,
senza toccare l'engine — è la prova che l'estensibilità è vera.

> **Chiuso per il suo test d'uscita, con il resto della milestone dichiarato
> aperto.** `tests/portal1.sh` fa i quattro blocchi classici dei puzzle a portali come mod e nient'altro:
> il pulsante si abbassa sotto chi ci sta sopra e risale, il segnale attraversa
> da un mod a un altro e il vent lascia cadere un cubo, il pedestal prende quello
> che gli si offre e lo restituisce a chi si avvicina, il clock conta e dopo aver
> chiuso e riaperto il mondo riparte dal numero a cui si era fermato. Quattordici
> controlli, headless, e l'ultimo è un `grep`: **nessun file sotto `src/` nomina
> un blocco, un item o un evento dei quattro**.
>
> **E poi chiusa per intero.** Quello che il test d'uscita non toccava — i
> quattro blocchi volevano stato per cella, non entità proprie — è stato scritto
> dopo, con la stessa forma: il gate prima del codice, `DISABLED` finché non è
> verde. `tests/sentry.sh` (headless, 16 controlli) fa **componenti e sistemi
> da Lua con iterazione batch** (512 entità, una chiamata), **fasi ordinate** da
> un numero e non dall'ordine di caricamento, un **canale di rete per mod** (il
> campo esisteva nell'handshake da M3 e nessuno lo usava; protocollo 20 → 21) e
> `find_nodes`, l'equivalente di un LBM dell'engine di partenza. `cnp_client --pass-check` (6
> controlli, gpu) fa i **render pass registrabili**: un pass gira dentro il
> render e una volta per camera, quindi quello che un mod disegna si vede anche
> attraverso un portale.
>
> **Resta aperto e dichiarato**, in `TODO.md`: un sistema nomina un componente
> solo (non c'è join fra due), le entità di un mod non sono visibili agli altri
> giocatori, e vivono nel mondo della tabella delle sessioni — quindi un run
> offline non ne ha. Misure in `docs/archive/M8_FINDINGS.md`.

### M9 — Performance e robustezza (4 sett.)
Meshing multi-thread a budget, occlusion culling, LOD lontano, reverse-Z, profiler in-game,
crash handler, migrazione dei salvataggi.
**Test:** tre gate, scritti prima del codice come quelli di M8. `frame_budget`
(gpu) misura cosa costa un frame mentre il mondo si muove, su un mondo da 512
nodi — 512 chunk, non i 32 degli altri gate — in tre fasi: fermo, un blocco per
frame, e tutto sporco insieme. I numeri sono quelli del giocatore e non quelli
che l'engine produce: 16.7 ms è un sessantesimo di secondo, 50 ms è uno scatto
che si vede, e una camera sepolta nella roccia deve disegnare quello che ha
intorno e non il mondo dietro la roccia. `crash_evidence` chiede che un processo
che muore dica di quale segnale, lasci uno stack, e **muoia lo stesso** — un
handler che inghiotte SIGSEGV è peggio di nessun handler. `migration` chiede che
un file del mondo scritto da una build precedente si apra e tenga quello che
c'era dentro, e che uno da una build successiva sia rifiutato per nome.

> Questa riga non c'era: M9 era l'unica milestone del piano con una lista di
> feature e nessun numero. È stata scritta prima di cominciarla, perché M7 ha
> chiuso con il suo test d'uscita scritto per ultimo e la prima run ha trovato
> ventidue minuti di giocatore che cadeva fuori dal mondo.
>
> **Chiusa dai suoi tre gate.** `migration` (14 controlli): la versione si legge
> **prima** che lo schema tocchi il file, perché un'apertura rifiutata non deve
> cambiare il file che ha rifiutato. `crash_evidence` (5): il segnale è nominato,
> lo stack è scritto, e il processo **muore lo stesso** — su `sigaltstack`,
> perché il modo più comune di prendere un SIGSEGV è finire lo stack.
> `frame_budget` (21): un mondo da 512 chunk che si ricostruisce interamente
> costa **4.23 ms** nel frame peggiore contro i 16.7 del budget, e recupera al
> frame 130 invece che mai.
>
> **Il profiler ha deciso il resto della milestone.** La sua proprietà è
> aritmetica — tutto ciò che ha un nome più tutto ciò che non ce l'ha **è** il
> frame — e il primo numero che ha prodotto dice dove va un chunk: `snapshot`
> 0.32 ms, `build` 3.12, `upload` 0.00. Il 90% è la parte che legge solo uno
> snapshot e il registry, cioè la sola che può lasciare il thread. Senza quella
> riga la scelta fra thread e LOD sarebbe stata un'opinione.
>
> **Occlusion culling è una camminata, non un test**: quindici bit per chunk che
> dicono quali facce si raggiungono fra loro, e il renderer entra in un vicino
> solo se poteva attraversare quello da cui viene. Camera sigillata nella roccia:
> **4 chunk invece di 129**. Davanti a un muro: **2 invece di 154, e non un pixel
> diverso** — che è l'unico controllo capace di prendere il fallimento che conta,
> un buco nel mondo.
>
> **Resta aperto e dichiarato**, in `docs/archive/M9_FINDINGS.md`: **LOD lontano e
> reverse-Z non sono stati scritti**, perché erano mezzi e il budget non li ha
> chiesti — il momento di farli è quando un numero li chiede, non prima. E il
> walk è a grana di chunk: in un mondo alto due chunk, dalla superficie, scarta
> zero. **57 test in `ctest`, tutti verdi, e per la prima volta niente
> `DISABLED`.**

### M10 — Distribuzione (2 sett.)
Build statiche Linux/Windows, AppImage/Flatpak (riutilizzando gli script esistenti), formato
pacchetto mod + versionamento, documentazione dell'API.

### M11 — Layer di compatibilità API dell'engine di partenza · **ANNULLATA il 2026-09-12**

Era il layer `compat_layer/` con il secondo backend LuaJIT: dieci settimane per far girare i mod
dell'engine di partenza e i suoi giochi sopra questo engine. **Non si fa.** L'engine di partenza è stato l'ispirazione e il fork
precedente è da dove viene l'esperienza: non è l'obiettivo, e questo engine non deve far
girare i mod di qualcun altro.

**Cosa resta di quella milestone, ed è già fatto.** M11 chiedeva all'engine tre cose che valgono
per conto loro e che infatti sono state costruite prima che la scelta cadesse:

- `param2` facedir/wallmounted come **trait** — TODO.md CLOSED-27, un byte sulla cella;
- **scheduler ABM/LBM** — CLOSED/OPEN-42, `core.register_abm` e `core.register_lbm`;
- **drawtype nodebox + mesh** — CLOSED-43, le scatole che fermano un corpo e un blocco disegnato
  come modello.

Nessuna delle tre è un pezzo di compatibilità: sono un blocco che ha un fronte, un mondo in cui
succede qualcosa senza che nessuno tocchi una cella, e un blocco che non è un cubo. Restano.

**Cosa cade con lei:** il parser formspec, i modificatori texture `[combine`/`[colorize`,
VoxelManip, gli alias di mapgen, la traduzione di `register_node`/`register_entity`, e il secondo
backend LuaJIT (§5.3), la cui prima ragione era far girare Lua 5.1 legacy.

**E cosa costa, detto e non nascosto:** §7.5 diceva che riscrivere i contenuti da zero vale
anni-uomo ed è il modo in cui questo progetto muore. Quella frase resta vera. La differenza è che
adesso i contenuti sono una scelta di *scope* e non un debito verso il catalogo di qualcun altro —
si veda M12.

### M12 — Contenuti propri (continuo)

I contenuti si scrivono qui, come mod di questo engine e con la sua API. Quello che c'è già in
`mods/` è la prova che la strada esiste: blocchi con forma e durezza, attrezzi che contano, casse,
craft, ABM, un mapgen con stage, entità con componenti — tutto senza toccare l'engine.

**Lo scope è deciso, 2026-09-12.** Il gioco è un clone del classico gioco a blocchi da cui i portali portano ad
altri mondi, e quei mondi possono avere mod diversi — fino a mondi che sono un altro gioco (M13).
La frase di §7.5 resta vera e va tenuta a vista: la *parità* di contenuti con il classico gioco a blocchi vale
anni-uomo, e caduti i contenuti di qualcun altro non c'è scorciatoia. Quello che questo scope ha
che "clone completo" non aveva è la forma: non tutto deve stare nel sandbox, perché il resto può
stare dietro un portale, in un altro mondo, con i suoi mod. Il consiglio di questo documento è
misurare M12 per partita e non per lista — un sandbox che regge una partita, come M7 ha misurato
in trenta minuti — e non inseguire l'elenco delle feature del classico gioco a blocchi.

### M13 — Mondi con un modo di gioco proprio (stima aperta)

Deciso con lo scope (§0). **Un modo di gioco è un mondo**: un server con i suoi mod, raggiunto da
un portale del sandbox, non una modalità dentro il sandbox. È la lettura che usa quello che questo
engine ha già — sessioni per server, media content-addressed, forme e modelli che viaggiano nel
welcome — e il fork ha già fatto metà di questa strada: la sua `yafps/` è un'arena FPS
minima «built as a cross-world portal destination», **552 righe in cinque mod** (armi 178,
bersagli 114, core 114, arena 83, HUD 63). È il criterio naturale, come `yaportal` lo era in §6.7.

**Test:** dal sandbox, un portale verso un server che fa girare `yafps` **riscritto come mod di
questo engine**, e uno verso un server con **una pista a tempo** — partenza, checkpoint, arrivo,
tempo a schermo. Il client che attraversa **non ha installato nessuno dei due mod**. Si gioca una
partita e un giro, e si torna dallo stesso portale. Più un `grep` come quello di M8: nessun file
sotto `src/` nomina un'arma, un veicolo o un checkpoint.

**Cosa manca oggi, letto dal codice e non stimato:**

1. **Da un mondo al client arrivano l'aspetto e la forma, non il comportamento.** Il client carica
   i mod dalla propria directory (`src/client/main.cpp`, `--mods` e `load_dir`). Sul filo viaggiano
   i nomi dei blocchi, le forme e il verso (protocollo 27), i modelli e i media. Non viaggiano se un
   blocco è solido, la durezza, il livello di scavo (TODO.md OPEN-43), né una riga di Luau. Un modo
   di gioco vive in buona parte sul client — il tempo o le munizioni a schermo, il riscontro di uno
   sparo, la previsione di un veicolo, un `core.render.pass` — e §5.6 dice che **nessun mod
   scaricato dal server viene eseguito sul client**. È la decisione aperta di questa milestone,
   sotto.
2. **Il movimento è un numero dell'engine.** `core/physics/movement.h`: camminata 4.8, sprint 8,
   volo 14, salto 8, gravità −25. Il validatore del server accetta in orizzontale al massimo
   `kFlySpeed × tolerance` = 14 × 1.6 = **22.4 m/s**, fuori dal secondo di grazia di un portale.
   Nessun mod può cambiare questi numeri, né per mondo né per corpo, quindi un'auto più veloce di
   un giocatore in volo è un cheat per questo server. `yafps` usava `set_physics_override` e
   `set_velocity`. Servono limiti dichiarati dal mondo e dal corpo, spediti nel welcome come le
   forme e letti dal validatore, e un corpo che non sia un giocatore a piedi. Il server verifica
   il movimento e non lo simula (`movement.h` lo dice), e per una corsa va misurato se basta
   prima di decidere che non basta.
   **Primo pezzo fatto, 2026-09-13** (TODO.md OPEN-44): `core.movement` dichiara camminata,
   sprint, volo, salto e gravità **per mondo**, i numeri viaggiano nel welcome (protocollo 28) e
   il validatore ricava i limiti da lì — a 60 m/s il gate rifiuta 0 claim su 98 nel mondo che li
   ha dichiarati e 14 su 99 in uno senza mod. Restano i limiti **per corpo** e un corpo che non
   è un giocatore a piedi.
3. **Il bersaglio lo sceglie il client.** TODO.md OPEN-15: il client lancia il raggio, il server
   controlla solo che il corpo nominato sia entro `place_reach` = **7 nodi**, e un colpo ogni
   `kHitCooldown` = **0.4 s**. Il fucile di `yafps` spara a **60 nodi ogni 180 ms**, con un
   raycast lato server, e il suo commento dice perché non serve compensare la latenza: *tutti i
   mondi girano su questa macchina*. Fra macchine diverse — che è quello che M6 ha misurato —
   quella frase non vale più.
4. **Le entità di un mod non le vede nessuno** (TODO.md OPEN-20), e i bersagli e i proiettili di
   un FPS sono esattamente entità di un mod.
5. ~~**Il mondo è una scatola** di 128 per 64, generata tutta (OPEN-35) e mai scaricata (OPEN-38).~~
   **Fatto il 2026-09-15:** il mondo del server non ha bordo, le colonne si generano o si rileggono
   quando qualcuno si avvicina e si lasciano quando nessuno c'è (protocollo 37).
   Un'arena ci sta — `yafps` chiedeva il mapgen `flat` —; una pista forse no, e va detto quando se
   ne disegna una.
6. **Dei difetti dell'attraversamento ne resta aperto uno**, OPEN-2 (incastrati nel blocco
   d'uscita), mai riprodotto. All'ingresso di un'arena o di una corsa è la prima cosa che si sente.

**Decisione presa, 2026-09-13: il codice lato client di un mondo.** La prima delle due strade
qui sotto, con una condizione che la raccomandazione non diceva: **il client può rifiutare il
codice di un mondo**, e quel mondo si gioca lo stesso, senza (§5.6). Le due strade, come erano
state messe:

- **Luau spedito dal server**, in una VM per mod e per sessione, con capability solo da client —
  disegnare, HUD, input, il canale del proprio mod — e niente file né rete. Chiede di riscrivere
  la riga di §5.6. È ciò per cui Luau è stato scelto (§5.3: una sandbox *progettata* per codice non
  fidato), ed è l'unica strada in cui un mondo nuovo non chiede un engine nuovo.
- **Primitive dell'engine guidate da dati** — HUD dichiarativo, parametri di movimento, veicolo,
  arma hitscan. Niente codice estraneo sul client, ma ogni modo che non ci sta dentro chiede una
  release dell'engine a tutti i giocatori: il contrario di "mondi con mod diversi".

La raccomandazione è la prima, con l'autorità che resta dove è: il client prevede e disegna, il
server decide — i limiti di movimento e il raggio di un colpo sono numeri che il server verifica
comunque, qualunque codice giri dall'altra parte.

**Totali:**
- Portali cross-server funzionanti (M-1..M6): **32 settimane (~7 mesi)**
- Engine + gioco minimo autonomo (…M10): **49 settimane (~11 mesi)**
- Contenuti propri (M12): **continuo**. La parità con il classico gioco a blocchi resta 2-3 anni, ed è la ragione
  per cui il criterio è una partita e non una lista.
- Mondi con un modo proprio (M13): **nessuna stima ancora**. La decisione sul codice lato client
  è presa (2026-09-13); il pezzo più grosso che ne viene è la sandbox da client, e non è stato
  misurato niente che dica quanto costa.

### Assetto durante il lavoro

- Il fork dell'engine di partenza è **congelato** dopo M-1: nessun lavoro nuovo, solo il tag e la documentazione
  dei bug aperti. (Consapevole del rischio: durante ~14 mesi non c'è prodotto vivo.)
- Il nuovo engine vive in **`/home/giacomo/dev/Cubes and portals/`**, repo git proprio, non dentro
  il fork precedente. Struttura prevista:

```
Cubes and portals/
  docs/DESIGN.md          ← questo documento
  src/core/               ← voxel, ECS, mapgen, protocollo (mai #include <raylib.h>)
  src/client/gfx/         ← gfx_* + backend raylib (poi eventualmente sokol)
  src/client/             ← sessioni, render graph, portali, UI
  src/server/             ← headless
  src/script/             ← script_* + backend Luau (poi luajit)
  mods/                   ← mod di base in .luau
  third_party/            ← raylib, luau, enet, sqlite, zstd, fastnoiselite, microui
```
- `docs/PORTAL_SYSTEM.md` e le invarianti di `AGENTS.MD` §461 diventano la **specifica di test**
  del nuovo engine: ogni invariante = un test automatico. È il modo di non ricomprare gli stessi
  bug (cielo bianco al secondo giro, doppia connessione passiva, tasti persi allo swap, avatar
  invisibile a sé stesso, identità per indirizzo invece che per mondo).

---

## 9. Rischi e decisioni aperte

| Rischio | Prob. | Impatto | Mitigazione |
|---|---|---|---|
| Il progetto si arena sui contenuti | **Alta** | Fatale | La mitigazione era M11 (far girare i giochi esistenti), **annullata il 2026-09-12**. Lo scope fissato lo stesso giorno non è più piccolo: clone del classico gioco a blocchi più mondi con modi di gioco propri. La mitigazione che resta è la forma — ogni modo è un mondo piccolo dietro un portale, e M12 si misura per partita e non per lista di feature. È più debole di quella che c'era, e va detto |
| Un modo veloce contro un validatore pensato per chi cammina | **Certa** | Alto | Oggi non è un rischio ma un fatto: 22.4 m/s in orizzontale, un colpo entro 7 nodi ogni 0.4 s. M13 punti 2-3: limiti dichiarati dal mondo e verificati dal server, raggio lanciato dal server |
| Codice client spedito dal server | Media | Alto | **Scelto il 2026-09-13.** Luau, una VM per mod e per sessione, capability solo da client, niente file né rete né processi, quota e interruzione come sul server, autorità al server; e il giocatore può rifiutarlo, per tutti i mondi o per uno (§5.6) |
| Fork congelato = nessun prodotto vivo per ~14 mesi | **Alta** | Alto | M-1 obbligatoria: release taggata, bug documentati, tutto pushato. Il fork resta *scongelabile* in un pomeriggio |
| Stanchezza / bus factor 1 | Alta | Fatale | Milestone che producono qualcosa di giocabile ogni volta; niente big-bang |
| raylib insufficiente in M9 | Media | Medio | Interfaccia `gfx_*` sottile → migrazione a sokol in giorni |
| Sandbox FFI bucata | Media | Alto | Capability nel manifest, FFI off di default, nessun mod client dal server |
| Cross-server abusato (scanner di mondi) | Media | Medio | Scope OBSERVE, ticket a scadenza, rate limit |
| Rewrite più lento del previsto (sempre) | **Alta** | Alto | Kill switch a M0; revisione dello scope a M4, M6 e alla fine di M10 |
| ~~M11 reintroduce vincoli di design dell'engine di partenza~~ | — | — | Non si applica più: il layer non si fa, e i vincoli che avrebbe ereditato (semantica `param2`, formspec, mapblock 16³) restano fuori |

**Decisioni prese** (§0): fork congelato dopo M-1 · raylib dietro `gfx_*` · 2-8 giocatori per
mondo · **niente compatibilità dell'engine di partenza** (2026-09-12) · **M10 sospesa** finché il gioco non è
dichiarato finito e giocabile · **scope: clone del classico gioco a blocchi con portali verso mondi con mod e modi
di gioco propri** (2026-09-12).

· **un mondo può spedire codice Luau al client, e il client può rifiutarlo** (2026-09-13, §5.6).

**Non resta aperta nessuna decisione di progetto grossa.** Resta il lavoro: quello che M13 elenca,
e il rischio n.1 qui sopra, che lo scope mitiga in modo più debole di come faceva M11.

---

## 10. Stato e prossimi passi

**Stato, 2026-09-12.** M-1 fatta (fork congelato e taggato); M0..M3 superate e M4..M9 chiuse,
ciascuna con i propri limiti dichiarati; M10 sospesa; M11 annullata; M12 continua; M13 aperta e
senza stima. I difetti aperti sono in `TODO.md`, le misure e lo storico in `docs/archive/`
(dal 2026-09-13, quando i test sono stati ridotti a 13 e la documentazione riscritta sullo stato
attuale: [`ARCHITECTURE.md`](ARCHITECTURE.md)).

**Prossimi passi, in ordine:**

1. ~~Decidere il codice lato client di un mondo~~ — **deciso il 2026-09-13**: Luau dal server,
   rifiutabile dal client (§5.6).
2. **Limiti di movimento dichiarati dal mondo** e letti dal validatore (M13 punto 2). **Per mondo
   fatto il 2026-09-13** (TODO.md OPEN-44). Da lì restano, dal più economico:
   - ~~lo streaming dei chunk a velocità di corsa, misurato prima di cambiarlo~~ — **misurato il
     2026-09-13** (TODO.md OPEN-45): regge fino a 120 m/s, dove il limite è il raggio (128 nodi,
     un secondo di strada); a 256 m/s no, 64 nodi di terreno davanti al minimo. Non cambiato:
     aspetta una pista che dica quanto lontano serve vedere;
   - ~~il percorso di un claim controllato dal server~~ — **fatto il 2026-09-13** (TODO.md
     OPEN-46): il server ripercorre il claim con il risolutore del client, in cinque modi, e lo
     accetta se uno arriva entro mezzo nodo; il primo claim dopo un piazzamento del server si
     giudica da dove finisce. Resta rifiutato un percorso che torna indietro su un
     asse fra due claim, cosa che a piedi non succede e a 160 m/s sì;
   - ~~velocità per corpo~~ — **fatto il 2026-09-13** (TODO.md OPEN-47): `core.body_movement`
     dà a un corpo numeri suoi in qualunque momento, con un messaggio a quel corpo e un secondo in
     cui il server accetta il più veloce fra i vecchi e i nuovi. È il primo pezzo di un veicolo e
     non un veicolo: niente sterzo, niente inerzia.
3. ~~Il colpo verificato dal server e le entità di un mod visibili~~ — **fatti il 2026-09-13**
   (TODO.md OPEN-15, OPEN-20): il server lancia il raggio verso il corpo che un colpo nomina e lo
   rifiuta se c'è un blocco solido in mezzo; `core.ecs.visible` fa mandare a tutti un'entità di un
   mod, come *cosa* e non come persona. Per strada: gli id dei corpi sul filo erano a 32 bit e
   perdevano la generazione (OPEN-48), ora sono interi.
4. ~~Il codice Luau che un mondo spedisce al client~~ — **fatto il 2026-09-13** (TODO.md
   OPEN-49), con il rifiuto del giocatore. Quello che ne manca è in quella voce: HUD e input da
   codice di un mondo, e un gate che lo guardi girare in una finestra.
5. ~~Un sandbox con un portale verso l'arena e uno verso la pista~~ — **fatto il 2026-09-14**
   (protocollo 34): ogni apertura ha la sua destinazione. Con il `grep` pulito lo stesso giorno,
   **il test di M13 passa**, a mano su tre server in locale (TODO.md, "one sandbox, two worlds").

**Stato, 2026-09-14.** M13 ha il suo test passato; quello che resta sono i buchi che il test
non guarda. Dal più economico:

6. ~~Il kart guidato a mano~~ — provato a mano (2026-09-14).
7. **La pista come gioco.** ~~Niente volo sopra i muri~~ e ~~un corpo largo quanto il kart~~ —
   **fatti il 2026-09-14** (protocollo 35, `core.body_width`; TODO.md, "the timed track"), e
   ~~conto alla rovescia, tempi tenuti per nome e confrontati~~ nel mod della pista, con
   `core.player_name`. ~~Un pilota scriptato che non porta il kart fuori dalla piazzola~~ —
   piazzola larga 13, due giri e un rientro provati in gioco (2026-09-14). ~~I tempi persi a un
   riavvio del server~~ — **tenuti dal 2026-09-19** con `core.storage_set`, nel world file.
   **Dal 2026-09-20 si arriva a piedi**: il mondo cammina, i kart stanno parcheggiati nella
   piazzola come blocchi che indossano il modello del kart, e un tasto destro su uno ci fa
   salire (`core.body_drive`, `core.body_model`, `core.model_id`; F fa scendere e il kart
   resta dov'è -- dal 2026-10-16, prima era un altro tasto destro, che faceva scendere per sbaglio).
8. **Inerzia e sterzo verificati dal server** (TODO.md OPEN-47). **Deciso il 2026-09-19:** il
   kart è un corpo rigido su ruote con Jolt Physics (5.6.0, build deterministica), il giocatore
   a piedi resta sul risolutore AABB proprio, con peso a terra e l'aria dei puzzle a portali; il movimento
   è in `core/physics` (`player_update`, `vehicle_update`). Il passo dopo: il client manda i tasti
   e il dt, il server li rigioca con lo stesso codice e corregge dove non torna, al posto dei
   controlli di velocità, percorso e altezza. L'aderenza la decidono le gomme di Jolt, non un
   numero di `core.drive`. **Finché quel passo non c'è**, tre regole tengono il kart dentro a
   quello che il server sa controllare (TODO.md, 2026-09-20): il telaio resta entro 26 gradi
   dalla verticale, le ruote non prendono come terreno niente oltre i 50 gradi — le due cose
   che lo facevano coricare su un muro e scalarlo — e "dentro un blocco" per un corpo che guida
   è il solo centro, perché il box del server non è la forma che il client simula e il rifiuto
   rimetteva il telaio dentro il muro.
9. Poi la decisione se il gioco è giocabile, che riapre M10.

---

## Verifica dei fatti citati

Fatti verificati durante l'analisi, non ricordati:

- raylib 5.5 installato localmente (`~/dev/raylib/prova FPS/venv`, `raylib-5.5.0.4`): confermata
  la presenza di `rlSetMatrixProjection`, `rlGetMatrixProjection`, `rlLoadFramebuffer`,
  `rlFramebufferAttach`, `rlLoadTextureDepth`, `rlLoadVertexArray`, `rlUpdateVertexBuffer`,
  `rlDrawVertexArrayElements`, `rlSetUniform(Matrix)`, `rlTextureParameters`,
  `DrawMeshInstanced`, filtri anisotropici. Confermata **l'assenza** di API per texture array.

  > **Aggiornato in M0:** il progetto è partito da **raylib 6.0** (submodule `third_party/raylib`,
  > tag `6.0`), non da 5.5. Riverificato sull'header di 6.0: tutte le funzioni sopra esistono
  > ancora, le texture array restano assenti come previsto, e la convenzione di proiezione non è
  > cambiata (RH, depth `[-1,1]`, `m11 = -1`). Assenti invece `rlGetRenderBatch` e
  > `rlLoadShaderCode`, che il §2.3 citava per errore. Build verificata: `PLATFORM_DESKTOP`,
  > `GRAPHICS_API_OPENGL_33`, GLFW su X11, raylib statica.
  > Scripting: **Luau 0.733** (`third_party/luau`).
- Fork engine: 77 file, +6 369/−309 rispetto a upstream v5.16.1 (`5ebd9b579`); misurato anche
  90 file / +6 587 contro la merge-base `750ae1956` di `upstream/master`.
  Base dell'engine di partenza `src/`: 214 491 righe (client 40 962, server 16 574, network 7 100, gui 18 692,
  mapgen 11 669).
- Renderer portali del fork: `portal.cpp` 1050 righe (100% nuove) + `portal.h` 84 +
  `portal_manager.{h,cpp}` 525 ≈ **1 660 righe**; hook in `clientmap.cpp` +182/−17;
  `sky.cpp` +187/−78; shader +12. `game.cpp` +1247/−105, di cui ~880 sono le dieci funzioni
  dual-client e `performSessionSwap` (282 righe da sola).
- Ricorsione dei portali: **assente** oggi — il quad hook è saltato quando
  `m_bypass_frustum_culling` è attivo (`clientmap.cpp:1281-1283`).
- Ogni RTT è alla dimensione piena di rendering (`ctx.target_size`, `ECF_A8R8G8B8`) e fa un
  `drawAll()` completo con culling disabilitato (`portal.cpp:660-919`); re-render intra-frame se
  la camera si sposta ≥ 4 nodi (`portal.cpp:1033-1047`).
- Mod: `yaportal/init.lua` 6911 righe / 289 KB, `yaportal_link/init.lua` 2221 / 94 KB,
  `yafps/` 552 righe + 160 KB di media; totale Lua ≈ 9 684 righe.
- Il mod **non usa mod channel**: tutta la comunicazione extra passa da HTTP + ambiente insicuro.
- Nel sistema esiste già un esperimento raylib in Python (`~/dev/raylib/prova FPS`, `pyray` 5.5,
  368 righe) — utile come riferimento di ergonomia, non come base.

**Non verificato sulla macchina (da confermare nello spike M0):** le proprietà di Luau
(native codegen, interrupt/quota memoria, assenza di `setfenv`/`loadstring`), di QuickJS-ng e di
WAMR provengono da conoscenza pregressa, non da una build locale. M0 deve includere: build di
Luau con CMake accanto a raylib, e una misura del costo di una chiamata scriptata per frame.
- API Lua aggiunte dal fork: `set_portals`, `set_portal_cam_hint`, `clear_portal_cam_hint`,
  `set_portal_xworld_dest`, `set_xworld_target`, `register/update_portal_sky_type`,
  `set_portal_sky`, `redirect_player`, `xworld_swap_player`, `xworld_exchange(_posts)`.
  Opcode nuovi: `TOCLIENT_PORTAL_TELEPORT 0x65`, `TOCLIENT_REDIRECT 0x66`,
  `TOCLIENT_XWORLD_SWAP 0x67`, `TOCLIENT_PORTAL_STATE 0x68`, `TOSERVER_XWORLD_STATUS 0x54`.
- Misura di performance dual-client (dal lavoro M7 sul fork): il render del mondo B costa il
  **3-7%** del frame; il collo di bottiglia è il mondo A.
