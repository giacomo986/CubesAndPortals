# M7 — Gioco minimo: cosa è stato costruito e cosa si è imparato

Milestone M7 di `DESIGN.md` §8: *inventario, craft, UI (microui/Clay guidata da
Lua), HUD, salute, drop, contenitori, mapgen con biomi, ciclo giorno/notte,
audio con attenuazione, mob base con ECS.*
**Test:** *una partita di 30 minuti dall'inizio, senza crash e senza toccare la
console.*

**In corso.** Sotto c'è solo ciò che è stato costruito e verificato da
esecuzioni.

---

## 1. Gli item, e una borsa in cui tenerli

Primo pezzo, e chiude anche una casella che M2 teneva aperta dichiarandola: il
suo exit test chiede *«un mod `--!strict` che aggiunge 20 blocchi e un item»*, e
il `README.md` diceva da allora **«l'item dell'exit test non c'è»** — non perché
il mod non sapesse scriverlo, ma perché non esisteva un posto dove registrarlo.

### 1.1 Due registry, non un campo su `NodeDef`

I due insiemi si sovrappongono solo in parte. Ogni blocco che un giocatore può
tenere in mano ha una forma-item, e l'engine la **deriva** invece di far
scrivere tutto due volte; ma un piccone, un bastone e un secchio sono item senza
blocco. Due tabelle, di cui una adotta dall'altra:

```cpp
size_t ItemRegistry::adopt_blocks(const NodeRegistry &nodes);
```

Chiamata **dopo** che i mod hanno parlato, non dentro `register_node`: un mod può
registrare un blocco e poi sovrascriverlo nello stesso giro, e adottare uno per
uno conierebbe un item per una definizione che non esiste più.

L'aria è esclusa. Non c'è modo di tenere in mano niente in particolare.

E la forma-item porta **il nome del blocco**, non `core:stone_item`: sono la
stessa cosa vista da due lati, e dargli due nomi è come una registry finisce per
avere bisogno di una tabella che mappa fra le due.

Numeri, sui mod veri:

```
4 mods running, 31 block(s) can be held, 32 item(s) in all
[mod] palette: 20 blocks registered, first id 7, last id 26, brush item 1
```

La differenza di uno è l'item che non è un blocco.

### 1.2 Le quattro regole che una prima stesura sbaglia

Sembrano ovvie tutte e quattro, e sono esattamente quelle che nessuno nota finché
un giocatore non conta i suoi blocchi.

**Le pile parziali si riempiono prima di prendere uno slot nuovo.** Senza questo
passaggio, un giocatore con otto slot da un sasso ciascuno si riempie la borsa
con **otto sassi**.

**Niente sparisce.** `add()` restituisce quello che **non** ci stava, e la
decisione su dove finisce è di chi chiama:

```
ok    a bag with room for sixteen returns the four that did not fit
ok    so what went in plus what came back is what was offered
```

**Una spesa è tutto o niente.** Prendere quattro da uno slot che ne ha tre viene
rifiutato e non cambia niente: una spesa parziale è come un inventario finisce
per pagare qualcosa che il giocatore non ha ricevuto.

**Uno slot vuoto è `kNoItem` con conto 0**, mai un conto 0 con ancora un nome
dentro. Una sola invariante, imposta in un solo posto (`normalise`).

### 1.3 L'ordine fra il pagamento e il blocco

```cpp
if (!try_place(s, x, y, z, want)) return false;
bag.spend_held(1);
```

Prima il blocco, poi il pagamento. Un mod può annullare il piazzamento
(`core:pre_place` è cancellabile), e pagare per un piazzamento che un handler ha
rifiutato è il difetto che questo ordine rende impossibile.

Nell'altro verso la scelta è opposta e dichiarata: **rompere con la borsa piena
rompe lo stesso**, e il drop si perde. L'alternativa è un giocatore che non può
scavare, e un mondo dove un blocco a volte rifiuta di rompersi per un motivo che
niente sullo schermo spiega.

### 1.4 Un reload non svuota la borsa di nessuno

Stessa domanda del reload dei blocchi, risposta diversa a metà. Gli id
**sopravvivono** — una pila di quell'item può essere nella borsa di qualcuno, e
rinumerare per fare ordine riscriverebbe ogni contenitore del mondo. Ma un item
orfano non diventa magenta: un item che nessuno può più ottenere non ha bisogno
di essere visibile da lontano. Smette di essere ottenibile e smette di piazzare.

```
ok    a dropped item keeps its id too
ok    and places nothing, so it cannot put down a block it no longer describes
ok    and breaking that block drops nothing, because nothing obtainable makes it
ok    a reload that registers an item again keeps its id
```

### 1.5 La casella di M2, chiusa dove era dichiarata aperta

`tests/mod_reload_test.cpp` è il file che cita quell'exit test nel suo commento
di testa. Ora c'è un terzo mod fixture che lo fa alla lettera:

```
twenty blocks and an item
  ok    the mod registered twenty blocks
  ok    and one item
  ok    which is not a block: it puts nothing down
  ok    and does not stack, being a tool
  ok    while the twenty blocks got item forms without the mod asking
```

E la stessa cosa nel gioco vero: `mods/palette` registra i venti blocchi e un
`palette:brush`.

Dal lato Lua sono due funzioni nuove, generate come tutte le altre dalla tabella
di `src/script/api.luau`:

```lua
core.register_item(name, max_stack?, places?)   -- places = nome di un blocco
core.item_id(name)
```

`places` è risolto **per nome e adesso**: un item che nomina un blocco che
nessuno ha registrato piazza aria, che è la risposta onesta a «metti giù una cosa
che non esiste».

### 1.6 Sabotaggi

Quattro, tutti rossi:

| Sabotaggio | Esito |
|---|---|
| Tolto il passaggio sulle pile parziali | 2 rossi |
| `take()` che spende parzialmente invece di rifiutare | 3 rossi |
| `adopt_blocks` che parte dall'aria | 1 rosso |
| `end_reload` che dimentica il nome, così un ri-registro conia un id nuovo | 2 rossi |

### 1.7 Limiti dichiarati

**Non c'è craft, non ci sono drop per terra, non ci sono contenitori e non c'è
UI.** La barra è nel log e nella riga di aiuto, non sullo schermo.

E soprattutto: **niente di questo passa ancora dalla rete.** Un client connesso
a un server non può rompere né piazzare un blocco — non è una regressione, non è
mai stato possibile, e il `README.md` lo dice da M3. `Inventory` sta in `core/`
proprio per questo: `DESIGN.md` §3.4 dice *«inventari solo server-side»*, e il
client a processo singolo è la propria autorità, quindi il percorso di rete
girerà lo **stesso** codice invece di una seconda copia che deve essere
d'accordo con la prima. È il pezzo successivo.

La borsa del giocatore locale sta accanto al `Player` e non dentro `Session`: si
porta dietro chi la porta, e chi attraversa un'apertura non la lascia nel mondo
da cui è uscito. Cosa succeda a un inventario quando i due mondi hanno **registry
diverse** è una domanda che `DESIGN.md` non risponde e che questo pezzo non apre.

## 2. Il blocco attraversa il filo

Fino a qui un client connesso a un server **non poteva rompere né piazzare un
blocco**. Non era una regressione: non è mai stato possibile, e il `README.md` lo
dichiarava da M3. I click in rete erano legati alle aperture, che sono l'unica
cosa che il server sapeva già decidere.

Protocollo **8**: `Edit`, `BlockChange`, `InventoryState`, e la tabella degli
item nel `Welcome` accanto a quella dei blocchi.

### 2.1 Il messaggio non nomina la cella

`Edit` porta **il blocco colpito dal raggio e la sua faccia**, mai la cella che
deve cambiare. Stessa forma di `PlacePortal` e stessa ragione: con una cella
d'arrivo sul filo esiste un campo in cui un client modificato appende un blocco
a mezz'aria, e il server dovrebbe ricostruire il raggio per accorgersene. Da un
nodo e una faccia la cella la deriva lui, e non c'è niente da ricostruire.

E `slot` viaggia **dentro il click**, non come messaggio a parte: una selezione
che arriva dopo il click che la intendeva è il modo in cui un giocatore paga un
blocco che non ha posato.

### 2.2 Chi decide e chi fa

```cpp
bool plan_edit(PeerId, const Edit&, EditPlan &out, std::string &why) const;
void edit_done(PeerId, const EditPlan&);
```

`core/net` decide **se**: scope, portata, cosa c'è lì, cosa c'è in mano, chi c'è
in mezzo. Il **fare** — gli handler dei mod, la scrittura, la riluminazione — è
di chi possiede il mondo, perché `core/net` non possiede né un mondo scrivibile
né un host di scripting, e farsi crescere una dipendenza dallo scripting per
arrivarci sarebbe il telescopio dalla parte sbagliata.

E siccome **niente viene speso o guadagnato prima di `edit_done`**, un
piazzamento che un mod annulla non costa nulla. Non è una promessa: è la
meccanica, ed è quella che il gate controlla.

### 2.3 Ogni edit ha una risposta, e la risposta si legge

Rifiutato o no, il server risponde con un `BlockChange` per quella cella: il
blocco nuovo se è successo, **quello che c'è ancora** se non è successo. Rimette
a posto una cella che un client avesse disegnato in anticipo, ed è un no-op per
uno che non l'ha fatto. È la regola di §25 — un timeout non è una risposta.

E la copia che torna a chi ha chiesto porta **la frase**, che è la lezione di
§31: un rifiuto che il giocatore non può leggere lo manda a cercare nel posto
sbagliato.

**L'ordine dei controlli è il messaggio.** Mano prima della cella: possono essere
sbagliate tutte e due insieme, e «there is already something there» manda il
giocatore a provare un'altra cella, dove succede la stessa cosa, perché il
problema non era la cella.

### 2.4 Tre gate verdi per la ragione sbagliata

Questa è la parte che vale la pena scrivere, perché è successa **tre volte in un
commit**.

| Controllo | Passava perché il rifiuto diceva |
|---|---|
| «piazzare a mano vuota è rifiutato» | *there is already something there* — miravo a una cella già piena |
| «non si costruisce addosso a chi c'è» | *your hand is empty* — a quel punto la borsa era vuota |
| «una finestra non tocca niente» | *the server does not know where you are yet* — l'osservatore non aveva ancora rivendicato un punto |

Tutti e tre **verdi**. Tutti e tre stavano misurando un rifiuto diverso da quello
che nominavano. Il terzo è quello che lo dimostra: tolta del tutto la regola sullo
scope, il gate restava **verde**.

La correzione è la stessa per tutti e tre: non basta controllare **che** sia
stato rifiutato, bisogna controllare **cosa dice**. Un gate su un rifiuto che
asserisce solo il rifiuto non è un gate.

> Aggiunta gratis dal terzo: un osservatore è rifiutato **due volte**. La sua
> `Move` è una rivendicazione di punto di vista e non una posizione (§23), quindi
> `authority().valid` per lui è sempre falso. La regola sullo scope è cintura e
> bretelle — e va bene così, purché sia il gate a saperlo.

### 2.5 Nessuno viene murato dentro

Il controllo sulla cella occupata guarda **tutti i corpi**, non solo chi chiede.
Un giocatore che può sigillare il vicino dentro un blocco ha un'arma, e il vicino
non ha modo di dire di no.

### 2.6 Niente regali

Il server non consegna nessun inventario iniziale. L'unico modo di avere un
blocco è averne rotto uno, quindi l'ordine del gate è l'ordine che il gioco
impone davvero: rompi, ricostruisci, e il terzo click è rifiutato perché la borsa
è di nuovo vuota.

```
edits: 4 sent, 4 cell(s) changed, 2 refused; the item table has 33 entries
the bag held 1 item(s) after the break
that edit was refused: your hand is empty
that edit was refused: that block is 40.7 nodes away, further than 7.0
```

E il blocco che torna nel mondo è **lo stesso** che ne era uscito, non solo
qualcosa di solido: un item che piazzasse il primo blocco della tabella
riempirebbe la cella e passerebbe un controllo che chiede solo «è di nuovo
pieno?».

### 2.7 Sabotaggi

Quattro, tutti rossi:

| Sabotaggio | Esito |
|---|---|
| Un osservatore può modificare il mondo | 1 rosso (e serviva il controllo sulla frase: senza, verde) |
| `edit_done` non spende | 3 rossi |
| Nessun controllo di portata | 2 rossi |
| Nessun controllo su chi sta nella cella | 2 rossi |

### 2.8 Due scenari in una run sono uno scenario che nessuno ha scritto

La sequenza di edit è stata messa dentro `run_network`, che è la stessa funzione
che fa l'attraversamento scriptato. In `bad_link` — 400 ms, 20% di perdita —
questo non ha funzionato, e vale la pena scrivere perché.

**Primo tentativo:** far aspettare la camminata verso l'apertura fino a sequenza
finita. Logico: l'attraversamento sposta il giocatore su un altro server con
un'altra borsa, quindi la sequenza deve chiudersi prima. E ha **peggiorato le
cose**: l'attraversamento slitta di otto secondi, cioè cambia il *timing della
cosa che `bad_link` misura*. Otto run: **tre rosse**, e nessuna sulle mani —
tutte sulla finestra sul mondo di là, che a fine run si trovava in uno stato che
i check dell'attraversamento non si aspettavano.

Un gate che va **ritarato** per far entrare un secondo scenario è un gate che
adesso sta misurando il secondo scenario.

**Correzione:** la sequenza non parte su un mondo le cui aperture portano
altrove. `bad_link` torna ai suoi 40 secondi e al suo timing di prima — sei run
su sei verdi — e le mani si misurano in `net_client`, dove il mondo è uno solo.

E la rinuncia è **detta**, non silenziosa: se la sequenza non parte il client
stampa perché, e `net_client.sh` controlla che lì sia partita davvero. Un blocco
di check che passa non succedendo è la cosa che questo commit ha già trovato tre
volte.

### 2.9 Limiti dichiarati

**Il click costa un round trip.** Niente viene disegnato in anticipo: quello che
esce è il blocco colpito, quello che entra è la decisione del server. Su loopback
non si vede; a 400 ms si vedrà, ed è il prezzo dichiarato di non mostrare mai un
blocco che potrebbe essere stato rifiutato.

**`BlockChange` va a tutte le sessioni Ready, osservatori compresi**, senza
filtro di interesse: questo mondo tiene al massimo otto sessioni e un cambio di
cella è una manciata di byte, quindi il filtro costerebbe più di quanto risparmia
e potrebbe sbagliare risposta — una finestra su questo mondo lo sta guardando, e
una cella cambiata dietro il portale di qualcuno **è** cambiata.

**Rompere e piazzare non sono misurati su un link che perde pacchetti.** Sono
misurati su loopback (`net_client`) e senza socket (`net_loopback`); `bad_link`
salta la sequenza di proposito, per §2.8. Il rischio che resta scoperto è quello
di un'attesa: la risposta arriva tardi e il client fa qualcosa di sbagliato nel
frattempo. Oggi il client non ritenta e non disegna niente in anticipo, quindi
non c'è niente che possa fare — ma è un argomento, non una misura.

**Non c'è un gate sul fatto che l'edit di un GIOCATORE sopravviva a un riavvio.**
Meccanicamente ci sopravvive — passa dallo stesso `World` che `persistence.sh`
salva e ricarica — ma quel gate riavvia un server senza client, quindi quello che
misura è la scrittura di un mod, non quella di chi gioca. Detto invece che dato
per scontato.

**Niente drop per terra, niente contenitori, niente UI**, e la barra è ancora una
riga di log. La tabella degli item non entra in `content_hash`: due server con gli
stessi blocchi e attrezzi diversi non sono un content mismatch, e rifiutare quel
join sarebbe una regola che nessuno ha chiesto.

## 3. Una barra che si legge, e la superficie sotto

`plat_debug_text` dice «debug overlay only. A real UI arrives in M7» dal giorno di
M0. Ecco M7.

### 3.1 Prima la superficie, poi i widget

DESIGN §2.7 dice «`microui` o `Clay` (C, MIT) + backend raylib». Quello che
entra adesso **non** è nessuno dei due: è il **backend** che tutti e due
chiedono. `microui` e `Clay` emettono una lista di rettangoli, immagini e
porzioni di testo con un rettangolo di clip, e `gfx_2d_*` è esattamente quella
lista dall'altro lato:

```c
void gfx_2d_begin(int width, int height);   // pixel, origine in alto a sinistra
void gfx_2d_rect(...);
void gfx_2d_image(gfx_texture, u, v, uw, vh, x, y, w, h, rgba);
void gfx_2d_scissor(...);
float gfx_2d_text(...);
```

Costruire prima i widget e poi la superficie è come una UI finisce per non
girare su nient'altro che il toolkit su cui è nata.

La sorgente dell'immagine è **normalizzata**, non in texel, e la ragione è
concreta: l'unico chiamante che esiste conosce la sua tile come frazione
dell'atlas (`colonna/tiles`) e non sa quanti pixel sia l'atlas. Con i texel
questo layer avrebbe dovuto tenere la dimensione di ogni texture perché il
chiamante la ridividesse subito dopo — scritto, misurato inutile, tolto.

### 3.2 La barra non tiene niente di suo

`draw_hotbar` prende **slot e non un `Inventory`**, e non è un dettaglio: la
borsa di un giocatore in rete è un `InventoryState` che il server ha mandato,
quella di un giocatore in processo singolo è un `Inventory`. Farla dipendere da
uno dei due significa convertire l'altro ogni frame — cioè una seconda copia di
una cosa che il server possiede, tenuta in un tipo il cui scopo è **essere**
l'autorità.

E l'icona esce dallo **stesso atlas** con cui è disegnato il mondo. Un'icona da
un secondo atlas è un'icona che può contraddire il blocco che nomina dopo che un
reload ne ha ricostruito uno solo dei due.

### 3.3 Il gate, e quattro verdi per la ragione sbagliata

Target di rendering **suo**, non quello condiviso con i gate dei portali: una
barra in fondo a un render di riferimento è una differenza che non c'entra
niente con quello che quelli misurano.

Ogni controllo confronta **due rettangoli o due frame**, mai un colore contro una
costante: quello con cui una mod dipinge un blocco non è affare del gate, e un
test che conosce il colore è un test che è d'accordo con sé stesso.

Nella prima stesura **quattro** controlli erano verdi misurando altro:

| Controllo | Passava perché |
|---|---|
| «uno slot vuoto non mostra un blocco» | contava i pixel «non di sfondo», e uno slot vuoto ha il suo riempimento: **1681 contro 1640**, verde con l'icona disegnata ovunque |
| «due blocchi diversi non si disegnano uguali» | confrontava lo slot **selezionato** con uno non selezionato: era il bordo giallo, non il blocco. Tile forzata a 0 per tutti: **verde** |
| «il conteggio è disegnato» | stessa cosa, stesso bordo. Conteggio tolto del tutto: **verde** |
| «lo stesso disegno in due slot è identico» | due slot cadono su offset **sub-pixel** diversi, quindi due disegni identici differiscono lo stesso di ~2%: sotto quella soglia non c'è segnale |

Le correzioni sono quattro tecniche, non quattro soglie allargate:

- confrontare **il centro** degli slot, dove non cade nessun bordo, invece di
  allargare la tolleranza finché il rumore ci sta dentro;
- confrontare slot che differiscono per **una cosa sola** — 1 e 2 differiscono
  solo per il blocco, entrambi non selezionati e entrambi da 64;
- misurare il conteggio su **uno slot in due frame**, dove l'offset sub-pixel non
  esiste per costruzione (uno slot non toccato legge **0.0%**);
- e per la selezione, guardare solo il **bordo**, che è dove il segno vive.

Numeri finali:

```
9 slot(s) of 81 px at 563 977, screen 1920x1080
slots 1 and 2 -- frame and stone, both 64, neither selected -- differ over 49.1%
the middles of two empty slots differ over 0.0%, an empty and a full one over 100.0%
changing slot 2's count from 64 to 1 changed 5.3% of it, and 0.0% of the slot beside it
moving the hand from slot 0 to slot 1 changed 100.0% of slot 0's edge, 100.0% of slot 1's,
0.0% of an untouched one
```

### 3.4 Sabotaggi

Cinque, tutti rossi:

| Sabotaggio | Esito |
|---|---|
| Tutti gli slot con lo stesso bordo | 1 rosso |
| Tutte le icone dalla tile 0 | 1 rosso |
| Nessun conteggio disegnato | 1 rosso |
| Un'icona anche negli slot vuoti | 1 rosso |
| La barra disegnata sotto il bordo dello schermo | 5 rossi |

### 3.5 Limiti dichiarati

**Non è ancora una UI**, è una barra e la superficie sotto. Niente pannelli,
niente inventario aperto, niente drag and drop, e niente `microui`: quello arriva
con il primo pannello vero, che è l'inventario.

**Niente da Lua.** DESIGN §8 chiede «UI guidata da Lua» e questo non lo è: non
c'è una sola chiamata `core.ui.*`. La superficie è il prerequisito, non la
risposta.

**Il testo è quello di raylib.** `gfx_2d_text` passa da `DrawTextEx`: font atlas,
metriche e codepoint sono mille righe che nessuno qui vuole possedere, e
alimentano lo stesso batch rlgl, quindi finiscono nella nostra ortografica come
tutto il resto. È l'unico punto di questo file che non guida rlgl direttamente, ed
è una scelta dichiarata.

**La barra sta fuori dalla capture.** Viene disegnata sulla finestra dopo la fine
del render target, perché i gate dei portali confrontano un frame catturato con un
render di riferimento dello stesso mondo. Il gate della barra si fa un target suo.

## 4. La borsa si apre, e non è `microui`

Primo pannello vero, ed è anche il punto in cui va detto perché `microui` non
entra.

### 4.1 La mitigazione di DESIGN §2.7 regge, ma non è questa

DESIGN elenca «nessuna UI dichiarativa» fra i buchi di raylib e propone
`microui` o `Clay`. Resta vero. Ma cosa serve **a una borsa**: un rettangolo,
una griglia di quadrati e un hit test. Nessuna delle tre è un widget.

Quello che `microui` comprerebbe davvero sono i **form**: campi di testo,
slider, checkbox, liste che scorrono. Cioè il lavoro di compatibilità formspec di
DESIGN §7.5. Portarsi in casa un toolkit prima del primo form è portarselo in
casa per la ragione sbagliata — e questo è scritto qui perché la prossima
persona che apre il file non deve rifare il ragionamento da capo.

Il quadrato è disegnato da **una** funzione, usata dalla barra e dal pannello.
Disegnarlo due volte con due pezzi di aritmetica è come i due arrivano a non
essere d'accordo su cosa sia uno slot — e un giocatore che li vede insieme se ne
accorge.

### 4.2 Non c'è nessuna pila nel cursore

Il modo ovvio di spostare uno stack è tenerlo attaccato al puntatore. Qui no, e
non è una semplificazione: **una pila a metà strada fuori dalla borsa è stato che
il client possiede e il server no**, e il primo pacchetto perso è un giocatore
che ha in mano una cosa di cui il server non ha mai sentito parlare.

Quindi: un click **sceglie** uno slot, il click dopo manda `MoveItem{from, to}`.
Il server fa l'aritmetica e risponde con **tutta** la borsa. Protocollo **9**, e
`MoveItem` è l'unico messaggio del protocollo che cambia un inventario — e
nemmeno lui è il client che ne cambia uno: è una richiesta.

Due segni diversi, perché sono due fatti diversi: **la mano** è da dove esce il
prossimo blocco, **la scelta** è da dove il prossimo click sposterà qualcosa. Un
giocatore che non li distingue non sa cosa farà il suo prossimo click.

E un click su uno slot vuoto non sceglie niente: senza quella riga, il primo
click sulla metà vuota della borsa arma uno spostamento che il click dopo spende
a non fare nulla.

### 4.3 Rifiutare, non aggiustare

`Inventory::move_to` risponde `false` e non tocca niente quando non può fare la
cosa. Tre casi, e sono quelli che un giocatore si aspetta: uno slot vuoto prende
tutto, lo **stesso** item si fonde e lascia indietro quello che non ci sta, una
cosa **diversa** si scambia — e solo a stack intero, perché mezzo stack non ha
dove mettere quello che ha spostato.

Uno spostamento che riesce a metà è come un inventario perde oggetti.

### 4.4 Il gate, e due verdi per la ragione sbagliata (ancora)

| Controllo | Passava perché |
|---|---|
| «ogni quadrato risponde all'hit test col suo numero» | provava **solo i centri**: un hit test che si rifà l'aritmetica e dimentica lo spazio fra i quadrati azzecca tutti i centri e sbaglia i bordi — **54 sonde su 180** fuori posto, e la versione a soli centri era verde |
| lo stesso controllo, con la griglia **trasposta** | `slot_at` e `slot_rect` passano dalla stessa funzione, quindi restano d'accordo fra loro mentre mettono nove righe da quattro sullo schermo |

Le correzioni: cinque sonde per quadrato invece di una, e un controllo separato
sulla **forma** della griglia (lo slot 1 sta *accanto* allo slot 0, quello una
riga più in là sta *sotto*) che non passa da `slot_rect` per decidere se ha
ragione.

Numeri:

```
36 slot(s), 9 x 4, at 563 366
0 of 180 probes landed on the wrong slot
the corner behind the panel reads 46 out of the 102 it was cleared to
picking slot 2 changed 94.1% of its edge, 0.0% of the hand's, 0.0% of a slot that is neither
the hand's top edge and the pick's agree on 0 of 81 pixels
```

### 4.5 Sabotaggi

Sei, tutti rossi:

| Sabotaggio | Esito |
|---|---|
| La mano e la scelta con lo stesso colore | 1 rosso |
| Niente velo scuro dietro il pannello | 1 rosso |
| Griglia trasposta | 2 rossi |
| Hit test che si rifà l'aritmetica | 1 rosso (54/180 sonde) |
| Una finestra può riordinare la sua borsa | 1 rosso |
| Slot fuori range clampato invece che rifiutato | 3 rossi (2 headless, 1 sul filo) |

### 4.6 Limiti dichiarati

**Non ci sono contenitori.** Il pannello mostra la borsa di chi guarda e nient'altro:
niente casse, niente fornaci, niente secondo inventario accanto al primo. È lì che
`MoveItem` dovrà crescere un «da quale inventario», ed è anche lì che comincia il
problema di chi ha il diritto di aprire cosa.

**Niente craft.** Una griglia di craft è un inventario con una regola sopra, e la
regola non c'è.

**Niente mezzo stack.** `MoveItem` porta un `count` e `move_to` lo usa, ma nessun
click lo manda: il tasto destro che divide una pila non è collegato.

**Niente da Lua**, come nel §3.

---

## 5. Un reload rendeva inottenibile ogni blocco dei mod

Trovato giocando, non da un gate. Il client stampa una riga per mod a ogni
ricarica; quella riga diceva, sedici volte di fila:

```
mod 'palette': 20 item(s) can no longer be obtained, 0 new one(s) can
mod 'spike':    3 item(s) can no longer be obtained, 0 new one(s) can
mod 'beacons':  2 item(s) can no longer be obtained, 0 new one(s) can
mod 'guard':    1 item(s) can no longer be obtained, 0 new one(s) can
```

