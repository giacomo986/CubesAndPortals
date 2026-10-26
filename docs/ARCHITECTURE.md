# Architettura

Come è fatto il codice oggi, e dove va una feature nuova. Il perché delle scelte grandi è in
[`DESIGN.md`](DESIGN.md); il perché delle piccole è quasi sempre nel commento in cima al file.

## Programmi e librerie

```
cnp_client ─┬─ cnp_script ─┬─ cnp_core
            ├─ cnp_net ────┘
            └─ raylib (solo sotto src/client/gfx e src/client/snd)
cnp_server ─── cnp_script, cnp_net, cnp_core      (nessuna GPU, nessuna finestra)
```

| Target | Cosa contiene |
|---|---|
| `cnp_core` | Tutto quello che non ha né socket né GPU: voxel, luce, mesher, mapgen, fisica, portali, registri, oggetti, ECS, media, protocollo e state machine delle sessioni |
| `cnp_net` | Quello che tocca il mondo esterno: transport ENet, cifratura, codec dei chunk, `ClientSession`, SQLite (`storage/world_db`) |
| `cnp_script` | Runtime Luau, `ModHost` (una VM per mod), event bus, codice che un mondo manda al client |
| `cnp_modpack_lib` | Il formato `.cnpmod` |
| `cnp_client` | Finestra, render, input, audio, HUD; gioco locale o connesso |
| `cnp_server` | Server headless |
| `tools/` | `cnp_api_gen` (binding), `cnp_modpack`, `cnp_bake_textures`, `cnp_netshape`, `cnp_clock_skew` |

## Regole che il codice rispetta

- **Niente raylib sopra `gfx.h`, `platform.h` e `snd.h`.** È quello che tiene aperta la
  migrazione a sokol. `src/core` non include mai niente di grafico.
- **Niente stato globale per mondo.** Tutto quello che appartiene a un mondo sta in una
  `Session` del client (`src/client/main.cpp`), e la `Scene` ne tiene più d'una: è così che un
  portale mostra un altro server.
- **Il server è l'autorità, ma non simula la fisica.** Il client muove il corpo e manda un
  claim; il server lo **verifica** (`check_move` in `core/physics/movement.h`, chiamata da
  `net/session.cpp`) e corregge. Scavi, piazzamenti, colpi, casse e craft li decide il server.
- **Una regola si scrive una volta.** Quando client e server devono essere d'accordo (quanto
  dura uno scavo, se una scatola è dentro un blocco, dove si nasce) la funzione sta in `core/` e
  la chiamano tutti e due.
- **Gli id si assegnano in ordine di registrazione; sul filo e su disco viaggiano i nomi.** Il
  client adotta la tabella dei blocchi del server; il file del mondo rifiuta una tabella diversa.
- **I media sono indirizzati per contenuto** (`core/media/content_hash.h`): il nome di un file è
  l'hash dei suoi byte, e la cache del client è una per tutti i mondi.
- **Il motore emette eventi e non ha regole di gioco private.** Proteggere una zona, sostituire un
  blocco, generare minerali: sono mod.

## Mappa di `src/`

### `src/core`

| Cartella | File principali |
|---|---|
| `voxel/` | `chunk` (32³, id + luce + verso), `mesher` (greedy, vertice 12 byte con luce morbida e direzionale), `mesh_pool` (meshing su thread), `light` (flood fill sole e blocchi), `raycast`, `dig` (lato client di uno scavo), `find` |
| `worldgen/` | `worldgen` (stage in ordine, una colonna di chunk alla volta; `world_hash`), `biome`, `noise`, `spawn` |
| `world/` | `columns` (quali colonne sono in memoria: blocchi, poi luce, poi lasciate), `daylight` (orologio del mondo; `sky_light_at`: sole e luna per il disegno), `modifiers` (ABM/LBM), `node_meta` (cosa ricorda una cella) |
| `registry/` | `node_registry` (blocchi), `node_shape` (scatole di un blocco, ruotate), `core_nodes` (i blocchi del motore), `model_registry` |
| `physics/` | `aabb` (box del giocatore contro il mondo), `movement` (limiti e validazione), `fall` |
| `portal/` | `portal_math` (near plane obliquo, clip plane), `portal_traverse` (dove esce un corpo, orientato come), `portal_clip` (taglio dei solidi), `portal_tuning` (i sei numeri di `config/portal.cfg`) |
| `item/` | `item_registry`, `inventory`, `container` (casse), `recipe`, `craft_grid`, `item_codec` (su disco) |
| `ecs/` | `components` (Flecs), `health`, `drop`, `mob`, `lua_ecs` (componenti e sistemi dei mod) |
| `media/` | `content_hash`, `media_set` (lato server), `media_cache` (lato client), `png`, `gltf`, `texture_pack`, `face_tiles`, `tile_bake` |
| `net/` | `protocol` (messaggi, versione 37, handshake), `session` (`ServerSessions`: tutto quello che il server fa con un peer, senza socket), `client_session`, `interest` (quali chunk e in che ordine), `chunk_codec`, `transport` (ENet), `crypto`, `bytes` |
| `storage/` | `world_db` (SQLite: chunk, casse, borse, metadati, migrazioni), `column_keeper` (le colonne che entrano, si illuminano e si lasciano; server e client da solo) |
| `ui/` | `panel` (pannelli dichiarati dai mod, layout e hit test) |
| `audio/` `diag/` `math/` | Attenuazione e toni; profiler e stack dump; vettori, matrici, frustum |

