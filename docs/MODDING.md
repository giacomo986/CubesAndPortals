# Scrivere un mod

Stato: M2 in corso. Questo documento descrive quello che **funziona oggi**, non
quello che è previsto; per il piano completo vedi `DESIGN.md` §5 e §6.

Un mod è una directory dentro `mods/` con due file:

```
mods/beacons/
    mod.conf      <- il manifest
    init.luau     <- il codice
```

Nessuna toolchain. `init.luau` è un file di testo che si apre in un editor e
gira. `--!strict` in cima abilita i tipi, ma non è richiesto (DESIGN.md §5.5).

---

## mod.conf

Formato `chiave = valore`, righe che iniziano con `#` sono commenti.

| chiave | default | significato |
|---|---|---|
| `name` | — (obbligatoria) | nome del mod, e namespace dei suoi blocchi |
| `version` | `0.0.0` | `1`, `1.2` o `1.2.3`; quello che manca è zero |
| `api` | la più vecchia servita | numero intero della API mod per cui il mod è scritto |
| `description` | vuoto | una riga |
| `entry` | `init.luau` | file caricato |
| `enabled` | `true` | `false` = presente ma non caricato |
| `capabilities` | vuoto | elenco separato da virgole, vedi sotto |
| `depends` | vuoto | mod che devono essere caricati prima, con o senza versione |
| `memory_kb` | 32768 | tetto dell'heap della VM |
| `interrupt_budget` | 2000000 | safepoint per singola chiamata |

**Una chiave sconosciuta è un errore di caricamento, non un avviso.** Scritto
`capabilties = os` per sbaglio, il mod non parte: un manifest che si legge come
"nessuna capability" per via di un refuso è esattamente il modo in cui un
sistema di permessi finisce per mentire.

Una directory senza `mod.conf` non è un mod che ottiene tutto: non è un mod.

---

## Versioni

`version` è **tre numeri**, non una stringa libera: `1`, `1.2` e `1.2.3` sono
tutte versioni, e quello che non è scritto vale zero. Tutto il resto — una
lettera, un componente vuoto, un quarto numero — è un errore di caricamento.

Il motivo è che una versione serve per essere **confrontata**. Come stringa
`1.10` viene prima di `1.9`, che è il contrario di quello che chiede chi scrive
`>= 1.9`.

### `depends`, con o senza versione

```
depends = spike >= 0.2, guard
```

Un nome da solo significa "qualsiasi versione", ed è quello che ogni `depends`
di questo repository voleva dire prima che le versioni fossero confrontabili.
Gli operatori sono cinque: `>=`, `>`, `=`, `<=`, `<`. Spazi a piacere:
`spike>=0.2`, `spike >=0.2` e `spike >= 0.2` sono la stessa cosa.

Due rifiuti diversi, perché sono due problemi diversi:

```
mod 'beacons' not loaded: depends on 'spike', which is not loaded
mod 'beacons' not loaded: depends on spike >= 2.0.0, and the one loaded is 0.2.0
```

Il primo manda a installare un mod, il secondo ad aggiornare quello che c'è.
Dire il primo quando la verità è il secondo è un pomeriggio perso.

### `api`, cioè per quale motore è scritto il mod

`api` è **un numero intero**, non una versione. Dice per quale versione della API
mod il mod è stato scritto. Questa build ne serve una sola — `api = 1` — e
rifiuta qualsiasi altra:

```
mod 'futuro' not loaded: written for mod API 2, and this build serves 1 to 1
```

Un mod che non lo dichiara viene preso per la **più vecchia** che questa build
serve. È l'unica assunzione che non possa promuovere di nascosto un mod vecchio
su una API nuova: il giorno in cui la più vecchia servita sale, quel mod viene
rifiutato e gli viene detto di dichiarare cosa vuole, invece di fallire alla
riga 40.

La riga di log lo dice, e dice anche quando non gliel'ha detto nessuno:

```
mod 'cave' 0.1.0 (api 1) loaded, capabilities: none
mod 'vecchio' 0.1.0 (api 1, assumed) loaded, capabilities: none
```

La stessa regola vale al reload: un `mod.conf` modificato in una API che questa
build non serve **non** viene caricato, e continua a girare la versione di prima.

---

## Cose che accadono alle celle da sole

Due tipi, e non sono la stessa cosa:

```lua
-- ABM: ogni `interval` secondi, ogni cella caricata che contiene uno di questi
-- blocchi ha una probabilità di uno su `chance` che l'azione parta.
core.register_abm("timber:moss", {"timber:log"}, 2.0, 4, function(cell)
    core.set_block(cell.x, cell.y, cell.z, MOSSY)
end)

-- LBM: una volta per cella corrispondente **quando il chunk viene caricato**, e
-- mai più. È quello che serve per sistemare un mondo scritto da una versione
-- vecchia del mod.
core.register_lbm("timber:clean", {"timber:mossy_log"}, function(cell)
    core.set_block(cell.x, cell.y, cell.z, LOG)
end)
```

L'azione riceve **una tabella** — `cell.x`, `cell.y`, `cell.z`, `cell.id` — che
è la forma di ogni handler di questo engine, da un evento a uno stage di mapgen.
L'elenco dei blocchi può essere una tabella o una stringa sola.

**Si registrano mentre il mod carica**, come `core.register_mapgen`: chiederne
uno da un handler di tick è rifiutato, perché sarebbe un modificatore che non
gira mai. Un nome già preso è rifiutato, e ABM e LBM condividono lo spazio dei
nomi. Un `interval` non positivo o una `chance` sotto uno sono rifiutati. Un
blocco che nessun mod ha registrato **disabilita** quel modificatore e lo dice
in log nominando il nome: un ABM su un blocco che non esiste spazzerebbe il
mondo per sempre senza trovare niente.

**Cosa costa, e perché non può far tardare il tick.** Un mondo da 512 sono 512
chunk e 16,7 milioni di celle. Un ABM non le legge: ogni chunk porta un bit per
id di blocco *modulo 64*, quindi un chunk che non può contenere i blocchi
dell'ABM viene saltato con un AND. Quello che resta viene letto cella per cella,
e ogni step ha un **budget di 2 ms**: se la spazzata non ci sta, si ferma dov'è
e riprende al tick dopo, e l'intervallo riparte solo da una spazzata
**finita**. Un mod che chiede più di così gira più lentamente di quanto ha
chiesto invece di far tardare il server, e il log lo dice:

```
modifiers: 3 action(s) over 262144 cell(s) in 0.008 s, 0 sweep(s) still going
  timber:moss           3 fired of     262144 tested, 0.008 s
```

**Determinismo.** La probabilità è un **hash** di seme, numero della spazzata e
coordinate della cella, non un generatore il cui stato dipende da come il budget
ha tagliato la spazzata. Due run dello stesso seme fanno la stessa cosa, e i
chunk si visitano in ordine ordinato per lo stesso motivo.

**Un'azione che va in errore costa il suo mod**, come un handler di evento, e
quel modificatore non viene più chiamato. Gli altri continuano nello stesso
step.

**Quello che un LBM non garantisce:** se il budget finisce a metà dei
modificatori di un chunk, quel chunk torna in coda e quelli che avevano già
girato ci girano di nuovo. Un LBM va scritto in modo che girare due volte su una
cella già sistemata sia innocuo — che è cosa vuol dire «caricamento» per un
modificatore che può essere interrotto.

---

## Blocchi che si girano

Un blocco dichiara **cosa significa** il byte di orientamento della cella, e
non significa niente se non lo dichiara:

| `turn` | valori | cosa è |
|---|---|---|
| — (niente) | 0 | non si gira |
| `upright` | 0..3 | quarti di giro attorno alla verticale: una cassa, una fornace |
| `facedir` | 0..23 | una qualsiasi delle ventiquattro orientazioni di un cubo: un tronco coricato, una scala |
| `wallmounted` | 0..5 | la **faccia** verso cui punta il davanti: una scaletta, un cartello |

```lua
local LOG = core.register_node("timber:log", 126, 92, 54, true, 0, "facedir", 0, 1.6, "wood")
core.set_block(x, y, z, LOG)
core.set_facing(x, y, z, 12)     -- coricato lungo x
```

`facedir` è `asse * 4 + giro`, dove l'asse dice **dove finisce il +Y del
blocco** — 0 +Y, 1 +Z, 2 -Z, 3 +X, 4 -X, 5 -Y — e il giro sono quarti di giro
attorno a quel nuovo alto. **0..3 sono esattamente i quattro giri che
esistevano**, perché l'asse 0 non inclina niente: ogni byte in ogni file di
mondo mantiene il significato che aveva. L'ordine degli assi è quello di
`param2` facedir dell'engine di partenza, che è l'engine da cui questo progetto viene: una
convenzione già provata, presa perché è buona. Il layer di compatibilità per cui
era stata scelta non si fa più (DESIGN.md §7.5), e l'ordine resta comunque questo
— cambiarlo adesso vorrebbe dire riscrivere il byte di ogni cella girata in ogni
file di mondo.

`wallmounted` è un **indice di faccia** in quest'ordine: 0 +X, 1 -X, 2 +Y,
3 -Y, 4 +Z, 5 -Z. Una cella che nessuno ha girato vale 0, cioè +X; il
piazzamento scrive sempre la faccia cliccata, quindi lo si vede solo su una
cella scritta con `core.set_block` e mai girata.

**Il piazzamento dà quattro giri, non ventiquattro.** Un blocco `upright` o
`facedir` messo giù da un giocatore si gira verso di lui, dalla sua
**posizione** (il server ha quella e non lo sguardo); un `wallmounted` punta
fuori dalla faccia che è stata cliccata. Le altre venti orientazioni esistono
per un mod — `core.set_facing` — e per l'attrezzo che un giorno girerà una cella
già piazzata.

**Un byte che il blocco non ammette viene rifiutato**, non mascherato: una
cassa girata a 7 è un errore di chi scrive il mod, e `core.set_facing` lo dice
in log con il nome del mod. Al disegno, una cella con un byte che il suo blocco
non ammette si disegna **non girata** invece di leggere fuori da una tabella.

**L'id non cambia mai.** Un blocco che si gira è **un** blocco: l'id che
`register_node` ha restituito è quello che c'è nel mondo comunque sia girato,
quindi un handler confronta `ev.id` direttamente.

**Le parti si girano con lo stesso giro delle facce.** Un blocco fatto di scatole
(`core.node_box`) coricato ha le scatole ruotate dalla stessa matrice che decide
quale immagine va su quale faccia: forma e immagini non possono dissentire su
come è messo.

---

## Un mod come un file solo

Una directory non è una cosa che si possa dare a qualcuno: niente dice quali
file ne fanno parte, niente dice che sono arrivati tutti, e niente si può
controllare prima di eseguire il Lua di un altro. Un **pacchetto** è quel file.

```sh
cnp_modpack pack   mods/ore            # ore-0.1.0.cnpmod, dal mod.conf che c'è dentro
cnp_modpack list   ore-0.1.0.cnpmod    # cosa contiene, e se è integro
cnp_modpack unpack ore-0.1.0.cnpmod ./ore
```

Un `.cnpmod` messo nella cartella dei mod si carica come una directory:

```
mods/
    cave/                 <- una directory
    ore-0.1.0.cnpmod      <- un pacchetto
```

Il motore lo verifica **intero**, lo scompatta in una cartella di appoggio e da
lì in poi è un mod come gli altri — stesso manifest, stesso ordine, stessi
rifiuti. Mezzo mod è peggio di nessun mod: registrerebbe metà dei suoi blocchi e
lascerebbe un mondo pieno di quelli, e chi lo guarda non può sapere quale metà.

Il formato, per intero, sta in [`src/modpack/mod_package.h`](../src/modpack/mod_package.h).
Quello che conta sapendo scrivere mod:

- **Il nome del mod non è scritto sul pacchetto**: sta nel `mod.conf` dentro,
  che è l'unico posto da cui si legge un manifest. Il nome del *file* lo decide
  il tool a partire da lì. Un nome fuori è un nome che può mentire.
- **Un pacchetto è canonico**: i file sono ordinati per percorso e i loro byte
  sono contigui, quindi impacchettare due volte lo stesso mod dà lo stesso file
  byte per byte. Misurato: `mods/ore` dà lo stesso `.cnpmod` a ogni giro.
- **Un percorso che esce dal mod è rifiutato** — `..`, un percorso assoluto, una
  barra rovesciata, due punti — e lo è tre volte: quando il pacchetto si scrive,
  quando si legge e quando si scompatta.
- **Cosa non c'è dentro**: i file che iniziano con un punto, a ogni livello. Un
  `.git` non fa parte di un mod.
- **Un pacchetto non si ricarica a caldo.** L'hot reload guarda i tempi di
  modifica dei file scompattati, che non cambiano: si edita una directory, si
  spedisce un pacchetto.
- **Due mod non possono avere lo stesso nome.** Un pacchetto accanto alla
  directory dello stesso mod sono due copie che registrano gli stessi blocchi:
  la seconda viene rifiutata dicendo da dove viene la prima.

---

## Capability

Luau parte già molto più piccolo di Lua: **non esistono** `io`, `package`,
`loadstring`, `dofile`, `os.execute`, `os.getenv`. Non c'è niente da rimuovere
perché non c'erano. Quello che resta e che un mod non ottiene per default:

| capability | cosa concede | stato |
|---|---|---|
| `os` | `os.clock`, `os.date`, `os.difftime`, `os.time` | concessa |
| `debug` | la tabella `debug` | concessa |
| `native`, `exec`, `fs:read <dir>`, `net:http <host>` | — | **rifiutate**: questa build non le concede a nessuno |

Una capability sconosciuta è un errore di caricamento. Una dichiarata ma non
concedibile è un errore di caricamento **con il nome dentro il messaggio** — non
un caricamento riuscito con una sorpresa a runtime.

Le globali non concesse vengono **rimosse dalla tabella globale prima che la
sandbox venga congelata** e prima che una riga di mod giri. Non sono nascoste né
vuote: sono assenti. Un mod può verificarlo da sé:

```lua
assert(os == nil, "questo mod non ha dichiarato os")
```

`os` è dietro una capability perché il tick deve poter essere deterministico per
seed + input (DESIGN.md §5.7), e un mod che legge l'orologio non lo è.

---

## Una VM per mod

Ogni mod ha la sua `lua_State`, la sua quota di memoria e il suo budget di
interruzione. Le conseguenze sono verificate da `tests/script_isolation_test.cpp`
(gira in `ctest`, senza GPU):

- Un mod che va in **loop infinito** viene interrotto (`script exceeded its
  interrupt budget`) e disabilitato. Gli altri continuano a girare **nello stesso
  tick**.
- Un mod che **esaurisce la memoria** riceve un errore Lua, non fa morire il
  processo. È la lezione del mondo 512 di M1, dove l'OOM killer arrivò prima.
- Le **globali non sono condivise**: due mod possono avere entrambi `elapsed` e
  `on_step` senza vedersi.
- Un mod che fallisce viene disabilitato una volta, non ritentato ogni frame.

Il timeout di 60 s sul test non è burocrazia: se l'interrupt smette di scattare,
il test non fallisce — gira per sempre dentro il `while true do end` del mod
cattivo. Verificato mettendo a zero quel budget.

---

## Ordine di caricamento

Alfabetico, poi riordinato per `depends`. Serve davvero: `beacons` viene prima di
`spike` in ordine alfabetico, e alla prima esecuzione con due mod
`core.node_id("spike:copper")` ha restituito `0`. Con `depends = spike` restituisce
l'id giusto.

Una dipendenza assente è un rifiuto che la nomina. Un ciclo non blocca il
caricamento: i mod coinvolti vengono rifiutati uno per uno, ciascuno dicendo cosa
stava aspettando.

---

## Hot reload

Modifica un file di un mod e salva: entro mezzo secondo il mod viene ricaricato,
**senza riavviare il gioco e senza rigenerare il mondo**. `F5` forza il reload di
tutti i mod.

Cosa garantisce il reload:

- **Il codice nuovo gira, quello vecchio sparisce.** VM nuova, globali nuove.
- **Una versione che non compila non ti costa il mod.** La VM nuova viene
  costruita e fatta girare *prima* che la vecchia venga distrutta: se fallisce,
  continua a girare la versione che funzionava, e l'errore va in log.
- **Gli id dei blocchi restano gli stessi.** Il mondo è pieno di quei blocchi;
  rinumerarli vorrebbe dire riscrivere ogni chunk.