Un solo F5 e **26 blocchi su 31** smettevano di essere posabili e di cadere
quando li si rompeva. Non tornavano più.

### 5.1 La causa

`ItemRegistry` chiedeva a sé stesso chi fosse ancora vivo:

1. `begin_reload(owner)` marchia stale ogni item di quel mod.
2. Il mod rigira il suo codice e ri-registra i suoi **blocchi**, cosa che tocca
   `NodeRegistry` e non l'inventario.
3. `end_reload(owner)` non trova nessuno che abbia ripulito lo stale e li
   dichiara tutti orfani: `places = kAir`.
4. `adopt_blocks` gira dopo e li salta, perché il nome è ancora nella tabella --
   orfano, ma presente.

Solo `register_item` azzerava `m_orphan`, e un item **adottato** non ci passa
mai: nessun mod scrive una riga sugli item dei propri blocchi, è il motore che
li conia. Sopravvivevano gli item `core:` (owner mai stale) e quelli che un mod
registra a mano.

### 5.2 La forma dell'errore

Un item «forma di blocco» è **derivato**, non dichiarato. Chiedere «il mod l'ha
registrato di nuovo?» è la domanda sbagliata da fargli: il mod non l'ha
registrato **mai**, nemmeno la prima volta. La domanda giusta va fatta ai
blocchi, e per farla serviva sapere quali blocchi un reload ha lasciato indietro
-- che `NodeRegistry` sapeva solo dipingendoli di magenta, senza tenerne conto.

Quindi:

- `NodeRegistry::is_orphan(BlockId)`, un flag parallelo come quello che
  `ItemRegistry` aveva già;
- `ItemRegistry::end_reload(owner, nodes)`: un item stale il cui nome è ancora
  un blocco vivo non è orfano, e la sua definizione viene riletta dal blocco;
- `adopt_blocks` risuscita un orfano il cui blocco è tornato, **sullo stesso
  id**, invece di saltarlo -- ed è quello che chiude il caso in cui a
  ri-registrare il blocco è un altro owner, l'unico modo in cui un blocco vivo
  può trovarsi davanti un item orfano.

L'id non si riusa mai: è il motivo per cui cinque `spike:copper` rimasti in
borsa attraverso la morte del blocco sono ancora cinque quando torna.

### 5.3 Perché il gate era verde

`mod_reload_test` provava tre casi e nessuno era quello vero:

| Caso | Esito | Perché passava |
|---|---|---|
| `live:pick`, item ri-registrato a mano | non orfano | passa da `register_item`, che pulisce |
| `live:wand`, item tolto | orfano | corretto |
| `live:three`, blocco nuovo | ha un item | nome nuovo, `adopt_blocks` lo conia |

Manca `live:two`: un blocco che **entrambe** le versioni registrano, il cui item
è adottato. Cioè tutta la popolazione reale. Non è un gate verde per la ragione
sbagliata -- è un buco esattamente dove sta il difetto, e i tre casi lo
circondano senza toccarlo.

Ora `live:two` è gatato, e `inventory_test` fa i quattro giri in ordine: il mod
tiene il blocco, lo lascia cadere, lo rimette, e infine lo rimette sotto un altro
owner.

### 5.4 Numeri

Prima, per ogni F5: `20 + 3 + 2 + 1 = 26` item persi, 0 recuperati.
Dopo, sul percorso vero (client avviato, `touch` su due mod, watcher che li
ricarica): nessuna riga -- la stampa è condizionata a `orfani > 0 || adottati >
0`, e sono entrambi zero.

### 5.5 Sabotaggi

| Sabotaggio | `inventory` | `mod_reload` |
|---|---|---|
| `end_reload` non chiede ai blocchi | 4 rossi | verde |
| `adopt_blocks` salta l'orfano | 4 rossi | verde |
| `NodeRegistry::end_reload` non marchia | 7 rossi | verde |
| `adopt_blocks` adotta anche i magenta | 3 rossi | verde |
| **le due metà insieme** (lo stato spedito) | 4 rossi | **3 rossi** |

`mod_reload` è un gate end-to-end e le due metà si coprono a vicenda: resta verde
se se ne rompe una sola, e diventa rosso esattamente nello stato che è stato
spedito. È il comportamento giusto per un gate di quel livello, ed è detto qui
perché «verde sotto sabotaggio» di solito non lo è.

---

## 6. Drag & drop, e perché è la stessa funzione del click

Un gesto solo: **il drag è l'unico modo in cui una pila si sposta.** Il
click-poi-click che c'era prima è stato tolto, non lasciato lì inutilizzato --
vedi 6.1.

Una funzione sola, `ui::bag_click(press, release, press_has_item)`, pura e
nell'header, chiamata da **entrambi** i loop del client. Un gate su una copia
della regola non gata niente.

| Gesto | Risultato |
|---|---|
| giù su A, su su B | `Move A -> B` |
| giù e su sulla stessa casella | niente: un click non sposta |
| giù su una casella vuota | niente |
| su fuori dal pannello | niente |
| giù fuori dal pannello | niente, comunque finisca |

Due regole valgono una spiegazione. **Il drag porta quello su cui è partito**, e
non ripiega su nessun'altra casella: un trascinamento che sposta di nascosto
qualcos'altro è come una pila sparisce sotto gli occhi di chi non la stava
guardando. E **un rilascio fuori dal pannello non sposta nulla**, perché non
esiste un terreno su cui far cadere una pila -- il 4.6 lo dichiara e questo lo
rispetta invece di inventarsi un comportamento.

**Nessuna pila nel cursore, ancora.** Il fantasma sotto il puntatore è un
disegno: la pila resta nella sua casella, il server non sa niente finché il
tasto non risale, e a quel punto parte esattamente il `MoveItem` che sarebbe
partito da due click. Un drag non è un messaggio nuovo.

### 6.1 Perché il click-poi-click è stato tolto e non lasciato

Due modi di fare una cosa sola sono due stati che chi gioca deve tenere in
testa, e uno dei due -- «c'è qualcosa di scelto in questo momento?» -- è
**invisibile** appena si guarda da un'altra parte. Il drag il suo stato lo tiene
nel tasto del mouse, dove una mano già sa dov'è.

Tolto e non semplicemente inutilizzato: chi si ricorda a metà il vecchio modo
deve ottenere **niente**, non qualcosa che non intendeva. Per questo
`press == release` è un caso dichiarato che restituisce `Nothing`, e non un
`Move` di lunghezza zero che qualcun altro più a valle rifiuta.

Sparisce con lui lo stato `picked` dal client: non c'è più una casella scelta,
c'è solo la casella da cui un drag sta sollevando. Il segno azzurro resta ma
dice un'altra cosa, e si chiama `dragged`.

### 6.2 La casella di partenza si svuota

Mentre il drag è in volo la casella di partenza è disegnata **vuota**: la pila è
sotto il cursore ed è **una** pila. Una borsa che la mostra in due posti sta
dicendo a chi gioca che ne ha due.

Il quadrato però resta -- riempimento e segno -- perché un buco nella griglia si
legge come «la borsa ha perso uno slot», non come «una pila è in volo».

**Ricompare da sé se il drop non vale.** Non c'è un ramo che la rimette a posto:
la casella è vuota solo finché il tasto è giù, quindi al rilascio torna piena da
sola, e se lo spostamento invece avviene è il modello a cambiare. Un annullamento
che deve ricordarsi di annullare è un annullamento che prima o poi dimentica.

**Una domanda sola, posta una volta.** Il frame chiede «si sta sollevando?» una
volta e passa la risposta sia a `draw_bag` sia al fantasma: sono la stessa pila,
e chiederlo due volte è come i due arrivano a non essere d'accordo per un frame.

**Sul filo costa un giro.** In rete la casella si ripiena al rilascio e resta
piena finché non arriva l'`InventoryState` del server: per un round trip la pila
è dove il server ce l'ha ancora, che è la verità. L'alternativa -- mostrarla già
spostata -- è il client che indovina.

### 6.3 Numeri

```
dragging changed 73.8% under the cursor, 0.0% a slot's width away;
moving the cursor left 0.0% behind and drew 73.8% at the new spot
the square it came from changed 100.0% while lifted, its neighbour 0.0%;
it differs from an empty square by 0.0% while lifted and 100.0% when not
```

L'ultima riga è la coppia che conta: mentre la pila è in volo la casella di
partenza è **indistinguibile** da una casella vuota vera (slot 4, vuoto in quel
bag -- non un colore inventato dal gate), e il resto del tempo ne differisce del
100%.

Misurato **dentro** il quadrato, lontano dal bordo: il bordo porta il segno del
drag e risponderebbe a questa domanda con un altro fatto.

### 6.4 Sabotaggi

Undici, tutti rossi:

| Sabotaggio | Esito |
|---|---|
| Un click sposta ancora | 3 rossi |
| `Move` trasposto (`to` e `from` scambiati) | 2 rossi |
| Rilascio fuori dal pannello accettato | 1 rosso |
| Pressione iniziata fuori dal pannello accettata | 1 rosso |
| Drag da casella vuota accettato | 1 rosso |
| La casella sollevata non si svuota | 2 rossi |
| Si svuota la casella sbagliata | 3 rossi |
| Si svuotano tutte | 2 rossi |
| La casella sollevata non è disegnata affatto | 1 rosso |
| Fantasma disegnato in un punto fisso | 2 rossi |
| Fantasma mai disegnato | 2 rossi |

Tre meritano una riga.

`Move` **trasposto**: 0 e 20 sono entrambe caselle vere e piene, quindi è uno
spostamento perfettamente legale verso il posto sbagliato. Un gate che contasse
«è avvenuto uno spostamento» resterebbe verde. Il controllo sul verso è la
seconda metà.

**Fantasma inchiodato**: «sparito da dov'era» da solo è vero anche di un
fantasma disegnato in un angolo che nessuno ha chiesto. Il gate chiede **le due
metà** -- sparito di là, disegnato di qua -- e la prima versione, che chiedeva
solo la prima, restava verde col fantasma a 100,100.

**Casella sollevata non disegnata affatto**: svuotare il quadrato e cancellarlo
si assomigliano finché non si guarda cosa c'è sotto. Il gate distingue perché
confronta con una casella vuota **vera**, che ha un riempimento -- un buco no.

### 6.5 Limiti dichiarati

**Il gesto è gatato, i due loop no.** `bag_click` è provata su tutti i suoi casi,
e il fatto che il frame ponga la domanda «si sta sollevando?» una volta sola è
riprodotto nel gate; ma che i loop la chiamino con press e release giusti è
verificato solo a mano: non c'è un gate che simuli una pressione del mouse.

**Niente mezzo stack, ancora.** Nemmeno col drag: il tasto destro che divide una
pila continua a non essere collegato, e `count` resta un campo che nessun gesto
riempie.

**Niente drop nel mondo.** Rilasciare fuori dal pannello non fa nulla, di
proposito. Quando ci sarà un oggetto per terra, quella è la riga che cambia.

---

## 7. Una pila per terra

DESIGN §8 elenca «drop» fra i pezzi di M7, e fino a qui il motore non aveva un
posto dove mettere una pila che nessuno stesse portando. La cosa era già scritta
due volte, come due limiti diversi:

- §1.3: rompere con la borsa piena rompe lo stesso e **perde** il drop, «e questa
  è la decisione, non una svista»;
- §6: rilasciare un drag fuori dal pannello non fa nulla, «perché non esiste un
  terreno su cui far cadere una pila».

Sono la stessa mancanza detta due volte. Questa sezione chiude la prima; la
seconda resta aperta e ora ha dove andare.

### 7.1 È un'entità, e questo è il punto

Una pila per terra è un'entità con `Position`, `Look`, `Body` e `Dropped`. Non ha
una lista sua, e non ha un messaggio suo.

`core/ecs/mob.h` era stato scritto per rendere verificabile **una** affermazione:
che il percorso di streaming è un percorso e non un caso speciale sulle
connessioni — «le entità vicine con un `Body`», non «le altre sessioni, meno gli
osservatori, meno quelle che non hanno ancora detto dove sono». Il drop è la
seconda prova della stessa affermazione, fatta da qualcosa che non è nemmeno una
creatura: arriva a ogni client attraverso codice che nessuno ha modificato per
farcelo arrivare.

Sul filo costa **due byte per corpo**: `EntityState.item`, `kNoItem` per un
giocatore o un mob. Un messaggio a parte avrebbe voluto dire riscrivere le regole
di interesse e distanza una seconda volta per dire la stessa cosa. Protocollo
**10**.

### 7.2 Dove si ferma

`step_drops` integra, quindi è una chiamata esplicita dal tick e non un sistema
Flecs come `core:wander` — quello è in forma chiusa e non ha passo da sbagliare.
Un gate che può consegnargli passi esatti vale più della simmetria.

Tre errori che una prima stesura fa, e che sono gate:

| Errore | Cosa si vede |
|---|---|
| riposare a `floor(y)` | la pila è **dentro** il blocco |
| riposare dove capita | la pila galleggia, e scende ancora un po' a ogni tick |
| chiedere `World::is_solid` | la pila si posa su un nodo che il giocatore attraversa |

Il terzo merita la riga: `World::is_solid` risponde «non è aria», e un mod può
registrare qualcosa che si attraversa. La domanda giusta va fatta al
`NodeRegistry`.

Il gate misura la posizione di riposo **esatta** — `4.1500`, cioè il tetto del
blocco più mezza altezza del cubo — e poi fa un secondo giro di sessanta passi
per controllare che non si muova più. Senza il secondo giro, «si ferma» è vero
anche di una pila che affonda di un pelo a ogni tick.

### 7.3 Chi la raccoglie

`collect_drops` chiede «le entità con una posizione e un inventario», non «i
giocatori»: una cassa che camminasse soddisfarebbe la stessa domanda, e questa
funzione non ha motivo di sapere la differenza.

**Un secondo di armamento**, per tutti e non solo per chi l'ha lasciata cadere.
Serve al gesto che butta via una pila: senza, la pila atterra dentro chi l'ha
buttata ed è di nuovo in borsa nello stesso tick. «Chi l'ha lasciata cadere» è un
fatto che il drop dovrebbe portarsi dietro, e un campo che può essere sbagliato
non vale un secondo in cui nessuno la tocca.

**Quel che ci sta entra, il resto resta per terra.** Tutto-o-niente qui vuol dire
una pila su cui un giocatore passa sopra per sempre. Misurato: borsa con otto
posti liberi e dieci sassi per terra — ne entrano 8, ne restano 2.

**Non restituisce niente e non manda niente: restituisce chi.** Un inventario che
cambia deve raggiungere il suo proprietario, e sapere quale connessione sia
appartiene alla tabella delle sessioni, non a questo file. Chi dimentica di
mandarlo è un giocatore la cui borsa si riempie in silenzio, quindi la lista è il
valore di ritorno e non qualcosa da chiedere dopo. Deduplicata: due pile in un
tick sono un messaggio, non due.

### 7.4 Numeri

```
it came to rest at y = 4.1500, wanted 4.1500
eight fit; it took 8 and left 2 on the ground
```

Sul filo, con la borsa piena in tutti e 36 gli slot: il blocco si rompe, in borsa
non entra niente, e `entities_for` restituisce un corpo con `item` giusto alla
cella dov'era il blocco. Dopo il secondo, il giocatore che ci sta sopra se lo
riprende e non resta niente per terra.

### 7.5 Sabotaggi

Undici, tutti rossi. Otto sul modello:

| Sabotaggio | Esito |
|---|---|
| Riposa dentro il blocco | 3 rossi |
| Riposa dove capita | 3 rossi |
| Nessuna finestra di armamento | 4 rossi |
| Raccoglie senza controllare la distanza | 2 rossi |
| Tutto-o-niente invece che quel che ci sta | 3 rossi |
| Lista dei cambiati non deduplicata | 1 rosso |
| `World::is_solid` invece del registry | 1 rosso |
| Distrugge il drop anche se ne resta | 2 rossi |

E tre sul filo:

| Sabotaggio | Esito |
|---|---|
| La borsa piena rimangia il drop | 4 rossi |
| `entities_for` non porta l'`item` | 2 rossi |
| Il drop nasce addosso al giocatore invece che sulla cella | 4 rossi |

### 7.6 Limiti dichiarati

**Niente drop nel mondo dal pannello, ancora.** Il terreno adesso c'è; il gesto
che ci butta sopra una pila no. È il pezzo successivo.

**I drop non si salvano.** Un riavvio del server li perde: non stanno nel
database del mondo, e non c'è un gate che dica il contrario.

**Non scadono e non si fondono.** Due pile della stessa cosa nella stessa cella
restano due entità, e nessuna sparisce mai da sola. Con una sessione lunga e
molte rotture questo cresce senza limite — è la voce da guardare per prima se il
conteggio delle entità diventa un problema.

**Non si muovono di lato.** Cadono e basta: nessuna spinta, nessun rimbalzo,
nessuna uscita da un muro in cui siano nate. Una pila nata dentro un blocco ci
resta.

**Il client single-process non ne ha.** L'ECS vive in `ServerSessions`, quindi la
demo senza server non ha entità: per vedere un drop serve `cnp_server`.

**Niente da Lua**, come nel §3.

---

## 8. Buttarla via, e la borsa che non c'era

Tre cose, e la prima è un difetto trovato giocando come il §5.

### 8.1 La borsa non si apriva

`E` non faceva niente appena entrati. La barra e il pannello si disegnano solo
se `s.hotbar.slots` non è vuoto, e quel vettore è **l'ultima cosa che il server
ha detto** — ma il server non diceva niente finché non arrivava una rottura o
uno spostamento di slot. Un giocatore appena entrato non aveva né barra né
borsa, e l'unico modo di farle comparire era rompere qualcosa alla cieca.

Una borsa vuota **è un fatto** e va mandata come qualsiasi altro. Ora ogni
sessione `Ready` è dovuta del suo inventario esattamente una volta, e la
decisione sta in `ServerSessions::inventories_to_send()` invece che in un flag
che il loop del server legge: così «ti è dovuto una volta sola» è una frase che
un gate può interrogare senza una socket.

### 8.2 `peek_type` aveva un `default:`

Il difetto più istruttivo del commit, e l'ha trovato il gate.

`DropItem` era scritto, il codec era scritto, l'handler era scritto — e il
server non rispondeva. Causa: `peek_type` enumera i tipi noti in uno `switch`
che finiva con `default: return MsgType::Invalid;`. Un tipo nuovo non elencato
non è un errore di compilazione: è **silenziosamente invalido**, `on_message`
non arriva mai all'handler, e l'unico sintomo è una richiesta senza risposta —
che assomiglia esattamente a un handler che nessuno ha scritto.

Il `default:` è quello che spegne `-Wswitch`. Tolto: ora ogni enumeratore va
elencato o non compila, e il byte che non è affatto un `MsgType` cade fuori
dallo switch al `return` sotto — che è quello per cui il `default:` serviva
davvero.

**Il sabotaggio corrispondente non si compila.** È più forte di un rosso: la
classe di difetto che ho appena preso in faccia adesso è un errore di build.

Da notare che `protocol_test.cpp` **aveva** lo stesso presidio e ha funzionato:
il suo switch senza `default:` ha rifiutato di compilare finché non ho aggiunto
`DropItem`. Due switch sullo stesso enum, uno protetto e uno no.

### 8.3 Il gesto

`DropItem{slot, count}`, protocollo **11**. Porta **uno slot e un conteggio e
nient'altro**: dove la pila atterra è aritmetica del server a partire da dove il
giocatore già è. Un punto sul filo è un campo in cui un client modificato butta
una pila dall'altra parte della mappa, e il server dovrebbe controllarlo contro
una portata che già conosce — lo stesso argomento per cui `Edit` nomina un
blocco e una faccia invece di una cella.

`count == 0` vuol dire tutta la pila, ed è l'unico modo per dire «tutta» senza
che il client debba sapere quante sono — e non lo sa, perché la borsa che
disegna è vecchia di un giro.

Tre modi di farlo, tutti sullo stesso messaggio:

| Gesto | Cosa parte |
|---|---|
| `Q` | `DropItem{mano, 1}` |
| `CTRL`+`Q` | `DropItem{mano, 0}` — tutta |
| trascinare fuori dal pannello | `DropItem{casella, 0}` |

Il terzo era `Nothing` «perché non esiste un terreno su cui far cadere una
pila». Adesso c'è.

`Inventory::take_from` è il verso opposto di `add()`, e **non** è `take()`:
quello ha un chiamante che sta **pagando** qualcosa e non deve pagare a metà,
questo ha un chiamante che sta buttando via roba, e chiedere più di quanto c'è è
un client la cui idea della borsa è vecchia di un giro. Quindi prende quel che
c'è invece di rifiutare, e restituisce quel che è uscito — così chi deve
metterlo da qualche parte non può dimenticarsi quanto ne ha preso.

### 8.4 E la demo ha un ECS

Il client single-process teneva la borsa in una variabile locale, quindi non
aveva niente che soddisfacesse la query di `collect_drops` («entità con un posto
e un inventario»). Ora la borsa sta su un'entità, come sul server. **Non** una
copia tenuta in riga a mano: la stessa e unica borsa, sull'entità.

Il riferimento si rilegge **ogni frame**, perché aggiungere entità può spostare
la riga di un componente e un riferimento tenuto dal frame prima punta a dov'era
la borsa.

Il §7.6 diceva «il client single-process non ne ha». Non è più vero.

### 8.5 Un gate verde per la ragione sbagliata, di nuovo

«Una finestra non può buttare via niente» era **verde con la regola di scope
tolta**: l'inventario di un osservatore è sempre vuoto, quindi il lancio non
lasciava cadere niente per mancanza di qualcosa da lasciar cadere.

Stessa trappola dell'`Edit` di una finestra, che aveva bisogno di un punto di
vista per la stessa ragione. Sistemato mettendo dieci sassi nella borsa della
finestra prima di chiederle di buttarli: adesso l'unica cosa che può rifiutare è
lo scope.

### 8.6 Numeri

```
the stack landed 0.90 m away, thrown from 34.50 30.50
```

A braccio teso e non addosso: una pila buttata ai propri piedi è una pila che il
secondo di armamento restituisce subito.

### 8.7 Sabotaggi

Otto. Sette rossi e uno che **non compila**:

| Sabotaggio | Esito |
|---|---|
| `peek_type` non elenca `DropItem` | **non compila** |
| Regola di scope tolta | 2 rossi |
| La pila nasce addosso al giocatore | 1 rosso |
| `count` ignorato (butta sempre tutto) | 2 rossi |
| `take_from` rifiuta invece di prendere quel che c'è | 2 rossi |
| L'inventario del join mandato a ogni tick | 1 rosso |
| Rilascio fuori dal pannello che non butta niente | 1 rosso |
| Butta la casella sbagliata | 1 rosso |

### 8.8 E un rosso che non era del gate

La suite completa dopo questo lavoro ha dato `hud (Failed)`. Non era un flake:
era un **binario vecchio**. Lo script di sabotaggio rimetteva a posto i sorgenti
e **non ricompilava**, quindi il `ctest` successivo ha girato un client rotto di
proposito. La riga fallita era `letting go outside the panel throws that square's
stack into the world` — esattamente il sabotaggio numero otto.

`git diff` diceva albero pulito; la cartella di build diceva un'altra cosa.

È la **seconda volta** che questo costa un rosso. Adesso è una regola: uno script
che ripristina i sorgenti **deve ricompilare**, e tutti quelli in scratchpad
finiscono con `cmake --build`. `TODO.md` OPEN-7 lo registra, e registra anche che
il **primo** dei due rossi resta senza spiegazione: il suo output era già stato
sovrascritto, e «compatibile con la stessa causa» non è «dimostrato».

### 8.9 Limiti dichiarati

**Mezzo stack ancora no.** `count` adesso è usato — da `Q` — ma solo con 1 o
tutto: nessun gesto manda una frazione.

**Non si può riprendere al volo.** Il secondo di armamento vale per tutti, quindi
chi butta una pila e resta fermo se la riprende comunque dopo un secondo: bisogna
farsi da parte. Renderlo «solo per chi l'ha buttata» vuol dire un campo in più
sul drop che può essere sbagliato, e per ora non vale.

**Una pila buttata contro un muro resta di là.** Nasce a 0.9 m davanti senza
controllare che ci sia spazio, e cade dritta. Con la faccia contro un muro
finisce dentro il blocco e ci resta.

**I due loop del client restano non gatati** come nel §6.5: `bag_click` è provata
su tutti i suoi casi, ma che `Q` mandi davvero quel che deve è verificato a mano.

---

## 9. La rotellina, e una finestra che ci sta sullo schermo

### 9.1 La rotellina

`ui::hotbar_scroll(selected, notches, count)`, pura e nell'header come
`bag_click`, chiamata da entrambi i loop.

Quattro cose che una prima stesura sbaglia, e sono tutte gate:

| Errore | Cosa si vede |
|---|---|
| verso invertito | girare via dal corpo va a destra invece che a sinistra |
| `%` che può dare negativo | `-1 % 9` è `-1` in C: la mano finisce sullo slot −1 |
| clamp invece di wrap | dal nono bisogna girare otto volte per tornare al primo |
| troncare invece di arrotondare | su un trackpad un trascinamento lento non muove **niente** |

Il verso merita la riga: allontanare la rotella va a **sinistra**, che è quello
che fa ogni gioco di questo tipo e l'**opposto** di quello che fa una barra di
scorrimento. Un gate che controllasse solo «si è mossa» non saprebbe quale dei
due.

Il wrap è controllato anche in blocco: da ogni slot, con ogni numero di scatti
da −20 a +20, il risultato è uno slot che esiste. È il controllo che prende il
modulo negativo, ed è quello che un caso singolo si perde.

Funziona **solo a borsa chiusa**: mentre un pannello è aperto la rotella
appartiene al pannello, e una che muovesse di nascosto la mano dietro sarebbe un
giocatore che posa un blocco che non ha scelto.

### 9.2 La finestra dietro la barra delle applicazioni

Chiedeva 1920x1080 su uno schermo 1920x1080. Una finestra grande come lo schermo
**non ci sta** sullo schermo: la barra del titolo e il pannello del desktop sono
da qualche parte, e il fondo del gioco finiva dietro la barra senza modo di
tirarlo fuori.

Ora: `FLAG_WINDOW_RESIZABLE`, e all'avvio la dimensione richiesta viene ridotta
ai **nove decimi** del monitor e centrata. Misurato: `1728x972, resizable` su
questo desktop.

**Ma solo per una finestra che si può ridimensionare.** Una misura chiede la
forma che intende misurare e la ottiene, su qualunque monitor, o i numeri
smettono di essere confrontabili fra macchine — e i gate sul frame rate misurano
contro lo **schermo**, quindi una finestra più piccola li renderebbe più
economici. Tutte e otto le prove GPU passano da un flag che sta in
`window_is_measured`, e stampano `window: 1920x1080, fixed for measurement`.

### 9.3 Quel che il ridimensionamento porta con sé

`gfx_screen_width/height` erano **catturate all'avvio** e non cambiavano più.
Ora si rileggono una volta per frame, in `gfx_begin_frame`: una volta sola,
perché richiederle a metà frame è come una proiezione e una viewport finiscono
costruite da due dimensioni diverse.