### `src/script`

| File | |
|---|---|
| `script.h`, `script_luau.cpp` | Il confine con Luau: VM, quote, interruzione, capability tolte prima che parta il mod, la tabella `script_host` di funzioni che il motore offre |
| `api.luau` → `api_generated.inc` | Le firme dell'API, **una volta sola**; il glue C++, `docs/api.d.luau` e `docs/API.md` sono generati (`cnp_bindings`) |
| `mod_host` | Carica `mod.conf`, versioni e dipendenze, una VM per mod, hot reload, `side = server/client/both` |
| `event_bus` | Eventi con schema, priorità, cancellazione |
| `world_code` | Il codice che un mondo manda al client: nome nei media, pacchetto, tabella host ristretta |

### `src/client`

| File | |
|---|---|
| `main.cpp` | `Session` (un mondo) e `Scene` (tutti i mondi), le funzioni host dei mod lato client (`make_client_host`), `run_interactive` (gioco locale), `run_network` (gioco connesso), `run_bench`, `run_dual_bench`, `run_soak`, `run_verify`, e `main()` con i flag |
| `player.cpp` | Movimento del giocatore: input, gravità, collisione, passo |
| `gfx/` `snd/` | Unico punto che include raylib |
| `render/` | `world_render` (chunk e atlas), `portal_render` (target, ricorsione, `draw_closed`: la membrana dei portali chiusi e la sua apertura), `body_render`, `model_render` e `model_store` (glTF), `selection`, `mod_pass` (pass dei mod), `shaders.h` |
| `ui/hud.cpp` | Hotbar, borsa, pannelli, barra di scavo, avvisi |

### `src/server/main.cpp`

Flag, creazione del mondo (la regione attorno allo spawn, mod, file), le funzioni host dei mod
lato server, e il tick a 20 Hz: messaggi dei peer, step dei mod e dei sistemi ECS, ABM, colonne
generate o rilette attorno a chi c'è e lasciate dove non c'è nessuno, streaming dei chunk per
interest, entità, salute, tempo, porte fra server.

**Il mondo non ha bordo.** Una colonna di chunk entra in memoria quando una sessione le arriva a
quattro colonne: dal file del mondo se c'è, dal generatore se no (core e stage dei mod, scritti
solo dentro la colonna). Ha la luce quando le otto attorno hanno i blocchi, e solo allora si
manda, si salva e la spazzano gli ABM. Esce dopo trenta secondi a più di otto colonne da tutte le
sessioni e da tutto quello che ci sta sopra (stack, mob, cose dei mod): se ha la luce la si tiene
nel file, o in memoria compressa se è cambiata e il file non c'è, e chi ne aveva i chunk riceve
`ChunksGone`. Queste regole sono una classe sola, `core/storage/column_keeper`, usata dal server e
dal client che gioca da solo; cambiano solo gli hook (quali stage, chi avvisare).

## Come scorre una partita

**Locale.** `cnp_client` genera il mondo con lo stesso `Mapgen` del server, carica i mod di
`mods/` nella `Session`, e `run_interactive` fa girare input, `player_update`, mod, meshing e
render nello stesso processo.