- **Un blocco che il mod non definisce più diventa orfano**: id conservato,
  texture magenta acceso, ancora solido (nessuno deve cadere attraverso un
  pavimento che c'era). Lo stesso patto che fanno i classici giochi a blocchi.
- **Le capability non si allargano da sole.** Il manifest viene riletto e
  rivalutato: aggiungere `capabilities = native` e salvare fa fallire il reload,
  non lo concede.
- **Gli altri mod non vengono toccati**: stessa VM, stesso stato, nessun tick
  perso.

### Stato che sopravvive al reload

```lua
function on_save(): string
    return tostring(counter)      -- il mod decide cosa sia il suo stato
end

function on_load(state: string?)
    counter = tonumber(state) or 0
end
```

Una stringa, non una tabella: un serializzatore generico dovrebbe avere
un'opinione su funzioni, cicli e userdata, e sarebbe sbagliata su tutte e tre.
Il limite è 64 KB, e superarlo è un errore — non un troncamento silenzioso.

---

## Event bus tipizzato

Lo schema di un evento si **dichiara**. Emettere un campo sbagliato è un errore
che nomina il campo, non un `nil` che qualcuno legge tre handler dopo.

```lua
core.event.define("guard:alarm", {
    fields = { x = "number", y = "number", z = "number" },
    cancellable = false,
})

core.event.on("core:pre_place", 10, function(ev)
    if ev.x >= 36 and ev.x <= 46 then
        ev.cancel = true                       -- annulla
    elseif ev.id == core.node_id("core:grass") then
        ev.id = core.node_id("core:stone")     -- modifica
    end
end)

core.event.emit("guard:alarm", { x = 40, y = 21, z = 40 })
```

Tipi ammessi nei campi: `number`, `string`, `boolean`. Tre, non uno qualunque:
un bus che trasporta qualsiasi cosa è un bus il cui schema non significa niente.

- **Priorità**: numero, default 0, si esegue dal più basso al più alto; a parità
  vince chi si è registrato prima. Dichiararla è il punto — l'ordine di
  caricamento non è un ordine.
- **Annullare** ferma gli handler *successivi*. Chi veniva prima ha già parlato.
- **Modificare** il payload lo cambia per gli handler dopo **e per il motore**:
  `core:pre_place` con `ev.id` riscritto piazza il blocco riscritto.
- **Cross-VM**: un mod emette, un altro ascolta, nessuno dei due chiama l'altro.
- Un handler che va in errore **disabilita il suo mod e basta**: l'evento
  prosegue verso gli altri handler.
- Ascoltare un evento che nessuno ha dichiarato è un errore di caricamento che
  lo nomina. Ridichiarare l'evento di un altro mod è rifiutato.

### Eventi del motore

Il core usa lo stesso bus, con gli stessi schemi, senza percorsi privilegiati
(DESIGN.md §6.3).

| evento | campi | annullabile |
|---|---|---|
| `core:pre_place` | `x, y, z, id, player` | sì |
| `core:placed` | `x, y, z, id, player` | no |
| `core:pre_break` | `x, y, z, id, player` | sì |
| `core:broken` | `x, y, z, id, player` | no |
| `core:ui_click` | `panel, id, player, text` | no |
| `core:used` | `x, y, z, id, player, held` | sì |
| `core:tick` | `dt` | no |
| `core:hit` | `player, target, damage, killed` | no |
| `core:watched` | `x, y, z` | no |
| `core:world_ready` | — | no |
| `core:net_message` | `player, mod, text` | no |
| `core:command` | `player, name, args` | no — solo al mod che l'ha dichiarato |
| `core:page_open` | `player, page` | no — solo al mod che ha registrato la pagina |
| `core:page_action` | `player, page, id, text` | no — solo al mod che ha mostrato la pagina |
| `core:input` | `action, player` | sì — solo su un client |
| `core:far_moved` | `index, x, y, z, axis, sign, half_w, half_h` | no — solo su un server a stanze |

`core:used` è un click destro su un blocco a portata, **con qualsiasi cosa in
mano**. Nomina il blocco colpito, non la faccia accanto, ed è emesso **prima**
del piazzamento: un handler che mette `ev.cancel = true` ha **reclamato** il
click, e il piazzamento dietro non avviene. È così che un baule si apre con un
blocco in mano invece di costringere a posarlo — e senza quel `cancel` lo stesso
click aprirebbe il baule e costruirebbe sopra. Con **Shift** premuto e un blocco
in mano che si può posare, `core:used` non viene emesso: il click piazza e basta,
anche contro un baule o un tavolo.

`held` è quello che chi ha cliccato **aveva in mano**, per nome, vuoto a mani
vuote — e `id` è il blocco **cliccato**. Un pedestal che leggeva `id` per sapere
cosa gli veniva offerto stava leggendo sé stesso. Il click è l'unico momento in
cui il motore sa cosa c'è in mano: `Inventory` tiene uno slot selezionato, ma sul
server viene scritto solo quando un piazzamento riesce, quindi fra due
piazzamenti è lo slot dell'ultimo e non la mano. Per questo `held` sta
sull'evento e non è una chiamata `core.held_item(player)` che risponderebbe con
qualcosa di vero un po' di tempo fa.

`text` è quello che il giocatore ha scritto in un campo (`core.ui_field`) quando
ha premuto Invio, `""` per un bottone: testo di un client, fino a 128 byte, da
trattare come non fidato.

`player` è chi l'ha fatto: un numero da ripassare a `core.ui_show` per mettere
un pannello su **quello** schermo e su nessun altro. Un mod non se lo inventa e
non tiene una tabella di giocatori. Nel client in un solo processo il campo c'è
lo stesso e vale sempre lo stesso valore, così un mod scritto per un server gira
lì senza sapere dove sta.

`core:tick` è l'hook di tick per chi preferisce sottoscriversi invece di
definire una globale `on_step`. Dal 2026-10-06 arriva anche al codice di un mondo
sulla macchina di un giocatore collegato, una volta per frame col suo `dt` (un
orologio da mostrare, un primo messaggio al server appena c'è); `on_step` e il
resto restano del server. Un mod `both` che registra un tick solo per il server lo
mette dentro `if serving`. Un emit senza handler costa una ricerca hash e
un vettore vuoto.

`core:world_ready` è **dove va messo tutto ciò che guarda il mondo**, e non è
una comodità. Un mod si carica **prima** che il file del mondo venga aperto — il
file viene controllato contro la tabella dei blocchi *finale*, quindi non si può
aprire finché ogni mod non ha registrato i suoi — perciò un mod che legge il
mondo mentre si carica sta leggendo quello che ha fatto il mapgen, non quello
salvato. Un clock che leggeva il proprio conteggio al caricamento leggeva zero
ogni volta, e vedendo che non c'era niente ricostruiva sopra ciò che stava per
essergli caricato addosso. `core:world_ready` è il motore che dice: adesso il
mondo è quello che sarà. Viene emesso una volta sola, e anche su un server senza
file del mondo — un mod che lo aspetta non deve aspettarlo per sempre.

`core:net_message` è quello che è arrivato **sul canale di un mod** — vedi
`core.net` più sotto. Il campo `mod` è il canale riletto come nome. L'evento
arriva **solo agli handler del mod di quel canale** (dal 2026-09-25): un messaggio
mandato da `hunger` non lo sente `enchant`. Prima arrivava a tutti, e sul client
le barre di `mods/hunger` e `mods/enchant` si scambiavano i valori.

`core:hit` è un colpo di un giocatore **andato a segno** su un corpo (una
creatura, un'entità di un mod con salute): `target` è l'id del corpo, `damage` i
punti tolti, `killed` se era l'ultimo. Emesso dopo il fatto, sul server e nel
client che è server di sé stesso. `mods/hunger` lo conta come fatica,
`mods/enchant` come esperienza per un'uccisione.

`core:far_moved`: un'apertura di un'altra stanza dello stesso server, che porta
all'apertura `index` di questo mondo, è stata spostata con `core.portal_place`;
gli altri campi sono la sua nuova forma. Arriva all'inizio del tick successivo.
Serve a spostare anche questo capo (`games/inferno` costruisce così il telaio
corrispondente). Un server su un altro indirizzo non lo sente. Le colonne attorno
a ogni apertura vengono sempre generate, anche senza giocatori vicini.

`core:watched` è una scrittura su una cella che il mod ha chiesto di osservare con
`core.watch_cell` — vedi più sotto. Arriva all'inizio del tick successivo, una
volta per cella per tick.

Limite noto: se una versione di un mod chiama `define` e poi fallisce il
caricamento, lo schema modificato resta. Gli handler no — quelli vengono tolti
con la VM che li aveva registrati (senza quel passaggio il processo va in
segfault al primo emit dopo un reload; verificato).

---

## L'API disponibile oggi

**Riferimento completo: [API.md](API.md)** — generato, con firme, tipi e default.
Per il type checker dell'editor: [api.d.luau](api.d.luau).

```lua
core.set_block(x, y, z, id)
core.get_block(x, y, z) -> id
core.register_node(nome, r?, g?, b?, solido?, luce?, girevole?, livello?, durezza?) -> id
core.node_id(nome) -> id            -- 0 se non esiste
core.node_name(id) -> nome
core.set_facing(x, y, z, giro)      -- 0 .. core.node_turns(id) - 1
core.node_turns(id) -> n            -- 1, 4, 6 o 24: quanti giri ha quel blocco
core.loaded(x, y, z) -> 1 | 0       -- 0: colonna non in memoria, get_block legge aria
core.light(x, y, z) -> 0..15        -- luce ora: dei blocchi, o del cielo per l'ora
core.get_facing(x, y, z) -> giro
core.set_portal(indice, x, y, z)    -- sul server: le aperture del mondo, solo mentre i mod caricano
core.portal_open(x, y, z, asse, verso, giro, mezza_l, mezza_a, colore?) -> 0..15|-1  -- sul server, quando ascolta: -1 prima, riprovare
                                    -- colore: com'è chiusa finché non porta da nessuna parte: 0 verde (fissa, default), 1 blu, 2 arancione (portal gun)
core.portal_close(x, y, z) -> 0..15|-1                             -- per centro, non per indice
core.portal_link(a, b) -> 1|0       -- a e b portano l'una all'altra; b = -1 scollega a
core.log(messaggio)
core.play_sound("mod:cosa", x, y, z) -> 1|0  -- a tutti nel mondo; file mods/<mod>/<cosa>.ogg sul client

core.take_item(player, nome, quanti?) -> presi   -- fino a `quanti`, dice quanti
core.give_item(player, nome, quanti?) -> entrati -- quello che non entra resta al mod

core.players() -> { id }                         -- chi ha un corpo in questo mondo
core.player_pos(id) -> (x, y, z) | nil           -- i PIEDI, o niente se non c'e'
core.player_name(id) -> nome                     -- "" se non c'e'; resta uguale a un rientro, l'id no
core.player_look(id) -> (dx, dy, dz) | nil       -- dove guarda, lunghezza 1
core.player_place(id, x, y, z, dx?, dy?, dz?) -> 1 | 0  -- sposta e gira un corpo, come un portale
core.ride(id, host, x?, y?, z?) -> 1 | 0         -- seduto sul corpo di host; host 0 = scende
core.monsters_near(x, y, z, r) -> n              -- creature che combattono entro r
core.raycast(x, y, z, dx, dy, dz, max?) -> (bx, by, bz, distanza) | nil

core.meta_set(x, y, z, chiave, valore) -> 1 | 0   -- valore vuoto = cancella
core.meta_get(x, y, z, chiave) -> stringa        -- "" se non c'è niente
core.storage_set(chiave, valore) -> 1 | 0         -- del mod, nel world file; vuoto = cancella
core.storage_get(chiave) -> stringa              -- "" se non c'è niente
core.shared_storage_set(chiave, valore) -> 1 | 0  -- lo stesso, uno per tutte le stanze
core.shared_storage_get(chiave) -> stringa
core.notice(player, testo) -> 1 | 0              -- una frase sullo schermo; 0 = tutti
core.chat(player, testo) -> 1 | 0                -- una riga nella chat (T); 0 = tutti
core.command(nome, aiuto, perm?) -> 1 | 0       -- dichiara /nome; arriva come core:command
core.has_perm(player, perm) -> 1 | 0            -- il ruolo del giocatore arriva a perm?
core.role(player) -> stringa                     -- guest, player, helper, moderator, admin, owner
core.is_muted(player) -> 1 | 0                   -- la sua chat è bloccata (/mute)?
core.page_register(nome, titolo, perm?) -> 1 | 0 -- una pagina che /page NOME apre
core.page_show(player, nome, markup) -> 1 | 0    -- mostra una pagina a un giocatore

core.find_nodes(x1,y1,z1, x2,y2,z2, nome) -> { x,y,z, x,y,z, ... }

core.register_mapgen(nome, fn)                   -- al caricamento, non dopo
core.terrain_height(x, z) -> y                   -- il suolo secondo il generatore
core.water_level(x, z) -> y                      -- fin dove sta l'acqua: 18 sul mare, un lago in quota il suo;
                                                 -- il suolo dove la colonna è asciutta, 0 senza generatore
core.biome(x, z) -> nome                         -- il clima della colonna, secondo il generatore:
                                                 -- core:plains/desert/tundra/peaks, o dove il suolo sta
                                                 -- rispetto al mare: core:ocean/sea/river/lake/beach,
                                                 -- o core:gorge
core.node_liquid(id, opacità?) -> 1 | 0          -- ci si nuota dentro, e ci passa la luce

core.container_take(x, y, z, item, quanti?) -> presi  -- da un contenitore, dal primo slot

core.ecs.component(nome, { "campo", ... })       -- campi numerici, dichiarati
core.ecs.phase(nome, ordine)                     -- un punto nel tick, e un numero
core.ecs.system(nome, fase, componente, fn)      -- fn(batch), una volta per molte
core.ecs.spawn(componente, x, y, z) -> id | nil
core.ecs.set(id, campo, valore) -> bool          -- "x"/"y"/"z" o un campo del mod
core.ecs.get(id, campo) -> numero | nil
core.ecs.kill(id) -> bool
core.ecs.clip(id, nome) -> bool                  -- una clip del suo modello; "" la ridà al movimento
core.ecs.box(id, mezza, alta) -> bool            -- quanto è grande per un colpo
core.ecs.count(componente) -> quante

core.net.open() -> canale | nil                  -- al caricamento, non dopo
core.net.send(player, testo) -> bool             -- 0 = tutti in questo mondo

core.render.pass(nome, ordine, fn)               -- solo su un client
core.render.box(x, y, z, w, h, d, r, g, b, a)    -- solo dentro un pass
core.render.model(id, x, y, z, yaw?, scala?, pitch?, roll?)   -- un modello del mondo, solo dentro un pass
core.render.fx(tipo, x, y, z, misura?, r?, g?, b?, a?, t?, x2?, y2?, z2?)   -- un effetto di luce, solo dentro un pass
core.overcast(quanto)                            -- cielo e nuvole grigi, 0..1: il meteo (mods/weather)

core.hud.pass(nome, ordine, fn)                  -- sullo schermo, una volta per frame
core.hud.text(x, y, testo, size?, align?, r, g, b, a)
core.hud.rect(x, y, w, h, r, g, b, a)            -- solo dentro un hud pass
core.hud.image(nome, x, y, w, h, a)              -- un PNG dei media del mondo
core.hud.item(nome, x, y, lato)                  -- il quadrato di un oggetto, come nella borsa
core.hud.mouse() -> x, y                         -- il cursore, in frazioni dello schermo

core.event.define(nome, { fields = {...}, cancellable = bool })
core.event.on(nome, priorità?, funzione) -> ref
core.event.emit(nome, { campi }) -> annullato
```

`register_node` restituisce l'id assegnato: **un mod non vede mai un numero che
non ha chiesto**, e nessuna parte del motore sa più qual è l'id della pietra.
Registrare due volte lo stesso nome restituisce lo stesso id — è l'aggancio su
cui poggia l'hot reload.

`on_step(dt)`, se definita come globale, viene chiamata una volta per frame;
`on_save`/`on_load` servono al reload. Un mod senza `on_step` non paga niente
per frame se non la ricerca della globale.

`meta_set`/`meta_get` sono **quello che una cella ricorda**: sopravvive alla
chiusura del mondo, e sparisce quando la cella diventa aria — rompere un blocco
si porta via il suo stato, così il blocco piazzato dopo non eredita quello di
prima. Cambiare *id* invece non lo cancella: un pulsante che passa da su a giù è
la stessa cella con lo stesso stato.

**Stringhe, e solo stringhe**, di proposito. Un pedestal che salva l'id 41 sta
salvando qualunque cosa voglia dire 41 la prossima volta che il mondo si apre,
perché gli id sono assegnati in ordine di registrazione; un **nome** sopravvive
all'aggiunta di un mod. Chi vuole un numero scrive `tostring` e legge `tonumber`,
che è una conversione che si vede. Limiti: nome del campo 1–64 byte, valore ≤ 256
byte, 16 campi per cella, 65536 celle per mondo — rifiutati, non troncati, con il
motivo nel log.

`storage_set`/`storage_get` sono **quello che un mod ricorda**, non legato a una
cella: stringhe sotto un nome, solo del mod che le scrive, salvate nel world file
con i chunk (solo le chiavi cambiate). Il file si legge dopo che i mod hanno
caricato, quindi si rilegge da `core:world_ready` in poi. Senza world file, e nel
client che gioca da solo, durano finché il processo gira; il codice che un mondo
manda al client non ne ha (0 e ""). Limiti: chiave 1–64 byte, valore ≤ 4096 byte,
4 MiB di chiavi e valori per mod (fino al 2026-09-28 erano 1024 chiavi). `games/track` ci tiene il giro migliore di ogni giocatore
(`best:<nome>`) e la classifica (`board`).

`shared_storage_set`/`shared_storage_get` sono lo stesso, ma **uno per tutte le
stanze** del server: nel world file della prima (`--room`, il sandbox), che ogni
stanza legge e scrive. Ci va quello che è del giocatore ovunque sia -- la fame
(`hunger`), l'esperienza (`enchant`), le chiavi viste (`keys`), il contatore che
numera gli zaini (`backpack`) -- e non quello che è di un posto (un letto, un
tavolo), che resta in `storage_set`. Un valore tenuto in memoria va riletto
quando il giocatore torna: nel frattempo può essere cambiato in un'altra stanza.
Su un server di un mondo solo è lo stesso archivio di `storage_set`. Il contenuto
di uno zaino (un container portato, sotto `kCarriedBelow`, al numero dello
zaino) lo sposta il server stesso nella stanza dove entra chi lo porta.

`notice` mette una frase sullo schermo di un giocatore per qualche secondo, dove
va il motivo di una modifica rifiutata. Serve a chi **cambia** quello che il
giocatore ha fatto invece di rifiutarlo: un mod che sostituisce il blocco piazzato
(`ev.id = ...`) deve dirlo, o il giocatore vede un blocco che non aveva in mano.

Un handler che scrive nel payload una chiave che l'evento non ha — `ev.cancelled`
invece di `ev.cancel` — viene segnalato nel log, una volta per mod, evento e chiave.

`players`/`player_pos` sono **chi c'è e dov'è**. Due chiamate e non una: «chi c'è»
e «dov'è quello» sono domande diverse, e una sola che restituisse una lista di
posizioni risponderebbe al pulsante e lascerebbe a bocca asciutta il pedestal,
che deve dare qualcosa a un giocatore **con un nome**. Gli osservatori — un
portale che guarda dentro — **non ci sono**: non hanno avatar né inventario
(DESIGN §4.4), quindi non c'è nessuno da mettere su un pulsante.

`player_pos` restituisce **i piedi**, ed è la posizione che l'autorità del
movimento ha *accettato*, non l'ultima rivendicata da un client. Per chi non è
in questo mondo non restituisce niente:

```lua
local x, y, z = core.player_pos(id)
if x == nil then continue end   -- se n'è andato fra le due chiamate
```

Non `0, 0, 0`: l'origine è un posto vero, e un mod che ci misurasse una distanza
troverebbe tutti in piedi sul pulsante nell'angolo del mondo.

Queste due sono anche le prime a usare **due forme di ritorno nuove** del
generatore — una lista e più valori. Prima di M8 `core.*` era «scalari dentro,
uno scalare fuori», e l'alternativa era scriverle come cinque chiamate
(`player_count`, `player_at`, `player_x/y/z`) dove ne bastano due.

`take_item`/`give_item` sono **la borsa di un giocatore**, per nome e non per id
per la stessa ragione di `meta_*`. Tutt'e due restituiscono **quanti se ne sono
mossi davvero**, che è l'unica risposta onesta: una borsa può non avere quello
che si chiede, e può essere piena. Quello che non entra con `give_item` **non
viene lasciato cadere e non si perde**: non gli succede niente, e decide il mod —
`core.drop_item` lo mette per terra dove finisce la pila di un blocco rotto, e
non fare niente lascia al giocatore il compito di fare spazio. Un motore che lo
buttasse deciderebbe per tutti i mod insieme. Chi le usa viene avvisato: la borsa
gli viene rimandata al tick dopo, così non continua a essere disegnato con in
mano quello che un mod gli ha tolto.

`find_nodes` sono **le celle che un mod non ha piazzato**: l'equivalente di un LBM
dell'engine di partenza. Un mod che ha messo giù i propri blocchi poteva anche solo ricordarsi
dove; questo serve per quelli che ha messo giù un *giocatore*, o il mapgen. Triple
piatte e non una tabella per cella, che sarebbe un'allocazione per cella
sull'unica chiamata che si fa su tutta una regione: `#trovati // 3` è quante sono.

Due limiti, dichiarati e non nascosti. Tornano al massimo **512 posizioni**,
quindi una ricerca che ne restituisce esattamente 512 probabilmente ne ha trovate
di più e va rifatta su una scatola più piccola. E una scatola di più di **un
milione di celle** viene **rifiutata**: la ricerca gira dentro il tick, e il
budget di interrupt che ferma un mod impazzito conta i safepoint nella VM, non il
tempo che il motore spende per conto del mod. Anche un **nome che nessuno ha
registrato** viene rifiutato invece di rispondere «non ce ne sono»: un refuso che
risponde zero è un mod che non fa niente e una segnalazione che dice che la
ricerca è rotta. Tutt'e due i rifiuti tornano una **lista vuota** — in una lista
non c'è posto per una frase — e la ragione va nel log.

### Entità che il mod possiede: `core.ecs`

```lua
core.ecs.component("torretta:pallino", { "vx", "vy", "vz", "eta" })
core.ecs.phase("torretta:moto", 100)
core.ecs.phase("torretta:colpo", 200)

core.ecs.system("torretta:vola", "torretta:moto", "torretta:pallino", function(b)
    for i = 1, b.n do
        b.x[i] += b.vx[i] * b.dt
        b.eta[i] += b.dt
    end
end)

local e = core.ecs.spawn("torretta:pallino", 32.5, 22.0, 32.5)
core.ecs.set(e, "vx", 0.4)
```

Il batch `b` che arriva a un sistema ha `b.n` (quante entità), `b.dt`, `b.tick`,
`b.id` e una **colonna per campo**: `b.x`, `b.y`, `b.z` e i campi del componente.
Sono array, si indicizzano da 1, e **quello che ci si scrive dentro viene
riletto**: un sistema che ha mosso qualcosa l'ha mossa.

Tre cose che questo fa e una tabella Lua scorsa in `on_step` no:

- **sono le entità del motore.** Un pallino è un'entità nel mondo dove stanno i
  mob e le pile per terra. Un secondo magazzino sarebbe due spazi di id per una
  cosa sola.
- **una chiamata per molte entità.** 512 entità, una chiamata. Una chiamata per
  entità spende più tempo ad attraversare il confine che dentro.
- **fasi ordinate da un numero.** Due mod che non si sono mai visti girano in un
  ordine dichiarato invece che nell'ordine in cui si sono caricati. I numeri si
  lasciano larghi apposta: chi vuole girare in mezzo a due non deve chiedere a
  nessuno di rinumerare.

Limiti, dichiarati: un sistema nomina **un** componente e riceve chi ce l'ha —
non c'è join fra due, e chi ne vuole un secondo lo legge per id, che è una
chiamata per entità e restituisce esattamente quello che il batch serviva a
evitare. I campi sono **numeri**; le stringhe stanno su una cella (`meta_set`) o
dentro il mod. Un batch è al massimo **4096** entità, quindi un mod che ne ha di
più riceve più chiamate. E le entità vivono nel mondo che possiede la tabella
delle sessioni: un server offline (`--ticks` senza `--listen`) non ne ha.

Una fase dichiarata due volte con **due numeri diversi** è un errore che nomina
tutt'e due i mod: tenere il primo in silenzio metterebbe uno dei due in un ordine
che non ha chiesto senza dirglielo mai.

### Un canale per mod: `core.net`

```lua
local canale = core.net.open()          -- mentre il mod si carica, non dopo
core.event.on("core:net_message", function(ev)
    core.net.send(ev.player, `ricevuto: {ev.text}`)
end)
```

Il messaggio **non porta il nome del mod**: il nome è il **canale**, che è quello
che l'handshake ha distribuito. Un nome nel payload sarebbe una seconda risposta
a «di chi è questo».

Chiesto al **caricamento**, perché i numeri escono nel welcome: un mod che
chiedesse dopo il primo client collegato avrebbe un numero che a quel client non è
mai stato detto, e `core.net.open` in quel caso non risponde niente e lo scrive nel
log. Un messaggio è una stringa di al massimo **512 byte**.

Dal lato client il numero è **del server**: `core.net.open` non risponde niente
finché non c'è una connessione, e un mod client non ne ha bisogno — manda con
`core.net.send` e sente con `core:net_message`. Il `player` è ignorato da quella
parte, perché un client parla al server e basta.

### Disegnare: `core.render`

```lua
core.render.pass("ologramma:luce", 100, function()
    core.render.box(34.5, 22.0, 32.5, 1.5, 1.5, 1.5, 255, 0, 255, 255)
end)
```

Un **pass** non è l'evento `core:tick`: un handler di tick gira *fra* i frame,
senza camera montata e senza niente in cui disegnare, e gira una volta per frame —
mentre un portale disegna questo mondo di nuovo da un'altra camera. Un ologramma
disegnato da un tick sarebbe assente attraverso ogni apertura. Un pass gira dentro
il render, **una volta per camera**, e disegnare è l'unica cosa che può fare:
`core.render.box` fuori da un pass è rifiutata e lo dice.

`core.render.model(id, x, y, z, yaw, scala)` disegna nello stesso punto del
frame un modello del mondo (un id di `core.model_id`), fermo nella sua posa di
riposo, con i piedi in `x, y, z` e girato di `yaw` attorno alla verticale (e prima,
se dati, inclinato di `pitch` attorno alla sua x e di `roll` attorno alla sua z:
una carta tenuta in mano, `mods/cards`): una
cosa che solo questa macchina sa dove sta, come il cubo in mano a un giocatore
di `games/chambers` (che il server non gli manda, `core.mob_unseen`). Restituisce
false per un modello che questo client non ha.

`core.render.fx(tipo, x, y, z, misura, r, g, b, a, t)` disegna un **effetto di
luce** con gli shader del motore (`src/client/render/fx_render.cpp`): si somma a
quello che c'è dietro, i muri lo coprono e non copre gli altri effetti. `tipo` è
`"glow"` (un bagliore), `"flame"` (una fiamma che sale), `"ring"` (un'onda d'urto
stesa a terra), `"frost"` (schegge di ghiaccio a terra), `"bolt"` (un fulmine da
`x, y, z` a `x2, y2, z2`), `"slash"` (l'arco di un fendente), `"sparks"`
(scintille che volano via), `"vortex"` (un vortice) o `"jet"` (una fiamma che
esce da `x, y, z` verso `x2, y2, z2`, il nitro dei kart). `misura` è il raggio in
nodi (per il fulmine la sua larghezza), `t` quanto della sua vita è passato, da 0
a 1: lo shader cresce, spazza e sfuma con quello, e il mod conta solo il tempo.
Restituisce false per un tipo che non c'è.

Il numero ordina i pass **fra di loro**, non contro il disegno del motore: c'è un
punto solo nel frame dove i mod disegnano, dopo il mondo opaco e i corpi e prima
delle aperture.

Solo su un client. Su un server `core.render.pass` dice «questo processo non
disegna» invece di fallire genericamente.

Quello che **non c'è ancora**: un `core.ecs` che sappia unire due componenti.
Le entità di un mod visibili agli altri giocatori ci sono: `core.ecs.visible`,
sotto.

### Un blocco che vuole un attrezzo

`register_node(..., dig_level = 2)` dichiara un blocco che **non esce** se in
mano non c'è almeno un attrezzo di livello 2. Non «esce più lentamente»: non
esce, per quanto si tenga premuto. `register_item(..., dig_level = 3)` dichiara
l'attrezzo dall'altra parte.

```lua
local DURO   = core.register_node("mymod:duro", 90, 90, 110, true, 0, false, 2)
local PUNTA  = core.register_item("mymod:punta", 1, "", 3)
```

Zero è la **mano nuda**, ed è quello che vale per ogni blocco e ogni oggetto che
non dice niente — cioè tutto quello che esisteva prima che i livelli esistessero.

È un **livello e non un tipo**: un'ascia di livello 2 scava la pietra come un
piccone di livello 2. I gruppi sono un'altra cosa e più grande; il livello è la
metà che fa una progressione.

Ed è separato da `breakable`, che è un no diverso: il pavimento del mondo non è
un blocco molto duro, è il pavimento, e non esce per nessun attrezzo che possa
mai esistere. Per questo il rifiuto per l'attrezzo dice cosa serve — «that needs
a level 2 tool, and this one is level 0» — e quello del pavimento no: mandare
qualcuno a cercare un attrezzo migliore per la roccia di fondo è mandarlo a
cercare una cosa che non c'è.

### Un blocco che ci mette del tempo

`register_node(..., dig_level, hardness)` dice quanti **secondi** ci vuole a
mano nuda; `register_item(..., dig_level, dig_speed)` dice quante volte più
veloce scava un attrezzo. Zero secondi è istantaneo, ed è quello che vale per
ogni blocco che non dice niente.

```lua
core.register_node("mymod:granito", 120, 120, 130, true, 0, false, 2, 4.0)
core.register_item("mymod:punta", 1, "", 3, 4.0)   -- quattro volte più veloce
```

Chi scava tiene premuto. **L'orologio è del server**: il primo colpo su una
cella avvia lo scavo, quello dopo i secondi la rompe, e in mezzo non succede
niente — due messaggi per uno scavo, non sessanta, e un client che si affretta
manda il secondo colpo troppo presto e non ottiene nulla. Spostare la mira o
lasciare il pulsante ricomincia da capo, e così fa un blocco che cambia sotto.

Sotto il mirino compare una barra mentre lo scavo va. A zero non si disegna
niente, quindi un blocco istantaneo non la fa mai comparire.

**I blocchi del motore dichiarano i loro secondi**: la pietra 1,5, il legno del
telaio 0,9, il vetro 0,35, e la terra, la sabbia e la neve sotto il mezzo
secondo. Nessuno di loro chiede un attrezzo — `dig_level` resta 0 su tutti,
perché gli attrezzi sono contenuto e stanno in un mod, e un blocco del motore
che ne pretendesse uno sarebbe un blocco che nessuno può scavare con i mod
spenti. La roccia di fondo non ha durezza affatto: una durata non è un rifiuto,
e un pavimento reso impassabile dall'essere molto lento è un pavimento che
qualcuno buca in un pomeriggio.

### Un attrezzo è per un tipo di lavoro

`register_node(..., material)` dice di cosa il blocco è **fatto**;
`register_item(..., digs)` dice per cosa l'attrezzo **è**. Livello e velocità
valgono contro quel materiale e sono quelli di una mano nuda contro tutto il
resto — ed è tutto qui quello che rende un'ascia diversa da un piccone.

```lua
core.register_node("mymod:granito", 120, 120, 130, true, 0, false, 1, 4.0, "stone")
core.register_item("mymod:piccone", 1, "", 1, 5.0, "stone")
core.register_item("mymod:pala",    1, "", 1, 4.0, "soil")
```

La pala qui sopra è **migliore del piccone in ogni numero che ha** — stesso
livello, e attraverso la terra va quattro volte più veloce di una mano — e
davanti al granito non vale niente: contro `stone` è livello 0 e velocità 1. Un
livello dice quanto è buono un attrezzo, un materiale dice **in cosa**, e senza
il secondo ogni attrezzo è lo stesso attrezzo con un numero diverso sopra.

`register_item(..., damage)`, dopo `digs`, dice quanti punti toglie un colpo
dato con quell'oggetto in mano: 0 è quello di un pugno (1). Il colpo porta lo
slot della hotbar e il numero lo legge il server dalla **sua** tabella, quindi
un client non può dire di colpire più forte.

```lua
core.register_item("tools:sword", 1, "", 0, 1.0, "", 4)   -- 4 dei 20 di un corpo
```

Vuoto da una parte o dall'altra vuol dire «nessuno l'ha detto», e allora la
regola non si applica: un attrezzo che non nomina un materiale è generale (vale
il suo livello ovunque) e un blocco che non dice di cosa è fatto si scava con
qualsiasi attrezzo per quello che vale. Sono i due modi di tacere, ed è quello
che è ogni blocco e ogni item registrato prima che i materiali esistessero.

Il rifiuto lo dice: «that needs a level 1 tool for stone, and this one is level 0
against stone». Il numero da solo suonerebbe come una bugia a chi ha in mano
un'ascia di livello 3: l'ascia **è** livello 3, quello che non è è un attrezzo
per questo.

I blocchi del motore usano quattro nomi — `stone`, `soil`, `wood`, `glass` — e
non uno per blocco: un materiale per ogni blocco è una tabella dove ogni
attrezzo è il suo attrezzo, cioè un elenco e non una regola.

### Un blocco che non è un cubo

`core.node_box(id, x0, y0, z0, x1, y1, z1, pezzo)` aggiunge una scatola alla
forma di un blocco. Chi ne dichiara almeno una **smette di essere un cubo**: il
mesher lascia stare la sua cella e le scatole le disegna un passo a parte.

```lua
local CHEST = core.register_node("chest:box", 150, 110, 60, true, 4, true)
core.node_box(CHEST, 0.00, 0.00, 0.00, 1.00, 0.62, 1.00)
core.node_box(CHEST, 0.03, 0.62, 0.03, 0.97, 0.88, 0.97, "lid")
```

Le coordinate sono nello **spazio del nodo**: 0 è una parete della cella, 1
quella opposta.

Le scatole **sono** il blocco, non un suo disegno: un corpo è fermato da quelle e
non dalla cella, un raggio trova quelle e attraversa il resto della cella, e il
contorno che il giocatore vede è disegnato attorno a quelle. Un blocco alto mezza
cella si calpesta a mezzo nodo da terra e nell'altra metà della sua cella ci si
cammina. `mods/steps` è il pezzo di gioco che lo mette in mano: una lastra e una
scala.

Ci si sale sopra **camminando**, fino a 0.55 di nodo: è quello che rende una
scala una scala. Più in alto si salta, e un nodo intero si salta sempre -- un
cubo resta un muro.

La forma **viaggia**: dal protocollo 27 il welcome la porta accanto al nome del
blocco, quindi anche un client che il tuo mod non ce l'ha vede le scatole giuste
e ci cammina sopra come chi ce l'ha. Quella del server vince sempre, anche su un
nome che il client conosce già.

### Un pezzo di pista: `core.node_piece`

Pezzi di strada come nei giochi di corse acrobatiche: una strada liscia su una griglia di 8 nodi,
che si aggancia alla griglia quando la si piazza e si unisce al pezzo accanto
dove i bordi coincidono. Il blocco è l'angolo del pezzo; le altre celle sono un
blocco "fill" (scatole sotto la strada, su cui si cammina, mai disegnate).

```lua
local FILL = core.register_node("pista:fill", 60, 62, 70)
local CURVA = core.register_node("pista:curva_sopraelevata", 64, 66, 76, true, 0, "upright")
-- plan 2 = curva a destra, raggio 2 quadrati, sponda sinistra 4 nodi più alta
core.node_piece(CURVA, FILL, 2, 2, 0, 0, 0, 4, 4, true)
core.place_piece(96, 20, 88, CURVA, 2)   -- entrata nel quadrato di (96, 88), verso +z
```

Il kart corre sulla strada vera (una mesh uguale su client e server); un piede,
un raggio e i controlli del server vedono le celle fill, sempre sotto la strada.
Scavare una cella qualunque toglie il pezzo intero; mirandolo si vede il contorno di tutto il pezzo, e con un pezzo in mano il pezzo stesso, trasparente, dove andrà (rosso se andrebbe sopra un altro pezzo o se coprirebbe il giocatore). Un pezzo prende il posto di terreno e blocchi che trova nelle sue celle e svuota le 3 celle sopra la strada; lo ferma solo un altro pezzo di pista; Z e X lo ruotano di un quarto prima di piazzarlo. Nella borsa l'icona di un pezzo è la sua pianta vista dall'alto. `plan` 3 e 4 sono una chicane, a destra e a sinistra. Con `wall = true` (ultimo argomento, solo dritti) bank0 e bank1 sono gradi di rotazione della strada attorno al suo centro: a 90 diventa un muro, e un kart ci resta sopra finché va veloce (da 10 m/s, del tutto da 16). `plan` 5 è una curva a sinistra; `width = 2` (ultimo argomento) fa la strada larga due quadrati, e un pezzo largo si piazza dal quadrato della sua corsia destra. `plan` 6 e 7 sono un giro della morte (stretto o largo, 2 o 3 quadrati, senza cordoli): la strada sale, passa sopra capovolta e riscende sulla corsia accanto, a sinistra (6) o a destra (7); il kart ci resta attaccato come su un muro, e lento cade dalla cima. Srotolato, un giro è una chicane: lo spostamento di una corsia si sterza. `plan` 8 e 9 (solo `width = 2`) restringono la strada larga alla sua corsia destra o sinistra in due quadrati; piazzati al contrario la allargano. Una discesa è una salita
piazzata al contrario. Esempio completo: `games/track/mods/track_core` (tutti i pezzi
e i circuiti che ci costruisce, `/pezzi` per averli nella borsa, con gli strumenti linea di partenza e checkpoint che `track_game` legge).

Sulla pista (`games/track`) `/pezzi` dà per primi tre strumenti, letti da
`track_game`:

- **start line**: tasto destro su una strada o un pezzo mette una linea di
  partenza di traverso, larga quanto la strada (12 per lato al massimo),
  attraversata allontanandosi da chi ha cliccato; trave bianca sopra. Una nuova
  linea è una pista nuova con la sua classifica (`customN`). Cliccata di nuovo,
  la toglie con tutta la pista.
- **checkpoint**: il checkpoint successivo, trave gialla; cliccato di nuovo, lo
  toglie.
- **offroad**: due click sono due angoli opposti di un'area (256 per lato al
  massimo) dove durante un giro il kart può stare su qualunque cosa; un paletto
  luminoso a ogni angolo, un click dentro la toglie.

E l'**editor** (`track_core:editor`, di `track_core`): tasto destro su un pezzo
segue la sua pista all'indietro fino all'inizio e in avanti fino al capo aperto,
e apre un pannello con i pezzi che ci si attaccano -- stessa larghezza, stessa
pendenza e stessa sopraelevazione all'ingresso di quelle che il capo lascia (la
metà che manca a una salita, l'uscita da una curva sopraelevata...), anche
piazzati al contrario ("rev"). Un click lo mette giù lì (`core.place_piece`) e
il pannello si rifà dal nuovo capo; **undo** toglie l'ultimo messo dal pannello
(`core.take_piece`); **close circuit** cerca (A* sulle caselle della griglia,
direzione e quota) i pezzi che dal capo aperto tornano all'inizio -- dritti,
curve, chicane e `rise` nei due versi, mai
dove c'è qualcosa, prima in piano e solo se non basta con le salite, e con gli
adattatori se le due estremità hanno larghezze diverse -- e li mette giù. Tenuto nella borsa, ogni pezzo messo a mano
apre il pannello da sé. **B** con l'editor in mano (o il pulsante **pick from afar** del pannello, che col cursore libero non sente i tasti) sceglie **da lontano**,
senza limiti di distanza: il corpo resta dov'è (`core.fixed_view`), la
telecamera guarda dal punto in cui si sta l'inizio del pezzo in prova, che è già
messo giù; A e D lo tolgono e provano il prossimo che si attacca (saltando quelli
che non entrano), **W** lo tiene e prova il primo dal nuovo capo, S
toglie l'ultimo tenuto, F chiude il circuito, B esce togliendo quello in prova.
Al posto della hotbar c'è una griglia con tutti i pezzi (`core.hud.item`): accesi
quelli che si attaccano al capo, scuri gli altri, in giallo quello in prova; un
click sinistro su un pezzo lo mette in prova (cliccato di nuovo, nell'altro verso
se entra in tutti e due), fuori dalla griglia non fa niente.
I tasti li sente la metà client di `track_game` e li passa a `track_core` con
l'evento `track_core:pick`; la vista torna con `track_core:pick_view`. A ogni pezzo la pista si ritrova con `core.piece_entry`,
che dice da dove si entra nel pezzo di una cella qualunque.

Un corpo che guida (`core.body_drive`) ha il nitro: Shift con l'acceleratore lo spinge fino a 1.35 volte la sua `sprint`, consumando in 3 s una riserva che si ricarica in 20; il motore lo calcola uguale su client e server.

Fuori da un giro il kart va dove vuole; durante un giro, fuori da strada, pezzi,
paddock e aree per mezzo secondo, è rimesso sulla strada o sul pezzo piano più
vicino. Il paddock e gli accessi alle piste sono terra battuta
(`track_core:paddock`), consentita in giro come la strada. Linee e aree stanno
nel salvataggio del mod (`core.storage_set`).

### Un blocco disegnato come modello

`core.node_model(id, modello)` fa disegnare il blocco come un **glTF** invece che
come un cubo o come scatole: è lo stesso modello che indossa un corpo, dichiarato
con `core.register_model`.

```lua
local M = core.register_model("steps:marker", "marker.gltf")
local B = core.register_node("steps:marker", 200, 170, 90, true, 0, "upright")
core.node_model(B, M)
core.node_box(B, 0.15, 0.0, 0.15, 0.85, 0.6, 0.85)   -- dove sta il corpo
```

**Il modello è la figura, le scatole sono il blocco.** Nessuno chiede a una mesh
dove può stare un corpo o cosa ha trovato un raggio: una mesh sono mille
triangoli, una collisione sono quattro scatole. Un blocco che vuole una forma
sotto il modello dichiara tutte e due, come `collision_box` accanto a
`drawtype = mesh` nell'engine di partenza.

Il modello viene ruotato attorno al **centro della cella** dal byte di facing,
come una scatola, quindi un modello disegnato dentro il cubo unitario con i piedi
a 0 sta dove sta il blocco e guarda dalla parte in cui è stato messo. Un client
che il file non ce l'ha disegna le scatole, e se non ce ne sono non disegna
niente: mai un cubo, perché un cubo è l'unica cosa che quel blocco non è.

Limiti dichiarati: un blocco-modello prende la luce direzionale dei corpi e non
quella della sua cella, non si anima e non viene tagliato da un portale.

Quello che una forma **non** cambia: un oggetto caduto si posa sulla cella e non
sulla scatola. E quello che **non** viaggia con lei: se il blocco è solido, la
sua durezza, il suo livello e il suo materiale restano del server -- chi non ha
il mod li scopre quando il server rifiuta. TODO.md OPEN-43.

Il nome del pezzo non è decorativo: **una scatola con un nome si muove**. Ruota
verso l'alto attorno al proprio spigolo posteriore inferiore finché qualcuno ha
quella cella aperta, e torna giù quando l'ultimo che ci guardava dentro chiude.
Il motore non impara cos'è un coperchio: chiede soltanto se quella scatola un
nome ce l'ha.

Chi apre e chi chiude lo decidono i contenitori: `core.container_show` mette la
cella fra quelle aperte, e uscire dal pannello, allontanarsi troppo o
disconnettersi la rimette fra quelle chiuse. **È la CELLA a essere aperta, non
la sessione** — un coperchio che vedesse solo chi ha cliccato non è
un'animazione, è un effetto locale.

Sul filo viaggia **un bit**, quando cambia: il movimento fra chiuso e aperto lo
disegna ogni client per conto suo, a un quarto di secondo, al proprio frame
rate. Un client che entra mentre una cassa è già aperta non lo sa finché quella
non cambia — scritto in TODO OPEN-32 invece che nascosto.

Tre conseguenze, e nessuna è un dettaglio:

- **Smette di nascondere i vicini.** Una scatola che riempie sei decimi della
  cella non copre niente, quindi il blocco non è più opaco qualunque cosa avesse
  detto. Una verità sola: il mod che si dimenticasse di spegnere l'opacità si
  ritroverebbe un buco nel pavimento dietro la cassa.
- **Il giro gira la FORMA.** Un cubo è sei quadrati sulle pareti della cella, e
  girarlo può solo voler dire spostare le immagini fra quelle sei; una scatola
  ha una forma, e gira con lei. La faccia verso cui una scatola guarda dopo il
  giro è quella di cui porta l'immagine ed è quella che la ombreggia.
- **Le collisioni restano quelle del cubo.** La forma è come il blocco si vede;
  la fisica non l'ha saputo. Una cassa alta sei decimi si cammina sopra come un
  blocco intero.

Non entra nella mesh del chunk perché non ci starebbe: lì le posizioni sono
**byte, un'unità per nodo, 0..32 inclusi**, e gli ottavi vorrebbero 0..256, cioè
un valore più di quanti ne tenga un byte. Le scatole visibili si raccolgono
invece in un buffer solo, ricostruito ogni frame e disegnato in una chiamata
sola, con **lo stesso shader e lo stesso atlante** dei cubi — quindi luce,
ombreggiatura per faccia e ciclo del giorno sono quelli, e non una seconda
copia che diverge.

Su un server, un client che quel mod non ce l'ha vede un **cubo**: la tabella
dei blocchi sul filo sono nomi, e la forma non viaggia ancora.

### Un liquido: `core.node_liquid`

```luau
local ACQUA = core.register_node("laghi:acqua", 52, 104, 178, false)
core.node_liquid(ACQUA, 150)      -- 0..254: quanto nasconde di quello che c'è dietro
core.node_liquid(RIVOLO, 150, 3)  -- e quanto è piena la cella, in ottavi (8 = tutta)
```

Il terzo argomento, `fill`, è **quanto è piena una cella** di quel liquido, da 1 a 8
ottavi. Viaggia nella tabella dei blocchi (protocollo 40), quindi un client senza il
mod disegna ogni livello alla sua altezza: la superficie di una cella prende, a ogni
angolo, l'altezza della più piena fra le celle di liquido che si toccano lì (una
cella con altro liquido sopra conta piena), e così **si inclina** dal lato alto a
quello basso e due vicine mettono la stessa altezza sullo spigolo che condividono.
Le facce fra due celle di liquido non si disegnano: un lago è una superficie, non
una pila di pareti trasparenti. Un liquido non passa più dal mesher dei cubi — i cui
vertici sono byte, un'unità per nodo — ma da celle con quattro altezze che il
renderer costruisce in float, come fa con i box.

Un blocco liquido **non ferma un corpo, non ferma la luce, si disegna trasparente
e non si scava**. Un corpo che ci sta dentro anche solo in parte **nuota**: i tasti
lo spingono a metà del suo passo, affonda piano (al massimo 2.5 m/s) e sale
tenendo premuto il salto (3.5 m/s). Sono numeri di `core/physics/movement.h`,
gli stessi con cui il server controlla i claim: un corpo in un liquido conta come
appoggiato, quindi risalire da un lago non è volare anche in un mondo dove non si
vola. Un blocco piazzato in una cella di liquido **prende il posto** del liquido.

Il server dice a ogni client che entra che cosa fa ogni blocco a un corpo e alla
luce (protocollo 38), quindi un client **senza il mod** nuota nel lago come quello
che ce l'ha — prima un blocco non solido di un mod era un muro per chi non aveva
il mod.

Quello che un liquido **non fa**: muoversi da solo. Il motore dice che una cella
è liquida, non che scorre — non c'è simulazione di fluidi da nessuna parte in
`src/`. Chi vuole l'acqua che scorre la scrive come un mod, e `mods/lakes` è
l'esempio: una **sorgente** (`lakes:water`, piena) e sei livelli di acqua che
scorre (`lakes:flow1`..`lakes:flow6`, da due a sette ottavi), un blocco per livello
perché il livello di una cella è il blocco che c'è dentro; un ABM due volte al secondo dà a ogni cella
che scorre un passo — giù se sotto c'è aria o acqua che scorre più debole, e
arriva a livello 6 (un ruscello scende un pendio fin dove il pendio va); di lato
un livello più debole **solo se sotto c'è qualcosa che la regge**, nell'aria e
**nell'acqua che scorre più debole**, che alza al proprio livello, e il nuovo
livello prosegue lungo quel ruscello; può finire anche sopra l'acqua, di un lago o
di un ruscello, ma lì si ferma, perché acqua che scorre sopra altra acqua ci si
unisce e basta — e **asciuga** quella che non ha più niente sopra né niente di più
bagnato accanto che si allarghi. Le sorgenti non vengono spazzate: un lago di mille celle ferme non
costa niente. Ad aprire la strada all'acqua ci sono due cose sole, una scavata
accanto (`core:broken`) e un secchio versato.

### Un blocco con un davanti

`register_node(..., rotatable = true)` dichiara un blocco che ha un **davanti**.
Piazzarlo gira **la cella**, non l'id: la cella porta un byte che dice di
quanti quarti di giro attorno alla verticale è girata, e il mesher legge quel
byte per decidere quale immagine va su quale faccia. L'id nel mondo è quello che
`register_node` ha restituito, sempre.

```lua
local FORNO = core.register_node("mymod:forno", 150, 110, 60, true, 0, true)

core.event.on("core:used", function(ev)
    if ev.id ~= FORNO then return end       -- l'id è quello, comunque sia girato
    core.log(`forno girato di {core.get_facing(ev.x, ev.y, ev.z)}`)
end)
```

Un mod può girare una cella da sé con `core.set_facing(x, y, z, giro)` e
leggerla con `core.get_facing(x, y, z)`. Il giro vale 0..3; qualunque altra cosa
viene ignorata. `core.set_block` dice **quale** blocco, non come è girato: la
cella che riscrive torna a 0, altrimenti una cassa costruita dove c'era un forno
guarderebbe dove guardava il forno.

Non è sempre stato così. Fino a poco fa registrare un blocco con un davanti
registrava **quattro id** — la base e tre quarti di giro — e quello nel mondo
era uno dei tre in più, per cui un handler che confrontava `ev.id` con l'id
restituito non riconosceva tre piazzamenti su quattro e la cassa si apriva solo
a mani vuote. Era un difetto vero, di questo mod. Il prezzo era su ogni
chiamante: chiedere `core.node_base` prima di sapere che blocco fosse. Quella
chiamata non c'è più, e nemmeno i tre id.

`core.find_nodes` cerca un nome e trova le celle qualunque verso guardino. Quello
che è sparito con gli id è cercare un **orientamento** — il vecchio
`mymod:forno^2` trovava solo quel giro: quando servirà, sarà un argomento in
più a `find_nodes`, non quattro nomi.

Il pacchetto di texture dipinge le facce per nome — `mymod:forno:front` — senza
sapere niente dei giri: vedi [TEXTURE_PACKS.md](TEXTURE_PACKS.md).

### Un corpo con una faccia

`core.register_model(nome, file)` dichiara un modello e restituisce un id. Il
**nome** è quello che viaggia — un corpo sul filo porta l'id, e il mondo manda la
tabella dei nomi una volta sola nel saluto — mentre il **file** è dove il
modello sta *su questa macchina*, accanto all'`init.luau` del mod:

```lua
local PERSONA = core.register_model("mymod:persona", "persona.gltf")
core.player_model(PERSONA)   -- il corpo di un giocatore
core.mob_model(PERSONA)      -- e le creature di --mobs
```

### Creature

`core.register_mob` dichiara un **tipo** di creatura; il comportamento è del
motore (`src/core/ecs/mob.h`). Due modi di combattere: `"melee"` si avvicina e
colpisce, `"archer"` resta a distanza e tira frecce finché vede il giocatore.

```lua
local ZOMBIE = core.register_model("mymod:zombie", "zombie.gltf")
--                nome            modello vita vel.  lotta    danno portata ogni
core.register_mob("mymod:zombie", ZOMBIE, 20,  2.2, "melee",  3,    1.6,    1.0)
core.spawn_mob("mymod:zombie", x, y, z)   -- una, adesso, lì
```

Le creature di ogni tipo registrato escono **al buio** (luce 7 o meno) tra 18
e 40 nodi da un giocatore, fino a sei vicino a ciascuno, puntano il più vicino
entro 16 nodi e spariscono quando nessuno è entro 72. Il modello suona le clip
per nome come un giocatore (`idle`, `walk`, `run`, `jump_loop`, `swim`) e, mentre
colpisce o tende l'arco, `attack` o `shoot`. Solo su un server: in un mondo
locale non ci sono creature. `mods/mobs` è l'esempio completo.

Una creatura che muore (o si allontana oltre la portata) non sparisce di colpo:
il client la lascia dov'era e la **dissolve** in 1.2 s, suonando una volta la clip
`death` del modello se ce l'ha, ferma nell'ultima posa se no. Solo lato client,
nessun messaggio in più: la lista dei corpi vicini arriva intera a ogni messaggio.

**Ogni nuova creatura deve avere la sua clip `death`**, nel modello che passa a
`core.register_mob` e in ogni variante che indossa (addomesticata, tosata,
cucciolo): una clip che suona una volta e resta ferma sull'ultima posa, circa un
secondo. Il server lo controlla quando la creatura viene registrata e scrive nel
log `register_mob('...'): its model has no "death" clip` se manca. Quelle del
progetto:

| creature | come muoiono | da dove |
|---|---|---|
| zombie, scheletro (`mods/mobs`), brute (`games/inferno`) | su un fianco | `tools/export_mob_models.py` (il brute copia lo zombie: poi `tools/make_inferno_models.py`) |
| cane, gatto (`mods/pets`), maiale, mucca, pecora, gallina (`mods/animals`), con cuccioli, tosata e addomesticati | su un fianco | `tools/make_pet_models.py`, `tools/make_animal_models.py` |
| wraith, cinder (`games/inferno`) | il wraith si inclina, scende e arriccia i tentacoli; al cinder cadono i bastoni e poi la testa | `tools/make_inferno_creatures.py` |
| ghoul, imp, pinky, gazer (`games/yafps`) | ognuno a modo suo | `tools/make_fps_models.py` |
| umano, rospo, talpa (`mods/villagers`), robot aiutante (`mods/helpers`) | su un fianco | `tools/make_villager_models.py` |
| bacteria, smiler (`games/backrooms`) | il bacteria barcolla e cade rigido in avanti; lo smiler chiude il ghigno e scende a terra girandosi | `tools/make_backrooms_creatures.py` |

In Blender la più semplice è `tip_over(rig, [mesh])` di `tools/make_pet_models.py`:
la creatura va su un fianco, sollevata di mezza larghezza perché non entri nel
pavimento, con qualunque rig; `drop=` la fa anche cadere, per una che vola.
Le clip che il motore suona per nome, tutte facoltative tranne `death`: `idle`,
`walk`, `run`, `jump_loop`, `swim`, `swim_idle`, `attack` (mischia) o `shoot`
(arciere), `sit` (animale seduto), `death`; un lavoratore quelle che il suo mod nomina.

Un tipo con lotta `"pet"` è un **animale domestico**: non attacca nessuno, esce
**di giorno** su terreno al sole (su `spawn_on`, se dato), al massimo sei
vicino a ogni giocatore (contati a parte dalle creature della notte), passeggia e si ferma a guardare chi gli sta entro 4
nodi. `core.mob_tame` dice cosa lo addomestica e cosa indossa dopo:

```lua
local DOG, DOG_TAME = core.register_model("mymod:dog", "dog.gltf"),
                      core.register_model("mymod:dog_tame", "dog_tame.gltf")
core.register_mob("mymod:dog", DOG, 16, 3.2, "pet", 0, 1.6, 1.0, "", 1, 0, false, 0, false, "core:grass")
core.mob_tame("mymod:dog", "mobs:bone", DOG_TAME)   -- l'oggetto, e il modello col collare
```

Tasto destro col suo oggetto in mano: ne consuma uno ed è tuo. Da allora ti
segue (corre se resti indietro di più di 5 nodi, ti ricompare accanto oltre
14), non sparisce mai, resta suo anche dopo un riavvio del server (il motore
lo salva nel file del mondo), e un altro tasto destro lo fa sedere (clip
`sit`) o rialzare. Vale anche nel mondo locale. Clip: `idle`, `walk`, `run`,
`sit`. `mods/pets` è l'esempio completo.

Un tipo con lotta `"worker"` è un **lavoratore**: non combatte, non esce da
solo e non sparisce mai; lo fa nascere il suo mod (`core.spawn_mob` restituisce
l'id) e lo guida lui. Il motore lo fa camminare e mostra cosa fa:

```lua
local id = core.spawn_mob("mymod:contadino", x + 0.5, y, z + 0.5)
core.mob_goto(id, cx, cy, cz)          -- a quella cella, per la strada di core.find_path
if core.mob_going(id) == 0 then        -- arrivato (o fermo dove non c'è strada)
    core.mob_face(id, wx, wz)
    core.mob_act(id, "gather", "farm:hoe")  -- clip in loop e oggetto in mano, a tutti (protocollo 81)
end
core.mob_act(id, "", "")               -- di nuovo idle e walk
core.mob_place(id, x + 0.5, y + 1, z + 0.5)  -- messo lì, fermo: per salire dove non si cammina
core.mob_remove(id)                    -- via subito, senza morte: il mod lo riprende
```

Un tasto destro su un lavoratore arriva al mod come `core:used_mob`
(`player, target, held`). L'oggetto in mano si disegna come quello di un
giocatore: il modello `<item>_held` sull'osso `hand_R`, o l'osso `tool_<nome>`
del modello stesso. `core.node_yield(id)` dice cosa dà un blocco rotto, per chi
scava al posto di un giocatore. `mods/villagers` e `mods/helpers` sono gli esempi.

Un `"pet"` senza `core.mob_tame` è un **animale selvatico**: esce, pascola e
lascia il suo drop, ma nessuno lo addomestica. Colpito, va nel **panico**:
scappa correndo dal colpo per cinque secondi. `mods/animals` (maiale,
mucca, pecora, gallina) è l'esempio.

```lua
-- tasto destro con le forbici: 2 lana a terra, modello tosato per 300 s, e panico
core.mob_shear("mymod:sheep", "mymod:shears", "mymod:wool", 2, SHEEP_SHORN, 300)
-- un uovo a terra ogni 120-300 s, e uno in più fra i drop quando muore
core.mob_lays("mymod:chicken", "mymod:egg", 120, 300)
-- nutrito d'erba è innamorato 30 s (HEARTS gli gira sopra la testa): due vicini
-- fanno un cucciolo (metà taglia, il suo modello, resta vicino a un genitore,
-- adulto dopo 1200 s); non di nuovo prima di 300 s
core.mob_breed("mymod:cow", "core:grass", COW_BABY, 1200, HEARTS)
```

Le forbici si consumano se hanno `durability` (register_item): una tosatura
ne usa un punto.

Il corpo di ogni creatura (con cui cammina, passa nei varchi e viene colpita)
è preso dal suo modello: alto quanto il modello, largo quanto è da fianco a
fianco, mai più di 1.8 × 0.6. Due creature non stanno una dentro l'altra: si
spingono via finché fra i loro centri c'è la somma delle loro mezze larghezze
di modello (queste non limitate a 0.3: una mucca ne tiene 0.375).

Le creature trovano la strada con `core.find_path`, che un mod può usare per
qualsiasi corpo che cammina (un animale che segue il padrone, un abitante che
torna a casa):

```lua
-- da, a, e il corpo: alto 2 celle, sale 1, scende 3, nuota; 2000 celle al massimo
local via = core.find_path(x1, y1, z1, x2, y2, z2, 2, 1, 3, true, 2000)
for i = 1, #via, 3 do
    local x, y, z = via[i], via[i + 1], via[i + 2]   -- la cella dei piedi, passo dopo passo
end
```

Senza strada, o finito il budget, la lista porta alla cella **più vicina**
raggiunta: controlla se l'ultima è quella che hai chiesto.

La strada legge i blocchi dalla **forma**: un mezzo blocco è un gradino che
si sale senza saltare, un pannello sottile (una scala, una porta aperta) lascia
passare accanto. `core.node_climbable(id)` fa di un blocco una **scala**: chi ci
sta dentro sale con salto e si tiene con accovacciarsi, e la strada ci sale e
scende. `core.node_door(chiusa, aperta)` dice che due blocchi sono **una porta**:
la strada passa da una aperta e si ferma davanti a una chiusa, tranne per un
corpo che apre le porte (`opens_doors` in `register_mob`). `mods/doors` e le
scale di `mods/timber` sono gli esempi.

Un modello di un altro mod si cerca per nome con `core.model_id("suomod:kart")`,
come `core.node_id` cerca un blocco: 0 se non c'è.

Un nome di file che prova a uscire dalla cartella del mod — con una barra, una
barra rovescia o un punto iniziale — **viene rifiutato** e la chiamata
restituisce 0. Un mod è contenuto e il contenuto arriva da dove capita.

Dichiarare lo stesso nome due volte **tiene l'id**: un reload non spoglia i corpi
che lo indossano.

Dall'altra parte il client non vede mai quel percorso. Il mondo **serve** il
modello — il `.gltf` e tutto quello che nomina accanto a sé — per la stessa
strada delle texture, e il client lo ricostruisce in una cartella sua. Quello che
il mondo non serve viene cercato nella cartella dei modelli del giocatore
(`--models`), dove `mymod:persona` diventa `mymod_persona.gltf`. Un client che
non ha né l'uno né l'altra disegna la scatola che disegnava prima — mai un buco
dove c'era un giocatore.

Un modello viaggia **intero o niente**: un `.gltf` senza il buffer che nomina
non è mezzo personaggio, è un personaggio con un buco, e chi lo guarda non può
sapere quale file manca.

Un modello fatto di **parti rigide** — un veicolo con le ruote, come esporta
Sketchfab — è montato dai suoi nodi: ogni mesh senza scheletro prende la
trasformazione del nodo che la porta, rotazioni, scale e specchi compresi. Una
mesh con uno scheletro la mettono i giunti. Il davanti di un corpo è −Z: un
modello esportato con il muso verso +Z va girato nel file. Il kart di
`games/track`, com'è uscito da Sketchfab, ha già il muso verso −Z.

Il mod `mods/chest` è questo, scritto per intero: `chest:box` è `rotatable`, e i
suoi due handler — quello che apre la cassa e quello che la svuota quando viene
rotta — confrontano `ev.id` con l'id che `core.register_node` ha restituito, e
basta. **Un mod che aggiunge un blocco sposta gli id di quelli registrati dopo
di lui**: gli id sono posizionali. Un file di mondo scritto prima tiene i nomi
della sua tabella, e all'apertura i suoi chunk vengono riscritti per nome in
quella nuova. I blocchi che nessun mod caricato registra più diventano
segnaposto magenta con il loro nome (non si possono tenere in mano), e tornano
loro stessi quando il mod torna; il server li elenca all'avvio. Un id tenuto in una variabile va quindi
chiesto a `core.register_node` a ogni avvio, mai scritto a mano.

### Scrivere tante celle in una volta

`core.set_block` scrive il mondo e lo dice a chi è connesso. Come lo dice
dipende da quante celle di UNO stesso chunk hai cambiato in quel tick:

* poche — una per una, nove byte più il motivo, per sessione;
* più di 150 — il chunk viene tolto da quello che quella sessione ha e lo
  streamer glielo rimanda intero, calcolato dal mondo com'è adesso.

Non c'è niente da scegliere: il risultato è lo stesso mondo in tutti e due i
casi. Misurato: 640 celle di un chunk sono 640 messaggi contro 1 chunk, e
quattrocento celle costano 6834 byte come celle e 233 come chunk. Una sessione a
cui il mondo sta ancora arrivando non riceve niente finché non è arrivato — le
celle aspettano, non si perdono.

L'illuminazione non si paga a cella: ogni posto da cui un mod scrive (un tick,
un evento, il caricamento) è dentro un `LightRun` del server, che rifà la luce
una volta sola alla fine, per tutte le celle cambiate. Un'esplosione che scava
trecento celle in un tick (`games/front`) è un ricalcolo, non trecento. Quello
che resta da pagare è la chiamata e il chunk rimandato.

### Generare una parte del mondo: `core.register_mapgen`

Uno **stage** è un pezzo della generazione del mondo scritto in Lua. Il motore
ne registra uno suo — `core:terrain` — e un mod ne aggiunge altri:

```luau
local ORE = core.register_node("miniera:ferro", 180, 120, 90)

core.register_mapgen("miniera:vene", function(ctx)
    -- ctx.seed, ctx.height, e la colonna da generare: ctx.min_x..ctx.max_x e
    -- ctx.min_z..ctx.max_z, estremi compresi, 32 per 32. Non c'è altro dentro,
    -- e non si scrive niente dentro: uno stage scrive con core.set_block e
    -- legge il terreno con core.terrain_height.
    for z = ctx.min_z, ctx.max_z, 8 do
        for x = ctx.min_x, ctx.max_x, 8 do
            local suolo = core.terrain_height(x, z)
            if suolo > 12 then
                core.set_block(x, suolo - 6, z, ORE)
            end
        end
    end
end)
```

**Il mondo non ha bordo, e uno stage gira una volta per colonna** di chunk
(32×32 blocchi, tutta l'altezza), quando qualcuno ci si avvicina per la prima
volta. Quello che scrive fuori dalla sua colonna viene **scartato** senza
errore: una struttura a cavallo di due colonne la scrivono tutte e due, ognuna
la sua metà, ed è l'unico modo perché venga intera in qualunque ordine le due
colonne vengano generate. Quindi una struttura che parte da una colonna vicina
(una vena, una galleria, la chioma di un albero) va *seguita* anche da qui:
`mods/ore` guarda gli inizi di vena fino a quattro colonne prima, `mods/cave`
le gallerie dei quadrati fino a quattro più in là. E `core.get_block` fuori
dalla colonna legge quello che c'è adesso, che può essere niente: uno stage
decide da quello che c'è **dentro** la colonna, o da funzioni del seme.

Quattro regole, e sono quello che rende uno stage diverso da un mod che scrive
blocchi al caricamento:

* **L'ordine di registrazione è il contratto.** Gli stage girano nell'ordine in
  cui sono stati chiesti — quindi nell'ordine di caricamento dei mod, che
  `depends` decide — e uno stage vede quello che hanno scritto quelli prima di
  lui.
* **Si registra mentre il mod si carica, non dopo.** Gli stage girano su ogni
  colonna nuova, per tutta la vita del server — sulla regione attorno allo spawn
  subito dopo il caricamento dei mod, sul resto quando serve — ma la lista si
  chiude quando i mod hanno finito di caricarsi: chiederne uno da `on_step` viene
  **rifiutato** con un errore, invece di essere accettato e girare solo sulle
  colonne generate da lì in poi.
* **Deve essere deterministico.** Stesso seme, stesso mondo, su qualunque
  macchina. Niente orologi, niente `math.random` senza seme, niente che dipenda
  da chi è connesso: un server e un client che non sono d'accordo su com'è il
  terreno sono un desync prima del primo pacchetto.
* **`core.set_block` dentro uno stage non costa l'illuminazione.** Il mondo
  viene illuminato una volta sola alla fine della passata, e nessun chunk va
  rimandato a nessuno: è il posto dove scrivere decine di migliaia di celle.

Uno stage che va in errore ferma **il suo** mod e nessun altro: quello che aveva
già scritto resta nel mondo, gli stage registrati dopo girano lo stesso, e il
log dice che è stato lo stage.

`mods/ore` è questo meccanismo in produzione e non un esempio: registra uno
stage che sotterra carbone e ferro nella pietra, il ferro a livello 1 — cioè
dietro il piccone di `mods/tools`. Vale la pena leggerlo per due cose che
l'esempio qui sopra non mostra: l'hash deterministico scritto a mano (un double
tiene 53 bit esatti e il prodotto di due numeri a 32 bit ne fa 64, quindi
`a * b` butta via proprio i bit bassi che servono a un hash) e la regola che
sostituisce **solo** le celle che sono `core:stone`, che è quello che tiene lo
stage fuori dalla superficie, dal filler del bioma, dalla bedrock e da quello
che ha costruito uno stage registrato prima.

`mods/cave` è il secondo, e mostra la cosa che l'ordine dei mod decide davvero:
la cartella dei mod si carica in ordine di nome, gli stage girano nell'ordine in
cui sono stati chiesti, e `cave` viene prima di `ore`. Quindi le gallerie si
scavano e *poi* il minerale riempie la pietra rimasta — ed è così che una vena
finisce nella **parete** di una caverna invece che dentro un blocco che nessuno
guarderà mai. Invertire i due nomi cambia il gioco senza cambiare una riga di
codice: è il contratto di cui parla questa sezione, visto da fuori.

Le altre due regole che vale la pena copiare da lì: una caverna per quadrato di
32×32 colonne, scelta *dentro* il quadrato invece che «una colonna su N», perché
un mondo piccolo con un hash sfortunato sarebbe un mondo senza caverne e un
gate che passa solo su certi semi; e il tetto tenuto sei blocchi sotto la
superficie, controllato a ogni passo contro `core.terrain_height` della colonna
in cui la galleria si trova adesso e non di quella da cui è partita.

`games/backrooms` è il terzo, e un mondo intero fatto solo di stage
(`core.mapgen_core(false)`): una griglia di stanze di 8 che sta tutta dentro una
colonna di 32, così ogni stanza si scrive per intero e nessuna struttura va
seguita da una colonna vicina. Due cose da copiare. La pianta è funzione di x e
z e **non del seme**, e per questo anche il tick la può leggere: trova dove sono
i portali senza aver visto girare lo stage, che in un mondo caricato dal file
non gira. E quello che attraversa più livelli (le scale, i buchi nel pavimento)
si scrive **dopo** tutti i livelli della colonna: `core.replace_nodes(…, AIR,
pavimento)` di un livello fatto dopo richiuderebbe il buco aperto prima.

Una colonna già generata e poi lasciata (nessuno ci passa da un po') non viene
rigenerata: torna dal file del mondo, o dalla memoria se il server non ne ha
uno. Uno stage cambiato fra due avvii cambia solo le colonne che nessuno ha
ancora visitato, e il confine con quelle vecchie si vede. Un mod che scrive con
`core.set_block` in una colonna non ancora pronta non perde la scrittura: aspetta
che la colonna sia generata e illuminata, e poi va. `core.get_block` in una
colonna che non c'è risponde aria. TODO.md OPEN-35 e OPEN-38 hanno i numeri.

### Come si muove un corpo: `core.movement`

```luau
-- camminata, sprint, volo, salto, gravità
core.movement(12, 60, 20, 10, -40)
```

Un mondo dice quanto veloce cammina, corre, vola e salta un corpo, e quanto forte
cade. È **del mondo** e non del giocatore: ogni client che entra riceve questi
numeri nel welcome e si muove con quelli, e il server giudica le posizioni con
limiti ricavati dagli stessi numeri. Quindi un corpo a 60 m/s non è un cheat in un
mondo che l'ha dichiarato, e uno a 150 sì.

* **Mentre il mod si carica.** Da un tick è troppo tardi e viene rifiutato: i
  numeri escono nell'handshake, e un mondo che ne cambiasse a metà avrebbe
  giocatori a cui ne sono stati detti altri.
* **Rifiutato, con il motivo nel log:** una velocità negativa, non numerica o
  sopra i **256 m/s**; una gravità non numerica o **verso l'alto**. Restituisce 1
  se i numeri sono stati presi, 0 se no.
* **Il volo raddoppia con lo sprint**, come con i numeri del motore.
* **La velocità terminale, 60 m/s, non si dichiara**: è quello che ferma una
  caduta qualunque sia la gravità, anche sul client.
* **Se due mod lo chiamano vince l'ultimo caricato**, e il log dice di chi ha
  sostituito i numeri.
* **Un client connesso usa i numeri del server**, anche se un suo mod ne dichiara
  altri: il server è il lato che controlla.

Quello che **non** c'è ancora: velocità per corpo — un veicolo è un corpo che non
è un giocatore a piedi — e un server che simula il movimento invece di
verificarne la fine. TODO.md OPEN-44.

### Quanto è grande un corpo: `core.body_width`

```luau
core.body_width(1.4, 2.6)   -- un kart: largo 1.4, lungo 2.6
```

A piedi, il lato del box quadrato con cui un corpo urta, in nodi: 0.6 se il mondo
non dice niente. L'altezza resta quella del motore, 1.8. Il secondo numero conta
solo in un mondo che guida (`core.drive`): lì il corpo è un **telaio su quattro
ruote con le sospensioni**, largo `width` e lungo `length` (0: lungo quanto è
largo), simulato come corpo rigido (Jolt, `core/physics/vehicle.h`). Gira col
kart, beccheggia in frenata, si inclina in curva, e se entra troppo veloce in una
curva le gomme non tengono e scivola largo. Il telaio resta entro 26 gradi dalla
verticale e le sue ruote non prendono come terreno niente di più ripido di 50
gradi: un kart che striscia un muro non ci si corica sopra e non lo scala.

* **Mentre il mod si carica**, come `core.movement`: esce nello stesso welcome, e
  `core.movement` e `core.body_movement` la tengono.
* **Il server controlla con un box quadrato**: largo `width` a piedi; per un corpo
  che guida, il quadrato che sta dentro al telaio comunque sia girato,
  `width / 1.41`. Chi esce da un portale viene spinto fuori dal muro accanto di
  mezza larghezza, o di mezza lunghezza se guida.
* **Stare DENTRO un blocco** è l'unica domanda che fa in modo diverso: a piedi la
  fa con quel box, guidando **col solo centro**. Un telaio gira, si inclina e ha
  le ruote fuori dal suo box, quindi i due capi non concordano su cosa tocca, e
  un rifiuto che rimette il kart dove il box del server è libero lascia il telaio
  dentro il muro che stava strisciando — e il server lo rifiuta di nuovo, a ogni
  claim (TODO.md). Un kart può appoggiarsi a un muro; uno col centro dentro viene
  ancora rimesso a posto.
* **Rifiutato:** una larghezza sotto 0.2 o sopra 4, una lunghezza diversa da 0
  sotto 0.2 o sopra 8, o un numero che non è un numero.
* **Un corpo più largo di un portale nel pavimento non ci passa**: il server esenta
  solo il nodo dell'apertura, e il box largo affonda anche nei nodi accanto.

**Chi non vola non sale.** In un mondo che guida (`core.drive`), o che dichiara
volo 0, il server rifiuta un corpo che non poggia su niente ed è più in alto di
dove ha poggiato l'ultima volta di più di un salto (v² / 2g, più 0.3). Un salto,
un gradino e un atterraggio su un blocco più alto passano; una salita a velocità
di salto sopra i due nodi dei muri della pista no.

### Un corpo diverso dagli altri: `core.body_movement`

```luau
core.event.on("core:net_message", function(ev)
    if ev.text == "sali in macchina" then
        -- giocatore, camminata, sprint, volo, salto, gravità
        core.body_movement(ev.player, 12, 150, 20, 10, -40)
    end
end)
```

`core.movement` dice come si muovono i corpi del mondo; questa dice come si muove
**un** corpo, e si può chiamare quando si vuole: un veicolo, un potenziamento, un
prigioniero rallentato. Il giocatore riceve i numeri in un messaggio suo, e da quel
momento il server controlla quel corpo con quelli.

* **Un secondo di margine.** Il client continua a muoversi con i numeri vecchi
  finché il messaggio non arriva, quindi per un secondo il server accetta il più
  veloce fra i vecchi e i nuovi: rallentare qualcuno non corregge i passi che
  aveva già fatto. Il prezzo, detto: un corpo rallentato ha un secondo in cui può
  ancora andare alla velocità di prima.
* **I default sono i numeri del motore, non quelli del mondo.** Per rimettere un
  corpo come il mondo l'ha dichiarato, si passano quelli.
* **Rifiutato, con il motivo nel log:** i numeri che `core.movement` rifiuta, un
  giocatore che non è nel mondo, e chi sta solo guardando.
* **Quanto in alto, il settimo numero (`rise`).** Senza, un corpo che non vola
  può stare al massimo la cima del suo salto sopra dove poggiava (per un kart,
  più lo stacco del telaio dalle ruote). Con, quello che dice il mondo, fino a
  64: la pista passa 10, e un kart che ci arriva rimbalza come contro un
  soffitto. Chi deve restare dentro un percorso lo decide il mod, non l'altezza:
  `games/track` riporta sulla strada più vicina un kart che ne è fuori da mezzo
  secondo (`core.player_place`).

Quello che **non** c'è ancora: il server non controlla sterzo e inerzia. E i numeri
di un corpo non attraversano un portale verso un altro mondo, dove vale il welcome
di quel mondo, e non si salvano. TODO.md OPEN-47.

### Uno solo sale in kart: `core.body_drive` e `core.body_model`

```luau
core.event.on("core:used", function(ev)
    if ev.id ~= KART then return end
    ev.cancel = true
    core.set_block(ev.x, ev.y, ev.z, 0)                       -- il kart ora è lui
    core.body_drive(ev.player, 12, 30, 2.6, 0.6, 1.4, 2.6)    -- accel, freno, sterzo, attrito, largo, lungo
    core.body_model(ev.player, KART_MODEL)                    -- e lo vedono così
end)
```

`core.drive` e `core.body_width` dicono come guida **il mondo**, e vanno chiamate
mentre i mod caricano; `core.body_drive` dice come guida **un corpo**, quando si
vuole, e `accel` a 0 lo rimette a piedi (passando anche la larghezza di prima).
`core.eye_height(h)` dice a che altezza sta l'occhio sopra i piedi (1.62 se il
mondo non dice niente): la telecamera in prima persona e il punto da cui si
misura la portata della mano, sul client e sul server. Si chiama mentre i mod
caricano, come `core.movement`, e va nel welcome (protocollo 50). Da 0.2 a 1.8:
il box del corpo resta alto 1.8. `mods/robot` dice 1.5, la visiera del robot.

`core.parkour(true)` dice che in questo mondo i corpi a piedi fanno parkour
(`core/physics/player.cpp`), e va nel welcome come il resto (protocollo 87):

* **corsa sul muro**: in aria vicino al culmine del salto, avanti tenuto a 4 m/s
  o più, con un muro accanto (a destra o a sinistra della direzione in cui si
  va), ci si corre sopra in piano, senza gravità, per al più 5 nodi;
* **salto dal muro**: salto premuto in aria con un muro entro 0.45 si spinge via
  dal muro, a 5.5 m/s e in su col salto del mondo; mai due volte di fila dallo
  stesso lato, così fra due muri a 2 si sale rimbalzando e su uno solo no;
* **scivolata**: crouch premuto correndo (oltre il passo) mette il corpo sulla
  schiena, alto 0.8, a 1.15 volte lo sprint che rallenta, per 0.9 s: passa sotto
  un blocco alto 1; sotto un soffitto basso resta giù e va al passo accovacciato;
* **capriola all'indietro**: salto con indietro tenuto, da terra, sale di 2.2
  nodi andando indietro a 3 m/s: su un gradino alto 2 alle spalle;
* **corda**: `core.rope(true)`, dal codice del mondo sul client, spara la corda
  dall'occhio fino a 40 nodi; il corpo oscilla attorno al punto dove si aggancia
  (le mani restano a distanza di corda), i tasti spingono l'oscillazione,
  crouch allunga, sprint accorcia, salto o `core.rope(false)` stacca.

In un mondo così le mani **non** si aggrappano alle sporgenze (lì un muro alto 2
è una capriola e uno alto è un camino), e il server permette quello che queste
mosse fanno: 20 m/s di oscillazione, qualunque altezza, un corpo scivolato
sotto un blocco (`limits_for`). Il robot ha le clip per ognuna: `wall_run_left`,
`wall_run_right`, `wall_jump`, `power_slide`, `backflip`, `rope_swing`.
`games/parkour` è il mondo fatto per questo.

`core.body_model` è `core.player_model` per un corpo solo. È così che si fa un
mondo in cui si arriva a piedi e si sale su un kart parcheggiato: `games/track`.

* **I numeri di velocità restano suoi.** `core.body_drive` cambia cosa *è* il
  corpo, non quanto va: `sprint` è la velocità massima e `walk` quella in
  retromarcia, e si cambiano con `core.body_movement`, che a sua volta lascia
  stare il kart (un conto alla rovescia a zero non fa scendere nessuno).
* **Salire alza il corpo di 1.07** — un telaio sta sulle sue molle — quindi un
  fermo che azzera anche il salto si fa correggere dal server: lascia il salto
  del mondo.
* **Quello che vede il giocatore del proprio corpo** con la telecamera dietro
  (`core.view`) è il modello di `core.player_model`: il welcome ne porta uno
  solo. Gli altri vedono quello di `core.body_model`. Il codice del mondo che
  gira sul client può chiamare `core.body_model(0, id)` per cambiare come
  **quella macchina** disegna il proprio corpo (0 lo rimette com'era):
  `games/track` ci mette il kart mentre si guida.
* **Ruote che girano e sterzano.** Un modello con uno scheletro le anima da
  solo, per nome degli ossi: `steer_*` sterza attorno al proprio asse,
  `wheel_*` rotola attorno al proprio (figlio di uno `steer_*`, sterza anche),
  `steering_wheel` gira di un quarto di giro a fondo corsa. "Il proprio asse" è
  la Y dell'osso, cioè dove punta un osso di Blender. Il raggio di una ruota è
  quanto sta alto il suo centro sul fondo del modello. Sterzo e metri percorsi
  il client li legge da come il corpo si muove: niente da dichiarare, e vale
  anche per gli altri giocatori. Un blocco che indossa il modello resta fermo.
* **Clip per nome.** Un modello con delle animazioni ne suona una scelta dal
  movimento del corpo, con un quinto di secondo di dissolvenza tra una e
  l'altra: `idle`, `walk` (fino a 5.5 m/s), `run`,
  `jump_loop` (in aria), `swim`/`swim_idle` (in un liquido), `slide` (su un
  nastro in discesa da 3 m/s in su: uno scivolo) e, per il proprio
  corpo, `fly`/`fly_hover` e mentre si scava `mine`, `dig` o `chop` secondo il
  materiale del blocco (`stone`, `soil`, `wood`). Una clip che il modello non
  ha lascia quella di prima.
* **Chi guida.** In un corpo che indossa un modello con ruote (`wheel_*`) il
  client siede il modello di `core.player_model`, alla stessa origine: suona
  `kart_enter` una volta, poi `drive_idle` piegato verso `drive_left` o
  `drive_right` quanto è girato lo sterzo. Le clip di chi guida vanno quindi
  fatte attorno all'origine del veicolo. `mods/robot` e il kart di
  `games/track` sono fatti così.
* **Pezzi del veicolo.** Un pezzo che una mod fa indossare a chi guida
  (`core.player_wear`) e che si chiama come il veicolo più `_<parte>_<variante>`
  (`track_core:kart_wheel_slick` su `track_core:kart`) va sulle ossa del
  veicolo, non di chi ci siede, e le ossa del veicolo che si chiamano `<parte>`
  o `<parte>_*` non si disegnano: il pezzo le sostituisce (le quattro
  `wheel_*`). Il veicolo stesso indossato come pezzo lo vernicia: il suo tint
  colora i `tint_1..3` del veicolo. Le fiamme del nitro escono dai giunti
  `flame_*` (lungo l'osso) dei pezzi che ne hanno, se no da quelli del
  veicolo; le sgommate dalle ossa `wheel_R*`, sulla strada sotto di loro (su
  un pezzo di pista, la sua superficie).
* **Anteprima in un pannello.** `core.ui_model(pannello, x, y, w, h, modello,
  tint, pezzi)` mette in un riquadro del pannello un modello che gira piano,
  inquadrato dalla sua misura, con i suoi colori e i pezzi (id separati da
  spazi) addosso come sopra: il garage di `games/track` mostra così il kart
  scelto. Non risponde ai click (protocollo 104). `tools/make_kart_parts.py` fa il
  kart diviso in parti (`nose`, `pods`, `pipes`) e i suoi 14 pezzi.
* **Braccia in prima persona.** Un modello registrato col nome del modello
  del giocatore più `_arms` (`robot:robot` → `robot:robot_arms`) viene
  disegnato davanti all'occhio quando la vista è in prima persona. È fatto
  nello spazio dell'occhio: origine all'occhio, avanti −Z, su +Y. Suona
  `swing` finché il tasto sinistro è giù e `idle` altrimenti, e ondeggia
  mentre si cammina. Un blocco in mano è un cubo delle sue facce nel palmo
  dell'osso `hand_R`; un pezzo di pista (`core.node_piece`) è il pezzo in
  miniatura, la sua strada col lato più lungo di 0.24 avambracci; per terra, lo
  stesso pezzo in miniatura. Vale anche nel mondo locale.
* **Vetri.** Un materiale con l'alpha del colore base sotto 1
  (`baseColorFactor`, in Blender l'Alpha del Principled BSDF) si vede
  attraverso: è disegnato dopo il resto del modello, mescolato e senza
  scrivere la profondità (i vetri dei mezzi del fronte, 0.3). L'alpha della
  texture invece ritaglia, sotto 0.5 il pixel non c'è.
* **Colori e accessori del giocatore.** I materiali `tint_1`, `tint_2`,
  `tint_3` del modello del giocatore (e delle sue `_arms`) prendono il
  primario, secondario e accento scelti nel menù Robot (protocollo 93,
  `Hello::livery`); senza scelta restano del colore del file. Gli accessori
  sono modelli col nome del giocatore più `_cap`, `_tophat`, `_headphones`,
  `_scarf`, `_bowtie`, `_medal` (`robot:robot_cap`), fatti sullo scheletro
  del giocatore e disegnati come un pezzo indossato; un mondo che non li
  registra semplicemente non li mostra. `tools/make_robot_extras.py` li fa.
  La specie (protocollo 94, due bit sopra gli accessori: umano, uomo talpa,
  uomo rana) sostituisce il modello del giocatore con quello col suo nome più
  `_human`, `_mole`, `_frog` (`robot:robot_mole`, con le sue `_arms`), sullo
  stesso scheletro e con le stesse clip; lo stile scelto è un pezzo indossato
  di nome `<specie>_<stile>` (`robot:robot_mole_lamp`). Gli accessori sono
  quelli rifatti per la specie (`robot:robot_mole_cap`) se il mondo li
  registra, se no quelli del giocatore; un accessorio in testa sostituisce gli
  stili che sono cappelli (lampada, corona, ninfea). Un mondo che non registra
  la specie mostra il robot.
  `tools/make_species.py` li fa tutti: ogni pezzo del corpo (testa, busto,
  braccio, gamba) è un'unica superficie, primitive fuse con un remesh a voxel,
  levigate e sfoltite, con pesi sfumati alle giunture; occhi e piccoli dettagli
  restano netti. Un osso `sway_*` oscilla solo in avanti, via dal petto.
  In un pezzo indossato, un osso che il giocatore non ha segue il suo
  genitore, e se si chiama `sway_*` dondola anche attorno a dove è legato.
* **Attrezzi in mano.** Un osso `tool_<nome>` si vede solo quando in mano c'è
  un oggetto che si chiama `<mod>:<nome>` (`tool_pick` per `tools:pick`), e gli
  altri `tool_*` spariscono: sulle braccia e sul proprio corpo in terza
  persona, e sugli altri giocatori (dal protocollo 48 il server manda a tutti
  cosa ha in mano ognuno).
* **Oggetti in mano.** Se il mondo ha un modello che si chiama come l'oggetto
  più `_held` (`core.register_model("ore:diamond_held", "diamond_held.gltf")`),
  chi tiene quell'oggetto lo ha nel palmo di `hand_R` al posto del cubo: un
  glTF rigido, senza scheletro, fatto attorno alla presa (origine al centro
  del pugno, +Y dove punta un attrezzo, cioè in su in prima persona, +Z verso
  l'occhio in prima persona: una cosa piatta sta nel piano XY), in metri del
  robot da 1.357 e scalato con l'avambraccio come il blocco. Vale sulle
  braccia, sul proprio corpo e sugli altri giocatori.
  `tools/make_held_models.py` fa i 41 dei mod del progetto e le icone di
  quelli che non ne avevano.
* **Armature indossate.** Un oggetto con `wear` (`core.register_item`) va in
  uno dei quattro slot indossati della borsa (testa, petto, gambe, piedi; lo
  shift-click a borsa aperta lo mette e lo toglie). Se il mondo ha un modello
  con lo stesso nome dell'oggetto (`core.register_model("armor:iron_helmet",
  ...)`), il corpo lo porta: un glTF skinnato su ossa con gli stessi nomi di
  quelle del giocatore, disegnato con la sua posa. Dal protocollo 58 il server
  manda a tutti cosa indossa ognuno. `tools/make_armor_models.py` fa i sedici
  di `mods/armor` sul rig del robot.
* **Accovacciato.** Ctrl tenuto a piedi (in volo resta "scendi"): il passo
  a 0.35 del `walk`, niente scatto, l'occhio a 0.8 della sua altezza. Il
  corpo suona `crouch_idle` o `crouch_walk`, e dal protocollo 51 lo vedono
  anche gli altri.
* **Chi lavora.** Col tasto sinistro giù il corpo suona la clip del suo
  attrezzo: `mine` col piccone, `dig` con la pala, `chop` con l'ascia,
  `sword_slash` con la spada; a mani nude quella del materiale del blocco. Dal
  protocollo 49 lo vedono anche gli altri.

### Un'entità che vedono tutti: `core.ecs.visible`

```luau
local cartello = core.ecs.spawn("mymod:cartello", 32.5, 22.5, 30.5)
core.ecs.visible(cartello, true)    -- da qui ogni giocatore vicino la riceve
core.ecs.visible(cartello, false)   -- e da qui non più
```

Un'entità di un mod è nel mondo del server e basta, finché il mod non chiede che
la vedano gli altri. Da lì viene mandata a ogni giocatore vicino dalla stessa
strada che manda persone, creature e oggetti a terra, **come cosa**: il client la
disegna come una scatola sua, non come una persona né come una creatura, perché
il motore non sa cosa sia.

* **Solo le entità di un mod.** Un giocatore, una creatura o un oggetto a terra
  hanno già un corpo che gli ha dato il motore, e `visible` li rifiuta in
  entrambe le direzioni: un mod non può nascondere un giocatore.
* **Nasconderla non è istantaneo per chi la stava guardando**: un client dimentica
  un corpo che non riceve più dopo cinque secondi di silenzio.
* **Un modello**: `core.ecs.model(id, core.register_model(...))` la disegna col
  modello invece che con la scatola (0 torna alla scatola), i piedi alla sua y.
  Solo su un'entità resa visibile da `visible`.
* **Salute**: `core.ecs.health(id, 3)` gliene dà 3, e da lì un colpo di un
  giocatore gliene toglie quanto toglie a una creatura; `core.ecs.health(id)` la
  legge. A 0 resta dov'è: cosa voglia dire morire lo decide il mod.
* **Una clip**: `core.ecs.clip(id, "shoot")` fa suonare quella clip del suo
  modello al posto di quella che il motore sceglie dal movimento (idle, walk,
  run secondo la velocità); `core.ecs.clip(id, "")` la ridà al movimento.
  `"death"` va una volta e resta ferma sull'ultima posa: si chiama, si aspetta
  la sua durata, poi `core.ecs.kill`. Viaggia ai client (protocollo 88).
* **Quanto è grande per un colpo**: `core.ecs.box(id, 1.1, 3.2)` -- mezza
  larghezza attorno al centro, altezza dai piedi in su. Senza, un colpo la
  trova solo in un cubetto di 0.6 ai suoi piedi.
* **Dove guarda**: `core.ecs.set(id, "yaw", a)` la gira come lo sguardo di un
  giocatore (0 guarda verso -z); `core.ecs.get(id, "yaw")` lo legge. "yaw" non
  può più essere il nome di un campo di un componente.
* **Non si scontra** con niente. TODO.md OPEN-20.

### Mezzi: `core.craft`

Un'entità di un mod resa visibile può diventare un **mezzo**: una jeep, un carro
armato, un elicottero, un aereo. Il motore la muove con Jolt (lo stesso del
kart, `core/physics/craft.h`), solo sul server, un passo fisso a 60 Hz; i
giocatori ci salgono come passeggeri di un corpo (`core.ride`) e chi guida è
chi il mod ascolta.

```luau
local id = core.ecs.spawn("fronte:mezzo", x, y, z)   -- i piedi: il fondo del modello
core.ecs.visible(id, true)
core.ecs.model(id, CARRO)
core.ecs.set(id, "yaw", 0)                            -- prima di make: dove guarda
core.craft.make(id, "tracked", 3000, 1.6, 0.6, 3.2, 4, 9, 0.9)
--               tipo, kg, mezza larghezza, mezza altezza, mezza lunghezza, m/s², m/s, rad/s
```

* **`core.craft.make(id, kind, mass, half_w, half_h, half_l, accel, top, turn)`**:
  `kind` è `"wheeled"` (quattro ruote motrici, le due davanti sterzano),
  `"tracked"` (due cingoli, si gira rallentando uno, da fermo sul posto),
  `"rotor"` (spinto lungo il suo su, inclinato verso dove deve andare) o `"wing"`
  (spinto lungo il muso, sostenuto da quanto va veloce: sotto il 45% di `top`
  stalla). Le misure sono lo scafo in nodi. Rifatto, ricomincia da fermo.
* **`core.craft.keys(id, forward, right, up, yaw)`**: quello che chiede chi
  guida, da -1 a 1, tenuto finché non si dice altro. Ruote e cingoli: gas e
  sterzo. Rotore: `forward` e `right` lo inclinano, `up` è il collettivo (0 sta
  sospeso), `yaw` lo gira. Ala: `up` muove la manetta (e lei resta dove l'ha
  lasciata), `forward` è la cloche in avanti (muso giù), `right` il rollio,
  `yaw` il timone. Chi gioca preme tasti sul suo client: il mod li manda con
  `core.net` e li passa qui.
* **`core.craft.pose(id)`**: una tabella `x y z yaw pitch roll vx vy vz speed
  grounded impact throttle`. `speed` è lungo il muso, `impact` il colpo più
  forte preso da quando lo si è chiesto l'ultima volta, in m/s (uno schianto),
  e chiederlo lo azzera.
* **`core.craft.push(id, vx, vy, vz)`**: aggiunto alla sua velocità: un'esplosione.
* **`core.craft.mount(id, on, x, y, z)`**: un'altra entità visibile portata su
  `on` a x, y, z nel suo riferimento (x a destra, y su, z avanti): una torretta
  su uno scafo, una canna sulla torretta. `on` 0 la lascia. Rimessa a posto
  ogni tick dopo i mezzi, chi porta prima di chi è portato.
* **`core.craft.aim(id, yaw, pitch)`**: quanto una cosa montata è girata su chi
  la porta e alzata; su una non montata, come guarda nel mondo (un mortaio).
* **`core.craft.seat(player, id, x, y, z)`**: il giocatore portato su
  quell'entità, al posto x, y, z nel suo riferimento, finché `core.ride(player,
  0)` non lo fa scendere o l'entità non sparisce. Da seduto guarda dove vuole
  (`core.player_look`) e il suo WASD non lo muove.

Ogni mezzo è un mondo Jolt suo con i blocchi attorno; il motore poi separa due
mezzi che si sovrappongono (in proporzione alla massa) e spinge fuori dallo
scafo chi è a piedi. Dal protocollo 100 lo scafo va ai client: il corpo di un
giocatore non ci entra, ci sta sopra e viene portato con lui. Il rollio di
un'entità va ai client dal protocollo 98.

**Sul client di chi guida**, `make`, `keys` e `pose` fanno girare il mezzo anche
lì, perché parta nel frame in cui si preme il tasto e non un andata e ritorno
dopo. `core.craft.make(id, ...)` vale solo per il mezzo su cui il giocatore è
seduto (con `core.craft.seat`) e torna `false` finché il seggiolino non è
arrivato: va richiamato finché non dice `true`. Da lì `keys` sono quelle che il
mod dà sul server (le stesse, calcolate allo stesso modo) e `pose` è dove il
mezzo sarà. Il motore lo disegna lì, ci mette sopra il giocatore e porta con lui
quello che ci sta sopra o dentro. Lo confronta con le pose del server: corregge
piano gli scarti piccoli e lo rimette dove dice il server se si allontana più di
3 nodi. Scendere dal mezzo ferma la previsione. Gli altri `core.craft` sul client
non fanno niente. Esempio: `drive_here` in `games/front/mods/front_game`.

### Codice che un mondo manda al client: `side`

```
# mod.conf
name = hud
side = both        # server | client | both; server se non c'è
```

```luau
if core.side() == "server" then
    core.event.on("core:net_message", function(ev) core.net.send(ev.player, "ciao") end)
else
    core.render.pass("hud:mirino", 100, function() core.render.box(0, 60, 0) end)
end
```

Un mod dice **dove gira**. `server` è ogni mod di prima: gira dove sta il mondo.
`client` il server non lo fa girare: lo impacchetta e lo manda ai giocatori come
media. `both` gira in tutti e due i posti, ed è quello che serve a un mod per
parlare con la sua altra metà sul proprio canale — il canale di un mod è il suo
nome, e il server lo dà ai mod che fa girare. `core.side()` dice a una metà quale
è: `"server"`, `"client"`, o `"both"` in un processo che è il server di se stesso.

Sulla macchina di un giocatore il codice di un mondo:

* **non scrive il mondo e non registra niente**: blocchi, forme, modelli, oggetti,
  ricette, stage, modificatori, borse, casse, pannelli, salute e movimento sono
  del server. La funzione c'è e non fa niente, o dà un errore che dice perché;
* **non ha nessuna capability**, nemmeno `os`: un mod che ne dichiara una non
  viene caricato, e il log la nomina;
* **non alza la propria quota** di memoria o il budget di interruzione sopra i
  default;
* **muore con la sessione**: attraversare un portale è entrare in un altro mondo,
  con il codice suo.

**Il giocatore può rifiutarlo**: `--world-code refuse` per tutti i mondi, o la
chiave di firma di un mondo, in esadecimale, una per riga in
`world_code_refused.txt` nella cache dei media. Un mondo rifiutato si gioca lo
stesso: il server resta l'autorità, quindi rifiutare non dà nessun vantaggio e
toglie solo quello che il mondo voleva mostrare.

Il codice di un mondo **disegna sullo schermo e sente i tasti**: `core.hud` e
`core:input`, sotto. Quello che **non** c'è ancora: un mod solo client che parli al
server senza una metà `both`. TODO.md OPEN-49.

### Sullo schermo e dai tasti: `core.hud` e `core:input`

```luau
core.hud.pass("gara:tempo", 100, function()
    core.hud.rect(0.5, 0.5, 0.026, 0.003, 223, 244, 255, 230)   -- mirino
    core.hud.text(0.5, 0.9, `{giri} / 3`, 0.045)
end)

core.event.on("core:input", function(ev)
    if ev.action == "mouse_left" then
        ev.cancel = true            -- il click è del mod: niente scavo, niente colpo
        core.net.send(0, "spara")
    end
end)
```

Un **hud pass** non è un render pass: quello gira una volta per camera, e un
portale sullo schermo disegnerebbe il tempo una volta per ogni mondo che mostra.
Questo gira **una volta per frame**, sopra la barra e sotto un pannello.
`x, y` sono frazioni dello schermo e sono **il centro** di quello che si disegna
(`align` 0 mette lì l'estremo sinistro del testo, 2 il destro); `w`, `h` e
`size` sono frazioni dell'**altezza**, così un quadrato resta quadrato a ogni
proporzione. Fuori da un pass `text`, `rect` e `image` sono rifiutate e lo dicono.

`core.hud.image(nome, x, y, w, h, a)` disegna un'immagine dei media di questo
mondo — un PNG fra le texture di un suo mod, per nome di file — con il centro in
`x, y` e le misure come `rect`; `a` è l'opacità, 0-255. Decodificata una volta
sola; un nome che il mondo non ha è detto nel log una volta e disegna niente.
Il server manda come media solo le texture dei **blocchi e degli oggetti
registrati**: un PNG che è solo un disegno per l'HUD va registrato come
oggetto che nessuno riceve (`core.register_item`), e si chiama col nome del
suo file. `games/track` fa così con le icone dei power-up e la macchia
dell'inchiostro (`track_core:item_*`, `track_core_item_boost.png`). Della
misura delle altre texture del mondo (32 px): l'atlante ne prende una sola, e
un'immagine di un'altra misura è un quadrato rosso nella borsa.

`core.hud.item(nome, x, y, lato)` disegna il quadrato di un oggetto come lo
disegna la borsa (un pezzo di pista come la sua pianta), centro in `x, y` e
lato in frazioni dell'altezza; false per un oggetto che il mondo non ha.
`core.hud.mouse()` dice dov'è il cursore, in frazioni dello schermo come le
`x, y` di `text` (niente dove non c'è schermo): con la camera di
`core.fixed_view` il cursore è libero e i click arrivano comunque a
`core:input`, così un mod ci fa una griglia da cliccare (l'editor della pista).

`core.hud.aspect()` è larghezza diviso altezza dello schermo (0 dove non c'è
schermo). Serve a mettere qualcosa **allineato alle barre del motore**, le cui
misure sono frazioni dell'altezza: la barra della fame di `mods/hunger` è larga
quanto la hotbar, e il centro di ogni tacca va in `0.5 + dx / aspect`. Le misure
del motore sono in `src/client/ui/hud.cpp` (uno slot è 0.075 dell'altezza, lo
spazio fra due 0.0075, una barra alta 0.0165).

`core.view(distanza)` mette la camera di questo mondo **dietro** il giocatore, a
quella distanza in nodi (0 è l'occhio, al massimo 16), con il suo corpo disegnato
nel modello di `core.player_model`. Vale per il mondo che la chiede: tornare in
un mondo che non ha detto niente riporta all'occhio. Il tasto V fa lo stesso a
mano. `games/track` la chiama con 5.

`core.engine_hud_on()` dice se la barra e le barre del motore sono disegnate in
questo mondo: un mod che disegna una barra accanto a loro (`mods/hunger`,
`mods/enchant`) la nasconde con loro quando il codice di un mondo ha detto
`core.engine_hud(false)`.

`core.fixed_view(x, y, z, distanza, yaw, pitch, fov, secondi)` è la camera di
`core.iso_view` tenuta su **un punto** invece che sulla testa: vola dall'occhio fin
lì in `secondi`, il cursore è libero (`core.cursor_ray`), il corpo sta fermo (i
tasti non camminano) e non si vede; `distanza` 0 la riporta all'occhio. È la
vista di `mods/chess` sopra la scacchiera.

`core:input` arriva quando il giocatore **preme** e quando **lascia**, mentre
gioca: `mouse_left`, `mouse_right`, e i tasti `key_w`, `key_a`, `key_s`, `key_d`,
`key_space`, `key_shift`, `key_ctrl`, `key_f`, `key_r`, `key_e`, `key_q`, `key_v`,
`key_c`, `key_b`, `key_f4`, `key_f8`, `key_i`, `key_k`, `key_1` … `key_9`; il rilascio è lo stesso nome con `_up` (`key_w_up`,
`mouse_left_up`). Con una chest aperta sopra la borsa arrivano anche `wheel_up` e
`wheel_down`, la rotella (`mods/creative` scorre le righe). Un tasto tenuto è quello premuto e non ancora lasciato. Un
handler che mette `ev.cancel = true` su una **pressione** del mouse l'ha
**reclamata**: il motore non scava, non colpisce, non piazza e non usa con quella
pressione, finché il tasto non torna su. I tasti si sentono e basta: il motore
cammina, vola e apre la borsa con i suoi comunque (`mods/creative` apre la sua
lista con C). Fanno eccezione `key_1` … `key_9`: reclamati, non cambiano
l'oggetto in mano (il fronte li usa per i posti dei mezzi). Solo sul client: il server non vede tasti,
vede quello che la metà client di un mod ha deciso di mandare — quindi un mod
`both` ascolta `core:input` solo dal lato che non è `"server"`.

### Un colpo deciso dal server: `core.player_look` e `core.raycast`

```luau
local x, y, z = core.player_pos(player)
local dx, dy, dz = core.player_look(player)
local _, _, _, muro = core.raycast(x, y + 1.62, z, dx, dy, dz, 60)
```

`player_look` è dove guarda un giocatore **secondo il server**: lo sguardo
arrivato con l'ultima posizione accettata. Con `player_pos` è un colpo che il
server lancia senza prendere la mira dal client — un client può mentire solo su
*quando* ha premuto. `raycast` restituisce il primo blocco solido lungo il raggio
e a che distanza, entro `max` (al massimo 256), o niente. **Solo blocchi**.

Un proiettile che colpisce **i corpi** è `core.shoot`: il primo corpo che si può
ferire (una creatura, o un'entità di un mod con `core.ecs.health`) prima del primo
blocco solido perde `damage` e viene respinto; torna a che distanza e se è morto.
`core.blast` è un'esplosione: tutti i corpi entro `r`, metà danno al bordo, e
torna quanti ne ha uccisi. Nessuno dei due tocca mai un giocatore.

```luau
local dist, morto = core.shoot(x, y + 1.5, z, dx, dy, dz, 80, 6)   -- niente se ha mancato
local uccisi = core.blast(x, y, z, 7, 140)
core.mob_sight("mymod:imp", 64)   -- da quanto lontano un tipo vede chi inseguire (16)
```

Sul client, `core.arms_clip("fire_shotgun")` suona una volta, dall'inizio, una clip
delle braccia in prima persona (il modello `<giocatore>_arms`), anche se sta già
suonando, e poi le lascia al motore: lo sparo di un'arma, un'arma che si alza.
`core.held_item` sul client risponde per il giocatore di questa macchina (id 1).

Le braccia a riposo suonano `idle_<oggetto>` quando ce l'hanno (`idle_shotgun`
con `yafps_game:shotgun` in mano), altrimenti `idle`: ogni arma si tiene a modo suo.

`games/yafps` è l'esempio intero: una base a più stanze in stile arena shooter moderno con
pistola, fucile a pompa, doppietta, mitragliatrice e plasma cannon (ogni arma un osso
`tool_<arma>` sotto la mano destra delle braccia del robot, le mani messe sulle
impugnature con l'IK in `tools/make_fps_models.py`), ondate di demoni fatti con
`core.register_mob`, munizioni e HUD.

### Che cosa esiste: `core.item_count`, `core.item_name`, `core.item_stack`

```luau
for id = 1, core.item_count() - 1 do
    local name = core.item_name(id)
    if name ~= "" then
        print(name, core.item_stack(id))   -- quanti ne stanno in uno slot
    end
end
```

`core.item_id` risponde su un nome che il mod **ha già**; queste tre rispondono
nell'altro senso, cioè "che cosa c'è in questo mondo". Servono a un inventario
creativo, a una casella di ricerca, a un mod che elenca quello che un mondo
tiene — cose che da Lua non si potevano nemmeno chiedere.

`item_name` risponde **vuoto** per un id che nessuno ha registrato e per un
**orfano**, cioè un item il cui mod è andato via: l'id resta (una pila in un
baule continua a valere quello che valeva) ma a nessuno si può più dare, quindi
camminare da 1 a `item_count` saltando i vuoti dà esattamente gli item che un
giocatore può ricevere. `mods/creative` è l'esempio: un pannello con un pulsante
per item, e un click dà una pila piena — `item_stack` e non 64, altrimenti ogni
attrezzo sarebbe sbagliato.

### Contenitori, cinnabar e altre cose aggiunte il 2026-09-25

Riassunto per chi scrive un mod; firme e default in [API.md](API.md).

**Contenitori fra loro.** `core.container_fill(x, y, z)` dice quanto è pieno un
contenitore, 0..1, come lo legge un comparatore (ogni slot vale la sua pila
diviso quanto ne sta in una pila), -1 se lì non c'è. `core.container_revision`
è un numero che cambia a ogni modifica del contenuto, a mano o da un mod: un
comparatore lo guarda ogni 0,25 s. `core.container_move(da, a, quanti)` sposta
la prima pila da un contenitore a un altro **così com'è** (usura e incantesimi
compresi), e `core.collect_drops(cella, contenitore)` mette dentro un
contenitore quello che giace in una cella: sono le due metà di un hopper
(`mods/cinnabar`).

**Osservare una cella.** `core.watch_cell(x, y, z, on)` chiede di sapere quando
**qualunque cosa** scrive quella cella: un mod (acqua che scorre, grano che
cresce), un giocatore che piazza o rompe, il motore (una porta aperta da una
creatura). Si sente come `core:watched`. Le osservazioni non si salvano: un
observer le richiede con un LBM quando il suo chunk si carica.

```luau
core.register_lbm("mymod:rewatch", { "mymod:sensor" }, function(cell)
    core.watch_cell(cell.x - 1, cell.y, cell.z, true)
end)
core.event.on("core:watched", function(ev) --[[ la cella è cambiata ]] end)
```

**Spostare corpi.** `core.push_mobs(x, y, z, dx, dy, dz)` sposta le creature coi
piedi in quella cella; un giocatore si sposta con `core.player_place` (il
pistone lo fa scivolare in quattro tick da 0,25).

**Blocchi.** `core.node_solid(id)` risponde 1 o 0 — confrontatelo con 1, perché
in Luau anche 0 è vero. Un blocco `wallmounted` guarda fuori dalla faccia contro
cui è stato messo, in sei direzioni: è così che pistoni, observer e hopper
puntano anche in su e in giù (+X, -X, +Y, -Y, +Z, -Z per le facce 0..5, e i box
si scrivono col davanti verso +Z). **L'immagine di un box si legge con la prima
riga in basso**: una torcia disegnata con la testa nelle prime righe esce a testa
in giù.

**Rinominare.** `core.node_alias(vecchio, nuovo)` fa caricare come `nuovo` le celle
salvate col nome vecchio, invece del segnaposto magenta; `core.item_alias` fa lo
stesso per le pile in borse e bauli. Si leggono solo quando si apre un mondo.

**Tempo, rinascita, rigenerazione.** `core.time_of_day()` (0 mezzanotte, 0.25
alba, 0.5 mezzogiorno, 0.75 tramonto) e `core.set_time(t)`, che tutti i client
sentono subito: il letto. `core.player_spawn(player, x, y, z, on)` dove quel
giocatore rinasce, finché è connesso. `core.player_regen(player, on)` ferma o
riattiva la salute che torna da sola: la fame.

**Creature.** `core.mob_drop(nome, oggetto, max)` è un secondo drop, 0..max: la
pelle della mucca. `core.monsters_near(x, y, z, r)` conta le creature che
combattono (non `"pet"`) entro r: il letto non fa dormire se ce n'è una entro 8.

**Luce.** `core.light(x, y, z)` è quanto è illuminata una cella adesso, 0..15: la
luce dei blocchi o quella del cielo per l'ora (piena a mezzogiorno, un quinto a
mezzanotte, niente sotto `core.world_sky`). Il grano cresce da 9 in su.

**Seduti su un altro: `core.ride`.** `core.ride(player, host, x, y, z)` mette
`player` sul corpo di `host`, a x alla sua destra, y in su e z davanti (ognuno
entro 8), e ce lo tiene girando con lui: il passeggero di un kart. Il server lo
rimette al posto a ogni tick e non guarda le sue posizioni; lui guarda, scava,
costruisce e usa come sempre. `core.ride(player, 0)` lo fa scendere dov'è; scende
da solo se `host` se ne va. Solo sul server (protocollo 63).

**Girare, e sapere cosa c'è.** `core.node_turns(id)` dice quanti valori prende
`core.set_facing` per quel blocco: 1 senza fronte, 4 dritto, 6 a muro, 24 un
tronco; `mods/screwdriver` li scorre uno a clic. `core.loaded(x, y, z)` è 0 per una
cella la cui colonna non è in memoria, che `core.get_block` legge come aria.

**Incantesimi.** Una pila tiene fino a quattro incantesimi in un byte, due bit di
livello per tipo: 1 Efficienza (scava 1 + 0,5 per livello più in fretta), 2
Affilatezza (un punto in più per livello), 3 Indistruttibilità (si consuma un uso
su livello + 1), 4 Protezione (un punto di armatura per livello, indossata).
`core.enchant_held(player, livello, tipo)` mette un tipo sulla cosa in mano
(solo se è una cosa sola) lasciando gli altri; `core.held_enchant(player)`
risponde col byte: il livello del tipo k è `(v // 4^(k-1)) % 4`.

**Eventi fra mod.** Un mod dichiara un evento suo e un altro lo ascolta, senza
che nessuno dei due chiami l'altro: `mods/furnace` tiene l'esperienza della
fusione nei meta della fornace e la dà con `furnace:xp` a chi la apre;
`mods/enchant` (che dipende da `furnace`) la conta.

```luau
core.event.define("furnace:xp", { fields = { player = "number", points = "number" }, cancellable = false })
core.event.emit("furnace:xp", { player = ev.player, points = 3 })
```

### Pagine: `core.page_register` e `core.page_show`

Una pagina è una schermata stile pagina web, impaginata dal motore sul server
e disegnata con gli stessi elementi di un pannello: nessun messaggio nuovo sul
filo. Un mod la registra con un nome; quando un giocatore la apre (`/page
nome`, o un link `page:nome`) il mod riceve `core:page_open` e risponde con
`core.page_show`. Il markup, un costrutto per riga:

```
# Titolo                  una barra
## Sottotitolo            con una riga sotto
testo con [un link](page:altra) dentro, che va a capo da solo
| nome | ruolo | [kick](cmd:/kick bob) |      righe di tabella, colonne larghe quanto serve
?[scrivi qui](act:invia)                       un campo: Invio esegue la destinazione
> testo più quieto
---
```

Le destinazioni: `page:NOME` apre un'altra pagina, `cmd:/riga` esegue un
comando come il giocatore che ha cliccato (con i suoi permessi, come se
l'avesse scritto; dopo, la pagina si riapre aggiornata), `act:ID` arriva al mod
come `core:page_action`. In un campo `{text}` diventa quello che è stato
scritto. Oltre una schermata compaiono "back" e "next". Il testo è misurato con
le larghezze dei caratteri del font del client, quindi va a capo dove viene
disegnato. Nel client a processo singolo (`--quick`) le pagine dei mod ci sono
lo stesso, con `/page`. L'esempio completo è `mods/board`.

Un giocatore con `/mute` non parla agli altri neanche attraverso i mod: mentre
gira un suo comando, un'azione su una pagina o un click su un pannello, il
`core.chat` del mod verso chiunque altro è trattenuto, e lui legge una volta che è mutato. `core.is_muted` serve solo per
rispondere meglio (come fa `/msg`).

### Come si aggiunge una funzione all'API

Non a mano. Le firme stanno **una volta sola** in `src/script/api.luau`, e
`tools/api_gen` (un generatore scritto in Luau, nessuna toolchain nuova) produce
al momento della build tre file:

| file | a cosa serve |
|---|---|
| `src/script/api_generated.inc` | il glue C++: `luaL_check*`, default, valori di ritorno, registrazione |
| `docs/api.d.luau` | i tipi per il language server |
| `docs/API.md` | questa documentazione |

Aggiungere `core.node_name` è costato sei righe nella tabella descrittiva più la
funzione host: il glue, i tipi e la doc si sono aggiornati da soli. Il costo
vero dei binding senza FFI (DESIGN.md §5.4) si paga con del tooling, non a mano.

`core.event.*` e `core.register_mapgen` restano scritte a mano: prendono tabelle
o funzioni invece di scalari, e un generatore che le coprisse sarebbe più grande
di quello che sostituisce.

---

## Esempio minimo

```lua
--!strict
local BLU = core.register_node("mio:blu", 60, 90, 220)

for i = 0, 9 do
    core.set_block(50 + i, 20, 50, BLU)
end

local t = 0.0
function on_step(dt: number)
    t += dt
end
```

con

```
name = mio
version = 0.1.0
capabilities =
```

I tre mod in `mods/` sono esempi funzionanti:

- `spike` — nessuna capability, una torre e un portale che si muove ogni frame;
- `beacons` — `capabilities = os`, `depends = spike`, un faro che pulsa;
- `palette` — venti blocchi dichiarati in un ciclo, che è anche la prova che
  l'atlante cresce da solo (da 16 a 64 tile) senza toccare niente nel motore;
- `guard` — annulla `core:pre_place` dentro la piazza dei portali, sostituisce
  il blocco sopra y=30, e alza un evento suo che `beacons` ascolta dalla propria
  VM. Nessuna riga di motore è stata scritta per la regola "qui non si
  costruisce";
- `portal` — una cornice di `portal:frame` attorno a un buco d'aria, in piedi o
  coricata, apre una delle 16 aperture del mondo (`core.portal_open`) e la
  collega all'ultima rimasta sola (`core.portal_link`); il tasto destro sulla
  cornice apre il suo menù, con le altre cornici a cui collegarla. Finché non
  è collegata il client la chiude con una membrana verde animata, che si apre
  dal centro sull'immagine quando l'altro capo arriva (0.9 s);
- `chambers_game` e `lab` — la portal gun: ogni colpo è un `core.portal_open`
  col colore del tasto (1 blu, 2 arancione), così il primo portale sparato è
  chiuso nel suo colore finché non c'è il secondo;
- `games/track` (`track_core`) e `games/inferno` (`inferno_track`) — la pista
  dell'inferno, fra due stanze con tre bocche ciascuna (fino a 4 per mondo):
  la cornice A della pista (apertura 1, `--portal-to @inferno/2`) porta a B,
  l'apertura 2 dell'inferno, che riporta ad A (`@pista/1`); C (apertura 1 del
  inferno, `@pista/2`) porta a D, l'apertura 2 della pista, che riporta a C
  (`@inferno/1`). Le aperture 2 e 3 si mettono con `core.set_portal` e
  `core.portal_place` come le prime due. Un corpo che guida
  (`core.body_drive`) passa nell'altra stanza ancora in kart, se quella stanza
  conosce il modello che indossa, e con lui la riga di `core.player_carry`
  (qui: il giro in corso), che di là si legge con `core.player_carried`; la
  stanza lasciata lo sa da `core.body_taken` e non parcheggia un kart che è
  andato via. `track_game` gira in tutte e due le stanze e cronometra
  solo i gate della sua (`inferno = true`). Dietro a una cornice che un kart
  attraversa serve spazio: il kart supera il piano di qualche nodo prima che
  il passaggio sia risposto;
- `tools` — un blocco che chiede il livello 1, il piccone che lo apre e la pala
  che non lo apre, con le ricette per farsene altri. È il posto dove sta un
  attrezzo: il motore mette i secondi e i materiali sui propri blocchi e lascia
  livelli e attrezzi al contenuto.