La proiezione seguiva già lo schermo. I **render target dei portali** no: sono
della forma della finestra e vengono campionati in spazio schermo, quindi uno
rimasto della forma vecchia mette un mondo stirato dentro l'apertura. Non lo
segnala niente — sembra soltanto un po' sbagliato, che è il genere di cosa che
qualcuno nota sei mesi dopo senza saperla collocare.

`resize_screen_targets` li ricostruisce, e il loop se ne accorge **confrontando
quel che ha costruito con quel che sta disegnando**, non leggendo un flag che
qualcuno deve azzerare: un fatto solo, tenuto dall'unico codice a cui interessa.

E `g_recursion_depth` viene ora assegnato esplicitamente invece di essere lasciato
a due default che per caso coincidono: la pool ricostruita un livello più bassa
di com'era disegna un'altra cosa.

### 9.4 Sabotaggi

Sei sulla rotella, tutti rossi:

| Sabotaggio | Esito |
|---|---|
| Verso invertito | 3 rossi |
| `%` senza correzione del negativo | 3 rossi |
| Clamp invece di wrap | 3 rossi |
| Troncare invece di arrotondare | 2 rossi |
| Un solo scatto per frame | 3 rossi |
| Guardia `count <= 0` tolta | crash (divisione per zero) |

### 9.5 195 ricostruzioni in un trascinamento

Trovato dal log di una prova a mano, non da un gate. La prima versione
ricostruiva i render target **a ogni pixel** del trascinamento: una sessione in
cui la finestra è stata trascinata una volta ha stampato **195 righe
`window:`**, cioè 195 giri di distruzione e creazione di oggetti GL, proprio nei
frame in cui la macchina è più carica.

Funzionava — la dimensione ha seguito fino a `2881x62` — e il costo era
invisibile. È il genere di difetto che un gate non prende perché non c'è niente
di sbagliato nel risultato.

`ScreenTargets` aspetta che la dimensione stia ferma **0.2 s** e poi ricostruisce
una volta sola. È un tipo e non due righe in ciascun loop perché la regola ha uno
stato — cosa è costruito, cosa la finestra ha detto, quando si è mossa — e due
copie di uno stato divergono.

Il prezzo, dichiarato: per due decimi di secondo dopo che si lascia il bordo
l'immagine dentro il portale è della forma vecchia, cioè stirata, mentre si ha
ancora la mano sulla cornice.

**Misurato dopo.** Una sessione con **cinque** ridimensionamenti — massimizza,
ripristina, e tre trascinamenti dei bordi — ha stampato cinque righe:

```
window: 1728x972, resizable
window: 1920x1008
window: 1440x748
window: 1588x352
window: 1588x561
window: 2198x561
```

Una ricostruzione per ridimensionamento contro 195 per trascinamento. È il
confronto che valeva la pena avere, e non esisteva finché la prima versione non
era stata spedita e guardata.

### 9.6 Limiti dichiarati

**Il ridimensionamento non è gatato.** Che i target si ricostruiscano alla forma
giusta è verificato **a mano**: un gate dovrebbe possedere la finestra e
trascinarla, e le prove che hanno una finestra sono esattamente quelle a cui è
vietato ridimensionarla. La riga `window: WxH` stampata a ogni cambio è quel che
resta per accorgersene.

**Nove decimi è un numero scelto, non derivato.** raylib non espone l'area utile
del monitor (GLFW sì, la sua API no), quindi la barra delle applicazioni è
stimata invece che misurata. Su un desktop con pannelli su due lati potrebbe non
bastare.

**Nessuno stato della finestra si salva.** Chiuderla ridimensionata e riaprirla
riparte dai nove decimi.

---

## 10. Una UI dichiarata da Lua

DESIGN §8 chiede «UI (microui/Clay guidata da Lua)» in M7, e ogni sezione di
questo documento finora è finita con «niente da Lua» — quattro volte. Questa è la
prima parte che smette di essere vera.

Un mod dice cosa c'è su un pannello, il motore lo disegna, e il click torna
indietro come evento. `mods/console` è la prova: **nessuna riga di codice del
motore sa che quel mod esiste.**

### 10.1 Ritenuta, non immediate mode

`microui` e Clay sono immediate: il programma descrive tutta la UI a ogni frame.
Un mod gira sul **server** e il pannello si disegna su un **client** — quindi un
pannello immediate vorrebbe dire mandare l'intera descrizione sessanta volte al
secondo dentro una socket.

Una dichiarazione mandata una volta e un click che torna è l'unica forma che
sopravvive al filo. Ed è la forma che **formspec ha già**, che è dove §7.5 va
comunque.

Quindi la scelta di §2.7 non è ancora fatta: quando arriverà un toolkit sarà per
i **form** dentro un pannello, non per il pannello.

### 10.2 Celle, non pixel

Le coordinate sono in **celle**. Una cella è uno slot della barra, cioè una
frazione dell'altezza dello schermo. Un pannello scritto in pixel è un
francobollo a 4K e copre lo schermo a 800x600; così un mod scrive un layout una
volta ed è la stessa forma ovunque.

Gatato: lo stesso pannello su uno schermo alto la metà ha celle grandi la metà.

### 10.3 Chi possiede cosa

Un pannello appartiene al mod che l'ha dichiarato. Un altro mod non può
ridichiararlo, non può aggiungerci niente e non può aprirlo — e il rifiuto dice
**di chi è**.

Ridichiararlo dal proprietario lo svuota, e questo è il caso che una prima
stesura sbaglia: un reload che aggiunge a quel che c'era finisce con **due di
ogni bottone**. Ma resta **aperto**, perché un mod che si ricarica non è un
giocatore che chiude la finestra.

### 10.4 Un'etichetta non si clicca

Il hit test risponde solo ai **bottoni**. Una didascalia che rispondesse ai click
manderebbe al mod eventi per una cosa che non ha mai reso cliccabile.

E scorre **all'indietro**, così fra due bottoni sovrapposti vince quello
dichiarato per ultimo — che è quello disegnato sopra, cioè quello che il
giocatore vede. Un hit test che scorre in avanti consegna il click a quello
sotto.

Provato con **nove sonde per elemento**: cinque dentro e quattro appena fuori.
Le sonde ai soli centri passano anche con un hit test che dimentica i bordi —
la stessa trappola del §4.

### 10.5 Un bottone senza id è rifiutato

Un click su un bottone senza id arriva al mod come un evento che non sa
distinguere da nessun altro. Rifiutato invece che numerato: un id generato è un
id che un mod non può scrivere.

### 10.6 Il click è un evento come gli altri

`core:ui_click` con `panel` e `id`, sullo **stesso bus** di `core:broken` e
`core:tick`. Non c'è un percorso privilegiato per la UI, che è quel che §6.3 del
DESIGN chiede.

Il gate lo prova end-to-end e headless: un mod scritto dal test dichiara un
pannello attraverso l'interfaccia host, il bus emette il click, e il handler di
**quel** mod scrive nel log.

Trovato scrivendolo: `ModHost` **non** collega il bus all'host — lo fa il client.
Il test non aveva mai usato eventi, quindi `core.event.on` falliva con un
messaggio vuoto e il mod non caricava. Ora il test collega le sue tre funzioni
come fa il client.

### 10.7 Numeri

```
0 of 27 probes landed on the wrong element
the button's rectangle differs from an empty screen over 100.0%,
and a corner behind the panel reads 46 out of the 102 it was cleared to
hovering the first button changed 100.0% of its edge and 0.0% of the other's
```

E sul percorso vero: `5 mods running, 33 block(s) can be held`, con
`console: panel 'console:menu' declared, switch at 38 20 47`.

### 10.8 Sabotaggi

Undici, tutti rossi:

| Sabotaggio | Esito |
|---|---|
| Anche le etichette rispondono al click | 2 rossi |
| Hit test in avanti invece che all'indietro | 1 rosso |
| Un mod può ridichiarare il pannello di un altro | 2 rossi |
| Un mod può aggiungere al pannello di un altro | 2 rossi |
| Ridichiarare non svuota | 1 rosso |
| Bottone senza id accettato | 3 rossi |
| «Il primo aperto» invece di «l'ultimo mostrato» | 1 rosso |
| Pannello spostato di 40 px | 1 rosso |
| Il bottone sotto il cursore non si segna | 1 rosso |
| Il reload non toglie i pannelli | 2 rossi |
| Il disegno sfasato rispetto al hit test | 1 rosso |

Il **penultimo** merita la riga, perché la prima volta è passato **verde**:
spostare il pannello di 40 px muove il disegno **e** il hit test insieme, quindi
i bottoni restano perfettamente cliccabili e nessun controllo se ne accorgeva.
Manca un'affermazione: `place_panel` dice di **centrare**. Ora è gatata contro
lo **schermo** e non contro un numero scritto nel test.

L'ultimo è l'altra metà: il disegno sfasato rispetto al hit test lascia un
bottone dove il click non arriva, e lo prende il controllo sull'evidenziazione —
perché evidenzia quello che è **disegnato** lì.

### 10.9 Un mod che muore con la mano sul bus

Il difetto peggiore di tutta M7, e l'ha trovato la **suite**: `net_client`
rosso, «il server ha smesso di rispondere per 5 secondi».

`mods/console` chiedeva `core.event.on("core:ui_click", ...)`, ma il **server**
non dichiarava quell'evento — l'avevo aggiunto solo al client. Quindi il mod
falliva su quella riga. Ma **due righe prima** aveva già registrato un handler
per `core:broken`.

E `ModHost::start_mod`, sul fallimento, faceva `script_destroy(mod.vm)` **senza**
`m_bus.drop_vm(mod.vm)`. L'handler restava sul bus con un puntatore a una VM
distrutta.

Risultato: il primo giocatore che rompe un blocco fa emettere `core:broken`, il
bus chiama dentro memoria liberata, e il server muore. **Use-after-free con
ritardo**, che è il tipo peggiore: il sintomo non è dove sta la causa, e da fuori
si vede solo silenzio.

Il sabotaggio — togliere di nuovo il `drop_vm` — non è rosso: **è un
segmentation fault**. Non c'è dimostrazione più forte.

**Il gate è stato verde per la ragione sbagliata due volte** mentre lo scrivevo:

1. il mod di prova ascoltava `core:broken`, che nel test **non era dichiarato**,
   quindi `core.event.on` falliva e nessun handler veniva mai registrato —
   «non ne resta nessuno» era vero perché non ce n'era mai stato uno;
2. la directory del mod non veniva creata, quindi falliva con «no mod.conf» e
   non arrivava a eseguire una riga.

Due modi diversi di far fallire il mod **prima** del punto che conta. Il gate ora
controlla per prima cosa che l'evento esista, cioè che l'handler venga davvero
registrato.

E il server dichiara `core:ui_click` anche se non può ancora emetterne uno: un
mod che gira su un client e non su un server sono due giochi.

### 10.10 Limiti dichiarati

**Non passa dalla rete.** Un pannello che un mod dichiara su un server non
raggiunge un client: server e client hanno la stessa `PanelSet` e le stesse
cinque chiamate, ma non c'è ancora un messaggio che porti la dichiarazione né uno
che riporti il click. È il pezzo successivo, ed è esattamente la forma per cui la
UI è ritenuta.

**Un pannello alla volta.** Due sovrapposti vogliono un ordine di impilamento, una
regola di fuoco e un click che deve scegliere, e nessuna delle tre vale prima che
esista il secondo pannello.

**Non si può cambiare un'etichetta.** Per aggiornare un numero un mod
**ridichiara** tutto il pannello. Costa una manciata di stringhe e funziona, ma è
la cosa che una `ui_set_text` risolverebbe.

**Il rifiuto va nel log, non in Lua.** Le funzioni sono generate e il generatore
non conosce la forma con il buffer d'errore che usa il bus degli eventi. Un mod
riceve `0` e la ragione la stampa il motore.

**Niente form.** Nessun campo di testo, nessuno slider, nessuna lista che scorre
— cioè esattamente quello per cui `microui` varrebbe la pena. È il lavoro §7.5.

**Nessun giocatore.** `ui_show` mette il pannello sullo schermo di chi sta in quel
mondo; non c'è modo di mostrarne uno a **una** persona, perché l'API Lua non ha
ancora la nozione di giocatore. Anche gli eventi portano solo numeri e stringhe,
mai chi.

## 11. Il pannello attraversa il filo, e il mod che non sapeva più di essere sé stesso

§10.10 diceva «non passa dalla rete» e la chiamava il pezzo successivo. Questo è
quel pezzo. Ma la cosa che vale la pena scrivere non è il messaggio: è quello che
il messaggio ha fatto emergere.

### 11.1 Il difetto che non dava nessun sintomo

Nel log di gioco dell'utente c'era questa riga, subito dopo aver rotto
`console:switch`:

```
[mod] ui: 'console:menu' belongs to mod 'console'
```

Sembra una conferma. È un **rifiuto**. La stampa `panel_ok` scrive solo quando la
chiamata fallisce, e il testo è il messaggio di errore di
`PanelSet::show`: *questo pannello appartiene a `console`* — detto a `console`.

La causa: `EventBus::emit` chiamava `script_call_handler` **senza** dire a nessuno
quale mod stesse girando. `mod_host_current()` è impostato da `CurrentMod`, e
`CurrentMod` viene creato in `ModHost::step`, in `start_mod`, in `reload` — mai
attorno a un handler. Quindi dentro un handler l'owner era la stringa vuota, e un
mod che tocca il **proprio** pannello viene rifiutato perché non è sé stesso.

Nessun crash. Nessun errore in Lua — `ui_show` torna `0` e il mod non lo guarda.
Il solo sintomo era un pannello che non si apriva.

E valeva per **ogni** callback host fatta da dentro un handler, non solo per la UI.

Il commento su `CurrentMod` diceva già *«Set around every entry into a VM»*.
Descriveva un invariante che il bus non rispettava.

### 11.2 Perché nessun gate lo vedeva

Tutti e tre i posti che esercitavano i pannelli — `panel_test`,
`mod_reload_test`, `run_hud_check` — chiamavano `ui_define`/`ui_show` al
**caricamento** del mod, dove `CurrentMod` c'è. L'handler di `core:ui_click` del
mod di prova si limitava a scrivere nel log.

Undici sabotaggi rossi in §10, e nessuno toccava il percorso vero, perché il
percorso vero è «un handler agisce sul proprio pannello» e nessun gate lo
chiedeva. Il mod `console` lo fa a ogni click, e non ha mai funzionato.

Il gate ora c'è: l'handler di `live` chiude il proprio pannello, e il test
verifica che sia chiuso davvero. Più uno che nessuno si aspettava di dover
scrivere: **quando l'handler ritorna, nessun mod sta girando**. Senza, uno scope
entrato e mai lasciato lascia l'ultimo mod che ha risposto a un evento a firmare
tutto quello che il motore fa dopo — e il primo sabotaggio su quel pezzo era
verde.

`EventBus::set_scope(enter, leave)`, sullo stesso modello di
`set_handler_error`: il bus tiene una VM e un nome, non sa cos'è un mod, quindi
non può risolverlo da solo. `ModHost` sì.

### 11.3 Il messaggio

Protocollo **12**. `PanelShow` (server → client) e `UiClick` (client → server).

`PanelShow` è **intero e non un delta**, per la stessa ragione di
`InventoryState`: un pannello è una manciata di elementi, e un delta è una
seconda fonte di verità che può divergere dalla prima. Ed è mandato **quando
cambia**, che è un'azione di un mod, non un flusso — che è tutta la differenza fra
ritenuto e immediate mode, e la ragione per cui §10 ha scelto il primo.

`open = false` significa **niente in scena**, ed è un messaggio e non un'assenza:
un client non distingue «il pannello si è chiuso» da «questo tick non è arrivato
niente», e un pannello rimasto sullo schermo dopo che il mod l'ha chiuso è un
giocatore che guarda un bottone che non fa più nulla.

`UiClick` porta il **nome del pannello e l'id del bottone**, mai un punto e mai un
indice. Non un punto per la stessa ragione per cui `Edit` nomina un blocco e una
faccia invece di una cella: dove fosse il bottone sullo schermo di quel giocatore
è aritmetica del client su una dimensione che il server non conosce. Non un
indice perché il pannello del client è vecchio di un round trip: un indice dentro
un pannello che il server ha già ridichiarato è un click sul bottone sbagliato,
in silenzio. Con gli id, un click su un bottone che la ridichiarazione ha tolto
non trova niente e si ferma — ed è gated.

Il tipo dell'elemento è **riscritto** nella struttura di rete invece di riusare
`ui::Element`, come `InventoryState::Slot` non riusa `ItemStack`. Due ragioni,
entrambe portanti: quello che sta sul filo va limitato e validato campo per
campo, e un formato di rete che è lo specchio di una struttura in memoria è a un
refactor di distanza dal cambiare in silenzio quello che i client vecchi
ricevono. I due tetti restano **un solo numero**: `protocol.cpp` ha uno
`static_assert` che il cap del filo sia quello del modello, e abbassarlo **non
compila**.

### 11.4 Contare i cambiamenti, non le chiamate

`PanelSet::revision()` è la domanda che il loop del server fa venti volte al
secondo al posto di confrontare i pannelli campo per campo.

Alla prima esecuzione il gate ha misurato **quattro** messaggi dove ne erano
dovuti tre. La ragione: ogni `define` e ogni `add` incrementavano il contatore.
Un mod costruisce un pannello con una `define` e una dozzina di `add`, e ognuna
costava a **ogni giocatore del mondo** un messaggio che descriveva un pannello
non in scena.

Ora conta solo quello che qualcuno **può vedere**: un `add` su un pannello chiuso
non conta, una `show` che rimette in cima quello che era già in cima non conta,
chiudere il pannello **sotto** non conta. Quattordici chiamate, cinque
cambiamenti da mandare.

### 11.5 Un lettore che non deve prendere un temporaneo

Scrivendo il gate del filo, tre check erano verdi per la ragione sbagliata:

```cpp
ByteReader br(bad.encode());   // il vector muore alla fine della dichiarazione
PanelShow  out;
check(!PanelShow::decode(br, out), "...");   // verde: legge memoria liberata
```

Un `decode` che rifiuta è esattamente quello che il check chiedeva. È il secondo
incontro con questo bug in una sessione, quindi non è stato solo corretto:
`ByteReader(std::vector<uint8_t> &&)` è ora `= delete`. Il sabotaggio che rimette
il temporaneo **non compila**.

### 11.6 Misure

```
3 panel message(s) to one player, 1 click(s) got through of the 5 sent
5 change(s) worth sending out of 14 calls that could have counted
a panel of 3 elements is 128 bytes, a click is 22
```

E sui binari veri, server e client su una socket:

```
[mod] probe: panel opened on a timer
a mod put a panel up: 'console:menu' from 'console', 6 element(s)
```

Diciassette sabotaggi: quindici rossi, due che **non compilano**. Suite 32/32.

### 11.7 Limiti dichiarati

**Un pannello è del mondo, non di un giocatore.** `ui_show` non ha un argomento
per dire a chi, perché l'API Lua non ha ancora la nozione di giocatore. Quindi
tutti quelli nel mondo vedono il pannello che un mod apre, e chi preme «close» lo
chiude per tutti. È la forma onesta di quello che esiste, ed è la forma che un
argomento «giocatore» cambierà.

**Nessun tasto lo chiude.** Solo il mod chiude il proprio pannello. Un ESC lato
client chiuderebbe solo la copia disegnata, e il server continuerebbe a crederlo
aperto: due verità sullo stesso pannello. Un mod che dichiara un pannello senza
bottone di chiusura intrappola il giocatore, esattamente come una formspec.

**Il click non dice chi.** `core:ui_click` porta pannello e bottone. Con un
pannello per mondo e nessuna nozione di giocatore, un mod non sa chi ha premuto.

**Il gate del click passa da una socket, non da un mouse.** `net_loopback` manda
un `UiClick` costruito a mano: quello che non è gated è che il pixel sotto il
cursore diventi quel messaggio nel loop di rete. `run_hud_check` lo verifica per
il loop in-process, e i due loop condividono `place_panel` e `button_at`.

**Restano aperti da §10.10:** un pannello alla volta; per cambiare un'etichetta si
ridichiara tutto; il rifiuto va nel log e non in Lua; niente form.

## 12. Un pannello sta su uno schermo, non su quello del mondo

§11.7 diceva: «un pannello è del mondo, non di un giocatore», e TODO OPEN-8
diceva che era una sola assenza con tre cappelli. Questa sezione la chiude.

### 12.1 Le tre facce di una cosa sola

`ui_show` non aveva un argomento per **a chi**, quindi il pannello andava a
tutti; `core:ui_click` portava pannello e bottone e non **chi** l'aveva premuto,
quindi un mod non poteva rispondere a una persona; e chi premeva «close» lo
chiudeva a tutti, perché `Panel` aveva **un solo** `open`.

Erano la stessa assenza: l'API Lua non aveva la nozione di giocatore. Gli eventi
portavano numeri e stringhe, mai una persona.

### 12.2 La dichiarazione è condivisa, la visibilità no

È l'unica frase del modello nuovo. Due giocatori che aprono lo stesso baule
guardano **una** descrizione e **due** risposte indipendenti a «è aperto», e
chiuderlo per uno non deve chiuderlo all'altro.

Quindi `Panel` perde `open` e `shown_at`, e `PanelSet` tiene una riga per
`(pannello, spettatore)`. `topmost_for(viewer)` è quella più recente fra le sue e
quelle del mondo — **sullo stesso orologio**, così un baule aperto per una
persona copre l'avviso che era su per tutti, e chiuderlo lo rimette.

Le righe sono per **nome** e non per indice: `end_reload` cancella dal mezzo di
`m_panels`, e ogni indice successivo andrebbe fatto arretrare — il genere di
contabilità che è giusta finché qualcuno non aggiunge una seconda cancellazione.

**`ui::PlayerId` è lo stesso `uint32_t` di `net::PeerId`**, deliberatamente: un
server nomina uno spettatore con la sessione che ha già in mano, senza una
seconda tabella da tenere allineata. E `kAllPlayers` **è** `kNoPeer`, che non può
mai essere una sessione vera — quindi un pannello mostrato a «nessuno in
particolare» è quello del mondo, e le due cose non si scontrano mai.

### 12.3 Una revisione per spettatore

`revision_for(v)` è quella del mondo **più** la sua. Un avviso che cambia arriva
a tutti; il baule di qualcun altro non arriva a nessun altro. E una modifica alla
dichiarazione condivisa — un'etichetta riscritta — è **un** cambiamento e
**quanti messaggi quante persone ce l'hanno su**, che è il conto onesto e non una
scorciatoia.

`kMaxShown = 512`, ed è un tetto che serviva: gli altri crescono con i **mod**,
questo cresce con i **giocatori**. Un mod che apre un pannello a chiunque passi
tiene una riga a persona.

E una sessione che se ne va viene dimenticata (`drop_player`, da
`on_disconnect`). Senza, la lista cresce di una riga per pannello per giocatore
che ne ha mai visto uno, e la sessione successiva a ricevere quel peer id
entrerebbe guardando il baule di un altro.

### 12.4 Gli eventi dicono chi

`core:pre_place`, `core:placed`, `core:pre_break`, `core:broken` e
`core:ui_click` portano un campo `player`. Un mod lo legge e lo **ripassa** a
`ui_show`: non se lo inventa mai e non tiene una tabella di giocatori.

Nel client in un solo processo il campo c'è lo stesso e vale `kLocalPlayer`. Un
mod scritto per un server gira lì senza sapere dove sta.

`mods/console` ora apre il pannello a chi ha rotto lo switch, e a nessun altro.

### 12.5 Misure

```
2 panel(s) declared, 3 viewer(s) asked, 4 change(s) for one of them
5 panel message(s) to one player, 3 to the other, 1 click(s) got through of the 5 sent
```

E con **due client veri** su un server:

```
'uno' joined as player, protocol 12
'due' joined as player, protocol 12
[mod] probe: panel opened for player 1 only
uno: a mod put a panel up: 'console:menu' from 'console', 6 element(s)
due: (niente)
```

Dodici sabotaggi, tutti rossi. Uno era verde alla prima prova: toglieva il
pannello dallo schermo del **mondo** durante una ridichiarazione, e la sezione
che lo copriva lavorava con uno spettatore vero — il sabotaggio non toccava la
proprietà. Riscritto per toglierlo da **ogni** schermo, è rosso.

Suite 32/32.

### 12.6 Limiti dichiarati

**Un pannello per spettatore alla volta.** Due sovrapposti sullo *stesso* schermo
vogliono un ordine di impilamento e un fuoco; due su schermi diversi funzionano
già.

**Nessun tasto lo chiude.** Solo il mod chiude un pannello. Un ESC lato client
chiuderebbe la copia disegnata e il server continuerebbe a crederlo aperto: due
verità sullo stesso pannello. Un mod che dichiara un pannello senza bottone di
chiusura intrappola chi lo apre, esattamente come una formspec.

**`player` è un numero e basta.** Non c'è `core.player_name(id)`, né un modo di
sapere se quel giocatore è ancora nel mondo. Un id di una sessione andata via è
un `ui_show` che non fa niente, il che è giusto e silenzioso.

**Il gate del click passa da una socket, non da un mouse.** Come §11.7.

**Restano aperti:** per cambiare un'etichetta si ridichiara tutto; il rifiuto va
nel log e non in Lua; niente form (§7.5).

## 13. Un inventario che appartiene a un posto

DESIGN §8 elenca «contenitori» in M7. La frase che li distingue da tutto quello
che c'è in `core/item` è corta: **una borsa appartiene a una sessione, un baule a
una cella**. La sessione se ne va e la borsa con lei; il baule resta dov'era, e
due persone possono averci le mani dentro nello stesso momento.

### 13.1 Una copia per spettatore sarebbero due risposte

Un `Inventory` per cella non è una struttura furba — è una mappa — ma è l'unica
forma in cui «ci stanno guardando in due» è un **fatto** e non una gara: c'è UN
oggetto, il server lo possiede, e le due sessioni mandano richieste contro lo
stesso. Una copia per spettatore sarebbero due risposte a «quanti ce n'è nello
slot 3», e la prima mossa di uno renderebbe sbagliata quella dell'altro.

Il gate lo chiede direttamente: due `find` rispondono un oggetto solo, e una pila
che uno toglie è una pila che l'altro **non vede più**.

### 13.2 Il gesto che mancava

Un baule ha bisogno di essere **aperto**, e questo protocollo aveva solo rompere
e piazzare. Un click destro a mani vuote veniva rifiutato con «your hand is
empty» — una frase su un piazzamento che nessuno stava tentando.

Ora è `core:used`, e nomina **il blocco colpito** e non la faccia accanto. Le
verifiche di portata e di «c'è qualcosa lì» girano prima, quindi quello che ne
esce è una cella che il giocatore può davvero toccare: il sabotaggio che dà
portata infinita a un uso è rosso.

Questo ha rotto `bad_link`, ed è stata una **mia regressione**, non una
scoperta. Il passo a mani vuote misura un'**assenza**, quindi non può finire
quando arriva la risposta: non ce n'è una. La prima versione aspettava
`kEditAnswer`, sei secondi, e la sequenza di modifiche è quella che la
camminata scriptata aspetta — il commento su quella camminata **lo diceva già**:
ri-temporizzarla sposta l'attraversamento, e tre run su otto tornavano con la
finestra lontana in uno stato che i controlli non si aspettavano.