**Connesso.** `ClientSession::connect` apre la connessione cifrata e manda `Hello`. Il server
risponde con `Welcome`: seme, hash del mondo, feature, canali dei mod, tabella dei blocchi con
forme e modelli, limiti di movimento. Il client adotta la tabella, scarica i media che non ha,
carica il codice del mondo se non lo rifiuta, e da lì riceve chunk (canale `Chunk`, dove arriva
anche `ChunksGone` con quelli da lasciare), corpi
(`Entity`, non affidabile) ed eventi (`Event`); manda claim di posizione, scavi, piazzamenti,
colpi e click. I canali da `kModChannelBase` (6) in su sono dei mod.

**Attraversare.** Un portale locale sposta il corpo con `portal_traverse`; lo decide il server,
che manda un `Teleport` con la velocità d'uscita (`exit_velocity` in `core/net/session.cpp`): la
velocità d'ingresso girata dalla mappa, con l'energia pareggiata solo fra il punto d'ingresso
mappato attraverso la coppia e il punto d'uscita, mai fra le quote delle due aperture. Un portale verso un
altro server usa un ticket firmato: il client ha già una sessione da osservatore aperta su
quel mondo (per disegnarlo dentro l'apertura) e la promuove a sessione attiva. Se non può, il
server manda un `Redirect` e il client si riconnette.

**Stanze.** Un server può tenere più mondi sulla stessa porta (`--room`, protocollo 56):
ognuno è `serve_room` su un thread suo, e il `Router` passa a ogni stanza gli eventi dei
suoi peer. Una stanza alla volta tiene il lock comune (`TurnLock` in `src/server/main.cpp`)
e lo lascia solo mentre aspetta la rete; il lock si passa **nell'ordine in cui è stato
chiesto**, e una stanza in ritardo sui tick lo lascia comunque una volta per poll. Con un
`std::mutex` una stanza occupata (l'inferno che genera colonne) se lo riprendeva prima che
le altre si svegliassero, e i giocatori della pista restavano senza risposta per più di un
secondo. Ogni stanza scrive nel log quanto l'ha tenuto al massimo.

## Come si disegna la luce

Il server calcola solo i livelli 0..15 (sole e blocchi, flood fill in `core/voxel/light.cpp`)
e li manda con i chunk; tutto il resto è del client, e il gioco (spawn, `core.light`) continua
a leggere i livelli e `daylight_at`.

- **Mesher** (`core/voxel/mesher.cpp`): per ogni angolo di faccia la media delle quattro celle
  che si toccano lì, già passata per `light_curve` (luminosità, non livello). La luce dei blocchi
  è girata verso la faccia: la direzione è la pendenza del flood fill (in salita verso la
  lampada), piena davanti, 0.6 di taglio, 0.25 da dietro (`kGrazing`, `kAway`, `kNormalBias`).
  Per questo lo snapshot ha due celle di bordo e non una. Un blocco luminoso illumina se stesso.
- **Shader del mondo** (`render/shaders.h`): cielo = visibilità del cielo × (ambiente per
  direzione della faccia + sole × N·L × ombra); poi il più chiaro fra cielo e lampade, × AO,
  su un minimo del 2% (`kLightFloor`).
- **Ombre** (`WorldRenderer::draw_shadows`): una shadow map ortografica attorno alla camera,
  ferma sulla griglia dei texel, con le foglie ritagliate dall'alfa dell'atlas. La disegna la
  vista principale e ogni vista dentro un portale per la propria camera virtuale, prima di
  aprire il proprio target (`gfx_shadow_create`: solo profondità, confronto in hardware).
- **Sole e luna** nel pass delle nuvole; **corpi e oggetti in mano** prendono la luce della cella
  (`WorldRenderer::cell_light`).
- **Classica** (`set_classic`, Opzioni → Lighting): la formula di prima, dal byte di luce
  impacchettato che il vertice porta ancora; niente ombre né sole.

## Dove si mette una feature

| Cosa | Dove |
|---|---|
| Una funzione Lua nuova | La firma in `src/script/api.luau`; la funzione host in `src/server/main.cpp` e, se serve anche in locale, in `make_client_host` di `src/client/main.cpp`; due righe in `docs/MODDING.md`. `docs/API.md` si rigenera da solo |
| Un evento del motore | Lo schema fra gli eventi del motore (`define_core_events` nel client, la tabella degli eventi in `main()` del server) e l'`emit` nel punto in cui succede |
| Una regola di gioco | Prima chiedersi se può essere un mod. Se è del motore: la funzione in `src/core/`, chiamata dal server (`net/session.cpp`) e dal client locale |
| Un messaggio di rete | Struttura con `encode`/`decode` in `core/net/protocol.h`, `kProtocol` +1 con una riga che dice cosa cambia, gestione in `net/session.cpp` e `net/client_session.cpp`. Un decoder deve rifiutare input troncato: il test `protocol` lo prova |
| Un dato salvato | `core/storage/world_db`, con `kWorldSchemaVersion` +1 e una migrazione dal formato precedente (il test `migration`) |
| Contenuti (blocchi, oggetti, mapgen, mob) | Un mod in `mods/`, vedi [`MODDING.md`](MODDING.md) |
| Grafica | `src/client/render/`, attraverso `gfx_*`; se serve una primitiva nuova, in `gfx.h` e `gfx_raylib.cpp` |

## Mod che il gioco include

| Mod | |
|---|---|
| `spike` | Il mod dello spike M0: blocchi, portale che si muove |
| `beacons` | Pilastri luminosi pulsanti, con capability `os` |
| `guard` | Protegge la piazza dei portali cancellando `core:pre_place` |
| `palette` | Venti blocchi colorati e un pennello |
| `console` | Un pannello dichiarato in Lua |
| `chest`, `craft`, `tools` | Casse, ricette, attrezzi con livello e durezza |
| `steps` | Lastra, scala e un blocco disegnato come modello |
| `timber` | Tronchi orientabili, pioli sulla faccia cliccata, muschio con ABM/LBM |
| `cave`, `ore`, `trees`, `lakes`, `ruins` | Grotte collegate, minerali, alberi, laghi e torri diroccate come stage del mapgen, una colonna alla volta |
| `depths` | Miniere abbandonate e cripte sotto terra, pianificate per regione e costruite una colonna alla volta |
| `portal` | Portali costruiti a blocchi: una cornice di `portal:frame` attorno a un buco d'aria, in piedi o coricata, apre un'apertura; tasto destro sulla cornice per il menù |
| `furnace` | Una fornace: il carbone fonde il grezzo in lingotti, la sabbia in vetro e il cibo; tiene l'esperienza della fusione e la dà a chi la apre (`furnace:xp`) |
| `creative` | Un inventario creativo su un pannello, col tasto C (`side = both`) |
| `doors` | Porte a due blocchi, anche per chi trova una strada (`core.find_path`) |
| `mobs`, `robot` | Zombie, scheletri, il robot che fa da corpo al giocatore |
| `animals`, `pets` | Maiali, mucche, pecore, galline (cibo, lana, uova, pelle, allevamento); gatto, cane, canna da pesca |
| `armor` | Sedici pezzi d'armatura nei quattro slot indossati |
| `infernoportal` | Ossidiana e portale dell'inferno costruito a mano |
| `farm` | Zappa, terra arata, grano in quattro stadi, pane |
| `bed` | Letto: rinascita; di notte ci si sdraia e con tutti a letto è l'alba, non con mostri vicini (`core.player_spawn`, `core.set_time`, `core.monsters_near`) |
| `screwdriver` | Cacciavite: gira un blocco già piazzato (`core.node_turns`) |
| `compact` | Blocchi da nove lingotti, gemme o polveri |
| `cinnabar` | Polvere, leva, pulsante, torcia, ripetitore, comparatore, lampada, pistoni, observer, hopper; il circuito si risolve in Lua con una coda di cambi ritardati |
| `torches`, `cane` | Torce; canna da zucchero sulle rive, carta, libri |
| `hunger` | Fame con saturazione ed esaurimento, barra disegnata dal mod (`side = both`) |
| `enchant` | Esperienza e tavolo da incantamento con estrazione a sorte e indizio (`side = both`) |

Un mondo con un modo di gioco suo sta in `games/<nome>/`, con `mods/` e `textures/`
da passare al server (`worlds/` resta ignorata: è dove una partita lascia i suoi
salvataggi). `games/yafps`: l'arena FPS del fork precedente, riscritta —
`yafps_core` (blocchi, arena come stage, movimento) e `yafps_game` (`side = both`:
bersagli, colpo deciso dal server, HUD). `games/track`: la pista coi kart a tempo.
`games/inferno`: caverne di scoria, lava, fortezze, brute, wraith e cinder.
`tools/track.sh` le serve tutte e tre da un solo server, come stanze del sandbox.