Corretto mettendo quel passo **per ultimo**, con una finestra sua di due secondi
(`kUseSilence`): cinque round trip a 400 ms, e un rifiuto è un messaggio
affidabile che il server manda subito. Più corta sarebbe il guasto peggiore —
concludere «non è arrivato nessun rifiuto» prima che potesse arrivarne uno.

### 13.3 Un lato, non un posto

`MoveItem` cresce `from_side` e `to_side`. `Side::Container` significa **«quello
che ho aperto»**, che il server già sa: il messaggio non nomina nessun posto,
quindi non c'è nessun posto dentro in cui mentire. Stesso argomento di `Edit` che
nomina un blocco e una faccia invece di una cella, e di `DropItem` che non porta
posizione.

Un byte fuori dall'enum è **rifiutato**, non letto come «la borsa».

E una mossa che nomina un baule quando non se ne ha nessuno aperto non muove
niente ed è comunque **risposta** — ogni richiesta ha una risposta.

### 13.4 La revisione, e il caso che il numero da solo non copre

`ContainerSet` tiene una revisione **per cella**: due bauli in uso insieme non
devono mandare a ciascuno spettatore i cambiamenti dell'altro.

Ma un numero da solo non basta, e il sabotaggio l'ha dimostrato restando
**verde**. Due bauli toccati lo stesso numero di volte sono alla stessa
revisione, quindi un giocatore che chiude uno e apre l'altro non è «indietro» di
quel numero — e continuerebbe a vedere il contenuto di un baule in cui non sta
più guardando, mentre il server crede che abbia aperto l'altro. Serve il flag
`container_owed`, e serve un gate che porti due bauli **alla stessa revisione**
di proposito.

Scrivendo quel gate ho sbagliato l'aritmetica — davo per scontato quante volte il
primo baule fosse stato toccato — e **l'affermazione che avevo messo l'ha
preso**: `and the two are at the same revision, which is what makes this hard`
era rossa. Il setup ora ci arriva invece di assumerlo.

### 13.5 Due griglie sullo schermo

Con un baule aperto un numero di slot **smette di nominare un quadrato**: lo slot
3 del baule e lo slot 3 della borsa condividono un numero e sono due posti. Da
qui `ui::BagPoint`, che porta lo slot **e** la griglia, e `BagAction` che dice
da quale griglia parte e in quale arriva.

La borsa **non si sposta** per far posto: la riga in basso del pannello È la
hotbar, e una riga che si sposta ogni volta che si apre un baule è la riga che la
mano aveva imparato. Il baule va sopra, con un vuoto di uno slot fra i due — così
un drag che finisce in mezzo finisce su nessuna delle due, e «niente» è meglio di
«quella che capitava più vicina».

Una pila trascinata **fuori da un baule** e lasciata cadere per terra non fa
niente: sarebbe un giocatore che svuota il baule di un altro con un gesto solo, e
il baule potrebbe non essere suo. Prima nella borsa: un drag in più, e diventa
roba sua prima di essere un problema di qualcuno.

### 13.6 Misure

```
5 chest message(s) to one player, 3 to the other, 2 chest(s) in the world
a chest of 3 slots is 28 bytes, a move is 9
```

E con **due client veri** su un server:

```
[mod] probe: opened the chest for player 1 (1) and player 2 (1)
uno: a chest is open: 27 slot(s) at 41 20 47
due: a chest is open: 27 slot(s) at 41 20 47
```

Ventuno sabotaggi, tutti rossi. Tre erano verdi alla prima prova: uno perché
`emplace` su una chiave che c'è già non sovrascrive, quindi il «dai una copia a
ogni spettatore» che avevo scritto non dava nessuna copia; uno perché il flag
`container_owed` è ridondante in ogni caso tranne due bauli alla stessa
revisione; uno perché niente controllava il formato di rete del baule.

Suite **33/33** (`container` è nuovo).

### 13.7 Limiti dichiarati

**I bauli non si salvano.** Un mondo ricaricato da disco non ha nessun baule, e
quello che c'era dentro non c'è più. `ContainerSet` sta in memoria e basta.

**Rompere un baule non ne versa il contenuto.** `container_clear` toglie il baule
e quello che c'è dentro con lui. `mods/chest` lo fa e lo dice nel log, ed è la
scelta sbagliata da tenere: quello che ci vuole è «è vuoto?» in Lua, che l'API
non sa ancora chiedere, e poi rifiutare la rottura oppure buttare le pile per
terra.

**Nessuna distanza.** Un giocatore che apre un baule e cammina via ce l'ha ancora
aperto: niente chiude un contenitore quando ci si allontana. Il server sa dov'è
il baule e dov'è il giocatore, quindi è una riga — non è scritta perché non è
gated, e una riga non gated è una riga che nessuno ha misurato.

**Mezza pila.** `MoveItem` porta `count` e nessun gesto ne manda una frazione,
esattamente come per la borsa (§6).

**Un contenitore alla volta per giocatore.** Come per i pannelli.

**Il gate del drag passa da `bag_click`, non da un mouse.** Quello che non è
gated è che il pixel sotto il cursore diventi quel `MoveItem` nel loop di rete;
`run_hud_check` copre la regola, i due loop condividono la funzione.

## 14. Due difetti trovati giocando, non da un gate

Segnalati dopo il commit dei contenitori: «faccio click destro sulla chest, si
apre l'inventario ma il mouse è ancora legato al movimento del player» e «adesso
quando faccio click sul blocco dorato si apre una schermata che non capisco».

### 14.1 Un pannello che si vede e non si tocca

`host_container_show` sul client in un solo processo apriva il pannello
(`hotbar.open = true`) e **non liberava il cursore**. Il giocatore vedeva il
baule e ogni movimento del mouse continuava ad andare alla camera.

Il loop di rete lo faceva e questo no: **la stessa regola scritta due volte**, e
due copie di una regola sono due copie che prima o poi divergono. Il gate ora
chiama la funzione host che un mod chiama davvero, invece di impostare i campi
per conto suo — un gate che assegna i campi verifica la propria assegnazione.

### 14.2 Non c'era modo di chiudere un baule

Trovato inseguendo il primo. Un mod poteva chiudere un baule, un giocatore **no**:
premere E nascondeva il pannello da questa parte mentre il server continuava a
tenerlo aperto, e il `ContainerState` successivo lo rimetteva sullo schermo.

Protocollo **14**, `CloseContainer`. Non porta niente — nemmeno **quale** baule:
una sessione ne ha uno aperto e il server sa quale, e una cella nel messaggio
sarebbe una cella da confrontare con quella già nota, cioè un controllo che può
solo essere in disaccordo con sé stesso. E non ha controllo di scope né rifiuto:
chiudere una cosa che hai aperto è l'unica richiesta che non può essere
sbagliata.

Chiudere il proprio baule **non tocca quello di chi ti sta accanto**, che è
gated: è la stessa cella e sono due sessioni.

### 14.3 Il blocco dorato

Non è un difetto: è `console:switch`, e il pannello che apre è
`mods/console`, l'esempio di UI da Lua. Non si era mai visto prima di §12 —
l'handler che lo apriva era rifiutato perché il bus non diceva quale mod stesse
girando (§11.1), quindi la prima volta che ha funzionato è comparso senza
spiegazione.

Il pannello ora **dice cosa è**: `mods/console -- an example panel`, «hai rotto
il blocco giallo», «ogni parola qui è scritta in Lua», e i bottoni dicono cosa
fanno invece di «build one» / «clear them». Un pannello di esempio che non si
presenta è indistinguibile da un bug.

### 14.4 Un gate che affamava una sessione

Aggiungendo il gate della chiusura, `net_loopback` è diventato rosso in modo
**deterministico** su un check di un'altra sezione: l'osservatore non riceveva il
pannello del mondo.

La causa non era nel server. La sezione dei bauli gira per decine di secondi
contro due giocatori e pompa solo loro; il peer ENet dell'osservatore andava in
**timeout da sotto il test**. Il sintomo era un messaggio che non arriva, in una
sezione che quel client non lo guardava nemmeno.

Da qui `pump_until_all` e `pump_all_for`: una sezione lunga serve **tutti** i
client, non solo quello di cui parla. Un gate che affama una sessione a cui non
sta guardando rende rossa la sezione successiva per qualcosa che il server ha
fatto correttamente.

### 14.5 Misure

```
a chest of 3 slots is 28 bytes, a move is 9, a close is 1
```

Cinque sabotaggi: la chiusura che non arriva al server, la chiusura che chiude
quella di tutti, `peek_type` che perde il messaggio, il baule che si apre col
mouse ancora sulla camera, e il baule che si apre dietro una borsa chiusa. Tutti
rossi. Suite **33/33**.

## 15. Aprire un baule con qualcosa in mano, e un pannello che restituisce il mouse

Due segnalazioni dal gioco: «permetti di aprire la chest anche con un blocco in
mano» e «quando chiudo la finestra del blocco giallo, il mouse non riprende il
controllo della visuale».

### 15.1 Ogni click destro è prima un uso

§13.2 aveva legato `core:used` alla **mano vuota**, e la conseguenza era che per
aprire un baule bisognava posare quello che si stava portando. Sbagliato, e la
forma giusta è quella che ogni gioco di questo tipo usa: **un click destro è
prima un uso e poi un piazzamento**, e un mod che **annulla** l'uso ha
**reclamato** il click.

Quindi `core:used` è ora **annullabile**, ed è emesso **prima**. `mods/chest`
mette `ev.cancel = true`, e senza quella riga lo stesso click aprirebbe il baule
**e** costruirebbe sopra.

Un `EditPlan` porta ora **due celle**: `use_x/y/z`, il blocco toccato, e
`x/y/z`, la cella in cui si costruirebbe. Sono diverse di proposito — una che
sovrascrive l'altra è un piazzamento che finisce dentro il baule.

E l'uso **sopravvive a un piazzamento rifiutato**. Un baule che non si apre
perché la cella sopra è occupata è un baule che funziona solo a mani vuote.

### 15.2 Un rifiuto che non è di nessuno

Da qui una cosa che non era ovvia: se il piazzamento è rifiutato ma un mod ha
reclamato il click, **il rifiuto non va mandato**. Un giocatore a cui si dice
«c'è già qualcosa lì» un istante dopo aver aperto un baule sta ricevendo la
risposta a una domanda che non ha fatto.

Il livello delle sessioni quindi **tiene** il rifiuto (`SessionAction::edit_refusal`)
invece di spedirlo: se debba essere visto dipende dall'uso, e solo chi possiede
il mod host lo sa. Una mano vuota, poi, restituisce una **frase vuota** — «non
c'era niente da posare», che non è un rifiuto e non conta come tale.

### 15.3 Il pannello non restituiva il mouse

Il pannello prendeva il cursore **in ognuno dei due loop** e **nessuno dei due**
lo ridava. Chiudere il pannello del blocco giallo lasciava il giocatore davanti
al mondo con il mouse libero e nessun modo di girarsi.

La stessa regola scritta due volte, di nuovo — la stessa causa del difetto del
cursore in §14.1, una settimana di lavoro più tardi. Ora è **una funzione**,
`ui::panel_cursor`, come `bag_click`: pura, nell'header, e il gate esegue quella
che eseguono i loop.

La regola ha una memoria (`took`) e serve: riafferrare il mouse ogni volta che
non c'è un pannello lo strapperebbe a una borsa aperta, e sapere di ridarlo
richiede sapere che era stato **preso**. ESC libera il cursore senza che nessun
pannello lo abbia preso, e quel caso non deve fare niente.

### 15.4 Misure

Dodici sabotaggi, tutti rossi. Due erano verdi alla prima prova: «un uso
nell'aria è comunque un uso» (niente lo controllava) e «il pannello non
restituisce mai il cursore» — verde finché la regola è rimasta scritta a mano
dentro i due loop, cioè finché non è esistita una funzione da sabotare.

Suite **33/33**.

### 15.5 Limiti dichiarati

**Niente sneak.** In altri giochi tenere shift forza il piazzamento su un blocco
usabile. Qui un blocco reclamato non si può costruirci sopra, punto.

**Il gate della riga vera è a metà.** Che un click reclamato non produca un
rifiuto è gated sul socket; che il *piazzamento* non avvenga è gated nel mod host
(`cancelled` torna vero) e nel client in un solo processo (contro il vero
`mods/chest`). La riga `if (a.has_edit && !claimed)` del binario server non ha un
gate suo: l'armatura di test non ha un mod host, e copiarci dentro la decisione
sarebbe una seconda copia della regola — esattamente ciò che questa sezione parla
di aver pagato due volte.

---

## 16. I bauli sopravvivono a un riavvio

`TODO.md` OPEN-10 diceva una frase sola: `ContainerSet` è memoria. Un mondo
riletto da disco tornava con i blocchi e senza niente dentro — e delle due
perdite possibili è la peggiore, perché il terreno che rivedi dice che è lo
stesso mondo.

Il file del mondo aveva tre tabelle: `meta`, `chunks`, `players`. `players`
salva **posizione e sguardo**, e nel binario del server non la scrive nessuno.
Nessuna delle tre ha mai saputo niente di un item.

### 16.1 Il nome viaggia, non il numero

Una `ItemStack` su disco è `(nome, quantità)`, non `(id, quantità)`.

Un `ItemId` è denso e assegnato nell'ordine di registrazione, e su disco non c'è
niente che lo protegga: `registry_content_hash` copre **solo la tabella dei
blocchi**. E anche se coprisse gli item, il rifiuto sarebbe la risposta
sbagliata: rifiutare di aprire un mondo perché qualcuno ha aggiunto un attrezzo
è molto più duro di quello che serve a una borsa. Un id sbagliato in un chunk è
un mondo fatto dei blocchi sbagliati; un id sbagliato in un baule è il contenuto
di una cassa.

I nomi costano qualche byte a pila e sopravvivono a una lista di mod cambiata.
Il gate lo misura nel modo che conta: due registry con **gli stessi nomi in
ordine diverso**, e la pila che passa dall'una all'altra è la stessa pila.

### 16.2 Il disco ha lo stesso problema del reload, a una settimana di distanza

Un nome che il registry non conosce più non viene buttato. Viene **coniato come
orfano**: id nuovo, inottenibile, non piazza niente — che è esattamente ciò che
`ItemRegistry::end_reload` fa a un item il cui mod ha smesso di registrarlo.

È la stessa domanda: *un mondo scritto quando un mod era installato, riletto
quando non lo è.* Il reload la fa un secondo dopo, il file una settimana dopo.
Una seconda risposta sarebbe stata una seconda regola.

E la risposta si chiude da sola: quando il mod torna, `adopt_blocks` **rianima**
il nome sull'id che le pile stanno già tenendo. Il gate lo percorre tutto — mod
presente, salvataggio, mod assente, lettura, mod di nuovo presente — e chiede
anche che una pila orfana si possa **trascinare**: inottenibile non vuol dire
inamovibile, e un giocatore davanti a un baule pieno di un mod disinstallato
deve poterlo svuotare.

**Un difetto trovato scrivendolo, in codice che c'era già.** Le due strade di
rianimazione (`adopt_blocks` e `end_reload`) rimettevano a posto `owner`,
`places` e `tex` **un campo alla volta**. Finché ogni riga nasceva da
`register_item` con i default nessuno se ne accorgeva. Un orfano coniato da
`adopt_stored` porta `max_stack = 65535` — «non tagliare niente», perché il mod
che sapeva il numero vero non c'è — e la toppa campo per campo se lo lasciava
dietro: `core:stone` usciva da un file del mondo impilandosi a 65535. Ora
entrambe ricostruiscono la riga intera da una `block_item()` sola, ed è la terza
copia della stessa derivazione che diventa una.

### 16.3 Tutto o niente, e la fessura larga

Il decoder legge **tutte** le pile prima di scriverne una. Un rifiuto che lascia
mezza borsa piena è peggio di entrambe le risposte.

Cosa è rifiutato e non aggiustato:

- **Una casella oltre la fine** dell'inventario in cui si legge. Non tagliata:
  una borsa più piccola che scarta in silenzio le pile oltre il proprio fondo è
  esattamente la perdita che tutto questo file esiste per impedire.
- **Due pile nella stessa casella** — un file che dice due cose sullo stesso
  posto. Quale delle due vinca è una regola sulla corruzione.
- **Byte in più in fondo**, un byte di formato che questa build non legge, una
  quantità di zero (che è ciò che è una casella vuota, e non si scrive), un
  numero di caselle che nessun contenitore può avere.

Il gate tronca i byte buoni a **ogni lunghezza** e chiede due cose per ognuna:
che sia rifiutata, e che non abbia lasciato niente nella borsa.

### 16.4 Una riga marcia non porta via il baule

Due strade diverse, e la seconda è quella che un gate solo non copre.

Una riga che **non è affatto un inventario** non arriva mai al decoder: si ferma
su `stored_slots`. Una riga che **comincia bene e si interrompe** ci arriva, il
baule è già stato creato della misura giusta, e la lista delle pile finisce a
metà.

In tutti e due i casi il baule **resta**, vuoto, e l'errore viene riportato. Una
riga che non si legge dice che il contenuto è illeggibile; non dice che il blocco
non c'era mai. Un mondo che rispondesse cancellando il blocco perderebbe più di
quanto avesse perso la riga.

Il primo sabotaggio su questa regola è stato **verde**: il gate della riga marcia
sostituiva il blob con «not an inventory», che muore prima del decoder, quindi
nessun controllo raggiungeva il ramo che stavo sabotando. Ho aggiunto la mezza
riga e ne sono usciti due rossi.

### 16.5 Fare un baule e guardarci dentro sono due atti

`core.container_show` era l'unico modo di **creare** un contenitore, e prende un
giocatore. Quindi finché nessuno apriva la cassa nessun mod poteva metterci
niente: un baule del tesoro si riempie prima che arrivi qualcuno, e un
dispenser lo rifornisce il mod che l'ha piazzato. Nessuno dei due ha uno schermo
a cui mostrare qualcosa.

Ora sono due chiamate. `core.container_make(x,y,z,slots)` — il contenitore
esiste. `core.container_add(x,y,z,item,count)` — ci entra roba, e **torna quello
che non ci sta**, la stessa contrattazione di `Inventory::add`: dove finisce il
resto lo decide chi chiama. Una cella senza baule restituisce tutto, e mettere
`kNoItem` restituisce tutto — `Inventory::add` per «niente» risponde «non è
avanzato nulla», che è vero e l'opposto di quello che serve sapere.

`container_show` ora chiama `container_make`: era già una copia della stessa
apertura in due binari, e adesso sono due chiamate a una.

### 16.6 Il gate passa dal binario, non dalla libreria

`persistence.sh` gira il server **vero** due volte sullo stesso file. Il mod
sotto `tests/storage_mods` rifornisce il baule **solo se lo trova vuoto**: una
build senza lo strato di persistenza fallisce il gate invece di riempire la
cassa a ogni giro e passare. L'ultimo controllo della sezione è proprio quello —
che la seconda run **non** abbia rifornito niente.

Il mod fa il lavoro al primo tick e non al caricamento, ed è un limite vero:
`core.item_id("core:stone")` in cima a un mod risponde **0**, perché la forma
item di un blocco viene coniata da `adopt_blocks` **dopo** che tutti i mod hanno
parlato. Dichiarato, non nascosto: sotto in 16.8.

**Un difetto che il gate ha preso su di sé.** Aggiungere `N containers (M
loaded)` alla riga di log del server ha rotto il controllo dei chunk, che
leggeva `(N loaded)` con un `.*` greedy e prendeva il **secondo**. Il gate è
diventato rosso subito. Ora entrambe le `sed` sono ancorate alle parole davanti
al numero.

### 16.7 Misure

**Venti sabotaggi, tutti rossi.** Uno verde alla prima prova — la riga che non si
legge (16.4) — con la sua causa: il gate che pensavo lo coprisse moriva un passo
prima del ramo sabotato.

Un baule vuoto da 128 caselle costa **meno di 16 byte**: si scrivono solo le
caselle piene, e il gate lo misura invece di fidarsi.

Suite **34/34** (era 33; `item_codec` è nuovo).

### 16.8 Limiti dichiarati

**La borsa del giocatore non è ancora salvata.** Questo commit salva i
contenitori e non gli inventari delle sessioni. Il codec è lo stesso e li regge
già entrambi; quello che manca è la chiave, e non è il nome — DESIGN §4.4 dice
che *la chiave è l'identità e il nome è un'etichetta*, quindi una borsa indicizzata
per nome è una borsa che chiunque si prende scrivendo il tuo. Prossimo passo.

**La tabella `players` non la scrive nessuno.** Esiste dal M3, ha un gate nel
`storage_test`, e il binario del server non la tocca. Un giocatore riparte
sempre dallo spawn.

**Salvataggio solo allo spegnimento.** I contenitori vanno su disco quando il
server si ferma, come i chunk. Un server ucciso con `kill -9` perde le modifiche
dall'ultimo avvio.

**I drop per terra non si salvano.** Una pila lasciata cadere è un'entità ECS e
sparisce con il processo.

**`core.item_id` non vede le forme item dei blocchi durante il caricamento dei
mod** (16.6). La cura è sistemare l'ordine — adottare per blocco man mano —
e la ragione per cui non è qui è nel commento di `adopt_blocks`: un mod può
registrare un blocco e sovrascriverlo nella stessa passata, e adottare a mano a
mano conierebbe un item per una definizione che non esiste più.

---

## 17. La borsa appartiene alla chiave, non al nome

Il commit precedente ha salvato i bauli e ha lasciato aperto quello che si perde
di più: **la borsa del giocatore**. In una partita di trenta minuti è tutto
quello che il giocatore ha fatto.

Il codec c'era già: una borsa e un baule sono gli stessi byte, e questo è metà
del motivo per cui il codec è nato da solo invece che dentro `ContainerSet`.
Quello che mancava è **con che cosa indicizzarla**.

### 17.1 Non è il nome

DESIGN §4.4 lo dice da prima che costasse qualcosa: *la chiave è l'identità e il
nome è un'etichetta — due giocatori possono scegliere lo stesso nome, e nessuno
dei due può scegliere la chiave dell'altro.*

Una borsa indicizzata per nome è **una borsa che chiunque si prende scrivendo il
tuo**. La tabella è quindi `player_items(identity BLOB PRIMARY KEY, name, data,
last_seen)`, e il nome ci sta dentro solo perché un umano che apre il file possa
leggere sotto quale etichetta ha giocato quella chiave. Niente lo cerca per
nome.

Il gate end-to-end lo chiede nel modo che conta: la terza connessione entra con
**lo stesso nome e una chiave diversa** e deve ricevere una borsa vuota. Il gate
unitario aggiunge la versione più stretta — una chiave che differisce di **un
bit** è un altro giocatore.

### 17.2 Chi non ha una chiave non ha una borsa salvata

Una sessione anonima non ha niente con cui indicizzare una borsa che un'altra
connessione non possa rivendicare allo stesso modo. Quindi non ne ha una: gioca,
raccoglie, e riparte vuota. È un limite dichiarato e non una svista.

E il gate ci sta sopra, perché la stesura sbagliata è ovvia: senza il controllo,
una sessione anonima salva sotto **trentadue byte di zero** e tutti gli anonimi
del mondo condividono una borsa. Il sabotaggio che toglie il controllo consegna
al secondo anonimo quello del primo.

### 17.3 Tre momenti, una regola

Una borsa va scritta quando qualcuno **se ne va**, quando la sessione viene
**buttata giù** (silenzio, o peer scaduto), e quando è il **server** a fermarsi
con la gente ancora dentro.

L'ultimo è una riga a parte e non ha altra copertura: un server che si ferma non
produce un evento `Disconnected` per nessuno, quindi senza quella riga le
uniche borse che sopravvivono sono di chi è uscito per primo. Il gate ha una
sonda che **non saluta**: rimane connessa finché il server non finisce i tick, e
quello che c'è su disco dopo può averlo scritto solo il passaggio allo
spegnimento.

Tre momenti e **una** coppia di lambda (`load_bag` / `save_bag`) con un predicato
solo, `bag_is_keepable`. Tre copie di «quali sessioni hanno una borsa che vale la
pena tenere» sono tre occasioni di non essere d'accordo.

Il salvataggio va **prima** di `on_disconnect`, che distrugge la sessione e il
corpo di cui la borsa è un componente. Sabotato spostandolo dopo: rosso.

### 17.4 `has_joined`, e perché non basta guardare lo stato

Leggere la borsa deve succedere esattamente una volta, **prima** che esca il
primo `InventoryState`. «Era Pending l'ultima volta che ho guardato ed è Ready
adesso» è una cosa che può notare solo chi interroga ogni sessione ogni tick, e
chi se ne accorgesse un tick tardi manderebbe una borsa vuota e poi quella vera.

Quindi `SessionAction::has_joined`, con la stessa divisione di `has_edit`: il
livello delle sessioni decide, chi possiede il resto fa.

### 17.5 Un sabotaggio che ha trovato il gate, non il codice

Il controllo che un **osservatore** non sovrascriva la borsa era verde al primo
sabotaggio. La causa non era nel codice: la sonda entrava **come giocatore**.

`ClientSession::connect` scrive `cfg.scope` sopra quello che portava l'hello —
lo scope è una cosa che la sessione tiene e cambia dopo, non un campo di un
messaggio — e la sonda lo impostava sull'hello. Il gate passava e non provava
niente, e l'unica cosa che l'ha rivelato è stato sabotare la regola che avrebbe
dovuto sorvegliare.

Corretto due volte: lo scope va sulla config, e il controllo «è entrata come
osservatore?» ora legge il **log del server** invece della parola della sonda su
sé stessa.

Rifatto il sabotaggio: rosso, e con il sintomo giusto — «guardare è costato al
giocatore la sua borsa: 2 prima, 0 dopo».

### 17.6 Un verde lasciato verde, e perché

Rilassare `bag_is_keepable` **da tutti e due i lati** — lettura e scrittura — è
verde, e resta verde di proposito: l'osservatore allora **legge** la borsa prima
di riscriverla, e la riscrive uguale. Non è un difetto, è un'implementazione
equivalente.

Il difetto vero è l'asimmetria, ed è quella che il gate prende: rilassare solo
la **scrittura** è rosso. Detto qui invece di lasciar credere che il controllo
sullo scope sia coperto da entrambi i lati.

Stessa storia per `INSERT OR REPLACE` al posto di `ON CONFLICT DO UPDATE`: verde,
perché fa la stessa cosa. Il sabotaggio vero è una `INSERT` semplice, che non
riesce a scrivere sopra la riga di prima — rosso.

### 17.7 Misure

**Nove sabotaggi rossi.** Due verdi alla prima prova, entrambi raccontati sopra:
uno era un'implementazione equivalente (17.6) e uno era un **gate rotto** (17.5).
Uno verde e lasciato tale con la ragione scritta.

Il gate nuovo, `bag_persists`, è **tre vite di un server** contro un file solo e
sei connessioni: prendere, tornare, restare fino allo spegnimento, tornare
ancora, un'altra chiave sotto lo stesso nome, nessuna chiave, e un osservatore.
44 s, etichettato `slow` perché quello che misura è che cosa sopravvive a un
server.

Suite **35/35** (era 34).

### 17.8 Limiti dichiarati

**La posizione no.** La tabella `players` esiste dal M3, ha un gate nel
`storage_test`, e il binario del server continua a non scriverla. Si riparte
sempre dallo spawn, con la propria roba in mano.

**I drop per terra no.** Una pila lasciata cadere è un'entità ECS e va via col
processo.

**Solo agli eventi, non periodicamente.** Una borsa raggiunge il disco quando
quella sessione se ne va o quando il server si ferma. Un server ucciso con
`kill -9` mentre qualcuno gioca perde quello che ha raccolto da quando è entrato.

**Anonimo vuol dire senza borsa** (17.2).

---

## 18. Un baule si chiude, e quello che c'era dentro cade per terra

Le tre voci rimaste di `TODO.md` OPEN-10, e una quarta trovata leggendo il codice
mentre le sistemavo.

### 18.1 Rompere un baule non lo mangia più

`mods/chest` chiamava `container_clear` su `core:pre_break`, che prende il baule
**e il contenuto**. Il commento sopra diceva che quello era un rifiuto; non lo
era, era un blocco che si mangia una pila per essere stato rotto.

E la persistenza l'ha **peggiorato**. Prima si perdeva tutto comunque al
riavvio; da quando un baule sopravvive, quelle erano le uniche pile del mondo che
non si potevano più riavere — e ce le mandava un click.

Ora sono **due atti in ordine**: `core.container_spill(x,y,z)` versa ogni pila
per terra e lascia la cassa vuota, `core.container_clear(x,y,z)` la toglie.
Separati perché un mod può voler svuotare qualcosa che non sta distruggendo, e
perché `container_clear` ha sempre detto nella sua documentazione che *non*
versa niente: cambiargli il senso sotto sarebbe stato rompere un contratto
scritto invece di aggiungerne uno.

Sotto c'è `core.drop_item(x,y,z,item,count)`, che è il primitivo vero: una pila
per terra al centro di una cella. È quello di cui è fatto un dispenser, ed è il
motivo per cui non ho aggiunto una lettura di casella e un ritorno a due valori
che il generatore dell'API non sa fare.

### 18.2 Cinque minuti

Una pila per terra sparisce dopo cinque minuti, contati **da quando è atterrata**
e non da quando qualcuno l'ha guardata: un timer che un giocatore di passaggio
rinfresca è un timer che in un mondo con gente dentro non scatta mai.

Un mondo dove niente scade si riempie del contenuto di ogni baule che qualcuno
abbia mai rotto, e ognuna di quelle pile è un'entità che viene **streammata** a
chi ci passa vicino: il costo di tenerle lo paga chi non c'entra niente.

`expire_drops` è una chiamata a parte e non una riga dentro `step_drops`, e la
differenza è che `step_drops` ha bisogno di un mondo e questa no — una pila
scade allo stesso modo in un mondo dove non c'è nessuno, e un gate può darle un
orologio senza costruire terreno su cui far cadere qualcosa.

Il gate chiede anche la metà che si vede: che l'entità sparisca **dallo stream**.
Sabotato togliendo il `Dropped` invece di distruggere l'entità — verde, perché il
controllo guardava l'*item*. Ora conta i corpi, e il sabotaggio è rosso: una
scadenza che lascia il corpo lascia una cosa invisibile in piedi dov'era una
pila, per sempre.

### 18.3 Allontanarsi lo chiude, nello stato in cui è

Chiudere è chiudere **la vista**. Quello che c'è dentro resta dentro, e chi è
ancora lì davanti continua a guardarlo.

La distanza non è un numero nuovo: è la **portata**, `place_reach`. Quindi la
regola si dice in una frase — *un baule che non arrivi più a toccare è un baule
in cui non hai più le mani* — invece di essere una seconda distanza che nessuno
sa spiegare. Nessun rimbalzo: la chiusura è a senso unico, non riapre.

E `container_in_reach` è una **funzione libera** in `core/item/container.h`,
perché deve essere la stessa in due posti. Il server chiude il baule di chi si
allontana; il client in un processo solo chiude lo stesso baule per lo stesso
motivo. Gli ultimi tre difetti in questa zona erano tutti *una regola scritta due
volte*, e questo sarebbe stato il quarto posto.

### 18.4 Il difetto che non era nella lista

Letto nel codice mentre sistemavo il resto. `ContainerSet::erase` toglie il
contenitore, e basta. La sessione di chi ci stava guardando dentro resta con
`container_open` vero e **quella cella** in mano.

La chiusura parte lo stesso — la revisione cade a zero, la sessione è in debito,
e il messaggio dice `open=false`. Sembra a posto. Ma la sessione punta ancora
lì: quando qualcuno costruisce un **nuovo** baule sulla stessa cella la revisione
torna a uno, la sessione è di nuovo in debito, e il baule **si apre da solo**
sullo schermo di uno che non l'ha toccato.

Una chiamata sola, `ServerSessions::erase_container`, che cancella e chiude per
tutti quelli che ci guardavano. Non due chiamate in due host diversi — che è la
forma che questa zona ha già pagato tre volte.

### 18.5 Un gate che misurava niente, e come si è visto

Il controllo «un baule cancellato non si riapre da solo» era **verde al
sabotaggio**.

Il baule condannato stava cinque nodi più in là del primo, cioè **fuori
portata**: la regola del punto 18.3, appena scritta, lo chiudeva da sola prima
che arrivasse la rottura. Il gate osservava una sessione che non aveva più niente
aperto, e passava.

È la seconda volta in due commit (§17.5 era la prima) che un sabotaggio trova il
gate invece del codice, e le due hanno la stessa forma: **il gate chiedeva alla
cosa sbagliata se lo stato di partenza c'era**. Ora il baule è a portata, e c'è
un controllo in più che chiede che sia ancora aperto **dopo** che il loop ha
girato — non solo dopo la chiamata che l'ha aperto.

### 18.6 Misure

**Tredici sabotaggi rossi**, uno che non compila (togliere un asse dal calcolo
della portata lascia una variabile inutilizzata: `-Werror` lo prende prima del
gate), due verdi alla prima prova e raccontati sopra — uno era un gate rotto
(18.5), uno guardava l'item invece del corpo (18.2).

Uno verde e dichiarato tale: sotto.

Suite **35/35**.

### 18.7 Limiti dichiarati

**La riga del binario server, di nuovo.** Che `host_container_clear` passi da
`sessions->erase_container` invece che da `containers.erase` non ha un gate suo:
l'armatura di rete non ha un mod host, quindi non ha `host_container_clear`. La
*regola* è gated in `net_loopback` e la stessa riga nel **client** è gated nella
`hud` — è la sola riga del server a non esserlo, esattamente come §15.5.

**La chiamata `expire_drops` nel tick del server** non è gated, come non lo sono
`step_drops` e `collect_drops` accanto a lei: quei tre gate danno alla funzione
un orologio a mano. Quello che è gated è la funzione e la sua conseguenza sullo
stream.

**Nessuna raccolta prima dei cinque minuti.** Un mondo con mille pile per terra
le tiene tutte fino alla loro scadenza. Il numero di entità non ha tetto, e il
tetto giusto non è un timer più corto ma un limite per cella.

**La riga del loop di gioco in un processo solo** che chiude il baule quando ci
si allontana non ha un gate suo: `container_in_reach` è gated nel
`container_test` e il loop non gira in nessun gate. Quello che il gate copre è la
regola, non la chiamata.

---

## 19. La tavola delle ricette, prima della griglia

DESIGN §8 mette il **craft** in M7. Questo commit è la tabella e il confronto:
niente griglia sullo schermo, niente messaggio sul filo, nessuna casella su cui
si possa cliccare.

È deliberato ed è lo stesso ordine di `core/ui/panel.h` un pezzo di milestone
prima: una regola che si può decidere con dell'aritmetica e qualche stringa va
decisa così, perché è la metà che un gate riesce a tenere ferma e guardare. La
griglia e il filo vengono dopo, contro una tavola già misurata.

### 19.1 I nomi, di nuovo, e stavolta per un motivo diverso

Una ricetta tiene **nomi** e non `ItemId`. §16.1 lo faceva perché su disco un id
non è protetto da niente; qui la ragione è un'altra, ed è una che il mod del
gate di §16.6 aveva già trovato dolorosamente:

**un mod registra le sue ricette mentre carica, e la forma item di un blocco non
esiste ancora.** L'engine ne conia una per blocco *dopo* che tutti i mod hanno
parlato, quindi `core.item_id("core:stone")` in cima a un mod risponde **0**. Una
tabella costruita con id presi lassù sarebbe una tabella di ricette per niente.

I nomi si risolvono al momento del confronto. Costa una lookup per cella per
ricetta — qualche migliaio di hash quando un giocatore sposta una pila, cioè a
velocità umana — e compra una tabella che non si può costruire sbagliata
costruendola presto.

E una ricetta che nomina qualcosa che nessuno ha registrato non è né un errore né
un silenzio: non combacia mai, e `unresolved` la elenca, così il server lo dice
**una volta** all'avvio invece di lasciare un autore di mod a rileggersi
l'aritmetica in cerca di un bug che non c'è.

### 19.2 Il riquadro, e perché non si fa scorrere niente

«La stessa forma **ovunque** nella griglia» è la cosa che una prima stesura
sbaglia: un due-per-due in un tre-per-tre sta in quattro posti, e una ricetta che
combacia solo nell'angolo è una ricetta che i giocatori considerano rotta.

La risposta ovvia — provare il disegno a tutti gli offset — sono nove confronti
dove ne basta uno, e nove posti dove sbagliare un indice.

Quella giusta: **il riquadro minimo**. Il disegno viene ritagliato al proprio
bounding box quando viene registrato, la griglia al proprio quando viene
confrontata, e o sono la stessa cosa o non lo sono. Non c'è niente da far
scorrere.

Il ritaglio si misura da fuori senza guardare dentro la tabella: la stessa
singola pietra scritta in mezzo a un tre-per-tre viene **rifiutata come
duplicato** di quella scritta da sola. Se non fosse ritagliata, sarebbero due
ricette diverse.

### 19.3 Tre cose che una forma deve essere

**Un buco è una cella che deve essere VUOTA.** Non «qualsiasi cosa». Un
confronto che guardasse solo le celle che il disegno nomina lascerebbe fare un
oggetto con della robaccia negli spazi.

**Uno specchio è un'altra forma.** La diagonale scritta non combacia con l'altra
diagonale. Un matcher che le confonde fa una barca con una porta.

**Una riga corta è riempita di buchi**, così un mod non deve scrivere i buchi in
coda — e quelle celle restano celle che devono essere vuote, non celle che non
contano.

### 19.4 Quando due ricette combaciano entrambe

Dichiarato invece che risolto: **decide l'ordine di registrazione**, e il gate lo
misura registrando le stesse due nei due ordini e chiedendo le due risposte
diverse.

Una tabella dove due ricette combaciano con una griglia è una tabella la cui
risposta dipende dall'ordine, e ogni regola per sceglierne una («prima le
shaped», «la più specifica») è una regola che qualcuno deve imparare.

Quello che invece si rifiuta è il **duplicato esatto**, che è l'unico caso
sempre-sbagliato rilevabile senza provare tutte le griglie che esistono.

### 19.5 Una ricetta non appartiene a nessuno

Il reload è più semplice di quello degli item, e la differenza vale la pena di
dirla: un `ItemId` è tenuto da ogni baule del mondo, quindi uno abbandonato tiene
il numero e diventa orfano. Una **ricetta non è tenuta da nessuno** — niente in
un file di salvataggio ci punta — quindi una abbandonata si toglie e basta.

Un caso l'ha trovato il gate, non io: durante un reload, **rimettere la stessa
ricetta è il mod che rimette la sua, non un duplicato**. Il primo `end_reload`
era rosso perché `add_shaped` rifiutava la ricetta che il mod stava
ri-registrando. Ora una forma identica marcata stale e dello **stesso** owner
viene sostituita **intera** — e intera, non toppata campo per campo, per la
ragione che la tabella degli item ha pagato in §16.2. Un mod di un altro nome che
allunga la mano su una forma solo stale viene comunque rifiutato: «lo sto
ricaricando» è una licenza sulle proprie righe e su nessun'altra.

### 19.6 Un errore trovato dal compilatore, non da me

Il primo argomento si chiamava `result`. Il binding generato chiama `result` il
proprio valore di ritorno, e due `result` in una funzione non compilano.

Trovato così invece che leggendo il generatore, che è il modo giusto: l'argomento
adesso si chiama `makes`.

### 19.7 Misure

**Quindici sabotaggi rossi.** Due verdi alla prima prova:

- `unresolved` che non guarda gli **ingredienti**: il gate aveva solo il caso in
  cui è il *risultato* a non esistere, che è la metà in cui un refuso non
  finisce quasi mai. Aggiunto il caso, e il sabotaggio è rosso.
- il salto `if (r.pattern.size() != loose.size()) continue;` in un confronto
  shapeless. Cambiarlo in `>` resta verde, ed è corretto che resti: quella riga
  è una **scorciatoia**, non una regola. La regola è il confronto dei due
  multiinsiemi in fondo, ed è rossa (sabotaggio 50). Detto qui invece di
  inventare un gate per una riga che non decide niente.

Suite **36/36** (`recipe` è nuovo).

### 19.8 Limiti dichiarati

**Non c'è ancora una griglia.** Nessun giocatore può craftare niente: non c'è una
casella in cui mettere le cose, niente sul filo, e il risultato non si può
prendere. È il prossimo pezzo, ed è il primo che vuole le *forms* di §7.5.

**Una cella costa sempre uno.** Nessuna ricetta può chiedere due pietre in una
casella, e non c'è nessun oggetto che resti indietro (un secchio che si svuota).
Sono le due estensioni che l'API vorrà, e nessuna delle due cambia il matcher.

**Nessun gruppo.** Una ricetta nomina un item preciso, non «un qualsiasi legno».
È ciò che serve perché un compat layer regga i mod veri (§7.5), e va nella stessa
passata dei gruppi sui nodi.

**Il cablaggio del reload nel client non ha un gate suo**, come non ce l'hanno
quelli dei pannelli e degli item accanto: `mod_reload_test` ha i propri callback.
È gated il `RecipeSet`, non la riga di `on_after_reload`.

---

## 20. Un rifiuto che nessuno leggeva

Segnalato giocando: *«vicino al portale, vicino allo spawn, posso rompere i
blocchi ma non piazzarli.»*

### 20.1 Non era un difetto del piazzamento

È `mods/guard`, e fa esattamente quello che dichiara di fare: rifiuta ogni
piazzamento dentro `x ∈ [36,46], z ∈ [36,46]` — la piazza attorno al portale A —
e la sua prima riga di commento dice «no building inside the portal plaza». È il
mod che dimostra l'annullamento sul bus degli eventi, ed è lì dal M2.

Rompere funziona perché il mod ascolta `core:pre_place` e non `core:pre_break`.

### 20.2 Il difetto vero: il server risponde e nessuno legge la risposta

Il server manda **una frase** con ogni edit rifiutato. Sta nel protocollo da
quando i blocchi vanno sul filo (§2.3, «ogni edit ha una risposta, e la risposta
si legge»). `ClientSession::edit_refusal()` la tiene da allora.

**Nessuno la disegnava.** Andava a `std::printf` — nel terminale, e solo le prime
quattro. Un giocatore che clicca vede il nulla.

Un rifiuto che arriva a un client e non viene disegnato è **la stessa cosa di
nessun rifiuto**. E §2.3 aveva scritto la metà giusta della frase: la risposta
esiste e si legge — quello che mancava è che qualcuno la leggesse *al giocatore*.

Il loop in un processo solo era muto per conto suo, allo stesso modo:
`place_from_bag` tornava `false` e basta.

### 20.3 Una frase sola, non due

Le due strade producevano la stessa situazione con parole diverse — anzi, il
loop locale non ne produceva nessuna. Ora `net::refused_by_mod(breaking)` è
l'unica, e la usano sia `apply_edit` nel binario server sia `try_place` nel
client.

Un giocatore a cui vengono dette due cose diverse sulla stessa regola è un
giocatore che sta imparando due giochi.

Deliberatamente vaga su **quale** mod e deliberatamente precisa sul fatto che ce
ne sia uno: «qualcosa qui non lascia mettere giù quel blocco» dice che il mondo
non è rotto e che provare la cella accanto ha senso. Nominare il mod sarebbe
nominare un dettaglio di implementazione a qualcuno in piedi in un campo.

### 20.4 `ui::Notice`

Stato minimo e disegno separato, la stessa divisione di tutto il resto in
`hud.h`: `alpha(now)` è aritmetica a cui un gate può dare un orologio, e
`draw_notice` prende un numero invece di chiedere che ore sono.

Sopra la barra e non sopra i suoi slot: una risposta a quello che il giocatore ha
appena fatto va dove i suoi occhi già sono, e non sopra la cosa che sta provando
a cliccare. Il gate lo misura come pixel — la banda sopra la barra si accende, e
gli slot della barra **non cambiano di un pixel**.

Un `said_at` iniziale di **-1000** e non di zero: su un orologio che parte da
zero, zero vuol dire «detto al tempo zero», cioè una riga sullo schermo per i
primi quattro secondi di ogni partita.

### 20.5 Due righe che si coprivano a vicenda

`alpha` aveva un `if (text.empty()) return 0` e `say` un `if (what.empty())
return`. Sabotandole **una alla volta** erano **entrambe verdi**: tolta l'una,
l'altra salvava il caso.

Due implementazioni di una regola, e ognuna nascondeva che l'altra fosse
sbagliata. La riga in `alpha` è sparita — non c'è nessuna strada che porti a un
`said_at` recente con niente detto, perché `say` rifiuta la frase vuota. Ora
sabotare `say` è rosso.

### 20.6 Misure

**Sei sabotaggi rossi**, uno verde che ha fatto **togliere** una riga (20.5), e
uno verde lasciato tale con la ragione: il `return` anticipato di `draw_notice`
quando `alpha <= 0` è una **scorciatoia**, non una regola — le tinte sono già
moltiplicate per alpha, quindi a zero non si disegna niente comunque.

Suite **36/36**.

### 20.7 Limiti dichiarati

**Una frase alla volta.** Due rifiuti in due tick e il primo non si vede. Va
bene per quello che c'è adesso — un rifiuto arriva quando il giocatore clicca —
e non andrà più bene quando qualcosa parlerà da solo.

**Solo i rifiuti degli edit.** Niente altro nel gioco dice niente al giocatore:
non un baule pieno, non una borsa piena, non un crossing rifiutato. Il posto
dove metterli adesso c'è.

**Nessun mod può parlare.** Un `core.notice(player, text)` è un'aggiunta ovvia e
non è qui: la prima cosa che la vorrà è il craft, e va scritta con quella.

---

## 21. La griglia, e la casella che non è una casella

§19 ha deciso che cos'è una ricetta senza griglia, senza schermo e senza filo.
Questo è il resto: i quattro quadrati nell'angolo della borsa, il tavolo che li
fa nove, e il modo in cui si prende quello che ne esce. Protocollo **15**.

### 21.1 La misura è lo stato

Non c'è nessun flag «un tavolo è aperto» sul filo. `CraftState` porta `w` e `h`,
e un client a cui è arrivato 3 **sa** che c'è un tavolo.

Due campi che dicono la stessa cosa sono due campi che possono non essere
d'accordo, e quello sbagliato dei due sarebbe una griglia 3×3 su uno schermo con
il server convinto che sia 2×2. Un campo, una verità.

Ne segue anche che la griglia da 2×2 **c'è sempre**: non si apre e non si chiude,
è nell'angolo della borsa. Solo il tavolo si apre.

### 21.2 Cambiare misura la svuota, e non è ordine

Le stesse nove caselle si leggono come 2×2 o come 3×3. La casella 3 è **(1,1)**
in una e **(0,1)** nell'altra.

Una griglia che sopravvivesse al ridimensionamento si **riordinerebbe da sola
sotto le mani del giocatore**. Quindi non esiste un modo di cambiare misura senza
svuotarla: `craft_resize` fa le due cose insieme, e il gate mette una pietra
nella casella 3 di un 2×2 per chiedere che dopo non sia lì.

Dove va quello che c'era: **prima in borsa**, e quello che non ci sta **per
terra**. `craft_empty` lo restituisce invece di lasciarlo cadere, perché
`core/item` non ha un mondo in cui far cadere niente — la stessa divisione che
`ContainerSet::erase` fa con il contenuto di un baule. È il **valore di ritorno**
e non un parametro di uscita che si può non passare: chi lo ignora perde pile.

### 21.3 Tutto o niente

Prendere il risultato non è una mossa fra due caselle. È «fallo, e pagalo con la
griglia», e o succedono tutte e due le cose o nessuna.

`craft_take` prova su una **copia** della borsa. Se il risultato non ci sta
tutto, non cambia niente: un giocatore a cui viene addebitato un craft che non ha
ricevuto sta peggio di uno a cui viene detto di fare spazio, e non esiste mezzo
piccone. Il caso affilato è il posto per due dei quattro — provarlo in posto e
disfarlo dopo è la stessa cosa scritta due volte, e la seconda metà è quella che
la gente dimentica.

E il quadrato su cui finisce il trascinamento **viene ignorato**: quattro
telai trascinati su una casella che ne tiene sessantadue non hanno dove mettere
i due che avanzano.

### 21.4 La casella che non è una casella

Nel risultato non si mette niente. È una **risposta**, non un posto.

Sul filo viaggia su `MoveItem`, perché è lo stesso gesto che il giocatore fa per
ogni altra mossa e un secondo messaggio sarebbe una seconda cosa che il codice
del trascinamento deve decidere mentre il pulsante è premuto.

`kCraftResultSlot` è **`kMaxInventorySlots`**, cioè fuori dalla portata di
qualunque inventario e non solo di una griglia: 64 sarebbe una casella
perfettamente buona in una borsa da 256, e un indice che può anche voler dire una
casella vera è un indice che un bug può voler dire in silenzio.

E questo fa **una** riga fare due lavori: quella che rifiuta «una casella che
nessuna borsa ha» rifiuta anche «una mossa dentro il risultato», senza una
seconda che lo dica. Ci ero arrivato per la strada lunga — vedi sotto.

### 21.5 `Grid`, e perché non tre bool

`BagPoint::chest` era un bool quando le griglie erano due. Con la griglia da
craft ne servirebbero due, e tre dei quattro casi sono impossibili: ogni chiamante
dovrebbe sapere quali.

Ora è un enum — `Bag`, `Chest`, `Craft`, `Result` — e non può contenere
un'assurdità. `side_of()` è l'unico posto dove una griglia sullo schermo diventa
un `Side` sul filo: il disegno nomina griglie, il protocollo nomina lati, e due
copie della corrispondenza sono un trascinamento che sposta roba da una griglia
che nessuno stava guardando.

### 21.6 Un segfault che era un ordine di due righe

Il primo giro del `net_loopback` è morto. `on_disconnect` distruggeva il corpo e
**poi** svuotava la griglia — e la borsa è un componente di quel corpo.

Due righe nell'ordine sbagliato, e il crash è arrivato subito perché il gate
esiste. Vale la pena di dirlo perché la versione «funzionante» di quel bug è
peggiore: se `bag()` avesse restituito un riferimento a memoria liberata invece
di crollare, il risultato sarebbe stato una griglia svuotata dentro il nulla.

### 21.7 Due verdi, e cosa erano

**La misura sul filo.** Il sabotaggio che toglie il controllo `2x2 o 3x3` era
verde: il gate mutava **un byte** di un messaggio 3×3 valido, quindi il conto dei
byte non tornava più e il decoder rifiutava per quello. Il controllo non veniva
mai raggiunto.

Ora il gate costruisce un messaggio **ben formato** per ogni misura sbagliata —
81 caselle per un 9×9, tutte presenti — così l'unica cosa che può rifiutarlo è il
controllo sulla misura. Rosso.

**Il rifiuto di mettere qualcosa nel risultato** era verde perché ridondante: con
`kCraftResultSlot == kMaxInventorySlots`, il controllo di intervallo lo rifiuta
già. Due implementazioni di una regola, di nuovo, e ognuna nascondeva che l'altra
fosse sbagliata (§20.5, una sezione fa). La riga in più è **sparita**, e il
sabotaggio adesso è cambiare la costante — rosso su «il risultato non è un numero
di casella che una borsa possa avere».

### 21.8 Misure

**Diciassette sabotaggi rossi.** Due verdi alla prima prova, entrambi sopra: uno
era un **gate che non raggiungeva la riga** (21.7), uno una **riga di troppo**
che è stata tolta.

Suite **37/37** (`craft_grid` è nuovo).

### 21.9 Limiti dichiarati

**Un craft alla volta.** Niente shift-click per farne trentadue, e niente
ripetizione tenendo premuto. Il gesto c'è, la scorciatoia no.

**Nessuna pila nel cursore.** Prendere il risultato lo mette in borsa dove ci
sta, non sotto il mouse — la stessa scelta di §4.2, per la stessa ragione: una
pila a metà strada è stato che il client possiede e il server no.

**Niente si conserva.** Una griglia non è salvata, di proposito. Chiudere la
finestra, allontanarsi dal tavolo o disconnettersi rimette tutto in borsa; il
crash del processo no.

**Il tavolo non ha un blocco che lo protegge.** Due giocatori allo stesso tavolo
hanno due griglie — giusto — ma niente impedisce di rompere il tavolo mentre
qualcuno ci sta lavorando: la sua griglia si chiude alla distanza e non alla
rottura. Sarebbe `erase_container` per i tavoli, e non c'è.

---

## 22. Due modi di chiudere una finestra, e solo uno sapeva del tavolo

Riportato giocando: «dopo che ho aperto la crafting table, mi resta la griglia
3×3 anche quando apro l'inventario personale».

### 22.1 Il difetto

Il loop a processo singolo aveva la regola «chiudere il pannello chiude quello
che c'era dietro» scritta **due volte**: una sotto E, una sotto ESC. Quella sotto
E aveva imparato del tavolo quando il tavolo è stato aggiunto (§21). Quella sotto
ESC no: chiudeva la borsa e il baule, e lasciava la griglia com'era.

Quindi la 3×3 sopravviveva a un ESC, e riappariva alla successiva apertura della
borsa — un tavolo che il giocatore non stava più guardando, con dentro le pile
che ci aveva lasciato.

E c'era un terzo modo di andarsene che nessuno dei due copriva: **allontanarsi**.
Il server chiude una griglia lasciata indietro da `close_far_crafts` fin da
§21; questo loop chiudeva il baule alla distanza e il tavolo mai. Si poteva
attraversare il mondo continuando ad avere sullo schermo la griglia di un tavolo
lontano chilometri.

È la stessa causa di §14, §15, §18 e §21: **una regola scritta due volte sono due
regole**, e prima o poi solo una impara qualcosa.

### 22.2 La correzione

Una funzione sola, `shut_bag`, chiamata da E e da ESC. Chiude il pannello, il
baule e — se c'è — il tavolo, che **svuota la griglia in borsa**: una griglia non
è salvata e non è disegnata a pannello chiuso, quindi una pila lasciata lì è una
pila che il giocatore non vede e non può prendere.

Per la distanza, la domanda passa a `core/item`:

```cpp
bool craft_out_of_reach(const CraftGrid &grid, float eye_x, float eye_y, float eye_z,
                        float reach);
```

Chiesta alla **griglia** e non a una cella, perché per la 2×2 di tutti la
risposta è «mai»: non sta a nessun tavolo, e `at` conserva l'ultimo tavolo usato.
Una regola che guardasse solo la distanza chiuderebbe la griglia personale nel
momento in cui il suo proprietario si allontana da un tavolo che ha chiuso
un'ora prima. Ogni chiamante che dovesse ricordarselo sarebbe un altro posto in
cui dimenticarlo — che è esattamente come è nato questo difetto.

Adesso `close_far_crafts` sul server e il loop a processo singolo fanno la stessa
domanda alla stessa funzione.

### 22.3 Allontanarsi non è chiudere

Due uscite diverse, di proposito:

- **E o ESC** chiudono il pannello, e il tavolo con esso.
- **Allontanarsi** chiude il **tavolo** e lascia il pannello. Chi fa un passo
  indietro da un tavolo non ha chiesto che gli si chiuda la borsa in faccia.

Il gate controlla entrambe le metà, e la seconda controlla anche che la borsa sia
**ancora aperta** dopo: un `shut_bag` chiamato per sbaglio dalla distanza
passerebbe qualsiasi controllo che guardasse solo la griglia.

### 22.4 Misure

**Sei sabotaggi rossi**, uno verde alla prima prova.

Il verde era «chiudere il pannello dimentica il baule»: il gate controllava
`!chest_open` in una sezione dove nessuno aveva mai aperto un baule. Un flag che
il gate non ha mai messo a `true` è un flag che la funzione può dimenticare senza
che si veda. Ora il baule si apre davvero, con `host_container_show`, prima di
chiudere. Rosso.

Suite **37/37**.

### 22.5 Limiti dichiarati

**I punti di chiamata non hanno un gate.** `shut_bag` e `craft_out_of_reach` sono
misurati; le due righe che li chiamano dentro il loop a processo singolo no —
quel loop non è guidato da nessun test, e questa è la stessa lacuna dichiarata in
§18.7. Sabotare la chiamata sotto ESC lascia la suite verde.

**Il tavolo continua a non avere protezione.** §21.9 vale ancora: romperlo mentre
qualcuno ci lavora non chiude la sua griglia.

---

## 23. Un item alla volta

Chiesto giocando: «lo voglio come il classico gioco a blocchi» — il tasto destro che posa **un**
item invece di tutta la pila.

### 23.1 Perché non è il gesto del classico gioco a blocchi

Lì la pila sta **nel cursore**: sinistro la raccoglie tutta, destro ne raccoglie
metà, e ogni click destro successivo ne posa uno. Qui una pila nel cursore non
c'è, di proposito (§21.9, §4.2): è stato che il client possiede e il server no, e
un client che perde la connessione con mezza pila in mano ha mezza pila che non
esiste da nessuna parte.

Quindi «posane uno» deve **nominare la casella** in cui va, e l'unica casella che
può voler dire è quella sotto il mouse. Da cui la regola:

```cpp
BagAction bag_tap(BagPoint press, BagPoint at, bool press_has_item);
```

`press` è dove il trascinamento è iniziato, `at` è dov'è il cursore **adesso**.
Non c'è nessun rilascio, perché il tasto sinistro è ancora premuto e il gesto non
è finito: il trascinamento continua, e ogni click destro ne posa un altro finché
la pila di partenza non è vuota.

### 23.2 Il conteggio esiste già in tre posti

`move_between` ha sempre avuto un `count`, `MoveItem` lo porta sul filo da quando
i bauli esistono, e **zero vuol dire tutta la pila** in tutti e due. Mancava solo
in mezzo: `BagAction` non aveva un numero, e i due loop passavano `0` scritto a
mano.

Ora ce l'ha, con la stessa convenzione — una parola per un'idea nei tre strati
che un movimento attraversa. Il gate lo dice ad alta voce (`drag.count == 0`)
invece di lasciarlo al valore di default: un campo che nessuno controlla è un
campo che può cambiare senza che si veda.

Nessun cambio di protocollo. Il server riceveva già `req.count` e lo passava a
`move_between`; il client mandava sempre zero.

### 23.3 Cosa rifiuta

Tutto quello che rifiuta `bag_click`, per le stesse ragioni, più una cosa:

**Niente esce dalla casella del risultato.** Un craft si paga tutto in una volta
o per niente (`craft_take`), e un nono di piccone non esiste.

E la più importante: **senza un trascinamento in corso non fa niente**. Il tasto
destro da solo non è un gesto qui — non c'è una pila nel cursore da raccogliere —
e un tasto che facesse cose diverse a seconda che l'altro fosse premuto sarebbe
due gesti su un tasto. Senza questo controllo, un giocatore che clicca in giro
in una borsa aperta distribuirebbe item dall'ultima casella che ha toccato.

Fuori dal pannello non butta niente per terra: distribuire item nel mondo un
click alla volta, sopra un mondo che il pannello copre, non l'ha chiesto nessuno.

### 23.4 La quinta copia, evitata

Il loop a processo singolo aveva la traduzione «BagAction verso le due
inventory» scritta dentro il ramo del rilascio. Con due gesti che producono un
`Move`, quella sarebbe diventata la **quinta** copia della stessa regola in
questo file (§14, §15, §18, §21, §22). È una lambda `apply_move` sola, chiamata
da tutti e due.

### 23.5 Misure

**Otto sabotaggi rossi**, nessuno verde alla prima prova.

Suite **37/37**.

### 23.6 Limiti dichiarati

**I punti di chiamata non hanno un gate**, come in §22.5: `bag_tap` è misurato,
le righe che lo chiamano nei due loop no. Sostituire `one.count` con `0` lascia
la suite verde.

**Il destro non raccoglie metà pila.** Non c'è un cursore in cui metterla. Il
gesto è «tieni premuto sinistro e distribuisci col destro», non «prendi metà».

**Niente trascinamento distributivo.** Tenere premuto il destro e passare sopra
più caselle non ne lascia uno per casella: conta il click, non il passaggio.

**Sul destro il tavolo non fa un craft.** Sinistro sul risultato prende la
risposta; destro sul risultato non fa niente, invece di farne una in più.

---

## 24. Lo shift-click, che nomina un quadrato e nessuna destinazione

Chiesto giocando: le combinazioni in in stile gioco a blocchi classico. Questa è la prima e la più
usata di tutte — shift e un click, e la pila va «dove ovviamente va».

### 24.1 Perché non è un `MoveItem`

Un `MoveItem` nomina **due** quadrati. Uno shift-click ne nomina **uno**: il
giocatore ha puntato una pila, non una destinazione. Dove finisce è una
**regola**:

```cpp
bool quick_target(Side from, bool container_open, Side &to);
```

Fuori da un baule si va in borsa; fuori dalla griglia si va in borsa; fuori dalla
borsa si va nel baule aperto — e fuori dalla borsa **senza niente aperto non si
va da nessuna parte**, perché una borsa non è altrove da sé stessa.

Un `MoveItem` con un `to` inventato sarebbe un campo che non vuol dire niente, e
un campo che non vuol dire niente è un campo che può mentire. Quindi un messaggio
suo: `QuickMove`, **quattro byte** — tipo, lato, quadrato. Protocollo **16**.

E il quadrato del risultato ci viaggia dentro con lo stesso sentinella di
`MoveItem` (`kCraftResultSlot`), che vuol dire «fallo finché la griglia o la
borsa non finiscono».

### 24.2 «Dove ci sta» è una cosa che si può sbagliare in tre modi

```cpp
bool quick_move(const ItemRegistry &, Inventory &from_bag, int from, Inventory &to_bag);
```

Tre errori che una prima stesura fa, e sono quello che il gate misura:

- **Il resto.** Una destinazione con posto per quattro di una pila di dieci ne
  prende quattro e gli altri sei tornano **nel quadrato da cui sono usciti** —
  non per terra, non da nessuna parte. La pila esce intera e rientra a metà, ed è
  esattamente lì che una prima stesura perde item.
- **Le pile parziali prima.** È la regola di `Inventory::add`, non una seconda
  scritta qui: una pila che prende un quadrato vuoto mentre c'era posto accanto è
  una borsa che si riempie più in fretta di quanto contenga.
- **Una inventory sola passata due volte.** «Dove ci sta» dentro una borsa
  troverebbe il quadrato in cui la pila è già. Rifiutato.

### 24.3 Il click torna a voler dire qualcosa

Un click senza modificatori continua a non muovere niente, di proposito (§13).
Quello che si legge qui è **shift E click**, che è un gesto che prima non faceva
nessuno — e il gate lo dice: «un click SENZA shift continua a non muovere
niente» sta nella stessa sezione, perché è la metà che dice che i due gesti non
sono diventati uno.

Uno shift **trascinamento** resta un trascinamento. Il modificatore non cambia
dove il giocatore ha puntato, e una pila che si teletrasporta altrove sarebbe
l'unico gesto del pannello il cui risultato non è dove è finito il mouse.

### 24.4 Un guardiano che non guardava niente

Il gestore `QuickMove` sul server aveva il controllo «un quadrato oltre la misura
attuale», copiato da `MoveItem`. Sabotato: **verde**.

Il motivo: `MoveItem` ne ha bisogno perché ci si può **mettere** dentro qualcosa,
e un quadrato oltre la fine è un posto dove niente disegna. Uno shift-click solo
**prende** — e un quadrato oltre la fine è sempre vuoto, perché `craft_resize`
svuota la griglia a ogni cambio di misura. `quick_move` lo rifiuta già per il
fatto che non c'è niente dentro.

Due guardiani su una regola, di nuovo (§20.5, §21.7). La riga è **sparita**, e il
gate del loopback continua a dire che un quadrato oltre la fine non manda niente
da nessuna parte.

### 24.5 Misure

**Dieci sabotaggi rossi**, uno verde — quello sopra, che ha tolto una riga invece
di allargare un gate.

Suite **37/37**.

### 24.6 Limiti dichiarati

**Niente rimescolamento borsa/barra.** Nel classico gioco a blocchi uno shift-click dentro
l'inventario sposta fra la barra e il resto. Qui la barra è i **primi nove
quadrati della stessa borsa**, non un secondo contenitore, e servirebbe un `add`
che sa lavorare su un intervallo di quadrati. Con niente aperto davanti, uno
shift-click in borsa non fa niente.

**Niente shift-click verso la griglia.** Va sempre verso la borsa o verso il
baule, mai dentro l'area di crafting — come nel classico gioco a blocchi.

**I punti di chiamata non hanno un gate**, come §22.5 e §23.6: `bag_shift_click`,
`quick_target` e `quick_move` sono misurati; le righe che leggono `PLAT_KEY_SHIFT`
nei due loop no.

---

## 25. Metà pila, e il disegno che deve tornare

L'altra combinazione del classico gioco a blocchi: il tasto destro che prende **metà**.

### 25.1 Il tasto destro vuol dire due cose

Quale delle due dipende da se un trascinamento è già in corso:

- **con il sinistro premuto** posa **un** item e lascia correre il gesto (§23);
- **da solo** ne comincia uno suo, che porta **metà**.

Un solo `press` sulla sessione, più un flag su quale bottone lo regge. Non un
secondo `press`: «un trascinamento è in corso» deve restare una domanda con una
risposta sola, e due stati che dicono di due trascinamenti diversi sono due stati
che possono essere veri insieme.

### 25.2 Metà si arrotonda per eccesso

```cpp
uint16_t half_of(uint16_t count);   // (count + 1) / 2
```

Uno di tre portato e due lasciati non è metà di niente che un giocatore abbia
contato, e per difetto una pila da **uno** non porterebbe niente — un gesto che
non fa nulla su un quadrato pieno.

### 25.3 Una funzione, perché la risposta serve in tre posti

Questo è il punto interessante. Metà serve:

1. al **movimento** che viene mandato,
2. al **fantasma** disegnato sotto il cursore,
3. al **conto che resta** nel quadrato di partenza.

Scritto tre volte, `(n + 1) / 2` è tre occasioni perché il disegno e il
movimento non siano d'accordo su quale metà si è mossa. È una funzione sola, e i
tre la chiamano.

E il quadrato di partenza **non** viene disegnato vuoto, come per il
trascinamento sinistro: viene disegnato con quello che gli resta, su una copia
della griglia. Un quadrato mostrato vuoto mentre ne tiene tre è il pannello che
mente sulla borsa, e il conto sotto il cursore più il conto nel quadrato devono
fare quello che il giocatore aveva.

### 25.4 Il conto lo fa il client

Il client sa quanto c'è perché è quello che sta **mostrando**, e quello che
mostra è vecchio al più di un giro di rete. Manda un numero, non «metà»: il
server accorcia un conto più grande di quanto il quadrato tenga e risponde
comunque con **tutta** la borsa, quindi una richiesta partita da una figura
vecchia si corregge da sola al messaggio dopo. Nessun cambio di protocollo:
`MoveItem` porta già un conto.

### 25.5 Misure

**Otto sabotaggi rossi**, nessuno verde.

Suite **37/37**.

### 25.6 Limiti dichiarati

**Niente distribuzione trascinando.** Tenere premuto e passare sopra più caselle
non lascia un item (o una parte uguale) per casella: contano il click e il
rilascio, non il passaggio.

**Niente doppio click che raccoglie.** Serve una pila nel cursore, che qui non
c'è.

**Niente 1-9 per scambiare con la barra** mentre si punta un quadrato.

**Il disegno del trascinamento destro non ha un gate.** L'aritmetica sì
(`half_of`, una funzione sola chiamata da tutti e tre), ma che il pannello
disegni davvero la copia rattoppata è codice di collegamento dentro
`render_frame`, come §22.5, §23.6 e §24.6.

---

## 26. La salute, e una caduta vista da chi non simula

DESIGN.md §8 mette **salute** fra i pezzi di M7. Due commit: prima l'aritmetica,
poi il filo.

### 26.1 Un componente, non un campo sulla sessione

Stessa ragione di tutti gli altri componenti: un mob ha un corpo e nessuna
sessione, un osservatore ha una sessione e nessun corpo, e chi attraversa un
portale tiene la sessione e prende un corpo nuovo. «Quanto è ferito quel coso»
appartiene al coso.

### 26.2 La rigenerazione non è un cuscinetto

In questo motore non c'è cibo. Senza rigenerazione la prima caduta è una ferita
che il giocatore si porta finché qualcosa non lo ammazza — un numero che scende e
basta non è una risorsa, è un conto alla rovescia.

Quattro secondi a punto, dopo cinque secondi senza ferite. **Due contatori e non
uno**: da quanto non viene colpito, e quanto del prossimo punto è maturato. Uno
solo dovrebbe voler dire tutti e due, e una ferita butterebbe via progresso già
pagato.

L'attesa è quello che la rende una regola invece di un regalo: un corpo che si
cura mentre qualcosa lo sta ancora ferendo non muore mai.

### 26.3 Una caduta vista da chi non simula

Questo server **non simula la fisica** — valida le pretese (`MovementLimits`).
Quindi non c'è nessuna velocità da leggere e nessun momento in cui a un corpo
viene detto che è atterrato. C'è un flusso di posizioni, e una caduta è quello
che descrivono: scendere per un po', e poi smettere.

```cpp
float fall_step(FallWatch &w, float y);   // quanto è caduto, se questo campione ATTERRA
```

Sta in `core/physics` perché il loop a processo singolo deve fare la stessa
domanda. Quel loop **ha** una velocità e potrebbe calcolarselo per conto suo — e
allora sarebbero due regole su cosa sia una caduta e due risposte su quanto fosse
lunga questa.

Tre punti liberi: due nodi è l'altezza di ogni gradino di un mondo costruito, e
prendere danno a scendere da uno farebbe male camminare. Un punto per nodo oltre,
arrotondato per difetto — così tre nodi e mezzo è ancora gratis: il confine si
supera **cadendo più giù**, non atterrando in pendenza. Ventitré nodi è una barra
intera: esiste un'altezza che ammazza, ed è un'altezza che si impara.

### 26.4 Solo le pretese ACCETTATE

Una posizione rifiutata è una pretesa su un posto in cui quel corpo non è mai
stato. Misurare una caduta anche su quelle vorrebbe dire lasciare che un client
modificato si faccia male con numeri che il server aveva già buttato via.

### 26.5 Morire costa il posto, non la borsa

Questa è una decisione, non una dimenticanza. Questo motore non dà nessun modo di
tornare dove sei morto — niente coordinate che non puoi spegnere, niente segnalino,
niente tomba — e una pila per terra dura cinque minuti. Una morte che svuotasse la
borsa cancellerebbe il lavoro di una sessione senza appello.

Il corpo torna **intero** subito, non all'arrivo: uno che girasse a zero fra il
morire e l'essere rimesso a posto sarebbe uno che qualsiasi cosa può ammazzare una
seconda volta, e la seconda morte è quella che il giocatore non vede arrivare.

E il controllo di velocità va avvisato, con la stessa grazia di un attraversamento
di portale — perché è lo stesso fatto: **il server** l'ha spostato. La caduta in
corso viene dimenticata, o l'atterraggio allo spawn è la caduta che l'ha ucciso,
addebitata due volte.

### 26.6 Sotto il mondo

Un corpo sotto il mondo è in una caduta senza fondo, e aspettare che atterri è
aspettare per sempre. Sotto `kBelowWorld` si muore.

Il gate ci arriva **attraverso la grazia** che una morte concede, e da **fuori**
dell'estensione costruita: sotto la piazza è tutta pietra, e una pretesa dentro un
blocco solido viene rifiutata anche con la grazia — che è esattamente la regola
che rende sicuro concederla.

### 26.7 Il difetto trovato aggiungendo un messaggio

Aggiunto `Vitals` al protocollo, **cinque sezioni del loopback sono diventate
rosse** in punti che non c'entravano niente: mondi non trasmessi, client che non
entravano.

`ClientSession` ha un `default:` che chiama `fail("unexpected message from the
server")` e chiude la sessione. Un messaggio che il client non conosce **uccide la
connessione** — che è la scelta giusta, ed è anche il motivo per cui aggiungere un
messaggio server→client senza insegnarlo al client non è un lavoro a metà: è un
lavoro rotto. Il gate esisteva già e ha fatto il suo mestiere.

### 26.9 Misure

**Ventotto sabotaggi rossi** in tutto: sedici sull'aritmetica, dodici sul filo e
sul server.

**Cinque verdi alla prima prova, e tutti e cinque erano colpa del gate.** Sui
primi tre (§26 aritmetica): una ferita che buttava via il punto a metà era
nascosta dietro l'attesa che la segue, un teletrasporto rimetteva il corpo alla
quota in cui era già, e una salita era controllata solo dove si fermava.

Gli altri due:

- **«la caduta che li ha uccisi non viene riaddebitata»**: lo spawn del gate era
  alla stessa quota dell'ultima pretesa, quindi senza `fall_reset` non c'era
  comunque niente da addebitare. Ora il corpo viene ucciso **in alto** e rimesso
  **in basso**, che è l'unica forma in cui quella riga fa qualcosa.
- **«il client tiene quello che gli è stato detto»**: nessun gate leggeva
  `ClientSession::vitals()` — la sezione della salute usa l'imitazione di client
  che il test si scrive da solo. Ora il controllo è nella sezione a cinque
  sessioni, contro il `ClientSession` vero, che è quello che il gioco usa.

Suite **38/38** (`health` è nuovo).

### 26.10 La barra, e cosa deve dire

Segmenti e non una striscia liscia: quanto ne resta si deve leggere **a colpo
d'occhio** mentre qualcosa ti insegue, e una barra che scorre è una barra da
misurare. Un segmento ogni due punti — i dieci cuori che chi gioca a questa roba
conta già.

Tre cose che il gate misura, e che una prima stesura sbaglia tutte:

- **Nessuno ha ancora detto niente ≠ vuota.** Con `max` a zero non si disegna
  proprio nulla. Disegnata come vuota direbbe a chiunque sia appena entrato che
  sta per morire.
- **Il pezzo che manca si disegna lo stesso**, in rosso scuro. Una barra che
  mostra solo quello che resta non si legge come una frazione, perché non c'è
  niente contro cui leggerla — e tutti gli altri controlli passerebbero comunque.
- **Si svuota da DESTRA**, e mezzo segmento si disegna mezzo. Nove punti e dieci
  devono essere due immagini diverse: quella è la lettura su cui un giocatore
  decide se scappare.

Il conto sta in `HealthLayout::pip_rect`, come `PanelLayout::slot_rect`: chi
chiede dov'è un segmento lo chiede al layout che l'ha disegnato, non rifà
l'aritmetica.

### 26.11 Anche il loop a processo singolo

Salute, caduta e rigenerazione locali, con `core.damage`, `core.heal` e
`core.health` che fanno lì quello che fanno sul server. Le regole sono di `core`,
quindi non sono due implementazioni: sono due chiamanti.

E la stessa divisione del server fra `kill_body` e `deaths_to_serve`: chi
uccide **lo dice**, e il loop — l'unica cosa lì che possiede una posizione — è
quello che rimette il corpo a posto.

Il vecchio blocco «caduto fuori dal mondo, rimesso allo spawn» è sparito: adesso
sotto `kBelowWorld` si muore, e la morte fa già quella cosa.

### 26.8 Limiti dichiarati

**Un client che ripete una posizione a mezz'aria spezza la sua caduta in due.**
Il danno da caduta è cooperativo per costruzione: il server misura quello che il
client dichiara, e il controllo di velocità limita quanto può mentire ma non gli
impedisce di farlo.

**Niente danno da mob, da annegamento, da fuoco o da fame.** Cadere e uscire dal
mondo sono le due cose che fanno male oggi, più quello che decide un mod.

**La barra sparisce quando la borsa è aperta.** Di proposito: il pannello è
quello che il giocatore sta guardando mentre è su, e una barra dietro un velo
scurito è una barra che nessuno legge. Ma vuol dire che non si vede il danno
preso mentre si sta rovistando.

**Nessun suono, nessun lampo, nessuna animazione.** Il numero cambia e basta.
Chi guarda l'inventario o il mondo davanti a sé non ha modo di accorgersi che gli
è successo qualcosa se non guardando la barra.

**I due loop del client restano non misurati nei punti di chiamata**, come
§22.5, §23.6 e §24.6: `draw_health`, `hurt_local` e le regole di `core` hanno un
gate; le righe che le chiamano dentro il loop no.

### 26.12 Misure della barra

**Sette sabotaggi rossi.** Due verdi alla prima prova, tutti e due colpa del
gate: il fantasma del segmento vuoto non era guardato da nessuna parte, e nessun
controllo usava una salute **dispari**, quindi «mezzo segmento» e «segmento
pieno» erano la stessa immagine.

Suite **38/38**.

---

## 27. I biomi, e la differenza fra un bordo e un dirupo

DESIGN §8 (M7) chiede «mapgen con biomi». Il mapgen esisteva da M3 — deterministico,
a stage registrabili — e generava un solo posto: erba sopra, pietra sotto,
pilastri di pietra sparsi. Adesso ce ne sono quattro, e quasi tutto il lavoro
sta in una distinzione sola: **cosa si sceglie e cosa si mescola.**

### 27.1 Un bioma è un clima, non un posto

Due assi, temperatura e umidità, entrambi `value_noise` continuo su periodi
diversi (40 e 31 blocchi: con lo stesso periodo i due campi condividerebbero i
bordi delle celle, e ogni confine fra biomi nel mondo cadrebbe sulla stessa
griglia). Ogni colonna ha un clima; il bioma **più vicino** in quello spazio la
possiede.

Il più vicino invece di una tabella di rettangoli, per una ragione sola: una
tabella di rettangoli ha **buchi**. Qualche clima non cade in nessun rettangolo,
e quella colonna non ha nessun blocco di superficie — un buco nel mondo prodotto
da una casella dimenticata in una tabella scritta a mano. Ogni punto ha un più
vicino: qui non c'è niente da dimenticare.

### 27.2 Si sceglie il blocco, si mescola la forma

Quale bioma possiede una colonna è una funzione **a gradino**, e un gradino nel
blocco di superficie è esattamente giusto: l'erba finisce, la sabbia comincia, e
il bordo si vede.

Un gradino nell'**altezza** del terreno è un dirupo inventato da una tabella
invece che dal rumore. Quindi i due parametri di forma di ogni bioma — `base`,
di quanto sta sopra la piazza, e `relief`, di quanto moltiplica il rumore —
non si scelgono: si **mescolano**, pesati sulla distanza nello spazio del clima.
La miscela è continua perché il clima è continuo, quindi nessuna cucitura può
comparire nel terreno che non ci fosse già nel rumore.

Misurato sullo stesso mondo (128, seme 1), salto massimo fra due colonne vicine:

| generatore | salto massimo |
|---|---|
| senza biomi (rilievo 1 dappertutto) | **6** blocchi |
| con i biomi, forma mescolata | **7** blocchi |
| con i biomi, forma **scelta** | **20** blocchi |

Venti blocchi è un muro. Il gate chiede ≤ 8 e diventa rosso sul terzo caso.

Il peso è l'inverso della **quarta** potenza della distanza, non del quadrato.
Col quadrato, stando in mezzo alle vette si prende tre quarti della loro altezza
e un quarto di quella di tutti gli altri: nessun bioma somiglierebbe mai a sé
stesso.

La prima stesura sfocava il rilievo su un intorno di 9×9 colonne — 81 campioni
di rumore per colonna. Mescolare nello spazio del **clima** invece che in quello
dello spazio dà la stessa continuità con **un** campione per colonna.

### 27.3 Il pareggio si rompe sul nome, non sull'ordine

Due biomi sullo stesso punto di clima devono avere un vincitore, e a deciderlo è
il **nome**. Se decidesse chi è stato registrato per primo, lo stesso seme
darebbe due mondi diversi su due build che caricano i mod in ordine diverso.

È l'opposto esatto degli **stage**, dove l'ordine di registrazione *è* il
contratto — uno stage vede quello che hanno scritto quelli prima di lui. I due
elenchi vivono nella stessa classe e hanno regole opposte, quindi lo dicono
tutti e due nel loro commento.

### 27.4 Il rumore non arriva agli estremi

Il value noise bilineare non passa molto tempo vicino a 0 o a 1: il centro di
una cella è la media di quattro numeri uniformi, e le medie si affollano in
mezzo. Lasciato stare, i due climi estremi sono abbastanza rari che un mondo
intero può venire fuori senza deserto. Uno stiramento di contrasto (×1.5 attorno
a 0.5, con taglio) li rimette fuori.

Misura su **sei** mondi 128 messi insieme, piazza esclusa:

| bioma | con lo stiramento | senza |
|---|---|---|
| core:plains | 32.4% | 47.2% |
| core:desert | **15.2%** | **7.9%** |
| core:tundra | 18.5% | 14.1% |
| core:peaks | 33.8% | 30.8% |

Il gate chiede che nessun bioma stia sotto un decimo del terreno, e diventa rosso
senza lo stiramento.

Sei semi e non uno perché **quale** bioma è raro in un mondo solo è affare del
seme: il seme 6 dà il 4.3% di deserto e il seme 2 ne dà il 27.6%. Un gate su un
seme misurerebbe quello invece del generatore.

### 27.5 La piazza non si muove

La piazza resta piatta e resta erba, qualunque cosa dica il clima lì. I gate dei
portali ci stanno sopra da M4: una scena che si sposta perché questo seme ha
tirato un deserto è un gate che fallisce per una ragione che nessuno riesce a
leggere.

Il controllo è **uno** e non due: il blocco a `y = kGroundTop - 1` è erba su
tutte le colonne della piazza. Se la piazza si alzasse lì ci sarebbe il
riempimento, se si abbassasse ci sarebbe aria, e se cambiasse bioma ci sarebbe
sabbia. Chiedere del blocco **più alto** sarebbe chiedere dei pilastri, che
sulla piazza ci stanno e devono starci.

### 27.6 I blocchi nuovi vanno in coda

`core:dirt`, `core:sand`, `core:snow` sono registrati **dopo** `core:glass` e
non in mezzo. Gli id sono posizioni: inserirne uno nel mezzo rinumera ogni
blocco dopo di lui, e con lui ogni mondo già salvato con i numeri vecchi. Il gate
lo dice esplicitamente adesso — 1..4 stanno dove stavano, e i tre nuovi hanno
preso 6, 7, 8.

### 27.7 Un gate che passava per caso

«Un blocco in più da uno stage in più cambia l'hash» metteva pietra in
`(5, 40, 5)`. Il primo sabotaggio — rilievo scelto invece che mescolato — ha
alzato le montagne fin lassù, e il gate è diventato rosso per una ragione che non
c'entrava niente con quello che misura: a quella colonna la pietra c'era già.

Adesso lo stage scrive **qualcos'altro** rispetto a quello che trova. La
proprietà misurata è «uno stage in più cambia il mondo», non «a y=40 c'era
aria» — che era una proprietà dei biomi di oggi, non del contratto degli stage.

### 27.8 Misure

**Nove sabotaggi, tutti e nove rossi, zero verdi alla prima prova**: forma scelta
invece che mescolata, rumore non stirato, pareggio rotto sull'ordine, un bioma
registrato sopra un altro, pilastri sempre di pietra, riempimento profondo un
blocco di troppo, piazza lasciata al clima, un blocco nuovo inserito in mezzo
alla tabella, e il blocco di un bioma cercato male nel registry.

Generare un mondo intero, biomi compresi: **3.6 ms** a estensione 64,
**15.6 ms** a 128 (128×64×128 = 1.05 M blocchi).

Hash del seme 1 spostato di proposito, nello stesso commit:
`5b194e88b8935d1b` → `e02fc46041a780ef`.

Suite **38/38**.

### 27.9 Limiti dichiarati

**Il bioma non passa dal filo.** Un client connesso adotta la tabella dei blocchi
e riceve i chunk: non gira il generatore, quindi non sa in che bioma sta. La riga
F3 di `ffc923f` dice il **blocco** sotto il mirino, non il posto — e OPEN-11
resta aperto per la stessa famiglia di ragioni.

**Niente alberi, niente acqua, niente caverne.** Un bioma qui cambia quattro
cose: il blocco di superficie, il riempimento sotto, di che cosa sono fatti i
pilastri, e la forma del terreno. La neve è un blocco di superficie, non un
manto; il deserto non ha dune e la tundra non ha ghiaccio.

**Nessuna API per i mod.** `BiomeSet::add` esiste in C++ e non in Lua: un mod può
aggiungere uno **stage** al mapgen — quello sì, da M3 — ma non un bioma.

**Il clima non ha altitudine.** Temperatura e umidità sono funzioni di x e z
soltanto: in cima alle vette fa esattamente caldo come ai loro piedi, e non c'è
neve in quota se il clima lì non è quello della tundra.

**La piazza è ancora una costante nel core.** `kPlazaX0..kPlazaZ1` vivono in
`worldgen.cpp` e valgono per ogni mondo, anche per quelli che non hanno nessun
portale da proteggere.

---

## 28. Il ciclo giorno/notte, e il secondo orologio che non c'è

### 28.1 Lo shader era pronto da M1

`uDaylight` sta in [`shaders.h`](../../src/client/render/shaders.h) da quando esiste il
mesher: scala **solo** il canale del sole, lascia stare la luce dei blocchi, e
prende il `max` dei due invece di sommarli così che una torcia non bruci una
superficie già illuminata. Tre decisioni giuste, scritte in un commento, e
nessuno gli ha mai detto che ora fosse — `set_daylight` non era chiamata da
nessuna parte e il valore restava 1.0 per sempre.

Quasi tutto questo commit è dirglielo, e la parte che non lo è sta nell'ora.

### 28.2 Un'ora è un punto su un cerchio

Ogni difetto in questa aritmetica è un posto dove qualcuno l'ha trattata come un
punto su una retta. `0.99` e `0.01` distano due centesimi, e una sottrazione dice
novantotto: `clock_gap` esiste solo per quello, e sabotandolo in una sottrazione
tre controlli diventano rossi.

La cucitura di mezzanotte sta **dentro** ogni misura invece che al bordo: il
campione a `i = 0` si confronta con quello appena **prima** di mezzanotte, non
con quello dopo. Un test che campiona da 0 a 1 e confronta i vicini misura tutto
il giorno tranne l'unico punto dove è facile sbagliare.

### 28.3 Il secondo orologio che non c'è

Il client **non ha un orologio**. Non interpola e non avanza fra un messaggio e
l'altro: tiene l'ultima ora che gli è stata detta finché non arriva la prossima.

Il costo è misurato invece che dichiarato. Attraverso un intervallo di
comunicazione il sole cambia di **0.0061** della sua corsa; il passo fra due dei
sedici livelli di luce che un blocco può avere è **0.0667**. Un decimo di un
livello di luce, per la cosa più veloce che il cielo fa in tutto il giorno.

La prima stesura aveva un `clock_follow`: correzione a velocità variabile, mai
all'indietro (il sole che risale è un difetto che nessuno segnalerebbe come un
orologio), snap oltre una soglia. Sei controlli e un gate. **Cancellato**, quando
la misura ha detto che non c'era niente da lisciare — una regola che nessuno
esegue non è una regola, e un orologio di qua per coprire un decimo di livello di
luce sarebbe stato un secondo orologio con cui litigare.

E si è portato via un problema: **la lunghezza del giorno non viaggia**. Se
viaggiasse, il client sarebbe libero di non essere d'accordo su quanto va veloce
il sole fra due aggiornamenti, che è esattamente la deriva che gli aggiornamenti
servono a togliere.

### 28.4 Quanto spesso, contato in giorni e non in secondi

`kTellEvery` è **un milleduecentesimo di un giorno**, non un secondo. Un server
con un giorno da venti secondi muove il cielo sessanta volte più in fretta e lo
dice sessanta volte più spesso, senza una seconda regola che lo dica — e
`--day-length 20` serve proprio a guardare tutto il ciclo senza prenotare il
pomeriggio.

Contato come accumulo e non come confronto fra due orari, così non c'è nessun
wrap da sbagliare quando un server si pianta per mezzo giorno.

### 28.5 L'osservatore riceve l'ora, la barra no

`Vitals` va solo a chi è entrato; `Time` va a ogni sessione pronta, osservatori
compresi. Chi guarda dentro questo mondo attraverso un portale sta guardando il
suo cielo, e un cielo fermo all'ora in cui ha aperto la finestra è il mondo
sbagliato disegnato benissimo. È l'unica differenza fra i due messaggi, ed è un
gate.

### 28.6 Due mondi, due orologi — e il gate che li distingueva con due letterali

`dual_session` distingue i due mondi contando i pixel di cielo A e cielo B sullo
schermo, e i due cieli erano due letterali con scritto accanto «un arancione».
Adesso il cielo appartiene all'orologio, quindi i due letterali sono diventati
due **ore**: mezzogiorno e tramonto. `build_local_session` prende un'ora al posto
di un colore, e il gate distingue i mondi con una cosa che anche il giocatore
legge sullo schermo.

Ed è il gate che ha trovato il difetto subito: applicando l'orologio alla sola
sessione attiva, il mondo dentro il portale prende l'ora sbagliata — **cinque
giri su dieci** con il cielo sbagliato dopo lo swap. È lo stesso singleton che M5
aveva già tolto al cielo e alla portata di vista, rimesso dentro da una riga
scritta con `s.act()` invece che con un ciclo.

### 28.7 Il pavimento della notte

`kNightFloor` è 0.22 e non è un gusto: chi non vede non gioca, e un mondo che
diventa nero ogni venti minuti è un mondo che si chiude. La luce dei blocchi non
si abbassa affatto, quindi una torcia si legge lo stesso contro quel fondo.

Alba e tramonto insieme sono il **26.5%** del giorno, misurato: abbastanza per
essere due momenti e non un interruttore.

### 28.8 Misure

**Dieci sabotaggi, tutti e dieci rossi** — ma due gate hanno dovuto essere
riparati prima, e sono i due pezzi interessanti di questo commit.

Il primo era **verde alla prima prova**: `clock_wrap` che restituisce 1.0 non
veniva preso da nessuno. Il controllo spazzava duemila tempi da −0.5 a +2.1, e il
caso per cui quella guardia esiste è un negativo **minuscolo** — `fmod(-1e-17, 1)`
lo restituisce tale e quale, sommare uno lo arrotonda a 1.0 esatto, e 1.0 non è
nell'intervallo promesso a ogni chiamante. Un centesimo di secondo prima di
mezzanotte non è il caso; un quintilionesimo sì. Aggiunta una riga con sei tempi
denormali, e il sabotaggio è diventato rosso.

Il secondo non compilava nemmeno: togliendo la chiamata a `clamp01` la funzione
resta inutilizzata e `-Werror` ferma la build. Sabotato il **corpo** invece della
chiamata, cinque controlli sono diventati rossi.

E un nome che prometteva troppo: «mezzanotte è il pavimento e non il nero»
restava verde con il pavimento a zero, perché diceva solo che mezzanotte *è* il
pavimento. Adesso sono due righe che dicono due cose.

Protocollo **18**. `Time` sono **tre byte** — tipo più sedici bit, perché
l'intervallo è noto e un float spenderebbe quasi tutti i suoi bit su esponenti di
numeri che non possono capitare. Un codice è un cinquantesimo di secondo di un
giorno da venti minuti.

Suite **39/39** (`daylight` è nuovo).

### 28.9 Limiti dichiarati

**Non c'è un sole in cielo.** Nessun disco, nessuna luna, nessuna stella: il
cielo è un colore piatto e il sole è un moltiplicatore. Un portale non mostra un
sole dall'altra parte perché non ce n'è uno da nessuna parte.

**La luce del sole non si ricalcola mai.** Il flood fill resta quello di M1 e
l'ora scala il canale nel vertex shader. Vuol dire che dentro una caverna la
notte non si vede, e che il `sun 15` della riga F3 non cambia con l'ora — è la
luce **scritta nel blocco**, non quella che arriva. I due numeri stanno accanto
sulla stessa riga apposta.

**Niente mob, niente spawn notturno, niente sonno, niente orologio in mano.** La
notte è più scura e basta.

**Nessuna API per i mod.** `ServerSessions::set_time` esiste in C++ e non in Lua:
un mod non può ancora dire «adesso è notte».

**I gate girano a mezzogiorno.** Solo i due loop che possiedono il mondo fanno
avanzare l'orologio; ogni altro gate resta a `kNoon`, che è esattamente
l'immagine che aveva prima del ciclo. Dichiarato perché vuol dire che nessun gate
guarda un cielo in movimento.

**La riga F3 e i due flag sono glue non misurata**, come §22.5, §23.6, §24.6 e
§26.8.

---

## 29. Il server che si arrende a un join che il client sta ancora facendo

Trovato dal commit precedente, non da una sessione di caccia: `bad_link` è andato
rosso dentro un `ctest -j4` e stavolta il gate ha **tenuto i log**. È il secondo
run da quando li tiene.

### 29.1 Quello che dicono i log

    client:  could not join: timed out -- no answer in 15 seconds,
             still handshaking (round trip 382 ms)
    server:  'player' joined as player, protocol 18, 1 features agreed
    server:  dropped 'player': nothing heard for 12 s
    shaper:  to the client  42 in, 25 forwarded, 16 dropped (38.1%)

Il join non è stato rifiutato e non è fallito all'andata: il server ha accettato
l'hello, l'ha scritto nel log, ha mandato il welcome — e poi ha buttato fuori il
giocatore perché non diceva niente, mentre il client stava ancora aspettando
quel welcome. Il 38% in quella direzione è sfortuna del seme su 42 datagrammi;
la sfortuna è l'occasione, non la causa.

### 29.2 Due pazienze scelte in due file

`kSessionSilence` valeva 12 secondi in `session.h`. La finestra del client è
`round_trip * kHandshakeRtts` in `client_session.cpp`, con un pavimento a 10.

Oltre i **300 ms** di round trip la seconda supera la prima, e da lì in poi il
server si arrende **sempre** per primo: la finestra del client non può più
significare niente, perché quando scade dall'altra parte non c'è nessuno da tre
secondi. Su loopback l'ordine è quello giusto e non si vede mai.

È la stessa forma di tutto quello che questa milestone ha trovato: **una regola
scritta due volte**. Qui le due copie non erano nemmeno d'accordo su cosa
stessero misurando.

### 29.3 E la regola sotto la costante era sbagliata, non solo il numero

`ServerSessions::silent()` contava dal momento in cui la sessione diventa
`Ready`, e il commento diceva perché fosse sicuro: «un client che gioca manda
una pretesa di posizione venti volte al secondo». Vero di un client che **gioca**.
A questo non era ancora stato detto che poteva.

Un client che non ha ricevuto il welcome non ha niente da dire, e il welcome è
il messaggio **più grosso** del protocollo — tabella dei blocchi più offerta
media, frammentato — quindi è esattamente quello che su un link con perdita sta
ancora venendo ritrasmesso mentre quel timer corre.

Quindi non è una soglia da allargare. Il silenzio va contato **dalla prima parola
dopo il welcome**, e finché quella parola non arriva la pazienza è un'altra.

### 29.4 Il server non misura il round trip, ma ha misurato quella stretta di mano

La pazienza da superare è quella del client, `kHandshakeRtts` round trip. Il
server non è il lato che misura un round trip — ma ha **quanto è durata la
stretta di mano appena finita**, su quella connessione: la connect di ENet più
lo scambio di chiavi, sullo stesso link.

Misurato: **377 ms** su un link da 400 ms di round trip. Una stretta di mano è
circa un round trip, quindi il server può usare la costante del client
direttamente invece di sceglierne un'altra accanto:

```cpp
inline constexpr double kJoinSilenceHandshakes = kHandshakeRtts * 1.5;
```

La metà in più è il margine, e non è arrotondamento: il server deve
**sopravvivere** al client, non pareggiare con lui. Il client è il lato che può
riprovare, e non può riprovare un server che se n'è già dimenticato. Il gate lo
chiede su duecento link da 5 ms a un secondo, non in un punto solo: la pazienza
del server non scende mai sotto **1.50×** quella del client. Togliere la metà fa
diventare rosso quel controllo — pareggiare non basta.

Su loopback la stretta di mano dura frazioni di millisecondo e il pavimento resta
`kSessionSilence`: niente cambia dove niente era rotto.

### 29.5 Un campo invece di un confronto fortunato

`spoke_since_welcome` è un campo e non `last_heard_at > since`. Le due
espressioni oggi dicono la stessa cosa, ma solo per l'ordine in cui due
assegnamenti capitano a cento righe di distanza: `last_heard_at = now` sta in
cima a `on_message`, `s.since = now` sta dentro il ramo dell'hello. Spostare una
delle due righe invertirebbe la regola in silenzio.

### 29.6 Come si misura una stretta di mano lenta senza un link lento

Il gate del loopback non può rallentare la rete. Rallenta la **stretta di mano**:
un client che si connette e sta fermo un secondo prima di dire hello è un client
di cui il server misura `joined_after = 1006 ms`, che è esattamente il numero da
cui deriva la pazienza.

E poi la regola non si aspetta, si **interroga**: `silent()` è una domanda pura,
quindi il gate gliela fa a tempi sintetici — a `kSessionSilence + 1 s` la
sessione è ancora lì, a `kSessionSilence + joined_after * kJoinSilenceHandshakes`
non c'è più. Quello che si misura è la regola, non l'orologio.

### 29.7 Misure

**Sette sabotaggi, tutti e sette rossi**, ognuno sul controllo che gli
corrisponde: la regola del silenzio che torna al numero fisso, la pazienza del
join data a tutti per sempre, la pazienza del join uguale a quella del giocatore,
una parola del client non contata come tale, il welcome contato come se avesse
parlato lui, la stretta di mano non misurata, e il server che pareggia con il
client invece di sopravvivergli.

Suite **41/41**.

Il numero nuovo nel log del server, perché è quello che decide quanto quella
sessione ha per rispondere al suo welcome e su quel link è l'unica cosa che i due
log sanno del link:

    'giacomo' joined as player, protocol 18, ..., handshake 377 ms

### 29.8 Limiti dichiarati

**Il posto resta occupato più a lungo.** Un client su un link da 400 ms che si
connette, viene ammesso e non parla mai tiene il posto per circa **22 secondi**
invece di 12. È il prezzo, ed è dichiarato: la scelta è fra un posto tenuto
troppo e un giocatore buttato fuori a metà di un join.

**La stretta di mano stima il link, non lo misura.** Un client lento a mandare
l'hello per ragioni sue — una macchina carica, non un link lento — si compra la
stessa pazienza. Non fa male a nessuno se non a un posto, ed è la ragione per cui
il numero sta nel log del server.

**Il caso vero resta coperto solo da `bad_link`.** Il gate del loopback prova la
regola con una stretta di mano lenta su un link veloce; il difetto originale
richiede un link lento, e lì non c'è nessuna asserzione diretta — solo che lo
scenario intero passi.

---

## 30. Il suono, e la porta da cui esce

DESIGN §8 (M7) chiede «audio con attenuazione». L'attenuazione è aritmetica, e
sta in `core` senza nessuna libreria audio sotto: il backend `snd_*` del client
riceve un guadagno e un pan e fa quello che gli si dice. Stessa divisione di
`gfx_*` e per la stessa ragione — quello che vale la pena controllare sono i
numeri, e un controllo che deve aprire un dispositivo audio è un controllo che
su una macchina di build non gira.

### 30.1 La curva, detta come una frase

A metà della portata, un quarto del volume. È la forma della legge dell'inverso
del quadrato, ed è una cosa che qualcuno può verificare a orecchio — quindi
`kRolloff` vale 2 perché è il numero che produce **esattamente** quello, non
perché sembrava giusto.

E la curva arriva a **zero alla portata**, non a 0.11. Non è cosmesi: una curva
che alla propria portata vale ancora un decimo o taglia in modo udibile, o
lascia ogni suono mai partito a tenere una voce per sempre. Il gate misura il
salto massimo su centomila campioni proprio al bordo.

### 30.2 Il pezzo che questo motore ha e un mondo piatto no

Un suono nel mondo dall'altra parte del portale arriva **attraverso**
l'apertura. Due metà, e la seconda è quella che morde:

- la distanza è il **giro**, non il passaggio: orecchio → apertura, più apertura
  di là → sorgente;
- la direzione è quella della **porta**, non della sorgente. Un suono che entra
  da una porta alla tua destra è alla tua destra, dovunque sia la stanza dietro.

Il difetto che la funzione esiste per impedire, misurato: una sorgente che nel
mondo di là sta alle **stesse coordinate** dell'ascoltatore. Calcolata come se
fosse in questo mondo dà distanza zero e volume pieno — un suono assordante da
una cosa che è cento nodi oltre la soglia. Attraverso l'apertura non si sente
per niente, perché le due gambe sommate superano la portata.

Stesso argomento della camera virtuale, e stesso modo di sbagliarlo.

### 30.3 Tre gate verdi al primo tentativo, e uno ha fatto cancellare una riga

**Il punto più vicino dell'apertura era il centro.** Sabotando
`nearest_on_opening` per restituire sempre `s.center`, tutto restava verde:
nella scena del gate l'ascoltatore sta in piedi davanti alla metà dell'apertura,
dove il clamp non morde mai. Adesso c'è una sezione sua, con un'apertura larga e
l'ascoltatore di lato — ed è dove si vede che una porta larga è più vicina al suo
**bordo** che al suo centro.

**Il suono partiva a piena ampiezza e nessuno se ne accorgeva**, perché due
guardie coprivano la stessa regola. C'erano `out.front() = 0; out.back() = 0;`
dopo l'inviluppo, e l'inviluppo è già zero esatto ai due estremi — `t/kAttack`
con t=0, e `(1-d)²` con d esattamente uno. Quindi la riga in più non faceva
niente **tranne** nascondere il sabotaggio della riga vera. **Cancellata**, come
la guardia doppia di §20.

E il controllo che restava era comunque sbagliato: misurava il **salto fra due
campioni**, che è la misura giusta per un tono e priva di senso per il rumore,
dove due campioni vicini devono essere diversi. Diceva 16089 per un fruscio e lo
chiamava un click. Adesso misura l'**inviluppo** — il campione più forte nel
primo centesimo e nell'ultimo — che funziona per tutti e due.

**Lo sweep poteva salire invece che scendere.** Il commento in `tone.cpp` dice
che `sin(2π f(t) t)` sale al doppio della frequenza che gli si dà; era una
affermazione e non una misura. Adesso il gate conta i **passaggi per lo zero**:
ce ne sono due per ogni ciclo, cioè due volte l'integrale di f, e con
l'espressione sbagliata la frequenza istantanea arriva a 220 Hz, si inverte a
due terzi del suono e torna indietro — 33 attraversamenti invece di 50.
Misurato: **50 contro i 50 che lo sweep chiede**.

### 30.4 Suoni generati, non registrati

Non c'è nessun `.wav` in questo repository. Un suono è una frequenza, una
lunghezza e se è intonato o no, e i campioni si generano — la stessa cosa che
`NodeDef` fa con un colore per avere una texture prima che qualcuno ne disegni
una.

E c'è una seconda ragione: un file nel repository è un file su cui due client
possono non essere d'accordo, e il percorso che risolverebbe la cosa
(content-addressed, streamato, in cache) esiste per le texture e non copre
ancora i suoni. Un suono generato è uguale sulle due macchine per costruzione, e
il gate lo chiede.

### 30.5 Misure

**Dieci sabotaggi, tutti rossi** — dopo che tre erano verdi alla prima prova,
tutti e tre colpa del gate, e uno ha fatto cancellare una riga di codice invece
di aggiungerne una al test.

Il pan di raylib usa la stessa convenzione di questo motore — −1 sinistra, 0
centro, +1 destra — quindi nel backend non c'è nessuna conversione da scrivere
al contrario. Detto ad alta voce perché è esattamente il tipo di riga che resta
sbagliata per un anno.

Suite **42/42**.

### 30.6 Limiti dichiarati

**Tre suoni.** Rompere, piazzare, farsi male. Nessun passo, nessun ambiente,
nessuna musica.

**Nessuna API per i mod.** `SoundBank` è una tabella e non un registry, e la
differenza è dichiarata: un registry con una `add()` che nessuno chiama
sembrerebbe una cosa che si può estendere.

**Una voce per suono.** Due rotture nello stesso istante non si sovrappongono —
la seconda riparte la prima. raylib vuole un alias per suonare la stessa `Sound`
due volte insieme, e non c'è.

**Niente audio nei gate.** Il dispositivo si apre solo per i loop che una persona
gioca — `--dual` è nominato esplicitamente perché è una finestra misurata e
giocata insieme. Vuol dire che **nessun gate sente niente**: quello che è
misurato è l'aritmetica, e i punti di chiamata restano colla non misurata come
§22.5, §23.6, §24.6, §26.8 e §28.9.

**Il suono attraverso il portale ha un solo punto di chiamata vero**: le
modifiche ai blocchi che arrivano dal mondo di là, in un client connesso senza
`--play-seconds`. Nei due loop a processo singolo non c'è niente che faccia
rumore nell'altro mondo.

**Nessuna occlusione, nessun riverbero, nessun Doppler.** Un muro non ferma
niente: la distanza è in linea d'aria, e attraverso il portale è la somma di due
linee d'aria.

---

## 31. Il silenzio che il server si è causato da solo

Trovato da **quattordici run di `bad_link` di fila** con il watchdog tolto: uno
rosso, il tredicesimo, e stavolta il gate ha tenuto il run.

### 31.1 Quello che dicono i log

    client:   walked into the opening; asking 127.0.0.1:25613 to take us out of
              its opening 1, waiting up to 10.0 s for an answer (round trip 401 ms)
    client:   the world through the opening did not answer in 10.0 s; staying here
    server B: 'player' joined as observer, protocol 18, handshake 1673 ms
    server B: 'player' came out of this world's opening 1 at 61 20 61
    server B: dropped 'player': nothing heard for 12 s

188349 frame in 40 secondi: il loop non si è mai fermato, quindi non è OPEN-6. Il
mondo è arrivato, quindi non è OPEN-9.

**Il server ha risposto, e poi ha buttato fuori chi stava aspettando quella
risposta.** «came out of this world's opening 1 at 61 20 61» è B che concede la
traversata; il client non l'ha mai ricevuta. La risposta va via affidabile,
quindi ENet la stava ancora ritrasmettendo — e il drop è **quello che ha messo
fine alle ritrasmissioni**. Un server che si arrende su un peer distrugge la
risposta che sta ancora cercando di consegnargli.

### 31.2 Il client taceva apposta

Non è un client bloccato. È una riga scritta di proposito in
[`main.cpp`](../../src/client/main.cpp), con il suo perché già nel commento: il client
smette di mandare pretese di punto di vista a una sessione **nel momento in cui
su quella sessione chiede una traversata** — «not to one that is on its way to
being a body» — perché da lì in poi la pretesa descrive il centro di un portale,
e un server che ha appena concesso la traversata la leggerebbe come una
posizione.

Quindi fra il chiedere e l'essere risposto il client è **muto per progetto**, e
il server contava quel silenzio come assenza.

`join_patience` non ci arriva: copre una sessione che non ha ancora parlato dopo
il welcome. Questa aveva parlato — si era unita, aveva riscattato, aveva chiesto
di passare — quindi ha preso i dodici secondi del giocatore, e la sua stretta di
mano da 1673 ms non le è servita a niente.

### 31.3 Dall'ultimo dei due, non dall'ultima parola

```cpp
double quiet_for(const Session &s, double now)
{
    const double since = s.last_owed_at > s.last_heard_at ? s.last_owed_at : s.last_heard_at;
    return now - since;
}
```

Dieci secondi in cui il server parlava seguiti da due di silenzio vero sono
**due** secondi di silenzio, non dodici. E `silent()` chiede a questa funzione
invece di rifare la sottrazione per conto suo: una regola scritta due volte sono
due regole, ed è il difetto che questa milestone ha trovato più spesso.

### 31.4 Byte, non «ha comandi in uscita»

ENet ha già `enet_peer_has_outgoing_commands`, e sarebbe stata la scelta ovvia.
Conta anche un **ping** in attesa del suo ack, e ENet pinga due volte al secondo:
una regola costruita su quella non avrebbe lasciato andare mai più nessuno.

Quello che si misura sono **byte di dati affidabili**: `reliableDataInTransit`
(mandati e non ancora confermati) più quello che è in coda dietro una finestra
piena. Ogni byte contato una volta sola — un comando esce da una lista ed entra
nell'altra — e un ping non ne porta nessuno.

### 31.5 Il messaggio che valeva un secondo e veniva ritrasmesso per sempre

Il gate nuovo è andato rosso subito, e la colpa era del **codice**, non del gate.

`Time` partiva affidabile, una volta al secondo (`kTellEvery` è 1/1200 di
giornata, e la giornata dura 1200 s), verso ogni sessione. Con la regola nuova
questo lasciava il server perennemente in debito di una risposta con chiunque:
`quiet_for` avrebbe restituito circa **un secondo per sempre** e non sarebbe
stato lasciato andare più nessuno. La regola sarebbe stata verde e morta.

Adesso è inaffidabile, per la stessa ragione del battito: **un messaggio il cui
valore scade prima che la sua ritrasmissione possa arrivare non va mandato
affidabile.** Una ritrasmissione di `Time` arriva annunciando un'ora già passata.
Uno perso costa un secondo di cielo vecchio — 1/1200 di giornata, contro il 1/16
di giornata che vale un livello di luce (§28.7).

E l'armatura del loopback mandava il **battito** affidabile dove il server vero
lo manda inaffidabile: una divergenza fra l'armatura e la cosa armata, che è
esattamente il posto dove un gate smette di misurare quello che crede. Il
controllo che prende tutta la classe è «owes it nothing new through three seconds
of heartbeats».

### 31.5b Il prezzo di renderlo inaffidabile, pagato subito

Reso `Time` inaffidabile, `net_loopback` è andato rosso **una volta su otto**
sotto `ctest -j4`, su un controllo che non c'entra niente con il silenzio:

    FAIL  and it is the hour the server set, to within one code on the wire

Quel controllo ferma la giornata (`--day-length` a un milione di secondi) per
poter confrontare un numero invece di un intervallo. E lì sta il difetto vero:
`kTellEvery` è una **frazione di giornata**, quindi su una giornata da un milione
di secondi il prossimo annuncio è **833 secondi** dopo. Finché il messaggio era
affidabile non importava. Da inaffidabile, un annuncio perso resta perso per
quattordici minuti — e l'ora che un client sente appena entrato è proprio quella
per cui non c'è una seconda occasione.

Non è una soglia da allargare né un gate da rendere più tollerante: è una regola
mancante. Adesso l'attesa è **il minore fra una frazione di giornata e un
secondo**, in un posto solo:

```cpp
double tell_interval(double day_length)
{
    const double by_day = kTellEvery * (day_length > 0.0 ? day_length : 0.0);
    if (by_day <= 0.0) return kTellAtLeastEvery;
    return by_day < kTellAtLeastEvery ? by_day : kTellAtLeastEvery;
}
```

Due proprietà, misurate su ventimila lunghezze di giornata da mezzo secondo a
quasi tre ore invece che su quattro punti: il cielo non è mai stantio più di
`kTellEvery` di giornata, e un annuncio perso non resta perso più di **un
secondo**. E l'accumulatore in `step_time` è tornato a essere **uno**, in
secondi: la frazione di giornata era la stessa regola scritta una seconda volta.

### 31.6 Chi lascia andare il posto

Stare da parte mentre si deve una risposta apre la domanda opposta: e se il peer
è morto e la coda non si svuota mai?

Non serve un secondo limite, perché ce n'è già uno e adesso i due si dividono le
due situazioni **esattamente**:

- niente in sospeso, il peer non dice niente → decide questa regola, perché il
  timeout di ENet guarda solo i comandi affidabili di cui aspetta l'ack, e non
  ce n'è nessuno;
- qualcosa in sospeso, il peer smette di confermare → decide ENet, alla finestra
  più larga che `hold_peer` gli dà.

Nessun buco e nessuna sovrapposizione. È l'unica ragione per cui è sicuro che
questa regola stia da parte mentre il server parla.

### 31.7 Una riga di log che poteva mentire

    dropped 'player': nothing heard for 12 s

Quel `12` era `net::kSessionSilence` stampato alla lettera, e ha smesso di essere
la pazienza di quella sessione il giorno in cui è arrivato `join_patience`. Un
log che riporta una regola che non legge.

Adesso i due numeri escono dalle **stesse due funzioni** che hanno preso la
decisione, quindi non possono più divergere da essa.

### 31.8 Cosa è misurato e cosa no

L'aritmetica è controllata su tempi sintetici in `protocol_test`; l'impianto
sotto — un transport vero che dice cosa deve a un peer vero, e una tavola delle
sessioni avvisata in tempo perché il posto sopravviva — su una connessione vera
in `net_loopback_test`, con mezzo megabyte che ENet deve per forza frammentare.

**La riga di log non ha un gate**: nessuno scenario produce un drop. Non può
andare alla deriva, però, perché entrambi i numeri vengono dalle due funzioni che
la decisione legge.

**E resta OPEN-14**, dallo stesso run: il client dà 10.0 s a una traversata per
essere risposta, mentre su quel link la ritrasmissione della risposta può durare
di più. Due finestre scelte in due posti, nessuna derivata dall'altra — la forma
di OPEN-12 su un'altra coppia.

### 31.9 Tredici sabotaggi, tutti rossi

Nessun verde, per la prima volta in questa milestone — e la ragione non è che i
gate fossero migliori: è che i due difetti veri li aveva già trovati il gate
stesso mentre lo scrivevo (§31.5 e §31.5b), quindi quando è arrivato il
sabotaggio il codice era già stato corretto.

Quelli che dicono di più:

- `quiet_for` che guarda solo l'ultima parola sentita — la regola di prima — fa
  cadere «one the server is still answering is not quiet at all»;
- `quiet_for` che guarda solo quello che si deve fa cadere «an answer finished
  before the client last spoke changes nothing»: è il **massimo** fra i due, non
  il secondo dei due;
- `owed_bytes` senza la coda dietro la finestra piena cade su «owes it those
  bytes before a single one has left» — cioè prima che ENet abbia mandato
  qualcosa; senza `reliableDataInTransit` cade sul controllo successivo;
- `owed_bytes` che conta un ping, `Time` di nuovo affidabile e il battito di
  nuovo affidabile cadono **tutti e tre sullo stesso controllo**, «owes it
  nothing new through three seconds of heartbeats». È il suo mestiere: prende la
  classe, non i tre casi;
- `silent()` che rifà la sottrazione invece di chiedere a `quiet_for` cade su
  «at that same moment the session is kept» — due copie di una regola sono due
  regole anche quando nascono uguali;
- `tell_interval` senza tetto e `tell_interval` che ignora la giornata cadono su
  due controlli **diversi**: il primo sull'ora persa, il secondo sul cielo
  stantio. Le due metà della regola sono separabili, quindi sono due proprietà.

---

## 32. Un mob che si può colpire

### 32.1 Quello che c'era era un corpo che derivava

[`mob.h`](../../src/core/ecs/mob.h) lo diceva di sé: «il meno mob che possa essere —
esiste, ha un corpo, e va alla deriva». Serviva a rendere controllabile una
frase sola, che il percorso di streaming è **un percorso** e non un caso
speciale sulle connessioni: la query è «le entità vicine con un `Body`», e un
mob la soddisfa senza che nessuno abbia cambiato niente per lasciarlo passare.

DESIGN.md 8 chiede «mob base con ECS», e il commento diceva dove finiva: «M7 è
dove stanno comportamento, salute e drop». Questo è quella metà.

### 32.2 Il messaggio dice cosa è stato colpito e nient'altro

`Hit` è **cinque byte**: un tipo e un id. Niente danno, niente arma, niente
punto d'impatto.

Un client che potesse dire quanto vale un colpo potrebbe dire venti, e il
controllo dall'altra parte sarebbe una seconda copia della regola che lo decide.
Il punto d'impatto servirebbe solo a disegnare, e un client che stava già mirando
sa dove ha colpito. E non dice nemmeno **dove stava** chi ha colpito, per la
stessa ragione per cui `Edit` non lo dice: il server ha una posizione che ha
accettato, e una seconda nel messaggio è una che può non essere d'accordo con la
prima.

### 32.3 Il raffreddamento sta sul server

Senza, un client che manda un colpo per frame uccide qualunque cosa dentro un
tick e il server crede a una cadenza che non ha modo di verificare.

Misurato: **venti colpi nello stesso istante tolgono quanto uno**, e uno dopo
`kHitCooldown` toglie di nuovo — così la regola è una *cadenza* e non un mute.

### 32.4 La portata è il braccio che posa un blocco, non un secondo numero

Misurata dall'occhio della posizione che il **server** ha accettato al mezzo del
corpo — non ai piedi, o un giocatore in piedi sopra un mob sarebbe più lontano di
uno che gli sta accanto.

**E il gate ha dovuto cambiare forma.** La prima versione faceva camminare il
giocatore quaranta nodi e poi colpire: rosso, e per la ragione giusta — quella
camminata è una che l'autorità di movimento **rifiuta**, quindi il giocatore era
ancora lì e il colpo atterrava. Un controllo del genere sarebbe potuto passare
per non aver misurato niente. Adesso il bersaglio lontano è un **secondo mob**, e
il giocatore non si muove.

### 32.5 Morire è una regola sola

`reap_dead` guarda solo le cose con `Mob`. L'avatar di un giocatore ha `Health`
anche lui, e sopra ci sta una sessione: distruggerlo lì toglierebbe il corpo da
sotto una connessione viva — un giocatore morto lo rimette in piedi `kill_body`,
che è la regola che esiste già.

E i drop nascono **fuori** dalla camminata: creare un'entità dentro una `each()`
sposta la tabella sotto l'iteratore. Le entità morte si raccolgono prima e si
distruggono dopo.

Un mob che non porta niente lascia **niente** — non uno stack di niente, che è
una cosa che un client dovrebbe disegnare.

### 32.6 Il filo doveva dire cosa è ogni corpo

Prima si leggeva da `item`: uno stack per terra era «quello che porta qualcosa»,
e un mob era «un giocatore che per caso è più piccolo» — che il client non poteva
vedere **affatto**. Quindi disegnava i mob come persone, e un giocatore che mena
un mob mena per sbaglio un altro giocatore.

`EntityKind` è un campo: `Player`, `Mob`, `Drop`. Un byte per corpo. E i due
campi devono essere d'accordo — `kind` dice **cosa** è, `item` dice **quale**
stack — perché un decoder che di fronte a un disaccordo sceglie il suo preferito
sceglie diverso su client diversi. Un `Drop` senza stack è rifiutato; una persona
con uno stack pure; e un `kind` che questa build non conosce è rifiutato invece
che disegnato come una persona.

### 32.7 Il primo numero era sbagliato, e il gate l'ha detto subito

`kMobHalfWidth` era `0.35`, cioè **più largo** di un giocatore (`0.3`). Scritto
"un mob è più piccolo di una persona" e messo il controllo, la prima esecuzione
ha detto rosso. Adesso è `0.25`.

Vale la pena notare che nessuno dei controlli su *cosa fa* la regola l'avrebbe
preso: mirare, colpire e morire funzionavano tutti benissimo con un mob largo.
Quello che l'ha preso è il controllo sulla **frase** — la ragione per cui il byte
`kind` esiste.

Anche `ray_hits_box` ha i suoi due: un raggio che si ferma prima del bersaglio
non lo colpisce (la portata è parte della domanda), e un raggio senza componente
lungo un asse si chiede a mano invece di dividere per zero — che darebbe un
infinito che funziona e un NaN quando l'origine sta esattamente su un piano.

### 32.8 Quello che non c'è, dichiarato

- **Nessuna arma.** Ogni mano toglie `kPunch`, che è 1. Non c'è una proprietà
  dell'oggetto che possa dire diversamente, e un campo che solo un oggetto
  imposta è un campo sbagliato di default per tutti gli altri.
- **Nessun comportamento.** Il mob gira sul suo cerchio e non si accorge di
  essere colpito: non scappa, non insegue, non fa male a nessuno. «Base» è la
  parola di DESIGN.md e questa è quella.
- **Nessuna barra per i mob.** Si vede che muore, non quanto gli manca.
- **Il bersaglio lo sceglie il client.** Il server controlla una *distanza*, non
  un raggio: un client modificato può colpire qualcosa dietro un muro, entro la
  portata. `ray_hits_box` sta in `core/physics` proprio perché prima o poi la
  domanda deve essere la stessa dalle due parti; oggi non lo è, ed è scritto in
  `ServerSessions::strike` invece che nascosto.
- **La mira del client non ha gate.** `aimed_body` non è misurata da niente:
  nessun test headless muove un mouse. È colla non misurata come §22.5, §23.6,
  §24.6, §26.8, §28.9 e §30.

### 32.9 Diciassette sabotaggi, tre verdi, e una riga in meno

Quattordici rossi alla prima prova. I tre verdi sono la parte che vale.

**Un raggio senza componente lungo un asse.** C'era un ramo esplicito che
chiedeva «questo raggio è parallelo a questo piano?» invece di dividere per zero,
con tanto di commento su infiniti e NaN. Sabotandolo — togliendo il ramo e
lasciando dividere — **nessun controllo diventa rosso**, e non perché il gate sia
debole: l'aritmetica generale dà già la risposta giusta in tutti e tre i casi.
Fuori dalla fetta i due attraversamenti escono con lo stesso segno e
`enter > leave` rifiuta; dentro escono `-inf` e `+inf` e l'asse non dice niente;
esattamente sul piano uno dei due è `0/0`, e ogni confronto con un NaN è falso,
il che lascia di nuovo l'asse muto — che è quello che un raggio disteso su una
faccia deve dire.

Quindi il ramo è **uscito**. Tre righe che ripetevano quello che i numeri
dicevano già. È la quarta volta in questa milestone che un sabotaggio verde fa
cancellare del **codice** invece di aggiungere un controllo (§20, §30, §31.5).

**«Non ha tolto niente» non è «l'ha rifiutato questa regola».** Gli altri due
verdi sono lo stesso errore di gate, ed è un errore che vale la pena guardare in
faccia: a rifiutare un colpo ci sono **sette** regole in fila, e un controllo che
chiede solo *se* è stato rifiutato passa qualunque sia quella che ha risposto.

- Una finestra che colpisce: tolto il controllo sullo scope, l'osservatore
  veniva rifiutato lo stesso — da `!s.authority().valid`, perché il `Move` di un
  osservatore imposta `has_moved` e **non** tocca mai l'autorità (session.cpp:332,
  con il suo perché scritto lì). Il gate restava verde su una regola cancellata.
- Uno stack per terra colpito: tolto il rifiuto per nome, il drop veniva
  rifiutato da «quello non si può ferire», perché un drop non ha `Health`. Verde
  anche quello.

Adesso i due controlli chiedono **quale** regola ha risposto, leggendo la riga di
log che il server ha scritto: `watching, not playing` e `picked up, not struck`.
Il che rende quelle due frasi parte del contratto invece che decorazione — ed è
il motivo per cui la seconda esiste, dato che «quello non si può ferire» è la
cosa sbagliata da dire di una cosa che si raccoglie.

Il log tiene la prima e poi una su cento, quindi il colpo allo stack lo tira il
**secondo** giocatore, per cui è il primo rifiuto.

---

## 33. Trenta minuti, e un giocatore che non si ferma

DESIGN.md 8 chiude M7 con una frase: «una partita di 30 minuti dall'inizio,
senza crash e senza toccare la console».

### 33.1 Trenta minuti di un client fermo non sono una partita

La cosa ovvia è `--play-seconds 1800` e aspettare. Quello che si misura così è
**un socket e una perdita di memoria**: il client entra, fa la sequenza scriptata
di rottura e ricostruzione nei primi secondi, e poi sta fermo per ventinove
minuti e mezzo. Nessun chunk nuovo, nessun remesh, nessuna creatura, nessun
drop, nessuna borsa che cambia.

Quindi `--roam`: il giocatore cammina in cerchio, rompe un blocco e ne
ricostruisce uno mentre va, e va incontro a quello che vede per menarlo —
attraverso **gli stessi messaggi che manda un click**, perché quello sotto esame
è il ciclo che continua a chiudersi e non un secondo percorso che usa solo la run
lunga.

È un flag suo e non «una `--play-seconds` lunga vuol dire questo»: ogni altro
gate qui gioca per qualche secondo e si aspetta che il giocatore resti dove lo
scenario l'ha messo.

### 33.2 Due difetti trovati scrivendolo, tutti e due misurati

**Un raggio a livello passa sopra la testa di una creatura.** Il roam camminava
dritto addosso alle otto creature del mondo e non ne colpiva nessuna: **150
secondi, zero colpi**. Una creatura è alta `kMobHeight` = 1.4 e un occhio sta a
`kPlayerEyeHeight` = 1.62, quindi un raggio orizzontale da un giocatore in piedi
sullo stesso terreno passa **venti centimetri sopra**. Adesso il roam guarda giù
verso la metà del corpo, e la stessa scena fa 136 colpi e otto creature morte.

Vale la pena notare che nessun gate lo prendeva, e correttamente: il gate del
loopback colpisce nominando un id, che è quello che il messaggio porta. Puntare
è roba del client, e §32.8 lo diceva già — «la mira del client non ha gate».
Questo è quel buco che si fa vedere.

**Un controllo che leggeva lo stato sbagliato.** «E la borsa ha pagato: non è
rimasto niente» leggeva la borsa **alla fine della run**. Giusto finché la run
finiva subito dopo la sequenza; con il roam il giocatore continua a rompere
blocchi per un'altra mezz'ora, quindi il controllo leggeva una borsa che un
**altro** scenario aveva riempito. Rosso, e aveva ragione sulla sua stessa
affermazione e torto su dove guardare: adesso campiona la borsa **mentre** la
sequenza gira, quindi l'ultimo valore scritto è quello con cui la sequenza ha
finito.

E il roam non parte finché quella sequenza non ha finito. Due scenari in una run
sono uno scenario che non ha scritto nessuno — questo file lo dice già
dell'attraversamento, ed è la stessa cosa.

### 33.3 Cosa dice il gate, oltre a «non è morto»

Arrivare in fondo senza crash è la maggior parte e non è tutto: anche un client
che smette di fare qualsiasi cosa dopo cinque minuti arriva in fondo. Quindi:

- il **ciclo ha continuato a chiudersi** — blocchi fuori dal mondo e dentro di
  nuovo, contati;
- la **memoria non è cresciuta senza limite** — stessa regola e stesso numero
  del soak di M1 (64 MB), non un secondo numero scelto accanto;
- il server ha visto il join e **non ha mai mollato la sessione** (una run in cui
  il server si arrende a metà finirebbe con lo stesso codice di uscita);
- e se sono partiti colpi, **qualcosa è morto**: il client conta quello che
  manda, e solo il server può dire se è servito a qualcosa.

Il watchdog è armato a 30 s invece dei 15 dei gate corti — la run è lunga
abbastanza perché la macchina faccia altro per un attimo — e se scatta la run è
rossa con lo **stack** attaccato, che è tutto il motivo per cui sta lì invece di
un timeout di ctest.

### 33.3b La prima run da trenta minuti è stata rossa, e aveva ragione

Vale la pena raccontarla per intero, perché è il motivo per cui una run lunga
non è una run corta più lunga.

**Il giocatore è uscito dal mondo e ci è rimasto ventidue minuti.** Le righe al
minuto dicono tutto: da 1080 s a 1800 s i contatori sono **congelati** — 22
rotti, 21 costruiti, 136 colpi, 40 svolte — mentre i frame continuavano a 6000
al secondo e le correzioni salivano di nove al minuto. Alla fine:

    corrected to 15.14 -130580.30 110.86: moved 8.68 blocks vertically in 0.05s
    stood at -3.06 -130822.35 97.95, block below is 'core:air' (NOT SOLID)

Il cerchio del roam ha un raggio di una cinquantina di nodi e il mondo ne è largo
128, quindi il giocatore ne è **uscito dal bordo** — e sotto il bordo non c'è
niente.

**E lì sotto non è morto.** La regola c'è, ed è giusta:

```cpp
if (v.y < kBelowWorld) kill_body(s, now);   // -16
```

Ma `kill_body` guarisce il corpo e poi:

```cpp
if (!m_spawn.known) return;   // nowhere to come back to: the health is all of it
```

E lo spawn del server era dentro `if (portals)`. Un mondo avviato **senza**
aperture non ne aveva **nessuno**, quindi un corpo che cade fuori dal mondo viene
ucciso e guarito a ogni pretesa, non viene mai spostato, e cade per sempre.

La cosa da guardare in faccia: **quella regola è gated**, in `net_loopback` —
«falling out of the bottom of the world puts the body back». E quel gate chiama
`set_spawn` da solo, quindi non ha mai chiesto una volta cosa fa un server che
non ne ha uno. La regola era giusta e irraggiungibile.

Adesso lo spawn è fuori dall'`if`: davanti alla prima apertura quando ce n'è una,
in mezzo alla piazza quando non c'è. Una funzione sola in mezzo alle due
(`free_beside`), perché due copie di una ricerca sono due ricerche. E il gate sta
dove sta il difetto — nel **binario**: `long_game.sh` e `net_client.sh` avviano
un server senza aperture e insistono che dica dove tornano i morti.

### 33.3c Tre controlli che leggevano lo stato del secondo scenario

Il resto della prima run rossa è **una causa sola detta tre volte**: una run
lunga è un **secondo scenario**, e ogni controllo che leggeva lo stato *alla fine
della run* stava applicando la sua affermazione a uno stato che apparteneva a
qualcun altro.

- «e la borsa ha pagato: non è rimasto niente» — il roam ci aveva messo dentro
  altri blocchi per mezz'ora;
- «il blocco sotto il giocatore viene dal server ed è solido» — il giocatore
  stava sul bordo di una buca che aveva scavato lui un secondo prima, quindi il
  controllo andava rosso su un blocco che aveva rotto lui;
- «stare fermi non è mai stato corretto» — il giocatore non stava fermo.

Tutti e tre adesso campionano **mentre la sequenza scriptata gira** — che è lo
stato di cui parlano — e la sequenza sta ferma. Il roam non parte finché quella
non ha finito, per la stessa ragione per cui l'attraversamento aspetta le mani:
due scenari in una run sono uno scenario che non ha scritto nessuno.

### 33.3d E poi è passata

Trenta minuti, dall'inizio, senza crash e senza toccare la console:

    left after 1800.0s, 7866142 frames, 32 chunks, 0 corrections applied,
                        0.85 MB in / 2.32 MB out
    edits: 452 sent, 436 cell(s) changed, 12 refused
    roam: 224 broke, 224 built, 136 swung, 294 turned away from something
    RSS from the first step 122.6 -> 123.8 MB (+1.1), worst seen 123.8 MB
    136 swing(s) killed 8 of them, and each left what it carried

I numeri che dicono qualcosa, uno per volta:

- **Zero correzioni in mezz'ora.** Il client predice, il server valida, e per
  1800 secondi non si sono mai contraddetti. La prima run, quella con il
  giocatore fuori dal mondo, ne aveva 246.
- **+1.1 MB di RSS**, e piatta: 123.7 al minuto uno e 123.8 al minuto trenta. La
  crescita è tutta nel primo minuto, che è il mondo che arriva e si mesha.
- **4370 frame al secondo**, stabili dall'inizio alla fine — 7.9 milioni in
  totale. Il socket nel loop non lo rallenta e non si degrada.
- **224 rotti e 224 ricostruiti.** Il ciclo si è chiuso 448 volte di fila senza
  che la borsa si sfasasse: 452 edit mandati, 436 celle cambiate, 12 rifiutati
  (che sono i rifiuti che un giocatore incontra davvero — mirare a niente,
  costruire dove c'è già qualcosa).
- **Un giorno e mezzo di gioco.** L'orologio nelle righe al minuto fa 13:11 →
  00:00 → 00:00: una giornata dura 1200 s, quindi trenta minuti sono una
  giornata e mezza, e il cielo ha attraversato due mezzanotti.

### 33.3e Quello che questa run **non** misura

- **Le creature finiscono.** Otto sono morte nel primo minuto e mezzo e non
  torna nessuno: `136 swung` è congelato dal minuto due in poi. Non c'è nessuno
  spawn di mob in M7 — DESIGN.md 8 dice «mob base» e questo è quello — quindi
  ventotto minuti su trenta sono una passeggiata con le mani.
- **Nessun attraversamento.** Questo mondo non ha aperture, di proposito: la
  sequenza delle mani e l'attraversamento sono due scenari, e `bad_link` è
  quello che fa il secondo.
- **Nessuna persona.** È uno script che cammina in cerchio, non un giocatore.
  «Senza toccare la console» è vero alla lettera e non nello spirito: quello che
  è misurato è che il motore regge mezz'ora di gioco continuo, non che sia
  divertente.

### 33.4 Sessanta run, e OPEN-6 non si è più visto

Lo scenario che dava **2 blocchi su 5** — un client attraverso `cnp_netshape` a
60 ms, 20 ms di jitter, 2% di perdita — rifatto due volte:

- **40 run con il watchdog nuovo armato**: tutte finite, 8-9 s l'una;
- **20 run senza nessun watchdog**, che è esattamente la configurazione che si
  bloccava, con un `timeout 60` intorno al client perché un blocco si vedesse:
  tutte finite.

Al vecchio tasso il solo controllo avrebbe dovuto bloccarsi **otto volte**. È la
prima prova che separa «lo strumento lo nascondeva» da «non sta succedendo»: il
watchdog non spezza più una `poll`, e la run senza watchdog è d'accordo con
quella con il watchdog.

Quello che **non** si afferma: che qualcosa qui l'abbia risolto. Niente è stato
cambiato pensando a questo, e i commit in mezzo hanno toccato le regole di
liveness del server, il watchdog stesso e l'affidabilità di due messaggi.
Ognuno di quelli può averlo spostato, e dire quale sarebbe una storia.

### 33.5 Quattro sabotaggi, e un log che raccontava un calcolo

Rossi: lo spawn della piazza spostato fuori dal mondo, `free_beside` che si tiene
il punto occupato invece di scansarsi, e la borsa riletta a fine run — che è il
difetto di §33.3c rimesso dentro apposta.

Il quarto è partito **verde**, ed è di nuovo §31.7.

Il sabotaggio era «rimetti `set_spawn` dentro `if (portals)`», cioè esattamente
il difetto che questa sezione ha trovato. Il gate — «il server dice dove tornano
i morti» — è rimasto verde, perché la riga di log stampava `sp`, la variabile
appena **calcolata**, e non quello che la tavola delle sessioni aveva davvero.
Un server senza spawn continuava ad annunciare uno spawn.

Stessa forma della riga sui drop: **un log che riporta una regola che non
legge**. Adesso c'è `ServerSessions::spawn()` e la riga la legge da lì, quindi il
silenzio è il rapporto onesto di un mondo in cui non si può tornare — e il gate
diventa rosso.
